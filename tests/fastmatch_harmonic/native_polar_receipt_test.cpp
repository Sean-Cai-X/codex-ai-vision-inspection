#include "../../cxgeom/polar_discrete_harmonic/receipt.h"
#include "../../libtorchsegmentation/src/utils/json.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <limits>
using namespace cxgeom::polar;
using Z=std::complex<double>;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(int argc,char** argv)try{
 const double pi=3.14159265358979323846;
 std::vector<Feature> a={{"a","edge",{-60,-20}},{"b","edge",{20,-40}},{"c","edge",{80,20}},
                         {"d","edge",{-20,60}},{"e","edge",{0,-20}}};
 int cases=0;std::string last;
 for(double angle:{-179.5,23.4,179.5})for(double noise:{0.,.005,.05}){
  auto b=a;
  for(size_t i=0;i<b.size();++i){
   b[i].point=std::polar(1.2,angle*pi/180)*a[i].point+Z(17.25,-8.5)+noise*Z(std::sin(double(i)),std::cos(2.*i));
   b[i].stable_id="t"+b[i].stable_id;
  }
  MatchConfig cfg;
  cfg.angle_min_deg=angle==23.4?10:170;cfg.angle_max_deg=angle==23.4?40:-170;
  auto r=MatchFull("ref\"\n",a,"target",b,cfg);
  check(r.search_complete&&r.candidates.size()==1,"noise/window match");
  const auto& p=r.candidates[0];
  check(std::abs(std::remainder(p.angle_deg-angle,360))<.1,"noise angle");
  check(std::abs(p.scale-1.2)<.001&&std::abs(p.translation-Z(17.25,-8.5))<.1,"noise pose");
  check(p.max_px<=cfg.max_residual_px,"noise residual");
  if(noise>0)check(p.rms_px>1e-6,"noise not erased");
  last=ReceiptV1(r);auto json=nlohmann::json::parse(last);
  check(json["schema"]=="polar_full_match_receipt.v1"&&json["reference_source"]=="ref\"\n","receipt escaping");
  check(json["candidates"][0]["pairs"].size()==5&&json["production_eligible"]==false,"receipt payload");
  std::reverse(b.begin(),b.end());
  check(ReceiptV1(MatchFull("ref\"\n",a,"target",b,cfg))==last,"receipt deterministic");
  std::cout<<"NOISE "<<noise<<" angle_error="<<std::abs(std::remainder(p.angle_deg-angle,360))
           <<" rms="<<p.rms_px<<" max="<<p.max_px<<std::endl;
  cfg.angle_min_deg=1;cfg.angle_max_deg=2;
  auto excluded=MatchFull("ref",a,"target",b,cfg);
  check(excluded.status=="ANGLE_RANGE_REJECTED"&&excluded.candidates.empty(),"angle exclusion");
  auto failJson=nlohmann::json::parse(ReceiptV1(excluded));
  check(failJson["candidates"].empty()&&!excluded.reason.empty(),"failure receipt");
  if(noise>0){
   cfg.angle_min_deg=-180;cfg.angle_max_deg=180;cfg.max_residual_px=1e-7;
   auto strict=MatchFull("ref",a,"target",b,cfg);
   check(strict.candidates.empty()&&strict.status=="NO_SPATIAL_MATCH","strict noise rejection");
  }
  ++cases;
 }
 bool rejected=false;
 try{MatchConfig c;c.angle_min_deg=181;MatchFull("a",a,"b",a,c);}catch(const std::invalid_argument&){rejected=true;}
 check(rejected,"invalid angle");
 auto r=MatchFull("a",a,"b",a);r.candidates[0].scale=std::numeric_limits<double>::quiet_NaN();
 rejected=false;try{ReceiptV1(r);}catch(const std::invalid_argument&){rejected=true;}
 check(rejected,"nonfinite receipt");
 if(argc==2){std::ofstream out(argv[1]);out<<last;out.flush();check(bool(out),"receipt file");}
 std::cout<<"POLAR_NOISE_RECEIPT_PASS cases="<<cases<<"; JSON parsed natively"<<std::endl;
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}
