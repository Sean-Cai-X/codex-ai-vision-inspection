#pragma once

#include "CxScriptSuiteRuntime.h"
#include "CxScriptCatalogRuntime.h"
#include "CxScriptImageManifestRuntime.h"
#include "CxScriptReviewGateRuntime.h"
#include <string>
#include <vector>
#include <utility>

struct CxScriptSuiteRunOptions
{
    bool enabled = false;

    std::string suite_path;
    std::string image_manifest_path;
    std::string catalog_path_override;
    std::string parameter_profile_path;
    std::string out_root_override;

    bool save_overlay = true;
    bool export_tool_display = true;
    bool export_best_examples = true;
    bool run_contract = true;
    bool export_evidence_summary = true;
    bool export_final_report = true;
    bool stop_after_headless = false;

    bool require_human_review = false;
    std::string review_stage;
    std::string review_decision;
    std::string resume_review_id;

    bool dry_run = false;
    bool preview_only = false;
    bool use_manual_gauge = false;
    bool probe_only = false;
    std::string gauge_annotation_path;
    std::string only_case_id;

    bool trace_run = false;
    bool dump_replay_package = true;
    bool dump_cxparser_ext_trace = true;

    std::string trace_dir;
    int heartbeat_ms = 1000;

    int case_timeout_sec = 120;
};

struct CxScriptSuiteCaseResult
{
    std::string case_id;
    std::string evidence_id;
    std::string script_id;
    std::string script_path;

    std::string image_id;
    std::string image_path;
    std::string level;
    std::string target_id;
    std::string tool;
    std::string parameter_profile_id;
    std::string contract_id;

    std::string expected_result;
    std::string expected_policy_guard;
    std::string actual_policy_guard;
    std::string contract_path;

    bool headless_ok = false;
    bool contract_pass = false;

    int points_count = 0;
    int valid_points_count = 0;
    bool has_fit_line = false;
    bool has_fit_circle = false;
    int runtime_valid_points_count = 0;
    int global_valid_points_count = 0;
    bool runtime_has_fit_line = false;
    bool global_has_fit_line = false;
    bool runtime_has_fit_circle = false;
    bool global_has_fit_circle = false;
    bool runtime_global_valid_points_count_mismatch = false;
    bool runtime_global_has_fit_line_mismatch = false;
    bool runtime_global_has_fit_circle_mismatch = false;
    bool runtime_global_result_mismatch = false;
    bool algorithm_executed = false;
    bool budget_exceeded = false;
    int rendered_measure_points_count = 0;
    int rendered_result_count = 0;
    int result_overlay_changed_pixels = 0;
    int scan_rows_examined = 0;
    int scan_rows_with_foreground = 0;
    int scan_runs_total = 0;
    int scan_runs_within_length_limit = 0;
    int scan_runs_over_length_limit = 0;
    int scan_runs_rejected_by_selection = 0;
    int scan_runs_rejected_near_endpoint = 0;
    int scan_points_emitted = 0;
    int findobject_strategy_id = 0;
    int findobject_component_count = 0;
    int findobject_component_accepted_count = 0;
    int findobject_component_rejected_count = 0;
    int findobject_foreground_before = 0;
    int findobject_foreground_after = 0;
    std::string findobject_algorithm_branch;
    int fit_filter_input_count = 0;
    int fit_filter_kept_count = 0;
    int fit_filter_rejected_count = 0;

    double local_support = 0.0;
    double local_mean_distance = 0.0;
    double fit_offset = 0.0;
    double boundary_subpixel_offset_mean = 0.0;
    double boundary_subpixel_offset_stddev = 0.0;
    double boundary_localization_sigma_mean_px = 0.0;
    double boundary_residual_rmse_px = 0.0;
    double boundary_residual_p95_px = 0.0;
    double boundary_residual_max_px = 0.0;
    double boundary_reliability_score = 0.0;


    std::string result_status;
    std::string failure_stage;
    std::string contract_status;
    std::string contract_conclusion;
    std::string conclusion;
    int torch_ok = 0;
    int torch_result_count = 0;
    std::string policy_guard;
    std::string gauge_source;
    std::string gauge_review_status;
    std::string gauge_annotation_path;

    double circle_radius = 0.0;
    double avgdist = 0.0;

    std::string case_dir;
    std::string snapshot_path;
    std::string summary_path;
    std::string result_overlay_path;
    std::string evidence_overlay_path;
    std::string tool_display_path;
    std::string roi_preview_path;
    std::string evidence_packet_path;
    std::string contract_result_path;

    bool stopped_for_review = false;
    std::string review_stage;

    int roi_x0 = 0;
    int roi_y0 = 0;
    int roi_x1 = 0;
    int roi_y1 = 0;

    int circle_cx = 0;
    int circle_cy = 0;
    int circle_px = 0;
    int circle_py = 0;

    int effective_tool_half_width = 0;
    int effective_wgap = 0;
    int effective_hgap = 0;
    int effective_gap = 0;
    int effective_linegap = 0;
    int effective_min_edge_run_width_px = 0;
    int effective_threshold = 0;
    int effective_filterprofile = 0;
    int effective_method = 0;

    double fit_line_x0 = 0.0;
    double fit_line_y0 = 0.0;
    double fit_line_x1 = 0.0;
    double fit_line_y1 = 0.0;

    double circle_center_x = 0.0;
    double circle_center_y = 0.0;

    std::vector<std::pair<double, double>> measure_points_xy;
};

struct CxScriptSuiteRunResult
{
    bool ok = false;
    std::string reason;

    int total_cases = 0;
    int executed_cases = 0;
    int contract_pass = 0;
    int contract_fail = 0;

    std::string report_root;

    std::vector<CxScriptSuiteCaseResult> case_results;
};

bool RunCxScriptSuite(
    const CxScriptSuiteRunOptions& options,
    CxScriptSuiteRunResult& result);

const CxScriptCatalogEntry* FindCatalogScriptById(
    const CxScriptCatalogRuntime& catalog,
    const std::string& script_id);
