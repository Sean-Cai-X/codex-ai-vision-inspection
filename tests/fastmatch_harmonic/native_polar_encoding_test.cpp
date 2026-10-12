#include "../../cxgeom/polar_discrete_harmonic/encoding.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace cxgeom::polar;
using Z=std::complex<double>;
constexpr double pi=3.14159265358979323846;
void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
void reject(const std::function<void()>& f,const char* why){
 try{f();}catch(const std::invalid_argument& e){
  check(std::string(e.what())==why,e.what());std::cout<<"REJECT "<<why<<std::endl;return;
 }throw std::runtime_error("missing rejection");
}
int main()try{
 std::vector<Feature> f={
  {"a","edge0",{-3,-1},1,.1,.2,true},
  {"b","edge0",{1,-2},.8,.2,.3,true},
  {"c","edge1",{4,1},.6,0,0,false},
  {"d","edge2",{-1,3},1,0,0,false},
  {"e","edge2",{0,-1},.7,0,0,false}};
 const auto original=f;
 auto d=Encode("source",f);
 check(!d.geometry_is_contour&&!d.production_eligible,"encoding only");
 check(d.features[0].source_element_id=="edge0"&&d.features[0].curvature==.2,"attributes retained");
 int cases=0;
 for(double angle:{-179.5,23.4,179.5})for(double scale:{.8,1.2}){
  auto g=f;Z rotation=std::polar(scale,angle*pi/180),translation(17.25,-8.5);
  for(auto& v:g){v.point=rotation*v.point+translation;v.local_direction_rad+=angle*pi/180;}
  auto e=Encode("target",g);
  check(std::abs(e.centroid-(rotation*d.centroid+translation))<1e-12,"centroid covariance");
  check(std::abs(e.rms_radius-scale*d.rms_radius)<1e-12,"scale covariance");
  for(int c=0;c<3;++c)for(int k=0;k<=d.config.max_order;++k)
   check(std::abs(e.moments[c][k]-d.moments[c][k]*std::polar(1.,-k*angle*pi/180))<1e-12,"rotation phase law");
  ++cases;
 }
 for(int i=0;i<100;++i){
  std::rotate(f.begin(),f.begin()+1,f.end());auto e=Encode("source",f);
  check(e.moments==d.moments&&e.angular_signal==d.angular_signal&&e.centroid==d.centroid,"permutation determinism");
 }
 auto missing=original;missing.erase(missing.begin());auto partial=Encode("partial",missing);
 check(std::abs(partial.centroid-d.centroid)>.1,"missing feature changes origin");
 std::vector<Feature> sameRay={
  {"a","a",{1,0}},{"b","b",{2,0}},{"c","c",{-1,0}},{"d","d",{-2,0}},
  {"e","e",{0,1}},{"f","f",{0,-1}},{"g","g",{0,0}}};
 auto rays=Encode("rays",sameRay);
 check(rays.features.size()==7&&std::abs(rays.central_weight_fraction-1./7)<1e-12,"central point retained");
 check(std::abs(rays.moments[0][0].real()-6./7)<1e-12,"same ray mass retained");
 check(std::abs(rays.moments[2][0].real()-1)<1e-12,"second radial moment retained");
 for(int c=0;c<3;++c){
  double sum=0;for(double v:rays.angular_signal[c])sum+=v;
  check(std::abs(sum-rays.moments[c][0].real())<1e-12,"signal mass conservation");
 }
 reject([&]{auto x=original;x[1].stable_id=x[0].stable_id;Encode("x",x);},"POLAR_INVALID_FEATURE_ID");
 reject([&]{auto x=original;x[0].point={std::numeric_limits<double>::quiet_NaN(),0};Encode("x",x);},"POLAR_INVALID_POINT");
 reject([&]{auto x=original;x[0].weight=0;Encode("x",x);},"POLAR_INVALID_WEIGHT");
 reject([&]{auto x=original;for(auto& v:x)v.point={1,1};Encode("x",x);},"POLAR_DEGENERATE_SCALE");
 reject([&]{Config c;c.bins=16;c.max_order=16;Encode("x",original,c);},"POLAR_INVALID_SAMPLING");
 reject([&]{Config c;c.minimum_rms_radius=0;Encode("x",original,c);},"POLAR_INVALID_RADIUS_LIMIT");
 reject([&]{Encode("",original);},"POLAR_INVALID_SOURCE");
 reject([&]{auto x=original;x[0].curvature=std::numeric_limits<double>::infinity();Encode("x",x);},"POLAR_INVALID_ATTRIBUTE");
 std::cout<<"POLAR_ENCODING_PASS transforms="<<cases<<" permutations=100; no pose/solvability claim"<<std::endl;
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}
