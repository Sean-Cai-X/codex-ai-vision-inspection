#include "CxPrecisionEvaluation.h"
#include "CxScriptSuiteRunner.h"

namespace
{
constexpr double kFindCircleResidualLimitPx = 8.0;
constexpr double kFindLineResidualCandidateLimitPx = 6.0;
constexpr double kFindLineSupportCandidateLimit = 0.90;
}

CxPrecisionGateSnapshot EvaluateCxPrecisionGate(
    const CxScriptSuiteCaseResult& result)
{
    CxPrecisionGateSnapshot gate;

    if (result.tool == "FindCircle")
    {
        gate.metric_family = "residual_avgdist";
        gate.residual_px = result.avgdist;
        gate.residual_limit_px = kFindCircleResidualLimitPx;

        if (!result.headless_ok)
        {
            gate.status = "NOT_EVALUATED_HEADLESS_FAILED";
            gate.reason = "Headless execution did not complete.";
            return gate;
        }

        if (result.valid_points_count < 3)
        {
            gate.status = "NOT_EVALUATED_INSUFFICIENT_POINTS";
            gate.reason = "FindCircle residual gate requires at least 3 valid points.";
            return gate;
        }

        if (!result.has_fit_circle || result.circle_radius <= 0.0)
        {
            gate.status = "NOT_EVALUATED_NO_FIT_CIRCLE";
            gate.reason = "FindCircle residual gate requires a fitted circle.";
            return gate;
        }

        gate.evaluated = true;
        gate.residual_pass = result.avgdist <= kFindCircleResidualLimitPx;
        gate.subpixel_offset_mean = result.boundary_subpixel_offset_mean;
        gate.subpixel_offset_stddev = result.boundary_subpixel_offset_stddev;
        gate.localization_sigma_mean_px =
            result.boundary_localization_sigma_mean_px;
        gate.residual_rmse_px = result.boundary_residual_rmse_px;
        gate.residual_p95_px = result.boundary_residual_p95_px;
        gate.residual_max_px = result.boundary_residual_max_px;
        gate.reliability_score = result.boundary_reliability_score;
        gate.status = gate.residual_pass
            ? "RESIDUAL_GATE_PASS"
            : "RESIDUAL_GATE_FAIL";
        gate.reason = gate.residual_pass
            ? "FindCircle avgdist is within the residual limit."
            : "FindCircle avgdist exceeds the residual limit.";
        return gate;
    }

    if (result.tool == "FindLine")
    {
        gate.metric_family = "line_residual_support";
        gate.residual_px = result.avgdist;
        gate.residual_limit_px = kFindLineResidualCandidateLimitPx;
        gate.support = result.local_support;
        gate.support_limit = kFindLineSupportCandidateLimit;

        if (!result.headless_ok)
        {
            gate.status = "NOT_EVALUATED_HEADLESS_FAILED";
            gate.reason = "Headless execution did not complete.";
            return gate;
        }

        gate.support_pass = result.local_support >= kFindLineSupportCandidateLimit;
        if (!gate.support_pass &&
            (result.valid_points_count < 2 || !result.has_fit_line))
        {
            gate.evaluated = true;
            gate.status = "SUPPORT_GATE_FAIL";
            gate.reason = "FindLine boundary coverage support is below the active support limit.";
            return gate;
        }

        if (result.valid_points_count < 2)
        {
            gate.status = "NOT_EVALUATED_INSUFFICIENT_POINTS";
            gate.reason = "FindLine precision gate requires at least 2 valid points.";
            return gate;
        }

        if (!result.has_fit_line)
        {
            gate.status = "NOT_EVALUATED_NO_FIT_LINE";
            gate.reason = "FindLine precision gate requires a fitted line.";
            return gate;
        }

        gate.evaluated = true;
        gate.residual_pass = result.avgdist <= kFindLineResidualCandidateLimitPx;
        gate.subpixel_offset_mean = result.boundary_subpixel_offset_mean;
        gate.subpixel_offset_stddev = result.boundary_subpixel_offset_stddev;
        gate.localization_sigma_mean_px = result.boundary_localization_sigma_mean_px;
        gate.residual_rmse_px = result.boundary_residual_rmse_px;
        gate.residual_p95_px = result.boundary_residual_p95_px;
        gate.residual_max_px = result.boundary_residual_max_px;
        gate.reliability_score = result.boundary_reliability_score;

        if (!gate.residual_pass)
        {
            gate.status = "RESIDUAL_GATE_FAIL";
            gate.reason = "FindLine residual exceeds the active residual limit.";
            return gate;
        }

        if (!gate.support_pass)
        {
            gate.status = "SUPPORT_GATE_FAIL";
            gate.reason = "FindLine boundary coverage support is below the active support limit.";
            return gate;
        }

        gate.status = "PRECISION_GATE_PASS";
        gate.reason = "FindLine residual and boundary coverage support are within active limits.";
        return gate;
    }

    return gate;
}
