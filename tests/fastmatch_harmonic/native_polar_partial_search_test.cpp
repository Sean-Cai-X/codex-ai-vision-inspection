#include "../../cxgeom/polar_discrete_harmonic/partial_search_receipt.h"
#include "../../cxgeom/polar_discrete_harmonic/partial_refinement.h"
#include <fstream>
#include "../../libtorchsegmentation/src/utils/json.hpp"
#include <algorithm>
#include <iostream>
using namespace cxgeom::polar;
using Z=std::complex<double>;
void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
int main()try{
 PartialRequest base;base.request_id="automatic-partial";base.reference_source="ref";base.observation_source="obs";
 base.config.maximum_hypotheses=4096;base.config.max_residual_px=1e-6;
 base.reference={{"r0","e",{-40,-10}},{"r1","e",{10,-30}},{"r2","e",{60,20}},
 {"r3","e",{-20,70}},{"r4","e",{-60,40}},{"r5","e",{30,60}},{"r6","e",{0,20}},{"r7","e",{70,-40}}};
 int cases=0;PartialRequest last;PartialSearchResult lastResult;
 for(double angle:{-179.5,23.4})for(int missing:{0,2,4}){
  auto r=base;Z rot=std::polar(1.2,angle*3.14159265358979323846/180),translation(17.25,-8.5);
  for(size_t i=missing;i<r.reference.size();++i){
   auto f=r.reference[i];f.stable_id="target"+std::to_string(17-i);
   f.point=rot*f.point+translation;r.observation.push_back(f);
  }
  r.observation.push_back({"out1","noise",{300,-200}});r.observation.push_back({"out2","noise",{330,-180}});
  // Only raw sets and parameters go into the search, never a truth transform.
  auto result=SearchPartial(r);
  check(result.search_complete&&!result.candidates.empty(),"automatic search completed");
  bool found=false;
  for(const auto& c:result.candidates)
   if(std::abs(std::remainder(c.supplied.angle_deg-angle,360))<1e-8&&
      std::abs(c.supplied.scale-1.2)<1e-10&&std::abs(c.supplied.translation-translation)<1e-8){
    check(c.pairs.size()==size_t(8-missing)&&c.extra_observation_ids.size()==2,"ground truth membership");
    check(c.missing_reference_ids.size()==size_t(missing)&&c.observed_max_px.value()<1e-8,"missing and residual");
    found=true;
   }
  check(found&&!result.production_eligible,"truth recovered, audit only");
  auto shuffled=r;std::reverse(shuffled.reference.begin(),shuffled.reference.end());
  std::reverse(shuffled.observation.begin(),shuffled.observation.end());
  auto replay=SearchPartial(shuffled);
  check(replay.status==result.status&&replay.attempted_seeds==result.attempted_seeds&&
        replay.pair_checks==result.pair_checks&&replay.candidates.size()==result.candidates.size(),"replay");
  for(size_t i=0;i<replay.candidates.size();++i)
   check(replay.candidates[i].supplied.translation==result.candidates[i].supplied.translation,"replay pose");
  std::cout<<"AUTO missing="<<missing<<" extras=2 candidates="<<result.candidates.size()
           <<" seeds="<<result.attempted_seeds<<" checks="<<result.pair_checks<<std::endl;
  last=r;lastResult=result;++cases;
 }
 auto bounded=last;bounded.config.maximum_hypotheses=lastResult.attempted_seeds;
 bounded.config.maximum_pair_checks=lastResult.pair_checks;
 check(SearchPartial(bounded).search_complete,"exact budgets");
 --bounded.config.maximum_hypotheses;auto r=SearchPartial(bounded);
 check(!r.search_complete&&r.candidates.empty()&&r.status=="SEED_BUDGET_EXHAUSTED","seed cutoff");
 bounded=last;bounded.config.maximum_pair_checks=lastResult.pair_checks-1;r=SearchPartial(bounded);
 check(!r.search_complete&&r.candidates.empty()&&r.status=="PAIR_BUDGET_EXHAUSTED","pair cutoff");
 PartialRequest square=base;square.reference={{"a","e",{1,1}},{"b","e",{-1,1}},{"c","e",{-1,-1}},{"d","e",{1,-1}}};
 square.observation=square.reference;r=SearchPartial(square);
 check(r.search_complete&&r.status=="AMBIGUOUS"&&r.candidates.size()==4,"square ambiguity");
 std::vector<double> symmetricAngles;
 for(const auto& candidate:r.candidates){
  auto fit=RefinePartial(square,candidate.supplied);
  const auto& verified=fit.accepted?fit.after:fit.before;
  check(verified.accepted,"symmetric pose retained");
  symmetricAngles.push_back(verified.supplied.angle_deg);
 }
 std::sort(symmetricAngles.begin(),symmetricAngles.end());
 for(size_t i=1;i<symmetricAngles.size();++i)
  check(symmetricAngles[i]-symmetricAngles[i-1]>80,"real symmetry not collapsed");
 auto invalidSeed=lastResult.candidates.front().supplied;invalidSeed.translation+=Z(1000,1000);
 check(!RefinePartial(last,invalidSeed).accepted,"wrong seed rejected");
 invalidSeed.source_id.clear();
 check(!RefinePartial(last,invalidSeed).accepted,"missing provenance rejected");
 PartialSearchConfig cfg;cfg.maximum_candidates=1;r=SearchPartial(square,cfg);
 check(!r.search_complete&&r.candidates.empty()&&r.status=="CANDIDATE_CAPACITY_EXHAUSTED","capacity");
 cfg.scale_min=-1;check(SearchPartial(last,cfg).status=="INVALID_SEARCH_CONFIG","invalid search config");
 for(double noise:{.005,.05}){
  auto noisy=last;noisy.config.max_residual_px=.3;
  for(size_t i=0;i<4;++i)noisy.observation[i].point+=Z(noise*std::sin(double(i+1)),noise*std::cos(double(2*i+1)));
  auto result=SearchPartial(noisy);
  check(result.search_complete&&result.status=="AMBIGUOUS","noise retains near-pose ambiguity");
  bool found=false;
  for(const auto& c:result.candidates){
   if(c.pairs.size()==4&&c.extra_observation_ids.size()==2&&c.missing_reference_ids.size()==4&&
      std::abs(std::remainder(c.supplied.angle_deg-23.4,360))<.2&&
      std::abs(c.supplied.scale-1.2)<.005&&std::abs(c.supplied.translation-Z(17.25,-8.5))<.3)
    found=true;
  }
  check(found,"noisy true membership and bounded pose error");
  std::optional<Hypothesis> common;
  for(const auto& c:result.candidates){
   auto fit=RefinePartial(noisy,c.supplied);
   check(fit.accepted&&!fit.production_eligible,"noisy refit verified");
   check(*fit.after.observed_rms_px<=*fit.before.observed_rms_px,"nonincreasing observed RMS");
   if(common)check(fit.after.supplied.translation==common->translation&&
    fit.after.supplied.angle_deg==common->angle_deg&&fit.after.supplied.scale==common->scale,"same pairs same fit");
   common=fit.after.supplied;
   auto exact=noisy;exact.config.maximum_pair_checks=fit.pair_checks;
   check(RefinePartial(exact,c.supplied).accepted,"refit exact budget");
   --exact.config.maximum_pair_checks;
   check(!RefinePartial(exact,c.supplied).accepted,"refit cutoff budget");
   auto shuffled=noisy;std::reverse(shuffled.reference.begin(),shuffled.reference.end());
   std::reverse(shuffled.observation.begin(),shuffled.observation.end());
   auto replay=RefinePartial(shuffled,c.supplied);
   check(replay.accepted&&replay.after.supplied.translation==common->translation,"refit deterministic");
   std::cout<<"REFIT noise="<<noise<<" rms_before="<<*fit.before.observed_rms_px
    <<" rms_after="<<*fit.after.observed_rms_px<<" max_after="<<*fit.after.observed_max_px<<std::endl;
   std::cout<<"REFIT_POSE source="<<fit.before.supplied.source_id
    <<" angle_before="<<fit.before.supplied.angle_deg<<" angle_after="<<fit.after.supplied.angle_deg
    <<" scale_before="<<fit.before.supplied.scale<<" scale_after="<<fit.after.supplied.scale<<std::endl;
  }
  auto receipt=PartialSearchReceiptV1(result);auto json=nlohmann::json::parse(receipt);
  check(json.at("candidates").size()==result.candidates.size(),"receipt candidates");
  check(json.at("production_eligible")==false,"receipt audit only");
  std::reverse(noisy.reference.begin(),noisy.reference.end());std::reverse(noisy.observation.begin(),noisy.observation.end());
  check(receipt==PartialSearchReceiptV1(SearchPartial(noisy)),"byte identical noisy replay");
  std::cout<<"NOISY_PARTIAL noise="<<noise<<" candidates="<<result.candidates.size()<<std::endl;
 }
 auto receipt=PartialSearchReceiptV1(lastResult);auto json=nlohmann::json::parse(receipt);
 check(!lastResult.deduplication.empty(),"exact duplicate provenance retained");
 check(json.at("deduplicated_seeds")==lastResult.deduplication.size(),"dedup receipt count");
 check(json.at("deduplication").size()==lastResult.deduplication.size(),"dedup receipt records");
 auto rejects=[&](PartialSearchResult bad){
  bool rejected=false;try{(void)PartialSearchReceiptV1(bad);}catch(const std::invalid_argument&){rejected=true;}
  check(rejected,"invalid receipt rejected");
 };
 auto bad=lastResult;bad.production_eligible=true;rejects(bad);
 bad=lastResult;bad.search_complete=false;rejects(bad);
 bad=lastResult;bad.candidates.clear();rejects(bad);
 bad=lastResult;bad.candidates[0].supplied.scale=std::numeric_limits<double>::quiet_NaN();rejects(bad);
 auto cutoff=last;cutoff.config.maximum_hypotheses=1;
 json=nlohmann::json::parse(PartialSearchReceiptV1(SearchPartial(cutoff)));
 bad=lastResult;bad.candidates[0].accepted=false;rejects(bad);
 bad=lastResult;bad.assessed_hypotheses=bad.attempted_seeds+1;rejects(bad);
 std::ofstream receiptFile("polar_partial_search_receipt.json");receiptFile<<receipt;
 check(bool(receiptFile),"receipt evidence persisted");
 check(json.at("candidates").empty()&&json.at("search_complete")==false,"failure receipt no pose");
 std::cout<<"PARTIAL_SEARCH_PASS cases="<<cases<<"; independent automatic seeds, not production"<<std::endl;
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}
