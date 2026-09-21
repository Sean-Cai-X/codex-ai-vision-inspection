#ifndef TORCH_NNMODULE_H
#define TORCH_NNMODULE_H


#include <torch/torch.h>
#include <vector>
#include <string>
#include <stdexcept>
#include <tuple>
#include <algorithm>
#include <cmath>
#include <cassert>
#include <iostream>


static int make_divisible(int v, int divisor = 8, float min_ratio = 0.9) {
    int new_v = std::max(divisor, static_cast<int>((v + divisor - 1) / divisor) * divisor);
    if (new_v < min_ratio * v) {
        new_v += divisor;
    }
    return new_v;
}

static std::vector<int> scale_channels(const std::vector<int64_t>& base_channels, float width_multiple) {
    std::vector<int> scaled_channels;
    for (int ch : base_channels) {
        int scaled_ch = make_divisible(static_cast<int>(ch * width_multiple));
        scaled_channels.push_back(scaled_ch);
    }
    return scaled_channels;
}

static int scale_blocks(int base_blocks, float depth_multiple) {
    return std::max(1, static_cast<int>(std::round(base_blocks * depth_multiple)));
}


struct ManualGradScaler {
private:
    float scale_;
    float growth_factor_;
    float backoff_factor_;
    int growth_interval_;
    int steps_since_growth_;
    bool found_inf_;

public:
    ManualGradScaler(
        float init_scale = 65536.0f,
        float growth_factor = 2.0f,
        float backoff_factor = 0.5f,
        int growth_interval = 2000
    ) : scale_(init_scale),
        growth_factor_(growth_factor),
        backoff_factor_(backoff_factor),
        growth_interval_(growth_interval),
        steps_since_growth_(0),
        found_inf_(false) {
    }

    torch::Tensor scale(torch::Tensor loss) {
        return loss * scale_;
    }

    bool _check_inf_nan(torch::nn::Module* model) {
        for (const auto& param : model->parameters()) {
            if (param.grad().defined()) {
                if (torch::isnan(param.grad()).any().item<bool>() ||
                    torch::isinf(param.grad()).any().item<bool>()) {
                    return true;
                }
            }
        }
        return false;
    }

    template<typename Optimizer>
    void step(Optimizer& optimizer, torch::nn::Module* model) {
        found_inf_ = _check_inf_nan(model);

        if (found_inf_) {
            optimizer.zero_grad();
            return;
        }

        float inv_scale = 1.0f / scale_;
        for (auto& param : model->parameters()) {
            if (param.grad().defined()) {
                param.grad().mul_(inv_scale);
            }
        }

        optimizer.step();
    }

    void update() {
        if (found_inf_) {
            scale_ *= backoff_factor_;
            steps_since_growth_ = 0;
        }
        else {
            steps_since_growth_++;
            if (steps_since_growth_ >= growth_interval_) {
                scale_ *= growth_factor_;
                steps_since_growth_ = 0;
            }
        }
        found_inf_ = false;
    }

    float get_scale() const { return scale_; }
};

template <typename ModuleHolder>
ModuleHolder safe_register_module(
    torch::nn::Module& parent,
    const std::string& key,
    ModuleHolder holder) {
    TORCH_CHECK(!key.empty(), "Module key cannot be empty");
    TORCH_CHECK(key.find('.') == std::string::npos,
        "Registration error: Module name contains dot: ", key);

    if (holder.is_empty()) {
        throw std::invalid_argument("Cannot register empty module holder: " + key);
    }

    parent.register_module(key, holder);
    return holder;
}


class ConvModuleImpl : public torch::nn::Module {
public:
    ConvModuleImpl(int64_t in_channels, int64_t out_channels, int64_t kernel_size,
        int64_t stride = 1, int64_t padding = -1,
        bool with_bn = true, bool with_act = true,
        const std::string& act_type = "silu")
        : conv_options_(in_channels, out_channels, kernel_size)
    {

        if (padding == -1) {
            padding = kernel_size / 2;
        }

        conv_options_.stride(stride).padding(padding).bias(!with_bn);

        conv = register_module("conv", torch::nn::Conv2d(conv_options_));

        if (with_bn) {
            bn = register_module("bn", torch::nn::BatchNorm2d(out_channels));
        }

        if (with_act) {
            if (act_type == "silu") {
                act_silu = register_module("act", torch::nn::SiLU());
            }
            else if (act_type == "relu") {
                act_relu = register_module("act", torch::nn::ReLU());
            }
            else if (act_type == "leaky_relu") {
                act_leaky_relu = register_module("act", torch::nn::LeakyReLU(torch::nn::LeakyReLUOptions().negative_slope(0.1)));
            }
            else if (act_type != "none") {
                TORCH_CHECK(false, "Unsupported activation type: ", act_type);
            }
        }
    }

