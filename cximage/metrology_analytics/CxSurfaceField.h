#pragma once

#include "metrology_analytics/CxPhysUnit.h"
#include "metrology_analytics/CxSurfaceFieldSnapshot.h"

#include <functional>
#include <vector>

namespace cxvision::metrology_analytics
{

class CxSurfaceField
{
public:
    CxSurfaceField() = default;
    CxSurfaceField(int xres, int yres, const CxPhysUnit& unit = {});

    const CxPhysUnit& unit() const { return m_unit; }
    int xres() const { return m_xres; }
    int yres() const { return m_yres; }
    int valueCount() const { return static_cast<int>(m_z.size()); }

    double at(int x, int y) const;
    void setAt(int x, int y, double value);

    double physicalWidth() const { return static_cast<double>(m_xres) * m_unit.x_scale_per_pixel; }
    double physicalHeight() const { return static_cast<double>(m_yres) * m_unit.y_scale_per_pixel; }
    double projectedPhysicalArea() const { return physicalWidth() * physicalHeight(); }

    const float* rawData() const { return m_z.empty() ? nullptr : m_z.data(); }

    void fillFromGenerator(const std::function<double(int x, int y)>& z_func);
    CxSurfaceFieldSnapshot snapshot(const std::string& source_ref = std::string()) const;

private:
    int indexOf(int x, int y) const;

    int m_xres = 0;
    int m_yres = 0;
    CxPhysUnit m_unit;
    std::vector<float> m_z;
};

} // namespace cxvision::metrology_analytics

