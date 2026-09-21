#include "GridPatternClassNet.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <utility>

#include <opencv2/imgproc.hpp>

namespace cxcore {
namespace {

constexpr double kEpsilon = 1.0e-12;

double ClampUnit(double value)
{
    return std::max(0.0, std::min(1.0, value));
}

cv::Mat NormalizeGray(const cv::Mat& input, const GridPatternConfig& config)
{
    if (input.empty())
    {
        return cv::Mat();
    }

    cv::Mat gray;
    if (input.channels() == 1)
    {
        gray = input;
    }
    else
    {
        cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);
    }

    if (gray.depth() != CV_8U)
    {
        cv::Mat converted;
        cv::normalize(gray, converted, 0.0, 255.0, cv::NORM_MINMAX, CV_8U);
        gray = converted;
    }

    cv::Mat normalized;
    cv::resize(
        gray,
        normalized,
        cv::Size(config.normalized_width, config.normalized_height),
        0.0,
        0.0,
        cv::INTER_LINEAR);

    if (config.equalize_contrast)
    {
        cv::Mat equalized;
        cv::equalizeHist(normalized, equalized);
        return equalized;
    }
    return normalized;
}

cv::Mat BuildForegroundMask(const cv::Mat& gray, const GridPatternConfig& config)
{
    cv::Mat mask;
    const int polarity = config.foreground_is_dark
        ? cv::THRESH_BINARY_INV
        : cv::THRESH_BINARY;
    if (config.foreground_threshold < 0.0)
    {
        cv::threshold(gray, mask, 0.0, 255.0, polarity | cv::THRESH_OTSU);
    }
    else
    {
        cv::threshold(gray, mask, config.foreground_threshold, 255.0, polarity);
    }
    return mask;
}

cv::Rect CellRect(int row, int col, int rows, int cols, int width, int height)
{
    const int x0 = col * width / cols;
    const int x1 = (col + 1) * width / cols;
    const int y0 = row * height / rows;
    const int y1 = (row + 1) * height / rows;
    return cv::Rect(x0, y0, std::max(1, x1 - x0), std::max(1, y1 - y0));
}

double RootMeanSquareDistance(
    const std::vector<double>& lhs,
    const std::vector<double>& rhs)
{
    if (lhs.empty() || lhs.size() != rhs.size())
    {
        return 1.0;
    }

    double squared_sum = 0.0;
    for (size_t i = 0; i < lhs.size(); ++i)
    {
        const double delta = lhs[i] - rhs[i];
        squared_sum += delta * delta;
    }
    return std::sqrt(squared_sum / static_cast<double>(lhs.size()));
}

std::vector<double> MeanVector(const std::vector<std::vector<double>>& values)
{
    if (values.empty())
    {
        return {};
    }

    std::vector<double> mean(values.front().size(), 0.0);
    for (const auto& value : values)
    {
        if (value.size() != mean.size())
        {
            return {};
        }
        for (size_t i = 0; i < value.size(); ++i)
        {
            mean[i] += value[i];
        }
    }
    const double divisor = static_cast<double>(values.size());
    for (double& value : mean)
    {
        value /= divisor;
    }
    return mean;
}

GridHierarchyLevel MakeFirstLevel(const GridFeatureMap& feature_map)
{
    GridHierarchyLevel level;
    level.level_index = 0;
    level.rows = feature_map.rows;
    level.cols = feature_map.cols;
    level.nodes.reserve(feature_map.cells.size());
    for (const GridCellFeature& cell : feature_map.cells)
    {
        GridHierarchyNode node;
        node.node_id = cell.cell_id;
        node.row = cell.row;
        node.col = cell.col;
        node.active_ratio = cell.active ? 1.0 : 0.0;
        node.values = cell.values;
        level.nodes.push_back(std::move(node));
    }
    return level;
}

GridHierarchyLevel PoolLevel(
    const GridHierarchyLevel& source,
    int target_rows,
    int target_cols,
    int level_index)
{
    GridHierarchyLevel target;
    target.level_index = level_index;
    target.rows = target_rows;
    target.cols = target_cols;
    target.nodes.resize(static_cast<size_t>(target_rows * target_cols));

    const size_t channel_count = source.nodes.empty() ? 0u : source.nodes.front().values.size();
    std::vector<int> counts(target.nodes.size(), 0);

    for (int row = 0; row < target_rows; ++row)
    {
        for (int col = 0; col < target_cols; ++col)
        {
            GridHierarchyNode& node = target.nodes[static_cast<size_t>(row * target_cols + col)];
            node.node_id = row * target_cols + col;
            node.row = row;
            node.col = col;
            node.values.assign(channel_count, 0.0);
        }
    }

    for (const GridHierarchyNode& source_node : source.nodes)
    {
        const int target_row = std::min(
            target_rows - 1,
            source_node.row * target_rows / source.rows);
        const int target_col = std::min(
            target_cols - 1,
            source_node.col * target_cols / source.cols);
        GridHierarchyNode& target_node =
            target.nodes[static_cast<size_t>(target_row * target_cols + target_col)];
        target_node.child_node_ids.push_back(source_node.node_id);
        target_node.active_ratio += source_node.active_ratio;
        for (size_t channel = 0; channel < channel_count; ++channel)
        {
            target_node.values[channel] += source_node.values[channel];
        }
        ++counts[static_cast<size_t>(target_node.node_id)];
    }

    for (GridHierarchyNode& node : target.nodes)
    {
        const int count = counts[static_cast<size_t>(node.node_id)];
        if (count <= 0)
        {
            continue;
        }
        node.active_ratio /= static_cast<double>(count);
        for (double& value : node.values)
        {
            value /= static_cast<double>(count);
        }
    }
    return target;
}

void LinkParentNodes(GridHierarchyLevel& child, const GridHierarchyLevel& parent)
{
    for (GridHierarchyNode& node : child.nodes)
    {
        const int parent_row = std::min(
            parent.rows - 1,
            node.row * parent.rows / child.rows);
        const int parent_col = std::min(
            parent.cols - 1,
            node.col * parent.cols / child.cols);
        node.parent_node_id = parent_row * parent.cols + parent_col;
    }
}

}  // namespace

