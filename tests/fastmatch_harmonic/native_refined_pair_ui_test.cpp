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
 rejects([&]{auto v=s;v["parameters"]["input_rejection_receipt_enabled"]=2;cxpairui::Build(v);},"capture boolean");
 rejects([&]{auto v=s;v["sides"][0]["gray"]["scan_step_px"]=.3;cxpairui::Build(v);},"sample count");
 auto edited=s;edited["sides"][1]["landmark"]["source"]="MANUAL_CONFIRMED";need(cxpairui::Build(edited)!=script,"side edits flow to script");
 auto legacy=s;for(auto& side:legacy["sides"]){side["gray"].erase("foreground_polarity");side["gray"].erase("recenter_iterations");side["gray"].erase("recenter_tolerance_px");}
 need(cxpairui::Read(cxpairui::Build(legacy))==legacy,"old gray fields remain absent");
 auto dark=s;dark["sides"][1]["gray"]["foreground_polarity"]=1;need(cxpairui::Read(cxpairui::Build(dark))==dark,"dark polarity executable roundtrip");
 rejects([&]{auto v=s;v["sides"][0]["gray"]["foreground_polarity"]=2;cxpairui::Build(v);},"invalid foreground polarity");
 legacy["parameters"].erase("matching_landmark_arc_enabled");
 need(cxpairui::Read(cxpairui::Build(legacy))==legacy,"legacy exact canonical script preserved");
 for(const auto& p:cxlandmarkquality::Parameters())legacy["parameters"].erase(p.key);
 legacy["parameters"].erase("input_rejection_receipt_enabled");
 need(cxpairui::Read(cxpairui::Build(legacy))==legacy,"pre-identity recipe exact preservation");
 rejects([&]{auto v=s;v["parameters"]["identity_required"]=1;cxpairui::Build(v);},"identity requires matching mode");
 rejects([&]{auto v=s;v["parameters"]["identity_fake"]=1;cxpairui::Build(v);},"unknown quality parameter");
 rejects([&]{auto v=s;v["parameters"]["identity_samples"]=3.5;cxpairui::Build(v);},"identity integer");
 rejects([&]{auto v=s;v["parameters"]["uncertainty_error_floor_px"]=-1;cxpairui::Build(v);},"negative floor");
 for(const auto& p:cxlandmarkquality::Parameters()){
  auto v=s;double value=p.maximum;v["parameters"][p.key]=value;
  if(std::string(p.key)=="identity_required"||std::string(p.key)=="uncertainty_required")v["parameters"]["matching_landmark_arc_enabled"]=1;
  auto changed=cxpairui::Build(v);need(cxpairui::Read(changed)==v,"quality parameter roundtrip");
  need(changed.find(std::string(",\"")+p.key+"\");")!=std::string::npos,"quality parameter executable call");
 }
 auto curve=s;curve["parameters"]["matching_landmark_arc_enabled"]=1;
 for(auto& side:curve["sides"])side["matching"]=cxpairui::MatchingDefaults();
 need(cxpairui::Read(cxpairui::Build(curve))==curve,"matching fields roundtrip");
 rejects([&]{auto v=curve;v["parameters"]["endpoint_projection_enabled"]=1;cxpairui::Build(v);},"curve/straight mode conflict");
 rejects([&]{auto v=curve;v["sides"][1]["matching"]["maximum_distance_px"]=0;cxpairui::Build(v);},"matching distance");
 auto moved=curve;moved["sides"][1]["matching"]["start_x"]=3;
 need(cxpairui::Build(moved)!=cxpairui::Build(curve),"matching landmark changes script");
 cxgeom::polar::LandmarkEvidence l{"MANUAL_CONFIRMED","local-receipt",std::string(64,'a'),.1};
 need(cxgeom::polar::CheckLandmarkEvidence(l,std::string(64,'a'),.25)=="LANDMARK_DECLARATION_BOUND_TO_IMAGE","binding");
 need(cxgeom::polar::CheckLandmarkEvidence(l,std::string(64,'b'),.25)=="LANDMARK_IMAGE_SHA_MISMATCH","different image");
 l.localization_error_px=-1;need(cxgeom::polar::CheckLandmarkEvidence(l,std::string(64,'a'),.25)=="LANDMARK_LOCALIZATION_ERROR_UNKNOWN","unknown error");
 need(std::abs(cxgeom::polar::PropagatedLandmarkError(1.5,.1,.2)-.35)<1e-12,"declared error propagation");
 std::cout<<"PAIR_UI_GUARDS_PASS canonical/edit/bounds/gray-profile/source-hash/unknown-error/propagation\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
