#include "CxGeoCornerSupport.h"
#include <stdexcept>
#include <iostream>
using Z=std::complex<double>;
void need(bool b){if(!b)throw std::runtime_error("corner guard");}
int main(){
 for(double a:{0.,.65,-1.2})for(double scale:{.75,1.,1.25}){
  Z origin(17,23),rot=std::polar(scale,a);std::vector<Z> l,r;
  for(int i=2;i<10;++i){l.push_back(origin+rot*Z(-i,0));r.push_back(origin+rot*Z(0,i));}
  auto x=cxgeom::EstimateCornerIntersection(l,r);need(x.accepted&&std::abs(x.point-origin)<1e-10);
 }
 std::vector<Z> a,b;for(int i=0;i<8;++i){a.emplace_back(i,0);b.emplace_back(i,2);}
 need(cxgeom::EstimateCornerIntersection(a,b).status=="ILL_CONDITIONED_INTERSECTION");
 b.clear();for(int i=20;i<28;++i)b.emplace_back(0,i);
 need(cxgeom::EstimateCornerIntersection(a,b).status=="EXTRAPOLATION_BUDGET");
 need(!cxgeom::EstimateCornerIntersection({},a).accepted);
 a[0]=Z(NAN,0);need(!cxgeom::EstimateCornerIntersection(a,b).accepted);
 std::cout<<"CORNER_SUPPORT_GUARDS_PASS poses=9 rejections=4\n";
}
