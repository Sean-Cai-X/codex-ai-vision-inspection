#include "muParser.h"
#include "../../cximage/CxRefinedArcUiParameters.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
namespace fs=std::filesystem;using J=nlohmann::json;using Z=std::complex<double>;
void need(bool v,const char* why){if(!v)throw std::runtime_error(why);}
J read(const fs::path& p){std::ifstream f(p);J j;f>>j;need(bool(f),"read");return j;}
void write(const fs::path& p,const std::string& s){std::ofstream f(p);f<<s;f.close();need(bool(f),"write");}
double curve(double x){return 45+.1*(x-40)+3*std::exp(-std::pow((x-28)/5,2))-4*std::exp(-std::pow((x-58)/6,2));}
double slope(double x){return (curve(x+.0001)-curve(x-.0001))/.0002;}
struct Case{const char* id;double width=1.6,amplitude=100,noise=0;int defect=0;double angle=37,scale=1.3;};
cv::Mat render(const Case& c,Z factor,Z shift){
 cv::Mat image(128,128,CV_8UC1);std::mt19937 rng(1031);std::uniform_real_distribution<double> noise(-c.noise,c.noise);
 for(int y=0;y<128;++y)for(int x=0;x<128;++x){
  auto q=(Z(x,y)-shift)/factor;double m=slope(q.real()),d=(q.imag()-curve(q.real()))*std::abs(factor)/std::sqrt(1+m*m);
  double amplitude=c.amplitude;if(c.defect==2&&q.real()>40&&q.real()<46)amplitude=4;
  double value=128+amplitude*std::erf(d/c.width)+noise(rng);
  if(c.defect==1&&q.real()>40&&q.real()<46)value=128;
  image.at<unsigned char>(y,x)=cv::saturate_cast<unsigned char>(value);
 }return image;
}
J execute(const J& s,cv::Mat* overlay=nullptr){
 const auto script=cxpairui::Build(s);need(cxpairui::Read(script)==s,"canonical");
 mu::Parser parser;double* org=nullptr;parser.DefineOrgClass("double",org);parser.UsingClass(true);RegisterRefinedArcAudit(parser);
 parser.SetExpr(script);parser.Eval();auto* a=static_cast<CxRefinedArcAudit*>(parser.GetClassObj("RefinedArcAudit","a"));
 need(a!=nullptr,"actual parser");if(overlay)*overlay=a->sideoverlay(0);return J::parse(a->receipt());
}
int main(int argc,char** argv)try{
 need(argc==3||argc==4,"clean fixture root / output [run-root|verify-headless]");
 const fs::path source=fs::absolute(argv[1]),root=fs::absolute(argv[2]);
 const std::vector<Case> cases={{"clean_control"},{"blur_3p2",3.2},{"blur_8",8},{"low_contrast",1.6,8},
  {"missing_boundary",1.6,0},{"boundary_gap",1.6,100,0,1},{"local_contrast_dropout",1.6,100,0,2},{"noise_4",1.6,100,4},
  {"sharpen_0p8",.8},{"roi_snap_ambiguous",1.6,100,0,0,-45,.8}};
 if(argc==4&&std::string(argv[3])=="verify-headless"){
  for(const auto& c:cases)need(read(root/c.id/"native.json")==read(root/"headless"/c.id/"refined_arc_receipt_0.json"),"stress native/app exact parity");
  std::cout<<"LANDMARK_STRESS_HEADLESS_PARITY_PASS\n";return 0;
 }
 fs::create_directories(root);const auto runroot=argc==4?fs::absolute(argv[3]):root;
#ifdef _WIN32
 _putenv_s("CXVISION_RUN_ROOT",runroot.string().c_str());
#else
 setenv("CXVISION_RUN_ROOT",runroot.string().c_str(),1);
#endif
 J matrix={{"schema","landmark_boundary_stress_matrix.v1"},{"production_eligible",false},
 {"error_calibration_completed",false},{"test_type","controlled_simulated_observation_stress_not_field_calibration"},
 {"cases",J::array()}},roots=J::array();
 const auto baseline=read(source/"asymmetric_combined/case_config.json");
 for(const auto& c:cases){
  const std::string id=c.id;auto dir=root/id;fs::create_directories(dir);auto s=baseline;
  s["parameters"]["input_rejection_receipt_enabled"]=1;
  s["correspondence_id"]="controlled:stress:"+id;
  const auto factor=std::polar(c.scale,c.angle*std::acos(-1.)/180.);
  const auto shift=Z(67,64)-factor*Z(44,45);
  fs::copy_file(source/"asymmetric_combined/reference.pgm",dir/"reference.pgm",fs::copy_options::overwrite_existing);
  auto image=render(c,factor,shift);need(cv::imwrite((dir/"observation.pgm").string(),image),"stress image");
  for(int side:{0,1}){
   auto& v=s["sides"][side];const char* name=side?"observation.pgm":"reference.pgm";
   v["image"]=fs::relative(dir/name,runroot).generic_string();v["case_id"]=id+(side?":observation":":reference");
   v["landmark"]["image_sha256"]=cxpairui::ImageSha(v);v["landmark"]["trace"]="fixture:stress:"+id;
  }
  auto point=[&](double x){return factor*Z(x,curve(x))+shift;};
  for(const auto& item:std::vector<std::pair<std::string,double>>{{"start",16},{"end",72},{"via",44}}){
   auto p=point(item.second);s["sides"][1]["endpoints"][item.first+"_x"]=p.real();s["sides"][1]["endpoints"][item.first+"_y"]=p.imag();
  }
  for(const auto& item:std::vector<std::pair<std::string,double>>{{"start",33},{"end",53}}){
   auto p=point(item.second);s["sides"][1]["matching"][item.first+"_x"]=p.real();s["sides"][1]["matching"][item.first+"_y"]=p.imag();
  }
  write(dir/"run.cxsc",cxpairui::Build(s));write(dir/"case_config.json",s.dump(2));
  cv::Mat overlay;auto j=execute(s,&overlay);write(dir/"native.json",j.dump(2));
  need(cv::imwrite((dir/"reference_overlay.png").string(),overlay),"overlay");
  J recipe={{"schema","boundary_stress_generation.v1"},{"erf_width_px",c.width},{"intensity_amplitude_dn",c.amplitude},
   {"noise_uniform_half_range_dn",c.noise},{"rng","mt19937_uniform_real"},{"seed",1031},{"defect_mode",c.defect},
   {"defect_source_x_interval",{40,46}},{"true_angle_deg",c.angle},{"true_scale",c.scale},
   {"true_translation",{shift.real(),shift.imag()}},{"pose_truth_used_only_for_generation_and_scoring",true}};
  write(dir/"fixture_parameters.json",recipe.dump(2));
  J q={{"status",j["status"]},{"production_eligible",false},{"input_rejected",j.value("input_rejected",false)},
   {"pose_evaluated",!j.value("input_rejected",false)&&!j["refined"]["candidates"].empty()},
   {"accepted",!j["refined"]["accepted_indices"].empty()},{"angle_error_deg",nullptr},{"scale_error_absolute",nullptr},{"translation_error_px",nullptr}};
  if(!j["refined"]["candidates"].empty()){
   const auto& candidate=j["refined"]["candidates"][0];auto t=candidate["translation"];
   q["reason"]=candidate["reason"];
   q["angle_error_deg"]=std::abs(std::remainder(candidate["angle_deg"].get<double>()-c.angle,360.));
   q["scale_error_absolute"]=std::abs(candidate["scale"].get<double>()-c.scale);
   q["translation_error_px"]=std::abs(Z(t[0],t[1])-shift);
   q["identity_supported"]=j["identity_diagnostics"][0]["geometric_identity_supported_within_search"];
  }
  q["input_rejections"]=j.value("input_rejections",J::array());
  if(j.value("input_rejected",false)){
   need(!q["accepted"].get<bool>()&&j["pose_evaluated"]==false,"rejected input must never pose-match");
   for(int side:{0,1}){
    const auto& input=j[side?"observation":"reference"]["input"];
    need(input["binding"]["gray_sha256"]==s["sides"][side]["landmark"]["image_sha256"],"actual failed-input SHA");
   }
  }
  // Score all actually refined observation points against the generating curve.
  // Controlled error study only; this is not field calibration or runtime truth input.
  const auto& observation=j["observation"];
  const J* refinement=nullptr;
  if(observation.contains("span"))refinement=&observation["span"]["refinement"];
  else if(observation["input"].contains("gray_audit"))refinement=&observation["input"]["gray_audit"]["refinement"];
  if(refinement){
   J reasons=J::object();size_t accepted=0,runs=0,run_length=0;double sum=0,maximum=0;
   auto distance=[&](Z image_point){
    Z p=(image_point-shift)/factor;double best_x=0,best=1e100;
    for(int k=0;k<=1000;++k){double x=k*.1,d=std::norm(p-Z(x,curve(x)));if(d<best){best=d;best_x=x;}}
    double lo=best_x-.1,hi=best_x+.1;
    for(int k=0;k<40;++k){double l=lo+(hi-lo)/3,h=hi-(hi-lo)/3;
     if(std::norm(p-Z(l,curve(l)))<std::norm(p-Z(h,curve(h))))hi=h;else lo=l;}
    return std::abs(factor)*std::abs(p-Z((lo+hi)*.5,curve((lo+hi)*.5)));
   };
   for(const auto& p:refinement->at("points")){
    const auto reason=p["status"].get<std::string>();reasons[reason]=reasons.value(reason,0)+1;
    if(p["accepted"].get<bool>()){
     ++accepted;++run_length;const auto xy=p["refined"]["point"];const double e=distance(Z(xy[0],xy[1]));sum+=e;maximum=std::max(maximum,e);
    }else{if(run_length>=3)++runs;run_length=0;}
   }if(run_length>=3)++runs;
   q["observation_refinement"]={{"status_counts",reasons},{"accepted_points",accepted},{"retained_contiguous_runs_min3",runs},
    {"truth_point_to_curve_max_px",accepted?J(maximum):J(nullptr)},{"truth_point_to_curve_mean_px",accepted?J(sum/accepted):J(nullptr)},
    {"truth_error_method","dense bracket plus local minimization on generating curve; scoring only"}};
   if(id=="local_contrast_dropout")need(runs>=2,"dropout must retain separated runs, not join gap");
  }
  if(id=="blur_3p2")need(!q["accepted"].get<bool>()&&q["reason"]=="LANDMARK_IDENTITY_NOT_CONFIRMED","blur identity gap must remain observable");
  if(id=="blur_8"||id=="local_contrast_dropout")need(j["status"]=="MATCHING_LANDMARK_TOO_FAR","do not relax projected-landmark support");
  if(id=="noise_4"||id=="sharpen_0p8"){
   need(q["accepted"].get<bool>()&&q["angle_error_deg"].get<double>()<1&&q["scale_error_absolute"].get<double>()<.05&&q["translation_error_px"].get<double>()<.5,"bounded noise/sharpen positive quality");
  }
  if(id=="missing_boundary")need(j["status"]=="MASK_BOUNDARY_NO_ANCHORED_CONTOUR","missing contour rejected");
  if(id=="boundary_gap")need(j["input_rejections"][0]["mask_diagnostics"]["candidate_count"]==2,"two unselected components retained");
  if(j.value("input_rejected",false)){
   auto legacy=s;legacy["parameters"]["input_rejection_receipt_enabled"]=0;
   bool thrown=false;try{execute(legacy);}catch(const std::invalid_argument& e){thrown=std::string(e.what())==j["status"].get<std::string>();}
   need(thrown,"disabled capture preserves exact legacy data exception");
  }
  if(id=="clean_control")need(q["accepted"],"clean positive control");
  if(id=="roi_snap_ambiguous"){
   need(j["status"]=="BOUNDARY_ENDPOINT_AMBIGUOUS_SNAP","reproduce exact -45/.8 ROI ambiguity");
   const auto& f=j["input_rejections"][0];need(f["side"]==1&&f["selection_diagnostics"]["candidates"].size()>=2,"ambiguity alternatives retained");
  }
  if(id=="low_contrast")need(j["status"]=="REFINED_ARC_SPAN_UNAVAILABLE","low contrast must not invent refined span");
  if(id=="boundary_gap")need(j["status"]=="MASK_BOUNDARY_AMBIGUOUS_CONTOURS","gap must not choose or join components");
  write(dir/"result_summary.json",q.dump(2));
  write(dir/"typed_label.json",J{{"schema","controlled_paired_boundary_label.v1"},{"geometry_type","open_curve"},{"annotation_status","PROPOSED"},
   {"human_reviewed",false},{"training_eligible",false},{"source_kind","controlled_boundary_observation_stress"}}.dump(2));
  write(dir/"case_manifest.json",J{{"schema","cxvision.evidence_case.v1"},{"run_id","landmark_boundary_stress_v1"},
   {"internal_case_id",id},{"review_item","Boundary Stress - "+id},{"tool","RefinedArcAudit"},{"display_group","Harmonic / Boundary Stress"},
   {"display_category","To Verify"},{"case_role","controlled_observation_stress"},{"geometry_type","open_curve"},{"binding_status","REFERENCE_ONLY"},
   {"parameter_summary","Input rejection evidence; blur/contrast/gap/noise; no automatic endpoint choice or gap bridging."},
   {"source_image","reference.pgm"},{"typed_label","typed_label.json"},{"geometry_facts_ref","native.json"},{"evidence_overlay","reference_overlay.png"},
   {"result_summary","result_summary.json"},{"script_snapshot","run.cxsc"},
   {"required_assets",{"reference.pgm","observation.pgm","run.cxsc","case_config.json","native.json","reference_overlay.png","result_summary.json","typed_label.json","fixture_parameters.json"}}}.dump(2));
  roots.push_back(id);matrix["cases"].push_back({{"id",id},{"recipe",recipe},{"result",q}});
  std::cout<<id<<" "<<q.dump()<<"\n";
 }
 write(root/"matrix.json",matrix.dump(2));fs::create_directories(root/"_shared");
 write(root/"_shared/evidence_case_roots.json",J{{"schema","cxvision.evidence_case_roots.v1"},{"roots",roots}}.dump(2));
 std::cout<<"LANDMARK_STRESS_GUARDS_PASS no production or field calibration\n";return 0;
}catch(const mu::Parser::exception_type& e){std::cerr<<e.GetMsg()<<"\n";return 1;}
catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
