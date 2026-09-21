#pragma once

#include <TopoDS_Shape.hxx>

#include <string>

namespace cxgeom
{
enum class CxShapeKind
{
  Unknown = 0,
  Curve,
  Wire,
  Face,
  Solid
};

class CxShapeHandle
{
public:
  CxShapeHandle();
  CxShapeHandle(int entity_id, std::string name, CxShapeKind kind);

  bool IsNull() const;

  int EntityId() const;
  const std::string& Name() const;
  CxShapeKind Kind() const;

  void SetNativeShape(const TopoDS_Shape& native_shape);
  const TopoDS_Shape& NativeShape() const;

private:
  int myEntityId;
  std::string myName;
  CxShapeKind myKind;
  TopoDS_Shape myNativeShape;
};
}
