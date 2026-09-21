#ifndef CX_SCRIPT_IMAGE_EVIDENCE_ANALYZER_H
#define CX_SCRIPT_IMAGE_EVIDENCE_ANALYZER_H

#include <vector>
#include <string>
#include <filesystem>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>

enum class CxFindlineEvidenceMode
{
    StrongestReferenceEdge = 0,
    MeasuredLocalEdge = 1,
    DualCompare = 2
};

struct CxMeasuredLocalEdgeEvidence
{
    bool found = false;

    double measured_x = 0.0;
    double measured_y = 0.0;

    double local_edge_x = 0.0;
    double local_edge_y = 0.0;

    double local_distance_px = 0.0;
    double local_gradient = 0.0;

    std::string polarity;
};

struct CxCircleLocalSupportStats
{
    int measured_points = 0;
    int local_edge_found = 0;
    int local_supported = 0;

    double mean_local_radial_distance_px = 0.0;
    double max_local_radial_distance_px = 0.0;
    double mean_local_gradient = 0.0;
    double local_support_score = 0.0;
};

struct CxImageEvidenceOptions
{
    bool enabled = true;

    double nearest_point_support_px = 3.0;
    double line_distance_support_px = 3.0;

    double min_gradient = 6.0;
    double min_gradient_ratio = 0.35;

    int profile_half_width = 40;
    int gradient_sample_radius = 2;

    int max_profiles = 200;

    bool use_line_distance_for_findline_support = true;

    bool save_profile_debug = false;

    std::string profile_name = "normal";

    CxFindlineEvidenceMode findline_mode = CxFindlineEvidenceMode::DualCompare;

    double measured_local_search_px = 8.0;
    double measured_local_support_px = 3.0;
    double reference_line_support_px = 4.0;
};

struct CxPointEvidence
{
    double measured_x = 0.0;
    double measured_y = 0.0;

    double reference_x = 0.0;
    double reference_y = 0.0;

    double nearest_reference_distance_px = 0.0;

    double reference_line_distance_px = 0.0;

    double radial_error_px = 0.0;

    double local_gradient = 0.0;
    double reference_gradient = 0.0;
    double gradient_ratio = 0.0;

    bool distance_supported = false;
    bool gradient_supported = false;
    bool combined_supported = false;

    bool local_edge_found = false;
    double local_edge_distance_px = 0.0;
    double local_edge_gradient = 0.0;
    bool local_edge_supported = false;

    std::string reference_polarity;
    std::string reason;
};

struct CxImageEvidenceSummary
{
    std::string tool;
    std::string object_name;

    bool reference_available = false;
    int reference_points_count = 0;

    int measured_points_count = 0;
    int supported_points_count = 0;
    int unsupported_points_count = 0;

    double mean_nearest_point_error_px = 0.0;
    double max_nearest_point_error_px = 0.0;

    double mean_reference_line_distance_px = 0.0;
    double max_reference_line_distance_px = 0.0;

    double mean_radial_error_px = 0.0;
    double max_radial_error_px = 0.0;

    double mean_error_px = 0.0;
    double max_error_px = 0.0;

    double edge_support_score = 0.0;

    double distance_support_score = 0.0;
    double gradient_support_score = 0.0;
    double combined_edge_support_score = 0.0;

    double mean_gradient_ratio = 0.0;

    int distance_supported_points = 0;
    int gradient_supported_points = 0;
    int combined_supported_points = 0;

    bool reference_fit_available = false;
    bool measured_fit_available = false;

    double fit_offset_error_px = 0.0;
    double fit_angle_error_deg = 0.0;

    double circle_center_error_px = 0.0;
    double circle_radius_error_px = 0.0;

    std::string best_reference_polarity;
    int positive_reference_points = 0;
    int negative_reference_points = 0;
    int abs_reference_points = 0;
    double mean_error_positive = 0.0;
    double mean_error_negative = 0.0;
    double mean_error_abs = 0.0;

    std::string primary_error_metric;
    std::string metric_quality;
    bool metric_valid = true;

    std::string conclusion;
    std::string support_conclusion;

    std::string line_orientation;
    double line_normal_x = 0.0;
    double line_normal_y = 0.0;

    double measured_local_support_score = 0.0;
    double measured_local_mean_distance_px = 0.0;
    double measured_local_max_distance_px = 0.0;
    double measured_local_mean_gradient = 0.0;
    int measured_local_supported_points = 0;
    int measured_local_missing_points = 0;

    double global_reference_mean_distance_px = 0.0;
    double global_reference_max_distance_px = 0.0;
    double global_reference_fit_offset_px = 0.0;

    double circle_local_support_score = 0.0;
    double circle_local_mean_radial_distance_px = 0.0;
    double circle_local_max_radial_distance_px = 0.0;
    double circle_local_mean_gradient = 0.0;
    std::string circle_reference_mode;

    double circle_global_reference_mean_distance_px = 0.0;
    double circle_global_reference_max_distance_px = 0.0;

    std::vector<CxPointEvidence> point_evidences;
};

struct ManualTestContext;

bool AnalyzeCxScriptImageEvidence(
    const cv::Mat& image,
    const ManualTestContext& context,
    const CxImageEvidenceOptions& options,
    const std::filesystem::path& outputDir,
    std::string& outReason);

#endif