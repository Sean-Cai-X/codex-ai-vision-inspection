#include "synthetic_materializer.h"
#include "../../cximage/CxInstanceMaskErrorAnalysis.h"
#include "../../libtorch_module/libtorch_module_runtime_c_api.h"
#include "../../libtorch_module/torch_runtime_task_types.h"
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
using namespace supervision_fixture;
using cxvision::mask_error::Prediction;
using cxvision::mask_error::AnalyzeSingleGroundTruth;
namespace {
std::string bytes(const fs::path& p){std::ifstream f(p,std::ios::binary);check(bool(f),"AUDIT_FILE_UNREADABLE");return {std::istreambuf_iterator<char>(f),{}};}
J json(const fs::path& p){return J::parse(bytes(p));}
std::string sha(const fs::path& p){return "sha256:"+Sha256Bytes(bytes(p));}
void lock(const fs::path& root,const std::string& expected){
    check(sha(root/"handoff.lock.json")==expected,"AUDIT_LOCK_MISMATCH");
    const auto document=json(root/"handoff.lock.json");
    for(const auto& e:document.at("files")){
        fs::path rel=e.at("relative_path").get<std::string>();check(!rel.is_absolute(),"AUDIT_PATH_ESCAPE");
        for(const auto& part:rel)check(part!="..","AUDIT_PATH_ESCAPE");
        check(sha(root/rel)==e.at("sha256"),"AUDIT_ASSET_SHA_MISMATCH");
    }
}
struct Handle{TorchRuntimeHandle p=nullptr;~Handle(){if(p)torch_runtime_destroy(p);}};
struct Result{TorchTaskResult r{};Result(){torch_runtime_init_result(&r);}~Result(){torch_runtime_free_result(&r);}};
void tests(){
    cv::Mat gt=cv::Mat::zeros(40,80,CV_8UC1);gt(cv::Rect(10,10,10,10)).setTo(255);
    auto exact=AnalyzeSingleGroundTruth(gt.size(),1,gt,{{"a",1,.8,gt}});
    check(exact.at("best_target_iou")==1&&exact.at("unmatched_gt_count")==0&&exact.at("error_tags").empty(),"TEST_EXACT");
    auto wrong=AnalyzeSingleGroundTruth(gt.size(),1,gt,{{"a",2,.8,gt}});
    check(wrong.at("best_target_iou")==0&&wrong.at("best_any_class_iou")==1&&wrong.at("error_tags")[0]=="ClassConfusion","TEST_WRONG_CLASS");
    cv::Mat shift=cv::Mat::zeros(gt.size(),CV_8UC1);shift(cv::Rect(40,10,10,10)).setTo(255);
    auto offset=AnalyzeSingleGroundTruth(gt.size(),1,gt,{{"a",1,.8,shift}});
    check(offset.at("target_count")==1&&offset.at("best_any_class_iou")==0&&offset.at("predictions")[0].at("center_error_pixels")==30,"TEST_WRONG_LOCATION");
    auto missing=AnalyzeSingleGroundTruth(gt.size(),1,gt,{});check(missing.at("unmatched_gt_count")==1&&missing.at("best_any_class").is_null(),"TEST_MISSING");
    auto unlabelled=AnalyzeSingleGroundTruth(gt.size(),1,{},{{"a",2,.8,gt}});
    check(unlabelled.at("best_any_class_iou").is_null()&&unlabelled.at("unmatched_gt_count").is_null()&&unlabelled.at("error_tags")[0]=="GeometryUncertain","TEST_UNLABELLED");
    auto duplicate=AnalyzeSingleGroundTruth(gt.size(),1,gt,{{"b",1,.9,gt},{"a",2,.8,gt}});
    check(duplicate.at("duplicate_candidate_pairs")==1&&duplicate.at("unmatched_prediction_count")==1&&duplicate.at("matched_prediction_id")=="a","TEST_DUPLICATE_AND_TIE");
    for(int i=0;i<3;++i){bool rejected=false;try{
        if(i==0)AnalyzeSingleGroundTruth(gt.size(),1,gt,{{"a",1,.8,cv::Mat::zeros(20,80,CV_8UC1)}});
        if(i==1)AnalyzeSingleGroundTruth(gt.size(),1,cv::Mat::zeros(gt.size(),CV_8UC1),{});
        if(i==2)AnalyzeSingleGroundTruth(gt.size(),1,gt,{},0);
    }catch(const std::runtime_error&){rejected=true;}check(rejected,"TEST_INVALID_INPUT");}
    std::cout<<"PASS: exact, wrong class, wrong location, missing, unlabelled, duplicates/ties, dimensions, empty GT, threshold\n";
}
struct Scene{std::string name;cv::Mat image,mask;J definition;};
std::vector<Scene> scenes(const std::string& shape,const J& rules){
    J annotation={{"id","oracle"},{"class_name",shape},{"closed",true},{"points",points(shape,0)}};
    J source={{"asset_ref","rd-transfer"},{"source_group","rd-transfer-seed0"},{"split","valid"},
        {"image_sha256",std::string(64,'0')},{"annotation_receipt_digest",Sha256Bytes(annotation.dump())},
        {"width",64},{"height",64},{"annotations",J::array({annotation})}};
    auto conversion=cxvision::supervision::Convert(source,rules);int target=shape=="circle"?1:2;
    cv::Mat mask=(cv::Mat(64,64,CV_8UC1,conversion.mask.data())==target),image(64,64,CV_8UC1);
    for(int y=0;y<64;++y)for(int x=0;x<64;++x)image.at<unsigned char>(y,x)=mask.at<unsigned char>(y,x)?210:20+(x+y*3)%7;
    std::vector<Scene> result;
    struct Spec{const char* name;int w,h;double scale,angle,dx,dy;};
    for(const auto& s:std::vector<Spec>{{"nominal",64,64,1,0,0,0},{"translated",64,64,1,0,-10,9},
        {"small",64,64,.45,0,0,0},{"rotated45",64,64,1,45,0,0},
        {"wide_letterbox",160,80,1,0,0,0},{"tall_letterbox",80,160,1,0,0,0}}){
        auto a=cv::getRotationMatrix2D({32,32},s.angle,s.scale);
        a.at<double>(0,2)+=s.w*.5-32+s.dx;a.at<double>(1,2)+=s.h*.5-32+s.dy;
        cv::Mat im,gt;cv::warpAffine(image,im,a,{s.w,s.h},cv::INTER_LINEAR,cv::BORDER_CONSTANT,23);
        cv::warpAffine(mask,gt,a,{s.w,s.h},cv::INTER_NEAREST,cv::BORDER_CONSTANT,0);
        result.push_back({s.name,im,gt,{{"width",s.w},{"height",s.h},{"scale",s.scale},{"angle",s.angle},{"dx",s.dx},{"dy",s.dy}}});
    }
    result.push_back({"inverted",255-image,mask.clone(),{{"photometry","255-original"}}});
    cv::Mat low;image.convertTo(low,CV_8UC1,.13,112);result.push_back({"low_contrast",low,mask.clone(),{{"gain",.13},{"offset",112}}});
    cv::Mat textured=image.clone();for(int y=0;y<64;++y)for(int x=0;x<64;++x)
        if(!mask.at<unsigned char>(y,x))textured.at<unsigned char>(y,x)=((x/3+y/3)%2)?170:35;
    result.push_back({"textured_background",textured,mask.clone(),{{"pattern","3px_checker_35_170_background_only"}}});
    return result;
}
}
int main(int argc,char** argv){
    fs::path run;
    try{
        if(argc==2&&std::string(argv[1])=="--self-test"){tests();return 0;}
        check(argc==6,"usage: image_error_probe delivery_root independent_lock_sha experiment_root output_base --rnd-synthetic-only");
        auto delivery=fs::canonical(argv[1]),experiment=fs::canonical(argv[3]);lock(delivery,argv[2]);
        auto runtime_sha=sha(fs::canonical(argv[0]).parent_path()/"libtorch_module_runtime.dll");
        check(std::string(argv[5])=="--rnd-synthetic-only","RND_ONLY_REQUIRED");
        run=fs::canonical(fresh_root(argv[4]));
        J report={{"schema","visionai.rd.synthetic_transfer_error_audit.v1"},{"observations",J::array()},
            {"runtime_sha256",runtime_sha},{"handoff_lock_sha256",argv[2]},{"training_executed",false},
            {"source","RND_SYNTHETIC_PERTURBATIONS"},{"independent_accuracy_evidence",false},{"production_allowed",false}};
        auto deadline=std::chrono::steady_clock::now()+std::chrono::minutes(5);
        for(const std::string shape:{"circle","ellipse"}){
            auto package=delivery/shape;auto dm=json(package/"delivery_manifest.json");auto base_manifest=json(package/"candidate/model_manifest.json");
            check(dm.at("runtime_sha256")==runtime_sha,"RUNTIME_SHA_MISMATCH");
            auto rules=json(package/"rules/seven_class_supervision_rules.v1.json");auto inputs=scenes(shape,rules);
            fs::create_directory(run/shape);
            for(auto& scene:inputs){
                auto p=run/shape/(scene.name+".pgm");check(cv::imwrite(p.string(),scene.image),"SYNTHETIC_WRITE_FAILED");
                check(cv::imwrite((run/shape/(scene.name+"-gt.pgm")).string(),scene.mask),"SYNTHETIC_WRITE_FAILED");
            }
            for(const std::string arm:{"legacy_v1","unified_v5_hard_negative_experimental"}){
                auto src=experiment/(shape+"-"+arm);auto result=json(src/"result.json");auto weights=src/"weights.pt";
                check(sha(weights)==result.at("checkpoint_sha256")&&result.at("parent_sha256")==dm.at("parent_checkpoint_sha256")&&
                    result.at("epochs")==40&&result.at("updates")==40&&result.at("batch_size")==2&&
                    result.at("dataset_revision")==dm.at("dataset_revision")&&result.at("seed")==20261012,
                    "AUDIT_TRAINING_PROVENANCE_MISMATCH");
                if(arm!="legacy_v1")check(result.at("checkpoint_sha256")==dm.at("candidate_checkpoint_sha256"),"AUDIT_CANDIDATE_MISMATCH");
                auto modelroot=run/shape/arm;fs::create_directory(modelroot);fs::copy_file(weights,modelroot/"weights.pt");
                auto manifest=base_manifest;manifest["weights"]="weights.pt";manifest["weights_hash"]=result.at("checkpoint_sha256");
                const std::string prepost_sha="sha256:"+Sha256Bytes(J({{"input",manifest.at("input")},{"postprocess",manifest.at("postprocess")}}).dump());
                manifest["model_id"]="rd-transfer-"+shape+"-"+arm;manifest["training_loss_version"]=arm;
                write_new(modelroot/"model_manifest.json",manifest.dump(2));
                std::string model=modelroot.generic_string(),output=run.generic_string(),mp=(modelroot/"model_manifest.json").generic_string();
                TorchRuntimeConfig config{model.c_str(),output.c_str(),"cpu","info"};Handle handle;
                check(torch_runtime_create(&config,&handle.p)==0&&handle.p,"AUDIT_RUNTIME_CREATE_FAILED");
                for(const auto& scene:inputs){
                    check(std::chrono::steady_clock::now()<deadline,"AUDIT_TIME_BUDGET");
                    auto image=run/shape/(scene.name+".pgm");auto before=sha(image);auto dest=modelroot/scene.name;fs::create_directory(dest);
                    std::string ip=image.generic_string(),op=dest.generic_string();TorchTaskRequest request{};
                    request.task=TorchRuntimeTaskIds::YoloV8InstanceSegmentation;request.device="cpu";request.input_image=ip.c_str();request.manifest_path=mp.c_str();request.output_dir=op.c_str();
                    Result res;int rc=torch_runtime_run_task(handle.p,&request,&res.r);check(rc==0&&res.r.ok,"AUDIT_INFERENCE_FAILED");
                    auto evidence=json(dest/"instances.json"),mapping=json(dest/"weight_mapping_report.json");
                    check(mapping.at("complete")==true&&mapping.at("loaded_count")==417,"AUDIT_STRICT_LOAD_FAILED");
                    check(evidence.at("weights_hash")==result.at("checkpoint_sha256"),"AUDIT_LOADED_MODEL_MISMATCH");
                    auto transform=evidence.at("transform");check(transform.at("original_width")==scene.image.cols&&transform.at("original_height")==scene.image.rows,"AUDIT_COORDINATE_MISMATCH");
                    std::vector<Prediction> predictions;
                    for(const auto& p:evidence.at("instances")){
                        auto m=cv::imread(p.at("binary_mask_ref").get<std::string>(),cv::IMREAD_GRAYSCALE);
                        predictions.push_back({p.at("stable_id"),p.at("class_id"),p.at("class_confidence"),m>0});
                    }
                    auto metrics=AnalyzeSingleGroundTruth(scene.image.size(),shape=="circle"?1:2,scene.mask,predictions);
                    auto nms=json(dest/"segmentation_metrics.json");
                    J row={{"shape",shape},{"arm",arm},{"sample_id",scene.name},{"sample_purpose","RND_STRESS_NOT_BUSINESS_VERIFY"},
                        {"image_sha256",before},{"annotation_sha256",sha(run/shape/(scene.name+"-gt.pgm"))},
                        {"model_sha256",result.at("checkpoint_sha256")},{"runtime_sha256",runtime_sha},
                        {"prepost_configuration_sha256",prepost_sha},
                        {"preprocessing_configuration",manifest.at("input")},{"postprocessing_configuration",manifest.at("postprocess")},
                        {"training_receipt_sha256",sha(src/"result.json")},{"training_trace_sha256",sha(src/"trace.json")},
                        {"training_dataset_revision",result.at("dataset_revision")},{"transform",transform},
                        {"synthetic_definition",scene.definition},{"metrics",metrics},{"nms",nms}};
                    check(sha(image)==before,"AUDIT_SOURCE_CHANGED");write_new(dest/"image_error.json",row.dump(2));report["observations"].push_back(row);
                    std::cout<<shape<<' '<<arm<<' '<<scene.name<<" target="<<metrics["best_target_iou"]<<" any="<<metrics["best_any_class_iou"]<<' '<<metrics["error_tags"]<<std::endl;
                }
                check(sha(weights)==result.at("checkpoint_sha256"),"AUDIT_CHECKPOINT_CHANGED");
            }
        }
        lock(delivery,argv[2]);report["execution_status"]="PASS";write_new(run/"transfer_error_audit.json",report.dump(2));
        std::cout<<"PASS execution (NOT model quality): "<<run.generic_string()<<std::endl;return 0;
    }catch(const std::exception& e){if(!run.empty())try{write_new(run/"failure.json",J({{"error",e.what()}}).dump(2));}catch(...){}std::cerr<<e.what()<<std::endl;return 1;}
}
