// Real business broker + typed transport + dynamically loaded production DLL.
// Only creates fresh R&D synthetic fixtures. Never reads a business image.
#include "../seven_class_supervision/synthetic_materializer.h"
#include "adapters/LocalBusinessAssetBroker.h"
#include <Windows.h>
#include <Psapi.h>
#include <iostream>
#include <thread>
using namespace supervision_fixture;
namespace ad = vision_ai::adapters;
namespace ob = vision_ai::offline_bridge::v1;
namespace {
std::string bytes(const fs::path& p) {
    std::ifstream f(p, std::ios::binary); check(bool(f), "PREVIEW_READ_FAILED");
    return std::string(std::istreambuf_iterator<char>(f), {});
}
J json(const fs::path& p) { return J::parse(bytes(p)); }
std::string sha(const fs::path& p) { return ad::LocalBusinessAssetBroker::ComputeSha256ForControlledFile(p); }
J failure(const ob::Failure& f) {return {{"code",f.code},{"stage",f.failure_stage},{"recovery",f.recovery}};}
J modules() {
    HMODULE list[2048]; DWORD count=0;
    check(EnumProcessModules(GetCurrentProcess(),list,sizeof(list),&count)&&count<=sizeof(list),"MODULE_ENUM_FAILED");
    J result=J::array();
    for(unsigned i=0;i<count/sizeof(HMODULE);++i) {
        wchar_t path[32768]; auto n=GetModuleFileNameW(list[i],path,32768);
        check(n>0&&n<32768,"MODULE_PATH_FAILED"); fs::path p(std::wstring(path,n));
        // Record all actually loaded files, including system dependency provenance.
        result.push_back({{"file",p.filename().string()},{"local_path",p.generic_string()},{"sha256",sha(p)}});
    }
    return result;
}
}
int main(int argc,char** argv) {
    fs::path run;
    try {
        check(argc>=6,"usage: rd_isolated_preview package expected_lock_sha rules output_base dependency_directory...");
        auto package=fs::canonical(argv[1]);
        check(sha(package/"handoff.lock.json")==argv[2],"HANDOFF_LOCK_SHA_MISMATCH");
        // Windows transport intentionally does not use PATH for DLL resolution.
        check(SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS)!=0,"DLL_SEARCH_FAILED");
        for(int i=5;i<argc;++i) check(AddDllDirectory(fs::canonical(argv[i]).wstring().c_str())!=nullptr,"DEPENDENCY_DIRECTORY_INVALID");
        auto delivery=json(package/"delivery_manifest.json");
        auto binding=json(package/"parent/registered_binding.json");
        auto shape=delivery.at("target_geometry").get<std::string>();
        check(shape=="circle"||shape=="ellipse","PREVIEW_GEOMETRY_INVALID");
        run=fresh_root(argv[4]); auto rules=json(argv[3]);
        auto materialized=make_case(run/"synthetic",rules,shape,"none",3);
        auto project=json(run/"synthetic/project.json");
        // The current business broker accepts a v1 inventory for inference.
        // Create a separate read-only compatibility VIEW; preserve the original
        // v2 training manifest/sidecar. This view is never submitted for training.
        std::istringstream original(bytes(run/"synthetic/training/manifest.v1"));
        std::ostringstream inventory;std::string line;
        while(std::getline(original,line)) {
            if(line.rfind("schema=",0)==0) inventory<<"schema=visionai.geometry-segmentation-materializer.v1\n";
            else if(line.rfind("supervision_",0)!=0) inventory<<line<<'\n';
        }
        fs::create_directory(run/"broker-inference-view");
        for(const auto& e:fs::directory_iterator(run/"synthetic/training"))
            if(e.path().extension()==".pgm") fs::copy_file(e.path(),run/"broker-inference-view"/e.path().filename());
        write_new(run/"broker-inference-view/manifest.v1",inventory.str());
        const auto broker_dataset_sha=sha(run/"broker-inference-view/manifest.v1");
        fs::create_directory(run/"output");
        J assets=J::array();
        for(int seed=0;seed<5;++seed) {
            const auto& sample=project.at("samples")[seed];
            std::string id="sample-"+std::to_string(seed),role=sample.at("split");
            std::string image=(seed==4?"verify/":"training/")+id+".pgm";
            J asset={{"asset_ref",id},{"image_logical_ref",id},{"split",role},
                {"source_path","synthetic/"+image},{"image_sha256",sha(run/"synthetic"/image)},
                {"class_id",shape=="circle"?1:2},{"class_name",shape}};
            if(seed<4) {
                // ACCEPTED here is a fixture oracle, explicitly NOT a human/business receipt.
                J receipt={{"source","RND_SYNTHETIC_ORACLE_NOT_HUMAN_ANNOTATION"},{"sample",sample}};
                auto rel="synthetic/receipt-"+id+".json";
                write_new(run/rel,receipt.dump(2));
                asset["annotation_state"]="ACCEPTED";
                asset["annotation_receipt_path"]=rel;asset["annotation_receipt_digest"]=sha(run/rel);
                asset["real_mask_manifest_path"]="synthetic/training/manifest.v1";
                asset["real_mask_manifest_digest"]=materialized.at("dataset_manifest_sha256");
            }
            assets.push_back(asset);
        }
        J frozen={{"schema",ad::kLocalBusinessProjectFrozenManifestSchema},
            {"project_scope","REFERENCE_GEOMETRY_TRIAL"},{"reference_only",true},{"frozen",true},
            {"reference_source_case_id","rd-synthetic-"+shape},
            {"reference_trial_authorization_id","rd-isolated-preview-integration-20261012"},
            {"dataset_use","development_trial"},{"case_id",materialized.at("case_id")},
            {"project_id","rd-isolated-"+shape},{"normalized_asset_root",run.generic_string()},
            {"normalized_parent_model_root",(package/"parent").generic_string()},
            {"normalized_trial_output_root",(run/"output").generic_string()},
            {"frozen_dataset_root","broker-inference-view"},{"dataset_manifest_path","broker-inference-view/manifest.v1"},
            {"dataset_revision",materialized.at("dataset_revision_id")},{"dataset_sha256",broker_dataset_sha},
            {"parent_model_digest",binding.at("checkpoint_sha256")},{"parent_manifest_path","model_manifest.json"},
            {"parent_manifest_sha256",binding.at("parent_manifest_sha256")},
            {"business_parent_attestation_path",binding.at("attestation_reference")},
            {"business_parent_attestation_sha256",binding.at("attestation_sha256")},
            {"staging_output_root","isolated_business_validation"},{"project_geometry_class",shape},
            {"project_geometry_class_id",shape=="circle"?1:2},{"geometry_contract",binding.at("geometry_contract")},
            {"training_split_percent",75},{"annotation_click_mode","contour"},
            {"allowed_trial_model_ids",J::array({binding.at("trial_model_id")})},
            {"allowed_annotation_click_modes",{"contour"}},{"assets",assets},{"candidate_registry",J::array()}};
        fs::create_directory(run/"output/isolated_business_validation");
        write_new(run/"frozen-project.json",frozen.dump(2));
        const auto frozen_sha=sha(run/"frozen-project.json");
        ad::LocalBusinessAssetBrokerConfiguration cfg;
        cfg.frozen_manifest_path=run/"frozen-project.json";
        cfg.expected_runtime_sha256=delivery.at("runtime_sha256");
        auto baseline=ad::LocalBusinessAssetBroker::Load(cfg);
        check(baseline.Ready(),("BASELINE_BROKER: "+baseline.failure.code+" "+baseline.failure.failure_stage).c_str());
        cfg.external_preview_root=package;cfg.external_preview_lock_sha256=argv[2];
        cfg.external_preview_target_manifest_sha256=frozen_sha;
        auto loaded=ad::LocalBusinessAssetBroker::Load(cfg);
        check(loaded.Ready(),("PREVIEW_BROKER: "+loaded.failure.code+" "+loaded.failure.failure_stage).c_str());
        auto broker=std::dynamic_pointer_cast<ad::LocalBusinessAssetBroker>(loaded.broker);
        auto roots=broker->ReadLocalRuntimeRoots(); check(roots.Ready(),"ROOTS_NOT_READY");
        fs::create_directories(roots.controlled_trial_output_root/"isolated_business_validation");
        const std::string model=delivery.at("candidate_checkpoint_sha256");
        const std::string case_id=frozen.at("case_id"), project_id=frozen.at("project_id");
        check(!baseline.broker->ResolveInferenceAsset(case_id,project_id,"sample-4",model).Ready(),"PREVIEW_LEAKED_INTO_DEFAULT_BROKER");
        check(!broker->ReadRecoverableTrialCandidate().Ready(),"PREVIEW_FAKED_LOCAL_TRAINING");
        ob::TrialParameters params{75,40,40,2,binding.at("trial_model_id"),"contour"};
        check(broker->ResolveTrainingDataset(case_id,project_id,params).failure.code=="EXTERNAL_PREVIEW_INFERENCE_ONLY","PREVIEW_TRAINING_NOT_BLOCKED");
        J rejections=J::array();
        for(int i=0;i<3;++i) {
            auto bad=cfg;const std::string zero="sha256:"+std::string(64,'0');
            if(i==0)bad.external_preview_lock_sha256=zero;
            if(i==1)bad.external_preview_target_manifest_sha256=zero;
            if(i==2)bad.expected_runtime_sha256=zero;
            auto rejected=ad::LocalBusinessAssetBroker::Load(bad);
            check(!rejected.Ready(),"PREVIEW_BAD_PIN_ACCEPTED");rejections.push_back(failure(rejected.failure));
        }
        ad::LocalOfflineBusinessRuntimeConfiguration runtime;
        runtime.runtime_library=package/"runtime/libtorch_module_runtime.dll";
        runtime.expected_runtime_sha256=cfg.expected_runtime_sha256;
        runtime.model_root=package/"parent";runtime.controlled_trial_output_root=roots.controlled_trial_output_root;
        runtime.trial_model_ids={binding.at("trial_model_id")};runtime.annotation_click_modes={"contour"};
        auto transport=ad::MakeLocalOfflineBusinessRuntimeTransport(runtime,loaded.broker);
        ob::OfflineBusinessServiceBridge bridge(transport);
        auto capability=bridge.GetCapability(ob::kInferenceCapability);
        check(capability.available,("CAPABILITY_UNAVAILABLE: "+capability.recovery).c_str());
        check(capability.runtime_sha256==cfg.expected_runtime_sha256,"CAPABILITY_RUNTIME_MISMATCH");
        J observations=J::array();
        for(int seed=0;seed<5;++seed) {
            std::string id="sample-"+std::to_string(seed);
            auto asset=broker->ResolveInferenceAsset(case_id,project_id,id,model);
            check(asset.Ready(),"INFERENCE_ASSET_NOT_READY");
            ob::InferenceRequest request{"rd-preview-"+shape+"-"+id,case_id,project_id,asset.asset_ref,asset.image_sha256,model,asset.dataset_revision};
            auto local=broker->ResolveLocalInferenceBinding(request);check(local.Ready(),"LOCAL_BINDING_FAILED");
            auto status=bridge.Submit(request);
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::minutes(3);
            while(status.failure.code.empty()&&(status.state=="QUEUED"||status.state=="RUNNING")) {
                check(std::chrono::steady_clock::now()<deadline,"INFERENCE_TIMEOUT");
                std::this_thread::sleep_for(std::chrono::milliseconds(50));status=bridge.PollInference(status.task_id);
            }
            auto receipt=bridge.InferenceReceiptFor(status.task_id);
            check(receipt.verified,("TYPED_RECEIPT_FAILED: "+status.state+" "+status.failure.code+" "+receipt.failure.code).c_str());
            check(receipt.model_digest==model&&receipt.image_sha256==asset.image_sha256&&receipt.dataset_revision==asset.dataset_revision,"RECEIPT_IDENTITY_MISMATCH");
            J counts=J::object(),predictions=J::array(); for(const auto& name:binding.at("geometry_contract"))counts[name.get<std::string>()]=0;
            for(const auto& p:receipt.predictions) {
                check(counts.contains(p.class_id)&&p.coordinate_space=="image.normalized","PREDICTION_CONTRACT_INVALID");
                counts[p.class_id]=counts[p.class_id].get<int>()+1;
                for(double v:p.geometry_coordinates) check(std::isfinite(v)&&v>=0&&v<=1,"COORDINATE_OUT_OF_BOUNDS");
                predictions.push_back({{"id",p.prediction_id},{"class",p.class_id},{"score",p.score},
                    {"bounds",p.bounds},{"geometry",p.geometry_coordinates},{"coordinate_space",p.coordinate_space}});
            }
            auto staging=local.controlled_trial_output_root/local.trial_output_scope/receipt.task_id/"staging";
            auto raw=json(staging/"instances.json"), mapping=json(staging/"weight_mapping_report.json");
            check(mapping.at("complete")==true&&mapping.at("loaded_count")==mapping.at("target_count"),"STRICT_TENSOR_MAPPING_FAILED");
            check(raw.at("weights_hash")==model&&raw.at("transform").at("original_width")==64&&
                raw.at("transform").at("original_height")==64,"RAW_IDENTITY_OR_SIZE_MISMATCH");
            J raw_counts=J::object();for(const auto& name:binding.at("geometry_contract"))raw_counts[name.get<std::string>()]=0;
            for(const auto& instance:raw.at("instances")) {
                std::string name=instance.at("class_name");raw_counts[name]=raw_counts[name].get<int>()+1;
                auto found=std::find_if(receipt.predictions.begin(),receipt.predictions.end(),[&](const auto& p){return p.prediction_id==instance.at("stable_id");});
                check(found!=receipt.predictions.end()&&found->class_id==name,"TYPED_RAW_INSTANCE_MISMATCH");
            }
            check(raw_counts==counts&&raw.at("instances").size()==receipt.predictions.size(),"TYPED_RAW_COUNTS_MISMATCH");
            const auto prior=json(package/"evidence/experimental_training_receipt.json").at("observations")[seed];
            // Same generated source, candidate, preprocessing and decoder must reproduce class counts.
            check(prior.at("class_counts")==counts,"EXPERIMENT_TYPED_COUNTS_MISMATCH");
            J row={{"sample_id",id},{"source_split",local.source_split},{"image_sha256",asset.image_sha256},
                {"task_id",receipt.task_id},{"dataset_revision",receipt.dataset_revision},{"candidate_sha256",model},
                {"verified",receipt.verified},{"typed_conclusion",receipt.conclusion_code},{"class_counts",counts},
                {"target_result",counts[shape].get<int>()?"TARGET_CANDIDATES_REQUIRE_REVIEW":"NO_PROJECT_TARGET_DETECTED"},
                {"predictions",predictions},{"model_quality_accepted",false},
                {"strict_loaded_tensors",mapping.at("loaded_count")},{"raw_typed_counts_equal",true},
                {"target_count",counts[shape]},{"wrong_class_count",receipt.predictions.size()-counts[shape].get<unsigned>()},
                {"nms_cap_reached",prior.at("cap_reached")},{"full_post_nms_count",nullptr},
                {"prior_same_input_geometric_metrics",prior},{"independent_accuracy_evidence",false}};
            observations.push_back(row);write_new(run/(id+"-typed-receipt.json"),row.dump(2));
            std::cout<<id<<" typed inference PASS; counts "<<counts.dump()<<std::endl;
        }
        // Creating an external preview must not alter the original frozen manifest or default session.
        check(sha(run/"frozen-project.json")==frozen_sha,"DEFAULT_MANIFEST_CHANGED");
        check(!baseline.broker->ResolveInferenceAsset(case_id,project_id,"sample-4",model).Ready(),"DEFAULT_SESSION_CHANGED");
        check(sha(package/"handoff.lock.json")==argv[2],"HANDOFF_CHANGED");
        J report={{"schema","visionai.rd.real_typed_preview_acceptance.v1"},{"status","PASS"},
            {"handoff_lock_sha256",argv[2]},{"runtime_sha256",cfg.expected_runtime_sha256},
            {"candidate_sha256",model},{"real_broker",true},{"real_typed_transport",true},{"real_runtime",true},
            {"default_session_unchanged",true},{"external_preview_training_rejected",true},{"negative_pin_tests",rejections},
            {"source","RND_SYNTHETIC_NOT_BUSINESS_DATA"},{"independent_accuracy_evidence",false},
            {"model_quality_accepted",false},{"production_allowed",false},{"observations",observations},
            {"loaded_modules",modules()}};
        write_new(run/"acceptance.json",report.dump(2));std::cout<<"PASS "<<run.generic_string()<<std::endl;
        return 0;
    } catch(const std::exception& e) {
        if(!run.empty())try{write_new(run/"failure.json",J({{"status","FAILED"},{"error",e.what()}}).dump(2));}catch(...){}
        std::cerr<<e.what()<<std::endl;return 1;
    }
}
