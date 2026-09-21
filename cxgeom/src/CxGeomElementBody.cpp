#include "CxGeomElementBody.h"

namespace cxgeom
{
CxGeomElement CxGeomElementBody::MakeElement(const CxShapeHandle& shape, bool visible)
{
  CxGeomElement element;
  element.entity_id = shape.EntityId();
  element.entity_type = "geom";
  element.name = shape.Name();
  element.source_stage = "runtime";
  element.status = "ok";
  element.kind = shape.Kind();
  element.shape = shape;
  element.measure_shape = shape;
  element.confidence = shape.IsNull() ? 0.0 : 1.0;
  element.success = !shape.IsNull();
  element.visible = visible;
  return element;
}

CxCurveElement CxGeomElementBody::MakeCurveElement(const CxShapeHandle& shape,
                                                   double length_hint,
                                                   bool closed,
                                                   bool visible)
{
  CxCurveElement element;
  element.base = MakeElement(shape, visible);
  element.curve_type = "curve";
  element.length_hint = length_hint;
  element.closed = closed;
  return element;
}

CxSurfaceElement CxGeomElementBody::MakeSurfaceElement(const CxShapeHandle& shape,
                                                       double area_hint,
                                                       bool visible)
{
  CxSurfaceElement element;
  element.base = MakeElement(shape, visible);
  element.surface_type = "surface";
  element.area_hint = area_hint;
  return element;
}

CxGeomBatchElement CxGeomElementBody::MakeBatchElement(int batch_id,
                                                       const char* name,
                                                       const std::vector<CxShapeHandle>& shapes,
                                                       bool visible)
{
  CxGeomBatchElement element;
  element.batch_id = batch_id;
  element.element_type = "batch";
  element.name = name ? name : "geom_batch";
  element.source_stage = "runtime";
  element.status = "ok";
  element.shapes = shapes;
  element.confidence = shapes.empty() ? 0.0 : 1.0;
  element.success = !shapes.empty();
  element.source_count = shapes.size();
  element.visible = visible;
  return element;
}
}
