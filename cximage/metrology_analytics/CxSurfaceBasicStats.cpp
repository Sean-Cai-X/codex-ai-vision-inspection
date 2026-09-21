#include "pch.h"
#include "metrology_analytics/CxSurfaceBasicStats.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace cxvision::metrology_analytics
{
namespace
{
CxHeightDistribution BuildDistribution(const CxSurfaceField& field, double min_z, double max_z, int bins)
{
    if (bins <= 0)
        throw std::invalid_argument("histogram bins must be positive.");

    CxHeightDistribution h;
    h.bin_centers.assign(static_cast<std::size_t>(bins), 0.0);
    h.adf.assign(static_cast<std::size_t>(bins), 0.0);
    h.bcdf.assign(static_cast<std::size_t>(bins), 0.0);

    const int n = field.valueCount();
    if (n <= 0)
        return h;

    const double range = max_z - min_z;
    if (range <= 0.0)
    {
        h.bin_centers[0] = min_z;
        h.adf[0] = 1.0;
        h.bcdf[0] = 1.0;
        for (int i = 1; i < bins; ++i)
        {
            h.bin_centers[static_cast<std::size_t>(i)] = min_z;
            h.bcdf[static_cast<std::size_t>(i)] = 0.0;
        }
        return h;
    }

    for (int i = 0; i < bins; ++i)
        h.bin_centers[static_cast<std::size_t>(i)] = min_z + (static_cast<double>(i) + 0.5) * range / static_cast<double>(bins);

    for (int y = 0; y < field.yres(); ++y)
    {
        for (int x = 0; x < field.xres(); ++x)
        {
            int idx = static_cast<int>(std::floor((field.at(x, y) - min_z) * static_cast<double>(bins) / range));
            idx = std::max(0, std::min(bins - 1, idx));
            h.adf[static_cast<std::size_t>(idx)] += 1.0;
        }
    }

    for (double& v : h.adf)
        v /= static_cast<double>(n);

    double running = 0.0;
    for (int i = bins - 1; i >= 0; --i)
    {
        running += h.adf[static_cast<std::size_t>(i)];
        h.bcdf[static_cast<std::size_t>(i)] = running;
    }
    return h;
}
}

std::vector<CxHeightPeak> findHeightDistributionPeaks(
    const CxHeightDistribution& distribution,
    const CxHeightPeakOptions& options)
{
    const std::size_t n = distribution.bin_centers.size();
    if (distribution.adf.size() != n || distribution.bcdf.size() != n)
        throw std::invalid_argument(
            "height distribution vectors must have identical sizes.");
    if (options.max_peaks <= 0 || n < 3)
        return {};
    if (options.min_prominence < 0.0 || options.min_distance_bins < 0)
        throw std::invalid_argument(
            "peak prominence and distance must be non-negative.");

    const double curve_max =
        *std::max_element(distribution.adf.begin(), distribution.adf.end());
    std::vector<double> signal(n, 0.0);
    for (std::size_t i = 0; i < n; ++i)
    {
        const double value = distribution.adf[i];
        if (!std::isfinite(value) ||
            !std::isfinite(distribution.bin_centers[i]))
            throw std::invalid_argument(
                "height distribution values must be finite.");
        signal[i] = options.invert ? curve_max - value : value;
    }

    std::vector<CxHeightPeak> candidates;
    for (std::size_t i = 1; i + 1 < n; ++i)
    {
        const double y_left = signal[i - 1];
        const double y = signal[i];
        const double y_right = signal[i + 1];
        if (!(y > y_left && y >= y_right))
            continue;

        double baseline = 0.0;
        if (options.background == CxHeightPeakBackground::BilateralMinimum)
        {
            const double left_min =
                *std::min_element(signal.begin(), signal.begin() + i + 1);
            const double right_min =
                *std::min_element(signal.begin() + i, signal.end());
            baseline = std::max(left_min, right_min);
        }

        const double prominence = y - baseline;
        if (prominence < options.min_prominence || prominence <= 0.0)
            continue;

        const double denominator = y_left - 2.0 * y + y_right;
        double sub_bin_offset = 0.0;
        if (std::abs(denominator) > 1.0e-15)
        {
            sub_bin_offset =
                0.5 * (y_left - y_right) / denominator;
            sub_bin_offset =
                std::max(-0.5, std::min(0.5, sub_bin_offset));
        }

        const double local_step =
            sub_bin_offset >= 0.0
                ? distribution.bin_centers[i + 1] -
                      distribution.bin_centers[i]
                : distribution.bin_centers[i] -
                      distribution.bin_centers[i - 1];
        const double refined_position =
            distribution.bin_centers[i] + sub_bin_offset * local_step;

        const double half_height = baseline + 0.5 * prominence;
        std::size_t left = i;
        while (left > 0 && signal[left] > half_height)
            --left;
        std::size_t right = i;
        while (right + 1 < n && signal[right] > half_height)
            ++right;

        double area = 0.0;
        for (std::size_t j = left; j < right; ++j)
        {
            const double dx =
                distribution.bin_centers[j + 1] -
                distribution.bin_centers[j];
            const double a = std::max(0.0, signal[j] - baseline);
            const double b =
                std::max(0.0, signal[j + 1] - baseline);
            area += 0.5 * (a + b) * std::abs(dx);
        }

        CxHeightPeak peak;
        peak.bin_index = static_cast<int>(i);
        peak.sub_bin_offset = sub_bin_offset;
        peak.position = refined_position;
        peak.curve_value = distribution.adf[i];
        peak.prominence = prominence;
        peak.area = area;
        peak.width = std::abs(
            distribution.bin_centers[right] -
            distribution.bin_centers[left]);
        if (peak.width <= 0.0)
            peak.width = std::abs(local_step);
        candidates.push_back(peak);
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const CxHeightPeak& a, const CxHeightPeak& b)
              {
                  if (a.prominence != b.prominence)
                      return a.prominence > b.prominence;
                  return a.position < b.position;
              });

    std::vector<CxHeightPeak> selected;
    selected.reserve(std::min(
        candidates.size(), static_cast<std::size_t>(options.max_peaks)));
    for (const CxHeightPeak& candidate : candidates)
    {
        bool separated = true;
        for (const CxHeightPeak& accepted : selected)
        {
            if (std::abs(candidate.bin_index - accepted.bin_index) <
                options.min_distance_bins)
            {
                separated = false;
                break;
            }
        }
        if (!separated)
            continue;
        selected.push_back(candidate);
        if (selected.size() >=
            static_cast<std::size_t>(options.max_peaks))
            break;
    }

    if (options.order == CxHeightPeakOrder::Position)
    {
        std::sort(selected.begin(), selected.end(),
                  [](const CxHeightPeak& a, const CxHeightPeak& b)
                  {
                      return a.position < b.position;
                  });
    }
    return selected;
}

