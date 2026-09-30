#pragma once
#include "torch_cyclic_analytic_lift.h"
namespace cxvision::cyclic {
// Experimental group-to-group convolution. Reuses the exact analytic basis
// implementation used by Lift; no second kernel generator or interpolation.
struct AnalyticGroupImpl : torch::nn::Module {
    AnalyticGroupImpl(int in_channels,int out_channels,int order,
                      int kernel_size=9,double sigma=1.4);
    torch::Tensor kernel(int step);
    torch::Tensor forward(const torch::Tensor& input);
    AnalyticLift bank{nullptr}; // coefficients [Cout,Cin*K,6], shared across g_out
private:
    int in_,out_,order_,size_;
};
TORCH_MODULE(AnalyticGroup);
}
