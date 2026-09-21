#include "geometry_accel/GeometryPath.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace geometry_accel {
namespace {

constexpr double kTolerance = 1e-9;
constexpr double kPi = 3.14159265358979323846;

Vec3 RotateVectorAroundAxis(const Vec3& value, const Vec3& axis, double angle_radians)
{
  const Vec3 unit_axis = Normalize(axis);
  const double cos_angle = std::cos(angle_radians);
  const double sin_angle = std::sin(angle_radians);
  return value * cos_angle +
    Cross(unit_axis, value) * sin_angle +
    unit_axis * (Dot(unit_axis, value) * (1.0 - cos_angle));
}

double Cross2D(const Vec3& lhs, const Vec3& rhs)
{
  return lhs.x * rhs.y - lhs.y * rhs.x;
}

bool SegmentIntersection2D(const Vec3& p0,
                           const Vec3& p1,
                           const Vec3& q0,
                           const Vec3& q1,
                           Vec3* intersection)
{
  const Vec3 r = p1 - p0;
  const Vec3 s = q1 - q0;
  const double denom = Cross2D(r, s);
  const Vec3 qp = q0 - p0;

  if (std::fabs(denom) < kTolerance) {
    return false;
  }

  const double t = Cross2D(qp, s) / denom;
  const double u = Cross2D(qp, r) / denom;
  if (t < -kTolerance || t > 1.0 + kTolerance || u < -kTolerance || u > 1.0 + kTolerance) {
    return false;
  }

  if (intersection != nullptr) {
    *intersection = p0 + r * t;
  }
  return true;
}

} // namespace

void GeometryPath::AddPoint(const Vec3& point)
{
  if (!points_.empty() && Distance(points_.back(), point) < 1e-6) {
    return;
  }
  points_.push_back(point);
  InvalidateLengthCache();
}

Vec3 GeometryPath::PointAtPercent(double percent) const
{
  if (percent < 0.0 || percent > 1.0) {
    throw std::out_of_range("Percent must be between 0 and 1.");
  }
  if (points_.size() < 2) {
    throw std::runtime_error("Not enough points in the path.");
  }
  if (percent == 0.0) {
    return points_.front();
  }
  if (percent == 1.0) {
    return points_.back();
  }

  const double total_length = CalculateTotalLength();
  const double target_length = total_length * percent;
  double accumulated_length = 0.0;

  for (std::size_t i = 0; i + 1 < points_.size(); ++i) {
    const Vec3 segment = points_[i + 1] - points_[i];
    const double segment_length = Length(segment);
    if (accumulated_length + segment_length >= target_length) {
      const double local_percent = (target_length - accumulated_length) / segment_length;
      return points_[i] + segment * local_percent;
    }
    accumulated_length += segment_length;
  }

  return points_.back();
}

Vec3 GeometryPath::ElementAt(std::size_t index) const
{
  if (index >= points_.size()) {
    throw std::out_of_range("Index out of range.");
  }
  return points_[index];
}

std::size_t GeometryPath::ElementCount() const
{
  return points_.size();
}

void GeometryPath::RotateAroundPoint(const Vec3& rotation_center,
                                     double angle_degrees,
                                     const Vec3& rotation_axis)
{
  const double angle_radians = angle_degrees * kPi / 180.0;
  for (Vec3& point : points_) {
    const Vec3 shifted = point - rotation_center;
    point = rotation_center + RotateVectorAroundAxis(shifted, rotation_axis, angle_radians);
  }
  InvalidateLengthCache();
}

void GeometryPath::ScaleAroundPoint(const Vec3& scale_center,
                                    double scale_factor_x,
                                    double scale_factor_y,
                                    double scale_factor_z)
{
  for (Vec3& point : points_) {
    const Vec3 delta = point - scale_center;
    point = {
      scale_center.x + delta.x * scale_factor_x,
      scale_center.y + delta.y * scale_factor_y,
      scale_center.z + delta.z * scale_factor_z
    };
  }
  InvalidateLengthCache();
}

void GeometryPath::RotateAroundLine(const Vec3& line_point,
                                    const Vec3& line_direction,
                                    double angle_degrees)
{
  RotateAroundPoint(line_point, angle_degrees, line_direction);
}

void GeometryPath::Translate(const Vec3& translation)
{
  for (Vec3& point : points_) {
    point = point + translation;
  }
  if (render_adapter_ != nullptr && !shape_ref_.empty()) {
    render_adapter_->TranslateShape(shape_ref_, translation);
  }
  InvalidateLengthCache();
}

