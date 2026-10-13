#pragma once
// R&D-only audit: invoke the unchanged unified loss on detached output leaves.
// This measures d(loss)/d(head output), NOT parameter gradients or optimizer updates.
namespace ownership_audit {
using namespace supervision_fixture;
inline torch::Tensor flatten_head_gradient(const torch::Tensor& value) {
    return value.permute({0,2,3,1}).reshape({-1,value.size(1)});
}
inline J gradients(YoloV8Segment& model,const YoloV8SegRawOutput& source,
    const YoloV8SegDatasetSample& sample,const torch::Tensor& input,
    const SegLetterbox& lb,const TorchModelManifest& manifest) {
    YoloV8SegRawOutput raw;
    for(const auto& v:source.class_logits)raw.class_logits.push_back(v.detach().clone().set_requires_grad(true));
    for(const auto& v:source.box_logits)raw.box_logits.push_back(v.detach().clone().set_requires_grad(true));
    for(const auto& v:source.mask_coefficients)raw.mask_coefficients.push_back(v.detach().clone().set_requires_grad(true));
    raw.prototypes=source.prototypes.detach().clone().set_requires_grad(true);
    auto table=BuildSegTaskAlignedAnchorTable(raw,model,manifest.input_width,manifest.input_height);
    std::vector<float> coords;
    for(const auto& b:sample.boxes_xyxy_norm)coords.insert(coords.end(),{
        float((lb.pad_x+b[0]*lb.resized_width)/manifest.input_width),float((lb.pad_y+b[1]*lb.resized_height)/manifest.input_height),
        float((lb.pad_x+b[2]*lb.resized_width)/manifest.input_width),float((lb.pad_y+b[3]*lb.resized_height)/manifest.input_height)});
    auto boxes=torch::tensor(coords,input.options()).view({-1,4});
    auto owners=unified_experiment::assign(table,boxes,sample.classes);
    auto target=torch::zeros_like(table.class_logits.index({0}));
    auto positive=torch::nonzero(owners>=0).squeeze(1);
    for(int64_t i=0;i<positive.numel();++i){auto a=positive[i].item<int64_t>();target.index_put_({a,sample.classes.at(owners[a].item<int64_t>())},1);}
    int64_t count=0;auto loss=unified_experiment::loss(model,raw,sample,input,lb,manifest,&count);
    check(count==positive.numel(),"AUDIT_OWNER_COUNT_MISMATCH");
    loss.total_loss.backward();J levels=J::array();int64_t start=0;
    for(std::size_t l=0;l<raw.class_logits.size();++l){
        auto cls=flatten_head_gradient(raw.class_logits[l].grad()),box=flatten_head_gradient(raw.box_logits[l].grad()),mask=flatten_head_gradient(raw.mask_coefficients[l].grad());
        check(torch::isfinite(cls).all().item<bool>()&&torch::isfinite(box).all().item<bool>()&&torch::isfinite(mask).all().item<bool>(),"AUDIT_NONFINITE_GRADIENT");
        const auto end=start+cls.size(0);auto t=target.slice(0,start,end);auto pos=owners.slice(0,start,end)>=0;
        auto positive_values=cls.masked_select(t>0),negative_values=cls.masked_select(t==0);
        // Zero gradients from saturated logits are recorded, not mistaken for a sign error.
        check((positive_values<=1e-7).all().item<bool>()&&(negative_values>=-1e-7).all().item<bool>(),"AUDIT_CLASS_GRADIENT_SIGN");
        const auto box_active=box.abs().sum(1)>0,mask_active=mask.abs().sum(1)>0;
        check(!(box_active&pos.logical_not()).any().item<bool>()&&!(mask_active&pos.logical_not()).any().item<bool>(),"AUDIT_GEOMETRY_GRADIENT_OUTSIDE_OWNER");
        J by_class=J::array();for(int64_t c=0;c<cls.size(1);++c)by_class.push_back(cls.select(1,c).abs().sum().item<double>());
        levels.push_back({{"level",l},{"anchors",cls.size(0)},{"positive_anchors",pos.sum().item<int64_t>()},
            {"box_active_anchors",box_active.sum().item<int64_t>()},{"mask_active_anchors",mask_active.sum().item<int64_t>()},
            {"positive_class_gradient_l1",positive_values.abs().sum().item<double>()},
            {"positive_zero_gradients",(positive_values==0).sum().item<int64_t>()},
            {"negative_class_gradient_l1",negative_values.abs().sum().item<double>()},{"class_gradient_l1",by_class}});start=end;
    }
    return {{"loss",loss.total_loss.item<double>()},{"class_loss",loss.class_loss.item<double>()},
        {"box_loss",loss.box_loss.item<double>()},{"mask_loss",loss.mask_loss.item<double>()},
        {"dfl_loss",loss.dfl_loss.item<double>()},{"levels",levels},{"gradient_scope","detached_head_outputs_only"}};
}
inline void self_test(){
    torch::manual_seed(17);YoloV8Segment model(7);TorchModelManifest manifest;manifest.input_width=64;manifest.input_height=64;manifest.num_classes=7;
    SegLetterbox lb;lb.resized_width=64;lb.resized_height=64;auto input=torch::zeros({1,3,64,64});
    for(int cls=0;cls<7;++cls){
        YoloV8SegRawOutput raw;
        for(int size:{8,4,2}){
            // Deliberately favor a wrong class; supervision must still push the true class upward.
            auto scores=torch::full({1,7,size,size},-2.0f);scores.select(1,(cls+1)%7).fill_(5);
            raw.class_logits.push_back(scores);raw.box_logits.push_back(torch::randn({1,64,size,size}));
            raw.mask_coefficients.push_back(torch::randn({1,32,size,size}));
        }
        raw.prototypes=torch::randn({1,32,16,16});YoloV8SegDatasetSample sample;
        sample.classes={cls,(cls+1)%7};sample.boxes_xyxy_norm={{{.05f,.05f,.6f,.6f}},{{.4f,.4f,.95f,.95f}}};
        sample.polygons_norm={{{.05f,.05f},{.6f,.05f},{.6f,.6f},{.05f,.6f}},{{.4f,.4f},{.95f,.4f},{.95f,.95f},{.4f,.95f}}};
        auto report=gradients(model,raw,sample,input,lb,manifest);double positive=0;
        for(const auto& level:report.at("levels"))positive+=level.at("positive_class_gradient_l1").get<double>();
        check(positive>0,"AUDIT_POSITIVE_GRADIENT_MISSING");
    }
}
inline int run(int argc,char** argv){
    check(argc==6,"usage: --ownership-audit binding parent_root coverage_root output_base");
    auto binding=mode_budget::json(argv[2]);auto parent=fs::canonical(argv[3]),coverage=fs::canonical(argv[4]);
    auto pm=mode_budget::json(parent/"model_manifest.json");
    check(mode_budget::hash(parent/"model_manifest.json")==binding.at("parent_manifest_sha256")&&
        mode_budget::hash(parent/pm.at("weights").get<std::string>())==binding.at("checkpoint_sha256")&&
        mode_budget::hash(parent/binding.at("attestation_reference").get<std::string>())==binding.at("attestation_sha256")&&
        mode_budget::hash(parent/binding.at("lineage_reference").get<std::string>())==binding.at("source_lineage_sha256"),"AUDIT_PARENT_BINDING");
    auto before=mode_budget::inventory(coverage);TorchModelManifest manifest;std::string reason;
    check(LoadTorchModelManifest((parent/"model_manifest.json").string(),parent.string(),manifest,reason),"AUDIT_MANIFEST");
    check(pm.at("classes")==binding.at("geometry_contract"),"AUDIT_CLASS_ORDER");
    auto root=fs::canonical(fresh_root(argv[5]));J rows=J::array();std::cout<<"Ownership audit: "<<root<<std::endl;
    auto started=std::chrono::steady_clock::now();
    for(const std::string shape:{"circle","ellipse"}){
        auto db=mode_budget::json(coverage/shape/"materialization_binding.json");BusinessTrialSettings settings;
        settings.case_id=db.at("case_id");settings.dataset_revision_id=db.at("dataset_revision_id");
        settings.dataset_manifest_sha256=db.at("dataset_manifest_sha256");settings.project_geometry_class=shape;settings.project_geometry_class_id=shape=="circle"?1:2;
        BusinessMaterializedDataset data;check(LoadBusinessMaterializedDataset(db.at("training_root"),settings,data,reason),"AUDIT_DATASET");
        auto dir=coverage/(shape+"-unified_v5_hard_negative_experimental-coverage-v1");auto receipt=mode_budget::json(dir/"result.json");
        const auto digest=mode_budget::hash(dir/"weights.pt");check(digest==receipt.at("checkpoint_sha256")&&receipt.at("parent_sha256")==binding.at("checkpoint_sha256"),"AUDIT_CHECKPOINT_BINDING");
        std::vector<YoloV8SegDatasetSample> samples={data.train_samples.at(0),data.validation_samples.at(0)};
        for(const auto& sample:samples){torch::Tensor eval_logits;
            for(const std::string mode:{"eval","train","train_bn_eval"}){
                check(std::chrono::steady_clock::now()-started<std::chrono::minutes(5),"AUDIT_TIME_BUDGET");
                // Fresh load prevents a diagnostic train forward contaminating subsequent modes.
                YoloV8Segment model(7);check(model->load_state_dict_strict((dir/"weights.pt").string()).complete(),"AUDIT_STRICT_LOAD");
                if(mode=="eval")model->eval();else model->train();
                if(mode=="train_bn_eval")for(const auto& module:model->modules())if(auto bn=std::dynamic_pointer_cast<torch::nn::BatchNorm2dImpl>(module))bn->eval();
                auto buffers=mode_budget::buffers(model);auto image=cv::imread(sample.image_ref);check(!image.empty(),"AUDIT_IMAGE");
                SegLetterbox lb;auto input=MakeSegInput(image,manifest,lb);YoloV8SegRawOutput raw;
                {torch::NoGradGuard guard;raw=model->forward(input);}
                auto table=BuildSegTaskAlignedAnchorTable(raw,model,manifest.input_width,manifest.input_height);
                if(mode=="eval")eval_logits=table.class_logits.detach().clone();
                double delta=(table.class_logits-eval_logits).abs().max().item<double>();
                if(mode=="train_bn_eval")check(delta<1e-6,"AUDIT_NON_BN_MODE_DIFFERENCE");
                if(mode!="train")check(mode_budget::buffer_delta(model,buffers)==0,"AUDIT_EVAL_BUFFER_MUTATION");
                std::vector<coverage_budget::Prediction> predictions;
                const fs::path image_path(sample.image_ref);
                auto truth_path=image_path.parent_path()/(image_path.stem().string()+"-mask.pgm");
                auto label=cv::imread(truth_path.string(),cv::IMREAD_GRAYSCALE);
                check(!label.empty()&&label.size()==image.size(),"AUDIT_SYNTHETIC_ORACLE_MISSING");
                cv::Mat truth=label==settings.project_geometry_class_id;
                auto candidates=DecodeCandidates(raw,model->head(),manifest);
                for(std::size_t i=0;i<candidates.size();++i){double q=0,s=0;auto mask=DecodeMask(candidates[i],raw.prototypes,manifest,lb,image.size(),q,s);
                    if(LargestContour(mask).size()>=3)predictions.push_back({"candidate-"+std::to_string(i),candidates[i].class_id,candidates[i].score,mask>0});}
                auto metrics=cxvision::mask_error::AnalyzeSingleGroundTruth(image.size(),settings.project_geometry_class_id,truth,predictions);
                auto g=gradients(model,raw,sample,input,lb,manifest);
                rows.push_back({{"shape",shape},{"sample_id",sample.image_id},{"split",sample.split},{"mode",mode},{"checkpoint_sha256",digest},
                    {"image_sha256",mode_budget::hash(sample.image_ref)},{"annotation_sha256",mode_budget::hash(truth_path)},{"max_logit_delta_from_eval",delta},
                    {"max_running_stats_delta",mode_budget::running_stats_delta(model,buffers)},{"gradients",g},{"metrics",metrics}});
                std::cout<<shape<<' '<<sample.split<<' '<<mode<<" logit_delta="<<delta<<" target_iou="<<metrics.at("best_target_iou")<<std::endl;
            }
        }
    }
    check(mode_budget::inventory(coverage)==before,"AUDIT_SOURCE_CHANGED");
    write_new(root/"ownership_audit.json",J({{"schema","visionai.rd.ownership_gradient_audit.v1"},{"observations",rows},
        {"source_comparison_sha256",mode_budget::hash(coverage/"comparison.json")},{"execution_status","PASS"},
        {"optimizer_updates",0},{"training_mode_is_diagnostic_only",true},{"independent_accuracy_evidence",false},
        {"model_quality_accepted",false},{"production_allowed",false}}).dump(2));
    std::cout<<"PASS ownership audit: "<<root<<std::endl;return 0;
}
}
