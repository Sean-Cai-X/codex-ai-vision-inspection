#pragma once

#include "CxGeomPresentation.h"
#include "CxGeomRenderStyle.h"
#include "CxShapeHandle.h"

#include <cstddef>
#include <string>
#include <vector>

namespace cxgeom
{
struct CxSetLineDisplayRequest
{
  int entity_id = 0;
  std::string batch_name = "setline_batch";
  std::vector<CxShapeHandle> shapes;
  CxGeomRenderStyle style;
};

struct CxSetLineDisplayResult
{
  CxShapeHandle batch_shape;
  CxGeomPresentation presentation;
  std::size_t source_count = 0;
  bool success = false;
};

class CxSetLineDisplay
{
public:
  CxSetLineDisplayResult MakeBatch(const CxSetLineDisplayRequest& request) const;
};
}
