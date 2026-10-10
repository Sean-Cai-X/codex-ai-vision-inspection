#include "../../cximage/CxSevenClassSupervisionBinding.h"
#include "../../cxgeom/include/CxGeoSO2ReferenceAsset.h"
#include "muParser.h"
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using J=nlohmann::json;
namespace fs=std::filesystem;
using cxvision::supervision::Convert;
using cxgeom::so2::Sha256Bytes;
namespace {
int checks=0;
void check(bool ok,const char* label) {
    if(!ok) throw std::runtime_error(label);
    ++checks;
}
J read(const fs::path& p) { std::ifstream f(p); J j; f>>j; return j; }
void write(const fs::path& p,const std::string& s) {
    std::ofstream f(p,std::ios::binary); f<<s; f.close();
    if(!f) throw std::runtime_error("fixture write failed");
}
J source(const std::string& name="line", J points={{5,8},{25,8}},bool closed=false) {
    return {{"asset_ref","synthetic-"+name},{"source_group","native-generated-"+name},
        {"split","train"},{"width",32},{"height",32},{"image_sha256",std::string(64,'1')},
        {"annotation_receipt_digest",std::string(64,'2')},
        {"annotations",J::array({{{"id","a"},{"class_name",name},{"points",points},{"closed",closed}}})}};
}
template<class F> void reject(const char* expected,F fn) {
    try { fn(); } catch(const std::invalid_argument& e) {
        check(std::string(e.what())==expected, e.what()); return;
    }
    throw std::runtime_error(std::string("missing rejection: ")+expected);
}
void math_cases(const J& rules) {
    check(Sha256Bytes("abc")=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","SHA256 abc");
    check(Sha256Bytes(std::string(1000000,'a'))=="cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0","SHA256 million a");
    reject("ASSET_SIZE_LIMIT",[]{ cxgeom::so2::ReferenceSha256(std::string(262145,'x')); });
    auto u=source("open_curve",{{5,5},{5,25},{25,25},{25,5}});
    auto r=Convert(u,rules);
    check(r.mask[5*32+15]==255,"no synthetic closing edge");
    check(r.mask[16*32+16]==255,"no bounding box fill");
    check(r.receipt["instances"][0]["endpoints"]==J({{5,5},{25,5}}),"source endpoints retained");
    auto line=source(); auto narrow=Convert(line,rules); auto wide_rules=rules;
    wide_rules["open_width_px"]=5.0; auto wide=Convert(line,wide_rules);
    check(narrow.mask[6*32+15]==255 && wide.mask[6*32+15]==3,"stroke width effective");
    check(narrow.mask[8*32+4]==3 && narrow.mask[8*32+3]==255,"round cap");
    check(narrow.receipt["sample_binding_sha256"]!=wide.receipt["sample_binding_sha256"],"width bound into digest");
    auto dense_rules=rules; dense_rules["sample_max_step_px"]=0.5;
    auto dense=Convert(line,dense_rules);
    check(dense.mask==narrow.mask,"sampling does not change raster");
    check(dense.receipt["sample_binding_sha256"]!=narrow.receipt["sample_binding_sha256"],"sampling bound into digest");
    const auto& pts=dense.receipt["instances"][0]["samples"];
    for(std::size_t i=1;i<pts.size();++i) {
        double dx=pts[i][0].get<double>()-pts[i-1][0].get<double>();
        double dy=pts[i][1].get<double>()-pts[i-1][1].get<double>();
        check(std::hypot(dx,dy)<=0.50000001,"sample spacing");
    }
    auto border=Convert(source("line",{{0,0},{20,0}}),rules);
    check(border.receipt["instances"][0]["touches_image_border"]==true,"border clipping reported");
    check(border.receipt["instances"][0]["endpoints"][0]==J({0,0}),"border endpoint preserved");
    auto verify=source(); verify["split"]="verify"; verify["annotations"]=J::array(); verify.erase("annotation_receipt_digest");
    auto vr=Convert(verify,rules);
    check(vr.mask.empty()&&vr.receipt["mask_sha256"].is_null(),"VERIFY never becomes background mask");
    check(vr.receipt["status"]=="VERIFY_NO_MASK","VERIFY status");
    auto invalid=rules; invalid["version"]="9.0.0";
    reject("SUPERVISION_RULES_UNSUPPORTED",[&]{Convert(line,invalid);});
    invalid=rules; invalid["open_width_px"]=0;
    reject("SUPERVISION_WIDTH_INVALID",[&]{Convert(line,invalid);});
    reject("SUPERVISION_TOPOLOGY_MISMATCH",[&]{Convert(source("open_curve",{{5,5},{20,20}},true),rules);});
    reject("SUPERVISION_REPEATED_VERTEX",[&]{Convert(source("open_curve",{{5,5},{20,20},{5,5}}),rules);});
    reject("SUPERVISION_SELF_INTERSECTION",[&]{Convert(source("polygon",{{5,5},{25,25},{5,25},{25,5}},true),rules);});
    reject("SUPERVISION_SEGMENT_BACKTRACK",[&]{Convert(source("open_curve",{{5,5},{20,5},{10,5}}),rules);});
    reject("SUPERVISION_VERTEX_OUT_OF_BOUNDS",[&]{Convert(source("line",{{-1,0},{20,5}}),rules);});
    reject("SUPERVISION_CLASS_INVALID",[&]{Convert(source("rectangle"),rules);});
    reject("SUPERVISION_RASTER_HOLE",[&]{Convert(source("open_curve",{{5,5},{5,25},{25,25},{25,5},{7,5}}),rules);});
    auto overlap=line; auto instance=overlap["annotations"][0]; instance["id"]="b"; overlap["annotations"].push_back(instance);
    reject("SUPERVISION_INSTANCES_TOUCH_OR_OVERLAP",[&]{Convert(overlap,rules);});
    auto missing=line; missing.erase("annotation_receipt_digest");
    reject("SUPERVISION_FIELDS_INVALID",[&]{Convert(missing,rules);});
    auto unknown=line; unknown["mask"]="ignored";
    reject("SUPERVISION_FIELDS_INVALID",[&]{Convert(unknown,rules);});
    auto too_big=line; too_big["width"]=20000;too_big["height"]=20000;
    reject("SUPERVISION_PIXEL_LIMIT",[&]{Convert(too_big,rules);});
    auto split=line; split["split"]="valid";
    check(Convert(split,rules).receipt["sample_binding_sha256"]!=narrow.receipt["sample_binding_sha256"],"split bound into digest");
}
void script_cases(const fs::path& rules_path,const J& rules,const fs::path& out) {
    J index=J::array();
    for(const auto& value:rules.at("classes")) {
        std::string name=value.get<std::string>();
        bool closed=name!="arc"&&name!="line"&&name!="open_curve";
        J pts={{5,5},{5,25},{25,25},{25,5}};
        if(name=="line") pts={{5,8},{25,8}};
        if(name=="circle"||name=="ellipse"||name=="arc") {
            pts=J::array();
            int n=name=="arc"?9:24;
            for(int i=0;i<n;++i) {double a=i*3.141592653589793/12;pts.push_back({16+9*std::cos(a),16+(name=="ellipse"?5:9)*std::sin(a)});}
        }
        J input=source(name,pts,closed);
        fs::path fixture=out/(name+".json"), run=out/(name+"-run");
        write(fixture,input.dump(2));
        std::string script="SevenClassSupervision conversion;\nconversion.loadrules(\""+rules_path.generic_string()+
            "\");\nconversion.load(\""+fixture.generic_string()+"\");\nconversion.run();\nconversion.expectstatus(\"CONVERTED\");\nconversion.save(\""+
            run.generic_string()+"\");\n";
        fs::path script_path=out/(name+".cxsc"); write(script_path,script);
        mu::Parser parser; double* scalar=nullptr;
        parser.DefineOrgClass("double",scalar); parser.UsingClass(true);
        RegisterSevenClassSupervision(parser);
        // Execute the saved CxScript, not a parallel test-only interpreter.
        std::ifstream sf(script_path); std::string actual((std::istreambuf_iterator<char>(sf)),{});
        parser.SetExpr(actual); parser.Eval();
        auto* host=static_cast<CxSevenClassSupervision*>(parser.GetClassObj("SevenClassSupervision","conversion"));
        check(host && host->status()=="CONVERTED","native CxScript result");
        auto expected=Convert(input,rules);
        check(host->result().mask==expected.mask,"CxScript matches native core");
        check(read(run/"conversion_receipt.json")==expected.receipt,"saved receipt equals actual result");
        for(auto pixel:expected.mask) check(pixel==255||pixel==unsigned(value=="arc"?0:value=="circle"?1:value=="ellipse"?2:value=="line"?3:value=="open_curve"?4:value=="polygon"?5:6),"class IDs unchanged");
        reject("SUPERVISION_OUTPUT_EXISTS",[&]{host->save(run.string().c_str());});
        host->load(fixture.string().c_str());
        check(host->result().mask.empty()&&host->status()=="NOT_RUN","reload invalidates stale result");
        reject("SUPERVISION_SUCCESS_REQUIRED",[&]{host->save((out/"must-not-exist").string().c_str());});
        host->run();
        reject("SUPERVISION_FILE_UNREADABLE",[&]{host->load((out/"missing.json").string().c_str());});
        check(host->result().mask.empty()&&host->status()=="NOT_RUN","failed reload invalidates result");
        host->run(); check(host->status()!="CONVERTED","failed reload cannot rerun old input");
        index.push_back({{"case_name","Seven Class Native "+name+" Conversion"},{"script",script_path.filename().string()},
            {"receipt",name+"-run/conversion_receipt.json"},{"synthetic",true},{"model_training_executed",false}});
    }
    write(out/"case_index.json",index.dump(2));
    CxSevenClassSupervision adapter;
    adapter.loadrules(rules_path.string().c_str());
    const auto duplicate=out/"duplicate-key.json";
    write(duplicate,"{\"split\":\"train\",\"split\":\"verify\"}");
    reject("SUPERVISION_DUPLICATE_JSON_KEY",[&]{adapter.load(duplicate.string().c_str());});
    auto bad=source("open_curve",{{5,5},{25,25}},true);
    const auto bad_path=out/"topology-mismatch.json"; write(bad_path,bad.dump());
    adapter.load(bad_path.string().c_str()); adapter.run();
    check(adapter.status()=="SUPERVISION_TOPOLOGY_MISMATCH" && adapter.result().mask.empty(),"failure has no stale mask");
    const auto invalid_rules=out/"unsupported-rules.json";
    auto changed_rules=rules; changed_rules["version"]="999"; write(invalid_rules,changed_rules.dump());
    reject("SUPERVISION_RULES_UNSUPPORTED",[&]{adapter.loadrules(invalid_rules.string().c_str());});
    adapter.run(); check(adapter.status()=="SUPERVISION_RULES_INVALID","failed rules load cannot use old rules");
}
}
int main(int argc,char** argv) {
    try {
        if(argc==3 && std::string(argv[1])=="--script") {
            std::ifstream file(argv[2]);
            if(!file) throw std::runtime_error("script unreadable");
            std::string script((std::istreambuf_iterator<char>(file)),{});
            mu::Parser parser; double* scalar=nullptr;
            parser.DefineOrgClass("double",scalar); parser.UsingClass(true);
            RegisterSevenClassSupervision(parser); parser.SetExpr(script); parser.Eval();
            std::cout<<"CxScript execution completed; inspect explicit status assertions/receipts.\n";
            return 0;
        }
        if(argc!=3) throw std::runtime_error("usage: native_test rules.json output_base");
        auto rules_path=fs::absolute(argv[1]); auto rules=read(rules_path);
        auto stamp=std::chrono::high_resolution_clock::now().time_since_epoch().count();
        fs::path out=fs::absolute(argv[2])/std::to_string(stamp);
        fs::create_directories(out.parent_path());
        if(!fs::create_directory(out)) throw std::runtime_error("unique evidence directory required");
        math_cases(rules); script_cases(rules_path,rules,out);
        J summary={{"status","PASS"},{"assertions",checks},{"backend","C++17 / mu::Parser"},
            {"python_executed",false},{"training_executed",false},{"dll_integration_verified",false},
            {"admission","development_trial"},{"rules_sha256",Sha256Bytes(rules.dump())}};
        write(out/"native_test_receipt.json",summary.dump(2));
        std::cout<<summary.dump()<<"\nEvidence: "<<out.string()<<'\n'; return 0;
    } catch(const mu::Parser::exception_type& e) {
        std::cerr<<"CxScript failure: "<<e.GetMsg()<<'\n';
    } catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<'\n';}
    return 1;
}
