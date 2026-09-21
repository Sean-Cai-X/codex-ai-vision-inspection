#ifndef CXIMAGE_CXSCRIPT_CATALOG_RUNTIME_H
#define CXIMAGE_CXSCRIPT_CATALOG_RUNTIME_H

#include <string>
#include <vector>

struct CxScriptCatalogEntry
{
    std::string script_id;
    std::string label;
    std::string path;
    std::string tool;
    std::string orientation;
    std::string parameter_policy_id;
    std::string parameter_role;
    std::string expected_result;
    std::string expected_status;
    std::string expected_policy_guard;
    std::string contract_path;

    bool frozen = false;
    bool manual_visible = false;
    bool regression_visible = false;
    bool advanced_visible = false;
};

struct CxScriptCatalogRuntime
{
    std::string name;
    std::string version;
    std::vector<CxScriptCatalogEntry> scripts;
};

bool LoadCxScriptCatalogFile(
    const std::string& catalog_path,
    CxScriptCatalogRuntime& out_catalog,
    std::string& out_reason);

#endif