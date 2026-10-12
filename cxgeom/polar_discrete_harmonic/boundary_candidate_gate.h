#pragma once
#include "boundary_order_audit.h"
#include "partial_pipeline_receipt.h"
namespace cxgeom::polar {
struct BoundaryOrderPolicy {
 size_t minimum_points=3;
 bool allow_reversed=false,allow_gaps=false,require_reference_endpoints=true;
};
inline bool BoundaryOrderAllowed(const BoundaryOrderAudit& audit,const BoundaryOrderPolicy& p){
 if(audit.segments.empty())return false;
 for(const auto& s:audit.segments){
  bool direction=s.status=="FORWARD"||(p.allow_reversed&&s.status=="REVERSED");
  if(p.allow_gaps)direction=direction||s.status=="FORWARD_WITH_GAPS"||(p.allow_reversed&&s.status=="REVERSED_WITH_GAPS");
  if(!direction||s.matched_points<p.minimum_points||
     (p.require_reference_endpoints&&!s.reference_endpoints_matched))return false;
 }
 return true;
}
struct BoundaryCandidateDecision {
 size_t raw_index=0;BoundaryOrderAudit audit;bool allowed=false;
};
// Separate observer. No access to test truth; no re-ranking or mutation of raw results.
class BoundaryCandidateGate {
 BoundaryOrderPolicy policy_;
 std::string raw_receipt_,status_;
 std::vector<BoundaryCandidateDecision> decisions_;
 std::vector<size_t> eligible_;
public:
 BoundaryCandidateGate(const ObservedBoundaryBundle& reference,const ObservedBoundaryBundle& target,
  const PartialPipelineResult& raw,const BoundaryOrderPolicy& policy={}):policy_(policy){
  if(policy.minimum_points<3||policy.minimum_points>128)throw std::invalid_argument("BOUNDARY_GATE_INVALID_MINIMUM");
  if(raw.raw.reference_source!=reference.segments().front().source_id()||
     raw.raw.observation_source!=target.segments().front().source_id())
   throw std::invalid_argument("BOUNDARY_GATE_SOURCE_MISMATCH");
  raw_receipt_=PartialPipelineReceiptV1(raw); // Existing provenance/accounting guards.
  if(!raw.complete){status_="RAW_INCOMPLETE";return;}
  for(size_t i=0;i<raw.candidates.size();++i){
   const auto& candidate=raw.candidates[i];
   auto audit=AuditBoundaryOrder(reference,target,candidate.pairs,policy.minimum_points);
   bool allowed=candidate.accepted&&BoundaryOrderAllowed(audit,policy);
   decisions_.push_back({i,std::move(audit),allowed});
   if(allowed)eligible_.push_back(i);
  }
  status_=eligible_.empty()?"NO_ORDER_CANDIDATE":
   eligible_.size()==1?"UNIQUE_ORDER_CANDIDATE":"ORDER_AMBIGUOUS";
 }
 const std::string& status()const{return status_;}
 const std::vector<size_t>& eligible_indices()const{return eligible_;}
 std::string receipt()const{
  std::ostringstream o;o.imbue(std::locale::classic());
  auto b=[](bool v){return v?"true":"false";};
  o<<"{\"schema\":\"boundary_candidate_gate.v1\",\"production_eligible\":false,\"topology_verified\":false";
  o<<",\"pose_modified\":false,\"selection_basis\":\"observed_order_policy_only\",\"status\":"<<receipt_detail::quote(status_);
  o<<",\"policy\":{\"minimum_points\":"<<policy_.minimum_points<<",\"allow_reversed\":"<<b(policy_.allow_reversed);
  o<<",\"allow_gaps\":"<<b(policy_.allow_gaps)<<",\"require_reference_endpoints\":"<<b(policy_.require_reference_endpoints)<<'}';
  o<<",\"eligible_raw_indices\":[";
  for(size_t i=0;i<eligible_.size();++i){if(i)o<<',';o<<eligible_[i];}
  o<<"],\"raw\":"<<raw_receipt_<<",\"decisions\":[";
  for(size_t i=0;i<decisions_.size();++i){
   if(i)o<<',';
   const auto& d=decisions_[i];
   o<<"{\"raw_index\":"<<d.raw_index<<",\"allowed\":"<<b(d.allowed)<<",\"audit\":"<<BoundaryOrderReceipt(d.audit)<<'}';
  }
  o<<"]}";return o.str();
 }
};
}
