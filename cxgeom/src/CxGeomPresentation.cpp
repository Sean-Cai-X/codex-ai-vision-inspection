#include "CxGeomPresentation.h"

#include <AIS_Shape.hxx>
#include <Quantity_Color.hxx>
#include <Quantity_TypeOfColor.hxx>

namespace cxgeom
{
CxGeomPresentation::CxGeomPresentation()
  : myShape()
  , myStyle()
  , myNativePresentation()
{
}

CxGeomPresentation::CxGeomPresentation(CxShapeHandle shape)
  : myShape(shape)
  , myStyle()
  , myNativePresentation()
{
}

const CxShapeHandle& CxGeomPresentation::Shape() const
{
  return myShape;
}

const CxGeomRenderStyle& CxGeomPresentation::Style() const
{
  return myStyle;
}

void CxGeomPresentation::SetStyle(const CxGeomRenderStyle& style)
{
  myStyle = style;
}

bool CxGeomPresentation::ApplyStyle()
{
  if (myNativePresentation.IsNull())
  {
    return false;
  }

  Handle(AIS_Shape) shape_presentation = Handle(AIS_Shape)::DownCast(myNativePresentation);
  if (shape_presentation.IsNull())
  {
    return false;
  }

  shape_presentation->SetColor(Quantity_Color(myStyle.red,
                                              myStyle.green,
                                              myStyle.blue,
                                              Quantity_TOC_RGB));
  shape_presentation->SetWidth(static_cast<Standard_Real>(myStyle.line_width));
  return true;
}

bool CxGeomPresentation::HasPresentation() const
{
  return !myNativePresentation.IsNull();
}

void CxGeomPresentation::SetNativePresentation(const Handle(AIS_InteractiveObject)& native_presentation)
{
  myNativePresentation = native_presentation;
}

const Handle(AIS_InteractiveObject)& CxGeomPresentation::NativePresentation() const
{
  return myNativePresentation;
}
}
