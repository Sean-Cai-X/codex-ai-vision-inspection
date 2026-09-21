#include "CxScriptGeometryFrameProbe.h"
#include "CxScriptGeometryFrameOverlay.h"
#include "FindLine.h"
#include "FindCircle.h"
#include "FormfitGauge.h"
#include <opencv2/opencv.hpp>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>

namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kTol = 1.5;

std::string Trim(std::string s)
{
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    const auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

std::vector<std::string> SplitParams(const std::string& text)
{
    std::vector<std::string> out;
    std::stringstream ss(text);
    std::string item;
    while (std::getline(ss, item, ',')) out.push_back(Trim(item));
    return out;
}

int ResolveInt(const std::map<std::string, int>& vars, const std::string& token, int fallback = 0)
{
    const std::string t = Trim(token);
    const auto it = vars.find(t);
    if (it != vars.end()) return it->second;
    try { return std::stoi(t); } catch (...) { return fallback; }
}

struct ParsedCircleSpec
{
    bool present = false;
    int cx = 0;
    int cy = 0;
    int px = 0;
    int py = 0;
    int gap = 0;
    int linegap = 0;
};

struct ParsedProbeScript
{
    std::string tool;
    std::map<std::string, int> vars;
    bool has_line = false;
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0, tool_half_width = 0;
    int wgap = 0, hgap = 0, linegap = 0;
    int cx = 0, cy = 0, px = 0, py = 0, gap = 0;
    ParsedCircleSpec outer_circle;
    ParsedCircleSpec inner_circle;
};

bool ReadText(const std::filesystem::path& path, std::string& text)
{
    std::ifstream file(path);
    if (!file.is_open()) return false;
    std::stringstream ss;
    ss << file.rdbuf();
    text = ss.str();
    return true;
}

bool ParseProbeScript(const std::filesystem::path& script, ParsedProbeScript& out, std::string& reason)
{
    std::string text;
    if (!ReadText(script, text)) { reason = "cannot read script: " + script.string(); return false; }
    std::stringstream ss(text);
    std::string line;
    std::regex intDecl(R"(^\s*int\s+([A-Za-z_][A-Za-z0-9_]*)\s*=\s*([^;]+)\s*;)");
    std::regex methodCall(R"((m_[A-Za-z0-9_]+)\.([A-Za-z_][A-Za-z0-9_]*)\((.*)\)\s*;)");
    while (std::getline(ss, line))
    {
        const std::string t = Trim(line);
        if (t.empty() || t.rfind("//", 0) == 0 || t.rfind("/*", 0) == 0 || t.rfind("*", 0) == 0) continue;
        std::smatch m;
        if (std::regex_search(t, m, intDecl))
        {
            out.vars[m[1].str()] = ResolveInt(out.vars, m[2].str(), 0);
            continue;
        }
        if (std::regex_search(t, m, methodCall))
        {
            const std::string object = m[1].str();
            const bool isOuterCircle = object.find("outer") != std::string::npos || object.find("Outer") != std::string::npos;
            const bool isInnerCircle = object.find("inner") != std::string::npos || object.find("Inner") != std::string::npos;
            const std::string method = m[2].str();
            const auto params = SplitParams(m[3].str());
            if (method == "SetWHgap" && params.size() >= 2)
            {
                out.wgap = ResolveInt(out.vars, params[0]);
                out.hgap = ResolveInt(out.vars, params[1]);
            }
            else if (method == "setlinegap" && params.size() >= 1)
            {
                out.linegap = ResolveInt(out.vars, params[0]);
                if (isOuterCircle) out.outer_circle.linegap = out.linegap;
                if (isInnerCircle) out.inner_circle.linegap = out.linegap;
            }
            else if (method == "setline" && params.size() >= 5)
            {
                out.tool = "FindLine";
                out.has_line = true;
                out.x0 = ResolveInt(out.vars, params[0]);
                out.y0 = ResolveInt(out.vars, params[1]);
                out.x1 = ResolveInt(out.vars, params[2]);
                out.y1 = ResolveInt(out.vars, params[3]);
                out.tool_half_width = ResolveInt(out.vars, params[4]);
            }
            else if (method == "Setgap" && params.size() >= 1)
            {
                out.gap = ResolveInt(out.vars, params[0]);
                if (isOuterCircle) out.outer_circle.gap = out.gap;
                if (isInnerCircle) out.inner_circle.gap = out.gap;
            }
            else if (method == "setcircle" && params.size() >= 4)
            {
                out.tool = "FindCircle";
                out.cx = ResolveInt(out.vars, params[0]);
                out.cy = ResolveInt(out.vars, params[1]);
                out.px = ResolveInt(out.vars, params[2]);
                out.py = ResolveInt(out.vars, params[3]);
                if (isOuterCircle)
                {
                    out.outer_circle.present = true;
                    out.outer_circle.cx = out.cx;
                    out.outer_circle.cy = out.cy;
                    out.outer_circle.px = out.px;
                    out.outer_circle.py = out.py;
                }
                if (isInnerCircle)
                {
                    out.inner_circle.present = true;
                    out.inner_circle.cx = out.cx;
                    out.inner_circle.cy = out.cy;
                    out.inner_circle.px = out.px;
                    out.inner_circle.py = out.py;
                }
            }
        }
    }
    if (out.outer_circle.present && out.inner_circle.present && out.has_line) out.tool = "CircleRingLineFormfitGauge";
    else if (out.outer_circle.present && out.inner_circle.present) out.tool = "CircleRingGauge";
    if (out.tool.empty()) { reason = "script does not contain setline or setcircle frame call"; return false; }
    return true;
}

bool RectIntersectsImage(const std::vector<GaugePoint2d>& rect, int w, int h)
{
    double minx = 1e30, miny = 1e30, maxx = -1e30, maxy = -1e30;
    for (const auto& p : rect) { minx = std::min(minx, p.x); miny = std::min(miny, p.y); maxx = std::max(maxx, p.x); maxy = std::max(maxy, p.y); }
    return maxx >= 0.0 && maxy >= 0.0 && minx <= (double)(w - 1) && miny <= (double)(h - 1);
}

bool RectInsideImage(const std::vector<GaugePoint2d>& rect, int w, int h)
{
    for (const auto& p : rect)
        if (p.x < 0.0 || p.y < 0.0 || p.x > (double)(w - 1) || p.y > (double)(h - 1)) return false;
    return true;
}

bool PointClose(const GaugePoint2d& a, const GaugePoint2d& b)
{
    return std::hypot(a.x - b.x, a.y - b.y) <= kTol;
}

bool PointSetCloseIgnoringOrder(const std::vector<GaugePoint2d>& a, const std::vector<GaugePoint2d>& b)
{
    if (a.size() != b.size()) return false;
    std::vector<bool> used(b.size(), false);
    for (const auto& pa : a)
    {
        bool found = false;
        for (std::size_t i = 0; i < b.size(); ++i)
        {
            if (!used[i] && PointClose(pa, b[i])) { used[i] = true; found = true; break; }
        }
        if (!found) return false;
    }
    return true;
}

GaugeLineFrameProbe BuildLineFrame(const ParsedProbeScript& s, int image_w, int image_h)
{
    GaugeLineFrameProbe p;
    p.image_width = image_w;
    p.image_height = image_h;
    p.x0 = s.x0; p.y0 = s.y0; p.x1 = s.x1; p.y1 = s.y1;
    p.tool_half_width = s.tool_half_width;
    p.linegap = s.linegap;
    p.wgap = s.wgap;
    p.hgap = s.hgap;
    const double dx = p.x1 - p.x0;
    const double dy = p.y1 - p.y0;
    p.line_length = std::hypot(dx, dy);
    if (p.line_length > 1e-9)
    {
        p.unit_dx = dx / p.line_length;
        p.unit_dy = dy / p.line_length;
        p.normal_x = -p.unit_dy;
        p.normal_y = p.unit_dx;
    }
    p.rect = {
        {p.x0 + p.normal_x * p.tool_half_width, p.y0 + p.normal_y * p.tool_half_width},
        {p.x0 - p.normal_x * p.tool_half_width, p.y0 - p.normal_y * p.tool_half_width},
        {p.x1 - p.normal_x * p.tool_half_width, p.y1 - p.normal_y * p.tool_half_width},
        {p.x1 + p.normal_x * p.tool_half_width, p.y1 + p.normal_y * p.tool_half_width}
    };
    p.scan_line_length = 2.0 * p.tool_half_width;
    p.scan_line_count = p.linegap > 0 ? std::max(1, (int)std::floor(p.line_length / p.linegap)) : 0;
    p.roi_intersects_image = RectIntersectsImage(p.rect, image_w, image_h);
    p.roi_fully_inside_image = RectInsideImage(p.rect, image_w, image_h);

    FindLine runtime;
    if (s.wgap > 0 || s.hgap > 0) runtime.SetWHgap(std::max(1, s.wgap), std::max(1, s.hgap));
    if (s.linegap > 0) runtime.setlinegap(s.linegap);
    runtime.setline(s.x0, s.y0, s.x1, s.y1, s.tool_half_width);
    FindLineDisplaySnapshot snap;
    p.runtime_has_scan_box = runtime.getdisplaysnapshot(snap) && snap.has_scan_box;
    if (p.runtime_has_scan_box)
    {
        for (int i = 0; i < 4; ++i)
            p.runtime_rect.push_back({snap.scan_box_xy[i * 2], snap.scan_box_xy[i * 2 + 1]});
        if (PointSetCloseIgnoringOrder(p.rect, p.runtime_rect))
        {
            p.frame_compare_status = "FRAME_MATCH";
            p.frame_compare_reason = "probe rect matches runtime line_scan_box_xy, corner order ignored";
        }
        else
        {
            p.frame_compare_status = "FRAME_GEOMETRY_MISMATCH";
            p.frame_compare_reason = "probe rect does not match runtime line_scan_box_xy";
        }
    }
    else
    {
        p.frame_compare_status = "RUNTIME_FRAME_UNAVAILABLE";
        p.frame_compare_reason = "FindLine runtime did not expose scan box";
    }
    if (!p.roi_intersects_image)
    {
        p.frame_compare_status = "ROI_OUT_OF_IMAGE";
        p.frame_compare_reason = "line gauge frame does not intersect image";
    }
    return p;
}

GaugeCircleFrameProbe BuildCircleFrame(const ParsedProbeScript& s, int image_w, int image_h)
{
    GaugeCircleFrameProbe p;
    p.image_width = image_w; p.image_height = image_h;
    p.cx = s.cx; p.cy = s.cy; p.px = s.px; p.py = s.py;
    p.gap = s.gap; p.linegap = s.linegap;
    p.radius = std::hypot(p.px - p.cx, p.py - p.cy);
    p.scan_line_count = p.gap > 0 ? std::max(1, 360 / p.gap) : 0;
    p.circle_intersects_image = (p.cx + p.radius >= 0.0 && p.cy + p.radius >= 0.0 && p.cx - p.radius <= image_w - 1 && p.cy - p.radius <= image_h - 1);
    p.circle_fully_inside_image = (p.cx - p.radius >= 0.0 && p.cy - p.radius >= 0.0 && p.cx + p.radius <= image_w - 1 && p.cy + p.radius <= image_h - 1);

    FindCircle runtime;
    if (s.gap > 0) runtime.Setgap(s.gap);
    if (s.linegap > 0) runtime.setlinegap(s.linegap);
    runtime.setcircle(s.cx, s.cy, s.px, s.py);
    const double rr = std::hypot((double)runtime.getcirclepax() - runtime.getcirclecentx(), (double)runtime.getcirclepay() - runtime.getcirclecenty());
    if (std::abs(runtime.getcirclecentx() - p.cx) <= kTol &&
        std::abs(runtime.getcirclecenty() - p.cy) <= kTol &&
        std::abs(runtime.getcirclepax() - p.px) <= kTol &&
        std::abs(runtime.getcirclepay() - p.py) <= kTol &&
        std::abs(rr - p.radius) <= kTol)
    {
        p.frame_compare_status = "FRAME_MATCH";
        p.frame_compare_reason = "probe circle matches runtime circle geometry";
    }
    else
    {
        p.frame_compare_status = "FRAME_GEOMETRY_MISMATCH";
        p.frame_compare_reason = "probe circle does not match runtime circle center/pass/radius";
    }
    if (!p.circle_intersects_image)
    {
        p.frame_compare_status = "ROI_OUT_OF_IMAGE";
        p.frame_compare_reason = "circle gauge frame does not intersect image";
    }
    return p;
}

bool CircleIntersectsImage(double cx, double cy, double radius, int image_w, int image_h)
{
    return cx + radius >= 0.0 && cy + radius >= 0.0 && cx - radius <= image_w - 1 && cy - radius <= image_h - 1;
}

bool CircleInsideImage(double cx, double cy, double radius, int image_w, int image_h)
{
    return cx - radius >= 0.0 && cy - radius >= 0.0 && cx + radius <= image_w - 1 && cy + radius <= image_h - 1;
}

GaugeCircleRingFrameProbe BuildCircleRingFrame(const ParsedProbeScript& s, int image_w, int image_h)
{
    GaugeCircleRingFrameProbe p;
    p.image_width = image_w;
    p.image_height = image_h;
    p.outer_cx = s.outer_circle.cx;
    p.outer_cy = s.outer_circle.cy;
    p.outer_px = s.outer_circle.px;
    p.outer_py = s.outer_circle.py;
    p.inner_cx = s.inner_circle.cx;
    p.inner_cy = s.inner_circle.cy;
    p.inner_px = s.inner_circle.px;
    p.inner_py = s.inner_circle.py;
    p.outer_gap = s.outer_circle.gap > 0 ? s.outer_circle.gap : s.gap;
    p.inner_gap = s.inner_circle.gap > 0 ? s.inner_circle.gap : s.gap;
    p.linegap = s.outer_circle.linegap > 0 ? s.outer_circle.linegap : s.linegap;
    if (p.linegap <= 0 && s.inner_circle.linegap > 0) p.linegap = s.inner_circle.linegap;
    p.outer_radius = std::hypot(p.outer_px - p.outer_cx, p.outer_py - p.outer_cy);
    p.inner_radius = std::hypot(p.inner_px - p.inner_cx, p.inner_py - p.inner_cy);
    p.center_distance = std::hypot(p.inner_cx - p.outer_cx, p.inner_cy - p.outer_cy);
    p.ring_thickness = p.outer_radius - p.inner_radius;
    const int scan_gap = p.outer_gap > 0 ? p.outer_gap : (p.inner_gap > 0 ? p.inner_gap : 5);
    p.scan_line_count = std::max(1, 360 / scan_gap);
    p.outer_circle_intersects_image = CircleIntersectsImage(p.outer_cx, p.outer_cy, p.outer_radius, image_w, image_h);
    p.outer_circle_fully_inside_image = CircleInsideImage(p.outer_cx, p.outer_cy, p.outer_radius, image_w, image_h);
    p.inner_circle_intersects_image = CircleIntersectsImage(p.inner_cx, p.inner_cy, p.inner_radius, image_w, image_h);
    p.inner_circle_fully_inside_image = CircleInsideImage(p.inner_cx, p.inner_cy, p.inner_radius, image_w, image_h);
    p.ring_geometry_valid = p.outer_radius > 0.0 && p.inner_radius > 0.0 && p.outer_radius > p.inner_radius && (p.center_distance + p.inner_radius) <= p.outer_radius;

    FindCircle outer_runtime;
    FindCircle inner_runtime;
    if (p.outer_gap > 0) outer_runtime.Setgap(p.outer_gap);
    if (p.inner_gap > 0) inner_runtime.Setgap(p.inner_gap);
    if (p.linegap > 0) { outer_runtime.setlinegap(p.linegap); inner_runtime.setlinegap(p.linegap); }
    outer_runtime.setcircle((int)p.outer_cx, (int)p.outer_cy, (int)p.outer_px, (int)p.outer_py);
    inner_runtime.setcircle((int)p.inner_cx, (int)p.inner_cy, (int)p.inner_px, (int)p.inner_py);
    const double outer_runtime_radius = std::hypot((double)outer_runtime.getcirclepax() - outer_runtime.getcirclecentx(), (double)outer_runtime.getcirclepay() - outer_runtime.getcirclecenty());
    const double inner_runtime_radius = std::hypot((double)inner_runtime.getcirclepax() - inner_runtime.getcirclecentx(), (double)inner_runtime.getcirclepay() - inner_runtime.getcirclecenty());
    const bool outer_matches = std::abs(outer_runtime.getcirclecentx() - p.outer_cx) <= kTol &&
                               std::abs(outer_runtime.getcirclecenty() - p.outer_cy) <= kTol &&
                               std::abs(outer_runtime_radius - p.outer_radius) <= kTol;
    const bool inner_matches = std::abs(inner_runtime.getcirclecentx() - p.inner_cx) <= kTol &&
                               std::abs(inner_runtime.getcirclecenty() - p.inner_cy) <= kTol &&
                               std::abs(inner_runtime_radius - p.inner_radius) <= kTol;
    if (!p.outer_circle_intersects_image || !p.inner_circle_intersects_image)
    {
        p.frame_compare_status = "ROI_OUT_OF_IMAGE";
        p.frame_compare_reason = "outer or inner circle frame does not intersect image";
    }
    else if (!p.ring_geometry_valid)
    {
        p.frame_compare_status = "RING_GEOMETRY_INVALID";
        p.frame_compare_reason = "inner circle is not fully inside outer circle or radius is invalid";
    }
    else if (outer_matches && inner_matches)
    {
        p.frame_compare_status = "FRAME_MATCH";
        p.frame_compare_reason = "outer and inner probe circles match runtime circle geometry";
    }
    else
    {
        p.frame_compare_status = "FRAME_GEOMETRY_MISMATCH";
        p.frame_compare_reason = "outer or inner probe circle does not match runtime circle geometry";
    }
    return p;
}

cxcore::CircleMeasurementOutput MakeCircleOutputFromProbe(double cx, double cy, double radius)
{
    cxcore::CircleMeasurementOutput output;
    output.center.x = cx;
    output.center.y = cy;
    output.radius = radius;
    output.average_distance = radius;
    output.has_direct_fit = true;
    output.measure_bounds.x = cx - radius;
    output.measure_bounds.y = cy - radius;
    output.measure_bounds.width = radius * 2.0;
    output.measure_bounds.height = radius * 2.0;
    return output;
}

cxcore::LineMeasurementOutput MakeLineOutputFromProbe(const GaugeLineFrameProbe& line)
{
    cxcore::LineMeasurementOutput output;
    double minx = 1e30, miny = 1e30, maxx = -1e30, maxy = -1e30;
    for (const GaugePoint2d& p : line.rect)
    {
        minx = std::min(minx, p.x);
        miny = std::min(miny, p.y);
        maxx = std::max(maxx, p.x);
        maxy = std::max(maxy, p.y);
    }
    if (line.rect.empty())
    {
        minx = std::min(line.x0, line.x1);
        miny = std::min(line.y0, line.y1);
        maxx = std::max(line.x0, line.x1);
        maxy = std::max(line.y0, line.y1);
    }
    output.measure_bounds.x = minx;
    output.measure_bounds.y = miny;
    output.measure_bounds.width = std::max(1.0, maxx - minx);
    output.measure_bounds.height = std::max(1.0, maxy - miny);
    return output;
}

void WriteLineJson(const GaugeLineFrameProbe& p, const std::filesystem::path& path)
{
    std::ofstream f(path);
    f << "{\n";
    f << "  \"tool\": \"FindLine\",\n";
    f << "  \"image_width\": " << p.image_width << ",\n";
    f << "  \"image_height\": " << p.image_height << ",\n";
    f << "  \"x0\": " << p.x0 << ", \"y0\": " << p.y0 << ",\n";
    f << "  \"x1\": " << p.x1 << ", \"y1\": " << p.y1 << ",\n";
    f << "  \"tool_half_width\": " << p.tool_half_width << ",\n";
    f << "  \"line_length\": " << p.line_length << ",\n";
    f << "  \"unit_direction\": [" << p.unit_dx << ", " << p.unit_dy << "],\n";
    f << "  \"unit_normal\": [" << p.normal_x << ", " << p.normal_y << "],\n";
    f << "  \"rect\": [\n";
    for (std::size_t i = 0; i < p.rect.size(); ++i)
        f << "    [" << p.rect[i].x << ", " << p.rect[i].y << "]" << (i + 1 < p.rect.size() ? "," : "") << "\n";
    f << "  ],\n";
    f << "  \"runtime_rect\": [\n";
    for (std::size_t i = 0; i < p.runtime_rect.size(); ++i)
        f << "    [" << p.runtime_rect[i].x << ", " << p.runtime_rect[i].y << "]" << (i + 1 < p.runtime_rect.size() ? "," : "") << "\n";
    f << "  ],\n";
    f << "  \"roi_intersects_image\": " << (p.roi_intersects_image ? "true" : "false") << ",\n";
    f << "  \"roi_fully_inside_image\": " << (p.roi_fully_inside_image ? "true" : "false") << ",\n";
    f << "  \"linegap\": " << p.linegap << ",\n";
    f << "  \"wgap\": " << p.wgap << ",\n";
    f << "  \"hgap\": " << p.hgap << ",\n";
    f << "  \"scan_line_count\": " << p.scan_line_count << ",\n";
    f << "  \"scan_line_direction\": \"normal_to_center_line\",\n";
    f << "  \"scan_line_length\": " << p.scan_line_length << ",\n";
    f << "  \"coordinate_rule\": \"image coordinate: x right, y down\",\n";
    f << "  \"orientation_rule\": \"line rectangle = center line +/- normal * half_width\",\n";
    f << "  \"frame_compare_status\": \"" << p.frame_compare_status << "\",\n";
    f << "  \"frame_compare_reason\": \"" << p.frame_compare_reason << "\"\n";
    f << "}\n";
}

void WriteCircleJson(const GaugeCircleFrameProbe& p, const std::filesystem::path& path)
{
    std::ofstream f(path);
    f << "{\n";
    f << "  \"tool\": \"FindCircle\",\n";
    f << "  \"image_width\": " << p.image_width << ",\n";
    f << "  \"image_height\": " << p.image_height << ",\n";
    f << "  \"cx\": " << p.cx << ", \"cy\": " << p.cy << ",\n";
    f << "  \"px\": " << p.px << ", \"py\": " << p.py << ",\n";
    f << "  \"radius\": " << p.radius << ",\n";
    f << "  \"gap\": " << p.gap << ",\n";
    f << "  \"linegap\": " << p.linegap << ",\n";
    f << "  \"circle_intersects_image\": " << (p.circle_intersects_image ? "true" : "false") << ",\n";
    f << "  \"circle_fully_inside_image\": " << (p.circle_fully_inside_image ? "true" : "false") << ",\n";
    f << "  \"scan_line_count\": " << p.scan_line_count << ",\n";
    f << "  \"scan_line_direction\": \"radial\",\n";
    f << "  \"coordinate_rule\": \"image coordinate: x right, y down\",\n";
    f << "  \"orientation_rule\": \"circle = center + radius from pass point\",\n";
    f << "  \"frame_compare_status\": \"" << p.frame_compare_status << "\",\n";
    f << "  \"frame_compare_reason\": \"" << p.frame_compare_reason << "\"\n";
    f << "}\n";
}

void WriteCircleRingJson(const GaugeCircleRingFrameProbe& p, const std::filesystem::path& path)
{
    std::ofstream f(path);
    f << "{\n";
    f << "  \"tool\": \"CircleRingGauge\",\n";
    f << "  \"image_width\": " << p.image_width << ",\n";
    f << "  \"image_height\": " << p.image_height << ",\n";
    f << "  \"outer\": {\n";
    f << "    \"cx\": " << p.outer_cx << ", \"cy\": " << p.outer_cy << ",\n";
    f << "    \"px\": " << p.outer_px << ", \"py\": " << p.outer_py << ",\n";
    f << "    \"radius\": " << p.outer_radius << ",\n";
    f << "    \"gap\": " << p.outer_gap << ",\n";
    f << "    \"circle_intersects_image\": " << (p.outer_circle_intersects_image ? "true" : "false") << ",\n";
    f << "    \"circle_fully_inside_image\": " << (p.outer_circle_fully_inside_image ? "true" : "false") << "\n";
    f << "  },\n";
    f << "  \"inner\": {\n";
    f << "    \"cx\": " << p.inner_cx << ", \"cy\": " << p.inner_cy << ",\n";
    f << "    \"px\": " << p.inner_px << ", \"py\": " << p.inner_py << ",\n";
    f << "    \"radius\": " << p.inner_radius << ",\n";
    f << "    \"gap\": " << p.inner_gap << ",\n";
    f << "    \"circle_intersects_image\": " << (p.inner_circle_intersects_image ? "true" : "false") << ",\n";
    f << "    \"circle_fully_inside_image\": " << (p.inner_circle_fully_inside_image ? "true" : "false") << "\n";
    f << "  },\n";
    f << "  \"center_distance\": " << p.center_distance << ",\n";
    f << "  \"ring_thickness\": " << p.ring_thickness << ",\n";
    f << "  \"ring_geometry_valid\": " << (p.ring_geometry_valid ? "true" : "false") << ",\n";
    f << "  \"linegap\": " << p.linegap << ",\n";
    f << "  \"scan_line_count\": " << p.scan_line_count << ",\n";
    f << "  \"scan_line_direction\": \"radial_inner_to_outer\",\n";
    f << "  \"coordinate_rule\": \"image coordinate: x right, y down\",\n";
    f << "  \"orientation_rule\": \"ring = outer circle minus inner circle, scan lines run from inner circle to outer circle\",\n";
    f << "  \"frame_compare_status\": \"" << p.frame_compare_status << "\",\n";
    f << "  \"frame_compare_reason\": \"" << p.frame_compare_reason << "\"\n";
    f << "}\n";
}

void WriteCircleRingLineFormfitJson(const GaugeCircleRingFrameProbe& ring,
                                    const GaugeLineFrameProbe& line,
                                    const cxcore::formfit::FormfitGauge& gauge,
                                    const std::filesystem::path& path)
{
    std::ofstream f(path);
    f << "{\n";
    f << "  \"tool\": \"CircleRingLineFormfitGauge\",\n";
    f << "  \"image_width\": " << ring.image_width << ",\n";
    f << "  \"image_height\": " << ring.image_height << ",\n";
    f << "  \"ring\": {\n";
    f << "    \"outer_radius\": " << ring.outer_radius << ",\n";
    f << "    \"inner_radius\": " << ring.inner_radius << ",\n";
    f << "    \"ring_thickness\": " << ring.ring_thickness << ",\n";
    f << "    \"center_distance\": " << ring.center_distance << ",\n";
    f << "    \"ring_geometry_valid\": " << (ring.ring_geometry_valid ? "true" : "false") << "\n";
    f << "  },\n";
    f << "  \"line\": {\n";
    f << "    \"x0\": " << line.x0 << ", \"y0\": " << line.y0 << ",\n";
    f << "    \"x1\": " << line.x1 << ", \"y1\": " << line.y1 << ",\n";
    f << "    \"tool_half_width\": " << line.tool_half_width << ",\n";
    f << "    \"line_length\": " << line.line_length << ",\n";
    f << "    \"scan_line_count\": " << line.scan_line_count << "\n";
    f << "  },\n";
    f << "  \"formfit_gauge\": {\n";
    f << "    \"gauge_id\": \"" << gauge.gauge_id << "\",\n";
    f << "    \"name\": \"" << gauge.name << "\",\n";
    f << "    \"learn_score\": " << gauge.learn_score << ",\n";
    f << "    \"element_count\": " << gauge.elements.size() << ",\n";
    f << "    \"relation_count\": " << gauge.relations.size() << ",\n";
    f << "    \"constraint_count\": " << gauge.constraints.size() << ",\n";
    f << "    \"elements\": [\n";
    for (std::size_t i = 0; i < gauge.elements.size(); ++i)
    {
        const auto& e = gauge.elements[i];
        f << "      {\"id\": \"" << e.element_id << "\", \"type\": \"" << cxcore::formfit::GaugeElementTypeName(e.element_type) << "\", \"source\": \"" << e.source_entity_id << "\"}" << (i + 1 < gauge.elements.size() ? "," : "") << "\n";
    }
    f << "    ],\n";
    f << "    \"relations\": [\n";
    for (std::size_t i = 0; i < gauge.relations.size(); ++i)
    {
        const auto& r = gauge.relations[i];
        f << "      {\"id\": \"" << r.relation_id << "\", \"lhs\": \"" << r.lhs_element_id << "\", \"rhs\": \"" << r.rhs_element_id << "\", \"type\": \"" << cxcore::formfit::GaugeRelationTypeName(r.relation_type) << "\", \"target\": " << r.target_value << ", \"tolerance\": " << r.tolerance << "}" << (i + 1 < gauge.relations.size() ? "," : "") << "\n";
    }
    f << "    ],\n";
    f << "    \"constraints\": [\n";
    for (std::size_t i = 0; i < gauge.constraints.size(); ++i)
    {
        const auto& c = gauge.constraints[i];
        f << "      {\"id\": \"" << c.constraint_id << "\", \"target\": \"" << c.target_element_id << "\", \"type\": \"" << cxcore::formfit::GaugeConstraintTypeName(c.constraint_type) << "\", \"target_value\": " << c.target_value << ", \"tolerance\": " << c.tolerance << "}" << (i + 1 < gauge.constraints.size() ? "," : "") << "\n";
    }
    f << "    ]\n";
    f << "  },\n";
    f << "  \"frame_compare_status\": \"" << ((ring.frame_compare_status == "FRAME_MATCH" && line.frame_compare_status == "FRAME_MATCH") ? "FRAME_MATCH" : "FRAME_GEOMETRY_MISMATCH") << "\",\n";
    f << "  \"frame_compare_reason\": \"circle ring and line feed a FormfitGauge composition\"\n";
    f << "}\n";
}

void WriteReport(const std::string& tool, const std::filesystem::path& script, const std::filesystem::path& image,
                 const std::string& status, const std::string& reason, const std::filesystem::path& path)
{
    std::ofstream f(path);
    f << "# Gauge Frame Probe Report\n\n";
    f << "## Case Info\n\n";
    f << "- Tool: " << tool << "\n";
    f << "- Script: " << script.string() << "\n";
    f << "- Image: " << image.string() << "\n";
    f << "- Probe mode: G0/G1 Geometry Frame Probe\n\n";
    f << "## Runtime Compare\n\n";
    f << "- frame compare status: " << status << "\n";
    f << "- frame compare reason: " << reason << "\n\n";
    f << "## Decision\n\n";
    if (status == "FRAME_MATCH") f << "PASS\n";
    else if (status == "ROI_OUT_OF_IMAGE") f << "WARNING\n";
    else f << "FAIL\n";
    f << "\n## Notes\n\nManual review should inspect frame_black.png and frame_on_image.png before any measure/fit test.\n";
}
}

