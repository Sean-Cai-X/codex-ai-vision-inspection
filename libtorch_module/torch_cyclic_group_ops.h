#pragma once
#include <torch/torch.h>

namespace cxvision::cyclic {
// Experimental V2A operators. Positive angles are torch::rot90 counterclockwise
// in displayed image coordinates. Features: [B,C,K,H,W], zero bin = unrotated.
// Only quarter turns are exact lattice symmetries; other bins use interpolation.
torch::Tensor RotateSpatial(const torch::Tensor& input, int step, int order);
torch::Tensor GroupAction(const torch::Tensor& input, int step, int order);

struct LiftConvImpl : torch::nn::Module {
    LiftConvImpl(int in_channels, int out_channels, int order, int kernel_size = 3);
    torch::Tensor forward(const torch::Tensor& input);
    torch::Tensor weight;
private:
    int in_, out_, order_, kernel_;
    torch::Tensor contract_;
};
TORCH_MODULE(LiftConv);

struct GroupConvImpl : torch::nn::Module {
    GroupConvImpl(int in_channels, int out_channels, int order, int kernel_size = 3);
    torch::Tensor forward(const torch::Tensor& input);
    // One shared relative-orientation kernel, not K independent output kernels.
    torch::Tensor weight; // [Cout,Cin,K,k,k]
private:
    int in_, out_, order_, kernel_;
    torch::Tensor contract_;
};
TORCH_MODULE(GroupConv);

struct AngularResult {
    torch::Tensor probabilities, angle_rad, concentration, entropy, peak_margin;
    torch::Tensor angle_valid;
    // Concentration is NOT calibrated uncertainty or a measurement confidence.
    static constexpr bool predictive_only = true;
};
// logits [...,K]; harmonic_order/valid have shape [...]. q=0 means no angle.
// q=1/2/4 correspond to 360/180/90 degree symmetry. K must exceed 2*q.
AngularResult DecodeAngles(const torch::Tensor& logits,
    const torch::Tensor& harmonic_order, const torch::Tensor& valid,
    double temperature = 1.0, double minimum_concentration = 1e-6);
} // namespace cxvision::cyclic
