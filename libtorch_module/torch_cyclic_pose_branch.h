#pragma once
#include "torch_cyclic_analytic_group.h"
#include "torch_cyclic_feature_ops.h"
namespace cxvision::cyclic {
struct InstancePoseOutput {
    torch::Tensor logits; // [B,N,K], no per-orientation learned bias
    AngularResult angles;
    torch::Tensor bins_rad, harmonic_order, period_rad;
    torch::Tensor mask_valid; // [B,N]; all pyramid scales must have support
    torch::Tensor scale_mass; // [B,N,S] on each feature grid
    torch::Tensor scale_weights; // [S], shared across instances and orientations
    std::vector<std::vector<std::string>> instance_ids;
    MaskSource mask_source;
    static constexpr bool predictive_only=true;
    static constexpr const char* source="experimental_cyclic_pose_v2a";
};
// Shared channel projection per scale; scale fusion is also orientation-shared.
struct AngularHeadImpl : torch::nn::Module {
    AngularHeadImpl(int channels,int order,int scales=3,double temperature=1.0);
    InstancePoseOutput forward(const std::vector<PooledInstances>& pooled,
        const torch::Tensor& harmonic_order,const torch::Tensor& angle_requested);
    torch::Tensor projection, scale_logits;
private:
    int channels_,order_,scales_;
    double temperature_;
    torch::Tensor contract_;
};
TORCH_MODULE(AngularHead);

// Research branch only: analytic Lift + two Group blocks, SharedBN + SiLU,
// relative-resolution pyramid 1,1/2,1/4. Not the YOLO P3/P4/P5 pyramid.
struct PoseBranchImpl : torch::nn::Module {
    PoseBranchImpl(int input_channels=1,int channels=2,int order=12);
    InstancePoseOutput forward(const torch::Tensor& image,const InstanceMasks& masks,
        const torch::Tensor& harmonic_order,const torch::Tensor& angle_requested);
    AnalyticLift lift{nullptr};
    AnalyticGroup group1{nullptr},group2{nullptr};
    SharedBatchNorm norm0{nullptr},norm1{nullptr},norm2{nullptr};
    AngularHead head{nullptr};
private:
    int input_channels_,channels_,order_;
};
TORCH_MODULE(PoseBranch);
}
