#include "CxExecutionTypes.h"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <fstream>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace
{
cv::Mat LoadCxBinaryMask(const std::string& path)
{
    cv::Mat mask = cv::imread(path, cv::IMREAD_GRAYSCALE);
    if (!mask.empty())
        cv::threshold(mask, mask, 0, 255, cv::THRESH_BINARY);
    return mask;
}

double CxSafeRatio(double numerator, double denominator)
{
    return denominator > 0.0 ? numerator / denominator : 0.0;
}

cv::Mat ExtractCxMaskBoundary(const cv::Mat& binary)
{
    cv::Mat eroded;
    cv::erode(binary, eroded, cv::Mat(), cv::Point(-1, -1), 1);
    cv::Mat boundary;
    cv::subtract(binary, eroded, boundary);
    return boundary;
}

std::string EscapeCxDiagnosticJson(const std::string& value)
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

void WriteCxMaskFacts(
    std::ofstream& file,
    const char* name,
    const CxMaskFactsSnapshot& facts)
{
    file << "  \"" << name << "\": {\n"
         << "    \"evaluated\": " << (facts.evaluated ? "true" : "false") << ",\n"
         << "    \"loadable\": " << (facts.loadable ? "true" : "false") << ",\n"
         << "    \"width\": " << facts.width << ",\n"
         << "    \"height\": " << facts.height << ",\n"
         << "    \"foreground_pixels\": " << facts.foreground_pixels << ",\n"
         << "    \"foreground_ratio\": " << facts.foreground_ratio << ",\n"
         << "    \"component_count\": " << facts.component_count << ",\n"
         << "    \"boundary_pixels\": " << facts.boundary_pixels << ",\n"
         << "    \"bbox\": [" << facts.bbox_x << ", " << facts.bbox_y << ", "
         << facts.bbox_width << ", " << facts.bbox_height << "],\n"
         << "    \"bbox_fill_ratio\": " << facts.bbox_fill_ratio << ",\n"
         << "    \"empty\": " << (facts.empty ? "true" : "false") << ",\n"
         << "    \"full_frame\": " << (facts.full_frame ? "true" : "false") << ",\n"
         << "    \"touches_border\": " << (facts.touches_border ? "true" : "false") << ",\n"
         << "    \"status\": \"" << EscapeCxDiagnosticJson(facts.status) << "\",\n"
         << "    \"reason\": \"" << EscapeCxDiagnosticJson(facts.reason) << "\"\n"
         << "  }";
}
} // namespace

bool ValidateCxTorchTaskSpec(const CxTorchTaskSpec& task, std::string& reason)
{
    if (task.kind == CxTorchTaskKind::Unknown)
    {
        reason = "torch task kind is unknown";
        return false;
    }

    if (task.task_id.empty())
    {
        reason = "torch task id is empty";
        return false;
    }

    if (task.requested_device != "cpu" &&
        task.requested_device != "cuda" &&
        task.requested_device != "auto")
    {
        reason = "unsupported torch device: " + task.requested_device;
        return false;
    }

    if (task.timeout_ms < 0)
    {
        reason = "torch timeout cannot be negative";
        return false;
    }

    const bool needs_image =
        task.kind == CxTorchTaskKind::Segmentation ||
        task.kind == CxTorchTaskKind::Detection ||
        task.kind == CxTorchTaskKind::Classification ||
        task.kind == CxTorchTaskKind::FeatureExtraction ||
        task.kind == CxTorchTaskKind::TemplateDifference ||
        task.kind == CxTorchTaskKind::PrototypeLifecycle ||
        task.kind == CxTorchTaskKind::SegmentationContract ||
        task.kind == CxTorchTaskKind::DetectionContract;

    if (needs_image && task.input_image_path.empty())
    {
        reason = "torch input image path is empty";
        return false;
    }

    reason.clear();
    return true;
}

