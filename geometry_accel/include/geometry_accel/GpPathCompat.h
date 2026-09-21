#ifndef GEOMETRY_ACCEL_GPPATHCOMPAT_H
#define GEOMETRY_ACCEL_GPPATHCOMPAT_H

#include "GeometryPath.h"

namespace geometry_accel {

class gp_PathCompat
{
public:
  gp_PathCompat() = default;

  void AddPoint(const Vec3& point) { path_.AddPoint(point); }
  Vec3 PointAtPercent(double percent) const { return path_.PointAtPercent(percent); }
  Vec3 ElementAt(std::size_t index) const { return path_.ElementAt(index); }
  std::size_t ElementCount() const { return path_.ElementCount(); }

  void RotateAroundPoint(const Vec3& rotation_center,
                         double angle_degrees,
                         const Vec3& rotation_axis = Vec3{ 0.0, 0.0, 1.0 })
  {
    path_.RotateAroundPoint(rotation_center, angle_degrees, rotation_axis);
  }

  void ScaleAroundPoint(const Vec3& scale_center,
                        double scale_factor_x,
                        double scale_factor_y,
                        double scale_factor_z = 1.0)
  {
    path_.ScaleAroundPoint(scale_center, scale_factor_x, scale_factor_y, scale_factor_z);
  }

  void RotateAroundLine(const Vec3& line_point,
                        const Vec3& line_direction,
                        double angle_degrees)
  {
    path_.RotateAroundLine(line_point, line_direction, angle_degrees);
  }

  void Translate(const Vec3& translation) { path_.Translate(translation); }

  void AddLine(const Vec3& start, const Vec3& end, int segments = 100)
  {
    path_.AddLine(start, end, segments);
  }

  void AddArc(const Vec3& center,
              double radius,
              double start_angle_radians,
              double end_angle_radians,
              int segments = 64)
  {
    path_.AddArc(center, radius, start_angle_radians, end_angle_radians, segments);
  }

  void AddCircle(const Vec3& center, double radius, int segments = 128)
  {
    path_.AddCircle(center, radius, segments);
  }

  void AddMCircle(const Vec3& center, double radius, int segments = 64)
  {
    path_.AddMCircle(center, radius, segments);
  }

  void AddRectangularEllipse(const Vec3& p1, const Vec3& p2, int segments = 128)
  {
    path_.AddRectangularEllipse(p1, p2, segments);
  }

  void AddRectangularEllipse5p(const Vec3* ellipse_points, int segments = 128)
  {
    path_.AddRectangularEllipse5p(ellipse_points, segments);
  }

  void AddCross(const Vec3& center, double size) { path_.AddCross(center, size); }
  void AddSquare(const Vec3& center, double size) { path_.AddSquare(center, size); }
  void AddTriangle(const Vec3& center, double size) { path_.AddTriangle(center, size); }
  void AddRect(const Rectangle& rect) { path_.AddRect(rect); }
  void AddRect2(const Vec3& p1, const Vec3& p2, const Vec3& p3, const Vec3& p4)
  {
    path_.AddRect2(p1, p2, p3, p4);
  }

  void AddPath(const gp_PathCompat& other_path) { path_.AddPath(other_path.path_); }
  void CopyPath(const gp_PathCompat& other_path) { path_.CopyPath(other_path.path_); }
  void Clear() { path_.Clear(); }
  void SubtractPath(const gp_PathCompat& other_path, double tolerance = 1e-6)
  {
    path_.SubtractPath(other_path.path_, tolerance);
  }

  std::vector<Vec3> IntersectPaths(const gp_PathCompat& other_path) const
  {
    return path_.IntersectPaths(other_path.path_);
  }

  void FindBestMatch(const gp_PathCompat& other_path,
                     Vec3& best_translation,
                     double& best_rotation_degrees) const
  {
    path_.FindBestMatch(other_path.path_, best_translation, best_rotation_degrees);
  }

  Rectangle boundingRect() const { return path_.BoundingRect(); }
  Vec3 centroid() const { return path_.Centroid(); }
  Vec3 weightedCentroid(const std::vector<double>& weights) const
  {
    return path_.WeightedCentroid(weights);
  }

  Vec3 OBBCenterAngleSort() { return path_.OBBCenterAngleSort(); }
  double CalculateTotalLength() const { return path_.CalculateTotalLength(); }

  void SetRenderAdapter(const std::shared_ptr<GeometryPathRenderAdapter>& adapter)
  {
    path_.SetRenderAdapter(adapter);
  }

  std::shared_ptr<GeometryPathRenderAdapter> GetRenderAdapter() const
  {
    return path_.GetRenderAdapter();
  }

  void setcolor(int r, int g, int b) { path_.SetColor(r, g, b); }
  void MakeShape() { path_.MakeShape(); }
  void MakeEdgeShape() { path_.MakeEdgeShape(); }
  void MakePointShape() { path_.MakePointShape(); }
  void MakeRectsShape() { path_.MakeRectsShape(); }
  void PathShow(bool show) { path_.PathShow(show); }

  const std::string& ShapeRef() const { return path_.ShapeRef(); }
  const std::vector<std::string>& ShapeRefs() const { return path_.ShapeRefs(); }

  std::vector<Vec3>& getpoints() { return path_.MutablePoints(); }
  const std::vector<Vec3>& getpoints() const { return path_.Points(); }

  GeometryPath& impl() { return path_; }
  const GeometryPath& impl() const { return path_; }

private:
  GeometryPath path_;
};

using gp_PathNeutralCompat = gp_PathCompat;

} // namespace geometry_accel

#endif
