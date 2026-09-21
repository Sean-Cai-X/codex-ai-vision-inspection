#pragma once

#include "TorchRuntimeTypes.h"
#include "../libtorch_module/libtorch_module_runtime_c_api.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <string>

class TorchRuntimeBridge
{
public:
    bool Load(const std::string& dll_path);
    void Unload();
    const std::string& LastErrorMessage() const;

    bool Create(const TorchRuntimeGuiConfig& config);
    TorchRuntimeGuiResult RunTask(const TorchRuntimeGuiRequest& request);
    void Destroy();

    bool IsLoaded() const;
    std::string RuntimeVersion() const;

private:
#ifdef _WIN32
    HMODULE dll_ = nullptr;
#else
    void* dll_ = nullptr;
#endif
    TorchRuntimeHandle handle_ = nullptr;

    using CreateFn = int (*)(const TorchRuntimeConfig*, TorchRuntimeHandle*);
    using DestroyFn = int (*)(TorchRuntimeHandle);
    using RunTaskFn = int (*)(TorchRuntimeHandle, const TorchTaskRequest*, TorchTaskResult*);
    using FreeResultFn = void (*)(TorchTaskResult*);
    using VersionFn = const char* (*)();

    CreateFn create_ = nullptr;
    DestroyFn destroy_ = nullptr;
    RunTaskFn run_task_ = nullptr;
    FreeResultFn free_result_ = nullptr;
    VersionFn version_ = nullptr;
    std::string last_error_message_;
};
