#ifndef TORCH_YOLOV8_LOSS_H
#define TORCH_YOLOV8_LOSS_H


#include <torch/torch.h>
#include <vector>
#include <tuple>
#include <cmath>
#include <algorithm>
#include <unordered_map>

#include "torch_taskalignedassigner.h"
#include "torch_util.h"
#include "torch_detect.h"
#include "torch_yolo_head.h"

namespace F = torch::nn::functional;


enum class YoloBoxDecodeStrategy {
    DirectStrideScaled
};

struct YoloLossConfig {
    float box_weight = 7.5f;
    float cls_weight = 0.5f;
    float dfl_weight = 1.5f;
    float fl_gamma = 0.0f;
    int64_t topk = 10;
    int64_t reg_max = 16;
    YoloBoxDecodeStrategy decode_strategy = YoloBoxDecodeStrategy::DirectStrideScaled;
    bool enable_dfl = false;
    bool use_assignment_quality_targets = true;
    bool global_multiscale_assignment = true;
    std::vector<float> class_loss_weights;

    void validate(int64_t num_classes) const {
        TORCH_CHECK(num_classes > 0, "num_classes must be positive");
        TORCH_CHECK(box_weight >= 0.0f, "box_weight must be non-negative");
        TORCH_CHECK(cls_weight >= 0.0f, "cls_weight must be non-negative");
        TORCH_CHECK(dfl_weight >= 0.0f, "dfl_weight must be non-negative");
        TORCH_CHECK(fl_gamma >= 0.0f, "fl_gamma must be non-negative");
        TORCH_CHECK(topk > 0, "topk must be positive");
        TORCH_CHECK(reg_max > 0, "reg_max must be positive");
        TORCH_CHECK(class_loss_weights.empty() ||
            static_cast<int64_t>(class_loss_weights.size()) == num_classes,
            "class_loss_weights must be empty or match num_classes");
        for (const float weight : class_loss_weights)
            TORCH_CHECK(weight > 0.0f && std::isfinite(weight),
                "class_loss_weights must contain finite positive values");
    }

    int64_t box_channels() const {
        return enable_dfl ? reg_max * 4 : 4;
    }
};

class YOLOv8LossImpl : public torch::nn::Module {
public:
    YOLOv8LossImpl(int64_t num_classes,
        float box_weight = 7.5f,
        float cls_weight = 0.5f,
        float dfl_weight = 1.5f,
        float fl_gamma = 0.0f,
        int64_t topk = 10)
        : num_classes_(num_classes),
        config_({ box_weight, cls_weight, dfl_weight, fl_gamma, topk, 16 }) {
        initialize_modules();
    }

    YOLOv8LossImpl(int64_t num_classes, const YoloLossConfig& config)
        : num_classes_(num_classes), config_(config) {
        initialize_modules();
    }

private:
    void initialize_modules() {
        config_.validate(num_classes_);

        assigner = register_module("assigner", TaskAlignedAssigner(config_.topk, num_classes_, 1.0f, 6.0f));

        strides_ = register_buffer("strides", torch::tensor({ 8, 16, 32 }, torch::kFloat32));
    }

public:

