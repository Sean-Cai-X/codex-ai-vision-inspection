#include "CxScriptSuiteRuntime.h"
#include "CxScriptSuiteRegister.h"
#include "muParser.h"
#include <fstream>
#include <sstream>
#include <filesystem>

bool LoadCxScriptSuiteFile(
    const std::string& suite_path,
    CxScriptSuiteRuntime& out_suite,
    std::string& out_reason)
{
    std::filesystem::path path(suite_path);
    if (!std::filesystem::exists(path))
    {
        out_reason = "Suite file not found: " + suite_path;
        return false;
    }

    std::ifstream file(path);
    if (!file.is_open())
    {
        out_reason = "Cannot open suite file: " + suite_path;
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string suite_script = buffer.str();

    mu::Parser parser;
    parser.UsingClass(true);

    RegisterCxScriptSuiteBindings(parser);

    try
    {
        parser.SetExpr(suite_script);
        parser.Eval();
    }
    catch (const mu::Parser::exception_type& e)
    {
        out_reason = "Suite parse error: " + std::string(e.GetMsg());
        return false;
    }

    out_suite = g_cxscript_suite;

    return true;
}