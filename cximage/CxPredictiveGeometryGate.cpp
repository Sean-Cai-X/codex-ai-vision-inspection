#include "CxPredictiveGeometryGate.h"

#include <cmath>
#include <algorithm>
#include <limits>
#include <opencv2/imgproc.hpp>
#include <vector>

#include <filesystem>
#include <fstream>
#include <iostream>

namespace
{
bool HasFiniteEvidence(const CxGeometryPrimitiveHypothesis& hypothesis)
{
    return std::isfinite(hypothesis.model_confidence) &&
        std::isfinite(hypothesis.evidence_strength) &&
        std::isfinite(hypothesis.classical_fit_residual_px) &&
        std::isfinite(hypothesis.residual_limit_px) &&
        std::isfinite(hypothesis.support) &&
        std::isfinite(hypothesis.support_limit) &&
        !hypothesis.geometry_type.empty() &&
        !hypothesis.source.empty() &&
        !hypothesis.tolerance_policy_ref.empty() &&
        hypothesis.residual_limit_px > 0.0;
}

bool ClassicalTolerancePass(const CxGeometryPrimitiveHypothesis& hypothesis)
{
    return hypothesis.classical_fit_residual_px <= hypothesis.residual_limit_px &&
        hypothesis.support >= hypothesis.support_limit;
}

CxGeometryPrimitiveHypothesis MakeSelfTestHypothesis()
{
    CxGeometryPrimitiveHypothesis hypothesis;
    hypothesis.geometry_type = "line";
    hypothesis.source = "geo_regression_head";
    hypothesis.tolerance_policy_ref = "controlled_line_tolerance_v1";
    hypothesis.model_confidence = 0.92;
    hypothesis.confidence_calibrated = true;
    hypothesis.confidence_calibration_ref = "selftest_calibration_v1";
    hypothesis.evidence_strength = 0.88;
    hypothesis.independent_image_evidence_verified = true;
    hypothesis.image_evidence_ref = "selftest_image_probe_v1";
    hypothesis.classical_fit_residual_px = 1.2;
    hypothesis.residual_limit_px = 6.0;
    hypothesis.support = 0.96;
    hypothesis.support_limit = 0.90;
    return hypothesis;
}
} // namespace

