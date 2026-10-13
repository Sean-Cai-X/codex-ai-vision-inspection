#include "muParser.h"
#include "../../cximage/CxRefinedArcUiParameters.h"
#include <filesystem>
#include <fstream>
#include <iostream>
namespace fs=std::filesystem;using J=nlohmann::json;using namespace cxpartialimage;using namespace cxgeom::polar;
void need(bool b,const char* why){if(!b)throw std::runtime_error(why);}
void write(const fs::path& p,const std::string& text){std::ofstream f(p);f<<text;f.close();need(bool(f),"write");}
J read(const fs::path& p){std::ifstream f(p);J j;f>>j;return j;}
double curve(double x,int profile){double z=x-40;return profile==0?42+.12*z+.006*z*z+.0001*z*z*z:42+.24*z+.006*z*z-.00018*z*z*z;}
double slope(double x,int profile){double z=x-40;return profile==0?.12+.012*z+.0003*z*z:.24+.012*z-.00054*z*z;}
cv::Mat render(std::complex<double> factor,std::complex<double> shift,int profile){
 cv::Mat image(128,128,CV_8UC1);
 for(int y=0;y<128;++y)for(int x=0;x<128;++x){
  auto q=(std::complex<double>(x,y)-shift)/factor;
  const double d=(q.imag()-curve(q.real(),profile))*std::abs(factor)/std::sqrt(1+slope(q.real(),profile)*slope(q.real(),profile));
  image.at<unsigned char>(y,x)=cv::saturate_cast<unsigned char>(128+100*std::erf(d/1.6));
 }return image;
}
double fraction(const BoundGrayFrame& frame,std::complex<double> p){
 const auto& raw=frame.raw().ordered_run().points;size_t index=0;double best=1e30;
 for(size_t i=0;i<raw.size();++i)if(std::abs(raw[i]-p)<best){best=std::abs(raw[i]-p);index=i;}
 return common_arc_detail::RawFractions(raw)[index];
}
J run(const J& s){
 mu::Parser parser;double* org=nullptr;parser.DefineOrgClass("double",org);parser.UsingClass(true);
 RegisterRefinedArcAudit(parser);parser.SetExpr(cxpairui::Build(s));parser.Eval();
 auto* a=static_cast<CxRefinedArcAudit*>(parser.GetClassObj("RefinedArcAudit","a"));need(a!=nullptr,"actual parser");
 return J::parse(a->receipt());
}
int main(int argc,char** argv)try{
 need(argc>=2,"root");const auto root=fs::absolute(argv[1]);fs::create_directories(root);
 if(argc==3&&std::string(argv[2])=="verify-headless"){
  for(const char* id:{"curve_combined","s_curve_missing","landmark_sha_rejected"}){
   auto dir=root/id;
   need(read(dir/"native.json")==read(root/"headless"/id/"refined_arc_receipt_0.json"),"paired actual app parity");
   need(fs::exists(root/"headless"/id/"manual_algorithm_review_handoff.md"),"actual handoff");
  }
  std::cout<<"PAIRED_CURVE_HEADLESS_PARITY_PASS\n";return 0;
 }
 const fs::path run_root=argc==3?fs::absolute(argv[2]):root;
#ifdef _WIN32
 _putenv_s("CXVISION_RUN_ROOT",run_root.string().c_str());
#else
 setenv("CXVISION_RUN_ROOT",run_root.string().c_str(),1);
#endif
 J matrix={{"schema","independent_asymmetric_boundary_matrix.v1"},{"production_eligible",false},
  {"fixture","two_independently_rasterized_analytic_asymmetric_gray_boundaries"},
  {"truth_usage","pose_scoring_only; endpoint_inputs_are_controlled_landmarks"},
  {"quality_limits",{{"angle_deg",1.},{"scale_absolute",.05},{"translation_px",.5}}},
  {"cases",J::array()}};int quality=0,total=0;
 struct Case{const char* id;int profile;double angle,scale,lo,hi;};
 const std::vector<Case> cases={{"curve_identity",0,0,1,24,62},{"curve_rotation",0,37,1,24,62},
  {"curve_scale",0,0,1.5,24,62},{"curve_combined",0,37,1.5,24,62},{"curve_negative",0,-60,.75,24,62},
  {"curve_missing",0,37,1.25,32,56},{"curve_extra",0,-60,1.25,16,70},
  {"s_curve_identity",1,0,1,24,62},{"s_curve_combined",1,37,1.5,24,62},
  {"s_curve_missing",1,-60,1.25,32,56},{"s_curve_extra",1,37,1.25,16,70}};
 std::vector<std::string> roots;
 J saved;
 for(const auto& c:cases){
  const auto dir=root/c.id;fs::create_directories(dir);roots.push_back(c.id);
  const auto factor=std::polar(c.scale,c.angle*std::acos(-1.)/180.);
  const std::complex<double> center(40,42),target(70.3,65.1),shift=target-factor*center;
  auto point=[&](double x){return std::complex<double>(x,curve(x,c.profile));};
  const auto ref=render({1,0},{0,0},c.profile),obs=render(factor,shift,c.profile);
  need(cv::imwrite((dir/"reference.pgm").string(),ref),"ref image");need(cv::imwrite((dir/"observation.pgm").string(),obs),"obs image");
  BoundaryEndpointAnchors ra{point(24),point(62),point(43),2.,1e-8,"controlled_asymmetric_endpoint_input"};
  BoundaryEndpointAnchors oa{factor*point(c.lo)+shift,factor*point(c.hi)+shift,factor*point((c.lo+c.hi)/2)+shift,2.,1e-8,"controlled_asymmetric_endpoint_input"};
  auto rf=BoundGrayFrame::Capture(ref,std::string(c.id)+":reference","v1",128,{0,0,128,128},&ra);
  auto of=BoundGrayFrame::Capture(obs,std::string(c.id)+":observation","v1",128,{0,0,128,128},&oa);
  auto rs=rf.refined_spans(),os=of.refined_spans();need(rs.size()==1&&os.size()==1,"fixture must have exactly one accepted open span");
  auto s=cxpairui::Default();s["correspondence_id"]=std::string("controlled:")+c.id;
  const double lo=std::max(24.,c.lo),hi=std::min(62.,c.hi);
  for(int i:{0,1}){
   auto& side=s["sides"][i];const auto& anchors=i==0?ra:oa;
   side["image"]=fs::relative(dir/(i==0?"reference.pgm":"observation.pgm"),run_root).generic_string();
   side["case_id"]=std::string(c.id)+(i==0?":reference":":observation");
   side["gray"]["anchor_x"]=0;side["gray"]["anchor_y"]=0;side["gray"]["anchor_width"]=128;side["gray"]["anchor_height"]=128;
   side["endpoints"]={{"start_x",anchors.start.real()},{"start_y",anchors.start.imag()},{"end_x",anchors.end.real()},{"end_y",anchors.end.imag()},
    {"via_x",anchors.via.real()},{"via_y",anchors.via.imag()},{"maximum_snap_px",anchors.maximum_snap_px},{"tie_epsilon_px",anchors.tie_epsilon_px}};
   side["endpoint_provenance"]=anchors.provenance;
   side["landmark"]={{"source","CONTROLLED_REFERENCE"},{"trace",std::string("fixture:")+c.id+":physical-endpoints"},
    {"image_sha256",cxpairui::ImageSha(side)},{"localization_error_px",0.}};
   side["raw_anchor_begin"]=i==0?fraction(rf,point(lo)):fraction(of,factor*point(lo)+shift);
   side["raw_anchor_end"]=i==0?fraction(rf,point(hi)):fraction(of,factor*point(hi)+shift);
  }
  const auto j=run(s);need(j["production_eligible"]==false,"development");
  need(j["landmark_gate"]["sides"][0]["status"]=="LANDMARK_DECLARATION_BOUND_TO_IMAGE","source binding");
  J metric={{"quality_pass",false},{"status",j["status"]}};
  if(!j["refined"]["candidates"].empty()){
   const auto& candidate=j["refined"]["candidates"][0];
   const double ae=std::abs(std::remainder(candidate["angle_deg"].get<double>()-c.angle,360.));
   const double se=std::abs(candidate["scale"].get<double>()-c.scale);
   const auto t=candidate["translation"];
   const double te=std::abs(std::complex<double>(t[0],t[1])-shift);
   const bool pass=candidate["accepted"].get<bool>()&&ae<=1&&se<=.05&&te<=.5;quality+=pass;
   metric={{"quality_pass",pass},{"angle_error_deg",ae},{"scale_error_absolute",se},{"translation_error_px",te},
    {"internal_fit_accepted",candidate["accepted"]},{"reason",candidate["reason"]}};
  }
  ++total;matrix["cases"].push_back({{"id",c.id},{"profile",c.profile},{"angle_deg",c.angle},{"scale",c.scale},
   {"observed_source_extent",{c.lo,c.hi}},{"quality",metric},{"common_support",j["common_support"]},{"full_raw_under_refined_pose",j["full_raw_under_refined_pose"]}});
  write(dir/"run.cxsc",cxpairui::Build(s));write(dir/"native.json",j.dump(2));write(dir/"result_summary.json",metric.dump(2));
  auto nativeoverlay=ref.clone();cv::Mat color;cv::cvtColor(nativeoverlay,color,cv::COLOR_GRAY2BGR);
  for(size_t k=1;k<rs[0].refined_points().size();++k){
   auto a=rs[0].refined_points()[k-1],b=rs[0].refined_points()[k];
   cv::line(color,{cvRound(a.real()),cvRound(a.imag())},{cvRound(b.real()),cvRound(b.imag())},{0,220,0},1);
  }need(cv::imwrite((dir/"reference_overlay.png").string(),color),"overlay");
  write(dir/"typed_label.json",J{{"schema","controlled_paired_boundary_label.v1"},{"geometry_type","open_curve"},
   {"annotation_status","PROPOSED"},{"human_reviewed",false},{"training_eligible",false},{"source_kind","generated_analytic_boundary_pair"}}.dump(2));
  write(dir/"case_config.json",s.dump(2));
  write(dir/"case_manifest.json",J{{"schema","cxvision.evidence_case.v1"},{"run_id","paired_boundary_landmarks_v1"},
   {"internal_case_id",c.id},{"review_item",std::string("Paired Boundary - ")+c.id},{"tool","RefinedArcAudit"},
   {"display_group","Harmonic / Paired Boundary"},{"display_category","To Verify"},{"case_role","controlled_independent_gray_pair"},
   {"geometry_type","open_curve"},{"binding_status","REFERENCE_ONLY"},{"parameter_summary","Two local images; explicit landmarks; source hashes; actual pose and full-Raw diagnostics."},
   {"source_image","reference.pgm"},{"typed_label","typed_label.json"},{"geometry_facts_ref","native.json"},{"evidence_overlay","reference_overlay.png"},{"result_summary","result_summary.json"},{"script_snapshot","run.cxsc"},
   {"required_assets",{"reference.pgm","observation.pgm","run.cxsc","native.json","reference_overlay.png","result_summary.json","typed_label.json","case_config.json"}}}.dump(2));
  if(std::string(c.id)=="curve_identity"){saved=s;need(metric["quality_pass"].get<bool>(),"identity must pass");}
 }
 for(const auto& mutation:std::vector<std::pair<std::string,std::string>>{{"landmark_sha_rejected","LANDMARK_IMAGE_SHA_MISMATCH"},{"landmark_error_rejected","LANDMARK_LOCALIZATION_ERROR_EXCEEDED"},{"landmark_source_rejected","LANDMARK_SOURCE_UNVERIFIED"},{"landmark_pose_error_rejected","NO_ARC_CANDIDATE"}}){
  auto s=saved;auto dir=root/mutation.first;fs::create_directories(dir);
  if(mutation.first=="landmark_sha_rejected")s["sides"][1]["landmark"]["image_sha256"]=std::string(64,'0');
  else if(mutation.first=="landmark_error_rejected")s["sides"][1]["landmark"]["localization_error_px"]=.8;
  else if(mutation.first=="landmark_pose_error_rejected"){
   s["sides"][0]["landmark"]["localization_error_px"]=.1;s["sides"][1]["landmark"]["localization_error_px"]=.1;
   s["parameters"]["maximum_landmark_pose_error_px"]=.1;
  }else s["sides"][1]["landmark"]["source"]="UNVERIFIED";
  const auto j=run(s);need(j["status"]==mutation.second,"landmark rejection");need(j["refined"]["accepted_indices"].empty(),"no rejected candidate accepted");
  write(dir/"run.cxsc",cxpairui::Build(s));write(dir/"native.json",j.dump(2));write(dir/"result_summary.json",J{{"status",j["status"]},{"production_eligible",false}}.dump(2));
  for(const char* name:{"reference.pgm","observation.pgm","reference_overlay.png","typed_label.json"})
   fs::copy_file(root/"curve_identity"/name,dir/name,fs::copy_options::overwrite_existing);
  auto manifest=read(root/"curve_identity/case_manifest.json");manifest["internal_case_id"]=mutation.first;
  manifest["review_item"]="Paired Boundary - "+mutation.first;
  write(dir/"case_manifest.json",manifest.dump(2));write(dir/"case_config.json",s.dump(2));roots.push_back(mutation.first);
 }
 matrix["quality_pass"]=quality;matrix["total"]=total;write(root/"matrix.json",matrix.dump(2));
 fs::create_directories(root/"_shared");write(root/"_shared/evidence_case_roots.json",J{{"schema","cxvision.evidence_case_roots.v1"},{"roots",roots}}.dump(2));
 std::cout<<"INDEPENDENT_ASYMMETRIC_GRAY_PAIR_PASS numerical quality "<<quality<<"/"<<total<<"; source binding/rejection/schema execution only, not production\n";return 0;
}catch(const mu::Parser::exception_type& e){std::cerr<<e.GetMsg()<<"\n";return 1;}
catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
