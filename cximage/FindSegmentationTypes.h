#pragma once

#include "CxPredictiveGeometryGate.h"


#include <opencv2/opencv.hpp>



#include <string>

#include <vector>



struct FindSegmentationContour

{

    std::vector<cv::Point> points;

    std::string source_instance_id;
    std::string source_mask_ref;
    std::string source_contour_ref;

    double area = 0.0;

    double perimeter = 0.0;

};

struct FindSegmentationRegion

{

    std::string stable_id;



    int class_id = -1;

    std::string class_name;

    double confidence = 0.0;



    cv::Rect bbox;

    cv::Mat mask;

    FindSegmentationContour contour;



    std::string mask_ref;

    std::string contour_ref;

};



struct FindSegmentationResult

{

    bool ok = false;



    std::string backend = "opencv_smoke";

    std::string task_id;

    std::string model_id;

    std::string model_package_ref;

    std::string manifest_path;

    std::string output_root;

    std::string postprocess_profile;

    std::string parameter_profile_ref;



    std::string backend_status = "not_run";

    std::string status = "not_run";

    std::string reason;



    cv::Mat mask;

    cv::Mat overlay;



    std::vector<FindSegmentationContour> contours;

    std::vector<FindSegmentationRegion> regions;

    // Model-native and contour-fit geometry candidates share this value-semantic
    // contract. Gate status is computed separately from raw inference output.
    std::vector<CxGeometryPrimitiveHypothesis> primitive_hypotheses;
    std::string requested_geometry_type;
    std::string geometry_fit_status = "not_run";
    std::string geometry_fit_reason;

    // Path and status of the mask-to-minAreaRect OBB baseline.  It is kept
    // separate from a model-native angle-regression result.
    std::string oriented_box_evidence_ref;
    std::string oriented_box_conclusion;




    int mask_width = 0;

    int mask_height = 0;

    int contour_count = 0;

    int region_count = 0;

    double primary_area = 0.0;



    bool raw_result_available = false;

    bool refined_result_available = false;

    bool fallback_used = false;

    std::string result_stage = "not_run";

    std::string refinement_method;



    std::string result_ref;

    std::string mask_ref;

    std::string contour_ref;

    std::string overlay_ref;



    std::string raw_result_ref;

    std::string raw_mask_ref;

    std::string raw_contour_ref;

    std::string raw_overlay_ref;



    std::string refined_result_ref;

    std::string refined_mask_ref;

    std::string refined_contour_ref;

    std::string refined_overlay_ref;

};



struct FindSegmentationInputSnapshot

{

    std::string backend = "opencv_smoke";

    std::string model_path;

    std::string device = "auto";

    std::string task_id;

    std::string model_id;

    std::string model_package_ref;

    std::string manifest_path;

    std::string output_root;

    std::string postprocess_profile;

    std::string parameter_profile_ref;



    double threshold = 0.5;

    int mode = 0;



    int image_width = 0;

    int image_height = 0;



    bool has_rect = false;

    int rect_x = 0;

    int rect_y = 0;

    int rect_width = 0;

    int rect_height = 0;



    // Legacy single point is retained only for replay compatibility. New

    // scripts use explicitly typed positive/negative prompts.

    bool has_point = false;

    int point_x = 0;

    int point_y = 0;



    bool has_positive_point = false;

    int positive_point_x = 0;

    int positive_point_y = 0;

    bool has_negative_point = false;

    int negative_point_x = 0;

    int negative_point_y = 0;

};



struct FindSegmentationBackendDiagnosticSnapshot

{

    std::string backend = "opencv_smoke";

    std::string task_id;

    std::string model_id;

    std::string model_package_ref;

    std::string manifest_path;

    std::string postprocess_profile;

    std::string parameter_profile_ref;



    std::string backend_status = "not_run";

    std::string status = "not_run";

    std::string reason;



    bool image_ready = false;

    bool prompt_rect_ready = false;

    bool prompt_point_ready = false;

    bool prompt_positive_ready = false;

    bool prompt_negative_ready = false;

    bool mask_ready = false;

    bool overlay_ready = false;



    bool raw_result_available = false;

    bool refined_result_available = false;

    bool fallback_used = false;

    std::string result_stage = "not_run";

    std::string refinement_method;



    int mask_width = 0;

    int mask_height = 0;

    int contour_count = 0;

    int region_count = 0;

    double primary_area = 0.0;

};
