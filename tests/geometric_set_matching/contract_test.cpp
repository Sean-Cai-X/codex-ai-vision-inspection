#include "../../cxgeom/geometric_set_matching/types.h"
#include "../../cxgeom/geometric_set_matching/topology.h"
#include "../../libtorchsegmentation/src/utils/json.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace cxgeom::gsm;
namespace {
int checks=0;
void need(bool condition,const char* label) {
    ++checks;if(!condition)throw std::runtime_error(label);
}
Request fixture() {
    Request r;r.request_id="GSM0";
    r.reference={"reference","fixture:reference",{0,0,100,100},{
        {"p0","fixture:p0",1,Point2{10,20}},
        {"p1","fixture:p1",0.8,Point2{75,40}},
        {"l0","fixture:l0",1,LineSegment{{20,30},{40,60}}},
        {"a0","fixture:a0",1,ArcSegment{{50,50},10,0,90}}}};
    r.target=r.reference;r.target.set_id="target";r.target.source_ref="fixture:target";
    r.parameters.search_roi={0,0,100,100};
    return r;
}
TopologyInput contour() {
    TopologyInput t;t.branch=Branch::ClosedContour;t.source_ref="fixture:square";
    t.order_evidence_ref="fixture:ordered_vertices";t.caller_confirmed=true;
    t.closure_edge_observed=true;t.complete=true;
    t.parts={{{10,10},{30,10},{30,30},{10,30}}};return t;
}
void rejected(const Request& r,const char* label) {
    auto v=Validate(r);need(!v.accepted,label);
    auto m=Match(r);need(m.execution_status!=ExecutionStatus::Completed &&
        !m.solvability && m.candidates.empty() && !m.search_complete && !m.production_eligible,
        "rejection cannot fabricate match");
}
void topologyReason(const TopologyInput& t,const char* reason,const TopologyPolicy& p={}) {
    auto r=ContourTopologyValidator::Check(t,p);
    need(!r.accepted && !r.physical_boundary_verified && !r.reasons.empty() &&
         r.reasons.front()==reason,reason);
}
}
int main(int argc,char** argv) try {
    auto r=fixture();need(Validate(r).accepted,"valid mixed contract");
    auto result=Match(r);
    need(result.execution_status==ExecutionStatus::Completed&&!result.solvability&&
         result.candidates.size()==1&&result.search_complete&&!result.production_eligible,
         "P1 candidate is not a calibrated solvability or production claim");
    need(result.request_id==r.request_id&&result.reasons.at(0)=="P1_FULL_SET_CANDIDATE",
         "request identity and explicit capability status");
    std::reverse(r.reference.elements.begin(),r.reference.elements.end());
    need(Validate(r).accepted,"input ordering irrelevant");
    need(r.reference.elements.front().stable_id=="a0","preflight does not reorder/mutate");
    r=fixture();r.target.elements[1].stable_id="p0";rejected(r,"duplicate within one side");
    r=fixture();r.target.elements[0].stable_id="";rejected(r,"empty id");
    r=fixture();r.reference.elements[0].source_ref="";rejected(r,"missing element source");
    r=fixture();r.reference.source_ref="";rejected(r,"missing set source");
    r=fixture();r.request_id="bad\nidentifier";rejected(r,"control character in id");
    r=fixture();r.target.elements.clear();rejected(r,"empty set");
    r=fixture();r.target.elements[0].quality=1.1;rejected(r,"quality outside range");
    r=fixture();r.target.elements[0].quality=std::numeric_limits<double>::quiet_NaN();rejected(r,"NaN quality");
    r=fixture();r.target.elements[0].geometry=Point2{INFINITY,20};rejected(r,"nonfinite coordinate");
    r=fixture();r.target.elements[0].geometry=Point2{101,20};rejected(r,"point outside ROI");
    r=fixture();r.target.elements[0].geometry=LineSegment{{10,10},{10,10}};rejected(r,"zero length segment");
    r=fixture();r.target.elements[0].geometry=ArcSegment{{50,50},0,0,90};rejected(r,"invalid arc radius");
    r=fixture();r.target.elements[0].geometry=ArcSegment{{50,50},10,0,0};rejected(r,"zero sweep arc");
    r=fixture();r.target.elements[0].geometry=ArcSegment{{95,50},10,-90,180};
    rejected(r,"arc endpoints inside but middle outside");
    r=fixture();r.target.elements[0].geometry=ArcSegment{{105,50},10,120,120};
    need(Validate(r).accepted,"arc center may be outside when entire arc is inside");
    r=fixture();r.parameters.scale_min=2;rejected(r,"reversed scale range");
    r=fixture();r.parameters.angle_min_deg=181;rejected(r,"angle range invalid");
    r=fixture();r.parameters.max_elapsed_ms=0;rejected(r,"unbounded execution forbidden");
    r=fixture();r.parameters.max_pair_checks=0;rejected(r,"invalid work budget");
    r=fixture();r.parameters.min_candidate_score_gap=INFINITY;rejected(r,"nonfinite threshold");
    r=fixture();r.parameters.search_roi={-1,0,100,100};rejected(r,"search beyond target");
    r=fixture();r.parameters.max_elements_per_set=3;
    need(Validate(r).execution_status==ExecutionStatus::BudgetExhausted,"budget is not geometric insufficiency");
    rejected(r,"element budget");
    r=fixture();r.parameters.min_matched_elements=1;r.target.elements.resize(1);
    need(Validate(r).accepted && !Match(r).solvability,"valid tiny input does not establish solvability");
    auto t=contour();
    auto report=ContourTopologyValidator::Check(t);
    need(report.accepted&&report.topology==Topology::ClosedSingle&&!report.physical_boundary_verified,
         "verified structure is not physical provenance proof");
    t.parts[0].push_back(t.parts[0].front());need(ContourTopologyValidator::Check(t).accepted,"explicit closing vertex");
    t=contour();t.caller_confirmed=false;topologyReason(t,"SOURCE_CONFIRMATION_REQUIRED");
    t=contour();t.order_evidence_ref="";topologyReason(t,"CONTOUR_ORDER_UNVERIFIED");
    t=contour();t.closure_edge_observed=false;topologyReason(t,"FORCED_CLOSING_REJECTED");
    t=contour();t.complete=false;topologyReason(t,"FORCED_CLOSING_REJECTED");
    t=contour();t.parts={{{10,10},{30,30},{10,30},{30,10}}};
    topologyReason(t,"TOPOLOGY_MISMATCH_SELF_INTERSECTION");
    t=contour();t.parts={{{10,10},{30,10},{20,10},{20,30}}};
    topologyReason(t,"ADJACENT_EDGE_OVERLAP");
    t=contour();t.parts[0][1]=t.parts[0][0];topologyReason(t,"DUPLICATE_ADJACENT_POINTS");
    t=contour();t.holes=1;topologyReason(t,"TOPOLOGY_MISMATCH_HOLES");
    t=contour();t.branch=Branch::OpenCurve;t.closure_edge_observed=false;t.complete=false;
    need(ContourTopologyValidator::Check(t).accepted,"open chain remains open");
    t.parts[0].push_back(t.parts[0].front());topologyReason(t,"OPEN_ENDPOINTS_COINCIDENT");
    t=contour();t.representation=Representation::UnorderedPoints;
    topologyReason(t,"FORCED_CONNECTION_REJECTED");
    t.branch=Branch::GeometricSet;
    need(ContourTopologyValidator::Check(t).accepted,"set route does not invent connecting edges");
    t=contour();t.parts.push_back({{40,40},{50,40}});
    topologyReason(t,"TOPOLOGY_MISMATCH_MULTI_SEGMENT");
    t.branch=Branch::GeometricSet;
    need(ContourTopologyValidator::Check(t).topology==Topology::MultiSegment,"retain multi-part type");
    t=contour();TopologyPolicy p;p.maximum_pair_checks=1;
    need(ContourTopologyValidator::Check(t,p).execution_status==ExecutionStatus::BudgetExhausted,
         "topology work cap distinct from invalid geometry");
    p={};p.maximum_edge_length_px=5;topologyReason(t,"EDGE_GAP_POLICY_EXCEEDED",p);
    p={};p.maximum_points=3;
    need(ContourTopologyValidator::Check(t,p).execution_status==ExecutionStatus::BudgetExhausted,"point cap");
    t=contour();t.parts[0][0].x=NAN;topologyReason(t,"INVALID_TOPOLOGY_COORDINATE");
    // Declared point sets remain point sets even if their coordinates form a simple polygon.
    t=contour();t.representation=Representation::UnorderedPoints;
    std::reverse(t.parts[0].begin(),t.parts[0].end());topologyReason(t,"FORCED_CONNECTION_REJECTED");
    t=contour();t.representation=static_cast<Representation>(99);
    t.parts.push_back({{50,50},{60,60}});topologyReason(t,"UNKNOWN_TOPOLOGY_REPRESENTATION");
    if(argc>1) {
        std::ifstream input(argv[1]);const auto schema=nlohmann::json::parse(input);
        const auto& defs=schema.at("$defs");const auto& params=defs.at("parameters").at("properties");
        need(params.at("max_elements_per_set").at("default")==Parameters{}.max_elements_per_set,
             "schema default element budget");
        need(params.at("scale_min").at("default")==Parameters{}.scale_min,"schema default scale");
        need(defs.at("result").at("properties").at("production_eligible").at("const")==false,
             "schema does not permit production claim");
        need(defs.at("result").at("properties").at("execution_status").at("enum").size()==5,
             "execution statuses separate");
    }
    std::cout<<"{\"schema\":\"cxvision.gsm0.test_receipt.v1\",\"status\":\"PASS\",\"checks\":"
             <<checks<<",\"matching_implemented\":true,\"production_eligible\":false}\n";
    return 0;
} catch(const std::exception& e) {std::cerr<<"GSM0_FAIL: "<<e.what()<<"\n";return 1;}
