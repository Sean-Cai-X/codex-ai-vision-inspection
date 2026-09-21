#include "pch.h"
#include "FastMatchPolicy.h"

FastMatchReadinessResult EvaluateFastMatchReadiness(
    const FastMatchReadinessInput& input)
{
    FastMatchReadinessResult result;
    result.status = "blocked";

    if (input.product_default_changed)
    {
        result.reason = "product default was changed; FastMatch must not be introduced during product-default mutation";
        result.gates.push_back("product_default_changed=true");
        return result;
    }

    if (!input.parameter_policy_valid)
    {
        result.reason = "parameter policy validation has not passed";
        result.gates.push_back("parameter_policy_valid=false");
        return result;
    }

    if (!input.l1_l3_coverage_ok)
    {
        result.reason = "L1/L2/L3 coverage is insufficient";
        result.gates.push_back("l1_l3_coverage_ok=false");
        return result;
    }

    if (!input.original_measure_available)
    {
        result.reason = "original Measure result is unavailable; FastMatch cannot be used as replacement";
        result.gates.push_back("original_measure_available=false");
        return result;
    }

    if (input.image_level != "L3_complex_boundary" &&
        !input.component_warning)
    {
        result.reason = "FastMatch diagnostic is reserved for L3 or component-warning cases";
        result.gates.push_back("not L3 and no component warning");
        return result;
    }

    result.allowed = true;
    result.status = "allowed_diagnostic";
    result.reason = "FastMatch diagnostic is allowed, but not promoted to Measure default";
    result.gates.push_back("diagnostic_only=true");

    return result;
}