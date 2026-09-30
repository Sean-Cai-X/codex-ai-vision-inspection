#include "torch_cyclic_feature_ops.h"
#include <cmath>
#include <set>
#include <limits>
namespace cxvision::cyclic {
namespace {
void Order(int k){TORCH_CHECK(k>=4&&k<=24&&k%4==0,"GROUP_FEATURE_ORDER");}
torch::Tensor Resize(const torch::Tensor& x,int h,int w) {
    TORCH_CHECK(h>0&&w>0,"GROUP_RESIZE_SIZE");
    auto ih=x.size(-2),iw=x.size(-1);
    if(h==ih&&w==iw)return x;
    TORCH_CHECK((h<=ih&&w<=iw)||(h>=ih&&w>=iw),"GROUP_RESIZE_MIXED_AXES");
    auto flat=x.reshape({-1,1,ih,iw});
    auto options=torch::nn::functional::InterpolateFuncOptions().size(std::vector<int64_t>{h,w});
    // Area averaging reduces aliasing; it is NOT an ideal rotation-equivariant filter.
    if(h<=ih&&w<=iw)options.mode(torch::kArea);
    else options.mode(torch::kBilinear).align_corners(false);
    auto y=torch::nn::functional::interpolate(flat,options);
    auto shape=x.sizes().vec();shape[shape.size()-2]=h;shape.back()=w;
    return y.reshape(shape);
}
torch::Tensor Contract(int channels,int order) {
    return torch::tensor({1,1,channels,order},torch::kInt64);
}
}
void GroupFeature::validate() const {
    Order(order);
    TORCH_CHECK(convention==1,"GROUP_FEATURE_CONVENTION");
    TORCH_CHECK(values.defined()&&values.dim()==5&&values.size(2)==order&&
        values.size(0)>0&&values.size(1)>0&&values.size(3)>0&&values.size(4)>0,
        "GROUP_FEATURE_SHAPE");
    TORCH_CHECK(values.scalar_type()==torch::kFloat32&&
        torch::isfinite(values).all().item<bool>(),"GROUP_FEATURE_FINITE_FP32");
}
GroupFeature ResizeGroup(const GroupFeature& x,int h,int w) {
    x.validate();return {Resize(x.values,h,w),x.order,x.convention};
}
GroupFeature ConcatGroup(const std::vector<GroupFeature>& xs) {
    TORCH_CHECK(!xs.empty(),"GROUP_CONCAT_EMPTY");
    xs[0].validate();const auto& first=xs[0];
    std::vector<torch::Tensor> tensors;
    for(const auto& x:xs) {
        x.validate();
        TORCH_CHECK(x.order==first.order&&x.convention==first.convention&&
            x.values.size(0)==first.values.size(0)&&
            x.values.size(3)==first.values.size(3)&&x.values.size(4)==first.values.size(4)&&
            x.values.device()==first.values.device(),"GROUP_CONCAT_CONTRACT");
        tensors.push_back(x.values);
    }
    return {torch::cat(tensors,1),first.order,first.convention};
}
SharedBatchNormImpl::SharedBatchNormImpl(int channels,int order):channels_(channels),order_(order) {
    Order(order);TORCH_CHECK(channels>0,"GROUP_NORM_CHANNELS");
    norm=register_module("norm",torch::nn::BatchNorm3d(channels));
    contract_=register_buffer("contract",Contract(channels,order));
}
GroupFeature SharedBatchNormImpl::forward(const GroupFeature& x) {
    x.validate();
    TORCH_CHECK(torch::equal(contract_.cpu(),Contract(channels_,order_))&&
        x.order==order_&&x.values.size(1)==channels_,"GROUP_NORM_CONTRACT");
    return {norm->forward(x.values),x.order,x.convention};
}
void InstanceMasks::validate() const {
    TORCH_CHECK(source==MaskSource::GroundTruth||source==MaskSource::Predicted,"MASK_SOURCE");
    TORCH_CHECK(values.defined()&&values.dim()==4&&values.size(0)>0&&
        values.size(1)>0&&values.size(2)>0&&values.size(3)>0&&
        values.scalar_type()==torch::kFloat32,"MASK_SHAPE_FP32");
    TORCH_CHECK(torch::isfinite(values).all().item<bool>()&&
        ((values>=0)&(values<=1)).all().item<bool>(),"MASK_RANGE");
    TORCH_CHECK(instance_ids.size()==static_cast<size_t>(values.size(0)),"MASK_BATCH_IDS");
    for(const auto& batch:instance_ids) {
        TORCH_CHECK(batch.size()==static_cast<size_t>(values.size(1)),"MASK_INSTANCE_IDS");
        std::set<std::string> seen;
        for(const auto& id:batch)
            TORCH_CHECK(!id.empty()&&seen.insert(id).second,"MASK_DUPLICATE_OR_EMPTY_ID");
    }
}
InstanceMasks ResizeMasks(const InstanceMasks& masks,int h,int w) {
    masks.validate();
    return {Resize(masks.values,h,w),masks.instance_ids,masks.source};
}
PooledInstances PoolInstances(const GroupFeature& f,const InstanceMasks& m,double minmass) {
    f.validate();m.validate();
    TORCH_CHECK(std::isfinite(minmass)&&minmass>=std::numeric_limits<float>::min()&&
        minmass<=std::numeric_limits<float>::max(),"MASK_MINIMUM_MASS");
    TORCH_CHECK(f.values.size(0)==m.values.size(0)&&
        f.values.size(3)==m.values.size(2)&&f.values.size(4)==m.values.size(3)&&
        f.values.device()==m.values.device(),"MASK_FEATURE_FRAME");
    const auto b=f.values.size(0),c=f.values.size(1),k=f.values.size(2);
    auto masks=m.values.flatten(2);
    auto mass=masks.sum(-1);
    auto valid=mass>=minmass;
    auto numerator=torch::bmm(masks,f.values.reshape({b,c*k,-1}).transpose(1,2));
    auto safe_mass=torch::where(valid,mass,torch::ones_like(mass));
    auto pooled=numerator/safe_mass.unsqueeze(-1);
    pooled=torch::where(valid.unsqueeze(-1),pooled,torch::zeros_like(pooled));
    TORCH_CHECK(torch::isfinite(pooled).all().item<bool>(),"MASK_POOL_OVERFLOW");
    return {pooled.reshape({b,m.values.size(1),c,k}),valid,mass,m.instance_ids,m.source};
}
}
