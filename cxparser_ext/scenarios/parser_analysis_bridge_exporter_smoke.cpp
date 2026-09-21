#include <iostream>
#include <string>

#include "../meta/parser_evidence.h"
#include "../pipeline/parser_analysis_bridge_exporter.h"
#include "../pipeline/parser_analysis_bridge_types.h"

namespace
{
bool RunRadareBridgeExportCase()
{
  cxparser_ext::ParserAnalysisBridgeResult bridge_result;
  bridge_result.bridge_name = "radare";
  bridge_result.bridge_stage = "radare_bridge";
  bridge_result.category = "binary_analysis";
  bridge_result.status = cxparser_ext::pabs_resolved;
  bridge_result.origin = cxparser_ext::pabo_execution;
  bridge_result.summary = "radare bridge resolved all matched calls";
  bridge_result.reason = "all execution and flow calls mapped to binary symbols";
  bridge_result.metrics.matched_call_count = 1;
  bridge_result.metrics.step_count = 1;
  bridge_result.metrics.point_count = 1;

  cxparser_ext::ParserAnalysisBridgeCall call;
  call.step_name = "infer";
  call.call_name = "infer_min";
  call.matched_symbol = "infer_min";
  call.matched = true;
  call.category = "call_match";
  bridge_result.calls.push_back(call);

  cxparser_ext::ParserAnalysisBridgeStep step;
  step.step_name = "infer";
  step.matched_symbol = "TorchYoloInfer";
  step.matched = true;
  step.category = "step_match";
  bridge_result.steps.push_back(step);

  cxparser_ext::ParserAnalysisBridgePoint point;
  point.point_kind = "breakpoint";
  point.point_name = "infer_bp";
  point.step_name = "infer";
  point.related_symbol = "TorchYoloInfer";
  point.category = "evidence_point";
  bridge_result.points.push_back(point);
  bridge_result.notes.push_back("matched exec call: infer_min -> infer_min");

  cxparser_ext::ParserEvidenceBundle bundle;
  cxparser_ext::ParserAnalysisBridgeExporter exporter;
  if (!exporter.BuildEvidence(bridge_result, bundle))
  {
    std::cerr << "[FAIL] radare bridge export failed\n";
    return false;
  }

  if (bundle.events.size() != 1 ||
      bundle.events[0].stage != "radare_bridge" ||
      bundle.events[0].code != "radare_call_matched" ||
      bundle.events[0].expected != "call should map through analysis bridge (call_match) [resolved]" ||
      bundle.notes.size() < 7)
  {
    std::cerr << "[FAIL] radare bridge export mismatch\n";
    return false;
  }

  std::cout << "[PASS] bridge export radare event=" << bundle.events[0].code
            << " notes=" << bundle.notes.size() << "\n";
  return true;
}

bool RunClangBridgeExportCase()
{
  cxparser_ext::ParserAnalysisBridgeResult bridge_result;
  bridge_result.bridge_name = "clang";
  bridge_result.bridge_stage = "clang_bridge";
  bridge_result.category = "schema_analysis";
  bridge_result.status = cxparser_ext::pabs_resolved;
  bridge_result.origin = cxparser_ext::pabo_schema;
  bridge_result.summary = "clang schema bridge exported schema classes";
  bridge_result.reason = "schema classes and methods were exported into analysis bridge results";
  bridge_result.metrics.matched_call_count = 1;
  bridge_result.metrics.step_count = 1;
  bridge_result.metrics.point_count = 1;

  cxparser_ext::ParserAnalysisBridgeCall call;
  call.step_name = "Infer";
  call.call_name = "infer_min";
  call.matched_symbol = "torch::Infer::infer_min";
  call.matched = true;
  call.category = "schema_call";
  bridge_result.calls.push_back(call);

  cxparser_ext::ParserAnalysisBridgeStep step;
  step.step_name = "Infer";
  step.matched_symbol = "torch::Infer";
  step.matched = true;
  step.category = "schema_step";
  bridge_result.steps.push_back(step);

  cxparser_ext::ParserAnalysisBridgePoint point;
  point.point_kind = "type_decl";
  point.point_name = "Infer";
  point.step_name = "Infer";
  point.step_id = 1;
  point.frame_id = 0;
  point.span.line_begin = 6;
  point.block_depth = 1;
  point.related_symbol = "torch::Infer";
  point.matched = true;
  point.category = "type_decl";
  bridge_result.points.push_back(point);
  bridge_result.notes.push_back("flow statements=4");

  cxparser_ext::ParserEvidenceBundle bundle;
  cxparser_ext::ParserAnalysisBridgeExporter exporter;
  if (!exporter.BuildEvidence(bridge_result, bundle))
  {
    std::cerr << "[FAIL] clang bridge export failed\n";
    return false;
  }

  bool saw_type_point_note = false;
  for (size_t i = 0; i < bundle.notes.size(); ++i)
  {
    if (bundle.notes[i].find("clang point type_decl Infer -> torch::Infer [matched] step=Infer step_id=1 line=6 depth=1 [type_decl]") != std::string::npos)
      saw_type_point_note = true;
  }

  if (bundle.events.size() != 1 ||
      bundle.events[0].stage != "clang_bridge" ||
      bundle.events[0].code != "clang_call_matched" ||
      bundle.events[0].expected != "call should map through analysis bridge (schema_call) [resolved]" ||
      bundle.notes.size() < 7 ||
      !saw_type_point_note)
  {
    std::cerr << "[FAIL] clang bridge export mismatch\n";
    return false;
  }

  std::cout << "[PASS] bridge export clang event=" << bundle.events[0].code
            << " notes=" << bundle.notes.size() << "\n";
  return true;
}
}

int main()
{
  if (!RunRadareBridgeExportCase())
    return 1;
  if (!RunClangBridgeExportCase())
    return 1;

  std::cout << "[PASS] parser_analysis_bridge_exporter_smoke\n";
  return 0;
}
