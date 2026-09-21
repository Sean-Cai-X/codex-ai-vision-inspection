#include "CxCurveBuilder.h"
#include "CxGeometryOperations.h"
#include "CxOcctConvert.h"
#include "CxSceneMapping.h"

#include <iostream>

namespace
{
bool Check(bool condition, const char* message)
{
  if (!condition)
  {
    std::cerr << "[cxgeom_scene_mapping_smoke] " << message << '\n';
    return false;
  }

  return true;
}
}

int main()
{
  cxgeom::CxCurveBuilder builder;
  cxgeom::CxShapeHandle shape = builder.BuildLine(7, "scene_line", 0.0, 0.0, 0.0, 1.0, 0.0, 0.0);

  cxgeom::CxGeometryItem item(shape);
  cxgeom::CxGeomRenderStyle style;
  style.visible = true;
  item.SetStyle(style);

  cxgeom::CxOcctConvert convert;
  item.SetPresentation(convert.MakePresentation(shape, style));

  const cxgeom::CxGeometryOperationResult add_result =
    cxgeom::CxGeometryOperations::AddGeometry(item.Revision());
  item.SetRevision(add_result.revisions);

  const cxgeom::CxGeometrySceneRecord record = cxgeom::CxSceneMapping::MakeRecord(item, add_result);

  if (!Check(record.entity_id == 7, "entity id mismatch"))
  {
    return 1;
  }

  if (!Check(record.shape_kind == cxgeom::CxShapeKind::Curve, "shape kind mismatch"))
  {
    return 1;
  }

  if (!Check(record.has_payload, "record should expose payload"))
  {
    return 1;
  }

  if (!Check(record.has_presentation, "record should expose presentation"))
  {
    return 1;
  }

  if (!Check(record.geometry_revision == 1, "geometry revision mismatch"))
  {
    return 1;
  }

  if (!Check(cxgeom::CxSceneMapping::CanPublish(record), "record should be publishable"))
  {
    return 1;
  }

  std::cout << "[cxgeom_scene_mapping_smoke]"
            << " entity_id=" << record.entity_id
            << " has_payload=" << record.has_payload
            << " has_presentation=" << record.has_presentation
            << " visible=" << record.visible
            << " geometry_revision=" << record.geometry_revision
            << '\n';
  std::cout << "[cxgeom_scene_mapping_smoke] ok" << '\n';
  return 0;
}
