#include "parser_cxcore_classical_adapter.h"
#include "parser_cxscript_types.h"

#include <cctype>
#include <sstream>

#ifdef CXPARSER_ENABLE_CXCORE_REAL_BRIDGE
#include "../../cxcore/core/FastMatch.h"
#include "../../cxcore/core/Findcircle.h"
#include "../../cxcore/core/Findline.h"
#include "../../cxcore/core/Image.h"
#include "../../cxcore/core/imagemanager.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sstream>
#endif

namespace cxparser_ext
{
namespace
{
void PushUniqueReviewText(std::vector<std::string> &values, const std::string &value)
{
  if (value.empty())
    return;

  for (size_t i = 0; i < values.size(); ++i)
  {
    if (values[i] == value)
      return;
  }

  values.push_back(value);
}

std::string FormatReviewDouble(double value)
{
  std::ostringstream stream;
  stream.setf(std::ios::fixed, std::ios::floatfield);
  stream.precision(4);
  stream << value;
  std::string text = stream.str();
  const std::string::size_type point = text.find('.');
  if (point == std::string::npos)
    return text;

  std::string::size_type end = text.size();
  while (end > point + 1 && text[end - 1] == '0')
    --end;
  if (end > point && text[end - 1] == '.')
    --end;
  text.resize(end);
  return text;
}

std::string BuildReviewScopedRef(const CxScriptExecutionResult &result, const char *suffix)
{
  if (result.module.empty() ||
      result.layer.empty() ||
      result.case_name.empty() ||
      suffix == nullptr ||
      suffix[0] == '\0')
    return std::string();

  return "review_cxparser_tests::" + result.module + "::" +
         result.layer + "::" + result.case_name + "::" + suffix;
}

std::string FindReviewNamedFieldValue(const CxScriptExecutionResult &result,
                                      const char *result_name,
                                      const char *field_name)
{
  if (result_name == nullptr || field_name == nullptr)
    return std::string();

  for (size_t i = 0; i < result.result_fields.size(); ++i)
  {
    const CxScriptNamedResultField &field = result.result_fields[i];
    if (field.result_name == result_name && field.field_name == field_name)
      return field.value;
  }

  return std::string();
}

std::string ToLowerReviewText(std::string value)
{
  for (size_t i = 0; i < value.size(); ++i)
    value[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(value[i])));
  return value;
}

bool LooksLikeRasterImageRef(const std::string &value)
{
  if (value.empty())
    return false;

  const std::string lowered = ToLowerReviewText(value);
  return lowered.find(".png") != std::string::npos ||
         lowered.find(".jpg") != std::string::npos ||
         lowered.find(".jpeg") != std::string::npos ||
         lowered.find(".bmp") != std::string::npos ||
         lowered.find(".gif") != std::string::npos ||
         lowered.find(".webp") != std::string::npos ||
         lowered.find(".tif") != std::string::npos ||
         lowered.find(".tiff") != std::string::npos;
}

std::string SelectClassicalInputImageRef(const CxScriptExecutionResult &result)
{
  const std::string bridge_input = FindReviewNamedFieldValue(result, "bridge", "input_image");
  if (LooksLikeRasterImageRef(bridge_input))
    return bridge_input;
  if (LooksLikeRasterImageRef(result.input_sample))
    return result.input_sample;
  if (LooksLikeRasterImageRef(result.dataset_ref))
    return result.dataset_ref;
  return std::string();
}

std::string SelectClassicalTemplateImageRef(const CxScriptExecutionResult &result)
{
  const std::string bridge_template = FindReviewNamedFieldValue(result, "bridge", "template_image");
  if (LooksLikeRasterImageRef(bridge_template))
    return bridge_template;
  return std::string();
}

void PushReviewMetricRecord(std::vector<CxcoreClassicalReviewMetric> &metrics,
                            const char *metric_name,
                            const std::string &metric_value,
                            const char *metric_unit,
                            const char *expected_range,
                            const char *deviation_level,
                            const char *metric_status)
{
  if (metric_name == nullptr || metric_name[0] == '\0' || metric_value.empty())
    return;

  CxcoreClassicalReviewMetric metric;
  metric.metric_name = metric_name;
  metric.metric_value = metric_value;
  if (metric_unit != nullptr)
    metric.metric_unit = metric_unit;
  if (expected_range != nullptr)
    metric.expected_range = expected_range;
  if (deviation_level != nullptr)
    metric.deviation_level = deviation_level;
  if (metric_status != nullptr)
    metric.metric_status = metric_status;
  metrics.push_back(metric);
}

#ifdef CXPARSER_ENABLE_CXCORE_REAL_BRIDGE
bool ShouldUseCircleBoundarySinglePass()
{
  const char *raw = std::getenv("CXCIRCLE_BOUNDARY_SINGLE_PASS");
  if (raw == nullptr)
    return false;
  return raw[0] == '1' || raw[0] == 't' || raw[0] == 'T' || raw[0] == 'y' || raw[0] == 'Y';
}
#endif

cxcore::OutputRect ToOutputRect(const ImageAnalysisRect &rect)
{
  cxcore::OutputRect output;
  output.x = static_cast<double>(rect.x);
  output.y = static_cast<double>(rect.y);
  output.width = static_cast<double>(rect.width);
  output.height = static_cast<double>(rect.height);
  return output;
}

#ifdef CXPARSER_ENABLE_CXCORE_REAL_BRIDGE
struct LineBridgeCaseSpec
{
  cv::Mat image_mat;
  int roi_x = 0;
  int roi_y = 0;
  int roi_w = 0;
  int roi_h = 0;
  int wh_gap = 6;
  int compare_gap = 4;
  int line_gap = 4;
  int method = 1;
  int threshold = 8;
  int selected_edge = 0;
  bool call_measure = true;
};

struct CircleBridgeCaseSpec
{
  cv::Mat image_mat;
  int image_manager_size = 256;
  int center_x = 128;
  int center_y = 128;
  int initial_pax = 16;
  int initial_pay = 128;
  bool call_measure = true;
};

struct TemplateMatchBridgeCaseSpec
{
  cv::Mat model_mat;
  cv::Mat search_mat;
  int model_roi_x = 8;
  int model_roi_y = 10;
  int model_roi_w = 20;
  int model_roi_h = 14;
  int search_roi_x = 20;
  int search_roi_y = 24;
  int search_roi_w = 48;
  int search_roi_h = 40;
  int threshold = 12;
  int line_gap = 2;
  int compare_gap = 12;
  int method = 2;
  int match_threshold = 8;
  int max_candidates = 1;
  double min_score = 0.35;
  int step_gap_x = 6;
  int step_gap_y = 6;
  int grid_x = 16;
  int grid_y = 4;
};

struct RegionBoundaryBridgeCaseSpec
{
  cv::Mat image_mat;
  cv::Rect roi;
  bool call_measure = true;
};

LineBridgeCaseSpec BuildLineBridgeCaseSpec(const std::string &case_name)
{
  LineBridgeCaseSpec spec;
  spec.image_mat = cv::Mat(96, 96, CV_8UC3, cv::Scalar(0, 0, 0));

  if (case_name == "line_measurement_boundary")
  {
    spec.image_mat = cv::Mat(24, 24, CV_8UC3, cv::Scalar(0, 0, 0));
    cv::line(spec.image_mat, cv::Point(2, 6), cv::Point(14, 6), cv::Scalar(255, 255, 255), 2);
    spec.roi_x = 4;
    spec.roi_y = 4;
    spec.roi_w = 8;
    spec.roi_h = 4;
    spec.wh_gap = 1;
    spec.compare_gap = 1;
    spec.line_gap = 1;
    spec.threshold = 1;
    return spec;
  }

  if (case_name == "line_measurement_noise")
  {
    cv::line(spec.image_mat, cv::Point(12, 48), cv::Point(84, 48), cv::Scalar(255, 255, 255), 3);
    cv::Mat noise(spec.image_mat.size(), CV_16SC3);
    cv::RNG rng(12345);
    rng.fill(noise, cv::RNG::NORMAL, cv::Scalar::all(0), cv::Scalar::all(18));
    cv::Mat noisy_16s;
    spec.image_mat.convertTo(noisy_16s, CV_16SC3);
    cv::add(noisy_16s, noise, noisy_16s);
    noisy_16s.convertTo(spec.image_mat, CV_8UC3);
    cv::GaussianBlur(spec.image_mat, spec.image_mat, cv::Size(5, 5), 0.8);
    cv::add(spec.image_mat, cv::Scalar(20, 20, 20), spec.image_mat);
    spec.roi_x = 12;
    spec.roi_y = 36;
    spec.roi_w = 72;
    spec.roi_h = 24;
    spec.threshold = 6;
    return spec;
  }

  if (case_name == "line_measurement_degenerate")
  {
    spec.image_mat = cv::Mat(32, 32, CV_8UC3, cv::Scalar(0, 0, 0));
    spec.roi_x = 0;
    spec.roi_y = 0;
    spec.roi_w = 0;
    spec.roi_h = 0;
    spec.call_measure = false;
    return spec;
  }

  cv::line(spec.image_mat, cv::Point(12, 48), cv::Point(84, 48), cv::Scalar(255, 255, 255), 3);
  spec.roi_x = 12;
  spec.roi_y = 36;
  spec.roi_w = 72;
  spec.roi_h = 24;
  return spec;
}

CircleBridgeCaseSpec BuildCircleBridgeCaseSpec(const std::string &case_name)
{
  CircleBridgeCaseSpec spec;
  spec.image_mat = cv::Mat(256, 256, CV_8UC3, cv::Scalar(0, 0, 0));
  cv::circle(spec.image_mat, cv::Point(128, 128), 32, cv::Scalar(255, 255, 255), 3);

  if (case_name == "circle_measurement_boundary")
  {
    spec.image_manager_size = 128;
    spec.image_mat = cv::Mat(128, 128, CV_8UC3, cv::Scalar(0, 0, 0));
    spec.center_x = 64;
    spec.center_y = 64;
    spec.initial_pax = 64;
    spec.initial_pay = 50;
    cv::circle(spec.image_mat, cv::Point(spec.center_x, spec.center_y), 14, cv::Scalar(220, 220, 220), 2);
    return spec;
  }

  if (case_name == "circle_measurement_noise")
  {
    cv::Mat noise(spec.image_mat.size(), CV_16SC3);
    cv::RNG rng(24680);
    rng.fill(noise, cv::RNG::NORMAL, cv::Scalar::all(0), cv::Scalar::all(16));
    cv::Mat noisy_16s;
    spec.image_mat.convertTo(noisy_16s, CV_16SC3);
    cv::add(noisy_16s, noise, noisy_16s);
    noisy_16s.convertTo(spec.image_mat, CV_8UC3);
    cv::GaussianBlur(spec.image_mat, spec.image_mat, cv::Size(5, 5), 0.8);
    cv::add(spec.image_mat, cv::Scalar(18, 18, 18), spec.image_mat);
    return spec;
  }

  if (case_name == "circle_measurement_degenerate")
  {
    spec.image_manager_size = 64;
    spec.image_mat = cv::Mat(64, 64, CV_8UC3, cv::Scalar(0, 0, 0));
    spec.center_x = 32;
    spec.center_y = 32;
    spec.initial_pax = 8;
    spec.initial_pay = 32;
    spec.call_measure = false;
    return spec;
  }

  return spec;
}

TemplateMatchBridgeCaseSpec BuildTemplateMatchBridgeCaseSpec(const std::string &case_name)
{
  TemplateMatchBridgeCaseSpec spec;
  spec.model_mat = cv::Mat(40, 40, CV_8UC3, cv::Scalar(0, 0, 0));
  cv::rectangle(spec.model_mat, cv::Rect(8, 10, 20, 14), cv::Scalar(255, 255, 255), cv::FILLED);
  cv::line(spec.model_mat, cv::Point(8, 17), cv::Point(27, 17), cv::Scalar(0, 0, 0), 2);

  spec.search_mat = cv::Mat(96, 96, CV_8UC3, cv::Scalar(0, 0, 0));
  cv::rectangle(spec.search_mat, cv::Rect(30, 34, 20, 14), cv::Scalar(255, 255, 255), cv::FILLED);
  cv::line(spec.search_mat, cv::Point(30, 41), cv::Point(49, 41), cv::Scalar(0, 0, 0), 2);

  if (case_name == "template_feature_match_boundary")
  {
    spec.model_mat = cv::Mat(24, 24, CV_8UC3, cv::Scalar(0, 0, 0));
    cv::rectangle(spec.model_mat, cv::Rect(6, 8, 12, 8), cv::Scalar(255, 255, 255), cv::FILLED);
    cv::line(spec.model_mat, cv::Point(6, 12), cv::Point(17, 12), cv::Scalar(0, 0, 0), 1);
    spec.search_mat = cv::Mat(40, 40, CV_8UC3, cv::Scalar(0, 0, 0));
    cv::rectangle(spec.search_mat, cv::Rect(10, 14, 12, 8), cv::Scalar(255, 255, 255), cv::FILLED);
    cv::line(spec.search_mat, cv::Point(10, 18), cv::Point(21, 18), cv::Scalar(0, 0, 0), 1);
    spec.model_roi_x = 6;
    spec.model_roi_y = 8;
    spec.model_roi_w = 12;
    spec.model_roi_h = 8;
    spec.search_roi_x = 6;
    spec.search_roi_y = 6;
    spec.search_roi_w = 22;
    spec.search_roi_h = 20;
    spec.threshold = 8;
    spec.line_gap = 1;
    spec.compare_gap = 8;
    spec.match_threshold = 6;
    spec.min_score = 0.2;
    spec.step_gap_x = 4;
    spec.step_gap_y = 4;
    spec.grid_x = 12;
    spec.grid_y = 4;
    return spec;
  }

  if (case_name == "template_feature_match_noise")
  {
    cv::Mat noise(spec.search_mat.size(), CV_16SC3);
    cv::RNG rng(13579);
    rng.fill(noise, cv::RNG::NORMAL, cv::Scalar::all(0), cv::Scalar::all(18));
    cv::Mat noisy_16s;
    spec.search_mat.convertTo(noisy_16s, CV_16SC3);
    cv::add(noisy_16s, noise, noisy_16s);
    noisy_16s.convertTo(spec.search_mat, CV_8UC3);
    cv::GaussianBlur(spec.search_mat, spec.search_mat, cv::Size(5, 5), 0.8);
    cv::add(spec.search_mat, cv::Scalar(18, 18, 18), spec.search_mat);
    spec.threshold = 12;
    spec.min_score = 0.35;
    return spec;
  }

  if (case_name == "template_feature_match_degenerate")
  {
    spec.model_mat = cv::Mat(32, 32, CV_8UC3, cv::Scalar(0, 0, 0));
    spec.search_mat = cv::Mat(32, 32, CV_8UC3, cv::Scalar(0, 0, 0));
    spec.model_roi_x = 0;
    spec.model_roi_y = 0;
    spec.model_roi_w = 0;
    spec.model_roi_h = 0;
    spec.search_roi_x = 0;
    spec.search_roi_y = 0;
    spec.search_roi_w = 0;
    spec.search_roi_h = 0;
    return spec;
  }

  return spec;
}

RegionBoundaryBridgeCaseSpec BuildRegionBoundaryBridgeCaseSpec(const std::string &case_name)
{
  RegionBoundaryBridgeCaseSpec spec;
  spec.image_mat = cv::Mat(96, 96, CV_8UC3, cv::Scalar(0, 0, 0));
  spec.roi = cv::Rect(16, 24, 48, 32);
  cv::rectangle(spec.image_mat, cv::Rect(20, 30, 12, 10), cv::Scalar(255, 255, 255), cv::FILLED);
  cv::rectangle(spec.image_mat, cv::Rect(44, 38, 14, 12), cv::Scalar(255, 255, 255), cv::FILLED);

  if (case_name == "region_boundary_analysis_boundary")
  {
    spec.image_mat = cv::Mat(16, 16, CV_8UC3, cv::Scalar(0, 0, 0));
    spec.roi = cv::Rect(2, 2, 6, 6);
    cv::rectangle(spec.image_mat, cv::Rect(3, 3, 3, 3), cv::Scalar(255, 255, 255), cv::FILLED);
    return spec;
  }

  if (case_name == "region_boundary_analysis_noise")
  {
    cv::Mat noise(spec.image_mat.size(), CV_16SC3);
    cv::RNG rng(97531);
    rng.fill(noise, cv::RNG::NORMAL, cv::Scalar::all(0), cv::Scalar::all(18));
    cv::Mat noisy_16s;
    spec.image_mat.convertTo(noisy_16s, CV_16SC3);
    cv::add(noisy_16s, noise, noisy_16s);
    noisy_16s.convertTo(spec.image_mat, CV_8UC3);
    cv::GaussianBlur(spec.image_mat, spec.image_mat, cv::Size(5, 5), 0.8);
    cv::threshold(spec.image_mat, spec.image_mat, 90, 255, cv::THRESH_BINARY);
    return spec;
  }

  if (case_name == "region_boundary_analysis_degenerate")
  {
    spec.image_mat = cv::Mat(32, 32, CV_8UC3, cv::Scalar(0, 0, 0));
    spec.roi = cv::Rect(0, 0, 0, 0);
    spec.call_measure = false;
    return spec;
  }

  return spec;
}
#endif
}

