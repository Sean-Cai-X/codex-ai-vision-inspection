#include "pch.h"
#include "CxScriptStage25CaseMatrix.h"

bool ShouldRunFindlineProfileOnLevel(
    const Stage25FindlineProfile& profile,
    const std::string& level)
{
    const std::string& policy = profile.parameter_policy_id;

    if (policy == "stage25_filter20")
        return true;

    if (policy == "legacy_default")
        return level == "L1_high_contrast" || level == "L0_basic";

    if (policy == "threshold8")
        return level == "L2_low_contrast_illumination";

    if (policy == "linegap10")
        return level == "L3_complex_boundary";

    if (policy == "filter_relax_min1")
        return level == "L3_complex_boundary";

    if (policy == "gamma")
        return level == "L2_low_contrast_illumination" ||
               level == "L3_complex_boundary";

    return false;
}

bool ShouldRunFindcircleProfileOnLevel(
    const Stage25FindcircleProfile& profile,
    const std::string& level)
{
    const std::string& policy = profile.parameter_policy_id;

    if (policy == "direct")
        return level == "L1_high_contrast" || level == "L0_basic";

    if (policy == "threshold8")
        return level == "L2_low_contrast_illumination";

    if (policy == "method1")
        return level == "L3_complex_boundary";

    if (policy == "filter_relax")
        return level == "L3_complex_boundary";

    return false;
}

std::vector<Stage25CaseMatrixEntry> BuildStage25CaseMatrix(
    const Stage25Manifest& manifest)
{
    std::vector<Stage25CaseMatrixEntry> matrix;

    for (const auto& image : manifest.images)
    {
        for (const auto& target : image.targets)
        {
            if (target.tool == "FindLine")
            {
                for (const auto& profile : manifest.findline_profiles)
                {
                    if (!ShouldRunFindlineProfileOnLevel(profile, image.level))
                    {
                        Stage25CaseMatrixEntry entry;
                        entry.image_id = image.image_id;
                        entry.level = image.level;
                        entry.target_id = target.target_id;
                        entry.tool = "FindLine";
                        entry.profile_id = profile.profile_id;
                        entry.parameter_policy_id = profile.parameter_policy_id;
                        entry.parameter_role = profile.parameter_role;
                        entry.evidence_profile = "";
                        entry.enabled = false;
                        entry.reason = "profile not applicable for level " + image.level;
                        matrix.push_back(entry);
                        continue;
                    }

                    for (const auto& evidence : manifest.evidence_profiles)
                    {
                        Stage25CaseMatrixEntry entry;
                        entry.image_id = image.image_id;
                        entry.level = image.level;
                        entry.target_id = target.target_id;
                        entry.tool = "FindLine";
                        entry.profile_id = profile.profile_id;
                        entry.parameter_policy_id = profile.parameter_policy_id;
                        entry.parameter_role = profile.parameter_role;
                        entry.evidence_profile = evidence.name;
                        entry.enabled = true;
                        entry.reason = "profile applicable for level";

                        matrix.push_back(entry);
                    }
                }
            }

            if (target.tool == "Findcircle")
            {
                for (const auto& profile : manifest.findcircle_profiles)
                {
                    if (!ShouldRunFindcircleProfileOnLevel(profile, image.level))
                    {
                        Stage25CaseMatrixEntry entry;
                        entry.image_id = image.image_id;
                        entry.level = image.level;
                        entry.target_id = target.target_id;
                        entry.tool = "Findcircle";
                        entry.profile_id = profile.profile_id;
                        entry.parameter_policy_id = profile.parameter_policy_id;
                        entry.parameter_role = profile.parameter_role;
                        entry.evidence_profile = "";
                        entry.enabled = false;
                        entry.reason = "profile not applicable for level " + image.level;
                        matrix.push_back(entry);
                        continue;
                    }

                    for (const auto& evidence : manifest.evidence_profiles)
                    {
                        Stage25CaseMatrixEntry entry;
                        entry.image_id = image.image_id;
                        entry.level = image.level;
                        entry.target_id = target.target_id;
                        entry.tool = "Findcircle";
                        entry.profile_id = profile.profile_id;
                        entry.parameter_policy_id = profile.parameter_policy_id;
                        entry.parameter_role = profile.parameter_role;
                        entry.evidence_profile = evidence.name;
                        entry.enabled = true;
                        entry.reason = "profile applicable for level";

                        matrix.push_back(entry);
                    }
                }
            }
        }
    }

    return matrix;
}
