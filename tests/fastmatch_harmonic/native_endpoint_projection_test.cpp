#include "../../cxgeom/polar_discrete_harmonic/endpoint_projected_arc.h"
#include <iostream>
using namespace cxgeom::polar;
void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
template<class F>void rejects(F f,const char* why){bool caught=false;try{f();}catch(const std::invalid_argument& e){caught=std::string(e.what())==why;}check(caught,why);}
int main()try{
 const std::vector<std::complex<double>> points={{0,0},{1,0},{2,0},{3,0},{4,0}};
 auto c=ProjectEndpointCoordinates(points,{0,0},{4,0},.3);check(c.valid(),"linear");
 auto reverse=ProjectEndpointCoordinates(points,{4,0},{0,0},.3);check(reverse.valid()&&reverse.reversed,"explicit reverse");
 auto d=IntersectProjectedSupport(c,reverse,.25);check(d.accepted(),"shared support");
 auto slice=common_arc_detail::SampleProjectedSlice(points,points,0,4,c,d,16);
 auto back=common_arc_detail::SampleProjectedSlice(points,points,0,4,reverse,d,16);
 check(slice.refined.front()==points.front()&&back.refined.front()==points.back(),"source order");
 for(size_t k=0;k<back.refined_samples.values.size();++k){
  const auto& v=back.refined_samples.values[k];auto p=points[v.edge]+v.fraction*(points[v.edge+1]-points[v.edge]);
  check(std::abs(p-v.point)<1e-12,"reversed source interpolation");
 }
 auto curve=points;curve[2]={2,1};
 auto bent=ProjectEndpointCoordinates(curve,{0,0},{4,0},.3);check(bent.status=="ENDPOINT_PROJECTION_NONLINEAR_SUPPORT","curve rejected");
 check(!IntersectProjectedSupport(c,bent,.25).accepted(),"curved no shared pose");
 auto fold=points;fold[2]={.5,0};
 check(ProjectEndpointCoordinates(fold,{0,0},{4,0},.3).status=="ENDPOINT_PROJECTION_NONMONOTONIC_SUPPORT","fold rejected");
 auto outside=ProjectEndpointCoordinates(points,{5,0},{9,0},.3);
 check(IntersectProjectedSupport(c,outside,.25).status=="COMMON_ARC_EMPTY_INTERSECTION","outside anchors empty");
 check(IntersectProjectedSupport(ProjectEndpointCoordinates(points,{0,0},{8,0},.3),c,.75).status=="COMMON_ARC_INSUFFICIENT_COVERAGE","coverage guard");
 rejects([&]{ProjectEndpointCoordinates(points,{0,0},{0,0},.3);},"ENDPOINT_PROJECTION_COLLAPSED_ANCHORS");
 rejects([&]{ProjectEndpointCoordinates(points,{0,0},{4,0},0);},"ENDPOINT_PROJECTION_INVALID_CONFIG");
 auto clipped=ProjectEndpointCoordinates({{1,0},{2,0},{3,0}},{0,0},{4,0},.3);
 rejects([&]{common_arc_detail::SampleProjectedSlice(points,{{1,0},{2,0},{3,0}},1,3,clipped,d,16);},"COMMON_ARC_CROSSES_REJECTED_GAP");
 std::cout<<"ENDPOINT_PROJECTION_GUARDS_PASS explicit endpoints / reversed provenance / curve / fold / empty / coverage / gap\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
