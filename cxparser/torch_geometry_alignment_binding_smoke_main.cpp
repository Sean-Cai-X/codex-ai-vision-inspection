#include "torch_geometry_alignment_binding.h"

#include <exception>
#include <iostream>

namespace
{

bool Check(bool condition, const char* message)
{
  if (!condition)
  {
    std::cerr << "[FAIL] " << message << std::endl;
    return false;
  }
  return true;
}

bool CheckActionMapEntry(
  const std::string& action_name,
  const char* expected_class,
  const char* expected_method)
{
  const cxparser::TorchGeometryBindingActionMapEntry* entry =
    cxparser::FindTorchGeometryBindingAction(action_name);
  if (!Check(entry != 0, action_name.c_str()))
    return false;
  if (!Check(std::string(entry->class_name) == expected_class, "binding action class mismatch"))
    return false;
  return Check(std::string(entry->method_name) == expected_method, "binding action method mismatch");
}

} 

int main()
{
  try
  {
    mu::Parser parser;
    cxparser::ConfigureTorchGeometryAlignmentBindings(parser);

    double export_count = 0.0;
    double packet_count = 0.0;
    double attach_count = 0.0;
    parser.DefineVar("export_count", &export_count);
    parser.DefineVar("packet_count", &packet_count);
    parser.DefineVar("attach_count", &attach_count);

    parser.SetExpr(cxparser::BuildTorchGeometryAlignmentSmokeExpr());
    parser.Eval();

    cxparser::GeometryExportNode* exporter = static_cast<cxparser::GeometryExportNode*>(parser.GetClassObj("GeometryExport", "exporter"));
    cxparser::GeometryAlignNode* aligner = static_cast<cxparser::GeometryAlignNode*>(parser.GetClassObj("GeometryAlign", "aligner"));
    cxparser::TorchGeometryBridgeNode* torch = static_cast<cxparser::TorchGeometryBridgeNode*>(parser.GetClassObj("TorchGeometryBridge", "torch"));
    cxparser::GeometryAttachNode* attach = static_cast<cxparser::GeometryAttachNode*>(parser.GetClassObj("GeometryAttach", "attach"));
    cxparser::PublishNode* publish = static_cast<cxparser::PublishNode*>(parser.GetClassObj("GeometryPublish", "publish"));

    if (!Check(exporter != 0, "exporter object missing")) return 1;
    if (!Check(aligner != 0, "aligner object missing")) return 1;
    if (!Check(torch != 0, "torch bridge object missing")) return 1;
    if (!Check(attach != 0, "attach object missing")) return 1;
    if (!Check(publish != 0, "publish object missing")) return 1;

    if (!Check(exporter->roi_count == 1, "roi export count mismatch")) return 1;
    if (!Check(exporter->line_count == 1, "line export count mismatch")) return 1;
    if (!Check(exporter->pointset_count == 1, "pointset export count mismatch")) return 1;
    if (!Check(exporter->mask_count == 1, "mask export count mismatch")) return 1;
    if (!Check(exporter->boundary_count == 1, "boundary export count mismatch")) return 1;
    if (!Check(exporter->keypoints_count == 1, "keypoints export count mismatch")) return 1;
    if (!Check(aligner->input_prior_aligned == 1, "input-prior alignment count mismatch")) return 1;
    if (!Check(aligner->label_aligned == 1, "training-label alignment count mismatch")) return 1;
    if (!Check(torch->request_count == 1, "torch request count mismatch")) return 1;
    if (!Check(torch->label_packet_count == 1, "torch label packet count mismatch")) return 1;
    if (!Check(torch->infer_count == 1, "torch infer count mismatch")) return 1;
    if (!Check(attach->roi_attach_count == 1, "roi attach count mismatch")) return 1;
    if (!Check(attach->mask_boundary_attach_count == 1, "mask/boundary attach count mismatch")) return 1;
    if (!Check(attach->keypoints_attach_count == 1, "keypoints attach count mismatch")) return 1;
    if (!Check(publish->packet_count == 1, "publish packet count mismatch")) return 1;
    if (!Check(export_count == 6.0, "script export_count mismatch")) return 1;
    if (!Check(attach_count == 3.0, "script attach_count mismatch")) return 1;
    if (!Check(packet_count == 1.0, "script packet_count mismatch")) return 1;
    if (!Check(cxparser::TorchGeometryBindingActionMapSize() == 15, "binding action map size mismatch")) return 1;
    if (!CheckActionMapEntry(cxparser::TorchGeometryAlignmentActions::ExportRoi, "GeometryExport", "ExportRoi")) return 1;
    if (!CheckActionMapEntry(cxparser::TorchGeometryAlignmentActions::ExportLine, "GeometryExport", "ExportLine")) return 1;
    if (!CheckActionMapEntry(cxparser::TorchGeometryAlignmentActions::ExportPointSet, "GeometryExport", "ExportPointSet")) return 1;
    if (!CheckActionMapEntry(cxparser::TorchGeometryAlignmentActions::ExportMask, "GeometryExport", "ExportMask")) return 1;
    if (!CheckActionMapEntry(cxparser::TorchGeometryAlignmentActions::ExportBoundary, "GeometryExport", "ExportBoundary")) return 1;
    if (!CheckActionMapEntry(cxparser::TorchGeometryAlignmentActions::ExportKeypoints, "GeometryExport", "ExportKeypoints")) return 1;
    if (!CheckActionMapEntry(cxparser::TorchGeometryAlignmentActions::AlignInputPrior, "GeometryAlign", "AlignInputPrior")) return 1;
    if (!CheckActionMapEntry(cxparser::TorchGeometryAlignmentActions::AlignTrainingLabel, "GeometryAlign", "AlignTrainingLabel")) return 1;
    if (!CheckActionMapEntry(cxparser::TorchGeometryAlignmentActions::BuildTorchRequest, "TorchGeometryBridge", "BuildRequest")) return 1;
    if (!CheckActionMapEntry(cxparser::TorchGeometryAlignmentActions::BuildTorchLabelPacket, "TorchGeometryBridge", "BuildLabelPacket")) return 1;
    if (!CheckActionMapEntry(cxparser::TorchGeometryAlignmentActions::RunTorchFullImage, "TorchGeometryBridge", "Infer")) return 1;
    if (!CheckActionMapEntry(cxparser::TorchGeometryAlignmentActions::AttachResultToRoi, "GeometryAttach", "AttachRoiResult")) return 1;
    if (!CheckActionMapEntry(cxparser::TorchGeometryAlignmentActions::AttachResultMaskBoundary, "GeometryAttach", "AttachMaskBoundary")) return 1;
    if (!CheckActionMapEntry(cxparser::TorchGeometryAlignmentActions::AttachResultKeypoints, "GeometryAttach", "AttachKeypoints")) return 1;
    if (!CheckActionMapEntry(cxparser::TorchGeometryAlignmentActions::PublishAttachPacket, "GeometryPublish", "Publish")) return 1;

    std::cout << "[TORCH-GEOM-BINDING] passed" << std::endl;
    return 0;
  }
  catch (const mu::Parser::exception_type& ex)
  {
    std::cerr << "[PARSER-EXCEPTION] " << ex.GetMsg() << std::endl;
    std::cerr << "[TOKEN] " << ex.GetToken() << std::endl;
    return 2;
  }
  catch (const std::exception& ex)
  {
    std::cerr << "[EXCEPTION] " << ex.what() << std::endl;
    return 3;
  }
}
