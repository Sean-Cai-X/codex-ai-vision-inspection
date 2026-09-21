#include "pch.h"
#include "PolylineShape.h"

PolylineShape::PolylineShape()
{
}

void PolylineShape::addPoint(double x, double y)
{
    m_points.push_back({ x, y });
}

void PolylineShape::insertPoint(int index, double x, double y)
{
    if (index >= 0 && index <= static_cast<int>(m_points.size()))
        m_points.insert(m_points.begin() + index, { x, y });
}

void PolylineShape::removePoint(int index)
{
    if (index >= 0 && index < static_cast<int>(m_points.size()))
        m_points.erase(m_points.begin() + index);
}

void PolylineShape::setPoint(int index, double x, double y)
{
    if (index >= 0 && index < static_cast<int>(m_points.size()))
    {
        m_points[index].x = x;
        m_points[index].y = y;
    }
}

void PolylineShape::clear()
{
    m_points.clear();
}

void PolylineShape::close(bool closed)
{
    m_closed = closed;
}

CxShapeHit PolylineShape::hitTest(double x, double y, double tolerance) const
{
    const int n = static_cast<int>(m_points.size());
    if (n < 2)
        return {};

    const double t_sq = tolerance * tolerance;

    for (int i = 0; i < n; ++i)
    {
        const double dx = x - m_points[i].x;
        const double dy = y - m_points[i].y;
        const double dist_sq = dx * dx + dy * dy;
        if (dist_sq <= t_sq)
            return { true, CxShapeHandleRole::Vertex, i, std::sqrt(dist_sq) };
    }

    double cx = 0.0, cy = 0.0;
    for (const auto& p : m_points)
    {
        cx += p.x;
        cy += p.y;
    }
    cx /= n;
    cy /= n;
    const double center_dist_sq = (x - cx) * (x - cx) + (y - cy) * (y - cy);
    if (center_dist_sq <= t_sq)
        return { true, CxShapeHandleRole::Center, -1, std::sqrt(center_dist_sq) };

    double min_dist = tolerance + 1.0;
    int min_segment = -1;

    for (int i = 0; i < n; ++i)
    {
        const int j = (i + 1) % n;
        if (!m_closed && j == 0)
            break;

        const double x0 = m_points[i].x;
        const double y0 = m_points[i].y;
        const double x1 = m_points[j].x;
        const double y1 = m_points[j].y;

        const double dx = x1 - x0;
        const double dy = y1 - y0;
        const double len = std::sqrt(dx * dx + dy * dy);

        if (len < 1e-9)
            continue;

        const double tdx = dx / len;
        const double tdy = dy / len;

        const double px = x - x0;
        const double py = y - y0;
        const double proj = px * tdx + py * tdy;
        const double clamped_proj = std::max(0.0, std::min(len, proj));
        const double near_x = x0 + tdx * clamped_proj;
        const double near_y = y0 + tdy * clamped_proj;
        const double dist = std::sqrt((x - near_x) * (x - near_x) + (y - near_y) * (y - near_y));

        if (dist < min_dist)
        {
            min_dist = dist;
            min_segment = i;
        }
    }

    if (min_dist <= tolerance && min_segment >= 0)
        return { true, CxShapeHandleRole::Body, min_segment, min_dist };

    return {};
}

void PolylineShape::enumerateHandles(std::vector<CxShapeHandle>& out) const
{
    const int n = static_cast<int>(m_points.size());
    for (int i = 0; i < n; ++i)
    {
        out.push_back({ CxShapeHandleRole::Vertex, i, m_points[i], "P" + std::to_string(i) });
    }

    if (n > 0)
    {
        double cx = 0.0;
        double cy = 0.0;
        for (const auto& p : m_points)
        {
            cx += p.x;
            cy += p.y;
        }
        cx /= n;
        cy /= n;
        out.push_back({ CxShapeHandleRole::Center, -1, { cx, cy }, "C" });
    }
}

void PolylineShape::dragHandle(CxShapeHandleRole role, int vertex_index, double x, double y)
{
    const int n = static_cast<int>(m_points.size());

    switch (role)
    {
    case CxShapeHandleRole::Vertex:
        if (vertex_index >= 0 && vertex_index < n)
        {
            m_points[vertex_index].x = x;
            m_points[vertex_index].y = y;
        }
        break;
    case CxShapeHandleRole::Body:
    case CxShapeHandleRole::Center:
    {
        if (n < 2)
            return;

        double cx = 0.0, cy = 0.0;
        for (const auto& p : m_points)
        {
            cx += p.x;
            cy += p.y;
        }
        cx /= n;
        cy /= n;

        const double dx = x - cx;
        const double dy = y - cy;

        for (auto& p : m_points)
        {
            p.x += dx;
            p.y += dy;
        }
        break;
    }
    default:
        break;
    }
}

void PolylineShape::translateBy(double dx, double dy)
{
    for (auto& p : m_points)
    {
        p.x += dx;
        p.y += dy;
    }
}

void PolylineShape::drawshape(gp_Path& painter)
{
    const int n = static_cast<int>(m_points.size());
    if (n < 2)
        return;

    for (int i = 0; i < n - 1; ++i)
    {
        painter.AddLine(
            gp_Pnt(m_points[i].x, m_points[i].y, 0),
            gp_Pnt(m_points[i + 1].x, m_points[i + 1].y, 0));
    }

    if (m_closed && n > 2)
    {
        painter.AddLine(
            gp_Pnt(m_points.back().x, m_points.back().y, 0),
            gp_Pnt(m_points.front().x, m_points.front().y, 0));
    }
}

void PolylineShape::exportPolyline(std::vector<CxShapePoint>& out, bool& closed) const
{
    out = m_points;
    closed = m_closed;
}

void PolylineShape::exportPoints(std::vector<CxShapePoint>& out) const
{
    out = m_points;
}

bool PolylineShape::snapshot(CxShapeGeometrySnapshot& out) const
{
    out = CxShapeGeometrySnapshot{};
    out.kind = CxShapeKind::Polyline;
    out.points = m_points;
    out.closed = m_closed;

    if (!m_points.empty())
    {
        for (const auto& p : m_points)
        {
            out.center.x += p.x;
            out.center.y += p.y;
        }
        out.center.x /= static_cast<double>(m_points.size());
        out.center.y /= static_cast<double>(m_points.size());
    }

    return !m_points.empty();
}
