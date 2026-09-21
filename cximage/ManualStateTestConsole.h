#ifndef CXIMAGE_MANUAL_STATE_TEST_CONSOLE_H
#define CXIMAGE_MANUAL_STATE_TEST_CONSOLE_H

#include "CxEvidenceSelfTestRuntime.h"
#include "CxParamRegressionRuntime.h"
#include "CxScriptCatalogRuntime.h"
#include "CxScriptHeadlessRuntime.h"
#include "ParserDebugBridge.h"
#include "metrology_analytics/CxSurfaceBasicStats.h"
#include "metrology_analytics/tests/ManualConsoleAnalyticsSmoke.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <imgui.h>
#include <iomanip>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
struct ScriptLineView {
  int line_no = 0;
  std::string statement;
  std::string module;
  std::string object_type;
  std::string object;
  std::string method;
  std::string params;
  std::string return_variable;
  std::string status = "PENDING";
  std::string reason = "not executed";
  std::string timestamp;
};

struct ScriptVariableView {
  std::string type;
  std::string name;
  std::string value;
  int declared_line = 0;
  std::string status = "observed_source";
  std::string image_path;
  bool image_initialized = false;
};

struct ScriptObjectView {
  std::string module;
  std::string type;
  std::string name;
  std::string status;
  std::string runtime_state;
  int runtime_source_line = 0;
  int declared_line = 0;
};

struct RuntimeObjectView {
  std::string name;
  std::string type;
  int declared_line = 0;

  bool exists_in_parser = false;

  std::string last_runtime_status = "PENDING";
  std::string runtime_state = "PENDING";
  std::string last_method;
  int last_update_line = 0;

  std::string display_summary;

  bool visualizable = false;
  std::string visual_source = "stale_runtime";
  bool stale = true;

  // FindEllipse ROI / measure status.  Current FindEllipse runtime exposes
  // ROI and measurement points; fitted ellipse result is a later binding.
  bool has_ellipse_roi = false;
  float ellipse_cx = 0.0f;
  float ellipse_cy = 0.0f;
  float ellipse_rx = 0.0f;
  float ellipse_ry = 0.0f;
  int ellipse_inner_scale_percent = 0;
  float ellipse_inner_rx = 0.0f;
  float ellipse_inner_ry = 0.0f;
  bool has_fit_ellipse = false;
  float fit_ellipse_cx = 0.0f;
  float fit_ellipse_cy = 0.0f;
  float fit_ellipse_rx = 0.0f;
  float fit_ellipse_ry = 0.0f;
  float fit_ellipse_angle_deg = 0.0f;
  float fit_ellipse_avgdist = 0.0f;
  std::string ellipse_result_status;
  std::string ellipse_result_reason;

  int ellipse_scan_line_count = 0;
  int ellipse_scan_line_length = 0;
  int ellipse_selected_edge_index = 0;
  int ellipse_scan_lines_cross_outside_ellipse_count = 0;
  int ellipse_accepted_points_outside_ellipse_count = 0;
  double ellipse_accepted_point_norm_min = 0.0;
  double ellipse_accepted_point_norm_avg = 0.0;
  double ellipse_accepted_point_norm_max = 0.0;
  int ellipse_rejected_boundary_band_candidate_count = 0;
  double ellipse_rejected_boundary_band_norm_min = 0.0;
  double ellipse_rejected_boundary_band_norm_avg = 0.0;
  double ellipse_rejected_boundary_band_norm_max = 0.0;
  int ellipse_point_consistency_enabled = 0;
  double ellipse_point_consistency_range = 0.0;
  int ellipse_point_consistency_input_points = 0;
  int ellipse_point_consistency_output_points = 0;
  int ellipse_point_consistency_removed_points = 0;
  std::string ellipse_scan_geometry_policy;
  std::string ellipse_candidate_policy;

  // setcircle(...) 参数圆
  bool has_circle = false;
  float circle_cx = 0.0f;
  float circle_cy = 0.0f;
  // The third and fourth setcircle() arguments are a boundary point,
  // not an inner radius / radius pair.  Keep explicit coordinates for
  // ManualGaugeState synchronization; the legacy fields below remain for
  // existing diagnostic/report code.
  float circle_px = 0.0f;
  float circle_py = 0.0f;
  float circle_inner = 0.0f;
  float circle_radius = 0.0f;

  // measure / FitResultMeasure 后的测量点
  bool has_measure_points = false;
  int measure_points_count = 0;
  int valid_points_count = 0;
  std::vector<float> measure_points_xy;

  // fitcircle 后的拟合结果圆
  bool has_fit_result = false;
  float fit_cx = 0.0f;
  float fit_cy = 0.0f;
  float fit_radius = 0.0f;
  float fit_avgdist = 0.0f;

  bool has_result_measure = false;

  // 可选：用于 summary，不强依赖算法内部接口。
  int scan_path = 0;
  int image_width = 0;
  int image_height = 0;
  int back_image_width = 0;
  int back_image_height = 0;

  // Findcircle measure budget stats
  int circle_scan_lines_processed = 0;
  int circle_total_samples = 0;
  int circle_elapsed_ms = 0;
  int circle_budget_max_scan_lines = 2048;
  int circle_budget_max_samples = 2000000;
  int circle_budget_max_elapsed_ms = 3000;

  // Findline ROI center line.
  bool has_line_roi = false;
  float line_x0 = 0.0f;
  float line_y0 = 0.0f;
  float line_x1 = 0.0f;
  float line_y1 = 0.0f;
  float line_scale = 1.0f;
  std::string line_orientation;
  double line_dx = 0.0;
  double line_dy = 0.0;
  double line_length = 0.0;
  double requested_tool_half_width = 0.0;
  double effective_tool_half_width = 0.0;

  // Findline scan box / scan band.
  bool has_line_scan_box = false;
  float line_scan_half_width = 3.0f;
  int linegap = 3;
  int line_tool_wgap = 0;
  int line_tool_hgap = 0;
  std::string line_display_source;
  std::array<float, 8> line_scan_box_xy = {0.0f, 0.0f, 0.0f, 0.0f,
                                           0.0f, 0.0f, 0.0f, 0.0f};

  // Findline measure points.
  bool has_line_measure_points = false;
  std::vector<float> line_measure_points_xy;
  int line_measure_points_count = 0;
  int valid_line_points_count = 0;

  // Raw split counts, used to determine whether tool produced w/h points.
  int line_pointsw_count = 0;
  int line_pointsh_count = 0;

  // CircleRingGauge fields
  bool has_ring_gauge = false;
  double ring_outer_radius = 0.0;
  double ring_inner_radius = 0.0;
  double ring_thickness = 0.0;
  double ring_center_distance = 0.0;
  bool ring_concentric_ok = false;
  bool ring_inside_ok = false;
  bool ring_thickness_ok = false;
  double ring_score = 0.0;
  std::string ring_status;
  std::string ring_reason;
  std::string ring_result_ref;

  // FindSegmentation runtime / torch bridge visibility.
  std::string segmentation_backend;
  std::string segmentation_backend_status;
  std::string segmentation_device;
  std::string segmentation_model_path;
  std::string segmentation_result_ref;
  std::string segmentation_mask_ref;
  std::string segmentation_contour_ref;
  std::string segmentation_overlay_ref;
  std::string segmentation_reason;
  int segmentation_status_code = 0;
  int segmentation_contour_count = 0;
  double segmentation_primary_area = 0.0;
  bool segmentation_has_prompt_rect = false;
  bool segmentation_has_positive_point = false;
  bool segmentation_has_negative_point = false;
  int segmentation_positive_x = 0;
  int segmentation_positive_y = 0;
  int segmentation_negative_x = 0;
  int segmentation_negative_y = 0;
  bool segmentation_has_boundary = false;
  bool segmentation_has_libtorch_contract = false;
  bool segmentation_real_mask_attach_ready = false;

  // TorchTask runtime / artifact evidence visibility.
  bool is_torch_task = false;
  int torch_ok = 0;
  int torch_error_code = 0;
  int torch_result_count = 0;
  int torch_mask_available = 0;
  double torch_infer_ms = 0.0;
  double torch_train_ms = 0.0;
  double torch_total_ms = 0.0;
  std::string torch_status;
  std::string torch_reason;
  std::string torch_failure_stage;
  std::string torch_actual_device;
  std::string torch_result_ref;
  std::string torch_evidence_ref;
  std::string torch_primary_visual_ref;
  std::string torch_mask_ref;
  std::string torch_overlay_ref;
  std::string torch_trainer_lifecycle_summary;
  std::string torch_unified_mainline_summary;

  // Findline fit result.
  bool has_fit_line = false;
  float fit_line_x0 = 0.0f;
  float fit_line_y0 = 0.0f;
  float fit_line_x1 = 0.0f;
  float fit_line_y1 = 0.0f;
  float line_avgdist = 0.0f;

  std::string line_fit_status;
  std::string line_fit_mode;
  std::string line_measure_status;
  std::string line_measure_hint;
  std::string line_measure_failure_hint;
  bool line_filter_min_exceeds_component_p90 = false;
  std::string line_result_status;
  std::string line_result_reason;

  bool has_line_seek_points = false;
  std::vector<float> line_seek_points_xy;
  int line_seek_points_count = 0;

  int line_profile_point_count = 0;
  int line_edgeband_count = 0;
  int line_chain_length = 0;
  std::string line_measure_failure_stage;

  bool line_measure_image_ready = false;
  int line_measure_image_width = 0;
  int line_measure_image_height = 0;
  int line_measure_image_channels = 0;
  int line_measure_image_type = 0;

  bool line_measure_roi_intersects_image = false;
  bool line_measure_roi_fully_inside_image = false;

  int line_measure_method = 0;
  int line_measure_threshold = 0;
  int line_measure_linegap = 0;
  int line_measure_wgap = 0;
  int line_measure_hgap = 0;

  int line_measure_profile_count = 0;
  int line_measure_sampled_pixel_count = 0;

  double line_measure_gray_min = 0.0;
  double line_measure_gray_max = 0.0;
  double line_measure_gray_mean = 0.0;
  double line_measure_max_gradient = 0.0;

  std::string line_measure_image_source;
  std::string line_measure_input_failure_stage;
  std::string line_measure_input_detail;

  bool line_measure_fallback_allowed = false;
  bool line_measure_fallback_used = false;

  std::string line_measure_source;
  std::string line_measure_original_failure_stage;
  std::string line_measure_original_detail;

  int line_measure_original_point_count = 0;
  int line_measure_original_edgeband_count = 0;
  int line_measure_original_chain_length = 0;

  bool line_measure_geometry_request_valid = false;
  bool line_measure_geometry_dirty = false;
  bool line_measure_geometry_ready = false;

