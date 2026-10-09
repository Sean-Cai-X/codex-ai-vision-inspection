#include "../../cximage/CxSevenClassSupervisionBinding.h"
#include "../../cxgeom/include/CxGeoSO2ReferenceAsset.h"
#include "muParser.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <chrono>
#include <cmath>
#include <stdexcept>
using J=nlohmann::json;
using namespace cxvision::supervision;
using cxgeom::so2::Sha256Bytes;
namespace fs=std::filesystem;
int checks=0;
void check(bool ok,const char* code) {if(!ok) throw std::runtime_error(code);++checks;}
template<class F> void reject(const char* code,F fn) {
    try {fn();} catch(const std::invalid_argument& e) {check(std::string(e.what())==code,e.what());return;}
    throw std::runtime_error(std::string("missing rejection: ")+code);
}
void write(const fs::path& p,const J& j) {std::ofstream f(p);f<<j.dump(2);f.close();check(bool(f),"write");}
J project() {
    J out={{"project_class","open_curve"},{"samples",J::array()}};
    for(int i=0;i<4;++i) {
        J s={{"asset_ref","sample-"+std::to_string(i)},{"source_group","source-"+std::to_string(i)},
            {"split",i<2?"train":i==2?"valid":"verify"},{"width",32},{"height",32},
            {"image_sha256",Sha256Bytes("synthetic-source-"+std::to_string(i))},{"annotations",J::array()}};
        if(i!=3) {
            s["annotation_receipt_digest"]=Sha256Bytes("annotation-"+std::to_string(i));
            s["annotations"].push_back({{"id","a"},{"class_name","open_curve"},{"closed",false},
                {"points",{{5,5},{5,25},{25,25},{25,5}}}});
        }
        out["samples"].push_back(s);
    }
    return out;
}
J materialized(const J& p,const J& rules) {
    J rows=J::array();
    for(const auto& s:p.at("samples")) if(s.at("split")!="verify") {
        auto c=Convert(s,rules);
        rows.push_back({{"asset_ref",s.at("asset_ref")},{"split",s.at("split")=="valid"?"val":"train"},
            {"image_sha256",s.at("image_sha256")},{"mask_pixels_sha256",c.receipt.at("mask_sha256")},
            {"width",s.at("width")},{"height",s.at("height")}});
    }
    return rows;
}
int main(int argc,char** argv) {
    try {
        if(argc!=3) throw std::runtime_error("rules path and evidence base required");
        fs::path rules_path=fs::absolute(argv[1]); std::ifstream f(rules_path);J rules;f>>rules;
        auto p=project(), frozen=FreezeDataset(p,rules), rows=materialized(p,rules);
        for(const auto& name:rules.at("classes")) {
            auto each=p;each["project_class"]=name;
            for(auto& s:each["samples"]) for(auto& a:s["annotations"]) {
                a["class_name"]=name;
                a["closed"]=name!="arc"&&name!="line"&&name!="open_curve";
                if(name=="line") a["points"]={{5,8},{25,8}};
                if(name=="circle"||name=="ellipse"||name=="arc") {
                    J points=J::array();
                    for(int i=0;i<(name=="arc"?9:24);++i) {
                        double angle=i*3.141592653589793/12;
                        points.push_back({16+9*std::cos(angle),16+(name=="ellipse"?5:9)*std::sin(angle)});
                    }
                    a["points"]=points;
                }
            }
            auto bound=FreezeDataset(each,rules);
            ValidateMaterializedDataset(bound,materialized(each,rules),bound.at("dataset_revision_id"),name);
            ++checks;
        }
        check(DecodeBusinessSha256("sha256:"+std::string(64,'A'))==std::string(64,'a'),"runtime digest prefix normalized");
        reject("SUPERVISION_RUNTIME_SHA_INVALID",[]{DecodeBusinessSha256(std::string(64,'a'));});
        reject("SUPERVISION_RUNTIME_SHA_INVALID",[]{DecodeBusinessSha256("sha256:"+std::string(64,'z'));});
        const std::string revision=frozen.at("dataset_revision_id");
        ValidateMaterializedDataset(frozen,rows,revision,"open_curve");++checks;
        auto shuffled=p;std::reverse(shuffled["samples"].begin(),shuffled["samples"].end());
        check(FreezeDataset(shuffled,rules)==frozen,"input order does not change revision");
        auto changed=rules;changed["open_width_px"]=5.0;
        check(FreezeDataset(p,changed).at("dataset_sha256")!=frozen.at("dataset_sha256"),"width invalidates revision");
        changed=rules;changed["sample_max_step_px"]=0.5;
        check(FreezeDataset(p,changed).at("dataset_sha256")!=frozen.at("dataset_sha256"),"sampling invalidates revision");
        auto q=p;q["samples"][2]["source_group"]=q["samples"][0]["source_group"];
        reject("SUPERVISION_DATASET_SPLIT_LEAKAGE",[&]{FreezeDataset(q,rules);});
        q=p;q["samples"][3]["image_sha256"]=q["samples"][0]["image_sha256"];
        reject("SUPERVISION_DATASET_SPLIT_LEAKAGE",[&]{FreezeDataset(q,rules);});
        q=p;q["samples"][1]["asset_ref"]=q["samples"][0]["asset_ref"];
        reject("SUPERVISION_DATASET_DUPLICATE_ASSET",[&]{FreezeDataset(q,rules);});
        q=p;q["samples"][1]["asset_ref"]="unsafe/id";
        reject("SUPERVISION_DATASET_ASSET_ID_INVALID",[&]{FreezeDataset(q,rules);});
        q=p;q["samples"][1]["annotations"][0]["class_name"]="polygon";
        reject("SUPERVISION_DATASET_CLASS_MIXED",[&]{FreezeDataset(q,rules);});
        q=p;q["samples"][2]["split"]="train";
        reject("SUPERVISION_DATASET_SPLIT_INSUFFICIENT",[&]{FreezeDataset(q,rules);});
        q=p;q["samples"][3]["annotations"]=q["samples"][0]["annotations"];
        reject("SUPERVISION_ANNOTATIONS_FOR_SPLIT_INVALID",[&]{FreezeDataset(q,rules);});
        auto altered=frozen;altered["records"][0]["mask_sha256"]=std::string(64,'a');
        reject("SUPERVISION_DATASET_DIGEST_MISMATCH",[&]{ValidateMaterializedDataset(altered,rows,revision,"open_curve");});
        altered=frozen;altered["project"]["samples"][0]["annotations"][0]["points"][1][0]=6;
        reject("SUPERVISION_DATASET_DIGEST_MISMATCH",[&]{ValidateMaterializedDataset(altered,rows,revision,"open_curve");});
        reject("SUPERVISION_DATASET_BINDING_MISMATCH",[&]{ValidateMaterializedDataset(frozen,rows,"old-revision","open_curve");});
        reject("SUPERVISION_DATASET_BINDING_MISMATCH",[&]{ValidateMaterializedDataset(frozen,rows,revision,"circle");});
        auto bad_rows=rows;bad_rows.erase(0);
        reject("SUPERVISION_MATERIALIZATION_COUNT_MISMATCH",[&]{ValidateMaterializedDataset(frozen,bad_rows,revision,"open_curve");});
        for(const auto* key:{"image_sha256","mask_pixels_sha256"}) {
            bad_rows=rows;bad_rows[0][key]=std::string(64,'f');
            reject("SUPERVISION_MATERIALIZATION_CONTENT_MISMATCH",[&]{ValidateMaterializedDataset(frozen,bad_rows,revision,"open_curve");});
        }
        bad_rows=rows;bad_rows[2]["split"]="valid";
        reject("SUPERVISION_MATERIALIZATION_SPLIT_MISMATCH",[&]{ValidateMaterializedDataset(frozen,bad_rows,revision,"open_curve");});
        bad_rows=rows;bad_rows[2]["asset_ref"]="sample-3";
        reject("SUPERVISION_MATERIALIZATION_ASSET_MISMATCH",[&]{ValidateMaterializedDataset(frozen,bad_rows,revision,"open_curve");});
        bad_rows=rows;bad_rows[0]["width"]=31;
        reject("SUPERVISION_MATERIALIZATION_CONTENT_MISMATCH",[&]{ValidateMaterializedDataset(frozen,bad_rows,revision,"open_curve");});

        fs::path base=fs::absolute(argv[2]);fs::create_directories(base);
        auto dir=base/std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count());
        check(fs::create_directory(dir),"fresh evidence directory");
        auto input=dir/"project.json";write(input,p);
        const std::string script="SevenClassSupervision dataset;dataset.loadrules(\""+rules_path.generic_string()+
            "\");dataset.load(\""+input.generic_string()+"\");dataset.freeze();dataset.expectstatus(\"DATASET_FROZEN\");dataset.savefreeze(\""+
            (dir/"frozen").generic_string()+"\");";
        auto script_path=dir/"freeze_dataset.cxsc";
        {std::ofstream sf(script_path);sf<<script;}
        std::ifstream sf(script_path);std::string actual((std::istreambuf_iterator<char>(sf)),{});
        mu::Parser parser;double* scalar=nullptr;parser.DefineOrgClass("double",scalar);parser.UsingClass(true);
        RegisterSevenClassSupervision(parser);parser.SetExpr(actual);parser.Eval();
        auto* host=static_cast<CxSevenClassSupervision*>(parser.GetClassObj("SevenClassSupervision","dataset"));
        check(host && host->frozen()==frozen,"real script equals native freeze");
        std::ifstream saved(dir/"frozen"/"supervision.v1.json");J saved_json;saved>>saved_json;
        check(saved_json==frozen,"saved frozen sidecar");
        host->load(input.string().c_str());check(host->frozen().is_null(),"reload invalidates frozen result");
        reject("SUPERVISION_SUCCESS_REQUIRED",[&]{host->savefreeze((dir/"stale").string().c_str());});
        J receipt={{"case_name","Seven Class Dataset Freeze and Materialization Guards"},{"status","PASS"},
            {"assertions",checks},{"dataset_sha256",frozen.at("dataset_sha256")},
            {"python_executed",false},{"dll_execution_verified",false},{"training_executed",false}};
        write(dir/"dataset_test_receipt.json",receipt);
        std::cout<<receipt.dump()<<"\nEvidence: "<<dir.string()<<'\n';return 0;
    } catch(const mu::Parser::exception_type& e) {std::cerr<<e.GetMsg()<<'\n';}
    catch(const std::exception& e) {std::cerr<<e.what()<<'\n';}
    return 1;
}
