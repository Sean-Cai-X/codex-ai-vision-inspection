#include "muParser.h"
#include "CxScriptSuiteRuntime.h"
#include "CxScriptSuiteRegister.h"

CxScriptSuiteRuntime g_cxscript_suite;
CxScriptSuiteCase* g_current_suite_case = nullptr;

double CxScriptSuite_reset(double)
{
    g_cxscript_suite = CxScriptSuiteRuntime{};
    g_current_suite_case = nullptr;
    return 0.0;
}

double CxScriptSuite_setid(const char* value)
{
    g_cxscript_suite.suite_id = value ? value : "";
    return 0.0;
}

double CxScriptSuite_setname(const char* value)
{
    g_cxscript_suite.name = value ? value : "";
    return 0.0;
}

double CxScriptSuite_setcatalog(const char* value)
{
    g_cxscript_suite.catalog_path = value ? value : "";
    return 0.0;
}

double CxScriptSuite_setoutputroot(const char* value)
{
    g_cxscript_suite.output_root = value ? value : "";
    return 0.0;
}

double CxScriptSuite_addcase(const char* value)
{
    CxScriptSuiteCase case_entry;
    case_entry.case_id = value ? value : "";
    g_cxscript_suite.cases.push_back(case_entry);
    g_current_suite_case = &g_cxscript_suite.cases.back();
    return 0.0;
}

double CxScriptSuite_case_setscriptid(const char* value)
{
    if (g_current_suite_case)
        g_current_suite_case->script_id = value ? value : "";
    return 0.0;
}

double CxScriptSuite_case_setimage(const char* value)
{
    if (g_current_suite_case)
        g_current_suite_case->image_id = value ? value : "";
    return 0.0;
}

double CxScriptSuite_case_setlevel(const char* value)
{
    if (g_current_suite_case)
        g_current_suite_case->level = value ? value : "";
    return 0.0;
}

double CxScriptSuite_case_setexpected(const char* value)
{
    if (g_current_suite_case)
        g_current_suite_case->expected_result = value ? value : "";
    return 0.0;
}

double CxScriptSuite_case_setexpectedpolicyguard(const char* value)
{
    if (g_current_suite_case)
        g_current_suite_case->expected_policy_guard = value ? value : "";
    return 0.0;
}

double CxScriptSuite_case_settarget(const char* value)
{
    if (g_current_suite_case)
        g_current_suite_case->target_id = value ? value : "";
    return 0.0;
}

double CxScriptSuite_case_setparameter(const char* value)
{
    if (g_current_suite_case)
        g_current_suite_case->parameter_profile_id = value ? value : "";
    return 0.0;
}

void RegisterCxScriptSuiteBindings(mu::Parser& parser)
{
    parser.DefineFun("CxScriptSuite_reset", (mu::fun_type1)&CxScriptSuite_reset);
    parser.DefineFun("CxScriptSuite_setid", (mu::strfun_type1)&CxScriptSuite_setid);
    parser.DefineFun("CxScriptSuite_setname", (mu::strfun_type1)&CxScriptSuite_setname);
    parser.DefineFun("CxScriptSuite_setcatalog", (mu::strfun_type1)&CxScriptSuite_setcatalog);
    parser.DefineFun("CxScriptSuite_setoutputroot", (mu::strfun_type1)&CxScriptSuite_setoutputroot);
    parser.DefineFun("CxScriptSuite_addcase", (mu::strfun_type1)&CxScriptSuite_addcase);
    parser.DefineFun("CxScriptSuite_case_setscriptid", (mu::strfun_type1)&CxScriptSuite_case_setscriptid);
    parser.DefineFun("CxScriptSuite_case_setimage", (mu::strfun_type1)&CxScriptSuite_case_setimage);
    parser.DefineFun("CxScriptSuite_case_setlevel", (mu::strfun_type1)&CxScriptSuite_case_setlevel);
    parser.DefineFun("CxScriptSuite_case_setexpected", (mu::strfun_type1)&CxScriptSuite_case_setexpected);
    parser.DefineFun("CxScriptSuite_case_setexpectedpolicyguard", (mu::strfun_type1)&CxScriptSuite_case_setexpectedpolicyguard);
    parser.DefineFun("CxScriptSuite_case_settarget", (mu::strfun_type1)&CxScriptSuite_case_settarget);
    parser.DefineFun("CxScriptSuite_case_setparameter", (mu::strfun_type1)&CxScriptSuite_case_setparameter);
}