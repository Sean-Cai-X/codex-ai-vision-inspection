#include "pch.h"
#include "CxShapeOverlayRenderer.h"
#include "CxScriptHeadlessRuntime.h"

#include <algorithm>
#include <cmath>

void DrawRoiRectangle(cv::Mat& img, const CxShapeElementSnapshot& shape, const cv::Scalar& color)
{
    if (shape.points.size() >= 4)
    {
        double min_x = shape.points[0];
        double max_x = shape.points[0];
        double min_y = shape.points[1];
        double max_y = shape.points[1];

        for (size_t i = 0; i + 1 < shape.points.size(); i += 2)
        {
            min_x = std::min(min_x, shape.points[i]);
            max_x = std::max(max_x, shape.points[i]);
            min_y = std::min(min_y, shape.points[i + 1]);
            max_y = std::max(max_y, shape.points[i + 1]);
        }

        cv::Point pt1(static_cast<int>(min_x), static_cast<int>(min_y));
        cv::Point pt2(static_cast<int>(max_x), static_cast<int>(max_y));
        cv::rectangle(img, pt1, pt2, color, 2);
    }
}

void DrawRoiPolyline(cv::Mat& img, const CxShapeElementSnapshot& shape, const cv::Scalar& color)
{
    if (shape.points.size() < 4)
        return;

    std::vector<cv::Point> pts;
    for (size_t i = 0; i + 1 < shape.points.size(); i += 2)
        pts.emplace_back(static_cast<int>(shape.points[i]), static_cast<int>(shape.points[i + 1]));

    cv::polylines(img, pts, true, color, 2);
}

void DrawRoiCircle(cv::Mat& img, const CxShapeElementSnapshot& shape, const cv::Scalar& color)
{
    cv::Point center(static_cast<int>(shape.center_x), static_cast<int>(shape.center_y));
    int radius = static_cast<int>(shape.radius > 0 ? shape.radius : shape.radius_x);
    cv::circle(img, center, radius, color, 2);
}

void DrawRoiEllipse(cv::Mat& img, const CxShapeElementSnapshot& shape, const cv::Scalar& color)
{
    cv::Point center(static_cast<int>(shape.center_x), static_cast<int>(shape.center_y));
    cv::Size axes(
        static_cast<int>(std::max(1.0, shape.radius_x)),
        static_cast<int>(std::max(1.0, shape.radius_y)));
    cv::ellipse(img, center, axes, shape.angle_deg, 0.0, 360.0, color, 2);
}

void DrawMeasurePoints(cv::Mat& img, const CxShapeElementSnapshot& shape, const cv::Scalar& color)
{
    for (size_t i = 0; i + 1 < shape.points.size(); i += 2)
    {
        cv::Point pt(static_cast<int>(shape.points[i]), static_cast<int>(shape.points[i + 1]));
        cv::circle(img, pt, 2, color, -1);
    }
}

void DrawFitLine(cv::Mat& img, const CxShapeElementSnapshot& shape, const cv::Scalar& color)
{
    if (shape.points.size() >= 4)
    {
        cv::Point pt1(static_cast<int>(shape.points[0]), static_cast<int>(shape.points[1]));
        cv::Point pt2(static_cast<int>(shape.points[2]), static_cast<int>(shape.points[3]));
        cv::line(img, pt1, pt2, color, 3);
        cv::circle(img, pt1, 4, color, -1);
        cv::circle(img, pt2, 4, color, -1);
    }
}

static bool IsTorchModelResultRole(const std::string& role)
{
    return role == "model_best_result" ||
           role == "model_candidate" ||
           role == "model_segmentation_mask" ||
           role == "model_segmentation_contour";
}

