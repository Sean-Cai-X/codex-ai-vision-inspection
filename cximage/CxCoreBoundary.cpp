#include "CxCoreBoundary.h"

#include "FastMatch.h"
#include "FindCircle.h"
#include "FindEllipse.h"
#include "FindLine.h"
#include "Image.h"
#include "RegionPatternNet.h"
#include "FindObject.h"
#include "shapebase.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <sstream>
#include <queue>

namespace cxcore {

namespace {

double QuietNaN()
{
    return std::numeric_limits<double>::quiet_NaN();
}

double SafeAspectRatio(double width, double height)
{
    if (height <= 0.0)
    {
        return QuietNaN();
    }
    return width / height;
}

double SafeRatio(double numerator, double denominator)
{
    if (denominator <= 0.0)
    {
        return QuietNaN();
    }
    return numerator / denominator;
}

std::string EscapeCsv(const std::string& value)
{
    const bool needs_quotes = value.find_first_of(",\"\n\r") != std::string::npos;
    if (!needs_quotes)
    {
        return value;
    }

    std::string escaped = "\"";
    for (const char ch : value)
    {
        if (ch == '"')
        {
            escaped += "\"\"";
        }
        else
        {
            escaped += ch;
        }
    }
    escaped += "\"";
    return escaped;
}

template<typename ValueType>
void AppendCsvValue(std::ostringstream& stream, const ValueType& value, bool& first)
{
    if (!first)
    {
        stream << ',';
    }
    stream << value;
    first = false;
}

void AppendCsvValue(std::ostringstream& stream, const std::string& value, bool& first)
{
    if (!first)
    {
        stream << ',';
    }
    stream << EscapeCsv(value);
    first = false;
}

std::vector<size_t> BaselineFeatureIndicesV1(BaselineFeatureSetV1 feature_set)
{
    switch (feature_set)
    {
    case BaselineFeatureSetV1::CoreGeom:
        return {
            0, 1, 2, 3, 4, 5,
            15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
            25, 26, 27, 28, 29, 30,
            31, 32, 33, 34, 35, 36, 37, 38, 39, 40
        };
    case BaselineFeatureSetV1::MatchOnly:
        return {
            41, 42, 43, 44, 45, 46, 47, 48, 49
        };
    case BaselineFeatureSetV1::AllV1:
        return {};
    }

    throw std::invalid_argument("Unsupported baseline feature set.");
}

template<typename ValueType>
std::vector<ValueType> SelectByIndices(
    const std::vector<ValueType>& values,
    const std::vector<size_t>& indices)
{
    if (indices.empty())
    {
        return values;
    }

    std::vector<ValueType> selected;
    selected.reserve(indices.size());
    for (const size_t index : indices)
    {
        if (index < values.size())
        {
            selected.push_back(values[index]);
        }
    }
    return selected;
}

cv::Rect MakeClampedRect(const Image& image)
{
    const int x = std::max(0, image.m_ix0);
    const int y = std::max(0, image.m_iy0);
    const int max_width = std::max(0, image.getWidth() - x);
    const int max_height = std::max(0, image.getHeight() - y);
    const int width = std::min(std::max(0, image.m_iw), max_width);
    const int height = std::min(std::max(0, image.m_ih), max_height);
    return cv::Rect(x, y, width, height);
}

cv::Mat MakeGrayRoi(const Image& image, const cv::Rect& roi)
{
    if (roi.width <= 0 || roi.height <= 0)
    {
        return cv::Mat();
    }

    const cv::Mat roi_image = const_cast<Image&>(image).getmat()(roi);
    cv::Mat gray;
    if (roi_image.channels() == 1)
    {
        gray = roi_image.clone();
    }
    else
    {
        cv::cvtColor(roi_image, gray, cv::COLOR_BGR2GRAY);
    }
    return gray;
}

void FillGrayStatistics(const cv::Mat& gray, BaselineFeatureSampleV1& sample)
{
    if (gray.empty())
    {
        sample.gray_mean = QuietNaN();
        sample.gray_std = QuietNaN();
        sample.gray_min = QuietNaN();
        sample.gray_max = QuietNaN();
        sample.edge_pixel_ratio = QuietNaN();
        sample.binary_foreground_ratio = QuietNaN();
        return;
    }

    cv::Scalar mean;
    cv::Scalar stddev;
    cv::meanStdDev(gray, mean, stddev);
    sample.gray_mean = mean[0];
    sample.gray_std = stddev[0];

    double min_value = 0.0;
    double max_value = 0.0;
    cv::minMaxLoc(gray, &min_value, &max_value);
    sample.gray_min = min_value;
    sample.gray_max = max_value;

    cv::Mat edges;
    cv::Canny(gray, edges, 50.0, 150.0);
    const double pixel_count = static_cast<double>(gray.rows * gray.cols);
    sample.edge_pixel_ratio = SafeRatio(static_cast<double>(cv::countNonZero(edges)), pixel_count);

    cv::Mat binary;
    cv::threshold(gray, binary, 0.0, 255.0, cv::THRESH_BINARY | cv::THRESH_OTSU);
    sample.binary_foreground_ratio = SafeRatio(static_cast<double>(cv::countNonZero(binary)), pixel_count);
}

void FillRegionPatternFields(const cv::Mat& gray, BaselineFeatureSampleV1& sample)
{
    if (gray.empty())
    {
        sample.region_pattern_foreground_ratio = QuietNaN();
        sample.region_pattern_descriptor_dim = 0.0;
        sample.region_pattern_descriptor_mean = QuietNaN();
        sample.region_pattern_descriptor_std = QuietNaN();
        return;
    }

    RegionPatternNet net;
    const RegionPatternDescriptor descriptor = net.BuildDescriptor(gray);
    sample.region_pattern_foreground_ratio = descriptor.global_foreground_ratio;
    sample.region_pattern_descriptor_dim = static_cast<double>(descriptor.values.size());

    if (descriptor.values.empty())
    {
        sample.region_pattern_descriptor_mean = QuietNaN();
        sample.region_pattern_descriptor_std = QuietNaN();
        return;
    }

    double sum = 0.0;
    for (const double value : descriptor.values)
    {
        sum += value;
    }
    const double mean = sum / static_cast<double>(descriptor.values.size());
    double variance_sum = 0.0;
    for (const double value : descriptor.values)
    {
        const double diff = value - mean;
        variance_sum += diff * diff;
    }

    sample.region_pattern_descriptor_mean = mean;
    sample.region_pattern_descriptor_std =
        std::sqrt(variance_sum / static_cast<double>(descriptor.values.size()));
}

void FillLargestComponent(const ImageAnalysisOutput& analysis, BaselineFeatureSampleV1& sample)
{
    sample.component_count = static_cast<double>(analysis.connected_components.size());
    if (analysis.connected_components.empty())
    {
        sample.largest_component_area = 0.0;
        sample.largest_component_ratio = 0.0;
        sample.largest_bbox_x = QuietNaN();
        sample.largest_bbox_y = QuietNaN();
        sample.largest_bbox_w = QuietNaN();
        sample.largest_bbox_h = QuietNaN();
        sample.largest_bbox_aspect_ratio = QuietNaN();
        sample.largest_centroid_x = QuietNaN();
        sample.largest_centroid_y = QuietNaN();
        return;
    }

    const auto largest = std::max_element(
        analysis.connected_components.begin(),
        analysis.connected_components.end(),
        [](const ImageComponentOutput& lhs, const ImageComponentOutput& rhs)
        {
            return lhs.area < rhs.area;
        });

    sample.largest_component_area = static_cast<double>(largest->area);
    sample.largest_component_ratio = SafeRatio(sample.largest_component_area, sample.roi_area);
    sample.largest_bbox_x = largest->bounds.x;
    sample.largest_bbox_y = largest->bounds.y;
    sample.largest_bbox_w = largest->bounds.width;
    sample.largest_bbox_h = largest->bounds.height;
    sample.largest_bbox_aspect_ratio = SafeAspectRatio(largest->bounds.width, largest->bounds.height);
    sample.largest_centroid_x = largest->centroid.x;
    sample.largest_centroid_y = largest->centroid.y;
}

void FillLineFields(const LineMeasurementOutput& line, BaselineFeatureSampleV1& sample)
{
    sample.line_w_points_count = static_cast<double>(line.horizontal_samples.points.size());
    sample.line_h_points_count = static_cast<double>(line.vertical_samples.points.size());
    sample.line_measure_bbox_x = line.measure_bounds.x;
    sample.line_measure_bbox_y = line.measure_bounds.y;
    sample.line_measure_bbox_w = line.measure_bounds.width;
    sample.line_measure_bbox_h = line.measure_bounds.height;
}

void FillCircleFields(const CircleMeasurementOutput& circle, BaselineFeatureSampleV1& sample)
{
    sample.circle_points_count = static_cast<double>(circle.sample_points.points.size());
    sample.circle_center_x = circle.center.x;
    sample.circle_center_y = circle.center.y;
    sample.circle_radius = circle.radius;
    sample.circle_avg_dist = circle.average_distance;
    sample.circle_measure_bbox_x = circle.measure_bounds.x;
    sample.circle_measure_bbox_y = circle.measure_bounds.y;
    sample.circle_measure_bbox_w = circle.measure_bounds.width;
    sample.circle_measure_bbox_h = circle.measure_bounds.height;
    sample.circle_fit_valid = circle.has_direct_fit ? 1.0 : 0.0;
}

void FillMatchFields(const MatchOutput& match, BaselineFeatureSampleV1& sample)
{
    sample.match_candidate_count = static_cast<double>(match.candidates.size());
    sample.image_model_score = match.image_model_score;

    if (match.candidates.empty())
    {
        sample.match_best_score = 0.0;
        sample.match_best_center_x = QuietNaN();
        sample.match_best_center_y = QuietNaN();
        sample.match_best_rect_x = QuietNaN();
        sample.match_best_rect_y = QuietNaN();
        sample.match_best_rect_w = QuietNaN();
        sample.match_best_rect_h = QuietNaN();
        return;
    }

    const auto best = std::max_element(
        match.candidates.begin(),
        match.candidates.end(),
        [](const MatchCandidateOutput& lhs, const MatchCandidateOutput& rhs)
        {
            return lhs.score < rhs.score;
        });

    sample.match_best_score = best->score;
    sample.match_best_center_x = best->center.x;
    sample.match_best_center_y = best->center.y;
    sample.match_best_rect_x = best->bounds.x;
    sample.match_best_rect_y = best->bounds.y;
    sample.match_best_rect_w = best->bounds.width;
    sample.match_best_rect_h = best->bounds.height;
}

OutputRect ToOutputRect(const gp_Rectangle& rect)
{
    const gp_Pnt top_left = rect.TopLeft();
    const gp_Pnt bottom_right = rect.BottomRight();
    const double min_x = std::min(top_left.X(), bottom_right.X());
    const double min_y = std::min(top_left.Y(), bottom_right.Y());
    const double width = std::fabs(bottom_right.X() - top_left.X());
    const double height = std::fabs(bottom_right.Y() - top_left.Y());

    return OutputRect{
        min_x,
        min_y,
        width,
        height
    };
}

PointSetOutput ExportPointSetInternal(PointsShape& points)
{
    PointSetOutput output;
    const int count = points.size();
    output.points.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i)
    {
        output.points.push_back(OutputPoint{ points.getx(i), points.gety(i) });
    }

