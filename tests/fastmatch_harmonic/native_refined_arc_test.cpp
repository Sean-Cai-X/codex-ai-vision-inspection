#include "../../cximage/CxRefinedBoundaryArc.h"
#include <opencv2/imgcodecs.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <type_traits>
using namespace cxpartialimage;using namespace cxgeom::polar;using J=nlohmann::json;
void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
void save(const std::filesystem::path& p,const J& j){std::ofstream f(p);f<<j.dump(2);f.close();check(bool(f),"write");}
cv::Mat line(std::complex<double> n,std::complex<double> center){
 cv::Mat gray(128,128,CV_8UC1);
 for(int y=0;y<128;++y)for(int x=0;x<128;++x)
  gray.at<unsigned char>(y,x)=cv::saturate_cast<unsigned char>(128+100*std::erf(std::real(std::conj(n)*(std::complex<double>(x,y)-center))/1.6));
 return gray;
}
int main(int argc,char** argv)try{
 static_assert(!std::is_convertible<BoundRefinedSpan,ObservedBoundaryInput>::value,"no refined as raw");
 check(argc==2,"output directory");std::filesystem::path out(argv[1]);std::filesystem::create_directories(out);
 const std::complex<double> center(40.3,40.1),t(0,1);
 auto gray=line({1,0},center);
 BoundaryEndpointAnchors anchor{center-12.*t,center+12.*t,center,2.,1e-8,"analytic_test_endpoints_not_automatic_localization"};
 auto frame=BoundGrayFrame::Capture(gray,"ref","v1",128,{0,0,128,128},&anchor);
 auto spans=frame.refined_spans();check(spans.size()==1,"reference span");
 const auto before=frame.raw().receipt(),span_before=spans[0].receipt();
 J summary={{"schema","refined_arc_matrix.v1"},{"production_eligible",false},
  {"fixture","independent_analytic_gray_halfplanes_not_general_contour_matching"},
  {"anchors","oracle_transformed_endpoints_not_automatic_localization"},
  {"selection","single_contiguous_span_required_no_best_span_search"},
  {"test_pass_means","provenance_and_diagnostic_contract_not_all_pose_quality"},
  {"quality_limits",{{"angle_deg",1.},{"scale_absolute",.05},{"translation_px",.5}}},
  {"cases",J::array()}};
 int quality=0,rawquality=0;
 for(double angle:{0.,37.,90.,-60.})for(double scale:{.75,1.,1.5}){
  const auto factor=std::polar(scale,angle*std::acos(-1.)/180.);
  const std::complex<double> target_center(70.3,65.1),translation=target_center-factor*center;
  auto n=std::polar(1.,angle*std::acos(-1.)/180.);
  std::string id="a"+std::to_string(angle)+"_s"+std::to_string(scale);
  auto target=line(n,target_center);
  BoundaryEndpointAnchors requested{factor*anchor.start+translation,factor*anchor.end+translation,
   target_center,2.,1e-8,"oracle_endpoints"};
  auto observed=BoundGrayFrame::Capture(target,id,"v1",128,{0,0,128,128},&requested);
  auto os=observed.refined_spans();check(os.size()==1,"single observation span");
  auto comparison=MatchRefinedArc(spans[0],os[0]);
  auto j=J::parse(comparison.receipt());
  check(j["whole_contour_approved"]==false&&j["raw_modified"]==false,"scope guard");
  check(j["reference"]["excluded_point_count"].get<size_t>()>0,"excluded points retained in evidence");
  check(j["reference"]["retained_raw_length_fraction"].get<double>()<1,"coverage shrink explicit");
  check(j["reference"]["start_id"]==spans[0].source_ids().front(),"endpoint identity");
  check(j["observation"]["end_id"]==os[0].source_ids().back(),"endpoint identity");
  for(const auto& s:j["reference"]["sampling"]){
   const auto edge=s["source_run_edge"].get<std::vector<size_t>>();
   check(edge[1]==edge[0]+1&&edge[0]>=spans[0].source_begin()&&edge[1]<=spans[0].source_end(),"no bridging");
   check(s["source_ids"][0]==frame.raw().features()[edge[0]].stable_id,"sample provenance");
   check(s["observed_pixel"]==false&&s["derived"]==true,"derived not observed");
   const size_t k=edge[0]-spans[0].source_begin();const double f=s["fraction"].get<double>();
   const auto q=spans[0].refined_points()[k]+f*(spans[0].refined_points()[k+1]-spans[0].refined_points()[k]);
   check(std::abs(q-std::complex<double>(s["point"][0].get<double>(),s["point"][1].get<double>()))<1e-12,"sample reconstructs from retained source edge");
  }
  check(j["refined"]["candidates"][0].contains("reference_refined")&&!j["refined"]["candidates"][0].contains("reference_observed"),"refined audit not relabeled raw");
  check(j["full_raw_under_refined_pose"].size()==comparison.refined.candidates.size(),"all pose diagnostic");
  auto metric=[&](const ArcCorrespondenceResult& r){
   const auto& c=r.candidates.front();
   double ae=std::abs(std::remainder(std::arg(c.factor)*180/std::acos(-1.)-angle,360.));
   double se=std::abs(std::abs(c.factor)-scale),te=std::abs(c.translation-translation);
   bool pass=c.accepted&&ae<=1&&se<=.05&&te<=.5;
   return J{{"quality_pass",pass},{"angle_error_deg",ae},{"scale_error_absolute",se},{"translation_error_px",te},
    {"status",r.status},{"reason",c.reason}};
  };
  auto refined_metric=metric(comparison.refined),raw_metric=metric(comparison.matched_raw);
  if(!j["support_interval_aligned"].get<bool>()){
   check(comparison.refined.accepted_indices.empty(),"different support cannot silently rescale");
   check(j["fallback"]=="RAW_REVIEW_REQUIRED_NO_AUTOMATIC_PROMOTION","explicit fallback");
   check(j["full_raw_under_refined_pose"][0]["status"]=="FULL_RAW_AUDITED","failed fit retains full-raw diagnostic");
  }
  quality+=refined_metric["quality_pass"].get<bool>();rawquality+=raw_metric["quality_pass"].get<bool>();
  if(angle==0&&scale==1)check(refined_metric["quality_pass"].get<bool>(),"identity control");
  summary["cases"].push_back({{"id",id},{"angle_deg",angle},{"scale",scale},{"refined",refined_metric},{"matched_raw",raw_metric},
   {"support_aligned",j["support_interval_aligned"]},{"support_delta",j["support_endpoint_fraction_delta"]},{"reference_retained_fraction",j["reference"]["retained_raw_length_fraction"]},{"observation_retained_fraction",j["observation"]["retained_raw_length_fraction"]}});
  j["diagnostic_expected_pose"]={{"angle_deg",angle},{"scale",scale},{"translation",{translation.real(),translation.imag()}}};
  j["refined_quality"]=refined_metric;j["matched_raw_quality"]=raw_metric;
  save(out/(id+".json"),j);check(cv::imwrite((out/(id+".pgm")).string(),target),"image write");
  check(comparison.receipt()==MatchRefinedArc(spans[0],os[0]).receipt(),"deterministic replay");
 }
 cv::Mat corner(64,64,CV_8UC1);
 for(int y=0;y<64;++y)for(int x=0;x<64;++x)
  corner.at<unsigned char>(y,x)=cv::saturate_cast<unsigned char>(28+200*.5*(1+std::erf((32.-x)/1.6))*.5*(1+std::erf((y-20.)/1.6)));
 auto corner_frame=BoundGrayFrame::Capture(corner,"corner","v1",128,{19,17,17,18});
 auto cs=corner_frame.refined_spans();check(cs.size()==2,"corner separate spans");
 check(cs[0].source_end()+1<cs[1].source_begin(),"rejected gap preserved");
 for(const auto& s:cs){
  auto j=J::parse(MatchRefinedArc(s,s).receipt());
  check(j["reference"]["excluded_point_count"].get<size_t>()>0,"full source retained per subspan");
  check(j["reference"]["gap_bridging"]==false,"open span");
 }
 GrayBoundaryConfig reject;reject.minimum_contrast=250;
 check(frame.refined_spans({},reject).empty(),"all rejected no fabricated input");
 RefinedArcConfig cfg;cfg.pose.minimum_scale=2;cfg.pose.maximum_scale=3;
 auto blocked=MatchRefinedArc(spans[0],spans[0],cfg);
 check(blocked.refined.accepted_indices.empty()&&blocked.refined.candidates[0].reason=="ARC_SCALE_OUT_OF_RANGE","scale guard");
 cfg={};cfg.pose.allow_reversed=true;
 check(MatchRefinedArc(spans[0],spans[0],cfg).refined.status=="ARC_AMBIGUOUS","line reversal ambiguity explicit");
 check(frame.raw().receipt()==before&&spans[0].receipt()==span_before,"raw and span immutable");
 summary["refined_quality_pass"]=quality;summary["matched_raw_quality_pass"]=rawquality;
 summary["total"]=12;save(out/"matrix.json",summary);
 std::cout<<"REFINED_ARC_PASS 12 paired transforms; quality refined="<<quality<<"/12 matched_raw="<<rawquality<<"/12; gap/identity/coverage/scale/ambiguity guards\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
