#pragma once

#include "CxShapeHandle.h"

namespace cxgeom
{
class CxFaceBuilder
{
public:
  CxShapeHandle BuildFaceFromWire(int entity_id,
                                  const char* name,
                                  const CxShapeHandle& wire) const;
};
}