    output.bounds = ToOutputRect(points.boundingRect());
    return output;
}

} // namespace

PointSetOutput ExportPointSet(PointsShape& points)
{
    return ExportPointSetInternal(points);
}

ImageAnalysisOutput ExportImageAnalysis(const Image& image, double min_area, int min_width, int min_height)
{
    ImageAnalysisOutput output;
    output.width = image.getWidth();
    output.height = image.getHeight();
    output.image_type = image.getType();

    const std::vector<std::vector<cv::Point>> components = image.findConnectedComponents(min_area, min_width, min_height);
    const std::vector<cv::Rect> boxes = image.getBoundingBoxes(components);
    const std::vector<int> areas = image.calculateAreas(components);
    const std::vector<cv::Point> centroids = image.getCentroids(min_area, min_width, min_height);

    const size_t item_count = std::min(boxes.size(), std::min(areas.size(), centroids.size()));
    output.connected_components.reserve(item_count);
    for (size_t i = 0; i < item_count; ++i)
    {
        const cv::Rect& box = boxes[i];
        const cv::Point& centroid = centroids[i];
        output.connected_components.push_back(ImageComponentOutput{
            OutputRect{
                static_cast<double>(box.x),
                static_cast<double>(box.y),
                static_cast<double>(box.width),
                static_cast<double>(box.height)
            },
            OutputPoint{
                static_cast<double>(centroid.x),
                static_cast<double>(centroid.y)
            },
            areas[i]
        });
    }

    return output;
}

