#pragma once
#include "partial_contract.h"
namespace cxgeom::polar {
struct Hypothesis {
 std::string source_id; // Caller provenance, never an inferred solver result.
 double angle_deg=0,scale=1;
 std::complex<double> translation;
};
struct PartialAssessment {
 std::string request_id,status,reason;
 Hypothesis supplied;
 PartialConfig executed;
 std::vector<Pair> pairs;
 std::vector<std::string> missing_reference_ids,extra_observation_ids;
 std::optional<double> reference_coverage,observation_coverage,reference_span,observed_rms_px,observed_max_px;
 size_t pair_checks=0;
 bool assessment_complete=false,accepted=false,production_eligible=false;
};
// Tests a supplied transform; does NOT search for or estimate a pose.
// Coverage is point-count fraction. Span is matched/reference diameter ratio.
// Ambiguous threshold correspondences are conservatively withheld.
PartialAssessment AssessHypothesis(const PartialRequest&,const Hypothesis&);
}
