#ifndef CXIMAGE_CXSCRIPT_HEADLESS_RUNTIME_H
#define CXIMAGE_CXSCRIPT_HEADLESS_RUNTIME_H

#include <array>

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include "CxRuntimeProjectionTypes.h"
#include "CxFastMatchShapeModel.h"
#include "FormfitGauge.h"

struct CxFindObjectMeasurementEvidence
{
    int object_index = -1;
    int component_label = 0;
    std::uint64_t generation = 0;
    std::uint64_t mask_hash = 0;
    std::string object_ref;
    std::string bbox_semantics;
    std::string centroid_semantics;
    int bbox_x = 0;
    int bbox_y = 0;
    int bbox_w = 0;
    int bbox_h = 0;
    std::uint64_t pixel_count = 0;
    double pixel_size_x = 1.0;
    double pixel_size_y = 1.0;
    double centroid_x = 0.0;
    double centroid_y = 0.0;
    double projected_area = 0.0;
    double equivalent_side = 0.0;
    double equivalent_radius = 0.0;
    double intensity_min = 0.0;
    double intensity_max = 0.0;
    double intensity_mean = 0.0;
    double intensity_stddev = 0.0;
    double intensity_skewness = 0.0;
    bool intensity_valid = false;
    std::string background_method;
    bool background_valid = false;
    int background_sample_count = 0;
    double background_baseline_mean = 0.0;
    double residual_threshold = 0.0;
    std::string segmentation_domain;
    double gw_pixel_perimeter = 0.0;
    bool subpixel_valid = false;
    int subpixel_candidate_point_count = 0;
    int subpixel_accepted_point_count = 0;
    int subpixel_rejected_low_gradient_count = 0;
    int subpixel_rejected_out_of_bounds_count = 0;
    double subpixel_mean_gradient = 0.0;
    double subpixel_mean_fit_residual_px = 0.0;
    double subpixel_perimeter = 0.0;
    double subpixel_area = 0.0;
    int subpixel_boundary_point_count = 0;
    std::string subpixel_failure_reason;
    std::string active_geometry_basis;
    double gwyddion_perimeter = 0.0;
    double polygon_perimeter = 0.0;
    double polygon_area = 0.0;
    std::array<std::uint64_t, 16> pixel_configuration_counts = {};
    int outer_boundary_point_count = 0;
    int hole_count = 0;
    double convex_hull_area = 0.0;
    double circularity = 0.0;
    double solidity = 0.0;
    double major_axis_length = 0.0;
    double minor_axis_length = 0.0;
    double orientation_deg = 0.0;
    double aspect_ratio = 0.0;
    double eccentricity = 0.0;
    bool moment_ellipse_valid = false;
    double feret_max = 0.0;
    double feret_max_angle_deg = 0.0;
    double feret_min = 0.0;
    double feret_min_angle_deg = 0.0;
    double inscribed_center_x = 0.0;
    double inscribed_center_y = 0.0;
    double inscribed_radius = 0.0;
    bool inscribed_circle_valid = false;
    double enclosing_center_x = 0.0;
    double enclosing_center_y = 0.0;
    double enclosing_radius = 0.0;
    bool enclosing_circle_valid = false;
    std::string length_unit;
    std::string area_unit;
    std::string boundary_method;
    std::string topology_policy;
    std::string inscribed_circle_method;
    std::string status;
};


struct CxFastMatchTemplateGeometryEvidence
{
    bool available = false;
    int source_object_index = -1;
    std::string source_object_ref;
    double bbox_x = 0.0;
    double bbox_y = 0.0;
    double bbox_w = 0.0;
    double bbox_h = 0.0;
    double centroid_x = 0.0;
    double centroid_y = 0.0;
    double projected_area = 0.0;
    double major_axis_length = 0.0;
    double minor_axis_length = 0.0;
    double orientation_deg = 0.0;
    double aspect_ratio = 0.0;
    double solidity = 0.0;
    int boundary_point_count = 0;
    std::string status;
};

