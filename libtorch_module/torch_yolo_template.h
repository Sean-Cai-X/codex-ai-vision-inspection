#ifndef TORCH_YOLO_TEMPLATE_H
#define TORCH_YOLO_TEMPLATE_H

// NOTE: formatting normalized during the libtorch_module cleanup pass.

#include <torch/torch.h>
#include <vector>
#include "torch_v8.h"
#include "torch_cross_scale_attention.h"

// =========================================================================
// =========================================================================
class TemplateEncoderImpl : public torch::nn::Module {
public:
    TemplateEncoderImpl(int64_t base_ch = 32) {
        // Input -> P1 (Stride 2)
        conv1 = register_module("conv1", ConvModule(2, base_ch, 3, 2, 1));

        // P1 -> P2 (Stride 4)
        conv2 = register_module("conv2", ConvModule(base_ch, base_ch * 2, 3, 2, 1));

        // P2 -> P3 (Stride 8) -> Output T3
        conv3 = register_module("conv3", ConvModule(base_ch * 2, base_ch * 4, 3, 2, 1));

        // P3 -> P4 (Stride 16) -> Output T4
        conv4 = register_module("conv4", ConvModule(base_ch * 4, base_ch * 8, 3, 2, 1));

        // P4 -> P5 (Stride 32) -> Output T5
        conv5 = register_module("conv5", ConvModule(base_ch * 8, base_ch * 16, 3, 2, 1));
    }

    std::vector<torch::Tensor> forward(torch::Tensor templates) {
        auto x = conv1->forward(templates); // /2
        x = conv2->forward(x);              // /4

        auto t3 = conv3->forward(x);        // /8
        auto t4 = conv4->forward(t3);       // /16
        auto t5 = conv5->forward(t4);       // /32

        return { t3, t4, t5 };
    }

    std::vector<int64_t> get_out_channels() const {
        int64_t base = conv1->options().out_channels();
        return { base * 4, base * 8, base * 16 };
    }

private:
    ConvModule conv1{ nullptr }, conv2{ nullptr }, conv3{ nullptr }, conv4{ nullptr }, conv5{ nullptr };
};
TORCH_MODULE(TemplateEncoder);

// =========================================================================
// =========================================================================
class TemplateGuidedPANImpl : public torch::nn::Module {
public:
    TemplateGuidedPANImpl(
        const std::vector<int64_t>& backbone_ch, // [P3, P4, P5]
        const std::vector<int64_t>& template_ch, // [T3, T4, T5]
        float depth_mult, float width_mult)
    {

        csa_p3 = register_module("csa_p3", CrossScaleAttention(backbone_ch[0], template_ch[0], 2));
        csa_p4 = register_module("csa_p4", CrossScaleAttention(backbone_ch[1], template_ch[1], 2));
        csa_p5 = register_module("csa_p5", CrossScaleAttention(backbone_ch[2], template_ch[2], 2));

        pan = register_module("pan", PAN(backbone_ch, depth_mult, width_mult));
    }

    // Forward
    // backbone_feats: [P3, P4, P5]
    // template_feats: [T3, T4, T5]
    std::vector<torch::Tensor> forward(
        const std::vector<torch::Tensor>& backbone_feats,
        const std::vector<torch::Tensor>& template_feats)
    {

        // P3' = Attention(Q=P3, KV=T3) + P3
        auto p3_guided = csa_p3->forward(backbone_feats[0], template_feats[0]);

        // P4' = Attention(Q=P4, KV=T4) + P4
        auto p4_guided = csa_p4->forward(backbone_feats[1], template_feats[1]);

        // P5' = Attention(Q=P5, KV=T5) + P5
        auto p5_guided = csa_p5->forward(backbone_feats[2], template_feats[2]);

        return pan->forward({ p3_guided, p4_guided, p5_guided });
    }

private:
    CrossScaleAttention csa_p3{ nullptr }, csa_p4{ nullptr }, csa_p5{ nullptr };
    PAN pan{ nullptr };
};
TORCH_MODULE(TemplateGuidedPAN);

// =========================================================================
// =========================================================================
class YOLOv8TemplateImpl : public torch::nn::Module {
public:
    YOLOv8TemplateImpl(ModelConfig config) : config_(config) {
        std::vector<int64_t> base_channels = { 64, 128, 256, 512, 1024 };
        backbone_ = register_module("backbone",
            YOLOv8Backbone(base_channels, config.depth_multiple, config.width_multiple));

        template_encoder_ = register_module("template_encoder", TemplateEncoder(32));

        // 3. Guided Neck
        auto backbone_out_ch = backbone_->get_out_channels(); // e.g. {256, 512, 1024}
        auto template_out_ch = template_encoder_->get_out_channels(); // e.g. {128, 256, 512}

        neck_ = register_module("neck",
            TemplateGuidedPAN(backbone_out_ch, template_out_ch, config.depth_multiple, config.width_multiple));

        head_ = register_module("head",
            YOLOv8Detect(config.num_classes, backbone_out_ch, config.strides));

        // 5. Loss
        loss_fn_ = register_module("loss_fn", YOLOv8Loss(config.num_classes));
    }

    // img: [B, 3, H, W]
    // templates: [B, 2, H, W] (Ch0: Positive Mask, Ch1: Negative Mask)
    torch::Tensor forward(torch::Tensor img, torch::Tensor templates) {
        auto img_feats = backbone_->forward(img); // [P3, P4, P5]

        auto tmpl_feats = template_encoder_->forward(templates); // [T3, T4, T5]

        auto neck_outs = neck_->forward(img_feats, tmpl_feats);

        return head_->forward(neck_outs);
    }

    std::tuple<torch::Tensor, std::unordered_map<std::string, float>>
        train_step(torch::Tensor img, torch::Tensor templates, torch::Tensor targets) {
        auto img_feats = backbone_->forward(img);
        auto tmpl_feats = template_encoder_->forward(templates);
        auto neck_outs = neck_->forward(img_feats, tmpl_feats);
        auto preds = head_->forward_train(neck_outs);

        return loss_fn_->forward(preds, targets);
    }

private:
    ModelConfig config_;
    YOLOv8Backbone backbone_{ nullptr };
    TemplateEncoder template_encoder_{ nullptr };
    TemplateGuidedPAN neck_{ nullptr };
    YOLOv8Detect head_{ nullptr };
    YOLOv8Loss loss_fn_{ nullptr };
};
TORCH_MODULE(YOLOv8Template);

#endif // TORCH_YOLO_TEMPLATE_H