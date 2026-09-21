#ifndef TORCH_MOBILENETV3_H
#define TORCH_MOBILENETV3_H


#include <torch/torch.h>
#include <vector>

class HardSigmoidImpl : public torch::nn::Module {
public:
    torch::Tensor forward(torch::Tensor x) {
        return torch::nn::functional::relu6(x + 3.0) / 6.0;
    }
};
TORCH_MODULE(HardSigmoid);

class HardSwishImpl : public torch::nn::Module {
public:
    torch::Tensor forward(torch::Tensor x) {
        return x * torch::nn::functional::relu6(x + 3.0) / 6.0;
    }
};
TORCH_MODULE(HardSwish);

class SqueezeExcitationImpl : public torch::nn::Module {
public:
    SqueezeExcitationImpl(int64_t input_channels, int64_t squeeze_factor = 4) {
        int64_t squeezed_channels = input_channels / squeeze_factor;
        fc1_ = register_module("fc1", torch::nn::Conv2d(input_channels, squeezed_channels, 1));
        relu_ = register_module("relu", torch::nn::ReLU());
        fc2_ = register_module("fc2", torch::nn::Conv2d(squeezed_channels, input_channels, 1));
        scale_activation_ = register_module("scale_activation", HardSigmoid());
    }

    torch::Tensor forward(torch::Tensor x) {
        auto scale = torch::adaptive_avg_pool2d(x, { 1, 1 });
        scale = fc1_->forward(scale);
        scale = relu_->forward(scale);
        scale = fc2_->forward(scale);
        scale = scale_activation_->forward(scale);
        return x * scale;
    }
private:
    torch::nn::Conv2d fc1_{ nullptr }, fc2_{ nullptr };
    torch::nn::ReLU relu_{ nullptr };
    HardSigmoid scale_activation_{ nullptr };
};
TORCH_MODULE(SqueezeExcitation);

struct InvertedResidualConfig {
    int64_t input_channels;
    int64_t kernel;
    int64_t expanded_channels;
    int64_t out_channels;
    bool use_se;
    std::string activation;
    int64_t stride;
    int64_t dilation;
};

class InvertedResidualImpl : public torch::nn::Module {
public:
    InvertedResidualImpl(InvertedResidualConfig cnf)
        : use_res_connect_(cnf.stride == 1 && cnf.input_channels == cnf.out_channels)
    {
        block_ = register_module("block", torch::nn::Sequential());

        auto add_activation = [&](torch::nn::Sequential& seq) {
            if (cnf.activation == "RE") {
                seq->push_back(torch::nn::ReLU(torch::nn::ReLUOptions().inplace(true)));
            }
            else {
                seq->push_back(HardSwish());
            }
            };

        if (cnf.expanded_channels != cnf.input_channels) {
            block_->push_back(torch::nn::Conv2d(
                torch::nn::Conv2dOptions(cnf.input_channels, cnf.expanded_channels, 1).bias(false)));
            block_->push_back(torch::nn::BatchNorm2d(cnf.expanded_channels));
            add_activation(block_);
        }

        block_->push_back(torch::nn::Conv2d(
            torch::nn::Conv2dOptions(cnf.expanded_channels, cnf.expanded_channels, cnf.kernel)
            .stride(cnf.stride)
            .padding((cnf.kernel - 1) / 2 * cnf.dilation)
            .dilation(cnf.dilation)
            .groups(cnf.expanded_channels)
            .bias(false)
        ));
        block_->push_back(torch::nn::BatchNorm2d(cnf.expanded_channels));
        add_activation(block_);

        if (cnf.use_se) {
            block_->push_back(SqueezeExcitation(cnf.expanded_channels));
        }

        block_->push_back(torch::nn::Conv2d(
            torch::nn::Conv2dOptions(cnf.expanded_channels, cnf.out_channels, 1).bias(false)));
        block_->push_back(torch::nn::BatchNorm2d(cnf.out_channels));
    }

    torch::Tensor forward(torch::Tensor x) {
        if (use_res_connect_) {
            return x + block_->forward(x);
        }
        return block_->forward(x);
    }

private:
    torch::nn::Sequential block_{ nullptr };
    bool use_res_connect_;
};

TORCH_MODULE(InvertedResidual);

