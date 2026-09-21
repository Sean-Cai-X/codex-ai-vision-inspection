#include "TorchRuntimeBridge.h"
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

namespace {

#ifdef _WIN32
std::string FormatWin32Error(DWORD code)
{
    if (code == 0) {
        return {};
    }

    LPSTR buffer = nullptr;
    const DWORD size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPSTR>(&buffer),
        0,
        nullptr);

    std::string message;
    if (size > 0 && buffer) {
        message.assign(buffer, size);
        while (!message.empty() && (message.back() == '\r' || message.back() == '\n' || message.back() == ' ')) {
            message.pop_back();
        }
    }
    if (buffer) {
        LocalFree(buffer);
    }
    return message;
}

std::string GetEnvironmentValue(const char* name)
{
    const DWORD required = GetEnvironmentVariableA(name, nullptr, 0);
    if (required == 0) {
        return {};
    }
    std::string value(required, '\0');
    const DWORD written = GetEnvironmentVariableA(name, value.data(), required);
    if (written == 0) {
        return {};
    }
    value.resize(written);
    return value;
}

void AddExistingDirectory(std::vector<std::string>& dirs, const std::filesystem::path& dir)
{
    std::error_code ec;
    if (!dir.empty() && std::filesystem::exists(dir, ec) && std::filesystem::is_directory(dir, ec)) {
        dirs.push_back(dir.string());
    }
}

void AddRuntimeDllDirectories(const std::vector<std::string>& dirs)
{
    using SetDefaultDllDirectoriesFn = BOOL (WINAPI *)(DWORD);
    using AddDllDirectoryFn = DLL_DIRECTORY_COOKIE (WINAPI *)(PCWSTR);

    HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
    if (!kernel32) {
        return;
    }

    auto set_default_dirs = reinterpret_cast<SetDefaultDllDirectoriesFn>(
        GetProcAddress(kernel32, "SetDefaultDllDirectories"));
    auto add_dll_directory = reinterpret_cast<AddDllDirectoryFn>(
        GetProcAddress(kernel32, "AddDllDirectory"));

    if (!set_default_dirs || !add_dll_directory) {
        return;
    }

    set_default_dirs(LOAD_LIBRARY_SEARCH_SYSTEM32 | LOAD_LIBRARY_SEARCH_USER_DIRS);

    for (const std::string& dir : dirs) {
        if (dir.empty()) {
            continue;
        }
        const int required = MultiByteToWideChar(CP_UTF8, 0, dir.c_str(), -1, nullptr, 0);
        if (required <= 0) {
            continue;
        }
        std::wstring wide(static_cast<size_t>(required), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, dir.c_str(), -1, wide.data(), required);
        if (!wide.empty() && wide.back() == L'\0') {
            wide.pop_back();
        }
        add_dll_directory(wide.c_str());
    }
}

void PrependRuntimeDllSearchPath(const std::string& dll_path)
{
    std::vector<std::string> dirs;

    const std::string libtorch_root = GetEnvironmentValue("LIBTORCH_ROOT");
    const std::string cxvision_libtorch_bin = GetEnvironmentValue("CXVISION_LIBTORCH_BIN");
    const std::string cxvision_opencv_bin = GetEnvironmentValue("CXVISION_OPENCV_BIN");
    const std::string cuda_path = GetEnvironmentValue("CUDA_PATH");

    AddExistingDirectory(dirs, std::filesystem::path(dll_path).parent_path());
    AddExistingDirectory(dirs, cxvision_libtorch_bin);
    if (!libtorch_root.empty()) {
        AddExistingDirectory(dirs, std::filesystem::path(libtorch_root) / "lib");
        AddExistingDirectory(dirs, std::filesystem::path(libtorch_root) / "bin");
    }

    AddExistingDirectory(dirs, "D:/libtorch/lib");
    AddExistingDirectory(dirs, "D:/libtorch/bin");

    AddExistingDirectory(dirs, cxvision_opencv_bin);
    AddExistingDirectory(dirs, "D:/opencv4.9/opencv/build/x64/vc16/bin");
    AddExistingDirectory(dirs, "D:/opencv/build/x64/vc16/bin");

    if (!cuda_path.empty()) {
        AddExistingDirectory(dirs, std::filesystem::path(cuda_path) / "bin");
    }

    if (dirs.empty()) {
        return;
    }

    AddRuntimeDllDirectories(dirs);

    std::string old_path = GetEnvironmentValue("PATH");
    std::ostringstream next_path;
    bool first = true;
    for (const std::string& dir : dirs) {
        if (!first) {
            next_path << ';';
        }
        next_path << dir;
        first = false;
    }
    if (!old_path.empty()) {
        next_path << ';' << old_path;
    }
    SetEnvironmentVariableA("PATH", next_path.str().c_str());
}
#endif

} // namespace

