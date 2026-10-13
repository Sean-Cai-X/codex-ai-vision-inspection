#pragma once
#include "observed_boundary_bundle.h"
#include "matching.h"
#include <algorithm>
namespace cxgeom::polar {
struct BoundarySegmentOrder {
 size_t reference_segment=0,matched_points=0;
 std::vector<size_t> possible_target_segments,reference_positions,target_positions;
 std::vector<std::string> reference_ids,target_ids;
 std::string status;
 bool reference_endpoints_matched=false;
};
struct BoundaryOrderAudit {
 std::vector<BoundarySegmentOrder> segments;
 bool order_consistent=false; // Point order only; never geometric/topological approval.
 size_t minimum_points=3;
 std::string status;
};
// Audits supplied one-to-one IDs only, WITHOUT modifying/re-ranking the raw pose.
// Reversed order is reported explicitly, not silently normalized.
inline BoundaryOrderAudit AuditBoundaryOrder(const ObservedBoundaryBundle& reference,
 const ObservedBoundaryBundle& target,const std::vector<Pair>& pairs,size_t minimum_points=3){
 auto need=[](bool b,const char* why){if(!b)throw std::invalid_argument(why);};
 need(minimum_points>=3&&minimum_points<=128,"BOUNDARY_ORDER_INVALID_MINIMUM");
 std::set<std::string> ref_ids,target_ids,used_target;
 for(const auto& p:reference.features())ref_ids.insert(p.stable_id);
 for(const auto& p:target.features())target_ids.insert(p.stable_id);
 std::map<std::string,std::string> mapping;
 for(const auto& p:pairs){
  need(ref_ids.count(p.reference_id)&&target_ids.count(p.target_id),"BOUNDARY_ORDER_UNKNOWN_ID");
  need(std::isfinite(p.residual_px)&&p.residual_px>=0,"BOUNDARY_ORDER_INVALID_RESIDUAL");
  need(mapping.emplace(p.reference_id,p.target_id).second&&used_target.insert(p.target_id).second,"BOUNDARY_ORDER_NOT_ONE_TO_ONE");
 }
 BoundaryOrderAudit out;out.minimum_points=minimum_points;out.order_consistent=true;
 for(size_t r=0;r<reference.segments().size();++r){
  BoundarySegmentOrder result;result.reference_segment=r;
  const auto& segment=reference.segments()[r].features();
  for(size_t i=0;i<segment.size();++i){
   auto found=mapping.find(segment[i].stable_id);if(found==mapping.end())continue;
   result.reference_positions.push_back(i);result.reference_ids.push_back(found->first);result.target_ids.push_back(found->second);
  }
  result.matched_points=result.reference_ids.size();
  result.reference_endpoints_matched=mapping.count(segment.front().stable_id)&&mapping.count(segment.back().stable_id);
  if(result.matched_points<minimum_points)result.status="INSUFFICIENT_ORDER_SUPPORT";
  else {
   for(size_t t=0;t<target.segments().size();++t){
    const auto& points=target.segments()[t].features();bool all=true;
    for(const auto& id:result.target_ids)
     all=all&&std::any_of(points.begin(),points.end(),[&](const Feature& p){return p.stable_id==id;});
    if(all)result.possible_target_segments.push_back(t);
   }
   if(result.possible_target_segments.empty())result.status="CROSS_SEGMENT";
   else if(result.possible_target_segments.size()>1)result.status="AMBIGUOUS_SEGMENT";
   else {
    const auto& points=target.segments()[result.possible_target_segments[0]].features();
    for(const auto& id:result.target_ids){
     auto it=std::find_if(points.begin(),points.end(),[&](const Feature& p){return p.stable_id==id;});
     result.target_positions.push_back(size_t(it-points.begin()));
    }
    bool forward=true,reverse=true,gaps=false;
    for(size_t i=1;i<result.matched_points;++i){
     const auto a=result.target_positions[i-1],b=result.target_positions[i];
     forward=forward&&b>a;reverse=reverse&&b<a;
     gaps=gaps||(a>b?a-b:b-a)!=1||result.reference_positions[i]-result.reference_positions[i-1]!=1;
    }
    if(!forward&&!reverse)result.status="NON_MONOTONIC";
    else result.status=std::string(forward?"FORWARD":"REVERSED")+(gaps?"_WITH_GAPS":"");
   }
  }
  out.order_consistent=out.order_consistent&&(result.status=="FORWARD"||result.status=="REVERSED");
  out.segments.push_back(std::move(result));
 }
 out.status=out.order_consistent?"OBSERVED_POINT_ORDER_CONSISTENT":"ORDER_REVIEW_REQUIRED";
 return out;
}
inline std::string BoundaryOrderReceipt(const BoundaryOrderAudit& audit){
 std::ostringstream o;o.imbue(std::locale::classic());
 o<<"{\"schema\":\"boundary_order_audit.v1\",\"scope\":\"supplied_correspondence_order_only\"";
 o<<",\"topology_verified\":false,\"production_eligible\":false,\"pose_modified\":false";
 o<<",\"status\":"<<receipt_detail::quote(audit.status)<<",\"order_consistent\":"<<(audit.order_consistent?"true":"false");
 o<<",\"minimum_points\":"<<audit.minimum_points<<",\"segments\":[";
 auto numbers=[&](const std::vector<size_t>& v){o<<'[';for(size_t i=0;i<v.size();++i){if(i)o<<',';o<<v[i];}o<<']';};
 auto ids=[&](const std::vector<std::string>& v){o<<'[';for(size_t i=0;i<v.size();++i){if(i)o<<',';o<<receipt_detail::quote(v[i]);}o<<']';};
 for(size_t i=0;i<audit.segments.size();++i){
  if(i)o<<',';
  const auto& s=audit.segments[i];
  o<<"{\"reference_segment\":"<<s.reference_segment<<",\"status\":"<<receipt_detail::quote(s.status);
  o<<",\"matched_points\":"<<s.matched_points<<",\"reference_endpoints_matched\":"<<(s.reference_endpoints_matched?"true":"false");
  o<<",\"possible_target_segments\":";numbers(s.possible_target_segments);
  o<<",\"reference_positions\":";numbers(s.reference_positions);
  o<<",\"target_positions\":";numbers(s.target_positions);
  o<<",\"reference_ids\":";ids(s.reference_ids);
  o<<",\"target_ids\":";ids(s.target_ids);o<<'}';
 }
 o<<"]}";return o.str();
}
}