    std::tuple<torch::Tensor, std::unordered_map<std::string, float>> forward(
        const std::vector<torch::Tensor>& preds,
        const torch::Tensor& targets_in) {

        torch::Device device = strides_.device();
        torch::Tensor targets = targets_in.to(device);

        torch::Tensor loss_box = torch::zeros({ 1 }, device);
        torch::Tensor loss_cls = torch::zeros({ 1 }, device);
        torch::Tensor loss_dfl = torch::zeros({ 1 }, device);
        torch::Tensor total_num_pos = torch::zeros({ 1 }, device);
        torch::Tensor total_cls_target_weight = torch::zeros({ 1 }, device);

        int64_t batch_size = targets.size(0);

        torch::Tensor gt_labels = targets.select(2, 1).to(torch::kLong);
        torch::Tensor gt_bboxes = targets.slice(2, 2, 6);
        torch::Tensor mask_gt = (targets.select(2, 1) >= 0).to(torch::kFloat32);

        // Direct normalized boxes from every detection scale share the same
        // coordinate space.  Assign them once as one anchor population so the
        // configured top-k is global rather than being multiplied by P3/P4/P5.
        // DFL keeps the existing per-scale path because its raw distances are
        // stride-specific and need to remain paired with their source scale.
        if (config_.global_multiscale_assignment && !config_.enable_dfl) {
            std::vector<torch::Tensor> decoded_scales;
            std::vector<torch::Tensor> class_scales;
            decoded_scales.reserve(preds.size());
            class_scales.reserve(preds.size());
            for (const torch::Tensor& pred : preds) {
                const int64_t box_channels = config_.box_channels();
                const torch::Tensor pred_boxes = pred.slice(2, 0, box_channels);
                const torch::Tensor pred_cls =
                    pred.slice(2, box_channels, box_channels + num_classes_);
                const int64_t grid_size = static_cast<int64_t>(
                    std::llround(std::sqrt(static_cast<double>(pred_boxes.size(1)))));
                TORCH_CHECK(grid_size * grid_size == pred_boxes.size(1),
                    "YOLO direct box decode requires a square feature grid");
                decoded_scales.push_back(
                    decode_boxes(pred_boxes, grid_size, grid_size));
                class_scales.push_back(pred_cls);
            }
            const torch::Tensor decoded_boxes = torch::cat(decoded_scales, 1);
            const torch::Tensor pred_cls = torch::cat(class_scales, 1);
            const torch::Tensor assigned_gt_inds = assigner->forward(
                torch::sigmoid(pred_cls).unsqueeze(1), decoded_boxes.unsqueeze(1),
                gt_labels.unsqueeze(-1), gt_bboxes, mask_gt.unsqueeze(-1));
            const torch::Tensor pos_mask = assigned_gt_inds > 0;
            torch::Tensor assignment_quality = torch::zeros_like(
                assigned_gt_inds, assigned_gt_inds.options().dtype(pred_cls.dtype()));
            total_num_pos = pos_mask.sum();
            if (total_num_pos.item<float>() > 0) {
                const torch::Tensor pos_decoded_boxes = decoded_boxes
                    .masked_select(pos_mask.unsqueeze(-1)).view({-1, 4});
                const auto batch_idx = torch::arange(batch_size, device)
                    .view({-1, 1}).expand_as(assigned_gt_inds);
                const auto valid_batch_idx = batch_idx.masked_select(pos_mask);
                const auto valid_gt_idx = assigned_gt_inds.masked_select(pos_mask)
                    .to(torch::kLong) - 1;
                const auto flat_indices = valid_batch_idx * gt_bboxes.size(1) +
                    valid_gt_idx;
                const torch::Tensor pos_gt_boxes = gt_bboxes.view({-1, 4})
                    .index_select(0, flat_indices);
                const torch::Tensor iou =
                    bbox_iou(pos_decoded_boxes, pos_gt_boxes, true);
                loss_box = (1.0f - iou).sum();
                const torch::Tensor quality = config_.use_assignment_quality_targets
                    ? bbox_iou(pos_decoded_boxes, pos_gt_boxes, false)
                          .detach().clamp(0.0f, 1.0f)
                    : torch::ones({pos_decoded_boxes.size(0)}, pred_cls.options());
                assignment_quality.masked_scatter_(pos_mask, quality);
                total_cls_target_weight = quality.sum();
            }
            loss_cls = compute_cls_loss_vectorized(
                pred_cls, gt_labels, assigned_gt_inds, pos_mask,
                assignment_quality).sum();
            const torch::Tensor num_pos_safe = torch::max(
                total_num_pos, torch::tensor(1.0f, device));
            loss_box = (loss_box / num_pos_safe) * config_.box_weight;
            const torch::Tensor cls_weight_safe = torch::max(
                total_cls_target_weight, torch::tensor(1.0f, device));
            loss_cls = (loss_cls / cls_weight_safe) * config_.cls_weight;
            const torch::Tensor total_loss = loss_box + loss_cls;
            return std::make_tuple(total_loss,
                std::unordered_map<std::string, float>{
                    {"box_loss", loss_box.item<float>()},
                    {"cls_loss", loss_cls.item<float>()},
                    {"dfl_loss", 0.0f},
                    {"total_loss", total_loss.item<float>()}});
        }

        for (size_t i = 0; i < preds.size(); ++i) {
            torch::Tensor pred = preds[i];
            int64_t box_channels = config_.box_channels();

            torch::Tensor pred_boxes = pred.slice(2, 0, box_channels);
            torch::Tensor pred_cls = pred.slice(2, box_channels, box_channels + num_classes_);

            const int64_t grid_size = static_cast<int64_t>(
                std::llround(std::sqrt(static_cast<double>(pred_boxes.size(1)))));
            TORCH_CHECK(grid_size * grid_size == pred_boxes.size(1),
                "YOLO direct box decode requires a square feature grid");
            torch::Tensor decoded_boxes = decode_boxes(
                pred_boxes, grid_size, grid_size);

            torch::Tensor assigned_gt_inds = assigner->forward(
                torch::sigmoid(pred_cls).unsqueeze(1),
                decoded_boxes.unsqueeze(1),
                gt_labels.unsqueeze(-1),
                gt_bboxes,
                mask_gt.unsqueeze(-1)
            );

            torch::Tensor pos_mask = assigned_gt_inds > 0;
            torch::Tensor assignment_quality = torch::zeros_like(
                assigned_gt_inds, assigned_gt_inds.options().dtype(pred_cls.dtype()));
            torch::Tensor num_pos_scale = pos_mask.sum();
            total_num_pos += num_pos_scale;

            if (num_pos_scale.item<float>() > 0) {
                torch::Tensor pos_decoded_boxes = decoded_boxes.masked_select(pos_mask.unsqueeze(-1)).view({ -1, 4 });

                auto batch_idx = torch::arange(batch_size, device).view({ -1, 1 }).expand_as(assigned_gt_inds);

                auto valid_batch_idx = batch_idx.masked_select(pos_mask);
                auto valid_gt_idx = assigned_gt_inds.masked_select(pos_mask).to(torch::kLong) - 1;

                auto gt_bboxes_flat = gt_bboxes.view({ -1, 4 });
                auto flat_indices = valid_batch_idx * gt_bboxes.size(1) + valid_gt_idx;

                torch::Tensor pos_gt_boxes = gt_bboxes_flat.index_select(0, flat_indices);

                torch::Tensor iou = bbox_iou(pos_decoded_boxes, pos_gt_boxes, true);
                loss_box = loss_box + (1.0f - iou).sum();

                const torch::Tensor quality = config_.use_assignment_quality_targets
                    ? bbox_iou(pos_decoded_boxes, pos_gt_boxes, false).detach().clamp(0.0f, 1.0f)
                    : torch::ones({pos_decoded_boxes.size(0)}, pred_cls.options());
                assignment_quality.masked_scatter_(pos_mask, quality);
                total_cls_target_weight += quality.sum();

                if (config_.enable_dfl) {
                    torch::Tensor pos_pred_boxes = pred_boxes.masked_select(pos_mask.unsqueeze(-1)).view({-1, config_.box_channels()});
                    loss_dfl = loss_dfl + compute_dfl_loss(pos_pred_boxes, pos_gt_boxes, strides_[i].item<int64_t>()).sum();
                }
            }

            torch::Tensor scale_cls_loss = compute_cls_loss_vectorized(
                pred_cls, gt_labels, assigned_gt_inds, pos_mask,
                assignment_quality);

            loss_cls = loss_cls + scale_cls_loss.sum();
        }

        torch::Tensor num_pos_safe = torch::max(total_num_pos, torch::tensor(1.0f, device));

        loss_box = (loss_box / num_pos_safe) * config_.box_weight;
        torch::Tensor cls_weight_safe = torch::max(
            total_cls_target_weight, torch::tensor(1.0f, device));
        loss_cls = (loss_cls / cls_weight_safe) * config_.cls_weight;
        loss_dfl = (loss_dfl / num_pos_safe) * config_.dfl_weight;

        torch::Tensor total_loss = loss_box + loss_cls + loss_dfl;

        std::unordered_map<std::string, float> loss_components = {
            {"box_loss", loss_box.item<float>()},
            {"cls_loss", loss_cls.item<float>()},
            {"dfl_loss", loss_dfl.item<float>()},
            {"total_loss", total_loss.item<float>()}
        };

        return std::make_tuple(total_loss, loss_components);
    }

private:
    torch::Tensor decode_boxes(
        const torch::Tensor& pred_boxes,
        int64_t grid_height,
        int64_t grid_width) {
        switch (config_.decode_strategy) {
        case YoloBoxDecodeStrategy::DirectStrideScaled:
        default:
            if (!config_.enable_dfl) {
                return decode_yolo_direct_boxes_normalized(
                    pred_boxes, grid_height, grid_width);
            }
            return decode_dfl_boxes(pred_boxes, strides_[0].item<int64_t>());
        }
    }

