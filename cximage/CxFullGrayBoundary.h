#pragma once
#include "CxBoundGrayFrame.h"
#include "../cxgeom/include/CxGeoBoundaryChunkPlan.h"
#include "../libtorchsegmentation/src/utils/json.hpp"
namespace cxpartialimage {
// Full observed contour aggregation; not a raw-only pose input.
struct FullGrayBoundaryResult {
 GrayBoundaryResult refinement;
 nlohmann::json evidence;
 std::vector<std::vector<size_t>> runs;
};
inline FullGrayBoundaryResult RefineFullGrayBoundary(const cv::Mat& image,
 const std::string& id,const std::string& revision,int threshold,int polarity,
 const cv::Rect& anchor,const cxgeom::polar::BoundaryNormalConfig& normal,
 const GrayBoundaryConfig& gray,size_t core=100,size_t maximum_chunks=64){
 using J=nlohmann::json;
 auto need=[](bool ok,const char* why){if(!ok)throw std::invalid_argument(why);};
 need(!image.empty()&&image.type()==CV_8UC1&&image.total()<=16777216,"FULL_GRAY_IMAGE");
 need(threshold>=0&&threshold<=254&&(polarity==0||polarity==1),"FULL_GRAY_THRESHOLD_POLARITY");
 need(core>0&&core+2*normal.half_window<=128&&maximum_chunks>0&&maximum_chunks<=256,"FULL_GRAY_CHUNK_BUDGET");
 need(anchor.width>0&&anchor.height>0&&anchor.x>=0&&anchor.y>=0&&
  double(anchor.x)+anchor.width<=image.cols&&double(anchor.y)+anchor.height<=image.rows,"FULL_GRAY_ANCHOR");
 cv::Mat mask;cv::threshold(image,mask,threshold,255,polarity?cv::THRESH_BINARY_INV:cv::THRESH_BINARY);
 std::vector<std::vector<cv::Point>> contours;
 cv::findContours(mask,contours,cv::RETR_TREE,cv::CHAIN_APPROX_NONE);
 need(contours.size()==1,"FULL_GRAY_SINGLE_COMPONENT_WITHOUT_HOLES_REQUIRED");
 const auto& parent=contours.front();bool anchored=false;
 for(auto p:parent){need(p.x>0&&p.y>0&&p.x<image.cols-1&&p.y<image.rows-1,"FULL_GRAY_BORDER_TOUCH");
  anchored=anchored||anchor.contains(p);}
 need(anchored,"FULL_GRAY_NO_ANCHORED_CONTOUR");
 auto plan=cxgeom::PlanClosedBoundaryChunks(parent.size(),core,normal.half_window);
 need(plan.size()<=maximum_chunks,"FULL_GRAY_MAXIMUM_CHUNKS");
 FullGrayBoundaryResult out;std::vector<int> ownership(parent.size());std::vector<bool> accepted(parent.size());
 J chunks=J::array(),mapping=J::array();out.refinement.points.resize(parent.size());
 for(size_t ci=0;ci<plan.size();++ci){
  const auto& c=plan[ci];cxgeom::polar::BoundaryEndpointAnchors e;
  auto point=[&](size_t k){auto p=parent[c.parent_indices[k]];return std::complex<double>(p.x,p.y);};
  e.start=point(0);e.end=point(c.parent_indices.size()-1);e.via=point(c.parent_indices.size()/2);
  e.maximum_snap_px=1e-6;e.tie_epsilon_px=0;e.provenance="image_extracted_ordered_chunk_not_pose_landmarks";
  // Select the complete observed component; anchor was already checked above.
  auto frame=BoundGrayFrame::Capture(image,id,revision,threshold,{0,0,image.cols,image.rows},&e,polarity);
  auto r=frame.refine(normal,gray);
  need(r.points.size()==c.parent_indices.size(),"FULL_GRAY_CHUNK_SELECTION_MISMATCH");
  if(ci==0){
   out.refinement.image_id=r.image_id;out.refinement.revision=r.revision;
   out.refinement.gray_sha256=r.gray_sha256;out.refinement.width=r.width;out.refinement.height=r.height;
   out.refinement.config=r.config;out.refinement.normals_basis="observed_cyclic_chunk_halo";
  }
  for(size_t k=0;k<c.parent_indices.size();++k)
   need(std::abs(r.points[k].raw-point(k))<1e-12,"FULL_GRAY_PARENT_ORDER_MISMATCH");
  for(size_t k=c.halo;k<c.halo+c.core_count;++k){
   size_t ix=c.parent_indices[k];need(++ownership[ix]==1,"FULL_GRAY_DUPLICATE_CORE");
   out.refinement.points[ix]=r.points[k];accepted[ix]=r.points[k].accepted;
   mapping.push_back({{"parent_index",ix},{"chunk_index",ci},{"local_index",k},{"source_id",r.points[k].source_id}});
  }
  chunks.push_back({{"index",ci},{"core_begin",c.core_begin},{"core_count",c.core_count},
   {"halo",c.halo},{"parent_indices",c.parent_indices},{"wraps_parent_origin",c.wraps_parent_origin},
   {"binding",J::parse(frame.receipt())},{"refinement",J::parse(r.receipt())}});
 }
 for(auto n:ownership)need(n==1,"FULL_GRAY_UNCOVERED_CORE");
 auto runs=cxgeom::OrderedAcceptedBoundaryRuns(accepted);out.runs=runs.runs;
 J raw=J::array();for(auto p:parent)raw.push_back({p.x,p.y});
 out.refinement.raw_receipt=J({{"schema","ordered_observed_contour.v1"},{"closed",true},{"points",raw}}).dump();
 out.refinement.normal_receipt_json=J({{"basis","per_chunk_observed_halo"},{"half_window",normal.half_window}}).dump();
 out.evidence={{"schema","full_gray_boundary_batch.v1"},{"production_eligible",false},{"pose_evaluated",false},
  {"gray_sha256",out.refinement.gray_sha256},{"requested_core_points",core},{"maximum_chunks",maximum_chunks},
  {"anchor",{{"x",anchor.x},{"y",anchor.y},{"width",anchor.width},{"height",anchor.height}}},
  {"halo_counted_in_scores",false},{"unique_core_count",parent.size()},{"chunks",chunks},{"core_mapping",mapping},
  {"accepted_runs",out.runs},{"output_runs_closed",false},{"observed_cycle_complete",runs.observed_cycle_complete},
  {"seam_supported",runs.seam_supported},{"synthetic_points",0}};
 return out;
}
}
