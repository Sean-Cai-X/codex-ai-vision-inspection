#pragma once
#include "partial_assessment.h"
namespace cxgeom::polar {
struct PartialSearchConfig {
 double scale_min=.5,scale_max=2,angle_min_deg=-180,angle_max_deg=180;
 double minimum_seed_length_px=1e-6;
 double merge_angle_deg=1e-8,merge_scale=1e-10,merge_translation_px=1e-7;
 size_t maximum_candidates=32;
 bool refine_before_capacity=false;
};
struct PartialSearchResult {
 std::string request_id,status,reason,reference_source,observation_source;
 struct Deduplication { std::string suppressed_seed,representative_seed; };
 std::vector<Deduplication> deduplication; // Bounded by maximum_hypotheses.
 std::string generator="deterministic_geometric_point_pairs";
 PartialConfig executed_assessment;
 PartialSearchConfig executed_search;
 std::vector<PartialAssessment> candidates;
 struct OnlineRefit { PartialAssessment before,after; std::string decision; size_t pair_checks=0; };
 std::vector<OnlineRefit> online_refits;
 size_t attempted_seeds=0,assessed_hypotheses=0,pair_checks=0;
 bool search_complete=false,production_eligible=false;
};
// Geometric fallback candidate generator, NOT a polar-phase partial solver.
// Enumerates finite point-pair seeds. No continuous/global optimum certification.
// On any exhausted budget candidates are cleared; no partial search acceptance.
PartialSearchResult SearchPartial(const PartialRequest&,const PartialSearchConfig& = {});
}
