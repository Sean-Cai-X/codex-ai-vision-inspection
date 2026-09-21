#include "pch.h"
#include "CxShapeTestRuntime.h"
#include "muParser.h"

static double CxShapeTest_reset(double)
{
    CxShapeTestRuntime::Reset();
    return 0.0;
}

static double CxShapeTest_addcase(const char* case_id)
{
    CxShapeTestRuntime::AddCase(case_id ? case_id : "");
    return 0.0;
}

static double CxShapeTest_settool(const char* tool_id)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
        current->tool_id = tool_id ? tool_id : "";
    return 0.0;
}

static double CxShapeTest_setoperation(const char* operation)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
        current->operation = operation ? operation : "";
    return 0.0;
}

static double CxShapeTest_sethandle(const char* handle)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
        current->handle = handle ? handle : "";
    return 0.0;
}

static double CxShapeTest_setfrom(double x, double y)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
    {
        current->from_x = x;
        current->from_y = y;
    }
    return 0.0;
}

static double CxShapeTest_setto(double x, double y)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
    {
        current->to_x = x;
        current->to_y = y;
    }
    return 0.0;
}

static double CxShapeTest_setzoom(double zoom)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
        current->zoom = zoom;
    return 0.0;
}

static double CxShapeTest_setpan(double x, double y)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
    {
        current->pan_x = x;
        current->pan_y = y;
    }
    return 0.0;
}

static double CxShapeTest_setvertex(double index)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
        current->vertex_index = static_cast<int>(index);
    return 0.0;
}

static double CxShapeTest_setexpected(const char* expected)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
        current->expected = expected ? expected : "";
    return 0.0;
}

static double CxShapeTest_setinitialpoints(const char* points_str)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current && points_str)
    {
        current->has_initial_points = true;
        current->initial_points.clear();
        
        std::string str = points_str;
        size_t pos = 0;
        while (pos < str.size())
        {
            size_t end = str.find_first_of(", ", pos);
            if (end == std::string::npos)
                end = str.size();
            
            try
            {
                current->initial_points.push_back(std::stod(str.substr(pos, end - pos)));
            }
            catch (...)
            {
            }
            
            pos = end + 1;
            while (pos < str.size() && (str[pos] == ',' || str[pos] == ' '))
                pos++;
        }
    }
    return 0.0;
}

static double CxShapeTest_setinitialrect(double x0, double y0, double x1, double y1)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
    {
        current->has_initial_rect = true;
        current->initial_rx0 = x0;
        current->initial_ry0 = y0;
        current->initial_rx1 = x1;
        current->initial_ry1 = y1;
    }
    return 0.0;
}

static double CxShapeTest_setinitialcircle(double cx, double cy, double radius)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
    {
        current->has_initial_circle = true;
        current->initial_ccx = cx;
        current->initial_ccy = cy;
        current->initial_cradius = radius;
    }
    return 0.0;
}

static double CxShapeTest_setinitialellipse(double radius_y, double radius_x, double center_y, double center_x)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
    {
        current->has_initial_ellipse = true;
        current->initial_ex = center_x;
        current->initial_ey = center_y;
        current->initial_erx = radius_x;
        current->initial_ery = radius_y;
    }
    return 0.0;
}

static double CxShapeTest_setinitialellipsecenter(double cx, double cy, double rx, double ry)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
    {
        current->has_initial_ellipse = true;
        current->initial_ex = cx;
        current->initial_ey = cy;
        current->initial_erx = rx;
        current->initial_ery = ry;
    }
    return 0.0;
}

static double CxShapeTest_setinitialline(double x0, double y0, double x1, double y1)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
    {
        current->has_initial_line = true;
        current->initial_lx0 = x0;
        current->initial_ly0 = y0;
        current->initial_lx1 = x1;
        current->initial_ly1 = y1;
    }
    return 0.0;
}

static double CxShapeTest_seteditable(double editable)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
        current->editable = (editable != 0.0);
    return 0.0;
}

static double CxShapeTest_setvisible(double visible)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
        current->visible = (visible != 0.0);
    return 0.0;
}

static double CxShapeTest_setexpectedhit(double expected)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
        current->expected_hit = static_cast<int>(expected);
    return 0.0;
}

