#pragma once

#include <string>
#include <vector>

#include <opencv2/core.hpp>

namespace cxcore {

struct RegionPatternConfig
{
    int normalized_width = 32;
    int normalized_height = 32;
    int pooling_rows = 4;
    int pooling_cols = 4;
    bool use_binary = false;
    double binarize_threshold = 128.0;
    bool foreground_is_dark = true;
};

struct RegionPatternDescriptor
{
    int normalized_width = 0;
    int normalized_height = 0;
    int pooling_rows = 0;
    int pooling_cols = 0;
    double global_foreground_ratio = 0.0;
    std::vector<double> values;
};

struct RegionPatternTemplate
{
    std::string template_id;
    RegionPatternDescriptor descriptor;
};

struct RegionPatternScore
{
    bool success = false;
    double descriptor_distance = 0.0;
    double content_score = 0.0;
    std::string summary;
};

class RegionPatternNet
{
public:
    RegionPatternNet() = default;

    void SetConfig(const RegionPatternConfig& config);
    const RegionPatternConfig& GetConfig() const;

    RegionPatternDescriptor BuildDescriptor(const cv::Mat& roi_patch) const;
    RegionPatternTemplate BuildTemplate(const std::string& template_id,
                                        const cv::Mat& roi_patch) const;
    RegionPatternScore Score(const RegionPatternDescriptor& lhs,
                             const RegionPatternDescriptor& rhs) const;

private:
    RegionPatternConfig config_;
};

}  // namespace cxcore