void GeometryPath::AddLine(const Vec3& start, const Vec3& end, int segments)
{
  const int count = std::max(1, segments);
  for (int index = 0; index <= count; ++index) {
    const double t = static_cast<double>(index) / static_cast<double>(count);
    AddPoint(start + (end - start) * t);
  }
}

void GeometryPath::AddArc(const Vec3& center,
                          double radius,
                          double start_angle_radians,
                          double end_angle_radians,
                          int segments)
{
  const int count = std::max(4, segments);
  const double delta = (end_angle_radians - start_angle_radians) / static_cast<double>(count);
  for (int index = 0; index <= count; ++index) {
    const double angle = start_angle_radians + delta * static_cast<double>(index);
    AddPoint({
      center.x + radius * std::cos(angle),
      center.y + radius * std::sin(angle),
      center.z
    });
  }
}

void GeometryPath::AddCircle(const Vec3& center, double radius, int segments)
{
  AddArc(center, radius, 0.0, 2.0 * kPi, std::max(8, segments));
}

void GeometryPath::AddRectangularEllipse(const Vec3& p1, const Vec3& p2, int segments)
{
  const Rectangle bounds(p1, p2);
  const Vec3 center = bounds.Center();
  const double radius_x = std::max(bounds.Width() / 2.0, kTolerance);
  const double radius_y = std::max(bounds.Height() / 2.0, kTolerance);
  const int count = std::max(16, segments);
  for (int index = 0; index <= count; ++index) {
    const double angle = (2.0 * kPi * static_cast<double>(index)) / static_cast<double>(count);
    AddPoint({
      center.x + radius_x * std::cos(angle),
      center.y + radius_y * std::sin(angle),
      center.z
    });
  }
}

void GeometryPath::AddRectangularEllipse5p(const Vec3* ellipse_points, int segments)
{
  if (ellipse_points == nullptr) {
    throw std::invalid_argument("ellipse_points must not be null");
  }

  double min_x = std::numeric_limits<double>::max();
  double min_y = std::numeric_limits<double>::max();
  double max_x = std::numeric_limits<double>::lowest();
  double max_y = std::numeric_limits<double>::lowest();
  for (int index = 0; index < 5; ++index) {
    min_x = std::min(min_x, ellipse_points[index].x);
    min_y = std::min(min_y, ellipse_points[index].y);
    max_x = std::max(max_x, ellipse_points[index].x);
    max_y = std::max(max_y, ellipse_points[index].y);
  }
  AddRectangularEllipse({ min_x, min_y, ellipse_points[0].z },
                        { max_x, max_y, ellipse_points[0].z },
                        segments);
}

void GeometryPath::AddCross(const Vec3& center, double size)
{
  const double half = size / 2.0;
  AddLine({ center.x - half, center.y - half, center.z },
          { center.x + half, center.y + half, center.z },
          2);
  AddLine({ center.x + half, center.y - half, center.z },
          { center.x - half, center.y + half, center.z },
          2);
}

void GeometryPath::AddMCircle(const Vec3& center, double radius, int segments)
{
  AddCircle(center, radius, segments);
}

void GeometryPath::AddSquare(const Vec3& center, double size)
{
  AddRect(Rectangle(center, size, size));
}

void GeometryPath::AddTriangle(const Vec3& center, double size)
{
  const double half = size / 2.0;
  const double height = std::sqrt(3.0) * half;
  AddRect2(
    { center.x, center.y + (2.0 * height / 3.0), center.z },
    { center.x - half, center.y - (height / 3.0), center.z },
    { center.x + half, center.y - (height / 3.0), center.z },
    { center.x, center.y + (2.0 * height / 3.0), center.z });
}

void GeometryPath::AddRect(const Rectangle& rect)
{
  AddRect2(
    { rect.min_corner.x, rect.min_corner.y, rect.min_corner.z },
    { rect.max_corner.x, rect.min_corner.y, rect.min_corner.z },
    { rect.max_corner.x, rect.max_corner.y, rect.max_corner.z },
    { rect.min_corner.x, rect.max_corner.y, rect.max_corner.z });
}

void GeometryPath::AddRect2(const Vec3& p1, const Vec3& p2, const Vec3& p3, const Vec3& p4)
{
  AddPoint(p1);
  AddPoint(p2);
  AddPoint(p3);
  AddPoint(p4);
}

void GeometryPath::AddPath(const GeometryPath& other_path)
{
  for (const Vec3& point : other_path.points_) {
    AddPoint(point);
  }
}

void GeometryPath::CopyPath(const GeometryPath& other_path)
{
  points_ = other_path.points_;
  color_ = other_path.color_;
  visible_ = other_path.visible_;
  cached_total_length_ = other_path.cached_total_length_;
}

