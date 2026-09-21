#include "pch.h"
#include "CircleRingGauge.h"

#include <cmath>
#include <sstream>

void CircleRingGauge::settolerance(
    double center_tolerance,
    double thickness_tolerance)
{
    if (std::isfinite(center_tolerance) && center_tolerance > 0.0)
        m_center_tolerance = center_tolerance;

    if (std::isfinite(thickness_tolerance) && thickness_tolerance > 0.0)
        m_thickness_tolerance = thickness_tolerance;
}

void CircleRingGauge::setouter(void* outer_circle_ptr)
{
    m_outer_circle = static_cast<FindCircle*>(outer_circle_ptr);
}

void CircleRingGauge::setinner(void* inner_circle_ptr)
{
    m_inner_circle = static_cast<FindCircle*>(inner_circle_ptr);
}

void CircleRingGauge::build()
{
    m_has_result = false;
    m_status = "failed";
    m_reason.clear();
    m_result_ref.clear();

    if (!m_outer_circle || !m_inner_circle)
    {
        m_reason = "outer_circle or inner_circle is null";
        return;
    }

    double outer_cx = m_outer_circle->getresultcentx();
    double outer_cy = m_outer_circle->getresultcenty();
    m_outer_radius = m_outer_circle->getradius();

    double inner_cx = m_inner_circle->getresultcentx();
    double inner_cy = m_inner_circle->getresultcenty();
    m_inner_radius = m_inner_circle->getradius();

    if (!std::isfinite(outer_cx) || !std::isfinite(outer_cy) || !std::isfinite(m_outer_radius))
    {
        m_reason = "outer circle has invalid geometry";
        return;
    }

    if (!std::isfinite(inner_cx) || !std::isfinite(inner_cy) || !std::isfinite(m_inner_radius))
    {
        m_reason = "inner circle has invalid geometry";
        return;
    }

    if (m_outer_radius <= 0.0)
    {
        m_reason = "outer circle radius must be positive";
        return;
    }

    if (m_inner_radius <= 0.0)
    {
        m_reason = "inner circle radius must be positive";
        return;
    }

    m_thickness = m_outer_radius - m_inner_radius;

    m_center_distance =
        std::hypot(
            outer_cx - inner_cx,
            outer_cy - inner_cy);

    m_concentric_ok =
        std::isfinite(m_center_distance) &&
        m_center_distance <= m_center_tolerance;

    m_inside_ok =
        m_inner_radius < m_outer_radius;

    m_thickness_ok =
        std::isfinite(m_thickness) &&
        m_thickness > 0.0 &&
        std::abs(m_thickness) <= m_thickness_tolerance;

    double concentric_score = m_concentric_ok ? 1.0 :
        (m_center_distance > 0.0 ? std::max(0.0, 1.0 - m_center_distance / m_center_tolerance) : 0.0);
    double inside_score = m_inside_ok ? 1.0 : 0.0;
    double thickness_score = m_thickness_ok ? 1.0 :
        (m_thickness > 0.0 ? std::max(0.0, 1.0 - std::abs(m_thickness) / m_thickness_tolerance) : 0.0);

    m_score = (concentric_score + inside_score + thickness_score) / 3.0;

    std::ostringstream ref;
    ref << "ring_gauge<=outer_circle+inner_circle"
        << "|outer_r=" << m_outer_radius
        << "|inner_r=" << m_inner_radius
        << "|thickness=" << m_thickness
        << "|center_dist=" << m_center_distance;

    m_result_ref = ref.str();

    if (m_concentric_ok && m_inside_ok && m_thickness_ok)
    {
        m_status = "ring_gauge_ok";
        m_reason = "outer and inner circle form a valid ring gauge";
        m_has_result = true;
    }
    else
    {
        m_status = "ring_gauge_marginal";
        m_reason = "ring gauge built but one or more checks failed";
        m_has_result = true;
    }
}

int CircleRingGauge::has_result()
{
    return m_has_result ? 1 : 0;
}

double CircleRingGauge::outer_radius()
{
    return m_outer_radius;
}

double CircleRingGauge::inner_radius()
{
    return m_inner_radius;
}

double CircleRingGauge::thickness()
{
    return m_thickness;
}

double CircleRingGauge::center_distance()
{
    return m_center_distance;
}

int CircleRingGauge::concentric_ok()
{
    return m_concentric_ok ? 1 : 0;
}

int CircleRingGauge::inside_ok()
{
    return m_inside_ok ? 1 : 0;
}

int CircleRingGauge::thickness_ok()
{
    return m_thickness_ok ? 1 : 0;
}

double CircleRingGauge::score()
{
    return m_score;
}

int CircleRingGauge::status_code()
{
    return StatusCodeFromString(m_status);
}

int CircleRingGauge::StatusCodeFromString(const std::string& status) const
{
    if (status == "ring_gauge_ok") return 0;
    if (status == "ring_gauge_marginal") return 1;
    if (status == "failed") return 2;
    return -1;
}