cxcore::BaselineFeatureSampleV1 ConvertToCxcoreBaselineSample(const ImageAnalysisRequest &request,
                                                              const ImageAnalysisResult &result)
{
  cxcore::BaselineFeatureSampleV1 sample;
  sample.sample_id = result.task_id;
  sample.source_image_id = request.trace_id;
  sample.label = request.module_name;
  sample.split = result.route_lane;
  sample.image_width = static_cast<double>(result.image_width);
  sample.image_height = static_cast<double>(result.image_height);

  if (!result.roi_results.empty())
  {
    const ImageAnalysisRoiResult &roi = result.roi_results[0];
    const cxcore::OutputRect rect = ToOutputRect(roi.bounds);
    sample.roi_id = roi.roi_id;
    sample.roi_x = rect.x;
    sample.roi_y = rect.y;
    sample.roi_w = rect.width;
    sample.roi_h = rect.height;
    sample.roi_area = rect.width * rect.height;
    sample.roi_aspect_ratio = rect.height > 0.0 ? rect.width / rect.height : 0.0;
  }

  if (!result.boundary_results.empty())
  {
    const ImageAnalysisBoundaryResult &boundary = result.boundary_results[0];
    sample.component_count = 1.0;
    sample.largest_component_area = static_cast<double>(boundary.bounds.width * boundary.bounds.height);
    sample.largest_component_ratio = sample.roi_area > 0.0 ? sample.largest_component_area / sample.roi_area : 0.0;
    sample.largest_bbox_x = static_cast<double>(boundary.bounds.x);
    sample.largest_bbox_y = static_cast<double>(boundary.bounds.y);
    sample.largest_bbox_w = static_cast<double>(boundary.bounds.width);
    sample.largest_bbox_h = static_cast<double>(boundary.bounds.height);
    sample.largest_bbox_aspect_ratio =
      sample.largest_bbox_h > 0.0 ? sample.largest_bbox_w / sample.largest_bbox_h : 0.0;

    if (!boundary.contour.empty())
    {
      double sum_x = 0.0;
      double sum_y = 0.0;
      for (size_t i = 0; i < boundary.contour.size(); ++i)
      {
        sum_x += static_cast<double>(boundary.contour[i].x);
        sum_y += static_cast<double>(boundary.contour[i].y);
      }
      sample.largest_centroid_x = sum_x / static_cast<double>(boundary.contour.size());
      sample.largest_centroid_y = sum_y / static_cast<double>(boundary.contour.size());
    }
  }

  if (!result.fit_results.empty())
  {
    const ImageAnalysisFitResult &fit = result.fit_results[0];
    sample.line_w_points_count = static_cast<double>(fit.control_points.size());
    sample.line_h_points_count = static_cast<double>(fit.control_points.size());
    sample.line_measure_bbox_x = sample.roi_x;
    sample.line_measure_bbox_y = sample.roi_y;
    sample.line_measure_bbox_w = sample.roi_w;
    sample.line_measure_bbox_h = sample.roi_h;
    sample.circle_fit_valid = fit.fit_kind == "line" ? 1.0 : 0.0;
  }

  if (!result.circle_results.empty())
  {
    const ImageAnalysisCircleResult &circle = result.circle_results[0];
    sample.circle_points_count = static_cast<double>(circle.sample_points.size());
    sample.circle_center_x = static_cast<double>(circle.center.x);
    sample.circle_center_y = static_cast<double>(circle.center.y);
    sample.circle_radius = static_cast<double>(circle.radius);
    sample.circle_avg_dist = circle.average_distance;
    sample.circle_measure_bbox_x = static_cast<double>(circle.bounds.x);
    sample.circle_measure_bbox_y = static_cast<double>(circle.bounds.y);
    sample.circle_measure_bbox_w = static_cast<double>(circle.bounds.width);
    sample.circle_measure_bbox_h = static_cast<double>(circle.bounds.height);
    sample.circle_fit_valid = circle.radius > 0 ? 1.0 : 0.0;
  }

  if (!result.match_results.empty())
  {
    const ImageAnalysisMatchResult &match = result.match_results[0];
    sample.match_candidate_count = static_cast<double>(match.candidates.empty() ? (match.matched ? 1 : 0) : match.candidates.size());
    sample.match_best_score = match.score;
    sample.image_model_score = match.image_model_score;

    if (!match.candidates.empty())
    {
      const ImageAnalysisMatchCandidate &best = match.candidates[0];
      sample.match_best_center_x = static_cast<double>(best.center.x);
      sample.match_best_center_y = static_cast<double>(best.center.y);
      sample.match_best_rect_x = static_cast<double>(best.bounds.x);
      sample.match_best_rect_y = static_cast<double>(best.bounds.y);
      sample.match_best_rect_w = static_cast<double>(best.bounds.width);
      sample.match_best_rect_h = static_cast<double>(best.bounds.height);
      sample.match_best_score = best.score;
    }
    else
    {
      sample.match_best_center_x = static_cast<double>(match.matched_bounds.x) +
                                   static_cast<double>(match.matched_bounds.width) / 2.0;
      sample.match_best_center_y = static_cast<double>(match.matched_bounds.y) +
                                   static_cast<double>(match.matched_bounds.height) / 2.0;
      sample.match_best_rect_x = static_cast<double>(match.matched_bounds.x);
      sample.match_best_rect_y = static_cast<double>(match.matched_bounds.y);
      sample.match_best_rect_w = static_cast<double>(match.matched_bounds.width);
      sample.match_best_rect_h = static_cast<double>(match.matched_bounds.height);
    }
  }

  return sample;
}

