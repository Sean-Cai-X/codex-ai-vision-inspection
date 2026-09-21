#include "pch.h"

#include "findobject.h"
#include "ImageAnnotationLayer.h"
#include "RectShape.h"

#include "CircleShape.h"
#include "EllipseShape.h"
#include "PolylineShape.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>
#include "imagemanager.h"
#include "occtinclude.h"

#include <opencv2/core/core.hpp>
#include <opencv2/core/version.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/video/tracking.hpp>

#include <queue>

typedef unsigned char BYTE;

namespace {

constexpr double kFindObjectPi = 3.14159265358979323846;

double NormalizeFindObjectAngle180(double degrees)
{
  while (degrees < 0.0)
    degrees += 180.0;
  while (degrees >= 180.0)
    degrees -= 180.0;
  return degrees;
}

double GWPixelConfigurationPerimeter(
    const cv::Mat &mask, double hx, double hy,
    std::array<std::uint64_t, 16> &counts)
{
  cv::Mat padded;
  cv::copyMakeBorder(mask, padded, 1, 1, 1, 1, cv::BORDER_CONSTANT,
                     cv::Scalar(0));
  const double diagonal = std::hypot(hx, hy);
  std::array<double, 16> contribution = {};
  for (const int code : {1, 2, 4, 7, 8, 11, 13, 14})
    contribution[code] = 0.5 * diagonal;
  contribution[3] = contribution[12] = hx;
  contribution[6] = contribution[9] = hy;
  contribution[5] = contribution[10] = diagonal;

  double perimeter = 0.0;
  for (int y = 0; y + 1 < padded.rows; ++y)
  {
    const uchar *row0 = padded.ptr<uchar>(y);
    const uchar *row1 = padded.ptr<uchar>(y + 1);
    for (int x = 0; x + 1 < padded.cols; ++x)
    {
      const int code = (row0[x] != 0 ? 1 : 0) |
                       (row0[x + 1] != 0 ? 2 : 0) |
                       (row1[x + 1] != 0 ? 4 : 0) |
                       (row1[x] != 0 ? 8 : 0);
      ++counts[code];
      perimeter += contribution[code];
    }
  }
  return perimeter;
}

void MeasureFindObjectIntensity(const cv::Mat &mask,
                                const cv::Point &origin,
                                const cv::Mat &source,
                                FindObjectMeasurementSnapshot &output)
{
  if (source.empty() || origin.x < 0 || origin.y < 0 ||
      origin.x + mask.cols > source.cols ||
      origin.y + mask.rows > source.rows)
    return;

  const cv::Rect requested(origin.x, origin.y, mask.cols, mask.rows);
  const cv::Mat source_roi = source(requested);
  cv::Mat gray;
  if (source_roi.channels() == 1)
    gray = source_roi;
  else
    cv::cvtColor(source_roi, gray, cv::COLOR_BGR2GRAY);
  if (gray.depth() != CV_64F)
    gray.convertTo(gray, CV_64F);

  cv::Scalar mean;
  cv::Scalar stddev;
  cv::meanStdDev(gray, mean, stddev, mask);
  cv::minMaxLoc(gray, &output.intensity_min, &output.intensity_max,
                nullptr, nullptr, mask);
  output.intensity_mean = mean[0];
  output.intensity_stddev = stddev[0];
  if (stddev[0] > std::numeric_limits<double>::epsilon())
  {
    double third_moment = 0.0;
    std::uint64_t count = 0;
    for (int y = 0; y < mask.rows; ++y)
    {
      const uchar *mask_row = mask.ptr<uchar>(y);
      const double *gray_row = gray.ptr<double>(y);
      for (int x = 0; x < mask.cols; ++x)
      {
        if (mask_row[x] == 0)
          continue;
        const double z = (gray_row[x] - mean[0]) / stddev[0];
        third_moment += z * z * z;
        ++count;
      }
    }
    if (count > 0)
      output.intensity_skewness =
          third_moment / static_cast<double>(count);
  }
  output.intensity_valid = true;
}

} // namespace
struct FindObjectBackgroundBuild
{
  cv::Mat residual;
  std::string method = "none";
  bool valid = true;
  int sample_count = 0;
  double baseline_mean = 0.0;
  bool corrected = false;
};

FindObjectBackgroundBuild BuildFindObjectBackgroundResidual(
    const cv::Mat& channel, const FindObjectMeasurementConfig& config)
{
  FindObjectBackgroundBuild output;
  if (channel.empty())
  {
    output.valid = false;
    output.method = "invalid_empty_input";
    return output;
  }
  cv::Mat gray;
  channel.convertTo(gray, CV_32F);
  if (config.background_method == FindObjectBackgroundMethod::None)
  {
    output.residual = gray;
    return output;
  }

  cv::Mat baseline;
  if (config.background_method == FindObjectBackgroundMethod::RoiBorderRobust)
  {
    const int border = std::max(1, std::min(
        config.background_border_width_px, std::min(gray.rows, gray.cols) / 2));
    std::vector<float> samples;
    for (int y = 0; y < gray.rows; ++y)
    {
      const float* row = gray.ptr<float>(y);
      for (int x = 0; x < gray.cols; ++x)
      {
        if (x < border || y < border || x >= gray.cols - border ||
            y >= gray.rows - border)
          samples.push_back(row[x]);
      }
    }
    if (samples.empty())
    {
      output.valid = false;
      output.method = "roi_border_robust";
      output.residual = gray;
      return output;
    }
    const std::size_t middle = samples.size() / 2U;
    std::nth_element(samples.begin(), samples.begin() + middle, samples.end());
    baseline = cv::Mat(gray.size(), CV_32F, cv::Scalar(samples[middle]));
    output.method = "roi_border_robust";
    output.sample_count = static_cast<int>(samples.size());
  }
  else
  {
    const int radius = std::max(1, config.background_morphology_radius_px);
    const cv::Mat kernel = cv::getStructuringElement(
        cv::MORPH_ELLIPSE, cv::Size(radius * 2 + 1, radius * 2 + 1));
    cv::morphologyEx(gray, baseline, cv::MORPH_OPEN, kernel);
    output.method = "morphological_opening";
    output.sample_count = gray.rows * gray.cols;
  }
  output.baseline_mean = cv::mean(baseline)[0];
  output.residual = gray - baseline;
  output.corrected = true;
  return output;
}

void ApplyFindObjectSubpixelRefinement(
    const cv::Mat& source_image, const cv::Mat& component_mask,
    const cv::Point& origin, const FindObjectMeasurementConfig& config,
    double threshold, FindObjectMeasurementSnapshot& output)
{
  if (!config.subpixel_enabled)
    return;
  if (source_image.empty() || component_mask.empty() || origin.x < 1 ||
      origin.y < 1 || origin.x + component_mask.cols >= source_image.cols ||
      origin.y + component_mask.rows >= source_image.rows)
  {
    output.subpixel_failure_reason = "source_or_component_outside_bounds";
    return;
  }
  cv::Mat source_roi = source_image(cv::Rect(
      origin.x, origin.y, component_mask.cols, component_mask.rows));
  cv::Mat gray;
  if (source_roi.channels() == 1)
    gray = source_roi;
  else
    cv::cvtColor(source_roi, gray, cv::COLOR_BGR2GRAY);
  gray.convertTo(gray, CV_32F);

  std::vector<std::vector<cv::Point>> contours;
  cv::findContours(component_mask, contours, cv::RETR_EXTERNAL,
                   cv::CHAIN_APPROX_NONE);
  if (contours.empty() || contours.front().size() < 3U)
  {
    output.subpixel_failure_reason = "integer_contour_unavailable";
    return;
  }
  double gradient_sum = 0.0;
  for (const cv::Point& point : contours.front())
  {
    ++output.subpixel_candidate_point_count;
    if (point.x <= 0 || point.y <= 0 || point.x + 1 >= gray.cols ||
        point.y + 1 >= gray.rows)
    {
      ++output.subpixel_rejected_out_of_bounds_count;
      continue;
    }
    const double gx = 0.5 * (gray.at<float>(point.y, point.x + 1) -
                             gray.at<float>(point.y, point.x - 1));
    const double gy = 0.5 * (gray.at<float>(point.y + 1, point.x) -
                             gray.at<float>(point.y - 1, point.x));
    const double gradient = std::hypot(gx, gy);
    if (gradient < config.subpixel_minimum_gradient)
    {
      ++output.subpixel_rejected_low_gradient_count;
      continue;
    }
    const double offset = std::max(-0.5, std::min(
        0.5, (threshold - gray.at<float>(point.y, point.x)) / gradient));
    output.subpixel_boundary.emplace_back(
        origin.x + point.x + 0.5 + offset * gx / gradient,
        origin.y + point.y + 0.5 + offset * gy / gradient);
    gradient_sum += gradient;
  }
  output.subpixel_accepted_point_count =
      static_cast<int>(output.subpixel_boundary.size());
  if (output.subpixel_boundary.size() < 3U)
  {
    output.subpixel_failure_reason = "insufficient_valid_gradient_points";
    return;
  }
  for (std::size_t i = 0; i < output.subpixel_boundary.size(); ++i)
  {
    const cv::Point2d& a = output.subpixel_boundary[i];
    const cv::Point2d& b = output.subpixel_boundary[
        (i + 1U) % output.subpixel_boundary.size()];
    output.subpixel_perimeter += std::hypot(
        (b.x - a.x) * output.pixel_size_x,
        (b.y - a.y) * output.pixel_size_y);
    output.subpixel_area += (a.x * b.y - b.x * a.y) *
                            output.pixel_size_x * output.pixel_size_y;
  }
  output.subpixel_area = std::abs(output.subpixel_area) * 0.5;
  output.subpixel_mean_gradient =
      gradient_sum / static_cast<double>(output.subpixel_accepted_point_count);
  output.subpixel_valid = true;
  output.subpixel_failure_reason.clear();
}

FindObjectMeasurementSnapshot FindObject::AnalyzeGeometry(
    const cv::Mat &component_mask, const cv::Point &mask_origin_px,
    const cv::Mat &source_image,
    const FindObjectMeasurementConfig &config)
{
  FindObjectMeasurementSnapshot output;
  output.pixel_size_x = config.pixel_size_x > 0.0 ? config.pixel_size_x : 1.0;
  output.pixel_size_y = config.pixel_size_y > 0.0 ? config.pixel_size_y : 1.0;
  const bool pixel_units =
      std::abs(output.pixel_size_x - 1.0) < 1e-12 &&
      std::abs(output.pixel_size_y - 1.0) < 1e-12;
  output.length_unit = pixel_units ? "px" : "calibrated";
  output.area_unit = pixel_units ? "px^2" : "calibrated^2";
  output.topology_policy =
      config.connectivity == 4 ? "4_connected_foreground"
                               : "8_connected_foreground";
  if (component_mask.empty())
    return output;

  cv::Mat mask;
  if (component_mask.channels() == 1)
    mask = component_mask.clone();
  else
    cv::extractChannel(component_mask, mask, 0);
  if (mask.depth() != CV_8U)
    mask.convertTo(mask, CV_8U);
  cv::threshold(mask, mask, 0, 255, cv::THRESH_BINARY);

  std::vector<cv::Point> pixels;
  cv::findNonZero(mask, pixels);
  if (pixels.empty())
    return output;

  const cv::Rect local_bbox = cv::boundingRect(pixels);
  output.bbox_px =
      cv::Rect(mask_origin_px.x + local_bbox.x,
               mask_origin_px.y + local_bbox.y,
               local_bbox.width, local_bbox.height);
  output.pixel_count = static_cast<std::uint64_t>(pixels.size());
  const double pixel_area = output.pixel_size_x * output.pixel_size_y;
  output.projected_area =
      static_cast<double>(output.pixel_count) * pixel_area;
  output.equivalent_side = std::sqrt(output.projected_area);
  output.equivalent_radius =
      std::sqrt(output.projected_area / kFindObjectPi);

  double sum_x = 0.0;
  double sum_y = 0.0;
  for (const cv::Point &p : pixels)
  {
    sum_x += mask_origin_px.x + p.x + 0.5;
    sum_y += mask_origin_px.y + p.y + 0.5;
  }
  output.centroid_px =
      cv::Point2d(sum_x / pixels.size(), sum_y / pixels.size());

  MeasureFindObjectIntensity(mask, mask_origin_px, source_image, output);
  output.gwyddion_perimeter =
      GWPixelConfigurationPerimeter(
          mask, output.pixel_size_x, output.pixel_size_y,
          output.pixel_configuration_counts);
  output.gw_pixel_perimeter = output.gwyddion_perimeter;
  output.boundary_method = "GW_2x2_pixel_configuration";

  cv::Mat padded;
  cv::copyMakeBorder(mask, padded, 1, 1, 1, 1, cv::BORDER_CONSTANT,
                     cv::Scalar(0));
  std::vector<std::vector<cv::Point>> contours;
  std::vector<cv::Vec4i> hierarchy;
  cv::findContours(
      padded, contours, hierarchy, cv::RETR_TREE, cv::CHAIN_APPROX_NONE,
      cv::Point(mask_origin_px.x - 1, mask_origin_px.y - 1));
  int outer_index = -1;
  double outer_area = -1.0;
  for (int i = 0; i < static_cast<int>(contours.size()); ++i)
  {
    const double area = std::abs(cv::contourArea(contours[i]));
    const bool is_outer = hierarchy.empty() || hierarchy[i][3] < 0;
    if (is_outer && area > outer_area)
    {
      outer_area = area;
      outer_index = i;
    }
    for (std::size_t j = 0; j < contours[i].size(); ++j)
    {
      const cv::Point &a = contours[i][j];
      const cv::Point &b = contours[i][(j + 1) % contours[i].size()];
      output.polygon_perimeter +=
          std::hypot((b.x - a.x) * output.pixel_size_x,
                     (b.y - a.y) * output.pixel_size_y);
    }
  }
  if (outer_index >= 0)
  {
    for (const cv::Point &p : contours[outer_index])
      output.outer_boundary.emplace_back(p.x, p.y);
    output.polygon_area =
        std::abs(cv::contourArea(contours[outer_index])) * pixel_area;
    if (config.include_hole_boundaries && !hierarchy.empty())
    {
      for (int i = 0; i < static_cast<int>(contours.size()); ++i)
      {
        if (hierarchy[i][3] != outer_index)
          continue;
        std::vector<cv::Point2d> hole;
        for (const cv::Point &p : contours[i])
          hole.emplace_back(p.x, p.y);
        output.hole_boundaries.push_back(std::move(hole));
        output.polygon_area -=
            std::abs(cv::contourArea(contours[i])) * pixel_area;
      }
    }
  }

  std::vector<cv::Point2f> physical_cell_corners;
  physical_cell_corners.reserve(pixels.size() * 4);
  for (const cv::Point &p : pixels)
  {
    const double cx =
        (mask_origin_px.x + p.x + 0.5) * output.pixel_size_x;
    const double cy =
        (mask_origin_px.y + p.y + 0.5) * output.pixel_size_y;
    const double hx = 0.5 * output.pixel_size_x;
    const double hy = 0.5 * output.pixel_size_y;
    physical_cell_corners.emplace_back(
        static_cast<float>(cx - hx), static_cast<float>(cy - hy));
    physical_cell_corners.emplace_back(
        static_cast<float>(cx + hx), static_cast<float>(cy - hy));
    physical_cell_corners.emplace_back(
        static_cast<float>(cx + hx), static_cast<float>(cy + hy));
    physical_cell_corners.emplace_back(
        static_cast<float>(cx - hx), static_cast<float>(cy + hy));
  }
  std::vector<cv::Point2f> hull;
  cv::convexHull(physical_cell_corners, hull);
  if (hull.size() >= 3)
    output.convex_hull_area = std::abs(cv::contourArea(hull));
  if (output.gwyddion_perimeter > 0.0)
    output.circularity =
        4.0 * kFindObjectPi * output.projected_area /
        (output.gwyddion_perimeter * output.gwyddion_perimeter);
  if (output.convex_hull_area > 0.0)
    output.solidity =
        std::min(1.0, output.projected_area / output.convex_hull_area);

  if (hull.size() >= 2)
  {
    double max_distance2 = -1.0;
    for (std::size_t i = 0; i < hull.size(); ++i)
    {
      for (std::size_t j = i + 1; j < hull.size(); ++j)
      {
        const cv::Point2d delta = hull[j] - hull[i];
        const double distance2 = delta.dot(delta);
        if (distance2 > max_distance2)
        {
          max_distance2 = distance2;
          output.feret_max_p0 = hull[i];
          output.feret_max_p1 = hull[j];
        }
      }
    }
    if (max_distance2 >= 0.0)
    {
      output.feret_max = std::sqrt(max_distance2);
      const cv::Point2d delta =
          output.feret_max_p1 - output.feret_max_p0;
      output.feret_max_angle_deg = NormalizeFindObjectAngle180(
          std::atan2(delta.y, delta.x) * 180.0 / kFindObjectPi);
    }

    output.feret_min = std::numeric_limits<double>::max();
    for (std::size_t i = 0; i < hull.size(); ++i)
    {
      const cv::Point2d a = hull[i];
      const cv::Point2d b = hull[(i + 1) % hull.size()];
      const cv::Point2d edge = b - a;
      const double length = std::hypot(edge.x, edge.y);
      if (length <= std::numeric_limits<double>::epsilon())
        continue;
      const cv::Point2d normal(-edge.y / length, edge.x / length);
      double min_projection = std::numeric_limits<double>::max();
      double max_projection = -std::numeric_limits<double>::max();
      for (const cv::Point2f &point : hull)
      {
        const double projection =
            point.x * normal.x + point.y * normal.y;
        min_projection = std::min(min_projection, projection);
        max_projection = std::max(max_projection, projection);
      }
      const double width = max_projection - min_projection;
      if (width < output.feret_min)
      {
        output.feret_min = width;
        output.feret_min_angle_deg = NormalizeFindObjectAngle180(
            std::atan2(edge.y, edge.x) * 180.0 / kFindObjectPi);
      }
    }
    if (!std::isfinite(output.feret_min))
      output.feret_min = 0.0;

    cv::Point2f enclosing_center;
    float enclosing_radius = 0.0f;
    cv::minEnclosingCircle(hull, enclosing_center, enclosing_radius);
    output.enclosing_circle_center_px =
        cv::Point2d(enclosing_center.x / output.pixel_size_x,
                    enclosing_center.y / output.pixel_size_y);
    output.enclosing_circle_radius = enclosing_radius;
    output.enclosing_circle_valid = enclosing_radius > 0.0f;
  }

  double mean_x = 0.0;
  double mean_y = 0.0;
  for (const cv::Point &p : pixels)
  {
    mean_x += (mask_origin_px.x + p.x) * output.pixel_size_x;
    mean_y += (mask_origin_px.y + p.y) * output.pixel_size_y;
  }
  mean_x /= pixels.size();
  mean_y /= pixels.size();
  double covariance_xx = 0.0;
  double covariance_xy = 0.0;
  double covariance_yy = 0.0;
  for (const cv::Point &p : pixels)
  {
    const double dx =
        (mask_origin_px.x + p.x) * output.pixel_size_x - mean_x;
    const double dy =
        (mask_origin_px.y + p.y) * output.pixel_size_y - mean_y;
    covariance_xx += dx * dx;
    covariance_xy += dx * dy;
    covariance_yy += dy * dy;
  }
  covariance_xx /= pixels.size();
  covariance_xy /= pixels.size();
  covariance_yy /= pixels.size();
  const double trace = covariance_xx + covariance_yy;
  const double discriminant =
      std::sqrt(std::max(
          0.0, (covariance_xx - covariance_yy) *
                       (covariance_xx - covariance_yy) +
                   4.0 * covariance_xy * covariance_xy));
  const double lambda_major = 0.5 * (trace + discriminant);
  const double lambda_minor = 0.5 * (trace - discriminant);
  if (lambda_major > std::numeric_limits<double>::epsilon())
  {
    output.major_axis_length = 4.0 * std::sqrt(lambda_major);
    output.minor_axis_length =
        4.0 * std::sqrt(std::max(0.0, lambda_minor));
    output.orientation_deg = NormalizeFindObjectAngle180(
        0.5 * std::atan2(2.0 * covariance_xy,
                         covariance_xx - covariance_yy) *
        180.0 / kFindObjectPi);
    if (output.minor_axis_length >
        std::numeric_limits<double>::epsilon())
      output.aspect_ratio =
          output.major_axis_length / output.minor_axis_length;
    output.eccentricity =
        std::sqrt(std::max(0.0, 1.0 - lambda_minor / lambda_major));
    output.moment_ellipse_valid = pixels.size() >= 2;
  }

  cv::Mat distance_input;
  cv::copyMakeBorder(mask, distance_input, 1, 1, 1, 1,
                     cv::BORDER_CONSTANT, cv::Scalar(0));
  cv::Mat distance;
  cv::distanceTransform(distance_input, distance, cv::DIST_L2,
                        cv::DIST_MASK_PRECISE);
  double max_distance = 0.0;
  cv::Point max_location;
  cv::minMaxLoc(distance, nullptr, &max_distance, nullptr, &max_location);
  const double edge_distance = std::max(0.0, max_distance - 0.5);
  output.inscribed_circle_center_px =
      cv::Point2d(mask_origin_px.x + max_location.x - 1 + 0.5,
                  mask_origin_px.y + max_location.y - 1 + 0.5);
  output.inscribed_circle_radius =
      edge_distance * std::min(output.pixel_size_x, output.pixel_size_y);
  output.inscribed_circle_valid = edge_distance > 0.0;
  ApplyFindObjectSubpixelRefinement(
      source_image, mask, mask_origin_px, config,
      config.subpixel_iso_threshold, output);
  output.status = "measured";
  return output;
}


