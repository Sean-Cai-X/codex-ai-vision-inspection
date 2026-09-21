#include "CxSetCircleBuild.h"

#include "CxCurveBuilder.h"

#include <algorithm>
#include <cmath>

namespace cxgeom
{
namespace
{
constexpr double kPi = 3.14159265358979323846;
}

CxSetCircleBuildResult CxSetCircleBuild::Build(const CxSetCircleRequest& request) const
{
  CxSetCircleBuildResult result;

  const double dx = request.pass_x - request.center_x;
  const double dy = request.pass_y - request.center_y;
  const double dz = request.pass_z - request.center_z;
  const double radius = std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
  if (!(radius > 0.0))
  {
    return result;
  }

  CxCurveBuilder builder;
  result.curve_shape = builder.BuildCircle(request.entity_id,
                                           request.curve_name.c_str(),
                                           request.center_x,
                                           request.center_y,
                                           request.center_z,
                                           radius);
  result.measure_shape = result.curve_shape;

  result.meta.radius = radius;
  result.meta.circumference_hint = 2.0 * kPi * radius;
  if (request.gap_degrees > 0.0)
  {
    result.meta.scan_count_hint = static_cast<std::size_t>(
      std::max(1.0, std::ceil(360.0 / request.gap_degrees)));
  }
  result.meta.compact_roi_hint =
    request.roi_width > 0 &&
    request.roi_height > 0 &&
    request.roi_width <= 64 &&
    request.roi_height <= 64 &&
    radius <= 20.0;

  result.success = !result.curve_shape.IsNull();
  return result;
}
}