LineMeasurementOutput ExportLineMeasurement(FindLine& line)
{
    LineMeasurementOutput output;
    output.horizontal_samples = ExportPointSetInternal(line.getresultpointsw());
    output.vertical_samples = ExportPointSetInternal(line.getresultpointsh());
    output.measure_bounds = ToOutputRect(line.measurepointsboundingrect());
    return output;
}

CircleMeasurementOutput ExportCircleMeasurement(FindCircle& circle)
{
    CircleMeasurementOutput output;
    output.sample_points = ExportPointSetInternal(circle.getresultpoints());
    output.measure_bounds = ToOutputRect(circle.measurepointsboundingrect());
    output.center = OutputPoint{ circle.getresultcentx(), circle.getresultcenty() };
    output.radius = circle.getradius();
    output.average_distance = circle.getavgdist();
    output.has_direct_fit = output.radius > 0.0;
    return output;
}

EllipseMeasurementOutput ExportEllipseMeasurement(FindEllipse& ellipse)
{
    EllipseMeasurementOutput output;
    output.sample_points = ExportPointSetInternal(ellipse.getresultpoints());
    output.measure_bounds = ToOutputRect(ellipse.measurepointsboundingrect());
    return output;
}

DetectionOutput ExportDetections(FindObject& object)
{
    DetectionOutput output;
    RectsShape& rects = object.getresultrects();
    const int count = rects.size();
    output.boxes.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i)
    {
        output.boxes.push_back(ToOutputRect(rects.getrect(i)));
    }
    return output;
}

MatchOutput ExportMatchOutput(FastMatch& matcher, int max_candidates)
{
    MatchOutput output;
    output.max_score = matcher.getmaxresult();
    output.image_model_score = matcher.getimagemodelreslut();

    const int actual_count = matcher.getresultrects() ? matcher.getresultrects()->size() : 0;
    const int candidate_count = std::max(0, std::min(max_candidates, actual_count));
    output.candidates.reserve(static_cast<size_t>(candidate_count));
    for (int i = 0; i < candidate_count; ++i)
    {
        output.candidates.push_back(MatchCandidateOutput{
            ToOutputRect(matcher.getresultrect(i)),
            OutputPoint{ matcher.getresultcentx(i), matcher.getresultcenty(i) },
            matcher.getresultnum(i)
        });
    }

    return output;
}

RegionPatternDescriptorOutput ExportRegionPatternDescriptor(const RegionPatternDescriptor& descriptor)
{
    RegionPatternDescriptorOutput output;
    output.normalized_width = descriptor.normalized_width;
    output.normalized_height = descriptor.normalized_height;
    output.pooling_rows = descriptor.pooling_rows;
    output.pooling_cols = descriptor.pooling_cols;
    output.global_foreground_ratio = descriptor.global_foreground_ratio;
    output.values = descriptor.values;
    return output;
}

RegionPatternScoreOutput ExportRegionPatternScore(const RegionPatternScore& score)
{
    RegionPatternScoreOutput output;
    output.success = score.success;
    output.descriptor_distance = score.descriptor_distance;
    output.content_score = score.content_score;
    output.summary = score.summary;
    return output;
}

BaselineFeatureSampleV1 ExportBaselineFeatureSampleV1(
    const Image& image,
    const ImageAnalysisOutput& analysis,
    const LineMeasurementOutput& line,
    const CircleMeasurementOutput& circle,
    const MatchOutput& match)
{
    BaselineFeatureSampleV1 sample;
    const cv::Rect roi = MakeClampedRect(image);
    const cv::Mat gray_roi = MakeGrayRoi(image, roi);

    sample.roi_x = static_cast<double>(image.m_ix0);
    sample.roi_y = static_cast<double>(image.m_iy0);
    sample.roi_w = static_cast<double>(image.m_iw);
    sample.roi_h = static_cast<double>(image.m_ih);
    sample.roi_area = sample.roi_w * sample.roi_h;
    sample.roi_aspect_ratio = SafeAspectRatio(sample.roi_w, sample.roi_h);

    sample.image_width = static_cast<double>(analysis.width);
    sample.image_height = static_cast<double>(analysis.height);
    sample.image_type = static_cast<double>(analysis.image_type);

    FillGrayStatistics(gray_roi, sample);
    FillRegionPatternFields(gray_roi, sample);
    FillLargestComponent(analysis, sample);
    FillLineFields(line, sample);
    FillCircleFields(circle, sample);
    FillMatchFields(match, sample);

    return sample;
}

std::vector<std::string> GetBaselineMetadataNamesV1()
{
    return {
        "sample_id",
        "source_image_id",
        "roi_id",
        "label",
        "split"
    };
}

std::vector<std::string> GetBaselineFeatureNamesV1()
{
    return {
        "roi_x",
        "roi_y",
        "roi_w",
        "roi_h",
        "roi_area",
        "roi_aspect_ratio",
        "image_width",
        "image_height",
        "image_type",
        "gray_mean",
        "gray_std",
        "gray_min",
        "gray_max",
        "edge_pixel_ratio",
        "binary_foreground_ratio",
        "region_pattern_foreground_ratio",
        "region_pattern_descriptor_dim",
        "region_pattern_descriptor_mean",
        "region_pattern_descriptor_std",
        "component_count",
        "largest_component_area",
        "largest_component_ratio",
        "largest_bbox_x",
        "largest_bbox_y",
        "largest_bbox_w",
        "largest_bbox_h",
        "largest_bbox_aspect_ratio",
        "largest_centroid_x",
        "largest_centroid_y",
        "line_w_points_count",
        "line_h_points_count",
        "line_measure_bbox_x",
        "line_measure_bbox_y",
        "line_measure_bbox_w",
        "line_measure_bbox_h",
        "circle_points_count",
        "circle_center_x",
        "circle_center_y",
        "circle_radius",
        "circle_avg_dist",
        "circle_measure_bbox_x",
        "circle_measure_bbox_y",
        "circle_measure_bbox_w",
        "circle_measure_bbox_h",
        "circle_fit_valid",
        "match_candidate_count",
        "match_best_score",
        "match_best_center_x",
        "match_best_center_y",
        "match_best_rect_x",
        "match_best_rect_y",
        "match_best_rect_w",
        "match_best_rect_h",
        "image_model_score"
    };
}

