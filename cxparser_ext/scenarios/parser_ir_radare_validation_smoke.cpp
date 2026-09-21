#include <iostream>
#include <string>

#include "../adapters/radare/radare_types.h"
#include "../meta/parser_evidence.h"
#include "../meta/parser_validation_types.h"
#include "../pipeline/parser_cxscript_runtime.h"
#include "../pipeline/parser_ir_radare_matcher.h"
#include "../pipeline/parser_task_types.h"
#include "../validation/parser_validation_engine.h"

namespace
{
bool BuildMatchBundle(const char *script,
                      const cxparser_ext::RadareAnalysisResult &analysis,
                      cxparser_ext::ParserEvidenceBundle &bundle,
                      cxparser_ext::ExecutionResult &execution_result)
{
  cxparser_ext::ParserCxScriptRuntime runtime;
  cxparser_ext::CxScriptExecutionContext context;
  cxparser_ext::CxScriptFlow flow;
  cxparser_ext::CxScriptParseError parse_error;
  cxparser_ext::CxScriptExecutionResult script_result;
  std::string error_message;

  if (!runtime.ParseScriptFlow("radare_validation.cxs", script, context, flow, parse_error, error_message))
    return false;

  if (!runtime.ExecuteScriptText("radare_validation.cxs", script, script_result))
    return false;

  cxparser_ext::ParserIrRadareMatcher matcher;
  cxparser_ext::ParserIrRadareMatchResult match_result;
  if (!matcher.MatchExecution(script_result, analysis, match_result))
    return false;

  bundle = cxparser_ext::ParserEvidenceBundle();
  bundle.task_id = script_result.task_id;
  bundle.trace_id = context.trace_id;
  if (!matcher.BuildEvidence(match_result, bundle))
    return false;

  execution_result = cxparser_ext::ExecutionResult();
  execution_result.success = script_result.success;
  execution_result.scalar_result = script_result.scalar_result;
  execution_result.error_message = script_result.error_message;
  return true;
}

bool RunMatchedValidationCase()
{
  const char *script =
    "kind=integration\n"
    "layer=infer\n"
    "module=torch\n"
    "mode=run\n"
    "step infer {\n"
    "breakpoint infer_bp;\n"
    "call infer_min(frame);\n"
    "}\n";

  cxparser_ext::RadareAnalysisResult analysis;
  analysis.success = true;
  analysis.current_symbol = "TorchYoloInfer";
  cxparser_ext::RadareFunctionInfo fn;
  fn.name = "TorchYoloInfer";
  fn.address = "0x401000";
  fn.call_targets.push_back("infer_min");
  analysis.functions.push_back(fn);

  cxparser_ext::ParserEvidenceBundle bundle;
  cxparser_ext::ExecutionResult execution_result;
  if (!BuildMatchBundle(script, analysis, bundle, execution_result))
  {
    std::cerr << "[FAIL] matched validation setup failed\n";
    return false;
  }

  cxparser_ext::ParserValidationEngine engine;
  cxparser_ext::ParserValidationReport report;
  if (!engine.CompareExecutionAndEvidence(execution_result, bundle, report))
  {
    std::cerr << "[FAIL] matched validation compare failed\n";
    return false;
  }

  if (!report.passed)
  {
    std::cerr << "[FAIL] matched validation should pass\n";
    return false;
  }

  bool found_match_issue = false;
  for (size_t i = 0; i < report.issues.size(); ++i)
  {
    if (report.issues[i].code == "radare_call_matched")
      found_match_issue = true;
  }

  if (!found_match_issue)
  {
    std::cerr << "[FAIL] matched validation missing radare_call_matched issue\n";
    return false;
  }

  std::cout << "[PASS] radare validation matched issues=" << report.issues.size()
            << " matched_points=" << report.matched_points.size() << "\n";
  return true;
}

bool RunUnresolvedValidationCase()
{
  const char *script =
    "kind=module\n"
    "layer=train\n"
    "module=mlpack\n"
    "mode=run\n"
    "step train {\n"
    "checkpoint after_train;\n"
    "call train_min(config);\n"
    "}\n";

  cxparser_ext::RadareAnalysisResult analysis;
  analysis.success = true;
  analysis.current_symbol = "MlpackTrain";
  cxparser_ext::RadareFunctionInfo fn;
  fn.name = "MlpackTrain";
  fn.address = "0x402000";
  fn.call_targets.push_back("fit_epoch");
  analysis.functions.push_back(fn);

  cxparser_ext::ParserEvidenceBundle bundle;
  cxparser_ext::ExecutionResult execution_result;
  if (!BuildMatchBundle(script, analysis, bundle, execution_result))
  {
    std::cerr << "[FAIL] unresolved validation setup failed\n";
    return false;
  }

  cxparser_ext::ParserValidationEngine engine;
  cxparser_ext::ParserValidationReport report;
  if (!engine.CompareExecutionAndEvidence(execution_result, bundle, report))
  {
    std::cerr << "[FAIL] unresolved validation compare failed\n";
    return false;
  }

  bool found_unresolved = false;
  bool has_error = false;
  for (size_t i = 0; i < report.issues.size(); ++i)
  {
    if (report.issues[i].code == "radare_call_unresolved")
      found_unresolved = true;
    if (report.issues[i].level == cxparser_ext::vl_error)
      has_error = true;
  }

  if (!found_unresolved)
  {
    std::cerr << "[FAIL] unresolved validation missing radare_call_unresolved issue\n";
    return false;
  }

  if (has_error)
  {
    std::cerr << "[FAIL] unresolved validation should remain warning-level\n";
    return false;
  }

  if (!report.passed)
  {
    std::cerr << "[FAIL] unresolved validation should still pass in first-stage bridge\n";
    return false;
  }

  std::cout << "[PASS] radare validation unresolved issues=" << report.issues.size()
            << " warnings_only=true\n";
  return true;
}
}

int main()
{
  if (!RunMatchedValidationCase())
    return 1;
  if (!RunUnresolvedValidationCase())
    return 1;

  std::cout << "[PASS] parser_ir_radare_validation_smoke\n";
  return 0;
}
