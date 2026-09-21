#include "CxImageReferenceCandidateGenerator.h"

#include "FindSegmentationOpenCvSmokeBackend.h"

#include <exception>
#include <filesystem>
#include <fstream>
#include <opencv2/imgcodecs.hpp>

namespace
{
std::string EscapeCandidateJson(const std::string& value)
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

bool WriteCandidateSummary(
    const CxImageReferenceCandidateRequest& request,
    const CxImageReferenceCandidateResult& result,
    std::string& reason)
{
    std::ofstream file(result.summary_ref, std::ios::trunc);
    if (!file)
    {
        reason = "cannot open cximage candidate summary: " + result.summary_ref;
        return false;
    }

    file << "{\n"
         << "  \"schema\": \"cxvision.cximage_reference_candidate.v1\",\n"
         << "  \"algorithm_id\": \"" << EscapeCandidateJson(request.algorithm_id) << "\",\n"
         << "  \"status\": \"" << EscapeCandidateJson(result.status) << "\",\n"
         << "  \"reason\": \"" << EscapeCandidateJson(result.reason) << "\",\n"
         << "  \"provenance\": \"" << EscapeCandidateJson(result.provenance) << "\",\n"
         << "  \"reference_semantics\": \"diagnostic_candidate_not_ground_truth\",\n"
         << "  \"input_image_ref\": \"" << EscapeCandidateJson(request.input_image_path.string()) << "\",\n"
         << "  \"threshold\": " << request.threshold << ",\n"
         << "  \"roi\": {\"enabled\": " << (request.has_roi ? "true" : "false")
         << ", \"xyxy\": [" << request.roi_x0 << ", " << request.roi_y0 << ", "
         << request.roi_x1 << ", " << request.roi_y1 << "]},\n"
         << "  \"mask_ref\": \"" << EscapeCandidateJson(result.mask_ref) << "\",\n"
         << "  \"overlay_ref\": \"" << EscapeCandidateJson(result.overlay_ref) << "\",\n"
         << "  \"instances_ref\": \"" << EscapeCandidateJson(result.instances_ref) << "\",\n"
         << "  \"mask_facts\": {\n"
         << "    \"width\": " << result.mask_facts.width << ",\n"
         << "    \"height\": " << result.mask_facts.height << ",\n"
         << "    \"foreground_ratio\": " << result.mask_facts.foreground_ratio << ",\n"
         << "    \"component_count\": " << result.mask_facts.component_count << ",\n"
         << "    \"bbox_xywh\": [" << result.mask_facts.bbox_x << ", "
         << result.mask_facts.bbox_y << ", " << result.mask_facts.bbox_width << ", "
         << result.mask_facts.bbox_height << "],\n"
         << "    \"touches_border\": " << (result.mask_facts.touches_border ? "true" : "false") << "\n"
         << "  }\n"
         << "}\n";
    if (!file.good())
    {
        reason = "failed to write cximage candidate summary: " + result.summary_ref;
        return false;
    }
    reason.clear();
    return true;
}

bool WriteCandidateInstances(
    const CxImageReferenceCandidateResult& result,
    std::string& reason)
{
    std::ofstream file(result.instances_ref, std::ios::trunc);
    if (!file)
    {
        reason = "cannot open cximage candidate instances: " + result.instances_ref;
        return false;
    }

    file << "{\n"
         << "  \"schema\": \"cxvision.reference_candidate_instances.v1\",\n"
         << "  \"reference_semantics\": \"diagnostic_candidate_not_ground_truth\",\n"
         << "  \"instance_count\": " << (result.mask_facts.empty ? 0 : 1) << ",\n"
         << "  \"mask_ref\": \"" << EscapeCandidateJson(result.mask_ref) << "\",\n"
         << "  \"instances\": [";
    if (!result.mask_facts.empty)
    {
        file << "{\"instance_id\": 1, \"bbox_xywh\": ["
             << result.mask_facts.bbox_x << ", "
             << result.mask_facts.bbox_y << ", "
             << result.mask_facts.bbox_width << ", "
             << result.mask_facts.bbox_height << "]}";
    }
    file << "]\n}\n";
    if (!file.good())
    {
        reason = "failed to write cximage candidate instances: " + result.instances_ref;
        return false;
    }
    reason.clear();
    return true;
}
} // namespace

