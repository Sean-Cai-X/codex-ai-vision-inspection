// Explicitly invoked R&D trial. Not an automatic CTest and not a UI action.
#include "synthetic_materializer.h"
#include "../../libtorch_module/libtorch_module_runtime_c_api.h"
#include <iostream>
#include <limits>
#ifdef _WIN32
#include <windows.h>
#endif

namespace {
using namespace supervision_fixture;
std::string read_bytes(const fs::path& path) {
    std::ifstream f(path,std::ios::binary|std::ios::ate);
    check(bool(f),"TRIAL_FILE_UNREADABLE");auto n=f.tellg();
    check(n>0&&n<=512*1024*1024,"TRIAL_FILE_SIZE_LIMIT");
    std::string bytes(static_cast<std::size_t>(n),'\0');f.seekg(0);f.read(bytes.data(),n);
    check(bool(f),"TRIAL_FILE_UNREADABLE");return bytes;
}
J read_json(const fs::path& p) {return cxvision::supervision::ParseJson(read_bytes(p));}
std::string digest(const fs::path& p) {return "sha256:"+Sha256Bytes(read_bytes(p));}
J tree_digests(const fs::path& root) {
    J hashes=J::object();
    for(const auto& entry:fs::recursive_directory_iterator(root)) if(entry.is_regular_file())
        hashes[entry.path().lexically_relative(root).generic_string()]=digest(entry.path());
    return hashes;
}
fs::path controlled_file(const fs::path& root,const std::string& relative) {
    fs::path r(relative);
    check(!r.empty()&&!r.is_absolute(),"TRIAL_RELATIVE_PARENT_REF_REQUIRED");
    fs::path p=root;
    for(const auto& part:r) {
        check(part!=".."&&part!=".","TRIAL_PARENT_PATH_INVALID");p/=part;
        check(!fs::is_symlink(fs::symlink_status(p)),"TRIAL_PARENT_SYMLINK_FORBIDDEN");
    }
    check(fs::is_regular_file(p),"TRIAL_PARENT_FILE_REQUIRED");return p;
}
J validate_parent(const fs::path& root,const J& b) {
    check(b.at("schema")=="visionai.rd_registered_parent_binding.v1"&&b.at("trial_only")==true&&
        b.at("production_allowed")==false&&b.at("parent_usage")=="DEVELOPMENT_ONLY","TRIAL_BINDING_SCOPE_INVALID");
    auto mp=controlled_file(root,b.at("parent_manifest_reference"));
    auto ap=controlled_file(root,b.at("attestation_reference"));
    auto lp=controlled_file(root,b.at("lineage_reference"));
    auto m=read_json(mp),a=read_json(ap),l=read_json(lp);
    auto wp=controlled_file(root,m.at("weights"));
    check(digest(mp)==b.at("parent_manifest_sha256"),"TRIAL_MANIFEST_SHA_MISMATCH");
    check(digest(ap)==b.at("attestation_sha256"),"TRIAL_ATTESTATION_SHA_MISMATCH");
    check(digest(lp)==b.at("source_lineage_sha256"),"TRIAL_LINEAGE_SHA_MISMATCH");
    check(digest(wp)==b.at("checkpoint_sha256"),"TRIAL_CHECKPOINT_SHA_MISMATCH");
    check(m.at("task")=="instance_segmentation"&&m.at("architecture")=="yolov8_seg"&&
        m.at("classes")==b.at("geometry_contract")&&m.at("num_classes")==7,"TRIAL_PARENT_CONTRACT_MISMATCH");
    check(a.at("schema")=="visionai.business.development_parent_attestation.v1"&&
        a.at("development_parent_model_id")==b.at("trial_model_id")&&
        b.at("development_parent_model_id")==b.at("trial_model_id")&&
        a.at("geometry_contract")==b.at("geometry_contract")&&a.at("trial_only")==true&&
        a.at("production_allowed")==false&&a.at("parent_usage")=="DEVELOPMENT_ONLY"&&
        a.at("checkpoint_sha256")==b.at("checkpoint_sha256")&&
        a.at("parent_manifest_sha256")==b.at("parent_manifest_sha256")&&
        a.at("source_lineage_sha256")==b.at("source_lineage_sha256"),"TRIAL_ATTESTATION_BINDING_MISMATCH");
    check(l.at("model_id")==b.at("trial_model_id")&&l.at("checkpoint_hash")==b.at("checkpoint_sha256"),"TRIAL_LINEAGE_BINDING_MISMATCH");
    return {{"status","PARENT_FILES_VERIFIED"},{"registered_binding",b},{"weights_format",m.at("weights_format")},
        {"strict_tensor_loading_verified",false}};
}
struct Handle {
    TorchRuntimeHandle value=nullptr;
    ~Handle(){if(value) torch_runtime_destroy(value);}
};
struct Result {
    TorchTaskResult value{};
    Result(){torch_runtime_init_result(&value);}
    ~Result(){torch_runtime_free_result(&value);}
};
std::string safe(const char* s){return s?s:"";}
J result_json(const TorchTaskResult& r,int rc) {
    J data={{"abi_return",rc},{"ok",r.ok},{"status",safe(r.status)},{"error_code",r.error_code},
        {"error_message",safe(r.error_message)},{"actual_device",safe(r.actual_device)},
        {"result",nullptr}};
    if(r.result_json&&*r.result_json) data["result"]=J::parse(r.result_json);
    return data;
}
struct Progress {
    J epochs=J::array();
    std::ofstream stream;
    bool failed=false;
    std::chrono::steady_clock::time_point start=std::chrono::steady_clock::now();
};
int on_epoch(void* p,unsigned percent,unsigned epoch,unsigned iterations,
             double loss,double score,int has_score,double lr) {
    auto& progress=*static_cast<Progress*>(p);
    try {
        check(std::isfinite(loss)&&std::isfinite(lr)&&lr>0&&(!has_score||std::isfinite(score)),"TRIAL_NONFINITE_METRIC");
        J row={{"progress_percent",percent},{"epoch",epoch},{"completed_iterations",iterations},
            {"training_loss",loss},{"validation_score",has_score?J(score):J(nullptr)},
            {"has_validation_score",has_score!=0},{"effective_learning_rate",lr}};
        progress.epochs.push_back(row);progress.stream<<row.dump()<<'\n';progress.stream.flush();
        check(bool(progress.stream),"TRIAL_PROGRESS_WRITE_FAILED");
        std::cout<<row.dump()<<std::endl;return 1;
    } catch(...) {progress.failed=true;return 0;}
}
int cancelled(void* p) {
    auto& progress=*static_cast<Progress*>(p);
    return progress.failed || std::chrono::steady_clock::now()-progress.start>std::chrono::minutes(10);
}
}