  std::uint64_t line_measure_geometry_version = 0;
  std::uint64_t line_measure_geometry_built_version = 0;

  double line_measure_geometry_half_width = 0.0;

  int line_original_scan_w_count = 0;
  int line_original_scan_h_count = 0;
  int line_original_scan_w_length = 0;
  int line_original_scan_h_length = 0;
  int line_original_process_width = 0;

  int line_scan_rows_examined = 0;
  int line_scan_rows_with_foreground = 0;
  int line_scan_runs_total = 0;
  int line_scan_runs_within_length_limit = 0;
  int line_scan_runs_over_length_limit = 0;
  int line_scan_runs_rejected_by_selection = 0;
  int line_scan_runs_rejected_near_endpoint = 0;
  int line_scan_points_emitted = 0;
  int line_point_consistency_enabled = 0;
  double line_point_consistency_range = 0.0;
  int line_point_consistency_input_points = 0;
  int line_point_consistency_output_points = 0;
  int line_point_consistency_removed_points = 0;
  int line_selected_edge_index = 0;
  int line_evaluated_edge_count = 0;
  int line_best_edge_index = 0;
  double line_best_edge_score = 0.0;
  std::vector<CxFindLineEdgeEvaluationSnapshot> line_edge_evaluations;

  bool line_measure_backimage_ready = false;
  bool line_measure_findobject_ready = false;

  int line_measure_objfilterset = 0;
  int line_measure_filter_borw = 0;
  int line_measure_filter_min = 0;
  int line_measure_filter_max = 0;

  int line_measure_filter_profile = 0;
  bool line_measure_filter_explicit = false;

  int line_measure_effective_filter_borw = 0;
  int line_measure_effective_filter_min = 0;
  int line_measure_effective_filter_max = 0;

  bool line_measure_findobject_called = false;
  bool line_measure_findobject_skipped = false;

  int line_measure_binary_foreground_pixels = 0;
  int line_measure_binary_roi_width = 0;
  int line_measure_binary_roi_height = 0;

  std::string line_measure_result_empty_reason;

  int line_findobject_component_total = 0;
  int line_findobject_component_accepted = 0;
  int line_findobject_component_rejected_by_min = 0;
  int line_findobject_component_rejected_by_max = 0;
  int line_findobject_component_rejected_by_borw = 0;

  int line_findobject_area_min_observed = 0;
  int line_findobject_area_max_observed = 0;
  double line_findobject_area_mean_observed = 0.0;
  int line_findobject_area_min = 0;
  int line_findobject_area_max = 0;
  double line_findobject_area_median = 0.0;
  double line_findobject_area_p90 = 0.0;

  std::string line_measure_cc_selected_foreground;

  int line_measure_cc_white_total = 0;
  int line_measure_cc_white_accepted = 0;
  int line_measure_cc_white_rejected_min = 0;
  double line_measure_cc_white_area_median = 0.0;
  double line_measure_cc_white_area_p90 = 0.0;

  int line_measure_cc_black_total = 0;
  int line_measure_cc_black_accepted = 0;
  int line_measure_cc_black_rejected_min = 0;
  double line_measure_cc_black_area_median = 0.0;
  double line_measure_cc_black_area_p90 = 0.0;

  int line_measure_cc_selected_total = 0;
  int line_measure_cc_selected_accepted = 0;
  int line_measure_cc_selected_rejected_min = 0;
  double line_measure_cc_selected_area_median = 0.0;
  double line_measure_cc_selected_area_p90 = 0.0;

  // Findcircle display snapshot.
  bool has_circle_roi_outer_polyline = false;
  std::vector<float> circle_roi_outer_xy;
  bool has_circle_roi_inner_polyline = false;
  std::vector<float> circle_roi_inner_xy;
  std::uint32_t circle_roi_segment_count = 0;

  bool has_fit_circle_polyline = false;
  std::vector<float> fit_circle_xy;
  std::uint32_t fit_circle_segment_count = 0;

  std::uint64_t display_version = 0;

  bool circle_measure_geometry_request_valid = false;
  bool circle_measure_geometry_dirty = false;
  bool circle_measure_geometry_ready = false;

  std::uint64_t circle_measure_geometry_version = 0;
  std::uint64_t circle_measure_geometry_built_version = 0;

  int circle_scan_line_count = 0;
  int circle_scan_line_length = 0;
  int circle_process_width = 0;
  int circle_selected_edge_index = 0;
  int circle_candidate_runs_total = 0;
  int circle_candidate_runs_max_per_line = 0;
  int circle_selected_edge_hits = 0;
  int circle_selected_edge_misses = 0;
  int circle_scan_boundary_clipped_lines = 0;
  int circle_scan_boundary_extended_samples = 0;
  int circle_candidate_boundary_reject_count = 0;
  double circle_selected_edge_radius_avg = 0.0;
  double circle_selected_edge_radius_min = 0.0;
  double circle_selected_edge_radius_max = 0.0;
  int circle_point_consistency_enabled = 0;
  double circle_point_consistency_range = 0.0;
  int circle_point_consistency_input_points = 0;
  int circle_point_consistency_output_points = 0;
  int circle_point_consistency_removed_points = 0;
  std::string circle_boundary_status;
  std::string circle_boundary_reliability_level;
  int circle_boundary_expected_scan_count = 0;
  int circle_boundary_accepted_point_count = 0;
  int circle_boundary_refined_point_count = 0;
  double circle_boundary_coverage_ratio = 0.0;
  double circle_boundary_subpixel_offset_mean = 0.0;
  double circle_boundary_subpixel_offset_stddev = 0.0;
  double circle_boundary_localization_sigma_mean_px = 0.0;
  double circle_boundary_residual_rmse_px = 0.0;
  double circle_boundary_residual_p95_px = 0.0;
  double circle_boundary_residual_max_px = 0.0;
  double circle_boundary_outlier_ratio = 0.0;
  double circle_boundary_reliability_score = 0.0;

  bool circle_measure_image_ready = false;
  int circle_measure_image_width = 0;
  int circle_measure_image_height = 0;
  int circle_measure_image_channels = 0;

  bool circle_measure_backimage_ready = false;
  bool circle_measure_findobject_ready = false;

  int circle_object_prefilter_requested = 0;
  int circle_object_prefilter_applied = 0;
  int circle_object_prefilter_restored = 0;
  int circle_object_prefilter_runs_before = 0;
  int circle_object_prefilter_runs_after = 0;
  int circle_object_prefilter_effective_min = 0;

  std::string circle_measure_source;
  std::string circle_measure_failure_stage;
  std::string circle_measure_detail;

  bool has_fastmatch_diagnostic = false;
  bool fastmatch_allowed = false;

  std::string fastmatch_status;
  std::string fastmatch_reason;
  std::string fastmatch_result_ref;
  std::string fastmatch_policy;
  std::string fastmatch_source_tool;
  std::string fastmatch_profile;
  std::string fastmatch_level;
  int fastmatch_model_point_count = 0;
  int fastmatch_learn_a_count = 0;
  int fastmatch_learn_b_count = 0;
  int fastmatch_learn_a2_count = 0;
  int fastmatch_learn_b2_count = 0;
  int fastmatch_learn_status_code = 0;
  std::array<int, 4> fastmatch_directional_probe_scans{};
  std::array<int, 4> fastmatch_directional_probe_raw{};
  std::array<int, 4> fastmatch_directional_probe_accepted{};
  std::array<int, 4> fastmatch_directional_probe_diagnostics{};
  std::array<int, 4> fastmatch_directional_probe_edge_count{};
  std::array<int, 4> fastmatch_directional_probe_selected_edge{};
  std::array<int, 4> fastmatch_directional_probe_runtime_selected_edge{};
  std::array<int, 4> fastmatch_directional_probe_compare_gap{};
  std::array<std::string, 4> fastmatch_directional_probe_status{};
  int fastmatch_pattern_a_count = 0;
  int fastmatch_pattern_b_count = 0;
  int fastmatch_normal_trace_domain_count = 0;
  int fastmatch_normal_trace_deduplicated_count = 0;
  int fastmatch_normal_trace_path_count = 0;
  int fastmatch_normal_trace_pair_count = 0;
  int fastmatch_normal_trace_side_count = 0;
  int fastmatch_normal_trace_segment_count = 0;
  double fastmatch_normal_trace_closure_error_px = -1.0;
  double fastmatch_normal_trace_max_consecutive_gap_px = -1.0;
  double fastmatch_normal_trace_gradient_coverage = 0.0;
  double fastmatch_normal_trace_selected_anchor_coverage = 0.0;
  int fastmatch_normal_trace_dijkstra_source_count = 0;
  int fastmatch_normal_trace_derived_count = 0;
  int fastmatch_normal_trace_junction_intersection_count = 0;
  int fastmatch_normal_trace_junction_blend_count = 0;
  std::array<int, 4> fastmatch_normal_trace_endpoint_spike_pruned_counts{};
  int fastmatch_normal_trace_generated_conclusion_count = 0;
  int fastmatch_normal_trace_findline_bound_pairs = 0;
  int fastmatch_normal_trace_binding_misses = 0;
  int fastmatch_normal_trace_corner_rejected = 0;
  int fastmatch_normal_trace_loop_erased_points = 0;
  std::array<int, 4> fastmatch_normal_trace_pair_direction_counts{};
  double fastmatch_normal_trace_anchor_normal_band_px = 0.0;
  std::array<int, 4> fastmatch_normal_trace_anchor_near_counts{};
  std::array<int, 4> fastmatch_normal_trace_anchor_band_rejected_counts{};
  std::array<int, 4> fastmatch_normal_trace_anchor_slope_rejected_counts{};
  std::array<int, 4> fastmatch_normal_trace_anchor_normal_rejected_counts{};
  std::array<int, 4> fastmatch_normal_trace_anchor_accepted_counts{};
  std::array<int, 4> fastmatch_normal_trace_domain_counts{};
  std::array<int, 4> fastmatch_normal_trace_keypoint_counts{};
  std::array<int, 4> fastmatch_normal_trace_ann_edge_counts{};
  std::array<int, 4> fastmatch_normal_trace_ann_component_counts{};
  std::array<int, 4> fastmatch_normal_trace_ann_selected_counts{};
  std::array<double, 4> fastmatch_normal_trace_ann_coverage{};
  std::string fastmatch_normal_trace_reason;
  int fastmatch_candidate_count = 0;
  double fastmatch_best_score = 0.0;
  int fastmatch_learn_rect_x0 = 0;
  int fastmatch_learn_rect_y0 = 0;
  int fastmatch_learn_rect_x1 = 0;
  int fastmatch_learn_rect_y1 = 0;
  int fastmatch_match_rect_x0 = 0;
  int fastmatch_match_rect_y0 = 0;
  int fastmatch_match_rect_x1 = 0;
  int fastmatch_match_rect_y1 = 0;

