#include "pch.h"
#include "CircleShape.h"

namespace
{
double NormalizeSignedDegrees(double degrees)
{
    double result = std::fmod(degrees, 360.0);
    if (result <= -180.0) result += 360.0;
    if (result > 180.0) result -= 360.0;
    return result;
}

CxShapePoint PointOnCircle(double cx, double cy, double radius, double degrees)
{
    const double radians = degrees * CV_PI / 180.0;
    return { cx + radius * std::cos(radians),
             cy + radius * std::sin(radians) };
}

} // namespace

CircleShape::CircleShape()
    : m_cx(0.0), m_cy(0.0), m_radius(50.0), m_innerRadius(0.0)
{
}

CircleShape::CircleShape(double cx, double cy, double radius, double inner_radius)
    : m_cx(cx), m_cy(cy), m_radius(std::max(1.0, radius)), m_innerRadius(inner_radius)
{
}

void CircleShape::setCenter(double cx, double cy)
{
    m_cx = cx;
    m_cy = cy;
}

void CircleShape::setRadius(double r)
{
    m_radius = std::max(1.0, r);
}

void CircleShape::setInnerRadius(double r)
{
    m_innerRadius = std::max(0.0, std::min(r, m_radius - 1.0));
}

void CircleShape::setScanSector(bool enabled,
                                double start_degrees,
                                double end_degrees)
{
    // 0..360 (and signed equivalents such as -180..180) is a full circle,
    // not a one-degree sector.  Do not normalize it first: that would turn
    // 360 into 0 and make two artificial A0/A1 handles at Rout.
    const bool full_turn = std::abs(end_degrees - start_degrees) >= 359.5;
    m_hasScanSector = enabled && !full_turn;
    if (!m_hasScanSector)
    {
        m_scanSectorStartDegrees = 0.0;
        m_scanSectorEndDegrees = 360.0;
        return;
    }

    m_scanSectorStartDegrees = NormalizeSignedDegrees(start_degrees);
    m_scanSectorEndDegrees = NormalizeSignedDegrees(end_degrees);
    if (std::abs(m_scanSectorStartDegrees - m_scanSectorEndDegrees) < 0.001)
        m_scanSectorEndDegrees =
            NormalizeSignedDegrees(m_scanSectorStartDegrees + 1.0);
}

CxShapePoint CircleShape::scanSectorInnerPoint(int endpoint_index) const
{
    const double degrees = endpoint_index == 0
        ? m_scanSectorStartDegrees
        : m_scanSectorEndDegrees;
    return PointOnCircle(m_cx, m_cy, m_innerRadius, degrees);
}

CxShapePoint CircleShape::scanSectorBoundaryPoint(int endpoint_index) const
{
    const double degrees = endpoint_index == 0
        ? m_scanSectorStartDegrees
        : m_scanSectorEndDegrees;
    return PointOnCircle(m_cx, m_cy, m_radius, degrees);
}

CxShapePoint CircleShape::scanSectorHandlePoint(int endpoint_index) const
{
    return scanSectorBoundaryPoint(endpoint_index);
}

CxShapeHit CircleShape::hitTest(double x, double y, double tolerance) const
{
    const double dx = x - m_cx;
    const double dy = y - m_cy;
    const double dist_sq = dx * dx + dy * dy;
    const double t_sq = tolerance * tolerance;

    if (dist_sq <= t_sq)
        return { true, CxShapeHandleRole::Center, -1, std::sqrt(dist_sq) };

    const double dist = std::sqrt(dist_sq);
    // A0 may intentionally coincide with Rout at zero degrees.  Angle handles
    // must win HitTest there; Rout remains draggable from the rest of the
    // outer circumference.
    if (m_hasScanSector)
    {
        const CxShapePoint a0 = scanSectorHandlePoint(0);
        const CxShapePoint a1 = scanSectorHandlePoint(1);
        const double a0dx = x - a0.x;
        const double a0dy = y - a0.y;
        if (a0dx * a0dx + a0dy * a0dy <= t_sq)
            return { true, CxShapeHandleRole::Vertex, 0,
                     std::sqrt(a0dx * a0dx + a0dy * a0dy) };
        const double a1dx = x - a1.x;
        const double a1dy = y - a1.y;
        if (a1dx * a1dx + a1dy * a1dy <= t_sq)
            return { true, CxShapeHandleRole::Vertex, 1,
                     std::sqrt(a1dx * a1dx + a1dy * a1dy) };
    }

    const double radius_dist = std::abs(dist - m_radius);
    if (radius_dist <= tolerance)
        return { true, CxShapeHandleRole::Radius, -1, radius_dist };

    if (m_innerRadius > 0)
    {
        const double inner_dist = std::abs(dist - m_innerRadius);
        if (inner_dist <= tolerance)
            return { true, CxShapeHandleRole::InnerRadius, -1, inner_dist };
    }

    if (dist >= m_innerRadius - tolerance && dist <= m_radius + tolerance)
        return { true, CxShapeHandleRole::Body, -1, std::min(std::abs(dist - m_innerRadius), std::abs(dist - m_radius)) };

    return {};
}