bool GridPatternClassNet::SetConfig(
    const GridPatternConfig& config,
    std::string* reason)
{
    std::string local_reason;
    if (!ValidateConfig(config, local_reason))
    {
        if (reason != nullptr)
        {
            *reason = local_reason;
        }
        return false;
    }
    config_ = config;
    model_.config = config;
    model_.prototypes.clear();
    if (reason != nullptr)
    {
        reason->clear();
    }
    return true;
}

const GridPatternConfig& GridPatternClassNet::GetConfig() const
{
    return config_;
}

bool GridPatternClassNet::ValidateConfig(
    const GridPatternConfig& config,
    std::string& reason) const
{
    if (config.normalized_width <= 0 || config.normalized_height <= 0)
    {
        reason = "normalized size must be positive";
        return false;
    }
    if (config.grid_rows <= 0 || config.grid_cols <= 0)
    {
        reason = "grid shape must be positive";
        return false;
    }
    if (config.normalized_width < config.grid_cols ||
        config.normalized_height < config.grid_rows)
    {
        reason = "normalized image must contain at least one pixel per grid cell";
        return false;
    }
    if (config.orientation_bins < 2 || config.orientation_bins > 36)
    {
        reason = "orientation_bins must be in [2,36]";
        return false;
    }
    if (config.hierarchy.size() < 3 || config.hierarchy.size() > 5)
    {
        reason = "hierarchy must contain 3 to 5 levels";
        return false;
    }
    if (config.hierarchy.front().rows != config.grid_rows ||
        config.hierarchy.front().cols != config.grid_cols)
    {
        reason = "hierarchy level 0 must match the feature grid";
        return false;
    }
    int previous_rows = config.grid_rows;
    int previous_cols = config.grid_cols;
    for (const GridPatternLevelShape& level : config.hierarchy)
    {
        if (level.rows <= 0 || level.cols <= 0 ||
            level.rows > previous_rows || level.cols > previous_cols)
        {
            reason = "hierarchy shapes must be positive and non-increasing";
            return false;
        }
        previous_rows = level.rows;
        previous_cols = level.cols;
    }
    if (config.edge_threshold < 0.0 || config.edge_threshold > 1.0 ||
        config.active_foreground_ratio < 0.0 || config.active_foreground_ratio > 1.0 ||
        config.active_edge_ratio < 0.0 || config.active_edge_ratio > 1.0)
    {
        reason = "feature thresholds must be in [0,1]";
        return false;
    }
    if (config.foreground_threshold > 255.0)
    {
        reason = "foreground_threshold must be negative for Otsu or at most 255";
        return false;
    }
    if (config.distance_scale <= 0.0 || config.top_k <= 0)
    {
        reason = "distance_scale and top_k must be positive";
        return false;
    }
    return true;
}

