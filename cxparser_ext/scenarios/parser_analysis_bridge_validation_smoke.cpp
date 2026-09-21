#include <iostream>
#include <string>

#include "../meta/parser_evidence.h"
#include "../meta/parser_validation_types.h"
#include "../pipeline/parser_analysis_bridge_exporter.h"
#include "../pipeline/parser_ir_radare_matcher.h"
#include "../pipeline/parser_analysis_bridge_types.h"
#include "../pipeline/parser_task_types.h"
#include "../validation/parser_validation_engine.h"

namespace
{
bool HasIssueCode(const cxparser_ext::ParserValidationReport &report,
                  const std::string &code,
                  cxparser_ext::ValidationLevel expected_level);

cxparser_ext::ParserIrRadareMatchResult MakeRadareMatchResult(const std::string &step_name,
                                                              const std::string &step_symbol,
                                                              const std::string &call_name,
                                                              const std::string &matched_symbol,
                                                              bool matched,
                                                              const std::string &point_kind,
                                                              const std::string &point_name)
{
  cxparser_ext::ParserIrRadareMatchResult result;

  cxparser_ext::ParserIrRadareStepMatch step_match;
  step_match.step_name = step_name;
  step_match.matched_symbol = step_symbol;
  step_match.matched = !step_symbol.empty();
  result.matched_steps.push_back(step_match);

  cxparser_ext::ParserIrRadareMatch call_match;
  call_match.step_name = step_name;
  call_match.call_name = call_name;
  call_match.matched_symbol = matched_symbol;
  call_match.matched = matched;
  result.matched_calls.push_back(call_match);

  if (!matched)
    result.unresolved_calls.push_back(call_name);

  cxparser_ext::ParserIrRadareEvidencePoint point;
  point.point_kind = point_kind;
  point.point_name = point_name;
  point.step_name = step_name;
  point.related_symbol = step_symbol;
  result.evidence_points.push_back(point);

  return result;
}

bool RunBridgeValidationCase(const cxparser_ext::ParserAnalysisBridgeResult &bridge_result,
                             const std::string &summary,
                             const std::string &matched_code,
                             const std::string &unresolved_code,
                             bool expect_warning)
{
  cxparser_ext::ParserEvidenceBundle bundle;
  cxparser_ext::ParserTraceEntry trace_entry;
  trace_entry.sequence = 1;
  trace_entry.trace_id = bridge_result.bridge_name + ".trace";
  trace_entry.stage = bridge_result.bridge_stage;
  trace_entry.action = "bridge_validate";
  trace_entry.status = "ok";
  trace_entry.detail = "trace:" + bridge_result.bridge_name;
  bundle.trace_entries.push_back(trace_entry);

  cxparser_ext::ParserLogEntry log_entry;
  log_entry.trace_id = trace_entry.trace_id;
  log_entry.level = "info";
  log_entry.stage = bridge_result.bridge_stage;
  log_entry.code = "bridge_validation";
  log_entry.message = "log:" + bridge_result.bridge_name;
  bundle.log_entries.push_back(log_entry);

  cxparser_ext::ParserAnalysisBridgeExporter exporter;
  if (!exporter.BuildEvidence(bridge_result, bundle))
  {
    std::cerr << "[FAIL] " << bridge_result.bridge_name << " bridge evidence export failed\n";
    return false;
  }

  cxparser_ext::ExecutionResult result;
  result.success = true;
  result.text_result = summary;

  cxparser_ext::ParserValidationEngine engine;
  cxparser_ext::ParserValidationReport report;
  if (!engine.CompareExecutionAndEvidence(result, bundle, report))
  {
    std::cerr << "[FAIL] " << bridge_result.bridge_name << " bridge validation compare failed\n";
    return false;
  }

  if (!report.passed ||
      !HasIssueCode(report, matched_code, cxparser_ext::vl_info) ||
      (expect_warning && !HasIssueCode(report, unresolved_code, cxparser_ext::vl_warning)))
  {
    std::cerr << "[FAIL] " << bridge_result.bridge_name << " bridge validation mismatch\n";
    return false;
  }

  std::cout << "[PASS] bridge validation " << bridge_result.bridge_name
            << " status=" << static_cast<int>(bridge_result.status)
            << " issues=" << report.issues.size() << "\n";
  return true;
}

bool HasIssueCode(const cxparser_ext::ParserValidationReport &report,
                  const std::string &code,
                  cxparser_ext::ValidationLevel expected_level)
{
  for (size_t i = 0; i < report.issues.size(); ++i)
  {
    if (report.issues[i].code == code && report.issues[i].level == expected_level)
      return true;
  }
  return false;
}

bool RunRadareBridgeValidationCase()
{
  cxparser_ext::ParserIrRadareMatcher matcher;
  cxparser_ext::ParserIrRadareMatchResult match_result =
    MakeRadareMatchResult("infer", "TorchYoloInfer", "infer_min", "infer_min", true, "checkpoint", "after_infer");

  cxparser_ext::ParserAnalysisBridgeResult bridge_result;
  if (!matcher.BuildExecutionBridgeResult(match_result, bridge_result))
  {
    std::cerr << "[FAIL] radare execution bridge build failed\n";
    return false;
  }

  return RunBridgeValidationCase(bridge_result,
                                 "radare execution bridge validation",
                                 "radare_call_matched",
                                 "radare_call_unresolved",
                                 false);
}

bool RunRadareFlowBridgeValidationCase()
{
  cxparser_ext::ParserIrRadareMatcher matcher;
  cxparser_ext::ParserIrRadareMatchResult match_result =
    MakeRadareMatchResult("infer", "TorchYoloInfer", "infer_min", "infer_min", true, "breakpoint", "infer_bp");

  cxparser_ext::ParserAnalysisBridgeResult bridge_result;
  if (!matcher.BuildFlowBridgeResult(match_result, bridge_result))
  {
    std::cerr << "[FAIL] radare flow bridge build failed\n";
    return false;
  }

  return RunBridgeValidationCase(bridge_result,
                                 "radare flow bridge validation",
                                 "radare_call_matched",
                                 "radare_call_unresolved",
                                 false);
}

bool RunClangBridgeValidationCase()
{
  cxparser_ext::ParserAnalysisBridgeResult bridge_result;
  bridge_result.bridge_name = "clang";
  bridge_result.bridge_stage = "clang_bridge";
  bridge_result.category = "schema_analysis";
  bridge_result.status = cxparser_ext::pabs_partial;
  bridge_result.origin = cxparser_ext::pabo_flow;
  bridge_result.summary = "clang bridge produced partial call coverage";
  bridge_result.reason = "one or more schema calls could not be mapped";
  bridge_result.metrics.matched_call_count = 1;
  bridge_result.metrics.unresolved_call_count = 1;

  cxparser_ext::ParserAnalysisBridgeCall call;
  call.step_name = "Infer";
  call.call_name = "infer_min";
  call.matched_symbol = "torch::Infer::infer_min";
  call.matched = true;
  call.category = "schema_call";
  bridge_result.calls.push_back(call);

  cxparser_ext::ParserAnalysisBridgeCall unresolved_call;
  unresolved_call.step_name = "Infer";
  unresolved_call.call_name = "missing_symbol";
  unresolved_call.matched = false;
  unresolved_call.category = "schema_call";
  bridge_result.calls.push_back(unresolved_call);

  bridge_result.notes.push_back("flow statements=4");
  return RunBridgeValidationCase(bridge_result,
                                 "clang bridge validation",
                                 "clang_call_matched",
                                 "clang_call_unresolved",
                                 true);
}
}

int main()
{
  if (!RunRadareBridgeValidationCase())
    return 1;
  if (!RunRadareFlowBridgeValidationCase())
    return 1;
  if (!RunClangBridgeValidationCase())
    return 1;

  std::cout << "[PASS] parser_analysis_bridge_validation_smoke\n";
  return 0;
}
