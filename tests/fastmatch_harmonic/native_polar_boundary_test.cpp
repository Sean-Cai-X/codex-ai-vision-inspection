#include "../../cxgeom/polar_discrete_harmonic/receipt.h"
#include "../../libtorchsegmentation/src/utils/json.hpp"
#include <iostream>
#include <functional>
#include <limits>
using namespace cxgeom::polar;
void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
void reject(const std::function<void()>& f,const char* why){
 try{f();}catch(const std::invalid_argument& e){check(std::string(e.what())==why,e.what());return;}
 throw std::runtime_error("expected rejection");
}
int main()try{
 std::vector<Feature> a={{"a","edge",{1,1}},{"b","edge",{-1,1}},{"c","edge",{-1,-1}},{"d","edge",{1,-1}}};
 MatchConfig cfg;cfg.max_residual_px=1e-6;
 auto full=MatchFull("a",a,"b",a,cfg);
 check(full.search_complete&&full.candidates.size()==4,"baseline symmetry");
 for(auto limits:{std::pair<double,double>{170,-170},{-180,-180},{180,180}}){
  cfg.angle_min_deg=limits.first;cfg.angle_max_deg=limits.second;
  auto r=MatchFull("a",a,"b",a,cfg);
  check(r.search_complete&&r.candidates.size()==1,"seam windows");
  check(std::abs(std::abs(r.candidates[0].angle_deg)-180)<1e-10,"seam pose");
  auto j=nlohmann::json::parse(ReceiptV1(r));
  check(j["executed"]["angle_min_deg"]==limits.first&&j["executed"]["angle_max_deg"]==limits.second,"executed window");
 }
 cfg=MatchConfig{};cfg.max_residual_px=1e-6;
 cfg.maximum_candidates=full.evaluated_candidates;
 check(MatchFull("a",a,"b",a,cfg).search_complete,"exact root budget");
 --cfg.maximum_candidates;
 auto exhausted=MatchFull("a",a,"b",a,cfg);
 check(!exhausted.search_complete&&exhausted.candidates.empty(),"discard partial candidate list");
 check(exhausted.status=="CANDIDATE_BUDGET_EXHAUSTED","root budget status");
 auto j=nlohmann::json::parse(ReceiptV1(exhausted));
 check(j["candidates"].empty()&&j["search_complete"]==false,"root budget receipt");
 check(j["evaluated_candidates"]==cfg.maximum_candidates&&!exhausted.reason.empty(),"root budget counters");
 cfg=MatchConfig{};cfg.max_residual_px=1e-6;cfg.maximum_pair_checks=full.pair_checks;
 check(MatchFull("a",a,"b",a,cfg).candidates.size()==4,"exact pair budget");
 --cfg.maximum_pair_checks;exhausted=MatchFull("a",a,"b",a,cfg);
 check(!exhausted.search_complete&&exhausted.candidates.empty()&&exhausted.status=="PAIR_BUDGET_EXHAUSTED","pair cutoff");
 check(exhausted.pair_checks==cfg.maximum_pair_checks,"pair bound");
 j=nlohmann::json::parse(ReceiptV1(exhausted));
 check(j["candidates"].empty()&&j["status"]=="PAIR_BUDGET_EXHAUSTED","pair receipt");
 cfg=MatchConfig{};cfg.min_phase_amplitude=100;
 auto weak=MatchFull("a",a,"b",a,cfg);
 check(weak.status=="ORIENTATION_UNOBSERVABLE"&&weak.candidates.empty(),"phase refusal");
 reject([&]{MatchConfig c;c.angle_min_deg=std::numeric_limits<double>::quiet_NaN();MatchFull("a",a,"b",a,c);},"POLAR_INVALID_ANGLE_RANGE");
 reject([&]{MatchConfig c;c.maximum_candidates=0;MatchFull("a",a,"b",a,c);},"POLAR_INVALID_MATCH_LIMIT");
 reject([&]{MatchConfig c;c.maximum_pair_checks=0;MatchFull("a",a,"b",a,c);},"POLAR_INVALID_MATCH_LIMIT");
 reject([&]{auto r=full;r.search_complete=false;ReceiptV1(r);},"POLAR_RECEIPT_INCOMPLETE_CANDIDATES");
 reject([&]{auto r=full;r.production_eligible=true;ReceiptV1(r);},"POLAR_RECEIPT_PRODUCTION_CLAIM");
 reject([&]{auto r=full;r.candidates.clear();ReceiptV1(r);},"POLAR_RECEIPT_INVALID_STATE");
 reject([&]{auto r=full;r.status="NO_SPATIAL_MATCH";ReceiptV1(r);},"POLAR_RECEIPT_INVALID_STATE");
 reject([&]{ReceiptV1(MatchResult{});},"POLAR_RECEIPT_INVALID_STATE");
 std::cout<<"POLAR_BOUNDARY_PASS seam/exact_budget/partial_discard/failure_receipts"<<std::endl;
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}
