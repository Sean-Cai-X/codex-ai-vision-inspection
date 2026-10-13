#pragma once
#include "CxRefinedArcAudit.h"
#include "CxGrayBoundaryUiParameters.h"
namespace cxpairui {
using J=nlohmann::json;
struct Parameter{const char* key;double initial,minimum,maximum;bool integer;};
inline std::vector<Parameter> Parameters(){
 const cxpartialimage::CommonRefinedArcConfig c;const auto& p=c.pose;
 return {{"samples",double(p.samples),3,128,true},{"minimum_scale",p.minimum_scale,1e-9,1e6,false},
 {"maximum_scale",p.maximum_scale,1e-9,1e6,false},{"maximum_fit_rms_px",p.maximum_fit_rms_px,1e-9,1e6,false},
 {"maximum_observed_error_px",p.maximum_observed_error_px,1e-9,1e6,false},{"support_distance_px",p.support_distance_px,1e-9,1e6,false},
 {"minimum_weighted_support",p.minimum_weighted_support,1e-9,1,false},{"maximum_endpoint_error_px",p.maximum_endpoint_error_px,1e-9,1e6,false},
 {"allow_reversed",0,0,0,true},{"common_support_enabled",1,1,1,true},{"endpoint_projection_enabled",0,0,1,true},
 {"minimum_common_anchor_fraction",c.minimum_common_anchor_fraction,1e-9,1,false},
 {"maximum_projection_normal_residual_px",c.maximum_projection_normal_residual_px,1e-9,1e6,false},
 {"reliable_landmarks_required",1,0,1,true},{"maximum_landmark_localization_error_px",c.maximum_landmark_localization_error_px,0,1e6,false},
 {"maximum_landmark_pose_error_px",c.maximum_landmark_pose_error_px,1e-9,1e6,false}};
}
inline J Default(){
 J p=J::object(),gray=J::object();for(const auto& v:Parameters())p[v.key]=v.initial;
 for(const auto& v:cxgrayparams::Parameters())gray[v.key]=v.initial;
 J sides=J::array();
 for(const char* name:{"reference","observation"})sides.push_back({{"image",""},{"case_id",name},{"revision","v1"},{"span",0},{"gray",gray},
  {"raw_anchor_begin",0.},{"raw_anchor_end",1.},
  {"endpoints",{{"start_x",0.},{"start_y",0.},{"end_x",1.},{"end_y",0.},{"via_x",.5},{"via_y",0.},{"maximum_snap_px",2.},{"tie_epsilon_px",1e-8}}},
  {"endpoint_provenance",""},{"landmark",{{"source","UNVERIFIED"},{"trace",""},{"image_sha256",""},{"localization_error_px",-1.}}}});
 return J{{"schema","refined_arc_gui_config.v1"},{"parameters",p},{"sides",sides},{"correspondence_id",""}};
}
inline bool IsCase(const std::string& script){
 try{return std::regex_search(cxgrayui::Structure(script),std::regex(R"(\bRefinedArcAudit\s+[A-Za-z_])"));}catch(...){return false;}
}
inline void Validate(const J& s){
 if(s.size()!=4||s.at("schema")!="refined_arc_gui_config.v1"||s.at("sides").size()!=2)throw std::invalid_argument("PAIR_UI_SCHEMA");
 const auto& p=s.at("parameters");if(p.size()!=Parameters().size())throw std::invalid_argument("PAIR_UI_UNKNOWN_PARAMETER");
 for(const auto& v:Parameters()){
  double x=p.at(v.key).get<double>();if(!std::isfinite(x)||x<v.minimum||x>v.maximum||(v.integer&&std::floor(x)!=x))
   throw std::invalid_argument(std::string("PAIR_UI_PARAMETER:")+v.key);
 }
 if(p.at("minimum_scale").get<double>()>p.at("maximum_scale").get<double>())throw std::invalid_argument("PAIR_UI_SCALE_RANGE");
 for(const auto& side:s.at("sides")){
  for(const char* key:{"image","case_id","revision","endpoint_provenance"})
   if(side.at(key).get<std::string>().size()>4095)throw std::invalid_argument("PAIR_UI_TEXT_LENGTH");
  if(side.at("span").get<double>()<0||side.at("span").get<double>()>127||std::floor(side.at("span").get<double>())!=side.at("span").get<double>())throw std::invalid_argument("PAIR_UI_SPAN");
  cxgeom::polar::ValidateRawArcAnchors({side.at("raw_anchor_begin"),side.at("raw_anchor_end")});
  for(const auto& v:cxgrayparams::Parameters())cxgrayparams::Validate(v,side.at("gray").at(v.key));
  const auto& g=side.at("gray");double count=2*g.at("scan_radius_px").get<double>()/g.at("scan_step_px").get<double>();
  if(count<8||count>256||std::abs(count-std::round(count))>1e-8||g.at("maximum_shift_px").get<double>()>g.at("scan_radius_px").get<double>())
   throw std::invalid_argument("PAIR_UI_GRAY_PROFILE");
  for(auto it=side.at("endpoints").begin();it!=side.at("endpoints").end();++it)if(!std::isfinite(it.value().get<double>()))throw std::invalid_argument("PAIR_UI_ENDPOINT");
  double snap=side.at("endpoints").at("maximum_snap_px"),tie=side.at("endpoints").at("tie_epsilon_px");
  if(snap<0||tie<0||tie>snap)throw std::invalid_argument("BOUNDARY_ENDPOINT_SNAP_CONFIG");
  const auto& l=side.at("landmark");double error=l.at("localization_error_px");
  if(!std::isfinite(error)||(error<0&&error!=-1))throw std::invalid_argument("PAIR_UI_LANDMARK_ERROR");
  for(const char* key:{"source","trace","image_sha256"})if(l.at(key).get<std::string>().size()>512)throw std::invalid_argument("PAIR_UI_LANDMARK_TEXT");
 }
 if(s.at("correspondence_id").get<std::string>().size()>512)throw std::invalid_argument("PAIR_UI_CORRESPONDENCE");
}
inline std::string Hex(const std::string& text){
 static const char* digits="0123456789abcdef";std::string out;for(unsigned char c:text){out+=digits[c>>4];out+=digits[c&15];}return out;
}
inline std::string Unhex(const std::string& text){
 if(text.size()%2)throw std::invalid_argument("PAIR_UI_TOKEN");
 auto digit=[](char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;throw std::invalid_argument("PAIR_UI_TOKEN");};
 std::string out;for(size_t i=0;i<text.size();i+=2)out+=char(digit(text[i])*16+digit(text[i+1]));return out;
}
inline std::string Build(const J& s){
 Validate(s);std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(17);
 out<<"RefinedArcAudit a;\n";
 for(int i:{0,1}){
  const auto& b=s.at("sides").at(i);
  out<<"a.select("<<i<<");\n";
  for(const auto& item:std::vector<std::pair<const char*,const char*>>{{"caseid","case_id"},{"revision","revision"},{"image","image"}})
   out<<"a."<<item.first<<"("<<b.at(item.second).dump()<<");\n";
  out<<"a.span("<<b.at("span")<<");\n";
  for(const auto& v:cxgrayparams::Parameters())out<<"a.grayparameter("<<b.at("gray").at(v.key)<<",\""<<v.key<<"\");\n";
  if(!b.at("endpoint_provenance").get<std::string>().empty()){
   for(auto it=b.at("endpoints").begin();it!=b.at("endpoints").end();++it)out<<"a.endpointparameter("<<it.value()<<","<<J(it.key()).dump()<<");\n";
   out<<"a.endpointprovenance("<<b.at("endpoint_provenance").dump()<<");\n";
  }
  out<<"a.anchor("<<b.at("raw_anchor_begin")<<","<<b.at("raw_anchor_end")<<");\n";
  const auto& l=b.at("landmark");
  for(const auto& item:std::vector<std::pair<const char*,const char*>>{{"landmarksource","source"},{"landmarktrace","trace"},{"landmarksha","image_sha256"}})
   if(!l.at(item.second).get<std::string>().empty())out<<"a."<<item.first<<"("<<l.at(item.second).dump()<<");\n";
  if(l.at("localization_error_px").get<double>()>=0)out<<"a.landmarkerror("<<l.at("localization_error_px")<<");\n";
 }
 for(const auto& v:Parameters())out<<"a.parameter("<<s.at("parameters").at(v.key)<<",\""<<v.key<<"\");\n";
 if(!s.at("correspondence_id").get<std::string>().empty())out<<"a.correspondence("<<s.at("correspondence_id").dump()<<");\n";
 out<<"a.guiconfig(\""<<Hex(s.dump())<<"\");\na.run();\n";return out.str();
}
inline J Read(const std::string& script){
 const std::string marker="a.guiconfig(\"";
 const size_t position=script.rfind(marker);
 if(position==std::string::npos)throw std::invalid_argument("Pair controls require a generated Evidence pair; use Script Editor for custom scripts.");
 const size_t end=script.find('"',position+marker.size());auto s=J::parse(Unhex(script.substr(position+marker.size(),end-position-marker.size())));
 if(Build(s)!=script)throw std::invalid_argument("Pair script differs from its declared configuration; use Script Editor.");
 return s;
}
inline std::filesystem::path ImagePath(const J& side){
 auto path=std::filesystem::path(side.at("image").get<std::string>());
 if(path.empty())throw std::invalid_argument("PAIR_IMAGE_REQUIRED");
 if(path.is_relative()){
  auto root=std::getenv("CXVISION_RUN_ROOT");if(!root||!*root)throw std::invalid_argument("PAIR_RUN_ROOT_REQUIRED");
  auto base=std::filesystem::weakly_canonical(root),resolved=std::filesystem::weakly_canonical(base/path),rel=resolved.lexically_relative(base);
  if(rel.empty()||rel.is_absolute()||*rel.begin()=="..")throw std::invalid_argument("PAIR_IMAGE_OUTSIDE_RUN_ROOT");
  path=resolved;
 }
 return path;
}
inline std::string ImageSha(const J& side){
 auto image=cv::imread(ImagePath(side).string(),cv::IMREAD_COLOR);if(image.empty())throw std::invalid_argument("PAIR_IMAGE_UNREADABLE");
 cv::Mat gray;cv::cvtColor(image,gray,cv::COLOR_BGR2GRAY);
 std::string bytes=std::to_string(gray.cols)+":"+std::to_string(gray.rows)+":CV_8UC1:";
 for(int y=0;y<gray.rows;++y)bytes.append(reinterpret_cast<const char*>(gray.ptr<unsigned char>(y)),size_t(gray.cols));
 return cxgeom::so2::Sha256Bytes(bytes);
}
inline J PoseValues(const J& s){
 J p=J::object();for(const auto* key:{"samples","minimum_scale","maximum_scale","maximum_fit_rms_px","maximum_observed_error_px","support_distance_px",
  "minimum_weighted_support","maximum_endpoint_error_px","allow_reversed"})p[key]=s.at("parameters").at(key);
 p["samples"]=s.at("parameters").at("samples").get<int>();p["allow_reversed"]=false;return p;
}
}
