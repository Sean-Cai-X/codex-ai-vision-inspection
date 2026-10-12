#include "../../cxgeom/polar_discrete_harmonic/observed_boundary_bundle.h"
#include "../../cxgeom/polar_discrete_harmonic/partial_pipeline_receipt.h"
#include "../../libtorchsegmentation/src/utils/json.hpp"
#include <functional>
#include <iostream>
using namespace cxgeom;using namespace cxgeom::polar;
void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
void reject(const std::function<void()>& f,const char* why){
 try{f();}catch(const std::invalid_argument& e){check(std::string(e.what())==why,e.what());return;}
 throw std::runtime_error("expected bundle rejection");
}
ObservedBoundaryInput segment(const std::vector<std::complex<double>>& p,size_t begin,size_t count,
 const std::string& id,const std::string& contour="c",const std::string& source="image",const std::string& revision="v"){
 OpenBoundaryRun r;r.parent_point_count=p.size();
 for(size_t k=0;k<count;++k){size_t i=(begin+k)%p.size();r.parent_indices.push_back(i);r.points.push_back(p[i]);}
 r.wraps_parent_origin=begin+count>p.size();
 return ObservedBoundaryInput(source,revision,contour,id,p,r);
}
int main()try{
 std::vector<std::complex<double>> p={{0,0},{3,0},{5,2},{4,5},{1,7},{-2,3}};
 auto a=segment(p,0,3,"a"),b=segment(p,2,3,"b");
 ObservedBoundaryBundle bundle({a,b});
 check(bundle.features().size()==5,"overlap deduplication");
 auto receipt=nlohmann::json::parse(bundle.receipt());
 check(receipt["deduplicated_occurrences"]==1&&receipt["source_point_occurrences"]==6,"dedup count");
 check(receipt["segments"].size()==2&&receipt["connect_segments"]==false,"no joining");
 for(size_t s=0;s<2;++s)for(size_t k=0;k<3;++k){
  const auto index=bundle.segment_indices()[s][k];
  check(bundle.features()[index].stable_id==bundle.segments()[s].features()[k].stable_id,"mapping");
  check(bundle.features()[index].point==bundle.segments()[s].features()[k].point,"mapping coordinates");
 }
 ObservedBoundaryBundle reordered({b,a});
 for(size_t i=0;i<5;++i)check(bundle.features()[i].stable_id==reordered.features()[i].stable_id,"canonical point order");
 // Coincident positions with distinct contour identities must NOT be deduplicated.
 ObservedBoundaryBundle coincident({a,segment(p,0,3,"a","different")});
 check(coincident.features().size()==6,"no coordinate dedup");
 reject([&]{ObservedBoundaryBundle x({a,a});},"BOUNDARY_BUNDLE_DUPLICATE_SEGMENT");
 reject([&]{ObservedBoundaryBundle x({a,segment(p,2,3,"b","c","other")});},"BOUNDARY_BUNDLE_MIXED_FRAME");
 reject([&]{ObservedBoundaryBundle x({a,segment(p,2,3,"b","c","image","v2")});},"BOUNDARY_BUNDLE_MIXED_FRAME");
 reject([&]{ObservedBoundaryBundle x({a,b},4);},"BOUNDARY_BUNDLE_FEATURE_BUDGET");
 reject([&]{ObservedBoundaryBundle x({});},"BOUNDARY_BUNDLE_SEGMENT_BUDGET");
 auto changed=p;changed[5]+=.1; // Conflict even outside both selected runs.
 reject([&]{ObservedBoundaryBundle x({a,segment(changed,2,3,"b")});},"BOUNDARY_BUNDLE_PARENT_CONFLICT");
 int cases=0;
 for(double angle:{-127.,37.,163.})for(double scale:{.7,1.2,1.8}){
  auto q=p;auto z=std::polar(scale,angle*std::acos(-1.)/180.);
  for(auto& v:q)v=z*v+std::complex<double>(17,-8);
  std::vector<std::complex<double>> extra={{300,200},{314,219},{329,203}};
  ObservedBoundaryBundle obs({segment(q,1,2,"one","target"),segment(q,3,2,"two","target"),segment(extra,0,2,"noise","extra")});
  PartialRequest request;request.request_id="bundle";request.reference_source="image";request.observation_source="image";
  request.reference=bundle.features();request.observation=obs.features();
  request.config.maximum_hypotheses=4096;request.config.max_residual_px=1e-7;
  auto r=SearchRefinedPartial(request);
  check(r.complete&&r.candidates.size()==1,"one bounded candidate");
  const auto& c=r.candidates.front();
  check(std::abs(c.supplied.angle_deg-angle)<1e-7&&std::abs(c.supplied.scale-scale)<1e-9,"pose");
  check(std::abs(c.supplied.translation-std::complex<double>(17,-8))<1e-8,"translation");
  check(c.observed_rms_px&&*c.observed_rms_px<1e-8,"observed residual");
  check(c.missing_reference_ids.size()==1&&c.extra_observation_ids.size()==2,"missing and extra");
  for(const auto& f:obs.segments().back().features())
   check(std::find(c.extra_observation_ids.begin(),c.extra_observation_ids.end(),f.stable_id)!=c.extra_observation_ids.end(),"noise provenance");
  auto reversed=obs.segments();std::reverse(reversed.begin(),reversed.end());
  request.observation=ObservedBoundaryBundle(reversed).features();
  auto again=SearchRefinedPartial(request);
  check(PartialPipelineReceiptV1(r)==PartialPipelineReceiptV1(again),"segment order invariance");
  request.config.maximum_hypotheses=1;
  auto stopped=SearchRefinedPartial(request);
  check(!stopped.complete&&stopped.candidates.empty(),"budget rejection");
  ++cases;
 }
 std::cout<<"BOUNDARY_BUNDLE_PASS cases="<<cases<<" overlap/missing/extra/frame/order; topology_not_verified\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
