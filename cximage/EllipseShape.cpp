#include "pch.h"
#include "EllipseShape.h"

EllipseShape::EllipseShape()
    : m_cx(0.0), m_cy(0.0), m_rx(50.0), m_ry(30.0), m_angleDeg(0.0)
{
}

EllipseShape::EllipseShape(double center_x, double center_y, double radius_x, double radius_y)
    : m_cx(center_x), m_cy(center_y),
      m_rx(std::max(1.0, radius_x)), m_ry(std::max(1.0, radius_y)),
      m_angleDeg(0.0)
{
}

EllipseShape::EllipseShape(
    double center_x,
    double center_y,
    double radius_x,
    double radius_y,
    double angle_deg)
    : m_cx(center_x), m_cy(center_y),
      m_rx(std::max(1.0, radius_x)), m_ry(std::max(1.0, radius_y)),
      m_angleDeg(angle_deg)
{
}

void EllipseShape::setCenter(double cx, double cy)
{
    m_cx = cx;
    m_cy = cy;
}

void EllipseShape::setRadiusX(double rx)
{
    m_rx = std::max(1.0, rx);
}

void EllipseShape::setRadiusY(double ry)
{
    m_ry = std::max(1.0, ry);
}

void EllipseShape::setAngleDegrees(double angle_deg)
{
    m_angleDeg = angle_deg;
}

void EllipseShape::setInnerScalePercent(double percent)
{
    m_innerScalePercent = std::max(0.0, std::min(99.0, percent));
}

static double MinDistanceToSegment(
    double px, double py,
    double x0, double y0,
    double x1, double y1)
{
    const double dx = x1 - x0;
    const double dy = y1 - y0;
    const double len2 = dx * dx + dy * dy;

    if (len2 < 1.0e-12)
    {
        const double dx_p = px - x0;
        const double dy_p = py - y0;
        return std::sqrt(dx_p * dx_p + dy_p * dy_p);
    }

    double t = ((px - x0) * dx + (py - y0) * dy) / len2;
    t = std::max(0.0, std::min(1.0, t));

    const double near_x = x0 + t * dx;
    const double near_y = y0 + t * dy;
    const double dx_n = px - near_x;
    const double dy_n = py - near_y;

    return std::sqrt(dx_n * dx_n + dy_n * dy_n);
}

static double MinDistanceToClosedPolyline(
    const std::vector<CxShapePoint>& points,
    double x, double y)
{
    if (points.size() < 2)
        return std::numeric_limits<double>::max();

    double minDist = std::numeric_limits<double>::max();
    const int n = static_cast<int>(points.size());

    for (int i = 0; i < n; ++i)
    {
        const int j = (i + 1) % n;
        const double dist = MinDistanceToSegment(
            x, y,
            points[i].x, points[i].y,
            points[j].x, points[j].y);
        minDist = std::min(minDist, dist);
    }

    return minDist;
}

void EllipseShape::EnumerateBoundaryPoints(
    std::vector<CxShapePoint>& out,
    int segments) const
{
    out.clear();
    const int count = std::max(24, segments);
    const double rotation = RADIAN(m_angleDeg);
    const double c = std::cos(rotation);
    const double s = std::sin(rotation);

    for (int i = 0; i < count; ++i)
    {
        const double angle = 2.0 * PI * static_cast<double>(i) / static_cast<double>(count);
        const double local_x = m_rx * std::cos(angle);
        const double local_y = m_ry * std::sin(angle);
        out.push_back({
            m_cx + local_x * c - local_y * s,
            m_cy + local_x * s + local_y * c
        });
    }
}

CxShapeHit EllipseShape::hitTest(double x, double y, double tolerance) const
{
    const double dx = x - m_cx;
    const double dy = y - m_cy;
    const double t_sq = tolerance * tolerance;
    const double rotation = RADIAN(m_angleDeg);
    const double c = std::cos(rotation);
    const double s = std::sin(rotation);

    if (dx * dx + dy * dy <= t_sq)
        return { true, CxShapeHandleRole::Center, -1, std::sqrt(dx * dx + dy * dy) };

    const double rx_handle_x = m_cx + m_rx * c;
    const double rx_handle_y = m_cy + m_rx * s;
    const double ry_handle_x = m_cx - m_ry * s;
    const double ry_handle_y = m_cy + m_ry * c;

    const double rx_handle_dist_sq = (x - rx_handle_x) * (x - rx_handle_x) + (y - rx_handle_y) * (y - rx_handle_y);
    if (rx_handle_dist_sq <= t_sq)
        return { true, CxShapeHandleRole::RadiusX, -1, std::sqrt(rx_handle_dist_sq) };

    const double ry_handle_dist_sq = (x - ry_handle_x) * (x - ry_handle_x) + (y - ry_handle_y) * (y - ry_handle_y);
    if (ry_handle_dist_sq <= t_sq)
        return { true, CxShapeHandleRole::RadiusY, -1, std::sqrt(ry_handle_dist_sq) };

    if (m_innerScalePercent > 0.0)
    {
        EllipseShape inner(m_cx, m_cy, innerRadiusX(), innerRadiusY(), m_angleDeg);
        std::vector<CxShapePoint> innerBoundary;
        inner.EnumerateBoundaryPoints(innerBoundary, 96);
        const double innerBoundaryDistance =
            MinDistanceToClosedPolyline(innerBoundary, x, y);
        if (innerBoundaryDistance <= tolerance)
        {
            return {
                true,
                CxShapeHandleRole::InnerRadius,
                -1,
                innerBoundaryDistance
            };
        }
    }

    std::vector<CxShapePoint> boundary;
    EnumerateBoundaryPoints(boundary, 96);
    const double boundaryDistance = MinDistanceToClosedPolyline(boundary, x, y);

    if (boundaryDistance <= tolerance)
    {
        return { true, CxShapeHandleRole::Body, -1, boundaryDistance };
    }

    return {};
}

