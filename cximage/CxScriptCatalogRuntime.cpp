#include "CxScriptCatalogRuntime.h"
#include "CxScriptCatalogRegister.h"
#include "muParser.h"
#include <fstream>
#include <sstream>
#include <filesystem>

bool LoadCxScriptCatalogFile(
    const std::string& catalog_path,
    CxScriptCatalogRuntime& out_catalog,
    std::string& out_reason)
{
    std::filesystem::path path(catalog_path);
    if (!std::filesystem::exists(path))
    {
        out_reason = "Catalog file not found: " + catalog_path;
        return false;
    }

    std::ifstream file(path);
    if (!file.is_open())
    {
        out_reason = "Cannot open catalog file: " + catalog_path;
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string catalog_script = buffer.str();

    mu::Parser parser;
    parser.UsingClass(true);

    RegisterCxScriptCatalogBindings(parser);

    try
    {
        parser.SetExpr(catalog_script);
        parser.Eval();
    }
    catch (const mu::Parser::exception_type& e)
    {
        out_reason = "Catalog parse error: " + std::string(e.GetMsg());
        return false;
    }

    out_catalog = g_cxscript_catalog;

    return true;
}