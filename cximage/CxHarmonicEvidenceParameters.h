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
 {"global_harmonic_roi_h","Source ROI height (px)",240,1,100000}
};
inline bool IsCase(const std::string& text) {
 return text.find("// harmonic_evidence_binding: 1")!=std::string::npos;
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
 reason.clear();return true;
}
inline bool Validate(const std::unordered_map<std::string,int>& values,std::string& reason) {
 for(const auto& p:parameters) {
  auto i=values.find(p.key);
  if(i==values.end()||i->second<p.minimum||i->second>p.maximum) {
   reason=std::string("Invalid/missing parameter: ")+p.key;return false;
  }
 }
 if(values.at("global_harmonic_max_order")>=values.at("global_harmonic_sample_count")/2) {
  reason="Maximum harmonic order must be < resample points / 2";return false;
 }
 reason.clear();return true;
}
}
