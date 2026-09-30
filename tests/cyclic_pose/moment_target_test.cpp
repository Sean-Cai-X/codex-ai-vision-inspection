#include "moment_target.h"
#include "torch_cyclic_group_ops.h"
#include <iostream>
using namespace cxvision::cyclic;
int main() {
 try {
    torch::set_num_threads(1);
    double worst=0;
    int count=0;
    for(int K:{12,16,24}) for(int q:{1,2,4}) {
        auto theta=torch::arange(1441,torch::kFloat32)*(2*3.14159265358979323846/1440);
        auto orders=torch::full({1441},q,torch::kInt64);
        auto p=experiment::MomentTarget(theta,orders,K);
        TORCH_CHECK(p.min().item<double>()>0,"positive target");
        TORCH_CHECK((p.sum(-1)-1).abs().max().item<double>()<1e-6,"normalization");
        auto decoded=DecodeAngles(p.log(),orders,torch::ones({1441},torch::kBool));
        auto delta=(decoded.angle_rad-theta)*q;
        double err=(torch::atan2(torch::sin(delta),torch::cos(delta)).abs()/q*
                    (180/3.14159265358979323846)).max().item<double>();
        TORCH_CHECK(err<0.0001,"ideal moment angle error: ",err);
        TORCH_CHECK((decoded.concentration-.4).abs().max().item<double>()<1e-6,"moment magnitude");
        TORCH_CHECK(decoded.angle_valid.all().item<bool>(),"valid angles");
        worst=std::max(worst,err);count+=1441;
    }
    auto theta=torch::zeros({1},torch::kFloat32);
    auto zero=torch::zeros({1},torch::kInt64);
    auto p=experiment::MomentTarget(theta,zero,12);
    TORCH_CHECK(!DecodeAngles(p.log(),zero,torch::ones({1},torch::kBool)).angle_valid.item<bool>(),
                "circle must remain undefined");
    int rejected=0;
    for(double rho:{0.0,0.5,-0.1}) {
        try {experiment::MomentTarget(theta,zero,12,rho);}
        catch(const c10::Error&) {++rejected;}
    }
    try {experiment::MomentTarget(theta,torch::full({1},4,torch::kInt64),8);}
    catch(const c10::Error&) {++rejected;}
    TORCH_CHECK(rejected==4,"invalid target inputs accepted");
    std::cout<<"TARGET_CONTRACT PASS angles="<<count<<" max_error_deg="<<worst
             <<" invalid_rejections="<<rejected<<std::endl;
    return 0;
 } catch(const std::exception& e) {std::cerr<<e.what()<<std::endl;return 1;}
}