CxSurfaceBasicStats computeSurfaceBasicStats(const CxSurfaceField& field, int bins_primary)
{
    if (field.valueCount() <= 0)
        throw std::invalid_argument("computeSurfaceBasicStats requires a non-empty field.");

    CxSurfaceBasicStats s;
    s.min = std::numeric_limits<double>::infinity();
    s.max = -std::numeric_limits<double>::infinity();

    double sum = 0.0;
    for (int y = 0; y < field.yres(); ++y)
    {
        for (int x = 0; x < field.xres(); ++x)
        {
            const double z = field.at(x, y);
            s.min = std::min(s.min, z);
            s.max = std::max(s.max, z);
            sum += z;
        }
    }

    const double n = static_cast<double>(field.valueCount());
    s.mean = sum / n;

    double sum_abs = 0.0;
    double sum2 = 0.0;
    double sum3 = 0.0;
    double sum4 = 0.0;
    for (int y = 0; y < field.yres(); ++y)
    {
        for (int x = 0; x < field.xres(); ++x)
        {
            const double d = field.at(x, y) - s.mean;
            const double d2 = d * d;
            sum_abs += std::abs(d);
            sum2 += d2;
            sum3 += d2 * d;
            sum4 += d2 * d2;
        }
    }

    s.ra = sum_abs / n;
    s.rms = std::sqrt(sum2 / n);
    if (s.rms > 0.0)
    {
        const double sigma3 = s.rms * s.rms * s.rms;
        const double sigma4 = sigma3 * s.rms;
        s.skewness = (sum3 / n) / sigma3;
        s.kurtosis_excess = (sum4 / n) / sigma4 - 3.0;
    }

    s.height_distribution_primary = BuildDistribution(field, s.min, s.max, bins_primary);
    s.height_distribution_1024 = BuildDistribution(field, s.min, s.max, 1024);
    return s;
}

} // namespace cxvision::metrology_analytics
