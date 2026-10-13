#pragma once
#include <complex>
#include <vector>
#include <string>
#include <stdexcept>
#include <cmath>
#include <algorithm>
namespace cxgeom {
struct CornerLineConfig {
 size_t minimum_points=5;
 double minimum_span_px=4,maximum_rms_px=.15,minimum_cross_sine=.25,maximum_extrapolation_px=8;
};
struct CornerLineFit {std::complex<double> center{},direction{};double rms=0,span=0;bool valid=false;};
struct CornerIntersection {
 bool accepted=false;std::string status;
 std::complex<double> point{};CornerLineFit left,right;
 double cross_sine=0,conditioning_gain=0,left_extrapolation=0,right_extrapolation=0;
 // This is a derived hypothesis, never a replacement observed boundary or calibrated uncertainty.
};
inline CornerLineFit FitCornerSupport(const std::vector<std::complex<double>>& p,const CornerLineConfig& c){
 CornerLineFit f;if(p.size()<c.minimum_points)return f;
 for(auto v:p){if(!std::isfinite(v.real())||!std::isfinite(v.imag()))return f;f.center+=v;}f.center/=double(p.size());
 double xx=0,yy=0,xy=0;for(auto v:p){v-=f.center;xx+=v.real()*v.real();yy+=v.imag()*v.imag();xy+=v.real()*v.imag();}
 double disc=std::hypot(xx-yy,2*xy);if(disc<=1e-12)return f;
 f.direction=std::polar(1.,.5*std::atan2(2*xy,xx-yy));double lo=1e100,hi=-1e100,sum=0;
 for(auto v:p){auto q=std::conj(f.direction)*(v-f.center);lo=std::min(lo,q.real());hi=std::max(hi,q.real());sum+=q.imag()*q.imag();}
 f.span=hi-lo;f.rms=std::sqrt(sum/p.size());f.valid=f.span>=c.minimum_span_px&&f.rms<=c.maximum_rms_px;return f;
}
inline CornerIntersection EstimateCornerIntersection(const std::vector<std::complex<double>>& left,
 const std::vector<std::complex<double>>& right,const CornerLineConfig& c={}){
 if(c.minimum_points<2||!std::isfinite(c.minimum_span_px)||c.minimum_span_px<=0||
 !std::isfinite(c.maximum_rms_px)||c.maximum_rms_px<0||!std::isfinite(c.minimum_cross_sine)||c.minimum_cross_sine<=0||c.minimum_cross_sine>1||
 !std::isfinite(c.maximum_extrapolation_px)||c.maximum_extrapolation_px<=0)throw std::invalid_argument("CORNER_CONFIG");
 CornerIntersection r;r.left=FitCornerSupport(left,c);r.right=FitCornerSupport(right,c);
 if(!r.left.valid||!r.right.valid){r.status="INSUFFICIENT_OR_NONLINEAR_SUPPORT";return r;}
 auto cross=[](auto a,auto b){return std::imag(std::conj(a)*b);};
 double det=cross(r.left.direction,r.right.direction);r.cross_sine=std::abs(det);
 if(r.cross_sine<c.minimum_cross_sine){r.status="ILL_CONDITIONED_INTERSECTION";return r;}
 r.conditioning_gain=1/r.cross_sine;
 r.point=r.left.center+cross(r.right.center-r.left.center,r.right.direction)/det*r.left.direction;
 auto outside=[](auto p,const auto& points,const auto& f){double lo=1e100,hi=-1e100;
  for(auto q:points){double t=std::real(std::conj(f.direction)*(q-f.center));lo=std::min(lo,t);hi=std::max(hi,t);}
  double t=std::real(std::conj(f.direction)*(p-f.center));return std::max({0.,lo-t,t-hi});};
 r.left_extrapolation=outside(r.point,left,r.left);r.right_extrapolation=outside(r.point,right,r.right);
 if(std::max(r.left_extrapolation,r.right_extrapolation)>c.maximum_extrapolation_px){r.status="EXTRAPOLATION_BUDGET";return r;}
 r.accepted=true;r.status="DERIVED_CORNER_HYPOTHESIS_NOT_OBSERVED";return r;
}
}
