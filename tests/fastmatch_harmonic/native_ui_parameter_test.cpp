#include "../../cximage/CxHarmonicEvidenceParameters.h"
#include <iostream>
#include <stdexcept>
int main() {
 using namespace cxharmonicui;
 auto require=[](bool v){if(!v)throw std::runtime_error("parameter contract failed");};
 std::unordered_map<std::string,int> values;std::string why;
 require(Defaults("// harmonic_default global_harmonic_anchor_x 35\n",values,why));
 require(values.at("global_harmonic_anchor_x")==35 && Validate(values,why));
 require(IsCase("// harmonic_evidence_binding: 1"));
 require(!IsCase("FastMatch audit;"));
 values["global_harmonic_sample_count"]=32;
 values["global_harmonic_max_order"]=16;require(!Validate(values,why));
 values["global_harmonic_max_order"]=15;require(Validate(values,why));
 values.erase("global_harmonic_method");require(!Validate(values,why));
 for(const auto* text:{"// harmonic_default unknown 1",
     "// harmonic_default global_harmonic_method 2",
     "// harmonic_default global_harmonic_method x",
     "// harmonic_default global_harmonic_method 0 extra"}) require(!Defaults(text,values,why));
 require(Defaults("",values,why));
 values["global_harmonic_coarse_angle_step"]=5;require(!Validate(values,why));
 values.erase("global_harmonic_coarse_angle_step");
 values["global_harmonic_receipt_path"]=0;require(!Validate(values,why));
 values.erase("global_harmonic_receipt_path");
 values["unrelated_tool_parameter"]=42;require(Validate(values,why));
 values["global_harmonic_sample_count"]=1025;require(!Validate(values,why));
 std::cout<<"HARMONIC_UI_PARAMETER_GUARDS_PASS\n";
}
