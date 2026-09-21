#ifndef TORCH_GEOMETRY_ROI_HEAD_H
#define TORCH_GEOMETRY_ROI_HEAD_H

#include <torch/torch.h>

#include <algorithm>
#include <cstdint>
#include <unordered_map>

// Geometry is intentionally a separate ROI branch.  Detection boxes are not
// treated as ellipse parameters or polygon vertices: callers must provide the
// corresponding geometry target tensors when training this branch.
struct GeometryRoiHeadConfig {
    bool enabled = false;
    int64_t hidden_channels = 64;
    int64_t polygon_vertex_count = 8;
    int64_t pooled_size = 4;

    void validate() const {
        TORCH_CHECK(hidden_channels > 0, "geometry ROI hidden_channels must be positive");
        TORCH_CHECK(polygon_vertex_count >= 3,
            "geometry ROI polygon_vertex_count must be at least three");
        TORCH_CHECK(pooled_size >= 2,
            "geometry ROI pooled_size must be at least two");
    }
};

struct GeometryRoiLossConfig {
    float parameter_weight = 1.0f;
    float contour_weight = 1.0f;
    float continuity_weight = 0.5f;
    float uncertainty_weight = 0.25f;
    float continuity_positive_weight = 1.0f;
    float calibration_weight = 0.1f;

    void validate() const {
        TORCH_CHECK(parameter_weight >= 0.0f, "geometry parameter_weight must be non-negative");
        TORCH_CHECK(contour_weight >= 0.0f, "geometry contour_weight must be non-negative");
        TORCH_CHECK(continuity_weight >= 0.0f, "geometry continuity_weight must be non-negative");
        TORCH_CHECK(uncertainty_weight >= 0.0f, "geometry uncertainty_weight must be non-negative");
        TORCH_CHECK(continuity_positive_weight > 0.0f,
            "geometry continuity_positive_weight must be positive");
        TORCH_CHECK(calibration_weight >= 0.0f,
            "geometry calibration_weight must be non-negative");
    }
};

// kind: 1 = ellipse, 2 = polygon.  ROI coordinates are normalized xyxy and
// use [batch_index, x1, y1, x2, y2].  The values come from a geometry target
// manifest / annotation pipeline, never from detector boxes fabricated as
// geometry labels.
struct GeometryRoiTargets {
    torch::Tensor rois;
    torch::Tensor kind;
    torch::Tensor ellipse_parameters;
    torch::Tensor polygon_vertices;
    torch::Tensor boundary_continuity;
    torch::Tensor affine_channel;

    bool defined() const {
        return rois.defined() && kind.defined() && ellipse_parameters.defined() &&
            polygon_vertices.defined() && boundary_continuity.defined() &&
            affine_channel.defined();
    }

    void validate(int64_t polygon_vertex_count) const {
        TORCH_CHECK(defined(), "geometry ROI targets must include ROI, kind, ellipse, polygon and continuity tensors");
        TORCH_CHECK(rois.dim() == 2 && rois.size(1) == 5,
            "geometry ROI targets.rois must be [N,5]");
        TORCH_CHECK(kind.dim() == 1 && kind.size(0) == rois.size(0),
            "geometry ROI targets.kind must be [N]");
        TORCH_CHECK(ellipse_parameters.dim() == 2 && ellipse_parameters.size(0) == rois.size(0) &&
                ellipse_parameters.size(1) == 6,
            "geometry ROI ellipse parameters must be [N,6] (cx,cy,rx,ry,sin2a,cos2a)");
        TORCH_CHECK(polygon_vertices.dim() == 2 && polygon_vertices.size(0) == rois.size(0) &&
                polygon_vertices.size(1) == polygon_vertex_count * 2,
            "geometry ROI polygon vertices must be [N,2*polygon_vertex_count]");
        TORCH_CHECK(boundary_continuity.numel() == rois.size(0),
            "geometry ROI boundary continuity must contain one value per ROI");
        TORCH_CHECK(affine_channel.numel() == rois.size(0),
            "geometry ROI affine channel must contain one value per ROI");
    }
};