struct CxFastMatchPoseCandidateEvidence
{
    int candidate_index = -1;
    int observed_geometry_index = -1;
    std::string observed_geometry_ref;
    double bbox_x = 0.0;
    double bbox_y = 0.0;
    double bbox_w = 0.0;
    double bbox_h = 0.0;
    double center_x = 0.0;
    double center_y = 0.0;
    double angle_deg = 0.0;
    double scale_x = 1.0;
    double scale_y = 1.0;
    double appearance_score = 0.0;
    double geometry_score = -1.0;
    double combined_score = 0.0;
    int boundary_point_count = 0;
    std::string status;
};

struct CxFastMatchNormalTracePointEvidence
{
    double x = 0.0;
    double y = 0.0;
};

struct CxFastMatchNormalTraceEvidence
{
    bool executed = false;
    bool succeeded = false;
    std::string reason = "NOT_RUN";
    int candidate_count = 0;
    int deduplicated_count = 0;
    int trace_point_count = 0;
    int normal_pair_count = 0;
    int directional_side_count = 0;
    int trace_segment_count = 0;
    double closure_error_px = -1.0;
    double max_consecutive_gap_px = -1.0;
    double gradient_coverage = 0.0;
    double selected_anchor_coverage = 0.0;
    int generated_conclusion_count = 0;
    int normal_pair_findline_bound_count = 0;
    int normal_pair_binding_miss_count = 0;
    int normal_pair_corner_rejected_count = 0;
    int loop_erased_point_count = 0;
    std::array<int, 4> normal_pair_counts_by_direction{};
    double anchor_normal_band_px = 0.0;
    std::array<int, 4> anchor_near_candidate_counts{};
    std::array<int, 4> anchor_band_rejected_counts{};
    std::array<int, 4> anchor_slope_rejected_counts{};
    std::array<int, 4> anchor_normal_rejected_counts{};
    std::array<int, 4> anchor_prefilter_accepted_counts{};
    std::array<int, 4> domain_deduplicated_counts{};
    std::array<int, 4> compressed_keypoint_counts{};
    std::array<int, 4> ann_edge_counts{};
    std::array<int, 4> ann_component_counts{};
    std::array<int, 4> ann_selected_point_counts{};
    std::array<double, 4> ann_selected_coverage{};
    std::array<double, 4> domain_join_allowed_gap_px{};
    std::array<double, 4> domain_join_selected_gap_px{};
    std::array<int, 4> endpoint_spike_pruned_counts{};
    int endpoint_spike_ratio_percent = 0;
    int junction_tangent_window_points = 0;
    int junction_min_cross_angle_deg = 0;
    int junction_max_extrapolation_percent = 0;
    int junction_join_spacing_multiplier_percent = 0;
    std::array<int, 4> derived_junction_modes{};
    std::array<double, 4> derived_junction_cross_angle_deg{};
    std::array<double, 4> derived_junction_current_extrapolation_px{};
    std::array<double, 4> derived_junction_next_extrapolation_px{};
    // Per-domain point snapshots make ANN re-clustering and Dijkstra source
    // selection directly reviewable when a later junction stage rejects the
    // cycle. These are value snapshots; they never feed the algorithm back.
    std::array<std::vector<CxFastMatchNormalTracePointEvidence>, 4>
        anchor_points_by_direction;
    std::array<std::vector<CxFastMatchNormalTracePointEvidence>, 4>
        domain_points_by_direction;
    std::array<std::vector<CxFastMatchNormalTracePointEvidence>, 4>
        compressed_points_by_direction;
    std::array<std::vector<CxFastMatchNormalTracePointEvidence>, 4>
        ann_selected_points_by_direction;
    std::vector<CxFastMatchNormalTracePointEvidence> derived_junction_points;
    std::vector<CxFastMatchNormalTracePointEvidence> dijkstra_trace_points;
    std::vector<int> dijkstra_source_directions;
    std::vector<int> dijkstra_source_scans;
    std::vector<CxFastMatchNormalTracePointEvidence> derived_trace_points;
    std::vector<CxFastMatchNormalTracePointEvidence> normal_pair_a;
    std::vector<CxFastMatchNormalTracePointEvidence> normal_pair_b;
    std::vector<CxFastMatchNormalTracePointEvidence> normal_pair_source_points;
    std::vector<int> normal_pair_source_directions;
    std::vector<int> normal_pair_source_scans;
};