static double CxShapeTest_setexpectedbegindrag(double expected)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
        current->expected_begin_drag = static_cast<int>(expected);
    return 0.0;
}

static double CxShapeTest_setexpectedcommit(double expected)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
        current->expected_commit = static_cast<int>(expected);
    return 0.0;
}

static double CxShapeTest_setexpectedradiusx(double expected)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
    {
        current->has_expected_radius_x = true;
        current->expected_radius_x = expected;
    }
    return 0.0;
}

static double CxShapeTest_setexpectedradiusy(double expected)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
    {
        current->has_expected_radius_y = true;
        current->expected_radius_y = expected;
    }
    return 0.0;
}

static double CxShapeTest_setpointersequence(const char* sequence)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
        current->pointer_sequence = sequence ? sequence : "";
    return 0.0;
}

static double CxShapeTest_setexpectedshapedelta(double delta)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
        current->expected_shape_count_delta = static_cast<int>(delta);
    return 0.0;
}

static double CxShapeTest_setexpectedcreatedkind(const char* kind)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
        current->expected_created_kind = kind ? kind : "";
    return 0.0;
}

static double CxShapeTest_setexpectedphase(const char* phase)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
        current->expected_phase = phase ? phase : "";
    return 0.0;
}

static double CxShapeTest_setexpectedstatus(const char* status)
{
    auto* current = CxShapeTestRuntime::Current();
    if (current)
        current->expected_status = status ? status : "";
    return 0.0;
}

static std::string s_last_test_role;

static double CxShapeTest_setrole(const char* role)
{
    s_last_test_role = role ? role : "";
    return 0.0;
}

static double CxShapeTest_expectrole(double min_count)
{
    CxShapeTestRuntime::ExpectRole(s_last_test_role, static_cast<int>(min_count));
    return 0.0;
}

static double CxShapeTest_expectrole_range(double min_count, double max_count)
{
    CxShapeTestRuntime::ExpectRole(s_last_test_role, static_cast<int>(min_count), static_cast<int>(max_count));
    return 0.0;
}

static double CxShapeTest_expecteditable(double editable)
{
    CxShapeTestRuntime::ExpectEditable(s_last_test_role, static_cast<int>(editable));
    return 0.0;
}

static double CxShapeTest_expecteditableelement(double result_element)
{
    CxShapeTestRuntime::ExpectResultElement(s_last_test_role, static_cast<int>(result_element));
    return 0.0;
}

static double CxShapeTest_setownerbinding(const char* binding)
{
    CxShapeTestRuntime::SetOwnerBinding(binding ? binding : "");
    return 0.0;
}

static double CxShapeTest_setexpectedhandle(const char* handle)
{
    CxShapeTestRuntime::SetExpectedHandle(handle ? handle : "");
    return 0.0;
}

static double CxShapeTest_setdragdelta(double dx, double dy)
{
    CxShapeTestRuntime::SetDragDelta(static_cast<int>(dx), static_cast<int>(dy));
    return 0.0;
}

static double CxShapeTest_setimage(const char* path)
{
    CxShapeTestRuntime::SetImage(path ? path : "");
    return 0.0;
}

static double CxShapeTest_setthreshold(double value)
{
    CxShapeTestRuntime::SetThreshold(static_cast<int>(value));
    return 0.0;
}

static double CxShapeTest_setmethod(double value)
{
    CxShapeTestRuntime::SetMethod(static_cast<int>(value));
    return 0.0;
}

static double CxShapeTest_setgap(double value)
{
    CxShapeTestRuntime::SetGap(static_cast<int>(value));
    return 0.0;
}

static double CxShapeTest_setlinegap(double value)
{
    CxShapeTestRuntime::SetLineGap(static_cast<int>(value));
    return 0.0;
}

static double CxShapeTest_setwhgap(double w, double h)
{
    CxShapeTestRuntime::SetWHGap(static_cast<int>(w), static_cast<int>(h));
    return 0.0;
}

static double CxShapeTest_settoolhalfwidth(double value)
{
    CxShapeTestRuntime::SetToolHalfWidth(static_cast<int>(value));
    return 0.0;
}

static double CxShapeTest_setfilterprofile(double value)
{
    CxShapeTestRuntime::SetFilterProfile(static_cast<int>(value));
    return 0.0;
}

