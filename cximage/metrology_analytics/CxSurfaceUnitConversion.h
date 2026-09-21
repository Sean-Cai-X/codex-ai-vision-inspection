#pragma once

#include "metrology_analytics/CxPhysUnit.h"

namespace cxvision::metrology_analytics
{

enum class CxUncertaintyStatus
{
    Complete = 0,
    IncompleteZOnly,
    Missing
};

double convertLength(double value, CxLengthUnit from, CxLengthUnit to);
CxPhysUnit rescaleUnit(const CxPhysUnit& unit, CxLengthUnit new_x, CxLengthUnit new_y, CxLengthUnit new_z);
CxUncertaintyStatus deriveStatusFromUnit(const CxPhysUnit& unit, bool has_z_repeatability_std);
const char* ToString(CxUncertaintyStatus status);

} // namespace cxvision::metrology_analytics