struct CxOcrGlyphCandidateEvidence
{
    int source_object_index = -1;
    int line_index = -1;
    int reading_order = -1;
    double bbox_x = 0.0;
    double bbox_y = 0.0;
    double bbox_w = 0.0;
    double bbox_h = 0.0;
    double centroid_x = 0.0;
    double centroid_y = 0.0;
    double orientation_deg = 0.0;
    double projected_area = 0.0;
    double aspect_ratio = 0.0;
    double solidity = 0.0;
    std::string label;
    double appearance_score = 0.0;
    double geometry_score = -1.0;
    double confidence = 0.0;
    std::string source_object_ref;
    std::string status;
};

struct CxFindLineScanDiagnosticSnapshot
{
    int scan_index = -1;
    int scan_type = 0;
    double x0 = 0.0;
    double y0 = 0.0;
    double x1 = 0.0;
    double y1 = 0.0;
    int candidate_count = 0;
    int binary_logical_row = -1;
    int binary_sample_row = -1;
    int binary_logical_foreground_count = 0;
    int binary_logical_plus1_foreground_count = 0;
    int binary_logical_plus2_foreground_count = 0;
    int binary_sample_foreground_count = 0;
    int threshold_run_count = 0;
    int threshold_max_run_width = 0;
    int min_edge_run_width_px = 1;
    int rejected_min_edge_run_width = 0;
    bool boundary_response_used = false;
    int boundary_response_mode = 0;
    int boundary_response_candidate_count = 0;
    int boundary_response_selected_index = -1;
    double boundary_response_selected_position = -1.0;
    double boundary_response_selected_score = 0.0;
    std::string boundary_response_status;
    std::string boundary_response_reason;
    int threshold_first_run_start = -1;
    int threshold_first_run_end = -1;
    int threshold_last_run_start = -1;
    int threshold_last_run_end = -1;
    bool accepted = false;
    double accepted_x = 0.0;
    double accepted_y = 0.0;
    std::string reject_reason;
};
struct CxFindLineBoundaryPointEvidenceSnapshot
{

    int scan_index = -1;
    int scan_type = 0;
    double measured_x = 0.0;
    double measured_y = 0.0;
    double refined_x = 0.0;
    double refined_y = 0.0;
    double subpixel_offset = 0.0;
    double response_strength = 0.0;
    double local_noise = 0.0;
    double localization_sigma_px = 0.0;
    double fit_residual_px = 0.0;
    int polarity = 0;
    bool interpolation_valid = false;
    bool fit_residual_valid = false;
    std::array<double, 5> profile = { 0.0, 0.0, 0.0, 0.0, 0.0 };
};

struct CxFindLineEdgeEvaluationSnapshot
{
    int edge_index = 0;
    int candidate_scan_rows = 0;
    int accepted_points = 0;
    int rejected_by_selection = 0;
    int rejected_near_endpoint = 0;
    int over_length_runs = 0;
    double coverage = 0.0;
    double score = 0.0;
    bool selected = false;
    bool fit_possible = false;
};

// Value-only transform-search receipt.  This remains independent of the
// FastMatch runtime object so evidence can outlive the parser session.
struct CxFastMatchTransformSearchEvidence
{
    bool executed = false;
    bool converged = false;
    bool budget_exceeded = false;
    bool fallback_to_rigid = false;
    bool calibration_applied = false;
    std::string calibration_snapshot_hash;
    std::string calibration_source_ref;
    std::string calibration_coordinate_frame_id;
    std::string calibration_xy_unit;
    double calibration_reprojection_rmse_px = -1.0;
    double best_physical_cx = 0.0;
    double best_physical_cy = 0.0;
    bool seed_available = false;
    std::string seed_source;
    std::string seed_model_id;
    int seed_detection_index = -1;
    int seed_class_id = -1;
    double seed_confidence = 0.0;
    std::string seed_angle_convention;
    std::string seed_reason;
    double initial_cx = 0.0;
    double initial_cy = 0.0;
    double initial_angle_deg = 0.0;
    double initial_scale_x = 1.0;
    double initial_scale_y = 1.0;
    double best_cx = 0.0;
    double best_cy = 0.0;
    double best_angle_deg = 0.0;
    double best_scale_x = 1.0;
    double best_scale_y = 1.0;
    double best_shear = 0.0;
    double best_projective_u = 0.0;
    double best_projective_v = 0.0;
    double best_score = 0.0;
    double appearance_score = 0.0;
    double continuity_score = 0.0;
    double gradient_score = 0.0;
    double geometric_residual_px = 0.0;
    double rigid_baseline_score = 0.0;
    int evaluated_candidates = 0;
    int accepted_candidates = 0;
    int sample_count = 0;
    int elapsed_ms = 0;
    std::string failure_stage = "not_run";
};

