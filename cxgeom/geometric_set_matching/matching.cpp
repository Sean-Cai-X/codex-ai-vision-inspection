#include "types.h"
#include "pose_estimation.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <numeric>

namespace cxgeom::gsm {
namespace {
constexpr double rad=0.01745329251994329577;
double distance(Point2 a,Point2 b){return std::hypot(a.x-b.x,a.y-b.y);}
Point2 anchor(const Geometry& g) {
    if(const auto* p=std::get_if<Point2>(&g))return *p;
    if(const auto* l=std::get_if<LineSegment>(&g))
        return {(l->start.x+l->end.x)/2,(l->start.y+l->end.y)/2};
    return std::get<ArcSegment>(g).center;
}
Point2 at(const ArcSegment& a,double f) {
    const double t=(a.start_deg+f*a.sweep_deg)*rad;
    return {a.center.x+a.radius*std::cos(t),a.center.y+a.radius*std::sin(t)};
}
double residual(const Geometry& a,const Geometry& b,const Similarity2d& s) {
    if(a.index()!=b.index())return std::numeric_limits<double>::infinity();
    if(const auto* p=std::get_if<Point2>(&a))return distance(Transform(*p,s),std::get<Point2>(b));
    if(const auto* l=std::get_if<LineSegment>(&a)) {
        const auto& m=std::get<LineSegment>(b);
        const auto u=Transform(l->start,s),v=Transform(l->end,s);
        return std::min(std::max(distance(u,m.start),distance(v,m.end)),
                        std::max(distance(u,m.end),distance(v,m.start)));
    }
    const auto& u=std::get<ArcSegment>(a);const auto& v=std::get<ArcSegment>(b);
    const double base=std::max(distance(Transform(u.center,s),v.center),
                               std::abs(u.radius*s.scale-v.radius));
    if(std::abs(u.sweep_deg)==360 && std::abs(v.sweep_deg)==360)return base;
    // Radius-scaled sweep discrepancy prevents complementary arcs aliasing.
    const double sweep=std::abs(std::abs(u.sweep_deg)-std::abs(v.sweep_deg))*rad*v.radius;
    double direct=std::max(base,sweep),reverse=direct;
    for(double f:{0.,0.25,0.5,0.75,1.}) {
        const auto p=Transform(at(u,f),s);
        direct=std::max(direct,distance(p,at(v,f)));
        reverse=std::max(reverse,distance(p,at(v,1-f)));
    }
    return std::min(direct,reverse);
}
bool inside(Point2 p,Rect r) {
    return p.x>=r.x&&p.y>=r.y&&p.x<=r.x+r.width&&p.y<=r.y+r.height;
}
bool supportInside(const Geometry& g,Rect r) {
    if(const auto* p=std::get_if<Point2>(&g))return inside(*p,r);
    if(const auto* l=std::get_if<LineSegment>(&g))return inside(l->start,r)&&inside(l->end,r);
    const auto& a=std::get<ArcSegment>(g);
    if(!inside(at(a,0),r)||!inside(at(a,1),r))return false;
    for(double angle:{0.,90.,180.,270.}) {
        const double d=std::fmod((a.sweep_deg>0?angle-a.start_deg:a.start_deg-angle)+720.,360.);
        if(d<=std::abs(a.sweep_deg) &&
           !inside({a.center.x+a.radius*std::cos(angle*rad),
                    a.center.y+a.radius*std::sin(angle*rad)},r))return false;
    }
    return true;
}
bool allowed(Similarity2d& s,const Parameters& p) {
    // -180 and +180 denote the same rotation.
    if(std::abs(std::abs(s.angle_deg)-180)<1e-10) {
        if(p.angle_max_deg==180)s.angle_deg=180;
        else if(p.angle_min_deg==-180)s.angle_deg=-180;
    }
    return std::abs(s.translation.x)<=1e9&&std::abs(s.translation.y)<=1e9&&
           s.scale>=p.scale_min-1e-12&&s.scale<=p.scale_max+1e-12&&
           s.angle_deg>=p.angle_min_deg-1e-10&&s.angle_deg<=p.angle_max_deg+1e-10;
}
struct Budget {
    const Parameters& p;Result& out;
    std::chrono::steady_clock::time_point start=std::chrono::steady_clock::now();
    bool exhausted=false;
    bool tick() {
        out.elapsed_ms=std::chrono::duration<double,std::milli>(
            std::chrono::steady_clock::now()-start).count();
        if(out.pair_checks>=p.max_pair_checks||out.elapsed_ms>=p.max_elapsed_ms)
            return !(exhausted=true);
        ++out.pair_checks;return true;
    }
};
using Order=std::vector<std::size_t>;
Order order(const GeometricSet& s) {
    Order o(s.elements.size());std::iota(o.begin(),o.end(),0);
    std::sort(o.begin(),o.end(),[&](auto a,auto b){
        return s.elements[a].stable_id<s.elements[b].stable_id;});return o;
}
// Reciprocal unique nearest neighbours: never two references assigned to one target.
// Ties are refused rather than silently broken by array order.
bool correspond(const Request& r,const Order& ro,const Order& to,const Similarity2d& s,
                std::vector<std::size_t>& map,Budget& budget) {
    const auto n=ro.size();map.assign(n,n);
    std::vector<std::size_t> reverse(n,n);
    std::vector<double> best(n,std::numeric_limits<double>::infinity()),
                        back(n,std::numeric_limits<double>::infinity());
    std::vector<bool> tie(n,false),backtie(n,false);
    for(std::size_t i=0;i<n;++i)for(std::size_t j=0;j<n;++j) {
        if(!budget.tick())return false;
        const double d=residual(r.reference.elements[ro[i]].geometry,r.target.elements[to[j]].geometry,s);
        if(!std::isfinite(d)||d>r.parameters.max_residual_px)continue;
        if(d<best[i]-1e-12){best[i]=d;map[i]=j;tie[i]=false;}
        else if(std::abs(d-best[i])<=1e-12)tie[i]=true;
        if(d<back[j]-1e-12){back[j]=d;reverse[j]=i;backtie[j]=false;}
        else if(std::abs(d-back[j])<=1e-12)backtie[j]=true;
    }
    for(std::size_t i=0;i<n;++i)
        if(map[i]==n||tie[i]||backtie[map[i]]||reverse[map[i]]!=i)return false;
    return true;
}
}
Result Match(const Request& r) {
    Result out;out.request_id=r.request_id;
    const auto valid=Validate(r);
    if(!valid.accepted){out.execution_status=valid.execution_status;out.reasons=valid.reasons;return out;}
    Budget budget{r.parameters,out};
    out.evidence_refs={r.reference.source_ref,r.target.source_ref};
    const auto n=r.reference.elements.size();
    if(n!=r.target.elements.size()) {
        out.execution_status=ExecutionStatus::NotImplemented;
        out.reasons={"P1_REQUIRES_EQUAL_FULL_SETS_PARTIAL_MATCH_PENDING"};return out;
    }
    if(n<r.parameters.min_matched_elements||n<2) {
        out.execution_status=ExecutionStatus::NotImplemented;
        out.reasons={"P1_INSUFFICIENT_ANCHORS_NO_SOLVABILITY_CLAIM"};return out;
    }
    for(const auto& e:r.target.elements)if(!supportInside(e.geometry,r.parameters.search_roi)) {
        out.execution_status=ExecutionStatus::NotImplemented;
        out.reasons={"P1_FULL_TARGET_SUPPORT_REQUIRED_IN_SEARCH_ROI"};return out;
    }
    for(const auto* set:{&r.reference,&r.target})for(const auto& e:set->elements)
        if(e.quality<=0) {
            out.execution_status=ExecutionStatus::NotImplemented;
            out.reasons={"P1_ZERO_QUALITY_ELEMENT_UNSUPPORTED"};return out;
        }
    const auto ro=order(r.reference),to=order(r.target);
    std::size_t a=0,b=0;double span=0;
    for(std::size_t i=0;i<n;++i)for(std::size_t j=i+1;j<n;++j) {
        if(!budget.tick())break;
        const double d=distance(anchor(r.reference.elements[ro[i]].geometry),
                                anchor(r.reference.elements[ro[j]].geometry));
        if(d>span){span=d;a=i;b=j;}
    }
    if(!budget.exhausted&&span<=1e-9) {
        out.execution_status=ExecutionStatus::NotImplemented;
        out.reasons={"P1_COINCIDENT_REPRESENTATIVES_UNSUPPORTED"};return out;
    }
    std::size_t hypotheses=0;
    for(std::size_t i=0;i<n&&!budget.exhausted;++i)
    for(std::size_t j=0;j<n&&!budget.exhausted;++j) {
        if(!budget.tick())break;
        if(i==j)continue;
        const auto& ra=r.reference.elements[ro[a]].geometry;
        const auto& rb=r.reference.elements[ro[b]].geometry;
        const auto& ta=r.target.elements[to[i]].geometry;
        const auto& tb=r.target.elements[to[j]].geometry;
        if(ra.index()!=ta.index()||rb.index()!=tb.index())continue;
        if(hypotheses>=r.parameters.max_hypotheses){budget.exhausted=true;break;}
        ++hypotheses;
        auto pose=EstimateSimilarity({{anchor(ra),anchor(ta)},{anchor(rb),anchor(tb)}});
        if(!pose||!allowed(*pose,r.parameters))continue;
        std::vector<std::size_t> map;
        if(!correspond(r,ro,to,*pose,map,budget))continue;
        // One bounded least-squares refinement, then reciprocal reprojection validation.
        std::vector<std::pair<Point2,Point2>> pairs;
        for(std::size_t k=0;k<n;++k)
            pairs.push_back({anchor(r.reference.elements[ro[k]].geometry),
                             anchor(r.target.elements[to[map[k]]].geometry)});
        auto refined=EstimateSimilarity(pairs);
        if(!refined||!allowed(*refined,r.parameters)||
           !correspond(r,ro,to,*refined,map,budget))continue;
        Candidate c;c.pose=refined;c.weighted_coverage=1;c.spatial_span_ratio=1;
        for(std::size_t k=0;k<n;++k) {
            const auto& re=r.reference.elements[ro[k]];
            const auto& te=r.target.elements[to[map[k]]];
            const double d=residual(re.geometry,te.geometry,*refined);
            c.residual_px=std::max(c.residual_px,d);
            c.correspondences.push_back({re.stable_id,te.stable_id,
                std::min(re.quality,te.quality)*std::max(0.,1-d/r.parameters.max_residual_px),d});
        }
        bool duplicate=false;
        for(const auto& prior:out.candidates) {
            bool same=true;
            for(std::size_t k=0;k<n;++k)
                same=same&&prior.correspondences[k].target_id==c.correspondences[k].target_id;
            if(same){duplicate=true;break;}
        }
        if(!duplicate)out.candidates.push_back(std::move(c));
    }
    out.elapsed_ms=std::chrono::duration<double,std::milli>(
        std::chrono::steady_clock::now()-budget.start).count();
    if(budget.exhausted||out.elapsed_ms>=r.parameters.max_elapsed_ms) {
        out.execution_status=ExecutionStatus::BudgetExhausted;out.candidates.clear();
        out.reasons={"P1_SEARCH_BUDGET_EXHAUSTED_NO_UNIQUE_CLAIM"};return out;
    }
    out.execution_status=ExecutionStatus::Completed;out.search_complete=true;
    std::sort(out.candidates.begin(),out.candidates.end(),[](const Candidate& a,const Candidate& b){
        if(a.residual_px!=b.residual_px)return a.residual_px<b.residual_px;
        return a.pose->angle_deg<b.pose->angle_deg;});
    // P3 calibrated observability/solvability is deliberately not inferred here.
    out.reasons={out.candidates.empty()?"P1_NO_RECIPROCAL_FULL_SET_CANDIDATE":
        out.candidates.size()>1?"P1_MULTIPLE_FULL_SET_CANDIDATES":"P1_FULL_SET_CANDIDATE"};
    out.reasons.push_back("P1_FINITE_ANCHOR_SEARCH_ONLY_P3_SOLVABILITY_PENDING");
    return out;
}
}
