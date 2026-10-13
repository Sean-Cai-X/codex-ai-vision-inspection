#pragma once
#include "common_arc_support.h"
namespace cxgeom::polar {
// Endpoint coordinates are explicit input in each OWN image frame. The caller
// asserts corresponding physical landmarks; the solver never receives a true pose.
struct EndpointProjectionCoordinates {
 std::vector<double> values; // per retained source vertex; may extend outside [0,1]
 double minimum=0,maximum=0,maximum_normal_residual_px=0;
 bool reversed=false;
 std::string status;
 bool valid()const{return status=="ENDPOINT_PROJECTION_AVAILABLE";}
};
inline EndpointProjectionCoordinates ProjectEndpointCoordinates(
 const std::vector<std::complex<double>>& points,std::complex<double> start,
 std::complex<double> end,double maximum_normal_residual_px){
 arc_detail::SampleArcPoints(points,3);
 if(!std::isfinite(std::abs(start))||!std::isfinite(std::abs(end))||
    !std::isfinite(maximum_normal_residual_px)||maximum_normal_residual_px<=0)
  throw std::invalid_argument("ENDPOINT_PROJECTION_INVALID_CONFIG");
 const auto chord=end-start;const double length=std::abs(chord);
 if(!std::isfinite(length)||length<=1e-9)throw std::invalid_argument("ENDPOINT_PROJECTION_COLLAPSED_ANCHORS");
 EndpointProjectionCoordinates out;const auto direction=chord/length;
 for(auto p:points){
  const auto q=std::conj(direction)*(p-start);
  out.values.push_back(q.real()/length);
  out.maximum_normal_residual_px=std::max(out.maximum_normal_residual_px,std::abs(q.imag()));
 }
 out.reversed=out.values.back()<out.values.front();
 out.minimum=std::min(out.values.front(),out.values.back());out.maximum=std::max(out.values.front(),out.values.back());
 if(out.maximum_normal_residual_px>maximum_normal_residual_px){
  out.status="ENDPOINT_PROJECTION_NONLINEAR_SUPPORT";return out;
 }
 for(size_t i=1;i<out.values.size();++i)if((out.reversed?-1.:1.)*(out.values[i]-out.values[i-1])<=1e-9){
  out.status="ENDPOINT_PROJECTION_NONMONOTONIC_SUPPORT";return out;
 }
 out.status="ENDPOINT_PROJECTION_AVAILABLE";return out;
}
inline CommonArcDomain IntersectProjectedSupport(const EndpointProjectionCoordinates& r,
 const EndpointProjectionCoordinates& o,double minimum_fraction){
 if(!std::isfinite(minimum_fraction)||minimum_fraction<=0||minimum_fraction>1)
  throw std::invalid_argument("COMMON_ARC_INVALID_MINIMUM_FRACTION");
 CommonArcDomain d;
 if(!r.valid()||!o.valid()){d.status=!r.valid()?r.status:o.status;return d;}
 d.begin=std::max({0.,r.minimum,o.minimum});d.end=std::min({1.,r.maximum,o.maximum});
 if(d.end-d.begin<=1e-9)d.status="COMMON_ARC_EMPTY_INTERSECTION";
 else if(d.end-d.begin+1e-12<minimum_fraction)d.status="COMMON_ARC_INSUFFICIENT_COVERAGE";
 else d.status="COMMON_ARC_DOMAIN_AVAILABLE";
 return d;
}
namespace common_arc_detail {
inline Slice SampleProjectedSlice(const std::vector<std::complex<double>>& full_raw,
 const std::vector<std::complex<double>>& refined,size_t begin,size_t end,
 const EndpointProjectionCoordinates& coordinates,const CommonArcDomain& domain,size_t count){
 if(!coordinates.valid()||!domain.accepted()||begin>=end||end>=full_raw.size()||
    refined.size()!=end-begin+1||coordinates.values.size()!=refined.size()||count<3||count>128)
  throw std::invalid_argument("ENDPOINT_PROJECTION_INVALID_SLICE");
 if(!std::isfinite(domain.begin)||!std::isfinite(domain.end)||domain.begin<0||domain.end>1||domain.begin>=domain.end)
  throw std::invalid_argument("COMMON_ARC_DOMAIN_REQUIRED");
 if(domain.begin<coordinates.minimum-1e-12||domain.end>coordinates.maximum+1e-12)
  throw std::invalid_argument("COMMON_ARC_CROSSES_REJECTED_GAP");
 RawFractions(full_raw);arc_detail::SampleArcPoints(refined,3);
 std::vector<double> values=coordinates.values;
 std::vector<size_t> indices;for(size_t k=0;k<values.size();++k)indices.push_back(k);
 if(coordinates.reversed){std::reverse(values.begin(),values.end());std::reverse(indices.begin(),indices.end());}
 for(size_t i=0;i<values.size();++i)if(!std::isfinite(values[i])||(i&&values[i]<=values[i-1]))
  throw std::invalid_argument("ENDPOINT_PROJECTION_NONMONOTONIC_SUPPORT");
 auto at=[&](double u,double normalized,bool refined_point){
  auto it=std::upper_bound(values.begin(),values.end(),u);
  size_t k=it==values.begin()?0:size_t(it-values.begin()-1);k=std::min(k,values.size()-2);
  double f=std::clamp((u-values[k])/(values[k+1]-values[k]),0.,1.);
  size_t edge=begin+std::min(indices[k],indices[k+1]);
  const double source_f=coordinates.reversed?1-f:f;
  const auto a=refined_point?refined[edge-begin]:full_raw[edge];
  const auto b=refined_point?refined[edge-begin+1]:full_raw[edge+1];
  return ArcSample{a+source_f*(b-a),edge,source_f,normalized};
 };
 Slice out;const auto raw_fractions=RawFractions(full_raw);
 auto source_fraction=[&](double u){
  const auto v=at(u,0,false);
  return raw_fractions[v.edge]+v.fraction*(raw_fractions[v.edge+1]-raw_fractions[v.edge]);
 };
 out.source_begin_fraction=source_fraction(domain.begin);out.source_end_fraction=source_fraction(domain.end);
 std::vector<double> knots{domain.begin};
 for(double u:values)if(u>domain.begin+1e-12&&u<domain.end-1e-12)knots.push_back(u);
 knots.push_back(domain.end);
 for(double u:knots){out.raw.push_back(at(u,0,false).point);out.refined.push_back(at(u,0,true).point);}
 for(size_t k=0;k<count;++k){
  double f=double(k)/double(count-1),u=domain.begin+f*(domain.end-domain.begin);
  out.raw_samples.values.push_back(at(u,f,false));out.refined_samples.values.push_back(at(u,f,true));
 }
 out.raw_samples.length=arc_detail::SampleArcPoints(out.raw,3).length;
 out.refined_samples.length=arc_detail::SampleArcPoints(out.refined,3).length;
 return out;
}
}
}