struct CxScriptExecutionCapture
{
    bool script_compiled = false;
    bool runtime_completed = false;

    int elapsed_ms = 0;
    int budget_ms = 0;
    int max_steps = 0;
    int max_scan_lines = 0;
    int max_samples = 0;
    int scan_line_count = 0;
    int sample_count = 0;
    int tool_method = 0;
    int tool_threshold = 0;
    int tool_wgap = 0;
    int tool_hgap = 0;
    int tool_linegap = 0;
    int tool_min_edge_run_width_px = 0;
    double tool_input_line_x0 = 0.0;
    double tool_input_line_y0 = 0.0;
    double tool_input_line_x1 = 0.0;
    double tool_input_line_y1 = 0.0;
    double tool_input_line_half_width = 0.0;
    int tool_input_circle_cx = 0;
    int tool_input_circle_cy = 0;
    int tool_input_circle_px = 0;
    int tool_input_circle_py = 0;
    int tool_input_circle_gap = 0;

    int scan_rows_examined = 0;
    int scan_rows_with_foreground = 0;
    int scan_runs_total = 0;
    int scan_runs_within_length_limit = 0;
    int scan_runs_over_length_limit = 0;
    int scan_runs_rejected_by_min_edge_width = 0;
    int scan_runs_rejected_by_selection = 0;
    int scan_runs_rejected_near_endpoint = 0;
    int scan_points_emitted = 0;
    int boundary_trigger_enabled = 0;
    int boundary_trigger_mode = 0;
    int boundary_trigger_polarity = 2;
    int boundary_trigger_selection = 0;
    int boundary_trigger_scan_rows_evaluated = 0;
    int boundary_trigger_candidate_count = 0;
    int boundary_trigger_points_emitted = 0;
    int boundary_trigger_rejected_no_candidate = 0;
    int boundary_trigger_rejected_endpoint = 0;
    int findline_point_consistency_enabled = 0;
    double findline_point_consistency_range = 0.0;
    int findline_point_consistency_input_points = 0;
    int findline_point_consistency_output_points = 0;
    int findline_point_consistency_removed_points = 0;
    int findline_selected_edge_index = 0;
    int findline_evaluated_edge_count = 0;
    int findline_best_edge_index = 0;
    double findline_best_edge_score = 0.0;

    std::string boundary_analysis_status;
    std::string boundary_reliability_level;
    int boundary_expected_scan_count = 0;
    int boundary_accepted_point_count = 0;
    int boundary_interpolation_valid_count = 0;
    int boundary_fit_residual_count = 0;
    double boundary_coverage_ratio = 0.0;
    double boundary_response_mean = 0.0;
    double boundary_response_median = 0.0;
    double boundary_response_cv = 0.0;
    double boundary_subpixel_offset_mean = 0.0;
    double boundary_subpixel_offset_stddev = 0.0;
    double boundary_localization_sigma_mean_px = 0.0;
    double boundary_residual_rmse_px = 0.0;
    double boundary_residual_p95_px = 0.0;
    double boundary_residual_max_px = 0.0;
    double boundary_outlier_ratio = 0.0;
    double boundary_reliability_score = 0.0;
    std::vector<CxFindLineBoundaryPointEvidenceSnapshot> boundary_points;

    int circle_point_consistency_enabled = 0;
    double circle_point_consistency_range = 0.0;
    int circle_point_consistency_input_points = 0;
    int circle_point_consistency_output_points = 0;
    int circle_point_consistency_removed_points = 0;

    int strategy_id = 0;
    int selected_method = 0;
    int selected_threshold = 0;
    int selected_wgap = 0;
    int selected_hgap = 0;
    int selected_linegap = 0;
    int selected_filterprofile = 0;
    int selected_min_edge_run_width_px = 0;

    int valid_points_count = 0;
    bool has_fit_line = false;
    bool has_fit_circle = false;
    bool has_fit_ellipse = false;
    bool has_result_rect = false;

