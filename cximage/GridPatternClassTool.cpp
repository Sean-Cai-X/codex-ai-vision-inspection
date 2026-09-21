#include "GridPatternClassTool.h"

#include "Image.h"
#include "ImageAnnotationLayer.h"
#include "PolylineShape.h"
#include "RectShape.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <numeric>

namespace {

double CellEvidence(const cxcore::GridCellFeature& cell)
{
    return cell.foreground_ratio + cell.edge_density;
}

}  // namespace

void GridPatternClassTool::setrect(int x, int y, int width, int height)
{
    roi_x_ = x;
    roi_y_ = y;
    roi_width_ = width;
    roi_height_ = height;
}

void GridPatternClassTool::setnormalized(int width, int height)
{
    config_.normalized_width = width;
    config_.normalized_height = height;
}

void GridPatternClassTool::setgrid(int rows, int cols)
{
    config_.grid_rows = rows;
    config_.grid_cols = cols;
    RebuildHierarchy();
}

void GridPatternClassTool::setlevels(int levels)
{
    requested_levels_ = std::max(3, std::min(5, levels));
    RebuildHierarchy();
}

void GridPatternClassTool::setorientationbins(int bins)
{
    config_.orientation_bins = bins;
}

void GridPatternClassTool::setforegroundthreshold(int threshold)
{
    config_.foreground_threshold = static_cast<double>(threshold);
}

void GridPatternClassTool::setforegrounddark(int enabled)
{
    config_.foreground_is_dark = enabled != 0;
}

void GridPatternClassTool::setequalizecontrast(int enabled)
{
    config_.equalize_contrast = enabled != 0;
}

void GridPatternClassTool::setactiveforegroundpercent(int percent)
{
    config_.active_foreground_ratio =
        static_cast<double>(std::max(0, std::min(100, percent))) / 100.0;
}

void GridPatternClassTool::setactiveedgepercent(int percent)
{
    config_.active_edge_ratio =
        static_cast<double>(std::max(0, std::min(100, percent))) / 100.0;
}

void GridPatternClassTool::setmaxoverlays(int max_overlays)
{
    max_overlays_ = std::max(1, std::min(512, max_overlays));
}

void GridPatternClassTool::setfusionmode(int fusion_mode)
{
    fusion_mode_ = std::max(0, std::min(3, fusion_mode));
}

void GridPatternClassTool::analyze(void* image)
{
    const auto started = std::chrono::steady_clock::now();
    feature_map_ = cxcore::GridFeatureMap();
    hierarchy_ = cxcore::GridPatternHierarchy();
    status_code_ = 0;
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
    RebuildHierarchy();

    cxcore::GridPatternClassNet network;
    std::string config_reason;
    if (!network.SetConfig(config_, &config_reason))
    {
        summary_ = "invalid_grid_config: " + config_reason;
        return;
    }

    const cv::Mat roi = source(cv::Rect(roi_x_, roi_y_, roi_width_, roi_height_));
    feature_map_ = network.BuildFeatureMap(roi);
    if (!feature_map_.success)
    {
        summary_ = "feature_map_failed: " + feature_map_.summary;
        return;
    }

    hierarchy_ = network.BuildHierarchy(feature_map_);
    if (!hierarchy_.success)
    {
        summary_ = "hierarchy_failed: " + hierarchy_.summary;
        return;
    }

    status_code_ = 1;
    overlay_count_ = std::min(feature_map_.active_cell_count, max_overlays_);
    overlay_truncated_ = feature_map_.active_cell_count > max_overlays_;
    summary_ = "grid_feature_available active_cells=" +
        std::to_string(feature_map_.active_cell_count) +
        " descriptor_dim=" + std::to_string(hierarchy_.descriptor.size()) +
        " levels=" + std::to_string(hierarchy_.levels.size()) +
        " fusion_mode=" + std::to_string(fusion_mode_) +
        " classification=model_not_bound";

    elapsed_ms_ = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started).count();
}

