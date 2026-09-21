#include "CxTypedLabelProposalGenerator.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <sstream>
#include <vector>

namespace
{
std::string EscapeJson(const std::string& value)
{
    std::string escaped;
    for (char ch : value)
    {
        if (ch == '\\' || ch == '"')
            escaped += '\\';
        if (ch == '\n') { escaped += "\\n"; continue; }
        if (ch == '\r') { escaped += "\\r"; continue; }
        escaped += ch;
    }
    return escaped;
}

bool WriteText(const std::filesystem::path& path, const std::string& text, std::string& reason)
{
    std::ofstream file(path, std::ios::trunc);
    if (!file)
    {
        reason = "cannot open output: " + path.string();
        return false;
    }
    file << text;
    if (!file.good())
    {
        reason = "cannot write output: " + path.string();
        return false;
    }
    return true;
}

cv::Mat LargestClosedForeground(const cv::Mat& bgr)
{
    cv::Mat gray;
    cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, gray, cv::Size(5, 5), 0.0);
    cv::Mat mask;
    cv::threshold(gray, mask, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

    const int border = std::max(1, std::min(mask.cols, mask.rows) / 50);
    const double borderMean = (cv::mean(mask.rowRange(0, border))[0] +
        cv::mean(mask.rowRange(mask.rows - border, mask.rows))[0] +
        cv::mean(mask.colRange(0, border))[0] +
        cv::mean(mask.colRange(mask.cols - border, mask.cols))[0]) / 4.0;
    if (borderMean > 127.0)
        cv::bitwise_not(mask, mask);

    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE,
        cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(11, 11)));
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN,
        cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5)));

    cv::Mat labels, stats, centroids;
    const int count = cv::connectedComponentsWithStats(mask, labels, stats, centroids, 8);
    int best = 0;
    int bestArea = 0;
    for (int i = 1; i < count; ++i)
    {
        const int area = stats.at<int>(i, cv::CC_STAT_AREA);
        if (area > bestArea)
        {
            bestArea = area;
            best = i;
        }
    }
    cv::Mat result = cv::Mat::zeros(mask.size(), CV_8UC1);
    if (best > 0)
        result.setTo(255, labels == best);
    return result;
}

void DrawMaskOverlay(const cv::Mat& image, const cv::Mat& mask, const cv::Scalar& color, cv::Mat& overlay)
{
    overlay = image.clone();
    cv::Mat tint(image.size(), image.type(), color);
    cv::Mat blended;
    cv::addWeighted(image, 0.55, tint, 0.45, 0.0, blended);
    blended.copyTo(overlay, mask);
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask.clone(), contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    cv::drawContours(overlay, contours, -1, color, 2);
}

