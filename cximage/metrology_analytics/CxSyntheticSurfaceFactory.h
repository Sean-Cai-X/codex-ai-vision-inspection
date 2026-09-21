#pragma once

#include "metrology_analytics/CxSurfaceField.h"

#include <cstdint>

namespace cxvision::metrology_analytics
{

class CxSyntheticSurfaceFactory
{
public:
    static CxSurfaceField flat(int xres, int yres, double z0, const CxPhysUnit& unit = {});
    static CxSurfaceField plane(int xres, int yres, double ax, double ay, double c, const CxPhysUnit& unit = {});
    static CxSurfaceField sine1D(int xres, int yres, double amplitude, double lambda_px, int axis = 0, const CxPhysUnit& unit = {});
    static CxSurfaceField gaussianRandom(int xres, int yres, double mu, double sigma, std::uint64_t seed, const CxPhysUnit& unit = {});
    static CxSurfaceField bimodal(int xres, int yres, double w, double m1, double s1, double m2, double s2, std::uint64_t seed, const CxPhysUnit& unit = {});
    static CxSurfaceField perfectDiscs(int xres, int yres, int n, double radius_px, double height, double min_dist, std::uint64_t seed, const CxPhysUnit& unit = {});
};

} // namespace cxvision::metrology_analytics

