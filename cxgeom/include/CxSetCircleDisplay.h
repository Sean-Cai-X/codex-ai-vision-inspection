#pragma once

#include "CxGeomPresentation.h"
#include "CxGeomRenderStyle.h"
#include "CxShapeHandle.h"

#include <cstddef>
#include <string>
#include <vector>

namespace cxgeom
{
struct CxSetCircleDisplayRequest
{
  int entity_id = 0;
  std::string batch_name = "setcircle_batch";
  std::vector<CxShapeHandle> shapes;
  CxGeomRenderStyle style;
};

struct CxSetCircleDisplayResult
{
  CxShapeHandle batch_shape;
  CxGeomPresentation presentation;
  std::size_t source_count = 0;
  bool success = false;
};

class CxSetCircleDisplay
{
public:
  CxSetCircleDisplayResult MakeBatch(const CxSetCircleDisplayRequest& request) const;
};
}
