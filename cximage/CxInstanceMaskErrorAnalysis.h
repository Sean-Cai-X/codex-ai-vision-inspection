#pragma once
#include "../libtorchsegmentation/src/utils/json.hpp"
#include <opencv2/core.hpp>
#include <string>
#include <vector>

namespace cxvision::mask_error {
struct Prediction {
    std::string id;
    int class_id = -1;
    double score = 0;
    cv::Mat mask; // Original image dimensions, binary CV_8UC1. Never modified.
};
// Explicitly SINGLE-GT diagnostic, not a general multiple-object evaluator.
// Empty truth means unlabelled: all GT-dependent measurements remain null.
// Matching chooses the largest mask IoU WITHOUT class filtering, tie by ID.
// Caller owns sample role/identity. This function never changes dataset splits.
nlohmann::json AnalyzeSingleGroundTruth(cv::Size original_size,
    int target_class, const cv::Mat& truth, const std::vector<Prediction>& predictions,
    double matching_iou = 0.5, double boundary_iou = 0.9);
}
