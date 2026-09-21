#ifndef TORCH_FEATURE_HEAD_H
#define TORCH_FEATURE_HEAD_H


#include <torch/torch.h>

#include <string>
#include <unordered_map>
#include <vector>

enum class FeatureBranchType {
    Semantic,
    Geometry,
    Texture,
    Shape
};

inline std::string feature_branch_name(FeatureBranchType branch) {
    switch (branch) {
    case FeatureBranchType::Semantic: return "semantic";
    case FeatureBranchType::Geometry: return "geometry";
    case FeatureBranchType::Texture:  return "texture";
    case FeatureBranchType::Shape:    return "shape";
    default:                          return "unknown";
    }
}

struct FeatureHeadConfig {
    int64_t in_channels = 512;
    int64_t pooled_dim = 512;
    int64_t hidden_dim = 256;
    int64_t semantic_dim = 256;
    int64_t geometry_dim = 256;
    int64_t texture_dim = 128;
    int64_t shape_dim = 128;
    int64_t external_geometry_dim = 0;
    int64_t external_shape_dim = 0;
    bool use_external_geometry = false;
    bool use_external_shape = false;
    bool l2_normalize = true;
    float dropout = 0.0f;
};

struct MultiBranchFeatureInput {
    torch::Tensor feature_map;
    torch::Tensor external_geometry;
    torch::Tensor external_shape;
};

struct MultiBranchEmbedding {
    torch::Tensor semantic;
    torch::Tensor geometry;
    torch::Tensor texture;
    torch::Tensor shape;

    std::unordered_map<std::string, torch::Tensor> as_map() const {
        return {
            {"semantic", semantic},
            {"geometry", geometry},
            {"texture",  texture},
            {"shape",    shape},
        };
    }
};

class FeatureProjectionHeadImpl : public torch::nn::Module {
public:
    FeatureProjectionHeadImpl(int64_t in_dim, int64_t hidden_dim, int64_t out_dim, float dropout = 0.0f)
        : out_dim_(out_dim) {
        layers_ = register_module("layers", torch::nn::Sequential(
            torch::nn::Linear(in_dim, hidden_dim),
            torch::nn::ReLU(),
            torch::nn::Dropout(dropout),
            torch::nn::Linear(hidden_dim, out_dim)
        ));
    }

    torch::Tensor forward(torch::Tensor x) {
        return layers_->forward(x);
    }

    int64_t output_dim() const {
        return out_dim_;
    }

private:
    int64_t out_dim_ = 0;
    torch::nn::Sequential layers_{nullptr};
};
TORCH_MODULE(FeatureProjectionHead);

class MultiBranchFeatureHeadImpl : public torch::nn::Module {
public:
    explicit MultiBranchFeatureHeadImpl(const FeatureHeadConfig & cfg)
        : cfg_(cfg) {
        _validate_config();
        pool_ = register_module("pool", torch::nn::AdaptiveAvgPool2d(torch::nn::AdaptiveAvgPool2dOptions({1, 1})));
        semantic_head_ = register_module("semantic_head", FeatureProjectionHead(cfg_.pooled_dim, cfg_.hidden_dim, cfg_.semantic_dim, cfg_.dropout));
        geometry_head_ = register_module("geometry_head", FeatureProjectionHead(
            cfg_.pooled_dim + ((cfg_.use_external_geometry && cfg_.external_geometry_dim > 0) ? cfg_.external_geometry_dim : 0),
            cfg_.hidden_dim,
            cfg_.geometry_dim,
            cfg_.dropout));
        texture_head_  = register_module("texture_head",  FeatureProjectionHead(cfg_.pooled_dim, cfg_.hidden_dim, cfg_.texture_dim, cfg_.dropout));
        shape_head_    = register_module("shape_head",    FeatureProjectionHead(
            cfg_.pooled_dim + ((cfg_.use_external_shape && cfg_.external_shape_dim > 0) ? cfg_.external_shape_dim : 0),
            cfg_.hidden_dim,
            cfg_.shape_dim,
            cfg_.dropout));
    }

    MultiBranchEmbedding forward(torch::Tensor feature_map) {
        _validate_feature_map(feature_map);
        MultiBranchFeatureInput input;
        input.feature_map = feature_map;
        return forward(input);
    }

    MultiBranchEmbedding forward(const MultiBranchFeatureInput & input) {
        _validate_feature_map(input.feature_map);
        auto pooled = _pool_feature_map(input.feature_map);

        MultiBranchEmbedding output;
        output.semantic = _encode_semantic(pooled);
        output.geometry = _encode_geometry(pooled, input.external_geometry);
        output.texture  = _encode_texture(pooled);
        output.shape    = _encode_shape(pooled, input.external_shape);

        if (cfg_.l2_normalize) {
            output.semantic = torch::nn::functional::normalize(output.semantic, torch::nn::functional::NormalizeFuncOptions().p(2).dim(1));
            output.geometry = torch::nn::functional::normalize(output.geometry, torch::nn::functional::NormalizeFuncOptions().p(2).dim(1));
            output.texture  = torch::nn::functional::normalize(output.texture,  torch::nn::functional::NormalizeFuncOptions().p(2).dim(1));
            output.shape    = torch::nn::functional::normalize(output.shape,    torch::nn::functional::NormalizeFuncOptions().p(2).dim(1));
        }

        return output;
    }

