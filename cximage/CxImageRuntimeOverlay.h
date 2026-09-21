#pragma once

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

struct CxLineScanBoxSnapshot
{
    bool valid = false;
    std::array<float, 8> xy = {
        0.0f, 0.0f,
        0.0f, 0.0f,
        0.0f, 0.0f,
        0.0f, 0.0f
    };
    float half_width = 0.0f;
};

inline CxLineScanBoxSnapshot BuildCxLineScanBoxSnapshotFromHalfWidth(
    float x0,
    float y0,
    float x1,
    float y1,
    float halfWidth)
{
    CxLineScanBoxSnapshot result;

    const float dx = x1 - x0;
    const float dy = y1 - y0;
    const float len = std::sqrt(dx * dx + dy * dy);

    if (len <= 1.0e-5f)
        return result;

    const float nx = -dy / len;
    const float ny = dx / len;

    const float hw = std::max(2.0f, halfWidth);

    const float ax0 = x0 + nx * hw;
    const float ay0 = y0 + ny * hw;
    const float ax1 = x1 + nx * hw;
    const float ay1 = y1 + ny * hw;

    const float bx0 = x0 - nx * hw;
    const float by0 = y0 - ny * hw;
    const float bx1 = x1 - nx * hw;
    const float by1 = y1 - ny * hw;

    result.xy = {
        ax0, ay0,
        ax1, ay1,
        bx1, by1,
        bx0, by0
    };

    result.half_width = hw;
    result.valid = true;
    return result;
}

inline CxLineScanBoxSnapshot BuildCxLineScanBoxSnapshot(float x0,
                                                        float y0,
                                                        float x1,
                                                        float y1,
                                                        float scale,
                                                        int linegap)
{
    const float safeScale = std::max(1.0f, scale);
    const float safeGap = static_cast<float>(std::max(1, linegap));
    const float halfWidth = std::max(2.0f, safeGap * safeScale);
    return BuildCxLineScanBoxSnapshotFromHalfWidth(x0, y0, x1, y1, halfWidth);
}

struct CxCirclePolylineSnapshot
{
    bool valid = false;
    float cx = 0.0f;
    float cy = 0.0f;
    float radius = 0.0f;
    std::vector<float> xy;
    std::uint32_t segment_count = 0;
};

inline CxCirclePolylineSnapshot BuildCxCirclePolylineSnapshot(float cx,
                                                              float cy,
                                                              float radius,
                                                              std::uint32_t preferredSegments = 96)
{
    CxCirclePolylineSnapshot result;

    if (!std::isfinite(cx) ||
        !std::isfinite(cy) ||
        !std::isfinite(radius) ||
        radius <= 0.0f)
    {
        return result;
    }

    std::uint32_t segments = preferredSegments;
    if (radius < 32.0f)
        segments = 48;
    else if (radius > 512.0f)
        segments = 128;

    segments = std::max<std::uint32_t>(24, segments);
    result.xy.clear();
    result.xy.reserve(static_cast<std::size_t>(segments) * 2U);

    constexpr float kTwoPi = 6.28318530717958647692f;
    for (std::uint32_t i = 0; i < segments; ++i)
    {
        const float t = kTwoPi * static_cast<float>(i) / static_cast<float>(segments);
        result.xy.push_back(cx + std::cos(t) * radius);
        result.xy.push_back(cy + std::sin(t) * radius);
    }

    result.cx = cx;
    result.cy = cy;
    result.radius = radius;
    result.segment_count = segments;
    result.valid = true;
    return result;
}
