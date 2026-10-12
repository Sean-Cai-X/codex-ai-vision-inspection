#pragma once
#include "coverage_augmentation.h"
#include "../../cximage/CxInstanceMaskErrorAnalysis.h"
namespace coverage_budget {
using namespace supervision_fixture;
using cxvision::mask_error::Prediction;
inline J evaluate(YoloV8Segment& model,const TorchModelManifest& manifest,const J& inputs){
    model->eval();torch::NoGradGuard guard;J rows=J::array();
    for(const auto& scene:inputs){
        auto image=cv::imread(scene.at("image").get<std::string>());
        auto truth=cv::imread(scene.at("gt").get<std::string>(),cv::IMREAD_GRAYSCALE);
        check(!image.empty()&&!truth.empty(),"COVERAGE_EVAL_INPUT_MISSING");
        check(mode_budget::hash(scene.at("image").get<std::string>())==scene.at("image_sha256")&&mode_budget::hash(scene.at("gt").get<std::string>())==scene.at("annotation_sha256"),"COVERAGE_EVAL_SOURCE_CHANGED");
        SegLetterbox lb;auto raw=model->forward(MakeSegInput(image,manifest,lb));SegNmsDiagnostics nms;
        auto candidates=DecodeCandidates(raw,model->head(),manifest,&nms);std::vector<Prediction> predictions;int rejected=0;
        for(std::size_t i=0;i<candidates.size();++i){const auto& c=candidates[i];double q=0,st=0;
            auto mask=DecodeMask(c,raw.prototypes,manifest,lb,image.size(),q,st);
            if(LargestContour(mask).size()<3){++rejected;continue;}
            predictions.push_back({"candidate-"+std::to_string(i),c.class_id,c.score,mask>0});
        }
        auto metrics=cxvision::mask_error::AnalyzeSingleGroundTruth(image.size(),scene.at("target_class_id"),truth,predictions);
        J record=scene;record.erase("image");record.erase("gt");record["metrics"]=metrics;
        record["nms"]={{"above_threshold",nms.above_threshold},{"examined",nms.examined},{"suppressed_examined",nms.suppressed},
            {"unexamined",nms.above_threshold-nms.examined},{"cap_reached",nms.cap_reached},{"mask_rejected",rejected},
            {"returned_post_nms",candidates.size()},{"full_post_nms_count",nms.examined==nms.above_threshold?J(candidates.size()):J(nullptr)}};
        rows.push_back(record);
    }return rows;
}
inline int run(int argc,char** argv){
    const bool geometry_only=std::string(argv[1])=="--coverage-geometry-budget";
    check(argc==(geometry_only?9:8),"usage: --coverage-budget binding parent_root rules prior_experiment transfer_audit_root output_base [covered_v5_root for --coverage-geometry-budget]");
    auto binding=mode_budget::json(argv[2]),rules=mode_budget::json(argv[4]);auto parent=fs::canonical(argv[3]);
    auto prior=fs::canonical(argv[5]),audit=fs::canonical(argv[6]);auto parent_manifest=mode_budget::json(parent/"model_manifest.json");
    auto weights=parent/parent_manifest.at("weights").get<std::string>();
    check(mode_budget::hash(weights)==binding.at("checkpoint_sha256")&&mode_budget::hash(parent/"model_manifest.json")==binding.at("parent_manifest_sha256")&&
        mode_budget::hash(parent/binding.at("attestation_reference").get<std::string>())==binding.at("attestation_sha256")&&
        mode_budget::hash(parent/binding.at("lineage_reference").get<std::string>())==binding.at("source_lineage_sha256"),"COVERAGE_PARENT_BINDING");
    auto parent_inventory=mode_budget::inventory(parent);auto source_audit=mode_budget::json(audit/"transfer_error_audit.json");
    check(source_audit.at("schema")=="visionai.rd.synthetic_transfer_error_audit.v1"&&source_audit.at("execution_status")=="PASS","COVERAGE_AUDIT_INVALID");
    const auto audit_digest=mode_budget::hash(audit/"transfer_error_audit.json");
    auto root=fs::canonical(fresh_root(argv[7]));std::cout<<"Coverage evidence: "<<root.generic_string()<<std::endl;
    fs::path covered_root;J covered_inventory;
    if(geometry_only){covered_root=fs::canonical(argv[8]);covered_inventory=mode_budget::inventory(covered_root);}
    TorchModelManifest manifest;std::string reason;check(LoadTorchModelManifest((parent/"model_manifest.json").string(),parent.string(),manifest,reason),"COVERAGE_MANIFEST");
    auto policy=coverage_augmentation::policy();const auto policy_sha="sha256:"+Sha256Bytes(policy.dump());
    write_new(root/"coverage_policy.json",policy.dump(2));J results=J::array();auto started=std::chrono::steady_clock::now();
    auto budget=[&](){check(std::chrono::steady_clock::now()-started<std::chrono::minutes(20),"COVERAGE_EXECUTION_BUDGET");};
    for(const std::string shape:{"circle","ellipse"}){
        auto data_binding=make_case(root/shape,rules,shape,"none",3);const auto original_inventory=mode_budget::inventory(root/shape);
        BusinessTrialSettings settings;settings.case_id=data_binding.at("case_id");settings.dataset_revision_id=data_binding.at("dataset_revision_id");
        settings.dataset_manifest_sha256=data_binding.at("dataset_manifest_sha256");settings.project_geometry_class=shape;settings.project_geometry_class_id=shape=="circle"?1:2;
        BusinessMaterializedDataset data;check(LoadBusinessMaterializedDataset(data_binding.at("training_root"),settings,data,reason),"COVERAGE_DATASET_LOADER");
        check(data.train_samples.size()==3&&data.validation_samples.size()==1,"COVERAGE_SPLIT_CHANGED");
        auto augmentation_root=root/(shape+"-augmentation");fs::create_directory(augmentation_root);
        std::vector<YoloV8SegDatasetSample> augmented;J exposures=J::array();
        for(int i=0;i<80;++i){
            auto sample=data.train_samples[i%3];check(sample.split=="train","COVERAGE_NON_TRAIN_SOURCE");
            auto original=cv::imread(sample.image_ref,cv::IMREAD_GRAYSCALE);
            auto applied=coverage_augmentation::apply(original,sample.polygons_norm,coverage_augmentation::plan(i));
            auto path=augmentation_root/("exposure-"+std::to_string(i)+".png");check(cv::imwrite(path.string(),applied.image),"COVERAGE_IMAGE_WRITE");
            auto record=applied.record;record["exposure"]=i;record["source_asset"]=sample.image_id;record["source_split"]="train";
            record["source_sha256"]=mode_budget::hash(sample.image_ref);record["image_sha256"]=mode_budget::hash(path);record["classes"]=sample.classes;
            record["policy_sha256"]=policy_sha;exposures.push_back(record);
            sample.image_ref=path.string();sample.polygons_norm=applied.polygons;sample.boxes_xyxy_norm=applied.boxes;
            sample.target_mask_ref.clear();sample.geometry_facts_ref.clear();sample.training_target_ref.clear();sample.metrology_target_ref.clear();sample.boundary_map_ref.clear();
            sample.geometry_target_valid.fill(0);augmented.push_back(sample);
        }
        J revision={{"base_dataset_revision",data_binding.at("dataset_revision_id")},{"base_manifest_sha256",data_binding.at("dataset_manifest_sha256")},
            {"policy_sha256",policy_sha},{"exposures",exposures}};
        const std::string revision_sha="sha256:"+Sha256Bytes(revision.dump());write_new(augmentation_root/"revision.json",revision.dump(2));
        J inputs=J::array();
        for(const auto& old:source_audit.at("observations"))if(old.at("shape")==shape&&old.at("arm")=="legacy_v1"){
            const std::string scene=old.at("sample_id");
            auto image=audit/shape/(scene+".pgm"),gt=audit/shape/(scene+"-gt.pgm");
            check(mode_budget::hash(image)==old.at("image_sha256")&&mode_budget::hash(gt)==old.at("annotation_sha256"),"COVERAGE_AUDIT_IMAGE_SHA");
            inputs.push_back({{"sample_id",scene},{"source_group",shape+"-train-seed0"},{"split","TRAIN_SOURCE_REGRESSION_STRESS"},
                {"image",image.string()},{"gt",gt.string()},{"image_sha256",old.at("image_sha256")},{"annotation_sha256",old.at("annotation_sha256")},
                {"target_class_id",settings.project_geometry_class_id},{"independent_accuracy_evidence",false}});
        }
        check(inputs.size()==9,"COVERAGE_STRESS_SCENES_MISSING");
        // Frozen nominal Valid remains unaugmented. VERIFY is never consumed by training.
        const auto& valid=data.validation_samples[0];
        // Loader target_mask_ref may be absent; the generated fixture path is authoritative.
        cv::Mat valid_gt=cv::imread((root/shape/"training/sample-3-mask.pgm").string(),cv::IMREAD_GRAYSCALE)==settings.project_geometry_class_id;
        auto valid_mask=augmentation_root/"valid-oracle.png";check(cv::imwrite(valid_mask.string(),valid_gt),"COVERAGE_VALID_MASK_WRITE");
        inputs.push_back({{"sample_id","nominal_valid_seed3"},{"source_group",shape+"-valid-seed3"},{"split","VALID_DIAGNOSTIC_SAME_GENERATOR"},
            {"image",valid.image_ref},{"gt",valid_mask.string()},{"image_sha256",mode_budget::hash(valid.image_ref)},
            {"annotation_sha256",mode_budget::hash(valid_mask)},{"target_class_id",settings.project_geometry_class_id},{"independent_accuracy_evidence",false}});
        for(int version=geometry_only?1:0;version<2;++version){
            const std::string baseline_loss=version?"unified_v5_hard_negative_experimental":"legacy_v1";
            const std::string loss_name=geometry_only?"unified_v11_geometry_assignment_experimental":baseline_loss;
            auto old_root=prior/(shape+"-"+baseline_loss);auto old=mode_budget::json(old_root/"result.json");
            check(old.at("parent_sha256")==binding.at("checkpoint_sha256")&&old.at("dataset_revision")==data_binding.at("dataset_revision_id")&&
                old.at("epochs")==40&&old.at("updates")==40&&old.at("batch_size")==2&&old.at("seed")==20261012&&
                mode_budget::hash(old_root/"weights.pt")==old.at("checkpoint_sha256"),"COVERAGE_BASELINE_PROVENANCE_MISMATCH");
            {YoloV8Segment reference(7);check(reference->load_state_dict_strict((old_root/"weights.pt").string()).complete(),"COVERAGE_BASELINE_LOAD");
                auto evaluated=evaluate(reference,manifest,inputs);
                // Baseline must reproduce prior diagnostic metrics before comparing new weights.
                for(const auto& row:evaluated)for(const auto& prev:source_audit.at("observations"))if(prev.at("shape")==shape&&prev.at("arm")==baseline_loss&&prev.at("sample_id")==row.at("sample_id")){
                    check(std::abs(prev.at("metrics").at("best_target_iou").get<double>()-row.at("metrics").at("best_target_iou").get<double>())<1e-12&&
                        prev.at("metrics").at("class_counts")==row.at("metrics").at("class_counts"),"COVERAGE_BASELINE_NOT_REPRODUCED");
                }
                results.push_back({{"shape",shape},{"loss",baseline_loss},{"coverage","original"},{"checkpoint_sha256",old.at("checkpoint_sha256")},{"observations",evaluated}});
            }
            if(geometry_only){
                auto base_dir=covered_root/(shape+"-"+baseline_loss+"-coverage-v1");auto base=mode_budget::json(base_dir/"result.json");
                check(base.at("coverage_revision_sha256")==revision_sha&&base.at("coverage_policy_sha256")==policy_sha&&
                    base.at("parent_sha256")==binding.at("checkpoint_sha256")&&base.at("epochs")==40&&base.at("updates")==40&&
                    base.at("batch_size")==2&&base.at("microbatch_size")==1&&base.at("seed")==20261012&&
                    mode_budget::hash(base_dir/"weights.pt")==base.at("checkpoint_sha256"),"GEOMETRY_CONTROL_NOT_MATCHED");
                YoloV8Segment baseline(7);check(baseline->load_state_dict_strict((base_dir/"weights.pt").string()).complete(),"GEOMETRY_CONTROL_LOAD");
                auto evaluated=evaluate(baseline,manifest,inputs);check(evaluated.size()==base.at("observations").size(),"GEOMETRY_CONTROL_COUNT");
                for(std::size_t i=0;i<evaluated.size();++i)check(evaluated[i].at("metrics")==base.at("observations")[i].at("metrics"),"GEOMETRY_CONTROL_NOT_REPRODUCED");
                base["observations"]=evaluated;results.push_back(base);
            }
            budget();torch::manual_seed(20261012);YoloV8Segment model(7);check(model->load_state_dict_strict(weights.string()).complete(),"COVERAGE_PARENT_LOAD");model->train();
            torch::optim::Adam optimizer(model->parameters(),torch::optim::AdamOptions(1e-4).weight_decay(0));
            auto compute=[&](const YoloV8SegDatasetSample& s,int64_t& positives){auto image=cv::imread(s.image_ref);SegLetterbox lb;auto in=MakeSegInput(image,manifest,lb);auto raw=model->forward(in);
                return version?unified_experiment::loss(model,raw,s,in,lb,manifest,&positives,false,false,geometry_only):LegacyBusinessYoloV8SegLoss(model,raw,s,in,lb,manifest);};
            J trace=J::array();auto dir=root/(shape+"-"+loss_name+"-coverage-v1");fs::create_directory(dir);
            std::ofstream progress(dir/"progress.jsonl");check(bool(progress),"COVERAGE_PROGRESS_OPEN");
            for(int epoch=0;epoch<40;++epoch){budget();double lr=1e-6+(1e-4-1e-6)*.5*(1+std::cos(std::acos(-1.)*epoch/39.));
                for(auto& group:optimizer.param_groups())static_cast<torch::optim::AdamOptions&>(group.options()).lr(lr);optimizer.zero_grad();
                double total=0,cls=0,box=0,mask=0,dfl=0;int64_t positives=0;
                for(int micro=0;micro<2;++micro){int i=epoch*2+micro;check(mode_budget::hash(augmented[i].image_ref)==exposures[i].at("image_sha256"),"COVERAGE_TRAIN_BYTES_CHANGED");
                    int64_t p=0;auto loss=compute(augmented[i],p);check(torch::isfinite(loss.total_loss).item<bool>(),"COVERAGE_NONFINITE_LOSS");
                    total+=loss.total_loss.item<double>();cls+=loss.class_loss.item<double>();box+=loss.box_loss.item<double>();mask+=loss.mask_loss.item<double>();dfl+=loss.dfl_loss.item<double>();positives+=p;
                    (loss.total_loss/2).backward();}
                optimizer.step();model->eval();double valid_loss=0;{torch::NoGradGuard guard;int64_t p=0;valid_loss=compute(valid,p).total_loss.item<double>();}model->train();
                J point={{"epoch",epoch+1},{"updates",epoch+1},{"effective_lr",lr},{"training_loss",total/2},{"class_loss",cls/2},{"box_loss",box/2},
                    {"mask_loss",mask/2},{"dfl_loss",dfl/2},{"validation_loss",valid_loss},{"positive_assignments",version?J(positives):J(nullptr)}};
                trace.push_back(point);progress<<point.dump()<<'\n';progress.flush();check(bool(progress),"COVERAGE_PROGRESS_WRITE");
                if(epoch%5==4)std::cout<<shape<<' '<<loss_name<<" coverage update="<<epoch+1<<" loss="<<total/2<<" valid="<<valid_loss<<std::endl;
            }
            check(WriteBusinessCheckpoint(model,dir/"weights.pt"),"COVERAGE_CHECKPOINT_WRITE");write_new(dir/"trace.json",trace.dump(2));
            // Measure serialized candidate, not just the in-memory training model.
            YoloV8Segment reloaded(7);check(reloaded->load_state_dict_strict((dir/"weights.pt").string()).complete(),"COVERAGE_CANDIDATE_RELOAD");
            auto evaluated=evaluate(reloaded,manifest,inputs);
            J result={{"shape",shape},{"loss",loss_name},{"coverage","paired_coverage_v1"},{"seed",20261012},
                {"epochs",40},{"updates",40},{"batch_size",2},{"microbatch_size",1},{"gradient_accumulation_steps",2},
                {"bn_policy","train running statistics per microbatch; eval validation/inference"},{"train_exposures",80},
                {"parent_sha256",binding.at("checkpoint_sha256")},{"base_dataset_revision",data_binding.at("dataset_revision_id")},
                {"coverage_revision_sha256",revision_sha},{"coverage_policy_sha256",policy_sha},{"checkpoint_sha256",mode_budget::hash(dir/"weights.pt")},
                {"assignment_ranking",geometry_only?"box_iou_pow6":"baseline"},{"strict_reload_evaluated",true},
                {"observations",evaluated},{"model_quality_accepted",false},{"production_allowed",false}};
            write_new(dir/"result.json",result.dump(2));results.push_back(result);
            write_new(root/(shape+"-"+loss_name+"-partial-results.json"),results.dump(2));
        }
        check(mode_budget::inventory(root/shape)==original_inventory,"COVERAGE_ORIGINAL_DATA_CHANGED");
    }
    check(mode_budget::inventory(parent)==parent_inventory&&mode_budget::hash(audit/"transfer_error_audit.json")==audit_digest,"COVERAGE_SOURCE_CHANGED");
    if(geometry_only)check(mode_budget::inventory(covered_root)==covered_inventory,"GEOMETRY_CONTROL_CHANGED");
    write_new(root/"comparison.json",J({{"schema","visionai.rd.coverage_budget_comparison.v1"},{"source_audit_sha256",audit_digest},
        {"coverage_policy_sha256",policy_sha},{"results",results},{"execution_status","PASS"},{"model_quality_accepted",false},{"production_allowed",false}}).dump(2));
    std::cout<<"PASS execution, NOT quality: "<<root.generic_string()<<std::endl;return 0;
}
}