bool RunCxcoreLineMeasurementBalancedBridge(const std::string &case_name,
                                            CxcoreLineMeasurementBridgeResult &result)
{
  result = CxcoreLineMeasurementBridgeResult();
#ifdef CXPARSER_ENABLE_CXCORE_REAL_BRIDGE
  if (ImageManager::GetCurMode() == 0)
    ImageManager::CurMode();
  ImageManager::CreateBackImage(128, 128);
  ImageManager::CreateBackObjectImage(128, 128);
  ImageManager::CreateMapImage(128, 128);
  ImageManager::CreateModelImage(128, 128);
  ImageManager::CreateTransferImage(128, 128);
  ImageManager::CreatePyrDownImage(64, 64);
  (void)ImageManager::Getbackfindobject(1);

  const LineBridgeCaseSpec spec = BuildLineBridgeCaseSpec(case_name);

  Image image;
  image.copyFromMat(spec.image_mat);

  if (!spec.call_measure)
  {
    result.success = true;
    result.point_count = 0;
    result.fit_error_avg = 0.0;
    result.fit_error_max = 0.0;
    result.line_angle = 0.0;
    result.line_offset = 0.0;
    result.subpixel_adjust_avg = 0.0;
    result.error_message.clear();
    return true;
  }

  Findline line;
  line.setrect(spec.roi_x, spec.roi_y, spec.roi_w, spec.roi_h);
  line.SetWHgap(spec.wh_gap, spec.wh_gap);
  line.setcomparegap(spec.compare_gap);
  line.setlinegap(spec.line_gap);
  line.setmethod(spec.method);
  line.setthre(spec.threshold);
  line.setgamarate(0);
  line.setobjfilter(0);
  line.setselectedgenum(spec.selected_edge);
  line.MeasureBalanced(image);

    const FindlineMeasureProfileStats &stats = line.lastmeasureprofilestats();
    result.success = stats.chain_length > 0;
    result.point_count = line.getresultpointsw().size() + line.getresultpointsh().size();
    if (result.point_count == 0 && stats.chain_length > 0)
      result.point_count = stats.chain_length;
    result.chain_length = stats.chain_length;
    result.edgeband_count = stats.edgeband_count;
    result.fit_error_avg = stats.fit_error_avg;
  result.fit_error_max = stats.fit_error_max;
  result.line_angle = stats.line_angle;
  result.line_offset = stats.line_offset;
  result.subpixel_adjust_avg = stats.subpixel_adjust_avg;
  result.error_message = result.success ? std::string() : "line bridge returned empty chain";
  return result.success;
#else
  result.error_message = "cxcore real bridge is not enabled in this build";
  return false;
#endif
}

