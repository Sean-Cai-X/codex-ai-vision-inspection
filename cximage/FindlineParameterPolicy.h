#pragma once

#include <string>

enum class FindlineParameterPolicyKind
{
    LegacyDefault = 0,
    Stage25Filter20 = 1,
    LowContrastThreshold8 = 2,
    ComplexBoundaryLinegap10 = 3,
    DebugFilterRelaxMin1 = 4,
    RiskGamma = 5
};

enum class FindlineParameterRole
{
    ProductLegacyDefault = 0,
    Stage25RecommendedTemplate = 1,
    ScenarioCandidate = 2,
    DebugOnly = 3,
    RiskCandidate = 4
};

struct FindlineParameterPolicy
{
    FindlineParameterPolicyKind kind =
        FindlineParameterPolicyKind::LegacyDefault;

    FindlineParameterRole role =
        FindlineParameterRole::ProductLegacyDefault;

    std::string policy_id;
    std::string display_name;
    std::string decision_class;

    int method = 0;
    int threshold = 20;
    int linegap = 6;
    int fitmode = 1;
    int tool_half_width = 32;

    int filter_profile = 0;

    bool explicit_filter = false;
    int filter_borw = 21;
    int filter_min = 50;
    int filter_max = 100000;

    bool enable_gamma = false;
    int gamma = 0;

    bool is_product_default = false;
    bool is_stage25_default = false;
    bool recommended_for_manual_console = false;
    bool can_be_promoted_to_product_default = false;
};

struct FindlineProductDefaultGateInput
{
    int total_images = 0;
    int level_count = 0;
    int orientation_count = 0;

    double original_success_rate = 0.0;
    double local_confirmed_rate = 0.0;
    double component_warning_rate = 0.0;
    double mean_fit_offset = 0.0;
};

struct FindlineProductDefaultGateResult
{
    bool can_promote = false;
    std::string reason;
};

const char* ToString(FindlineParameterPolicyKind kind);
const char* ToString(FindlineParameterRole role);

FindlineParameterPolicy MakeFindlinePolicy(
    FindlineParameterPolicyKind kind);

FindlineProductDefaultGateResult EvaluateFindlineProductDefaultGate(
    const FindlineProductDefaultGateInput& input);
