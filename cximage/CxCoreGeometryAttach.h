#ifndef CXCORE_CORE_CXCOREGEOMETRYATTACH_H
#define CXCORE_CORE_CXCOREGEOMETRYATTACH_H

#include <cstdint>
#include <string>
#include <vector>

#include "CxCoreBoundary.h"
#include "../cxgeom/include/CxGeomAnnotationBody.h"
#include "../cxgeom/include/CxGeomElementBody.h"

namespace cxcore {

enum class GeometryObjectKind
{
    Unknown = 0,
    Roi,
    Line,
    PointSet,
    Mask,
    Boundary,
    Keypoints,
    FractalPartition,
    DistanceField,
    Skeleton
};

enum class GeometryRole
{
    Unknown = 0,
    InputPrior,
    TrainingLabel,
    OutputAttach
};

enum class GeometryAtomicOperation
{
    Unknown = 0,
    Load,
    Publish,
    Attach,
    Summarize,
    ExportFeatureVector
};

struct StableGeometryRef
{
    std::string object_id;
    std::string source_image_id;
    std::string parent_roi_id;
    GeometryObjectKind kind = GeometryObjectKind::Unknown;
};

struct RoiObject
{
    StableGeometryRef ref;
    OutputRect bounds;
    float score_hint = 0.0f;
    int label_hint = -1;
};

struct LineObject
{
    StableGeometryRef ref;
    PointSetOutput samples;
    OutputPoint start;
    OutputPoint end;
    float score_hint = 0.0f;
};

struct PointSetObject
{
    StableGeometryRef ref;
    PointSetOutput samples;
    std::string sampling_mode;
};

struct MaskObject
{
    StableGeometryRef ref;
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> values;
};

struct BoundaryObject
{
    StableGeometryRef ref;
    PointSetOutput contour;
    bool closed = false;
};

struct KeypointsObject
{
    StableGeometryRef ref;
    std::vector<OutputPoint> points;
    std::vector<float> visibility;
};

struct FractalPartitionObject
{
    StableGeometryRef ref;
    FractalPartitionOutput output;
    std::string overlay_object_id;
};

struct DistanceFieldObject
{
    StableGeometryRef ref;
    GeometryDistanceFieldOutput output;
    std::string overlay_object_id;
};

struct SkeletonObject
{
    StableGeometryRef ref;
    GeometrySkeletonOutput output;
    std::string overlay_object_id;
};

struct GeometryAttachRecord
{
    StableGeometryRef target;
    std::string attach_id;
    std::string label;
    float score = 0.0f;
    OutputRect refined_bounds;
    std::vector<float> embedding;
    std::string mask_object_id;
    std::string boundary_object_id;
    std::string keypoints_object_id;
    std::string fractal_partition_object_id;
    std::string distance_field_object_id;
    std::string skeleton_object_id;
};

struct GeometryDisplayHint
{
    bool supports_2d_overlay = false;
    bool supports_3d_overlay = false;
    std::string overlay_object_id;
    std::string preview_ref;
};

struct GeometryPropertyItem
{
    std::string key;
    std::string value;
};

struct GeometryObjectSummary
{
    StableGeometryRef ref;
    std::string summary;
    int width = 0;
    int height = 0;
    GeometryDisplayHint display;
    std::vector<GeometryPropertyItem> properties;
};

struct GeometryTopologyBundle
{
    std::string bundle_id;
    StableGeometryRef target_roi_ref;
    std::string fractal_partition_object_id;
    std::string distance_field_object_id;
    std::string skeleton_object_id;
};

struct GeometryTopologyBundleSummary
{
    std::string bundle_id;
    StableGeometryRef target_roi_ref;
    int object_count = 0;
    std::vector<std::string> object_ids;
    std::vector<GeometryObjectSummary> object_summaries;
};

inline const char* GeometryObjectKindName(GeometryObjectKind kind)
{
    switch (kind)
    {
    case GeometryObjectKind::Roi:
        return "roi";
    case GeometryObjectKind::Line:
        return "line";
    case GeometryObjectKind::PointSet:
        return "pointset";
    case GeometryObjectKind::Mask:
        return "mask";
    case GeometryObjectKind::Boundary:
        return "boundary";
    case GeometryObjectKind::Keypoints:
        return "keypoints";
    case GeometryObjectKind::FractalPartition:
        return "fractal_partition";
    case GeometryObjectKind::DistanceField:
        return "distance_field";
    case GeometryObjectKind::Skeleton:
        return "skeleton";
    case GeometryObjectKind::Unknown:
    default:
        return "unknown";
    }
}

inline bool IsValidGeometryRef(const StableGeometryRef& ref)
{
    return !ref.object_id.empty() && ref.kind != GeometryObjectKind::Unknown;
}

inline bool IsSuitableAsTorchInputPrior(GeometryObjectKind kind)
{
    switch (kind)
    {
    case GeometryObjectKind::Roi:
    case GeometryObjectKind::Line:
    case GeometryObjectKind::PointSet:
    case GeometryObjectKind::Mask:
    case GeometryObjectKind::Boundary:
    case GeometryObjectKind::Keypoints:
    case GeometryObjectKind::FractalPartition:
    case GeometryObjectKind::DistanceField:
    case GeometryObjectKind::Skeleton:
        return true;
    case GeometryObjectKind::Unknown:
    default:
        return false;
    }
}

inline bool IsSuitableAsTrainingLabel(GeometryObjectKind kind)
{
    switch (kind)
    {
    case GeometryObjectKind::Roi:
    case GeometryObjectKind::Mask:
    case GeometryObjectKind::Boundary:
    case GeometryObjectKind::Keypoints:
        return true;
    case GeometryObjectKind::Line:
    case GeometryObjectKind::PointSet:
    case GeometryObjectKind::FractalPartition:
    case GeometryObjectKind::DistanceField:
    case GeometryObjectKind::Skeleton:
    case GeometryObjectKind::Unknown:
    default:
        return false;
    }
}

inline bool NeedsModelOutputAttach(GeometryObjectKind kind)
{
    switch (kind)
    {
    case GeometryObjectKind::Roi:
    case GeometryObjectKind::Line:
    case GeometryObjectKind::PointSet:
    case GeometryObjectKind::Mask:
    case GeometryObjectKind::Boundary:
    case GeometryObjectKind::Keypoints:
    case GeometryObjectKind::FractalPartition:
    case GeometryObjectKind::DistanceField:
    case GeometryObjectKind::Skeleton:
        return true;
    case GeometryObjectKind::Unknown:
    default:
        return false;
    }
}

inline const char* GeometryAtomicOperationName(GeometryAtomicOperation op)
{
    switch (op)
    {
    case GeometryAtomicOperation::Load:
        return "load";
    case GeometryAtomicOperation::Publish:
        return "publish";
    case GeometryAtomicOperation::Attach:
        return "attach";
    case GeometryAtomicOperation::Summarize:
        return "summarize";
    case GeometryAtomicOperation::ExportFeatureVector:
        return "export_feature_vector";
    case GeometryAtomicOperation::Unknown:
    default:
        return "unknown";
    }
}

inline const char* GeometryObjectScriptSuffix(GeometryObjectKind kind)
{
    switch (kind)
    {
    case GeometryObjectKind::Roi:
        return "roi_object";
    case GeometryObjectKind::Line:
        return "line_object";
    case GeometryObjectKind::PointSet:
        return "pointset_object";
    case GeometryObjectKind::Mask:
        return "mask_object";
    case GeometryObjectKind::Boundary:
        return "boundary_object";
    case GeometryObjectKind::Keypoints:
        return "keypoints_object";
    case GeometryObjectKind::FractalPartition:
        return "fractal_partition_object";
    case GeometryObjectKind::DistanceField:
        return "distance_field_object";
    case GeometryObjectKind::Skeleton:
        return "skeleton_object";
    case GeometryObjectKind::Unknown:
    default:
        return "unknown_object";
    }
}

inline bool SupportsAtomicOperation(GeometryObjectKind kind, GeometryAtomicOperation op);

inline std::string BuildGeometryAtomicCommandName(GeometryObjectKind kind, GeometryAtomicOperation op)
{
    if (!SupportsAtomicOperation(kind, op))
    {
        return std::string();
    }
    return std::string("cxcore.") + GeometryAtomicOperationName(op) + "." + GeometryObjectScriptSuffix(kind);
}

inline bool SupportsAtomicOperation(GeometryObjectKind kind, GeometryAtomicOperation op)
{
    switch (op)
    {
    case GeometryAtomicOperation::Load:
    case GeometryAtomicOperation::Publish:
    case GeometryAtomicOperation::Summarize:
        return kind != GeometryObjectKind::Unknown;

    case GeometryAtomicOperation::Attach:
        return NeedsModelOutputAttach(kind);

    case GeometryAtomicOperation::ExportFeatureVector:
        return kind == GeometryObjectKind::Line ||
            kind == GeometryObjectKind::PointSet ||
            kind == GeometryObjectKind::FractalPartition ||
            kind == GeometryObjectKind::DistanceField ||
            kind == GeometryObjectKind::Skeleton;

    case GeometryAtomicOperation::Unknown:
    default:
        return false;
    }
}

inline std::vector<std::string> GetAtomicOperationNamesForKind(GeometryObjectKind kind)
{
    std::vector<std::string> names;
    const GeometryAtomicOperation ops[] = {
        GeometryAtomicOperation::Load,
        GeometryAtomicOperation::Publish,
        GeometryAtomicOperation::Attach,
        GeometryAtomicOperation::Summarize,
        GeometryAtomicOperation::ExportFeatureVector
    };
    for (const GeometryAtomicOperation op : ops)
    {
        if (SupportsAtomicOperation(kind, op))
        {
            names.push_back(GeometryAtomicOperationName(op));
        }
    }
    return names;
}

inline std::vector<std::string> GetAtomicOperationNamesForTopologyBundle()
{
    return {
        GeometryAtomicOperationName(GeometryAtomicOperation::Load),
        GeometryAtomicOperationName(GeometryAtomicOperation::Publish),
        GeometryAtomicOperationName(GeometryAtomicOperation::Attach),
        GeometryAtomicOperationName(GeometryAtomicOperation::Summarize)
    };
}

inline bool ValidateMaskObject(const MaskObject& object)
{
    if (!IsValidGeometryRef(object.ref))
    {
        return false;
    }
    if (object.width <= 0 || object.height <= 0)
    {
        return false;
    }
    const auto expected = static_cast<std::size_t>(object.width) * static_cast<std::size_t>(object.height);
    return object.values.size() == expected;
}

inline bool ValidateBoundaryObject(const BoundaryObject& object)
{
    return IsValidGeometryRef(object.ref) && !object.contour.points.empty();
}

inline bool ValidateKeypointsObject(const KeypointsObject& object)
{
    return IsValidGeometryRef(object.ref) &&
        !object.points.empty() &&
        object.points.size() == object.visibility.size();
}

inline bool ValidateFractalPartitionObject(const FractalPartitionObject& object)
{
    return IsValidGeometryRef(object.ref) &&
        object.output.status == 1 &&
        object.output.width > 0 &&
        object.output.height > 0 &&
        object.output.node_count >= 0 &&
        static_cast<int>(object.output.nodes.size()) <= object.output.node_count;
}

inline bool ValidateDistanceFieldObject(const DistanceFieldObject& object)
{
    const auto expected = static_cast<std::size_t>(object.output.width) * static_cast<std::size_t>(object.output.height);
    return IsValidGeometryRef(object.ref) &&
        object.output.status == 1 &&
        object.output.width > 0 &&
        object.output.height > 0 &&
        (object.output.raster_distances.empty() || object.output.raster_distances.size() == expected);
}

inline bool ValidateSkeletonObject(const SkeletonObject& object)
{
    const auto expected = static_cast<std::size_t>(object.output.width) * static_cast<std::size_t>(object.output.height);
    return IsValidGeometryRef(object.ref) &&
        object.output.status == 1 &&
        object.output.width > 0 &&
        object.output.height > 0 &&
        object.output.skeleton_pixel_count >= 0 &&
        object.output.endpoint_count >= 0 &&
        object.output.branch_point_count >= 0 &&
        (object.output.skeleton_mask.empty() || object.output.skeleton_mask.size() == expected);
}

inline GeometryAttachRecord MakeRoiAttachRecord(
    const RoiObject& roi,
    const std::string& attach_id,
    const std::string& label,
    float score,
    const OutputRect& refined_bounds,
    const std::vector<float>& embedding = {})
{
    GeometryAttachRecord record;
    record.target = roi.ref;
    record.attach_id = attach_id;
    record.label = label;
    record.score = score;
    record.refined_bounds = refined_bounds;
    record.embedding = embedding;
    return record;
}

inline GeometryAttachRecord MakeMaskAttachRecord(
    const RoiObject& roi,
    const std::string& attach_id,
    const std::string& mask_object_id,
    const std::string& boundary_object_id,
    float score)
{
    GeometryAttachRecord record;
    record.target = roi.ref;
    record.attach_id = attach_id;
    record.score = score;
    record.refined_bounds = roi.bounds;
    record.mask_object_id = mask_object_id;
    record.boundary_object_id = boundary_object_id;
    return record;
}

inline GeometryAttachRecord MakeKeypointsAttachRecord(
    const RoiObject& roi,
    const std::string& attach_id,
    const std::string& keypoints_object_id,
    float score)
{
    GeometryAttachRecord record;
    record.target = roi.ref;
    record.attach_id = attach_id;
    record.score = score;
    record.refined_bounds = roi.bounds;
    record.keypoints_object_id = keypoints_object_id;
    return record;
}

inline GeometryAttachRecord MakeTopologyAttachRecord(
    const RoiObject& roi,
    const std::string& attach_id,
    const std::string& fractal_partition_object_id,
    const std::string& distance_field_object_id,
    const std::string& skeleton_object_id,
    float score)
{
    GeometryAttachRecord record;
    record.target = roi.ref;
    record.attach_id = attach_id;
    record.score = score;
    record.refined_bounds = roi.bounds;
    record.fractal_partition_object_id = fractal_partition_object_id;
    record.distance_field_object_id = distance_field_object_id;
    record.skeleton_object_id = skeleton_object_id;
    return record;
}

inline GeometryDisplayHint MakeDisplayHint(
    bool supports_2d_overlay,
    bool supports_3d_overlay,
    const std::string& overlay_object_id,
    const std::string& preview_ref)
{
    GeometryDisplayHint hint;
    hint.supports_2d_overlay = supports_2d_overlay;
    hint.supports_3d_overlay = supports_3d_overlay;
    hint.overlay_object_id = overlay_object_id;
    hint.preview_ref = preview_ref;
    return hint;
}

inline GeometryObjectSummary SummarizeFractalPartitionObject(const FractalPartitionObject& object)
{
    GeometryObjectSummary summary;
    summary.ref = object.ref;
    summary.summary = object.output.summary;
    summary.width = object.output.width;
    summary.height = object.output.height;
    summary.display = MakeDisplayHint(true, true, object.overlay_object_id, object.output.debug_preview_ref);
    summary.properties.push_back({ "node_count", std::to_string(object.output.node_count) });
    summary.properties.push_back({ "leaf_node_count", std::to_string(object.output.leaf_node_count) });
    summary.properties.push_back({ "boundary_node_count", std::to_string(object.output.boundary_node_count) });
    summary.properties.push_back({ "max_depth", std::to_string(object.output.max_depth) });
    return summary;
}

inline GeometryObjectSummary SummarizeDistanceFieldObject(const DistanceFieldObject& object)
{
    GeometryObjectSummary summary;
    summary.ref = object.ref;
    summary.summary = object.output.summary;
    summary.width = object.output.width;
    summary.height = object.output.height;
    summary.display = MakeDisplayHint(true, true, object.overlay_object_id, object.output.debug_heatmap_ref);
    summary.properties.push_back({ "seed_count", std::to_string(object.output.seed_count) });
    summary.properties.push_back({ "min_distance", std::to_string(object.output.min_distance) });
    summary.properties.push_back({ "max_distance", std::to_string(object.output.max_distance) });
    summary.properties.push_back({ "mean_distance", std::to_string(object.output.mean_distance) });
    return summary;
}

inline GeometryObjectSummary SummarizeSkeletonObject(const SkeletonObject& object)
{
    GeometryObjectSummary summary;
    summary.ref = object.ref;
    summary.summary = object.output.summary;
    summary.width = object.output.width;
    summary.height = object.output.height;
    summary.display = MakeDisplayHint(true, true, object.overlay_object_id, object.output.debug_overlay_ref);
    summary.properties.push_back({ "skeleton_pixel_count", std::to_string(object.output.skeleton_pixel_count) });
    summary.properties.push_back({ "endpoint_count", std::to_string(object.output.endpoint_count) });
    summary.properties.push_back({ "branch_point_count", std::to_string(object.output.branch_point_count) });
    return summary;
}

inline GeometryTopologyBundle MakeGeometryTopologyBundle(
    const RoiObject& roi,
    const FractalPartitionObject& partition,
    const DistanceFieldObject& distance,
    const SkeletonObject& skeleton,
    const std::string& bundle_id)
{
    GeometryTopologyBundle bundle;
    bundle.bundle_id = bundle_id;
    bundle.target_roi_ref = roi.ref;
    bundle.fractal_partition_object_id = partition.ref.object_id;
    bundle.distance_field_object_id = distance.ref.object_id;
    bundle.skeleton_object_id = skeleton.ref.object_id;
    return bundle;
}

inline bool ValidateGeometryTopologyBundle(const GeometryTopologyBundle& bundle)
{
    return !bundle.bundle_id.empty() &&
        IsValidGeometryRef(bundle.target_roi_ref) &&
        !bundle.fractal_partition_object_id.empty() &&
        !bundle.distance_field_object_id.empty() &&
        !bundle.skeleton_object_id.empty();
}

inline GeometryTopologyBundleSummary SummarizeGeometryTopologyBundle(
    const GeometryTopologyBundle& bundle,
    const FractalPartitionObject& partition,
    const DistanceFieldObject& distance,
    const SkeletonObject& skeleton)
{
    GeometryTopologyBundleSummary summary;
    summary.bundle_id = bundle.bundle_id;
    summary.target_roi_ref = bundle.target_roi_ref;
    summary.object_count = 3;
    summary.object_ids.push_back(bundle.fractal_partition_object_id);
    summary.object_ids.push_back(bundle.distance_field_object_id);
    summary.object_ids.push_back(bundle.skeleton_object_id);
    summary.object_summaries.push_back(SummarizeFractalPartitionObject(partition));
    summary.object_summaries.push_back(SummarizeDistanceFieldObject(distance));
    summary.object_summaries.push_back(SummarizeSkeletonObject(skeleton));
    return summary;
}

inline std::vector<std::string> BuildGeometryTopologyBundleCommandNames()
{
    return {
        "cxcore.load.topology_bundle",
        "cxcore.publish.topology_bundle",
        "cxcore.attach.topology_bundle",
        "cxcore.summarize.topology_bundle"
    };
}

inline cxgeom::CxGeomBounds MakeGeomBounds(const OutputRect& rect)
{
    cxgeom::CxGeomBounds bounds;
    bounds.min_x = rect.x;
    bounds.min_y = rect.y;
    bounds.min_z = 0.0;
    bounds.max_x = rect.x + rect.width;
    bounds.max_y = rect.y + rect.height;
    bounds.max_z = 0.0;
    bounds.valid = rect.width > 0.0 || rect.height > 0.0;
    return bounds;
}

inline cxgeom::CxShapeHandle MakeMetadataShape(int entity_id,
                                               const std::string& name,
                                               cxgeom::CxShapeKind kind)
{
    return cxgeom::CxShapeHandle(entity_id, name, kind);
}

inline cxgeom::CxGeomElement MakeGeomElementFromRoi(const RoiObject& roi, int entity_id)
{
    cxgeom::CxGeomElement element = cxgeom::CxGeomElementBody::MakeElement(
        MakeMetadataShape(entity_id, roi.ref.object_id, cxgeom::CxShapeKind::Face),
        true);
    element.entity_type = "roi";
    element.source_stage = "analysis";
    element.status = "ok";
    element.measure_shape = element.shape;
    element.bbox = MakeGeomBounds(roi.bounds);
    element.confidence = roi.score_hint;
    element.success = true;
    return element;
}

inline cxgeom::CxCurveElement MakeCurveElementFromLine(const LineObject& line, int entity_id)
{
    cxgeom::CxCurveElement element = cxgeom::CxGeomElementBody::MakeCurveElement(
        MakeMetadataShape(entity_id, line.ref.object_id, cxgeom::CxShapeKind::Curve),
        0.0,
        false,
        true);
    element.base.entity_type = "line";
    element.base.source_stage = "analysis";
    element.base.status = "ok";
    element.base.bbox = line.samples.bounds.width > 0.0 || line.samples.bounds.height > 0.0
        ? MakeGeomBounds(line.samples.bounds)
        : MakeGeomBounds(OutputRect{ line.start.x, line.start.y, line.end.x - line.start.x, line.end.y - line.start.y });
    element.base.confidence = line.score_hint;
    element.base.success = !line.samples.points.empty();
    element.curve_type = "line";
    element.start_point = gp_Pnt(line.start.x, line.start.y, 0.0);
    element.end_point = gp_Pnt(line.end.x, line.end.y, 0.0);
    element.center_point = gp_Pnt((line.start.x + line.end.x) * 0.5, (line.start.y + line.end.y) * 0.5, 0.0);
    element.length_hint = std::hypot(line.end.x - line.start.x, line.end.y - line.start.y);
    return element;
}

inline cxgeom::CxCurveElement MakeCurveElementFromCircle(const CircleMeasurementOutput& circle, int entity_id, const char* name)
{
    cxgeom::CxCurveElement element = cxgeom::CxGeomElementBody::MakeCurveElement(
        MakeMetadataShape(entity_id, name ? name : "circle", cxgeom::CxShapeKind::Curve),
        circle.radius > 0.0 ? (2.0 * 3.14159265358979323846 * circle.radius) : 0.0,
        true,
        true);
    element.base.entity_type = "circle";
    element.base.source_stage = "analysis";
    element.base.status = circle.has_direct_fit ? "ok" : "fallback";
    element.base.bbox = MakeGeomBounds(circle.measure_bounds);
    element.base.confidence = circle.has_direct_fit ? 1.0 : 0.5;
    element.base.success = !circle.sample_points.points.empty();
    element.curve_type = "circle";
    element.center_point = gp_Pnt(circle.center.x, circle.center.y, 0.0);
    element.length_hint = circle.radius > 0.0 ? (2.0 * 3.14159265358979323846 * circle.radius) : 0.0;
    element.closed = true;
    return element;
}

inline cxgeom::CxSurfaceElement MakeSurfaceElementFromRect(const OutputRect& rect,
                                                           int entity_id,
                                                           const char* name,
                                                           double confidence = 1.0)
{
    cxgeom::CxSurfaceElement element = cxgeom::CxGeomElementBody::MakeSurfaceElement(
        MakeMetadataShape(entity_id, name ? name : "rect", cxgeom::CxShapeKind::Face),
        rect.width * rect.height,
        true);
    element.base.entity_type = "rect";
    element.base.source_stage = "analysis";
    element.base.status = "ok";
    element.base.bbox = MakeGeomBounds(rect);
    element.base.confidence = confidence;
    element.base.success = rect.width > 0.0 && rect.height > 0.0;
    element.surface_type = "rect";
    element.outer_boundary = {
        gp_Pnt(rect.x, rect.y, 0.0),
        gp_Pnt(rect.x + rect.width, rect.y, 0.0),
        gp_Pnt(rect.x + rect.width, rect.y + rect.height, 0.0),
        gp_Pnt(rect.x, rect.y + rect.height, 0.0)
    };
    return element;
}

inline cxgeom::CxGeomBatchElement MakeBatchElementFromPoints(const PointSetOutput& pointset,
                                                             int batch_id,
                                                             const char* name,
                                                             const char* element_type)
{
    cxgeom::CxGeomBatchElement batch = cxgeom::CxGeomElementBody::MakeBatchElement(batch_id, name, {}, true);
    batch.element_type = element_type ? element_type : "pointset";
    batch.source_stage = "analysis";
    batch.status = "ok";
    batch.bbox = MakeGeomBounds(pointset.bounds);
    batch.confidence = pointset.points.empty() ? 0.0 : 1.0;
    batch.success = !pointset.points.empty();
    batch.source_count = pointset.points.size();
    return batch;
}

inline cxgeom::CxGeomAnnotation MakeAnnotationForElement(int annotation_id,
                                                         const cxgeom::CxGeomElement& target,
                                                         const char* text,
                                                         double anchor_x,
                                                         double anchor_y,
                                                         double anchor_z = 0.0)
{
    return cxgeom::CxGeomAnnotationBody::MakeAnnotation(
        annotation_id,
        target.entity_id,
        0,
        text,
        anchor_x,
        anchor_y,
        anchor_z,
        true);
}

inline cxgeom::CxGeomAnnotation MakeAnnotationForBatch(int annotation_id,
                                                       const cxgeom::CxGeomBatchElement& target,
                                                       const char* text,
                                                       double anchor_x,
                                                       double anchor_y,
                                                       double anchor_z = 0.0)
{
    return cxgeom::CxGeomAnnotationBody::MakeAnnotation(
        annotation_id,
        0,
        target.batch_id,
        text,
        anchor_x,
        anchor_y,
        anchor_z,
        true);
}

} // namespace cxcore

#endif
