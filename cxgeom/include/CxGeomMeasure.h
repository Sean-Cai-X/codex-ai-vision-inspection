#pragma once

#include "CxShapeHandle.h"

namespace cxgeom
{
struct CxGeomMeasureResult
{
  const char* metric_name;
  double value;
};

class CxGeomMeasure
{
public:
  CxGeomMeasureResult MeasureLength(const CxShapeHandle& shape) const;
  CxGeomMeasureResult MeasureArea(const CxShapeHandle& shape) const;
  CxGeomMeasureResult MeasureDistance(const CxShapeHandle& lhs,
                                      const CxShapeHandle& rhs) const;
};
}