  bool has_grid_pattern = false;
  int grid_pattern_status_code = 0;
  int grid_pattern_active_cell_count = 0;
  int grid_pattern_descriptor_dim = 0;
  int grid_pattern_level_count = 0;
  int grid_pattern_overlay_count = 0;
  bool grid_pattern_overlay_truncated = false;
  double grid_pattern_elapsed_ms = 0.0;
  std::string grid_pattern_summary;

  bool has_region_pattern = false;
  int region_pattern_status_code = 0;
  int region_pattern_descriptor_dim = 0;
  int region_pattern_foreground_permille = 0;
  int region_pattern_mean_permille = 0;
  int region_pattern_std_permille = 0;
  int region_pattern_pooling_rows = 0;
  int region_pattern_pooling_cols = 0;
  int region_pattern_overlay_count = 0;
  bool region_pattern_overlay_truncated = false;
  double region_pattern_elapsed_ms = 0.0;
  std::string region_pattern_summary;
};

struct DebugStepSnapshot {
  std::string script_path;
  std::string flow_block_id;
  int current_line = 0;
  std::string statement;
  std::string object;
  std::string method;
  std::string params;
  std::string runtime_state;
  std::string object_summary;
  std::string geometry_summary;
  std::string image_overlay_summary;
  std::string current_result_ref;
  std::string last_debug_result;
  std::string reason;
};

struct ResultRefView {
  std::string name;          // global_circle_ref
  std::string value;         // runtime_object:afindcircle0
  std::string source_object; // afindcircle0
  std::string result_type;   // FindcircleResult
  std::string status = "uninitialized";
  std::string reason;

  float fit_cx = 0.0f;
  float fit_cy = 0.0f;
  float fit_radius = 0.0f;
  float avgdist = 0.0f;

  int points_count = 0;
  int valid_points_count = 0;

  int line_no = 0;

  // Line result fields.
  float line_x0 = 0.0f;
  float line_y0 = 0.0f;
  float line_x1 = 0.0f;
  float line_y1 = 0.0f;
  float line_avgdist = 0.0f;
  int line_points_count = 0;
  int valid_line_points_count = 0;

  std::string line_result_status;
  std::string line_result_reason;
  std::string line_measure_status;
  std::string line_measure_hint;
  std::string line_measure_failure_hint;
  bool line_filter_min_exceeds_component_p90 = false;

  std::string line_measure_source;
  bool line_measure_fallback_used = false;
};

enum class GaugeHandleType {
  None,
  LineP0,
  LineP1,
  LineCenter,
  LineWidthPlus,
  LineWidthMinus,
  CircleCenter,
  CircleRadius,
  CircleInner,
  CircleOuter
};

struct GaugeHandle {
  GaugeHandleType type = GaugeHandleType::None;
  float screen_x = 0.0f;
  float screen_y = 0.0f;
  float radius = 6.0f;
  bool hovered = false;
  bool active = false;
};

struct ManualSegmentationPromptPoint {
  std::string ref;
  int x = 0;
  int y = 0;
};

struct ManualGaugeState {
  std::string case_id;
  std::string image_id;
  std::string target_id;
  std::string tool = "FindLine"; // Findline / Findcircle
  std::string primary_object_type;
  std::string primary_object_name;
  std::string primary_object_status = "unresolved";

  std::string source = "manual"; // manifest / replay / ai_suggested / manual
  std::string review_status =
      "editing"; // editing / accepted / rejected / promoted

  bool has_line_gauge = false;
  int line_x0 = 0;
  int line_y0 = 0;
  int line_x1 = 0;
  int line_y1 = 0;
  int tool_half_width = 20;
  int wgap = 32;
  int hgap = 8;
  int scan_direction = 2; // 1=W-only, 2=H-only; Manual UI is exclusive.
  int linegap = 6;
  int min_edge_run_width_px = 3;
  int threshold = 20;
  int filterprofile = 1;
  int method = 2;
  // Unified UI setting for the object prefilter switch.
  // FindLine maps this to setobjfilter(); FindCircle/FindEllipse/FindRect
  // map it to setfindsetting().  -1 means "use the tool's native default".
  int findsetting = -1;

  bool has_circle_gauge = false;
  int circle_cx = 0;
  int circle_cy = 0;
  int circle_px = 0;
  int circle_py = 0;
  int gap = 5;

  bool has_ellipse_gauge = false;
  int ellipse_x0 = 0;
  int ellipse_y0 = 0;
  int ellipse_x1 = 0;
  int ellipse_y1 = 0;
  int ellipse_inner_scale_percent = 0;

  bool has_segmentation_prompt_rect = false;
  int segmentation_prompt_x0 = 120;
  int segmentation_prompt_y0 = 120;
  int segmentation_prompt_x1 = 980;
  int segmentation_prompt_y1 = 820;
  int segmentation_mode = 2;
  bool has_segmentation_positive_point = false;
  int segmentation_positive_x = 0;
  int segmentation_positive_y = 0;
  bool has_segmentation_negative_point = false;
  int segmentation_negative_x = 0;
  int segmentation_negative_y = 0;
  std::vector<ManualSegmentationPromptPoint> segmentation_positive_points;
  std::vector<ManualSegmentationPromptPoint> segmentation_negative_points;
  int segmentation_threshold_percent = 50;
  // 0 = none, 1 = next Image View point is positive prompt,
  // 2 = next Image View point is negative prompt.  This is UI selection
  // state only; global_* enabled flags are set only after an actual point
  // shape is created or edited.
  int segmentation_prompt_pick_mode = 0;

  // FindObject is a thresholded connected-components tool.  Keep its ROI and
  // filtering values separate from line/circle gauge fields so the parameter
  // panel, Shape repository and next CxScript invocation share one snapshot.
  bool has_findobject_roi = false;
  int findobject_x0 = 80;
  int findobject_y0 = 80;
  int findobject_x1 = 1180;
  int findobject_y1 = 880;
  int findobject_foreground_mode = 1;
  int findobject_threshold = 20;
  int findobject_min_area = 10;

  double findobject_pixel_size_x = 1.0;
  double findobject_pixel_size_y = 1.0;
  int findobject_geometry_connectivity = 8;
  int findobject_selected_measurement = 0;
  bool findobject_show_all_conclusions = false;
  bool findobject_show_boundary = true;
  bool findobject_show_moment_ellipse = true;
  bool findobject_show_feret = true;
  bool findobject_show_circles = true;
  int findobject_conclusion_shape = 0;
  int findobject_background_method = 0;
  int findobject_background_border_px = 2;
  int findobject_background_radius_px = 32;
  bool findobject_subpixel_enabled = false;
  double findobject_subpixel_min_gradient = 3.0;
  double findobject_subpixel_iso_threshold = 0.0;
  int findobject_geometry_basis = 0; // 0 GW discrete, 1 subpixel when valid.

  int radius = 0;
  // For FindCircle, these are absolute display radii for the auxiliary
  // inner/outer Gauge rings.  They are visual/editing aids and must not be
  // confused with algorithm gap/linegap.
  int inner_radius = 0;
  int outer_radius = 0;
  bool circle_arc_enabled = false;
  int circle_arc_start_deg = 0;
  int circle_arc_end_deg = 360;

  bool dirty = false;
  bool accepted = false;
};

struct ManualParamRegressionState {
  bool initialized = false;
  std::string status = "disabled";
  std::string reason = "Manual gauge must be accepted first.";
  CxParamRegressionTask task;
  CxParamRangeSet range_set;
  std::vector<CxParamCandidate> candidates;
  std::vector<CxParamEvalRecord> records;
  std::vector<CxParamAccuracyStats> accuracy_stats;
  int max_candidates = 12;
  int max_case_seconds = 10;
  int max_total_seconds = 60;
  int selected_candidate_index = 0;
  int edge_mode = 0; // 0 auto, 1 black-to-white, 2 white-to-black
  int contrast_percent = 20;
  int valid_length_percent = 50;
  int interference_length_percent = 20;
  int roughness = 8;
  int burr_filter_percent = 0;
  int measure_order = 3;
  int black_index = 50;
  int sample_points = 8;
  int catch_method = 0;
  bool enable_fast_measure = true;
  bool enable_filter = true;
  int tuning_tab = 0;
  std::string output_dir;
  std::string last_export_status;
  std::string last_export_reason;
  std::vector<std::string> exported_files;
};

struct ManualMetrologyUiState {
  bool enabled = false;
  int active_tab = 0;

  // One-based radial/scan Gauge Line selected for detailed analytics.
  int gauge_line_num = 1;

  // S1 behavior capture / scan profile preview.
  bool show_scan_profile = false;
  int scan_profile_source = 0; // 0 runtime, 1 gauge preview, 2 saved evidence
  int scan_profile_max_lines = 256;
  int scan_profile_sample_stride = 1;
  int scan_profile_edge_band_index = 0;
  int scan_profile_smoothing_radius = 1;

  bool profile_cursor_visible = true;
  int profile_cursor_sample_index = -1;
  int profile_cursor_sample_count = 0;
  int profile_cursor_permille = 500;
  bool profile_cursor_user_dragged = false;
  int profile_cursor_gauge_line_num = 0;
  std::string profile_cursor_source_key;




  // Candidate and feature-map review.
  bool show_edge_band_candidates = false;
  int candidate_rank = 0;
  int candidate_min_gradient = 8;
  int candidate_max_width = 80;
  int feature_map_mode = 0; // 0 gradient, 1 connected component, 2 confidence
  int feature_map_normalize = 1;

  // Boundary Trigger settings. Preview uses the same value semantics as the
  // FindLine/FindCircle/FindEllipse boundary response setters; direct scripts
  // opt in by reading the matching global_metrology_boundary_* values.
  bool boundary_preview_enabled = true;
  int boundary_baseline_mode = 1; // 0 none, 1 offset, 2 linear, 3 robust, 4 rolling
  int boundary_denoise_mode = 2;  // 0 none, 1 box, 2 gaussian, 3 median, 4 SG, 5 Haar
  int boundary_smoothing_radius = 2;
  int boundary_baseline_window = 12;
  int boundary_response_mode = 0; // CxBoundaryResponseMode
  int boundary_polarity = 2;      // 0 rising, 1 falling, 2 either
  int boundary_wavelet_scale = 4;
  int boundary_trigger_threshold_permille = 120;
  int boundary_level_permille = 500;
  int boundary_hysteresis_permille = 50;
  int boundary_gate_start_permille = 0;
  int boundary_gate_end_permille = 1000;
  int boundary_selection_mode = 4; // CxBoundarySelectionMode::NearestGateCenter
  int boundary_nth_candidate = 1;
  int boundary_min_plateau_width = 3;
  int boundary_min_amplitude_permille = 80;
  int boundary_pair_min_width = 2;
  int boundary_pair_max_width = 80;
  int boundary_pair_anchor_mode = 0; // 0 center, 1 first edge, 2 second edge
  int boundary_subpixel_mode = 2;
  bool boundary_show_conditioned = true;
  bool boundary_show_response = true;
  bool boundary_show_scalogram = false;
  int boundary_reference_mode = 0; // 0 none, 1 session reference
  bool boundary_reference_bound = false;
  int boundary_reference_position_permille = 500;
  std::string boundary_reference_label;
  std::vector<float> boundary_reference_profile;

