#pragma once
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

// Shared by GUI defaults, frozen cxscript and headless replay. Fixed-point
// units are explicit. ROI/provenance remain in the immutable request asset.
namespace cxsetmatchui {
struct Parameter { const char* key; const char* field; const char* label; int value, minimum, maximum, divisor; };
inline const Parameter parameters[] = {
 {"global_setmatch_angle_min","angle_min_deg","Minimum angle (0.001 deg)",-180000,-180000,180000,1000},
 {"global_setmatch_angle_max","angle_max_deg","Maximum angle (0.001 deg)",180000,-180000,180000,1000},
 {"global_setmatch_scale_min","scale_min","Minimum scale (0.001x)",800,1,1000000,1000},
 {"global_setmatch_scale_max","scale_max","Maximum scale (0.001x)",1200,1,1000000,1000},
 {"global_setmatch_residual","max_residual_px","Maximum residual (0.001 px)",1000,1,1000000,1000},
 {"global_setmatch_coverage","min_weighted_coverage","Minimum weighted coverage (ppm)",500000,1,1000000,1000000},
 {"global_setmatch_span","min_spatial_span_ratio","Minimum spatial span (ppm)",250000,0,1000000,1000000},
 {"global_setmatch_min_elements","min_matched_elements","Minimum matched elements",3,1,8192,1},
 {"global_setmatch_max_elements","max_elements_per_set","Maximum elements / set",4096,1,8192,1},
 {"global_setmatch_hypotheses","max_hypotheses","Maximum hypotheses",256,1,4096,1},
 {"global_setmatch_pair_checks","max_pair_checks","Maximum pair checks",1000000,1,100000000,1},
 {"global_setmatch_elapsed","max_elapsed_ms","Time budget (ms)",100,1,60000,1}
};
inline bool IsCase(const std::string& text) {
 return text.find("// setmatch_evidence_binding: 1")!=std::string::npos;
}
inline bool Validate(const std::unordered_map<std::string,int>& values,std::string& reason) {
 for(const auto& p:parameters) {
  auto i=values.find(p.key);
  if(i==values.end()||i->second<p.minimum||i->second>p.maximum) {
   reason=std::string("Invalid/missing parameter: ")+p.key;return false;
  }
 }
 for(const auto& pair: {std::pair<const char*,const char*>{"global_setmatch_angle_min","global_setmatch_angle_max"},
      {"global_setmatch_scale_min","global_setmatch_scale_max"},{"global_setmatch_min_elements","global_setmatch_max_elements"}})
  if(values.at(pair.first)>values.at(pair.second)){reason=std::string(pair.first)+" must not exceed "+pair.second;return false;}
 reason.clear();return true;
}
inline bool Defaults(const std::string& text,std::unordered_map<std::string,int>& values,std::string& reason) {
 values.clear();for(const auto& p:parameters)values[p.key]=p.value;
 std::istringstream lines(text);std::string line;
 while(std::getline(lines,line)) {
  const std::string prefix="// setmatch_default ";
  if(line.compare(0,prefix.size(),prefix)!=0)continue;
  std::istringstream in(line.substr(prefix.size()));std::string key,extra;int value;
  if(!(in>>key>>value)||(in>>extra)){reason="Malformed set-match default";return false;}
  bool found=false;
  for(const auto& p:parameters)if(key==p.key){values[key]=value;found=true;break;}
  if(!found){reason="Unknown set-match default: "+key;return false;}
 }
 return Validate(values,reason);
}
// Expand coupled ranges first so a valid final range never fails merely
// because the preceding request has a different interval.
inline std::string ParameterScript(const std::string& object) {
 std::ostringstream s;
 for(const auto& p: {std::pair<const char*,double>{"angle_min_deg",-180},
     {"angle_max_deg",180},{"scale_max",1000000},{"scale_min",.001},
     {"max_elements_per_set",8192},{"min_matched_elements",1}})
  s<<object<<".parameter("<<p.second<<",\""<<p.first<<"\");\n";
 for(const auto& p:parameters)
  s<<object<<".parameter("<<p.key<<" / "<<p.divisor<<",\""<<p.field<<"\");\n";
 return s.str();
}
}