static double CxShapeTest_expectminvalidpoints(double value)
{
    CxShapeTestRuntime::ExpectMinValidPoints(static_cast<int>(value));
    return 0.0;
}

static double CxShapeTest_expectfitline(double value)
{
    CxShapeTestRuntime::ExpectFitLine(static_cast<int>(value));
    return 0.0;
}

static double CxShapeTest_expectfitcircle(double value)
{
    CxShapeTestRuntime::ExpectFitCircle(static_cast<int>(value));
    return 0.0;
}

static double CxShapeTest_expectfitellipse(double value)
{
    CxShapeTestRuntime::ExpectFitEllipse(static_cast<int>(value));
    return 0.0;
}

static double CxShapeTest_expectresultrect(double value)
{
    CxShapeTestRuntime::ExpectResultRect(static_cast<int>(value));
    return 0.0;
}

static double CxShapeTest_expectmaxresidual(double value)
{
    CxShapeTestRuntime::ExpectMaxResidual(value);
    return 0.0;
}

static double CxShapeTest_setinitiallearnrect(double x0, double y0, double x1, double y1)
{
    CxShapeTestRuntime::SetInitialLearnRect(x0, y0, x1, y1);
    return 0.0;
}

static double CxShapeTest_setinitialsearchrect(double x0, double y0, double x1, double y1)
{
    CxShapeTestRuntime::SetInitialSearchRect(x0, y0, x1, y1);
    return 0.0;
}

static double CxShapeTest_setminscore(double value)
{
    CxShapeTestRuntime::SetMinScore(value);
    return 0.0;
}

static double CxShapeTest_setfindnum(double value)
{
    CxShapeTestRuntime::SetFindNum(static_cast<int>(value));
    return 0.0;
}

static double CxShapeTest_setcomparespace(double value)
{
    CxShapeTestRuntime::SetCompareGap(static_cast<int>(value));
    return 0.0;
}

static double CxShapeTest_expectminmodelpoints(double value)
{
    CxShapeTestRuntime::ExpectMinModelPoints(static_cast<int>(value));
    return 0.0;
}

static double CxShapeTest_expectmincandidates(double value)
{
    CxShapeTestRuntime::ExpectMinCandidates(static_cast<int>(value));
    return 0.0;
}

static double CxShapeTest_expectminbestscore(double value)
{
    CxShapeTestRuntime::ExpectMinBestScore(value);
    return 0.0;
}

static double CxShapeTest_expectresultbox(double value)
{
    CxShapeTestRuntime::ExpectResultBox(static_cast<int>(value));
    return 0.0;
}

static double CxShapeTest_setmanifest(const char* path)
{
    CxShapeTestRuntime::SetManifest(path);
    return 0.0;
}

static double CxShapeTest_setmanifesttarget(const char* combined)
{
    std::string s(combined);
    size_t comma_pos = s.find(',');
    if (comma_pos != std::string::npos)
    {
        std::string image_id = s.substr(0, comma_pos);
        std::string target_id = s.substr(comma_pos + 1);
        CxShapeTestRuntime::SetManifestTarget(image_id, target_id);
    }
    return 0.0;
}

static double CxShapeTest_setmanifestmatchcase(const char* case_id)
{
    CxShapeTestRuntime::SetManifestMatchCase(case_id);
    return 0.0;
}

static double CxShapeTest_expectgeometrykind(const char* kind)
{
    CxShapeTestRuntime::ExpectGeometryKind(kind ? kind : "");
    return 0.0;
}

static double CxShapeTest_expectpointcount(double count)
{
    CxShapeTestRuntime::ExpectGeometryPointCount(static_cast<int>(count));
    return 0.0;
}

static double CxShapeTest_expectmanifestgeometry(double enabled)
{
    CxShapeTestRuntime::ExpectManifestGeometry(enabled != 0.0);
    return 0.0;
}

