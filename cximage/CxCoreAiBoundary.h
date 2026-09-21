#ifndef CXCORE_CORE_CXCOREAIBOUNDARY_H
#define CXCORE_CORE_CXCOREAIBOUNDARY_H

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "CxCoreBoundary.h"
#include "CxCoreGeometryAttach.h"
#include "GridPatternClassNet.h"
#include "RegionPatternNet.h"

namespace cxcore {

enum class AiTaskKind
{
    Unknown = 0,
    DirectMeasurement,
    NumericClassification,
    NumericClustering,
    GeometryMatching,
    GeometryClustering,
    RegionClassification,
    RegionDetectionRefinement,
    DenseSegmentation,
    VisualEmbedding
};

enum class AiRoute
{
    StayInCxcore = 0,
    RouteToMlpack,
    UpgradeToTorchModule,
    ManualReview
};

enum class AiExecutionMode
{
    Infer = 0,
    Train
};

struct TensorLayout
{
    int width = 0;
    int height = 0;
    int channels = 0;
    int batch = 0;
};

struct TensorBufferView
{
    TensorLayout layout;
    std::vector<float> values;
    bool normalized = false;
};

struct RoiInput
{
    OutputRect bounds;
    float confidence = 0.0f;
    int label_hint = -1;
};

struct FeatureVectorInput
{
    std::string name;
    std::string role;
    std::string source;
    std::vector<float> values;
};

struct GeometrySignalInput
{
    LineMeasurementOutput line;
    CircleMeasurementOutput circle;
    EllipseMeasurementOutput ellipse;
    MatchOutput match;
    bool has_line = false;
    bool has_circle = false;
    bool has_ellipse = false;
    bool has_match = false;
};

struct AiTaskEnvelope
{
    AiTaskKind task = AiTaskKind::Unknown;
    AiExecutionMode mode = AiExecutionMode::Infer;
    bool requires_end_to_end_learning = false;
    bool requires_dense_spatial_output = false;
    bool requires_classical_explainability = false;
    TensorBufferView image_tensor;
    std::vector<RoiInput> proposals;
    std::vector<FeatureVectorInput> descriptors;
    GeometrySignalInput geometry;
    int class_count = 0;
    int topk = 1;
};

struct EvidenceDescriptorSummary
{
    std::string name;
    std::string role;
    std::string source;
    int dim = 0;
};

struct AiRouteDecision
{
    AiRoute route = AiRoute::ManualReview;
    const char* reason = "task boundary is incomplete";
};

struct AiOutputRecord
{
    int label = -1;
    float score = 0.0f;
    OutputRect bounds;
    std::vector<float> embedding;
};

struct AiTaskResult
{
    AiRoute route = AiRoute::ManualReview;
    AiTaskKind task = AiTaskKind::Unknown;
    std::vector<AiOutputRecord> outputs;
    std::vector<float> losses;
    std::vector<std::string> diagnostics;
};

inline const char* RegionPatternFeatureVectorName()
{
    return "region_pattern_descriptor";
}

inline const char* GridPatternHierarchyFeatureVectorName()
{
    return "grid_pattern_hierarchy_descriptor";
}

inline const char* BaselineFeatureVectorName()
{
    return "baseline_feature_v1";
}

inline const char* FastMatchFeatureVectorName()
{
    return "fastmatch_structural_feature";
}

inline const char* FractalPartitionFeatureVectorName()
{
    return "fractal_partition_feature";
}

inline const char* GeometryDistanceFieldFeatureVectorName()
{
    return "geometry_distance_field_feature";
}

inline const char* GeometrySkeletonFeatureVectorName()
{
    return "geometry_skeleton_feature";
}

inline const char* GeometryTopologyBundleFeatureVectorName()
{
    return "geometry_topology_bundle_feature";
}

inline const char* RegionPatternFeatureRole()
{
    return "content_aux";
}

inline const char* GridPatternHierarchyFeatureRole()
{
    return "class_hierarchy_primary";
}

inline const char* BaselineFeatureRole()
{
    return "baseline_summary";
}

inline const char* FastMatchFeatureRole()
{
    return "structural_primary";
}

inline const char* FractalPartitionFeatureRole()
{
    return "geometry_partition_aux";
}

inline const char* GeometryDistanceFieldFeatureRole()
{
    return "geometry_distance_aux";
}

inline const char* GeometrySkeletonFeatureRole()
{
    return "geometry_skeleton_aux";
}

inline const char* GeometryTopologyBundleFeatureRole()
{
    return "geometry_topology_bundle";
}

inline const char* RegionPatternFeatureSource()
{
    return "region_pattern_net";
}

inline const char* GridPatternHierarchyFeatureSource()
{
    return "GridPatternClassNet";
}

inline const char* BaselineFeatureSource()
{
    return "baseline_feature_sample_v1";
}

inline const char* FastMatchFeatureSource()
{
    return "FastMatch";
}

inline const char* FractalPartitionFeatureSource()
{
    return "fractal_partition";
}

inline const char* GeometryDistanceFieldFeatureSource()
{
    return "geometry_distance_field";
}

inline const char* GeometrySkeletonFeatureSource()
{
    return "geometry_skeleton";
}

inline const char* GeometryTopologyBundleFeatureSource()
{
    return "geometry_topology_bundle";
}

inline const char* GeometryTopologyBundleEnvelopeBuildCommandName()
{
    return "cxcore.build.topology_bundle_envelope";
}

inline const char* GeometryTopologyBundleEnvelopeRouteCommandName()
{
    return "cxcore.route.topology_bundle_envelope";
}

inline const char* GeometryTopologyBundleEnvelopeSummarizeCommandName()
{
    return "cxcore.summarize.topology_bundle_envelope";
}

inline std::vector<std::string> GetGeometryTopologyBundleEnvelopeCommandNames()
{
    return {
        GeometryTopologyBundleEnvelopeBuildCommandName(),
        GeometryTopologyBundleEnvelopeRouteCommandName(),
        GeometryTopologyBundleEnvelopeSummarizeCommandName()
    };
}

inline const char* StructuralEvidenceBundleFeatureVectorName()
{
    return "structural_evidence_bundle_feature";
}

inline const char* StructuralEvidenceBundleFeatureRole()
{
    return "structural_bundle";
}

inline const char* StructuralEvidenceBundleFeatureSource()
{
    return "fastmatch_plus_topology_bundle";
}

inline const char* StructuralEvidenceBundleEnvelopeBuildCommandName()
{
    return "cxcore.build.structural_evidence_bundle_envelope";
}

inline const char* StructuralEvidenceBundleEnvelopeRouteCommandName()
{
    return "cxcore.route.structural_evidence_bundle_envelope";
}

inline const char* StructuralEvidenceBundleEnvelopeSummarizeCommandName()
{
    return "cxcore.summarize.structural_evidence_bundle_envelope";
}

inline std::vector<std::string> GetStructuralEvidenceBundleEnvelopeCommandNames()
{
    return {
        StructuralEvidenceBundleEnvelopeBuildCommandName(),
        StructuralEvidenceBundleEnvelopeRouteCommandName(),
        StructuralEvidenceBundleEnvelopeSummarizeCommandName()
    };
}

inline const char* AiTaskKindName(AiTaskKind task)
{
    switch (task)
    {
    case AiTaskKind::DirectMeasurement:
        return "direct_measurement";
    case AiTaskKind::NumericClassification:
        return "numeric_classification";
    case AiTaskKind::NumericClustering:
        return "numeric_clustering";
    case AiTaskKind::GeometryMatching:
        return "geometry_matching";
    case AiTaskKind::GeometryClustering:
        return "geometry_clustering";
    case AiTaskKind::RegionClassification:
        return "region_classification";
    case AiTaskKind::RegionDetectionRefinement:
        return "region_detection_refinement";
    case AiTaskKind::DenseSegmentation:
        return "dense_segmentation";
    case AiTaskKind::VisualEmbedding:
        return "visual_embedding";
    case AiTaskKind::Unknown:
    default:
        return "unknown";
    }
}

inline const char* AiRouteName(AiRoute route)
{
    switch (route)
    {
    case AiRoute::StayInCxcore:
        return "stay_in_cxcore";
    case AiRoute::RouteToMlpack:
        return "route_to_mlpack";
    case AiRoute::UpgradeToTorchModule:
        return "upgrade_to_torch_module";
    case AiRoute::ManualReview:
    default:
        return "manual_review";
    }
}

inline bool HasImageTensor(const AiTaskEnvelope& envelope)
{
    return envelope.image_tensor.layout.width > 0 &&
        envelope.image_tensor.layout.height > 0 &&
        envelope.image_tensor.layout.channels > 0 &&
        envelope.image_tensor.layout.batch > 0 &&
        !envelope.image_tensor.values.empty();
}

inline bool HasDescriptors(const AiTaskEnvelope& envelope)
{
    for (const auto& item : envelope.descriptors)
    {
        if (!item.values.empty())
        {
            return true;
        }
    }
    return false;
}

inline bool HasGeometrySignals(const AiTaskEnvelope& envelope)
{
    return envelope.geometry.has_line ||
        envelope.geometry.has_circle ||
        envelope.geometry.has_ellipse ||
        envelope.geometry.has_match;
}

inline bool ValidateAiTaskEnvelope(const AiTaskEnvelope& envelope, std::string* error = nullptr)
{
    const bool has_image = HasImageTensor(envelope);
    const bool has_descriptors = HasDescriptors(envelope);
    const bool has_geometry = HasGeometrySignals(envelope);
    const bool has_any_signal = has_image || has_descriptors || has_geometry || !envelope.proposals.empty();

    if (!has_any_signal)
    {
        if (error) *error = "AiTaskEnvelope requires image, roi, descriptor, or geometry input";
        return false;
    }

    if (has_image)
    {
        const auto& layout = envelope.image_tensor.layout;
        const int64_t expected = static_cast<int64_t>(layout.batch) * layout.channels * layout.height * layout.width;
        if (expected <= 0 || static_cast<int64_t>(envelope.image_tensor.values.size()) != expected)
        {
            if (error) *error = "AiTaskEnvelope image tensor size does not match BCHW layout";
            return false;
        }
    }

    if (envelope.mode == AiExecutionMode::Train && envelope.class_count <= 0)
    {
        if (error) *error = "training tasks require a positive class_count";
        return false;
    }

    if (envelope.topk <= 0)
    {
        if (error) *error = "topk must be positive";
        return false;
    }

    return true;
}

inline AiRouteDecision DecideAiRoute(const AiTaskEnvelope& envelope)
{
    const bool has_image = HasImageTensor(envelope);
    const bool has_descriptors = HasDescriptors(envelope);
    const bool has_geometry = HasGeometrySignals(envelope);

    switch (envelope.task)
    {
    case AiTaskKind::DirectMeasurement:
        return { AiRoute::StayInCxcore, "direct measurement stays in cxcore" };

    case AiTaskKind::NumericClassification:
    case AiTaskKind::NumericClustering:
    case AiTaskKind::GeometryMatching:
    case AiTaskKind::GeometryClustering:
        if (!has_image && (has_descriptors || has_geometry) && !envelope.requires_end_to_end_learning)
        {
            return { AiRoute::RouteToMlpack, "structured descriptor or geometry task routes to mlpack" };
        }
        if (!has_image && has_geometry && envelope.requires_classical_explainability)
        {
            return { AiRoute::StayInCxcore, "classical explainability requirement keeps task in cxcore" };
        }
        return { AiRoute::ManualReview, "structured task mixes incompatible upgrade signals" };

    case AiTaskKind::RegionClassification:
    case AiTaskKind::RegionDetectionRefinement:
    case AiTaskKind::DenseSegmentation:
    case AiTaskKind::VisualEmbedding:
        if (has_image)
        {
            return { AiRoute::UpgradeToTorchModule, "image-backed learning task upgrades to torch_module" };
        }
        return { AiRoute::ManualReview, "visual task requires image tensor input from cxcore" };

    case AiTaskKind::Unknown:
    default:
        break;
    }

    if (has_image && envelope.requires_end_to_end_learning)
    {
        return { AiRoute::UpgradeToTorchModule, "end-to-end image learning upgrades to torch_module" };
    }

    if (!has_image && (has_descriptors || has_geometry))
    {
        return { AiRoute::RouteToMlpack, "non-visual structured task routes to mlpack" };
    }

    return { AiRoute::ManualReview, "task boundary requires explicit route selection" };
}

inline AiTaskEnvelope MakeRegionClassificationEnvelope(
    const TensorBufferView& image_tensor,
    const std::vector<RoiInput>& proposals,
    const std::vector<FeatureVectorInput>& descriptors = {})
{
    AiTaskEnvelope envelope;
    envelope.task = AiTaskKind::RegionClassification;
    envelope.mode = AiExecutionMode::Infer;
    envelope.image_tensor = image_tensor;
    envelope.proposals = proposals;
    envelope.descriptors = descriptors;
    envelope.topk = 1;
    return envelope;
}

inline AiTaskEnvelope MakeStructuredGeometryEnvelope(
    AiTaskKind task,
    const GeometrySignalInput& geometry,
    const std::vector<FeatureVectorInput>& descriptors,
    bool requires_classical_explainability = false)
{
    AiTaskEnvelope envelope;
    envelope.task = task;
    envelope.mode = AiExecutionMode::Infer;
    envelope.geometry = geometry;
    envelope.descriptors = descriptors;
    envelope.requires_classical_explainability = requires_classical_explainability;
    envelope.topk = 1;
    return envelope;
}

inline FeatureVectorInput MakeRegionPatternFeatureVector(
    const RegionPatternDescriptor& descriptor,
    const std::string& name = RegionPatternFeatureVectorName(),
    bool include_foreground_ratio = true)
{
    FeatureVectorInput input;
    input.name = name;
    input.role = RegionPatternFeatureRole();
    input.source = RegionPatternFeatureSource();
    input.values.reserve(descriptor.values.size() + (include_foreground_ratio ? 1u : 0u));
    if (include_foreground_ratio)
    {
        input.values.push_back(static_cast<float>(descriptor.global_foreground_ratio));
    }
    for (const double value : descriptor.values)
    {
        input.values.push_back(static_cast<float>(value));
    }
    return input;
}

inline FeatureVectorInput MakeGridPatternHierarchyFeatureVector(
    const GridPatternHierarchy& hierarchy,
    const std::string& name = GridPatternHierarchyFeatureVectorName())
{
    FeatureVectorInput input;
    input.name = name;
    input.role = GridPatternHierarchyFeatureRole();
    input.source = GridPatternHierarchyFeatureSource();
    input.values.reserve(hierarchy.descriptor.size());
    for (const double value : hierarchy.descriptor)
    {
        input.values.push_back(static_cast<float>(value));
    }
    return input;
}

inline FeatureVectorInput MakeBaselineFeatureVector(
    const BaselineFeatureSampleV1& sample,
    const std::string& name = BaselineFeatureVectorName())
{
    FeatureVectorInput input;
    input.name = name;
    input.role = BaselineFeatureRole();
    input.source = BaselineFeatureSource();
    input.values = {
        static_cast<float>(sample.roi_area),
        static_cast<float>(sample.component_count),
        static_cast<float>(sample.largest_component_ratio),
        static_cast<float>(sample.line_w_points_count),
        static_cast<float>(sample.circle_radius),
        static_cast<float>(sample.match_candidate_count),
        static_cast<float>(sample.match_best_score),
        static_cast<float>(sample.image_model_score),
        static_cast<float>(sample.region_pattern_foreground_ratio),
        static_cast<float>(sample.region_pattern_descriptor_dim),
        static_cast<float>(sample.region_pattern_descriptor_mean),
        static_cast<float>(sample.region_pattern_descriptor_std)
    };
    return input;
}

inline FeatureVectorInput MakeFastMatchFeatureVector(
    const MatchOutput& match,
    const std::string& name = FastMatchFeatureVectorName())
{
    const MatchCandidateOutput* primary = match.candidates.empty() ? nullptr : &match.candidates.front();

    FeatureVectorInput input;
    input.name = name;
    input.role = FastMatchFeatureRole();
    input.source = FastMatchFeatureSource();
    input.values = {
        static_cast<float>(match.candidates.size()),
        static_cast<float>(match.max_score),
        static_cast<float>(match.image_model_score),
        primary ? static_cast<float>(primary->score) : 0.0f,
        primary ? static_cast<float>(primary->center.x) : 0.0f,
        primary ? static_cast<float>(primary->center.y) : 0.0f,
        primary ? static_cast<float>(primary->bounds.width) : 0.0f,
        primary ? static_cast<float>(primary->bounds.height) : 0.0f
    };
    return input;
}

inline FeatureVectorInput MakeFractalPartitionFeatureVector(
    const FractalPartitionOutput& partition,
    const std::string& name = FractalPartitionFeatureVectorName())
{
    FeatureVectorInput input;
    input.name = name;
    input.role = FractalPartitionFeatureRole();
    input.source = FractalPartitionFeatureSource();
    input.values = {
        static_cast<float>(partition.node_count),
        static_cast<float>(partition.leaf_node_count),
        static_cast<float>(partition.boundary_node_count),
        static_cast<float>(partition.max_depth),
        static_cast<float>(partition.width),
        static_cast<float>(partition.height)
    };
    return input;
}

inline FeatureVectorInput MakeGeometryDistanceFieldFeatureVector(
    const GeometryDistanceFieldOutput& distance,
    const std::string& name = GeometryDistanceFieldFeatureVectorName())
{
    FeatureVectorInput input;
    input.name = name;
    input.role = GeometryDistanceFieldFeatureRole();
    input.source = GeometryDistanceFieldFeatureSource();
    input.values = {
        static_cast<float>(distance.seed_count),
        static_cast<float>(distance.min_distance),
        static_cast<float>(distance.max_distance),
        static_cast<float>(distance.mean_distance),
        static_cast<float>(distance.width),
        static_cast<float>(distance.height)
    };
    return input;
}

inline FeatureVectorInput MakeGeometrySkeletonFeatureVector(
    const GeometrySkeletonOutput& skeleton,
    const std::string& name = GeometrySkeletonFeatureVectorName())
{
    FeatureVectorInput input;
    input.name = name;
    input.role = GeometrySkeletonFeatureRole();
    input.source = GeometrySkeletonFeatureSource();
    input.values = {
        static_cast<float>(skeleton.skeleton_pixel_count),
        static_cast<float>(skeleton.endpoint_count),
        static_cast<float>(skeleton.branch_point_count),
        static_cast<float>(skeleton.width),
        static_cast<float>(skeleton.height)
    };
    return input;
}

inline FeatureVectorInput MakeGeometryTopologyBundleSummaryFeatureVector(
    const GeometryTopologyBundleSummary& summary,
    const std::string& name = GeometryTopologyBundleFeatureVectorName())
{
    FeatureVectorInput input;
    input.name = name;
    input.role = GeometryTopologyBundleFeatureRole();
    input.source = GeometryTopologyBundleFeatureSource();
    input.values = {
        static_cast<float>(summary.object_count),
        static_cast<float>(summary.object_summaries.size()),
        static_cast<float>(summary.object_ids.size())
    };
    return input;
}

inline FeatureVectorInput MakeStructuralEvidenceBundleFeatureVector(
    const MatchOutput& match,
    const GeometryTopologyBundleSummary& summary,
    const std::string& name = StructuralEvidenceBundleFeatureVectorName())
{
    FeatureVectorInput input;
    input.name = name;
    input.role = StructuralEvidenceBundleFeatureRole();
    input.source = StructuralEvidenceBundleFeatureSource();
    input.values = {
        static_cast<float>(match.candidates.size()),
        static_cast<float>(match.max_score),
        static_cast<float>(match.image_model_score),
        static_cast<float>(summary.object_count),
        static_cast<float>(summary.object_summaries.size())
    };
    return input;
}

inline AiTaskEnvelope MakeGeometryContentEnvelope(
    AiTaskKind task,
    const GeometrySignalInput& geometry,
    const RegionPatternDescriptor& descriptor,
    bool requires_classical_explainability = false)
{
    std::vector<FeatureVectorInput> descriptors;
    descriptors.push_back(MakeRegionPatternFeatureVector(descriptor));
    return MakeStructuredGeometryEnvelope(task, geometry, descriptors, requires_classical_explainability);
}

inline AiTaskEnvelope MakeGeometryContentEnvelopeFromPatch(
    AiTaskKind task,
    const GeometrySignalInput& geometry,
    const cv::Mat& roi_patch,
    bool requires_classical_explainability = false,
    const RegionPatternConfig& config = RegionPatternConfig())
{
    RegionPatternNet net;
    net.SetConfig(config);
    return MakeGeometryContentEnvelope(
        task,
        geometry,
        net.BuildDescriptor(roi_patch),
        requires_classical_explainability);
}

inline AiTaskEnvelope MakeRegionClassificationEnvelopeFromPatch(
    const TensorBufferView& image_tensor,
    const std::vector<RoiInput>& proposals,
    const cv::Mat& roi_patch,
    const RegionPatternConfig& config = RegionPatternConfig())
{
    RegionPatternNet net;
    net.SetConfig(config);
    std::vector<FeatureVectorInput> descriptors;
    descriptors.push_back(MakeRegionPatternFeatureVector(net.BuildDescriptor(roi_patch)));
    return MakeRegionClassificationEnvelope(image_tensor, proposals, descriptors);
}

inline AiTaskEnvelope MakeBaselineFeatureEnvelope(
    AiTaskKind task,
    const BaselineFeatureSampleV1& sample,
    bool requires_classical_explainability = false)
{
    AiTaskEnvelope envelope;
    envelope.task = task;
    envelope.mode = AiExecutionMode::Infer;
    envelope.descriptors.push_back(MakeBaselineFeatureVector(sample));
    envelope.requires_classical_explainability = requires_classical_explainability;
    envelope.topk = 1;
    return envelope;
}

inline AiTaskEnvelope MakeFastMatchEnvelope(
    AiTaskKind task,
    const MatchOutput& match,
    bool requires_classical_explainability = true)
{
    AiTaskEnvelope envelope;
    envelope.task = task;
    envelope.mode = AiExecutionMode::Infer;
    envelope.requires_classical_explainability = requires_classical_explainability;
    envelope.descriptors.push_back(MakeFastMatchFeatureVector(match));
    envelope.geometry.match = match;
    envelope.geometry.has_match = true;
    envelope.topk = 1;
    return envelope;
}

inline AiTaskEnvelope MakeFastMatchGridClassEnvelope(
    AiTaskKind task,
    const MatchOutput& match,
    const GridPatternHierarchy& hierarchy,
    bool requires_classical_explainability = true)
{
    AiTaskEnvelope envelope;
    envelope.task = task;
    envelope.mode = AiExecutionMode::Infer;
    envelope.requires_classical_explainability = requires_classical_explainability;
    envelope.descriptors.push_back(MakeFastMatchFeatureVector(match));
    envelope.descriptors.push_back(MakeGridPatternHierarchyFeatureVector(hierarchy));
    envelope.geometry.match = match;
    envelope.geometry.has_match = true;
    envelope.topk = 1;
    return envelope;
}

inline AiTaskEnvelope MakeCombinedEvidenceEnvelope(
    AiTaskKind task,
    const MatchOutput& match,
    const BaselineFeatureSampleV1& sample,
    const RegionPatternDescriptor& descriptor,
    bool requires_classical_explainability = false)
{
    AiTaskEnvelope envelope;
    envelope.task = task;
    envelope.mode = AiExecutionMode::Infer;
    envelope.requires_classical_explainability = requires_classical_explainability;
    envelope.descriptors.push_back(MakeFastMatchFeatureVector(match));
    envelope.descriptors.push_back(MakeBaselineFeatureVector(sample));
    envelope.descriptors.push_back(MakeRegionPatternFeatureVector(descriptor));
    envelope.geometry.match = match;
    envelope.geometry.has_match = true;
    envelope.topk = 1;
    return envelope;
}

inline AiTaskEnvelope MakeCombinedEvidenceEnvelopeFromPatch(
    AiTaskKind task,
    const MatchOutput& match,
    const BaselineFeatureSampleV1& sample,
    const cv::Mat& roi_patch,
    bool requires_classical_explainability = false,
    const RegionPatternConfig& config = RegionPatternConfig())
{
    RegionPatternNet net;
    net.SetConfig(config);
    return MakeCombinedEvidenceEnvelope(
        task,
        match,
        sample,
        net.BuildDescriptor(roi_patch),
        requires_classical_explainability);
}

inline AiTaskEnvelope MakeGeometryTopologyEvidenceEnvelope(
    AiTaskKind task,
    const FractalPartitionOutput& partition,
    const GeometryDistanceFieldOutput& distance,
    const GeometrySkeletonOutput& skeleton,
    bool requires_classical_explainability = false)
{
    AiTaskEnvelope envelope;
    envelope.task = task;
    envelope.mode = AiExecutionMode::Infer;
    envelope.requires_classical_explainability = requires_classical_explainability;
    envelope.descriptors.push_back(MakeFractalPartitionFeatureVector(partition));
    envelope.descriptors.push_back(MakeGeometryDistanceFieldFeatureVector(distance));
    envelope.descriptors.push_back(MakeGeometrySkeletonFeatureVector(skeleton));
    envelope.topk = 1;
    return envelope;
}

inline AiTaskEnvelope MakeGeometryTopologyBundleEnvelope(
    AiTaskKind task,
    const GeometryTopologyBundle& bundle,
    const FractalPartitionObject& partition,
    const DistanceFieldObject& distance,
    const SkeletonObject& skeleton,
    bool requires_classical_explainability = false)
{
    const GeometryTopologyBundleSummary summary =
        SummarizeGeometryTopologyBundle(bundle, partition, distance, skeleton);

    AiTaskEnvelope envelope;
    envelope.task = task;
    envelope.mode = AiExecutionMode::Infer;
    envelope.requires_classical_explainability = requires_classical_explainability;
    envelope.descriptors.push_back(MakeGeometryTopologyBundleSummaryFeatureVector(summary));
    envelope.descriptors.push_back(MakeFractalPartitionFeatureVector(partition.output));
    envelope.descriptors.push_back(MakeGeometryDistanceFieldFeatureVector(distance.output));
    envelope.descriptors.push_back(MakeGeometrySkeletonFeatureVector(skeleton.output));
    envelope.topk = 1;
    return envelope;
}

inline AiTaskEnvelope MakeStructuralEvidenceBundleEnvelope(
    AiTaskKind task,
    const MatchOutput& match,
    const GeometryTopologyBundle& bundle,
    const FractalPartitionObject& partition,
    const DistanceFieldObject& distance,
    const SkeletonObject& skeleton,
    bool requires_classical_explainability = true)
{
    const GeometryTopologyBundleSummary summary =
        SummarizeGeometryTopologyBundle(bundle, partition, distance, skeleton);

    AiTaskEnvelope envelope;
    envelope.task = task;
    envelope.mode = AiExecutionMode::Infer;
    envelope.requires_classical_explainability = requires_classical_explainability;
    envelope.descriptors.push_back(MakeStructuralEvidenceBundleFeatureVector(match, summary));
    envelope.descriptors.push_back(MakeFastMatchFeatureVector(match));
    envelope.descriptors.push_back(MakeGeometryTopologyBundleSummaryFeatureVector(summary));
    envelope.descriptors.push_back(MakeFractalPartitionFeatureVector(partition.output));
    envelope.descriptors.push_back(MakeGeometryDistanceFieldFeatureVector(distance.output));
    envelope.descriptors.push_back(MakeGeometrySkeletonFeatureVector(skeleton.output));
    envelope.geometry.match = match;
    envelope.geometry.has_match = true;
    envelope.topk = 1;
    return envelope;
}

inline std::vector<std::string> GetDescriptorNames(const AiTaskEnvelope& envelope)
{
    std::vector<std::string> names;
    names.reserve(envelope.descriptors.size());
    for (const auto& item : envelope.descriptors)
    {
        names.push_back(item.name);
    }
    return names;
}

inline std::vector<std::string> GetDescriptorRoles(const AiTaskEnvelope& envelope)
{
    std::vector<std::string> roles;
    roles.reserve(envelope.descriptors.size());
    for (const auto& item : envelope.descriptors)
    {
        roles.push_back(item.role);
    }
    return roles;
}

inline std::vector<std::string> GetDescriptorSources(const AiTaskEnvelope& envelope)
{
    std::vector<std::string> sources;
    sources.reserve(envelope.descriptors.size());
    for (const auto& item : envelope.descriptors)
    {
        sources.push_back(item.source);
    }
    return sources;
}

inline std::vector<EvidenceDescriptorSummary> BuildDescriptorSummary(const AiTaskEnvelope& envelope)
{
    std::vector<EvidenceDescriptorSummary> summary;
    summary.reserve(envelope.descriptors.size());
    for (const auto& item : envelope.descriptors)
    {
        EvidenceDescriptorSummary entry;
        entry.name = item.name;
        entry.role = item.role;
        entry.source = item.source;
        entry.dim = static_cast<int>(item.values.size());
        summary.push_back(entry);
    }
    return summary;
}

inline bool HasDescriptorRole(const AiTaskEnvelope& envelope, const std::string& role)
{
    for (const auto& item : envelope.descriptors)
    {
        if (item.role == role)
        {
            return true;
        }
    }
    return false;
}

inline bool HasDescriptorNamed(const AiTaskEnvelope& envelope, const std::string& name)
{
    for (const auto& item : envelope.descriptors)
    {
        if (item.name == name)
        {
            return true;
        }
    }
    return false;
}

inline AiTaskResult MakeEmptyAiTaskResult(const AiTaskEnvelope& envelope, AiRoute route)
{
    AiTaskResult result;
    result.route = route;
    result.task = envelope.task;
    return result;
}

} // namespace cxcore

#endif
