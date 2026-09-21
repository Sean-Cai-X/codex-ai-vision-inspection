#ifndef CXCORE_CORE_CXCORETORCHHANDOFFBRIDGE_H
#define CXCORE_CORE_CXCORETORCHHANDOFFBRIDGE_H

#include <string>

#include "CxCoreAiBoundary.h"
#include "../../libtorch_module/torch_test_host.h"

namespace cxcore {

enum class TorchMeasurementKind
{
    Unknown = 0,
    Line,
    Circle,
    Ellipse,
    Boundary
};

enum class TorchHandoffRouteState
{
    Unknown = 0,
    StayInCxcore,
    RouteToMlpack,
    HandoffToEnsmallen,
    UpgradeToTorchModule,
    ManualReview
};

enum class CxcoreInternalAcceptanceStage
{
    Unknown = 0,
    ModularManualUi,
    RemoteAiSemantic,
    AtomicSemantic
};

enum class CxcoreInternalPreparationStage
{
    Unknown = 0,
    CodeCleaning
};

struct TorchHandoffMetadata
{
    std::string source_hash;
    std::string result_ref;
    std::string evidence_ref;
    std::string log_path;
    std::string model_version;
    std::string next_action;
};

struct TorchGeometrySemanticRefRecord
{
    StableGeometryRef ref;
    std::string role;
};

struct TorchMeasurementRefRecord
{
    std::string ref;
    TorchMeasurementKind kind = TorchMeasurementKind::Unknown;
    bool is_direct_measurement = false;
};

struct TorchHandoffSummary
{
    std::string handoff_type;
    std::string summary;
    std::vector<std::string> refs;
};

struct TorchHandoffManifestEntry
{
    std::string name;
    std::string role;
    std::string source;
    int dim = 0;
};

struct TorchHandoffTaskSnapshot
{
    std::string handoff_type;
    std::string primary_ref;
    std::string route_hint;
    TorchHandoffRouteState route_state = TorchHandoffRouteState::Unknown;
    std::string source_hash;
    std::string result_ref;
    std::string evidence_ref;
};

struct TorchFeatureSemanticEvidenceStatus
{
    bool template_alignment_exported = false;
    bool roi_diff_candidate_exported = false;
    bool prior_roi_region_exported = false;
    bool roi_crop_packet_exported = false;
    bool chain_established = false;
    bool algorithm_effect_established = false;
    bool manual_review_required = true;
};

struct TorchGeometryEvidenceStatus
{
    bool bbox_candidate_list_exported = false;
    bool chain_established = false;
    bool manual_review_required = true;
};

struct TorchPriorityBridgeMirrorStatus
{
    bool bbox_candidate_list_exported = false;
    bool prior_roi_region_exported = false;
    bool roi_crop_packet_exported = false;
    bool chain_established = false;
    bool algorithm_effect_established = false;
    bool manual_review_required = true;
};

struct TorchGeometryHandoffBridgeRecord
{
    TorchHandoffMetadata metadata;
    StableGeometryRef bbox;
    StableGeometryRef mask;
    StableGeometryRef roi;
    StableGeometryRef region;
    StableGeometryRef contour;
    TorchGeometrySemanticRefRecord geometry;
    TorchMeasurementRefRecord measurement;
    std::string bbox_candidate_list_ref;
};

struct TorchFeatureSemanticHandoffBridgeRecord
{
    TorchHandoffMetadata metadata;
    StableGeometryRef roi;
    TorchGeometrySemanticRefRecord geometry;
    std::string roi_stats_ref;
    std::string embedding_ref;
    std::string feature_vector_ref;
    std::string feature_set_ref;
    int feature_dim = 0;
    std::string top1_class_ref;
    std::string class_confidence_ref;
    double confidence = 0.0;
    std::string template_alignment_ref;
    std::string template_test_alignment_status;
    std::string roi_diff_candidate_ref;
    std::string roi_diff_candidate_count;
    std::string prior_roi_region_ref;
    std::string roi_crop_packet_ref;
    std::string roi_crop_count;
    std::string roi_crop_spatial_size;
    std::string roi_crop_policy_ref;
};

struct TorchOptimizationHandoffBridgeRecord
{
    TorchHandoffMetadata metadata;
    TorchGeometrySemanticRefRecord geometry;
    std::string objective_ref;
    std::string threshold_ref;
    std::string crop_policy_ref;
    std::string boundary_error_ref;
    std::string alignment_error_ref;
    std::string optimization_result_ref;
};

inline const char* TorchGeometryHandoffFeatureVectorName()
{
    return "torch_geometry_handoff_feature";
}

inline const char* TorchGeometryHandoffFeatureRole()
{
    return "torch_geometry_handoff";
}

inline const char* TorchGeometryHandoffFeatureSource()
{
    return "torch_test_host";
}

inline const char* TorchFeatureSemanticHandoffFeatureVectorName()
{
    return "torch_feature_semantic_handoff_feature";
}

inline const char* TorchFeatureSemanticHandoffFeatureRole()
{
    return "torch_feature_semantic_handoff";
}

inline const char* TorchFeatureSemanticHandoffFeatureSource()
{
    return "torch_test_host";
}

inline const char* TorchOptimizationHandoffFeatureVectorName()
{
    return "torch_optimization_handoff_feature";
}

inline const char* TorchOptimizationHandoffFeatureRole()
{
    return "torch_optimization_handoff";
}

inline const char* TorchOptimizationHandoffFeatureSource()
{
    return "torch_test_host";
}

inline const char* CxcoreInternalAcceptanceStageName(CxcoreInternalAcceptanceStage stage)
{
    switch (stage)
    {
    case CxcoreInternalAcceptanceStage::ModularManualUi:
        return "cxcore_modular_manual_ui_acceptance";
    case CxcoreInternalAcceptanceStage::RemoteAiSemantic:
        return "remote_ai_semantic_acceptance";
    case CxcoreInternalAcceptanceStage::AtomicSemantic:
        return "atomic_semantic_acceptance";
    case CxcoreInternalAcceptanceStage::Unknown:
    default:
        return "unknown";
    }
}

inline const char* CxcoreInternalPreparationStageName(CxcoreInternalPreparationStage stage)
{
    switch (stage)
    {
    case CxcoreInternalPreparationStage::CodeCleaning:
        return "cxcore_code_cleaning_stage";
    case CxcoreInternalPreparationStage::Unknown:
    default:
        return "unknown";
    }
}

inline std::vector<std::string> BuildCxcoreInternalAcceptanceStageSequence()
{
    return {
        CxcoreInternalAcceptanceStageName(CxcoreInternalAcceptanceStage::ModularManualUi),
        CxcoreInternalAcceptanceStageName(CxcoreInternalAcceptanceStage::RemoteAiSemantic),
        CxcoreInternalAcceptanceStageName(CxcoreInternalAcceptanceStage::AtomicSemantic)
    };
}

inline std::vector<std::string> BuildCxcoreInternalExecutionStageSequence()
{
    std::vector<std::string> sequence;
    sequence.push_back(CxcoreInternalPreparationStageName(CxcoreInternalPreparationStage::CodeCleaning));
    const std::vector<std::string> acceptance_sequence =
        BuildCxcoreInternalAcceptanceStageSequence();
    sequence.insert(sequence.end(), acceptance_sequence.begin(), acceptance_sequence.end());
    return sequence;
}

inline const char* CxcoreInternalPrimaryTestInterfaceName()
{
    return "cxcore.internal.manual_ui_local_analysis";
}

inline const char* CxcoreInternalPrimaryTestInterfacePurpose()
{
    return "modular_manual_ui_local_analysis";
}

inline std::vector<std::string> BuildCxcoreInternalPrimaryTestInterfaceCommands()
{
    return {
        "cxcore.internal.test.load",
        "cxcore.internal.test.publish",
        "cxcore.internal.test.summarize",
        "cxcore.internal.test.overlay",
        "cxcore.internal.test.accept"
    };
}

inline const char* TorchGeometryHandoffIngestCommandName()
{
    return "cxcore.ingest.torch_geometry_handoff";
}

inline const char* TorchFeatureSemanticHandoffIngestCommandName()
{
    return "cxcore.ingest.torch_feature_semantic_handoff";
}

inline const char* TorchOptimizationHandoffIngestCommandName()
{
    return "cxcore.ingest.torch_optimization_handoff";
}

inline const char* TorchGeometryHandoffSummarizeCommandName()
{
    return "cxcore.summarize.torch_geometry_handoff";
}

inline const char* TorchFeatureSemanticHandoffSummarizeCommandName()
{
    return "cxcore.summarize.torch_feature_semantic_handoff";
}

inline const char* TorchOptimizationHandoffSummarizeCommandName()
{
    return "cxcore.summarize.torch_optimization_handoff";
}

inline const char* TorchGeometryHandoffExportCommandName()
{
    return "cxcore.export_feature_vector.torch_geometry_handoff";
}

inline const char* TorchFeatureSemanticHandoffExportCommandName()
{
    return "cxcore.export_feature_vector.torch_feature_semantic_handoff";
}

inline const char* TorchOptimizationHandoffExportCommandName()
{
    return "cxcore.export_feature_vector.torch_optimization_handoff";
}

inline const char* TorchGeometrySemanticRolePublishCommandName(const std::string& role)
{
    if (role == TorchGeometrySemanticRole_GeometryHandoff())
    {
        return "cxcore.publish.geometry_detection_anchor";
    }
    if (role == TorchGeometrySemanticRole_FeatureSemanticHandoff())
    {
        return "cxcore.publish.feature_semantic_geometry_anchor";
    }
    if (role == TorchGeometrySemanticRole_OptimizationHandoff())
    {
        return "cxcore.publish.optimization_geometry_anchor";
    }
    return "cxcore.publish.geometry_anchor";
}

inline const char* TorchMeasurementKindPublishCommandName(TorchMeasurementKind kind)
{
    switch (kind)
    {
    case TorchMeasurementKind::Line:
        return "cxcore.publish.line_measurement_ref";
    case TorchMeasurementKind::Circle:
        return "cxcore.publish.circle_measurement_ref";
    case TorchMeasurementKind::Ellipse:
        return "cxcore.publish.ellipse_measurement_ref";
    case TorchMeasurementKind::Boundary:
        return "cxcore.publish.boundary_measurement_ref";
    case TorchMeasurementKind::Unknown:
    default:
        return "cxcore.publish.measurement_ref";
    }
}

inline const char* TorchMeasurementKindSummarizeCommandName(TorchMeasurementKind kind)
{
    switch (kind)
    {
    case TorchMeasurementKind::Line:
        return "cxcore.summarize.line_measurement_ref";
    case TorchMeasurementKind::Circle:
        return "cxcore.summarize.circle_measurement_ref";
    case TorchMeasurementKind::Ellipse:
        return "cxcore.summarize.ellipse_measurement_ref";
    case TorchMeasurementKind::Boundary:
        return "cxcore.summarize.boundary_measurement_ref";
    case TorchMeasurementKind::Unknown:
    default:
        return "cxcore.summarize.measurement_ref";
    }
}

inline const char* TorchMeasurementKindExportCommandName(TorchMeasurementKind kind)
{
    switch (kind)
    {
    case TorchMeasurementKind::Line:
        return "cxcore.export_feature_vector.line_measurement_ref";
    case TorchMeasurementKind::Circle:
        return "cxcore.export_feature_vector.circle_measurement_ref";
    case TorchMeasurementKind::Ellipse:
        return "cxcore.export_feature_vector.ellipse_measurement_ref";
    case TorchMeasurementKind::Boundary:
        return "cxcore.export_feature_vector.boundary_measurement_ref";
    case TorchMeasurementKind::Unknown:
    default:
        return "cxcore.export_feature_vector.measurement_ref";
    }
}

inline const char* TorchGeometrySemanticRoleSummarizeCommandName(const std::string& role)
{
    if (role == TorchGeometrySemanticRole_GeometryHandoff())
    {
        return "cxcore.summarize.geometry_detection_anchor";
    }
    if (role == TorchGeometrySemanticRole_FeatureSemanticHandoff())
    {
        return "cxcore.summarize.feature_semantic_geometry_anchor";
    }
    if (role == TorchGeometrySemanticRole_OptimizationHandoff())
    {
        return "cxcore.summarize.optimization_geometry_anchor";
    }
    return "cxcore.summarize.geometry_anchor";
}

inline const char* TorchGeometrySemanticRoleExportCommandName(const std::string& role)
{
    if (role == TorchGeometrySemanticRole_GeometryHandoff())
    {
        return "cxcore.export_feature_vector.geometry_detection_anchor";
    }
    if (role == TorchGeometrySemanticRole_FeatureSemanticHandoff())
    {
        return "cxcore.export_feature_vector.feature_semantic_geometry_anchor";
    }
    if (role == TorchGeometrySemanticRole_OptimizationHandoff())
    {
        return "cxcore.export_feature_vector.optimization_geometry_anchor";
    }
    return "cxcore.export_feature_vector.geometry_anchor";
}

inline const char* TorchGeometrySemanticRole_GeometryHandoff()
{
    return "geometry_detection_anchor";
}

inline const char* TorchGeometrySemanticRole_FeatureSemanticHandoff()
{
    return "feature_semantic_geometry_anchor";
}

inline const char* TorchGeometrySemanticRole_OptimizationHandoff()
{
    return "optimization_geometry_anchor";
}

inline const char* TorchMeasurementKindName(TorchMeasurementKind kind)
{
    switch (kind)
    {
    case TorchMeasurementKind::Line:
        return "line";
    case TorchMeasurementKind::Circle:
        return "circle";
    case TorchMeasurementKind::Ellipse:
        return "ellipse";
    case TorchMeasurementKind::Boundary:
        return "boundary";
    case TorchMeasurementKind::Unknown:
    default:
        return "unknown";
    }
}

inline const char* TorchHandoffRouteStateName(TorchHandoffRouteState state)
{
    switch (state)
    {
    case TorchHandoffRouteState::StayInCxcore:
        return "stay_in_cxcore";
    case TorchHandoffRouteState::RouteToMlpack:
        return "route_to_mlpack";
    case TorchHandoffRouteState::HandoffToEnsmallen:
        return "handoff_to_ensmallen";
    case TorchHandoffRouteState::UpgradeToTorchModule:
        return "upgrade_to_torch_module";
    case TorchHandoffRouteState::ManualReview:
        return "manual_review";
    case TorchHandoffRouteState::Unknown:
    default:
        return "unknown";
    }
}

inline TorchHandoffRouteState DetectTorchHandoffRouteState(const std::string& next_action)
{
    if (next_action.find("route_to_mlpack") != std::string::npos)
    {
        return TorchHandoffRouteState::RouteToMlpack;
    }
    if (next_action.find("ensmallen") != std::string::npos)
    {
        return TorchHandoffRouteState::HandoffToEnsmallen;
    }
    if (next_action.find("torch") != std::string::npos)
    {
        return TorchHandoffRouteState::UpgradeToTorchModule;
    }
    if (next_action.find("cxcore") != std::string::npos)
    {
        return TorchHandoffRouteState::StayInCxcore;
    }
    if (next_action.find("manual") != std::string::npos ||
        next_action.find("review") != std::string::npos)
    {
        return TorchHandoffRouteState::ManualReview;
    }
    return TorchHandoffRouteState::Unknown;
}

inline const char* TorchGeometryAnchorPropertyKeyRole()
{
    return "role";
}

inline const char* TorchGeometryAnchorPropertyKeySourceImageId()
{
    return "source_image_id";
}

inline const char* TorchGeometryAnchorPropertyKeyParentRoiId()
{
    return "parent_roi_id";
}

inline const char* TorchMeasurementPropertyKeyRef()
{
    return "measurement_ref";
}

inline const char* TorchMeasurementPropertyKeyKind()
{
    return "measurement_kind";
}

inline const char* TorchMeasurementPropertyKeyDirect()
{
    return "is_direct_measurement";
}

inline const char* TorchManifestPropertyKeyName()
{
    return "manifest_name";
}

inline const char* TorchManifestPropertyKeyRole()
{
    return "manifest_role";
}

inline const char* TorchManifestPropertyKeySource()
{
    return "manifest_source";
}

inline const char* TorchManifestPropertyKeyDim()
{
    return "manifest_dim";
}

inline const char* TorchPublishedFieldKeyHandoffType()
{
    return "published_handoff_type";
}

inline const char* TorchPublishedFieldKeyPrimaryRef()
{
    return "published_primary_ref";
}

inline const char* TorchPublishedFieldKeyRouteHint()
{
    return "published_route_hint";
}

inline const char* TorchPublishedFieldKeySourceHash()
{
    return "published_source_hash";
}

inline const char* TorchPublishedFieldKeyEvidenceRef()
{
    return "published_evidence_ref";
}

inline const char* TorchPublishedFieldKeyRouteState()
{
    return "published_route_state";
}

inline const char* TorchPublishedFieldKeyResultRef()
{
    return "published_result_ref";
}

inline const char* TorchPublishedFieldKeyBboxCandidateListRef()
{
    return "published_bbox_candidate_list_ref";
}

inline const char* TorchPublishedFieldKeyTemplateAlignmentRef()
{
    return "published_template_alignment_ref";
}

inline const char* TorchPublishedFieldKeyTemplateTestAlignmentStatus()
{
    return "published_template_test_alignment_status";
}

inline const char* TorchPublishedFieldKeyRoiDiffCandidateRef()
{
    return "published_roi_diff_candidate_ref";
}

inline const char* TorchPublishedFieldKeyRoiDiffCandidateCount()
{
    return "published_roi_diff_candidate_count";
}

inline const char* TorchPublishedFieldKeyPriorRoiRegionRef()
{
    return "published_prior_roi_region_ref";
}

inline const char* TorchPublishedFieldKeyRoiCropPacketRef()
{
    return "published_roi_crop_packet_ref";
}

inline const char* TorchPublishedFieldKeyRoiCropCount()
{
    return "published_roi_crop_count";
}

inline const char* TorchPublishedFieldKeyRoiCropSpatialSize()
{
    return "published_roi_crop_spatial_size";
}

inline const char* TorchPublishedFieldKeyRoiCropPolicyRef()
{
    return "published_roi_crop_policy_ref";
}

inline const char* TorchEvidenceFieldKeyBboxCandidateListExported()
{
    return "bbox_candidate_list_exported";
}

inline const char* TorchEvidenceFieldKeyTemplateAlignmentExported()
{
    return "template_alignment_exported";
}

inline const char* TorchEvidenceFieldKeyRoiDiffCandidateExported()
{
    return "roi_diff_candidate_exported";
}

inline const char* TorchEvidenceFieldKeyPriorRoiRegionExported()
{
    return "prior_roi_region_exported";
}

inline const char* TorchEvidenceFieldKeyRoiCropPacketExported()
{
    return "roi_crop_packet_exported";
}

inline const char* TorchEvidenceFieldKeyChainEstablished()
{
    return "chain_established";
}

inline const char* TorchEvidenceFieldKeyAlgorithmEffectEstablished()
{
    return "algorithm_effect_established";
}

inline const char* TorchEvidenceFieldKeyManualReviewRequired()
{
    return "manual_review_required";
}

inline TorchMeasurementKind DetectTorchMeasurementKindFromRef(const std::string& ref)
{
    if (ref.find("line") != std::string::npos)
    {
        return TorchMeasurementKind::Line;
    }
    if (ref.find("circle") != std::string::npos)
    {
        return TorchMeasurementKind::Circle;
    }
    if (ref.find("ellipse") != std::string::npos)
    {
        return TorchMeasurementKind::Ellipse;
    }
    if (ref.find("boundary") != std::string::npos)
    {
        return TorchMeasurementKind::Boundary;
    }
    return TorchMeasurementKind::Unknown;
}

inline StableGeometryRef MakeTorchGeometryRef(
    const std::string& object_id,
    GeometryObjectKind kind,
    const std::string& source_hash,
    const std::string& parent_roi_id = std::string())
{
    StableGeometryRef ref;
    ref.object_id = object_id;
    ref.source_image_id = source_hash;
    ref.parent_roi_id = parent_roi_id;
    ref.kind = kind;
    return ref;
}

inline TorchHandoffMetadata MakeTorchHandoffMetadata(
    const std::string& source_hash,
    const std::string& result_ref,
    const std::string& evidence_ref,
    const std::string& log_path,
    const std::string& model_version,
    const std::string& next_action)
{
    TorchHandoffMetadata metadata;
    metadata.source_hash = source_hash;
    metadata.result_ref = result_ref;
    metadata.evidence_ref = evidence_ref;
    metadata.log_path = log_path;
    metadata.model_version = model_version;
    metadata.next_action = next_action;
    return metadata;
}

inline TorchGeometrySemanticRefRecord MakeTorchGeometrySemanticRefRecord(
    const std::string& object_id,
    const std::string& source_hash,
    const std::string& parent_roi_id,
    const std::string& role)
{
    TorchGeometrySemanticRefRecord record;
    record.ref = MakeTorchGeometryRef(
        object_id,
        GeometryObjectKind::PointSet,
        source_hash,
        parent_roi_id);
    record.role = role;
    return record;
}

inline TorchMeasurementRefRecord MakeTorchMeasurementRefRecord(const std::string& measurement_ref)
{
    TorchMeasurementRefRecord record;
    record.ref = measurement_ref;
    record.kind = DetectTorchMeasurementKindFromRef(measurement_ref);
    record.is_direct_measurement = record.kind != TorchMeasurementKind::Unknown;
    return record;
}

inline TorchHandoffSummary SummarizeTorchGeometryHandoff(
    const TorchGeometryHandoffBridgeRecord& record)
{
    TorchHandoffSummary summary;
    summary.handoff_type = "TorchGeometryHandoff";
    summary.summary =
        std::string("roi=") + record.roi.object_id +
        " bbox_candidates=" + record.bbox_candidate_list_ref +
        " geometry_role=" + record.geometry.role +
        " measurement_kind=" + TorchMeasurementKindName(record.measurement.kind);
    summary.refs = {
        record.bbox.object_id,
        record.mask.object_id,
        record.roi.object_id,
        record.region.object_id,
        record.contour.object_id,
        record.geometry.ref.object_id,
        record.measurement.ref,
        record.bbox_candidate_list_ref
    };
    return summary;
}

inline TorchHandoffSummary SummarizeTorchFeatureSemanticHandoff(
    const TorchFeatureSemanticHandoffBridgeRecord& record)
{
    TorchHandoffSummary summary;
    summary.handoff_type = "TorchFeatureSemanticHandoff";
    summary.summary =
        std::string("roi=") + record.roi.object_id +
        " template_alignment=" + record.template_alignment_ref +
        " prior_roi_region=" + record.prior_roi_region_ref +
        " roi_diff=" + record.roi_diff_candidate_ref +
        " roi_crop_packet=" + record.roi_crop_packet_ref +
        " feature_dim=" + std::to_string(record.feature_dim) +
        " confidence=" + std::to_string(record.confidence);
    summary.refs = {
        record.roi.object_id,
        record.geometry.ref.object_id,
        record.roi_stats_ref,
        record.embedding_ref,
        record.feature_vector_ref,
        record.feature_set_ref,
        record.top1_class_ref,
        record.class_confidence_ref,
        record.template_alignment_ref,
        record.template_test_alignment_status,
        record.roi_diff_candidate_ref,
        record.roi_diff_candidate_count,
        record.prior_roi_region_ref,
        record.roi_crop_packet_ref,
        record.roi_crop_count,
        record.roi_crop_spatial_size,
        record.roi_crop_policy_ref
    };
    return summary;
}

inline TorchHandoffSummary SummarizeTorchOptimizationHandoff(
    const TorchOptimizationHandoffBridgeRecord& record)
{
    TorchHandoffSummary summary;
    summary.handoff_type = "TorchOptimizationHandoff";
    summary.summary =
        std::string("geometry_role=") + record.geometry.role +
        " objective_ref=" + record.objective_ref +
        " threshold_ref=" + record.threshold_ref;
    summary.refs = {
        record.geometry.ref.object_id,
        record.objective_ref,
        record.threshold_ref,
        record.crop_policy_ref,
        record.boundary_error_ref,
        record.alignment_error_ref,
        record.optimization_result_ref
    };
    return summary;
}

inline TorchHandoffManifestEntry MakeTorchHandoffManifestEntry(
    const FeatureVectorInput& feature)
{
    TorchHandoffManifestEntry entry;
    entry.name = feature.name;
    entry.role = feature.role;
    entry.source = feature.source;
    entry.dim = static_cast<int>(feature.values.size());
    return entry;
}

inline EvidenceDescriptorSummary MakeEvidenceDescriptorSummary(
    const TorchHandoffManifestEntry& entry)
{
    EvidenceDescriptorSummary summary;
    summary.name = entry.name;
    summary.role = entry.role;
    summary.source = entry.source;
    summary.dim = entry.dim;
    return summary;
}

inline std::vector<GeometryPropertyItem> BuildManifestProperties(
    const TorchHandoffManifestEntry& entry)
{
    return {
        { TorchManifestPropertyKeyName(), entry.name },
        { TorchManifestPropertyKeyRole(), entry.role },
        { TorchManifestPropertyKeySource(), entry.source },
        { TorchManifestPropertyKeyDim(), std::to_string(entry.dim) }
    };
}

inline std::vector<GeometryPropertyItem> BuildPublishedFieldProperties(
    const TorchGeometryHandoffBridgeRecord& record)
{
    return {
        { TorchPublishedFieldKeyHandoffType(), "TorchGeometryHandoff" },
        { TorchPublishedFieldKeyPrimaryRef(), record.roi.object_id },
        { TorchPublishedFieldKeyRouteHint(), record.metadata.next_action },
        { TorchPublishedFieldKeyRouteState(), TorchHandoffRouteStateName(DetectTorchHandoffRouteState(record.metadata.next_action)) },
        { TorchPublishedFieldKeySourceHash(), record.metadata.source_hash },
        { TorchPublishedFieldKeyEvidenceRef(), record.metadata.evidence_ref },
        { TorchPublishedFieldKeyBboxCandidateListRef(), record.bbox_candidate_list_ref }
    };
}

inline std::vector<GeometryPropertyItem> BuildPublishedFieldProperties(
    const TorchFeatureSemanticHandoffBridgeRecord& record)
{
    return {
        { TorchPublishedFieldKeyHandoffType(), "TorchFeatureSemanticHandoff" },
        { TorchPublishedFieldKeyPrimaryRef(), record.roi.object_id },
        { TorchPublishedFieldKeyRouteHint(), record.metadata.next_action },
        { TorchPublishedFieldKeyRouteState(), TorchHandoffRouteStateName(DetectTorchHandoffRouteState(record.metadata.next_action)) },
        { TorchPublishedFieldKeySourceHash(), record.metadata.source_hash },
        { TorchPublishedFieldKeyEvidenceRef(), record.metadata.evidence_ref },
        { TorchPublishedFieldKeyTemplateAlignmentRef(), record.template_alignment_ref },
        { TorchPublishedFieldKeyTemplateTestAlignmentStatus(), record.template_test_alignment_status },
        { TorchPublishedFieldKeyRoiDiffCandidateRef(), record.roi_diff_candidate_ref },
        { TorchPublishedFieldKeyRoiDiffCandidateCount(), record.roi_diff_candidate_count },
        { TorchPublishedFieldKeyPriorRoiRegionRef(), record.prior_roi_region_ref },
        { TorchPublishedFieldKeyRoiCropPacketRef(), record.roi_crop_packet_ref },
        { TorchPublishedFieldKeyRoiCropCount(), record.roi_crop_count },
        { TorchPublishedFieldKeyRoiCropSpatialSize(), record.roi_crop_spatial_size },
        { TorchPublishedFieldKeyRoiCropPolicyRef(), record.roi_crop_policy_ref }
    };
}

inline std::vector<GeometryPropertyItem> BuildPublishedFieldProperties(
    const TorchOptimizationHandoffBridgeRecord& record)
{
    return {
        { TorchPublishedFieldKeyHandoffType(), "TorchOptimizationHandoff" },
        { TorchPublishedFieldKeyPrimaryRef(), record.geometry.ref.object_id },
        { TorchPublishedFieldKeyRouteHint(), record.metadata.next_action },
        { TorchPublishedFieldKeyRouteState(), TorchHandoffRouteStateName(DetectTorchHandoffRouteState(record.metadata.next_action)) },
        { TorchPublishedFieldKeySourceHash(), record.metadata.source_hash },
        { TorchPublishedFieldKeyEvidenceRef(), record.metadata.evidence_ref }
    };
}

inline TorchHandoffTaskSnapshot MakeTorchHandoffTaskSnapshot(
    const TorchGeometryHandoffBridgeRecord& record)
{
    TorchHandoffTaskSnapshot snapshot;
    snapshot.handoff_type = "TorchGeometryHandoff";
    snapshot.primary_ref = record.roi.object_id;
    snapshot.route_hint = record.metadata.next_action;
    snapshot.route_state = DetectTorchHandoffRouteState(record.metadata.next_action);
    snapshot.source_hash = record.metadata.source_hash;
    snapshot.result_ref = record.metadata.result_ref;
    snapshot.evidence_ref = record.metadata.evidence_ref;
    return snapshot;
}

inline TorchHandoffTaskSnapshot MakeTorchHandoffTaskSnapshot(
    const TorchFeatureSemanticHandoffBridgeRecord& record)
{
    TorchHandoffTaskSnapshot snapshot;
    snapshot.handoff_type = "TorchFeatureSemanticHandoff";
    snapshot.primary_ref = record.roi.object_id;
    snapshot.route_hint = record.metadata.next_action;
    snapshot.route_state = DetectTorchHandoffRouteState(record.metadata.next_action);
    snapshot.source_hash = record.metadata.source_hash;
    snapshot.result_ref = record.metadata.result_ref;
    snapshot.evidence_ref = record.metadata.evidence_ref;
    return snapshot;
}

inline TorchHandoffTaskSnapshot MakeTorchHandoffTaskSnapshot(
    const TorchOptimizationHandoffBridgeRecord& record)
{
    TorchHandoffTaskSnapshot snapshot;
    snapshot.handoff_type = "TorchOptimizationHandoff";
    snapshot.primary_ref = record.geometry.ref.object_id;
    snapshot.route_hint = record.metadata.next_action;
    snapshot.route_state = DetectTorchHandoffRouteState(record.metadata.next_action);
    snapshot.source_hash = record.metadata.source_hash;
    snapshot.result_ref = record.metadata.result_ref;
    snapshot.evidence_ref = record.metadata.evidence_ref;
    return snapshot;
}

inline std::vector<GeometryPropertyItem> BuildTaskSnapshotProperties(
    const TorchHandoffTaskSnapshot& snapshot)
{
    return {
        { TorchPublishedFieldKeyHandoffType(), snapshot.handoff_type },
        { TorchPublishedFieldKeyPrimaryRef(), snapshot.primary_ref },
        { TorchPublishedFieldKeyRouteHint(), snapshot.route_hint },
        { TorchPublishedFieldKeyRouteState(), TorchHandoffRouteStateName(snapshot.route_state) },
        { TorchPublishedFieldKeySourceHash(), snapshot.source_hash },
        { TorchPublishedFieldKeyResultRef(), snapshot.result_ref },
        { TorchPublishedFieldKeyEvidenceRef(), snapshot.evidence_ref }
    };
}

inline std::string BuildTorchHandoffTaskSnapshotSummary(
    const TorchHandoffTaskSnapshot& snapshot)
{
    return
        std::string("handoff_type=") + snapshot.handoff_type +
        " primary_ref=" + snapshot.primary_ref +
        " route_state=" + TorchHandoffRouteStateName(snapshot.route_state) +
        " result_ref=" + snapshot.result_ref +
        " evidence_ref=" + snapshot.evidence_ref;
}

inline std::vector<std::string> BuildTorchHandoffPublishedReadbackKeys()
{
    return {
        TorchPublishedFieldKeyHandoffType(),
        TorchPublishedFieldKeyPrimaryRef(),
        TorchPublishedFieldKeyRouteHint(),
        TorchPublishedFieldKeyRouteState(),
        TorchPublishedFieldKeySourceHash(),
        TorchPublishedFieldKeyEvidenceRef()
    };
}

inline std::vector<std::string> BuildTorchGeometryPublishedReadbackKeys()
{
    return {
        TorchPublishedFieldKeyBboxCandidateListRef()
    };
}

inline TorchGeometryEvidenceStatus BuildTorchGeometryEvidenceStatus(
    const TorchGeometryHandoffBridgeRecord& record)
{
    TorchGeometryEvidenceStatus status;
    status.bbox_candidate_list_exported = !record.bbox_candidate_list_ref.empty();
    status.chain_established = status.bbox_candidate_list_exported;
    status.manual_review_required = true;
    return status;
}

inline std::vector<GeometryPropertyItem> BuildTorchGeometryEvidenceProperties(
    const TorchGeometryEvidenceStatus& status)
{
    return {
        { TorchEvidenceFieldKeyBboxCandidateListExported(), status.bbox_candidate_list_exported ? "true" : "false" },
        { TorchEvidenceFieldKeyChainEstablished(), status.chain_established ? "true" : "false" },
        { TorchEvidenceFieldKeyManualReviewRequired(), status.manual_review_required ? "true" : "false" }
    };
}

inline std::string BuildTorchGeometryEvidenceSummary(
    const TorchGeometryEvidenceStatus& status)
{
    return
        std::string("bbox_candidate_list_exported=") + (status.bbox_candidate_list_exported ? "true" : "false") +
        " chain_established=" + (status.chain_established ? "true" : "false") +
        " manual_review_required=" + (status.manual_review_required ? "true" : "false");
}

inline std::vector<std::string> BuildTorchHandoffTaskSummaryReadbackKeys()
{
    return {
        TorchPublishedFieldKeyHandoffType(),
        TorchPublishedFieldKeyPrimaryRef(),
        TorchPublishedFieldKeyRouteState(),
        TorchPublishedFieldKeyResultRef(),
        TorchPublishedFieldKeyEvidenceRef()
    };
}

inline std::vector<std::string> BuildTorchFeatureSemanticPublishedReadbackKeys()
{
    return {
        TorchPublishedFieldKeyTemplateAlignmentRef(),
        TorchPublishedFieldKeyTemplateTestAlignmentStatus(),
        TorchPublishedFieldKeyRoiDiffCandidateRef(),
        TorchPublishedFieldKeyRoiDiffCandidateCount(),
        TorchPublishedFieldKeyPriorRoiRegionRef(),
        TorchPublishedFieldKeyRoiCropPacketRef(),
        TorchPublishedFieldKeyRoiCropCount(),
        TorchPublishedFieldKeyRoiCropSpatialSize(),
        TorchPublishedFieldKeyRoiCropPolicyRef()
    };
}

inline bool IsTorchFeatureSemanticEffectStatusPositive(const std::string& value)
{
    return value == "ok" ||
           value == "pass" ||
           value == "passed" ||
           value == "aligned" ||
           value == "ready" ||
           value == "stable" ||
           value == "exported";
}

inline TorchFeatureSemanticEvidenceStatus BuildTorchFeatureSemanticEvidenceStatus(
    const TorchFeatureSemanticHandoffBridgeRecord& record)
{
    TorchFeatureSemanticEvidenceStatus status;
    status.template_alignment_exported = !record.template_alignment_ref.empty();
    status.roi_diff_candidate_exported = !record.roi_diff_candidate_ref.empty();
    status.prior_roi_region_exported = !record.prior_roi_region_ref.empty();
    status.roi_crop_packet_exported = !record.roi_crop_packet_ref.empty();
    status.chain_established =
        status.template_alignment_exported ||
        status.roi_diff_candidate_exported ||
        status.roi_crop_packet_exported ||
        status.prior_roi_region_exported;

    const bool template_effect =
        status.template_alignment_exported &&
        IsTorchFeatureSemanticEffectStatusPositive(record.template_test_alignment_status);
    const bool roi_diff_effect =
        status.roi_diff_candidate_exported &&
        !record.roi_diff_candidate_count.empty() &&
        record.roi_diff_candidate_count != "0";
    const bool roi_crop_effect =
        status.roi_crop_packet_exported &&
        !record.roi_crop_count.empty() &&
        record.roi_crop_count != "0";

    status.algorithm_effect_established =
        template_effect ||
        roi_diff_effect ||
        roi_crop_effect;
    status.manual_review_required = !status.algorithm_effect_established;
    return status;
}

inline std::vector<GeometryPropertyItem> BuildTorchFeatureSemanticEvidenceProperties(
    const TorchFeatureSemanticEvidenceStatus& status)
{
    return {
        { TorchEvidenceFieldKeyTemplateAlignmentExported(), status.template_alignment_exported ? "true" : "false" },
        { TorchEvidenceFieldKeyRoiDiffCandidateExported(), status.roi_diff_candidate_exported ? "true" : "false" },
        { TorchEvidenceFieldKeyPriorRoiRegionExported(), status.prior_roi_region_exported ? "true" : "false" },
        { TorchEvidenceFieldKeyRoiCropPacketExported(), status.roi_crop_packet_exported ? "true" : "false" },
        { TorchEvidenceFieldKeyChainEstablished(), status.chain_established ? "true" : "false" },
        { TorchEvidenceFieldKeyAlgorithmEffectEstablished(), status.algorithm_effect_established ? "true" : "false" },
        { TorchEvidenceFieldKeyManualReviewRequired(), status.manual_review_required ? "true" : "false" }
    };
}

inline std::string BuildTorchFeatureSemanticEvidenceSummary(
    const TorchFeatureSemanticEvidenceStatus& status)
{
    return
        std::string("template_alignment_exported=") + (status.template_alignment_exported ? "true" : "false") +
        " roi_diff_candidate_exported=" + (status.roi_diff_candidate_exported ? "true" : "false") +
        " prior_roi_region_exported=" + (status.prior_roi_region_exported ? "true" : "false") +
        " roi_crop_packet_exported=" + (status.roi_crop_packet_exported ? "true" : "false") +
        " chain_established=" + (status.chain_established ? "true" : "false") +
        " algorithm_effect_established=" + (status.algorithm_effect_established ? "true" : "false") +
        " manual_review_required=" + (status.manual_review_required ? "true" : "false");
}

inline TorchPriorityBridgeMirrorStatus BuildTorchPriorityBridgeMirrorStatus(
    const TorchGeometryHandoffBridgeRecord& geometry_record,
    const TorchFeatureSemanticHandoffBridgeRecord& semantic_record)
{
    const TorchGeometryEvidenceStatus geometry_status =
        BuildTorchGeometryEvidenceStatus(geometry_record);
    const TorchFeatureSemanticEvidenceStatus semantic_status =
        BuildTorchFeatureSemanticEvidenceStatus(semantic_record);

    TorchPriorityBridgeMirrorStatus status;
    status.bbox_candidate_list_exported = geometry_status.bbox_candidate_list_exported;
    status.prior_roi_region_exported = semantic_status.prior_roi_region_exported;
    status.roi_crop_packet_exported = semantic_status.roi_crop_packet_exported;
    status.chain_established =
        status.bbox_candidate_list_exported ||
        status.prior_roi_region_exported ||
        status.roi_crop_packet_exported;
    status.algorithm_effect_established = semantic_status.algorithm_effect_established;
    status.manual_review_required =
        geometry_status.manual_review_required ||
        semantic_status.manual_review_required;
    return status;
}

inline std::vector<GeometryPropertyItem> BuildTorchPriorityBridgeMirrorProperties(
    const TorchPriorityBridgeMirrorStatus& status)
{
    return {
        { TorchEvidenceFieldKeyBboxCandidateListExported(), status.bbox_candidate_list_exported ? "true" : "false" },
        { TorchEvidenceFieldKeyPriorRoiRegionExported(), status.prior_roi_region_exported ? "true" : "false" },
        { TorchEvidenceFieldKeyRoiCropPacketExported(), status.roi_crop_packet_exported ? "true" : "false" },
        { TorchEvidenceFieldKeyChainEstablished(), status.chain_established ? "true" : "false" },
        { TorchEvidenceFieldKeyAlgorithmEffectEstablished(), status.algorithm_effect_established ? "true" : "false" },
        { TorchEvidenceFieldKeyManualReviewRequired(), status.manual_review_required ? "true" : "false" }
    };
}

inline std::vector<std::string> BuildTorchPriorityBridgeMirrorFieldKeys()
{
    return {
        TorchEvidenceFieldKeyBboxCandidateListExported(),
        TorchEvidenceFieldKeyPriorRoiRegionExported(),
        TorchEvidenceFieldKeyRoiCropPacketExported(),
        TorchEvidenceFieldKeyChainEstablished(),
        TorchEvidenceFieldKeyAlgorithmEffectEstablished(),
        TorchEvidenceFieldKeyManualReviewRequired()
    };
}

inline std::vector<std::string> BuildTorchPriorityBridgeMirrorFieldOrder()
{
    return BuildTorchPriorityBridgeMirrorFieldKeys();
}

inline std::vector<std::string> BuildTorchPriorityBridgePublishedReadbackKeys()
{
    return {
        TorchPublishedFieldKeyBboxCandidateListRef(),
        TorchPublishedFieldKeyPriorRoiRegionRef(),
        TorchPublishedFieldKeyRoiCropPacketRef()
    };
}

inline std::string BuildTorchPriorityBridgeMirrorDisplayOrderSummary()
{
    const std::vector<std::string> keys = BuildTorchPriorityBridgeMirrorFieldOrder();
    std::string summary;
    for (size_t index = 0; index < keys.size(); ++index)
    {
        if (!summary.empty())
        {
            summary += " ";
        }
        summary += std::to_string(index);
        summary += ":";
        summary += keys[index];
    }
    return summary;
}

inline std::string BuildTorchPriorityBridgePublishedReadbackOrderSummary()
{
    const std::vector<std::string> keys = BuildTorchPriorityBridgePublishedReadbackKeys();
    std::string summary;
    for (size_t index = 0; index < keys.size(); ++index)
    {
        if (!summary.empty())
        {
            summary += " ";
        }
        summary += std::to_string(index);
        summary += ":";
        summary += keys[index];
    }
    return summary;
}

inline std::string BuildTorchPriorityBridgeMirrorSummary(
    const TorchPriorityBridgeMirrorStatus& status)
{
    return
        std::string("bbox_candidate_list_exported=") + (status.bbox_candidate_list_exported ? "true" : "false") +
        " prior_roi_region_exported=" + (status.prior_roi_region_exported ? "true" : "false") +
        " roi_crop_packet_exported=" + (status.roi_crop_packet_exported ? "true" : "false") +
        " chain_established=" + (status.chain_established ? "true" : "false") +
        " algorithm_effect_established=" + (status.algorithm_effect_established ? "true" : "false") +
        " manual_review_required=" + (status.manual_review_required ? "true" : "false");
}

inline std::string BuildTorchPriorityBridgePublishedReviewSummary(
    const TorchPriorityBridgeMirrorStatus& status)
{
    return
        std::string("[PUBLISHED_REVIEW] order={") +
        BuildTorchPriorityBridgeMirrorDisplayOrderSummary() +
        "} readback_order={" +
        BuildTorchPriorityBridgePublishedReadbackOrderSummary() +
        "} fields={" +
        BuildTorchPriorityBridgeMirrorSummary(status) +
        "}";
}

inline std::vector<TorchHandoffManifestEntry> BuildTorchHandoffManifest(
    const TorchGeometryHandoffBridgeRecord& geometry,
    const TorchFeatureSemanticHandoffBridgeRecord& semantic,
    const TorchOptimizationHandoffBridgeRecord& optimization)
{
    return {
        MakeTorchHandoffManifestEntry(MakeTorchGeometryHandoffFeatureVector(geometry)),
        MakeTorchHandoffManifestEntry(MakeTorchFeatureSemanticHandoffFeatureVector(semantic)),
        MakeTorchHandoffManifestEntry(MakeTorchOptimizationHandoffFeatureVector(optimization))
    };
}

inline std::vector<EvidenceDescriptorSummary> BuildTorchHandoffDescriptorSummary(
    const TorchGeometryHandoffBridgeRecord& geometry,
    const TorchFeatureSemanticHandoffBridgeRecord& semantic,
    const TorchOptimizationHandoffBridgeRecord& optimization)
{
    const std::vector<TorchHandoffManifestEntry> manifest =
        BuildTorchHandoffManifest(geometry, semantic, optimization);
    std::vector<EvidenceDescriptorSummary> summary;
    summary.reserve(manifest.size());
    for (const auto& entry : manifest)
    {
        summary.push_back(MakeEvidenceDescriptorSummary(entry));
    }
    return summary;
}

inline GeometryObjectSummary SummarizeTorchGeometrySemanticRefRecord(
    const TorchGeometrySemanticRefRecord& record,
    const std::string& preview_ref = std::string())
{
    GeometryObjectSummary summary;
    summary.ref = record.ref;
    summary.summary = std::string("torch geometry semantic anchor: ") + record.role;
    summary.display = MakeDisplayHint(
        true,
        false,
        std::string("overlay://") + record.ref.object_id,
        preview_ref);
    summary.properties.push_back({ TorchGeometryAnchorPropertyKeyRole(), record.role });
    summary.properties.push_back({ TorchGeometryAnchorPropertyKeySourceImageId(), record.ref.source_image_id });
    summary.properties.push_back({ TorchGeometryAnchorPropertyKeyParentRoiId(), record.ref.parent_roi_id });
    return summary;
}

inline GeometryObjectSummary SummarizeTorchMeasurementRefRecord(
    const TorchMeasurementRefRecord& record)
{
    GeometryObjectSummary summary;
    summary.summary = std::string("torch measurement ref: ") + record.ref;
    summary.display = MakeDisplayHint(
        true,
        false,
        std::string("overlay://") + record.ref,
        std::string("preview://") + record.ref);
    summary.properties.push_back({ TorchMeasurementPropertyKeyRef(), record.ref });
    summary.properties.push_back({ TorchMeasurementPropertyKeyKind(), TorchMeasurementKindName(record.kind) });
    summary.properties.push_back({ TorchMeasurementPropertyKeyDirect(), record.is_direct_measurement ? "true" : "false" });
    return summary;
}

inline std::vector<std::string> BuildTorchGeometryAnchorCommandNames(const std::string& role)
{
    return {
        TorchGeometrySemanticRolePublishCommandName(role),
        TorchGeometrySemanticRoleSummarizeCommandName(role),
        TorchGeometrySemanticRoleExportCommandName(role)
    };
}

inline std::vector<std::string> BuildTorchMeasurementCommandNames(TorchMeasurementKind kind)
{
    return {
        TorchMeasurementKindPublishCommandName(kind),
        TorchMeasurementKindSummarizeCommandName(kind),
        TorchMeasurementKindExportCommandName(kind)
    };
}

inline TorchGeometryHandoffBridgeRecord MakeTorchGeometryHandoffBridgeRecord(
    const TorchGeometryHandoff& handoff)
{
    TorchGeometryHandoffBridgeRecord record;
    record.metadata = MakeTorchHandoffMetadata(
        handoff.source_hash,
        handoff.result_ref,
        handoff.evidence_ref,
        handoff.log_path,
        handoff.model_version,
        handoff.next_action);
    record.bbox = MakeTorchGeometryRef(
        handoff.bbox_ref,
        GeometryObjectKind::Roi,
        handoff.source_hash,
        handoff.roi_ref);
    record.mask = MakeTorchGeometryRef(
        handoff.mask_ref,
        GeometryObjectKind::Mask,
        handoff.source_hash,
        handoff.roi_ref);
    record.roi = MakeTorchGeometryRef(
        handoff.roi_ref,
        GeometryObjectKind::Roi,
        handoff.source_hash);
    record.region = MakeTorchGeometryRef(
        handoff.region_ref,
        GeometryObjectKind::Mask,
        handoff.source_hash,
        handoff.roi_ref);
    record.contour = MakeTorchGeometryRef(
        handoff.contour_ref,
        GeometryObjectKind::Boundary,
        handoff.source_hash,
        handoff.roi_ref);
    record.geometry = MakeTorchGeometrySemanticRefRecord(
        handoff.geometry_ref,
        handoff.source_hash,
        handoff.roi_ref,
        TorchGeometrySemanticRole_GeometryHandoff());
    record.measurement = MakeTorchMeasurementRefRecord(handoff.measurement_ref);
    record.bbox_candidate_list_ref = handoff.bbox_candidate_list_ref;
    return record;
}

inline TorchFeatureSemanticHandoffBridgeRecord MakeTorchFeatureSemanticHandoffBridgeRecord(
    const TorchFeatureSemanticHandoff& handoff)
{
    TorchFeatureSemanticHandoffBridgeRecord record;
    record.metadata = MakeTorchHandoffMetadata(
        handoff.source_hash,
        handoff.result_ref,
        handoff.evidence_ref,
        handoff.log_path,
        handoff.model_version,
        handoff.next_action);
    record.roi = MakeTorchGeometryRef(
        handoff.roi_ref,
        GeometryObjectKind::Roi,
        handoff.source_hash);
    record.geometry = MakeTorchGeometrySemanticRefRecord(
        handoff.geometry_ref,
        handoff.source_hash,
        handoff.roi_ref,
        TorchGeometrySemanticRole_FeatureSemanticHandoff());
    record.roi_stats_ref = handoff.roi_stats_ref;
    record.embedding_ref = handoff.embedding_ref;
    record.feature_vector_ref = handoff.feature_vector_ref;
    record.feature_set_ref = handoff.feature_set_ref;
    record.feature_dim = handoff.feature_dim;
    record.top1_class_ref = handoff.top1_class_ref;
    record.class_confidence_ref = handoff.class_confidence_ref;
    record.confidence = handoff.confidence;
    record.template_alignment_ref = handoff.template_alignment_ref;
    record.template_test_alignment_status = handoff.template_test_alignment_status;
    record.roi_diff_candidate_ref = handoff.roi_diff_candidate_ref;
    record.roi_diff_candidate_count = handoff.roi_diff_candidate_count;
    record.prior_roi_region_ref = handoff.prior_roi_region_ref;
    record.roi_crop_packet_ref = handoff.roi_crop_packet_ref;
    record.roi_crop_count = handoff.roi_crop_count;
    record.roi_crop_spatial_size = handoff.roi_crop_spatial_size;
    record.roi_crop_policy_ref = handoff.roi_crop_policy_ref;
    return record;
}

inline TorchOptimizationHandoffBridgeRecord MakeTorchOptimizationHandoffBridgeRecord(
    const TorchOptimizationHandoff& handoff)
{
    TorchOptimizationHandoffBridgeRecord record;
    record.metadata = MakeTorchHandoffMetadata(
        handoff.source_hash,
        handoff.result_ref,
        handoff.evidence_ref,
        handoff.log_path,
        handoff.model_version,
        handoff.next_action);
    record.geometry = MakeTorchGeometrySemanticRefRecord(
        handoff.geometry_ref,
        handoff.source_hash,
        std::string(),
        TorchGeometrySemanticRole_OptimizationHandoff());
    record.objective_ref = handoff.objective_ref;
    record.threshold_ref = handoff.threshold_ref;
    record.crop_policy_ref = handoff.crop_policy_ref;
    record.boundary_error_ref = handoff.boundary_error_ref;
    record.alignment_error_ref = handoff.alignment_error_ref;
    record.optimization_result_ref = handoff.optimization_result_ref;
    return record;
}

inline FeatureVectorInput MakeTorchGeometryHandoffFeatureVector(
    const TorchGeometryHandoffBridgeRecord& record,
    const std::string& name = TorchGeometryHandoffFeatureVectorName())
{
    FeatureVectorInput feature;
    feature.name = name;
    feature.role = TorchGeometryHandoffFeatureRole();
    feature.source = TorchGeometryHandoffFeatureSource();
    feature.values = {
        record.bbox.object_id.empty() ? 0.0f : 1.0f,
        record.mask.object_id.empty() ? 0.0f : 1.0f,
        record.roi.object_id.empty() ? 0.0f : 1.0f,
        record.region.object_id.empty() ? 0.0f : 1.0f,
        record.contour.object_id.empty() ? 0.0f : 1.0f,
        record.geometry.ref.object_id.empty() ? 0.0f : 1.0f,
        record.measurement.ref.empty() ? 0.0f : 1.0f,
        static_cast<float>(record.measurement.kind),
        record.bbox_candidate_list_ref.empty() ? 0.0f : 1.0f
    };
    return feature;
}

inline FeatureVectorInput MakeTorchFeatureSemanticHandoffFeatureVector(
    const TorchFeatureSemanticHandoffBridgeRecord& record,
    const std::string& name = TorchFeatureSemanticHandoffFeatureVectorName())
{
    FeatureVectorInput feature;
    feature.name = name;
    feature.role = TorchFeatureSemanticHandoffFeatureRole();
    feature.source = TorchFeatureSemanticHandoffFeatureSource();
    feature.values = {
        record.roi.object_id.empty() ? 0.0f : 1.0f,
        record.geometry.ref.object_id.empty() ? 0.0f : 1.0f,
        static_cast<float>(record.feature_dim),
        static_cast<float>(record.confidence),
        record.embedding_ref.empty() ? 0.0f : 1.0f,
        record.feature_vector_ref.empty() ? 0.0f : 1.0f,
        record.feature_set_ref.empty() ? 0.0f : 1.0f,
        record.top1_class_ref.empty() ? 0.0f : 1.0f,
        record.class_confidence_ref.empty() ? 0.0f : 1.0f,
        record.template_alignment_ref.empty() ? 0.0f : 1.0f,
        record.template_test_alignment_status.empty() ? 0.0f : 1.0f,
        record.roi_diff_candidate_ref.empty() ? 0.0f : 1.0f,
        record.roi_diff_candidate_count.empty() ? 0.0f : 1.0f,
        record.prior_roi_region_ref.empty() ? 0.0f : 1.0f,
        record.roi_crop_packet_ref.empty() ? 0.0f : 1.0f,
        record.roi_crop_count.empty() ? 0.0f : 1.0f,
        record.roi_crop_spatial_size.empty() ? 0.0f : 1.0f,
        record.roi_crop_policy_ref.empty() ? 0.0f : 1.0f
    };
    return feature;
}

inline FeatureVectorInput MakeTorchOptimizationHandoffFeatureVector(
    const TorchOptimizationHandoffBridgeRecord& record,
    const std::string& name = TorchOptimizationHandoffFeatureVectorName())
{
    FeatureVectorInput feature;
    feature.name = name;
    feature.role = TorchOptimizationHandoffFeatureRole();
    feature.source = TorchOptimizationHandoffFeatureSource();
    feature.values = {
        record.geometry.ref.object_id.empty() ? 0.0f : 1.0f,
        record.objective_ref.empty() ? 0.0f : 1.0f,
        record.threshold_ref.empty() ? 0.0f : 1.0f,
        record.crop_policy_ref.empty() ? 0.0f : 1.0f,
        record.boundary_error_ref.empty() ? 0.0f : 1.0f,
        record.alignment_error_ref.empty() ? 0.0f : 1.0f,
        record.optimization_result_ref.empty() ? 0.0f : 1.0f
    };
    return feature;
}

inline AiTaskEnvelope MakeTorchGeometryHandoffEnvelope(
    const TorchGeometryHandoffBridgeRecord& record,
    AiTaskKind task,
    bool requires_classical_explainability = true)
{
    AiTaskEnvelope envelope;
    envelope.task = task;
    envelope.requires_classical_explainability = requires_classical_explainability;
    envelope.descriptors.push_back(MakeTorchGeometryHandoffFeatureVector(record));
    return envelope;
}

inline AiTaskEnvelope MakeTorchFeatureSemanticHandoffEnvelope(
    const TorchFeatureSemanticHandoffBridgeRecord& record,
    AiTaskKind task)
{
    AiTaskEnvelope envelope;
    envelope.task = task;
    envelope.descriptors.push_back(MakeTorchFeatureSemanticHandoffFeatureVector(record));
    return envelope;
}

inline AiTaskEnvelope MakeTorchOptimizationHandoffEnvelope(
    const TorchOptimizationHandoffBridgeRecord& record,
    AiTaskKind task,
    bool requires_classical_explainability = true)
{
    AiTaskEnvelope envelope;
    envelope.task = task;
    envelope.requires_classical_explainability = requires_classical_explainability;
    envelope.descriptors.push_back(MakeTorchOptimizationHandoffFeatureVector(record));
    return envelope;
}

inline GeometryAttachRecord MakeTorchGeometryAttachRecord(
    const TorchGeometryHandoffBridgeRecord& record,
    const std::string& attach_id,
    float score = 1.0f)
{
    GeometryAttachRecord attach;
    attach.target = record.roi;
    attach.attach_id = attach_id;
    attach.label = "torch_geometry_handoff";
    attach.score = score;
    attach.mask_object_id = record.mask.object_id;
    attach.boundary_object_id = record.contour.object_id;
    return attach;
}

} // namespace cxcore

#endif