void DrawLineGaugeFrame(cv::Mat& img, const CxShapeElementSnapshot& shape, const cv::Scalar& color)
{
    if (shape.points.size() < 4)
        return;

    const double x0 = shape.points[0];
    const double y0 = shape.points[1];
    const double x1 = shape.points[2];
    const double y1 = shape.points[3];
    const double dx = x1 - x0;
    const double dy = y1 - y0;
    const double len = std::sqrt(dx * dx + dy * dy);
    if (len < 1.0)
        return;

    const double half_width = std::max(1.0, shape.half_width);
    const double ndx = -dy / len;
    const double ndy = dx / len;

    std::vector<cv::Point> box;
    box.emplace_back(static_cast<int>(std::lround(x0 + ndx * half_width)),
                     static_cast<int>(std::lround(y0 + ndy * half_width)));
    box.emplace_back(static_cast<int>(std::lround(x1 + ndx * half_width)),
                     static_cast<int>(std::lround(y1 + ndy * half_width)));
    box.emplace_back(static_cast<int>(std::lround(x1 - ndx * half_width)),
                     static_cast<int>(std::lround(y1 - ndy * half_width)));
    box.emplace_back(static_cast<int>(std::lround(x0 - ndx * half_width)),
                     static_cast<int>(std::lround(y0 - ndy * half_width)));

    cv::polylines(img, box, true, color, 2);
    cv::line(
        img,
        cv::Point(static_cast<int>(std::lround(x0)), static_cast<int>(std::lround(y0))),
        cv::Point(static_cast<int>(std::lround(x1)), static_cast<int>(std::lround(y1))),
        color,
        1);
}

void DrawFitCircle(cv::Mat& img, const CxShapeElementSnapshot& shape, const cv::Scalar& color)
{
    cv::Point center(static_cast<int>(shape.center_x), static_cast<int>(shape.center_y));
    int radius = static_cast<int>(shape.radius_x);
    cv::circle(img, center, radius, color, 3);

    int cross_size = 8;
    cv::line(img, cv::Point(center.x - cross_size, center.y), cv::Point(center.x + cross_size, center.y), color, 2);
    cv::line(img, cv::Point(center.x, center.y - cross_size), cv::Point(center.x, center.y + cross_size), color, 2);
}

