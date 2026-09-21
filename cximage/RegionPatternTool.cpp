#include "RegionPatternTool.h"

#include "Image.h"
#include "ImageAnnotationLayer.h"
#include "RectShape.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <numeric>
#include <vector>

namespace {

int ClampPermille(double value)
{
    if (!std::isfinite(value))
        return 0;
    return std::max(0, std::min(1000, static_cast<int>(std::round(value * 1000.0))));
}

}  // namespace

void RegionPatternTool::setrect(int x, int y, int width, int height)
{
    roi_x_ = x;
    roi_y_ = y;
    roi_width_ = width;
    roi_height_ = height;
}

void RegionPatternTool::setnormalized(int width, int height)
{
    config_.normalized_width = std::max(8, std::min(512, width));
    config_.normalized_height = std::max(8, std::min(512, height));
}

void RegionPatternTool::setpooling(int rows, int cols)
{
    config_.pooling_rows = std::max(1, std::min(64, rows));
    config_.pooling_cols = std::max(1, std::min(64, cols));
}

void RegionPatternTool::setbinary(int enabled)
{
    config_.use_binary = enabled != 0;
}

void RegionPatternTool::setthreshold(int threshold)
{
    config_.binarize_threshold = static_cast<double>(std::max(0, std::min(255, threshold)));
}

void RegionPatternTool::setforegrounddark(int enabled)
{
    config_.foreground_is_dark = enabled != 0;
}

void RegionPatternTool::setmaxoverlays(int max_overlays)
{
    max_overlays_ = std::max(1, std::min(512, max_overlays));
}

void RegionPatternTool::analyze(void* image)
{
    const auto started = std::chrono::steady_clock::now();
    descriptor_ = cxcore::RegionPatternDescriptor();
    status_code_ = 0;
    mean_permille_ = 0;
    std_permille_ = 0;
    overlay_count_ = 0;
    overlay_truncated_ = false;
    summary_ = "analysis_not_started";

    Image* input = static_cast<Image*>(image);
    if (input == nullptr || input->getmat().empty())
    {
        summary_ = "input_image_unavailable";
        return;
    }

    const cv::Mat& source = input->getmat();
    const int x0 = std::max(0, std::min(source.cols, roi_x_));
    const int y0 = std::max(0, std::min(source.rows, roi_y_));
    const int x1 = std::max(x0, std::min(source.cols, roi_x_ + roi_width_));
    const int y1 = std::max(y0, std::min(source.rows, roi_y_ + roi_height_));
    if (x1 <= x0 || y1 <= y0)
    {
        summary_ = "roi_outside_image_or_empty";
        return;
    }

    roi_x_ = x0;
    roi_y_ = y0;
    roi_width_ = x1 - x0;
    roi_height_ = y1 - y0;

    cxcore::RegionPatternNet network;
    network.SetConfig(config_);
    descriptor_ = network.BuildDescriptor(
        source(cv::Rect(roi_x_, roi_y_, roi_width_, roi_height_)));

    if (descriptor_.values.empty())
    {
        summary_ = "descriptor_empty";
        return;
    }

    const double mean = std::accumulate(
        descriptor_.values.begin(), descriptor_.values.end(), 0.0) /
        static_cast<double>(descriptor_.values.size());
    double variance = 0.0;
    for (double value : descriptor_.values)
    {
        const double diff = value - mean;
        variance += diff * diff;
    }
    variance /= static_cast<double>(descriptor_.values.size());

    status_code_ = 1;
    mean_permille_ = ClampPermille(mean);
    std_permille_ = ClampPermille(std::sqrt(variance));
    overlay_count_ = std::min(static_cast<int>(descriptor_.values.size()), max_overlays_);
    overlay_truncated_ = descriptor_.values.size() > static_cast<size_t>(max_overlays_);
    elapsed_ms_ = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started).count();

    summary_ = "region_descriptor_available descriptor_dim=" +
        std::to_string(descriptor_.values.size()) +
        " foreground_permille=" + std::to_string(getforegroundpermille()) +
        " pooling=" + std::to_string(config_.pooling_rows) +
        "x" + std::to_string(config_.pooling_cols) +
        " classifier=model_not_bound";
}

int RegionPatternTool::getstatuscode() { return status_code_; }
int RegionPatternTool::getdescriptordim() { return static_cast<int>(descriptor_.values.size()); }
int RegionPatternTool::getforegroundpermille() { return ClampPermille(descriptor_.global_foreground_ratio); }
int RegionPatternTool::getmeanpermille() { return mean_permille_; }
int RegionPatternTool::getstdpermille() { return std_permille_; }
int RegionPatternTool::getpoolingrows() { return config_.pooling_rows; }
int RegionPatternTool::getpoolingcols() { return config_.pooling_cols; }
int RegionPatternTool::getoverlaycount() { return overlay_count_; }
int RegionPatternTool::getoverlaytruncated() { return overlay_truncated_ ? 1 : 0; }
double RegionPatternTool::getelapsedms() { return elapsed_ms_; }
const char* RegionPatternTool::getsummary() { return summary_.c_str(); }

void RegionPatternTool::PublishDisplayShapes(
    ICxShapeSink& sink,
    const std::string& owner_ref) const
{
    if (roi_width_ <= 0 || roi_height_ <= 0)
        return;

    auto roi_shape = std::make_unique<RectShape>();
    roi_shape->setRect(
        roi_x_, roi_y_, roi_x_ + roi_width_, roi_y_ + roi_height_);
    sink.UpsertShape(
        owner_ref + ".region_analysis_roi",
        "RegionPatternTool",
        owner_ref,
        "region_analysis_roi",
        "region_analysis_roi",
        true,
        false,
        std::move(roi_shape));

    if (descriptor_.values.empty() ||
        config_.pooling_rows <= 0 ||
        config_.pooling_cols <= 0)
    {
        return;
    }

    std::vector<int> indices(descriptor_.values.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::stable_sort(
        indices.begin(), indices.end(),
        [this](int lhs, int rhs) {
            return descriptor_.values[static_cast<size_t>(lhs)] >
                   descriptor_.values[static_cast<size_t>(rhs)];
        });
    if (indices.size() > static_cast<size_t>(max_overlays_))
        indices.resize(static_cast<size_t>(max_overlays_));

    const double cell_width =
        static_cast<double>(roi_width_) / config_.pooling_cols;
    const double cell_height =
        static_cast<double>(roi_height_) / config_.pooling_rows;
    for (int index : indices)
    {
        const int row = index / config_.pooling_cols;
        const int col = index % config_.pooling_cols;
        const double x0 = roi_x_ + col * cell_width;
        const double y0 = roi_y_ + row * cell_height;
        const double x1 = roi_x_ + (col + 1) * cell_width;
        const double y1 = roi_y_ + (row + 1) * cell_height;

        auto block_shape = std::make_unique<RectShape>();
        block_shape->setRect(x0, y0, x1, y1);
        sink.UpsertShape(
            owner_ref + ".pooled_region_block." + std::to_string(index),
            "RegionPatternTool",
            owner_ref,
            "",
            "pooled_region_block",
            false,
            true,
            std::move(block_shape));
    }
}
