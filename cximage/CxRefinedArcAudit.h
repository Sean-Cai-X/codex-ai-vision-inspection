#pragma once
#include "CxGrayBoundaryAudit.h"
#include "CxLandmarkRefinedArc.h"
#include "CxPoseAcceptanceParameters.h"
#include <chrono>
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
 nlohmann::json acceptance_=cxposeacceptance::Defaults();
 bool acceptance_explicit_=false;
 using Clock=std::chrono::steady_clock;
 Clock::time_point run_started_;
 std::optional<double> side_ms_[2];
 std::optional<double> execution_ms_,matching_ms_,receipt_ms_,parse_ms_,final_dump_ms_;
 nlohmann::json landmark_timing_;
 nlohmann::json execution_configuration()const{
  using J=nlohmann::json;
  cxpartialimage::RefinedArcComparison empty;empty.refined.config=settings_.pose;
  empty.refined.reference_receipt="{}";empty.refined.observation_receipt="{}";
  J p=J::parse(empty.refined.receipt()).at("config");
  p["maximum_support_endpoint_fraction_delta"]=settings_.maximum_support_endpoint_fraction_delta;
  p["common_support_enabled"]=common_enabled_;p["matching_landmark_arc_enabled"]=matching_arc_enabled_;
  p["input_rejection_receipt_enabled"]=input_rejection_receipt_enabled_;
  p["minimum_common_anchor_fraction"]=common_.minimum_common_anchor_fraction;
  p["endpoint_projection_enabled"]=common_.endpoint_projection_enabled;
  p["maximum_projection_normal_residual_px"]=common_.maximum_projection_normal_residual_px;
  p["reliable_landmarks_required"]=common_.reliable_landmarks_required;
  p["maximum_landmark_localization_error_px"]=common_.maximum_landmark_localization_error_px;
  p["maximum_landmark_pose_error_px"]=common_.maximum_landmark_pose_error_px;
  J sides=J::array();
  for(int i:{0,1}){
   auto s=sides_[i].configurationreceipt();const auto& a=i==0?common_.reference:common_.observation;
   s["requested_span"]=span_[i];s["raw_anchor_set"]=anchor_set_[i];
   s["raw_anchor_begin"]=a.begin;s["raw_anchor_end"]=a.end;
   const auto& m=matching_[i];s["matching"]={{"start_x",m.start.real()},{"start_y",m.start.imag()},
    {"end_x",m.end.real()},{"end_y",m.end.imag()},{"maximum_distance_px",m.maximum_distance_px},
    {"tie_epsilon_px",m.tie_epsilon_px},{"provenance",m.provenance}};
   const auto& l=common_.landmarks[i];s["landmark"]={{"source",l.source},{"trace",l.trace},
    {"image_sha256",l.image_sha256},{"localization_error_px",l.localization_error_px}};
   sides.push_back(std::move(s));
  }
  return {{"schema","refined_arc_execution_configuration.v1"},{"basis","runtime_member_state_not_gui_token"},
   {"parameters",p},{"quality",cxlandmarkquality::Values(quality_)},{"acceptance",acceptance_},
   {"correspondence_id",common_.correspondence_id},{"sides",sides},{"production_allowed",false}};
 }
 // Only our own nonempty object receipts enter here. Metadata keys are owned
 // exclusively by this wrapper; do not use this for arbitrary external JSON.
 // Preserve the large immutable evidence payload and serialize only new fields.
 void append_metadata(const nlohmann::json& metadata){
  if(receipt_.size()<3||receipt_.front()!='{'||receipt_.back()!='}'||!metadata.is_object()||metadata.empty())
   throw std::runtime_error("REFINED_ARC_METADATA_CONTRACT");
  const auto fields=metadata.dump(2);
  receipt_.pop_back();
  receipt_+=",";
  receipt_.append(fields,1,fields.size()-2);
  receipt_+="}";
 }
 void append_acceptance(std::optional<double> pose_ms=std::nullopt){
  if(!acceptance_explicit_)return;
  cxposeacceptance::Metrics metrics;
  metrics.latency_scope=acceptance_.at("pose_latency_scope").get<int>();
  metrics.elapsed_ms=metrics.latency_scope==0?
   std::optional<double>(std::chrono::duration<double,std::milli>(Clock::now()-run_started_).count()):pose_ms;
  // This audit has no independently bound truth or calibrated physical DOFs.
  // Fit residuals and conditional uncertainty are NOT pose errors.
  // No whole-payload parse is performed. Zero means eliminated work, whereas
  // null in timingreceipt() means acceptance was not executed.
  nlohmann::json j=nlohmann::json::object();
  parse_ms_=0.0;
  // Diagnostic wall-clock stages, not additive CPU samples or pure solver time.
  // Legacy recipes remain byte-compatible: emitted only with explicit acceptance.
  const double total_ms=std::chrono::duration<double,std::milli>(Clock::now()-run_started_).count();
  auto nullable=[](std::optional<double> v){return v?nlohmann::json(*v):nlohmann::json(nullptr);};
  j["timing_profile"]={{"schema","refined_arc_timing.v1"},{"clock","steady_clock"},
   {"reference_measurement_ms",nullable(side_ms_[0])},{"observation_measurement_ms",nullable(side_ms_[1])},
   {"pose_and_receipt_ms",nullable(pose_ms)},{"until_profile_capture_ms",total_ms},
   {"scope","tool_run; pose includes result serialization; excludes application startup, overlay and final acceptance serialization"}};
  j["pose_acceptance"]=cxposeacceptance::Evaluate(acceptance_,metrics);
  j["pose_acceptance"]["evidence_status"]="INDEPENDENT_POSE_TRUTH_NOT_BOUND";
  j["pose_acceptance"]["measured_duration_ms"]=metrics.elapsed_ms?nlohmann::json(*metrics.elapsed_ms):nlohmann::json(nullptr);
  j["pose_acceptance"]["duration_scope"]=metrics.latency_scope;
  const auto dump_started=Clock::now();
  append_metadata(j);
  final_dump_ms_=std::chrono::duration<double,std::milli>(Clock::now()-dump_started).count();
 }
 void clear_anchor(){anchor_set_[side_]=false;common_.correspondence_id.clear();common_.landmarks[side_]={};matching_[side_]={};}
 std::string receipt_;
 void invalidate(){receipt_.clear();input_rejected_=false;execution_ms_.reset();matching_ms_.reset();receipt_ms_.reset();parse_ms_.reset();final_dump_ms_.reset();landmark_timing_=nullptr;}
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
  receipt_=j.dump(2);input_rejected_=true;append_acceptance();
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
  if(cxposeacceptance::Set(acceptance_,key,v)){acceptance_explicit_=true;return;}
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
private:
 void run_impl(){
  invalidate();
  side_ms_[0].reset();side_ms_[1].reset();
  run_started_=Clock::now();acceptance_=cxposeacceptance::Normalize(acceptance_);
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
  for(int i:{0,1}){
   const auto side_started=Clock::now();
   try{sides_[i].run();}
   catch(const std::invalid_argument& e){
    if(!input_rejection_receipt_enabled_||!IsDataRejection(e.what()))throw;
    nlohmann::json failure={{"side",i},{"stage","boundary_selection"},{"reason",e.what()}};
    if(auto* detail=dynamic_cast<const cxgeom::polar::BoundaryEndpointRejection*>(&e))failure["selection_diagnostics"]=nlohmann::json::parse(detail->receipt());
    if(auto* detail=dynamic_cast<const cxpartialimage::MaskBoundaryRejection*>(&e))failure["mask_diagnostics"]=nlohmann::json::parse(detail->receipt());
    failures.push_back(failure);
   }
   side_ms_[i]=std::chrono::duration<double,std::milli>(Clock::now()-side_started).count();
  }
  if(!failures.empty()){record_input_rejection(failures);return;}
  const auto& a=sides_[0].spans();const auto& b=sides_[1].spans();
  if(size_t(span_[0])>=a.size()||size_t(span_[1])>=b.size()){
   if(!input_rejection_receipt_enabled_)throw std::invalid_argument("REFINED_ARC_SPAN_UNAVAILABLE");
   for(int i:{0,1})if(size_t(span_[i])>=(i?b:a).size())failures.push_back({{"side",i},{"stage","refined_span_selection"},
    {"reason","REFINED_ARC_SPAN_UNAVAILABLE"},{"requested_span",span_[i]},{"available_span_count",(i?b:a).size()}});
   record_input_rejection(failures);return;
  }
  const auto pose_started=Clock::now();
  // Matching currently includes internal evidence construction. Do not label it
  // pure numerical solver time until that internal work is separately measured.
  const auto serialize=[&](const auto& result){
   const auto started=Clock::now();
   matching_ms_=std::chrono::duration<double,std::milli>(started-pose_started).count();
   receipt_=result.receipt();
   receipt_ms_=std::chrono::duration<double,std::milli>(Clock::now()-started).count();
  };
  if(common_enabled_){
   common_.pose=settings_.pose;
   if(matching_arc_enabled_)serialize(cxpartialimage::MatchLandmarkRefinedArc(a[size_t(span_[0])],b[size_t(span_[1])],common_,matching_[0],matching_[1],quality_,&landmark_timing_));
   else serialize(cxpartialimage::MatchCommonRefinedArc(a[size_t(span_[0])],b[size_t(span_[1])],common_));
  }else serialize(cxpartialimage::MatchRefinedArc(a[size_t(span_[0])],b[size_t(span_[1])],settings_));
  const double pose_ms=std::chrono::duration<double,std::milli>(Clock::now()-pose_started).count();
  if(input_rejection_receipt_enabled_)append_metadata({{"input_rejection_receipt_enabled",true}});
  append_acceptance(pose_ms);
 }
