#pragma once
#include "partial_search.h"
namespace cxgeom::polar {
inline bool SamePartialAssessment(const PartialAssessment& a,const PartialAssessment& b){
 if(a.request_id!=b.request_id||a.status!=b.status||a.accepted!=b.accepted||
    a.assessment_complete!=b.assessment_complete||a.production_eligible!=b.production_eligible||
    a.supplied.source_id!=b.supplied.source_id||a.supplied.angle_deg!=b.supplied.angle_deg||
    a.supplied.scale!=b.supplied.scale||a.supplied.translation!=b.supplied.translation||
    a.reference_coverage!=b.reference_coverage||a.observation_coverage!=b.observation_coverage||
    a.reference_span!=b.reference_span||a.observed_rms_px!=b.observed_rms_px||
    a.observed_max_px!=b.observed_max_px||a.missing_reference_ids!=b.missing_reference_ids||
    a.extra_observation_ids!=b.extra_observation_ids||a.pairs.size()!=b.pairs.size())return false;
 for(size_t i=0;i<a.pairs.size();++i)
  if(a.pairs[i].reference_id!=b.pairs[i].reference_id||a.pairs[i].target_id!=b.pairs[i].target_id||
     a.pairs[i].residual_px!=b.pairs[i].residual_px)return false;
 return true;
}
inline bool EquivalentPartialFit(const PartialAssessment& a,const PartialAssessment& b,const PartialSearchConfig& cfg){
 if(!a.accepted||!b.accepted||a.pairs.size()!=b.pairs.size())return false;
 for(size_t i=0;i<a.pairs.size();++i)
  if(a.pairs[i].reference_id!=b.pairs[i].reference_id||a.pairs[i].target_id!=b.pairs[i].target_id)return false;
 return std::abs(std::remainder(a.supplied.angle_deg-b.supplied.angle_deg,360))<=cfg.merge_angle_deg&&
  std::abs(a.supplied.scale-b.supplied.scale)<=cfg.merge_scale&&
  std::abs(a.supplied.translation-b.supplied.translation)<=cfg.merge_translation_px;
}
inline void ValidateOnlineRefits(const PartialSearchResult& r){
 if((!r.executed_search.refine_before_capacity&&!r.online_refits.empty())||
    r.online_refits.size()>r.assessed_hypotheses)
  throw std::invalid_argument("invalid_online_refit_count");
 size_t checks=0;
 for(const auto& t:r.online_refits){
  if(t.pair_checks>r.pair_checks-checks)throw std::invalid_argument("online_refit_budget_mismatch");
  checks+=t.pair_checks;
  bool merged=t.decision.rfind("equivalent_to:",0)==0;
  if(t.decision!="refit_selected"&&!merged){
   if(t.decision!="keep_seed"&&t.decision!="outside_window_keep_seed"&&t.decision!="budget_exhausted")
    throw std::invalid_argument("unknown_online_decision");
   continue;
  }
  if(!t.before.accepted||!t.after.accepted||!t.before.assessment_complete||!t.after.assessment_complete||
     t.before.production_eligible||t.after.production_eligible||t.before.request_id!=r.request_id||
     t.after.request_id!=r.request_id||!t.before.observed_rms_px||!t.after.observed_rms_px||
     *t.after.observed_rms_px>*t.before.observed_rms_px||
     t.after.supplied.source_id!="least_squares:"+t.before.supplied.source_id||
     t.before.pairs.size()!=t.after.pairs.size())
   throw std::invalid_argument("invalid_online_fit_provenance");
  for(size_t i=0;i<t.before.pairs.size();++i)
   if(t.before.pairs[i].reference_id!=t.after.pairs[i].reference_id||
      t.before.pairs[i].target_id!=t.after.pairs[i].target_id)
    throw std::invalid_argument("online_fit_changed_correspondence");
  if(!r.search_complete)continue; // Candidates deliberately cleared on exhaustion.
  bool found=false;
  for(const auto& c:r.candidates){
   if(!merged&&SamePartialAssessment(c,t.after))found=true;
   if(merged&&c.supplied.source_id==t.decision.substr(14)&&EquivalentPartialFit(c,t.after,r.executed_search))found=true;
  }
  if(!found)throw std::invalid_argument("orphan_online_fit");
 }
}
}
