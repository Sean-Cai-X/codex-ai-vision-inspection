#pragma once
#include <complex>
#include <string>
#include <vector>

namespace cxgeom { namespace so2 {
// Pure numerical screening API. No image, GUI, model registry or runtime seed writes.
struct Config {
    int sample_count = 256;
    int max_order = 12;
    int minimum_source_points = 16;
    double minimum_perimeter = 20.0;
    bool normalize_scale = true;
    double maximum_pose_residual = 0.035;
    double symmetry_relative_amplitude = 0.002;
    double peak_relative_tolerance = 0.0001;
    int maximum_hypotheses = 16;
};
struct Contour {
    std::vector<std::complex<double>> points; // Original pixel coordinates, not bbox-normalized.
    bool topology_verified = false; // Caller must establish provenance and topology independently.
    bool closed = false;
    bool complete = true;
    int holes = 0;
    int components = 1;
};
enum class Method { Dft, Efd };
struct Descriptor {
    Config config;
    Method method = Method::Dft;
    std::vector<int> frequencies;
    std::vector<std::complex<double>> coefficients;
    std::complex<double> centroid{};
    double scale = 0;
    double perimeter = 0;
};
struct Pose {
    double angle_deg = 0, scale = 1, correlation = 0, residual = 0, cyclic_shift = 0;
    double translation_x = 0, translation_y = 0; // Reference to observation.
};
struct Result {
    bool succeeded = false;
    bool measurement_evidence = false;
    bool used_for_seed = false;
    bool used_for_prefilter = false;
    std::string status, fallback_reason;
    double invariant_distance = 0;
    int symmetry_order = 0;
    std::vector<Pose> poses;
};
// Invalid inputs throw std::invalid_argument with a stable reason code.
// Descriptor is an in-memory value: validation is repeated at every public boundary.
// Versioned persistence is provided separately by CxGeoSO2ReferenceAsset.h.
Descriptor Build(const Contour&, const Config& = Config{}, Method = Method::Dft);
double Distance(const Descriptor&, const Descriptor&);
Result Match(const Descriptor&, const Descriptor&);
}}
