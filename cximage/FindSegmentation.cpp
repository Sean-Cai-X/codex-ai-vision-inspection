#include "CircleShape.h"
#include "EllipseShape.h"

#include "FindSegmentation.h"
#include "FindSegmentationEdgeSamBackend.h"
#include "FindSegmentationOpenCvSmokeBackend.h"
#include "ImageAnnotationLayer.h"
#include "PolylineShape.h"
#include "RectShape.h"

#include <exception>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>

namespace {
std::string EscapeSegmentationJson(const std::string& value) {
  std::string escaped;
  escaped.reserve(value.size());
  for (const char ch : value) {
    if (ch == '\\' || ch == '"')
      escaped.push_back('\\');
    if (ch == '\n')
      escaped += "\\n";
    else if (ch != '\n')
      escaped.push_back(ch);
  }
  return escaped;
}

struct ContourObbOverlapMetrics {
  double contour_raster_mask_obb_iou = 0.0;
  double contour_raster_mask_aabb_iou = 0.0;
  double contour_area_px = 0.0;
  double obb_area_px = 0.0;
  double aabb_area_px = 0.0;
};

ContourObbOverlapMetrics MeasureContourObbOverlap(
    const std::vector<cv::Point>& contour,
    const CxGeometryPrimitiveHypothesis& box) {
  ContourObbOverlapMetrics metrics;
  if (contour.size() < 3 || box.ordered_points.size() != 4)
    return metrics;

  std::vector<cv::Point> obb;
  obb.reserve(box.ordered_points.size());
  for (const cv::Point2d& point : box.ordered_points)
    obb.emplace_back(cvRound(point.x), cvRound(point.y));
  const cv::Rect contour_bounds = cv::boundingRect(contour);
  const cv::Rect obb_bounds = cv::boundingRect(obb);
  const cv::Rect bounds = contour_bounds | obb_bounds;
  if (bounds.width <= 0 || bounds.height <= 0)
    return metrics;

  // Rasterize only the local union bounds; this is exact for the emitted
  // contour representation and avoids treating a bounding-area ratio as IoU.
  std::vector<cv::Point> local_contour;
  std::vector<cv::Point> local_obb;
  local_contour.reserve(contour.size());
  local_obb.reserve(obb.size());
  for (const cv::Point& point : contour)
    local_contour.emplace_back(point.x - bounds.x, point.y - bounds.y);
  for (const cv::Point& point : obb)
    local_obb.emplace_back(point.x - bounds.x, point.y - bounds.y);
  cv::Mat contour_mask = cv::Mat::zeros(bounds.height, bounds.width, CV_8U);
  cv::Mat obb_mask = cv::Mat::zeros(bounds.height, bounds.width, CV_8U);
  const std::vector<std::vector<cv::Point>> contour_polygons{local_contour};
  cv::fillPoly(contour_mask, contour_polygons, cv::Scalar(255));
  cv::fillConvexPoly(obb_mask, local_obb, cv::Scalar(255));
  cv::Mat intersection_mask;
  cv::Mat union_mask;
  cv::bitwise_and(contour_mask, obb_mask, intersection_mask);
  cv::bitwise_or(contour_mask, obb_mask, union_mask);
  const double intersection = static_cast<double>(cv::countNonZero(intersection_mask));
  const double union_count = static_cast<double>(cv::countNonZero(union_mask));
  metrics.contour_raster_mask_obb_iou = union_count > 0.0 ? intersection / union_count : 0.0;

  cv::Mat aabb_mask = cv::Mat::zeros(bounds.height, bounds.width, CV_8U);
  const cv::Rect local_aabb(contour_bounds.x - bounds.x, contour_bounds.y - bounds.y,
                            contour_bounds.width, contour_bounds.height);
  cv::rectangle(aabb_mask, local_aabb, cv::Scalar(255), cv::FILLED);
  cv::bitwise_and(contour_mask, aabb_mask, intersection_mask);
  cv::bitwise_or(contour_mask, aabb_mask, union_mask);
  const double aabb_intersection = static_cast<double>(cv::countNonZero(intersection_mask));
  const double aabb_union = static_cast<double>(cv::countNonZero(union_mask));
  metrics.contour_raster_mask_aabb_iou =
      aabb_union > 0.0 ? aabb_intersection / aabb_union : 0.0;
  metrics.contour_area_px = static_cast<double>(cv::countNonZero(contour_mask));
  metrics.obb_area_px = static_cast<double>(cv::countNonZero(obb_mask));
  metrics.aabb_area_px = static_cast<double>(cv::countNonZero(aabb_mask));
  return metrics;
}

bool WriteMaskToObbEvidence(const FindSegmentationResult& result,
                            std::string& evidence_ref,
                            std::string& conclusion,
                            std::string& reason) {
  evidence_ref.clear();
  conclusion.clear();
  if (result.output_root.empty()) {
    reason = "oriented-box evidence requires a case-local runtime output root";
    return false;
  }

  const std::filesystem::path evidence_path =
      std::filesystem::path(result.output_root) / "oriented_boxes.json";
  std::ofstream file(evidence_path);
  if (!file) {
    reason = "cannot write oriented-box evidence: " + evidence_path.string();
    return false;
  }

  CxSegmentationGeometryFitOptions options;
  options.geometry_type = "oriented_box";
  options.tolerance_policy_ref = result.postprocess_profile.empty()
      ? "mask_min_area_rect_default_v1"
      : result.postprocess_profile;
  bool any_complete = false;
  file << std::setprecision(12);
  file << "{\n"
       << "  \"schema\": \"cx.yolov8.seg.mask_obb.v2\",\n"
       << "  \"algorithm\": \"mask_to_min_area_rect\",\n"
       << "  \"angle_source\": \"postprocess_not_native_angle_regression\",\n"
       << "  \"canonical_contract\": {\"angle_range_deg\": \"[-90,90)\", "
          "\"width_greater_or_equal_height\": true},\n"
       << "  \"instances\": [\n";
  for (std::size_t index = 0; index < result.contours.size(); ++index) {
    const FindSegmentationContour& contour = result.contours[index];
    CxSegmentationGeometryFitResult fit;
    const auto fit_started = std::chrono::steady_clock::now();
    FitCxSegmentationContourGeometry(contour.points, options, fit);
    const double fit_elapsed_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - fit_started).count();
    if (index > 0)
      file << ",\n";
    const std::string instance_id = contour.source_instance_id.empty()
        ? "contour_index_" + std::to_string(index)
        : contour.source_instance_id;
    const std::string& mask_ref = contour.source_mask_ref.empty()
        ? result.mask_ref : contour.source_mask_ref;
    const std::string& contour_ref = contour.source_contour_ref.empty()
        ? result.contour_ref : contour.source_contour_ref;
    file << "    {\"source_instance_id\": \""
         << EscapeSegmentationJson(instance_id) << "\""
         << ", \"source_mask_ref\": \"" << EscapeSegmentationJson(mask_ref)
         << "\", \"source_contour_ref\": \""
         << EscapeSegmentationJson(contour_ref) << "\""
         << ", \"contour_point_count\": " << contour.points.size()
         << ", \"fit_status\": \"" << EscapeSegmentationJson(fit.status)
         << "\"";
    if (fit.complete) {
      any_complete = true;
      const CxGeometryPrimitiveHypothesis& box = fit.hypothesis;
      const ContourObbOverlapMetrics overlap =
          MeasureContourObbOverlap(contour.points, box);
      file << ", \"center_x\": " << box.center.x
           << ", \"center_y\": " << box.center.y
           << ", \"width\": " << box.axes.width
           << ", \"height\": " << box.axes.height
           << ", \"canonical_angle_deg\": " << box.angle_deg
           << ", \"fit_residual_px\": " << box.classical_fit_residual_px
           << ", \"support\": " << box.support
           << ", \"postprocess_elapsed_ms\": " << fit_elapsed_ms
           << ", \"contour_raster_mask_obb_iou\": "
           << overlap.contour_raster_mask_obb_iou
           << ", \"contour_raster_mask_aabb_iou\": "
           << overlap.contour_raster_mask_aabb_iou
           << ", \"contour_raster_area_px\": " << overlap.contour_area_px
           << ", \"obb_raster_area_px\": " << overlap.obb_area_px
           << ", \"aabb_raster_area_px\": " << overlap.aabb_area_px
           << ", \"reference_metrics_status\": \"REFERENCE_REQUIRED\""
           << ", \"center_error_px\": null"
           << ", \"major_axis_error_px\": null"
           << ", \"minor_axis_error_px\": null"
           << ", \"angle_error_deg\": null"
           << ", \"vertices\": [";
      for (std::size_t vertex = 0; vertex < box.ordered_points.size(); ++vertex) {
        if (vertex > 0)
          file << ", ";
        file << "[" << box.ordered_points[vertex].x << ", "
             << box.ordered_points[vertex].y << "]";
      }
      file << "]";
    } else {
      file << ", \"fit_reason\": \"" << EscapeSegmentationJson(fit.reason)
           << "\"";
    }
    file << "}";
  }
  file << "\n  ],\n"
       << "  \"conclusion\": \""
       << (any_complete ? "YOLOV8_SEG_POSTPROCESS_OBB_PASS"
                        : "YOLOV8_SEG_POSTPROCESS_OBB_PENDING")
       << "\"\n}\n";
  file.close();
  if (!file) {
    reason = "failed while writing oriented-box evidence: " + evidence_path.string();
    return false;
  }

