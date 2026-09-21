#pragma once

#include "CxScriptStage25Manifest.h"
#include "FindlineParameterPolicy.h"

#include <string>
#include <vector>

struct Stage25PolicyValidationIssue
{
    std::string severity;
    std::string scope;
    std::string profile_id;
    std::string message;
};

struct Stage25PolicyValidationResult
{
    bool ok = true;
    std::vector<Stage25PolicyValidationIssue> issues;
};

Stage25PolicyValidationResult ValidateStage25ParameterPolicies(
    const Stage25Manifest& manifest);

struct Stage25CoverageGateResult
{
    bool ok = false;
    std::string reason;

    int l1_findline_targets = 0;
    int l2_findline_targets = 0;
    int l3_findline_targets = 0;

    int l1_findcircle_targets = 0;
    int l2_findcircle_targets = 0;
    int l3_findcircle_targets = 0;
};

Stage25CoverageGateResult EvaluateStage25CoverageGate(
    const Stage25Manifest& manifest);
