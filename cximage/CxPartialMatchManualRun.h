#pragma once
#include "CxPartialMatchAudit.h"
#include "CxPartialMatchUiParameters.h"
#include "ManualStateTestConsole.h"
#include "ManualConsoleUtils.h"
#include "ParserDebugBridge.h"
#include "../libtorchsegmentation/src/utils/json.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
namespace cxpartialmanual {
inline bool Current(const ManualTestContext& c){
 return c.partial_receipt_valid&&c.partial_executed_case==c.active_case_id&&
        c.partial_executed_script==c.editor_text;
}
inline void Invalidate(ManualTestContext& c){
 c.partial_receipt.clear();c.partial_receipt_valid=false;
 c.partial_result_summary="STALE / NOT RUN - rerun the current script";
}
inline bool Prepare(ManualTestContext& c,std::string& reason){
 Invalidate(c);
 if(!cxpartialui::IsCase(c.editor_text))return true;
 c.partial_output_path.clear();
 try{
  const auto object=cxpartialui::Object(c.editor_text);
  const auto run=cxpartialui::RunPosition(c.editor_text,object);
  nlohmann::json values=nlohmann::json::object();
  for(const auto& p:cxpartialui::Parameters()){
   auto v=cxpartialui::Read(c.editor_text,object,p);
   if(!v.present||v.position>run)throw std::runtime_error("Expose all parameters before GUI Run");
   cxpartialui::Call(object,p,v.value);values[p.key]=v.value;
  }
  const auto root=ResolveCxVisionRunPath("cxscript_runs/partial_match_manual");
  std::filesystem::create_directories(root);
  const auto stamp=std::chrono::system_clock::now().time_since_epoch().count();
  std::filesystem::path dir;bool created=false;
  for(int i=0;i<100&&!created;++i){
   dir=root/("run_"+std::to_string(stamp)+"_"+std::to_string(i));
   created=std::filesystem::create_directory(dir);
  }
  if(!created)throw std::runtime_error("Cannot reserve partial-match run directory");
  std::ofstream script(dir/"run.cxsc");script<<c.editor_text;script.close();
  std::ofstream input(dir/"input_parameters.json");
  input<<nlohmann::json{{"schema","cxvision.partial_ui_input.v1"},{"case_id",c.active_case_id},
    {"production_eligible",false},{"image_extraction",false},{"parameters",values}}.dump(2);input.close();
  if(!script||!input)throw std::runtime_error("Cannot persist partial-match inputs");
  c.partial_executed_script=c.editor_text;c.partial_executed_case=c.active_case_id;
  c.partial_output_path=dir.string();c.partial_result_summary="RUNNING";
  return true;
 }catch(const std::exception& e){reason=e.what();c.partial_result_summary=reason;return false;}
}
inline void Collect(ManualTestContext& c,ParserDebugBridge& bridge,bool ran){
 if(!cxpartialui::IsCase(c.editor_text))return;
 c.partial_receipt_valid=false;c.partial_receipt.clear();
 if(!ran){c.partial_result_summary="RUN FAILED: "+c.debug_reason;return;}
 try{
  if(c.partial_output_path.empty()||c.partial_executed_script!=c.editor_text||
     c.partial_executed_case!=c.active_case_id)throw std::runtime_error("Execution snapshot mismatch");
  auto object=cxpartialui::Object(c.editor_text);
  auto* p=static_cast<CxPartialMatchAudit*>(bridge.QueryClassObject("PartialMatchAudit",object));
  if(!p)throw std::runtime_error("PartialMatchAudit runtime object missing");
  auto text=p->receipt();auto j=nlohmann::json::parse(text);
  if(j.at("schema")!="polar_partial_pipeline_receipt.v1"||j.at("production_eligible")!=false)
   throw std::runtime_error("Invalid partial audit receipt");
  const auto& raw=j.at("raw_diagnostic");
  for(const auto& param:cxpartialui::Parameters()){
   const auto value=cxpartialui::Read(c.editor_text,object,param).value;
   const auto& a=raw.at("assessment_config");const auto& s=raw.at("search_config");
   const auto& actual=a.contains(param.key)?a.at(param.key):s.at(param.key);
   double executed=actual.is_boolean()?(actual.get<bool>()?1.:0.):actual.get<double>();
   if(value!=executed)throw std::runtime_error("Requested/executed parameter mismatch");
  }
  std::ofstream out(std::filesystem::path(c.partial_output_path)/"partial_match_receipt.json");
  out<<text;out.close();if(!out)throw std::runtime_error("Cannot persist partial-match receipt");
  c.partial_receipt=text;c.partial_receipt_valid=true;
  c.partial_result_summary=j.at("status").get<std::string>()+" | candidates="+
    std::to_string(j.at("candidates").size())+" | complete="+
    (j.at("complete").get<bool>()?"true":"false")+" | production=false";
 }catch(const std::exception& e){
  c.partial_result_summary=std::string("RECEIPT FAILED: ")+e.what();
  c.run_state="failed";c.debug_status="partial_receipt_failed";c.debug_reason=c.partial_result_summary;
 }
}
}
