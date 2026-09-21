#pragma once

#include "CxScriptStage25Manifest.h"

#include <string>
#include <vector>

struct Stage25CaseMatrixEntry
{
    std::string image_id;
    std::string level;
    std::string target_id;
    std::string tool;

    std::string profile_id;
    std::string parameter_policy_id;
    std::string parameter_role;

    std::string evidence_profile;

    bool enabled = true;
    std::string reason;
};

std::vector<Stage25CaseMatrixEntry> BuildStage25CaseMatrix(
    const Stage25Manifest& manifest);

bool ShouldRunFindlineProfileOnLevel(
    const Stage25FindlineProfile& profile,
    const std::string& level);

bool ShouldRunFindcircleProfileOnLevel(
    const Stage25FindcircleProfile& profile,
    const std::string& level);
