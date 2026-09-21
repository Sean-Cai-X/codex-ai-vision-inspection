#include "CxShapeHandle.h"

namespace cxgeom
{
CxShapeHandle::CxShapeHandle()
  : myEntityId(0)
  , myName()
  , myKind(CxShapeKind::Unknown)
  , myNativeShape()
{
}

CxShapeHandle::CxShapeHandle(int entity_id, std::string name, CxShapeKind kind)
  : myEntityId(entity_id)
  , myName(name)
  , myKind(kind)
  , myNativeShape()
{
}

bool CxShapeHandle::IsNull() const
{
  return myNativeShape.IsNull();
}

int CxShapeHandle::EntityId() const
{
  return myEntityId;
}

const std::string& CxShapeHandle::Name() const
{
  return myName;
}

CxShapeKind CxShapeHandle::Kind() const
{
  return myKind;
}

void CxShapeHandle::SetNativeShape(const TopoDS_Shape& native_shape)
{
  myNativeShape = native_shape;
}

const TopoDS_Shape& CxShapeHandle::NativeShape() const
{
  return myNativeShape;
}
}
