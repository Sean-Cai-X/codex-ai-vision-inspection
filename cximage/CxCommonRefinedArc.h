#pragma once
#include "CxRefinedBoundaryArc.h"
#include "../cxgeom/polar_discrete_harmonic/common_arc_support.h"
namespace cxpartialimage {
struct CommonRefinedArcConfig {
 cxgeom::polar::ArcCorrespondenceConfig pose;
 cxgeom::polar::RawArcAnchors reference,observation;
 double minimum_common_anchor_fraction=.25;
 std::string correspondence_id; // caller assertion, never proof of correspondence
};
inline RefinedArcComparison MatchCommonRefinedArc(const BoundRefinedSpan& reference,
 const BoundRefinedSpan& observation,const CommonRefinedArcConfig& config){
 using namespace cxgeom::polar;using J=nlohmann::json;
 arc_detail::ValidateArcConfig(config.pose);
 if(config.correspondence_id.empty()||config.correspondence_id.size()>512)
  throw std::invalid_argument("COMMON_ARC_CORRESPONDENCE_REQUIRED");
 if(config.pose.allow_reversed)throw std::invalid_argument("COMMON_ARC_ORIENTATION_ALREADY_ANCHORED");
 auto support=[](const BoundRefinedSpan& s){
  auto f=common_arc_detail::RawFractions(s.source_raw().ordered_run().points);
  return std::pair<double,double>{f.at(s.source_begin()),f.at(s.source_end())};
 };
 const auto rs=support(reference),os=support(observation);
 const auto d=IntersectCommonArcDomain(config.reference,config.observation,rs,os,config.minimum_common_anchor_fraction);
 RefinedArcComparison out;
 out.refined.config=config.pose;out.refined.reference_receipt=reference.receipt();out.refined.observation_receipt=observation.receipt();
 out.matched_raw=out.refined;
 const auto pose_parameters=J::parse(out.refined.receipt()).at("config");
 auto input=[](const BoundRefinedSpan& s,const RawArcAnchors& a,const std::pair<double,double>& interval){
  return J{{"span",J::parse(s.receipt())},{"anchors_raw_fractions",{a.begin,a.end}},
   {"retained_raw_fraction_interval",{interval.first,interval.second}},
   {"source_run_begin",s.source_begin()},{"source_run_end",s.source_end()},
   {"source_ids",s.source_ids()},{"full_raw_point_count",s.source_raw().ordered_run().points.size()},
   {"closed",false},{"gap_bridging",false}};
 };
 J evidence={{"schema","refined_boundary_arc_comparison.v1"},{"mode","explicit_common_raw_arc"},
  {"production_eligible",false},{"whole_contour_approved",false},{"raw_modified",false},
  {"uncertainty_calibrated",false},{"scope","conditional_on_caller_asserted_endpoint_correspondence"},
  {"pose_parameters",pose_parameters},{"correspondence_id",config.correspondence_id},{"anchor_correspondence_verified",false},
  {"automatic_overlap_discovery",false},{"independent_refined_length_normalization",false},
  {"common_support",{{"status",d.status},{"anchor_interval",{d.begin,d.end}},
    {"minimum_anchor_fraction",config.minimum_common_anchor_fraction},
    {"retained_anchor_fraction",std::max(0.,d.end-d.begin)},
    {"coverage_semantics","fraction_of_caller_anchor_interval_not_whole_shape_quality"}}},
  {"reference",input(reference,config.reference,rs)},{"observation",input(observation,config.observation,os)},
  {"full_raw_under_refined_pose",J::array()}};
 auto empty=[&](const char* basis){return J{{"status",d.status},{"config",pose_parameters},{"coordinate_basis",basis},{"candidates",J::array()},{"accepted_indices",J::array()}};};
 if(!d.accepted()){
  out.refined.status=d.status;out.matched_raw.status=d.status;
  evidence["status"]=d.status;evidence["fallback"]="RAW_REVIEW_REQUIRED_NO_AUTOMATIC_PROMOTION";
  evidence["refined"]=empty("no_common_refined_fit");evidence["matched_raw"]=empty("no_common_raw_fit");
  out.evidence=evidence.dump(2);return out;
 }
 const auto r=common_arc_detail::SampleCommonSlice(reference.source_raw().ordered_run().points,
  reference.refined_points(),reference.source_begin(),reference.source_end(),config.reference,d,config.pose.samples);
 const auto o=common_arc_detail::SampleCommonSlice(observation.source_raw().ordered_run().points,
  observation.refined_points(),observation.source_begin(),observation.source_end(),config.observation,d,config.pose.samples);
 out.refined=arc_detail::MatchArcPoints(r.refined,o.refined,reference.receipt(),observation.receipt(),config.pose,
  &r.refined_samples,&o.refined_samples);
 out.matched_raw=arc_detail::MatchArcPoints(r.raw,o.raw,reference.receipt(),observation.receipt(),config.pose,
  &r.raw_samples,&o.raw_samples);
 auto samples=[](const BoundRefinedSpan& s,const ArcSamples& values){
  J trace=J::array();const auto& raw=s.source_raw();const auto& run=raw.ordered_run();
  for(const auto& v:values.values)trace.push_back({{"derived",true},{"observed_pixel",false},
   {"source_run_edge",{v.edge,v.edge+1}},{"source_ids",{raw.features().at(v.edge).stable_id,raw.features().at(v.edge+1).stable_id}},
   {"parent_indices",{run.parent_indices.at(v.edge),run.parent_indices.at(v.edge+1)}},
   {"fraction",v.fraction},{"common_interval_fraction",v.normalized_length},{"point",{v.point.real(),v.point.imag()}}});
  return trace;
 };
 auto describe=[&](const char* key,const BoundRefinedSpan& span,const common_arc_detail::Slice& slice){
  evidence[key]["common_raw_fraction_interval"]={slice.source_begin_fraction,slice.source_end_fraction};
  evidence[key]["common_raw_length_px"]=slice.raw_samples.length;
  evidence[key]["common_refined_length_px"]=slice.refined_samples.length;
  evidence[key]["audit_vertex_count"]=slice.raw.size();
  evidence[key]["audit_semantics"]="clipped_derived_endpoints_plus_every_internal_vertex";
  evidence[key]["sampling"]=samples(span,slice.refined_samples);
  evidence[key]["raw_control_sampling"]=samples(span,slice.raw_samples);
 };
 describe("reference",reference,r);describe("observation",observation,o);
 for(auto& c:out.refined.candidates)if(c.accepted)c.reason="COMMON_REFINED_ARC_CONDITIONAL_NOT_PRODUCTION";
 auto refined=J::parse(out.refined.receipt());
 refined["schema"]="refined_arc_pose_assessment.v1";
 refined["coordinate_basis"]="derived_refined_points_at_common_raw_arc_coordinates";
 refined["sampling"]="shared_anchor_coordinate_not_independent_refined_length";
 for(auto& c:refined["candidates"]){
  c["reference_refined"]=c["reference_observed"];c.erase("reference_observed");
  c["observation_refined"]=c["observation_observed"];c.erase("observation_observed");
 }
 auto raw=J::parse(out.matched_raw.receipt());
 raw["coordinate_basis"]="raw_polyline_control_with_derived_clipped_endpoints";
 raw["sampling"]="same_common_anchor_coordinates_as_refined";
 auto audit=[](const ArcObservedAudit& a){return J{{"weighted_rms_px",a.weighted_rms_px},
  {"maximum_error_px",a.maximum_error_px},{"weighted_vertex_support",a.weighted_vertex_support},{"monotonic",a.monotonic}};};
 for(const auto& c:out.refined.candidates){
  auto rp=reference.source_raw().ordered_run().points,op=observation.source_raw().ordered_run().points;
  if(config.reference.begin>config.reference.end)std::reverse(rp.begin(),rp.end());
  if(config.observation.begin>config.observation.end)std::reverse(op.begin(),op.end());
  for(auto& p:rp)p=c.factor*p+c.translation;
  J a={{"diagnostic_only",true},{"used_for_common_fit_acceptance",false},
   {"reference",nullptr},{"observation",nullptr}};
  if(std::abs(c.factor)>1e-9){
   a["reference"]=audit(AuditObservedPolyline(rp,op,config.pose.support_distance_px));
   a["observation"]=audit(AuditObservedPolyline(op,rp,config.pose.support_distance_px));
   a["endpoint_error_px"]=std::max(std::abs(rp.front()-op.front()),std::abs(rp.back()-op.back()));
   a["status"]="FULL_RAW_AUDITED";
  }else a["status"]="DEGENERATE_POSE_NOT_AUDITED";
  evidence["full_raw_under_refined_pose"].push_back(a);
 }
 evidence["status"]=out.refined.status;evidence["refined"]=refined;evidence["matched_raw"]=raw;
 evidence["fallback"]=out.refined.accepted_indices.empty()?"RAW_REVIEW_REQUIRED_NO_AUTOMATIC_PROMOTION":"NONE";
 out.evidence=evidence.dump(2);return out;
}
}