std::vector<std::string> ExportBaselineMetadataValuesV1(const BaselineFeatureSampleV1& sample)
{
    return {
        sample.sample_id,
        sample.source_image_id,
        sample.roi_id,
        sample.label,
        sample.split
    };
}

std::vector<double> ExportBaselineFeatureValuesV1(const BaselineFeatureSampleV1& sample)
{
    return {
        sample.roi_x,
        sample.roi_y,
        sample.roi_w,
        sample.roi_h,
        sample.roi_area,
        sample.roi_aspect_ratio,
        sample.image_width,
        sample.image_height,
        sample.image_type,
        sample.gray_mean,
        sample.gray_std,
        sample.gray_min,
        sample.gray_max,
        sample.edge_pixel_ratio,
        sample.binary_foreground_ratio,
        sample.region_pattern_foreground_ratio,
        sample.region_pattern_descriptor_dim,
        sample.region_pattern_descriptor_mean,
        sample.region_pattern_descriptor_std,
        sample.component_count,
        sample.largest_component_area,
        sample.largest_component_ratio,
        sample.largest_bbox_x,
        sample.largest_bbox_y,
        sample.largest_bbox_w,
        sample.largest_bbox_h,
        sample.largest_bbox_aspect_ratio,
        sample.largest_centroid_x,
        sample.largest_centroid_y,
        sample.line_w_points_count,
        sample.line_h_points_count,
        sample.line_measure_bbox_x,
        sample.line_measure_bbox_y,
        sample.line_measure_bbox_w,
        sample.line_measure_bbox_h,
        sample.circle_points_count,
        sample.circle_center_x,
        sample.circle_center_y,
        sample.circle_radius,
        sample.circle_avg_dist,
        sample.circle_measure_bbox_x,
        sample.circle_measure_bbox_y,
        sample.circle_measure_bbox_w,
        sample.circle_measure_bbox_h,
        sample.circle_fit_valid,
        sample.match_candidate_count,
        sample.match_best_score,
        sample.match_best_center_x,
        sample.match_best_center_y,
        sample.match_best_rect_x,
        sample.match_best_rect_y,
        sample.match_best_rect_w,
        sample.match_best_rect_h,
        sample.image_model_score
    };
}

const char* BaselineFeatureSetNameV1(BaselineFeatureSetV1 feature_set)
{
    switch (feature_set)
    {
    case BaselineFeatureSetV1::CoreGeom:
        return "core_geom";
    case BaselineFeatureSetV1::MatchOnly:
        return "match_only";
    case BaselineFeatureSetV1::AllV1:
        return "all_v1";
    }

    return "unknown";
}

std::vector<std::string> GetBaselineFeatureNamesV1(BaselineFeatureSetV1 feature_set)
{
    return SelectByIndices(GetBaselineFeatureNamesV1(), BaselineFeatureIndicesV1(feature_set));
}

std::vector<double> ExportBaselineFeatureValuesV1(
    const BaselineFeatureSampleV1& sample,
    BaselineFeatureSetV1 feature_set)
{
    return SelectByIndices(ExportBaselineFeatureValuesV1(sample), BaselineFeatureIndicesV1(feature_set));
}

std::string ExportBaselineCsvHeaderV1()
{
    std::ostringstream header;
    bool first = true;
    for (const std::string& name : GetBaselineMetadataNamesV1())
    {
        AppendCsvValue(header, name, first);
    }
    for (const std::string& name : GetBaselineFeatureNamesV1())
    {
        AppendCsvValue(header, name, first);
    }
    return header.str();
}

std::string ExportBaselineCsvHeaderV1(BaselineFeatureSetV1 feature_set)
{
    std::ostringstream header;
    bool first = true;
    for (const std::string& name : GetBaselineMetadataNamesV1())
    {
        AppendCsvValue(header, name, first);
    }
    for (const std::string& name : GetBaselineFeatureNamesV1(feature_set))
    {
        AppendCsvValue(header, name, first);
    }
    return header.str();
}

std::string ExportBaselineCsvRowV1(const BaselineFeatureSampleV1& sample)
{
    std::ostringstream row;
    bool first = true;
    for (const std::string& value : ExportBaselineMetadataValuesV1(sample))
    {
        AppendCsvValue(row, value, first);
    }
    for (const double value : ExportBaselineFeatureValuesV1(sample))
    {
        AppendCsvValue(row, value, first);
    }
    return row.str();
}

std::string ExportBaselineCsvRowV1(
    const BaselineFeatureSampleV1& sample,
    BaselineFeatureSetV1 feature_set)
{
    std::ostringstream row;
    bool first = true;
    for (const std::string& value : ExportBaselineMetadataValuesV1(sample))
    {
        AppendCsvValue(row, value, first);
    }
    for (const double value : ExportBaselineFeatureValuesV1(sample, feature_set))
    {
        AppendCsvValue(row, value, first);
    }
    return row.str();
}


namespace {

size_t TopologyMaskIndex(int x, int y, int width)
{
    return static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x);
}

bool TopologyIsInside(int x, int y, int width, int height)
{
    return x >= 0 && y >= 0 && x < width && y < height;
}

bool TopologyIsForeground(const std::vector<unsigned char>& mask, int width, int height, int x, int y)
{
    if (!TopologyIsInside(x, y, width, height))
    {
        return false;
    }
    return mask[TopologyMaskIndex(x, y, width)] != 0;
}

std::vector<std::pair<int, int>> TopologyNeighborOffsets(bool use_eight_connected)
{
    std::vector<std::pair<int, int>> offsets = {
        { 1, 0 },
        { -1, 0 },
        { 0, 1 },
        { 0, -1 }
    };

    if (use_eight_connected)
    {
        offsets.push_back({ 1, 1 });
        offsets.push_back({ 1, -1 });
        offsets.push_back({ -1, 1 });
        offsets.push_back({ -1, -1 });
    }

    return offsets;
}