#define HI4bit(w) static_cast<BYTE>((w >> 4) & 0x0F)
#define LO4bit(w) static_cast<BYTE>(w & 0x0F)
#define LOBYTE(w) static_cast<BYTE>(w & 0xFF)

static gp_Pnt G_SearchPointGroup[224] = {
    {1, 0, 0},   {0, -1, 0},  {-1, 0, 0},  {0, 1, 0},   {1, 1, 0},
    {-1, 1, 0},  {-1, -1, 0}, {1, -1, 0},  {2, -1, 0},  {2, 0, 0},
    {2, 1, 0},   {2, 2, 0},   {1, 2, 0},   {0, 2, 0},   {-1, 2, 0},
    {-2, 2, 0},  {-2, 1, 0},  {-2, 0, 0},  {-2, -1, 0}, {-2, -2, 0},
    {-1, -2, 0}, {0, -2, 0},  {1, -2, 0},  {2, -2, 0},  {3, -2, 0},
    {3, -1, 0},  {3, 0, 0},   {3, 1, 0},   {3, 2, 0},   {3, 3, 0},
    {2, 3, 0},   {1, 3, 0},   {0, 3, 0},   {-1, 3, 0},  {-2, 3, 0},
    {-3, 3, 0},  {-3, 2, 0},  {-3, 1, 0},  {-3, 0, 0},  {-3, -1, 0},
    {-3, -2, 0}, {-3, -3, 0}, {-2, -3, 0}, {-1, -3, 0}, {0, -3, 0},
    {1, -3, 0},  {2, -3, 0},  {3, -3, 0},  {4, -3, 0},  {4, -2, 0},
    {4, -1, 0},  {4, 0, 0},   {4, 1, 0},   {4, 2, 0},   {4, 3, 0},
    {4, 4, 0},   {3, 4, 0},   {2, 4, 0},   {1, 4, 0},   {0, 4, 0},
    {-1, 4, 0},  {-2, 4, 0},  {-3, 4, 0},  {-4, 4, 0},  {-4, 3, 0},
    {-4, 2, 0},  {-4, 1, 0},  {-4, 0, 0},  {-4, -1, 0}, {-4, -2, 0},
    {-4, -3, 0}, {-4, -4, 0}, {-3, -4, 0}, {-2, -4, 0}, {-1, -4, 0},
    {0, -4, 0},  {1, -4, 0},  {2, -4, 0},  {3, -4, 0},  {4, -4, 0},
    {5, -4, 0},  {5, -3, 0},  {5, -2, 0},  {5, -1, 0},  {5, 0, 0},
    {5, 1, 0},   {5, 2, 0},   {5, 3, 0},   {5, 4, 0},   {5, 5, 0},
    {4, 5, 0},   {3, 5, 0},   {2, 5, 0},   {1, 5, 0},   {0, 5, 0},
    {-1, 5, 0},  {-2, 5, 0},  {-3, 5, 0},  {-4, 5, 0},  {-5, 5, 0},
    {-5, 4, 0},  {-5, 3, 0},  {-5, 2, 0},  {-5, 1, 0},  {-5, 0, 0},
    {-5, -1, 0}, {-5, -2, 0}, {-5, -3, 0}, {-5, -4, 0}, {-5, -5, 0},
    {-4, -5, 0}, {-3, -5, 0}, {-2, -5, 0}, {-1, -5, 0}, {0, -5, 0},
    {1, -5, 0},  {2, -5, 0},  {3, -5, 0},  {4, -5, 0},  {5, -5, 0},
    {6, -5, 0},  {6, -4, 0},  {6, -3, 0},  {6, -2, 0},  {6, -1, 0},
    {6, 0, 0},   {6, 1, 0},   {6, 2, 0},   {6, 3, 0},   {6, 4, 0},
    {6, 5, 0},   {6, 6, 0},   {5, 6, 0},   {4, 6, 0},   {3, 6, 0},
    {2, 6, 0},   {1, 6, 0},   {0, 6, 0},   {-1, 6, 0},  {-2, 6, 0},
    {-3, 6, 0},  {-4, 6, 0},  {-5, 6, 0},  {-6, 6, 0},  {-6, 5, 0},
    {-6, 4, 0},  {-6, 3, 0},  {-6, 2, 0},  {-6, 1, 0},  {-6, 0, 0},
    {-6, -1, 0}, {-6, -2, 0}, {-6, -3, 0}, {-6, -4, 0}, {-6, -5, 0},
    {-6, -6, 0}, {-5, -6, 0}, {-4, -6, 0}, {-3, -6, 0}, {-2, -6, 0},
    {-1, -6, 0}, {0, -6, 0},  {1, -6, 0},  {2, -6, 0},  {3, -6, 0},
    {4, -6, 0},  {5, -6, 0},  {6, -6, 0},  {7, -6, 0},  {7, -5, 0},
    {7, -4, 0},  {7, -3, 0},  {7, -2, 0},  {7, -1, 0},  {7, 0, 0},
    {7, 1, 0},   {7, 2, 0},   {7, 3, 0},   {7, 4, 0},   {7, 5, 0},
    {7, 6, 0},   {7, 7, 0},   {6, 7, 0},   {5, 7, 0},   {4, 7, 0},
    {3, 7, 0},   {2, 7, 0},   {1, 7, 0},   {0, 7, 0},   {-1, 7, 0},
    {-2, 7, 0},  {-3, 7, 0},  {-4, 7, 0},  {-5, 7, 0},  {-6, 7, 0},
    {-7, 7, 0},  {-7, 6, 0},  {-7, 5, 0},  {-7, 4, 0},  {-7, 3, 0},
    {-7, 2, 0},  {-7, 1, 0},  {-7, 0, 0},  {-7, -1, 0}, {-7, -2, 0},
    {-7, -3, 0}, {-7, -4, 0}, {-7, -5, 0}, {-7, -6, 0}, {-7, -7, 0},
    {-6, -7, 0}, {-5, -7, 0}, {-4, -7, 0}, {-3, -7, 0}, {-2, -7, 0},
    {-1, -7, 0}, {0, -7, 0},  {1, -7, 0},  {2, -7, 0},  {3, -7, 0},
    {4, -7, 0},  {5, -7, 0},  {6, -7, 0},  {7, -7, 0},
};
static gp_Pnt G_SearchPointGroup_OOD[16] = {
    {0, -1, 0},  {0, 1, 0},  {1, 0, 0},  {-1, 0, 0}, {1, 1, 0}, {-1, 1, 0},
    {-1, -1, 0}, {1, -1, 0}, {0, 2, 0},  {0, 3, 0},  {0, 4, 0}, {0, 5, 0},
    {0, 8, 0},   {0, 11, 0}, {0, 15, 0}, {0, 20, 0}

};
static gp_Pnt G_SearchPointGroup_OD[16] = {
    {0, -1, 0},  {0, 1, 0},  {1, 0, 0}, {-1, 0, 0}, {1, 1, 0}, {-1, 1, 0},
    {-1, -1, 0}, {1, -1, 0}, {0, 2, 0}, {0, 3, 0},  {0, 4, 0}, {0, 5, 0},
    {0, 6, 0},   {0, 7, 0},  {0, 8, 0}, {0, 9, 0}

};
static gp_Pnt G_SearchPointGroup_ODD[16] = {
    {0, -1, 0},  {0, 1, 0},  {1, 0, 0},  {-1, 0, 0}, {1, 1, 0}, {-1, 1, 0},
    {-1, -1, 0}, {1, -1, 0}, {0, 3, 0},  {0, 6, 0},  {0, 9, 0}, {0, 12, 0},
    {0, 15, 0},  {0, 18, 0}, {0, 21, 0}, {0, 24, 0}

};
static gp_Pnt G_SearchPointGroup_L[16] = {
    {1, 0, 0},  {0, 1, 0},  {0, -1, 0}, {-1, 0, 0}, {2, 0, 0}, {3, 0, 0},
    {4, 0, 0},  {5, 0, 0},  {6, 0, 0},  {7, 0, 0},  {8, 0, 0}, {9, 0, 0},
    {10, 0, 0}, {11, 0, 0}, {12, 0, 0}, {13, 0, 0}

};
static gp_Pnt G_SearchPointGroup_LL[16] = {
    {1, 0, 0},  {0, 1, 0},  {0, -1, 0}, {-1, 0, 0}, {2, 0, 0},  {4, 0, 0},
    {6, 0, 0},  {8, 0, 0},  {10, 0, 0}, {12, 0, 0}, {14, 0, 0}, {16, 0, 0},
    {18, 0, 0}, {20, 0, 0}, {22, 0, 0}, {24, 0, 0}

};
static gp_Pnt G_SearchPointGroup_LLL[16] = {
    {1, 0, 0},  {0, 1, 0},  {0, -1, 0}, {-1, 0, 0}, {3, 0, 0},  {6, 0, 0},
    {9, 0, 0},  {12, 0, 0}, {15, 0, 0}, {18, 0, 0}, {21, 0, 0}, {24, 0, 0},
    {27, 0, 0}, {30, 0, 0}, {33, 0, 0}, {36, 0, 0}

};
static gp_Pnt G_SearchPointGroup_LMAX[16] = {
    {1, 0, 0},  {0, 1, 0},  {0, -1, 0}, {-1, 0, 0}, {5, 0, 0},  {10, 0, 0},
    {15, 0, 0}, {20, 0, 0}, {25, 0, 0}, {30, 0, 0}, {35, 0, 0}, {40, 0, 0},
    {45, 0, 0}, {50, 0, 0}, {55, 0, 0}, {60, 0, 0}

};
static gp_Pnt G_SearchPointGroup_R[16] = {
    {-1, 0, 0},  {1, 0, 0},   {0, 1, 0},   {0, -1, 0}, {-2, 0, 0}, {-3, 0, 0},
    {-4, 0, 0},  {-5, 0, 0},  {-6, 0, 0},  {-7, 0, 0}, {-8, 0, 0}, {-9, 0, 0},
    {-10, 0, 0}, {-11, 0, 0}, {-12, 0, 0}, {-13, 0, 0}

};
static gp_Pnt G_SearchPointGroup_U[16] = {
    {0, -1, 0},  {0, 1, 0},   {1, 0, 0},   {-1, 0, 0}, {0, -2, 0}, {0, -3, 0},
    {0, -4, 0},  {0, -5, 0},  {0, -6, 0},  {0, -7, 0}, {0, -8, 0}, {0, -9, 0},
    {0, -10, 0}, {0, -11, 0}, {0, -12, 0}, {0, -13, 0}

};
static gp_Pnt G_SearchPointGroup_D[16] = {
    {0, -1, 0}, {0, 1, 0},  {1, 0, 0},  {-1, 0, 0}, {0, 2, 0}, {0, 3, 0},
    {0, 4, 0},  {0, 5, 0},  {0, 6, 0},  {0, 7, 0},  {0, 8, 0}, {0, 9, 0},
    {0, 10, 0}, {0, 11, 0}, {0, 12, 0}, {0, 13, 0}};
static gp_Pnt G_SearchPointGroup_DD[16] = {
    {0, -1, 0}, {0, 1, 0},  {1, 0, 0},  {-1, 0, 0}, {0, 2, 0},  {0, 4, 0},
    {0, 6, 0},  {0, 8, 0},  {0, 10, 0}, {0, 12, 0}, {0, 14, 0}, {0, 16, 0},
    {0, 18, 0}, {0, 20, 0}, {0, 22, 0}, {0, 24, 0}};
static gp_Pnt G_SearchPointGroup_DDD[16] = {
    {0, -1, 0}, {0, 1, 0},  {1, 0, 0},  {-1, 0, 0}, {0, 3, 0},  {0, 6, 0},
    {0, 9, 0},  {0, 12, 0}, {0, 15, 0}, {0, 18, 0}, {0, 21, 0}, {0, 24, 0},
    {0, 27, 0}, {0, 30, 0}, {0, 33, 0}, {0, 36, 0}

};
static gp_Pnt G_SearchPointGroup_X[16] = {
    {0, -1, 0}, {0, 1, 0},  {1, 0, 0},  {-1, 0, 0}, {0, 2, 0}, {0, 3, 0},
    {0, 4, 0},  {0, 5, 0},  {0, 6, 0},  {0, 7, 0},  {0, 8, 0}, {0, 9, 0},
    {0, 10, 0}, {0, 11, 0}, {0, 12, 0}, {0, 13, 0}

};

