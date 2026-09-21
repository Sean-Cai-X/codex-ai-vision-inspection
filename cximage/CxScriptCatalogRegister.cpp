#include "muParser.h"
#include "CxScriptCatalogRuntime.h"
#include "CxScriptCatalogRegister.h"

CxScriptCatalogRuntime g_cxscript_catalog;
CxScriptCatalogEntry* g_current_catalog_entry = nullptr;

double CxScriptCatalog_reset(double)
{
    g_cxscript_catalog = CxScriptCatalogRuntime{};
    g_current_catalog_entry = nullptr;
    return 0.0;
}

double CxScriptCatalog_setversion(const char* value)
{
    g_cxscript_catalog.version = value ? value : "";
    return 0.0;
}

double CxScriptCatalog_setname(const char* value)
{
    g_cxscript_catalog.name = value ? value : "";
    return 0.0;
}

double CxScriptCatalog_addscript(const char* value)
{
    CxScriptCatalogEntry entry;
    entry.script_id = value ? value : "";
    g_cxscript_catalog.scripts.push_back(entry);
    g_current_catalog_entry = &g_cxscript_catalog.scripts.back();
    return 0.0;
}

double CxScriptCatalog_script_setlabel(const char* value)
{
    if (g_current_catalog_entry)
        g_current_catalog_entry->label = value ? value : "";
    return 0.0;
}

double CxScriptCatalog_script_setpath(const char* value)
{
    if (g_current_catalog_entry)
        g_current_catalog_entry->path = value ? value : "";
    return 0.0;
}

double CxScriptCatalog_script_settool(const char* value)
{
    if (g_current_catalog_entry)
        g_current_catalog_entry->tool = value ? value : "";
    return 0.0;
}

double CxScriptCatalog_script_setorientation(const char* value)
{
    if (g_current_catalog_entry)
        g_current_catalog_entry->orientation = value ? value : "";
    return 0.0;
}

double CxScriptCatalog_script_setparameterpolicy(const char* value)
{
    if (g_current_catalog_entry)
        g_current_catalog_entry->parameter_policy_id = value ? value : "";
    return 0.0;
}

double CxScriptCatalog_script_setparameterrole(const char* value)
{
    if (g_current_catalog_entry)
        g_current_catalog_entry->parameter_role = value ? value : "";
    return 0.0;
}

double CxScriptCatalog_script_setexpected(const char* value)
{
    if (g_current_catalog_entry)
        g_current_catalog_entry->expected_result = value ? value : "";
    return 0.0;
}

double CxScriptCatalog_script_setexpectedstatus(const char* value)
{
    if (g_current_catalog_entry)
        g_current_catalog_entry->expected_status = value ? value : "";
    return 0.0;
}

double CxScriptCatalog_script_setexpectedpolicyguard(const char* value)
{
    if (g_current_catalog_entry)
        g_current_catalog_entry->expected_policy_guard = value ? value : "";
    return 0.0;
}

double CxScriptCatalog_script_setcontract(const char* value)
{
    if (g_current_catalog_entry)
        g_current_catalog_entry->contract_path = value ? value : "";
    return 0.0;
}

double CxScriptCatalog_script_setfrozen(double value)
{
    if (g_current_catalog_entry)
        g_current_catalog_entry->frozen = (value != 0);
    return 0.0;
}

double CxScriptCatalog_script_setmanualvisible(double value)
{
    if (g_current_catalog_entry)
        g_current_catalog_entry->manual_visible = (value != 0);
    return 0.0;
}

double CxScriptCatalog_script_setregressionvisible(double value)
{
    if (g_current_catalog_entry)
        g_current_catalog_entry->regression_visible = (value != 0);
    return 0.0;
}

double CxScriptCatalog_script_setadvancedvisible(double value)
{
    if (g_current_catalog_entry)
        g_current_catalog_entry->advanced_visible = (value != 0);
    return 0.0;
}

void RegisterCxScriptCatalogBindings(mu::Parser& parser)
{
    parser.DefineFun("CxScriptCatalog_reset", (mu::fun_type1)&CxScriptCatalog_reset);
    parser.DefineFun("CxScriptCatalog_setversion", (mu::strfun_type1)&CxScriptCatalog_setversion);
    parser.DefineFun("CxScriptCatalog_setname", (mu::strfun_type1)&CxScriptCatalog_setname);
    parser.DefineFun("CxScriptCatalog_addscript", (mu::strfun_type1)&CxScriptCatalog_addscript);
    parser.DefineFun("CxScriptCatalog_script_setlabel", (mu::strfun_type1)&CxScriptCatalog_script_setlabel);
    parser.DefineFun("CxScriptCatalog_script_setpath", (mu::strfun_type1)&CxScriptCatalog_script_setpath);
    parser.DefineFun("CxScriptCatalog_script_settool", (mu::strfun_type1)&CxScriptCatalog_script_settool);
    parser.DefineFun("CxScriptCatalog_script_setorientation", (mu::strfun_type1)&CxScriptCatalog_script_setorientation);
    parser.DefineFun("CxScriptCatalog_script_setparameterpolicy", (mu::strfun_type1)&CxScriptCatalog_script_setparameterpolicy);
    parser.DefineFun("CxScriptCatalog_script_setparameterrole", (mu::strfun_type1)&CxScriptCatalog_script_setparameterrole);
    parser.DefineFun("CxScriptCatalog_script_setexpected", (mu::strfun_type1)&CxScriptCatalog_script_setexpected);
    parser.DefineFun("CxScriptCatalog_script_setexpectedstatus", (mu::strfun_type1)&CxScriptCatalog_script_setexpectedstatus);
    parser.DefineFun("CxScriptCatalog_script_setexpectedpolicyguard", (mu::strfun_type1)&CxScriptCatalog_script_setexpectedpolicyguard);
    parser.DefineFun("CxScriptCatalog_script_setcontract", (mu::strfun_type1)&CxScriptCatalog_script_setcontract);
    parser.DefineFun("CxScriptCatalog_script_setfrozen", (mu::fun_type1)&CxScriptCatalog_script_setfrozen);
    parser.DefineFun("CxScriptCatalog_script_setmanualvisible", (mu::fun_type1)&CxScriptCatalog_script_setmanualvisible);
    parser.DefineFun("CxScriptCatalog_script_setregressionvisible", (mu::fun_type1)&CxScriptCatalog_script_setregressionvisible);
    parser.DefineFun("CxScriptCatalog_script_setadvancedvisible", (mu::fun_type1)&CxScriptCatalog_script_setadvancedvisible);
}