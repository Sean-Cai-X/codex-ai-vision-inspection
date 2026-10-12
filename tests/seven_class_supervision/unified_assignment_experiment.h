// R&D-only unified assignment experiment. Not a production runtime default.
namespace unified_experiment {
inline torch::Tensor overlap(const torch::Tensor& a,const torch::Tensor& b) {
    auto inter=(torch::minimum(a.slice(1,2,4),b.slice(1,2,4))-torch::maximum(a.slice(1,0,2),b.slice(1,0,2))).clamp_min(0).prod(1);
    auto aa=(a.slice(1,2,4)-a.slice(1,0,2)).clamp_min(0).prod(1),bb=(b.slice(1,2,4)-b.slice(1,0,2)).clamp_min(0).prod(1);
    return inter/(aa+bb-inter+1e-7);
}
// Each anchor receives at most one GT. Ties retain the first GT deterministically.
inline torch::Tensor assign(const SegTaskAlignedAnchorTable& t,const torch::Tensor& boxes,
                            const std::vector<int64_t>& classes,int topk=10,bool per_level=false) {
    torch::NoGradGuard guard;
    const auto centers=t.centers_xy.index({0});const auto predicted=t.decoded_boxes.index({0});
    const auto scores=t.class_logits.index({0}).sigmoid();const auto n=centers.size(0);
    auto owner=torch::full({n},-1,torch::TensorOptions().dtype(torch::kLong).device(centers.device()));
    auto winning=torch::full({n},-1.0,centers.options());
    for(std::size_t g=0;g<classes.size();++g){
        auto box=boxes.index({static_cast<int64_t>(g)}).unsqueeze(0).expand({n,4});
        auto inside=(centers.select(1,0)>box.select(1,0))&(centers.select(1,0)<box.select(1,2))&
            (centers.select(1,1)>box.select(1,1))&(centers.select(1,1)<box.select(1,3));
        auto iou=overlap(predicted,box);auto metric=scores.select(1,classes[g])*iou.pow(6);
        metric=torch::where(inside&(iou>0),metric,torch::full_like(metric,-1));
        auto select=[&](int64_t begin,int64_t end,int quota){
            auto best=metric.slice(0,begin,end).topk(std::min<int64_t>(quota,end-begin));
            auto values=std::get<0>(best),indices=std::get<1>(best);
            for(int64_t i=0;i<indices.size(0);++i){auto idx=begin+indices[i].item<int64_t>();auto v=values[i].item<double>();
                if(v>=0&&v>winning[idx].item<double>()){winning.index_put_({idx},v);owner.index_put_({idx},static_cast<int64_t>(g));}}};
        if(!per_level)select(0,n,topk);
        else {
            const auto widths=t.grid_widths.index({0}).to(torch::kCPU).contiguous();
            const auto heights=t.grid_heights.index({0}).to(torch::kCPU).contiguous();
            const auto w=widths.accessor<float,1>(),h=heights.accessor<float,1>();
            for(int64_t begin=0;begin<n;){int64_t end=begin+1;
                while(end<n&&w[end]==w[begin]&&h[end]==h[begin])++end;
                select(begin,end,end==n?topk:1);begin=end;}
        }
    }
    return owner;
}
inline BusinessLossTensors loss(YoloV8Segment& model,const YoloV8SegRawOutput& raw,
    const YoloV8SegDatasetSample& sample,const torch::Tensor& input,const SegLetterbox& lb,
    const TorchModelManifest& manifest,int64_t* positive_count=nullptr,bool per_level=false) {
    auto t=BuildSegTaskAlignedAnchorTable(raw,model,manifest.input_width,manifest.input_height);
    std::vector<float> coords;
    for(const auto& b:sample.boxes_xyxy_norm)coords.insert(coords.end(),{
        float((lb.pad_x+b[0]*lb.resized_width)/manifest.input_width),float((lb.pad_y+b[1]*lb.resized_height)/manifest.input_height),
        float((lb.pad_x+b[2]*lb.resized_width)/manifest.input_width),float((lb.pad_y+b[3]*lb.resized_height)/manifest.input_height)});
    auto boxes=torch::tensor(coords,input.options()).view({-1,4});
    auto owners=assign(t,boxes,sample.classes,10,per_level);auto positive=owners>=0;auto indices=torch::nonzero(positive).squeeze(1);
    TORCH_CHECK(indices.numel()>0,"UNIFIED_NO_POSITIVE_ASSIGNMENT");
    if(positive_count)*positive_count=indices.numel();
    auto gt=owners.index_select(0,indices);auto logits=t.class_logits.index({0});auto target=torch::zeros_like(logits);
    auto labels=torch::tensor(sample.classes,torch::TensorOptions().dtype(torch::kLong).device(input.device()));
    target.index_put_({indices,labels.index_select(0,gt)},1.0);
    auto bce=torch::nn::functional::binary_cross_entropy_with_logits(logits,target,
        torch::nn::functional::BinaryCrossEntropyWithLogitsFuncOptions().reduction(torch::kNone));
    auto weighted=bce*(logits.sigmoid()-target).abs().pow(2);
    // There is one positive class logit among seven channels per assigned anchor.
    // Balance individual positive logits against all negative logits; averaging
    // channels first would dilute the target-class gradient by seven.
    auto negatives=weighted.masked_fill(target>0,-1).reshape({-1});
    const auto hard_count=std::min<int64_t>(negatives.numel()-target.sum().item<int64_t>(),
        std::max<int64_t>(1,indices.numel()*20));
    auto hard_negative=std::get<0>(negatives.topk(hard_count)).mean();
    BusinessLossTensors r;r.class_loss=(weighted*target).sum()/target.sum().clamp_min(1)+
        (weighted*(1-target)).sum()/(1-target).sum().clamp_min(1)+2*hard_negative;
    auto target_boxes=boxes.index_select(0,gt);auto predicted=t.decoded_boxes.index({0}).index_select(0,indices);
    auto matched_iou=overlap(predicted,target_boxes);
    auto quality=per_level?matched_iou.detach().clamp_min(.05):torch::ones_like(matched_iou);
    auto quality_mean=[&](const torch::Tensor& values){return (values*quality).sum()/quality.sum().clamp_min(1e-7);};
    r.box_loss=per_level?quality_mean(1-matched_iou):(1-matched_iou).mean();
    auto centers=t.centers_xy.index({0}).index_select(0,indices);
    auto w=t.grid_widths.index({0}).index_select(0,indices),h=t.grid_heights.index({0}).index_select(0,indices);
    auto distances=torch::stack({(centers.select(1,0)-target_boxes.select(1,0))*w,(centers.select(1,1)-target_boxes.select(1,1))*h,
        (target_boxes.select(1,2)-centers.select(1,0))*w,(target_boxes.select(1,3)-centers.select(1,1))*h},1).clamp(0,14.999);
    auto left=distances.floor().to(torch::kLong).reshape({-1});auto right=left+1;auto weight=(distances-distances.floor()).reshape({-1});
    auto dfl=t.dfl_logits.index({0}).index_select(0,indices).reshape({-1,16});
    auto options=torch::nn::functional::CrossEntropyFuncOptions().reduction(torch::kNone);
    auto dfl_per_distance=(1-weight)*torch::nn::functional::cross_entropy(dfl,left,options)+
        weight*torch::nn::functional::cross_entropy(dfl,right,options);
    r.dfl_loss=per_level?quality_mean(dfl_per_distance.reshape({-1,4}).mean(1)):dfl_per_distance.mean();
    std::vector<torch::Tensor> masks;for(const auto& p:sample.polygons_norm)masks.push_back(BusinessMaskTarget(p,raw.prototypes.size(3),raw.prototypes.size(2),lb,manifest,input.options()));
    auto mask_targets=torch::stack(masks).index_select(0,gt).flatten(1);
    auto mask_logits=torch::matmul(t.mask_coefficients.index({0}).index_select(0,indices),raw.prototypes.index({0}).flatten(1));
    auto probability=mask_logits.sigmoid();
    auto dice=1-(2*(probability*mask_targets).sum(1)+1)/(probability.sum(1)+mask_targets.sum(1)+1);
    if(per_level) {
        auto mask_bce=torch::nn::functional::binary_cross_entropy_with_logits(mask_logits,mask_targets,
            torch::nn::functional::BinaryCrossEntropyWithLogitsFuncOptions().reduction(torch::kNone)).mean(1);
        r.mask_loss=quality_mean(mask_bce+dice);
    } else r.mask_loss=torch::binary_cross_entropy_with_logits(mask_logits,mask_targets)+dice.mean();
    r.total_loss=r.class_loss+r.mask_loss+r.box_loss*.25+r.dfl_loss*.25;return r;
}
}
