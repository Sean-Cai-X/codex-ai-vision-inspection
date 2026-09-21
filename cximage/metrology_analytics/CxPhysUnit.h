#pragma once

#include <string>

namespace cxvision::metrology_analytics
{

enum class CxLengthUnit
{
    Pixel = 0,
    Nanometer,
    Micrometer,
    Millimeter,
    Meter
};

struct CxPhysUnit
{
    CxLengthUnit x_unit = CxLengthUnit::Pixel;
    CxLengthUnit y_unit = CxLengthUnit::Pixel;
    CxLengthUnit z_unit = CxLengthUnit::Pixel;
    double x_scale_per_pixel = 1.0;
    double y_scale_per_pixel = 1.0;
    double z_scale_per_pixel = 1.0;

    bool operator==(const CxPhysUnit& other) const;
    bool operator!=(const CxPhysUnit& other) const { return !(*this == other); }
};

const char* ToString(CxLengthUnit unit);
bool TryParseLengthUnit(const std::string& text, CxLengthUnit& out_unit);
bool IsValidPhysUnit(const CxPhysUnit& unit);
void ValidatePhysUnitOrThrow(const CxPhysUnit& unit);

} // namespace cxvision::metrology_analytics