void GeometryPath::Clear()
{
  points_.clear();
  cached_total_length_ = -1.0;
  if (render_adapter_ != nullptr) {
    if (!shape_ref_.empty()) {
      render_adapter_->EraseShape(shape_ref_);
    }
    for (const std::string& shape_ref : shape_refs_) {
      render_adapter_->EraseShape(shape_ref);
    }
  }
  shape_ref_.clear();
  shape_refs_.clear();
}

void GeometryPath::SubtractPath(const GeometryPath& other_path, double tolerance)
{
  std::vector<Vec3> filtered;
  filtered.reserve(points_.size());
  for (const Vec3& point : points_) {
    bool matched = false;
    for (const Vec3& other_point : other_path.points_) {
      if (Distance(point, other_point) < tolerance) {
        matched = true;
        break;
      }
    }
    if (!matched) {
      filtered.push_back(point);
    }
  }
  points_ = std::move(filtered);
  InvalidateLengthCache();
}

std::vector<Vec3> GeometryPath::IntersectPaths(const GeometryPath& other_path) const
{
  std::vector<Vec3> intersections;
  if (points_.size() < 2 || other_path.points_.size() < 2) {
    return intersections;
  }

  for (std::size_t left = 0; left + 1 < points_.size(); ++left) {
    for (std::size_t right = 0; right + 1 < other_path.points_.size(); ++right) {
      Vec3 intersection;
      if (SegmentIntersection2D(points_[left],
                                points_[left + 1],
                                other_path.points_[right],
                                other_path.points_[right + 1],
                                &intersection)) {
        bool duplicate = false;
        for (const Vec3& existing : intersections) {
          if (Distance(existing, intersection) < 1e-6) {
            duplicate = true;
            break;
          }
        }
        if (!duplicate) {
          intersections.push_back(intersection);
        }
      }
    }
  }
  return intersections;
}

void GeometryPath::FindBestMatch(const GeometryPath& other_path,
                                 Vec3& best_translation,
                                 double& best_rotation_degrees) const
{
  if (points_.empty() || other_path.points_.empty()) {
    best_translation = {};
    best_rotation_degrees = 0.0;
    return;
  }

  double min_error = std::numeric_limits<double>::max();
  for (double angle = 0.0; angle < 360.0; angle += 1.0) {
    for (const Vec3& point : points_) {
      const Vec3 translation = point;
      const double error = CalculateError(other_path, angle, translation);
      if (error < min_error) {
        min_error = error;
        best_rotation_degrees = angle;
        best_translation = translation;
      }
    }
  }
}

Rectangle GeometryPath::BoundingRect() const
{
  if (points_.empty()) {
    return Rectangle({}, {});
  }

  double min_x = std::numeric_limits<double>::max();
  double min_y = std::numeric_limits<double>::max();
  double min_z = std::numeric_limits<double>::max();
  double max_x = std::numeric_limits<double>::lowest();
  double max_y = std::numeric_limits<double>::lowest();
  double max_z = std::numeric_limits<double>::lowest();
  for (const Vec3& point : points_) {
    min_x = std::min(min_x, point.x);
    min_y = std::min(min_y, point.y);
    min_z = std::min(min_z, point.z);
    max_x = std::max(max_x, point.x);
    max_y = std::max(max_y, point.y);
    max_z = std::max(max_z, point.z);
  }
  return Rectangle({ min_x, min_y, min_z }, { max_x, max_y, max_z });
}

Vec3 GeometryPath::Centroid() const
{
  if (points_.empty()) {
    return {};
  }

  Vec3 sum{};
  for (const Vec3& point : points_) {
    sum = sum + point;
  }
  return sum / static_cast<double>(points_.size());
}

Vec3 GeometryPath::WeightedCentroid(const std::vector<double>& weights) const
{
  if (weights.size() != points_.size() || points_.empty()) {
    return {};
  }

  Vec3 weighted_sum{};
  double total_weight = 0.0;
  for (std::size_t index = 0; index < points_.size(); ++index) {
    weighted_sum = weighted_sum + points_[index] * weights[index];
    total_weight += weights[index];
  }
  if (std::fabs(total_weight) < kTolerance) {
    return {};
  }
  return weighted_sum / total_weight;
}

Vec3 GeometryPath::OBBCenterAngleSort()
{
  if (points_.size() < 2) {
    return {};
  }

  const Vec3 center = BoundingRect().Center();
  std::sort(points_.begin(), points_.end(), [center](const Vec3& lhs, const Vec3& rhs) {
    const double lhs_angle = std::atan2(lhs.y - center.y, lhs.x - center.x);
    const double rhs_angle = std::atan2(rhs.y - center.y, rhs.x - center.x);
    if (std::fabs(lhs_angle - rhs_angle) < 1e-6) {
      return Distance(lhs, center) < Distance(rhs, center);
    }
    return lhs_angle < rhs_angle;
  });
  return center;
}

