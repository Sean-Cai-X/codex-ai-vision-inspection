#include "../../cxgeom/polar_discrete_harmonic/boundary_arc_correspondence.h"
#include "../../libtorchsegmentation/src/utils/json.hpp"
#include <functional>
#include <iostream>
using namespace cxgeom::polar;
void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
ObservedBoundaryInput input(std::vector<std::complex<double>> p,const std::string& source){
 cxgeom::OpenBoundaryRun run;run.parent_point_count=p.size()+1;
 for(size_t i=0;i<p.size();++i){run.points.push_back(p[i]);run.parent_indices.push_back(i);}
 p.push_back({-9,-11});return {source,"v1","contour","segment",p,run};
}
void reject(const std::function<void()>& f,const char* why){
 try{f();}catch(const std::invalid_argument& e){check(std::string(e.what())==why,e.what());return;}
 throw std::runtime_error("expected arc rejection");
}
int main()try{
 std::vector<std::complex<double>> points={{0,0},{4,0},{4,3},{7,5},{9,4}};
 auto reference=input(points,"ref");auto before=reference.receipt();
 for(double angle:{0.,15.,37.,90.,-110.})for(double scale:{.5,1.,1.5,2.}){
  const auto factor=std::polar(scale,angle*std::acos(-1.)/180.);
  const std::complex<double> translation(20,-7);
  std::vector<std::complex<double>> dense;
  for(size_t i=1;i<points.size();++i)for(int k=0;k<7;++k)
   dense.push_back(factor*(points[i-1]+double(k)/7.*(points[i]-points[i-1]))+translation);
  dense.push_back(factor*points.back()+translation);
  auto observation=input(dense,"obs");
  auto result=MatchObservedArc(reference,observation);
  check(result.status=="UNIQUE_ARC_CANDIDATE","density invariant fit");
  const auto& pose=result.candidates.front();
  check(std::abs(pose.factor-factor)<1e-10&&std::abs(pose.translation-translation)<1e-10,"exact similarity");
  check(pose.reference.maximum_error_px<1e-10&&pose.observation.maximum_error_px<1e-10,"original vertex audit");
  auto json=nlohmann::json::parse(result.receipt());
  check(json["reference"]==nlohmann::json::parse(before),"original evidence");
  check(json["reference_sampling"]["samples"][0]["observed"]==false,"derived sample tag");
  for(const auto& sample:result.reference_samples.values){
   auto a=points[sample.edge],b=points[sample.edge+1];
   check(std::abs(sample.point-(a+sample.fraction*(b-a)))<1e-12,"source interval reconstruction");
  }
  check(result.receipt()==MatchObservedArc(reference,observation).receipt(),"deterministic replay");
 }
 auto reversed=points;std::reverse(reversed.begin(),reversed.end());
 ArcCorrespondenceConfig cfg;cfg.allow_reversed=true;
 auto reverse=MatchObservedArc(reference,input(reversed,"reverse"),cfg);
 check(reverse.accepted_indices.size()==1&&reverse.candidates[reverse.accepted_indices[0]].reversed,"explicit reverse");
 auto line=input({{0,0},{1,0},{2,0},{3,0}},"line");
 check(MatchObservedArc(line,line,cfg).status=="ARC_AMBIGUOUS","symmetric ambiguity retained");
 auto wrong=points;wrong[2]+=std::complex<double>(0.,3.);
 check(MatchObservedArc(reference,input(wrong,"wrong")).accepted_indices.empty(),"deformed rejection");
 auto cut=points;cut.erase(cut.begin(),cut.begin()+2);
 check(MatchObservedArc(reference,input(cut,"cut")).accepted_indices.empty(),"partial span not silently stretched");
 auto huge=points;for(auto& p:huge)p*=8;
 check(MatchObservedArc(reference,input(huge,"huge")).candidates[0].reason=="ARC_SCALE_OUT_OF_RANGE","scale guard");
 check(nlohmann::json::parse(MatchObservedArc(reference,input(huge,"huge")).receipt())["candidates"][0]["reference_observed"].is_null(),"unexecuted audit is null");
 ArcCorrespondenceConfig diagnostic;diagnostic.samples=3;diagnostic.maximum_fit_rms_px=10;diagnostic.maximum_endpoint_error_px=10;
 diagnostic.maximum_observed_error_px=.01;
 auto rawcheck=MatchObservedArc(reference,input(wrong,"deformed"),diagnostic);
 check(rawcheck.accepted_indices.empty()&&rawcheck.candidates[0].observation.maximum_error_px>.01,"raw points cannot hide behind sparse interpolation");
 cfg=ArcCorrespondenceConfig{};cfg.maximum_scale=std::numeric_limits<double>::infinity();
 reject([&]{MatchObservedArc(reference,reference,cfg);},"ARC_INVALID_CONFIG");
 auto sparse=reference.features(),denseFeatures=input({{0,0},{1,0},{2,0},{3,0},{4,0},{4,1},{4,2},{4,3},{5.5,4},{7,5},{8,4.5},{9,4}},"dense").features();
 PartialRequest request;request.request_id="density";request.reference_source="ref";request.observation_source="dense";
 request.reference=sparse;request.observation=denseFeatures;
 check(ValidatePartial(request).reason=="PARTIAL_COVERAGE_IMPOSSIBLE","legacy limitation preserved");
 cfg.samples=2;reject([&]{MatchObservedArc(reference,reference,cfg);},"ARC_INVALID_CONFIG");
 reject([&]{SampleObservedArc(reference,129);},"ARC_SAMPLE_BUDGET");
 check(reference.receipt()==before,"raw unchanged");
 std::cout<<"ARC_CORRESPONDENCE_PASS 20 transforms/density/provenance/raw_audit/reverse/ambiguity/partial_rejection\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