int FindObject::m_curfindobjectnum = 0;
FindObject::FindObject()
    : m_iborw(3), m_ifilterNedge(0), m_idistance(16), m_icurobj(0),
      m_iobjnum(0), m_iminarea(5), m_imaxarea(99999), m_iminobjw(0),
      m_iminobjh(0), m_imaxobjw(9999), m_imaxobjh(9999), m_ihgap(50),
      m_isgap(20), m_iogap(30), m_pgetimage(0), m_icopyw(30), m_icopyh(30),
      m_icopywgrid(20), m_background_edge(2), m_background_method(1),
      m_ioffsetx0(0), m_ioffsetx1(0), m_ioffsety0(0), m_ioffsety1(0),
      m_imagethre(18), m_imagethreincrease(0), m_imagecomparegap(2),
      m_imagefindBorW(0), m_imageedge_5o7(5), m_debug_component_count(0),
      m_debug_accepted_count(0), m_debug_rejected_count(0),
      m_debug_max_component_area(0), m_debug_max_component_w(0),
      m_debug_max_component_h(0),
      m_irelationrect(gp_Pnt(0, 0, 0), gp_Pnt(0, 0, 0)) {
  string strname = string("fobject%1");
  setname(strname.c_str());
  m_curfindobjectnum = m_curfindobjectnum + 1;

  setcolor(0, 205, 180);
  int icurmodule = ImageManager::GetCurMode();
  g_pbackobjectimage = ImageManager::GetBackObjectImage(icurmodule);
  g_pmapimage = ImageManager::GetMapImage(icurmodule);

  m_objlistscanorA = ImageManager::GetListScan(icurmodule);
  m_objlistcollectorA = ImageManager::GetListCollect(icurmodule);
  m_SearchPointGroup = G_SearchPointGroup;
  m_searchtype = ObjectSearchType::Search_O;
}
FindObject::~FindObject() {}
void FindObject::setbrow(int iborw) { m_iborw = iborw; }
void FindObject::setfilteredge(int iw) { m_ifilterNedge = iw < 0 ? 0 : iw; }
void FindObject::setcolor(int ir, int ig, int ib) {
  m_rectresults.setcolor(ir, ig, ib);
}
void FindObject::setshow(int ishow) {
  if (1 == ishow) {
    m_rectresults.setcolor(255, 0, 0);
    m_rectresults.setshow(1);
    m_rectresults.MakeShape();
  }
  Shape::setshow(ishow);
}
void FindObject::getshape(void *pshape) {
  Shape *pshape0 = (Shape *)pshape;
  if (pshape0 == nullptr)
    return;

  const gp_Rectangle arect = rect();
  pshape0->setrect(arect.TopLeft().X(), arect.TopLeft().Y(), arect.Width(),
                   arect.Height());
}
void FindObject::setrect(int ix, int iy, int iw, int ih) {
  Shape::setrect(ix, iy, iw, ih);
}
void FindObject::drawshape() { Shape::drawshape(); }
int FindObject::getresultcentx(int inum) {
  const FindObjectMeasurementSnapshot *measurement = getmeasurement(inum);
  if (measurement != nullptr && measurement->status == "measured")
    return static_cast<int>(std::lround(measurement->centroid_px.x));
  if (inum >= 0 && inum < m_rectresults.size()) {
    const gp_Rectangle rect = m_rectresults.getrect(inum);
    return static_cast<int>(std::lround(
        0.5 * (rect.TopLeft().X() + rect.BottomRight().X())));
  }
  return 0;
}
int FindObject::getresultcenty(int inum) {
  const FindObjectMeasurementSnapshot *measurement = getmeasurement(inum);
  if (measurement != nullptr && measurement->status == "measured")
    return static_cast<int>(std::lround(measurement->centroid_px.y));
  if (inum >= 0 && inum < m_rectresults.size()) {
    const gp_Rectangle rect = m_rectresults.getrect(inum);
    return static_cast<int>(std::lround(
        0.5 * (rect.TopLeft().Y() + rect.BottomRight().Y())));
  }
  return 0;
}
int FindObject::getresultx(int inum) {
  const FindObjectMeasurementSnapshot *measurement = getmeasurement(inum);
  if (measurement != nullptr && measurement->status == "measured")
    return measurement->bbox_px.x;
  if (inum >= 0 && inum < m_rectresults.size())
    return static_cast<int>(m_rectresults.getrect(inum).TopLeft().X());
  return 0;
}
int FindObject::getresulty(int inum) {
  const FindObjectMeasurementSnapshot *measurement = getmeasurement(inum);
  if (measurement != nullptr && measurement->status == "measured")
    return measurement->bbox_px.y;
  if (inum >= 0 && inum < m_rectresults.size())
    return static_cast<int>(m_rectresults.getrect(inum).TopLeft().Y());
  return 0;
}
int FindObject::getresultw(int inum) {
  const FindObjectMeasurementSnapshot *measurement = getmeasurement(inum);
  if (measurement != nullptr && measurement->status == "measured")
    return measurement->bbox_px.width;
  if (inum >= 0 && inum < m_rectresults.size())
    return static_cast<int>(m_rectresults.getrect(inum).Width()) + 1;
  return 0;
}
int FindObject::getresulth(int inum) {
  const FindObjectMeasurementSnapshot *measurement = getmeasurement(inum);
  if (measurement != nullptr && measurement->status == "measured")
    return measurement->bbox_px.height;
  if (inum >= 0 && inum < m_rectresults.size())
    return static_cast<int>(m_rectresults.getrect(inum).Height()) + 1;
  return 0;
}
int FindObject::getresultsize(int inum) {
  if (inum >= 0 && inum < m_rectresults.size() &&
      inum < static_cast<int>(m_vobjnum.size()))
    return m_vobjnum.at(inum);
  else
    return 0;
}
int FindObject::getresultobjsnum() { return m_rectresults.size(); }
int FindObject::getdebugcomponentcount() { return m_debug_component_count; }
int FindObject::getdebugacceptedcount() { return m_debug_accepted_count; }
int FindObject::getdebugrejectedcount() { return m_debug_rejected_count; }
int FindObject::getdebugmaxcomponentarea() {
  return m_debug_max_component_area;
}
int FindObject::getdebugmaxcomponentw() { return m_debug_max_component_w; }
int FindObject::getdebugmaxcomponenth() { return m_debug_max_component_h; }
const std::string &FindObject::getdebugalgorithmbranch() const {
  return m_debug_algorithm_branch;
}
void FindObject::setgeometrycalibration(double pixel_size_x,
                                            double pixel_size_y)
{
  m_measurement_config.pixel_size_x =
      pixel_size_x > 0.0 ? pixel_size_x : 1.0;
  m_measurement_config.pixel_size_y =
      pixel_size_y > 0.0 ? pixel_size_y : 1.0;
}

void FindObject::setgeometryconnectivity(int connectivity)
{
  m_measurement_config.connectivity = connectivity == 4 ? 4 : 8;
}

void FindObject::setmeasurementselection(int index)
{
  m_measurement_selection = index < -1 ? -1 : index;
}

void FindObject::setshowboundary(int enabled)
{
  m_show_boundary = enabled != 0;
}

void FindObject::setshowmomentellipse(int enabled)
{
  m_show_moment_ellipse = enabled != 0;
}

void FindObject::setshowferet(int enabled)
{
  m_show_feret = enabled != 0;
}

void FindObject::setshowgeometrycircles(int enabled)
{
  m_show_geometry_circles = enabled != 0;
}

void FindObject::setconclusionshape(int shape)
{
  m_conclusion_shape = std::max(0, std::min(4, shape));
}
void FindObject::setbackgroundmethod(int method)
{
  if (method <= 0)
    m_measurement_config.background_method = FindObjectBackgroundMethod::None;
  else if (method == 1)
    m_measurement_config.background_method =
        FindObjectBackgroundMethod::RoiBorderRobust;
  else
    m_measurement_config.background_method =
        FindObjectBackgroundMethod::MorphologicalOpening;
}

void FindObject::setbackgroundborderwidth(int pixels)
{
  m_measurement_config.background_border_width_px = std::max(1, pixels);
}

void FindObject::setbackgroundmorphologyradius(int pixels)
{
  m_measurement_config.background_morphology_radius_px = std::max(1, pixels);
}

void FindObject::setsubpixelenabled(int enabled)
{
  m_measurement_config.subpixel_enabled = enabled != 0;
}

void FindObject::setsubpixelminimumgradient(double gradient)
{
  m_measurement_config.subpixel_minimum_gradient = std::max(0.0, gradient);
}

void FindObject::setsubpixelisothreshold(double threshold)
{
  m_measurement_config.subpixel_iso_threshold = threshold;
}

void FindObject::setactivegeometrybasis(int basis)
{
  m_active_geometry_basis = basis == 1 ? 1 : 0;
}

int FindObject::getmeasurementcount()
{
  return static_cast<int>(m_measurements.size());
}

const FindObjectMeasurementSnapshot *
FindObject::getmeasurement(int index) const
{
  if (index < 0 || index >= static_cast<int>(m_measurements.size()))
    return nullptr;
  return &m_measurements[static_cast<std::size_t>(index)];
}

double FindObject::getarea(int index)
{
  const auto *value = getmeasurement(index);
  return value != nullptr ? value->projected_area : 0.0;
}

double FindObject::getperimeter(int index)
{
  const auto *value = getmeasurement(index);
  return value != nullptr ? value->gwyddion_perimeter : 0.0;
}

double FindObject::getequivalentside(int index)
{
  const auto *value = getmeasurement(index);
  return value != nullptr ? value->equivalent_side : 0.0;
}

double FindObject::getcircularity(int index)
{
  const auto *value = getmeasurement(index);
  return value != nullptr ? value->circularity : 0.0;
}

double FindObject::getsolidity(int index)
{
  const auto *value = getmeasurement(index);
  return value != nullptr ? value->solidity : 0.0;
}

double FindObject::getmajoraxis(int index)
{
  const auto *value = getmeasurement(index);
  return value != nullptr ? value->major_axis_length : 0.0;
}

double FindObject::getminoraxis(int index)
{
  const auto *value = getmeasurement(index);
  return value != nullptr ? value->minor_axis_length : 0.0;
}

double FindObject::getorientation(int index)
{
  const auto *value = getmeasurement(index);
  return value != nullptr ? value->orientation_deg : 0.0;
}

double FindObject::getferetmax(int index)
{
  const auto *value = getmeasurement(index);
  return value != nullptr ? value->feret_max : 0.0;
}

double FindObject::getferetmin(int index)
{
  const auto *value = getmeasurement(index);
  return value != nullptr ? value->feret_min : 0.0;
}

double FindObject::getinscribedradius(int index)
{
  const auto *value = getmeasurement(index);
  return value != nullptr ? value->inscribed_circle_radius : 0.0;
}

double FindObject::getenclosingradius(int index)
{
  const auto *value = getmeasurement(index);
  return value != nullptr ? value->enclosing_circle_radius : 0.0;
}
void FindObject::StoreAcceptedLabelMask(
    const cv::Mat &labels, int label, int service_id,
    int origin_x, int origin_y)
{
  if (g_pmapimage == nullptr || labels.empty() ||
      service_id <= 0 || service_id > 255)
    return;
  for (int y = 0; y < labels.rows; ++y)
  {
    const int *row = labels.ptr<int>(y);
    for (int x = 0; x < labels.cols; ++x)
    {
      if (row[x] == label)
        SetMAP_service(origin_x + x, origin_y + y, service_id);
    }
  }
}

void FindObject::RefreshGeometryMeasurements(Image &image)
{
  m_measurements.clear();
  ++m_measurement_generation;

  m_measurements.reserve(m_scanid.size());
  const int roi_x = static_cast<int>(rect().TopLeft().X());
  const int roi_y = static_cast<int>(rect().TopLeft().Y());
  const int roi_w = static_cast<int>(rect().Width());
  const int roi_h = static_cast<int>(rect().Height());

  for (int object_index = 0;
       object_index < static_cast<int>(m_scanid.size());
       ++object_index)
  {
    FindObjectMeasurementSnapshot unavailable;
    unavailable.object_index = object_index;
    unavailable.component_label = m_scanid[object_index];

    const int service_id = m_scanid[object_index];
    if (g_pmapimage == nullptr || service_id <= 0 || service_id > 255 ||
        roi_w <= 0 || roi_h <= 0)
    {
      unavailable.status = service_id > 255
                               ? "component_label_overflow"
                               : "component_mask_unavailable";
      m_measurements.push_back(std::move(unavailable));
      continue;
    }

    int min_x = roi_x + roi_w;
    int min_y = roi_y + roi_h;
    int max_x = roi_x - 1;
    int max_y = roi_y - 1;
    for (int y = roi_y; y < roi_y + roi_h; ++y)
    {
      for (int x = roi_x; x < roi_x + roi_w; ++x)
      {
        if (MAP_service(x, y) != service_id)
          continue;
        min_x = std::min(min_x, x);
        min_y = std::min(min_y, y);
        max_x = std::max(max_x, x);
        max_y = std::max(max_y, y);
      }
    }
    if (max_x < min_x || max_y < min_y)
    {
      unavailable.status = "component_mask_empty";
      m_measurements.push_back(std::move(unavailable));
      continue;
    }

    cv::Mat mask(max_y - min_y + 1, max_x - min_x + 1,
                 CV_8UC1, cv::Scalar(0));
    for (int y = min_y; y <= max_y; ++y)
    {
      uchar *row = mask.ptr<uchar>(y - min_y);
      for (int x = min_x; x <= max_x; ++x)
      {
        if (MAP_service(x, y) == service_id)
          row[x - min_x] = 255;
      }
    }

    FindObjectMeasurementSnapshot measured =
        AnalyzeGeometry(mask, cv::Point(min_x, min_y), image.getmat(),
                        m_measurement_config);
    measured.object_index = object_index;
    measured.component_label = service_id;
    measured.background_method = m_last_background_method;
    measured.background_valid = m_last_background_valid;
    measured.background_sample_count = m_last_background_sample_count;
    measured.background_baseline_mean = m_last_background_baseline_mean;
    measured.residual_threshold = m_last_residual_threshold;
    measured.segmentation_domain = m_last_segmentation_domain;
    measured.active_geometry_basis =
        (m_active_geometry_basis == 1 && measured.subpixel_valid)
            ? "subpixel"
            : "gw_discrete";
    measured.generation = m_measurement_generation;
    std::uint64_t mask_hash = 1469598103934665603ULL;
    for (int mask_y = 0; mask_y < mask.rows; ++mask_y)
    {
      const uchar* mask_row = mask.ptr<uchar>(mask_y);
      for (int mask_x = 0; mask_x < mask.cols; ++mask_x)
      {
        mask_hash ^= static_cast<std::uint64_t>(mask_row[mask_x]);
        mask_hash *= 1099511628211ULL;
      }
    }
    measured.mask_hash = mask_hash;
    measured.object_ref =
        "findobject:g" + std::to_string(measured.generation) +
        ":label" + std::to_string(service_id) +
        ":bbox" + std::to_string(measured.bbox_px.x) + "," +
        std::to_string(measured.bbox_px.y) + "," +
        std::to_string(measured.bbox_px.width) + "," +
        std::to_string(measured.bbox_px.height) +
        ":mask" + std::to_string(measured.mask_hash);

    m_measurements.push_back(std::move(measured));
  }
}

