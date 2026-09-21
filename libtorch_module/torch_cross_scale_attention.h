#ifndef TORCH_CROSS_SCALE_ATTENTION_H
#define TORCH_CROSS_SCALE_ATTENTION_H


#include <torch/torch.h>
#include "torch_nnmodule.h"

class CrossScaleAttentionImpl : public torch::nn::Module {
public:
    CrossScaleAttentionImpl(int64_t in_channels_q, int64_t in_channels_kv, int64_t reduction = 2) {

        int64_t inter_channels = in_channels_q / reduction;
        if (inter_channels < 16) inter_channels = 16;

        conv_query = register_module("conv_query",
            torch::nn::Conv2d(torch::nn::Conv2dOptions(in_channels_q, inter_channels, 1).bias(false)));

        conv_key = register_module("conv_key",
            torch::nn::Conv2d(torch::nn::Conv2dOptions(in_channels_kv, inter_channels, 1).bias(false)));

        conv_value = register_module("conv_value",
            torch::nn::Conv2d(torch::nn::Conv2dOptions(in_channels_kv, in_channels_q, 1).bias(false)));

        conv_out = register_module("conv_out",
            torch::nn::Conv2d(torch::nn::Conv2dOptions(in_channels_q, in_channels_q, 1).bias(false)));

        torch::nn::init::constant_(conv_out->weight, 0);

        scale_ = 1.0f / std::sqrt(static_cast<float>(inter_channels));
    }

    torch::Tensor forward(torch::Tensor x_q, torch::Tensor x_kv) {
        int64_t batch_size = x_q.size(0);

        auto query = conv_query->forward(x_q);
        int64_t h3 = query.size(2);
        int64_t w3 = query.size(3);
        int64_t n3 = h3 * w3;
        query = query.view({ batch_size, -1, n3 }).permute({ 0, 2, 1 });

        auto key = conv_key->forward(x_kv);
        int64_t n4 = key.size(2) * key.size(3);
        key = key.view({ batch_size, -1, n4 });

        auto value = conv_value->forward(x_kv);
        value = value.view({ batch_size, -1, n4 }).permute({ 0, 2, 1 });

        auto attn = torch::bmm(query, key);
        attn = attn * scale_;
        attn = torch::softmax(attn, -1);

        auto out = torch::bmm(attn, value);

        out = out.permute({ 0, 2, 1 }).view({ batch_size, -1, h3, w3 });

        out = conv_out->forward(out);

        return x_q + out;
    }

private:
    torch::nn::Conv2d conv_query{ nullptr };
    torch::nn::Conv2d conv_key{ nullptr };
    torch::nn::Conv2d conv_value{ nullptr };
    torch::nn::Conv2d conv_out{ nullptr };
    float scale_;
};

TORCH_MODULE(CrossScaleAttention);

#endif