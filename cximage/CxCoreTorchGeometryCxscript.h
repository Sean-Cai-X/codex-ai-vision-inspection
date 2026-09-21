#ifndef CXCORE_CORE_CXCORETORCHGEOMETRYCXSCRIPT_H
#define CXCORE_CORE_CXCORETORCHGEOMETRYCXSCRIPT_H

#include <string>

#include "CxCoreTorchHandoffBridge.h"

namespace cxcore {

inline const char* TorchGeometryCxscriptFragmentName_InputPriorRoiLinePointSet()
{
    return "feature.integration.torch_geometry_input_prior_roi_line_pointset";
}

inline const char* TorchGeometryCxscriptFragmentName_LabelMaskBoundaryKeypoints()
{
    return "feature.integration.torch_geometry_label_mask_boundary_keypoints";
}

inline const char* TorchGeometryCxscriptFragmentName_AttachBackToGeometry()
{
    return "feature.integration.torch_geometry_attach_back";
}

inline const char* TorchGeometryCxscriptFragmentName_StructuralEvidenceBundle()
{
    return "feature.integration.structural_evidence_bundle";
}

inline const char* TorchGeometryCxscriptFragmentName_TorchHandoffIngress()
{
    return "feature.integration.torch_handoff_ingress";
}

inline const char* TorchGeometryCxscriptFragmentName_TorchHandoffSummary()
{
    return "feature.integration.torch_handoff_summary";
}

inline const char* TorchGeometryCxscriptFragmentName_TorchHandoffObjectSummary()
{
    return "feature.integration.torch_handoff_object_summary";
}

inline const char* TorchGeometryCxscriptFragmentName_TorchHandoffPublishedReadback()
{
    return "feature.integration.torch_handoff_published_readback";
}

inline const char* TorchGeometryCxscriptFragmentName_TorchHandoffTaskSnapshot()
{
    return "feature.integration.torch_handoff_task_snapshot";
}

inline const char* TorchGeometryCxscriptFragmentName_TorchHandoffTaskSummary()
{
    return "feature.integration.torch_handoff_task_summary";
}

inline const char* TorchGeometryCxscriptFragmentName_TorchHandoffPublishedReview()
{
    return "feature.integration.torch_handoff_published_review";
}

inline std::string TorchGeometryCxscript_InputPriorRoiLinePointSet()
{
    return
        "Image source;\n"
        "Shape roi;\n"
        "FindLine line;\n"
        "FastMatch matcher;\n"
        "\n"
        "roi.setname(\"roi_input_prior\");\n"
        "source.getshape(roi);\n"
        "line.shapesetroi(roi);\n"
        "matcher.shapesetroi(roi);\n"
        "\n"
        "cxcore.export.roi_object(source, roi, \"roi_main\");\n"
        "cxcore.export.line_object(line, \"line_main\");\n"
        "cxcore.export.pointset_object(line, \"pointset_line_samples\");\n"
        "\n"
        "cxcore.align.input_prior(\"roi_main\", \"line_main\");\n"
        "cxcore.align.input_prior(\"roi_main\", \"pointset_line_samples\");\n"
        "cxcore.build.torch_request(\"roi_main\", \"input_prior_bundle\");\n"
        "cxcore.attach.descriptor(\"input_prior_bundle\", \"line_main\", \"external_geometry_descriptor\");\n"
        "cxcore.attach.descriptor(\"input_prior_bundle\", \"pointset_line_samples\", \"external_shape_descriptor\");\n"
        "cxcore.route.to_torch(\"input_prior_bundle\");\n";
}

inline std::string TorchGeometryCxscript_LabelMaskBoundaryKeypoints()
{
    return
        "Image source;\n"
        "Image labelMask;\n"
        "Shape roi;\n"
        "FindLine line;\n"
        "\n"
        "source.getshape(roi);\n"
        "labelMask.getshape(roi);\n"
        "line.shapesetroi(roi);\n"
        "\n"
        "cxcore.export.roi_object(source, roi, \"roi_train\");\n"
        "cxcore.export.mask_object(labelMask, roi, \"mask_gt\");\n"
        "cxcore.export.boundary_object(labelMask, roi, \"boundary_gt\");\n"
        "cxcore.export.keypoints_object(line, roi, \"keypoints_gt\");\n"
        "\n"
        "cxcore.align.training_label(\"roi_train\", \"mask_gt\");\n"
        "cxcore.align.training_label(\"roi_train\", \"boundary_gt\");\n"
        "cxcore.align.training_label(\"roi_train\", \"keypoints_gt\");\n"
        "cxcore.build.torch_label_packet(\"roi_train\", \"torch_label_packet\");\n"
        "cxcore.attach.label(\"torch_label_packet\", \"mask_gt\");\n"
        "cxcore.attach.label(\"torch_label_packet\", \"boundary_gt\");\n"
        "cxcore.attach.label(\"torch_label_packet\", \"keypoints_gt\");\n";
}

inline std::string TorchGeometryCxscript_AttachBackToGeometry()
{
    return
        "cxcore.load.roi_object(\"roi_main\");\n"
        "cxcore.load.mask_object(\"mask_pred\");\n"
        "cxcore.load.boundary_object(\"boundary_pred\");\n"
        "cxcore.load.keypoints_object(\"keypoints_pred\");\n"
        "\n"
        "cxcore.attach.result_to_roi(\"roi_main\", \"attach_cls_001\");\n"
        "cxcore.attach.result_label(\"attach_cls_001\", \"predicted_class\");\n"
        "cxcore.attach.result_score(\"attach_cls_001\", 0.95);\n"
        "cxcore.attach.result_embedding(\"attach_cls_001\", \"embedding_main\");\n"
        "\n"
        "cxcore.attach.result_mask(\"roi_main\", \"mask_pred\", \"attach_seg_001\");\n"
        "cxcore.attach.result_boundary(\"roi_main\", \"boundary_pred\", \"attach_seg_001\");\n"
        "cxcore.attach.result_keypoints(\"roi_main\", \"keypoints_pred\", \"attach_kp_001\");\n"
        "\n"
        "cxcore.publish.attach_record(\"attach_cls_001\");\n"
        "cxcore.publish.attach_record(\"attach_seg_001\");\n"
        "cxcore.publish.attach_record(\"attach_kp_001\");\n";
}

inline std::string TorchGeometryCxscript_StructuralEvidenceBundle()
{
    return
        "FastMatch matcher;\n"
        "\n"
        "cxcore.load.fractal_partition_object(\"fract_main\");\n"
        "cxcore.load.distance_field_object(\"dist_main\");\n"
        "cxcore.load.skeleton_object(\"skel_main\");\n"
        "cxcore.load.roi_object(\"roi_main\");\n"
        "\n"
        "cxcore.build.topology_bundle_envelope(\"topology_bundle_main\");\n"
        "cxcore.build.structural_evidence_bundle_envelope(\"matcher\", \"topology_bundle_main\", \"structural_bundle_main\");\n"
        "cxcore.summarize.structural_evidence_bundle_envelope(\"structural_bundle_main\");\n"
        "cxcore.route.structural_evidence_bundle_envelope(\"structural_bundle_main\");\n";
}

inline std::string TorchGeometryCxscript_TorchHandoffIngress()
{
    return
        "TorchGeometryHandoff geom_handoff;\n"
        "TorchFeatureSemanticHandoff semantic_handoff;\n"
        "TorchOptimizationHandoff optimization_handoff;\n"
        "\n"
        + std::string(TorchGeometryHandoffIngestCommandName()) + "(geom_handoff, \"torch_geom_main\");\n"
        + std::string(TorchFeatureSemanticHandoffIngestCommandName()) + "(semantic_handoff, \"torch_semantic_main\");\n"
        + std::string(TorchOptimizationHandoffIngestCommandName()) + "(optimization_handoff, \"torch_optimization_main\");\n"
        "\n"
        + std::string(StructuralEvidenceBundleEnvelopeSummarizeCommandName()) + "(\"structural_bundle_main\");\n"
        + std::string(StructuralEvidenceBundleEnvelopeRouteCommandName()) + "(\"structural_bundle_main\");\n";
}

inline std::string TorchGeometryCxscript_TorchHandoffSummary()
{
    return
        "TorchGeometryHandoff geom_handoff;\n"
        "TorchFeatureSemanticHandoff semantic_handoff;\n"
        "TorchOptimizationHandoff optimization_handoff;\n"
        "\n"
        + std::string(TorchGeometryHandoffIngestCommandName()) + "(geom_handoff, \"torch_geom_main\");\n"
        + std::string(TorchFeatureSemanticHandoffIngestCommandName()) + "(semantic_handoff, \"torch_semantic_main\");\n"
        + std::string(TorchOptimizationHandoffIngestCommandName()) + "(optimization_handoff, \"torch_optimization_main\");\n"
        "\n"
        + std::string(TorchGeometryHandoffSummarizeCommandName()) + "(\"torch_geom_main\");\n"
        + std::string(TorchGeometryHandoffExportCommandName()) + "(\"torch_geom_main\");\n"
        + std::string(TorchFeatureSemanticHandoffSummarizeCommandName()) + "(\"torch_semantic_main\");\n"
        + std::string(TorchFeatureSemanticHandoffExportCommandName()) + "(\"torch_semantic_main\");\n"
        + std::string(TorchOptimizationHandoffSummarizeCommandName()) + "(\"torch_optimization_main\");\n"
        + std::string(TorchOptimizationHandoffExportCommandName()) + "(\"torch_optimization_main\");\n"
        "\n"
        + std::string(TorchMeasurementKindSummarizeCommandName(TorchMeasurementKind::Circle)) + "(\"circle_measurement_main\");\n"
        + std::string(TorchMeasurementKindExportCommandName(TorchMeasurementKind::Circle)) + "(\"circle_measurement_main\");\n"
        + std::string(TorchGeometrySemanticRoleSummarizeCommandName(TorchGeometrySemanticRole_GeometryHandoff())) + "(\"torch_geom_main.geometry_anchor\");\n"
        + std::string(TorchGeometrySemanticRoleExportCommandName(TorchGeometrySemanticRole_GeometryHandoff())) + "(\"torch_geom_main.geometry_anchor\");\n";
}

inline std::string TorchGeometryCxscript_TorchHandoffObjectSummary()
{
    return
        TorchGeometryCxscript_TorchHandoffSummary() +
        std::string("\n") +
        "cxcore.publish.attach_record(\"attach-torch-geometry-001\");\n"
        + std::string(TorchGeometrySemanticRolePublishCommandName(TorchGeometrySemanticRole_GeometryHandoff())) + "(\"torch_geom_main.geometry_anchor\");\n"
        + std::string(TorchMeasurementKindPublishCommandName(TorchMeasurementKind::Circle)) + "(\"circle_measurement_main\");\n"
        "cxcore.summarize.geometry_detection_anchor(\"torch_geom_main.geometry_anchor\");\n"
        "cxcore.summarize.circle_measurement_ref(\"circle_measurement_main\");\n";
}

inline std::string TorchGeometryCxscript_TorchHandoffPublishedReadback()
{
    return
        TorchGeometryCxscript_TorchHandoffObjectSummary() +
        std::string("\n") +
        "readresult(\"published_handoff_type\");\n"
        "readresult(\"published_primary_ref\");\n"
        "readresult(\"published_route_hint\");\n"
        "readresult(\"published_route_state\");\n"
        "readresult(\"published_source_hash\");\n"
        "readresult(\"published_evidence_ref\");\n";
}

inline std::string TorchGeometryCxscript_TorchHandoffTaskSnapshot()
{
    return
        TorchGeometryCxscript_TorchHandoffPublishedReadback() +
        std::string("\n") +
        "readresult(\"published_result_ref\");\n";
}

inline std::string TorchGeometryCxscript_TorchHandoffTaskSummary()
{
    return
        TorchGeometryCxscript_TorchHandoffTaskSnapshot() +
        std::string("\n") +
        "readresult(\"published_handoff_type\");\n"
        "readresult(\"published_primary_ref\");\n"
        "readresult(\"published_route_state\");\n"
        "readresult(\"published_result_ref\");\n"
        "readresult(\"published_evidence_ref\");\n"
        "readresult(\"published_bbox_candidate_list_ref\");\n"
        "readresult(\"published_template_alignment_ref\");\n"
        "readresult(\"published_template_test_alignment_status\");\n"
        "readresult(\"published_roi_diff_candidate_ref\");\n"
        "readresult(\"published_roi_diff_candidate_count\");\n"
        "readresult(\"published_prior_roi_region_ref\");\n"
        "readresult(\"published_roi_crop_packet_ref\");\n"
        "readresult(\"published_roi_crop_count\");\n"
        "readresult(\"published_roi_crop_spatial_size\");\n"
        "readresult(\"published_roi_crop_policy_ref\");\n";
}

inline std::string TorchGeometryCxscript_TorchHandoffPublishedReview()
{
    return
        TorchGeometryCxscript_TorchHandoffTaskSummary() +
        std::string("\n") +
        "readresult(\"published_bbox_candidate_list_ref\");\n"
        "readresult(\"published_prior_roi_region_ref\");\n"
        "readresult(\"published_roi_crop_packet_ref\");\n";
}

inline std::string TorchGeometryCxscript_MinimalEndToEndRoiMaskBoundary()
{
    return
        TorchGeometryCxscript_InputPriorRoiLinePointSet() +
        std::string("\n") +
        TorchGeometryCxscript_LabelMaskBoundaryKeypoints() +
        std::string("\n") +
        TorchGeometryCxscript_AttachBackToGeometry();
}

} // namespace cxcore

#endif
