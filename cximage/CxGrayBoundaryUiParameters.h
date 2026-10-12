
#pragma once
#include "CxGrayBoundaryParameters.h"
#include "../libtorchsegmentation/src/utils/json.hpp"
#include "CxPartialMatchUiParameters.h"
namespace cxgrayui {
using cxgrayparams::Parameters;
inline std::string Structure(const std::string& script){
 auto code=cxpartialui::Code(script);bool quote=false,escape=false;
 for(auto& ch:code){
  const char c=ch;
  if(quote){ch=' ';if(escape)escape=false;else if(c=='\\')escape=true;else if(c=='"')quote=false;}
  else if(c=='"'){quote=true;ch=' ';}
 }
 return code;
}
inline bool IsCase(const std::string& script){
 try{return std::regex_search(Structure(script),std::regex(R"(\bGrayBoundaryAudit\s+[A-Za-z_])"));}
 catch(...){return false;}
}
inline std::string Object(const std::string& script){
 auto code=Structure(script);
 std::regex decl(R"(\bGrayBoundaryAudit\s+([A-Za-z_][A-Za-z0-9_]*)\s*;)");
 auto it=std::sregex_iterator(code.begin(),code.end(),decl);
 if(it==std::sregex_iterator())throw std::invalid_argument("Missing GrayBoundaryAudit declaration");
 auto first=*it;if(++it!=std::sregex_iterator())throw std::invalid_argument("Multiple instances: use Script Editor");
 return first[1];
}
inline cxpartialui::Parameter Literal(const cxgrayparams::Parameter& p){return {p.key,p.initial,p.minimum,p.maximum,p.integer};}
inline auto Read(const std::string& script,const std::string& object,const cxgrayparams::Parameter& p){
 auto value=cxpartialui::Read(script,object,Literal(p));
 if(value.present&&value.position>cxpartialui::RunPosition(Structure(script),object))
  throw std::invalid_argument("Parameter after run: use Script Editor");
 return value;
}
inline void Set(std::string& script,const cxgrayparams::Parameter& p,double value){
 auto obj=Object(script);auto old=Read(script,obj,p);
 if(!old.present)throw std::invalid_argument("Expose missing parameters first");
 cxgrayparams::Validate(p,value);
 script.replace(old.position,old.length,cxpartialui::Call(obj,Literal(p),value));
}
inline void Expose(std::string& script){
 const auto obj=Object(script);const auto run=cxpartialui::RunPosition(Structure(script),obj);
 std::string additions;
 for(const auto& p:Parameters())if(!Read(script,obj,p).present)additions+=cxpartialui::Call(obj,Literal(p),p.initial)+"\n";
 script.insert(run,additions);
}
inline nlohmann::json Values(const std::string& script){
 const auto obj=Object(script);nlohmann::json values=nlohmann::json::object();
 for(const auto& p:Parameters()){
  auto v=Read(script,obj,p);if(!v.present)throw std::invalid_argument("Expose all parameters before Run");
  cxgrayparams::Validate(p,v.value);values[p.key]=v.value;
 }
 const double radius=values.at("scan_radius_px"),step=values.at("scan_step_px");
 const double samples=2*radius/step;
 if(samples<8||samples>256||std::abs(samples-std::round(samples))>=1e-8||values.at("maximum_shift_px").get<double>()>radius)
  throw std::invalid_argument("Profile constraint: shift <= radius; 2*radius/step integer in [8,256]");
 return values;
}
}
