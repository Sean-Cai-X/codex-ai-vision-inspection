#include "CxTorchExecutionAdapter.h"
#include "TorchRuntimeResultAdapter.h"
#include <algorithm>
#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>
#ifdef _WIN32
#include <Windows.h>
#endif

namespace
{
std::string EscapePairedJson(const std::string& value)
{
    std::string escaped;
    for (char ch : value)
    {
        if (ch == '\\' || ch == '"')
            escaped += '\\';
        if (ch == '\n')
        {
            escaped += "\\n";
            continue;
        }
        if (ch == '\r')
        {
            escaped += "\\r";
            continue;
        }
        escaped += ch;
    }
    return escaped;
}

std::filesystem::path NormalizedAbsolutePath(const std::filesystem::path& path)
{
    try
    {
        return std::filesystem::absolute(path).lexically_normal();
    }
    catch (...)
    {
        return path.lexically_normal();
    }
}

void WriteComparisonMetrics(
    std::ofstream& file,
    const char* name,
    const CxMaskComparisonSnapshot& comparison,
    bool trailing_comma)
{
    file << "    \"" << name << "\": {"
         << "\"status\": \"" << EscapePairedJson(comparison.status) << "\", "
         << "\"iou\": " << comparison.iou << ", "
         << "\"dice\": " << comparison.dice << ", "
         << "\"boundary_fscore\": " << comparison.boundary_fscore << ", "
         << "\"foreground_ratio_delta\": " << comparison.foreground_ratio_delta
         << "}" << (trailing_comma ? "," : "") << "\n";
}

bool WritePairedDiagnosticReport(
    const CxPairedInferenceRequest& request,
    CxPairedInferenceDiagnostic& diagnostic,
    std::string& reason)
{
    try
    {
        if (request.report_path.empty())
        {
            reason = "paired diagnostic report path is empty";
            return false;
        }
        if (!request.report_path.parent_path().empty())
            std::filesystem::create_directories(request.report_path.parent_path());

        std::ofstream file(request.report_path, std::ios::trunc);
        if (!file)
        {
            reason = "cannot open paired diagnostic report: " + request.report_path.string();
            return false;
        }

        file << "{\n"
             << "  \"schema\": \"cxvision.paired_inference_diagnostic.v1\",\n"
             << "  \"status\": \"" << EscapePairedJson(diagnostic.status) << "\",\n"
             << "  \"reason\": \"" << EscapePairedJson(diagnostic.reason) << "\",\n"
             << "  \"input_image_ref\": \"" << EscapePairedJson(request.parent_task.input_image_path.string()) << "\",\n"
             << "  \"parent_model_id\": \"" << EscapePairedJson(request.parent_task.model_id) << "\",\n"
             << "  \"child_model_id\": \"" << EscapePairedJson(request.child_task.model_id) << "\",\n"
             << "  \"observation\": \"" << EscapePairedJson(diagnostic.observation) << "\",\n"
             << "  \"failure_class\": \"" << EscapePairedJson(diagnostic.failure_class) << "\",\n"
             << "  \"affected_stage\": \"" << EscapePairedJson(diagnostic.affected_stage) << "\",\n"
             << "  \"confidence\": " << diagnostic.confidence << ",\n"
             << "  \"gate_recommendation\": \"" << EscapePairedJson(diagnostic.gate_recommendation) << "\",\n"
             << "  \"promotion_allowed\": false,\n"
             << "  \"human_review_role\": \"process_audit_and_rule_extraction\",\n"
             << "  \"comparisons\": {\n";

        int comparison_count = 1;
        comparison_count += diagnostic.parent_label.has_value() ? 1 : 0;
        comparison_count += diagnostic.child_label.has_value() ? 1 : 0;
        comparison_count += diagnostic.parent_cximage.has_value() ? 1 : 0;
        comparison_count += diagnostic.child_cximage.has_value() ? 1 : 0;
        comparison_count += diagnostic.label_cximage.has_value() ? 1 : 0;
        int comparison_index = 0;
        WriteComparisonMetrics(
            file, "parent_child", diagnostic.parent_child,
            ++comparison_index < comparison_count);
        if (diagnostic.parent_label.has_value())
            WriteComparisonMetrics(file, "parent_label", diagnostic.parent_label.value(), ++comparison_index < comparison_count);
        if (diagnostic.child_label.has_value())
            WriteComparisonMetrics(file, "child_label", diagnostic.child_label.value(), ++comparison_index < comparison_count);
        if (diagnostic.parent_cximage.has_value())
            WriteComparisonMetrics(file, "parent_cximage", diagnostic.parent_cximage.value(), ++comparison_index < comparison_count);
        if (diagnostic.child_cximage.has_value())
            WriteComparisonMetrics(file, "child_cximage", diagnostic.child_cximage.value(), ++comparison_index < comparison_count);
        if (diagnostic.label_cximage.has_value())
            WriteComparisonMetrics(file, "label_cximage", diagnostic.label_cximage.value(), ++comparison_index < comparison_count);
        file << "  },\n"
             << "  \"artifact_refs\": [";
        for (std::size_t i = 0; i < diagnostic.artifact_refs.size(); ++i)
        {
            if (i > 0)
                file << ", ";
            file << "\"" << EscapePairedJson(diagnostic.artifact_refs[i]) << "\"";
        }
        file << "]\n}\n";
        if (!file.good())
        {
            reason = "failed to write paired diagnostic report: " + request.report_path.string();
            return false;
        }
    }
    catch (const std::exception& error)
    {
        reason = error.what();
        return false;
    }

    diagnostic.report_ref = request.report_path.string();
    diagnostic.artifact_refs.push_back(diagnostic.report_ref);
    reason.clear();
    return true;
}

bool SaveComparison(
    const CxMaskComparisonSnapshot& comparison,
    const std::filesystem::path& report_dir,
    const char* filename,
    CxPairedInferenceDiagnostic& diagnostic,
    std::string& reason)
{
    const std::filesystem::path path = report_dir / filename;
    if (!WriteCxMaskComparisonJson(comparison, path.string(), reason))
        return false;
    diagnostic.artifact_refs.push_back(path.string());
    return true;
}

bool LoadPrecomputedInferenceResult(
    const CxTorchTaskSpec& task,
    CxInferenceResult& result,
    std::string& reason)
{
    result = {};
    result.executed = true;
    result.task_id = task.task_id;
    result.case_id = task.case_id;
    result.model_id = task.model_id;
    result.model_hash = task.precomputed_model_hash;
    result.requested_device = task.requested_device;
    result.actual_device = "precomputed";
    result.schema = "cxvision.precomputed_inference_result.v1";
    result.result_ref = task.precomputed_result_ref.string();
    result.evidence_ref = result.result_ref;
    result.primary_visual_ref = task.precomputed_overlay_path.string();

    if (task.precomputed_result_ref.empty() ||
        !std::filesystem::is_regular_file(task.precomputed_result_ref) ||
        task.precomputed_mask_path.empty() ||
        !std::filesystem::is_regular_file(task.precomputed_mask_path))
    {
        result.ok = false;
        result.status = "ASSET_MISSING";
        result.failure_stage = "precomputed_inference_binding";
        result.reason = "precomputed inference result requires physical result_ref and mask_path assets";
        reason = result.reason;
        return false;
    }

    CxMaskFactsSnapshot mask_facts;
    if (!AnalyzeCxMaskFile(task.precomputed_mask_path.string(), mask_facts, reason))
    {
        result.ok = false;
        result.status = mask_facts.status;
        result.failure_stage = "precomputed_mask_analysis";
        result.reason = reason;
        return false;
    }

    CxTorchMask mask;
    mask.available = true;
    mask.mask_ref = task.precomputed_mask_path.string();
    mask.overlay_ref = task.precomputed_overlay_path.string();
    mask.width = mask_facts.width;
    mask.height = mask_facts.height;
    mask.foreground_ratio = mask_facts.foreground_ratio;
    result.mask = mask;
    result.metrics["mask_foreground_pixels"] = mask_facts.foreground_pixels;
    result.metrics["mask_component_count"] = mask_facts.component_count;
    result.metrics["mask_boundary_pixels"] = mask_facts.boundary_pixels;
    result.metrics["mask_bbox_fill_ratio"] = mask_facts.bbox_fill_ratio;
    result.metrics["mask_touches_border"] = mask_facts.touches_border ? 1.0 : 0.0;
    result.artifact_refs.push_back(result.result_ref);
    result.artifact_refs.push_back(mask.mask_ref);
    if (!mask.overlay_ref.empty())
        result.artifact_refs.push_back(mask.overlay_ref);
    result.ok = true;
    result.status = "IMPORTED_INFERENCE_RESULT_BOUND";
    result.reason = "precomputed inference result imported for paired evaluation";
    reason.clear();
    return true;
}
} // namespace