    torch::Tensor decode_dfl_boxes(const torch::Tensor& pred_boxes, int64_t stride) {
        TORCH_CHECK(config_.enable_dfl, "decode_dfl_boxes requires enable_dfl=true");
        TORCH_CHECK(pred_boxes.size(-1) == config_.box_channels(),
            "DFL box channel mismatch, expected ", config_.box_channels(), " got ", pred_boxes.size(-1));

        auto shape = pred_boxes.sizes();
        auto pred = pred_boxes.view({shape[0], shape[1], 4, config_.reg_max});
        auto probs = torch::softmax(pred, -1);
        auto bins = torch::arange(config_.reg_max, probs.options());
        auto distances = (probs * bins).sum(-1);
        return distances * static_cast<float>(stride);
    }

    torch::Tensor compute_dfl_loss(
        const torch::Tensor& pred_boxes,
        const torch::Tensor& gt_boxes,
        int64_t stride) {
        TORCH_CHECK(config_.enable_dfl, "compute_dfl_loss requires enable_dfl=true");
        TORCH_CHECK(pred_boxes.size(-1) == config_.box_channels(),
            "DFL box channel mismatch, expected ", config_.box_channels(), " got ", pred_boxes.size(-1));
        TORCH_CHECK(gt_boxes.size(-1) == 4, "DFL gt boxes must have 4 channels");

        auto logits = pred_boxes.view({pred_boxes.size(0), 4, config_.reg_max});

        auto target = (gt_boxes / static_cast<float>(stride)).clamp(0.0f, static_cast<float>(config_.reg_max - 1) - 1e-3f);
        auto target_left = torch::floor(target).to(torch::kLong);
        auto target_right = torch::clamp(target_left + 1, 0, config_.reg_max - 1);
        auto weight_right = (target - target_left.to(target.dtype())).clamp(0.0f, 1.0f);
        auto weight_left = 1.0f - weight_right;

        auto flat_logits = logits.view({-1, config_.reg_max});
        auto flat_left = target_left.view({-1});
        auto flat_right = target_right.view({-1});
        auto flat_w_left = weight_left.view({-1});
        auto flat_w_right = weight_right.view({-1});

        auto ce_left = F::cross_entropy(
            flat_logits,
            flat_left,
            F::CrossEntropyFuncOptions().reduction(torch::kNone));
        auto ce_right = F::cross_entropy(
            flat_logits,
            flat_right,
            F::CrossEntropyFuncOptions().reduction(torch::kNone));

        auto loss = ce_left * flat_w_left + ce_right * flat_w_right;
        return loss.view({pred_boxes.size(0), 4}).sum(1);
    }

