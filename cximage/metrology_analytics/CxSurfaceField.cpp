#include "pch.h"
#include "metrology_analytics/CxSurfaceField.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace cxvision::metrology_analytics
{

CxSurfaceField::CxSurfaceField(int xres, int yres, const CxPhysUnit& unit)
    : m_xres(xres), m_yres(yres), m_unit(unit)
{
    if (xres <= 0 || yres <= 0)
        throw std::invalid_argument("CxSurfaceField requires positive xres and yres.");
    ValidatePhysUnitOrThrow(unit);
    m_z.assign(static_cast<std::size_t>(xres) * static_cast<std::size_t>(yres), 0.0f);
}

int CxSurfaceField::indexOf(int x, int y) const
{
    if (x < 0 || y < 0 || x >= m_xres || y >= m_yres)
        throw std::out_of_range("CxSurfaceField coordinate is out of range.");
    return y * m_xres + x;
}

double CxSurfaceField::at(int x, int y) const
{
    return static_cast<double>(m_z[static_cast<std::size_t>(indexOf(x, y))]);
}

void CxSurfaceField::setAt(int x, int y, double value)
{
    if (!std::isfinite(value))
        throw std::invalid_argument("CxSurfaceField value must be finite.");
    m_z[static_cast<std::size_t>(indexOf(x, y))] = static_cast<float>(value);
}

void CxSurfaceField::fillFromGenerator(const std::function<double(int x, int y)>& z_func)
{
    if (!z_func)
        throw std::invalid_argument("CxSurfaceField generator must be callable.");
    for (int y = 0; y < m_yres; ++y)
    {
        for (int x = 0; x < m_xres; ++x)
            setAt(x, y, z_func(x, y));
    }
}

CxSurfaceFieldSnapshot CxSurfaceField::snapshot(const std::string& source_ref) const
{
    CxSurfaceFieldSnapshot s;
    s.xres = m_xres;
    s.yres = m_yres;
    s.unit = m_unit;
    s.source_ref = source_ref;

    if (m_z.empty())
        return s;

    double sum = 0.0;
    s.min_z = std::numeric_limits<double>::infinity();
    s.max_z = -std::numeric_limits<double>::infinity();
    for (float v : m_z)
    {
        const double z = static_cast<double>(v);
        s.min_z = std::min(s.min_z, z);
        s.max_z = std::max(s.max_z, z);
        sum += z;
    }
    s.mean_z = sum / static_cast<double>(m_z.size());
    return s;
}

} // namespace cxvision::metrology_analytics