bool FindObject::RefreshAlgorithmRuntimeResources(int image_width,
                                                  int image_height) {
  if (!ImageManager::EnsureAlgorithmRuntimeResources(image_width, image_height))
    return false;

  const int icurmodule = ImageManager::GetCurMode();
  g_pbackobjectimage = ImageManager::GetBackObjectImage(icurmodule);
  g_pmapimage = ImageManager::GetMapImage(icurmodule);
  m_objlistscanorA = ImageManager::GetListScan(icurmodule);
  m_objlistcollectorA = ImageManager::GetListCollect(icurmodule);

  return g_pmapimage != nullptr && m_objlistscanorA != nullptr &&
         m_objlistcollectorA != nullptr;
}
void FindObject::FinalizeRegionGrowthDebugCounters() {
  m_debug_accepted_count = m_rectresults.size();
  m_debug_rejected_count =
      (m_debug_component_count > m_debug_accepted_count)
          ? (m_debug_component_count - m_debug_accepted_count)
          : 0;
  if (m_pgetimage != nullptr)
    RefreshGeometryMeasurements(*m_pgetimage);
}
void FindObject::ObserveDebugComponent(int area, int width, int height) {
  if (area > m_debug_max_component_area) {
    m_debug_max_component_area = area;
    m_debug_max_component_w = width;
    m_debug_max_component_h = height;
  }
}
bool FindObject::IsSamePixel(const cv::Vec3b &lhs, const cv::Vec3b &rhs) const {
  return lhs[0] == rhs[0] && lhs[1] == rhs[1] && lhs[2] == rhs[2];
}
void FindObject::setdistance(int idist) {
  switch (m_searchtype) {
  case ObjectSearchType::Search_O: {
    if (idist > 3 && idist < 1520)
      m_idistance = idist;
    else if (idist < 4)
      m_idistance = 4;
    else if (idist > 1519)
      m_idistance = 1520;
  } break;
  case ObjectSearchType::Search_OL:
  case ObjectSearchType::Search_OR:
  case ObjectSearchType::Search_OU:
  case ObjectSearchType::Search_OD:
  case ObjectSearchType::Search_OX: {
    if (idist > 0 && idist < 16)
      m_idistance = idist;
    else if (idist < 0)
      m_idistance = 4;
    else if (idist > 16)
      m_idistance = 16;
  } break;
  default: {
    m_idistance = 16;
  } break;
  }
}
void FindObject::setoffset(int ix0, int ix1, int iy0, int iy1) {
  m_ioffsetx0 = ix0;
  m_ioffsetx1 = ix1;
  m_ioffsety0 = iy0;
  m_ioffsety1 = iy1;
}
void FindObject::setsearchtype(int itype) {
  switch (itype) {
  case 0:
    m_searchtype = ObjectSearchType::Search_O;
    m_SearchPointGroup = G_SearchPointGroup;
    break;
  case 1:
    m_searchtype = ObjectSearchType::Search_OL;
    m_SearchPointGroup = G_SearchPointGroup_L;
    break;
  case 11:
    m_searchtype = ObjectSearchType::Search_OL;
    m_SearchPointGroup = G_SearchPointGroup_LL;
    break;
  case 111:
    m_searchtype = ObjectSearchType::Search_OL;
    m_SearchPointGroup = G_SearchPointGroup_LLL;
    break;
  case 1111:
    m_searchtype = ObjectSearchType::Search_OL;
    m_SearchPointGroup = G_SearchPointGroup_LMAX;
    break;

  case 2:
    m_searchtype = ObjectSearchType::Search_OR;
    m_SearchPointGroup = G_SearchPointGroup_R;
    break;
  case 3:
    m_searchtype = ObjectSearchType::Search_OU;
    m_SearchPointGroup = G_SearchPointGroup_U;

    break;
  case 4:
    m_searchtype = ObjectSearchType::Search_OD;
    m_SearchPointGroup = G_SearchPointGroup_D;

    break;
  case 44:
    m_searchtype = ObjectSearchType::Search_OD;
    m_SearchPointGroup = G_SearchPointGroup_DD;

    break;
  case 444:
    m_searchtype = ObjectSearchType::Search_OD;
    m_SearchPointGroup = G_SearchPointGroup_DDD;

    break;

  case 40:
    m_searchtype = ObjectSearchType::Search_OD;
    m_SearchPointGroup = G_SearchPointGroup_OD;

    break;
  case 440:
    m_searchtype = ObjectSearchType::Search_OD;
    m_SearchPointGroup = G_SearchPointGroup_ODD;

    break;
  case 441:
    m_searchtype = ObjectSearchType::Search_OD;
    m_SearchPointGroup = G_SearchPointGroup_OOD;

    break;

  case 5:
    m_searchtype = ObjectSearchType::Search_OX;
    m_SearchPointGroup = G_SearchPointGroup_X;

    break;
  case 6:

    break;

  default:
    m_searchtype = ObjectSearchType::Search_O;
    m_SearchPointGroup = G_SearchPointGroup;

    break;
  }
}
void FindObject::Measure(Image &image) {
  m_debug_algorithm_branch = "region_growth";
  m_pgetimage = &image;
  m_measurements.clear();
  if (image.getmat().empty())
    return;
  if (!RefreshAlgorithmRuntimeResources(image.getWidth(), image.getHeight()))
    return;
  if (image.getWidth() < rect().TopLeft().X() + rect().Width() ||
      image.getHeight() < rect().TopLeft().Y() + rect().Height())
    return;
  int iw = rect().Width();
  int ih = rect().Height();
  int ix = rect().TopLeft().X();
  int iy = rect().TopLeft().Y();
  if (iw <= 0 || ih <= 0 || ix < 0 || iy < 0)
    return;
  if (g_pmapimage == nullptr || m_objlistscanorA == nullptr ||
      m_objlistcollectorA == nullptr)
    return;
  if (g_pmapimage->getWidth() < ix + iw || g_pmapimage->getHeight() < iy + ih)
    return;
  int ix1 = ix + iw;
  int iy1 = iy + ih;

  int m_isearchfirstx = rect().TopLeft().X();
  int m_isearchfirsty = rect().TopLeft().Y();

  int nx0 = 0;
  int ny0 = 0;
  int nx = 0;
  int ny = 0;
  int nservice = 0;
  cv::Vec3b abyte, bytenext;

  int nScanerID = 1;
  int icurScanerNUM = 0;
  bool boverflow = false;

  ncurscan = -1;
  nscansize = 0;

  ncursearchseek = -1;
  nsearchseeksize = 0;

  int ishowfont = 0;
  int mapservice = 0;
  int mapanalysis = 0;
  int mapedge = 0;

  int iminx = 9999;
  int iminy = 9999;
  int imaxx = 0;
  int imaxy = 0;

  int iborw = 0;

  switch (m_iborw) {
  case 901: {
    int isize = m_cent_h_bw_points_v.size();
    for (int i = 0; i < isize; i++)
      m_cent_h_bw_points_v[i].clear();
    m_cent_h_bw_points_v.clear();
  } break;
  case 902: {
    int isize = m_cent_v_bw_points_v.size();
    for (int i = 0; i < isize; i++)
      m_cent_v_bw_points_v[i].clear();
    m_cent_v_bw_points_v.clear();
  } break;
  case 903: {
    int isize = m_cent_h_wb_points_v.size();
    for (int i = 0; i < isize; i++)
      m_cent_h_wb_points_v[i].clear();
    m_cent_h_wb_points_v.clear();
  } break;
  case 904: {
    int isize = m_cent_v_wb_points_v.size();
    for (int i = 0; i < isize; i++)
      m_cent_v_wb_points_v[i].clear();
    m_cent_v_wb_points_v.clear();
  } break;
  }

  m_icurobj = 0;
  m_totalarea = 0;
  m_debug_component_count = 0;
  m_debug_accepted_count = 0;
  m_debug_rejected_count = 0;
  m_debug_max_component_area = 0;
  m_debug_max_component_w = 0;
  m_debug_max_component_h = 0;

  m_scanid.clear();
  m_vborw.clear();
  m_vrow.clear();
  m_vobjnum.clear();
  m_rectresults.clear();
  m_keypoint.clear();
  m_fitwh.clear();
  ;
  MAPCLEAR();

  CLEAR_SEARCHSEEK();
  PUSH_SEARCHSEEK(m_isearchfirstx, m_isearchfirsty);

  for (int icurSeekNum = 0; icurSeekNum < nsearchseeksize;) {
  FORBEGIN:
    if (icurSeekNum < 0 || icurSeekNum >= nsearchseeksize)
      break;
    mapservice = MAP_service(m_objlistcollectorA[icurSeekNum].X(),
                             m_objlistcollectorA[icurSeekNum].Y());

    if (mapservice > 0) {
      icurSeekNum++;

      if (icurSeekNum >= nsearchseeksize)
        break;
      else
        goto FORBEGIN;
    }
    iminx = 9999;
    iminy = 9999;
    imaxx = 0;
    imaxy = 0;

    CLEAR_SCANOR();
    PUSH_SCANOR(m_objlistcollectorA[icurSeekNum].X(),
                m_objlistcollectorA[icurSeekNum].Y());
    if (nscansize <= 0)
      break;
    iminx = static_cast<int>(m_objlistcollectorA[icurSeekNum].X());
    iminy = static_cast<int>(m_objlistcollectorA[icurSeekNum].Y());
    imaxx = iminx;
    imaxy = iminy;
    SetMAP_service(m_objlistcollectorA[icurSeekNum].X(),
                   m_objlistcollectorA[icurSeekNum].Y(), nScanerID);
    int itestx0 = MAP_service(m_objlistcollectorA[icurSeekNum].X(),
                              m_objlistcollectorA[icurSeekNum].Y());
    icurScanerNUM = 0;

  CURSCANERBEGIN:
    while (icurScanerNUM != nscansize) {
      nx0 = m_objlistscanorA[icurScanerNUM].X();
      ny0 = m_objlistscanorA[icurScanerNUM].Y();
      abyte = image.pixel(nx0, ny0);
      mapanalysis = MAP_analysis(nx0, ny0);

      if (mapanalysis == FindObject::ANLAYSIS_OVER)
        goto NEXTFINDSTEP;
      for (int i = 0; i < m_idistance; i++) {
        nx = nx0 + m_SearchPointGroup[i].X();
        ny = ny0 + m_SearchPointGroup[i].Y();
        if (nx < ix || ny < iy || nx >= ix1 || ny >= iy1)
          continue;
        mapanalysis = MAP_analysis(nx, ny);
        mapservice = MAP_service(nx, ny);
        mapedge = MAP_edge(nx, ny);
        if (mapanalysis == ANLAYSIS_OVER || mapservice > 0 ||
            (mapedge == mapservice && 0 != mapedge))
          continue;
        bytenext = image.pixel(nx, ny);

        if (abyte == bytenext) {
          PUSH_SCANOR(nx, ny);
          SetMAP_service(nx, ny, nScanerID);
          iborw = (abyte[0] == 0) ? 0 : 1;
          SetMAP_pixel(nx, ny, iborw);
          if (iminx > nx)
            iminx = nx;
          if (iminy > ny)
            iminy = ny;
          if (imaxx < nx)
            imaxx = nx;
          if (imaxy < ny)
            imaxy = ny;
        } else {
          PUSH_SEARCHSEEK(nx, ny);
          SetMAP_edge(nx, ny, nScanerID);
        }
      }
      SetMAP_analysis(nx0, ny0, ANLAYSIS_OVER);
    NEXTFINDSTEP:
      icurScanerNUM++;
    }
  CURSCANEREND:
    if (nscansize > 0)
      m_debug_component_count++;
    int iobjw = (imaxx - iminx <= 0) ? 1 : imaxx - iminx;
    int iobjh = (imaxy - iminy <= 0) ? 1 : imaxy - iminy;
    ObserveDebugComponent(nscansize, iobjw, iobjh);
    if ((m_ifilterNedge > 0 &&
         (iminx > m_ifilterNedge && imaxx < iw - m_ifilterNedge &&
          iminy > m_ifilterNedge && imaxy < ih - m_ifilterNedge)) ||
        m_ifilterNedge == 0) {
      if (nscansize > m_iminarea && nscansize < m_imaxarea &&
          iobjw < m_imaxobjw && iobjw >= m_iminobjw && iobjh < m_imaxobjh &&
          iobjh >= m_iminobjh) {
        m_vrow.push_back(nscansize);

        abyte = image.pixel(m_objlistscanorA[0].X(), m_objlistscanorA[0].Y());

        m_vborw.push_back(abyte);

        boverflow = false;
        {
          switch (m_iborw) {
          case 3: {
            gp_Rectangle arectresult(gp_Pnt(iminx, iminy, 0),
                                     gp_Pnt(imaxx, imaxy, 0));
            gp_Pnt apoint(m_objlistscanorA[0].X(), m_objlistscanorA[0].Y(), 0);
            m_keypoint.addpoint(apoint);

            m_rectresults.addrect(arectresult);
            m_scanid.push_back(nScanerID);
            m_vobjnum.push_back(nscansize);
            m_iobjnum++;
          }
            ishowfont++;
            m_icurobj++;

            break;
          case 0:
            break;
          case 1: {
            {
              gp_Rectangle arectresult(gp_Pnt(iminx, iminy, 0),
                                       gp_Pnt(imaxx, imaxy, 0));
              if (m_vborw[m_icurobj][0] > 0) {
                gp_Pnt apoint(m_objlistscanorA[0].X(), m_objlistscanorA[0].Y(),
                              0);
                m_keypoint.addpoint(apoint);
                m_rectresults.addrect(arectresult);
                m_scanid.push_back(nScanerID);
                m_vobjnum.push_back(nscansize);
                m_totalarea = m_totalarea + nscansize;
                m_iobjnum++;
              }
              ishowfont++;
              m_icurobj++;
            }
          } break;
          case 2: {
            {
              gp_Rectangle arectresult(gp_Pnt(iminx, iminy, 0),
                                       gp_Pnt(imaxx, imaxy, 0));
              if (m_vborw[m_icurobj][0] < 255) {
                gp_Pnt apoint(m_objlistscanorA[0].X(), m_objlistscanorA[0].Y(),
                              0);
                m_keypoint.addpoint(apoint);

                m_rectresults.addrect(arectresult);
                m_scanid.push_back(nScanerID);
                m_vobjnum.push_back(nscansize);

                m_totalarea = m_totalarea + nscansize;
                m_iobjnum++;
              }
              ishowfont++;
              m_icurobj++;
            }
          } break;

          case 11: {

            gp_Rectangle arectresult(gp_Pnt(iminx, iminy, 0),
                                     gp_Pnt(imaxx, imaxy, 0));
            if (m_vborw[m_icurobj][0] > 0) {
              for (int ir = 0; ir < nscansize; ir++) {
                image.setPixel(m_objlistscanorA[ir].X(),
                               m_objlistscanorA[ir].Y(), cv::Vec3b(0, 0, 0));
              }
              gp_Pnt apoint(m_objlistscanorA[0].X(), m_objlistscanorA[0].Y(),
                            0);
              m_keypoint.addpoint(apoint);
              m_rectresults.addrect(arectresult);
              m_scanid.push_back(nScanerID);
              m_vobjnum.push_back(nscansize);

              m_totalarea = m_totalarea + nscansize;
              m_iobjnum++;
            }
            ishowfont++;
            m_icurobj++;

          } break;
          case 101: {

            gp_Rectangle arectresult(gp_Pnt(iminx, iminy, 0),
                                     gp_Pnt(imaxx, imaxy, 0));
            if (m_vborw[m_icurobj][0] > 0) {
              for (int ir = 0; ir < nscansize; ir++) {
                image.setPixel(m_objlistscanorA[ir].X(),
                               m_objlistscanorA[ir].Y(),
                               cv::Vec3b(255, 255, 255));
              }
              gp_Pnt apoint(m_objlistscanorA[0].X(), m_objlistscanorA[0].Y(),
                            0);
              m_keypoint.addpoint(apoint);
              m_rectresults.addrect(arectresult);
              m_scanid.push_back(nScanerID);
              m_vobjnum.push_back(nscansize);

              m_totalarea = m_totalarea + nscansize;
              m_iobjnum++;
            }
            ishowfont++;
            m_icurobj++;

          } break;
          case 12: {
            {
              gp_Rectangle arectresult(gp_Pnt(iminx, iminy, 0),
                                       gp_Pnt(imaxx, imaxy, 0));
              if (m_vborw[m_icurobj][0] < 255) {
                for (int ir = 0; ir < nscansize; ir++) {
                  image.setPixel(m_objlistscanorA[ir].X(),
                                 m_objlistscanorA[ir].Y(), cv::Vec3b(0, 0, 0));
                }
                gp_Pnt apoint(m_objlistscanorA[0].X(), m_objlistscanorA[0].Y(),
                              0);
                m_keypoint.addpoint(apoint);

                m_rectresults.addrect(arectresult);
                m_scanid.push_back(nScanerID);
                m_vobjnum.push_back(nscansize);

                m_totalarea = m_totalarea + nscansize;
                m_iobjnum++;
              }
              ishowfont++;
              m_icurobj++;
            }

          } break;
          case 102: {
            {
              gp_Rectangle arectresult(gp_Pnt(iminx, iminy, 0),
                                       gp_Pnt(imaxx, imaxy, 0));
              if (m_vborw[m_icurobj][0] < 255) {
                for (int ir = 0; ir < nscansize; ir++) {
                  image.setPixel(m_objlistscanorA[ir].X(),
                                 m_objlistscanorA[ir].Y(),
                                 cv::Vec3b(255, 255, 255));
                }
                gp_Pnt apoint(m_objlistscanorA[0].X(), m_objlistscanorA[0].Y(),
                              0);
                m_keypoint.addpoint(apoint);

                m_rectresults.addrect(arectresult);
                m_scanid.push_back(nScanerID);
                m_vobjnum.push_back(nscansize);

                m_totalarea = m_totalarea + nscansize;
                m_iobjnum++;
              }
              ishowfont++;
              m_icurobj++;
            }
          } break;
          case 13: {
            {
              if (m_vborw[m_icurobj][0] < 255) {
                for (int ir = 0; ir < nscansize; ir++) {
                  image.setPixel(m_objlistscanorA[ir].X(),
                                 m_objlistscanorA[ir].Y(),
                                 cv::Vec3b(255, 0, 0));
                }
              }
              if (m_vborw[m_icurobj][0] > 0) {
                for (int ir = 0; ir < nscansize; ir++) {
                  image.setPixel(m_objlistscanorA[ir].X(),
                                 m_objlistscanorA[ir].Y(),
                                 cv::Vec3b(0, 0, 255));
                }
              }
              ishowfont++;
              m_icurobj++;
            }
          } break;
          case 901: {
            if (m_vborw[m_icurobj][0] < 255) {
              TwoPointsShape atpshape;
              for (int ir = 0; ir < nscansize; ir++) {
                atpshape.addpoint(m_objlistscanorA[ir].X(),
                                  m_objlistscanorA[ir].Y());
              }
              m_cent_h_bw_points_v.push_back(atpshape);
            }
            if (m_vborw[m_icurobj][0] > 0) {
              TwoPointsShape atpshape;
              for (int ir = 0; ir < nscansize; ir++) {
                atpshape.addpoint(m_objlistscanorA[ir].X(),
                                  m_objlistscanorA[ir].Y());
              }
              m_cent_h_bw_points_v.push_back(atpshape);
            }
            ishowfont++;
            m_icurobj++;
          } break;
          case 902: {
            if (m_vborw[m_icurobj][0] < 255) {
              TwoPointsShape atpshape;
              for (int ir = 0; ir < nscansize; ir++) {
                atpshape.addpoint(m_objlistscanorA[ir].Y(),
                                  m_objlistscanorA[ir].X());
              }
              m_cent_v_bw_points_v.push_back(atpshape);
            }
            if (m_vborw[m_icurobj][0] > 0) {
              TwoPointsShape atpshape;
              for (int ir = 0; ir < nscansize; ir++) {
                atpshape.addpoint(m_objlistscanorA[ir].Y(),
                                  m_objlistscanorA[ir].X());
              }
              m_cent_v_bw_points_v.push_back(atpshape);
            }
            ishowfont++;
            m_icurobj++;
          } break;
          case 903: {
            if (m_vborw[m_icurobj][0] < 255) {
              TwoPointsShape atpshape;
              for (int ir = 0; ir < nscansize; ir++) {
                atpshape.addpoint(m_objlistscanorA[ir].X(),
                                  m_objlistscanorA[ir].Y());
              }
              m_cent_h_wb_points_v.push_back(atpshape);
            }
            if (m_vborw[m_icurobj][0] > 0) {
              TwoPointsShape atpshape;
              for (int ir = 0; ir < nscansize; ir++) {
                atpshape.addpoint(m_objlistscanorA[ir].X(),
                                  m_objlistscanorA[ir].Y());
              }
              m_cent_h_wb_points_v.push_back(atpshape);
            }
            ishowfont++;
            m_icurobj++;
          } break;
          case 904: {
            if (m_vborw[m_icurobj][0] < 255) {
              TwoPointsShape atpshape;
              for (int ir = 0; ir < nscansize; ir++) {
                atpshape.addpoint(m_objlistscanorA[ir].Y(),
                                  m_objlistscanorA[ir].X());
              }
              m_cent_v_wb_points_v.push_back(atpshape);
            }
            if (m_vborw[m_icurobj][0] > 0) {
              TwoPointsShape atpshape;
              for (int ir = 0; ir < nscansize; ir++) {
                atpshape.addpoint(m_objlistscanorA[ir].Y(),
                                  m_objlistscanorA[ir].X());
              }
              m_cent_v_wb_points_v.push_back(atpshape);
            }
            ishowfont++;
            m_icurobj++;
          } break;
          }
        }
      } else {
        switch (m_iborw) {
        case 21: {
          for (int ir = 0; ir < nscansize; ir++) {
            image.setPixel(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y(),
                           cv::Vec3b(0, 0, 0));
          }
        } break;
        case 22: {
          for (int ir = 0; ir < nscansize; ir++) {
            image.setPixel(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y(),
                           cv::Vec3b(255, 255, 255));
          }
        } break;
        case 23: {
          for (int ir = 0; ir < nscansize; ir++) {
            image.setPixel(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y(),
                           cv::Vec3b(255, 0, 0));
          }
        } break;
        }
      }
    } else {
      switch (m_iborw) {
      case 21: {
        for (int ir = 0; ir < nscansize; ir++) {
          image.setPixel(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y(),
                         cv::Vec3b(0, 0, 0));
        }
      } break;
      case 22: {
        for (int ir = 0; ir < nscansize; ir++) {
          image.setPixel(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y(),
                         cv::Vec3b(255, 255, 255));
        }
      } break;
      case 23: {
        for (int ir = 0; ir < nscansize; ir++) {
          image.setPixel(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y(),
                         cv::Vec3b(255, 0, 0));
        }
      } break;
      }
    }

    iminx = 9999;
    iminy = 9999;
    imaxx = 0;
    imaxy = 0;

    nScanerID++;
    icurSeekNum++;
  FOREND:;
  }

  switch (m_iborw) {
  case 901: {
    int isize = m_cent_h_bw_points_v.size();
    for (int i = 0; i < isize; i++)
      m_cent_h_bw_points_v[i].makepath(0);

  } break;
  case 902: {
    int isize = m_cent_v_bw_points_v.size();
    for (int i = 0; i < isize; i++)
      m_cent_v_bw_points_v[i].makepath(1);
  } break;
  case 903: {
    int isize = m_cent_h_wb_points_v.size();
    for (int i = 0; i < isize; i++)
      m_cent_h_wb_points_v[i].makepath(0);

  } break;
  case 904: {
    int isize = m_cent_v_wb_points_v.size();
    for (int i = 0; i < isize; i++)
      m_cent_v_wb_points_v[i].makepath(1);
  } break;
  }

  FinalizeRegionGrowthDebugCounters();
  CLEAR_SCANOR();
  CLEAR_SEARCHSEEK();
}

