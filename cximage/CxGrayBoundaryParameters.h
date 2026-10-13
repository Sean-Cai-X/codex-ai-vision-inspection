
#pragma once
#include "CxGrayBoundaryRefinement.h"
#include "../cxgeom/polar_discrete_harmonic/boundary_normal_estimation.h"
#include <vector>
#include <cmath>
namespace cxgrayparams {
struct Parameter { const char* key; double initial,minimum,maximum; bool integer; const char* group; const char* description; };
inline std::vector<Parameter> Parameters(){
 const cxpartialimage::GrayBoundaryConfig g;const cxgeom::polar::BoundaryNormalConfig n;
 return {
 {"threshold",128,0,254,true,"Image / anchor","Threshold in DN; foreground_polarity selects bright > or dark <=."},
 {"full_boundary_enabled",0,0,1,true,"Image / anchor","1: single closed observed component, halo batching; not pose-ready. No holes, border touch or endpoint selection."},
 {"chunk_core_points",100,1,126,true,"Observed normals","Core points per chunk; core + 2*normal_half_window must not exceed 128."},
 {"maximum_chunks",64,1,256,true,"Observed normals","Full boundary work budget. Exceeding it rejects the run."},
 {"foreground_polarity",0,0,1,true,"Image / anchor","0: bright foreground > threshold (legacy); 1: dark foreground <= threshold. Original gray image is not inverted."},
 {"anchor_x",29,0,100000,true,"Image / anchor","Anchor origin X in source pixels."},
 {"anchor_y",12,0,100000,true,"Image / anchor","Anchor origin Y in source pixels."},
 {"anchor_width",6,1,100000,true,"Image / anchor","Anchor selects the observed open span, not a result box."},
 {"anchor_height",30,1,100000,true,"Image / anchor","Anchor height in source pixels."},
 {"normal_half_window",double(n.half_window),1,32,true,"Observed normals","Two-sided support count; unsupported endpoints are rejected."},
 {"normal_minimum_span_px",n.minimum_side_span_px,1e-9,1e12,false,"Observed normals","Minimum chord support on each side, pixels."},
 {"normal_maximum_turn_deg",n.maximum_turn_deg,1e-9,179.999999,false,"Observed normals","Reject corners above this turn, degrees."},
 {"scan_radius_px",g.radius_px,1e-9,16,false,"Gray profile","Profile half length, pixels."},
 {"scan_step_px",g.step_px,1e-9,1,false,"Gray profile","2*radius/step must be an integer between 8 and 256."},
 {"minimum_contrast",g.minimum_contrast,1e-9,255,false,"Gray profile","Minimum endpoint intensity contrast in DN."},
 {"maximum_shift_px",g.maximum_shift_px,1e-9,16,false,"Gray profile","Maximum derived offset; must not exceed scan radius."},
 {"monotonic_tolerance",g.monotonic_tolerance,0,255,false,"Gray profile","Allowed local intensity reversal in DN."},
 {"recenter_iterations",0,0,16,true,"Gray profile","Optional symmetric window recentering; zero preserves legacy midpoint."},
 {"recenter_tolerance_px",.001,1e-9,.25,false,"Gray profile","Convergence of successive offsets in pixels, not a quality threshold."},
 {"noise_floor_dn",g.noise_floor_dn,1e-9,255,false,"Gray profile","Sensitivity estimate only; not calibrated uncertainty."}};
}
inline bool OptionalLegacy(const std::string& key){return key.find("recenter_")==0||key=="foreground_polarity"||key=="full_boundary_enabled"||key=="chunk_core_points"||key=="maximum_chunks";}
inline void Validate(const Parameter& p,double value){
 if(!std::isfinite(value)||value<p.minimum||value>p.maximum||(p.integer&&std::floor(value)!=value))
  throw std::invalid_argument(std::string("GRAY_PARAMETER_RANGE:")+p.key);
}
inline Parameter Find(const std::string& key){
 for(const auto& p:Parameters())if(key==p.key)return p;
 throw std::invalid_argument("GRAY_SCRIPT_UNKNOWN_PARAMETER");
}
}