  evidence_ref = evidence_path.string();
  conclusion = any_complete ? "YOLOV8_SEG_POSTPROCESS_OBB_PASS"
                            : "YOLOV8_SEG_POSTPROCESS_OBB_PENDING";
  reason.clear();
  return true;
}
} // namespace

FindSegmentation::FindSegmentation() {}

void FindSegmentation::setbackend(const char *backend) {
  if (backend != nullptr)
    m_backend = backend;
}

void FindSegmentation::setmodel(const char *model_path) {
  if (model_path != nullptr)
    m_model_path = model_path;
}

void FindSegmentation::settask(const char *task_id) {
  if (task_id != nullptr)
    m_task_id = task_id;
}

void FindSegmentation::setmodelid(const char *model_id) {
  if (model_id != nullptr)
    m_model_id = model_id;
}

void FindSegmentation::setmodelpackage(const char *model_package_ref) {
  if (model_package_ref != nullptr)
    m_model_package_ref = model_package_ref;
}

void FindSegmentation::setmanifest(const char *manifest_path) {
  if (manifest_path != nullptr)
    m_manifest_path = manifest_path;
}

void FindSegmentation::setoutputroot(const char *output_root) {
  if (output_root == nullptr)
    return;
  std::string configured = output_root;
  // Headless owns a single, already registered string context in the form
  // case|image|output_root|metadata|dataset. Parse only its output field;
  // ordinary callers still pass a directory directly.
  const std::size_t first_separator = configured.find('|');
  const std::size_t second_separator =
      first_separator == std::string::npos ? std::string::npos
                                           : configured.find('|', first_separator + 1);
  const std::size_t third_separator =
      second_separator == std::string::npos ? std::string::npos
                                            : configured.find('|', second_separator + 1);
  if (second_separator != std::string::npos &&
      third_separator != std::string::npos) {
    m_output_root = configured.substr(
        second_separator + 1, third_separator - second_separator - 1);
  } else {
    m_output_root = configured;
  }
}

