#include "../../libtorchsegmentation/src/utils/json.hpp"
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
using J=nlohmann::json;
static void need(bool ok){if(!ok)throw std::runtime_error("FEATURE_RECEIPT_CONTRACT_FAILED");}
static J read(const std::string& root,const char* name){
 std::ifstream f(root+"/"+name+"/harmonic_audit_receipt.json");J r;f>>r;
 need(r.at("production_eligible")==false && r.at("measurement_evidence")==false);
 return r.at("runs");
}
static void spectrum(const J& run,int method) {
 const auto& debug=run.at("debug");need(debug.at("status")=="AVAILABLE");
 need(debug.at("features").size()==2);
 for(const auto& d:debug.at("features")){
  need(d.at("method")==method && d.at("phase_units")=="radians");
  need(d.at("phase_semantics")=="coefficient_phase_not_rotation_response");
  const int order=run.at("parameters").at("max_order");
  const auto& c=d.at("coefficients");need(c.size()==std::size_t(order*2));
  int index=0;
  for(int frequency=-order;frequency<=order;++frequency)if(frequency) {
   const auto& v=c.at(index++);need(v.at("frequency")==frequency);
   double re=v.at("real"),im=v.at("imag"),amp=v.at("amplitude");
   need(std::isfinite(re)&&std::isfinite(im)&&std::isfinite(amp));
   need(std::abs(std::hypot(re,im)-amp)<1e-12);
   if(amp<=1e-12)need(v.at("phase_rad").is_null());
   else need(std::abs(v.at("phase_rad").get<double>()-std::atan2(im,re))<1e-12);
  }
 }
}
int main(int argc,char**argv)try {
 need(argc==2);const std::string root=argv[1];
 auto off=read(root,"off").back(),dft=read(root,"dft").back(),efd=read(root,"efd").back();
 need(off.at("debug").at("status")=="DISABLED"&&off.at("debug").at("features").empty());
 spectrum(dft,0);spectrum(efd,1);
 auto a=off,b=dft;a.erase("debug");b.erase("debug");need(a==b);
 auto open=read(root,"open").back();
 need(open.at("debug").at("status")=="UNAVAILABLE"&&open.at("debug").at("features").empty());
 need(open.at("status")=="OPEN_CONTOUR_LEGACY_FALLBACK");
 auto stale=read(root,"stale");need(stale.size()==2);spectrum(stale.at(0),0);
 need(stale.at(1).at("debug").at("status")=="UNAVAILABLE");
 need(stale.at(1).at("debug").at("features").empty());
 auto toggle=read(root,"toggle");need(toggle.size()==2);spectrum(toggle.at(0),0);
 need(toggle.at(1).at("debug").at("status")=="DISABLED");
 need(toggle.at(1).at("debug").at("features").empty());
 std::cout<<"HARMONIC_FEATURE_RECEIPTS_PASS dft/efd/off/open/stale/toggle/invariance\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