GridFeatureMap GridPatternClassNet::BuildFeatureMap(const cv::Mat& roi_patch) const
{
    GridFeatureMap output;
    output.normalized_width = config_.normalized_width;
    output.normalized_height = config_.normalized_height;
    output.rows = config_.grid_rows;
    output.cols = config_.grid_cols;

    std::string reason;
    if (!ValidateConfig(config_, reason))
    {
        output.summary = "invalid config: " + reason;
        return output;
    }

    const cv::Mat gray = NormalizeGray(roi_patch, config_);
    if (gray.empty())
    {
        output.summary = "empty ROI patch";
        return output;
    }
    const cv::Mat foreground = BuildForegroundMask(gray, config_);

    cv::Mat gradient_x;
    cv::Mat gradient_y;
    cv::Mat magnitude;
    cv::Mat angle;
    cv::Sobel(gray, gradient_x, CV_32F, 1, 0, 3);
    cv::Sobel(gray, gradient_y, CV_32F, 0, 1, 3);
    cv::cartToPolar(gradient_x, gradient_y, magnitude, angle, true);

    output.cells.reserve(static_cast<size_t>(config_.grid_rows * config_.grid_cols));
    for (int row = 0; row < config_.grid_rows; ++row)
    {
        for (int col = 0; col < config_.grid_cols; ++col)
        {
            const cv::Rect rect = CellRect(
                row,
                col,
                config_.grid_rows,
                config_.grid_cols,
                gray.cols,
                gray.rows);
            const cv::Mat gray_cell = gray(rect);
            const cv::Mat foreground_cell = foreground(rect);
            const cv::Mat magnitude_cell = magnitude(rect);
            const cv::Mat angle_cell = angle(rect);

            GridCellFeature cell;
            cell.cell_id = row * config_.grid_cols + col;
            cell.row = row;
            cell.col = col;
            cell.x = rect.x;
            cell.y = rect.y;
            cell.width = rect.width;
            cell.height = rect.height;
            cell.valid = true;
            cell.foreground_ratio = static_cast<double>(cv::countNonZero(foreground_cell)) /
                static_cast<double>(rect.area());

            cv::Scalar gray_mean;
            cv::Scalar gray_stddev;
            cv::meanStdDev(gray_cell, gray_mean, gray_stddev);
            cell.gray_mean = gray_mean[0] / 255.0;
            cell.gray_stddev = ClampUnit(gray_stddev[0] / 128.0);

            cell.orientation_histogram.assign(
                static_cast<size_t>(config_.orientation_bins),
                0.0);
            int edge_count = 0;
            double orientation_weight = 0.0;
            for (int y = 0; y < rect.height; ++y)
            {
                for (int x = 0; x < rect.width; ++x)
                {
                    const double normalized_magnitude =
                        ClampUnit(static_cast<double>(magnitude_cell.at<float>(y, x)) / 1020.0);
                    if (normalized_magnitude < config_.edge_threshold)
                    {
                        continue;
                    }
                    ++edge_count;
                    double orientation = std::fmod(
                        static_cast<double>(angle_cell.at<float>(y, x)),
                        180.0);
                    if (orientation < 0.0)
                    {
                        orientation += 180.0;
                    }
                    const int bin = std::min(
                        config_.orientation_bins - 1,
                        static_cast<int>(orientation * config_.orientation_bins / 180.0));
                    cell.orientation_histogram[static_cast<size_t>(bin)] += normalized_magnitude;
                    orientation_weight += normalized_magnitude;
                }
            }
            cell.edge_density = static_cast<double>(edge_count) /
                static_cast<double>(rect.area());
            if (orientation_weight > kEpsilon)
            {
                for (double& value : cell.orientation_histogram)
                {
                    value /= orientation_weight;
                }
                const auto dominant = std::max_element(
                    cell.orientation_histogram.begin(),
                    cell.orientation_histogram.end());
                const int dominant_bin = static_cast<int>(
                    std::distance(cell.orientation_histogram.begin(), dominant));
                cell.dominant_orientation_degrees =
                    (static_cast<double>(dominant_bin) + 0.5) * 180.0 /
                    static_cast<double>(config_.orientation_bins);
            }
            cell.active =
                cell.foreground_ratio >= config_.active_foreground_ratio ||
                cell.edge_density >= config_.active_edge_ratio;
            if (cell.active)
            {
                ++output.active_cell_count;
            }

            cell.values.reserve(static_cast<size_t>(5 + config_.orientation_bins));
            cell.values.push_back(cell.foreground_ratio);
            cell.values.push_back(cell.gray_mean);
            cell.values.push_back(cell.gray_stddev);
            cell.values.push_back(cell.edge_density);
            cell.values.push_back(cell.active ? 1.0 : 0.0);
            cell.values.insert(
                cell.values.end(),
                cell.orientation_histogram.begin(),
                cell.orientation_histogram.end());
            output.cells.push_back(std::move(cell));
        }
    }

    output.success = true;
    output.summary = "deterministic grid feature map built";
    return output;
}