void FindSegmentation::setpostprocessprofile(const char *postprocess_profile) {
  if (postprocess_profile != nullptr)
    m_postprocess_profile = postprocess_profile;
}

void FindSegmentation::setparameterprofile(const char *parameter_profile_ref) {
  if (parameter_profile_ref != nullptr)
    m_parameter_profile_ref = parameter_profile_ref;
}


void FindSegmentation::setdevice(const char *device) {
  if (device != nullptr)
    m_device = device;
}

void FindSegmentation::setthreshold(double threshold) {
  m_threshold = threshold;
}

void FindSegmentation::setpromptrect(int x0, int y0, int x1, int y1) {
  m_x0 = x0;
  m_y0 = y0;
  m_x1 = x1;
  m_y1 = y1;
  m_has_rect = true;
}

void FindSegmentation::setpromptrectxyxy(int y1, int x1, int y0, int x0) {
  setpromptrect(x0, y0, x1, y1);
}

void FindSegmentation::setpoint(int x, int y) { setpositivepoint(x, y); }

void FindSegmentation::setpositivepoint(int x, int y) {
  m_positive_x = x;
  m_positive_y = y;
  m_has_positive_point = true;
}

void FindSegmentation::setnegativepoint(int x, int y) {
  m_negative_x = x;
  m_negative_y = y;
  m_has_negative_point = true;
}

void FindSegmentation::setpositivepointxy(int y, int x) {
  setpositivepoint(x, y);
}

void FindSegmentation::setnegativepointxy(int y, int x) {
  setnegativepoint(x, y);
}

void FindSegmentation::setmode(int mode) { m_mode = mode; }