void RegisterCxShapeTestBindings(mu::Parser& parser)
{
    parser.DefineFun("CxShapeTest_reset", (mu::fun_type1)&CxShapeTest_reset);
    parser.DefineFun("CxShapeTest_addcase", (mu::strfun_type1)&CxShapeTest_addcase);
    parser.DefineFun("CxShapeTest_settool", (mu::strfun_type1)&CxShapeTest_settool);
    parser.DefineFun("CxShapeTest_setoperation", (mu::strfun_type1)&CxShapeTest_setoperation);
    parser.DefineFun("CxShapeTest_sethandle", (mu::strfun_type1)&CxShapeTest_sethandle);
    parser.DefineFun("CxShapeTest_setfrom", (mu::fun_type2)&CxShapeTest_setfrom);
    parser.DefineFun("CxShapeTest_setto", (mu::fun_type2)&CxShapeTest_setto);
    parser.DefineFun("CxShapeTest_setzoom", (mu::fun_type1)&CxShapeTest_setzoom);
    parser.DefineFun("CxShapeTest_setpan", (mu::fun_type2)&CxShapeTest_setpan);
    parser.DefineFun("CxShapeTest_setvertex", (mu::fun_type1)&CxShapeTest_setvertex);
    parser.DefineFun("CxShapeTest_setexpected", (mu::strfun_type1)&CxShapeTest_setexpected);
    parser.DefineFun("CxShapeTest_setinitialpoints", (mu::strfun_type1)&CxShapeTest_setinitialpoints);
    parser.DefineFun("CxShapeTest_setinitialrect", (mu::fun_type4)&CxShapeTest_setinitialrect);
    parser.DefineFun("CxShapeTest_setinitialcircle", (mu::fun_type3)&CxShapeTest_setinitialcircle);
    parser.DefineFun("CxShapeTest_setinitialellipse", (mu::fun_type4)&CxShapeTest_setinitialellipse);
    parser.DefineFun("CxShapeTest_setinitialellipsecenter", (mu::fun_type4)&CxShapeTest_setinitialellipsecenter);
    parser.DefineFun("CxShapeTest_setinitialline", (mu::fun_type4)&CxShapeTest_setinitialline);
    parser.DefineFun("CxShapeTest_seteditable", (mu::fun_type1)&CxShapeTest_seteditable);
    parser.DefineFun("CxShapeTest_setvisible", (mu::fun_type1)&CxShapeTest_setvisible);
    parser.DefineFun("CxShapeTest_setexpectedhit", (mu::fun_type1)&CxShapeTest_setexpectedhit);
    parser.DefineFun("CxShapeTest_setexpectedbegindrag", (mu::fun_type1)&CxShapeTest_setexpectedbegindrag);
    parser.DefineFun("CxShapeTest_setexpectedcommit", (mu::fun_type1)&CxShapeTest_setexpectedcommit);
    parser.DefineFun("CxShapeTest_setexpectedradiusx", (mu::fun_type1)&CxShapeTest_setexpectedradiusx);
    parser.DefineFun("CxShapeTest_setexpectedradiusy", (mu::fun_type1)&CxShapeTest_setexpectedradiusy);
    parser.DefineFun("CxShapeTest_setpointersequence", (mu::strfun_type1)&CxShapeTest_setpointersequence);
    parser.DefineFun("CxShapeTest_setexpectedshapedelta", (mu::fun_type1)&CxShapeTest_setexpectedshapedelta);
    parser.DefineFun("CxShapeTest_setexpectedcreatedkind", (mu::strfun_type1)&CxShapeTest_setexpectedcreatedkind);
    parser.DefineFun("CxShapeTest_setexpectedphase", (mu::strfun_type1)&CxShapeTest_setexpectedphase);
    parser.DefineFun("CxShapeTest_setexpectedstatus", (mu::strfun_type1)&CxShapeTest_setexpectedstatus);
    parser.DefineFun("CxShapeTest_setrole", (mu::strfun_type1)&CxShapeTest_setrole);
    parser.DefineFun("CxShapeTest_expectrole", (mu::fun_type1)&CxShapeTest_expectrole);
    parser.DefineFun("CxShapeTest_expectrole_range", (mu::fun_type2)&CxShapeTest_expectrole_range);
    parser.DefineFun("CxShapeTest_expecteditable", (mu::fun_type1)&CxShapeTest_expecteditable);
    parser.DefineFun("CxShapeTest_expecteditableelement", (mu::fun_type1)&CxShapeTest_expecteditableelement);
    parser.DefineFun("CxShapeTest_setownerbinding", (mu::strfun_type1)&CxShapeTest_setownerbinding);
    parser.DefineFun("CxShapeTest_setexpectedhandle", (mu::strfun_type1)&CxShapeTest_setexpectedhandle);
    parser.DefineFun("CxShapeTest_setdragdelta", (mu::fun_type2)&CxShapeTest_setdragdelta);
    parser.DefineFun("CxShapeTest_setimage", (mu::strfun_type1)&CxShapeTest_setimage);
    parser.DefineFun("CxShapeTest_setthreshold", (mu::fun_type1)&CxShapeTest_setthreshold);
    parser.DefineFun("CxShapeTest_setmethod", (mu::fun_type1)&CxShapeTest_setmethod);
    parser.DefineFun("CxShapeTest_setgap", (mu::fun_type1)&CxShapeTest_setgap);
    parser.DefineFun("CxShapeTest_setlinegap", (mu::fun_type1)&CxShapeTest_setlinegap);
    parser.DefineFun("CxShapeTest_setwhgap", (mu::fun_type2)&CxShapeTest_setwhgap);
    parser.DefineFun("CxShapeTest_settoolhalfwidth", (mu::fun_type1)&CxShapeTest_settoolhalfwidth);
    parser.DefineFun("CxShapeTest_setfilterprofile", (mu::fun_type1)&CxShapeTest_setfilterprofile);
    parser.DefineFun("CxShapeTest_expectminvalidpoints", (mu::fun_type1)&CxShapeTest_expectminvalidpoints);
    parser.DefineFun("CxShapeTest_expectfitline", (mu::fun_type1)&CxShapeTest_expectfitline);
    parser.DefineFun("CxShapeTest_expectfitcircle", (mu::fun_type1)&CxShapeTest_expectfitcircle);
    parser.DefineFun("CxShapeTest_expectfitellipse", (mu::fun_type1)&CxShapeTest_expectfitellipse);
    parser.DefineFun("CxShapeTest_expectresultrect", (mu::fun_type1)&CxShapeTest_expectresultrect);
    parser.DefineFun("CxShapeTest_expectmaxresidual", (mu::fun_type1)&CxShapeTest_expectmaxresidual);
    parser.DefineFun("CxShapeTest_setinitiallearnrect", (mu::fun_type4)&CxShapeTest_setinitiallearnrect);
    parser.DefineFun("CxShapeTest_setinitialsearchrect", (mu::fun_type4)&CxShapeTest_setinitialsearchrect);
    parser.DefineFun("CxShapeTest_setminscore", (mu::fun_type1)&CxShapeTest_setminscore);
    parser.DefineFun("CxShapeTest_setfindnum", (mu::fun_type1)&CxShapeTest_setfindnum);
    parser.DefineFun("CxShapeTest_setcomparespace", (mu::fun_type1)&CxShapeTest_setcomparespace);
    parser.DefineFun("CxShapeTest_expectminmodelpoints", (mu::fun_type1)&CxShapeTest_expectminmodelpoints);
    parser.DefineFun("CxShapeTest_expectmincandidates", (mu::fun_type1)&CxShapeTest_expectmincandidates);
    parser.DefineFun("CxShapeTest_expectminbestscore", (mu::fun_type1)&CxShapeTest_expectminbestscore);
    parser.DefineFun("CxShapeTest_expectresultbox", (mu::fun_type1)&CxShapeTest_expectresultbox);
    parser.DefineFun("CxShapeTest_setmanifest", (mu::strfun_type1)&CxShapeTest_setmanifest);
    parser.DefineFun("CxShapeTest_setmanifesttarget", (mu::strfun_type1)&CxShapeTest_setmanifesttarget);
    parser.DefineFun("CxShapeTest_setmanifestmatchcase", (mu::strfun_type1)&CxShapeTest_setmanifestmatchcase);
    parser.DefineFun("CxShapeTest_expectgeometrykind", (mu::strfun_type1)&CxShapeTest_expectgeometrykind);
    parser.DefineFun("CxShapeTest_expectpointcount", (mu::fun_type1)&CxShapeTest_expectpointcount);
    parser.DefineFun("CxShapeTest_expectmanifestgeometry", (mu::fun_type1)&CxShapeTest_expectmanifestgeometry);
}
