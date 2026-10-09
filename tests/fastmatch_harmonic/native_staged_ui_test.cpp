#include "../../cximage/CxHarmonicEvidenceParameters.h"
#include "../../cximage/CxHarmonicStagedPresentation.h"
#include "../../cximage/CxHarmonicStagedContract.h"
#include <fstream>
#include <iostream>
using J=nlohmann::json;
void need(bool b){if(!b)throw std::runtime_error("STAGED_UI_GUARD_FAILED");}
int main(int argc,char**argv)try {
 using namespace cxharmonicui;
 std::unordered_map<std::string,int> v;std::string why,binding;
 need(Defaults("",v,why));need(ValidateScript("",v,why));
 for(int i=23;i<31;++i) {
  const std::string k=parameters[i].key;
  binding+="a.parameter("+k+",\""+k.substr(16)+"\");\n";
 }
 need(SupportsStagedSearch(binding));need(!SupportsStagedSearch("// "+binding.substr(0,binding.find('\n'))));
 need(!SupportsStagedSearch("/*"+binding+"*/"));
 for(int i=23;i<31;++i){
  auto partial=binding;const std::string k=parameters[i].key;
  partial.replace(partial.find(k),k.size(),"unbound");need(!SupportsStagedSearch(partial));
 }
 v["global_harmonic_staged_search"]=1;
 need(!ValidateScript("",v,why));need(ValidateScript(binding,v,why));
 need(!ValidateScript(binding+"a.fromobjectarc(o);",v,why));
 v["global_harmonic_staged_fine_order"]=13;need(!Validate(v,why));
 v["global_harmonic_staged_fine_order"]=12;
 v["global_harmonic_staged_coarse_order"]=13;need(!Validate(v,why));
 v["global_harmonic_staged_coarse_order"]=4;
 v["global_harmonic_staged_coarse_samples"]=15;need(!Validate(v,why));
 v["global_harmonic_staged_coarse_samples"]=64;
 v["global_harmonic_staged_fine_samples"]=63;need(!Validate(v,why));
 v["global_harmonic_staged_fine_samples"]=256;
 v["global_harmonic_staged_capture_response"]=1;need(!Validate(v,why));
 v["global_harmonic_debug_mode"]=1;need(ValidateScript(binding,v,why));
 need(!Defaults("// harmonic_default global_harmonic_staged_search 1",v,why));
 need(!StagedDisplayData(J::object()).lines.empty());
 cxharmonic::StagedSettings settings;
 J run={{"staged_search",cxharmonic::StagedReceipt(settings,std::nullopt,"")}};
 need(StagedDisplayData(run).coarse.empty());
 settings.enabled=true;settings.config.capture_response=true;
 cxgeom::so2::StagedResult r;r.executed=settings.config;r.evaluations=1;
 r.match.status="STAGED_SEARCH_BUDGET_EXHAUSTED";r.coarse_response.push_back({0,23.4,.9,.1});
 run["staged_search"]=cxharmonic::StagedReceipt(settings,r,"");
 need(StagedDisplayData(run).coarse.size()==1);
 auto reject=[&](J bad){bool fail=false;try{StagedDisplayData(bad);}catch(const std::exception&){fail=true;}need(fail);};
 auto bad=run;bad["staged_search"]["coarse_response"][0]["correlation"]=2;reject(bad);
 bad=run;bad["staged_search"]["continuous_global_optimum_certified"]=true;reject(bad);
 bad=run;bad["staged_search"]["status"]="DISABLED";reject(bad);
 bad=run;bad["staged_search"]["evaluations"]=262145;reject(bad);
 bad=run;bad["staged_search"]["fine_response"]=J::array();
 for(int i=0;i<2049;++i)bad["staged_search"]["fine_response"].push_back(J::object());
 reject(bad);
 if(argc==2) {
  std::ifstream input(argv[1]);J receipt;input>>receipt;
  for(const auto& item:receipt.at("runs")) {
   auto view=StagedDisplayData(item);need(!view.lines.empty());
   need(view.coarse.size()==item.at("staged_search").at("coarse_response").size());
  }
 }
 std::cout<<"STAGED_UI_PASS bounds/binding/open/debug/malformed/legacy/receipt"<<std::endl;
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}
