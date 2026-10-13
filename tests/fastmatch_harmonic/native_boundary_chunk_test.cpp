#include "CxGeoBoundaryChunkPlan.h"
#include <iostream>
void need(bool b,const char*s){if(!b)throw std::runtime_error(s);}
int main()try{
 size_t plans=0;
 for(size_t n:{8,9,99,100,101,127,128,129,199,200,201,4096})for(size_t core:{1,17,100,128}){
  auto chunks=cxgeom::PlanClosedBoundaryChunks(n,core,3);std::vector<int> seen(n);
  for(auto c:chunks){need(c.parent_indices.size()<=128&&c.parent_indices.size()<n,"budget");
   for(size_t k=0;k<c.parent_indices.size();++k){if(k)need(c.parent_indices[k]==(c.parent_indices[k-1]+1)%n,"order");
    if(k>=c.halo&&k<c.halo+c.core_count)++seen[c.parent_indices[k]];
   }}
  for(auto v:seen)need(v==1,"unique core ownership");++plans;
 }
 for(size_t n:{3,8,15})for(size_t bits=0;bits<(size_t(1)<<n);++bits){
  std::vector<bool>a(n);size_t count=0;for(size_t i=0;i<n;++i){a[i]=(bits>>i)&1;count+=a[i];}
  auto r=cxgeom::OrderedAcceptedBoundaryRuns(a);std::vector<int>seen(n);
  for(auto run:r.runs)for(size_t k=0;k<run.size();++k){size_t i=run[k];need(a[i],"rejected in run");++seen[i];if(k)need(i==(run[k-1]+1)%n,"gap joined");}
  for(size_t i=0;i<n;++i)need(seen[i]==int(a[i]),"lost or duplicate accepted");
  need(r.observed_cycle_complete==(count==n),"cycle fact");need(r.seam_supported==(a[0]&&a[n-1]),"seam fact");
 }
 for(size_t n:{0,3,7,4097}){bool failed=false;try{cxgeom::PlanClosedBoundaryChunks(n);}catch(const std::invalid_argument&){failed=true;}need(failed,"invalid plan accepted");}
 std::cout<<"BOUNDARY_CHUNK_GUARDS_PASS plans="<<plans<<" exhaustive_acceptance_patterns=33032\n";
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}