void FindSegmentation::setgeometrytype(const char *geometry_type) {
  m_geometry_type = geometry_type == nullptr ? "" : geometry_type;
}


void FindSegmentation::segment(void *image) {
  std::cout << "[FindSegmentation] segment begin image=" << image << "\n"
            << std::flush;

  m_last_input_request = FindSegmentationInputSnapshot();
  m_backend_diagnostic = FindSegmentationBackendDiagnosticSnapshot();
  m_last_input_request.backend = m_backend;
  m_last_input_request.model_path = m_model_path;
  m_last_input_request.device = m_device;
  m_last_input_request.task_id = m_task_id;
  m_last_input_request.model_id = m_model_id;
  m_last_input_request.model_package_ref = m_model_package_ref;
  m_last_input_request.manifest_path = m_manifest_path;
  m_last_input_request.output_root = m_output_root;
  m_last_input_request.postprocess_profile = m_postprocess_profile;
  m_last_input_request.parameter_profile_ref = m_parameter_profile_ref;

  m_last_input_request.threshold = m_threshold;
  m_last_input_request.mode = m_mode;
  m_backend_diagnostic.backend = m_backend;
  m_backend_diagnostic.task_id = m_task_id;
  m_backend_diagnostic.model_id = m_model_id;
  m_backend_diagnostic.model_package_ref = m_model_package_ref;
  m_backend_diagnostic.manifest_path = m_manifest_path;
  m_backend_diagnostic.postprocess_profile = m_postprocess_profile;
  m_backend_diagnostic.parameter_profile_ref = m_parameter_profile_ref;

  m_backend_diagnostic.prompt_rect_ready = m_has_rect;
  m_backend_diagnostic.prompt_point_ready = m_has_positive_point;
  m_backend_diagnostic.prompt_positive_ready = m_has_positive_point;
  m_backend_diagnostic.prompt_negative_ready = m_has_negative_point;

  if (image == nullptr) {
    m_status = "failed";
    m_reason = "segment image pointer is null";
    m_result.ok = false;
    m_backend_diagnostic.status = m_status;
    m_backend_diagnostic.reason = m_reason;
    std::cout << "[FindSegmentation] segment end status=" << m_status
              << " reason=" << m_reason << "\n"
              << std::flush;
    return;
  }

  Image *img = static_cast<Image *>(image);
  cv::Mat mat = img->getmat();

  if (mat.empty()) {
    m_status = "failed";
    m_reason = "segment input mat is empty";
    m_result.ok = false;
    m_backend_diagnostic.status = m_status;
    m_backend_diagnostic.reason = m_reason;
    std::cout << "[FindSegmentation] segment end status=" << m_status
              << " reason=" << m_reason << "\n"
              << std::flush;
    return;
  }

  m_last_input_request.image_width = mat.cols;
  m_last_input_request.image_height = mat.rows;
  m_backend_diagnostic.image_ready = true;

  std::cout << "[FindSegmentation] backend=" << m_backend << "\n" << std::flush;

  FindSegmentationInput input;
  input.image = mat;
  input.model_path = m_model_path;
  input.device = m_device;
  input.backend = m_backend;
  input.task_id = m_task_id;
  input.model_id = m_model_id;
  input.model_package_ref = m_model_package_ref;
  input.manifest_path = m_manifest_path;
  input.output_root = m_output_root;
  input.postprocess_profile = m_postprocess_profile;
  input.parameter_profile_ref = m_parameter_profile_ref;

  input.threshold = m_threshold;
  input.mode = m_mode;

  if (m_has_rect) {
    input.has_rect = true;
    input.rect = cv::Rect(m_x0, m_y0, m_x1 - m_x0, m_y1 - m_y0);
    m_last_input_request.has_rect = true;
    m_last_input_request.rect_x = input.rect.x;
    m_last_input_request.rect_y = input.rect.y;
    m_last_input_request.rect_width = input.rect.width;
    m_last_input_request.rect_height = input.rect.height;
    std::cout << "[FindSegmentation] prompt_rect state=" << m_x0 << "," << m_y0
              << "," << m_x1 << "," << m_y1 << " input_rect=" << input.rect.x
              << "," << input.rect.y << "," << input.rect.width << ","
              << input.rect.height << " image=" << mat.cols << "x" << mat.rows
              << "\n"
              << std::flush;
  }

  if (m_has_positive_point) {
    // Populate the legacy slot for old backends while preserving prompt
    // polarity in the explicit fields below.
    input.has_point = true;
    input.point = cv::Point(m_positive_x, m_positive_y);
    m_last_input_request.has_point = true;
    m_last_input_request.point_x = input.point.x;
    m_last_input_request.point_y = input.point.y;
    input.has_positive_point = true;
    input.positive_point = input.point;
    m_last_input_request.has_positive_point = true;
    m_last_input_request.positive_point_x = input.point.x;
    m_last_input_request.positive_point_y = input.point.y;
  }

  if (m_has_negative_point) {
    input.has_negative_point = true;
    input.negative_point = cv::Point(m_negative_x, m_negative_y);
    m_last_input_request.has_negative_point = true;
    m_last_input_request.negative_point_x = input.negative_point.x;
    m_last_input_request.negative_point_y = input.negative_point.y;
  }

  std::string reason;

  try {
    if (m_backend == "torch" || m_backend == "edgesam" || m_backend == "libtorch_segmentation")
    {
      FindSegmentationEdgeSamBackend backend;
      backend.Run(input, m_result, reason);
    }
    else {
      FindSegmentationOpenCvSmokeBackend backend;
      backend.Run(input, m_result, reason);
    }
  }
  catch (const std::exception &ex) {
    m_result = FindSegmentationResult();
    m_result.ok = false;
    m_result.backend = m_backend;
    m_result.backend_status = "backend_exception";
    m_result.status = "backend_exception";
    m_result.reason =
        std::string("FindSegmentation backend exception: ") + ex.what();
    reason = m_result.reason;
  }
  catch (...) {
    m_result = FindSegmentationResult();
    m_result.ok = false;
    m_result.backend = m_backend;
    m_result.backend_status = "backend_unknown_exception";
    m_result.status = "backend_unknown_exception";
    m_result.reason = "FindSegmentation backend unknown exception";
    reason = m_result.reason;
  }

  if (m_result.task_id.empty())
    m_result.task_id = m_task_id;

  if (m_result.model_id.empty())
    m_result.model_id = m_model_id;

  if (m_result.model_package_ref.empty())
    m_result.model_package_ref = m_model_package_ref;

  if (m_result.manifest_path.empty())
    m_result.manifest_path = m_manifest_path;

  if (m_result.postprocess_profile.empty())
    m_result.postprocess_profile = m_postprocess_profile;

  if (m_result.parameter_profile_ref.empty())
    m_result.parameter_profile_ref = m_parameter_profile_ref;

  if (m_result.result_stage == "not_run")
    m_result.result_stage = m_result.ok ? "raw" : "failed";

  if (m_result.ok && !m_geometry_type.empty())
    extractboundary();


  m_status = m_result.status;
  m_reason = m_result.reason;
  m_backend_diagnostic.backend =
      m_result.backend.empty() ? m_backend : m_result.backend;
  m_backend_diagnostic.backend_status = m_result.backend_status;
  m_backend_diagnostic.status = m_result.status;
  m_backend_diagnostic.reason = m_result.reason;
  m_backend_diagnostic.mask_ready =
      !m_result.mask.empty() || !m_result.mask_ref.empty();
  m_backend_diagnostic.overlay_ready =
      !m_result.overlay.empty() || !m_result.overlay_ref.empty();
  m_backend_diagnostic.mask_width = m_result.mask_width;
  m_backend_diagnostic.mask_height = m_result.mask_height;
  m_backend_diagnostic.contour_count = m_result.contour_count;
  m_backend_diagnostic.primary_area = m_result.primary_area;
  m_backend_diagnostic.region_count = m_result.region_count;
  m_backend_diagnostic.raw_result_available = m_result.raw_result_available;
  m_backend_diagnostic.refined_result_available = m_result.refined_result_available;
  m_backend_diagnostic.fallback_used = m_result.fallback_used;
  m_backend_diagnostic.result_stage = m_result.result_stage;
  m_backend_diagnostic.refinement_method = m_result.refinement_method;


  m_result_ref = m_result.result_ref.empty() ? "segmentation:" + m_backend
                                             : m_result.result_ref;
  m_mask_ref =
      m_result.mask_ref.empty() ? "mask:" + m_backend : m_result.mask_ref;
  m_contour_ref = m_result.contour_ref.empty() ? "contour:" + m_backend
                                               : m_result.contour_ref;
  m_overlay_ref = m_result.overlay_ref.empty() ? "overlay:" + m_backend
                                               : m_result.overlay_ref;

  std::cout << "[FindSegmentation] segment end status=" << m_status << "\n"
            << std::flush;
}

