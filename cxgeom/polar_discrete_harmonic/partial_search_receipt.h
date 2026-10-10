#pragma once
#include "partial_search.h"
#include "receipt.h"
namespace cxgeom::polar {
// Audit receipt, not a signature, approval or exhaustive continuous pose proof.
inline std::string PartialSearchReceiptV1(const PartialSearchResult& r){
 const bool success=r.status=="PARTIAL_AUDIT_CANDIDATE"||r.status=="AMBIGUOUS";
 if(r.request_id.empty()||r.reference_source.empty()||r.observation_source.empty()||
    r.status.empty()||r.reason.empty()||r.production_eligible||
    r.generator!="deterministic_geometric_point_pairs"||
    r.assessed_hypotheses>r.attempted_seeds||r.deduplication.size()>r.attempted_seeds-r.assessed_hypotheses||
    (r.status=="PARTIAL_AUDIT_CANDIDATE"&&(r.candidates.size()!=1||!r.candidates.front().accepted))||
    (!r.search_complete&&!r.candidates.empty())||success!=!r.candidates.empty())
  throw std::invalid_argument("inconsistent_partial_search_receipt");
 std::ostringstream o;o.imbue(std::locale::classic());o<<std::setprecision(17);
 auto q=receipt_detail::quote;
 auto number=[&](double v){if(!std::isfinite(v))throw std::invalid_argument("nonfinite_receipt");o<<v;};
 auto field=[&](const char* name,double v){o<<','<<q(name)<<':';number(v);};
 auto ids=[&](const std::vector<std::string>& v){
  o<<'[';bool first=true;for(const auto& id:v){if(!first){o<<',';}first=false;o<<q(id);}o<<']';
 };
 o<<"{\"schema\":\"polar_partial_search_receipt.v1\",\"production_eligible\":false";
 o<<",\"request_id\":"<<q(r.request_id)<<",\"reference_source\":"<<q(r.reference_source);
 o<<",\"observation_source\":"<<q(r.observation_source)<<",\"generator\":"<<q(r.generator);
 o<<",\"status\":"<<q(r.status)<<",\"reason\":"<<q(r.reason);
 o<<",\"search_complete\":"<<(r.search_complete?"true":"false");
 o<<",\"search_scope\":\"finite_point_pair_seeds_not_continuous_global_optimum\"";
 o<<",\"attempted_seeds\":"<<r.attempted_seeds<<",\"assessed_hypotheses\":"<<r.assessed_hypotheses;
 o<<",\"pair_checks\":"<<r.pair_checks<<",\"deduplicated_seeds\":"<<r.deduplication.size();
 const auto& a=r.executed_assessment;const auto& s=r.executed_search;
 o<<",\"assessment_config\":{\"minimum_matches\":"<<a.minimum_matches;
 o<<",\"maximum_features\":"<<a.maximum_features<<",\"maximum_hypotheses\":"<<a.maximum_hypotheses;
 o<<",\"maximum_pair_checks\":"<<a.maximum_pair_checks;
 field("minimum_reference_coverage",a.minimum_reference_coverage);
 field("minimum_observation_coverage",a.minimum_observation_coverage);
 field("minimum_spatial_span",a.minimum_spatial_span);field("max_residual_px",a.max_residual_px);
 o<<",\"encoding_bins\":"<<a.encoding.bins<<",\"encoding_max_order\":"<<a.encoding.max_order;
 field("minimum_rms_radius",a.encoding.minimum_rms_radius);
 o<<"},\"search_config\":{\"maximum_candidates\":"<<s.maximum_candidates;
 field("scale_min",s.scale_min);field("scale_max",s.scale_max);
 field("angle_min_deg",s.angle_min_deg);field("angle_max_deg",s.angle_max_deg);
 field("minimum_seed_length_px",s.minimum_seed_length_px);
 field("merge_angle_deg",s.merge_angle_deg);field("merge_scale",s.merge_scale);
 field("merge_translation_px",s.merge_translation_px);
 o<<",\"refine_before_capacity\":"<<(s.refine_before_capacity?"true":"false");
 o<<"},\"online_refits\":[";
 bool firstRefit=true;
 for(const auto& trace:r.online_refits){
  if(!firstRefit){o<<',';}firstRefit=false;
  o<<"{\"seed\":"<<q(trace.before.supplied.source_id)<<",\"decision\":"<<q(trace.decision);
  o<<",\"pair_checks\":"<<trace.pair_checks<<",\"before_status\":"<<q(trace.before.status);
  o<<",\"after_status\":"<<q(trace.after.status);
  if(trace.before.observed_rms_px)field("before_rms_px",*trace.before.observed_rms_px);
  if(trace.after.observed_rms_px)field("after_rms_px",*trace.after.observed_rms_px);
  if(trace.after.assessment_complete){
   field("angle_deg",trace.after.supplied.angle_deg);field("scale",trace.after.supplied.scale);
   field("translation_x",trace.after.supplied.translation.real());field("translation_y",trace.after.supplied.translation.imag());
  }
  o<<'}';
 }
 o<<']';
 o<<",\"deduplication\":[";bool first=true;
 for(const auto& d:r.deduplication){
  if(!first){o<<',';}first=false;
  o<<"{\"suppressed_seed\":"<<q(d.suppressed_seed)<<",\"representative_seed\":"<<q(d.representative_seed)<<'}';
 }
 o<<"],\"candidates\":[";first=true;
 for(const auto& c:r.candidates){
  if(c.request_id!=r.request_id||c.supplied.source_id.empty()||
     (c.accepted?c.status!="HYPOTHESIS_VERIFIED":c.status!="AMBIGUOUS_CORRESPONDENCE"))
   throw std::invalid_argument("inconsistent_candidate_receipt");
  if(!c.assessment_complete||c.production_eligible||!c.reference_coverage||
     !c.observation_coverage||!c.reference_span||!c.observed_rms_px||!c.observed_max_px)
   throw std::invalid_argument("incomplete_candidate_receipt");
  if(!first){o<<',';}first=false;
  o<<"{\"seed_source\":"<<q(c.supplied.source_id)<<",\"status\":"<<q(c.status);
  o<<",\"accepted\":"<<(c.accepted?"true":"false");
  field("angle_deg",c.supplied.angle_deg);field("scale",c.supplied.scale);
  field("translation_x",c.supplied.translation.real());field("translation_y",c.supplied.translation.imag());
  field("reference_count_coverage",*c.reference_coverage);field("observation_count_coverage",*c.observation_coverage);
  field("reference_diameter_span",*c.reference_span);
  field("observed_rms_px",*c.observed_rms_px);field("observed_max_px",*c.observed_max_px);
  o<<",\"missing_reference_ids\":";ids(c.missing_reference_ids);
  o<<",\"extra_observation_ids\":";ids(c.extra_observation_ids);
  o<<",\"pairs\":[";bool firstPair=true;
  for(const auto& p:c.pairs){
   if(!firstPair){o<<',';}firstPair=false;
   o<<"{\"reference_id\":"<<q(p.reference_id)<<",\"target_id\":"<<q(p.target_id);
   field("residual_px",p.residual_px);o<<'}';
  }
  o<<"]}";
 }
 o<<"]}";return o.str();
}
}
