#pragma once
#include "CxGrayBoundaryAudit.h"
#include "CxCommonRefinedArc.h"
// Script-only comparison entry. Owns two independent image audit executors;
// no untyped object pointer import, JSON-to-geometry import or GUI activation.
class CxRefinedArcAudit {
 CxGrayBoundaryAudit sides_[2];
 cxpartialimage::RefinedArcConfig settings_;
 int side_=0,span_[2]={-1,-1};
 bool common_enabled_=false,anchor_set_[2]={false,false};
 cxpartialimage::CommonRefinedArcConfig common_;
 void clear_anchor(){anchor_set_[side_]=false;common_.correspondence_id.clear();}
 std::string receipt_;
 void invalidate(){receipt_.clear();}
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
 void anchor(double begin,double end){
  invalidate();clear_anchor();cxgeom::polar::RawArcAnchors a{begin,end};cxgeom::polar::ValidateRawArcAnchors(a);
  (side_==0?common_.reference:common_.observation)=a;anchor_set_[side_]=true;
 }
 void correspondence(const char* id){invalidate();common_.correspondence_id=text(id);}
 void parameter(double v,const char* name){
  invalidate();const auto key=text(name);auto& c=settings_.pose;
  if(!std::isfinite(v))throw std::invalid_argument("REFINED_ARC_NONFINITE_PARAMETER");
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
  else if(key=="maximum_projection_normal_residual_px")common_.maximum_projection_normal_residual_px=v;
  else if(key=="common_support_enabled"){if(v!=0&&v!=1)throw std::invalid_argument("REFINED_ARC_BOOLEAN");common_enabled_=v!=0;}
  else if(key=="allow_reversed"){if(v!=0&&v!=1)throw std::invalid_argument("REFINED_ARC_BOOLEAN");c.allow_reversed=v!=0;}
  else throw std::invalid_argument("REFINED_ARC_UNKNOWN_PARAMETER");
 }
 void run(){
  invalidate();
  if(span_[0]<0||span_[1]<0)throw std::invalid_argument("REFINED_ARC_EXPLICIT_SPANS_REQUIRED");
  if(common_.endpoint_projection_enabled&&!common_enabled_)
   throw std::invalid_argument("ENDPOINT_PROJECTION_COMMON_MODE_REQUIRED");
  if(common_enabled_&&(common_.correspondence_id.empty()||
     (!common_.endpoint_projection_enabled&&(!anchor_set_[0]||!anchor_set_[1]))))
   throw std::invalid_argument("COMMON_ARC_EXPLICIT_ANCHOR_PAIR_REQUIRED");
  sides_[0].run();sides_[1].run();
  const auto& a=sides_[0].spans();const auto& b=sides_[1].spans();
  if(size_t(span_[0])>=a.size()||size_t(span_[1])>=b.size())throw std::invalid_argument("REFINED_ARC_SPAN_UNAVAILABLE");
  if(common_enabled_){
   common_.pose=settings_.pose;
   receipt_=cxpartialimage::MatchCommonRefinedArc(a[size_t(span_[0])],b[size_t(span_[1])],common_).receipt();
  }else receipt_=cxpartialimage::MatchRefinedArc(a[size_t(span_[0])],b[size_t(span_[1])],settings_).receipt();
 }
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
 parser.DefineClassFun("RefinedArcAudit",p,"anchor",&CxRefinedArcAudit::anchor);
 parser.DefineClassFun("RefinedArcAudit",p,"correspondence",&CxRefinedArcAudit::correspondence);
 parser.DefineClassFun("RefinedArcAudit",p,"run",&CxRefinedArcAudit::run);
 parser.DefineClassFun("RefinedArcAudit",p,"expectstatus",&CxRefinedArcAudit::expectstatus);
 parser.DefineClassFun("RefinedArcAudit",p,"save",&CxRefinedArcAudit::save);
}
