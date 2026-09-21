#include "pch.h"
#include "RectShape.h"

RectShape::RectShape()
    : m_points{{0, 0}, {100, 0}, {100, 100}, {0, 100}}
{
}

RectShape::RectShape(double x0, double y0, double x1, double y1)
{
    setRect(x0, y0, x1, y1);
}

void RectShape::setRect(double x0, double y0, double x1, double y1)
{
    m_points[0] = {x0, y0};
    m_points[1] = {x1, y0};
    m_points[2] = {x1, y1};
    m_points[3] = {x0, y1};
}

CxShapeHit RectShape::hitTest(double x, double y, double tolerance) const
{
    const double t_sq = tolerance * tolerance;
    double dist;

    dist = (x - m_points[0].x) * (x - m_points[0].x) + (y - m_points[0].y) * (y - m_points[0].y);
    if (dist <= t_sq)
        return { true, CxShapeHandleRole::Corner0, -1, std::sqrt(dist) };

    dist = (x - m_points[1].x) * (x - m_points[1].x) + (y - m_points[1].y) * (y - m_points[1].y);
    if (dist <= t_sq)
        return { true, CxShapeHandleRole::Corner1, -1, std::sqrt(dist) };

    dist = (x - m_points[2].x) * (x - m_points[2].x) + (y - m_points[2].y) * (y - m_points[2].y);
    if (dist <= t_sq)
        return { true, CxShapeHandleRole::Corner2, -1, std::sqrt(dist) };

    dist = (x - m_points[3].x) * (x - m_points[3].x) + (y - m_points[3].y) * (y - m_points[3].y);
    if (dist <= t_sq)
        return { true, CxShapeHandleRole::Corner3, -1, std::sqrt(dist) };

    const double cx = (m_points[0].x + m_points[2].x) * 0.5;
    const double cy = (m_points[0].y + m_points[2].y) * 0.5;
    dist = (x - cx) * (x - cx) + (y - cy) * (y - cy);
    if (dist <= t_sq)
        return { true, CxShapeHandleRole::Center, -1, std::sqrt(dist) };

    const double min_x = std::min(m_points[0].x, m_points[2].x);
    const double max_x = std::max(m_points[0].x, m_points[2].x);
    const double min_y = std::min(m_points[0].y, m_points[2].y);
    const double max_y = std::max(m_points[0].y, m_points[2].y);

    if (x >= min_x - tolerance && x <= max_x + tolerance &&
        y >= min_y - tolerance && y <= max_y + tolerance)
    {
        const double dx = std::max(min_x - x, std::max(x - max_x, 0.0));
        const double dy = std::max(min_y - y, std::max(y - max_y, 0.0));
        return { true, CxShapeHandleRole::Body, -1, std::sqrt(dx * dx + dy * dy) };
    }

    return {};
}

void RectShape::enumerateHandles(std::vector<CxShapeHandle>& out) const
{
    out.push_back({CxShapeHandleRole::Corner0, -1, m_points[0], "Corner0"});
    out.push_back({CxShapeHandleRole::Corner1, -1, m_points[1], "Corner1"});
    out.push_back({CxShapeHandleRole::Corner2, -1, m_points[2], "Corner2"});
    out.push_back({CxShapeHandleRole::Corner3, -1, m_points[3], "Corner3"});
    out.push_back({
        CxShapeHandleRole::Center,
        -1,
        {
            (m_points[0].x + m_points[2].x) * 0.5,
            (m_points[0].y + m_points[2].y) * 0.5
        },
        "C"
    });
}

void RectShape::dragHandle(CxShapeHandleRole role, int vertex_index, double x, double y)
{
    switch (role)
    {
    case CxShapeHandleRole::Corner0:
        m_points[0].x = x;
        m_points[0].y = y;
        break;
    case CxShapeHandleRole::Corner1:
        m_points[1].x = x;
        m_points[1].y = y;
        break;
    case CxShapeHandleRole::Corner2:
        m_points[2].x = x;
        m_points[2].y = y;
        break;
    case CxShapeHandleRole::Corner3:
        m_points[3].x = x;
        m_points[3].y = y;
        break;
    case CxShapeHandleRole::Body:
    case CxShapeHandleRole::Center:
        translateBy(x - (m_points[0].x + m_points[2].x) / 2, y - (m_points[0].y + m_points[2].y) / 2);
        break;
    default:
        break;
    }
}

void RectShape::translateBy(double dx, double dy)
{
    for (int i = 0; i < 4; ++i)
    {
        m_points[i].x += dx;
        m_points[i].y += dy;
    }
}

void RectShape::exportPoints(std::vector<CxShapePoint>& out) const
{
    out.clear();
    for (int i = 0; i < 4; ++i)
        out.push_back(m_points[i]);
}

void RectShape::drawshape(gp_Path& painter)
{
    (void)painter;
}

void RectShape::exportPolyline(std::vector<CxShapePoint>& out, bool& closed) const
{
    out.clear();
    for (int i = 0; i < 4; ++i)
        out.push_back(m_points[i]);
    closed = true;
}
