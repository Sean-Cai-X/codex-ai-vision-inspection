#include "pch.h"
#include "metrology_analytics/CxSurfaceAreas.h"

#include <array>
#include <cmath>

namespace cxvision::metrology_analytics
{
namespace
{
struct Vec3
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

double TriangleArea(const Vec3& a, const Vec3& b, const Vec3& c)
{
    const Vec3 u{ b.x - a.x, b.y - a.y, b.z - a.z };
    const Vec3 v{ c.x - a.x, c.y - a.y, c.z - a.z };
    const Vec3 cross{
        u.y * v.z - u.z * v.y,
        u.z * v.x - u.x * v.z,
        u.x * v.y - u.y * v.x
    };
    return 0.5 * std::sqrt(cross.x * cross.x + cross.y * cross.y + cross.z * cross.z);
}

double CornerHeight(const CxSurfaceField& f, int corner_x, int corner_y)
{
    double sum = 0.0;
    int count = 0;
    for (int dy = -1; dy <= 0; ++dy)
    {
        for (int dx = -1; dx <= 0; ++dx)
        {
            const int px = corner_x + dx;
            const int py = corner_y + dy;
            if (px >= 0 && py >= 0 && px < f.xres() && py < f.yres())
            {
                sum += f.at(px, py);
                ++count;
            }
        }
    }
    return count > 0 ? sum / static_cast<double>(count) : 0.0;
}
}

CxSurfaceAreaResult computeProjectedAndSurfaceArea(const CxSurfaceField& field)
{
    CxSurfaceAreaResult result;
    result.projected_area = field.projectedPhysicalArea();

    if (field.xres() <= 0 || field.yres() <= 0)
        return result;

    const double sx = field.unit().x_scale_per_pixel;
    const double sy = field.unit().y_scale_per_pixel;
    const double sz = field.unit().z_scale_per_pixel;

    double surface = 0.0;
    for (int y = 0; y < field.yres(); ++y)
    {
        for (int x = 0; x < field.xres(); ++x)
        {
            const Vec3 center{
                (static_cast<double>(x) + 0.5) * sx,
                (static_cast<double>(y) + 0.5) * sy,
                field.at(x, y) * sz
            };

            const std::array<Vec3, 4> corners = {
                Vec3{ static_cast<double>(x) * sx, static_cast<double>(y) * sy, CornerHeight(field, x, y) * sz },
                Vec3{ static_cast<double>(x + 1) * sx, static_cast<double>(y) * sy, CornerHeight(field, x + 1, y) * sz },
                Vec3{ static_cast<double>(x + 1) * sx, static_cast<double>(y + 1) * sy, CornerHeight(field, x + 1, y + 1) * sz },
                Vec3{ static_cast<double>(x) * sx, static_cast<double>(y + 1) * sy, CornerHeight(field, x, y + 1) * sz }
            };

            surface += TriangleArea(center, corners[0], corners[1]);
            surface += TriangleArea(center, corners[1], corners[2]);
            surface += TriangleArea(center, corners[2], corners[3]);
            surface += TriangleArea(center, corners[3], corners[0]);
        }
    }

    result.surface_area = surface;
    result.area_ratio = result.projected_area > 0.0 ? result.surface_area / result.projected_area : 0.0;
    return result;
}

} // namespace cxvision::metrology_analytics

