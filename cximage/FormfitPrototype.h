#ifndef CXCORE_CORE_FORMFITPROTOTYPE_H
#define CXCORE_CORE_FORMFITPROTOTYPE_H

#include "CxCoreBoundary.h"
#include "FormfitGauge.h"

namespace cxcore
{
namespace formfit
{

struct PrototypeCandidateScore
{
    int candidate_index = -1;
    double evaluation_score = 0.0;
    int rank_index = -1;
    bool valid = false;
};

struct PrototypeRunResult
{
    FormfitGauge gauge;
    FitTaskSpec task;
    FitProblem problem;
    FitProblem configured_problem;
    FitMethodDescriptor method;
    FitMethodResult method_result;
    FitCandidateGroup candidate_group;
    FitOperationConfig operation_config;
    double group_score = 0.0;
    double evaluation_score = 0.0;
    int selected_match_candidate_index = -1;
    int best_match_candidate_index = -1;
    int evaluated_match_candidate_count = 0;
    std::vector<PrototypeCandidateScore> candidate_scores;
    std::vector<int> ordered_match_candidate_indices;
};

FitOperationConfig MakeRectCircleCompositeOperationConfig();
FitOperationConfig MakeRectCircleLineCompositeOperationConfig();
FitOperationConfig MakeRectCircleLineMatchCompositeOperationConfig();
FitOperationConfig MakeCircleRingCompositeOperationConfig();
FitOperationConfig MakeCircleRingLineCompositeOperationConfig();

PrototypeRunResult RunRectCirclePrototype(const OutputRect& rect,
                                          const CircleMeasurementOutput& circle,
                                          const FitOperationConfig& operation_config);
PrototypeRunResult RunRectCircleLinePrototype(const OutputRect& rect,
                                              const CircleMeasurementOutput& circle,
                                              const LineMeasurementOutput& line,
                                              const FitOperationConfig& operation_config);
PrototypeRunResult RunRectCircleLineMatchPrototype(const OutputRect& rect,
                                                   const CircleMeasurementOutput& circle,
                                                   const LineMeasurementOutput& line,
                                                   const MatchOutput& match,
                                                   const FitOperationConfig& operation_config);
PrototypeRunResult RunCircleRingPrototype(const CircleMeasurementOutput& outer_circle,
                                          const CircleMeasurementOutput& inner_circle,
                                          const FitOperationConfig& operation_config,
                                          double center_tolerance = 3.0,
                                          double thickness_tolerance = 5.0);
PrototypeRunResult RunCircleRingLinePrototype(const CircleMeasurementOutput& outer_circle,
                                              const CircleMeasurementOutput& inner_circle,
                                              const LineMeasurementOutput& line,
                                              const FitOperationConfig& operation_config,
                                              double center_tolerance = 3.0,
                                              double thickness_tolerance = 5.0);

} // namespace formfit
} // namespace cxcore

#endif
