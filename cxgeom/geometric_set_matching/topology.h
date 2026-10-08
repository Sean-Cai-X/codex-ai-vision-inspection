#pragma once
#include "types.h"

namespace cxgeom::gsm {
enum class Branch { ClosedContour, OpenCurve, GeometricSet };
enum class Representation { OrderedChain, UnorderedPoints, SegmentCollection };
enum class Topology { Unverified, ClosedSingle, OpenSingle, MultiSegment, ScatteredPoints };
struct TopologyInput {
    Branch branch=Branch::ClosedContour;
    Representation representation=Representation::OrderedChain;
    std::vector<std::vector<Point2>> parts;
    std::string source_ref;
    std::string order_evidence_ref;
    bool caller_confirmed=false;
    bool closure_edge_observed=false; // Evidence assertion, NOT inferred from endpoint distance.
    bool complete=false;
    unsigned holes=0;
};
struct TopologyPolicy {
    double epsilon_px=1e-6;
    double maximum_edge_length_px=1e9;
    std::size_t maximum_points=4096;
    std::size_t maximum_pair_checks=1000000;
};
struct TopologyReport {
    bool accepted=false;
    bool physical_boundary_verified=false; // Coordinates alone cannot prove source semantics.
    Topology topology=Topology::Unverified;
    ExecutionStatus execution_status=ExecutionStatus::InvalidInput;
    std::string source_ref, order_evidence_ref;
    std::vector<std::string> reasons;
    std::size_t point_count=0, pair_checks=0;
};
class ContourTopologyValidator {
public:
    static TopologyReport Check(const TopologyInput&, const TopologyPolicy& = {});
};
const char* Name(Topology);
}