    double circle_radius = 0.0;
    double ellipse_cx = 0.0;
    double ellipse_cy = 0.0;
    double ellipse_radius_x = 0.0;
    double ellipse_radius_y = 0.0;
    double ellipse_angle_deg = 0.0;
    int ellipse_selected_edge_index = 0;
    int ellipse_scan_candidate_lines = 0;
    int ellipse_scan_total_candidates = 0;
    int ellipse_scan_accepted_points_before_gate = 0;
    double ellipse_accepted_min_boundary_ratio = 0.0;
    double ellipse_accepted_max_boundary_ratio = 0.0;
    double ellipse_accepted_avg_boundary_ratio = 0.0;
    std::string ellipse_candidate_policy;

    int ellipse_scan_lines_cross_outside_ellipse_count = 0;
    double ellipse_scan_endpoint_norm_min = 0.0;
    double ellipse_scan_endpoint_norm_avg = 0.0;
    double ellipse_scan_endpoint_norm_max = 0.0;
    int ellipse_accepted_points_outside_ellipse_count = 0;
    double ellipse_accepted_point_norm_min = 0.0;
    double ellipse_accepted_point_norm_avg = 0.0;
    double ellipse_accepted_point_norm_max = 0.0;
    int ellipse_rejected_boundary_band_candidate_count = 0;
    double ellipse_rejected_boundary_band_norm_min = 0.0;
    double ellipse_rejected_boundary_band_norm_avg = 0.0;
    double ellipse_rejected_boundary_band_norm_max = 0.0;
    std::string ellipse_scan_geometry_policy;
    int ellipse_point_consistency_enabled = 0;
    double ellipse_point_consistency_range = 0.0;
    int ellipse_point_consistency_input_points = 0;
    int ellipse_point_consistency_output_points = 0;
    int ellipse_point_consistency_removed_points = 0;

    double avgdist = 0.0;

    int result_rect_count = 0;
    int top1_rect_x = 0;
    int top1_rect_y = 0;
    int top1_rect_w = 0;
    int top1_rect_h = 0;

    int model_point_count = 0;
    int fastmatch_learn_a_count = 0;
    int fastmatch_learn_b_count = 0;
    int fastmatch_learn_a2_count = 0;
    int fastmatch_learn_b2_count = 0;
    int fastmatch_learn_status_code = 0;
    int fastmatch_model_width = 0;
    int fastmatch_model_height = 0;
    int fastmatch_pattern_a_count = 0;
    int fastmatch_pattern_b_count = 0;
    double fastmatch_pattern_a_x = 0.0;
    double fastmatch_pattern_a_y = 0.0;
    double fastmatch_pattern_a_width = 0.0;
    double fastmatch_pattern_a_height = 0.0;
    double fastmatch_pattern_b_x = 0.0;
    double fastmatch_pattern_b_y = 0.0;
    double fastmatch_pattern_b_width = 0.0;
    double fastmatch_pattern_b_height = 0.0;
    int candidate_count = 0;
    double best_score = 0.0;
    bool has_result_box = false;
    bool has_best_result = false;
    int fastmatch_match_call_count = 0;
    int fastmatch_match_ab_call_count = 0;
    int fastmatch_match_sample_ab_call_count = 0;
    int fastmatch_match_last_stage = 0;
    int fastmatch_match_image_width = 0;
    int fastmatch_match_image_height = 0;
    int fastmatch_learn_rect_x0 = 0;
    int fastmatch_learn_rect_y0 = 0;
    int fastmatch_learn_rect_x1 = 0;
    int fastmatch_learn_rect_y1 = 0;
    int fastmatch_match_rect_x0 = 0;
    int fastmatch_match_rect_y0 = 0;
    int fastmatch_match_rect_x1 = 0;
    int fastmatch_match_rect_y1 = 0;
    int fastmatch_raw_probe_count = 0;
    int fastmatch_raw_threshold_hit_count = 0;
    int fastmatch_result_to_list_count = 0;
    int fastmatch_candidate_insert_count = 0;
    int fastmatch_candidate_replace_count = 0;
    int fastmatch_candidate_reject_count = 0;
    bool fastmatch_rotate_budget_exceeded = false;
    int fastmatch_rotate_candidate_count = 0;
    int fastmatch_rotate_probe_count = 0;
    int fastmatch_rotate_sample_count = 0;
    int fastmatch_rotate_elapsed_ms = 0;