namespace
{
constexpr double kGeometryPi = 3.14159265358979323846;

double NormalizeAngle180(double angle_deg)
{
    double normalized = std::fmod(angle_deg, 180.0);
    if (normalized < 0.0)
        normalized += 180.0;
    return normalized;
}

double AngleError180(double lhs, double rhs)
{
    const double difference =
        std::abs(NormalizeAngle180(lhs) - NormalizeAngle180(rhs));
    return std::min(difference, 180.0 - difference);
}

double PointDistance(const cv::Point2d& lhs, const cv::Point2d& rhs)
{
    return std::hypot(lhs.x - rhs.x, lhs.y - rhs.y);
}

std::vector<cv::Point2d> ToDoublePoints(
    const std::vector<cv::Point>& contour)
{
    std::vector<cv::Point2d> points;
    points.reserve(contour.size());
    for (const cv::Point& point : contour)
        points.emplace_back(point.x, point.y);
    return points;
}

bool SummarizeResiduals(
    const std::vector<double>& residuals,
    double residual_limit_px,
    double& robust_mean,
    double& support)
{
    if (residuals.empty() || !std::isfinite(residual_limit_px) ||
        residual_limit_px <= 0.0)
        return false;

    std::vector<double> ordered = residuals;
    for (double residual : ordered)
    {
        if (!std::isfinite(residual))
            return false;
    }
    std::sort(ordered.begin(), ordered.end());
    const std::size_t retained =
        std::max<std::size_t>(
            1, static_cast<std::size_t>(
                   std::ceil(static_cast<double>(ordered.size()) * 0.90)));
    double total = 0.0;
    for (std::size_t index = 0; index < retained; ++index)
        total += ordered[index];
    robust_mean = total / static_cast<double>(retained);
    support = static_cast<double>(
        std::count_if(
            residuals.begin(), residuals.end(),
            [&](double residual) { return residual <= residual_limit_px; })) /
        static_cast<double>(residuals.size());
    return true;
}

bool FitCircleLeastSquares(
    const std::vector<cv::Point2d>& points,
    cv::Point2d& center,
    double& radius)
{
    if (points.size() < 3)
        return false;
    cv::Mat system(static_cast<int>(points.size()), 3, CV_64F);
    cv::Mat rhs(static_cast<int>(points.size()), 1, CV_64F);
    for (int row = 0; row < static_cast<int>(points.size()); ++row)
    {
        const double x = points[row].x;
        const double y = points[row].y;
        system.at<double>(row, 0) = 2.0 * x;
        system.at<double>(row, 1) = 2.0 * y;
        system.at<double>(row, 2) = 1.0;
        rhs.at<double>(row, 0) = x * x + y * y;
    }
    cv::Mat solution;
    if (!cv::solve(system, rhs, solution, cv::DECOMP_SVD))
        return false;
    center.x = solution.at<double>(0, 0);
    center.y = solution.at<double>(1, 0);
    const double squared_radius =
        solution.at<double>(2, 0) +
        center.x * center.x + center.y * center.y;
    if (!std::isfinite(squared_radius) || squared_radius <= 0.0)
        return false;
    radius = std::sqrt(squared_radius);
    return std::isfinite(radius);
}

void PopulateFitEvidence(
    const CxSegmentationGeometryFitOptions& options,
    CxGeometryPrimitiveHypothesis& hypothesis,
    double residual,
    double support)
{
    hypothesis.geometry_type = options.geometry_type;
    hypothesis.source = "seg_contour_fit";
    hypothesis.tolerance_policy_ref = options.tolerance_policy_ref;
    hypothesis.image_evidence_ref = options.image_evidence_ref;
    hypothesis.independent_image_evidence_verified =
        options.independent_image_evidence_verified;
    hypothesis.classical_fit_residual_px = residual;
    hypothesis.residual_limit_px = options.residual_limit_px;
    hypothesis.support = support;
    hypothesis.support_limit = options.support_limit;
    hypothesis.classical_verified = true;
    hypothesis.evidence_strength = std::clamp(
        support * std::exp(
            -residual / std::max(options.residual_limit_px, 1.0e-9)),
        0.0, 1.0);
}

bool FitCircleGeometry(
    const std::vector<cv::Point2d>& points,
    const CxSegmentationGeometryFitOptions& options,
    CxGeometryPrimitiveHypothesis& hypothesis)
{
    cv::Point2d center;
    double radius = 0.0;
    if (!FitCircleLeastSquares(points, center, radius))
        return false;
    std::vector<double> residuals;
    residuals.reserve(points.size());
    for (const cv::Point2d& point : points)
        residuals.push_back(std::abs(PointDistance(point, center) - radius));
    double residual = 0.0;
    double support = 0.0;
    if (!SummarizeResiduals(
            residuals, options.residual_limit_px, residual, support))
        return false;
    hypothesis.center = center;
    hypothesis.radius = radius;
    hypothesis.axes = cv::Size2d(radius, radius);
    PopulateFitEvidence(options, hypothesis, residual, support);
    return true;
}

bool FitEllipseGeometry(
    const std::vector<cv::Point2d>& points,
    const CxSegmentationGeometryFitOptions& options,
    CxGeometryPrimitiveHypothesis& hypothesis)
{
    if (points.size() < 5)
        return false;
    std::vector<cv::Point2f> fit_points;
    fit_points.reserve(points.size());
    for (const cv::Point2d& point : points)
        fit_points.emplace_back(
            static_cast<float>(point.x), static_cast<float>(point.y));
    const cv::RotatedRect fitted = cv::fitEllipse(fit_points);
    double major = fitted.size.width * 0.5;
    double minor = fitted.size.height * 0.5;
    double angle_deg = fitted.angle;
    if (minor > major)
    {
        std::swap(major, minor);
        angle_deg += 90.0;
    }
    if (major <= 0.0 || minor <= 0.0)
        return false;
    angle_deg = NormalizeAngle180(angle_deg);
    const double angle_rad = angle_deg * kGeometryPi / 180.0;
    const double cosine = std::cos(angle_rad);
    const double sine = std::sin(angle_rad);
    std::vector<double> residuals;
    residuals.reserve(points.size());
    for (const cv::Point2d& point : points)
    {
        const double dx = point.x - fitted.center.x;
        const double dy = point.y - fitted.center.y;
        const double local_x = cosine * dx + sine * dy;
        const double local_y = -sine * dx + cosine * dy;
        const double normalized_radius = std::sqrt(
            local_x * local_x / (major * major) +
            local_y * local_y / (minor * minor));
        residuals.push_back(
            std::abs(normalized_radius - 1.0) * minor);
    }
    double residual = 0.0;
    double support = 0.0;
    if (!SummarizeResiduals(
            residuals, options.residual_limit_px, residual, support))
        return false;
    hypothesis.center = fitted.center;
    hypothesis.axes = cv::Size2d(major, minor);
    hypothesis.angle_deg = angle_deg;
    PopulateFitEvidence(options, hypothesis, residual, support);
    return true;
}

double NormalizeAngleSigned90(double angle_deg)
{
    while (angle_deg >= 90.0)
        angle_deg -= 180.0;
    while (angle_deg < -90.0)
        angle_deg += 180.0;
    return angle_deg;
}

bool FitOrientedBoxGeometry(
    const std::vector<cv::Point2d>& points,
    const CxSegmentationGeometryFitOptions& options,
    CxGeometryPrimitiveHypothesis& hypothesis)
{
    if (points.size() < 3)
        return false;

    std::vector<cv::Point2f> fit_points;
    fit_points.reserve(points.size());
    for (const cv::Point2d& point : points)
        fit_points.emplace_back(
            static_cast<float>(point.x), static_cast<float>(point.y));

    const cv::RotatedRect fitted = cv::minAreaRect(fit_points);
    double width = fitted.size.width;
    double height = fitted.size.height;
    double angle_deg = fitted.angle;
    if (height > width)
    {
        std::swap(width, height);
        angle_deg += 90.0;
    }
    if (!std::isfinite(width) || !std::isfinite(height) ||
        width <= 0.0 || height <= 0.0)
        return false;

    // The long axis is canonical: width >= height and angle in [-90, 90).
    angle_deg = NormalizeAngleSigned90(angle_deg);
    const double angle_rad = angle_deg * kGeometryPi / 180.0;
    const cv::Point2d major(std::cos(angle_rad), std::sin(angle_rad));
    const cv::Point2d minor(-std::sin(angle_rad), std::cos(angle_rad));
    const cv::Point2d center(fitted.center.x, fitted.center.y);
    const double half_width = width * 0.5;
    const double half_height = height * 0.5;

    std::vector<double> residuals;
    residuals.reserve(points.size());
    for (const cv::Point2d& point : points)
    {
        const cv::Point2d delta = point - center;
        const double local_x = std::abs(delta.dot(major));
        const double local_y = std::abs(delta.dot(minor));
        const double outside_x = std::max(0.0, local_x - half_width);
        const double outside_y = std::max(0.0, local_y - half_height);
        const double residual = outside_x > 0.0 || outside_y > 0.0
            ? std::hypot(outside_x, outside_y)
            : std::min(half_width - local_x, half_height - local_y);
        residuals.push_back(residual);
    }

    double residual = 0.0;
    double support = 0.0;
    if (!SummarizeResiduals(
            residuals, options.residual_limit_px, residual, support))
        return false;

    hypothesis.center = center;
    // OBB axes carry the full canonical width and height, unlike ellipse axes.
    hypothesis.axes = cv::Size2d(width, height);
    hypothesis.angle_deg = angle_deg;
    hypothesis.ordered_points = {
        center - major * half_width - minor * half_height,
        center + major * half_width - minor * half_height,
        center + major * half_width + minor * half_height,
        center - major * half_width + minor * half_height};
    PopulateFitEvidence(options, hypothesis, residual, support);
    return true;
}

bool FitLineGeometry(
    const std::vector<cv::Point2d>& points,
    const CxSegmentationGeometryFitOptions& options,
    CxGeometryPrimitiveHypothesis& hypothesis)
{
    if (points.size() < 2)
        return false;

    std::vector<cv::Point2f> fit_points;
    fit_points.reserve(points.size());
    for (const cv::Point2d& point : points)
        fit_points.emplace_back(
            static_cast<float>(point.x), static_cast<float>(point.y));

    cv::Vec4f initial_fit;
    cv::fitLine(
        fit_points, initial_fit, cv::DIST_HUBER, 0.0, 0.01, 0.01);
    cv::Point2d initial_direction(initial_fit[0], initial_fit[1]);
    const double initial_length =
        std::hypot(initial_direction.x, initial_direction.y);
    if (!std::isfinite(initial_length) || initial_length <= 0.0)
        return false;
    initial_direction.x /= initial_length;
    initial_direction.y /= initial_length;
    const cv::Point2d initial_origin(initial_fit[2], initial_fit[3]);
    const cv::Point2d initial_normal(
        -initial_direction.y, initial_direction.x);

    struct ProjectedPoint
    {
        double longitudinal = 0.0;
        double normal = 0.0;
    };
    std::vector<ProjectedPoint> projected;
    projected.reserve(points.size());
    for (const cv::Point2d& point : points)
    {
        const cv::Point2d delta = point - initial_origin;
        projected.push_back(
            {delta.dot(initial_direction), delta.dot(initial_normal)});
    }
    std::sort(
        projected.begin(), projected.end(),
        [](const ProjectedPoint& lhs, const ProjectedPoint& rhs) {
            return lhs.longitudinal < rhs.longitudinal;
        });

    // Collapse both sides of a rasterized thick line into one center sample
    // per longitudinal pixel. Support then measures centerline agreement,
    // independently of the annotation stroke width.
    std::vector<double> normal_coordinates;
    normal_coordinates.reserve(projected.size());
    for (const ProjectedPoint& point : projected)
        normal_coordinates.push_back(point.normal);
    std::sort(normal_coordinates.begin(), normal_coordinates.end());
    const std::size_t lower_index =
        normal_coordinates.size() / 10;
    const std::size_t upper_index =
        (normal_coordinates.size() - 1) * 9 / 10;
    const double robust_stroke_width = std::max(
        0.0, normal_coordinates[upper_index] -
                 normal_coordinates[lower_index]);
    const double longitudinal_bin_width =
        std::max(1.0, robust_stroke_width * 0.5);

    std::vector<cv::Point2f> centerline_points;

    centerline_points.reserve(projected.size());
    std::size_t first_index = 0;
    while (first_index < projected.size())
    {
        const double bin = std::floor(
            projected[first_index].longitudinal /
            longitudinal_bin_width);
        std::size_t last_index = first_index;
        double longitudinal_total = 0.0;
        double minimum_normal = std::numeric_limits<double>::infinity();
        double maximum_normal = -std::numeric_limits<double>::infinity();
        while (last_index < projected.size() &&
               std::floor(
                   projected[last_index].longitudinal /
                   longitudinal_bin_width) == bin)

        {
            longitudinal_total += projected[last_index].longitudinal;
            minimum_normal =
                std::min(minimum_normal, projected[last_index].normal);
            maximum_normal =
                std::max(maximum_normal, projected[last_index].normal);
            ++last_index;
        }
        const double count =
            static_cast<double>(last_index - first_index);
        const double longitudinal_center = longitudinal_total / count;
        const double normal_center =
            (minimum_normal + maximum_normal) * 0.5;
        const cv::Point2d center =
            initial_origin + initial_direction * longitudinal_center +
            initial_normal * normal_center;
        centerline_points.emplace_back(
            static_cast<float>(center.x), static_cast<float>(center.y));
        first_index = last_index;
    }
    if (centerline_points.size() < 2)
        return false;

    cv::Vec4f fitted;
    cv::fitLine(
        centerline_points, fitted, cv::DIST_HUBER, 0.0, 0.01, 0.01);
    cv::Point2d direction(fitted[0], fitted[1]);
    const double length = std::hypot(direction.x, direction.y);
    if (!std::isfinite(length) || length <= 0.0)
        return false;
    direction.x /= length;
    direction.y /= length;
    const cv::Point2d origin(fitted[2], fitted[3]);

    std::vector<double> residuals;
    residuals.reserve(centerline_points.size());
    for (const cv::Point2f& point : centerline_points)
    {
        const cv::Point2d delta(
            static_cast<double>(point.x) - origin.x,
            static_cast<double>(point.y) - origin.y);
        residuals.push_back(std::abs(
            delta.x * direction.y - delta.y * direction.x));
    }

    double residual = 0.0;
    double support = 0.0;
    if (!SummarizeResiduals(
            residuals, options.residual_limit_px, residual, support))
        return false;

    double minimum = std::numeric_limits<double>::infinity();
    double maximum = -std::numeric_limits<double>::infinity();
    for (const cv::Point2d& point : points)
    {
        const double projection = (point - origin).dot(direction);
        minimum = std::min(minimum, projection);
        maximum = std::max(maximum, projection);
    }
    cv::Point2d first = origin + direction * minimum;
    cv::Point2d last = origin + direction * maximum;
    if (first.x > last.x || (first.x == last.x && first.y > last.y))
        std::swap(first, last);
    hypothesis.center = (first + last) * 0.5;
    hypothesis.angle_deg = NormalizeAngle180(
        std::atan2(direction.y, direction.x) * 180.0 / kGeometryPi);
    hypothesis.ordered_points = {first, last};
    PopulateFitEvidence(options, hypothesis, residual, support);
    return true;
}


bool RunSegmentationGeometryFitChecks()
{
    CxSegmentationGeometryFitOptions options;
    options.tolerance_policy_ref = "selftest_pixel_tolerance_v1";
    options.residual_limit_px = 1.5;
    options.support_limit = 0.95;

    std::vector<cv::Point> circle;
    for (int index = 0; index < 360; ++index)
    {
        const double angle = index * kGeometryPi / 180.0;
        circle.emplace_back(
            cvRound(100.0 + 40.0 * std::cos(angle)),
            cvRound(80.0 + 40.0 * std::sin(angle)));
    }
    options.geometry_type = "circle";
    CxSegmentationGeometryFitResult circle_result;
    const bool circle_ok = FitCxSegmentationContourGeometry(
        circle, options, circle_result);

    std::vector<cv::Point> ellipse;
    const double ellipse_angle = 30.0 * kGeometryPi / 180.0;
    for (int index = 0; index < 360; ++index)
    {
        const double parameter = index * kGeometryPi / 180.0;
        const double x = 50.0 * std::cos(parameter);
        const double y = 25.0 * std::sin(parameter);
        ellipse.emplace_back(
            cvRound(120.0 + std::cos(ellipse_angle) * x -
                    std::sin(ellipse_angle) * y),
            cvRound(90.0 + std::sin(ellipse_angle) * x +
                    std::cos(ellipse_angle) * y));
    }
    options.geometry_type = "ellipse";
    CxSegmentationGeometryFitResult ellipse_result;
    const bool ellipse_ok = FitCxSegmentationContourGeometry(
        ellipse, options, ellipse_result);

    std::vector<cv::Point> line;
    const cv::Point2d line_first(20.0, 30.0);
    const cv::Point2d line_last(180.0, 110.0);
    for (int index = 0; index <= 160; ++index)
    {
        const double t = static_cast<double>(index) / 160.0;
        const cv::Point2d point =
            line_first * (1.0 - t) + line_last * t;
        line.emplace_back(cvRound(point.x), cvRound(point.y));
    }
    options.geometry_type = "line";
    CxSegmentationGeometryFitResult line_result;
    const bool line_ok = FitCxSegmentationContourGeometry(
        line, options, line_result);

    cv::Mat thick_line_mask = cv::Mat::zeros(160, 220, CV_8UC1);
    cv::line(
        thick_line_mask, line_first, line_last, cv::Scalar(255), 9,
        cv::LINE_8);
    std::vector<std::vector<cv::Point>> thick_line_contours;
    cv::findContours(
        thick_line_mask, thick_line_contours, cv::RETR_EXTERNAL,
        cv::CHAIN_APPROX_NONE);
    CxSegmentationGeometryFitOptions thick_line_options = options;
    thick_line_options.residual_limit_px = 2.0;
    thick_line_options.support_limit = 0.80;

    CxSegmentationGeometryFitResult thick_line_result;
    const bool thick_line_ok =
        thick_line_contours.size() == 1 &&
        FitCxSegmentationContourGeometry(
            thick_line_contours.front(), thick_line_options,
            thick_line_result);

    const double direct_line_error =
        line_result.hypothesis.ordered_points.size() == 2
            ? std::max(
                  PointDistance(
                      line_result.hypothesis.ordered_points[0], line_first),
                  PointDistance(
                      line_result.hypothesis.ordered_points[1], line_last))
            : std::numeric_limits<double>::infinity();
    const double reversed_line_error =
        line_result.hypothesis.ordered_points.size() == 2
            ? std::max(
                  PointDistance(
                      line_result.hypothesis.ordered_points[0], line_last),
                  PointDistance(
                      line_result.hypothesis.ordered_points[1], line_first))
            : std::numeric_limits<double>::infinity();

    std::cout
        << "segmentation_geometry_fit_selftest"
        << " circle_ok=" << circle_ok
        << " circle_residual="
        << circle_result.hypothesis.classical_fit_residual_px
        << " circle_support=" << circle_result.hypothesis.support
        << " ellipse_ok=" << ellipse_ok
        << " ellipse_residual="
        << ellipse_result.hypothesis.classical_fit_residual_px
        << " ellipse_support=" << ellipse_result.hypothesis.support
        << " line_ok=" << line_ok
        << " line_residual="
        << line_result.hypothesis.classical_fit_residual_px
        << " line_support=" << line_result.hypothesis.support
        << " line_endpoint_error="
        << std::min(direct_line_error, reversed_line_error)
        << " thick_line_ok=" << thick_line_ok
        << " thick_line_residual="
        << thick_line_result.hypothesis.classical_fit_residual_px
        << " thick_line_support="
        << thick_line_result.hypothesis.support
        << " thick_line_angle="
        << thick_line_result.hypothesis.angle_deg << "\n";

    return circle_ok &&

        PointDistance(
            circle_result.hypothesis.center, cv::Point2d(100.0, 80.0)) < 0.5 &&
        std::abs(circle_result.hypothesis.radius - 40.0) < 0.5 &&
        circle_result.hypothesis.support >= options.support_limit &&
        ellipse_ok &&
        PointDistance(
            ellipse_result.hypothesis.center, cv::Point2d(120.0, 90.0)) < 0.75 &&
        std::abs(ellipse_result.hypothesis.axes.width - 50.0) < 0.75 &&
        std::abs(ellipse_result.hypothesis.axes.height - 25.0) < 0.75 &&
        AngleError180(ellipse_result.hypothesis.angle_deg, 30.0) < 1.0 &&
        ellipse_result.hypothesis.support >= options.support_limit &&
        line_ok && std::min(direct_line_error, reversed_line_error) < 1.5 &&
        line_result.hypothesis.support >= options.support_limit &&
        thick_line_ok &&
        thick_line_result.hypothesis.classical_fit_residual_px <=
            options.residual_limit_px &&
        thick_line_result.hypothesis.support >=
            thick_line_options.support_limit &&

        AngleError180(thick_line_result.hypothesis.angle_deg, 26.565) < 1.0;

}
} // namespace

