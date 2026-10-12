// Test-only compilation of the exact production executor translation unit.
// Its private loader stays private: no new DLL export or business ABI bypass.
#include "../../libtorch_module/torch_runtime_yolov8_seg_executor.cpp"
#include "synthetic_materializer.h"
#include <iostream>
#include "mode_budget_diagnostic.h"

int main(int argc,char** argv) {
    using namespace supervision_fixture;
    try {
        if(argc==2&&std::string(argv[1])=="--nms-count-test") {
            TorchModelManifest manifest;manifest.max_detections=2;manifest.iou_threshold=.45;manifest.class_agnostic_nms=true;
            auto point=[](int cls,float score,float x){SegCandidate c;c.class_id=cls;c.score=score;c.x0=x;c.y0=0;c.x1=x+10;c.y1=10;return c;};
            SegNmsDiagnostics d;
            auto r=SelectSegCandidates({point(2,.9,0),point(2,.8,0),point(2,.7,30),point(2,.6,60)},manifest,d);
            check(r.size()==2&&d.above_threshold==4&&d.examined==3&&d.suppressed==1&&d.cap_reached,"NMS_TRUNCATED_COUNTS");
            check(r[0].score==.9f&&r[1].score==.7f,"NMS_SELECTION_CHANGED");
            manifest.max_detections=20;
            r=SelectSegCandidates({point(2,.9,0),point(1,.8,0)},manifest,d);
            check(r.size()==1&&d.examined==2&&d.suppressed==1&&!d.cap_reached,"NMS_CROSS_CLASS_COUNTS");
            manifest.class_agnostic_nms=false;
            r=SelectSegCandidates({point(2,.9,0),point(1,.8,0)},manifest,d);
            check(r.size()==2&&d.suppressed==0,"NMS_CLASS_AWARE_CHANGED");
            r=SelectSegCandidates({},manifest,d);check(r.empty()&&d.examined==0&&!d.cap_reached,"NMS_EMPTY_COUNTS");
            std::cout<<"PASS NMS accounting and preserved selection\n";return 0;
        }
        if(argc>1&&std::string(argv[1])=="--mode-budget")return mode_budget::run(argc,argv);
        if(argc==2&&std::string(argv[1])=="--classification-loss-test") {
            for(int cls=0;cls<7;++cls) {
                auto logits=torch::zeros({1,7,2,2},torch::TensorOptions().requires_grad(true));
                auto target=torch::zeros_like(logits).detach();
                SetBusinessCenterClassTarget(target,cls,0,0);
                auto loss=BusinessDenseClassificationLoss(logits,target);loss.backward();
                auto grad=logits.grad();
                for(int c=0;c<7;++c) {
                    check((grad.index({0,c,0,0}).item<double>()<0)==(c==cls),"CLASS_CHANNEL_GRADIENT_WRONG");
                    check(grad.index({0,c,1,1}).item<double>()>0,"BACKGROUND_GRADIENT_MISSING");
                }
                auto updated=(logits.detach()-grad).set_requires_grad(false);
                check(BusinessDenseClassificationLoss(updated,target).item<double>()<loss.item<double>(),"CLASS_LOSS_STEP_NOT_DESCENDING");
            }
            auto empty_logits=torch::zeros({1,7,2,2},torch::TensorOptions().requires_grad(true));
            auto empty=BusinessDenseClassificationLoss(empty_logits,torch::zeros_like(empty_logits).detach());
            empty.backward();check(torch::isfinite(empty).item<bool>()&&(empty_logits.grad()>0).all().item<bool>(),"EMPTY_TARGET_BACKGROUND_INVALID");
            auto target=torch::zeros({1,7,2,2});SetBusinessCenterClassTarget(target,2,0,0);
            SetBusinessCenterClassTarget(target,2,0,0); // same-class duplicate is idempotent
            SetBusinessCenterClassTarget(target,3,1,1); // distinct objects retain class channels
            check(target.sum().item<int>()==2,"MULTI_OBJECT_TARGET_INVALID");
            bool collision=false;try {SetBusinessCenterClassTarget(target,4,0,0);}catch(const c10::Error&){collision=true;}
            check(collision,"COMPETING_CLASS_SILENT_OVERWRITE");
            std::cout<<"PASS seven class gradients, dense background, loss descent, empty targets, competing centers\n";return 0;
        }
        if(argc!=3) throw std::runtime_error("rules and evidence base required");
        std::ifstream f(argv[1]);std::string bytes((std::istreambuf_iterator<char>(f)),{});
        auto rules=cxvision::supervision::ParseJson(bytes);
        auto root=fresh_root(argv[2]);auto suite=make_suite(root,rules);
        J results=J::array();
        auto load=[&](const J& binding,const std::string& expected_error) {
            BusinessTrialSettings settings;
            settings.case_id=binding.at("case_id");
            settings.dataset_revision_id=binding.at("dataset_revision_id");
            settings.dataset_manifest_sha256=binding.at("dataset_manifest_sha256");
            settings.project_geometry_class=binding.at("project_class");
            auto found=std::find(kBusinessGeometryClasses.begin(),kBusinessGeometryClasses.end(),settings.project_geometry_class);
            settings.project_geometry_class_id=static_cast<int>(found-kBusinessGeometryClasses.begin());
            BusinessMaterializedDataset dataset;std::string code;
            bool ok=LoadBusinessMaterializedDataset(binding.at("training_root").get<std::string>(),settings,dataset,code);
            if(expected_error.empty()) {
                check(ok,"REAL_LOADER_POSITIVE_FAILED");
                check(dataset.train_samples.size()==2 && dataset.validation_samples.size()==1,"REAL_LOADER_SPLIT_COUNTS");
                check(dataset.supervision_binding_status=="train_valid_verified_verify_metadata_only","REAL_LOADER_BOUND_STATUS");
                for(const auto& sample:dataset.train_samples) check(!sample.classes.empty(),"REAL_LOADER_EMPTY_GEOMETRY");
            } else {
                if(ok || code!=expected_error) throw std::runtime_error("REAL_LOADER_REJECTION_MISMATCH: "+code+" expected "+expected_error);
            }
            results.push_back({{"case_name",binding.at("case_name")},{"fault",binding.at("fault")},
                {"loader_ok",ok},{"error_code",code},{"expected_error_code",expected_error},
                {"train_count",dataset.train_samples.size()},{"valid_count",dataset.validation_samples.size()},
                {"binding_status",dataset.supervision_binding_status},{"expectation_passed",true}});
        };
        for(const auto& binding:suite.at("cases")) load(binding,"");
        const std::pair<const char*,const char*> faults[]={
            {"wrong_mask_pixels","SUPERVISION_MATERIALIZATION_CONTENT_MISMATCH"},
            {"wrong_encoded_mask_hash","BUSINESS_DATASET_ASSET_DIGEST_MISMATCH"},
            {"wrong_sidecar_hash","BUSINESS_SUPERVISION_FILE_DIGEST_MISMATCH"},
            {"wrong_width","BUSINESS_SUPERVISION_WIDTH_MISMATCH"},
            {"verify_in_training","BUSINESS_DATASET_VERIFY_MATERIALIZATION_FORBIDDEN"},
            {"v1_with_binding","BUSINESS_SUPERVISION_SCHEMA_REQUIRED"}};
        for(const auto& fault:faults) load(make_case(root/fault.first,rules,"open_curve",fault.first),fault.second);
        J receipt={{"schema","visionai.native_runtime_dataset_probe.v1"},{"status","PASS"},{"cases",results},
            {"actual_file_hashing",true},{"actual_opencv_decode",true},{"actual_production_loader",true},
            {"public_dll_dataset_call_executed",false},{"model_training_executed",false},
            {"python_executed",false},{"independent_model_accuracy_evidence",false},{"admission","development_trial"}};
        write_new(root/"runtime_dataset_probe_receipt.json",receipt.dump(2));
        std::cout<<"PASS real production loader: "<<results.size()<<" cases. Evidence: "<<root.generic_string()<<'\n';return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
