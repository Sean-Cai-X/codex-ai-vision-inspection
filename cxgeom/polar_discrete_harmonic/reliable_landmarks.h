#pragma once
#include <cmath>
#include <string>
#include <stdexcept>
namespace cxgeom::polar {
struct LandmarkEvidence {
 std::string source="UNVERIFIED",trace,image_sha256;
 double localization_error_px=-1;
};
inline std::string CheckLandmarkEvidence(const LandmarkEvidence& e,const std::string& image_sha,double maximum_error){
 if(!std::isfinite(maximum_error)||maximum_error<0)throw std::invalid_argument("LANDMARK_INVALID_ERROR_LIMIT");
 if(e.source!="MANUAL_CONFIRMED"&&e.source!="UPSTREAM_VERIFIED"&&e.source!="CONTROLLED_REFERENCE")
  return "LANDMARK_SOURCE_UNVERIFIED";
 if(e.trace.empty()||e.trace.size()>512)return "LANDMARK_TRACE_REQUIRED";
 if(e.image_sha256.size()!=64||e.image_sha256!=image_sha)return "LANDMARK_IMAGE_SHA_MISMATCH";
 if(!std::isfinite(e.localization_error_px)||e.localization_error_px<0)return "LANDMARK_LOCALIZATION_ERROR_UNKNOWN";
 if(e.localization_error_px>maximum_error)return "LANDMARK_LOCALIZATION_ERROR_EXCEEDED";
 return "LANDMARK_DECLARATION_BOUND_TO_IMAGE";
}
inline double PropagatedLandmarkError(double scale,double reference_error,double observation_error){
 if(!std::isfinite(scale)||scale<=0||!std::isfinite(reference_error)||reference_error<0||
    !std::isfinite(observation_error)||observation_error<0)throw std::invalid_argument("LANDMARK_INVALID_PROPAGATION");
 return scale*reference_error+observation_error;
}
}
