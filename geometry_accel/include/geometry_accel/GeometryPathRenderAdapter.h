#ifndef GEOMETRY_ACCEL_GEOMETRYPATHRENDERADAPTER_H
#define GEOMETRY_ACCEL_GEOMETRYPATHRENDERADAPTER_H

#include "GeometryTypes.h"

#include <string>
#include <vector>

namespace geometry_accel {

class GeometryPathRenderAdapter
{
public:
  virtual ~GeometryPathRenderAdapter() = default;

  virtual std::string PublishPolyline(const PolylineShape& shape) = 0;
  virtual std::string PublishPointCloud(const PolylineShape& shape) = 0;
  virtual std::vector<std::string> PublishRectangles(
    const std::vector<PolylineShape>& rectangles) = 0;
  virtual void EraseShape(const std::string& shape_ref) = 0;
  virtual void SetShapeVisible(const std::string& shape_ref, bool visible) = 0;
  virtual void TranslateShape(const std::string& shape_ref, const Vec3& translation) = 0;
};

} // namespace geometry_accel

#endif
