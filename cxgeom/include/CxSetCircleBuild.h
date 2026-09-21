#pragma once

#include "CxShapeHandle.h"

#include <cstddef>
#include <string>

namespace cxgeom
{
struct CxSetCircleRequest
{
  int entity_id = 0;
  std::string curve_name = "setcircle_curve";
  double center_x = 0.0;
  double center_y = 0.0;
  double center_z = 0.0;
  double pass_x = 0.0;
  double pass_y = 0.0;
  double pass_z = 0.0;
  double gap_degrees = 0.0;
  int roi_width = 0;
  int roi_height = 0;
};

struct CxSetCircleBuildMeta
{
  double radius = 0.0;
  double circumference_hint = 0.0;
  std::size_t scan_count_hint = 0;
  bool compact_roi_hint = false;
};

struct CxSetCircleBuildResult
{
  CxShapeHandle curve_shape;
  CxShapeHandle measure_shape;
  CxSetCircleBuildMeta meta;
  bool success = false;
};

class CxSetCircleBuild
{
public:
  CxSetCircleBuildResult Build(const CxSetCircleRequest& request) const;
};
}
