// Explicit R&D diagnostic: production C ABI inference, no training or activation.
#include "synthetic_materializer.h"
#include "../../libtorch_module/libtorch_module_runtime_c_api.h"
#include "../../libtorch_module/torch_runtime_task_types.h"
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
using namespace supervision_fixture;
namespace {
std::string bytes(const fs::path& p) {
    std::ifstream f(p,std::ios::binary); check(bool(f),"COMPARE_FILE_UNREADABLE");
    return std::string(std::istreambuf_iterator<char>(f),{});
}
J json(const fs::path& p) {return cxvision::supervision::ParseJson(bytes(p));}
std::string hash(const fs::path& p) {return "sha256:"+Sha256Bytes(bytes(p));}
J inventory(const fs::path& root) {
    J r=J::object(); for(const auto& e:fs::recursive_directory_iterator(root))
        if(e.is_regular_file()) r[e.path().lexically_relative(root).generic_string()]=hash(e.path());
    return r;
}
struct Handle {TorchRuntimeHandle p=nullptr;~Handle(){if(p)torch_runtime_destroy(p);}};
struct Result {TorchTaskResult r{};~Result(){torch_runtime_free_result(&r);}};
double iou(const cv::Mat& a,const cv::Mat& b) {
    cv::Mat both,either;cv::bitwise_and(a,b,both);cv::bitwise_or(a,b,either);
    int n=cv::countNonZero(either);return n?double(cv::countNonZero(both))/n:1.0;
}
J boundary_distance(const cv::Mat& a,const cv::Mat& b) {
    cv::Mat ea,eb,da,db;
    cv::erode(a,ea,cv::Mat());cv::subtract(a,ea,ea);
    cv::erode(b,eb,cv::Mat());cv::subtract(b,eb,eb);
    if(!cv::countNonZero(ea)||!cv::countNonZero(eb))return nullptr;
    cv::distanceTransform(255-ea,da,cv::DIST_L2,cv::DIST_MASK_PRECISE);
    cv::distanceTransform(255-eb,db,cv::DIST_L2,cv::DIST_MASK_PRECISE);
    return (cv::mean(db,ea)[0]+cv::mean(da,eb)[0])*0.5;
}
}
int main(int argc,char** argv) {
    fs::path output;
    try {
        if(argc==2&&std::string(argv[1])=="--metrics-self-test") {
            cv::Mat a=cv::Mat::zeros(8,8,CV_8UC1),b=a.clone();
            a(cv::Rect(2,2,2,2)).setTo(255);b(cv::Rect(3,2,2,2)).setTo(255);
            check(std::abs(iou(a,a)-1)<1e-12&&std::abs(iou(a,b)-1.0/3)<1e-12,"METRIC_IOU_FAILED");
            check(boundary_distance(a,a).get<double>()==0&&std::abs(boundary_distance(a,b).get<double>()-0.5)<1e-6,"METRIC_DISTANCE_FAILED");
            check(boundary_distance(a,cv::Mat::zeros(8,8,CV_8UC1)).is_null(),"METRIC_EMPTY_MUST_BE_MISSING");
            std::cout<<"PASS metric identity, shifted mask, missing boundary\n";return 0;
        }
        check(argc==6,"usage: binding parent_root prior_trial_root rules evidence_base");
        auto binding=json(argv[1]);fs::path parent=fs::canonical(argv[2]),trial=fs::canonical(argv[3]);
        auto rules=json(argv[4]);auto summary=json(trial/"trial_summary.json");
        auto candidate=trial/"output/development_trials/ellipse-train/staging/candidate";
        auto pm=json(parent/"model_manifest.json"),cm=json(candidate/"model_manifest.json");
        check(hash(parent/"model_manifest.json")==binding.at("parent_manifest_sha256")&&
            hash(parent/pm.at("weights").get<std::string>())==binding.at("checkpoint_sha256")&&
            hash(parent/binding.at("attestation_reference").get<std::string>())==binding.at("attestation_sha256")&&
            hash(parent/binding.at("lineage_reference").get<std::string>())==binding.at("source_lineage_sha256"),"COMPARE_PARENT_DIGEST_MISMATCH");
        check(hash(candidate/cm.at("weights").get<std::string>())==summary.at("candidate_model_sha256"),"COMPARE_CANDIDATE_DIGEST_MISMATCH");
        check(pm.at("classes")==binding.at("geometry_contract")&&cm.at("classes")==pm.at("classes"),"COMPARE_CLASS_ORDER_MISMATCH");
        check(pm.at("postprocess")==cm.at("postprocess"),"COMPARE_POSTPROCESS_MISMATCH");
        auto before_parent=inventory(parent),before_trial=inventory(trial);
        auto project=json(trial/"ellipse/project.json");
        output=fs::canonical(fresh_root(argv[5]));J observations=J::array();
        for(int m=0;m<2;++m) {
            std::string label=m?"candidate":"parent",model=(m?candidate:parent).generic_string();
            std::string manifest=(fs::path(model)/"model_manifest.json").generic_string(),out=output.generic_string();
            TorchRuntimeConfig config{model.c_str(),out.c_str(),"cpu","info"};Handle handle;
            check(torch_runtime_create(&config,&handle.p)==0&&handle.p,"COMPARE_RUNTIME_CREATE_FAILED");
            for(int seed=0;seed<4;++seed) {
                auto source=project.at("samples")[seed];
                fs::path image_path=trial/"ellipse"/(seed==3?"verify":"training")/("sample-"+std::to_string(seed)+".pgm");
                check(hash(image_path)=="sha256:"+source.at("image_sha256").get<std::string>(),"COMPARE_IMAGE_DIGEST_MISMATCH");
                // Reconstruction uses the exact versioned synthetic generator. Never alters VERIFY metadata.
                if(seed==3) source["annotations"]=J::array({{{"id","analysis-oracle"},{"class_name","ellipse"},
                    {"closed",true},{"points",points("ellipse",3)}}});
                if(seed==3) source["annotation_receipt_digest"]=Sha256Bytes(source["annotations"][0].dump());
                source["split"]="valid";
                auto converted=cxvision::supervision::Convert(source,rules);
                cv::Mat ground(64,64,CV_8UC1,converted.mask.data());cv::Mat truth=ground==2;
                if(seed<3) {
                    auto saved=cv::imread((trial/"ellipse/training"/("sample-"+std::to_string(seed)+"-mask.pgm")).string(),cv::IMREAD_GRAYSCALE);
                    check(saved.size()==ground.size()&&cv::countNonZero(saved!=ground)==0,"COMPARE_SUPERVISION_MISMATCH");
                }
                auto dir=output/(label+"-"+std::to_string(seed));fs::create_directory(dir);
                std::string image=image_path.generic_string(),destination=dir.generic_string();
                TorchTaskRequest req{};req.task=TorchRuntimeTaskIds::YoloV8InstanceSegmentation;
                req.device="cpu";req.input_image=image.c_str();req.manifest_path=manifest.c_str();req.output_dir=destination.c_str();
                Result result;int rc=torch_runtime_run_task(handle.p,&req,&result.r);
                write_new(dir/"call.json",J({{"return_code",rc},{"ok",result.r.ok},
                    {"error",result.r.error_message?result.r.error_message:""}}).dump(2));
                check(rc==0&&result.r.ok,"COMPARE_INFERENCE_FAILED");
                auto evidence=json(dir/"instances.json"),mapping=json(dir/"weight_mapping_report.json");
                check(mapping.at("complete")==true&&mapping.at("loaded_count")==mapping.at("target_count"),"COMPARE_STRICT_LOAD_FAILED");
                check(evidence.at("transform").at("original_width")==64&&evidence.at("transform").at("original_height")==64,"COMPARE_COORDINATE_DIMENSIONS");
                J counts=J::object(),metrics=J::array();double best=0,best_target=0;std::string best_class;
                for(const auto& instance:evidence.at("instances")) {
                    int id=instance.at("class_id");check(id>=0&&id<7&&instance.at("class_name")==pm.at("classes")[id],"COMPARE_CLASS_DECODE_MISMATCH");
                    auto mask=cv::imread(instance.at("binary_mask_ref").get<std::string>(),cv::IMREAD_GRAYSCALE);
                    check(!mask.empty()&&mask.size()==truth.size(),"COMPARE_MASK_COORDINATES");
                    mask=mask>0;double overlap=iou(mask,truth);std::string name=instance.at("class_name");
                    counts[name]=counts.value(name,0)+1;
                    if(overlap>best){best=overlap;best_class=name;}if(id==2)best_target=std::max(best_target,overlap);
                    metrics.push_back({{"id",instance.at("stable_id")},{"class_name",name},{"mask_iou",overlap},
                        {"symmetric_mean_boundary_distance_pixels",boundary_distance(mask,truth)},
                        {"score",instance.at("class_confidence")},{"foreground_pixels",cv::countNonZero(mask)}});
                }
                J row={{"model",label},{"sample_id",seed},{"purpose",seed<2?"Train":seed==2?"Valid":"VERIFY"},
                    {"class_counts",counts},{"best_any_class_iou",best},{"best_any_class_name",best_class},
                    {"best_ellipse_iou",best_target},{"instances",metrics},{"strict_tensor_load",true}};
                observations.push_back(row);std::cout<<row.dump()<<std::endl;
            }
        }
        check(inventory(parent)==before_parent&&inventory(trial)==before_trial,"COMPARE_INPUT_MUTATED");
        J report={{"schema","visionai.native_parent_candidate_comparison.v1"},{"execution_status","PASS"},
            {"observations",observations},{"parent_sha256",binding.at("checkpoint_sha256")},
            {"candidate_sha256",summary.at("candidate_model_sha256")},{"dataset_revision_id",summary.at("dataset_revision_id")},
            {"postprocess",pm.at("postprocess")},{"runtime_version",torch_runtime_version()},
            {"input_files_unchanged",true},{"python_executed",false},{"production_activation",false},
            {"model_quality_accepted",false},{"independent_accuracy_evidence",false},
            {"verify_oracle","synthetic generator only; not training supervision"}};
#ifdef _WIN32
        report["runtime_dll_sha256"]=hash(fs::canonical(argv[0]).parent_path()/"libtorch_module_runtime.dll");
#endif
        write_new(output/"comparison.json",report.dump(2));std::cout<<"Evidence: "<<output<<std::endl;return 0;
    } catch(const std::exception& e) {
        std::cerr<<e.what()<<std::endl;
        if(!output.empty())write_new(output/"failure.json",J({{"error",e.what()}}).dump(2));return 1;
    }
}
