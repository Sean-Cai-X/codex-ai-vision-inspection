#include "muParser.h"
#include "../../cximage/CxRefinedArcAudit.h"
#include <iostream>
namespace fs=std::filesystem;using J=nlohmann::json;
void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
void write(const fs::path& p,const std::string& s){std::ofstream f(p);f<<s;f.close();check(bool(f),"write");}
J read(const fs::path& p){std::ifstream f(p);J j;f>>j;return j;}
void replay(const std::string& script,const fs::path& receipt){
 mu::Parser parser;double* org=nullptr;parser.DefineOrgClass("double",org);parser.UsingClass(true);
 RegisterRefinedArcAudit(parser);parser.SetExpr(script);parser.Eval();
 auto* audit=static_cast<CxRefinedArcAudit*>(parser.GetClassObj("RefinedArcAudit","a"));
 check(audit!=nullptr,"parser object");write(receipt,audit->receipt());
}
int main(int argc,char** argv)try{
 check(argc>=2,"output path");auto root=fs::absolute(argv[1]);fs::create_directories(root);
 if(argc==3&&std::string(argv[2])=="verify-headless"){
  check(read(root/"native.json")==read(root/"script_saved.json"),"native and headless parity");
  check(read(root/"native.json")==read(root/"headless/refined_arc_receipt_0.json"),"runtime capture parity");
  check(fs::exists(root/"headless/manual_algorithm_review_handoff.md"),"formal handoff");
  std::cout<<"REFINED_ARC_HEADLESS_PARITY_PASS\n";return 0;
 }
 const bool common=argc==3&&(std::string(argv[2])=="common"||std::string(argv[2])=="common-reject");
 const bool reject=argc==3&&std::string(argv[2])=="common-reject";
 cv::Mat image(64,64,CV_8UC1);
 for(int y=0;y<64;++y)for(int x=0;x<64;++x)
  image.at<unsigned char>(y,x)=cv::saturate_cast<unsigned char>(128+100*std::erf((x-31.3)/1.6));
 check(cv::imwrite((root/"source.pgm").string(),image),"image");
 std::string script="RefinedArcAudit a;\n";
 for(int side:{0,1}){
  script+="a.select("+std::to_string(side)+");\na.caseid(\"side"+std::to_string(side)+"\");\na.revision(\"v1\");\na.image(\""+(root/"source.pgm").generic_string()+"\");\na.span(0);\na.grayparameter(20,\"minimum_contrast\");\n";
 }
 if(common){
  script+="a.select(0); a.anchor(0,1); a.select(1); a.anchor(0,1); a.correspondence(\"explicit_fixture_endpoint_pair\"); a.parameter(1,\"common_support_enabled\");\n";
  if(reject)script+="a.parameter(1,\"minimum_common_anchor_fraction\");\n";
 }
 script+="a.parameter(32,\"samples\");\na.parameter(0.25,\"minimum_scale\");\na.parameter(4,\"maximum_scale\");\na.parameter(0.02,\"maximum_support_endpoint_fraction_delta\");\na.run();\na.expectstatus(\"UNIQUE_ARC_CANDIDATE\");\na.save(\""+(root/"script_saved.json").generic_string()+"\");\n";
 if(reject){
  const std::string token="UNIQUE_ARC_CANDIDATE";
  auto pos=script.find(token);check(pos!=std::string::npos,"expected status");
  script.replace(pos,token.size(),"COMMON_ARC_INSUFFICIENT_COVERAGE");
 }
 write(root/"run.cxsc",script);replay(script,root/"native.json");
 if(!reject)check(read(root/"native.json")["refined"]["config"]["samples"]==32,"script config propagated");
 if(common){
  auto j=read(root/"native.json");check(j["mode"]=="explicit_common_raw_arc","common mode");
  check(j["anchor_correspondence_verified"]==false,"no fake correspondence proof");
  if(reject)check(j["refined"]["accepted_indices"].empty(),"rejected receipt exported");
 }
 // Force a stale receipt then fail; neither missing spans nor invalid config
 // may expose the last successful comparison.
 CxRefinedArcAudit audit;
 for(int side:{0,1}){audit.select(side);audit.caseid(("side"+std::to_string(side)).c_str());audit.image((root/"source.pgm").string().c_str());audit.span(0);}
 audit.run();audit.grayparameter(250,"minimum_contrast");
 bool no_run=false;try{audit.receipt();}catch(const std::runtime_error&){no_run=true;}check(no_run,"mutation clears receipt");
 bool unavailable=false;try{audit.run();}catch(const std::invalid_argument& e){unavailable=std::string(e.what())=="REFINED_ARC_SPAN_UNAVAILABLE";}check(unavailable,"zero refined fails closed");
 no_run=false;try{audit.receipt();}catch(const std::runtime_error&){no_run=true;}check(no_run,"failed run clears receipt");
 CxRefinedArcAudit missing;bool guarded=false;try{missing.run();}catch(const std::invalid_argument& e){guarded=std::string(e.what())=="REFINED_ARC_EXPLICIT_SPANS_REQUIRED";}check(guarded,"explicit span selection");
 audit.grayparameter(20,"minimum_contrast");audit.parameter(-1,"maximum_support_endpoint_fraction_delta");
 guarded=false;try{audit.run();}catch(const std::invalid_argument& e){guarded=std::string(e.what())=="REFINED_ARC_INVALID_SUPPORT_TOLERANCE";}check(guarded,"invalid support tolerance");
 audit.parameter(.02,"maximum_support_endpoint_fraction_delta");audit.parameter(1,"common_support_enabled");
 guarded=false;try{audit.run();}catch(const std::invalid_argument& e){guarded=std::string(e.what())=="COMMON_ARC_EXPLICIT_ANCHOR_PAIR_REQUIRED";}check(guarded,"common mode needs explicit anchors");
 audit.select(0);audit.anchor(0,1);audit.select(1);audit.anchor(0,1);audit.correspondence("pair-fixture");audit.run();
 audit.grayparameter(20,"minimum_contrast");
 guarded=false;try{audit.run();}catch(const std::invalid_argument& e){guarded=std::string(e.what())=="COMMON_ARC_EXPLICIT_ANCHOR_PAIR_REQUIRED";}check(guarded,"source settings invalidate anchors");
 // Leave the headless output absent; parity verification cannot pass on a
 // stale output written by this native parser run.
 fs::remove(root/"script_saved.json");
 std::cout<<"REFINED_ARC_CXSCRIPT_PASS parameters / typed spans / stale and invalid guards\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