bool ProposeClosedObject(const cv::Mat& image, cv::Mat& label, cv::Mat& overlay,
    std::string& facts, std::string& reason)
{
    label = LargestClosedForeground(image);
    const int area = cv::countNonZero(label);
    if (area == 0)
    {
        reason = "no closed foreground component found";
        return false;
    }
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(label.clone(), contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
    const cv::Rect box = cv::boundingRect(contours.front());
    DrawMaskOverlay(image, label, cv::Scalar(40, 210, 40), overlay);
    std::ostringstream out;
    out << "\"detected_structure\": \"single_closed_foreground_object\",\n"
        << "    \"component_count\": 1,\n"
        << "    \"foreground_pixels\": " << area << ",\n"
        << "    \"bbox_xywh\": [" << box.x << ", " << box.y << ", " << box.width << ", " << box.height << "],\n"
        << "    \"processing_direction\": \"binary semantic segmentation with closed-region topology checks\"";
    facts = out.str();
    return true;
}

bool ProposeDefect(const cv::Mat& image, cv::Mat& label, cv::Mat& overlay,
    std::string& facts, std::string& reason)
{
    const cv::Mat object = LargestClosedForeground(image);
    if (cv::countNonZero(object) == 0)
    {
        reason = "host object was not found";
        return false;
    }
    cv::Mat gray;
    cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    cv::Mat local;
    cv::GaussianBlur(gray, local, cv::Size(21, 21), 0.0);
    cv::Mat residual;
    cv::subtract(local, gray, residual);

    cv::Mat interior;
    cv::erode(object, interior,
        cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(15, 15)));
    cv::Scalar mean, stddev;
    cv::meanStdDev(residual, mean, stddev, interior);
    const double thresholdValue = std::max(8.0, mean[0] + 3.0 * stddev[0]);
    cv::threshold(residual, label, thresholdValue, 255, cv::THRESH_BINARY);
    cv::bitwise_and(label, interior, label);
    cv::morphologyEx(label, label, cv::MORPH_OPEN,
        cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3)));

    cv::Mat labels, stats, centroids;
    const int count = cv::connectedComponentsWithStats(label, labels, stats, centroids, 8);
    cv::Mat filtered = cv::Mat::zeros(label.size(), CV_8UC1);
    const int objectArea = cv::countNonZero(object);
    int bestComponent = 0;
    double bestScore = 0.0;
    for (int i = 1; i < count; ++i)
    {
        const int area = stats.at<int>(i, cv::CC_STAT_AREA);
        const int width = stats.at<int>(i, cv::CC_STAT_WIDTH);
        const int height = stats.at<int>(i, cv::CC_STAT_HEIGHT);
        if (area >= 3 && area <= std::max(20, objectArea / 100) &&
            width <= std::max(8, height * 4) && height <= std::max(8, width * 4))
        {
            const double score = cv::mean(residual, labels == i)[0] * std::sqrt(static_cast<double>(area));
            if (score > bestScore)
            {
                bestScore = score;
                bestComponent = i;
            }
        }
    }
    const int kept = bestComponent > 0 ? 1 : 0;
    if (bestComponent > 0)
        filtered.setTo(255, labels == bestComponent);
    label = filtered;
    if (kept == 0)
    {
        reason = "no compact dark anomaly passed the provisional defect filter";
        return false;
    }
    DrawMaskOverlay(image, label, cv::Scalar(20, 30, 240), overlay);
    std::ostringstream out;
    out << "\"detected_structure\": \"compact_dark_regions_inside_closed_host\",\n"
        << "    \"candidate_component_count\": " << kept << ",\n"
        << "    \"candidate_pixels\": " << cv::countNonZero(label) << ",\n"
        << "    \"local_dark_residual_threshold\": " << std::fixed << std::setprecision(3) << thresholdValue << ",\n"
        << "    \"processing_direction\": \"binary defect semantic segmentation conditioned on host-object mask\"";
    facts = out.str();
    return true;
}

