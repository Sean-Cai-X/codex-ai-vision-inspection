#pragma once
#include "CxRefinedBoundaryArc.h"
#include "../cxgeom/polar_discrete_harmonic/endpoint_projected_arc.h"
#include "../cxgeom/polar_discrete_harmonic/reliable_landmarks.h"
namespace cxpartialimage {
struct CommonRefinedArcConfig {
 cxgeom::polar::ArcCorrespondenceConfig pose;
 cxgeom::polar::RawArcAnchors reference,observation;
 double minimum_common_anchor_fraction=.25;
 bool endpoint_projection_enabled=false,reliable_landmarks_required=false;
 cxgeom::polar::LandmarkEvidence landmarks[2];
 double maximum_landmark_localization_error_px=.25,maximum_landmark_pose_error_px=.5;
 double maximum_projection_normal_residual_px=.3;
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
 EndpointProjectionCoordinates rpcoords,opcoords;
 if(config.endpoint_projection_enabled){
  if(!reference.endpoint_anchors()||!observation.endpoint_anchors())
   throw std::invalid_argument("ENDPOINT_PROJECTION_SOURCE_ANCHORS_REQUIRED");
  const auto& r=*reference.endpoint_anchors();const auto& o=*observation.endpoint_anchors();
  rpcoords=ProjectEndpointCoordinates(reference.refined_points(),r.start,r.end,config.maximum_projection_normal_residual_px);
  opcoords=ProjectEndpointCoordinates(observation.refined_points(),o.start,o.end,config.maximum_projection_normal_residual_px);
 }
 auto d=config.endpoint_projection_enabled?
  IntersectProjectedSupport(rpcoords,opcoords,config.minimum_common_anchor_fraction):
  IntersectCommonArcDomain(config.reference,config.observation,rs,os,config.minimum_common_anchor_fraction);
 const auto geometric_support_status=d.status;
 std::string landmark_status[2]={"NOT_REQUIRED","NOT_REQUIRED"};
 if(config.reliable_landmarks_required){
  if(!std::isfinite(config.maximum_landmark_pose_error_px)||config.maximum_landmark_pose_error_px<=0)
   throw std::invalid_argument("LANDMARK_INVALID_POSE_ERROR_LIMIT");
  landmark_status[0]=CheckLandmarkEvidence(config.landmarks[0],J::parse(reference.receipt())["binding"]["gray_sha256"],config.maximum_landmark_localization_error_px);
  landmark_status[1]=CheckLandmarkEvidence(config.landmarks[1],J::parse(observation.receipt())["binding"]["gray_sha256"],config.maximum_landmark_localization_error_px);
  for(int i:{0,1})if(landmark_status[i]!="LANDMARK_DECLARATION_BOUND_TO_IMAGE"){d.status=landmark_status[i];break;}
 }
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
 J evidence={{"schema","refined_boundary_arc_comparison.v1"},
  {"mode",config.endpoint_projection_enabled?"explicit_endpoint_projection":"explicit_common_raw_arc"},
  {"maximum_projection_normal_residual_px",config.maximum_projection_normal_residual_px},
  {"raw_fraction_anchors_used",!config.endpoint_projection_enabled},
  {"production_eligible",false},{"whole_contour_approved",false},{"raw_modified",false},
  {"uncertainty_calibrated",false},{"scope","conditional_on_caller_asserted_endpoint_correspondence"},
  {"pose_parameters",pose_parameters},{"correspondence_id",config.correspondence_id},{"anchor_correspondence_verified",false},
  {"automatic_overlap_discovery",false},{"independent_refined_length_normalization",false},
  {"common_support",{{"status",geometric_support_status},{"anchor_interval",{d.begin,d.end}},
    {"minimum_anchor_fraction",config.minimum_common_anchor_fraction},
    {"retained_anchor_fraction",std::max(0.,d.end-d.begin)},
    {"coverage_semantics","fraction_of_caller_anchor_interval_not_whole_shape_quality"}}},
  {"reference",input(reference,config.reference,rs)},{"observation",input(observation,config.observation,os)},
  {"full_raw_under_refined_pose",J::array()}};
 evidence["landmark_gate"]={{"required",config.reliable_landmarks_required},{"maximum_localization_error_px",config.maximum_landmark_localization_error_px},
  {"maximum_propagated_pose_error_px",config.maximum_landmark_pose_error_px},{"localization_error_calibrated",false},
  {"semantic_identity_automatically_verified",false},{"sides",J::array()}};
 for(int i:{0,1})evidence["landmark_gate"]["sides"].push_back({{"source",config.landmarks[i].source},{"trace",config.landmarks[i].trace},
  {"declared_image_sha256",config.landmarks[i].image_sha256},{"declared_localization_error_px",config.landmarks[i].localization_error_px},{"status",landmark_status[i]}});
 if(config.endpoint_projection_enabled){
  auto projection=[](const BoundRefinedSpan& span,const EndpointProjectionCoordinates& p){
   const auto& a=*span.endpoint_anchors();
   return J{{"source","immutable_owned_gray_snapshot_endpoint_selection"},{"provenance",a.provenance},
    {"start",{a.start.real(),a.start.imag()}},{"end",{a.end.real(),a.end.imag()}},{"via",{a.via.real(),a.via.imag()}},
    {"maximum_snap_px",a.maximum_snap_px},{"tie_epsilon_px",a.tie_epsilon_px},
    {"coordinates",p.values},{"status",p.status},{"parent_reversed",p.reversed},
    {"maximum_normal_residual_px",p.maximum_normal_residual_px},{"support_interval",{p.minimum,p.maximum}}};
  };
  evidence["reference"]["endpoint_projection"]=projection(reference,rpcoords);
  evidence["observation"]["endpoint_projection"]=projection(observation,opcoords);
  evidence["common_support"]["coordinate_basis"]="continuous_requested_endpoints_projected_on_retained_refined_vertices";
  evidence["raw_control_coordinate_source"]="same_refined_vertex_endpoint_projection_not_independent_raw_parameterization";
  evidence["scope"]="straight_support_conditional_on_caller_asserted_continuous_endpoint_correspondence";
  evidence["pose_constraint_source"]="caller_endpoint_pair_constrains_longitudinal_origin_scale_and_direction";
  evidence["image_evidence_role"]="gray_boundary_refinement_and_observed_support_consistency";
  evidence["image_only_pose_recovery"]=false;
 }
 auto empty=[&](const char* basis){return J{{"status",d.status},{"config",pose_parameters},{"coordinate_basis",basis},{"candidates",J::array()},{"accepted_indices",J::array()}};};
 if(!d.accepted()){
  out.refined.status=d.status;out.matched_raw.status=d.status;
  evidence["status"]=d.status;evidence["fallback"]="RAW_REVIEW_REQUIRED_NO_AUTOMATIC_PROMOTION";
  evidence["refined"]=empty("fit_not_executed_due_to_input_gate");evidence["matched_raw"]=empty("fit_not_executed_due_to_input_gate");
  out.evidence=evidence.dump(2);return out;
 }
 const auto r=config.endpoint_projection_enabled?
  common_arc_detail::SampleProjectedSlice(reference.source_raw().ordered_run().points,reference.refined_points(),
   reference.source_begin(),reference.source_end(),rpcoords,d,config.pose.samples):
  common_arc_detail::SampleCommonSlice(reference.source_raw().ordered_run().points,
   reference.refined_points(),reference.source_begin(),reference.source_end(),config.reference,d,config.pose.samples);
 const auto o=config.endpoint_projection_enabled?
  common_arc_detail::SampleProjectedSlice(observation.source_raw().ordered_run().points,observation.refined_points(),
   observation.source_begin(),observation.source_end(),opcoords,d,config.pose.samples):
  common_arc_detail::SampleCommonSlice(observation.source_raw().ordered_run().points,
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
 // Diagnostics use exactly the existing clipped audit vertices and supplied
 // correspondence samples. They do not refit, trim, or alter any gate.
 evidence["residual_diagnostics"]={{"schema","common_arc_residual_trace.v1"},
  {"diagnostic_only",true},{"used_for_acceptance",false},
  {"coordinate_frame","observation_image_pixels"},
  {"evaluation_semantics","same_fit_domain_not_independent_holdout"},
  {"source_trace","reference/observation sampling source_ids and source_run_edge"},
  {"candidates",J::array()}};
 auto trace_json=[](const std::vector<ArcVertexResidual>& trace){
  J rows=J::array();
  for(const auto& v:trace)rows.push_back({{"audit_vertex",v.vertex},{"endpoint",v.endpoint},
   {"point",{v.point.real(),v.point.imag()}},{"nearest",{v.nearest.real(),v.nearest.imag()}},
   {"target_edge",{v.target_edge,v.target_edge+1}},{"target_fraction",v.target_fraction},
   {"error_px",v.error_px},{"arc_weight_px",v.weight}});
  return rows;
 };
 for(size_t ci=0;ci<out.refined.candidates.size();++ci){
  const auto& c=out.refined.candidates[ci];
  J diagnostic={{"candidate",ci},{"fit_samples",J::array()}};
  if(std::abs(c.factor)<=1e-9){diagnostic["status"]="DEGENERATE_POSE_NOT_AUDITED";
   evidence["residual_diagnostics"]["candidates"].push_back(diagnostic);continue;}
  auto transformed=r.refined;for(auto& p:transformed)p=c.factor*p+c.translation;
  std::vector<ArcVertexResidual> rt,ot;
  AuditObservedPolyline(transformed,o.refined,config.pose.support_distance_px,&rt);
  AuditObservedPolyline(o.refined,transformed,config.pose.support_distance_px,&ot);
  diagnostic["status"]="TRACED_EXISTING_FIT_AND_AUDIT_DOMAIN";
  diagnostic["reference_audit"]=trace_json(rt);diagnostic["observation_audit"]=trace_json(ot);
  for(size_t i=0;i<r.refined_samples.values.size();++i){
   const auto& a=r.refined_samples.values[i];const auto& b=o.refined_samples.values[i];
   const auto mapped=c.factor*a.point+c.translation;
   diagnostic["fit_samples"].push_back({{"sampling_index",i},{"derived",true},{"observed_pixel",false},
    {"common_interval_fraction",a.normalized_length},{"reference_point",{a.point.real(),a.point.imag()}},
    {"mapped_reference",{mapped.real(),mapped.imag()}},{"observation_point",{b.point.real(),b.point.imag()}},
    {"error_px",std::abs(mapped-b.point)}});
  }
  evidence["residual_diagnostics"]["candidates"].push_back(diagnostic);
 }
 if(config.reliable_landmarks_required){
  out.refined.accepted_indices.clear();
  for(size_t i=0;i<out.refined.candidates.size();++i){
   auto& c=out.refined.candidates[i];if(std::abs(c.factor)<=1e-9){c.accepted=false;c.reason="ARC_DEGENERATE_FIT";continue;}const double error=PropagatedLandmarkError(std::abs(c.factor),config.landmarks[0].localization_error_px,config.landmarks[1].localization_error_px);
   evidence["landmark_gate"]["candidate_errors"].push_back({{"candidate",i},{"declared_propagated_error_px",error}});
   if(error>config.maximum_landmark_pose_error_px){c.accepted=false;c.reason="LANDMARK_PROPAGATED_POSE_ERROR_EXCEEDED";}
   if(c.accepted)out.refined.accepted_indices.push_back(i);
  }
  out.refined.status=out.refined.accepted_indices.empty()?"NO_ARC_CANDIDATE":out.refined.accepted_indices.size()==1?"UNIQUE_ARC_CANDIDATE":"ARC_AMBIGUOUS";
 }
 for(auto& c:out.refined.candidates)if(c.accepted)c.reason="COMMON_REFINED_ARC_CONDITIONAL_NOT_PRODUCTION";
 auto refined=J::parse(out.refined.receipt());
 refined["schema"]="refined_arc_pose_assessment.v1";
 refined["coordinate_basis"]=config.endpoint_projection_enabled?
  "derived_refined_points_at_explicit_continuous_endpoint_projection":"derived_refined_points_at_common_raw_arc_coordinates";
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
  if(config.endpoint_projection_enabled?rpcoords.reversed:config.reference.begin>config.reference.end)std::reverse(rp.begin(),rp.end());
  if(config.endpoint_projection_enabled?opcoords.reversed:config.observation.begin>config.observation.end)std::reverse(op.begin(),op.end());
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
