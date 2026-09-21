#include "pch.h"
#include "FastMatchDiagnostic.h"
#include "FastMatchPolicy.h"

#include <sstream>

void FastMatchDiagnostic::setpolicy(const char* policy_id)
{
    m_policy_id = policy_id ? policy_id : "";
}

void FastMatchDiagnostic::setsource(const char* source_tool)
{
    m_source_tool = source_tool ? source_tool : "";
}

void FastMatchDiagnostic::setlevel(const char* image_level)
{
    m_image_level = image_level ? image_level : "";
}

void FastMatchDiagnostic::setprofile(const char* profile_id)
{
    m_profile_id = profile_id ? profile_id : "";
}

void FastMatchDiagnostic::set_l1_l3_coverage_ok(int value)
{
    m_l1_l3_coverage_ok = value != 0;
}

void FastMatchDiagnostic::set_parameter_policy_valid(int value)
{
    m_parameter_policy_valid = value != 0;
}

void FastMatchDiagnostic::set_product_default_changed(int value)
{
    m_product_default_changed = value != 0;
}

void FastMatchDiagnostic::set_original_measure_available(int value)
{
    m_original_measure_available = value != 0;
}

void FastMatchDiagnostic::set_local_evidence_confirmed(int value)
{
    m_local_evidence_confirmed = value != 0;
}

void FastMatchDiagnostic::set_component_warning(int value)
{
    m_component_warning = value != 0;
}

void FastMatchDiagnostic::run()
{
    FastMatchReadinessInput input;
    input.l1_l3_coverage_ok = m_l1_l3_coverage_ok;
    input.parameter_policy_valid = m_parameter_policy_valid;
    input.product_default_changed = m_product_default_changed;
    input.original_measure_available = m_original_measure_available;
    input.local_evidence_confirmed = m_local_evidence_confirmed;
    input.component_warning = m_component_warning;
    input.tool = m_source_tool;
    input.image_level = m_image_level;
    input.profile_id = m_profile_id;

    auto readiness = EvaluateFastMatchReadiness(input);

    m_allowed = readiness.allowed;
    m_status = readiness.status;
    m_reason = readiness.reason;

    if (m_status == "not_run")
        m_status_code = 0;
    else if (m_status == "blocked")
        m_status_code = 1;
    else if (m_status == "allowed_diagnostic")
        m_status_code = 2;

    if (m_reason.find("product default was changed") != std::string::npos)
        m_reason_code = 1;
    else if (m_reason.find("parameter policy validation") != std::string::npos)
        m_reason_code = 2;
    else if (m_reason.find("L1/L2/L3 coverage") != std::string::npos)
        m_reason_code = 3;
    else if (m_reason.find("original Measure result is unavailable") != std::string::npos)
        m_reason_code = 4;
    else if (m_reason.find("reserved for L3 or component-warning") != std::string::npos)
        m_reason_code = 5;
    else if (m_reason.find("diagnostic is allowed") != std::string::npos)
        m_reason_code = 6;

    std::ostringstream ref;
    ref << "fastmatch_diagnostic<="
        << m_source_tool
        << "|profile=" << m_profile_id
        << "|level=" << m_image_level
        << "|allowed=" << (m_allowed ? "true" : "false");

    m_result_ref = ref.str();
}

int FastMatchDiagnostic::allowed() const
{
    return m_allowed ? 1 : 0;
}

int FastMatchDiagnostic::status_code() const
{
    return m_status_code;
}

int FastMatchDiagnostic::reason_code() const
{
    return m_reason_code;
}

const char* FastMatchDiagnostic::result_ref() const
{
    return m_result_ref.c_str();
}