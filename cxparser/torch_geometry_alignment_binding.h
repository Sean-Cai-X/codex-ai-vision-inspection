#ifndef CXPARSER_TORCH_GEOMETRY_ALIGNMENT_BINDING_H
#define CXPARSER_TORCH_GEOMETRY_ALIGNMENT_BINDING_H

#include "muParser.h"
#include "torch_geometry_alignment_actions.h"

#include <cstddef>
#include <string>

namespace cxparser
{

struct TorchGeometryBindingActionMapEntry
{
  const char* action_name;
  const char* class_name;
  const char* method_name;
  const char* phase;
};

class GeometryExportNode
{
public:
  GeometryExportNode()
    : roi_count(0)
    , line_count(0)
    , pointset_count(0)
    , mask_count(0)
    , boundary_count(0)
    , keypoints_count(0)
  {
  }

  void ExportRoi() { ++roi_count; }
  void ExportLine() { ++line_count; }
  void ExportPointSet() { ++pointset_count; }
  void ExportMask() { ++mask_count; }
  void ExportBoundary() { ++boundary_count; }
  void ExportKeypoints() { ++keypoints_count; }

  double InputPriorReady()
  {
    return (roi_count > 0 && line_count > 0 && pointset_count > 0) ? 1.0 : 0.0;
  }

  double LabelReady()
  {
    return (roi_count > 0 && mask_count > 0 && boundary_count > 0 && keypoints_count > 0) ? 1.0 : 0.0;
  }

  double TotalExports()
  {
    return static_cast<double>(roi_count + line_count + pointset_count + mask_count + boundary_count + keypoints_count);
  }

  int roi_count;
  int line_count;
  int pointset_count;
  int mask_count;
  int boundary_count;
  int keypoints_count;
};

class GeometryAlignNode
{
public:
  GeometryAlignNode()
    : input_prior_aligned(0)
    , label_aligned(0)
  {
  }

  void AlignInputPrior(double ready)
  {
    if (ready > 0.0)
      ++input_prior_aligned;
  }

  void AlignTrainingLabel(double ready)
  {
    if (ready > 0.0)
      ++label_aligned;
  }

  double InputPriorAligned() { return static_cast<double>(input_prior_aligned); }
  double TrainingLabelAligned() { return static_cast<double>(label_aligned); }

  int input_prior_aligned;
  int label_aligned;
};

class TorchGeometryBridgeNode
{
public:
  TorchGeometryBridgeNode()
    : request_count(0)
    , label_packet_count(0)
    , infer_count(0)
  {
  }

  void BuildRequest(double input_prior_aligned)
  {
    if (input_prior_aligned > 0.0)
      ++request_count;
  }

  void BuildLabelPacket(double label_aligned)
  {
    if (label_aligned > 0.0)
      ++label_packet_count;
  }

  void Infer(double request_ready, double label_ready)
  {
    if (request_ready > 0.0 && label_ready > 0.0)
      ++infer_count;
  }

  double RequestReady() { return static_cast<double>(request_count); }
  double LabelPacketReady() { return static_cast<double>(label_packet_count); }
  double InferReady() { return static_cast<double>(infer_count); }

  int request_count;
  int label_packet_count;
  int infer_count;
};

class GeometryAttachNode
{
public:
  GeometryAttachNode()
    : roi_attach_count(0)
    , mask_boundary_attach_count(0)
    , keypoints_attach_count(0)
  {
  }

  void AttachRoiResult(double infer_ready)
  {
    if (infer_ready > 0.0)
      ++roi_attach_count;
  }

  void AttachMaskBoundary(double infer_ready)
  {
    if (infer_ready > 0.0)
      ++mask_boundary_attach_count;
  }

  void AttachKeypoints(double infer_ready)
  {
    if (infer_ready > 0.0)
      ++keypoints_attach_count;
  }

  double AttachReady()
  {
    return (roi_attach_count > 0 && mask_boundary_attach_count > 0 && keypoints_attach_count > 0) ? 1.0 : 0.0;
  }

  double TotalAttach()
  {
    return static_cast<double>(roi_attach_count + mask_boundary_attach_count + keypoints_attach_count);
  }

