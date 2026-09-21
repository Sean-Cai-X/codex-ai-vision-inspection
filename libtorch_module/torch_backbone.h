#ifndef TORCH_BACKBONE_H
#define TORCH_BACKBONE_H


#include <torch/torch.h>
#include <vector>
#include <string>
#include "torch_nnmodule.h"


class YOLOv8BackboneImpl : public torch::nn::Module {
public:
    YOLOv8BackboneImpl(const std::vector<int64_t>& base_channels, float depth_multiple, float width_multiple) {

        channels_ = scale_channels(base_channels, width_multiple);

        int d1 = scale_blocks(3, depth_multiple);
        int d2 = scale_blocks(6, depth_multiple);
        int d3 = scale_blocks(6, depth_multiple);
        int d4 = scale_blocks(3, depth_multiple);

        stem = safe_register_module(*this, "stem", Stem(3, channels_[0]));

        conv1 = safe_register_module(*this, "conv1", ConvModule(channels_[0], channels_[1], 3, 2, 1));
        c2f1 = safe_register_module(*this, "c2f1", C2f(channels_[1], channels_[1], d1, true));

        conv2 = safe_register_module(*this, "conv2", ConvModule(channels_[1], channels_[2], 3, 2, 1));
        c2f2 = safe_register_module(*this, "c2f2", C2f(channels_[2], channels_[2], d2, true));

        conv3 = safe_register_module(*this, "conv3", ConvModule(channels_[2], channels_[3], 3, 2, 1));
        c2f3 = safe_register_module(*this, "c2f3", C2f(channels_[3], channels_[3], d3, true));

        conv4 = safe_register_module(*this, "conv4", ConvModule(channels_[3], channels_[4], 3, 2, 1));
        c2f4 = safe_register_module(*this, "c2f4", C2f(channels_[4], channels_[4], d4, true));
        sppf = safe_register_module(*this, "sppf", SPPF(channels_[4], channels_[4], 5));
    }

    std::vector<torch::Tensor> forward(torch::Tensor x) {
        x = stem->forward(x);

        x = conv1->forward(x);
        x = c2f1->forward(x);

        x = conv2->forward(x);
        auto p3 = c2f2->forward(x);

        x = conv3->forward(x);
        auto p4 = c2f3->forward(x);

        x = conv4->forward(x);
        x = c2f4->forward(x);
        auto p5 = sppf->forward(x);

        return { p3, p4, p5 };
    }

    std::vector<int64_t> get_out_channels() const {
        return { channels_[2], channels_[3], channels_[4] };
    }

    const Stem& get_stem() const { return stem; }
private:
    std::vector<int> channels_;

    Stem stem{ nullptr };

    ConvModule conv1{ nullptr };
    C2f c2f1{ nullptr };

    ConvModule conv2{ nullptr };
    C2f c2f2{ nullptr };

    ConvModule conv3{ nullptr };
    C2f c2f3{ nullptr };

    ConvModule conv4{ nullptr };
    C2f c2f4{ nullptr };

    SPPF sppf{ nullptr };
};

TORCH_MODULE(YOLOv8Backbone);

#endif
