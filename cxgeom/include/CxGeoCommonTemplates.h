#pragma once
#include "CxGeoSO2Harmonic.h"
#include <cmath>
#include <stdexcept>
namespace cxgeom::so2::templates {
struct Spec { const char* id;const char* geometry;bool closed;int symmetry; };
inline std::vector<Spec> Catalog(){return {
 {"circle","circle",true,0},{"ellipse","ellipse",true,2},{"square","polygon",true,4},
 {"rectangle","polygon",true,2},{"rounded_rectangle","closed_curve",true,2},{"capsule","closed_curve",true,2},
 {"triangle_asymmetric","polygon",true,1},{"quadrilateral_asymmetric","polygon",true,1},
 {"notched","polygon",true,1},{"smooth_asymmetric","closed_curve",true,1},
 {"line","line",false,2},{"arc","arc",false,1},{"ellipse_arc","arc",false,1},{"open_s","open_curve",false,1}};}
struct Parameters { double width=80,height=50,corner_radius=8;int samples=256; };
inline Contour Generate(const std::string& id,const Parameters& p={}){
 if(!std::isfinite(p.width)||!std::isfinite(p.height)||!std::isfinite(p.corner_radius)||p.width<=0||p.height<=0||p.samples<32||p.samples>4096)
  throw std::invalid_argument("TEMPLATE_PARAMETER_RANGE");
 Spec spec{};bool found=false;for(auto s:Catalog())if(id==s.id){spec=s;found=true;break;}
 if(!found)throw std::invalid_argument("TEMPLATE_UNKNOWN_ID");
 Contour c;c.closed=spec.closed;c.topology_verified=true;
 constexpr double pi=3.14159265358979323846;const double a=p.width/2,b=p.height/2;
 auto poly=[&](std::vector<std::complex<double>> vertices){
  const int count=std::max(4,p.samples/int(vertices.size()));
  for(size_t k=0;k<vertices.size();++k)for(int j=0;j<count;++j)c.points.push_back(vertices[k]+(vertices[(k+1)%vertices.size()]-vertices[k])*(j/double(count)));
 };
 if(id=="square")poly({{-a,-a},{a,-a},{a,a},{-a,a}});
 else if(id=="rectangle")poly({{-a,-b},{a,-b},{a,b},{-a,b}});
 else if(id=="triangle_asymmetric")poly({{-a,-b},{a,-.6*b},{-.3*a,b}});
 else if(id=="quadrilateral_asymmetric")poly({{-a,-b},{.7*a,-.6*b},{a,.5*b},{-.4*a,b}});
 else if(id=="notched")poly({{-a,-b},{a,-b},{a,-.2*b},{.1*a,-.2*b},{.1*a,.3*b},{a,.3*b},{a,b},{-a,b}});
 else if(id=="rounded_rectangle"||id=="capsule"){
  const double r=id=="capsule"?b:p.corner_radius;
  if(r<=0||r>std::min(a,b))throw std::invalid_argument("TEMPLATE_CORNER_RADIUS");
  for(int k=0;k<4;++k){
   std::complex<double> center(k==0||k==3?a-r:-a+r,k<2?b-r:-b+r);
   for(int j=0;j<p.samples/4;++j){double t=(k+j/double(p.samples/4))*pi/2;c.points.push_back(center+std::polar(r,t));}
  }
 }else for(int i=0;i<p.samples;++i){
  double u=i/double(c.closed?p.samples:p.samples-1),t=2*pi*u;
  if(id=="circle")c.points.emplace_back(a*std::cos(t),a*std::sin(t));
  else if(id=="ellipse")c.points.emplace_back(a*std::cos(t),b*std::sin(t));
  else if(id=="smooth_asymmetric"){double r=1+.15*std::cos(3*t)+.08*std::sin(2*t)+.05*std::cos(t);c.points.emplace_back(a*r*std::cos(t),b*r*std::sin(t));}
  else if(id=="line")c.points.emplace_back(-a+p.width*u,0);
  else if(id=="arc"||id=="ellipse_arc"){t=-.7*pi+1.1*pi*u;c.points.emplace_back(a*std::cos(t),(id=="arc"?a:b)*std::sin(t));}
  else c.points.emplace_back(-a+p.width*u,b*(.6*std::sin(2*pi*u)+.2*u*u));
 }
 return c;
}
}
