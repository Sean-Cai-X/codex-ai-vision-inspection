#include "../../cximage/CxSetMatchEvidenceParameters.h"
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

void evidence(const std::filesystem::path& out,const J& request,const std::string& id,const std::string& title,int pairBudget=1000000) {
    const auto dir=out/id;std::filesystem::create_directory(dir);
    write(dir/"request.json",request.dump(2));
    std::string script="// setmatch_evidence_binding: 1\n// setmatch_default global_setmatch_elapsed 3000\n";
    script+="// setmatch_default global_setmatch_pair_checks "+std::to_string(pairBudget)+"\n";
    script+="FindSetMatch m_set;\nm_set.load(global_setmatch_request_path);\n";
    script+=cxsetmatchui::ParameterScript("m_set");
    script+="m_set.run();\nm_set.save(global_setmatch_receipt_path);\n";
    write(dir/"run.cxsc",script);
    // Diagram only: reference gray, target black; never connects unordered points.
    constexpr int size=400;std::vector<int> pixels(size*size,255);
    auto dot=[&](double x,double y,int color) {
        int u=int(std::lround((x+30)*5)),v=int(std::lround((y+30)*5));
        for(int a=-2;a<=2;++a)for(int b=-2;b<=2;++b)
          if(u+a>=0&&u+a<size&&v+b>=0&&v+b<size)pixels[(v+b)*size+u+a]=color;
    };
    for(const char* side:{"reference","target"})for(const auto& e:request.at(side).at("elements")) {
        const auto& g=e.at("geometry");const auto type=g.at("type").get<std::string>();
        const int color=std::string(side)=="reference"?140:0;
        if(type=="POINT")dot(g.at("point").at("x"),g.at("point").at("y"),color);
        else if(type=="LINE_SEGMENT")for(int i=0;i<=200;++i) {
            const double t=i/200.;
            dot(g.at("start").at("x").get<double>()*(1-t)+g.at("end").at("x").get<double>()*t,
                g.at("start").at("y").get<double>()*(1-t)+g.at("end").at("y").get<double>()*t,color);
        } else if(type=="ARC_SEGMENT")for(int i=0;i<=200;++i) {
            const double a=(g.at("start_deg").get<double>()+g.at("sweep_deg").get<double>()*i/200.)*.017453292519943295;
            dot(g.at("center").at("x").get<double>()+g.at("radius").get<double>()*std::cos(a),
                g.at("center").at("y").get<double>()+g.at("radius").get<double>()*std::sin(a),color);
        }
    }
    std::ostringstream image;image<<"P2\n400 400\n255\n";for(int p:pixels)image<<p<<" ";
    write(dir/"source_image.pgm",image.str());
    write(dir/"typed_label.json",J{{"schema","cxvision.geometric_set_fixture_label.v1"},
        {"status","proposed"},{"human_accepted",false},{"training_eligible",false},
        {"source_ref","synthetic:geometric_set"},{"request_ref","request.json"}}.dump(2));
    FindSetMatch preview;preview.requestjson(request.dump().c_str());
    preview.parameter(pairBudget,"max_pair_checks");preview.run();
    write(dir/"fixture_receipt.json",preview.receipt());
    write(dir/"result_summary.json",J{{"schema","cxvision.geometric_set_fixture_summary.v1"},
      {"status",cxgeom::gsm::Name(preview.result().execution_status)},
      {"candidate_count",preview.result().candidates.size()},
      {"image_extraction_performed",false},{"production_eligible",false},
      {"overlay_semantics","input_geometry_diagram_not_inference"}}.dump(2));
    write(dir/"case_manifest.json",J{{"schema","cxvision.evidence_case.v1"},
        {"run_id","geometric_set_ui_v1"},{"internal_case_id",id},{"review_item",title},
        {"tool","FindSetMatch"},{"display_group","FindSetMatch / Development Full Set"},
        {"display_category","To Verify"},{"case_role","development_fixture"},
        {"geometry_type","geometric_set"},{"binding_status","REFERENCE_ONLY"},
        {"parameter_summary","Structured geometry only; diagram is not image extraction; production=false"},
        {"source_image","source_image.pgm"},{"typed_label","typed_label.json"},{"script_snapshot","run.cxsc"},
        {"geometry_facts_ref","fixture_receipt.json"},{"result_summary","result_summary.json"},
        {"evidence_overlay","source_image.pgm"},
        {"required_assets",J::array({"source_image.pgm","typed_label.json","request.json","run.cxsc","fixture_receipt.json","result_summary.json"})}}.dump(2));
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

    std::unordered_map<std::string,int> values;std::string reason;
    need(cxsetmatchui::Defaults("// setmatch_evidence_binding: 1",values,reason),"UI defaults valid");
    need(values.size()==12,"all twelve active numeric controls");
    values["global_setmatch_scale_min"]=1300;
    need(!cxsetmatchui::Validate(values,reason),"inverted scale rejected");
    need(!cxsetmatchui::Defaults("// setmatch_default unknown 1",values,reason),"unknown UI default rejected");
    need(!cxsetmatchui::Defaults("// setmatch_default global_setmatch_elapsed 0",values,reason),"zero time rejected");
    need(!cxsetmatchui::Defaults("// setmatch_default global_setmatch_elapsed 1 trailing",values,reason),"trailing UI default rejected");
    need(cxsetmatchui::Defaults("// setmatch_default global_setmatch_elapsed 3000",values,reason),"case override accepted");
    const auto cases=out/"evidence";std::filesystem::create_directory(cases);
    evidence(cases,request,"setmatch_asymmetric_mixed","Set Match - Asymmetric Mixed / development");
    auto square=request;square["request_id"]="GSM1_SQUARE";
    for(const char* side:{"reference","target"}) {
      square[side]["elements"]=J::array();int i=0;
      for(const auto xy: {std::pair<double,double>{0,0},{10,0},{10,10},{0,10}}) {
        std::string id=std::string(side)+std::to_string(i++);
        square[side]["elements"].push_back({{"stable_id",id},{"source_ref","synthetic:"+id},{"quality",1},
          {"geometry",{{"type","POINT"},{"point",point(xy.first,xy.second)}}}});
      }
    }
    evidence(cases,square,"setmatch_symmetric_square","Set Match - Symmetric Multiple Poses / development");
    evidence(cases,request,"setmatch_budget_stop","Set Match - Budget Stop / development",1);

    std::cout<<J{{"status","PASS"},{"checks",checks},{"production_eligible",false}}.dump()<<"\n";return 0;
} catch(const std::exception& e){std::cerr<<"SETMATCH_ADAPTER_FAIL "<<e.what()<<"\n";return 1;}
