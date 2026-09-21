#ifndef CXPARSER_TORCH_GEOMETRY_ALIGNMENT_ACTIONS_H
#define CXPARSER_TORCH_GEOMETRY_ALIGNMENT_ACTIONS_H

namespace cxparser
{

struct TorchGeometryAlignmentActions
{
  static constexpr const char* ExportRoi = "cxcore.export.roi_object";
  static constexpr const char* ExportLine = "cxcore.export.line_object";
  static constexpr const char* ExportPointSet = "cxcore.export.pointset_object";
  static constexpr const char* ExportMask = "cxcore.export.mask_object";
  static constexpr const char* ExportBoundary = "cxcore.export.boundary_object";
  static constexpr const char* ExportKeypoints = "cxcore.export.keypoints_object";

  static constexpr const char* AlignInputPrior = "cxcore.align.input_prior";
  static constexpr const char* AlignTrainingLabel = "cxcore.align.training_label";

  static constexpr const char* BuildTorchRequest = "cxcore.build.torch_request";
  static constexpr const char* BuildTorchLabelPacket = "cxcore.build.torch_label_packet";
  static constexpr const char* RunTorchFullImage = "torch_module.run_full_image";

  static constexpr const char* AttachResultToRoi = "cxcore.attach.result_to_roi";
  static constexpr const char* AttachResultMaskBoundary = "cxcore.attach.result_mask_boundary";
  static constexpr const char* AttachResultKeypoints = "cxcore.attach.result_keypoints";

  static constexpr const char* PublishAttachPacket = "cxcore.publish.attach_packet";
};

} 

#endif
