#pragma once

#include <string>

class FastMatchDiagnostic
{
public:
    void setpolicy(const char* policy_id);
    void setsource(const char* source_tool);
    void setlevel(const char* image_level);
    void setprofile(const char* profile_id);

    void set_l1_l3_coverage_ok(int value);
    void set_parameter_policy_valid(int value);
    void set_product_default_changed(int value);
    void set_original_measure_available(int value);
    void set_local_evidence_confirmed(int value);
    void set_component_warning(int value);

    void run();

    int allowed() const;
    int status_code() const;
    int reason_code() const;
    const char* result_ref() const;

private:
    std::string m_policy_id = "readiness_only";
    std::string m_source_tool;
    std::string m_image_level;
    std::string m_profile_id;

    bool m_l1_l3_coverage_ok = false;
    bool m_parameter_policy_valid = false;
    bool m_product_default_changed = false;
    bool m_original_measure_available = false;
    bool m_local_evidence_confirmed = false;
    bool m_component_warning = false;

    bool m_allowed = false;
    std::string m_status = "not_run";
    std::string m_reason;
    std::string m_result_ref;
    int m_status_code = 0;
    int m_reason_code = 0;
};