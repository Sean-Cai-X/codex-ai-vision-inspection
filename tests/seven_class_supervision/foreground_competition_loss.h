#pragma once
// Experimental training objective only. Seven emitted logits and inference
// sigmoid are unchanged; a fixed zero background logit is used only in the loss.
namespace foreground_competition {
struct Terms {
    torch::Tensor total,foreground,background,hard_background;
    // Diagnostic masks/indices from the actual selection, never inferred from gradients.
    torch::Tensor positive_anchors,hard_background_anchors;
};
inline torch::Tensor stratified_indices(const torch::Tensor& values,const torch::Tensor& positive,
    int64_t count,const std::vector<int64_t>& sizes){
    torch::NoGradGuard guard;
    TORCH_CHECK(sizes.size()==3&&sizes[0]>0&&sizes[1]>0&&sizes[2]>0&&sizes[0]+sizes[1]+sizes[2]==values.numel(),"STRATIFIED_LEVEL_LAYOUT");
    auto used=torch::zeros_like(positive);std::vector<torch::Tensor> selected;int64_t offset=0,total=0;
    const std::vector<int64_t> quotas={count/5,count/5,count-2*(count/5)};
    for(int level=0;level<3;++level){
        auto indices=torch::nonzero(positive.slice(0,offset,offset+sizes[level]).logical_not()).squeeze(1)+offset;
        auto quota=std::min<int64_t>(indices.numel(),quotas[level]);
        if(quota){auto best=std::get<1>(values.index_select(0,indices).topk(quota));auto ids=indices.index_select(0,best);
            selected.push_back(ids);used.index_put_({ids},true);total+=quota;}offset+=sizes[level];
    }
    if(total<count){auto remaining=torch::nonzero(positive.logical_not()&used.logical_not()).squeeze(1);
        TORCH_CHECK(remaining.numel()>=count-total,"STRATIFIED_BUDGET_UNFILLABLE");
        selected.push_back(remaining.index_select(0,std::get<1>(values.index_select(0,remaining).topk(count-total))));}
    return selected.empty()?torch::empty({0},positive.options().dtype(torch::kLong)):torch::cat(selected);
}
inline Terms loss(const torch::Tensor& logits,const torch::Tensor& one_hot,bool sigmoid_background=false,
    const std::vector<int64_t>& level_sizes={}) {
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
    t.positive_anchors=positive.detach();
    t.hard_background_anchors=torch::empty({0},logits.options().dtype(torch::kLong));
    if(!level_sizes.empty()){
        TORCH_CHECK(sigmoid_background,"STRATIFIED_REQUIRES_SIGMOID_BACKGROUND");
        t.hard_background_anchors=stratified_indices(bg_per_anchor,positive,count,level_sizes);
        t.hard_background=count?bg_per_anchor.index_select(0,t.hard_background_anchors).mean():zero;
    }else if(count){auto selected=bg.topk(count);t.hard_background=std::get<0>(selected).mean();
        t.hard_background_anchors=torch::nonzero(positive.logical_not()).squeeze(1).index_select(0,std::get<1>(selected));
    }else t.hard_background=zero;
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
    // Coarse-level scores dominate the global miner; floors must still sample fine levels.
    auto logits=torch::zeros({900,7},torch::TensorOptions().requires_grad(true));
    {torch::NoGradGuard guard;logits.slice(0,600,900).fill_(5);}
    auto target=torch::zeros_like(logits).detach();target.slice(0,600,610).select(1,1).fill_(1);
    auto global=loss(logits,target,true),balanced=loss(logits,target,true,{300,300,300});
    TORCH_CHECK(torch::equal(global.foreground,balanced.foreground)&&torch::equal(global.background,balanced.background),"STRATIFIED_CHANGED_OTHER_TERMS");
    TORCH_CHECK(global.hard_background_anchors.min().item<int64_t>()>=610&&balanced.hard_background_anchors.numel()==200,"STRATIFIED_CONTROL_OR_BUDGET");
    const auto& ids=balanced.hard_background_anchors;
    TORCH_CHECK((ids<300).sum().item<int64_t>()==40&&((ids>=300)&(ids<600)).sum().item<int64_t>()==40&&(ids>=600).sum().item<int64_t>()==120,"STRATIFIED_QUOTAS");
    TORCH_CHECK(!(ids.ge(600)&ids.lt(610)).any().item<bool>(),"STRATIFIED_SELECTED_POSITIVE");
    balanced.total.backward();TORCH_CHECK(torch::isfinite(logits.grad()).all().item<bool>(),"STRATIFIED_NONFINITE_GRADIENT");
    auto sparse_target=torch::zeros({13,7});sparse_target.index_put_({2,1},1);
    auto sparse=loss(torch::zeros({13,7}),sparse_target,true,{1,1,11});
    auto sorted=std::get<0>(sparse.hard_background_anchors.sort());
    TORCH_CHECK(sorted.numel()==12&&torch::equal(sorted,torch::nonzero(sparse.positive_anchors.logical_not()).squeeze(1)),"STRATIFIED_SCARCITY_DUPLICATE_OR_LOSS");
}
}
