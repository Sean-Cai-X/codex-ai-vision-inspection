#include "muParser.h"
#include "../../cximage/CxRefinedArcUiParameters.h"
#include "../../libtorchsegmentation/src/utils/json.hpp"
#include <opencv2/imgcodecs.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
namespace fs=std::filesystem;using J=nlohmann::json;using namespace cxpartialimage;using namespace cxgeom::polar;
void need(bool b,const char* why){if(!b)throw std::runtime_error(why);}
J read(const fs::path& p){std::ifstream f(p);J j;f>>j;need(bool(f),"read");return j;}
void save(const fs::path& p,const J& j){std::ofstream f(p);f<<j.dump(2);f.close();need(bool(f),"save");}
double curve(double x,int profile){double z=x-40;return profile==0?42+.12*z+.006*z*z+.0001*z*z*z:42+.24*z+.006*z*z-.00018*z*z*z;}
J score(const J& receipt,double angle,double scale,std::complex<double> shift){
 J m={{"quality_pass",false},{"status",receipt["status"]}};
 if(receipt["refined"]["candidates"].empty())return m;
 const auto& c=receipt["refined"]["candidates"][0];auto t=c["translation"];
 const double ae=std::abs(std::remainder(c["angle_deg"].get<double>()-angle,360.)),se=std::abs(c["scale"].get<double>()-scale);
 const double te=std::abs(std::complex<double>(t[0],t[1])-shift);
 return {{"quality_pass",c["accepted"].get<bool>()&&ae<=1&&se<=.05&&te<=.5},
  {"angle_error_deg",ae},{"scale_error_absolute",se},{"translation_error_px",te},{"internal_fit_accepted",c["accepted"]},{"reason",c["reason"]}};
}
void projection_guards(){
 std::vector<std::complex<double>> line={{0,0},{1,0},{2,0},{3,0},{4,0}};
 MatchingArcLandmarks a{{1,0},{3,0},.25,1e-8,"unit:matching-not-ROI"};
 auto p=ProjectMatchingLandmarks(line,a);need(p.status=="MATCHING_LANDMARK_ARC_AVAILABLE","interior");
 need(p.coordinates.values[0]==-.5&&p.coordinates.values.back()==1.5,"extent independent of landmarks");
 std::swap(a.start,a.end);auto reversed=ProjectMatchingLandmarks(line,a);
 need(reversed.coordinates.reversed&&reversed.coordinates.values[1]==1,"reversed");
 a.start={1,2};need(ProjectMatchingLandmarks(line,a).status=="MATCHING_LANDMARK_TOO_FAR","distance");
 a.start={0,0};need(ProjectMatchingLandmarks(line,a).status=="MATCHING_LANDMARK_OUTSIDE_INTERIOR_SUPPORT","no extrapolation");
 a.start=a.end;need(ProjectMatchingLandmarks(line,a).status=="MATCHING_LANDMARK_COLLAPSED_ARC","collapsed");
 std::vector<std::complex<double>> folded={{0,0},{2,2},{0,2},{2,0},{3,0}};
 a.start={1,1};a.end={2.5,0};need(ProjectMatchingLandmarks(folded,a).status=="MATCHING_LANDMARK_AMBIGUOUS_PROJECTION","crossing ambiguity");
 a.provenance="";bool rejected=false;try{ProjectMatchingLandmarks(line,a);}catch(const std::invalid_argument&){rejected=true;}need(rejected,"trace");
}
J execute(const J& s,cv::Mat* overlay=nullptr){
 const auto script=cxpairui::Build(s);need(cxpairui::Read(script)==s,"canonical recipe roundtrip");
 mu::Parser parser;double* org=nullptr;parser.DefineOrgClass("double",org);parser.UsingClass(true);
 RegisterRefinedArcAudit(parser);parser.SetExpr(script);parser.Eval();
 auto* tool=static_cast<CxRefinedArcAudit*>(parser.GetClassObj("RefinedArcAudit","a"));need(tool!=nullptr,"actual CxScript object");
 if(overlay)*overlay=tool->sideoverlay(0);
 return J::parse(tool->receipt());
}
int main(int argc,char** argv)try{
 need(argc==3||argc==4,"frozen fixture root / output root [verify-headless]");
 if(argc==4&&std::string(argv[3])=="verify-headless"){
  for(const char* id:{"curve_combined","curve_missing","s_curve_extra"}){
   const fs::path out=argv[2];
   need(read(out/id/"native.json")==read(out/"headless"/id/"refined_arc_receipt_0.json"),"actual app exact parity");
  }
  std::cout<<"MATCHING_LANDMARK_HEADLESS_PARITY_PASS\\n";return 0;
 }
 projection_guards();
 const fs::path input=argv[1],out=argv[2];fs::create_directories(out);const auto baseline=read(input/"matrix.json");
 const fs::path run_root=argc==4?fs::absolute(argv[3]):fs::absolute(out);
#ifdef _WIN32
 _putenv_s("CXVISION_RUN_ROOT",run_root.string().c_str());
#else
 setenv("CXVISION_RUN_ROOT",run_root.string().c_str(),1);
#endif
 J matrix={{"schema","matching_landmark_arc_comparison_matrix.v1"},{"production_eligible",false},
  {"baseline_quality_pass",baseline["quality_pass"]},{"quality_limits",baseline["quality_limits"]},
  {"images_regenerated",false},{"thresholds_relaxed",false},{"truth_usage","scoring_only; controlled_corresponding_local_landmarks_are_explicit_inputs"},
  {"autonomous_landmark_discovery",false},{"cases",J::array()},{"guard_tests_passed",7}};
 int quality=0;
 for(const auto& item:baseline["cases"]){
  const auto id=item["id"].get<std::string>();const auto dir=input/id;const auto cfg=read(dir/"case_config.json");
  const double angle=item["angle_deg"],scale=item["scale"];const int profile=item["profile"];
  const auto factor=std::polar(scale,angle*std::acos(-1.)/180.);
  const auto shift=std::complex<double>(70.3,65.1)-factor*std::complex<double>(40,42);
  auto image=[&](int i){return cv::imread((dir/(i?"observation.pgm":"reference.pgm")).string(),cv::IMREAD_GRAYSCALE);};
  auto endpoints=[&](int i){
   const auto& e=cfg["sides"][i]["endpoints"];
   return BoundaryEndpointAnchors{{e["start_x"],e["start_y"]},{e["end_x"],e["end_y"]},{e["via_x"],e["via_y"]},e["maximum_snap_px"],e["tie_epsilon_px"],cfg["sides"][i]["endpoint_provenance"]};
  };
  const auto re=endpoints(0),oe=endpoints(1);
  const auto rf=BoundGrayFrame::Capture(image(0),cfg["sides"][0]["case_id"],"v1",128,{0,0,128,128},&re);
  const auto of=BoundGrayFrame::Capture(image(1),cfg["sides"][1]["case_id"],"v1",128,{0,0,128,128},&oe);
  const auto rs=rf.refined_spans(),os=of.refined_spans();need(rs.size()==1&&os.size()==1,"same accepted spans");
  CommonRefinedArcConfig c;c.correspondence_id="controlled:interior-matching:"+id;c.reliable_landmarks_required=true;
  const auto original=read(dir/"native.json");
  for(int i:{0,1}){
   const auto& l=cfg["sides"][i]["landmark"];c.landmarks[i]={l["source"],l["trace"],l["image_sha256"],l["localization_error_px"]};
   const auto binding=J::parse((i?os[0]:rs[0]).receipt())["binding"]["gray_sha256"];
   need(binding==original[i?"observation":"reference"]["span"]["binding"]["gray_sha256"],"frozen image binding");
  }
  const auto p0=std::complex<double>(36,curve(36,profile)),p1=std::complex<double>(52,curve(52,profile));
  MatchingArcLandmarks a{p0,p1,.25,1e-8,"controlled:interior-physical-landmarks:x36-x52"};
  MatchingArcLandmarks b{factor*p0+shift,factor*p1+shift,.25,1e-8,a.provenance};
  const auto result=MatchLandmarkRefinedArc(rs[0],os[0],c,a,b);const auto receipt=J::parse(result.receipt());
  fs::create_directories(out/id);
  auto s=cfg;s["parameters"]["matching_landmark_arc_enabled"]=1;s["correspondence_id"]=c.correspondence_id;
  const MatchingArcLandmarks* matching[2]={&a,&b};
  for(int i:{0,1}){
   const auto name=i?"observation.pgm":"reference.pgm";
   fs::copy_file(dir/name,out/id/name,fs::copy_options::overwrite_existing);
   s["sides"][i]["image"]=fs::relative(out/id/name,run_root).generic_string();const auto& m=*matching[i];
   s["sides"][i]["matching"]={{"start_x",m.start.real()},{"start_y",m.start.imag()},
    {"end_x",m.end.real()},{"end_y",m.end.imag()},{"maximum_distance_px",m.maximum_distance_px},
    {"tie_epsilon_px",m.tie_epsilon_px},{"provenance",m.provenance}};
  }
  cv::Mat overlay;const auto actual=execute(s,&overlay);need(actual==receipt,"real CxScript/core exact parity");
  {std::ofstream f(out/id/"run.cxsc");f<<cxpairui::Build(s);need(bool(f),"script");}
  save(out/id/"case_config.json",s);save(out/id/"native.json",actual);
  need(cv::imwrite((out/id/"reference_overlay.png").string(),overlay),"actual overlay");
  const auto old_script=read(dir/"case_config.json");
  need(cxpairui::Read(cxpairui::Build(old_script))==old_script,"legacy recipe remains readable");
  auto manifest=read(dir/"case_manifest.json");manifest["review_item"]="Matching Boundary - "+id;
  manifest["run_id"]="matching_landmark_refined_arc_v1";manifest["parameter_summary"]="ROI selection separate from interior matching landmarks; continuous derived arc; explicit source and uncertainty.";
  save(out/id/"case_manifest.json",manifest);
  fs::copy_file(dir/"typed_label.json",out/id/"typed_label.json",fs::copy_options::overwrite_existing);
  const auto metric=score(receipt,angle,scale,shift);quality+=metric["quality_pass"].get<bool>();
  auto raw_a=a,raw_b=b;raw_a.maximum_distance_px=2;raw_b.maximum_distance_px=2;
  const auto raw_r=ProjectMatchingLandmarks(rf.raw().ordered_run().points,raw_a);
  const auto raw_o=ProjectMatchingLandmarks(of.raw().ordered_run().points,raw_b);
  need(raw_r.status=="MATCHING_LANDMARK_ARC_AVAILABLE"&&raw_o.status=="MATCHING_LANDMARK_ARC_AVAILABLE","raw ablation anchors");
  auto ablated=c;
  const double rl=arc_detail::SampleArcPoints(rf.raw().ordered_run().points,3).length,ol=arc_detail::SampleArcPoints(of.raw().ordered_run().points,3).length;
  ablated.reference={raw_r.arc_lengths[0]/rl,raw_r.arc_lengths[1]/rl};
  ablated.observation={raw_o.arc_lengths[0]/ol,raw_o.arc_lengths[1]/ol};
  const auto ablation=J::parse(MatchCommonRefinedArc(rs[0],os[0],ablated).receipt());
  const auto ablation_metric=score(ablation,angle,scale,shift);
  save(out/id/"same_landmarks_raw_coordinate_receipt.json",ablation);
  save(out/id/"same_landmarks_raw_coordinate_quality.json",ablation_metric);
  fs::create_directories(out/id);save(out/id/"receipt.json",receipt);save(out/id/"quality.json",metric);save(out/id/"result_summary.json",metric);
  matrix["cases"].push_back({{"id",id},{"baseline",item["quality"]},{"new",metric},
   {"selection_endpoints_changed",false},{"matching_landmarks_independent_of_selection",true},
   {"historical_comparison_matching_inputs_and_support_changed",true},{"same_landmark_raw_coordinate_ablation",ablation_metric},
   {"matching_source_x_interval",{36,52}},{"whole_shape_quality_approved",false},
   {"reference_projection",receipt["inputs"][0]["projection"]},{"observation_projection",receipt["inputs"][1]["projection"]},
   {"full_raw_under_refined_pose",receipt["full_raw_under_refined_pose"]}});
  std::cout<<id<<" "<<metric.dump()<<'\n';
  J sensitivities=J::array();
  for(double offset:{-.75,-.25,0.,.25,.75}){
   auto perturbed=b;const auto direction=std::polar(1.,std::arg(factor*(p1-p0)));
   perturbed.start+=offset*direction;perturbed.end+=offset*direction;
   const auto r=MatchLandmarkRefinedArc(rs[0],os[0],c,a,perturbed);
   auto guarded=cxlandmarkquality::Config{};guarded.identity_required=true;
   const auto guarded_receipt=J::parse(MatchLandmarkRefinedArc(rs[0],os[0],c,a,perturbed,guarded).receipt());
   need(guarded_receipt["refined"]["accepted_indices"].empty(),"weak-curve misidentification cannot pass identity gate");
   sensitivities.push_back({{"guarded_status",guarded_receipt["status"]},{"guarded_accepted",false},
    {"guarded_identity",guarded_receipt.value("identity_diagnostics",J::array())},{"tangential_landmark_offset_px",offset},{"diagnostic_only",true},
    {"declared_error_intentionally_unchanged",true},{"physical_identity_not_verified",true},
    {"quality",score(J::parse(r.receipt()),angle,scale,shift)}});
  }
  save(out/id/"landmark_sensitivity.json",sensitivities);
  matrix["cases"].back()["landmark_sensitivity"]=sensitivities;
  if(id=="curve_identity"){
   for(const char* negative:{"matching_shifted_landmarks","matching_distance_rejected","matching_sha_rejected","matching_uncertainty_rejected"}){
    auto v=s;const std::string nid=negative;const auto ndir=out/nid;fs::create_directories(ndir);
    for(int i:{0,1}){
     const auto name=i?"observation.pgm":"reference.pgm";fs::copy_file(dir/name,ndir/name,fs::copy_options::overwrite_existing);
     v["sides"][i]["image"]=fs::relative(ndir/name,run_root).generic_string();
    }
    if(nid=="matching_sha_rejected")v["sides"][1]["landmark"]["image_sha256"]=std::string(64,'0');
    else if(nid=="matching_distance_rejected")v["sides"][1]["matching"]["start_y"]=b.start.imag()+3;
    else{
     const auto offset=.75*std::polar(1.,std::arg(factor*(p1-p0)));
     v["sides"][1]["matching"]["start_x"]=b.start.real()+offset.real();
     v["sides"][1]["matching"]["start_y"]=b.start.imag()+offset.imag();
     v["sides"][1]["matching"]["end_x"]=b.end.real()+offset.real();
     v["sides"][1]["matching"]["end_y"]=b.end.imag()+offset.imag();
     if(nid=="matching_uncertainty_rejected")v["sides"][1]["landmark"]["localization_error_px"]=.75;
    }
    cv::Mat no;const auto nr=execute(v,&no);const auto nq=score(nr,angle,scale,shift);
    if(nid=="matching_shifted_landmarks"){
     need(nr["status"]=="UNIQUE_ARC_CANDIDATE"&&!nq["quality_pass"].get<bool>(),"factual physical identity counterexample must remain visible");
    }else{
     need(nr["refined"]["accepted_indices"].empty(),"negative candidate blocked");
     if(nid=="matching_sha_rejected")need(nr["status"]=="LANDMARK_IMAGE_SHA_MISMATCH","negative SHA");
     if(nid=="matching_uncertainty_rejected")need(nr["status"]=="LANDMARK_LOCALIZATION_ERROR_EXCEEDED","honest error declaration");
     if(nid=="matching_distance_rejected")need(nr["status"]=="MATCHING_LANDMARK_TOO_FAR","negative distance");
    }
    {std::ofstream f(ndir/"run.cxsc");f<<cxpairui::Build(v);need(bool(f),"negative script");}
    save(ndir/"case_config.json",v);save(ndir/"native.json",nr);save(ndir/"result_summary.json",nq);
    need(cv::imwrite((ndir/"reference_overlay.png").string(),no),"negative overlay");
    fs::copy_file(dir/"typed_label.json",ndir/"typed_label.json",fs::copy_options::overwrite_existing);
    auto nm=manifest;nm["internal_case_id"]=nid;nm["review_item"]="Matching Boundary - "+nid;
    nm["parameter_summary"]="Counterexample/rejection: an accepted local fit is not verified physical landmark identity; production=false.";
    save(ndir/"case_manifest.json",nm);
   }
   auto bad=b;bad.start+=std::complex<double>(0,3);
   const auto refusal=MatchLandmarkRefinedArc(rs[0],os[0],c,a,bad);
   need(refusal.refined.accepted_indices.empty(),"bad projection rejected");save(out/"far_landmark_rejected.json",J::parse(refusal.receipt()));
   auto sha=c;sha.landmarks[1].image_sha256=std::string(64,'0');
   const auto rejected=MatchLandmarkRefinedArc(rs[0],os[0],sha,a,b);
   need(rejected.refined.status=="LANDMARK_IMAGE_SHA_MISMATCH","SHA still enforced");save(out/"sha_rejected.json",J::parse(rejected.receipt()));
  }
 }
 matrix["quality_pass"]=quality;matrix["total"]=baseline["total"];save(out/"matrix.json",matrix);
 fs::create_directories(out/"_shared");J roots=J::array();
 for(const auto& item:matrix["cases"])roots.push_back(item["id"]);
 for(const char* id:{"matching_shifted_landmarks","matching_distance_rejected","matching_sha_rejected","matching_uncertainty_rejected"})roots.push_back(id);
 save(out/"_shared/evidence_case_roots.json",{{"schema","cxvision.evidence_case_roots.v1"},{"roots",roots}});
 need(quality==11&&baseline["total"]==11,"fixed controlled matching quality regressed");
 std::cout<<"MATCHING_LANDMARK_ARC_TEST_PASS quality "<<quality<<"/"<<baseline["total"]<<" (not production)\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
