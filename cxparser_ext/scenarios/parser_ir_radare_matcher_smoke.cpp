#include <iostream>
#include <string>

#include "../adapters/radare/radare_types.h"
#include "../meta/parser_evidence.h"
#include "../pipeline/parser_cxscript_runtime.h"
#include "../pipeline/parser_ir_radare_matcher.h"

namespace
{
bool RunMatchSuccessCase()
{
  cxparser_ext::ParserCxScriptRuntime runtime;
  cxparser_ext::CxScriptExecutionContext context;
  cxparser_ext::CxScriptFlow flow;
  cxparser_ext::CxScriptParseError parse_error;
  cxparser_ext::CxScriptExecutionResult execution_result;
  std::string error_message;

  const char *script =
    "kind=integration\n"
    "layer=infer\n"
    "module=torch\n"
    "mode=run\n"
    "step infer {\n"
    "breakpoint infer_bp;\n"
    "call infer_min(frame);\n"
    "}\n";

  if (!runtime.ParseScriptFlow("match_success.cxs", script, context, flow, parse_error, error_message))
  {
    std::cerr << "[FAIL] parse success case failed: " << error_message << "\n";
    return false;
  }

  if (!runtime.ExecuteScriptText("match_success.cxs", script, execution_result))
  {
    std::cerr << "[FAIL] execute success case failed: " << execution_result.summary << "\n";
    return false;
  }

  cxparser_ext::RadareAnalysisResult analysis;
  analysis.success = true;
  analysis.current_symbol = "TorchYoloInfer";

  cxparser_ext::RadareFunctionInfo fn;
  fn.name = "TorchYoloInfer";
  fn.address = "0x401000";
  fn.call_targets.push_back("infer_min");
  analysis.functions.push_back(fn);

  cxparser_ext::ParserIrRadareMatcher matcher;
  cxparser_ext::ParserIrRadareMatchResult match_result;
  cxparser_ext::ParserIrRadareMatchResult exec_match_result;
  cxparser_ext::ParserAnalysisBridgeResult flow_bridge_result;
  cxparser_ext::ParserAnalysisBridgeResult exec_bridge_result;
  cxparser_ext::ParserEvidenceBundle bundle;
  if (!matcher.MatchFlow(flow, analysis, match_result))
  {
    std::cerr << "[FAIL] matcher success case failed\n";
    return false;
  }

  if (!matcher.MatchExecution(execution_result, analysis, exec_match_result))
  {
    std::cerr << "[FAIL] execution matcher success case failed\n";
    return false;
  }

  if (!matcher.BuildEvidence(exec_match_result, bundle))
  {
    std::cerr << "[FAIL] matcher evidence build failed\n";
    return false;
  }

  if (!matcher.BuildFlowBridgeResult(match_result, flow_bridge_result))
  {
    std::cerr << "[FAIL] matcher flow bridge build failed\n";
    return false;
  }

  if (!matcher.BuildExecutionBridgeResult(exec_match_result, exec_bridge_result))
  {
    std::cerr << "[FAIL] matcher execution bridge build failed\n";
    return false;
  }

  if (match_result.matched_calls.size() != 1 ||
      !match_result.matched_calls[0].matched ||
      match_result.matched_calls[0].matched_symbol != "infer_min" ||
      !match_result.unresolved_calls.empty())
  {
    std::cerr << "[FAIL] matcher success result mismatch\n";
    return false;
  }

  if (match_result.matched_steps.size() != 1 ||
      !match_result.matched_steps[0].matched ||
      match_result.matched_steps[0].step_name != "infer" ||
      match_result.matched_steps[0].matched_symbol != "TorchYoloInfer")
  {
    std::cerr << "[FAIL] matcher success step mismatch\n";
    return false;
  }

  if (match_result.evidence_points.size() != 1 ||
      match_result.evidence_points[0].point_kind != "breakpoint" ||
      match_result.evidence_points[0].point_name != "infer_bp" ||
      match_result.evidence_points[0].related_symbol != "TorchYoloInfer")
  {
    std::cerr << "[FAIL] matcher success evidence mismatch\n";
    return false;
  }

  if (exec_match_result.matched_calls.size() != 1 ||
      !exec_match_result.matched_calls[0].matched ||
      exec_match_result.matched_calls[0].call_name != "infer_min" ||
      exec_match_result.matched_steps.size() != 1 ||
      exec_match_result.matched_steps[0].step_name != "infer" ||
      exec_match_result.evidence_points.size() != 1 ||
      exec_match_result.evidence_points[0].point_name != "infer_bp")
  {
    std::cerr << "[FAIL] execution matcher success mismatch\n";
    return false;
  }

  if (bundle.events.size() != 1 ||
      bundle.events[0].code != "radare_call_matched" ||
      bundle.events[0].actual != "infer_min" ||
      bundle.notes.size() < 3)
  {
    std::cerr << "[FAIL] matcher success evidence bundle mismatch\n";
    return false;
  }

  if (flow_bridge_result.bridge_name != "radare" ||
      flow_bridge_result.origin != cxparser_ext::pabo_flow ||
      flow_bridge_result.calls.size() != 1 ||
      !flow_bridge_result.calls[0].matched ||
      flow_bridge_result.steps.size() != 1 ||
      flow_bridge_result.points.size() != 1 ||
      exec_bridge_result.bridge_name != "radare" ||
      exec_bridge_result.origin != cxparser_ext::pabo_execution ||
      exec_bridge_result.calls.size() != 1 ||
      !exec_bridge_result.calls[0].matched ||
      exec_bridge_result.steps.size() != 1 ||
      exec_bridge_result.points.size() != 1)
  {
    std::cerr << "[FAIL] matcher success bridge result mismatch\n";
    return false;
  }

  std::cout << "[PASS] matcher success call=" << match_result.matched_calls[0].call_name
            << " symbol=" << match_result.matched_calls[0].matched_symbol
            << " step=" << match_result.matched_calls[0].step_name
            << " step_symbol=" << match_result.matched_steps[0].matched_symbol
            << " evidence=" << match_result.evidence_points[0].point_name
            << " exec_call=" << exec_match_result.matched_calls[0].call_name
            << " flow_bridge=" << flow_bridge_result.bridge_name
            << " exec_bridge=" << exec_bridge_result.bridge_name << "\n";
  return true;
}

bool RunMatchFailureCase()
{
  cxparser_ext::ParserCxScriptRuntime runtime;
  cxparser_ext::CxScriptExecutionContext context;
  cxparser_ext::CxScriptFlow flow;
  cxparser_ext::CxScriptParseError parse_error;
  cxparser_ext::CxScriptExecutionResult execution_result;
  std::string error_message;

  const char *script =
    "kind=module\n"
    "layer=train\n"
    "module=mlpack\n"
    "mode=run\n"
    "step train {\n"
    "checkpoint after_train;\n"
    "call train_min(config);\n"
    "}\n";

  if (!runtime.ParseScriptFlow("match_failure.cxs", script, context, flow, parse_error, error_message))
  {
    std::cerr << "[FAIL] parse failure case failed: " << error_message << "\n";
    return false;
  }

  if (!runtime.ExecuteScriptText("match_failure.cxs", script, execution_result))
  {
    std::cerr << "[FAIL] execute failure case failed: " << execution_result.summary << "\n";
    return false;
  }

  cxparser_ext::RadareAnalysisResult analysis;
  analysis.success = true;
  analysis.current_symbol = "MlpackTrain";

  cxparser_ext::RadareFunctionInfo fn;
  fn.name = "MlpackTrain";
  fn.address = "0x402000";
  fn.call_targets.push_back("fit_epoch");
  analysis.functions.push_back(fn);

  cxparser_ext::ParserIrRadareMatcher matcher;
  cxparser_ext::ParserIrRadareMatchResult match_result;
  cxparser_ext::ParserIrRadareMatchResult exec_match_result;
  cxparser_ext::ParserAnalysisBridgeResult flow_bridge_result;
  cxparser_ext::ParserAnalysisBridgeResult exec_bridge_result;
  cxparser_ext::ParserEvidenceBundle bundle;
  if (!matcher.MatchFlow(flow, analysis, match_result))
  {
    std::cerr << "[FAIL] matcher failure case failed\n";
    return false;
  }

  if (!matcher.MatchExecution(execution_result, analysis, exec_match_result))
  {
    std::cerr << "[FAIL] execution matcher failure case failed\n";
    return false;
  }

  if (!matcher.BuildEvidence(exec_match_result, bundle))
  {
    std::cerr << "[FAIL] matcher failure evidence build failed\n";
    return false;
  }

  if (!matcher.BuildFlowBridgeResult(match_result, flow_bridge_result))
  {
    std::cerr << "[FAIL] matcher failure flow bridge build failed\n";
    return false;
  }

  if (!matcher.BuildExecutionBridgeResult(exec_match_result, exec_bridge_result))
  {
    std::cerr << "[FAIL] matcher failure execution bridge build failed\n";
    return false;
  }

  if (match_result.matched_calls.size() != 1 ||
      match_result.matched_calls[0].matched ||
      match_result.unresolved_calls.size() != 1 ||
      match_result.unresolved_calls[0] != "train_min")
  {
    std::cerr << "[FAIL] matcher failure result mismatch\n";
    return false;
  }

  if (match_result.matched_steps.size() != 1 ||
      !match_result.matched_steps[0].matched ||
      match_result.matched_steps[0].matched_symbol != "MlpackTrain")
  {
    std::cerr << "[FAIL] matcher failure step mismatch\n";
    return false;
  }

  if (match_result.evidence_points.size() != 1 ||
      match_result.evidence_points[0].point_kind != "checkpoint" ||
      match_result.evidence_points[0].point_name != "after_train" ||
      match_result.evidence_points[0].related_symbol != "MlpackTrain")
  {
    std::cerr << "[FAIL] matcher failure evidence mismatch\n";
    return false;
  }

  if (exec_match_result.unresolved_calls.size() != 1 ||
      exec_match_result.unresolved_calls[0] != "train_min" ||
      exec_match_result.matched_steps.size() != 1 ||
      exec_match_result.matched_steps[0].matched_symbol != "MlpackTrain" ||
      exec_match_result.evidence_points.size() != 1 ||
      exec_match_result.evidence_points[0].point_name != "after_train")
  {
    std::cerr << "[FAIL] execution matcher failure mismatch\n";
    return false;
  }

  if (bundle.events.size() != 1 ||
      bundle.events[0].code != "radare_call_unresolved" ||
      bundle.events[0].actual != "train_min" ||
      bundle.notes.size() < 3)
  {
    std::cerr << "[FAIL] matcher failure evidence bundle mismatch\n";
    return false;
  }

  if (flow_bridge_result.bridge_name != "radare" ||
      flow_bridge_result.origin != cxparser_ext::pabo_flow ||
      flow_bridge_result.calls.size() != 1 ||
      flow_bridge_result.calls[0].matched ||
      flow_bridge_result.unresolved_calls.size() != 1 ||
      flow_bridge_result.unresolved_calls[0] != "train_min" ||
      exec_bridge_result.bridge_name != "radare" ||
      exec_bridge_result.origin != cxparser_ext::pabo_execution ||
      exec_bridge_result.calls.size() != 1 ||
      exec_bridge_result.calls[0].matched ||
      exec_bridge_result.unresolved_calls.size() != 1 ||
      exec_bridge_result.unresolved_calls[0] != "train_min")
  {
    std::cerr << "[FAIL] matcher failure bridge result mismatch\n";
    return false;
  }

  std::cout << "[PASS] matcher unresolved call=" << match_result.unresolved_calls[0]
            << " step=" << match_result.matched_calls[0].step_name
            << " step_symbol=" << match_result.matched_steps[0].matched_symbol
            << " evidence=" << match_result.evidence_points[0].point_name
            << " exec_unresolved=" << exec_match_result.unresolved_calls[0]
            << " flow_bridge=" << flow_bridge_result.bridge_name
            << " exec_bridge=" << exec_bridge_result.bridge_name << "\n";
  return true;
}
}

int main()
{
  if (!RunMatchSuccessCase())
    return 1;
  if (!RunMatchFailureCase())
    return 1;

  std::cout << "[PASS] parser_ir_radare_matcher_smoke\n";
  return 0;
}