bool CxTorchExecutionAdapter::Execute(const CxTorchTaskSpec& task, CxInferenceResult& result, std::string& reason)
{
    result = {};


    if (!ValidateCxTorchTaskSpec(task, reason)) {
        result.failure_stage = "torch_request_validation";
        result.reason = reason;
        return false;
    }

    if (!task.precomputed_result_ref.empty() || !task.precomputed_mask_path.empty()) {
        return LoadPrecomputedInferenceResult(task, result, reason);
    }

    if (!EnsureRuntime(task, reason)) {

        result.failure_stage = "torch_runtime_initialize";
        result.reason = reason;
        return false;
    }

    CxTorchTaskRequest runtime_request;
    if (!BuildRuntimeRequest(task, runtime_request, reason)) {
        result.failure_stage = "torch_request_conversion";
        result.reason = reason;
        return false;
    }

    CxTorchTaskResponse runtime_response;
    if (!service_.Execute(runtime_request, runtime_response, reason)) {
        result.executed = true;
        result.failure_stage = "torch_task_execute";
        result.reason = reason;
        return false;
    }

    TorchRuntimeGuiResult gui_result;
    gui_result.ok = runtime_response.ok;
    gui_result.error_code = runtime_response.error_code;
    gui_result.status = runtime_response.status;
    gui_result.error_message = runtime_response.error_message;
    gui_result.requested_device = runtime_response.requested_device;
    gui_result.actual_device = runtime_response.actual_device;
    gui_result.train_runtime_ms = runtime_response.train_runtime_ms;
    gui_result.infer_runtime_ms = runtime_response.infer_runtime_ms;
    gui_result.algorithm_runtime_ms = runtime_response.algorithm_runtime_ms;
    gui_result.placeholder_runtime_ms = runtime_response.placeholder_runtime_ms;
    gui_result.result_json = runtime_response.result_json;
    gui_result.evidence_ref = runtime_response.evidence_ref;
    gui_result.result_ref = runtime_response.result_ref;
    gui_result.input_image_ref = runtime_response.input_image_ref;
    gui_result.primary_visual_ref = runtime_response.primary_visual_ref;
    gui_result.visualization_refs = runtime_response.visualization_refs;
    gui_result.bbox_candidate_list_ref = runtime_response.bbox_candidate_list_ref;
    gui_result.roi_crop_packet_ref = runtime_response.roi_crop_packet_ref;
    gui_result.attach_back_ref = runtime_response.attach_back_ref;
    gui_result.template_alignment_ref = runtime_response.template_alignment_ref;
    gui_result.roi_diff_candidate_ref = runtime_response.roi_diff_candidate_ref;
    gui_result.trainer_lifecycle_summary = runtime_response.trainer_lifecycle_summary;
    gui_result.unified_mainline_summary = runtime_response.unified_mainline_summary;

    if (!TorchRuntimeResultAdapter::AdaptToInferenceResult(gui_result, task, result, reason)) {
        result.failure_stage = "torch_result_parse";
        result.reason = reason;
        return false;
    }

    reason.clear();
    return true;
}