  // Surface field / area / statistics.
  int surface_source = 0; // 0 image-gray, 1 segmentation-mask, 2 synthetic
  int surface_width = 256;
  int surface_height = 256;
  int surface_stride = 1;
  int surface_z_channel = 0;
  int surface_area_method = 1; // 1 four-triangle fan
  int histogram_bins = 256;
  int histogram_mode = 0; // 0 ADF, 1 BCDF, 2 both
  bool histogram_log_scale = false;

  int peak_max_count = 12;
  int peak_order = 0; // 0 position, 1 prominence
  int peak_min_prominence_permille = 20;
  int peak_min_distance_bins = 4;
  int peak_background = 1; // 0 zero, 1 bilateral minimum
  bool peak_invert = false;

  // Curve fitting defaults are designed to run without routine adjustment.
  int curve_fit_source = 0;   // 0 ADF, 1 BCDF, 2 scan profile
  int curve_fit_function = 0; // 0 Gaussian, 1 Lorentzian, 2 polynomial order 2
  bool curve_fit_auto_estimate = true;
  bool curve_fit_auto_plot = true;
  bool curve_fit_full_range = true;
  bool curve_fit_output_residual = false;
  int curve_fit_range_start_permille = 0;
  int curve_fit_range_end_permille = 1000;

  // Critical dimension defaults mirror the first stable CD preset.
  int critical_dimension_source = 0; // 0 scan profile, 1 ADF
  int critical_dimension_function =
      0; // 0 right edge, 1 left edge, 2..5 other presets
  bool critical_dimension_auto_fit = true;
  bool critical_dimension_full_range = true;
  int critical_dimension_range_start_permille = 0;
  int critical_dimension_range_end_permille = 1000;
  bool critical_dimension_draw_whole_circle = false;

  // Plane correction.
  bool enable_plane_correction = false;
  int plane_method = 1;         // 0 three-points, 1 OLS, 2 Huber
  int plane_reference_mode = 0; // 0 whole surface, 1 ROI, 2 mask
  int plane_huber_delta_permille = 100;

  // Physical unit conversion and Z perturbation.
  int x_unit = 2; // 0 pixel, 1 nm, 2 um, 3 mm
  int y_unit = 2;
  int z_unit = 2;
  int x_scale_permille = 1000;
  int y_scale_permille = 1000;
  int z_scale_permille = 1000;
  bool enable_gaussian_z = false;
  int gaussian_z_sigma_permille = 0;
  int gaussian_seed = 42;

  // ISO 1D roughness.
  bool enable_iso_roughness_1d = false;
  int roughness_profile_axis = 0; // 0 x, 1 y, 2 selected line
  int roughness_profile_index = 0;
  int roughness_cutoff_px = 0;
  int roughness_bins = 1024;

  bool height_peak_analysis_ready = false;
  std::string height_peak_analysis_status = "PENDING";
  std::string height_peak_analysis_reason = "analysis not run";
  std::string height_peak_source_ref;
  int height_peak_grid_width = 0;
  int height_peak_grid_height = 0;
  int height_peak_sample_count = 0;
  cxvision::metrology_analytics::CxSurfaceBasicStats height_peak_stats;
  std::vector<cxvision::metrology_analytics::CxHeightPeak> height_peaks;
  unsigned long long height_peak_analysis_revision = 0;

  unsigned long long edit_revision = 0;
  std::string last_summary;
};

struct EvidenceChainThumb {
  std::string case_id;
  std::string script_id;
  std::string script_path;
  std::string image_id;
  std::string image_path;
  std::string thumbnail_path;
  std::string target_id;
  std::string tool;

  std::string parameter_summary;
  std::string status;
  std::string reason;

  unsigned int texture_id = 0;
  int texture_w = 0;
  int texture_h = 0;
  bool texture_loaded = false;
};

struct ManualEvidenceItem {
  std::string case_id;
  std::string review_item;
  std::string level;
  std::string image_id;
  std::string target_id;
  std::string tool;

  std::string script_id;
  std::string parameter_profile_id;

  std::string gauge_status;
  std::string probe_status;
  std::string contract_status;
  std::string review_status;

  std::string image_path;
  std::string replay_package_path;
  // Non-empty only for Case-tab rows projected from a file-driven Evidence
  // Chain.
  std::string source_evidence_chain_path;
  std::string gauge_annotation_path;
};

struct ManifestImageItem {
  std::string image_id;
  std::string image_path;
  std::string level;
  std::string status;
};

struct TorchTrainingAnnotationShapeSnapshot {
  int class_id = 0;
  std::string stable_ref;
  std::string tool_id;
  std::string owner_type;
  std::string owner_ref;
  std::string owner_binding;
  std::string semantic_role;
  std::string shape_kind;
  std::string status = "editing";
  std::vector<double> points_xy;
  double center_x = 0.0;
  double center_y = 0.0;
  double radius = 0.0;
  double inner_radius = 0.0;
  double radius_x = 0.0;
  double radius_y = 0.0;
  double angle = 0.0;
  double half_width = 0.0;
  bool closed = false;
  bool editable = true;
  bool visible = true;
  bool result_element = false;
};

struct TorchTrainingImageItem {
  std::string image_id;
  std::string image_path;
  std::string case_id;
  std::string target_id;
  std::string source = "manual";   // evidence / manifest / manual
  std::string split = "train";     // train / val / test
  std::string label = "unlabeled"; // good / anomaly / unlabeled / pending
  std::string status = "pending";

  unsigned int texture_id = 0;
  int texture_w = 0;
  int texture_h = 0;
  bool texture_loaded = false;
  bool texture_failed = false;
  bool texture_placeholder = false;

  std::string annotation_status =
      "unlabeled"; // unlabeled / draft_pending_human_review / editing
  std::string annotation_reason;
  int annotation_shape_count = 0;
  int annotation_overlay_count = 0;
  std::vector<TorchTrainingAnnotationShapeSnapshot> annotation_shapes;
};

struct CxEvidenceDatasetImageBinding {
  std::string image_id;
  std::string image_path;
  std::string split = "train";
  std::string label = "unlabeled";
  std::string source = "evidence_dataset";
};

struct CxEvidenceAnnotationBinding {
  std::string image_id;
  std::string shape_kind = "RectShape";
  std::string semantic_role = "bbox";
  std::string owner_binding = "label_bbox";
  std::string label = "anomaly";
  int class_id = -1;
  double x0 = 0.0;
  double y0 = 0.0;
  double x1 = 0.0;
  double y1 = 0.0;
  bool normalized = false;
  bool closed = false;
  std::vector<double> points_xy;
};

struct CxTorchTrainingEpochMetric {
  int epoch = 0;
  double learning_rate = 0.0;
  double total_loss = 0.0;
  double box_loss = 0.0;
  double class_loss = 0.0;
  double dfl_loss = 0.0;
  double mask_loss = 0.0;
  int sample_count = 0;
  int instance_count = 0;
  struct ParameterGroup {
    std::string name;
    bool grad_defined = false;
    double grad_mean = 0.0;
    double grad_max = 0.0;
    double grad_norm = 0.0;
    double param_norm = 0.0;
    double update_norm = 0.0;
    int parameter_count = 0;
  };
  std::vector<ParameterGroup> parameter_groups;
};

struct CxTorchTrainingRunBinding {
  bool available = false;
  std::string status;
  std::string task;
  std::string dataset_source;
  std::string dataset_summary_path;
  std::string training_trace_path;
  std::string learning_curve_path;
  std::string incremental_model_manifest_path;
  std::string incremental_model_path;
  std::string optimizer;
  std::string lr_schedule = "constant";
  std::string loss_phase;
  int configured_epochs = 0;
  int completed_epochs = 0;
  int train_sample_count = 0;
  int train_instance_count = 0;
  int batches_per_epoch = 0;
  double learning_rate = 0.0;
  double min_learning_rate = 0.0;
  double weight_decay = 0.0;
  double box_loss_weight = 1.0;
  double class_loss_weight = 1.0;
  double dfl_loss_weight = 1.0;
  double mask_loss_weight = 1.0;
  bool early_stopped = false;
  int best_validation_epoch = 0;
  double best_validation_f1 = 0.0;
  int assignment_topk = 0;
  double classification_focal_gamma = 0.0;
  bool use_assignment_quality_targets = false;
  double postprocess_confidence_threshold = 0.0;
  double postprocess_iou_threshold = 0.0;
  int postprocess_max_detections = 0;
  bool postprocess_class_agnostic_nms = false;
  std::string checkpoint_selection_path;
  std::vector<CxTorchTrainingEpochMetric> epochs;

  bool HasRealMultiEpochSeries() const {
    return available && epochs.size() >= 2 && completed_epochs >= 2 &&
           configured_epochs >= 2 && train_sample_count > 0;
  }

  std::string TrainingCurveStatus() const {
    if (!available || epochs.empty())
      return "CURVE_SAMPLES_MISSING";
    if (!HasRealMultiEpochSeries())
      return "PENDING_EXTERNAL_TRAINING_CURVES";
    return "REAL_MULTI_EPOCH_SERIES";
  }
};

struct CxModelLineageUiNode {
  bool accepted = false;
  std::string model_id;
  std::string display_name;
  std::string node_kind;
  std::string root_release_id;
  std::string branch_id;
  std::vector<std::string> parent_model_ids;
  std::string task;
  std::string architecture_id;
  std::string output_contract;
  std::vector<std::string> capabilities;
  std::string capability_manifest_path;
  std::string model_manifest_path;
  std::string checkpoint_path;
  std::string checkpoint_hash;
  std::string checkpoint_hash_status;
  std::string class_ontology_path;
  std::string dataset_binding_path;
  std::string training_plan_path;
  std::string metric_bundle_path;
  std::string promotion_status;

