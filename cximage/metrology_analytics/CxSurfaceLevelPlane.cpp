#include "pch.h"
#include "metrology_analytics/CxSurfaceLevelPlane.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace cxvision::metrology_analytics
{
namespace
{
bool Solve3x3(double a[3][4], CxPlaneCoeffs& out)
{
    for (int col = 0; col < 3; ++col)
    {
        int pivot = col;
        for (int row = col + 1; row < 3; ++row)
        {
            if (std::abs(a[row][col]) > std::abs(a[pivot][col]))
                pivot = row;
        }

        if (std::abs(a[pivot][col]) < 1e-18)
            return false;

        if (pivot != col)
        {
            for (int k = col; k < 4; ++k)
                std::swap(a[col][k], a[pivot][k]);
        }

        const double div = a[col][col];
        for (int k = col; k < 4; ++k)
            a[col][k] /= div;

        for (int row = 0; row < 3; ++row)
        {
            if (row == col)
                continue;
            const double factor = a[row][col];
            for (int k = col; k < 4; ++k)
                a[row][k] -= factor * a[col][k];
        }
    }

    out.a = a[0][3];
    out.b = a[1][3];
    out.c = a[2][3];
    return true;
}

void AccumulateNormalEquation(
    const CxSurfaceField& field,
    double huber_delta,
    const CxPlaneCoeffs* seed,
    double normal[3][4])
{
    for (int y = 0; y < field.yres(); ++y)
    {
        for (int x = 0; x < field.xres(); ++x)
        {
            const double px = static_cast<double>(x) * field.unit().x_scale_per_pixel;
            const double py = static_cast<double>(y) * field.unit().y_scale_per_pixel;
            const double z = field.at(x, y) * field.unit().z_scale_per_pixel;

            double w = 1.0;
            if (seed != nullptr && huber_delta > 0.0)
            {
                const double r = std::abs(z - (seed->a * px + seed->b * py + seed->c));
                w = (r <= huber_delta) ? 1.0 : (huber_delta / r);
            }

            const double v[3] = { px, py, 1.0 };
            for (int row = 0; row < 3; ++row)
            {
                for (int col = 0; col < 3; ++col)
                    normal[row][col] += w * v[row] * v[col];
                normal[row][3] += w * v[row] * z;
            }
        }
    }
}

CxPlaneCoeffs FitOls(const CxSurfaceField& field, const CxPlaneCoeffs* seed = nullptr, double huber_delta = 0.0)
{
    double normal[3][4] = {};
    AccumulateNormalEquation(field, huber_delta, seed, normal);
    CxPlaneCoeffs plane;
    if (!Solve3x3(normal, plane))
        throw std::runtime_error("fitPlane failed: singular normal equation.");
    return plane;
}
}

CxPlaneCoeffs fitPlane(const CxSurfaceField& field, PlaneLevelMethod method)
{
    if (field.valueCount() < 3)
        throw std::invalid_argument("fitPlane requires at least three samples.");

    if (method == PlaneLevelMethod::ThreePoints)
    {
        const double sx = field.unit().x_scale_per_pixel;
        const double sy = field.unit().y_scale_per_pixel;
        const double z00 = field.at(0, 0) * field.unit().z_scale_per_pixel;
        const double z10 = field.at(std::max(0, field.xres() - 1), 0) * field.unit().z_scale_per_pixel;
        const double z01 = field.at(0, std::max(0, field.yres() - 1)) * field.unit().z_scale_per_pixel;
        const double x1 = static_cast<double>(std::max(1, field.xres() - 1)) * sx;
        const double y1 = static_cast<double>(std::max(1, field.yres() - 1)) * sy;
        return { (z10 - z00) / x1, (z01 - z00) / y1, z00 };
    }

    CxPlaneCoeffs ols = FitOls(field);
    if (method == PlaneLevelMethod::WeightedTlsHuber)
        return FitOls(field, &ols, std::max(1e-9, ols.c == 0.0 ? 1.0 : std::abs(ols.c) * 0.01));
    return ols;
}

void subtractPlaneInPlace(CxSurfaceField& field, const CxPlaneCoeffs& plane)
{
    for (int y = 0; y < field.yres(); ++y)
    {
        for (int x = 0; x < field.xres(); ++x)
        {
            const double px = static_cast<double>(x) * field.unit().x_scale_per_pixel;
            const double py = static_cast<double>(y) * field.unit().y_scale_per_pixel;
            const double z = field.at(x, y) * field.unit().z_scale_per_pixel;
            field.setAt(x, y, (z - (plane.a * px + plane.b * py + plane.c)) / field.unit().z_scale_per_pixel);
        }
    }
}

} // namespace cxvision::metrology_analytics

