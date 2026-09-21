#ifndef TORCH_YOLO_HEAD_H
#define TORCH_YOLO_HEAD_H

#include <torch/torch.h>

#include <algorithm>
#include <cstdint>
#include <vector>

#include "torch_nnmodule.h"

inline torch::Tensor decode_yolo_direct_boxes_normalized(
    const torch::Tensor& raw_boxes,
    int64_t grid_height,
    int64_t grid_width) {
    TORCH_CHECK(raw_boxes.dim() == 3 && raw_boxes.size(2) == 4,
        "direct YOLO boxes must be [B, anchors, 4], got ", raw_boxes.sizes());
    TORCH_CHECK(grid_height > 0 && grid_width > 0 &&
        raw_boxes.size(1) == grid_height * grid_width,
        "direct YOLO box grid does not match anchor count");

    auto options = raw_boxes.options();
    auto grid_y = torch::arange(grid_height, options)
        .view({grid_height, 1})
        .expand({grid_height, grid_width})
        .reshape({1, -1});
    auto grid_x = torch::arange(grid_width, options)
        .view({1, grid_width})
        .expand({grid_height, grid_width})
        .reshape({1, -1});
    auto activated = torch::sigmoid(raw_boxes);
    auto center_x = (activated.select(2, 0) + grid_x) /
        static_cast<double>(grid_width);
    auto center_y = (activated.select(2, 1) + grid_y) /
        static_cast<double>(grid_height);
    auto width = activated.select(2, 2);
    auto height = activated.select(2, 3);

    return torch::stack({
        (center_x - width * 0.5).clamp(0.0, 1.0),
        (center_y - height * 0.5).clamp(0.0, 1.0),
        (center_x + width * 0.5).clamp(0.0, 1.0),
        (center_y + height * 0.5).clamp(0.0, 1.0)}, 2);
}

struct YoloDetectHeadConfig {
    int64_t reg_max = 16;
    int64_t min_box_channels = 16;
    int64_t max_class_channels = 100;
    bool use_dfl = false;

    void validate(int64_t num_classes, const std::vector<int64_t>& ch, const std::vector<int64_t>& strides) const {
        TORCH_CHECK(num_classes > 0, "detect head num_classes must be positive");
        TORCH_CHECK(!ch.empty(), "detect head channels must not be empty");
        TORCH_CHECK(ch.size() == strides.size(), "detect head channel/stride count mismatch");
        TORCH_CHECK(reg_max > 0, "detect head reg_max must be positive");
        TORCH_CHECK(min_box_channels > 0, "detect head min_box_channels must be positive");
        TORCH_CHECK(max_class_channels > 0, "detect head max_class_channels must be positive");
    }

    int64_t box_output_channels() const {
        return use_dfl ? reg_max * 4 : 4;
    }
};

class YOLOv8DetectImpl : public torch::nn::Module {
public:
    YOLOv8DetectImpl(int64_t num_classes, const std::vector<int64_t>& ch, const std::vector<int64_t>& strides)
        : YOLOv8DetectImpl(num_classes, ch, strides, YoloDetectHeadConfig{}) {
    }

    YOLOv8DetectImpl(
        int64_t num_classes,
        const std::vector<int64_t>& ch,
        const std::vector<int64_t>& strides,
        const YoloDetectHeadConfig& head_config)
        : num_classes_(num_classes), strides_(strides), head_config_(head_config) {

        head_config_.validate(num_classes, ch, strides);

        int64_t num_cv2 = std::max(head_config_.min_box_channels, ch[0] / 4);
        int64_t num_cv3 = std::max(ch[0], std::min(num_classes, head_config_.max_class_channels));

        for (const auto& c : ch) {
            auto cv2 = torch::nn::Sequential(
                ConvModule(c, num_cv2, 3),
                ConvModule(num_cv2, num_cv2, 3),
                torch::nn::Conv2d(torch::nn::Conv2dOptions(num_cv2, head_config_.box_output_channels(), 1))
            );
            cv2_layers_->push_back(register_module("cv2_" + std::to_string(cv2_layers_->size()), cv2));

            auto cv3 = torch::nn::Sequential(
                ConvModule(c, num_cv3, 3),
                ConvModule(num_cv3, num_cv3, 3),
                torch::nn::Conv2d(torch::nn::Conv2dOptions(num_cv3, num_classes, 1))
            );
            cv3_layers_->push_back(register_module("cv3_" + std::to_string(cv3_layers_->size()), cv3));
        }
    }

    int64_t box_output_channels() const { return head_config_.box_output_channels(); }
    int64_t class_output_channels() const { return num_classes_; }
    const YoloDetectHeadConfig& get_config() const { return head_config_; }

    torch::Tensor forward(const std::vector<torch::Tensor>& x) {
        std::vector<torch::Tensor> preds;
        for (size_t i = 0; i < x.size(); ++i) {
            auto feat = x[i];
            auto box_out = cv2_layers_[i]->as<torch::nn::Sequential>()->forward(feat);
            auto cls_out = cv3_layers_[i]->as<torch::nn::Sequential>()->forward(feat);
            int64_t bs = box_out.size(0);
            int64_t h = box_out.size(2);
            int64_t w = box_out.size(3);
            auto raw_boxes = box_out.view({bs, 4, h * w})
                .permute({0, 2, 1}).contiguous();
            auto boxes = decode_yolo_direct_boxes_normalized(raw_boxes, h, w);
            auto class_probabilities = torch::sigmoid(cls_out)
                .view({bs, num_classes_, h * w})
                .permute({0, 2, 1}).contiguous();
            preds.push_back(torch::cat({boxes, class_probabilities}, 2));
        }
        return torch::cat(preds, 1);
    }

    std::vector<torch::Tensor> forward_train(const std::vector<torch::Tensor>& x) {
        std::vector<torch::Tensor> preds;
        for (size_t i = 0; i < x.size(); ++i) {
            auto feat = x[i];
            auto box_out = cv2_layers_[i]->as<torch::nn::Sequential>()->forward(feat);
            auto cls_out = cv3_layers_[i]->as<torch::nn::Sequential>()->forward(feat);
            auto output = torch::cat({ box_out, cls_out }, 1);

            int64_t bs = output.size(0);
            int64_t c = output.size(1);
            int64_t h = output.size(2);
            int64_t w = output.size(3);

            preds.push_back(output.view({ bs, c, h * w }).permute({ 0, 2, 1 }).contiguous());
        }
        return preds;
    }

private:
    int64_t num_classes_;
    std::vector<int64_t> strides_;
    YoloDetectHeadConfig head_config_;
    torch::nn::ModuleList cv2_layers_;
    torch::nn::ModuleList cv3_layers_;
};
TORCH_MODULE(YOLOv8Detect);

#endif