    torch::Tensor forward(torch::Tensor x) {
        x = conv->forward(x);

        if (bn) {
            x = bn->forward(x);
        }

        if (act_silu) {
            x = act_silu->forward(x);
        }
        else if (act_relu) {
            x = act_relu->forward(x);
        }
        else if (act_leaky_relu) {
            x = act_leaky_relu->forward(x);
        }

        return x;
    }

    torch::nn::Conv2dOptions options() const {
        return conv_options_;
    }

private:
    torch::nn::Conv2dOptions conv_options_;

    torch::nn::Conv2d conv{ nullptr };
    torch::nn::BatchNorm2d bn{ nullptr };
    torch::nn::SiLU act_silu{ nullptr };
    torch::nn::ReLU act_relu{ nullptr };
    torch::nn::LeakyReLU act_leaky_relu{ nullptr };
};
TORCH_MODULE(ConvModule);

class BottleneckImpl : public torch::nn::Module {
public:
    BottleneckImpl(int64_t in_channels, int64_t out_channels,
        bool shortcut = true, float expansion = 1.0f,
        const std::string& act_type = "silu") {

        int64_t hidden_channels = static_cast<int64_t>(out_channels * expansion);

        cv1 = safe_register_module(*this, "cv1",
            ConvModule(in_channels, hidden_channels, 3, 1, 1, true, true, act_type));
        cv2 = safe_register_module(*this, "cv2",
            ConvModule(hidden_channels, out_channels, 3, 1, 1, true, false, act_type));

        shortcut_ = shortcut && (in_channels == out_channels);
    }

    torch::Tensor forward(torch::Tensor x) {
        auto y = cv1->forward(x);
        y = cv2->forward(y);
        if (shortcut_) {
            y = y + x;
        }
        return y;
    }

private:
    ConvModule cv1{ nullptr }, cv2{ nullptr };
    bool shortcut_;
};
TORCH_MODULE(Bottleneck);

class C2fImpl : public torch::nn::Module {
public:
    C2fImpl(int64_t in_channels, int64_t out_channels, int64_t num_blocks = 1,
        bool shortcut = true, float expansion = 0.5f) {

        int64_t hidden_channels = static_cast<int64_t>(out_channels * expansion);

        cv1 = safe_register_module(*this, "cv1",
            ConvModule(in_channels, 2 * hidden_channels, 1, 1));

        cv2 = safe_register_module(*this, "cv2",
            ConvModule((2 + num_blocks) * hidden_channels, out_channels, 1, 1));

        for (int64_t i = 0; i < num_blocks; ++i) {
            bottlenecks_.push_back(
                safe_register_module(*this, "bottleneck_" + std::to_string(i),
                    Bottleneck(hidden_channels, hidden_channels, shortcut, 1.0f))
            );
        }
    }

    torch::Tensor forward(torch::Tensor x) {
        auto y = cv1->forward(x);
        auto chunks = y.chunk(2, 1);

        std::vector<torch::Tensor> list = { chunks[0], chunks[1] };

        auto y_curr = chunks[1];
        for (auto& m : bottlenecks_) {
            y_curr = m->forward(y_curr);
            list.push_back(y_curr);
        }

        auto out = torch::cat(list, 1);
        return cv2->forward(out);
    }

private:
    ConvModule cv1{ nullptr }, cv2{ nullptr };
    std::vector<Bottleneck> bottlenecks_;
};
TORCH_MODULE(C2f);

class StemImpl : public torch::nn::Module {
public:
    StemImpl(int64_t in_channels = 3, int64_t out_channels = 64) {
        conv = safe_register_module(*this, "conv",
            ConvModule(in_channels, out_channels, 3, 2, 1));
    }

    torch::Tensor forward(torch::Tensor x) {
        return conv->forward(x);
    }

private:
    ConvModule conv{ nullptr };
};
TORCH_MODULE(Stem);

class SPPFImpl : public torch::nn::Module {
public:
    SPPFImpl(int64_t c1, int64_t c2, int64_t k = 5) {
        int64_t c_ = c1 / 2;

        cv1 = safe_register_module(*this, "cv1", ConvModule(c1, c_, 1, 1));
        cv2 = safe_register_module(*this, "cv2", ConvModule(c_ * 4, c2, 1, 1));

        m = register_module("m", torch::nn::MaxPool2d(
            torch::nn::MaxPool2dOptions(k).stride(1).padding(k / 2)));
    }

    torch::Tensor forward(torch::Tensor x) {
        x = cv1->forward(x);
        auto y1 = m->forward(x);
        auto y2 = m->forward(y1);
        auto y3 = m->forward(y2);

        return cv2->forward(torch::cat({ x, y1, y2, y3 }, 1));
    }

private:
    ConvModule cv1{ nullptr }, cv2{ nullptr };
    torch::nn::MaxPool2d m{ nullptr };
};
TORCH_MODULE(SPPF);

#endif