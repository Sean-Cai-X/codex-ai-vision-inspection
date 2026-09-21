#include "pch.h"
#include "CxScriptStage25PolicyValidator.h"

static void AddIssue(
    Stage25PolicyValidationResult& result,
    const std::string& severity,
    const std::string& scope,
    const std::string& profile_id,
    const std::string& message)
{
    Stage25PolicyValidationIssue issue;
    issue.severity = severity;
    issue.scope = scope;
    issue.profile_id = profile_id;
    issue.message = message;

    result.issues.push_back(issue);

    if (severity == "error")
        result.ok = false;
}

Stage25PolicyValidationResult ValidateStage25ParameterPolicies(
    const Stage25Manifest& manifest)
{
    Stage25PolicyValidationResult result;

    for (const auto& profile : manifest.findline_profiles)
    {
        if (profile.profile_id.empty())
        {
            AddIssue(
                result,
                "error",
                "findline_profile",
                "",
                "profile_id is empty");
            continue;
        }

        if (profile.parameter_policy_id.empty())
        {
            AddIssue(
                result,
                "error",
                "findline_profile",
                profile.profile_id,
                "parameter_policy_id is empty");
        }

        if (profile.parameter_role.empty())
        {
            AddIssue(
                result,
                "error",
                "findline_profile",
                profile.profile_id,
                "parameter_role is empty");
        }

        if (profile.parameter_policy_id == "stage25_filter20" &&
            profile.is_product_default)
        {
            AddIssue(
                result,
                "error",
                "findline_profile",
                profile.profile_id,
                "stage25_filter20 must not be marked as product default");
        }

        if (profile.parameter_policy_id == "legacy_default" &&
            !profile.is_product_default)
        {
            AddIssue(
                result,
                "warning",
                "findline_profile",
                profile.profile_id,
                "legacy_default should normally be marked as product default baseline");
        }

        if (profile.is_product_default && profile.is_stage25_default)
        {
            AddIssue(
                result,
                "error",
                "findline_profile",
                profile.profile_id,
                "profile cannot be both product default and stage25 default");
        }
    }

    for (const auto& profile : manifest.findcircle_profiles)
    {
        if (profile.profile_id.empty())
        {
            AddIssue(
                result,
                "error",
                "findcircle_profile",
                "",
                "profile_id is empty");
            continue;
        }

        if (profile.parameter_policy_id.empty())
        {
            AddIssue(
                result,
                "error",
                "findcircle_profile",
                profile.profile_id,
                "parameter_policy_id is empty");
        }

        if (profile.parameter_role.empty())
        {
            AddIssue(
                result,
                "error",
                "findcircle_profile",
                profile.profile_id,
                "parameter_role is empty");
        }

        if (profile.is_product_default && profile.is_stage25_default)
        {
            AddIssue(
                result,
                "error",
                "findcircle_profile",
                profile.profile_id,
                "profile cannot be both product default and stage25 default");
        }
    }

    return result;
}

Stage25CoverageGateResult EvaluateStage25CoverageGate(
    const Stage25Manifest& manifest)
{
    Stage25CoverageGateResult r;

    for (const auto& img : manifest.images)
    {
        for (const auto& target : img.targets)
        {
            if (target.tool == "FindLine")
            {
                if (img.level == "L1_high_contrast")
                    r.l1_findline_targets++;
                else if (img.level == "L2_low_contrast_illumination")
                    r.l2_findline_targets++;
                else if (img.level == "L3_complex_boundary")
                    r.l3_findline_targets++;
            }

            if (target.tool == "Findcircle")
            {
                if (img.level == "L1_high_contrast")
                    r.l1_findcircle_targets++;
                else if (img.level == "L2_low_contrast_illumination")
                    r.l2_findcircle_targets++;
                else if (img.level == "L3_complex_boundary")
                    r.l3_findcircle_targets++;
            }
        }
    }

    if (r.l1_findline_targets < 2 ||
        r.l2_findline_targets < 2 ||
        r.l3_findline_targets < 2)
    {
        r.reason = "insufficient Findline L1/L2/L3 coverage";
        return r;
    }

    if (r.l1_findcircle_targets < 2 ||
        r.l2_findcircle_targets < 2 ||
        r.l3_findcircle_targets < 2)
    {
        r.reason = "insufficient Findcircle L1/L2/L3 coverage";
        return r;
    }

    r.ok = true;
    r.reason = "coverage gate passed";
    return r;
}
