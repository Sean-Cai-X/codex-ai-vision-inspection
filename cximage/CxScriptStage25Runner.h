#ifndef CXIMAGE_CXSCRIPT_STAGE25_RUNNER_H
#define CXIMAGE_CXSCRIPT_STAGE25_RUNNER_H

#include <string>
#include <vector>
#include <filesystem>
#include "CxScriptStage25Manifest.h"

struct Stage25CaseResult
{
    std::string case_id;
    std::string image_id;
    std::string level;
    std::string target_id;
    std::string tool;
    std::string profile_id;
    std::string evidence_profile;
    std::string orientation;

    std::string policy_classification;
    std::string quality_classification;

    bool skipped_by_preflight = false;
    std::string skip_reason;

    bool t0_pass = false;
    bool t1_pass = false;
    bool t2_pass = false;

    bool headless_ok = false;
    std::string run_state;
    std::string failure_stage;
    std::string failure_reason;

    int points_count = 0;
    int valid_points_count = 0;

    bool has_fit = false;
    bool has_fit_line = false;
    bool has_fit_circle = false;

    double line_avgdist = 0.0;
    double circle_avgdist = 0.0;

    double measured_local_support_score = 0.0;
    double measured_local_mean_distance_px = 0.0;
    double global_reference_mean_distance_px = 0.0;
    double fit_offset_error_px = 0.0;

    double circle_local_support_score = 0.0;
    double circle_local_mean_radial_distance_px = 0.0;
    double circle_global_reference_mean_distance_px = 0.0;
    double circle_center_error_px = 0.0;

    std::string measure_source;
    bool fallback_used = false;

    std::string component_shape;
    bool line_filter_min_exceeds_component_p90 = false;

    std::string parameter_policy_id;
    std::string parameter_role;
    bool is_product_default = false;
    bool is_stage25_default = false;

    std::string summary_path;
    std::string evidence_summary_path;
    std::string snapshot_path;
    std::string overlay_path;
    std::string evidence_overlay_path;
    std::string generated_script_path;

    bool generated_script_exists = false;
    bool summary_exists = false;
    bool evidence_summary_exists = false;
    bool snapshot_exists = false;
    bool overlay_exists = false;
    bool evidence_overlay_exists = false;

    bool l1_l3_coverage_ok = false;
    bool parameter_policy_valid = false;
    bool product_default_changed = false;
    bool original_measure_available = false;
    bool local_evidence_confirmed = false;
    bool component_warning = false;

    bool fastmatch_allowed = false;
    std::string fastmatch_status;
    std::string fastmatch_reason;
};

struct Stage25RunOptions
{
    std::filesystem::path manifest_path;
    std::filesystem::path out_root;
    bool stop_on_error = false;
    bool run_preflight = true;
    bool run_evidence = true;
};

struct Stage25ProfileAggregate
{
    std::string tool;
    std::string profile_id;
    std::string parameter_policy_id;
    std::string parameter_role;

    int total_cases = 0;
    int total_images = 0;
    int level_count = 0;
    int orientation_count = 0;

    int t1_pass = 0;
    int t2_pass = 0;
    int local_confirmed = 0;
    int component_warning = 0;

    double original_success_rate = 0.0;
    double local_confirmed_rate = 0.0;
    double component_warning_rate = 0.0;

    double mean_local_support = 0.0;
    double mean_local_distance = 0.0;
    double mean_fit_offset = 0.0;

    bool can_promote_to_product_default = false;
    std::string product_default_gate_reason;
};

struct Stage25RunResult
{
    bool ok = false;
    std::string reason;

    int total_cases = 0;
    int t0_pass = 0;
    int t1_pass = 0;
    int t2_pass = 0;

    std::filesystem::path batch_report_path;
    std::filesystem::path coverage_report_path;
    std::filesystem::path stability_report_path;
    std::filesystem::path policy_report_path;
    std::filesystem::path preflight_report_path;

    std::vector<Stage25CaseResult> case_results;
};

bool RunStage25ManifestFile(
    const Stage25RunOptions& options,
    Stage25RunResult& result);

#endif
