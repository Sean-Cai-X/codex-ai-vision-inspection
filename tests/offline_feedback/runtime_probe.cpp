// Actual C ABI execution on fresh R&D synthetic images only. No business image IO.
#include "../../libtorch_module/libtorch_module_runtime_c_api.h"
#include "../../contracts/OfflineFeedbackValidation.h"
#include "../seven_class_supervision/synthetic_materializer.h"
#include <iostream>
#ifdef _WIN32
#include <Windows.h>
#endif
using namespace supervision_fixture;
namespace fb = vision_ai::offline_feedback;
J read(const fs::path& path) { std::ifstream in(path,std::ios::binary); check(bool(in),"PROBE_READ_FAILED"); J j; in >> j; return j; }
std::string hash(const fs::path& path) {
    std::ifstream in(path,std::ios::binary); check(bool(in),"PROBE_HASH_READ_FAILED");
    return "sha256:" + Sha256Bytes(std::string(std::istreambuf_iterator<char>(in),{}));
}
struct Handle { TorchRuntimeHandle value=nullptr; ~Handle(){ if(value) torch_runtime_destroy(value); } };
struct Result { TorchTaskResult value{}; Result(){torch_runtime_init_result(&value);} ~Result(){torch_runtime_free_result(&value);} };
int main(int argc, char** argv) {
    try {
        check(argc==5,"usage: probe RND-collection supervision-rules feedback-contract output-base");
        const auto run=fresh_root(argv[4]); const auto rules=read(argv[2]); const auto contract=read(argv[3]);
        std::string loadedRuntimeSha;
#ifdef _WIN32
        wchar_t modulePath[32768]; const auto module=GetModuleHandleW(L"libtorch_module_runtime.dll");
        const auto length=GetModuleFileNameW(module,modulePath,32768);
        check(module && length>0 && length<32768,"LOADED_RUNTIME_IDENTITY_FAILED");
        loadedRuntimeSha=hash(fs::path(std::wstring(modulePath,length)));
#else
        check(false,"PROBE_WINDOWS_RUNTIME_IDENTITY_REQUIRED");
#endif
        auto output=run/"output";fs::create_directory(output); J rows=J::array();
        for(const auto* shape:{"circle","ellipse"}) {
            const auto candidate=fs::canonical(fs::path(argv[1])/shape/"candidate");
            const auto manifestPath=candidate/"model_manifest.json";const auto manifest=read(manifestPath);
            const auto source=run/shape;make_case(source,rules,shape,"none",3);
            // An additional wide, non-square synthetic image exercises real letterbox padding.
            std::vector<unsigned char> pixels(192*96,20);
            for(int y=0;y<96;++y) for(int x=0;x<192;++x) {
                const double dx=(x-96)/35.0,dy=(y-48)/(std::string(shape)=="circle"?35.0:23.0);
                if(dx*dx+dy*dy<1) pixels[y*192+x]=220;
            }
            write_new(source/"wide.pgm","P5\n192 96\n255\n"+std::string(pixels.begin(),pixels.end()));
            const std::string modelRoot=candidate.generic_string(),outRoot=output.generic_string();
            TorchRuntimeConfig config{modelRoot.c_str(),outRoot.c_str(),"cpu","info"};
            Handle handle;check(torch_runtime_create(&config,&handle.value)==0 && handle.value,"RUNTIME_CREATE_FAILED");
            for(int index=0;index<4;++index) {
                const auto image=index==3?source/"wide.pgm":source/(index==2?"verify":"training")/("sample-"+std::to_string(index==0?0:index==1?3:4)+".pgm");
                const auto stage=output/"development_trials"/(std::string(shape)+"-"+std::to_string(index))/"staging";
                fs::create_directories(stage);
                const std::string input=image.generic_string(),mp=manifestPath.generic_string(),op=stage.generic_string();
                const std::string imageSha=hash(image);
                const auto extra=J{{"verify_asset_binding_id","synthetic-"+std::to_string(index)},{"verify_asset_sha256",imageSha}}.dump();
                TorchTaskRequest request{};request.device="cpu";request.input_image=input.c_str();request.manifest_path=mp.c_str();request.output_dir=op.c_str();request.extra_json=extra.c_str();
                Result result; const auto rc=torch_runtime_run_business_inference_v1(handle.value,&request,&result.value);
                check(rc==0 && result.value.ok==1,"REAL_BUSINESS_INFERENCE_FAILED");
                const auto diagnostic=fb::ExtractInferenceDiagnostics(result.value.result_json,contract);
                check(!diagnostic.is_null(),"DIAGNOSTICS_MISSING");
                const auto instances=read(stage/"instances.json"); const auto metrics=read(stage/"segmentation_metrics.json");
                check(diagnostic==metrics.at("diagnostics"),"RAW_RETURN_DIAGNOSTICS_DIFFER");
                check(metrics.at("full_post_nms_count")==diagnostic.at("counts").at("full_post_nms_count"),"LEGACY_FULL_COUNT_DIFFER");
                J counts=J::array({0,0,0,0,0,0,0});
                for(const auto& instance:instances.at("instances")) {
                    const auto id=instance.at("class_id").get<std::size_t>();check(id<7,"CLASS_ID_INVALID");
                    counts[id]=counts[id].get<unsigned>()+1;
                }
                check(counts==diagnostic.at("raw_class_counts"),"RAW_RETURN_CLASS_COUNTS_DIFFER");
                check(diagnostic.at("counts").at("returned_count")==instances.at("instances").size(),"RETURNED_COUNT_DIFFER");
                check(diagnostic.at("transform").at("original_width")== (index==3?192:64) && diagnostic.at("transform").at("original_height")== (index==3?96:64),"ORIGINAL_DIMENSIONS_DIFFER");
                if(index==3) check(diagnostic.at("transform").at("pad_y")==160 && diagnostic.at("transform").at("pad_x")==0 &&
                    diagnostic.at("transform").at("resized_width")==640 && diagnostic.at("transform").at("resized_height")==320 &&
                    std::abs(diagnostic.at("transform").at("scale").get<double>()-640.0/192)<1e-10,"WIDE_PADDING_NOT_TESTED");
                const auto mapping=read(stage/"weight_mapping_report.json");check(mapping.at("loaded_count")==417 && mapping.at("complete")==true,"STRICT_WEIGHT_LOAD_FAILED");
                rows.push_back({{"shape",shape},{"source_role",index==0?"Train":index==1?"Valid":index==2?"VERIFY":"RND_WIDE"},
                    {"image_sha256",imageSha},{"candidate_manifest_sha256",hash(manifestPath)},
                    {"checkpoint_sha256",manifest.at("weights_hash")},{"diagnostics",diagnostic},{"loaded_count",417}});
                std::cout << shape << ' ' << index << " real C ABI diagnostics PASS\n";
            }
        }
        write_new(run/"diagnostic_acceptance.json",J{{"schema","visionai.rd.runtime_diagnostics_acceptance.v1"},{"status","PASS"},
            {"loaded_runtime_sha256",loadedRuntimeSha},
            {"real_c_abi",true},{"business_typed_receipt_extension_integrated",false},{"synthetic_only",true},
            {"model_quality_accepted",false},{"production_allowed",false},{"observations",rows}}.dump(2));
        std::cout << "EVIDENCE: " << run.generic_string() << '\n';return 0;
    } catch(const std::exception& e){std::cerr << e.what() << '\n';return 1;}
}
