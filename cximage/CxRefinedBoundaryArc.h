#pragma once
#include "CxBoundGrayFrame.h"
#include "../cxgeom/polar_discrete_harmonic/boundary_arc_correspondence.h"
#include "../libtorchsegmentation/src/utils/json.hpp"
namespace cxpartialimage {
// Explicitly selected, endpoint-aligned retained spans only. This does not find
// overlap, associate multiple spans, repair gaps or certify a whole contour.
struct RefinedArcConfig {
 cxgeom::polar::ArcCorrespondenceConfig pose;
 // Fraction of each ORIGINAL raw span, not pixels. Reject different retained
 // intervals instead of normalizing away endpoint loss and inventing scale.
 double maximum_support_endpoint_fraction_delta=.02;
};
struct RefinedArcComparison {
 cxgeom::polar::ArcCorrespondenceResult refined,matched_raw;
 std::string evidence;
 const std::string& receipt()const{return evidence;}
};
inline RefinedArcComparison MatchRefinedArc(const BoundRefinedSpan& reference,
 const BoundRefinedSpan& observation,const RefinedArcConfig& settings={}){
 using J=nlohmann::json;using namespace cxgeom::polar;
 const auto& config=settings.pose;
 if(!std::isfinite(settings.maximum_support_endpoint_fraction_delta)||
    settings.maximum_support_endpoint_fraction_delta<0||settings.maximum_support_endpoint_fraction_delta>1)
  throw std::invalid_argument("REFINED_ARC_INVALID_SUPPORT_TOLERANCE");
 // The only public input is the immutable span produced from an owned gray
 // frame. Never relabel its derived coordinates as ObservedBoundaryInput.
 RefinedArcComparison out;
 out.refined=arc_detail::MatchArcPoints(reference.refined_points(),observation.refined_points(),
  reference.receipt(),observation.receipt(),config);
 out.matched_raw=arc_detail::MatchArcPoints(reference.raw_points(),observation.raw_points(),
  reference.receipt(),observation.receipt(),config);
 auto length=[](const auto& points){double sum=0;for(size_t i=1;i<points.size();++i)sum+=std::abs(points[i]-points[i-1]);return sum;};
 auto support=[&](const BoundRefinedSpan& span){
  const auto& p=span.source_raw().ordered_run().points;
  double total=length(p),begin=0,end=0;
  for(size_t i=1;i<p.size();++i){
   const double d=std::abs(p[i]-p[i-1]);
   if(i<=span.source_begin())begin+=d;
   if(i<=span.source_end())end+=d;
  }
  return std::pair<double,double>{begin/total,end/total};
 };
 const auto rs=support(reference),os=support(observation);
 const double support_delta=std::max(std::abs(rs.first-os.first),std::abs(rs.second-os.second));
 bool support_aligned=false;J support_checks=J::array();out.refined.accepted_indices.clear();
 for(size_t i=0;i<out.refined.candidates.size();++i){
  auto& c=out.refined.candidates[i];
  const auto oriented=c.reversed?std::make_pair(1-os.second,1-os.first):os;
  const double delta=std::max(std::abs(rs.first-oriented.first),std::abs(rs.second-oriented.second));
  const bool aligned=delta<=settings.maximum_support_endpoint_fraction_delta;
  support_aligned=support_aligned||aligned;
  support_checks.push_back({{"reversed",c.reversed},{"endpoint_fraction_delta",delta},
   {"aligned",aligned},{"pose_reason_before_support_gate",c.reason}});
  if(!aligned){c.accepted=false;c.reason="REFINED_ARC_SUPPORT_INTERVAL_MISMATCH";}
  if(c.accepted){c.reason="ARC_REFINED_SPAN_VERIFIED_NOT_PRODUCTION";out.refined.accepted_indices.push_back(i);}
 }
 out.refined.status=!support_aligned?"REFINED_ARC_SUPPORT_INTERVAL_MISMATCH":
  out.refined.accepted_indices.empty()?"NO_ARC_CANDIDATE":out.refined.accepted_indices.size()==1?"UNIQUE_ARC_CANDIDATE":"ARC_AMBIGUOUS";
 auto input=[&](const BoundRefinedSpan& span,const ArcSamples& samples){
  const auto& raw=span.source_raw();const auto& run=raw.ordered_run();
  J trace=J::array();
  for(const auto& s:samples.values){
   const size_t a=span.source_begin()+s.edge,b=a+1;
   trace.push_back({{"derived",true},{"observed_pixel",false},{"source_run_edge",{a,b}},
    {"source_ids",{raw.features().at(a).stable_id,raw.features().at(b).stable_id}},
    {"parent_indices",{run.parent_indices.at(a),run.parent_indices.at(b)}},
    {"fraction",s.fraction},{"normalized_length",s.normalized_length},
    {"point",{s.point.real(),s.point.imag()}}});
  }
  const double full=length(run.points),retained=length(span.raw_points());const auto interval=support(span);
  return J{{"span",J::parse(span.receipt())},{"source_run_begin",span.source_begin()},
   {"source_run_end",span.source_end()},{"start_id",span.source_ids().front()},{"end_id",span.source_ids().back()},
   {"full_raw_point_count",run.points.size()},{"retained_point_count",span.raw_points().size()},
   {"excluded_point_count",run.points.size()-span.raw_points().size()},
   {"full_raw_length_px",full},{"retained_raw_length_px",retained},{"retained_raw_length_fraction",retained/full},
   {"source_arc_fraction_interval",{interval.first,interval.second}},{"refined_length_px",samples.length},{"closed",false},{"gap_bridging",false},{"sampling",trace}};
 };
 auto audit=[](const ArcObservedAudit& a){
  return J{{"weighted_rms_px",a.weighted_rms_px},{"maximum_error_px",a.maximum_error_px},
   {"weighted_vertex_support",a.weighted_vertex_support},{"monotonic",a.monotonic}};
 };
 // Audit EVERY original raw point under EACH refined pose, including excluded
 // endpoints and rejected gaps. Keep this diagnostic separate from span fit.
 J full=J::array();
 for(const auto& c:out.refined.candidates){
  auto rp=reference.source_raw().ordered_run().points,op=observation.source_raw().ordered_run().points;
  for(auto& p:rp)p=c.factor*p+c.translation;
  if(c.reversed)std::reverse(op.begin(),op.end());
  J a={{"diagnostic_only",true},{"used_for_span_acceptance",false},{"reference",nullptr},{"observation",nullptr}};
  if(std::abs(c.factor)>1e-9){
   a["reference"]=audit(AuditObservedPolyline(rp,op,config.support_distance_px));
   a["observation"]=audit(AuditObservedPolyline(op,rp,config.support_distance_px));
   a["endpoint_error_px"]=std::max(std::abs(rp.front()-op.front()),std::abs(rp.back()-op.back()));
   a["status"]="FULL_RAW_AUDITED";
  }else a["status"]="DEGENERATE_POSE_NOT_AUDITED";
  full.push_back(a);
 }
 auto refined=J::parse(out.refined.receipt());
 refined["schema"]="refined_arc_pose_assessment.v1";
 refined["coordinate_basis"]="gray_derived_vertices_not_raw_observations";
 for(auto& c:refined["candidates"]){
  c["reference_refined"]=c["reference_observed"];c.erase("reference_observed");
  c["observation_refined"]=c["observation_observed"];c.erase("observation_observed");
  if(c["accepted"]==true)c["reason"]="ARC_REFINED_SPAN_VERIFIED_NOT_PRODUCTION";
 }
 auto raw=J::parse(out.matched_raw.receipt());
 raw["coordinate_basis"]="original_raw_vertices_on_exact_same_retained_source_intervals";
 out.evidence=J({{"schema","refined_boundary_arc_comparison.v1"},{"production_eligible",false},
  {"whole_contour_approved",false},{"raw_modified",false},{"uncertainty_calibrated",false},
  {"scope","caller_selected_same_open_span_endpoint_alignment_required"},{"support_gate_proves_correspondence",false},
  {"coverage_semantics","retained_source_raw_arc_length_fraction_not_matching_quality"},
  {"status",out.refined.status},{"support_endpoint_fraction_delta",support_delta},
  {"maximum_support_endpoint_fraction_delta",settings.maximum_support_endpoint_fraction_delta},
  {"support_interval_aligned",support_aligned},{"candidate_support_checks",support_checks},
  {"fallback",support_aligned?"NONE":"RAW_REVIEW_REQUIRED_NO_AUTOMATIC_PROMOTION"},
  {"reference",input(reference,out.refined.reference_samples)},
  {"observation",input(observation,out.refined.observation_samples)},{"refined",refined},
  {"matched_raw",raw},{"full_raw_under_refined_pose",full}}).dump(2);
 return out;
}
}