int ClassifyTopologyRegion(
    const std::vector<unsigned char>& mask,
    int image_width,
    int image_height,
    int x,
    int y,
    int width,
    int height)
{
    int foreground_count = 0;
    const int pixel_count = width * height;
    for (int row = y; row < y + height; ++row)
    {
        for (int col = x; col < x + width; ++col)
        {
            if (TopologyIsForeground(mask, image_width, image_height, col, row))
            {
                ++foreground_count;
            }
        }
    }

    if (foreground_count == 0)
    {
        return 0;
    }
    if (foreground_count == pixel_count)
    {
        return 1;
    }
    return 2;
}

void BuildFractalPartitionRecursive(
    FractalPartitionOutput& output,
    const std::vector<unsigned char>& mask,
    int image_width,
    int image_height,
    int x,
    int y,
    int width,
    int height,
    int depth,
    int parent_id,
    int& next_node_id,
    const GeometryTopologyBuildConfig& config)
{
    if (width <= 0 || height <= 0)
    {
        return;
    }

    FractalPartitionNodeRecord node;
    node.node_id = next_node_id++;
    node.parent_id = parent_id;
    node.depth = depth;
    node.x = x;
    node.y = y;
    node.width = width;
    node.height = height;
    node.status = ClassifyTopologyRegion(mask, image_width, image_height, x, y, width, height);

    const bool size_limited = width <= config.min_cell_size || height <= config.min_cell_size;
    const bool stop_split =
        depth >= config.max_depth ||
        width <= 1 ||
        height <= 1 ||
        node.status != 2 ||
        size_limited;

    node.is_leaf = stop_split ? 1 : 0;
    output.nodes.push_back(node);

    if (stop_split)
    {
        return;
    }

    const int left_width = width / 2;
    const int right_width = width - left_width;
    const int top_height = height / 2;
    const int bottom_height = height - top_height;

    BuildFractalPartitionRecursive(output, mask, image_width, image_height, x, y, left_width, top_height, depth + 1, node.node_id, next_node_id, config);
    BuildFractalPartitionRecursive(output, mask, image_width, image_height, x + left_width, y, right_width, top_height, depth + 1, node.node_id, next_node_id, config);
    BuildFractalPartitionRecursive(output, mask, image_width, image_height, x, y + top_height, left_width, bottom_height, depth + 1, node.node_id, next_node_id, config);
    BuildFractalPartitionRecursive(output, mask, image_width, image_height, x + left_width, y + top_height, right_width, bottom_height, depth + 1, node.node_id, next_node_id, config);
}

bool TopologyIsBoundarySeed(const std::vector<unsigned char>& mask, int width, int height, int x, int y)
{
    if (!TopologyIsForeground(mask, width, height, x, y))
    {
        return false;
    }

    static const int offsets[4][2] = {
        { 1, 0 },
        { -1, 0 },
        { 0, 1 },
        { 0, -1 }
    };

    for (const auto& offset : offsets)
    {
        const int nx = x + offset[0];
        const int ny = y + offset[1];
        if (!TopologyIsInside(nx, ny, width, height) || !TopologyIsForeground(mask, width, height, nx, ny))
        {
            return true;
        }
    }

    return false;
}

struct TopologyDistanceEntry
{
    double distance = 0.0;
    int x = 0;
    int y = 0;
};

struct TopologyDistanceEntryCompare
{
    bool operator()(const TopologyDistanceEntry& lhs, const TopologyDistanceEntry& rhs) const
    {
        return lhs.distance > rhs.distance;
    }
};

int CountSkeletonNeighbors(const std::vector<unsigned char>& skeleton_mask, int width, int height, int x, int y)
{
    int count = 0;
    for (int dy = -1; dy <= 1; ++dy)
    {
        for (int dx = -1; dx <= 1; ++dx)
        {
            if (dx == 0 && dy == 0)
            {
                continue;
            }

            const int nx = x + dx;
            const int ny = y + dy;
            if (!TopologyIsInside(nx, ny, width, height))
            {
                continue;
            }

            if (skeleton_mask[TopologyMaskIndex(nx, ny, width)] != 0)
            {
                ++count;
            }
        }
    }
    return count;
}

std::vector<std::vector<GeometrySkeletonPointRecord>> CollectSkeletonComponents(
    const std::vector<unsigned char>& skeleton_mask,
    int width,
    int height)
{
    std::vector<std::vector<GeometrySkeletonPointRecord>> components;
    std::vector<unsigned char> visited(static_cast<size_t>(width * height), 0);
    const std::vector<std::pair<int, int>> offsets = TopologyNeighborOffsets(true);

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const size_t index = TopologyMaskIndex(x, y, width);
            if (visited[index] || skeleton_mask[index] == 0)
            {
                continue;
            }

            std::vector<GeometrySkeletonPointRecord> component;
            std::queue<GeometrySkeletonPointRecord> queue;
            queue.push({ x, y });
            visited[index] = 1;

            while (!queue.empty())
            {
                const GeometrySkeletonPointRecord point = queue.front();
                queue.pop();
                component.push_back(point);

                for (const auto& offset : offsets)
                {
                    const int nx = point.x + offset.first;
                    const int ny = point.y + offset.second;
                    if (!TopologyIsInside(nx, ny, width, height))
                    {
                        continue;
                    }

                    const size_t neighbor_index = TopologyMaskIndex(nx, ny, width);
                    if (visited[neighbor_index] || skeleton_mask[neighbor_index] == 0)
                    {
                        continue;
                    }

                    visited[neighbor_index] = 1;
                    queue.push({ nx, ny });
                }
            }

            components.push_back(component);
        }
    }

    return components;
}

std::vector<GeometrySkeletonPointRecord> CollectComponentEndpoints(
    const std::vector<GeometrySkeletonPointRecord>& component,
    const std::vector<unsigned char>& skeleton_mask,
    int width,
    int height)
{
    std::vector<GeometrySkeletonPointRecord> endpoints;
    for (const GeometrySkeletonPointRecord& point : component)
    {
        const int neighbors = CountSkeletonNeighbors(skeleton_mask, width, height, point.x, point.y);
        if (neighbors <= 1)
        {
            endpoints.push_back(point);
        }
    }
    return endpoints;
}