void FindSegmentation::extractboundary() {
  m_result.primitive_hypotheses.clear();
  m_result.requested_geometry_type = m_geometry_type;
  m_result.geometry_fit_status = "not_run";
  m_result.geometry_fit_reason.clear();
  m_result.oriented_box_evidence_ref.clear();
  m_result.oriented_box_conclusion.clear();

  if (m_geometry_type.empty()) {
    m_result.geometry_fit_status = "GEOMETRY_TYPE_REQUIRED";
    m_result.geometry_fit_reason =
        "setgeometrytype must be called with circle, ellipse, oriented_box, or line";
    return;
  }

  const FindSegmentationContour *best = nullptr;
  for (const FindSegmentationContour &contour : m_result.contours) {
    if (best == nullptr || contour.area > best->area)
      best = &contour;
  }
  if (best == nullptr || best->points.empty()) {
    m_result.geometry_fit_status = "CONTOUR_EMPTY";
    m_result.geometry_fit_reason =
        "segmentation produced no contour for geometry fitting";
    return;
  }

  CxSegmentationGeometryFitOptions options;
  options.geometry_type = m_geometry_type;
  options.tolerance_policy_ref = m_postprocess_profile.empty()
      ? "unbound_geometry_tolerance"
      : m_postprocess_profile;
  CxSegmentationGeometryFitResult fit;
  FitCxSegmentationContourGeometry(best->points, options, fit);
  m_result.geometry_fit_status = fit.status;
  m_result.geometry_fit_reason = fit.reason;
  if (!fit.complete)
    return;

  if (m_geometry_type == "oriented_box") {
    fit.hypothesis.source = "yolov8_seg_mask_min_area_rect";
    std::string evidence_reason;
    if (!WriteMaskToObbEvidence(
            m_result, m_result.oriented_box_evidence_ref,
            m_result.oriented_box_conclusion, evidence_reason)) {
      m_result.geometry_fit_status = "OBB_EVIDENCE_WRITE_FAILED";
      m_result.geometry_fit_reason = evidence_reason;
      return;
    }
    m_result.geometry_fit_status = m_result.oriented_box_conclusion;
    m_result.geometry_fit_reason =
        "mask-to-minAreaRect postprocess completed; this is not native angle regression";
    m_result.refinement_method = "yolov8_seg_mask_min_area_rect";
  }
  m_result.primitive_hypotheses.push_back(std::move(fit.hypothesis));
  if (m_geometry_type != "oriented_box")
    m_result.refinement_method = "seg_contour_fit:" + m_geometry_type;
}

