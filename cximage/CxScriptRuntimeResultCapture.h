#ifndef CXIMAGE_CXSCRIPT_RUNTIME_RESULT_CAPTURE_H
#define CXIMAGE_CXSCRIPT_RUNTIME_RESULT_CAPTURE_H

#include "CxScriptHeadlessRuntime.h"

namespace mu
{
    class CxParserRuntime;
}

class FindLine;
class FindCircle;
class FindEllipse;
class FindObject;
class FindRect;
class FindSegmentation;
class FastMatch;
class CxTextInspect;
class TorchTask;

struct CxScriptToolResultCapture
{
    std::string type;
    std::string name;
    std::string owner_ref;

    bool algorithm_executed = false;
    bool measure_completed = false;
    bool fit_completed = false;
    bool budget_exceeded = false;

    int elapsed_ms = 0;
    int scan_line_count = 0;
    int sample_count = 0;
    int tool_method = 0;
    int tool_threshold = 0;
    int tool_wgap = 0;
    int tool_hgap = 0;
    int tool_linegap = 0;
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
    int tool_min_edge_run_width_px = 0;

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

    int valid_points_count = 0;
    bool has_fit_line = false;
    bool has_fit_circle = false;
    bool has_fit_ellipse = false;
    bool has_result_rect = false;

    double avgdist = 0.0;

    double fit_line_x0 = 0.0;
    double fit_line_y0 = 0.0;
    double fit_line_x1 = 0.0;
    double fit_line_y1 = 0.0;

    double circle_cx = 0.0;
    double circle_cy = 0.0;
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

    int ellipse_scan_lines_outside_roi_count = 0;
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
    std::string torch_status;
    std::string torch_failure_stage;
    std::string torch_reason;
    int torch_result_count = 0;
    std::string torch_evidence_ref;
    std::string torch_primary_visual_ref;
    std::string torch_trainer_lifecycle_summary;
    std::string torch_unified_mainline_summary;

    std::string failure_stage;
    std::string reason;

    std::vector<CxShapeElementSnapshot> shapes;
    std::vector<CxFindLineScanDiagnosticSnapshot> findline_scan_diagnostics;
    std::vector<CxFindLineEdgeEvaluationSnapshot> findline_edge_evaluations;
};

bool CaptureRuntimeToolResults(
    mu::CxParserRuntime& runtime,
    CxScriptExecutionCapture& capture,
    std::string& reason);

bool CaptureFindLineResult(
    class FindLine& tool,
    const std::string& object_name,
    CxScriptToolResultCapture& output);

bool CaptureFindCircleResult(
    class FindCircle& tool,
    const std::string& object_name,
    CxScriptToolResultCapture& output);

bool CaptureFindEllipseResult(
    class FindEllipse& tool,
    const std::string& object_name,
    CxScriptToolResultCapture& output);

bool CaptureFindRectResult(
    class FindRect& tool,
    const std::string& object_name,
    CxScriptToolResultCapture& output);

bool CaptureFindObjectResult(
    class FindObject& tool,
    const std::string& object_name,
    CxScriptToolResultCapture& output);

bool CaptureFastMatchResult(
    class FastMatch& tool,
    const std::string& object_name,
    CxScriptToolResultCapture& output);

bool CaptureCxTextInspectResult(
    class CxTextInspect& tool,
    const std::string& object_name,
    CxScriptToolResultCapture& output);
bool CaptureFindSegmentationResult(
    class FindSegmentation& tool,
    const std::string& object_name,
    CxScriptToolResultCapture& output);

bool CaptureTorchTaskResult(
    class TorchTask& tool,
    const std::string& object_name,
    CxScriptToolResultCapture& output);

#endif
