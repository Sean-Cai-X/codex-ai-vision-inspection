#include "../../cxgeom/polar_discrete_harmonic/landmark_identity_uncertainty.h"
#include <iostream>
#include <cstdlib>
using namespace cxgeom::polar;
void require(bool v,const char* reason){if(!v)throw std::runtime_error(reason);}
int main(){try{
 const double pi=std::acos(-1.);
 LandmarkPoint u0(30,40),u1(50,43),a=std::polar(1.3,.6),t(7,-9);
 unsigned checks=0;
 for(int j=0;j<16;++j)for(int k=0;k<16;++k){
  auto e0=std::polar(.1,2*pi*j/16),e1=std::polar(.1,2*pi*k/16);
  auto f0=std::polar(.15,2*pi*k/16),f1=std::polar(.15,2*pi*j/16);
  auto U0=u0+e0,U1=u1+e1,V0=a*u0+t+f0,V1=a*u1+t+f1;
  auto fitted=(V1-V0)/(U1-U0),shift=V0-fitted*U0;
  auto b=BoundLandmarkPose(U0,U1,V0,V1,fitted,shift,.1,.15);
  require(b.status=="CONDITIONAL_LANDMARK_DISK_BOUND","bound unavailable");
  require(std::abs(std::arg(fitted/a))<=b.angle_rad+1e-12,"angle containment");
  require(std::abs(std::abs(fitted)-std::abs(a))<=b.scale_absolute+1e-12,"scale containment");
  require(std::abs(shift-t)<=b.translation_px+1e-12,"translation containment");
  auto m=(U0+U1)*.5;
  require(std::abs((fitted-a)*m+shift-t)<=b.center_px+1e-12,"center containment");
  ++checks;
 }
 auto b=BoundLandmarkPose(u0,u1,a*u0+t,a*u1+t,a,t,.1,.15);
 auto h=LandmarkPoint(1000,1000);
 auto c=BoundLandmarkPose(u0+h,u1+h,a*u0+t,a*u1+t,a,t-a*h,.1,.15);
 require(c.translation_px>b.translation_px,"origin leverage");
 require(std::abs(c.center_px-b.center_px)<1e-10,"center invariance");
 require(BoundLandmarkPose(0.,.1,0.,.1,1.,0.,.1,.1).status=="LANDMARK_BASELINE_UNRESOLVED","short baseline");
 std::vector<LandmarkPoint> r,o,line;
 for(int i=0;i<=200;++i){double x=i*.1;r.emplace_back(x,.04*x*x+2*std::exp(-std::pow((x-9)/2,2)));o.push_back(a*r.back()+t);line.emplace_back(x,0);}
 landmark_identity_detail::Curve rc(r),oc(o);
 LandmarkIdentityConfig cfg;cfg.radius_px=4;cfg.step_px=.02;cfg.score_tolerance_px=.005;cfg.maximum_hypotheses=4096;
 double s=rc.s[100];
 auto positive=SearchLandmarkIdentity(r,o,s,s*std::abs(a),-1,-1,std::abs(a),cfg);
 require(positive.status=="GEOMETRIC_IDENTITY_SUPPORTED_WITHIN_SEARCH",positive.status.c_str());
 auto shifted=SearchLandmarkIdentity(r,o,s,s*std::abs(a)+.75,-1,-1,std::abs(a),cfg);
 require(shifted.status=="IDENTITY_CLAIM_DISPLACED",shifted.status.c_str());
 auto flat=SearchLandmarkIdentity(line,line,10,10,-1,-1,1,cfg);
 require(flat.status=="IDENTITY_AMBIGUOUS_WITHIN_RETAINED_RUN","flat ambiguity");
 auto missing=SearchLandmarkIdentity(r,o,.1,.1,-1,-1,1,cfg);
 require(missing.status=="IDENTITY_INSUFFICIENT_HELD_OUT_SUPPORT","support guard");
 bool bad=false;try{BoundLandmarkPose(0.,1.,0.,1.,1.,0.,-1,0);}catch(const std::invalid_argument&){bad=true;}
 require(bad,"negative error guard");
 std::cout<<"LANDMARK_IDENTITY_UNCERTAINTY_PASS bounded_poses="<<checks
  <<" identity="<<positive.status<<" shifted="<<shifted.status<<" flat="<<flat.status<<"\n";
 return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
