#pragma once

#include <vector>

namespace cxvision::metrology_analytics
{

struct CxProfile1D
{
    std::vector<double> z;
    double delta_x_physical = 1.0;
};

struct CxRoughness1DResult
{
    double Ra = 0.0;
    double Rq = 0.0;
    double Rsk = 0.0;
    double Rku_excess = 0.0;
    double Rku_std = 0.0;
    std::vector<double> adf_bins;
    std::vector<double> bcdf_curve;
};

CxRoughness1DResult computeProfileRoughness(const CxProfile1D& profile, int bins = 1024);

} // namespace cxvision::metrology_analytics

