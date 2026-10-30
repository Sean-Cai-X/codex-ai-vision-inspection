#pragma once
#include "../libtorchsegmentation/src/utils/json.hpp"
#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

// Shared numeric parameter owner for tool controls and immutable run snapshots.
// Diagnostic acceptance only: evidence authorization and production activation
// remain the caller's responsibility. Never infer truth from fit residuals.
namespace cxposeacceptance {
using J = nlohmann::json;
struct Parameter { const char* key; double initial, minimum, maximum; bool integer; };
inline std::vector<Parameter> Parameters() {
 return {{"pose_acceptance_enabled",0,0,1,true},{"pose_acceptance_configured",0,0,1,true},
 {"pose_require_tx",1,0,1,true},{"pose_require_ty",1,0,1,true},
 {"pose_require_angle",1,0,1,true},{"pose_require_scale",1,0,1,true},
 {"pose_translation_unit",0,0,1,true}, // 0 pixels; 1 calibrated millimetres
 {"pose_max_translation_error",.5,0,1e6,false},
 {"pose_max_angle_error_deg",1,0,180,false},
 {"pose_max_scale_error_percent",1,0,1e6,false},
 {"pose_max_latency_ms",100,0,1e9,false},
 {"pose_latency_scope",0,0,1,true}}; // 0 tool measurement + pose; 1 pose only
}
inline J Defaults() { J j=J::object();for(const auto& p:Parameters())j[p.key]=p.initial;return j; }
inline bool Set(J& config,const std::string& key,double value) {
 for(const auto& p:Parameters())if(key==p.key){
  if(!std::isfinite(value)||value<p.minimum||value>p.maximum||(p.integer&&std::floor(value)!=value))
   throw std::invalid_argument("POSE_ACCEPTANCE_PARAMETER:"+key);
  config[key]=value;return true;
 }
 return false;
}
inline J Normalize(const J& input) {
 if(!input.is_object())throw std::invalid_argument("POSE_ACCEPTANCE_CONFIG_OBJECT");
 J result=Defaults();for(auto it=input.begin();it!=input.end();++it){
  if(!it.value().is_number()||!Set(result,it.key(),it.value().get<double>()))
   throw std::invalid_argument("POSE_ACCEPTANCE_UNKNOWN_OR_NONNUMERIC_PARAMETER:"+it.key());
 }
 if(result["pose_acceptance_enabled"]==1&&result["pose_require_tx"]==0&&result["pose_require_ty"]==0&&
    result["pose_require_angle"]==0&&result["pose_require_scale"]==0)
  throw std::invalid_argument("POSE_ACCEPTANCE_REQUIRES_DOF");
 return result;
}
struct Metrics {
 std::optional<double> dx,dy,angle_error_deg,scale_error_percent,elapsed_ms;
 bool independent_truth_verified=false,calibration_verified=false;
 bool tx_observable=false,ty_observable=false,angle_observable=false,scale_observable=false;
 int translation_unit=0,latency_scope=0;
};
inline J Evaluate(const J& requested,const Metrics& m) {
 const auto c=Normalize(requested);
 J r={{"schema","cxvision.pose_acceptance.v1"},{"executed_parameters",c},
 {"status","DISABLED"},{"checks",J::object()},{"production_allowed",false}};
 if(c["pose_acceptance_enabled"]==0)return r;
 if(c["pose_acceptance_configured"]==0){r["status"]="NOT_CONFIGURED";return r;}
 bool failed=false,pending=false;
 auto check=[&](const char* key,bool required,bool observable,std::optional<double> error,double limit,const char* missing){
  J v={{"required",required},{"limit",limit},{"value",nullptr},{"status","NOT_REQUIRED"}};
  if(required){
   if(!observable){v["status"]="UNOBSERVABLE";pending=true;}
   else if(!error){v["status"]="NOT_EVALUATED";v["reason"]=missing;pending=true;}
   else if(!std::isfinite(*error)||*error<0){v["status"]="INVALID_METRIC";pending=true;}
   else {v["value"]=*error;v["status"]=*error<=limit?"PASS":"FAIL";failed|=*error>limit;}
  }
  r["checks"][key]=v;
 };
 const bool tx=c["pose_require_tx"]==1,ty=c["pose_require_ty"]==1;
 const bool unit_ok=m.translation_unit==c["pose_translation_unit"].get<int>()&&(m.translation_unit==0||m.calibration_verified);
 std::optional<double> translation;
 if(m.independent_truth_verified&&unit_ok&&(!tx||m.dx)&&(!ty||m.dy))
  translation=std::hypot(tx?*m.dx:0.,ty?*m.dy:0.);
 check("translation",tx||ty,(!tx||m.tx_observable)&&(!ty||m.ty_observable),translation,c["pose_max_translation_error"],
       "Independent truth, matching units and calibration (for mm) required");
 std::optional<double> angle;
 if(m.independent_truth_verified&&m.angle_error_deg)angle=std::abs(std::remainder(*m.angle_error_deg,360.));
 check("angle_deg",c["pose_require_angle"]==1,m.angle_observable,angle,c["pose_max_angle_error_deg"],"Independent truth required; no symmetry folding");
 check("scale_percent",c["pose_require_scale"]==1,m.scale_observable,
       m.independent_truth_verified?m.scale_error_percent:std::nullopt,c["pose_max_scale_error_percent"],"Independent truth and relative percent error required");
 check("latency_ms",true,true,m.latency_scope==c["pose_latency_scope"].get<int>()?m.elapsed_ms:std::nullopt,
       c["pose_max_latency_ms"],"Measured duration with matching scope required; not an execution timeout");
 r["status"]=failed?"FAIL":pending?"NOT_EVALUATED":"PASS_TECHNICAL_METRICS_ONLY";
 return r;
}
}
