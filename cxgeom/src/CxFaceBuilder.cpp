#include "CxFaceBuilder.h"

#include <BRepBuilderAPI_MakeFace.hxx>
#include <TopoDS.hxx>
#include <TopAbs_ShapeEnum.hxx>

namespace cxgeom
{
CxShapeHandle CxFaceBuilder::BuildFaceFromWire(int entity_id,
                                               const char* name,
                                               const CxShapeHandle& wire) const
{
  CxShapeHandle shape(entity_id, name ? name : "face", CxShapeKind::Face);
  if (!wire.NativeShape().IsNull() && wire.NativeShape().ShapeType() == TopAbs_WIRE)
  {
    BRepBuilderAPI_MakeFace make_face(TopoDS::Wire(wire.NativeShape()));
    if (make_face.IsDone())
    {
      shape.SetNativeShape(make_face.Shape());
    }
  }
  return shape;
}
}