bool TorchRuntimeBridge::Load(const std::string& dll_path)
{
    Unload();
    last_error_message_.clear();

#ifdef _WIN32
    PrependRuntimeDllSearchPath(dll_path);

    SetLastError(0);
    dll_ = LoadLibraryExA(
        dll_path.c_str(),
        nullptr,
        LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!dll_) {
        const DWORD error_code = GetLastError();
        std::ostringstream oss;
        oss << "LoadLibraryExA failed for '" << dll_path << "'";
        if (error_code != 0) {
            oss << " error=" << error_code;
            const std::string formatted = FormatWin32Error(error_code);
            if (!formatted.empty()) {
                oss << " (" << formatted << ")";
            }
        }
        last_error_message_ = oss.str();
        return false;
    }

    create_ = reinterpret_cast<CreateFn>(GetProcAddress(dll_, "torch_runtime_create"));
    destroy_ = reinterpret_cast<DestroyFn>(GetProcAddress(dll_, "torch_runtime_destroy"));
    run_task_ = reinterpret_cast<RunTaskFn>(GetProcAddress(dll_, "torch_runtime_run_task"));
    free_result_ = reinterpret_cast<FreeResultFn>(GetProcAddress(dll_, "torch_runtime_free_result"));
    version_ = reinterpret_cast<VersionFn>(GetProcAddress(dll_, "torch_runtime_version"));

    if (!create_ || !destroy_ || !run_task_ || !free_result_ || !version_) {
        std::ostringstream oss;
        oss << "torch runtime exports missing:";
        if (!create_) oss << " torch_runtime_create";
        if (!destroy_) oss << " torch_runtime_destroy";
        if (!run_task_) oss << " torch_runtime_run_task";
        if (!free_result_) oss << " torch_runtime_free_result";
        if (!version_) oss << " torch_runtime_version";
        last_error_message_ = oss.str();
        Unload();
        return false;
    }
#else
    dll_ = dlopen(dll_path.c_str(), RTLD_LAZY);
    if (!dll_) {
        const char* message = dlerror();
        last_error_message_ = message ? message : "dlopen failed";
        return false;
    }

    create_ = reinterpret_cast<CreateFn>(dlsym(dll_, "torch_runtime_create"));
    destroy_ = reinterpret_cast<DestroyFn>(dlsym(dll_, "torch_runtime_destroy"));
    run_task_ = reinterpret_cast<RunTaskFn>(dlsym(dll_, "torch_runtime_run_task"));
    free_result_ = reinterpret_cast<FreeResultFn>(dlsym(dll_, "torch_runtime_free_result"));
    version_ = reinterpret_cast<VersionFn>(dlsym(dll_, "torch_runtime_version"));

    if (!create_ || !destroy_ || !run_task_ || !free_result_ || !version_) {
        last_error_message_ = "torch runtime exports missing";
        Unload();
        return false;
    }
#endif

    return true;
}

void TorchRuntimeBridge::Unload()
{
    if (handle_) {
        if (destroy_) {
            destroy_(handle_);
        }
        handle_ = nullptr;
    }

    create_ = nullptr;
    destroy_ = nullptr;
    run_task_ = nullptr;
    free_result_ = nullptr;
    version_ = nullptr;

#ifdef _WIN32
    if (dll_) {
        // libtorch keeps process-wide runtime/autograd state behind the runtime
        // DLL.  Training lifecycle smoke can finish successfully and still
        // fault during an eager FreeLibrary() on Windows while those static
        // resources are unwinding.  Keep the module loaded until process exit;
        // still destroy the runtime handle above and clear function pointers so
        // callers cannot use stale state.  This preserves the single Parser
        // owner model and avoids a detached worker/timeout workaround.
        dll_ = nullptr;
    }
#else
    if (dll_) {
        dlclose(dll_);
        dll_ = nullptr;
    }
#endif
}

bool TorchRuntimeBridge::Create(const TorchRuntimeGuiConfig& config)
{
    if (!IsLoaded()) {
        return false;
    }

    if (handle_) {
        destroy_(handle_);
        handle_ = nullptr;
    }

    TorchRuntimeConfig c_api_config{};
    if (!config.model_root.empty()) c_api_config.model_root = config.model_root.c_str();
    if (!config.output_root.empty()) c_api_config.output_root = config.output_root.c_str();
    if (!config.device.empty()) c_api_config.device = config.device.c_str();
    if (!config.log_level.empty()) c_api_config.log_level = config.log_level.c_str();

    int ret = create_(&c_api_config, &handle_);
    return (ret == 0 && handle_ != nullptr);
}