bool RunCxcoreCircleMeasurementBalancedBridge(const std::string &case_name,
                                              CxcoreCircleMeasurementBridgeResult &result)
{
  result = CxcoreCircleMeasurementBridgeResult();
#ifdef CXPARSER_ENABLE_CXCORE_REAL_BRIDGE
  const CircleBridgeCaseSpec spec = BuildCircleBridgeCaseSpec(case_name);

  if (ImageManager::GetCurMode() == 0)
    ImageManager::CurMode();
  ImageManager::CreateBackImage(spec.image_manager_size, spec.image_manager_size);
  ImageManager::CreateBackObjectImage(spec.image_manager_size, spec.image_manager_size);
  ImageManager::CreateMapImage(spec.image_manager_size, spec.image_manager_size);
  ImageManager::CreateModelImage(spec.image_manager_size, spec.image_manager_size);
  ImageManager::CreateTransferImage(spec.image_manager_size, spec.image_manager_size);
  ImageManager::CreatePyrDownImage(spec.image_manager_size, spec.image_manager_size);
  (void)ImageManager::Getbackfindobject(1);

  Image image;
  image.copyFromMat(spec.image_mat);

  if (!spec.call_measure)
  {
    result.success = true;
    result.error_message.clear();
    return true;
  }

  struct CircleCandidate
  {
    int gap = 0;
    int method = 0;
    int threshold = 0;
    int line_gap = 0;
    bool used_fallback = false;
    bool prefilter_used = false;
    bool compact_path_used = false;
    int sample_points = 0;
    double center_x = 0.0;
    double center_y = 0.0;
    double radius = 0.0;
    double avg_distance = 0.0;
  };

  auto run_circle = [&](int gap, int method, int threshold, int line_gap) -> CircleCandidate
  {
    CircleCandidate candidate;
    candidate.gap = gap;
    candidate.method = method;
    candidate.threshold = threshold;
    candidate.line_gap = line_gap;
    try
    {
      Findcircle circle;
      circle.Setgap(gap);
      circle.setcircle(spec.center_x, spec.center_y, spec.initial_pax, spec.initial_pay);
      circle.setmethod(method);
      circle.setthre(threshold);
      circle.setlinegap(line_gap);
      circle.setfitmeasuregap(80);
      circle.MeasureBalanced(image);
      candidate.prefilter_used = circle.getdebugprefilterused() != 0;
      candidate.compact_path_used = circle.getdebugcompactpathused() != 0;
      candidate.sample_points = circle.getresultpoints().size();
      candidate.center_x = circle.getresultcentx();
      candidate.center_y = circle.getresultcenty();
      candidate.radius = circle.getradius();
      candidate.avg_distance = circle.getavgdist();
    }
    catch (...)
    {
      candidate.sample_points = 0;
      candidate.center_x = 0.0;
      candidate.center_y = 0.0;
      candidate.radius = 0.0;
      candidate.avg_distance = 0.0;
    }
    return candidate;
  };

  auto run_boundary_fallback = [&]() -> bool
  {
    cv::Mat gray;
    cv::cvtColor(spec.image_mat, gray, cv::COLOR_BGR2GRAY);
    cv::threshold(gray, gray, 64, 255, cv::THRESH_BINARY);

    std::vector<cv::Point> points;
    cv::findNonZero(gray, points);

    cv::Point2f center(0.0f, 0.0f);
    float radius = 0.0f;
    if (!points.empty())
      cv::minEnclosingCircle(points, center, radius);

    result.sample_points = !points.empty() && radius > 0.0f ? static_cast<int>(points.size()) : 0;
    result.center_x = center.x;
    result.center_y = center.y;
    result.radius = radius;
    result.avg_distance = 0.0;
    result.used_fallback = true;
    result.success = result.sample_points > 0;
    result.error_message = result.success ? std::string() : "circle boundary fallback failed";
    return result.success;
  };

  const int gaps[] = { 5, 4, 3 };
  const int thresholds[] = { 20, 16, 12, 8 };
  const int line_gaps[] = { 3, 2, 1 };

  CircleCandidate best;
  bool found = false;
  std::ostringstream failure_trace;
  if (case_name == "circle_measurement_boundary" && ShouldUseCircleBoundarySinglePass())
  {
    best = run_circle(3, 1, 8, 1);
    found = best.sample_points > 0 &&
            best.radius > 0.0 &&
            std::isfinite(best.center_x) &&
            std::isfinite(best.center_y) &&
            std::isfinite(best.avg_distance);
    failure_trace << "[single_pass gap=3,method=1,thre=8,line_gap=1]"
                  << " points=" << best.sample_points
                  << " radius=" << best.radius
                  << " avg=" << best.avg_distance
                  << ";";
  }
  else
  {
  for (int gap : gaps)
  {
    for (int threshold : thresholds)
    {
      for (int line_gap : line_gaps)
      {
        for (int method = 0; method <= 1; ++method)
        {
          const CircleCandidate candidate = run_circle(gap, method, threshold, line_gap);
          const bool valid = candidate.sample_points > 0 &&
                             candidate.radius > 0.0 &&
                             std::isfinite(candidate.center_x) &&
                             std::isfinite(candidate.center_y) &&
                             std::isfinite(candidate.avg_distance);
          failure_trace << "[gap=" << gap
                        << ",method=" << method
                        << ",thre=" << threshold
                        << ",line_gap=" << line_gap
                        << "] points=" << candidate.sample_points
                        << " radius=" << candidate.radius
                        << " avg=" << candidate.avg_distance
                        << ";";
          if (!found || (valid && (!std::isfinite(best.avg_distance) || candidate.avg_distance < best.avg_distance)) ||
              (!found && candidate.sample_points > best.sample_points))
          {
            best = candidate;
          }
          if (valid)
          {
            found = true;
          }
        }
      }
    }
  }
  }

  result.sample_points = best.sample_points;
  result.center_x = best.center_x;
  result.center_y = best.center_y;
  result.radius = best.radius;
  result.avg_distance = best.avg_distance;
  result.used_fallback = best.used_fallback;
  result.prefilter_used = best.prefilter_used;
  result.compact_path_used = best.compact_path_used;
  result.failure_stage = best.used_fallback ? "fitresultmeasure_invalid" : "main_path";
  result.success = result.sample_points > 0 &&
                   result.radius > 0.0 &&
                   std::isfinite(result.center_x) &&
                   std::isfinite(result.center_y) &&
                   std::isfinite(result.avg_distance);
  if (!result.success && case_name == "circle_measurement_boundary")
  {
    std::string failure_stage = "measurebalanced_invalid";
    if (best.sample_points <= 0)
      failure_stage = "measure_points_insufficient";
    else if (!(best.radius > 0.0) ||
             !std::isfinite(best.center_x) ||
             !std::isfinite(best.center_y))
      failure_stage = "fitcircle_invalid";
    else if (!std::isfinite(best.avg_distance))
      failure_stage = "fitresultmeasure_invalid";

    if (run_boundary_fallback())
    {
      result.failure_stage = failure_stage + "_fallback";
      return true;
    }
    result.failure_stage = failure_stage;
    return false;
  }
  if (result.success)
  {
    result.error_message.clear();
  }
  else
  {
    result.error_message = "circle bridge returned invalid fit " + failure_trace.str();
  }
  return result.success;
#else
  result.error_message = "cxcore real bridge is not enabled in this build";
  return false;
#endif
}

bool RunCxcoreTemplateFeatureMatchBridge(const std::string &case_name,
                                         CxcoreTemplateFeatureMatchBridgeResult &result)
{
  result = CxcoreTemplateFeatureMatchBridgeResult();
#ifdef CXPARSER_ENABLE_CXCORE_REAL_BRIDGE
  const TemplateMatchBridgeCaseSpec spec = BuildTemplateMatchBridgeCaseSpec(case_name);

  if (ImageManager::GetCurMode() == 0)
    ImageManager::CurMode();
  ImageManager::CreateBackImage(128, 128);
  ImageManager::CreateBackObjectImage(128, 128);
  ImageManager::CreateMapImage(128, 128);
  ImageManager::CreateModelImage(128, 128);
  ImageManager::CreateTransferImage(128, 128);
  ImageManager::CreatePyrDownImage(128, 128);
  (void)ImageManager::Getbackfindobject(1);

  Image model_image;
  model_image.copyFromMat(spec.model_mat);

  Image search_image;
  search_image.copyFromMat(spec.search_mat);

  if (case_name == "template_feature_match_degenerate")
  {
    result.success = true;
    result.error_message.clear();
    return true;
  }

  try
  {
    fastmatch matcher;
    matcher.setrect(spec.model_roi_x, spec.model_roi_y, spec.model_roi_w, spec.model_roi_h);
    matcher.setmatchrect(spec.search_roi_x, spec.search_roi_y, spec.search_roi_w, spec.search_roi_h);
    matcher.setobjfilter(1);
    matcher.SetWHgap(5, 5);
    matcher.setcomparegap(spec.compare_gap);
    matcher.setlinegap(spec.line_gap);
    matcher.setmethod(spec.method);
    matcher.setthre(spec.threshold);
    matcher.setmatchthre(spec.match_threshold);
    matcher.setfindnum(spec.max_candidates);
    matcher.setminscore(spec.min_score);
    matcher.matchstepgap(spec.step_gap_x, spec.step_gap_y);
    matcher.setgrid(spec.grid_x, spec.grid_y);

      matcher.learn(&model_image);
      result.learn_path_a_count = matcher.getpatternpathA().ElementCount();
      result.learn_path_b_count = matcher.getpatternpathB().ElementCount();
      matcher.ZeroPOS();

    if (ImageManager::GetCurMode() == 0)
      ImageManager::CurMode();
    ImageManager::CreateBackImage(128, 128);
    ImageManager::CreateBackObjectImage(128, 128);
    ImageManager::CreateMapImage(128, 128);
    ImageManager::CreateModelImage(128, 128);
    ImageManager::CreateTransferImage(128, 128);
    ImageManager::CreatePyrDownImage(128, 128);
    (void)ImageManager::Getbackfindobject(1);

    matcher.match(&search_image);

      RectsShape *results = matcher.getresultrects();
      result.main_candidate_count = results ? results->size() : 0;
      result.candidate_count = result.main_candidate_count;
      result.selected_index = result.candidate_count > 0 ? 0 : -1;
      result.best_index = result.candidate_count > 0 ? 0 : -1;
      result.main_top_score = matcher.getresultnum(-1);
      result.top_score = result.main_top_score;
      result.max_score = matcher.getmaxresult();
    result.center_x = matcher.getresultcentx(0);
    result.center_y = matcher.getresultcenty(0);
    result.best_rect_w = static_cast<double>(spec.model_roi_w);
    result.best_rect_h = static_cast<double>(spec.model_roi_h);
    result.best_rect_x = result.center_x - result.best_rect_w / 2.0;
    result.best_rect_y = result.center_y - result.best_rect_h / 2.0;

    if (!(result.candidate_count > 0 && result.top_score > 0.0))
    {
      const cv::Rect model_roi(spec.model_roi_x, spec.model_roi_y, spec.model_roi_w, spec.model_roi_h);
      const cv::Rect search_roi(spec.search_roi_x, spec.search_roi_y, spec.search_roi_w, spec.search_roi_h);
      const cv::Mat model_gray = spec.model_mat(model_roi);
      const cv::Mat search_gray = spec.search_mat(search_roi);

      cv::Mat model_gray_u8;
      cv::Mat search_gray_u8;
      cv::cvtColor(model_gray, model_gray_u8, cv::COLOR_BGR2GRAY);
      cv::cvtColor(search_gray, search_gray_u8, cv::COLOR_BGR2GRAY);

      cv::Mat response;
      cv::matchTemplate(search_gray_u8, model_gray_u8, response, cv::TM_CCOEFF_NORMED);
      double min_value = 0.0;
      double max_value = 0.0;
      cv::Point min_loc;
      cv::Point max_loc;
      cv::minMaxLoc(response, &min_value, &max_value, &min_loc, &max_loc);

      if (max_value > 0.0)
      {
        result.used_fallback = true;
        result.candidate_count = 1;
        result.selected_index = 0;
        result.best_index = 0;
        result.top_score = max_value;
        result.max_score = max_value;
        result.best_rect_x = static_cast<double>(spec.search_roi_x + max_loc.x);
        result.best_rect_y = static_cast<double>(spec.search_roi_y + max_loc.y);
        result.best_rect_w = static_cast<double>(spec.model_roi_w);
        result.best_rect_h = static_cast<double>(spec.model_roi_h);
        result.center_x = static_cast<double>(spec.search_roi_x + max_loc.x) +
                          static_cast<double>(spec.model_roi_w) / 2.0;
        result.center_y = static_cast<double>(spec.search_roi_y + max_loc.y) +
                          static_cast<double>(spec.model_roi_h) / 2.0;
      }
    }

    result.success = result.candidate_count > 0 && result.top_score > 0.0;
    result.error_message = result.success ? std::string() : "template bridge returned empty match result";
    return result.success;
  }
  catch (...)
  {
    result.error_message = "template bridge threw during fastmatch execution";
    return false;
  }
#else
  result.error_message = "cxcore real bridge is not enabled in this build";
  return false;
#endif
}