std::vector<GeometryPathPointRecord> BuildOrderedPath(
    const std::vector<GeometrySkeletonPointRecord>& component,
    const std::vector<unsigned char>& skeleton_mask,
    int width,
    int height)
{
    std::vector<GeometryPathPointRecord> ordered;
    if (component.empty())
    {
        return ordered;
    }

    const std::vector<GeometrySkeletonPointRecord> endpoints =
        CollectComponentEndpoints(component, skeleton_mask, width, height);
    const GeometrySkeletonPointRecord start = endpoints.empty() ? component.front() : endpoints.front();

    std::queue<GeometrySkeletonPointRecord> queue;
    std::vector<unsigned char> visited(static_cast<size_t>(width * height), 0);
    queue.push(start);
    visited[TopologyMaskIndex(start.x, start.y, width)] = 1;

    const std::vector<std::pair<int, int>> offsets = TopologyNeighborOffsets(true);
    while (!queue.empty())
    {
        const GeometrySkeletonPointRecord point = queue.front();
        queue.pop();
        ordered.push_back({ point.x, point.y });

        for (const auto& offset : offsets)
        {
            const int nx = point.x + offset.first;
            const int ny = point.y + offset.second;
            if (!TopologyIsInside(nx, ny, width, height))
            {
                continue;
            }

            const size_t neighbor_index = TopologyMaskIndex(nx, ny, width);
            if (visited[neighbor_index] || skeleton_mask[neighbor_index] == 0)
            {
                continue;
            }

            visited[neighbor_index] = 1;
            queue.push({ nx, ny });
        }
    }

    return ordered;
}

GeometryPathRecord BuildPathRecord(
    int path_id,
    const std::vector<GeometrySkeletonPointRecord>& component,
    const std::vector<unsigned char>& skeleton_mask,
    int width,
    int height)
{
    GeometryPathRecord record;
    record.path_id = path_id;
    record.points = BuildOrderedPath(component, skeleton_mask, width, height);

    double length = 0.0;
    for (size_t i = 1; i < record.points.size(); ++i)
    {
        const double dx = static_cast<double>(record.points[i].x - record.points[i - 1].x);
        const double dy = static_cast<double>(record.points[i].y - record.points[i - 1].y);
        length += std::sqrt(dx * dx + dy * dy);
    }
    record.path_length = length;
    return record;
}

std::vector<GeometryPathPointRecord> BuildLinePath(int x0, int y0, int x1, int y1)
{
    std::vector<GeometryPathPointRecord> points;
    int dx = std::abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (true)
    {
        points.push_back({ x0, y0 });
        if (x0 == x1 && y0 == y1)
        {
            break;
        }

        const int e2 = 2 * err;
        if (e2 >= dy)
        {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx)
        {
            err += dx;
            y0 += sy;
        }
    }

    return points;
}

double MeasurePathLength(const std::vector<GeometryPathPointRecord>& points)
{
    double length = 0.0;
    for (size_t i = 1; i < points.size(); ++i)
    {
        const double dx = static_cast<double>(points[i].x - points[i - 1].x);
        const double dy = static_cast<double>(points[i].y - points[i - 1].y);
        length += std::sqrt(dx * dx + dy * dy);
    }
    return length;
}

} // namespace

FractalPartitionOutput BuildFractalPartitionFromMask(
    const std::vector<unsigned char>& mask,
    int width,
    int height,
    const std::string& source_mask_id,
    const GeometryTopologyBuildConfig& config)
{
    FractalPartitionOutput output;
    output.source_mask_id = source_mask_id;
    output.width = width;
    output.height = height;

    if (width <= 0 || height <= 0 || mask.size() != static_cast<size_t>(width * height))
    {
        output.summary = "invalid_mask";
        return output;
    }

    int next_node_id = 0;
    BuildFractalPartitionRecursive(output, mask, width, height, 0, 0, width, height, 0, -1, next_node_id, config);

    output.status = 1;
    output.node_count = static_cast<int>(output.nodes.size());
    output.leaf_node_count = 0;
    output.boundary_node_count = 0;
    output.max_depth = 0;
    for (const FractalPartitionNodeRecord& node : output.nodes)
    {
        output.leaf_node_count += node.is_leaf != 0 ? 1 : 0;
        output.boundary_node_count += node.status == 2 ? 1 : 0;
        output.max_depth = std::max(output.max_depth, node.depth);
    }

    std::ostringstream summary;
    summary << "partition_ok; nodes=" << output.node_count
            << "; boundary_nodes=" << output.boundary_node_count
            << "; max_depth=" << output.max_depth;
    output.summary = summary.str();
    return output;
}

GeometryDistanceFieldOutput BuildGeometryDistanceFieldFromMask(
    const std::vector<unsigned char>& mask,
    int width,
    int height,
    const std::string& source_mask_id,
    bool use_eight_connected)
{
    GeometryDistanceFieldOutput output;
    output.source_mask_id = source_mask_id;
    output.width = width;
    output.height = height;

    if (width <= 0 || height <= 0 || mask.size() != static_cast<size_t>(width * height))
    {
        output.summary = "invalid_mask";
        return output;
    }

    output.raster_distances.assign(static_cast<size_t>(width * height), -1.0);
    std::priority_queue<TopologyDistanceEntry, std::vector<TopologyDistanceEntry>, TopologyDistanceEntryCompare> queue;
    const std::vector<std::pair<int, int>> offsets = TopologyNeighborOffsets(use_eight_connected);

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            if (!TopologyIsBoundarySeed(mask, width, height, x, y))
            {
                continue;
            }

            const size_t index = TopologyMaskIndex(x, y, width);
            output.raster_distances[index] = 0.0;
            queue.push({ 0.0, x, y });
            ++output.seed_count;
        }
    }

    if (output.seed_count == 0)
    {
        output.summary = "no_boundary_seed";
        return output;
    }

    while (!queue.empty())
    {
        const TopologyDistanceEntry entry = queue.top();
        queue.pop();
        const size_t index = TopologyMaskIndex(entry.x, entry.y, width);
        if (entry.distance > output.raster_distances[index])
        {
            continue;
        }

        for (const auto& offset : offsets)
        {
            const int nx = entry.x + offset.first;
            const int ny = entry.y + offset.second;
            if (!TopologyIsForeground(mask, width, height, nx, ny))
            {
                continue;
            }

            const double step =
                (offset.first != 0 && offset.second != 0) ? std::sqrt(2.0) : 1.0;
            const double next_distance = entry.distance + step;
            const size_t neighbor_index = TopologyMaskIndex(nx, ny, width);
            if (output.raster_distances[neighbor_index] >= 0.0 && output.raster_distances[neighbor_index] <= next_distance)
            {
                continue;
            }

            output.raster_distances[neighbor_index] = next_distance;
            queue.push({ next_distance, nx, ny });
        }
    }

    double sum = 0.0;
    int count = 0;
    output.min_distance = std::numeric_limits<double>::max();
    output.max_distance = 0.0;
    for (double value : output.raster_distances)
    {
        if (value < 0.0)
        {
            continue;
        }
        output.node_distances.push_back(value);
        output.min_distance = std::min(output.min_distance, value);
        output.max_distance = std::max(output.max_distance, value);
        sum += value;
        ++count;
    }

    if (count == 0)
    {
        output.min_distance = 0.0;
        output.summary = "distance_empty";
        return output;
    }

    output.mean_distance = sum / static_cast<double>(count);
    output.status = 1;

    std::ostringstream summary;
    summary << "distance_ok; seeds=" << output.seed_count
            << "; max_distance=" << output.max_distance;
    output.summary = summary.str();
    return output;
}