public:
 void run(){
  const auto started=Clock::now();
  run_impl();
  // Explicit acceptance opts into the new execution evidence; legacy receipts stay unchanged.
  if(acceptance_explicit_)append_metadata({{"execution_configuration",execution_configuration()}});
  execution_ms_=std::chrono::duration<double,std::milli>(Clock::now()-started).count();
 }
 // Separate report avoids recursive serialization inside the measured interval.
 // Failed/invalidated runs cannot expose stale successful timing.
 std::string timingreceipt()const{
  if(!execution_ms_||receipt_.empty())throw std::runtime_error("REFINED_ARC_RUN_REQUIRED");
  auto nullable=[](std::optional<double> v){return v?nlohmann::json(*v):nlohmann::json(nullptr);};
  const double limit=acceptance_.at("pose_max_latency_ms").get<double>();
  const bool enabled=acceptance_explicit_&&acceptance_.at("pose_acceptance_enabled").get<double>()!=0&&acceptance_.at("pose_acceptance_configured").get<double>()!=0;
  return nlohmann::json{{"schema","refined_arc_execution_timing.v1"},{"clock","steady_clock"},
   {"scope","run_impl_including_final_receipt_and_temporary_destruction_excludes_overlay_file_io_app_startup_and_timing_report"},
   {"execution_ms",*execution_ms_},{"reference_measurement_ms",nullable(side_ms_[0])},{"observation_measurement_ms",nullable(side_ms_[1])},
   {"matching_and_internal_evidence_ms",nullable(matching_ms_)},{"result_receipt_ms",nullable(receipt_ms_)},
   {"acceptance_parse_ms",nullable(parse_ms_)},{"acceptance_final_dump_ms",nullable(final_dump_ms_)},
   {"landmark_internal",landmark_timing_},
   {"latency_limit_ms",limit},{"latency_status",enabled?(*execution_ms_<=limit?"PASS":"FAIL"):"NOT_EVALUATED"},
   {"input_rejected",input_rejected_},{"production_allowed",false}}.dump(2);
 }
 void savetiming(const char* path){const auto value=timingreceipt();std::ofstream f(text(path));f<<value;f.close();if(!f)throw std::runtime_error("REFINED_ARC_WRITE_FAILED");}
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
 parser.DefineClassFun("RefinedArcAudit",p,"savetiming",&CxRefinedArcAudit::savetiming);
}
