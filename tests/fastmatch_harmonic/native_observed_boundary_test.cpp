#include "../../cxgeom/polar_discrete_harmonic/observed_boundary_input.h"
#include "../../cxgeom/polar_discrete_harmonic/partial_pipeline_receipt.h"
#include "../../libtorchsegmentation/src/utils/json.hpp"
#include <functional>
#include <iostream>
using namespace cxgeom;
using namespace cxgeom::polar;
void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
void reject(const std::function<void()>& f,const char* why){
 try{f();}catch(const std::invalid_argument& e){check(std::string(e.what())==why,e.what());return;}
 throw std::runtime_error("expected boundary refusal");
}
int main()try{
 std::vector<std::complex<double>> parent={{0,0},{3,0},{5,2},{4,5},{1,7},{-2,3}};
 auto run=ExtractOpenBoundaryRun(parent,0,0,6,8);
 ObservedBoundaryInput ref("image-r","rev1","c0","anchor0",parent,run);
 check(run.points.size()==5&&ref.features().size()==5,"no downsampling");
 auto j=nlohmann::json::parse(ref.receipt());
 check(j["closed"]==false&&j["solver_uses_order"]==false&&j["topology_verified"]==false,"no topology claim");
 check(j["ordered_points"][4]["parent_index"]==4&&j["synthetic_count"]==0,"ordered evidence");
 check(j["start_id"]==ref.features().front().stable_id&&j["end_id"]==ref.features().back().stable_id,"endpoints");
 ObservedBoundaryInput alternate("image-r","rev1","c0","other-anchor",parent,run);
 ObservedBoundaryInput revision("image-r","rev2","c0","anchor0",parent,run);
 check(alternate.features()[0].stable_id==ref.features()[0].stable_id,"point ID independent of anchor");
 check(revision.features()[0].stable_id!=ref.features()[0].stable_id,"revision isolation");
 reject([&]{ObservedBoundaryInput x("","v","c","s",parent,run);},"BOUNDARY_MISSING_PROVENANCE");
 reject([&]{ObservedBoundaryInput x("r","v","c","s",parent,run,4);},"BOUNDARY_FEATURE_BUDGET");
 auto bad=run;bad.points[0]+=.1;
 reject([&]{ObservedBoundaryInput x("r","v","c","s",parent,bad);},"BOUNDARY_POINT_PARENT_MISMATCH");
 bad=run;std::swap(bad.points[1],bad.points[2]);std::swap(bad.parent_indices[1],bad.parent_indices[2]);
 reject([&]{ObservedBoundaryInput x("r","v","c","s",parent,bad);},"BOUNDARY_ORDER_MISMATCH");
 bad=run;bad.wraps_parent_origin=true;
 reject([&]{ObservedBoundaryInput x("r","v","c","s",parent,bad);},"BOUNDARY_WRAP_MISMATCH");
 bad=run;bad.parent_indices[0]=parent.size();
 reject([&]{ObservedBoundaryInput x("r","v","c","s",parent,bad);},"BOUNDARY_INDEX_OUT_OF_RANGE");
 const std::vector<std::complex<double>> square={{0,0},{1,0},{2,0},{2,1},{2,2},{1,2},{0,2},{0,1}};
 auto wrap=ExtractOpenBoundaryRun(square,0,0,.5,1.5,2);
 ObservedBoundaryInput w("w","v","c","s",square,wrap);
 check(w.ordered_run().parent_indices==std::vector<size_t>({7,0}),"wrap order");
 check(nlohmann::json::parse(w.receipt())["wraps_parent_origin"]==true,"wrap evidence");
 // Controlled contours, not raster extraction: rotation/scale + missing endpoint.
 int cases=0;
 for(double angle:{-127.,37.,163.})for(double scale:{.7,1.2,1.8}){
  auto transformed=parent;const auto z=std::polar(scale,angle*std::acos(-1.)/180.);
  for(auto& p:transformed)p=z*p+std::complex<double>(17,-8);
  OpenBoundaryRun observed;observed.parent_point_count=parent.size();
  for(size_t i=1;i<5;++i){observed.parent_indices.push_back(i);observed.points.push_back(transformed[i]);}
  ObservedBoundaryInput obs("image-o","rev1","c1","s1",transformed,observed);
  PartialRequest request;request.request_id="boundary";request.reference_source="image-r";request.observation_source="image-o";
  request.reference=ref.features();request.observation=obs.features();
  request.config.maximum_hypotheses=4096;request.config.max_residual_px=1e-7;
  PartialSearchConfig cfg;cfg.scale_min=.5;cfg.scale_max=2;
  const auto result=SearchRefinedPartial(request,cfg);
  check(result.complete&&result.candidates.size()==1,"unique bounded point candidate");
  const auto& found=result.candidates[0];
  check(std::abs(found.supplied.angle_deg-angle)<1e-7&&std::abs(found.supplied.scale-scale)<1e-9,"pose");
  check(found.observed_rms_px.has_value()&&*found.observed_rms_px<1e-8,"observed error");
  check(std::abs(found.supplied.translation-std::complex<double>(17,-8))<1e-8,"translation");
  auto receipt=nlohmann::json::parse(PartialPipelineReceiptV1(result));
  check(receipt["production_eligible"]==false,"no production claim");
  check(receipt["raw_diagnostic"]["candidates"][0]["missing_reference_ids"].size()==1,"missing ID");
  request.config.maximum_hypotheses=1;
  const auto stopped=SearchRefinedPartial(request,cfg);
  check(!stopped.complete&&stopped.candidates.empty(),"no candidate on budget stop");
  ++cases;
 }
 std::cout<<"OBSERVED_BOUNDARY_INPUT_PASS cases="<<cases<<" provenance/order/endpoints/budget; topology_not_verified\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
