#pragma once

#include "metrology_analytics/CxPhysUnit.h"

#include <string>

namespace cxvision::metrology_analytics
{

struct CxSurfaceFieldSnapshot
{
    int xres = 0;
    int yres = 0;
    CxPhysUnit unit;
    double min_z = 0.0;
    double max_z = 0.0;
    double mean_z = 0.0;
    std::string source_ref;
};

} // namespace cxvision::metrology_analytics

