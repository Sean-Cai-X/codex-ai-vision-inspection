#pragma once

#include <string>
#include <vector>

#include <opencv2/core.hpp>

namespace cxcore {

struct GridPatternLevelShape
{
    int rows = 0;
    int cols = 0;
};

struct GridPatternConfig
{
    int normalized_width = 48;
    int normalized_height = 48;
    int grid_rows = 12;
    int grid_cols = 12;
    int orientation_bins = 8;
    double foreground_threshold = -1.0;
    bool foreground_is_dark = true;
    bool equalize_contrast = false;
    double edge_threshold = 0.08;
    double active_foreground_ratio = 0.05;
    double active_edge_ratio = 0.03;
    double distance_scale = 0.20;
    double min_class_score = 0.55;
    double min_class_margin = 0.03;
    int top_k = 3;
    std::vector<GridPatternLevelShape> hierarchy = {
        {12, 12},
        {6, 6},
        {3, 3}
    };
};

struct GridCellFeature
{
    int cell_id = -1;
    int row = 0;
    int col = 0;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    bool valid = false;
    bool active = false;
    double foreground_ratio = 0.0;
    double gray_mean = 0.0;
    double gray_stddev = 0.0;
    double edge_density = 0.0;
    double dominant_orientation_degrees = -1.0;
    std::vector<double> orientation_histogram;
    std::vector<double> values;
};

struct GridFeatureMap
{
    bool success = false;
    int normalized_width = 0;
    int normalized_height = 0;
    int rows = 0;
    int cols = 0;
    int active_cell_count = 0;
    std::vector<GridCellFeature> cells;
    std::string summary;
};

struct GridHierarchyNode
{
    int node_id = -1;
    int parent_node_id = -1;
    int row = 0;
    int col = 0;
    double active_ratio = 0.0;
    std::vector<int> child_node_ids;
    std::vector<double> values;
};

struct GridHierarchyLevel
{
    int level_index = 0;
    int rows = 0;
    int cols = 0;
    std::vector<GridHierarchyNode> nodes;
};

struct GridPatternHierarchy
{
    bool success = false;
    std::vector<GridHierarchyLevel> levels;
    std::vector<double> descriptor;
    std::string summary;
};

struct GridPatternTrainingSample
{
    std::string class_id;
    cv::Mat roi_patch;
};

struct GridClassPrototype
{
    std::string class_id;
    int sample_count = 0;
    double mean_training_distance = 0.0;
    std::vector<double> centroid;
};

struct GridClassModel
{
    GridPatternConfig config;
    std::vector<GridClassPrototype> prototypes;
};

struct GridPatternTrainingReport
{
    bool success = false;
    int requested_sample_count = 0;
    int accepted_sample_count = 0;
    int rejected_sample_count = 0;
    int class_count = 0;
    std::vector<std::string> rejected_samples;
    std::string summary;
};

struct GridClassScore
{
    std::string class_id;
    double score = 0.0;
    double distance = 1.0;
};

struct GridClassResult
{
    bool success = false;
    bool rejected = true;
    std::string class_id;
    double best_score = 0.0;
    double second_score = 0.0;
    double score_margin = 0.0;
    double reject_score = 1.0;
    double elapsed_ms = 0.0;
    GridFeatureMap feature_map;
    GridPatternHierarchy hierarchy;
    std::vector<GridClassScore> top_classes;
    std::vector<std::string> anomaly_flags;
    std::string summary;
};

class GridPatternClassNet
{
public:
    GridPatternClassNet() = default;

    bool SetConfig(const GridPatternConfig& config, std::string* reason = nullptr);
    const GridPatternConfig& GetConfig() const;

    GridFeatureMap BuildFeatureMap(const cv::Mat& roi_patch) const;
    GridPatternHierarchy BuildHierarchy(const GridFeatureMap& feature_map) const;
    std::vector<double> BuildDescriptor(const cv::Mat& roi_patch) const;

    GridPatternTrainingReport Fit(
        const std::vector<GridPatternTrainingSample>& samples);
    void SetModel(const GridClassModel& model);
    const GridClassModel& GetModel() const;

    GridClassResult Infer(const cv::Mat& roi_patch) const;
    cv::Mat RenderFeatureOverlay(
        const cv::Mat& roi_patch,
        const GridFeatureMap& feature_map) const;

private:
    bool ValidateConfig(const GridPatternConfig& config, std::string& reason) const;

    GridPatternConfig config_;
    GridClassModel model_;
};

}  // namespace cxcore
