#ifndef TORCH_DEEPLABV3_H
#define TORCH_DEEPLABV3_H


#include <torch/torch.h>
#include "torch_resnet18.h"

class ASPPConvImpl : public torch::nn::Module {
public:
    ASPPConvImpl(int64_t in_channels, int64_t out_channels, int64_t dilation) {
        conv_ = register_module("conv", torch::nn::Conv2d(
            torch::nn::Conv2dOptions(in_channels, out_channels, 3)
            .padding(dilation).dilation(dilation).bias(false)));
        bn_ = register_module("bn", torch::nn::BatchNorm2d(out_channels));
        relu_ = register_module("relu", torch::nn::ReLU());
    }

    torch::Tensor forward(torch::Tensor x) {
        return relu_->forward(bn_->forward(conv_->forward(x)));
    }

private:
    torch::nn::Conv2d conv_{ nullptr };
    torch::nn::BatchNorm2d bn_{ nullptr };
    torch::nn::ReLU relu_{ nullptr };
};
TORCH_MODULE(ASPPConv);

class ASPPPoolingImpl : public torch::nn::Module {
public:
    ASPPPoolingImpl(int64_t in_channels, int64_t out_channels) {
        conv_ = register_module("conv", torch::nn::Conv2d(
            torch::nn::Conv2dOptions(in_channels, out_channels, 1).bias(false)));
        bn_ = register_module("bn", torch::nn::BatchNorm2d(out_channels));
        relu_ = register_module("relu", torch::nn::ReLU());
    }

    torch::Tensor forward(torch::Tensor x) {
        auto size = x.sizes();
        auto out = torch::adaptive_avg_pool2d(x, { 1, 1 });
        out = relu_->forward(bn_->forward(conv_->forward(out)));
        return torch::nn::functional::interpolate(out,
            torch::nn::functional::InterpolateFuncOptions()
            .size(std::vector<int64_t>{size[2], size[3]})
            .mode(torch::kBilinear).align_corners(false));
    }

private:
    torch::nn::Conv2d conv_{ nullptr };
    torch::nn::BatchNorm2d bn_{ nullptr };
    torch::nn::ReLU relu_{ nullptr };
};
TORCH_MODULE(ASPPPooling);

class ASPPImpl : public torch::nn::Module {
public:
    ASPPImpl(int64_t in_channels, const std::vector<int64_t>& atrous_rates) {
        int64_t out_channels = 256;

        conv1x1_ = register_module("conv1x1",
            torch::nn::Sequential(
                torch::nn::Conv2d(torch::nn::Conv2dOptions(in_channels, out_channels, 1).bias(false)),
                torch::nn::BatchNorm2d(out_channels),
                torch::nn::ReLU()
            ));

        for (size_t i = 0; i < atrous_rates.size(); ++i) {
            aspp_convs_.push_back(register_module(
                "aspp_conv" + std::to_string(i),
                ASPPConv(in_channels, out_channels, atrous_rates[i])));
        }

        img_pool_ = register_module("img_pool", ASPPPooling(in_channels, out_channels));

        project_ = register_module("project", torch::nn::Sequential(
            torch::nn::Conv2d(torch::nn::Conv2dOptions(out_channels * (2 + atrous_rates.size()), out_channels, 1).bias(false)),
            torch::nn::BatchNorm2d(out_channels),
            torch::nn::ReLU(),
            torch::nn::Dropout(0.5)
        ));
    }

    torch::Tensor forward(torch::Tensor x) {
        std::vector<torch::Tensor> res;
        res.push_back(conv1x1_->forward(x));
        for (auto& conv : aspp_convs_) {
            res.push_back(conv->forward(x));
        }
        res.push_back(img_pool_->forward(x));
        auto out = torch::cat(res, 1);
        return project_->forward(out);
    }

private:
    torch::nn::Sequential conv1x1_{ nullptr };
    std::vector<ASPPConv> aspp_convs_;
    ASPPPooling img_pool_{ nullptr };
    torch::nn::Sequential project_{ nullptr };
};
TORCH_MODULE(ASPP);

class DeepLabV3Impl : public torch::nn::Module {
public:
    DeepLabV3Impl(int64_t num_classes, int64_t backbone_layers = 50) {

        backbone_ = register_module("backbone", ResNet18(1000));

        int64_t in_channels = 512;

        aspp_ = register_module("aspp", ASPP(in_channels, std::vector<int64_t>{12, 24, 36}));

        classifier_ = register_module("classifier", torch::nn::Conv2d(
            torch::nn::Conv2dOptions(256, num_classes, 1)));
    }

    torch::Tensor forward(torch::Tensor x) {
        auto input_shape = x.sizes();


        auto features = backbone_->as<ResNet18Impl>()->forward_features(x);
        TORCH_CHECK(!features.empty(), "ResNet18 forward_features returned no features");

        auto aspp_out = aspp_->forward(features.back());
        auto out = classifier_->forward(aspp_out);

        return torch::nn::functional::interpolate(out,
            torch::nn::functional::InterpolateFuncOptions()
            .size(std::vector<int64_t>{input_shape[2], input_shape[3]})
            .mode(torch::kBilinear).align_corners(false));
    }

    std::shared_ptr<torch::nn::Module> get_backbone() { return backbone_; }

private:
    std::shared_ptr<torch::nn::Module> backbone_;
    ASPP aspp_{ nullptr };
    torch::nn::Conv2d classifier_{ nullptr };
};
TORCH_MODULE(DeepLabV3);

#endif
