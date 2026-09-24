#include "libtorch_module_runtime_c_api.h"

#include <cstring>
#include <iostream>

namespace
{
bool Expect(const bool condition, const char* message)
{
    if (condition)
        return true;
    std::cerr << "FAILED: " << message << "\n";
    return false;
}

int AlwaysCancel(void*)
{
    return 1;
}
} // namespace

int main()
{
    TorchRuntimeConfig config{};
    config.device = "cpu";
    TorchRuntimeHandle handle = nullptr;
    if (!Expect(
            torch_runtime_create(&config, &handle) == 0 && handle != nullptr,
            "runtime creation"))
    {
        return 1;
    }

    TorchTaskRequest empty_request{};
    TorchTaskResult result{};
    torch_runtime_init_result(&result);
    bool passed = Expect(
        torch_runtime_run_business_trial_v1(
            handle, &empty_request, nullptr, nullptr, nullptr, &result) == 0,
        "business trial ABI call") &&
        Expect(result.ok == 0, "invalid business trial must not succeed") &&
        Expect(result.error_message != nullptr &&
                   std::strcmp(
                       result.error_message,
                       "TRAINING_PARAMETER_INVALID") == 0,
               "invalid business trial error contract");
    torch_runtime_free_result(&result);

    // A v1 call with a non-null output must return a release-safe failure
    // record even when the opaque runtime handle is invalid.  This exercises
    // the ABI boundary without needing a model, image or dataset.
    TorchTaskResult invalid_handle_result{};
    torch_runtime_init_result(&invalid_handle_result);
    passed = passed && Expect(
        torch_runtime_run_business_trial_v1(
            nullptr, &empty_request, nullptr, nullptr, nullptr,
            &invalid_handle_result) != 0,
        "invalid business handle return") &&
        Expect(invalid_handle_result.ok == 0,
               "invalid business handle must not succeed") &&
        Expect(invalid_handle_result.error_message != nullptr &&
                   std::strcmp(
                       invalid_handle_result.error_message,
                       "RUNTIME_ARGUMENT_INVALID") == 0,
               "invalid business handle safe failure contract");
    torch_runtime_free_result(&invalid_handle_result);

    TorchTaskResult cancellation_result{};
    torch_runtime_init_result(&cancellation_result);
    passed = passed && Expect(
        torch_runtime_run_business_trial_v1(
            handle, &empty_request, nullptr, AlwaysCancel, nullptr,
            &cancellation_result) == 0,
        "business cancellation ABI call") &&
        Expect(cancellation_result.ok == 0, "cancelled trial must not succeed") &&
        Expect(cancellation_result.status != nullptr &&
                   std::strcmp(cancellation_result.status, "cancelled") == 0,
               "business cancellation status contract");
    torch_runtime_free_result(&cancellation_result);

    TorchTaskResult inference_result{};
    torch_runtime_init_result(&inference_result);
    passed = passed && Expect(
        torch_runtime_run_business_inference_v1(
            handle, &empty_request, &inference_result) == 0,
        "business inference ABI call") &&
        Expect(inference_result.ok == 0, "invalid business inference must not succeed") &&
        Expect(inference_result.error_message != nullptr &&
                   std::strcmp(
                       inference_result.error_message,
                       "BUSINESS_INFERENCE_OUTPUT_SCOPE_INVALID") == 0,
               "business inference output scope contract");
    torch_runtime_free_result(&inference_result);

    passed = passed &&
        Expect(torch_runtime_destroy(handle) == 0, "runtime destruction");
    return passed ? 0 : 1;
}
