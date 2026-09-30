#include "CxGeoOpenBoundary.h"
#include <functional>
#include <algorithm>
#include <iostream>
#include <limits>
using namespace cxgeom;
void check(bool v){if(!v)throw std::runtime_error("open boundary check failed");}
void reject(const std::function<void()>& f,const char* reason){
 try{f();}catch(const std::invalid_argument& e){check(std::string(e.what())==reason);return;}
 throw std::runtime_error("missing rejection");
}
int main(){
 const std::vector<std::complex<double>> p={{0,0},{1,0},{2,0},{2,1},{2,2},{1,2},{0,2},{0,1}};
 auto wrap=ExtractOpenBoundaryRun(p,0,0,.5,1.5,2);
 check(wrap.parent_indices==std::vector<size_t>({7,0}));
 check(wrap.points.front()==p[7]&&wrap.points.back()==p[0]&&wrap.wraps_parent_origin);
 auto top=ExtractOpenBoundaryRun(p,0,0,3,.5,3);
 check(top.parent_indices==std::vector<size_t>({0,1,2}));
 check(top.points.front()==p[0]&&top.points.back()==p[2]&&!top.wraps_parent_origin);
 reject([&]{ExtractOpenBoundaryRun(p,0,0,3,3);},"SUBCURVE_WHOLE_CLOSED_CONTOUR");
 reject([&]{ExtractOpenBoundaryRun(p,0,.5,3,1);},"SUBCURVE_AMBIGUOUS_RUNS");
 reject([&]{ExtractOpenBoundaryRun(p,4,4,1,1);},"SUBCURVE_NO_BOUNDARY");
 reject([&]{ExtractOpenBoundaryRun(p,0,0,.5,.5,2);},"SUBCURVE_INSUFFICIENT_POINTS");
 reject([&]{ExtractOpenBoundaryRun(p,0,0,0,1);},"INVALID_SUBCURVE_ANCHOR");
 reject([&]{ExtractOpenBoundaryRun(p,0,0,1,1,1);},"INVALID_SUBCURVE_MINIMUM");
 auto bad=p;bad[1]={std::numeric_limits<double>::quiet_NaN(),0};
 reject([&]{ExtractOpenBoundaryRun(bad,0,0,3,.5);},"NONFINITE_SUBCURVE_PARENT");
 auto reverse=p;std::reverse(reverse.begin(),reverse.end());
 auto reversed=ExtractOpenBoundaryRun(reverse,0,0,3,.5,3);
 check(reversed.points.front()==p[2]&&reversed.points.back()==p[0]);
 std::cout<<"native open boundary guards PASS\n";
}
