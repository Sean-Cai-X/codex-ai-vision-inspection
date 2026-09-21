#include "pch.h"
#include "metrology_analytics/CxSyntheticSurfaceFactory.h"

#include <cmath>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace cxvision::metrology_analytics
{
namespace
{
constexpr double kPi = 3.141592653589793238462643383279502884;
}

CxSurfaceField CxSyntheticSurfaceFactory::flat(int xres, int yres, double z0, const CxPhysUnit& unit)
{
    CxSurfaceField f(xres, yres, unit);
    f.fillFromGenerator([z0](int, int) { return z0; });
    return f;
}

CxSurfaceField CxSyntheticSurfaceFactory::plane(
    int xres, int yres, double ax, double ay, double c, const CxPhysUnit& unit)
{
    CxSurfaceField f(xres, yres, unit);
    f.fillFromGenerator([&](int x, int y)
    {
        return ax * (static_cast<double>(x) * unit.x_scale_per_pixel) +
               ay * (static_cast<double>(y) * unit.y_scale_per_pixel) +
               c;
    });
    return f;
}

CxSurfaceField CxSyntheticSurfaceFactory::sine1D(
    int xres, int yres, double amplitude, double lambda_px, int axis, const CxPhysUnit& unit)
{
    if (lambda_px <= 0.0)
        throw std::invalid_argument("sine1D requires positive lambda_px.");
    CxSurfaceField f(xres, yres, unit);
    f.fillFromGenerator([&](int x, int y)
    {
        const int p = (axis == 1) ? y : x;
        return amplitude * std::sin(2.0 * kPi * static_cast<double>(p) / lambda_px);
    });
    return f;
}

CxSurfaceField CxSyntheticSurfaceFactory::gaussianRandom(
    int xres, int yres, double mu, double sigma, std::uint64_t seed, const CxPhysUnit& unit)
{
    if (sigma < 0.0)
        throw std::invalid_argument("gaussianRandom requires non-negative sigma.");
    CxSurfaceField f(xres, yres, unit);
    std::mt19937_64 rng(seed);
    std::normal_distribution<double> dist(mu, sigma);
    f.fillFromGenerator([&](int, int) { return dist(rng); });
    return f;
}

CxSurfaceField CxSyntheticSurfaceFactory::bimodal(
    int xres, int yres, double w, double m1, double s1, double m2, double s2, std::uint64_t seed, const CxPhysUnit& unit)
{
    if (w < 0.0 || w > 1.0 || s1 < 0.0 || s2 < 0.0)
        throw std::invalid_argument("bimodal requires w in [0,1] and non-negative sigma values.");
    CxSurfaceField f(xres, yres, unit);
    std::mt19937_64 rng(seed);
    std::bernoulli_distribution choose_first(w);
    std::normal_distribution<double> a(m1, s1);
    std::normal_distribution<double> b(m2, s2);
    f.fillFromGenerator([&](int, int) { return choose_first(rng) ? a(rng) : b(rng); });
    return f;
}

CxSurfaceField CxSyntheticSurfaceFactory::perfectDiscs(
    int xres,
    int yres,
    int n,
    double radius_px,
    double height,
    double min_dist,
    std::uint64_t seed,
    const CxPhysUnit& unit)
{
    if (n < 0 || radius_px <= 0.0 || min_dist < 0.0)
        throw std::invalid_argument("perfectDiscs requires n>=0, radius_px>0 and min_dist>=0.");

    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<double> dx(radius_px, std::max(radius_px, static_cast<double>(xres) - radius_px));
    std::uniform_real_distribution<double> dy(radius_px, std::max(radius_px, static_cast<double>(yres) - radius_px));

    std::vector<std::pair<double, double>> centers;
    const int max_attempts = std::max(100, n * 100);
    for (int attempt = 0; attempt < max_attempts && static_cast<int>(centers.size()) < n; ++attempt)
    {
        const double cx = dx(rng);
        const double cy = dy(rng);
        bool ok = true;
        for (const auto& c : centers)
        {
            const double xdiff = cx - c.first;
            const double ydiff = cy - c.second;
            if (std::sqrt(xdiff * xdiff + ydiff * ydiff) < min_dist)
            {
                ok = false;
                break;
            }
        }
        if (ok)
            centers.push_back({ cx, cy });
    }

    CxSurfaceField f(xres, yres, unit);
    f.fillFromGenerator([&](int x, int y)
    {
        const double px = static_cast<double>(x) + 0.5;
        const double py = static_cast<double>(y) + 0.5;
        for (const auto& c : centers)
        {
            const double xdiff = px - c.first;
            const double ydiff = py - c.second;
            if (xdiff * xdiff + ydiff * ydiff <= radius_px * radius_px)
                return height;
        }
        return 0.0;
    });
    return f;
}

} // namespace cxvision::metrology_analytics

