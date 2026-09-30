#include "torch_cyclic_group_ops.h"
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>

using namespace cxvision::cyclic;
namespace {
constexpr double pi=3.14159265358979323846;
int checks=0;
void Require(bool ok,const char* message) {
    ++checks;
    if(!ok) throw std::runtime_error(message);
}
void Reject(const std::function<void()>& fn) {
    bool rejected=false;
    try { fn(); } catch(const c10::Error&) { rejected=true; }
    Require(rejected,"expected rejection");
}
double Relative(const torch::Tensor& a,const torch::Tensor& b) {
    return (a-b).square().mean().sqrt().item<double>() /
        std::max(1e-12,b.square().mean().sqrt().item<double>());
}
void GeometryAndGradient() {
    for(int k:{4,8,12,24}) {
        torch::manual_seed(20260918+k);
        LiftConv lift(2,3,k,5);
        GroupConv group(3,2,k,3);
        auto x=torch::randn({2,2,25,25});
        auto f=lift->forward(x);
        Require(f.sizes()==torch::IntArrayRef({2,3,k,25,25}),"lifting shape");
        for(int m:{0,k/4,k/2,3*k/4}) {
            auto rotated=lift->forward(RotateSpatial(x,m,k));
            double e=Relative(rotated,GroupAction(f,m,k));
            Require(e<3e-5,"quarter-turn lift equivariance");
            auto expected=GroupAction(group->forward(f),m,k);
            auto observed=group->forward(GroupAction(f,m,k));
            double eg=Relative(observed,expected);
            Require(eg<5e-5,"quarter-turn group equivariance");
            std::cout<<"METRIC K="<<k<<" step="<<m<<" lift_relative="<<e
                     <<" group_relative="<<eg<<"\n";
        }
        // Shared BN statistics/affine parameters over K; pointwise activation.
        torch::nn::BatchNorm3d bn(3);
        bn->eval();
        auto n=torch::silu(bn->forward(f));
        Require(Relative(torch::silu(bn->forward(GroupAction(f,k/4,k))),
                         GroupAction(n,k/4,k))<3e-5,"BN/activation equivariance");
        // Intermediate bins are approximate on a pixel lattice: report, not PASS.
        if(k>4) {
            auto a=lift->forward(RotateSpatial(x,1,k));
            auto b=GroupAction(f,1,k);
            std::cout<<"DIAGNOSTIC K="<<k<<" interpolated_lift_relative="
                     <<Relative(a,b)<<" status=UNQUALIFIED_INTERPOLATION\n";
        }
        auto y=group->forward(torch::silu(f));
        auto projection=torch::randn({2},torch::requires_grad());
        auto logits=(y.mean({3,4})*projection.view({1,2,1})).sum(1);
        auto result=DecodeAngles(logits,torch::ones({2},torch::kInt64),
                                 torch::ones({2},torch::kBool));
        auto target=torch::arange(k,logits.options())*(2*pi/k);
        auto target_probs=torch::softmax(2*torch::cos(target-0.7),0);
        auto loss=-(target_probs*torch::log_softmax(logits,-1)).sum(-1).mean();
        loss.backward();
        for(auto w:{lift->weight,group->weight,projection}) {
            Require(w.grad().defined() && torch::isfinite(w.grad()).all().item<bool>() &&
                    w.grad().abs().sum().item<double>()>1e-8,"nonzero finite gradient");
        }
        // A gradient step must change the loss, not only populate grad fields.
        double before=loss.item<double>();
        {
            torch::NoGradGuard guard;
            lift->weight.sub_(0.01*lift->weight.grad());
            group->weight.sub_(0.01*group->weight.grad());
            projection.sub_(0.01*projection.grad());
        }
        auto updated=group->forward(torch::silu(lift->forward(x)));
        auto updated_logits=(updated.mean({3,4})*projection.view({1,2,1})).sum(1);
        double after=(-(target_probs*torch::log_softmax(updated_logits,-1))
                         .sum(-1).mean()).item<double>();
        Require(after<before,"loss did not decrease");
    }
}
void DecodeContracts() {
    const int k=24;
    for(int q:{1,2,4}) {
        for(double angle:{-1.0,1.0,89.0,179.0,359.0}) {
            auto phi=torch::arange(k,torch::kFloat32)*(2*pi/k);
            // An analytic positive single-harmonic distribution has an exact moment.
            // A discretized sharp von Mises target does NOT promise exact sub-bin angles.
            auto logits=(1+0.8*torch::cos(q*(phi-angle*pi/180))).log().unsqueeze(0);
            auto r=DecodeAngles(logits,torch::tensor({q},torch::kInt64),
                                torch::ones({1},torch::kBool));
            double error=std::remainder(r.angle_rad.item<double>()-angle*pi/180,2*pi/q);
            Require(r.angle_valid.item<bool>() && std::abs(error)<2e-4,"symmetry/wrap decode");
            Require(std::abs(r.probabilities.sum().item<double>()-1)<1e-6,"probability sum");
            Require(r.predictive_only,"predictive boundary");
        }
    }
    auto z=torch::zeros({2,24});
    auto q=torch::tensor({0,1},torch::kInt64);
    auto valid=torch::ones({2},torch::kBool);
    auto r=DecodeAngles(z,q,valid);
    Require(!r.angle_valid.any().item<bool>(),"circle/uniform must not have angle");
    Require(r.angle_rad.eq(0).all().item<bool>(),"invalid angle sentinel");
    // Mixed-instance target semantics stay per-instance.
    auto mixed=DecodeAngles(torch::randn({2,3,24}),
        torch::tensor({0,1,2,4,0,1},torch::kInt64).reshape({2,3}),
        torch::ones({2,3},torch::kBool));
    Require(!mixed.angle_valid[0][0].item<bool>() &&
            !mixed.angle_valid[1][1].item<bool>(),"circle per-instance masking");
    auto differentiable=torch::randn({2,24},torch::requires_grad());
    auto decoded=DecodeAngles(differentiable,torch::ones({2},torch::kInt64),valid);
    auto phase_loss=(1-torch::cos(decoded.angle_rad-0.3)).mean()
                    +0.1*decoded.concentration.mean();
    phase_loss.backward();
    Require(torch::isfinite(differentiable.grad()).all().item<bool>() &&
            differentiable.grad().abs().sum().item<double>()>1e-8,"decoder gradient");
    auto invalid_logits=torch::zeros({2,24},torch::requires_grad());
    auto invalid_result=DecodeAngles(invalid_logits,torch::zeros({2},torch::kInt64),valid);
    (invalid_result.angle_rad.sum()+invalid_result.concentration.sum()).backward();
    Require(torch::isfinite(invalid_logits.grad()).all().item<bool>() &&
            invalid_logits.grad().abs().sum().item<double>()==0,"invalid angle gradient");
    Reject([&]{DecodeAngles(z,q,valid,0);});
    Reject([&]{DecodeAngles(torch::ones({2,24}),q,valid,1e-300);});
    Reject([&]{DecodeAngles(torch::zeros({0,24}),torch::zeros({0},torch::kInt64),
                            torch::zeros({0},torch::kBool));});
    Reject([&]{DecodeAngles(z,q,valid,1,0);});
    Reject([&]{DecodeAngles(z,torch::full({2},3,torch::kInt64),valid);});
    Reject([&]{DecodeAngles(torch::zeros({2,8}),torch::full({2},4,torch::kInt64),valid);});
    Reject([&]{DecodeAngles(z,q.to(torch::kFloat32),valid);});
    Reject([&]{DecodeAngles(z,torch::zeros({1},torch::kInt64),valid);});
    Reject([&]{DecodeAngles(torch::full({2,24},std::numeric_limits<float>::quiet_NaN()),q,valid);});
}
void SerializationAndGuards() {
    LiftConv a(1,2,12),b(1,2,12),wrong(1,2,8);
    auto x=torch::randn({1,1,17,17});
    auto y=a->forward(x);
    std::stringstream bytes;
    torch::serialize::OutputArchive out;
    a->save(out);out.save_to(bytes);
    torch::serialize::InputArchive in;
    in.load_from(bytes);b->load(in);
    Require(torch::equal(y,b->forward(x)),"serialization roundtrip");
    bytes.clear();bytes.seekg(0);
    torch::serialize::InputArchive mismatch;
    mismatch.load_from(bytes);wrong->load(mismatch);
    Reject([&]{wrong->forward(x);});
    GroupConv ga(2,2,12),gb(2,2,12);
    auto gy=ga->forward(y);
    std::stringstream group_bytes;
    torch::serialize::OutputArchive gout;
    ga->save(gout);gout.save_to(group_bytes);
    torch::serialize::InputArchive gin;
    gin.load_from(group_bytes);gb->load(gin);
    Require(torch::equal(gy,gb->forward(y)),"group serialization roundtrip");
    Reject([]{LiftConv bad(1,1,3);});
    Reject([]{GroupConv bad(1,1,12,2);});
    Reject([&]{a->forward(torch::zeros({1,2,17,17}));});
    Reject([&]{ga->forward(torch::zeros({1,2,8,17,17}));});
    Reject([]{RotateSpatial(torch::zeros({3,5}),1,12);});
    // Direction convention independently checked with a single off-centre pixel.
    auto marker=torch::zeros({5,5});marker[2][4]=1;
    Require(RotateSpatial(marker,1,4)[0][2].item<float>()==1,"CCW convention");
}
void CudaParity() {
    if(!torch::cuda::is_available()) {
        std::cout<<"CUDA_PARITY=SKIPPED_NO_DEVICE\n";return;
    }
    LiftConv lift(1,2,12);GroupConv group(2,2,12);
    auto x=torch::randn({1,1,17,17});
    auto cpu=group->forward(lift->forward(x));
    lift->to(torch::kCUDA);group->to(torch::kCUDA);
    auto gpu=group->forward(lift->forward(x.to(torch::kCUDA)));
    Require(Relative(gpu.cpu(),cpu)<1e-4,"CPU/CUDA forward parity");
    gpu.square().mean().backward();
    Require(torch::isfinite(lift->weight.grad()).all().item<bool>(),"CUDA gradient finite");
    std::cout<<"CUDA_PARITY=PASS\n";
}
}
int main() {
    try {
        torch::set_num_threads(1);
        GeometryAndGradient();DecodeContracts();SerializationAndGuards();CudaParity();
        std::cout<<"CONTRACT_CHECKS="<<checks<<" RESULT=PASS production_eligible=false\n";
        return 0;
    } catch(const std::exception& e) {
        std::cerr<<"RESULT=FAIL "<<e.what()<<"\n";return 1;
    }
}
