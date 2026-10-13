#include "../../cximage/CxRefinedArcUiParameters.h"
#include <iostream>
using J=nlohmann::json;
void need(bool b,const char* why){if(!b)throw std::runtime_error(why);}
template<class F>void rejects(F f,const char* why){bool b=false;try{f();}catch(const std::exception&){b=true;}need(b,why);}
int main()try{
 auto s=cxpairui::Default();auto script=cxpairui::Build(s);
 need(cxpairui::IsCase(script)&&cxpairui::Read(script)==s,"canonical reversible");
 need(!cxpairui::IsCase("// RefinedArcAudit fake;"),"comment");
 need(!cxpairui::IsCase("text(\"RefinedArcAudit fake;\");"),"quoted declaration");
 rejects([&]{cxpairui::Read(script+"a.run();");},"script drift cannot rewrite user edits");
 rejects([&]{auto v=s;v["parameters"]["maximum_scale"]=0;cxpairui::Build(v);},"range");
 rejects([&]{auto v=s;v["parameters"]["minimum_scale"]=8;cxpairui::Build(v);},"cross scale");
 rejects([&]{auto v=s;v["parameters"]["samples"]=3.5;cxpairui::Build(v);},"integer");
 rejects([&]{auto v=s;v["sides"][0]["gray"]["scan_step_px"]=.3;cxpairui::Build(v);},"sample count");
 auto edited=s;edited["sides"][1]["landmark"]["source"]="MANUAL_CONFIRMED";need(cxpairui::Build(edited)!=script,"side edits flow to script");
 cxgeom::polar::LandmarkEvidence l{"MANUAL_CONFIRMED","local-receipt",std::string(64,'a'),.1};
 need(cxgeom::polar::CheckLandmarkEvidence(l,std::string(64,'a'),.25)=="LANDMARK_DECLARATION_BOUND_TO_IMAGE","binding");
 need(cxgeom::polar::CheckLandmarkEvidence(l,std::string(64,'b'),.25)=="LANDMARK_IMAGE_SHA_MISMATCH","different image");
 l.localization_error_px=-1;need(cxgeom::polar::CheckLandmarkEvidence(l,std::string(64,'a'),.25)=="LANDMARK_LOCALIZATION_ERROR_UNKNOWN","unknown error");
 need(std::abs(cxgeom::polar::PropagatedLandmarkError(1.5,.1,.2)-.35)<1e-12,"declared error propagation");
 std::cout<<"PAIR_UI_GUARDS_PASS canonical/edit/bounds/gray-profile/source-hash/unknown-error/propagation\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
