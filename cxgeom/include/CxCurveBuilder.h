#pragma once

#include "CxShapeHandle.h"

namespace cxgeom
{
class CxCurveBuilder
{
public:
  CxShapeHandle BuildLine(int entity_id, const char* name) const;
  CxShapeHandle BuildCircle(int entity_id, const char* name) const;
  CxShapeHandle BuildEllipse(int entity_id, const char* name) const;

  CxShapeHandle BuildLine(int entity_id,
                          const char* name,
                          double x0,
                          double y0,
                          double z0,
                          double x1,
                          double y1,
                          double z1) const;
  CxShapeHandle BuildCircle(int entity_id,
                            const char* name,
                            double center_x,
                            double center_y,
                            double center_z,
                            double radius) const;
  CxShapeHandle BuildEllipse(int entity_id,
                             const char* name,
                             double center_x,
                             double center_y,
                             double center_z,
                             double major_radius,
                             double minor_radius) const;
};
}
