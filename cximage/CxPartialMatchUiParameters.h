#pragma once
#include "../cxgeom/polar_discrete_harmonic/partial_search.h"
#include <regex>
#include <sstream>
#include <iomanip>
#include <locale>
namespace cxpartialui {
struct Parameter {const char* key;double initial;double minimum,maximum;bool integer;};
inline std::vector<Parameter> Parameters(){
 cxgeom::polar::PartialConfig a;cxgeom::polar::PartialSearchConfig s;
 return {
 {"maximum_hypotheses",double(a.maximum_hypotheses),1,65536,true},
 {"maximum_pair_checks",double(a.maximum_pair_checks),1,100000000,true},
 {"maximum_candidates",double(s.maximum_candidates),1,256,true},
 {"maximum_features",double(a.maximum_features),3,128,true},
 {"minimum_matches",double(a.minimum_matches),3,128,true},
 {"minimum_reference_coverage",a.minimum_reference_coverage,1e-9,1,false},
 {"minimum_observation_coverage",a.minimum_observation_coverage,1e-9,1,false},
 {"minimum_spatial_span",a.minimum_spatial_span,1e-9,1,false},
 {"max_residual_px",a.max_residual_px,1e-9,1e12,false},
 {"scale_min",s.scale_min,1e-9,1e6,false},{"scale_max",s.scale_max,1e-9,1e6,false},
 {"angle_min_deg",s.angle_min_deg,-180,180,false},{"angle_max_deg",s.angle_max_deg,-180,180,false},
 {"minimum_seed_length_px",s.minimum_seed_length_px,1e-12,1e12,false},
 {"merge_angle_deg",s.merge_angle_deg,1e-12,180,false},
 {"merge_scale",s.merge_scale,1e-12,1e6,false},
 {"merge_translation_px",s.merge_translation_px,1e-12,1e12,false},
 {"encoding_bins",double(a.encoding.bins),16,4096,true},
 {"encoding_max_order",double(a.encoding.max_order),1,64,true},
 {"minimum_rms_radius",a.encoding.minimum_rms_radius,1e-12,1e12,false},
 {"refine_before_capacity",s.refine_before_capacity?1.:0.,0,1,true}};
}
inline bool IsCase(const std::string& script){return std::regex_search(script,std::regex(R"(\bPartialMatchAudit\s+[A-Za-z_])"));}
// Keep offsets stable while excluding comments from editable bindings.
inline std::string Code(const std::string& script){
 std::string code=script;bool quoted=false,escape=false,line=false,block=false;
 for(size_t i=0;i<script.size();++i){
  char c=script[i],next=i+1<script.size()?script[i+1]:0;
  if(line){if(c=='\n')line=false;else code[i]=' ';continue;}
  if(block){code[i]=' ';if(c=='*'&&next=='/'){code[++i]=' ';block=false;}continue;}
  if(quoted){if(escape)escape=false;else if(c=='\\')escape=true;else if(c=='"')quoted=false;continue;}
  if(c=='"'){quoted=true;continue;}
  if(c=='/'&&(next=='/'||next=='*')){line=next=='/';block=next=='*';code[i]=' ';code[++i]=' ';}
 }
 if(block||quoted)throw std::invalid_argument("Incomplete script comment or string");
 return code;
}
inline size_t RunPosition(const std::string& script,const std::string& object){
 auto code=Code(script);std::regex run("\\b"+object+R"(\.run\s*\(\s*\)\s*;)");
 auto it=std::sregex_iterator(code.begin(),code.end(),run);
 if(it==std::sregex_iterator())throw std::invalid_argument("Missing run call");
 auto match=*it;if(++it!=std::sregex_iterator())throw std::invalid_argument("Multiple runs: edit script explicitly");
 return size_t(match.position());
}


inline std::string Object(const std::string& script){
 auto code=Code(script);
 std::regex declaration(R"(\bPartialMatchAudit\s+([A-Za-z_][A-Za-z0-9_]*)\s*;)");
 auto begin=std::sregex_iterator(code.begin(),code.end(),declaration);
 if(begin==std::sregex_iterator())throw std::invalid_argument("Missing PartialMatchAudit declaration");
 auto found=*begin;if(++begin!=std::sregex_iterator())throw std::invalid_argument("Multiple instances: edit the script explicitly");
 return found[1];
}
struct Value {bool present=false;double value=0;size_t position=0,length=0;};
inline Value Read(const std::string& script,const std::string& object,const Parameter& p){
 std::regex call("\\b"+object+R"(\.parameter\s*\(\s*([^,]+),\s*")"+p.key+R"("\s*\)\s*;)");
 auto code=Code(script);auto it=std::sregex_iterator(code.begin(),code.end(),call);Value out;
 if(it==std::sregex_iterator())return out;
 auto match=*it;if(++it!=std::sregex_iterator())throw std::invalid_argument("Repeated parameter assignment: edit script explicitly");
 std::istringstream input(match[1].str());input.imbue(std::locale::classic());
 if(!(input>>out.value))throw std::invalid_argument("Expression-bound parameter: edit script explicitly");
 input>>std::ws;if(!input.eof()||!std::isfinite(out.value))throw std::invalid_argument("Invalid literal parameter");
 out.present=true;out.position=size_t(match.position());out.length=size_t(match.length());return out;
}
inline std::string Call(const std::string& object,const Parameter& p,double value){
 if(!std::isfinite(value)||value<p.minimum||value>p.maximum||(p.integer&&std::floor(value)!=value))
  throw std::invalid_argument("Parameter outside editable range");
 std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(17);
 out<<object<<".parameter("<<value<<",\""<<p.key<<"\");";return out.str();
}
inline void Set(std::string& script,const Parameter& p,double value){
 auto object=Object(script);auto run=RunPosition(script,object);auto old=Read(script,object,p);
 if(old.present&&old.position>run)throw std::invalid_argument("Parameter after run: edit script explicitly");
 if(!old.present)throw std::invalid_argument("Expose missing parameters first");
 script.replace(old.position,old.length,Call(object,p,value));
}
inline void Expose(std::string& script){
 auto object=Object(script);auto run=RunPosition(script,object);
 std::string additions;
 for(const auto& p:Parameters()){
  auto old=Read(script,object,p);
  if(old.present&&old.position>run)throw std::invalid_argument("Parameter after run: edit script explicitly");
  if(!old.present)additions+=Call(object,p,p.initial)+"\n";
 }
 script.insert(run,additions);
}
}