void FindSegmentation::buildoverlay(void *image) {
  if (image == nullptr)
    return;

  if (!m_result.overlay_ref.empty()) {
    m_overlay_ref = m_result.overlay_ref;
    return;
  }

  if (!m_result.overlay.empty()) {
    m_overlay_ref = "overlay:" + m_backend + ":generated";
  }
}

const char *FindSegmentation::get_result() { return m_result_ref.c_str(); }

const char *FindSegmentation::get_mask_ref() { return m_mask_ref.c_str(); }

const char *FindSegmentation::get_contour_ref() {
  return m_contour_ref.c_str();
}

const char *FindSegmentation::get_overlay_ref() {
  return m_overlay_ref.c_str();
}

int FindSegmentation::status_code() { return m_result.ok ? 1 : 0; }

int FindSegmentation::get_contour_count() { return m_result.contour_count; }

double FindSegmentation::get_primary_area() { return m_result.primary_area; }

int FindSegmentation::get_geometry_count() {
  return static_cast<int>(m_result.primitive_hypotheses.size());
}

double FindSegmentation::get_geometry_residual() {
  return m_result.primitive_hypotheses.empty()
      ? 0.0
      : m_result.primitive_hypotheses.front().classical_fit_residual_px;
}

double FindSegmentation::get_geometry_support() {
  return m_result.primitive_hypotheses.empty()
      ? 0.0
      : m_result.primitive_hypotheses.front().support;
}

double FindSegmentation::get_geometry_center_x() {
  return m_result.primitive_hypotheses.empty()
      ? 0.0
      : m_result.primitive_hypotheses.front().center.x;
}

double FindSegmentation::get_geometry_center_y() {
  return m_result.primitive_hypotheses.empty()
      ? 0.0
      : m_result.primitive_hypotheses.front().center.y;
}

