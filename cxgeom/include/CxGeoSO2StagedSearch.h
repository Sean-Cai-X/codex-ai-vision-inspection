#pragma once
#include "CxGeoSO2Harmonic.h"
#include <cstddef>
namespace cxgeom { namespace so2 {
// Shift is contour parameter phase, NOT a physical angle-search step.
// Coarse peaks prioritize refinement; the fine grid always covers a complete turn.
struct StagedConfig {
    int coarse_order=4, fine_order=12;
    int coarse_samples=64, fine_samples=256;
    int refinement_iterations=32;
    std::size_t maximum_evaluations=65536;
    bool capture_response=false;
};
struct ResponseSample {
    double shift_turns=0, angle_deg=0, correlation=0, residual=0;
};
struct StagedResult {
    Result match;
    bool search_complete=false;
    bool continuous_global_optimum_certified=false;
    std::size_t evaluations=0, coarse_peaks=0, fine_peaks=0;
    StagedConfig executed;
    std::vector<ResponseSample> coarse_response, fine_response;
};
// Additive API. Existing Match and persisted descriptor configuration are unchanged.
// Invalid configuration throws. Budget exhaustion returns NO accepted pose.
StagedResult MatchStaged(const Descriptor&,const Descriptor&,const StagedConfig& = {});
}}