bool FitCxSegmentationContourGeometry(
    const std::vector<cv::Point>& contour,
    const CxSegmentationGeometryFitOptions& options,
    CxSegmentationGeometryFitResult& result)
{
    result = {};
    if (options.geometry_type.empty())
    {
        result.status = "GEOMETRY_TYPE_REQUIRED";
        result.reason = "geometry type must be supplied by the asset or caller";
        return false;
    }
    if (!std::isfinite(options.residual_limit_px) ||
        options.residual_limit_px <= 0.0 ||
        !std::isfinite(options.support_limit) ||
        options.support_limit < 0.0 || options.support_limit > 1.0 ||
        options.tolerance_policy_ref.empty())
    {
        result.status = "INVALID_FIT_OPTIONS";
        result.reason = "finite tolerance, support, and policy are required";
        return false;
    }
    const std::vector<cv::Point2d> points = ToDoublePoints(contour);
    if (points.empty())
    {
        result.status = "CONTOUR_EMPTY";
        result.reason = "segmentation contour is empty";
        return false;
    }

    bool fitted = false;
    try
    {
        if (options.geometry_type == "circle")
            fitted = FitCircleGeometry(points, options, result.hypothesis);
        else if (options.geometry_type == "ellipse")
            fitted = FitEllipseGeometry(points, options, result.hypothesis);
        else if (options.geometry_type == "oriented_box")
            fitted = FitOrientedBoxGeometry(points, options, result.hypothesis);
        else if (options.geometry_type == "line")
            fitted = FitLineGeometry(points, options, result.hypothesis);
        else
        {
            result.status = "UNSUPPORTED_GEOMETRY_TYPE";
            result.reason =
                "contour fitter supports circle, ellipse, oriented_box, and line";
            return false;
        }
    }
    catch (const cv::Exception& exception)
    {
        result.status = "GEOMETRY_FIT_EXCEPTION";
        result.reason = exception.what();
        return false;
    }

    if (!fitted)
    {
        result.status = "GEOMETRY_FIT_FAILED";
        result.reason = "contour does not contain enough valid geometry";
        return false;
    }
    result.complete = true;
    const bool within_tolerance =
        result.hypothesis.classical_fit_residual_px <=
            result.hypothesis.residual_limit_px &&
        result.hypothesis.support >= result.hypothesis.support_limit;
    result.status = within_tolerance
        ? "FIT_WITHIN_TOLERANCE"
        : "FIT_OUTSIDE_TOLERANCE";
    result.reason = within_tolerance
        ? "contour fit satisfies residual and support limits"
        : "contour fit is available but does not satisfy tolerance";
    return true;
}