void FindObject::MeasureFast(Image &image) {
  m_debug_algorithm_branch = "region_growth_fast";
  m_pgetimage = &image;
  m_measurements.clear();
  if (image.getmat().empty())
    return;
  if (!RefreshAlgorithmRuntimeResources(image.getWidth(), image.getHeight()))
    return;
  if (image.getWidth() < rect().TopLeft().X() + rect().Width() ||
      image.getHeight() < rect().TopLeft().Y() + rect().Height())
    return;
  int iw = rect().Width();
  int ih = rect().Height();
  int ix = rect().TopLeft().X();
  int iy = rect().TopLeft().Y();
  if (iw <= 0 || ih <= 0 || ix < 0 || iy < 0)
    return;
  if (g_pmapimage == nullptr || m_objlistscanorA == nullptr ||
      m_objlistcollectorA == nullptr)
    return;
  if (g_pmapimage->getWidth() < ix + iw || g_pmapimage->getHeight() < iy + ih)
    return;
  int ix1 = ix + iw;
  int iy1 = iy + ih;

  int m_isearchfirstx = rect().TopLeft().X();
  int m_isearchfirsty = rect().TopLeft().Y();

  int nx0 = 0;
  int ny0 = 0;
  int nx = 0;
  int ny = 0;
  int nservice = 0;
  cv::Vec3b abyte, bytenext;

  int nScanerID = 1;
  int icurScanerNUM = 0;
  bool boverflow = false;

  ncurscan = -1;
  nscansize = 0;

  ncursearchseek = -1;
  nsearchseeksize = 0;

  int ishowfont = 0;
  int mapservice = 0;
  int mapanalysis = 0;
  int mapedge = 0;

  int iminx = 9999;
  int iminy = 9999;
  int imaxx = 0;
  int imaxy = 0;

  int iborw = 0;

  switch (m_iborw) {
  case 901: {
    int isize = m_cent_h_bw_points_v.size();
    for (int i = 0; i < isize; i++)
      m_cent_h_bw_points_v[i].clear();
    m_cent_h_bw_points_v.clear();
  } break;
  case 902: {
    int isize = m_cent_v_bw_points_v.size();
    for (int i = 0; i < isize; i++)
      m_cent_v_bw_points_v[i].clear();
    m_cent_v_bw_points_v.clear();
  } break;
  case 903: {
    int isize = m_cent_h_wb_points_v.size();
    for (int i = 0; i < isize; i++)
      m_cent_h_wb_points_v[i].clear();
    m_cent_h_wb_points_v.clear();
  } break;
  case 904: {
    int isize = m_cent_v_wb_points_v.size();
    for (int i = 0; i < isize; i++)
      m_cent_v_wb_points_v[i].clear();
    m_cent_v_wb_points_v.clear();
  } break;
  }

  m_icurobj = 0;
  m_totalarea = 0;
  m_debug_component_count = 0;
  m_debug_accepted_count = 0;
  m_debug_rejected_count = 0;
  m_debug_max_component_area = 0;
  m_debug_max_component_w = 0;
  m_debug_max_component_h = 0;

  m_scanid.clear();
  m_vborw.clear();
  m_vrow.clear();
  m_vobjnum.clear();
  m_rectresults.clear();
  m_keypoint.clear();
  m_fitwh.clear();
  ;
  MAPCLEAR();

  CLEAR_SEARCHSEEK();
  PUSH_SEARCHSEEK(m_isearchfirstx, m_isearchfirsty);

  for (int icurSeekNum = 0; icurSeekNum < nsearchseeksize;) {
  FORBEGIN:
    if (icurSeekNum < 0 || icurSeekNum >= nsearchseeksize)
      break;
    const int seekx = m_objlistcollectorA[icurSeekNum].X();
    const int seeky = m_objlistcollectorA[icurSeekNum].Y();
    mapservice = GetServiceValue(MAP(seekx, seeky));

    if (mapservice > 0) {
      icurSeekNum++;

      if (icurSeekNum >= nsearchseeksize)
        break;
      else
        goto FORBEGIN;
    }
    iminx = 9999;
    iminy = 9999;
    imaxx = 0;
    imaxy = 0;

    CLEAR_SCANOR();
    PUSH_SCANOR(seekx, seeky);
    if (nscansize <= 0)
      break;
    iminx = static_cast<int>(seekx);
    iminy = static_cast<int>(seeky);
    imaxx = iminx;
    imaxy = iminy;
    SetMAP_service(seekx, seeky, nScanerID);
    icurScanerNUM = 0;

  CURSCANERBEGIN:
    while (icurScanerNUM != nscansize) {
      nx0 = m_objlistscanorA[icurScanerNUM].X();
      ny0 = m_objlistscanorA[icurScanerNUM].Y();
      abyte = image.pixel(nx0, ny0);
      mapanalysis = MAP_analysis(nx0, ny0);

      if (mapanalysis == FindObject::ANLAYSIS_OVER)
        goto NEXTFINDSTEP;
      for (int i = 0; i < m_idistance; i++) {
        nx = nx0 + m_SearchPointGroup[i].X();
        ny = ny0 + m_SearchPointGroup[i].Y();
        if (nx < ix || ny < iy || nx >= ix1 || ny >= iy1)
          continue;
        const MapState map_state = DecodeMapState(MAP(nx, ny));
        mapanalysis = map_state.analysis;
        mapservice = map_state.service;
        mapedge = map_state.edge;
        if (mapanalysis == ANLAYSIS_OVER || mapservice > 0 ||
            (mapedge == mapservice && 0 != mapedge))
          continue;
        bytenext = image.pixel(nx, ny);

        if (IsSamePixel(abyte, bytenext)) {
          PUSH_SCANOR(nx, ny);
          iborw = (abyte[0] == 0) ? 0 : 1;
          SetMAP_service_pixel(nx, ny, nScanerID, iborw);
          if (iminx > nx)
            iminx = nx;
          if (iminy > ny)
            iminy = ny;
          if (imaxx < nx)
            imaxx = nx;
          if (imaxy < ny)
            imaxy = ny;
        } else {
          PUSH_SEARCHSEEK(nx, ny);
          SetMAP_edge(nx, ny, nScanerID);
        }
      }
      SetMAP_analysis(nx0, ny0, ANLAYSIS_OVER);
    NEXTFINDSTEP:
      icurScanerNUM++;
    }
  CURSCANEREND:
    if (nscansize > 0)
      m_debug_component_count++;
    int iobjw = (imaxx - iminx <= 0) ? 1 : imaxx - iminx;
    int iobjh = (imaxy - iminy <= 0) ? 1 : imaxy - iminy;
    ObserveDebugComponent(nscansize, iobjw, iobjh);
    if ((m_ifilterNedge > 0 &&
         (iminx > m_ifilterNedge && imaxx < iw - m_ifilterNedge &&
          iminy > m_ifilterNedge && imaxy < ih - m_ifilterNedge)) ||
        m_ifilterNedge == 0) {
      if (nscansize > m_iminarea && nscansize < m_imaxarea &&
          iobjw < m_imaxobjw && iobjw >= m_iminobjw && iobjh < m_imaxobjh &&
          iobjh >= m_iminobjh) {
        m_vrow.push_back(nscansize);

        abyte = image.pixel(m_objlistscanorA[0].X(), m_objlistscanorA[0].Y());

        m_vborw.push_back(abyte);

        boverflow = false;
        {
          switch (m_iborw) {
          case 3: {
            gp_Rectangle arectresult(gp_Pnt(iminx, iminy, 0),
                                     gp_Pnt(imaxx, imaxy, 0));
            gp_Pnt apoint(m_objlistscanorA[0].X(), m_objlistscanorA[0].Y(), 0);
            m_keypoint.addpoint(apoint);

            m_rectresults.addrect(arectresult);
            m_scanid.push_back(nScanerID);
            m_vobjnum.push_back(nscansize);
            m_iobjnum++;
          }
            ishowfont++;
            m_icurobj++;

            break;
          case 0:
            break;
          case 1: {
            {
              gp_Rectangle arectresult(gp_Pnt(iminx, iminy, 0),
                                       gp_Pnt(imaxx, imaxy, 0));
              if (m_vborw[m_icurobj][0] > 0) {
                gp_Pnt apoint(m_objlistscanorA[0].X(), m_objlistscanorA[0].Y(),
                              0);
                m_keypoint.addpoint(apoint);
                m_rectresults.addrect(arectresult);
                m_scanid.push_back(nScanerID);
                m_vobjnum.push_back(nscansize);
                m_totalarea = m_totalarea + nscansize;
                m_iobjnum++;
              }
              ishowfont++;
              m_icurobj++;
            }
          } break;
          case 2: {
            {
              gp_Rectangle arectresult(gp_Pnt(iminx, iminy, 0),
                                       gp_Pnt(imaxx, imaxy, 0));
              if (m_vborw[m_icurobj][0] < 255) {
                gp_Pnt apoint(m_objlistscanorA[0].X(), m_objlistscanorA[0].Y(),
                              0);
                m_keypoint.addpoint(apoint);

                m_rectresults.addrect(arectresult);
                m_scanid.push_back(nScanerID);
                m_vobjnum.push_back(nscansize);

                m_totalarea = m_totalarea + nscansize;
                m_iobjnum++;
              }
              ishowfont++;
              m_icurobj++;
            }
          } break;

          case 11: {

            gp_Rectangle arectresult(gp_Pnt(iminx, iminy, 0),
                                     gp_Pnt(imaxx, imaxy, 0));
            if (m_vborw[m_icurobj][0] > 0) {
              for (int ir = 0; ir < nscansize; ir++) {
                image.setPixel(m_objlistscanorA[ir].X(),
                               m_objlistscanorA[ir].Y(), cv::Vec3b(0, 0, 0));
              }
              gp_Pnt apoint(m_objlistscanorA[0].X(), m_objlistscanorA[0].Y(),
                            0);
              m_keypoint.addpoint(apoint);
              m_rectresults.addrect(arectresult);
              m_scanid.push_back(nScanerID);
              m_vobjnum.push_back(nscansize);

              m_totalarea = m_totalarea + nscansize;
              m_iobjnum++;
            }
            ishowfont++;
            m_icurobj++;

          } break;
          case 101: {

            gp_Rectangle arectresult(gp_Pnt(iminx, iminy, 0),
                                     gp_Pnt(imaxx, imaxy, 0));
            if (m_vborw[m_icurobj][0] > 0) {
              for (int ir = 0; ir < nscansize; ir++) {
                image.setPixel(m_objlistscanorA[ir].X(),
                               m_objlistscanorA[ir].Y(),
                               cv::Vec3b(255, 255, 255));
              }
              gp_Pnt apoint(m_objlistscanorA[0].X(), m_objlistscanorA[0].Y(),
                            0);
              m_keypoint.addpoint(apoint);
              m_rectresults.addrect(arectresult);
              m_scanid.push_back(nScanerID);
              m_vobjnum.push_back(nscansize);

              m_totalarea = m_totalarea + nscansize;
              m_iobjnum++;
            }
            ishowfont++;
            m_icurobj++;

          } break;
          case 12: {
            {
              gp_Rectangle arectresult(gp_Pnt(iminx, iminy, 0),
                                       gp_Pnt(imaxx, imaxy, 0));
              if (m_vborw[m_icurobj][0] < 255) {
                for (int ir = 0; ir < nscansize; ir++) {
                  image.setPixel(m_objlistscanorA[ir].X(),
                                 m_objlistscanorA[ir].Y(), cv::Vec3b(0, 0, 0));
                }
                gp_Pnt apoint(m_objlistscanorA[0].X(), m_objlistscanorA[0].Y(),
                              0);
                m_keypoint.addpoint(apoint);

                m_rectresults.addrect(arectresult);
                m_scanid.push_back(nScanerID);
                m_vobjnum.push_back(nscansize);

                m_totalarea = m_totalarea + nscansize;
                m_iobjnum++;
              }
              ishowfont++;
              m_icurobj++;
            }

          } break;
          case 102: {
            {
              gp_Rectangle arectresult(gp_Pnt(iminx, iminy, 0),
                                       gp_Pnt(imaxx, imaxy, 0));
              if (m_vborw[m_icurobj][0] < 255) {
                for (int ir = 0; ir < nscansize; ir++) {
                  image.setPixel(m_objlistscanorA[ir].X(),
                                 m_objlistscanorA[ir].Y(),
                                 cv::Vec3b(255, 255, 255));
                }
                gp_Pnt apoint(m_objlistscanorA[0].X(), m_objlistscanorA[0].Y(),
                              0);
                m_keypoint.addpoint(apoint);

                m_rectresults.addrect(arectresult);
                m_scanid.push_back(nScanerID);
                m_vobjnum.push_back(nscansize);

                m_totalarea = m_totalarea + nscansize;
                m_iobjnum++;
              }
              ishowfont++;
              m_icurobj++;
            }
          } break;
          case 13: {
            {
              if (m_vborw[m_icurobj][0] < 255) {
                for (int ir = 0; ir < nscansize; ir++) {
                  image.setPixel(m_objlistscanorA[ir].X(),
                                 m_objlistscanorA[ir].Y(),
                                 cv::Vec3b(255, 0, 0));
                }
              }
              if (m_vborw[m_icurobj][0] > 0) {
                for (int ir = 0; ir < nscansize; ir++) {
                  image.setPixel(m_objlistscanorA[ir].X(),
                                 m_objlistscanorA[ir].Y(),
                                 cv::Vec3b(0, 0, 255));
                }
              }
              ishowfont++;
              m_icurobj++;
            }
          } break;
          case 901: {
            if (m_vborw[m_icurobj][0] < 255) {
              TwoPointsShape atpshape;
              for (int ir = 0; ir < nscansize; ir++) {
                atpshape.addpoint(m_objlistscanorA[ir].X(),
                                  m_objlistscanorA[ir].Y());
              }
              m_cent_h_bw_points_v.push_back(atpshape);
            }
            if (m_vborw[m_icurobj][0] > 0) {
              TwoPointsShape atpshape;
              for (int ir = 0; ir < nscansize; ir++) {
                atpshape.addpoint(m_objlistscanorA[ir].X(),
                                  m_objlistscanorA[ir].Y());
              }
              m_cent_h_bw_points_v.push_back(atpshape);
            }
            ishowfont++;
            m_icurobj++;
          } break;
          case 902: {
            if (m_vborw[m_icurobj][0] < 255) {
              TwoPointsShape atpshape;
              for (int ir = 0; ir < nscansize; ir++) {
                atpshape.addpoint(m_objlistscanorA[ir].Y(),
                                  m_objlistscanorA[ir].X());
              }
              m_cent_v_bw_points_v.push_back(atpshape);
            }
            if (m_vborw[m_icurobj][0] > 0) {
              TwoPointsShape atpshape;
              for (int ir = 0; ir < nscansize; ir++) {
                atpshape.addpoint(m_objlistscanorA[ir].Y(),
                                  m_objlistscanorA[ir].X());
              }
              m_cent_v_bw_points_v.push_back(atpshape);
            }
            ishowfont++;
            m_icurobj++;
          } break;
          case 903: {
            if (m_vborw[m_icurobj][0] < 255) {
              TwoPointsShape atpshape;
              for (int ir = 0; ir < nscansize; ir++) {
                atpshape.addpoint(m_objlistscanorA[ir].X(),
                                  m_objlistscanorA[ir].Y());
              }
              m_cent_h_wb_points_v.push_back(atpshape);
            }
            if (m_vborw[m_icurobj][0] > 0) {
              TwoPointsShape atpshape;
              for (int ir = 0; ir < nscansize; ir++) {
                atpshape.addpoint(m_objlistscanorA[ir].X(),
                                  m_objlistscanorA[ir].Y());
              }
              m_cent_h_wb_points_v.push_back(atpshape);
            }
            ishowfont++;
            m_icurobj++;
          } break;
          case 904: {
            if (m_vborw[m_icurobj][0] < 255) {
              TwoPointsShape atpshape;
              for (int ir = 0; ir < nscansize; ir++) {
                atpshape.addpoint(m_objlistscanorA[ir].Y(),
                                  m_objlistscanorA[ir].X());
              }
              m_cent_v_wb_points_v.push_back(atpshape);
            }
            if (m_vborw[m_icurobj][0] > 0) {
              TwoPointsShape atpshape;
              for (int ir = 0; ir < nscansize; ir++) {
                atpshape.addpoint(m_objlistscanorA[ir].Y(),
                                  m_objlistscanorA[ir].X());
              }
              m_cent_v_wb_points_v.push_back(atpshape);
            }
            ishowfont++;
            m_icurobj++;
          } break;
          }
        }
      } else {
        switch (m_iborw) {
        case 21: {
          for (int ir = 0; ir < nscansize; ir++) {
            image.setPixel(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y(),
                           cv::Vec3b(0, 0, 0));
          }
        } break;
        case 22: {
          for (int ir = 0; ir < nscansize; ir++) {
            image.setPixel(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y(),
                           cv::Vec3b(255, 255, 255));
          }
        } break;
        case 23: {
          for (int ir = 0; ir < nscansize; ir++) {
            image.setPixel(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y(),
                           cv::Vec3b(255, 0, 0));
          }
        } break;
        }
      }
    } else {
      switch (m_iborw) {
      case 21: {
        for (int ir = 0; ir < nscansize; ir++) {
          image.setPixel(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y(),
                         cv::Vec3b(0, 0, 0));
        }
      } break;
      case 22: {
        for (int ir = 0; ir < nscansize; ir++) {
          image.setPixel(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y(),
                         cv::Vec3b(255, 255, 255));
        }
      } break;
      case 23: {
        for (int ir = 0; ir < nscansize; ir++) {
          image.setPixel(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y(),
                         cv::Vec3b(255, 0, 0));
        }
      } break;
      }
    }

    iminx = 9999;
    iminy = 9999;
    imaxx = 0;
    imaxy = 0;

    nScanerID++;
    icurSeekNum++;
  FOREND:;
  }

  switch (m_iborw) {
  case 901: {
    int isize = m_cent_h_bw_points_v.size();
    for (int i = 0; i < isize; i++)
      m_cent_h_bw_points_v[i].makepath(0);

  } break;
  case 902: {
    int isize = m_cent_v_bw_points_v.size();
    for (int i = 0; i < isize; i++)
      m_cent_v_bw_points_v[i].makepath(1);
  } break;
  case 903: {
    int isize = m_cent_h_wb_points_v.size();
    for (int i = 0; i < isize; i++)
      m_cent_h_wb_points_v[i].makepath(0);

  } break;
  case 904: {
    int isize = m_cent_v_wb_points_v.size();
    for (int i = 0; i < isize; i++)
      m_cent_v_wb_points_v[i].makepath(1);
  } break;
  }

  FinalizeRegionGrowthDebugCounters();
  CLEAR_SCANOR();
  CLEAR_SEARCHSEEK();
}