double GeometryPath::CalculateTotalLength() const
{
  if (cached_total_length_ >= 0.0) {
    return cached_total_length_;
  }

  cached_total_length_ = 0.0;
  for (std::size_t index = 0; index + 1 < points_.size(); ++index) {
    cached_total_length_ += Distance(points_[index], points_[index + 1]);
  }
  return cached_total_length_;
}

void GeometryPath::SetColor(int r, int g, int b)
{
  color_ = {
    std::clamp(static_cast<double>(r) / 255.0, 0.0, 1.0),
    std::clamp(static_cast<double>(g) / 255.0, 0.0, 1.0),
    std::clamp(static_cast<double>(b) / 255.0, 0.0, 1.0)
  };
}

void GeometryPath::SetRenderAdapter(const std::shared_ptr<GeometryPathRenderAdapter>& adapter)
{
  render_adapter_ = adapter;
}

std::shared_ptr<GeometryPathRenderAdapter> GeometryPath::GetRenderAdapter() const
{
  return render_adapter_;
}

void GeometryPath::MakeShape()
{
  shape_ref_ = PublishMainShape(false, true);
}

void GeometryPath::MakeEdgeShape()
{
  shape_ref_ = PublishMainShape(false, false);
}

void GeometryPath::MakePointShape()
{
  shape_ref_ = PublishMainShape(true, false);
}

void GeometryPath::MakeRectsShape()
{
  if (render_adapter_ == nullptr) {
    return;
  }
  for (const std::string& shape_ref : shape_refs_) {
    render_adapter_->EraseShape(shape_ref);
  }
  shape_refs_.clear();
  shape_refs_ = render_adapter_->PublishRectangles(BuildRectangleShapes());
}

void GeometryPath::PathShow(bool show)
{
  visible_ = show;
  if (render_adapter_ != nullptr && !shape_ref_.empty()) {
    render_adapter_->SetShapeVisible(shape_ref_, show);
  }
}

const std::string& GeometryPath::ShapeRef() const
{
  return shape_ref_;
}

const std::vector<std::string>& GeometryPath::ShapeRefs() const
{
  return shape_refs_;
}

ColorRgb GeometryPath::Color() const
{
  return color_;
}

bool GeometryPath::Visible() const
{
  return visible_;
}

std::vector<Vec3>& GeometryPath::MutablePoints()
{
  InvalidateLengthCache();
  return points_;
}

const std::vector<Vec3>& GeometryPath::Points() const
{
  return points_;
}

double GeometryPath::CalculateError(const GeometryPath& other_path,
                                    double angle_degrees,
                                    const Vec3& translation) const
{
  const double angle_radians = angle_degrees * kPi / 180.0;
  double error = 0.0;
  const std::size_t limit = std::min(points_.size(), other_path.points_.size());
  for (std::size_t index = 0; index < limit; ++index) {
    const Vec3 rotated = RotateVectorAroundAxis(points_[index], { 0.0, 0.0, 1.0 }, angle_radians);
    error += Distance(rotated + translation, other_path.points_[index]);
  }
  return error;
}

void GeometryPath::InvalidateLengthCache()
{
  cached_total_length_ = -1.0;
}

std::vector<PolylineShape> GeometryPath::BuildRectangleShapes() const
{
  std::vector<PolylineShape> shapes;
  for (std::size_t index = 0; index + 3 < points_.size(); index += 4) {
    PolylineShape shape;
    shape.points = {
      points_[index],
      points_[index + 1],
      points_[index + 2],
      points_[index + 3],
      points_[index]
    };
    shape.style.color = color_;
    shape.style.visible = visible_;
    shapes.push_back(shape);
  }
  return shapes;
}

std::string GeometryPath::PublishMainShape(bool as_point_cloud, bool close_loop)
{
  if (render_adapter_ == nullptr || points_.empty()) {
    return {};
  }

  PolylineShape shape;
  shape.points = points_;
  if (close_loop && points_.size() >= 2 && Distance(points_.front(), points_.back()) > 1e-6) {
    shape.points.push_back(points_.front());
  }
  shape.style.color = color_;
  shape.style.visible = visible_;
  return as_point_cloud
    ? render_adapter_->PublishPointCloud(shape)
    : render_adapter_->PublishPolyline(shape);
}

} // namespace geometry_accel
