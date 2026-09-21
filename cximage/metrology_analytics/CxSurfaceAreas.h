#pragma once

#include "metrology_analytics/CxSurfaceField.h"

namespace cxvision::metrology_analytics
{

struct CxSurfaceAreaResult
{
    double projected_area = 0.0;
    double surface_area = 0.0;
    double area_ratio = 0.0;
};

CxSurfaceAreaResult computeProjectedAndSurfaceArea(const CxSurfaceField& field);

} // namespace cxvision::metrology_analytics