bool ParseGaugeFrameProbeArgs(int argc, char** argv, GaugeFrameProbeOptions& options)
{
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--cxscript-frame-probe") options.enabled = true;
        else if (arg == "--image" && i + 1 < argc) options.image_path = argv[++i];
        else if (arg == "--script" && i + 1 < argc) options.script_path = argv[++i];
        else if (arg == "--out" && i + 1 < argc) options.out_root = argv[++i];
        else if (arg == "--case-name" && i + 1 < argc) options.case_name = argv[++i];
    }
    return options.enabled;
}

bool RunGaugeFrameProbe(const GaugeFrameProbeOptions& options, GaugeFrameProbeResult& result)
{
    result.exit_code = 1;
    if (options.script_path.empty()) { result.reason = "--script is required"; return false; }
    if (options.image_path.empty()) { result.reason = "--image is required"; return false; }
    const std::filesystem::path out = options.out_root.empty() ? std::filesystem::path("gauge_frame_probe_out") : options.out_root;
    std::filesystem::create_directories(out);

    cv::Mat image = cv::imread(options.image_path.string(), cv::IMREAD_COLOR);
    if (image.empty()) { result.reason = "cannot load image: " + options.image_path.string(); return false; }

    ParsedProbeScript script;
    if (!ParseProbeScript(options.script_path, script, result.reason)) return false;
    result.tool = script.tool;
    result.frame_black_path = out / "frame_black.png";
    result.frame_on_image_path = out / "frame_on_image.png";
    result.frame_geometry_path = out / "frame_geometry.json";
    result.frame_report_path = out / "frame_geometry_report.md";
    result.snapshot_path = out / "snapshot.txt";

    std::string imageReason;
    if (script.tool == "FindLine")
    {
        GaugeLineFrameProbe probe = BuildLineFrame(script, image.cols, image.rows);
        WriteLineJson(probe, result.frame_geometry_path);
        WriteReport(probe.tool, options.script_path, options.image_path, probe.frame_compare_status, probe.frame_compare_reason, result.frame_report_path);
        if (!SaveLineFrameProbeImages(probe, image, result.frame_black_path, result.frame_on_image_path, imageReason))
        { result.reason = imageReason; return false; }
        std::ofstream s(result.snapshot_path);
        s << "tool: FindLine\nscript_path: " << options.script_path.string() << "\nframe_compare_status: " << probe.frame_compare_status << "\nscan_line_count: " << probe.scan_line_count << "\nscan_line_length: " << probe.scan_line_length << "\n";
        result.ok = (probe.frame_compare_status == "FRAME_MATCH");
    }
    else if (script.tool == "CircleRingLineFormfitGauge")
    {
        GaugeCircleRingFrameProbe ringProbe = BuildCircleRingFrame(script, image.cols, image.rows);
        GaugeLineFrameProbe lineProbe = BuildLineFrame(script, image.cols, image.rows);
        cxcore::CircleMeasurementOutput outerOutput = MakeCircleOutputFromProbe(ringProbe.outer_cx, ringProbe.outer_cy, ringProbe.outer_radius);
        cxcore::CircleMeasurementOutput innerOutput = MakeCircleOutputFromProbe(ringProbe.inner_cx, ringProbe.inner_cy, ringProbe.inner_radius);
        cxcore::LineMeasurementOutput lineOutput = MakeLineOutputFromProbe(lineProbe);
        cxcore::formfit::FormfitGauge gauge = cxcore::formfit::MakeCircleRingLineGauge(
            outerOutput,
            innerOutput,
            lineOutput,
            "circle_ring_line_gauge",
            "Circle Ring + Line Gauge",
            3.0,
            5.0);
        const std::string status = (ringProbe.frame_compare_status == "FRAME_MATCH" && lineProbe.frame_compare_status == "FRAME_MATCH") ? "FRAME_MATCH" : "FRAME_GEOMETRY_MISMATCH";
        WriteCircleRingLineFormfitJson(ringProbe, lineProbe, gauge, result.frame_geometry_path);
        WriteReport("CircleRingLineFormfitGauge", options.script_path, options.image_path, status, "circle ring and line feed a FormfitGauge composition", result.frame_report_path);
        if (!SaveCircleRingLineFormfitProbeImages(ringProbe, lineProbe, image, result.frame_black_path, result.frame_on_image_path, imageReason))
        { result.reason = imageReason; return false; }
        std::ofstream s(result.snapshot_path);
        s << "tool: CircleRingLineFormfitGauge\nscript_path: " << options.script_path.string() << "\nframe_compare_status: " << status << "\nformfit_element_count: " << gauge.elements.size() << "\nformfit_relation_count: " << gauge.relations.size() << "\nformfit_constraint_count: " << gauge.constraints.size() << "\nlearn_score: " << gauge.learn_score << "\n";
        result.ok = (status == "FRAME_MATCH" && gauge.elements.size() == 3 && gauge.relations.size() >= 4 && gauge.constraints.size() >= 3);
    }    else if (script.tool == "CircleRingGauge")
    {
        GaugeCircleRingFrameProbe probe = BuildCircleRingFrame(script, image.cols, image.rows);
        WriteCircleRingJson(probe, result.frame_geometry_path);
        WriteReport(probe.tool, options.script_path, options.image_path, probe.frame_compare_status, probe.frame_compare_reason, result.frame_report_path);
        if (!SaveCircleRingFrameProbeImages(probe, image, result.frame_black_path, result.frame_on_image_path, imageReason))
        { result.reason = imageReason; return false; }
        std::ofstream s(result.snapshot_path);
        s << "tool: CircleRingGauge\nscript_path: " << options.script_path.string() << "\nframe_compare_status: " << probe.frame_compare_status << "\nouter_radius: " << probe.outer_radius << "\ninner_radius: " << probe.inner_radius << "\nring_thickness: " << probe.ring_thickness << "\nscan_line_count: " << probe.scan_line_count << "\n";
        result.ok = (probe.frame_compare_status == "FRAME_MATCH");
    }
    else
    {
        GaugeCircleFrameProbe probe = BuildCircleFrame(script, image.cols, image.rows);
        WriteCircleJson(probe, result.frame_geometry_path);
        WriteReport(probe.tool, options.script_path, options.image_path, probe.frame_compare_status, probe.frame_compare_reason, result.frame_report_path);
        if (!SaveCircleFrameProbeImages(probe, image, result.frame_black_path, result.frame_on_image_path, imageReason))
        { result.reason = imageReason; return false; }
        std::ofstream s(result.snapshot_path);
        s << "tool: FindCircle\nscript_path: " << options.script_path.string() << "\nframe_compare_status: " << probe.frame_compare_status << "\nradius: " << probe.radius << "\nscan_line_count: " << probe.scan_line_count << "\n";
        result.ok = (probe.frame_compare_status == "FRAME_MATCH");
    }

    result.exit_code = result.ok ? 0 : 2;
    result.reason = result.ok ? "" : "frame probe did not pass";
    return result.ok;
}
