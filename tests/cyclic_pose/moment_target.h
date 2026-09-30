#pragma once
#include <torch/torch.h>
#include <cmath>

namespace cxvision::cyclic::experiment {
// First-harmonic probability target. Its q-th moment is rho*exp(i*q*theta)
// on a uniform K-bin grid when K > 2*q; unlike exp(kappa*cos), no higher
// harmonics alias into that moment. q0 gets uniform probabilities.
inline torch::Tensor MomentTarget(const torch::Tensor& theta,
                                 const torch::Tensor& q, int K, double rho=0.4) {
    TORCH_CHECK(theta.scalar_type()==torch::kFloat32 && q.scalar_type()==torch::kInt64,
                "target expects FP32 theta and int64 q");
    TORCH_CHECK(theta.sizes()==q.sizes() && theta.device()==q.device() && theta.numel()>0,
                "target shape/device mismatch");
    TORCH_CHECK(torch::isfinite(theta).all().item<bool>(), "nonfinite target angle");
    TORCH_CHECK(((q==0)|(q==1)|(q==2)|(q==4)).all().item<bool>(), "unsupported symmetry");
    TORCH_CHECK(K>2*q.max().item<int64_t>() && K>=3, "insufficient angular bins");
    TORCH_CHECK(std::isfinite(rho) && rho>0 && rho<0.5, "rho must be in (0,.5)");
    auto bins=torch::arange(K,theta.options())*(2*3.14159265358979323846/K);
    auto modulation=2*rho*torch::cos(q.unsqueeze(-1)*(bins-theta.unsqueeze(-1)));
    return (1+torch::where(q.unsqueeze(-1)>0,modulation,torch::zeros_like(modulation)))/K;
}
} // namespace cxvision::cyclic::experiment
