#include "RegionPatternNet.h"

#include <algorithm>
#include <cmath>

#include <opencv2/imgproc.hpp>

namespace cxcore {
namespace {

cv::Mat NormalizePatch(const cv::Mat& input, const RegionPatternConfig& config)
{
    if (input.empty()) {
        return cv::Mat();
    }

    cv::Mat gray;
    if (input.channels() == 1) {
        gray = input;
    } else {
        cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);
    }

    cv::Mat resized;
    cv::resize(gray,
               resized,
               cv::Size(config.normalized_width, config.normalized_height),
               0.0,
               0.0,
               cv::INTER_LINEAR);

    if (!config.use_binary) {
        return resized;
    }

    cv::Mat binary;
    const int threshold_mode = config.foreground_is_dark ? cv::THRESH_BINARY_INV : cv::THRESH_BINARY;
    cv::threshold(resized, binary, config.binarize_threshold, 255.0, threshold_mode);
    return binary;
}

double ComputeForegroundRatio(const cv::Mat& normalized, const RegionPatternConfig& config)
{
    if (normalized.empty()) {
        return 0.0;
    }

    if (config.use_binary) {
        return static_cast<double>(cv::countNonZero(normalized)) /
               static_cast<double>(normalized.rows * normalized.cols);
    }

    cv::Mat foreground_mask;
    if (config.foreground_is_dark) {
        cv::threshold(normalized,
                      foreground_mask,
                      config.binarize_threshold,
                      255.0,
                      cv::THRESH_BINARY_INV);
    } else {
        cv::threshold(normalized,
                      foreground_mask,
                      config.binarize_threshold,
                      255.0,
                      cv::THRESH_BINARY);
    }

    return static_cast<double>(cv::countNonZero(foreground_mask)) /
           static_cast<double>(normalized.rows * normalized.cols);
}

std::vector<double> BuildPooledValues(const cv::Mat& normalized, const RegionPatternConfig& config)
{
    std::vector<double> values;
    if (normalized.empty() || config.pooling_rows <= 0 || config.pooling_cols <= 0) {
        return values;
    }

    values.reserve(static_cast<size_t>(config.pooling_rows * config.pooling_cols));
    for (int row = 0; row < config.pooling_rows; ++row) {
        const int y0 = row * normalized.rows / config.pooling_rows;
        const int y1 = (row + 1) * normalized.rows / config.pooling_rows;
        for (int col = 0; col < config.pooling_cols; ++col) {
            const int x0 = col * normalized.cols / config.pooling_cols;
            const int x1 = (col + 1) * normalized.cols / config.pooling_cols;
            const cv::Rect roi(x0, y0, std::max(1, x1 - x0), std::max(1, y1 - y0));
            const cv::Scalar block_mean = cv::mean(normalized(roi));
            values.push_back(block_mean[0] / 255.0);
        }
    }
    return values;
}

double ComputeDescriptorDistance(const RegionPatternDescriptor& lhs,
                                 const RegionPatternDescriptor& rhs)
{
    if (lhs.values.size() != rhs.values.size() || lhs.values.empty()) {
        return 1.0;
    }

    double squared_sum = 0.0;
    for (size_t i = 0; i < lhs.values.size(); ++i) {
        const double diff = lhs.values[i] - rhs.values[i];
        squared_sum += diff * diff;
    }
    return std::sqrt(squared_sum / static_cast<double>(lhs.values.size()));
}

}  // namespace

void RegionPatternNet::SetConfig(const RegionPatternConfig& config)
{
    config_ = config;
}

const RegionPatternConfig& RegionPatternNet::GetConfig() const
{
    return config_;
}

RegionPatternDescriptor RegionPatternNet::BuildDescriptor(const cv::Mat& roi_patch) const
{
    RegionPatternDescriptor descriptor;
    descriptor.normalized_width = config_.normalized_width;
    descriptor.normalized_height = config_.normalized_height;
    descriptor.pooling_rows = config_.pooling_rows;
    descriptor.pooling_cols = config_.pooling_cols;

    const cv::Mat normalized = NormalizePatch(roi_patch, config_);
    descriptor.global_foreground_ratio = ComputeForegroundRatio(normalized, config_);
    descriptor.values = BuildPooledValues(normalized, config_);
    return descriptor;
}

RegionPatternTemplate RegionPatternNet::BuildTemplate(const std::string& template_id,
                                                      const cv::Mat& roi_patch) const
{
    RegionPatternTemplate output;
    output.template_id = template_id;
    output.descriptor = BuildDescriptor(roi_patch);
    return output;
}

RegionPatternScore RegionPatternNet::Score(const RegionPatternDescriptor& lhs,
                                           const RegionPatternDescriptor& rhs) const
{
    RegionPatternScore score;
    if (lhs.values.empty() || rhs.values.empty() || lhs.values.size() != rhs.values.size()) {
        score.summary = "descriptor shape mismatch";
        return score;
    }

    score.success = true;
    score.descriptor_distance = ComputeDescriptorDistance(lhs, rhs);
    score.content_score = std::max(0.0, 1.0 - score.descriptor_distance);
    score.summary = "descriptor distance computed";
    return score;
}

}  // namespace cxcore
