#include "encoding.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace cxgeom::polar {
namespace {
constexpr double pi=3.14159265358979323846;
void need(bool ok,const char* why){if(!ok)throw std::invalid_argument(why);}
}
Descriptor Encode(const std::string& id,const std::vector<Feature>& input,const Config& cfg){
 need(!id.empty()&&input.size()>=3&&input.size()<=4096,"POLAR_INVALID_SOURCE");
 need(cfg.bins>=16&&cfg.bins<=4096&&cfg.max_order>=1&&cfg.max_order<=64&&
      cfg.bins>2*cfg.max_order,"POLAR_INVALID_SAMPLING");
 need(std::isfinite(cfg.minimum_rms_radius)&&cfg.minimum_rms_radius>0,"POLAR_INVALID_RADIUS_LIMIT");
 Descriptor d;d.config=cfg;d.source_id=id;d.features=input;
 std::sort(d.features.begin(),d.features.end(),[](const Feature& a,const Feature& b){
  return a.stable_id<b.stable_id;
 });
 std::string previous;
 for(const auto& f:d.features){
  need(!f.stable_id.empty()&&!f.source_element_id.empty()&&f.stable_id!=previous,"POLAR_INVALID_FEATURE_ID");
  previous=f.stable_id;
  need(std::isfinite(f.point.real())&&std::isfinite(f.point.imag())&&
       std::abs(f.point.real())<=1e12&&std::abs(f.point.imag())<=1e12,"POLAR_INVALID_POINT");
  need(std::isfinite(f.weight)&&f.weight>0&&f.weight<=1,"POLAR_INVALID_WEIGHT");
  need(std::isfinite(f.curvature)&&std::isfinite(f.local_direction_rad),"POLAR_INVALID_ATTRIBUTE");
  d.total_weight+=f.weight;
 }
 // Center relative to a fixed canonical anchor to reduce cancellation.
 const auto anchor=d.features.front().point;
 for(const auto& f:d.features)d.centroid+=(f.point-anchor)*(f.weight/d.total_weight);
 d.centroid+=anchor;
 double sum=0;
 for(const auto& f:d.features)sum+=std::norm(f.point-d.centroid)*(f.weight/d.total_weight);
 d.rms_radius=std::sqrt(sum);
 need(std::isfinite(d.rms_radius)&&d.rms_radius>=cfg.minimum_rms_radius,"POLAR_DEGENERATE_SCALE");
 for(int c=0;c<3;++c){
  d.angular_signal[c].assign(cfg.bins,0);
  d.moments[c].assign(cfg.max_order+1,{});
 }
 size_t angular_count=0;
 for(const auto& f:d.features){
  auto z=(f.point-d.centroid)/d.rms_radius;
  double r=std::abs(z),w=f.weight/d.total_weight;
  if(r<=1e-12){d.central_weight_fraction+=w;continue;}
  ++angular_count;
  double theta=std::arg(z);if(theta<0)theta+=2*pi;
  double bin=theta*cfg.bins/(2*pi);
  int lo=int(std::floor(bin));double fraction=bin-lo;
  lo%=cfg.bins;
  const double amplitudes[3]={w,w*r,w*r*r};
  for(int c=0;c<3;++c){
   d.angular_signal[c][lo]+=amplitudes[c]*(1-fraction);
   d.angular_signal[c][(lo+1)%cfg.bins]+=amplitudes[c]*fraction;
   for(int k=0;k<=cfg.max_order;++k)
    d.moments[c][k]+=amplitudes[c]*std::polar(1.,-k*theta);
  }
 }
 need(angular_count>=3,"POLAR_INSUFFICIENT_ANGULAR_FEATURES");
 return d;
}
}
