#pragma once
#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
namespace cxgeom::polar {
using LandmarkPoint=std::complex<double>;
struct LandmarkPoseBounds {
 std::string status="UNAVAILABLE";
 double baseline=0,factor_radius=0,angle_rad=0,scale_absolute=0,translation_px=0,center_px=0;
 bool calibrated=false; // deterministic conditional disk bound, NOT a statistical confidence interval
};
inline LandmarkPoseBounds BoundLandmarkPose(LandmarkPoint u0,LandmarkPoint u1,
 LandmarkPoint v0,LandmarkPoint v1,LandmarkPoint fitted_factor,LandmarkPoint fitted_translation,
 double reference_error,double observation_error){
 for(auto z:{u0,u1,v0,v1,fitted_factor,fitted_translation})
  if(!std::isfinite(std::abs(z)))throw std::invalid_argument("LANDMARK_NONFINITE_POSE");
 if(!std::isfinite(reference_error)||!std::isfinite(observation_error)||reference_error<0||observation_error<0)
  throw std::invalid_argument("LANDMARK_INVALID_DISK_BOUND");
 LandmarkPoseBounds out;out.baseline=std::abs(u1-u0);
 if(out.baseline<=2*reference_error+1e-12){out.status="LANDMARK_BASELINE_UNRESOLVED";return out;}
 auto a=(v1-v0)/(u1-u0),m=(u0+u1)*.5,n=(v0+v1)*.5,t=n-a*m;
 const double scale=std::abs(a);
 out.factor_radius=(2*observation_error+2*scale*reference_error)/(out.baseline-2*reference_error);
 out.scale_absolute=std::abs(std::abs(fitted_factor)-scale)+out.factor_radius;
 const double center_radius=observation_error+(scale+out.factor_radius)*reference_error;
 out.translation_px=std::abs(fitted_translation-t)+center_radius+out.factor_radius*std::abs(m);
 out.center_px=std::abs(fitted_factor*m+fitted_translation-n)+center_radius;
 if(scale<=out.factor_radius||std::abs(fitted_factor)<=1e-12){
  out.angle_rad=std::acos(-1.);out.status="LANDMARK_ANGLE_UNRESOLVED";return out;
 }
 out.angle_rad=std::abs(std::arg(fitted_factor/a))+std::asin(out.factor_radius/scale);
 out.angle_rad=std::min(std::acos(-1.),out.angle_rad);
 out.status="CONDITIONAL_LANDMARK_DISK_BOUND";return out;
}
// Independent outside-fit context retrieval; no pose translation/rotation is used.
// Uniqueness is limited to this retained run and finite search resolution.
struct LandmarkIdentityConfig {
 double radius_px=4,step_px=.1,score_tolerance_px=.01,maximum_context_error_px=.05,maximum_displacement_px=.25;
 int samples=12,maximum_hypotheses=2048;
};
struct LandmarkIdentitySearch {
 std::string status="IDENTITY_UNAVAILABLE";
 double best_arc=0,best_score=0,plausible_begin=0,plausible_end=0;
 std::vector<double> arc_positions,scores;
};
namespace landmark_identity_detail {
struct Curve {
 const std::vector<LandmarkPoint>& p;std::vector<double> s;
 explicit Curve(const std::vector<LandmarkPoint>& points):p(points),s(points.size(),0){
  if(p.size()<2||p.size()>4096)throw std::invalid_argument("IDENTITY_CURVE_SIZE");
  for(size_t i=0;i<p.size();++i){
   if(!std::isfinite(std::abs(p[i])))throw std::invalid_argument("IDENTITY_NONFINITE_CURVE");
   if(i){double d=std::abs(p[i]-p[i-1]);if(d<=1e-12)throw std::invalid_argument("IDENTITY_ZERO_EDGE");s[i]=s[i-1]+d;}
  }
 }
 LandmarkPoint at(double x)const{
  const double epsilon=1e-12*std::max(1.,s.back());
  if(x < -epsilon || x > s.back()+epsilon)throw std::invalid_argument("IDENTITY_EXTRAPOLATION");
  x=std::clamp(x,0.,s.back()); // rounding only, never extends retained support
  auto it=std::upper_bound(s.begin(),s.end(),x);size_t j=std::min(p.size()-1,static_cast<size_t>(it-s.begin()));
  j=std::max(size_t(1),j);return p[j-1]+(p[j]-p[j-1])*((x-s[j-1])/(s[j]-s[j-1]));
 }
 std::vector<LandmarkPoint> signature(double x,int direction,double radius,int count)const{
  if(x<0||x>s.back()||x+direction*radius<0||x+direction*radius>s.back())return {};
  auto origin=at(x),chord=at(x+direction*radius)-origin;
  if(std::abs(chord)<radius*.05)return {};
  auto unit=chord/std::abs(chord);std::vector<LandmarkPoint> result;
  for(int i=1;i<=count;++i)result.push_back((at(x+direction*radius*i/count)-origin)/unit/radius);
  return result;
 }
};
}
inline LandmarkIdentitySearch SearchLandmarkIdentity(
 const std::vector<LandmarkPoint>& reference,const std::vector<LandmarkPoint>& observation,
 double reference_arc,double claimed_observation_arc,int reference_outward,int observation_outward,
 double scale,const LandmarkIdentityConfig& config={}){
 if((reference_outward!=1&&reference_outward!=-1)||(observation_outward!=1&&observation_outward!=-1)||
  !std::isfinite(scale)||scale<=0||!std::isfinite(config.radius_px)||config.radius_px<=0||
  !std::isfinite(config.step_px)||config.step_px<=0||!std::isfinite(config.score_tolerance_px)||config.score_tolerance_px<0||
  !std::isfinite(config.maximum_context_error_px)||config.maximum_context_error_px<0||
  !std::isfinite(config.maximum_displacement_px)||config.maximum_displacement_px<=0||
  config.samples<3||config.samples>128||config.maximum_hypotheses<2||config.maximum_hypotheses>100000||
  !std::isfinite(reference_arc)||!std::isfinite(claimed_observation_arc))
  throw std::invalid_argument("IDENTITY_SEARCH_CONFIG");
 using namespace landmark_identity_detail;Curve r(reference),o(observation);LandmarkIdentitySearch out;
 auto ref=r.signature(reference_arc,reference_outward,config.radius_px,config.samples);
 double radius=config.radius_px*scale;
 if(ref.empty()||o.signature(claimed_observation_arc,observation_outward,radius,config.samples).empty()){
  out.status="IDENTITY_INSUFFICIENT_HELD_OUT_SUPPORT";return out;
 }
 double lo=observation_outward<0?radius:0,hi=observation_outward>0?o.s.back()-radius:o.s.back();
 if((hi-lo)/config.step_px+3>config.maximum_hypotheses){out.status="IDENTITY_SEARCH_BUDGET_EXCEEDED";return out;}
 auto score=[&](double x){
  auto sig=o.signature(x,observation_outward,radius,config.samples);
  if(sig.empty())return;
  double sum=0;for(size_t i=0;i<sig.size();++i)sum+=std::norm(sig[i]-ref[i]);
  out.arc_positions.push_back(x);out.scores.push_back(radius*std::sqrt(sum/sig.size()));
 };
 for(double x=lo;x<hi;x+=config.step_px)score(x);
 score(hi);score(claimed_observation_arc);
 auto best=std::min_element(out.scores.begin(),out.scores.end());size_t index=best-out.scores.begin();
 out.best_score=*best;out.best_arc=out.arc_positions[index];
 out.plausible_begin=hi;out.plausible_end=lo;
 for(size_t i=0;i<out.scores.size();++i)if(out.scores[i]<=out.best_score+config.score_tolerance_px){
  out.plausible_begin=std::min(out.plausible_begin,out.arc_positions[i]);
  out.plausible_end=std::max(out.plausible_end,out.arc_positions[i]);
 }
 if(out.best_score>config.maximum_context_error_px)out.status="IDENTITY_CONTEXT_MISMATCH";
 else if(out.plausible_end-out.plausible_begin>2*config.maximum_displacement_px)
  out.status="IDENTITY_AMBIGUOUS_WITHIN_RETAINED_RUN";
 else if(std::abs(out.best_arc-claimed_observation_arc)>config.maximum_displacement_px)
  out.status="IDENTITY_CLAIM_DISPLACED";
 else out.status="GEOMETRIC_IDENTITY_SUPPORTED_WITHIN_SEARCH";
 return out;
}
}