  int roi_attach_count;
  int mask_boundary_attach_count;
  int keypoints_attach_count;
};

class PublishNode
{
public:
  PublishNode()
    : packet_count(0)
  {
  }

  void Publish(double attach_ready)
  {
    if (attach_ready > 0.0)
      ++packet_count;
  }

  double PacketCount() { return static_cast<double>(packet_count); }

  int packet_count;
};

inline void ConfigureTorchGeometryAlignmentBindings(mu::Parser& parser)
{
  double* org_double = 0;
  parser.DefineOrgClass("double", org_double);
  parser.UsingClass(true);

  GeometryExportNode* export_node = 0;
  parser.DefineClass("GeometryExport", export_node);
  parser.DefineClassFun("GeometryExport", export_node, "ExportRoi", &GeometryExportNode::ExportRoi);
  parser.DefineClassFun("GeometryExport", export_node, "ExportLine", &GeometryExportNode::ExportLine);
  parser.DefineClassFun("GeometryExport", export_node, "ExportPointSet", &GeometryExportNode::ExportPointSet);
  parser.DefineClassFun("GeometryExport", export_node, "ExportMask", &GeometryExportNode::ExportMask);
  parser.DefineClassFun("GeometryExport", export_node, "ExportBoundary", &GeometryExportNode::ExportBoundary);
  parser.DefineClassFun("GeometryExport", export_node, "ExportKeypoints", &GeometryExportNode::ExportKeypoints);
  parser.DefineClassFun("GeometryExport", export_node, "InputPriorReady", &GeometryExportNode::InputPriorReady);
  parser.DefineClassFun("GeometryExport", export_node, "LabelReady", &GeometryExportNode::LabelReady);
  parser.DefineClassFun("GeometryExport", export_node, "TotalExports", &GeometryExportNode::TotalExports);

  GeometryAlignNode* align_node = 0;
  parser.DefineClass("GeometryAlign", align_node);
  parser.DefineClassFun("GeometryAlign", align_node, "AlignInputPrior", &GeometryAlignNode::AlignInputPrior);
  parser.DefineClassFun("GeometryAlign", align_node, "AlignTrainingLabel", &GeometryAlignNode::AlignTrainingLabel);
  parser.DefineClassFun("GeometryAlign", align_node, "InputPriorAligned", &GeometryAlignNode::InputPriorAligned);
  parser.DefineClassFun("GeometryAlign", align_node, "TrainingLabelAligned", &GeometryAlignNode::TrainingLabelAligned);

  TorchGeometryBridgeNode* torch_node = 0;
  parser.DefineClass("TorchGeometryBridge", torch_node);
  parser.DefineClassFun("TorchGeometryBridge", torch_node, "BuildRequest", &TorchGeometryBridgeNode::BuildRequest);
  parser.DefineClassFun("TorchGeometryBridge", torch_node, "BuildLabelPacket", &TorchGeometryBridgeNode::BuildLabelPacket);
  parser.DefineClassFun("TorchGeometryBridge", torch_node, "Infer", &TorchGeometryBridgeNode::Infer);
  parser.DefineClassFun("TorchGeometryBridge", torch_node, "RequestReady", &TorchGeometryBridgeNode::RequestReady);
  parser.DefineClassFun("TorchGeometryBridge", torch_node, "LabelPacketReady", &TorchGeometryBridgeNode::LabelPacketReady);
  parser.DefineClassFun("TorchGeometryBridge", torch_node, "InferReady", &TorchGeometryBridgeNode::InferReady);

  GeometryAttachNode* attach_node = 0;
  parser.DefineClass("GeometryAttach", attach_node);
  parser.DefineClassFun("GeometryAttach", attach_node, "AttachRoiResult", &GeometryAttachNode::AttachRoiResult);
  parser.DefineClassFun("GeometryAttach", attach_node, "AttachMaskBoundary", &GeometryAttachNode::AttachMaskBoundary);
  parser.DefineClassFun("GeometryAttach", attach_node, "AttachKeypoints", &GeometryAttachNode::AttachKeypoints);
  parser.DefineClassFun("GeometryAttach", attach_node, "AttachReady", &GeometryAttachNode::AttachReady);
  parser.DefineClassFun("GeometryAttach", attach_node, "TotalAttach", &GeometryAttachNode::TotalAttach);

  PublishNode* publish_node = 0;
  parser.DefineClass("GeometryPublish", publish_node);
  parser.DefineClassFun("GeometryPublish", publish_node, "Publish", &PublishNode::Publish);
  parser.DefineClassFun("GeometryPublish", publish_node, "PacketCount", &PublishNode::PacketCount);
}

inline std::string BuildTorchGeometryAlignmentSmokeExpr()
{
  return
    "GeometryExport exporter;"
    "GeometryAlign aligner;"
    "TorchGeometryBridge torch;"
    "GeometryAttach attach;"
    "GeometryPublish publish;"
    "exporter.ExportRoi();"
    "exporter.ExportLine();"
    "exporter.ExportPointSet();"
    "aligner.AlignInputPrior(exporter.InputPriorReady());"
    "torch.BuildRequest(aligner.InputPriorAligned());"
    "exporter.ExportMask();"
    "exporter.ExportBoundary();"
    "exporter.ExportKeypoints();"
    "aligner.AlignTrainingLabel(exporter.LabelReady());"
    "torch.BuildLabelPacket(aligner.TrainingLabelAligned());"
    "torch.Infer(torch.RequestReady(), torch.LabelPacketReady());"
    "attach.AttachRoiResult(torch.InferReady());"
    "attach.AttachMaskBoundary(torch.InferReady());"
    "attach.AttachKeypoints(torch.InferReady());"
    "publish.Publish(attach.AttachReady());"
    "export_count=exporter.TotalExports();"
    "attach_count=attach.TotalAttach();"
    "packet_count=publish.PacketCount();";
}

inline const TorchGeometryBindingActionMapEntry* TorchGeometryBindingActionMap()
{
  static const TorchGeometryBindingActionMapEntry kEntries[] = {
    {TorchGeometryAlignmentActions::ExportRoi, "GeometryExport", "ExportRoi", "export"},
    {TorchGeometryAlignmentActions::ExportLine, "GeometryExport", "ExportLine", "export"},
    {TorchGeometryAlignmentActions::ExportPointSet, "GeometryExport", "ExportPointSet", "export"},
    {TorchGeometryAlignmentActions::ExportMask, "GeometryExport", "ExportMask", "export"},
    {TorchGeometryAlignmentActions::ExportBoundary, "GeometryExport", "ExportBoundary", "export"},
    {TorchGeometryAlignmentActions::ExportKeypoints, "GeometryExport", "ExportKeypoints", "export"},
    {TorchGeometryAlignmentActions::AlignInputPrior, "GeometryAlign", "AlignInputPrior", "align"},
    {TorchGeometryAlignmentActions::AlignTrainingLabel, "GeometryAlign", "AlignTrainingLabel", "align"},
    {TorchGeometryAlignmentActions::BuildTorchRequest, "TorchGeometryBridge", "BuildRequest", "bridge"},
    {TorchGeometryAlignmentActions::BuildTorchLabelPacket, "TorchGeometryBridge", "BuildLabelPacket", "bridge"},
    {TorchGeometryAlignmentActions::RunTorchFullImage, "TorchGeometryBridge", "Infer", "bridge"},
    {TorchGeometryAlignmentActions::AttachResultToRoi, "GeometryAttach", "AttachRoiResult", "attach"},
    {TorchGeometryAlignmentActions::AttachResultMaskBoundary, "GeometryAttach", "AttachMaskBoundary", "attach"},
    {TorchGeometryAlignmentActions::AttachResultKeypoints, "GeometryAttach", "AttachKeypoints", "attach"},
    {TorchGeometryAlignmentActions::PublishAttachPacket, "GeometryPublish", "Publish", "publish"},
  };
  return kEntries;
}

inline std::size_t TorchGeometryBindingActionMapSize()
{
  return 15;
}

inline const TorchGeometryBindingActionMapEntry* FindTorchGeometryBindingAction(const std::string& action_name)
{
  const TorchGeometryBindingActionMapEntry* entries = TorchGeometryBindingActionMap();
  for (std::size_t i = 0; i < TorchGeometryBindingActionMapSize(); ++i)
  {
    if (action_name == entries[i].action_name)
      return &entries[i];
  }
  return 0;
}

} 

#endif
