#pragma once
#include "CxCommonRefinedArc.h"
#include "../cxgeom/polar_discrete_harmonic/landmark_arc_coordinates.h"
#include "CxLandmarkQualityParameters.h"
namespace cxpartialimage {
// Development branch: caller-supplied matching landmarks calibrate arc origin,
// scale and direction. This is not autonomous physical-landmark discovery.
inline RefinedArcComparison MatchLandmarkRefinedArc(const BoundRefinedSpan& reference,
 const BoundRefinedSpan& observation,const CommonRefinedArcConfig& config,
 const cxgeom::polar::MatchingArcLandmarks& reference_landmarks,
 const cxgeom::polar::MatchingArcLandmarks& observation_landmarks,
 const cxlandmarkquality::Config& quality_config={}){
 using namespace cxgeom::polar;using J=nlohmann::json;
 arc_detail::ValidateArcConfig(config.pose);
 cxlandmarkquality::Validate(quality_config);const auto& identity_config=quality_config.identity;
 if((quality_config.identity_required||quality_config.uncertainty_required)&&!config.reliable_landmarks_required)
  throw std::invalid_argument("LANDMARK_QUALITY_REQUIRES_SOURCE_BINDING");
 if(config.correspondence_id.empty()||config.correspondence_id.size()>512||config.pose.allow_reversed||config.endpoint_projection_enabled)
  throw std::invalid_argument("MATCHING_ARC_CORRESPONDENCE_CONFIG");
 const BoundRefinedSpan* spans[2]={&reference,&observation};
 const MatchingArcLandmarks* anchors[2]={&reference_landmarks,&observation_landmarks};
 LandmarkArcProjection p[2]={ProjectMatchingLandmarks(reference.refined_points(),reference_landmarks),
  ProjectMatchingLandmarks(observation.refined_points(),observation_landmarks)};
 auto domain=IntersectProjectedSupport(p[0].coordinates,p[1].coordinates,config.minimum_common_anchor_fraction);
 for(int i:{0,1})if(p[i].status!="MATCHING_LANDMARK_ARC_AVAILABLE"){domain.status=p[i].status;break;}
 if(config.reliable_landmarks_required&&(!std::isfinite(config.maximum_landmark_pose_error_px)||config.maximum_landmark_pose_error_px<=0))
  throw std::invalid_argument("LANDMARK_INVALID_POSE_ERROR_LIMIT");
 J evidence={{"schema","landmark_refined_arc_comparison.v1"},{"mode","explicit_matching_landmarks_refined_arclength"},
  {"production_eligible",false},{"whole_contour_approved",false},{"raw_modified",false},{"closed",false},
  {"gap_bridging",false},{"extrapolation",false},{"image_only_pose_recovery",false},
  {"anchor_correspondence_verified",false},{"uncertainty_calibrated",false},
  {"landmark_quality_parameters",cxlandmarkquality::Values(quality_config)},
  {"correspondence_id",config.correspondence_id},{"pose_constraint_source","caller_matching_landmarks_on_actual_retained_refined_curve"},
  {"common_support",{{"status",domain.status},{"anchor_interval",{domain.begin,domain.end}},
   {"minimum_anchor_fraction",config.minimum_common_anchor_fraction},{"retained_anchor_fraction",std::max(0.,domain.end-domain.begin)},
   {"coordinate_basis","continuous_refined_arclength_between_matching_landmarks_not_ROI_endpoints"}}},
  {"inputs",J::array()},{"full_raw_under_refined_pose",J::array()}};
 for(int i:{0,1}){
  auto owned=J::parse(spans[i]->receipt());const auto& a=*anchors[i];
  auto binding_status=config.reliable_landmarks_required?
   CheckLandmarkEvidence(config.landmarks[i],owned["binding"]["gray_sha256"],config.maximum_landmark_localization_error_px):"NOT_REQUIRED";
  evidence["inputs"].push_back({{"span",owned},{"source_ids",spans[i]->source_ids()},
   {"matching_landmarks",{{"start",{a.start.real(),a.start.imag()}},{"end",{a.end.real(),a.end.imag()}},
    {"provenance",a.provenance},{"maximum_distance_px",a.maximum_distance_px},{"tie_epsilon_px",a.tie_epsilon_px}}},
   {"projection",{{"status",p[i].status},{"distances_px",{p[i].distances[0],p[i].distances[1]}},
    {"arc_lengths_px",{p[i].arc_lengths[0],p[i].arc_lengths[1]}},{"source_edges",{spans[i]->source_begin()+p[i].edges[0],spans[i]->source_begin()+p[i].edges[1]}},
    {"edge_fractions",{p[i].edge_fractions[0],p[i].edge_fractions[1]}}}},
   {"landmark_gate",{{"status",binding_status},{"source",config.landmarks[i].source},{"trace",config.landmarks[i].trace},
    {"declared_image_sha256",config.landmarks[i].image_sha256},{"declared_error_px",config.landmarks[i].localization_error_px},
    {"calibrated",false},{"semantic_identity_automatically_verified",false}}}});
  if(binding_status!="NOT_REQUIRED"&&binding_status!="LANDMARK_DECLARATION_BOUND_TO_IMAGE"&&domain.accepted())
   domain.status=binding_status;
 }
 RefinedArcComparison out;out.refined.config=config.pose;out.refined.reference_receipt=reference.receipt();
 out.refined.observation_receipt=observation.receipt();
 evidence["pose_parameters"]=J::parse(out.refined.receipt()).at("config");
 evidence["landmark_gate"]={{"required",config.reliable_landmarks_required},
  {"maximum_localization_error_px",config.maximum_landmark_localization_error_px},
  {"maximum_propagated_pose_error_px",config.maximum_landmark_pose_error_px},{"localization_error_calibrated",false},
  {"semantic_identity_automatically_verified",false},{"sides",J::array()}};
 for(int i:{0,1}){
  evidence[i==0?"reference":"observation"]=evidence["inputs"][i];
  evidence["landmark_gate"]["sides"].push_back(evidence["inputs"][i]["landmark_gate"]);
 }
 out.matched_raw=out.refined;
 if(domain.accepted()){
  auto slice=[&](int i){return common_arc_detail::SampleProjectedSlice(spans[i]->source_raw().ordered_run().points,
   spans[i]->refined_points(),spans[i]->source_begin(),spans[i]->source_end(),p[i].coordinates,domain,config.pose.samples);};
  auto r=slice(0),o=slice(1);
  out.refined=arc_detail::MatchArcPoints(r.refined,o.refined,reference.receipt(),observation.receipt(),config.pose,&r.refined_samples,&o.refined_samples);
  out.matched_raw=arc_detail::MatchArcPoints(r.raw,o.raw,reference.receipt(),observation.receipt(),config.pose,&r.raw_samples,&o.raw_samples);
  const common_arc_detail::Slice* slices[2]={&r,&o};
  for(int i:{0,1}){
   J samples=J::array();
   for(const auto& v:slices[i]->refined_samples.values)
    samples.push_back({{"derived",true},{"observed_pixel",false},{"source_run_edge",{v.edge,v.edge+1}},
     {"source_ids",{spans[i]->source_raw().features().at(v.edge).stable_id,spans[i]->source_raw().features().at(v.edge+1).stable_id}},
     {"edge_fraction",v.fraction},{"common_interval_fraction",v.normalized_length},{"point",{v.point.real(),v.point.imag()}}});
   evidence["inputs"][i]["sampling"]=samples;evidence["inputs"][i]["retained_audit_vertex_count"]=slices[i]->refined.size();
  }
  out.refined.accepted_indices.clear();
  for(size_t i=0;i<out.refined.candidates.size();++i){
   auto& c=out.refined.candidates[i];
   if(std::abs(c.factor)<=1e-9){c.accepted=false;c.reason="ARC_DEGENERATE_FIT";}
   else if(config.reliable_landmarks_required){
    const double error=PropagatedLandmarkError(std::abs(c.factor),config.landmarks[0].localization_error_px,config.landmarks[1].localization_error_px);
    evidence["declared_propagated_errors_px"].push_back(error);
    if(error>config.maximum_landmark_pose_error_px){c.accepted=false;c.reason="LANDMARK_PROPAGATED_POSE_ERROR_EXCEEDED";}
   }
   // Configurable development gates never promote a candidate to production.
   if(std::abs(c.factor)>1e-9){
    J identity={{"candidate",i},{"semantic_identity_verified",false},{"diagnostic_only",!quality_config.identity_required},
     {"scope","held_out_context_search_within_each_retained_run_conditioned_on_fitted_scale"},
     {"parameters",{{"radius_px",identity_config.radius_px},{"step_px",identity_config.step_px},
      {"score_tolerance_px",identity_config.score_tolerance_px},{"maximum_context_error_px",identity_config.maximum_context_error_px},
      {"maximum_displacement_px",identity_config.maximum_displacement_px},
      {"samples",identity_config.samples},{"maximum_hypotheses",identity_config.maximum_hypotheses}}},
     {"queries",J::array()}};
    bool supported=true;
    for(int side:{0,1})for(int endpoint:{0,1}){
     const int other=1-side;
     int direction=(p[side].arc_lengths[1]>p[side].arc_lengths[0]?1:-1)*(endpoint?1:-1);
     int target_direction=(p[other].arc_lengths[1]>p[other].arc_lengths[0]?1:-1)*(endpoint?1:-1);
     auto check=SearchLandmarkIdentity(spans[side]->refined_points(),spans[other]->refined_points(),
      p[side].arc_lengths[endpoint],p[other].arc_lengths[endpoint],direction,target_direction,
      side?1/std::abs(c.factor):std::abs(c.factor),identity_config);
     supported=supported&&check.status=="GEOMETRIC_IDENTITY_SUPPORTED_WITHIN_SEARCH";
     identity["queries"].push_back({{"reference_side",side},{"endpoint",endpoint},{"status",check.status},
      {"best_arc_px",check.best_arc},{"best_score_px",check.best_score},
      {"plausible_arc_interval_px",{check.plausible_begin,check.plausible_end}},
      {"searched_arc_positions_px",check.arc_positions},{"scores_px",check.scores}});
    }
    identity["geometric_identity_supported_within_search"]=supported;
    evidence["identity_diagnostics"].push_back(identity);
    if(quality_config.identity_required&&!supported&&c.accepted){c.accepted=false;c.reason="LANDMARK_IDENTITY_NOT_CONFIRMED";}
    const double er=config.landmarks[0].localization_error_px,eo=config.landmarks[1].localization_error_px;
    J uncertainty={{"candidate",i},{"calibrated",false},{"diagnostic_only",!quality_config.uncertainty_required},
     {"identity_precondition_supported",supported},{"statistical_confidence_interval",false},
     {"assumptions","same physical endpoints; declared error disks valid; exact similarity model"},
     {"reference_declared_error_px",er},{"observation_declared_error_px",eo}};
    bool uncertainty_pass=false;
    if(er>=0&&eo>=0){
     const double effective_r=std::max(er,quality_config.error_floor_px),effective_o=std::max(eo,quality_config.error_floor_px);
     uncertainty["effective_reference_error_px"]=effective_r;uncertainty["effective_observation_error_px"]=effective_o;
     uncertainty["error_floor_is_policy_not_calibration"]=true;
     auto b=BoundLandmarkPose(reference_landmarks.start,reference_landmarks.end,
      observation_landmarks.start,observation_landmarks.end,c.factor,c.translation,effective_r,effective_o);
     uncertainty["status"]=b.status;uncertainty["baseline_px"]=b.baseline;
     uncertainty["angle_bound_rad"]=b.angle_rad;uncertainty["scale_absolute_bound"]=b.scale_absolute;
     uncertainty["translation_at_origin_bound_px"]=b.translation_px;uncertainty["center_position_bound_px"]=b.center_px;
     uncertainty["factor_disk_radius"]=b.factor_radius;
     uncertainty["zero_declared_error_is_not_calibration"]=(er==0||eo==0);
     uncertainty_pass=supported&&b.status=="CONDITIONAL_LANDMARK_DISK_BOUND"&&
      b.angle_rad*180/std::acos(-1.)<=quality_config.maximum_angle_deg&&b.scale_absolute<=quality_config.maximum_scale_error&&
      b.translation_px<=quality_config.maximum_translation_px&&b.center_px<=quality_config.maximum_center_px;
    }else uncertainty["status"]="LOCALIZATION_ERROR_UNDECLARED";
    uncertainty["conditional_gate_pass"]=uncertainty_pass;
    evidence["pose_uncertainty_diagnostics"].push_back(uncertainty);
    if(quality_config.uncertainty_required&&!uncertainty_pass&&c.accepted){c.accepted=false;c.reason="LANDMARK_UNCERTAINTY_NOT_ACCEPTABLE";}
   }
   if(c.accepted){c.reason="LANDMARK_REFINED_ARC_CONDITIONAL_NOT_PRODUCTION";out.refined.accepted_indices.push_back(i);}
   auto rp=reference.source_raw().ordered_run().points,op=observation.source_raw().ordered_run().points;
   if(p[0].coordinates.reversed)std::reverse(rp.begin(),rp.end());
   if(p[1].coordinates.reversed)std::reverse(op.begin(),op.end());
   if(std::abs(c.factor)>1e-9){
    for(auto& v:rp)v=c.factor*v+c.translation;
    const auto ra=AuditObservedPolyline(rp,op,config.pose.support_distance_px),oa=AuditObservedPolyline(op,rp,config.pose.support_distance_px);
    evidence["full_raw_under_refined_pose"].push_back({{"candidate",i},{"diagnostic_only",true},
     {"reference_maximum_error_px",ra.maximum_error_px},{"observation_maximum_error_px",oa.maximum_error_px},
     {"reference_weighted_rms_px",ra.weighted_rms_px},{"observation_weighted_rms_px",oa.weighted_rms_px},
     {"reference_monotonic",ra.monotonic},{"observation_monotonic",oa.monotonic}});
   }
  }
  out.refined.status=out.refined.accepted_indices.empty()?"NO_ARC_CANDIDATE":out.refined.accepted_indices.size()==1?"UNIQUE_ARC_CANDIDATE":"ARC_AMBIGUOUS";
 }else{out.refined.status=domain.status;out.matched_raw.status=domain.status;}
 evidence["status"]=out.refined.status;evidence["refined"]=J::parse(out.refined.receipt());
 evidence["matched_raw"]=J::parse(out.matched_raw.receipt());
 evidence["fallback"]=out.refined.accepted_indices.empty()?"RAW_REVIEW_REQUIRED_NO_AUTOMATIC_PROMOTION":"NONE";
 evidence["refined"]["coordinate_basis"]="derived_points_at_matching_landmark_refined_arclength";
 for(auto& candidate:evidence["refined"]["candidates"]){
  candidate["reference_refined"]=candidate["reference_observed"];candidate.erase("reference_observed");
  candidate["observation_refined"]=candidate["observation_observed"];candidate.erase("observation_observed");
 }
 out.evidence=evidence.dump(2);return out;
}
}
