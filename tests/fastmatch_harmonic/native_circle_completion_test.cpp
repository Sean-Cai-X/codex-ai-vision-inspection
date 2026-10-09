#include "CxGeoCircleCompletion.h"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <functional>
#include <algorithm>
using namespace cxgeom::completion;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
void rejects(const std::function<void()>& f,const char* why){
 try{f();}catch(const std::invalid_argument& e){check(std::string(e.what())==why,e.what());std::cout<<"REJECT "<<why<<std::endl;return;}
 throw std::runtime_error("expected rejection");
}
std::vector<Point> arc(double coverage,double angle,double scale,bool reverse=false){
 std::vector<Point> p;Point rot=std::polar(scale,angle*pi/180);
 for(int i=0;i<80;++i){
  double t=.31+coverage*2*pi*i/79;
  p.push_back(rot*(Point(120,85)+std::polar(40.,t))+Point(17.25,-8.5));
 }
 if(reverse)std::reverse(p.begin(),p.end());
 return p;
}
int main(int argc,char** argv)try{
 check(argc==2,"evidence path required");std::ofstream out(argv[1]);check(bool(out),"evidence open");
 out<<std::setprecision(17);
 out<<"coverage,angle,scale,reversed,center_error_px,radius_error_px,observed_rms_px,observed_max_px,synthetic_count,orientation,scope\n";
 int cases=0;
 for(double coverage:{.5,.7})for(double angle:{-179.5,23.4,179.5})
 for(double scale:{.8,1.2})for(bool reverse:{false,true}){
  auto p=arc(coverage,angle,scale,reverse);
  auto raw=p;raw.insert(raw.begin(),3,Point(-10,-20));raw.push_back(Point(900,900));
  auto run=Crop(raw,"controlled-circle-source",3,83);
  auto r=CompleteCircle(run),again=CompleteCircle(run);
  Point truth=std::polar(scale,angle*pi/180)*Point(120,85)+Point(17.25,-8.5);
  double ce=std::abs(r.center-truth),re=std::abs(r.radius-40*scale);
  check(ce<1e-8&&re<1e-8&&r.observed_max_px<1e-8,"fit accuracy");
  check(std::abs(r.angular_coverage-coverage)<1e-10,"angular coverage");
  check(!r.orientation_observable&&!r.measurement_evidence,"audit boundary");
  check(r.contour.points==again.contour.points&&r.source_indices==again.source_indices,"determinism");
  auto projected=ProjectObserved(r);
  check(projected.points==p&&projected.source_indices.front()==3&&projected.source_indices.back()==82,"projection");
  check(raw.size()==84&&raw.front()==Point(-10,-20),"raw unchanged");
  check(std::count(r.synthetic.begin(),r.synthetic.end(),true)==127,"synthetic tags");
  for(size_t i=80;i<r.source_indices.size();++i)
   check(r.source_indices[i]==std::numeric_limits<size_t>::max(),"synthetic source");
  auto descriptor=cxgeom::so2::Build(r.contour);
  auto match=cxgeom::so2::Match(descriptor,descriptor);
  check(!match.succeeded&&match.status=="ORIENTATION_UNOBSERVABLE","circle angle ambiguity");
  out<<coverage<<','<<angle<<','<<scale<<','<<reverse<<','<<ce<<','<<re<<',';
  out<<r.observed_rms_px<<','<<r.observed_max_px<<",127,unobservable,audit_only\n";
  ++cases;
 }
 // Deterministic radial noise: observed residual must not be replaced by synthetic fit residual.
 auto noisy=arc(.7,23.4,1.2);Point center=std::polar(1.2,23.4*pi/180)*Point(120,85)+Point(17.25,-8.5);
 for(size_t i=0;i<noisy.size();++i)noisy[i]+=(noisy[i]-center)/std::abs(noisy[i]-center)*(.05*std::sin(double(i)));
 auto noisyRun=Crop(noisy,"noisy-circle",0,noisy.size());auto noisyResult=CompleteCircle(noisyRun);
 check(noisyResult.observed_rms_px>.01&&noisyResult.observed_max_px<.1,"observed noisy residual");
 check(ProjectObserved(noisyResult).points==noisy,"noise not overwritten by fit");
 rejects([&]{Config c;c.maximum_observed_residual_px=.001;CompleteCircle(noisyRun,c);},"CIRCLE_PRIOR_RESIDUAL_REJECTED");
 auto p=arc(.7,0,1);auto run=Crop(p,"circle",0,p.size());
 rejects([&]{auto a=arc(.2,0,1);CompleteCircle(Crop(a,"short",0,a.size()));},"CIRCLE_COVERAGE_REJECTED");
 rejects([&]{auto a=run;for(auto& v:a.points)v=Point(v.real(),v.imag()*1.4);CompleteCircle(a);},"CIRCLE_PRIOR_RESIDUAL_REJECTED");
 rejects([&]{auto a=run;std::swap(a.points[20],a.points[21]);CompleteCircle(a);},"CIRCLE_ORDER_REVERSAL");
 rejects([&]{auto a=run;a.source_indices[1]=0;CompleteCircle(a);},"INVALID_SOURCE_ORDER");
 rejects([&]{auto a=run;a.points[0]=Point(std::numeric_limits<double>::quiet_NaN(),0);CompleteCircle(a);},"NONFINITE_OBSERVATION");
 rejects([&]{Config c;c.minimum_angular_coverage=.1;CompleteCircle(run,c);},"INVALID_COMPLETION_COVERAGE");
 rejects([&]{auto a=run;for(size_t i=0;i<a.points.size();++i)a.points[i]=Point(double(i),0);CompleteCircle(a);},"CIRCLE_FIT_DEGENERATE");
 rejects([&]{auto r=CompleteCircle(run);r.contour.points[0]+=Point(1,0);ProjectObserved(r);},"OBSERVED_PROVENANCE_MISMATCH");
 out.flush();check(bool(out),"evidence write");
 std::cout<<"CIRCLE_COMPLETION_PASS cases="<<cases<<" rejects=9; explicit crop; no automatic trim or angle claim\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