void FindObject::MeasureConnectedComponents(Image &image) {
  const bool selection_mask_mode = (m_iborw == 21 || m_iborw == 22);
  const bool select_white_mask = (m_iborw == 21);
  m_debug_algorithm_branch = selection_mask_mode
                                 ? "connected_components_selection_mask"
                                 : "connected_components";
  m_pgetimage = &image;
  m_measurements.clear();
  if (image.getmat().empty())
    return;
  if (!RefreshAlgorithmRuntimeResources(image.getWidth(), image.getHeight()))
    return;
  if (image.getWidth() < rect().TopLeft().X() + rect().Width() ||
      image.getHeight() < rect().TopLeft().Y() + rect().Height())
    return;
  int iw = rect().Width();
  int ih = rect().Height();
  int ix = rect().TopLeft().X();
  int iy = rect().TopLeft().Y();
  if (iw <= 0 || ih <= 0 || ix < 0 || iy < 0)
    return;
  m_icurobj = 0;
  m_iobjnum = 0;
  m_totalarea = 0;
  m_debug_component_count = 0;
  m_debug_accepted_count = 0;
  m_debug_rejected_count = 0;
  m_debug_max_component_area = 0;
  m_debug_max_component_w = 0;
  m_debug_max_component_h = 0;
  m_scanid.clear();
  m_vborw.clear();
  m_vrow.clear();
  m_vobjnum.clear();
  m_rectresults.clear();
  m_keypoint.clear();
  m_fitwh.clear();
  if (g_pmapimage != nullptr && g_pmapimage->getWidth() >= ix + iw &&
      g_pmapimage->getHeight() >= iy + ih) {
    MAPCLEAR();
  }
  CLEAR_SCANOR();
  CLEAR_SEARCHSEEK();

  const cv::Mat &source = image.getmat();
  const cv::Rect roi_rect(ix, iy, iw, ih);
  cv::Mat roi = source(roi_rect);
  cv::Mat channel;
  if (roi.channels() == 1)
    channel = roi;
  else
    cv::extractChannel(roi, channel, 0);
  if (channel.depth() != CV_8U)
    channel.convertTo(channel, CV_8U);
  const FindObjectBackgroundBuild background =
      BuildFindObjectBackgroundResidual(channel, m_measurement_config);
  if (!background.residual.empty())
    channel = background.residual;
  m_last_background_method = background.method;
  m_last_background_valid = background.valid;
  m_last_background_sample_count = background.sample_count;
  m_last_background_baseline_mean = background.baseline_mean;
  m_last_residual_threshold = static_cast<double>(m_imagethre);
  m_last_segmentation_domain =
      background.corrected ? "background_corrected_residual" : "raw_intensity";

  int nScanerID = 1;
  cv::Mat selection_mask;
  if (selection_mask_mode) {
    selection_mask = cv::Mat(roi.rows, roi.cols, CV_8UC1, cv::Scalar(0));
  }

  const auto accept_component =
      [&](int local_x, int local_y, int comp_w, int comp_h, int area,
          const cv::Point2d &centroid, bool is_white_region) -> bool {
    m_debug_component_count++;
    const int iminx = ix + local_x;
    const int iminy = iy + local_y;
    const int imaxx = ix + local_x + comp_w - 1;
    const int imaxy = iy + local_y + comp_h - 1;
    const int iobjw = (comp_w <= 1) ? 1 : comp_w - 1;
    const int iobjh = (comp_h <= 1) ? 1 : comp_h - 1;
    ObserveDebugComponent(area, iobjw, iobjh);

    const bool inside_edge_filter =
        (m_ifilterNedge > 0 && local_x > m_ifilterNedge &&
         local_x + comp_w - 1 < iw - m_ifilterNedge &&
         local_y > m_ifilterNedge &&
         local_y + comp_h - 1 < ih - m_ifilterNedge) ||
        m_ifilterNedge == 0;
    if (!inside_edge_filter) {
      m_debug_rejected_count++;
      return false;
    }
    if (area <= m_iminarea || area >= m_imaxarea) {
      m_debug_rejected_count++;
      return false;
    }

    const bool accept_white = selection_mask_mode
                                  ? select_white_mask
                                  : (m_iborw == 1 || m_iborw == 3);
    const bool accept_black = selection_mask_mode
                                  ? !select_white_mask
                                  : (m_iborw == 2 || m_iborw == 3);
    if ((is_white_region && !accept_white) ||
        (!is_white_region && !accept_black)) {
      m_debug_rejected_count++;
      return false;
    }

    m_vrow.push_back(area);
    m_vborw.push_back(is_white_region ? cv::Vec3b(255, 255, 255)
                                      : cv::Vec3b(0, 0, 0));

    gp_Rectangle arectresult(gp_Pnt(iminx, iminy, 0), gp_Pnt(imaxx, imaxy, 0));
    gp_Pnt apoint(static_cast<int>(ix + centroid.x),
                  static_cast<int>(iy + centroid.y), 0);
    m_keypoint.addpoint(apoint);
    m_rectresults.addrect(arectresult);
    m_scanid.push_back(nScanerID);
    m_vobjnum.push_back(area);
    m_totalarea = m_totalarea + area;
    m_iobjnum++;
    m_icurobj++;
    m_debug_accepted_count++;
    nScanerID++;
    return true;
  };

  const auto run_connected_components = [&](bool is_white_region) {
    cv::Mat mask;
    // Connected components must receive a real binary foreground mask.  A
    // JPEG background is often non-zero, so comparing against 0 incorrectly
    // turns the entire ROI into one white component.
    const double foreground_threshold =
        static_cast<double>(std::max(0, std::min(255, m_imagethre)));
    if (is_white_region) {
      cv::threshold(channel, mask, foreground_threshold, 255,
                    cv::THRESH_BINARY);
    } else {
      const double dark_threshold =
          background.corrected ? -foreground_threshold : foreground_threshold;
      cv::threshold(channel, mask, dark_threshold, 255,
                    cv::THRESH_BINARY_INV);
    }
    if (mask.depth() != CV_8U)
      mask.convertTo(mask, CV_8U);

    cv::Mat labels;
    cv::Mat stats;
    cv::Mat centroids;
    const int component_count = cv::connectedComponentsWithStats(
        mask, labels, stats, centroids, m_measurement_config.connectivity, CV_32S);
    for (int label = 1; label < component_count; ++label) {
      if (accept_component(stats.at<int>(label, cv::CC_STAT_LEFT),
                           stats.at<int>(label, cv::CC_STAT_TOP),
                           stats.at<int>(label, cv::CC_STAT_WIDTH),
                           stats.at<int>(label, cv::CC_STAT_HEIGHT),
                           stats.at<int>(label, cv::CC_STAT_AREA),
                           cv::Point2d(centroids.at<double>(label, 0),
                                       centroids.at<double>(label, 1)),
                           is_white_region)) {

        StoreAcceptedLabelMask(labels, label, nScanerID - 1, ix, iy);
        if (selection_mask_mode) {
          selection_mask.setTo(cv::Scalar(255), labels == label);
        }
      }
    }
  };

  if (m_iborw == 1 || m_iborw == 3 || select_white_mask)
    run_connected_components(true);
  if (m_iborw == 2 || m_iborw == 3 ||
      (selection_mask_mode && !select_white_mask))
    run_connected_components(false);

  FinalizeRegionGrowthDebugCounters();

  if (selection_mask_mode) {
    for (int y = 0; y < selection_mask.rows; ++y) {
      for (int x = 0; x < selection_mask.cols; ++x) {
        const uchar value = selection_mask.at<uchar>(y, x);
        image.setPixel(ix + x, iy + y, cv::Vec3b(value, value, value));
      }
    }
  }
}

std::vector<cv::Point>
FindObject::DetectPeakSeeds(const cv::Mat &distance_map,
                            double min_peak_distance) const {
  std::vector<cv::Point> seeds;
  if (distance_map.empty())
    return seeds;

  double min_val = 0, max_val = 0;
  cv::minMaxLoc(distance_map, &min_val, &max_val);

  if (max_val <= 0)
    return seeds;

  double threshold = std::max(min_peak_distance, max_val * 0.3);

  cv::Mat peak_mask;
  cv::threshold(distance_map, peak_mask, threshold, 255, cv::THRESH_BINARY);
  peak_mask.convertTo(peak_mask, CV_8U);

  cv::Mat labels, stats, centroids;
  int n_labels = cv::connectedComponentsWithStats(peak_mask, labels, stats,
                                                  centroids, m_measurement_config.connectivity, CV_32S);

  for (int i = 1; i < n_labels; ++i) {
    int area = stats.at<int>(i, cv::CC_STAT_AREA);
    if (area >= 1) {
      double cx = centroids.at<double>(i, 0);
      double cy = centroids.at<double>(i, 1);
      seeds.emplace_back(static_cast<int>(cx), static_cast<int>(cy));
    }
  }

  if (seeds.empty()) {
    int rows = distance_map.rows;
    int cols = distance_map.cols;
    for (int y = 0; y < rows && seeds.size() < 10; ++y) {
      for (int x = 0; x < cols && seeds.size() < 10; ++x) {
        if (distance_map.at<float>(y, x) >= min_peak_distance) {
          seeds.emplace_back(x, y);
        }
      }
    }
  }

  if (seeds.empty()) {
    cv::Point max_loc;
    cv::minMaxLoc(distance_map, &min_val, &max_val, &max_loc);
    if (max_val > min_peak_distance) {
      seeds.push_back(max_loc);
    }
  }

  std::sort(seeds.begin(), seeds.end(),
            [](const cv::Point &a, const cv::Point &b) {
              if (a.x != b.x)
                return a.x < b.x;
              return a.y < b.y;
            });

  return seeds;
}

cv::Rect FindObject::ComputeLocalSearchROI(const cv::Point &peak,
                                           const cv::Mat &distance_map,
                                           int max_edge_width) const {
  float max_dist = distance_map.at<float>(peak.y, peak.x);
  int radius =
      std::max(5, std::min(static_cast<int>(max_dist * 3.0f), max_edge_width));

  cv::Rect roi(peak.x - radius, peak.y - radius, 2 * radius, 2 * radius);
  roi &= cv::Rect(0, 0, distance_map.cols, distance_map.rows);
  return roi;
}

void FindObject::RunPreemptiveLocalBFS(
    const cv::Point &peak, const cv::Rect &roi, const cv::Mat &binary_image,
    cv::Mat &label_map, int component_id, std::vector<cv::Rect> &out_bboxes,
    int &out_area, bool is_white_region) const {
  if (binary_image.empty() || label_map.empty())
    return;
  if (!roi.contains(peak))
    return;

  std::queue<cv::Point> q;
  q.push(peak);
  label_map.at<int>(peak.y, peak.x) = component_id;

  int min_x = peak.x, max_x = peak.x;
  int min_y = peak.y, max_y = peak.y;
  int pixel_count = 0;

  int dx[8] = {-1, 1, 0, 0, -1, -1, 1, 1};
  int dy[8] = {0, 0, -1, 1, -1, 1, -1, 1};

  while (!q.empty()) {
    cv::Point curr = q.front();
    q.pop();
    pixel_count++;

    min_x = std::min(min_x, curr.x);
    max_x = std::max(max_x, curr.x);
    min_y = std::min(min_y, curr.y);
    max_y = std::max(max_y, curr.y);

    for (int i = 0; i < 8; ++i) {
      int nx = curr.x + dx[i];
      int ny = curr.y + dy[i];

      if (nx < roi.x || ny < roi.y || nx >= roi.x + roi.width ||
          ny >= roi.y + roi.height)
        continue;

      uchar pixel_val = binary_image.at<uchar>(ny, nx);
      bool is_foreground = is_white_region ? (pixel_val > 0) : (pixel_val == 0);
      if (!is_foreground)
        continue;

      int &label = label_map.at<int>(ny, nx);
      if (label == 0) {
        label = component_id;
        q.push(cv::Point(nx, ny));
      }
    }
  }

  if (pixel_count > 0) {
    out_bboxes.push_back(
        cv::Rect(min_x, min_y, max_x - min_x + 1, max_y - min_y + 1));
    out_area += pixel_count;
  }
}

void FindObject::CollectComponentFromLabels(const cv::Mat &labels,
                                            int component_id,
                                            const cv::Rect &roi, int &out_x,
                                            int &out_y, int &out_w,
                                            int &out_h) const {
  int min_x = roi.x + roi.width;
  int min_y = roi.y + roi.height;
  int max_x = roi.x;
  int max_y = roi.y;
  bool found = false;

  for (int y = roi.y; y < roi.y + roi.height; ++y) {
    for (int x = roi.x; x < roi.x + roi.width; ++x) {
      if (labels.at<int>(y, x) == component_id) {
        found = true;
        min_x = std::min(min_x, x);
        min_y = std::min(min_y, y);
        max_x = std::max(max_x, x);
        max_y = std::max(max_y, y);
      }
    }
  }

  if (found) {
    out_x = min_x;
    out_y = min_y;
    out_w = max_x - min_x + 1;
    out_h = max_y - min_y + 1;
  }
}

void FindObject::AcceptPeakComponent(int local_x, int local_y, int comp_w,
                                     int comp_h, int area,
                                     bool is_white_region) {
  m_debug_component_count++;

  int iminx = rect().TopLeft().X() + local_x;
  int iminy = rect().TopLeft().Y() + local_y;
  int imaxx = iminx + comp_w - 1;
  int imaxy = iminy + comp_h - 1;
  int iobjw = (comp_w <= 1) ? 1 : comp_w - 1;
  int iobjh = (comp_h <= 1) ? 1 : comp_h - 1;

  ObserveDebugComponent(area, iobjw, iobjh);

  int iw = rect().Width();
  int ih = rect().Height();
  bool inside_edge_filter =
      (m_ifilterNedge > 0 && local_x > m_ifilterNedge &&
       local_x + comp_w - 1 < iw - m_ifilterNedge && local_y > m_ifilterNedge &&
       local_y + comp_h - 1 < ih - m_ifilterNedge) ||
      m_ifilterNedge == 0;

  if (!inside_edge_filter) {
    m_debug_rejected_count++;
    return;
  }
  if (area <= 0) {
    m_debug_rejected_count++;
    return;
  }

  bool accept_white = (m_iborw == 1 || m_iborw == 3);
  bool accept_black = (m_iborw == 2 || m_iborw == 3);

  if ((is_white_region && !accept_white) ||
      (!is_white_region && !accept_black)) {
    m_debug_rejected_count++;
    return;
  }

  m_vrow.push_back(area);
  m_vborw.push_back(is_white_region ? cv::Vec3b(255, 255, 255)
                                    : cv::Vec3b(0, 0, 0));

  gp_Rectangle arectresult(gp_Pnt(iminx, iminy, 0), gp_Pnt(imaxx, imaxy, 0));

  gp_Pnt apoint(static_cast<int>(iminx + comp_w / 2),
                static_cast<int>(iminy + comp_h / 2), 0);
  m_keypoint.addpoint(apoint);
  m_rectresults.addrect(arectresult);
  m_scanid.push_back(m_iobjnum + 1);
  m_vobjnum.push_back(area);
  m_totalarea = m_totalarea + area;
  m_iobjnum++;
  m_icurobj++;
  m_debug_accepted_count++;
}

