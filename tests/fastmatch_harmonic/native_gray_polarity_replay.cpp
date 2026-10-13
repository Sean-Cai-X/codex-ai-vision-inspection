#include "../../cximage/CxGrayBoundaryAudit.h"
#include "../../cximage/CxGrayBoundaryUiParameters.h"
#include <iostream>
using J=nlohmann::json;namespace fs=std::filesystem;
void need(bool b,const char*s){if(!b)throw std::runtime_error(s);}
J read(fs::path p){std::ifstream f(p);J j;f>>j;return j;}
void write(fs::path p,std::string s){std::ofstream f(p);f<<s;need(bool(f),"write");}
int main(int argc,char**argv)try{
 need(argc>=3,"source / fresh output [verify]");fs::path source=fs::absolute(argv[1]),out=fs::absolute(argv[2]);
 if(argc==4){
  int count=0;for(auto row:read(out/"matrix.json")){auto dir=out/row["case_id"].get<std::string>();
   need(read(dir/"native.json")==read(dir/"headless/gray_boundary_receipt_0.json"),"actual headless differs");
   auto bytes=[](const fs::path& p){std::ifstream f(p,std::ios::binary);need(bool(f),"source readable");return std::string((std::istreambuf_iterator<char>(f)),{});};
   std::string sourceCase=dir.filename().string();sourceCase=sourceCase.substr(0,sourceCase.rfind("_r"));
   need(bytes(dir/"source.pgm")==bytes(source/sourceCase/"source.pgm"),"source bytes changed");
   if(fs::exists(dir/"case_manifest.json")){
    auto manifest=read(dir/"case_manifest.json");std::set<std::string> assets;
    for(auto item:manifest["required_assets"]){fs::path p=item.get<std::string>();need(!p.is_absolute(),"absolute asset outside case");
     auto relative=fs::weakly_canonical(dir/p).lexically_relative(fs::weakly_canonical(dir));
     need(!relative.empty()&&*relative.begin()!=".."&&!fs::is_symlink(dir/p)&&fs::is_regular_file(dir/p),"unsafe case asset");assets.insert(p.generic_string());
    }
    for(const char* key:{"source_image","typed_label","geometry_facts_ref","evidence_overlay","result_summary","script_snapshot"})need(assets.count(manifest[key].get<std::string>())==1,"required asset missing");
   }++count;
  }std::cout<<"GRAY_POLARITY_HEADLESS_PARITY_PASS "<<count<<"\n";return 0;
 }
 need(!fs::exists(out),"fresh output");fs::create_directories(out);J matrix=J::array();
 for(auto row:read(source/"matrix.json")){
  std::string id=row["case"];int iterations=row["recenter_iterations"];std::string name=id+"_r"+std::to_string(iterations);auto dir=out/name;fs::create_directories(dir);
  auto image=dir/"source.pgm";need(fs::copy_file(source/id/"source.pgm",image),"copy immutable source");auto old=read(source/id/("recenter"+std::to_string(iterations)+".json"))["chunks"][0];
  auto points=old["points"];J params=J::object();for(auto p:cxgrayparams::Parameters())params[p.key]=p.initial;
  params["foreground_polarity"]=1;params["recenter_iterations"]=iterations;params["anchor_x"]=0;params["anchor_y"]=0;params["anchor_width"]=400;params["anchor_height"]=400;
  CxGrayBoundaryAudit g;g.caseid(name.c_str());g.image(image.c_str());
  std::ostringstream script;script<<"GrayBoundaryAudit g;\ng.caseid(\""<<name<<"\");\ng.image(\""<<image.string()<<"\");\n";
  for(auto p=params.begin();p!=params.end();++p){g.parameter(p.value(),p.key().c_str());script<<"g.parameter("<<p.value()<<",\""<<p.key()<<"\");\n";}
  J anchors={{"start",points[0]["raw"]},{"end",points.back()["raw"]},{"via",points[points.size()/2]["raw"]}};
  for(auto p:anchors.items())for(int axis:{0,1}){
   std::string key=p.key()+(axis?"_y":"_x");double value=p.value()[axis];g.endpointparameter(value,key.c_str());script<<"g.endpointparameter("<<value<<",\""<<key<<"\");\n";
  }
  g.endpointparameter(1e-6,"maximum_snap_px");g.endpointprovenance("frozen_observed_chunk0_not_truth_landmarks");
  script<<"g.endpointparameter(1e-6,\"maximum_snap_px\");\ng.endpointprovenance(\"frozen_observed_chunk0_not_truth_landmarks\");\ng.run();\n";
  need(cxgrayui::Values(script.str())["foreground_polarity"]==1,"UI polarity lost");
  g.run();auto actual=J::parse(g.receipt());need(actual["binding"]["threshold_mode"]=="BINARY_LESS_OR_EQUAL","mode receipt");
  need(actual["binding"]["gray_sha256"]==old["gray_sha256"],"same image gray SHA");
  auto refined=actual["refinement"]["points"];need(refined.size()==points.size(),"same support count");
  for(size_t i=0;i<points.size();++i){auto a=points[i],b=refined[i];a.erase("source_id");b.erase("source_id");need(a==b,"raw normal profile refinement parity");}
  auto mode=actual["parameters"];need(mode["foreground_polarity"]==1,"executed polarity");
  bool stale=false;g.parameter(0,"foreground_polarity");try{g.receipt();}catch(const std::runtime_error&){stale=true;}need(stale,"polarity edit stale receipt");
  g.parameter(1,"foreground_polarity");
  for(auto p:anchors.items())for(int axis:{0,1})g.endpointparameter(p.value()[axis],(p.key()+(axis?"_y":"_x")).c_str());
  g.endpointparameter(1e-6,"maximum_snap_px");g.endpointprovenance("frozen_observed_chunk0_not_truth_landmarks");g.run();
  need(actual==J::parse(g.receipt()),"rerun deterministic");
  write(dir/"run.cxsc",script.str());write(dir/"native.json",actual.dump(2));need(cv::imwrite((dir/"overlay.png").string(),g.overlay()),"overlay");
  auto summary=J({{"case_id",name},{"status",actual["status"]},{"raw_points",actual["raw_point_count"]},{"accepted",actual["accepted_point_count"]},{"rejected",actual["rejected_point_count"]},
    {"selection","chunk0_from_frozen_observed_contour; runtime reextracts from original image; no pose landmarks"},{"production_eligible",false}});
  write(dir/"result_summary.json",summary.dump(2));matrix.push_back(summary);
  if(id=="case_0"||id=="case_24"){
   write(dir/"typed_label.json",J({{"annotation_status","PROPOSED"},{"human_reviewed",false},{"training_eligible",false},{"geometry_type","open_curve"}}).dump(2));
   write(dir/"case_manifest.json",J({{"schema","cxvision.evidence_case.v1"},{"run_id","gray_polarity_20261013"},{"internal_case_id",name},{"review_item","Gray Polarity - "+id+" recenter"+std::to_string(iterations)+" - dark boundary"},
    {"tool","GrayBoundaryAudit"},{"display_group","Harmonic / Bound Gray Audit"},{"display_category","To Verify"},{"case_role","controlled_gray_boundary_audit"},{"geometry_type","open_curve"},{"binding_status","REFERENCE_ONLY"},
    {"parameter_summary","foreground_polarity=1 dark <=128; original gray retained; explicit observed chunk0 selection, not whole closed pose."},
    {"source_image","source.pgm"},{"typed_label","typed_label.json"},{"geometry_facts_ref","native.json"},{"evidence_overlay","overlay.png"},{"result_summary","result_summary.json"},{"script_snapshot","run.cxsc"},
    {"required_assets",{"source.pgm","typed_label.json","native.json","overlay.png","result_summary.json","run.cxsc"}}}).dump(2));
  }
 }
 write(out/"matrix.json",matrix.dump(2));std::cout<<"GRAY_POLARITY_NATIVE_PASS "<<matrix.size()<<" identical selected-point profiles; raw unmodified; no pose\n";
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}
