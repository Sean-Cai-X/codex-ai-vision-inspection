#pragma once

#include "metrology_analytics/CxSurfaceField.h"

namespace cxvision::metrology_analytics
{

enum class PlaneLevelMethod
{
    ThreePoints = 0,
    OrdinaryLeastSquares,
    WeightedTlsHuber
};

struct CxPlaneCoeffs
{
    double a = 0.0;
    double b = 0.0;
    double c = 0.0;
};

CxPlaneCoeffs fitPlane(const CxSurfaceField& field, PlaneLevelMethod method);
void subtractPlaneInPlace(CxSurfaceField& field, const CxPlaneCoeffs& plane);

} // namespace cxvision::metrology_analytics

