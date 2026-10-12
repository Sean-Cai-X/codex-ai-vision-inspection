#pragma once
#include "observed_boundary_input.h"
#include <map>
#include <set>
namespace cxgeom::polar {
// A bundle shares ONE image coordinate frame and revision. Segments stay separate.
// Deduplication uses stable source point identity, never coordinate proximity.
class ObservedBoundaryBundle {
 std::vector<ObservedBoundaryInput> segments_;
 std::vector<Feature> features_;
 std::vector<std::vector<size_t>> segment_indices_;
 size_t occurrences_=0;
public:
 explicit ObservedBoundaryBundle(std::vector<ObservedBoundaryInput> segments,size_t maximum_features=128){
  auto need=[](bool b,const char* why){if(!b)throw std::invalid_argument(why);};
  need(!segments.empty()&&segments.size()<=32,"BOUNDARY_BUNDLE_SEGMENT_BUDGET");
  need(maximum_features>=2&&maximum_features<=128,"BOUNDARY_BUNDLE_INVALID_FEATURE_BUDGET");
  const auto source=segments.front().source_id(),revision=segments.front().revision_id();
  std::set<std::pair<std::string,std::string>> segment_ids;
  std::map<std::string,std::vector<std::complex<double>>> parents;
  std::map<std::string,Feature> unique;
  for(const auto& s:segments){
   need(s.source_id()==source&&s.revision_id()==revision,"BOUNDARY_BUNDLE_MIXED_FRAME");
   need(segment_ids.emplace(s.contour_id(),s.segment_id()).second,"BOUNDARY_BUNDLE_DUPLICATE_SEGMENT");
   auto p=parents.emplace(s.contour_id(),s.parent_points());
   need(p.second||p.first->second==s.parent_points(),"BOUNDARY_BUNDLE_PARENT_CONFLICT");
   for(const auto& f:s.features()){
    ++occurrences_;
    const auto entry=unique.emplace(f.stable_id,f);
    need(entry.second||entry.first->second.point==f.point,"BOUNDARY_BUNDLE_POINT_CONFLICT");
   }
  }
  need(unique.size()<=maximum_features,"BOUNDARY_BUNDLE_FEATURE_BUDGET");
  std::map<std::string,size_t> indices;
  for(const auto& kv:unique){indices.emplace(kv.first,features_.size());features_.push_back(kv.second);}
  for(const auto& s:segments){
   std::vector<size_t> mapping;
   for(const auto& f:s.features())mapping.push_back(indices.at(f.stable_id));
   segment_indices_.push_back(std::move(mapping));
  }
  segments_=std::move(segments);
 }
 const std::vector<Feature>& features()const{return features_;}
 const std::vector<ObservedBoundaryInput>& segments()const{return segments_;}
 const std::vector<std::vector<size_t>>& segment_indices()const{return segment_indices_;}
 std::string receipt()const{
  std::ostringstream o;o.imbue(std::locale::classic());
  o<<"{\"schema\":\"observed_boundary_bundle.v1\",\"solver_uses_order\":false";
  o<<",\"topology_verified\":false,\"production_eligible\":false,\"connect_segments\":false";
  o<<",\"source_point_occurrences\":"<<occurrences_<<",\"unique_feature_count\":"<<features_.size();
  o<<",\"deduplicated_occurrences\":"<<occurrences_-features_.size()<<",\"segments\":[";
  for(size_t i=0;i<segments_.size();++i){
   if(i)o<<',';
   o<<"{\"input\":"<<segments_[i].receipt()<<",\"feature_indices\":[";
   for(size_t k=0;k<segment_indices_[i].size();++k){if(k)o<<',';o<<segment_indices_[i][k];}
   o<<"]}";
  }
  o<<"],\"feature_ids\":[";
  for(size_t i=0;i<features_.size();++i){if(i)o<<',';o<<receipt_detail::quote(features_[i].stable_id);}
  o<<"]}";return o.str();
 }
};
}
