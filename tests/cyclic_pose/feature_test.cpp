#include "torch_cyclic_feature_ops.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace cxvision::cyclic;
namespace {
int checks=0;
void Check(bool v,const char* why){++checks;if(!v)throw std::runtime_error(why);}
template<class F>void Reject(F f){bool rejected=false;try{f();}catch(const c10::Error&){rejected=true;}Check(rejected,"expected rejection");}
bool Near(const torch::Tensor& a,const torch::Tensor& b){return torch::allclose(a,b,2e-5,2e-6);}
}
int main(){
 try{
    torch::set_num_threads(1);torch::manual_seed(918);
    for(int k:{8,12,24}) {
        GroupFeature f{torch::randn({2,3,k,17,17}),k};
        SharedBatchNorm a(3,k),b(3,k);
        a->train();b->train();
        auto n=a->forward(f);
        auto rotated=b->forward({GroupAction(f.values,k/4,k),k});
        Check(Near(rotated.values,GroupAction(n.values,k/4,k)),"train BN rotation");
        Check(Near(a->norm->running_mean,b->norm->running_mean),"shared running mean");
        Check(Near(a->norm->running_var,b->norm->running_var),"shared running variance");
        Check(a->norm->weight.numel()==3,"per-orientation affine leak");
        (n.values.square().mean()+n.values.mean()).backward();
        for(auto parameter:{a->norm->weight,a->norm->bias})
            Check(torch::isfinite(parameter.grad()).all().item<bool>()&&
                parameter.grad().abs().sum().item<double>()>1e-6,"BN affine gradient");
        a->eval();
        Check(Near(a->forward({GroupAction(f.values,k/4,k),k}).values,
                   GroupAction(a->forward(f).values,k/4,k)),"eval BN rotation");
        for(int size:{8,17,25}) {
            auto resized=ResizeGroup(f,size,size);
            Check(resized.values.sizes()==torch::IntArrayRef({2,3,k,size,size}),"resize shape");
            Check(Near(ResizeGroup({GroupAction(f.values,k/4,k),k},size,size).values,
                       GroupAction(resized.values,k/4,k)),"resize quarter-turn");
            Check(Near(ResizeGroup({torch::roll(f.values,{1},{2}),k},size,size).values,
                       torch::roll(resized.values,{1},{2})),"resize orientation axis");
        }
        auto cat=ConcatGroup({f,f});
        Check(cat.values.size(1)==6&&cat.values.size(2)==k,"concat axes");
        Check(torch::equal(cat.values.slice(1,0,3),f.values),"concat values");
        Reject([&]{ConcatGroup({f,{f.values,k,2}});});
        Reject([&]{ResizeGroup(f,8,25);});
        Reject([&]{ResizeGroup(f,0,8);});
        Reject([&]{a->forward({f.values,k==8?12:8});});
        std::stringstream stream;torch::serialize::OutputArchive out;
        a->save(out);out.save_to(stream);
        SharedBatchNorm reloaded(3,k);torch::serialize::InputArchive in;
        in.load_from(stream);reloaded->load(in);reloaded->eval();
        Check(torch::equal(a->forward(f).values,reloaded->forward(f).values),"norm archive");
        auto mask=torch::zeros({2,3,17,17});
        mask.select(1,0).slice(1,1,7).slice(2,2,6).fill_(1);
        mask.select(1,1).slice(1,9,15).slice(2,10,16).fill_(0.5);
        InstanceMasks m{mask,{{"left","right","empty"},{"left","right","empty"}},MaskSource::GroundTruth};
        auto pooled=PoolInstances(f,m);
        Check(pooled.values.sizes()==torch::IntArrayRef({2,3,3,k}),"pool axes");
        Check(pooled.instance_ids==m.instance_ids&&pooled.source==m.source,"pool provenance");
        Check(!pooled.valid.select(1,2).any().item<bool>()&&
               pooled.values.select(1,2).eq(0).all().item<bool>(),"empty mask validity");
        for(int batch=0;batch<2;++batch)for(int instance=0;instance<2;++instance) {
            auto expected=(f.values[batch]*mask[batch][instance]).sum({2,3})/mask[batch][instance].sum();
            Check(Near(pooled.values[batch][instance],expected),"independent manual pooling oracle");
        }
        auto perm=torch::tensor({1,0,2},torch::kInt64);
        InstanceMasks reordered{mask.index_select(1,perm),
            {{"right","left","empty"},{"right","left","empty"}},MaskSource::Predicted};
        Check(Near(PoolInstances(f,reordered).values,pooled.values.index_select(1,perm)),"instance permutation");
        Check(PoolInstances(f,reordered).source==MaskSource::Predicted,"predicted source");
        InstanceMasks mr{RotateSpatial(mask,k/4,k),m.instance_ids,m.source};
        auto pr=PoolInstances({GroupAction(f.values,k/4,k),k},mr);
        Check(Near(pr.values,torch::roll(pooled.values,{k/4},{3})),"joint mask feature rotation");
        auto resized_masks=ResizeMasks(m,8,8);
        Check(Near(ResizeMasks(mr,8,8).values,RotateSpatial(resized_masks.values,k/4,k)),"mask resize rotation");
        Check(PoolInstances(ResizeGroup(f,8,8),resized_masks).valid.select(1,0).all().item<bool>(),"multiscale pool");
        Reject([&]{PoolInstances(f,resized_masks);});
        auto duplicate=m;duplicate.instance_ids[0][1]="left";
        Reject([&]{PoolInstances(f,duplicate);});
        auto invalid=m;invalid.values=-mask;
        Reject([&]{PoolInstances(f,invalid);});
        Reject([&]{PoolInstances(f,m,0);});
        Reject([&]{PoolInstances(f,m,1e-300);});
        auto grad=torch::randn({2,3,k,17,17},torch::requires_grad());
        auto p=PoolInstances({grad,k},m);
        p.values[0][0].sum().backward();
        Check(grad.grad()[1].eq(0).all().item<bool>(),"cross-batch gradient leak");
        auto outside=(mask[0][0]==0).to(torch::kFloat32);
        Check((grad.grad()[0]*outside).eq(0).all().item<bool>(),"cross-instance gradient leak");
        Check(torch::isfinite(grad.grad()).all().item<bool>()&&grad.grad().abs().sum().item<double>()>0,"pool gradients");
        auto zero_masks=torch::zeros({2,3,17,17},torch::requires_grad());
        InstanceMasks empty{zero_masks,m.instance_ids,m.source};
        PoolInstances(f,empty,1e-37).values.sum().backward();
        Check(torch::isfinite(zero_masks.grad()).all().item<bool>()&&
            zero_masks.grad().eq(0).all().item<bool>(),"empty-mask safe gradient");
        auto resize_grad=torch::randn({2,3,k,17,17},torch::requires_grad());
        ResizeGroup({resize_grad,k},8,8).values.square().mean().backward();
        Check(torch::isfinite(resize_grad.grad()).all().item<bool>()&&
            resize_grad.grad().abs().sum().item<double>()>0,"resize gradient");
        auto nonfinite=m;nonfinite.values=mask/0;
        Reject([&]{PoolInstances(f,nonfinite);});
    }
    Reject([]{ConcatGroup({});});
    std::cout<<"CHECKS="<<checks<<" RESULT=PASS scope=FEATURE_MASK_CONTRACT full_cn_gate=NOT_PASSED\n";
    return 0;
 }catch(const std::exception& e){std::cerr<<"RESULT=FAIL "<<e.what()<<"\n";return 1;}
}
