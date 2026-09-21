#include "libtorch_module_runtime_c_api.h"
#include "torch_runtime_core.h"
#include <cstring>
#include <string>

struct TorchRuntimeImpl
{
    TorchRuntimeCoreConfig config;
};

static void copy_string_to_result(const std::string& src, const char*& dst)
{
    if (src.empty()) {
        dst = nullptr;
        return;
    }
    char* buf = new char[src.size() + 1];
    std::strcpy(buf, src.c_str());
    dst = buf;
}

static void free_result_strings(TorchTaskResult* result)
{
    if (result->status) delete[] result->status;
    if (result->error_message) delete[] result->error_message;
    if (result->requested_device) delete[] result->requested_device;
    if (result->actual_device) delete[] result->actual_device;
    if (result->result_json) delete[] result->result_json;
    if (result->evidence_ref) delete[] result->evidence_ref;
    if (result->result_ref) delete[] result->result_ref;
    if (result->input_image_ref) delete[] result->input_image_ref;
    if (result->primary_visual_ref) delete[] result->primary_visual_ref;
    if (result->visualization_refs) delete[] result->visualization_refs;
    if (result->bbox_candidate_list_ref) delete[] result->bbox_candidate_list_ref;
    if (result->roi_crop_packet_ref) delete[] result->roi_crop_packet_ref;
    if (result->attach_back_ref) delete[] result->attach_back_ref;
    if (result->template_alignment_ref) delete[] result->template_alignment_ref;
    if (result->roi_diff_candidate_ref) delete[] result->roi_diff_candidate_ref;
    if (result->trainer_lifecycle_summary) delete[] result->trainer_lifecycle_summary;
    if (result->unified_mainline_summary) delete[] result->unified_mainline_summary;
}

TORCH_RUNTIME_API int torch_runtime_create(
    const TorchRuntimeConfig* config,
    TorchRuntimeHandle* out_handle)
{
    if (!config || !out_handle) {
        return -1;
    }

    try {
        auto* impl = new TorchRuntimeImpl();
        if (config->model_root) impl->config.model_root = config->model_root;
        if (config->output_root) impl->config.output_root = config->output_root;
        if (config->device) impl->config.device = config->device;
        if (config->log_level) impl->config.log_level = config->log_level;
        *out_handle = impl;
        return 0;
    } catch (...) {
        return -1;
    }
}

TORCH_RUNTIME_API int torch_runtime_destroy(
    TorchRuntimeHandle handle)
{
    if (!handle) {
        return -1;
    }

    try {
        delete static_cast<TorchRuntimeImpl*>(handle);
        return 0;
    } catch (...) {
        return -1;
    }
}

TORCH_RUNTIME_API int torch_runtime_run_task(
    TorchRuntimeHandle handle,
    const TorchTaskRequest* request,
    TorchTaskResult* out_result)
{
    if (!handle || !request || !out_result) {
        return -1;
    }

    try {
        TorchRuntimeImpl* impl = static_cast<TorchRuntimeImpl*>(handle);

        TorchTaskRequestCpp cpp_request;
        if (request->task) cpp_request.task = request->task;
        if (request->device) cpp_request.device = request->device;
        if (request->input_image) cpp_request.input_image = request->input_image;
        if (request->dataset_root) cpp_request.dataset_root = request->dataset_root;
        if (request->manifest_path) cpp_request.manifest_path = request->manifest_path;
        if (request->case_name) cpp_request.case_name = request->case_name;
        if (request->extra_json) cpp_request.extra_json = request->extra_json;
        if (request->output_dir) cpp_request.output_dir = request->output_dir;

        TorchTaskResultCpp cpp_result = RunTorchTask(impl->config, cpp_request);

        out_result->ok = cpp_result.ok ? 1 : 0;
        out_result->error_code = cpp_result.error_code;
        out_result->train_runtime_ms = cpp_result.train_runtime_ms;
        out_result->infer_runtime_ms = cpp_result.infer_runtime_ms;
        out_result->algorithm_runtime_ms = cpp_result.algorithm_runtime_ms;
        out_result->placeholder_runtime_ms = cpp_result.placeholder_runtime_ms;

        copy_string_to_result(cpp_result.status, out_result->status);
        copy_string_to_result(cpp_result.error_message, out_result->error_message);
        copy_string_to_result(cpp_result.requested_device, out_result->requested_device);
        copy_string_to_result(cpp_result.actual_device, out_result->actual_device);
        copy_string_to_result(cpp_result.result_json, out_result->result_json);
        copy_string_to_result(cpp_result.evidence_ref, out_result->evidence_ref);
        copy_string_to_result(cpp_result.result_ref, out_result->result_ref);
        copy_string_to_result(cpp_result.input_image_ref, out_result->input_image_ref);
        copy_string_to_result(cpp_result.primary_visual_ref, out_result->primary_visual_ref);
        copy_string_to_result(cpp_result.visualization_refs, out_result->visualization_refs);
        copy_string_to_result(cpp_result.bbox_candidate_list_ref, out_result->bbox_candidate_list_ref);
        copy_string_to_result(cpp_result.roi_crop_packet_ref, out_result->roi_crop_packet_ref);
        copy_string_to_result(cpp_result.attach_back_ref, out_result->attach_back_ref);
        copy_string_to_result(cpp_result.template_alignment_ref, out_result->template_alignment_ref);
        copy_string_to_result(cpp_result.roi_diff_candidate_ref, out_result->roi_diff_candidate_ref);
        copy_string_to_result(cpp_result.trainer_lifecycle_summary, out_result->trainer_lifecycle_summary);
        copy_string_to_result(cpp_result.unified_mainline_summary, out_result->unified_mainline_summary);

        return 0;
    } catch (...) {
        return -1;
    }
}

TORCH_RUNTIME_API void torch_runtime_free_result(
    TorchTaskResult* result)
{
    if (!result) {
        return;
    }

    free_result_strings(result);

    result->ok = 0;
    result->error_code = 0;
    result->train_runtime_ms = 0.0;
    result->infer_runtime_ms = 0.0;
    result->algorithm_runtime_ms = 0.0;
    result->placeholder_runtime_ms = 0.0;
    result->status = nullptr;
    result->error_message = nullptr;
    result->requested_device = nullptr;
    result->actual_device = nullptr;
    result->result_json = nullptr;
    result->evidence_ref = nullptr;
    result->result_ref = nullptr;
    result->input_image_ref = nullptr;
    result->primary_visual_ref = nullptr;
    result->visualization_refs = nullptr;
    result->bbox_candidate_list_ref = nullptr;
    result->roi_crop_packet_ref = nullptr;
    result->attach_back_ref = nullptr;
    result->template_alignment_ref = nullptr;
    result->roi_diff_candidate_ref = nullptr;
    result->trainer_lifecycle_summary = nullptr;
    result->unified_mainline_summary = nullptr;
}

TORCH_RUNTIME_API const char* torch_runtime_version()
{
    static const char version[] = "libtorch_module_runtime 1.0.0";
    return version;
}
