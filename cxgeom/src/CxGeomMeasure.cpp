#include "CxGeomMeasure.h"

#include <BRepGProp.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <GProp_GProps.hxx>

namespace cxgeom
{
CxGeomMeasureResult CxGeomMeasure::MeasureLength(const CxShapeHandle& shape) const
{
  if (shape.NativeShape().IsNull())
  {
    return CxGeomMeasureResult{"length", 0.0};
  }

  GProp_GProps props;
  BRepGProp::LinearProperties(shape.NativeShape(), props);
  return CxGeomMeasureResult{"length", props.Mass()};
}

CxGeomMeasureResult CxGeomMeasure::MeasureArea(const CxShapeHandle& shape) const
{
  if (shape.NativeShape().IsNull())
  {
    return CxGeomMeasureResult{"area", 0.0};
  }

  GProp_GProps props;
  BRepGProp::SurfaceProperties(shape.NativeShape(), props);
  return CxGeomMeasureResult{"area", props.Mass()};
}

CxGeomMeasureResult CxGeomMeasure::MeasureDistance(const CxShapeHandle& lhs,
                                                   const CxShapeHandle& rhs) const
{
  if (lhs.NativeShape().IsNull() || rhs.NativeShape().IsNull())
  {
    return CxGeomMeasureResult{"distance", 0.0};
  }

  BRepExtrema_DistShapeShape extrema(lhs.NativeShape(), rhs.NativeShape());
  extrema.Perform();
  return CxGeomMeasureResult{"distance", extrema.IsDone() ? extrema.Value() : 0.0};
}
}
