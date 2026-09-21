#pragma once

#include "FindSegmentationTypes.h"

#include <opencv2/opencv.hpp>

#include <string>

struct FindSegmentationInput
{
    cv::Mat image;

    std::string model_path;
    std::string device = "auto";
    std::string backend = "opencv_smoke";
    std::string task_id;
    std::string model_id;
    std::string model_package_ref;
    std::string manifest_path;
    std::string output_root;
    std::string postprocess_profile;
    std::string parameter_profile_ref;

    double threshold = 0.5;
    int mode = 0;

    bool has_rect = false;
    cv::Rect rect;

    bool has_point = false;
    cv::Point point;

    bool has_positive_point = false;
    cv::Point positive_point;
    bool has_negative_point = false;
    cv::Point negative_point;
};

class IFindSegmentationBackend
{
public:
    virtual ~IFindSegmentationBackend() = default;

    virtual bool Run(
        const FindSegmentationInput& input,
        FindSegmentationResult& output,
        std::string& reason) = 0;
};
