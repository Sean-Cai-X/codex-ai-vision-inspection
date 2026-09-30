#include "torch_cyclic_group_ops.h"
#include <cmath>

namespace cxvision::cyclic {
namespace {
constexpr double pi = 3.14159265358979323846;
void CheckOrder(int order) {
    TORCH_CHECK(order >= 4 && order <= 24 && order % 4 == 0,
                "CYCLIC_INVALID_GROUP_ORDER");
}
void CheckOptions(int in, int out, int order, int kernel) {
    CheckOrder(order);
    TORCH_CHECK(in > 0 && out > 0 && kernel > 0 && kernel <= 15 && kernel % 2 == 1,
                "CYCLIC_INVALID_CONV_OPTIONS");
}
torch::Tensor Contract(int in, int out, int order, int kernel) {
    // schema, orientation convention, Cin, Cout, K, kernel
    return torch::tensor({1, 1, in, out, order, kernel}, torch::kInt64);
}
void CheckContract(const torch::Tensor& contract, int in, int out, int order, int kernel) {
    TORCH_CHECK(torch::equal(contract.cpu(), Contract(in,out,order,kernel)),
                "CYCLIC_SERIALIZED_CONTRACT_MISMATCH");
}
void CheckInput(const torch::Tensor& x, const torch::Tensor& w, int rank, int channels) {
    TORCH_CHECK(x.defined() && x.dim() == rank && x.size(1) == channels,
                "CYCLIC_INPUT_SHAPE");
    TORCH_CHECK(x.scalar_type() == w.scalar_type() && x.device() == w.device(),
                "CYCLIC_INPUT_TYPE_OR_DEVICE");
    TORCH_CHECK(x.size(0) > 0 && x.size(-1) > 0 && x.size(-2) > 0,
                "CYCLIC_EMPTY_INPUT");
}
}

torch::Tensor RotateSpatial(const torch::Tensor& x, int step, int order) {
    CheckOrder(order);
    TORCH_CHECK(x.defined() && x.dim() >= 2 && x.is_floating_point(),
                "CYCLIC_ROTATION_INPUT");
    TORCH_CHECK(x.size(-1) == x.size(-2) && x.size(-1) > 0,
                "CYCLIC_ROTATION_REQUIRES_SQUARE");
    step = (step % order + order) % order;
    if ((step * 4) % order == 0)
        return torch::rot90(x, step * 4 / order, {-2,-1});
    const double a = 2.0*pi*step/order;
    // Output-to-input grid for a displayed CCW rotation (image y points down).
    auto theta = torch::tensor({std::cos(a),-std::sin(a),0.0,
                                std::sin(a), std::cos(a),0.0},
                                torch::kFloat64).to(x.options()).reshape({1,2,3});
    const auto n = x.size(-1);
    auto flat = x.reshape({-1,1,n,n});
    theta = theta.expand({flat.size(0),2,3});
    auto grid = torch::nn::functional::affine_grid(theta, flat.sizes(), false);
    auto rotated = torch::nn::functional::grid_sample(flat, grid,
        torch::nn::functional::GridSampleFuncOptions()
            .mode(torch::kBilinear).padding_mode(torch::kZeros).align_corners(false));
    return rotated.reshape(x.sizes());
}

torch::Tensor GroupAction(const torch::Tensor& x, int step, int order) {
    TORCH_CHECK(x.dim() == 5 && x.size(2) == order, "CYCLIC_GROUP_SHAPE");
    return torch::roll(RotateSpatial(x,step,order), {step}, {2});
}

LiftConvImpl::LiftConvImpl(int in, int out, int order, int kernel)
    : in_(in),out_(out),order_(order),kernel_(kernel) {
    CheckOptions(in,out,order,kernel);
    weight = register_parameter("weight", torch::randn({out,in,kernel,kernel}) /
        std::sqrt(static_cast<double>(in*kernel*kernel)));
    contract_ = register_buffer("contract", Contract(in,out,order,kernel));
}
torch::Tensor LiftConvImpl::forward(const torch::Tensor& x) {
    CheckContract(contract_,in_,out_,order_,kernel_);
    CheckInput(x,weight,4,in_);
    std::vector<torch::Tensor> result;
    for(int g=0;g<order_;++g)
        result.push_back(torch::nn::functional::conv2d(x,RotateSpatial(weight,g,order_),
            torch::nn::functional::Conv2dFuncOptions().padding(kernel_/2)));
    return torch::stack(result,2);
}

GroupConvImpl::GroupConvImpl(int in, int out, int order, int kernel)
    : in_(in),out_(out),order_(order),kernel_(kernel) {
    CheckOptions(in,out,order,kernel);
    weight = register_parameter("weight", torch::randn({out,in,order,kernel,kernel}) /
        std::sqrt(static_cast<double>(in*order*kernel*kernel)));
    contract_ = register_buffer("contract", Contract(in,out,order,kernel));
}
torch::Tensor GroupConvImpl::forward(const torch::Tensor& x) {
    CheckContract(contract_,in_,out_,order_,kernel_);
    CheckInput(x,weight,5,in_);
    TORCH_CHECK(x.size(2) == order_, "CYCLIC_GROUP_SHAPE");
    auto flat = x.reshape({x.size(0),in_*order_,x.size(3),x.size(4)});
    std::vector<torch::Tensor> result;
    for(int g=0;g<order_;++g) {
        // roll(+g) selects relative orientation (h-g) for input channel h.
        auto kernel = torch::roll(RotateSpatial(weight,g,order_),{g},{2})
                          .reshape({out_,in_*order_,kernel_,kernel_});
        result.push_back(torch::nn::functional::conv2d(flat,kernel,
            torch::nn::functional::Conv2dFuncOptions().padding(kernel_/2)));
    }
    return torch::stack(result,2);
}

AngularResult DecodeAngles(const torch::Tensor& logits, const torch::Tensor& q,
                          const torch::Tensor& valid, double temperature,
                          double minimum_concentration) {
    TORCH_CHECK(logits.defined() && logits.dim() >= 2 && logits.is_floating_point(),
                "CYCLIC_INVALID_LOGITS");
    const auto k=logits.size(-1);
    TORCH_CHECK(k>=4 && k<=24 && logits.numel()>0, "CYCLIC_INVALID_LOGIT_SIZE");
    CheckOrder(static_cast<int>(k));
    TORCH_CHECK(q.sizes() == logits.sizes().slice(0,logits.dim()-1) &&
                valid.sizes() == q.sizes() && q.device() == logits.device() &&
                valid.device() == logits.device() &&
                q.scalar_type() == torch::kInt64 && valid.scalar_type() == torch::kBool,
                "CYCLIC_INVALID_TARGET_SHAPE_OR_TYPE");
    TORCH_CHECK(std::isfinite(temperature) && temperature>0 &&
                std::isfinite(minimum_concentration) &&
                minimum_concentration>0 && minimum_concentration<=1,
                "CYCLIC_INVALID_DECODE_CONFIG");
    TORCH_CHECK(torch::isfinite(logits).all().item<bool>(), "CYCLIC_NONFINITE_LOGITS");
    TORCH_CHECK(((q==0)|(q==1)|(q==2)|(q==4)).all().item<bool>(),
                "CYCLIC_INVALID_SYMMETRY");
    TORCH_CHECK(((q==0)|(2*q<k)).all().item<bool>(), "CYCLIC_HARMONIC_ALIAS");
    AngularResult r;
    auto scaled=logits.to(torch::kFloat32)/temperature;
    TORCH_CHECK(torch::isfinite(scaled).all().item<bool>(), "CYCLIC_LOGIT_SCALE_OVERFLOW");
    r.probabilities = torch::softmax(scaled,-1);
    auto qf=q.clamp_min(1).to(torch::kFloat32);
    auto bins=torch::arange(k,r.probabilities.options())*(2*pi/k);
    auto phases=qf.unsqueeze(-1)*bins;
    auto real=(r.probabilities*torch::cos(phases)).sum(-1);
    auto imag=(r.probabilities*torch::sin(phases)).sum(-1);
    r.concentration=torch::stack({real,imag},-1).norm(2,-1).clamp(0,1);
    r.angle_valid=valid & (q>0) & (r.concentration>=minimum_concentration);
    // Mask BEFORE atan2 to avoid undefined derivative at the zero moment.
    auto safe_real=torch::where(r.angle_valid,real,torch::ones_like(real));
    auto safe_imag=torch::where(r.angle_valid,imag,torch::zeros_like(imag));
    r.angle_rad=torch::remainder(torch::atan2(safe_imag,safe_real)/qf,2*pi/qf);
    r.concentration=torch::where(valid & (q>0),r.concentration,
                                 torch::zeros_like(r.concentration));
    r.entropy=-(r.probabilities*r.probabilities.clamp_min(1e-30).log()).sum(-1);
    auto top=std::get<0>(r.probabilities.topk(2,-1));
    r.peak_margin=top.select(-1,0)-top.select(-1,1);
    return r;
}
} // namespace cxvision::cyclic
