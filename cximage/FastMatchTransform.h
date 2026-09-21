#ifndef CXIMAGE_FAST_MATCH_TRANSFORM_H
#define CXIMAGE_FAST_MATCH_TRANSFORM_H

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

// Value-only transform used by the affine FastMatch search.  It deliberately
// has no Image/Shape ownership, so it can also be supplied by an OBB adapter.
struct FastMatchTransform
{
    double cx = 0.0;
    double cy = 0.0;
    double angle_deg = 0.0;
    double scale_x = 1.0;
    double scale_y = 1.0;
    double shear = 0.0;
    // P1 projective terms operate in local-template coordinates.  Zero keeps
    // the exact affine mapping used by P0 and the legacy rigid path.
    double projective_u = 0.0;
    double projective_v = 0.0;
    double half_u = 0.0;
    double half_v = 0.0;

    static FastMatchTransform fromOrientedBox(double center_x, double center_y,
                                              double width, double height,
                                              double angle_degrees)
    {
        FastMatchTransform value;
        value.cx = center_x;
        value.cy = center_y;
        value.angle_deg = angle_degrees;
        value.half_u = std::max(0.0, width * 0.5);
        value.half_v = std::max(0.0, height * 0.5);
        return value;
    }

    bool valid() const
    {
        return std::isfinite(cx) && std::isfinite(cy) &&
               std::isfinite(angle_deg) && std::isfinite(scale_x) &&
               std::isfinite(scale_y) && std::isfinite(shear) &&
               std::isfinite(projective_u) && std::isfinite(projective_v) &&
               std::abs(scale_x) > 1e-9 && std::abs(scale_y) > 1e-9;
    }

    void localToGlobal(double u, double v, double& x, double& y) const
    {
        const double radians = angle_deg * 3.14159265358979323846 / 180.0;
        const double c = std::cos(radians);
        const double s = std::sin(radians);
        const double qx = scale_x * u + shear * scale_y * v;
        const double qy = scale_y * v;
        const double denominator = 1.0 + projective_u * u + projective_v * v;
        if (std::abs(denominator) <= 1e-9) {
            x = y = std::numeric_limits<double>::quiet_NaN();
            return;
        }
        x = cx + (c * qx - s * qy) / denominator;
        y = cy + (s * qx + c * qy) / denominator;
    }

    bool globalToLocal(double x, double y, double& u, double& v) const
    {
        if (!valid())
            return false;
        const double radians = angle_deg * 3.14159265358979323846 / 180.0;
        const double c = std::cos(radians);
        const double s = std::sin(radians);
        const double dx = x - cx;
        const double dy = y - cy;
        const double qx = c * dx + s * dy;
        const double qy = -s * dx + c * dy;
        const double rv = qy / scale_y;
        const double ru = (qx - shear * scale_y * rv) / scale_x;
        const double denominator = 1.0 - projective_u * ru - projective_v * rv;
        if (std::abs(denominator) <= 1e-9)
            return false;
        u = ru / denominator;
        v = rv / denominator;
        return std::isfinite(u) && std::isfinite(v);
    }
};

struct FastMatchTransformSearchConfig
{
    bool enabled = false;
    int scale_range_percent = 15;
    int angle_range_deg = 15;
    int coarse_steps_per_axis = 3;
    int fine_range_percent = 3;
    int fine_angle_range_deg = 3;
    int max_candidates = 27;
    int max_samples = 200000;
    int max_elapsed_ms = 100;
    int shear_range_permille = 0;
    int projective_range_permille = 0;
};

struct FastMatchTransformSeedEvidence
{
    bool available = false;
    std::string source = "manual";
    std::string model_id;
    int detection_index = -1;
    int class_id = -1;
    double confidence = 0.0;
    std::string angle_convention;
    std::string reason;
};

struct FastMatchTransformSearchResult
{
    bool executed = false;
    bool converged = false;
    bool budget_exceeded = false;
    bool fallback_to_rigid = false;
    // Bound by value from CxCalibration.  Search always remains in pixel
    // coordinates; this receipt only establishes output metrology provenance.
    bool calibration_applied = false;
    std::string calibration_snapshot_hash;
    std::string calibration_source_ref;
    std::string calibration_coordinate_frame_id;
    std::string calibration_xy_unit;
    double calibration_reprojection_rmse_px = -1.0;
    double best_physical_cx = 0.0;
    double best_physical_cy = 0.0;
    FastMatchTransformSeedEvidence seed;
    FastMatchTransform initial;
    FastMatchTransform best;
    double best_score = 0.0;
    double appearance_score = 0.0;
    double continuity_score = 0.0;
    double gradient_score = 0.0;
    double geometric_residual_px = 0.0;
    double rigid_baseline_score = 0.0;
    int evaluated_candidates = 0;
    int accepted_candidates = 0;
    int sample_count = 0;
    int elapsed_ms = 0;
    const char* failure_stage = "not_run";
};

#endif
