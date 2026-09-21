#include "pch.h"
#include "metrology_analytics/CxRoughness1D.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace cxvision::metrology_analytics
{

CxRoughness1DResult computeProfileRoughness(const CxProfile1D& profile, int bins)
{
    if (profile.z.empty())
        throw std::invalid_argument("computeProfileRoughness requires non-empty profile.");
    if (profile.delta_x_physical <= 0.0)
        throw std::invalid_argument("computeProfileRoughness requires positive delta_x_physical.");
    if (bins <= 0)
        throw std::invalid_argument("computeProfileRoughness requires positive bins.");

    CxRoughness1DResult r;
    const double n = static_cast<double>(profile.z.size());
    double sum = 0.0;
    double min_z = std::numeric_limits<double>::infinity();
    double max_z = -std::numeric_limits<double>::infinity();
    for (double z : profile.z)
    {
        sum += z;
        min_z = std::min(min_z, z);
        max_z = std::max(max_z, z);
    }
    const double mean = sum / n;

    double sum_abs = 0.0;
    double sum2 = 0.0;
    double sum3 = 0.0;
    double sum4 = 0.0;
    for (double z : profile.z)
    {
        const double d = z - mean;
        const double d2 = d * d;
        sum_abs += std::abs(d);
        sum2 += d2;
        sum3 += d2 * d;
        sum4 += d2 * d2;
    }

    r.Ra = sum_abs / n;
    r.Rq = std::sqrt(sum2 / n);
    if (r.Rq > 0.0)
    {
        const double sigma3 = r.Rq * r.Rq * r.Rq;
        const double sigma4 = sigma3 * r.Rq;
        r.Rsk = (sum3 / n) / sigma3;
        r.Rku_excess = (sum4 / n) / sigma4 - 3.0;
        r.Rku_std = r.Rku_excess + 3.0;
    }

    r.adf_bins.assign(static_cast<std::size_t>(bins), 0.0);
    r.bcdf_curve.assign(static_cast<std::size_t>(bins), 0.0);
    const double range = max_z - min_z;
    if (range <= 0.0)
    {
        r.adf_bins[0] = 1.0;
        r.bcdf_curve[0] = 1.0;
        return r;
    }

    for (double z : profile.z)
    {
        int idx = static_cast<int>(std::floor((z - min_z) * static_cast<double>(bins) / range));
        idx = std::max(0, std::min(bins - 1, idx));
        r.adf_bins[static_cast<std::size_t>(idx)] += 1.0;
    }
    for (double& v : r.adf_bins)
        v /= n;
    double running = 0.0;
    for (int i = bins - 1; i >= 0; --i)
    {
        running += r.adf_bins[static_cast<std::size_t>(i)];
        r.bcdf_curve[static_cast<std::size_t>(i)] = running;
    }
    return r;
}

} // namespace cxvision::metrology_analytics

