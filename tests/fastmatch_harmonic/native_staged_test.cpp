#include "CxGeoSO2StagedSearch.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <locale>
#include <sstream>
using namespace cxgeom::so2;
using Z=std::complex<double>;
constexpr double pi=3.14159265358979323846;
void need(bool b){if(!b)throw std::runtime_error("STAGED_GUARD_FAILED");}
Contour shape(int type=0){
 Contour c;c.closed=true;c.topology_verified=true;
 for(int i=0;i<64;++i){
  double t=2*pi*i/64;
  double r=40*(1+.15*std::cos(3*t)+.08*std::sin(2*t)+.05*std::cos(t));
  Z z=type==0?r*std::polar(1.0,t):Z(40*std::cos(t),(type==1?40:25)*std::sin(t));
  c.points.push_back(z+Z(120,85));
 }return c;
}
std::string bytes(const StagedResult& r){
 std::ostringstream o;o.imbue(std::locale::classic());o<<std::hexfloat;
 o<<r.match.status<<r.match.fallback_reason<<r.match.symmetry_order;
 o<<r.search_complete<<r.evaluations<<r.coarse_peaks<<r.fine_peaks;
 for(const auto& p:r.match.poses)
  o<<p.angle_deg<<','<<p.scale<<','<<p.translation_x<<','<<p.translation_y
   <<','<<p.correlation<<','<<p.residual<<','<<p.cyclic_shift<<';';
 for(const auto* v:{&r.coarse_response,&r.fine_response})
  for(const auto& p:*v)o<<p.shift_turns<<','<<p.angle_deg<<','<<p.correlation<<','<<p.residual;
 return o.str();
}
int main()try{
 int cases=0;
 for(auto method:{Method::Dft,Method::Efd})
 for(bool normalize:{true,false})
 for(double angle:{-179.5,23.4,179.5})
 for(double scale:{.8,1.2}){
  Config cfg;cfg.normalize_scale=normalize;
  auto a=shape(),b=a;
  for(auto& z:b.points)z=scale*std::polar(1.0,angle*pi/180)*z+Z(17.25,-8.5);
  std::rotate(b.points.begin(),b.points.begin()+11,b.points.end());
  auto da=Build(a,cfg,method),db=Build(b,cfg,method);
  StagedConfig s;s.capture_response=true;
  const auto r=MatchStaged(da,db,s);
  need(r.search_complete&&r.match.succeeded&&r.match.poses.size()==1);
  need(!r.continuous_global_optimum_certified&&!r.match.measurement_evidence);
  const auto& p=r.match.poses.front();
  need(std::abs(std::remainder(p.angle_deg-angle,360))<.1);
  need(std::abs(p.scale-scale)<.001);
  need(std::abs(Z(p.translation_x,p.translation_y)-Z(17.25,-8.5))<.3);
  need(r.coarse_response.size()==64&&r.fine_response.size()==256);
  need(r.evaluations==320+r.fine_peaks*35);
  const auto original=bytes(r);
  for(int n=0;n<100;++n)need(bytes(MatchStaged(da,db,s))==original);
  s.capture_response=false;auto no_trace=MatchStaged(da,db,s);
  need(no_trace.coarse_response.empty()&&no_trace.fine_response.empty());
  need(no_trace.match.poses.front().translation_x==p.translation_x);
  s.coarse_order=1;need(MatchStaged(da,db,s).match.poses.size()==1);
  ++cases;
 }
 auto d=Build(shape());
 StagedConfig s;s.capture_response=true;s.maximum_evaluations=10;
 auto r=MatchStaged(d,d,s);
 need(!r.search_complete&&!r.match.succeeded&&r.match.poses.empty()&&r.evaluations==10);
 need(r.coarse_response.size()==10&&r.fine_response.empty());
 s.maximum_evaluations=320;r=MatchStaged(d,d,s);
 need(r.match.status=="STAGED_SEARCH_BUDGET_EXHAUSTED"&&r.match.poses.empty());
 s=StagedConfig{};auto full=MatchStaged(d,d,s);
 s.maximum_evaluations=full.evaluations;need(MatchStaged(d,d,s).match.succeeded);
 --s.maximum_evaluations;need(!MatchStaged(d,d,s).match.succeeded);
 bool invalid=false;s=StagedConfig{};s.fine_order=32;
 try{MatchStaged(d,d,s);}catch(const std::invalid_argument&){invalid=true;}need(invalid);
 Config cfg;cfg.max_order=32;d=Build(shape(),cfg);
 s.coarse_order=8;need(MatchStaged(d,d,s).match.succeeded);
 s=StagedConfig{};
 for(auto method:{Method::Dft,Method::Efd}){
  auto circle=Build(shape(1),Config{},method);r=MatchStaged(circle,circle,s);
  need(r.match.status=="ORIENTATION_UNOBSERVABLE"&&r.match.poses.empty());
  auto ellipse=Build(shape(2),Config{},method);r=MatchStaged(ellipse,ellipse,s);
  need(r.match.succeeded&&r.match.poses.size()==2);
  Config one;one.maximum_hypotheses=1;ellipse=Build(shape(2),one,method);
  r=MatchStaged(ellipse,ellipse,s);
  need(r.match.status=="HYPOTHESIS_BUDGET_EXCEEDED"&&r.match.poses.empty());
 }
 std::cout<<"STAGED_PASS "<<cases<<" shifted-start cases x100; budgets/symmetry/order32"<<std::endl;
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}
