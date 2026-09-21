#ifndef CXPARSER_EXT_PARSER_CXCORE_CLASSICAL_ADAPTER_H
#define CXPARSER_EXT_PARSER_CXCORE_CLASSICAL_ADAPTER_H

#include <string>
#include <vector>

#include "../meta/parser_image_analysis_protocol.h"
#include "../../cxcore/core/CxCoreBoundary.h"

namespace cxparser_ext
{
struct CxScriptExecutionResult;

struct CxcoreLineMeasurementBridgeResult
{
  bool success = false;
  int point_count = 0;
  int chain_length = 0;
  int edgeband_count = 0;
  double fit_error_avg = 0.0;
  double fit_error_max = 0.0;
  double line_angle = 0.0;
  double line_offset = 0.0;
  double subpixel_adjust_avg = 0.0;
  std::string error_message;
};

struct CxcoreCircleMeasurementBridgeResult
{
  bool success = false;
  bool used_fallback = false;
  bool prefilter_used = false;
  bool compact_path_used = false;
  int sample_points = 0;
  double center_x = 0.0;
  double center_y = 0.0;
  double radius = 0.0;
  double avg_distance = 0.0;
  std::string failure_stage;
  std::string error_message;
};

struct CxcoreTemplateFeatureMatchBridgeResult
{
  bool success = false;
  bool used_fallback = false;
  int learn_path_a_count = 0;
  int learn_path_b_count = 0;
  int main_candidate_count = 0;
  int candidate_count = 0;
  int selected_index = -1;
  int best_index = -1;
  double main_top_score = 0.0;
  double top_score = 0.0;
  double max_score = 0.0;
  double center_x = 0.0;
  double center_y = 0.0;
  double best_rect_x = 0.0;
  double best_rect_y = 0.0;
  double best_rect_w = 0.0;
  double best_rect_h = 0.0;
  std::string error_message;
};

struct CxcoreRegionBoundaryBridgeResult
{
  bool success = false;
  int raw_connected_components = 0;
  int connected_components = 0;
  int width = 0;
  int height = 0;
  int bounds_count = 0;
  double foreground_ratio = 0.0;
  std::string error_message;
};

struct CxcoreClassicalReviewMetric
{
  std::string metric_name;
  std::string metric_value;
  std::string metric_unit;
  std::string expected_range;
  std::string baseline_value;
  std::string deviation_level;
  std::string metric_status;
};

struct CxcoreClassicalReviewAdapterResult
{
  bool matched_case = false;
  std::string primary_visual_ref;
  std::vector<std::string> visualization_refs;
  std::vector<CxcoreClassicalReviewMetric> metrics;
  std::vector<std::string> anomaly_flags;
  std::vector<std::string> phenomenon_evidence;
  std::vector<std::string> focus_image_ids;
  std::vector<std::string> analysis_suggestions;
  std::vector<std::string> notes;
};

cxcore::BaselineFeatureSampleV1 ConvertToCxcoreBaselineSample(const ImageAnalysisRequest &request,
                                                              const ImageAnalysisResult &result);

bool RunCxcoreLineMeasurementBalancedBridge(const std::string &case_name,
                                            CxcoreLineMeasurementBridgeResult &result);
bool RunCxcoreCircleMeasurementBalancedBridge(const std::string &case_name,
                                              CxcoreCircleMeasurementBridgeResult &result);
bool RunCxcoreTemplateFeatureMatchBridge(const std::string &case_name,
                                         CxcoreTemplateFeatureMatchBridgeResult &result);
bool RunCxcoreRegionBoundaryBridge(const std::string &case_name,
                                   CxcoreRegionBoundaryBridgeResult &result);
CxcoreClassicalReviewAdapterResult BuildCxcoreClassicalReviewAdapter(const CxScriptExecutionResult &result);
}

#endif
