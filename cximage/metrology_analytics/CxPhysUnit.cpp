#include "pch.h"
#include "metrology_analytics/CxPhysUnit.h"

#include <cmath>
#include <stdexcept>

namespace cxvision::metrology_analytics
{

bool CxPhysUnit::operator==(const CxPhysUnit& other) const
{
    return x_unit == other.x_unit &&
           y_unit == other.y_unit &&
           z_unit == other.z_unit &&
           x_scale_per_pixel == other.x_scale_per_pixel &&
           y_scale_per_pixel == other.y_scale_per_pixel &&
           z_scale_per_pixel == other.z_scale_per_pixel;
}

const char* ToString(CxLengthUnit unit)
{
    switch (unit)
    {
    case CxLengthUnit::Pixel: return "Pixel";
    case CxLengthUnit::Nanometer: return "Nanometer";
    case CxLengthUnit::Micrometer: return "Micrometer";
    case CxLengthUnit::Millimeter: return "Millimeter";
    case CxLengthUnit::Meter: return "Meter";
    default: return "Unknown";
    }
}

bool TryParseLengthUnit(const std::string& text, CxLengthUnit& out_unit)
{
    if (text == "Pixel" || text == "px")
    {
        out_unit = CxLengthUnit::Pixel;
        return true;
    }
    if (text == "Nanometer" || text == "nm")
    {
        out_unit = CxLengthUnit::Nanometer;
        return true;
    }
    if (text == "Micrometer" || text == "um" || text == "micron")
    {
        out_unit = CxLengthUnit::Micrometer;
        return true;
    }
    if (text == "Millimeter" || text == "mm")
    {
        out_unit = CxLengthUnit::Millimeter;
        return true;
    }
    if (text == "Meter" || text == "m")
    {
        out_unit = CxLengthUnit::Meter;
        return true;
    }
    return false;
}

bool IsValidPhysUnit(const CxPhysUnit& unit)
{
    return std::isfinite(unit.x_scale_per_pixel) &&
           std::isfinite(unit.y_scale_per_pixel) &&
           std::isfinite(unit.z_scale_per_pixel) &&
           unit.x_scale_per_pixel > 0.0 &&
           unit.y_scale_per_pixel > 0.0 &&
           unit.z_scale_per_pixel > 0.0;
}

void ValidatePhysUnitOrThrow(const CxPhysUnit& unit)
{
    if (!IsValidPhysUnit(unit))
        throw std::invalid_argument("CxPhysUnit requires finite positive x/y/z scale values.");
}

} // namespace cxvision::metrology_analytics

