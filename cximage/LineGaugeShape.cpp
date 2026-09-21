#include "pch.h"
#include "LineGaugeShape.h"

LineGaugeShape::LineGaugeShape()
    : m_x0(0.0), m_y0(0.0), m_x1(100.0), m_y1(100.0), m_halfWidth(20.0)
{
}

LineGaugeShape::LineGaugeShape(double x0, double y0, double x1, double y1, double half_width)
    : m_x0(x0), m_y0(y0), m_x1(x1), m_y1(y1), m_halfWidth(half_width)
{
}

void LineGaugeShape::setLine(double x0, double y0, double x1, double y1)
{
    m_x0 = x0;
    m_y0 = y0;
    m_x1 = x1;
    m_y1 = y1;
}

void LineGaugeShape::setHalfWidth(double hw)
{
    m_halfWidth = std::max(1.0, hw);
}

CxShapeHit LineGaugeShape::hitTest(double x, double y, double tolerance) const
{
    const double dx = m_x1 - m_x0;
    const double dy = m_y1 - m_y0;
    const double len = std::sqrt(dx * dx + dy * dy);
    
    if (len < 1.0)
        return {};

    const double tdx = dx / len;
    const double tdy = dy / len;
    const double ndx = -tdy;
    const double ndy = tdx;

    const double cx = (m_x0 + m_x1) * 0.5;
    const double cy = (m_y0 + m_y1) * 0.5;

    const double wx_plus = cx + ndx * m_halfWidth;
    const double wy_plus = cy + ndy * m_halfWidth;
    const double wx_minus = cx - ndx * m_halfWidth;
    const double wy_minus = cy - ndy * m_halfWidth;

    const double t_sq = tolerance * tolerance;

    double dist;

    dist = (x - m_x0) * (x - m_x0) + (y - m_y0) * (y - m_y0);
    if (dist <= t_sq)
        return { true, CxShapeHandleRole::Start, -1, std::sqrt(dist) };

    dist = (x - m_x1) * (x - m_x1) + (y - m_y1) * (y - m_y1);
    if (dist <= t_sq)
        return { true, CxShapeHandleRole::End, -1, std::sqrt(dist) };

    dist = (x - cx) * (x - cx) + (y - cy) * (y - cy);
    if (dist <= t_sq)
        return { true, CxShapeHandleRole::Center, -1, std::sqrt(dist) };

    dist = (x - wx_plus) * (x - wx_plus) + (y - wy_plus) * (y - wy_plus);
    if (dist <= t_sq)
        return { true, CxShapeHandleRole::WidthPositive, -1, std::sqrt(dist) };

    dist = (x - wx_minus) * (x - wx_minus) + (y - wy_minus) * (y - wy_minus);
    if (dist <= t_sq)
        return { true, CxShapeHandleRole::WidthNegative, -1, std::sqrt(dist) };

    const double px = x - m_x0;
    const double py = y - m_y0;
    const double proj = px * tdx + py * tdy;
    const double clamped_proj = std::max(0.0, std::min(len, proj));
    const double near_x = m_x0 + tdx * clamped_proj;
    const double near_y = m_y0 + tdy * clamped_proj;
    const double body_dist = std::sqrt((x - near_x) * (x - near_x) + (y - near_y) * (y - near_y));

    if (body_dist <= m_halfWidth + tolerance)
        return { true, CxShapeHandleRole::Body, -1, body_dist };

    return {};
}

void LineGaugeShape::enumerateHandles(std::vector<CxShapeHandle>& out) const
{
    const double dx = m_x1 - m_x0;
    const double dy = m_y1 - m_y0;
    const double len = std::sqrt(dx * dx + dy * dy);
    
    if (len < 1.0)
        return;

    const double tdx = dx / len;
    const double tdy = dy / len;
    const double ndx = -tdy;
    const double ndy = tdx;

    const double cx = (m_x0 + m_x1) * 0.5;
    const double cy = (m_y0 + m_y1) * 0.5;

    const double wx_plus = cx + ndx * m_halfWidth;
    const double wy_plus = cy + ndy * m_halfWidth;
    const double wx_minus = cx - ndx * m_halfWidth;
    const double wy_minus = cy - ndy * m_halfWidth;

    out.push_back({ CxShapeHandleRole::Start, -1, { m_x0, m_y0 }, "P0" });
    out.push_back({ CxShapeHandleRole::End, -1, { m_x1, m_y1 }, "P1" });
    out.push_back({ CxShapeHandleRole::Center, -1, { cx, cy }, "C" });
    out.push_back({ CxShapeHandleRole::WidthPositive, -1, { wx_plus, wy_plus }, "W+" });
    out.push_back({ CxShapeHandleRole::WidthNegative, -1, { wx_minus, wy_minus }, "W-" });
}