    torch::Tensor compute_cls_loss_vectorized(
        const torch::Tensor& pred_cls,
        const torch::Tensor& gt_labels,
        const torch::Tensor& assigned_gt_inds,
        const torch::Tensor& pos_mask,
        const torch::Tensor& assignment_quality)
    {
        torch::Tensor target_one_hot = torch::zeros_like(pred_cls);

        if (pos_mask.any().item<bool>()) {
            auto gather_inds = (assigned_gt_inds - 1).clamp_min(0).to(torch::kLong);

            // Padded GT rows use class -1.  `gather_inds` is intentionally
            // evaluated for every anchor before `pos_mask` is applied, so
            // clamp those non-positive rows to a harmless in-range class for
            // one_hot; the following pos_mask makes their target exactly zero.
            // This preserves the negative/background contract rather than
            // fabricating a class label for padding.
            auto target_cls_ids = gt_labels.gather(1, gather_inds)
                .clamp(0, num_classes_ - 1);

            auto one_hot = F::one_hot(target_cls_ids, num_classes_).to(pred_cls.dtype());

            target_one_hot = one_hot * pos_mask.unsqueeze(-1) *
                assignment_quality.unsqueeze(-1);
        }

        auto bce_options = F::BinaryCrossEntropyWithLogitsFuncOptions().reduction(torch::kNone);
        torch::Tensor loss = F::binary_cross_entropy_with_logits(pred_cls, target_one_hot, bce_options);

        if (config_.fl_gamma > 0.0f) {
            torch::Tensor pt = torch::exp(-loss);
            loss = (1.0f - pt).pow(config_.fl_gamma) * loss;
        }

        if (!config_.class_loss_weights.empty()) {
            const torch::Tensor weights = torch::tensor(
                config_.class_loss_weights, pred_cls.options())
                .view({1, 1, num_classes_});
            loss = loss * weights;
        }

        return loss;
    }

