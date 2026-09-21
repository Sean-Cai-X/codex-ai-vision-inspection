#ifndef CXCORE_CORE_CXCOREBOUNDARY_H
#define CXCORE_CORE_CXCOREBOUNDARY_H

#include <string>
#include <vector>

class Image;
class PointsShape;
class FindLine;
class FindCircle;
class FindEllipse;
class FindObject;
class FastMatch;
namespace cxcore { struct RegionPatternDescriptor; struct RegionPatternScore; }

namespace cxcore {

// Stable output contracts for the classical analysis layer.
// These structs intentionally stop at direct measurements so higher layers
// can own optimization, fusion, and model-based reasoning.
struct OutputPoint
{
    double x = 0.0;
    double y = 0.0;
};

struct OutputRect
{
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
};

struct PointSetOutput
{
    std::vector<OutputPoint> points;
    OutputRect bounds;
};

struct ImageComponentOutput
{
    OutputRect bounds;
    OutputPoint centroid;
    int area = 0;
};

struct ImageAnalysisOutput
{
    int width = 0;
    int height = 0;
    int image_type = 0;
    std::vector<ImageComponentOutput> connected_components;
};

struct LineMeasurementOutput
{
    PointSetOutput horizontal_samples;
    PointSetOutput vertical_samples;
    OutputRect measure_bounds;
};

struct CircleMeasurementOutput
{
    PointSetOutput sample_points;
    OutputRect measure_bounds;
    OutputPoint center;
    double radius = 0.0;
    double average_distance = 0.0;
    bool has_direct_fit = false;
};

struct EllipseMeasurementOutput
{
    PointSetOutput sample_points;
    OutputRect measure_bounds;
};

struct DetectionOutput
{
    std::vector<OutputRect> boxes;
};

struct MatchCandidateOutput
{
    OutputRect bounds;
    OutputPoint center;
    double score = 0.0;
};

struct MatchOutput
{
    std::vector<MatchCandidateOutput> candidates;
    double max_score = 0.0;
    double image_model_score = 0.0;
};

struct RegionPatternDescriptorOutput
{
    int normalized_width = 0;
    int normalized_height = 0;
    int pooling_rows = 0;
    int pooling_cols = 0;
    double global_foreground_ratio = 0.0;
    std::vector<double> values;
};

struct RegionPatternScoreOutput
{
    bool success = false;
    double descriptor_distance = 0.0;
    double content_score = 0.0;
    std::string summary;
};

struct FractalPartitionNodeRecord
{
    int node_id = -1;
    int parent_id = -1;
    int depth = 0;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int status = 0;
    int is_leaf = 0;
};

struct FractalPartitionOutput
{
    int status = 0;
    std::string summary;

    std::string source_mask_id;
    int width = 0;
    int height = 0;

    int node_count = 0;
    int leaf_node_count = 0;
    int boundary_node_count = 0;
    int max_depth = 0;

    std::vector<FractalPartitionNodeRecord> nodes;

    std::string debug_preview_ref;
};

struct GeometryDistanceFieldOutput
{
    int status = 0;
    std::string summary;

    std::string source_mask_id;
    int width = 0;
    int height = 0;

    int seed_count = 0;

    double min_distance = 0.0;
    double max_distance = 0.0;
    double mean_distance = 0.0;

    std::vector<double> node_distances;
    std::vector<double> raster_distances;

    std::string debug_heatmap_ref;
};

struct GeometrySkeletonPointRecord
{
    int x = 0;
    int y = 0;
};

struct GeometrySkeletonOutput
{
    int status = 0;
    std::string summary;

    std::string source_mask_id;
    int width = 0;
    int height = 0;

    int skeleton_pixel_count = 0;
    int endpoint_count = 0;
    int branch_point_count = 0;

    std::vector<unsigned char> skeleton_mask;
    std::vector<GeometrySkeletonPointRecord> endpoints;
    std::vector<GeometrySkeletonPointRecord> branch_points;

    std::string debug_overlay_ref;
};

struct GeometryPathPointRecord
{
    int x = 0;
    int y = 0;
};

struct GeometryPathRecord
{
    int path_id = -1;
    double path_length = 0.0;
    std::vector<GeometryPathPointRecord> points;
};

struct GeometryCenterlineOutput
{
    int status = 0;
    std::string summary;

