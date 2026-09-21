#include "geometry_accel/GpPathCompatBridge.h"

#include <exception>

namespace geometry_accel {
namespace {

bool RequirePointCount(const GeometryPathCommand& command,
                       std::size_t expected_count,
                       std::string* error_message)
{
  if (command.points.size() == expected_count) {
    return true;
  }
  if (error_message != nullptr) {
    *error_message = "unexpected point count";
  }
  return false;
}

} // namespace

bool GpPathCompatBridge::Execute(const GeometryPathCommand& command,
                                 std::string* error_message)
{
  try {
    switch (command.kind) {
    case GeometryPathCommandKind::AddPoint:
      if (!RequirePointCount(command, 1, error_message)) {
        return false;
      }
      path_.AddPoint(command.points.front());
      return true;
    case GeometryPathCommandKind::AddLine:
      if (!RequirePointCount(command, 2, error_message)) {
        return false;
      }
      path_.AddLine(command.points[0], command.points[1], command.int_a > 0 ? command.int_a : 100);
      return true;
    case GeometryPathCommandKind::AddArc:
      if (!RequirePointCount(command, 1, error_message)) {
        return false;
      }
      path_.AddArc(command.points[0],
                   command.scalar_a,
                   command.scalar_b,
                   command.scalar_c,
                   command.int_a > 0 ? command.int_a : 64);
      return true;
    case GeometryPathCommandKind::AddCircle:
      if (!RequirePointCount(command, 1, error_message)) {
        return false;
      }
      path_.AddCircle(command.points[0], command.scalar_a, command.int_a > 0 ? command.int_a : 128);
      return true;
    case GeometryPathCommandKind::AddCross:
      if (!RequirePointCount(command, 1, error_message)) {
        return false;
      }
      path_.AddCross(command.points[0], command.scalar_a);
      return true;
    case GeometryPathCommandKind::AddMCircle:
      if (!RequirePointCount(command, 1, error_message)) {
        return false;
      }
      path_.AddMCircle(command.points[0], command.scalar_a, command.int_a > 0 ? command.int_a : 64);
      return true;
    case GeometryPathCommandKind::AddSquare:
      if (!RequirePointCount(command, 1, error_message)) {
        return false;
      }
      path_.AddSquare(command.points[0], command.scalar_a);
      return true;
    case GeometryPathCommandKind::AddTriangle:
      if (!RequirePointCount(command, 1, error_message)) {
        return false;
      }
      path_.AddTriangle(command.points[0], command.scalar_a);
      return true;
    case GeometryPathCommandKind::AddRect:
      path_.AddRect(command.rect);
      return true;
    case GeometryPathCommandKind::AddRect2:
      if (!RequirePointCount(command, 4, error_message)) {
        return false;
      }
      path_.AddRect2(command.points[0], command.points[1], command.points[2], command.points[3]);
      return true;
    case GeometryPathCommandKind::AddPathFromSnapshot: {
      gp_PathCompat other_path;
      other_path.getpoints() = command.points;
      path_.AddPath(other_path);
      return true;
    }
    case GeometryPathCommandKind::CopyPathFromSnapshot: {
      gp_PathCompat other_path;
      other_path.getpoints() = command.points;
      path_.CopyPath(other_path);
      return true;
    }
    case GeometryPathCommandKind::SubtractPathFromSnapshot: {
      gp_PathCompat other_path;
      other_path.getpoints() = command.points;
      path_.SubtractPath(other_path, command.scalar_a > 0.0 ? command.scalar_a : 1e-6);
      return true;
    }
    case GeometryPathCommandKind::Translate:
      path_.Translate(command.vector_a);
      return true;
    case GeometryPathCommandKind::RotateAroundPoint:
      path_.RotateAroundPoint(command.vector_a, command.scalar_a, command.vector_b);
      return true;
    case GeometryPathCommandKind::RotateAroundLine:
      path_.RotateAroundLine(command.vector_a, command.vector_b, command.scalar_a);
      return true;
    case GeometryPathCommandKind::ScaleAroundPoint:
      path_.ScaleAroundPoint(command.vector_a, command.scalar_a, command.scalar_b, command.scalar_c);
      return true;
    case GeometryPathCommandKind::SetColor:
      path_.setcolor(command.int_a, command.int_b, command.int_c);
      return true;
    case GeometryPathCommandKind::Clear:
      path_.Clear();
      return true;
    case GeometryPathCommandKind::SortByBoundingCenterAngle:
      path_.OBBCenterAngleSort();
      return true;
    case GeometryPathCommandKind::MakeShape:
      path_.MakeShape();
      return true;
    case GeometryPathCommandKind::MakeEdgeShape:
      path_.MakeEdgeShape();
      return true;
    case GeometryPathCommandKind::MakePointShape:
      path_.MakePointShape();
      return true;
    case GeometryPathCommandKind::PathShow:
      path_.PathShow(command.flag);
      return true;
    }
  } catch (const std::exception& ex) {
    if (error_message != nullptr) {
      *error_message = ex.what();
    }
    return false;
  }

  if (error_message != nullptr) {
    *error_message = "unsupported command";
  }
  return false;
}

GeometryPathSnapshot GpPathCompatBridge::Snapshot() const
{
  return CaptureGeometryPathSnapshot(path_.impl());
}

} // namespace geometry_accel
