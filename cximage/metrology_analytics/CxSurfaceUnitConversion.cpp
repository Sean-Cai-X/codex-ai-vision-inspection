#include "pch.h"
#include "metrology_analytics/CxSurfaceUnitConversion.h"

#include <stdexcept>

namespace cxvision::metrology_analytics
{
namespace
{
double UnitToMeter(CxLengthUnit unit)
{
    switch (unit)
    {
    case CxLengthUnit::Pixel: return 1.0;
    case CxLengthUnit::Nanometer: return 1e-9;
    case CxLengthUnit::Micrometer: return 1e-6;
    case CxLengthUnit::Millimeter: return 1e-3;
    case CxLengthUnit::Meter: return 1.0;
    default: throw std::invalid_argument("unknown CxLengthUnit.");
    }
}
}

double convertLength(double value, CxLengthUnit from, CxLengthUnit to)
{
    if (from == CxLengthUnit::Pixel || to == CxLengthUnit::Pixel)
    {
        if (from == to)
            return value;
        throw std::invalid_argument("Pixel length conversion requires an explicit scale and cannot be converted directly.");
    }
    return value * UnitToMeter(from) / UnitToMeter(to);
}

CxPhysUnit rescaleUnit(const CxPhysUnit& unit, CxLengthUnit new_x, CxLengthUnit new_y, CxLengthUnit new_z)
{
    ValidatePhysUnitOrThrow(unit);
    CxPhysUnit out = unit;
    out.x_scale_per_pixel = convertLength(unit.x_scale_per_pixel, unit.x_unit, new_x);
    out.y_scale_per_pixel = convertLength(unit.y_scale_per_pixel, unit.y_unit, new_y);
    out.z_scale_per_pixel = convertLength(unit.z_scale_per_pixel, unit.z_unit, new_z);
    out.x_unit = new_x;
    out.y_unit = new_y;
    out.z_unit = new_z;
    return out;
}

CxUncertaintyStatus deriveStatusFromUnit(const CxPhysUnit& unit, bool has_z_repeatability_std)
{
    if (unit.x_unit == CxLengthUnit::Pixel ||
        unit.y_unit == CxLengthUnit::Pixel ||
        unit.z_unit == CxLengthUnit::Pixel)
    {
        return CxUncertaintyStatus::Missing;
    }
    return has_z_repeatability_std ? CxUncertaintyStatus::Complete : CxUncertaintyStatus::IncompleteZOnly;
}

const char* ToString(CxUncertaintyStatus status)
{
    switch (status)
    {
    case CxUncertaintyStatus::Complete: return "Complete";
    case CxUncertaintyStatus::IncompleteZOnly: return "IncompleteZOnly";
    case CxUncertaintyStatus::Missing: return "Missing";
    default: return "Unknown";
    }
}

} // namespace cxvision::metrology_analytics

