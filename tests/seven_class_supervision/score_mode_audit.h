#pragma once
// Score-only counterfactual; each mode starts from a fresh immutable checkpoint.
namespace score_mode_audit {
using namespace supervision_fixture;
inline int run(int argc,char** argv){
    check(argc==7,"usage: --score-mode-audit binding parent_root candidate_root output_base candidate_arm");
    auto binding=mode_budget::json(argv[2]);auto parent=fs::canonical(argv[3]),source=fs::canonical(argv[4]);std::string arm=argv[6];
    check(arm=="unified_v12_foreground_competition_experimental"||arm=="unified_v13_sigmoid_background_experimental","SCORE_MODE_ARM");
    auto pm=mode_budget::json(parent/"model_manifest.json");
    check(mode_budget::hash(parent/"model_manifest.json")==binding.at("parent_manifest_sha256")&&
        mode_budget::hash(parent/pm.at("weights").get<std::string>())==binding.at("checkpoint_sha256")&&
        mode_budget::hash(parent/binding.at("attestation_reference").get<std::string>())==binding.at("attestation_sha256")&&
        mode_budget::hash(parent/binding.at("lineage_reference").get<std::string>())==binding.at("source_lineage_sha256")&&pm.at("classes")==binding.at("geometry_contract"),"SCORE_MODE_PARENT_BINDING");
    auto before=mode_budget::inventory(source);TorchModelManifest manifest;std::string reason;
    check(LoadTorchModelManifest((parent/"model_manifest.json").string(),parent.string(),manifest,reason),"SCORE_MODE_MANIFEST");
    auto root=fs::canonical(fresh_root(argv[5]));J rows=J::array();std::cout<<"Score modes: "<<root<<std::endl;
    auto started=std::chrono::steady_clock::now();
    for(const std::string shape:{"circle","ellipse"}){
        auto db=mode_budget::json(source/shape/"materialization_binding.json");BusinessTrialSettings settings;
        settings.case_id=db.at("case_id");settings.dataset_revision_id=db.at("dataset_revision_id");settings.dataset_manifest_sha256=db.at("dataset_manifest_sha256");
        settings.project_geometry_class=shape;settings.project_geometry_class_id=shape=="circle"?1:2;
        BusinessMaterializedDataset data;check(LoadBusinessMaterializedDataset(db.at("training_root"),settings,data,reason),"SCORE_MODE_DATASET");
        auto dir=source/(shape+"-"+arm+"-coverage-v1");auto receipt=mode_budget::json(dir/"result.json");auto sha=mode_budget::hash(dir/"weights.pt");
        check(sha==receipt.at("checkpoint_sha256")&&receipt.at("parent_sha256")==binding.at("checkpoint_sha256"),"SCORE_MODE_CHECKPOINT");
        std::vector<YoloV8SegDatasetSample> samples={data.train_samples.at(0),data.validation_samples.at(0)};
        for(const auto& sample:samples){torch::Tensor reference;
            for(const std::string mode:{"eval","train","train_bn_eval"}){
                check(std::chrono::steady_clock::now()-started<std::chrono::minutes(5),"SCORE_MODE_BUDGET");
                YoloV8Segment model(7);check(model->load_state_dict_strict((dir/"weights.pt").string()).complete(),"SCORE_MODE_LOAD");
                if(mode=="eval")model->eval();else model->train();
                if(mode=="train_bn_eval")for(const auto& module:model->modules())if(auto bn=std::dynamic_pointer_cast<torch::nn::BatchNorm2dImpl>(module))bn->eval();
                auto buffers=mode_budget::buffers(model);torch::NoGradGuard guard;auto image=cv::imread(sample.image_ref);check(!image.empty(),"SCORE_MODE_IMAGE");
                SegLetterbox lb;auto input=MakeSegInput(image,manifest,lb);auto raw=model->forward(input);
                auto table=BuildSegTaskAlignedAnchorTable(raw,model,manifest.input_width,manifest.input_height);
                auto logits=table.class_logits[0];if(mode=="eval")reference=logits.clone();double delta=(logits-reference).abs().max().item<double>();
                if(mode=="train_bn_eval")check(delta<1e-6,"SCORE_MODE_NON_BN_DIFFERENCE");
                if(mode!="train")check(mode_budget::buffer_delta(model,buffers)==0,"SCORE_MODE_EVAL_BUFFER_CHANGE");
                const auto& b=sample.boxes_xyxy_norm.at(0);auto centers=table.centers_xy[0];
                const float x0=float((lb.pad_x+b[0]*lb.resized_width)/manifest.input_width),x1=float((lb.pad_x+b[2]*lb.resized_width)/manifest.input_width);
                const float y0=float((lb.pad_y+b[1]*lb.resized_height)/manifest.input_height),y1=float((lb.pad_y+b[3]*lb.resized_height)/manifest.input_height);
                auto outside=(centers.select(1,0)<x0)|(centers.select(1,0)>x1)|(centers.select(1,1)<y0)|(centers.select(1,1)>y1);
                auto scores=logits.sigmoid().max(1);auto maxima=std::get<0>(scores);J levels=J::array();int64_t offset=0;
                for(std::size_t l=0;l<raw.class_logits.size();++l){int64_t count=raw.class_logits[l].size(2)*raw.class_logits[l].size(3);
                    auto values=maxima.slice(0,offset,offset+count),bg=values.masked_select(outside.slice(0,offset,offset+count));
                    levels.push_back({{"level",l},{"anchors",count},{"above_threshold",(values>=manifest.confidence_threshold).sum().item<int64_t>()},
                        {"outside_gt_box_anchors",bg.numel()},{"outside_gt_box_above_threshold",(bg>=manifest.confidence_threshold).sum().item<int64_t>()},
                        {"outside_gt_box_mean_max_sigmoid",bg.numel()?J(bg.mean().item<double>()):J(nullptr)}});offset+=count;
                }
                rows.push_back({{"shape",shape},{"sample_id",sample.image_id},{"split",sample.split},{"mode",mode},{"arm",arm},
                    {"checkpoint_sha256",sha},{"image_sha256",mode_budget::hash(sample.image_ref)},{"levels",levels},
                    {"above_threshold",(maxima>=manifest.confidence_threshold).sum().item<int64_t>()},{"anchors",maxima.numel()},
                    {"max_class_logit_delta_from_eval",delta},{"max_running_stats_delta",mode_budget::running_stats_delta(model,buffers)}});
                std::cout<<shape<<' '<<sample.split<<' '<<mode<<" above="<<rows.back().at("above_threshold")<<" logit_delta="<<delta<<std::endl;
            }
        }
    }
    check(mode_budget::inventory(source)==before,"SCORE_MODE_SOURCE_CHANGED");
    write_new(root/"score_mode_audit.json",J({{"schema","visionai.rd.score_mode_audit.v1"},{"observations",rows},
        {"source_comparison_sha256",mode_budget::hash(source/"comparison.json")},{"probe_executable_sha256",mode_budget::hash(fs::canonical(argv[0]))},
        {"confidence",manifest.confidence_threshold},{"execution_status","PASS"},{"optimizer_updates",0},
        {"train_mode_is_counterfactual_only",true},{"independent_accuracy_evidence",false},{"production_allowed",false}}).dump(2));
    std::cout<<"PASS score modes: "<<root<<std::endl;return 0;
}
}
