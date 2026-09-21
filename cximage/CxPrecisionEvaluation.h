#pragma once

#include <string>

struct CxScriptSuiteCaseResult;

struct CxPrecisionGateSnapshot
{
    bool evaluated = false;
    bool residual_pass = false;
    bool support_pass = false;
    double subpixel_offset_mean = 0.0;
    double subpixel_offset_stddev = 0.0;
    double localization_sigma_mean_px = 0.0;
    double residual_rmse_px = 0.0;
    double residual_p95_px = 0.0;
    double residual_max_px = 0.0;
    double reliability_score = 0.0;

    double residual_px = 0.0;
    double residual_limit_px = 0.0;
    double support = 0.0;
    double support_limit = 0.0;
    std::string status = "NOT_BOUND";
    std::string reason = "Precision gate is not bound for this tool.";
    std::string metric_family = "not_bound";
};

CxPrecisionGateSnapshot EvaluateCxPrecisionGate(
    const CxScriptSuiteCaseResult& result);
