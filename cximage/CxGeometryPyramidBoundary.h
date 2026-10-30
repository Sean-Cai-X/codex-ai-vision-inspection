#pragma once
#include "CxGeometryImagePyramid.h"
#include <set>
namespace cximage::geometry {
struct PyramidBoundaryPoint {
 std::string local_id;
 cv::Point2d measured; // extractor's level-local pixel coordinates
};
struct PyramidBoundaryExtraction {
 std::string extractor_revision,effective_parameters;
 std::vector<PyramidBoundaryPoint> points;
};
struct PyramidBoundaryTrace {
 std::string source_revision,point_id,extractor_revision,effective_parameters;
 int level=0;
 cv::Point2d measured,original;
 double original_units_per_pixel=1;
 double original_units_per_pixel_y=1;
 cv::Point2d original_offset;
};
struct PyramidBoundaryLevel {
 int index=0;
 std::string status;
 std::vector<PyramidBoundaryTrace> points;
};
struct PyramidBoundaryResult {
 std::vector<PyramidBoundaryLevel> levels; // coarse to fine; no inferred cross-level identity
 bool production_eligible=false;
};
// Extractor adapter: caller owns algorithm choice and effective parameters.
// Point clouds alone do not establish ordering, closure, identity or pose.
template<class Extractor> PyramidBoundaryResult ExtractPyramidBoundary(
 const ImagePyramid& pyramid,Extractor extract,size_t maximum_points_per_level=100000){
 if(pyramid.levels.empty()||maximum_points_per_level==0||maximum_points_per_level>1000000)
  throw std::invalid_argument("INVALID_BOUNDARY_EXTRACTION_BUDGET");
 PyramidBoundaryResult result;
 for(auto it=pyramid.levels.rbegin();it!=pyramid.levels.rend();++it){
  const auto& l=*it;
  if(l.gray.empty()||l.gray.type()!=CV_8UC1||l.source_revision.empty()||
    !std::isfinite(l.original_units_per_pixel)||l.original_units_per_pixel<=0)
   throw std::invalid_argument("INVALID_BOUNDARY_LEVEL");
  // Deep copy isolates even OpenCV's shallow-const Mat from extractor mutations.
  auto isolated=l;isolated.gray=l.gray.clone();const auto extraction=extract(isolated);
  if(extraction.extractor_revision.empty()||extraction.effective_parameters.empty())
   throw std::invalid_argument("MISSING_EXTRACTOR_PROVENANCE");
  if(extraction.points.size()>maximum_points_per_level)throw std::invalid_argument("BOUNDARY_POINT_BUDGET_EXHAUSTED");
  PyramidBoundaryLevel level;level.index=l.index;level.status=extraction.points.empty()?"NO_BOUNDARY":"OBSERVED_POINTS_ONLY";
  std::set<std::string> ids;
  for(const auto& p:extraction.points){
   if(p.local_id.empty()||!ids.insert(p.local_id).second||!std::isfinite(p.measured.x)||!std::isfinite(p.measured.y)||
     p.measured.x<0||p.measured.y<0||p.measured.x>l.gray.cols-1||p.measured.y>l.gray.rows-1)
    throw std::invalid_argument("INVALID_BOUNDARY_POINT");
   const auto original=l.ToOriginal(p.measured);
   if(!std::isfinite(original.x)||!std::isfinite(original.y))throw std::invalid_argument("BOUNDARY_MAPPING_OVERFLOW");
   level.points.push_back({l.source_revision,"L"+std::to_string(l.index)+":"+p.local_id,
    extraction.extractor_revision,extraction.effective_parameters,l.index,p.measured,original,l.original_units_per_pixel,l.ScaleY(),l.original_offset});
  }
  result.levels.push_back(std::move(level));
 }
 return result;
}
}
