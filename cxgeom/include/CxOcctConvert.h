#pragma once

#include "CxGeomPresentation.h"
#include "CxShapeHandle.h"

namespace cxgeom
{
class CxOcctConvert
{
public:
  CxGeomPresentation MakePresentation(const CxShapeHandle& shape) const;
  CxGeomPresentation MakePresentation(const CxShapeHandle& shape,
                                      const CxGeomRenderStyle& style) const;
};
}
