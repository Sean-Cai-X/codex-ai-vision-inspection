#pragma once
#include "CxBoundGrayFrame.h"
#include "../libtorchsegmentation/src/utils/json.hpp"
#include <opencv2/imgcodecs.hpp>
#include <filesystem>
#include <cstdlib>
#include <fstream>
class CxGrayBoundaryAudit {
 using J=nlohmann::json;
 std::string path_,case_id_="gray_boundary",revision_="v1",receipt_;
 cv::Rect anchor_{29,12,6,30};int threshold_=128;
 cxgeom::polar::BoundaryNormalConfig normal_;
 cxpartialimage::GrayBoundaryConfig gray_;
 cxpartialimage::GrayBoundaryResult result_;
 cv::Mat image_;bool ran_=false;
 void invalidate(){ran_=false;receipt_.clear();result_={};image_.release();}
 static std::string text(const char* s){if(!s||!*s)throw std::invalid_argument("GRAY_SCRIPT_MISSING_TEXT");return s;}
public:
 void image(const char* path){invalidate();path_=text(path);}
 void caseid(const char* id){invalidate();case_id_=text(id);}
 void revision(const char* value){invalidate();revision_=text(value);}
 void parameter(double value,const char* name){
  invalidate();const auto key=text(name);if(!std::isfinite(value))throw std::invalid_argument("GRAY_SCRIPT_NONFINITE_PARAMETER");
  auto integer=[&](int lo,int hi){if(value<lo||value>hi||std::floor(value)!=value)throw std::invalid_argument("GRAY_SCRIPT_INTEGER_RANGE");return int(value);};
  if(key=="threshold")threshold_=integer(0,254);
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
  else if(key=="noise_floor_dn")gray_.noise_floor_dn=value;
  else throw std::invalid_argument("GRAY_SCRIPT_UNKNOWN_PARAMETER");
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
  auto frame=cxpartialimage::BoundGrayFrame::Capture(image_,case_id_,revision_,threshold_,anchor_);
  result_=frame.refine(normal_,gray_);
  auto spans=frame.refined_spans(normal_,gray_);
  J serialized=J::array();for(const auto& span:spans)serialized.push_back(J::parse(span.receipt()));
  size_t accepted=0;for(const auto& p:result_.points)accepted+=p.accepted?1:0;
  J params={{"threshold",threshold_},{"anchor_x",anchor_.x},{"anchor_y",anchor_.y},{"anchor_width",anchor_.width},{"anchor_height",anchor_.height},
   {"normal_half_window",normal_.half_window},{"normal_minimum_span_px",normal_.minimum_side_span_px},{"normal_maximum_turn_deg",normal_.maximum_turn_deg},
   {"scan_radius_px",gray_.radius_px},{"scan_step_px",gray_.step_px},{"minimum_contrast",gray_.minimum_contrast},
   {"maximum_shift_px",gray_.maximum_shift_px},{"monotonic_tolerance",gray_.monotonic_tolerance},{"noise_floor_dn",gray_.noise_floor_dn}};
  receipt_=J({{"schema","gray_boundary_audit.v1"},{"case_id",case_id_},{"dataset_revision",revision_},
   {"production_eligible",false},{"status",accepted?"REFINED_POINTS_AVAILABLE":"NO_REFINED_POINTS"},{"parameters",params},
   {"decode_policy","IMREAD_COLOR_then_BGR2GRAY"},
   {"raw_point_count",result_.points.size()},{"accepted_point_count",accepted},{"rejected_point_count",result_.points.size()-accepted},
   {"binding",J::parse(frame.receipt())},{"refinement",J::parse(result_.receipt())},{"refined_spans",serialized},
   {"legend","Raw cyan; Refined green; rejected red. Refined segments never bridge rejected points."}}).dump(2);
  ran_=true;
 }
 const cxpartialimage::GrayBoundaryResult& result()const{if(!ran_)throw std::runtime_error("GRAY_SCRIPT_RUN_REQUIRED");return result_;}
 const std::string& receipt()const{result();return receipt_;}
 cv::Mat overlay()const{
  result();cv::Mat overlay;cv::cvtColor(image_,overlay,cv::COLOR_GRAY2BGR);
  auto pt=[](std::complex<double> p){return cv::Point(cvRound(p.real()*16),cvRound(p.imag()*16));};
  for(size_t i=1;i<result_.points.size();++i)cv::line(overlay,pt(result_.points[i-1].raw),pt(result_.points[i].raw),{255,255,0},1,cv::LINE_AA,4);
  for(const auto& run:cxpartialimage::AcceptedGrayRuns(result_,2))
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
 parser.DefineClassFun("GrayBoundaryAudit",p,"run",&CxGrayBoundaryAudit::run);
 parser.DefineClassFun("GrayBoundaryAudit",p,"expectaccepted",&CxGrayBoundaryAudit::expectaccepted);
 parser.DefineClassFun("GrayBoundaryAudit",p,"save",&CxGrayBoundaryAudit::save);
}
