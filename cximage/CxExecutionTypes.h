#pragma once

#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include <map>

struct CxPoint2D
{
    double x = 0.0;
    double y = 0.0;
};

struct CxLineResult
{
    double x0 = 0.0;
    double y0 = 0.0;
    double x1 = 0.0;
    double y1 = 0.0;
    double avgdist = 0.0;
};

struct CxCircleResult
{
    double cx = 0.0;
    double cy = 0.0;
    double radius = 0.0;
};

struct CxMatchResult
{
    std::string template_id;
    double search_roi_x = 0.0;
    double search_roi_y = 0.0;
    double search_roi_w = 0.0;
    double search_roi_h = 0.0;
    double score = 0.0;
    double angle = 0.0;
    double scale = 0.0;
    double matched_x = 0.0;
    double matched_y = 0.0;
    int model_point_count = 0;
    int candidate_count = 0;
    bool has_result_box = false;
};

enum class CxTorchTaskKind
{
    Unknown = 0,
    RuntimeDiagnostic,
    DeviceDiagnostic,
    WeightDiagnostic,
    RuntimeCapabilities,
    SegmentationContract,
    DetectionContract,
    Segmentation,
    Detection,
    Classification,
    FeatureExtraction,
    TemplateDifference,
    TrainingLifecycle,
    PrototypeLifecycle
};
struct CxTorchTaskSpec
{
    CxTorchTaskKind kind = CxTorchTaskKind::Unknown;
    std::string task_id;
    std::string case_id;
    std::string model_id;
    std::filesystem::path model_path;
    std::filesystem::path manifest_path;
    std::filesystem::path dataset_root;
    std::filesystem::path input_image_path;
    std::filesystem::path template_image_path;
    std::filesystem::path output_dir;
    std::string requested_device = "cpu";
    int timeout_ms = 0;
    std::string extra_json;
    std::filesystem::path precomputed_result_ref;
    std::filesystem::path precomputed_mask_path;
    std::filesystem::path precomputed_overlay_path;
    std::string precomputed_model_hash;
};





struct CxTorchDetection
{
    int source_index = -1;
    int class_id = -1;
    std::string class_name;
    double confidence = 0.0;
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;

    // A regular detection remains an axis-aligned proposal.  Only an
    // explicitly tagged oriented box may seed a FastMatchTransform.
    bool oriented_box_available = false;
    double oriented_center_x = 0.0;
    double oriented_center_y = 0.0;
    double oriented_width = 0.0;
    double oriented_height = 0.0;
    double oriented_angle_deg = 0.0;
    std::string oriented_angle_convention;
};

struct CxTorchMask
{
    bool available = false;
    int width = 0;
    int height = 0;
    double foreground_ratio = 0.0;
    std::string mask_ref;
    std::string contour_ref;
    std::string overlay_ref;
};

struct CxInferenceResult
{
    bool executed = false;
    bool ok = false;
    int error_code = 0;
    std::string schema;
    std::string schema_version;
    std::string task_id;
    std::string case_id;
    std::string model_id;
    std::string model_hash;
    std::string requested_device;
    std::string actual_device;
    std::string status;
    std::string failure_stage;
    std::string reason;
    double train_runtime_ms = 0.0;
    double infer_runtime_ms = 0.0;
    double algorithm_runtime_ms = 0.0;
    double total_runtime_ms = 0.0;
    std::vector<CxTorchDetection> detections;
    std::optional<CxTorchMask> mask;
    std::map<std::string, double> metrics;
    std::vector<std::string> artifact_refs;
    std::string evidence_ref;
    std::string result_ref;
    std::string primary_visual_ref;
    std::string bbox_candidate_list_ref;
    std::string roi_crop_packet_ref;
    std::string template_alignment_ref;
    std::string roi_diff_candidate_ref;
    std::string trainer_lifecycle_summary;
    std::string unified_mainline_summary;
    std::string raw_result_json;
};

bool ValidateCxTorchTaskSpec(const CxTorchTaskSpec& task, std::string& reason);

struct CxImageInput
{
    std::filesystem::path path;
    int width = 0;
    int height = 0;
    int channels = 0;
    std::string hash;
};

