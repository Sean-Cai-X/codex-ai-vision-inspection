#pragma once
#include "receipt.h"
#include <optional>
namespace cxgeom::polar {
struct PartialConfig {
 Config encoding;
 size_t minimum_matches=3,maximum_features=128;
 size_t maximum_hypotheses=256,maximum_pair_checks=1000000;
 double minimum_reference_coverage=.5,minimum_observation_coverage=.5;
 double minimum_spatial_span=.25,max_residual_px=.3;
};
struct PartialRequest {
 std::string request_id,reference_source,observation_source;
 std::vector<Feature> reference,observation;
 PartialConfig config;
};
struct PartialValidation {
 bool ready_for_solver=false;
 std::string reason;
};
// Future solver output: no default identity pose, no inferred coverage or unmatched IDs.
struct PartialCandidate {
 std::optional<PoseCandidate> pose;
 std::vector<std::string> missing_reference_ids,extra_observation_ids;
 std::optional<double> reference_coverage,observation_coverage,spatial_span;
};
struct PartialResult {
 std::string status="NOT_RUN",reason="partial_solver_not_implemented";
 std::vector<PartialCandidate> candidates;
 bool search_complete=false,production_eligible=false;
};
inline PartialValidation ValidatePartial(const PartialRequest& r){
 const auto& c=r.config;
 if(r.request_id.empty()||r.reference_source.empty()||r.observation_source.empty())
  return {false,"PARTIAL_MISSING_PROVENANCE"};
 if(c.minimum_matches<3||c.maximum_features<3||c.maximum_features>128||
    c.minimum_matches>c.maximum_features||c.maximum_hypotheses==0||
    c.maximum_hypotheses>65536||c.maximum_pair_checks==0)
  return {false,"PARTIAL_INVALID_BUDGET"};
 auto fraction=[](double x){return std::isfinite(x)&&x>0&&x<=1;};
 if(!fraction(c.minimum_reference_coverage)||!fraction(c.minimum_observation_coverage)||
    !fraction(c.minimum_spatial_span)||!std::isfinite(c.max_residual_px)||c.max_residual_px<=0)
  return {false,"PARTIAL_INVALID_THRESHOLDS"};
 if(r.reference.size()>c.maximum_features||r.observation.size()>c.maximum_features)
  return {false,"PARTIAL_FEATURE_BUDGET"};
 if(r.reference.size()<c.minimum_matches||r.observation.size()<c.minimum_matches)
  return {false,"PARTIAL_INSUFFICIENT_FEATURES"};
 const size_t possible=std::min(r.reference.size(),r.observation.size());
 if(double(possible)/r.reference.size()<c.minimum_reference_coverage||
    double(possible)/r.observation.size()<c.minimum_observation_coverage)
  return {false,"PARTIAL_COVERAGE_IMPOSSIBLE"};
 try{
  Encode(r.reference_source,r.reference,c.encoding);
  Encode(r.observation_source,r.observation,c.encoding);
 }catch(const std::invalid_argument& e){return {false,e.what()};}
 return {true,"PARTIAL_REQUEST_VALIDATED_NOT_SOLVED"};
}
// Request receipt only. Actual coverage, missing/extra IDs and poses remain unknown.
inline std::string PartialRequestReceiptV1(const PartialRequest& r){
 auto v=ValidatePartial(r);std::ostringstream o;o.imbue(std::locale::classic());
 o<<std::setprecision(17);
 o<<"{\"schema\":\"polar_partial_request_receipt.v1\",\"request_id\":"<<receipt_detail::quote(r.request_id);
 o<<",\"reference_source\":"<<receipt_detail::quote(r.reference_source);
 o<<",\"observation_source\":"<<receipt_detail::quote(r.observation_source);
 o<<",\"validation_reason\":"<<receipt_detail::quote(v.reason);
 o<<",\"ready_for_solver\":"<<(v.ready_for_solver?"true":"false");
 o<<",\"execution_status\":\"NOT_RUN\",\"production_eligible\":false";
 o<<",\"reference_count\":"<<r.reference.size()<<",\"observation_count\":"<<r.observation.size();
 o<<",\"pose\":null,\"coverage\":null,\"missing_reference_ids\":null,\"extra_observation_ids\":null";
 o<<",\"executed\":null,\"requested\":";
 // Invalid configurations are deliberately not serialized as JSON NaN/Infinity.
 if(!v.ready_for_solver)o<<"null";
 else {
  const auto& c=r.config;
  o<<"{\"minimum_matches\":"<<c.minimum_matches<<",\"maximum_features\":"<<c.maximum_features;
  o<<",\"maximum_hypotheses\":"<<c.maximum_hypotheses<<",\"maximum_pair_checks\":"<<c.maximum_pair_checks;
  o<<",\"minimum_reference_coverage\":"<<c.minimum_reference_coverage;
  o<<",\"minimum_observation_coverage\":"<<c.minimum_observation_coverage;
  o<<",\"minimum_spatial_span\":"<<c.minimum_spatial_span<<",\"max_residual_px\":"<<c.max_residual_px;
  o<<",\"bins\":"<<c.encoding.bins<<",\"max_order\":"<<c.encoding.max_order;
  o<<",\"minimum_rms_radius\":"<<c.encoding.minimum_rms_radius<<'}';
 }
 o<<'}';return o.str();
}
}
