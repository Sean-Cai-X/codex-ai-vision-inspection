#pragma once
#include "boundary_arc_correspondence.h"
namespace cxgeom::polar {
// The two endpoints are explicitly asserted corresponding landmarks, expressed
// in ORIGINAL raw arc fractions. This is not an automatic correspondence finder.
struct RawArcAnchors { double begin=0,end=1; };
struct CommonArcDomain {
 double begin=0,end=0;
 std::string status;
 bool accepted()const{return status=="COMMON_ARC_DOMAIN_AVAILABLE";}
};
inline void ValidateRawArcAnchors(const RawArcAnchors& a){
 if(!std::isfinite(a.begin)||!std::isfinite(a.end)||a.begin<0||a.begin>1||
    a.end<0||a.end>1||std::abs(a.end-a.begin)<=1e-9)
  throw std::invalid_argument("COMMON_ARC_INVALID_ANCHORS");
}
inline CommonArcDomain IntersectCommonArcDomain(const RawArcAnchors& r,const RawArcAnchors& o,
 const std::pair<double,double>& rs,const std::pair<double,double>& os,double minimum_fraction){
 ValidateRawArcAnchors(r);ValidateRawArcAnchors(o);
 if(!std::isfinite(minimum_fraction)||minimum_fraction<=0||minimum_fraction>1)
  throw std::invalid_argument("COMMON_ARC_INVALID_MINIMUM_FRACTION");
 auto interval=[](const RawArcAnchors& a,const std::pair<double,double>& support){
  if(!std::isfinite(support.first)||!std::isfinite(support.second)||support.first<0||
     support.second>1||support.first>=support.second)
   throw std::invalid_argument("COMMON_ARC_INVALID_SUPPORT");
  double x=(support.first-a.begin)/(a.end-a.begin),y=(support.second-a.begin)/(a.end-a.begin);
  return std::pair<double,double>{std::min(x,y),std::max(x,y)};
 };
 const auto ri=interval(r,rs),oi=interval(o,os);
 CommonArcDomain d;d.begin=std::max({0.,ri.first,oi.first});d.end=std::min({1.,ri.second,oi.second});
 if(d.end-d.begin<=1e-9)d.status="COMMON_ARC_EMPTY_INTERSECTION";
 else if(d.end-d.begin+1e-12<minimum_fraction)d.status="COMMON_ARC_INSUFFICIENT_COVERAGE";
 else d.status="COMMON_ARC_DOMAIN_AVAILABLE";
 return d;
}
namespace common_arc_detail {
inline std::vector<double> RawFractions(const std::vector<std::complex<double>>& raw){
 arc_detail::SampleArcPoints(raw,3); // finite, ordered-edge and size validation
 std::vector<double> f(raw.size(),0);
 for(size_t i=1;i<raw.size();++i)f[i]=f[i-1]+std::abs(raw[i]-raw[i-1]);
 const double length=f.back();for(auto& v:f)v/=length;return f;
}
struct Slice {
 std::vector<std::complex<double>> raw,refined; // clipped endpoints + ALL internal vertices
 ArcSamples raw_samples,refined_samples;
 double source_begin_fraction=0,source_end_fraction=0;
};
inline Slice SampleCommonSlice(const std::vector<std::complex<double>>& full_raw,
 const std::vector<std::complex<double>>& refined,size_t begin,size_t end,
 const RawArcAnchors& anchors,const CommonArcDomain& domain,size_t count){
 ValidateRawArcAnchors(anchors);
 if(!domain.accepted()||!std::isfinite(domain.begin)||!std::isfinite(domain.end)||
    domain.begin<0||domain.end>1||domain.begin>=domain.end)
  throw std::invalid_argument("COMMON_ARC_DOMAIN_REQUIRED");
 if(begin>=end||end>=full_raw.size()||refined.size()!=end-begin+1||count<3||count>128)
  throw std::invalid_argument("COMMON_ARC_INVALID_SPAN");
 arc_detail::SampleArcPoints(refined,3);
 const auto fractions=RawFractions(full_raw);
 Slice out;out.source_begin_fraction=anchors.begin+domain.begin*(anchors.end-anchors.begin);
 out.source_end_fraction=anchors.begin+domain.end*(anchors.end-anchors.begin);
 const double lo=std::min(out.source_begin_fraction,out.source_end_fraction);
 const double hi=std::max(out.source_begin_fraction,out.source_end_fraction);
 if(lo<fractions[begin]-1e-12||hi>fractions[end]+1e-12)
  throw std::invalid_argument("COMMON_ARC_CROSSES_REJECTED_GAP");
 auto at=[&](double u,double normalized,bool use_refined){
  auto it=std::upper_bound(fractions.begin()+begin,fractions.begin()+end+1,u);
  size_t edge=it==fractions.begin()+begin?begin:size_t(it-fractions.begin()-1);
  edge=std::clamp(edge,begin,end-1);
  double t=std::clamp((u-fractions[edge])/(fractions[edge+1]-fractions[edge]),0.,1.);
  auto a=use_refined?refined[edge-begin]:full_raw[edge];
  auto b=use_refined?refined[edge-begin+1]:full_raw[edge+1];
  return ArcSample{a+t*(b-a),edge,t,normalized};
 };
 std::vector<double> knots{lo};
 for(size_t i=begin+1;i<end;++i)if(fractions[i]>lo+1e-12&&fractions[i]<hi-1e-12)knots.push_back(fractions[i]);
 knots.push_back(hi);if(out.source_begin_fraction>out.source_end_fraction)std::reverse(knots.begin(),knots.end());
 for(double u:knots){out.raw.push_back(at(u,0,false).point);out.refined.push_back(at(u,0,true).point);}
 for(size_t i=0;i<count;++i){
  double v=double(i)/double(count-1),u=out.source_begin_fraction+v*(out.source_end_fraction-out.source_begin_fraction);
  out.raw_samples.values.push_back(at(u,v,false));out.refined_samples.values.push_back(at(u,v,true));
 }
 out.raw_samples.length=arc_detail::SampleArcPoints(out.raw,3).length;
 out.refined_samples.length=arc_detail::SampleArcPoints(out.refined,3).length;
 return out;
}
}
}
