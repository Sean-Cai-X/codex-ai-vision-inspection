#pragma once
#include "observed_boundary_input.h"
#include <algorithm>
namespace cxgeom::polar {
struct BoundaryNormalConfig {
 size_t half_window=3;
 double minimum_side_span_px=2,maximum_turn_deg=30;
};
struct BoundaryNormalPoint {
 size_t support_begin=0,support_end=0;
 std::complex<double> normal{};
 double turn_deg=0;
 std::string source_id,status;
 bool accepted=false;
};
struct BoundaryNormalResult {
 BoundaryNormalConfig config;
 std::vector<BoundaryNormalPoint> points;
 std::string receipt()const{
  std::ostringstream o;o.imbue(std::locale::classic());o<<std::setprecision(17);
  o<<"{\"schema\":\"observed_boundary_normals.v1\",\"method\":\"two_sided_chord_turn_gate\",\"production_eligible\":false";
  o<<",\"normal_orientation\":\"left_of_ordered_tangent_not_inward_or_outward\",\"half_window\":"<<config.half_window;
  o<<",\"minimum_side_span_px\":"<<config.minimum_side_span_px<<",\"maximum_turn_deg\":"<<config.maximum_turn_deg<<",\"points\":[";
  for(size_t i=0;i<points.size();++i){
   if(i)o<<',';
   const auto& p=points[i];
   o<<"{\"source_id\":"<<receipt_detail::quote(p.source_id)<<",\"status\":"<<receipt_detail::quote(p.status);
   o<<",\"support_run_indices\":["<<p.support_begin<<','<<p.support_end<<"],\"turn_deg\":"<<p.turn_deg<<",\"normal\":";
   if(p.accepted)o<<'['<<p.normal.real()<<','<<p.normal.imag()<<']';else o<<"null";
   o<<'}';
  }o<<"]}";return o.str();
 }
};
inline BoundaryNormalResult EstimateBoundaryNormals(const ObservedBoundaryInput& raw,const BoundaryNormalConfig& cfg={}){
 if(cfg.half_window<1||cfg.half_window>32||!std::isfinite(cfg.minimum_side_span_px)||cfg.minimum_side_span_px<=0||
    !std::isfinite(cfg.maximum_turn_deg)||cfg.maximum_turn_deg<=0||cfg.maximum_turn_deg>=180)
  throw std::invalid_argument("BOUNDARY_NORMAL_CONFIG");
 BoundaryNormalResult out;out.config=cfg;const auto& points=raw.ordered_run().points;
 for(size_t i=0;i<points.size();++i){
  BoundaryNormalPoint p;p.source_id=raw.features()[i].stable_id;
  p.support_begin=i>cfg.half_window?i-cfg.half_window:0;p.support_end=std::min(points.size()-1,i+cfg.half_window);
  if(i<cfg.half_window||i+cfg.half_window>=points.size())p.status="INSUFFICIENT_TWO_SIDED_SUPPORT";
  else{
   auto left=points[i]-points[p.support_begin],right=points[p.support_end]-points[i];
   double a=std::abs(left),b=std::abs(right);
   if(!std::isfinite(a)||!std::isfinite(b)||a<cfg.minimum_side_span_px||b<cfg.minimum_side_span_px)p.status="INSUFFICIENT_SIDE_SPAN";
   else{
    auto l=left/a,r=right/b;
    p.turn_deg=std::acos(std::clamp(std::real(std::conj(l)*r),-1.,1.))*180/std::acos(-1.);
    if(p.turn_deg>cfg.maximum_turn_deg)p.status="CORNER_OR_HIGH_CURVATURE";
    else{
     auto tangent=l+r;p.normal=tangent/std::abs(tangent)*std::complex<double>(0,1);
     p.accepted=true;p.status="OBSERVED_NORMAL_ESTIMATE";
    }
   }
  }
  out.points.push_back(p);
 }
 return out;
}
}