void FindObject::MeasurePeakLocalBFS(Image &image) {
  m_debug_algorithm_branch = "peak_local_bfs_component_candidates";
  m_pgetimage = &image;
  m_measurements.clear();
  if (image.getmat().empty())
    return;
  if (!RefreshAlgorithmRuntimeResources(image.getWidth(), image.getHeight()))
    return;

  int iw = rect().Width();
  int ih = rect().Height();
  int ix = rect().TopLeft().X();
  int iy = rect().TopLeft().Y();
  if (iw <= 0 || ih <= 0 || ix < 0 || iy < 0)
    return;
  if (image.getWidth() < ix + iw || image.getHeight() < iy + ih)
    return;

  m_icurobj = 0;
  m_iobjnum = 0;
  m_totalarea = 0;
  m_debug_component_count = 0;
  m_debug_accepted_count = 0;
  m_debug_rejected_count = 0;
  m_debug_max_component_area = 0;
  m_debug_max_component_w = 0;
  m_debug_max_component_h = 0;

  m_scanid.clear();
  m_vborw.clear();
  m_vrow.clear();
  m_vobjnum.clear();
  m_rectresults.clear();
  m_keypoint.clear();
  m_fitwh.clear();

  if (g_pmapimage != nullptr && g_pmapimage->getWidth() >= ix + iw &&
      g_pmapimage->getHeight() >= iy + ih) {
    MAPCLEAR();
  }

  CLEAR_SCANOR();
  CLEAR_SEARCHSEEK();

  const cv::Mat &source = image.getmat();
  cv::Rect roi_rect(ix, iy, iw, ih);
  cv::Mat roi = source(roi_rect);

  cv::Mat channel;
  if (roi.channels() == 1)
    channel = roi;
  else
    cv::extractChannel(roi, channel, 0);
  if (channel.depth() != CV_8U)
    channel.convertTo(channel, CV_8U);
  const FindObjectBackgroundBuild background =
      BuildFindObjectBackgroundResidual(channel, m_measurement_config);
  if (!background.residual.empty())
    channel = background.residual;
  m_last_background_method = background.method;
  m_last_background_valid = background.valid;
  m_last_background_sample_count = background.sample_count;
  m_last_background_baseline_mean = background.baseline_mean;
  m_last_residual_threshold = static_cast<double>(m_imagethre);
  m_last_segmentation_domain =
      background.corrected ? "background_corrected_residual" : "raw_intensity";

  const auto process_region = [&](bool is_white_region) {
    cv::Mat binary;
    const double foreground_threshold =
        static_cast<double>(std::max(0, std::min(255, m_imagethre)));
    if (is_white_region)
      cv::threshold(channel, binary, foreground_threshold, 255,
                    cv::THRESH_BINARY);
    else
      cv::threshold(channel, binary,
                    background.corrected ? -foreground_threshold
                                         : foreground_threshold,
                    255, cv::THRESH_BINARY_INV);
    if (binary.depth() != CV_8U)
      binary.convertTo(binary, CV_8U);
    cv::Mat labels, stats, centroids;
    int n_labels = cv::connectedComponentsWithStats(binary, labels, stats,
                                                    centroids, m_measurement_config.connectivity, CV_32S);

    if (n_labels <= 1)
      return;

    for (int i = 1; i < n_labels; ++i) {
      int comp_area = stats.at<int>(i, cv::CC_STAT_AREA);
      if (comp_area <= 0)
        continue;

      int bb_x = stats.at<int>(i, cv::CC_STAT_LEFT);
      int bb_y = stats.at<int>(i, cv::CC_STAT_TOP);
      int bb_w = stats.at<int>(i, cv::CC_STAT_WIDTH);
      int bb_h = stats.at<int>(i, cv::CC_STAT_HEIGHT);

      int filter_w = (bb_w <= 1) ? 1 : bb_w - 1;
      int filter_h = (bb_h <= 1) ? 1 : bb_h - 1;

      bool area_ok = (comp_area >= m_iminarea && comp_area <= m_imaxarea);
      bool w_ok = (filter_w >= m_iminobjw && filter_w < m_imaxobjw);
      bool h_ok = (filter_h >= m_iminobjh && filter_h < m_imaxobjh);

      ObserveDebugComponent(comp_area, filter_w, filter_h);

      if (area_ok && w_ok && h_ok) {
        int edge_filter_margin = m_ifilterNedge;
        bool edge_ok = true;
        if (edge_filter_margin > 0) {
          int local_lx = bb_x;
          int local_ly = bb_y;
          int local_rx = bb_x + bb_w - 1;
          int local_ry = bb_y + bb_h - 1;
          if (local_lx <= edge_filter_margin ||
              local_ly <= edge_filter_margin ||
              local_rx >= iw - edge_filter_margin ||
              local_ry >= ih - edge_filter_margin)
            edge_ok = false;
        }

        if (edge_ok) {
          const int accepted_before = m_iobjnum;
          AcceptPeakComponent(bb_x, bb_y, bb_w, bb_h, comp_area,
                              is_white_region);
          if (m_iobjnum > accepted_before && !m_scanid.empty())
            StoreAcceptedLabelMask(labels, i, m_scanid.back(), ix, iy);
        } else {
          m_debug_rejected_count++;
        }
      } else {
        m_debug_rejected_count++;
      }
    }
  };

  bool run_white = (m_iborw == 1 || m_iborw == 3);
  bool run_black = (m_iborw == 2 || m_iborw == 3);

  if (run_white)
    process_region(true);

  if (run_black)
    process_region(false);

  FinalizeRegionGrowthDebugCounters();
}

void FindObject::MeasureGrid(Grid *pgrid) {}

void FindObject::Edge(int inum) {
  if (inum >= m_scanid.size() || inum < 0)
    return;
  int imapservice = m_scanid[inum];
  int ix = getresultx(inum);
  int iy = getresulty(inum);
  int iw = getresultw(inum);
  int ih = getresulth(inum);

  m_curedge.setshow(3);
  m_curedge.clear();

  for (int iy0 = 0; iy0 < ih; iy0++) {
    int ibeginx = 0;
    int iendx = iw - 1;
    for (int ix0 = 0; ix0 < iw; ix0++) {
      int imapservicecur = MAP_service(ix0 + ix, iy0 + iy);
      if (imapservicecur == imapservice) {

        ibeginx = ix0;
        break;
      }
    }
    for (int ix0 = iw - 1; ix0 >= 0; ix0--) {
      int imapservicecur = MAP_service(ix0 + ix, iy0 + iy);
      if (imapservicecur == imapservice) {
        iendx = ix0;
        break;
      }
    }
    Standard_Real qrx1 = ibeginx + ix;
    Standard_Real qry1 = iy0 + iy;
    Standard_Real qrx2 = iendx + ix;
    Standard_Real qry2 = iy0 + iy;

    m_curedge.addpointa(qrx1, qry1);
    m_curedge.addpointb(qrx2, qry2);
  }
}
void FindObject::Object(int inum) {
  if (inum >= m_scanid.size() || inum < 0)
    return;
  int imapservice = m_scanid[inum];
  int ix = getresultx(inum);
  int iy = getresulty(inum);
  int iw = getresultw(inum);
  int ih = getresulth(inum);

  m_curobject.setshow(3);
  m_curobject.clear();

  for (int iy0 = 0; iy0 < ih; iy0++) {
    for (int ix0 = 0; ix0 < iw; ix0++) {
      int imapservicecur = MAP_service(ix0 + ix, iy0 + iy);
      if (imapservicecur == imapservice) {
        Standard_Real qrx1 = ix0;
        Standard_Real qry1 = iy0;

        m_curobject.addpoint(qrx1, qry1);
      }
    }
  }
}

void FindObject::measure(void *pimage) {
  Image *pgetimage = (Image *)pimage;
  if (pgetimage == nullptr)
    return;
  Measure(*pgetimage);
}

void FindObject::measurefast(void *pimage) {
  Image *pgetimage = (Image *)pimage;
  if (pgetimage == nullptr)
    return;
  MeasureFast(*pgetimage);
}

void FindObject::measurecc(void *pimage) {
  Image *pgetimage = (Image *)pimage;
  if (pgetimage == nullptr)
    return;
  MeasureConnectedComponents(*pgetimage);
}

void FindObject::measurexbfs(void *pimage) {
  Image *pgetimage = (Image *)pimage;
  if (pgetimage == nullptr)
    return;
  MeasurePeakLocalBFS(*pgetimage);
}

void FindObject::MeasureXPeakLocalBFS(Image &image) {
  MeasurePeakLocalBFS(image);
}

void FindObject::measurexpeakbfs(void *pimage) {
  Image *pgetimage = (Image *)pimage;
  if (pgetimage == nullptr)
    return;
  MeasureXPeakLocalBFS(*pgetimage);
}

void FindObject::sethsogap(int ihgap, int isgap, int iogap) {
  m_ihgap = ihgap;
  m_isgap = isgap;
  m_iogap = iogap;
}
void FindObject::setminmaxarea(int imin, int imax) {
  m_iminarea = imin;
  m_imaxarea = imax;
}
void FindObject::MeasureX(Image &image) {
  m_pgetimage = &image;
  m_measurements.clear();
  if (image.getmat().empty())
    return;
  if (!RefreshAlgorithmRuntimeResources(image.getWidth(), image.getHeight()))
    return;
  if (image.getWidth() < rect().TopLeft().X() + rect().Width() ||
      image.getHeight() < rect().TopLeft().Y() + rect().Height())
    return;
  int iw = rect().Width();
  int ih = rect().Height();
  int ix = rect().TopLeft().X();
  int iy = rect().TopLeft().Y();
  int ix1 = ix + iw;
  int iy1 = iy + ih;

  int m_isearchfirstx = rect().TopLeft().X();
  int m_isearchfirsty = rect().TopLeft().Y();

  int nx0 = 0;
  int ny0 = 0;
  int nx = 0;
  int ny = 0;
  int nservice = 0;
  cv::Vec3b abyte, bytenext;

  int nScanerID = 1;
  int icurScanerNUM = 0;
  bool boverflow = false;

  ncurscan = -1;
  nscansize = 0;

  ncursearchseek = -1;
  nsearchseeksize = 0;

  int ishowfont = 0;
  int mapservice = 0;
  int mapanalysis = 0;
  int mapedge = 0;

  int iminx = 9999;
  int iminy = 9999;
  int imaxx = 0;
  int imaxy = 0;

  int iborw = 0;

  m_icurobj = 0;
  m_iobjnum = 0;
  m_totalarea = 0;
  m_debug_component_count = 0;
  m_debug_accepted_count = 0;
  m_debug_rejected_count = 0;
  m_debug_max_component_area = 0;
  m_debug_max_component_w = 0;
  m_debug_max_component_h = 0;

  m_scanid.clear();
  m_vborw.clear();
  m_vrow.clear();
  m_vobjnum.clear();
  m_rectresults.clear();
  m_keypoint.clear();
  m_fitwh.clear();
  ;
  MAPCLEAR();

  CLEAR_SEARCHSEEK();
  PUSH_SEARCHSEEK(m_isearchfirstx, m_isearchfirsty);

  for (int icurSeekNum = 0; icurSeekNum < nsearchseeksize;) {
  FORBEGIN:
    mapservice = MAP_service(m_objlistcollectorA[icurSeekNum].X(),
                             m_objlistcollectorA[icurSeekNum].Y());

    if (mapservice > 0) {
      icurSeekNum++;

      if (icurSeekNum >= nsearchseeksize)
        break;
      else
        goto FORBEGIN;
    }
    CLEAR_SCANOR();
    PUSH_SCANOR(m_objlistcollectorA[icurSeekNum].X(),
                m_objlistcollectorA[icurSeekNum].Y());
    SetMAP_service(m_objlistcollectorA[icurSeekNum].X(),
                   m_objlistcollectorA[icurSeekNum].Y(), nScanerID);
    icurScanerNUM = 0;

  CURSCANERBEGIN:
    while (icurScanerNUM != nscansize) {
      nx0 = m_objlistscanorA[icurScanerNUM].X();
      ny0 = m_objlistscanorA[icurScanerNUM].Y();
      abyte = image.pixel(nx0, ny0);

      mapanalysis = MAP_analysis(nx0, ny0);

      if (mapanalysis == FindObject::ANLAYSIS_OVER)
        goto NEXTFINDSTEP;
      for (int i = 0; i < m_idistance; i++) {
        nx = nx0 + m_SearchPointGroup[i].X();
        ny = ny0 + m_SearchPointGroup[i].Y();
        if (nx <= ix || ny <= iy || nx >= ix1 || ny >= iy1)
          continue;
        mapanalysis = MAP_analysis(nx, ny);
        mapservice = MAP_service(nx, ny);
        mapedge = MAP_edge(nx, ny);
        if (mapanalysis == ANLAYSIS_OVER || mapservice != 0 ||
            (mapedge == mapservice && 0 != mapedge))
          continue;
        bytenext = image.pixel(nx, ny);

        if (abs(abyte[0] - bytenext[0]) < m_ihgap &&
            abs(abyte[1] - bytenext[1]) < m_isgap &&
            abs(abyte[2] - bytenext[2]) < m_iogap) {
          PUSH_SCANOR(nx, ny);
          SetMAP_service(nx, ny, nScanerID);
          iborw = (abyte[0] == 0) ? 0 : 1;
          SetMAP_pixel(nx, ny, iborw);
          if (iminx > nx)
            iminx = nx;
          if (iminy > ny)
            iminy = ny;
          if (imaxx < nx)
            imaxx = nx;
          if (imaxy < ny)
            imaxy = ny;
        } else {
          PUSH_SEARCHSEEK(nx, ny);
          SetMAP_edge(nx, ny, nScanerID);
        }
      }
      SetMAP_analysis(nx0, ny0, ANLAYSIS_OVER);
    NEXTFINDSTEP:
      icurScanerNUM++;
    }
  CURSCANEREND:
    if (nscansize > 0)
      m_debug_component_count++;
    int iobjw = imaxx - iminx;
    int iobjh = imaxy - iminy;
    ObserveDebugComponent(nscansize, iobjw, iobjh);
    if ((m_ifilterNedge > 0 &&
         (iminx > m_ifilterNedge && imaxx < iw - m_ifilterNedge &&
          iminy > m_ifilterNedge && imaxy < ih - m_ifilterNedge)) ||
        m_ifilterNedge == 0) {
      if (nscansize > m_iminarea && nscansize < m_imaxarea &&
          iobjw < m_imaxobjw && iobjw >= m_iminobjw && iobjh < m_imaxobjh &&
          iobjh >= m_iminobjh) {
        m_vrow.push_back(nscansize);

        abyte = image.pixel(m_objlistscanorA[0].X(), m_objlistscanorA[0].Y());

        m_vborw.push_back(abyte);

        boverflow = false;
        {
          switch (m_iborw) {
          case 3: {
            gp_Rectangle arectresult(gp_Pnt(iminx, iminy, 0),
                                     gp_Pnt(imaxx, imaxy, 0));
            gp_Pnt apoint(m_objlistscanorA[0].X(), m_objlistscanorA[0].Y(), 0);
            m_keypoint.addpoint(apoint);

            m_rectresults.addrect(arectresult);
            m_scanid.push_back(nScanerID);
            m_iobjnum++;
          }
            ishowfont++;
            m_icurobj++;

            break;
          case 13: {
            {
              if (m_vborw[m_icurobj][0] < 255) {
                for (int ir = 0; ir < nscansize; ir++) {
                  image.setPixel(m_objlistscanorA[ir].X(),
                                 m_objlistscanorA[ir].Y(),
                                 cv::Vec3b(255, 0, 0));
                }
              }
              if (m_vborw[m_icurobj][0] > 0) {
                for (int ir = 0; ir < nscansize; ir++) {
                  image.setPixel(m_objlistscanorA[ir].X(),
                                 m_objlistscanorA[ir].Y(),
                                 cv::Vec3b(0, 0, 255));
                }
              }
              ishowfont++;
              m_icurobj++;
            }
          } break;
          }
        }
      } else {
        switch (m_iborw) {
        case 21: {
          for (int ir = 0; ir < nscansize; ir++) {
            image.setPixel(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y(),
                           cv::Vec3b(0, 0, 0));
          }
        } break;
        case 22: {
          for (int ir = 0; ir < nscansize; ir++) {
            image.setPixel(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y(),
                           cv::Vec3b(255, 255, 255));
          }
        } break;
        case 23: {
          for (int ir = 0; ir < nscansize; ir++) {
            image.setPixel(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y(),
                           cv::Vec3b(255, 0, 0));
          }
        } break;
        }
      }
    } else {
      switch (m_iborw) {
      case 21: {
        for (int ir = 0; ir < nscansize; ir++) {
          image.setPixel(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y(),
                         cv::Vec3b(0, 0, 0));
        }
      } break;
      case 22: {
        for (int ir = 0; ir < nscansize; ir++) {
          image.setPixel(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y(),
                         cv::Vec3b(255, 255, 255));
        }
      } break;
      case 23: {
        for (int ir = 0; ir < nscansize; ir++) {
          image.setPixel(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y(),
                         cv::Vec3b(255, 0, 0));
        }
      } break;
      }
    }
    iminx = 9999;
    iminy = 9999;
    imaxx = 0;
    imaxy = 0;

    nScanerID++;
    icurSeekNum++;
  FOREND:;
  }

  FinalizeRegionGrowthDebugCounters();
  CLEAR_SCANOR();
  CLEAR_SEARCHSEEK();
}

