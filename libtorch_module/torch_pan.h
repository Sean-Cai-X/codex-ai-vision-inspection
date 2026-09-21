#ifndef TORCH_PAN_H
#define TORCH_PAN_H


#include <torch/torch.h>
#include <vector>
#include <string>
#include <iostream>
#include <tuple>

#include "torch_nnmodule.h"


class PANImpl : public torch::nn::Module {
public:
    PANImpl(const std::vector<int64_t>& in_channels, float depth_multiple = 1.0f, float width_multiple = 1.0f) {
        TORCH_CHECK(in_channels.size() == 3, "PAN expects 3 input channels (P3, P4, P5)");

        int64_t c3 = in_channels[0];
        int64_t c4 = in_channels[1];
        int64_t c5 = in_channels[2];

        int d = scale_blocks(3, depth_multiple);



        up = register_module("up", torch::nn::Upsample(
            torch::nn::UpsampleOptions().scale_factor(std::vector<double>{2.0, 2.0}).mode(torch::kNearest)));


        c2f_p4 = safe_register_module(*this, "c2f_p4", C2f(c5 + c4, c4, d, false));

        c2f_p3 = safe_register_module(*this, "c2f_p3", C2f(c4 + c3, c3, d, false));


        down_p3 = safe_register_module(*this, "down_p3", ConvModule(c3, c3, 3, 2));
        c2f_n4 = safe_register_module(*this, "c2f_n4", C2f(c3 + c4, c4, d, false));

        down_p4 = safe_register_module(*this, "down_p4", ConvModule(c4, c4, 3, 2));
        c2f_n5 = safe_register_module(*this, "c2f_n5", C2f(c4 + c5, c5, d, false));
    }

    std::vector<torch::Tensor> forward(const std::vector<torch::Tensor>& x) {
        if (x.size() != 3) {
            throw std::runtime_error("PAN forward expects 3 inputs (P3, P4, P5)");
        }

        auto p3 = x[0];
        auto p4 = x[1];
        auto p5 = x[2];


        auto p5_up = up->forward(p5);
        if (p5_up.size(2) != p4.size(2) || p5_up.size(3) != p4.size(3)) {
        }

        auto p4_cat = torch::cat({ p5_up, p4 }, 1);
        auto p4_td = c2f_p4->forward(p4_cat);

        auto p4_up = up->forward(p4_td);
        auto p3_cat = torch::cat({ p4_up, p3 }, 1);
        auto p3_out = c2f_p3->forward(p3_cat);



        auto p3_down = down_p3->forward(p3_out);


        if (p3_down.size(2) != p4_td.size(2) || p3_down.size(3) != p4_td.size(3)) {
            std::cerr << "ERROR: Cat dimension mismatch H/W!" << std::endl;
        }
        if (p3_down.size(0) != p4_td.size(0)) {
            std::cerr << "ERROR: Cat dimension mismatch Batch!" << std::endl;
        }

        auto p4_bu_cat = torch::cat({ p3_down, p4_td }, 1);
        auto p4_out = c2f_n4->forward(p4_bu_cat);

        auto p4_down = down_p4->forward(p4_out);
        auto p5_bu_cat = torch::cat({ p4_down, p5 }, 1);
        auto p5_out = c2f_n5->forward(p5_bu_cat);

        return { p3_out, p4_out, p5_out };
    }

private:
    torch::nn::Upsample up{ nullptr };

    C2f c2f_p4{ nullptr };
    C2f c2f_p3{ nullptr };

    ConvModule down_p3{ nullptr };
    C2f c2f_n4{ nullptr };

    ConvModule down_p4{ nullptr };
    C2f c2f_n5{ nullptr };
};

TORCH_MODULE(PAN);

#endif
