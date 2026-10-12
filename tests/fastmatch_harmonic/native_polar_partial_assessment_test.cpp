#include "../../cxgeom/polar_discrete_harmonic/partial_assessment.h"
#include <algorithm>
#include <iostream>
using namespace cxgeom::polar;
using Z=std::complex<double>;
void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
int main()try{
 PartialRequest base;base.request_id="assessment";base.reference_source="ref";base.observation_source="obs";
 base.reference={{"r0","e",{-40,-10}},{"r1","e",{10,-30}},{"r2","e",{60,20}},
 {"r3","e",{-20,70}},{"r4","e",{-60,40}},{"r5","e",{30,60}},{"r6","e",{0,20}},{"r7","e",{70,-40}}};
 int cases=0;PartialRequest last;Hypothesis lastPose;
 for(double angle:{-179.5,23.4,179.5})for(double scale:{.8,1.2})
 for(int missing:{0,2,4})for(int extras:{0,2}){
  auto r=base;Hypothesis h;h.source_id="fixture_supplied_truth_not_solver";h.angle_deg=angle;h.scale=scale;h.translation={17.25,-8.5};
  Z rot=std::polar(scale,angle*3.14159265358979323846/180);
  for(size_t i=missing;i<r.reference.size();++i){
   auto f=r.reference[i];f.stable_id="t"+f.stable_id;f.point=rot*f.point+h.translation;r.observation.push_back(f);
  }
  for(int i=0;i<extras;++i)r.observation.push_back({"extra"+std::to_string(i),"noise",{300.+30*i,-200.+20*i}});
  auto result=AssessHypothesis(r,h);
  check(result.assessment_complete&&result.accepted&&result.status=="HYPOTHESIS_VERIFIED","supplied hypothesis assessment");
  check(result.pairs.size()==size_t(8-missing),"correspondence count");
  for(const auto& p:result.pairs)check(p.target_id=="t"+p.reference_id&&p.residual_px<1e-10,"spatial IDs");
  check(result.missing_reference_ids.size()==size_t(missing)&&result.extra_observation_ids.size()==size_t(extras),"missing and extra");
  check(std::abs(*result.reference_coverage-double(8-missing)/8)<1e-12,"reference coverage");
  check(std::abs(*result.observation_coverage-double(8-missing)/(8-missing+extras))<1e-12,"observation coverage");
  check(!result.production_eligible,"audit only");
  auto wrong=h;wrong.translation+=Z(1000,1000);auto rejected=AssessHypothesis(r,wrong);
  check(!rejected.accepted&&rejected.status=="INSUFFICIENT_MATCHES"&&!rejected.observed_rms_px,"wrong pose");
  auto bounded=r;bounded.config.maximum_pair_checks=result.pair_checks;
  check(AssessHypothesis(bounded,h).accepted,"exact budget");
  --bounded.config.maximum_pair_checks;auto exhausted=AssessHypothesis(bounded,h);
  check(!exhausted.assessment_complete&&!exhausted.accepted&&exhausted.pairs.empty(),"budget no partial output");
  check(!exhausted.reference_coverage&&exhausted.status=="PAIR_BUDGET_EXHAUSTED","budget no metrics");
  auto reordered=r;std::reverse(reordered.reference.begin(),reordered.reference.end());
  std::reverse(reordered.observation.begin(),reordered.observation.end());
  auto repeat=AssessHypothesis(reordered,h);
  check(repeat.status==result.status&&repeat.pair_checks==result.pair_checks&&repeat.observed_rms_px==result.observed_rms_px,"deterministic");
  last=r;lastPose=h;++cases;
 }
 auto strict=last;strict.config.minimum_reference_coverage=.75;
 check(AssessHypothesis(strict,lastPose).status=="COVERAGE_REJECTED","coverage gate");
 strict=last;strict.config.minimum_spatial_span=1;
 strict.observation.erase(strict.observation.begin()+3);strict.config.minimum_reference_coverage=.3;
 check(AssessHypothesis(strict,lastPose).status=="SPAN_REJECTED","span gate");
 auto ambiguous=last;auto copy=ambiguous.observation[0];copy.stable_id="duplicate-location";
 ambiguous.observation.push_back(copy);
 check(AssessHypothesis(ambiguous,lastPose).status=="AMBIGUOUS_CORRESPONDENCE","ambiguity not suppressed");
 auto invalid=lastPose;invalid.scale=0;
 check(AssessHypothesis(last,invalid).status=="INVALID_HYPOTHESIS","invalid hypothesis");
 std::cout<<"PARTIAL_ASSESSMENT_PASS fixtures="<<cases<<"; supplied poses only; no automatic solver"<<std::endl;
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}
