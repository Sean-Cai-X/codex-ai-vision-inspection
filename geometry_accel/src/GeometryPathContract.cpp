#include "geometry_accel/GeometryPathContract.h"

namespace geometry_accel {

GeometryPathSnapshot CaptureGeometryPathSnapshot(const GeometryPath& path)
{
  GeometryPathSnapshot snapshot;
  snapshot.point_count = path.ElementCount();
  snapshot.points = path.Points();
  snapshot.bounds = path.BoundingRect();
  snapshot.centroid = path.Centroid();
  snapshot.total_length = path.CalculateTotalLength();
  snapshot.color = path.Color();
  snapshot.visible = path.Visible();
  snapshot.shape_ref = path.ShapeRef();
  snapshot.shape_refs = path.ShapeRefs();
  return snapshot;
}

} // namespace geometry_accel