// Batch target layout used by the YOLO dataset when geometry supervision is
// enabled.  The first six columns remain the normal detector target layout.
// Geometry columns are sidecar-provided annotations, never derived from a box.
struct GeometryRoiTargetLayout {
    static constexpr int64_t DetectorColumns = 6;
    static constexpr int64_t Kind = DetectorColumns;
    static constexpr int64_t RoiX1 = Kind + 1;
    static constexpr int64_t RoiY1 = RoiX1 + 1;
    static constexpr int64_t RoiX2 = RoiY1 + 1;
    static constexpr int64_t RoiY2 = RoiX2 + 1;
    static constexpr int64_t Ellipse = RoiY2 + 1;
    static constexpr int64_t EllipseColumns = 6;

    static int64_t polygon_offset() { return Ellipse + EllipseColumns; }
    static int64_t continuity_offset(int64_t polygon_vertex_count) {
        return polygon_offset() + polygon_vertex_count * 2;
    }
    static int64_t affine_channel_offset(int64_t polygon_vertex_count) {
        return continuity_offset(polygon_vertex_count) + 1;
    }
    static int64_t total_columns(int64_t polygon_vertex_count) {
        return affine_channel_offset(polygon_vertex_count) + 1;
    }
};

inline GeometryRoiTargets geometry_roi_targets_from_batch(
    const torch::Tensor& batch_targets, int64_t polygon_vertex_count) {
    TORCH_CHECK(batch_targets.dim() == 3,
        "geometry ROI batch targets must be [B,max_instances,columns]");
    TORCH_CHECK(batch_targets.size(2) == GeometryRoiTargetLayout::total_columns(polygon_vertex_count),
        "geometry ROI batch target column count does not match polygon_vertex_count");
    const torch::Tensor valid = batch_targets.select(2, GeometryRoiTargetLayout::Kind) > 0;
    const torch::Tensor indices = torch::nonzero(valid);
    TORCH_CHECK(indices.size(0) > 0,
        "geometry ROI batch has no sidecar-supervised instances");
    const torch::Tensor selected = batch_targets.index({indices.select(1, 0), indices.select(1, 1)});
    return GeometryRoiTargets{
        torch::cat({indices.select(1, 0).to(selected.options().dtype()).unsqueeze(1),
            selected.slice(1, GeometryRoiTargetLayout::RoiX1, GeometryRoiTargetLayout::RoiY2 + 1)}, 1),
        selected.select(1, GeometryRoiTargetLayout::Kind).to(torch::kLong),
        selected.slice(1, GeometryRoiTargetLayout::Ellipse,
            GeometryRoiTargetLayout::Ellipse + GeometryRoiTargetLayout::EllipseColumns),
        selected.slice(1, GeometryRoiTargetLayout::polygon_offset(),
            GeometryRoiTargetLayout::continuity_offset(polygon_vertex_count)),
        selected.select(1, GeometryRoiTargetLayout::continuity_offset(polygon_vertex_count)),
        selected.select(1, GeometryRoiTargetLayout::affine_channel_offset(polygon_vertex_count))};
}

struct GeometryRoiPrediction {
    torch::Tensor ellipse_parameters;
    torch::Tensor polygon_vertices;
    torch::Tensor boundary_continuity_logits;
    torch::Tensor log_variance;
};

class GeometryRoiHeadImpl : public torch::nn::Module {
public:
    GeometryRoiHeadImpl(int64_t feature_channels, GeometryRoiHeadConfig config)
        : config_(config) {
        config_.validate();
        TORCH_CHECK(feature_channels > 0, "geometry ROI feature_channels must be positive");
        projector_ = register_module("projector", torch::nn::Sequential(
            torch::nn::Conv2d(torch::nn::Conv2dOptions(feature_channels, config_.hidden_channels, 1)),
            torch::nn::ReLU(torch::nn::ReLUOptions(true)),
            torch::nn::Conv2d(torch::nn::Conv2dOptions(config_.hidden_channels, config_.hidden_channels, 3).padding(1)),
            torch::nn::ReLU(torch::nn::ReLUOptions(true))));
        const int64_t representation_size = config_.hidden_channels *
            config_.pooled_size * config_.pooled_size;
        ellipse_ = register_module("ellipse", torch::nn::Linear(representation_size, 6));
        polygon_ = register_module("polygon", torch::nn::Linear(
            representation_size, config_.polygon_vertex_count * 2));
        continuity_ = register_module("continuity", torch::nn::Linear(representation_size, 1));
        uncertainty_ = register_module("uncertainty", torch::nn::Linear(representation_size, 3));
    }

