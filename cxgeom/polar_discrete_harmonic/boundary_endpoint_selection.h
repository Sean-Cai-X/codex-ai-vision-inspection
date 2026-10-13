#pragma once
#include "observed_boundary_input.h"
#include <limits>
namespace cxgeom::polar {
// All three anchors are in the observed image frame, not an inferred pose.
// Via selects one of the two arcs. Output always preserves parent traversal.
struct BoundaryEndpointAnchors {
 std::complex<double> start,end,via;
 double maximum_snap_px=2.0,tie_epsilon_px=1e-8;
 std::string provenance;
};
// Typed data rejection retains competing pixels without choosing an identity.
class BoundaryEndpointRejection:public std::invalid_argument{
 std::string evidence_;
public:
 BoundaryEndpointRejection(const char* reason,std::string evidence):std::invalid_argument(reason),evidence_(std::move(evidence)){}
 const std::string& receipt()const{return evidence_;}
};
struct EndpointBoundarySelection {
 cxgeom::OpenBoundaryRun run;
 BoundaryEndpointAnchors anchors;
 size_t start_index=0,end_index=0,via_index=0;
 double start_snap=0,end_snap=0,via_snap=0;
 bool endpoints_swapped=false;
 std::string receipt()const{
  std::ostringstream o;o.imbue(std::locale::classic());o<<std::setprecision(17);
  o<<"{\"schema\":\"boundary_endpoint_selection.v1\",\"direction\":\"parent_forward\",\"closed\":false,\"synthetic_count\":0";
  o<<",\"provenance\":"<<receipt_detail::quote(anchors.provenance);
  auto point=[&](const char* name,std::complex<double> p,size_t index,double distance){
   o<<",\""<<name<<"\":{\"requested\":["<<p.real()<<','<<p.imag()<<"],\"parent_index\":"<<index<<",\"snap_px\":"<<distance<<'}';
  };
  point("start",anchors.start,start_index,start_snap);point("end",anchors.end,end_index,end_snap);point("via",anchors.via,via_index,via_snap);
  o<<",\"maximum_snap_px\":"<<anchors.maximum_snap_px<<",\"tie_epsilon_px\":"<<anchors.tie_epsilon_px;
  o<<",\"endpoints_swapped\":"<<(endpoints_swapped?"true":"false")<<",\"production_eligible\":false}";
  return o.str();
 }
};
inline EndpointBoundarySelection SelectBoundaryEndpoints(
 const std::vector<std::complex<double>>& parent,const BoundaryEndpointAnchors& a,size_t minimum=3){
 auto need=[](bool v,const char* why){if(!v)throw std::invalid_argument(why);};
 need(parent.size()>=3&&parent.size()<=4096,"BOUNDARY_ENDPOINT_PARENT_SIZE");
 need(minimum>=3&&minimum<=128,"BOUNDARY_ENDPOINT_MINIMUM");
 need(!a.provenance.empty(),"BOUNDARY_ENDPOINT_MISSING_PROVENANCE");
 need(std::isfinite(a.maximum_snap_px)&&a.maximum_snap_px>=0&&std::isfinite(a.tie_epsilon_px)&&
  a.tie_epsilon_px>=0&&a.tie_epsilon_px<=a.maximum_snap_px,"BOUNDARY_ENDPOINT_SNAP_CONFIG");
 for(auto p:parent)need(std::isfinite(p.real())&&std::isfinite(p.imag()),"BOUNDARY_ENDPOINT_NONFINITE_PARENT");
 auto snap=[&](const char* role,std::complex<double> p,size_t& index,double& distance){
  need(std::isfinite(p.real())&&std::isfinite(p.imag()),"BOUNDARY_ENDPOINT_NONFINITE_ANCHOR");
  distance=std::numeric_limits<double>::infinity();double second=distance;
  for(size_t i=0;i<parent.size();++i){
   double d=std::abs(p-parent[i]);
   if(d<distance){second=distance;distance=d;index=i;}else if(d<second)second=d;
  }
  const char* reason=distance>a.maximum_snap_px?"BOUNDARY_ENDPOINT_OUT_OF_RANGE":
   second-distance<=a.tie_epsilon_px?"BOUNDARY_ENDPOINT_AMBIGUOUS_SNAP":nullptr;
  if(reason){
   std::ostringstream o;o.imbue(std::locale::classic());o<<std::setprecision(17);
   o<<"{\"schema\":\"boundary_endpoint_rejection.v1\",\"status\":"<<receipt_detail::quote(reason)
    <<",\"role\":"<<receipt_detail::quote(role)<<",\"requested\":["<<p.real()<<','<<p.imag()<<']'
    <<",\"nearest_distance_px\":"<<distance<<",\"second_distance_px\":"<<second
    <<",\"maximum_snap_px\":"<<a.maximum_snap_px<<",\"tie_epsilon_px\":"<<a.tie_epsilon_px
    <<",\"provenance\":"<<receipt_detail::quote(a.provenance)
    <<",\"selected\":false,\"production_eligible\":false,\"candidates\":[";
   bool comma=false;for(size_t i=0;i<parent.size();++i){
    const double d=std::abs(p-parent[i]);if(d>second+a.tie_epsilon_px)continue;
    if(comma)o<<',';comma=true;
    o<<"{\"parent_index\":"<<i<<",\"point\":["<<parent[i].real()<<','<<parent[i].imag()
     <<"],\"distance_px\":"<<d<<",\"selected\":false}";
   }o<<"]}";throw BoundaryEndpointRejection(reason,o.str());
  }
 };
 EndpointBoundarySelection out;out.anchors=a;
 snap("start",a.start,out.start_index,out.start_snap);snap("end",a.end,out.end_index,out.end_snap);snap("via",a.via,out.via_index,out.via_snap);
 need(out.start_index!=out.end_index,"BOUNDARY_ENDPOINT_COLLAPSED");
 need(out.via_index!=out.start_index&&out.via_index!=out.end_index,"BOUNDARY_ENDPOINT_VIA_AT_ENDPOINT");
 const size_t n=parent.size();
 auto forward=[&](size_t i,size_t j){return (j+n-i)%n;};
 size_t first=out.start_index,last=out.end_index;
 if(forward(first,out.via_index)>forward(first,last)){std::swap(first,last);out.endpoints_swapped=true;}
 const size_t count=forward(first,last)+1;
 need(count<n,"BOUNDARY_ENDPOINT_WHOLE_CONTOUR");
 need(count>=minimum,"BOUNDARY_ENDPOINT_INSUFFICIENT_POINTS");
 out.run.parent_point_count=n;out.run.wraps_parent_origin=first+count>n;
 for(size_t k=0;k<count;++k){size_t i=(first+k)%n;out.run.parent_indices.push_back(i);out.run.points.push_back(parent[i]);}
 return out;
}
}