    CxFastMatchNormalTraceEvidence fastmatch_normal_trace;
    CxFastMatchShapeModel fastmatch_reference_shape_model;
    CxFastMatchShapeModel fastmatch_observed_shape_model;
    CxFastMatchFormFitResult fastmatch_form_fit;
    cxcore::formfit::FormfitGauge fastmatch_form_fit_gauge;

    CxFastMatchTemplateGeometryEvidence fastmatch_template_geometry;
    std::vector<CxFastMatchPoseCandidateEvidence> fastmatch_pose_candidates;
    CxFastMatchTransformSearchEvidence fastmatch_transform_search;
    std::string ocr_final_text;
    std::string ocr_failure_reason;
    std::vector<CxOcrGlyphCandidateEvidence> ocr_glyph_candidates;

    bool object_prefilter_requested = false;
    bool object_prefilter_applied = false;
    int actual_findsetting = 0;
    int object_filter_strategy_id = 0;
    int object_filter_borw = 0;
    int object_filter_min = 0;
    int object_filter_max = 0;
    int object_component_count = 0;
    int object_component_accepted_count = 0;
    int object_component_rejected_count = 0;
    int object_component_max_area = 0;
    int object_component_max_width = 0;
    int object_component_max_height = 0;
    int object_foreground_before = 0;
    int object_foreground_after = 0;
    int object_white_component_count = 0;
    int object_white_accepted_count = 0;
    int object_white_rejected_count = 0;
    int object_black_component_count = 0;
    int object_black_accepted_count = 0;
    int object_black_rejected_count = 0;
    std::string object_algorithm_branch;

    std::vector<CxFindObjectMeasurementEvidence> findobject_measurements;
    int fit_filter_input_count = 0;
    int fit_filter_kept_count = 0;
    int fit_filter_rejected_count = 0;
    double fit_filter_sigma = 0.0;
    double fit_filter_threshold = 0.0;
    bool findrect_seed_valid = false;
    bool findrect_top_valid = false;
    bool findrect_bottom_valid = false;
    bool findrect_left_valid = false;
    bool findrect_right_valid = false;
    int findrect_top_points = 0;
    int findrect_bottom_points = 0;
    int findrect_left_points = 0;
    int findrect_right_points = 0;
    double findrect_coarse_score = 0.0;
    double findrect_refine_score = 0.0;

    int segmentation_status_code = 0;
    int segmentation_contour_count = 0;
    double segmentation_primary_area = 0.0;
    std::string segmentation_result_ref;
    std::string segmentation_mask_ref;
    std::string segmentation_contour_ref;
    std::string segmentation_overlay_ref;
    std::string segmentation_task_id;

    std::string segmentation_model_id;

    std::string segmentation_model_package_ref;

    std::string segmentation_manifest_path;

    std::string segmentation_postprocess_profile;

    std::string segmentation_parameter_profile_ref;

    int segmentation_region_count = 0;

    bool segmentation_raw_result_available = false;

    bool segmentation_refined_result_available = false;

    bool segmentation_fallback_used = false;

    std::string segmentation_result_stage;

    std::string segmentation_refinement_method;
    std::string segmentation_requested_geometry_type;
    std::string segmentation_geometry_fit_status;
    std::string segmentation_geometry_fit_reason;
    int segmentation_geometry_count = 0;
    double segmentation_geometry_residual_px = 0.0;
    double segmentation_geometry_support = 0.0;
    double segmentation_geometry_center_x = 0.0;
    double segmentation_geometry_center_y = 0.0;
    double segmentation_geometry_radius = 0.0;
    double segmentation_geometry_axis_x = 0.0;
    double segmentation_geometry_axis_y = 0.0;
    double segmentation_geometry_angle_deg = 0.0;
    std::string segmentation_oriented_box_evidence_ref;
    std::string segmentation_oriented_box_conclusion;


    std::string segmentation_raw_result_ref;

    std::string segmentation_raw_mask_ref;

    std::string segmentation_raw_contour_ref;

    std::string segmentation_raw_overlay_ref;

    std::string segmentation_refined_result_ref;

    std::string segmentation_refined_mask_ref;

    std::string segmentation_refined_contour_ref;

    std::string segmentation_refined_overlay_ref;



