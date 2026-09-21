#include "muParser.h"
#include "CxParameterProfileRuntime.h"
#include "CxParameterProfileRegister.h"

CxParameterProfileRuntime g_cxscript_parameter_profile;
CxParameterProfile* g_current_parameter_profile = nullptr;

double CxParameterProfile_reset(double)
{
    g_cxscript_parameter_profile = CxParameterProfileRuntime{};
    g_current_parameter_profile = nullptr;
    return 0.0;
}

double CxParameterProfile_settool(const char* tool)
{
    g_cxscript_parameter_profile.current_tool = tool ? tool : "";
    return 0.0;
}

double CxParameterProfile_add(const char* profile_id)
{
    if (!profile_id || profile_id[0] == '\0')
        return 0.0;

    CxParameterProfile profile;
    profile.profile_id = profile_id;
    profile.tool = g_cxscript_parameter_profile.current_tool;
    g_cxscript_parameter_profile.profiles.push_back(profile);
    g_current_parameter_profile = &g_cxscript_parameter_profile.profiles.back();
    return 0.0;
}

double CxParameterProfile_setrole(const char* role)
{
    if (!g_current_parameter_profile)
        return 0.0;

    g_current_parameter_profile->role = role ? role : "";
    return 0.0;
}

double CxParameterProfile_setmethod(double method)
{
    if (!g_current_parameter_profile)
        return 0.0;

    g_current_parameter_profile->method = static_cast<int>(method);
    g_current_parameter_profile->has_method = true;
    return 0.0;
}

double CxParameterProfile_setthreshold(double threshold)
{
    if (!g_current_parameter_profile)
        return 0.0;

    g_current_parameter_profile->threshold = static_cast<int>(threshold);
    g_current_parameter_profile->has_threshold = true;
    return 0.0;
}

double CxParameterProfile_setgap(double gap)
{
    if (!g_current_parameter_profile)
        return 0.0;

    g_current_parameter_profile->gap = static_cast<int>(gap);
    g_current_parameter_profile->has_gap = true;
    return 0.0;
}

double CxParameterProfile_setlinegap(double linegap)
{
    if (!g_current_parameter_profile)
        return 0.0;

    g_current_parameter_profile->linegap = static_cast<int>(linegap);
    g_current_parameter_profile->has_linegap = true;
    return 0.0;
}

double CxParameterProfile_setminedgerunwidth(double width)
{
    if (!g_current_parameter_profile)
        return 0.0;

    g_current_parameter_profile->min_edge_run_width_px =
        std::max(1, std::min(20, static_cast<int>(width)));
    g_current_parameter_profile->has_min_edge_run_width_px = true;
    return 0.0;
}

double CxParameterProfile_setwgap(double wgap)
{
    if (!g_current_parameter_profile)
        return 0.0;

    g_current_parameter_profile->wgap = static_cast<int>(wgap);
    g_current_parameter_profile->has_wgap = true;
    return 0.0;
}

double CxParameterProfile_sethgap(double hgap)
{
    if (!g_current_parameter_profile)
        return 0.0;

    g_current_parameter_profile->hgap = static_cast<int>(hgap);
    g_current_parameter_profile->has_hgap = true;
    return 0.0;
}

double CxParameterProfile_setfilterprofile(double filterprofile)
{
    if (!g_current_parameter_profile)
        return 0.0;

    g_current_parameter_profile->filterprofile = static_cast<int>(filterprofile);
    g_current_parameter_profile->has_filterprofile = true;
    return 0.0;
}

double CxParameterProfile_setdescription(const char* description)
{
    if (!g_current_parameter_profile)
        return 0.0;

    g_current_parameter_profile->description = description ? description : "";
    return 0.0;
}

void RegisterCxParameterProfileBindings(mu::Parser& parser)
{
    parser.DefineFun("CxParameterProfile_reset", (mu::fun_type1)&CxParameterProfile_reset);
    parser.DefineFun("CxParameterProfile_settool", (mu::strfun_type1)&CxParameterProfile_settool);
    parser.DefineFun("CxParameterProfile_add", (mu::strfun_type1)&CxParameterProfile_add);
    parser.DefineFun("CxParameterProfile_setrole", (mu::strfun_type1)&CxParameterProfile_setrole);
    parser.DefineFun("CxParameterProfile_setmethod", (mu::fun_type1)&CxParameterProfile_setmethod);
    parser.DefineFun("CxParameterProfile_setthreshold", (mu::fun_type1)&CxParameterProfile_setthreshold);
    parser.DefineFun("CxParameterProfile_setgap", (mu::fun_type1)&CxParameterProfile_setgap);
    parser.DefineFun("CxParameterProfile_setlinegap", (mu::fun_type1)&CxParameterProfile_setlinegap);
    parser.DefineFun("CxParameterProfile_setminedgerunwidth",
                     (mu::fun_type1)&CxParameterProfile_setminedgerunwidth);
    parser.DefineFun("CxParameterProfile_setwgap", (mu::fun_type1)&CxParameterProfile_setwgap);
    parser.DefineFun("CxParameterProfile_sethgap", (mu::fun_type1)&CxParameterProfile_sethgap);
    parser.DefineFun("CxParameterProfile_setfilterprofile", (mu::fun_type1)&CxParameterProfile_setfilterprofile);
    parser.DefineFun("CxParameterProfile_setdescription", (mu::strfun_type1)&CxParameterProfile_setdescription);
}
