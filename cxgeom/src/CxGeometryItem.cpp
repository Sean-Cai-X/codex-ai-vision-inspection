#include "CxGeometryItem.h"

namespace cxgeom
{
CxGeometryItem::CxGeometryItem()
  : myPayload()
  , myStyle()
  , myPresentation()
  , myRevision()
{
}

CxGeometryItem::CxGeometryItem(const CxShapeHandle& payload)
  : myPayload(payload)
  , myStyle()
  , myPresentation(payload)
  , myRevision()
{
}

int CxGeometryItem::EntityId() const
{
  return myPayload.EntityId();
}

const CxShapeHandle& CxGeometryItem::Payload() const
{
  return myPayload;
}

void CxGeometryItem::SetPayload(const CxShapeHandle& payload)
{
  myPayload = payload;
}

const CxGeomRenderStyle& CxGeometryItem::Style() const
{
  return myStyle;
}

void CxGeometryItem::SetStyle(const CxGeomRenderStyle& style)
{
  myStyle = style;
}

const CxGeomPresentation& CxGeometryItem::Presentation() const
{
  return myPresentation;
}

CxGeomPresentation& CxGeometryItem::Presentation()
{
  return myPresentation;
}

void CxGeometryItem::SetPresentation(const CxGeomPresentation& presentation)
{
  myPresentation = presentation;
}

const CxSceneRevision& CxGeometryItem::Revision() const
{
  return myRevision;
}

void CxGeometryItem::SetRevision(const CxSceneRevision& revision)
{
  myRevision = revision;
}
}
