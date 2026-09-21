#ifndef GEOMETRY_ACCEL_GEOMETRYPATHCONTRACT_H
#define GEOMETRY_ACCEL_GEOMETRYPATHCONTRACT_H

#include "GeometryPath.h"

#include <cstddef>
#include <string>
#include <vector>

namespace geometry_accel {

enum class GeometryPathCommandKind
{
  AddPoint,
  AddLine,
  AddArc,
  AddCircle,
  AddCross,
  AddMCircle,
  AddSquare,
  AddTriangle,
  AddRect,
  AddRect2,
  AddPathFromSnapshot,
  CopyPathFromSnapshot,
  SubtractPathFromSnapshot,
  Translate,
  RotateAroundPoint,
  RotateAroundLine,
  ScaleAroundPoint,
  SetColor,
  Clear,
  SortByBoundingCenterAngle,
  MakeShape,
  MakeEdgeShape,
  MakePointShape,
  PathShow
};

struct GeometryPathCommand
{
  GeometryPathCommandKind kind = GeometryPathCommandKind::AddPoint;
  std::vector<Vec3> points;
  Rectangle rect;
  Vec3 vector_a;
  Vec3 vector_b;
  Vec3 vector_c;
  double scalar_a = 0.0;
  double scalar_b = 0.0;
  double scalar_c = 0.0;
  int int_a = 0;
  int int_b = 0;
  int int_c = 0;
  bool flag = false;
};

struct GeometryPathSnapshot
{
  std::size_t point_count = 0;
  std::vector<Vec3> points;
  Rectangle bounds;
  Vec3 centroid;
  double total_length = 0.0;
  ColorRgb color;
  bool visible = true;
  std::string shape_ref;
  std::vector<std::string> shape_refs;
};

GeometryPathSnapshot CaptureGeometryPathSnapshot(const GeometryPath& path);

} // namespace geometry_accel

#endif
