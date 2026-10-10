#pragma once
#include "partial_refinement.h"
namespace cxgeom::polar {
struct PartialPipelineResult {
 PartialSearchResult raw;
 std::vector<PartialRefinement> refinements;
 std::vector<std::string> decisions;
 std::vector<PartialAssessment> candidates;
 std::vector<std::vector<size_t>> candidate_sources; // Indices into raw.candidates/refinements.
 size_t pair_checks=0;
 std::string status,reason;
 bool complete=false,production_eligible=false;
};
inline bool PartialPoseInWindow(const Hypothesis& p,const PartialSearchConfig& cfg){
 auto contains=[&](double v){return cfg.angle_min_deg<=cfg.angle_max_deg?
  v>=cfg.angle_min_deg&&v<=cfg.angle_max_deg:v>=cfg.angle_min_deg||v<=cfg.angle_max_deg;};
 return std::isfinite(p.scale)&&std::isfinite(p.angle_deg)&&p.scale>=cfg.scale_min&&p.scale<=cfg.scale_max&&
  (contains(p.angle_deg)||(std::abs(p.angle_deg)==180&&contains(-p.angle_deg)));
}
// Shared distance budget across raw search and all refits. Raw is diagnostic;
// only candidates with complete=true are consumable. No global uniqueness claim.
inline PartialPipelineResult SearchRefinedPartial(const PartialRequest& request,const PartialSearchConfig& cfg={}){
 PartialPipelineResult out;out.raw=SearchPartial(request,cfg);out.pair_checks=out.raw.pair_checks;
 auto fail=[&](const std::string& status){
  out.status=status;out.reason="incomplete_pipeline_raw_is_diagnostic_only";
  out.candidates.clear();out.candidate_sources.clear();
 };
 if(!out.raw.search_complete){fail(out.raw.status);return out;}
 for(size_t i=0;i<out.raw.candidates.size();++i){
  const auto& raw=out.raw.candidates[i];PartialAssessment selected=raw;bool refined=false;
  if(out.pair_checks>=request.config.maximum_pair_checks){fail("PAIR_BUDGET_EXHAUSTED");return out;}
  auto bounded=request;bounded.config.maximum_pair_checks-=out.pair_checks;
  auto fit=RefinePartial(bounded,raw.supplied);out.pair_checks+=fit.pair_checks;
  out.refinements.push_back(fit);
  if(fit.status=="PAIR_BUDGET_EXHAUSTED"||fit.before.status=="PAIR_BUDGET_EXHAUSTED"||
     fit.after.status=="PAIR_BUDGET_EXHAUSTED"){
   out.decisions.push_back("budget_exhausted");fail("PAIR_BUDGET_EXHAUSTED");return out;
  }
  if(fit.accepted&&PartialPoseInWindow(fit.after.supplied,cfg)){
   selected=fit.after;refined=true;out.decisions.push_back("refit_selected");
  }else out.decisions.push_back(fit.accepted?"refit_outside_window_keep_raw":"refit_rejected_keep_raw");
  bool merged=false;
  // Merge only verified refits with identical pair IDs, never merely nearby poses.
  if(refined)for(size_t j=0;j<out.candidates.size();++j){
   const auto& previous=out.candidates[j];
   if(out.decisions[out.candidate_sources[j].front()]!="refit_selected")continue;
   if(previous.pairs.size()!=selected.pairs.size())continue;
   bool same=true;
   for(size_t k=0;k<selected.pairs.size();++k)
    if(previous.pairs[k].reference_id!=selected.pairs[k].reference_id||
       previous.pairs[k].target_id!=selected.pairs[k].target_id){same=false;break;}
   const auto& a=previous.supplied;const auto& b=selected.supplied;
   if(same&&std::abs(std::remainder(a.angle_deg-b.angle_deg,360))<=cfg.merge_angle_deg&&
      std::abs(a.scale-b.scale)<=cfg.merge_scale&&std::abs(a.translation-b.translation)<=cfg.merge_translation_px){
    out.candidate_sources[j].push_back(i);merged=true;break;
   }
  }
  if(!merged){out.candidates.push_back(selected);out.candidate_sources.push_back({i});}
 }
 out.complete=true;
 out.status=out.candidates.empty()?"NO_VERIFIED_HYPOTHESIS":"PARTIAL_AUDIT_CANDIDATE";
 if(out.candidates.size()>1)out.status="AMBIGUOUS";
 for(const auto& c:out.candidates)if(!c.accepted)out.status="AMBIGUOUS";
 out.reason="finite_seed_search_and_verified_refits;not_global_uniqueness_or_production";
 return out;
}
}
