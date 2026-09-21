#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <vector>

// The business workflow runtime is deliberately value based.  It does not own
// parser, image, shape, model, or GUI objects and it never retains raw pointers
// supplied by a provider.

struct CxBusinessWorkflowImageRef
{
    std::string image_id;
    std::string image_revision;
    std::string source_type;
    std::string uri;
    std::string coordinate_space;
    std::string content_hash;
    int width = 0;
    int height = 0;
    int channels = 0;
};

struct CxBusinessWorkflowFrameRef
{
    std::string frame_id;
    std::string image_id;
    std::string source_type;
    std::string uri;
    std::string captured_at;
    long long sequence = 0;
};

struct CxBusinessWorkflowRoi
{
    std::string roi_id;
    std::string source_image_id;
    std::string image_revision;
    std::string recipe_revision;
    std::string coordinate_space;
    std::string geometry;
    std::string transform_ref;
};

struct CxBusinessWorkflowAnnotation
{
    std::string object_id;
    std::string object_type;
    std::string coordinate_space;
    std::string source_image_id;
    std::string image_revision;
    std::string roi_id;
    std::string recipe_revision;
    std::string geometry;
    std::string creation_method;
    std::string creator;
    double confidence = 0.0;
    std::string transform_ref;
    std::string status;
    std::string parent_ref;
    std::string evidence_ref;
};

struct CxBusinessWorkflowDetectionElement
{
    std::string element_id;
    std::string element_type;
    std::string name;
    std::string status;
    double score = 0.0;
    std::string source;
    std::string coordinate_space;
    std::string geometry;
    std::string evidence_ref;
};

struct CxBusinessWorkflowAssetPreflightRecord
{
    std::filesystem::path path;
    std::string kind;
    std::string status;
    std::string code;
    std::string reason;
    std::uintmax_t byte_size = 0;
    int width = 0;
    int height = 0;
    int channels = 0;
    std::string content_hash;
};

struct CxBusinessObservationRecord
{
    // These channels are intentionally independent.  A zero detection result
    // is not a display failure, and successful execution is not evaluation
    // acceptance.
    std::string execution_status;
    std::string detection_status;
    std::string display_status;
    std::string evaluation_status;
    std::string reason;
    std::vector<CxBusinessWorkflowDetectionElement> detection_elements;
};

struct CxBusinessRiskRecord
{
    std::string code;
    std::string severity;
    std::string disposition;
    std::string stage;
    std::string reason;
    bool blocking = false;
};

struct CxBusinessWorkflowHumanDecision
{
    std::string decision;
    std::string operator_id;
    std::string occurred_at;
    std::string input_version;
    std::string output_version;
    // Required for provider-backed human decisions. Contract fixtures may
    // leave this empty, but they never count as business execution.
    std::string event_id;
};

struct CxBusinessWorkflowProviderRequest
{
    std::string run_id;
    std::string internal_case_id;
    std::string case_id;
    std::string action;
    std::string provider;
    std::string input_ref;
    std::string requested_output_ref;
    std::string idempotency_key;
    std::string state_before;
    std::filesystem::path case_directory;
    std::filesystem::path manifest_path;
    std::map<std::string, std::string> metadata;
};

struct CxBusinessWorkflowProviderResult
{
    bool executed = false;
    std::string status;
    std::string code;
    std::string reason;
    std::string output_ref;
    CxBusinessObservationRecord observation;
    CxBusinessWorkflowHumanDecision human_decision;
    std::vector<CxBusinessRiskRecord> risks;
};

using CxBusinessWorkflowProviderExecutor = std::function<
    CxBusinessWorkflowProviderResult(const CxBusinessWorkflowProviderRequest&)>;

struct CxBusinessWorkflowStepResult
{
    std::size_t index = 0;
    std::string action;
    std::string provider;
    std::string input_ref;
    std::string output_ref;
    std::string idempotency_key;
    std::string declared_capability_status;
    std::string capability_status;
    std::string expected_status;
    std::string status;
    std::string code;
    std::string reason;
    std::string state_before;
    std::string state_after;
    std::string recorded_at;
    std::string verification_scope;
    std::string contract_validation_status;
    bool state_changed = false;
    bool deduplicated = false;
    bool expected_rejection = false;
    bool requires_provider = false;
    bool provider_executed = false;
    bool contract_only = false;
    std::string evidence_ref;
    CxBusinessObservationRecord observation;
    // Expected/schema-only elements remain separate from provider-observed
    // detections so fixtures can never masquerade as inference output.
    std::vector<CxBusinessWorkflowDetectionElement> contract_fixture_elements;
    CxBusinessWorkflowHumanDecision human_decision;
    std::vector<CxBusinessRiskRecord> risks;
};