    GeometryRoiPrediction forward(const torch::Tensor& feature_map,
                                  const torch::Tensor& normalized_rois) {
        TORCH_CHECK(feature_map.dim() == 4, "geometry ROI feature map must be [B,C,H,W]");
        TORCH_CHECK(normalized_rois.dim() == 2 && normalized_rois.size(1) == 5,
            "geometry ROI normalized_rois must be [N,5]");
        TORCH_CHECK(normalized_rois.size(0) > 0, "geometry ROI requires at least one ROI");
        const torch::Tensor cpu_rois = normalized_rois.detach().to(torch::kCPU);
        std::vector<torch::Tensor> pooled;
        pooled.reserve(static_cast<size_t>(cpu_rois.size(0)));
        const int64_t height = feature_map.size(2);
        const int64_t width = feature_map.size(3);
        for (int64_t index = 0; index < cpu_rois.size(0); ++index) {
            const int64_t batch = std::clamp(cpu_rois[index][0].item<int64_t>(),
                int64_t{0}, feature_map.size(0) - 1);
            const float x1 = std::clamp(cpu_rois[index][1].item<float>(), 0.0f, 1.0f);
            const float y1 = std::clamp(cpu_rois[index][2].item<float>(), 0.0f, 1.0f);
            const float x2 = std::clamp(cpu_rois[index][3].item<float>(), x1, 1.0f);
            const float y2 = std::clamp(cpu_rois[index][4].item<float>(), y1, 1.0f);
            const int64_t left = std::clamp(static_cast<int64_t>(x1 * width), int64_t{0}, width - 1);
            const int64_t top = std::clamp(static_cast<int64_t>(y1 * height), int64_t{0}, height - 1);
            const int64_t right = std::clamp(static_cast<int64_t>(std::ceil(x2 * width)), left + 1, width);
            const int64_t bottom = std::clamp(static_cast<int64_t>(std::ceil(y2 * height)), top + 1, height);
            const torch::Tensor crop = feature_map.select(0, batch)
                .narrow(1, top, bottom - top)
                .narrow(2, left, right - left)
                .unsqueeze(0);
            pooled.push_back(torch::adaptive_avg_pool2d(projector_->forward(crop),
                {config_.pooled_size, config_.pooled_size}).reshape({1, -1}));
        }
        const torch::Tensor representation = torch::cat(pooled, 0);
        const torch::Tensor ellipse_raw = ellipse_->forward(representation);
        const torch::Tensor ellipse_position_scale = torch::sigmoid(ellipse_raw.slice(1, 0, 4));
        const torch::Tensor ellipse_angle_raw = torch::tanh(ellipse_raw.slice(1, 4, 6));
        const torch::Tensor ellipse_angle = ellipse_angle_raw /
            torch::sqrt(torch::pow(ellipse_angle_raw, 2).sum(1, true).clamp_min(1e-6));
        return GeometryRoiPrediction{
            torch::cat({ellipse_position_scale, ellipse_angle}, 1),
            torch::sigmoid(polygon_->forward(representation)),
            continuity_->forward(representation),
            uncertainty_->forward(representation).clamp(-3.0, 3.0)};
    }

    const GeometryRoiHeadConfig& config() const { return config_; }

private:
    GeometryRoiHeadConfig config_;
    torch::nn::Sequential projector_{nullptr};
    torch::nn::Linear ellipse_{nullptr};
    torch::nn::Linear polygon_{nullptr};
    torch::nn::Linear continuity_{nullptr};
    torch::nn::Linear uncertainty_{nullptr};
};
TORCH_MODULE(GeometryRoiHead);

class GeometryRoiLossImpl : public torch::nn::Module {
public:
    GeometryRoiLossImpl(int64_t polygon_vertex_count, GeometryRoiLossConfig config)
        : polygon_vertex_count_(polygon_vertex_count), config_(config) {
        TORCH_CHECK(polygon_vertex_count_ >= 3, "geometry ROI polygon_vertex_count must be at least three");
        config_.validate();
    }

