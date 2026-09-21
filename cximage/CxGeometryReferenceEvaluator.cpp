#include "CxGeometryReferenceEvaluator.h"
#include "CxPredictiveGeometryGate.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <set>
#include <sstream>
#include <vector>

namespace {
constexpr double kPi = 3.14159265358979323846;

struct CaseMetrics {
  std::string directory;
  std::string review_item;
  std::string case_track;
  std::string geometry_type;
  std::string status = "ASSET_MISSING";
  std::string reason;
  double label_geometry_error_px = 0.0;
  double baseline_iou = 0.0;
  double baseline_geometry_error_px = 0.0;
  std::filesystem::path overlay_path;
  std::filesystem::path evidence_case_path;
};

std::string NodeString(const cv::FileNode &node, const char *key) {
  const cv::FileNode value = node[key];
  return value.empty() ? std::string() : static_cast<std::string>(value);
}

double NodeDouble(const cv::FileNode &node, const char *key,
                  double fallback = 0.0) {
  const cv::FileNode value = node[key];
  return value.empty() ? fallback : static_cast<double>(value);
}

std::vector<cv::Point2f> ReadPoints(const cv::FileNode &node) {
  std::vector<cv::Point2f> points;
  if (!node.isSeq())
    return points;
  for (const cv::FileNode &point : node) {
    if (point.isSeq() && point.size() == 2)
      points.emplace_back(static_cast<float>(point[0]),
                          static_cast<float>(point[1]));
  }
  return points;
}

bool ReadPoint(const cv::FileNode &node, cv::Point2f &point) {
  if (!node.isSeq() || node.size() != 2)
    return false;
  point.x = static_cast<float>(node[0]);
  point.y = static_cast<float>(node[1]);
  return true;
}

std::string EscapeJson(const std::string &value) {
  std::string escaped;
  for (char ch : value) {
    if (ch == '\\' || ch == '"')
      escaped += '\\';
    if (ch == '\n')
      escaped += "\\n";
    else if (ch != '\r')
      escaped += ch;
  }
  return escaped;
}

std::string
StableInternalCaseId(const std::filesystem::path &source_case_path) {
  const std::string key = source_case_path.lexically_normal().generic_string();
  std::uint64_t hash = 1469598103934665603ULL;
  for (unsigned char ch : key) {
    hash ^= ch;
    hash *= 1099511628211ULL;
  }
  std::ostringstream value;
  value << "asset_case_" << std::hex << std::setw(16) << std::setfill('0')
        << hash;
  return value.str();
}

bool WriteEvidenceCasePackage(
    const CxGeometryReferenceEvaluationOptions &options,
    const std::filesystem::path &source_case_path,
    const std::filesystem::path &source_manifest_path,
    const std::filesystem::path &input_path,
    const std::filesystem::path &label_path,
    const std::filesystem::path &facts_path, const std::string &topology,
    const std::string &typed_label_kind, CaseMetrics &metrics) {
  const std::filesystem::path package =
      options.output_dir / "cases" / metrics.directory;
  std::error_code error;
  std::filesystem::create_directories(package, error);
  if (error)
    return false;

  const std::filesystem::path source_copy =
      package / ("source_image" + input_path.extension().string());
  const std::filesystem::path label_copy =
      package / ("typed_label" + label_path.extension().string());
  const std::filesystem::path facts_copy = package / "geometry_facts.json";
  const std::filesystem::path overlay_copy = package / "evidence_overlay.png";
  std::filesystem::copy_file(input_path, source_copy,
                             std::filesystem::copy_options::overwrite_existing,
                             error);
  if (error)
    return false;
  std::filesystem::copy_file(label_path, label_copy,
                             std::filesystem::copy_options::overwrite_existing,
                             error);
  if (error)
    return false;
  std::filesystem::copy_file(facts_path, facts_copy,
                             std::filesystem::copy_options::overwrite_existing,
                             error);
  if (error)
    return false;
  std::filesystem::copy_file(metrics.overlay_path, overlay_copy,
                             std::filesystem::copy_options::overwrite_existing,
                             error);
  if (error)
    return false;

  const std::filesystem::path summary_path = package / "result_summary.json";
  std::ofstream summary(summary_path, std::ios::trunc);
  summary << "{\n"
          << "  \"schema\": \"cxvision.geometry_case_result.v1\",\n"
          << "  \"status\": \"" << EscapeJson(metrics.status) << "\",\n"
          << "  \"review_item\": \"" << EscapeJson(metrics.review_item)
          << "\",\n"
          << "  \"case_track\": \"" << EscapeJson(metrics.case_track) << "\",\n"
          << "  \"geometry_type\": \"" << EscapeJson(metrics.geometry_type)
          << "\",\n"
          << "  \"label_geometry_error_px\": "
          << metrics.label_geometry_error_px << ",\n"
          << "  \"baseline_iou\": " << metrics.baseline_iou << ",\n"
          << "  \"baseline_geometry_error_px\": "
          << metrics.baseline_geometry_error_px << ",\n"
          << "  \"conclusion\": \"CONTROLLED_FIXTURE_READY_FOR_HUMAN_REVIEW\"\n"
          << "}\n";
  summary.close();
  if (!summary)
    return false;

  const std::string run_id = options.output_dir.filename().string();
  const std::filesystem::path manifest_path = package / "case_manifest.json";
  std::ofstream manifest(manifest_path, std::ios::trunc);
  manifest << "{\n"
           << "  \"schema\": \"cxvision.evidence_case.v1\",\n"
           << "  \"run_id\": \"" << EscapeJson(run_id) << "\",\n"
           << "  \"internal_case_id\": \""
           << StableInternalCaseId(source_case_path) << "\",\n"
           << "  \"review_item\": \"" << EscapeJson(metrics.review_item)
           << "\",\n"
           << "  \"case_track\": \"" << EscapeJson(metrics.case_track)
           << "\",\n"
           << "  \"geometry_type\": \"" << EscapeJson(metrics.geometry_type)
           << "\",\n"
           << "  \"topology\": \"" << EscapeJson(topology) << "\",\n"
           << "  \"training_enabled\": 0,\n"
           << "  \"binding_status\": \"PENDING_HUMAN_REVIEW\",\n"
           << "  \"source_image\": \""
           << EscapeJson(source_copy.filename().string()) << "\",\n"
           << "  \"typed_label\": \""
           << EscapeJson(label_copy.filename().string()) << "\",\n"
           << "  \"typed_label_kind\": \"" << EscapeJson(typed_label_kind)
           << "\",\n"
           << "  \"geometry_facts_ref\": \"geometry_facts.json\",\n"
           << "  \"evidence_overlay\": \"evidence_overlay.png\",\n"
           << "  \"result_summary\": \"result_summary.json\",\n"
           << "  \"source_case_path\": \""
           << EscapeJson(source_case_path.string()) << "\",\n"
           << "  \"source_manifest_path\": \""
           << EscapeJson(source_manifest_path.string()) << "\",\n"
           << "  \"required_assets\": [\n"
           << "    \"" << EscapeJson(source_copy.filename().string()) << "\",\n"
           << "    \"" << EscapeJson(label_copy.filename().string()) << "\",\n"
           << "    \"geometry_facts.json\",\n"
           << "    \"evidence_overlay.png\",\n"
           << "    \"result_summary.json\"\n"
           << "  ]\n"
           << "}\n";
  manifest.close();
  if (!manifest)
    return false;

  metrics.evidence_case_path = package;
  return true;
}

bool IsWithinRoot(const std::filesystem::path &root,
                  const std::filesystem::path &path) {
  std::error_code error;
  const std::filesystem::path canonical_root =
      std::filesystem::weakly_canonical(root, error);
  if (error)
    return false;
  const std::filesystem::path canonical_path =
      std::filesystem::weakly_canonical(path, error);
  if (error)
    return false;
  const std::filesystem::path relative =
      std::filesystem::relative(canonical_path, canonical_root, error);
  if (error || relative.empty())
    return !error;
  const auto first = relative.begin();
  return first == relative.end() || *first != "..";
}

cv::Mat ReadBinaryMask(const std::filesystem::path &path) {
  cv::Mat mask = cv::imread(path.string(), cv::IMREAD_GRAYSCALE);
  if (!mask.empty())
    cv::threshold(mask, mask, 0, 255, cv::THRESH_BINARY);
  return mask;
}

double MaskIou(const cv::Mat &left, const cv::Mat &right) {
  if (left.empty() || right.empty() || left.size() != right.size())
    return 0.0;
  cv::Mat intersection;
  cv::Mat combined;
  cv::bitwise_and(left, right, intersection);
  cv::bitwise_or(left, right, combined);
  const int union_count = cv::countNonZero(combined);
  return union_count == 0
             ? 1.0
             : static_cast<double>(cv::countNonZero(intersection)) /
                   union_count;
}

double PointDistance(const cv::Point2f &left, const cv::Point2f &right) {
  return cv::norm(left - right);
}

double PointSegmentDistance(const cv::Point2f &point, const cv::Point2f &start,
                            const cv::Point2f &end) {
  const cv::Point2f segment = end - start;
  const double length_sq = segment.dot(segment);
  if (length_sq <= std::numeric_limits<double>::epsilon())
    return PointDistance(point, start);
  const double parameter = std::clamp(
      static_cast<double>((point - start).dot(segment)) / length_sq, 0.0, 1.0);
  return PointDistance(point, start + segment * static_cast<float>(parameter));
}

double NormalizeAngle180(double angle) {
  while (angle < 0.0)
    angle += 180.0;
  while (angle >= 180.0)
    angle -= 180.0;
  return angle;
}

double AngleError180(double left, double right) {
  const double delta =
      std::abs(NormalizeAngle180(left) - NormalizeAngle180(right));
  return std::min(delta, 180.0 - delta);
}

bool LargestContour(const cv::Mat &mask, std::vector<cv::Point> &contour) {
  std::vector<std::vector<cv::Point>> contours;
  cv::findContours(mask.clone(), contours, cv::RETR_EXTERNAL,
                   cv::CHAIN_APPROX_NONE);
  if (contours.empty())
    return false;
  const auto found =
      std::max_element(contours.begin(), contours.end(),
                       [](const auto &left, const auto &right) {
                         return cv::contourArea(left) < cv::contourArea(right);
                       });
  contour = *found;
  return !contour.empty();
}

bool FitCircleLeastSquares(const std::vector<cv::Point2f> &points,
                           cv::Point2f &center, double &radius) {
  if (points.size() < 3)
    return false;
  cv::Mat design(static_cast<int>(points.size()), 3, CV_64F);
  cv::Mat target(static_cast<int>(points.size()), 1, CV_64F);
  for (int row = 0; row < design.rows; ++row) {
    const cv::Point2f point = points[static_cast<std::size_t>(row)];
    design.at<double>(row, 0) = 2.0 * point.x;
    design.at<double>(row, 1) = 2.0 * point.y;
    design.at<double>(row, 2) = 1.0;
    target.at<double>(row, 0) = point.x * point.x + point.y * point.y;
  }
  cv::Mat solution;
  if (!cv::solve(design, target, solution, cv::DECOMP_SVD))
    return false;
  center.x = static_cast<float>(solution.at<double>(0, 0));
  center.y = static_cast<float>(solution.at<double>(1, 0));
  const double squared =
      solution.at<double>(2, 0) + center.x * center.x + center.y * center.y;
  if (squared <= 0.0)
    return false;
  radius = std::sqrt(squared);
  return true;
}

std::vector<cv::Point2f> NonZeroPoints(const cv::Mat &mask) {
  std::vector<cv::Point> integer_points;
  cv::findNonZero(mask, integer_points);
  std::vector<cv::Point2f> points;
  points.reserve(integer_points.size());
  for (const cv::Point &point : integer_points)
    points.emplace_back(static_cast<float>(point.x),
                        static_cast<float>(point.y));
  return points;
}

double DistanceToMask(const cv::Mat &mask,
                      const std::vector<cv::Point2f> &expected) {
  if (mask.empty() || expected.empty())
    return std::numeric_limits<double>::infinity();
  cv::Mat inverse;
  cv::threshold(mask, inverse, 0, 255, cv::THRESH_BINARY_INV);
  cv::Mat distances;
  cv::distanceTransform(inverse, distances, cv::DIST_L2, 3);
  double total = 0.0;
  int count = 0;
  for (const cv::Point2f &point : expected) {
    const int x = std::clamp(cvRound(point.x), 0, mask.cols - 1);
    const int y = std::clamp(cvRound(point.y), 0, mask.rows - 1);
    total += distances.at<float>(y, x);
    ++count;
  }
  return count == 0 ? std::numeric_limits<double>::infinity() : total / count;
}

std::vector<cv::Point2f>
SampleBezier(const std::vector<cv::Point2f> &control_points) {
  std::vector<cv::Point2f> points;
  if (control_points.size() != 4)
    return points;
  for (int index = 0; index <= 160; ++index) {
    const double t = static_cast<double>(index) / 160.0;
    const double u = 1.0 - t;
    points.push_back(control_points[0] * static_cast<float>(u * u * u) +
                     control_points[1] * static_cast<float>(3.0 * u * u * t) +
                     control_points[2] * static_cast<float>(3.0 * u * t * t) +
                     control_points[3] * static_cast<float>(t * t * t));
  }
  return points;
}

std::vector<cv::Point2f> SampleArc(const cv::Point2f &center, double radius,
                                   double start_angle, double end_angle) {
  std::vector<cv::Point2f> points;
  for (int index = 0; index <= 160; ++index) {
    const double angle =
        (start_angle + (end_angle - start_angle) * index / 160.0) * kPi / 180.0;
    points.emplace_back(
        static_cast<float>(center.x + radius * std::cos(angle)),
        static_cast<float>(center.y + radius * std::sin(angle)));
  }
  return points;
}

double EvaluateGeometry(const std::string &geometry_type, const cv::Mat &mask,
                        const cv::FileNode &facts) {
  if (mask.empty())
    return std::numeric_limits<double>::infinity();

  if (geometry_type == "circle") {
    const cv::FileNode instance = facts["instances"][0];
    cv::Point2f expected_center;
    std::vector<cv::Point> contour;
    if (!ReadPoint(instance["center_xy"], expected_center) ||
        !LargestContour(mask, contour))
      return std::numeric_limits<double>::infinity();
    cv::Point2f center;
    float radius = 0.0f;
    cv::minEnclosingCircle(contour, center, radius);
    return std::max(PointDistance(center, expected_center),
                    std::abs(static_cast<double>(radius) -
                             NodeDouble(instance, "radius_px")));
  }

  if (geometry_type == "ellipse") {
    const cv::FileNode instance = facts["instances"][0];
    cv::Point2f expected_center;
    std::vector<cv::Point> contour;
    if (!ReadPoint(instance["center_xy"], expected_center) ||
        !LargestContour(mask, contour) || contour.size() < 5)
      return std::numeric_limits<double>::infinity();
    const cv::RotatedRect ellipse = cv::fitEllipse(contour);
    double major = ellipse.size.width * 0.5;
    double minor = ellipse.size.height * 0.5;
    double angle = ellipse.angle;
    if (minor > major) {
      std::swap(major, minor);
      angle += 90.0;
    }
    return std::max(
        {PointDistance(ellipse.center, expected_center),
         std::abs(major - NodeDouble(instance, "radius_x_px")),
         std::abs(minor - NodeDouble(instance, "radius_y_px")),
         AngleError180(angle, NodeDouble(instance, "rotation_deg"))});
  }

  if (geometry_type == "rectangle") {
    const cv::FileNode instance = facts["instances"][0];
    cv::Point2f expected_center;
    std::vector<cv::Point> contour;
    if (!ReadPoint(instance["center_xy"], expected_center) ||
        !LargestContour(mask, contour))
      return std::numeric_limits<double>::infinity();
    const cv::RotatedRect rectangle = cv::minAreaRect(contour);
    double width = rectangle.size.width;
    double height = rectangle.size.height;
    double angle = rectangle.angle;
    if (height > width) {
      std::swap(width, height);
      angle += 90.0;
    }
    return std::max(
        {PointDistance(rectangle.center, expected_center),
         std::abs(width - NodeDouble(instance, "width_px")),
         std::abs(height - NodeDouble(instance, "height_px")),
         AngleError180(angle, NodeDouble(instance, "rotation_deg"))});
  }

  if (geometry_type == "polygon") {
    const cv::FileNode instance = facts["instances"][0];
    const std::vector<cv::Point2f> vertices =
        ReadPoints(instance["vertices_xy"]);
    std::vector<cv::Point> contour;
    if (vertices.size() < 3 || !LargestContour(mask, contour))
      return std::numeric_limits<double>::infinity();
    double total = 0.0;
    for (const cv::Point &integer_point : contour) {
      const cv::Point2f point(integer_point);
      double closest = std::numeric_limits<double>::infinity();
      for (std::size_t index = 0; index < vertices.size(); ++index)
        closest = std::min(
            closest,
            PointSegmentDistance(point, vertices[index],
                                 vertices[(index + 1) % vertices.size()]));
      total += closest;
    }
    return contour.empty() ? std::numeric_limits<double>::infinity()
                           : total / contour.size();
  }

  if (geometry_type == "line") {
    const std::vector<cv::Point2f> expected = ReadPoints(facts["endpoints_xy"]);
    const std::vector<cv::Point2f> points = NonZeroPoints(mask);
    if (expected.size() != 2 || points.size() < 2)
      return std::numeric_limits<double>::infinity();
    cv::Vec4f line;
    cv::fitLine(points, line, cv::DIST_L2, 0.0, 0.01, 0.01);
    cv::Point2f direction(line[0], line[1]);
    const cv::Point2f origin(line[2], line[3]);
    double minimum = std::numeric_limits<double>::infinity();
    double maximum = -std::numeric_limits<double>::infinity();
    for (const cv::Point2f &point : points) {
      const double projection = (point - origin).dot(direction);
      minimum = std::min(minimum, projection);
      maximum = std::max(maximum, projection);
    }
    const cv::Point2f first = origin + direction * static_cast<float>(minimum);
    const cv::Point2f last = origin + direction * static_cast<float>(maximum);
    const double direct = std::max(PointDistance(first, expected[0]),
                                   PointDistance(last, expected[1]));
    const double reversed = std::max(PointDistance(first, expected[1]),
                                     PointDistance(last, expected[0]));
    return std::min(direct, reversed);
  }

  if (geometry_type == "arc") {
    cv::Point2f center;
    if (!ReadPoint(facts["center_xy"], center))
      return std::numeric_limits<double>::infinity();
    const double radius = NodeDouble(facts, "radius_px");
    const std::vector<cv::Point2f> expected =
        SampleArc(center, radius, NodeDouble(facts, "start_angle_deg"),
                  NodeDouble(facts, "end_angle_deg"));
    cv::Point2f fitted_center;
    double fitted_radius = 0.0;
    const std::vector<cv::Point2f> points = NonZeroPoints(mask);
    if (!FitCircleLeastSquares(points, fitted_center, fitted_radius))
      return std::numeric_limits<double>::infinity();
    return std::max({DistanceToMask(mask, expected),
                     PointDistance(fitted_center, center),
                     std::abs(fitted_radius - radius)});
  }

  if (geometry_type == "open_curve") {
    const std::vector<cv::Point2f> expected =
        SampleBezier(ReadPoints(facts["control_points_xy"]));
    return DistanceToMask(mask, expected);
  }

  return std::numeric_limits<double>::infinity();
}

bool EvaluateCase(const std::filesystem::path &root,
                  const cv::FileNode &index_case,
                  const CxGeometryReferenceEvaluationOptions &options,
                  CaseMetrics &metrics) {
  metrics.directory = NodeString(index_case, "directory");
  metrics.review_item = NodeString(index_case, "review_item");
  metrics.case_track = NodeString(index_case, "case_track");
  metrics.geometry_type = NodeString(index_case, "geometry_type");
  if (metrics.directory.empty() || metrics.review_item.empty() ||
      metrics.geometry_type.empty()) {
    metrics.reason =
        "index row is missing directory, review_item, or geometry_type";
    return false;
  }

  const std::filesystem::path case_dir = root / metrics.directory;
  const std::filesystem::path manifest_path =
      case_dir / NodeString(index_case, "manifest");
  const std::filesystem::path resolved_manifest =
      std::filesystem::is_regular_file(manifest_path)
          ? manifest_path
          : root / NodeString(index_case, "manifest");
  if (!IsWithinRoot(root, case_dir) || std::filesystem::is_symlink(case_dir) ||
      !std::filesystem::is_regular_file(resolved_manifest)) {
    metrics.reason =
        "case directory or manifest is missing or outside the reference root";
    return false;
  }

  cv::FileStorage manifest(resolved_manifest.string(),
                           cv::FileStorage::READ |
                               cv::FileStorage::FORMAT_JSON);
  if (!manifest.isOpened()) {
    metrics.reason = "case manifest is not valid JSON";
    return false;
  }
  if (NodeString(manifest.root(), "review_item") != metrics.review_item ||
      NodeString(manifest.root(), "case_track") != metrics.case_track ||
      NodeString(manifest.root(), "geometry_type") != metrics.geometry_type) {
    metrics.reason = "case index and manifest semantics do not match";
    return false;
  }
  if (static_cast<int>(manifest["training_enabled"]) != 0) {
    metrics.reason =
        "controlled geometry reference must keep training disabled";
    return false;
  }

  const std::filesystem::path input_path =
      case_dir / NodeString(manifest.root(), "input_image");
  const std::filesystem::path label_path =
      case_dir / NodeString(manifest.root(), "typed_label");
  const std::filesystem::path facts_path =
      case_dir / NodeString(manifest.root(), "geometry_facts_ref");
  if (!IsWithinRoot(root, input_path) || !IsWithinRoot(root, label_path) ||
      !IsWithinRoot(root, facts_path) ||
      !std::filesystem::is_regular_file(input_path) ||
      !std::filesystem::is_regular_file(label_path) ||
      !std::filesystem::is_regular_file(facts_path)) {
    metrics.reason =
        "required input, typed label, or geometry facts asset is missing";
    return false;
  }

  const cv::Mat input = cv::imread(input_path.string(), cv::IMREAD_COLOR);
  const cv::Mat label = ReadBinaryMask(label_path);
  cv::FileStorage facts(facts_path.string(),
                        cv::FileStorage::READ | cv::FileStorage::FORMAT_JSON);
  if (input.empty() || label.empty() || !facts.isOpened() ||
      input.size() != label.size() ||
      input.cols != static_cast<int>(manifest["image_width"]) ||
      input.rows != static_cast<int>(manifest["image_height"])) {
    metrics.reason =
        "image, typed label, facts, or declared dimensions are invalid";
    return false;
  }

  cv::Mat gray;
  cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);
  cv::Mat baseline;
  cv::threshold(gray, baseline, options.threshold, 255, cv::THRESH_BINARY);
  metrics.label_geometry_error_px =
      EvaluateGeometry(metrics.geometry_type, label, facts.root());
  metrics.baseline_iou = MaskIou(label, baseline);
  metrics.baseline_geometry_error_px =
      EvaluateGeometry(metrics.geometry_type, baseline, facts.root());

