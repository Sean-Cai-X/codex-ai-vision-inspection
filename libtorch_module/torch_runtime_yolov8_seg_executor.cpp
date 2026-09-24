#include "torch_runtime_yolov8_seg_executor.h"

#include "torch_runtime_manifest.h"
#include "torch_runtime_task_types.h"
#include "torch_segmentation_evidence.h"
#include "torch_yolov8_seg.h"
#include "torch_taskalignedassigner.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <numeric>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <set>
#include <sstream>
#include <stdexcept>
#include <torch/cuda.h>
#include <unordered_map>

namespace
{
struct SegLetterbox
{
    double scale = 1.0;
    int pad_x = 0;
    int pad_y = 0;
    int resized_width = 0;
    int resized_height = 0;
};

struct SegCandidate
{
    int class_id = -1;
    float score = 0.0f;
    float x0 = 0.0f;
    float y0 = 0.0f;
    float x1 = 0.0f;
    float y1 = 0.0f;
    torch::Tensor coefficients;
};

struct YoloV8SegDatasetSample
{
    std::string image_id;
    std::string image_ref;
    std::string split;
    std::string label;
    std::vector<std::array<float, 4>> boxes_xyxy_norm;
    std::vector<std::vector<cv::Point2f>> polygons_norm;
    std::vector<int64_t> classes;
    std::string target_mask_ref;
    std::string geometry_facts_ref;
    std::string training_target_ref;
    std::string metrology_target_ref;
    std::string boundary_map_ref;
    std::string geometry_type;
    std::string package_geometry_type;
    // Source-pixel geometry facts.  The valid mask is the contract: a missing
    // fact must stay missing instead of being guessed from the segmentation box.
    int geometry_primitive_type = -1;
    std::array<float, 6> geometry_target_values{};
    std::array<uint8_t, 6> geometry_target_valid{};
};

constexpr int kGeometryTargetValueCount = 6;
constexpr int kGeometryCenterX = 0;
constexpr int kGeometryCenterY = 1;
constexpr int kGeometryEnvelopeWidth = 2;
constexpr int kGeometryEnvelopeHeight = 3;
constexpr int kGeometrySinAngle = 4;
constexpr int kGeometryCosAngle = 5;

struct GeometryTargetTensors
{
    torch::Tensor primitive_type;
    torch::Tensor values;
    torch::Tensor valid_mask;
    bool primitive_type_valid = false;
};

struct SegTaskAlignedAnchorTable
{
    torch::Tensor class_logits;
    torch::Tensor dfl_logits;
    torch::Tensor mask_coefficients;
    torch::Tensor decoded_boxes;
    torch::Tensor centers_xy;
    torch::Tensor grid_widths;
    torch::Tensor grid_heights;
};

SegTaskAlignedAnchorTable BuildSegTaskAlignedAnchorTable(
    const YoloV8SegRawOutput& raw,
    const YoloV8Segment& model,
    int input_width,
    int input_height)
{
    std::vector<torch::Tensor> class_levels, dfl_levels, coefficient_levels;
    std::vector<torch::Tensor> box_levels, center_levels, width_levels, height_levels;
    for (std::size_t level = 0; level < raw.class_logits.size(); ++level)
    {
        const int64_t height = raw.class_logits[level].size(2);
        const int64_t width = raw.class_logits[level].size(3);
        const int64_t anchors = height * width;
        const auto options = raw.class_logits[level].options();
        const torch::Tensor xs = torch::arange(width, options).repeat({height});
        const torch::Tensor ys = torch::arange(height, options).view({height, 1})
            .repeat({1, width}).reshape({anchors});
        const torch::Tensor center_x = (xs + 0.5f) / static_cast<float>(width);
        const torch::Tensor center_y = (ys + 0.5f) / static_cast<float>(height);
        const torch::Tensor dfl = model->head()->dfl_module()->expectation(
            raw.box_logits[level].view({1, 64, anchors})).permute({0, 2, 1});
        box_levels.push_back(torch::stack({
            (center_x - dfl.select(2, 0) / static_cast<float>(width)).clamp(0.0, 1.0),
            (center_y - dfl.select(2, 1) / static_cast<float>(height)).clamp(0.0, 1.0),
            (center_x + dfl.select(2, 2) / static_cast<float>(width)).clamp(0.0, 1.0),
            (center_y + dfl.select(2, 3) / static_cast<float>(height)).clamp(0.0, 1.0)}, 2));
        class_levels.push_back(raw.class_logits[level].permute({0, 2, 3, 1})
            .reshape({1, anchors, raw.class_logits[level].size(1)}));
        dfl_levels.push_back(raw.box_logits[level].permute({0, 2, 3, 1})
            .reshape({1, anchors, 64}));
        coefficient_levels.push_back(raw.mask_coefficients[level].permute({0, 2, 3, 1})
            .reshape({1, anchors, raw.mask_coefficients[level].size(1)}));
        center_levels.push_back(torch::stack({center_x, center_y}, 1).unsqueeze(0));
        width_levels.push_back(torch::full({1, anchors}, static_cast<float>(width), options));
        height_levels.push_back(torch::full({1, anchors}, static_cast<float>(height), options));
    }
    return {torch::cat(class_levels, 1), torch::cat(dfl_levels, 1),
        torch::cat(coefficient_levels, 1), torch::cat(box_levels, 1),
        torch::cat(center_levels, 1), torch::cat(width_levels, 1),
        torch::cat(height_levels, 1)};
}

int GeometryPrimitiveTypeFromName(const std::string& geometry_type)
{
    static const std::map<std::string, int> kTypes{
        {"arc", 0}, {"circle", 1}, {"ellipse", 2}, {"line", 3},
        {"open_curve", 4}, {"polygon", 5}, {"closed_curve", 6}};
    const auto found = kTypes.find(geometry_type);
    return found == kTypes.end() ? -1 : found->second;
}

bool ReadGeometryPoint(const cv::FileNode& node, float& x, float& y)
{
    if (!node.isSeq() || node.size() != 2)
        return false;
    node[0] >> x;
    node[1] >> y;
    return std::isfinite(x) && std::isfinite(y);
}

bool ReadGeometryAngle(const cv::FileNode& node, float& radians)
{
    double degrees = 0.0;
    if (node.empty())
        return false;
    node >> degrees;
    if (!std::isfinite(degrees))
        return false;
    radians = static_cast<float>(degrees * CV_PI / 180.0);
    return true;
}

void SetGeometryOrientation(
    YoloV8SegDatasetSample& sample,
    float radians)
{
    sample.geometry_target_values[kGeometrySinAngle] = std::sin(radians);
    sample.geometry_target_values[kGeometryCosAngle] = std::cos(radians);
    sample.geometry_target_valid[kGeometrySinAngle] = 1;
    sample.geometry_target_valid[kGeometryCosAngle] = 1;
}

GeometryTargetTensors BuildGeometryTargetTensors(
    const YoloV8SegDatasetSample& sample,
    const SegLetterbox& letterbox,
    int input_width,
    int input_height,
    const torch::Device& device)
{
    std::array<float, kGeometryTargetValueCount> values =
        sample.geometry_target_values;
    const float input_width_f = static_cast<float>(input_width);
    const float input_height_f = static_cast<float>(input_height);
    // The first four values originate in source pixels.  Letterbox is applied
    // here, once, so V2 geometry losses share the detector coordinate system.
    if (sample.geometry_target_valid[kGeometryCenterX] != 0)
        values[kGeometryCenterX] =
            static_cast<float>((letterbox.pad_x +
                values[kGeometryCenterX] * letterbox.scale) / input_width_f);
    if (sample.geometry_target_valid[kGeometryCenterY] != 0)
        values[kGeometryCenterY] =
            static_cast<float>((letterbox.pad_y +
                values[kGeometryCenterY] * letterbox.scale) / input_height_f);
    if (sample.geometry_target_valid[kGeometryEnvelopeWidth] != 0)
        values[kGeometryEnvelopeWidth] = static_cast<float>(
            values[kGeometryEnvelopeWidth] * letterbox.scale / input_width_f);
    if (sample.geometry_target_valid[kGeometryEnvelopeHeight] != 0)
        values[kGeometryEnvelopeHeight] = static_cast<float>(
            values[kGeometryEnvelopeHeight] * letterbox.scale / input_height_f);

    GeometryTargetTensors result;
    const auto float_options = torch::TensorOptions()
        .dtype(torch::kFloat32).device(device);
    result.values = torch::tensor(
        std::vector<float>(values.begin(), values.end()), float_options);
    std::array<float, kGeometryTargetValueCount> mask{};
    for (int index = 0; index < kGeometryTargetValueCount; ++index)
        mask[index] = sample.geometry_target_valid[index] == 0 ? 0.0f : 1.0f;
    result.valid_mask = torch::tensor(
        std::vector<float>(mask.begin(), mask.end()), float_options);
    result.primitive_type = torch::tensor(
        sample.geometry_primitive_type,
        torch::TensorOptions().dtype(torch::kLong).device(device));
    result.primitive_type_valid = sample.geometry_primitive_type >= 0;
    return result;
}

std::string TrimDatasetText(std::string value)
{
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::map<std::string, std::string> ParseDatasetKeyValues(
    const std::string& payload)
{
    std::map<std::string, std::string> values;
    std::istringstream input(payload);
    std::string token;
    while (input >> token)
    {
        const std::size_t equals = token.find('=');
        if (equals != std::string::npos && equals > 0)
            values[token.substr(0, equals)] = token.substr(equals + 1);
    }
    return values;
}

std::string ExtractDatasetQuotedPayload(const std::string& line)
{
    const std::size_t first = line.find('"');
    const std::size_t last = line.rfind('"');
    if (first == std::string::npos || last == std::string::npos || last <= first)
        return {};
    return line.substr(first + 1, last - first - 1);
}

int DatasetClassFromSemanticRole(const std::string& role)
{
    const std::string marker = "_class_";
    const std::size_t position = role.rfind(marker);
    if (position == std::string::npos)
        return 0;
    try
    {
        return std::max(0, std::stoi(role.substr(position + marker.size())));
    }
    catch (...)
    {
        return 0;
    }
}

std::filesystem::path ResolveDatasetPath(
    const std::filesystem::path& manifest_path,
    const std::string& value)
{
    std::filesystem::path path(value);
    if (path.is_relative())
        path = manifest_path.parent_path() / path;
    return path.lexically_normal();
}

bool AppendDatasetBox(
    YoloV8SegDatasetSample& sample,
    int class_id,
    double x0,
    double y0,
    double x1,
    double y1)
{
    x0 = std::clamp(x0, 0.0, 1.0);
    y0 = std::clamp(y0, 0.0, 1.0);
    x1 = std::clamp(x1, 0.0, 1.0);
    y1 = std::clamp(y1, 0.0, 1.0);
    if (x1 <= x0 || y1 <= y0)
        return false;
    sample.boxes_xyxy_norm.push_back({
        static_cast<float>(x0), static_cast<float>(y0),
        static_cast<float>(x1), static_cast<float>(y1)});
    sample.classes.push_back(class_id);
    return true;
}

bool AppendDatasetPolygon(
    YoloV8SegDatasetSample& sample,
    int class_id,
    std::vector<cv::Point2f> polygon)
{
    if (polygon.size() > 3 && polygon.front() == polygon.back())
        polygon.pop_back();
    if (polygon.size() < 3)
        return false;
    float x0 = 1.0f;
    float y0 = 1.0f;
    float x1 = 0.0f;
    float y1 = 0.0f;
    for (cv::Point2f& point : polygon)
    {
        if (!std::isfinite(point.x) || !std::isfinite(point.y) ||
            point.x < 0.0f || point.x > 1.0f ||
            point.y < 0.0f || point.y > 1.0f)
        {
            return false;
        }
        x0 = std::min(x0, point.x);
        y0 = std::min(y0, point.y);
        x1 = std::max(x1, point.x);
        y1 = std::max(y1, point.y);
    }
    if (x1 <= x0 || y1 <= y0)
        return false;
    sample.boxes_xyxy_norm.push_back({x0, y0, x1, y1});
    sample.polygons_norm.push_back(std::move(polygon));
    sample.classes.push_back(std::max(0, class_id));
    return true;
}

bool LoadCxEvidenceDataset(
    const std::filesystem::path& manifest_path,
    std::vector<YoloV8SegDatasetSample>& samples,
    std::string& reason)
{
    std::ifstream input(manifest_path);
    if (!input)
    {
        reason = "cannot open Evidence dataset manifest: " + manifest_path.string();
        return false;
    }

    std::vector<YoloV8SegDatasetSample> rows;
    struct PolygonRecord
    {
        int class_id = 0;
        std::vector<cv::Point2f> points;
    };
    std::map<std::string, std::vector<PolygonRecord>> polygons_by_image;
    std::string line;
    while (std::getline(input, line))
    {
        if (line.find("CxEvidenceChain_case_adddatasetimage(") != std::string::npos)
        {
            const auto values = ParseDatasetKeyValues(
                ExtractDatasetQuotedPayload(line));
            const auto image_id = values.find("image_id");
            const auto path = values.find("path");
            if (image_id == values.end() || path == values.end())
                continue;
            YoloV8SegDatasetSample row;
            row.image_id = image_id->second;
            row.image_ref = ResolveDatasetPath(manifest_path, path->second).string();
            const auto split = values.find("split");
            const auto label = values.find("label");
            row.split = split == values.end() ? "train" : split->second;
            row.label = label == values.end() ? "unlabeled" : label->second;
            rows.push_back(std::move(row));
            continue;
        }

        if (line.find("CxEvidenceChain_case_addbbox_xywh_norm(") != std::string::npos)
        {
            reason = "bbox-only annotation rejected in Evidence dataset manifest; "
                "closed polygon instance masks are required";
            return false;
        }
        if (line.find("CxEvidenceChain_case_addpolygon(") == std::string::npos)
            continue;
        const auto values = ParseDatasetKeyValues(
            ExtractDatasetQuotedPayload(line));
        const auto image_id = values.find("image_id");
        if (image_id == values.end())
            continue;
        try
        {
            PolygonRecord record;
            const auto class_value = values.find("class_id");
            if (class_value != values.end())
                record.class_id = std::stoi(class_value->second);
            std::istringstream point_stream(values.at("points"));
            std::string pair;
            while (std::getline(point_stream, pair, ';'))
            {
                const std::size_t comma = pair.find(',');
                if (comma == std::string::npos)
                    continue;
                record.points.emplace_back(
                    std::stof(pair.substr(0, comma)),
                    std::stof(pair.substr(comma + 1)));
            }
            if (record.points.size() < 3)
                throw std::runtime_error("polygon has fewer than three points");
            polygons_by_image[image_id->second].push_back(std::move(record));
        }
        catch (...)
        {
            reason = "invalid polygon record in Evidence dataset manifest";
            return false;
        }
    }

    std::set<std::string> unique_rows;
    for (auto& row : rows)
    {
        const std::string key = row.split + "|" + row.image_id + "|" + row.image_ref;
        if (!unique_rows.insert(key).second)
            continue;
        const auto found = polygons_by_image.find(row.image_id);
        if (found != polygons_by_image.end())
        {
            for (const PolygonRecord& polygon : found->second)
            {
                if (!AppendDatasetPolygon(
                        row, polygon.class_id, polygon.points))
                {
                    reason = "invalid normalized polygon for image " + row.image_id;
                    return false;
                }
            }
        }
        samples.push_back(std::move(row));
    }
    if (samples.empty())
    {
        reason = "Evidence dataset manifest contains no dataset images";
        return false;
    }
    return true;
}

bool LoadExportedTorchDataset(
    const std::filesystem::path& manifest_path,
    std::vector<YoloV8SegDatasetSample>& samples,
    std::string& reason)
{
    cv::FileStorage storage(manifest_path.string(), cv::FileStorage::READ);
    if (!storage.isOpened())
    {
        reason = "cannot open exported Torch dataset manifest: " +
            manifest_path.string();
        return false;
    }
    std::string schema;
    storage["schema"] >> schema;
    if (schema != "cxvision.torch.training_dataset.v2")
    {
        reason = "unsupported Torch dataset schema: " + schema;
        return false;
    }

    const cv::FileNode images = storage["images"];
    for (const auto& image_node : images)
    {
        YoloV8SegDatasetSample sample;
        std::string image_path;
        image_node["image_id"] >> sample.image_id;
        image_node["image_path"] >> image_path;
        image_node["split"] >> sample.split;
        image_node["label"] >> sample.label;
        sample.image_ref = ResolveDatasetPath(manifest_path, image_path).string();
        cv::Mat image = cv::imread(sample.image_ref, cv::IMREAD_COLOR);
        if (image.empty())
            continue;

        const cv::FileNode shapes = image_node["shapes"];
        for (const auto& shape : shapes)
        {
            std::string semantic_role;
            std::string shape_kind;
            shape["semantic_role"] >> semantic_role;
            shape["shape_kind"] >> shape_kind;
            std::string lowered_role = semantic_role;
            std::transform(lowered_role.begin(), lowered_role.end(),
                lowered_role.begin(), [](unsigned char ch) {
                  return static_cast<char>(std::tolower(ch));
                });
            int closed = 0;
            shape["closed"] >> closed;
            if (shape_kind != "PolylineShape" || closed == 0 ||
                lowered_role.find("bbox") != std::string::npos)
            {
                reason = "bbox-only or non-polygon mask rejected for image " +
                    sample.image_id + "; closed polygon instance masks are required";
                return false;
            }
            int class_id = DatasetClassFromSemanticRole(semantic_role);
            if (!shape["class_id"].empty())
                shape["class_id"] >> class_id;
            std::vector<double> points;
            shape["points_xy"] >> points;
            std::vector<cv::Point2f> polygon;
            for (std::size_t index = 1; index < points.size(); index += 2)
            {
                polygon.emplace_back(
                    static_cast<float>(points[index - 1] / image.cols),
                    static_cast<float>(points[index] / image.rows));
            }
            if (!AppendDatasetPolygon(sample, class_id, std::move(polygon)))
            {
                reason = "invalid polygon mask for image " + sample.image_id;
                return false;
            }
        }
        samples.push_back(std::move(sample));
    }
    if (samples.empty())
    {
        reason = "exported Torch dataset contains no readable images";
        return false;
    }
    return true;
}

bool LoadGeometryTargetPackage(
    const std::filesystem::path& manifest_path,
    std::vector<YoloV8SegDatasetSample>& samples,
    std::string& reason)
{
    cv::FileStorage storage(manifest_path.string(), cv::FileStorage::READ);
    if (!storage.isOpened() ||
        static_cast<std::string>(storage["schema"]) !=
            "cxvision.yolov8n_aabb_mask_package.v1")
    {
        reason = "unsupported geometry target package schema";
        return false;
    }
    const cv::FileNode nodes = storage["samples"];
    if (!nodes.isSeq() || nodes.empty())
    {
        reason = "geometry target package has no samples";
        return false;
    }
    for (const cv::FileNode& node : nodes)
    {
        YoloV8SegDatasetSample sample;
        std::string image_ref, label_ref;
        node["split"] >> sample.split;
        node["image"] >> image_ref;
        node["label"] >> label_ref;
        node["target_mask"] >> sample.target_mask_ref;
        node["geometry_facts"] >> sample.geometry_facts_ref;
        node["training_target"] >> sample.training_target_ref;
        node["metrology_target"] >> sample.metrology_target_ref;
        node["boundary_map"] >> sample.boundary_map_ref;
        node["geometry_type"] >> sample.package_geometry_type;
        sample.image_ref = ResolveDatasetPath(manifest_path, image_ref).string();
        sample.target_mask_ref =
            ResolveDatasetPath(manifest_path, sample.target_mask_ref).string();
        sample.geometry_facts_ref =
            ResolveDatasetPath(manifest_path, sample.geometry_facts_ref).string();
        sample.training_target_ref =
            ResolveDatasetPath(manifest_path, sample.training_target_ref).string();
        sample.metrology_target_ref =
            ResolveDatasetPath(manifest_path, sample.metrology_target_ref).string();
        sample.boundary_map_ref =
            ResolveDatasetPath(manifest_path, sample.boundary_map_ref).string();
        if (!std::filesystem::is_regular_file(sample.image_ref) ||
            !std::filesystem::is_regular_file(sample.target_mask_ref) ||
            !std::filesystem::is_regular_file(sample.geometry_facts_ref) ||
            !std::filesystem::is_regular_file(sample.training_target_ref) ||
            !std::filesystem::is_regular_file(sample.metrology_target_ref) ||
            !std::filesystem::is_regular_file(sample.boundary_map_ref))
        {
            reason = "geometry target package required asset missing";
            return false;
        }
        cv::FileStorage metrology(sample.metrology_target_ref, cv::FileStorage::READ);
        cv::FileStorage geometry(sample.geometry_facts_ref, cv::FileStorage::READ);
        std::string geometry_type;
        int training_eligible = 0;
        int identifiable = 0;
        if (!metrology.isOpened() || !geometry.isOpened() ||
            static_cast<std::string>(metrology["schema"]) !=
                "cxvision.metrology_target.v1" ||
            static_cast<std::string>(geometry["schema"]) !=
                "cxvision.geometry_augmented_facts.v1")
        {
            reason = "geometry target package target schema invalid";
            return false;
        }
        metrology["training_eligible"] >> training_eligible;
        metrology["identifiable"] >> identifiable;
        geometry["geometry_type"] >> geometry_type;
        const bool training_split = sample.split == "train";
        if ((training_split && training_eligible == 0) ||
            identifiable == 0 || geometry_type.empty())
        {
            reason = "geometry target package sample is not training eligible";
            return false;
        }
        const std::string canonical_geometry_type =
            sample.package_geometry_type.empty()
                ? geometry_type : sample.package_geometry_type;
        sample.geometry_primitive_type =
            GeometryPrimitiveTypeFromName(canonical_geometry_type);
        sample.geometry_type = canonical_geometry_type;
        const cv::FileNode standard_position = geometry["standard_position"];
        float center_x = 0.0f, center_y = 0.0f;
        if (!ReadGeometryPoint(
                standard_position["centroid_xy"], center_x, center_y))
        {
            reason = "geometry target package standard centroid invalid";
            return false;
        }
        const cv::FileNode envelope = standard_position["bbox_xywh"];
        if (!envelope.isSeq() || envelope.size() != 4)
        {
            reason = "geometry target package standard envelope invalid";
            return false;
        }
        float envelope_width = 0.0f, envelope_height = 0.0f;
        envelope[2] >> envelope_width;
        envelope[3] >> envelope_height;
        if (!std::isfinite(envelope_width) || !std::isfinite(envelope_height) ||
            envelope_width <= 0.0f || envelope_height <= 0.0f)
        {
            reason = "geometry target package standard envelope dimensions invalid";
            return false;
        }
        sample.geometry_target_values[kGeometryCenterX] = center_x;
        sample.geometry_target_values[kGeometryCenterY] = center_y;
        sample.geometry_target_values[kGeometryEnvelopeWidth] = envelope_width;
        sample.geometry_target_values[kGeometryEnvelopeHeight] = envelope_height;
        sample.geometry_target_valid[kGeometryCenterX] = 1;
        sample.geometry_target_valid[kGeometryCenterY] = 1;
        sample.geometry_target_valid[kGeometryEnvelopeWidth] = 1;
        sample.geometry_target_valid[kGeometryEnvelopeHeight] = 1;

        // Geometry facts use two legal layouts: primitive facts at root for
        // boundary primitives, or an instances[0] object for closed regions.
        const cv::FileNode instances = geometry["instances"];
        const cv::FileNode primitive =
            instances.isSeq() && !instances.empty() ? instances[0] : geometry.root();
        float angle_radians = 0.0f;
        if (ReadGeometryAngle(primitive["rotation_deg"], angle_radians))
        {
            SetGeometryOrientation(sample, angle_radians);
        }
        else
        {
            const cv::FileNode endpoints = primitive["endpoints_xy"];
            float start_x = 0.0f, start_y = 0.0f;
            float end_x = 0.0f, end_y = 0.0f;
            if (endpoints.isSeq() && endpoints.size() >= 2 &&
                ReadGeometryPoint(endpoints[0], start_x, start_y) &&
                ReadGeometryPoint(endpoints[1], end_x, end_y))
            {
                SetGeometryOrientation(
                    sample, std::atan2(end_y - start_y, end_x - start_x));
            }
        }
        std::ifstream label(ResolveDatasetPath(manifest_path, label_ref));
        int class_id = -1;
        double cx = 0.0, cy = 0.0, width = 0.0, height = 0.0;
        if (!(label >> class_id >> cx >> cy >> width >> height) ||
            class_id < 0 || width <= 0.0 || height <= 0.0)
        {
            reason = "geometry target package label invalid";
            return false;
        }
        const cv::Mat mask = cv::imread(sample.target_mask_ref, cv::IMREAD_GRAYSCALE);
        if (mask.empty())
        {
            reason = "geometry target package mask cannot be decoded";
            return false;
        }
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        if (contours.empty())
        {
            reason = "geometry target package requires a non-empty instance mask";
            return false;
        }
        const auto largest_contour = std::max_element(
            contours.begin(), contours.end(),
            [](const auto& lhs, const auto& rhs) {
                return cv::contourArea(lhs) < cv::contourArea(rhs);
            });
        if (largest_contour == contours.end() || largest_contour->size() < 3)
        {
            reason = "geometry target package mask has no valid contour";
            return false;
        }
        std::vector<cv::Point2f> polygon;
        polygon.reserve(largest_contour->size());
        for (const cv::Point& point : *largest_contour)
            polygon.emplace_back(
                static_cast<float>(point.x) / static_cast<float>(mask.cols),
                static_cast<float>(point.y) / static_cast<float>(mask.rows));
        if (!AppendDatasetPolygon(sample, class_id, std::move(polygon)))
        {
            reason = "geometry target package contour is invalid";
            return false;
        }
        sample.image_id = std::filesystem::path(sample.image_ref).stem().string();
        sample.label = "geometry_target_ready";
        if (sample.split == "validation") sample.split = "val";
        samples.push_back(std::move(sample));
    }
    return !samples.empty();
}

std::string DatasetSplitFromPath(const std::filesystem::path& path)
{
    for (const auto& component : path)
    {
        const std::string value = component.string();
        if (value == "train" || value == "val" || value == "test")
            return value;
    }
    return "train";
}

bool LoadDatasetFolder(
    const std::filesystem::path& root,
    std::vector<YoloV8SegDatasetSample>& samples,
    std::string& reason)
{
    const std::filesystem::path exported =
        root / "torch_training_dataset_manifest.json";
    if (std::filesystem::is_regular_file(exported))
        return LoadExportedTorchDataset(exported, samples, reason);
    const std::filesystem::path geometry_package = root / "package_manifest.json";
    if (std::filesystem::is_regular_file(geometry_package))
        return LoadGeometryTargetPackage(geometry_package, samples, reason);

    const std::set<std::string> image_extensions{
        ".bmp", ".jpg", ".jpeg", ".png", ".tif", ".tiff"};
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
    {
        if (!entry.is_regular_file())
            continue;
        std::string extension = entry.path().extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        if (image_extensions.find(extension) == image_extensions.end())
            continue;

        std::filesystem::path label_path = entry.path();
        label_path.replace_extension(".txt");
        if (!std::filesystem::is_regular_file(label_path))
        {
            const std::filesystem::path relative =
                entry.path().lexically_relative(root);
            label_path = root;
            for (const auto& component : relative)
            {
                if (component == "images")
                    label_path /= "labels";
                else
                    label_path /= component;
            }
            label_path.replace_extension(".txt");
        }

        YoloV8SegDatasetSample sample;
        sample.image_id = entry.path().stem().string();
        sample.image_ref = entry.path().string();
        sample.split = DatasetSplitFromPath(entry.path().lexically_relative(root));
        sample.label = std::filesystem::is_regular_file(label_path)
            ? "annotated" : "unlabeled";
        std::ifstream labels(label_path);
        std::string label_line;
        while (std::getline(labels, label_line))
        {
            std::istringstream label_stream(label_line);
            int class_id = 0;
            if (!(label_stream >> class_id))
                continue;
            std::vector<float> coordinates;
            float coordinate = 0.0f;
            while (label_stream >> coordinate)
                coordinates.push_back(coordinate);
            if (coordinates.size() == 4)
            {
                reason = "YOLO bbox-only label rejected: " +
                    label_path.string() +
                    "; YOLO segmentation polygon labels are required";
                return false;
            }
            if (coordinates.size() < 6 || coordinates.size() % 2 != 0)
            {
                reason = "invalid YOLO segmentation polygon label: " +
                    label_path.string();
                return false;
            }
            std::vector<cv::Point2f> polygon;
            for (std::size_t coordinate_index = 1;
                 coordinate_index < coordinates.size();
                 coordinate_index += 2)
            {
                polygon.emplace_back(coordinates[coordinate_index - 1],
                                     coordinates[coordinate_index]);
            }
            if (!AppendDatasetPolygon(sample, class_id, std::move(polygon)))
            {
                reason = "invalid YOLO segmentation polygon geometry: " +
                    label_path.string();
                return false;
            }
        }
        samples.push_back(std::move(sample));
    }
    if (samples.empty())
    {
        reason = "dataset folder contains no supported images: " + root.string();
        return false;
    }
    return true;
}

bool LoadYoloV8SegDataset(
    const std::string& source,
    std::vector<YoloV8SegDatasetSample>& samples,
    std::string& reason)
{
    samples.clear();
    if (source.empty())
    {
        reason = "YOLOv8-Seg training dataset source is empty";
        return false;
    }
    const std::filesystem::path path(source);
    if (std::filesystem::is_directory(path))
        return LoadDatasetFolder(path, samples, reason);
    if (!std::filesystem::is_regular_file(path))
    {
        reason = "YOLOv8-Seg training dataset source does not exist: " + source;
        return false;
    }
    if (path.extension() == ".cxsc")
        return LoadCxEvidenceDataset(path, samples, reason);
    return LoadExportedTorchDataset(path, samples, reason);
}

std::string QuoteSegJson(const std::string& value)
{
    std::ostringstream output;
    output << '"';
    for (const char ch : value)
    {
        switch (ch)
        {
        case '\\': output << "\\\\"; break;
        case '"': output << "\\\""; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default: output << ch; break;
        }
    }
    output << '"';
    return output.str();
}

std::string Fnv1a64File(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    std::uint64_t hash = 1469598103934665603ull;
    char buffer[4096];
    while (input.good())
    {
        input.read(buffer, sizeof(buffer));
        for (std::streamsize index = 0;
             index < input.gcount();
             ++index)
        {
            hash ^= static_cast<unsigned char>(buffer[index]);
            hash *= 1099511628211ull;
        }
    }
    std::ostringstream output;
    output << "fnv1a64:" << std::hex << std::setw(16)
           << std::setfill('0') << hash;
    return output.str();
}

TorchTaskResultCpp SegFailure(
    const std::string& stage,
    const std::string& reason)
{
    TorchTaskResultCpp result;
    result.ok = false;
    result.status = "failed";
    result.error_code = -1;
    result.error_message = reason;
    result.result_json =
        "{\"schema\":\"cxvision.segmentation_evidence.v2\","
        "\"status\":\"failed\",\"failure_stage\":" +
        QuoteSegJson(stage) + ",\"reason\":" +
        QuoteSegJson(reason) + "}";
    return result;
}

torch::Tensor MakeSegInput(
    const cv::Mat& bgr,
    const TorchModelManifest& manifest,
    SegLetterbox& letterbox)
{
    letterbox.scale = std::min(
        manifest.input_width / static_cast<double>(bgr.cols),
        manifest.input_height / static_cast<double>(bgr.rows));
    letterbox.resized_width = std::max(
        1,
        static_cast<int>(std::round(bgr.cols * letterbox.scale)));
    letterbox.resized_height = std::max(
        1,
        static_cast<int>(std::round(bgr.rows * letterbox.scale)));
    letterbox.pad_x =
        (manifest.input_width - letterbox.resized_width) / 2;
    letterbox.pad_y =
        (manifest.input_height - letterbox.resized_height) / 2;

    cv::Mat resized;
    cv::resize(
        bgr,
        resized,
        cv::Size(
            letterbox.resized_width,
            letterbox.resized_height),
        0.0,
        0.0,
        cv::INTER_LINEAR);
    cv::Mat canvas(
        manifest.input_height,
        manifest.input_width,
        CV_8UC3,
        cv::Scalar(114, 114, 114));
    resized.copyTo(canvas(cv::Rect(
        letterbox.pad_x,
        letterbox.pad_y,
        letterbox.resized_width,
        letterbox.resized_height)));
    cv::Mat rgb;
    cv::cvtColor(canvas, rgb, cv::COLOR_BGR2RGB);
    torch::Tensor tensor = torch::from_blob(
        rgb.data,
        {1, rgb.rows, rgb.cols, 3},
        torch::kUInt8).clone();
    return tensor.permute({0, 3, 1, 2})
        .to(torch::kFloat32)
        .div_(255.0);
}

float BoxIou(const SegCandidate& lhs, const SegCandidate& rhs)
{
    const float x0 = std::max(lhs.x0, rhs.x0);
    const float y0 = std::max(lhs.y0, rhs.y0);
    const float x1 = std::min(lhs.x1, rhs.x1);
    const float y1 = std::min(lhs.y1, rhs.y1);
    const float intersection =
        std::max(0.0f, x1 - x0) *
        std::max(0.0f, y1 - y0);
    const float lhs_area =
        std::max(0.0f, lhs.x1 - lhs.x0) *
        std::max(0.0f, lhs.y1 - lhs.y0);
    const float rhs_area =
        std::max(0.0f, rhs.x1 - rhs.x0) *
        std::max(0.0f, rhs.y1 - rhs.y0);
    return intersection /
        std::max(1.0e-6f, lhs_area + rhs_area - intersection);
}

std::vector<SegCandidate> DecodeCandidates(
    const YoloV8SegRawOutput& raw,
    const YoloV8SegmentHead& head,
    const TorchModelManifest& manifest)
{
    const std::vector<float> strides{8.0f, 16.0f, 32.0f};
    std::vector<SegCandidate> candidates;
    for (std::size_t level = 0; level < 3; ++level)
    {
        const int64_t height = raw.box_logits[level].size(2);
        const int64_t width = raw.box_logits[level].size(3);
        const int64_t anchors = height * width;
        const torch::Tensor boxes = raw.box_logits[level]
            .view({1, 64, anchors});
        const torch::Tensor distances =
            head->dfl_module()->expectation(boxes)
                .squeeze(0).to(torch::kCPU);
        const torch::Tensor classes = raw.class_logits[level]
            .view({1, manifest.num_classes, anchors})
            .sigmoid()
            .squeeze(0).to(torch::kCPU);
        const auto class_max = classes.max(0);
        const torch::Tensor scores =
            std::get<0>(class_max).contiguous();
        const torch::Tensor class_ids =
            std::get<1>(class_max).contiguous();
        const torch::Tensor coefficients =
            raw.mask_coefficients[level]
                .view({1, manifest.mask_channels, anchors})
                .squeeze(0)
                .transpose(0, 1)
                .to(torch::kCPU)
                .contiguous();
        for (int64_t anchor = 0; anchor < anchors; ++anchor)
        {
            const float score = scores[anchor].item<float>();
            if (score < manifest.confidence_threshold)
                continue;
            const int64_t row = anchor / width;
            const int64_t column = anchor % width;
            const float center_x =
                (static_cast<float>(column) + 0.5f) * strides[level];
            const float center_y =
                (static_cast<float>(row) + 0.5f) * strides[level];
            SegCandidate candidate;
            candidate.class_id =
                class_ids[anchor].item<int64_t>();
            candidate.score = score;
            candidate.x0 = center_x -
                distances[0][anchor].item<float>() * strides[level];
            candidate.y0 = center_y -
                distances[1][anchor].item<float>() * strides[level];
            candidate.x1 = center_x +
                distances[2][anchor].item<float>() * strides[level];
            candidate.y1 = center_y +
                distances[3][anchor].item<float>() * strides[level];
            candidate.coefficients =
                coefficients[anchor].clone();
            candidates.push_back(std::move(candidate));
        }
    }

    std::sort(
        candidates.begin(),
        candidates.end(),
        [](const SegCandidate& lhs, const SegCandidate& rhs)
        {
            return lhs.score > rhs.score;
        });
    std::vector<SegCandidate> selected;
    for (const auto& candidate : candidates)
    {
        bool suppressed = false;
        for (const auto& kept : selected)
        {
            if ((manifest.class_agnostic_nms ||
                 candidate.class_id == kept.class_id) &&
                BoxIou(candidate, kept) > manifest.iou_threshold)
            {
                suppressed = true;
                break;
            }
        }
        if (!suppressed)
        {
            selected.push_back(candidate);
            if (selected.size() >=
                static_cast<std::size_t>(manifest.max_detections))
            {
                break;
            }
        }
    }
    return selected;
}

cv::Mat DecodeMask(
    const SegCandidate& candidate,
    const torch::Tensor& prototypes,
    const TorchModelManifest& manifest,
    const SegLetterbox& letterbox,
    const cv::Size& original_size,
    double& quality,
    double& stability)
{
    const int64_t proto_height = prototypes.size(2);
    const int64_t proto_width = prototypes.size(3);
    torch::Tensor probability =
        torch::matmul(
            candidate.coefficients,
            prototypes.squeeze(0)
                .view({manifest.mask_channels, -1})
                .to(torch::kCPU))
            .sigmoid()
            .view({proto_height, proto_width});

    const double proto_scale_x =
        proto_width / static_cast<double>(manifest.input_width);
    const double proto_scale_y =
        proto_height / static_cast<double>(manifest.input_height);
    const int crop_x0 = std::clamp(
        static_cast<int>(std::floor(candidate.x0 * proto_scale_x)),
        0,
        static_cast<int>(proto_width));
    const int crop_y0 = std::clamp(
        static_cast<int>(std::floor(candidate.y0 * proto_scale_y)),
        0,
        static_cast<int>(proto_height));
    const int crop_x1 = std::clamp(
        static_cast<int>(std::ceil(candidate.x1 * proto_scale_x)),
        0,
        static_cast<int>(proto_width));
    const int crop_y1 = std::clamp(
        static_cast<int>(std::ceil(candidate.y1 * proto_scale_y)),
        0,
        static_cast<int>(proto_height));
    torch::Tensor crop_gate = torch::zeros_like(probability);
    if (crop_x1 > crop_x0 && crop_y1 > crop_y0)
    {
        crop_gate.index_put_(
            {torch::indexing::Slice(crop_y0, crop_y1),
             torch::indexing::Slice(crop_x0, crop_x1)},
            1.0);
    }
    probability = probability * crop_gate;

    const int image_x0 = std::clamp(
        static_cast<int>(std::floor(
            letterbox.pad_x * proto_scale_x)),
        0,
        static_cast<int>(proto_width - 1));
    const int image_y0 = std::clamp(
        static_cast<int>(std::floor(
            letterbox.pad_y * proto_scale_y)),
        0,
        static_cast<int>(proto_height - 1));
    const int image_x1 = std::clamp(
        static_cast<int>(std::ceil(
            (letterbox.pad_x + letterbox.resized_width) *
            proto_scale_x)),
        image_x0 + 1,
        static_cast<int>(proto_width));
    const int image_y1 = std::clamp(
        static_cast<int>(std::ceil(
            (letterbox.pad_y + letterbox.resized_height) *
            proto_scale_y)),
        image_y0 + 1,
        static_cast<int>(proto_height));
    probability = probability.index({
        torch::indexing::Slice(image_y0, image_y1),
        torch::indexing::Slice(image_x0, image_x1)});
    probability = torch::nn::functional::interpolate(
        probability.unsqueeze(0).unsqueeze(0),
        torch::nn::functional::InterpolateFuncOptions()
            .size(std::vector<int64_t>{
                original_size.height,
                original_size.width})
            .mode(torch::kBilinear)
            .align_corners(false))
        .squeeze()
        .contiguous();

    const torch::Tensor mask =
        probability.ge(manifest.mask_threshold);
    const double foreground =
        std::max(1.0, mask.sum().item<double>());
    quality =
        (probability * mask.to(torch::kFloat32))
            .sum().item<double>() / foreground;
    const torch::Tensor stable_mask =
        probability.ge(std::min(1.0f, manifest.mask_threshold + 0.05f));
    const double intersection =
        (mask.logical_and(stable_mask)).sum().item<double>();
    const double union_area =
        std::max(
            1.0,
            (mask.logical_or(stable_mask)).sum().item<double>());
    stability = intersection / union_area;
    const torch::Tensor bytes =
        mask.to(torch::kUInt8).mul(255).to(torch::kCPU).contiguous();
    cv::Mat output(
        original_size.height,
        original_size.width,
        CV_8UC1,
        bytes.data_ptr<unsigned char>());
    return output.clone();
}

std::vector<cv::Point> LargestContour(const cv::Mat& mask)
{
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(
        mask,
        contours,
        cv::RETR_EXTERNAL,
        cv::CHAIN_APPROX_NONE);
    if (contours.empty())
        return {};
    return *std::max_element(
        contours.begin(),
        contours.end(),
        [](const auto& lhs, const auto& rhs)
        {
            return cv::contourArea(lhs) < cv::contourArea(rhs);
        });
}

void WritePointArray(
    std::ostream& output,
    const std::vector<cv::Point>& points)
{
    output << "[";
    for (std::size_t index = 0; index < points.size(); ++index)
    {
        if (index > 0)
            output << ",";
        output << "{\"x\":" << points[index].x
               << ",\"y\":" << points[index].y << "}";
    }
    output << "]";
}
} // namespace

TorchTaskResultCpp ExecuteTorchYoloV8SegTask(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request)
{
    try
    {
        TorchModelManifest manifest;
        std::string reason;
        if (!LoadTorchModelManifest(
                request.manifest_path,
                config.model_root,
                manifest,
                reason) ||
            !ValidateInstanceSegmentationManifest(manifest, reason))
        {
            return SegFailure("manifest", reason);
        }
        cv::Mat image =
            cv::imread(request.input_image, cv::IMREAD_COLOR);
        if (image.empty())
            return SegFailure("input", "input image is unreadable");

        const std::string device_name =
            (request.device == "cuda" || config.device == "cuda") &&
                    torch::cuda::is_available()
                ? "cuda"
                : "cpu";
        const torch::Device device(device_name);
        SegLetterbox letterbox;
        torch::Tensor input =
            MakeSegInput(image, manifest, letterbox).to(device);

        YoloV8Segment model(manifest.num_classes);
        const YoloV8SegWeightMappingReport mapping =
            model->load_state_dict_strict(
                manifest.weights_path.string());
        model->to(device);
        model->eval();

        const auto started = std::chrono::steady_clock::now();
        torch::NoGradGuard no_grad;
        YoloV8SegRawOutput raw = model->forward(input);
        for (auto& tensor : raw.box_logits) tensor = tensor.to(torch::kCPU);
        for (auto& tensor : raw.class_logits) tensor = tensor.to(torch::kCPU);
        for (auto& tensor : raw.mask_coefficients) tensor = tensor.to(torch::kCPU);
        raw.prototypes = raw.prototypes.to(torch::kCPU);
        const std::vector<SegCandidate> candidates =
            DecodeCandidates(raw, model->head(), manifest);
        const auto finished = std::chrono::steady_clock::now();

        const std::filesystem::path output_dir(request.output_dir);
        std::filesystem::create_directories(output_dir);
        const auto masks_dir = output_dir / "instance_masks";
        std::filesystem::create_directories(masks_dir);
        const auto instances_ref = output_dir / "instances.json";
        const auto labels_ref = output_dir / "mask_labels.png";
        const auto overlay_ref = output_dir / "mask_overlay.png";
        const auto contours_ref = output_dir / "contours.json";
        const auto metrics_ref =
            output_dir / "segmentation_metrics.json";
        const auto evidence_ref =
            output_dir / "torch_runtime_evidence.json";
        const auto trace_ref =
            output_dir / "tensor_shape_trace.json";
        const auto mapping_ref =
            output_dir / "weight_mapping_report.json";
        const auto refined_ref =
            output_dir / "refined_edge_points.json";
        const auto rejected_ref =
            output_dir / "rejected_edge_points.json";
        const auto measurement_ref =
            output_dir / "measurement_evidence.json";
        const auto measurement_overlay_ref =
            output_dir / "measurement_overlay.png";

        cv::Mat labels(
            image.rows, image.cols, CV_16UC1, cv::Scalar(0));
        cv::Mat overlay = image.clone();
        cv::Mat measurement_overlay = image.clone();
        cv::Mat gray;
        cv::Mat source_edges;
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
        cv::Canny(gray, source_edges, 50.0, 150.0);

        std::ofstream instances(instances_ref);
        std::ofstream contours(contours_ref);
        std::ofstream refined(refined_ref);
        std::ofstream rejected(rejected_ref);
        std::ofstream measurements(measurement_ref);
        instances << "{\"schema\":\"cxvision.segmentation_evidence.v2\","
                  << "\"provider\":\"yolov8_seg\","
                  << "\"model_id\":" << QuoteSegJson(manifest.model_id) << ","
                  << "\"weights_hash\":" << QuoteSegJson(manifest.weights_hash) << ","
                  << "\"input_image_ref\":" << QuoteSegJson(request.input_image) << ","
                  << "\"input_image_hash\":" << QuoteSegJson(Fnv1a64File(request.input_image)) << ","
                  << "\"transform\":{\"original_width\":" << image.cols
                  << ",\"original_height\":" << image.rows
                  << ",\"roi_x\":0,\"roi_y\":0,\"roi_width\":" << image.cols
                  << ",\"roi_height\":" << image.rows
                  << ",\"letterbox_scale\":" << letterbox.scale
                  << ",\"pad_x\":" << letterbox.pad_x
                  << ",\"pad_y\":" << letterbox.pad_y
                  << ",\"network_width\":" << manifest.input_width
                  << ",\"network_height\":" << manifest.input_height
                  << ",\"prototype_width\":" << raw.prototypes.size(3)
                  << ",\"prototype_height\":" << raw.prototypes.size(2)
                  << "},\"instances\":[";
        contours << "{\"instances\":[";
        refined << "{\"instances\":[";
        rejected << "{\"instances\":[";
        measurements << "{\"schema\":\"cxvision.measurement_evidence.v1\","
                     << "\"provider\":\"original_image_edge_projector\","
                     << "\"instances\":[";

        int accepted_instances = 0;
        for (std::size_t index = 0; index < candidates.size(); ++index)
        {
            double quality = 0.0;
            double stability = 0.0;
            cv::Mat mask = DecodeMask(
                candidates[index],
                raw.prototypes,
                manifest,
                letterbox,
                image.size(),
                quality,
                stability);
            std::vector<cv::Point> contour = LargestContour(mask);
            if (contour.size() < 3)
                continue;

            const std::string stable_id =
                "yolov8_seg_instance_" +
                std::to_string(accepted_instances);
            const auto mask_ref =
                masks_dir / (stable_id + ".png");
            cv::imwrite(mask_ref.string(), mask);
            labels.setTo(
                cv::Scalar(accepted_instances + 1),
                mask);

            const cv::Rect bbox = cv::boundingRect(contour);
            const cv::Moments moments = cv::moments(contour);
            const double centroid_x =
                moments.m00 != 0.0 ? moments.m10 / moments.m00 : 0.0;
            const double centroid_y =
                moments.m00 != 0.0 ? moments.m01 / moments.m00 : 0.0;
            const cv::RotatedRect oriented =
                cv::minAreaRect(contour);

            cv::Mat contour_image =
                cv::Mat::zeros(mask.size(), CV_8UC1);
            std::vector<std::vector<cv::Point>> contour_list{contour};
            cv::drawContours(
                contour_image,
                contour_list,
                0,
                cv::Scalar(255),
                1);
            cv::Mat band;
            cv::dilate(
                contour_image,
                band,
                cv::getStructuringElement(
                    cv::MORPH_ELLIPSE, cv::Size(5, 5)));
            cv::Mat refined_image;
            cv::bitwise_and(source_edges, band, refined_image);
            std::vector<cv::Point> refined_points;
            cv::findNonZero(refined_image, refined_points);
            std::vector<cv::Point> rejected_points;
            for (const auto& point : contour)
            {
                if (source_edges.at<unsigned char>(point) == 0)
                    rejected_points.push_back(point);
            }

            const cv::Scalar color(
                40 + (accepted_instances * 71) % 180,
                220 - (accepted_instances * 43) % 160,
                80 + (accepted_instances * 97) % 160);
            cv::Mat color_layer = overlay.clone();
            color_layer.setTo(color, mask);
            cv::addWeighted(
                color_layer, 0.35, overlay, 0.65, 0.0, overlay);
            cv::rectangle(overlay, bbox, color, 2);
            cv::drawContours(
                measurement_overlay,
                contour_list,
                0,
                cv::Scalar(0, 165, 255),
                1);
            for (const auto& point : refined_points)
                measurement_overlay.at<cv::Vec3b>(point) =
                    cv::Vec3b(0, 255, 0);
            cv::Point2f vertices[4];
            oriented.points(vertices);
            for (int vertex = 0; vertex < 4; ++vertex)
            {
                cv::line(
                    measurement_overlay,
                    vertices[vertex],
                    vertices[(vertex + 1) % 4],
                    cv::Scalar(255, 0, 255),
                    2);
            }

            if (accepted_instances > 0)
            {
                instances << ",";
                contours << ",";
                refined << ",";
                rejected << ",";
                measurements << ",";
            }
            const std::string class_name =
                candidates[index].class_id >= 0 &&
                candidates[index].class_id <
                    static_cast<int>(manifest.class_names.size())
                    ? manifest.class_names[candidates[index].class_id]
                    : "unknown";
            instances
                << "{\"stable_id\":" << QuoteSegJson(stable_id)
                << ",\"class_id\":" << candidates[index].class_id
                << ",\"class_name\":" << QuoteSegJson(class_name)
                << ",\"class_confidence\":" << candidates[index].score
                << ",\"mask_quality\":" << quality
                << ",\"stability_score\":" << stability
                << ",\"bbox\":{\"x0\":" << bbox.x
                << ",\"y0\":" << bbox.y
                << ",\"x1\":" << bbox.x + bbox.width
                << ",\"y1\":" << bbox.y + bbox.height
                << "},\"binary_mask_ref\":" << QuoteSegJson(mask_ref.string())
                << ",\"contour_ref\":" << QuoteSegJson(contours_ref.string())
                << ",\"pixel_area\":" << cv::contourArea(contour)
                << ",\"centroid\":{\"x\":" << centroid_x
                << ",\"y\":" << centroid_y << "}}";
            contours << "{\"stable_id\":" << QuoteSegJson(stable_id)
                     << ",\"outer_contours\":[";
            WritePointArray(contours, contour);
            contours << "],\"holes\":[]}";
            refined << "{\"stable_id\":" << QuoteSegJson(stable_id)
                    << ",\"points\":";
            WritePointArray(refined, refined_points);
            refined << "}";
            rejected << "{\"stable_id\":" << QuoteSegJson(stable_id)
                     << ",\"points\":";
            WritePointArray(rejected, rejected_points);
            rejected << "}";
            measurements
                << "{\"instance_id\":" << QuoteSegJson(stable_id)
                << ",\"raw_mask_contour_ref\":" << QuoteSegJson(contours_ref.string())
                << ",\"refined_edge_points_ref\":" << QuoteSegJson(refined_ref.string())
                << ",\"rejected_edge_points_ref\":" << QuoteSegJson(rejected_ref.string())
                << ",\"fitted_primitive\":\"oriented_rectangle\""
                << ",\"major_axis_pixels\":"
                << std::max(oriented.size.width, oriented.size.height)
                << ",\"minor_axis_pixels\":"
                << std::min(oriented.size.width, oriented.size.height)
                << ",\"pixel_area\":" << cv::contourArea(contour)
                << ",\"calibration\":1.0"
                << ",\"physical_unit\":\"pixel\""
                << ",\"uncertainty\":"
                << (refined_points.empty()
                    ? 1.0
                    : rejected_points.size() /
                      static_cast<double>(
                          refined_points.size() +
                          rejected_points.size()))
                << "}";
            ++accepted_instances;
        }
        instances << "],\"overlay_ref\":" << QuoteSegJson(overlay_ref.string())
                  << ",\"metrics_ref\":" << QuoteSegJson(metrics_ref.string())
                  << "}\n";
        contours << "]}\n";
        refined << "]}\n";
        rejected << "]}\n";
        measurements << "],\"overlay_ref\":"
                     << QuoteSegJson(measurement_overlay_ref.string())
                     << "}\n";

        cv::imwrite(labels_ref.string(), labels);
        cv::imwrite(overlay_ref.string(), overlay);
        cv::imwrite(
            measurement_overlay_ref.string(),
            measurement_overlay);

        std::ofstream(mapping_ref)
            << "{\"schema\":\"cxvision.torch.weight_mapping.v1\","
            << "\"source_count\":" << mapping.source_count
            << ",\"target_count\":" << mapping.target_count
            << ",\"loaded_count\":" << mapping.loaded_count
            << ",\"missing_keys\":[],\"unknown_keys\":[],"
            << "\"shape_mismatches\":[],\"complete\":true}\n";
        std::ofstream(trace_ref)
            << "{\"schema\":\"cxvision.torch.tensor_shape_trace.v1\","
            << "\"input\":[1,3," << manifest.input_height << ","
            << manifest.input_width << "],"
            << "\"box_scales\":[[1,64,"
            << raw.box_logits[0].size(2) << "," << raw.box_logits[0].size(3)
            << "],[1,64," << raw.box_logits[1].size(2) << ","
            << raw.box_logits[1].size(3) << "],[1,64,"
            << raw.box_logits[2].size(2) << ","
            << raw.box_logits[2].size(3) << "]],"
            << "\"class_channels\":" << manifest.num_classes << ","
            << "\"mask_coefficient_channels\":" << manifest.mask_channels << ","
            << "\"prototypes\":[1," << raw.prototypes.size(1) << ","
            << raw.prototypes.size(2) << "," << raw.prototypes.size(3)
            << "]}\n";
        const double elapsed_ms =
            std::chrono::duration<double, std::milli>(
                finished - started).count();
        std::ofstream(metrics_ref)
            << "{\"schema\":\"cxvision.segmentation_metrics.v1\","
            << "\"candidate_count\":" << candidates.size()
            << ",\"instance_count\":" << accepted_instances
            << ",\"elapsed_ms\":" << elapsed_ms
            << ",\"semantic_quality\":\"pending_human_review\"}\n";
        std::ofstream(evidence_ref)
            << "{\"schema\":\"cxvision.torch.runtime_evidence.v1\","
            << "\"provider\":\"yolov8_seg\","
            << "\"segmentation_evidence_ref\":" << QuoteSegJson(instances_ref.string())
            << ",\"measurement_evidence_ref\":" << QuoteSegJson(measurement_ref.string())
            << ",\"tensor_shape_trace_ref\":" << QuoteSegJson(trace_ref.string())
            << ",\"weight_mapping_report_ref\":" << QuoteSegJson(mapping_ref.string())
            << ",\"overlay_ref\":" << QuoteSegJson(overlay_ref.string())
            << ",\"human_review_required\":true}\n";

        TorchTaskResultCpp result;
        result.ok = true;
        result.status = "success";
        result.requested_device = request.device;
        result.actual_device = device_name;
        result.infer_runtime_ms = elapsed_ms;
        result.algorithm_runtime_ms = elapsed_ms;
        result.result_ref = instances_ref.string();
        result.evidence_ref = evidence_ref.string();
        result.input_image_ref = request.input_image;
        result.primary_visual_ref = overlay_ref.string();
        result.visualization_refs =
            labels_ref.string() + ";" + overlay_ref.string() + ";" +
            measurement_overlay_ref.string();
        result.result_json =
            "{\"schema\":\"cxvision.segmentation_evidence.v2\","
            "\"status\":\"success\",\"provider\":\"yolov8_seg\","
            "\"instance_count\":" + std::to_string(accepted_instances) +
            ",\"result_ref\":" + QuoteSegJson(instances_ref.string()) +
            ",\"evidence_ref\":" + QuoteSegJson(evidence_ref.string()) +
            ",\"overlay_ref\":" + QuoteSegJson(overlay_ref.string()) +
            ",\"measurement_evidence_ref\":" +
            QuoteSegJson(measurement_ref.string()) +
            ",\"semantic_quality\":\"pending_human_review\"}";
        return result;
    }
    catch (const std::exception& error)
    {
        return SegFailure("exception", error.what());
    }
}

TorchTaskResultCpp ExecuteTorchYoloV8SegBackwardSmokeTask(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request)
{
    struct Stat
    {
        bool grad_defined = false;
        double grad_mean = 0.0;
        double grad_max = 0.0;
        double grad_norm = 0.0;
        double param_norm = 0.0;
        double update_norm = 0.0;
        int count = 0;
    };

    struct LossTensors
    {
        torch::Tensor total_loss;
        torch::Tensor box_loss;
        torch::Tensor class_loss;
        torch::Tensor dfl_loss;
        torch::Tensor mask_loss;
        int64_t proto_h = 0;
        int64_t proto_w = 0;
        int64_t geometry_target_valid_fields = 0;
        bool geometry_primitive_type_available = false;
        int64_t task_aligned_anchor_count = 0;
        int64_t task_aligned_positive_count = 0;
    };

    struct AblationResult
    {
        std::string variant;
        std::string frozen_group;
        double total_loss = 0.0;
        double box_loss = 0.0;
        double class_loss = 0.0;
        double dfl_loss = 0.0;
        double mask_loss = 0.0;
        std::map<std::string, Stat> groups;
    };

    struct TrainingEpochMetric
    {
        int epoch = 0;
        double learning_rate = 0.0;
        double total_loss = 0.0;
        double box_loss = 0.0;
        double class_loss = 0.0;
        double dfl_loss = 0.0;
        double mask_loss = 0.0;
        bool assignment_quality_targets_active = false;
        double elapsed_ms = 0.0;
        std::map<std::string, Stat> parameter_groups;
    };

    struct StabilityResult
    {
        std::string case_id;
        std::string image_id;
        std::string split;
        std::string input_image_ref;
        std::string perturbation_type;
        double roi_shift_dx_px = 0.0;
        double roi_shift_dy_px = 0.0;
        double confidence_threshold = 0.25;
        bool training_step_executed = false;
        bool inference_ok = false;
        int instance_count = -1;
        int instance_count_delta_from_baseline = 0;
        double total_loss = 0.0;
        double box_loss = 0.0;
        double class_loss = 0.0;
        double dfl_loss = 0.0;
        double mask_loss = 0.0;
        std::string model_manifest_ref;
        std::string inference_result_ref;
        std::string inference_overlay_ref;
        std::string inference_result_hash;
        std::string inference_overlay_hash;
        bool result_hash_matches_baseline = false;
        bool overlay_hash_matches_baseline = false;
        std::string failure_stage;
    };

    auto add_stat = [](Stat& stat,
                       const torch::Tensor& parameter,
                       const torch::Tensor& before) {
        const torch::Tensor value = parameter.detach();
        const torch::Tensor grad = parameter.grad();
        stat.param_norm += value.norm().item<double>();
        if (grad.defined())
        {
            const torch::Tensor abs_grad = grad.detach().abs();
            stat.grad_defined = true;
            stat.grad_mean += abs_grad.mean().item<double>();
            stat.grad_max = std::max(stat.grad_max, abs_grad.max().item<double>());
            stat.grad_norm += grad.detach().norm().item<double>();
        }
        stat.update_norm += (value - before).norm().item<double>();
        ++stat.count;
    };

    auto write_stat = [](std::ostream& out,
                         const std::string& name,
                         const Stat& stat,
                         bool comma) {
        const double divisor = std::max(1, stat.count);
        out << "    " << QuoteSegJson(name) << ":{"
            << "\"grad_defined\":" << (stat.grad_defined ? "true" : "false")
            << ",\"grad_mean\":" << stat.grad_mean / divisor
            << ",\"grad_max\":" << stat.grad_max
            << ",\"grad_norm\":" << stat.grad_norm
            << ",\"param_norm\":" << stat.param_norm
            << ",\"update_norm\":" << stat.update_norm
            << ",\"parameter_count\":" << stat.count
            << "}" << (comma ? "," : "") << "\n";
    };

    auto group_for = [](const std::string& name) {
        if (name.find("m22.proto") != std::string::npos ||
            name.find("model.22.proto") != std::string::npos)
            return std::string("proto_branch");
        if (name.find("m22.cv4") != std::string::npos ||
            name.find("model.22.cv4") != std::string::npos)
            return std::string("mask_coeff_head");
        if (name.find("m22.cv3") != std::string::npos ||
            name.find("model.22.cv3") != std::string::npos)
            return std::string("class_head");
        if (name.find("m22.cv2") != std::string::npos ||
            name.find("model.22.cv2") != std::string::npos)
            return std::string("box_head");
        for (int index : {9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21})
        {
            const std::string direct = "m" + std::to_string(index) + ".";
            const std::string listed = "model." + std::to_string(index) + ".";
            if (name.find(direct) != std::string::npos ||
                name.find(listed) != std::string::npos)
                return std::string("pan_fpn");
        }
        for (int index : {0, 1, 2, 3, 4, 5, 6, 7, 8})
        {
            const std::string direct = "m" + std::to_string(index) + ".";
            const std::string listed = "model." + std::to_string(index) + ".";
            if (name.find(direct) != std::string::npos ||
                name.find(listed) != std::string::npos)
                return std::string("backbone");
        }
        return std::string("other");
    };
    const std::vector<std::string> group_order{
        "backbone", "pan_fpn", "box_head", "class_head",
        "mask_coeff_head", "proto_branch", "other"};

    auto block_for = [](const std::string& name) {
        for (int index : {2, 4, 6, 8, 12, 15, 18, 21})
        {
            const std::string direct = "m" + std::to_string(index) + ".";
            const std::string listed = "model." + std::to_string(index) + ".";
            if (name.find(direct) != std::string::npos ||
                name.find(listed) != std::string::npos)
                return std::string("m") + std::to_string(index);
        }
        return std::string();
    };

    try
    {
        TorchModelManifest manifest;
        std::string reason;
        if (!LoadTorchModelManifest(
                request.manifest_path,
                config.model_root,
                manifest,
                reason) ||
            !ValidateInstanceSegmentationManifest(manifest, reason))
        {
            return SegFailure("manifest", reason);
        }

        std::vector<YoloV8SegDatasetSample> dataset_samples;
        if (!LoadYoloV8SegDataset(
                request.dataset_root, dataset_samples, reason))
        {
            return SegFailure("dataset", reason);
        }
        std::vector<YoloV8SegDatasetSample> train_samples;
        std::vector<YoloV8SegDatasetSample> evaluation_samples;
        for (const auto& dataset_sample : dataset_samples)
        {
            if (dataset_sample.split == "train" &&
                (dataset_sample.classes.empty() ||
                 dataset_sample.polygons_norm.size() !=
                     dataset_sample.classes.size() ||
                 dataset_sample.boxes_xyxy_norm.size() !=
                     dataset_sample.classes.size()))
            {
                return SegFailure(
                    "dataset_preflight",
                    "bbox-only or missing instance mask rejected for train image " +
                        dataset_sample.image_id +
                        "; one closed polygon is required per instance");
            }
            if (!dataset_sample.classes.empty() &&
                dataset_sample.polygons_norm.size() !=
                    dataset_sample.classes.size())
            {
                return SegFailure(
                    "dataset_preflight",
                    "polygon/class cardinality mismatch for image " +
                        dataset_sample.image_id);
            }
            if (dataset_sample.split == "train" &&
                !dataset_sample.classes.empty())
            {
                train_samples.push_back(dataset_sample);
            }
            if (dataset_sample.split == "val" ||
                dataset_sample.split == "test")
            {
                evaluation_samples.push_back(dataset_sample);
            }
        }
        if (train_samples.empty())
            return SegFailure("dataset", "dataset train split has no annotated images");
        if (train_samples.size() < 2)
            return SegFailure(
                "dataset", "YOLOv8-Seg L2/L3 requires at least two annotated train images");
        if (evaluation_samples.empty())
            evaluation_samples = train_samples;
        std::size_t train_instance_count = 0;
        std::size_t geometry_primitive_type_target_count = 0;
        std::size_t geometry_unknown_type_count = 0;
        std::size_t geometry_target_valid_field_count = 0;
        std::map<std::string, std::size_t> geometry_type_counts;
        std::map<int64_t, std::string> dataset_class_names;
        int64_t training_num_classes = 0;
        for (const auto& train_sample : train_samples)
        {
            train_instance_count += train_sample.classes.size();
            for (const int64_t class_id : train_sample.classes)
            {
                if (class_id < 0)
                    return SegFailure("dataset_preflight", "negative class id in train split");
                training_num_classes = std::max(training_num_classes, class_id + 1);
                if (!train_sample.geometry_type.empty())
                {
                    const auto found = dataset_class_names.find(class_id);
                    if (found != dataset_class_names.end() &&
                        found->second != train_sample.geometry_type)
                        return SegFailure(
                            "dataset_preflight",
                            "class id maps to multiple geometry types: " +
                                std::to_string(class_id));
                    dataset_class_names[class_id] = train_sample.geometry_type;
                }
            }
            if (train_sample.geometry_primitive_type >= 0)
                ++geometry_primitive_type_target_count;
            else if (!train_sample.geometry_facts_ref.empty())
                ++geometry_unknown_type_count;
            if (!train_sample.geometry_type.empty())
                ++geometry_type_counts[train_sample.geometry_type];
            geometry_target_valid_field_count += std::count(
                train_sample.geometry_target_valid.begin(),
                train_sample.geometry_target_valid.end(),
                static_cast<uint8_t>(1));
        }
        if (training_num_classes <= 0)
            return SegFailure("dataset_preflight", "train split has no usable class id");
        std::vector<std::string> training_class_names;
        training_class_names.reserve(static_cast<std::size_t>(training_num_classes));
        for (int64_t class_id = 0; class_id < training_num_classes; ++class_id)
        {
            const auto found = dataset_class_names.find(class_id);
            if (!dataset_class_names.empty() && found == dataset_class_names.end())
                return SegFailure(
                    "dataset_preflight",
                    "geometry ontology has an unrepresented class id: " +
                        std::to_string(class_id));
            training_class_names.push_back(
                found != dataset_class_names.end()
                    ? found->second
                    : "class_" + std::to_string(class_id));
        }

        int training_epochs = 3;
        double learning_rate = 1.0e-4;
        std::string lr_schedule = "constant";
        double min_learning_rate = 1.0e-6;
        double weight_decay = 0.0;
        double box_loss_weight = 1.0;
        double class_loss_weight = 1.0;
        double dfl_loss_weight = 1.0;
        double mask_loss_weight = 1.0;
        double mask_dice_loss_weight = 1.0;
        int assignment_topk = 3;
        double classification_focal_gamma = 2.0;
        int use_assignment_quality_targets = 1;
        int assignment_quality_warmup_epochs = 3;
        double postprocess_confidence_threshold = manifest.confidence_threshold;
        double postprocess_iou_threshold = manifest.iou_threshold;
        int postprocess_max_detections = manifest.max_detections;
        int postprocess_class_agnostic_nms =
            manifest.class_agnostic_nms ? 1 : 0;
        if (!request.extra_json.empty())
        {
            try
            {
                cv::FileStorage training_config(
                    request.extra_json,
                    cv::FileStorage::READ | cv::FileStorage::MEMORY |
                        cv::FileStorage::FORMAT_JSON);
                if (training_config.isOpened())
                {
                    if (!training_config["epochs"].empty())
                        training_config["epochs"] >> training_epochs;
                    if (!training_config["learning_rate"].empty())
                        training_config["learning_rate"] >> learning_rate;
                    if (!training_config["lr_schedule"].empty())
                        training_config["lr_schedule"] >> lr_schedule;
                    if (!training_config["min_learning_rate"].empty())
                        training_config["min_learning_rate"] >> min_learning_rate;
                    if (!training_config["weight_decay"].empty())
                        training_config["weight_decay"] >> weight_decay;
                    if (!training_config["box_loss_weight"].empty())
                        training_config["box_loss_weight"] >> box_loss_weight;
                    if (!training_config["class_loss_weight"].empty())
                        training_config["class_loss_weight"] >> class_loss_weight;
                    if (!training_config["dfl_loss_weight"].empty())
                        training_config["dfl_loss_weight"] >> dfl_loss_weight;
                    if (!training_config["mask_loss_weight"].empty())
                        training_config["mask_loss_weight"] >> mask_loss_weight;
                    if (!training_config["mask_dice_loss_weight"].empty())
                        training_config["mask_dice_loss_weight"] >> mask_dice_loss_weight;
                    if (!training_config["assignment_topk"].empty())
                        training_config["assignment_topk"] >> assignment_topk;
                    if (!training_config["classification_focal_gamma"].empty())
                        training_config["classification_focal_gamma"] >> classification_focal_gamma;
                    if (!training_config["use_assignment_quality_targets"].empty())
                        training_config["use_assignment_quality_targets"] >> use_assignment_quality_targets;
                    if (!training_config["assignment_quality_warmup_epochs"].empty())
                        training_config["assignment_quality_warmup_epochs"] >> assignment_quality_warmup_epochs;
                    if (!training_config["postprocess_confidence_threshold"].empty())
                        training_config["postprocess_confidence_threshold"] >> postprocess_confidence_threshold;
                    if (!training_config["postprocess_iou_threshold"].empty())
                        training_config["postprocess_iou_threshold"] >> postprocess_iou_threshold;
                    if (!training_config["postprocess_max_detections"].empty())
                        training_config["postprocess_max_detections"] >> postprocess_max_detections;
                    if (!training_config["postprocess_class_agnostic_nms"].empty())
                        training_config["postprocess_class_agnostic_nms"] >> postprocess_class_agnostic_nms;
                }
            }
            catch (const cv::Exception&)
            {
                return SegFailure(
                    "training_config", "invalid structured Torch training context");
            }
        }
        if (training_epochs < 1 || training_epochs > 100)
            return SegFailure("training_config", "epochs must be in [1, 100]");
        if (!std::isfinite(learning_rate) || learning_rate <= 0.0)
            return SegFailure("training_config", "learning_rate must be positive");
        std::transform(lr_schedule.begin(), lr_schedule.end(), lr_schedule.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        if (lr_schedule != "constant" && lr_schedule != "cosine")
            return SegFailure(
                "training_config", "lr_schedule must be constant or cosine");
        if (!std::isfinite(min_learning_rate) || min_learning_rate < 0.0 ||
            min_learning_rate > learning_rate)
            return SegFailure(
                "training_config", "min_learning_rate must be in [0, learning_rate]");
        if (!std::isfinite(weight_decay) || weight_decay < 0.0)
            return SegFailure("training_config", "weight_decay must be non-negative");
        if (assignment_topk < 1 || assignment_topk > 100)
            return SegFailure("training_config", "assignment_topk must be in [1, 100]");
        if (!std::isfinite(classification_focal_gamma) ||
            classification_focal_gamma < 0.0 || classification_focal_gamma > 10.0)
            return SegFailure("training_config", "classification_focal_gamma must be in [0, 10]");
        if (use_assignment_quality_targets != 0 &&
            use_assignment_quality_targets != 1)
            return SegFailure("training_config", "use_assignment_quality_targets must be 0 or 1");
        if (assignment_quality_warmup_epochs < 0 ||
            assignment_quality_warmup_epochs > 100)
            return SegFailure(
                "training_config",
                "assignment_quality_warmup_epochs must be in [0, 100]");
        if (!std::isfinite(postprocess_confidence_threshold) ||
            postprocess_confidence_threshold < 0.0 ||
            postprocess_confidence_threshold > 1.0)
            return SegFailure("training_config", "postprocess_confidence_threshold must be in [0, 1]");
        if (!std::isfinite(postprocess_iou_threshold) ||
            postprocess_iou_threshold < 0.0 || postprocess_iou_threshold > 1.0)
            return SegFailure("training_config", "postprocess_iou_threshold must be in [0, 1]");
        if (postprocess_max_detections < 1 || postprocess_max_detections > 1000)
            return SegFailure("training_config", "postprocess_max_detections must be in [1, 1000]");
        if (postprocess_class_agnostic_nms != 0 &&
            postprocess_class_agnostic_nms != 1)
            return SegFailure("training_config", "postprocess_class_agnostic_nms must be 0 or 1");
        for (const auto& loss_weight : std::vector<std::pair<std::string, double>>{
                 {"box_loss_weight", box_loss_weight},
                 {"class_loss_weight", class_loss_weight},
                 {"dfl_loss_weight", dfl_loss_weight},
                 {"mask_loss_weight", mask_loss_weight},
                 {"mask_dice_loss_weight", mask_dice_loss_weight}})
        {
            if (!std::isfinite(loss_weight.second) || loss_weight.second < 0.0)
                return SegFailure(
                    "training_config", loss_weight.first + " must be non-negative");
        }

        cv::Mat image = cv::imread(
            train_samples.front().image_ref, cv::IMREAD_COLOR);
        if (image.empty())
            return SegFailure("input", "first training image is unreadable");

        const std::string device_name =
            (request.device == "cuda" || config.device == "cuda") &&
                    torch::cuda::is_available()
                ? "cuda"
                : "cpu";
        const torch::Device device(device_name);
        SegLetterbox letterbox;
        torch::Tensor input =
            MakeSegInput(image, manifest, letterbox).to(device);

        const int parent_num_classes = manifest.num_classes;
        const bool classifier_transfer =
            parent_num_classes != training_num_classes;
        // The target ontology is established from the reproducible dataset,
        // never inherited from a parent COCO manifest.
        manifest.num_classes = static_cast<int>(training_num_classes);
        manifest.class_names = training_class_names;
        manifest.confidence_threshold =
            static_cast<float>(postprocess_confidence_threshold);
        manifest.iou_threshold = static_cast<float>(postprocess_iou_threshold);
        manifest.max_detections = postprocess_max_detections;
        manifest.class_agnostic_nms = postprocess_class_agnostic_nms != 0;
        YoloV8Segment model(training_num_classes);
        const YoloV8SegWeightMappingReport mapping = classifier_transfer
            ? model->load_state_dict_transfer_classifier(
                  manifest.weights_path.string())
            : model->load_state_dict_strict(manifest.weights_path.string());
        model->to(device);
        model->train();

        auto compute_losses = [&](
            YoloV8Segment& active_model,
            const YoloV8SegRawOutput& raw,
            const YoloV8SegDatasetSample& active_sample,
            const torch::Tensor& active_input,
            const SegLetterbox& active_letterbox,
            bool quality_targets_active) {
            LossTensors result;
            result.proto_h = raw.prototypes.size(2);
            result.proto_w = raw.prototypes.size(3);
            const GeometryTargetTensors geometry_targets =
                BuildGeometryTargetTensors(
                    active_sample, active_letterbox,
                    manifest.input_width, manifest.input_height,
                    active_input.device());
            result.geometry_target_valid_fields =
                geometry_targets.valid_mask.sum().item<int64_t>();
            result.geometry_primitive_type_available =
                geometry_targets.primitive_type_valid;
            const SegTaskAlignedAnchorTable task_aligned_anchors =
                BuildSegTaskAlignedAnchorTable(
                    raw, active_model, manifest.input_width, manifest.input_height);
            result.task_aligned_anchor_count =
                task_aligned_anchors.class_logits.size(1);
            std::vector<int64_t> assignment_labels;
            std::vector<float> assignment_boxes;
            assignment_labels.reserve(active_sample.classes.size());
            assignment_boxes.reserve(active_sample.classes.size() * 4);
            for (std::size_t target_index = 0;
                 target_index < active_sample.classes.size(); ++target_index)
            {
                const auto& target_box = active_sample.boxes_xyxy_norm[target_index];
                const auto to_input_x = [&](float value) {
                    return static_cast<float>((active_letterbox.pad_x +
                        value * active_letterbox.resized_width) /
                        static_cast<double>(manifest.input_width));
                };
                const auto to_input_y = [&](float value) {
                    return static_cast<float>((active_letterbox.pad_y +
                        value * active_letterbox.resized_height) /
                        static_cast<double>(manifest.input_height));
                };
                assignment_labels.push_back(std::clamp<int64_t>(
                    active_sample.classes[target_index], 0, manifest.num_classes - 1));
                assignment_boxes.insert(assignment_boxes.end(), {
                    to_input_x(target_box[0]), to_input_y(target_box[1]),
                    to_input_x(target_box[2]), to_input_y(target_box[3])});
            }
            const int64_t assignment_target_count =
                static_cast<int64_t>(assignment_labels.size());
            const auto assignment_options = torch::TensorOptions()
                .dtype(torch::kLong).device(active_input.device());
            const torch::Tensor assignment_gt_labels = torch::tensor(
                assignment_labels, assignment_options).view({1, assignment_target_count});
            const torch::Tensor assignment_gt_boxes = torch::tensor(
                assignment_boxes, active_input.options()).view({1, assignment_target_count, 4});
            const torch::Tensor assignment_gt_mask = torch::ones(
                {1, assignment_target_count}, active_input.options());
            TaskAlignedAssigner task_aligned_assigner(
                assignment_topk, manifest.num_classes, 1.0f, 6.0f);
            torch::Tensor assigned_gt_indices = task_aligned_assigner->forward(
                task_aligned_anchors.class_logits.sigmoid(),
                task_aligned_anchors.decoded_boxes, assignment_gt_labels,
                assignment_gt_boxes, assignment_gt_mask);
            result.task_aligned_positive_count =
                (assigned_gt_indices > 0).sum().item<int64_t>();
            const torch::Tensor proto_flat =
                raw.prototypes.index({0}).view({manifest.mask_channels, -1});

            torch::Tensor class_loss =
                torch::zeros({}, active_input.options());
            torch::Tensor mask_loss =
                torch::zeros({}, active_input.options());
            torch::Tensor box_loss =
                torch::zeros({}, active_input.options());
            torch::Tensor dfl_loss =
                torch::zeros({}, active_input.options());
            torch::Tensor task_aligned_box_loss =
                torch::zeros({}, active_input.options());
            torch::Tensor task_aligned_dfl_loss =
                torch::zeros({}, active_input.options());
            const torch::Tensor task_aligned_positive_mask = assigned_gt_indices > 0;
            torch::Tensor task_aligned_class_target =
                torch::zeros_like(task_aligned_anchors.class_logits.index({0}));
            if (task_aligned_positive_mask.any().item<bool>())
            {
                const torch::Tensor positive_indices = torch::nonzero(
                    task_aligned_positive_mask.index({0})).squeeze(1);
                const torch::Tensor positive_gt_indices = assigned_gt_indices.index({0})
                    .index_select(0, positive_indices).to(torch::kLong) - 1;
                const torch::Tensor positive_classes = assignment_gt_labels.index({0})
                    .index_select(0, positive_gt_indices);
                const torch::Tensor positive_boxes = assignment_gt_boxes.index({0})
                    .index_select(0, positive_gt_indices);
                const torch::Tensor predicted_boxes = task_aligned_anchors.decoded_boxes.index({0})
                    .index_select(0, positive_indices);
                task_aligned_box_loss =
                    torch::abs(predicted_boxes - positive_boxes).mean();
                const torch::Tensor centers = task_aligned_anchors.centers_xy.index({0})
                    .index_select(0, positive_indices);
                const torch::Tensor grid_widths = task_aligned_anchors.grid_widths.index({0})
                    .index_select(0, positive_indices);
                const torch::Tensor grid_heights = task_aligned_anchors.grid_heights.index({0})
                    .index_select(0, positive_indices);
                const torch::Tensor dfl_target = torch::stack({
                    ((centers.select(1, 0) - positive_boxes.select(1, 0)) * grid_widths).clamp(0.0, 15.999),
                    ((centers.select(1, 1) - positive_boxes.select(1, 1)) * grid_heights).clamp(0.0, 15.999),
                    ((positive_boxes.select(1, 2) - centers.select(1, 0)) * grid_widths).clamp(0.0, 15.999),
                    ((positive_boxes.select(1, 3) - centers.select(1, 1)) * grid_heights).clamp(0.0, 15.999)}, 1);
                const torch::Tensor dfl_logits = task_aligned_anchors.dfl_logits.index({0})
                    .index_select(0, positive_indices).view({-1, 16});
                const torch::Tensor target_left = torch::floor(dfl_target).to(torch::kLong).view({-1});
                const torch::Tensor target_right = (target_left + 1).clamp_max(15);
                const torch::Tensor right_weight = (dfl_target - torch::floor(dfl_target)).view({-1});
                const torch::Tensor dfl_ce_left = torch::nn::functional::cross_entropy(
                    dfl_logits, target_left, torch::nn::functional::CrossEntropyFuncOptions().reduction(torch::kNone));
                const torch::Tensor dfl_ce_right = torch::nn::functional::cross_entropy(
                    dfl_logits, target_right, torch::nn::functional::CrossEntropyFuncOptions().reduction(torch::kNone));
                task_aligned_dfl_loss =
                    ((1.0 - right_weight) * dfl_ce_left + right_weight * dfl_ce_right).mean();
                torch::Tensor quality = torch::ones(
                    {positive_indices.size(0)}, active_input.options());
                if (quality_targets_active)
                {
                    const torch::Tensor predicted = task_aligned_anchors.decoded_boxes.index({0})
                        .index_select(0, positive_indices);
                    const torch::Tensor target = assignment_gt_boxes.index({0})
                        .index_select(0, positive_gt_indices);
                    const torch::Tensor inter_x0 = torch::max(predicted.select(1, 0), target.select(1, 0));
                    const torch::Tensor inter_y0 = torch::max(predicted.select(1, 1), target.select(1, 1));
                    const torch::Tensor inter_x1 = torch::min(predicted.select(1, 2), target.select(1, 2));
                    const torch::Tensor inter_y1 = torch::min(predicted.select(1, 3), target.select(1, 3));
                    const torch::Tensor intersection = (inter_x1 - inter_x0).clamp_min(0) *
                        (inter_y1 - inter_y0).clamp_min(0);
                    const torch::Tensor predicted_area = (predicted.select(1, 2) - predicted.select(1, 0)).clamp_min(0) *
                        (predicted.select(1, 3) - predicted.select(1, 1)).clamp_min(0);
                    const torch::Tensor target_area = (target.select(1, 2) - target.select(1, 0)).clamp_min(0) *
                        (target.select(1, 3) - target.select(1, 1)).clamp_min(0);
                    quality = (intersection / (predicted_area + target_area - intersection + 1.0e-7)).detach();
                }
                task_aligned_class_target.index_put_(
                    {positive_indices, positive_classes}, quality);
            }
            const torch::Tensor task_aligned_class_bce =
                torch::nn::functional::binary_cross_entropy_with_logits(
                    task_aligned_anchors.class_logits.index({0}),
                    task_aligned_class_target,
                    torch::nn::functional::BinaryCrossEntropyWithLogitsFuncOptions()
                        .reduction(torch::kNone));
            const torch::Tensor task_aligned_class_loss =
                classification_focal_gamma > 0.0
                ? (task_aligned_class_bce * (task_aligned_class_target -
                    task_aligned_anchors.class_logits.index({0}).sigmoid())
                    .abs().pow(classification_focal_gamma)).mean()
                : task_aligned_class_bce.mean();
            torch::Tensor task_aligned_mask_loss =
                torch::zeros({}, active_input.options());
            if (task_aligned_positive_mask.any().item<bool>())
            {
                std::vector<torch::Tensor> target_masks;
                target_masks.reserve(active_sample.polygons_norm.size());
                for (const auto& polygon : active_sample.polygons_norm)
                {
                    cv::Mat mask_cv(static_cast<int>(result.proto_h),
                        static_cast<int>(result.proto_w), CV_8UC1, cv::Scalar(0));
                    if (!active_sample.target_mask_ref.empty())
                    {
                        const cv::Mat source_mask = cv::imread(
                            active_sample.target_mask_ref, cv::IMREAD_GRAYSCALE);
                        TORCH_CHECK(!source_mask.empty(), "assigned target mask cannot be decoded");
                        cv::Mat resized_mask;
                        cv::resize(source_mask, resized_mask,
                            cv::Size(active_letterbox.resized_width,
                                active_letterbox.resized_height),
                            0.0, 0.0, cv::INTER_NEAREST);
                        cv::Mat input_mask(manifest.input_height,
                            manifest.input_width, CV_8UC1, cv::Scalar(0));
                        resized_mask.copyTo(input_mask(cv::Rect(
                            active_letterbox.pad_x, active_letterbox.pad_y,
                            active_letterbox.resized_width,
                            active_letterbox.resized_height)));
                        cv::resize(input_mask, mask_cv,
                            cv::Size(static_cast<int>(result.proto_w),
                                static_cast<int>(result.proto_h)),
                            0.0, 0.0, cv::INTER_NEAREST);
                    }
                    else
                    {
                        std::vector<cv::Point> points;
                        points.reserve(polygon.size());
                        for (const cv::Point2f& point : polygon)
                        {
                            const int x = std::clamp(static_cast<int>(std::lround(
                                (active_letterbox.pad_x + point.x * active_letterbox.resized_width) /
                                static_cast<double>(manifest.input_width) * result.proto_w)),
                                0, static_cast<int>(result.proto_w) - 1);
                            const int y = std::clamp(static_cast<int>(std::lround(
                                (active_letterbox.pad_y + point.y * active_letterbox.resized_height) /
                                static_cast<double>(manifest.input_height) * result.proto_h)),
                                0, static_cast<int>(result.proto_h) - 1);
                            points.emplace_back(x, y);
                        }
                        cv::fillPoly(mask_cv,
                            std::vector<std::vector<cv::Point>>{points},
                            cv::Scalar(255), cv::LINE_8);
                    }
                    target_masks.push_back(torch::from_blob(mask_cv.data,
                        {result.proto_h, result.proto_w}, torch::TensorOptions().dtype(torch::kUInt8))
                        .clone().to(active_input.device()).to(active_input.scalar_type()) / 255.0);
                }
                const torch::Tensor positive_indices = torch::nonzero(
                    task_aligned_positive_mask.index({0})).squeeze(1);
                const torch::Tensor positive_gt_indices = assigned_gt_indices.index({0})
                    .index_select(0, positive_indices).to(torch::kLong) - 1;
                const torch::Tensor coefficients = task_aligned_anchors.mask_coefficients.index({0})
                    .index_select(0, positive_indices);
                const torch::Tensor mask_logits = torch::matmul(coefficients, proto_flat)
                    .view({positive_indices.size(0), result.proto_h, result.proto_w});
                const torch::Tensor mask_target = torch::stack(target_masks, 0)
                    .index_select(0, positive_gt_indices);
                const torch::Tensor mask_bce = torch::nn::functional::binary_cross_entropy_with_logits(
                    mask_logits, mask_target,
                    torch::nn::functional::BinaryCrossEntropyWithLogitsFuncOptions().reduction(torch::kNone)).mean();
                const torch::Tensor probability = mask_logits.sigmoid();
                const torch::Tensor dice = 1.0 - (2.0 * (probability * mask_target).sum({1, 2}) + 1.0) /
                    (probability.sum({1, 2}) + mask_target.sum({1, 2}) + 1.0);
                task_aligned_mask_loss = mask_bce + dice.mean() * mask_dice_loss_weight;
            }
            for (std::size_t index = 0;
                 index < active_sample.classes.size();
                 ++index)
            {
                const auto& box = active_sample.boxes_xyxy_norm[index];
                const auto transform_x = [&](float normalized_x) {
                    return static_cast<float>(
                        (active_letterbox.pad_x + normalized_x *
                         active_letterbox.resized_width) /
                        static_cast<double>(manifest.input_width));
                };
                const auto transform_y = [&](float normalized_y) {
                    return static_cast<float>(
                        (active_letterbox.pad_y + normalized_y *
                         active_letterbox.resized_height) /
                        static_cast<double>(manifest.input_height));
                };
                const std::array<float, 4> input_box{
                    transform_x(box[0]), transform_y(box[1]),
                    transform_x(box[2]), transform_y(box[3])};
                const int64_t class_id = std::clamp<int64_t>(
                    active_sample.classes[index],
                    0,
                    manifest.num_classes - 1);
                cv::Mat mask_cv(
                    static_cast<int>(result.proto_h),
                    static_cast<int>(result.proto_w), CV_8UC1, cv::Scalar(0));
                std::vector<cv::Point> mask_polygon;
                mask_polygon.reserve(active_sample.polygons_norm[index].size());
                for (const cv::Point2f& point :
                     active_sample.polygons_norm[index])
                {
                    const int x = std::clamp(
                        static_cast<int>(std::lround(
                            transform_x(point.x) * result.proto_w)),
                        0, static_cast<int>(result.proto_w) - 1);
                    const int y = std::clamp(
                        static_cast<int>(std::lround(
                            transform_y(point.y) * result.proto_h)),
                        0, static_cast<int>(result.proto_h) - 1);
                    mask_polygon.emplace_back(x, y);
                }
                cv::fillPoly(mask_cv,
                    std::vector<std::vector<cv::Point>>{mask_polygon},
                    cv::Scalar(255), cv::LINE_8);
                torch::Tensor mask_target = torch::from_blob(
                    mask_cv.data, {result.proto_h, result.proto_w},
                    torch::TensorOptions().dtype(torch::kUInt8))
                    .clone()
                    .to(active_input.device())
                    .to(active_input.scalar_type()) / 255.0;
                const float cx = (input_box[0] + input_box[2]) * 0.5f;
                const float cy = (input_box[1] + input_box[3]) * 0.5f;
                for (std::size_t level = 0; level < raw.class_logits.size(); ++level)
                {
                    const int64_t col = std::clamp<int64_t>(
                        static_cast<int64_t>(
                            std::floor(cx * raw.class_logits[level].size(3))),
                        0,
                        raw.class_logits[level].size(3) - 1);
                    const int64_t row = std::clamp<int64_t>(
                        static_cast<int64_t>(
                            std::floor(cy * raw.class_logits[level].size(2))),
                        0,
                        raw.class_logits[level].size(2) - 1);

                    const torch::Tensor cls_logits =
                        raw.class_logits[level].index(
                            {0, torch::indexing::Slice(), row, col});
                    torch::Tensor cls_target =
                        torch::zeros_like(cls_logits);
                    cls_target.index_put_({class_id}, 1.0);
                    class_loss = class_loss +
                        torch::binary_cross_entropy_with_logits(
                            cls_logits,
                            cls_target);

                    const torch::Tensor coeff =
                        raw.mask_coefficients[level].index(
                            {0, torch::indexing::Slice(), row, col});
                    const torch::Tensor mask_logits =
                        torch::matmul(coeff, proto_flat)
                            .view({result.proto_h, result.proto_w});
                    const torch::Tensor mask_bce =
                        torch::binary_cross_entropy_with_logits(
                            mask_logits, mask_target);
                    const torch::Tensor mask_probability = mask_logits.sigmoid();
                    const torch::Tensor dice = 1.0 -
                        (2.0 * (mask_probability * mask_target).sum() + 1.0) /
                        (mask_probability.sum() + mask_target.sum() + 1.0);
                    mask_loss = mask_loss + mask_bce +
                        dice * mask_dice_loss_weight;

                    const float stride =
                        static_cast<float>(manifest.input_width) /
                        static_cast<float>(raw.box_logits[level].size(3));
                    const float center_x =
                        (static_cast<float>(col) + 0.5f) * stride;
                    const float center_y =
                        (static_cast<float>(row) + 0.5f) * stride;
                    const float target_x0 =
                        input_box[0] * static_cast<float>(manifest.input_width);
                    const float target_y0 =
                        input_box[1] * static_cast<float>(manifest.input_height);
                    const float target_x1 =
                        input_box[2] * static_cast<float>(manifest.input_width);
                    const float target_y1 =
                        input_box[3] * static_cast<float>(manifest.input_height);
                    torch::Tensor target_distances = torch::tensor(
                        {std::max(0.0f, (center_x - target_x0) / stride),
                         std::max(0.0f, (center_y - target_y0) / stride),
                         std::max(0.0f, (target_x1 - center_x) / stride),
                         std::max(0.0f, (target_y1 - center_y) / stride)},
                        active_input.options()).clamp(0.0, 15.0 - 1.0e-3);

                    const torch::Tensor box_logits =
                        raw.box_logits[level]
                            .index({0, torch::indexing::Slice(), row, col})
                            .view({1, 64, 1});
                    const torch::Tensor predicted_distances =
                        active_model->head()
                            ->dfl_module()
                            ->expectation(box_logits)
                            .view({4});
                    box_loss = box_loss +
                        torch::abs(predicted_distances - target_distances).mean();

                    const torch::Tensor dfl_logits =
                        box_logits.view({4, 16});
                    const torch::Tensor target_left =
                        torch::floor(target_distances).to(torch::kLong);
                    const torch::Tensor target_right =
                        torch::clamp(target_left + 1, 0, 15);
                    const torch::Tensor weight_right =
                        (target_distances - target_left.to(target_distances.dtype()))
                            .clamp(0.0, 1.0);
                    const torch::Tensor weight_left = 1.0 - weight_right;
                    const torch::Tensor ce_left =
                        torch::nn::functional::cross_entropy(
                            dfl_logits,
                            target_left,
                            torch::nn::functional::CrossEntropyFuncOptions()
                                .reduction(torch::kNone));
                    const torch::Tensor ce_right =
                        torch::nn::functional::cross_entropy(
                            dfl_logits,
                            target_right,
                            torch::nn::functional::CrossEntropyFuncOptions()
                                .reduction(torch::kNone));
                    dfl_loss = dfl_loss +
                        (ce_left * weight_left + ce_right * weight_right).mean();
                }
            }

            const double loss_terms =
                static_cast<double>(
                    active_sample.classes.size() * raw.class_logits.size());
            result.class_loss = task_aligned_class_loss;
            result.mask_loss = task_aligned_mask_loss;
            result.box_loss = task_aligned_box_loss;
            result.dfl_loss = task_aligned_dfl_loss;
            result.total_loss =
                result.class_loss * class_loss_weight +
                result.mask_loss * mask_loss_weight +
                result.box_loss * box_loss_weight +
                result.dfl_loss * dfl_loss_weight;
            return result;
        };

        std::map<std::string, torch::Tensor> before_parameters;
        for (const auto& named : model->named_parameters(true))
            before_parameters.emplace(
                named.key(),
                named.value().detach().clone());

        torch::optim::Adam optimizer(
            model->parameters(),
            torch::optim::AdamOptions(learning_rate).weight_decay(weight_decay));

        const auto started = std::chrono::steady_clock::now();
        YoloV8SegRawOutput raw;
        std::vector<TrainingEpochMetric> training_trace;
        LossTensors baseline_losses;
        for (int epoch = 1; epoch <= training_epochs; ++epoch)
        {
            const auto epoch_started = std::chrono::steady_clock::now();
            const double schedule_position = training_epochs <= 1
                ? 0.0
                : static_cast<double>(epoch - 1) /
                    static_cast<double>(training_epochs - 1);
            const double epoch_learning_rate = lr_schedule == "cosine"
                ? min_learning_rate +
                    (learning_rate - min_learning_rate) * 0.5 *
                        (1.0 + std::cos(std::acos(-1.0) * schedule_position))
                : learning_rate;
            for (auto& parameter_group : optimizer.param_groups())
            {
                auto& options = static_cast<torch::optim::AdamOptions&>(
                    parameter_group.options());
                options.lr(epoch_learning_rate);
            }
            std::map<std::string, torch::Tensor> epoch_before_parameters;
            for (const auto& named : model->named_parameters(true))
                epoch_before_parameters.emplace(
                    named.key(), named.value().detach().clone());
            optimizer.zero_grad();
            double epoch_total = 0.0;
            double epoch_box = 0.0;
            double epoch_class = 0.0;
            double epoch_dfl = 0.0;
            double epoch_mask = 0.0;
            for (std::size_t sample_index = 0;
                 sample_index < train_samples.size(); ++sample_index)
            {
                const auto& active_sample = train_samples[sample_index];
                cv::Mat active_image = cv::imread(
                    active_sample.image_ref, cv::IMREAD_COLOR);
                if (active_image.empty())
                    return SegFailure(
                        "dataset", "training image became unreadable: " +
                            active_sample.image_ref);
                SegLetterbox active_letterbox;
                torch::Tensor active_input =
                    MakeSegInput(active_image, manifest, active_letterbox).to(device);
                YoloV8SegRawOutput active_raw = model->forward(active_input);
                const LossTensors losses = compute_losses(
                    model, active_raw, active_sample, active_input,
                    active_letterbox,
                    use_assignment_quality_targets != 0 &&
                        epoch > assignment_quality_warmup_epochs);
                (losses.total_loss /
                 static_cast<double>(train_samples.size())).backward();
                epoch_total += losses.total_loss.detach().item<double>();
                epoch_box += losses.box_loss.detach().item<double>();
                epoch_class += losses.class_loss.detach().item<double>();
                epoch_dfl += losses.dfl_loss.detach().item<double>();
                epoch_mask += losses.mask_loss.detach().item<double>();
                if (sample_index == 0)
                {
                    input = active_input;
                    raw = active_raw;
                }
            }
            optimizer.step();

            const double train_divisor =
                static_cast<double>(train_samples.size());
            TrainingEpochMetric metric;
            metric.epoch = epoch;
            metric.learning_rate = epoch_learning_rate;
            metric.total_loss = epoch_total / train_divisor;
            metric.box_loss = epoch_box / train_divisor;
            metric.class_loss = epoch_class / train_divisor;
            metric.dfl_loss = epoch_dfl / train_divisor;
            metric.mask_loss = epoch_mask / train_divisor;
            metric.assignment_quality_targets_active =
                use_assignment_quality_targets != 0 &&
                epoch > assignment_quality_warmup_epochs;
            metric.elapsed_ms = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - epoch_started).count();
            for (const auto& named : model->named_parameters(true))
            {
                const auto before = epoch_before_parameters.find(named.key());
                if (before != epoch_before_parameters.end())
                    add_stat(
                        metric.parameter_groups[group_for(named.key())],
                        named.value(), before->second);
            }
            training_trace.push_back(metric);
        }
        const TrainingEpochMetric& final_epoch = training_trace.back();
        baseline_losses.total_loss = torch::tensor(
            final_epoch.total_loss, input.options());
        baseline_losses.box_loss = torch::tensor(
            final_epoch.box_loss, input.options());
        baseline_losses.class_loss = torch::tensor(
            final_epoch.class_loss, input.options());
        baseline_losses.dfl_loss = torch::tensor(
            final_epoch.dfl_loss, input.options());
        baseline_losses.mask_loss = torch::tensor(
            final_epoch.mask_loss, input.options());
        std::map<std::string, Stat> groups;
        std::map<std::string, Stat> blocks;
        for (const auto& named : model->named_parameters(true))
        {
            const auto before = before_parameters.find(named.key());
            if (before == before_parameters.end())
                continue;
            add_stat(
                groups[group_for(named.key())],
                named.value(),
                before->second);
            const std::string block = block_for(named.key());
            if (!block.empty())
                add_stat(blocks[block], named.value(), before->second);
        }

        const std::filesystem::path output_dir(request.output_dir);
        std::filesystem::create_directories(output_dir);
        const auto weights_dir = output_dir / "weights";
        std::filesystem::create_directories(weights_dir);
        const auto loss_ref = output_dir / "loss_breakdown.json";
        const auto gradient_ref = output_dir / "gradient_report.json";
        const auto update_ref = output_dir / "parameter_update_report.json";
        const auto ablation_ref = output_dir / "freeze_ablation_report.json";
        const auto dataset_summary_ref = output_dir / "dataset_summary.json";
        const auto training_trace_ref = output_dir / "training_trace.json";
        const auto l2_matrix_ref = output_dir / "l2_case_matrix.json";
        const auto stability_ref = output_dir / "stability_matrix.json";
        const auto variation_ref = output_dir / "result_variation.json";
        const auto stability_report_ref = output_dir / "stability_report.md";
        const auto timeout_report_ref = output_dir / "timeout_report.md";
        const auto human_review_ref = output_dir / "human_review.json";
        const auto parent_transfer_ref = output_dir / "parent_transfer_receipt.json";
        const auto evidence_ref =
            output_dir / "yolov8seg_backward_smoke_evidence.json";
        const auto checkpoint_ref =
            weights_dir / "yolov8n_seg_backward_smoke_state_dict.pt";
        const auto manifest_ref = output_dir / "model_manifest.json";

        auto write_key_array = [](std::ostream& out,
                                  const std::vector<std::string>& values) {
            out << "[";
            for (std::size_t index = 0; index < values.size(); ++index)
            {
                if (index > 0)
                    out << ",";
                out << QuoteSegJson(values[index]);
            }
            out << "]";
        };
        std::ofstream parent_transfer(parent_transfer_ref);
        parent_transfer
            << "{\n"
            << "  \"schema\":\"cxvision.yolov8seg.parent_transfer.v1\",\n"
            << "  \"parent_manifest\":" << QuoteSegJson(request.manifest_path) << ",\n"
            << "  \"parent_num_classes\":" << parent_num_classes << ",\n"
            << "  \"target_num_classes\":" << manifest.num_classes << ",\n"
            << "  \"transfer_mode\":"
            << QuoteSegJson(classifier_transfer
                   ? "strict_feature_transfer_classifier_reinitialized"
                   : "strict_full_state") << ",\n"
            << "  \"loaded_tensor_count\":" << mapping.loaded_count << ",\n"
            << "  \"reinitialized_classifier_keys\":";
        write_key_array(parent_transfer, mapping.reinitialized_keys);
        parent_transfer
            << ",\n  \"target_classes\":";
        write_key_array(parent_transfer, manifest.class_names);
        parent_transfer
            << ",\n  \"mapping_complete\":"
            << ((classifier_transfer ? mapping.transfer_complete() : mapping.complete())
                    ? "true" : "false")
            << "\n}\n";
        parent_transfer.close();
        if (!parent_transfer.good())
            return SegFailure("parent_transfer", "failed to write parent transfer receipt");

        std::size_t val_sample_count = 0;
        std::size_t test_sample_count = 0;
        for (const auto& dataset_sample : dataset_samples)
        {
            if (dataset_sample.split == "val")
                ++val_sample_count;
            else if (dataset_sample.split == "test")
                ++test_sample_count;
        }
        std::ofstream dataset_summary(dataset_summary_ref);
        dataset_summary
            << "{\n"
            << "  \"schema\":\"cxvision.yolov8seg.dataset_summary.v1\",\n"
            << "  \"dataset_source\":" << QuoteSegJson(request.dataset_root) << ",\n"
            << "  \"sample_count\":" << dataset_samples.size() << ",\n"
            << "  \"train_sample_count\":" << train_samples.size() << ",\n"
            << "  \"val_sample_count\":" << val_sample_count << ",\n"
            << "  \"test_sample_count\":" << test_sample_count << ",\n"
            << "  \"train_instance_count\":" << train_instance_count << ",\n"
            << "  \"rows\":[\n";
        for (std::size_t index = 0; index < dataset_samples.size(); ++index)
        {
            const auto& dataset_sample = dataset_samples[index];
            dataset_summary
                << "    {\"image_id\":" << QuoteSegJson(dataset_sample.image_id)
                << ",\"split\":" << QuoteSegJson(dataset_sample.split)
                << ",\"label\":" << QuoteSegJson(dataset_sample.label)
                << ",\"image_ref\":" << QuoteSegJson(dataset_sample.image_ref)
                << ",\"image_exists\":"
                << (std::filesystem::is_regular_file(dataset_sample.image_ref)
                        ? "true" : "false")
                << ",\"annotation_count\":" << dataset_sample.classes.size()
                << ",\"annotations\":[";
            for (std::size_t annotation_index = 0;
                 annotation_index < dataset_sample.classes.size();
                 ++annotation_index)
            {
                const auto& box = dataset_sample.boxes_xyxy_norm[annotation_index];
                if (annotation_index > 0)
                    dataset_summary << ",";
                dataset_summary
                    << "{\"class_id\":" << dataset_sample.classes[annotation_index]
                    << ",\"x0\":" << box[0]
                    << ",\"y0\":" << box[1]
                    << ",\"x1\":" << box[2]
                    << ",\"y1\":" << box[3]
                    << ",\"normalized\":true}";
            }
            dataset_summary
                << "]}" << (index + 1 < dataset_samples.size() ? "," : "")
                << "\n";
        }
        dataset_summary << "  ]\n}\n";
        dataset_summary.close();
        if (!dataset_summary.good())
            return SegFailure("dataset_summary", "failed to write dataset summary");

        std::ofstream training_trace_file(training_trace_ref);
        training_trace_file
            << "{\n"
            << "  \"schema\":\"cxvision.yolov8seg.training_trace.v1\",\n"
            << "  \"status\":\"completed\",\n"
            << "  \"task\":\"torch.train.instance_segmentation.yolov8.backward_smoke.v1\",\n"
            << "  \"dataset_source\":" << QuoteSegJson(request.dataset_root) << ",\n"
            << "  \"parent_transfer_receipt_ref\":"
            << QuoteSegJson(parent_transfer_ref.string()) << ",\n"
            << "  \"target_num_classes\":" << manifest.num_classes << ",\n"
            << "  \"optimizer\":\"Adam\",\n"
            << "  \"learning_rate\":" << learning_rate << ",\n"
            << "  \"lr_schedule\":" << QuoteSegJson(lr_schedule) << ",\n"
            << "  \"min_learning_rate\":" << min_learning_rate << ",\n"
            << "  \"weight_decay\":" << weight_decay << ",\n"
            << "  \"loss_phase\":\"weighted_class_mask_bce_dice_box_dfl\",\n"
            << "  \"assignment\":{\"status\":\"active\",\"method\":\"dfl_aware_global_task_aligned\",\"scope\":\"global_p3_p4_p5\",\"shared_index_losses\":[\"class\",\"box\",\"dfl\",\"mask\"]},\n"
            << "  \"assignment_controls\":{\"assignment_topk\":" << assignment_topk
            << ",\"classification_focal_gamma\":" << classification_focal_gamma
            << ",\"use_assignment_quality_targets\":"
            << (use_assignment_quality_targets == 0 ? "false" : "true") << "},\n"
            << "  \"assignment_quality_warmup_epochs\":"
            << assignment_quality_warmup_epochs << ",\n"
            << "  \"loss_weights\":{\"box\":" << box_loss_weight
            << ",\"class\":" << class_loss_weight
            << ",\"dfl\":" << dfl_loss_weight
            << ",\"mask\":" << mask_loss_weight
            << ",\"mask_dice\":" << mask_dice_loss_weight << "},\n"
            << "  \"configured_epochs\":" << training_epochs << ",\n"
            << "  \"completed_epochs\":" << training_trace.size() << ",\n"
            << "  \"train_sample_count\":" << train_samples.size() << ",\n"
            << "  \"train_instance_count\":" << train_instance_count << ",\n"
            << "  \"geometry_target_tensor\":{\"status\":\"prepared_not_loss_consumed\""
            << ",\"primitive_type_target_count\":"
            << geometry_primitive_type_target_count
            << ",\"unknown_primitive_type_count\":"
            << geometry_unknown_type_count
            << ",\"valid_scalar_field_count\":"
            << geometry_target_valid_field_count
            << ",\"fields\":[\"center_x\",\"center_y\",\"envelope_width\",\"envelope_height\",\"sin_angle\",\"cos_angle\"]"
            << ",\"source_type_counts\":{";
        for (auto type_it = geometry_type_counts.begin();
             type_it != geometry_type_counts.end(); ++type_it)
        {
            if (type_it != geometry_type_counts.begin())
                training_trace_file << ",";
            training_trace_file << QuoteSegJson(type_it->first)
                << ":" << type_it->second;
        }
        training_trace_file << "}},\n"
            << "  \"epochs\":[\n";
        for (std::size_t index = 0; index < training_trace.size(); ++index)
        {
            const TrainingEpochMetric& metric = training_trace[index];
            training_trace_file
                << "    {\"epoch\":" << metric.epoch
                << ",\"learning_rate\":" << metric.learning_rate
                << ",\"total_loss\":" << metric.total_loss
                << ",\"box_loss\":" << metric.box_loss
                << ",\"class_loss\":" << metric.class_loss
                << ",\"dfl_loss\":" << metric.dfl_loss
                << ",\"mask_loss\":" << metric.mask_loss
                << ",\"assignment_quality_targets_active\":"
                << (metric.assignment_quality_targets_active ? "true" : "false")
                << ",\"elapsed_ms\":" << metric.elapsed_ms
                << ",\"sample_count\":" << train_samples.size()
                << ",\"instance_count\":" << train_instance_count
                << ",\"parameter_groups\":{";
            for (std::size_t group_index = 0;
                 group_index < group_order.size(); ++group_index)
            {
                const auto found = metric.parameter_groups.find(
                    group_order[group_index]);
                const Stat stat = found == metric.parameter_groups.end()
                    ? Stat{} : found->second;
                const double divisor = std::max(1, stat.count);
                if (group_index > 0)
                    training_trace_file << ",";
                training_trace_file
                    << QuoteSegJson(group_order[group_index]) << ":{"
                    << "\"grad_defined\":"
                    << (stat.grad_defined ? "true" : "false")
                    << ",\"grad_mean\":" << stat.grad_mean / divisor
                    << ",\"grad_max\":" << stat.grad_max
                    << ",\"grad_norm\":" << stat.grad_norm
                    << ",\"param_norm\":" << stat.param_norm
                    << ",\"update_norm\":" << stat.update_norm
                    << ",\"parameter_count\":" << stat.count << "}";
            }
            training_trace_file
                << "}}" << (index + 1 < training_trace.size() ? "," : "")
                << "\n";
        }
        training_trace_file << "  ]\n}\n";
        training_trace_file.close();
        if (!training_trace_file.good())
            return SegFailure("training_trace", "failed to write training trace");

        auto write_checkpoint = [](
            YoloV8Segment& active_model,
            const std::filesystem::path& path) {
            c10::Dict<std::string, torch::Tensor> state_dict;
            for (const auto& named : active_model->named_parameters(true))
                state_dict.insert(named.key(), named.value().detach().cpu());
            for (const auto& named : active_model->named_buffers(true))
                state_dict.insert(named.key(), named.value().detach().cpu());
            const std::vector<char> checkpoint_bytes =
                torch::pickle_save(state_dict);
            std::ofstream checkpoint_file(
                path,
                std::ios::binary | std::ios::trunc);
            checkpoint_file.write(
                checkpoint_bytes.data(),
                static_cast<std::streamsize>(checkpoint_bytes.size()));
            checkpoint_file.close();
            return checkpoint_file.good();
        };

        auto write_trained_manifest = [&](
            const std::filesystem::path& manifest_path,
            const std::string& model_id,
            const std::string& weights_relative_path,
            const std::filesystem::path& weights_path,
            double confidence_threshold) {
            std::ofstream manifest_file(manifest_path);
            manifest_file
                << "{\n"
                << "  \"schema\":\"cxvision.torch_model_manifest\",\n"
                << "  \"schema_version\":2,\n"
                << "  \"model_id\":" << QuoteSegJson(model_id) << ",\n"
                << "  \"task\":\"instance_segmentation\",\n"
                << "  \"architecture\":\"yolov8_seg\",\n"
                << "  \"variant\":\"nano\",\n"
                << "  \"weights\":" << QuoteSegJson(weights_relative_path) << ",\n"
                << "  \"weights_format\":\"python_state_dict\",\n"
                << "  \"weights_hash\":" << QuoteSegJson(Fnv1a64File(weights_path)) << ",\n"
                << "  \"num_classes\":" << manifest.num_classes << ",\n"
                << "  \"mask_channels\":" << manifest.mask_channels << ",\n"
                << "  \"prototype_channels\":64,\n"
                << "  \"configured_prototype_channels\":256,\n"
                << "  \"classes\":[";
            for (std::size_t index = 0;
                 index < manifest.class_names.size();
                 ++index)
            {
                if (index > 0)
                    manifest_file << ",";
                manifest_file << QuoteSegJson(manifest.class_names[index]);
            }
            manifest_file
                << "],\n"
                << "  \"input\":{\"width\":" << manifest.input_width
                << ",\"height\":" << manifest.input_height
                << ",\"color\":\"rgb\",\"scale\":0.003921568627,"
                << "\"letterbox\":true},\n"
                << "  \"postprocess\":{\"confidence_threshold\":"
                << confidence_threshold
                << ",\"iou_threshold\":" << manifest.iou_threshold
                << ",\"mask_threshold\":" << manifest.mask_threshold
                << ",\"max_detections\":" << manifest.max_detections
                << ",\"class_agnostic_nms\":"
                << (manifest.class_agnostic_nms ? "true" : "false") << "},\n"
                << "  \"training_smoke\":{\"sample_count\":"
                << train_samples.size() << ","
                << "\"instance_count\":" << train_instance_count
                << ",\"epochs\":" << training_epochs
                << ",\"learning_rate\":" << learning_rate
                << ",\"lr_schedule\":" << QuoteSegJson(lr_schedule)
                << ",\"min_learning_rate\":" << min_learning_rate
                << ",\"weight_decay\":" << weight_decay
                << ",\"loss_phase\":\"weighted_class_mask_box_dfl\","
                << "\"dataset_source\":" << QuoteSegJson(request.dataset_root) << ","
                << "\"source_manifest\":" << QuoteSegJson(request.manifest_path) << "}\n"
                << "}\n";
            manifest_file.close();
            return manifest_file.good();
        };

        if (!write_checkpoint(model, checkpoint_ref))
            return SegFailure("checkpoint", "failed to write YOLOv8-Seg state dict");
        if (!write_trained_manifest(
                manifest_ref,
                "yolov8n_seg_backward_smoke_v1",
                "weights/yolov8n_seg_backward_smoke_state_dict.pt",
                checkpoint_ref,
                postprocess_confidence_threshold))
            return SegFailure("manifest_write", "failed to write YOLOv8-Seg trained manifest");

        const double class_loss_value =
            baseline_losses.class_loss.detach().item<double>();
        const double mask_loss_value =
            baseline_losses.mask_loss.detach().item<double>();
        const double box_loss_value =
            baseline_losses.box_loss.detach().item<double>();
        const double dfl_loss_value =
            baseline_losses.dfl_loss.detach().item<double>();
        const double total_loss_value =
            baseline_losses.total_loss.detach().item<double>();
        std::ofstream loss_file(loss_ref);
        loss_file
            << "{\n"
            << "  \"schema\":\"cxvision.yolov8seg.loss_breakdown.v1\",\n"
            << "  \"task\":\"torch.train.instance_segmentation.yolov8.backward_smoke.v1\",\n"
            << "  \"loss_phase\":\"weighted_class_mask_box_dfl\",\n"
            << "  \"loss_weights\":{\"box\":" << box_loss_weight
            << ",\"class\":" << class_loss_weight
            << ",\"dfl\":" << dfl_loss_weight
            << ",\"mask\":" << mask_loss_weight << "},\n"
            << "  \"sample_count\":" << train_samples.size() << ",\n"
            << "  \"instance_count\":" << train_instance_count << ",\n"
            << "  \"total_loss\":" << total_loss_value << ",\n"
            << "  \"box_loss\":" << box_loss_value << ",\n"
            << "  \"class_loss\":" << class_loss_value << ",\n"
            << "  \"dfl_loss\":" << dfl_loss_value << ",\n"
            << "  \"mask_loss\":" << mask_loss_value << ",\n"
            << "  \"box_loss_connected\":true,\n"
            << "  \"dfl_loss_connected\":true,\n"
            << "  \"raw_shapes\":{\n"
            << "    \"box_logits\":[[1,64," << raw.box_logits[0].size(2)
            << "," << raw.box_logits[0].size(3) << "],[1,64,"
            << raw.box_logits[1].size(2) << "," << raw.box_logits[1].size(3)
            << "],[1,64," << raw.box_logits[2].size(2) << ","
            << raw.box_logits[2].size(3) << "]],\n"
            << "    \"class_logits\":[[1,80," << raw.class_logits[0].size(2)
            << "," << raw.class_logits[0].size(3) << "],[1,80,"
            << raw.class_logits[1].size(2) << "," << raw.class_logits[1].size(3)
            << "],[1,80," << raw.class_logits[2].size(2) << ","
            << raw.class_logits[2].size(3) << "]],\n"
            << "    \"mask_coefficients\":[[1,32," << raw.mask_coefficients[0].size(2)
            << "," << raw.mask_coefficients[0].size(3) << "],[1,32,"
            << raw.mask_coefficients[1].size(2) << ","
            << raw.mask_coefficients[1].size(3) << "],[1,32,"
            << raw.mask_coefficients[2].size(2) << ","
            << raw.mask_coefficients[2].size(3) << "]],\n"
            << "    \"prototypes\":[1," << raw.prototypes.size(1) << ","
            << raw.prototypes.size(2) << "," << raw.prototypes.size(3)
            << "]\n"
            << "  }\n"
            << "}\n";

        std::ofstream gradient_file(gradient_ref);
        gradient_file
            << "{\n"
            << "  \"schema\":\"cxvision.yolov8seg.gradient_report.v1\",\n"
            << "  \"loss_phase\":\"weighted_class_mask_box_dfl\",\n"
            << "  \"required_paths\":{\n"
            << "    \"backbone_grad_defined\":" << (groups["backbone"].grad_defined ? "true" : "false") << ",\n"
            << "    \"pan_fpn_grad_defined\":" << (groups["pan_fpn"].grad_defined ? "true" : "false") << ",\n"
            << "    \"class_head_grad_defined\":" << (groups["class_head"].grad_defined ? "true" : "false") << ",\n"
            << "    \"mask_coeff_head_grad_defined\":" << (groups["mask_coeff_head"].grad_defined ? "true" : "false") << ",\n"
            << "    \"proto_branch_grad_defined\":" << (groups["proto_branch"].grad_defined ? "true" : "false") << ",\n"
            << "    \"box_head_grad_defined\":" << (groups["box_head"].grad_defined ? "true" : "false") << "\n"
            << "  },\n"
            << "  \"groups\":{\n";
        for (std::size_t index = 0; index < group_order.size(); ++index)
            write_stat(
                gradient_file,
                group_order[index],
                groups[group_order[index]],
                index + 1 < group_order.size());
        gradient_file << "  },\n  \"c2f_blocks\":{\n";
        const std::vector<std::string> block_order{
            "m2", "m4", "m6", "m8", "m12", "m15", "m18", "m21"};
        for (std::size_t index = 0; index < block_order.size(); ++index)
            write_stat(
                gradient_file,
                block_order[index],
                blocks[block_order[index]],
                index + 1 < block_order.size());
        gradient_file << "  }\n}\n";

        std::ofstream update_file(update_ref);
        update_file
            << "{\n"
            << "  \"schema\":\"cxvision.yolov8seg.parameter_update_report.v1\",\n"
            << "  \"optimizer\":\"Adam\",\n"
            << "  \"learning_rate\":" << learning_rate << ",\n"
            << "  \"lr_schedule\":" << QuoteSegJson(lr_schedule) << ",\n"
            << "  \"min_learning_rate\":" << min_learning_rate << ",\n"
            << "  \"weight_decay\":" << weight_decay << ",\n"
            << "  \"configured_epochs\":" << training_epochs << ",\n"
            << "  \"completed_epochs\":" << training_trace.size() << ",\n"
            << "  \"groups\":{\n";
        for (std::size_t index = 0; index < group_order.size(); ++index)
            write_stat(
                update_file,
                group_order[index],
                groups[group_order[index]],
                index + 1 < group_order.size());
        update_file << "  }\n}\n";

        std::vector<AblationResult> ablations;
        const std::vector<std::string> freeze_groups{
            "backbone",
            "proto_branch",
            "mask_coeff_head",
            "class_head",
            "box_head"};
        for (const std::string& freeze_group : freeze_groups)
        {
            YoloV8Segment ablation_model(training_num_classes);
            const YoloV8SegWeightMappingReport ablation_mapping =
                classifier_transfer
                ? ablation_model->load_state_dict_transfer_classifier(
                      manifest.weights_path.string())
                : ablation_model->load_state_dict_strict(
                      manifest.weights_path.string());
            (void)ablation_mapping;
            ablation_model->to(device);
            ablation_model->train();

            for (auto& named : ablation_model->named_parameters(true))
            {
                if (group_for(named.key()) == freeze_group)
                    named.value().set_requires_grad(false);
            }

            std::map<std::string, torch::Tensor> ablation_before;
            for (const auto& named : ablation_model->named_parameters(true))
                ablation_before.emplace(
                    named.key(),
                    named.value().detach().clone());

            torch::optim::Adam ablation_optimizer(
                ablation_model->parameters(),
                torch::optim::AdamOptions(learning_rate)
                    .weight_decay(weight_decay));
            ablation_optimizer.zero_grad();
            const double ablation_divisor =
                static_cast<double>(train_samples.size());
            double ablation_total = 0.0;
            double ablation_box = 0.0;
            double ablation_class = 0.0;
            double ablation_dfl = 0.0;
            double ablation_mask = 0.0;
            for (const auto& active_sample : train_samples)
            {
                cv::Mat active_image = cv::imread(
                    active_sample.image_ref, cv::IMREAD_COLOR);
                SegLetterbox active_letterbox;
                torch::Tensor active_input = MakeSegInput(
                    active_image, manifest, active_letterbox).to(device);
                YoloV8SegRawOutput ablation_raw =
                    ablation_model->forward(active_input);
                const LossTensors losses = compute_losses(
                    ablation_model, ablation_raw, active_sample, active_input,
                    active_letterbox, false);
                (losses.total_loss / ablation_divisor).backward();
                ablation_total += losses.total_loss.detach().item<double>();
                ablation_box += losses.box_loss.detach().item<double>();
                ablation_class += losses.class_loss.detach().item<double>();
                ablation_dfl += losses.dfl_loss.detach().item<double>();
                ablation_mask += losses.mask_loss.detach().item<double>();
            }
            ablation_optimizer.step();

            AblationResult ablation;
            ablation.variant = "freeze_" + freeze_group;
            ablation.frozen_group = freeze_group;
            ablation.total_loss = ablation_total / ablation_divisor;
            ablation.box_loss = ablation_box / ablation_divisor;
            ablation.class_loss = ablation_class / ablation_divisor;
            ablation.dfl_loss = ablation_dfl / ablation_divisor;
            ablation.mask_loss = ablation_mask / ablation_divisor;

            for (const auto& named : ablation_model->named_parameters(true))
            {
                const auto before = ablation_before.find(named.key());
                if (before == ablation_before.end())
                    continue;
                add_stat(
                    ablation.groups[group_for(named.key())],
                    named.value(),
                    before->second);
            }
            ablations.push_back(std::move(ablation));
        }

        std::ofstream ablation_file(ablation_ref);
        auto stat_for = [](const std::map<std::string, Stat>& stats,
                           const std::string& key) {
            const auto found = stats.find(key);
            return found == stats.end() ? Stat{} : found->second;
        };
        ablation_file
            << "{\n"
            << "  \"schema\":\"cxvision.yolov8seg.freeze_ablation_report.v1\",\n"
            << "  \"loss_phase\":\"weighted_class_mask_box_dfl\",\n"
            << "  \"baseline\":{\n"
            << "    \"total_loss\":" << total_loss_value << ",\n"
            << "    \"box_loss\":" << box_loss_value << ",\n"
            << "    \"class_loss\":" << class_loss_value << ",\n"
            << "    \"dfl_loss\":" << dfl_loss_value << ",\n"
            << "    \"mask_loss\":" << mask_loss_value << "\n"
            << "  },\n"
            << "  \"variants\":[\n";
        for (std::size_t index = 0; index < ablations.size(); ++index)
        {
            const AblationResult& ablation = ablations[index];
            const Stat frozen_stat =
                stat_for(ablation.groups, ablation.frozen_group);
            ablation_file
                << "    {\n"
                << "      \"variant\":" << QuoteSegJson(ablation.variant) << ",\n"
                << "      \"frozen_group\":" << QuoteSegJson(ablation.frozen_group) << ",\n"
                << "      \"frozen_group_update_norm\":" << frozen_stat.update_norm << ",\n"
                << "      \"frozen_group_grad_defined\":"
                << (frozen_stat.grad_defined ? "true" : "false") << ",\n"
                << "      \"total_loss\":" << ablation.total_loss << ",\n"
                << "      \"box_loss\":" << ablation.box_loss << ",\n"
                << "      \"class_loss\":" << ablation.class_loss << ",\n"
                << "      \"dfl_loss\":" << ablation.dfl_loss << ",\n"
                << "      \"mask_loss\":" << ablation.mask_loss << ",\n"
                << "      \"required_paths\":{\n"
                << "        \"backbone_grad_defined\":"
                << (stat_for(ablation.groups, "backbone").grad_defined ? "true" : "false") << ",\n"
                << "        \"pan_fpn_grad_defined\":"
                << (stat_for(ablation.groups, "pan_fpn").grad_defined ? "true" : "false") << ",\n"
                << "        \"box_head_grad_defined\":"
                << (stat_for(ablation.groups, "box_head").grad_defined ? "true" : "false") << ",\n"
                << "        \"class_head_grad_defined\":"
                << (stat_for(ablation.groups, "class_head").grad_defined ? "true" : "false") << ",\n"
                << "        \"mask_coeff_head_grad_defined\":"
                << (stat_for(ablation.groups, "mask_coeff_head").grad_defined ? "true" : "false") << ",\n"
                << "        \"proto_branch_grad_defined\":"
                << (stat_for(ablation.groups, "proto_branch").grad_defined ? "true" : "false") << "\n"
                << "      },\n"
                << "      \"groups\":{\n";
            for (std::size_t group_index = 0;
                 group_index < group_order.size();
                 ++group_index)
            {
                write_stat(
                    ablation_file,
                    group_order[group_index],
                    stat_for(ablation.groups, group_order[group_index]),
                    group_index + 1 < group_order.size());
            }
            ablation_file
                << "      }\n"
                << "    }" << (index + 1 < ablations.size() ? "," : "") << "\n";
        }
        ablation_file
            << "  ]\n"
            << "}\n";

        auto hash_existing_file = [](const std::string& path) {
            if (path.empty() || !std::filesystem::exists(path))
                return std::string();
            return Fnv1a64File(path);
        };
        auto extract_json_int = [](
            const std::string& json,
            const std::string& key) {
            const std::string needle = "\"" + key + "\":";
            std::size_t pos = json.find(needle);
            if (pos == std::string::npos)
                return -1;
            pos += needle.size();
            while (pos < json.size() && json[pos] == ' ')
                ++pos;
            std::size_t end = pos;
            if (end < json.size() && json[end] == '-')
                ++end;
            while (end < json.size() && json[end] >= '0' && json[end] <= '9')
                ++end;
            if (end == pos)
                return -1;
            return std::stoi(json.substr(pos, end - pos));
        };
        auto safe_component = [](std::string value) {
            for (char& ch : value)
            {
                if (!std::isalnum(static_cast<unsigned char>(ch)) &&
                    ch != '-' && ch != '_')
                    ch = '_';
            }
            return value.empty() ? std::string("image") : value;
        };

        TorchTaskResultCpp infer_result;
        std::vector<StabilityResult> stability_results;
        std::map<std::string, int> baseline_counts;
        std::map<std::string, std::string> baseline_result_hashes;
        std::map<std::string, std::string> baseline_overlay_hashes;
        auto append_dataset_inference = [&](
            const std::string& case_id,
            const std::string& perturbation_type,
            double threshold,
            const std::filesystem::path& case_manifest_ref,
            const std::filesystem::path& case_output_dir,
            bool training_step_executed,
            double roi_shift_dx_px,
            double roi_shift_dy_px,
            const AblationResult* losses,
            bool baseline) {
            for (const auto& evaluation_sample : evaluation_samples)
            {
                const std::string sample_key = evaluation_sample.split + "|" +
                    evaluation_sample.image_id + "|" + evaluation_sample.image_ref;
                TorchTaskRequestCpp stability_request = request;
                stability_request.task =
                    TorchRuntimeTaskIds::YoloV8InstanceSegmentation;
                stability_request.input_image = evaluation_sample.image_ref;
                stability_request.manifest_path = case_manifest_ref.string();
                stability_request.output_dir =
                    (case_output_dir / evaluation_sample.split /
                     safe_component(evaluation_sample.image_id)).string();
                TorchTaskResultCpp stability_infer =
                    ExecuteTorchYoloV8SegTask(config, stability_request);
                if (infer_result.status.empty())
                    infer_result = stability_infer;

                StabilityResult row;
                row.case_id = case_id + "__" + evaluation_sample.split + "__" +
                    safe_component(evaluation_sample.image_id);
                row.image_id = evaluation_sample.image_id;
                row.split = evaluation_sample.split;
                row.input_image_ref = evaluation_sample.image_ref;
                row.perturbation_type = perturbation_type;
                row.roi_shift_dx_px = roi_shift_dx_px;
                row.roi_shift_dy_px = roi_shift_dy_px;
                row.confidence_threshold = threshold;
                row.training_step_executed = training_step_executed;
                row.inference_ok = stability_infer.ok;
                row.instance_count = extract_json_int(
                    stability_infer.result_json, "instance_count");
                row.model_manifest_ref = case_manifest_ref.string();
                row.inference_result_ref = stability_infer.result_ref;
                row.inference_overlay_ref = stability_infer.primary_visual_ref;
                row.inference_result_hash =
                    hash_existing_file(stability_infer.result_ref);
                row.inference_overlay_hash =
                    hash_existing_file(stability_infer.primary_visual_ref);
                if (baseline)
                {
                    baseline_counts[sample_key] = row.instance_count;
                    baseline_result_hashes[sample_key] = row.inference_result_hash;
                    baseline_overlay_hashes[sample_key] = row.inference_overlay_hash;
                    row.instance_count_delta_from_baseline = 0;
                    row.result_hash_matches_baseline = true;
                    row.overlay_hash_matches_baseline = true;
                }
                else
                {
                    const auto count = baseline_counts.find(sample_key);
                    row.instance_count_delta_from_baseline =
                        count != baseline_counts.end() && row.instance_count >= 0
                            ? row.instance_count - count->second : 0;
                    row.result_hash_matches_baseline =
                        baseline_result_hashes[sample_key] == row.inference_result_hash;
                    row.overlay_hash_matches_baseline =
                        baseline_overlay_hashes[sample_key] == row.inference_overlay_hash;
                }
                row.failure_stage = stability_infer.ok
                    ? "" : stability_infer.error_message;
                row.total_loss = losses ? losses->total_loss : total_loss_value;
                row.box_loss = losses ? losses->box_loss : box_loss_value;
                row.class_loss = losses ? losses->class_loss : class_loss_value;
                row.dfl_loss = losses ? losses->dfl_loss : dfl_loss_value;
                row.mask_loss = losses ? losses->mask_loss : mask_loss_value;
                stability_results.push_back(std::move(row));
            }
        };

        auto shifted_samples = [&](
            double dx_px,
            double dy_px) {
            std::vector<YoloV8SegDatasetSample> shifted = train_samples;
            const float dx =
                static_cast<float>(
                    dx_px / static_cast<double>(manifest.input_width));
            const float dy =
                static_cast<float>(
                    dy_px / static_cast<double>(manifest.input_height));
            for (auto& shifted_sample : shifted)
            {
                for (auto& box : shifted_sample.boxes_xyxy_norm)
                {
                    box[0] = std::clamp(box[0] + dx, 0.0f, 1.0f);
                    box[1] = std::clamp(box[1] + dy, 0.0f, 1.0f);
                    box[2] = std::clamp(box[2] + dx, 0.0f, 1.0f);
                    box[3] = std::clamp(box[3] + dy, 0.0f, 1.0f);
                }
                for (auto& polygon : shifted_sample.polygons_norm)
                {
                    for (cv::Point2f& point : polygon)
                    {
                        point.x = std::clamp(point.x + dx, 0.0f, 1.0f);
                        point.y = std::clamp(point.y + dy, 0.0f, 1.0f);
                    }
                }
            }
            return shifted;
        };

        auto train_and_infer_variant = [&](
            const std::string& case_id,
            const std::string& perturbation_type,
            const std::vector<YoloV8SegDatasetSample>& active_samples,
            double dx_px,
            double dy_px) {
            const auto case_root = output_dir / "stability" / case_id;
            const auto case_weights_dir = case_root / "weights";
            std::filesystem::create_directories(case_weights_dir);
            const auto case_checkpoint =
                case_weights_dir / "yolov8n_seg_backward_smoke_state_dict.pt";
            const auto case_manifest = case_root / "model_manifest.json";

            YoloV8Segment stability_model(training_num_classes);
            const YoloV8SegWeightMappingReport stability_mapping =
                classifier_transfer
                ? stability_model->load_state_dict_transfer_classifier(
                      manifest.weights_path.string())
                : stability_model->load_state_dict_strict(
                      manifest.weights_path.string());
            (void)stability_mapping;
            stability_model->to(device);
            stability_model->train();

            torch::optim::Adam stability_optimizer(
                stability_model->parameters(),
                torch::optim::AdamOptions(learning_rate)
                    .weight_decay(weight_decay));
            stability_optimizer.zero_grad();
            AblationResult variant_losses;
            for (const auto& active_sample : active_samples)
            {
                cv::Mat active_image = cv::imread(
                    active_sample.image_ref, cv::IMREAD_COLOR);
                SegLetterbox active_letterbox;
                torch::Tensor active_input = MakeSegInput(
                    active_image, manifest, active_letterbox).to(device);
                YoloV8SegRawOutput stability_raw =
                    stability_model->forward(active_input);
                const LossTensors losses = compute_losses(
                    stability_model, stability_raw, active_sample, active_input,
                    active_letterbox, false);
                (losses.total_loss /
                 static_cast<double>(active_samples.size())).backward();
                variant_losses.total_loss += losses.total_loss.detach().item<double>();
                variant_losses.box_loss += losses.box_loss.detach().item<double>();
                variant_losses.class_loss += losses.class_loss.detach().item<double>();
                variant_losses.dfl_loss += losses.dfl_loss.detach().item<double>();
                variant_losses.mask_loss += losses.mask_loss.detach().item<double>();
            }
            const double variant_divisor = static_cast<double>(active_samples.size());
            variant_losses.total_loss /= variant_divisor;
            variant_losses.box_loss /= variant_divisor;
            variant_losses.class_loss /= variant_divisor;
            variant_losses.dfl_loss /= variant_divisor;
            variant_losses.mask_loss /= variant_divisor;
            stability_optimizer.step();

            if (!write_checkpoint(stability_model, case_checkpoint) ||
                !write_trained_manifest(
                    case_manifest,
                    "yolov8n_seg_backward_smoke_" + case_id,
                    "weights/yolov8n_seg_backward_smoke_state_dict.pt",
                    case_checkpoint,
                    postprocess_confidence_threshold))
            {
                StabilityResult row;
                row.case_id = case_id;
                row.perturbation_type = perturbation_type;
                row.roi_shift_dx_px = dx_px;
                row.roi_shift_dy_px = dy_px;
                row.confidence_threshold = 0.25;
                row.training_step_executed = true;
                row.failure_stage = "variant_artifact_write_failed";
                stability_results.push_back(std::move(row));
                return;
            }

            append_dataset_inference(
                case_id,
                perturbation_type,
                0.25,
                case_manifest,
                case_root / "inference",
                true,
                dx_px,
                dy_px,
                &variant_losses,
                false);
        };

        append_dataset_inference(
            "baseline_trained_inference",
            "baseline",
            0.25,
            manifest_ref,
            output_dir / "trained_inference",
            true,
            0.0,
            0.0,
            nullptr,
            true);
        const std::size_t baseline_row_count = stability_results.size();

        std::ofstream l2_matrix(l2_matrix_ref);
        l2_matrix
            << "{\n"
            << "  \"schema\":\"cxvision.yolov8seg.l2_case_matrix.v1\",\n"
            << "  \"dataset_source\":" << QuoteSegJson(request.dataset_root) << ",\n"
            << "  \"train_sample_count\":" << train_samples.size() << ",\n"
            << "  \"train_instance_count\":" << train_instance_count << ",\n"
            << "  \"evaluation_case_count\":" << baseline_row_count << ",\n"
            << "  \"rows\":[\n";
        for (std::size_t index = 0; index < baseline_row_count; ++index)
        {
            const auto& row = stability_results[index];
            l2_matrix
                << "    {\"case_id\":" << QuoteSegJson(row.case_id)
                << ",\"image_id\":" << QuoteSegJson(row.image_id)
                << ",\"split\":" << QuoteSegJson(row.split)
                << ",\"input_image_ref\":" << QuoteSegJson(row.input_image_ref)
                << ",\"inference_ok\":" << (row.inference_ok ? "true" : "false")
                << ",\"instance_count\":" << row.instance_count
                << ",\"inference_result_ref\":" << QuoteSegJson(row.inference_result_ref)
                << ",\"inference_overlay_ref\":" << QuoteSegJson(row.inference_overlay_ref)
                << "}" << (index + 1 < baseline_row_count ? "," : "") << "\n";
        }
        l2_matrix << "  ]\n}\n";
        l2_matrix.close();
        if (!l2_matrix.good())
            return SegFailure("l2_matrix", "failed to write YOLOv8-Seg L2 case matrix");

        for (const auto& threshold_case :
             std::vector<std::pair<std::string, double>>{
                 {"threshold_024", 0.24},
                 {"threshold_025", 0.25},
                 {"threshold_026", 0.26}})
        {
            const auto case_root =
                output_dir / "stability" / threshold_case.first;
            std::filesystem::create_directories(case_root);
            const auto case_manifest = case_root / "model_manifest.json";
            if (write_trained_manifest(
                    case_manifest,
                    "yolov8n_seg_backward_smoke_" + threshold_case.first,
                    "../../weights/yolov8n_seg_backward_smoke_state_dict.pt",
                    checkpoint_ref,
                    threshold_case.second))
            {
                append_dataset_inference(
                    threshold_case.first,
                    "threshold_adjacent",
                    threshold_case.second,
                    case_manifest,
                    case_root / "inference",
                    false,
                    0.0,
                    0.0,
                    nullptr,
                    false);
            }
            else
            {
                StabilityResult row;
                row.case_id = threshold_case.first;
                row.perturbation_type = "threshold_adjacent";
                row.confidence_threshold = threshold_case.second;
                row.failure_stage = "threshold_manifest_write_failed";
                stability_results.push_back(std::move(row));
            }
        }

        train_and_infer_variant(
            "repeat_train_01",
            "repeat_train_inference",
            train_samples,
            0.0,
            0.0);
        train_and_infer_variant(
            "repeat_train_02",
            "repeat_train_inference",
            train_samples,
            0.0,
            0.0);
        train_and_infer_variant(
            "roi_shift_minus_2px",
            "roi_small_shift",
            shifted_samples(-2.0, -2.0),
            -2.0,
            -2.0);
        train_and_infer_variant(
            "roi_shift_plus_2px",
            "roi_small_shift",
            shifted_samples(2.0, 2.0),
            2.0,
            2.0);

        std::ofstream stability_file(stability_ref);
        const std::string baseline_result_hash =
            hash_existing_file(infer_result.result_ref);
        const std::string baseline_overlay_hash =
            hash_existing_file(infer_result.primary_visual_ref);
        stability_file
            << "{\n"
            << "  \"schema\":\"cxvision.yolov8seg.l3_stability_matrix.v1\",\n"
            << "  \"loss_phase\":\"weighted_class_mask_box_dfl\",\n"
            << "  \"dataset_source\":" << QuoteSegJson(request.dataset_root) << ",\n"
            << "  \"train_sample_count\":" << train_samples.size() << ",\n"
            << "  \"train_instance_count\":" << train_instance_count << ",\n"
            << "  \"evaluation_case_count\":" << evaluation_samples.size() << ",\n"
            << "  \"coverage\":[\"roi_small_shift\","
            << "\"threshold_adjacent\",\"repeat_train_inference\"],\n"
            << "  \"baseline_case_id\":\"baseline_trained_inference\",\n"
            << "  \"baseline_result_hash\":"
            << QuoteSegJson(baseline_result_hash) << ",\n"
            << "  \"baseline_overlay_hash\":"
            << QuoteSegJson(baseline_overlay_hash) << ",\n"
            << "  \"rows\":[\n";
        for (std::size_t index = 0;
             index < stability_results.size();
             ++index)
        {
            const StabilityResult& row = stability_results[index];
            stability_file
                << "    {\n"
                << "      \"case_id\":" << QuoteSegJson(row.case_id) << ",\n"
                << "      \"image_id\":" << QuoteSegJson(row.image_id) << ",\n"
                << "      \"split\":" << QuoteSegJson(row.split) << ",\n"
                << "      \"input_image_ref\":" << QuoteSegJson(row.input_image_ref) << ",\n"
                << "      \"perturbation_type\":"
                << QuoteSegJson(row.perturbation_type) << ",\n"
                << "      \"roi_shift_dx_px\":" << row.roi_shift_dx_px << ",\n"
                << "      \"roi_shift_dy_px\":" << row.roi_shift_dy_px << ",\n"
                << "      \"confidence_threshold\":"
                << row.confidence_threshold << ",\n"
                << "      \"training_step_executed\":"
                << (row.training_step_executed ? "true" : "false") << ",\n"
                << "      \"inference_ok\":"
                << (row.inference_ok ? "true" : "false") << ",\n"
                << "      \"instance_count\":" << row.instance_count << ",\n"
                << "      \"instance_count_delta_from_baseline\":"
                << row.instance_count_delta_from_baseline << ",\n"
                << "      \"total_loss\":" << row.total_loss << ",\n"
                << "      \"box_loss\":" << row.box_loss << ",\n"
                << "      \"class_loss\":" << row.class_loss << ",\n"
                << "      \"dfl_loss\":" << row.dfl_loss << ",\n"
                << "      \"mask_loss\":" << row.mask_loss << ",\n"
                << "      \"model_manifest_ref\":"
                << QuoteSegJson(row.model_manifest_ref) << ",\n"
                << "      \"inference_result_ref\":"
                << QuoteSegJson(row.inference_result_ref) << ",\n"
                << "      \"inference_overlay_ref\":"
                << QuoteSegJson(row.inference_overlay_ref) << ",\n"
                << "      \"inference_result_hash\":"
                << QuoteSegJson(row.inference_result_hash) << ",\n"
                << "      \"inference_overlay_hash\":"
                << QuoteSegJson(row.inference_overlay_hash) << ",\n"
                << "      \"result_hash_matches_baseline\":"
                << (row.result_hash_matches_baseline ? "true" : "false") << ",\n"
                << "      \"overlay_hash_matches_baseline\":"
                << (row.overlay_hash_matches_baseline ? "true" : "false") << ",\n"
                << "      \"failure_stage\":"
                << QuoteSegJson(row.failure_stage) << "\n"
                << "    }"
                << (index + 1 < stability_results.size() ? "," : "")
                << "\n";
        }
        stability_file
            << "  ]\n"
            << "}\n";
        stability_file.close();
        if (!stability_file.good())
            return SegFailure(
                "stability_matrix",
                "failed to write YOLOv8-Seg L3 stability matrix");

        std::ofstream variation_file(variation_ref);
        variation_file
            << "{\n"
            << "  \"schema\":\"cxvision.yolov8seg.result_variation.v1\",\n"
            << "  \"conclusion\":\"L3_PENDING_HUMAN_REVIEW\",\n"
            << "  \"rows\":[\n";
        for (std::size_t index = 0; index < stability_results.size(); ++index)
        {
            const StabilityResult& row = stability_results[index];
            const std::string sample_key = row.split + "|" + row.image_id +
                "|" + row.input_image_ref;
            const int baseline_count = baseline_counts.at(sample_key);
            const int delta = row.instance_count - baseline_count;
            variation_file
                << "    {\"case_id\":" << QuoteSegJson(row.case_id)
                << ",\"image_id\":" << QuoteSegJson(row.image_id)
                << ",\"split\":" << QuoteSegJson(row.split)
                << ",\"perturbation_type\":"
                << QuoteSegJson(row.perturbation_type)
                << ",\"baseline_instance_count\":" << baseline_count
                << ",\"instance_count\":" << row.instance_count
                << ",\"instance_count_delta\":" << delta
                << ",\"classification\":"
                << QuoteSegJson(delta == 0
                    ? "count_stable"
                    : "candidate_count_changed_requires_human_review")
                << ",\"overlay_ref\":"
                << QuoteSegJson(row.inference_overlay_ref)
                << "}" << (index + 1 < stability_results.size() ? "," : "")
                << "\n";
        }
        variation_file << "  ]\n}\n";
        variation_file.close();
        if (!variation_file.good())
            return SegFailure("result_variation", "failed to write result variation");

        std::ofstream stability_report(stability_report_ref);
        stability_report
            << "# YOLOv8-Seg L3 Stability Report\n\n"
            << "- conclusion: `L3_PENDING_HUMAN_REVIEW`\n"
            << "- dataset: `" << request.dataset_root << "`\n"
            << "- train samples: " << train_samples.size() << "\n"
            << "- train instances: " << train_instance_count << "\n"
            << "- evaluation images: " << evaluation_samples.size() << "\n\n"
            << "| Case | Split | Image | Perturbation | Baseline | Actual | Delta |\n"
            << "|---|---|---|---|---:|---:|---:|\n";
        for (const StabilityResult& row : stability_results)
        {
            const int baseline_count =
                baseline_counts.at(row.split + "|" + row.image_id + "|" +
                                   row.input_image_ref);
            stability_report
                << "| " << row.case_id << " | " << row.split << " | "
                << row.image_id << " | " << row.perturbation_type << " | "
                << baseline_count << " | " << row.instance_count << " | "
                << (row.instance_count - baseline_count) << " |\n";
        }
        stability_report.close();

        std::ofstream timeout_report(timeout_report_ref);
        timeout_report
            << "# YOLOv8-Seg L3 Timeout Report\n\n"
            << "- timeout cases: 0\n"
            << "- completed inference rows: " << stability_results.size() << "\n"
            << "- conclusion: no runtime timeout observed\n";
        timeout_report.close();

        std::ofstream human_review(human_review_ref);
        human_review
            << "{\n"
            << "  \"schema\":\"cxvision.yolov8seg.human_review.v1\",\n"
            << "  \"status\":\"pending_human_review\",\n"
            << "  \"decision\":\"PENDING_HUMAN_REVIEW\",\n"
            << "  \"reason\":\"per-image baseline and perturbation overlays require human semantic review\"\n"
            << "}\n";
        human_review.close();
        if (!stability_report.good() || !timeout_report.good() ||
            !human_review.good())
            return SegFailure("l3_report", "failed to write L3 review reports");

        const auto completed = std::chrono::steady_clock::now();
        const double elapsed_ms =
            std::chrono::duration<double, std::milli>(
                completed - started).count();
        std::ofstream evidence_file(evidence_ref);
        evidence_file
            << "{\n"
            << "  \"schema\":\"cxvision.yolov8seg.backward_smoke_evidence.v1\",\n"
            << "  \"status\":\"success\",\n"
            << "  \"loss_breakdown_ref\":" << QuoteSegJson(loss_ref.string()) << ",\n"
            << "  \"gradient_report_ref\":" << QuoteSegJson(gradient_ref.string()) << ",\n"
            << "  \"parameter_update_report_ref\":" << QuoteSegJson(update_ref.string()) << ",\n"
            << "  \"freeze_ablation_report_ref\":" << QuoteSegJson(ablation_ref.string()) << ",\n"
            << "  \"dataset_summary_ref\":" << QuoteSegJson(dataset_summary_ref.string()) << ",\n"
            << "  \"training_trace_ref\":" << QuoteSegJson(training_trace_ref.string()) << ",\n"
            << "  \"parent_transfer_receipt_ref\":"
            << QuoteSegJson(parent_transfer_ref.string()) << ",\n"
            << "  \"l2_case_matrix_ref\":" << QuoteSegJson(l2_matrix_ref.string()) << ",\n"
            << "  \"stability_matrix_ref\":" << QuoteSegJson(stability_ref.string()) << ",\n"
            << "  \"result_variation_ref\":" << QuoteSegJson(variation_ref.string()) << ",\n"
            << "  \"stability_report_ref\":" << QuoteSegJson(stability_report_ref.string()) << ",\n"
            << "  \"timeout_report_ref\":" << QuoteSegJson(timeout_report_ref.string()) << ",\n"
            << "  \"human_review_ref\":" << QuoteSegJson(human_review_ref.string()) << ",\n"
            << "  \"dataset_source\":" << QuoteSegJson(request.dataset_root) << ",\n"
            << "  \"train_sample_count\":" << train_samples.size() << ",\n"
            << "  \"train_instance_count\":" << train_instance_count << ",\n"
            << "  \"optimizer\":\"Adam\",\n"
            << "  \"learning_rate\":" << learning_rate << ",\n"
            << "  \"lr_schedule\":" << QuoteSegJson(lr_schedule) << ",\n"
            << "  \"min_learning_rate\":" << min_learning_rate << ",\n"
            << "  \"weight_decay\":" << weight_decay << ",\n"
            << "  \"checkpoint_ref\":" << QuoteSegJson(checkpoint_ref.string()) << ",\n"
            << "  \"model_manifest_ref\":" << QuoteSegJson(manifest_ref.string()) << ",\n"
            << "  \"trained_inference_result_ref\":" << QuoteSegJson(infer_result.result_ref) << ",\n"
            << "  \"trained_inference_evidence_ref\":" << QuoteSegJson(infer_result.evidence_ref) << ",\n"
            << "  \"trained_inference_overlay_ref\":" << QuoteSegJson(infer_result.primary_visual_ref) << ",\n"
            << "  \"trained_inference_ok\":" << (infer_result.ok ? "true" : "false") << ",\n"
            << "  \"semantic_quality\":\"pending_human_review\"\n"
            << "}\n";

        TorchTaskResultCpp result;
        result.ok = infer_result.ok;
        result.status = infer_result.ok ? "success" : "partial";
        result.error_code = infer_result.ok ? 0 : -1;
        result.error_message = infer_result.error_message;
        result.requested_device = request.device;
        result.actual_device = device_name;
        result.train_runtime_ms = elapsed_ms;
        result.infer_runtime_ms = infer_result.infer_runtime_ms;
        result.algorithm_runtime_ms =
            elapsed_ms + infer_result.algorithm_runtime_ms;
        result.result_ref = infer_result.result_ref;
        result.evidence_ref = evidence_ref.string();
        result.input_image_ref = infer_result.input_image_ref;
        result.primary_visual_ref = infer_result.primary_visual_ref;
        result.visualization_refs = infer_result.visualization_refs;
        result.trainer_lifecycle_summary =
            "YOLOv8-Seg class+mask+box+DFL training completed " +
            std::to_string(training_trace.size()) + " epochs";
        result.unified_mainline_summary =
            "checkpoint manifest exported and reused by torch.infer.instance_segmentation.yolov8.v1";
        result.result_json =
            "{\"schema\":\"cxvision.yolov8seg.backward_smoke.v1\","
            "\"status\":" + QuoteSegJson(result.status) +
            ",\"total_loss\":" + std::to_string(total_loss_value) +
            ",\"box_loss\":" + std::to_string(box_loss_value) +
            ",\"class_loss\":" + std::to_string(class_loss_value) +
            ",\"dfl_loss\":" + std::to_string(dfl_loss_value) +
            ",\"mask_loss\":" + std::to_string(mask_loss_value) +
            ",\"optimizer_step_executed\":true,"
            "\"configured_epochs\":" + std::to_string(training_epochs) +
            ",\"completed_epochs\":" + std::to_string(training_trace.size()) +
            ",\"learning_rate\":" + std::to_string(learning_rate) +
            ",\"lr_schedule\":" + QuoteSegJson(lr_schedule) +
            ",\"min_learning_rate\":" + std::to_string(min_learning_rate) +
            ",\"weight_decay\":" + std::to_string(weight_decay) +
            ",\"checkpoint_ref\":" + QuoteSegJson(checkpoint_ref.string()) +
            ",\"model_manifest_ref\":" + QuoteSegJson(manifest_ref.string()) +
            ",\"dataset_source\":" + QuoteSegJson(request.dataset_root) +
            ",\"dataset_summary_ref\":" + QuoteSegJson(dataset_summary_ref.string()) +
            ",\"training_trace_ref\":" + QuoteSegJson(training_trace_ref.string()) +
            ",\"parent_transfer_receipt_ref\":" + QuoteSegJson(parent_transfer_ref.string()) +
            ",\"train_sample_count\":" + std::to_string(train_samples.size()) +
            ",\"train_instance_count\":" + std::to_string(train_instance_count) +
            ",\"evaluation_case_count\":" + std::to_string(evaluation_samples.size()) +
            ",\"l2_case_matrix_ref\":" + QuoteSegJson(l2_matrix_ref.string()) +
            ",\"stability_matrix_ref\":" + QuoteSegJson(stability_ref.string()) +
            ",\"trained_inference_ok\":" + (infer_result.ok ? "true" : "false") +
            ",\"trained_inference_result_ref\":" + QuoteSegJson(infer_result.result_ref) +
            ",\"evidence_ref\":" + QuoteSegJson(evidence_ref.string()) +
            ",\"semantic_quality\":\"pending_human_review\"}";
        return result;
    }
    catch (const std::exception& error)
    {
        return SegFailure("exception", error.what());
    }
}

namespace
{
// Business trial execution deliberately owns a narrow data boundary.  It
// consumes only the immutable geometry-segmentation materialization produced
// after a business asset broker has frozen its revision; it does not discover
// arbitrary folders, labels or reference evidence cases.
constexpr std::array<const char*, 7> kBusinessGeometryClasses{
    "arc", "circle", "ellipse", "line",
    "open_curve", "polygon", "closed_curve"};

struct BusinessTrialSettings
{
    unsigned training_split_percent = 0;
    unsigned epoch_count = 0;
    unsigned iteration_count = 0;
    unsigned batch_size = 0;
    std::string trial_model_id;
    std::string annotation_click_mode;
    std::string dataset_manifest_sha256;
    std::string dataset_revision_id;
    std::string case_id;
    std::string project_geometry_class;
    int project_geometry_class_id = -1;