    std::string source_mask_id;
    int width = 0;
    int height = 0;

    int path_count = 0;
    int junction_count = 0;

    double min_path_length = 0.0;
    double max_path_length = 0.0;
    double mean_path_length = 0.0;

    int main_path_id = -1;
    std::vector<GeometryPathRecord> centerline_paths;

    std::string debug_path_overlay_ref;
};

struct GeometryTopologyRepairOutput
{
    int status = 0;
    std::string summary;

    std::string source_mask_id;
    int width = 0;
    int height = 0;

    int repair_path_count = 0;
    int repaired_gap_count = 0;

    double min_repair_cost = 0.0;
    double max_repair_cost = 0.0;
    double mean_repair_cost = 0.0;
    double repair_success_rate = 0.0;

    std::vector<GeometryPathRecord> repair_paths;

    std::string debug_repair_overlay_ref;
};


struct GeometryTopologyBuildConfig
{
    int max_depth = 3;
    int min_cell_size = 4;
    bool use_eight_connected = true;
};

struct GeometryTopologyPipelineOutput
{
    FractalPartitionOutput partition;
    GeometryDistanceFieldOutput distance;
    GeometrySkeletonOutput skeleton;
    GeometryCenterlineOutput centerline;
    GeometryTopologyRepairOutput repair;
};

// Baseline validation layer for cxcore structured outputs.
// The schema is intentionally flat so downstream evaluators can emit
// deterministic CSV rows and compare classical baselines directly.
struct BaselineFeatureSampleV1
{
    std::string sample_id;
    std::string source_image_id;
    std::string roi_id;
    std::string label;
    std::string split;

    double roi_x = 0.0;
    double roi_y = 0.0;
    double roi_w = 0.0;
    double roi_h = 0.0;
    double roi_area = 0.0;
    double roi_aspect_ratio = 0.0;

    double image_width = 0.0;
    double image_height = 0.0;
    double image_type = 0.0;

    double gray_mean = 0.0;
    double gray_std = 0.0;
    double gray_min = 0.0;
    double gray_max = 0.0;
    double edge_pixel_ratio = 0.0;
    double binary_foreground_ratio = 0.0;
    double region_pattern_foreground_ratio = 0.0;
    double region_pattern_descriptor_dim = 0.0;
    double region_pattern_descriptor_mean = 0.0;
    double region_pattern_descriptor_std = 0.0;

    double component_count = 0.0;
    double largest_component_area = 0.0;
    double largest_component_ratio = 0.0;
    double largest_bbox_x = 0.0;
    double largest_bbox_y = 0.0;
    double largest_bbox_w = 0.0;
    double largest_bbox_h = 0.0;
    double largest_bbox_aspect_ratio = 0.0;
    double largest_centroid_x = 0.0;
    double largest_centroid_y = 0.0;

    double line_w_points_count = 0.0;
    double line_h_points_count = 0.0;
    double line_measure_bbox_x = 0.0;
    double line_measure_bbox_y = 0.0;
    double line_measure_bbox_w = 0.0;
    double line_measure_bbox_h = 0.0;

    double circle_points_count = 0.0;
    double circle_center_x = 0.0;
    double circle_center_y = 0.0;
    double circle_radius = 0.0;
    double circle_avg_dist = 0.0;
    double circle_measure_bbox_x = 0.0;
    double circle_measure_bbox_y = 0.0;
    double circle_measure_bbox_w = 0.0;
    double circle_measure_bbox_h = 0.0;
    double circle_fit_valid = 0.0;

