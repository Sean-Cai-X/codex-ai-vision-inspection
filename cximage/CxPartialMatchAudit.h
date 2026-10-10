#pragma once
#include "../cxgeom/polar_discrete_harmonic/partial_pipeline_receipt.h"
#include <fstream>
// Script adapter owns input copies. No image extraction or production activation.
class CxPartialMatchAudit {
 cxgeom::polar::PartialRequest request_;
 cxgeom::polar::PartialSearchConfig config_;
 cxgeom::polar::PartialPipelineResult result_;
 int side_=0;bool ran_=false;std::string pending_id_;
 void invalidate(){ran_=false;result_={};}
 static std::string text(const char* s){if(!s||!*s)throw std::invalid_argument("missing_script_id");return s;}
public:
 void requestid(const char* id){invalidate();request_.request_id=text(id);}
 void select(int side){invalidate();if(side!=0&&side!=1)throw std::invalid_argument("invalid_side");side_=side;pending_id_.clear();}
 void source(const char* id){invalidate();(side_?request_.observation_source:request_.reference_source)=text(id);}
 void featureid(const char* id){invalidate();pending_id_=text(id);}
 void clear(){invalidate();(side_?request_.observation:request_.reference).clear();pending_id_.clear();}
 void point_script(double y,double x){
  invalidate();if(pending_id_.empty())throw std::invalid_argument("featureid_required_before_point");
  auto& points=side_?request_.observation:request_.reference;
  if(points.size()>=128)throw std::invalid_argument("script_feature_limit");
  points.push_back({pending_id_,side_?request_.observation_source:request_.reference_source,{x,y}});
  pending_id_.clear();
 }
 void parameter(double value,const char* name){
  invalidate();auto key=text(name);if(!std::isfinite(value))throw std::invalid_argument("nonfinite_parameter");
  auto count=[&](){if(value<1||value>100000000||std::floor(value)!=value)throw std::invalid_argument("invalid_integer_parameter");return size_t(value);};
  auto& a=request_.config;auto& s=config_;
  if(key=="maximum_hypotheses")a.maximum_hypotheses=count();
  else if(key=="maximum_pair_checks")a.maximum_pair_checks=count();
  else if(key=="maximum_candidates")s.maximum_candidates=count();
  else if(key=="maximum_features")a.maximum_features=count();
  else if(key=="minimum_matches")a.minimum_matches=count();
  else if(key=="minimum_reference_coverage")a.minimum_reference_coverage=value;
  else if(key=="minimum_observation_coverage")a.minimum_observation_coverage=value;
  else if(key=="minimum_spatial_span")a.minimum_spatial_span=value;
  else if(key=="max_residual_px")a.max_residual_px=value;
  else if(key=="scale_min")s.scale_min=value;
  else if(key=="scale_max")s.scale_max=value;
  else if(key=="angle_min_deg")s.angle_min_deg=value;
  else if(key=="angle_max_deg")s.angle_max_deg=value;
  else if(key=="minimum_seed_length_px")s.minimum_seed_length_px=value;
  else if(key=="merge_angle_deg")s.merge_angle_deg=value;
  else if(key=="merge_scale")s.merge_scale=value;
  else if(key=="merge_translation_px")s.merge_translation_px=value;
  else if(key=="encoding_bins")a.encoding.bins=int(count());
  else if(key=="encoding_max_order")a.encoding.max_order=int(count());
  else if(key=="minimum_rms_radius")a.encoding.minimum_rms_radius=value;
  else if(key=="refine_before_capacity"){
   if(value!=0&&value!=1)throw std::invalid_argument("invalid_boolean_parameter");s.refine_before_capacity=value==1;
  }else throw std::invalid_argument("unknown_partial_parameter");
 }
 void run(){invalidate();result_=cxgeom::polar::SearchRefinedPartial(request_,config_);ran_=true;}
 void expectstatus(const char* status){if(!ran_||result_.status!=text(status))throw std::runtime_error("partial_status_assertion");}
 void expectcount(int n){if(!ran_||n<0||result_.candidates.size()!=size_t(n))throw std::runtime_error("partial_count_assertion");}
 void save(const char* path){
  if(!ran_)throw std::runtime_error("run_required_after_input_change");
  auto receipt=cxgeom::polar::PartialPipelineReceiptV1(result_);
  std::ofstream file(text(path),std::ios::binary);file<<receipt;file.close();
  if(!file)throw std::runtime_error("partial_receipt_write_failed");
 }
};
template<class Parser> void RegisterPartialMatchAudit(Parser& parser){
 CxPartialMatchAudit* instance=nullptr;
 parser.DefineClass("PartialMatchAudit",instance);
 parser.DefineClassFun("PartialMatchAudit",instance,"requestid",&CxPartialMatchAudit::requestid);
 parser.DefineClassFun("PartialMatchAudit",instance,"select",&CxPartialMatchAudit::select);
 parser.DefineClassFun("PartialMatchAudit",instance,"source",&CxPartialMatchAudit::source);
 parser.DefineClassFun("PartialMatchAudit",instance,"featureid",&CxPartialMatchAudit::featureid);
 parser.DefineClassFun("PartialMatchAudit",instance,"point",&CxPartialMatchAudit::point_script);
 parser.DefineClassFun("PartialMatchAudit",instance,"clear",&CxPartialMatchAudit::clear);
 parser.DefineClassFun("PartialMatchAudit",instance,"parameter",&CxPartialMatchAudit::parameter);
 parser.DefineClassFun("PartialMatchAudit",instance,"run",&CxPartialMatchAudit::run);
 parser.DefineClassFun("PartialMatchAudit",instance,"expectstatus",&CxPartialMatchAudit::expectstatus);
 parser.DefineClassFun("PartialMatchAudit",instance,"expectcount",&CxPartialMatchAudit::expectcount);
 parser.DefineClassFun("PartialMatchAudit",instance,"save",&CxPartialMatchAudit::save);
}
