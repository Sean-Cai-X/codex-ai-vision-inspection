#include "muParser.h"
#include "../../cximage/CxRefinedArcUiParameters.h"
#include <filesystem>
#include <fstream>
#include <iostream>
namespace fs=std::filesystem;using J=nlohmann::json;using namespace cxgeom::polar;
void need(bool v,const char* why){if(!v)throw std::runtime_error(why);}
void write(const fs::path& p,const std::string& s){std::ofstream f(p);f<<s;f.close();need(bool(f),"write");}
J read(const fs::path& p){std::ifstream f(p);J j;f>>j;return j;}
double curve(double x,bool repeated){
 if(repeated)return 45+2.5*std::sin((x-20)*std::acos(-1.)/10);
 return 45+.1*(x-40)+3*std::exp(-std::pow((x-28)/5,2))-4*std::exp(-std::pow((x-58)/6,2));
}
double slope(double x,bool repeated){return (curve(x+.0001,repeated)-curve(x-.0001,repeated))/.0002;}
cv::Mat render(std::complex<double> a,std::complex<double> t,bool repeated){
 cv::Mat result(128,128,CV_8UC1);
 for(int y=0;y<128;++y)for(int x=0;x<128;++x){
  auto p=(std::complex<double>(x,y)-t)/a;double m=slope(p.real(),repeated);
  double d=(p.imag()-curve(p.real(),repeated))*std::abs(a)/std::sqrt(1+m*m);
  result.at<unsigned char>(y,x)=cv::saturate_cast<unsigned char>(128+100*std::erf(d/1.6));
 }return result;
}
J execute(const J& s,cv::Mat* overlay=nullptr){
 auto script=cxpairui::Build(s);need(cxpairui::Read(script)==s,"recipe");
 mu::Parser parser;double* org=nullptr;parser.DefineOrgClass("double",org);parser.UsingClass(true);
 RegisterRefinedArcAudit(parser);parser.SetExpr(script);parser.Eval();
 auto* a=static_cast<CxRefinedArcAudit*>(parser.GetClassObj("RefinedArcAudit","a"));need(a!=nullptr,"actual parser");
 if(overlay)*overlay=a->sideoverlay(0);return J::parse(a->receipt());
}
int main(int argc,char** argv)try{
 need(argc==2||argc==3,"output [run-root|verify-headless]");const auto root=fs::absolute(argv[1]);
 if(argc==3&&std::string(argv[2])=="verify-headless"){
  for(const char* id:{"asymmetric_combined","repeated_boundary","uncertainty_rejected"}){
   need(read(root/id/"native.json")==read(root/"headless"/id/"refined_arc_receipt_0.json"),"actual app exact parity");
  }std::cout<<"IDENTITY_RASTER_HEADLESS_PARITY_PASS\n";return 0;
 }
 fs::create_directories(root);auto runroot=argc==3?fs::absolute(argv[2]):root;
#ifdef _WIN32
 _putenv_s("CXVISION_RUN_ROOT",runroot.string().c_str());
#else
 setenv("CXVISION_RUN_ROOT",runroot.string().c_str(),1);
#endif
 struct Case{const char* id;double angle,scale;bool repeated;};
 std::vector<Case> cases={{"asymmetric_identity",0,1,false},{"asymmetric_combined",37,1.3,false},
 {"asymmetric_negative",-43,.8,false},{"repeated_boundary",0,1,true}};
 J matrix={{"schema","independent_landmark_identity_raster.v1"},{"production_eligible",false},
 {"truth_usage","controlled landmark inputs; transform used only to rasterize independent images and score, never passed to identity search"},
 {"cases",J::array()}};
 J roots=J::array(),saved,repeated_saved;std::complex<double> savedshift;double savedangle=0,savedscale=1;
 auto emit=[&](const std::string& id,J s,double angle,double scale,std::complex<double> shift){
  const auto dir=root/id;fs::create_directories(dir);cv::Mat overlay;auto j=execute(s,&overlay);
  write(dir/"run.cxsc",cxpairui::Build(s));write(dir/"case_config.json",s.dump(2));write(dir/"native.json",j.dump(2));
  need(cv::imwrite((dir/"reference_overlay.png").string(),overlay),"overlay");
  J q={{"status",j["status"]},{"production_eligible",false}};
  if(!j["refined"]["candidates"].empty()){
   const auto& c=j["refined"]["candidates"][0];auto t=c["translation"];
   q["angle_error_deg"]=std::abs(std::remainder(c["angle_deg"].get<double>()-angle,360.));
   q["scale_absolute_error"]=std::abs(c["scale"].get<double>()-scale);
   q["translation_error_px"]=std::abs(std::complex<double>(t[0],t[1])-shift);
   q["accepted"]=c["accepted"];q["reason"]=c["reason"];
  }
  q["identity_diagnostics"]=j.value("identity_diagnostics",J::array());
  q["pose_uncertainty_diagnostics"]=j.value("pose_uncertainty_diagnostics",J::array());
  write(dir/"result_summary.json",q.dump(2));
  write(dir/"typed_label.json",J{{"schema","controlled_paired_boundary_label.v1"},{"geometry_type","open_curve"},
   {"annotation_status","PROPOSED"},{"human_reviewed",false},{"training_eligible",false},{"source_kind","independently_rasterized_asymmetric_or_repeated_boundary"}}.dump(2));
  write(dir/"case_manifest.json",J{{"schema","cxvision.evidence_case.v1"},{"run_id","landmark_identity_controls_v1"},
   {"internal_case_id",id},{"review_item","Landmark Identity - "+id},{"tool","RefinedArcAudit"},
   {"display_group","Harmonic / Landmark Identity"},{"display_category","To Verify"},{"case_role","controlled_independent_gray_pair"},
   {"geometry_type","open_curve"},{"binding_status","REFERENCE_ONLY"},{"parameter_summary","Held-out identity retrieval and conditional pose bounds; configurable development gates."},
   {"source_image","reference.pgm"},{"typed_label","typed_label.json"},{"geometry_facts_ref","native.json"},{"evidence_overlay","reference_overlay.png"},
   {"result_summary","result_summary.json"},{"script_snapshot","run.cxsc"},
   {"required_assets",{"reference.pgm","observation.pgm","run.cxsc","native.json","reference_overlay.png","result_summary.json","typed_label.json","case_config.json"}}}.dump(2));
  const bool accepted=!j["refined"]["accepted_indices"].empty();
  if(id.rfind("asymmetric_",0)==0){
   need(accepted,"independent distinctive boundary must pass identity");
   need(q["angle_error_deg"].get<double>()<=1&&q["scale_absolute_error"].get<double>()<=.05&&q["translation_error_px"].get<double>()<=.5,"independent pose quality");
   need(j["identity_diagnostics"][0]["geometric_identity_supported_within_search"],"bidirectional identity");
  }
  if(id=="repeated_boundary")need(!accepted&&q["reason"]=="LANDMARK_IDENTITY_NOT_CONFIRMED","repetition rejected");
  if(id=="uncertainty_rejected")need(!accepted&&q["reason"]=="LANDMARK_UNCERTAINTY_NOT_ACCEPTABLE","default global translation bound rejects");
  if(id=="conditional_one_pixel_budget")need(accepted&&j["pose_uncertainty_diagnostics"][0]["conditional_gate_pass"].get<bool>(),"explicit one-pixel policy");
  if(id=="identity_gate_disabled")need(accepted&&!j["identity_diagnostics"][0]["geometric_identity_supported_within_search"].get<bool>(),"disabled gate visible");
  if(id=="shifted_identity"){
   need(!accepted,"shifted identity rejected");
   for(const auto& query:j["identity_diagnostics"][0]["queries"])need(query["status"]=="IDENTITY_CLAIM_DISPLACED","wrong identity detected independently of fit");
  }
  if(id=="context_mismatch"||id=="search_budget_rejected")need(!accepted,"negative context/budget gate");
  need(j["production_eligible"]==false,"no automatic approval");
  roots.push_back(id);matrix["cases"].push_back({{"id",id},{"result",q}});
  std::cout<<id<<" "<<q["status"]<<" ";
  if(q.contains("reason"))std::cout<<q["reason"]<<" ";
  if(!q["identity_diagnostics"].empty())for(const auto& r:q["identity_diagnostics"][0]["queries"])
   std::cout<<r["status"]<<":"<<r["best_score_px"]<<" ";
  std::cout<<"\n";return j;
 };
 for(const auto& c:cases){
  auto dir=root/c.id;fs::create_directories(dir);auto a=std::polar(c.scale,c.angle*std::acos(-1.)/180.);
  auto t=std::complex<double>(67,64)-a*std::complex<double>(44,45);
  auto point=[&](double x){return std::complex<double>(x,curve(x,c.repeated));};
  auto s=cxpairui::Default();s["correspondence_id"]=std::string("controlled:identity-context:")+c.id;
  s["parameters"]["matching_landmark_arc_enabled"]=1;s["parameters"]["identity_required"]=1;
  s["parameters"]["identity_radius_px"]=8;
  for(int side:{0,1}){
   auto transform=[&](std::complex<double> p){return side?a*p+t:p;};
   auto image=render(side?a:std::complex<double>(1,0),side?t:std::complex<double>(0,0),c.repeated);
   const auto name=side?"observation.pgm":"reference.pgm";need(cv::imwrite((dir/name).string(),image),"image");
   auto& v=s["sides"][side];v["image"]=fs::relative(dir/name,runroot).generic_string();
   v["case_id"]=std::string(c.id)+(side?":observation":":reference");
   v["gray"]["normal_maximum_turn_deg"]=60;
   v["gray"]["anchor_x"]=0;v["gray"]["anchor_y"]=0;v["gray"]["anchor_width"]=128;v["gray"]["anchor_height"]=128;
   auto start=transform(point(16)),end=transform(point(72)),via=transform(point(44));
   v["endpoints"]={{"start_x",start.real()},{"start_y",start.imag()},{"end_x",end.real()},{"end_y",end.imag()},
    {"via_x",via.real()},{"via_y",via.imag()},{"maximum_snap_px",2.},{"tie_epsilon_px",1e-8}};
   v["endpoint_provenance"]="controlled:ROI-not-matching-identity";
   start=transform(point(33));end=transform(point(53));
   v["matching"]={{"start_x",start.real()},{"start_y",start.imag()},{"end_x",end.real()},{"end_y",end.imag()},
    {"maximum_distance_px",.25},{"tie_epsilon_px",1e-8},{"provenance","controlled:matching-x33-x53"}};
   v["landmark"]={{"source","CONTROLLED_REFERENCE"},{"trace",std::string("raster:")+c.id},
    {"image_sha256",cxpairui::ImageSha(v)},{"localization_error_px",.05}};
  }
  const auto j=emit(c.id,s,c.angle,c.scale,t);
  if(std::string(c.id)=="repeated_boundary")repeated_saved=s;
  if(std::string(c.id)=="asymmetric_combined"){saved=s;savedangle=c.angle;savedscale=c.scale;savedshift=t;}
 }
 for(const char* id:{"uncertainty_rejected","conditional_one_pixel_budget","identity_gate_disabled","shifted_identity","context_mismatch","search_budget_rejected"}){
  const bool repeated=std::string(id)=="identity_gate_disabled";
  auto s=repeated?repeated_saved:saved;auto dir=root/id;fs::create_directories(dir);
  for(int side:{0,1}){
   auto name=side?"observation.pgm":"reference.pgm";
   fs::copy_file(root/(repeated?"repeated_boundary":"asymmetric_combined")/name,dir/name,fs::copy_options::overwrite_existing);
   s["sides"][side]["image"]=fs::relative(dir/name,runroot).generic_string();
  }
  if(std::string(id)=="uncertainty_rejected"){
   s["parameters"]["uncertainty_required"]=1;
  }else if(std::string(id)=="conditional_one_pixel_budget"){
   s["parameters"]["uncertainty_required"]=1;s["parameters"]["uncertainty_maximum_translation_px"]=1.;
  }else if(std::string(id)=="context_mismatch")s["parameters"]["identity_maximum_context_error_px"]=.005;
  else if(std::string(id)=="search_budget_rejected")s["parameters"]["identity_maximum_hypotheses"]=2;
  else if(std::string(id)=="identity_gate_disabled")s["parameters"]["identity_required"]=0;
  else{
   auto factor=std::polar(savedscale,savedangle*std::acos(-1.)/180.);
   auto point=[&](double x){return factor*std::complex<double>(x,curve(x,false))+savedshift;};
   for(const auto& item:std::vector<std::pair<const char*,double>>{{"start",33.75},{"end",53.75}}){
    auto p=point(item.second);s["sides"][1]["matching"][std::string(item.first)+"_x"]=p.real();
    s["sides"][1]["matching"][std::string(item.first)+"_y"]=p.imag();
   }
  }
  emit(id,s,repeated?0:savedangle,repeated?1:savedscale,repeated?std::complex<double>(23,19):savedshift);
 }
 write(root/"matrix.json",matrix.dump(2));fs::create_directories(root/"_shared");
 write(root/"_shared/evidence_case_roots.json",J{{"schema","cxvision.evidence_case_roots.v1"},{"roots",roots}}.dump(2));
 std::cout<<"IDENTITY_RASTER_GUARDS_PASS not production\n";return 0;
}catch(const mu::Parser::exception_type& e){std::cerr<<e.GetMsg()<<"\n";return 1;}
catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
