#include "pch.h"

#include "FormfitPrototype.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <vector>

namespace cxcore
{
namespace formfit
{

namespace
{
double PositiveOr(double value, double fallback)
{
    return std::isfinite(value) && value > 0.0 ? value : fallback;
}

double NonNegativeOr(double value, double fallback = 0.0)
{
    return std::isfinite(value) && value >= 0.0 ? value : fallback;
}

double WorstScore()
{
    return std::numeric_limits<double>::max();
}

double StageScore(double cost, double max_residual)
{
    const double score = NonNegativeOr(cost) + NonNegativeOr(max_residual);
    return std::isfinite(score) ? score : WorstScore();
}

bool IsBetterScore(double lhs, double rhs)
{
    const bool lhs_valid = std::isfinite(lhs);
    const bool rhs_valid = std::isfinite(rhs);
    if (lhs_valid != rhs_valid)
    {
        return lhs_valid;
    }
    return lhs < rhs;
}

double SafeRankDelta(double next_score, double best_score)
{
    return std::isfinite(next_score) && std::isfinite(best_score) && next_score >= best_score ?
        next_score - best_score : 0.0;
}

void FinalizeCandidateGroup(FitCandidateGroup& group, const FitMethodResult& method_result)
{
    group.candidate_count = static_cast<int>(group.candidates.size());
    group.ranked_candidate_count = static_cast<int>(group.baseline_rank.size());

    group.ordered_candidate_ids.clear();
    group.ordered_candidate_ids.reserve(group.baseline_rank.size());
    for (const FitCandidateRankItem& rank_item : group.baseline_rank)
    {
        group.ordered_candidate_ids.push_back(rank_item.candidate_id);
    }
    group.ordered_candidate_count = static_cast<int>(group.ordered_candidate_ids.size());

    int total_iterations = 0;
    for (const FitCandidate& candidate : group.candidates)
    {
        total_iterations += std::max(0, candidate.iterations);
    }
    group.fixed_point_iterations = method_result.fit_result.iterations > 0 ?
        method_result.fit_result.iterations : total_iterations;
    group.fixed_point_delta = NonNegativeOr(group.rank_delta);
    group.fixed_point_converged = method_result.valid &&
        !group.best_candidate_id.empty() &&
        std::isfinite(group.group_score);
    group.convergence_status = group.fixed_point_converged ?
        "converged" : (group.candidates.empty() ? "no_candidates" : "not_converged");
}

FitOperationConfig NormalizeOperationConfig(FitOperationConfig config)
{
    if (config.has_global_tolerance_scale)
    {
        config.global_tolerance_scale = PositiveOr(config.global_tolerance_scale, 1.0);
    }
    if (config.has_global_weight_scale)
    {
        config.global_weight_scale = PositiveOr(config.global_weight_scale, 1.0);
    }
    for (FitStageSetting& stage : config.stage_settings)
    {
        if (stage.has_stage_weight)
        {
            stage.stage_weight = PositiveOr(stage.stage_weight, 1.0);
        }
        if (stage.has_tolerance_scale)
        {
            stage.tolerance_scale = PositiveOr(stage.tolerance_scale, 1.0);
        }
        if (stage.has_max_iterations)
        {
            stage.max_iterations = std::max(0, stage.max_iterations);
        }
    }
    for (FitResidualSetting& residual : config.residual_settings)
    {
        if (residual.has_weight)
        {
            residual.weight = PositiveOr(residual.weight, 1.0);
        }
        if (residual.has_tolerance)
        {
            residual.tolerance = PositiveOr(residual.tolerance, 1.0);
        }
    }
    for (FitVariableSetting& variable : config.variable_settings)
    {
        if (variable.has_weight)
        {
            variable.weight = PositiveOr(variable.weight, 1.0);
        }
    }
    return config;
}
} // namespace

FitOperationConfig MakeRectCircleCompositeOperationConfig()
{
    FitOperationConfig config;
    config.config_id = "rect_circle_composite_config";
    config.operation_kind = FitOperationKind::SetStage;
    config.has_global_tolerance_scale = true;
    config.global_tolerance_scale = 1.0;
    config.has_global_weight_scale = true;
    config.global_weight_scale = 1.0;

    FitStageSetting coarse;
    coarse.stage_id = "coarse";
    coarse.has_method = true;
    coarse.method_type = FitMethodType::LeastSquares;
    coarse.has_order_index = true;
    coarse.order_index = 0;
    coarse.has_stage_weight = true;
    coarse.stage_weight = 0.75;
    coarse.has_tolerance_scale = true;
    coarse.tolerance_scale = 1.20;
    coarse.has_max_iterations = true;
    coarse.max_iterations = 16;
    config.stage_settings.push_back(coarse);

    FitStageSetting refine;
    refine.stage_id = "refine";
    refine.has_method = true;
    refine.method_type = FitMethodType::WeightedLeastSquares;
    refine.has_order_index = true;
    refine.order_index = 1;
    refine.has_stage_weight = true;
    refine.stage_weight = 1.00;
    refine.has_tolerance_scale = true;
    refine.tolerance_scale = 1.00;
    refine.has_max_iterations = true;
    refine.max_iterations = 32;
    config.stage_settings.push_back(refine);

    FitStageSetting final_stage;
    final_stage.stage_id = "final";
    final_stage.has_method = true;
    final_stage.method_type = FitMethodType::Constrained;
    final_stage.has_order_index = true;
    final_stage.order_index = 2;
    final_stage.has_stage_weight = true;
    final_stage.stage_weight = 1.25;
    final_stage.has_tolerance_scale = true;
    final_stage.tolerance_scale = 0.85;
    final_stage.has_max_iterations = true;
    final_stage.max_iterations = 24;
    config.stage_settings.push_back(final_stage);

    return NormalizeOperationConfig(config);
}

FitOperationConfig MakeRectCircleLineCompositeOperationConfig()
{
    FitOperationConfig config = MakeRectCircleCompositeOperationConfig();
    config.config_id = "rect_circle_line_composite_config";
    config.has_global_weight_scale = true;
    config.global_weight_scale = 1.1;
    return NormalizeOperationConfig(config);
}

FitOperationConfig MakeRectCircleLineMatchCompositeOperationConfig()
{
    FitOperationConfig config = MakeRectCircleLineCompositeOperationConfig();
    config.config_id = "rect_circle_line_match_composite_config";
    config.has_global_tolerance_scale = true;
    config.global_tolerance_scale = 0.9;
    config.has_global_weight_scale = true;
    config.global_weight_scale = 1.2;

    FitStageSetting refine_stage;
    refine_stage.stage_id = "refine";
    refine_stage.has_method = true;
    refine_stage.method_type = FitMethodType::Robust;
    refine_stage.has_tolerance_scale = true;
    refine_stage.tolerance_scale = 0.85;
    refine_stage.has_stage_weight = true;
    refine_stage.stage_weight = 1.15;
    config.stage_settings.push_back(refine_stage);

    FitStageSetting final_stage;
    final_stage.stage_id = "final";
    final_stage.has_method = true;
    final_stage.method_type = FitMethodType::Constrained;
    final_stage.has_max_iterations = true;
    final_stage.max_iterations = 48;
    final_stage.has_stage_weight = true;
    final_stage.stage_weight = 1.35;
    config.stage_settings.push_back(final_stage);

    FitResidualSetting match_center_distance;
    match_center_distance.residual_name = "rect_match_center_distance";
    match_center_distance.has_weight = true;
    match_center_distance.weight = 1.5;
    match_center_distance.has_tolerance = true;
    match_center_distance.tolerance = 3.5;
    config.residual_settings.push_back(match_center_distance);

    FitResidualSetting match_width;
    match_width.residual_name = "match_rect_width";
    match_width.has_weight = true;
    match_width.weight = 1.1;
    match_width.has_tolerance = true;
    match_width.tolerance = 3.0;
    config.residual_settings.push_back(match_width);

    FitResidualSetting match_height;
    match_height.residual_name = "match_rect_height";
    match_height.has_weight = true;
    match_height.weight = 1.1;
    match_height.has_tolerance = true;
    match_height.tolerance = 3.0;
    config.residual_settings.push_back(match_height);

    FitVariableSetting rect_rotation;
    rect_rotation.variable_name = "rect.rotation";
    rect_rotation.has_locked = true;
    rect_rotation.locked = true;
    rect_rotation.has_weight = true;
    rect_rotation.weight = 0.25;
    config.variable_settings.push_back(rect_rotation);

    FitVariableSetting circle_radius;
    circle_radius.variable_name = "circle.radius";
    circle_radius.has_weight = true;
    circle_radius.weight = 1.4;
    config.variable_settings.push_back(circle_radius);

    return NormalizeOperationConfig(config);
}

FitOperationConfig MakeCircleRingLineCompositeOperationConfig()
{
    FitOperationConfig config = MakeCircleRingCompositeOperationConfig();
    config.config_id = "circle_ring_line_composite_config";
    config.has_global_weight_scale = true;
    config.global_weight_scale = 1.1;
    return NormalizeOperationConfig(config);
}

FitOperationConfig MakeCircleRingCompositeOperationConfig()
{
    FitOperationConfig config;

    config.config_id = "circle_ring_composite_config";
    config.operation_kind = FitOperationKind::SetStage;
    config.has_global_tolerance_scale = true;
    config.global_tolerance_scale = 1.0;
    config.has_global_weight_scale = true;
    config.global_weight_scale = 1.0;

    FitStageSetting coarse;
    coarse.stage_id = "coarse";
    coarse.has_method = true;
    coarse.method_type = FitMethodType::LeastSquares;
    coarse.has_order_index = true;
    coarse.order_index = 0;
    coarse.has_stage_weight = true;
    coarse.stage_weight = 0.75;
    coarse.has_tolerance_scale = true;
    coarse.tolerance_scale = 1.20;
    coarse.has_max_iterations = true;
    coarse.max_iterations = 16;
    config.stage_settings.push_back(coarse);

    FitStageSetting refine;
    refine.stage_id = "refine";
    refine.has_method = true;
    refine.method_type = FitMethodType::WeightedLeastSquares;
    refine.has_order_index = true;
    refine.order_index = 1;
    refine.has_stage_weight = true;
    refine.stage_weight = 1.00;
    refine.has_tolerance_scale = true;
    refine.tolerance_scale = 1.00;
    refine.has_max_iterations = true;
    refine.max_iterations = 32;
    config.stage_settings.push_back(refine);

    FitStageSetting final_stage;
    final_stage.stage_id = "final";
    final_stage.has_method = true;
    final_stage.method_type = FitMethodType::Constrained;
    final_stage.has_order_index = true;
    final_stage.order_index = 2;
    final_stage.has_stage_weight = true;
    final_stage.stage_weight = 1.25;
    final_stage.has_tolerance_scale = true;
    final_stage.tolerance_scale = 0.85;
    final_stage.has_max_iterations = true;
    final_stage.max_iterations = 24;
    config.stage_settings.push_back(final_stage);

    return NormalizeOperationConfig(config);
}

PrototypeRunResult RunRectCirclePrototype(const OutputRect& rect,
                                          const CircleMeasurementOutput& circle,
                                          const FitOperationConfig& operation_config)
{
    PrototypeRunResult result;
    result.evaluation_score = WorstScore();
    result.gauge = MakeRectCircleGauge(rect, circle, "rect_circle_gauge", "rect_circle_gauge");
    result.task = MakeTaskSpecFromGauge(result.gauge, "rect_circle_task", FitTaskType::GaugeFit);
    result.problem = MakeFitProblemFromGauge(result.gauge, "rect_circle_problem");
    result.configured_problem = result.problem;
    result.method = MakeFitMethodDescriptor(
        "rect_circle_composite",
        FitMethodCategory::CompositeFamily,
        FitMethodType::Constrained);
    result.operation_config = operation_config;
    FitOperationController::Apply(result.operation_config, result.configured_problem);

    std::shared_ptr<IOptimizerBackend> backend = std::make_shared<LinearizedLeastSquaresOptimizerBackend>();
    std::shared_ptr<ICompositeFitter> fitter = std::make_shared<CompositeFitter>(backend);
    DefaultMethodExecutor executor(fitter);
    result.method_result = executor.Execute(result.task, result.method, result.operation_config, result.problem);
    
    result.candidate_group.group_id = "rect_circle_candidates";
    result.candidate_group.task_id = result.task.task_id;
    result.candidate_group.problem_id = result.problem.problem_id;
    result.candidate_group.config_id = result.operation_config.config_id;

    if (!result.method_result.fit_result.stage_summaries.empty())
    {
        for (const FitStageSummary& summary : result.method_result.fit_result.stage_summaries)
        {
            FitCandidate candidate;
            candidate.candidate_id = summary.stage_id;
            candidate.source_stage_id = summary.stage_id;
            candidate.source_method_type = summary.method_type;
            candidate.valid = result.method_result.valid;
            candidate.final_cost = NonNegativeOr(summary.stage_cost);
            candidate.max_residual = NonNegativeOr(summary.stage_max_residual);
            candidate.iterations = summary.iterations;
            candidate.elapsed_ms = result.method_result.elapsed_ms;
            result.candidate_group.candidates.push_back(candidate);
        }

        std::vector<FitCandidateRankItem> ranking;
        ranking.reserve(result.candidate_group.candidates.size());
        for (const FitCandidate& candidate : result.candidate_group.candidates)
        {
            FitCandidateRankItem item;
            item.candidate_id = candidate.candidate_id;
            item.score = StageScore(candidate.final_cost, candidate.max_residual);
            ranking.push_back(item);
        }

        std::stable_sort(ranking.begin(), ranking.end(),
                         [](const FitCandidateRankItem& lhs, const FitCandidateRankItem& rhs)
                         {
                             return IsBetterScore(lhs.score, rhs.score);
                         });

        for (size_t i = 0; i < ranking.size(); ++i)
        {
            ranking[i].rank_index = static_cast<int>(i);
            result.candidate_group.baseline_rank.push_back(ranking[i]);
        }

        if (!ranking.empty())
        {
            result.candidate_group.best_candidate_id = ranking.front().candidate_id;
            result.candidate_group.group_score = ranking.front().score;
            result.candidate_group.rank_delta =
                ranking.size() > 1 ? SafeRankDelta(ranking[1].score, ranking.front().score) : 0.0;
            result.group_score = result.candidate_group.group_score;
            result.evaluation_score = result.group_score;
        }
    }
    FinalizeCandidateGroup(result.candidate_group, result.method_result);
    return result;
}

PrototypeRunResult RunRectCircleLinePrototype(const OutputRect& rect,
                                              const CircleMeasurementOutput& circle,
                                              const LineMeasurementOutput& line,
                                              const FitOperationConfig& operation_config)
{
    PrototypeRunResult result;
    result.evaluation_score = WorstScore();
    result.gauge = MakeRectCircleLineGauge(rect, circle, line, "rect_circle_line_gauge", "rect_circle_line_gauge");
    result.task = MakeTaskSpecFromGauge(result.gauge, "rect_circle_line_task", FitTaskType::StructureFit);
    result.problem = MakeFitProblemFromGauge(result.gauge, "rect_circle_line_problem");
    result.configured_problem = result.problem;
    result.method = MakeFitMethodDescriptor(
        "rect_circle_line_composite",
        FitMethodCategory::CompositeFamily,
        FitMethodType::Constrained);
    result.operation_config = operation_config;
    FitOperationController::Apply(result.operation_config, result.configured_problem);

    std::shared_ptr<IOptimizerBackend> backend = std::make_shared<LinearizedLeastSquaresOptimizerBackend>();
    std::shared_ptr<ICompositeFitter> fitter = std::make_shared<CompositeFitter>(backend);
    DefaultMethodExecutor executor(fitter);
    result.method_result = executor.Execute(result.task, result.method, result.operation_config, result.problem);
    
    result.candidate_group.group_id = "rect_circle_line_candidates";
    result.candidate_group.task_id = result.task.task_id;
    result.candidate_group.problem_id = result.problem.problem_id;
    result.candidate_group.config_id = result.operation_config.config_id;

    if (!result.method_result.fit_result.stage_summaries.empty())
    {
        for (const FitStageSummary& summary : result.method_result.fit_result.stage_summaries)
        {
            FitCandidate candidate;
            candidate.candidate_id = summary.stage_id;
            candidate.source_stage_id = summary.stage_id;
            candidate.source_method_type = summary.method_type;
            candidate.valid = result.method_result.valid;
            candidate.final_cost = NonNegativeOr(summary.stage_cost);
            candidate.max_residual = NonNegativeOr(summary.stage_max_residual);
            candidate.iterations = summary.iterations;
            candidate.elapsed_ms = result.method_result.elapsed_ms;
            result.candidate_group.candidates.push_back(candidate);
        }

        std::vector<FitCandidateRankItem> ranking;
        ranking.reserve(result.candidate_group.candidates.size());
        for (const FitCandidate& candidate : result.candidate_group.candidates)
        {
            FitCandidateRankItem item;
            item.candidate_id = candidate.candidate_id;
            item.score = StageScore(candidate.final_cost, candidate.max_residual);
            ranking.push_back(item);
        }

        std::stable_sort(ranking.begin(), ranking.end(),
                         [](const FitCandidateRankItem& lhs, const FitCandidateRankItem& rhs)
                         {
                             return IsBetterScore(lhs.score, rhs.score);
                         });

        for (size_t i = 0; i < ranking.size(); ++i)
        {
            ranking[i].rank_index = static_cast<int>(i);
            result.candidate_group.baseline_rank.push_back(ranking[i]);
        }

        if (!ranking.empty())
        {
            result.candidate_group.best_candidate_id = ranking.front().candidate_id;
            result.candidate_group.group_score = ranking.front().score;
            result.candidate_group.rank_delta =
                ranking.size() > 1 ? SafeRankDelta(ranking[1].score, ranking.front().score) : 0.0;
            result.group_score = result.candidate_group.group_score;
            result.evaluation_score = result.group_score;
        }
    }
    FinalizeCandidateGroup(result.candidate_group, result.method_result);
    return result;
}

PrototypeRunResult RunRectCircleLineMatchPrototype(const OutputRect& rect,
                                                   const CircleMeasurementOutput& circle,
                                                   const LineMeasurementOutput& line,
                                                   const MatchOutput& match,
                                                   const FitOperationConfig& operation_config)
{
    auto run_single = [&](const MatchOutput& single_match, int candidate_index) -> PrototypeRunResult
    {
        PrototypeRunResult result;
    result.evaluation_score = WorstScore();
        result.gauge = MakeRectCircleLineMatchGauge(rect, circle, line, single_match, "rect_circle_line_match_gauge", "rect_circle_line_match_gauge");
        result.task = MakeTaskSpecFromGauge(result.gauge, "rect_circle_line_match_task", FitTaskType::ComparativeFit);
        result.problem = MakeFitProblemFromGauge(result.gauge, "rect_circle_line_match_problem");
        result.configured_problem = result.problem;
        result.method = MakeFitMethodDescriptor(
            "rect_circle_line_match_composite",
            FitMethodCategory::CompositeFamily,
            FitMethodType::Constrained);
        result.operation_config = operation_config;
        FitOperationController::Apply(result.operation_config, result.configured_problem);

        std::shared_ptr<IOptimizerBackend> backend = std::make_shared<LinearizedLeastSquaresOptimizerBackend>();
        std::shared_ptr<ICompositeFitter> fitter = std::make_shared<CompositeFitter>(backend);
        DefaultMethodExecutor executor(fitter);
        result.method_result = executor.Execute(result.task, result.method, result.operation_config, result.problem);
        
    result.candidate_group.group_id = "rect_circle_line_match_candidates";
    result.candidate_group.task_id = result.task.task_id;
    result.candidate_group.problem_id = result.problem.problem_id;
    result.candidate_group.config_id = result.operation_config.config_id;

    if (!result.method_result.fit_result.stage_summaries.empty())
    {
        for (const FitStageSummary& summary : result.method_result.fit_result.stage_summaries)
        {
            FitCandidate candidate;
            candidate.candidate_id = summary.stage_id;
            candidate.source_stage_id = summary.stage_id;
            candidate.source_method_type = summary.method_type;
            candidate.valid = result.method_result.valid;
            candidate.final_cost = NonNegativeOr(summary.stage_cost);
            candidate.max_residual = NonNegativeOr(summary.stage_max_residual);
            candidate.iterations = summary.iterations;
            candidate.elapsed_ms = result.method_result.elapsed_ms;
            result.candidate_group.candidates.push_back(candidate);
        }

        std::vector<FitCandidateRankItem> ranking;
        ranking.reserve(result.candidate_group.candidates.size());
        for (const FitCandidate& candidate : result.candidate_group.candidates)
        {
            FitCandidateRankItem item;
            item.candidate_id = candidate.candidate_id;
            item.score = StageScore(candidate.final_cost, candidate.max_residual);
            ranking.push_back(item);
        }

        std::stable_sort(ranking.begin(), ranking.end(),
                         [](const FitCandidateRankItem& lhs, const FitCandidateRankItem& rhs)
                         {
                             return IsBetterScore(lhs.score, rhs.score);
                         });

        for (size_t i = 0; i < ranking.size(); ++i)
        {
            ranking[i].rank_index = static_cast<int>(i);
            result.candidate_group.baseline_rank.push_back(ranking[i]);
        }

        if (!ranking.empty())
        {
            result.candidate_group.best_candidate_id = ranking.front().candidate_id;
            result.candidate_group.group_score = ranking.front().score;
            result.candidate_group.rank_delta =
                ranking.size() > 1 ? SafeRankDelta(ranking[1].score, ranking.front().score) : 0.0;
            result.group_score = result.candidate_group.group_score;
            result.evaluation_score = result.group_score;
        }
    }
        FinalizeCandidateGroup(result.candidate_group, result.method_result);
        result.selected_match_candidate_index = candidate_index;
        result.evaluated_match_candidate_count = 1;
        return result;
    };

    if (match.candidates.empty())
    {
        return run_single(match, -1);
    }

    int best_match_candidate_index = -1;
    PrototypeRunResult best_result;
    bool has_best = false;
    int evaluated_count = 0;
    std::vector<PrototypeCandidateScore> candidate_scores;
    for (size_t i = 0; i < match.candidates.size(); ++i)
    {
        MatchOutput single_match = match;
        single_match.candidates = { match.candidates[i] };
        PrototypeRunResult current = run_single(single_match, static_cast<int>(i));
        ++evaluated_count;
        current.evaluated_match_candidate_count = evaluated_count;
        PrototypeCandidateScore score_entry;
        score_entry.candidate_index = static_cast<int>(i);
        score_entry.evaluation_score = current.evaluation_score;
        score_entry.valid = current.method_result.valid;
        candidate_scores.push_back(score_entry);
        if (!has_best || IsBetterScore(current.evaluation_score, best_result.evaluation_score))
        {
            best_result = current;
            best_match_candidate_index = static_cast<int>(i);
            has_best = true;
        }
    }
    std::vector<PrototypeCandidateScore> ordered_scores = candidate_scores;
    std::stable_sort(ordered_scores.begin(), ordered_scores.end(),
                     [](const PrototypeCandidateScore& lhs, const PrototypeCandidateScore& rhs)
                     {
                         return IsBetterScore(lhs.evaluation_score, rhs.evaluation_score);
                     });
    for (size_t i = 0; i < ordered_scores.size(); ++i)
    {
        ordered_scores[i].rank_index = static_cast<int>(i);
    }
    if (has_best)
    {
        best_result.best_match_candidate_index = best_match_candidate_index;
        best_result.evaluated_match_candidate_count = evaluated_count;
        best_result.candidate_scores = ordered_scores;
        best_result.ordered_match_candidate_indices.clear();
        best_result.ordered_match_candidate_indices.reserve(ordered_scores.size());
        for (const PrototypeCandidateScore& score_entry : ordered_scores)
        {
            best_result.ordered_match_candidate_indices.push_back(score_entry.candidate_index);
        }
        return best_result;
    }
    return run_single(match, -1);
}

PrototypeRunResult RunCircleRingPrototype(const CircleMeasurementOutput& outer_circle,
                                          const CircleMeasurementOutput& inner_circle,
                                          const FitOperationConfig& operation_config,
                                          double center_tolerance,
                                          double thickness_tolerance)
{
    PrototypeRunResult result;

    result.operation_config = operation_config;

    result.gauge =
        MakeCircleRingGauge(
            outer_circle,
            inner_circle,
            "circle_ring_gauge",
            "Circle Ring Gauge",
            center_tolerance,
            thickness_tolerance);

    result.task =
        MakeTaskSpecFromGauge(
            result.gauge,
            "circle_ring_task",
            FitTaskType::GaugeFit);

    result.problem =
        MakeFitProblemFromGauge(
            result.gauge,
            "circle_ring_problem");

    result.configured_problem = result.problem;

    result.group_score = result.gauge.learn_score;
    result.evaluation_score = result.gauge.learn_score;

    return result;
}

PrototypeRunResult RunCircleRingLinePrototype(const CircleMeasurementOutput& outer_circle,
                                              const CircleMeasurementOutput& inner_circle,
                                              const LineMeasurementOutput& line,
                                              const FitOperationConfig& operation_config,
                                              double center_tolerance,
                                              double thickness_tolerance)
{
    PrototypeRunResult result;

    result.operation_config = operation_config;

    result.gauge =
        MakeCircleRingLineGauge(
            outer_circle,
            inner_circle,
            line,
            "circle_ring_line_gauge",
            "Circle Ring + Line Gauge",
            center_tolerance,
            thickness_tolerance);

    result.task =
        MakeTaskSpecFromGauge(
            result.gauge,
            "circle_ring_line_task",
            FitTaskType::StructureFit);

    result.problem =
        MakeFitProblemFromGauge(
            result.gauge,
            "circle_ring_line_problem");

    result.configured_problem = result.problem;
    FitOperationController::Apply(result.operation_config, result.configured_problem);

    result.group_score = result.gauge.learn_score;
    result.evaluation_score = result.gauge.learn_score;

    return result;
}
} // namespace formfit
} // namespace cxcore
