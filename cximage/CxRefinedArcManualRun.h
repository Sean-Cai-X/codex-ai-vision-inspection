#pragma once
#include "CxRefinedArcUiParameters.h"
#include "ManualStateTestConsole.h"
#include "ManualConsoleUtils.h"
#include "ParserDebugBridge.h"
#include <chrono>
#include "../imgui/imgui.h"
namespace cxpairmanual {
inline std::string Stamp(const std::string& path){
 return std::to_string(std::filesystem::last_write_time(path).time_since_epoch().count())+":"+std::to_string(std::filesystem::file_size(path));
}
inline void Invalidate(ManualTestContext& c){auto& r=c.refined_arc;r.valid=false;r.receipt.clear();r.summary="STALE / NOT RUN";}
inline bool Current(const ManualTestContext& c){
 const auto& r=c.refined_arc;
 if(!r.valid||r.executed_script!=c.editor_text||r.executed_case!=c.active_case_id)return false;
 try{for(int i:{0,1})if(Stamp(r.paths[i])!=r.stamps[i])return false;}catch(...){return false;}
 return true;
}
inline void Write(const std::filesystem::path& p,const std::string& text){
 std::ofstream f(p,std::ios::binary);f<<text;f.close();if(!f)throw std::runtime_error("PAIR_GUI_WRITE_FAILED");
}
inline bool Fail(ManualTestContext& c,const std::string& message){
 Invalidate(c);c.refined_arc.summary=message;c.debug_reason=message;c.debug_status="refined_pair_failed";c.run_state="failed";return false;
}
inline bool Prepare(ManualTestContext& c,std::string& reason){
 if(!cxpairui::IsCase(c.editor_text))return true;
 Invalidate(c);auto& r=c.refined_arc;r.output_path.clear();
 try{
  auto s=cxpairui::Read(c.editor_text);
  for(int i:{0,1}){
   r.sha[i]=cxpairui::ImageSha(s["sides"][i]);r.paths[i]=cxpairui::ImagePath(s["sides"][i]).string();r.stamps[i]=Stamp(r.paths[i]);
   if(r.pending&&(r.pending_case!=c.active_case_id||r.pending_sha[i]!=r.sha[i]))throw std::runtime_error("PAIR_GUI_QUEUED_SOURCE_CHANGED");
  }
  const auto root=ResolveCxVisionRunPath("cxscript_runs/refined_arc_manual");std::filesystem::create_directories(root);
  auto time=std::chrono::system_clock::now().time_since_epoch().count();bool created=false;std::filesystem::path dir;
  for(int i=0;i<100&&!created;++i){dir=root/("run_"+std::to_string(time)+"_"+std::to_string(i));created=std::filesystem::create_directory(dir);}
  if(!created)throw std::runtime_error("PAIR_GUI_OUTPUT_RESERVATION");
  r.output_path=dir.string();r.executed_script=c.editor_text;r.executed_case=c.active_case_id;
  Write(dir/"run.cxsc",c.editor_text);
  Write(dir/"input_parameters.json",nlohmann::json{{"schema","refined_arc_gui_input.v1"},{"case_id",c.active_case_id},
   {"parameters",s},{"actual_gray_sha256",{r.sha[0],r.sha[1]}},{"production_eligible",false}}.dump(2));
  r.summary="RUNNING";return true;
 }catch(const std::exception& e){reason=e.what();return Fail(c,reason);}
}
inline bool Collect(ManualTestContext& c,ParserDebugBridge& bridge,bool ran){
 if(!cxpairui::IsCase(c.editor_text))return ran;
 if(!ran)return Fail(c,"RUN FAILED: "+c.debug_reason);
 auto& r=c.refined_arc;
 try{
  auto s=cxpairui::Read(c.editor_text);
  if(r.output_path.empty()||r.executed_script!=c.editor_text||r.executed_case!=c.active_case_id)throw std::runtime_error("PAIR_GUI_SNAPSHOT_MISMATCH");
  auto* tool=static_cast<CxRefinedArcAudit*>(bridge.QueryClassObject("RefinedArcAudit","a"));if(!tool)throw std::runtime_error("PAIR_GUI_RUNTIME_OBJECT_MISSING");
  const auto text=tool->receipt();auto j=nlohmann::json::parse(text);
  if(j["production_eligible"]!=false||j["pose_parameters"]!=cxpairui::PoseValues(s))throw std::runtime_error("PAIR_GUI_EXECUTED_CONFIG_MISMATCH");
  if(j.value("input_rejection_receipt_enabled",false)!=(s["parameters"].value("input_rejection_receipt_enabled",0.)!=0))
   throw std::runtime_error("PAIR_GUI_INPUT_REJECTION_POLICY_MISMATCH");
  if(s["parameters"].value("matching_landmark_arc_enabled",0.)!=0&&j.at("landmark_quality_parameters")!=cxpairui::QualityValues(s))
   throw std::runtime_error("PAIR_GUI_LANDMARK_QUALITY_CONFIG_MISMATCH");
  for(int i:{0,1}){
   const auto& side=j.at(i==0?"reference":"observation");
   const bool rejected=j.value("input_rejected",false);
   if(rejected&&(s["parameters"].value("input_rejection_receipt_enabled",0.)!=1||j.at("pose_evaluated")!=false||
      !j["refined"]["accepted_indices"].empty()))throw std::runtime_error("PAIR_GUI_INVALID_INPUT_REJECTION");
   const auto& binding=rejected?side.at("input").at("binding"):side.at("span").at("binding");
   if(binding.at("gray_sha256")!=r.sha[i]||cxpairui::ImageSha(s["sides"][i])!=r.sha[i]||
      binding.at("image_id")!=s["sides"][i]["case_id"])
    throw std::runtime_error("PAIR_GUI_SOURCE_BINDING_MISMATCH");
   if(!cv::imwrite((std::filesystem::path(r.output_path)/(i==0?"reference_overlay.png":"observation_overlay.png")).string(),tool->sideoverlay(i)))
    throw std::runtime_error("PAIR_GUI_OVERLAY_WRITE_FAILED");
  }
  if(j["common_support"]["minimum_anchor_fraction"]!=s["parameters"]["minimum_common_anchor_fraction"]||
     j["landmark_gate"]["required"]!=bool(s["parameters"]["reliable_landmarks_required"].get<int>())||
     j["landmark_gate"]["maximum_localization_error_px"]!=s["parameters"]["maximum_landmark_localization_error_px"]||
     j["landmark_gate"]["maximum_propagated_pose_error_px"]!=s["parameters"]["maximum_landmark_pose_error_px"])
   throw std::runtime_error("PAIR_GUI_EXECUTED_GATE_MISMATCH");
  Write(std::filesystem::path(r.output_path)/"refined_arc_receipt.json",text);
  r.receipt=text;r.valid=true;r.summary=j["status"].get<std::string>()+" | candidates="+j["refined"]["accepted_indices"].dump()+" | production=false";
  Write(std::filesystem::path(r.output_path)/"status.json",nlohmann::json{{"schema","refined_arc_gui_status.v1"},
   {"status",j["status"]},{"two_image_binding_verified",true},{"production_eligible",false}}.dump(2));return true;
 }catch(const std::exception& e){return Fail(c,e.what());}
}
inline void Draw(const ManualTestContext& c,ImDrawList* draw,const ImVec2& origin,float zoom){
 if(!Current(c)||!draw||zoom<=0)return;
 const auto j=nlohmann::json::parse(c.refined_arc.receipt);
 if(j.value("input_rejected",false))return; // no matched geometry exists; never draw a stale/partial pose
 int side=-1;
 for(int i:{0,1})if(c.gray_boundary.view_sha==c.refined_arc.sha[i])side=i;
 if(side<0)return;
 const auto& input=j.at(side==0?"reference":"observation");
 const auto& span=input.at("span");
 auto point=[&](const nlohmann::json& p){return ImVec2(origin.x+p[0].get<float>()*zoom,origin.y+p[1].get<float>()*zoom);};
 const auto& full=span.at("refinement").at("points");
 for(size_t i=1;i<full.size();++i)draw->AddLine(point(full[i-1]["raw"]),point(full[i]["raw"]),IM_COL32(0,255,255,255),1.5f);
 const auto& refined=span.at("ordered_points");
 for(size_t i=1;i<refined.size();++i)draw->AddLine(point(refined[i-1]["refined"]),point(refined[i]["refined"]),IM_COL32(0,220,0,255),1.5f);
 for(const auto& p:full)if(!p["accepted"].get<bool>())draw->AddCircle(point(p["raw"]),3,IM_COL32(255,0,0,255),8,1.5f);
 if(input.contains("matching_landmarks")){
  for(const char* key:{"start","end"}){
   const auto xy=point(input["matching_landmarks"][key]);
   draw->AddCircle(xy,5,IM_COL32(255,220,0,255),12,1.5f);
   draw->AddText(ImVec2(xy.x+6,xy.y-12),IM_COL32(255,220,0,255),key);
  }
 }
}

}
