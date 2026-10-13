#pragma once
#include <torch/csrc/autograd/autograd.h>
namespace classification_update_audit {
using namespace supervision_fixture;
inline J tensor_stats(const torch::Tensor& value){
    if(!value.defined())return {{"defined",false},{"l1",nullptr},{"l2",nullptr},{"max_abs",nullptr}};
    auto v=value.detach();check(torch::isfinite(v).all().item<bool>(),"PARAMETER_AUDIT_NONFINITE");
    return {{"defined",true},{"l1",v.abs().sum().item<double>()},{"l2",v.norm().item<double>()},
        {"max_abs",v.abs().max().item<double>()},{"nonzero",(v!=0).sum().item<int64_t>()}};
}
inline J channel_values(const torch::Tensor& value){J r=J::array();for(int64_t i=0;i<value.numel();++i)r.push_back(value[i].item<double>());return r;}
struct Parameter {std::string name;int level;torch::Tensor value,initial,before,expected;};
class Audit {
    std::vector<Parameter> parameters_;
    torch::optim::Adam& optimizer_;
    J micros_=J::array();int update_=0;
public:
    Audit(YoloV8Segment& model,torch::optim::Adam& optimizer):optimizer_(optimizer){
        torch::NoGradGuard guard;
        // Backbone C2f blocks also have a cv3. Identify head tensors by identity,
        // using the actual head module rather than a whole-model name substring.
        std::map<const c10::TensorImpl*,int> head_levels;
        for(const auto& entry:model->head()->named_parameters())if(entry.key().rfind("cv3.",0)==0){
            check(entry.key().size()>5&&entry.key()[4]>='0'&&entry.key()[4]<='2'&&entry.key()[5]=='.',"PARAMETER_AUDIT_HEAD_NAME");
            head_levels[entry.value().unsafeGetTensorImpl()]=entry.key()[4]-'0';}
        for(const auto& entry:model->named_parameters()){
            auto found=head_levels.find(entry.value().unsafeGetTensorImpl());if(found==head_levels.end())continue;
            auto level=found->second;
            check(entry.value().requires_grad(),"PARAMETER_AUDIT_FROZEN_HEAD");int owners=0;
            for(const auto& group:optimizer.param_groups())for(const auto& p:group.params())if(p.unsafeGetTensorImpl()==entry.value().unsafeGetTensorImpl())++owners;
            check(owners==1,"PARAMETER_AUDIT_OPTIMIZER_MEMBERSHIP");
            parameters_.push_back({entry.key(),level,entry.value(),entry.value().detach().clone(),{}, {}});
        }
        check(!parameters_.empty(),"PARAMETER_AUDIT_NO_CLASS_HEAD");
        for(int l=0;l<3;++l){int terminal=0;for(const auto& p:parameters_)if(p.level==l&&(p.name.find(".2.weight")!=std::string::npos||p.name.find(".2.bias")!=std::string::npos)){
            check(p.value.size(0)==7,"PARAMETER_AUDIT_CLASS_CHANNELS");++terminal;}
            check(terminal==2,"PARAMETER_AUDIT_TERMINAL_CONV");}
    }
    void begin_update(int update){torch::NoGradGuard guard;update_=update;micros_=J::array();
        for(auto& p:parameters_){p.before=p.value.detach().clone();p.expected=torch::zeros_like(p.value);}}
    void observe(const YoloV8SegRawOutput& raw,const foreground_competition::Terms& terms,
        const YoloV8SegDatasetSample& sample,const SegLetterbox& lb,const TorchModelManifest& manifest,int micro){
        std::vector<torch::Tensor> inputs=raw.class_logits,old_grad;
        for(const auto& p:parameters_){inputs.push_back(p.value);old_grad.push_back(p.value.grad().defined()?p.value.grad().clone():torch::Tensor());}
        auto centers_x=std::vector<torch::Tensor>{},centers_y=std::vector<torch::Tensor>{};
        for(const auto& level:raw.class_logits){const int64_t h=level.size(2),w=level.size(3);
            centers_x.push_back((torch::arange(w,level.options()).repeat({h})+.5)/w);
            centers_y.push_back((torch::arange(h,level.options()).view({h,1}).repeat({1,w}).reshape({-1})+.5)/h);}
        auto x=torch::cat(centers_x),y=torch::cat(centers_y);auto inside=torch::zeros_like(terms.positive_anchors);
        for(const auto& b:sample.boxes_xyxy_norm)inside|=(x>=(lb.pad_x+b[0]*lb.resized_width)/manifest.input_width)&
            (x<=(lb.pad_x+b[2]*lb.resized_width)/manifest.input_width)&(y>=(lb.pad_y+b[1]*lb.resized_height)/manifest.input_height)&
            (y<=(lb.pad_y+b[3]*lb.resized_height)/manifest.input_height);
        auto hard=torch::zeros_like(inside);hard.index_put_({terms.hard_background_anchors},true);
        J components=J::array();
        const std::vector<std::pair<std::string,torch::Tensor>> losses={
            {"foreground",terms.foreground},{"background",terms.background},{"hard_background_weighted",2*terms.hard_background}};
        for(const auto& component:losses){
            auto gradients=torch::autograd::grad({component.second},inputs,{},true,false,true);J levels=J::array(),params=J::array();int64_t start=0;
            for(std::size_t l=0;l<raw.class_logits.size();++l){const int64_t n=raw.class_logits[l].size(2)*raw.class_logits[l].size(3),end=start+n;
                auto pos=terms.positive_anchors.slice(0,start,end),in=inside.slice(0,start,end),hard_level=hard.slice(0,start,end);
                check(gradients[l].defined(),"PARAMETER_AUDIT_MISSING_OUTPUT_GRAD");
                auto grad=gradients[l].permute({0,2,3,1}).reshape({n,7});auto probabilities=raw.class_logits[l].detach().permute({0,2,3,1}).reshape({n,7}).sigmoid();
                auto max_score=std::get<0>(probabilities.max(1));auto outside=in.logical_not();auto within_unassigned=in&pos.logical_not();J channels=J::array();
                for(int c=0;c<7;++c)channels.push_back(grad.select(1,c).sum().item<double>());
                levels.push_back({{"level",l},{"anchors",n},{"assigned",pos.sum().item<int64_t>()},{"outside_gt",outside.sum().item<int64_t>()},
                    {"unassigned_inside_gt",within_unassigned.sum().item<int64_t>()},{"hard_background",hard_level.sum().item<int64_t>()},
                    {"hard_background_outside_gt",(hard_level&outside).sum().item<int64_t>()},
                    {"output_gradient",tensor_stats(grad)},{"outside_gt_gradient_l1",grad.masked_select(outside.unsqueeze(1)).abs().sum().item<double>()},
                    {"unassigned_inside_gt_gradient_l1",grad.masked_select(within_unassigned.unsqueeze(1)).abs().sum().item<double>()},
                    {"output_gradient_signed_by_class",channels},{"outside_gt_above_threshold",((max_score>=manifest.confidence_threshold)&outside).sum().item<int64_t>()}});start=end;
            }
            for(std::size_t i=0;i<parameters_.size();++i){auto& p=parameters_[i];auto g=gradients[raw.class_logits.size()+i];
                check(g.defined(),"PARAMETER_AUDIT_MISSING_COMPONENT_GRAD");p.expected+=g.detach()/2;
                J r={{"name",p.name},{"level",p.level},{"gradient",tensor_stats(g)}};
                if(p.name.find(".2.bias")!=std::string::npos)r["gradient_by_class"]=channel_values(g);params.push_back(r);}
            components.push_back({{"name",component.first},{"head_output_gradients",levels},{"parameter_gradients",params}});
        }
        for(std::size_t i=0;i<parameters_.size();++i){auto now=parameters_[i].value.grad();
            check(old_grad[i].defined()?now.defined()&&torch::equal(old_grad[i],now):!now.defined(),"PARAMETER_AUDIT_POLLUTED_ACCUMULATION");}
        micros_.push_back({{"microbatch",micro},{"exposure",(update_-1)*2+micro},{"components",components}});
    }
    J before_step(){J rows=J::array();torch::NoGradGuard guard;
        for(const auto& p:parameters_){auto g=p.value.grad();check(g.defined(),"PARAMETER_AUDIT_NO_ACTUAL_GRAD");
            check(torch::allclose(g,p.expected,1e-4,1e-6),"PARAMETER_AUDIT_DECOMPOSITION_MISMATCH");
            rows.push_back({{"name",p.name},{"level",p.level},{"actual_accumulated_gradient",tensor_stats(g)},
                {"decomposition_max_abs_gap",(g-p.expected).abs().max().item<double>()}});}
        return rows;
    }
    J after_step(const J& before){J rows=J::array();torch::NoGradGuard guard;
        for(std::size_t i=0;i<parameters_.size();++i){const auto& p=parameters_[i];auto d=p.value.detach()-p.before;auto initial_norm=p.before.norm().item<double>();
            auto found=optimizer_.state().find(p.value.unsafeGetTensorImpl());check(found!=optimizer_.state().end(),"PARAMETER_AUDIT_ADAM_STATE_MISSING");
            auto* state=dynamic_cast<torch::optim::AdamParamState*>(found->second.get());check(state&&state->step()==update_,"PARAMETER_AUDIT_ADAM_STEP_MISMATCH");
            J r=before[i];r["update"]=tensor_stats(d);r["relative_update_l2"]=initial_norm>0?J(d.norm().item<double>()/initial_norm):J(nullptr);
            r["adam_step"]=state->step();r["adam_first_moment"]=tensor_stats(state->exp_avg());r["adam_second_moment"]=tensor_stats(state->exp_avg_sq());
            if(p.name.find(".2.bias")!=std::string::npos){r["value_before_by_class"]=channel_values(p.before);r["value_after_by_class"]=channel_values(p.value);r["update_by_class"]=channel_values(d);}
            rows.push_back(r);}
        J lrs=J::array();for(const auto& g:optimizer_.param_groups())lrs.push_back(static_cast<const torch::optim::AdamOptions&>(g.options()).lr());
        return {{"update",update_},{"actual_optimizer_lrs",lrs},{"microbatches",micros_},{"parameters",rows},{"optimizer_head_membership_checked",true},
            {"gradient_accumulation_scale",.5},{"component_grad_did_not_modify_accumulation",true}};
    }
    J cumulative(){J rows=J::array();torch::NoGradGuard guard;for(const auto& p:parameters_){auto delta=p.value.detach()-p.initial;
        J r={{"name",p.name},{"level",p.level},{"cumulative_update",tensor_stats(delta)},{"initial_l2",p.initial.norm().item<double>()}};
        if(p.name.find(".2.bias")!=std::string::npos){r["initial_bias_by_class"]=channel_values(p.initial);r["final_bias_by_class"]=channel_values(p.value);r["delta_by_class"]=channel_values(delta);}rows.push_back(r);}return rows;}
};
inline J state_difference(YoloV8Segment& a,YoloV8Segment& b){torch::NoGradGuard guard;double maximum=0;int changed=0;
    auto params=b->named_parameters(),buffers=b->named_buffers();
    for(const auto& p:a->named_parameters()){auto delta=(p.value()-params[p.key()]).abs().max().item<double>();maximum=std::max(maximum,delta);changed+=delta!=0;}
    for(const auto& p:a->named_buffers()){auto delta=(p.value().to(torch::kDouble)-buffers[p.key()].to(torch::kDouble)).abs().max().item<double>();maximum=std::max(maximum,delta);changed+=delta!=0;}
    return {{"max_state_delta",maximum},{"changed_state_tensors",changed},{"exact",maximum==0}};
}
inline void self_test(){
    torch::manual_seed(91);YoloV8Segment model(7);model->train();
    auto one=model->named_parameters().begin()->value();torch::optim::Adam missing(std::vector<torch::Tensor>{one},torch::optim::AdamOptions(1e-4));
    bool rejected=false;try{Audit bad(model,missing);}catch(const std::runtime_error&){rejected=true;}check(rejected,"PARAMETER_AUDIT_MISSING_OPTIMIZER_NOT_REJECTED");
    torch::optim::Adam optimizer(model->parameters(),torch::optim::AdamOptions(1e-4));Audit audit(model,optimizer);audit.begin_update(1);
    TorchModelManifest manifest;manifest.input_width=64;manifest.input_height=64;manifest.num_classes=7;manifest.confidence_threshold=.25;
    YoloV8SegDatasetSample sample;sample.classes={1};sample.boxes_xyxy_norm={{{.25f,.25f,.75f,.75f}}};
    sample.polygons_norm={{{.25f,.25f},{.75f,.25f},{.75f,.75f},{.25f,.75f}}};SegLetterbox lb;lb.resized_width=64;lb.resized_height=64;
    optimizer.zero_grad();for(int micro=0;micro<2;++micro){auto input=torch::rand({1,3,64,64});auto raw=model->forward(input);foreground_competition::Terms terms;
        auto loss=unified_experiment::loss(model,raw,sample,input,lb,manifest,nullptr,false,false,false,true,&terms,true);
        audit.observe(raw,terms,sample,lb,manifest,micro);(loss.total_loss/2).backward();}
    auto before=audit.before_step();optimizer.step();auto result=audit.after_step(before);int changed=0;
    for(const auto& p:result.at("parameters"))changed+=p.at("update").at("nonzero").get<int64_t>()>0;check(changed>0,"PARAMETER_AUDIT_NO_REAL_UPDATE");
}
inline int run(int argc,char** argv){
    check(argc==6,"usage: --classification-update-audit binding parent_root frozen_v13_root output_base");
    auto binding=mode_budget::json(argv[2]);auto parent=fs::canonical(argv[3]),source=fs::canonical(argv[4]);auto pm=mode_budget::json(parent/"model_manifest.json");
    const auto weights=parent/pm.at("weights").get<std::string>();
    check(mode_budget::hash(weights)==binding.at("checkpoint_sha256")&&mode_budget::hash(parent/"model_manifest.json")==binding.at("parent_manifest_sha256")&&
        mode_budget::hash(parent/binding.at("attestation_reference").get<std::string>())==binding.at("attestation_sha256")&&
        mode_budget::hash(parent/binding.at("lineage_reference").get<std::string>())==binding.at("source_lineage_sha256")&&pm.at("classes")==binding.at("geometry_contract"),"PARAMETER_AUDIT_PARENT_BINDING");
    auto source_before=mode_budget::inventory(source),parent_before=mode_budget::inventory(parent);auto root=fs::canonical(fresh_root(argv[5]));
    TorchModelManifest manifest;std::string reason;check(LoadTorchModelManifest((parent/"model_manifest.json").string(),parent.string(),manifest,reason),"PARAMETER_AUDIT_MANIFEST");
    std::cout<<"Parameter audit: "<<root<<std::endl;J results=J::array();const auto started=std::chrono::steady_clock::now();
    for(const std::string shape:{"circle","ellipse"}){
        auto db=mode_budget::json(source/shape/"materialization_binding.json");BusinessTrialSettings settings;settings.case_id=db.at("case_id");
        settings.dataset_revision_id=db.at("dataset_revision_id");settings.dataset_manifest_sha256=db.at("dataset_manifest_sha256");
        settings.project_geometry_class=shape;settings.project_geometry_class_id=shape=="circle"?1:2;BusinessMaterializedDataset data;
        check(LoadBusinessMaterializedDataset(db.at("training_root"),settings,data,reason)&&data.train_samples.size()==3&&data.validation_samples.size()==1,"PARAMETER_AUDIT_DATASET");
        auto dir=source/(shape+"-unified_v13_sigmoid_background_experimental-coverage-v1");auto receipt=mode_budget::json(dir/"result.json");
        check(receipt.at("parent_sha256")==binding.at("checkpoint_sha256")&&receipt.at("classification_policy")=="foreground_competition_sigmoid_background_v1"&&
            receipt.at("seed")==20261012&&receipt.at("epochs")==40&&receipt.at("updates")==40&&receipt.at("batch_size")==2&&receipt.at("microbatch_size")==1&&
            receipt.at("gradient_accumulation_steps")==2&&mode_budget::hash(dir/"weights.pt")==receipt.at("checkpoint_sha256"),"PARAMETER_AUDIT_RECEIPT");
        auto revision=mode_budget::json(source/(shape+"-augmentation/revision.json"));
        check("sha256:"+Sha256Bytes(revision.dump())==receipt.at("coverage_revision_sha256")&&revision.at("policy_sha256")=="sha256:"+Sha256Bytes(coverage_augmentation::policy().dump())&&
            revision.at("exposures").size()==80,"PARAMETER_AUDIT_COVERAGE_REVISION");
        std::vector<YoloV8SegDatasetSample> exposures;
        for(int i=0;i<80;++i){auto s=data.train_samples[i%3];auto path=source/(shape+"-augmentation")/("exposure-"+std::to_string(i)+".png");auto recorded=revision.at("exposures")[i];
            check(mode_budget::hash(path)==recorded.at("image_sha256")&&mode_budget::hash(s.image_ref)==recorded.at("source_sha256"),"PARAMETER_AUDIT_EXPOSURE_SHA");
            auto applied=coverage_augmentation::apply(cv::imread(s.image_ref,cv::IMREAD_GRAYSCALE),s.polygons_norm,coverage_augmentation::plan(i));
            check(cv::norm(applied.image,cv::imread(path.string(),cv::IMREAD_GRAYSCALE),cv::NORM_INF)==0,"PARAMETER_AUDIT_EXPOSURE_CHANGED");
            s.image_ref=path.string();s.polygons_norm=applied.polygons;s.boxes_xyxy_norm=applied.boxes;s.target_mask_ref.clear();s.geometry_facts_ref.clear();
            s.training_target_ref.clear();s.metrology_target_ref.clear();s.boundary_map_ref.clear();s.geometry_target_valid.fill(0);exposures.push_back(s);}
        auto output=root/shape;fs::create_directory(output);std::ofstream progress(output/"progress.jsonl");check(bool(progress),"PARAMETER_AUDIT_PROGRESS");
        torch::manual_seed(20261012);YoloV8Segment model(7);check(model->load_state_dict_strict(weights.string()).complete(),"PARAMETER_AUDIT_PARENT_LOAD");model->train();
        torch::optim::Adam optimizer(model->parameters(),torch::optim::AdamOptions(1e-4).weight_decay(0));Audit audit(model,optimizer);J trace=J::array();
        auto compute=[&](const YoloV8SegDatasetSample& s,int micro){auto image=cv::imread(s.image_ref);check(!image.empty(),"PARAMETER_AUDIT_IMAGE");SegLetterbox lb;auto input=MakeSegInput(image,manifest,lb);
            auto raw=model->forward(input);foreground_competition::Terms terms;auto loss=unified_experiment::loss(model,raw,s,input,lb,manifest,nullptr,false,false,false,true,&terms,true);
            if(micro>=0)audit.observe(raw,terms,s,lb,manifest,micro);return loss;};
        for(int epoch=0;epoch<40;++epoch){check(std::chrono::steady_clock::now()-started<std::chrono::minutes(20),"PARAMETER_AUDIT_BUDGET");
            double lr=1e-6+(1e-4-1e-6)*.5*(1+std::cos(std::acos(-1.)*epoch/39.));
            for(auto& group:optimizer.param_groups())static_cast<torch::optim::AdamOptions&>(group.options()).lr(lr);optimizer.zero_grad();audit.begin_update(epoch+1);double training=0;
            for(int micro=0;micro<2;++micro){auto loss=compute(exposures[epoch*2+micro],micro);check(torch::isfinite(loss.total_loss).item<bool>(),"PARAMETER_AUDIT_NONFINITE_LOSS");
                training+=loss.total_loss.item<double>()/2;(loss.total_loss/2).backward();}
            auto before=audit.before_step();optimizer.step();auto record=audit.after_step(before);model->eval();double valid;
            {torch::NoGradGuard guard;valid=compute(data.validation_samples[0],-1).total_loss.item<double>();}model->train();
            record["training_loss"]=training;record["validation_loss"]=valid;trace.push_back(record);progress<<record.dump()<<'\n';progress.flush();check(bool(progress),"PARAMETER_AUDIT_PROGRESS_WRITE");
            if(epoch%10==9)std::cout<<shape<<" audited update="<<epoch+1<<" loss="<<training<<std::endl;
        }
        write_new(output/"trace.json",trace.dump(2));check(WriteBusinessCheckpoint(model,output/"replayed_weights.pt"),"PARAMETER_AUDIT_SAVE");
        YoloV8Segment reference(7),reloaded(7);check(reference->load_state_dict_strict((dir/"weights.pt").string()).complete()&&reloaded->load_state_dict_strict((output/"replayed_weights.pt").string()).complete(),"PARAMETER_AUDIT_RELOAD");
        auto replay=state_difference(model,reference),reload=state_difference(model,reloaded);
        check(replay.at("exact").get<bool>(),"PARAMETER_AUDIT_REPLAY_NOT_EXACT");check(reload.at("exact").get<bool>(),"PARAMETER_AUDIT_RELOAD_NOT_EXACT");
        J result={{"shape",shape},{"updates",40},{"train_exposures",80},{"reference_checkpoint_sha256",receipt.at("checkpoint_sha256")},
            {"replayed_checkpoint_sha256",mode_budget::hash(output/"replayed_weights.pt")},{"state_vs_frozen_reference",replay},{"state_vs_reload",reload},
            {"cumulative_head_updates",audit.cumulative()},{"trace_sha256",mode_budget::hash(output/"trace.json")},{"progress_sha256",mode_budget::hash(output/"progress.jsonl")}};
        write_new(output/"result.json",result.dump(2));results.push_back(result);
    }
    check(mode_budget::inventory(source)==source_before&&mode_budget::inventory(parent)==parent_before,"PARAMETER_AUDIT_SOURCE_MUTATION");
    write_new(root/"classification_update_audit.json",J({{"schema","visionai.rd.classification_update_audit.v1"},{"results",results},
        {"source_comparison_sha256",mode_budget::hash(source/"comparison.json")},{"parent_manifest_sha256",binding.at("parent_manifest_sha256")},
        {"class_order",manifest.class_names},{"seed",20261012},{"optimizer_updates_total",80},{"probe_executable_sha256",mode_budget::hash(fs::canonical(argv[0]))},
        {"execution_status","PASS"},{"replay_only",true},{"independent_accuracy_evidence",false},{"model_quality_accepted",false},{"production_allowed",false}}).dump(2));
    std::cout<<"PASS classification parameter audit: "<<root<<std::endl;return 0;
}
}