  std::string admission_status;
  std::string parent_usage;
  bool development_parent_eligible = false;
  bool production_registry_write_allowed = true;
  std::string model_node_path;
  bool parent_selection_reviewed = false;
  bool parent_gate_satisfied = false;
  bool freeze_policy_reviewed = false;
  bool inference_difference_reviewed = false;
  std::string parent_selection_review_path;
  std::string freeze_policy_review_path;
  std::string inference_difference_review_path;
  std::string gate_status;
  std::string gate_reason;
};

struct CxModelLineageRejectedAsset {
  std::string path;
  std::string reason;
};

struct CxModelPerformanceStage {
  std::string name;
  double min_ms = 0.0;
  double mean_ms = 0.0;
  double p50_ms = 0.0;
  double p95_ms = 0.0;
  double p99_ms = 0.0;
  double max_ms = 0.0;
};

struct CxModelPerformanceProfile {
  bool available = false;
  std::string status;
  std::string profile_scope;
  std::string requested_device;
  std::string profile_path;
  double service_initialize_ms = 0.0;
  int sample_count = 0;
  std::vector<CxModelPerformanceStage> stages;
  std::string reason;
};

struct CxValidationDatasetReadiness {
  bool available = false;
  std::string status = "PENDING_VALIDATION_DATASET_SCAN";
  std::string reason;
  std::string dataset_manifest_path;
  int train_sample_count = 0;
  int validation_sample_count = 0;
  int missing_required_asset_count = 0;
  int source_lineage_overlap_count = 0;
  bool frozen = false;
};

struct CxModelLineageOperationSelfTestProjection {
  bool available = false;
  std::string status = "NO_OPERATION_SELFTEST_RECORD";
  std::string created_at;
  std::string report_path;
  std::string validation_gate_status;
  std::string validation_gate_reason;
  std::string compare_preflight;
  std::string augmentation_retrain_preflight;
  int accepted_nodes = 0;
  int rejected_nodes = 0;
  int parameter_assets_verified = 0;
  int parameter_assets_rejected = 0;
  int parent_links_present = 0;
  int parent_links_missing = 0;
  std::string human_review_status = "PENDING_HUMAN_REVIEW";
  bool promotion_allowed = false;
};

struct CxModelWorkflowUiNode {
  std::string node_id;
  std::string display_name;
  std::string node_kind;
  std::vector<std::string> parent_node_ids;
  std::string asset_ref;
  std::string result_ref;
  std::string status;
  std::string detail;
};

struct CxModelWorkflowProjection {
  bool available = false;
  std::string display_name;
  std::string workflow_path;
  std::string status = "NO_WORKFLOW_MANIFEST";
  bool promotion_allowed = false;
  std::vector<CxModelWorkflowUiNode> nodes;
};

struct ScriptEvidenceThumb {
  std::string candidate_id;
  std::string candidate_dir;
  std::string evidence_binding_path;
  std::string parameter_snapshot_path;
  std::string runtime_globals_path;
  std::string gauge_annotation_path;
  std::string working_script_snapshot_path;
  bool is_candidate = false;
  bool has_saved_state = false;
  std::string source_evidence_script_path;
  std::string case_id;
  std::string review_item;
  std::string script_id;
  std::string script_path;
  std::string image_id;
  std::string image_path;
  std::string thumbnail_path;
  std::string target_id;
  std::string tool;
  std::string parameter_summary;
  std::string evidence_output_root;
  std::string contract_id;
  std::string expected_result;
  std::string expected_policy_guard;
  std::string evidence_level;
  std::string evidence_case_role;
  std::string source_case_id;
  bool manual_review_required = true;
  bool promotion_candidate = false;
  std::string evidence_category_override;
  std::string evidence_group_override;
  std::string evidence_head_folder;
  std::string evidence_case_folder;
  std::string workflow_id;
  std::string workflow_stage;
  std::string workflow_status;
  std::string workflow_prerequisites;
  std::string dataset_role;
  std::string annotation_policy;
  std::string gate_policy;
  std::string parent_model_ref;
  std::string child_model_ref;
  int workflow_stage_index = 0;
  int workflow_stage_count = 0;
  bool dataset_frozen = false;
  std::string admission_status = "REFERENCE_ONLY";
  std::string admission_reason;
  std::string dataset_summary_ref;
  std::string training_receipt_ref;
  std::string candidate_artifact_ref;
  std::string evaluation_report_ref;
  std::string evidence_bundle_ref;
  std::string rollback_model_ref;
  std::string quality_conclusion_ref;
  std::string training_config_ref;
  std::string model_manifest_ref;
  std::string inference_config_ref;
  std::string quality_policy_ref;
  std::string ontology_ref;
  std::string failure_samples_ref;
  std::string status;
  std::string reason;
  std::string primary_object_type;
  std::string primary_object_name;
  std::string primary_object_status;
  unsigned int texture_id = 0;
  int texture_w = 0;
  int texture_h = 0;
  bool texture_loaded = false;
  bool texture_failed = false;
  bool texture_placeholder = false;
  std::vector<CxEvidenceDatasetImageBinding> dataset_images;
  std::vector<CxEvidenceAnnotationBinding> annotations;
  CxTorchTrainingRunBinding training_run;
};

struct CxEvidenceEditableObjectRef {
  std::string type;
  std::string name;
  int declared_line = 0;
};

struct ScriptEvidenceGroup {
  std::string script_id;
  std::string script_path;
  std::string label;
  std::vector<ScriptEvidenceThumb> thumbs;
};

struct CxEvidenceSelectionSnapshot {
  bool valid = false;

  int group_index = -1;
  int thumb_index = -1;

  std::string case_id;
  std::string review_item;

  std::string candidate_id;
  std::string candidate_dir;
  std::string evidence_binding_path;
  std::string parameter_snapshot_path;
  std::string runtime_globals_path;
  std::string gauge_annotation_path;
  std::string working_script_snapshot_path;
  bool is_candidate = false;
  bool has_saved_state = false;
  std::string source_evidence_script_path;

  std::string script_id;
  std::string script_path;

  std::string image_id;
  std::string image_path;

  std::string target_id;
  std::string tool;

  std::string parameter_profile_id;
  std::string parameter_summary;

  std::string evidence_output_root;
  std::string contract_id;
  std::string expected_result;
  std::string expected_policy_guard;
  std::string evidence_level;
  std::string evidence_case_role;
  std::string source_case_id;
  bool manual_review_required = true;
  bool promotion_candidate = false;
  std::string evidence_group_override;
  std::string workflow_id;
  std::string workflow_stage;
  std::string workflow_status;
  std::string workflow_prerequisites;
  std::string dataset_role;
  std::string annotation_policy;
  std::string gate_policy;
  std::string parent_model_ref;
  std::string child_model_ref;
  int workflow_stage_index = 0;
  int workflow_stage_count = 0;
  bool dataset_frozen = false;
  std::string admission_status = "REFERENCE_ONLY";
  std::string admission_reason;
  std::string dataset_summary_ref;
  std::string training_receipt_ref;
  std::string candidate_artifact_ref;
  std::string evaluation_report_ref;
  std::string evidence_bundle_ref;
  std::string rollback_model_ref;
  std::string quality_conclusion_ref;
  std::string training_config_ref;
  std::string model_manifest_ref;
  std::string inference_config_ref;
  std::string quality_policy_ref;
  std::string ontology_ref;
  std::string failure_samples_ref;

  std::string status;
  std::string reason;

  std::string source;

  std::string primary_object_type;
  std::string primary_object_name;
  std::string primary_object_status;
  std::vector<CxEvidenceEditableObjectRef> editable_objects;
  std::vector<CxEvidenceDatasetImageBinding> dataset_images;
  std::vector<CxEvidenceAnnotationBinding> annotations;
  CxTorchTrainingRunBinding training_run;
};

struct ScriptEvidenceRowRef {
  int group_index = -1;
  int thumb_index = -1;
  bool is_group_header = false;
  std::string label;
};

struct ManualFindLineEdgeParamState {
  bool initialized = false;
  int threshold = 20;
  int method = 0;
  int linegap = 6;
  int wgap = 8;
  int hgap = 32;
  int filterprofile = 0;
};

struct ManualFindCircleEdgeParamState {
  bool initialized = false;
  int threshold = 20;
  int method = 0;
  int linegap = 3;
  int gap = 6;
};

struct ManualOperationTraceEvent {
  unsigned long long sequence = 0;
  unsigned long long key_parameter_edit_revision = 0;
  std::string operation;
  std::string status;
  std::string reason;
  std::string summary;
  std::string editor_source;
  std::string loaded_script_path;
  CxEvidenceSelectionSnapshot evidence_selection;
  ManualGaugeState gauge;
  std::unordered_map<std::string, int> globals;
};

struct ManualTestContext {
  std::string script_file_path;
  std::string image_file_path;
  std::string data_file_path;
  std::string model_file_path;
  std::string param_file_path;
  std::string bound_state_node_id;
  std::string bound_state_script_path;
  std::string editor_text;
  std::string analyzed_text;
  std::string editor_source = "manual";
  std::string loaded_script_path;
  std::string case_directory = "docs/notes/cxscript_case";
  std::string manual_gauge_output_root = "cxscript_runs/manual_gauge_workbench";
  std::string trace_status = "PENDING";
  std::string trace_reason = "not executed";
  std::string run_state = "idle";
  std::string debug_action = "none";
  std::string debug_status = "PENDING";
  std::string debug_reason = "not started";
  bool cxparser_ext_debug_ok = false;
  std::string cxparser_ext_debug_status;
  std::string cxparser_ext_debug_reason;
  std::string debug_parser_output;
  std::string user_expected;
  std::string codex_task;
  std::string forbidden_changes =
      "No coordinators, routers, UnifiedEntry, operator catalogs, automatic "
      "long-chain runs, fake PASS, Qt migration, or dev_analysis_gui business "
      "logic.";

  std::string catalog_path;
  bool catalog_loaded = false;
  std::vector<CxScriptCatalogEntry> catalog_entries;
  int current_line = 0;
  std::vector<ScriptLineView> line_views;
  std::vector<CxScriptLineView> cxparser_ext_line_views;
  std::vector<CxScriptStatementView> cxparser_ext_statement_views;
  std::vector<CxScriptObjectAssignmentView> cxparser_ext_object_assignments;
  std::vector<ScriptVariableView> global_variable_views = {
      {"Image", "global_matInput", "uninitialized", 0, "not_initialized",
       "D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/01.jpg", false}};
  std::vector<ScriptVariableView> variable_views;
  std::vector<ScriptObjectView> object_views;
  std::vector<RuntimeObjectView> runtime_objects;
  std::uint64_t runtime_overlay_version = 0;
  std::vector<DebugStepSnapshot> debug_snapshots;
  DebugStepSnapshot current_debug_snapshot;
  std::unordered_map<std::string, int> runtime_int_vars;
  // Identifies the Evidence/task profile that seeded the current Torch
  // request defaults.  User edits are preserved while the key is unchanged.
  std::string torch_parameter_defaults_key;
  ResultRefView current_result_ref;

