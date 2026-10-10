#include "../../cxgeom/polar_discrete_harmonic/partial_pipeline_receipt.h"
#include "../../libtorchsegmentation/src/utils/json.hpp"
#include <algorithm>
#include <set>
#include <fstream>
#include <iostream>
using namespace cxgeom::polar;
using Z=std::complex<double>;
void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
int main(int argc,char** argv)try{
 check(argc==2,"external evidence path required");std::ofstream evidence(argv[1]);
 check(bool(evidence),"evidence opened");
 PartialRequest base;base.reference_source="reference";base.observation_source="observation";
 base.config.maximum_hypotheses=4096;base.config.max_residual_px=.3;
 base.reference={{"r0","e",{-40,-10}},{"r1","e",{10,-30}},{"r2","e",{60,20}},
 {"r3","e",{-20,70}},{"r4","e",{-60,40}},{"r5","e",{30,60}},{"r6","e",{0,20}},{"r7","e",{70,-40}}};
 int count=0;size_t maxChecks=0;double maxAngle=0,maxTranslation=0,maxRms=0;
 size_t maxGroupedChecks=0,maxGroupedCandidates=0;
 PartialPipelineResult last;
 for(double angle:{-179.5,23.4,179.5})for(double scale:{.6,1.8})
 for(int missing:{0,2,4})for(int extras:{0,2})for(double noise:{.005,.05}){
  auto request=base;request.request_id="matrix-"+std::to_string(count);
  auto factor=std::polar(scale,angle*3.14159265358979323846/180);Z translation(17.25,-8.5);
  for(size_t i=missing;i<8;++i){
   auto f=base.reference[i];f.stable_id="target"+std::to_string(17-i);
   f.point=factor*f.point+translation+Z(noise*std::sin(double(i+1)),noise*std::cos(double(2*i+1)));
   request.observation.push_back(f);
  }
  for(int i=0;i<extras;++i)request.observation.push_back({"extra"+std::to_string(i),"noise",{300.+30*i,-200.+20*i}});
  if(count==0){
   auto limited=SearchRefinedPartial(request);
   check(!limited.complete&&limited.candidates.empty()&&limited.status=="CANDIDATE_CAPACITY_EXHAUSTED","default capacity boundary");
   evidence<<PartialPipelineReceiptV1(limited)<<'\n';
  }
  PartialSearchConfig matrixConfig;matrixConfig.maximum_candidates=128;
  auto result=SearchRefinedPartial(request,matrixConfig);
  check(result.raw.executed_search.maximum_candidates==128,"explicit matrix capacity");
  std::cout<<"CASE "<<count<<" missing="<<missing<<" extras="<<extras<<" noise="<<noise<<" status="<<result.status<<std::endl;
  check(result.complete&&!result.candidates.empty()&&!result.production_eligible,"matrix complete audit candidate");
  PartialSearchConfig groupedConfig;groupedConfig.refine_before_capacity=true;
  auto grouped=SearchRefinedPartial(request,groupedConfig);
  check(grouped.complete&&!grouped.candidates.empty(),"online grouping capacity32 completion");
  maxGroupedChecks=std::max(maxGroupedChecks,grouped.pair_checks);
  maxGroupedCandidates=std::max(maxGroupedCandidates,grouped.raw.candidates.size());
  auto signatures=[](const PartialPipelineResult& value){
   std::set<std::vector<std::pair<std::string,std::string>>> keys;
   for(const auto& c:value.candidates){
    std::vector<std::pair<std::string,std::string>> key;
    for(const auto& p:c.pairs)key.push_back({p.reference_id,p.target_id});
    keys.insert(key);
   }
   return keys;
  };
  check(signatures(grouped)==signatures(result),"online grouping preserves different correspondences");
  check(!grouped.raw.online_refits.empty(),"online trace retained");
  auto groupedReceipt=PartialPipelineReceiptV1(grouped);
  (void)nlohmann::json::parse(groupedReceipt);
  evidence<<groupedReceipt<<'\n';
  auto exact=request;exact.config.maximum_pair_checks=grouped.pair_checks;
  check(SearchRefinedPartial(exact,groupedConfig).complete,"online shared exact budget");
  --exact.config.maximum_pair_checks;
  auto cut=SearchRefinedPartial(exact,groupedConfig);
  check(!cut.complete&&cut.candidates.empty(),"online shared cutoff");
  auto reversed=request;std::reverse(reversed.reference.begin(),reversed.reference.end());
  std::reverse(reversed.observation.begin(),reversed.observation.end());
  check(groupedReceipt==PartialPipelineReceiptV1(SearchRefinedPartial(reversed,groupedConfig)),"online byte replay");
  bool found=false;
  for(const auto& c:result.candidates){
   double ae=std::abs(std::remainder(c.supplied.angle_deg-angle,360));
   double te=std::abs(c.supplied.translation-translation);
   if(ae<.3&&te<.3&&std::abs(c.supplied.scale-scale)<.005&&
      c.pairs.size()==size_t(8-missing)){
    check(c.pairs.size()==size_t(8-missing)&&c.missing_reference_ids.size()==size_t(missing)&&
     c.extra_observation_ids.size()==size_t(extras),"matrix count membership");
    for(const auto& p:c.pairs){
     auto index=std::stoi(p.reference_id.substr(1));
     check(index>=missing&&p.target_id=="target"+std::to_string(17-index),"independent exact ID correspondence");
    }
    maxAngle=std::max(maxAngle,ae);maxTranslation=std::max(maxTranslation,te);
    maxRms=std::max(maxRms,*c.observed_rms_px);found=true;
   }
  }
  check(found,"independent truth recovered");
  auto receipt=PartialPipelineReceiptV1(result);auto json=nlohmann::json::parse(receipt);
  check(json.at("complete")==true&&json.at("candidates").size()==result.candidates.size(),"parsed matrix receipt");
  std::reverse(request.reference.begin(),request.reference.end());std::reverse(request.observation.begin(),request.observation.end());
  check(receipt==PartialPipelineReceiptV1(SearchRefinedPartial(request,matrixConfig)),"matrix replay");
  evidence<<receipt<<'\n';last=result;maxChecks=std::max(maxChecks,result.pair_checks);++count;
 }
 check(bool(evidence),"matrix evidence flushed");
 auto reject=[](const PartialPipelineResult& bad){
  bool threw=false;try{(void)PartialPipelineReceiptV1(bad);}catch(const std::invalid_argument&){threw=true;}
  check(threw,"inconsistent receipt must reject");
 };
 auto bad=last;bad.status="NO_VERIFIED_HYPOTHESIS";reject(bad);
 bad=last;bad.complete=false;reject(bad);
 bad=last;bad.production_eligible=true;reject(bad);
 bad=last;++bad.pair_checks;reject(bad);
 bad=last;bad.candidates.front().supplied.translation+=Z(10,0);reject(bad);
 bad=last;bad.candidates.front().production_eligible=true;reject(bad);
 std::cout<<"MATRIX_PASS cases="<<count<<" max_checks="<<maxChecks<<" max_angle_deg="<<maxAngle
  <<" max_translation_px="<<maxTranslation<<" max_rms_px="<<maxRms<<std::endl;
 std::cout<<"ONLINE_GROUPED max_checks="<<maxGroupedChecks<<" max_raw_candidates="<<maxGroupedCandidates<<std::endl;
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}
