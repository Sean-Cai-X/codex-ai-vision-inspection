#pragma once
#include <algorithm>
#include <cmath>
#include <complex>
#include <stdexcept>
#include <vector>

namespace cxgeom::polar {
// Diagnostic proposal only. Indices refer to the immutable caller point order.
struct OrderedNeighborConfig {
 size_t window=4, minimum_neighbors=2, minimum_segment_points=3;
 double support_spacing_factor=3., gap_spacing_factor=4., minimum_retained_fraction=.75;
};
struct OrderedNeighborDecision {
 size_t input_index=0, support_count=0;
 bool proposed_keep=true, endpoint_protected=false;
};
struct OrderedNeighborProposal {
 double median_spacing=0, retained_fraction=0;
 bool coverage_ok=false;
 std::vector<OrderedNeighborDecision> points;
 std::vector<size_t> gap_after;
 std::vector<std::vector<size_t>> segments;
};
inline OrderedNeighborProposal ProposeOrderedNeighborFilter(
 const std::vector<std::complex<double>>& points,const OrderedNeighborConfig& c={}) {
 if(c.window<1||c.window>128||c.minimum_neighbors<1||c.minimum_neighbors>2*c.window||
    c.minimum_segment_points<2||!std::isfinite(c.support_spacing_factor)||c.support_spacing_factor<=0||
    !std::isfinite(c.gap_spacing_factor)||c.gap_spacing_factor<=0||
    !std::isfinite(c.minimum_retained_fraction)||c.minimum_retained_fraction<=0||c.minimum_retained_fraction>1)
  throw std::invalid_argument("NEIGHBOR_INVALID_CONFIG");
 if(points.size()<3||points.size()>4096)throw std::invalid_argument("NEIGHBOR_POINT_BUDGET");
 std::vector<double> edges;
 for(size_t i=0;i<points.size();++i){
  if(!std::isfinite(points[i].real())||!std::isfinite(points[i].imag()))throw std::invalid_argument("NEIGHBOR_NONFINITE");
  if(i){double length=std::abs(points[i]-points[i-1]);
   if(!std::isfinite(length)||length<=1e-9)throw std::invalid_argument("NEIGHBOR_DEGENERATE_EDGE");
   edges.push_back(length);}
 }
 auto sorted=edges;std::sort(sorted.begin(),sorted.end());
 OrderedNeighborProposal out;out.median_spacing=sorted[sorted.size()/2];
 const double radius=out.median_spacing*c.support_spacing_factor;
 for(size_t i=0;i<edges.size();++i)if(edges[i]>out.median_spacing*c.gap_spacing_factor)out.gap_after.push_back(i);
 for(size_t i=0;i<points.size();++i){
  size_t count=0;const size_t lo=i>c.window?i-c.window:0,hi=std::min(points.size()-1,i+c.window);
  for(size_t j=lo;j<=hi;++j)if(i!=j){
   const size_t a=std::min(i,j),b=std::max(i,j);
   const auto gap=std::lower_bound(out.gap_after.begin(),out.gap_after.end(),a);
   if(gap!=out.gap_after.end()&&*gap<b)continue;
   if(std::abs(points[i]-points[j])<=radius)++count;
  }
  const bool endpoint=i==0||i+1==points.size();
  out.points.push_back({i,count,endpoint||count>=c.minimum_neighbors,endpoint});
 }
 std::vector<size_t> run;
 auto flush=[&](){if(!run.empty())out.segments.push_back(run);run.clear();};
 for(size_t i=0;i<points.size();++i){
  if(!out.points[i].proposed_keep)flush();else run.push_back(i);
  if(std::binary_search(out.gap_after.begin(),out.gap_after.end(),i))flush();
 }flush();
 size_t retained=0;for(const auto& p:out.points)if(p.proposed_keep)++retained;
 out.retained_fraction=double(retained)/double(points.size());
 out.coverage_ok=out.retained_fraction>=c.minimum_retained_fraction;
 // Short segments remain in evidence; the consumer must not bridge them.
 return out;
}
}