  std::string geometry_summary;
  std::string image_overlay_summary;
  std::string findcircle_debug_snapshot_summary;

  std::string active_script_case_name;
  std::string active_script_case_path;
  std::string active_script_case_purpose;

  std::string active_case_id;
  std::string active_image_id;
  std::string active_target_id;

  std::string runtime_current_status = "PENDING";
  std::string runtime_current_node;
  std::string runtime_current_connect;
  bool editor_dirty = false;
  bool stop_requested = false;
  bool show_image = true;
  bool pick_points = false;
  bool test_points = false;
  bool show_result_overlay = false;
  bool show_line_gauge_scan_lines = false;
  bool show_circle_gauge_scan_lines = false;
  bool show_ellipse_gauge_scan_lines = false;
  // Image input is immutable.  FastMatch diagnostics are opt-in render-only
  // layers drawn by ImGui; they must never be confused with source pixels.
  bool show_fastmatch_debug_vectors = false;
  // Independent FastMatch Learn display layers.  Keeping the result layer
  // separate lets an operator inspect FindLine scans and conclusion points
  // without the learned A/B model obscuring other Image View overlays.
  bool show_fastmatch_learn_result = false;
  bool show_fastmatch_learn_scan_ticks = false;
  int fastmatch_selected_learn_edge = 0; // Top, Bottom, Left, Right.
  bool show_fastmatch_compare_gap_pairs = false;
  bool show_fastmatch_keypoint_tangents = false;
  bool show_fastmatch_filtered_points = false;
  bool show_fastmatch_normal_trace_domain = false;
  bool show_fastmatch_normal_trace_ann_component = false;
  bool show_fastmatch_normal_trace_path = false;
  bool show_fastmatch_normal_trace_pairs = false;




  // UI slot 2 maps to runtime -1 because the public "last edge" selector is -1.
  int findline_selected_scan_edge = 0;
  int findline_scan_edge_count = 4;
  int findline_best_fit_edge = 0; // Runtime/manual best fitting point-set edge.
  int findline_recommended_fit_edge =
      0; // Future advisor/param regression recommendation.
  int findline_relation_edge = 0; // Future combined/related point-set edge.
  int findline_attach_edge = 0;   // Future annotation attach/binding edge.
  bool findline_point_consistency_enabled = false;
  int findline_point_consistency_range =
      0; // UI materializes default to half of search half-width.
  std::vector<ManualFindLineEdgeParamState> findline_edge_params;
  // 0 = accept all eligible crossings; 1..N = Nth candidate crossing on each
  // radial scan line.  This is not the angular A0/A1 scan-sector selection.
  int findcircle_selected_scan_edge = 0;
  int findcircle_scan_edge_count = 4;
  int findcircle_best_fit_edge = 0; // Runtime/manual best fitting arc.
  int findcircle_recommended_fit_edge =
      0; // Future advisor/param regression recommendation.
  int findcircle_relation_edge = 0; // Future combined/related arc point-set.
  int findcircle_attach_edge = 0;   // Future annotation attach/binding arc.
  bool findcircle_point_consistency_enabled = false;
  int findcircle_point_consistency_range = 0;
  std::vector<ManualFindCircleEdgeParamState> findcircle_edge_params;
  int findellipse_selected_scan_edge = 0;
  int findellipse_scan_edge_count = 3;
  int findellipse_best_fit_edge = 0;
  int findellipse_recommended_fit_edge = 0;
  int findellipse_relation_edge = 0;
  int findellipse_attach_edge = 0;
  bool findellipse_point_consistency_enabled = false;
  int findellipse_point_consistency_range = 0;
  std::vector<ManualFindCircleEdgeParamState> findellipse_edge_params;
  bool source_preview_enabled = false;
  int manual_elements_count = 0;
  ManualGaugeState current_gauge;
  ManualParamRegressionState param_regression;
  ManualMetrologyUiState metrology_ui;
  cxvision::metrology_analytics::ManualConsoleAnalyticsSmokeUiState
      analytics_smoke_ui;

  GaugeHandleType active_gauge_handle = GaugeHandleType::None;
  float gauge_drag_start_x = 0.0f;
  float gauge_drag_start_y = 0.0f;
  ManualGaugeState drag_start_gauge;

  std::vector<ManualEvidenceItem> evidence_items;
  bool evidence_items_seed_attempted = false;
  std::size_t evidence_items_seed_catalog_count = 0;

  bool workbench_assets_loaded = false;
  std::string manifest_path;
  bool manifest_loaded = false;
  std::string manifest_load_reason;
  std::vector<std::string> image_manifest_entries;
  std::vector<ManifestImageItem> image_manifest_items;

  std::vector<TorchTrainingImageItem> torch_training_images;
  int selected_torch_training_image = -1;
  int torch_training_annotation_class_id = 0;
  std::string torch_training_new_image_path;
  std::string torch_training_image_status = "PENDING";
  std::string torch_training_image_reason =
      "training image set not initialized";

  std::string torch_training_last_exported_dataset_manifest;
  std::string torch_training_last_exported_case_id;
  std::string torch_training_inference_model_manifest;
  std::string torch_training_inference_device = "cuda";
  std::string torch_training_inference_status = "PENDING_SELECTED_IMAGE_INFERENCE";
  std::string torch_training_inference_reason =
      "select an image and provide a model manifest";
  std::string torch_training_inference_result_ref;
  std::string torch_training_inference_overlay_ref;
  std::string torch_training_inference_candidates_ref;
  double torch_training_inference_runtime_ms = 0.0;
  int torch_training_inference_detection_count = 0;
  CxTorchTrainingRunBinding torch_training_run;
  std::string model_lineage_scan_root = "cxscript_runs";
  bool model_lineage_scan_attempted = false;
  std::string model_lineage_scan_status = "PENDING";
  std::string model_lineage_scan_reason =
      "model lineage assets have not been scanned";
  std::string model_lineage_scan_debug_path;
  std::vector<CxModelLineageUiNode> model_lineage_nodes;
  std::vector<CxModelLineageRejectedAsset> model_lineage_rejected;
  int selected_model_lineage_node = -1;
  std::string model_lineage_parent_selection_reason;
  std::string model_lineage_parent_selection_status = "PENDING_HUMAN_REVIEW";
  std::string model_lineage_parent_selection_message;
  std::string model_lineage_parent_selection_review_path;
  std::string model_lineage_operation_status = "PENDING_NODE_OPERATION";
  std::string model_lineage_operation_message;
  bool model_performance_profile_scan_attempted = false;
  std::string model_performance_profile_scan_root = "cxscript_runs/yolov8n_incremental_acceptance";
  CxModelPerformanceProfile model_performance_profile;
  bool validation_dataset_scan_attempted = false;
  std::string validation_dataset_scan_root = "cxscript_runs";
  CxValidationDatasetReadiness validation_dataset;
  bool model_lineage_operation_selftest_scan_attempted = false;
  std::string model_lineage_operation_selftest_scan_root = "cxscript_runs";
  CxModelLineageOperationSelfTestProjection model_lineage_operation_selftest;
  bool model_workflow_scan_attempted = false;
  std::string model_workflow_scan_root = "cxscript_runs";
  CxModelWorkflowProjection model_workflow;
  int selected_model_workflow_node = -1;
  std::string model_workflow_operation_status = "PENDING_NODE_SELECTION";
  std::string model_workflow_operation_message;
  bool torch_training_latest_scan_attempted = false;
  bool torch_training_process_running = false;
  std::uintptr_t torch_training_process_handle = 0;
  std::string torch_training_active_output_dir;
  std::string torch_training_active_curve_path;
  std::string torch_training_active_result_path;
  bool geometry_aug_include_brightness = true;
  bool geometry_aug_include_local_gap = true;
  bool geometry_aug_include_jagged_cut = true;
  bool geometry_aug_include_line_break = true;
  bool geometry_aug_require_source_disjoint_validation = true;
  float geometry_aug_train_brightness_scale = 0.72f;
  float geometry_aug_test_brightness_scale = 0.42f;
  int geometry_aug_gap_width_px = 14;
  int geometry_aug_gap_height_px = 22;
  int geometry_aug_jagged_px = 5;
  int geometry_aug_line_break_px = 14;
  int geometry_aug_epochs = 0;
  // Display capacity only. It never changes the training plan; it keeps the
  // live curve's horizontal axis stable while new epochs arrive.
  int torch_training_epoch_axis_capacity = 100;
  // Display capacity only. Keeping the loss origin at (0, 0) makes a live
  // polyline directly comparable to its completed run.
  float torch_training_loss_axis_maximum = 30.0f;
  float geometry_aug_learning_rate = 0.0f;
  bool yolo_training_tuning_loaded = false;
  std::string yolo_training_tuning_source;
  int yolo_training_batch_size = 0;
  int yolo_training_input_size = 0;
  int yolo_training_max_batches_per_epoch = 0;
  int yolo_training_seed = 0;
  int yolo_training_assignment_topk = 0;
  float yolo_training_classification_focal_gamma = 0.0f;
  bool yolo_training_use_assignment_quality_targets = false;
  bool yolo_training_global_multiscale_assignment = false;
  std::vector<std::string> yolo_training_class_names;
  std::vector<float> yolo_training_class_loss_weights;
  int yolo_training_validation_interval = 0;
  int yolo_training_early_stop_patience = 0;
  int yolo_training_minimum_complete_epochs = 0;
  float yolo_training_minimum_f1_delta = 0.0f;
  bool yolo_training_restore_best_validation_checkpoint = false;
  float yolo_training_postprocess_confidence_threshold = 0.0f;
  float yolo_training_postprocess_iou_threshold = 0.0f;
  int yolo_training_postprocess_max_detections = 0;
  bool yolo_training_postprocess_class_agnostic_nms = false;
  float yolo_training_evaluation_match_iou_threshold = 0.0f;
  // Geometry ROI head parameters are case-external controls.  They are loaded
  // from the selected versioned training plan, can be drafted in the UI, and
  // are echoed into every runtime plan/receipt; they never enable geometry
  // training without a sidecar target manifest.
  bool yolo_training_geometry_roi_head_enabled = false;
  int yolo_training_geometry_roi_hidden_channels = 64;
  int yolo_training_geometry_roi_pooled_size = 4;
  int yolo_training_geometry_roi_polygon_vertex_count = 8;
  float yolo_training_geometry_roi_parameter_weight = 1.0f;
  float yolo_training_geometry_roi_contour_weight = 1.0f;
  float yolo_training_geometry_roi_continuity_weight = 0.5f;
  float yolo_training_geometry_roi_uncertainty_weight = 0.25f;
  float yolo_training_geometry_roi_continuity_positive_weight = 1.0f;
  float yolo_training_geometry_roi_calibration_weight = 0.1f;
  std::string yolo_training_geometry_target_manifest;
  // Affine channel controls describe the asset-generation contract. They are
  // exported with the runtime plan and must match the selected sidecar
  // dataset; compound is evaluation-only and cannot be selected for training.
  int yolo_training_affine_channel_mode = 0; // 0=rotation, 1=scale, 2=compound evaluation
  float yolo_training_rotation_train_positive_deg = 7.0f;
  float yolo_training_rotation_train_negative_deg = -8.0f;
  float yolo_training_rotation_validation_deg = 14.0f;
  float yolo_training_rotation_holdout_deg = -18.0f;
  float yolo_training_scale_train_down = 0.88f;
  float yolo_training_scale_train_up = 1.12f;
  float yolo_training_scale_validation = 0.90f;
  float yolo_training_scale_holdout = 1.24f;
  float yolo_training_compound_validation_scale = 0.90f;
  float yolo_training_compound_validation_rotation_deg = 11.0f;
  float yolo_training_compound_holdout_scale = 1.16f;
  float yolo_training_compound_holdout_rotation_deg = -16.0f;
  // Geometry auto-tune is a separate, evidence-driven inference profile
  // search. Defaults are loaded from its versioned policy asset; the GUI
  // exposes the requested profile but never promotes a model automatically.
  bool geometry_auto_tune_policy_loaded = false;
  std::string geometry_auto_tune_policy_source;
  std::string geometry_auto_tune_ontology_source;
  int geometry_auto_tune_topk_per_roi = 0;
  int geometry_auto_tune_max_trials = 0;
  float geometry_auto_tune_match_iou_threshold = 0.0f;
  std::string geometry_auto_tune_scan_root = "cxscript_runs";
  std::string geometry_auto_tune_summary_path;
  std::string geometry_auto_tune_status = "AUTO_TUNE_NOT_RUN";
  std::string geometry_auto_tune_quality_state = "PENDING";
  std::string geometry_auto_tune_production_state = "REFERENCE_ONLY";
  std::string geometry_auto_tune_reason = "no auto-tune evidence loaded";
  float geometry_auto_tune_selected_confidence = 0.0f;
  int geometry_auto_tune_selected_topk = 0;
  float geometry_auto_tune_validation_f1 = 0.0f;
  float geometry_auto_tune_holdout_f1 = 0.0f;
  std::string geometry_aug_run_status = "PENDING";
  std::string geometry_aug_run_reason = "not generated from GUI";
  std::string geometry_aug_output_dir;
  std::string geometry_aug_plan_path;
  std::string geometry_aug_dataset_manifest_path;
  std::string geometry_aug_training_request_path;

