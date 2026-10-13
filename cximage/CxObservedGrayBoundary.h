#pragma once
#include "CxGrayBoundaryRefinement.h"
#include "../cxgeom/polar_discrete_harmonic/boundary_normal_estimation.h"
namespace cxpartialimage {
inline GrayBoundaryResult RefineGrayBoundaryFromObserved(const cv::Mat& gray,
 const cxgeom::polar::ObservedBoundaryInput& raw,const std::string& image_id,const std::string& revision,
 const cxgeom::polar::BoundaryNormalConfig& normal_config={},const GrayBoundaryConfig& gray_config={}){
 auto estimated=cxgeom::polar::EstimateBoundaryNormals(raw,normal_config);
 std::vector<std::complex<double>> normals;std::vector<std::string> exclusions;
 for(const auto& p:estimated.points){
  normals.push_back(p.accepted?p.normal:std::complex<double>(1,0)); // Never sampled for excluded points.
  exclusions.push_back(p.accepted?"":p.status);
 }
 auto result=RefineGrayBoundary(gray,raw,normals,image_id,revision,gray_config,&exclusions);
 result.normals_basis="observed_two_sided_chords";result.normal_receipt_json=estimated.receipt();
 return result;
}
// Refined runs are source-index selections, NOT new raw ObservedBoundaryInput.
// Every rejected point splits the run; no joining, smoothing or closing across gaps.
inline std::vector<std::vector<size_t>> AcceptedGrayRuns(const GrayBoundaryResult& result,size_t minimum=3){
 if(minimum<2||minimum>128)throw std::invalid_argument("REFINED_RUN_MINIMUM");
 std::vector<std::vector<size_t>> runs;std::vector<size_t> current;
 auto flush=[&]{if(current.size()>=minimum)runs.push_back(current);current.clear();};
 for(size_t i=0;i<result.points.size();++i){
  if(result.points[i].accepted)current.push_back(i);else flush();
 }
 flush();return runs;
}
}
