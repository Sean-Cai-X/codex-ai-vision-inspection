#pragma once
#include "CxGeoSO2Harmonic.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
namespace cxgeom {
namespace completion {
using Point=std::complex<double>;
constexpr double pi=3.14159265358979323846;
inline void require(bool ok,const char* why){
 if(!ok)throw std::invalid_argument(why);
}
struct ObservedRun {
 std::string source_id;
 std::vector<Point> points;
 std::vector<size_t> source_indices;
 size_t source_count=0;
};
// Explicit caller-selected crop, not automatic surplus detection. End is exclusive.
inline ObservedRun Crop(const std::vector<Point>& raw,const std::string& id,size_t first,size_t end){
 require(!id.empty()&&raw.size()<=4096,"INVALID_CROP_SOURCE");
 require(first<end&&end<=raw.size()&&end-first>=3,"INVALID_CROP_RANGE");
 ObservedRun out;out.source_id=id;out.source_count=raw.size();
 for(size_t i=0;i<raw.size();++i){
  require(std::isfinite(raw[i].real())&&std::isfinite(raw[i].imag()),"NONFINITE_CROP_SOURCE");
  if(i>=first&&i<end){out.points.push_back(raw[i]);out.source_indices.push_back(i);}
 }
 return out;
}
struct Config {
 double minimum_angular_coverage=.4;
 double maximum_observed_residual_px=.3;
 double maximum_angular_step_deg=15;
 int completion_segments=128;
};
struct Result {
 so2::Contour contour;
 std::vector<bool> synthetic;
 std::vector<size_t> source_indices; // synthetic points carry size_t max, never a real index
 ObservedRun observed; // raw selected points and order preserved
 Point center;
 double radius=0,angular_coverage=0,observed_rms_px=0,observed_max_px=0;
 std::string method="circle_algebraic_least_squares";
 bool orientation_observable=false;
 bool measurement_evidence=false;
};
// Strong circle prior only. No general curve extension or production admission.
inline Result CompleteCircle(const ObservedRun& run,const Config& cfg=Config{}){
 require(std::isfinite(cfg.minimum_angular_coverage)&&cfg.minimum_angular_coverage>=.4&&
         cfg.minimum_angular_coverage<1,"INVALID_COMPLETION_COVERAGE");
 require(std::isfinite(cfg.maximum_observed_residual_px)&&cfg.maximum_observed_residual_px>0,
         "INVALID_COMPLETION_RESIDUAL");
 require(std::isfinite(cfg.maximum_angular_step_deg)&&cfg.maximum_angular_step_deg>0&&
         cfg.maximum_angular_step_deg<=90&&cfg.completion_segments>=8&&
         cfg.completion_segments<=4096,"INVALID_COMPLETION_SAMPLING");
 const auto& p=run.points;
 require(!run.source_id.empty()&&p.size()>=16&&p.size()<=4096&&
         run.source_count<=4096&&run.source_indices.size()==p.size(),"INVALID_OBSERVED_RUN");
 Point mean{};
 for(size_t i=0;i<p.size();++i){
  require(std::isfinite(p[i].real())&&std::isfinite(p[i].imag()),"NONFINITE_OBSERVATION");
  require(run.source_indices[i]<run.source_count&&
          (i==0||run.source_indices[i]==run.source_indices[i-1]+1),"INVALID_SOURCE_ORDER");
  mean+=p[i];
 }
 mean/=double(p.size());
 double xx=0,xy=0,yy=0,xq=0,yq=0,qsum=0;
 for(auto v:p){
  auto z=v-mean;double x=z.real(),y=z.imag(),q=std::norm(z);
  xx+=x*x;xy+=x*y;yy+=y*y;xq+=x*q;yq+=y*q;qsum+=q;
 }
 double det=xx*yy-xy*xy;
 require(std::isfinite(det)&&xx+yy>1e-12&&det>1e-10*(xx+yy)*(xx+yy),
         "CIRCLE_FIT_DEGENERATE");
 Point offset((xq*yy-yq*xy)/(2*det),(yq*xx-xq*xy)/(2*det));
 Result r;r.observed=run;r.center=mean+offset;
 r.radius=std::sqrt(qsum/p.size()+std::norm(offset));
 require(std::isfinite(r.radius)&&r.radius>1e-6,"INVALID_FITTED_RADIUS");
 double square=0;
 for(auto v:p){
  double e=std::abs(std::abs(v-r.center)-r.radius);
  square+=e*e;r.observed_max_px=std::max(r.observed_max_px,e);
 }
 r.observed_rms_px=std::sqrt(square/p.size());
 require(r.observed_max_px<=cfg.maximum_observed_residual_px,"CIRCLE_PRIOR_RESIDUAL_REJECTED");
 double sweep=0,direction=0;
 for(size_t i=1;i<p.size();++i){
  double d=std::remainder(std::arg(p[i]-r.center)-std::arg(p[i-1]-r.center),2*pi);
  require(std::abs(d)>1e-10&&std::abs(d)<=cfg.maximum_angular_step_deg*pi/180,
          "CIRCLE_DISCONTINUOUS_OBSERVATION");
  if(i==1)direction=d>0?1:-1;
  require(d*direction>0,"CIRCLE_ORDER_REVERSAL");
  sweep+=d;
 }
 r.angular_coverage=std::abs(sweep)/(2*pi);
 require(r.angular_coverage>=cfg.minimum_angular_coverage&&r.angular_coverage<.999,
         "CIRCLE_COVERAGE_REJECTED");
 require(p.size()+size_t(cfg.completion_segments)-1<=4096,"COMPLETION_POINT_BUDGET");
 r.contour.points=p;r.source_indices=run.source_indices;r.synthetic.assign(p.size(),false);
 double start=std::arg(p.back()-r.center),gap=direction*2*pi-sweep;
 for(int i=1;i<cfg.completion_segments;++i){
  r.contour.points.push_back(r.center+std::polar(r.radius,start+gap*i/cfg.completion_segments));
  r.synthetic.push_back(true);r.source_indices.push_back(std::numeric_limits<size_t>::max());
 }
 r.contour.closed=true;r.contour.complete=true;
 // Reuse core self-intersection/topology guards before marking the result usable.
 r.contour.topology_verified=true;
 so2::Build(r.contour);
 return r;
}
// Projection contains observed points only; the closed synthetic contour stays separate.
inline ObservedRun ProjectObserved(const Result& r){
 require(r.contour.points.size()==r.synthetic.size()&&r.synthetic.size()==r.source_indices.size(),
         "INVALID_COMPLETION_TAGS");
 ObservedRun out;out.source_id=r.observed.source_id;out.source_count=r.observed.source_count;
 for(size_t i=0;i<r.synthetic.size();++i)if(!r.synthetic[i]){
  out.points.push_back(r.contour.points[i]);out.source_indices.push_back(r.source_indices[i]);
 }
 require(out.points==r.observed.points&&out.source_indices==r.observed.source_indices,
         "OBSERVED_PROVENANCE_MISMATCH");
 return out;
}
}
}
