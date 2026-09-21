#include "CxOcctConvert.h"

#include <AIS_Shape.hxx>

namespace cxgeom
{
CxGeomPresentation CxOcctConvert::MakePresentation(const CxShapeHandle& shape) const
{
  return MakePresentation(shape, CxGeomRenderStyle());
}

CxGeomPresentation CxOcctConvert::MakePresentation(const CxShapeHandle& shape,
                                                   const CxGeomRenderStyle& style) const
{
  CxGeomPresentation presentation(shape);
  presentation.SetStyle(style);
  if (!shape.NativeShape().IsNull())
  {
    presentation.SetNativePresentation(new AIS_Shape(shape.NativeShape()));
    presentation.ApplyStyle();
  }
  return presentation;
}
}