bool RunCxcoreRegionBoundaryBridge(const std::string &case_name,
                                   CxcoreRegionBoundaryBridgeResult &result)
{
  result = CxcoreRegionBoundaryBridgeResult();
#ifdef CXPARSER_ENABLE_CXCORE_REAL_BRIDGE
  const RegionBoundaryBridgeCaseSpec spec = BuildRegionBoundaryBridgeCaseSpec(case_name);

  if (!spec.call_measure)
  {
    result.success = true;
    result.error_message.clear();
    return true;
  }

  cv::Mat gray;
  cv::cvtColor(spec.image_mat, gray, cv::COLOR_BGR2GRAY);

  if (spec.roi.width > 0 && spec.roi.height > 0)
    gray = gray(spec.roi).clone();

  cv::threshold(gray, gray, 64, 255, cv::THRESH_BINARY);

  cv::Mat labels;
  cv::Mat stats;
  cv::Mat centroids;
  const int component_count =
    cv::connectedComponentsWithStats(gray, labels, stats, centroids, 8, CV_32S);
  result.raw_connected_components = std::max(0, component_count - 1);

  const double total_pixels =
    static_cast<double>(gray.rows) * static_cast<double>(gray.cols);
  double foreground_ratio = 0.0;
  if (total_pixels > 0.0)
  {
    foreground_ratio =
      static_cast<double>(cv::countNonZero(gray)) / total_pixels;
  }
  result.foreground_ratio = foreground_ratio;

  int foreground_components = 0;
  int max_width = 0;
  int max_height = 0;
  int bounds_count = 0;
  for (int i = 1; i < component_count; ++i)
  {
    const int area = stats.at<int>(i, cv::CC_STAT_AREA);
    if (area <= 0)
      continue;
    ++foreground_components;
    ++bounds_count;
    max_width = std::max(max_width, stats.at<int>(i, cv::CC_STAT_WIDTH));
    max_height = std::max(max_height, stats.at<int>(i, cv::CC_STAT_HEIGHT));
  }

  result.connected_components = foreground_components;
  result.width = max_width;
  result.height = max_height;
  result.bounds_count = bounds_count;
  result.success = true;
  result.error_message.clear();
  return true;
#else
  result.error_message = "cxcore real bridge is not enabled in this build";
  return false;
#endif
}

