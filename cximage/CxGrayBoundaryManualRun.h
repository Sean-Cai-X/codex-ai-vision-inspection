
#pragma once
#include "CxGrayBoundaryAudit.h"
#include "CxGrayBoundaryUiParameters.h"
#include "ManualStateTestConsole.h"
#include "ManualConsoleUtils.h"
#include "ParserDebugBridge.h"
#include "../imgui/imgui.h"
#include <chrono>
namespace cxgraymanual {
inline std::string ImageSha(const cv::Mat& image){
 if(image.empty()||image.depth()!=CV_8U)throw std::invalid_argument("GRAY_GUI_IMAGE_REQUIRED");
 cv::Mat gray;
 if(image.channels()==1)gray=image;
 else if(image.channels()==3)cv::cvtColor(image,gray,cv::COLOR_BGR2GRAY);
 else if(image.channels()==4)cv::cvtColor(image,gray,cv::COLOR_BGRA2GRAY);
 else throw std::invalid_argument("GRAY_GUI_IMAGE_CHANNELS");
 std::string bytes=std::to_string(gray.cols)+":"+std::to_string(gray.rows)+":CV_8UC1:";
 for(int y=0;y<gray.rows;++y)bytes.append(reinterpret_cast<const char*>(gray.ptr<unsigned char>(y)),size_t(gray.cols));
 return cxgeom::so2::Sha256Bytes(bytes);
}
inline bool Current(const ManualTestContext& c){
 const auto& g=c.gray_boundary;
 return g.valid&&g.executed_script==c.editor_text&&g.executed_case==c.active_case_id&&
  g.executed_image==c.image_file_path&&g.executed_sha==g.view_sha;
}
inline void Invalidate(ManualTestContext& c){
 auto& g=c.gray_boundary;g.valid=false;g.receipt.clear();g.raw.clear();g.rejected.clear();g.refined.clear();
 g.summary="STALE / NOT RUN - rerun the current image and script";
}
inline void ImageChanged(ManualTestContext& c,const cv::Mat& image){
 Invalidate(c);
 try{c.gray_boundary.view_sha=ImageSha(image);}catch(...){c.gray_boundary.view_sha.clear();}
}
inline void Write(const std::filesystem::path& path,const std::string& text){
 std::ofstream f(path,std::ios::binary);f<<text;f.close();if(!f)throw std::runtime_error("GRAY_GUI_WRITE_FAILED");
}
inline bool Fail(ManualTestContext& c,const std::string& reason){
 Invalidate(c);auto& g=c.gray_boundary;g.summary=reason;
 c.run_state="failed";c.debug_status="gray_boundary_failed";c.debug_reason=reason;
 if(!g.output_path.empty())try{
  Write(std::filesystem::path(g.output_path)/"status.json",nlohmann::json{
   {"schema","gray_boundary_gui_status.v1"},{"status","FAILED"},{"reason",reason},{"production_eligible",false}}.dump(2));
 }catch(...){}
 return false;
}
inline bool Prepare(ManualTestContext& c,const cv::Mat& image,std::string& reason){
 if(!cxgrayui::IsCase(c.editor_text))return true;
 Invalidate(c);auto& g=c.gray_boundary;g.output_path.clear();
 try{
  auto values=cxgrayui::Values(c.editor_text);const auto sha=ImageSha(image);
  if(g.pending&&(g.pending_case!=c.active_case_id||g.pending_image!=c.image_file_path||g.pending_sha!=sha))
   throw std::runtime_error("GRAY_GUI_QUEUED_IMAGE_OR_CASE_CHANGED");
  g.view_sha=sha;
  const auto root=ResolveCxVisionRunPath("cxscript_runs/gray_boundary_manual");
  std::filesystem::create_directories(root);
  const auto stamp=std::chrono::system_clock::now().time_since_epoch().count();
  std::filesystem::path dir;bool created=false;
  for(int i=0;i<100&&!created;++i){dir=root/("run_"+std::to_string(stamp)+"_"+std::to_string(i));created=std::filesystem::create_directory(dir);}
  if(!created)throw std::runtime_error("Cannot reserve gray boundary run directory");
  g.output_path=dir.string();g.executed_script=c.editor_text;g.executed_case=c.active_case_id;
  g.executed_image=c.image_file_path;g.executed_sha=sha;
  Write(dir/"run.cxsc",c.editor_text);
  Write(dir/"input_parameters.json",nlohmann::json{{"schema","gray_boundary_gui_input.v1"},
   {"case_id",c.active_case_id},{"gray_sha256",sha},{"parameters",values},{"production_eligible",false}}.dump(2));
  g.summary="RUNNING";return true;
 }catch(const std::exception& e){reason=e.what();return Fail(c,reason);}
}
inline bool Collect(ManualTestContext& c,ParserDebugBridge& bridge,const cv::Mat& image,bool ran){
 if(!cxgrayui::IsCase(c.editor_text))return ran;
 if(!ran)return Fail(c,"RUN FAILED: "+c.debug_reason);
 auto& g=c.gray_boundary;
 try{
  if(g.output_path.empty()||g.executed_script!=c.editor_text||g.executed_case!=c.active_case_id||
     g.executed_image!=c.image_file_path||g.executed_sha!=ImageSha(image))
   throw std::runtime_error("GRAY_GUI_EXECUTION_SNAPSHOT_MISMATCH");
  auto* tool=static_cast<CxGrayBoundaryAudit*>(bridge.QueryClassObject("GrayBoundaryAudit",cxgrayui::Object(c.editor_text)));
  if(!tool)throw std::runtime_error("GRAY_GUI_RUNTIME_OBJECT_MISSING");
  const auto text=tool->receipt();auto j=nlohmann::json::parse(text);
  if(j.at("schema")!="gray_boundary_audit.v1"||j.at("production_eligible")!=false||
     j.at("case_id")!=c.active_case_id||j.at("binding").at("gray_sha256")!=g.executed_sha)
   throw std::runtime_error("GRAY_GUI_IMAGE_OR_CASE_BINDING_MISMATCH");
  if(j.at("parameters")!=cxgrayui::Values(c.editor_text))throw std::runtime_error("GRAY_GUI_PARAMETER_MISMATCH");
  const auto dir=std::filesystem::path(g.output_path);
  Write(dir/"gray_boundary_receipt.json",text);
  if(!cv::imwrite((dir/"overlay.png").string(),tool->overlay()))throw std::runtime_error("GRAY_GUI_OVERLAY_WRITE_FAILED");
  g.raw.clear();g.rejected.clear();g.refined.clear();const auto& r=tool->result();
  for(const auto& p:r.points){
   g.raw.insert(g.raw.end(),{p.raw.real(),p.raw.imag()});
   if(!p.accepted)g.rejected.insert(g.rejected.end(),{p.raw.real(),p.raw.imag()});
  }
  for(const auto& run:cxpartialimage::AcceptedGrayRuns(r,2)){
   std::vector<double> xy;for(auto i:run)xy.insert(xy.end(),{r.points[i].refined.real(),r.points[i].refined.imag()});
   g.refined.push_back(std::move(xy));
  }
  g.receipt=text;g.valid=true;
  g.summary=j.at("status").get<std::string>()+" | raw="+j.at("raw_point_count").dump()+
   " accepted="+j.at("accepted_point_count").dump()+" rejected="+j.at("rejected_point_count").dump()+" | production=false";
  Write(dir/"status.json",nlohmann::json{{"schema","gray_boundary_gui_status.v1"},
   {"status",j.at("status")},{"image_binding_verified",true},{"production_eligible",false}}.dump(2));
  return true;
 }catch(const std::exception& e){return Fail(c,e.what());}
}
inline void Draw(const ManualTestContext& c,ImDrawList* draw,const ImVec2& origin,float zoom){
 if(!Current(c)||!draw||!std::isfinite(zoom)||zoom<=0)return;
 auto point=[&](double x,double y){return ImVec2(origin.x+float(x)*zoom,origin.y+float(y)*zoom);};
 auto line=[&](const std::vector<double>& xy,ImU32 color){
  for(size_t i=2;i+1<xy.size();i+=2)draw->AddLine(point(xy[i-2],xy[i-1]),point(xy[i],xy[i+1]),color,1.5f);
 };
 line(c.gray_boundary.raw,IM_COL32(0,255,255,255));
 for(const auto& run:c.gray_boundary.refined)line(run,IM_COL32(0,220,0,255));
 const auto& xy=c.gray_boundary.rejected;
 for(size_t i=0;i+1<xy.size();i+=2)draw->AddCircle(point(xy[i],xy[i+1]),3,IM_COL32(255,0,0,255),8,1.5f);
}
}