GeometrySkeletonOutput BuildGeometrySkeletonFromDistanceField(
    const GeometryDistanceFieldOutput& distance,
    const std::vector<unsigned char>& mask,
    int width,
    int height,
    const std::string& source_mask_id)
{
    GeometrySkeletonOutput output;
    output.source_mask_id = source_mask_id.empty() ? distance.source_mask_id : source_mask_id;
    output.width = width;
    output.height = height;

    if (width <= 0 || height <= 0 ||
        mask.size() != static_cast<size_t>(width * height) ||
        distance.raster_distances.size() != static_cast<size_t>(width * height))
    {
        output.summary = "invalid_distance_or_mask";
        return output;
    }

    output.skeleton_mask.assign(static_cast<size_t>(width * height), 0);

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            if (!TopologyIsForeground(mask, width, height, x, y))
            {
                continue;
            }

            const size_t index = TopologyMaskIndex(x, y, width);
            const double center_distance = distance.raster_distances[index];
            if (center_distance < 0.0)
            {
                continue;
            }

            bool is_local_max = center_distance > 0.0;
            for (int dy = -1; dy <= 1 && is_local_max; ++dy)
            {
                for (int dx = -1; dx <= 1; ++dx)
                {
                    if (dx == 0 && dy == 0)
                    {
                        continue;
                    }
                    const int nx = x + dx;
                    const int ny = y + dy;
                    if (!TopologyIsInside(nx, ny, width, height))
                    {
                        continue;
                    }
                    const double neighbor_distance = distance.raster_distances[TopologyMaskIndex(nx, ny, width)];
                    if (neighbor_distance > center_distance)
                    {
                        is_local_max = false;
                        break;
                    }
                }
            }

            if (is_local_max)
            {
                output.skeleton_mask[index] = 255;
                ++output.skeleton_pixel_count;
            }
        }
    }

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const size_t index = TopologyMaskIndex(x, y, width);
            if (output.skeleton_mask[index] == 0)
            {
                continue;
            }

            const int neighbors = CountSkeletonNeighbors(output.skeleton_mask, width, height, x, y);
            if (neighbors <= 1)
            {
                output.endpoints.push_back({ x, y });
            }
            else if (neighbors >= 3)
            {
                output.branch_points.push_back({ x, y });
            }
        }
    }

    output.endpoint_count = static_cast<int>(output.endpoints.size());
    output.branch_point_count = static_cast<int>(output.branch_points.size());
    output.status = 1;

    std::ostringstream summary;
    summary << "skeleton_ok; pixels=" << output.skeleton_pixel_count
            << "; endpoints=" << output.endpoint_count
            << "; branch_points=" << output.branch_point_count;
    output.summary = summary.str();
    return output;
}

GeometryCenterlineOutput BuildGeometryCenterlineFromSkeleton(
    const GeometrySkeletonOutput& skeleton,
    const std::string& source_mask_id)
{
    GeometryCenterlineOutput output;
    output.source_mask_id = source_mask_id.empty() ? skeleton.source_mask_id : source_mask_id;
    output.width = skeleton.width;
    output.height = skeleton.height;

    if (skeleton.width <= 0 || skeleton.height <= 0 ||
        skeleton.skeleton_mask.size() != static_cast<size_t>(skeleton.width * skeleton.height))
    {
        output.summary = "invalid_skeleton";
        return output;
    }

    const std::vector<std::vector<GeometrySkeletonPointRecord>> components =
        CollectSkeletonComponents(skeleton.skeleton_mask, skeleton.width, skeleton.height);

    output.path_count = static_cast<int>(components.size());
    output.min_path_length = std::numeric_limits<double>::max();
    output.max_path_length = 0.0;
    double length_sum = 0.0;

    for (size_t i = 0; i < components.size(); ++i)
    {
        GeometryPathRecord record = BuildPathRecord(static_cast<int>(i), components[i], skeleton.skeleton_mask, skeleton.width, skeleton.height);
        output.junction_count += static_cast<int>(components[i].size() > 2 ? 1 : 0);
        output.min_path_length = std::min(output.min_path_length, record.path_length);
        output.max_path_length = std::max(output.max_path_length, record.path_length);
        length_sum += record.path_length;
        if (output.main_path_id < 0 || record.path_length >= output.max_path_length)
        {
            output.main_path_id = record.path_id;
        }
        output.centerline_paths.push_back(record);
    }

    if (output.centerline_paths.empty())
    {
        output.min_path_length = 0.0;
        output.summary = "centerline_empty";
        return output;
    }

    output.mean_path_length = length_sum / static_cast<double>(output.centerline_paths.size());
    output.status = 1;

    std::ostringstream summary;
    summary << "centerline_ok; paths=" << output.path_count
            << "; main_path_id=" << output.main_path_id;
    output.summary = summary.str();
    return output;
}

