#include "../../cximage/CxCommonRefinedArc.h"
#include <opencv2/imgcodecs.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace cxpartialimage;using namespace cxgeom::polar;using J=nlohmann::json;
void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
cv::Mat gray_line(std::complex<double> n,std::complex<double> center){
 cv::Mat gray(128,128,CV_8UC1);
 for(int y=0;y<128;++y)for(int x=0;x<128;++x)
  gray.at<unsigned char>(y,x)=cv::saturate_cast<unsigned char>(128+100*std::erf(std::real(std::conj(n)*(std::complex<double>(x,y)-center))/1.6));
 return gray;
}
double along(std::complex<double> p,std::complex<double> unit){return std::real(std::conj(unit)*p);}
J geometry(const BoundGrayFrame& frame,const BoundRefinedSpan& span,const BoundaryEndpointAnchors& anchors,std::complex<double> tangent){
 const auto& raw=frame.raw().ordered_run().points;
 const auto bind=J::parse(frame.receipt());const bool swapped=bind["mask"]["endpoint_selection"]["endpoints_swapped"];
 auto first=swapped?anchors.end:anchors.start,last=swapped?anchors.start:anchors.end;
 double length=0;for(size_t i=1;i<raw.size();++i)length+=std::abs(raw[i]-raw[i-1]);
 auto fractions=common_arc_detail::RawFractions(raw);
 return J{{"raw_full_length_px",length},{"raw_projected_extent_px",std::abs(along(raw.back()-raw.front(),tangent))},
  {"requested_anchor_extent_px",std::abs(along(last-first,tangent))},
  {"raw_length_inflation",length/std::abs(along(raw.back()-raw.front(),tangent))},
  {"start_snap_along_px",along(raw.front()-first,tangent)},{"end_snap_along_px",along(raw.back()-last,tangent)},
  {"start_snap_normal_px",along(raw.front()-first,tangent*std::complex<double>(0,1))},
  {"end_snap_normal_px",along(raw.back()-last,tangent*std::complex<double>(0,1))},
  {"source_begin",span.source_begin()},{"source_end",span.source_end()},
  {"retained_raw_fraction_interval",{fractions[span.source_begin()],fractions[span.source_end()]}},
  {"raw_count",raw.size()},{"refined_count",span.refined_points().size()}};
}
int main(int argc,char** argv)try{
 check(argc>=2,"output root");std::filesystem::path root=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(root);
 if(argc==3&&std::string(argv[2])=="verify-headless"){
  for(const char* id:{"r37_s075","r37_s150","rm60_s075","rm60_s150"}){
   auto folder=root/id;
   auto read=[](const std::filesystem::path& path){std::ifstream f(path);J j;f>>j;return j;};
   check(read(folder/"native.json")==read(folder/"headless/refined_arc_receipt_0.json"),"four-case real app receipt parity");
   check(read(folder/"native.json")==read(folder/"script_saved.json"),"script save parity");
   check(std::filesystem::exists(folder/"headless/manual_algorithm_review_handoff.md"),"formal handoff");
  }
  std::cout<<"FOUR_FAILED_POSES_HEADLESS_PARITY_PASS\n";return 0;
 }
 J summary={{"schema","refined_pose_error_diagnostic.v1"},{"production_eligible",false},
  {"scope","controlled_halfplanes_with_explicit_oracle_endpoint_input_not_automatic_localization"},
  {"truth_usage","scoring_only_expected_pose_not_passed_to_matcher"},
  {"quality_limits",{{"angle_deg",1.},{"scale_absolute",.05},{"translation_px",.5}}},
  {"cases",J::array()},{"anchor_perturbations",J::array()}};int quality=0,projectedquality=0;
 for(double angle:{0.,37.,90.,-60.})for(double scale:{.75,1.,1.5}){
  auto factor=std::polar(scale,angle*std::acos(-1.)/180.);
  std::complex<double> baseline_factor{},baseline_translation{},baseline_center_error{};
  for(std::complex<double> placement:{std::complex<double>(0,0),{8,0},{0,8},{.25,.125},{-.2,.35}}){
   const std::complex<double> center=std::complex<double>(40.3,40.1)+placement,target_center=std::complex<double>(70.3,65.1)+placement;
   const auto translation=target_center-factor*center,normal=std::polar(1.,angle*std::acos(-1.)/180.);
   BoundaryEndpointAnchors ra{center-std::complex<double>(0,12),center+std::complex<double>(0,12),center,2.,1e-8,"explicit_controlled_endpoints"};
   BoundaryEndpointAnchors oa{factor*ra.start+translation,factor*ra.end+translation,target_center,2.,1e-8,"explicit_controlled_endpoints"};
   auto rf=BoundGrayFrame::Capture(gray_line({1,0},center),"diagnostic_reference","v1",128,{0,0,128,128},&ra);
   auto of=BoundGrayFrame::Capture(gray_line(normal,target_center),"diagnostic_observation","v1",128,{0,0,128,128},&oa);
   const auto rs=rf.refined_spans(),os=of.refined_spans();check(rs.size()==1&&os.size()==1,"one span");
   auto mutable_anchor=ra;
   auto snapshot=BoundGrayFrame::Capture(gray_line({1,0},center),"owned_anchor_snapshot","v1",128,{0,0,128,128},&mutable_anchor);
   mutable_anchor.start+=std::complex<double>(1,1);
   const auto snapshot_spans=snapshot.refined_spans();
   check(snapshot_spans[0].endpoint_anchors()->start==ra.start,"anchor input copied into immutable snapshot");
   CommonRefinedArcConfig cfg;cfg.correspondence_id="diagnostic_endpoint_pair";
   auto compared=MatchCommonRefinedArc(rs[0],os[0],cfg);const auto& c=compared.refined.candidates.front();
   const auto center_error=c.factor*center+c.translation-target_center;
   const auto factor_error=c.factor-factor,translation_error=c.translation-translation;
   check(std::abs(translation_error-(center_error-factor_error*center))<1e-10,"translation decomposition");
   double ae=std::abs(std::remainder(std::arg(c.factor)*180/std::acos(-1.)-angle,360.));
   bool pass=c.accepted&&ae<=1&&std::abs(std::abs(c.factor)-scale)<=.05&&std::abs(translation_error)<=.5;quality+=pass;
   if(placement==std::complex<double>{}){baseline_factor=c.factor;baseline_translation=c.translation;baseline_center_error=center_error;}
   else if(std::floor(placement.real())==placement.real()&&std::floor(placement.imag())==placement.imag()){
    check(std::abs(c.factor-baseline_factor)<1e-9,"integer placement factor invariant");
    check(std::abs(center_error-baseline_center_error)<1e-8,"integer placement physical error invariant");
    auto original_truth=std::complex<double>(70.3,65.1)-factor*std::complex<double>(40.3,40.1);
    auto predicted_delta=-(baseline_factor-factor)*placement;
    check(std::abs((translation_error-(baseline_translation-original_truth))-predicted_delta)<1e-8,"origin sensitivity explained");
   }
   auto receipt=J::parse(compared.receipt());
   J row={{"angle_deg",angle},{"scale",scale},{"placement",{placement.real(),placement.imag()}},
    {"reference_geometry",geometry(rf,rs[0],ra,{0,1})},{"observation_geometry",geometry(of,os[0],oa,normal*std::complex<double>(0,1))},
    {"angle_error_deg",ae},{"scale_error_absolute",std::abs(std::abs(c.factor)-scale)},
    {"translation_error_px",std::abs(translation_error)},{"center_error_px",std::abs(center_error)},
    {"center_normal_error_px",along(center_error,normal)},{"center_tangent_error_px",along(center_error,normal*std::complex<double>(0,1))},
    {"center_error",{center_error.real(),center_error.imag()}},{"factor_origin_contribution_px",std::abs(factor_error*center)},
    {"quality_pass",pass},{"fit_rms_px",c.fit_rms_px},{"internal_fit_accepted",c.accepted},
    {"full_raw_under_refined_pose",receipt["full_raw_under_refined_pose"]}};
   cfg.endpoint_projection_enabled=true;
   auto projected=MatchCommonRefinedArc(rs[0],os[0],cfg);
   check(!projected.refined.candidates.empty(),"projection candidate");
   const auto& pc=projected.refined.candidates.front();
   const auto pce=pc.factor*center+pc.translation-target_center;
   const double pae=std::abs(std::remainder(std::arg(pc.factor)*180/std::acos(-1.)-angle,360.));
   const double pse=std::abs(std::abs(pc.factor)-scale),pte=std::abs(pc.translation-translation);
   const bool pp=pc.accepted&&pae<=1&&pse<=.05&&pte<=.5;check(pp,"projected pose quality gate");projectedquality+=pp;
   row["endpoint_projected"]={{"quality_pass",pp},{"angle_error_deg",pae},{"scale_error_absolute",pse},
    {"translation_error_px",pte},{"center_error_px",std::abs(pce)},{"fit_rms_px",pc.fit_rms_px},{"accepted",pc.accepted}};
   row["endpoint_projected_receipt"]=J::parse(projected.receipt());
   if(placement==std::complex<double>{}&&!pass){
    const std::string id=std::string(angle==37?"r37":"rm60")+(scale==.75?"_s075":"_s150");
    auto folder=root/id;std::filesystem::create_directories(folder);
    check(cv::imwrite((folder/"reference.pgm").string(),gray_line({1,0},center)),"reference file");
    check(cv::imwrite((folder/"observation.pgm").string(),gray_line(normal,target_center)),"observation file");
    std::ofstream nf(folder/"native.json");nf<<projected.receipt();nf.close();check(bool(nf),"native receipt");
    std::ostringstream script;script<<std::setprecision(17)<<"RefinedArcAudit a;\n";
    for(int side:{0,1}){
     const auto& anchors=side==0?ra:oa;
     script<<"a.select("<<side<<"); a.caseid(\"diagnostic_"<<(side==0?"reference":"observation")<<"\"); a.revision(\"v1\");\n";
     script<<"a.image(\""<<(folder/(side==0?"reference.pgm":"observation.pgm")).generic_string()<<"\"); a.span(0);\n";
     for(const auto& v:std::vector<std::pair<std::string,double>>{{"anchor_x",0},{"anchor_y",0},{"anchor_width",128},{"anchor_height",128}})
      script<<"a.grayparameter("<<v.second<<",\""<<v.first<<"\");\n";
     for(const auto& v:std::vector<std::pair<std::string,double>>{{"start_x",anchors.start.real()},{"start_y",anchors.start.imag()},
       {"end_x",anchors.end.real()},{"end_y",anchors.end.imag()},{"via_x",anchors.via.real()},{"via_y",anchors.via.imag()},
       {"maximum_snap_px",anchors.maximum_snap_px},{"tie_epsilon_px",anchors.tie_epsilon_px}})
      script<<"a.endpointparameter("<<v.second<<",\""<<v.first<<"\");\n";
     script<<"a.endpointprovenance(\""<<anchors.provenance<<"\");\n";
    }
    script<<"a.parameter(1,\"common_support_enabled\"); a.parameter(1,\"endpoint_projection_enabled\");\n"
      <<"a.correspondence(\"diagnostic_endpoint_pair\"); a.run(); a.expectstatus(\"UNIQUE_ARC_CANDIDATE\");\n"
      <<"a.save(\""<<(folder/"script_saved.json").generic_string()<<"\");\n";
    std::ofstream sf(folder/"run.cxsc");sf<<script.str();sf.close();check(bool(sf),"script");
   }
   summary["cases"].push_back(row);
   if(placement==std::complex<double>{}){
    const auto image=gray_line(normal,target_center);
    for(double shift_px:{-.25,.25,.75}){
     auto shifted=oa;const auto shift_vector=normal*std::complex<double>(0,shift_px);
     shifted.start+=shift_vector;shifted.end+=shift_vector;shifted.via+=shift_vector;
     shifted.provenance="controlled_endpoint_tangent_perturbation";
     auto perturbed=BoundGrayFrame::Capture(image,"diagnostic_observation","v1",128,{0,0,128,128},&shifted);
     check(J::parse(perturbed.receipt())["gray_sha256"]==J::parse(of.receipt())["gray_sha256"],"identical image endpoint perturbation");
     auto spans=perturbed.refined_spans();check(spans.size()==1,"perturbation support");
     auto result=MatchCommonRefinedArc(rs[0],spans[0],cfg);
     const auto& cc=result.refined.candidates.front();
     const double ce=std::abs(cc.factor*center+cc.translation-target_center),te=std::abs(cc.translation-translation);
     summary["anchor_perturbations"].push_back({{"angle_deg",angle},{"scale",scale},{"endpoint_tangent_shift_px",shift_px},
      {"same_gray_sha",true},{"center_error_px",ce},{"translation_error_px",te},{"internal_fit_accepted",cc.accepted},
      {"anchor_correspondence_verified",false},{"interpretation","uniform_line_cannot_verify_tangent_landmark_identity"}});
     check(std::abs(ce-std::abs(shift_px))<.06,"physical endpoint uncertainty visible");
    }
   }
  }
 }
 summary["quality_pass"]=quality;summary["projected_quality_pass"]=projectedquality;summary["total"]=summary["cases"].size();
 std::ofstream f(root/"diagnostic.json");f<<summary.dump(2);f.close();check(bool(f),"save");
 std::cout<<"REFINED_POSE_DIAGNOSTIC_PASS "<<quality<<"/"<<summary["cases"].size()<<" quality; integer-placement decomposition verified\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
