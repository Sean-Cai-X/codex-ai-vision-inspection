#include "FastMatchGridClassAdapter.h"

#include <algorithm>

namespace cxcore {
namespace {

double ClampUnit(double value)
{
    return std::max(0.0, std::min(1.0, value));
}

const char* ModeName(FastMatchGridFusionMode mode)
{
    switch (mode)
    {
    case FastMatchGridFusionMode::StructuralOnly:
        return "structural_only";
    case FastMatchGridFusionMode::GridClassOnly:
        return "grid_class_only";
    case FastMatchGridFusionMode::Cascade:
        return "fastmatch_then_grid_class";
    case FastMatchGridFusionMode::ScoreFusion:
        return "score_fusion";
    }
    return "unknown";
}

}  // namespace

void FastMatchGridClassAdapter::SetConfig(const FastMatchGridFusionConfig& config)
{
    config_ = config;
}

const FastMatchGridFusionConfig& FastMatchGridClassAdapter::GetConfig() const
{
    return config_;
}

FastMatchGridFusionResult FastMatchGridClassAdapter::Fuse(
    const FastMatchGridFusionInput& input) const
{
    FastMatchGridFusionResult output;
    output.candidate_id = input.candidate_id;
    output.class_id = input.class_result.class_id;
    output.mode = ModeName(config_.mode);
    output.structural_score = ClampUnit(input.structural_score);
    output.class_score = ClampUnit(input.class_result.best_score);
    output.evidence_stages = {
        "edge_or_region",
        "fastmatch_candidate",
        "normalized_grid",
        "hierarchy_activation",
        "class_match",
        "review_signal"
    };

    const bool structural_ok =
        input.structural_available &&
        input.candidate_count > 0 &&
        output.structural_score >= config_.structural_min_score;
    const bool class_ok =
        input.class_result.success &&
        !input.class_result.rejected &&
        output.class_score >= config_.class_min_score;
    const bool needs_structural =
        config_.mode != FastMatchGridFusionMode::GridClassOnly;
    const bool needs_class =
        config_.mode != FastMatchGridFusionMode::StructuralOnly;

    if (needs_structural && !structural_ok)
    {
        output.anomaly_flags.push_back("structural_candidate_rejected");
    }
    if (needs_class && !class_ok)
    {
        output.anomaly_flags.push_back("grid_class_rejected");
    }

    switch (config_.mode)
    {
    case FastMatchGridFusionMode::StructuralOnly:
        output.success = input.structural_available;
        output.rejected = !structural_ok;
        output.fused_score = output.structural_score;
        break;
    case FastMatchGridFusionMode::GridClassOnly:
        output.success = input.class_result.success;
        output.rejected = !class_ok;
        output.fused_score = output.class_score;
        break;
    case FastMatchGridFusionMode::Cascade:
        output.success = input.structural_available && input.class_result.success;
        output.rejected = !(structural_ok && class_ok);
        output.fused_score = structural_ok ? output.class_score : 0.0;
        break;
    case FastMatchGridFusionMode::ScoreFusion:
    {
        const double structural_weight = std::max(0.0, config_.structural_weight);
        const double class_weight = std::max(0.0, config_.class_weight);
        const double weight_sum = structural_weight + class_weight;
        output.success = input.structural_available && input.class_result.success && weight_sum > 0.0;
        output.fused_score = weight_sum > 0.0
            ? (structural_weight * output.structural_score +
               class_weight * output.class_score) / weight_sum
            : 0.0;
        output.rejected = !(structural_ok && class_ok);
        break;
    }
    }

    output.summary = output.rejected
        ? "fusion completed with manual review signal"
        : "fusion completed";
    return output;
}

}  // namespace cxcore
