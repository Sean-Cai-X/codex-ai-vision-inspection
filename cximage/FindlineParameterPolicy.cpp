#include "pch.h"
#include "FindlineParameterPolicy.h"

const char* ToString(FindlineParameterPolicyKind kind)
{
    switch (kind)
    {
    case FindlineParameterPolicyKind::LegacyDefault:
        return "legacy_default";
    case FindlineParameterPolicyKind::Stage25Filter20:
        return "stage25_filter20";
    case FindlineParameterPolicyKind::LowContrastThreshold8:
        return "threshold8";
    case FindlineParameterPolicyKind::ComplexBoundaryLinegap10:
        return "linegap10";
    case FindlineParameterPolicyKind::DebugFilterRelaxMin1:
        return "filter_relax_min1";
    case FindlineParameterPolicyKind::RiskGamma:
        return "gamma";
    default:
        return "unknown";
    }
}

const char* ToString(FindlineParameterRole role)
{
    switch (role)
    {
    case FindlineParameterRole::ProductLegacyDefault:
        return "PRODUCT_LEGACY_DEFAULT";
    case FindlineParameterRole::Stage25RecommendedTemplate:
        return "STAGE25_RECOMMENDED_TEMPLATE";
    case FindlineParameterRole::ScenarioCandidate:
        return "SCENARIO_CANDIDATE";
    case FindlineParameterRole::DebugOnly:
        return "DEBUG_ONLY";
    case FindlineParameterRole::RiskCandidate:
        return "RISK_CANDIDATE";
    default:
        return "UNKNOWN";
    }
}

FindlineParameterPolicy MakeFindlinePolicy(
    FindlineParameterPolicyKind kind)
{
    FindlineParameterPolicy p;
    p.kind = kind;

    switch (kind)
    {
    case FindlineParameterPolicyKind::LegacyDefault:
        p.policy_id = "legacy_default";
        p.display_name = "Legacy Default";
        p.decision_class = "LEGACY_COMPARE";
        p.role = FindlineParameterRole::ProductLegacyDefault;

        p.method = 0;
        p.threshold = 20;
        p.linegap = 6;
        p.fitmode = 1;
        p.tool_half_width = 32;
        p.filter_profile = 0;

        p.is_product_default = true;
        p.is_stage25_default = false;
        p.recommended_for_manual_console = false;
        p.can_be_promoted_to_product_default = false;
        break;

    case FindlineParameterPolicyKind::Stage25Filter20:
        p.policy_id = "stage25_filter20";
        p.display_name = "Stage25 Filter20";
        p.decision_class = "STAGE25_RECOMMENDED_TEMPLATE";
        p.role = FindlineParameterRole::Stage25RecommendedTemplate;

        p.method = 0;
        p.threshold = 20;
        p.linegap = 6;
        p.fitmode = 1;
        p.tool_half_width = 32;
        p.filter_profile = 1;

        p.is_product_default = false;
        p.is_stage25_default = true;
        p.recommended_for_manual_console = true;
        p.can_be_promoted_to_product_default = false;
        break;

    case FindlineParameterPolicyKind::LowContrastThreshold8:
        p.policy_id = "threshold8";
        p.display_name = "Threshold8 Low Contrast Candidate";
        p.decision_class = "LOW_CONTRAST_CANDIDATE";
        p.role = FindlineParameterRole::ScenarioCandidate;

        p.method = 0;
        p.threshold = 8;
        p.linegap = 6;
        p.fitmode = 1;
        p.tool_half_width = 32;
        p.filter_profile = 0;
        break;

    case FindlineParameterPolicyKind::ComplexBoundaryLinegap10:
        p.policy_id = "linegap10";
        p.display_name = "Linegap10 Complex Boundary Candidate";
        p.decision_class = "COMPLEX_BOUNDARY_CANDIDATE";
        p.role = FindlineParameterRole::ScenarioCandidate;

        p.method = 0;
        p.threshold = 20;
        p.linegap = 10;
        p.fitmode = 1;
        p.tool_half_width = 32;
        p.filter_profile = 0;
        break;

    case FindlineParameterPolicyKind::DebugFilterRelaxMin1:
        p.policy_id = "filter_relax_min1";
        p.display_name = "Filter Relax Min1 Debug";
        p.decision_class = "DEBUG_ONLY";
        p.role = FindlineParameterRole::DebugOnly;

        p.method = 0;
        p.threshold = 20;
        p.linegap = 6;
        p.fitmode = 1;
        p.tool_half_width = 32;
        p.filter_profile = 0;

        p.explicit_filter = true;
        p.filter_borw = 21;
        p.filter_min = 1;
        p.filter_max = 100000;
        break;

    case FindlineParameterPolicyKind::RiskGamma:
        p.policy_id = "gamma";
        p.display_name = "Gamma Risk Candidate";
        p.decision_class = "RISK_CANDIDATE";
        p.role = FindlineParameterRole::RiskCandidate;

        p.method = 0;
        p.threshold = 20;
        p.linegap = 6;
        p.fitmode = 1;
        p.tool_half_width = 32;
        p.filter_profile = 0;

        p.enable_gamma = true;
        p.gamma = 1;
        break;
    }

    return p;
}

FindlineProductDefaultGateResult EvaluateFindlineProductDefaultGate(
    const FindlineProductDefaultGateInput& input)
{
    FindlineProductDefaultGateResult result;

    if (input.total_images < 12)
    {
        result.reason = "total_images < 12";
        return result;
    }

    if (input.level_count < 3)
    {
        result.reason = "level_count < 3; requires L1/L2/L3 coverage";
        return result;
    }

    if (input.orientation_count < 2)
    {
        result.reason = "orientation_count < 2; requires at least horizontal and vertical";
        return result;
    }

    if (input.original_success_rate < 0.85)
    {
        result.reason = "original_success_rate < 0.85";
        return result;
    }

    if (input.local_confirmed_rate < 0.80)
    {
        result.reason = "local_confirmed_rate < 0.80";
        return result;
    }

    if (input.component_warning_rate > 0.20)
    {
        result.reason = "component_warning_rate > 0.20";
        return result;
    }

    if (input.mean_fit_offset > 6.0)
    {
        result.reason = "mean_fit_offset > 6.0";
        return result;
    }

    result.can_promote = true;
    result.reason = "all gates passed";

    return result;
}
