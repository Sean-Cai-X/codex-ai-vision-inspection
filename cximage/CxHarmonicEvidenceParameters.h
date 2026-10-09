#pragma once
#include <sstream>
#include <string>
#include <unordered_map>

// One parameter schema for Evidence scripts, GUI and native headless replay.
// Integer controls expose explicit fixed-point units; no implicit rounding.
namespace cxharmonicui {
struct Parameter { const char* key; const char* label; int value, minimum, maximum; };
inline const Parameter parameters[] = {
 {"global_harmonic_method","Method (0 DFT / 1 EFD)",0,0,1},
 {"global_harmonic_sample_count","Resample points",256,32,1024},
 {"global_harmonic_max_order","Maximum harmonic order",12,1,511},
 {"global_harmonic_minimum_source_points","Minimum source points",16,3,4096},
 {"global_harmonic_minimum_perimeter_milli","Min perimeter (0.001 px)",20000,1,10000000},
 {"global_harmonic_normalize_scale","Normalize scale (0/1)",1,0,1},
 {"global_harmonic_residual_ppm","Max pose residual (ppm)",35000,1,1000000},
 {"global_harmonic_symmetry_ppm","Symmetry amplitude (ppm)",2000,1,1000000},
 {"global_harmonic_peak_ppm","Peak tolerance (ppm)",100,1,1000000},
 {"global_harmonic_maximum_hypotheses","Maximum pose hypotheses",16,1,1024},
 {"global_harmonic_anchor_x","Anchor x (px)",150,0,100000},
 {"global_harmonic_anchor_y","Anchor y (px)",100,0,100000},
 {"global_harmonic_anchor_w","Anchor width (px)",20,1,100000},
 {"global_harmonic_anchor_h","Anchor height (px)",20,1,100000},
 {"global_harmonic_anchor_points","Minimum anchor points",3,2,4096},
 {"global_harmonic_threshold","Foreground threshold",128,0,255},
 {"global_harmonic_min_area","Minimum component area",20,1,10000000},
 {"global_harmonic_roi_x","Source ROI x (px)",0,0,100000},
 {"global_harmonic_roi_y","Source ROI y (px)",0,0,100000},
 {"global_harmonic_roi_w","Source ROI width (px)",320,1,100000},
 {"global_harmonic_roi_h","Source ROI height (px)",240,1,100000},
 {"global_harmonic_debug_mode","Debug mode (0/1)",0,0,1},
 {"global_harmonic_save_features","Save intermediate features (0/1)",0,0,1},

 {"global_harmonic_staged_search","Enable staged search (0/1)",0,0,1},
 {"global_harmonic_staged_coarse_order","Coarse harmonic order",4,1,511},
 {"global_harmonic_staged_fine_order","Fine harmonic order",12,1,511},
 {"global_harmonic_staged_coarse_samples","Coarse shift samples",64,4,2048},
 {"global_harmonic_staged_fine_samples","Fine shift samples",256,4,2048},
 {"global_harmonic_staged_refinement_iterations","Refinement iterations",32,1,64},
 {"global_harmonic_staged_maximum_evaluations","Maximum spectral evaluations",65536,1,262144},
 {"global_harmonic_staged_capture_response","Capture shift response (0/1)",0,0,1},
};
inline bool IsCase(const std::string& text) {
 return text.find("// harmonic_evidence_binding: 1")!=std::string::npos;
}
inline bool SupportsFeatureDebug(const std::string& text) {
 return text.find(".parameter(global_harmonic_debug_mode,")!=std::string::npos &&
        text.find(".parameter(global_harmonic_save_features,")!=std::string::npos;
}
inline bool SupportsStagedSearch(const std::string& text) {
 // Canonical shipped-script binding check; actual receipts remain execution evidence.
 std::string code,line;std::istringstream lines(text);
 while(std::getline(lines,line))code+=line.substr(0,line.find("//"));
 std::size_t at=0;
 while((at=code.find("/*",at))!=std::string::npos) {
  const auto end=code.find("*/",at+2);if(end==std::string::npos)return false;
  code.erase(at,end+2-at);
 }
 for(int i=23;i<31;++i) {
  const std::string key=parameters[i].key;
  const std::string call=".parameter("+key+",\""+key.substr(16)+"\");";
  if(code.find(call)==std::string::npos)return false;
 }
 return true;
}
inline bool Defaults(const std::string& text, std::unordered_map<std::string,int>& out,
                     std::string& reason) {
 out.clear();
 for(const auto& p:parameters) out[p.key]=p.value;
 std::istringstream lines(text); std::string line;
 while(std::getline(lines,line)) {
  const std::string prefix="// harmonic_default ";
  if(line.compare(0,prefix.size(),prefix)!=0) continue;
  std::istringstream in(line.substr(prefix.size()));std::string key,extra;int value;
  if(!(in>>key>>value)||(in>>extra)) {reason="Malformed harmonic default";return false;}
  bool found=false;
  for(const auto& p:parameters) if(key==p.key) {
   if(value<p.minimum||value>p.maximum) {reason="Harmonic default out of range: "+key;return false;}
   out[key]=value;found=true;break;
  }
  if(!found){reason="Unknown harmonic default: "+key;return false;}
 }
 if((out.at("global_harmonic_debug_mode") || out.at("global_harmonic_save_features")) && !SupportsFeatureDebug(text)) {reason="Script lacks harmonic feature debug binding";return false;}
 if(out.at("global_harmonic_staged_search") && !SupportsStagedSearch(text)) {
  reason="Script lacks complete staged search binding";return false;
 }
 reason.clear();return true;
}
inline bool Validate(const std::unordered_map<std::string,int>& values,std::string& reason) {
 for(const auto& entry:values) {
  if(entry.first.rfind("global_harmonic_",0)!=0)continue;
  bool known=false;for(const auto& p:parameters)if(entry.first==p.key){known=true;break;}
  if(!known){reason="Unsupported harmonic parameter: "+entry.first;return false;}
 }
 for(const auto& p:parameters) {
  auto i=values.find(p.key);
  if(i==values.end()||i->second<p.minimum||i->second>p.maximum) {
   reason=std::string("Invalid/missing parameter: ")+p.key;return false;
  }
 }
 if(values.at("global_harmonic_max_order")>=values.at("global_harmonic_sample_count")/2) {
  reason="Maximum harmonic order must be < resample points / 2";return false;
 }
 if(values.at("global_harmonic_save_features") && !values.at("global_harmonic_debug_mode")) {
  reason="Save intermediate features requires debug mode";return false;
 }
 if(values.at("global_harmonic_staged_search")) {
  auto v=[&](const char* key){return values.at(std::string("global_harmonic_")+key);};
  if(v("staged_coarse_order")>v("staged_fine_order") || v("staged_fine_order")>v("max_order")) {
   reason="Staged orders must satisfy coarse <= fine <= maximum order";return false;
  }
  if(v("staged_coarse_samples")<4*v("staged_coarse_order") ||
     v("staged_fine_samples")<4*v("staged_fine_order") ||
     v("staged_fine_samples")<v("staged_coarse_samples")) {
   reason="Shift samples must be >= 4 * order and fine >= coarse";return false;
  }
  if(v("staged_capture_response") && !v("debug_mode")) {
   reason="Staged response capture requires debug mode";return false;
  }
 }
 reason.clear();return true;
}
inline bool ValidateScript(const std::string& text,
 const std::unordered_map<std::string,int>& values,std::string& reason) {
 if(!Validate(values,reason))return false;
 if(values.at("global_harmonic_staged_search")) {
  if(!SupportsStagedSearch(text)){reason="Script lacks complete staged search binding";return false;}
  if(text.find(".fromobjectarc(")!=std::string::npos) {
   reason="Open observation cannot execute closed staged search";return false;
  }
 }
 return true;
}
}
