#pragma once

#include <string>
#include <vector>

enum class FastMatchPolicyKind
{
    Disabled = 0,
    ReadinessOnly = 1,
    DiagnosticCandidate = 2,
    ExperimentalEnabled = 3
};

struct FastMatchReadinessInput
{
    bool l1_l3_coverage_ok = false;
    bool parameter_policy_valid = false;
    bool product_default_changed = false;

    bool original_measure_available = false;
    bool local_evidence_confirmed = false;
    bool component_warning = false;

    std::string tool;
    std::string image_level;
    std::string profile_id;
};

struct FastMatchReadinessResult
{
    bool allowed = false;
    std::string status;
    std::string reason;
    std::vector<std::string> gates;
};

FastMatchReadinessResult EvaluateFastMatchReadiness(
    const FastMatchReadinessInput& input);