#include <iostream>
#include <string>
#include <cstdlib>

#include "torch_runtime_core.h"
#include "torch_test_host.h"

namespace {

void apply_requested_device_env(const std::string& requested_device) {
#ifdef _WIN32
    if (requested_device == "cpu") {
        _putenv_s("LIBTORCH_MODULE_USE_CUDA", "0");
        return;
    }
    if (requested_device == "gpu") {
        _putenv_s("LIBTORCH_MODULE_USE_CUDA", "1");
        return;
    }
    _putenv_s("LIBTORCH_MODULE_USE_CUDA", "");
#else
    if (requested_device == "cpu") {
        setenv("LIBTORCH_MODULE_USE_CUDA", "0", 1);
        return;
    }
    if (requested_device == "gpu") {
        setenv("LIBTORCH_MODULE_USE_CUDA", "1", 1);
        return;
    }
    unsetenv("LIBTORCH_MODULE_USE_CUDA");
#endif
}

TorchTestProfile parse_profile(const std::string& value) {
    TorchTestProfile profile = TorchTestProfile::FullAll;
    if (TorchTestHost::try_parse_profile_name(value, profile)) return profile;
    TORCH_CHECK(false, "Unknown demo profile: ", value);
}

void print_usage() {
    std::cout << "libtorch_module demo\n";
    std::cout << "Usage:\n";
    std::cout << "  libtorch_module_demo [--profile <name>] [--task <task_id>] [--device <cpu|gpu|auto>] [--steps] [--tasks]\n";
    std::cout << "Profiles:\n";
    std::cout << "  preprocess-contract\n";
    std::cout << "  postprocess-contract\n";
    std::cout << "  full-dataset\n";
    std::cout << "  full-image\n";
    std::cout << "  full-train\n";
    std::cout << "  full-all\n";
    std::cout << "Task binding:\n";
    std::cout << "  --task torch.feature.mobilevit.session\n";
    std::cout << "  --task torch.feature.deeplab.contract\n";
    std::cout << "  --task torch.feature.yolo.eval\n";
    std::cout << "Device mode:\n";
    std::cout << "  --device cpu\n";
    std::cout << "  --device gpu\n";
    std::cout << "  --device auto\n";
}

void print_steps(TorchTestProfile profile) {
    std::cout << "Demo steps for profile=" << TorchTestHost::profile_name(profile) << "\n";
    switch (profile) {
    case TorchTestProfile::PreprocessContract:
        std::cout << "1. Validate image preprocessing and tensor conversion.\n";
        std::cout << "2. Validate annotation synchronization and dataset contract.\n";
        break;
    case TorchTestProfile::PostprocessContract:
        std::cout << "1. Validate YOLO loss/build configuration contract.\n";
        std::cout << "2. Confirm postprocess-side checks can be consumed by the host.\n";
        break;
    case TorchTestProfile::FullDataset:
        std::cout << "1. Run YOLO image/annotation/dataset validation.\n";
        std::cout << "2. Run MobileViT dataset smoke.\n";
        std::cout << "3. Review stage report and check lines.\n";
        break;
    case TorchTestProfile::FullImage:
        std::cout << "1. Run full-dataset path.\n";
        std::cout << "2. Run two-stage inference smoke.\n";
        std::cout << "3. Review inference-side stage report.\n";
        break;
    case TorchTestProfile::FullTrain:
        std::cout << "1. Run full-image path.\n";
        std::cout << "2. Run YOLO smoke train, GPU smoke, trainer session, and unified mainline bundle.\n";
        std::cout << "3. Run MobileViT train smoke.\n";
        std::cout << "4. Review lifecycle, unified bundle, and stage report.\n";
        break;
    case TorchTestProfile::FullAll:
    default:
        std::cout << "1. Run current full host profile.\n";
        std::cout << "2. Review aggregated stage report and checks.\n";
        break;
    }
}

}

