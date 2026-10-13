#pragma once
#include "../cxgeom/polar_discrete_harmonic/observed_boundary_bundle.h"
#include "../cxgeom/polar_discrete_harmonic/boundary_endpoint_selection.h"
#include "../cxgeom/include/CxGeoSO2ReferenceAsset.h"
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cstdint>
namespace cxpartialimage {
class MaskBoundaryRejection:public std::invalid_argument{
 std::string evidence_;
public:
 MaskBoundaryRejection(const char* reason,std::string evidence):std::invalid_argument(reason),evidence_(std::move(evidence)){}
 const std::string& receipt()const{return evidence_;}
};
struct MaskBoundaryInput {
 cxgeom::polar::ObservedBoundaryInput boundary;
 std::string mask_sha256;
 int width=0,height=0,contour_index=0,outer_count=0,hole_count=0;
 cv::Rect anchor;
 size_t maximum_features=128;
 int minimum_points=3;
 std::string endpoint_selection_json="null";
 std::string receipt()const{
  std::ostringstream o;o.imbue(std::locale::classic());
  o<<"{\"schema\":\"mask_boundary_input.v1\",\"extraction\":\"RETR_TREE_CHAIN_APPROX_NONE\"";
  o<<",\"mask_sha256\":"<<cxgeom::polar::receipt_detail::quote(mask_sha256);
  o<<",\"hash_basis\":\"width_height_CV_8UC1_row_bytes_v1\",\"width\":"<<width<<",\"height\":"<<height;
  o<<",\"contour_index\":"<<contour_index<<",\"outer_count\":"<<outer_count<<",\"hole_count\":"<<hole_count;
  o<<",\"anchor_half_open\":["<<anchor.x<<','<<anchor.y<<','<<anchor.width<<','<<anchor.height<<']';
  o<<",\"maximum_features\":"<<maximum_features<<",\"minimum_points\":"<<minimum_points;
  o<<",\"endpoint_selection\":"<<endpoint_selection_json;
  o<<",\"model_inference_performed\":false,\"production_eligible\":false,\"boundary\":"<<boundary.receipt()<<'}';
  // SHA describes decoded mask bytes, not a signature or original photograph.
  return o.str();
 }
};
// Explicit binary mask only. No threshold choice, morphology, fitting, largest-component
// selection, contour compression, or sampling is hidden in this bridge.
inline MaskBoundaryInput FromBinaryMask(const cv::Mat& mask,const cv::Rect& anchor,
 const std::string& image_id,const std::string& dataset_revision,const std::string& segment_id,
 size_t maximum_features=128,int minimum_points=3,
 const cxgeom::polar::BoundaryEndpointAnchors* endpoints=nullptr){
 auto need=[](bool b,const char* why){if(!b)throw std::invalid_argument(why);};
 need(!mask.empty()&&mask.type()==CV_8UC1&&mask.total()<=16777216,"MASK_BOUNDARY_INVALID_IMAGE");
 need(maximum_features>=3&&maximum_features<=128&&minimum_points>=3&&size_t(minimum_points)<=maximum_features,"MASK_BOUNDARY_INVALID_POINT_LIMITS");
 need(!image_id.empty(),"MASK_BOUNDARY_MISSING_IMAGE_ID");
 need(!dataset_revision.empty()&&!segment_id.empty(),"BOUNDARY_MISSING_PROVENANCE");
 need(anchor.x>=0&&anchor.y>=0&&anchor.width>0&&anchor.height>0&&
  int64_t(anchor.x)+anchor.width<=mask.cols&&int64_t(anchor.y)+anchor.height<=mask.rows,"MASK_BOUNDARY_INVALID_ANCHOR");
 std::string bytes=std::to_string(mask.cols)+":"+std::to_string(mask.rows)+":CV_8UC1:";
 for(int y=0;y<mask.rows;++y){
  const auto* row=mask.ptr<unsigned char>(y);
  for(int x=0;x<mask.cols;++x)need(row[x]==0||row[x]==255,"MASK_BOUNDARY_NOT_BINARY");
  bytes.append(reinterpret_cast<const char*>(row),size_t(mask.cols));
 }
 const auto sha=cxgeom::so2::Sha256Bytes(bytes);
 cv::Mat padded;cv::copyMakeBorder(mask,padded,1,1,1,1,cv::BORDER_CONSTANT,cv::Scalar(0));
 std::vector<std::vector<cv::Point>> contours;std::vector<cv::Vec4i> hierarchy;
 cv::findContours(padded,contours,hierarchy,cv::RETR_TREE,cv::CHAIN_APPROX_NONE,cv::Point(-1,-1));
 int chosen=-1,candidates=0,outer=0,holes=0;
 for(size_t i=0;i<contours.size();++i){
  if(hierarchy[i][3]>=0){
   int depth=0;for(int p=hierarchy[i][3];p>=0;p=hierarchy[size_t(p)][3])++depth;
   if(depth%2==1)++holes;
   continue;
  }
  ++outer;
  if(std::any_of(contours[i].begin(),contours[i].end(),[&](const cv::Point& p){return anchor.contains(p);})){
   chosen=int(i);++candidates;
  }
 }
 if(candidates!=1){
  const char* reason=candidates?"MASK_BOUNDARY_AMBIGUOUS_CONTOURS":"MASK_BOUNDARY_NO_ANCHORED_CONTOUR";
  std::ostringstream o;o.imbue(std::locale::classic());
  o<<"{\"schema\":\"mask_boundary_rejection.v1\",\"status\":"<<cxgeom::polar::receipt_detail::quote(reason)
   <<",\"mask_sha256\":"<<cxgeom::polar::receipt_detail::quote(sha)<<",\"candidate_count\":"<<candidates
   <<",\"outer_count\":"<<outer<<",\"hole_count\":"<<holes<<",\"selected\":false,\"gap_bridging\":false,\"candidate_contours\":[";
  bool comma=false;for(size_t i=0;i<contours.size();++i)if(hierarchy[i][3]<0&&
   std::any_of(contours[i].begin(),contours[i].end(),[&](const cv::Point& p){return anchor.contains(p);})){
   if(comma)o<<',';comma=true;auto box=cv::boundingRect(contours[i]);
   o<<"{\"contour_index\":"<<i<<",\"point_count\":"<<contours[i].size()<<",\"bounds\":["
    <<box.x<<','<<box.y<<','<<box.width<<','<<box.height<<"]}";
  }o<<"]}";throw MaskBoundaryRejection(reason,o.str());
 }
 std::vector<std::complex<double>> parent;
 for(const auto& p:contours[size_t(chosen)])parent.emplace_back(p.x,p.y);
 cxgeom::OpenBoundaryRun run;std::string selection="null";
 if(endpoints){
  // In endpoint mode the rectangle selects a component, not the segment extent.
  try{
   auto selected=cxgeom::polar::SelectBoundaryEndpoints(parent,*endpoints,size_t(minimum_points));
   run=std::move(selected.run);selection=selected.receipt();
  }catch(const cxgeom::polar::BoundaryEndpointRejection& e){
   auto evidence=e.receipt();evidence.pop_back();
   evidence+=",\"mask_sha256\":"+cxgeom::polar::receipt_detail::quote(sha)+",\"contour_index\":"+std::to_string(chosen)+
    ",\"parent_point_count\":"+std::to_string(parent.size())+"}";
   throw cxgeom::polar::BoundaryEndpointRejection(e.what(),evidence);
  }
 }else{
  run=cxgeom::ExtractOpenBoundaryRun(parent,anchor.x,anchor.y,anchor.width,anchor.height,minimum_points);
 }
 const auto source=image_id+":mask_sha256="+sha;
 cxgeom::polar::ObservedBoundaryInput input(source,dataset_revision,"outer:"+std::to_string(chosen),segment_id,parent,run,maximum_features);
 need(!image_id.empty(),"MASK_BOUNDARY_MISSING_IMAGE_ID");
 return {std::move(input),sha,mask.cols,mask.rows,chosen,outer,holes,anchor,maximum_features,minimum_points,selection};
}
}
