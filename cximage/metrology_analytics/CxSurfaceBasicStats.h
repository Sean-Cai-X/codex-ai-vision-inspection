#pragma once

#include "metrology_analytics/CxSurfaceField.h"

#include <vector>

namespace cxvision::metrology_analytics
{

struct CxHeightDistribution
{
    std::vector<double> bin_centers;
    std::vector<double> adf;
    std::vector<double> bcdf;
};

enum class CxHeightPeakOrder
{
    Position = 0,
    Prominence
};

enum class CxHeightPeakBackground
{
    Zero = 0,
    BilateralMinimum
};

struct CxHeightPeakOptions
{
    int max_peaks = 12;
    double min_prominence = 0.0;
    int min_distance_bins = 1;
    bool invert = false;
    CxHeightPeakOrder order = CxHeightPeakOrder::Position;
    CxHeightPeakBackground background =
        CxHeightPeakBackground::BilateralMinimum;
};

struct CxHeightPeak
{
    int bin_index = 0;
    double sub_bin_offset = 0.0;
    double position = 0.0;
    double curve_value = 0.0;
    double prominence = 0.0;
    double area = 0.0;
    double width = 0.0;
};

std::vector<CxHeightPeak> findHeightDistributionPeaks(
    const CxHeightDistribution& distribution,
    const CxHeightPeakOptions& options = {});

struct CxSurfaceBasicStats
{
    double min = 0.0;
    double max = 0.0;
    double mean = 0.0;
    double rms = 0.0;
    double ra = 0.0;
    double skewness = 0.0;
    double kurtosis_excess = 0.0;
    CxHeightDistribution height_distribution_primary;
    CxHeightDistribution height_distribution_1024;
};

CxSurfaceBasicStats computeSurfaceBasicStats(const CxSurfaceField& field, int bins_primary = 256);

} // namespace cxvision::metrology_analytics