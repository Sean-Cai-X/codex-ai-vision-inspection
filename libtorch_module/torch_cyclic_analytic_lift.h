#pragma once
#include "torch_cyclic_group_ops.h"

namespace cxvision::cyclic {
// Research-only alternative to resampling a small pixel kernel.
// Angular orders 0,1,2 with a smooth radial envelope. Parameters are shared
// across all orientations; each kernel is evaluated analytically on the lattice.
// This does not remove interpolation/aliasing in an input image or feature map.
struct AnalyticLiftImpl : torch::nn::Module {
    AnalyticLiftImpl(int in_channels,int out_channels,int order,
                     int kernel_size=9,double sigma=1.4);
    torch::Tensor kernel(int step);
    torch::Tensor forward(const torch::Tensor& input);
    torch::Tensor coefficients; // [Cout,Cin,6]
private:
    int in_,out_,order_,size_;
    double sigma_;
    torch::Tensor contract_, basis_;
    void validate() const;
};
TORCH_MODULE(AnalyticLift);
}
