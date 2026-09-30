#include "torch_cyclic_analytic_group.h"
#include <cmath>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace cxvision::cyclic;
namespace {
int checks=0;
void Check(bool x,const char* s){++checks;if(!x)throw std::runtime_error(s);}
template<class F> void Reject(F f){bool yes=false;try{f();}catch(const c10::Error&){yes=true;}Check(yes,"expected rejection");}
double Error(torch::Tensor a,torch::Tensor b,int crop=18) {
    a=a.slice(-2,crop,a.size(-2)-crop).slice(-1,crop,a.size(-1)-crop);
    b=b.slice(-2,crop,b.size(-2)-crop).slice(-1,crop,b.size(-1)-crop);
    double energy=b.square().mean().sqrt().item<double>();
    Check(energy>1e-8,"zero-feature shortcut");
    return (a-b).square().mean().sqrt().item<double>()/energy;
}
torch::Tensor Image() {
    auto t=torch::arange(49,torch::kFloat32)-24;
    auto x=t.view({1,49}),y=-t.view({49,1});
    return (torch::exp(-((x+7).square()/40+(y-3).square()/90))+
        0.6*torch::exp(-((x-8).square()/32+(y+6).square()/50))-
        0.3*torch::exp(-((x-3).square()/70+(y-9).square()/28))).reshape({1,1,49,49});
}
}
int main(){
 try{
    torch::set_num_threads(1);
    double analytic_sum=0,sampled_sum=0,worst=0;
    int cases=0;
    for(int seed:{18,29})for(int k:{8,12,24}) {
        torch::manual_seed(seed);
        AnalyticLift lift(1,1,k);
        AnalyticGroup g1(1,1,k),g2(1,1,k);
        LiftConv sl(1,1,k,9);
        GroupConv sg1(1,1,k,9),sg2(1,1,k,9);
        {torch::NoGradGuard guard;
            sl->weight.copy_(lift->kernel(0));
            sg1->weight.copy_(g1->kernel(0).reshape({1,1,k,9,9}));
            sg2->weight.copy_(g2->kernel(0).reshape({1,1,k,9,9}));
        }
        auto stages=[&](torch::Tensor x,bool analytic) {
            auto a=torch::silu(analytic?lift->forward(x):sl->forward(x));
            auto b=torch::silu(analytic?g1->forward(a):sg1->forward(a));
            auto c=torch::silu(analytic?g2->forward(b):sg2->forward(b));
            return std::vector<torch::Tensor>{a,b,c};
        };
        auto x=Image();
        {
            torch::NoGradGuard guard;
            auto base=stages(x,true),sampled_base=stages(x,false);
            for(int step:{1,k/4,k/4+1,k-1}) {
                auto rotated=stages(RotateSpatial(x,step,k),true);
                auto sr=stages(RotateSpatial(x,step,k),false);
                for(int depth=0;depth<3;++depth) {
                    double ae=Error(rotated[depth],GroupAction(base[depth],step,k));
                    double se=Error(sr[depth],GroupAction(sampled_base[depth],step,k));
                    Check(std::isfinite(ae)&&std::isfinite(se),"nonfinite metric");
                    if(step==k/4)Check(ae<5e-5,"quarter-turn stack");
                    else {
                        // Predeclared controlled smooth-field criterion, not G1.
                        Check(ae<0.10,"smooth stack 10 percent gate");
                        if(depth==2){analytic_sum+=ae;sampled_sum+=se;++cases;worst=std::max(worst,ae);}
                    }
                    std::cout<<"STACK seed="<<seed<<" K="<<k<<" step="<<step
                        <<" depth="<<depth+1<<" analytic="<<ae<<" sampled="<<se<<"\n";
                }
            }
            // E2 group-only test on arbitrary orientation channels, no Lift dependency.
            auto feature=torch::randn({1,1,k,49,49});
            Check(Error(g1->forward(GroupAction(feature,k/4,k)),
                GroupAction(g1->forward(feature),k/4,k))<5e-5,"group random quarter-turn");
            auto rolled=torch::roll(feature,{1},{2});
            // Negative control: orientation-only shift is NOT the full group action.
            double bad=Error(g1->forward(rolled),torch::roll(g1->forward(feature),{1},{2}));
            Check(bad>1e-4,"negative control insensitive to missing spatial rotation");
        }
        auto last=stages(x,true).back();
        // Orientation-weighted loss prevents invariant pooling from hiding routes.
        auto angular_weights=torch::arange(k,last.options()).view({1,1,k,1,1})/k;
        (last*angular_weights).square().mean().backward();
        for(auto c:{lift->coefficients,g1->bank->coefficients,g2->bank->coefficients})
            Check(c.grad().defined()&&torch::isfinite(c.grad()).all().item<bool>()&&
                  c.grad().abs().sum().item<double>()>1e-9,"stack gradient");
        std::stringstream s;torch::serialize::OutputArchive oa;g1->save(oa);oa.save_to(s);
        AnalyticGroup reloaded(1,1,k);
        torch::serialize::InputArchive ia;ia.load_from(s);reloaded->load(ia);
        auto f=lift->forward(x);
        Check(torch::equal(g1->forward(f),reloaded->forward(f)),"group archive");
        s.clear();s.seekg(0);
        AnalyticGroup mismatch(1,1,k==8?12:8);
        torch::serialize::InputArchive mi;mi.load_from(s);mismatch->load(mi);
        Reject([&]{mismatch->forward(torch::zeros({1,1,k==8?12:8,49,49}));});
        Reject([&]{g1->forward(torch::zeros({1,1,k+1,49,49}));});
        Reject([&]{g1->forward(torch::zeros({1,1,k,49,49},torch::kFloat64));});
    }
    Reject([]{AnalyticGroup g(1,1,3);});
    Reject([]{AnalyticGroup g(0,1,12);});
    Check(cases==18 && analytic_sum<sampled_sum,"matched stack aggregate improvement");
    std::cout<<"SUMMARY cases="<<cases<<" analytic_mean="<<analytic_sum/cases
        <<" sampled_mean="<<sampled_sum/cases<<" analytic_max="<<worst<<"\n";
    std::cout<<"CHECKS="<<checks<<" RESULT=PASS scope=CONTROLLED_SMOOTH_STACK full_cn_gate=NOT_PASSED\n";
    return 0;
 }catch(const std::exception& e){std::cerr<<"RESULT=FAIL "<<e.what()<<"\n";return 1;}
}
