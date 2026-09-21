#pragma once

#include <string>
#include <vector>

struct CxSegmentationPoint
{
    double x = 0.0;
    double y = 0.0;
};

struct CxSegmentationBoundingBox
{
    double x0 = 0.0;
    double y0 = 0.0;
    double x1 = 0.0;
    double y1 = 0.0;
};

struct CxImageTransform
{
    int original_width = 0;
    int original_height = 0;
    int roi_x = 0;
    int roi_y = 0;
    int roi_width = 0;
    int roi_height = 0;
    int network_width = 0;
    int network_height = 0;
    int prototype_width = 0;
    int prototype_height = 0;
    double letterbox_scale = 1.0;
    double pad_x = 0.0;
    double pad_y = 0.0;
};

struct CxSegmentationInstance
{
    std::string stable_id;
    int class_id = -1;
    std::string class_name;
    double class_confidence = 0.0;
    double mask_quality = 0.0;
    double stability_score = 0.0;
    CxSegmentationBoundingBox bbox;
    std::string binary_mask_ref;
    std::string contour_ref;
    std::vector<std::vector<CxSegmentationPoint>> outer_contours;
    std::vector<std::vector<CxSegmentationPoint>> holes;
    double pixel_area = 0.0;
    CxSegmentationPoint centroid;
};

struct CxSegmentationEvidence
{
    std::string schema = "cxvision.segmentation_evidence.v2";
    std::string evidence_id;
    std::string parent_evidence_id;
    std::string provider;
    std::string model_id;
    std::string weights_hash;
    std::string input_image_ref;
    std::string input_image_hash;
    CxImageTransform transform;
    std::vector<CxSegmentationInstance> instances;
    std::string overlay_ref;
    std::string metrics_ref;
};

struct CxMeasurementEvidence
{
    std::string schema = "cxvision.measurement_evidence.v1";
    std::string evidence_id;
    std::string segmentation_evidence_id;
    std::string instance_id;
    std::string raw_mask_contour_ref;
    std::string refined_edge_points_ref;
    std::string rejected_edge_points_ref;
    std::string fitted_primitive;
    double fit_residual = 0.0;
    double pixel_measurement = 0.0;
    double physical_measurement = 0.0;
    std::string physical_unit = "pixel";
    double uncertainty = 0.0;
    std::string overlay_ref;
};