void EllipseShape::enumerateHandles(std::vector<CxShapeHandle>& out) const
{
    const double rotation = RADIAN(m_angleDeg);
    const double c = std::cos(rotation);
    const double s = std::sin(rotation);
    out.push_back({ CxShapeHandleRole::Center, -1, { m_cx, m_cy }, "C" });
    out.push_back({ CxShapeHandleRole::RadiusX, -1, { m_cx + m_rx * c, m_cy + m_rx * s }, "Rx" });
    out.push_back({ CxShapeHandleRole::RadiusY, -1, { m_cx - m_ry * s, m_cy + m_ry * c }, "Ry" });
    if (m_innerScalePercent > 0.0)
    {
        const double inner_rx = innerRadiusX();
        out.push_back({
            CxShapeHandleRole::InnerRadius,
            -1,
            { m_cx + inner_rx * c, m_cy + inner_rx * s },
            "Rin"
        });
    }
}

void EllipseShape::dragHandle(CxShapeHandleRole role, int vertex_index, double x, double y)
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
    case CxShapeHandleRole::RadiusX:
    {
        const double rotation = RADIAN(m_angleDeg);
        const double axis_x = std::cos(rotation);
        const double axis_y = std::sin(rotation);
        const double rx = std::max(1.0, std::abs((x - m_cx) * axis_x + (y - m_cy) * axis_y));
        m_rx = rx;
        break;
    }
    case CxShapeHandleRole::RadiusY:
    {
        const double rotation = RADIAN(m_angleDeg);
        const double axis_x = -std::sin(rotation);
        const double axis_y = std::cos(rotation);
        const double ry = std::max(1.0, std::abs((x - m_cx) * axis_x + (y - m_cy) * axis_y));
        m_ry = ry;
        break;
    }
    case CxShapeHandleRole::InnerRadius:
    {
        const double rotation = RADIAN(m_angleDeg);
        const double axis_x = std::cos(rotation);
        const double axis_y = std::sin(rotation);
        const double projected =
            std::abs((x - m_cx) * axis_x + (y - m_cy) * axis_y);
        const double inner_rx = std::min(m_rx - 1.0, std::max(0.0, projected));
        m_innerScalePercent = m_rx > 1.0
            ? std::max(0.0, std::min(99.0, inner_rx * 100.0 / m_rx))
            : 0.0;
        break;
    }
    default:
        break;
    }
}

void EllipseShape::translateBy(double dx, double dy)
{
    m_cx += dx;
    m_cy += dy;
}

void EllipseShape::exportPoints(std::vector<CxShapePoint>& out) const
{
    const double rotation = RADIAN(m_angleDeg);
    const double c = std::cos(rotation);
    const double s = std::sin(rotation);
    out.clear();
    out.push_back({ m_cx, m_cy });
    out.push_back({ m_cx + m_rx * c, m_cy + m_rx * s });
    out.push_back({ m_cx - m_ry * s, m_cy + m_ry * c });
}

void EllipseShape::drawshape(gp_Path& painter)
{
    gp_Pnt p0(m_cx - m_rx, m_cy - m_ry, 0);
    gp_Pnt p1(m_cx + m_rx, m_cy + m_ry, 0);
    painter.AddRectangularEllipse(p0, p1);
    if (m_innerScalePercent > 0.0)
    {
        const double inner_rx = innerRadiusX();
        const double inner_ry = innerRadiusY();
        gp_Pnt inner0(m_cx - inner_rx, m_cy - inner_ry, 0);
        gp_Pnt inner1(m_cx + inner_rx, m_cy + inner_ry, 0);
        painter.AddRectangularEllipse(inner0, inner1);
    }
}

bool EllipseShape::exportEllipse(CxShapePoint& center, double& radius_x, double& radius_y, double& angle) const
{
    center.x = m_cx;
    center.y = m_cy;
    radius_x = m_rx;
    radius_y = m_ry;
    angle = m_angleDeg;
    return true;
}

bool EllipseShape::snapshot(CxShapeGeometrySnapshot& out) const
{
    out.kind = CxShapeKind::Ellipse;
    out.points.clear();
    out.points.push_back({ m_cx, m_cy });
    const double rotation = RADIAN(m_angleDeg);
    const double c = std::cos(rotation);
    const double s = std::sin(rotation);
    out.points.push_back({ m_cx + m_rx * c, m_cy + m_rx * s });
    out.points.push_back({ m_cx - m_ry * s, m_cy + m_ry * c });
    out.center = { m_cx, m_cy };
    out.radius_x = m_rx;
    out.radius_y = m_ry;
    out.angle = m_angleDeg;
    out.closed = true;
    return true;
}