GridPatternHierarchy GridPatternClassNet::BuildHierarchy(
    const GridFeatureMap& feature_map) const
{
    GridPatternHierarchy output;
    if (!feature_map.success || feature_map.cells.empty())
    {
        output.summary = "feature map is unavailable";
        return output;
    }
    if (feature_map.rows != config_.grid_rows || feature_map.cols != config_.grid_cols)
    {
        output.summary = "feature map shape does not match config";
        return output;
    }

    output.levels.reserve(config_.hierarchy.size());
    output.levels.push_back(MakeFirstLevel(feature_map));
    for (size_t level_index = 1; level_index < config_.hierarchy.size(); ++level_index)
    {
        const GridPatternLevelShape& shape = config_.hierarchy[level_index];
        output.levels.push_back(PoolLevel(
            output.levels.back(),
            shape.rows,
            shape.cols,
            static_cast<int>(level_index)));
        LinkParentNodes(
            output.levels[output.levels.size() - 2],
            output.levels.back());
    }

    for (const GridHierarchyLevel& level : output.levels)
    {
        for (const GridHierarchyNode& node : level.nodes)
        {
            output.descriptor.insert(
                output.descriptor.end(),
                node.values.begin(),
                node.values.end());
        }
    }
    output.success = !output.descriptor.empty();
    output.summary = output.success
        ? "3-5 level deterministic hierarchy built"
        : "hierarchy descriptor is empty";
    return output;
}

std::vector<double> GridPatternClassNet::BuildDescriptor(
    const cv::Mat& roi_patch) const
{
    return BuildHierarchy(BuildFeatureMap(roi_patch)).descriptor;
}

GridPatternTrainingReport GridPatternClassNet::Fit(
    const std::vector<GridPatternTrainingSample>& samples)
{
    GridPatternTrainingReport report;
    report.requested_sample_count = static_cast<int>(samples.size());

    std::map<std::string, std::vector<std::vector<double>>> grouped;
    for (size_t index = 0; index < samples.size(); ++index)
    {
        const GridPatternTrainingSample& sample = samples[index];
        const std::vector<double> descriptor = BuildDescriptor(sample.roi_patch);
        if (sample.class_id.empty() || descriptor.empty())
        {
            ++report.rejected_sample_count;
            report.rejected_samples.push_back(
                "sample[" + std::to_string(index) + "]: empty class or descriptor");
            continue;
        }
        grouped[sample.class_id].push_back(descriptor);
        ++report.accepted_sample_count;
    }

    GridClassModel next_model;
    next_model.config = config_;
    for (const auto& entry : grouped)
    {
        GridClassPrototype prototype;
        prototype.class_id = entry.first;
        prototype.sample_count = static_cast<int>(entry.second.size());
        prototype.centroid = MeanVector(entry.second);
        if (prototype.centroid.empty())
        {
            continue;
        }
        double distance_sum = 0.0;
        for (const std::vector<double>& descriptor : entry.second)
        {
            distance_sum += RootMeanSquareDistance(descriptor, prototype.centroid);
        }
        prototype.mean_training_distance =
            distance_sum / static_cast<double>(entry.second.size());
        next_model.prototypes.push_back(std::move(prototype));
    }

    model_ = std::move(next_model);
    report.class_count = static_cast<int>(model_.prototypes.size());
    report.success = report.class_count > 0;
    report.summary = report.success
        ? "deterministic class prototypes fitted"
        : "no valid class prototype was fitted";
    return report;
}

void GridPatternClassNet::SetModel(const GridClassModel& model)
{
    config_ = model.config;
    model_ = model;
}

