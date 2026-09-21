#ifndef CXPARSER_EXT_PARSER_CXSCRIPT_TYPES_H
#define CXPARSER_EXT_PARSER_CXSCRIPT_TYPES_H

#include "../meta/parser_multimodal_slice_types.h"

#include <string>
#include <vector>

namespace cxparser_ext
{
struct ParserTestRunResult;

struct CxScriptSourceSpan
{
  int line_begin = 0;
  int column_begin = 0;
  int line_end = 0;
  int column_end = 0;
};

struct CxScriptParseError
{
  std::string message;
  std::string token;
  int line = 0;
  int column = 0;
  int block_depth = 0;
  std::string step_name;
};

struct CxScriptStepTrace
{
  int step_id = 0;
  int frame_id = 0;
  std::string step_name;
  CxScriptSourceSpan span;
  int block_depth = 0;
  std::string status;
};

struct CxScriptSourceMapEntry
{
  int step_id = 0;
  int frame_id = 0;
  std::string step_name;
  std::string statement_kind;
  CxScriptSourceSpan span;
  int block_depth = 0;
};

struct CxScriptCheckpointRecord
{
  int step_id = 0;
  int frame_id = 0;
  std::string name;
  std::string step_name;
  CxScriptSourceSpan span;
  int block_depth = 0;
};

struct CxScriptBreakpointRecord
{
  int step_id = 0;
  int frame_id = 0;
  std::string name;
  std::string step_name;
  CxScriptSourceSpan span;
  int block_depth = 0;
};

enum CxScriptExecutionOpcode
{
  cxseo_unknown,
  cxseo_header_metadata,
  cxseo_step_enter,
  cxseo_block,
  cxseo_type_decl,
  cxseo_type_use,
  cxseo_var_decl,
  cxseo_input,
  cxseo_call,
  cxseo_compile,
  cxseo_action,
  cxseo_expect,
  cxseo_emit,
  cxseo_breakpoint,
  cxseo_checkpoint
};

struct CxScriptExecutionOp
{
  int sequence = 0;
  int step_id = 0;
  int frame_id = 0;
  CxScriptExecutionOpcode opcode = cxseo_unknown;
  std::string step_name;
  std::string payload;
  CxScriptSourceSpan span;
  int block_depth = 0;
};

struct CxScriptReplayFrame
{
  int sequence = 0;
  int previous_sequence = 0;
  int next_sequence = 0;
  int step_id = 0;
  int frame_id = 0;
  std::string step_name;
  std::string action;
  std::string status;
  CxScriptSourceSpan span;
};

enum CxScriptExecutionStepKind
{
  cxsesk_unknown,
  // Metadata header step: represents script header fields in execution/debug/replay.
  // This is not a normal call/action and currently accepts both key=value and name(...).
  // In debug/replay it should be read as "header metadata applied at this source line".
  cxsesk_header_metadata,
  cxsesk_step,
  cxsesk_frame_enter,
  cxsesk_frame_exit,
  cxsesk_type_decl,
  cxsesk_type_use,
  cxsesk_var_decl,
  cxsesk_input,
  cxsesk_call,
  cxsesk_compile,
  cxsesk_action,
  cxsesk_check,
  cxsesk_print,
  cxsesk_breakpoint,
  cxsesk_checkpoint
};

struct CxScriptExecutionStepView
{
  int sequence = 0;
  int step_id = 0;
  int frame_id = 0;
  int previous_sequence = 0;
  int next_sequence = 0;
  CxScriptExecutionStepKind kind = cxsesk_unknown;
  std::string step_name;
  std::string action;
  std::string control_tag;
  std::string payload;
  CxScriptSourceSpan span;
  int block_depth = 0;
};

struct CxScriptExecutionSummary
{
  int entry_step_id = 0;
  int check_step_id = 0;
  int max_step_id = 0;
  int max_frame_id = 0;
  int max_sequence = 0;
  int max_block_depth = 0;
  int step_count = 0;
  int replay_frame_count = 0;
  int source_entry_count = 0;
  int header_step_count = 0;
  int frame_step_count = 0;
  int call_step_count = 0;
  int compile_step_count = 0;
  int check_step_count = 0;
  int print_step_count = 0;
  int breakpoint_step_count = 0;
  int checkpoint_step_count = 0;
};

struct CxScriptNamedResultObject
{
  std::string result_name;
  std::string stage_name;
  std::string object_name;
  std::string status;
  std::string failure_stage;
};

struct CxScriptNamedResultField
{
  std::string result_name;
  std::string stage_name;
  std::string field_name;
  std::string value;
};

struct CxScriptVariableDecl
{
  int step_id = 0;
  int frame_id = 0;
  std::string type_name;
  std::string variable_name;
  std::string step_name;
  CxScriptSourceSpan span;
  int block_depth = 0;
  bool initialized = false;
  bool known_type = false;
};

struct CxScriptTypeSpec
{
  std::string name;
  std::string type_name;
  bool builtin = false;
  bool user_defined = false;
};

struct CxScriptFunctionFragment
{
  std::string function_name;
  std::string category;
  std::string flow_role;
  std::string summary;
  std::vector<std::string> input_names;
  std::string output_name;
  std::vector<std::string> check_points;
  std::string source_hint;
  bool preferred_for_stage_test = false;
};

struct CxScriptFlowSnippet
{
  std::string snippet_id;
  std::string summary;
  std::vector<std::string> function_names;
  std::vector<std::string> input_prepare_functions;
  std::vector<std::string> operator_functions;
  std::vector<std::string> result_check_functions;
  bool reusable_for_cxcore = false;
};

struct CxScriptCStyleSnippet
{
  std::string snippet_id;
  std::string action_family;
  std::string script_text;
  std::vector<std::string> input_objects;
  std::vector<std::string> output_objects;
  std::vector<std::string> check_points;
};

struct CxScriptMainlineReadiness
{
  struct Group
  {
    std::string group_name;
    std::vector<std::string> ready_flow_ids;
    std::vector<std::string> gap_flow_ids;
  };