  std::vector<EvidenceChainThumb> evidence_chain_thumbs;
  int selected_evidence_thumb = -1;

  std::vector<ScriptEvidenceGroup> script_evidence_groups;
  // The active Evidence list stays small.  The catalog is metadata-only and
  // contains cases that an operator may explicitly add to the active list.
  // It never loads a thumbnail or activates an image by itself.
  std::vector<ScriptEvidenceThumb> hidden_evidence_case_catalog;
  std::unordered_set<std::string> selected_hidden_evidence_case_keys;
  // Runtime-only operator exclusions. Asset-declared manual_visible supplies
  // an initial queue only; a right-click Hide must win until explicitly added
  // again in this session.
  std::unordered_set<std::string> operator_hidden_evidence_case_keys;
  std::string hidden_evidence_case_filter;
  bool hidden_evidence_case_catalog_dirty = true;
  int selected_evidence_group = -1;
  std::string script_evidence_case_filter;
  bool script_evidence_groups_dirty = true;
  unsigned long long script_evidence_groups_revision = 0;
  unsigned long long script_evidence_groups_debug_revision = 0;

  std::vector<ScriptEvidenceRowRef> script_evidence_row_refs;
  bool script_evidence_row_refs_dirty = true;

  int script_evidence_thumb_load_budget_per_frame = 4;
  int script_evidence_thumb_load_count_this_frame = 0;
  int last_evidence_click_group = -1;
  int last_evidence_click_thumb = -1;
  double last_evidence_click_time = -1.0;
  std::unordered_map<std::string, std::string> evidence_category_overrides;
  // Stable evidence identity keys, newest first.  These are persisted as
  // navigation history and are resolved against freshly scanned assets.
  std::vector<std::string> recent_evidence_case_keys;
  bool recent_evidence_cases_loaded = false;

  CxEvidenceSelectionSnapshot current_evidence_selection;
  std::string last_evidence_candidate_id;
  std::string last_evidence_candidate_dir;
  std::string last_evidence_candidate_reason;

  // A Save-And-Run request is a value snapshot, not a reference to the live
  // UI state.  The next ImGui frame may rebuild Evidence/Runtime views, so the
  // execution path must consume this frozen copy rather than re-reading a
  // possibly reseeded current_gauge/runtime_int_vars pair.
  bool has_pending_execution_snapshot = false;
  // A saved Evidence case restores only value-semantic input state.  The
  // parser-owned objects from the previously selected case must never be
  // reused, so the Debug Compiler consumes this flag on the next UI pass and
  // recreates them through the normal serial execution path.
  bool pending_evidence_selection_rehydrate = false;
  ManualGaugeState pending_execution_gauge;
  std::unordered_map<std::string, int> pending_execution_globals;
  std::string pending_execution_candidate_id;

  // Candidate persistence changes Evidence Chain vectors.  It must occur on
  // the next UI frame, after the Key Parameter Controls traversal has
  // released any row/group references that a vector reallocation can stale.
  bool has_pending_candidate_save = false;
  bool pending_candidate_save_requests_run = false;
  ManualGaugeState pending_candidate_save_gauge;
  std::unordered_map<std::string, int> pending_candidate_save_globals;

  unsigned long long manual_operation_trace_sequence = 0;
  std::vector<ManualOperationTraceEvent> pending_manual_operation_trace_events;

  unsigned long long key_parameter_edit_revision = 0;
  std::string last_key_parameter_edit_summary;
  // Revision captured by the most recent FastMatch action. This lets the UI
  // distinguish newly edited parameters from the snapshot being executed.
  unsigned long long fastmatch_last_requested_revision = 0;
  int fastmatch_last_requested_action = 0;
  bool apply_gauge_to_shape_requested = false;
  std::string pending_annotation_tool_id;
  std::string pending_annotation_tool_reason;

  CxEvidenceSelfTestResult last_evidence_selftest_result;
};

void SeedDefaultManualGlobals(ManualTestContext &context,
                              const std::string &scriptPath);

struct ScriptSnippet {
  std::string name;
  std::string description;
  std::string text;
  std::string source_path;
  bool runnable = true;

  std::string parameter_policy_id;
  std::string parameter_role;
  bool is_product_default = false;
  bool is_stage25_default = false;
  bool recommended = false;

  std::string script_id;
  std::string expected_result;
  std::string expected_result_status;
  std::string expected_policy_guard;
  std::string contract_path;
  std::string label;
  std::string category;
  std::string failure_hint;
  bool expects_measure_points = false;
  bool expects_fit_line = false;
  bool expected_filter_failure = false;
};

struct ManualCatalogVisibleEntry {
  std::string script_id;
  std::string label;
  std::string path;
  std::string tool;
  std::string expected_result;
  std::string expected_policy_guard;
  std::string contract_path;
  std::string parameter_policy_id;
  std::string parameter_role;
};

struct ManualCatalogHiddenEntry {
  std::string script_id;
  std::string label;
  std::string path;
  std::string tool;
  std::string expected_result;
  std::string hidden_reason;
  std::string contract_path;
};

struct ManualCatalogUiState {
  bool loaded = false;
  std::string catalog_path;
  std::string load_status;
  std::string load_reason;

  std::vector<ManualCatalogVisibleEntry> visible_scripts;
  std::vector<ManualCatalogHiddenEntry> hidden_scripts;
  std::vector<ManualCatalogHiddenEntry> advanced_scripts;
};

struct DirectCapabilityMethod {
  std::string name;
  std::string status;
};

struct DirectCapability {
  std::string module;
  std::string type;
  std::string status;
  std::vector<DirectCapabilityMethod> methods;
};

bool UpdateRuntimeFindlineSetlineFromUi(ManualTestContext &context,
                                        const std::string &objectName, float x0,
                                        float y0, float x1, float y1,
                                        float scale, std::string &outReason);

inline float ManualGaugeDistanceSquared(float x1, float y1, float x2,
                                        float y2) {
  const float dx = x2 - x1;
  const float dy = y2 - y1;
  return dx * dx + dy * dy;
}

struct LineGaugeGeometry {
  ImVec2 p0;
  ImVec2 p1;
  ImVec2 center;
  ImVec2 tangent;
  ImVec2 normal;
  float length = 0.0f;
  float half_width = 0.0f;

  ImVec2 corner0;
  ImVec2 corner1;
  ImVec2 corner2;
  ImVec2 corner3;

  ImVec2 w_plus;
  ImVec2 w_minus;

  bool valid = false;
};

struct CircleGaugeGeometry {
  ImVec2 center;
  float radius;
  float innerRadius;
  float outerRadius;

  ImVec2 radiusHandle;
  ImVec2 innerHandle;
  ImVec2 outerHandle;
};

inline CircleGaugeGeometry
BuildCircleGaugeGeometry(const ManualGaugeState &gauge) {
  CircleGaugeGeometry geo;

  geo.center = ImVec2((float)gauge.circle_cx, (float)gauge.circle_cy);

  geo.radius = (float)std::max(1, gauge.radius);

  const float fallbackBand = (float)std::max(1, gauge.linegap);
  geo.innerRadius = gauge.inner_radius > 0
                        ? (float)gauge.inner_radius
                        : std::max(1.0f, geo.radius - fallbackBand);
  geo.outerRadius = gauge.outer_radius > 0 ? (float)gauge.outer_radius
                                           : geo.radius + fallbackBand;

  if (geo.outerRadius <= geo.innerRadius)
    geo.outerRadius = geo.innerRadius + 1.0f;

  geo.radiusHandle = ImVec2(geo.center.x + geo.radius, geo.center.y);
  geo.innerHandle = ImVec2(geo.center.x + geo.innerRadius, geo.center.y);
  geo.outerHandle = ImVec2(geo.center.x + geo.outerRadius, geo.center.y);

  return geo;
}

