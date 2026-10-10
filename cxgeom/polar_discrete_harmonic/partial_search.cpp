#include "partial_search.h"
#include "partial_refinement.h"
#include <algorithm>
namespace cxgeom::polar {
PartialSearchResult SearchPartial(const PartialRequest& request,const PartialSearchConfig& cfg){
 PartialSearchResult out;out.request_id=request.request_id;
 out.reference_source=request.reference_source;out.observation_source=request.observation_source;
 out.executed_assessment=request.config;out.executed_search=cfg;
 auto valid=ValidatePartial(request);
 if(!valid.ready_for_solver){out.status="INVALID_REQUEST";out.reason=valid.reason;return out;}
 auto positive=[](double v){return std::isfinite(v)&&v>0;};
 if(!positive(cfg.scale_min)||!positive(cfg.scale_max)||cfg.scale_min>cfg.scale_max||
    !std::isfinite(cfg.angle_min_deg)||!std::isfinite(cfg.angle_max_deg)||
    cfg.angle_min_deg< -180||cfg.angle_min_deg>180||cfg.angle_max_deg< -180||cfg.angle_max_deg>180||
    !positive(cfg.minimum_seed_length_px)||!positive(cfg.merge_angle_deg)||
    !positive(cfg.merge_scale)||!positive(cfg.merge_translation_px)||
    cfg.maximum_candidates==0||cfg.maximum_candidates>256){
  out.status="INVALID_SEARCH_CONFIG";out.reason="invalid_range_or_limit";return out;
 }
 auto a=Encode(request.reference_source,request.reference,request.config.encoding).features;
 auto b=Encode(request.observation_source,request.observation,request.config.encoding).features;
 auto angleAllowed=[&](double x){
  auto contains=[&](double v){return cfg.angle_min_deg<=cfg.angle_max_deg?
   v>=cfg.angle_min_deg&&v<=cfg.angle_max_deg:v>=cfg.angle_min_deg||v<=cfg.angle_max_deg;};
  return contains(x)||(std::abs(x)==180&&contains(-x));
 };
 auto fail=[&](const char* status){out.status=status;out.reason="bounded_search_incomplete";out.candidates.clear();};
 for(size_t i=0;i<a.size();++i)for(size_t j=i+1;j<a.size();++j)
 for(size_t u=0;u<b.size();++u)for(size_t v=0;v<b.size();++v){
  if(u==v)continue;
  if(out.attempted_seeds>=request.config.maximum_hypotheses){fail("SEED_BUDGET_EXHAUSTED");return out;}
  ++out.attempted_seeds;
  auto da=a[j].point-a[i].point,db=b[v].point-b[u].point;
  if(std::abs(da)<cfg.minimum_seed_length_px||std::abs(db)<cfg.minimum_seed_length_px)continue;
  auto factor=db/da;Hypothesis h;
  h.scale=std::abs(factor);h.angle_deg=std::arg(factor)*180/3.14159265358979323846;
  h.translation=b[u].point-factor*a[i].point;
  if(h.scale<cfg.scale_min||h.scale>cfg.scale_max||!angleAllowed(h.angle_deg))continue;
  h.source_id="pair:"+a[i].stable_id+":"+a[j].stable_id+"->"+b[u].stable_id+":"+b[v].stable_id;
  bool duplicate=false;
  for(const auto& existing:out.candidates){
   const auto& p=existing.supplied;
   if(std::abs(std::remainder(p.angle_deg-h.angle_deg,360))<=cfg.merge_angle_deg&&
      std::abs(p.scale-h.scale)<=cfg.merge_scale&&
      std::abs(p.translation-h.translation)<=cfg.merge_translation_px){
    out.deduplication.push_back({h.source_id,p.source_id});
    duplicate=true;break;
   }
  }
  if(duplicate)continue;
  if(out.pair_checks>=request.config.maximum_pair_checks){fail("PAIR_BUDGET_EXHAUSTED");return out;}
  auto bounded=request;bounded.config.maximum_pair_checks-=out.pair_checks;
  auto assessment=AssessHypothesis(bounded,h);
  ++out.assessed_hypotheses;out.pair_checks+=assessment.pair_checks;
  if(assessment.status=="PAIR_BUDGET_EXHAUSTED"){fail("PAIR_BUDGET_EXHAUSTED");return out;}
  if(!assessment.accepted&&assessment.status!="AMBIGUOUS_CORRESPONDENCE")continue;
  if(cfg.refine_before_capacity&&assessment.accepted){
   if(out.pair_checks>=request.config.maximum_pair_checks){fail("PAIR_BUDGET_EXHAUSTED");return out;}
   auto remaining=request;remaining.config.maximum_pair_checks-=out.pair_checks;
   auto fit=RefinePartial(remaining,assessment.supplied);
   out.pair_checks+=fit.pair_checks;
   out.online_refits.push_back({fit.before,fit.after,"keep_seed",fit.pair_checks});
   auto& trace=out.online_refits.back();
   if(fit.status=="PAIR_BUDGET_EXHAUSTED"||fit.before.status=="PAIR_BUDGET_EXHAUSTED"||
      fit.after.status=="PAIR_BUDGET_EXHAUSTED"){
    trace.decision="budget_exhausted";fail("PAIR_BUDGET_EXHAUSTED");return out;
   }
   if(fit.accepted&&fit.after.supplied.scale>=cfg.scale_min&&fit.after.supplied.scale<=cfg.scale_max&&
      angleAllowed(fit.after.supplied.angle_deg)){
    assessment=fit.after;trace.decision="refit_selected";
    bool equivalent=false;
    for(const auto& existing:out.candidates){
     if(!existing.accepted||existing.pairs.size()!=assessment.pairs.size())continue;
     bool same=true;
     for(size_t k=0;k<existing.pairs.size();++k)
      if(existing.pairs[k].reference_id!=assessment.pairs[k].reference_id||
         existing.pairs[k].target_id!=assessment.pairs[k].target_id){same=false;break;}
     const auto& p=existing.supplied;const auto& q=assessment.supplied;
     if(same&&std::abs(std::remainder(p.angle_deg-q.angle_deg,360))<=cfg.merge_angle_deg&&
        std::abs(p.scale-q.scale)<=cfg.merge_scale&&std::abs(p.translation-q.translation)<=cfg.merge_translation_px){
      trace.decision="equivalent_to:"+p.source_id;equivalent=true;break;
     }
    }
    if(equivalent)continue;
   }else if(fit.accepted)trace.decision="outside_window_keep_seed";
  }
  if(out.candidates.size()>=cfg.maximum_candidates){fail("CANDIDATE_CAPACITY_EXHAUSTED");return out;}
  out.candidates.push_back(std::move(assessment));
 }
 out.search_complete=true;
 std::sort(out.candidates.begin(),out.candidates.end(),[](const PartialAssessment& x,const PartialAssessment& y){
  if(x.pairs.size()!=y.pairs.size())return x.pairs.size()>y.pairs.size();
  if(x.observed_rms_px!=y.observed_rms_px)return x.observed_rms_px<y.observed_rms_px;
  return x.supplied.angle_deg<y.supplied.angle_deg;
 });
 out.status=out.candidates.empty()?"NO_VERIFIED_HYPOTHESIS":"PARTIAL_AUDIT_CANDIDATE";
 if(out.candidates.size()>1)out.status="AMBIGUOUS";
 for(const auto& c:out.candidates)if(!c.accepted)out.status="AMBIGUOUS";
 out.reason="finite_geometric_pair_seeds;not_polar_phase_search;no_production_admission";
 return out;
}
}