bool CxTorchExecutionAdapter::ExecutePair(
    const CxPairedInferenceRequest& request,
    CxPairedInferenceDiagnostic& diagnostic,
    std::string& reason)
{
    diagnostic = {};
    diagnostic.executed = true;
    diagnostic.affected_stage = "paired_inference_diagnostic";

    if (NormalizedAbsolutePath(request.parent_task.input_image_path) !=
        NormalizedAbsolutePath(request.child_task.input_image_path))
    {
        diagnostic.status = "PAIRED_INPUT_MISMATCH";
        diagnostic.reason = "parent and child must run on the same input image";
        reason = diagnostic.reason;
        return false;
    }
    if (request.parent_task.requested_device != request.child_task.requested_device)
    {
        diagnostic.status = "PAIRED_DEVICE_MISMATCH";
        diagnostic.reason = "parent and child must request the same device";
        reason = diagnostic.reason;
        return false;
    }
    if (NormalizedAbsolutePath(request.parent_task.output_dir) ==
        NormalizedAbsolutePath(request.child_task.output_dir))
    {
        diagnostic.status = "PAIRED_OUTPUT_COLLISION";
        diagnostic.reason = "parent and child output directories must differ";
        reason = diagnostic.reason;
        return false;
    }

    std::string cximage_mask_ref;
    if (request.cximage_candidate_request.has_value())
    {
        CxImageReferenceCandidateRequest candidate_request =
            request.cximage_candidate_request.value();
        if (candidate_request.input_image_path.empty())
            candidate_request.input_image_path = request.parent_task.input_image_path;
        if (NormalizedAbsolutePath(candidate_request.input_image_path) !=
            NormalizedAbsolutePath(request.parent_task.input_image_path))
        {
            diagnostic.status = "REFERENCE_INPUT_MISMATCH";
            diagnostic.reason = "cximage candidate must use the paired input image";
            reason = diagnostic.reason;
            return false;
        }

        CxImageReferenceCandidateResult candidate_result;
        CxImageReferenceCandidateGenerator generator;
        if (!generator.Generate(candidate_request, candidate_result, reason))
        {
            diagnostic.cximage_candidate = candidate_result;
            diagnostic.status = candidate_result.status;
            diagnostic.reason = candidate_result.reason;
            diagnostic.failure_class = "cximage_reference_candidate_unavailable";
            return false;
        }
        cximage_mask_ref = candidate_result.mask_ref;
        diagnostic.artifact_refs.push_back(candidate_result.mask_ref);
        diagnostic.artifact_refs.push_back(candidate_result.overlay_ref);
        diagnostic.artifact_refs.push_back(candidate_result.summary_ref);
        diagnostic.cximage_candidate = candidate_result;
    }

    // The shared runtime service is invoked synchronously: parent completes
    // before child starts, preserving the single-owner execution boundary.
    if (!Execute(request.parent_task, diagnostic.parent_result, reason))
    {
        diagnostic.status = "PARENT_EXECUTION_FAILED";
        diagnostic.reason = reason;
        diagnostic.failure_class = "parent_runtime_failure";
        diagnostic.affected_stage = diagnostic.parent_result.failure_stage;
        return false;
    }
    if (!Execute(request.child_task, diagnostic.child_result, reason))
    {
        diagnostic.status = "CHILD_EXECUTION_FAILED";
        diagnostic.reason = reason;
        diagnostic.failure_class = "child_runtime_failure";
        diagnostic.affected_stage = diagnostic.child_result.failure_stage;
        return false;
    }

    if (!diagnostic.parent_result.mask.has_value() ||
        !diagnostic.child_result.mask.has_value())
    {
        diagnostic.status = "PENDING_MASK_BINDING";
        diagnostic.reason = "paired runtime completed without both mask assets";
        diagnostic.observation = "mask evaluator could not compare parent and child";
        diagnostic.failure_class = "runtime_mask_binding_missing";
        diagnostic.gate_recommendation = "insufficient_evidence";
        if (!WritePairedDiagnosticReport(request, diagnostic, reason))
            return false;
        reason.clear();
        return true;
    }

    const std::string parent_mask = diagnostic.parent_result.mask->mask_ref;
    const std::string child_mask = diagnostic.child_result.mask->mask_ref;
    if (!CompareCxMaskFiles(parent_mask, child_mask, diagnostic.parent_child, reason))
    {
        diagnostic.status = diagnostic.parent_child.status;
        diagnostic.reason = reason;
        diagnostic.failure_class = "mask_evaluator_failure";
        return false;
    }

    const std::filesystem::path report_dir = request.report_path.parent_path();
    if (!SaveComparison(
            diagnostic.parent_child, report_dir,
            "parent_child_mask_comparison.json", diagnostic, reason))
        return false;

    if (!request.dataset_label_mask_path.empty())
    {
        CxMaskComparisonSnapshot parent_label;
        CxMaskComparisonSnapshot child_label;
        if (!CompareCxMaskFiles(parent_mask, request.dataset_label_mask_path.string(), parent_label, reason) ||
            !CompareCxMaskFiles(child_mask, request.dataset_label_mask_path.string(), child_label, reason))
        {
            diagnostic.status = "LABEL_MASK_EVALUATION_FAILED";
            diagnostic.reason = reason;
            diagnostic.failure_class = "dataset_label_invalid";
            return false;
        }
        diagnostic.parent_label = parent_label;
        diagnostic.child_label = child_label;
        if (!SaveComparison(parent_label, report_dir, "parent_label_mask_comparison.json", diagnostic, reason) ||
            !SaveComparison(child_label, report_dir, "child_label_mask_comparison.json", diagnostic, reason))
            return false;
    }

    if (!cximage_mask_ref.empty())
    {
        CxMaskComparisonSnapshot parent_cximage;
        CxMaskComparisonSnapshot child_cximage;
        if (!CompareCxMaskFiles(parent_mask, cximage_mask_ref, parent_cximage, reason) ||
            !CompareCxMaskFiles(child_mask, cximage_mask_ref, child_cximage, reason))
        {
            diagnostic.status = "CXIMAGE_MASK_EVALUATION_FAILED";
            diagnostic.reason = reason;
            diagnostic.failure_class = "cximage_candidate_invalid";
            return false;
        }
        diagnostic.parent_cximage = parent_cximage;
        diagnostic.child_cximage = child_cximage;
        if (!SaveComparison(parent_cximage, report_dir, "parent_cximage_mask_comparison.json", diagnostic, reason) ||
            !SaveComparison(child_cximage, report_dir, "child_cximage_mask_comparison.json", diagnostic, reason))
            return false;

        if (!request.dataset_label_mask_path.empty())
        {
            CxMaskComparisonSnapshot label_cximage;
            if (!CompareCxMaskFiles(
                    request.dataset_label_mask_path.string(),
                    cximage_mask_ref,
                    label_cximage,
                    reason))
            {
                diagnostic.status = "FOUR_WAY_MASK_EVALUATION_FAILED";
                diagnostic.reason = reason;
                diagnostic.failure_class = "label_cximage_comparison_invalid";
                return false;
            }
            diagnostic.label_cximage = label_cximage;
            if (!SaveComparison(
                    label_cximage,
                    report_dir,
                    "label_cximage_mask_comparison.json",
                    diagnostic,
                    reason))
                return false;
        }
    }

    if (diagnostic.parent_label.has_value() && diagnostic.child_label.has_value())
    {
        const double delta = diagnostic.child_label->iou - diagnostic.parent_label->iou;
        diagnostic.confidence = (std::min)(
            1.0,
            std::abs(delta) / (std::max)(request.material_iou_delta, 0.000001));
        if (delta > request.material_iou_delta)
        {
            diagnostic.observation = "child label IoU improved on the paired sample";
            diagnostic.failure_class = "target_improvement_candidate";
            diagnostic.gate_recommendation = "continue_set_validation";
        }
        else if (delta < -request.material_iou_delta)
        {
            diagnostic.observation = "child label IoU regressed on the paired sample";
            diagnostic.failure_class = "regression_candidate";
            diagnostic.gate_recommendation = "recommend_reject_regression";
        }
        else
        {
            diagnostic.observation = "no material label IoU change on the paired sample";
            diagnostic.failure_class = "no_material_label_change";
            diagnostic.gate_recommendation = "hold_no_material_change";
        }
    }
    else if (diagnostic.parent_child.iou >= request.stable_pair_iou)
    {
        diagnostic.observation = "parent and child masks are stable on the paired sample";
        diagnostic.failure_class = "stable_no_material_change";
        diagnostic.confidence = diagnostic.parent_child.iou;
    }
    else if (diagnostic.parent_child.foreground_ratio_delta > request.material_foreground_delta)
    {
        diagnostic.observation = "child foreground expanded materially";
        diagnostic.failure_class = "boundary_leak_candidate";
        diagnostic.confidence = (std::min)(
            1.0, diagnostic.parent_child.foreground_ratio_delta);
    }
    else if (diagnostic.parent_child.foreground_ratio_delta < -request.material_foreground_delta)
    {
        diagnostic.observation = "child foreground contracted materially";
        diagnostic.failure_class = "missing_region_candidate";
        diagnostic.confidence = (std::min)(
            1.0, -diagnostic.parent_child.foreground_ratio_delta);
    }
    else if (diagnostic.parent_child.boundary_fscore < request.minimum_boundary_fscore)
    {
        diagnostic.observation = "parent and child boundaries differ materially";
        diagnostic.failure_class = "boundary_change_candidate";
        diagnostic.confidence = 1.0 - diagnostic.parent_child.boundary_fscore;
    }
    else
    {
        diagnostic.observation = "paired masks changed without a decisive reference";
        diagnostic.failure_class = "changed_requires_reference";
        diagnostic.confidence = 1.0 - diagnostic.parent_child.iou;
    }

    diagnostic.complete = true;
    diagnostic.status = "PAIRED_DIAGNOSTIC_COMPLETE";
    diagnostic.reason = "single-image diagnostic complete; promotion remains disabled";
    if (!WritePairedDiagnosticReport(request, diagnostic, reason))
        return false;

    reason.clear();
    return true;
}

