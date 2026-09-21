#ifndef CXIMAGE_CXSCRIPT_SUITE_RUNTIME_H
#define CXIMAGE_CXSCRIPT_SUITE_RUNTIME_H

#include <string>
#include <vector>

struct CxScriptSuiteCase
{
    std::string case_id;
    std::string script_id;
    std::string image_id;
    std::string target_id;
    std::string parameter_profile_id;
    std::string level;
    std::string expected_result;
    std::string expected_policy_guard;
};

struct CxScriptSuiteRuntime
{
    std::string suite_id;
    std::string name;
    std::string catalog_path;
    std::string output_root;
    std::vector<CxScriptSuiteCase> cases;
};

bool LoadCxScriptSuiteFile(
    const std::string& suite_path,
    CxScriptSuiteRuntime& out_suite,
    std::string& out_reason);

#endif