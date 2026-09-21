#ifndef TORCH_DEEPLABV3_PLUS_H
#define TORCH_DEEPLABV3_PLUS_H


#include <torch/torch.h>
#include <map>
#include <string>
#include "torch_mobilenetv3.h"
#include "torch_resnet18.h"
#include "torch_DeepLabV3.h"

using DeepLabOutput = std::map<std::string, torch::Tensor>;

class DeepLabV3PlusImpl : public torch::nn::Module {
public:
    DeepLabV3PlusImpl(const std::string& backbone_name, int64_t num_classes) {

        int64_t aspp_in_channels = 0;
        int64_t low_level_channels = 0;

        if (backbone_name == "resnet18") {
            backbone_ = register_module("backbone", ResNet18(1000));
            aspp_in_channels = 512;
            low_level_channels = 128;
        } else if (backbone_name == "mobilenet_v3_large") {
            backbone_ = register_module("backbone", MobileNetV3("large"));
            aspp_in_channels = 960;
            low_level_channels = 40;
        } else {
            throw std::runtime_error("Unsupported backbone");
        }

        aspp_ = register_module("aspp", ASPP(aspp_in_channels, std::vector<int64_t>{12, 24, 36}));

        low_level_conv_ = register_module("low_level_conv", torch::nn::Sequential(
            torch::nn::Conv2d(torch::nn::Conv2dOptions(low_level_channels, 48, 1).bias(false)),
            torch::nn::BatchNorm2d(48),
            torch::nn::ReLU()
        ));

        classifier_ = register_module("classifier", torch::nn::Sequential(
            torch::nn::Conv2d(torch::nn::Conv2dOptions(304, 256, 3).padding(1).bias(false)),
            torch::nn::BatchNorm2d(256),
            torch::nn::ReLU(),
            torch::nn::Conv2d(torch::nn::Conv2dOptions(256, num_classes, 1))
        ));
    }

    DeepLabOutput forward(torch::Tensor x) {
        auto input_shape = x.sizes();

        torch::Tensor low_level_feat, high_level_feat;

        if (auto m = std::dynamic_pointer_cast<MobileNetV3Impl>(backbone_)) {
            auto feats = m->forward_features(x);
            low_level_feat = feats.first;
            high_level_feat = feats.second;
        } else if (auto r = std::dynamic_pointer_cast<ResNet18Impl>(backbone_)) {
            auto feats = r->forward_features(x);
            TORCH_CHECK(feats.size() >= 3, "ResNet18 forward_features must return at least 3 feature maps");
            low_level_feat = feats.front();
            high_level_feat = feats.back();
        }

        auto aspp_out = aspp_->forward(high_level_feat);

        aspp_out = torch::nn::functional::interpolate(aspp_out,
            torch::nn::functional::InterpolateFuncOptions()
            .size(std::vector<int64_t>{low_level_feat.size(2), low_level_feat.size(3)})
            .mode(torch::kBilinear).align_corners(false));

        auto low_level_proj = low_level_conv_->forward(low_level_feat);

        if (aspp_out.size(2) != low_level_proj.size(2)) {
             aspp_out = torch::nn::functional::interpolate(aspp_out,
                torch::nn::functional::InterpolateFuncOptions()
                .size(std::vector<int64_t>{low_level_proj.size(2), low_level_proj.size(3)})
                .mode(torch::kBilinear).align_corners(false));
        }

        auto dec_out = torch::cat({aspp_out, low_level_proj}, 1);
        auto out = classifier_->forward(dec_out);

        out = torch::nn::functional::interpolate(out,
            torch::nn::functional::InterpolateFuncOptions()
            .size(std::vector<int64_t>{input_shape[2], input_shape[3]})
            .mode(torch::kBilinear).align_corners(false));

        DeepLabOutput output;
        output["out"] = out;
        return output;
    }

private:
    std::shared_ptr<torch::nn::Module> backbone_;
    ASPP aspp_{nullptr};
    torch::nn::Sequential low_level_conv_{nullptr};
    torch::nn::Sequential classifier_{nullptr};
};
TORCH_MODULE(DeepLabV3Plus);

#endif