    int torch_ok = 0;
    int torch_error_code = 0;
    double torch_train_ms = 0.0;
    double torch_infer_ms = 0.0;
    double torch_total_ms = 0.0;
    int torch_result_count = 0;
    std::string torch_status;
    std::string torch_failure_stage;
    std::string torch_reason;
    std::string torch_evidence_ref;
    std::string torch_primary_visual_ref;
    std::string torch_trainer_lifecycle_summary;
    std::string torch_unified_mainline_summary;

    bool budget_exceeded = false;

    int rendered_roi_count = 0;
    int rendered_scan_count = 0;
    int rendered_measure_points_count = 0;
    int rendered_result_count = 0;

    int result_overlay_changed_pixels = 0;

    std::string failure_stage;
    std::string reason;
    bool contract_context = false;
    bool contract_pass = false;
    std::string contract_status;
    std::string contract_conclusion;
    std::map<std::string, double> runtime_globals;

    std::vector<CxShapeElementSnapshot> shapes;
    std::vector<CxFindLineScanDiagnosticSnapshot> findline_scan_diagnostics;
    std::vector<CxFindLineEdgeEvaluationSnapshot> findline_edge_evaluations;

    bool smoke_pass = false;
    std::string smoke_findline_object_name;
    bool smoke_findline_roi = false;
    bool smoke_findline_scan = false;
    std::string smoke_findcircle_object_name;
    std::string smoke_findcircle_roi_shape_kind;
    double smoke_findcircle_roi_radius = 0.0;
    double smoke_findcircle_outer_scan_radius = 0.0;
};

struct CxScriptResultPackage
{
    std::map<std::string, double> runtime_globals_before;
    std::map<std::string, double> runtime_globals_after;

    std::vector<CxShapeElementSnapshot> shapes;

    std::map<std::string, double> metrics;
    std::map<std::string, std::string> facts;

    std::string tool;
    std::string object_name;
    std::string status;
    std::string failure_stage;
    std::string reason;
};

struct CxScriptHeadlessOptions
{
    bool enabled = false;

    std::string case_id;
    std::string script_path;
    std::string image_path;
    std::string template_image_path;
    std::string torch_dataset_root;
    int torch_training_epochs = 3;
    double torch_learning_rate = 1.0e-4;
    std::string torch_lr_schedule = "constant";
    double torch_min_learning_rate = 1.0e-6;
    double torch_weight_decay = 0.0;
    double torch_box_loss_weight = 1.0;
    double torch_class_loss_weight = 1.0;
    double torch_dfl_loss_weight = 1.0;
    double torch_mask_loss_weight = 1.0;
    bool torch_use_assignment_quality_targets = true;
    int torch_assignment_quality_warmup_epochs = 3;
    // Inference-set contract controls.  These defaults are visible values,
    // not executor constants; callers may override every field.
    double torch_postprocess_confidence_threshold = 0.25;
    double torch_postprocess_iou_threshold = 0.45;
    int torch_postprocess_max_detections = 20;
    bool torch_postprocess_class_agnostic_nms = true;
    std::string output_dir;

    std::string globals_path;
    std::string manifest_path;
    std::string image_id;
    std::string target_id;

    int timeout_sec = 30;
    int max_steps = 10000;

    bool contract_context_enabled = false;
    bool runtime_capture_smoke = false;

    std::map<std::string, double> cli_global_overrides;

    int roi_x0 = 0;
    int roi_y0 = 0;
    int roi_x1 = 0;
    int roi_y1 = 0;

    int circle_cx = 0;
    int circle_cy = 0;
    int circle_px = 0;
    int circle_py = 0;
    int findcircle_arc_enabled = 0;
    int findcircle_arc_start_deg = 0;
    int findcircle_arc_end_deg = 360;

    int ellipse_x0 = 0;
    int ellipse_y0 = 0;
    int ellipse_x1 = 0;
    int ellipse_y1 = 0;
    int ellipse_inner_scale_percent = 0;

    int tool_half_width = 20;
    int wgap = 32;
    int hgap = 8;
    int gap = 5;
    int linegap = 6;
    int min_edge_run_width_px = 3;
    int threshold = 20;
    int method = 0;
    int filterprofile = 0;
    int samplerate = 1;
    double min_score = 0.0;
    int find_num = 1;
    int compare_gap = 0;
    int strategy_id = 0;
    int algorithm_executed = 0;