bool ProposeInstances(const cv::Mat& image, cv::Mat& label, cv::Mat& overlay,
    std::string& facts, std::string& instancesJson, std::string& reason)
{
    cv::Mat gray;
    cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, gray, cv::Size(7, 7), 0.0);
    cv::Mat candidate;
    cv::Canny(gray, candidate, 24, 72);
    cv::morphologyEx(candidate, candidate, cv::MORPH_CLOSE,
        cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(7, 7)));
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(candidate, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);
    struct InstanceCandidate { std::vector<cv::Point> contour; cv::Rect box; double area = 0.0; };
    std::vector<InstanceCandidate> accepted;
    const double imageArea = static_cast<double>(image.rows) * image.cols;
    std::sort(contours.begin(), contours.end(), [](const auto& lhs, const auto& rhs) {
        return std::abs(cv::contourArea(lhs)) > std::abs(cv::contourArea(rhs));
    });
    for (const auto& contour : contours)
    {
        const double area = std::abs(cv::contourArea(contour));
        const cv::Rect box = cv::boundingRect(contour);
        const double fill = area / std::max(1.0, static_cast<double>(box.area()));
        const double aspect = static_cast<double>(box.width) / std::max(1, box.height);
        if (area < imageArea * 0.0015 || area > imageArea * 0.04 ||
            box.width < 35 || box.height < 35 || fill < 0.42 || fill > 0.94 ||
            aspect < 0.45 || aspect > 2.8)
            continue;
        bool overlaps = false;
        for (const auto& existing : accepted)
        {
            const cv::Rect intersection = box & existing.box;
            const double iou = static_cast<double>(intersection.area()) /
                std::max(1.0, static_cast<double>((box | existing.box).area()));
            if (iou > 0.35 || existing.box.contains((box.tl() + box.br()) * 0.5))
            {
                overlaps = true;
                break;
            }
        }
        if (!overlaps)
            accepted.push_back({contour, box, area});
    }
    label = cv::Mat::zeros(image.size(), CV_16UC1);
    overlay = image.clone();
    std::ostringstream items;
    int instanceId = 0;
    for (const auto& item : accepted)
    {
        const auto& contour = item.contour;
        const cv::Rect box = item.box;
        const double area = item.area;
        ++instanceId;
        cv::Mat one = cv::Mat::zeros(image.size(), CV_8UC1);
        std::vector<std::vector<cv::Point>> draw{contour};
        cv::drawContours(one, draw, 0, cv::Scalar(255), cv::FILLED);
        label.setTo(instanceId, one);
        const cv::Scalar color(40 + (instanceId * 67) % 190,
            40 + (instanceId * 113) % 190, 40 + (instanceId * 149) % 190);
        cv::drawContours(overlay, draw, 0, color, 3);
        cv::putText(overlay, std::to_string(instanceId), box.tl() + cv::Point(3, 22),
            cv::FONT_HERSHEY_SIMPLEX, 0.65, color, 2, cv::LINE_AA);
        if (instanceId > 1)
            items << ",";
        items << "{\"instance_id\": " << instanceId
              << ", \"class_id\": 1, \"class_name\": \"pill\", \"bbox_xywh\": ["
              << box.x << ", " << box.y << ", " << box.width << ", " << box.height
              << "], \"area\": " << static_cast<int>(area) << "}";
    }
    if (instanceId == 0)
    {
        reason = "no pill-sized instance candidates found";
        return false;
    }
    std::ostringstream companion;
    companion << "{\n  \"schema\": \"cxvision.provisional_instance_classes.v1\",\n"
              << "  \"reference_semantics\": \"auto_provisional_not_ground_truth\",\n"
              << "  \"instance_count\": " << instanceId << ",\n"
              << "  \"instances\": [" << items.str() << "]\n}\n";
    instancesJson = companion.str();
    std::ostringstream out;
    out << "\"detected_structure\": \"multiple_separable_pill_sized_regions\",\n"
        << "    \"instance_count\": " << instanceId << ",\n"
        << "    \"processing_direction\": \"instance segmentation with per-instance class and id mask\"";
    facts = out.str();
    return true;
}

