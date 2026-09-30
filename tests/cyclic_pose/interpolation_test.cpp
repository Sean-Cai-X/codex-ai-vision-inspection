#include "torch_cyclic_analytic_lift.h"
#include <cmath>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace cxvision::cyclic;
namespace {
int checks=0;
void Check(bool v,const char* why) {++checks;if(!v)throw std::runtime_error(why);}
template<class F> void Reject(F fn) {
    bool rejected=false;
    try{fn();}catch(const c10::Error&){rejected=true;}
    Check(rejected,"invalid analytic contract accepted");
}
double Error(torch::Tensor a,torch::Tensor b,int crop=0) {
    if(crop) {
        a=a.slice(-2,crop,a.size(-2)-crop).slice(-1,crop,a.size(-1)-crop);
        b=b.slice(-2,crop,b.size(-2)-crop).slice(-1,crop,b.size(-1)-crop);
    }
    double energy=b.square().mean().sqrt().item<double>();
    Check(energy>1e-7,"degenerate output");
    return (a-b).square().mean().sqrt().item<double>()/energy;
}
// Fixed analytic image family, not a re-resampled image or a fitted reference.
// angle is applied to the underlying continuous Gaussian field before sampling.
torch::Tensor Field(int step,int k) {
    constexpr double pi=3.14159265358979323846;
    auto t=torch::arange(49,torch::kFloat32)-24;
    auto x=t.view({1,49}).expand({49,49}),y=-t.view({49,1}).expand({49,49});
    double a=2*pi*step/k;
    auto u=std::cos(a)*x+std::sin(a)*y,v=-std::sin(a)*x+std::cos(a)*y;
    return (torch::exp(-((u+7).square()/40+(v-3).square()/90))+
        0.6*torch::exp(-((u-8).square()/32+(v+6).square()/50))-
        0.3*torch::exp(-((u-3).square()/70+(v-9).square()/28))).reshape({1,1,49,49});
}
}
int main() {
 try {
    torch::set_num_threads(1);
    Reject([]{AnalyticLift x(1,2,3);});
    Reject([]{AnalyticLift x(1,2,12,6);});
    Reject([]{AnalyticLift x(1,2,12,9,0.1);});
    Reject([]{AnalyticLift x(0,2,12);});
    AnalyticLift guard(1,2,12);
    Reject([&]{guard->forward(torch::zeros({1,2,49,49}));});
    Reject([&]{guard->forward(torch::zeros({1,1,49,49},torch::kFloat64));});
    double matched_sum=0,analytic_sum=0,worst=0;
    int samples=0;
    for(int seed:{18,29,41})for(int k:{8,12,24}) {
        torch::manual_seed(seed);
        AnalyticLift analytic(1,2,k);
        LiftConv sampled(1,2,k,9),arbitrary(1,2,k,9);
        {torch::NoGradGuard guard;sampled->weight.copy_(analytic->kernel(0));}
        auto white=torch::randn({1,1,49,49}),smooth=Field(0,k);
        for(int m=1;m<k;++m) {
            torch::NoGradGuard guard;
            auto rx=RotateSpatial(smooth,m,k),ideal=Field(m,k);
            auto expected=GroupAction(analytic->forward(smooth),m,k);
            auto observed=analytic->forward(rx);
            auto se=GroupAction(sampled->forward(smooth),m,k);
            auto so=sampled->forward(rx);
            double ea=Error(observed,expected,12),es=Error(so,se,12);
            double eideal=Error(analytic->forward(ideal),expected,12);
            Check(std::isfinite(ea)&&std::isfinite(es),"finite error");
            if((4*m)%k==0)Check(ea<2e-5,"quarter-turn regression");
            else {
                // Declared before measuring: smooth fixed-field interior only.
                Check(ea<0.08,"smooth-field interior 8 percent gate");
                matched_sum+=es;analytic_sum+=ea;worst=std::max(worst,ea);++samples;
            }
            std::cout<<"SMOOTH seed="<<seed<<" K="<<k<<" step="<<m
                <<" input_resample="<<Error(rx,ideal,12)
                <<" sampled_interior="<<es<<" analytic_interior="<<ea
                <<" analytic_full="<<Error(observed,expected)
                <<" analytic_ideal_input="<<eideal<<"\n";
        }
        {
            torch::NoGradGuard guard;
            auto rx=RotateSpatial(white,1,k);
            std::cout<<"WHITE seed="<<seed<<" K="<<k
                <<" arbitrary_full="<<Error(arbitrary->forward(rx),GroupAction(arbitrary->forward(white),1,k))
                <<" matched_interior="<<Error(sampled->forward(rx),GroupAction(sampled->forward(white),1,k),12)
                <<" analytic_interior="<<Error(analytic->forward(rx),GroupAction(analytic->forward(white),1,k),12)<<"\n";
        }
        auto y=analytic->forward(smooth);
        y.square().mean().backward();
        Check(analytic->coefficients.grad().defined() &&
              torch::isfinite(analytic->coefficients.grad()).all().item<bool>() &&
              analytic->coefficients.grad().abs().sum().item<double>()>1e-6,"analytic gradient");
        // Archive must reproduce output, not just weight dimensions.
        std::stringstream stream;
        torch::serialize::OutputArchive oa;analytic->save(oa);oa.save_to(stream);
        AnalyticLift restored(1,2,k);
        torch::serialize::InputArchive ia;ia.load_from(stream);restored->load(ia);
        Check(torch::equal(y,restored->forward(smooth)),"analytic archive");
        stream.clear();stream.seekg(0);
        AnalyticLift wrong_order(1,2,k==8?12:8);
        torch::serialize::InputArchive wrong_archive;wrong_archive.load_from(stream);
        wrong_order->load(wrong_archive);
        Reject([&]{wrong_order->forward(smooth);});
    }
    Check(samples>0 && analytic_sum<matched_sum,"matched-bank aggregate improvement");
    std::cout<<"SUMMARY samples="<<samples<<" sampled_mean="<<matched_sum/samples
             <<" analytic_mean="<<analytic_sum/samples<<" analytic_max="<<worst<<"\n";
    std::cout<<"CHECKS="<<checks<<" RESULT=PASS scope=SMOOTH_FIELD_LIFT_ONLY full_cn_gate=NOT_PASSED\n";
    return 0;
 }catch(const std::exception& e){std::cerr<<"RESULT=FAIL "<<e.what()<<"\n";return 1;}
}