    torch::Tensor bbox_iou(const torch::Tensor& boxes1, const torch::Tensor& boxes2, bool ciou = true) {
        auto b1_x1 = boxes1.select(1, 0);
        auto b1_y1 = boxes1.select(1, 1);
        auto b1_x2 = boxes1.select(1, 2);
        auto b1_y2 = boxes1.select(1, 3);
        auto b2_x1 = boxes2.select(1, 0);
        auto b2_y1 = boxes2.select(1, 1);
        auto b2_x2 = boxes2.select(1, 2);
        auto b2_y2 = boxes2.select(1, 3);
        auto b1_w = (b1_x2 - b1_x1).clamp_min(0);
        auto b1_h = (b1_y2 - b1_y1).clamp_min(0);
        auto b2_w = (b2_x2 - b2_x1).clamp_min(0);
        auto b2_h = (b2_y2 - b2_y1).clamp_min(0);

        auto inter_x1 = torch::max(b1_x1, b2_x1);
        auto inter_y1 = torch::max(b1_y1, b2_y1);
        auto inter_x2 = torch::min(b1_x2, b2_x2);
        auto inter_y2 = torch::min(b1_y2, b2_y2);

        auto inter_area = (inter_x2 - inter_x1).clamp_min(0) * (inter_y2 - inter_y1).clamp_min(0);
        auto union_area = (b1_w * b1_h) + (b2_w * b2_h) - inter_area;

        auto iou = inter_area / (union_area + 1e-7f);

        if (ciou) {
            auto c_x1 = torch::min(b1_x1, b2_x1);
            auto c_y1 = torch::min(b1_y1, b2_y1);
            auto c_x2 = torch::max(b1_x2, b2_x2);
            auto c_y2 = torch::max(b1_y2, b2_y2);

            auto c_diag_sq = (c_x2 - c_x1).pow(2) + (c_y2 - c_y1).pow(2) + 1e-7f;

            auto b1_center_x = (b1_x1 + b1_x2) * 0.5f;
            auto b1_center_y = (b1_y1 + b1_y2) * 0.5f;
            auto b2_center_x = (b2_x1 + b2_x2) * 0.5f;
            auto b2_center_y = (b2_y1 + b2_y2) * 0.5f;
            auto rho_sq = (b2_center_x - b1_center_x).pow(2) +
                (b2_center_y - b1_center_y).pow(2);

            auto w2_h2 = b2_w / (b2_h + 1e-7f);
            auto w1_h1 = b1_w / (b1_h + 1e-7f);
            auto v = (4.0f / (M_PI * M_PI)) * torch::pow(torch::atan(w2_h2) - torch::atan(w1_h1), 2);

            torch::Tensor alpha;
            {
                torch::NoGradGuard no_grad;
                alpha = v / (1.0f - iou + v + 1e-7f);
            }

            return iou - (rho_sq / c_diag_sq + v * alpha);
        }
        return iou;
    }

    int64_t num_classes_;
    YoloLossConfig config_;

    TaskAlignedAssigner assigner{ nullptr };
    torch::Tensor strides_;
};

TORCH_MODULE(YOLOv8Loss);

#endif
