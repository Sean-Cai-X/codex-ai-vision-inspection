#include "torch_cyclic_pose_branch.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace cxvision::cyclic;
namespace {
constexpr double pi=3.14159265358979323846;
int checks=0;
void Check(bool x,const char* why){++checks;if(!x)throw std::runtime_error(why);}
template<class F>void Reject(F f){bool yes=false;try{f();}catch(const c10::Error&){yes=true;}Check(yes,"expected rejection");}
void HeadContract(int k) {
    AngularHead h(1,k,3);
    {torch::NoGradGuard no;h->projection.fill_(1);}
    auto q=torch::tensor({1,2,4,0,1},torch::kInt64).reshape({1,5});
    auto theta=torch::tensor({-0.03,0.31,0.21,0.0,0.0}).reshape({1,5,1});
    auto bins=torch::arange(k,torch::kFloat32).view({1,1,k})*(2*pi/k);
    auto logits=(1+0.8*torch::cos(q.unsqueeze(-1)*(bins-theta))).log();
    auto valid=torch::tensor({true,true,true,true,false},torch::kBool).reshape({1,5});
    auto mass=valid.to(torch::kFloat32)*10;
    PooledInstances pooled{logits.unsqueeze(2),valid,mass,
                           {{"directed","axis","square","circle","empty"}},MaskSource::GroundTruth};
    std::vector<PooledInstances> scales(3,pooled);
    auto requested=torch::ones({1,5},torch::kBool);
    auto result=h->forward(scales,q,requested);
    Check(result.instance_ids==pooled.instance_ids&&result.mask_source==pooled.source,"head provenance");
    Check(result.predictive_only&&result.angles.predictive_only,"predictive boundary");
    Check(result.scale_mass.sizes()==torch::IntArrayRef({1,5,3}),"mass shape");
    Check(!h->forward(scales,q,torch::zeros_like(requested)).angles.angle_valid.any().item<bool>(),
          "requested validity honored");
    std::stringstream state;torch::serialize::OutputArchive saved;
    h->save(saved);saved.save_to(state);
    AngularHead wrong_temperature(1,k,3,2.0);
    torch::serialize::InputArchive loaded;loaded.load_from(state);wrong_temperature->load(loaded);
    Reject([&]{wrong_temperature->forward(scales,q,requested);});
    Check(!result.angles.angle_valid[0][3].item<bool>()&&!result.angles.angle_valid[0][4].item<bool>(),"circle/empty invalid");
    Check(result.logits[0][4].eq(0).all().item<bool>(),"empty logits neutral");
    for(int i=0;i<3;++i){
        int harmonic=q[0][i].item<int>();
        double err=std::remainder(result.angles.angle_rad[0][i].item<double>()-theta[0][i][0].item<double>(),2*pi/harmonic);
        Check(result.angles.angle_valid[0][i].item<bool>()&&std::abs(err)<1e-5,"known per-instance angle");
    }
    auto rotated=scales;
    for(auto& v:rotated)v.values=torch::roll(v.values,{1},{3});
    auto rr=h->forward(rotated,q,requested);
    Check(torch::allclose(rr.angles.probabilities,torch::roll(result.angles.probabilities,{1},{2}),1e-5,1e-6),"head orientation shift");
    auto unsupported=scales;unsupported[1].valid=valid.clone();unsupported[1].valid[0][0]=false;
    Check(!h->forward(unsupported,q,requested).mask_valid[0][0].item<bool>(),"missing one scale invalid");
    auto bad=scales;bad[1].instance_ids[0][0]="wrong";
    Reject([&]{h->forward(bad,q,requested);});
    bad=scales;bad[2].source=MaskSource::Predicted;
    Reject([&]{h->forward(bad,q,requested);});
    Reject([&]{h->forward(scales,q,requested.to(torch::kFloat32));});
    Reject([&]{h->forward({pooled},q,requested);});
    auto perm=torch::tensor({2,0,1,3,4},torch::kInt64);
    auto reordered=scales;
    for(auto& v:reordered){
        v.values=v.values.index_select(1,perm);v.valid=v.valid.index_select(1,perm);
        v.mass=v.mass.index_select(1,perm);
        v.instance_ids={{"square","directed","axis","circle","empty"}};
    }
    auto rp=h->forward(reordered,q.index_select(1,perm),requested);
    Check(torch::allclose(rp.angles.angle_rad,result.angles.angle_rad.index_select(1,perm)),"instance order preserved");
}
void BranchContract() {
    torch::manual_seed(918);const int k=12;
    PoseBranch model(1,2,k);model->eval();
    auto image=torch::randn({2,1,32,32});
    auto mask=torch::zeros({2,3,32,32});
    mask.select(1,0).slice(1,4,14).slice(2,3,12).fill_(1);
    mask.select(1,1).slice(1,17,28).slice(2,18,29).fill_(1);
    InstanceMasks masks{mask,{{"a","b","empty"},{"c","d","empty"}},MaskSource::Predicted};
    auto q=torch::tensor({1,2,1,1,2,0},torch::kInt64).reshape({2,3});
    auto requested=torch::ones({2,3},torch::kBool);
    auto base=model->forward(image,masks,q,requested);
    Check(base.logits.sizes()==torch::IntArrayRef({2,3,k}),"branch shape");
    Check(base.scale_mass.sizes()==torch::IntArrayRef({2,3,3}),"branch pyramid");
    Check(base.instance_ids==masks.instance_ids&&base.mask_source==masks.source,"branch provenance");
    InstanceMasks rm{RotateSpatial(mask,3,k),masks.instance_ids,masks.source};
    auto rotated=model->forward(RotateSpatial(image,3,k),rm,q,requested);
    double error=(rotated.logits-torch::roll(base.logits,{3},{2})).abs().max().item<double>();
    Check(error<1e-5,"branch quarter-turn");
    std::cout<<"METRIC branch_quarter_turn_max_abs="<<error<<"\n";
    Check(!base.angles.angle_valid.select(1,2).any().item<bool>(),"branch empty validity");
    model->train();
    auto trained=model->forward(image,masks,q,requested);
    auto bins=trained.bins_rad.view({1,1,k});
    auto target=torch::softmax(2*torch::cos(q.unsqueeze(-1)*(bins-0.4)),-1);
    // Supervision validity must not depend on the model's current concentration.
    auto supervised=trained.mask_valid&(q>0)&requested;
    auto ce=-(target*torch::log_softmax(trained.logits,-1)).sum(-1);
    auto loss=ce.masked_select(supervised).mean();
    loss.backward();
    for(auto parameter:{model->lift->coefficients,model->group1->bank->coefficients,
        model->group2->bank->coefficients,model->head->projection,model->head->scale_logits})
        Check(parameter.grad().defined()&&torch::isfinite(parameter.grad()).all().item<bool>()&&
            parameter.grad().abs().sum().item<double>()>1e-12,"branch gradient chain");
    std::cout<<"METRIC branch_training_loss="<<loss.item<double>()<<"\n";
    model->eval();
    auto before=model->forward(image,masks,q,requested);
    std::stringstream stream;torch::serialize::OutputArchive out;model->save(out);out.save_to(stream);
    PoseBranch restored(1,2,k);torch::serialize::InputArchive in;in.load_from(stream);restored->load(in);restored->eval();
    auto after=restored->forward(image,masks,q,requested);
    Check(torch::equal(before.logits,after.logits)&&torch::equal(before.angles.angle_rad,after.angles.angle_rad),"branch archive");
    Reject([&]{model->forward(image,ResizeMasks(masks,16,16),q,requested);});
    Reject([&]{model->forward(torch::zeros({2,1,30,30}),masks,q,requested);});
    Reject([&]{model->forward(image,masks,torch::ones({2},torch::kInt64),requested);});
}
}
int main(){
 try {
    torch::set_num_threads(1);HeadContract(12);HeadContract(24);BranchContract();
    std::cout<<"CHECKS="<<checks<<" RESULT=PASS scope=EXPERIMENTAL_POSE_BRANCH full_cn_gate=NOT_PASSED\n";return 0;
 }catch(const std::exception& e){std::cerr<<"RESULT=FAIL "<<e.what()<<"\n";return 1;}
}
