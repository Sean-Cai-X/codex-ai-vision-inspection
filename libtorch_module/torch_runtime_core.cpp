#include "torch_runtime_core.h"
#include "torch_runtime_task_dispatcher.h"
#include <torch/torch.h>

static void apply_requested_device_env(const std::string& requested_device) {
#ifdef _WIN32
    if (requested_device == "cpu") {
        _putenv_s("LIBTORCH_MODULE_USE_CUDA", "0");
        return;
    }
    if (requested_device == "gpu" || requested_device == "cuda") {
        _putenv_s("LIBTORCH_MODULE_USE_CUDA", "1");
        return;
    }
    _putenv_s("LIBTORCH_MODULE_USE_CUDA", "");
#else
    if (requested_device == "cpu") {
        setenv("LIBTORCH_MODULE_USE_CUDA", "0", 1);
        return;
    }
    if (requested_device == "gpu" || requested_device == "cuda") {
        setenv("LIBTORCH_MODULE_USE_CUDA", "1", 1);
        return;
    }
    unsetenv("LIBTORCH_MODULE_USE_CUDA");
#endif
}

TorchTaskResultCpp RunTorchTask(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request)
{
    try
    {
        TorchRuntimeCoreConfig effective_config = config;
        if (!request.device.empty()) {
            effective_config.device = request.device;
        }

        const std::string& requested_device = !effective_config.device.empty() ? effective_config.device : "auto";
        apply_requested_device_env(requested_device);

        return DispatchTorchRuntimeTask(effective_config, request);
    }
    catch (const c10::Error& error)
    {
        TorchTaskResultCpp result;
        result.ok = false;
        result.error_code = 1601;
        result.status = "torch_exception";
        result.error_message = error.what();
        return result;
    }
    catch (const std::exception& error)
    {
        TorchTaskResultCpp result;
        result.ok = false;
        result.error_code = -1;
        result.status = "exception";
        result.error_message = error.what();
        return result;
    }
    catch (...)
    {
        TorchTaskResultCpp result;
        result.ok = false;
        result.error_code = -2;
        result.status = "unknown_exception";
        result.error_message = "Unknown exception during torch task";
        return result;
    }
}
