#include "muParser.h"
#include "../../cximage/CxPartialMatchAudit.h"
#include "../../libtorchsegmentation/src/utils/json.hpp"
#include <iostream>
#include <sstream>
#include <filesystem>
using J=nlohmann::json;
void need(bool b){if(!b)throw std::runtime_error("partial_script_contract");}
int main(int argc,char** argv)try{
 need(argc==2);std::filesystem::path root(argv[1]);std::filesystem::create_directories(root);
 std::ostringstream script;
 script<<"PartialMatchAudit p; p.requestid(\"script-partial\"); p.parameter(4096,\"maximum_hypotheses\"); p.parameter(1,\"refine_before_capacity\");";
 const double points[5][2]={{-40,-10},{10,-30},{60,20},{-20,70},{30,60}};
 for(int side=0;side<2;++side){
  script<<"p.select("<<side<<"); p.source(\""<<(side?"obs":"ref")<<"\");";
  for(int i=side;i<5;++i){
   const double x=points[i][0],y=points[i][1];
   script<<"p.featureid(\""<<(side?"t":"r")<<i<<"\");p.point("<<(side?-1.2*y+17:x)<<","<<(side?1.2*x-8:y)<<");";
  }
  if(side)script<<"p.featureid(\"extra\");p.point(300,-200);";
 }
 auto receipt=(root/"partial_script_receipt.json").string();
 script<<"p.run();p.expectstatus(\"PARTIAL_AUDIT_CANDIDATE\");p.expectcount(1);p.save(\""<<receipt<<"\");";
 {std::ofstream file(root/"partial_case.cxsc");file<<script.str();need(bool(file));}
 mu::Parser parser;double* org=nullptr;parser.DefineOrgClass("double",org);parser.UsingClass(true);
 RegisterPartialMatchAudit(parser);
 parser.SetExpr(script.str());parser.Eval();
 std::ifstream input(receipt);J j;input>>j;
 need(j.at("complete")==true&&j.at("production_eligible")==false);
 const auto& pose=j.at("candidates").at(0).at("assessment").at("pose");
 need(std::abs(pose.at("angle_deg").get<double>()-90)<1e-8);
 need(std::abs(pose.at("scale").get<double>()-1.2)<1e-8);
 need(std::abs(pose.at("translation_x").get<double>()-17)<1e-8);
 need(std::abs(pose.at("translation_y").get<double>()+8)<1e-8);
 auto* bound=static_cast<CxPartialMatchAudit*>(parser.GetClassObj("PartialMatchAudit","p"));need(bound!=nullptr);
 bound->parameter(.2,"max_residual_px");
 bool staleRejected=false;
 try{bound->save((root/"stale.json").string().c_str());}catch(const std::runtime_error&){staleRejected=true;}
 need(staleRejected&&!std::filesystem::exists(root/"stale.json"));
 auto failedPath=(root/"partial_budget_receipt.json").string();
 mu::Parser failureParser;failureParser.DefineOrgClass("double",org);failureParser.UsingClass(true);
 RegisterPartialMatchAudit(failureParser);
 auto failedScript=script.str()+"p.parameter(1,\"maximum_hypotheses\");p.run();p.expectstatus(\"SEED_BUDGET_EXHAUSTED\");p.expectcount(0);p.save(\""+failedPath+"\");";
 {std::ofstream file(root/"partial_budget_case.cxsc");file<<failedScript;need(bool(file));}
 failureParser.SetExpr(failedScript);failureParser.Eval();
 std::ifstream failedInput(failedPath);J failed;failedInput>>failed;
 need(failed.at("complete")==false&&failed.at("candidates").empty());
 // Adapter invalidation guards are also checked independently of parser exception wrapping.
 CxPartialMatchAudit adapter;bool rejected=false;
 try{adapter.parameter(1,"unknown");}catch(const std::invalid_argument&){rejected=true;}need(rejected);
 rejected=false;try{adapter.point_script(2,1);}catch(const std::invalid_argument&){rejected=true;}need(rejected);
 rejected=false;try{adapter.save((root/"should_not_exist.json").string().c_str());}catch(const std::runtime_error&){rejected=true;}need(rejected);
 std::cout<<"PARTIAL_CXSCRIPT_PASS real_parser=true geometry_audit_only=true"<<std::endl;return 0;
}catch(const mu::Parser::exception_type& e){std::cerr<<e.GetMsg()<<std::endl;return 1;}
catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}
