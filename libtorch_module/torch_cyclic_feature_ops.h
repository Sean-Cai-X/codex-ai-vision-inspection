#pragma once
#include "torch_cyclic_group_ops.h"
#include <string>
#include <vector>
namespace cxvision::cyclic {
struct GroupFeature {
    torch::Tensor values; // [B,C,K,H,W], FP32 research contract
    int order=0;
    int convention=1; // 1 = displayed CCW, bin zero unrotated
    void validate() const;
};
GroupFeature ResizeGroup(const GroupFeature& input,int height,int width);
GroupFeature ConcatGroup(const std::vector<GroupFeature>& inputs);
struct SharedBatchNormImpl : torch::nn::Module {
    SharedBatchNormImpl(int channels,int order);
    GroupFeature forward(const GroupFeature& input);
    torch::nn::BatchNorm3d norm{nullptr};
private:
    int channels_,order_;
    torch::Tensor contract_;
};
TORCH_MODULE(SharedBatchNorm);
enum class MaskSource { GroundTruth=0, Predicted=1 };
struct InstanceMasks {
    torch::Tensor values; // [B,N,H,W], finite soft masks in [0,1]
    std::vector<std::vector<std::string>> instance_ids;
    MaskSource source=MaskSource::Predicted;
    void validate() const;
};
// Explicit resize only: caller must ensure same image frame/ROI beforehand.
InstanceMasks ResizeMasks(const InstanceMasks& masks,int height,int width);
struct PooledInstances {
    torch::Tensor values; // [B,N,C,K]
    torch::Tensor valid;  // [B,N], false for insufficient mask mass
    torch::Tensor mass;   // sum on the FEATURE grid, not physical area
    std::vector<std::vector<std::string>> instance_ids;
    MaskSource source;
};
PooledInstances PoolInstances(const GroupFeature& features,const InstanceMasks& masks,
                              double minimum_mass=1e-6);
}
