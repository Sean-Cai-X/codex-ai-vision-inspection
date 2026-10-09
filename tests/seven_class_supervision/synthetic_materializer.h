#pragma once
// R&D fixtures only. Never opens/edits a business image or an existing run.
#include "../../cximage/CxSevenClassSupervision.h"
#include "../../cxgeom/include/CxGeoSO2ReferenceAsset.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace supervision_fixture {
using J=nlohmann::json;
namespace fs=std::filesystem;
using cxgeom::so2::Sha256Bytes;
inline void check(bool ok,const char* code) {if(!ok) throw std::runtime_error(code);}
inline void write_new(const fs::path& p,const std::string& bytes) {
    check(!fs::exists(p),"FIXTURE_OVERWRITE_FORBIDDEN");
    std::ofstream f(p,std::ios::binary);f.write(bytes.data(),bytes.size());f.close();
    check(bool(f),"FIXTURE_WRITE_FAILED");
}
inline std::string pgm(const std::vector<unsigned char>& bytes) {
    return "P5\n64 64\n255\n"+std::string(bytes.begin(),bytes.end());
}
inline J points(const std::string& name,int seed) {
    double shift=seed*1.0;
    if(name=="line") return {{12+shift,18},{48,44-shift}};
    if(name=="open_curve") return {{12+shift,12},{12+shift,48},{48,48},{48,16+shift}};
    if(name=="polygon") return {{12+shift,12},{50,16+shift},{46,49},{14,46}};
    J p=J::array();
    for(int i=0;i<(name=="arc"?17:48);++i) {
        double t=i*2*3.141592653589793/48;
        double r=18+shift+(name=="closed_curve"?2*std::cos(3*t):0);
        p.push_back({32+r*std::cos(t),32+(name=="ellipse"?r*0.65:r)*std::sin(t)});
    }
    return p;
}
inline J make_case(const fs::path& root,const J& rules,const std::string& name,
                   const std::string& fault="none") {
    check(fs::create_directory(root),"FIXTURE_NEW_CASE_REQUIRED");
    fs::create_directory(root/"training");fs::create_directory(root/"verify");
    J project={{"project_class",name},{"samples",J::array()}};
    J assets=J::array();
    for(int seed=0;seed<4;++seed) {
        bool closed=name!="arc"&&name!="line"&&name!="open_curve";
        J annotation={{"id","a"},{"class_name",name},{"closed",closed},{"points",points(name,seed)}};
        J source={{"asset_ref","sample-"+std::to_string(seed)},{"source_group",name+"-draw-"+std::to_string(seed)},
            {"split",seed<2?"train":"valid"},{"width",64},{"height",64},
            {"image_sha256",std::string(64,'0')},{"annotation_receipt_digest",Sha256Bytes(annotation.dump())},
            {"annotations",J::array({annotation})}};
        auto conversion=cxvision::supervision::Convert(source,rules);
        std::vector<unsigned char> image(4096);
        for(int y=0;y<64;++y) for(int x=0;x<64;++x)
            image[y*64+x]=conversion.mask[y*64+x]==255?static_cast<unsigned char>(20+(x*(seed+1)+y*3)%7):210;
        std::string image_bytes=pgm(image);
        source["image_sha256"]=Sha256Bytes(image_bytes);
        if(seed==3) {source["split"]="verify";source.erase("annotation_receipt_digest");source["annotations"]=J::array();}
        project["samples"].push_back(source);
        std::string image_name=(seed==3?"verify/":"training/")+std::string("sample-")+std::to_string(seed)+".pgm";
        write_new(root/image_name,image_bytes);
        if(seed!=3) {
            auto mask=conversion.mask;
            // Deliberately bad fixture created once, never mutate an original.
            if(fault=="wrong_mask_pixels" && seed==0) {
                std::fill(mask.begin(),mask.end(),255);
                for(int y=8;y<22;++y) for(int x=8;x<22;++x) mask[y*64+x]=static_cast<unsigned char>(
                    std::find(rules.at("classes").begin(),rules.at("classes").end(),name)-rules.at("classes").begin());
            }
            std::string mask_name="training/sample-"+std::to_string(seed)+"-mask.pgm";
            std::string mask_bytes=pgm(mask);write_new(root/mask_name,mask_bytes);
            assets.push_back({{"id",source["asset_ref"]},{"split",seed<2?"train":"val"},
                {"image_path",image_name},{"image_sha256",Sha256Bytes(image_bytes)},
                {"mask_path",mask_name},{"mask_sha256",Sha256Bytes(mask_bytes)}});
        }
    }
    auto frozen=cxvision::supervision::FreezeDataset(project,rules);
    const std::string sidecar=frozen.dump();
    write_new(root/"training"/"supervision.v1.json",sidecar);
    write_new(root/"project.json",project.dump(2));
    write_new(root/"verify"/"asset_binding.json",project["samples"][3].dump(2));
    const std::string case_id="native-"+name;
    std::ostringstream manifest;
    manifest<<"schema=visionai.geometry-segmentation-materializer."<<(fault=="v1_with_binding"?"v1":"v2")<<'\n'
        <<"snapshot_id="<<case_id<<"\nsnapshot_digest=sha256:"<<Sha256Bytes(project.dump())
        <<"\ncase_id="<<case_id<<"\ndataset_revision_id="<<frozen.at("dataset_revision_id").get<std::string>()
        <<"\nannotation_receipt_digest=sha256:"<<Sha256Bytes(project.at("samples").dump())
        <<"\nbackground_pixel=255\nboundary_stroke_width_pixels="<<(fault=="wrong_width"?5.0:rules.at("open_width_px").get<double>())
        <<"\nsupervision_rules_version=1.0.0\nsupervision_rules_sha256=sha256:"<<frozen.at("rules_sha256").get<std::string>()
        <<"\nsupervision_dataset_sha256=sha256:"<<frozen.at("dataset_sha256").get<std::string>()
        <<"\nsupervision_file_sha256=sha256:"<<(fault=="wrong_sidecar_hash"?std::string(64,'f'):Sha256Bytes(sidecar))<<'\n';
    for(const auto& a:assets) {
        // Paths in the training root are relative to that root, not suite root.
        std::string img=fs::path(a.at("image_path").get<std::string>()).filename().string();
        std::string mask=fs::path(a.at("mask_path").get<std::string>()).filename().string();
        manifest<<"image="<<a.at("id").get<std::string>()<<'|'<<a.at("split").get<std::string>()<<'|'<<img<<"|sha256:"<<a.at("image_sha256").get<std::string>()<<'\n';
        manifest<<"mask="<<a.at("id").get<std::string>()<<'|'<<a.at("split").get<std::string>()<<'|'<<mask<<"|sha256:"
            <<(fault=="wrong_encoded_mask_hash"?std::string(64,'e'):a.at("mask_sha256").get<std::string>())<<'\n';
    }
    if(fault=="verify_in_training") manifest<<"image=sample-3|verify|sample-3.pgm|sha256:"<<project["samples"][3]["image_sha256"].get<std::string>()<<'\n';
    write_new(root/"training"/"manifest.v1",manifest.str());
    J binding={{"case_name","Seven Class Native "+name+" Materialization"+
        (fault=="none"?std::string():" Rejection "+fault)},{"case_id",case_id},{"project_class",name},
        {"training_root",(root/"training").generic_string()},{"dataset_revision_id",frozen.at("dataset_revision_id")},
        {"dataset_manifest_sha256","sha256:"+Sha256Bytes(manifest.str())},{"fault",fault},
        {"source_kind","deterministic_rnd_synthetic"},{"independent_model_accuracy_evidence",false},
        {"training_executed",false},{"production_allowed",false}};
    write_new(root/"materialization_binding.json",binding.dump(2));
    return binding;
}
inline fs::path fresh_root(const fs::path& base) {
    fs::create_directories(base);
    auto root=fs::absolute(base)/std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    check(fs::create_directory(root),"FIXTURE_NEW_RUN_REQUIRED");return root;
}
inline J make_suite(const fs::path& root,const J& rules) {
    J cases=J::array();
    for(const auto& value:rules.at("classes")) {
        auto name=value.get<std::string>();cases.push_back(make_case(root/name,rules,name));
    }
    J receipt={{"schema","visionai.native_synthetic_materialization.v1"},{"admission","development_trial"},
        {"cases",cases},{"python_executed",false},{"training_executed",false},{"independent_model_accuracy_evidence",false}};
    write_new(root/"case_index.json",receipt.dump(2));return receipt;
}
}