void CircleShape::enumerateHandles(std::vector<CxShapeHandle>& out) const
{
    out.push_back({ CxShapeHandleRole::Center, -1, { m_cx, m_cy }, "C" });
    out.push_back({ CxShapeHandleRole::Radius, -1, { m_cx + m_radius, m_cy }, "Rout" });
    
    if (m_innerRadius > 0)
        out.push_back({ CxShapeHandleRole::InnerRadius, -1, { m_cx + m_innerRadius, m_cy }, "Rin" });
    if (m_hasScanSector)
    {
        out.push_back({ CxShapeHandleRole::Vertex, 0,
                        scanSectorHandlePoint(0), "A0" });
        out.push_back({ CxShapeHandleRole::Vertex, 1,
                        scanSectorHandlePoint(1), "A1" });
    }
}

void CircleShape::dragHandle(CxShapeHandleRole role, int vertex_index, double x, double y)
{
    (void)vertex_index;

    switch (role)
    {
    case CxShapeHandleRole::Center:
    case CxShapeHandleRole::Body:
    {
        const double dx = x - m_cx;
        const double dy = y - m_cy;
        m_cx += dx;
        m_cy += dy;
        break;
    }
    case CxShapeHandleRole::Radius:
    {
        const double dx = x - m_cx;
        const double dy = y - m_cy;
        const double r = std::max(1.0, std::sqrt(dx * dx + dy * dy));
        m_radius = r;
        break;
    }
    case CxShapeHandleRole::InnerRadius:
    {
        const double dx = x - m_cx;
        const double dy = y - m_cy;
        const double r = std::min(m_radius - 1.0, std::max(0.0, std::sqrt(dx * dx + dy * dy)));
        m_innerRadius = r;
        break;
    }
    case CxShapeHandleRole::Vertex:
    {
        if (!m_hasScanSector || (vertex_index != 0 && vertex_index != 1))
            break;
        const double degrees = NormalizeSignedDegrees(
            std::atan2(y - m_cy, x - m_cx) * 180.0 / CV_PI);
        if (vertex_index == 0)
            m_scanSectorStartDegrees = degrees;
        else
            m_scanSectorEndDegrees = degrees;
        break;
    }
    default:
        break;
    }
}

void CircleShape::translateBy(double dx, double dy)
{
    m_cx += dx;
    m_cy += dy;
}

void CircleShape::exportPoints(std::vector<CxShapePoint>& out) const
{
    out.clear();
    out.push_back({ m_cx, m_cy });
    out.push_back({ m_cx + m_radius, m_cy });
}

void CircleShape::drawshape(gp_Path& painter)
{
    painter.AddCircle(gp_Pnt(m_cx, m_cy, 0), m_radius);
    
    if (m_innerRadius > 0)
        painter.AddCircle(gp_Pnt(m_cx, m_cy, 0), m_innerRadius);
}

bool CircleShape::exportCircle(CxShapePoint& center, double& radius, double& inner_radius) const
{
    center.x = m_cx;
    center.y = m_cy;
    radius = m_radius;
    inner_radius = m_innerRadius;
    return true;
}

bool CircleShape::snapshot(CxShapeGeometrySnapshot& out) const
{
    out.kind = CxShapeKind::Circle;
    out.points.clear();
    out.points.push_back({ m_cx, m_cy });
    out.points.push_back({ m_cx + m_radius, m_cy });
    out.center = { m_cx, m_cy };
    out.radius = m_radius;
    out.inner_radius = m_innerRadius;
    out.closed = true;
    return true;
}
