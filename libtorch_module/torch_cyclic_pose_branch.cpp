#include "torch_cyclic_pose_branch.h"
#include <cmath>
namespace cxvision::cyclic {
namespace {
torch::Tensor Contract(int c,int k,int s,double t) {
    return torch::tensor({1.0,1.0,static_cast<double>(c),static_cast<double>(k),
        static_cast<double>(s),t},torch::kFloat64);
}
}
AngularHeadImpl::AngularHeadImpl(int c,int k,int s,double t)
    :channels_(c),order_(k),scales_(s),temperature_(t) {
    TORCH_CHECK(c>0&&k>=4&&k<=24&&k%4==0&&s>0&&s<=3&&
        std::isfinite(t)&&t>0,"ANGULAR_HEAD_OPTIONS");
    projection=register_parameter("projection",torch::randn({s,c})/std::sqrt(double(c)));
    scale_logits=register_parameter("scale_logits",torch::zeros({s}));
    contract_=register_buffer("contract",Contract(c,k,s,t));
}
InstancePoseOutput AngularHeadImpl::forward(const std::vector<PooledInstances>& ps,
        const torch::Tensor& q,const torch::Tensor& requested) {
    TORCH_CHECK(torch::equal(contract_.cpu(),Contract(channels_,order_,scales_,temperature_)),
                "ANGULAR_HEAD_ARCHIVE_CONTRACT");
    TORCH_CHECK(ps.size()==static_cast<size_t>(scales_),"ANGULAR_HEAD_SCALE_COUNT");
    const auto& first=ps.front();
    TORCH_CHECK(first.values.defined()&&first.values.dim()==4,"ANGULAR_HEAD_POOL_SHAPE");
    const auto b=first.values.size(0),n=first.values.size(1);
    TORCH_CHECK(b>0&&n>0,"ANGULAR_HEAD_EMPTY_INSTANCES");
    std::vector<torch::Tensor> logits,masses,valids;
    for(int s=0;s<scales_;++s) {
        const auto& p=ps[s];
        TORCH_CHECK(p.values.sizes()==torch::IntArrayRef({b,n,channels_,order_})&&
            p.values.scalar_type()==torch::kFloat32&&p.values.device()==projection.device()&&
            torch::isfinite(p.values).all().item<bool>(),"ANGULAR_HEAD_POOL_CONTRACT");
        TORCH_CHECK(p.valid.sizes()==torch::IntArrayRef({b,n})&&p.valid.scalar_type()==torch::kBool&&
            p.mass.sizes()==p.valid.sizes()&&p.mass.scalar_type()==torch::kFloat32&&
            p.valid.device()==p.values.device()&&p.mass.device()==p.values.device()&&
            torch::isfinite(p.mass).all().item<bool>()&&(p.mass>=0).all().item<bool>()&&
            ((~p.valid)|(p.mass>0)).all().item<bool>(),"ANGULAR_HEAD_MASK_CONTRACT");
        TORCH_CHECK(p.instance_ids==first.instance_ids&&p.source==first.source,
                    "ANGULAR_HEAD_INSTANCE_ALIGNMENT");
        // Reuse the mask identity/source validator without constructing spatial masks.
        InstanceMasks identity{p.mass.gt(0).to(torch::kFloat32).unsqueeze(-1).unsqueeze(-1),
                               p.instance_ids,p.source};
        identity.validate();
        auto values=torch::where(p.valid.unsqueeze(-1).unsqueeze(-1),
                                 p.values,torch::zeros_like(p.values));
        logits.push_back((values*projection[s].view({1,1,channels_,1})).sum(2));
        masses.push_back(p.mass);valids.push_back(p.valid);
    }
    auto weights=torch::softmax(scale_logits,0);
    auto valid=torch::stack(valids,-1).all(-1);
    auto fused=(torch::stack(logits,-1)*weights).sum(-1);
    fused=torch::where(valid.unsqueeze(-1),fused,torch::zeros_like(fused));
    TORCH_CHECK(requested.defined()&&requested.sizes()==valid.sizes()&&
        requested.scalar_type()==torch::kBool&&requested.device()==valid.device(),
        "ANGULAR_HEAD_REQUESTED_CONTRACT");
    auto angles=DecodeAngles(fused,q,valid&requested,temperature_);
    constexpr double pi=3.14159265358979323846;
    auto period=torch::where(q>0,2*pi/q.clamp_min(1).to(torch::kFloat32),
                             torch::zeros_like(fused.select(-1,0)));
    return {fused,angles,torch::arange(order_,fused.options())*(2*pi/order_),q.clone(),
        period,valid,torch::stack(masses,-1),weights,first.instance_ids,first.source};
}
PoseBranchImpl::PoseBranchImpl(int in,int c,int k):input_channels_(in),channels_(c),order_(k) {
    TORCH_CHECK(in>0&&in<=4&&c>0&&c<=32,"POSE_BRANCH_CHANNELS");
    lift=register_module("lift",AnalyticLift(in,c,k));
    group1=register_module("group1",AnalyticGroup(c,c,k));
    group2=register_module("group2",AnalyticGroup(c,c,k));
    norm0=register_module("norm0",SharedBatchNorm(c,k));
    norm1=register_module("norm1",SharedBatchNorm(c,k));
    norm2=register_module("norm2",SharedBatchNorm(c,k));
    head=register_module("head",AngularHead(c,k,3));
}
InstancePoseOutput PoseBranchImpl::forward(const torch::Tensor& image,const InstanceMasks& masks,
        const torch::Tensor& q,const torch::Tensor& requested) {
    masks.validate();
    TORCH_CHECK(image.defined()&&image.dim()==4&&image.size(0)>0&&
        image.size(1)==input_channels_&&image.size(2)==image.size(3)&&
        image.size(2)>=16&&image.size(2)%4==0&&image.scalar_type()==torch::kFloat32&&
        torch::isfinite(image).all().item<bool>(),"POSE_BRANCH_IMAGE");
    TORCH_CHECK(image.size(0)==masks.values.size(0)&&image.size(2)==masks.values.size(2)&&
        image.size(3)==masks.values.size(3)&&image.device()==masks.values.device(),
        "POSE_BRANCH_MASK_FRAME");
    const auto size=image.size(2);
    auto f0=norm0->forward({lift->forward(image),order_});f0.values=torch::silu(f0.values);
    auto f1=ResizeGroup(f0,size/2,size/2);
    f1=norm1->forward({group1->forward(f1.values),order_});f1.values=torch::silu(f1.values);
    auto f2=ResizeGroup(f1,size/4,size/4);
    f2=norm2->forward({group2->forward(f2.values),order_});f2.values=torch::silu(f2.values);
    return head->forward({PoolInstances(f0,masks),
        PoolInstances(f1,ResizeMasks(masks,size/2,size/2)),
        PoolInstances(f2,ResizeMasks(masks,size/4,size/4))},q,requested);
}
}
