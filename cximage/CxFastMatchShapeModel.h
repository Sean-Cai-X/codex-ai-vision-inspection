#ifndef CXIMAGE_CX_FAST_MATCH_SHAPE_MODEL_H
#define CXIMAGE_CX_FAST_MATCH_SHAPE_MODEL_H

#include "FastMatchTransform.h"

#include <opencv2/core.hpp>

#include <chrono>
#include <string>
#include <vector>

enum class CxFastMatchStructuralAnchorType
{
    Unknown = 0,
    DirectionJunction,
    CurvatureExtremum,
    ContourExtremum,
    SymmetryPair,
    OperatorConfirmed
};

struct CxFastMatchShapePoint
{
    double x = 0.0;
    double y = 0.0;
    double tangent_x = 1.0;
    double tangent_y = 0.0;
    double normal_x = 0.0;
    double normal_y = 1.0;
    double curvature = 0.0;
    double gradient_magnitude = 0.0;
    double confidence = 0.0;
    int polarity = 0;
    int source_direction = -1;
    int source_scan = -1;
    int source_point_index = -1;
    bool derived = false;
};

struct CxFastMatchStructuralAnchor
{
    CxFastMatchStructuralAnchorType type =
        CxFastMatchStructuralAnchorType::Unknown;
    int point_index = -1;
    int paired_anchor_index = -1;
    double x = 0.0;
    double y = 0.0;
    double strength = 0.0;
    std::string source;
};

struct CxFastMatchShapeModel
{
    std::string model_id;
    std::string coordinate_frame = "image_px";
    std::string status = "NOT_BUILT";
    bool available = false;
    bool closed = false;
    bool dense_available = false;
    double centroid_x = 0.0;
    double centroid_y = 0.0;
    double bbox_x = 0.0;
    double bbox_y = 0.0;
    double bbox_width = 0.0;
    double bbox_height = 0.0;
    int source_conclusion_count = 0;
    int derived_point_count = 0;
    int subpixel_point_count = 0;
    std::vector<CxFastMatchShapePoint> sparse_points;
    std::vector<CxFastMatchShapePoint> dense_points;
    std::vector<CxFastMatchStructuralAnchor> anchors;
};

struct CxFastMatchFormFitConfig
{
    bool enabled = false;
    int dense_sample_step_milli_px = 1000;
    int profile_half_width_milli_px = 2500;
    int profile_step_milli_px = 250;
    int profile_min_gradient = 4;
    int curvature_anchor_threshold_millideg = 12000;
    int maximum_structural_anchors = 64;
    int ann_search_radius_milli_px = 8000;
    int ann_normal_tolerance_deg = 35;
    int ann_trim_percent = 20;
    int minimum_mutual_pairs = 6;
    int maximum_iterations = 8;
    int maximum_elapsed_ms = 80;
    bool allow_nonuniform_affine = true;
};

struct CxFastMatchCorrespondence
{
    int reference_index = -1;
    int observed_index = -1;
    double distance_px = 0.0;
    double normal_delta_deg = 0.0;
    double weight = 0.0;
    bool mutual = false;
    bool accepted = false;
};

struct CxFastMatchFormFitResult
{
    bool executed = false;
    bool succeeded = false;
    bool budget_exceeded = false;
    std::string status = "NOT_RUN";
    std::string failure_stage = "not_run";
    std::string reference_model_id;
    std::string observed_model_id;
    int reference_sparse_count = 0;
    int observed_sparse_count = 0;
    int reference_dense_count = 0;
    int observed_dense_count = 0;
    int structural_anchor_count = 0;
    int sparse_mutual_count = 0;
    int dense_forward_count = 0;
    int dense_reverse_count = 0;
    int dense_mutual_count = 0;
    int rejected_distance_count = 0;
    int rejected_normal_count = 0;
    int iterations = 0;
    int elapsed_ms = 0;
    double forward_coverage = 0.0;
    double reverse_coverage = 0.0;
    double mutual_coverage = 0.0;
    double mean_residual_px = -1.0;
    double max_residual_px = -1.0;
    double symmetric_residual_px = -1.0;
    double normal_residual_deg = -1.0;
    double score = 0.0;
    double affine_a = 1.0;
    double affine_b = 0.0;
    double affine_c = 0.0;
    double affine_d = 1.0;
    double translate_x = 0.0;
    double translate_y = 0.0;
    double angle_deg = 0.0;
    double scale_x = 1.0;
    double scale_y = 1.0;
    std::vector<CxFastMatchCorrespondence> correspondences;
};

struct CxFastMatchSourceObservation
{
    double x = 0.0;
    double y = 0.0;
    double normal_x = 0.0;
    double normal_y = 1.0;
    int polarity = 0;
    int source_direction = -1;
    int source_scan = -1;
};

CxFastMatchShapeModel BuildFastMatchReferenceShapeModel(
    const std::vector<CxFastMatchSourceObservation>& source_points,
    const std::vector<cv::Point2d>& derived_closed_trace,
    const std::vector<cv::Point2d>& junction_points,
    const cv::Mat& image,
    const CxFastMatchFormFitConfig& config,
    const std::string& model_id);

CxFastMatchShapeModel BuildFastMatchObservedShapeModel(
    const CxFastMatchShapeModel& reference,
    const cv::Mat& image,
    const FastMatchTransform& seed,
    const CxFastMatchFormFitConfig& config,
    const std::string& model_id);

CxFastMatchFormFitResult RunFastMatchBidirectionalFormFit(
    const CxFastMatchShapeModel& reference,
    const CxFastMatchShapeModel& observed,
    const FastMatchTransform& seed,
    const CxFastMatchFormFitConfig& config);

const char* CxFastMatchStructuralAnchorTypeName(
    CxFastMatchStructuralAnchorType type);

#endif