    double match_candidate_count = 0.0;
    double match_best_score = 0.0;
    double match_best_center_x = 0.0;
    double match_best_center_y = 0.0;
    double match_best_rect_x = 0.0;
    double match_best_rect_y = 0.0;
    double match_best_rect_w = 0.0;
    double match_best_rect_h = 0.0;
    double image_model_score = 0.0;
};

enum class BaselineFeatureSetV1
{
    CoreGeom,
    MatchOnly,
    AllV1
};

struct BaselineSummaryRecordV1
{
    std::string task;
    std::string feature_set;
    std::string model;
    double accuracy = 0.0;
    double macro_f1 = 0.0;
    double fit_time_ms = 0.0;
    double infer_time_ms = 0.0;
    double feature_dim = 0.0;
    std::string notes;
};

PointSetOutput ExportPointSet(PointsShape& points);
ImageAnalysisOutput ExportImageAnalysis(const Image& image, double min_area, int min_width = 0, int min_height = 0);
LineMeasurementOutput ExportLineMeasurement(FindLine& line);
CircleMeasurementOutput ExportCircleMeasurement(FindCircle& circle);
EllipseMeasurementOutput ExportEllipseMeasurement(FindEllipse& ellipse);
DetectionOutput ExportDetections(FindObject& object);
MatchOutput ExportMatchOutput(FastMatch& matcher, int max_candidates);
RegionPatternDescriptorOutput ExportRegionPatternDescriptor(const RegionPatternDescriptor& descriptor);
RegionPatternScoreOutput ExportRegionPatternScore(const RegionPatternScore& score);

FractalPartitionOutput BuildFractalPartitionFromMask(
    const std::vector<unsigned char>& mask,
    int width,
    int height,
    const std::string& source_mask_id = std::string(),
    const GeometryTopologyBuildConfig& config = GeometryTopologyBuildConfig());
GeometryDistanceFieldOutput BuildGeometryDistanceFieldFromMask(
    const std::vector<unsigned char>& mask,
    int width,
    int height,
    const std::string& source_mask_id = std::string(),
    bool use_eight_connected = true);
GeometrySkeletonOutput BuildGeometrySkeletonFromDistanceField(
    const GeometryDistanceFieldOutput& distance,
    const std::vector<unsigned char>& mask,
    int width,
    int height,
    const std::string& source_mask_id = std::string());
GeometryCenterlineOutput BuildGeometryCenterlineFromSkeleton(
    const GeometrySkeletonOutput& skeleton,
    const std::string& source_mask_id = std::string());
GeometryTopologyRepairOutput BuildGeometryTopologyRepairFromSkeleton(
    const GeometrySkeletonOutput& skeleton,
    const std::string& source_mask_id = std::string());
GeometryTopologyPipelineOutput BuildGeometryTopologyPipelineFromMask(
    const std::vector<unsigned char>& mask,
    int width,
    int height,
    const std::string& source_mask_id = std::string(),
    const GeometryTopologyBuildConfig& config = GeometryTopologyBuildConfig());
BaselineFeatureSampleV1 ExportBaselineFeatureSampleV1(
    const Image& image,
    const ImageAnalysisOutput& analysis,
    const LineMeasurementOutput& line,
    const CircleMeasurementOutput& circle,
    const MatchOutput& match);
std::vector<std::string> GetBaselineMetadataNamesV1();
std::vector<std::string> GetBaselineFeatureNamesV1();
std::vector<std::string> ExportBaselineMetadataValuesV1(const BaselineFeatureSampleV1& sample);
std::vector<double> ExportBaselineFeatureValuesV1(const BaselineFeatureSampleV1& sample);
std::string ExportBaselineCsvHeaderV1();
std::string ExportBaselineCsvRowV1(const BaselineFeatureSampleV1& sample);
const char* BaselineFeatureSetNameV1(BaselineFeatureSetV1 feature_set);
std::vector<std::string> GetBaselineFeatureNamesV1(BaselineFeatureSetV1 feature_set);
std::vector<double> ExportBaselineFeatureValuesV1(
    const BaselineFeatureSampleV1& sample,
    BaselineFeatureSetV1 feature_set);
std::string ExportBaselineCsvHeaderV1(BaselineFeatureSetV1 feature_set);
std::string ExportBaselineCsvRowV1(
    const BaselineFeatureSampleV1& sample,
    BaselineFeatureSetV1 feature_set);
std::vector<std::string> GetBaselineSummaryNamesV1();
std::vector<std::string> ExportBaselineSummaryValuesV1(const BaselineSummaryRecordV1& record);
std::string ExportBaselineSummaryCsvHeaderV1();
std::string ExportBaselineSummaryCsvRowV1(const BaselineSummaryRecordV1& record);

} // namespace cxcore

#endif