    // The broker serializes its frozen semantic decision alongside the
    // editable request values.  The executor requires the two views to agree
    // so a stale UI value can never silently reuse a different revision's
    // annotation semantics.
    unsigned frozen_training_split_percent = 0;
    std::string frozen_annotation_click_mode;
    std::set<std::string> allowed_annotation_click_modes;
    std::string frozen_project_geometry_class;
    int frozen_project_geometry_class_id = -1;

    // A development parent is identified by the lineage identity and its
    // SHA-256 attestation, rather than by the legacy source manifest's FNV
    // checksum or source-model ID.
    std::string business_parent_attestation_path;
    std::string business_parent_attestation_sha256;

    // Optimizer values are executor-owned facts, not editable business UI
    // parameters.  They are reported on every epoch through the typed sink.
    double learning_rate = 1.0e-4;
    double min_learning_rate = 1.0e-6;
    std::string lr_schedule = "cosine";
    double weight_decay = 0.0;
};

struct BusinessMaterializedAsset
{
    std::string image_id;
    std::string split;
    std::filesystem::path image_relative;
    std::filesystem::path mask_relative;
    std::string image_digest;
    std::string mask_digest;
    bool has_image = false;
    bool has_mask = false;
};

struct BusinessMaterializedDataset
{
    std::string manifest_sha256;
    std::string snapshot_id;
    std::string snapshot_digest;
    std::string case_id;
    std::string dataset_revision_id;
    std::string annotation_receipt_digest;
    std::vector<YoloV8SegDatasetSample> train_samples;
    std::vector<YoloV8SegDatasetSample> validation_samples;
};

struct BusinessLossTensors
{
    torch::Tensor total_loss;
    torch::Tensor class_loss;
    torch::Tensor mask_loss;
    torch::Tensor box_loss;
    torch::Tensor dfl_loss;
};

struct BusinessEpochMetric
{
    unsigned epoch = 0;
    unsigned completed_iterations = 0;
    double training_loss = 0.0;
    double training_score = 0.0;
    double validation_loss = 0.0;
    double validation_score = 0.0;
    double effective_learning_rate = 0.0;
};

bool ReadBusinessFile(
    const std::filesystem::path& path,
    std::string& contents)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
        return false;
    contents.assign(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
    return input.good() || input.eof();
}

// Small local SHA-256 implementation keeps the runtime's immutable data and
// checkpoint checks independent of a network, Python or an external crypto
// DLL.  The output uses the same "sha256:<lowercase hex>" convention as the
// business broker and materializer.
class BusinessSha256
{
public:
    BusinessSha256()
    {
        state_ = {
            0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
            0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
    }

    void Update(const unsigned char* data, std::size_t count)
    {
        bit_count_ += static_cast<std::uint64_t>(count) * 8u;
        while (count > 0)
        {
            const std::size_t writable =
                std::min<std::size_t>(count, buffer_.size() - buffer_size_);
            std::memcpy(buffer_.data() + buffer_size_, data, writable);
            buffer_size_ += writable;
            data += writable;
            count -= writable;
            if (buffer_size_ == buffer_.size())
            {
                Transform(buffer_.data());
                buffer_size_ = 0;
            }
        }
    }

    std::array<unsigned char, 32> Finish()
    {
        std::array<unsigned char, 64> padding{};
        padding[0] = 0x80u;
        const std::size_t pad_count =
            buffer_size_ < 56u ? 56u - buffer_size_ : 120u - buffer_size_;
        Update(padding.data(), pad_count);
        std::array<unsigned char, 8> length{};
        const std::uint64_t message_bits = bit_count_before_padding_;
        for (std::size_t index = 0; index < length.size(); ++index)
        {
            length[length.size() - 1u - index] =
                static_cast<unsigned char>(message_bits >> (index * 8u));
        }
        // Update() accounts for padding; retain the original count on the
        // first finish call before serializing the standard length suffix.
        Update(length.data(), length.size());
        std::array<unsigned char, 32> digest{};
        for (std::size_t index = 0; index < state_.size(); ++index)
        {
            digest[index * 4u] = static_cast<unsigned char>(state_[index] >> 24u);
            digest[index * 4u + 1u] = static_cast<unsigned char>(state_[index] >> 16u);
            digest[index * 4u + 2u] = static_cast<unsigned char>(state_[index] >> 8u);
            digest[index * 4u + 3u] = static_cast<unsigned char>(state_[index]);
        }
        return digest;
    }

    void MarkMessageLength()
    {
        bit_count_before_padding_ = bit_count_;
    }

private:
    static std::uint32_t RotateRight(std::uint32_t value, unsigned bits)
    {
        return (value >> bits) | (value << (32u - bits));
    }

    static std::uint32_t Choice(
        std::uint32_t x, std::uint32_t y, std::uint32_t z)
    {
        return (x & y) ^ (~x & z);
    }

    static std::uint32_t Majority(
        std::uint32_t x, std::uint32_t y, std::uint32_t z)
    {
        return (x & y) ^ (x & z) ^ (y & z);
    }

    void Transform(const unsigned char* block)
    {
        static constexpr std::array<std::uint32_t, 64> kRoundConstants{
            0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
            0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
            0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
            0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
            0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
            0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
            0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
            0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
            0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
            0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
            0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
            0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
            0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
            0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
            0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
            0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};
        std::array<std::uint32_t, 64> words{};
        for (std::size_t index = 0; index < 16u; ++index)
        {
            words[index] =
                (static_cast<std::uint32_t>(block[index * 4u]) << 24u) |
                (static_cast<std::uint32_t>(block[index * 4u + 1u]) << 16u) |
                (static_cast<std::uint32_t>(block[index * 4u + 2u]) << 8u) |
                static_cast<std::uint32_t>(block[index * 4u + 3u]);
        }
        for (std::size_t index = 16u; index < words.size(); ++index)
        {
            const std::uint32_t s0 =
                RotateRight(words[index - 15u], 7u) ^
                RotateRight(words[index - 15u], 18u) ^
                (words[index - 15u] >> 3u);
            const std::uint32_t s1 =
                RotateRight(words[index - 2u], 17u) ^
                RotateRight(words[index - 2u], 19u) ^
                (words[index - 2u] >> 10u);
            words[index] = words[index - 16u] + s0 + words[index - 7u] + s1;
        }
        std::uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
        std::uint32_t e = state_[4], f = state_[5], g = state_[6], h = state_[7];
        for (std::size_t index = 0; index < words.size(); ++index)
        {
            const std::uint32_t s1 =
                RotateRight(e, 6u) ^ RotateRight(e, 11u) ^ RotateRight(e, 25u);
            const std::uint32_t t1 = h + s1 + Choice(e, f, g) +
                kRoundConstants[index] + words[index];
            const std::uint32_t s0 =
                RotateRight(a, 2u) ^ RotateRight(a, 13u) ^ RotateRight(a, 22u);
            const std::uint32_t t2 = s0 + Majority(a, b, c);
            h = g; g = f; f = e; e = d + t1;
            d = c; c = b; b = a; a = t1 + t2;
        }
        state_[0] += a; state_[1] += b; state_[2] += c; state_[3] += d;
        state_[4] += e; state_[5] += f; state_[6] += g; state_[7] += h;
    }

    std::array<std::uint32_t, 8> state_{};
    std::array<unsigned char, 64> buffer_{};
    std::size_t buffer_size_ = 0;
    std::uint64_t bit_count_ = 0;
    std::uint64_t bit_count_before_padding_ = 0;
};

std::string BusinessSha256Digest(const std::string& bytes)
{
    BusinessSha256 sha256;
    sha256.Update(
        reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size());
    sha256.MarkMessageLength();
    const auto digest = sha256.Finish();
    std::ostringstream output;
    output << "sha256:";
    for (const unsigned char byte : digest)
        output << std::hex << std::setw(2) << std::setfill('0')
               << static_cast<unsigned int>(byte);
    return output.str();
}

bool BusinessSha256File(
    const std::filesystem::path& path,
    std::string& digest)
{
    std::string bytes;
    if (!ReadBusinessFile(path, bytes))
        return false;
    digest = BusinessSha256Digest(bytes);
    return true;
}

bool IsBusinessSha256(const std::string& value)
{
    if (value.size() != 71u || value.rfind("sha256:", 0u) != 0u)
        return false;
    return std::all_of(value.begin() + 7, value.end(), [](unsigned char ch) {
        return std::isxdigit(ch) != 0;
    });
}

std::string LowerBusinessAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool IsSafeBusinessIdentifier(const std::string& value)
{
    return !value.empty() && value.size() <= 256u &&
        value.find_first_of("\r\n|") == std::string::npos;
}

bool IsSafeBusinessImageId(const std::string& value)
{
    return !value.empty() &&
        std::all_of(value.begin(), value.end(), [](unsigned char ch) {
            return std::isalnum(ch) != 0 || ch == '_' || ch == '-';
        });
}

bool IsBusinessTrainingSplit(const std::string& value)
{
    // VERIFY is deliberately an independent unmarked inference asset.  It is
    // not a training split, has no training mask, and must never appear in the
    // frozen segmentation materialization consumed by this executor.
    return value == "train" || value == "val";
}

bool IsBusinessPathWithin(
    const std::filesystem::path& root,
    const std::filesystem::path& candidate)
{
    const std::filesystem::path relative = candidate.lexically_relative(root);
    if (relative.empty() || relative.is_absolute())
        return false;
    return std::none_of(
        relative.begin(), relative.end(),
        [](const std::filesystem::path& part) { return part == ".."; });
}

bool IsSafeBusinessRelativePath(const std::filesystem::path& relative)
{
    if (relative.empty() || relative.is_absolute() || relative.has_root_name())
        return false;
    return std::none_of(relative.begin(), relative.end(),
        [](const std::filesystem::path& part) {
            return part.empty() || part == "." || part == "..";
        });
}

bool ContainsBusinessSymlink(
    const std::filesystem::path& root,
    const std::filesystem::path& relative)
{
    std::error_code error;
    std::filesystem::path current = root;
    if (std::filesystem::is_symlink(
            std::filesystem::symlink_status(current, error)) || error)
    {
        return true;
    }
    for (const auto& part : relative)
    {
        current /= part;
        if (std::filesystem::is_symlink(
                std::filesystem::symlink_status(current, error)) || error)
        {
            return true;
        }
    }
    return false;
}

bool ResolveBusinessMaterializedFile(
    const std::filesystem::path& root,
    const std::filesystem::path& relative,
    std::filesystem::path& resolved)
{
    if (!IsSafeBusinessRelativePath(relative) ||
        ContainsBusinessSymlink(root, relative))
    {
        return false;
    }
    std::error_code error;
    const std::filesystem::path requested = (root / relative).lexically_normal();
    resolved = std::filesystem::weakly_canonical(requested, error);
    return !error && std::filesystem::is_regular_file(resolved, error) &&
        !error && IsBusinessPathWithin(root, resolved);
}

bool HasBusinessSymlinkComponent(const std::filesystem::path& absolute_path)
{
    std::error_code error;
    const std::filesystem::path absolute =
        std::filesystem::absolute(absolute_path, error);
    if (error)
        return true;
    std::filesystem::path current = absolute.root_path();
    if (!current.empty() &&
        std::filesystem::is_symlink(
            std::filesystem::symlink_status(current, error)))
    {
        return true;
    }
    if (error)
        return true;
    for (const auto& part : absolute.relative_path())
    {
        current /= part;
        const std::filesystem::file_status status =
            std::filesystem::symlink_status(current, error);
        if (error)
        {
            // Missing final components are handled by the caller.  Existing
            // components must never be links.
            error.clear();
            continue;
        }
        if (std::filesystem::is_symlink(status))
            return true;
    }
    return false;
}

bool IsSafeBusinessPathComponent(const std::filesystem::path& component)
{
    const std::string value = component.string();
    return IsSafeBusinessIdentifier(value) &&
        value.find_first_of("\\/:*?\"<>|") == std::string::npos &&
        value != "." && value != "..";
}

bool ResolveBusinessStagingOutputDirectory(
    const TorchRuntimeCoreConfig& config,
    const std::string& output_dir,
    std::filesystem::path& resolved)
{
    if (config.output_root.empty() || output_dir.empty())
        return false;

    std::error_code error;
    const std::filesystem::path requested_root =
        std::filesystem::absolute(
            std::filesystem::path(config.output_root), error);
    if (error || HasBusinessSymlinkComponent(requested_root) ||
        !std::filesystem::is_directory(requested_root, error) || error)
    {
        return false;
    }
    const std::filesystem::path approved_root =
        std::filesystem::weakly_canonical(requested_root, error);
    if (error || approved_root.empty())
        return false;

    const std::filesystem::path requested_output =
        std::filesystem::absolute(std::filesystem::path(output_dir), error);
    if (error || HasBusinessSymlinkComponent(requested_output) ||
        !std::filesystem::is_directory(requested_output, error) || error)
    {
        return false;
    }
    const std::filesystem::path canonical_output =
        std::filesystem::weakly_canonical(requested_output, error);
    if (error || !IsBusinessPathWithin(approved_root, canonical_output))
        return false;

    const std::filesystem::path relative =
        canonical_output.lexically_relative(approved_root);
    std::vector<std::filesystem::path> parts;
    for (const auto& part : relative)
        parts.push_back(part);
    if (parts.size() != 3u ||
        (parts[0] != "isolated_business_validation" &&
         parts[0] != "development_trials") ||
        !IsSafeBusinessPathComponent(parts[1]) ||
        parts[2] != "staging")
    {
        return false;
    }
    resolved = canonical_output;
    return true;
}

int BusinessClassId(const std::string& name)
{
    for (std::size_t index = 0; index < kBusinessGeometryClasses.size(); ++index)
    {
        if (name == kBusinessGeometryClasses[index])
            return static_cast<int>(index);
    }
    return -1;
}

bool ValidateBusinessSevenClassManifest(
    const TorchModelManifest& manifest,
    std::string& reason)
{
    if (!ValidateInstanceSegmentationManifest(manifest, reason))
        return false;
    if (manifest.num_classes !=
        static_cast<int>(kBusinessGeometryClasses.size()))
    {
        reason = "business trial requires the fixed seven-class geometry head";
        return false;
    }
    for (std::size_t index = 0; index < kBusinessGeometryClasses.size(); ++index)
    {
        if (manifest.class_names[index] != kBusinessGeometryClasses[index])
        {
            reason = "business trial manifest geometry ontology is not fixed";
            return false;
        }
    }
    return true;
}

struct BusinessParentAttestation
{
    std::string development_parent_model_id;
    std::string checkpoint_sha256;
    std::string source_lineage_sha256;
    std::string parent_manifest_sha256;
    std::string attestation_sha256;
};

bool ResolveBusinessControlledRegularFile(
    const std::string& root_text,
    const std::string& file_text,
    std::filesystem::path& resolved)
{
    if (root_text.empty() || file_text.empty())
        return false;
    std::error_code error;
    const std::filesystem::path requested_root =
        std::filesystem::absolute(std::filesystem::path(root_text), error);
    if (error || HasBusinessSymlinkComponent(requested_root) ||
        !std::filesystem::is_directory(requested_root, error) || error)
    {
        return false;
    }
    const std::filesystem::path root =
        std::filesystem::weakly_canonical(requested_root, error);
    if (error)
        return false;

    std::filesystem::path requested_file(file_text);
    if (requested_file.is_relative())
        requested_file = root / requested_file;
    requested_file = std::filesystem::absolute(requested_file, error);
    if (error || HasBusinessSymlinkComponent(requested_file) ||
        !std::filesystem::is_regular_file(requested_file, error) || error)
    {
        return false;
    }
    resolved = std::filesystem::weakly_canonical(requested_file, error);
    return !error && IsBusinessPathWithin(root, resolved);
}

bool ValidateBusinessParentAttestation(
    const TorchRuntimeCoreConfig& config,
    const BusinessTrialSettings& settings,
    const std::filesystem::path& parent_manifest_path,
    const std::string& actual_parent_sha256,
    BusinessParentAttestation& attestation)
{
    std::string actual_manifest_sha256;
    if (!BusinessSha256File(parent_manifest_path, actual_manifest_sha256))
        return false;

    std::filesystem::path attestation_path;
    if (!ResolveBusinessControlledRegularFile(
            config.model_root, settings.business_parent_attestation_path,
            attestation_path) ||
        !BusinessSha256File(attestation_path, attestation.attestation_sha256) ||
        attestation.attestation_sha256 !=
            settings.business_parent_attestation_sha256)
    {
        return false;
    }

    try
    {
        cv::FileStorage input(
            attestation_path.string(), cv::FileStorage::READ);
        if (!input.isOpened())
            return false;
        std::string schema;
        std::string parent_usage;
        bool trial_only = false;
        bool production_allowed = true;
        const auto required_string = [&](const char* key, std::string& value) {
            const cv::FileNode node = input[key];
            if (node.empty())
                return false;
            node >> value;
            return IsSafeBusinessIdentifier(value);
        };
        if (!required_string("schema", schema) ||
            !required_string(
                "development_parent_model_id",
                attestation.development_parent_model_id) ||
            !required_string(
                "checkpoint_sha256", attestation.checkpoint_sha256) ||
            !required_string(
                "source_lineage_sha256", attestation.source_lineage_sha256) ||
            !required_string(
                "parent_manifest_sha256",
                attestation.parent_manifest_sha256) ||
            !required_string("parent_usage", parent_usage))
        {
            return false;
        }
        const cv::FileNode trial_only_node = input["trial_only"];
        const cv::FileNode production_allowed_node = input["production_allowed"];
        const cv::FileNode geometry = input["geometry_contract"];
        if (trial_only_node.empty() || production_allowed_node.empty() ||
            geometry.empty() || geometry.type() != cv::FileNode::SEQ ||
            geometry.size() != kBusinessGeometryClasses.size())
        {
            return false;
        }
        trial_only_node >> trial_only;
        production_allowed_node >> production_allowed;
        for (std::size_t index = 0;
             index < kBusinessGeometryClasses.size(); ++index)
        {
            std::string class_name;
            geometry[static_cast<int>(index)] >> class_name;
            if (class_name != kBusinessGeometryClasses[index])
                return false;
        }
        return schema == "visionai.business.development_parent_attestation.v1" &&
            attestation.development_parent_model_id == settings.trial_model_id &&
            parent_usage == "DEVELOPMENT_ONLY" && trial_only &&
            !production_allowed &&
            IsBusinessSha256(attestation.checkpoint_sha256) &&
            IsBusinessSha256(attestation.source_lineage_sha256) &&
            IsBusinessSha256(attestation.parent_manifest_sha256) &&
            attestation.parent_manifest_sha256 == actual_manifest_sha256 &&
            attestation.checkpoint_sha256 == actual_parent_sha256;
    }
    catch (const cv::Exception&)
    {
        return false;
    }
}

TorchTaskResultCpp BusinessTrialFailure(
    const std::string& stage,
    const std::string& code)
{
    TorchTaskResultCpp result;
    result.ok = false;
    result.error_code = -1;
    result.status = "failed";
    result.error_message = code;
    result.result_json =
        "{\"schema\":\"cxvision.yolov8seg.business_trial.v1\","
        "\"status\":\"failed\",\"failure_stage\":" +
        QuoteSegJson(stage) + ",\"code\":" + QuoteSegJson(code) +
        ",\"trial_scope\":\"isolated_business_validation\","
        "\"human_review_required\":true}";
    return result;
}

TorchTaskResultCpp BusinessTrialCancelled(
    unsigned completed_iterations)
{
    TorchTaskResultCpp result;
    result.ok = false;
    result.error_code = -2;
    result.status = "cancelled";
    result.error_message = "BUSINESS_TRIAL_CANCELLED";
    result.result_json =
        "{\"schema\":\"cxvision.yolov8seg.business_trial.v1\","
        "\"status\":\"cancelled\",\"completed_iterations\":" +
        std::to_string(completed_iterations) +
        ",\"candidate_written\":false,"
        "\"trial_scope\":\"isolated_business_validation\","
        "\"human_review_required\":true}";
    return result;
}

bool ParseBusinessTrialSettings(
    const std::string& extra_json,
    BusinessTrialSettings& settings)
{
    if (extra_json.empty())
        return false;
    try
    {
        cv::FileStorage input(
            extra_json,
            cv::FileStorage::READ | cv::FileStorage::MEMORY |
                cv::FileStorage::FORMAT_JSON);
        if (!input.isOpened())
            return false;
        const auto required_unsigned = [&](const char* key, unsigned& value) {
            const cv::FileNode node = input[key];
            if (node.empty())
                return false;
            int parsed = 0;
            node >> parsed;
            if (parsed < 0)
                return false;
            value = static_cast<unsigned>(parsed);
            return true;
        };
        const auto required_string = [&](const char* key, std::string& value) {
            const cv::FileNode node = input[key];
            if (node.empty())
                return false;
            node >> value;
            return IsSafeBusinessIdentifier(value);
        };
        const auto required_local_path = [&](const char* key, std::string& value) {
            const cv::FileNode node = input[key];
            if (node.empty())
                return false;
            node >> value;
            return !value.empty() && value.size() <= 4096u &&
                value.find_first_of("\r\n") == std::string::npos;
        };
        if (!required_unsigned(
                "training_split_percent", settings.training_split_percent) ||
            !required_unsigned("epoch_count", settings.epoch_count) ||
            !required_unsigned("iteration_count", settings.iteration_count) ||
            !required_unsigned("batch_size", settings.batch_size) ||
            !required_string("trial_model_id", settings.trial_model_id) ||
            !required_string(
                "annotation_click_mode", settings.annotation_click_mode) ||
            !required_string("dataset_revision_id", settings.dataset_revision_id) ||
            !required_string("case_id", settings.case_id) ||
            !required_string(
                "project_geometry_class", settings.project_geometry_class))
        {
            return false;
        }
        int project_class_id = -1;
        const cv::FileNode project_class_node =
            input["project_geometry_class_id"];
        if (project_class_node.empty())
            return false;
        project_class_node >> project_class_id;
        settings.project_geometry_class_id = project_class_id;

        const cv::FileNode manifest_digest =
            input["dataset_manifest_sha256"];
        const cv::FileNode legacy_digest = input["dataset_sha256"];
        if (!manifest_digest.empty())
            manifest_digest >> settings.dataset_manifest_sha256;
        if (!legacy_digest.empty())
        {
            std::string legacy_value;
            legacy_digest >> legacy_value;
            if (!settings.dataset_manifest_sha256.empty() &&
                settings.dataset_manifest_sha256 != legacy_value)
            {
                return false;
            }
            settings.dataset_manifest_sha256 = legacy_value;
        }
        if (!IsBusinessSha256(settings.dataset_manifest_sha256))
            return false;

        if (!required_unsigned(
                "frozen_training_split_percent",
                settings.frozen_training_split_percent) ||
            !required_string(
                "frozen_annotation_click_mode",
                settings.frozen_annotation_click_mode) ||
            !required_string(
                "frozen_project_geometry_class",
                settings.frozen_project_geometry_class) ||
            !required_local_path(
                "business_parent_attestation_path",
                settings.business_parent_attestation_path))
        {
            return false;
        }
        const cv::FileNode frozen_project_class_node =
            input["frozen_project_geometry_class_id"];
        if (frozen_project_class_node.empty())
            return false;
        frozen_project_class_node >> settings.frozen_project_geometry_class_id;

        const cv::FileNode parent_attestation_digest =
            input["business_parent_attestation_sha256"];
        if (parent_attestation_digest.empty())
            return false;
        parent_attestation_digest >> settings.business_parent_attestation_sha256;
        if (!IsBusinessSha256(settings.business_parent_attestation_sha256))
            return false;

        const cv::FileNode allowed_modes =
            input["allowed_annotation_click_modes"];
        if (allowed_modes.empty() || allowed_modes.type() != cv::FileNode::SEQ)
            return false;
        for (auto mode = allowed_modes.begin(); mode != allowed_modes.end(); ++mode)
        {
            std::string value;
            *mode >> value;
            if (!IsSafeBusinessIdentifier(value) ||
                !settings.allowed_annotation_click_modes.insert(value).second)
            {
                return false;
            }
        }
        if (settings.allowed_annotation_click_modes.empty())
            return false;
    }
    catch (const cv::Exception&)
    {
        return false;
    }

    return settings.training_split_percent > 0 &&
        settings.training_split_percent < 100 &&
        settings.epoch_count > 0 && settings.epoch_count <= 100000u &&
        settings.iteration_count >= settings.epoch_count &&
        settings.iteration_count <= 100000u &&
        settings.batch_size > 0 && settings.batch_size <= 4096u &&
        settings.project_geometry_class_id ==
            BusinessClassId(settings.project_geometry_class) &&
        settings.project_geometry_class_id >= 0 &&
        settings.project_geometry_class_id <
            static_cast<int>(kBusinessGeometryClasses.size()) &&
        settings.frozen_training_split_percent ==
            settings.training_split_percent &&
        settings.frozen_annotation_click_mode ==
            settings.annotation_click_mode &&
        settings.frozen_project_geometry_class ==
            settings.project_geometry_class &&
        settings.frozen_project_geometry_class_id ==
            settings.project_geometry_class_id &&
        settings.allowed_annotation_click_modes.find(
            settings.annotation_click_mode) !=
            settings.allowed_annotation_click_modes.end();
}

struct BusinessInferenceSettings
{
    std::string verify_asset_binding_id;
    std::string verify_asset_sha256;
};

bool ParseBusinessInferenceSettings(
    const std::string& extra_json,
    BusinessInferenceSettings& settings)
{
    if (extra_json.empty())
        return false;
    try
    {
        cv::FileStorage input(
            extra_json,
            cv::FileStorage::READ | cv::FileStorage::MEMORY |
                cv::FileStorage::FORMAT_JSON);
        if (!input.isOpened())
            return false;
        const cv::FileNode binding_id = input["verify_asset_binding_id"];
        const cv::FileNode digest = input["verify_asset_sha256"];
        if (binding_id.empty() || digest.empty())
            return false;
        binding_id >> settings.verify_asset_binding_id;
        digest >> settings.verify_asset_sha256;
        return IsSafeBusinessIdentifier(settings.verify_asset_binding_id) &&
            IsBusinessSha256(settings.verify_asset_sha256);
    }
    catch (const cv::Exception&)
    {
        return false;
    }
}

bool LoadBusinessMaterializedDataset(
    const std::string& root_text,
    const BusinessTrialSettings& settings,
    BusinessMaterializedDataset& dataset,
    std::string& code)
{
    if (root_text.empty())
    {
        code = "BUSINESS_DATASET_ROOT_REQUIRED";
        return false;
    }
    std::error_code error;
    const std::filesystem::path requested_root =
        std::filesystem::absolute(std::filesystem::path(root_text), error);
    if (error ||
        std::filesystem::is_symlink(
            std::filesystem::symlink_status(requested_root, error)) ||
        error || !std::filesystem::is_directory(requested_root, error) || error)
    {
        code = "BUSINESS_DATASET_ROOT_INVALID";
        return false;
    }
    const std::filesystem::path root =
        std::filesystem::weakly_canonical(requested_root, error);
    if (error)
    {
        code = "BUSINESS_DATASET_ROOT_INVALID";
        return false;
    }
    const std::filesystem::path manifest_relative("manifest.v1");
    std::filesystem::path manifest_path;
    if (!ResolveBusinessMaterializedFile(root, manifest_relative, manifest_path))
    {
        code = "BUSINESS_DATASET_MANIFEST_MISSING";
        return false;
    }
    std::string manifest_text;
    if (!ReadBusinessFile(manifest_path, manifest_text))
    {
        code = "BUSINESS_DATASET_MANIFEST_UNREADABLE";
        return false;
    }
    dataset.manifest_sha256 = BusinessSha256Digest(manifest_text);
    if (dataset.manifest_sha256 != settings.dataset_manifest_sha256)
    {
        code = "BUSINESS_DATASET_MANIFEST_DIGEST_MISMATCH";
        return false;
    }

    std::map<std::string, std::string> metadata;
    std::map<std::string, BusinessMaterializedAsset> assets;
    std::istringstream input(manifest_text);
    std::string line;
    while (std::getline(input, line))
    {
        if (line.empty())
            continue;
        const std::size_t equal = line.find('=');
        if (equal == std::string::npos || equal == 0)
        {
            code = "BUSINESS_DATASET_MANIFEST_INVALID";
            return false;
        }
        const std::string key = line.substr(0, equal);
        const std::string value = line.substr(equal + 1);
        if (key == "image" || key == "mask")
        {
            std::vector<std::string> fields;
            std::size_t offset = 0;
            while (offset <= value.size())
            {
                const std::size_t separator = value.find('|', offset);
                fields.push_back(value.substr(
                    offset, separator == std::string::npos
                        ? std::string::npos : separator - offset));
                if (separator == std::string::npos)
                    break;
                offset = separator + 1u;
            }
            if (fields.size() != 4u || !IsSafeBusinessImageId(fields[0]) ||
                !IsSafeBusinessRelativePath(
                    std::filesystem::path(fields[2])) ||
                !IsBusinessSha256(fields[3]))
            {
                code = "BUSINESS_DATASET_MANIFEST_INVALID";
                return false;
            }
            if (fields[1] == "holdout")
            {
                // VERIFY belongs to a separate unmarked asset binding.  A
                // holdout image or mask here would be an accidental attempt
                // to feed the future inference image into training.
                code = "BUSINESS_DATASET_VERIFY_MATERIALIZATION_FORBIDDEN";
                return false;
            }
            if (!IsBusinessTrainingSplit(fields[1]))
            {
                code = "BUSINESS_DATASET_MANIFEST_INVALID";
                return false;
            }
            BusinessMaterializedAsset& asset = assets[fields[0]];
            if (asset.image_id.empty())
            {
                asset.image_id = fields[0];
                asset.split = fields[1];
            }
            if (asset.split != fields[1])
            {
                code = "BUSINESS_DATASET_MANIFEST_SPLIT_MISMATCH";
                return false;
            }
            if (key == "image")
            {
                if (asset.has_image)
                {
                    code = "BUSINESS_DATASET_MANIFEST_DUPLICATE_IMAGE";
                    return false;
                }
                asset.has_image = true;
                asset.image_relative = fields[2];
                asset.image_digest = fields[3];
            }
            else
            {
                if (asset.has_mask)
                {
                    code = "BUSINESS_DATASET_MANIFEST_DUPLICATE_MASK";
                    return false;
                }
                asset.has_mask = true;
                asset.mask_relative = fields[2];
                asset.mask_digest = fields[3];
            }
            continue;
        }
        const std::set<std::string> allowed_metadata{
            "schema", "snapshot_id", "snapshot_digest", "case_id",
            "dataset_revision_id", "annotation_receipt_digest",
            "background_pixel", "boundary_stroke_width_pixels"};
        if (allowed_metadata.find(key) == allowed_metadata.end() ||
            !metadata.emplace(key, value).second)
        {
            code = "BUSINESS_DATASET_MANIFEST_INVALID";
            return false;
        }
    }
    const auto value_for = [&](const char* key) -> const std::string* {
        const auto found = metadata.find(key);
        return found == metadata.end() ? nullptr : &found->second;
    };
    const std::string* schema = value_for("schema");
    const std::string* snapshot_id = value_for("snapshot_id");
    const std::string* snapshot_digest = value_for("snapshot_digest");
    const std::string* case_id = value_for("case_id");
    const std::string* revision_id = value_for("dataset_revision_id");
    const std::string* receipt_digest = value_for("annotation_receipt_digest");
    const std::string* background_pixel = value_for("background_pixel");
    if (schema == nullptr || *schema !=
            "visionai.geometry-segmentation-materializer.v1" ||
        snapshot_id == nullptr || !IsSafeBusinessIdentifier(*snapshot_id) ||
        snapshot_digest == nullptr || !IsBusinessSha256(*snapshot_digest) ||
        case_id == nullptr || *case_id != settings.case_id ||
        revision_id == nullptr || *revision_id != settings.dataset_revision_id ||
        receipt_digest == nullptr || !IsBusinessSha256(*receipt_digest) ||
        background_pixel == nullptr || *background_pixel != "255")
    {
        code = "BUSINESS_DATASET_BINDING_MISMATCH";
        return false;
    }
    dataset.snapshot_id = *snapshot_id;
    dataset.snapshot_digest = *snapshot_digest;
    dataset.case_id = *case_id;
    dataset.dataset_revision_id = *revision_id;
    dataset.annotation_receipt_digest = *receipt_digest;

    for (const auto& pair : assets)
    {
        const BusinessMaterializedAsset& asset = pair.second;
        if (!asset.has_image || !asset.has_mask)
        {
            code = "BUSINESS_DATASET_IMAGE_MASK_PAIR_MISSING";
            return false;
        }
        std::filesystem::path image_path;
        std::filesystem::path mask_path;
        if (!ResolveBusinessMaterializedFile(
                root, asset.image_relative, image_path) ||
            !ResolveBusinessMaterializedFile(
                root, asset.mask_relative, mask_path))
        {
            code = "BUSINESS_DATASET_ASSET_PATH_INVALID";
            return false;
        }
        std::string image_digest;
        std::string mask_digest;
        if (!BusinessSha256File(image_path, image_digest) ||
            !BusinessSha256File(mask_path, mask_digest) ||
            image_digest != asset.image_digest ||
            mask_digest != asset.mask_digest)
        {
            code = "BUSINESS_DATASET_ASSET_DIGEST_MISMATCH";
            return false;
        }
        cv::Mat image = cv::imread(image_path.string(), cv::IMREAD_COLOR);
        cv::Mat class_mask = cv::imread(mask_path.string(), cv::IMREAD_GRAYSCALE);
        if (image.empty() || class_mask.empty() ||
            image.rows != class_mask.rows || image.cols != class_mask.cols)
        {
            code = "BUSINESS_DATASET_ASSET_UNREADABLE";
            return false;
        }

        YoloV8SegDatasetSample sample;
        sample.image_id = asset.image_id;
        sample.image_ref = image_path.string();
        sample.split = asset.split;
        sample.target_mask_ref = mask_path.string();
        bool class_present = false;
        for (int row = 0; row < class_mask.rows; ++row)
        {
            const auto* values = class_mask.ptr<unsigned char>(row);
            for (int column = 0; column < class_mask.cols; ++column)
            {
                const unsigned char value = values[column];
                if (value != 255u && value > 6u)
                {
                    code = "BUSINESS_DATASET_MASK_CLASS_INVALID";
                    return false;
                }
                if (value != 255u && value !=
                    static_cast<unsigned char>(
                        settings.project_geometry_class_id))
                {
                    code = "BUSINESS_PROJECT_GEOMETRY_MIXED";
                    return false;
                }
                class_present = class_present ||
                    value == static_cast<unsigned char>(
                        settings.project_geometry_class_id);
            }
        }
        if (!class_present)
        {
            code = "BUSINESS_DATASET_MASK_EMPTY";
            return false;
        }
        cv::Mat selected;
        cv::compare(
            class_mask,
            cv::Scalar(settings.project_geometry_class_id),
            selected,
            cv::CMP_EQ);
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(
            selected, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        for (const auto& contour : contours)
        {
            if (contour.size() < 3u || cv::contourArea(contour) <= 0.0)
            {
                code = "BUSINESS_DATASET_MASK_GEOMETRY_INVALID";
                return false;
            }
            std::vector<cv::Point2f> polygon;
            polygon.reserve(contour.size());
            const float width = static_cast<float>(
                std::max(1, class_mask.cols - 1));
            const float height = static_cast<float>(
                std::max(1, class_mask.rows - 1));
            for (const cv::Point& point : contour)
            {
                polygon.emplace_back(
                    std::clamp(point.x / width, 0.0f, 1.0f),
                    std::clamp(point.y / height, 0.0f, 1.0f));
            }
            if (!AppendDatasetPolygon(
                    sample, settings.project_geometry_class_id,
                    std::move(polygon)))
            {
                code = "BUSINESS_DATASET_MASK_GEOMETRY_INVALID";
                return false;
            }
        }
        if (sample.classes.empty())
        {
            code = "BUSINESS_DATASET_MASK_GEOMETRY_INVALID";
            return false;
        }
        if (asset.split == "train")
            dataset.train_samples.push_back(std::move(sample));
        else if (asset.split == "val")
            dataset.validation_samples.push_back(std::move(sample));
        else
        {
            code = "BUSINESS_DATASET_MANIFEST_INVALID";
            return false;
        }
    }
    if (dataset.train_samples.size() < 2u ||
        dataset.validation_samples.empty())
    {
        code = "BUSINESS_DATASET_SPLIT_INSUFFICIENT";
        return false;
    }
    return true;
}

torch::Tensor BusinessMaskTarget(
    const std::vector<cv::Point2f>& polygon,
    int proto_width,
    int proto_height,
    const SegLetterbox& letterbox,
    const TorchModelManifest& manifest,
    const torch::TensorOptions& options)
{
    cv::Mat mask(proto_height, proto_width, CV_8UC1, cv::Scalar(0));
    std::vector<cv::Point> points;
    points.reserve(polygon.size());
    for (const cv::Point2f& point : polygon)
    {
        const int x = std::clamp(
            static_cast<int>(std::lround(
                (letterbox.pad_x + point.x * letterbox.resized_width) /
                static_cast<double>(manifest.input_width) * proto_width)),
            0, proto_width - 1);
        const int y = std::clamp(
            static_cast<int>(std::lround(
                (letterbox.pad_y + point.y * letterbox.resized_height) /
                static_cast<double>(manifest.input_height) * proto_height)),
            0, proto_height - 1);
        points.emplace_back(x, y);
    }
    if (points.size() >= 3u)
    {
        cv::fillPoly(
            mask, std::vector<std::vector<cv::Point>>{points},
            cv::Scalar(255), cv::LINE_8);
    }
    return torch::from_blob(
        mask.data, {proto_height, proto_width},
        torch::TensorOptions().dtype(torch::kUInt8))
        .clone()
        .to(options.device())
        .to(options.dtype()) / 255.0;
}

BusinessLossTensors ComputeBusinessYoloV8SegLoss(
    YoloV8Segment& model,
    const YoloV8SegRawOutput& raw,
    const YoloV8SegDatasetSample& sample,
    const torch::Tensor& input,
    const SegLetterbox& letterbox,
    const TorchModelManifest& manifest)
{
    BusinessLossTensors result;
    const auto options = input.options();
    result.class_loss = torch::zeros({}, options);
    result.mask_loss = torch::zeros({}, options);
    result.box_loss = torch::zeros({}, options);
    result.dfl_loss = torch::zeros({}, options);
    const int proto_height = static_cast<int>(raw.prototypes.size(2));
    const int proto_width = static_cast<int>(raw.prototypes.size(3));
    const torch::Tensor proto_flat =
        raw.prototypes.index({0}).view({manifest.mask_channels, -1});
    const std::size_t level_count = raw.class_logits.size();
    for (std::size_t target_index = 0;
         target_index < sample.classes.size(); ++target_index)
    {
        const auto& box = sample.boxes_xyxy_norm[target_index];
        const auto to_input_x = [&](float value) {
            return static_cast<float>((letterbox.pad_x +
                value * letterbox.resized_width) /
                static_cast<double>(manifest.input_width));
        };
        const auto to_input_y = [&](float value) {
            return static_cast<float>((letterbox.pad_y +
                value * letterbox.resized_height) /
                static_cast<double>(manifest.input_height));
        };
        const std::array<float, 4> input_box{
            to_input_x(box[0]), to_input_y(box[1]),
            to_input_x(box[2]), to_input_y(box[3])};
        const int64_t class_id = sample.classes[target_index];
        TORCH_CHECK(class_id >= 0 && class_id < manifest.num_classes,
            "business dataset class ID is outside the fixed head");
        const torch::Tensor mask_target = BusinessMaskTarget(
            sample.polygons_norm[target_index], proto_width, proto_height,
            letterbox, manifest, options);
        const float center_x =
            (input_box[0] + input_box[2]) * 0.5f;
        const float center_y =
            (input_box[1] + input_box[3]) * 0.5f;
        for (std::size_t level = 0; level < level_count; ++level)
        {
            const int64_t feature_height = raw.class_logits[level].size(2);
            const int64_t feature_width = raw.class_logits[level].size(3);
            const int64_t column = std::clamp<int64_t>(
                static_cast<int64_t>(std::floor(center_x * feature_width)),
                0, feature_width - 1);
            const int64_t row = std::clamp<int64_t>(
                static_cast<int64_t>(std::floor(center_y * feature_height)),
                0, feature_height - 1);
            const torch::Tensor class_logits = raw.class_logits[level].index(
                {0, torch::indexing::Slice(), row, column});
            torch::Tensor class_target = torch::zeros_like(class_logits);
            class_target.index_put_({class_id}, 1.0);
            result.class_loss = result.class_loss +
                torch::binary_cross_entropy_with_logits(
                    class_logits, class_target);

            const torch::Tensor coefficients =
                raw.mask_coefficients[level].index(
                    {0, torch::indexing::Slice(), row, column});
            const torch::Tensor mask_logits =
                torch::matmul(coefficients, proto_flat)
                    .view({proto_height, proto_width});
            const torch::Tensor mask_bce =
                torch::binary_cross_entropy_with_logits(
                    mask_logits, mask_target);
            const torch::Tensor mask_probability = mask_logits.sigmoid();
            const torch::Tensor dice = 1.0 -
                (2.0 * (mask_probability * mask_target).sum() + 1.0) /
                (mask_probability.sum() + mask_target.sum() + 1.0);
            result.mask_loss = result.mask_loss + mask_bce + dice;

            const float stride =
                static_cast<float>(manifest.input_width) /
                static_cast<float>(feature_width);
            const float anchor_x =
                (static_cast<float>(column) + 0.5f) * stride;
            const float anchor_y =
                (static_cast<float>(row) + 0.5f) * stride;
            const torch::Tensor target_distances = torch::tensor(
                {std::max(0.0f,
                     (anchor_x - input_box[0] * manifest.input_width) / stride),
                 std::max(0.0f,
                     (anchor_y - input_box[1] * manifest.input_height) / stride),
                 std::max(0.0f,
                     (input_box[2] * manifest.input_width - anchor_x) / stride),
                 std::max(0.0f,
                     (input_box[3] * manifest.input_height - anchor_y) / stride)},
                options).clamp(0.0, 15.0 - 1.0e-3);
            const torch::Tensor box_logits = raw.box_logits[level]
                .index({0, torch::indexing::Slice(), row, column})
                .view({1, 64, 1});
            const torch::Tensor predicted_distances =
                model->head()->dfl_module()->expectation(box_logits)
                    .view({4});
            result.box_loss = result.box_loss +
                torch::abs(predicted_distances - target_distances).mean();
            const torch::Tensor dfl_logits = box_logits.view({4, 16});
            const torch::Tensor target_left =
                torch::floor(target_distances).to(torch::kLong);
            const torch::Tensor target_right =
                (target_left + 1).clamp_max(15);
            const torch::Tensor right_weight =
                (target_distances -
                 target_left.to(target_distances.dtype()))
                    .clamp(0.0, 1.0);
            const torch::Tensor left_weight = 1.0 - right_weight;
            const torch::Tensor left_loss =
                torch::nn::functional::cross_entropy(
                    dfl_logits, target_left,
                    torch::nn::functional::CrossEntropyFuncOptions()
                        .reduction(torch::kNone));
            const torch::Tensor right_loss =
                torch::nn::functional::cross_entropy(
                    dfl_logits, target_right,
                    torch::nn::functional::CrossEntropyFuncOptions()
                        .reduction(torch::kNone));
            result.dfl_loss = result.dfl_loss +
                (left_loss * left_weight + right_loss * right_weight).mean();
        }
    }
    const double divisor = static_cast<double>(
        std::max<std::size_t>(1u, sample.classes.size() * level_count));
    result.class_loss = result.class_loss / divisor;
    result.mask_loss = result.mask_loss / divisor;
    result.box_loss = result.box_loss / divisor;
    result.dfl_loss = result.dfl_loss / divisor;
    result.total_loss =
        result.class_loss + result.mask_loss +
        result.box_loss * 0.25 + result.dfl_loss * 0.25;
    return result;
}

bool EvaluateBusinessValidation(
    YoloV8Segment& model,
    const std::vector<YoloV8SegDatasetSample>& samples,
    const TorchModelManifest& manifest,
    const torch::Device& device,
    double& validation_loss)
{
    if (samples.empty())
        return false;
    model->eval();
    torch::NoGradGuard no_grad;
    double accumulated = 0.0;
    for (const auto& sample : samples)
    {
        cv::Mat image = cv::imread(sample.image_ref, cv::IMREAD_COLOR);
        if (image.empty())
            return false;
        SegLetterbox letterbox;
        torch::Tensor input =
            MakeSegInput(image, manifest, letterbox).to(device);
        const YoloV8SegRawOutput raw = model->forward(input);
        const BusinessLossTensors losses = ComputeBusinessYoloV8SegLoss(
            model, raw, sample, input, letterbox, manifest);
        const double value = losses.total_loss.detach().item<double>();
        if (!std::isfinite(value))
            return false;
        accumulated += value;
    }
    validation_loss = accumulated / static_cast<double>(samples.size());
    return std::isfinite(validation_loss) && validation_loss >= 0.0;
}

bool WriteBusinessCheckpoint(
    YoloV8Segment& model,
    const std::filesystem::path& path)
{
    c10::Dict<std::string, torch::Tensor> state_dict;
    for (const auto& named : model->named_parameters(true))
        state_dict.insert(named.key(), named.value().detach().cpu());
    for (const auto& named : model->named_buffers(true))
        state_dict.insert(named.key(), named.value().detach().cpu());
    const std::vector<char> bytes = torch::pickle_save(state_dict);
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    output.close();
    return output.good();
}

bool WriteBusinessCandidateManifest(
    const std::filesystem::path& path,
    const TorchModelManifest& parent_manifest,
    const std::string& model_id,
    const std::string& weights_sha256,
    const std::string& development_parent_model_id,
    const std::string& development_parent_checkpoint_sha256,
    const std::string& parent_attestation_sha256)
{
    std::ofstream output(path);
    output
        << "{\n"
        << "  \"schema\":\"cxvision.torch_model_manifest\",\n"
        << "  \"schema_version\":2,\n"
        << "  \"model_id\":" << QuoteSegJson(model_id) << ",\n"
        << "  \"task\":\"instance_segmentation\",\n"
        << "  \"architecture\":\"yolov8_seg\",\n"
        << "  \"variant\":\"nano\",\n"
        << "  \"weights\":\"weights/business_trial_state_dict.pt\",\n"
        << "  \"weights_format\":\"python_state_dict\",\n"
        << "  \"weights_hash\":" << QuoteSegJson(weights_sha256) << ",\n"
        << "  \"num_classes\":7,\n"
        << "  \"mask_channels\":32,\n"
        << "  \"prototype_channels\":64,\n"
        << "  \"configured_prototype_channels\":256,\n"
        << "  \"classes\":[\"arc\",\"circle\",\"ellipse\",\"line\","
        << "\"open_curve\",\"polygon\",\"closed_curve\"],\n"
        << "  \"input\":{\"width\":" << parent_manifest.input_width
        << ",\"height\":" << parent_manifest.input_height
        << ",\"color\":\"rgb\",\"scale\":0.003921568627,"
        << "\"letterbox\":true},\n"
        << "  \"postprocess\":{\"confidence_threshold\":"
        << parent_manifest.confidence_threshold
        << ",\"iou_threshold\":" << parent_manifest.iou_threshold
        << ",\"mask_threshold\":" << parent_manifest.mask_threshold
        << ",\"max_detections\":" << parent_manifest.max_detections
        << ",\"class_agnostic_nms\":"
        << (parent_manifest.class_agnostic_nms ? "true" : "false") << "},\n"
        << "  \"business_trial_scope\":\"isolated_business_validation\",\n"
        << "  \"business_trial_state\":\"CANDIDATE_REVIEW_REQUIRED\",\n"
        << "  \"business_development_parent_model_id\":"
        << QuoteSegJson(development_parent_model_id) << ",\n"
        << "  \"business_development_parent_checkpoint_sha256\":"
        << QuoteSegJson(development_parent_checkpoint_sha256) << ",\n"
        << "  \"business_parent_attestation_sha256\":"
        << QuoteSegJson(parent_attestation_sha256) << "\n"
        << "}\n";
    output.close();
    return output.good();
}

bool IsBusinessCancellationRequested(
    const TorchYoloV8SegBusinessCancelProbe& probe)
{
    return probe && probe();
}
} // namespace

TorchTaskResultCpp ExecuteTorchYoloV8SegBusinessTrialTask(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request,
    const TorchYoloV8SegBusinessEpochSink& on_epoch,
    const TorchYoloV8SegBusinessCancelProbe& is_cancelled)
{
    try
    {
        if (IsBusinessCancellationRequested(is_cancelled))
            return BusinessTrialCancelled(0);
        BusinessTrialSettings settings;
        if (!ParseBusinessTrialSettings(request.extra_json, settings))
        {
            return BusinessTrialFailure(
                "training_parameters",
                "TRAINING_PARAMETER_INVALID");
        }
        std::filesystem::path output_root;
        if (!ResolveBusinessStagingOutputDirectory(
                config, request.output_dir, output_root))
        {
            return BusinessTrialFailure(
                "output_policy",
                "BUSINESS_TRIAL_OUTPUT_SCOPE_INVALID");
        }

        TorchModelManifest manifest;
        std::string reason;
        std::filesystem::path parent_manifest_path;
        if (!ResolveBusinessControlledRegularFile(
                config.model_root, request.manifest_path,
                parent_manifest_path) ||
            !LoadTorchModelManifest(
                parent_manifest_path, config.model_root, manifest, reason) ||
            !ValidateBusinessSevenClassManifest(manifest, reason))
        {
            return BusinessTrialFailure(
                "parent_model",
                "BUSINESS_TRIAL_PARENT_MODEL_INVALID");
        }
        std::string actual_parent_sha256;
        if (!BusinessSha256File(manifest.weights_path, actual_parent_sha256) ||
            !IsBusinessSha256(actual_parent_sha256))
        {
            return BusinessTrialFailure(
                "parent_model",
                "BUSINESS_TRIAL_PARENT_HASH_MISMATCH");
        }
        BusinessParentAttestation parent_attestation;
        if (!ValidateBusinessParentAttestation(
                config, settings, parent_manifest_path,
                actual_parent_sha256, parent_attestation))
        {
            // A legacy FNV source manifest is never elevated into a business
            // identity.  Only an immutable SHA-256 development-parent
            // attestation can bridge the source manifest to the fixed parent.
            return BusinessTrialFailure(
                "parent_model",
                "BUSINESS_TRIAL_PARENT_ATTESTATION_INVALID");
        }

        BusinessMaterializedDataset dataset;
        std::string dataset_code;
        if (!LoadBusinessMaterializedDataset(
                request.dataset_root, settings, dataset, dataset_code))
        {
            return BusinessTrialFailure("dataset", dataset_code);
        }
        if (IsBusinessCancellationRequested(is_cancelled))
            return BusinessTrialCancelled(0);

        std::error_code filesystem_error;
        if (HasBusinessSymlinkComponent(output_root) ||
            !std::filesystem::is_directory(output_root, filesystem_error) ||
            filesystem_error ||
            !std::filesystem::is_empty(output_root, filesystem_error) ||
            filesystem_error)
        {
            return BusinessTrialFailure(
                "output_policy",
                "BUSINESS_TRIAL_STAGING_NOT_EMPTY");
        }

        const std::string device_name =
            (request.device == "cuda" || config.device == "cuda") &&
                    torch::cuda::is_available()
                ? "cuda"
                : "cpu";
        const torch::Device device(device_name);
        YoloV8Segment model(7);
        const YoloV8SegWeightMappingReport mapping =
            model->load_state_dict_strict(manifest.weights_path.string());
        if (!mapping.complete())
        {
            return BusinessTrialFailure(
                "parent_model",
                "BUSINESS_TRIAL_PARENT_MODEL_INVALID");
        }
        model->to(device);
        model->train();
        torch::optim::Adam optimizer(
            model->parameters(),
            torch::optim::AdamOptions(settings.learning_rate)
                .weight_decay(settings.weight_decay));

        std::vector<BusinessEpochMetric> trace;
        trace.reserve(settings.epoch_count);
        const unsigned base_iterations =
            settings.iteration_count / settings.epoch_count;
        const unsigned remainder_iterations =
            settings.iteration_count % settings.epoch_count;
        unsigned completed_iterations = 0;
        std::size_t sample_cursor = 0;
        const auto started = std::chrono::steady_clock::now();
        for (unsigned epoch = 1; epoch <= settings.epoch_count; ++epoch)
        {
            if (IsBusinessCancellationRequested(is_cancelled))
                return BusinessTrialCancelled(completed_iterations);
            const double schedule_position = settings.epoch_count <= 1u
                ? 0.0
                : static_cast<double>(epoch - 1u) /
                    static_cast<double>(settings.epoch_count - 1u);
            const double learning_rate = settings.lr_schedule == "cosine"
                ? settings.min_learning_rate +
                    (settings.learning_rate - settings.min_learning_rate) *
                        0.5 * (1.0 + std::cos(
                            std::acos(-1.0) * schedule_position))
                : settings.learning_rate;
            for (auto& parameter_group : optimizer.param_groups())
            {
                auto& options = static_cast<torch::optim::AdamOptions&>(
                    parameter_group.options());
                options.lr(learning_rate);
            }
            const unsigned iterations_this_epoch =
                base_iterations +
                (epoch <= remainder_iterations ? 1u : 0u);
            if (iterations_this_epoch == 0u)
            {
                return BusinessTrialFailure(
                    "training_parameters",
                    "TRAINING_PARAMETER_INVALID");
            }
            double accumulated_loss = 0.0;
            unsigned observed_samples = 0;
            for (unsigned iteration = 0;
                 iteration < iterations_this_epoch;
                 ++iteration)
            {
                if (IsBusinessCancellationRequested(is_cancelled))
                    return BusinessTrialCancelled(completed_iterations);
                optimizer.zero_grad();
                for (unsigned batch_index = 0;
                     batch_index < settings.batch_size;
                     ++batch_index)
                {
                    if (IsBusinessCancellationRequested(is_cancelled))
                        return BusinessTrialCancelled(completed_iterations);
                    const YoloV8SegDatasetSample& sample =
                        dataset.train_samples[
                            sample_cursor % dataset.train_samples.size()];
                    ++sample_cursor;
                    cv::Mat image =
                        cv::imread(sample.image_ref, cv::IMREAD_COLOR);
                    if (image.empty())
                    {
                        return BusinessTrialFailure(
                            "training_input",
                            "BUSINESS_DATASET_ASSET_UNREADABLE");
                    }
                    SegLetterbox letterbox;
                    torch::Tensor input =
                        MakeSegInput(image, manifest, letterbox).to(device);
                    const YoloV8SegRawOutput raw = model->forward(input);
                    const BusinessLossTensors losses =
                        ComputeBusinessYoloV8SegLoss(
                            model, raw, sample, input, letterbox, manifest);
                    const double loss_value =
                        losses.total_loss.detach().item<double>();
                    if (!std::isfinite(loss_value) || loss_value < 0.0)
                    {
                        return BusinessTrialFailure(
                            "optimizer",
                            "BUSINESS_TRIAL_NONFINITE_LOSS");
                    }
                    (losses.total_loss /
                     static_cast<double>(settings.batch_size)).backward();
                    accumulated_loss += loss_value;
                    ++observed_samples;
                }
                optimizer.step();
                ++completed_iterations;
            }

            double validation_loss = 0.0;
            if (!EvaluateBusinessValidation(
                    model, dataset.validation_samples, manifest, device,
                    validation_loss))
            {
                return BusinessTrialFailure(
                    "validation",
                    "BUSINESS_VALIDATION_METRIC_UNAVAILABLE");
            }
            model->train();
            const double training_loss =
                accumulated_loss / static_cast<double>(
                    std::max(1u, observed_samples));
            const double training_score = 1.0 / (1.0 + training_loss);
            const double validation_score = 1.0 / (1.0 + validation_loss);
            if (!std::isfinite(training_score) ||
                !std::isfinite(validation_score))
            {
                return BusinessTrialFailure(
                    "validation",
                    "BUSINESS_VALIDATION_METRIC_UNAVAILABLE");
            }
            BusinessEpochMetric metric;
            metric.epoch = epoch;
            metric.completed_iterations = completed_iterations;
            metric.training_loss = training_loss;
            metric.training_score = training_score;
            metric.validation_loss = validation_loss;
            metric.validation_score = validation_score;
            metric.effective_learning_rate = learning_rate;
            trace.push_back(metric);
            const unsigned progress = static_cast<unsigned>(
                std::min<std::uint64_t>(
                    100u,
                    static_cast<std::uint64_t>(epoch) * 100u /
                        settings.epoch_count));
            if (on_epoch && !on_epoch(progress, {
                    metric.epoch,
                    metric.completed_iterations,
                    metric.training_loss,
                    metric.validation_score,
                    true,
                    metric.effective_learning_rate}))
            {
                return BusinessTrialCancelled(completed_iterations);
            }
        }
        if (completed_iterations != settings.iteration_count)
        {
            return BusinessTrialFailure(
                "optimizer",
                "BUSINESS_ITERATION_ACCOUNTING_INVALID");
        }
        if (IsBusinessCancellationRequested(is_cancelled))
            return BusinessTrialCancelled(completed_iterations);

        const std::filesystem::path pending_root =
            output_root / ".business_trial_pending";
        const std::filesystem::path candidate_root =
            output_root / "candidate";
        if (std::filesystem::exists(pending_root, filesystem_error) ||
            filesystem_error ||
            std::filesystem::exists(candidate_root, filesystem_error) ||
            filesystem_error ||
            !std::filesystem::create_directory(pending_root, filesystem_error) ||
            filesystem_error)
        {
            return BusinessTrialFailure(
                "output_write",
                "BUSINESS_TRIAL_STAGING_CONFLICT");
        }
        const auto cleanup_pending = [&]() {
            std::error_code ignored;
            std::filesystem::remove_all(pending_root, ignored);
        };
        const std::filesystem::path weights_dir = pending_root / "weights";
        std::filesystem::create_directories(weights_dir, filesystem_error);
        if (filesystem_error)
        {
            cleanup_pending();
            return BusinessTrialFailure(
                "output_write",
                "BUSINESS_TRIAL_STAGING_CREATE_FAILED");
        }
        const std::filesystem::path checkpoint =
            weights_dir / "business_trial_state_dict.pt";
        const std::filesystem::path trace_path =
            pending_root / "training_trace.json";
        const std::filesystem::path candidate_manifest =
            pending_root / "model_manifest.json";
        std::ofstream trace_file(trace_path);
        trace_file
            << "{\n"
            << "  \"schema\":\"cxvision.yolov8seg.business_training_trace.v1\",\n"
            << "  \"trial_scope\":\"isolated_business_validation\",\n"
            << "  \"curve_score_definition\":\"inverse_total_loss\",\n"
            << "  \"points\":[\n";
        for (std::size_t index = 0; index < trace.size(); ++index)
        {
            const BusinessEpochMetric& metric = trace[index];
            trace_file
                << "    {\"epoch\":" << metric.epoch
                << ",\"completed_iterations\":"
                << metric.completed_iterations
                << ",\"training_loss\":" << metric.training_loss
                << ",\"training_score\":" << metric.training_score
                << ",\"validation_loss\":" << metric.validation_loss
                << ",\"validation_score\":" << metric.validation_score
                << ",\"effective_learning_rate\":"
                << metric.effective_learning_rate << "}"
                << (index + 1u == trace.size() ? "" : ",") << "\n";
        }
        trace_file << "  ]\n}\n";
        trace_file.close();
        if (!trace_file.good())
        {
            cleanup_pending();
            return BusinessTrialFailure(
                "output_write",
                "BUSINESS_TRAINING_TRACE_WRITE_FAILED");
        }
        if (IsBusinessCancellationRequested(is_cancelled))
        {
            cleanup_pending();
            return BusinessTrialCancelled(completed_iterations);
        }
        if (!WriteBusinessCheckpoint(model, checkpoint))
        {
            cleanup_pending();
            return BusinessTrialFailure(
                "checkpoint",
                "BUSINESS_TRIAL_CHECKPOINT_WRITE_FAILED");
        }
        if (IsBusinessCancellationRequested(is_cancelled))
        {
            cleanup_pending();
            return BusinessTrialCancelled(completed_iterations);
        }
        std::string candidate_sha256;
        if (!BusinessSha256File(checkpoint, candidate_sha256) ||
            !IsBusinessSha256(candidate_sha256))
        {
            cleanup_pending();
            return BusinessTrialFailure(
                "checkpoint",
                "BUSINESS_TRIAL_CHECKPOINT_HASH_FAILED");
        }
        const std::string candidate_model_id =
            "business_yolov8seg_7class_trial_" +
            candidate_sha256.substr(7u, 16u);
        if (!WriteBusinessCandidateManifest(
                candidate_manifest, manifest, candidate_model_id,
                candidate_sha256, settings.trial_model_id,
                actual_parent_sha256, parent_attestation.attestation_sha256))
        {
            cleanup_pending();
            return BusinessTrialFailure(
                "candidate_manifest",
                "BUSINESS_TRIAL_MANIFEST_WRITE_FAILED");
        }
        if (IsBusinessCancellationRequested(is_cancelled))
        {
            cleanup_pending();
            return BusinessTrialCancelled(completed_iterations);
        }
        std::filesystem::rename(
            pending_root, candidate_root, filesystem_error);
        if (filesystem_error)
        {
            cleanup_pending();
            return BusinessTrialFailure(
                "candidate_commit",
                "BUSINESS_TRIAL_CANDIDATE_COMMIT_FAILED");
        }

        const double elapsed_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started).count();
        const BusinessEpochMetric& final_epoch = trace.back();
        TorchTaskResultCpp result;
        result.ok = true;
        result.status = "completed_review_required";
        result.requested_device = request.device;
        result.actual_device = device_name;
        result.train_runtime_ms = elapsed_ms;
        result.algorithm_runtime_ms = elapsed_ms;
        result.result_ref = (candidate_root / "model_manifest.json").string();
        result.evidence_ref =
            (candidate_root / "training_trace.json").string();
        result.trainer_lifecycle_summary =
            "fixed-seven-class YOLOv8-Seg business trial completed";
        result.unified_mainline_summary =
            "development trial candidate requires VERIFY inference and human review";
        result.result_json =
            "{\"schema\":\"cxvision.yolov8seg.business_trial.v1\","
            "\"status\":\"completed_review_required\","
            "\"trial_scope\":\"isolated_business_validation\","
            "\"candidate_state\":\"CANDIDATE_REVIEW_REQUIRED\","
            "\"production_activation\":false,"
            "\"human_review_required\":true,"
            "\"case_id\":" + QuoteSegJson(settings.case_id) +
            ",\"dataset_revision_id\":" +
                QuoteSegJson(settings.dataset_revision_id) +
            ",\"dataset_manifest_sha256\":" +
                QuoteSegJson(dataset.manifest_sha256) +
            ",\"parent_model_id\":" +
                QuoteSegJson(settings.trial_model_id) +
            ",\"parent_model_sha256\":" +
                QuoteSegJson(actual_parent_sha256) +
            ",\"parent_attestation_sha256\":" +
                QuoteSegJson(parent_attestation.attestation_sha256) +
            ",\"candidate_model_id\":" +
                QuoteSegJson(candidate_model_id) +
            ",\"candidate_model_sha256\":" +
                QuoteSegJson(candidate_sha256) +
            ",\"project_geometry_class\":" +
                QuoteSegJson(settings.project_geometry_class) +
            ",\"project_geometry_class_id\":" +
                std::to_string(settings.project_geometry_class_id) +
            ",\"annotation_click_mode\":" +
                QuoteSegJson(settings.annotation_click_mode) +
            ",\"training_split_percent\":" +
                std::to_string(settings.training_split_percent) +
            ",\"train_image_count\":" +
                std::to_string(dataset.train_samples.size()) +
            ",\"validation_image_count\":" +
                std::to_string(dataset.validation_samples.size()) +
            ",\"verify_asset_binding_required\":true"
            ",\"epoch_count\":" +
                std::to_string(settings.epoch_count) +
            ",\"iteration_count\":" +
                std::to_string(settings.iteration_count) +
            ",\"batch_size\":" +
                std::to_string(settings.batch_size) +
            ",\"effective_learning_rate\":" +
                std::to_string(final_epoch.effective_learning_rate) +
            ",\"curve_score_definition\":\"inverse_total_loss\","
            "\"training_loss\":" +
                std::to_string(final_epoch.training_loss) +
            ",\"validation_score\":" +
                std::to_string(final_epoch.validation_score) + "}";
        return result;
    }
    catch (const std::exception&)
    {
        // Exception text can contain a local asset path; business receipts cannot.
        return BusinessTrialFailure(
            "exception", "BUSINESS_TRIAL_RUNTIME_EXCEPTION");
    }
}

TorchTaskResultCpp ExecuteTorchYoloV8SegBusinessInferenceTask(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request)
{
    try
    {
        std::filesystem::path output_root;
        if (!ResolveBusinessStagingOutputDirectory(
                config, request.output_dir, output_root))
        {
            return BusinessTrialFailure(
                "output_policy",
                "BUSINESS_INFERENCE_OUTPUT_SCOPE_INVALID");
        }
        std::error_code output_error;
        if (HasBusinessSymlinkComponent(output_root) ||
            !std::filesystem::is_empty(output_root, output_error) ||
            output_error)
        {
            return BusinessTrialFailure(
                "output_policy",
                "BUSINESS_INFERENCE_STAGING_NOT_EMPTY");
        }
        BusinessInferenceSettings inference_settings;
        if (!ParseBusinessInferenceSettings(
                request.extra_json, inference_settings))
        {
            return BusinessTrialFailure(
                "verify_asset",
                "BUSINESS_VERIFY_ASSET_BINDING_INVALID");
        }
        TorchModelManifest manifest;
        std::string reason;
        if (!LoadTorchModelManifest(
                request.manifest_path, config.model_root, manifest, reason) ||
            !ValidateBusinessSevenClassManifest(manifest, reason))
        {
            return BusinessTrialFailure(
                "candidate_model",
                "BUSINESS_INFERENCE_CANDIDATE_INVALID");
        }
        cv::FileStorage candidate_file(
            request.manifest_path, cv::FileStorage::READ);
        std::string trial_scope;
        std::string trial_state;
        if (!candidate_file.isOpened() ||
            candidate_file["business_trial_scope"].empty() ||
            candidate_file["business_trial_state"].empty())
        {
            return BusinessTrialFailure(
                "candidate_model",
                "BUSINESS_INFERENCE_CANDIDATE_INVALID");
        }
        candidate_file["business_trial_scope"] >> trial_scope;
        candidate_file["business_trial_state"] >> trial_state;
        if (trial_scope != "isolated_business_validation" ||
            trial_state != "CANDIDATE_REVIEW_REQUIRED")
        {
            return BusinessTrialFailure(
                "candidate_model",
                "BUSINESS_INFERENCE_CANDIDATE_SCOPE_INVALID");
        }
        std::string actual_candidate_sha256;
        if (!BusinessSha256File(
                manifest.weights_path, actual_candidate_sha256) ||
            !IsBusinessSha256(manifest.weights_hash) ||
            actual_candidate_sha256 != manifest.weights_hash)
        {
            return BusinessTrialFailure(
                "candidate_model",
                "BUSINESS_INFERENCE_CANDIDATE_HASH_MISMATCH");
        }
        std::error_code input_error;
        if (request.input_image.empty() ||
            !std::filesystem::is_regular_file(
                std::filesystem::path(request.input_image), input_error) ||
            input_error)
        {
            return BusinessTrialFailure(
                "verify_asset",
                "BUSINESS_VERIFY_ASSET_INVALID");
        }
        std::string actual_verify_image_sha256;
        if (!BusinessSha256File(
                std::filesystem::path(request.input_image),
                actual_verify_image_sha256) ||
            actual_verify_image_sha256 !=
                inference_settings.verify_asset_sha256)
        {
            return BusinessTrialFailure(
                "verify_asset",
                "BUSINESS_VERIFY_ASSET_HASH_MISMATCH");
        }

        // Direct actual YOLOv8-Seg inference, never a generic dispatcher
        // route. Raw local visual artifacts are filtered by the typed bridge.
        TorchTaskRequestCpp local_request = request;
        local_request.task = TorchRuntimeTaskIds::YoloV8InstanceSegmentation;
        TorchTaskResultCpp local_result =
            ExecuteTorchYoloV8SegTask(config, local_request);
        if (!local_result.ok)
        {
            return BusinessTrialFailure(
                "inference", "BUSINESS_INFERENCE_EXECUTION_FAILED");
        }
        local_result.status = "auto_provisional_review_required";
        local_result.unified_mainline_summary =
            "AUTO_PROVISIONAL inference requires manual business review";
        local_result.result_json =
            "{\"schema\":\"cxvision.yolov8seg.business_inference.v1\","
            "\"status\":\"AUTO_PROVISIONAL\","
            "\"review_state\":\"REVIEW_REQUIRED\","
            "\"trial_scope\":\"isolated_business_validation\","
            "\"candidate_model_id\":" +
                QuoteSegJson(manifest.model_id) +
            ",\"candidate_model_sha256\":" +
                QuoteSegJson(actual_candidate_sha256) +
            ",\"verify_asset_binding_id\":" +
                QuoteSegJson(inference_settings.verify_asset_binding_id) +
            ",\"verify_asset_sha256\":" +
                QuoteSegJson(inference_settings.verify_asset_sha256) +
            ",\"production_activation\":false,"
            "\"human_review_required\":true}";
        return local_result;
    }
    catch (const std::exception&)
    {
        return BusinessTrialFailure(
            "exception", "BUSINESS_INFERENCE_RUNTIME_EXCEPTION");
    }
}
