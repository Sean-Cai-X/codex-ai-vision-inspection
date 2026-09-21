#include "CxSetLineBuild.h"

#include "CxCurveBuilder.h"

#include <algorithm>
#include <cmath>

namespace cxgeom
{
CxSetLineBuildResult CxSetLineBuild::Build(const CxSetLineRequest& request) const
{
  CxSetLineBuildResult result;

  const double dx = request.x1 - request.x0;
  const double dy = request.y1 - request.y0;
  const double dz = request.z1 - request.z0;
  const double length = std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
  if (!(length > 0.0))
  {
    return result;
  }

  CxCurveBuilder builder;
  result.line_shape = builder.BuildLine(request.entity_id,
                                        request.line_name.c_str(),
                                        request.x0,
                                        request.y0,
                                        request.z0,
                                        request.x1,
                                        request.y1,
                                        request.z1);
  result.measure_shape = result.line_shape;

  result.meta.length_hint = length;
  if (request.gap > 0.0)
  {
    result.meta.scan_count_hint = static_cast<std::size_t>(
      std::max(1.0, std::ceil(length / request.gap)));
  }
  result.meta.compact_roi_hint =
    request.roi_width > 0 &&
    request.roi_height > 0 &&
    request.roi_width <= 64 &&
    request.roi_height <= 64 &&
    length <= 32.0;

  result.success = !result.line_shape.IsNull();
  return result;
}
}
