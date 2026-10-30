#pragma once
#include <complex>
#include <string>
#include <vector>
#include <set>
#include <cmath>
#include <stdexcept>
namespace cxgeom::polar {
enum class BoundaryEdgeOrigin { Observed, Synthetic };
struct BoundaryEvidencePoint {
 std::string source_id;
 std::complex<double> original_point;
};
struct BoundaryEvidenceEdge {
 size_t from=0,to=0;
 BoundaryEdgeOrigin origin=BoundaryEdgeOrigin::Observed;
 std::string provenance; // measured adjacency or explicit completion prior revision
};
struct BoundaryEvidence {
 std::vector<BoundaryEvidencePoint> points;
 std::vector<BoundaryEvidenceEdge> edges;
 bool closed=false;
 std::string source_revision,completion_prior_revision;
};
struct BoundaryEvidenceAudit {
 double observed_length=0,synthetic_length=0;
 std::vector<size_t> measurement_edges; // ONLY observed edges; not a closed DFT subset
 bool pose_aided_by_completion=false,production_eligible=false;
};
// Validates caller-declared ordering/closure. Never joins endpoints automatically.
// Edge tags (not just point tags) prevent a synthetic closing edge masquerading
// as measured support when both endpoints happen to be observed.
inline BoundaryEvidenceAudit AuditBoundaryEvidence(const BoundaryEvidence& b){
 if(b.source_revision.empty()||b.points.size()<2||b.edges.size()!=(b.closed?b.points.size():b.points.size()-1))
  throw std::invalid_argument("INVALID_BOUNDARY_TOPOLOGY");
 std::set<std::string> ids;
 for(const auto& p:b.points)if(p.source_id.empty()||!ids.insert(p.source_id).second||
   !std::isfinite(p.original_point.real())||!std::isfinite(p.original_point.imag()))throw std::invalid_argument("INVALID_BOUNDARY_POINT");
 BoundaryEvidenceAudit out;
 for(size_t i=0;i<b.edges.size();++i){const auto& e=b.edges[i];
  if(e.from!=i||e.to!=(i+1)%b.points.size()||e.provenance.empty())throw std::invalid_argument("INVALID_BOUNDARY_EDGE_PROVENANCE");
  const double length=std::abs(b.points[e.to].original_point-b.points[e.from].original_point);
  if(!std::isfinite(length)||length<=0)throw std::invalid_argument("DEGENERATE_BOUNDARY_EDGE");
  if(e.origin==BoundaryEdgeOrigin::Observed){out.observed_length+=length;out.measurement_edges.push_back(i);}
  else if(e.origin==BoundaryEdgeOrigin::Synthetic){
   if(b.completion_prior_revision.empty())throw std::invalid_argument("COMPLETION_PRIOR_REQUIRED");
   out.synthetic_length+=length;out.pose_aided_by_completion=true;
  }else throw std::invalid_argument("INVALID_BOUNDARY_ORIGIN");
 }
 if(out.observed_length<=0)throw std::invalid_argument("NO_OBSERVED_SUPPORT");
 return out;
}
}
