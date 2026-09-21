#ifndef CXIMAGE_CXRUNTIME_PROJECTION_TYPES_H
#define CXIMAGE_CXRUNTIME_PROJECTION_TYPES_H

#include <string>
#include <map>
#include <vector>
#include "CxScriptImageManifestRuntime.h"

struct CxShapeElementSnapshot
{
    std::string stable_ref;
    std::string owner_type;
    std::string owner_ref;
    std::string semantic_role;
    std::string shape_kind;
    std::vector<double> points;
    double center_x = 0.0;
    double center_y = 0.0;
    double radius = 0.0;
    double inner_radius = 0.0;
    double half_width = 0.0;
    double radius_x = 0.0;
    double radius_y = 0.0;
    double angle_deg = 0.0;
    bool closed = false;
    bool editable = false;
    bool result_element = false;
};

struct CxRuntimeProjectionRequest
{
    std::string case_id;
    std::string tool_id;
    std::string owner_type;
    std::string owner_ref;

    std::string image_path;

    double roi_x0 = 0.0;
    double roi_y0 = 0.0;
    double roi_x1 = 0.0;
    double roi_y1 = 0.0;

    double circle_cx = 0.0;
    double circle_cy = 0.0;
    double circle_px = 0.0;
    double circle_py = 0.0;

    int tool_half_width = 1;
    int wgap = 2;
    int hgap = 2;
    int gap = 5;
    int linegap = 3;
    int threshold = 20;
    int method = 0;
    int filter_profile = 0;

    double min_score = 0.0;
    int find_num = 1;
    int compare_gap = 0;

    bool require_algorithm_execution = false;

    bool has_ellipse_roi = false;
    double ellipse_cx = 0.0;
    double ellipse_cy = 0.0;
    double ellipse_rx = 0.0;
    double ellipse_ry = 0.0;
    double ellipse_angle_deg = 0.0;

    bool has_rotated_rect_roi = false;
    double rect_cx = 0.0;
    double rect_cy = 0.0;
    double rect_width = 0.0;
    double rect_height = 0.0;
    double rect_angle_deg = 0.0;

    std::string template_image_path;
    std::string test_image_path;

    bool has_learn_roi = false;
    CxManifestRect learn_roi;

    bool has_search_roi = false;
    CxManifestRect search_roi;

    bool has_expected_rect = false;
    CxManifestRect expected_rect;
};

struct CxRuntimeProjectionResult
{
    bool executed = false;
    bool algorithm_ok = false;
    bool publish_ok = false;

    std::string owner_type;
    std::string owner_ref;
    std::string failure_stage;
    std::string reason;

    int valid_points_count = 0;

    bool has_fit_line = false;
    bool has_fit_circle = false;
    bool has_fit_ellipse = false;
    bool has_result_rect = false;

    double fit_residual = 0.0;
    double circle_radius = 0.0;
    double avgdist = 0.0;

    int model_point_count = 0;
    int candidate_count = 0;
    double best_score = 0.0;
    bool has_result_box = false;
    bool has_best_result = false;

    std::map<std::string, int> role_counts;

    std::vector<CxShapeElementSnapshot> published_shapes;
};

#endif