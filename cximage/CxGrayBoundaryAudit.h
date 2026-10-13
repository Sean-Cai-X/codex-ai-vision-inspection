#pragma once
#include "CxBoundGrayFrame.h"
#include "CxFullGrayBoundary.h"
#include "CxGrayBoundaryParameters.h"
#include "../libtorchsegmentation/src/utils/json.hpp"
#include <opencv2/imgcodecs.hpp>
#include <filesystem>
#include <cstdlib>
#include <fstream>
class CxGrayBoundaryAudit {
 using J=nlohmann::json;
 std::string path_,case_id_="gray_boundary",revision_="v1",receipt_;
 cv::Rect anchor_{29,12,6,30};int threshold_=128,foreground_polarity_=0;
 cxgeom::polar::BoundaryNormalConfig normal_;
 cxpartialimage::GrayBoundaryConfig gray_;
 cxpartialimage::GrayBoundaryResult result_;
 std::vector<cxpartialimage::BoundRefinedSpan> spans_;
 cv::Mat image_;bool ran_=false;int full_boundary_enabled_=0,chunk_core_points_=100,maximum_chunks_=64;
 std::vector<std::vector<size_t>> batch_runs_;
 cxgeom::polar::BoundaryEndpointAnchors endpoints_;
 bool endpoint_set_[6]={false,false,false,false,false,false};
 void clear_endpoints(){for(auto& v:endpoint_set_)v=false;endpoints_.provenance.clear();}
 void invalidate(){ran_=false;receipt_.clear();result_={};spans_.clear();batch_runs_.clear();image_.release();}
 static std::string text(const char* s){if(!s||!*s)throw std::invalid_argument("GRAY_SCRIPT_MISSING_TEXT");return s;}
 J parameters_receipt()const{J j={{"threshold",threshold_},{"anchor_x",anchor_.x},{"anchor_y",anchor_.y},{"anchor_width",anchor_.width},{"anchor_height",anchor_.height},
   {"normal_half_window",normal_.half_window},{"normal_minimum_span_px",normal_.minimum_side_span_px},{"normal_maximum_turn_deg",normal_.maximum_turn_deg},
   {"scan_radius_px",gray_.radius_px},{"scan_step_px",gray_.step_px},{"minimum_contrast",gray_.minimum_contrast},
   {"maximum_shift_px",gray_.maximum_shift_px},{"monotonic_tolerance",gray_.monotonic_tolerance},{"noise_floor_dn",gray_.noise_floor_dn}};if(gray_.recenter_iterations){j["recenter_iterations"]=gray_.recenter_iterations;j["recenter_tolerance_px"]=gray_.recenter_tolerance_px;}if(foreground_polarity_)j["foreground_polarity"]=foreground_polarity_;if(full_boundary_enabled_||chunk_core_points_!=100||maximum_chunks_!=64){j["full_boundary_enabled"]=full_boundary_enabled_;j["chunk_core_points"]=chunk_core_points_;j["maximum_chunks"]=maximum_chunks_;}return j;}
public:
 CxGrayBoundaryAudit(){for(const auto& p:cxgrayparams::Parameters())parameter(p.initial,p.key);}
 void image(const char* path){invalidate();clear_endpoints();path_=text(path);}
 void caseid(const char* id){invalidate();clear_endpoints();case_id_=text(id);}
 void revision(const char* value){invalidate();clear_endpoints();revision_=text(value);}
 void parameter(double value,const char* name){
  invalidate();clear_endpoints();const auto key=text(name);cxgrayparams::Validate(cxgrayparams::Find(key),value);
  auto integer=[&](int lo,int hi){if(value<lo||value>hi||std::floor(value)!=value)throw std::invalid_argument("GRAY_SCRIPT_INTEGER_RANGE");return int(value);};
  if(key=="threshold")threshold_=integer(0,254);
  else if(key=="full_boundary_enabled")full_boundary_enabled_=integer(0,1);
  else if(key=="chunk_core_points")chunk_core_points_=integer(1,126);
  else if(key=="maximum_chunks")maximum_chunks_=integer(1,256);
  else if(key=="foreground_polarity")foreground_polarity_=integer(0,1);
  else if(key=="anchor_x")anchor_.x=integer(0,100000);
  else if(key=="anchor_y")anchor_.y=integer(0,100000);
  else if(key=="anchor_width")anchor_.width=integer(1,100000);
  else if(key=="anchor_height")anchor_.height=integer(1,100000);
  else if(key=="normal_half_window")normal_.half_window=size_t(integer(1,32));
  else if(key=="normal_minimum_span_px")normal_.minimum_side_span_px=value;
  else if(key=="normal_maximum_turn_deg")normal_.maximum_turn_deg=value;
  else if(key=="scan_radius_px")gray_.radius_px=value;
  else if(key=="scan_step_px")gray_.step_px=value;
  else if(key=="minimum_contrast")gray_.minimum_contrast=value;
  else if(key=="maximum_shift_px")gray_.maximum_shift_px=value;
  else if(key=="monotonic_tolerance")gray_.monotonic_tolerance=value;
  else if(key=="recenter_iterations")gray_.recenter_iterations=integer(0,16);
  else if(key=="recenter_tolerance_px")gray_.recenter_tolerance_px=value;
  else if(key=="noise_floor_dn")gray_.noise_floor_dn=value;
  else throw std::invalid_argument("GRAY_SCRIPT_UNKNOWN_PARAMETER");
 }
 void endpointpoint(double x,double y,const char* role){
  invalidate();const auto key=text(role);
  const int i=key=="start"?0:key=="end"?1:key=="via"?2:-1;
  if(i<0)throw std::invalid_argument("GRAY_SCRIPT_ENDPOINT_ROLE");
  endpoint_set_[2*i]=false;endpoint_set_[2*i+1]=false;
  if(!std::isfinite(x)||!std::isfinite(y))throw std::invalid_argument("GRAY_SCRIPT_NONFINITE_ENDPOINT");
  (i==0?endpoints_.start:i==1?endpoints_.end:endpoints_.via)={x,y};endpoint_set_[2*i]=true;endpoint_set_[2*i+1]=true;
 }
 void endpointparameter(double value,const char* name){
  invalidate();const auto key=text(name);
  if(!std::isfinite(value))throw std::invalid_argument("GRAY_SCRIPT_NONFINITE_ENDPOINT");
  if(key=="maximum_snap_px"){if(value<0)throw std::invalid_argument("BOUNDARY_ENDPOINT_SNAP_CONFIG");endpoints_.maximum_snap_px=value;return;}
  if(key=="tie_epsilon_px"){if(value<0)throw std::invalid_argument("BOUNDARY_ENDPOINT_SNAP_CONFIG");endpoints_.tie_epsilon_px=value;return;}
  const std::string keys[6]={"start_x","start_y","end_x","end_y","via_x","via_y"};
  int index=-1;for(int i=0;i<6;++i)if(key==keys[i])index=i;
  if(index<0)throw std::invalid_argument("GRAY_SCRIPT_ENDPOINT_PARAMETER");
  auto& p=index<2?endpoints_.start:index<4?endpoints_.end:endpoints_.via;
  if(index%2==0)p.real(value);else p.imag(value);
  endpoint_set_[index]=true;
 }
 void endpointprovenance(const char* source){invalidate();endpoints_.provenance=text(source);}
 void endpointconfig(double maximum_snap_px,double tie_epsilon_px,const char* provenance){
  invalidate();endpoints_.provenance.clear();
  if(!std::isfinite(maximum_snap_px)||maximum_snap_px<0||!std::isfinite(tie_epsilon_px)||
     tie_epsilon_px<0||tie_epsilon_px>maximum_snap_px)
   throw std::invalid_argument("BOUNDARY_ENDPOINT_SNAP_CONFIG");
  endpoints_.maximum_snap_px=maximum_snap_px;endpoints_.tie_epsilon_px=tie_epsilon_px;
  endpoints_.provenance=text(provenance);
 }
 void run(){
  invalidate();if(path_.empty())throw std::invalid_argument("GRAY_SCRIPT_IMAGE_REQUIRED");
  std::filesystem::path path(path_);
  if(path.is_relative()){
   const char* root=std::getenv("CXVISION_RUN_ROOT");
   if(!root||!*root)throw std::invalid_argument("GRAY_SCRIPT_RUN_ROOT_REQUIRED");
   // Relative image assets must remain beneath the explicitly configured run root.
   auto base=std::filesystem::weakly_canonical(root),resolved=std::filesystem::weakly_canonical(base/path);
   auto rel=resolved.lexically_relative(base);
   if(rel.empty()||rel.is_absolute()||*rel.begin()=="..")throw std::invalid_argument("GRAY_SCRIPT_IMAGE_OUTSIDE_RUN_ROOT");
   path=resolved;
  }
  const auto color=cv::imread(path.string(),cv::IMREAD_COLOR);
  if(color.empty())throw std::invalid_argument("GRAY_SCRIPT_IMAGE_UNREADABLE");
  cv::cvtColor(color,image_,cv::COLOR_BGR2GRAY);
  bool has_endpoints=false,complete=true;for(bool set:endpoint_set_){has_endpoints=has_endpoints||set;complete=complete&&set;}
  if(has_endpoints&&(!complete||endpoints_.provenance.empty()))
   throw std::invalid_argument("GRAY_SCRIPT_ENDPOINT_TRIPLE_REQUIRED");
  if(full_boundary_enabled_){
   if(has_endpoints)throw std::invalid_argument("FULL_GRAY_ENDPOINT_MODE_CONFLICT");
   auto full=cxpartialimage::RefineFullGrayBoundary(image_,case_id_,revision_,threshold_,foreground_polarity_,anchor_,normal_,gray_,chunk_core_points_,maximum_chunks_);
   result_=std::move(full.refinement);batch_runs_=std::move(full.runs);
   size_t accepted=0;for(const auto& p:result_.points)accepted+=p.accepted?1:0;
   receipt_=J({{"schema","gray_boundary_audit.v1"},{"case_id",case_id_},{"dataset_revision",revision_},
    {"production_eligible",false},{"status",accepted?"REFINED_POINTS_AVAILABLE":"NO_REFINED_POINTS"},
    {"parameters",parameters_receipt()},{"decode_policy","IMREAD_COLOR_then_BGR2GRAY"},
    {"raw_point_count",result_.points.size()},{"accepted_point_count",accepted},{"rejected_point_count",result_.points.size()-accepted},
    {"binding",{{"gray_sha256",result_.gray_sha256},{"image_id",case_id_},{"dataset_revision",revision_}}},
    {"refinement",J::parse(result_.receipt())},{"full_boundary",full.evidence},{"refined_spans",J::array()},
    {"pose_ready",false},{"legend","Raw cyan; Refined green; rejected red. Open runs never bridge rejection gaps."}}).dump(2);
   ran_=true;return;
  }
  auto frame=cxpartialimage::BoundGrayFrame::Capture(image_,case_id_,revision_,threshold_,anchor_,
   has_endpoints?&endpoints_:nullptr,foreground_polarity_);
  result_=frame.refine(normal_,gray_);
  spans_=frame.refined_spans(normal_,gray_);
  J serialized=J::array();for(const auto& span:spans_)serialized.push_back(J::parse(span.receipt()));
  size_t accepted=0;for(const auto& p:result_.points)accepted+=p.accepted?1:0;
  J params=parameters_receipt();
  receipt_=J({{"schema","gray_boundary_audit.v1"},{"case_id",case_id_},{"dataset_revision",revision_},
   {"production_eligible",false},{"status",accepted?"REFINED_POINTS_AVAILABLE":"NO_REFINED_POINTS"},{"parameters",params},
   {"decode_policy","IMREAD_COLOR_then_BGR2GRAY"},
   {"raw_point_count",result_.points.size()},{"accepted_point_count",accepted},{"rejected_point_count",result_.points.size()-accepted},
   {"binding",J::parse(frame.receipt())},{"refinement",J::parse(result_.receipt())},{"refined_spans",serialized},
   {"legend","Raw cyan; Refined green; rejected red. Refined segments never bridge rejected points."}}).dump(2);
  ran_=true;
 }
 // Actual decoded snapshot and execution progress, also available after a data rejection.
 J inputreceipt()const{
  if(image_.empty())throw std::runtime_error("GRAY_SCRIPT_NO_DECODED_SNAPSHOT");
  std::string bytes=std::to_string(image_.cols)+":"+std::to_string(image_.rows)+":CV_8UC1:";
  for(int y=0;y<image_.rows;++y)bytes.append(reinterpret_cast<const char*>(image_.ptr<unsigned char>(y)),size_t(image_.cols));
  J j={{"schema","gray_boundary_input_attempt.v1"},{"binding",{{"image_id",case_id_},{"dataset_revision",revision_},
   {"gray_sha256",cxgeom::so2::Sha256Bytes(bytes)},{"width",image_.cols},{"height",image_.rows},
   {"binding_method","actual_decoded_owned_gray_snapshot"}}},{"gray_audit_completed",ran_},{"production_eligible",false},{"executed_parameters",parameters_receipt()},
   {"endpoint_provenance",endpoints_.provenance},{"selection_anchors",{{"start",{endpoints_.start.real(),endpoints_.start.imag()}},
    {"end",{endpoints_.end.real(),endpoints_.end.imag()}},{"via",{endpoints_.via.real(),endpoints_.via.imag()}},
    {"maximum_snap_px",endpoints_.maximum_snap_px},{"tie_epsilon_px",endpoints_.tie_epsilon_px}}}};
  if(ran_)j["gray_audit"]=J::parse(receipt_);
  return j;
 }
 cv::Mat inputoverlay()const{
  if(ran_)return overlay();
  if(image_.empty())throw std::runtime_error("GRAY_SCRIPT_NO_DECODED_SNAPSHOT");
  cv::Mat out;cv::cvtColor(image_,out,cv::COLOR_GRAY2BGR);return out; // source only; no invented boundary
 }
 const cxpartialimage::GrayBoundaryResult& result()const{if(!ran_)throw std::runtime_error("GRAY_SCRIPT_RUN_REQUIRED");return result_;}
 const std::vector<cxpartialimage::BoundRefinedSpan>& spans()const{result();if(full_boundary_enabled_)throw std::runtime_error("FULL_GRAY_POSE_SPANS_UNSUPPORTED");return spans_;}
 const std::string& receipt()const{result();return receipt_;}
 std::vector<std::vector<size_t>> displayruns()const{result();return full_boundary_enabled_?batch_runs_:cxpartialimage::AcceptedGrayRuns(result_,2);}
 cv::Mat overlay()const{
  result();cv::Mat overlay;cv::cvtColor(image_,overlay,cv::COLOR_GRAY2BGR);
  auto pt=[](std::complex<double> p){return cv::Point(cvRound(p.real()*16),cvRound(p.imag()*16));};
  for(size_t i=1;i<result_.points.size();++i)cv::line(overlay,pt(result_.points[i-1].raw),pt(result_.points[i].raw),{255,255,0},1,cv::LINE_AA,4);
  for(const auto& run:displayruns())
   for(size_t i=1;i<run.size();++i)cv::line(overlay,pt(result_.points[run[i-1]].refined),pt(result_.points[run[i]].refined),{0,220,0},1,cv::LINE_AA,4);
  for(const auto& p:result_.points)if(!p.accepted)cv::circle(overlay,pt(p.raw),16,{0,0,255},1,cv::LINE_AA,4);
  return overlay;
 }
 void expectaccepted(int n){if(n<0||J::parse(receipt()).at("accepted_point_count")!=n)throw std::runtime_error("GRAY_SCRIPT_ACCEPTED_ASSERTION");}
 void save(const char* path){const auto& r=receipt();std::ofstream f(text(path));f<<r;f.close();if(!f)throw std::runtime_error("GRAY_SCRIPT_WRITE_FAILED");}
};
template<class Parser> void RegisterGrayBoundaryAudit(Parser& parser){
 CxGrayBoundaryAudit* p=nullptr;parser.DefineClass("GrayBoundaryAudit",p);
 parser.DefineClassFun("GrayBoundaryAudit",p,"image",&CxGrayBoundaryAudit::image);
 parser.DefineClassFun("GrayBoundaryAudit",p,"caseid",&CxGrayBoundaryAudit::caseid);
 parser.DefineClassFun("GrayBoundaryAudit",p,"revision",&CxGrayBoundaryAudit::revision);
 parser.DefineClassFun("GrayBoundaryAudit",p,"parameter",&CxGrayBoundaryAudit::parameter);
 parser.DefineClassFun("GrayBoundaryAudit",p,"endpointparameter",&CxGrayBoundaryAudit::endpointparameter);
 parser.DefineClassFun("GrayBoundaryAudit",p,"endpointprovenance",&CxGrayBoundaryAudit::endpointprovenance);
 parser.DefineClassFun("GrayBoundaryAudit",p,"run",&CxGrayBoundaryAudit::run);
 parser.DefineClassFun("GrayBoundaryAudit",p,"expectaccepted",&CxGrayBoundaryAudit::expectaccepted);
 parser.DefineClassFun("GrayBoundaryAudit",p,"save",&CxGrayBoundaryAudit::save);
}
