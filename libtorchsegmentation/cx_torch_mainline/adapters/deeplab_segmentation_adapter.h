#pragma once

#include "../bridge/cx_torch_result_bridge.h"

#include <string>
#include <vector>

namespace cx_torch_mainline {

struct DeepLabSegmentationAdapterInput {
    std::string template_image_ref;
    std::string test_image_ref;
    std::string weight_ref;
    std::string requested_device;
    std::string case_name = "torch.deeplab.unified.infer";
    std::string image_id;
};

struct DeepLabSegmentationAdapterOutput {
    std::string template_alignment_ref;
    std::string roi_diff_candidate_ref;
    int roi_diff_candidate_count = 0;
    std::string primary_visual_ref;
    double verified_runtime_ms = -1.0;
    bool manifest_input_closed = false;
};

inline CxTorchUnifiedReviewDraft BuildDeepLabSegmentationReviewDraft(
    const DeepLabSegmentationAdapterInput& input,
    const DeepLabSegmentationAdapterOutput& output) {
    CxTorchUnifiedReviewDraft draft;
    draft.source_thread = "torch_main_model";
    draft.case_name = input.case_name;
    draft.image_id = input.image_id.empty() ? input.test_image_ref : input.image_id;
    draft.stage = "segmentation_template_diff";
    draft.input_image_ref = input.test_image_ref;
    draft.primary_visual_ref = output.primary_visual_ref;
    draft.status = output.roi_diff_candidate_count >= 0 ? "watch" : "missing";
    draft.visualization_refs = { output.primary_visual_ref };

    draft.elements.push_back(MakeElementRef(
        "template_alignment",
        "line_segment",
        output.template_alignment_ref,
        output.template_alignment_ref.empty() ? "missing" : "matched",
        "Template alignment is the first segmentation-to-cxparser bridge element."));
    draft.elements.push_back(MakeElementRef(
        "roi_diff_candidate",
        "closed_region",
        output.roi_diff_candidate_ref,
        output.roi_diff_candidate_ref.empty() ? "missing" : "watch",
        "ROI diff candidate carries segmentation mask/diff evidence."));

    draft.element_chains.push_back(MakeChainRef(
        "bbox",
        "missing",
        {},
        "DeepLab segmentation path does not produce bbox in this phase."));
    draft.element_chains.push_back(MakeChainRef(
        "roi_crop",
        "missing",
        {},
        "DeepLab segmentation path does not produce ROI crop in this phase."));
    draft.element_chains.push_back(MakeChainRef(
        "template_alignment",
        output.template_alignment_ref.empty() ? "missing" : "matched",
        { "template_alignment" },
        "Template/test alignment projected from segmentation output."));
    draft.element_chains.push_back(MakeChainRef(
        "roi_diff",
        output.roi_diff_candidate_ref.empty() ? "missing" : "watch",
        { "roi_diff_candidate" },
        "Diff region projected as cxparser-visible closed_region evidence."));

    draft.element_summary = "DeepLab adapter exposes template_alignment and roi_diff elements.";
    draft.element_chain_summary =
        "bbox=missing; roi_crop=missing; template_alignment=" +
        std::string(output.template_alignment_ref.empty() ? "missing" : "matched") +
        "; roi_diff=" +
        std::string(output.roi_diff_candidate_ref.empty() ? "missing" : "watch");
    return draft;
}

}  // namespace cx_torch_mainline