bool CxTorchExecutionAdapter::EnsureRuntime(const CxTorchTaskSpec& task, std::string& reason)
{
    if (service_.IsReady()) {
        return true;
    }

    std::filesystem::path exe_dir;
#ifdef _WIN32
    wchar_t exe_path[MAX_PATH];
    if (GetModuleFileNameW(nullptr, exe_path, MAX_PATH) > 0) {
        exe_dir = std::filesystem::path(exe_path).parent_path();
    } else {
        exe_dir = std::filesystem::current_path();
    }
#else
    exe_dir = std::filesystem::current_path();
#endif

    auto dll_path = exe_dir / "libtorch_module_runtime.dll";

    CxTorchRuntimeConfig config;
    config.runtime_dll_path = dll_path.string();
    config.device = task.requested_device;

    return service_.Initialize(config, reason);
}

bool CxTorchExecutionAdapter::BuildRuntimeRequest(const CxTorchTaskSpec& task, CxTorchTaskRequest& request, std::string& reason) const
{
    request = {};

    request.task = task.task_id;
    request.device = task.requested_device;
    request.input_image = task.input_image_path.string();
    request.dataset_root = task.dataset_root.string();
    request.manifest_path = task.manifest_path.string();
    request.case_name = task.case_id;
    request.extra_json = task.extra_json;
    request.output_dir = task.output_dir.string();

    if (request.task.empty()) {
        reason = "runtime task is empty";
        return false;
    }

    reason.clear();
    return true;
}