int main(int argc, char** argv) {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    TorchTestProfile profile = TorchTestHost::current_profile();
    bool show_steps = false;
    bool show_tasks = false;
    std::string selected_task;
    std::string requested_device = "auto";

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_usage();
            return 0;
        }
        if (arg == "--steps") {
            show_steps = true;
            continue;
        }
        if (arg == "--tasks") {
            show_tasks = true;
            continue;
        }
        if (arg == "--profile") {
            TORCH_CHECK(i + 1 < argc, "--profile requires a value");
            profile = parse_profile(argv[++i]);
            continue;
        }
        if (arg == "--task") {
            TORCH_CHECK(i + 1 < argc, "--task requires a value");
            selected_task = argv[++i];
            continue;
        }
        if (arg == "--device") {
            TORCH_CHECK(i + 1 < argc, "--device requires a value");
            const std::string raw_device = argv[++i];
            std::string normalized_device;
            TORCH_CHECK(TorchTestHost::try_normalize_device_mode(raw_device, normalized_device),
                        "Unknown device mode: ", raw_device);
            requested_device = normalized_device;
            continue;
        }
        TORCH_CHECK(false, "Unknown argument: ", arg);
    }

    const TorchTaskSpec* selected_spec = nullptr;
    if (!selected_task.empty()) {
        selected_spec = TorchTestHost::find_task_spec(selected_task);
        TORCH_CHECK(selected_spec != nullptr, "Unknown demo task: ", selected_task);
        profile = selected_spec->profile;
    }

    apply_requested_device_env(requested_device);

    std::cout << "=================================\n";
    std::cout << " libtorch_module demo\n";
    std::cout << " active profile = " << TorchTestHost::profile_name(profile) << "\n";
    std::cout << " stage id       = " << TorchTestHost::profile_stage_id(profile) << "\n";
    std::cout << " requested device = " << requested_device << "\n";
    if (const char* use_cuda_env = std::getenv("LIBTORCH_MODULE_USE_CUDA")) {
        std::cout << " cuda env       = " << use_cuda_env << "\n";
    } else {
        std::cout << " cuda env       = <auto>\n";
    }
    if (selected_spec != nullptr) {
        for (const auto& line : TorchTestHost::format_task_binding_lines(*selected_spec)) {
            std::cout << " task binding   = " << line << "\n";
        }
        std::cout << " task binding   = requested_device=" << requested_device << "\n";
    }
    std::cout << "=================================\n";

    if (show_steps) {
        print_steps(profile);
    }
    if (show_tasks) {
        if (selected_spec != nullptr) {
            std::cout << "demo tasks  = profile=" << TorchTestHost::profile_name(profile)
                      << " count=1 selected_task=" << selected_spec->task_id << "\n";
            std::cout << "demo task    = " << TorchTestHost::format_task_spec_line(*selected_spec) << "\n";
            for (const auto& line : TorchTestHost::format_task_detail_lines(*selected_spec)) {
                std::cout << "demo task    = " << line << "\n";
            }
        } else {
            const auto specs = TorchTestHost::task_specs_for_profile(profile);
            std::cout << "demo tasks  = profile=" << TorchTestHost::profile_name(profile)
                      << " count=" << specs.size() << "\n";
            for (const auto& spec : specs) {
                std::cout << "demo task    = " << TorchTestHost::format_task_spec_line(spec) << "\n";
                for (const auto& line : TorchTestHost::format_task_detail_lines(spec)) {
                    std::cout << "demo task    = " << line << "\n";
                }
            }
        }
    }

    if (!selected_task.empty()) {
        TorchRuntimeCoreConfig config;
        config.device = requested_device;

        TorchTaskRequestCpp request;
        request.task = selected_task;

        TorchTaskResultCpp result = RunTorchTask(config, request);

        std::cout << "ACTUAL_DEVICE=" << result.actual_device << "\n";
        std::cout << "RUNTIME_MS=" << (result.train_runtime_ms + result.infer_runtime_ms + result.algorithm_runtime_ms) << "\n";
        std::cout << "demo summary = " << result.result_json << "\n";
        std::cout << "demo result  = ok=" << (result.ok ? "true" : "false") << "\n";
        return result.ok ? 0 : 1;
    } else {
        TorchTestHost host;
        const auto report = host.run_profile_report(profile);
        std::cout << "ACTUAL_DEVICE=" << report.actual_device << "\n";
        std::cout << "RUNTIME_MS=" << report.runtime_ms << "\n";
        std::cout << "demo summary = " << report.summary << "\n";
        std::cout << "demo report  = " << TorchTestHost::format_report_line(report) << "\n";
        for (const auto& line : TorchTestHost::demo_focus_lines(profile)) {
            std::cout << "demo focus   = " << line << "\n";
        }
        for (const auto& line : TorchTestHost::format_check_lines(report)) {
            std::cout << "demo check   = " << line << "\n";
        }

        return report.failures;
    }
}