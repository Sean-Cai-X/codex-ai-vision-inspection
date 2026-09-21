#include "CxScriptGeometryFrameOverlay.h"
#include <algorithm>
#include <cmath>

namespace
{
cv::Point ToPoint(const GaugePoint2d& p)
{
    return cv::Point((int)std::lround(p.x), (int)std::lround(p.y));
}

void PutLabel(cv::Mat& canvas, const std::string& text, int row)
{
    cv::putText(canvas, text, cv::Point(16, 28 + row * 24),
                cv::FONT_HERSHEY_SIMPLEX, 0.58, cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
}

void DrawLineFrame(cv::Mat& canvas, const GaugeLineFrameProbe& p)
{
    if (p.rect.size() == 4)
    {
        for (int i = 0; i < 4; ++i)
            cv::line(canvas, ToPoint(p.rect[i]), ToPoint(p.rect[(i + 1) % 4]), cv::Scalar(0, 255, 255), 2, cv::LINE_AA);
    }

    cv::line(canvas, cv::Point((int)std::lround(p.x0), (int)std::lround(p.y0)),
             cv::Point((int)std::lround(p.x1), (int)std::lround(p.y1)),
             cv::Scalar(0, 255, 0), 2, cv::LINE_AA);

    const int count = std::max(1, p.scan_line_count);
    const double denom = count > 1 ? (double)(count - 1) : 1.0;
    for (int i = 0; i < count; ++i)
    {
        const double t = count > 1 ? (double)i / denom : 0.5;
        const double cx = p.x0 + (p.x1 - p.x0) * t;
        const double cy = p.y0 + (p.y1 - p.y0) * t;
        GaugePoint2d a{cx + p.normal_x * p.tool_half_width, cy + p.normal_y * p.tool_half_width};
        GaugePoint2d b{cx - p.normal_x * p.tool_half_width, cy - p.normal_y * p.tool_half_width};
        cv::line(canvas, ToPoint(a), ToPoint(b), cv::Scalar(255, 0, 0), 1, cv::LINE_AA);
    }

    cv::circle(canvas, cv::Point((int)std::lround(p.x0), (int)std::lround(p.y0)), 5, cv::Scalar(0, 0, 255), -1, cv::LINE_AA);
    cv::circle(canvas, cv::Point((int)std::lround(p.x1), (int)std::lround(p.y1)), 5, cv::Scalar(255, 255, 0), -1, cv::LINE_AA);

    PutLabel(canvas, "Findline Gauge Frame Probe", 0);
    PutLabel(canvas, "center=(" + std::to_string((int)p.x0) + "," + std::to_string((int)p.y0) + ")->(" + std::to_string((int)p.x1) + "," + std::to_string((int)p.y1) + ") half_width=" + std::to_string((int)p.tool_half_width), 1);
    PutLabel(canvas, "linegap=" + std::to_string(p.linegap) + " scan_line_count=" + std::to_string(p.scan_line_count) + " compare=" + p.frame_compare_status, 2);
}

void DrawCircleFrame(cv::Mat& canvas, const GaugeCircleFrameProbe& p)
{
    cv::Point c((int)std::lround(p.cx), (int)std::lround(p.cy));
    cv::Point pass((int)std::lround(p.px), (int)std::lround(p.py));
    cv::circle(canvas, c, (int)std::lround(p.radius), cv::Scalar(0, 255, 0), 2, cv::LINE_AA);
    cv::line(canvas, c, pass, cv::Scalar(0, 255, 255), 2, cv::LINE_AA);

    const int count = std::max(1, p.scan_line_count);
    for (int i = 0; i < count; ++i)
    {
        const double a = (2.0 * CV_PI * i) / count;
        cv::Point e((int)std::lround(p.cx + std::cos(a) * p.radius),
                    (int)std::lround(p.cy + std::sin(a) * p.radius));
        cv::line(canvas, c, e, cv::Scalar(255, 0, 0), 1, cv::LINE_AA);
    }

    cv::circle(canvas, c, 5, cv::Scalar(0, 0, 255), -1, cv::LINE_AA);
    cv::circle(canvas, pass, 5, cv::Scalar(255, 255, 0), -1, cv::LINE_AA);
    PutLabel(canvas, "FindCircle Gauge Frame Probe", 0);
    PutLabel(canvas, "center=(" + std::to_string((int)p.cx) + "," + std::to_string((int)p.cy) + ") pass=(" + std::to_string((int)p.px) + "," + std::to_string((int)p.py) + ") radius=" + std::to_string((int)std::lround(p.radius)), 1);
    PutLabel(canvas, "gap=" + std::to_string(p.gap) + " linegap=" + std::to_string(p.linegap) + " scan_line_count=" + std::to_string(p.scan_line_count) + " compare=" + p.frame_compare_status, 2);
}
void DrawCircleRingFrame(cv::Mat& canvas, const GaugeCircleRingFrameProbe& p)
{
    cv::Point outer_c((int)std::lround(p.outer_cx), (int)std::lround(p.outer_cy));
    cv::Point inner_c((int)std::lround(p.inner_cx), (int)std::lround(p.inner_cy));
    cv::Point outer_pass((int)std::lround(p.outer_px), (int)std::lround(p.outer_py));
    cv::Point inner_pass((int)std::lround(p.inner_px), (int)std::lround(p.inner_py));

    cv::circle(canvas, outer_c, (int)std::lround(p.outer_radius), cv::Scalar(0, 255, 0), 2, cv::LINE_AA);
    cv::circle(canvas, inner_c, (int)std::lround(p.inner_radius), cv::Scalar(0, 255, 255), 2, cv::LINE_AA);
    cv::line(canvas, outer_c, inner_c, cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
    cv::line(canvas, outer_c, outer_pass, cv::Scalar(0, 180, 255), 2, cv::LINE_AA);
    cv::line(canvas, inner_c, inner_pass, cv::Scalar(0, 180, 255), 2, cv::LINE_AA);

    const int count = std::max(1, p.scan_line_count);
    for (int i = 0; i < count; ++i)
    {
        const double a = (2.0 * CV_PI * i) / count;
        cv::Point inner_pt((int)std::lround(p.inner_cx + std::cos(a) * p.inner_radius),
                           (int)std::lround(p.inner_cy + std::sin(a) * p.inner_radius));
        cv::Point outer_pt((int)std::lround(p.outer_cx + std::cos(a) * p.outer_radius),
                           (int)std::lround(p.outer_cy + std::sin(a) * p.outer_radius));
        cv::line(canvas, inner_pt, outer_pt, cv::Scalar(255, 0, 0), 1, cv::LINE_AA);
    }

    cv::circle(canvas, outer_c, 5, cv::Scalar(0, 0, 255), -1, cv::LINE_AA);
    cv::circle(canvas, inner_c, 4, cv::Scalar(255, 255, 0), -1, cv::LINE_AA);
    cv::circle(canvas, outer_pass, 4, cv::Scalar(255, 255, 0), -1, cv::LINE_AA);
    cv::circle(canvas, inner_pass, 4, cv::Scalar(255, 180, 0), -1, cv::LINE_AA);

    PutLabel(canvas, "CircleRing Gauge Frame Probe", 0);
    PutLabel(canvas, "outer center=(" + std::to_string((int)p.outer_cx) + "," + std::to_string((int)p.outer_cy) + ") radius=" + std::to_string((int)std::lround(p.outer_radius)), 1);
    PutLabel(canvas, "inner center=(" + std::to_string((int)p.inner_cx) + "," + std::to_string((int)p.inner_cy) + ") radius=" + std::to_string((int)std::lround(p.inner_radius)), 2);
    PutLabel(canvas, "thickness=" + std::to_string((int)std::lround(p.ring_thickness)) + " center_distance=" + std::to_string((int)std::lround(p.center_distance)) + " compare=" + p.frame_compare_status, 3);
}
}

bool SaveLineFrameProbeImages(const GaugeLineFrameProbe& probe, const cv::Mat& image,
                              const std::filesystem::path& black_path,
                              const std::filesystem::path& image_path,
                              std::string& reason)
{
    cv::Mat black(std::max(1, probe.image_height), std::max(1, probe.image_width), CV_8UC3, cv::Scalar(0, 0, 0));
    cv::Mat overlay = image.empty() ? black.clone() : image.clone();
    if (overlay.channels() == 1)
        cv::cvtColor(overlay, overlay, cv::COLOR_GRAY2BGR);
    DrawLineFrame(black, probe);
    DrawLineFrame(overlay, probe);
    std::filesystem::create_directories(black_path.parent_path());
    if (!cv::imwrite(black_path.string(), black)) { reason = "failed to write " + black_path.string(); return false; }
    if (!cv::imwrite(image_path.string(), overlay)) { reason = "failed to write " + image_path.string(); return false; }
    return true;
}

bool SaveCircleFrameProbeImages(const GaugeCircleFrameProbe& probe, const cv::Mat& image,
                                const std::filesystem::path& black_path,
                                const std::filesystem::path& image_path,
                                std::string& reason)
{
    cv::Mat black(std::max(1, probe.image_height), std::max(1, probe.image_width), CV_8UC3, cv::Scalar(0, 0, 0));
    cv::Mat overlay = image.empty() ? black.clone() : image.clone();
    if (overlay.channels() == 1)
        cv::cvtColor(overlay, overlay, cv::COLOR_GRAY2BGR);
    DrawCircleFrame(black, probe);
    DrawCircleFrame(overlay, probe);
    std::filesystem::create_directories(black_path.parent_path());
    if (!cv::imwrite(black_path.string(), black)) { reason = "failed to write " + black_path.string(); return false; }
    if (!cv::imwrite(image_path.string(), overlay)) { reason = "failed to write " + image_path.string(); return false; }
    return true;
}

bool SaveCircleRingFrameProbeImages(const GaugeCircleRingFrameProbe& probe, const cv::Mat& image,
                                    const std::filesystem::path& black_path,
                                    const std::filesystem::path& image_path,
                                    std::string& reason)
{
    cv::Mat black(std::max(1, probe.image_height), std::max(1, probe.image_width), CV_8UC3, cv::Scalar(0, 0, 0));
    cv::Mat overlay = image.empty() ? black.clone() : image.clone();
    if (overlay.channels() == 1)
        cv::cvtColor(overlay, overlay, cv::COLOR_GRAY2BGR);
    DrawCircleRingFrame(black, probe);
    DrawCircleRingFrame(overlay, probe);
    std::filesystem::create_directories(black_path.parent_path());
    if (!cv::imwrite(black_path.string(), black)) { reason = "failed to write " + black_path.string(); return false; }
    if (!cv::imwrite(image_path.string(), overlay)) { reason = "failed to write " + image_path.string(); return false; }
    return true;
}
bool SaveCircleRingLineFormfitProbeImages(const GaugeCircleRingFrameProbe& ring_probe,
                                          const GaugeLineFrameProbe& line_probe,
                                          const cv::Mat& image,
                                          const std::filesystem::path& black_path,
                                          const std::filesystem::path& image_path,
                                          std::string& reason)
{
    const int h = std::max(1, std::max(ring_probe.image_height, line_probe.image_height));
    const int w = std::max(1, std::max(ring_probe.image_width, line_probe.image_width));
    cv::Mat black(h, w, CV_8UC3, cv::Scalar(0, 0, 0));
    cv::Mat overlay = image.empty() ? black.clone() : image.clone();
    if (overlay.channels() == 1)
        cv::cvtColor(overlay, overlay, cv::COLOR_GRAY2BGR);
    DrawCircleRingFrame(black, ring_probe);
    DrawLineFrame(black, line_probe);
    DrawCircleRingFrame(overlay, ring_probe);
    DrawLineFrame(overlay, line_probe);
    PutLabel(black, "CircleRing + Line -> FormfitGauge Probe", 4);
    PutLabel(overlay, "CircleRing + Line -> FormfitGauge Probe", 4);
    std::filesystem::create_directories(black_path.parent_path());
    if (!cv::imwrite(black_path.string(), black)) { reason = "failed to write " + black_path.string(); return false; }
    if (!cv::imwrite(image_path.string(), overlay)) { reason = "failed to write " + image_path.string(); return false; }
    return true;
}