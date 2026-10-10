#pragma once
#include "encoding.h"
namespace cxgeom::polar {
struct MatchConfig {
 Config encoding;
 double scale_min=.5, scale_max=2;
 // Degrees in [-180,180]; min>max selects a window crossing the +/-180 seam.
 double angle_min_deg=-180,angle_max_deg=180;
 double max_residual_px=.3, max_spectral_distance=.05;
 double min_phase_amplitude=1e-6;
 size_t maximum_candidates=64, maximum_pair_checks=1000000;
};
struct Pair {std::string reference_id,target_id;double residual_px=0;};
struct PoseCandidate {
 double angle_deg=0,scale=1,rms_px=0,max_px=0,spectral_distance=0;
 std::complex<double> translation;
 std::vector<Pair> pairs;
 bool correspondence_ambiguous=false;
};
struct MatchResult {
 std::string status,reason,reference_source,target_source;
 MatchConfig executed;
 std::vector<PoseCandidate> candidates;
 size_t evaluated_candidates=0,pair_checks=0;
 size_t angle_rejected=0;
 int phase_order=0,phase_channel=0;
 bool search_complete=false,production_eligible=false;
};
// Complete equal-cardinality point sets with uniform weights only.
// Phase roots generate candidates; original coordinates decide acceptance.
// Not a partial/outlier matcher, continuous optimizer, or production admission.
MatchResult MatchFull(const std::string&,const std::vector<Feature>&,
                      const std::string&,const std::vector<Feature>&,const MatchConfig& = {});
}