struct CxBusinessWorkflowCaseResult
{
    std::string run_id;
    std::string started_at;
    std::string completed_at;
    std::string internal_case_id;
    std::string case_id;
    std::string display_name;
    std::string description;
    std::string identity_key;
    std::filesystem::path manifest_path;
    std::filesystem::path case_directory;
    std::filesystem::path normalized_path;
    std::filesystem::path output_directory;
    std::filesystem::path case_result_path;
    std::string expected_final_status;
    std::string final_status;
    std::string final_code;
    std::string final_reason;
    std::string final_state;
    std::string verification_scope;
    std::size_t contract_fixture_step_pass_count = 0;
    std::size_t provider_execution_step_pass_count = 0;
    std::size_t pending_step_count = 0;
    std::vector<std::filesystem::path> required_assets;
    std::vector<CxBusinessWorkflowAssetPreflightRecord> asset_preflight;
    std::vector<CxBusinessWorkflowImageRef> image_refs;
    std::vector<CxBusinessWorkflowFrameRef> frame_refs;
    std::vector<CxBusinessWorkflowRoi> rois;
    std::vector<CxBusinessWorkflowAnnotation> annotations;
    std::vector<CxBusinessWorkflowStepResult> steps;
    std::vector<CxBusinessRiskRecord> risks;
};

struct CxBusinessWorkflowScanRecord
{
    std::filesystem::path path;
    std::filesystem::path normalized_path;
    std::string internal_case_id;
    std::string case_id;
    std::string outcome;
    std::string code;
    std::string reason;
};

struct CxBusinessWorkflowBatchRequest
{
    std::string run_id;
    std::filesystem::path case_root;
    std::filesystem::path out_dir;
    std::size_t max_cases = 0;

    // A request-level capability value overrides the declaration in a case
    // manifest.  Supported values are AVAILABLE, PENDING, and NOT_AVAILABLE.
    std::map<std::string, std::string> provider_capability_status;

    // Steps with step_N_requires_provider=true are never simulated.  They are
    // PENDING when this callback is not bound.  Other steps exercise only the
    // local workflow contract and report NOT_EXECUTED plus a separate contract
    // validation status, never algorithm execution success.
    CxBusinessWorkflowProviderExecutor provider_executor;
};

struct CxBusinessWorkflowBatchResult
{
    std::string run_id;
    std::string started_at;
    std::string completed_at;
    std::filesystem::path case_root;
    std::filesystem::path out_dir;
    std::filesystem::path summary_path;
    std::filesystem::path report_md_path;
    std::filesystem::path report_html_path;
    std::filesystem::path audit_jsonl_path;
    std::filesystem::path scan_debug_path;
    std::filesystem::path risk_register_path;

    std::size_t discovered_count = 0;
    std::size_t accepted_count = 0;
    std::size_t rejected_count = 0;
    std::size_t skipped_count = 0;
    std::size_t processed_count = 0;
    std::size_t pass_count = 0;
    std::size_t contract_fixture_pass_count = 0;
    std::size_t provider_execution_step_pass_count = 0;
    std::size_t pending_count = 0;
    std::size_t fail_count = 0;

    std::string final_status;
    std::string final_code;
    std::string final_reason;
    std::vector<CxBusinessWorkflowScanRecord> scan_records;
    std::vector<CxBusinessWorkflowCaseResult> case_results;
    std::vector<CxBusinessRiskRecord> risks;
    std::vector<std::string> output_errors;
};

// Returns true when scanning/execution completed and every required evidence
// file was written.  Business acceptance is reported separately through
// result.final_status; therefore a completed batch containing rejected cases
// can return true with final_status == "FAIL".
bool RunCxBusinessWorkflowAcceptance(
    const CxBusinessWorkflowBatchRequest& request,
    CxBusinessWorkflowBatchResult& result,
    std::string& reason);
