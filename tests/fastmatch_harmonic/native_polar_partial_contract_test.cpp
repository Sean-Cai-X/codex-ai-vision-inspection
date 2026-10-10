#include "../../cxgeom/polar_discrete_harmonic/partial_contract.h"
#include "../../libtorchsegmentation/src/utils/json.hpp"
#include <fstream>
#include <iostream>
using namespace cxgeom::polar;
using Z=std::complex<double>;
void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
int main(int argc,char** argv)try{
 PartialRequest base;base.request_id="partial-benchmark";base.reference_source="ref";base.observation_source="obs";
 base.reference={{"r0","edge0",{-40,-10}},{"r1","edge1",{10,-30}},{"r2","edge2",{60,20}},
 {"r3","edge3",{-20,70}},{"r4","edge4",{-60,40}},{"r5","edge5",{30,60}},
 {"r6","edge6",{0,20}},{"r7","edge7",{70,-40}}};
 int cases=0;std::string last;
 for(double angle:{-179.5,23.4,179.5})for(double scale:{.8,1.2})
 for(int missing:{0,2,4})for(int extras:{0,2}){
  auto r=base;Z rot=std::polar(scale,angle*3.14159265358979323846/180),tr(17.25,-8.5);
  for(size_t i=missing;i<r.reference.size();++i){
   auto f=r.reference[i];f.stable_id="t"+f.stable_id;f.point=rot*f.point+tr;r.observation.push_back(f);
  }
  for(int i=0;i<extras;++i)r.observation.push_back({"extra"+std::to_string(i),"noise",{300.+30*i,-200.+20*i}});
  auto v=ValidatePartial(r);check(v.ready_for_solver,"valid partial request");
  auto ref=Encode(r.reference_source,r.reference),obs=Encode(r.observation_source,r.observation);
  double drift=std::abs(obs.centroid-(rot*ref.centroid+tr));
  if(missing||extras)check(drift>1,"centroid drift must remain visible");
  else check(drift<1e-10,"complete covariance");
  // Ground-truth membership is a test fixture only; validation does not receive it.
  check(r.observation.size()==8-size_t(missing)+size_t(extras),"fixture counts");
  last=PartialRequestReceiptV1(r);auto json=nlohmann::json::parse(last);
  check(json["execution_status"]=="NOT_RUN"&&json["pose"].is_null()&&json["coverage"].is_null(),"not solved");
  check(json["missing_reference_ids"].is_null()&&json["extra_observation_ids"].is_null(),"not guessed");
  check(json["executed"].is_null()&&json["requested"]["minimum_matches"]==3,"requested not executed");
  auto reordered=r;std::reverse(reordered.observation.begin(),reordered.observation.end());
  check(PartialRequestReceiptV1(reordered)==last,"request receipt repeat");
  std::cout<<"FIXTURE missing="<<missing<<" extra="<<extras<<" centroid_drift="<<drift<<std::endl;
  ++cases;
 }
 auto r=base;r.observation=r.reference;
 auto bad=r;bad.observation.resize(2);
 check(ValidatePartial(bad).reason=="PARTIAL_INSUFFICIENT_FEATURES","too few");
 bad=r;bad.config.maximum_hypotheses=0;
 check(ValidatePartial(bad).reason=="PARTIAL_INVALID_BUDGET","budget");
 bad=r;bad.observation.resize(3);bad.config.minimum_reference_coverage=.8;
 check(ValidatePartial(bad).reason=="PARTIAL_COVERAGE_IMPOSSIBLE","coverage cardinality bound");
 bad=r;bad.config.minimum_spatial_span=std::numeric_limits<double>::quiet_NaN();
 check(ValidatePartial(bad).reason=="PARTIAL_INVALID_THRESHOLDS","nonfinite");
 check(nlohmann::json::parse(PartialRequestReceiptV1(bad))["requested"].is_null(),"invalid JSON config omitted");
 bad=r;bad.request_id.clear();
 check(ValidatePartial(bad).reason=="PARTIAL_MISSING_PROVENANCE","provenance");
 bad=r;bad.observation[1].stable_id=bad.observation[0].stable_id;
 check(ValidatePartial(bad).reason=="POLAR_INVALID_FEATURE_ID","duplicate ids");
 PartialResult result;PartialCandidate candidate;
 check(result.status=="NOT_RUN"&&result.candidates.empty()&&!result.production_eligible,"default result");
 check(!candidate.pose&& !candidate.reference_coverage,"no identity or fake coverage");
 if(argc==2){std::ofstream o(argv[1]);o<<last;o.flush();check(bool(o),"receipt write");}
 std::cout<<"PARTIAL_CONTRACT_PASS fixtures="<<cases<<"; solver NOT_IMPLEMENTED"<<std::endl;
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}
