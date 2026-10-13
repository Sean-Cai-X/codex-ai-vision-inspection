#pragma once
#include "endpoint_projected_arc.h"
namespace cxgeom::polar {
// Matching landmarks are independent of ROI/selection endpoints. Both must lie
// inside one retained observed run. No extrapolation, smoothing, or gap bridging.
struct MatchingArcLandmarks {
 std::complex<double> start{},end{};
 double maximum_distance_px=.25,tie_epsilon_px=1e-8;
 std::string provenance;
};
struct LandmarkArcProjection {
 EndpointProjectionCoordinates coordinates; // numeric adapter for existing slice sampler
 double distances[2]={0,0},arc_lengths[2]={0,0};
 size_t edges[2]={0,0};
 double edge_fractions[2]={0,0};
 std::string status;
};
inline LandmarkArcProjection ProjectMatchingLandmarks(
 const std::vector<std::complex<double>>& points,const MatchingArcLandmarks& anchors){
 auto lengths=common_arc_detail::RawFractions(points);
 const double total=arc_detail::SampleArcPoints(points,3).length;
 if(anchors.provenance.empty()||anchors.provenance.size()>512||
    !std::isfinite(std::abs(anchors.start))||!std::isfinite(std::abs(anchors.end))||
    !std::isfinite(anchors.maximum_distance_px)||anchors.maximum_distance_px<=0||
    !std::isfinite(anchors.tie_epsilon_px)||anchors.tie_epsilon_px<0)
  throw std::invalid_argument("MATCHING_ARC_LANDMARK_CONFIG");
 LandmarkArcProjection out;
 for(size_t k=0;k<2;++k){
  const auto p=k?anchors.end:anchors.start;
  double best=std::numeric_limits<double>::infinity();bool ambiguous=false;
  for(size_t i=0;i+1<points.size();++i){
   const auto delta=points[i+1]-points[i];const double d=std::norm(delta);
   if(d<=1e-18)throw std::invalid_argument("MATCHING_ARC_DEGENERATE_EDGE");
   const double f=std::clamp(std::real(std::conj(delta)*(p-points[i]))/d,0.,1.);
   const double distance=std::abs(points[i]+f*delta-p);
   const double s=total*(lengths[i]+f*(lengths[i+1]-lengths[i]));
   if(distance<best-anchors.tie_epsilon_px){
    best=distance;out.edges[k]=i;out.edge_fractions[k]=f;out.arc_lengths[k]=s;ambiguous=false;
   }else if(std::abs(distance-best)<=anchors.tie_epsilon_px&&
      std::abs(s-out.arc_lengths[k])>std::max(1e-8,anchors.tie_epsilon_px)){
    ambiguous=true;
   }
  }
  out.distances[k]=best;
  if(ambiguous){out.status="MATCHING_LANDMARK_AMBIGUOUS_PROJECTION";return out;}
  if(best>anchors.maximum_distance_px){out.status="MATCHING_LANDMARK_TOO_FAR";return out;}
  if(out.arc_lengths[k]<=1e-9||out.arc_lengths[k]>=total-1e-9){
   out.status="MATCHING_LANDMARK_OUTSIDE_INTERIOR_SUPPORT";return out;
  }
 }
 const double span=out.arc_lengths[1]-out.arc_lengths[0];
 if(std::abs(span)<=1e-9){out.status="MATCHING_LANDMARK_COLLAPSED_ARC";return out;}
 auto& c=out.coordinates;c.reversed=span<0;
 for(double f:lengths)c.values.push_back((f*total-out.arc_lengths[0])/span);
 c.minimum=std::min(c.values.front(),c.values.back());c.maximum=std::max(c.values.front(),c.values.back());
 c.maximum_normal_residual_px=std::max(out.distances[0],out.distances[1]);
 c.status="ENDPOINT_PROJECTION_AVAILABLE"; // sampler adapter, NOT straight-chord projection
 out.status="MATCHING_LANDMARK_ARC_AVAILABLE";return out;
}
}
