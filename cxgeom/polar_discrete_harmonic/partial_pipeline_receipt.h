#pragma once
#include "partial_pipeline.h"
#include "partial_search_receipt.h"
namespace cxgeom::polar {
inline std::string PartialPipelineReceiptV1(const PartialPipelineResult& r){
 if(r.production_eligible||r.status.empty()||r.reason.empty()||
    (!r.complete&&(!r.candidates.empty()||!r.candidate_sources.empty()))||
    r.candidates.size()!=r.candidate_sources.size()||r.refinements.size()!=r.decisions.size()||
    r.refinements.size()>r.raw.candidates.size()||r.pair_checks>r.raw.executed_assessment.maximum_pair_checks)
  throw std::invalid_argument("inconsistent_partial_pipeline");
 for(size_t i=0;i<r.refinements.size();++i){
  const auto& fit=r.refinements[i];
  if(fit.status!="ONLINE_REFIT_REUSED")continue;
  bool found=false;
  for(const auto& trace:r.raw.online_refits)
   if(trace.decision=="refit_selected"&&SamePartialAssessment(trace.before,fit.before)&&
      SamePartialAssessment(trace.after,fit.after)&&SamePartialAssessment(trace.after,r.raw.candidates[i]))found=true;
  if(!found||!fit.accepted||fit.pair_checks!=0||fit.production_eligible)
   throw std::invalid_argument("invalid_online_reuse_receipt");
 }
 size_t checks=r.raw.pair_checks;for(const auto& fit:r.refinements)checks+=fit.pair_checks;
 if(checks!=r.pair_checks||(r.complete&&r.refinements.size()!=r.raw.candidates.size()))
  throw std::invalid_argument("inconsistent_pipeline_accounting");
 const bool success=r.status=="PARTIAL_AUDIT_CANDIDATE"||r.status=="AMBIGUOUS";
 if(success!=!r.candidates.empty())throw std::invalid_argument("inconsistent_pipeline_status");
 if(r.complete&&!r.raw.search_complete)throw std::invalid_argument("incomplete_raw_search");
 for(size_t i=0;i<r.candidates.size();++i){
  const auto& c=r.candidates[i];
  if(!c.assessment_complete||c.production_eligible||!PartialPoseInWindow(c.supplied,r.raw.executed_search)||
     r.candidate_sources[i].empty())throw std::invalid_argument("invalid_final_candidate");
  const auto index=r.candidate_sources[i].front();
  if(index>=r.refinements.size())throw std::invalid_argument("invalid_final_source");
  const bool selected=r.decisions[index]=="refit_selected";
  const auto& expected=selected?r.refinements[index].after:r.raw.candidates[index];
  if((selected&&!r.refinements[index].accepted)||c.accepted!=expected.accepted||c.status!=expected.status||
     c.supplied.source_id!=expected.supplied.source_id||c.supplied.angle_deg!=expected.supplied.angle_deg||
     c.supplied.scale!=expected.supplied.scale||c.supplied.translation!=expected.supplied.translation||
     c.observed_rms_px!=expected.observed_rms_px||c.observed_max_px!=expected.observed_max_px||
     c.pairs.size()!=expected.pairs.size())
   throw std::invalid_argument("final_candidate_source_mismatch");
  for(size_t k=0;k<c.pairs.size();++k)
   if(c.pairs[k].reference_id!=expected.pairs[k].reference_id||c.pairs[k].target_id!=expected.pairs[k].target_id||
      c.pairs[k].residual_px!=expected.pairs[k].residual_px)
    throw std::invalid_argument("final_pair_source_mismatch");
 }
 if(r.status=="PARTIAL_AUDIT_CANDIDATE"&&(r.candidates.size()!=1||!r.candidates.front().accepted))
  throw std::invalid_argument("false_unique_candidate_status");
 std::ostringstream o;o.imbue(std::locale::classic());o<<std::setprecision(17);
 auto q=receipt_detail::quote;
 auto num=[&](double v){if(!std::isfinite(v))throw std::invalid_argument("nonfinite_pipeline");o<<v;};
 auto metric=[&](const char* key,const std::optional<double>& v){
  o<<','<<q(key)<<':';if(v)num(*v);else o<<"null";
 };
 auto assessment=[&](const PartialAssessment& a){
  o<<"{\"status\":"<<q(a.status)<<",\"accepted\":"<<(a.accepted?"true":"false");
  o<<",\"pose\":";
  if(a.assessment_complete){
   o<<"{\"source\":"<<q(a.supplied.source_id)<<",\"angle_deg\":";num(a.supplied.angle_deg);
   o<<",\"scale\":";num(a.supplied.scale);o<<",\"translation_x\":";num(a.supplied.translation.real());
   o<<",\"translation_y\":";num(a.supplied.translation.imag());o<<'}';
  }else o<<"null";
  metric("observed_rms_px",a.observed_rms_px);metric("observed_max_px",a.observed_max_px);
  o<<",\"pairs\":[";bool first=true;
  for(const auto& p:a.pairs){
   if(!first){o<<',';}first=false;
   o<<"{\"reference_id\":"<<q(p.reference_id)<<",\"target_id\":"<<q(p.target_id)<<",\"residual_px\":";
   num(p.residual_px);o<<'}';
  }
  o<<"]}";
 };
 o<<"{\"schema\":\"polar_partial_pipeline_receipt.v1\",\"production_eligible\":false";
 o<<",\"status\":"<<q(r.status)<<",\"reason\":"<<q(r.reason);
 o<<",\"complete\":"<<(r.complete?"true":"false")<<",\"total_pair_checks\":"<<r.pair_checks;
 o<<",\"raw_diagnostic\":"<<PartialSearchReceiptV1(r.raw)<<",\"refinements\":[";
 for(size_t i=0;i<r.refinements.size();++i){
  if(i){o<<',';}const auto& f=r.refinements[i];
  o<<"{\"raw_index\":"<<i<<",\"status\":"<<q(f.status)<<",\"reason\":"<<q(f.reason);
  o<<",\"decision\":"<<q(r.decisions[i])<<",\"pair_checks\":"<<f.pair_checks<<",\"before\":";
  assessment(f.before);o<<",\"after\":";assessment(f.after);o<<'}';
 }
 o<<"],\"candidates\":[";
 std::vector<bool> used(r.raw.candidates.size(),false);
 for(size_t i=0;i<r.candidates.size();++i){
  if(i){o<<',';}o<<"{\"assessment\":";assessment(r.candidates[i]);o<<",\"raw_indices\":[";
  if(r.candidate_sources[i].empty())throw std::invalid_argument("missing_pipeline_provenance");
  for(size_t k=0;k<r.candidate_sources[i].size();++k){
   const auto index=r.candidate_sources[i][k];
   if(index>=used.size()||used[index])throw std::invalid_argument("invalid_pipeline_provenance");
   used[index]=true;if(k){o<<',';}o<<index;
  }
  o<<"]}";
 }
 if(r.complete)for(bool retained:used)if(!retained)throw std::invalid_argument("lost_pipeline_provenance");
 o<<"]}";return o.str();
}
}