    std::tuple<torch::Tensor, std::unordered_map<std::string, float>> forward(
        const GeometryRoiPrediction& prediction, const GeometryRoiTargets& raw_targets) const {
        raw_targets.validate(polygon_vertex_count_);
        const auto device = prediction.ellipse_parameters.device();
        const auto options = prediction.ellipse_parameters.options();
        const torch::Tensor kinds = raw_targets.kind.to(device, torch::kLong);
        const torch::Tensor ellipse_targets = raw_targets.ellipse_parameters.to(device, options.dtype());
        const torch::Tensor polygon_targets = raw_targets.polygon_vertices.to(device, options.dtype());
        const torch::Tensor continuity_targets = raw_targets.boundary_continuity
            .to(device, options.dtype()).reshape({-1, 1});
        const torch::Tensor ellipse_mask = kinds == 1;
        const torch::Tensor polygon_mask = kinds == 2;
        torch::Tensor parameter_loss = torch::zeros({}, options);
        torch::Tensor contour_loss = torch::zeros({}, options);
        torch::Tensor uncertainty_loss = torch::zeros({}, options);
        if (ellipse_mask.any().item<bool>()) {
            const torch::Tensor predicted = prediction.ellipse_parameters.index({ellipse_mask});
            const torch::Tensor target = ellipse_targets.index({ellipse_mask});
            parameter_loss = torch::smooth_l1_loss(predicted, target);
            const torch::Tensor squared = torch::pow(predicted - target, 2).mean(1, true);
            const torch::Tensor log_variance = prediction.log_variance.index({ellipse_mask}).select(1, 0).unsqueeze(1);
            uncertainty_loss = uncertainty_loss +
                (torch::exp(-log_variance) * squared + log_variance).mean();
        }
        if (polygon_mask.any().item<bool>()) {
            const torch::Tensor predicted = prediction.polygon_vertices.index({polygon_mask});
            const torch::Tensor target = polygon_targets.index({polygon_mask});
            contour_loss = torch::smooth_l1_loss(predicted, target);
            const torch::Tensor squared = torch::pow(predicted - target, 2).mean(1, true);
            const torch::Tensor log_variance = prediction.log_variance.index({polygon_mask}).select(1, 1).unsqueeze(1);
            uncertainty_loss = uncertainty_loss +
                (torch::exp(-log_variance) * squared + log_variance).mean();
        }
        const torch::Tensor positive_term = -config_.continuity_positive_weight *
            continuity_targets * torch::log_sigmoid(prediction.boundary_continuity_logits);
        const torch::Tensor negative_term = -(1.0f - continuity_targets) *
            torch::log_sigmoid(-prediction.boundary_continuity_logits);
        const torch::Tensor topology_loss = (positive_term + negative_term).mean();
        const torch::Tensor calibration_loss = torch::pow(
            torch::sigmoid(prediction.boundary_continuity_logits) - continuity_targets, 2).mean();
        const torch::Tensor weighted_parameter = parameter_loss * config_.parameter_weight;
        const torch::Tensor weighted_contour = contour_loss * config_.contour_weight;
        const torch::Tensor weighted_topology = topology_loss * config_.continuity_weight;
        const torch::Tensor weighted_uncertainty = uncertainty_loss * config_.uncertainty_weight;
        const torch::Tensor weighted_calibration = calibration_loss * config_.calibration_weight;
        const torch::Tensor total = weighted_parameter + weighted_contour +
            weighted_topology + weighted_uncertainty + weighted_calibration;
        return std::make_tuple(total, std::unordered_map<std::string, float>{
            {"geometry_roi_parameter_loss", weighted_parameter.item<float>()},
            {"geometry_roi_contour_loss", weighted_contour.item<float>()},
            {"geometry_roi_topology_loss", weighted_topology.item<float>()},
            {"geometry_roi_uncertainty_loss", weighted_uncertainty.item<float>()},
            {"geometry_roi_calibration_loss", weighted_calibration.item<float>()},
            {"geometry_roi_total_loss", total.item<float>()}});
    }

private:
    int64_t polygon_vertex_count_;
    GeometryRoiLossConfig config_;
};
TORCH_MODULE(GeometryRoiLoss);

#endif
