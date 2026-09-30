#include "torch_cyclic_analytic_lift.h"
#include <cmath>

namespace cxvision::cyclic {
namespace {
constexpr double pi=3.14159265358979323846;
torch::Tensor MakeBasis(int size,double sigma) {
    auto t=torch::arange(size,torch::kFloat64)-(size-1)/2.0;
    auto x=t.view({1,size}).expand({size,size});
    auto y=-t.view({size,1}).expand({size,size});
    auto r=torch::sqrt(x*x+y*y);
    const double radius=(size-1)/2.0;
    // Circular compact support prevents rotating square-corner energy in/out.
    auto envelope=torch::exp(-r*r/(2*sigma*sigma))*
        (0.5*(1+torch::cos(pi*r/radius)))*r.lt(radius);
    auto u=x/sigma,v=y/sigma;
    std::vector<torch::Tensor> b={envelope,envelope*u,envelope*v,
        envelope*(u*u-v*v),envelope*(2*u*v),
        envelope*(u*u+v*v-2)};
    // Same scaling for both members of a harmonic pair preserves steering.
    for(auto pair: {std::pair<int,int>{0,0},{1,2},{3,4},{5,5}}) {
        auto norm=torch::sqrt((b[pair.first].square().sum()+
                               b[pair.second].square().sum())/2).clamp_min(1e-12);
        b[pair.first]=b[pair.first]/norm;
        if(pair.first!=pair.second)b[pair.second]=b[pair.second]/norm;
    }
    return torch::stack(b).to(torch::kFloat32);
}
torch::Tensor Contract(int in,int out,int order,int size,double sigma) {
    return torch::tensor({1.0,1.0,static_cast<double>(in),static_cast<double>(out),
                          static_cast<double>(order),static_cast<double>(size),sigma},
                         torch::kFloat64);
}
}
AnalyticLiftImpl::AnalyticLiftImpl(int in,int out,int order,int size,double sigma)
    :in_(in),out_(out),order_(order),size_(size),sigma_(sigma) {
    TORCH_CHECK(in>0 && out>0 && order>=4 && order<=24 && order%4==0 &&
        size>=7 && size<=15 && size%2==1 && std::isfinite(sigma) &&
        sigma>=0.8 && sigma<=(size-1)/3.0,"ANALYTIC_LIFT_OPTIONS");
    coefficients=register_parameter("coefficients",
        torch::randn({out,in,6})/std::sqrt(6.0*in));
    contract_=register_buffer("contract",Contract(in,out,order,size,sigma));
    basis_=register_buffer("basis",MakeBasis(size,sigma));
}
void AnalyticLiftImpl::validate() const {
    TORCH_CHECK(torch::equal(contract_.cpu(),Contract(in_,out_,order_,size_,sigma_)),
                "ANALYTIC_LIFT_ARCHIVE_CONTRACT");
}
torch::Tensor AnalyticLiftImpl::kernel(int step) {
    validate();
    step=(step%order_+order_)%order_;
    double a=2*pi*step/order_;
    auto b0=basis_[0],b1=basis_[1],b2=basis_[2],b3=basis_[3],b4=basis_[4],b5=basis_[5];
    // Evaluate f(R^-1 x); displayed CCW, mathematical y points up.
    auto bank=torch::stack({b0,std::cos(a)*b1+std::sin(a)*b2,
        -std::sin(a)*b1+std::cos(a)*b2,
        std::cos(2*a)*b3+std::sin(2*a)*b4,
        -std::sin(2*a)*b3+std::cos(2*a)*b4,b5});
    return torch::matmul(coefficients,bank.reshape({6,size_*size_}))
        .reshape({out_,in_,size_,size_});
}
torch::Tensor AnalyticLiftImpl::forward(const torch::Tensor& x) {
    TORCH_CHECK(x.defined() && x.dim()==4 && x.size(0)>0 &&
        x.size(1)==in_ && x.size(2)>0 && x.size(3)>0 &&
        x.device()==coefficients.device() && x.scalar_type()==coefficients.scalar_type(),
        "ANALYTIC_LIFT_INPUT");
    std::vector<torch::Tensor> y;
    for(int g=0;g<order_;++g)
        y.push_back(torch::nn::functional::conv2d(x,kernel(g),
            torch::nn::functional::Conv2dFuncOptions().padding(size_/2)));
    return torch::stack(y,2);
}
}
