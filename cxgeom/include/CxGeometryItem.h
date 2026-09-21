#pragma once

#include "CxGeomPresentation.h"
#include "CxGeomRenderStyle.h"
#include "CxRefreshDecision.h"
#include "CxShapeHandle.h"

namespace cxgeom
{
class CxGeometryItem
{
public:
  CxGeometryItem();
  explicit CxGeometryItem(const CxShapeHandle& payload);

  int EntityId() const;
  const CxShapeHandle& Payload() const;
  void SetPayload(const CxShapeHandle& payload);

  const CxGeomRenderStyle& Style() const;
  void SetStyle(const CxGeomRenderStyle& style);

  const CxGeomPresentation& Presentation() const;
  CxGeomPresentation& Presentation();
  void SetPresentation(const CxGeomPresentation& presentation);

  const CxSceneRevision& Revision() const;
  void SetRevision(const CxSceneRevision& revision);

private:
  CxShapeHandle myPayload;
  CxGeomRenderStyle myStyle;
  CxGeomPresentation myPresentation;
  CxSceneRevision myRevision;
};
}
