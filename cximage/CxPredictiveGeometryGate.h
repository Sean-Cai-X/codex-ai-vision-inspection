#pragma once

#include <opencv2/core.hpp>

#include <string>
#include <vector>

enum class CxGeometryConclusionStatus
{
    DirectMeasurement,
    ClampedMeasurement,
    PredictiveHypothesis,
    Undeterminable
};

struct CxGeometryPrimitiveHypothesis
{
    std::string geometry_type;
    std::string source;
    std::string tolerance_policy_ref;
    double model_confidence = 0.0;
    bool confidence_calibrated = false;
    std::string confidence_calibration_ref;
    double evidence_strength = 0.0;
    bool independent_image_evidence_verified = false;
    std::string image_evidence_ref;
    double classical_fit_residual_px = 0.0;
    double residual_limit_px = 0.0;
    double support = 0.0;
    double support_limit = 0.0;
    bool classical_verified = false;
    bool clamp_verified = false;
    double clamp_score = 0.0;

    cv::Point2d center;
    double radius = 0.0;
    cv::Size2d axes;
    double angle_deg = 0.0;
    std::vector<cv::Point2d> ordered_points;
};

struct CxPredictiveGeometryGatePolicy
{
    double direct_confidence = 0.85;
    double minimum_predictive_confidence = 0.35;
    double minimum_evidence_strength = 0.70;
    double minimum_clamp_score = 0.70;
};

struct CxPredictiveGeometryGateResult
{
    CxGeometryConclusionStatus status = CxGeometryConclusionStatus::Undeterminable;
    std::string reason;
    bool measurement_allowed = false;
    bool human_review_required = true;
};

const char* ToString(CxGeometryConclusionStatus status);

struct CxSegmentationGeometryFitOptions
{
    std::string geometry_type;
    std::string tolerance_policy_ref = "unbound_geometry_tolerance";
    std::string image_evidence_ref;
    double residual_limit_px = 2.0;
    double support_limit = 0.80;
    bool independent_image_evidence_verified = false;
};

struct CxSegmentationGeometryFitResult
{
    bool complete = false;
    std::string status = "NOT_RUN";
    std::string reason;
    CxGeometryPrimitiveHypothesis hypothesis;
};

bool FitCxSegmentationContourGeometry(
    const std::vector<cv::Point>& contour,
    const CxSegmentationGeometryFitOptions& options,
    CxSegmentationGeometryFitResult& result);


CxPredictiveGeometryGateResult EvaluateCxPredictiveGeometryGate(
    const CxGeometryPrimitiveHypothesis& hypothesis,
    const CxPredictiveGeometryGatePolicy& policy = {});

int RunCxPredictiveGeometryGateSelfTest(const std::string& output_directory);