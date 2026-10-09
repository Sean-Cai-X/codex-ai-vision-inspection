#include "../../libtorchsegmentation/src/utils/json.hpp"
#include <cmath>
#include <complex>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <stdexcept>
using J=nlohmann::json;
using Z=std::complex<double>;
constexpr double pi=3.14159265358979323846;
void need(bool ok){if(!ok)throw std::runtime_error("POSE_RECEIPT_CONTRACT_FAILED");}
void generate(const char* path){
 std::ofstream f(path);f.imbue(std::locale::classic());f<<std::setprecision(17);
 f<<R"(Image m_image;
FindObject m_object;
m_image.copyFromMat(global_matInput);
m_object.setrect(0,0,100,100);
m_object.setsearchtype(0);
m_object.setdistance(1);
m_object.setbrow(2);
m_object.setroithre(128);
m_object.setminmax(1,99999999);
m_object.measurecc(m_image);
)";
 f<<"HarmonicAudit a;\n";
 for(int side=0;side<2;++side){
  f<<"a.select("<<side<<");\n";
  for(int i=0;i<64;++i){
   double t=2*pi*i/64;
   double r=40*(1+.15*std::cos(3*t)+.08*std::sin(2*t)+.05*std::cos(t));
   Z z=r*std::polar(1.0,t)+Z(120,85);
   if(side) z=1.2*std::polar(1.0,23.4*pi/180)*z+Z(17.25,-8.5);
   f<<"a.point("<<z.real()<<","<<z.imag()<<");\n";
  }
  f<<"a.topology(1,1,1,0,1);\n";
 }
 f<<R"(a.run();
a.expectposebounds(23.4,1.2,0.1,0.001);
a.parameter(1,"method");
a.run();
a.expectposebounds(23.4,1.2,0.1,0.001);
a.topology(1,0,0,0,1);
a.run();
a.expectstatus("OPEN_CONTOUR_LEGACY_FALLBACK");
a.save(global_harmonic_receipt_path);
)";
 f.close();need(bool(f));
}
int main(int argc,char**argv)try{
 need(argc==3);
 if(std::string(argv[1])=="generate"){generate(argv[2]);return 0;}
 need(std::string(argv[1])=="check");
 std::ifstream f(argv[2]);J j;f>>j;
 need(j.at("schema")=="cxvision.harmonic_audit_receipt.v1");
 need(j.at("production_eligible")==false&&j.at("measurement_evidence")==false);
 need(j.at("used_for_seed")==false&&j.at("used_for_prefilter")==false);
 const auto& runs=j.at("runs");need(runs.size()==3);
 for(int method=0;method<2;++method){
  const auto& r=runs.at(method);
  need(r.at("parameters").at("method")==method);
  need(r.at("status")=="AUDIT_POSE_HYPOTHESES"&&r.at("poses").size()==1);
  const auto& p=r.at("poses").at(0);
  need(p.at("mapping")=="reference_to_observation");
  need(p.at("coordinate_units")=="source_units");
  double a=p.at("angle_deg"),s=p.at("scale"),x=p.at("translation_x"),y=p.at("translation_y");
  double shift=p.at("cyclic_shift"),residual=p.at("residual");
  need(std::isfinite(a)&&std::isfinite(s)&&std::isfinite(x)&&std::isfinite(y));
  need(std::abs(std::remainder(a-23.4,360))<.1&&std::abs(s-1.2)<1e-8);
  need(std::hypot(x-17.25,y+8.5)<.3&&shift>=0&&shift<1);
  need(std::isfinite(residual)&&residual<.035);
 }
 need(runs.at(2).at("status")=="OPEN_CONTOUR_LEGACY_FALLBACK");
 need(runs.at(2).at("poses").empty());
 std::cout<<"POSE_RECEIPT_PASS DFT/EFD/translation/mapping/no-stale/no-promotion"<<std::endl;
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}
