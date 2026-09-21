#pragma once

#include "CxShapeHandle.h"

#include <vector>

namespace cxgeom
{
class CxWireBuilder
{
public:
  CxShapeHandle BuildWireFromCurves(int entity_id,
                                    const char* name,
                                    const std::vector<CxShapeHandle>& curves) const;
};
}
