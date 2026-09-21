#ifndef TORCH_FUSION_HEAD_H
#define TORCH_FUSION_HEAD_H


#include <torch/torch.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "torch_feature_head.h"

struct FusionHeadConfig {
    int64_t semantic_dim = 256;
    int64_t geometry_dim = 256;
    int64_t texture_dim = 128;
    int64_t shape_dim = 128;
    int64_t hidden_dim = 256;
    int64_t fused_dim = 256;
    int64_t num_classes = 10;
    float dropout = 0.1f;
};

struct FusionOutput {
    torch::Tensor fused_embedding;
    torch::Tensor class_logits;
};

class MultiFeatureFusionHeadImpl : public torch::nn::Module {
public:
    explicit MultiFeatureFusionHeadImpl(const FusionHeadConfig & cfg)
        : cfg_(cfg) {
        _validate_config();
        const int64_t total_dim = cfg_.semantic_dim + cfg_.geometry_dim + cfg_.texture_dim + cfg_.shape_dim;
        projector_ = register_module("projector", torch::nn::Sequential(
            torch::nn::Linear(total_dim, cfg_.hidden_dim),
            torch::nn::ReLU(),
            torch::nn::Dropout(cfg_.dropout),
            torch::nn::Linear(cfg_.hidden_dim, cfg_.fused_dim)
        ));
        classifier_ = register_module("classifier", torch::nn::Linear(cfg_.fused_dim, cfg_.num_classes));
    }

    FusionOutput forward(const MultiBranchEmbedding & embedding) {
        _validate_embedding(embedding);
        auto fused_input = torch::cat({
            embedding.semantic,
            embedding.geometry,
            embedding.texture,
            embedding.shape
        }, 1);

        FusionOutput out;
        out.fused_embedding = projector_->forward(fused_input);
        out.class_logits = classifier_->forward(out.fused_embedding);
        return out;
    }

    void reset_num_classes(int64_t num_classes) {
        TORCH_CHECK(num_classes > 0, "FusionHeadConfig.num_classes must be positive");
        cfg_.num_classes = num_classes;
        unregister_module("classifier");
        classifier_ = register_module("classifier", torch::nn::Linear(cfg_.fused_dim, cfg_.num_classes));
    }

    const FusionHeadConfig & config() const {
        return cfg_;
    }

private:
    void _validate_config() const {
        TORCH_CHECK(cfg_.semantic_dim > 0, "FusionHeadConfig.semantic_dim must be positive");
        TORCH_CHECK(cfg_.geometry_dim > 0, "FusionHeadConfig.geometry_dim must be positive");
        TORCH_CHECK(cfg_.texture_dim > 0, "FusionHeadConfig.texture_dim must be positive");
        TORCH_CHECK(cfg_.shape_dim > 0, "FusionHeadConfig.shape_dim must be positive");
        TORCH_CHECK(cfg_.hidden_dim > 0, "FusionHeadConfig.hidden_dim must be positive");
        TORCH_CHECK(cfg_.fused_dim > 0, "FusionHeadConfig.fused_dim must be positive");
        TORCH_CHECK(cfg_.num_classes > 0, "FusionHeadConfig.num_classes must be positive");
        TORCH_CHECK(cfg_.dropout >= 0.0f && cfg_.dropout <= 1.0f, "FusionHeadConfig.dropout must be in [0, 1]");
    }

    void _validate_branch_tensor(
        const torch::Tensor & tensor,
        int64_t expected_width,
        const char * name,
        int64_t expected_batch) const {

        TORCH_CHECK(tensor.defined(), "Fusion head branch ", name, " must be defined");
        TORCH_CHECK(tensor.dim() == 2, "Fusion head branch ", name, " must be rank-2, got ", tensor.sizes());
        TORCH_CHECK(
            tensor.size(1) == expected_width,
            "Fusion head branch ",
            name,
            " width mismatch. Expected ",
            expected_width,
            " got ",
            tensor.size(1));
        if (expected_batch >= 0) {
            TORCH_CHECK(
                tensor.size(0) == expected_batch,
                "Fusion head branch batch mismatch for ",
                name,
                ". Expected ",
                expected_batch,
                " got ",
                tensor.size(0));
        }
    }

    void _validate_embedding(const MultiBranchEmbedding & embedding) const {
        _validate_branch_tensor(embedding.semantic, cfg_.semantic_dim, "semantic", -1);
        const auto batch = embedding.semantic.size(0);
        _validate_branch_tensor(embedding.geometry, cfg_.geometry_dim, "geometry", batch);
        _validate_branch_tensor(embedding.texture, cfg_.texture_dim, "texture", batch);
        _validate_branch_tensor(embedding.shape, cfg_.shape_dim, "shape", batch);
    }

    FusionHeadConfig cfg_;
    torch::nn::Sequential projector_{nullptr};
    torch::nn::Linear classifier_{nullptr};
};
TORCH_MODULE(MultiFeatureFusionHead);

#endif
