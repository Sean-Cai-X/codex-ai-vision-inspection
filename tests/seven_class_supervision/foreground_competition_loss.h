#pragma once
// Experimental training objective only. Seven emitted logits and inference
// sigmoid are unchanged; a fixed zero background logit is used only in the loss.
namespace foreground_competition {
struct Terms {torch::Tensor total,foreground,background,hard_background;};
inline Terms loss(const torch::Tensor& logits,const torch::Tensor& one_hot,bool sigmoid_background=false) {
    TORCH_CHECK(logits.dim()==2&&logits.size(0)>0&&logits.size(1)==7&&one_hot.sizes()==logits.sizes(),"COMPETITION_SHAPE");
    TORCH_CHECK(((one_hot==0)|(one_hot==1)).all().item<bool>()&&(one_hot.sum(1)<=1).all().item<bool>(),"COMPETITION_TARGET_NOT_SINGLE_OWNER");
    auto positive=one_hot.sum(1)>0;
    auto labels=torch::where(positive,std::get<1>(one_hot.max(1))+1,torch::zeros({logits.size(0)},logits.options().dtype(torch::kLong)));
    auto competing=torch::cat({torch::zeros({logits.size(0),1},logits.options()),logits},1);
    auto per_anchor=torch::nn::functional::cross_entropy(competing,labels,torch::nn::functional::CrossEntropyFuncOptions().reduction(torch::kNone));
    auto fg=per_anchor.masked_select(positive);
    // Inference accepts any sigmoid channel. Sum BCE over all seven channels
    // so a background anchor pays for each emitted false-class score.
    auto bg_per_anchor=sigmoid_background?torch::nn::functional::binary_cross_entropy_with_logits(logits,torch::zeros_like(logits),
        torch::nn::functional::BinaryCrossEntropyWithLogitsFuncOptions().reduction(torch::kNone)).sum(1):per_anchor;
    auto bg=bg_per_anchor.masked_select(positive.logical_not());
    auto zero=logits.sum()*0;Terms t;
    t.foreground=fg.numel()?fg.mean():zero;t.background=bg.numel()?bg.mean():zero;
    const auto count=std::min<int64_t>(bg.numel(),std::max<int64_t>(1,fg.numel()*20));
    t.hard_background=count?std::get<0>(bg.topk(count)).mean():zero;
    t.total=t.foreground+t.background+2*t.hard_background;return t;
}
inline void self_test(){
    for(int cls=0;cls<7;++cls){
        auto logits=torch::zeros({4,7});logits.index_put_({0,(cls+1)%7},5);logits.set_requires_grad(true);
        auto target=torch::zeros_like(logits).detach();target.index_put_({0,cls},1);target.index_put_({1,(cls+1)%7},1);
        auto terms=loss(logits,target);terms.total.backward();auto g=logits.grad();
        for(int c=0;c<7;++c){TORCH_CHECK((g[0][c].item<double>()<0)==(c==cls),"COMPETITION_CLASS_GRADIENT");
            TORCH_CHECK(g[2][c].item<double>()>0&&g[3][c].item<double>()>0,"COMPETITION_BACKGROUND_GRADIENT");}
        TORCH_CHECK(loss(logits.detach()-.01*g,target).total.item<double>()<terms.total.item<double>(),"COMPETITION_NOT_DESCENDING");
        TORCH_CHECK(torch::allclose(terms.total,terms.foreground+terms.background+2*terms.hard_background),"COMPETITION_TERM_SUM");
    }
    for(bool all_positive:{false,true}){
        auto logits=torch::full({2,7},50.,torch::TensorOptions().requires_grad(true));auto target=torch::zeros_like(logits).detach();
        if(all_positive)target.select(1,2).fill_(1);auto t=loss(logits,target);t.total.backward();
        TORCH_CHECK(torch::isfinite(t.total).item<bool>()&&torch::isfinite(logits.grad()).all().item<bool>(),"COMPETITION_EMPTY_PARTITION_NONFINITE");
        if(!all_positive)TORCH_CHECK((logits.grad()>0).all().item<bool>(),"COMPETITION_ALL_BACKGROUND_GRADIENT");
    }
    bool rejected=false;try{auto bad=torch::zeros({2,7});bad[0][1]=1;bad[0][2]=1;loss(torch::zeros_like(bad),bad);}catch(const c10::Error&){rejected=true;}
    TORCH_CHECK(rejected,"COMPETITION_MULTILABEL_ACCEPTED");
    for(int cls=0;cls<7;++cls){
        auto logits=torch::zeros({3,7},torch::TensorOptions().requires_grad(true));
        auto target=torch::zeros_like(logits).detach();target.index_put_({0,cls},1);
        auto previous=loss(logits,target),aligned=loss(logits,target,true);
        TORCH_CHECK(torch::equal(previous.foreground,aligned.foreground),"SIGMOID_BACKGROUND_CHANGED_FOREGROUND");
        TORCH_CHECK(std::abs(aligned.background.item<double>()-7*std::log(2.0))<1e-5,"SIGMOID_BACKGROUND_BCE_SCALE");
        aligned.total.backward();auto grad=logits.grad();
        for(int c=0;c<7;++c){TORCH_CHECK((grad[0][c].item<double>()<0)==(c==cls),"SIGMOID_BACKGROUND_POSITIVE_GRADIENT");
            TORCH_CHECK(std::abs(grad[1][c].item<double>()-.75)<1e-6,"SIGMOID_BACKGROUND_CHANNEL_GRADIENT");}
        TORCH_CHECK(loss(logits.detach()-.01*grad,target,true).total.item<double>()<aligned.total.item<double>(),"SIGMOID_BACKGROUND_NOT_DESCENDING");
    }
    for(bool positive:{false,true}){
        auto logits=torch::full({2,7},50.,torch::TensorOptions().requires_grad(true));auto target=torch::zeros_like(logits).detach();
        if(positive)target.select(1,1).fill_(1);auto t=loss(logits,target,true);t.total.backward();
        TORCH_CHECK(torch::isfinite(t.total).item<bool>()&&torch::isfinite(logits.grad()).all().item<bool>(),"SIGMOID_BACKGROUND_NONFINITE");
    }
}
}
