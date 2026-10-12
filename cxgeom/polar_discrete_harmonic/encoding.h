#pragma once
#include <array>
#include <complex>
#include <cstddef>
#include <string>
#include <vector>
namespace cxgeom::polar {
struct Feature {
 std::string stable_id, source_element_id;
 std::complex<double> point;
 double weight=1, local_direction_rad=0, curvature=0;
 bool has_direction=false;
};
struct Config {
 int bins=128, max_order=16;
 double minimum_rms_radius=1e-6;
};
struct Descriptor {
 Config config;
 std::string source_id;
 std::vector<Feature> features; // canonical stable-ID order, all original attributes retained
 std::complex<double> centroid;
 double rms_radius=0, total_weight=0, central_weight_fraction=0;
 std::array<std::vector<double>,3> angular_signal;
 std::array<std::vector<std::complex<double>>,3> moments;
 bool geometry_is_contour=false, production_eligible=false;
};
// Encoding only: no pose, correspondence, solvability, or silent topology invention.
// Channels are mass, mass*r/RMS, mass*(r/RMS)^2; moments use exp(-ik*theta).
// Missing/outlier features change the normalization frame; spatial validation is mandatory.
Descriptor Encode(const std::string& source_id,const std::vector<Feature>&,const Config& = {});
}
