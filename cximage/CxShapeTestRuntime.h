#ifndef CXSHAPETESTRUNTIME_H
#define CXSHAPETESTRUNTIME_H

#include <string>
#include <vector>
#include <map>

struct CxShapeRoleExpectation
{
    std::string role;
    int min_count = 0;
    int max_count = -1;
    int require_editable = -1;
    int require_result_element = -1;
};

struct CxShapeTestCase
{
    std::string case_id;
    std::string tool_id;
    std::string operation;
    std::string handle;
    std::string expected;

    double from_x = 0.0;
    double from_y = 0.0;
    double to_x = 0.0;
    double to_y = 0.0;

    double zoom = 1.0;
    double pan_x = 0.0;
    double pan_y = 0.0;
    int vertex_index = -1;

    bool has_initial_points = false;
    std::vector<double> initial_points;

    bool has_initial_rect = false;
    double initial_rx0 = 0.0;
    double initial_ry0 = 0.0;
    double initial_rx1 = 0.0;
    double initial_ry1 = 0.0;

    bool has_initial_circle = false;
    double initial_ccx = 0.0;
    double initial_ccy = 0.0;
    double initial_cradius = 0.0;

    bool has_initial_ellipse = false;
    double initial_ex = 0.0;
    double initial_ey = 0.0;
    double initial_erx = 0.0;
    double initial_ery = 0.0;

    bool has_initial_line = false;
    double initial_lx0 = 0.0;
    double initial_ly0 = 0.0;
    double initial_lx1 = 0.0;
    double initial_ly1 = 0.0;

    bool editable = true;
    bool visible = true;

    int expected_hit = 1;
    int expected_begin_drag = 1;
    int expected_commit = 1;

    bool has_expected_radius_x = false;
    bool has_expected_radius_y = false;
    double expected_radius_x = 0.0;
    double expected_radius_y = 0.0;

    std::string pointer_sequence;
    int expected_shape_count_delta = 0;
    std::string expected_created_kind;
    std::string expected_phase;
    std::string expected_status;

    std::vector<CxShapeRoleExpectation> role_expectations;

    std::string owner_binding;
    std::string expected_handle;
    int drag_delta_x = 0;
    int drag_delta_y = 0;

    std::string image_path;
    std::string expected_failure_stage;

    int threshold = 20;
    int method = 0;
    int gap = 5;
    int linegap = 3;
    int wgap = 2;
    int hgap = 2;
    int tool_half_width = 10;
    int filter_profile = 0;

    int expected_min_valid_points = -1;
    int expected_has_fit_line = -1;
    int expected_has_fit_circle = -1;
    int expected_has_fit_ellipse = -1;
    int expected_has_result_rect = -1;

    double expected_min_radius = -1.0;
    double expected_max_residual = -1.0;

    bool has_initial_learn_rect = false;
    double initial_learn_x0 = 0.0;
    double initial_learn_y0 = 0.0;
    double initial_learn_x1 = 0.0;
    double initial_learn_y1 = 0.0;

    bool has_initial_search_rect = false;
    double initial_search_x0 = 0.0;
    double initial_search_y0 = 0.0;
    double initial_search_x1 = 0.0;
    double initial_search_y1 = 0.0;

    double min_score = 0.0;
    int find_num = 1;
    int compare_gap = 0;

    int expected_min_model_points = -1;
    int expected_min_candidates = -1;
    double expected_min_best_score = -1.0;
    int expected_has_result_box = -1;

    std::string image_manifest_path;
    std::string manifest_image_id;
    std::string manifest_target_id;
    std::string manifest_match_case_id;

    std::string expected_geometry_kind;
    int expected_geometry_point_count = -1;
    bool expect_geometry_from_manifest = false;
};

class CxShapeTestRuntime
{
public:
    static void Reset();
    static void AddCase(const std::string& case_id);
    static CxShapeTestCase* Current();
    static const std::vector<CxShapeTestCase>& Cases();
    static const CxShapeTestCase* FindCase(const std::string& case_id);

    static void ExpectRole(const std::string& role, int min_count, int max_count = -1);
    static void ExpectEditable(const std::string& role, int editable);
    static void ExpectResultElement(const std::string& role, int result_element);
    static void SetOwnerBinding(const std::string& binding);
    static void SetExpectedHandle(const std::string& handle);
    static void SetDragDelta(int dx, int dy);

    static void SetImage(const std::string& path);
    static void SetInitialLine(double x0, double y0, double x1, double y1);
    static void SetThreshold(int value);
    static void SetMethod(int value);
    static void SetGap(int value);
    static void SetLineGap(int value);
    static void SetWHGap(int w, int h);
    static void SetToolHalfWidth(int value);
    static void SetFilterProfile(int value);

    static void ExpectMinValidPoints(int value);
    static void ExpectFitLine(int value);
    static void ExpectFitCircle(int value);
    static void ExpectFitEllipse(int value);
    static void ExpectResultRect(int value);
    static void ExpectMaxResidual(double value);

    static void SetInitialLearnRect(double x0, double y0, double x1, double y1);
    static void SetInitialSearchRect(double x0, double y0, double x1, double y1);

    static void SetMinScore(double value);
    static void SetFindNum(int value);
    static void SetCompareGap(int value);

    static void ExpectMinModelPoints(int value);
    static void ExpectMinCandidates(int value);
    static void ExpectMinBestScore(double value);
    static void ExpectResultBox(int value);

    static void SetManifest(const std::string& path);
    static void SetManifestTarget(const std::string& image_id, const std::string& target_id);
    static void SetManifestMatchCase(const std::string& case_id);
    static void ExpectGeometryKind(const std::string& kind);
    static void ExpectGeometryPointCount(int count);
    static void ExpectManifestGeometry(bool enabled);

private:
    static std::vector<CxShapeTestCase> s_cases;
    static CxShapeTestCase* s_current;
};

#endif
