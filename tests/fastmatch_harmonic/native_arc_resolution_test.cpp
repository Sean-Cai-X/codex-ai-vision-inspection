#include "../../cximage/CxPartialBoundaryMaskInput.h"
#include "../../cxgeom/polar_discrete_harmonic/boundary_arc_correspondence.h"
#include "../../libtorchsegmentation/src/utils/json.hpp"
#include <opencv2/imgcodecs.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace cxpartialimage;using namespace cxgeom::polar;using Json=nlohmann::json;
void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
void save(const std::filesystem::path& p,const Json& j){std::ofstream f(p);f<<j.dump(2);f.close();check(bool(f),"evidence write");}
struct Scenario {const char* id;double angle,scale,phase,hint;bool deformed;};
cv::Mat raster(const std::vector<std::complex<double>>& vertices){
 cv::Mat m=cv::Mat::zeros(256,256,CV_8UC1);std::vector<cv::Point> points;
 for(auto p:vertices)points.emplace_back(cvRound(p.real()*256),cvRound(p.imag()*256));
 cv::fillPoly(m,std::vector<std::vector<cv::Point>>{points},cv::Scalar(255),cv::LINE_8,8);return m;
}
ObservedBoundaryInput exact(const std::vector<std::complex<double>>& points,const std::string& id){
 auto parent=points;parent.push_back({-10,-10});cxgeom::OpenBoundaryRun r;r.parent_point_count=parent.size();
 for(size_t i=0;i<points.size();++i){r.points.push_back(points[i]);r.parent_indices.push_back(i);}
 return {id,"v1","analytic","span",parent,r};
}
int main(int argc,char** argv)try{
 check(argc==2,"output directory");std::filesystem::path out(argv[1]);std::filesystem::create_directories(out);
 const Scenario scenarios[]={{"identity",0,1,0,0,false},{"rotate37",37,1,0,0,false},
 {"scale150",0,1.5,0,0,false},{"combined37_150",37,1.5,0,0,false},
 {"phase025",0,1,.25,0,false},{"phase050",0,1,.5,0,false},
 {"hint040",0,1,0,.4,false},{"hint120",0,1,0,1.2,false},
 {"deformation",0,1,0,0,true}};
 Json summary={{"schema","arc_resolution_diagnostic.v1"},{"production_eligible",false},
 {"fixture","independently_rasterized_asymmetric_polygon_fixed_point_8"},
 {"anchor_basis","oracle_endpoints_and_via_not_automatic_localization"},
 {"quality_limits",{{"angle_deg",1},{"scale_absolute",.05},{"translation_px",.5}}},
 {"cases",Json::array()},{"test_pass_means","diagnostic_contract_not_all_quality"}};
 size_t passed=0,identities=0,exact_controls=0;
 for(int resolution:{1,2,4}){
  auto base=std::vector<std::complex<double>>{{0,0},{8,0},{8,2},{3,2},{3,7},{0,7}};
  for(auto& p:base)p=double(resolution)*p+std::complex<double>(20,20);
  auto reference_pixels=raster(base);
  auto reference_path=out/("reference_r"+std::to_string(resolution)+".pgm");
  check(cv::imwrite(reference_path.string(),reference_pixels),"reference write");
  BoundaryEndpointAnchors anchor{base[0],base[2]+.6*(base[3]-base[2]),(base[1]+base[2])*.5,2,1e-8,"analytic_reference_span"};
  auto reference=FromBinaryMask(cv::imread(reference_path.string(),0),{0,0,256,256},"reference_r"+std::to_string(resolution),"v1","span",128,3,&anchor);
  for(const auto& scenario:scenarios){
   std::string id=std::string(scenario.id)+"_r"+std::to_string(resolution);
   auto factor=std::polar(scenario.scale,scenario.angle*std::acos(-1.)/180.);
   std::complex<double> translation(110+scenario.phase,90+scenario.phase);
   auto transform=[&](std::complex<double> p){return factor*p+translation;};
   auto target=base;for(auto& p:target)p=transform(p);
   if(scenario.deformed)target[2]+=std::complex<double>(0,2*resolution);
   auto pixels=raster(target);auto path=out/(id+".pgm");check(cv::imwrite(path.string(),pixels),"observation write");
   auto requested=anchor;requested.start=transform(anchor.start)+std::complex<double>(scenario.hint,0);
   requested.end=transform(anchor.end);requested.via=transform(anchor.via);requested.provenance="known_transform_with_explicit_hint_perturbation";
   Json entry={{"id",id},{"resolution",resolution},{"phase_px",scenario.phase},{"hint_dx_px",scenario.hint},
    {"deformed",scenario.deformed},{"quality_pass",false},{"reference",Json::parse(reference.receipt())},
    {"expected",{{"angle_deg",scenario.angle},{"scale",scenario.scale},{"tx",translation.real()},{"ty",translation.imag()}}},
    {"requested_anchors",{{"start",{requested.start.real(),requested.start.imag()}},{"end",{requested.end.real(),requested.end.imag()}},
     {"via",{requested.via.real(),requested.via.imag()}}}}};
   // Matched analytic span isolates rasterization from the same geometry/pose.
   auto analytic_reference=exact({base[0],base[1],base[2],anchor.end},"analytic_ref");
   auto analytic_target=exact({target[0],target[1],target[2],transform(anchor.end)},"analytic_target");
   auto analytic=MatchObservedArc(analytic_reference,analytic_target);
   entry["analytic_control"]=Json::parse(analytic.receipt());
   if(!scenario.deformed){
    check(analytic.accepted_indices.size()==1,"analytic control rejected");
    auto c=analytic.candidates[analytic.accepted_indices[0]];
    check(std::abs(c.factor-factor)<1e-10&&std::abs(c.translation-translation)<1e-9,"analytic control pose");++exact_controls;
   }
   try{
    auto observation=FromBinaryMask(cv::imread(path.string(),0),{0,0,256,256},id,"v1","span",128,3,&requested);
    entry["observation"]=Json::parse(observation.receipt());
    auto oracle_points=reference.boundary.ordered_run().points;
    for(auto& p:oracle_points)p=transform(p);
    const auto& observed_points=observation.boundary.ordered_run().points;
    auto oa=AuditObservedPolyline(oracle_points,observed_points,.3);
    auto ob=AuditObservedPolyline(observed_points,oracle_points,.3);
    auto length=[](const std::vector<std::complex<double>>& p){double sum=0;for(size_t i=1;i<p.size();++i)sum+=std::abs(p[i]-p[i-1]);return sum;};
    entry["oracle_pose_diagnostic"]={{"used_for_selection",false},{"reference_max_px",oa.maximum_error_px},{"observation_max_px",ob.maximum_error_px},
     {"reference_rms_px",oa.weighted_rms_px},{"observation_rms_px",ob.weighted_rms_px},
     {"endpoint_error_px",std::max(std::abs(oracle_points.front()-observed_points.front()),std::abs(oracle_points.back()-observed_points.back()))},
     {"raster_arc_length_ratio_error",length(observed_points)/length(oracle_points)-1.}};
    auto result=MatchObservedArc(reference.boundary,observation.boundary);
    check(result.receipt()==MatchObservedArc(reference.boundary,observation.boundary).receipt(),"replay");
    entry["arc"]=Json::parse(result.receipt());entry["status"]=result.status;
    entry["diagnostic_errors"]=Json::array();
    for(const auto& c:result.candidates){
     double ae=std::abs(std::remainder(std::arg(c.factor)*180/std::acos(-1.)-scenario.angle,360.));
     double se=std::abs(std::abs(c.factor)-scenario.scale),te=std::abs(c.translation-translation);
     entry["diagnostic_errors"].push_back({{"angle_deg",ae},{"scale_absolute",se},{"translation_px",te},{"within_limits",ae<=1&&se<=.05&&te<=.5}});
    }
    if(result.accepted_indices.size()==1)
     entry["quality_pass"]=entry["diagnostic_errors"][result.accepted_indices[0]]["within_limits"];
    entry["reason"]=result.candidates.empty()?"NO_ASSESSMENT":result.candidates.front().reason;
   }catch(const std::invalid_argument& e){
    const std::string reason=e.what();
    check(reason.find("BOUNDARY_")==0||reason.find("MASK_BOUNDARY_")==0||reason.find("ARC_")==0,"unexpected rejection");
    entry["status"]="INPUT_REJECTED";entry["reason"]=reason;
   }
   if(std::string(scenario.id)=="identity"){check(entry["quality_pass"].get<bool>(),"identity control");++identities;}
   if(entry["quality_pass"].get<bool>())++passed;
   entry["false_accept_deformation"]=scenario.deformed&&entry["status"]=="UNIQUE_ARC_CANDIDATE";
   check(!entry["false_accept_deformation"].get<bool>(),"deformation falsely accepted");
   if(std::string(scenario.id)=="scale150"&&resolution>=2)
    check(entry["quality_pass"].get<bool>(),"longer scale control regressed");
   save(out/(id+".json"),entry);
   summary["cases"].push_back({{"id",id},{"status",entry["status"]},{"reason",entry["reason"]},
    {"quality_pass",entry["quality_pass"]},{"false_accept_deformation",entry["false_accept_deformation"]}});
   std::cout<<id<<" "<<entry["status"]<<" "<<entry["reason"]<<" quality="<<entry["quality_pass"]<<'\n';
  }
 }
 summary["quality_pass_count"]=passed;summary["case_count"]=summary["cases"].size();summary["analytic_control_pass_count"]=exact_controls;
 summary["negative_deformation_count"]=3;summary["negative_deformation_false_accepts"]=0;
 check(identities==3&&exact_controls==24,"control counts");save(out/"matrix.json",summary);
 std::cout<<"ARC_RESOLUTION_CONTRACT_PASS quality="<<passed<<"/27; not production\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