bool RenderCxShapeOverlay(
    const cv::Mat& source,
    const std::vector<CxShapeElementSnapshot>& shapes,
    CxOverlayLayer layer,
    cv::Mat& output,
    CxOverlayRenderResult& result)
{
    result = CxOverlayRenderResult{};

    if (source.empty())
    {
        result.reason = "source image is empty";
        return false;
    }

    output = source.clone();
    cv::Mat original = source.clone();

    cv::Scalar roi_color(0, 255, 255);
    cv::Scalar scan_color(0, 255, 0);
    cv::Scalar measure_color(0, 0, 255);
    cv::Scalar result_color(0, 255, 255);

    if (layer == CxOverlayLayer::RESULT)
    {
        roi_color = cv::Scalar(100, 150, 150);
        scan_color = cv::Scalar(100, 180, 100);
        measure_color = cv::Scalar(100, 50, 200);
        result_color = cv::Scalar(0, 200, 255);
    }

    for (const auto& shape : shapes)
    {
        if (shape.shape_kind.empty())
            continue;

        bool should_render = false;

        if (shape.semantic_role == "boundary_refined_points")
            should_render = true;

        if (layer == CxOverlayLayer::EVIDENCE)
        {
            if (shape.semantic_role == "roi" || shape.semantic_role == "scan" || shape.semantic_role == "measure_points")
                should_render = true;
            if (shape.semantic_role == "learn_roi" || shape.semantic_role == "search_roi")
                should_render = true;
            if (shape.semantic_role == "prompt_positive" ||
                shape.semantic_role == "prompt_negative" ||
                shape.semantic_role == "boundary" ||
                shape.semantic_role == "boundary_bbox")
                should_render = true;
            if (IsTorchModelResultRole(shape.semantic_role))
                should_render = true;
        }
        else if (layer == CxOverlayLayer::RESULT)
        {
            if (shape.semantic_role == "result" || shape.semantic_role == "measure_points")
                should_render = true;
            if (shape.semantic_role == "roi" || shape.semantic_role == "scan")
                should_render = true;
            if (shape.semantic_role == "expected_gt")
                should_render = true;
            if (shape.semantic_role == "prompt_positive" ||
                shape.semantic_role == "prompt_negative" ||
                shape.semantic_role == "boundary" ||
                shape.semantic_role == "boundary_bbox")
                should_render = true;
            if (IsTorchModelResultRole(shape.semantic_role))
                should_render = true;
        }
        else if (layer == CxOverlayLayer::TOOL_DISPLAY)
        {
            should_render = true;
        }

        if (!should_render)
            continue;

        result.rendered_element_count++;

        if (shape.semantic_role == "roi")
        {
            result.rendered_roi_count++;
            if (shape.shape_kind == "CircleShape")
                DrawRoiCircle(output, shape, roi_color);
            else if (shape.shape_kind == "EllipseShape")
                DrawRoiEllipse(output, shape, roi_color);
            else if (shape.shape_kind == "PolylineShape")
                DrawRoiPolyline(output, shape, roi_color);
            else if (shape.shape_kind == "RectShape")
                DrawRoiRectangle(output, shape, roi_color);
            else if (shape.shape_kind == "LineGaugeShape")
                DrawLineGaugeFrame(output, shape, roi_color);
        }
        else if (shape.semantic_role == "scan")
        {
            result.rendered_scan_count++;
            if (shape.shape_kind == "CircleShape")
                DrawRoiCircle(output, shape, scan_color);
            else if (shape.shape_kind == "EllipseShape")
                DrawRoiEllipse(output, shape, scan_color);
            else
                DrawRoiPolyline(output, shape, scan_color);
        }
        else if (shape.semantic_role == "measure_points")
        {
            result.rendered_measure_points_count += static_cast<int>(shape.points.size()) / 2;
            DrawMeasurePoints(output, shape, measure_color);
        }
else if (shape.semantic_role == "boundary_refined_points")
        {
            DrawMeasurePoints(output, shape, cv::Scalar(255, 80, 255));
        }
        else if (shape.semantic_role == "prompt_positive")
        {
            // Prompt points are input evidence, not measurement samples.  They
            // still belong in every artifact layer so a human can see why a
            // segmentation result was produced.
            DrawMeasurePoints(output, shape, cv::Scalar(80, 235, 100));
        }
        else if (shape.semantic_role == "prompt_negative")
        {
            DrawMeasurePoints(output, shape, cv::Scalar(80, 80, 245));
        }
        else if (shape.semantic_role == "boundary")
        {
            result.rendered_result_count++;
            DrawRoiPolyline(output, shape, cv::Scalar(255, 220, 40));
        }
        else if (shape.semantic_role == "boundary_bbox")
        {
            result.rendered_result_count++;
            DrawRoiRectangle(output, shape, cv::Scalar(255, 180, 40));
        }
        else if (shape.semantic_role == "result")
        {
            result.rendered_result_count++;
            if (shape.shape_kind == "CircleShape")
                DrawFitCircle(output, shape, result_color);
            else if (shape.shape_kind == "EllipseShape")
                DrawRoiEllipse(output, shape, result_color);
            else if (shape.shape_kind == "LineShape")
                DrawFitLine(output, shape, result_color);
            else if (shape.shape_kind == "RectShape")
                DrawRoiRectangle(output, shape, result_color);
        }
        else if (shape.semantic_role == "model_best_result" ||
                 shape.semantic_role == "model_candidate")
        {
            result.rendered_result_count++;
            DrawRoiRectangle(output, shape, result_color);
        }
        else if (shape.semantic_role == "model_segmentation_contour")
        {
            result.rendered_result_count++;
            DrawRoiPolyline(output, shape, cv::Scalar(255, 220, 40));
        }
        else if (shape.semantic_role == "model_segmentation_mask")
        {
            result.rendered_result_count++;
        }
        else if (shape.semantic_role == "learn_roi" || shape.semantic_role == "search_roi")
        {
            result.rendered_roi_count++;
            DrawRoiRectangle(output, shape, roi_color);
        }
        else if (shape.semantic_role == "expected_gt")
        {
            result.rendered_result_count++;
            cv::Scalar gt_color(255, 100, 100);
            DrawRoiRectangle(output, shape, gt_color);
        }
    }

    cv::Mat diff;
    cv::absdiff(output, original, diff);
    // countNonZero requires a single-channel matrix.  The overlays are BGR,
    // so flatten only for counting while preserving the rendered image.
    result.changed_pixel_count = cv::countNonZero(diff.reshape(1));

    if (result.changed_pixel_count == 0 && !shapes.empty())
    {
        cv::putText(output, "NO FIT RESULT", cv::Point(20, 40),
            cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 255), 2);
    }

    result.ok = true;
    return true;
}
