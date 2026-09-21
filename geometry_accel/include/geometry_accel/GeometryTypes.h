#ifndef GEOMETRY_ACCEL_GEOMETRYTYPES_H
#define GEOMETRY_ACCEL_GEOMETRYTYPES_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace geometry_accel {

struct Vec3
{
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
};

inline Vec3 operator+(const Vec3& lhs, const Vec3& rhs)
{
  return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
}

inline Vec3 operator-(const Vec3& lhs, const Vec3& rhs)
{
  return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
}

inline Vec3 operator*(const Vec3& value, double scale)
{
  return { value.x * scale, value.y * scale, value.z * scale };
}

inline Vec3 operator/(const Vec3& value, double scale)
{
  return { value.x / scale, value.y / scale, value.z / scale };
}

inline double Dot(const Vec3& lhs, const Vec3& rhs)
{
  return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

inline Vec3 Cross(const Vec3& lhs, const Vec3& rhs)
{
  return {
    lhs.y * rhs.z - lhs.z * rhs.y,
    lhs.z * rhs.x - lhs.x * rhs.z,
    lhs.x * rhs.y - lhs.y * rhs.x
  };
}

inline double Length(const Vec3& value)
{
  return std::sqrt(Dot(value, value));
}

inline Vec3 Normalize(const Vec3& value)
{
  const double length = Length(value);
  if (length <= 1e-12) {
    return { 0.0, 0.0, 1.0 };
  }
  return value / length;
}

inline double Distance(const Vec3& lhs, const Vec3& rhs)
{
  return Length(lhs - rhs);
}

struct ColorRgb
{
  double r = 0.0;
  double g = 1.0;
  double b = 0.0;
};

struct LineSegment
{
  Vec3 start;
  Vec3 end;

  double Length() const
  {
    return geometry_accel::Length(end - start);
  }
};

struct Rectangle
{
  Vec3 min_corner;
  Vec3 max_corner;

  Rectangle() = default;

  Rectangle(const Vec3& top_left, const Vec3& bottom_right)
  {
    min_corner = {
      std::min(top_left.x, bottom_right.x),
      std::min(top_left.y, bottom_right.y),
      std::min(top_left.z, bottom_right.z)
    };
    max_corner = {
      std::max(top_left.x, bottom_right.x),
      std::max(top_left.y, bottom_right.y),
      std::max(top_left.z, bottom_right.z)
    };
  }

  Rectangle(const Vec3& center, double width, double height)
  {
    const double half_width = width / 2.0;
    const double half_height = height / 2.0;
    min_corner = { center.x - half_width, center.y - half_height, center.z };
    max_corner = { center.x + half_width, center.y + half_height, center.z };
  }

  double Width() const { return max_corner.x - min_corner.x; }
  double Height() const { return max_corner.y - min_corner.y; }
  Vec3 Center() const
  {
    return {
      (min_corner.x + max_corner.x) / 2.0,
      (min_corner.y + max_corner.y) / 2.0,
      (min_corner.z + max_corner.z) / 2.0
    };
  }

  bool Contains(const Vec3& point) const
  {
    return point.x >= min_corner.x && point.x <= max_corner.x &&
      point.y >= min_corner.y && point.y <= max_corner.y &&
      point.z >= min_corner.z && point.z <= max_corner.z;
  }

  void Translate(double dx, double dy, double dz = 0.0)
  {
    min_corner = min_corner + Vec3{ dx, dy, dz };
    max_corner = max_corner + Vec3{ dx, dy, dz };
  }
};

struct DisplayStyle
{
  ColorRgb color;
  bool visible = true;
};

struct PolylineShape
{
  std::string shape_ref;
  std::vector<Vec3> points;
  DisplayStyle style;
};

} // namespace geometry_accel

#endif