bool AnalyzeCxMaskFile(
    const std::string& mask_path,
    CxMaskFactsSnapshot& snapshot,
    std::string& reason)
{
    snapshot = {};
    snapshot.evaluated = true;

    const cv::Mat binary = LoadCxBinaryMask(mask_path);
    if (binary.empty())
    {
        snapshot.status = "MASK_LOAD_FAILED";
        snapshot.reason = "mask is missing or unreadable: " + mask_path;
        reason = snapshot.reason;
        return false;
    }

    snapshot.loadable = true;
    snapshot.width = binary.cols;
    snapshot.height = binary.rows;
    snapshot.foreground_pixels = cv::countNonZero(binary);
    const int total_pixels = binary.cols * binary.rows;
    snapshot.foreground_ratio = CxSafeRatio(snapshot.foreground_pixels, total_pixels);
    snapshot.empty = snapshot.foreground_pixels == 0;
    snapshot.full_frame = snapshot.foreground_ratio >= 0.98;

    const cv::Mat boundary = ExtractCxMaskBoundary(binary);
    snapshot.boundary_pixels = cv::countNonZero(boundary);

    if (!snapshot.empty)
    {
        cv::Mat labels;
        cv::Mat stats;
        cv::Mat centroids;
        const int label_count = cv::connectedComponentsWithStats(
            binary, labels, stats, centroids, 8, CV_32S);
        snapshot.component_count = std::max(0, label_count - 1);

        std::vector<cv::Point> points;
        cv::findNonZero(binary, points);
        const cv::Rect box = cv::boundingRect(points);
        snapshot.bbox_x = box.x;
        snapshot.bbox_y = box.y;
        snapshot.bbox_width = box.width;
        snapshot.bbox_height = box.height;
        snapshot.bbox_fill_ratio = CxSafeRatio(
            snapshot.foreground_pixels,
            static_cast<double>(box.width) * box.height);
        snapshot.touches_border =
            box.x == 0 || box.y == 0 ||
            box.x + box.width == binary.cols ||
            box.y + box.height == binary.rows;
    }

    snapshot.status = "MASK_FACTS_COMPLETE";
    snapshot.reason.clear();
    reason.clear();
    return true;
}

bool CompareCxMaskFiles(
    const std::string& left_mask_path,
    const std::string& right_mask_path,
    CxMaskComparisonSnapshot& snapshot,
    std::string& reason)
{
    snapshot = {};
    snapshot.evaluated = true;

    std::string left_reason;
    std::string right_reason;
    const bool left_ok = AnalyzeCxMaskFile(left_mask_path, snapshot.left, left_reason);
    const bool right_ok = AnalyzeCxMaskFile(right_mask_path, snapshot.right, right_reason);
    if (!left_ok || !right_ok)
    {
        snapshot.status = "MASK_INPUT_INVALID";
        snapshot.reason = !left_ok ? left_reason : right_reason;
        reason = snapshot.reason;
        return false;
    }

    snapshot.dimensions_match =
        snapshot.left.width == snapshot.right.width &&
        snapshot.left.height == snapshot.right.height;
    if (!snapshot.dimensions_match)
    {
        snapshot.status = "MASK_DIMENSION_MISMATCH";
        snapshot.reason = "mask dimensions differ";
        reason = snapshot.reason;
        return false;
    }

    const cv::Mat left = LoadCxBinaryMask(left_mask_path);
    const cv::Mat right = LoadCxBinaryMask(right_mask_path);
    cv::Mat intersection;
    cv::Mat mask_union;
    cv::bitwise_and(left, right, intersection);
    cv::bitwise_or(left, right, mask_union);
    snapshot.intersection_pixels = cv::countNonZero(intersection);
    snapshot.union_pixels = cv::countNonZero(mask_union);

    if (snapshot.union_pixels == 0)
    {
        snapshot.iou = 1.0;
        snapshot.dice = 1.0;
    }
    else
    {
        snapshot.iou = CxSafeRatio(snapshot.intersection_pixels, snapshot.union_pixels);
        snapshot.dice = CxSafeRatio(
            2.0 * snapshot.intersection_pixels,
            snapshot.left.foreground_pixels + snapshot.right.foreground_pixels);
    }
    snapshot.foreground_ratio_delta =
        snapshot.right.foreground_ratio - snapshot.left.foreground_ratio;

    const cv::Mat left_boundary = ExtractCxMaskBoundary(left);
    const cv::Mat right_boundary = ExtractCxMaskBoundary(right);
    if (snapshot.left.boundary_pixels == 0 && snapshot.right.boundary_pixels == 0)
    {
        snapshot.boundary_precision = 1.0;
        snapshot.boundary_recall = 1.0;
        snapshot.boundary_fscore = 1.0;
    }
    else if (snapshot.left.boundary_pixels > 0 && snapshot.right.boundary_pixels > 0)
    {
        const cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
        cv::Mat dilated_left;
        cv::Mat dilated_right;
        cv::dilate(left_boundary, dilated_left, kernel);
        cv::dilate(right_boundary, dilated_right, kernel);

        cv::Mat left_matches;
        cv::Mat right_matches;
        cv::bitwise_and(left_boundary, dilated_right, left_matches);
        cv::bitwise_and(right_boundary, dilated_left, right_matches);
        snapshot.boundary_precision = CxSafeRatio(
            cv::countNonZero(left_matches), snapshot.left.boundary_pixels);
        snapshot.boundary_recall = CxSafeRatio(
            cv::countNonZero(right_matches), snapshot.right.boundary_pixels);
        snapshot.boundary_fscore = CxSafeRatio(
            2.0 * snapshot.boundary_precision * snapshot.boundary_recall,
            snapshot.boundary_precision + snapshot.boundary_recall);
    }

    snapshot.status = "MASK_COMPARISON_COMPLETE";
    snapshot.reason.clear();
    reason.clear();
    return true;
}

