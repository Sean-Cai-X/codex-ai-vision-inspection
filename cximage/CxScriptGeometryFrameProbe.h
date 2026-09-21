#ifndef CXIMAGE_CXSCRIPT_GEOMETRY_FRAME_PROBE_H
#define CXIMAGE_CXSCRIPT_GEOMETRY_FRAME_PROBE_H

#include <filesystem>
#include <string>
#include <vector>

struct GaugeFrameProbeOptions
{
    bool enabled = false;
    std::filesystem::path image_path;
    std::filesystem::path script_path;
    std::filesystem::path out_root;
    std::string case_name;
};

struct GaugeFrameProbeResult
{
    bool ok = false;
    int exit_code = 1;
    std::string reason;
    std::string tool;
    std::filesystem::path frame_black_path;
    std::filesystem::path frame_on_image_path;
    std::filesystem::path frame_geometry_path;
    std::filesystem::path frame_report_path;
    std::filesystem::path snapshot_path;
};

struct GaugePoint2d
{
    double x = 0.0;
    double y = 0.0;
};

struct GaugeLineFrameProbe
{
    std::string tool = "FindLine";
    int image_width = 0;
    int image_height = 0;
    double x0 = 0.0;
    double y0 = 0.0;
    double x1 = 0.0;
    double y1 = 0.0;
    double tool_half_width = 0.0;
    double line_length = 0.0;
    double unit_dx = 0.0;
    double unit_dy = 0.0;
    double normal_x = 0.0;
    double normal_y = 0.0;
    int linegap = 0;
    int wgap = 0;
    int hgap = 0;
    int scan_line_count = 0;
    double scan_line_length = 0.0;
    bool roi_intersects_image = false;
    bool roi_fully_inside_image = false;
    bool runtime_has_scan_box = false;
    std::vector<GaugePoint2d> rect;
    std::vector<GaugePoint2d> runtime_rect;
    std::string frame_compare_status;
    std::string frame_compare_reason;
};

struct GaugeCircleFrameProbe
{
    std::string tool = "Findcircle";
    int image_width = 0;
    int image_height = 0;
    double cx = 0.0;
    double cy = 0.0;
    double px = 0.0;
    double py = 0.0;
    double radius = 0.0;
    int gap = 0;
    int linegap = 0;
    int scan_line_count = 0;
    bool circle_intersects_image = false;
    bool circle_fully_inside_image = false;
    std::string frame_compare_status;
    std::string frame_compare_reason;
};

struct GaugeCircleRingFrameProbe
{
    std::string tool = "CircleRingGauge";
    int image_width = 0;
    int image_height = 0;
    double outer_cx = 0.0;
    double outer_cy = 0.0;
    double outer_px = 0.0;
    double outer_py = 0.0;
    double outer_radius = 0.0;
    double inner_cx = 0.0;
    double inner_cy = 0.0;
    double inner_px = 0.0;
    double inner_py = 0.0;
    double inner_radius = 0.0;
    double center_distance = 0.0;
    double ring_thickness = 0.0;
    int outer_gap = 0;
    int inner_gap = 0;
    int linegap = 0;
    int scan_line_count = 0;
    bool outer_circle_intersects_image = false;
    bool outer_circle_fully_inside_image = false;
    bool inner_circle_intersects_image = false;
    bool inner_circle_fully_inside_image = false;
    bool ring_geometry_valid = false;
    std::string frame_compare_status;
    std::string frame_compare_reason;
};
bool ParseGaugeFrameProbeArgs(int argc, char** argv, GaugeFrameProbeOptions& options);
bool RunGaugeFrameProbe(const GaugeFrameProbeOptions& options, GaugeFrameProbeResult& result);

#endif
