#pragma once

#include <string>
#include <vector>

struct CxParamRange
{
    std::string name;
    double min_value = 0.0;
    double max_value = 0.0;
    double step = 1.0;
    std::vector<double> discrete_values;
    bool enabled = true;
    bool integer_only = true;
    std::string role = "formal"; // formal / diagnostic / risky / locked
};

struct CxParamRangeSet
{
    std::string range_set_id;
    std::string tool;
    std::string source_profile_id;
    std::vector<CxParamRange> ranges;
    int max_candidates = 12;
    int max_case_seconds = 10;
    int max_total_seconds = 60;
};

struct CxParamRegressionTask
{
    std::string task_id;
    std::string case_id;
    std::string image_id;
    std::string target_id;
    std::string tool;
    std::string gauge_annotation_path;
    std::string base_script_id;
    std::string base_parameter_profile_id;
    int max_candidates = 12;
    int max_case_seconds = 10;
    int max_total_seconds = 60;
    bool require_manual_gauge = true;
    bool allow_mlpack_rank = true;
    bool allow_ensmallen_opt = true;
    bool allow_promote = false;
};

struct CxParamCandidate
{
    std::string candidate_id;
    std::string source = "manual_seed";
    int method = 0;
    int threshold = 20;
    int gap = 5;
    int linegap = 6;
    int min_edge_run_width_px = 3;
    int wgap = 8;
    int hgap = 32;
    int filterprofile = 1;
    int samplerate = 1;
    double min_score = 0.0;
    int find_num = 1;
    int compare_gap = 0;
    double predicted_quality = 0.0;
    double predicted_risk = 0.0;
    std::string predicted_failure_class = "unknown";
    bool selected_for_probe = false;
};

struct CxParamEvalRecord
{
    std::string candidate_id;
    std::string case_id;
    std::string tool;
    bool executed = false;
    bool timeout = false;
    int points = 0;
    bool fit_available = false;
    double support_score = 0.0;
    double mean_distance = 0.0;
    double fit_offset = 0.0;
    std::string failure_stage;
    std::string classification;
    std::string result_summary_path;
    std::string tool_display_path;
    std::string replay_package_path;
    std::string image_id;
    std::string target_id;
    std::string image_path;
    std::string script_path;
    std::string contract_path;
    int timeout_seconds = 0;
    int roi_x0 = 0;
    int roi_y0 = 0;
    int roi_x1 = 0;
    int roi_y1 = 0;
    int tool_half_width = 0;
    int max_elapsed_ms = 0;
    int max_scan_lines = 0;
    int max_samples = 0;
    int method = 0;
    int threshold = 0;
    int gap = 0;
    int linegap = 0;
    int min_edge_run_width_px = 0;
    int wgap = 0;
    int hgap = 0;
    int filterprofile = 0;
    int samplerate = 0;
    double min_score = 0.0;
    int find_num = 0;
    int compare_gap = 0;

};

struct CxHitDistributionBin
{
    double bin_start = 0.0;
    double bin_end = 0.0;
    int hit_count = 0;
    int accepted_count = 0;
    int outlier_count = 0;
    double mean_error = 0.0;
    double max_error = 0.0;
};

struct CxHitDistributionSummary
{
    std::string tool;
    std::string candidate_id;
    int total_points = 0;
    int valid_points = 0;
    int outlier_points = 0;
    double coverage_ratio = 0.0;
    double support_score = 0.0;
    double mean_distance = 0.0;
    double max_distance = 0.0;
    std::vector<CxHitDistributionBin> bins;
};

struct CxParamAccuracyStats
{
    std::string candidate_id;
    std::string tool;
    int total_cases = 0;
    int executed_cases = 0;
    int timeout_cases = 0;
    int geometry_pass = 0;
    int evidence_pass = 0;
    int human_accept = 0;
    double geometry_pass_rate = 0.0;
    double evidence_pass_rate = 0.0;
    double human_accept_rate = 0.0;
    double avg_support_score = 0.0;
    double avg_mean_distance = 0.0;
    double avg_fit_offset = 0.0;
    double stability_score = 0.0;
    double risk_score = 0.0;
};

struct CxParamOptimizationResult
{
    std::string task_id;
    std::vector<CxParamCandidate> candidates;
    std::vector<CxParamEvalRecord> records;
    std::string best_candidate_id;
    std::string recommended_action = "inspect_gauge";
    std::string report_path;
};

struct CxParamRegressionRuntime
{
    CxParamRegressionTask task;
    CxParamRangeSet range_set;
    std::vector<CxParamCandidate> candidates;
    std::vector<CxParamEvalRecord> records;
    CxParamOptimizationResult optimization;

    void Clear();
};

CxParamRangeSet MakeConservativeRangeSet(const std::string& tool);
std::vector<CxParamCandidate> GenerateBasicParamCandidates(
    const CxParamRangeSet& ranges,
    int max_candidates);

bool ExportParamRegressionReports(
    const std::string& out_dir,
    const CxParamRegressionTask& task,
    const CxParamRangeSet& ranges,
    const std::vector<CxParamCandidate>& candidates,
    const std::vector<CxParamEvalRecord>& records,
    const std::vector<CxParamAccuracyStats>& stats,
    std::string& reason);

bool LoadCxParamRegressionFile(
    const std::string& script_path,
    CxParamRegressionRuntime& out_runtime,
    std::string& out_reason);