bool WriteCxMaskComparisonJson(
    const CxMaskComparisonSnapshot& snapshot,
    const std::string& output_path,
    std::string& reason)
{
    try
    {
        const std::filesystem::path path(output_path);
        if (path.empty())
        {
            reason = "mask comparison output path is empty";
            return false;
        }
        if (!path.parent_path().empty())
            std::filesystem::create_directories(path.parent_path());

        std::ofstream file(path, std::ios::trunc);
        if (!file)
        {
            reason = "cannot open mask comparison output: " + output_path;
            return false;
        }

        file << "{\n"
             << "  \"schema\": \"cxvision.mask_diagnostic.v1\",\n"
             << "  \"status\": \"" << EscapeCxDiagnosticJson(snapshot.status) << "\",\n"
             << "  \"reason\": \"" << EscapeCxDiagnosticJson(snapshot.reason) << "\",\n"
             << "  \"dimensions_match\": " << (snapshot.dimensions_match ? "true" : "false") << ",\n";
        WriteCxMaskFacts(file, "left", snapshot.left);
        file << ",\n";
        WriteCxMaskFacts(file, "right", snapshot.right);
        file << ",\n"
             << "  \"metrics\": {\n"
             << "    \"intersection_pixels\": " << snapshot.intersection_pixels << ",\n"
             << "    \"union_pixels\": " << snapshot.union_pixels << ",\n"
             << "    \"iou\": " << snapshot.iou << ",\n"
             << "    \"dice\": " << snapshot.dice << ",\n"
             << "    \"boundary_precision\": " << snapshot.boundary_precision << ",\n"
             << "    \"boundary_recall\": " << snapshot.boundary_recall << ",\n"
             << "    \"boundary_fscore\": " << snapshot.boundary_fscore << ",\n"
             << "    \"foreground_ratio_delta\": " << snapshot.foreground_ratio_delta << "\n"
             << "  }\n"
             << "}\n";
        if (!file.good())
        {
            reason = "failed to write mask comparison output: " + output_path;
            return false;
        }
    }
    catch (const std::exception& error)
    {
        reason = error.what();
        return false;
    }

    reason.clear();
    return true;
}
