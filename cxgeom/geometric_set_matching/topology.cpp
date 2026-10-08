#include "topology.h"
#include <algorithm>
#include <cmath>

namespace cxgeom::gsm {
namespace {
double distance(Point2 a,Point2 b) {return std::hypot(a.x-b.x,a.y-b.y);}
double cross(Point2 a,Point2 b,Point2 p) {
    return (b.x-a.x)*(p.y-a.y)-(b.y-a.y)*(p.x-a.x);
}
int side(Point2 a,Point2 b,Point2 p,double e) {
    const double z=cross(a,b,p),limit=e*distance(a,b);
    return z>limit?1:(z< -limit?-1:0);
}
bool on(Point2 a,Point2 b,Point2 p,double e) {
    return side(a,b,p,e)==0&&p.x>=std::min(a.x,b.x)-e&&p.x<=std::max(a.x,b.x)+e&&
           p.y>=std::min(a.y,b.y)-e&&p.y<=std::max(a.y,b.y)+e;
}
bool intersects(Point2 a,Point2 b,Point2 c,Point2 d,double e) {
    const int s1=side(a,b,c,e),s2=side(a,b,d,e),s3=side(c,d,a,e),s4=side(c,d,b,e);
    return (s1*s2<0&&s3*s4<0)||on(a,b,c,e)||on(a,b,d,e)||on(c,d,a,e)||on(c,d,b,e);
}
bool supported(Branch b) {
    return b==Branch::ClosedContour||b==Branch::OpenCurve||b==Branch::GeometricSet;
}
}
TopologyReport ContourTopologyValidator::Check(const TopologyInput& in,const TopologyPolicy& p) {
    TopologyReport r;r.source_ref=in.source_ref;r.order_evidence_ref=in.order_evidence_ref;
    auto fail=[&](const char* reason){r.reasons.push_back(reason);return r;};
    if(!supported(in.branch)||!std::isfinite(p.epsilon_px)||p.epsilon_px<=0||
       !std::isfinite(p.maximum_edge_length_px)||p.maximum_edge_length_px<=p.epsilon_px||
       p.maximum_points<2||p.maximum_points>8192||p.maximum_pair_checks==0||p.maximum_pair_checks>100000000)
        return fail("INVALID_TOPOLOGY_POLICY");
    if(in.representation!=Representation::OrderedChain &&
       in.representation!=Representation::UnorderedPoints &&
       in.representation!=Representation::SegmentCollection)
        return fail("UNKNOWN_TOPOLOGY_REPRESENTATION");
    if(in.parts.empty())return fail("EMPTY_TOPOLOGY_INPUT");
    for(const auto& part:in.parts) {
        if(part.empty())return fail("EMPTY_TOPOLOGY_PART");
        if(part.size()>p.maximum_points-r.point_count) {
            r.execution_status=ExecutionStatus::BudgetExhausted;return fail("TOPOLOGY_POINT_BUDGET_EXCEEDED");
        }
        r.point_count+=part.size();
        for(auto v:part)if(!std::isfinite(v.x)||!std::isfinite(v.y)||std::abs(v.x)>1e9||std::abs(v.y)>1e9)
            return fail("INVALID_TOPOLOGY_COORDINATE");
    }
    if(in.source_ref.empty()||!in.caller_confirmed)return fail("SOURCE_CONFIRMATION_REQUIRED");
    if(in.representation==Representation::UnorderedPoints) {
        r.topology=Topology::ScatteredPoints;
        if(in.branch!=Branch::GeometricSet)return fail("FORCED_CONNECTION_REJECTED");
    } else if(in.representation==Representation::SegmentCollection || in.parts.size()>1) {
        r.topology=Topology::MultiSegment;
        if(in.branch!=Branch::GeometricSet)return fail("TOPOLOGY_MISMATCH_MULTI_SEGMENT");
    } else if(in.representation==Representation::OrderedChain) {
        if(in.branch==Branch::GeometricSet)return fail("EXPLICIT_SET_ELEMENTS_REQUIRED");
        if(in.order_evidence_ref.empty())return fail("CONTOUR_ORDER_UNVERIFIED");
        const bool closed=in.branch==Branch::ClosedContour;
        r.topology=closed?Topology::ClosedSingle:Topology::OpenSingle;
        if(closed&&(!in.closure_edge_observed||!in.complete))
            return fail("FORCED_CLOSING_REJECTED");
        if(!closed&&in.closure_edge_observed)return fail("TOPOLOGY_MISMATCH_CLOSED_AS_OPEN");
        if(closed&&in.holes)return fail("TOPOLOGY_MISMATCH_HOLES");
        auto points=in.parts.front();
        if(closed&&points.size()>1&&distance(points.front(),points.back())<=p.epsilon_px)points.pop_back();
        if(points.size()<(closed?3u:2u))return fail("INSUFFICIENT_TOPOLOGY_POINTS");
        if(!closed&&distance(points.front(),points.back())<=p.epsilon_px)return fail("OPEN_ENDPOINTS_COINCIDENT");
        const std::size_t n=points.size(),edges=closed?n:n-1;
        for(std::size_t i=0;i<edges;++i) {
            const double len=distance(points[i],points[(i+1)%n]);
            if(len<=p.epsilon_px)return fail("DUPLICATE_ADJACENT_POINTS");
            if(len>p.maximum_edge_length_px)return fail("EDGE_GAP_POLICY_EXCEEDED");
        }
        for(std::size_t i=0;i<edges;++i) {
            const std::size_t next=(i+1)%edges;
            if(closed||i+1<edges) {
                // Adjacent segments may share a vertex but may not backtrack/overlap.
                auto a=points[i],b=points[(i+1)%n],c=points[(i+2)%n];
                if(side(a,b,c,p.epsilon_px)==0 &&
                   (b.x-a.x)*(c.x-b.x)+(b.y-a.y)*(c.y-b.y)<0)
                    return fail("ADJACENT_EDGE_OVERLAP");
            }
            for(std::size_t j=i+1;j<edges;++j) {
                if(j==next||(closed&&i==0&&j+1==edges))continue;
                if(r.pair_checks>=p.maximum_pair_checks) {
                    r.execution_status=ExecutionStatus::BudgetExhausted;return fail("TOPOLOGY_PAIR_BUDGET_EXCEEDED");
                }
                ++r.pair_checks;
                if(intersects(points[i],points[(i+1)%n],points[j],points[(j+1)%n],p.epsilon_px))
                    return fail("TOPOLOGY_MISMATCH_SELF_INTERSECTION");
            }
        }
    } else return fail("UNKNOWN_TOPOLOGY_REPRESENTATION");
    r.accepted=true;r.execution_status=ExecutionStatus::NotRun;
    return r;
}
const char* Name(Topology t) {
    switch(t) {
    case Topology::Unverified:return "UNVERIFIED";
    case Topology::ClosedSingle:return "CLOSED_SINGLE";
    case Topology::OpenSingle:return "OPEN_SINGLE";
    case Topology::MultiSegment:return "MULTI_SEGMENT";
    case Topology::ScatteredPoints:return "SCATTERED_POINTS";
    }
    return "UNKNOWN";
}
}
