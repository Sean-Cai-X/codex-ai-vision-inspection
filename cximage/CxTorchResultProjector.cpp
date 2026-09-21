#include "CxTorchResultProjector.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

namespace
{
bool LoadFirstContourPoints(
    const std::string& contour_ref,
    std::vector<double>& out_points,
    double& out_center_x,
    double& out_center_y)
{
    out_points.clear();
    out_center_x = 0.0;
    out_center_y = 0.0;

    if (contour_ref.empty())
        return false;

    std::ifstream input(contour_ref);
    if (!input)
        return false;

    const std::string json(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());

    std::size_t points_key = json.find(R"("points")");
    if (points_key == std::string::npos)
        points_key = json.find(R"("outer_contours")");
    if (points_key == std::string::npos)
        return false;

    const std::size_t array_begin = json.find('[', points_key);
    if (array_begin == std::string::npos)
        return false;

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

    if (array_end == std::string::npos || array_end <= array_begin)
        return false;

    const std::string array_text = json.substr(array_begin, array_end - array_begin + 1);
    const char* cursor = array_text.c_str();
    char* next = nullptr;
    std::vector<double> values;

    while (*cursor != '\0')
    {
        if (std::isdigit(static_cast<unsigned char>(*cursor)) ||
            *cursor == '-' ||
            *cursor == '+' ||
            *cursor == '.')
        {
            const double value = std::strtod(cursor, &next);
            if (next != cursor)
            {
                values.push_back(value);
                cursor = next;
                continue;
            }
        }
        ++cursor;
    }

    if (values.size() < 4 || (values.size() % 2) != 0)
        return false;

    out_points = values;

    double min_x = values[0];
    double max_x = values[0];
    double min_y = values[1];
    double max_y = values[1];
    for (std::size_t i = 0; i + 1 < values.size(); i += 2)
    {
        const double x = values[i];
        const double y = values[i + 1];
        if (x < min_x) min_x = x;
        if (x > max_x) max_x = x;
        if (y < min_y) min_y = y;
        if (y > max_y) max_y = y;
    }

    out_center_x = (min_x + max_x) * 0.5;
    out_center_y = (min_y + max_y) * 0.5;
    return true;
}
}

bool CxTorchResultProjector::Project(
    const CxInferenceResult& inference_result,
    const std::string& owner_type,
    const std::string& owner_ref,
    std::vector<CxShapeElementSnapshot>& shapes)
{
    shapes.clear();

    if (!inference_result.executed)
        return false;

    if (!inference_result.ok)
        return false;

    ProjectDetections(inference_result.detections, owner_type, owner_ref, shapes);

    if (inference_result.mask.has_value())
        ProjectMask(inference_result.mask.value(), owner_type, owner_ref, shapes);

    return !shapes.empty();
}

bool CxTorchResultProjector::TryBuildFastMatchTransform(
    const CxInferenceResult& inference_result,
    FastMatchTransform& transform,
    int& detection_index,
    int& class_id,
    double& confidence,
    std::string& angle_convention,
    std::string& reason)
{
    transform = FastMatchTransform();
    detection_index = -1;
    class_id = -1;
    confidence = 0.0;
    angle_convention.clear();
    reason.clear();

    if (!inference_result.executed || !inference_result.ok)
    {
        reason = "torch inference result is not successful";
        return false;
    }

    const CxTorchDetection* best = nullptr;
    for (const CxTorchDetection& candidate : inference_result.detections)
    {
        if (!candidate.oriented_box_available ||
            !(candidate.oriented_width > 0.0) ||
            !(candidate.oriented_height > 0.0) ||
            !std::isfinite(candidate.oriented_center_x) ||
            !std::isfinite(candidate.oriented_center_y) ||
            !std::isfinite(candidate.oriented_angle_deg))
        {
            continue;
        }
        if (best == nullptr || candidate.confidence > best->confidence)
            best = &candidate;
    }

    if (best == nullptr)
    {
        reason = "no explicit oriented Torch detection is available";
        return false;
    }

    transform = FastMatchTransform::fromOrientedBox(
        best->oriented_center_x, best->oriented_center_y,
        best->oriented_width, best->oriented_height,
        best->oriented_angle_deg);
    if (!transform.valid())
    {
        reason = "Torch oriented box produced an invalid FastMatch transform";
        return false;
    }

    detection_index = best->source_index;
    class_id = best->class_id;
    confidence = best->confidence;
    angle_convention = best->oriented_angle_convention;
    return true;
}

void CxTorchResultProjector::ProjectDetections(
    const std::vector<CxTorchDetection>& detections,
    const std::string& owner_type,
    const std::string& owner_ref,
    std::vector<CxShapeElementSnapshot>& shapes)
{
    for (std::size_t index = 0; index < detections.size(); ++index)
    {
        const auto& detection = detections[index];

        CxShapeElementSnapshot box;

        box.shape_kind = "rect";
        box.stable_ref = owner_ref + ".detection_" + std::to_string(index);
        box.semantic_role = (index == 0) ? "model_best_result" : "model_candidate";
        box.owner_type = owner_type;
        box.owner_ref = owner_ref;
        box.result_element = true;
        box.editable = false;

        box.center_x = detection.x + detection.width * 0.5;
        box.center_y = detection.y + detection.height * 0.5;

        box.points.push_back(detection.x);
        box.points.push_back(detection.y);
        box.points.push_back(detection.x + detection.width);
        box.points.push_back(detection.y);
        box.points.push_back(detection.x + detection.width);
        box.points.push_back(detection.y + detection.height);
        box.points.push_back(detection.x);
        box.points.push_back(detection.y + detection.height);
        box.closed = true;

        shapes.push_back(std::move(box));
    }
}

void CxTorchResultProjector::ProjectMask(
    const CxTorchMask& mask,
    const std::string& owner_type,
    const std::string& owner_ref,
    std::vector<CxShapeElementSnapshot>& shapes)
{
    if (!mask.available)
        return;

    CxShapeElementSnapshot mask_element;
    mask_element.stable_ref = owner_ref + ".segmentation_mask";
    mask_element.shape_kind = "mask";
    mask_element.semantic_role = "model_segmentation_mask";
    mask_element.owner_type = owner_type;
    mask_element.owner_ref = owner_ref;
    mask_element.result_element = true;
    mask_element.editable = false;

    shapes.push_back(std::move(mask_element));

    if (!mask.contour_ref.empty())
    {
        CxShapeElementSnapshot contour_element;
        contour_element.stable_ref = owner_ref + ".segmentation_contour";
        contour_element.shape_kind = "polyline";
        contour_element.semantic_role = "model_segmentation_contour";
        contour_element.owner_type = owner_type;
        contour_element.owner_ref = owner_ref;
        contour_element.result_element = true;
        contour_element.editable = false;
        contour_element.closed = true;
        LoadFirstContourPoints(
            mask.contour_ref,
            contour_element.points,
            contour_element.center_x,
            contour_element.center_y);

        shapes.push_back(std::move(contour_element));
    }
}