    int learn_roi_x = 120;
    int learn_roi_y = 120;
    int learn_roi_w = 120;
    int learn_roi_h = 90;
    int search_roi_x = 0;
    int search_roi_y = 0;
    int search_roi_w = 640;
    int search_roi_h = 480;
    int expected_rect_x = 0;
    int expected_rect_y = 0;
    int expected_rect_w = 0;
    int expected_rect_h = 0;

    bool enable_evidence_analysis = true;
    int max_elapsed_ms = 5000;
    int max_scan_lines = 4096;
    int max_samples = 200000;

    int contract_headless_ok = 0;
    int contract_pass_initial = 0;
    int contract_algorithm_executed = 0;
    int contract_budget_exceeded = 0;
    int contract_rendered_measure_points_count = 0;
    int contract_rendered_result_count = 0;
    int contract_result_overlay_changed_pixels = 0;
    int contract_torch_ok = 0;
    int contract_torch_result_count = 0;
    int points_count = 0;
    int valid_points_count = 0;
    int has_fit_line = 0;
    int has_fit_circle = 0;
    int runtime_valid_points_count = 0;
    int global_valid_points_count = 0;
    int runtime_has_fit_line = 0;
    int global_has_fit_line = 0;
    int runtime_has_fit_circle = 0;
    int global_has_fit_circle = 0;
    int runtime_global_valid_points_count_mismatch = 0;
    int runtime_global_has_fit_line_mismatch = 0;
    int runtime_global_has_fit_circle_mismatch = 0;
    int runtime_global_result_mismatch = 0;
    double local_support = 0.0;
    double local_mean_distance = 0.0;
    double fit_offset = 0.0;
    double circle_radius = 0.0;
    double avgdist = 0.0;
    std::string policy_guard;
    int policy_guard_match = 0;
    std::string result_status;
    std::string failure_stage;
    std::string result_overlay_path;
    std::string evidence_overlay_path;
    std::string tool_display_path;

    double double_method = 0.0;
    double double_threshold = 0.0;
    double double_gap = 0.0;
    double double_linegap = 0.0;
    double double_wgap = 0.0;
    double double_hgap = 0.0;
    double double_strategy_id = 0.0;

    std::string case_name;
    std::string snapshot_path;
    std::string overlay_path;
    std::string summary_path;
    std::string runtime_log_path;
    bool save_overlay = true;

    std::string stage25_image_id;
    std::string stage25_level;
    std::string stage25_target_id;
    std::string stage25_tool;

    bool save_evidence_candidate = false;
    std::string evidence_candidate_root;
    std::string evidence_candidate_id;
};

struct CxScriptHeadlessResult
{
    bool launched = false;
    bool executed = false;
    bool runtime_ok = false;
    bool assets_complete = false;

    int exit_code = 0;

    std::string failure_stage;
    std::string reason;

    std::string output_dir;
    std::string snapshot_path;
    std::string summary_path;
    std::string result_overlay_path;
    std::string evidence_overlay_path;
    std::string tool_display_path;
    std::string runtime_log_path;

    bool ok = false;
    bool timed_out = false;
    bool support_available = false;

    std::string overlay_path;

    std::string run_state;
    std::string debug_status;
    std::string debug_reason;

    std::string current_result_name;
    std::string current_result_status;
    std::string current_result_reason;

    int points_count = 0;
    int valid_points_count = 0;
    bool has_fit_line = false;
    bool has_fit_circle = false;
    bool has_fit_ellipse = false;
    bool has_result_rect = false;
    int model_point_count = 0;
    int fastmatch_learn_a_count = 0;
    int fastmatch_learn_b_count = 0;
    int fastmatch_learn_a2_count = 0;
    int fastmatch_learn_b2_count = 0;
    int fastmatch_learn_status_code = 0;
    int candidate_count = 0;
    double best_score = 0.0;
    bool has_result_box = false;
    bool has_best_result = false;

    double local_support = 0.0;
    double local_mean_distance = 0.0;
    double fit_offset = 0.0;
    double circle_radius = 0.0;
    double avgdist = 0.0;
};

bool RunCxScriptHeadless(
    const CxScriptHeadlessOptions& options,
    CxScriptHeadlessResult& result);

#endif