bool ProposeOpenBoundary(const cv::Mat& image, std::vector<cv::Point>& polyline,
    cv::Mat& boundaryMap, cv::Mat& overlay, std::string& facts, std::string& reason)
{
    cv::Mat object = LargestClosedForeground(image);
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(object.clone(), contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);
    if (contours.empty() || contours.front().size() < 4)
    {
        reason = "host contour is unavailable";
        return false;
    }
    const auto& contour = contours.front();
    std::vector<int> hull;
    cv::convexHull(contour, hull, false, false);
    if (hull.size() < 3)
    {
        reason = "host contour has no convex hull";
        return false;
    }
    std::vector<cv::Vec4i> defects;
    cv::convexityDefects(contour, hull, defects);
    if (defects.empty())
    {
        reason = "no open-boundary concavity found";
        return false;
    }
    const cv::Vec4i* best = &defects.front();
    for (const auto& defect : defects)
        if (defect[3] > (*best)[3]) best = &defect;
    const double depth = (*best)[3] / 256.0;
    if (depth < 2.0)
    {
        reason = "largest boundary concavity is below proposal threshold";
        return false;
    }
    int index = (*best)[0];
    const int end = (*best)[1];
    for (;;)
    {
        polyline.push_back(contour[index]);
        if (index == end)
            break;
        index = (index + 1) % static_cast<int>(contour.size());
        if (polyline.size() > contour.size())
            break;
    }
    if (polyline.size() < 3)
    {
        reason = "open-boundary polyline is too short";
        return false;
    }
    boundaryMap = cv::Mat::zeros(image.size(), CV_8UC1);
    cv::polylines(boundaryMap, polyline, false, cv::Scalar(255), 2, cv::LINE_AA);
    overlay = image.clone();
    cv::polylines(overlay, polyline, false, cv::Scalar(0, 40, 255), 3, cv::LINE_AA);
    cv::circle(overlay, polyline.front(), 5, cv::Scalar(0, 255, 255), cv::FILLED);
    cv::circle(overlay, polyline.back(), 5, cv::Scalar(0, 255, 255), cv::FILLED);
    std::ostringstream out;
    out << "\"detected_structure\": \"boundary_concavity_intersecting_host_outline\",\n"
        << "    \"polyline_point_count\": " << polyline.size() << ",\n"
        << "    \"concavity_depth_px\": " << std::fixed << std::setprecision(3) << depth << ",\n"
        << "    \"processing_direction\": \"open-boundary segmentation followed by endpoint and geometric measurement\"";
    facts = out.str();
    return true;
}
} // namespace

