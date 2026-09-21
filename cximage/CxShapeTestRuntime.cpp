#include "pch.h"
#include "CxShapeTestRuntime.h"

std::vector<CxShapeTestCase> CxShapeTestRuntime::s_cases;
CxShapeTestCase* CxShapeTestRuntime::s_current = nullptr;

void CxShapeTestRuntime::Reset()
{
    s_cases.clear();
    s_current = nullptr;
}

void CxShapeTestRuntime::AddCase(const std::string& case_id)
{
    s_cases.push_back({});
    s_cases.back().case_id = case_id;
    s_current = &s_cases.back();
}

CxShapeTestCase* CxShapeTestRuntime::Current()
{
    return s_current;
}

const std::vector<CxShapeTestCase>& CxShapeTestRuntime::Cases()
{
    return s_cases;
}

const CxShapeTestCase* CxShapeTestRuntime::FindCase(const std::string& case_id)
{
    for (const auto& c : s_cases)
    {
        if (c.case_id == case_id)
            return &c;
    }
    return nullptr;
}

void CxShapeTestRuntime::ExpectRole(const std::string& role, int min_count, int max_count)
{
    if (s_current)
    {
        for (auto& exp : s_current->role_expectations)
        {
            if (exp.role == role)
            {
                exp.min_count = min_count;
                exp.max_count = max_count;
                return;
            }
        }
        s_current->role_expectations.push_back({role, min_count, max_count, -1, -1});
    }
}

void CxShapeTestRuntime::ExpectEditable(const std::string& role, int editable)
{
    if (s_current)
    {
        for (auto& exp : s_current->role_expectations)
        {
            if (exp.role == role)
            {
                exp.require_editable = editable;
                return;
            }
        }
        s_current->role_expectations.push_back({role, 0, -1, editable, -1});
    }
}

void CxShapeTestRuntime::ExpectResultElement(const std::string& role, int result_element)
{
    if (s_current)
    {
        for (auto& exp : s_current->role_expectations)
        {
            if (exp.role == role)
            {
                exp.require_result_element = result_element;
                return;
            }
        }
        s_current->role_expectations.push_back({role, 0, -1, -1, result_element});
    }
}

void CxShapeTestRuntime::SetOwnerBinding(const std::string& binding)
{
    if (s_current)
    {
        s_current->owner_binding = binding;
    }
}

void CxShapeTestRuntime::SetExpectedHandle(const std::string& handle)
{
    if (s_current)
    {
        s_current->expected_handle = handle;
    }
}

void CxShapeTestRuntime::SetDragDelta(int dx, int dy)
{
    if (s_current)
    {
        s_current->drag_delta_x = dx;
        s_current->drag_delta_y = dy;
    }
}

void CxShapeTestRuntime::SetImage(const std::string& path)
{
    if (s_current)
        s_current->image_path = path;
}

void CxShapeTestRuntime::SetInitialLine(double x0, double y0, double x1, double y1)
{
    if (s_current)
    {
        s_current->has_initial_line = true;
        s_current->initial_lx0 = x0;
        s_current->initial_ly0 = y0;
        s_current->initial_lx1 = x1;
        s_current->initial_ly1 = y1;
    }
}

void CxShapeTestRuntime::SetThreshold(int value)
{
    if (s_current)
        s_current->threshold = value;
}

void CxShapeTestRuntime::SetMethod(int value)
{
    if (s_current)
        s_current->method = value;
}

void CxShapeTestRuntime::SetGap(int value)
{
    if (s_current)
        s_current->gap = value;
}

void CxShapeTestRuntime::SetLineGap(int value)
{
    if (s_current)
        s_current->linegap = value;
}

void CxShapeTestRuntime::SetWHGap(int w, int h)
{
    if (s_current)
    {
        s_current->wgap = w;
        s_current->hgap = h;
    }
}

void CxShapeTestRuntime::SetToolHalfWidth(int value)
{
    if (s_current)
        s_current->tool_half_width = value;
}

void CxShapeTestRuntime::SetFilterProfile(int value)
{
    if (s_current)
        s_current->filter_profile = value;
}

void CxShapeTestRuntime::ExpectMinValidPoints(int value)
{
    if (s_current)
        s_current->expected_min_valid_points = value;
}

void CxShapeTestRuntime::ExpectFitLine(int value)
{
    if (s_current)
        s_current->expected_has_fit_line = value;
}

void CxShapeTestRuntime::ExpectFitCircle(int value)
{
    if (s_current)
        s_current->expected_has_fit_circle = value;
}

void CxShapeTestRuntime::ExpectFitEllipse(int value)
{
    if (s_current)
        s_current->expected_has_fit_ellipse = value;
}

void CxShapeTestRuntime::ExpectResultRect(int value)
{
    if (s_current)
        s_current->expected_has_result_rect = value;
}

void CxShapeTestRuntime::ExpectMaxResidual(double value)
{
    if (s_current)
        s_current->expected_max_residual = value;
}

void CxShapeTestRuntime::SetInitialLearnRect(double x0, double y0, double x1, double y1)
{
    if (s_current)
    {
        s_current->has_initial_learn_rect = true;
        s_current->initial_learn_x0 = x0;
        s_current->initial_learn_y0 = y0;
        s_current->initial_learn_x1 = x1;
        s_current->initial_learn_y1 = y1;
    }
}

void CxShapeTestRuntime::SetInitialSearchRect(double x0, double y0, double x1, double y1)
{
    if (s_current)
    {
        s_current->has_initial_search_rect = true;
        s_current->initial_search_x0 = x0;
        s_current->initial_search_y0 = y0;
        s_current->initial_search_x1 = x1;
        s_current->initial_search_y1 = y1;
    }
}

void CxShapeTestRuntime::SetMinScore(double value)
{
    if (s_current)
        s_current->min_score = value;
}

void CxShapeTestRuntime::SetFindNum(int value)
{
    if (s_current)
        s_current->find_num = value;
}

void CxShapeTestRuntime::SetCompareGap(int value)
{
    if (s_current)
        s_current->compare_gap = value;
}

void CxShapeTestRuntime::ExpectMinModelPoints(int value)
{
    if (s_current)
        s_current->expected_min_model_points = value;
}

void CxShapeTestRuntime::ExpectMinCandidates(int value)
{
    if (s_current)
        s_current->expected_min_candidates = value;
}

void CxShapeTestRuntime::ExpectMinBestScore(double value)
{
    if (s_current)
        s_current->expected_min_best_score = value;
}

void CxShapeTestRuntime::ExpectResultBox(int value)
{
    if (s_current)
        s_current->expected_has_result_box = value;
}

void CxShapeTestRuntime::SetManifest(const std::string& path)
{
    if (s_current)
        s_current->image_manifest_path = path;
}

void CxShapeTestRuntime::SetManifestTarget(const std::string& image_id, const std::string& target_id)
{
    if (s_current)
    {
        s_current->manifest_image_id = image_id;
        s_current->manifest_target_id = target_id;
    }
}

void CxShapeTestRuntime::SetManifestMatchCase(const std::string& case_id)
{
    if (s_current)
        s_current->manifest_match_case_id = case_id;
}

void CxShapeTestRuntime::ExpectGeometryKind(const std::string& kind)
{
    if (s_current)
        s_current->expected_geometry_kind = kind;
}

void CxShapeTestRuntime::ExpectGeometryPointCount(int count)
{
    if (s_current)
        s_current->expected_geometry_point_count = count;
}

void CxShapeTestRuntime::ExpectManifestGeometry(bool enabled)
{
    if (s_current)
        s_current->expect_geometry_from_manifest = enabled;
}