  cv::Mat overlay = input.clone();
  std::vector<std::vector<cv::Point>> label_contours;
  std::vector<std::vector<cv::Point>> baseline_contours;
  cv::findContours(label.clone(), label_contours, cv::RETR_LIST,
                   cv::CHAIN_APPROX_SIMPLE);
  cv::findContours(baseline.clone(), baseline_contours, cv::RETR_LIST,
                   cv::CHAIN_APPROX_SIMPLE);
  cv::drawContours(overlay, label_contours, -1, cv::Scalar(0, 255, 0), 2);
  cv::drawContours(overlay, baseline_contours, -1, cv::Scalar(255, 0, 255), 1);
  std::filesystem::create_directories(options.output_dir / "overlays");
  metrics.overlay_path =
      options.output_dir / "overlays" / (metrics.directory + ".png");
  const bool overlay_ok = cv::imwrite(metrics.overlay_path.string(), overlay);

  const bool open_boundary = metrics.case_track == "O1";
  const double geometry_limit = metrics.geometry_type == "line" ? 5.0 : 3.0;
  const double iou_limit = open_boundary ? 0.30 : 0.95;
  bool pass = std::isfinite(metrics.label_geometry_error_px) &&
              std::isfinite(metrics.baseline_geometry_error_px) &&
              metrics.label_geometry_error_px <= geometry_limit &&
              metrics.baseline_geometry_error_px <= geometry_limit &&
              metrics.baseline_iou >= iou_limit && overlay_ok;
  metrics.status = pass ? "PASS" : "FAIL";
  if (pass && !WriteEvidenceCasePackage(
                  options, case_dir, resolved_manifest, input_path, label_path,
                  facts_path, NodeString(manifest.root(), "topology"),
                  NodeString(manifest.root(), "typed_label_kind"), metrics)) {
    pass = false;
    metrics.status = "ASSET_EXPORT_FAIL";
  }
  metrics.reason =
      pass ? "typed label and threshold baseline satisfy the geometry contract"
           : (metrics.status == "ASSET_EXPORT_FAIL"
                  ? "validated geometry could not be exported as a "
                    "self-contained Evidence case"
                  : "typed label or threshold baseline violates geometry "
                    "error, IoU, or overlay requirements");
  return pass;
}

bool WriteReports(const CxGeometryReferenceEvaluationOptions &options,
                  const std::filesystem::path &root,
                  const std::vector<CaseMetrics> &cases,
                  CxGeometryReferenceEvaluationResult &result) {
  result.report_json =
      options.output_dir / "geometry_reference_evaluation.json";
  result.report_markdown =
      options.output_dir / "geometry_reference_evaluation.md";
  std::ofstream json(result.report_json, std::ios::trunc);
  json << "{\n"
       << "  \"schema\": \"cxvision.geometry_reference_evaluation.v1\",\n"
       << "  \"root\": \"" << EscapeJson(root.string()) << "\",\n"
       << "  \"threshold\": " << options.threshold << ",\n"
       << "  \"discovered_cases\": " << result.discovered_cases << ",\n"
       << "  \"accepted_cases\": " << result.accepted_cases << ",\n"
       << "  \"rejected_cases\": " << result.rejected_cases << ",\n"
       << "  \"conclusion\": \"" << EscapeJson(result.status) << "\",\n"
       << "  \"cases\": [\n";
  for (std::size_t index = 0; index < cases.size(); ++index) {
    const CaseMetrics &item = cases[index];
    json << "    {\n"
         << "      \"review_item\": \"" << EscapeJson(item.review_item)
         << "\",\n"
         << "      \"directory\": \"" << EscapeJson(item.directory) << "\",\n"
         << "      \"case_track\": \"" << EscapeJson(item.case_track) << "\",\n"
         << "      \"geometry_type\": \"" << EscapeJson(item.geometry_type)
         << "\",\n"
         << "      \"status\": \"" << EscapeJson(item.status) << "\",\n"
         << "      \"label_geometry_error_px\": "
         << item.label_geometry_error_px << ",\n"
         << "      \"baseline_iou\": " << item.baseline_iou << ",\n"
         << "      \"baseline_geometry_error_px\": "
         << item.baseline_geometry_error_px << ",\n"
         << "      \"overlay\": \"" << EscapeJson(item.overlay_path.string())
         << "\",\n"
         << "      \"evidence_case_path\": \""
         << EscapeJson(item.evidence_case_path.string()) << "\",\n"
         << "      \"reason\": \"" << EscapeJson(item.reason) << "\"\n"
         << "    }" << (index + 1 == cases.size() ? "\n" : ",\n");
  }
  json << "  ]\n}\n";

  std::ofstream markdown(result.report_markdown, std::ios::trunc);
  markdown << "# Geometry Reference Evaluation\n\n"
           << "Conclusion: `" << result.status << "`\n\n"
           << "| Review item | Track | Geometry | Label error px | Baseline "
              "IoU | Baseline error px | Status |\n"
           << "|---|---:|---|---:|---:|---:|---|\n";
  for (const CaseMetrics &item : cases) {
    markdown << "| " << item.review_item << " | " << item.case_track << " | "
             << item.geometry_type << " | " << std::fixed
             << std::setprecision(3) << item.label_geometry_error_px << " | "
             << item.baseline_iou << " | " << item.baseline_geometry_error_px
             << " | " << item.status << " |\n";
  }
  return json.good() && markdown.good();
}

bool RegisterEvidenceCaseRoot(const std::filesystem::path &output_directory) {
  std::error_code error;
  std::filesystem::path output =
      std::filesystem::weakly_canonical(output_directory, error);
  if (error)
    output =
        std::filesystem::absolute(output_directory, error).lexically_normal();
  if (error)
    return false;

  std::filesystem::path run_root;
  for (std::filesystem::path cursor = output; !cursor.empty();
       cursor = cursor.parent_path()) {
    if (cursor.filename() == "cxscript_runs") {
      run_root = cursor;
      break;
    }
    const std::filesystem::path parent = cursor.parent_path();
    if (parent == cursor)
      break;
  }
  if (run_root.empty() || output.parent_path() == run_root)
    return false;

  const std::filesystem::path category_root = output.parent_path();
  const std::filesystem::path relative_category =
      std::filesystem::relative(category_root, run_root, error);
  if (error || relative_category.empty() || *relative_category.begin() == "..")
    return false;

  const std::filesystem::path registry_path =
      run_root / "_shared" / "evidence_case_roots.json";
  std::set<std::string> roots;
  if (std::filesystem::is_regular_file(registry_path)) {
    cv::FileStorage existing;
    bool existing_opened = false;
    try {
      existing_opened =
          existing.open(registry_path.string(),
                        cv::FileStorage::READ | cv::FileStorage::FORMAT_JSON);
    } catch (const cv::Exception &) {
      return false;
    }
    if (existing_opened && existing["roots"].isSeq()) {
      for (const cv::FileNode &root : existing["roots"])
        roots.insert(static_cast<std::string>(root));
    }
  }
  roots.insert(relative_category.generic_string());

  std::filesystem::create_directories(registry_path.parent_path(), error);
  if (error)
    return false;
  std::ofstream registry(registry_path, std::ios::trunc);
  registry << "{\n"
           << "  \"schema\": \"cxvision.evidence_case_roots.v1\",\n"
           << "  \"roots\": [\n";
  std::size_t index = 0;
  for (const std::string &root : roots) {
    registry << "    \"" << EscapeJson(root) << "\""
             << (++index == roots.size() ? "\n" : ",\n");
  }
  registry << "  ]\n}\n";
  return registry.good();
}
}

bool RunCxGeometryReferenceEvaluation(
    const CxGeometryReferenceEvaluationOptions &options,
    CxGeometryReferenceEvaluationResult &result, std::string &reason) {
  result = CxGeometryReferenceEvaluationResult{};
  if (!std::filesystem::is_regular_file(options.index_path) ||
      options.output_dir.empty()) {
    reason = "geometry reference index and output directory are required";
    result.status = "ASSET_PREFLIGHT_FAIL";
    result.reason = reason;
    return false;
  }
  cv::FileStorage index(options.index_path.string(),
                        cv::FileStorage::READ | cv::FileStorage::FORMAT_JSON);
  if (!index.isOpened() || !index["cases"].isSeq()) {
    reason = "geometry reference index is not valid JSON or has no cases";
    result.status = "ASSET_PREFLIGHT_FAIL";
    result.reason = reason;
    return false;
  }

  std::filesystem::create_directories(options.output_dir);
  const std::filesystem::path root = options.index_path.parent_path();
  std::set<std::string> directories;
  std::set<std::string> review_items;
  std::vector<CaseMetrics> cases;
  for (const cv::FileNode &index_case : index["cases"]) {
    CaseMetrics metrics;
    ++result.discovered_cases;
    const bool unique =
        directories.insert(NodeString(index_case, "directory")).second &&
        review_items.insert(NodeString(index_case, "review_item")).second;
    const bool accepted =
        unique && EvaluateCase(root, index_case, options, metrics);
    if (accepted)
      ++result.accepted_cases;
    else {
      ++result.rejected_cases;
      if (!unique) {
        metrics.status = "DUPLICATE_CASE";
        metrics.reason = "duplicate directory or review item";
      }
    }
    cases.push_back(metrics);
  }

  result.complete = result.discovered_cases > 0 && result.rejected_cases == 0;
  result.status = result.complete ? "GEOMETRY_REFERENCE_EVALUATOR_PASS"
                                  : "GEOMETRY_REFERENCE_EVALUATOR_FAIL";
  result.reason = result.complete
                      ? "all controlled geometry cases passed asset, typed "
                        "label, and baseline checks"
                      : "one or more controlled geometry cases were rejected";
  if (!WriteReports(options, root, cases, result)) {
    reason = "failed to write geometry reference evaluation reports";
    result.complete = false;
    result.status = "GEOMETRY_REFERENCE_EVALUATOR_FAIL";
    result.reason = reason;
    return false;
  }
  if (result.complete && !RegisterEvidenceCaseRoot(options.output_dir)) {
    reason = "failed to register the Evidence case category under RUN_ROOT";
    result.complete = false;
    result.status = "GEOMETRY_REFERENCE_EVALUATOR_FAIL";
    result.reason = reason;
    return false;
  }
  reason.clear();
  return result.complete;
}