inline LineGaugeGeometry BuildLineGaugeGeometry(const ManualGaugeState &gauge) {
  LineGaugeGeometry g;

  g.p0 = ImVec2((float)gauge.line_x0, (float)gauge.line_y0);
  g.p1 = ImVec2((float)gauge.line_x1, (float)gauge.line_y1);

  const float dx = g.p1.x - g.p0.x;
  const float dy = g.p1.y - g.p0.y;
  const float len = std::sqrt(dx * dx + dy * dy);

  if (len < 1.0f)
    return g;

  g.valid = true;
  g.length = len;
  g.half_width = std::max(1.0f, (float)gauge.tool_half_width);

  g.tangent = ImVec2(dx / len, dy / len);
  g.normal = ImVec2(-g.tangent.y, g.tangent.x);

  g.center = ImVec2((g.p0.x + g.p1.x) * 0.5f, (g.p0.y + g.p1.y) * 0.5f);

  g.corner0 = ImVec2(g.p0.x + g.normal.x * g.half_width,
                     g.p0.y + g.normal.y * g.half_width);
  g.corner1 = ImVec2(g.p1.x + g.normal.x * g.half_width,
                     g.p1.y + g.normal.y * g.half_width);
  g.corner2 = ImVec2(g.p1.x - g.normal.x * g.half_width,
                     g.p1.y - g.normal.y * g.half_width);
  g.corner3 = ImVec2(g.p0.x - g.normal.x * g.half_width,
                     g.p0.y - g.normal.y * g.half_width);

  g.w_plus = ImVec2(g.center.x + g.normal.x * g.half_width,
                    g.center.y + g.normal.y * g.half_width);
  g.w_minus = ImVec2(g.center.x - g.normal.x * g.half_width,
                     g.center.y - g.normal.y * g.half_width);

  return g;
}

inline GaugeHandleType HitTestGaugeHandle(const ManualGaugeState &gauge,
                                          float mouse_x, float mouse_y,
                                          float handle_radius) {
  const float r2 = handle_radius * handle_radius;

  if (gauge.tool == "Findcircle" || gauge.has_circle_gauge) {
    if (!gauge.has_circle_gauge)
      return GaugeHandleType::None;

    if (ManualGaugeDistanceSquared((float)gauge.circle_cx,
                                   (float)gauge.circle_cy, mouse_x,
                                   mouse_y) <= r2) {
      return GaugeHandleType::CircleCenter;
    }

    const CircleGaugeGeometry geom = BuildCircleGaugeGeometry(gauge);

    if (ManualGaugeDistanceSquared(geom.radiusHandle.x, geom.radiusHandle.y,
                                   mouse_x, mouse_y) <= r2) {
      return GaugeHandleType::CircleRadius;
    }

    if (ManualGaugeDistanceSquared(geom.innerHandle.x, geom.innerHandle.y,
                                   mouse_x, mouse_y) <= r2) {
      return GaugeHandleType::CircleInner;
    }

    if (ManualGaugeDistanceSquared(geom.outerHandle.x, geom.outerHandle.y,
                                   mouse_x, mouse_y) <= r2) {
      return GaugeHandleType::CircleOuter;
    }

    return GaugeHandleType::None;
  }

  if (!gauge.has_line_gauge)
    return GaugeHandleType::None;

  LineGaugeGeometry geom = BuildLineGaugeGeometry(gauge);
  if (!geom.valid)
    return GaugeHandleType::None;

  if (ManualGaugeDistanceSquared(geom.p0.x, geom.p0.y, mouse_x, mouse_y) <= r2)
    return GaugeHandleType::LineP0;

  if (ManualGaugeDistanceSquared(geom.p1.x, geom.p1.y, mouse_x, mouse_y) <= r2)
    return GaugeHandleType::LineP1;

  if (ManualGaugeDistanceSquared(geom.center.x, geom.center.y, mouse_x,
                                 mouse_y) <= r2)
    return GaugeHandleType::LineCenter;

  if (ManualGaugeDistanceSquared(geom.w_plus.x, geom.w_plus.y, mouse_x,
                                 mouse_y) <= r2)
    return GaugeHandleType::LineWidthPlus;

  if (ManualGaugeDistanceSquared(geom.w_minus.x, geom.w_minus.y, mouse_x,
                                 mouse_y) <= r2)
    return GaugeHandleType::LineWidthMinus;

  return GaugeHandleType::None;
}

inline int ClampCircleRadiusToImage(int cx, int cy, int radius, int imageW,
                                    int imageH) {
  const int maxR =
      std::min(std::min(cx, imageW - 1 - cx), std::min(cy, imageH - 1 - cy));
  return std::clamp(radius, 1, maxR);
}

inline void DragGaugeHandle(ManualGaugeState &gauge,
                            const ManualGaugeState &drag_start_gauge,
                            GaugeHandleType handle,
                            const ImVec2 &mouse_image_pos,
                            const ImVec2 &drag_start_mouse_image,
                            bool shift_down, int imageW = 0, int imageH = 0) {
  if (gauge.tool == "Findcircle" || gauge.has_circle_gauge) {
    if (!gauge.has_circle_gauge)
      return;

    switch (handle) {
    case GaugeHandleType::CircleCenter: {
      const float dx = mouse_image_pos.x - drag_start_mouse_image.x;
      const float dy = mouse_image_pos.y - drag_start_mouse_image.y;
      gauge.circle_cx =
          drag_start_gauge.circle_cx + static_cast<int>(std::round(dx));
      gauge.circle_cy =
          drag_start_gauge.circle_cy + static_cast<int>(std::round(dy));
      gauge.circle_px =
          drag_start_gauge.circle_px + static_cast<int>(std::round(dx));
      gauge.circle_py =
          drag_start_gauge.circle_py + static_cast<int>(std::round(dy));
      break;
    }
    case GaugeHandleType::CircleRadius: {
      float dx = mouse_image_pos.x - (float)drag_start_gauge.circle_cx;
      float dy = mouse_image_pos.y - (float)drag_start_gauge.circle_cy;
      int r = std::max(
          1, static_cast<int>(std::round(std::sqrt(dx * dx + dy * dy))));

      if (imageW > 0 && imageH > 0)
        r = ClampCircleRadiusToImage(drag_start_gauge.circle_cx,
                                     drag_start_gauge.circle_cy, r, imageW,
                                     imageH);

      gauge.radius = r;
      gauge.circle_px = drag_start_gauge.circle_cx + r;
      gauge.circle_py = drag_start_gauge.circle_cy;

      break;
    }
    case GaugeHandleType::CircleInner: {
      float dx = mouse_image_pos.x - (float)drag_start_gauge.circle_cx;
      float dy = mouse_image_pos.y - (float)drag_start_gauge.circle_cy;
      int r = static_cast<int>(std::round(std::sqrt(dx * dx + dy * dy)));

      if (imageW > 0 && imageH > 0)
        r = ClampCircleRadiusToImage(drag_start_gauge.circle_cx,
                                     drag_start_gauge.circle_cy, r, imageW,
                                     imageH);

      const CircleGaugeGeometry startGeom =
          BuildCircleGaugeGeometry(drag_start_gauge);
      r = std::min(
          r,
          std::max(1, static_cast<int>(std::round(startGeom.outerRadius)) - 1));
      gauge.inner_radius = std::max(1, r);
      break;
    }
    case GaugeHandleType::CircleOuter: {
      float dx = mouse_image_pos.x - (float)drag_start_gauge.circle_cx;
      float dy = mouse_image_pos.y - (float)drag_start_gauge.circle_cy;
      int r = static_cast<int>(std::round(std::sqrt(dx * dx + dy * dy)));

      if (imageW > 0 && imageH > 0)
        r = ClampCircleRadiusToImage(drag_start_gauge.circle_cx,
                                     drag_start_gauge.circle_cy, r, imageW,
                                     imageH);

      const CircleGaugeGeometry startGeom =
          BuildCircleGaugeGeometry(drag_start_gauge);
      r = std::max(r, static_cast<int>(std::round(startGeom.innerRadius)) + 1);
      gauge.outer_radius = std::max(1, r);
      break;
    }
    default:
      break;
    }
  } else {
    if (!gauge.has_line_gauge)
      return;

    switch (handle) {
    case GaugeHandleType::LineP0:
      gauge.line_x0 = static_cast<int>(std::round(mouse_image_pos.x));
      gauge.line_y0 = static_cast<int>(std::round(mouse_image_pos.y));
      gauge.line_x1 = drag_start_gauge.line_x1;
      gauge.line_y1 = drag_start_gauge.line_y1;
      break;

    case GaugeHandleType::LineP1:
      gauge.line_x0 = drag_start_gauge.line_x0;
      gauge.line_y0 = drag_start_gauge.line_y0;
      gauge.line_x1 = static_cast<int>(std::round(mouse_image_pos.x));
      gauge.line_y1 = static_cast<int>(std::round(mouse_image_pos.y));
      break;

    case GaugeHandleType::LineCenter: {
      const float dx = mouse_image_pos.x - drag_start_mouse_image.x;
      const float dy = mouse_image_pos.y - drag_start_mouse_image.y;

      gauge.line_x0 =
          drag_start_gauge.line_x0 + static_cast<int>(std::round(dx));
      gauge.line_y0 =
          drag_start_gauge.line_y0 + static_cast<int>(std::round(dy));
      gauge.line_x1 =
          drag_start_gauge.line_x1 + static_cast<int>(std::round(dx));
      gauge.line_y1 =
          drag_start_gauge.line_y1 + static_cast<int>(std::round(dy));
      break;
    }

    case GaugeHandleType::LineWidthPlus:
    case GaugeHandleType::LineWidthMinus: {
      LineGaugeGeometry geom = BuildLineGaugeGeometry(drag_start_gauge);
      if (!geom.valid)
        return;

      const float fromCenterX = mouse_image_pos.x - geom.center.x;
      const float fromCenterY = mouse_image_pos.y - geom.center.y;
      const float signedDistance =
          fromCenterX * geom.normal.x + fromCenterY * geom.normal.y;
      const int newHalfWidth =
          std::max(1, static_cast<int>(std::round(std::abs(signedDistance))));
      gauge.tool_half_width = newHalfWidth;

      gauge.line_x0 = drag_start_gauge.line_x0;
      gauge.line_y0 = drag_start_gauge.line_y0;
      gauge.line_x1 = drag_start_gauge.line_x1;
      gauge.line_y1 = drag_start_gauge.line_y1;
      break;
    }

    default:
      break;
    }
  }

  gauge.dirty = true;
  gauge.accepted = false;
  gauge.review_status = "editing";
}

static constexpr const char *kCxImageCatalogPath =
    "cxparser/cxscript/module/cximage/catalog/cximage_catalog.cxsc";

#endif
