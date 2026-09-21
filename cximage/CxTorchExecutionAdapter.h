#pragma once

#include "CxImageReferenceCandidateGenerator.h"
#include "CxExecutionTypes.h"
#include "CxTorchRuntimeService.h"

struct CxPairedInferenceRequest
{
    CxTorchTaskSpec parent_task;
    CxTorchTaskSpec child_task;
    std::optional<CxImageReferenceCandidateRequest> cximage_candidate_request;
    std::filesystem::path dataset_label_mask_path;
    std::filesystem::path report_path;
    double material_iou_delta = 0.01;
    double stable_pair_iou = 0.98;
    double material_foreground_delta = 0.03;
    double minimum_boundary_fscore = 0.85;
};

struct CxPairedInferenceDiagnostic
{
    bool executed = false;
    bool complete = false;
    std::string status = "NOT_RUN";
    std::string reason;
    std::string observation;
    std::string failure_class;
    std::string affected_stage;
    double confidence = 0.0;
    std::string gate_recommendation = "insufficient_evidence";
    CxInferenceResult parent_result;
    CxInferenceResult child_result;
    std::optional<CxImageReferenceCandidateResult> cximage_candidate;
    CxMaskComparisonSnapshot parent_child;
    std::optional<CxMaskComparisonSnapshot> parent_label;
    std::optional<CxMaskComparisonSnapshot> child_label;
    std::optional<CxMaskComparisonSnapshot> parent_cximage;
    std::optional<CxMaskComparisonSnapshot> child_cximage;
    std::optional<CxMaskComparisonSnapshot> label_cximage;
    std::vector<std::string> artifact_refs;
    std::string report_ref;
};

class CxTorchExecutionAdapter
{
public:
    bool Execute(const CxTorchTaskSpec& task, CxInferenceResult& result, std::string& reason);
    bool ExecutePair(
        const CxPairedInferenceRequest& request,
        CxPairedInferenceDiagnostic& diagnostic,
        std::string& reason);

private:
    bool EnsureRuntime(const CxTorchTaskSpec& task, std::string& reason);
    bool BuildRuntimeRequest(const CxTorchTaskSpec& task, CxTorchTaskRequest& request, std::string& reason) const;

    CxTorchRuntimeService service_;
};
