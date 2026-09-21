#include "TorchRuntimeResultAdapter.h"
#include <cmath>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <initializer_list>
#include <sstream>

namespace
{

bool IsSegmentationTask(const CxTorchTaskSpec& task)
{
    return task.kind == CxTorchTaskKind::Segmentation ||
           task.kind == CxTorchTaskKind::SegmentationContract ||
           task.task_id.find("segmentation") != std::string::npos ||
           task.task_id.find("deeplab") != std::string::npos;
}

bool IsDetectionTask(const CxTorchTaskSpec& task)
{
    return task.kind == CxTorchTaskKind::Detection ||
           task.kind == CxTorchTaskKind::DetectionContract ||
           task.task_id.find("detection") != std::string::npos ||
           task.task_id.find("yolo") != std::string::npos;
}

bool ReadTorchAdapterTextFile(
    const std::filesystem::path& path,
    std::string& text)
{
    text.clear();
    if (path.empty() || !std::filesystem::exists(path))
        return false;

    std::ifstream input(path);
    if (!input)
        return false;

    text.assign(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
    return true;
}

bool ExtractTorchAdapterJsonNumber(
    const std::string& json,
    const std::string& key,
    double& value)
{
    const std::string needle = "\"" + key + "\"";
    const std::size_t key_pos = json.find(needle);
    if (key_pos == std::string::npos)
        return false;

    const std::size_t colon_pos = json.find(':', key_pos + needle.size());
    if (colon_pos == std::string::npos)
        return false;

    const char* cursor = json.c_str() + colon_pos + 1;
    while (*cursor != '\0' && std::isspace(static_cast<unsigned char>(*cursor)))
        ++cursor;

    char* end = nullptr;
    const double parsed = std::strtod(cursor, &end);
    if (end == cursor)
        return false;

    value = parsed;
    return true;
}

bool ExtractTorchAdapterJsonNumberAny(
    const std::string& json,
    const std::initializer_list<const char*>& keys,
    double& value)
{
    for (const char* key : keys)
    {
        if (key != nullptr && ExtractTorchAdapterJsonNumber(json, key, value))
            return true;
    }
    return false;
}

// OBB receipts must carry an explicit angle unit. A bare `angle` is rejected
// so radians and degrees cannot be silently mixed at the FastMatch boundary.
bool AttachOrientedBoxGeometry(
    const std::string& json,
    CxTorchDetection& detection)
{
    double center_x = 0.0;
    double center_y = 0.0;
    double width = 0.0;
    double height = 0.0;
    double angle = 0.0;
    if (!ExtractTorchAdapterJsonNumberAny(json, {"cx", "center_x"}, center_x) ||
        !ExtractTorchAdapterJsonNumberAny(json, {"cy", "center_y"}, center_y) ||
        !ExtractTorchAdapterJsonNumberAny(json, {"w", "width", "obb_width"}, width) ||
        !ExtractTorchAdapterJsonNumberAny(json, {"h", "height", "obb_height"}, height) ||
        !std::isfinite(center_x) || !std::isfinite(center_y) ||
        !std::isfinite(width) || !std::isfinite(height) ||
        width <= 0.0 || height <= 0.0)
    {
        return false;
    }

    bool angle_is_radians = false;
    if (!ExtractTorchAdapterJsonNumberAny(json,
            {"angle_deg", "rotation_deg", "theta_deg"}, angle))
    {
        if (!ExtractTorchAdapterJsonNumberAny(json,
                {"angle_rad", "rotation_rad", "theta_rad"}, angle))
            return false;
        angle_is_radians = true;
    }

    if (!std::isfinite(angle))
        return false;

    constexpr double kRadiansToDegrees = 57.2957795130823208768;
    detection.oriented_box_available = true;
    detection.oriented_center_x = center_x;
    detection.oriented_center_y = center_y;
    detection.oriented_width = width;
    detection.oriented_height = height;
    detection.oriented_angle_deg = angle_is_radians ? angle * kRadiansToDegrees : angle;
    detection.oriented_angle_convention = angle_is_radians
        ? "image_clockwise_radians_converted_to_degrees"
        : "image_clockwise_degrees";
    return true;
}

void AttachDetectionResults(
    const TorchRuntimeGuiResult& source,
    const CxTorchTaskSpec& task,
    CxInferenceResult& target)
{
    if (!IsDetectionTask(task))
        return;

    std::filesystem::path detections_path;
    if (!source.bbox_candidate_list_ref.empty())
        detections_path = source.bbox_candidate_list_ref;

    if (detections_path.empty() && !source.result_ref.empty())
        detections_path = std::filesystem::path(source.result_ref).parent_path() / "detections.json";

    std::string json;
    if (!ReadTorchAdapterTextFile(detections_path, json))
        return;

    const std::size_t detections_key = json.find("\"detections\"");
    if (detections_key == std::string::npos)
        return;

    const std::size_t array_begin = json.find('[', detections_key);
    if (array_begin == std::string::npos)
        return;

    int depth = 0;
    std::size_t array_end = std::string::npos;
    for (std::size_t i = array_begin; i < json.size(); ++i)
    {
        if (json[i] == '[')
            ++depth;
        else if (json[i] == ']')
        {
            --depth;
            if (depth == 0)
            {
                array_end = i;
                break;
            }
        }
    }

    if (array_end == std::string::npos)
        return;

    int source_index = 0;
    std::size_t object_pos = json.find('{', array_begin);
    while (object_pos != std::string::npos && object_pos < array_end)
    {
        const std::size_t object_end = json.find('}', object_pos);
        if (object_end == std::string::npos || object_end > array_end)
            break;

        const std::string object_json = json.substr(object_pos, object_end - object_pos + 1);

        double x1 = 0.0;
        double y1 = 0.0;
        double x2 = 0.0;
        double y2 = 0.0;
        double confidence = 0.0;
        double class_id = -1.0;

        const bool has_box =
            ExtractTorchAdapterJsonNumber(object_json, "x1", x1) &&
            ExtractTorchAdapterJsonNumber(object_json, "y1", y1) &&
            ExtractTorchAdapterJsonNumber(object_json, "x2", x2) &&
            ExtractTorchAdapterJsonNumber(object_json, "y2", y2);

        CxTorchDetection detection;
        detection.source_index = source_index;
        const bool has_oriented_box = AttachOrientedBoxGeometry(object_json, detection);
        if (has_box || has_oriented_box)
        {
            ExtractTorchAdapterJsonNumber(object_json, "confidence", confidence);
            ExtractTorchAdapterJsonNumber(object_json, "class_id", class_id);

            if (has_box)
            {
                detection.x = x1;
                detection.y = y1;
                detection.width = x2 - x1;
                detection.height = y2 - y1;
            }
            else
            {
                detection.x = detection.oriented_center_x - detection.oriented_width * 0.5;
                detection.y = detection.oriented_center_y - detection.oriented_height * 0.5;
                detection.width = detection.oriented_width;
                detection.height = detection.oriented_height;
            }
            detection.confidence = confidence;
            detection.class_id = static_cast<int>(class_id);

            if (detection.width > 0.0 && detection.height > 0.0)
                target.detections.push_back(detection);
        }

        ++source_index;
        object_pos = json.find('{', object_end + 1);
    }
}

void AttachInstanceSegmentationResults(
    const TorchRuntimeGuiResult& source,
    const CxTorchTaskSpec& task,
    CxInferenceResult& target)
{
    if (task.task_id.find("instance_segmentation") == std::string::npos ||
        source.result_ref.empty())
    {
        return;
    }

    std::string json;
    if (!ReadTorchAdapterTextFile(source.result_ref, json))
        return;

    std::size_t instance_pos = json.find(R"("stable_id")");
    while (instance_pos != std::string::npos)
    {
        const std::size_t next_instance =
            json.find(R"("stable_id")", instance_pos + 11);
        const std::string instance_json = json.substr(
            instance_pos,
            next_instance == std::string::npos
                ? std::string::npos
                : next_instance - instance_pos);

        double x0 = 0.0;
        double y0 = 0.0;
        double x1 = 0.0;
        double y1 = 0.0;
        double confidence = 0.0;
        double class_id = -1.0;
        const bool has_box =
            ExtractTorchAdapterJsonNumber(instance_json, "x0", x0) &&
            ExtractTorchAdapterJsonNumber(instance_json, "y0", y0) &&
            ExtractTorchAdapterJsonNumber(instance_json, "x1", x1) &&
            ExtractTorchAdapterJsonNumber(instance_json, "y1", y1);

        if (has_box && x1 > x0 && y1 > y0)
        {
            ExtractTorchAdapterJsonNumber(
                instance_json, "class_confidence", confidence);
            ExtractTorchAdapterJsonNumber(
                instance_json, "class_id", class_id);

            CxTorchDetection detection;
            detection.x = x0;
            detection.y = y0;
            detection.width = x1 - x0;
            detection.height = y1 - y0;
            detection.confidence = confidence;
            detection.class_id = static_cast<int>(class_id);
            target.detections.push_back(detection);
        }

        instance_pos = next_instance;
    }

    double instance_count = 0.0;
    if (ExtractTorchAdapterJsonNumber(
            source.result_json, "instance_count", instance_count))
    {
        target.metrics["instance_count"] = instance_count;
    }
}

void AttachSegmentationMaskRefs(
    const TorchRuntimeGuiResult& source,
    const CxTorchTaskSpec& task,
    CxInferenceResult& target)
{
    if (!IsSegmentationTask(task) || source.result_ref.empty()) {
        return;
    }

    const std::filesystem::path result_path(source.result_ref);
    const std::filesystem::path result_dir = result_path.parent_path();
    std::filesystem::path mask_path = result_dir / "mask_binary.png";
    const std::filesystem::path contour_path = result_dir / "contours.json";
    std::filesystem::path overlay_path = source.primary_visual_ref.empty()
        ? result_dir / "mask_overlay.png"
        : std::filesystem::path(source.primary_visual_ref);

    if (!std::filesystem::is_regular_file(mask_path)) {
        const std::size_t separator = source.visualization_refs.find(';');
        if (separator != std::string::npos) {
            mask_path = source.visualization_refs.substr(0, separator);
            if (overlay_path.empty())
                overlay_path = source.visualization_refs.substr(separator + 1);
        }
    }

    if (!std::filesystem::is_regular_file(mask_path)) {
        return;
    }

    CxTorchMask mask;
    mask.available = true;
    mask.mask_ref = mask_path.string();
    if (std::filesystem::exists(contour_path)) {
        mask.contour_ref = contour_path.string();
    }
    if (std::filesystem::exists(overlay_path)) {
        mask.overlay_ref = overlay_path.string();
    }

    CxMaskFactsSnapshot mask_facts;
    std::string mask_reason;
    if (AnalyzeCxMaskFile(mask.mask_ref, mask_facts, mask_reason)) {
        mask.width = mask_facts.width;
        mask.height = mask_facts.height;
        mask.foreground_ratio = mask_facts.foreground_ratio;
        target.metrics["mask_foreground_pixels"] = mask_facts.foreground_pixels;
        target.metrics["mask_component_count"] = mask_facts.component_count;
        target.metrics["mask_boundary_pixels"] = mask_facts.boundary_pixels;
        target.metrics["mask_bbox_fill_ratio"] = mask_facts.bbox_fill_ratio;
        target.metrics["mask_touches_border"] = mask_facts.touches_border ? 1.0 : 0.0;
        target.mask = mask;
    }
}

} // namespace

bool TorchRuntimeResultAdapter::AdaptToInferenceResult(
    const TorchRuntimeGuiResult& source,
    const CxTorchTaskSpec& task,
    CxInferenceResult& target,
    std::string& reason)
{
    target = {};

    target.executed = true;
    target.ok = source.ok;
    target.error_code = source.error_code;

    target.task_id = task.task_id;
    target.case_id = task.case_id;
    target.model_id = task.model_id;

    target.requested_device = source.requested_device;
    target.actual_device = source.actual_device;

    target.status = source.status;
    target.reason = source.error_message;

    target.train_runtime_ms = source.train_runtime_ms;
    target.infer_runtime_ms = source.infer_runtime_ms;
    target.algorithm_runtime_ms = source.algorithm_runtime_ms;
    target.total_runtime_ms =
        source.train_runtime_ms +
        source.infer_runtime_ms +
        source.algorithm_runtime_ms +
        source.placeholder_runtime_ms;

    target.raw_result_json = source.result_json;
    target.evidence_ref = source.evidence_ref;
    target.result_ref = source.result_ref;
    target.primary_visual_ref = source.primary_visual_ref;
    target.bbox_candidate_list_ref = source.bbox_candidate_list_ref;
    target.roi_crop_packet_ref = source.roi_crop_packet_ref;
    target.template_alignment_ref = source.template_alignment_ref;
    target.roi_diff_candidate_ref = source.roi_diff_candidate_ref;
    target.trainer_lifecycle_summary = source.trainer_lifecycle_summary;
    target.unified_mainline_summary = source.unified_mainline_summary;

    AttachSegmentationMaskRefs(source, task, target);
    AttachInstanceSegmentationResults(source, task, target);
    AttachDetectionResults(source, task, target);

    if (!source.ok) {
        target.failure_stage = "torch_task_execute";
        reason = target.reason.empty() ? "torch task returned failure" : target.reason;
        return false;
    }

    reason.clear();
    return true;
}

TorchRuntimeGuiReview TorchRuntimeResultAdapter::AdaptToGuiReview(const CxInferenceResult& result)
{
    TorchRuntimeGuiReview review;

    review.stages.push_back({"torch_runtime_start", "Torch Runtime Start", "completed"});
    review.stages.push_back({"torch_task_execution", "Task Execution", result.status});
    review.stages.push_back({"torch_runtime_end", "Torch Runtime End", result.ok ? "success" : "failed"});

    if (!result.primary_visual_ref.empty()) {
        review.image_refs.push_back({"primary_visual", result.primary_visual_ref, "Primary Visual"});
    }
    if (result.mask.has_value()) {
        const CxTorchMask& mask = result.mask.value();
        if (!mask.mask_ref.empty()) {
            review.image_refs.push_back({"segmentation_mask", mask.mask_ref, "Segmentation Mask"});
        }
        if (!mask.overlay_ref.empty()) {
            review.image_refs.push_back({"segmentation_overlay", mask.overlay_ref, "Segmentation Overlay"});
        }
        if (!mask.contour_ref.empty()) {
            review.result_fields.push_back({"Mask Contours", mask.contour_ref, ""});
        }
    }
    if (!result.bbox_candidate_list_ref.empty()) {
        review.image_refs.push_back({"bbox_candidates", result.bbox_candidate_list_ref, "BBox Candidates"});
    }
    if (!result.roi_crop_packet_ref.empty()) {
        review.image_refs.push_back({"roi_crops", result.roi_crop_packet_ref, "ROI Crops"});
    }
    if (!result.template_alignment_ref.empty()) {
        review.image_refs.push_back({"template_alignment", result.template_alignment_ref, "Template Alignment"});
    }
    if (!result.roi_diff_candidate_ref.empty()) {
        review.image_refs.push_back({"roi_diff", result.roi_diff_candidate_ref, "ROI Diff"});
    }

    review.result_fields.push_back({"Status", result.status, ""});
    review.result_fields.push_back({"Requested Device", result.requested_device, ""});
    review.result_fields.push_back({"Actual Device", result.actual_device, ""});

    if (result.train_runtime_ms > 0) {
        std::ostringstream os;
        os << std::fixed << result.train_runtime_ms;
        review.result_fields.push_back({"Train Runtime", os.str(), "ms"});
    }
    if (result.infer_runtime_ms > 0) {
        std::ostringstream os;
        os << std::fixed << result.infer_runtime_ms;
        review.result_fields.push_back({"Infer Runtime", os.str(), "ms"});
    }
    if (result.algorithm_runtime_ms > 0) {
        std::ostringstream os;
        os << std::fixed << result.algorithm_runtime_ms;
        review.result_fields.push_back({"Algorithm Runtime", os.str(), "ms"});
    }

    if (!result.result_ref.empty()) {
        review.result_fields.push_back({"Result Ref", result.result_ref, ""});
    }
    if (!result.evidence_ref.empty()) {
        review.result_fields.push_back({"Evidence Ref", result.evidence_ref, ""});
    }
    if (!result.trainer_lifecycle_summary.empty()) {
        review.result_fields.push_back({"Trainer Lifecycle Summary", result.trainer_lifecycle_summary, ""});
    }
    if (!result.unified_mainline_summary.empty()) {
        review.result_fields.push_back({"Unified Mainline Summary", result.unified_mainline_summary, ""});
    }
    if (!result.detections.empty()) {
        review.result_fields.push_back({"Detection Count", std::to_string(result.detections.size()), ""});
    }
    if (result.mask.has_value()) {
        const CxTorchMask& mask = result.mask.value();
        review.result_fields.push_back({"Mask Available", mask.available ? "true" : "false", ""});
        review.result_fields.push_back({"Mask Width", std::to_string(mask.width), "px"});
        review.result_fields.push_back({"Mask Height", std::to_string(mask.height), "px"});
        {
            std::ostringstream os;
            os << std::fixed << mask.foreground_ratio;
            review.result_fields.push_back({"Mask Foreground Ratio", os.str(), ""});
        }
        const auto component_it = result.metrics.find("mask_component_count");
        if (component_it != result.metrics.end()) {
            review.result_fields.push_back({
                "Mask Component Count",
                std::to_string(static_cast<int>(component_it->second)),
                ""});
        }
        if (!mask.mask_ref.empty()) {
            review.result_fields.push_back({"Mask Ref", mask.mask_ref, ""});
        }
        if (!mask.overlay_ref.empty()) {
            review.result_fields.push_back({"Mask Overlay Ref", mask.overlay_ref, ""});
        }
    }

    if (!result.reason.empty()) {
        review.issue_refs.push_back("error: " + result.reason);
    }

    return review;
}

TorchRuntimeGuiReview TorchRuntimeResultAdapter::Adapt(const TorchRuntimeGuiResult& result)
{
    CxTorchTaskSpec dummy_task;
    CxInferenceResult inference_result;
    std::string reason;

    AdaptToInferenceResult(result, dummy_task, inference_result, reason);
    return AdaptToGuiReview(inference_result);
}
