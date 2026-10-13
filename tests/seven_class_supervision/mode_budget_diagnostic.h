// Test-only controlled experiment; production ABI and behavior are unchanged.
#include "legacy_center_loss_reference.h"
namespace mode_budget {
using namespace supervision_fixture;
inline std::string read(const fs::path& p) {std::ifstream f(p,std::ios::binary);check(bool(f),"DIAG_INPUT_UNREADABLE");return {std::istreambuf_iterator<char>(f),{}};}
inline J json(const fs::path& p){return cxvision::supervision::ParseJson(read(p));}
inline std::string hash(const fs::path& p){return "sha256:"+Sha256Bytes(read(p));}
inline J inventory(const fs::path& p){J j=J::object();for(const auto& e:fs::recursive_directory_iterator(p))if(e.is_regular_file())j[e.path().lexically_relative(p).generic_string()]=hash(e.path());return j;}
inline std::vector<torch::Tensor> buffers(YoloV8Segment& model){std::vector<torch::Tensor> r;for(const auto& b:model->named_buffers())r.push_back(b.value().clone());return r;}
inline double buffer_delta(YoloV8Segment& model,const std::vector<torch::Tensor>& before){double d=0;std::size_t n=0;for(const auto& b:model->named_buffers())d=std::max(d,(b.value().to(torch::kDouble)-before.at(n++).to(torch::kDouble)).abs().max().item<double>());return d;}
inline double running_stats_delta(YoloV8Segment& model,const std::vector<torch::Tensor>& before){double d=0;std::size_t n=0;for(const auto& b:model->named_buffers()){if(b.key().find("running_mean")!=std::string::npos||b.key().find("running_var")!=std::string::npos)d=std::max(d,(b.value()-before.at(n)).abs().max().item<double>());++n;}return d;}
inline int run(int argc,char** argv) {
    check(argc==6,"usage: --mode-budget binding parent_root prior_40_update_trial output_base");
    auto binding=json(argv[2]);auto parent=fs::canonical(argv[3]),trial=fs::canonical(argv[4]);
    auto pm=json(parent/"model_manifest.json"),prior=json(trial/"trial_summary.json");
    auto parent_weights=parent/pm.at("weights").get<std::string>();
    check(hash(parent/"model_manifest.json")==binding.at("parent_manifest_sha256")&&hash(parent_weights)==binding.at("checkpoint_sha256")&&
        hash(parent/binding.at("attestation_reference").get<std::string>())==binding.at("attestation_sha256")&&
        hash(parent/binding.at("lineage_reference").get<std::string>())==binding.at("source_lineage_sha256"),"DIAG_PARENT_BINDING_MISMATCH");
    check(pm.at("classes")==binding.at("geometry_contract"),"DIAG_CLASSES_MISMATCH");
    const auto parent_inventory=inventory(parent),trial_inventory=inventory(trial);
    TorchModelManifest manifest;std::string reason;
    check(LoadTorchModelManifest((parent/"model_manifest.json").string(),parent.string(),manifest,reason),"DIAG_MANIFEST_FAILED");
    BusinessTrialSettings settings;check(ParseBusinessTrialSettings(read(trial/"effective_training_request.json"),settings),"DIAG_SETTINGS_INVALID");
    check(settings.epoch_count==20&&settings.iteration_count==40&&settings.batch_size==1&&settings.lr_schedule=="cosine","DIAG_FIXED_BUDGET_REQUIRED");
    BusinessMaterializedDataset dataset;check(LoadBusinessMaterializedDataset((trial/"ellipse/training").string(),settings,dataset,reason),"DIAG_DATASET_INVALID");
    check(dataset.train_samples.size()==2&&dataset.validation_samples.size()==1,"DIAG_SPLIT_INVALID");
    const auto root=fs::canonical(fresh_root(argv[5]));
    std::cout<<"Diagnostic output: "<<root<<std::endl;
    J traces=J::object(),model_paths=J::object();model_paths["parent"]=parent_weights.string();
    const auto started=std::chrono::steady_clock::now();
    auto deadline=[&](){check(std::chrono::steady_clock::now()-started<std::chrono::minutes(10),"DIAG_TIME_BUDGET");};
    for(int version=0;version<2;++version) {
        const std::string label=version?"dense_v2":"legacy_v1";
        torch::manual_seed(20261010);YoloV8Segment model(7);
        check(model->load_state_dict_strict(parent_weights.string()).complete(),"DIAG_STRICT_LOAD");model->train();
        torch::optim::Adam optimizer(model->parameters(),torch::optim::AdamOptions(settings.learning_rate).weight_decay(settings.weight_decay));
        auto loss=[&](const YoloV8SegDatasetSample& sample){
            auto image=cv::imread(sample.image_ref);SegLetterbox lb;auto input=MakeSegInput(image,manifest,lb);
            auto raw=model->forward(input);
            return version?ComputeBusinessYoloV8SegLoss(model,raw,sample,input,lb,manifest):LegacyBusinessYoloV8SegLoss(model,raw,sample,input,lb,manifest);
        };
        J trace=J::array();
        for(unsigned epoch=0;epoch<20;++epoch){
            deadline();double lr=settings.min_learning_rate+(settings.learning_rate-settings.min_learning_rate)*.5*(1+std::cos(std::acos(-1.0)*epoch/19.0));
            for(auto& group:optimizer.param_groups())static_cast<torch::optim::AdamOptions&>(group.options()).lr(lr);
            double train_loss=0;model->train();
            for(const auto& sample:dataset.train_samples){optimizer.zero_grad();auto l=loss(sample).total_loss;check(torch::isfinite(l).item<bool>(),"DIAG_NONFINITE_LOSS");train_loss+=l.item<double>();l.backward();optimizer.step();}
            model->eval();double valid_loss=0;{torch::NoGradGuard guard;valid_loss=loss(dataset.validation_samples.front()).total_loss.item<double>();}
            model->train();trace.push_back({{"epoch",epoch+1},{"updates",2*(epoch+1)},{"lr",lr},{"train_loss",train_loss/2},{"valid_loss",valid_loss}});
            std::cout<<label<<" epoch "<<epoch+1<<" train "<<train_loss/2<<" valid "<<valid_loss<<std::endl;
        }
        auto checkpoint=root/(label+".pt");check(WriteBusinessCheckpoint(model,checkpoint),"DIAG_CHECKPOINT_FAILED");
        model_paths[label]=checkpoint.string();traces[label]=trace;
        write_new(root/(label+"_trace.json"),trace.dump(2));
    }
    // Require reproduction of the previous real C ABI run before interpreting
    // the controlled loop as an old/new-loss comparison of that experiment.
    auto old_candidate=trial/"output/development_trials/ellipse-train/staging/candidate";
    auto old_manifest=json(old_candidate/"model_manifest.json");
    auto old_weights=old_candidate/old_manifest.at("weights").get<std::string>();
    check(hash(old_weights)==prior.at("candidate_model_sha256"),"DIAG_PRIOR_CANDIDATE_DIGEST");
    YoloV8Segment reproduced(7),reference(7);
    check(reproduced->load_state_dict_strict(model_paths["dense_v2"].get<std::string>()).complete()&&reference->load_state_dict_strict(old_weights.string()).complete(),"DIAG_REPRODUCTION_LOAD");
    double state_delta=0;auto rp=reference->named_parameters(),rb=reference->named_buffers();
    for(const auto& p:reproduced->named_parameters())state_delta=std::max(state_delta,(p.value()-rp[p.key()]).abs().max().item<double>());
    for(const auto& b:reproduced->named_buffers())state_delta=std::max(state_delta,(b.value().to(torch::kDouble)-rb[b.key()].to(torch::kDouble)).abs().max().item<double>());
    write_new(root/"reproduction.json",J({{"max_state_delta",state_delta},{"prior_candidate_sha256",prior.at("candidate_model_sha256")}}).dump(2));
    check(state_delta==0,"DIAG_C_ABI_STATE_NOT_REPRODUCED");
    J rows=J::array();
    for(const auto& entry:model_paths.items())for(int sample=0;sample<4;++sample){
        torch::Tensor eval_logits;
        for(const std::string mode:{"eval","train","train_bn_eval"}){
            deadline();YoloV8Segment model(7);check(model->load_state_dict_strict(entry.value().get<std::string>()).complete(),"DIAG_MODE_LOAD");
            if(mode=="eval")model->eval();else model->train();
            unsigned bn_count=0;
            for(const auto& module:model->modules())if(auto bn=std::dynamic_pointer_cast<torch::nn::BatchNorm2dImpl>(module)){++bn_count;if(mode=="train_bn_eval")bn->eval();}
            auto before=buffers(model);torch::NoGradGuard guard;
            auto image=cv::imread((trial/"ellipse"/(sample==3?"verify":"training")/("sample-"+std::to_string(sample)+".pgm")).string());
            SegLetterbox lb;auto input=MakeSegInput(image,manifest,lb);auto raw=model->forward(input);
            auto table=BuildSegTaskAlignedAnchorTable(raw,model,manifest.input_width,manifest.input_height);
            if(mode=="eval")eval_logits=table.class_logits.clone();
            double logit_delta=(table.class_logits-eval_logits).abs().max().item<double>();
            auto candidates=DecodeCandidates(raw,model->head(),manifest);J counts=J::object();double ellipse_iou=0,any_iou=0;int best_class=-1;
            // Synthetic oracle only. VERIFY oracle is never a loader/training input.
            cv::Mat truth;
            if(sample<3)truth=cv::imread((trial/"ellipse/training"/("sample-"+std::to_string(sample)+"-mask.pgm")).string(),cv::IMREAD_GRAYSCALE)==2;
            else {auto source=json(trial/"ellipse/project.json").at("samples")[3];source["split"]="valid";source["annotations"]=J::array({{{"id","oracle"},{"class_name","ellipse"},{"closed",true},{"points",points("ellipse",3)}}});source["annotation_receipt_digest"]=Sha256Bytes(source["annotations"][0].dump());
                auto rules=json(fs::path(__FILE__).parent_path()/"../../contracts/seven_class_supervision_rules.v1.json");auto conversion=cxvision::supervision::Convert(source,rules);truth=(cv::Mat(64,64,CV_8UC1,conversion.mask.data())==2);}
            for(const auto& c:candidates){double q=0,s=0;auto mask=DecodeMask(c,raw.prototypes,manifest,lb,image.size(),q,s);if(LargestContour(mask).size()<3)continue;
                auto name=manifest.class_names[c.class_id];counts[name]=counts.value(name,0)+1;
                cv::Mat intersection,united;cv::bitwise_and(mask,truth,intersection);cv::bitwise_or(mask,truth,united);double overlap=double(cv::countNonZero(intersection))/std::max(1,cv::countNonZero(united));
                if(overlap>any_iou){any_iou=overlap;best_class=c.class_id;}if(c.class_id==2)ellipse_iou=std::max(ellipse_iou,overlap);
            }
            double delta=buffer_delta(model,before);check(mode=="train"||delta==0,"DIAG_EVAL_MUTATED_BUFFERS");
            if(mode=="train_bn_eval")check(logit_delta<1e-6,"DIAG_NON_BN_MODE_DIFFERENCE");
            rows.push_back({{"model",entry.key()},{"mode",mode},{"sample",sample},{"purpose",sample<2?"Train":sample==2?"Valid":"VERIFY"},
                {"class_counts",counts},{"best_ellipse_iou",ellipse_iou},{"best_any_iou",any_iou},{"best_class_id",best_class},
                {"max_class_logit_delta_from_eval",logit_delta},{"max_buffer_delta",delta},{"max_running_stats_delta",running_stats_delta(model,before)},{"batchnorm_modules",bn_count}});
        }
    }
    check(inventory(parent)==parent_inventory&&inventory(trial)==trial_inventory,"DIAG_INPUT_CHANGED");
    J report={{"schema","visionai.mode_budget_diagnostic.v1"},{"execution_status","PASS"},{"seed",20261010},
        {"epochs",20},{"updates",40},{"batch_size",1},{"device","cpu"},{"training_order","two Train samples in frozen loader order"},
        {"learning_rate",settings.learning_rate},{"min_learning_rate",settings.min_learning_rate},{"lr_schedule",settings.lr_schedule},{"weight_decay",settings.weight_decay},
        {"parent_sha256",binding.at("checkpoint_sha256")},{"dataset_revision",settings.dataset_revision_id},
        {"dense_v2_reproduces_c_abi_state_exactly",true},{"old_loss_source_commit","f89ce6a"},
        {"traces",traces},{"observations",rows},{"source_files_unchanged",true},{"production_activation",false},{"model_quality_accepted",false},
        {"independent_accuracy_evidence",false},{"mode_forward_has_no_optimizer_step",true}};
    report["legacy_checkpoint_sha256"]=hash(model_paths["legacy_v1"].get<std::string>());report["dense_checkpoint_sha256"]=hash(model_paths["dense_v2"].get<std::string>());
    report["executable_sha256"]=hash(fs::canonical(argv[0]));
    report["legacy_reference_file_sha256"]=hash(fs::path(__FILE__).parent_path()/"legacy_center_loss_reference.h");
    report["diagnostic_loop_file_sha256"]=hash(__FILE__);
    write_new(root/"mode_budget_report.json",report.dump(2));std::cout<<"PASS evidence: "<<root<<std::endl;return 0;
}
}
