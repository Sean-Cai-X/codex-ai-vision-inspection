// Native synthetic geometry acceptance; no image-derived accuracy claim.
// Reference: 1024 samples. Observation: independently sampled 997 points, shifted origin.
// Scenarios: 0 complete, 1 missing 30%, 2 radial surplus, 3 component-metadata guard.
// Scenario 3 validates caller topology metadata, NOT extraction of a second component.
// Synthetic truth is used only for evaluation; solver receives no correspondences.
// Observed error uses held-out analytic points, excluding the injected surplus span.
// Rejected poses have NA errors. Rejection never implies an executed fallback.
// Two byte-identical formatted receipts are required within this build/process.
// Initial fixed-direction surplus self-intersected; radial surplus avoids that confound.
// All results remain audit-only; completion/cropping recovery is not implemented here.
#include "CxGeoSO2StagedSearch.h"
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
using namespace cxgeom::so2;
using Z=std::complex<double>;
constexpr double pi=3.141592653589793;
Z point(double u,int kind){
 double t=2*pi*u,r=40*(1+.15*cos(3*t)+.08*sin(2*t)+.05*cos(t));
 if(kind)r+=4*sin(7*t)+2*cos(11*t);
 return Z(120,85)+std::polar(r,t);
}
std::string run(int kind,Method method,double angle,double scale,int scenario,bool& ok){
 Contour a,b;a.closed=b.closed=true;a.topology_verified=b.topology_verified=true;
 Z rot=std::polar(scale,angle*pi/180),tr(17.25,-8.5);
 for(int i=0;i<1024;++i)a.points.push_back(point(i/1024.,kind));
 for(int i=0;i<997;++i)b.points.push_back(rot*point((i+.37)/997.,kind)+tr);
 if(scenario==1){b.points.resize(698);b.closed=false;b.complete=false;}
 if(scenario==2)for(int i=200;i<300;++i)b.points[i]+=rot*std::polar(25*sin(pi*(i-200)/100.),2*pi*(i+.37)/997.);
 if(scenario==3)b.components=2;
 std::ostringstream o;o<<std::setprecision(17);
 o<<kind<<','<<int(method)<<','<<scenario<<','<<angle<<','<<scale<<',';
 try{
  auto r=MatchStaged(Build(a,Config{},method),Build(b,Config{},method));
  o<<r.match.status<<','<<r.match.fallback_reason<<','<<r.match.poses.size();
  double ae=0,se=0,te=0,rms=0,mx=0;
  if(!r.match.poses.empty()){
   auto p=r.match.poses.front();ae=std::abs(std::remainder(p.angle_deg-angle,360));
   se=std::abs(p.scale-scale);te=abs(Z(p.translation_x,p.translation_y)-tr);
   int n=0;
   for(int i=0;i<2000;++i){
    double u=(i+.19)/2000.;
    if(scenario==2&&u>=200./997&&u<=300.37/997)continue;
    Z z=point(u,kind);
    double e=abs(std::polar(p.scale,p.angle_deg*pi/180)*z+Z(p.translation_x,p.translation_y)-(rot*z+tr));
    rms+=e*e;mx=std::max(mx,e);++n;
   }
   rms=sqrt(rms/n);o<<','<<ae<<','<<se<<','<<te<<','<<rms<<','<<mx;
  }else o<<",NA,NA,NA,NA,NA";
  ok=scenario==0?(r.search_complete&&r.match.succeeded&&r.match.poses.size()==1&&ae<=.1&&se<=.001&&te<=.3&&rms<=.3):(!r.match.succeeded&&r.match.poses.empty());
  o<<','<<(ok?"PASS":"GAP")<<",audit_only";
 }catch(const std::invalid_argument& e){
  std::string why=e.what();
  ok=(scenario==1&&why=="OPEN_CONTOUR_LEGACY_FALLBACK")||(scenario==3&&why=="UNSUPPORTED_TOPOLOGY_LEGACY_FALLBACK");
  o<<"REJECTED,"<<why<<",0,NA,NA,NA,NA,NA,"<<(ok?"PASS":"GAP")<<",no_fallback_executed";
 }
 return o.str();
}
int main(int argc,char** argv){
 if(argc!=2)return 2;std::ofstream f(argv[1]);if(!f)return 2;
 f<<"shape,method,scenario,truth_angle,truth_scale,status,reason,poses,angle_error_deg,scale_error,translation_error_px,observed_rms_px,observed_max_px,verdict,scope\n";
 int count=0,gaps=0;
 for(int k:{0,1})for(auto m:{Method::Dft,Method::Efd})
 for(double a:{-179.5,23.4,179.5})for(double s:{.8,1.2})for(int mode:{0,1,2,3}){
  bool ok=false,again=false;auto row=run(k,m,a,s,mode,ok);
  if(row!=run(k,m,a,s,mode,again)||ok!=again)return 2;
  f<<row<<'\n';++count;if(!ok)++gaps;
 }
 std::cout<<"INDEPENDENT_MATRIX cases="<<count<<" gaps="<<gaps<<" repeat=2\n";return gaps?1:0;
}