    const FeatureHeadConfig & config() const {
        return cfg_;
    }

private:
    void _validate_config() const {
        TORCH_CHECK(cfg_.in_channels > 0, "FeatureHeadConfig.in_channels must be positive");
        TORCH_CHECK(cfg_.pooled_dim > 0, "FeatureHeadConfig.pooled_dim must be positive");
        TORCH_CHECK(cfg_.hidden_dim > 0, "FeatureHeadConfig.hidden_dim must be positive");
        TORCH_CHECK(cfg_.semantic_dim > 0, "FeatureHeadConfig.semantic_dim must be positive");
        TORCH_CHECK(cfg_.geometry_dim > 0, "FeatureHeadConfig.geometry_dim must be positive");
        TORCH_CHECK(cfg_.texture_dim > 0, "FeatureHeadConfig.texture_dim must be positive");
        TORCH_CHECK(cfg_.shape_dim > 0, "FeatureHeadConfig.shape_dim must be positive");
        TORCH_CHECK(cfg_.dropout >= 0.0f && cfg_.dropout <= 1.0f, "FeatureHeadConfig.dropout must be in [0, 1]");

        TORCH_CHECK(
            !cfg_.use_external_geometry || cfg_.external_geometry_dim > 0,
            "FeatureHeadConfig.external_geometry_dim must be positive when use_external_geometry is enabled");
        TORCH_CHECK(
            !cfg_.use_external_shape || cfg_.external_shape_dim > 0,
            "FeatureHeadConfig.external_shape_dim must be positive when use_external_shape is enabled");
    }

    void _validate_feature_map(const torch::Tensor & feature_map) const {
        TORCH_CHECK(feature_map.defined(), "FeatureHead input feature_map must be defined");
        TORCH_CHECK(feature_map.dim() == 4, "FeatureHead input feature_map must be rank-4 BCHW, got ", feature_map.sizes());
        TORCH_CHECK(feature_map.size(1) == cfg_.in_channels,
            "FeatureHead input channel mismatch. Expected ",
            cfg_.in_channels,
            " got ",
            feature_map.size(1));
    }

    torch::Tensor _pool_feature_map(const torch::Tensor & feature_map) {
        return pool_->forward(feature_map).flatten(1);
    }

    torch::Tensor _encode_semantic(const torch::Tensor & pooled) {
        return semantic_head_->forward(pooled);
    }

    torch::Tensor _encode_texture(const torch::Tensor & pooled) {
        return texture_head_->forward(pooled);
    }

    torch::Tensor _encode_geometry(const torch::Tensor & pooled, const torch::Tensor & external_geometry) {
        return geometry_head_->forward(_merge_external_branch_input(
            pooled,
            external_geometry,
            cfg_.use_external_geometry,
            cfg_.external_geometry_dim));
    }

    torch::Tensor _encode_shape(const torch::Tensor & pooled, const torch::Tensor & external_shape) {
        return shape_head_->forward(_merge_external_branch_input(
            pooled,
            external_shape,
            cfg_.use_external_shape,
            cfg_.external_shape_dim));
    }

    torch::Tensor _merge_external_branch_input(
        const torch::Tensor & pooled,
        const torch::Tensor & external,
        bool use_external,
        int64_t expected_dim) const {

        if (!use_external || expected_dim <= 0) {
            return pooled;
        }

        if (!external.defined() || external.numel() == 0) {
            auto zeros = torch::zeros({pooled.size(0), expected_dim}, pooled.options());
            return torch::cat({pooled, zeros}, 1);
        }

        auto normalized = external.dim() == 1 ? external.unsqueeze(0) : external;
        TORCH_CHECK(
            normalized.dim() == 2,
            "External branch descriptor must be rank-2 after normalization, got ",
            normalized.sizes());
        TORCH_CHECK(
            normalized.size(0) == pooled.size(0),
            "External branch batch size must match pooled feature batch size. Got external batch ",
            normalized.size(0),
            " and pooled batch ",
            pooled.size(0));
        TORCH_CHECK(
            normalized.size(1) == expected_dim,
            "External branch descriptor width mismatch. Expected ",
            expected_dim,
            " got ",
            normalized.size(1));

        return torch::cat({pooled, normalized.to(pooled.device()).to(pooled.dtype())}, 1);
    }

    FeatureHeadConfig cfg_;
    torch::nn::AdaptiveAvgPool2d pool_{nullptr};
    FeatureProjectionHead semantic_head_{nullptr};
    FeatureProjectionHead geometry_head_{nullptr};
    FeatureProjectionHead texture_head_{nullptr};
    FeatureProjectionHead shape_head_{nullptr};
};
TORCH_MODULE(MultiBranchFeatureHead);

#endif
