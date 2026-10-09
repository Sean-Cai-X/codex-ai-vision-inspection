#include "CxGeoSO2Harmonic.h"
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>
using namespace cxgeom::so2;
using Z=std::complex<double>;
constexpr double pi=3.14159265358979323846;
void need(bool b,const char* s){if(!b){std::cerr<<s<<std::endl;std::exit(1);}}
std::string canonical(const Result& r){
    std::ostringstream o;o.imbue(std::locale::classic());o<<std::hexfloat;
    o<<r.succeeded<<r.status<<r.fallback_reason<<r.symmetry_order;
    o<<r.invariant_distance<<r.measurement_evidence<<r.used_for_seed<<r.used_for_prefilter;
    for(const auto& p:r.poses)
        o<<p.angle_deg<<','<<p.scale<<','<<p.translation_x<<','<<p.translation_y
         <<','<<p.correlation<<','<<p.residual<<','<<p.cyclic_shift<<';';
    return o.str();
}
Contour shape(int type){
    Contour c;c.closed=true;c.topology_verified=true;
    for(int i=0;i<64;++i){
        double t=2*pi*i/64;
        double r=40*(1+.15*std::cos(3*t)+.08*std::sin(2*t)+.05*std::cos(t));
        Z z= type==0?r*std::polar(1.0,t):Z(40*std::cos(t),(type==1?40:25)*std::sin(t));
        c.points.push_back(z+Z(120,85));
    } return c;
}
int main(){
    int cases=0;
    for(auto method:{Method::Dft,Method::Efd})
    for(bool normalized:{true,false})
    for(double angle:{-179.5,23.4,179.5})
    for(double scale:{.8,1.2}){
        Config cfg;cfg.normalize_scale=normalized;
        auto a=shape(0), b=a;const Z translation(17.25,-8.5);
        for(auto& z:b.points) z=scale*std::polar(1.0,angle*pi/180)*z+translation;
        auto da=Build(a,cfg,method),db=Build(b,cfg,method);
        auto result=Match(da,db);
        need(result.succeeded&&result.poses.size()==1,"asymmetric unique candidate");
        const auto& p=result.poses.front();
        need(std::abs(std::remainder(p.angle_deg-angle,360))<.1,"rotation");
        need(std::abs(p.scale-scale)<1e-8,"scale");
        need(std::abs(Z(p.translation_x,p.translation_y)-translation)<.3,"translation");
        need(!result.measurement_evidence&&!result.used_for_seed,"audit boundary");
        const auto bytes=canonical(result);
        for(int i=0;i<100;++i) need(canonical(Match(da,db))==bytes,"repeat determinism");
        ++cases;
    }
    for(auto method:{Method::Dft,Method::Efd}){
        auto circle=Build(shape(1),Config{},method);
        auto r=Match(circle,circle);
        need(!r.succeeded&&r.status=="ORIENTATION_UNOBSERVABLE","circle unobservable");
        auto ellipse=Build(shape(2),Config{},method);
        r=Match(ellipse,ellipse);
        need(r.succeeded&&r.poses.size()==2,"ellipse ambiguity preserved");
        const auto bytes=canonical(r);
        for(int i=0;i<100;++i) need(canonical(Match(ellipse,ellipse))==bytes,"ambiguous determinism");
    }
    std::cout<<cases<<" similarity cases, 100 repeats each; circle/ellipse guards PASS"<<std::endl;
}