bool GenerateCxTypedLabelProposal(const CxTypedLabelProposalOptions& options,
    CxTypedLabelProposalResult& result, std::string& reason)
{
    result = {};
    if (options.image_path.empty() || options.output_dir.empty() || options.label_kind.empty())
    {
        reason = "--image, --out and --typed-label-kind are required";
        return false;
    }
    cv::Mat image = cv::imread(options.image_path.string(), cv::IMREAD_COLOR);
    if (image.empty())
    {
        reason = "input image is missing or unreadable";
        return false;
    }
    try { std::filesystem::create_directories(options.output_dir); }
    catch (const std::exception& error) { reason = error.what(); return false; }

    cv::Mat label, overlay;
    std::string facts;
    std::string companionJson;
    std::vector<cv::Point> polyline;
    bool ok = false;
    if (options.label_kind == "binary_closed_object_mask")
        ok = ProposeClosedObject(image, label, overlay, facts, reason);
    else if (options.label_kind == "binary_closed_defect_mask")
        ok = ProposeDefect(image, label, overlay, facts, reason);
    else if (options.label_kind == "instance_id_mask_with_class")
        ok = ProposeInstances(image, label, overlay, facts, companionJson, reason);
    else if (options.label_kind == "open_boundary_polyline_with_endpoints")
        ok = ProposeOpenBoundary(image, polyline, label, overlay, facts, reason);
    else
    {
        reason = "unsupported typed label kind: " + options.label_kind;
        return false;
    }
    if (!ok)
        return false;

    result.overlay_ref = options.output_dir / "proposal_overlay.png";
    result.analysis_ref = options.output_dir / "content_analysis.json";
    result.manifest_ref = options.output_dir / "typed_label_manifest.json";
    if (options.label_kind == "open_boundary_polyline_with_endpoints")
    {
        result.label_ref = options.output_dir / "typed_label.json";
        result.companion_ref = options.output_dir / "boundary_map.png";
        std::ostringstream json;
        json << "{\n  \"schema\": \"cxvision.provisional_open_boundary.v1\",\n"
             << "  \"reference_semantics\": \"auto_provisional_not_ground_truth\",\n"
             << "  \"closed\": false,\n  \"points_xy\": [";
        for (size_t i = 0; i < polyline.size(); ++i)
        {
            if (i) json << ",";
            json << "[" << polyline[i].x << ", " << polyline[i].y << "]";
        }
        json << "],\n  \"endpoints_xy\": [[" << polyline.front().x << ", " << polyline.front().y
             << "], [" << polyline.back().x << ", " << polyline.back().y << "]]\n}\n";
        if (!WriteText(result.label_ref, json.str(), reason) ||
            !cv::imwrite(result.companion_ref.string(), label))
            return false;
    }
    else
    {
        result.label_ref = options.output_dir / "typed_label.png";
        if (!cv::imwrite(result.label_ref.string(), label))
        {
            reason = "failed to write typed label image";
            return false;
        }
        if (!companionJson.empty())
        {
            result.companion_ref = options.output_dir / "instances.json";
            if (!WriteText(result.companion_ref, companionJson, reason))
                return false;
        }
    }
    if (!cv::imwrite(result.overlay_ref.string(), overlay))
    {
        reason = "failed to write proposal overlay";
        return false;
    }

    std::ostringstream analysis;
    analysis << "{\n  \"schema\": \"cxvision.typed_label_content_analysis.v1\",\n"
             << "  \"input_image_ref\": \"" << EscapeJson(options.image_path.string()) << "\",\n"
             << "  \"typed_label_kind\": \"" << EscapeJson(options.label_kind) << "\",\n"
             << "  \"automatic_decision\": {\n    " << facts << "\n  },\n"
             << "  \"confidence_role\": \"proposal_for_human_confirmation\",\n"
             << "  \"independent_ground_truth\": false,\n"
             << "  \"training_enabled\": 0\n}\n";
    if (!WriteText(result.analysis_ref, analysis.str(), reason))
        return false;

    std::ostringstream manifest;
    manifest << "{\n  \"schema\": \"cxvision.auto_provisional_typed_label.v1\",\n"
             << "  \"status\": \"auto_provisional\",\n"
             << "  \"kind\": \"" << EscapeJson(options.label_kind) << "\",\n"
             << "  \"label_ref\": \"" << EscapeJson(result.label_ref.string()) << "\",\n"
             << "  \"companion_ref\": \"" << EscapeJson(result.companion_ref.string()) << "\",\n"
             << "  \"overlay_ref\": \"" << EscapeJson(result.overlay_ref.string()) << "\",\n"
             << "  \"content_analysis_ref\": \"" << EscapeJson(result.analysis_ref.string()) << "\",\n"
             << "  \"provenance\": \"cximage_typed_label_proposal_v1\",\n"
             << "  \"independent_ground_truth\": false,\n"
             << "  \"promotion_eligible\": false,\n"
             << "  \"training_enabled\": 0\n}\n";
    if (!WriteText(result.manifest_ref, manifest.str(), reason))
        return false;

    result.complete = true;
    result.conclusion = "HEADLESS_EXECUTION_PASS";
    result.reason = "typed label proposal generated; human confirmation and independent label binding remain required";
    reason.clear();
    return true;
}

int RunCxTypedLabelProposalCli(const CxTypedLabelProposalOptions& options)
{
    CxTypedLabelProposalResult result;
    std::string reason;
    const bool ok = GenerateCxTypedLabelProposal(options, result, reason);
    std::cout << "typed_label_proposal_ok=" << (ok && result.complete ? "true" : "false") << "\n"
              << "conclusion=" << (ok ? result.conclusion : "FAIL") << "\n"
              << "reason=" << (result.reason.empty() ? reason : result.reason) << "\n"
              << "typed_label=" << result.label_ref.string() << "\n"
              << "companion=" << result.companion_ref.string() << "\n"
              << "overlay=" << result.overlay_ref.string() << "\n"
              << "content_analysis=" << result.analysis_ref.string() << "\n"
              << "manifest=" << result.manifest_ref.string() << "\n";
    return ok && result.complete ? 0 : 1;
}
