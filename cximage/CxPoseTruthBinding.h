#pragma once
#include "CxPoseAcceptanceParameters.h"
#include <array>

// Pure scoring adapter: no image extraction, candidate selection or activation.
// Callers obtain bindings from actual decoded inputs, not from the truth record.
namespace cxposetruth {
using J = nlohmann::json;
struct Input {
  std::string case_id, dataset_revision, gray_sha256;
  bool operator==(const Input& b) const {
    return case_id==b.case_id && dataset_revision==b.dataset_revision && gray_sha256==b.gray_sha256;
  }
};
struct Binding {
  std::array<Input,2> sides; // reference, observation
  std::string coordinate_frame="image_pixels_x_right_y_down";
  std::string direction="reference_to_observation";
  std::string angle_convention="atan2_y_down_degrees";
};
struct Pose { double tx=0,ty=0,angle_deg=0,scale=1; };
struct Truth {
  Binding binding;
  Pose pose;
  std::string truth_id, revision, source_digest, provenance;
  std::array<bool,4> observable{{false,false,false,false}}; // tx,ty,angle,scale
};
inline bool Digest(const std::string& s) {
  return s.size()==64 && s.find_first_not_of("0123456789abcdef")==std::string::npos;
}
inline void Validate(const Binding& b) {
  if(b.coordinate_frame!="image_pixels_x_right_y_down" ||
     b.direction!="reference_to_observation" || b.angle_convention!="atan2_y_down_degrees")
    throw std::invalid_argument("POSE_TRUTH_UNSUPPORTED_COORDINATES");
  for(const auto& s:b.sides)
    if(s.case_id.empty()||s.dataset_revision.empty()||!Digest(s.gray_sha256))
      throw std::invalid_argument("POSE_TRUTH_INCOMPLETE_INPUT_BINDING");
}
inline void Validate(const Pose& p) {
  if(!std::isfinite(p.tx)||!std::isfinite(p.ty)||!std::isfinite(p.angle_deg)||
     !std::isfinite(p.scale)||p.scale<=0)
    throw std::invalid_argument("POSE_TRUTH_INVALID_TRANSFORM");
}
// Only controlled independent fixture truth is supported in this first adapter.
// A digest proves identity, not authenticity, metrological calibration or approval.
inline J EvaluateControlled(const Truth& truth,const Binding& actual,
    const std::vector<Pose>& accepted,const J& requested,
    std::optional<double> duration_ms,int duration_scope) {
  Validate(truth.binding);Validate(actual);Validate(truth.pose);
  if(truth.truth_id.empty()||truth.revision.empty()||truth.provenance.empty()||!Digest(truth.source_digest))
    throw std::invalid_argument("POSE_TRUTH_PROVENANCE_REQUIRED");
  if(truth.binding.sides!=actual.sides)
    throw std::invalid_argument("POSE_TRUTH_INPUT_BINDING_MISMATCH");
  const auto config=cxposeacceptance::Normalize(requested);
  if(config.at("pose_translation_unit")!=0)
    throw std::invalid_argument("POSE_TRUTH_PIXEL_ONLY_NO_PHYSICAL_CALIBRATION");
  if(duration_scope!=0 && duration_scope!=1)
    throw std::invalid_argument("POSE_TRUTH_DURATION_SCOPE");
  cxposeacceptance::Metrics metrics;
  metrics.elapsed_ms=duration_ms;metrics.latency_scope=duration_scope;
  std::string evidence=accepted.empty()?"NO_ACCEPTED_POSE":"AMBIGUOUS_POSE_NO_TRUTH_SELECTION";
  if(accepted.size()==1){
    const auto& pose=accepted.front();Validate(pose);
    metrics.independent_truth_verified=true;
    metrics.tx_observable=truth.observable[0];metrics.ty_observable=truth.observable[1];
    metrics.angle_observable=truth.observable[2];metrics.scale_observable=truth.observable[3];
    metrics.dx=pose.tx-truth.pose.tx;metrics.dy=pose.ty-truth.pose.ty;
    metrics.angle_error_deg=pose.angle_deg-truth.pose.angle_deg;
    metrics.scale_error_percent=100*std::abs(pose.scale/truth.pose.scale-1);
    evidence="CONTROLLED_FIXTURE_TRUTH_BOUND";
  }
  auto result=cxposeacceptance::Evaluate(config,metrics);
  result["evidence_status"]=evidence;
  result["truth_binding"]={{"truth_id",truth.truth_id},{"revision",truth.revision},
    {"source_digest",truth.source_digest},{"provenance",truth.provenance},
    {"scope","controlled_fixture_only_not_physical_calibration"},
    {"coordinate_frame",actual.coordinate_frame},{"direction",actual.direction},
    {"angle_convention",actual.angle_convention},{"observable_dofs",truth.observable},
    {"accepted_candidate_count",accepted.size()},{"sides",J::array()}};
  for(const auto& s:actual.sides) result["truth_binding"]["sides"].push_back(
    {{"case_id",s.case_id},{"dataset_revision",s.dataset_revision},{"gray_sha256",s.gray_sha256}});
  result["measured_duration_ms"]=duration_ms?J(*duration_ms):J(nullptr);
  result["duration_scope"]=duration_scope;
  return result;
}
}