const char* ToString(CxGeometryConclusionStatus status)
{
    switch (status)
    {
    case CxGeometryConclusionStatus::DirectMeasurement:
        return "DIRECT_MEASUREMENT";
    case CxGeometryConclusionStatus::ClampedMeasurement:
        return "CLAMPED_MEASUREMENT";
    case CxGeometryConclusionStatus::PredictiveHypothesis:
        return "PREDICTIVE_HYPOTHESIS";
    case CxGeometryConclusionStatus::Undeterminable:
    default:
        return "UNDETERMINABLE";
    }
}

CxPredictiveGeometryGateResult EvaluateCxPredictiveGeometryGate(
    const CxGeometryPrimitiveHypothesis& hypothesis,
    const CxPredictiveGeometryGatePolicy& policy)
{
    CxPredictiveGeometryGateResult result;
    if (!HasFiniteEvidence(hypothesis))
    {
        result.reason = "required typed geometry, calibration, evidence, or tolerance facts are missing";
        return result;
    }

    const bool tolerance_pass = ClassicalTolerancePass(hypothesis);
    if (hypothesis.classical_verified &&
        hypothesis.independent_image_evidence_verified &&
        !hypothesis.image_evidence_ref.empty() && tolerance_pass &&
        hypothesis.evidence_strength >= policy.minimum_evidence_strength)
    {
        result.status = CxGeometryConclusionStatus::DirectMeasurement;
        result.reason = "classical image evidence satisfies residual, support, and tolerance policy";
        result.measurement_allowed = true;
        result.human_review_required = false;
        return result;
    }

    if (hypothesis.confidence_calibrated &&
        !hypothesis.confidence_calibration_ref.empty() &&
        hypothesis.independent_image_evidence_verified &&
        !hypothesis.image_evidence_ref.empty() &&
        hypothesis.model_confidence >= policy.direct_confidence &&
        hypothesis.evidence_strength >= policy.minimum_evidence_strength &&
        tolerance_pass)
    {
        result.status = CxGeometryConclusionStatus::DirectMeasurement;
        result.reason = "calibrated model hypothesis and independent image evidence satisfy tolerance policy";
        result.measurement_allowed = true;
        result.human_review_required = false;
        return result;
    }

    if (hypothesis.clamp_verified &&
        hypothesis.independent_image_evidence_verified &&
        !hypothesis.image_evidence_ref.empty() &&
        hypothesis.clamp_score >= policy.minimum_clamp_score && tolerance_pass)
    {
        result.status = CxGeometryConclusionStatus::ClampedMeasurement;
        result.reason = "positive-negative or classical clamp evidence verified the hypothesis";
        result.measurement_allowed = true;
        result.human_review_required = false;
        return result;
    }

    if (hypothesis.confidence_calibrated &&
        !hypothesis.confidence_calibration_ref.empty() &&
        hypothesis.model_confidence >= policy.minimum_predictive_confidence)
    {
        result.status = CxGeometryConclusionStatus::PredictiveHypothesis;
        result.reason = "geometry remains plausible but measurement evidence is insufficient";
        result.measurement_allowed = false;
        result.human_review_required = true;
        return result;
    }

    result.reason = "neither calibrated model confidence nor image evidence is admissible";
    return result;
}

