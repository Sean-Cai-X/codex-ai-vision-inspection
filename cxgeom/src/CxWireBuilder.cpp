#include "CxWireBuilder.h"

#include <BRepBuilderAPI_MakeWire.hxx>
#include <TopoDS.hxx>
#include <TopAbs_ShapeEnum.hxx>

namespace cxgeom
{
CxShapeHandle CxWireBuilder::BuildWireFromCurves(int entity_id,
                                                 const char* name,
                                                 const std::vector<CxShapeHandle>& curves) const
{
  CxShapeHandle shape(entity_id, name ? name : "wire", CxShapeKind::Wire);
  BRepBuilderAPI_MakeWire make_wire;
  for (const CxShapeHandle& curve : curves)
  {
    if (!curve.NativeShape().IsNull() && curve.NativeShape().ShapeType() == TopAbs_EDGE)
    {
      make_wire.Add(TopoDS::Edge(curve.NativeShape()));
    }
  }

  if (make_wire.IsDone())
  {
    shape.SetNativeShape(make_wire.Shape());
  }
  return shape;
}
}
