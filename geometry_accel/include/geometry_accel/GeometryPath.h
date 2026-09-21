#ifndef GEOMETRY_ACCEL_GEOMETRYPATH_H
#define GEOMETRY_ACCEL_GEOMETRYPATH_H

#include "GeometryPathRenderAdapter.h"
#include "GeometryTypes.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace geometry_accel {

class GeometryPath
{
public:
  GeometryPath() = default;

  void AddPoint(const Vec3& point);
  Vec3 PointAtPercent(double percent) const;
  Vec3 ElementAt(std::size_t index) const;
  std::size_t ElementCount() const;

  void RotateAroundPoint(const Vec3& rotation_center,
                         double angle_degrees,
                         const Vec3& rotation_axis = Vec3{ 0.0, 0.0, 1.0 });
  void ScaleAroundPoint(const Vec3& scale_center,
                        double scale_factor_x,
                        double scale_factor_y,
                        double scale_factor_z = 1.0);
  void RotateAroundLine(const Vec3& line_point,
                        const Vec3& line_direction,
                        double angle_degrees);
  void Translate(const Vec3& translation);

  void AddLine(const Vec3& start, const Vec3& end, int segments = 100);
  void AddArc(const Vec3& center,
              double radius,
              double start_angle_radians,
              double end_angle_radians,
              int segments = 64);
  void AddCircle(const Vec3& center, double radius, int segments = 128);
  void AddRectangularEllipse(const Vec3& p1, const Vec3& p2, int segments = 128);
  void AddRectangularEllipse5p(const Vec3* ellipse_points, int segments = 128);
  void AddCross(const Vec3& center, double size);
  void AddMCircle(const Vec3& center, double radius, int segments = 64);
  void AddSquare(const Vec3& center, double size);
  void AddTriangle(const Vec3& center, double size);
  void AddRect(const Rectangle& rect);
  void AddRect2(const Vec3& p1, const Vec3& p2, const Vec3& p3, const Vec3& p4);

  void AddPath(const GeometryPath& other_path);
  void CopyPath(const GeometryPath& other_path);
  void Clear();
  void SubtractPath(const GeometryPath& other_path, double tolerance = 1e-6);
  std::vector<Vec3> IntersectPaths(const GeometryPath& other_path) const;
  void FindBestMatch(const GeometryPath& other_path,
                     Vec3& best_translation,
                     double& best_rotation_degrees) const;

  Rectangle BoundingRect() const;
  Vec3 Centroid() const;
  Vec3 WeightedCentroid(const std::vector<double>& weights) const;
  Vec3 OBBCenterAngleSort();
  double CalculateTotalLength() const;

  void SetColor(int r, int g, int b);
  void SetRenderAdapter(const std::shared_ptr<GeometryPathRenderAdapter>& adapter);
  std::shared_ptr<GeometryPathRenderAdapter> GetRenderAdapter() const;

  void MakeShape();
  void MakeEdgeShape();
  void MakePointShape();
  void MakeRectsShape();
  void PathShow(bool show);

  const std::string& ShapeRef() const;
  const std::vector<std::string>& ShapeRefs() const;
  ColorRgb Color() const;
  bool Visible() const;
  std::vector<Vec3>& MutablePoints();
  const std::vector<Vec3>& Points() const;

private:
  double CalculateError(const GeometryPath& other_path,
                        double angle_degrees,
                        const Vec3& translation) const;
  void InvalidateLengthCache();
  std::vector<PolylineShape> BuildRectangleShapes() const;
  std::string PublishMainShape(bool as_point_cloud, bool close_loop);

  mutable double cached_total_length_ = -1.0;
  std::vector<Vec3> points_;
  ColorRgb color_;
  bool visible_ = true;
  std::shared_ptr<GeometryPathRenderAdapter> render_adapter_;
  std::string shape_ref_;
  std::vector<std::string> shape_refs_;
};

} // namespace geometry_accel

#endif
