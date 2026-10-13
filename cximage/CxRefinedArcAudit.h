#pragma once
#include "CxGrayBoundaryAudit.h"
#include "CxLandmarkRefinedArc.h"
// Script-only comparison entry. Owns two independent image audit executors;
// no untyped object pointer import, JSON-to-geometry import or GUI activation.
class CxRefinedArcAudit {
 CxGrayBoundaryAudit sides_[2];
 cxpartialimage::RefinedArcConfig settings_;
 int side_=0,span_[2]={-1,-1};
 bool common_enabled_=false,anchor_set_[2]={false,false};
 cxpartialimage::CommonRefinedArcConfig common_;
 bool matching_arc_enabled_=false,input_rejection_receipt_enabled_=false,input_rejected_=false;
 cxgeom::polar::MatchingArcLandmarks matching_[2];
 cxlandmarkquality::Config quality_;
 void clear_anchor(){anchor_set_[side_]=false;common_.correspondence_id.clear();common_.landmarks[side_]={};matching_[side_]={};}
 std::string receipt_;
 void invalidate(){receipt_.clear();input_rejected_=false;}
 static bool IsDataRejection(const std::string& s){
  return s=="BOUNDARY_ENDPOINT_AMBIGUOUS_SNAP"||s=="BOUNDARY_ENDPOINT_OUT_OF_RANGE"||
   s=="MASK_BOUNDARY_AMBIGUOUS_CONTOURS"||s=="MASK_BOUNDARY_NO_ANCHORED_CONTOUR";
 }
 void record_input_rejection(const nlohmann::json& failures){
  using J=nlohmann::json;cxpartialimage::RefinedArcComparison empty;empty.refined.config=settings_.pose;
  empty.refined.reference_receipt="{}";empty.refined.observation_receipt="{}";
  J j={{"schema","refined_arc_input_rejection.v1"},{"status",failures.at(0).at("reason")},{"input_rejected",true},
   {"production_eligible",false},{"whole_contour_approved",false},{"raw_modified",false},{"closed",false},{"gap_bridging",false},
   {"pose_evaluated",false},{"input_rejections",failures},{"pose_parameters",J::parse(empty.refined.receipt()).at("config")},
   {"landmark_quality_parameters",cxlandmarkquality::Values(quality_)},{"input_rejection_receipt_enabled",true},
   {"common_support",{{"status","NOT_EVALUATED"},{"minimum_anchor_fraction",common_.minimum_common_anchor_fraction}}},
   {"landmark_gate",{{"required",common_.reliable_landmarks_required},{"maximum_localization_error_px",common_.maximum_landmark_localization_error_px},
    {"maximum_propagated_pose_error_px",common_.maximum_landmark_pose_error_px},{"status","NOT_EVALUATED"}}},
   {"refined",{{"candidates",J::array()},{"accepted_indices",J::array()}}},{"full_raw_under_refined_pose",J::array()},
   {"fallback","INPUT_REVIEW_REQUIRED_NO_AUTOMATIC_SELECTION"}};
  for(int i:{0,1})j[i==0?"reference":"observation"]={{"input",sides_[i].inputreceipt()},{"requested_span",span_[i]}};
  receipt_=j.dump(2);input_rejected_=true;
 }
 static std::string text(const char* s){if(!s||!*s)throw std::invalid_argument("REFINED_ARC_MISSING_TEXT");return s;}
public:
 void select(int side){invalidate();if(side<0||side>1)throw std::invalid_argument("REFINED_ARC_SIDE");side_=side;}
 void image(const char* path){invalidate();clear_anchor();sides_[side_].image(path);}
 void caseid(const char* id){invalidate();clear_anchor();sides_[side_].caseid(id);}
 void revision(const char* id){invalidate();clear_anchor();sides_[side_].revision(id);}
 void grayparameter(double v,const char* key){invalidate();clear_anchor();sides_[side_].parameter(v,key);}
 void span(int index){invalidate();clear_anchor();if(index<0||index>127)throw std::invalid_argument("REFINED_ARC_SPAN_INDEX");span_[side_]=index;}
 void endpointparameter(double value,const char* key){invalidate();clear_anchor();sides_[side_].endpointparameter(value,key);}
 void endpointprovenance(const char* source){invalidate();clear_anchor();sides_[side_].endpointprovenance(source);}
 // Opaque GUI recipe token. It is never decoded into geometry or execution settings.
 void guiconfig(const char* value){
  invalidate();const auto token=text(value);if(token.size()>131072)throw std::invalid_argument("PAIR_GUI_TOKEN_BUDGET");
  for(char c:token)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))throw std::invalid_argument("PAIR_GUI_TOKEN_HEX");
 }
 void landmarksource(const char* value){invalidate();common_.landmarks[side_].source=text(value);}
 void landmarktrace(const char* value){invalidate();common_.landmarks[side_].trace=text(value);}
 void landmarksha(const char* value){invalidate();common_.landmarks[side_].image_sha256=text(value);}
 void landmarkerror(double value){invalidate();if(!std::isfinite(value)||value<0)throw std::invalid_argument("LANDMARK_LOCALIZATION_ERROR_UNKNOWN");common_.landmarks[side_].localization_error_px=value;}
 void anchor(double begin,double end){
  invalidate();clear_anchor();cxgeom::polar::RawArcAnchors a{begin,end};cxgeom::polar::ValidateRawArcAnchors(a);
  (side_==0?common_.reference:common_.observation)=a;anchor_set_[side_]=true;
 }
 void matchingparameter(double v,const char* name){
  invalidate();if(!std::isfinite(v))throw std::invalid_argument("MATCHING_ARC_NONFINITE");
  const auto key=text(name);auto& a=matching_[side_];
  if(key=="start_x")a.start.real(v);else if(key=="start_y")a.start.imag(v);
  else if(key=="end_x")a.end.real(v);else if(key=="end_y")a.end.imag(v);
  else if(key=="maximum_distance_px"){if(v<=0)throw std::invalid_argument("MATCHING_ARC_DISTANCE");a.maximum_distance_px=v;}
  else if(key=="tie_epsilon_px"){if(v<0)throw std::invalid_argument("MATCHING_ARC_TIE");a.tie_epsilon_px=v;}
  else throw std::invalid_argument("MATCHING_ARC_UNKNOWN_PARAMETER");
 }
 void matchingprovenance(const char* value){invalidate();matching_[side_].provenance=text(value);}
 void correspondence(const char* id){invalidate();common_.correspondence_id=text(id);}
 void parameter(double v,const char* name){
  invalidate();const auto key=text(name);auto& c=settings_.pose;
  if(!std::isfinite(v))throw std::invalid_argument("REFINED_ARC_NONFINITE_PARAMETER");
  if(key=="input_rejection_receipt_enabled"){
   if(v!=0&&v!=1)throw std::invalid_argument("REFINED_ARC_BOOLEAN");
   input_rejection_receipt_enabled_=v!=0;return;
  }
  if(cxlandmarkquality::Set(quality_,key,v))return;
  if(key=="samples"){if(v<3||v>128||std::floor(v)!=v)throw std::invalid_argument("ARC_SAMPLE_BUDGET");c.samples=size_t(v);}
  else if(key=="minimum_scale")c.minimum_scale=v;
  else if(key=="maximum_scale")c.maximum_scale=v;
  else if(key=="maximum_fit_rms_px")c.maximum_fit_rms_px=v;
  else if(key=="maximum_observed_error_px")c.maximum_observed_error_px=v;
  else if(key=="support_distance_px")c.support_distance_px=v;
  else if(key=="minimum_weighted_support")c.minimum_weighted_support=v;
  else if(key=="maximum_endpoint_error_px")c.maximum_endpoint_error_px=v;
  else if(key=="maximum_support_endpoint_fraction_delta")settings_.maximum_support_endpoint_fraction_delta=v;
  else if(key=="minimum_common_anchor_fraction")common_.minimum_common_anchor_fraction=v;
  else if(key=="endpoint_projection_enabled"){if(v!=0&&v!=1)throw std::invalid_argument("REFINED_ARC_BOOLEAN");common_.endpoint_projection_enabled=v!=0;}
  else if(key=="matching_landmark_arc_enabled"){if(v!=0&&v!=1)throw std::invalid_argument("REFINED_ARC_BOOLEAN");matching_arc_enabled_=v!=0;}
  else if(key=="maximum_projection_normal_residual_px")common_.maximum_projection_normal_residual_px=v;
  else if(key=="reliable_landmarks_required"){if(v!=0&&v!=1)throw std::invalid_argument("REFINED_ARC_BOOLEAN");common_.reliable_landmarks_required=v!=0;}
  else if(key=="maximum_landmark_localization_error_px")common_.maximum_landmark_localization_error_px=v;
  else if(key=="maximum_landmark_pose_error_px")common_.maximum_landmark_pose_error_px=v;
  else if(key=="common_support_enabled"){if(v!=0&&v!=1)throw std::invalid_argument("REFINED_ARC_BOOLEAN");common_enabled_=v!=0;}
  else if(key=="allow_reversed"){if(v!=0&&v!=1)throw std::invalid_argument("REFINED_ARC_BOOLEAN");c.allow_reversed=v!=0;}
  else throw std::invalid_argument("REFINED_ARC_UNKNOWN_PARAMETER");
 }
 void run(){
  invalidate();
  if(span_[0]<0||span_[1]<0)throw std::invalid_argument("REFINED_ARC_EXPLICIT_SPANS_REQUIRED");
  if(matching_arc_enabled_&&(!common_enabled_||common_.endpoint_projection_enabled))
   throw std::invalid_argument("MATCHING_ARC_MODE_CONFLICT");
  if(common_.reliable_landmarks_required&&!common_enabled_)throw std::invalid_argument("LANDMARK_COMMON_MODE_REQUIRED");
  if(common_.endpoint_projection_enabled&&!common_enabled_)
   throw std::invalid_argument("ENDPOINT_PROJECTION_COMMON_MODE_REQUIRED");
  if(common_enabled_&&(common_.correspondence_id.empty()||
     (!matching_arc_enabled_&&!common_.endpoint_projection_enabled&&(!anchor_set_[0]||!anchor_set_[1]))))
   throw std::invalid_argument("COMMON_ARC_EXPLICIT_ANCHOR_PAIR_REQUIRED");
  if((quality_.identity_required||quality_.uncertainty_required)&&(!matching_arc_enabled_||!common_.reliable_landmarks_required))
   throw std::invalid_argument("LANDMARK_QUALITY_REQUIRES_MATCHING_MODE_AND_SOURCE_BINDING");
  cxgeom::polar::arc_detail::ValidateArcConfig(settings_.pose);cxlandmarkquality::Validate(quality_);
  nlohmann::json failures=nlohmann::json::array();
  for(int i:{0,1})try{sides_[i].run();}
   catch(const std::invalid_argument& e){
    if(!input_rejection_receipt_enabled_||!IsDataRejection(e.what()))throw;
    nlohmann::json failure={{"side",i},{"stage","boundary_selection"},{"reason",e.what()}};
    if(auto* detail=dynamic_cast<const cxgeom::polar::BoundaryEndpointRejection*>(&e))failure["selection_diagnostics"]=nlohmann::json::parse(detail->receipt());
    if(auto* detail=dynamic_cast<const cxpartialimage::MaskBoundaryRejection*>(&e))failure["mask_diagnostics"]=nlohmann::json::parse(detail->receipt());
    failures.push_back(failure);
   }
  if(!failures.empty()){record_input_rejection(failures);return;}
  const auto& a=sides_[0].spans();const auto& b=sides_[1].spans();
  if(size_t(span_[0])>=a.size()||size_t(span_[1])>=b.size()){
   if(!input_rejection_receipt_enabled_)throw std::invalid_argument("REFINED_ARC_SPAN_UNAVAILABLE");
   for(int i:{0,1})if(size_t(span_[i])>=(i?b:a).size())failures.push_back({{"side",i},{"stage","refined_span_selection"},
    {"reason","REFINED_ARC_SPAN_UNAVAILABLE"},{"requested_span",span_[i]},{"available_span_count",(i?b:a).size()}});
   record_input_rejection(failures);return;
  }
  if(common_enabled_){
   common_.pose=settings_.pose;
   receipt_=matching_arc_enabled_?
    cxpartialimage::MatchLandmarkRefinedArc(a[size_t(span_[0])],b[size_t(span_[1])],common_,matching_[0],matching_[1],quality_).receipt():
    cxpartialimage::MatchCommonRefinedArc(a[size_t(span_[0])],b[size_t(span_[1])],common_).receipt();
  }else receipt_=cxpartialimage::MatchRefinedArc(a[size_t(span_[0])],b[size_t(span_[1])],settings_).receipt();
  if(input_rejection_receipt_enabled_){auto j=nlohmann::json::parse(receipt_);j["input_rejection_receipt_enabled"]=true;receipt_=j.dump(2);}
 }
 cv::Mat sideoverlay(int side)const{if(side<0||side>1)throw std::invalid_argument("REFINED_ARC_SIDE");receipt();return input_rejected_?sides_[side].inputoverlay():sides_[side].overlay();}
 const std::string& receipt()const{if(receipt_.empty())throw std::runtime_error("REFINED_ARC_RUN_REQUIRED");return receipt_;}
 void expectstatus(const char* status){if(nlohmann::json::parse(receipt()).at("status")!=text(status))throw std::runtime_error("REFINED_ARC_STATUS_ASSERTION");}
 void save(const char* path){const auto& r=receipt();std::ofstream f(text(path));f<<r;f.close();if(!f)throw std::runtime_error("REFINED_ARC_WRITE_FAILED");}
};
template<class Parser> void RegisterRefinedArcAudit(Parser& parser){
 CxRefinedArcAudit* p=nullptr;parser.DefineClass("RefinedArcAudit",p);
 parser.DefineClassFun("RefinedArcAudit",p,"select",&CxRefinedArcAudit::select);
 parser.DefineClassFun("RefinedArcAudit",p,"image",&CxRefinedArcAudit::image);
 parser.DefineClassFun("RefinedArcAudit",p,"caseid",&CxRefinedArcAudit::caseid);
 parser.DefineClassFun("RefinedArcAudit",p,"revision",&CxRefinedArcAudit::revision);
 parser.DefineClassFun("RefinedArcAudit",p,"grayparameter",&CxRefinedArcAudit::grayparameter);
 parser.DefineClassFun("RefinedArcAudit",p,"span",&CxRefinedArcAudit::span);
 parser.DefineClassFun("RefinedArcAudit",p,"parameter",&CxRefinedArcAudit::parameter);
 parser.DefineClassFun("RefinedArcAudit",p,"endpointparameter",&CxRefinedArcAudit::endpointparameter);
 parser.DefineClassFun("RefinedArcAudit",p,"endpointprovenance",&CxRefinedArcAudit::endpointprovenance);
 parser.DefineClassFun("RefinedArcAudit",p,"guiconfig",&CxRefinedArcAudit::guiconfig);
 parser.DefineClassFun("RefinedArcAudit",p,"landmarksource",&CxRefinedArcAudit::landmarksource);
 parser.DefineClassFun("RefinedArcAudit",p,"landmarktrace",&CxRefinedArcAudit::landmarktrace);
 parser.DefineClassFun("RefinedArcAudit",p,"landmarksha",&CxRefinedArcAudit::landmarksha);
 parser.DefineClassFun("RefinedArcAudit",p,"landmarkerror",&CxRefinedArcAudit::landmarkerror);
 parser.DefineClassFun("RefinedArcAudit",p,"anchor",&CxRefinedArcAudit::anchor);
 parser.DefineClassFun("RefinedArcAudit",p,"matchingparameter",&CxRefinedArcAudit::matchingparameter);
 parser.DefineClassFun("RefinedArcAudit",p,"matchingprovenance",&CxRefinedArcAudit::matchingprovenance);
 parser.DefineClassFun("RefinedArcAudit",p,"correspondence",&CxRefinedArcAudit::correspondence);
 parser.DefineClassFun("RefinedArcAudit",p,"run",&CxRefinedArcAudit::run);
 parser.DefineClassFun("RefinedArcAudit",p,"expectstatus",&CxRefinedArcAudit::expectstatus);
 parser.DefineClassFun("RefinedArcAudit",p,"save",&CxRefinedArcAudit::save);
}