struct CxGaugeSnapshot
{
    std::string type;
    std::string revision;
    std::string source;
    std::string hash;

    double roi_x0 = 0.0;
    double roi_y0 = 0.0;
    double roi_x1 = 0.0;
    double roi_y1 = 0.0;
};

struct CxParameterSnapshot
{
    std::string profile_id;
    std::string revision;
};

struct CxExecutionBudget
{
    int max_elapsed_ms = 0;
    int max_scan_lines = 0;
    int max_samples = 0;
};

struct CxContractSpec
{
    std::filesystem::path contract_path;
    bool enabled = false;
};

struct CxMetricSummary
{
    int elapsed_ms = 0;
    int scan_line_count = 0;
    int sample_count = 0;
    int valid_points_count = 0;
    int rejected_points_count = 0;
};

struct CxExecutionTraceSummary
{
    std::string run_id;
    std::string case_id;
    std::string script_id;
};

struct CxExecutionRequest
{
    std::string run_id;
    std::string case_id;
    std::string image_id;
    std::string target_id;

    std::string tool;
    std::string script_path;

    CxImageInput image;
    CxGaugeSnapshot gauge;
    CxParameterSnapshot parameters;

    CxExecutionBudget budget;
    CxContractSpec contract;
};

struct CxExecutionResult
{
    std::string run_id;
    std::string case_id;
    std::string tool;
    std::string object_name;

    bool executed = false;
    bool geometry_available = false;
    bool fit_available = false;

    std::string status;
    std::string failure_stage;
    std::string reason;

    CxGaugeSnapshot input_gauge;
    CxParameterSnapshot input_parameters;

    std::vector<CxPoint2D> measure_points;
    std::vector<CxPoint2D> valid_points;
    std::vector<CxPoint2D> rejected_points;

    std::optional<CxLineResult> line_result;
    std::optional<CxCircleResult> circle_result;
    std::optional<CxMatchResult> match_result;
    std::optional<CxInferenceResult> inference_result;

    CxMetricSummary metrics;
    CxExecutionTraceSummary trace;
};

struct CxEvidencePackage
{
    std::filesystem::path snapshot_path;
    std::filesystem::path summary_path;
    std::filesystem::path result_overlay_path;
    std::filesystem::path evidence_overlay_path;
    std::filesystem::path tool_display_path;
    std::filesystem::path trace_path;
    std::filesystem::path replay_package_path;
    std::filesystem::path contract_result_path;
};

struct CxReviewDecision
{
    std::string run_id;
    std::string decision;
    std::string reason;
    std::string reviewer;
};

struct CxMaskFactsSnapshot
{
    bool evaluated = false;
    bool loadable = false;
    int width = 0;
    int height = 0;
    int foreground_pixels = 0;
    int component_count = 0;
    int boundary_pixels = 0;
    int bbox_x = 0;
    int bbox_y = 0;
    int bbox_width = 0;
    int bbox_height = 0;
    double foreground_ratio = 0.0;
    double bbox_fill_ratio = 0.0;
    bool empty = true;
    bool full_frame = false;
    bool touches_border = false;
    std::string status = "NOT_EVALUATED";
    std::string reason;
};

struct CxMaskComparisonSnapshot
{
    bool evaluated = false;
    bool dimensions_match = false;
    CxMaskFactsSnapshot left;
    CxMaskFactsSnapshot right;
    int intersection_pixels = 0;
    int union_pixels = 0;
    double iou = 0.0;
    double dice = 0.0;
    double boundary_precision = 0.0;
    double boundary_recall = 0.0;
    double boundary_fscore = 0.0;
    double foreground_ratio_delta = 0.0;
    std::string status = "NOT_EVALUATED";
    std::string reason;
};

bool AnalyzeCxMaskFile(
    const std::string& mask_path,
    CxMaskFactsSnapshot& snapshot,
    std::string& reason);

bool CompareCxMaskFiles(
    const std::string& left_mask_path,
    const std::string& right_mask_path,
    CxMaskComparisonSnapshot& snapshot,
    std::string& reason);

bool WriteCxMaskComparisonJson(
    const CxMaskComparisonSnapshot& snapshot,
    const std::string& output_path,
    std::string& reason);
