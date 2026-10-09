#include "../../cximage/CxHarmonicStagedContract.h"
#include <iostream>
using namespace cxharmonic;
void need(bool b){if(!b)throw std::runtime_error("STAGED_CONTRACT_FAILED");}
int main()try{
 StagedSettings s;need(!s.enabled);
 need(!SetStagedParameter(s,2,"method"));
 need(SetStagedParameter(s,1,"staged_search")&&s.enabled);
 const char* keys[]={"staged_coarse_order","staged_fine_order","staged_coarse_samples",
  "staged_fine_samples","staged_refinement_iterations","staged_maximum_evaluations"};
 const double values[]={4,12,64,256,32,65536};
 for(int i=0;i<6;++i)need(SetStagedParameter(s,values[i],keys[i]));
 need(SetStagedParameter(s,1,"staged_capture_response")&&s.config.capture_response);
 auto reject=[&](double v,const char* key){
  bool fail=false;try{SetStagedParameter(s,v,key);}catch(const std::invalid_argument&){fail=true;}
  need(fail);
 };
 reject(.5,"staged_search");reject(2,"staged_search");reject(-1,"staged_fine_order");
 reject(262145,"staged_maximum_evaluations");reject(1,"staged_unknown");
 auto j=StagedReceipt(s,std::nullopt,"INVALID_STAGED_ORDER");
 need(j.at("status")=="NOT_RUN"&&j.at("executed").is_null());
 cxgeom::so2::StagedResult r;r.executed=s.config;r.evaluations=10;
 r.match.status="STAGED_SEARCH_BUDGET_EXHAUSTED";
 r.coarse_response.push_back({0,23.4,1,0});
 j=StagedReceipt(s,r,"");
 need(j.at("status")=="BUDGET_EXHAUSTED"&&j.at("evaluations")==10);
 need(j.at("coarse_response").size()==1&&!j.at("search_complete").get<bool>());
 need(j.at("executed")==StagedConfigJson(s.config));
 r.search_complete=true;r.match.status="AUDIT_STAGED_POSE_HYPOTHESES";
 j=StagedReceipt(s,r,"");need(j.at("status")=="COMPLETED");
 need(j.at("continuous_global_optimum_certified")==false);
 s.enabled=false;j=StagedReceipt(s,r,"old");
 need(j.at("status")=="DISABLED"&&j.at("coarse_response").empty()&&j.at("executed").is_null());
 std::cout<<"STAGED_CONTRACT_PASS parameters/requested/executed/budget/no-stale"<<std::endl;
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}
