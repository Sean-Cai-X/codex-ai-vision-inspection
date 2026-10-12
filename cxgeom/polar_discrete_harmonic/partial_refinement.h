#pragma once
#include "partial_search.h"
#include <map>
namespace cxgeom::polar {
struct PartialRefinement {
 std::string status,reason;
 PartialAssessment before,after;
 size_t pair_checks=0;
 bool accepted=false,production_eligible=false;
};
// One deterministic unweighted least-squares similarity fit on verified pairs.
// Revalidates input and output. Never replaces input on failure or ambiguity.
inline PartialRefinement RefinePartial(const PartialRequest& request,const Hypothesis& seed){
 PartialRefinement out;
 out.before=AssessHypothesis(request,seed);out.pair_checks=out.before.pair_checks;
 if(!out.before.accepted){out.status="INPUT_NOT_VERIFIED";out.reason=out.before.status;return out;}
 std::map<std::string,std::complex<double>> refs,observations;
 for(const auto& f:request.reference)refs.emplace(f.stable_id,f.point);
 for(const auto& f:request.observation)observations.emplace(f.stable_id,f.point);
 const auto& pairs=out.before.pairs;
 const auto originA=refs.at(pairs.front().reference_id);
 const auto originB=observations.at(pairs.front().target_id);
 std::complex<double> meanA{},meanB{};
 for(const auto& p:pairs){meanA+=refs.at(p.reference_id)-originA;meanB+=observations.at(p.target_id)-originB;}
 meanA/=double(pairs.size());meanB/=double(pairs.size());
 double denominator=0;std::complex<double> numerator{};
 for(const auto& p:pairs){
  auto a=refs.at(p.reference_id)-originA-meanA,b=observations.at(p.target_id)-originB-meanB;
  denominator+=std::norm(a);numerator+=std::conj(a)*b;
 }
 if(!std::isfinite(denominator)||denominator<=0){
  out.status="DEGENERATE_FIT";out.reason="zero_or_invalid_matched_span";return out;
 }
 const auto factor=numerator/denominator;
 Hypothesis fitted;fitted.source_id="least_squares:"+seed.source_id;
 fitted.scale=std::abs(factor);fitted.angle_deg=std::arg(factor)*180/3.14159265358979323846;
 fitted.translation=originB+meanB-factor*(originA+meanA);
 if(out.pair_checks>=request.config.maximum_pair_checks){
  out.status="PAIR_BUDGET_EXHAUSTED";out.reason="no_budget_for_revalidation";return out;
 }
 auto bounded=request;bounded.config.maximum_pair_checks-=out.pair_checks;
 out.after=AssessHypothesis(bounded,fitted);out.pair_checks+=out.after.pair_checks;
 if(!out.after.accepted){out.status="REFIT_NOT_VERIFIED";out.reason=out.after.status;return out;}
 if(out.after.pairs.size()!=pairs.size()){
  out.status="CORRESPONDENCE_CHANGED";out.reason="no_silent_membership_change";return out;
 }
 for(size_t i=0;i<pairs.size();++i){
  if(out.after.pairs[i].reference_id!=pairs[i].reference_id||out.after.pairs[i].target_id!=pairs[i].target_id){
   out.status="CORRESPONDENCE_CHANGED";out.reason="no_silent_membership_change";return out;
  }
 }
 // No tolerance-based acceptance of a worse fit. Rejected fit remains diagnostic.
 if(*out.after.observed_rms_px>*out.before.observed_rms_px){
  out.status="NO_RMS_IMPROVEMENT";out.reason="retain_original_hypothesis";return out;
 }
 out.accepted=true;out.status="REFIT_VERIFIED";
 out.reason="same_correspondence_nonincreasing_rms;not_unique_pose_or_production_approval";
 return out;
}
}