TorchRuntimeGuiResult TorchRuntimeBridge::RunTask(const TorchRuntimeGuiRequest& request)
{
    TorchRuntimeGuiResult result;

    if (!IsLoaded() || !handle_) {
        result.ok = false;
        result.error_message = "Torch runtime not initialized";
        return result;
    }

    TorchTaskRequest c_api_request{};
    if (!request.task.empty()) c_api_request.task = request.task.c_str();
    if (!request.device.empty()) c_api_request.device = request.device.c_str();
    if (!request.input_image.empty()) c_api_request.input_image = request.input_image.c_str();
    if (!request.dataset_root.empty()) c_api_request.dataset_root = request.dataset_root.c_str();
    if (!request.manifest_path.empty()) c_api_request.manifest_path = request.manifest_path.c_str();
    if (!request.case_name.empty()) c_api_request.case_name = request.case_name.c_str();
    if (!request.extra_json.empty()) c_api_request.extra_json = request.extra_json.c_str();
    if (!request.output_dir.empty()) c_api_request.output_dir = request.output_dir.c_str();

    TorchTaskResult c_api_result{};

    struct ResultGuard
    {
        FreeResultFn free_result = nullptr;
        TorchTaskResult* result = nullptr;

        ~ResultGuard()
        {
            if (free_result != nullptr && result != nullptr) {
                free_result(result);
            }
        }
    };

    ResultGuard guard{free_result_, &c_api_result};

    int ret = run_task_(handle_, &c_api_request, &c_api_result);
    if (ret != 0) {
        result.ok = false;
        result.error_message = "torch_runtime_run_task failed";
        return result;
    }

    result.ok = (c_api_result.ok != 0);
    result.error_code = c_api_result.error_code;
    result.train_runtime_ms = c_api_result.train_runtime_ms;
    result.infer_runtime_ms = c_api_result.infer_runtime_ms;
    result.algorithm_runtime_ms = c_api_result.algorithm_runtime_ms;
    result.placeholder_runtime_ms = c_api_result.placeholder_runtime_ms;

    if (c_api_result.status) result.status = c_api_result.status;
    if (c_api_result.error_message) result.error_message = c_api_result.error_message;
    if (c_api_result.requested_device) result.requested_device = c_api_result.requested_device;
    if (c_api_result.actual_device) result.actual_device = c_api_result.actual_device;
    if (c_api_result.result_json) result.result_json = c_api_result.result_json;
    if (c_api_result.evidence_ref) result.evidence_ref = c_api_result.evidence_ref;
    if (c_api_result.result_ref) result.result_ref = c_api_result.result_ref;
    if (c_api_result.input_image_ref) result.input_image_ref = c_api_result.input_image_ref;
    if (c_api_result.primary_visual_ref) result.primary_visual_ref = c_api_result.primary_visual_ref;
    if (c_api_result.visualization_refs) result.visualization_refs = c_api_result.visualization_refs;
    if (c_api_result.bbox_candidate_list_ref) result.bbox_candidate_list_ref = c_api_result.bbox_candidate_list_ref;
    if (c_api_result.roi_crop_packet_ref) result.roi_crop_packet_ref = c_api_result.roi_crop_packet_ref;
    if (c_api_result.attach_back_ref) result.attach_back_ref = c_api_result.attach_back_ref;
    if (c_api_result.template_alignment_ref) result.template_alignment_ref = c_api_result.template_alignment_ref;
    if (c_api_result.roi_diff_candidate_ref) result.roi_diff_candidate_ref = c_api_result.roi_diff_candidate_ref;
    if (c_api_result.trainer_lifecycle_summary) result.trainer_lifecycle_summary = c_api_result.trainer_lifecycle_summary;
    if (c_api_result.unified_mainline_summary) result.unified_mainline_summary = c_api_result.unified_mainline_summary;

    return result;
}

void TorchRuntimeBridge::Destroy()
{
    if (handle_ && destroy_) {
        destroy_(handle_);
        handle_ = nullptr;
    }
}

bool TorchRuntimeBridge::IsLoaded() const
{
    return dll_ != nullptr && create_ != nullptr && destroy_ != nullptr && run_task_ != nullptr && free_result_ != nullptr && version_ != nullptr;
}

std::string TorchRuntimeBridge::RuntimeVersion() const
{
    if (version_ == nullptr) {
        return {};
    }

    const char* version = version_();
    return version != nullptr ? std::string(version) : std::string();
}

const std::string& TorchRuntimeBridge::LastErrorMessage() const
{
    return last_error_message_;
}
