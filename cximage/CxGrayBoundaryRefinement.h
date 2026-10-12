#pragma once
#include "../cxgeom/polar_discrete_harmonic/observed_boundary_input.h"
#include "../cxgeom/include/CxGeoSO2ReferenceAsset.h"
#include <opencv2/core.hpp>
#include <set>
namespace cxpartialimage {
// Local grayscale midpoint crossing only. Caller supplies ordered observation
// normals; not an automatic normal estimator, segmentation model, or metrology certification.
struct GrayBoundaryConfig {
 double radius_px=4,step_px=.25,minimum_contrast=20,maximum_shift_px=2;
 double monotonic_tolerance=2,noise_floor_dn=1;
};
struct GrayBoundaryPoint {
 std::string source_id,status;
 std::complex<double> raw,normal,refined;
 std::vector<double> profile;
 bool accepted=false,normal_available=true;
 double contrast=0,shift_px=0,slope_dn_per_px=0,sensitivity_px=0;
};
struct GrayBoundaryResult {
 std::string raw_receipt,image_id,revision,gray_sha256;
 int width=0,height=0;
 std::string normals_basis="caller_supplied",normal_receipt_json="null";
 GrayBoundaryConfig config;
 std::vector<GrayBoundaryPoint> points;
 std::string receipt()const{
  using cxgeom::polar::receipt_detail::quote;
  std::ostringstream o;o.imbue(std::locale::classic());o<<std::setprecision(17);
  o<<"{\"schema\":\"gray_boundary_refinement.v1\",\"production_eligible\":false,\"raw_modified\":false";
  o<<",\"method\":\"bilinear_profile_endpoint_midpoint_crossing\",\"normals_basis\":"<<quote(normals_basis)<<",\"normal_estimation\":"<<normal_receipt_json;
  o<<",\"uncertainty_calibrated\":false,\"sensitivity_semantics\":\"noise_floor_plus_half_DN_divided_by_local_slope_not_confidence_interval\"";
  o<<",\"image_id\":"<<quote(image_id)<<",\"dataset_revision\":"<<quote(revision)<<",\"gray_sha256\":"<<quote(gray_sha256);
  o<<",\"width\":"<<width<<",\"height\":"<<height;
  o<<",\"hash_basis\":\"width_height_CV_8UC1_row_bytes_v1\",\"raw\":"<<raw_receipt;
  o<<",\"config\":{\"radius_px\":"<<config.radius_px<<",\"step_px\":"<<config.step_px<<",\"minimum_contrast\":"<<config.minimum_contrast;
  o<<",\"maximum_shift_px\":"<<config.maximum_shift_px<<",\"monotonic_tolerance\":"<<config.monotonic_tolerance<<",\"noise_floor_dn\":"<<config.noise_floor_dn<<"},\"points\":[";
  for(size_t i=0;i<points.size();++i){
   if(i)o<<',';
   const auto& p=points[i];
   o<<"{\"source_id\":"<<quote(p.source_id)<<",\"status\":"<<quote(p.status)<<",\"accepted\":"<<(p.accepted?"true":"false");
   o<<",\"raw\":["<<p.raw.real()<<','<<p.raw.imag()<<"],\"normal\":";
   if(p.normal_available)o<<'['<<p.normal.real()<<','<<p.normal.imag()<<']';else o<<"null";
   o<<",\"contrast\":"<<p.contrast;
   o<<",\"refined\":";
   if(p.accepted)o<<"{\"derived\":true,\"observed_pixel\":false,\"point\":["<<p.refined.real()<<','<<p.refined.imag()<<"],\"shift_px\":"<<p.shift_px<<",\"slope_dn_per_px\":"<<p.slope_dn_per_px<<",\"sensitivity_px\":"<<p.sensitivity_px<<'}';
   else o<<"null";
   o<<",\"profile\":[";for(size_t j=0;j<p.profile.size();++j){if(j)o<<',';o<<p.profile[j];}o<<"]}";
  }o<<"]}";return o.str();
 }
};
inline GrayBoundaryResult RefineGrayBoundary(const cv::Mat& gray,
 const cxgeom::polar::ObservedBoundaryInput& raw,const std::vector<std::complex<double>>& normals,
 const std::string& image_id,const std::string& revision,const GrayBoundaryConfig& cfg={},
 const std::vector<std::string>* excluded_points=nullptr){
 auto need=[](bool b,const char* why){if(!b)throw std::invalid_argument(why);};
 auto positive=[](double v){return std::isfinite(v)&&v>0;};
 need(!gray.empty()&&gray.type()==CV_8UC1&&gray.total()<=16777216,"GRAY_BOUNDARY_IMAGE");
 need(!image_id.empty()&&revision==raw.revision_id(),"GRAY_BOUNDARY_PROVENANCE");
 need(normals.size()==raw.features().size(),"GRAY_BOUNDARY_NORMAL_COUNT");
 need(!excluded_points||excluded_points->size()==raw.features().size(),"GRAY_BOUNDARY_EXCLUSION_COUNT");
 need(positive(cfg.radius_px)&&cfg.radius_px<=16&&positive(cfg.step_px)&&cfg.step_px<=1&&
  2*cfg.radius_px/cfg.step_px>=8&&2*cfg.radius_px/cfg.step_px<=256&&std::abs(2*cfg.radius_px/cfg.step_px-std::round(2*cfg.radius_px/cfg.step_px))<1e-8&&
  positive(cfg.minimum_contrast)&&cfg.minimum_contrast<=255&&positive(cfg.maximum_shift_px)&&cfg.maximum_shift_px<=cfg.radius_px&&
  std::isfinite(cfg.monotonic_tolerance)&&cfg.monotonic_tolerance>=0&&cfg.monotonic_tolerance<=255&&positive(cfg.noise_floor_dn)&&cfg.noise_floor_dn<=255,"GRAY_BOUNDARY_CONFIG");
 for(auto n:normals)need(std::isfinite(std::abs(n))&&std::abs(std::abs(n)-1)<1e-6,"GRAY_BOUNDARY_UNIT_NORMAL");
 std::string bytes=std::to_string(gray.cols)+":"+std::to_string(gray.rows)+":CV_8UC1:";
 for(int y=0;y<gray.rows;++y)bytes.append(reinterpret_cast<const char*>(gray.ptr<unsigned char>(y)),size_t(gray.cols));
 GrayBoundaryResult out;out.raw_receipt=raw.receipt();out.image_id=image_id;out.revision=revision;
 out.gray_sha256=cxgeom::so2::Sha256Bytes(bytes);out.config=cfg;
 out.width=gray.cols;out.height=gray.rows;
 const size_t count=size_t(std::llround(2*cfg.radius_px/cfg.step_px))+1;
 for(size_t i=0;i<raw.features().size();++i){
  GrayBoundaryPoint p;p.raw=raw.features()[i].point;p.source_id=raw.features()[i].stable_id;p.normal=normals[i];
  if(excluded_points&&!(*excluded_points)[i].empty()){
   p.normal_available=false;p.status=(*excluded_points)[i];out.points.push_back(p);continue;
  }
  std::set<int> native_levels;
  for(size_t j=0;j<count;++j){
   auto q=p.raw+(-cfg.radius_px+double(j)*cfg.step_px)*p.normal;
   if(q.real()<0||q.imag()<0||q.real()>=gray.cols-1||q.imag()>=gray.rows-1){p.status="PROFILE_OUT_OF_IMAGE";break;}
   int x=int(std::floor(q.real())),y=int(std::floor(q.imag()));double u=q.real()-x,v=q.imag()-y;
   double a=gray.at<unsigned char>(y,x),b=gray.at<unsigned char>(y,x+1),c=gray.at<unsigned char>(y+1,x),d=gray.at<unsigned char>(y+1,x+1);
   for(double value:{a,b,c,d})native_levels.insert(int(value));
   p.profile.push_back((1-v)*((1-u)*a+u*b)+v*((1-u)*c+u*d));
  }
  if(!p.status.empty()){out.points.push_back(p);continue;}
  p.contrast=std::abs(p.profile.back()-p.profile.front());
  if(p.contrast<cfg.minimum_contrast)p.status="LOW_CONTRAST";
  else if(native_levels.size()<3)p.status="INSUFFICIENT_NATIVE_GRAY_LEVELS";
  else{
   double level=.5*(p.profile.front()+p.profile.back()),sign=p.profile.back()>p.profile.front()?1.:-1.;
   bool monotonic=true,plateau=false;std::vector<double> crossings;double slope=0;
   for(size_t j=1;j<count;++j){
    double a=sign*(p.profile[j-1]-level),b=sign*(p.profile[j]-level);
    if(b-a < -cfg.monotonic_tolerance)monotonic=false;
    if(a==0&&b==0)plateau=true;
    if(a<=0&&b>0){
     crossings.push_back(-cfg.radius_px+(double(j-1)-a/(b-a))*cfg.step_px);
     slope=(b-a)/cfg.step_px;
    }
   }
   if(plateau||crossings.size()!=1)p.status="AMBIGUOUS_LEVEL_CROSSING";
   else if(!monotonic)p.status="NON_MONOTONIC_PROFILE";
   else if(std::abs(crossings.front())>cfg.maximum_shift_px)p.status="SHIFT_OUT_OF_RANGE";
   else{
    p.shift_px=crossings.front();p.refined=p.raw+p.shift_px*p.normal;p.slope_dn_per_px=slope;
    p.sensitivity_px=(cfg.noise_floor_dn+.5)/slope;p.accepted=true;p.status="REFINED_GRAY_ESTIMATE_NOT_CALIBRATED";
   }
  }
  out.points.push_back(p);
 }
 return out;
}
}
