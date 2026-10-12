#pragma once
#include "CxObservedGrayBoundary.h"
#include "CxPartialBoundaryMaskInput.h"
namespace cxpartialimage {
// Immutable values, not an ObservedBoundaryInput: derived coordinates must never
// enter raw-only APIs by implicit conversion.
class BoundRefinedSpan {
 std::vector<std::complex<double>> raw_,refined_;
 std::vector<std::string> source_ids_;
 size_t begin_=0,end_=0;
 std::string evidence_;
 cxgeom::polar::ObservedBoundaryInput source_raw_;
 BoundRefinedSpan(const GrayBoundaryResult& result,const std::vector<size_t>& indices,const std::string& binding,
  const cxgeom::polar::ObservedBoundaryInput& raw):
  begin_(indices.front()),end_(indices.back()),source_raw_(raw){
  std::ostringstream o;o.imbue(std::locale::classic());o<<std::setprecision(17);
  o<<"{\"schema\":\"bound_refined_span.v1\",\"representation\":\"gray_derived_not_raw\",\"closed\":false,\"production_eligible\":false";
  o<<",\"source_run_begin\":"<<begin_<<",\"source_run_end\":"<<end_<<",\"binding\":"<<binding;
  o<<",\"refinement\":"<<result.receipt()<<",\"ordered_points\":[";
  for(size_t k=0;k<indices.size();++k){
   const size_t i=indices[k];const auto& p=result.points.at(i);
   if(!p.accepted||i!=begin_+k)throw std::invalid_argument("REFINED_SPAN_NONCONTIGUOUS");
   raw_.push_back(p.raw);refined_.push_back(p.refined);source_ids_.push_back(p.source_id);
   if(k)o<<',';
   o<<"{\"source_run_index\":"<<i<<",\"source_id\":"<<cxgeom::polar::receipt_detail::quote(p.source_id);
   o<<",\"derived\":true,\"raw\":["<<p.raw.real()<<','<<p.raw.imag()<<"],\"refined\":["<<p.refined.real()<<','<<p.refined.imag()<<"]}";
  }o<<"]}";evidence_=o.str();
 }
 friend class BoundGrayFrame;
public:
 const cxgeom::polar::ObservedBoundaryInput& source_raw()const{return source_raw_;}
 size_t source_begin()const{return begin_;}
 size_t source_end()const{return end_;}
 const std::vector<std::complex<double>>& raw_points()const{return raw_;}
 const std::vector<std::complex<double>>& refined_points()const{return refined_;}
 const std::vector<std::string>& source_ids()const{return source_ids_;}
 const std::string& receipt()const{return evidence_;}
};
class BoundGrayFrame {
 cv::Mat gray_;
 MaskBoundaryInput mask_;
 std::string image_id_,revision_,sha_,binding_;
 BoundGrayFrame(cv::Mat gray,MaskBoundaryInput mask,std::string image_id,std::string revision,std::string sha,int threshold):
  gray_(std::move(gray)),mask_(std::move(mask)),image_id_(std::move(image_id)),revision_(std::move(revision)),sha_(std::move(sha)){
  using cxgeom::polar::receipt_detail::quote;
  std::ostringstream o;o.imbue(std::locale::classic());
  o<<"{\"schema\":\"bound_gray_frame.v1\",\"binding_method\":\"owned_gray_snapshot_threshold_mask_extraction\"";
  o<<",\"same_pixel_frame\":true,\"transform\":\"identity\",\"threshold_mode\":\"BINARY_GREATER_THAN\",\"threshold\":"<<threshold;
  o<<",\"image_id\":"<<quote(image_id_)<<",\"dataset_revision\":"<<quote(revision_)<<",\"gray_sha256\":"<<quote(sha_);
  o<<",\"width\":"<<gray_.cols<<",\"height\":"<<gray_.rows<<",\"mask\":"<<mask_.receipt()<<",\"production_eligible\":false}";
  binding_=o.str();
 }
public:
 // Establishes correspondence by construction, not by trusting a supplied mask,
 // caller SHA, image_id, or mutable refinement receipt.
 static BoundGrayFrame Capture(const cv::Mat& gray,const std::string& image_id,const std::string& revision,
  int threshold,const cv::Rect& anchor,const cxgeom::polar::BoundaryEndpointAnchors* endpoints=nullptr){
  if(gray.empty()||gray.type()!=CV_8UC1||gray.total()>16777216)throw std::invalid_argument("BOUND_FRAME_IMAGE");
  if(image_id.empty()||revision.empty())throw std::invalid_argument("BOUND_FRAME_PROVENANCE");
  if(threshold<0||threshold>254)throw std::invalid_argument("BOUND_FRAME_THRESHOLD");
  auto snapshot=gray.clone();std::string bytes=std::to_string(gray.cols)+":"+std::to_string(gray.rows)+":CV_8UC1:";
  for(int y=0;y<snapshot.rows;++y)bytes.append(reinterpret_cast<const char*>(snapshot.ptr<unsigned char>(y)),size_t(snapshot.cols));
  auto sha=cxgeom::so2::Sha256Bytes(bytes);
  cv::Mat mask;cv::threshold(snapshot,mask,double(threshold),255,cv::THRESH_BINARY);
  auto input=FromBinaryMask(mask,anchor,image_id+":gray_sha256="+sha+":threshold="+std::to_string(threshold),revision,"bound_span",128,3,endpoints);
  return BoundGrayFrame(std::move(snapshot),std::move(input),image_id,revision,sha,threshold);
 }
 const cxgeom::polar::ObservedBoundaryInput& raw()const{return mask_.boundary;}
 const std::string& receipt()const{return binding_;}
 GrayBoundaryResult refine(const cxgeom::polar::BoundaryNormalConfig& normal={},const GrayBoundaryConfig& gray={})const{
  auto result=RefineGrayBoundaryFromObserved(gray_,mask_.boundary,image_id_,revision_,normal,gray);
  if(result.gray_sha256!=sha_||result.raw_receipt!=mask_.boundary.receipt())throw std::logic_error("BOUND_FRAME_INTERNAL_MISMATCH");
  return result;
 }
 std::vector<BoundRefinedSpan> refined_spans(const cxgeom::polar::BoundaryNormalConfig& normal={},
  const GrayBoundaryConfig& gray={},size_t minimum=3)const{
  auto result=refine(normal,gray);std::vector<BoundRefinedSpan> spans;
  for(const auto& indices:AcceptedGrayRuns(result,minimum))spans.push_back(BoundRefinedSpan(result,indices,binding_,raw()));
  return spans;
 }
};
}