void FindObject::MeasureXFast(Image &image) {
  m_pgetimage = &image;
  m_measurements.clear();
  if (image.getmat().empty())
    return;
  if (!RefreshAlgorithmRuntimeResources(image.getWidth(), image.getHeight()))
    return;
  if (image.getWidth() < rect().TopLeft().X() + rect().Width() ||
      image.getHeight() < rect().TopLeft().Y() + rect().Height())
    return;
  int iw = rect().Width();
  int ih = rect().Height();
  int ix = rect().TopLeft().X();
  int iy = rect().TopLeft().Y();
  int ix1 = ix + iw;
  int iy1 = iy + ih;

  int m_isearchfirstx = rect().TopLeft().X();
  int m_isearchfirsty = rect().TopLeft().Y();

  int nx0 = 0;
  int ny0 = 0;
  int nx = 0;
  int ny = 0;
  int nservice = 0;
  cv::Vec3b abyte, bytenext;

  int nScanerID = 1;
  int icurScanerNUM = 0;
  bool boverflow = false;

  ncurscan = -1;
  nscansize = 0;

  ncursearchseek = -1;
  nsearchseeksize = 0;

  int ishowfont = 0;
  int mapservice = 0;
  int mapanalysis = 0;
  int mapedge = 0;

  int iminx = 9999;
  int iminy = 9999;
  int imaxx = 0;
  int imaxy = 0;

  int iborw = 0;

  m_icurobj = 0;
  m_iobjnum = 0;
  m_totalarea = 0;
  m_debug_component_count = 0;
  m_debug_accepted_count = 0;
  m_debug_rejected_count = 0;
  m_debug_max_component_area = 0;
  m_debug_max_component_w = 0;
  m_debug_max_component_h = 0;

  m_scanid.clear();
  m_vborw.clear();
  m_vrow.clear();
  m_vobjnum.clear();
  m_rectresults.clear();
  m_keypoint.clear();
  m_fitwh.clear();
  ;
  MAPCLEAR();

  CLEAR_SEARCHSEEK();
  PUSH_SEARCHSEEK(m_isearchfirstx, m_isearchfirsty);

  for (int icurSeekNum = 0; icurSeekNum < nsearchseeksize;) {
  FORBEGIN:
    const int seekx = m_objlistcollectorA[icurSeekNum].X();
    const int seeky = m_objlistcollectorA[icurSeekNum].Y();
    mapservice = GetServiceValue(MAP(seekx, seeky));

    if (mapservice > 0) {
      icurSeekNum++;

      if (icurSeekNum >= nsearchseeksize)
        break;
      else
        goto FORBEGIN;
    }
    CLEAR_SCANOR();
    PUSH_SCANOR(seekx, seeky);
    SetMAP_service(seekx, seeky, nScanerID);
    icurScanerNUM = 0;

  CURSCANERBEGIN:
    while (icurScanerNUM != nscansize) {
      nx0 = m_objlistscanorA[icurScanerNUM].X();
      ny0 = m_objlistscanorA[icurScanerNUM].Y();
      abyte = image.pixel(nx0, ny0);

      mapanalysis = MAP_analysis(nx0, ny0);

      if (mapanalysis == FindObject::ANLAYSIS_OVER)
        goto NEXTFINDSTEP;
      for (int i = 0; i < m_idistance; i++) {
        nx = nx0 + m_SearchPointGroup[i].X();
        ny = ny0 + m_SearchPointGroup[i].Y();
        if (nx <= ix || ny <= iy || nx >= ix1 || ny >= iy1)
          continue;
        const MapState map_state = DecodeMapState(MAP(nx, ny));
        mapanalysis = map_state.analysis;
        mapservice = map_state.service;
        mapedge = map_state.edge;
        if (mapanalysis == ANLAYSIS_OVER || mapservice != 0 ||
            (mapedge == mapservice && 0 != mapedge))
          continue;
        bytenext = image.pixel(nx, ny);

        if (abs(abyte[0] - bytenext[0]) < m_ihgap &&
            abs(abyte[1] - bytenext[1]) < m_isgap &&
            abs(abyte[2] - bytenext[2]) < m_iogap) {
          PUSH_SCANOR(nx, ny);
          iborw = (abyte[0] == 0) ? 0 : 1;
          SetMAP_service_pixel(nx, ny, nScanerID, iborw);
          if (iminx > nx)
            iminx = nx;
          if (iminy > ny)
            iminy = ny;
          if (imaxx < nx)
            imaxx = nx;
          if (imaxy < ny)
            imaxy = ny;
        } else {
          PUSH_SEARCHSEEK(nx, ny);
          SetMAP_edge(nx, ny, nScanerID);
        }
      }
      SetMAP_analysis(nx0, ny0, ANLAYSIS_OVER);
    NEXTFINDSTEP:
      icurScanerNUM++;
    }
  CURSCANEREND:
    if (nscansize > 0)
      m_debug_component_count++;
    int iobjw = imaxx - iminx;
    int iobjh = imaxy - iminy;
    ObserveDebugComponent(nscansize, iobjw, iobjh);
    if ((m_ifilterNedge > 0 &&
         (iminx > m_ifilterNedge && imaxx < iw - m_ifilterNedge &&
          iminy > m_ifilterNedge && imaxy < ih - m_ifilterNedge)) ||
        m_ifilterNedge == 0) {
      if (nscansize > m_iminarea && nscansize < m_imaxarea &&
          iobjw < m_imaxobjw && iobjw >= m_iminobjw && iobjh < m_imaxobjh &&
          iobjh >= m_iminobjh) {
        m_vrow.push_back(nscansize);

        abyte = image.pixel(m_objlistscanorA[0].X(), m_objlistscanorA[0].Y());

        m_vborw.push_back(abyte);

        boverflow = false;
        {
          switch (m_iborw) {
          case 3: {
            gp_Rectangle arectresult(gp_Pnt(iminx, iminy, 0),
                                     gp_Pnt(imaxx, imaxy, 0));
            gp_Pnt apoint(m_objlistscanorA[0].X(), m_objlistscanorA[0].Y(), 0);
            m_keypoint.addpoint(apoint);

            m_rectresults.addrect(arectresult);
            m_scanid.push_back(nScanerID);
            m_iobjnum++;
          }
            ishowfont++;
            m_icurobj++;

            break;
          case 13: {
            {
              if (m_vborw[m_icurobj][0] < 255) {
                for (int ir = 0; ir < nscansize; ir++) {
                  image.setPixel(m_objlistscanorA[ir].X(),
                                 m_objlistscanorA[ir].Y(),
                                 cv::Vec3b(255, 0, 0));
                }
              }
              if (m_vborw[m_icurobj][0] > 0) {
                for (int ir = 0; ir < nscansize; ir++) {
                  image.setPixel(m_objlistscanorA[ir].X(),
                                 m_objlistscanorA[ir].Y(),
                                 cv::Vec3b(0, 0, 255));
                }
              }
              ishowfont++;
              m_icurobj++;
            }
          } break;
          }
        }
      } else {
        switch (m_iborw) {
        case 21: {
          for (int ir = 0; ir < nscansize; ir++) {
            image.setPixel(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y(),
                           cv::Vec3b(0, 0, 0));
          }
        } break;
        case 22: {
          for (int ir = 0; ir < nscansize; ir++) {
            image.setPixel(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y(),
                           cv::Vec3b(255, 255, 255));
          }
        } break;
        case 23: {
          for (int ir = 0; ir < nscansize; ir++) {
            image.setPixel(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y(),
                           cv::Vec3b(255, 0, 0));
          }
        } break;
        }
      }
    } else {
      switch (m_iborw) {
      case 21: {
        for (int ir = 0; ir < nscansize; ir++) {
          image.setPixel(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y(),
                         cv::Vec3b(0, 0, 0));
        }
      } break;
      case 22: {
        for (int ir = 0; ir < nscansize; ir++) {
          image.setPixel(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y(),
                         cv::Vec3b(255, 255, 255));
        }
      } break;
      case 23: {
        for (int ir = 0; ir < nscansize; ir++) {
          image.setPixel(m_objlistscanorA[ir].X(), m_objlistscanorA[ir].Y(),
                         cv::Vec3b(255, 0, 0));
        }
      } break;
      }
    }
    iminx = 9999;
    iminy = 9999;
    imaxx = 0;
    imaxy = 0;

    nScanerID++;
    icurSeekNum++;
  FOREND:;
  }

  FinalizeRegionGrowthDebugCounters();
  CLEAR_SCANOR();
  CLEAR_SEARCHSEEK();
}

void FindObject::MeasureXConnectedComponents(Image &image) {
  MeasureConnectedComponents(image);
}

void FindObject::measurex(void *pimage) {
  Image *pgetimage = (Image *)pimage;
  if (pgetimage == nullptr)
    return;

  MeasureX(*pgetimage);
}

void FindObject::measurexfast(void *pimage) {
  Image *pgetimage = (Image *)pimage;
  if (pgetimage == nullptr)
    return;

  MeasureXFast(*pgetimage);
}

void FindObject::measurexcc(void *pimage) {
  Image *pgetimage = (Image *)pimage;
  if (pgetimage == nullptr)
    return;

  MeasureXConnectedComponents(*pgetimage);
}

void FindObject::setminmaxwh(int iminw, int imaxw, int iminh, int imaxh) {
  m_imaxobjw = imaxw;
  m_iminobjw = iminw;
  m_imaxobjh = imaxh;
  m_iminobjh = iminh;
}
void FindObject::setobjectgrid(int iw, int ih, int ixgrid) {
  m_icopyw = iw;
  m_icopyh = ih;
  m_icopywgrid = ixgrid;
}
int FindObject::getobjectgridw() { return m_icopyw; }
int FindObject::getobjectgridh() { return m_icopyh; }

void FindObject::setbackground(int iedge, int ibackgroundmethod) {
  m_background_edge = std::max(1, iedge);
  m_background_method = ibackgroundmethod;
  setbackgroundborderwidth(m_background_edge);
  setbackgroundmethod(m_background_method);
}
void FindObject::resultsrectfilter() { m_rectresults.removecontains_c(); }
void FindObject::objectgrid(void *pimage) {}
void FindObject::objectsort() { m_rectresults.sort(); }
gp_Rectangle FindObject::getgrid(int inum) {
  int igridh = inum / m_icopywgrid;
  int igridw = inum % m_icopywgrid;

  int ibeginx = igridw * m_icopyw;
  int ibeginy = igridh * m_icopyh;

  gp_Rectangle arect(gp_Pnt(ibeginx, ibeginy, 0),
                     gp_Pnt(m_icopyw, m_icopyh, 0));

  return arect;
}
gp_Rectangle FindObject::getgridex(int inum) {
  if (inum < m_rectgrids.size() && inum >= 0)
    return m_rectgrids.getrect(inum);
  else
    return gp_Rectangle(gp_Pnt(0, 0, 0), gp_Pnt(0, 0, 0));
}
void FindObject::setedgeoi(int iw, int ioffset, int iheadtail) {
  switch (m_iborw) {
  case 901: {
    int isize = m_cent_h_bw_points_v.size();
    for (int i = 0; i < isize; i++)
      m_cent_h_bw_points_v[i].setedgeoi(iw, ioffset, iheadtail);
  } break;
  case 902: {
    int isize = m_cent_v_bw_points_v.size();
    for (int i = 0; i < isize; i++)
      m_cent_v_bw_points_v[i].setedgeoi(iw, ioffset, iheadtail);
  } break;
  case 903: {
    int isize = m_cent_h_wb_points_v.size();
    for (int i = 0; i < isize; i++)
      m_cent_h_wb_points_v[i].setedgeoi(iw, ioffset, iheadtail);
  } break;
  case 904: {
    int isize = m_cent_v_wb_points_v.size();
    for (int i = 0; i < isize; i++)
      m_cent_v_wb_points_v[i].setedgeoi(iw, ioffset, iheadtail);
  } break;
  }
}
void FindObject::edgeimage(void *pimage) {
  if (0 == pimage)
    return;
  Image *paimage = (Image *)pimage;
  switch (m_iborw) {
  case 901: {
    int isize = m_cent_h_bw_points_v.size();
    for (int i = 0; i < isize; i++)
      m_cent_h_bw_points_v[i].edgeimage(paimage->getmat(), 0);
  } break;
  case 902: {
    int isize = m_cent_v_bw_points_v.size();
    for (int i = 0; i < isize; i++)
      m_cent_v_bw_points_v[i].edgeimage(paimage->getmat(), 1);
  } break;
  case 903: {
    int isize = m_cent_h_wb_points_v.size();
    for (int i = 0; i < isize; i++)
      m_cent_h_wb_points_v[i].edgeimage(paimage->getmat(), 2);
  } break;
  case 904: {
    int isize = m_cent_v_wb_points_v.size();
    for (int i = 0; i < isize; i++)
      m_cent_v_wb_points_v[i].edgeimage(paimage->getmat(), 3);
  } break;
  }
}
void FindObject::setcolorstyle(int istyle) { m_istyle = istyle; }
void FindObject::drawshapex(double dmovx, double dmovy, double dangle,
                            double dzoomx, double dzoomy) {}

void FindObject::setrelationrectfromresultnum(int inum) {
  m_irelationresultnum = inum;
}
void FindObject::setrelationrectfrom_matchresult(void *pmatch) {}
void FindObject::setrelationxy(int iprex1, int iprey1, int iendx1, int iendy1) {

}
void FindObject::setrelationzoom(double drelationzoomx, double drelationzoomy) {

}
void FindObject::setrelationtorect() {}
void FindObject::SetImageROIthre(int ithre) { m_imagethre = ithre; }
void FindObject::SetImageROIincrease(int increase) {
  m_imagethreincrease = increase;
}
void FindObject::SetImageROIcomparegap(int icomparegap) {
  m_imagecomparegap = icomparegap;
}
void FindObject::SetImageROIfindBorW(int ifindBorW) {
  m_imagefindBorW = ifindBorW;
}
void FindObject::SetImageROIedge_5o7(int i5o7) { m_imageedge_5o7 = i5o7; }
void FindObject::ImageROIthre(void *pimage) {
  Image *pgetimage = (Image *)pimage;
}
void FindObject::ImageROIedge(void *pimage) {}
void FindObject::ImageROIedgeH(void *pimage) {}
void FindObject::shapesetroi(void *pshape) {
  if (pshape == nullptr)
    return;
  Shape::shapesetroi(pshape);
}

void FindObject::PublishDisplayShapes(
    ICxShapeSink& sink,
    const std::string& owner_ref) const
{
  const gp_Rectangle roi = rect();
  const double x = roi.TopLeft().X();
  const double y = roi.TopLeft().Y();
  const double w = roi.Width();
  const double h = roi.Height();
  if (w > 0.0 && h > 0.0)
  {
    auto roi_shape = std::make_unique<RectShape>();
    roi_shape->setRect(x, y, x + w, y + h);
    sink.UpsertShape(owner_ref + ".roi", "FindObject", owner_ref,
                     "setrect", "roi", true, false, std::move(roi_shape));
  }

  for (int i = 0; i < static_cast<int>(m_rectresults.size()); ++i)
  {
    const FindObjectMeasurementSnapshot *measurement = getmeasurement(i);
    const bool measured =
        measurement != nullptr && measurement->status == "measured";
    if (!measured)
      continue;
    if (m_measurement_selection >= 0 && i != m_measurement_selection)
      continue;

    const gp_Rectangle found = m_rectresults.getrect(i);
    const double rx = measurement->bbox_px.x;
    const double ry = measurement->bbox_px.y;
    const double rw = measurement->bbox_px.width;
    const double rh = measurement->bbox_px.height;
    if (rw <= 0.0 || rh <= 0.0)
      continue;
    if (m_conclusion_shape == 1)
    {
      auto result_shape = std::make_unique<RectShape>();
      result_shape->setRect(rx, ry, rx + rw, ry + rh);
      sink.UpsertShape(owner_ref + ".result_rect." + std::to_string(i),
                       "FindObject", owner_ref, "result", "result",
                       false, true, std::move(result_shape));
    }

    if (m_show_boundary && !measurement->outer_boundary.empty())
    {
      auto boundary = std::make_unique<PolylineShape>();
      for (const cv::Point2d &point : measurement->outer_boundary)
        boundary->addPoint(point.x, point.y);
      boundary->close(true);
      sink.UpsertShape(owner_ref + ".boundary." + std::to_string(i),
                       "FindObject", owner_ref, "boundary", "boundary",
                       false, true, std::move(boundary));
    }
    for (int hole_index = 0;
         m_show_boundary &&
         hole_index < static_cast<int>(measurement->hole_boundaries.size());
         ++hole_index)
    {
      auto hole = std::make_unique<PolylineShape>();
      for (const cv::Point2d &point :
           measurement->hole_boundaries[hole_index])
        hole->addPoint(point.x, point.y);
      hole->close(true);
      sink.UpsertShape(
          owner_ref + ".boundary_hole." + std::to_string(i) + "." +
              std::to_string(hole_index),
          "FindObject", owner_ref, "boundary_hole", "boundary_hole",
          false, true, std::move(hole));
    }

    const bool isotropic =
        std::abs(measurement->pixel_size_x -
                 measurement->pixel_size_y) < 1e-12;
    if (m_show_moment_ellipse && measurement->moment_ellipse_valid && isotropic)
    {
      const double scale = measurement->pixel_size_x;
      auto ellipse = std::make_unique<EllipseShape>(
          measurement->centroid_px.x, measurement->centroid_px.y,
          measurement->major_axis_length / scale,
          measurement->minor_axis_length / scale,
          measurement->orientation_deg);
      sink.UpsertShape(owner_ref + ".moment_ellipse." + std::to_string(i),
                       "FindObject", owner_ref, "moment_ellipse", "ellipse",
                       false, true, std::move(ellipse));
    }
    if (m_show_feret && measurement->feret_max > 0.0)
    {
      auto feret = std::make_unique<PolylineShape>();
      feret->addPoint(
          measurement->feret_max_p0.x / measurement->pixel_size_x,
          measurement->feret_max_p0.y / measurement->pixel_size_y);
      feret->addPoint(
          measurement->feret_max_p1.x / measurement->pixel_size_x,
          measurement->feret_max_p1.y / measurement->pixel_size_y);
      feret->close(false);
      sink.UpsertShape(owner_ref + ".feret_max." + std::to_string(i),
                       "FindObject", owner_ref, "feret_max", "line",
                       false, true, std::move(feret));
    }
    if (m_conclusion_shape == 3 && measurement->inscribed_circle_valid && isotropic)
    {
      auto circle = std::make_unique<CircleShape>(
          measurement->inscribed_circle_center_px.x,
          measurement->inscribed_circle_center_px.y,
          measurement->inscribed_circle_radius /
              measurement->pixel_size_x);
      sink.UpsertShape(
          owner_ref + ".inscribed_circle." + std::to_string(i),
          "FindObject", owner_ref, "inscribed_circle", "circle",
          false, true, std::move(circle));
    }
    if (m_conclusion_shape == 4 && measurement->enclosing_circle_valid && isotropic)
    {
      auto circle = std::make_unique<CircleShape>(
          measurement->enclosing_circle_center_px.x,
          measurement->enclosing_circle_center_px.y,
          measurement->enclosing_circle_radius /
              measurement->pixel_size_x);
      sink.UpsertShape(
          owner_ref + ".enclosing_circle." + std::to_string(i),
          "FindObject", owner_ref, "enclosing_circle", "circle",
          false, true, std::move(circle));
    }
  }
}
