#pragma once
#include "observed_boundary_input.h"
#include <algorithm>
namespace cxgeom::polar {
struct ObservedContextConfig { double margin_px=8,maximum_edge_px=1.5;size_t maximum_points=128; };
struct ObservedContextExtension {
 ObservedBoundaryInput context;
 size_t original_offset=0;
 double before_length_px=0,after_length_px=0;
};
inline ObservedContextExtension ExtendObservedContext(const ObservedBoundaryInput& input,
 const ObservedContextConfig& c={},const std::vector<size_t>& forbidden_parent_indices={}){
 if(!std::isfinite(c.margin_px)||c.margin_px<=0||!std::isfinite(c.maximum_edge_px)||c.maximum_edge_px<=0||
    c.maximum_points<3||c.maximum_points>128)throw std::invalid_argument("CONTEXT_INVALID_CONFIG");
 const auto& parent=input.parent_points();const auto& original=input.ordered_run();
 auto indices=original.parent_indices;size_t offset=0;double before=0,after=0;
 auto grow=[&](bool front,double& length){
  while(length<c.margin_px){
   if(indices.size()>=c.maximum_points||indices.size()+1>=parent.size())throw std::invalid_argument("CONTEXT_BUDGET_OR_CLOSURE");
   size_t current=front?indices.front():indices.back();size_t next=(current+(front?parent.size()-1:1))%parent.size();
   if(std::find(indices.begin(),indices.end(),next)!=indices.end())throw std::invalid_argument("CONTEXT_REVISIT");
   if(std::find(forbidden_parent_indices.begin(),forbidden_parent_indices.end(),next)!=forbidden_parent_indices.end())
    throw std::invalid_argument("CONTEXT_FORBIDDEN_SOURCE");
   double edge=std::abs(parent[next]-parent[current]);
   if(!std::isfinite(edge)||edge<=1e-9||edge>c.maximum_edge_px)throw std::invalid_argument("CONTEXT_GAP");
   if(front){indices.insert(indices.begin(),next);++offset;}else indices.push_back(next);length+=edge;
  }
 };
 grow(true,before);grow(false,after);
 cxgeom::OpenBoundaryRun run;run.parent_point_count=parent.size();run.parent_indices=indices;
 for(size_t i=0;i<indices.size();++i){run.points.push_back(parent[indices[i]]);if(i&&indices[i]==0)run.wraps_parent_origin=true;}
 ObservedBoundaryInput context(input.source_id(),input.revision_id(),input.contour_id(),input.segment_id()+":context",parent,run,c.maximum_points);
 for(size_t i=0;i<original.points.size();++i)
  if(context.features().at(offset+i).stable_id!=input.features().at(i).stable_id||run.points[offset+i]!=original.points[i])
   throw std::logic_error("CONTEXT_ORIGINAL_CHANGED");
 return {std::move(context),offset,before,after};
}
}
