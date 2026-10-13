#pragma once
#include <algorithm>
#include <stdexcept>
#include <vector>
#include <cstddef>
namespace cxgeom {
struct BoundaryChunk {
 size_t core_begin=0,core_count=0,halo=0;
 std::vector<size_t> parent_indices;
 bool wraps_parent_origin=false;
};
// Index-only plan. Halo is observed support, never synthetic interpolation.
// Every original point owns exactly one core slot; halo must never be scored twice.
inline std::vector<BoundaryChunk> PlanClosedBoundaryChunks(size_t count,size_t core=100,size_t halo=3){
 if(count<3||count>4096||core==0||core>128||halo==0||halo>32||count<=2*halo+1)
  throw std::invalid_argument("BOUNDARY_CHUNK_CONFIG");
 core=std::min({core,size_t(128)-2*halo,count-1-2*halo});
 std::vector<BoundaryChunk> out;
 for(size_t start=0;start<count;start+=core){
  BoundaryChunk c;c.core_begin=start;c.core_count=std::min(core,count-start);c.halo=halo;
  for(size_t k=0;k<c.core_count+2*halo;++k){
   size_t index=(start+count-halo+k)%count;
   if(k&&index==0)c.wraps_parent_origin=true;
   c.parent_indices.push_back(index);
  }
  out.push_back(c);
 }
 return out;
}
struct BoundaryAcceptedRuns {
 std::vector<std::vector<size_t>> runs;
 bool observed_cycle_complete=false;
 bool seam_supported=false;
};
// Preserve every accepted index, including singleton runs. No invented joins.
// Full support is reported separately, not implicitly converted to closed output.
inline BoundaryAcceptedRuns OrderedAcceptedBoundaryRuns(const std::vector<bool>& accepted){
 if(accepted.size()<3||accepted.size()>4096)throw std::invalid_argument("BOUNDARY_RUN_COUNT");
 BoundaryAcceptedRuns out;out.seam_supported=accepted.front()&&accepted.back();
 auto rejected=std::find(accepted.begin(),accepted.end(),false);
 out.observed_cycle_complete=rejected==accepted.end();
 size_t start=out.observed_cycle_complete?0:(size_t(rejected-accepted.begin())+1)%accepted.size();
 std::vector<size_t> run;
 for(size_t k=0;k<accepted.size();++k){size_t i=(start+k)%accepted.size();
  if(accepted[i])run.push_back(i);else if(!run.empty()){out.runs.push_back(run);run.clear();}
 }
 if(!run.empty())out.runs.push_back(run);
 return out;
}
}
