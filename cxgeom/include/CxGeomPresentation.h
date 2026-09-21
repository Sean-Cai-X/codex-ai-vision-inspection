#pragma once

#include <AIS_InteractiveObject.hxx>

#include "CxGeomRenderStyle.h"
#include "CxShapeHandle.h"

namespace cxgeom
{
class CxGeomPresentation
{
public:
  CxGeomPresentation();
  explicit CxGeomPresentation(CxShapeHandle shape);

  const CxShapeHandle& Shape() const;
  const CxGeomRenderStyle& Style() const;
  void SetStyle(const CxGeomRenderStyle& style);
  bool ApplyStyle();

  bool HasPresentation() const;
  void SetNativePresentation(const Handle(AIS_InteractiveObject)& native_presentation);
  const Handle(AIS_InteractiveObject)& NativePresentation() const;

private:
  CxShapeHandle myShape;
  CxGeomRenderStyle myStyle;
  Handle(AIS_InteractiveObject) myNativePresentation;
};
}
