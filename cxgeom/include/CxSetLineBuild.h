#pragma once

#include "CxShapeHandle.h"

#include <cstddef>
#include <string>

namespace cxgeom
{
struct CxSetLineRequest
{
  int entity_id = 0;
  std::string line_name = "setline_curve";
  double x0 = 0.0;
  double y0 = 0.0;
  double z0 = 0.0;
  double x1 = 0.0;
  double y1 = 0.0;
  double z1 = 0.0;
  double gap = 0.0;
  int roi_width = 0;
  int roi_height = 0;
};

struct CxSetLineBuildMeta
{
  double length_hint = 0.0;
  std::size_t scan_count_hint = 0;
  bool compact_roi_hint = false;
};

struct CxSetLineBuildResult
{
  CxShapeHandle line_shape;
  CxShapeHandle measure_shape;
  CxSetLineBuildMeta meta;
  bool success = false;
};

class CxSetLineBuild
{
public:
  CxSetLineBuildResult Build(const CxSetLineRequest& request) const;
};
}
