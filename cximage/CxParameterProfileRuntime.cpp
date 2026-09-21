#include "CxParameterProfileRuntime.h"
#include "CxParameterProfileRegister.h"
#include "muParser.h"
#include <fstream>
#include <sstream>
#include <filesystem>

const CxParameterProfile* CxParameterProfileRuntime::FindProfile(const std::string& profile_id) const
{
    for (const auto& p : profiles)
    {
        if (p.profile_id == profile_id)
            return &p;
    }
    return nullptr;
}

const CxParameterProfile* CxParameterProfileRuntime::FindProfileByToolAndRole(const std::string& tool, const std::string& role) const
{
    for (const auto& p : profiles)
    {
        if (p.tool == tool && p.role == role)
            return &p;
    }
    return nullptr;
}

void CxParameterProfileRuntime::Clear()
{
    current_tool.clear();
    profiles.clear();
}

bool LoadCxParameterProfileFile(
    const std::string& script_path,
    CxParameterProfileRuntime& out_profiles,
    std::string& out_reason)
{
    std::filesystem::path path(script_path);
    if (!std::filesystem::exists(path))
    {
        out_reason = "Parameter profile file not found: " + script_path;
        return false;
    }

    std::ifstream file(path);
    if (!file.is_open())
    {
        out_reason = "Cannot open parameter profile file: " + script_path;
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string script = buffer.str();

    mu::Parser parser;
    parser.UsingClass(true);
    RegisterCxParameterProfileBindings(parser);

    try
    {
        parser.SetExpr(script);
        parser.Eval();
    }
    catch (const mu::Parser::exception_type& e)
    {
        out_reason = "Parameter profile parse error: " + std::string(e.GetMsg());
        return false;
    }

    out_profiles = g_cxscript_parameter_profile;
    return true;
}
