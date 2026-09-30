#pragma once
#include <cmath>
#include <complex>
#include <stdexcept>
#include <vector>
namespace cxgeom {
struct OpenBoundaryRun {
    std::vector<std::complex<double>> points;
    std::vector<size_t> parent_indices;
    size_t parent_point_count=0;
    bool wraps_parent_origin=false;
};
inline OpenBoundaryRun ExtractOpenBoundaryRun(
    const std::vector<std::complex<double>>& parent,double x,double y,double w,double h,int minimum=3) {
    auto need=[](bool ok,const char* why){if(!ok)throw std::invalid_argument(why);};
    need(std::isfinite(x)&&std::isfinite(y)&&std::isfinite(w)&&std::isfinite(h)&&
         x>=0&&y>=0&&w>0&&h>0&&std::isfinite(x+w)&&std::isfinite(y+h),"INVALID_SUBCURVE_ANCHOR");
    need(minimum>=2&&minimum<=4096,"INVALID_SUBCURVE_MINIMUM");
    need(parent.size()>=3 && parent.size()<=4096,"INVALID_SUBCURVE_PARENT_SIZE");
    std::vector<bool> hits;size_t count=0,start=0,runs=0;
    for(auto p:parent){
        need(std::isfinite(p.real())&&std::isfinite(p.imag()),"NONFINITE_SUBCURVE_PARENT");
        bool hit=p.real()>=x&&p.real()<x+w&&p.imag()>=y&&p.imag()<y+h;
        hits.push_back(hit);if(hit)++count;
    }
    need(count>0,"SUBCURVE_NO_BOUNDARY");
    need(count<parent.size(),"SUBCURVE_WHOLE_CLOSED_CONTOUR");
    for(size_t i=0;i<parent.size();++i)
        if(hits[i]&&!hits[(i+parent.size()-1)%parent.size()]){++runs;start=i;}
    need(runs==1,"SUBCURVE_AMBIGUOUS_RUNS");
    need(count>=static_cast<size_t>(minimum),"SUBCURVE_INSUFFICIENT_POINTS");
    OpenBoundaryRun result;result.parent_point_count=parent.size();
    for(size_t k=0;k<count;++k){
        size_t i=(start+k)%parent.size();result.parent_indices.push_back(i);result.points.push_back(parent[i]);
    }
    result.wraps_parent_origin=start+count>parent.size();
    need(std::abs(result.points.front()-result.points.back())>1e-9,"SUBCURVE_COINCIDENT_ENDPOINTS");
    return result; // No sorting, reversal, fitting, interpolation, or closing edge.
}
}
