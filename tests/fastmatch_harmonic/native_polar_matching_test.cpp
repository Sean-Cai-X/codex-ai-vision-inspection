#include "../../cxgeom/polar_discrete_harmonic/matching.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace cxgeom::polar;
using Z=std::complex<double>;
void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
int main()try{
 constexpr double pi=3.14159265358979323846;
 std::vector<Feature> a={{"a","a",{-3,-1}},{"b","b",{1,-2}},{"c","c",{4,1}},
                         {"d","d",{-1,3}},{"e","e",{0,-1}}};
 int cases=0;
 for(double angle:{-179.5,23.4,179.5})for(double scale:{.8,1.2}){
  auto b=a;Z rot=std::polar(scale,angle*pi/180),tr(17.25,-8.5);
  for(auto& f:b){f.point=rot*f.point+tr;f.stable_id="target_"+f.stable_id;}
  std::reverse(b.begin(),b.end());MatchConfig cfg;cfg.max_residual_px=1e-6;
  auto r=MatchFull("reference",a,"target",b,cfg);
  check(r.search_complete&&r.status=="FULL_SET_AUDIT_CANDIDATES"&&r.candidates.size()==1,"unique full match");
  auto p=r.candidates.front();
  check(std::abs(std::remainder(p.angle_deg-angle,360))<1e-8,"angle");
  check(std::abs(p.scale-scale)<1e-12&&std::abs(p.translation-tr)<1e-10,"scale translation");
  check(p.max_px<1e-10&&p.pairs.size()==a.size(),"original geometry verification");
  for(const auto& pair:p.pairs)check(pair.target_id=="target_"+pair.reference_id,"correspondence");
  for(int repeat=0;repeat<100;++repeat){
   std::rotate(b.begin(),b.begin()+1,b.end());auto s=MatchFull("reference",a,"target",b,cfg);
   check(s.status==r.status&&s.pair_checks==r.pair_checks&&s.candidates.size()==1,"repeat state");
   check(s.candidates[0].angle_deg==p.angle_deg&&s.candidates[0].translation==p.translation,"repeat pose");
  }
  check(!r.production_eligible,"audit scope");++cases;
  std::cout<<"POSE angle="<<p.angle_deg<<" scale="<<p.scale<<" tx="<<p.translation.real()<<" ty="<<p.translation.imag()<<std::endl;
  std::cout<<"OBSERVED rms="<<p.rms_px<<" max="<<p.max_px<<" pairs="<<p.pairs.size()<<" roots="<<r.evaluated_candidates<<std::endl;
 }
 std::vector<Feature> square={{"a","a",{1,1}},{"b","b",{-1,1}},{"c","c",{-1,-1}},{"d","d",{1,-1}}};
 MatchConfig cfg;cfg.max_residual_px=1e-6;
 auto r=MatchFull("square",square,"square2",square,cfg);
 check(r.status=="AMBIGUOUS"&&r.candidates.size()==4,"four square rotations");
 cfg.maximum_candidates=1;r=MatchFull("square",square,"square2",square,cfg);
 check(!r.search_complete&&r.candidates.empty()&&r.status=="CANDIDATE_BUDGET_EXHAUSTED","candidate budget");
 cfg=MatchConfig{};cfg.maximum_pair_checks=1;r=MatchFull("a",a,"b",a,cfg);
 check(!r.search_complete&&r.candidates.empty()&&r.status=="PAIR_BUDGET_EXHAUSTED","pair budget");
 auto missing=a;missing.pop_back();r=MatchFull("a",a,"b",missing);
 check(r.status=="UNSUPPORTED_PARTIAL_SET"&&r.candidates.empty(),"no partial claim");
 auto weighted=a;weighted[0].weight=.5;r=MatchFull("a",a,"b",weighted);
 check(r.status=="UNSUPPORTED_NONUNIFORM_WEIGHTS","weight contract");
 auto wrong=a;wrong[0].point+=Z(8,9);cfg=MatchConfig{};cfg.max_spectral_distance=100;cfg.max_residual_px=1e-6;
 r=MatchFull("a",a,"wrong",wrong,cfg);
 check(r.search_complete&&r.status=="NO_SPATIAL_MATCH"&&r.candidates.empty(),"spectrum cannot bypass spatial gate");
 cfg.scale_min=2;cfg.scale_max=3;r=MatchFull("a",a,"b",a,cfg);
 check(r.status=="SCALE_RANGE_REJECTED"&&r.candidates.empty(),"scale guard");
 std::cout<<"POLAR_FULL_MATCH_PASS transforms="<<cases<<" repeats=100; symmetry/budgets/partial/spatial guards"<<std::endl;
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}