  std::vector<std::string> ready_flow_ids;
  std::vector<std::string> gap_flow_ids;
  std::vector<std::string> gap_reasons;
  std::vector<std::string> handoff_order;
  std::vector<Group> groups;
};

struct CxScriptDebugView
{
  CxScriptParseError parse_error;
  CxScriptExecutionSummary execution_summary;
  std::vector<CxScriptNamedResultObject> named_results;
  std::vector<CxScriptNamedResultField> result_fields;
  std::vector<CxScriptStepTrace> step_traces;
  std::vector<CxScriptSourceMapEntry> source_map;
  std::vector<CxScriptCheckpointRecord> checkpoints;
  std::vector<CxScriptBreakpointRecord> breakpoints;
  std::vector<CxScriptExecutionOp> execution_ops;
  std::vector<CxScriptReplayFrame> replay_frames;
  std::vector<CxScriptExecutionStepView> execution_steps;
  std::vector<CxScriptVariableDecl> variables;
};

struct CxScriptDebugQueryResult
{
  bool found = false;
  int sequence = 0;
  int step_id = 0;
  int frame_id = 0;
  std::string step_name;
  int line = 0;
  int current_block_depth = 0;
  CxScriptExecutionSummary execution_summary;
  std::vector<CxScriptNamedResultObject> named_results;
  std::vector<CxScriptNamedResultField> result_fields;
  CxScriptStepTrace matched_step_trace;
  CxScriptSourceMapEntry matched_source_entry;
  CxScriptCheckpointRecord nearest_checkpoint;
  CxScriptCheckpointRecord previous_checkpoint;
  CxScriptCheckpointRecord next_checkpoint;
  CxScriptBreakpointRecord nearest_breakpoint;
  CxScriptBreakpointRecord previous_breakpoint;
  CxScriptBreakpointRecord next_breakpoint;
  CxScriptExecutionOp matched_execution_op;
  CxScriptReplayFrame previous_replay_frame;
  CxScriptReplayFrame nearest_replay_frame;
  CxScriptReplayFrame next_replay_frame;
  CxScriptExecutionStepView previous_execution_step;
  CxScriptExecutionStepView matched_execution_step;
  CxScriptExecutionStepView next_execution_step;
  std::vector<CxScriptSourceMapEntry> source_entries;
  std::vector<CxScriptExecutionOp> execution_ops;
  std::vector<CxScriptReplayFrame> replay_frames;
  std::vector<CxScriptExecutionStepView> execution_steps;
  std::vector<CxScriptCheckpointRecord> checkpoints;
  std::vector<CxScriptBreakpointRecord> breakpoints;
  std::vector<CxScriptVariableDecl> variables;
};

enum CxScriptKind
{
  cxsk_unknown,
  cxsk_module,
  cxsk_integration
};

struct CxScriptExecutionContext
{
  std::string script_path;
  std::string script_name;
  std::string kind;
  std::string layer;
  std::string module;
  std::string case_name;
  std::string mode;
  std::string route;
  bool report_on = true;
  std::string trace_id;
};

struct CxScriptExecutionResult
{
  CxScriptExecutionResult() = default;
  CxScriptExecutionResult(const ParserTestRunResult &result);
  bool success = false;
  bool degraded = false;
  bool train_ok = false;
  bool infer_ok = false;
  bool score_ok = false;
  std::string script_path;
  std::string script_name;
  std::string kind;
  std::string layer;
  std::string module;
  std::string case_name;
  std::string route;
  std::string task_id;
  double scalar_result = 0.0;
  double runtime_ms = 0.0;
  double fit_time_ms = 0.0;
  double infer_time_ms = 0.0;
  double feature_dim = 0.0;
  double accuracy = 0.0;
  double macro_f1 = 0.0;
  double prediction_count = 0.0;
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
  double line_measure_bbox_w_value = 0.0;
  double line_angle_value = 0.0;
  double line_offset_value = 0.0;
  double subpixel_adjust_avg_value = 0.0;
  double circle_center_x_value = 0.0;
  double circle_center_y_value = 0.0;
  double circle_radius_value = 0.0;
  double circle_avg_distance_value = 0.0;
  double circle_sample_points_value = 0.0;
  std::string fit_mode;
  double fit_compare_enabled_value = 0.0;
  double fit_legacy_available_value = 0.0;
  double fit_enhanced_available_value = 0.0;
  double circle_legacy_runtime_ms_value = 0.0;
  double circle_enhanced_runtime_ms_value = 0.0;
  double circle_compare_runtime_delta_ms_value = 0.0;
  double circle_legacy_center_x_value = 0.0;
  double circle_legacy_center_y_value = 0.0;
  double circle_legacy_radius_value = 0.0;
  double circle_legacy_avg_distance_value = 0.0;
  double circle_legacy_sample_points_value = 0.0;
  double circle_enhanced_center_x_value = 0.0;
  double circle_enhanced_center_y_value = 0.0;
  double circle_enhanced_radius_value = 0.0;
  double circle_enhanced_avg_distance_value = 0.0;
  double circle_enhanced_sample_points_value = 0.0;
  double circle_compare_radius_delta_value = 0.0;
  double circle_compare_center_delta_value = 0.0;
  double circle_compare_avg_distance_delta_value = 0.0;
  double circle_compare_sample_points_delta_value = 0.0;
  std::string circle_legacy_failure_stage;
  std::string circle_enhanced_failure_stage;
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
  double formfit_legacy_runtime_ms_value = 0.0;
  double formfit_enhanced_runtime_ms_value = 0.0;
  double formfit_compare_runtime_delta_ms_value = 0.0;
  double formfit_legacy_candidate_count_value = 0.0;
  double formfit_enhanced_candidate_count_value = 0.0;
  double formfit_compare_candidate_count_delta_value = 0.0;
  double formfit_legacy_selected_index_value = -1.0;
  double formfit_enhanced_selected_index_value = -1.0;
  double formfit_legacy_best_index_value = -1.0;
  double formfit_enhanced_best_index_value = -1.0;
  double formfit_legacy_best_score_value = 0.0;
  double formfit_enhanced_best_score_value = 0.0;
  double formfit_compare_best_score_delta_value = 0.0;
  double formfit_legacy_rect_x_value = 0.0;
  double formfit_legacy_rect_y_value = 0.0;
  double formfit_legacy_rect_w_value = 0.0;
  double formfit_legacy_rect_h_value = 0.0;
  double formfit_enhanced_rect_x_value = 0.0;
  double formfit_enhanced_rect_y_value = 0.0;
  double formfit_enhanced_rect_w_value = 0.0;
  double formfit_enhanced_rect_h_value = 0.0;
  double formfit_compare_rect_center_delta_value = 0.0;
  std::string formfit_legacy_failure_stage;
  std::string formfit_enhanced_failure_stage;
  double template_used_fallback_value = 0.0;
  double roi_area_value = 0.0;
  double component_count_value = 0.0;
  double image_model_score_value = 0.0;
  double roi_patch_count_value = 0.0;
  double roi_class_label_count_value = 0.0;
  double region_spatial_size_value = 0.0;
  double mask_label_spatial_size_value = 0.0;
  double roi_patch_spatial_size_value = 0.0;
  double baseline_roi_area_value = 0.0;
  double baseline_component_count_value = 0.0;
  double baseline_match_best_score_value = 0.0;
  double baseline_image_model_score_value = 0.0;
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
  std::string model_name;
  std::string feature_set;
  std::string label_column;
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
  std::string selected_method;
  std::string ordered_candidates;
  std::string config_name;
  std::string model_path;
  std::string predictions_csv;
  std::string output_summary_csv;
  std::string input_dataset;
  std::string dataset_profile;
  std::string prepared_root;
  std::string input_task;
  std::string input_profile;
  std::string requested_device;
  std::string consumed_weight_files;
  std::string consumed_weight_paths;
  std::string required_input_contract;
  std::string required_label_contract;
  std::string template_root;
  std::string pairs_ref;
  std::string attach_back_result;
  std::string train_param_summary;
  std::string infer_param_summary;
  std::string attach_back_overlay_status;
  std::string attach_back_top1_class;
  std::string attach_back_confidence;
  std::string optimize_summary_object;
  std::string compare_summary_object;
  std::string replay_result_object;
  std::string rag_writeback_note_object;
  std::string result_object;
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
  std::string failure_mode;
  std::string error_message;
  std::string summary;
  double baseline_objective = 0.0;
  double best_objective = 0.0;
  double objective_delta = 0.0;
  double metric_delta = 0.0;
  double stability_delta = 0.0;
  std::string pass_level;
  std::string replay_log_path;
  std::vector<std::string> compiled_stage_names;
  std::vector<CxScriptNamedResultObject> named_results;
  std::vector<CxScriptNamedResultField> result_fields;
  int last_step_id = 0;
  int last_frame_id = 0;
  int last_sequence = 0;
  int last_source_line = 0;
  int failure_step_id = 0;
  int failure_frame_id = 0;
  int failure_sequence = 0;
  int failure_line = 0;
  std::string failure_phase;
  CxScriptParseError parse_error;
  CxScriptExecutionSummary execution_summary;
  std::vector<CxScriptStepTrace> step_traces;
  std::vector<CxScriptSourceMapEntry> source_map;
  std::vector<CxScriptCheckpointRecord> checkpoints;
  std::vector<CxScriptBreakpointRecord> breakpoints;
  std::vector<CxScriptExecutionOp> execution_ops;
  std::vector<CxScriptReplayFrame> replay_frames;
  std::vector<CxScriptExecutionStepView> execution_steps;
  std::vector<CxScriptVariableDecl> variables;
  std::vector<CxScriptTypeSpec> declared_types;
  std::vector<MultimodalSlice> multimodal_slices;
  std::vector<OperationAtom> operation_atoms;
  std::vector<TaskContext> task_contexts;
  std::vector<GeometrySemanticType> geometry_semantic_types;
  std::vector<GeometryTemplateSpec> geometry_template_specs;
  std::vector<ImageAcquisitionSpec> image_acquisition_specs;
  std::vector<TrainingInput> training_inputs;
  std::vector<RunInput> run_inputs;
  std::vector<ReviewDecision> review_decisions;
  std::vector<FlowbackAction> flowback_actions;
  std::vector<SequenceLinkTrace> sequence_link_traces;
  std::vector<UnifiedImageReviewRecord> unified_image_reviews;
  std::vector<UnifiedTaskReviewBundle> unified_task_reviews;
  std::vector<UnifiedCompareSlice> unified_compare_slices;
  std::vector<UnifiedAnomalyFocusBundle> unified_anomaly_focus_bundles;
  CxScriptDebugView debug_view;
  std::vector<std::string> details;
};
}

#endif

