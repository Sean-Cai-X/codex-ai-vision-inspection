
#include "../../cximage/CxGrayBoundaryAudit.h"
#include "../../cximage/CxGrayBoundaryUiParameters.h"
#include <iostream>
#include <limits>
void need(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
template<class F>void rejects(F f,const char* why){bool bad=false;try{f();}catch(const std::exception&){bad=true;}need(bad,why);}
int main(int argc,char** argv)try{
 need(argc==2,"source image required");
 std::string script="GrayBoundaryAudit g;\ng.run();\n";
 cxgrayui::Expose(script);const auto exposed=script;cxgrayui::Expose(script);
 need(script==exposed,"expose not idempotent");need(cxgrayui::Values(script).size()==20,"twenty parameters");
 CxGrayBoundaryAudit tool;tool.image(argv[1]);tool.run();auto receipt=nlohmann::json::parse(tool.receipt());
 tool.parameter(0,"foreground_polarity");tool.run();need(nlohmann::json::parse(tool.receipt())==receipt,"explicit bright must preserve legacy receipt");
 for(const auto& p:cxgrayparams::Parameters()){
  need(receipt.at("parameters").value(p.key,p.initial)==p.initial,"UI/runtime default drift");
  rejects([&]{tool.parameter(p.maximum+1,p.key);},"runtime upper range");
  rejects([&]{auto s=script;cxgrayui::Set(s,p,p.maximum+1);},"UI upper range");
  rejects([&]{tool.parameter(std::numeric_limits<double>::quiet_NaN(),p.key);},"runtime NaN");
  if(p.integer){rejects([&]{tool.parameter(p.initial+.5,p.key);},"runtime integer");}
 }
 auto legacy=script;for(const char* key:{"recenter_iterations","recenter_tolerance_px","foreground_polarity","full_boundary_enabled","chunk_core_points","maximum_chunks"}){auto p=cxgrayparams::Find(key);auto value=cxgrayui::Read(legacy,"g",p);legacy.erase(value.position,value.length);}
 need(cxgrayui::Values(legacy).size()==14,"legacy gray settings preserved");
 need(!cxgrayui::IsCase("// GrayBoundaryAudit fake;\n"),"comment classified as case");
 need(!cxgrayui::IsCase("message(\"GrayBoundaryAudit fake;\");"),"string classified as case");
 const auto contrast=cxgrayparams::Find("minimum_contrast");
 auto changed=script;cxgrayui::Set(changed,contrast,250);
 need(cxgrayui::Values(changed).at("minimum_contrast")==250,"parameter edit missing");
 need(cxgrayui::Values(script).at("minimum_contrast")==20,"edit touched original");
 rejects([&]{auto s=script+"g.parameter(10,\"minimum_contrast\");";cxgrayui::Values(s);},"duplicate assignment");
 rejects([&]{auto s=std::string("GrayBoundaryAudit g;g.run();g.parameter(10,\"minimum_contrast\");");cxgrayui::Expose(s);},"after run");
 rejects([&]{auto s=std::string("GrayBoundaryAudit g;g.parameter(1+2,\"minimum_contrast\");g.run();");cxgrayui::Expose(s);},"expression overwritten");
 rejects([&]{auto s=script+"GrayBoundaryAudit second;";cxgrayui::Expose(s);},"multiple instances");
 rejects([&]{auto s=script+"g.run();";cxgrayui::Expose(s);},"multiple runs");
 rejects([&]{auto s=script;cxgrayui::Set(s,cxgrayparams::Find("scan_radius_px"),1);cxgrayui::Values(s);},"shift exceeds radius");
 rejects([&]{auto s=script;cxgrayui::Set(s,cxgrayparams::Find("scan_step_px"),.3);cxgrayui::Values(s);},"fractional sample count");
 rejects([&]{auto s=script;cxgrayui::Set(s,cxgrayparams::Find("scan_step_px"),.0001);cxgrayui::Values(s);},"sample budget");
 std::string ignored="message(\"g.run();\");\n"+script;
 need(cxgrayui::Values(ignored).size()==20,"quoted run interfered");
 std::cout<<"GRAY_UI_PARAMETERS_PASS defaults / literal edits / range / integer / NaN / stale-safe / cross-constraints\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
