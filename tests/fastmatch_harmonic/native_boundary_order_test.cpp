#include "../../cxgeom/polar_discrete_harmonic/boundary_candidate_gate.h"
#include "../../cxgeom/polar_discrete_harmonic/boundary_order_audit.h"
#include "../../cxgeom/polar_discrete_harmonic/partial_pipeline_receipt.h"
#include "../../libtorchsegmentation/src/utils/json.hpp"
#include <functional>
#include <iostream>
using namespace cxgeom;using namespace cxgeom::polar;
void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
void reject(const std::function<void()>& f,const char* why){
 try{f();}catch(const std::invalid_argument& e){check(std::string(e.what())==why,e.what());return;}
 throw std::runtime_error("expected order rejection");
}
ObservedBoundaryInput segment(const std::vector<std::complex<double>>& p,size_t begin,size_t count,
 const std::string& id,const std::string& source){
 OpenBoundaryRun r;r.parent_point_count=p.size();
 for(size_t k=0;k<count;++k){auto i=(begin+k)%p.size();r.parent_indices.push_back(i);r.points.push_back(p[i]);}
 r.wraps_parent_origin=begin+count>p.size();
 return ObservedBoundaryInput(source,"v","c",id,p,r);
}
int main()try{
 std::vector<std::complex<double>> p={{0,0},{3,0},{5,2},{4,5},{1,7},{-2,3}};
 auto q=p;for(auto& v:q)v=std::polar(1.2,.7)*v+std::complex<double>(17,-8);
 auto r=segment(p,0,5,"r","ref"),t=segment(q,0,5,"t","obs");
 ObservedBoundaryBundle ref({r}),obs({t});
 auto make=[&](std::vector<size_t> positions){
  std::vector<Pair> v;for(size_t i=0;i<positions.size();++i)
   v.push_back({r.features()[i].stable_id,t.features()[positions[i]].stable_id,0});
  return v;
 };
 auto forward=make({0,1,2,3,4});
 auto audit=AuditBoundaryOrder(ref,obs,forward);
 check(audit.order_consistent&&audit.segments[0].status=="FORWARD","forward");
 check(audit.segments[0].reference_endpoints_matched,"endpoints");
 auto j=nlohmann::json::parse(BoundaryOrderReceipt(audit));
 check(j["topology_verified"]==false&&j["pose_modified"]==false,"limited claim");
 check(j["segments"][0]["reference_ids"].size()==5,"source IDs");
 auto reversed=AuditBoundaryOrder(ref,obs,make({4,3,2,1,0}));
 check(reversed.order_consistent&&reversed.segments[0].status=="REVERSED","explicit reverse");
 auto scrambled=AuditBoundaryOrder(ref,obs,make({0,2,1,3,4}));
 check(!scrambled.order_consistent&&scrambled.segments[0].status=="NON_MONOTONIC","scrambled");
 auto gaps=forward;gaps.erase(gaps.begin()+3);gaps.erase(gaps.begin()+1);
 auto gap=AuditBoundaryOrder(ref,obs,gaps);
 check(!gap.order_consistent&&gap.segments[0].status=="FORWARD_WITH_GAPS","no bridging gaps");
 auto weak=AuditBoundaryOrder(ref,obs,make({0,1}));
 check(!weak.order_consistent&&weak.segments[0].status=="INSUFFICIENT_ORDER_SUPPORT","insufficient");
 ObservedBoundaryBundle split({segment(q,0,3,"a","obs"),segment(q,3,2,"b","obs")});
 auto cross=AuditBoundaryOrder(ref,split,forward);
 check(!cross.order_consistent&&cross.segments[0].status=="CROSS_SEGMENT","cross segment");
 ObservedBoundaryBundle overlap({t,segment(q,0,5,"alternate","obs")});
 auto ambiguous=AuditBoundaryOrder(ref,overlap,forward);
 check(!ambiguous.order_consistent&&ambiguous.segments[0].status=="AMBIGUOUS_SEGMENT","no silent segment selection");
 auto duplicate=forward;duplicate.push_back(forward[0]);
 reject([&]{AuditBoundaryOrder(ref,obs,duplicate);},"BOUNDARY_ORDER_NOT_ONE_TO_ONE");
 auto many=forward;many[1].target_id=many[0].target_id;
 reject([&]{AuditBoundaryOrder(ref,obs,many);},"BOUNDARY_ORDER_NOT_ONE_TO_ONE");
 auto foreign=forward;foreign[0].target_id="unknown";
 reject([&]{AuditBoundaryOrder(ref,obs,foreign);},"BOUNDARY_ORDER_UNKNOWN_ID");
 reject([&]{AuditBoundaryOrder(ref,obs,forward,2);},"BOUNDARY_ORDER_INVALID_MINIMUM");
 auto invalid=forward;invalid[0].residual_px=-1;
 reject([&]{AuditBoundaryOrder(ref,obs,invalid);},"BOUNDARY_ORDER_INVALID_RESIDUAL");
 // Actual solver integration: a separate audit must not change the raw receipt.
 PartialRequest request;request.request_id="order";request.reference_source="ref";request.observation_source="obs";
 request.reference=ref.features();request.observation=obs.features();request.config.maximum_hypotheses=4096;
 auto raw=SearchRefinedPartial(request);check(raw.complete&&raw.candidates.size()==1,"raw solver");
 auto before=PartialPipelineReceiptV1(raw);
 auto real=AuditBoundaryOrder(ref,obs,raw.candidates[0].pairs);
 check(real.order_consistent&&real.segments[0].status=="FORWARD","real correspondence order");
 check(before==PartialPipelineReceiptV1(raw),"raw receipt unchanged");
 BoundaryOrderPolicy policy;
 check(BoundaryOrderAllowed(audit,policy)&&!BoundaryOrderAllowed(reversed,policy),"default direction");
 policy.allow_reversed=true;check(BoundaryOrderAllowed(reversed,policy),"reverse enabled");
 check(!BoundaryOrderAllowed(gap,policy),"gaps disabled");
 policy.allow_gaps=true;check(BoundaryOrderAllowed(gap,policy),"gaps enabled");
 auto no_end=forward;no_end.pop_back();
 auto endpoint=AuditBoundaryOrder(ref,obs,no_end);
 check(!BoundaryOrderAllowed(endpoint,policy),"endpoints required");
 policy.require_reference_endpoints=false;check(BoundaryOrderAllowed(endpoint,policy),"endpoints optional");
 BoundaryCandidateGate gate(ref,obs,raw);
 check(gate.status()=="UNIQUE_ORDER_CANDIDATE"&&gate.eligible_indices().size()==1,"unique gate");
 check(nlohmann::json::parse(gate.receipt())["raw"]==nlohmann::json::parse(before),"embedded raw unchanged");
 policy.minimum_points=6;check(BoundaryCandidateGate(ref,obs,raw,policy).status()=="NO_ORDER_CANDIDATE","no candidate");
 policy.minimum_points=2;
 reject([&]{BoundaryCandidateGate bad(ref,obs,raw,policy);},"BOUNDARY_GATE_INVALID_MINIMUM");
 reject([&]{BoundaryCandidateGate bad(obs,ref,raw);},"BOUNDARY_GATE_SOURCE_MISMATCH");
 request.config.maximum_hypotheses=1;
 auto incomplete=SearchRefinedPartial(request);
 check(!incomplete.complete&&BoundaryCandidateGate(ref,obs,incomplete).status()=="RAW_INCOMPLETE","incomplete gate");
 check(before==PartialPipelineReceiptV1(raw),"gate leaves raw unchanged");
 std::reverse(forward.begin(),forward.end());
 check(BoundaryOrderReceipt(AuditBoundaryOrder(ref,obs,forward))==BoundaryOrderReceipt(audit),"pair input order invariance");
 std::cout<<"BOUNDARY_ORDER_PASS forward/reverse/gaps/cross/ambiguous/scrambled/insufficient/raw_unchanged\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
