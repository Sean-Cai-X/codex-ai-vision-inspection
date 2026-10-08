#include "../../cximage/FindSetMatch.h"
#include "../../cxgeom/include/CxGeoSO2ReferenceAsset.h"
#include "../../libtorchsegmentation/src/utils/json.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <functional>
#include <stdexcept>
using J=nlohmann::json;
int checks=0;
void need(bool b,const char* s){++checks;if(!b)throw std::runtime_error(s);}
void rejects(const std::function<void()>& f,const char* s){bool caught=false;try{f();}catch(const std::exception&){caught=true;}need(caught,s);}
J point(double x,double y){return {{"x",x},{"y",y}};}
J fixture(const char* schema) {
    std::ifstream f(schema);auto contract=J::parse(f);J p=J::object();
    for(auto it=contract["$defs"]["parameters"]["properties"].begin();it!=contract["$defs"]["parameters"]["properties"].end();++it)
        if(it.value().contains("default"))p[it.key()]=it.value()["default"];
    J roi={{"x",-100},{"y",-100},{"width",200},{"height",200}};
    p["search_roi"]=roi;p["max_elapsed_ms"]=3000;
    J ref={{"set_id","reference"},{"source_ref","synthetic:reference"},{"roi",roi},{"elements",J::array()}};
    J target=ref;target["set_id"]="target";target["source_ref"]="synthetic:target";
    auto add=[&](const char* id,J a,J b){
        ref["elements"].push_back({{"stable_id",std::string("r")+id},{"source_ref",std::string("synthetic:r")+id},{"quality",1},{"geometry",a}});
        target["elements"].push_back({{"stable_id",std::string("t")+id},{"source_ref",std::string("synthetic:t")+id},{"quality",1},{"geometry",b}});
    };
    add("0",{{"type","POINT"},{"point",point(1,1)}},{{"type","POINT"},{"point",point(9,-4)}});
    add("1",{{"type","LINE_SEGMENT"},{"start",point(5,3)},{"end",point(7,4)}},
            {{"type","LINE_SEGMENT"},{"start",point(6,2)},{"end",point(7,0)}});
    add("2",{{"type","ARC_SEGMENT"},{"center",point(10,10)},{"radius",2},{"start_deg",0},{"sweep_deg",90}},
            {{"type","ARC_SEGMENT"},{"center",point(0,5)},{"radius",2},{"start_deg",180},{"sweep_deg",-90}});
    add("3",{{"type","POINT"},{"point",point(20,2)}},{{"type","POINT"},{"point",point(8,15)}});
    std::reverse(target["elements"].begin(),target["elements"].end());
    return {{"schema","cxvision.geometric_set_match.request.v1"},{"request_id","GSM1_ADAPTER"},
        {"reference",ref},{"target",target},{"parameters",p}};
}
void write(const std::filesystem::path& p,const std::string& s) {
    std::ofstream f(p);f<<s;f.close();if(!f)throw std::runtime_error("fixture write failed");
}
int main(int argc,char** argv) try {
    if(argc!=3)throw std::runtime_error("schema and fresh external output required");
    const std::filesystem::path out(argv[2]);
    if(std::filesystem::exists(out))throw std::runtime_error("fresh output required");
    std::filesystem::create_directories(out);
    const auto request=fixture(argv[1]);const auto raw=request.dump();
    FindSetMatch m;m.requestjson(raw.c_str());m.run();m.expectstatus("COMPLETED");m.expectcount(1);
    auto receipt=J::parse(m.receipt());const auto& pose=receipt["result"]["candidates"][0]["pose"];
    need(std::abs(pose["angle_deg"].get<double>()-90)<1e-8,"decoded mixed pose angle");
    need(std::abs(pose["translation"]["x"].get<double>()-10)<1e-8,"decoded translation");
    need(receipt["input_bytes_sha256"]==cxgeom::so2::ReferenceSha256(raw),"input SHA");
    need(receipt["request"]==request,"effective request snapshot");
    need(receipt["production_eligible"]==false&&!receipt["image_extraction_performed"].get<bool>(),"no production/image claim");
    need(receipt["result"]["solvability"].is_null(),"no solvability claim");
    const auto path=(out/"native_receipt.json").string();m.save(path.c_str());
    rejects([&]{m.save(path.c_str());},"receipt never overwritten");
    m.parameter(.01,"max_residual_px");rejects([&]{m.receipt();},"edit invalidates result");
    m.run();receipt=J::parse(m.receipt());
    need(receipt["request"]["parameters"]["max_residual_px"]==.01,"fractional parameter survives integer JSON spelling");
    need(receipt["input_bytes_sha256"]!=receipt["effective_request_sha256"],"changed effective hash");
    m.parameter(1,"max_pair_checks");m.run();m.expectstatus("BUDGET_EXHAUSTED");m.expectcount(0);
    rejects([&]{m.parameter(1.5,"max_pair_checks");},"integer parameter checked");
    rejects([&]{m.receipt();},"failed edit invalidates result");
    rejects([&]{m.parameter(2,"seed");},"reserved parameter rejected");
    auto bad=request;bad["business_image"]="bytes";
    rejects([&]{m.requestjson(bad.dump().c_str());},"unknown field rejected");
    rejects([&]{m.run();},"failed load clears previous request");
    bad=request;bad["parameters"]["max_hypotheses"]=true;
    rejects([&]{m.requestjson(bad.dump().c_str());},"bool not count");
    bad=request;bad["parameters"]["max_hypotheses"]=1.5;
    rejects([&]{m.requestjson(bad.dump().c_str());},"fraction not count");
    bad=request;bad["parameters"]["max_elapsed_ms"]=4294967297ULL;
    rejects([&]{m.requestjson(bad.dump().c_str());},"integer overflow blocked");
    bad=request;bad["reference"]["elements"][0]["geometry"]["type"]="RECTANGLE";
    rejects([&]{m.requestjson(bad.dump().c_str());},"no rectangle fallback");
    std::string duplicate=raw;duplicate.insert(1,"\"schema\":\"other\",");
    rejects([&]{m.requestjson(duplicate.c_str());},"duplicate key rejected");
    std::string huge(4*1024*1024+1,' ');
    rejects([&]{m.requestjson(huge.c_str());},"byte cap");
    std::string deep(40,'[');deep+=std::string(40,']');
    rejects([&]{m.requestjson(deep.c_str());},"depth cap");
    m.requestjson(raw.c_str());m.run();
    rejects([&]{m.load((out/"missing.json").string().c_str());},"missing file");
    rejects([&]{m.receipt();},"missing file cannot export stale receipt");
    write(out/"request.json",raw);
    m.load((out/"request.json").string().c_str());m.run();m.expectcount(1);
    need(J::parse(m.receipt())["result"]["candidates"][0]["correspondences"].size()==4,"IDs returned");
    write(out/"nul.json",raw+std::string(1,'\0'));
    rejects([&]{m.load((out/"nul.json").string().c_str());},"embedded NUL rejected");
    const std::string load="FindSetMatch m_set;\nm_set.load(\""+(out/"request.json").string()+"\");\n";
    write(out/"success.cxsc",load+"m_set.run();\nm_set.expectstatus(\"COMPLETED\");\nm_set.expectcount(1);\nm_set.save(\""+(out/"script_receipt.json").string()+"\");\n");
    write(out/"budget.cxsc",load+"m_set.parameter(1,\"max_pair_checks\");\nm_set.run();\nm_set.expectstatus(\"BUDGET_EXHAUSTED\");\nm_set.expectcount(0);\nm_set.save(\""+(out/"budget_receipt.json").string()+"\");\n");
    write(out/"stale.cxsc",load+"m_set.run();\nm_set.parameter(0.1,\"max_residual_px\");\nm_set.save(\""+(out/"stale_receipt.json").string()+"\");\n");
    write(out/"unknown.cxsc",load+"m_set.parameter(1,\"not_a_parameter\");\n");
    write(out/"overwrite.cxsc",load+"m_set.run();\nm_set.save(\""+(out/"script_receipt.json").string()+"\");\n");
    write(out/"auto.cxsc",load+"m_set.run();\nm_set.expectcount(1);\n");
    write(out/"mixed.cxsc","FindObject m_other;\n"+load+"m_set.run();\n");
    std::cout<<J{{"status","PASS"},{"checks",checks},{"production_eligible",false}}.dump()<<"\n";return 0;
} catch(const std::exception& e){std::cerr<<"SETMATCH_ADAPTER_FAIL "<<e.what()<<"\n";return 1;}
