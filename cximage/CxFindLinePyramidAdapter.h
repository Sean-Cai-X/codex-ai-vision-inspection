#pragma once
#include "CxGeometryPyramidBoundary.h"
#include <optional>
namespace cximage::geometry {
struct FindLinePyramidTrace {
 std::string id,source_revision,analysis_status;
 int level=0,scan_index=-1,scan_type=-1,polarity=0;
 cv::Point2d measured_original,selected_original;
 bool refined_used=false,uncertainty_calibrated=false;
 std::optional<double> localization_proxy_original;
 std::optional<double> fit_residual_original;
};
// Snapshot coordinates must refer to the image passed to FindLine, explicitly
// selected by the caller. No inference from coordinate magnitude is allowed.
enum class FindLineSnapshotFrame { LevelImage, OriginalImage };
template<class Snapshot> std::vector<FindLinePyramidTrace> AdaptFindLinePyramidSnapshot(
 const ImagePyramidLevel& level,const Snapshot& snapshot,FindLineSnapshotFrame frame,
 const std::string& execution_revision,size_t maximum_points=100000){
 if(execution_revision.empty()||level.source_revision.empty()||snapshot.status.empty()||
   maximum_points==0||snapshot.points.size()>maximum_points||level.gray.empty()||
   !std::isfinite(level.original_units_per_pixel)||level.original_units_per_pixel<=0)
  throw std::invalid_argument("INVALID_FINDLINE_SNAPSHOT_BINDING");
 if(frame!=FindLineSnapshotFrame::LevelImage&&frame!=FindLineSnapshotFrame::OriginalImage)
  throw std::invalid_argument("INVALID_FINDLINE_COORDINATE_FRAME");
 const double scale=frame==FindLineSnapshotFrame::LevelImage?level.original_units_per_pixel:1.;
 if(frame==FindLineSnapshotFrame::LevelImage&&!level.IsIsotropic())
  throw std::invalid_argument("FINDLINE_DIRECTIONAL_UNCERTAINTY_REQUIRED_FOR_ANISOTROPIC_LEVEL");
 auto map=[&](double x,double y){
  if(!std::isfinite(x)||!std::isfinite(y))throw std::invalid_argument("NONFINITE_FINDLINE_POINT");
  auto p=cv::Point2d(x,y);
  if(frame==FindLineSnapshotFrame::LevelImage){
   if(x<0||y<0||x>level.gray.cols-1||y>level.gray.rows-1)throw std::invalid_argument("FINDLINE_POINT_OUTSIDE_LEVEL");
   return level.ToOriginal(p);
  }
  if(x<level.original_roi.x||y<level.original_roi.y||x>level.original_roi.x+level.original_roi.width-1||y>level.original_roi.y+level.original_roi.height-1)
   throw std::invalid_argument("FINDLINE_POINT_OUTSIDE_ROI");
  return p;
 };
 std::vector<FindLinePyramidTrace> out;std::set<std::pair<int,int>> ids;
 for(const auto& p:snapshot.points){
  if(p.scan_index<0||(p.scan_type!=0&&p.scan_type!=1)||p.polarity<-1||p.polarity>1||!ids.emplace(p.scan_type,p.scan_index).second)
   throw std::invalid_argument("INVALID_FINDLINE_SCAN_IDENTITY");
  FindLinePyramidTrace t;t.id=execution_revision+":L"+std::to_string(level.index)+":S"+std::to_string(p.scan_type)+":"+std::to_string(p.scan_index);
  t.source_revision=level.source_revision;t.analysis_status=snapshot.status;t.level=level.index;t.scan_index=p.scan_index;t.scan_type=p.scan_type;t.polarity=p.polarity;
  t.measured_original=map(p.measured_x,p.measured_y);t.selected_original=t.measured_original;
  if(p.interpolation_valid){t.selected_original=map(p.refined_x,p.refined_y);t.refined_used=true;}
  // Existing sigma is noise/gradient proxy, not calibrated localization sigma.
  // Zero/default and absent profile response stay unknown, never perfect accuracy.
  if(std::isfinite(p.response_strength)&&p.response_strength>0&&std::isfinite(p.localization_sigma_px)&&p.localization_sigma_px>0)
   t.localization_proxy_original=p.localization_sigma_px*scale;
  if(p.fit_residual_valid){
   if(!std::isfinite(p.fit_residual_px))throw std::invalid_argument("INVALID_FINDLINE_RESIDUAL");
   t.fit_residual_original=p.fit_residual_px*scale;
  }
  out.push_back(t);
 }
 return out;
}
}