int main(int argc,char** argv) {
    using namespace supervision_fixture;
    fs::path run;
    try {
        if(argc!=5&&argc!=7) throw std::runtime_error("usage: business_trial binding.json parent_root rules.json output_base [epochs iterations]");
        auto count=[](const char* value) {
            std::string s=value;check(!s.empty()&&s.find_first_not_of("0123456789")==std::string::npos,"TRIAL_COUNT_INVALID");
            auto n=std::stoul(s);check(n>0&&n<=200,"TRIAL_COUNT_OUT_OF_BOUNDS");return static_cast<unsigned>(n);
        };
        const unsigned epochs=argc==7?count(argv[5]):2,iterations=argc==7?count(argv[6]):2;
        check(iterations>=epochs,"TRIAL_ITERATIONS_BELOW_EPOCHS");
        auto binding=read_json(argv[1]);auto rules=read_json(argv[3]);
        auto parent=fs::absolute(argv[2]);
        auto parent_check=validate_parent(parent,binding);
        run=fresh_root(argv[4]);write_new(run/"parent_verification.json",parent_check.dump(2));
        auto bundle=make_case(run/"ellipse",rules,"ellipse");
        auto input_inventory=tree_digests(run/"ellipse");
        write_new(run/"input_asset_inventory.json",input_inventory.dump(2));
        auto output=run/"output";fs::create_directory(output);
        auto train_dir=output/"development_trials"/"ellipse-train"/"staging";
        fs::create_directories(train_dir);
        std::string model_root=parent.generic_string(),out_root=output.generic_string();
        std::string manifest=(parent/binding.at("parent_manifest_reference").get<std::string>()).generic_string();
        std::string train_root=bundle.at("training_root"),staging=train_dir.generic_string();
        J extra={{"training_split_percent",67},{"frozen_training_split_percent",67},
            {"epoch_count",epochs},{"iteration_count",iterations},{"batch_size",1},
            {"trial_model_id",binding.at("trial_model_id")},{"annotation_click_mode","contour"},
            {"frozen_annotation_click_mode","contour"},{"allowed_annotation_click_modes",{"contour"}},
            {"case_id",bundle.at("case_id")},{"dataset_revision_id",bundle.at("dataset_revision_id")},
            {"dataset_manifest_sha256",bundle.at("dataset_manifest_sha256")},
            {"project_geometry_class","ellipse"},{"frozen_project_geometry_class","ellipse"},
            {"project_geometry_class_id",2},{"frozen_project_geometry_class_id",2},
            {"business_parent_attestation_path",binding.at("attestation_reference")},
            {"business_parent_attestation_sha256",binding.at("attestation_sha256")}};
        write_new(run/"effective_training_request.json",extra.dump(2));
        J runtime={{"version",safe(torch_runtime_version())},{"device","cpu"}};
#ifdef _WIN32
        wchar_t dll_path[32768];
        auto module=GetModuleHandleW(L"libtorch_module_runtime.dll");
        auto length=GetModuleFileNameW(module,dll_path,32768);
        check(module&&length>0&&length<32768,"TRIAL_RUNTIME_IDENTITY_UNAVAILABLE");
        runtime["dll_sha256"]=digest(fs::path(std::wstring(dll_path,length)));
#endif
        write_new(run/"runtime_identity.json",runtime.dump(2));
        TorchRuntimeConfig config{model_root.c_str(),out_root.c_str(),"cpu","info"};
        Handle handle;check(torch_runtime_create(&config,&handle.value)==0&&handle.value,"TRIAL_RUNTIME_CREATE_FAILED");
        Progress progress;progress.stream.open(run/"epoch_events.jsonl");
        check(bool(progress.stream),"TRIAL_PROGRESS_OPEN_FAILED");
        std::string serialized=extra.dump();
        TorchTaskRequest req{};req.device="cpu";req.dataset_root=train_root.c_str();req.manifest_path=manifest.c_str();
        req.output_dir=staging.c_str();req.extra_json=serialized.c_str();req.case_name="Seven Class Native ellipse Business Trial";
        std::cout<<"Starting real CPU trial: "<<epochs<<" epochs, "<<iterations<<" optimizer updates, batch size 1. Evidence: "<<run.generic_string()<<std::endl;
        Result training;int rc=torch_runtime_run_business_trial_v1(handle.value,&req,on_epoch,cancelled,&progress,&training.value);
        auto train_result=result_json(training.value,rc);write_new(run/"training_receipt.json",train_result.dump(2));
        check(rc==0&&training.value.ok==1,"TRIAL_TRAINING_FAILED_SEE_RECEIPT");
        check(progress.epochs.size()==epochs&&progress.epochs.back().at("completed_iterations")==iterations,"TRIAL_EPOCH_ACCOUNTING_MISMATCH");
        auto candidate=train_dir/"candidate";
        auto trace=read_json(candidate/"training_trace.json");
        check(trace.at("points").size()==progress.epochs.size(),"TRIAL_TRACE_COUNT_MISMATCH");
        for(std::size_t i=0;i<progress.epochs.size();++i) {
            const auto& a=progress.epochs[i];const auto& b=trace.at("points")[i];
            check(a.at("epoch")==b.at("epoch")&&a.at("completed_iterations")==b.at("completed_iterations"),"TRIAL_TRACE_TASK_MISMATCH");
            for(const char* key:{"training_loss","validation_score","effective_learning_rate"}) {
                double x=a.at(key).get<double>(),y=b.at(key).get<double>();
                check(std::abs(x-y)<=1e-5*std::max(1.0,std::abs(x)),"TRIAL_TRACE_METRIC_MISMATCH");
            }
        }
        auto cm=read_json(candidate/"model_manifest.json");
        check(cm.at("training_loss_version")=="center_dense_balanced_bce_v2"&&
            cm.at("training_loss_version")==trace.at("training_loss_version")&&
            cm.at("training_loss_version")==train_result.at("result").at("training_loss_version"),"TRIAL_LOSS_VERSION_MISMATCH");
        auto weights=candidate/cm.at("weights").get<std::string>();
        check(digest(weights)==train_result.at("result").at("candidate_model_sha256"),"TRIAL_CANDIDATE_DIGEST_MISMATCH");
        auto verify=read_json(run/"ellipse"/"verify"/"asset_binding.json");
        auto infer_dir=output/"development_trials"/"ellipse-verify"/"staging";fs::create_directories(infer_dir);
        std::string candidate_root=candidate.generic_string(),candidate_manifest=(candidate/"model_manifest.json").generic_string();
        std::string image=(run/"ellipse"/"verify"/"sample-3.pgm").generic_string(),infer_output=infer_dir.generic_string();
        TorchRuntimeConfig infer_config{candidate_root.c_str(),out_root.c_str(),"cpu","info"};
        Handle infer;check(torch_runtime_create(&infer_config,&infer.value)==0&&infer.value,"TRIAL_INFERENCE_RUNTIME_CREATE_FAILED");
        J infer_extra={{"verify_asset_binding_id",verify.at("asset_ref")},{"verify_asset_sha256","sha256:"+verify.at("image_sha256").get<std::string>()}};
        std::string infer_json=infer_extra.dump();TorchTaskRequest ir{};ir.device="cpu";ir.input_image=image.c_str();
        ir.manifest_path=candidate_manifest.c_str();ir.output_dir=infer_output.c_str();ir.extra_json=infer_json.c_str();
        Result inferred;rc=torch_runtime_run_business_inference_v1(infer.value,&ir,&inferred.value);
        auto inference_result=result_json(inferred.value,rc);write_new(run/"inference_receipt.json",inference_result.dump(2));
        check(rc==0&&inferred.value.ok==1,"TRIAL_INFERENCE_FAILED_SEE_RECEIPT");
        check(inference_result.at("result").at("candidate_model_sha256")==train_result.at("result").at("candidate_model_sha256"),"TRIAL_INFERENCE_MODEL_MISMATCH");
        auto instances=read_json(infer_dir/"instances.json");
        auto mapping=read_json(infer_dir/"weight_mapping_report.json");
        check(mapping.at("complete")==true&&mapping.at("loaded_count")==mapping.at("target_count"),"TRIAL_CANDIDATE_STRICT_MAPPING_FAILED");
        check(instances.at("weights_hash")==train_result.at("result").at("candidate_model_sha256"),"TRIAL_INSTANCE_MODEL_MISMATCH");
        check(instances.at("transform").at("original_width")==64&&instances.at("transform").at("original_height")==64,"TRIAL_ORIGINAL_DIMENSIONS_MISMATCH");
        J class_counts=J::object();unsigned target_count=0;
        for(const auto& instance:instances.at("instances")) {
            int id=instance.at("class_id").get<int>();
            check(id>=0&&id<7&&instance.at("class_name")==binding.at("geometry_contract")[id],"TRIAL_DECODED_CLASS_INVALID");
            double score=instance.at("class_confidence").get<double>();
            check(std::isfinite(score)&&score>=0&&score<=1,"TRIAL_DECODED_SCORE_INVALID");
            const auto& box=instance.at("bbox");
            double x0=box.at("x0"),x1=box.at("x1"),y0=box.at("y0"),y1=box.at("y1");
            check(std::isfinite(x0)&&std::isfinite(x1)&&std::isfinite(y0)&&std::isfinite(y1)&&
                x0>=0&&y0>=0&&x1<=64&&y1<=64&&x1>=x0&&y1>=y0,"TRIAL_ORIGINAL_COORDINATES_INVALID");
            std::string name=instance.at("class_name");class_counts[name]=class_counts.value(name,0)+1;
            if(id==2) ++target_count;
        }
        J observation={{"execution_status","PASS"},{"sample_purpose","VERIFY"},{"project_class","ellipse"},
            {"raw_instance_count",instances.at("instances").size()},{"target_instance_count",target_count},
            {"class_counts",class_counts},{"project_target_result",target_count?"TARGET_CANDIDATES_REQUIRE_REVIEW":"NO_PROJECT_TARGET_DETECTED"},
            {"model_quality_accepted",false},{"independent_model_accuracy_evidence",false},
            {"coordinate_bounds_checked",true},{"geometry_accuracy_measured",false},
            {"review_required",true}};
        write_new(run/"verify_observation.json",observation.dump(2));
        // Recheck read-only parent files after execution.
        validate_parent(parent,binding);
        check(tree_digests(run/"ellipse")==input_inventory,"TRIAL_SOURCE_ASSETS_CHANGED");
        J summary={{"status","PASS"},{"project_id",binding.at("project_id")},{"runtime",runtime},
            {"parent_files_unchanged",true},{"source_assets_unchanged",true},{"strict_tensor_loading_verified",true},
            {"real_training_executed",true},{"optimizer_updates",iterations},{"epoch_callbacks",epochs},
            {"real_verify_inference_executed",true},{"python_executed",false},
            {"verify_observation",observation},
            {"dataset_revision_id",bundle.at("dataset_revision_id")},{"candidate_model_sha256",train_result.at("result").at("candidate_model_sha256")},
            {"admission","development_trial"},{"independent_model_accuracy_evidence",false},{"production_activation",false}};
        write_new(run/"trial_summary.json",summary.dump(2));std::cout<<summary.dump()<<std::endl;return 0;
    } catch(const std::exception& e) {
        if(!run.empty()) {try {write_new(run/"runner_failure.json",J({{"status","FAILED"},{"code",e.what()},{"production_activation",false}}).dump(2));}catch(...) {}}
        std::cerr<<"FAIL "<<e.what()<<'\n';return 1;
    }
}
