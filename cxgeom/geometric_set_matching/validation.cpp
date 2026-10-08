#include "types.h"
#include <algorithm>
#include <cmath>
#include <set>

namespace cxgeom::gsm {
namespace {
bool coordinate(double x) { return std::isfinite(x) && std::abs(x)<=1e9; }
bool point(Point2 p) { return coordinate(p.x)&&coordinate(p.y); }
bool fraction(double x) { return std::isfinite(x)&&x>=0&&x<=1; }
bool id(const std::string& value) {
    if(value.empty()||value.size()>256) return false;
    for(unsigned char ch:value) if(ch<32||ch==127) return false;
    return true;
}
bool rect(Rect r) {
    return coordinate(r.x)&&coordinate(r.y)&&std::isfinite(r.width)&&std::isfinite(r.height)&&
        r.width>0&&r.height>0&&coordinate(r.x+r.width)&&coordinate(r.y+r.height);
}
bool inside(Point2 p, Rect r) {
    return point(p)&&p.x>=r.x&&p.y>=r.y&&p.x<=r.x+r.width&&p.y<=r.y+r.height;
}
bool arcInside(const ArcSegment& a, Rect r) {
    if(!point(a.center)||!std::isfinite(a.radius)||a.radius<=0||a.radius>1e9||
       !std::isfinite(a.start_deg)||a.start_deg< -180||a.start_deg>180||
       !std::isfinite(a.sweep_deg)||a.sweep_deg==0||std::abs(a.sweep_deg)>360) return false;
    constexpr double radians=0.01745329251994329577;
    auto at=[&](double deg){return Point2{a.center.x+a.radius*std::cos(deg*radians),
                                      a.center.y+a.radius*std::sin(deg*radians)};};
    if(!inside(at(a.start_deg),r)||!inside(at(a.start_deg+a.sweep_deg),r)) return false;
    for(double cardinal:{0.,90.,180.,270.}) {
        double d=std::fmod((a.sweep_deg>0?cardinal-a.start_deg:a.start_deg-cardinal)+720.,360.);
        if(d<=std::abs(a.sweep_deg)&&!inside(at(cardinal),r)) return false;
    }
    return true;
}
void validateSet(const GeometricSet& s,const char* side,std::vector<std::string>& errors) {
    const std::string prefix=std::string(side)+":";
    if(!id(s.set_id)||!id(s.source_ref)) errors.push_back(prefix+"MISSING_OR_INVALID_PROVENANCE");
    if(!rect(s.roi)) {errors.push_back(prefix+"INVALID_ROI");return;}
    if(s.elements.empty()) errors.push_back(prefix+"EMPTY_SET");
    std::set<std::string> ids;
    for(const auto& e:s.elements) {
        if(!id(e.stable_id)||!id(e.source_ref)) errors.push_back(prefix+"INVALID_ELEMENT_ID_OR_SOURCE");
        if(!ids.insert(e.stable_id).second) errors.push_back(prefix+"DUPLICATE_STABLE_ID");
        if(!fraction(e.quality)) errors.push_back(prefix+"INVALID_ELEMENT_QUALITY");
        bool valid=false;
        if(const auto* p=std::get_if<Point2>(&e.geometry)) valid=inside(*p,s.roi);
        else if(const auto* l=std::get_if<LineSegment>(&e.geometry))
            valid=inside(l->start,s.roi)&&inside(l->end,s.roi)&&
                  std::hypot(l->end.x-l->start.x,l->end.y-l->start.y)>1e-9;
        else if(const auto* a=std::get_if<ArcSegment>(&e.geometry)) valid=arcInside(*a,s.roi);
        if(!valid) errors.push_back(prefix+"INVALID_OR_OUTSIDE_ROI_GEOMETRY");
    }
}
}
Validation Validate(const Request& r) {
    Validation v;
    const auto& p=r.parameters;
    if(!id(r.request_id)) v.reasons.push_back("INVALID_REQUEST_ID");
    if(!rect(p.search_roi)||!std::isfinite(p.angle_min_deg)||!std::isfinite(p.angle_max_deg)||
       p.angle_min_deg< -180||p.angle_max_deg>180||p.angle_min_deg>p.angle_max_deg||
       !std::isfinite(p.scale_min)||!std::isfinite(p.scale_max)||p.scale_min<=0||
       p.scale_max>1e6||p.scale_min>p.scale_max||
       !std::isfinite(p.max_residual_px)||p.max_residual_px<=0||
       !fraction(p.min_weighted_coverage)||p.min_weighted_coverage==0||
       !fraction(p.min_spatial_span_ratio)||!fraction(p.min_candidate_score_gap)||
       p.min_matched_elements==0||p.max_elements_per_set==0||p.max_elements_per_set>8192||
       p.min_matched_elements>p.max_elements_per_set||p.max_hypotheses==0||p.max_hypotheses>4096||
       p.max_pair_checks==0||p.max_pair_checks>100000000||p.max_elapsed_ms==0||p.max_elapsed_ms>60000) {
        v.reasons.push_back("INVALID_PARAMETERS");return v;
    }
    if(!v.reasons.empty())return v;
    if(r.reference.elements.size()>p.max_elements_per_set||r.target.elements.size()>p.max_elements_per_set) {
        v.execution_status=ExecutionStatus::BudgetExhausted;
        v.reasons.push_back("ELEMENT_BUDGET_EXCEEDED");return v;
    }
    validateSet(r.reference,"REFERENCE",v.reasons);
    validateSet(r.target,"TARGET",v.reasons);
    const auto& roi=r.target.roi;
    if(!inside({p.search_roi.x,p.search_roi.y},roi)||
       !inside({p.search_roi.x+p.search_roi.width,p.search_roi.y+p.search_roi.height},roi))
        v.reasons.push_back("SEARCH_ROI_OUTSIDE_TARGET_ROI");
    v.accepted=v.reasons.empty();
    v.execution_status=v.accepted?ExecutionStatus::NotRun:ExecutionStatus::InvalidInput;
    return v;
}
const char* Name(ExecutionStatus s) {
    switch(s) {
    case ExecutionStatus::NotRun:return "NOT_RUN";
    case ExecutionStatus::InvalidInput:return "INVALID_INPUT";
    case ExecutionStatus::BudgetExhausted:return "BUDGET_EXHAUSTED";
    case ExecutionStatus::NotImplemented:return "NOT_IMPLEMENTED";
    case ExecutionStatus::Completed:return "COMPLETED";
    }
    return "UNKNOWN";
}
const char* Name(Solvability s) {
    switch(s) {
    case Solvability::FullySolvable:return "FULLY_SOLVABLE";
    case Solvability::PartiallySolvable:return "PARTIALLY_SOLVABLE";
    case Solvability::Ambiguous:return "AMBIGUOUS";
    case Solvability::Insufficient:return "INSUFFICIENT";
    }
    return "UNKNOWN";
}
}