class MobileNetV3Impl : public torch::nn::Module {
public:
    MobileNetV3Impl(const std::string& mode = "large", int64_t output_stride = 16) {
        std::vector<InvertedResidualConfig> configs;
        const int64_t low_level_boundary_index = (mode == "large") ? 5 : 2;

        int64_t input_channel = 16;
        stem_->push_back(torch::nn::Conv2d(torch::nn::Conv2dOptions(3, input_channel, 3).stride(2).padding(1).bias(false)));
        stem_->push_back(torch::nn::BatchNorm2d(input_channel));
        stem_->push_back(HardSwish());

        if (mode == "large") {
            configs = {
                {16, 3, 16, 16, false, "RE", 1, 1},
                {16, 3, 64, 24, false, "RE", 2, 1},
                {24, 3, 72, 24, false, "RE", 1, 1},
                {24, 5, 72, 40, true, "RE", 2, 1},
                {40, 5, 120, 40, true, "RE", 1, 1},
                {40, 5, 120, 40, true, "RE", 1, 1},
                {40, 3, 240, 80, false, "HS", 2, 1},
                {80, 3, 200, 80, false, "HS", 1, 1},
                {80, 3, 184, 80, false, "HS", 1, 1},
                {80, 3, 184, 80, false, "HS", 1, 1},
                {80, 3, 480, 112, true, "HS", 1, 1},
                {112, 3, 672, 112, true, "HS", 1, 1},
                {112, 5, 672, 160, true, "HS", 2, 1},
                {160, 5, 960, 160, true, "HS", 1, 1},
                {160, 5, 960, 160, true, "HS", 1, 1},
            };
        }
        else {
            configs = {
               {16, 3, 16, 16, true, "RE", 2, 1},
               {16, 3, 72, 24, false, "RE", 2, 1},
               {24, 3, 88, 24, false, "RE", 1, 1},
               {24, 5, 96, 40, true, "HS", 2, 1},
               {40, 5, 240, 40, true, "HS", 1, 1},
               {40, 5, 240, 40, true, "HS", 1, 1},
               {40, 5, 120, 48, true, "HS", 1, 1},
               {48, 5, 144, 48, true, "HS", 1, 1},
               {48, 5, 288, 96, true, "HS", 2, 1},
               {96, 5, 576, 96, true, "HS", 1, 1},
               {96, 5, 576, 96, true, "HS", 1, 1},
            };
        }

        int64_t current_stride = 2;
        int64_t dilation = 1;

        for (size_t i = 0; i < configs.size(); ++i) {
            auto& cnf = configs[i];
            if (current_stride == output_stride && cnf.stride > 1) {
                dilation *= cnf.stride;
                cnf.stride = 1;
            }
            else {
                current_stride *= cnf.stride;
            }
            cnf.dilation = dilation;

            auto block = InvertedResidual(cnf);
            if (static_cast<int64_t>(i) <= low_level_boundary_index) {
                low_level_features_->push_back(block);
                low_level_out_channels_ = cnf.out_channels;
            } else {
                high_level_features_->push_back(block);
            }
        }

        high_level_out_channels_ = (mode == "large") ? 960 : 576;
        final_layer_->push_back(torch::nn::Conv2d(torch::nn::Conv2dOptions(configs.back().out_channels, high_level_out_channels_, 1).bias(false)));
        final_layer_->push_back(torch::nn::BatchNorm2d(high_level_out_channels_));
        final_layer_->push_back(HardSwish());

        register_module("stem", stem_);
        register_module("low_level_features", low_level_features_);
        register_module("high_level_features", high_level_features_);
        register_module("final_layer", final_layer_);
    }

    std::pair<torch::Tensor, torch::Tensor> forward_features(torch::Tensor x) {
        auto stem_feat = stem_->forward(x);
        auto low_level_feat = low_level_features_->forward(stem_feat);
        auto high_level_feat = high_level_features_->is_empty()
            ? low_level_feat
            : high_level_features_->forward(low_level_feat);
        high_level_feat = final_layer_->forward(high_level_feat);
        return { low_level_feat, high_level_feat };
    }

    int64_t get_out_channels() const { return high_level_out_channels_; }
    int64_t get_low_level_channels() const { return low_level_out_channels_; }

private:
    torch::nn::Sequential stem_;
    torch::nn::Sequential low_level_features_;
    torch::nn::Sequential high_level_features_;
    torch::nn::Sequential final_layer_;
    int64_t low_level_out_channels_ = 0;
    int64_t high_level_out_channels_ = 0;
};
TORCH_MODULE(MobileNetV3);

#endif
