#pragma once

#include <string>
#include <vector>

struct CxEvidenceSelfTestRequest
{
    std::string run_id;
    std::string case_id;

    int group_index = -1;
    int thumb_index = -1;

    std::string script_id;
    std::string script_path;
    std::string image_id;
    std::string image_path;
    std::string target_id;
    std::string tool;
    std::string parameter_summary;
    std::string primary_object_type;
    std::string primary_object_name;
    std::string primary_object_status;
    int editable_object_count = 0;

    std::string out_dir;

    bool require_image = true;
    bool require_params = true;
    bool require_runtime_object = true;
    bool require_gauge_projection = true;
    bool require_result_projection = true;
};

struct CxEvidenceSelfTestStepResult
{
    std::string code;
    std::string status;
    std::string reason;

    std::string script_path;
    std::string image_path;
    std::string target_id;
    std::string parameter_summary;

    double elapsed_ms = 0.0;
};

struct CxEvidenceSelfTestResult
{
    std::string run_id;
    std::string case_id;

    bool executed = false;
    bool passed_to_human_review = false;

    std::string final_code;
    std::string final_status;
    std::string final_reason;

    std::string script_id;
    std::string script_path;

    std::string image_id;
    std::string image_path;

    std::string target_id;
    std::string tool;
    std::string parameter_summary;
    std::string primary_object_type;
    std::string primary_object_name;
    std::string primary_object_status;
    int editable_object_count = 0;

    int runtime_object_count = 0;
    int shape_element_count = 0;
    int gauge_shape_count = 0;
    int result_shape_count = 0;
    int shape_element_count_before = 0;
    int shape_element_count_after = 0;
    int projected_shape_count = 0;

    bool parser_binding_ok = false;
    bool runtime_executed = false;
    bool runtime_object_ok = false;
    bool gauge_projection_ok = false;
    bool shape_projection_ok = false;
    bool result_projection_ok = false;

    std::string runtime_object_type;
    std::string runtime_object_name;
    std::string runtime_status;
    std::string result_ref;
    std::string evidence_ref;
    std::string gauge_status;
    std::string projection_reason;

    std::string algorithm_status;
    std::string algorithm_reason;
    int measure_points_count = 0;
    int valid_points_count = 0;
    bool has_measure_points = false;
    bool has_fit_result = false;
    double avgdist = 0.0;
    double fit_cx = 0.0;
    double fit_cy = 0.0;
    double fit_radius = 0.0;
    double fit_radius_x = 0.0;
    double fit_radius_y = 0.0;
    double fit_angle_deg = 0.0;
    int fastmatch_model_point_count = 0;
    int fastmatch_learn_a_count = 0;
    int fastmatch_learn_b_count = 0;
    int fastmatch_learn_a2_count = 0;
    int fastmatch_learn_b2_count = 0;
    int fastmatch_learn_status_code = 0;
    int fastmatch_pattern_a_count = 0;
    int fastmatch_pattern_b_count = 0;
    int fastmatch_candidate_count = 0;
    double fastmatch_best_score = 0.0;

    std::string torch_result_ref;
    std::string torch_evidence_ref;
    std::string torch_primary_visual_ref;
    std::string torch_mask_ref;
    std::string torch_overlay_ref;
    std::string torch_actual_device;
    std::string torch_trainer_lifecycle_summary;
    std::string torch_unified_mainline_summary;
    int torch_ok = 0;
    int torch_error_code = 0;
    int torch_result_count = 0;
    int torch_mask_available = 0;
    double torch_infer_ms = 0.0;
    double torch_train_ms = 0.0;
    double torch_total_ms = 0.0;

    std::string segmentation_backend_status;
    std::string segmentation_result_ref;
    std::string segmentation_mask_ref;
    std::string segmentation_contour_ref;
    std::string segmentation_overlay_ref;
    int segmentation_contour_count = 0;
    double segmentation_primary_area = 0.0;

    std::vector<CxEvidenceSelfTestStepResult> steps;
};

void AddEvidenceSelfTestStep(
    CxEvidenceSelfTestResult& result,
    const std::string& code,
    const std::string& status,
    const std::string& reason);

bool WriteEvidenceSelfTestSummaryJson(
    const CxEvidenceSelfTestResult& result,
    const std::string& outPath,
    std::string& reason);

bool WriteEvidenceSelfTestReportMd(
    const CxEvidenceSelfTestResult& result,
    const std::string& outPath,
    std::string& reason);

struct CxEvidenceSelfTestBatchRequest
{
    std::string run_id;
    std::string out_dir;

    int max_cases = 0;

    bool include_pending_result = true;
    bool include_generic_scripts = false;
    std::string tool_filter;

    std::vector<CxEvidenceSelfTestRequest> cases;
};

struct CxEvidenceSelfTestBatchResult
{
    std::string run_id;
    std::string out_dir;

    int total_cases = 0;
    int executed_cases = 0;
    int pass_count = 0;
    int pending_count = 0;
    int fail_count = 0;

    std::string final_code;
    std::string final_status;
    std::string final_reason;

    std::vector<CxEvidenceSelfTestResult> case_results;
};

bool WriteEvidenceSelfTestBatchSummaryJson(
    const CxEvidenceSelfTestBatchResult& result,
    const std::string& outPath,
    std::string& reason);

bool WriteEvidenceSelfTestBatchReportMd(
    const CxEvidenceSelfTestBatchResult& result,
    const std::string& outPath,
    std::string& reason);

bool WriteTorchArtifactAuditReport(
    const CxEvidenceSelfTestBatchResult& result,
    const std::string& outDir,
    std::string& reason);