bool CxImageReferenceCandidateGenerator::Generate(
    const CxImageReferenceCandidateRequest& request,
    CxImageReferenceCandidateResult& result,
    std::string& reason) const
{
    result = {};
    result.executed = true;
    result.provenance = request.algorithm_id;

    if (request.algorithm_id != "find_segmentation_opencv_smoke")
    {
        result.status = "UNSUPPORTED_REFERENCE_ALGORITHM";
        result.reason = "unsupported cximage reference algorithm: " + request.algorithm_id;
        reason = result.reason;
        return false;
    }

    cv::Mat image = cv::imread(request.input_image_path.string(), cv::IMREAD_COLOR);
    if (image.empty())
    {
        result.status = "REFERENCE_INPUT_INVALID";
        result.reason = "cximage reference input is missing or unreadable";
        reason = result.reason;
        return false;
    }

    FindSegmentationInput input;
    input.image = image;
    input.backend = "opencv_smoke";
    input.threshold = request.threshold;
    if (request.has_roi)
    {
        input.has_rect = true;
        input.rect = cv::Rect(
            request.roi_x0,
            request.roi_y0,
            request.roi_x1 - request.roi_x0,
            request.roi_y1 - request.roi_y0);
    }
    if (request.has_positive_point)
    {
        input.has_point = true;
        input.point = cv::Point(request.positive_x, request.positive_y);
        input.has_positive_point = true;
        input.positive_point = input.point;
    }
    if (request.has_negative_point)
    {
        input.has_negative_point = true;
        input.negative_point = cv::Point(request.negative_x, request.negative_y);
    }

    FindSegmentationResult backend_result;
    FindSegmentationOpenCvSmokeBackend backend;
    std::string backend_reason;
    if (!backend.Run(input, backend_result, backend_reason) || backend_result.mask.empty())
    {
        result.status = "REFERENCE_CANDIDATE_NOT_AVAILABLE";
        result.reason = backend_reason.empty()
            ? "cximage reference backend did not produce a mask"
            : backend_reason;
        reason = result.reason;
        return false;
    }

    try
    {
        std::filesystem::create_directories(request.output_dir);
        result.mask_ref = (request.output_dir / "cximage_reference_mask.png").string();
        result.overlay_ref = (request.output_dir / "mask_overlay.png").string();
        result.instances_ref = (request.output_dir / "instances.json").string();
        result.summary_ref = (request.output_dir / "cximage_reference_candidate.json").string();
        if (!cv::imwrite(result.mask_ref, backend_result.mask) ||
            backend_result.overlay.empty() ||
            !cv::imwrite(result.overlay_ref, backend_result.overlay))
        {
            result.status = "REFERENCE_ASSET_WRITE_FAILED";
            result.reason = "failed to write cximage reference candidate assets";
            reason = result.reason;
            return false;
        }
    }
    catch (const std::exception& error)
    {
        result.status = "REFERENCE_ASSET_WRITE_FAILED";
        result.reason = error.what();
        reason = result.reason;
        return false;
    }

    std::string facts_reason;
    if (!AnalyzeCxMaskFile(result.mask_ref, result.mask_facts, facts_reason))
    {
        result.status = "REFERENCE_MASK_EVALUATION_FAILED";
        result.reason = facts_reason;
        reason = result.reason;
        return false;
    }

    result.ok = true;
    result.status = "REFERENCE_CANDIDATE_COMPLETE";
    result.reason = "cximage diagnostic candidate generated; not ground truth";
    if (!WriteCandidateInstances(result, reason) ||
        !WriteCandidateSummary(request, result, reason))
    {
        result.ok = false;
        result.status = "REFERENCE_ASSET_WRITE_FAILED";
        result.reason = reason;
        return false;
    }

    reason.clear();
    return true;
}
