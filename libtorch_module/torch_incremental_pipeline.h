#ifndef TORCH_INCREMENTAL_PIPELINE_H
#define TORCH_INCREMENTAL_PIPELINE_H


#include <functional>
#include <string>
#include <utility>
#include <vector>

#include <torch/torch.h>

#include "torch_feature_head.h"
#include "torch_fusion_head.h"
#include "torch_llama_bridge.h"
#include "torch_prototype_index.h"

struct RoiSample {
    std::string sample_id;
    std::string image_path;
    std::string class_name;
    std::string subtype_name;
    std::string modality_tag;
    torch::Tensor roi_tensor;
    torch::Tensor feature_map;
    torch::Tensor external_geometry_descriptor;
    torch::Tensor external_shape_descriptor;
};

struct PipelinePrediction {
    FusionOutput fusion;
    std::vector<PrototypeSearchResult> topk;
    std::string predicted_class;
    bool needs_llm_rerank = false;
};

class IncrementalFeaturePipeline {
public:
    IncrementalFeaturePipeline(
        MultiBranchFeatureHead feature_head,
        MultiFeatureFusionHead fusion_head)
        : feature_head_(std::move(feature_head)),
          fusion_head_(std::move(fusion_head)) {}

    PipelinePrediction infer(
        const RoiSample & sample,
        int topk = 5,
        PrototypeFusionWeights weights = {}) {

        _validate_sample(sample);
        TORCH_CHECK(topk > 0, "IncrementalFeaturePipeline.infer topk must be positive");

        auto embedding = feature_head_->forward(_make_feature_input(sample));

        auto fusion = fusion_head_->forward(embedding);

        PrototypeSearchQuery query{
            embedding.semantic,
            embedding.geometry,
            embedding.texture,
            embedding.shape
        };

        auto candidates = prototype_index_.search_topk(query, topk, weights);

        PipelinePrediction pred;
        pred.fusion = fusion;
        pred.topk = candidates;
        pred.predicted_class = _best_class_from_logits(fusion.class_logits);
        pred.needs_llm_rerank = candidates.empty() || _is_low_margin(fusion.class_logits);
        return pred;
    }

    PrototypeEntry make_prototype_entry(
        const RoiSample & sample,
        const MultiBranchEmbedding & embedding,
        const std::string & prototype_id,
        float confidence = 1.0f) const {

        PrototypeEntry entry;
        entry.prototype_id = prototype_id;
        entry.class_name = sample.class_name;
        entry.subtype_name = sample.subtype_name;
        entry.modality_tag = sample.modality_tag;
        entry.semantic_vec = embedding.semantic.detach().cpu();
        entry.geometry_vec = embedding.geometry.detach().cpu();
        entry.texture_vec = embedding.texture.detach().cpu();
        entry.shape_vec = embedding.shape.detach().cpu();
        entry.confidence = confidence;
        entry.sample_count = 1;
        return entry;
    }

    void incremental_update(
        const RoiSample & sample,
        const std::string & prototype_id,
        float confidence = 1.0f) {

        _validate_sample(sample);
        TORCH_CHECK(!prototype_id.empty(), "IncrementalFeaturePipeline.incremental_update prototype_id must not be empty");
        TORCH_CHECK(confidence >= 0.0f, "IncrementalFeaturePipeline.incremental_update confidence must be non-negative");

        auto embedding = feature_head_->forward(_make_feature_input(sample));
        prototype_index_.add_or_update(make_prototype_entry(sample, embedding, prototype_id, confidence));
    }

    std::string build_llama_prompt(
        const RoiSample & sample,
        const std::vector<PrototypeSearchResult> & candidates,
        const std::string & class_hint = "",
        const std::string & task_prompt = "") const {

        LlamaBridgeRequest req;
        req.image_path = sample.image_path;
        req.class_hint = class_hint;
        req.task_prompt = task_prompt;
        req.candidates = candidates;
        return LlamaBridge::build_prompt(req);
    }

    PrototypeIndex & prototype_index() {
        return prototype_index_;
    }

    const PrototypeIndex & prototype_index() const {
        return prototype_index_;
    }

private:
    static void _validate_sample(const RoiSample & sample) {
        TORCH_CHECK(sample.feature_map.defined(), "RoiSample.feature_map must be defined");
        TORCH_CHECK(sample.feature_map.dim() == 4, "RoiSample.feature_map must be rank-4 BCHW, got ", sample.feature_map.sizes());
        if (sample.external_geometry_descriptor.defined()) {
            TORCH_CHECK(
                sample.external_geometry_descriptor.dim() == 1 || sample.external_geometry_descriptor.dim() == 2,
                "RoiSample.external_geometry_descriptor must be rank-1 or rank-2, got ",
                sample.external_geometry_descriptor.sizes());
        }
        if (sample.external_shape_descriptor.defined()) {
            TORCH_CHECK(
                sample.external_shape_descriptor.dim() == 1 || sample.external_shape_descriptor.dim() == 2,
                "RoiSample.external_shape_descriptor must be rank-1 or rank-2, got ",
                sample.external_shape_descriptor.sizes());
        }
    }

    MultiBranchFeatureInput _make_feature_input(const RoiSample & sample) const {
        MultiBranchFeatureInput input;
        input.feature_map = sample.feature_map;
        input.external_geometry = sample.external_geometry_descriptor;
        input.external_shape = sample.external_shape_descriptor;
        return input;
    }

    static std::string _best_class_from_logits(const torch::Tensor & logits) {
        if (!logits.defined() || logits.numel() == 0) {
            return "";
        }
        auto idx = logits.argmax(1).item<int64_t>();
        return std::to_string(idx);
    }

    static bool _is_low_margin(const torch::Tensor & logits, float min_margin = 0.15f) {
        if (!logits.defined() || logits.size(1) < 2) {
            return false;
        }
        auto probs = torch::softmax(logits, 1).cpu();
        auto topk = std::get<0>(probs.topk(2, 1));
        const auto best = topk[0][0].item<float>();
        const auto second = topk[0][1].item<float>();
        return (best - second) < min_margin;
    }

    MultiBranchFeatureHead feature_head_{nullptr};
    MultiFeatureFusionHead fusion_head_{nullptr};
    PrototypeIndex prototype_index_;
};

#endif