namespace {
struct AugOperation {
  std::string type;
  int kernel = 0;
  int width_px = 0;
  int height_px = 0;
  int count = 1;
  int jagged_px = 0;
  double sigma = 0.0;
  double angle_deg = 0.0;
  double offset_y_px = 0.0;
  double scale = 1.0;
  double offset = 0.0;
};

struct AugVariant {
  std::string id;
  std::string review_suffix;
  std::string split;
  int seed = 0;
  std::vector<AugOperation> operations;
};

struct AugSource {
  std::string directory;
  std::string review_item;
  std::string source_split;
  std::string case_track;
  std::string geometry_type;
  std::string topology;
  std::string label_kind;
  std::filesystem::path image;
  std::filesystem::path label;
  std::filesystem::path facts;
  std::filesystem::path anchor_policy;
  std::filesystem::path anchor_candidates;
};

struct AugRow {
  std::string review_item;
  std::string split;
  std::string geometry_type;
  std::string variant_id;
  std::string status;
  std::string reason;
  std::string size_bucket;
  std::string angle_bucket;
  std::string difficulty_bucket;
  std::string degradation_bucket;
  std::string mask_fit_status;
  bool identifiable = false;
  double visible_ratio = 0.0;
  std::filesystem::path case_path;
};

std::string AugFileHash(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  if (!input)
    return {};
  std::uint64_t hash = 1469598103934665603ULL;
  char buffer[32768];
  while (input) {
    input.read(buffer, sizeof(buffer));
    for (std::streamsize index = 0; index < input.gcount(); ++index) {
      hash ^= static_cast<unsigned char>(buffer[index]);
      hash *= 1099511628211ULL;
    }
  }
  std::ostringstream value;
  value << std::hex << std::setw(16) << std::setfill('0') << hash;
  return value.str();
}

cv::Matx33d AugMatrix(const cv::Mat &affine) {
  return cv::Matx33d(affine.at<double>(0, 0), affine.at<double>(0, 1),
                     affine.at<double>(0, 2), affine.at<double>(1, 0),
                     affine.at<double>(1, 1), affine.at<double>(1, 2), 0.0, 0.0,
                     1.0);
}

cv::Point2f AugPoint(const cv::Matx33d &matrix, const cv::Point2f &point) {
  return {static_cast<float>(matrix(0, 0) * point.x + matrix(0, 1) * point.y +
                             matrix(0, 2)),
          static_cast<float>(matrix(1, 0) * point.x + matrix(1, 1) * point.y +
                             matrix(1, 2))};
}

void AugWritePoint(cv::FileStorage &output, const char *key,
                   const cv::Point2f &point) {
  output << key << "[" << point.x << point.y << "]";
}

void AugWritePoints(cv::FileStorage &output, const char *key,
                    const cv::FileNode &input, const cv::Matx33d &matrix) {
  output << key << "[";
  for (const cv::Point2f &point : ReadPoints(input)) {
    const cv::Point2f transformed = AugPoint(matrix, point);
    output << "[" << transformed.x << transformed.y << "]";
  }
  output << "]";
}

void AugWriteMatrix(cv::FileStorage &output, const char *key,
                    const cv::Matx33d &matrix) {
  output << key << "[";
  for (int row = 0; row < 3; ++row)
    output << "[" << matrix(row, 0) << matrix(row, 1) << matrix(row, 2) << "]";
  output << "]";
}

bool AugSafeComponent(const std::string &value) {
  return !value.empty() && value != "." && value != ".." &&
         value.find('/') == std::string::npos &&
         value.find('\\') == std::string::npos &&
         value.find(':') == std::string::npos;
}

bool ReadAugPlan(const std::filesystem::path &path,
                 std::vector<AugVariant> &variants, std::string &reason) {
  cv::FileStorage plan;
  try {
    plan.open(path.string(),
              cv::FileStorage::READ | cv::FileStorage::FORMAT_JSON);
  } catch (const cv::Exception &error) {
    reason = std::string("augmentation plan parse failed: ") + error.what();
    return false;
  }
  if (!plan.isOpened() ||
      NodeString(plan.root(), "schema") !=
          "cxvision.geometry_augmentation_plan.v1" ||
      !plan["variants"].isSeq()) {
    reason = "augmentation plan schema or variants are invalid";
    return false;
  }

  std::set<std::string> ids;
  for (const cv::FileNode &node : plan["variants"]) {
    AugVariant variant;
    variant.id = NodeString(node, "id");
    variant.review_suffix = NodeString(node, "review_suffix");
    variant.split = NodeString(node, "split");
    variant.seed = static_cast<int>(NodeDouble(node, "seed"));
    if (!AugSafeComponent(variant.id) || variant.review_suffix.empty() ||
        (variant.split != "train" && variant.split != "validation" &&
         variant.split != "holdout") ||
        !ids.insert(variant.id).second || !node["operations"].isSeq()) {
      reason = "augmentation variant metadata is invalid";
      return false;
    }
    for (const cv::FileNode &item : node["operations"]) {
      AugOperation operation;
      operation.type = NodeString(item, "type");
      operation.kernel = static_cast<int>(NodeDouble(item, "kernel"));
      operation.width_px = static_cast<int>(NodeDouble(item, "width_px"));
      operation.height_px = static_cast<int>(NodeDouble(item, "height_px"));
      operation.count = static_cast<int>(NodeDouble(item, "count", 1.0));
      operation.jagged_px = static_cast<int>(NodeDouble(item, "jagged_px"));
      operation.sigma = NodeDouble(item, "sigma");
      operation.angle_deg = NodeDouble(item, "angle_deg");
      operation.offset_y_px = NodeDouble(item, "offset_y_px");
      operation.scale = NodeDouble(item, "scale", 1.0);
      operation.offset = NodeDouble(item, "offset");
      const bool valid =
          (operation.type == "gaussian_blur" && operation.kernel >= 3 &&
           operation.kernel % 2 == 1 && operation.sigma > 0.0) ||
          (operation.type == "sensor_noise" && operation.sigma > 0.0) ||
          (operation.type == "rotate" && std::abs(operation.angle_deg) > 0.0 &&
           std::abs(operation.angle_deg) <= 45.0) ||
          (operation.type == "translate_y" &&
           std::abs(operation.offset_y_px) > 0.0) ||
           (operation.type == "brightness_scale" && operation.scale > 0.05 &&
            operation.scale <= 2.0 && std::abs(operation.offset) <= 255.0) ||
           (operation.type == "scale_scene" && operation.scale >= 0.70 &&
            operation.scale <= 1.30) ||
           (operation.type == "side_appearance_shift" && operation.scale > 0.25 &&
            operation.scale <= 1.75 && std::abs(operation.offset) <= 96.0) ||
           (operation.type == "boundary_sharpen" && operation.sigma > 0.0 &&
            operation.scale > 0.0 && operation.scale <= 4.0) ||
           (operation.type == "elastic_deform" && operation.kernel >= 16 &&
            operation.sigma > 0.0 && operation.sigma <= 24.0) ||
          ((operation.type == "local_gap" ||
            operation.type == "edge_jagged_cut" ||
            operation.type == "line_break") &&
           operation.count >= 1 && operation.count <= 8 &&
           operation.width_px >= 0 && operation.height_px >= 0 &&
           operation.jagged_px >= 0 && operation.jagged_px <= 32);
      if (!valid) {
        reason = "unsupported augmentation operation or invalid parameters";
        return false;
      }
      variant.operations.push_back(operation);
    }
    if (variant.operations.empty()) {
      reason = "augmentation variant has no operations";
      return false;
    }
    variants.push_back(std::move(variant));
  }
  return !variants.empty();
}

bool ReadAugSources(const std::filesystem::path &index_path,
                    std::vector<AugSource> &sources, std::string &reason) {
  cv::FileStorage index;
  try {
    index.open(index_path.string(),
               cv::FileStorage::READ | cv::FileStorage::FORMAT_JSON);
  } catch (const cv::Exception &error) {
    reason = std::string("reference index parse failed: ") + error.what();
    return false;
  }
  if (!index.isOpened() || !index["cases"].isSeq()) {
    reason = "reference index is invalid";
    return false;
  }

  const std::filesystem::path root = index_path.parent_path();
  std::error_code path_error;
  if (!std::filesystem::is_directory(root, path_error) ||
      std::filesystem::is_symlink(root, path_error)) {
    reason = "reference asset root is missing or unsafe";
    return false;
  }

  std::set<std::string> directories;
  for (const cv::FileNode &item : index["cases"]) {
    AugSource source;
    source.directory = NodeString(item, "directory");
    source.source_split = NodeString(item, "source_split");
    const std::string manifest_ref = NodeString(item, "manifest");
    const std::filesystem::path manifest_path = root / manifest_ref;
    path_error.clear();
    if (!AugSafeComponent(source.directory) || manifest_ref.empty() ||
        manifest_ref.find(':') != std::string::npos ||
        !directories.insert(source.directory).second ||
        !IsWithinRoot(root, manifest_path) ||
        !std::filesystem::is_regular_file(manifest_path, path_error) ||
        std::filesystem::is_symlink(manifest_path, path_error)) {
      reason = "reference case directory or manifest is invalid";
      return false;
    }

    cv::FileStorage manifest;
    try {
      manifest.open(manifest_path.string(),
                    cv::FileStorage::READ | cv::FileStorage::FORMAT_JSON);
    } catch (const cv::Exception &error) {
      reason = std::string("reference manifest parse failed: ") + error.what();
      return false;
    }
    if (!manifest.isOpened()) {
      reason = "reference manifest is invalid";
      return false;
    }

    const std::filesystem::path case_root = manifest_path.parent_path();
    if (!IsWithinRoot(root, case_root) ||
        std::filesystem::is_symlink(case_root, path_error)) {
      reason = "reference case root is outside asset root or unsafe";
      return false;
    }

    source.review_item = NodeString(manifest.root(), "review_item");
    source.case_track = NodeString(manifest.root(), "case_track");
    source.geometry_type = NodeString(manifest.root(), "geometry_type");
    source.topology = NodeString(manifest.root(), "topology");
    source.label_kind = NodeString(manifest.root(), "typed_label_kind");
    source.image = case_root / NodeString(manifest.root(), "input_image");
    source.label = case_root / NodeString(manifest.root(), "typed_label");
    source.facts =
        case_root / NodeString(manifest.root(), "geometry_facts_ref");
    const std::string anchor_policy_ref =
        NodeString(manifest.root(), "anchor_policy_ref");
    const std::string anchor_candidates_ref =
        NodeString(manifest.root(), "anchor_candidates_ref");
    if (anchor_policy_ref.empty() != anchor_candidates_ref.empty()) {
      reason = "reference anchor assets must provide policy and candidates together";
      return false;
    }
    if (!anchor_policy_ref.empty()) {
      source.anchor_policy = case_root / anchor_policy_ref;
      source.anchor_candidates = case_root / anchor_candidates_ref;
    }

    for (const std::filesystem::path *asset :
         {&source.image, &source.label, &source.facts}) {
      path_error.clear();
      if (!IsWithinRoot(root, *asset) || !IsWithinRoot(case_root, *asset) ||
          !std::filesystem::is_regular_file(*asset, path_error) ||
          std::filesystem::is_symlink(*asset, path_error)) {
        reason = "reference case required assets are missing or unsafe";
        return false;
      }
    }
    if (source.review_item.empty() || source.geometry_type.empty()) {
      reason = "reference case required identity fields are missing";
      return false;
    }
    if ((!source.anchor_policy.empty() &&
         (!IsWithinRoot(root, source.anchor_policy) ||
          !std::filesystem::is_regular_file(source.anchor_policy, path_error) ||
          std::filesystem::is_symlink(source.anchor_policy, path_error))) ||
        (!source.anchor_candidates.empty() &&
         (!IsWithinRoot(root, source.anchor_candidates) ||
          !std::filesystem::is_regular_file(source.anchor_candidates, path_error) ||
          std::filesystem::is_symlink(source.anchor_candidates, path_error)))) {
      reason = "reference anchor asset is missing or unsafe";
      return false;
    }
    sources.push_back(std::move(source));
  }
  return !sources.empty();
}

bool AugVariantHasStructuralDefect(const AugVariant &variant) {
  for (const AugOperation &operation : variant.operations) {
    if (operation.type == "local_gap" ||
        operation.type == "edge_jagged_cut" ||
        operation.type == "line_break")
      return true;
  }
  return false;
}

bool AugVariantCanChangeVisibleBoundaryExtent(const AugVariant &variant) {
  for (const AugOperation &operation : variant.operations) {
    if (operation.type == "local_gap" || operation.type == "edge_jagged_cut" ||
        operation.type == "line_break" || operation.type == "rotate" ||
        operation.type == "scale_scene" || operation.type == "elastic_deform")
      return true;
  }
  return false;
}

bool AugVariantHasPhotometricShift(const AugVariant &variant) {
  for (const AugOperation &operation : variant.operations) {
    if (operation.type == "gaussian_blur" ||
         operation.type == "sensor_noise" ||
         operation.type == "brightness_scale" ||
         operation.type == "side_appearance_shift" ||
         operation.type == "boundary_sharpen")
      return true;
  }
  return false;
}

bool ApplySideAppearanceShift(cv::Mat &image, const cv::Mat &label,
                              const AugOperation &operation,
                              std::string &reason) {
  if (image.empty() || label.empty() || image.size() != label.size()) {
    reason = "side appearance shift needs matching image and boundary label";
    return false;
  }
  cv::Mat barrier;
  cv::dilate(label, barrier, cv::getStructuringElement(cv::MORPH_ELLIPSE,
      cv::Size(5, 5)));
  cv::Mat passable;
  cv::threshold(barrier, passable, 0, 255, cv::THRESH_BINARY_INV);
  cv::Mat top = passable.clone();
  cv::floodFill(top, cv::Point(image.cols / 2, 0), cv::Scalar(128));
  top = top == 128;
  // For an open boundary, the remaining passable pixels are the opposite
  // image side. For a closed boundary, they are the enclosed material region.
  cv::Mat bottom;
  cv::bitwise_and(passable, ~top, bottom);
  if (cv::countNonZero(top) == 0 || cv::countNonZero(bottom) == 0) {
    reason = "side appearance shift could not resolve two boundary sides";
    return false;
  }
  cv::Mat upper, lower;
  image.convertTo(upper, image.type(), operation.scale, operation.offset);
  image.convertTo(lower, image.type(), 2.0 - operation.scale, -operation.offset);
  upper.copyTo(image, top);
  lower.copyTo(image, bottom);
  return true;
}

bool ApplyBoundarySharpen(cv::Mat &image, const cv::Mat &label,
                          const AugOperation &operation,
                          std::string &reason) {
  if (image.empty() || label.empty() || image.size() != label.size()) {
    reason = "boundary sharpen needs matching image and boundary label";
    return false;
  }
  cv::Mat blurred, sharpened, band;
  cv::GaussianBlur(image, blurred, cv::Size(), operation.sigma, operation.sigma);
  cv::addWeighted(image, 1.0 + operation.scale, blurred, -operation.scale,
                  0.0, sharpened);
  cv::dilate(label, band, cv::getStructuringElement(cv::MORPH_ELLIPSE,
      cv::Size(9, 9)));
  sharpened.copyTo(image, band);
  return true;
}

bool ApplyElasticDeform(cv::Mat &image, cv::Mat &label, cv::Mat &latent_label,
                        const AugOperation &operation, int seed,
                        std::string &reason) {
  if (image.empty() || label.empty() || latent_label.empty()) {
    reason = "elastic deformation needs image and both boundary labels";
    return false;
  }
  cv::Mat map_x(image.size(), CV_32FC1), map_y(image.size(), CV_32FC1);
  const float amplitude = static_cast<float>(operation.sigma);
  const float wavelength = static_cast<float>(operation.kernel);
  const float phase_x = static_cast<float>((seed % 97) * 0.071);
  const float phase_y = static_cast<float>((seed % 89) * 0.083);
  for (int y = 0; y < image.rows; ++y) {
    for (int x = 0; x < image.cols; ++x) {
      map_x.at<float>(y, x) = static_cast<float>(x) + amplitude *
          std::sin(static_cast<float>(y) / wavelength + phase_x);
      map_y.at<float>(y, x) = static_cast<float>(y) + amplitude *
          std::sin(static_cast<float>(x) / wavelength + phase_y);
    }
  }
  cv::remap(image, image, map_x, map_y, cv::INTER_LINEAR,
            cv::BORDER_REPLICATE);
  cv::remap(label, label, map_x, map_y, cv::INTER_NEAREST,
            cv::BORDER_CONSTANT, cv::Scalar(0));
  cv::remap(latent_label, latent_label, map_x, map_y, cv::INTER_NEAREST,
            cv::BORDER_CONSTANT, cv::Scalar(0));
  return true;
}

std::string AugDegradationBucket(const AugVariant &variant) {
  const bool structural = AugVariantCanChangeVisibleBoundaryExtent(variant);
  const bool photometric = AugVariantHasPhotometricShift(variant);
  if (structural && photometric)
    return "photometric_and_structural_defect";
  if (structural)
    return "structural_defect";
  if (photometric)
    return "photometric_degradation";
  return "geometric_transform";
}

bool ApplyAugCutMask(cv::Mat &image, cv::Mat &label,
                     const cv::Scalar &background,
                     const AugOperation &operation, cv::RNG &random,
                     std::string &reason) {
  std::vector<cv::Point> label_points;
  cv::findNonZero(label, label_points);
  if (label_points.empty()) {
    reason = "typed label is empty before defect operation";
    return false;
  }
  const cv::Rect bounds = cv::boundingRect(label_points);
  const cv::Rect image_rect(0, 0, image.cols, image.rows);
  cv::Mat cut_mask(label.size(), CV_8UC1, cv::Scalar(0));
  const int repeat = std::max(1, operation.count);

  for (int index = 0; index < repeat; ++index) {
    const int base_width =
        operation.width_px > 0
            ? operation.width_px
            : std::max(4, static_cast<int>(std::lround(bounds.width * 0.18)));
    const int base_height =
        operation.height_px > 0
            ? operation.height_px
            : std::max(4, static_cast<int>(std::lround(bounds.height * 0.18)));

    if (operation.type == "line_break") {
      if (bounds.width >= bounds.height) {
        const int gap_width = std::max(3, base_width);
        const int x_min = bounds.x + std::max(1, bounds.width / 4);
        const int x_max = bounds.x + std::max(1, bounds.width * 3 / 4);
        const int cx = random.uniform(std::min(x_min, x_max),
                                      std::max(x_min + 1, x_max + 1));
        const cv::Rect cut(cx - gap_width / 2, bounds.y - 3, gap_width,
                           bounds.height + 6);
        cv::rectangle(cut_mask, cut & image_rect, cv::Scalar(255), cv::FILLED);
      } else {
        const int gap_height = std::max(3, base_height);
        const int y_min = bounds.y + std::max(1, bounds.height / 4);
        const int y_max = bounds.y + std::max(1, bounds.height * 3 / 4);
        const int cy = random.uniform(std::min(y_min, y_max),
                                      std::max(y_min + 1, y_max + 1));
        const cv::Rect cut(bounds.x - 3, cy - gap_height / 2, bounds.width + 6,
                           gap_height);
        cv::rectangle(cut_mask, cut & image_rect, cv::Scalar(255), cv::FILLED);
      }
      continue;
    }

    const int side = random.uniform(0, 4);
    cv::Rect cut;
    if (side == 0) {
      const int span = std::max(4, base_height);
      const int y0 = random.uniform(bounds.y, std::max(bounds.y + 1,
                                                       bounds.y + bounds.height));
      cut = cv::Rect(bounds.x - 2, y0 - span / 2, base_width + 4, span);
    } else if (side == 1) {
      const int span = std::max(4, base_height);
      const int y0 = random.uniform(bounds.y, std::max(bounds.y + 1,
                                                       bounds.y + bounds.height));
      cut = cv::Rect(bounds.x + bounds.width - base_width - 2, y0 - span / 2,
                     base_width + 4, span);
    } else if (side == 2) {
      const int span = std::max(4, base_width);
      const int x0 = random.uniform(bounds.x, std::max(bounds.x + 1,
                                                       bounds.x + bounds.width));
      cut = cv::Rect(x0 - span / 2, bounds.y - 2, span, base_height + 4);
    } else {
      const int span = std::max(4, base_width);
      const int x0 = random.uniform(bounds.x, std::max(bounds.x + 1,
                                                       bounds.x + bounds.width));
      cut = cv::Rect(x0 - span / 2, bounds.y + bounds.height - base_height - 2,
                     span, base_height + 4);
    }
    cut &= image_rect;
    if (cut.area() <= 0)
      continue;
    cv::rectangle(cut_mask, cut, cv::Scalar(255), cv::FILLED);

    if (operation.type == "edge_jagged_cut" && operation.jagged_px > 0) {
      const int teeth = std::max(3, std::min(12, (cut.width + cut.height) / 8));
      for (int tooth = 0; tooth < teeth; ++tooth) {
        const cv::Point center(random.uniform(cut.x, cut.x + cut.width),
                               random.uniform(cut.y, cut.y + cut.height));
        cv::circle(cut_mask, center, operation.jagged_px, cv::Scalar(255),
                   cv::FILLED, cv::LINE_AA);
      }
    }
  }

  if (cv::countNonZero(cut_mask) <= 0) {
    reason = "defect operation produced empty cut mask";
    return false;
  }
  image.setTo(background, cut_mask);
  label.setTo(cv::Scalar(0), cut_mask);
  return true;
}


bool ApplyAugVariant(const cv::Mat &source_image, const cv::Mat &source_label,
                     const AugVariant &variant, cv::Mat &image, cv::Mat &label,
                     cv::Mat &latent_label,
                     cv::Matx33d &matrix, double &rotation_deg,
                     std::string &reason) {
  image = source_image.clone();
  label = source_label.clone();
  latent_label = source_label.clone();
  matrix = cv::Matx33d::eye();
  rotation_deg = 0.0;
  const cv::Rect corner(0, 0, std::min(8, image.cols), std::min(8, image.rows));
  const cv::Scalar background = cv::mean(image(corner));
  cv::RNG random(
      static_cast<std::uint64_t>(static_cast<std::uint32_t>(variant.seed)));

  for (const AugOperation &operation : variant.operations) {
    if (operation.type == "gaussian_blur") {
      cv::GaussianBlur(image, image,
                       cv::Size(operation.kernel, operation.kernel),
                       operation.sigma, operation.sigma, cv::BORDER_REPLICATE);
      continue;
    }
    if (operation.type == "sensor_noise") {
      cv::Mat converted;
      image.convertTo(converted, CV_MAKETYPE(CV_16S, image.channels()));
      cv::Mat noise(image.size(), converted.type());
      random.fill(noise, cv::RNG::NORMAL, 0.0, operation.sigma);
      cv::add(converted, noise, converted);
      converted.convertTo(image, source_image.type());
      continue;
    }
    if (operation.type == "brightness_scale") {
      image.convertTo(image, source_image.type(), operation.scale,
                      operation.offset);
      continue;
    }
    if (operation.type == "side_appearance_shift") {
      if (!ApplySideAppearanceShift(image, latent_label, operation, reason))
        return false;
      continue;
    }
    if (operation.type == "boundary_sharpen") {
      if (!ApplyBoundarySharpen(image, latent_label, operation, reason))
        return false;
      continue;
    }
    if (operation.type == "elastic_deform") {
      if (!ApplyElasticDeform(image, label, latent_label, operation,
                              variant.seed, reason))
        return false;
      continue;
    }
    if (operation.type == "local_gap" ||
        operation.type == "edge_jagged_cut" ||
        operation.type == "line_break") {
      if (!ApplyAugCutMask(image, label, background, operation, random, reason))
        return false;
      continue;
    }

    cv::Mat affine;
    if (operation.type == "rotate") {
      affine = cv::getRotationMatrix2D(
          cv::Point2f(static_cast<float>(image.cols - 1) * 0.5f,
                      static_cast<float>(image.rows - 1) * 0.5f),
          operation.angle_deg, 1.0);
      rotation_deg += operation.angle_deg;
    } else if (operation.type == "translate_y") {
      affine = (cv::Mat_<double>(2, 3) << 1.0, 0.0, 0.0, 0.0, 1.0,
                operation.offset_y_px);
    } else if (operation.type == "scale_scene") {
      affine = cv::getRotationMatrix2D(
          cv::Point2f(static_cast<float>(image.cols - 1) * 0.5f,
                      static_cast<float>(image.rows - 1) * 0.5f), 0.0,
          operation.scale);
    } else {
      reason = "unsupported augmentation operation reached execution";
      return false;
    }
    cv::warpAffine(image, image, affine, image.size(), cv::INTER_LINEAR,
                   cv::BORDER_CONSTANT, background);
    cv::warpAffine(label, label, affine, label.size(), cv::INTER_NEAREST,
                    cv::BORDER_CONSTANT, cv::Scalar(0));
    cv::warpAffine(latent_label, latent_label, affine, latent_label.size(),
                    cv::INTER_NEAREST, cv::BORDER_CONSTANT, cv::Scalar(0));
    matrix = AugMatrix(affine) * matrix;
  }
  cv::threshold(label, label, 0, 255, cv::THRESH_BINARY);
  cv::threshold(latent_label, latent_label, 0, 255, cv::THRESH_BINARY);
  return true;
}

bool WriteAugFacts(const AugSource &source,
                   const std::filesystem::path &output_path,
                   const cv::Matx33d &matrix, double rotation_deg,
                   const cv::Rect &bounds, const cv::Point2d &centroid) {
  cv::FileStorage input(source.facts.string(),
                        cv::FileStorage::READ | cv::FileStorage::FORMAT_JSON);
  cv::FileStorage output(output_path.string(),
                         cv::FileStorage::WRITE | cv::FileStorage::FORMAT_JSON);
  if (!input.isOpened() || !output.isOpened())
    return false;

  output << "schema"
         << "cxvision.geometry_augmented_facts.v1";
  output << "geometry_type" << source.geometry_type;
  output << "topology" << source.topology;
  output << "source_facts_ref"
         << "source_geometry_facts.json";
  AugWriteMatrix(output, "affine_source_to_output", matrix);
  output << "applied_rotation_deg" << rotation_deg;
  output << "standard_position"
         << "{";
  output << "bbox_xywh"
         << "[" << bounds.x << bounds.y << bounds.width << bounds.height << "]";
  output << "centroid_xy"
         << "[" << centroid.x << centroid.y << "]";
  output << "}";

  if (source.topology == "closed") {
    const cv::FileNode instance = input["instances"][0];
    output << "instances"
           << "["
           << "{";
    output << "instance_id" << 1;
    output << "class_name" << source.geometry_type;
    if (!instance["center_xy"].empty()) {
      cv::Point2f point;
      if (!ReadPoint(instance["center_xy"], point))
        return false;
      AugWritePoint(output, "center_xy", AugPoint(matrix, point));
    }
    const char *scalar_keys[] = {"radius_px", "radius_x_px", "radius_y_px",
                                 "width_px", "height_px"};
    for (const char *key : scalar_keys) {
      if (!instance[key].empty())
        output << key << NodeDouble(instance, key);
    }
    if (!instance["rotation_deg"].empty())
      output << "rotation_deg"
             << NodeDouble(instance, "rotation_deg") + rotation_deg;
    if (!instance["vertices_xy"].empty())
      AugWritePoints(output, "vertices_xy", instance["vertices_xy"], matrix);
    if (!instance["control_points_xy"].empty())
      AugWritePoints(output, "control_points_xy",
                     instance["control_points_xy"], matrix);
    if (!instance["tension"].empty())
      output << "tension" << NodeDouble(instance, "tension");
    output << "closed" << 1;
    output << "}"
           << "]";
  } else {
    if (!input["center_xy"].empty()) {
      cv::Point2f point;
      if (!ReadPoint(input["center_xy"], point))
        return false;
      AugWritePoint(output, "center_xy", AugPoint(matrix, point));
    }
    if (!input["radius_px"].empty())
      output << "radius_px" << NodeDouble(input.root(), "radius_px");
    if (!input["start_angle_deg"].empty())
      output << "start_angle_deg"
             << NodeDouble(input.root(), "start_angle_deg") + rotation_deg;
    if (!input["end_angle_deg"].empty())
      output << "end_angle_deg"
             << NodeDouble(input.root(), "end_angle_deg") + rotation_deg;
    if (!input["endpoints_xy"].empty())
      AugWritePoints(output, "endpoints_xy", input["endpoints_xy"], matrix);
    if (!input["control_points_xy"].empty())
      AugWritePoints(output, "control_points_xy", input["control_points_xy"],
                     matrix);
    if (!input["measurement_width_px"].empty())
      output << "measurement_width_px"
             << NodeDouble(input.root(), "measurement_width_px");
    output << "closed" << 0;
  }
  output.release();
  return true;
}

bool WriteAugPosition(const std::filesystem::path &path,
                      const AugSource &source, const AugVariant &variant,
                      const cv::Matx33d &matrix, const cv::Rect &bounds,
                      const cv::Point2f &source_centroid,
                      const cv::Point2f &affine_centroid,
                      const cv::Point2d &centroid, double retained_ratio,
                      double centroid_error, int width, int height) {
  cv::FileStorage output(path.string(),
                         cv::FileStorage::WRITE | cv::FileStorage::FORMAT_JSON);
  if (!output.isOpened())
    return false;
  output << "schema"
         << "cxvision.standard_geometry_position.v1";
  output << "coordinate_system"
         << "pixel_center_origin_top_left_x_right_y_down";
  output << "image_width" << width;
  output << "image_height" << height;
  output << "geometry_type" << source.geometry_type;
  output << "topology" << source.topology;
  output << "split" << variant.split;
  output << "typed_label"
          << "typed_label.png";
  output << "latent_boundary_map"
         << "latent_boundary_map.png";
  output << "bbox_xywh"
         << "[" << bounds.x << bounds.y << bounds.width << bounds.height << "]";
  output << "centroid_xy"
         << "[" << centroid.x << centroid.y << "]";
  output << "source_centroid_xy"
         << "[" << source_centroid.x << source_centroid.y << "]";
  output << "affine_centroid_xy"
         << "[" << affine_centroid.x << affine_centroid.y << "]";
  output << "centroid_consistency_error_px" << centroid_error;
  output << "retained_label_ratio" << retained_ratio;
  AugWriteMatrix(output, "affine_source_to_output", matrix);
  output << "operations"
         << "[";
  for (const AugOperation &operation : variant.operations) {
    output << "{";
    output << "type" << operation.type;
    if (operation.kernel != 0)
      output << "kernel" << operation.kernel;
    if (operation.width_px != 0)
      output << "width_px" << operation.width_px;
    if (operation.height_px != 0)
      output << "height_px" << operation.height_px;
    if (operation.count != 1)
      output << "count" << operation.count;
    if (operation.jagged_px != 0)
      output << "jagged_px" << operation.jagged_px;
    if (operation.sigma != 0.0)
      output << "sigma" << operation.sigma;
    if (operation.angle_deg != 0.0)
      output << "angle_deg" << operation.angle_deg;
    if (operation.offset_y_px != 0.0)
      output << "offset_y_px" << operation.offset_y_px;
    if (operation.scale != 1.0)
      output << "scale" << operation.scale;
    if (operation.offset != 0.0)
      output << "offset" << operation.offset;
    output << "}";
  }
  output << "]";
  output.release();
  return true;
}




std::string AugDifficultyBucket(const AugVariant &variant) {
  bool photometric = false;
  bool geometric = false;
  bool structural = false;
  for (const AugOperation &operation : variant.operations) {
    photometric = photometric || operation.type == "gaussian_blur" ||
                  operation.type == "sensor_noise" ||
                  operation.type == "brightness_scale";
    geometric = geometric || operation.type == "rotate" ||
                operation.type == "translate_y";
    structural = structural || operation.type == "local_gap" ||
                 operation.type == "edge_jagged_cut" ||
                 operation.type == "line_break";
  }
  if (structural)
    return photometric || geometric ? "compound_structural_defect"
                                    : "structural_defect";
  if (photometric && geometric)
    return "compound";
  return variant.operations.size() > 1 ? "multi_perturbation"
                                       : "single_perturbation";
}

bool WriteAugTrainingTarget(const std::filesystem::path &directory,
                            const AugSource &source,
                            const AugVariant &variant,
                            const cv::Rect &bounds, double rotation_deg,
                            double retained_ratio, double centroid_error,
                            int width, int height, AugRow &row) {
  const double image_area =
      static_cast<double>(std::max(1, width * height));
  const double area_ratio =
      static_cast<double>(bounds.area()) / image_area;
  row.size_bucket =
      area_ratio < 0.05 ? "small" : (area_ratio < 0.20 ? "medium" : "large");
  if (row.angle_bucket.empty()) {
    const double normalized_rotation =
        std::fmod(std::abs(rotation_deg), 90.0);
    row.angle_bucket =
        normalized_rotation < 2.0 || normalized_rotation > 88.0
            ? "axis_aligned"
            : "rotated";
  }
  row.difficulty_bucket = AugDifficultyBucket(variant);
  row.degradation_bucket = AugDegradationBucket(variant);
  row.visible_ratio = retained_ratio;
  const bool structural = AugVariantCanChangeVisibleBoundaryExtent(variant);
  row.identifiable =
      bounds.area() > 0 &&
      (structural ? retained_ratio >= 0.70 : retained_ratio >= 0.985) &&
      (structural ? centroid_error <= 24.0 : centroid_error <= 2.0);

  cv::FileStorage output((directory / "training_target.json").string(),
                         cv::FileStorage::WRITE |
                             cv::FileStorage::FORMAT_JSON);
  if (!output.isOpened())
    return false;
  output << "schema" << "cxvision.geometry_training_target.v1";
  output << "geometry_type" << source.geometry_type;
  output << "topology" << source.topology;
  output << "instance_target_mode"
         << "per_annotation_optimal_geometry_parameters";
  output << "teacher_signal_ref" << "geometry_facts.json";
  output << "typed_label_ref" << "typed_label.png";
  output << "tolerance_policy_ref"
         << ("geometry_typed_contracts.json#" + source.geometry_type);
  output << "visibility_ratio" << retained_ratio;
  output << "degradation" << row.degradation_bucket;
  output << "partial_visibility"
          << (AugVariantCanChangeVisibleBoundaryExtent(variant) ? 1 : 0);
  output << "identifiable" << (row.identifiable ? 1 : 0);
  output << "rejection_reason"
         << (row.identifiable ? "" : "GEOMETRY_NOT_IDENTIFIABLE");
  output << "strata" << "{";
  output << "class" << source.geometry_type;
  output << "size" << row.size_bucket;
  output << "angle" << row.angle_bucket;
  output << "difficulty" << row.difficulty_bucket;
  output << "degradation" << row.degradation_bucket;
  output << "}";
  output << "selection_semantics"
         << "best_parameter_solution_is_selected_per_annotation_before_dataset_level_optimization";
  output << "dataset_objective"
         << "minimize_robust_aggregate_error_over_accepted_instance_targets";
  output << "training_enabled" << 0;
  output << "human_review_required" << 1;
  output.release();
  return row.identifiable;
}



bool WriteAugMaskGeometryFit(const std::filesystem::path &directory,
                             const AugSource &source,
                             const cv::Mat &label, AugRow &row) {
  const bool supported = source.geometry_type == "circle" ||
                         source.geometry_type == "ellipse" ||
                         source.geometry_type == "line";
  CxSegmentationGeometryFitResult fit;
  if (supported) {
    std::vector<cv::Point> contour;
    if (!LargestContour(label, contour)) {
      row.mask_fit_status = "CONTOUR_EMPTY";
      return false;
    }
    CxSegmentationGeometryFitOptions options;
    options.geometry_type = source.geometry_type;
    options.tolerance_policy_ref =
        "geometry_typed_contracts.json#" + source.geometry_type;
    options.image_evidence_ref = "typed_label.png";
    options.residual_limit_px = 2.0;
    options.support_limit = 0.80;
    options.independent_image_evidence_verified = false;
    FitCxSegmentationContourGeometry(contour, options, fit);
    row.mask_fit_status = fit.status;
  } else {
    fit.status = "PENDING_FITTER_BINDING";
    fit.reason =
        "typed target is ready; contour fitter is not bound for this type";
    row.mask_fit_status = fit.status;
  }

  cv::FileStorage output((directory / "mask_geometry_fit.json").string(),
                         cv::FileStorage::WRITE |
                             cv::FileStorage::FORMAT_JSON);
  if (!output.isOpened())
    return false;
  output << "schema" << "cxvision.seg_mask_geometry_fit.v1";
  output << "geometry_type" << source.geometry_type;
  output << "mask_source" << "typed_ground_truth";
  output << "yolov8_seg_model_executed" << 0;
  output << "model_binding_status" << "PENDING_MODEL_MASK";
  output << "fit_status" << fit.status;
  output << "fit_reason" << fit.reason;
  output << "fit_complete" << (fit.complete ? 1 : 0);
  output << "best_parameter_solution_scope" << "one_annotation_instance";
  if (fit.complete) {
    const CxGeometryPrimitiveHypothesis &hypothesis = fit.hypothesis;

    if (source.geometry_type == "circle") {
      row.angle_bucket = "not_applicable";
    } else {
      const double normalized_angle =
          std::fmod(std::abs(hypothesis.angle_deg), 90.0);
      row.angle_bucket =
          normalized_angle < 2.0 || normalized_angle > 88.0
              ? "axis_aligned"
              : "rotated";
    }
    output << "center_xy"
           << "[" << hypothesis.center.x << hypothesis.center.y << "]";
    output << "radius_px" << hypothesis.radius;
    output << "axes_radius_xy"
           << "[" << hypothesis.axes.width << hypothesis.axes.height << "]";
    output << "angle_deg" << hypothesis.angle_deg;
    output << "fit_residual_px" << hypothesis.classical_fit_residual_px;
    output << "support" << hypothesis.support;
    output << "ordered_points_xy" << "[";
    for (const cv::Point2d &point : hypothesis.ordered_points)
      output << "[" << point.x << point.y << "]";
    output << "]";
  }
  output << "allowed_claim"
         << "typed_mask_to_geometry_fitter_pipeline_only";
  output << "forbidden_claim"
         << "yolov8_seg_model_accuracy_or_inference_pass";
  output.release();
  return !supported || fit.complete;
}


bool WriteAugTrainingIndex(
    const CxGeometryAugmentationDatasetOptions &options,
    const std::vector<AugRow> &rows) {
  cv::FileStorage output(
      (options.output_dir / "geometry_training_index.json").string(),
      cv::FileStorage::WRITE | cv::FileStorage::FORMAT_JSON);
  if (!output.isOpened())
    return false;
  output << "schema" << "cxvision.geometry_training_index.v1";
  output << "coordinate_system"
         << "pixel_center_origin_top_left_x_right_y_down";
  output << "sample_target_semantics"
         << "one_best_geometry_parameter_solution_per_annotation";
  output << "dataset_objective"
         << "robust_aggregate_loss_over_accepted_per_instance_targets";
  output << "stratification_axes"
      << "[" << "geometry_type" << "size_bucket" << "angle_bucket"
      << "difficulty_bucket" << "degradation_bucket" << "]";
  output << "training_enabled" << 0;
  output << "human_review_required" << 1;
  output << "samples" << "[";
  for (const AugRow &row : rows) {
    if (row.status != "GENERATED")
      continue;
    const std::filesystem::path relative =
        row.case_path.lexically_relative(options.output_dir);
    output << "{";
    output << "review_item" << row.review_item;
    output << "split" << row.split;
    output << "geometry_type" << row.geometry_type;
    output << "size_bucket" << row.size_bucket;
    output << "angle_bucket" << row.angle_bucket;
    output << "difficulty_bucket" << row.difficulty_bucket;
    output << "degradation_bucket" << row.degradation_bucket;
    output << "visibility_ratio" << row.visible_ratio;
    output << "identifiable" << (row.identifiable ? 1 : 0);
    output << "mask_fit_status" << row.mask_fit_status;
    output << "case_path" << relative.generic_string();
    output << "teacher_signal_ref"
           << (relative / "geometry_facts.json").generic_string();
    output << "training_target_ref"
           << (relative / "training_target.json").generic_string();
    output << "mask_geometry_fit_ref"
           << (relative / "mask_geometry_fit.json").generic_string();
    output << "training_eligible"
           << (row.split == "train" && row.identifiable ? 1 : 0);
    output << "}";
  }
  output << "]";
  output.release();
  return true;
}





bool WriteAugHashes(const std::filesystem::path &directory,
                    const AugSource &source,
                    const std::filesystem::path &image_path,
                    const std::filesystem::path &label_path) {
  const std::string source_image_hash = AugFileHash(source.image);
  const std::string source_label_hash = AugFileHash(source.label);
  const std::string generated_image_hash = AugFileHash(image_path);
  const std::string generated_label_hash = AugFileHash(label_path);
  if (source_image_hash.empty() || source_label_hash.empty() ||
      generated_image_hash.empty() || generated_label_hash.empty())
    return false;

  cv::FileStorage output((directory / "source_hashes.json").string(),
                         cv::FileStorage::WRITE | cv::FileStorage::FORMAT_JSON);
  if (!output.isOpened())
    return false;
  output << "schema"
         << "cxvision.augmentation_hashes.v1";
  output << "algorithm"
         << "fnv1a64";
  output << "source_image" << source_image_hash;
  output << "source_typed_label" << source_label_hash;
  output << "generated_image" << generated_image_hash;
  output << "generated_typed_label" << generated_label_hash;
  output.release();
  return true;
}

bool WriteAugReviewAssets(const std::filesystem::path &directory,
                          const AugSource &source, const AugVariant &variant,
                          const std::string &review_item) {
  cv::FileStorage summary((directory / "result_summary.json").string(),
                          cv::FileStorage::WRITE |
                              cv::FileStorage::FORMAT_JSON);
  if (!summary.isOpened())
    return false;
  summary << "schema"
          << "cxvision.geometry_augmentation_result.v1";
  summary << "status"
          << "PASS_TO_REVIEW";
  summary << "review_item" << review_item;
  summary << "source_review_item" << source.review_item;
  summary << "geometry_type" << source.geometry_type;
  summary << "case_track" << source.case_track;
  summary << "split" << variant.split;
  summary << "variant_id" << variant.id;
  summary << "training_eligible" << (variant.split == "train" ? 1 : 0);
  summary << "training_enabled" << 0;
  summary << "human_review_required" << 1;
  summary << "allowed_claim"
          << "deterministic_augmentation_and_annotation_ready_for_human_review";
  summary.release();

  cv::FileStorage review((directory / "review_decision.json").string(),
                         cv::FileStorage::WRITE | cv::FileStorage::FORMAT_JSON);
  if (!review.isOpened())
    return false;
  review << "schema"
         << "cxvision.manual_review_decision.v1";
  review << "review_item" << review_item;
  review << "decision"
         << "PENDING_HUMAN_REVIEW";
  review << "reviewer"
         << "";
  review << "reviewed_at"
         << "";
  review << "notes"
         << "";
  review << "training_enabled" << 0;
  review.release();
  return true;
}

bool WriteAugAnchorProjection(const std::filesystem::path &directory,
                              const AugSource &source,
                              const AugVariant &variant,
                              const cv::Matx33d &matrix) {
  if (source.anchor_policy.empty() || source.anchor_candidates.empty())
    return true;
  const bool requires_reselection =
      AugVariantCanChangeVisibleBoundaryExtent(variant);
  cv::FileStorage output((directory / "anchor_projection.json").string(),
                         cv::FileStorage::WRITE | cv::FileStorage::FORMAT_JSON);
  if (!output.isOpened())
    return false;
  output << "schema" << "cxvision.boundary_anchor_projection.v1";
  output << "source_anchor_policy_ref" << "boundary_anchor_policy.json";
  output << "source_anchor_candidates_ref" << "source_anchor_candidates.json";
  output << "projection_matrix" << "[";
  for (int row = 0; row < 3; ++row)
    output << "[" << matrix(row, 0) << matrix(row, 1) << matrix(row, 2) << "]";
  output << "]";
  output << "projection_status"
         << (requires_reselection ? "REQUIRES_POST_AUGMENTATION_ANCHOR_RESELECTION"
                                  : "SOURCE_ANCHOR_CONTEXT_PRESERVED");
  output << "automatic_anchor_selection_allowed" << 0;
  output << "reason"
         << (requires_reselection
                 ? "gap, crop, rotation, scale, or non-affine deformation can change anchor visibility"
                 : "photometric-only variation preserves source anchor context");
  output.release();
  return true;
}

bool WriteImageDrivenGeometryHeadProbe(const std::filesystem::path &directory,
                                      const AugSource &source,
                                      const cv::Mat &image,
                                      const cv::Mat &truth_label) {
  cv::FileStorage output((directory / "image_geometry_head_probe.json").string(),
                         cv::FileStorage::WRITE | cv::FileStorage::FORMAT_JSON);
  if (!output.isOpened())
    return false;
  output << "schema" << "cxvision.image_geometry_head_probe.v1";
  output << "review_input" << "source_image.png";
  output << "prediction_source" << "image_pixels_only";
  output << "evaluation_truth_ref" << "typed_label.png";
  output << "roi_source" << "FULL_IMAGE_CONTROLLED_TEST_ONLY";
  output << "end_to_end_detector_roi_accepted" << 0;
  output << "geometry_type" << source.geometry_type;
  if ((source.geometry_type != "ellipse" && source.geometry_type != "polygon") ||
      image.empty() || truth_label.empty()) {
    output << "status" << "NOT_EXECUTED_FOR_THIS_GEOMETRY_TYPE";
    output.release();
    return true;
  }

  cv::Mat gray;
  cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
  cv::Mat binary;
  cv::threshold(gray, binary, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
  cv::morphologyEx(binary, binary, cv::MORPH_CLOSE,
                   cv::getStructuringElement(cv::MORPH_ELLIPSE,
                                             cv::Size(5, 5)));
  std::vector<std::vector<cv::Point>> contours;
  const auto select_interior_contour = [&image](const cv::Mat &candidate,
                                                 std::vector<std::vector<cv::Point>> &out) {
    cv::findContours(candidate, out, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);
    int selected = -1;
    double best_area = 0.0;
    for (std::size_t index = 0; index < out.size(); ++index) {
      const cv::Rect bounds = cv::boundingRect(out[index]);
      const bool touches_border = bounds.x <= 0 || bounds.y <= 0 ||
                                  bounds.x + bounds.width >= image.cols ||
                                  bounds.y + bounds.height >= image.rows;
      const double area = std::abs(cv::contourArea(out[index]));
      if (!touches_border && area > best_area) {
        best_area = area;
        selected = static_cast<int>(index);
      }
    }
    return selected;
  };
  int selected = select_interior_contour(binary, contours);
  std::string segmentation_polarity = "otsu_bright_region";
  if (selected < 0) {
    cv::Mat inverse;
    cv::bitwise_not(binary, inverse);
    selected = select_interior_contour(inverse, contours);
    segmentation_polarity = "otsu_inverse_region";
  }
  if (selected < 0) {
    cv::Mat smoothed, edges;
    cv::GaussianBlur(gray, smoothed, cv::Size(5, 5), 1.1);
    cv::Canny(smoothed, edges, 28.0, 84.0, 3, true);
    cv::morphologyEx(edges, edges, cv::MORPH_CLOSE,
                     cv::getStructuringElement(cv::MORPH_ELLIPSE,
                                               cv::Size(9, 9)));
    selected = select_interior_contour(edges, contours);
    segmentation_polarity = "canny_edge_closed_contour";
  }
  if (selected < 0 || contours[static_cast<std::size_t>(selected)].size() < 5) {
    output << "status" << "IMAGE_CONTOUR_NOT_FOUND";
    output.release();
    return true;
  }

  const std::vector<cv::Point> &contour = contours[static_cast<std::size_t>(selected)];
  output << "segmentation_polarity" << segmentation_polarity;
  cv::Mat predicted_boundary = cv::Mat::zeros(truth_label.size(), CV_8UC1);
  int vertex_count = 0;
  if (source.geometry_type == "ellipse") {
    const cv::RotatedRect ellipse = cv::fitEllipse(contour);
    cv::ellipse(predicted_boundary, ellipse, cv::Scalar(255), 2, cv::LINE_AA);
    output << "center_xy" << "[" << ellipse.center.x << ellipse.center.y << "]";
    output << "axes_radius_xy" << "[" << ellipse.size.width * 0.5
           << ellipse.size.height * 0.5 << "]";
    output << "angle_deg" << ellipse.angle;
  } else {
    std::vector<cv::Point> approximation;
    cv::approxPolyDP(contour, approximation, 0.015 * cv::arcLength(contour, true),
                     true);
    if (approximation.size() < 3) {
      output << "status" << "IMAGE_POLYGON_APPROXIMATION_FAILED";
      output.release();
      return true;
    }
    vertex_count = static_cast<int>(approximation.size());
    cv::polylines(predicted_boundary, approximation, true, cv::Scalar(255), 2,
                  cv::LINE_AA);
    output << "vertex_count" << vertex_count;
  }
  const double prediction_to_truth_px =
      DistanceToMask(truth_label, NonZeroPoints(predicted_boundary));
  output << "mean_prediction_to_truth_boundary_px" << prediction_to_truth_px;
  output << "status" << "IMAGE_DRIVEN_GEOMETRY_HEAD_EXECUTED_CONTROLLED_ROI_ONLY";
  output << "geometry_claim_status"
         << "NOT_END_TO_END_UNTIL_ACCEPTED_DETECTOR_ROI_AND_RUNTIME_RECEIPT";
  output.release();
  return true;
}

bool WriteAugManifest(const std::filesystem::path &directory,
                      const AugSource &source, const AugVariant &variant,
                      const std::string &review_item) {
  cv::FileStorage output((directory / "case_manifest.json").string(),
                         cv::FileStorage::WRITE | cv::FileStorage::FORMAT_JSON);
  if (!output.isOpened())
    return false;

  std::filesystem::path run_directory = directory;
  for (int level = 0; level < 4 && !run_directory.empty(); ++level)
    run_directory = run_directory.parent_path();
  const std::string run_id = run_directory.filename().string();
  const std::filesystem::path published_case_path = run_directory / "dataset" /
                                                    variant.split /
                                                    source.directory /
                                                    variant.id;

  output << "schema"
         << "cxvision.evidence_case.v1";
  output << "run_id" << run_id;
  output << "internal_case_id" << StableInternalCaseId(published_case_path);
  output << "asset_role"
         << "deterministic_geometry_augmentation";
  output << "review_item" << review_item;
  output << "source_review_item" << source.review_item;
  output << "case_track" << source.case_track;
  output << "geometry_type" << source.geometry_type;
  output << "topology" << source.topology;
  output << "split" << variant.split;
  output << "variant_id" << variant.id;
  output << "degradation_bucket" << AugDegradationBucket(variant);
  output << "partial_visibility"
         << (AugVariantCanChangeVisibleBoundaryExtent(variant) ? 1 : 0);
  output << "source_image"
         << "source_image.png";
  output << "input_image"
         << "source_image.png";
  output << "typed_label"
         << "typed_label.png";
  output << "typed_label_kind" << source.label_kind;
  output << "geometry_facts_ref"
         << "geometry_facts.json";
  output << "position_annotation_ref"
         << "position_annotation.json";

  output << "training_target_ref"
         << "training_target.json";
  output << "mask_geometry_fit_ref"
         << "mask_geometry_fit.json";
  output << "image_geometry_head_probe_ref" << "image_geometry_head_probe.json";
  output << "metrology_target_ref"
         << "metrology_target.json";
  output << "tolerance_contract_ref"
         << "geometry_typed_contracts.json";
  if (!source.anchor_policy.empty() && !source.anchor_candidates.empty()) {
    output << "anchor_policy_ref" << "boundary_anchor_policy.json";
    output << "anchor_candidates_source_ref" << "source_anchor_candidates.json";
    output << "anchor_projection_ref" << "anchor_projection.json";
  }
  output << "evidence_overlay"
         << "evidence_overlay.png";
  output << "result_summary"
         << "result_summary.json";
  output << "training_eligible" << (variant.split == "train" ? 1 : 0);
  output << "training_enabled" << 0;
  output << "human_review_required" << 1;
  output << "binding_status"
         << "PENDING_HUMAN_REVIEW";
  output << "required_assets"
          << "["
         << "source_image.png"
          << "typed_label.png"
          << "latent_boundary_map.png"
         << "geometry_facts.json"
           << "training_target.json"
           << "mask_geometry_fit.json"
           << "image_geometry_head_probe.json"
           << "metrology_target.json"
           << "geometry_typed_contracts.json";
  if (!source.anchor_policy.empty() && !source.anchor_candidates.empty()) {
    output << "boundary_anchor_policy.json"
           << "source_anchor_candidates.json"
           << "anchor_projection.json";
  }
  output
         << "source_geometry_facts.json"
         << "source_hashes.json"
         << "evidence_overlay.png"
         << "result_summary.json"
         << "review_decision.json"
         << "]";
  output.release();
  return true;
}

bool WriteAugMetrologyTarget(const std::filesystem::path &sample_path,
                             const AugSource &source,
                             const AugVariant &variant,
                             const std::string &review_item,
                             const cv::Rect &bounds, double rotation_deg,
                             double visibility_ratio,
                             double centroid_error_px, int image_width,
                             int image_height, const AugRow &row) {
  cv::FileStorage target((sample_path / "metrology_target.json").string(),
                         cv::FileStorage::WRITE | cv::FileStorage::FORMAT_JSON);
  if (!target.isOpened())
    return false;
  target << "schema" << "cxvision.metrology_target.v1";
  target << "review_item" << review_item;
  target << "source_review_item" << source.review_item;
  target << "primary_model_family" << "YOLOv8-n_detection";
  target << "model_structure_policy" << "reuse_base_yolov8n_without_disassembly";
  target << "training_execution_mode" << "external_incremental_training_evidence_only";
  target << "python_training_in_process" << 0;
  target << "sample_role" << "controlled_flow_case_not_real_scene_accuracy";
  target << "split" << variant.split;
  target << "variant_id" << variant.id;
  target << "geometry_type" << source.geometry_type;
  target << "topology" << source.topology;
  target << "typed_label_kind" << source.label_kind;
  target << "image_ref" << "source_image.png";
  target << "typed_label_ref" << "typed_label.png";
  target << "geometry_target_ref" << "training_target.json";
  target << "mask_geometry_fit_ref" << "mask_geometry_fit.json";
  target << "coordinate_system"
         << "pixel_center_origin_top_left_x_right_y_down";
  target << "image_size_px" << "[" << image_width << image_height << "]";
  target << "bbox_xywh_px" << "[" << bounds.x << bounds.y << bounds.width
         << bounds.height << "]";
  target << "roi_xyxy_px" << "[" << bounds.x << bounds.y
         << (bounds.x + bounds.width) << (bounds.y + bounds.height) << "]";
  target << "rotation_deg" << rotation_deg;
  target << "visibility_ratio" << visibility_ratio;
  target << "degradation" << row.degradation_bucket;
  target << "partial_visibility"
         << (AugVariantHasStructuralDefect(variant) ? 1 : 0);
  target << "centroid_consistency_error_px" << centroid_error_px;
  target << "identifiable" << (row.identifiable ? 1 : 0);
  target << "training_eligible"
         << (variant.split == "train" && row.identifiable ? 1 : 0);
  target << "clear_degraded_pair_status"
         << "PENDING_REAL_SCENE_CLEAR_DEGRADED_PAIR_REVIEW";
  target << "assignment_status" << "PENDING_YOLOV8N_INFERENCE_EVIDENCE";
  target << "detector_claim_status" << "DETECTION_ONLY_GEOMETRY_UNCONFIRMED";
  target << "geometry_inference_executed" << 0;
  target << "geometry_claim_status" << "PENDING_GEOMETRY_HEAD_BINDING";
  target << "allowed_conclusion"
         << "detector robustness only; no ellipse, polygon, or other geometry parameter conclusion";
  target << "forbidden_conclusion"
         << "rotation_or_scale_geometry_pass_without_geometry_head_receipt";
  target << "instance_geometry_loss_status"
         << "PENDING_MODEL_PREDICTION_ASSIGNMENT";
  target << "model_output_uncertainty_status"
         << "PENDING_YOLOV8N_OUTPUT_SCORE_AND_CALIBRATION";
  target << "physical_calibration_status"
         << "PENDING_PIXEL_TO_PHYSICAL_CALIBRATION";
  target << "independent_gt_status"
         << "PENDING_INDEPENDENT_METROLOGY_GT";
  target << "p95_p99_acceptance_status"
         << "PENDING_BATCH_METROLOGY_EVALUATION";
  target << "mask_fit_status" << row.mask_fit_status;
  target << "human_review_required" << 1;
  target << "training_enabled" << 0;
  target << "promotion_allowed" << 0;
  target.release();
  return true;
}

bool WriteAugSample(const CxGeometryAugmentationDatasetOptions &options,
                    const AugSource &source, const AugVariant &variant,
                    AugRow &row, std::string &reason) {
  const cv::Mat source_image =
      cv::imread(source.image.string(), cv::IMREAD_COLOR);
  const cv::Mat source_label =
      cv::imread(source.label.string(), cv::IMREAD_GRAYSCALE);
  if (source_image.empty() || source_label.empty() ||
      source_image.size() != source_label.size()) {
    reason = "source image or typed label cannot be decoded";
    return false;
  }

  cv::Mat image;
  cv::Mat label;
  cv::Mat latent_label;
  cv::Matx33d matrix;
  double rotation_deg = 0.0;
  if (!ApplyAugVariant(source_image, source_label, variant, image, label,
                       latent_label,
                       matrix, rotation_deg, reason))
    return false;

  const int before_pixels = cv::countNonZero(source_label);
  const int after_pixels = cv::countNonZero(label);
  if (before_pixels <= 0 || after_pixels <= 0) {
    reason = "typed label became empty";
    return false;
  }
  const double retained_ratio = std::min(
      1.0, static_cast<double>(after_pixels) / before_pixels);
  const bool structural_defect = AugVariantHasStructuralDefect(variant);
  const bool visible_extent_change =
      AugVariantCanChangeVisibleBoundaryExtent(variant);
  // Open boundaries intentionally meet image edges.  Scene rotation, scale,
  // and non-affine deformation alter their raster length or crop edge entry
  // points; this is a visibility fact, not evidence corruption.
  const double min_retained_ratio = visible_extent_change ? 0.70 : 0.985;
  if (retained_ratio < min_retained_ratio) {
    reason = "geometric augmentation clips more than 1.5 percent of the label";
    return false;
  }

  std::vector<cv::Point> points;
  cv::findNonZero(label, points);
  const cv::Rect bounds = cv::boundingRect(points);
  const cv::Moments before = cv::moments(source_label, true);
  const cv::Moments after = cv::moments(label, true);
  if (before.m00 <= 0.0 || after.m00 <= 0.0) {
    reason = "typed label moments are invalid";
    return false;
  }
  const cv::Point2f source_centroid(
      static_cast<float>(before.m10 / before.m00),
      static_cast<float>(before.m01 / before.m00));
  const cv::Point2f affine_centroid = AugPoint(matrix, source_centroid);
  const cv::Point2d centroid(after.m10 / after.m00, after.m01 / after.m00);
  const double centroid_error =
      cv::norm(cv::Point2d(affine_centroid.x, affine_centroid.y) - centroid);
  const double max_centroid_error = visible_extent_change ? 24.0 : 2.0;
  if (centroid_error > max_centroid_error) {
    reason =
        "label centroid differs from affine position truth by more than allowed";
    return false;
  }

  const std::filesystem::path final_path = options.output_dir / "dataset" /
                                           variant.split / source.directory /
                                           variant.id;
  const std::filesystem::path temporary_path =
      final_path.parent_path() / ("." + variant.id + ".partial");
  std::error_code error;
  if (std::filesystem::exists(final_path)) {
    reason = "sample output already exists";
    return false;
  }
  std::filesystem::remove_all(temporary_path, error);
  error.clear();
  std::filesystem::create_directories(temporary_path, error);
  if (error) {
    reason = "cannot create temporary sample directory";
    return false;
  }

  const std::filesystem::path image_path = temporary_path / "source_image.png";
  const std::filesystem::path label_path = temporary_path / "typed_label.png";
  const std::filesystem::path latent_label_path =
      temporary_path / "latent_boundary_map.png";
  const std::filesystem::path overlay_path =
      temporary_path / "evidence_overlay.png";
  if (!cv::imwrite(image_path.string(), image) ||
      !cv::imwrite(label_path.string(), label) ||
      !cv::imwrite(latent_label_path.string(), latent_label)) {
    std::filesystem::remove_all(temporary_path, error);
    reason = "cannot write augmented image or typed label";
    return false;
  }

  cv::Mat overlay = image.clone();
  cv::Mat red(label.size(), CV_8UC3, cv::Scalar(0, 0, 0));
  red.setTo(cv::Scalar(0, 0, 255), label);
  cv::addWeighted(overlay, 0.78, red, 0.35, 0.0, overlay);
  cv::rectangle(overlay, bounds, cv::Scalar(0, 255, 255), 2, cv::LINE_AA);
  cv::drawMarker(overlay, cv::Point(cvRound(centroid.x), cvRound(centroid.y)),
                 cv::Scalar(0, 255, 0), cv::MARKER_CROSS, 18, 2, cv::LINE_AA);
  cv::putText(overlay, variant.split + " | " + variant.id, cv::Point(14, 28),
              cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(255, 255, 255), 2,
              cv::LINE_AA);
  if (!cv::imwrite(overlay_path.string(), overlay)) {
    std::filesystem::remove_all(temporary_path, error);
    reason = "cannot write evidence overlay";
    return false;
  }

  std::filesystem::copy_file(
      source.facts, temporary_path / "source_geometry_facts.json",
      std::filesystem::copy_options::overwrite_existing, error);
  if (!error) {
    std::filesystem::copy_file(
        options.reference_index_path.parent_path() /
            "geometry_typed_contracts.json",
        temporary_path / "geometry_typed_contracts.json",
        std::filesystem::copy_options::overwrite_existing, error);
  }
  if (!error && !source.anchor_policy.empty() &&
      !source.anchor_candidates.empty()) {
    std::filesystem::copy_file(source.anchor_policy,
        temporary_path / "boundary_anchor_policy.json",
        std::filesystem::copy_options::overwrite_existing, error);
    if (!error) {
      std::filesystem::copy_file(source.anchor_candidates,
          temporary_path / "source_anchor_candidates.json",
          std::filesystem::copy_options::overwrite_existing, error);
    }
  }
  const std::string review_item =
      source.review_item + " / " + variant.review_suffix;
  if (error ||
      !WriteAugFacts(source, temporary_path / "geometry_facts.json", matrix,
                     rotation_deg, bounds, centroid) ||
      !WriteAugPosition(temporary_path / "position_annotation.json", source,
                        variant, matrix, bounds, source_centroid,
                        affine_centroid, centroid, retained_ratio,
                        centroid_error, image.cols, image.rows) ||
       !WriteAugMaskGeometryFit(temporary_path, source, label, row) ||
       !WriteAugAnchorProjection(temporary_path, source, variant, matrix) ||
       !WriteImageDrivenGeometryHeadProbe(temporary_path, source, image, label) ||
      !WriteAugTrainingTarget(temporary_path, source, variant, bounds,
                              rotation_deg, retained_ratio, centroid_error,
                              image.cols, image.rows, row) ||
      !WriteAugMetrologyTarget(temporary_path, source, variant, review_item,
                               bounds, rotation_deg, retained_ratio,
                               centroid_error, image.cols, image.rows, row) ||
      !WriteAugHashes(temporary_path, source, image_path, label_path) ||
      !WriteAugReviewAssets(temporary_path, source, variant, review_item) ||
      !WriteAugManifest(temporary_path, source, variant, review_item)) {
    std::filesystem::remove_all(temporary_path, error);
    reason = row.identifiable ? "cannot write complete Evidence metadata"
                              : "GEOMETRY_NOT_IDENTIFIABLE";
    return false;
  }

  const char *mandatory[] = {
       "source_image.png",           "typed_label.png",
      "latent_boundary_map.png",
      "geometry_facts.json",        "position_annotation.json",
      "training_target.json",       "mask_geometry_fit.json",
       "metrology_target.json",
       "image_geometry_head_probe.json",
      "geometry_typed_contracts.json",
      "source_geometry_facts.json", "source_hashes.json",
      "evidence_overlay.png",       "result_summary.json",
      "review_decision.json",       "case_manifest.json"};
  for (const char *name : mandatory) {
    if (!std::filesystem::is_regular_file(temporary_path / name)) {
      std::filesystem::remove_all(temporary_path, error);
      reason = std::string("ASSET_MISSING: ") + name;
      return false;
    }
  }
  if (!source.anchor_policy.empty() && !source.anchor_candidates.empty()) {
    for (const char *name : {"boundary_anchor_policy.json",
                             "source_anchor_candidates.json",
                             "anchor_projection.json"}) {
      if (!std::filesystem::is_regular_file(temporary_path / name)) {
        std::filesystem::remove_all(temporary_path, error);
        reason = std::string("ASSET_MISSING: ") + name;
        return false;
      }
    }
  }

  std::filesystem::rename(temporary_path, final_path, error);
  if (error) {
    std::filesystem::remove_all(temporary_path, error);
    reason = "cannot publish completed sample directory";
    return false;
  }
  row.review_item = review_item;
  row.case_path = final_path;
  return true;
}

bool WriteAugMetrologyChainArtifacts(
    const CxGeometryAugmentationDatasetOptions &options,
    const std::vector<AugRow> &rows,
    const CxGeometryAugmentationDatasetResult &result) {
  int generated_samples = 0;
  int train_samples = 0;
  int validation_samples = 0;
  int holdout_samples = 0;
  for (const AugRow &row : rows) {
    if (row.status != "GENERATED")
      continue;
    ++generated_samples;
    if (row.split == "train")
      ++train_samples;
    else if (row.split == "validation")
      ++validation_samples;
    else if (row.split == "holdout")
      ++holdout_samples;
  }

  cv::FileStorage chain((options.output_dir / "metrology_chain_manifest.json").string(),
                        cv::FileStorage::WRITE | cv::FileStorage::FORMAT_JSON);
  if (!chain.isOpened())
    return false;
  chain << "schema" << "cxvision.metrology_incremental_chain.v1";
  chain << "purpose"
        << "current_cases_validate_evidence_flow_not_real_scene_accuracy";
  chain << "primary_model_family" << "YOLOv8-n_detection";
  chain << "segmentation_model_role"
        << "optional_future_mask_geometry_extension_not_primary";
  chain << "training_execution_mode"
        << "external_incremental_training_on_existing_base_model";
  chain << "python_training_in_process" << 0;
  chain << "model_structure_policy" << "do_not_disassemble_yolo_model";
  chain << "base_model_ref_status" << "PENDING_EXTERNAL_YOLOV8N_BASE_MODEL_REF";
  chain << "incremental_model_ref_status"
        << "PENDING_EXTERNAL_YOLOV8N_INCREMENTAL_MODEL_REF";
  chain << "training_curve_ref_status" << "PENDING_EXTERNAL_TRAINING_CURVES";
  chain << "base_inference_ref_status" << "PENDING_BASE_MODEL_INFERENCE";
  chain << "incremental_inference_ref_status"
        << "PENDING_INCREMENTAL_MODEL_INFERENCE";
  chain << "metrology_target_index_ref" << "metrology_target_index.json";
  chain << "clear_degraded_pair_index_ref" << "metrology_pair_index.json";
  chain << "assignment_contract_ref" << "metrology_assignment_contract.json";
  chain << "instance_loss_contract_ref"
        << "metrology_instance_loss_contract.json";
  chain << "model_output_contract_ref"
        << "metrology_model_output_contract.json";
  chain << "physical_calibration_contract_ref"
        << "metrology_physical_calibration_contract.json";
  chain << "independent_gt_contract_ref"
        << "metrology_independent_gt_contract.json";
  chain << "acceptance_gate_ref" << "metrology_acceptance_gate.json";
  chain << "generated_sample_count" << generated_samples;
  chain << "train_sample_count" << train_samples;
  chain << "validation_sample_count" << validation_samples;
  chain << "holdout_sample_count" << holdout_samples;
  chain << "human_review_required" << 1;
  chain << "training_enabled" << 0;
  chain << "promotion_allowed" << 0;
  chain << "evidence_chain"
        << "[" << "normalized_metrology_target"
        << "clear_degraded_pair_expansion" << "instance_assignment"
        << "per_instance_geometry_loss" << "model_output_and_uncertainty"
        << "pixel_to_physical_calibration" << "independent_metrology_gt_compare"
        << "p95_p99_failure_rate_acceptance" << "]";
  chain.release();

  cv::FileStorage target_index(
      (options.output_dir / "metrology_target_index.json").string(),
      cv::FileStorage::WRITE | cv::FileStorage::FORMAT_JSON);
  if (!target_index.isOpened())
    return false;
  target_index << "schema" << "cxvision.metrology_target_index.v1";
  target_index << "coordinate_system"
               << "pixel_center_origin_top_left_x_right_y_down";
  target_index << "primary_model_family" << "YOLOv8-n_detection";
  target_index << "target_source" << "per_sample_metrology_target_json";
  target_index << "samples" << "[";
  for (const AugRow &row : rows) {
    if (row.status != "GENERATED")
      continue;
    const std::string relative =
        row.case_path.lexically_relative(options.output_dir).generic_string();
    target_index << "{";
    target_index << "review_item" << row.review_item;
    target_index << "split" << row.split;
    target_index << "geometry_type" << row.geometry_type;
    target_index << "case_path" << relative;
    target_index << "metrology_target_ref" << (relative + "/metrology_target.json");
    target_index << "training_eligible"
                 << (row.split == "train" && row.identifiable ? 1 : 0);
    target_index << "assignment_status" << "PENDING_YOLOV8N_INFERENCE_EVIDENCE";
    target_index << "geometry_claim_status"
                 << "DETECTION_ONLY_GEOMETRY_UNCONFIRMED";
    target_index << "}";
  }
  target_index << "]";
  target_index.release();

  cv::FileStorage pair_index((options.output_dir / "metrology_pair_index.json").string(),
                             cv::FileStorage::WRITE | cv::FileStorage::FORMAT_JSON);
  if (!pair_index.isOpened())
    return false;
  pair_index << "schema" << "cxvision.metrology_clear_degraded_pair_index.v1";
  pair_index << "pairing_policy"
             << "current_augmented_cases_are_flow_samples_real_scene_pairs_pending";
  pair_index << "required_real_scene_pair_fields"
             << "[" << "pair_id" << "clear_image_ref" << "degraded_image_ref"
             << "same_instance_id" << "degradation_profile" << "split" << "]";
  pair_index << "samples" << "[";
  for (const AugRow &row : rows) {
    if (row.status != "GENERATED")
      continue;
    const std::string relative =
        row.case_path.lexically_relative(options.output_dir).generic_string();
    pair_index << "{";
    pair_index << "review_item" << row.review_item;
    pair_index << "split" << row.split;
    pair_index << "variant_id" << row.variant_id;
    pair_index << "case_path" << relative;
    pair_index << "current_sample_image_ref" << (relative + "/source_image.png");
    pair_index << "clear_reference_status"
               << "PENDING_REAL_SCENE_CLEAR_IMAGE_ASSET";
    pair_index << "pair_status" << "PENDING_HUMAN_PAIR_REVIEW";
    pair_index << "}";
  }
  pair_index << "]";
  pair_index.release();

  cv::FileStorage assignment(
      (options.output_dir / "metrology_assignment_contract.json").string(),
      cv::FileStorage::WRITE | cv::FileStorage::FORMAT_JSON);
  if (!assignment.isOpened())
    return false;
  assignment << "schema" << "cxvision.metrology_assignment_contract.v1";
  assignment << "primary_model_family" << "YOLOv8-n_detection";
  assignment << "required_inputs"
             << "[" << "metrology_target" << "base_model_detection"
             << "incremental_model_detection" << "]";
  assignment << "assignment_fields"
             << "[" << "gt_instance_id" << "detection_id" << "class_match"
             << "iou" << "center_distance_px" << "assignment_state" << "]";
  assignment << "assignment_states"
             << "[" << "TP" << "FP" << "FN" << "AMBIGUOUS" << "]";
  assignment << "status" << "PENDING_YOLOV8N_INFERENCE_EVIDENCE";
  assignment << "model_executed" << 0;
  assignment << "geometry_claim_status"
             << "DETECTION_ONLY_GEOMETRY_UNCONFIRMED";
  assignment.release();

  cv::FileStorage loss(
      (options.output_dir / "metrology_instance_loss_contract.json").string(),
      cv::FileStorage::WRITE | cv::FileStorage::FORMAT_JSON);
  if (!loss.isOpened())
    return false;
  loss << "schema" << "cxvision.metrology_instance_loss_contract.v1";
  loss << "target_semantics" << "independent_loss_per_assigned_instance";
  loss << "not_allowed" << "share_one_geometry_parameter_across_same_class";
  loss << "bbox_loss_fields"
       << "[" << "center_error_px" << "width_error_px" << "height_error_px"
       << "iou_loss" << "]";
  loss << "optional_geometry_loss_fields"
       << "[" << "fit_residual_px" << "axis_error_px" << "angle_error_deg"
       << "endpoint_error_px" << "]";
  loss << "aggregate_selection"
       << "validation_robust_loss_then_p95_p99_then_failure_rate";
  loss << "status" << "PENDING_MODEL_PREDICTION_ASSIGNMENT";
  loss.release();

  cv::FileStorage model_output(
      (options.output_dir / "metrology_model_output_contract.json").string(),
      cv::FileStorage::WRITE | cv::FileStorage::FORMAT_JSON);
  if (!model_output.isOpened())
    return false;
  model_output << "schema" << "cxvision.metrology_model_output_contract.v1";
  model_output << "primary_model_family" << "YOLOv8-n_detection";
  model_output << "required_output_fields"
               << "[" << "model_role" << "model_ref" << "class_id"
               << "class_name" << "confidence" << "bbox_xywh_px"
               << "uncertainty" << "source_epoch_or_weight" << "]";
  model_output << "model_roles" << "[" << "base" << "incremental" << "]";
  model_output << "uncertainty_policy"
               << "confidence_bucket_first_then_calibrated_uncertainty";
  model_output << "status" << "PENDING_YOLOV8N_OUTPUT_SCORE_AND_CALIBRATION";
  model_output.release();

  cv::FileStorage calibration(
      (options.output_dir / "metrology_physical_calibration_contract.json").string(),
      cv::FileStorage::WRITE | cv::FileStorage::FORMAT_JSON);
  if (!calibration.isOpened())
    return false;
  calibration << "schema" << "cxvision.metrology_physical_calibration_contract.v1";
  calibration << "pixel_coordinate_system"
              << "pixel_center_origin_top_left_x_right_y_down";
  calibration << "required_fields"
              << "[" << "calibration_ref" << "unit" << "scale"
              << "transform" << "valid_roi" << "calibration_status" << "]";
  calibration << "output_fields"
              << "[" << "bbox_physical" << "center_physical"
              << "size_physical" << "error_physical" << "]";
  calibration << "status" << "PENDING_PIXEL_TO_PHYSICAL_CALIBRATION";
  calibration.release();

  cv::FileStorage gt((options.output_dir / "metrology_independent_gt_contract.json").string(),
                     cv::FileStorage::WRITE | cv::FileStorage::FORMAT_JSON);
  if (!gt.isOpened())
    return false;
  gt << "schema" << "cxvision.metrology_independent_gt_contract.v1";
  gt << "gt_policy" << "independent_metrology_gt_not_derived_from_same_annotation";
  gt << "required_fields"
     << "[" << "gt_source" << "measurement_id" << "physical_value"
     << "unit" << "uncertainty" << "review_decision" << "]";
  gt << "status" << "PENDING_INDEPENDENT_METROLOGY_GT";
  gt.release();

  cv::FileStorage acceptance(
      (options.output_dir / "metrology_acceptance_gate.json").string(),
      cv::FileStorage::WRITE | cv::FileStorage::FORMAT_JSON);
  if (!acceptance.isOpened())
    return false;
  acceptance << "schema" << "cxvision.metrology_acceptance_gate.v1";
  acceptance << "primary_model_family" << "YOLOv8-n_detection";
  acceptance << "required_statistics"
             << "[" << "p50_error" << "p95_error" << "p99_error"
             << "failure_rate" << "false_negative_rate" << "false_positive_rate"
             << "old_scene_regression_rate" << "new_scene_improvement_rate" << "]";
  acceptance << "p95_status" << "PENDING_MODEL_AND_GT_EVIDENCE";
  acceptance << "p99_status" << "PENDING_MODEL_AND_GT_EVIDENCE";
  acceptance << "failure_rate_status" << "PENDING_MODEL_AND_GT_EVIDENCE";
  acceptance << "human_review_status" << "PENDING_HUMAN_REVIEW";
  acceptance << "training_enabled" << 0;
  acceptance << "promotion_allowed" << 0;
  acceptance << "allowed_claim" << "metrology_evidence_chain_structure_ready";
  acceptance << "forbidden_claim"
             << "incremental_training_effectiveness_or_production_accuracy";
  acceptance.release();
  return true;
}

bool WriteAugRegressionGate(
    const CxGeometryAugmentationDatasetOptions &options,
    const std::vector<AugRow> &rows,
    const CxGeometryAugmentationDatasetResult &result) {
  int fit_within_tolerance_samples = 0;
  int fit_outside_tolerance_samples = 0;
  int fit_failed_samples = 0;
  int pending_fitter_samples = 0;
  for (const AugRow &row : rows) {
    if (row.status != "GENERATED")
      continue;
    if (row.mask_fit_status == "PENDING_FITTER_BINDING") {
      ++pending_fitter_samples;
    } else if (row.mask_fit_status == "FIT_WITHIN_TOLERANCE") {
      ++fit_within_tolerance_samples;
    } else if (row.mask_fit_status == "FIT_OUTSIDE_TOLERANCE") {
      ++fit_outside_tolerance_samples;
    } else {
      ++fit_failed_samples;
    }
  }

  const int fitted_mask_samples =
      fit_within_tolerance_samples + fit_outside_tolerance_samples;
  const bool mask_contract_ok =
      fitted_mask_samples > 0 && fit_outside_tolerance_samples == 0 &&
      fit_failed_samples == 0;

  cv::FileStorage gate(
      (options.output_dir / "geometry_regression_gate.json").string(),
      cv::FileStorage::WRITE | cv::FileStorage::FORMAT_JSON);
  if (!gate.isOpened())
    return false;
  gate << "schema" << "cxvision.geometry_regression_gate.v1";
  gate << "step_1_annotation_schema" << "READY";
  gate << "step_2_per_instance_targets"
       << (result.rejected_sample_count == 0 ? "READY"
                                            : "REJECTED_SAMPLES_PRESENT");
  gate << "step_3_stratified_index_ref" << "geometry_training_index.json";
  gate << "step_4_typed_mask_fit_sample_count" << fitted_mask_samples;
  gate << "step_4_fit_within_tolerance_sample_count"
       << fit_within_tolerance_samples;
  gate << "step_4_fit_outside_tolerance_sample_count"
       << fit_outside_tolerance_samples;
  gate << "step_4_fit_failed_sample_count" << fit_failed_samples;
  gate << "step_4_pending_fitter_sample_count" << pending_fitter_samples;
  gate << "step_4_typed_mask_pipeline_status"
       << (fitted_mask_samples > 0 ? "SEG_MASK_GEOMETRY_PIPELINE_COMPLETE"
                                   : "SEG_MASK_GEOMETRY_PIPELINE_PENDING");
  gate << "step_4_typed_mask_contract_status"
       << (mask_contract_ok ? "SEG_MASK_GEOMETRY_CONTRACT_PASS"
                            : "SEG_MASK_GEOMETRY_CONTRACT_FAIL");
  gate << "step_4_yolov8_seg_model_status"
       << "OPTIONAL_SEGMENTATION_EXTENSION_PENDING_NOT_PRIMARY";
  gate << "primary_model_family" << "YOLOv8-n_detection";
  gate << "model_structure_policy" << "reuse_base_yolov8n_without_disassembly";
  gate << "training_execution_mode"
       << "external_incremental_training_evidence_only";
  gate << "python_training_in_process" << 0;
  gate << "metrology_chain_status"
       << "METROLOGY_EVIDENCE_CHAIN_READY_FOR_EXTERNAL_MODEL_RESULTS";
  gate << "metrology_chain_ref" << "metrology_chain_manifest.json";
  gate << "metrology_target_index_ref" << "metrology_target_index.json";
  gate << "metrology_pair_index_ref" << "metrology_pair_index.json";
  gate << "metrology_assignment_contract_ref"
       << "metrology_assignment_contract.json";
  gate << "metrology_instance_loss_contract_ref"
       << "metrology_instance_loss_contract.json";
  gate << "metrology_model_output_contract_ref"
       << "metrology_model_output_contract.json";
  gate << "physical_calibration_status"
       << "PENDING_PIXEL_TO_PHYSICAL_CALIBRATION";
  gate << "independent_gt_status" << "PENDING_INDEPENDENT_METROLOGY_GT";
  gate << "p95_p99_failure_rate_status"
       << "PENDING_MODEL_AND_GT_EVIDENCE";
  gate << "yolov8n_base_inference_status" << "PENDING_BASE_MODEL_INFERENCE";
  gate << "yolov8n_incremental_inference_status"
       << "PENDING_INCREMENTAL_MODEL_INFERENCE";
  gate << "l1_status"
       << (result.generated_sample_count > 0 &&
                   result.rejected_sample_count == 0
               ? "L1_EXECUTION_PASS"
               : "L1_EXECUTION_FAIL");
  gate << "l2_status" << "L2_EXECUTION_COMPLETE";
  gate << "l2_contract_status"
       << (result.rejected_sample_count == 0 && mask_contract_ok
               ? "L2_CONTRACT_PASS"
               : "L2_CONTRACT_FAIL");
  gate << "l3_status" << "L3_PENDING_HUMAN_REVIEW";
  gate << "human_review_status" << "PENDING_HUMAN_REVIEW";
  gate << "promotion_allowed" << 0;
  gate << "training_enabled" << 0;
  gate << "allowed_claim"
       << "controlled_annotation_augmentation_and_typed_mask_fit_execution";
  gate << "forbidden_claim"
       << "production_model_accuracy_or_human_acceptance";
  gate.release();

  cv::FileStorage review(
      (options.output_dir / "human_review.json").string(),
      cv::FileStorage::WRITE | cv::FileStorage::FORMAT_JSON);
  if (!review.isOpened())
    return false;
  review << "schema" << "cxvision.geometry_dataset_human_review.v1";
  review << "decision" << "PENDING_HUMAN_REVIEW";
  review << "required_review_scope"
         << "[" << "per_instance_geometry_target"
         << "stratification_coverage" << "mask_geometry_overlay"
         << "validation_split_independence" << "normalized_metrology_target"
         << "clear_degraded_pairing" << "instance_assignment"
         << "per_instance_geometry_loss" << "model_output_uncertainty"
         << "physical_calibration" << "independent_metrology_gt"
         << "p95_p99_failure_rate_acceptance" << "]";
  review << "training_enabled" << 0;
  review << "promotion_allowed" << 0;
  review.release();
  return true;
}

bool WriteAugGeometryContracts(
    const CxGeometryAugmentationDatasetOptions &options) {

  std::error_code contract_error;
  std::filesystem::copy_file(
      options.reference_index_path.parent_path() /
          "geometry_typed_contracts.json",
      options.output_dir / "geometry_typed_contracts.json",
      std::filesystem::copy_options::overwrite_existing, contract_error);
  if (contract_error)
    return false;
  cv::FileStorage schema(
      (options.output_dir / "geometry_annotation_schema.json").string(),
      cv::FileStorage::WRITE | cv::FileStorage::FORMAT_JSON);
  if (!schema.isOpened())
    return false;
  schema << "schema" << "cxvision.geometry_annotation_schema.v1";
  schema << "coordinate_system"
         << "pixel_center_origin_top_left_x_right_y_down";
  schema << "typed_contracts_ref"
         << "geometry_typed_contracts.json";
  schema << "required_instance_fields"
         << "[" << "instance_id" << "geometry_type" << "geometry_parameters"
         << "visibility_ratio" << "identifiable" << "rejection_reason"
         << "tolerance_policy_ref" << "]";
  schema << "best_solution_semantics"
         << "solve_geometry_parameters_independently_for_each_annotation";
  schema << "batch_semantics"
         << "preserve_each_instance_target_and_optimize_robust_aggregate_loss";
  schema << "reject_when"
         << "[" << "required_asset_missing" << "empty_typed_label"
         << "visibility_below_policy" << "fit_not_identifiable"
         << "duplicate_internal_case_id" << "]";
  schema << "training_enabled" << 0;
  schema << "human_review_required" << 1;
  schema.release();

  cv::FileStorage contract(
      (options.output_dir / "geometry_head_training_contract.json").string(),
      cv::FileStorage::WRITE | cv::FileStorage::FORMAT_JSON);
  if (!contract.isOpened())
    return false;
  contract << "schema" << "cxvision.geometry_head_training_contract.v1";
  contract << "binding_status" << "PENDING_GEOMETRY_HEAD_BINDING";
  contract << "detector_only_geometry_claim_status"
           << "DETECTION_ONLY_GEOMETRY_UNCONFIRMED";
  contract << "forbid_geometry_pass_without_runtime_receipt" << 1;
  contract << "input_feature_source"
           << "segmentation_backbone_or_mask_feature_map";
  contract << "teacher_signal"
           << "accepted_per_instance_geometry_facts";
  contract << "teacher_target_index_ref" << "geometry_training_index.json";
  contract << "parameterization_ref" << "geometry_typed_contracts.json";
  contract << "loss_terms"
           << "[" << "type_classification" << "parameter_regression"
           << "mask_contour_consistency" << "fit_residual"
           << "uncertainty_calibration" << "]";
  contract << "target_weighting"
           << "visibility_and_identifiability_weighted_without_class_name_branches";
  contract << "aggregate_selection"
           << "validation_robust_loss_then_tail_error_then_failure_rate";
  contract << "independent_validation"
           << "validation_split_never_training_eligible";
  contract << "required_promotion_gates"
           << "[" << "L1_HUMAN_ACCEPTED" << "L2_HUMAN_ACCEPTED"
           << "L3_HUMAN_ACCEPTED" << "YOLOV8_SEG_MODEL_BINDING_COMPLETE"
           << "INDEPENDENT_HOLDOUT_PASS" << "]";
  contract << "training_enabled" << 0;
  contract << "promotion_allowed" << 0;
  contract << "model_executed" << 0;
  contract.release();
  return true;
}

bool WriteAugReports(const CxGeometryAugmentationDatasetOptions &options,
                     const std::vector<AugRow> &rows,
                     CxGeometryAugmentationDatasetResult &result) {
  result.dataset_manifest_path = options.output_dir / "dataset_manifest.json";
  result.report_json_path = options.output_dir / "augmentation_report.json";
  result.report_markdown_path = options.output_dir / "augmentation_report.md";


  if (!WriteAugTrainingIndex(options, rows) ||
      !WriteAugGeometryContracts(options) ||
      !WriteAugMetrologyChainArtifacts(options, rows, result) ||
      !WriteAugRegressionGate(options, rows, result))
    return false;

  cv::FileStorage manifest(result.dataset_manifest_path.string(),
                           cv::FileStorage::WRITE |
                               cv::FileStorage::FORMAT_JSON);
  if (!manifest.isOpened())
    return false;
  manifest << "schema"
           << "cxvision.geometry_augmentation_dataset.v1";
  manifest << "source_index" << options.reference_index_path.generic_string();
  manifest << "augmentation_plan"
           << options.augmentation_plan_path.generic_string();
  manifest << "training_enabled" << 0;
  manifest << "human_review_required" << 1;

  manifest << "annotation_schema_ref" << "geometry_annotation_schema.json";
  manifest << "training_index_ref" << "geometry_training_index.json";
  manifest << "regression_gate_ref" << "geometry_regression_gate.json";
  manifest << "geometry_head_contract_ref"
           << "geometry_head_training_contract.json";
  manifest << "human_review_ref" << "human_review.json";
  manifest << "primary_model_family" << "YOLOv8-n_detection";
  manifest << "geometry_claim_status"
           << "DETECTION_ONLY_GEOMETRY_UNCONFIRMED";
  manifest << "training_execution_mode"
           << "external_incremental_training_evidence_only";
  manifest << "python_training_in_process" << 0;
  manifest << "metrology_chain_ref" << "metrology_chain_manifest.json";
  manifest << "metrology_target_index_ref" << "metrology_target_index.json";
  manifest << "metrology_pair_index_ref" << "metrology_pair_index.json";
  manifest << "metrology_acceptance_gate_ref" << "metrology_acceptance_gate.json";
  manifest << "source_case_count" << result.source_case_count;
  manifest << "variant_count" << result.variant_count;
  manifest << "generated_sample_count" << result.generated_sample_count;
  manifest << "rejected_sample_count" << result.rejected_sample_count;
  manifest << "train_sample_count" << result.train_sample_count;
  manifest << "validation_sample_count" << result.validation_sample_count;
  manifest << "holdout_sample_count" << result.holdout_sample_count;
  manifest << "samples"
           << "[";
  for (const AugRow &row : rows) {
    if (row.status != "GENERATED")
      continue;
    manifest << "{";
    manifest << "review_item" << row.review_item;
    manifest << "split" << row.split;
    manifest << "geometry_type" << row.geometry_type;
    manifest << "variant_id" << row.variant_id;
    manifest << "case_path"
             << row.case_path.lexically_relative(options.output_dir)
                    .generic_string();
    manifest << "training_target_ref" << "training_target.json";
    manifest << "mask_geometry_fit_ref" << "mask_geometry_fit.json";
    manifest << "metrology_target_ref" << "metrology_target.json";
    manifest << "model_assignment_status"
             << "PENDING_YOLOV8N_INFERENCE_EVIDENCE";
    manifest << "geometry_claim_status"
             << "DETECTION_ONLY_GEOMETRY_UNCONFIRMED";
    manifest << "size_bucket" << row.size_bucket;
    manifest << "angle_bucket" << row.angle_bucket;
    manifest << "difficulty_bucket" << row.difficulty_bucket;
    manifest << "degradation_bucket" << row.degradation_bucket;
    manifest << "mask_fit_status" << row.mask_fit_status;
    manifest << "visibility_ratio" << row.visible_ratio;
    manifest << "identifiable" << (row.identifiable ? 1 : 0);
    manifest << "training_eligible"
             << (row.split == "train" && row.identifiable ? 1 : 0);
    manifest << "training_enabled" << 0;
    manifest << "}";
  }
  manifest << "]";
  manifest.release();

  cv::FileStorage report(result.report_json_path.string(),
                         cv::FileStorage::WRITE | cv::FileStorage::FORMAT_JSON);
  if (!report.isOpened())
    return false;
  report << "schema"
         << "cxvision.geometry_augmentation_report.v1";
  report << "status" << result.status;
  report << "reason" << result.reason;
  report << "source_case_count" << result.source_case_count;
  report << "variant_count" << result.variant_count;
  report << "generated_sample_count" << result.generated_sample_count;
  report << "rejected_sample_count" << result.rejected_sample_count;
  report << "train_sample_count" << result.train_sample_count;
  report << "validation_sample_count" << result.validation_sample_count;
  report << "holdout_sample_count" << result.holdout_sample_count;
  report << "rows"
         << "[";
  for (const AugRow &row : rows) {
    report << "{";
    report << "review_item" << row.review_item;
    report << "split" << row.split;
    report << "geometry_type" << row.geometry_type;
    report << "variant_id" << row.variant_id;
    report << "degradation_bucket" << row.degradation_bucket;
    report << "status" << row.status;
    report << "reason" << row.reason;
    report << "case_path" << row.case_path.generic_string();
    report << "}";
  }
  report << "]";
  report.release();

  std::ofstream markdown(result.report_markdown_path);
  if (!markdown)
    return false;
  markdown << "# Geometry Augmentation Dataset\n\n";
  markdown << "- Status: " << result.status << "\n";
  markdown << "- Source cases: " << result.source_case_count << "\n";
  markdown << "- Variants: " << result.variant_count << "\n";
  markdown << "- Generated: " << result.generated_sample_count << "\n";
  markdown << "- Rejected: " << result.rejected_sample_count << "\n";
  markdown << "- Train: " << result.train_sample_count << "\n";
  markdown << "- Validation: " << result.validation_sample_count << "\n";
  markdown << "- Holdout: " << result.holdout_sample_count << "\n";
  markdown << "- Training enabled: false\n";
  markdown << "- Human review: required\n\n";
  markdown << "| Evidence item | Split | Geometry | Variant | Status |\n";
  markdown << "|---|---|---|---|---|\n";
  for (const AugRow &row : rows) {
    markdown << "| " << row.review_item << " | " << row.split << " | "
             << row.geometry_type << " | " << row.variant_id << " | "
             << row.status << " |\n";
  }
  return markdown.good();
}

}


bool RunCxGeometryAugmentationDataset(
    const CxGeometryAugmentationDatasetOptions &options,
    CxGeometryAugmentationDatasetResult &result, std::string &reason) {
  result = {};
  reason.clear();
  if (!std::filesystem::is_regular_file(options.reference_index_path) ||
      !std::filesystem::is_regular_file(options.augmentation_plan_path) ||
      options.output_dir.empty()) {
    result.status = "ASSET_PREFLIGHT_FAIL";
    result.reason =
        "reference index, augmentation plan, or output directory is invalid";
    reason = result.reason;
    return false;
  }

  std::vector<AugSource> sources;
  std::vector<AugVariant> variants;
  if (!ReadAugSources(options.reference_index_path, sources, reason) ||
      !ReadAugPlan(options.augmentation_plan_path, variants, reason)) {
    result.status = "ASSET_PREFLIGHT_FAIL";
    result.reason = reason;
    return false;
  }

  if (options.require_source_disjoint_validation) {
    std::set<std::string> trainClasses;
    std::set<std::string> validationClasses;
    std::set<std::string> holdoutClasses;
    for (const AugSource &source : sources) {
      if (source.source_split != "train" &&
          source.source_split != "validation" &&
          source.source_split != "holdout") {
        result.status = "SOURCE_SPLIT_DECLARATION_REQUIRED";
        result.reason = "each source case must declare source_split=train, "
                        "source_split=validation, or source_split=holdout "
                        "before independent validation";
        reason = result.reason;
        return false;
      }
      if (source.source_split == "train")
        trainClasses.insert(source.geometry_type);
      else if (source.source_split == "validation")
        validationClasses.insert(source.geometry_type);
      else
        holdoutClasses.insert(source.geometry_type);
    }
    if (trainClasses.empty() || validationClasses.empty() ||
        holdoutClasses.empty() || trainClasses != validationClasses ||
        trainClasses != holdoutClasses) {
      result.status = "SOURCE_SPLIT_CLASS_COVERAGE_FAIL";
      result.reason = "train, validation, and holdout source partitions must "
                      "be non-empty and have matching class coverage";
      reason = result.reason;
      return false;
    }
  }

  std::error_code error;
  if (std::filesystem::exists(options.output_dir)) {
    result.status = "OUTPUT_EXISTS";
    result.reason = "output directory already exists";
    reason = result.reason;
    return false;
  }
  std::filesystem::create_directories(options.output_dir, error);
  if (error) {
    result.status = "OUTPUT_CREATE_FAILED";
    result.reason = "cannot create output directory";
    reason = result.reason;
    return false;
  }

  result.source_case_count = static_cast<int>(sources.size());
  result.variant_count = static_cast<int>(variants.size());
  std::vector<AugRow> rows;
  rows.reserve(sources.size() * variants.size());
  int expected = 0;
  for (const AugSource &source : sources) {
    for (const AugVariant &variant : variants) {
      if (options.require_source_disjoint_validation &&
          source.source_split != variant.split)
        continue;
      ++expected;
      AugRow row;
      row.review_item = source.review_item + " / " + variant.review_suffix;
      row.split = variant.split;
      row.geometry_type = source.geometry_type;
      row.variant_id = variant.id;
      std::string sample_reason;
      if (WriteAugSample(options, source, variant, row, sample_reason)) {
        row.status = "GENERATED";
        ++result.generated_sample_count;
        if (variant.split == "train")
          ++result.train_sample_count;
        else if (variant.split == "validation")
          ++result.validation_sample_count;
        else if (variant.split == "holdout")
          ++result.holdout_sample_count;
      } else {
        row.status = "REJECTED";
        row.reason = sample_reason;
        ++result.rejected_sample_count;
      }
      rows.push_back(std::move(row));
    }
  }

  result.complete = result.rejected_sample_count == 0 &&
                    result.generated_sample_count == expected;
  result.status = result.complete ? "DATASET_GENERATION_COMPLETE"
                                  : "DATASET_GENERATION_PARTIAL";
  result.reason =
      result.complete ? "" : "one or more augmentation samples were rejected";

  if (!WriteAugReports(options, rows, result)) {
    result.complete = false;
    result.status = "REPORT_WRITE_FAILED";
    result.reason = "cannot write augmentation dataset reports";
  }
  if (result.complete && !RegisterEvidenceCaseRoot(options.output_dir)) {
    result.complete = false;
    result.status = "EVIDENCE_REGISTRATION_FAILED";
    result.reason = "cannot register augmentation dataset under RUN_ROOT";
  }
  reason = result.reason;
  return result.complete;
}
