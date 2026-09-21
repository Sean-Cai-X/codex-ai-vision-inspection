#include "CxTorchRuntimeService.h"

namespace {

void CopyGuiResultToResponse(const TorchRuntimeGuiResult& source, CxTorchTaskResponse& target)
{
    target.ok = source.ok;
    target.error_code = source.error_code;

    target.status = source.status;
    target.error_message = source.error_message;

    target.requested_device = source.requested_device;
    target.actual_device = source.actual_device;

    target.train_runtime_ms = source.train_runtime_ms;
    target.infer_runtime_ms = source.infer_runtime_ms;
    target.algorithm_runtime_ms = source.algorithm_runtime_ms;
    target.placeholder_runtime_ms = source.placeholder_runtime_ms;

    target.result_json = source.result_json;
    target.evidence_ref = source.evidence_ref;
    target.result_ref = source.result_ref;

    target.input_image_ref = source.input_image_ref;
    target.primary_visual_ref = source.primary_visual_ref;
    target.visualization_refs = source.visualization_refs;

    target.bbox_candidate_list_ref = source.bbox_candidate_list_ref;
    target.roi_crop_packet_ref = source.roi_crop_packet_ref;
    target.attach_back_ref = source.attach_back_ref;
    target.template_alignment_ref = source.template_alignment_ref;
    target.roi_diff_candidate_ref = source.roi_diff_candidate_ref;

    target.trainer_lifecycle_summary = source.trainer_lifecycle_summary;
    target.unified_mainline_summary = source.unified_mainline_summary;
}

} // namespace

CxTorchRuntimeService::CxTorchRuntimeService() = default;

CxTorchRuntimeService::~CxTorchRuntimeService()
{
    Shutdown();
}

bool CxTorchRuntimeService::Initialize(const CxTorchRuntimeConfig& config, std::string& reason)
{
    Shutdown();

    if (config.runtime_dll_path.empty()) {
        reason = "CxTorchRuntimeService: runtime_dll_path is empty";
        return false;
    }

    if (!bridge_.Load(config.runtime_dll_path)) {
        reason = "CxTorchRuntimeService: failed to load runtime DLL: " + bridge_.LastErrorMessage();
        return false;
    }

    TorchRuntimeGuiConfig gui_config{};
    gui_config.model_root = config.model_root;
    gui_config.output_root = config.output_root;
    gui_config.device = config.device;
    gui_config.log_level = config.log_level;

    if (!bridge_.Create(gui_config)) {
        reason = "CxTorchRuntimeService: failed to create runtime bridge";
        bridge_.Unload();
        return false;
    }

    initialized_ = true;
    return true;
}

bool CxTorchRuntimeService::Execute(
    const CxTorchTaskRequest& request,
    CxTorchTaskResponse& response,
    std::string& reason)
{
    if (!IsReady()) {
        reason = "CxTorchRuntimeService: not initialized";
        return false;
    }

    TorchRuntimeGuiRequest gui_request{};
    gui_request.task = request.task;
    gui_request.device = request.device;
    gui_request.input_image = request.input_image;
    gui_request.dataset_root = request.dataset_root;
    gui_request.manifest_path = request.manifest_path;
    gui_request.case_name = request.case_name;
    gui_request.extra_json = request.extra_json;

    if (!request.output_dir.empty()) {
        gui_request.output_dir = request.output_dir;
    }

    const TorchRuntimeGuiResult gui_result = bridge_.RunTask(gui_request);
    CopyGuiResultToResponse(gui_result, response);

    if (!response.ok) {
        reason = response.error_message.empty()
            ? "CxTorchRuntimeService: task execution failed"
            : "CxTorchRuntimeService: " + response.error_message;
        return false;
    }

    return true;
}

void CxTorchRuntimeService::Shutdown()
{
    bridge_.Unload();
    initialized_ = false;
}

bool CxTorchRuntimeService::IsReady() const noexcept
{
    return initialized_ && bridge_.IsLoaded();
}

std::string CxTorchRuntimeService::RuntimeVersion() const
{
    return bridge_.RuntimeVersion();
}