int GridPatternClassTool::getstatuscode() { return status_code_; }
int GridPatternClassTool::getactivecellcount() { return feature_map_.active_cell_count; }
int GridPatternClassTool::getdescriptordim() { return static_cast<int>(hierarchy_.descriptor.size()); }
int GridPatternClassTool::getlevelcount() { return static_cast<int>(hierarchy_.levels.size()); }
int GridPatternClassTool::getoverlaycount() { return overlay_count_; }
int GridPatternClassTool::getoverlaytruncated() { return overlay_truncated_ ? 1 : 0; }
double GridPatternClassTool::getelapsedms() { return elapsed_ms_; }
const char* GridPatternClassTool::getsummary() { return summary_.c_str(); }

void GridPatternClassTool::PublishDisplayShapes(
    ICxShapeSink& sink,
    const std::string& owner_ref) const
{
    if (roi_width_ <= 0 || roi_height_ <= 0)
        return;

    auto roi_shape = std::make_unique<RectShape>();
    roi_shape->setRect(
        roi_x_, roi_y_, roi_x_ + roi_width_, roi_y_ + roi_height_);
    sink.UpsertShape(
        owner_ref + ".analysis_roi",
        "GridPatternClassTool",
        owner_ref,
        "analysis_roi",
        "analysis_roi",
        true,
        false,
        std::move(roi_shape));

    if (!feature_map_.success || feature_map_.rows <= 0 || feature_map_.cols <= 0)
        return;

    std::vector<const cxcore::GridCellFeature*> active_cells;
    for (const cxcore::GridCellFeature& cell : feature_map_.cells)
    {
        if (cell.active)
            active_cells.push_back(&cell);
    }
    std::stable_sort(
        active_cells.begin(), active_cells.end(),
        [](const auto* lhs, const auto* rhs) {
            return CellEvidence(*lhs) > CellEvidence(*rhs);
        });
    if (active_cells.size() > static_cast<size_t>(max_overlays_))
        active_cells.resize(static_cast<size_t>(max_overlays_));

    const double cell_width =
        static_cast<double>(roi_width_) / feature_map_.cols;
    const double cell_height =
        static_cast<double>(roi_height_) / feature_map_.rows;
    for (const cxcore::GridCellFeature* cell : active_cells)
    {
        const double x0 = roi_x_ + cell->col * cell_width;
        const double y0 = roi_y_ + cell->row * cell_height;
        const double x1 = roi_x_ + (cell->col + 1) * cell_width;
        const double y1 = roi_y_ + (cell->row + 1) * cell_height;
        const std::string cell_ref = owner_ref + ".active_cell." +
            std::to_string(cell->cell_id);

        auto cell_shape = std::make_unique<RectShape>();
        cell_shape->setRect(x0, y0, x1, y1);
        sink.UpsertShape(
            cell_ref,
            "GridPatternClassTool",
            owner_ref,
            "",
            "active_grid_cell",
            false,
            true,
            std::move(cell_shape));

        if (cell->dominant_orientation_degrees >= 0.0)
        {
            const double angle = cell->dominant_orientation_degrees *
                3.14159265358979323846 / 180.0;
            const double cx = (x0 + x1) * 0.5;
            const double cy = (y0 + y1) * 0.5;
            const double half = std::max(1.0, std::min(x1 - x0, y1 - y0) * 0.35);
            auto direction = std::make_unique<PolylineShape>();
            direction->addPoint(cx - std::cos(angle) * half,
                                cy - std::sin(angle) * half);
            direction->addPoint(cx + std::cos(angle) * half,
                                cy + std::sin(angle) * half);
            sink.UpsertShape(
                cell_ref + ".orientation",
                "GridPatternClassTool",
                owner_ref,
                "",
                "cell_orientation",
                false,
                true,
                std::move(direction));
        }
    }
}

void GridPatternClassTool::RebuildHierarchy()
{
    config_.hierarchy.clear();
    int rows = std::max(1, config_.grid_rows);
    int cols = std::max(1, config_.grid_cols);
    for (int level = 0; level < requested_levels_; ++level)
    {
        config_.hierarchy.push_back({rows, cols});
        rows = std::max(1, (rows + 1) / 2);
        cols = std::max(1, (cols + 1) / 2);
    }
}
