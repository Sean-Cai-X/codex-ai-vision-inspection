#pragma once
#include "../cxgeom/polar_discrete_harmonic/landmark_identity_uncertainty.h"
#include "../libtorchsegmentation/src/utils/json.hpp"
namespace cxlandmarkquality {
struct Config {
 cxgeom::polar::LandmarkIdentityConfig identity;
 bool identity_required=false,uncertainty_required=false;
 double error_floor_px=.05,maximum_angle_deg=1,maximum_scale_error=.05,maximum_translation_px=.5,maximum_center_px=.5;
};
struct Parameter {const char* key;double initial,minimum,maximum;bool integer;};
inline std::vector<Parameter> Parameters(){
 const Config c;return {
 {"identity_required",0,0,1,true},{"identity_radius_px",c.identity.radius_px,.1,1000,false},
 {"identity_step_px",c.identity.step_px,.001,100,false},
 {"identity_score_tolerance_px",c.identity.score_tolerance_px,0,100,false},
 {"identity_maximum_context_error_px",c.identity.maximum_context_error_px,0,100,false},
 {"identity_maximum_displacement_px",c.identity.maximum_displacement_px,.001,100,false},
 {"identity_samples",double(c.identity.samples),3,128,true},
 {"identity_maximum_hypotheses",double(c.identity.maximum_hypotheses),2,100000,true},
 {"uncertainty_required",0,0,1,true},{"uncertainty_error_floor_px",c.error_floor_px,0,100,false},
 {"uncertainty_maximum_angle_deg",c.maximum_angle_deg,.000001,180,false},
 {"uncertainty_maximum_scale_error",c.maximum_scale_error,.000001,100,false},
 {"uncertainty_maximum_translation_px",c.maximum_translation_px,.000001,10000,false},
 {"uncertainty_maximum_center_px",c.maximum_center_px,.000001,10000,false}};
}
inline bool Set(Config& c,const std::string& key,double v){
 bool known=false;for(const auto& p:Parameters())if(key==p.key){
  known=true;if(!std::isfinite(v)||v<p.minimum||v>p.maximum||(p.integer&&std::floor(v)!=v))
   throw std::invalid_argument("LANDMARK_QUALITY_PARAMETER:"+key);
 }
 if(!known)return false;
 if(key=="identity_required")c.identity_required=v!=0;
 else if(key=="identity_radius_px")c.identity.radius_px=v;
 else if(key=="identity_step_px")c.identity.step_px=v;
 else if(key=="identity_score_tolerance_px")c.identity.score_tolerance_px=v;
 else if(key=="identity_maximum_context_error_px")c.identity.maximum_context_error_px=v;
 else if(key=="identity_maximum_displacement_px")c.identity.maximum_displacement_px=v;
 else if(key=="identity_samples")c.identity.samples=int(v);
 else if(key=="identity_maximum_hypotheses")c.identity.maximum_hypotheses=int(v);
 else if(key=="uncertainty_required")c.uncertainty_required=v!=0;
 else if(key=="uncertainty_error_floor_px")c.error_floor_px=v;
 else if(key=="uncertainty_maximum_angle_deg")c.maximum_angle_deg=v;
 else if(key=="uncertainty_maximum_scale_error")c.maximum_scale_error=v;
 else if(key=="uncertainty_maximum_translation_px")c.maximum_translation_px=v;
 else if(key=="uncertainty_maximum_center_px")c.maximum_center_px=v;
 return true;
}
inline nlohmann::json Values(const Config& c){
 return {{"identity_required",int(c.identity_required)},{"identity_radius_px",c.identity.radius_px},
 {"identity_step_px",c.identity.step_px},{"identity_score_tolerance_px",c.identity.score_tolerance_px},
 {"identity_maximum_context_error_px",c.identity.maximum_context_error_px},
 {"identity_maximum_displacement_px",c.identity.maximum_displacement_px},{"identity_samples",c.identity.samples},
 {"identity_maximum_hypotheses",c.identity.maximum_hypotheses},{"uncertainty_required",int(c.uncertainty_required)},
 {"uncertainty_error_floor_px",c.error_floor_px},{"uncertainty_maximum_angle_deg",c.maximum_angle_deg},
 {"uncertainty_maximum_scale_error",c.maximum_scale_error},{"uncertainty_maximum_translation_px",c.maximum_translation_px},
 {"uncertainty_maximum_center_px",c.maximum_center_px}};
}
inline void Validate(const Config& c){
 Config checked;auto values=Values(c);for(const auto& p:Parameters())Set(checked,p.key,values.at(p.key));
}
}