double FindSegmentation::get_geometry_radius() {
  return m_result.primitive_hypotheses.empty()
      ? 0.0
      : m_result.primitive_hypotheses.front().radius;
}

double FindSegmentation::get_geometry_axis_x() {
  return m_result.primitive_hypotheses.empty()
      ? 0.0
      : m_result.primitive_hypotheses.front().axes.width;
}

double FindSegmentation::get_geometry_axis_y() {
  return m_result.primitive_hypotheses.empty()
      ? 0.0
      : m_result.primitive_hypotheses.front().axes.height;
}

double FindSegmentation::get_geometry_angle_deg() {
  return m_result.primitive_hypotheses.empty()
      ? 0.0
      : m_result.primitive_hypotheses.front().angle_deg;
}


const std::string &FindSegmentation::backend() const { return m_backend; }

const std::string &FindSegmentation::model_path() const { return m_model_path; }

const std::string &FindSegmentation::device() const { return m_device; }

const FindSegmentationResult &FindSegmentation::result() const {
  return m_result;
}

const FindSegmentationInputSnapshot &
FindSegmentation::lastinputrequest() const {
  return m_last_input_request;
}

const FindSegmentationBackendDiagnosticSnapshot &
FindSegmentation::backenddiagnostic() const {
  return m_backend_diagnostic;
}

void FindSegmentation::PublishDisplayShapes(
    ICxShapeSink &sink, const std::string &owner_ref) const {
  // Prompt ROI / positive points / negative points are user-owned annotation
  // inputs. They are already created and edited by ImageAnnotationLayer from
  // the Tool Palette. Runtime projection must not mirror them back as new
  // ShapeElements, otherwise every script run duplicates the prompt points.
  //
  // This method only publishes algorithm result evidence owned by the
  // FindSegmentation runtime object: boundary polyline and boundary bbox.

  if (m_result.contours.empty())
    return;

  const FindSegmentationContour *best = nullptr;
  for (const FindSegmentationContour &contour : m_result.contours) {
    if (best == nullptr || contour.area > best->area)
      best = &contour;
  }

  if (best == nullptr || best->points.empty())
    return;

  auto polyline = std::make_unique<PolylineShape>();
  for (const cv::Point &p : best->points)
    polyline->addPoint(p.x, p.y);
  polyline->close(true);
  sink.UpsertShape(owner_ref + ".boundary_polyline", "FindSegmentation",
                   owner_ref, "boundary", "boundary", false, true,
                   std::move(polyline));

  cv::Rect bbox = cv::boundingRect(best->points);
  auto rect = std::make_unique<RectShape>();
  rect->setRect(bbox.x, bbox.y, bbox.x + bbox.width, bbox.y + bbox.height);
  sink.UpsertShape(owner_ref + ".boundary_bbox", "FindSegmentation", owner_ref,
                   "boundary_bbox", "boundary_bbox", false, true,
                   std::move(rect));

  if (m_result.primitive_hypotheses.empty())
    return;
  const CxGeometryPrimitiveHypothesis &geometry =
      m_result.primitive_hypotheses.front();
  std::unique_ptr<ShapeBase> geometry_shape;
  if (geometry.geometry_type == "circle" && geometry.radius > 0.0) {
    geometry_shape = std::make_unique<CircleShape>(
        geometry.center.x, geometry.center.y, geometry.radius);
  } else if (geometry.geometry_type == "ellipse" &&
             geometry.axes.width > 0.0 && geometry.axes.height > 0.0) {
    geometry_shape = std::make_unique<EllipseShape>(
        geometry.center.x, geometry.center.y, geometry.axes.width,
        geometry.axes.height, geometry.angle_deg);
  } else if (geometry.geometry_type == "line" &&
             geometry.ordered_points.size() == 2) {
    auto line = std::make_unique<PolylineShape>();
    line->addPoint(geometry.ordered_points[0].x,
                   geometry.ordered_points[0].y);
    line->addPoint(geometry.ordered_points[1].x,
                   geometry.ordered_points[1].y);
    line->close(false);
    geometry_shape = std::move(line);
  }
  if (geometry_shape) {
    sink.UpsertShape(owner_ref + ".geometry_fit", "FindSegmentation",
                     owner_ref, "geometry_fit", geometry.geometry_type,
                     false, true, std::move(geometry_shape));
  }

}
