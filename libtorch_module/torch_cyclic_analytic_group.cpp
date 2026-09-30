#include "torch_cyclic_analytic_group.h"
namespace cxvision::cyclic {
AnalyticGroupImpl::AnalyticGroupImpl(int in,int out,int order,int size,double sigma)
    :in_(in),out_(out),order_(order),size_(size) {
    TORCH_CHECK(in>0 && in<=4096 && order>=4 && order<=24 &&
                order%4==0,"ANALYTIC_GROUP_OPTIONS");
    bank=register_module("bank",AnalyticLift(in*order,out,order,size,sigma));
}
torch::Tensor AnalyticGroupImpl::kernel(int step) {
    step=(step%order_+order_)%order_;
    // For input orientation h and output g use relative orientation h-g.
    return torch::roll(bank->kernel(step).reshape({out_,in_,order_,size_,size_}),
                       {step},{2}).reshape({out_,in_*order_,size_,size_});
}
torch::Tensor AnalyticGroupImpl::forward(const torch::Tensor& x) {
    TORCH_CHECK(x.defined() && x.dim()==5 && x.size(0)>0 && x.size(1)==in_ &&
        x.size(2)==order_ && x.size(3)>0 && x.size(4)>0 &&
        x.device()==bank->coefficients.device() &&
        x.scalar_type()==bank->coefficients.scalar_type(),"ANALYTIC_GROUP_INPUT");
    auto flat=x.reshape({x.size(0),in_*order_,x.size(3),x.size(4)});
    std::vector<torch::Tensor> y;
    for(int g=0;g<order_;++g)
        y.push_back(torch::nn::functional::conv2d(flat,kernel(g),
            torch::nn::functional::Conv2dFuncOptions().padding(size_/2)));
    return torch::stack(y,2);
}
}