GeometryTopologyRepairOutput BuildGeometryTopologyRepairFromSkeleton(
    const GeometrySkeletonOutput& skeleton,
    const std::string& source_mask_id)
{
    GeometryTopologyRepairOutput output;
    output.source_mask_id = source_mask_id.empty() ? skeleton.source_mask_id : source_mask_id;
    output.width = skeleton.width;
    output.height = skeleton.height;

    if (skeleton.width <= 0 || skeleton.height <= 0 ||
        skeleton.skeleton_mask.size() != static_cast<size_t>(skeleton.width * skeleton.height))
    {
        output.summary = "invalid_skeleton";
        return output;
    }

    const std::vector<std::vector<GeometrySkeletonPointRecord>> components =
        CollectSkeletonComponents(skeleton.skeleton_mask, skeleton.width, skeleton.height);

    if (components.size() <= 1)
    {
        output.status = 1;
        output.repair_success_rate = 1.0;
        output.summary = "repair_not_needed";
        return output;
    }

    std::vector<int> attached_components = { 0 };
    std::vector<unsigned char> in_attached(components.size(), 0);
    in_attached[0] = 1;

    while (attached_components.size() < components.size())
    {
        double best_distance = std::numeric_limits<double>::max();
        int best_from_component = -1;
        int best_to_component = -1;
        GeometrySkeletonPointRecord best_from_point{};
        GeometrySkeletonPointRecord best_to_point{};

        for (size_t i = 0; i < components.size(); ++i)
        {
            if (!in_attached[i])
            {
                continue;
            }

            std::vector<GeometrySkeletonPointRecord> from_points =
                CollectComponentEndpoints(components[i], skeleton.skeleton_mask, skeleton.width, skeleton.height);
            if (from_points.empty())
            {
                from_points = components[i];
            }

            for (size_t j = 0; j < components.size(); ++j)
            {
                if (in_attached[j])
                {
                    continue;
                }

                std::vector<GeometrySkeletonPointRecord> to_points =
                    CollectComponentEndpoints(components[j], skeleton.skeleton_mask, skeleton.width, skeleton.height);
                if (to_points.empty())
                {
                    to_points = components[j];
                }

                for (const GeometrySkeletonPointRecord& from_point : from_points)
                {
                    for (const GeometrySkeletonPointRecord& to_point : to_points)
                    {
                        const double dx = static_cast<double>(to_point.x - from_point.x);
                        const double dy = static_cast<double>(to_point.y - from_point.y);
                        const double distance_value = std::sqrt(dx * dx + dy * dy);
                        if (distance_value < best_distance)
                        {
                            best_distance = distance_value;
                            best_from_component = static_cast<int>(i);
                            best_to_component = static_cast<int>(j);
                            best_from_point = from_point;
                            best_to_point = to_point;
                        }
                    }
                }
            }
        }

        if (best_from_component < 0 || best_to_component < 0)
        {
            break;
        }

        GeometryPathRecord repair_record;
        repair_record.path_id = static_cast<int>(output.repair_paths.size());
        repair_record.points = BuildLinePath(best_from_point.x, best_from_point.y, best_to_point.x, best_to_point.y);
        repair_record.path_length = MeasurePathLength(repair_record.points);
        output.repair_paths.push_back(repair_record);
        in_attached[best_to_component] = 1;
        attached_components.push_back(best_to_component);
    }

    output.repair_path_count = static_cast<int>(output.repair_paths.size());
    output.repaired_gap_count = output.repair_path_count;
    output.min_repair_cost = std::numeric_limits<double>::max();
    output.max_repair_cost = 0.0;
    double cost_sum = 0.0;

    for (const GeometryPathRecord& record : output.repair_paths)
    {
        output.min_repair_cost = std::min(output.min_repair_cost, record.path_length);
        output.max_repair_cost = std::max(output.max_repair_cost, record.path_length);
        cost_sum += record.path_length;
    }

    if (output.repair_paths.empty())
    {
        output.min_repair_cost = 0.0;
        output.summary = "repair_failed";
        return output;
    }

    output.mean_repair_cost = cost_sum / static_cast<double>(output.repair_paths.size());
    const int needed_repairs = static_cast<int>(components.size()) - 1;
    output.repair_success_rate = needed_repairs > 0
        ? static_cast<double>(output.repair_path_count) / static_cast<double>(needed_repairs)
        : 1.0;
    output.status = 1;

    std::ostringstream summary;
    summary << "repair_ok; repairs=" << output.repair_path_count
            << "; success_rate=" << output.repair_success_rate;
    output.summary = summary.str();
    return output;
}

GeometryTopologyPipelineOutput BuildGeometryTopologyPipelineFromMask(
    const std::vector<unsigned char>& mask,
    int width,
    int height,
    const std::string& source_mask_id,
    const GeometryTopologyBuildConfig& config)
{
    GeometryTopologyPipelineOutput output;
    output.partition = BuildFractalPartitionFromMask(mask, width, height, source_mask_id, config);
    output.distance = BuildGeometryDistanceFieldFromMask(mask, width, height, source_mask_id, config.use_eight_connected);
    output.skeleton = BuildGeometrySkeletonFromDistanceField(output.distance, mask, width, height, source_mask_id);
    output.centerline = BuildGeometryCenterlineFromSkeleton(output.skeleton, source_mask_id);
    output.repair = BuildGeometryTopologyRepairFromSkeleton(output.skeleton, source_mask_id);
    return output;
}

std::vector<std::string> GetBaselineSummaryNamesV1()
{
    return {
        "task",
        "feature_set",
        "model",
        "accuracy",
        "macro_f1",
        "fit_time_ms",
        "infer_time_ms",
        "feature_dim",
        "notes"
    };
}

std::vector<std::string> ExportBaselineSummaryValuesV1(const BaselineSummaryRecordV1& record)
{
    std::vector<std::string> values;
    values.reserve(9);
    values.push_back(record.task);
    values.push_back(record.feature_set);
    values.push_back(record.model);
    values.push_back(std::to_string(record.accuracy));
    values.push_back(std::to_string(record.macro_f1));
    values.push_back(std::to_string(record.fit_time_ms));
    values.push_back(std::to_string(record.infer_time_ms));
    values.push_back(std::to_string(record.feature_dim));
    values.push_back(record.notes);
    return values;
}

std::string ExportBaselineSummaryCsvHeaderV1()
{
    std::ostringstream header;
    bool first = true;
    for (const std::string& name : GetBaselineSummaryNamesV1())
    {
        AppendCsvValue(header, name, first);
    }
    return header.str();
}

std::string ExportBaselineSummaryCsvRowV1(const BaselineSummaryRecordV1& record)
{
    std::ostringstream row;
    bool first = true;
    for (const std::string& value : ExportBaselineSummaryValuesV1(record))
    {
        AppendCsvValue(row, value, first);
    }
    return row.str();
}

} // namespace cxcore