const GridClassModel& GridPatternClassNet::GetModel() const
{
    return model_;
}

GridClassResult GridPatternClassNet::Infer(const cv::Mat& roi_patch) const
{
    const auto start = std::chrono::steady_clock::now();
    GridClassResult output;
    output.feature_map = BuildFeatureMap(roi_patch);
    output.hierarchy = BuildHierarchy(output.feature_map);

    if (!output.hierarchy.success)
    {
        output.anomaly_flags.push_back("grid_hierarchy_unavailable");
        output.summary = output.hierarchy.summary;
    }
    else if (model_.prototypes.empty())
    {
        output.anomaly_flags.push_back("class_model_unavailable");
        output.summary = "class model has no prototypes";
    }
    else
    {
        for (const GridClassPrototype& prototype : model_.prototypes)
        {
            if (prototype.centroid.size() != output.hierarchy.descriptor.size())
            {
                output.anomaly_flags.push_back(
                    "prototype_shape_mismatch:" + prototype.class_id);
                continue;
            }
            GridClassScore score;
            score.class_id = prototype.class_id;
            score.distance = RootMeanSquareDistance(
                output.hierarchy.descriptor,
                prototype.centroid);
            score.score = std::exp(-score.distance / config_.distance_scale);
            output.top_classes.push_back(std::move(score));
        }

        std::sort(
            output.top_classes.begin(),
            output.top_classes.end(),
            [](const GridClassScore& lhs, const GridClassScore& rhs)
            {
                if (lhs.score != rhs.score)
                {
                    return lhs.score > rhs.score;
                }
                return lhs.class_id < rhs.class_id;
            });
        const double full_second_score = output.top_classes.size() > 1
            ? output.top_classes[1].score
            : 0.0;
        if (output.top_classes.size() > static_cast<size_t>(config_.top_k))
        {
            output.top_classes.resize(static_cast<size_t>(config_.top_k));
        }

        if (!output.top_classes.empty())
        {
            output.success = true;
            output.class_id = output.top_classes.front().class_id;
            output.best_score = output.top_classes.front().score;
            output.second_score = full_second_score;
            output.score_margin = output.best_score - output.second_score;
            output.reject_score = 1.0 - output.best_score;
            output.rejected =
                output.best_score < config_.min_class_score ||
                output.score_margin < config_.min_class_margin;
            if (output.best_score < config_.min_class_score)
            {
                output.anomaly_flags.push_back("class_score_below_threshold");
            }
            if (output.score_margin < config_.min_class_margin)
            {
                output.anomaly_flags.push_back("class_margin_ambiguous");
            }
            output.summary = output.rejected
                ? "classification completed with reject signal"
                : "classification completed";
        }
        else
        {
            output.summary = "no compatible class prototype";
        }
    }

    const auto end = std::chrono::steady_clock::now();
    output.elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    return output;
}

cv::Mat GridPatternClassNet::RenderFeatureOverlay(
    const cv::Mat& roi_patch,
    const GridFeatureMap& feature_map) const
{
    if (!feature_map.success || roi_patch.empty())
    {
        return cv::Mat();
    }

    cv::Mat gray = NormalizeGray(roi_patch, config_);
    cv::Mat overlay;
    cv::cvtColor(gray, overlay, cv::COLOR_GRAY2BGR);

    for (const GridCellFeature& cell : feature_map.cells)
    {
        const cv::Rect rect(cell.x, cell.y, cell.width, cell.height);
        cv::rectangle(
            overlay,
            rect,
            cell.active ? cv::Scalar(255, 128, 0) : cv::Scalar(96, 96, 96),
            1,
            cv::LINE_8);
        if (!cell.active || cell.dominant_orientation_degrees < 0.0)
        {
            continue;
        }
        const double radians = cell.dominant_orientation_degrees * CV_PI / 180.0;
        const cv::Point center(
            rect.x + rect.width / 2,
            rect.y + rect.height / 2);
        const double half_length = 0.35 * static_cast<double>(
            std::max(1, std::min(rect.width, rect.height)));
        const cv::Point delta(
            static_cast<int>(std::lround(std::cos(radians) * half_length)),
            static_cast<int>(std::lround(std::sin(radians) * half_length)));
        cv::line(
            overlay,
            center - delta,
            center + delta,
            cv::Scalar(0, 255, 255),
            1,
            cv::LINE_AA);
    }
    return overlay;
}

}  // namespace cxcore