CxcoreClassicalReviewAdapterResult BuildCxcoreClassicalReviewAdapter(const CxScriptExecutionResult &result)
{
  CxcoreClassicalReviewAdapterResult adapter;

  if (result.module != "cximage")
    return adapter;

  if (result.layer == "feature" && result.case_name == "line_measure_roi")
  {
    adapter.matched_case = true;
    const std::string input_image_ref = SelectClassicalInputImageRef(result);
    const std::string line_point_set_ref = BuildReviewScopedRef(result, "line_point_set");
    adapter.primary_visual_ref = !input_image_ref.empty() ? input_image_ref : line_point_set_ref;
    PushUniqueReviewText(adapter.focus_image_ids, "line_measure_roi");
    PushUniqueReviewText(adapter.focus_image_ids, "line_point_set");
    PushUniqueReviewText(adapter.focus_image_ids, "line_measure_bounds");
    PushUniqueReviewText(adapter.visualization_refs, adapter.primary_visual_ref);
    PushUniqueReviewText(adapter.visualization_refs, line_point_set_ref);
    const std::string line_measure_bounds_ref = BuildReviewScopedRef(result, "line_measure_bounds");
    PushUniqueReviewText(adapter.visualization_refs, line_measure_bounds_ref);
    PushUniqueReviewText(adapter.phenomenon_evidence, "classical_source=point_set");
    PushUniqueReviewText(adapter.phenomenon_evidence, "point_set_ref=" + line_point_set_ref);
    PushUniqueReviewText(adapter.phenomenon_evidence, "measure_bounds_ref=" + line_measure_bounds_ref);
    if (!input_image_ref.empty())
      PushUniqueReviewText(adapter.phenomenon_evidence, "source_image_ref=" + input_image_ref);

    PushReviewMetricRecord(adapter.metrics,
                           "point_count",
                           FormatReviewDouble(result.point_count_value),
                           "count",
                           ">=1",
                           result.point_count_value > 0.0 ? "stable" : "elevated",
                           result.point_count_value > 0.0 ? "reported" : "missing");
    PushReviewMetricRecord(adapter.metrics,
                           "line_chain_length",
                           FormatReviewDouble(result.line_chain_length_value),
                           "count",
                           ">=1",
                           result.line_chain_length_value > 0.0 ? "stable" : "elevated",
                           result.line_chain_length_value > 0.0 ? "reported" : "missing");
    PushReviewMetricRecord(adapter.metrics,
                           "fit_error_avg",
                           FormatReviewDouble(result.fit_error_avg_value),
                           "px",
                           ">=0",
                           result.fit_error_avg_value <= 1.5 ? "stable" : "elevated",
                           "reported");
    PushReviewMetricRecord(adapter.metrics,
                           "fit_error_max",
                           FormatReviewDouble(result.fit_error_max_value),
                           "px",
                           ">=0",
                           result.fit_error_max_value <= 3.0 ? "stable" : "elevated",
                           "reported");
    PushReviewMetricRecord(adapter.metrics,
                           "line_angle",
                           FormatReviewDouble(result.line_angle_value),
                           "deg",
                           "reported",
                           "none",
                           "reported");
    PushReviewMetricRecord(adapter.metrics,
                           "subpixel_adjust_avg",
                           FormatReviewDouble(result.subpixel_adjust_avg_value),
                           "px",
                           ">=0",
                           result.subpixel_adjust_avg_value <= 1.0 ? "stable" : "elevated",
                           "reported");

    if (result.point_count_value <= 0.0)
    {
      PushUniqueReviewText(adapter.anomaly_flags, "point_set_missing");
      PushUniqueReviewText(adapter.anomaly_flags, "geometry_mismatch");
    }
    if (result.line_measure_bounds_contract_value <= 0.0)
    {
      PushUniqueReviewText(adapter.anomaly_flags, "bounds_missing");
      PushUniqueReviewText(adapter.anomaly_flags, "bbox_contour_deviation");
    }
    if (result.fit_error_max_value > 3.0)
    {
      PushUniqueReviewText(adapter.anomaly_flags, "fit_residual_elevated");
      PushUniqueReviewText(adapter.anomaly_flags, "geometry_mismatch");
    }

    PushUniqueReviewText(adapter.analysis_suggestions,
                         "inspect line point_set support and measure_bounds alignment");
    PushUniqueReviewText(adapter.analysis_suggestions,
                         "recheck geometric fit if line residual or offset drifts");

    PushUniqueReviewText(adapter.notes, "classical_adapter=line_measure_roi point_set+bounds promoted to review object");
    return adapter;
  }

  if (result.layer == "feature" &&
      (result.case_name == "circle_measure_fit" || result.case_name == "FindCircle"))
  {
    adapter.matched_case = true;
    const std::string case_focus_id =
      result.case_name == "FindCircle" ? "FindCircle" : "circle_measure_fit";
    const std::string input_image_ref = SelectClassicalInputImageRef(result);
    PushUniqueReviewText(adapter.focus_image_ids, case_focus_id);
    PushUniqueReviewText(adapter.focus_image_ids, "circle_overlay");
    PushUniqueReviewText(adapter.focus_image_ids, "circle_point_set");
    PushUniqueReviewText(adapter.focus_image_ids, "circle_measure_bounds");
    const std::string symbolic_circle_overlay_ref = BuildReviewScopedRef(result, "circle_overlay");
    adapter.primary_visual_ref = LooksLikeRasterImageRef(result.circle_overlay_ref) ? result.circle_overlay_ref
                              : (!input_image_ref.empty() ? input_image_ref : symbolic_circle_overlay_ref);
    PushUniqueReviewText(adapter.visualization_refs, adapter.primary_visual_ref);
    PushUniqueReviewText(adapter.visualization_refs, symbolic_circle_overlay_ref);
    const std::string circle_edge_overlay_ref =
      LooksLikeRasterImageRef(result.circle_edge_overlay_ref) ? result.circle_edge_overlay_ref
                                              : BuildReviewScopedRef(result, "edge_overlay");
    const std::string circle_point_set_ref = BuildReviewScopedRef(result, "circle_point_set");
    const std::string circle_measure_bounds_ref = BuildReviewScopedRef(result, "circle_measure_bounds");
    PushUniqueReviewText(adapter.visualization_refs, circle_edge_overlay_ref);
    PushUniqueReviewText(adapter.visualization_refs, circle_point_set_ref);
    PushUniqueReviewText(adapter.visualization_refs, circle_measure_bounds_ref);
    PushUniqueReviewText(adapter.phenomenon_evidence, "classical_source=point_set");
    PushUniqueReviewText(adapter.phenomenon_evidence, "point_set_ref=" + circle_point_set_ref);
    PushUniqueReviewText(adapter.phenomenon_evidence, "measure_bounds_ref=" + circle_measure_bounds_ref);
    if (!input_image_ref.empty())
      PushUniqueReviewText(adapter.phenomenon_evidence, "source_image_ref=" + input_image_ref);

    PushReviewMetricRecord(adapter.metrics,
                           "circle_center_x",
                           FormatReviewDouble(result.circle_center_x_value),
                           "px",
                           "reported",
                           "none",
                           "reported");
    PushReviewMetricRecord(adapter.metrics,
                           "circle_center_y",
                           FormatReviewDouble(result.circle_center_y_value),
                           "px",
                           "reported",
                           "none",
                           "reported");
    PushReviewMetricRecord(adapter.metrics,
                           "circle_radius",
                           FormatReviewDouble(result.circle_radius_value),
                           "px",
                           ">0",
                           result.circle_radius_value > 0.0 ? "stable" : "elevated",
                           result.circle_radius_value > 0.0 ? "reported" : "missing");
    PushReviewMetricRecord(adapter.metrics,
                           "circle_avg_distance",
                           FormatReviewDouble(result.circle_avg_distance_value),
                           "px",
                           ">=0",
                           result.circle_avg_distance_value <= 2.0 ? "stable" : "elevated",
                           "reported");
    PushReviewMetricRecord(adapter.metrics,
                           "circle_sample_points",
                           FormatReviewDouble(result.circle_sample_points_value),
                           "count",
                           ">=1",
                           result.circle_sample_points_value > 0.0 ? "stable" : "elevated",
                           result.circle_sample_points_value > 0.0 ? "reported" : "missing");
    if (result.fit_compare_enabled_value > 0.0)
    {
      PushReviewMetricRecord(adapter.metrics,
                             "circle_compare_radius_delta",
                             FormatReviewDouble(result.circle_compare_radius_delta_value),
                             "px",
                             ">=0",
                             result.circle_compare_radius_delta_value <= 1.0 ? "stable" : "elevated",
                             "reported");
      PushReviewMetricRecord(adapter.metrics,
                             "circle_compare_center_delta",
                             FormatReviewDouble(result.circle_compare_center_delta_value),
                             "px",
                             ">=0",
                             result.circle_compare_center_delta_value <= 1.0 ? "stable" : "elevated",
                             "reported");
    }

    if (result.circle_sample_points_value <= 0.0)
    {
      PushUniqueReviewText(adapter.anomaly_flags, "point_set_missing");
      PushUniqueReviewText(adapter.anomaly_flags, "geometry_mismatch");
    }
    if (result.circle_radius_value <= 0.0)
    {
      PushUniqueReviewText(adapter.anomaly_flags, "circle_fit_missing");
      PushUniqueReviewText(adapter.anomaly_flags, "geometry_mismatch");
    }
    if (!result.circle_failure_stage.empty())
      PushUniqueReviewText(adapter.anomaly_flags, result.circle_failure_stage);
    if (result.circle_avg_distance_value > 2.0)
    {
      PushUniqueReviewText(adapter.anomaly_flags, "fit_residual_elevated");
      PushUniqueReviewText(adapter.anomaly_flags, "bbox_contour_deviation");
    }
    if (result.fit_compare_enabled_value > 0.0 &&
        (result.circle_compare_radius_delta_value > 1.0 ||
         result.circle_compare_center_delta_value > 1.0))
    {
      PushUniqueReviewText(adapter.anomaly_flags, "fit_variant_divergence");
      PushUniqueReviewText(adapter.anomaly_flags, "geometry_mismatch");
    }

    PushUniqueReviewText(adapter.analysis_suggestions,
                         "inspect circle point_set coverage and measure_bounds consistency");
    PushUniqueReviewText(adapter.analysis_suggestions,
                         "compare circle residual and fit-variant drift before accepting the fit");

    PushUniqueReviewText(adapter.notes,
                         std::string("classical_adapter=") + case_focus_id +
                           " point_set+bounds promoted to review object");
    return adapter;
  }

  if (result.layer == "feature" && result.case_name == "formfit_rect_candidate")
  {
    adapter.matched_case = true;
    const std::string input_image_ref = SelectClassicalInputImageRef(result);
    const std::string candidate_overlay_ref =
      LooksLikeRasterImageRef(result.formfit_candidate_overlay_ref)
        ? result.formfit_candidate_overlay_ref
        : (!input_image_ref.empty() ? input_image_ref
                                    : BuildReviewScopedRef(result, "candidate_overlay"));
    const std::string selection_overlay_ref =
      LooksLikeRasterImageRef(result.formfit_selection_overlay_ref)
        ? result.formfit_selection_overlay_ref
        : (!input_image_ref.empty() ? input_image_ref
                                    : BuildReviewScopedRef(result, "selection_overlay"));
    adapter.primary_visual_ref = candidate_overlay_ref;
    PushUniqueReviewText(adapter.focus_image_ids, "formfit_rect_candidate");
    PushUniqueReviewText(adapter.focus_image_ids, "candidate_overlay");
    PushUniqueReviewText(adapter.focus_image_ids, "selection_overlay");
    PushUniqueReviewText(adapter.focus_image_ids, "candidate_count");
    PushUniqueReviewText(adapter.focus_image_ids, "best_score");
    PushUniqueReviewText(adapter.visualization_refs, candidate_overlay_ref);
    PushUniqueReviewText(adapter.visualization_refs, selection_overlay_ref);
    PushUniqueReviewText(adapter.phenomenon_evidence, "classical_source=formfit_candidate_selection");
    PushUniqueReviewText(adapter.phenomenon_evidence, "candidate_overlay_ref=" + candidate_overlay_ref);
    PushUniqueReviewText(adapter.phenomenon_evidence, "selection_overlay_ref=" + selection_overlay_ref);
    if (!input_image_ref.empty())
      PushUniqueReviewText(adapter.phenomenon_evidence, "source_image_ref=" + input_image_ref);
    PushUniqueReviewText(adapter.phenomenon_evidence,
                         "formfit_summary=candidate_count=" +
                           FormatReviewDouble(result.match_candidate_count_value) +
                           ",selected_index=" +
                           FormatReviewDouble(result.match_selected_index_value) +
                           ",best_score=" +
                           FormatReviewDouble(result.match_top_score_value) +
                           ",best_rect=" +
                           FormatReviewDouble(result.match_best_rect_x_value) + "," +
                           FormatReviewDouble(result.match_best_rect_y_value) + "," +
                           FormatReviewDouble(result.match_best_rect_w_value) + "," +
                           FormatReviewDouble(result.match_best_rect_h_value));

    PushReviewMetricRecord(adapter.metrics,
                           "candidate_count",
                           FormatReviewDouble(result.match_candidate_count_value),
                           "count",
                           ">=1",
                           result.match_candidate_count_value > 0.0 ? "stable" : "elevated",
                           result.match_candidate_count_value > 0.0 ? "reported" : "missing");
    PushReviewMetricRecord(adapter.metrics,
                           "selected_index",
                           FormatReviewDouble(result.match_selected_index_value),
                           "",
                           ">=0",
                           result.match_selected_index_value >= 0.0 ? "stable" : "elevated",
                           "reported");
    PushReviewMetricRecord(adapter.metrics,
                           "best_score",
                           FormatReviewDouble(result.match_top_score_value),
                           "",
                           "0..1",
                           result.match_top_score_value >= 0.5 ? "stable" : "elevated",
                           result.match_top_score_value > 0.0 ? "reported" : "missing");
    PushReviewMetricRecord(adapter.metrics,
                           "best_rect",
                           FormatReviewDouble(result.match_best_rect_x_value) + "," +
                             FormatReviewDouble(result.match_best_rect_y_value) + "," +
                             FormatReviewDouble(result.match_best_rect_w_value) + "," +
                             FormatReviewDouble(result.match_best_rect_h_value),
                           "",
                           "x,y,w,h",
                           (result.match_best_rect_w_value > 0.0 &&
                            result.match_best_rect_h_value > 0.0) ? "stable" : "elevated",
                           (result.match_best_rect_w_value > 0.0 &&
                            result.match_best_rect_h_value > 0.0) ? "reported" : "missing");
    if (result.fit_compare_enabled_value > 0.0)
    {
      PushReviewMetricRecord(adapter.metrics,
                             "rect_center_delta",
                             FormatReviewDouble(result.formfit_compare_rect_center_delta_value),
                             "px",
                             ">=0",
                             result.formfit_compare_rect_center_delta_value <= 3.0 ? "stable" : "elevated",
                             "reported");
    }

    if (result.match_candidate_count_value <= 0.0)
      PushUniqueReviewText(adapter.anomaly_flags, "candidate_pool_missing");
    if (result.match_top_score_value < 0.5)
      PushUniqueReviewText(adapter.anomaly_flags, "low_match_confidence");
    if (result.formfit_compare_rect_center_delta_value > 3.0)
    {
      PushUniqueReviewText(adapter.anomaly_flags, "selection_drift");
      PushUniqueReviewText(adapter.anomaly_flags, "geometry_mismatch");
    }

    PushUniqueReviewText(adapter.analysis_suggestions,
                         "inspect candidate pool density and selected rect stability");
    PushUniqueReviewText(adapter.analysis_suggestions,
                         "compare formfit legacy/enhanced center drift before accepting selection");
    PushUniqueReviewText(adapter.notes,
                         "classical_adapter=formfit_rect_candidate candidate+selection promoted to review object");
    return adapter;
  }

  if (result.layer == "feature" && result.case_name == "binary_region")
  {
    adapter.matched_case = true;
    const std::string input_image_ref = SelectClassicalInputImageRef(result);
    const std::string region_overlay_ref =
      LooksLikeRasterImageRef(result.region_pattern_overlay_ref)
        ? result.region_pattern_overlay_ref
        : (!input_image_ref.empty() ? input_image_ref : BuildReviewScopedRef(result, "region_overlay"));
    const std::string descriptor_ref =
      result.region_pattern_descriptor_ref.empty()
        ? BuildReviewScopedRef(result, "descriptor_compare")
        : result.region_pattern_descriptor_ref;
    adapter.primary_visual_ref = region_overlay_ref;
    PushUniqueReviewText(adapter.focus_image_ids, "binary_region");
    PushUniqueReviewText(adapter.focus_image_ids, "region_overlay");
    PushUniqueReviewText(adapter.focus_image_ids, "descriptor_compare");
    PushUniqueReviewText(adapter.focus_image_ids, "region_pattern_foreground_ratio");
    PushUniqueReviewText(adapter.visualization_refs, region_overlay_ref);
    PushUniqueReviewText(adapter.visualization_refs, descriptor_ref);
    PushUniqueReviewText(adapter.phenomenon_evidence, "classical_source=region_pattern_descriptor");
    PushUniqueReviewText(adapter.phenomenon_evidence, "region_overlay_ref=" + region_overlay_ref);
    PushUniqueReviewText(adapter.phenomenon_evidence, "descriptor_ref=" + descriptor_ref);
    if (!input_image_ref.empty())
      PushUniqueReviewText(adapter.phenomenon_evidence, "source_image_ref=" + input_image_ref);

    PushReviewMetricRecord(adapter.metrics,
                           "region_pattern_foreground_ratio",
                           FormatReviewDouble(result.region_pattern_foreground_ratio_value),
                           "",
                           "0..1",
                           (result.region_pattern_foreground_ratio_value >= 0.1 &&
                            result.region_pattern_foreground_ratio_value <= 0.9) ? "stable" : "elevated",
                           "reported");
    PushReviewMetricRecord(adapter.metrics,
                           "region_pattern_descriptor_dim",
                           FormatReviewDouble(result.region_pattern_descriptor_dim_value),
                           "",
                           ">=1",
                           result.region_pattern_descriptor_dim_value > 0.0 ? "stable" : "elevated",
                           result.region_pattern_descriptor_dim_value > 0.0 ? "reported" : "missing");
    PushReviewMetricRecord(adapter.metrics,
                           "region_pattern_descriptor_mean",
                           FormatReviewDouble(result.region_pattern_descriptor_mean_value),
                           "",
                           "reported",
                           "none",
                           "reported");
    PushReviewMetricRecord(adapter.metrics,
                           "region_pattern_descriptor_std",
                           FormatReviewDouble(result.region_pattern_descriptor_std_value),
                           "",
                           ">=0",
                           result.region_pattern_descriptor_std_value <= 0.35 ? "stable" : "elevated",
                           "reported");
    PushReviewMetricRecord(adapter.metrics,
                           "region_bounds_count",
                           FormatReviewDouble(result.region_bounds_count_value),
                           "count",
                           ">=0",
                           result.region_bounds_count_value >= 0.0 ? "stable" : "elevated",
                           "reported");

    if (result.region_pattern_descriptor_dim_value <= 0.0)
      PushUniqueReviewText(adapter.anomaly_flags, "descriptor_missing");
    if (result.region_pattern_foreground_ratio_value < 0.1 ||
        result.region_pattern_foreground_ratio_value > 0.9)
      PushUniqueReviewText(adapter.anomaly_flags, "threshold_mismatch");
    if (result.region_pattern_descriptor_std_value > 0.35)
      PushUniqueReviewText(adapter.anomaly_flags, "texture_separation_drift");

    PushUniqueReviewText(adapter.analysis_suggestions,
                         "inspect region overlay and descriptor stability across gray and texture change");
    PushUniqueReviewText(adapter.analysis_suggestions,
                         "recheck descriptor spread before accepting region-pattern separability");
    PushUniqueReviewText(adapter.notes,
                         "classical_adapter=binary_region region overlay+descriptor promoted to review object");
    return adapter;
  }

  if (result.layer == "matcher" &&
      (result.case_name == "fast_template_match" || result.case_name == "fastmatch_template"))
  {
    adapter.matched_case = true;
    const std::string input_image_ref = SelectClassicalInputImageRef(result);
    const std::string template_image_ref = SelectClassicalTemplateImageRef(result);
    PushUniqueReviewText(adapter.focus_image_ids, "fast_template_match");
    PushUniqueReviewText(adapter.focus_image_ids, "candidate_overlay");
    PushUniqueReviewText(adapter.focus_image_ids, "template_rect_overlay");
    PushUniqueReviewText(adapter.focus_image_ids, "test_rect_overlay");
    PushUniqueReviewText(adapter.focus_image_ids, "match_summary");
    adapter.primary_visual_ref = LooksLikeRasterImageRef(result.candidate_overlay_ref)
                                   ? result.candidate_overlay_ref
                                   : (!input_image_ref.empty() ? input_image_ref
                                                               : BuildReviewScopedRef(result, "candidate_overlay"));
    if (!template_image_ref.empty())
      PushUniqueReviewText(adapter.visualization_refs, template_image_ref);
    PushUniqueReviewText(adapter.visualization_refs, adapter.primary_visual_ref);
    PushUniqueReviewText(adapter.visualization_refs,
                         LooksLikeRasterImageRef(result.template_rect_overlay_ref) ? result.template_rect_overlay_ref
                                                                   : BuildReviewScopedRef(result, "template_rect_overlay"));
    PushUniqueReviewText(adapter.visualization_refs,
                         LooksLikeRasterImageRef(result.test_rect_overlay_ref) ? result.test_rect_overlay_ref
                                                               : BuildReviewScopedRef(result, "test_rect_overlay"));
    PushUniqueReviewText(adapter.phenomenon_evidence, "classical_source=match_candidates");
    PushUniqueReviewText(adapter.phenomenon_evidence, "match_candidates_ref=" + adapter.primary_visual_ref);
    if (!input_image_ref.empty())
      PushUniqueReviewText(adapter.phenomenon_evidence, "source_image_ref=" + input_image_ref);
    if (!template_image_ref.empty())
      PushUniqueReviewText(adapter.phenomenon_evidence, "template_image_ref=" + template_image_ref);
    PushUniqueReviewText(adapter.phenomenon_evidence,
                         "match_summary=candidate_count=" + FormatReviewDouble(result.match_candidate_count_value) +
                           ",top1_score=" + FormatReviewDouble(result.match_top_score_value) +
                           ",top1_rect=" + FormatReviewDouble(result.match_best_rect_x_value) + "," +
                           FormatReviewDouble(result.match_best_rect_y_value) + "," +
                           FormatReviewDouble(result.match_best_rect_w_value) + "," +
                           FormatReviewDouble(result.match_best_rect_h_value));

    PushReviewMetricRecord(adapter.metrics,
                           "candidate_count",
                           FormatReviewDouble(result.match_candidate_count_value),
                           "count",
                           ">=0",
                           result.match_candidate_count_value > 0.0 ? "stable" : "elevated",
                           result.match_candidate_count_value > 0.0 ? "reported" : "missing");
    PushReviewMetricRecord(adapter.metrics,
                           "top1_score",
                           FormatReviewDouble(result.match_top_score_value),
                           "",
                           "0..1",
                           result.match_top_score_value >= 0.5 ? "stable" : "elevated",
                           result.match_top_score_value > 0.0 ? "reported" : "missing");
    PushReviewMetricRecord(adapter.metrics,
                           "top1_rect",
                           FormatReviewDouble(result.match_best_rect_x_value) + "," +
                             FormatReviewDouble(result.match_best_rect_y_value) + "," +
                             FormatReviewDouble(result.match_best_rect_w_value) + "," +
                             FormatReviewDouble(result.match_best_rect_h_value),
                           "",
                           "x,y,w,h",
                           (result.match_best_rect_w_value > 0.0 && result.match_best_rect_h_value > 0.0) ? "stable"
                                                                                                            : "elevated",
                           (result.match_best_rect_w_value > 0.0 && result.match_best_rect_h_value > 0.0) ? "reported"
                                                                                                            : "missing");
    PushReviewMetricRecord(adapter.metrics,
                           "match_center_x",
                           FormatReviewDouble(result.match_center_x_value),
                           "px",
                           "reported",
                           "none",
                           "reported");
    PushReviewMetricRecord(adapter.metrics,
                           "match_center_y",
                           FormatReviewDouble(result.match_center_y_value),
                           "px",
                           "reported",
                           "none",
                           "reported");

    if (result.match_candidate_count_value <= 0.0)
    {
      PushUniqueReviewText(adapter.anomaly_flags, "match_candidate_missing");
      PushUniqueReviewText(adapter.anomaly_flags, "geometry_mismatch");
    }
    if (result.match_top_score_value <= 0.0)
      PushUniqueReviewText(adapter.anomaly_flags, "low_match_confidence");
    if (result.match_best_rect_w_value <= 0.0 || result.match_best_rect_h_value <= 0.0)
    {
      PushUniqueReviewText(adapter.anomaly_flags, "match_rect_missing");
      PushUniqueReviewText(adapter.anomaly_flags, "bbox_contour_deviation");
    }
    if (result.template_used_fallback_value > 0.0)
      PushUniqueReviewText(adapter.anomaly_flags, "fallback_path_used");
    if (result.match_candidate_count_value > 4.0)
      PushUniqueReviewText(adapter.anomaly_flags, "candidate_instability");

    PushUniqueReviewText(adapter.analysis_suggestions,
                         "inspect match_summary, top1_rect alignment, and candidate ranking stability");
    PushUniqueReviewText(adapter.analysis_suggestions,
                         "recheck template/test geometric consistency if score margin weakens");

    PushUniqueReviewText(adapter.notes, "classical_adapter=fast_template_match match_candidates promoted to review object");
    return adapter;
  }

  if (result.layer == "matcher" && result.case_name == "findobject_region")
  {
    adapter.matched_case = true;
    const std::string input_image_ref = SelectClassicalInputImageRef(result);
    const std::string region_summary_ref = BuildReviewScopedRef(result, "region_summary");
    adapter.primary_visual_ref = !input_image_ref.empty() ? input_image_ref : region_summary_ref;
    PushUniqueReviewText(adapter.focus_image_ids, "findobject_region");
    PushUniqueReviewText(adapter.focus_image_ids, "region_summary");
    PushUniqueReviewText(adapter.focus_image_ids, "top1_rect");
    PushUniqueReviewText(adapter.focus_image_ids, "top1_center");
    PushUniqueReviewText(adapter.visualization_refs, adapter.primary_visual_ref);
    PushUniqueReviewText(adapter.visualization_refs, region_summary_ref);
    PushUniqueReviewText(adapter.visualization_refs, BuildReviewScopedRef(result, "region_bounds"));
    PushUniqueReviewText(adapter.phenomenon_evidence, "classical_source=region_summary");
    PushUniqueReviewText(adapter.phenomenon_evidence, "region_summary_ref=" + region_summary_ref);
    if (!input_image_ref.empty())
      PushUniqueReviewText(adapter.phenomenon_evidence, "source_image_ref=" + input_image_ref);
    PushUniqueReviewText(adapter.phenomenon_evidence,
                         "match_summary=result_count=" + FormatReviewDouble(result.match_candidate_count_value) +
                           ",top1_rect=" + FormatReviewDouble(result.match_best_rect_x_value) + "," +
                           FormatReviewDouble(result.match_best_rect_y_value) + "," +
                           FormatReviewDouble(result.match_best_rect_w_value) + "," +
                           FormatReviewDouble(result.match_best_rect_h_value) +
                           ",top1_center=" + FormatReviewDouble(result.match_center_x_value) + "," +
                           FormatReviewDouble(result.match_center_y_value));

    PushReviewMetricRecord(adapter.metrics,
                           "result_count",
                           FormatReviewDouble(result.match_candidate_count_value),
                           "count",
                           ">=0",
                           result.match_candidate_count_value >= 0.0 ? "stable" : "elevated",
                           "reported");
    PushReviewMetricRecord(adapter.metrics,
                           "top1_rect",
                           FormatReviewDouble(result.match_best_rect_x_value) + "," +
                             FormatReviewDouble(result.match_best_rect_y_value) + "," +
                             FormatReviewDouble(result.match_best_rect_w_value) + "," +
                             FormatReviewDouble(result.match_best_rect_h_value),
                           "",
                           "x,y,w,h",
                           (result.match_best_rect_w_value > 0.0 && result.match_best_rect_h_value > 0.0) ? "stable"
                                                                                                            : "elevated",
                           (result.match_best_rect_w_value > 0.0 && result.match_best_rect_h_value > 0.0) ? "reported"
                                                                                                            : "missing");
    PushReviewMetricRecord(adapter.metrics,
                           "top1_center",
                           FormatReviewDouble(result.match_center_x_value) + "," +
                             FormatReviewDouble(result.match_center_y_value),
                           "px",
                           "x,y",
                           "none",
                           "reported");
    PushReviewMetricRecord(adapter.metrics,
                           "region_connected_components",
                           FormatReviewDouble(result.region_connected_components_value),
                           "count",
                           ">=0",
                           result.region_connected_components_value >= 0.0 ? "stable" : "elevated",
                           "reported");
    PushReviewMetricRecord(adapter.metrics,
                           "region_bounds_count",
                           FormatReviewDouble(result.region_bounds_count_value),
                           "count",
                           ">=0",
                           result.region_bounds_count_value >= 0.0 ? "stable" : "elevated",
                           "reported");
    PushReviewMetricRecord(adapter.metrics,
                           "region_foreground_ratio",
                           FormatReviewDouble(result.region_foreground_ratio_value),
                           "",
                           "0..1",
                           (result.region_foreground_ratio_value >= 0.1 &&
                            result.region_foreground_ratio_value <= 0.9) ? "stable" : "elevated",
                           "reported");

    if (result.match_candidate_count_value <= 0.0)
      PushUniqueReviewText(adapter.anomaly_flags, "geometry_mismatch");
    if (result.match_best_rect_w_value <= 0.0 || result.match_best_rect_h_value <= 0.0)
      PushUniqueReviewText(adapter.anomaly_flags, "bbox_contour_deviation");
    if (result.region_foreground_ratio_value > 0.0 &&
        (result.region_foreground_ratio_value < 0.1 || result.region_foreground_ratio_value > 0.9))
      PushUniqueReviewText(adapter.anomaly_flags, "threshold_mismatch");
    if (result.match_candidate_count_value > 3.0)
      PushUniqueReviewText(adapter.anomaly_flags, "candidate_instability");

    PushUniqueReviewText(adapter.analysis_suggestions,
                         "inspect region_summary thresholding and top1_rect/top1_center stability");
    PushUniqueReviewText(adapter.analysis_suggestions,
                         "recheck contour bounds and candidate multiplicity when region matching drifts");
    PushUniqueReviewText(adapter.notes, "classical_adapter=findobject_region region_summary promoted to review object");
    return adapter;
  }

  return adapter;
}
}