void LineGaugeShape::dragHandle(CxShapeHandleRole role, int vertex_index, double x, double y)
{
    (void)vertex_index;

    switch (role)
    {
    case CxShapeHandleRole::Start:
        m_x0 = x;
        m_y0 = y;
        break;
    case CxShapeHandleRole::End:
        m_x1 = x;
        m_y1 = y;
        break;
    case CxShapeHandleRole::Center:
    case CxShapeHandleRole::Body:
    {
        const double cx_old = (m_x0 + m_x1) * 0.5;
        const double cy_old = (m_y0 + m_y1) * 0.5;
        const double dx = x - cx_old;
        const double dy = y - cy_old;
        m_x0 += dx;
        m_y0 += dy;
        m_x1 += dx;
        m_y1 += dy;
        break;
    }
    case CxShapeHandleRole::WidthPositive:
    {
        const double dx = m_x1 - m_x0;
        const double dy = m_y1 - m_y0;
        const double len = std::sqrt(dx * dx + dy * dy);
        if (len < 1.0)
            return;
        const double tdx = dx / len;
        const double tdy = dy / len;
        const double ndx = -tdy;
        const double ndy = tdx;
        const double cx = (m_x0 + m_x1) * 0.5;
        const double cy = (m_y0 + m_y1) * 0.5;
        const double signed_dist = (x - cx) * ndx + (y - cy) * ndy;
        m_halfWidth = std::max(1.0, signed_dist);
        break;
    }
    case CxShapeHandleRole::WidthNegative:
    {
        const double dx = m_x1 - m_x0;
        const double dy = m_y1 - m_y0;
        const double len = std::sqrt(dx * dx + dy * dy);
        if (len < 1.0)
            return;
        const double tdx = dx / len;
        const double tdy = dy / len;
        const double ndx = -tdy;
        const double ndy = tdx;
        const double cx = (m_x0 + m_x1) * 0.5;
        const double cy = (m_y0 + m_y1) * 0.5;
        const double signed_dist = (x - cx) * ndx + (y - cy) * ndy;
        m_halfWidth = std::max(1.0, -signed_dist);
        break;
    }
    default:
        break;
    }
}

void LineGaugeShape::translateBy(double dx, double dy)
{
    m_x0 += dx;
    m_y0 += dy;
    m_x1 += dx;
    m_y1 += dy;
}

void LineGaugeShape::exportPoints(std::vector<CxShapePoint>& out) const
{
    out.clear();
    out.push_back({ m_x0, m_y0 });
    out.push_back({ m_x1, m_y1 });
}

void LineGaugeShape::drawshape(gp_Path& painter)
{
    const double dx = m_x1 - m_x0;
    const double dy = m_y1 - m_y0;
    const double len = std::sqrt(dx * dx + dy * dy);
    
    if (len < 1.0)
        return;

    const double tdx = dx / len;
    const double tdy = dy / len;
    const double ndx = -tdy;
    const double ndy = tdx;

    const double c0x = m_x0 + ndx * m_halfWidth;
    const double c0y = m_y0 + ndy * m_halfWidth;
    const double c1x = m_x1 + ndx * m_halfWidth;
    const double c1y = m_y1 + ndy * m_halfWidth;
    const double c2x = m_x1 - ndx * m_halfWidth;
    const double c2y = m_y1 - ndy * m_halfWidth;
    const double c3x = m_x0 - ndx * m_halfWidth;
    const double c3y = m_y0 - ndy * m_halfWidth;

    painter.AddLine(gp_Pnt(c0x, c0y, 0), gp_Pnt(c1x, c1y, 0));
    painter.AddLine(gp_Pnt(c1x, c1y, 0), gp_Pnt(c2x, c2y, 0));
    painter.AddLine(gp_Pnt(c2x, c2y, 0), gp_Pnt(c3x, c3y, 0));
    painter.AddLine(gp_Pnt(c3x, c3y, 0), gp_Pnt(c0x, c0y, 0));

    painter.AddLine(gp_Pnt(m_x0, m_y0, 0), gp_Pnt(m_x1, m_y1, 0));
}

bool LineGaugeShape::exportLine(CxShapePoint& p0, CxShapePoint& p1) const
{
    p0.x = m_x0;
    p0.y = m_y0;
    p1.x = m_x1;
    p1.y = m_y1;
    return true;
}

bool LineGaugeShape::snapshot(CxShapeGeometrySnapshot& out) const
{
    out.kind = CxShapeKind::LineGauge;
    out.points.clear();
    out.points.push_back({ m_x0, m_y0 });
    out.points.push_back({ m_x1, m_y1 });
    out.center = { (m_x0 + m_x1) * 0.5, (m_y0 + m_y1) * 0.5 };
    out.half_width = m_halfWidth;
    return true;
}