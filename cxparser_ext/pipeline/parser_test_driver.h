#ifndef CXPARSER_EXT_PARSER_TEST_DRIVER_H
#define CXPARSER_EXT_PARSER_TEST_DRIVER_H

#include <string>
#include <vector>

#include "../meta/parser_binding_spec.h"
#include "../meta/parser_image_analysis_protocol.h"
#include "../meta/parser_pseudocode_types.h"
#include "parser_task_types.h"
#include "parser_unified_entry.h"

namespace cxparser_ext
{
struct CxScriptExecutionResult;

enum ParserTestPlanKind
{
  ptpk_unknown,
  ptpk_execution_target,
  ptpk_image_analysis
};

struct ParserTestRequest
{
  std::string layer;
  std::string module;
  std::string case_name;
  std::string mode;
  std::string input_dataset;
  std::string input_split;
  std::vector<std::string> input_samples;
  std::vector<std::string> input_artifacts;
  std::vector<std::string> input_params;
  bool report_on = true;
  bool debug_on = false;
};

struct ParserTestPlan
{
  ParserTestPlanKind kind = ptpk_unknown;
  bool supported = false;
  bool requires_binding = false;
  std::string reason;
  std::string cxscript_text;
  PseudoClassSpec pseudo_class;
  ExecutionTarget target;
  ImageAnalysisRequest image_request;
};

struct ParserTestRunResult
{
  bool success = false;
  bool degraded = false;
  bool build_planned = false;
  bool run_executed = false;
  std::string status;
  std::string layer;
  std::string module;
  std::string case_name;
  std::string route;
  std::string task_id;
  double scalar_result = 0.0;
  double runtime_ms = 0.0;
  double accuracy = 0.0;
  double macro_f1 = 0.0;
  bool bridge_enabled = false;
  double line_horizontal_samples_contract_value = 0.0;
  double line_vertical_samples_contract_value = 0.0;
  double line_measure_bounds_contract_value = 0.0;
  double circle_center_contract_value = 0.0;
  double circle_radius_contract_value = 0.0;
  double circle_avg_distance_contract_value = 0.0;
  double template_candidate_count_contract_value = 0.0;
  double template_top_score_contract_value = 0.0;
  double template_match_center_contract_value = 0.0;
  double template_min_candidate_count_contract_value = 0.0;
  double template_min_top_score_contract_value = 0.0;
  double region_connected_components_contract_value = 0.0;
  double region_size_contract_value = 0.0;
  double region_bounds_contract_value = 0.0;
  double region_min_connected_components_contract_value = 0.0;
  double region_min_bounds_count_contract_value = 0.0;
  double point_count_value = 0.0;
  double line_chain_length_value = 0.0;
  double line_edgeband_count_value = 0.0;
  double fit_error_avg_value = 0.0;
  double fit_error_max_value = 0.0;
  double line_angle_value = 0.0;
  double line_offset_value = 0.0;
  double subpixel_adjust_avg_value = 0.0;
  double circle_center_x_value = 0.0;
  double circle_center_y_value = 0.0;
  double circle_radius_value = 0.0;
  double circle_avg_distance_value = 0.0;
  double circle_sample_points_value = 0.0;
  double circle_used_fallback_value = 0.0;
  double circle_prefilter_used_value = 0.0;
  double circle_compact_path_value = 0.0;
  std::string circle_failure_stage;
  double match_candidate_count_value = 0.0;
  double match_selected_index_value = -1.0;
  double match_best_index_value = -1.0;
  double candidate_count_value = 0.0;
  double selected_candidate_index_value = -1.0;
  double selected_candidate_score_value = 0.0;
  double score_total_value = 0.0;
  double template_learn_path_a_count_value = 0.0;
  double template_learn_path_b_count_value = 0.0;
  double template_main_candidate_count_value = 0.0;
  double template_main_top_score_value = 0.0;
  double match_top_score_value = 0.0;
  double match_max_score_value = 0.0;
  double match_center_x_value = 0.0;
  double match_center_y_value = 0.0;
  double match_best_rect_x_value = 0.0;
  double match_best_rect_y_value = 0.0;
  double match_best_rect_w_value = 0.0;
  double match_best_rect_h_value = 0.0;
  double template_used_fallback_value = 0.0;
  double roi_area_value = 0.0;
  double component_count_value = 0.0;
  double image_model_score_value = 0.0;
  double baseline_roi_area_value = 0.0;
  double baseline_component_count_value = 0.0;
  double baseline_match_best_score_value = 0.0;
  double baseline_image_model_score_value = 0.0;
  double roi_patch_count_value = 0.0;
  double roi_patch_spatial_size_value = 0.0;
  double roi_class_label_count_value = 0.0;
  double region_spatial_size_value = 0.0;
  double mask_label_spatial_size_value = 0.0;
  double baseline_roi_patch_count_value = 0.0;
  double baseline_roi_alignment_status_value = 0.0;
  double baseline_mask_alignment_status_value = 0.0;
  double baseline_export_contract_value = 0.0;
  double region_pattern_foreground_ratio_value = 0.0;
  double region_pattern_descriptor_dim_value = 0.0;
  double region_pattern_descriptor_mean_value = 0.0;
  double region_pattern_descriptor_std_value = 0.0;
  double region_connected_components_value = 0.0;
  double region_raw_connected_components_value = 0.0;
  double region_width_value = 0.0;
  double region_height_value = 0.0;
  double region_bounds_count_value = 0.0;
  double region_foreground_ratio_value = 0.0;
  std::string published_handoff_type;
  std::string published_primary_ref;
  std::string published_route_hint;
  std::string published_route_state;
  std::string published_source_hash;
  std::string published_result_ref;
  std::string published_evidence_ref;
  std::string published_bbox_candidate_list_ref;
  std::string published_template_alignment_ref;
  std::string published_template_test_alignment_status;
  std::string published_roi_diff_candidate_ref;
  std::string published_roi_diff_candidate_count;
  std::string published_prior_roi_region_ref;
  std::string published_roi_crop_packet_ref;
  std::string published_roi_crop_count;
  std::string published_roi_crop_spatial_size;
  std::string published_roi_crop_policy_ref;
  std::string internal_test_interface_name;
  std::string internal_test_interface_purpose;
  std::string execution_stage_0;
  std::string execution_stage_1;
  std::string execution_stage_2;
  std::string execution_stage_3;
  std::string metrics;
  std::string roi_patch_tensor_value;
  std::string roi_class_label_value;
  std::string region_tensor_value;
  std::string region_channel_layout_value;
  std::string mask_or_region_label_value;
  std::string roi_alignment_status_value;
  std::string mask_alignment_status_value;
  std::string fractal_partition_value;
  std::string distance_field_value;
  std::string skeleton_mask_value;
  std::string centerline_paths_value;
  std::string topology_repair_paths_value;
  std::string result_object;
  std::string input_dataset;
  std::string input_sample;
  std::string input_split;
  std::string input_artifacts;
  std::string input_params;
  std::string dataset_ref;
  std::string sample_bundle_ref;
  std::string objective_ref;
  std::string optimization_result_ref;
  std::string best_params_ref;
  std::string objective_delta_ref;
  std::string summary_ref;
  std::string compare_ref;
  std::string replay_ref;
  std::string cluster_ref;
  std::string distance_ref;
  std::string anomaly_ref;
  std::string baseline_class_ref;
  std::string baseline_feature_ref;
  std::string attach_back_ref;
  std::string attach_back_overlay_status;
  std::string attach_back_top1_class;
  std::string attach_back_confidence;
  std::string bbox_candidate_list_ref;
  std::string roi_crop_packet_ref;
  std::string template_alignment_ref;
  std::string candidate_overlay_ref;
  std::string template_rect_overlay_ref;
  std::string test_rect_overlay_ref;
  std::string template_test_alignment_status;
  std::string roi_diff_candidate_ref;
  std::string roi_diff_candidate_count;
  std::string circle_overlay_ref;
  std::string circle_edge_overlay_ref;
  std::string formfit_candidate_overlay_ref;
  std::string formfit_selection_overlay_ref;
  std::string region_pattern_overlay_ref;
  std::string region_pattern_descriptor_ref;
  std::string fractal_partition_overlay_ref;
  std::string distance_field_overlay_ref;
  std::string skeleton_overlay_ref;
  std::string centerline_overlay_ref;
  std::string topology_repair_overlay_ref;
  std::string tolerance;
  std::string selected_method;
  std::string config_name;
  std::string failure_mode;
  std::string error_message;
  std::string summary;
  double baseline_objective = 0.0;
  double best_objective = 0.0;
  double objective_delta = 0.0;
  double metric_delta = 0.0;
  double stability_delta = 0.0;
  std::string pass_level;
  std::vector<std::string> details;

  operator CxScriptExecutionResult() const;
};

class ParserTestDriver
{
public:
  bool Execute(const ParserTestRequest &request, ParserTestRunResult &result);

private:
  bool BuildBindingSpecIfNeeded(const ParserTestPlan &plan, ParserBindingSpec &spec);
  bool SubmitPlan(ParserUnifiedEntry &entry, const ParserTestPlan &plan, ParserTestRunResult &result);
  bool FinalizeResult(ParserUnifiedEntry &entry, const ParserTestPlan &plan, ParserTestRunResult &result);
};
}

#endif
