#pragma once
#include "../include/CxGeoOpenBoundary.h"
#include "partial_contract.h"
namespace cxgeom::polar {
// Immutable observation adapter. The solver consumes points, NOT curve topology.
// Parent coordinates must be in the same image coordinate frame as the run.
// Caller must supply observed (not completed/synthetic) parent samples.
// This verifies internal consistency, not image provenance or extraction accuracy.
// Stable parent indexing and revision identity are the caller's responsibility.
class ObservedBoundaryInput {
 std::string source_,revision_,contour_,segment_;
 cxgeom::OpenBoundaryRun run_;
 std::vector<Feature> features_;
 static std::string token(const std::string& s){return std::to_string(s.size())+":"+s;}
public:
 ObservedBoundaryInput(const std::string& source,const std::string& revision,
  const std::string& contour,const std::string& segment,
  const std::vector<std::complex<double>>& parent,const cxgeom::OpenBoundaryRun& run,
  size_t maximum_features=128):source_(source),revision_(revision),contour_(contour),segment_(segment),run_(run){
  auto need=[](bool b,const char* why){if(!b)throw std::invalid_argument(why);};
  need(!source.empty()&&!revision.empty()&&!contour.empty()&&!segment.empty(),"BOUNDARY_MISSING_PROVENANCE");
  need(maximum_features>=2&&maximum_features<=128,"BOUNDARY_INVALID_FEATURE_BUDGET");
  need(parent.size()>=3&&parent.size()<=4096&&run.parent_point_count==parent.size(),"BOUNDARY_PARENT_MISMATCH");
  need(run.points.size()>=2&&run.points.size()<parent.size()&&run.points.size()==run.parent_indices.size(),"BOUNDARY_INVALID_OPEN_RUN");
  need(run.points.size()<=maximum_features,"BOUNDARY_FEATURE_BUDGET");
  for(auto p:parent)need(std::isfinite(p.real())&&std::isfinite(p.imag()),"BOUNDARY_NONFINITE_PARENT");
  size_t wraps=0;
  for(size_t i=0;i<run.points.size();++i){
   const size_t index=run.parent_indices[i];
   need(index<parent.size(),"BOUNDARY_INDEX_OUT_OF_RANGE");
   need(run.points[i]==parent[index],"BOUNDARY_POINT_PARENT_MISMATCH");
   if(i){
    need(index==(run.parent_indices[i-1]+1)%parent.size(),"BOUNDARY_ORDER_MISMATCH");
    if(index==0)++wraps;
    need(std::abs(run.points[i]-run.points[i-1])>1e-9,"BOUNDARY_ZERO_LENGTH_EDGE");
   }
   const auto id=token(source)+token(revision)+token(contour)+std::to_string(index);
   features_.push_back({id,source,run.points[i]});
  }
  need(run.wraps_parent_origin==(wraps==1),"BOUNDARY_WRAP_MISMATCH");
  need(std::abs(run.points.front()-run.points.back())>1e-9,"BOUNDARY_COINCIDENT_ENDPOINTS");
 }
 const std::vector<Feature>& features()const{return features_;}
 const cxgeom::OpenBoundaryRun& ordered_run()const{return run_;}
 std::string receipt()const{
  using receipt_detail::quote;
  std::ostringstream o;o.imbue(std::locale::classic());o<<std::setprecision(17);
  o<<"{\"schema\":\"observed_boundary_input.v1\",\"source_id\":"<<quote(source_);
  o<<",\"dataset_revision\":"<<quote(revision_)<<",\"contour_id\":"<<quote(contour_)<<",\"segment_id\":"<<quote(segment_);
  o<<",\"closed\":false,\"synthetic_count\":0,\"sampling\":\"none\",\"solver_uses_order\":false";
  o<<",\"topology_verified\":false,\"production_eligible\":false,\"parent_point_count\":"<<run_.parent_point_count;
  o<<",\"wraps_parent_origin\":"<<(run_.wraps_parent_origin?"true":"false");
  o<<",\"start_id\":"<<quote(features_.front().stable_id)<<",\"end_id\":"<<quote(features_.back().stable_id);
  o<<",\"ordered_points\":[";
  for(size_t i=0;i<features_.size();++i){
   if(i)o<<',';
   o<<"{\"id\":"<<quote(features_[i].stable_id)<<",\"parent_index\":"<<run_.parent_indices[i];
   o<<",\"x\":"<<features_[i].point.real()<<",\"y\":"<<features_[i].point.imag()<<",\"observed\":true}";
  }
  o<<"]}";return o.str();
 }
};
}
