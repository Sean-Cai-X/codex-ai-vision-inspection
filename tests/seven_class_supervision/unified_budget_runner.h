// Includes production helpers through runtime_dataset_probe; no business images.
#include "unified_assignment_experiment.h"
namespace unified_budget {
using namespace supervision_fixture;
inline int run(int argc,char** argv){
    check(argc==6,"usage: --unified-budget binding parent_root rules output_base");
    auto b=mode_budget::json(argv[2]);auto parent=fs::canonical(argv[3]);auto rules=mode_budget::json(argv[4]);
    auto pm=mode_budget::json(parent/"model_manifest.json");auto weights=parent/pm.at("weights").get<std::string>();
    check(mode_budget::hash(weights)==b.at("checkpoint_sha256")&&mode_budget::hash(parent/"model_manifest.json")==b.at("parent_manifest_sha256")&&
        mode_budget::hash(parent/b.at("attestation_reference").get<std::string>())==b.at("attestation_sha256")&&
        mode_budget::hash(parent/b.at("lineage_reference").get<std::string>())==b.at("source_lineage_sha256"),"UNIFIED_PARENT_SHA");
    auto inventory=mode_budget::inventory(parent);auto root=fs::canonical(fresh_root(argv[5]));std::cout<<"Evidence: "<<root<<std::endl;
    TorchModelManifest manifest;std::string reason;check(LoadTorchModelManifest((parent/"model_manifest.json").string(),parent.string(),manifest,reason),"UNIFIED_MANIFEST");
    J results=J::array();auto start=std::chrono::steady_clock::now();
    for(const std::string shape:{"circle","ellipse"}){
        auto binding=make_case(root/shape,rules,shape,"none",3);auto source_inventory=mode_budget::inventory(root/shape);
        BusinessTrialSettings settings;settings.case_id=binding.at("case_id");settings.dataset_revision_id=binding.at("dataset_revision_id");
        settings.dataset_manifest_sha256=binding.at("dataset_manifest_sha256");settings.project_geometry_class=shape;settings.project_geometry_class_id=shape=="circle"?1:2;
        BusinessMaterializedDataset data;check(LoadBusinessMaterializedDataset(binding.at("training_root"),settings,data,reason),"UNIFIED_LOADER");
        check(data.train_samples.size()==3&&data.validation_samples.size()==1,"UNIFIED_SPLIT");
        for(int version=0;version<4;++version){
            const char* label=version==0?"legacy_v1":version==1?"unified_v5_hard_negative_experimental":
                version==2?"unified_v8_quality_weighted_multiscale_experimental":"unified_v9_level_balanced_hard_negative_experimental";
            torch::manual_seed(20261012);YoloV8Segment model(7);
            check(model->load_state_dict_strict(weights.string()).complete(),"UNIFIED_LOAD");model->train();
            torch::optim::Adam optimizer(model->parameters(),torch::optim::AdamOptions(1e-4).weight_decay(0));
            auto compute=[&](const YoloV8SegDatasetSample& s,int64_t& positives){auto image=cv::imread(s.image_ref);SegLetterbox lb;auto in=MakeSegInput(image,manifest,lb);auto raw=model->forward(in);
                return version?unified_experiment::loss(model,raw,s,in,lb,manifest,&positives,version==2,version==3):
                    LegacyBusinessYoloV8SegLoss(model,raw,s,in,lb,manifest);};
            J trace=J::array();unsigned cursor=0;
            for(unsigned epoch=0;epoch<40;++epoch){
                check(std::chrono::steady_clock::now()-start<std::chrono::minutes(15),"UNIFIED_TIME_BUDGET");
                double lr=1e-6+(1e-4-1e-6)*.5*(1+std::cos(std::acos(-1.)*epoch/39.));
                for(auto& group:optimizer.param_groups())static_cast<torch::optim::AdamOptions&>(group.options()).lr(lr);
                optimizer.zero_grad();double train_loss=0;int64_t positives=0;
                for(int batch=0;batch<2;++batch){int64_t count=0;auto loss=compute(data.train_samples[cursor++%3],count).total_loss;
                    check(torch::isfinite(loss).item<bool>(),"UNIFIED_NONFINITE");train_loss+=loss.item<double>();positives+=count;(loss/2).backward();}
                optimizer.step();model->eval();double valid=0;{torch::NoGradGuard guard;int64_t ignored=0;valid=compute(data.validation_samples[0],ignored).total_loss.item<double>();}model->train();
                trace.push_back({{"epoch",epoch+1},{"updates",epoch+1},{"batch_size",2},{"lr",lr},{"training_loss",train_loss/2},{"validation_loss",valid},{"positive_assignments",version?J(positives):J(nullptr)}});
                if(epoch%10==9)std::cout<<shape<<' '<<label<<" updates="<<epoch+1<<" loss="<<train_loss/2<<std::endl;
            }
            auto dir=root/(shape+"-"+label);fs::create_directory(dir);check(WriteBusinessCheckpoint(model,dir/"weights.pt"),"UNIFIED_CHECKPOINT");
            write_new(dir/"trace.json",trace.dump(2));model->eval();torch::NoGradGuard guard;J observations=J::array();
            for(int seed=0;seed<5;++seed){
                auto project=mode_budget::json(root/shape/"project.json");auto source=project.at("samples")[seed];
                if(seed==4){source["split"]="valid";source["annotations"]=J::array({{{"id","oracle"},{"class_name",shape},{"closed",true},{"points",points(shape,seed)}}});source["annotation_receipt_digest"]=Sha256Bytes(source["annotations"][0].dump());}
                auto conversion=cxvision::supervision::Convert(source,rules);cv::Mat truth=(cv::Mat(64,64,CV_8UC1,conversion.mask.data())==settings.project_geometry_class_id);
                auto image=cv::imread((root/shape/(seed==4?"verify":"training")/("sample-"+std::to_string(seed)+".pgm")).string());
                SegLetterbox lb;auto raw=model->forward(MakeSegInput(image,manifest,lb));SegNmsDiagnostics diag;auto candidates=DecodeCandidates(raw,model->head(),manifest,&diag);
                J counts=J::object();for(const auto& name:manifest.class_names)counts[name]=0;
                std::vector<cv::Mat> target_masks;double best=0;cv::Mat union_mask=cv::Mat::zeros(truth.size(),CV_8UC1);int accepted=0,wrong=0;
                for(const auto& c:candidates){double q=0,stability=0;auto mask=DecodeMask(c,raw.prototypes,manifest,lb,image.size(),q,stability);if(LargestContour(mask).size()<3)continue;
                    ++accepted;std::string name=manifest.class_names[c.class_id];counts[name]=counts.at(name).get<int>()+1;
                    if(c.class_id!=settings.project_geometry_class_id){++wrong;continue;}
                    target_masks.push_back(mask);cv::bitwise_or(union_mask,mask,union_mask);cv::Mat both,either;cv::bitwise_and(mask,truth,both);cv::bitwise_or(mask,truth,either);best=std::max(best,double(cv::countNonZero(both))/std::max(1,cv::countNonZero(either)));
                }
                int duplicate_pairs=0,pairs=0;for(std::size_t a=0;a<target_masks.size();++a)for(std::size_t c=a+1;c<target_masks.size();++c){++pairs;cv::Mat both,either;cv::bitwise_and(target_masks[a],target_masks[c],both);cv::bitwise_or(target_masks[a],target_masks[c],either);if(double(cv::countNonZero(both))/std::max(1,cv::countNonZero(either))>.5)++duplicate_pairs;}
                cv::Mat both,either;cv::bitwise_and(union_mask,truth,both);cv::bitwise_or(union_mask,truth,either);
                observations.push_back({{"seed",seed},{"split",seed<3?"Train":seed==3?"Valid":"VERIFY_SYNTHETIC_ORACLE"},
                    {"class_counts",counts},{"target_count",target_masks.size()},{"wrong_class_count",wrong},{"accepted_count",accepted},
                    {"best_target_iou",best},{"union_target_iou",double(cv::countNonZero(both))/std::max(1,cv::countNonZero(either))},
                    {"same_class_pair_iou_gt_0_5",duplicate_pairs},{"pair_count",pairs},{"pre_nms",diag.above_threshold},{"nms_unexamined",diag.above_threshold-diag.examined},{"cap_reached",diag.cap_reached}});
            }
            J result={{"shape",shape},{"loss",label},{"parent_sha256",b.at("checkpoint_sha256")},{"dataset_revision",binding.at("dataset_revision_id")},
                {"checkpoint_sha256",mode_budget::hash(dir/"weights.pt")},{"seed",20261012},{"epochs",40},{"updates",40},{"batch_size",2},
                {"bn_policy","train statistics; eval validation and inference"},{"observations",observations},{"production_allowed",false},{"model_quality_accepted",false}};
            write_new(dir/"result.json",result.dump(2));results.push_back(result);
        }
        check(mode_budget::inventory(root/shape)==source_inventory,"UNIFIED_INPUT_CHANGED");
    }
    check(mode_budget::inventory(parent)==inventory,"UNIFIED_PARENT_CHANGED");write_new(root/"comparison.json",results.dump(2));std::cout<<"PASS controlled execution: "<<root<<std::endl;return 0;
}
}