int RunCxPredictiveGeometryGateSelfTest(const std::string& output_directory)
{
    if (output_directory.empty())
        return 2;
    std::filesystem::create_directories(output_directory);
    const bool contour_fit_pass = RunSegmentationGeometryFitChecks();


    CxGeometryPrimitiveHypothesis direct = MakeSelfTestHypothesis();
    direct.classical_verified = true;
    const CxPredictiveGeometryGateResult direct_result = EvaluateCxPredictiveGeometryGate(direct);

    CxGeometryPrimitiveHypothesis model_direct = MakeSelfTestHypothesis();
    const CxPredictiveGeometryGateResult model_direct_result =
        EvaluateCxPredictiveGeometryGate(model_direct);

    CxGeometryPrimitiveHypothesis confidence_only = MakeSelfTestHypothesis();
    confidence_only.independent_image_evidence_verified = false;
    confidence_only.image_evidence_ref.clear();
    const CxPredictiveGeometryGateResult confidence_only_result =
        EvaluateCxPredictiveGeometryGate(confidence_only);

    CxGeometryPrimitiveHypothesis clamped = MakeSelfTestHypothesis();
    clamped.model_confidence = 0.60;
    clamped.evidence_strength = 0.45;
    clamped.clamp_verified = true;
    clamped.clamp_score = 0.82;
    const CxPredictiveGeometryGateResult clamped_result = EvaluateCxPredictiveGeometryGate(clamped);

    CxGeometryPrimitiveHypothesis predictive = MakeSelfTestHypothesis();
    predictive.model_confidence = 0.58;
    predictive.evidence_strength = 0.20;
    predictive.classical_fit_residual_px = 8.0;
    const CxPredictiveGeometryGateResult predictive_result = EvaluateCxPredictiveGeometryGate(predictive);

    CxGeometryPrimitiveHypothesis undeterminable = MakeSelfTestHypothesis();
    undeterminable.model_confidence = 0.10;
    undeterminable.confidence_calibrated = false;
    undeterminable.confidence_calibration_ref.clear();
    undeterminable.evidence_strength = 0.10;
    undeterminable.classical_fit_residual_px = 20.0;
    const CxPredictiveGeometryGateResult undeterminable_result =
        EvaluateCxPredictiveGeometryGate(undeterminable);

    const bool pass = contour_fit_pass &&
        direct_result.status == CxGeometryConclusionStatus::DirectMeasurement &&
        direct_result.measurement_allowed &&
        model_direct_result.status == CxGeometryConclusionStatus::DirectMeasurement &&
        model_direct_result.measurement_allowed &&
        confidence_only_result.status == CxGeometryConclusionStatus::PredictiveHypothesis &&
        !confidence_only_result.measurement_allowed &&
        clamped_result.status == CxGeometryConclusionStatus::ClampedMeasurement &&
        clamped_result.measurement_allowed &&
        predictive_result.status == CxGeometryConclusionStatus::PredictiveHypothesis &&
        !predictive_result.measurement_allowed && predictive_result.human_review_required &&
        undeterminable_result.status == CxGeometryConclusionStatus::Undeterminable &&
        !undeterminable_result.measurement_allowed;

    const std::filesystem::path report_path =
        std::filesystem::path(output_directory) / "predictive_geometry_gate_selftest.json";
    std::ofstream report(report_path, std::ios::trunc);
    report << "{\n"
           << "  \"schema\": \"cxvision.predictive_geometry_gate_selftest.v1\",\n"
           << "  \"segmentation_contour_fit\": "
           << (contour_fit_pass ? "true" : "false") << ",\n"
           << "  \"direct\": \"" << ToString(direct_result.status) << "\",\n"
           << "  \"model_direct_with_independent_evidence\": \""
           << ToString(model_direct_result.status) << "\",\n"
           << "  \"confidence_only_without_independent_evidence\": \""
           << ToString(confidence_only_result.status) << "\",\n"
           << "  \"clamped\": \"" << ToString(clamped_result.status) << "\",\n"
           << "  \"predictive\": \"" << ToString(predictive_result.status) << "\",\n"
           << "  \"undeterminable\": \"" << ToString(undeterminable_result.status) << "\",\n"
           << "  \"predictive_measurement_allowed\": "
           << (predictive_result.measurement_allowed ? "true" : "false") << ",\n"
           << "  \"conclusion\": \""
           << (pass ? "PREDICTIVE_GEOMETRY_GATE_SELFTEST_PASS" : "FAIL") << "\"\n"
           << "}\n";
    const bool report_ok = report.good();
    report.close();
    const bool final_pass = pass && report_ok;
    std::cout << "predictive_geometry_gate_selftest_ok=" << (final_pass ? "true" : "false") << "\n"
              << "conclusion="
              << (final_pass ? "PREDICTIVE_GEOMETRY_GATE_SELFTEST_PASS" : "FAIL") << "\n"
              << "report=" << report_path.string() << "\n";
    return final_pass ? 0 : 1;
}
