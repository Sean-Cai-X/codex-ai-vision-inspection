#include <iostream>

#include "../pipeline/parser_runtime_facade.h"
#include "../validation/parser_validation_engine.h"

namespace
{
int RunBadScriptCase()
{
  cxparser_ext::ExecutionTarget target;
  target.task_id = "runtime_error.bad_script";
  target.task_name = "runtime_error.bad_script";
  target.trace_id = "trace.runtime_error.bad_script";
  target.script_text = "3+";

  cxparser_ext::ParserRuntimeFacade runtime;
  if (!runtime.LoadScript(target))
  {
    std::cerr << "[FAIL] runtime refused bad-script target\n";
    return 1;
  }

  cxparser_ext::ExecutionResult result;
  if (runtime.Execute(result))
  {
    std::cerr << "[FAIL] bad script should not execute successfully\n";
    return 1;
  }

  if (result.error_kind != "parser_exception")
  {
    std::cerr << "[FAIL] bad script should surface parser_exception\n";
    return 1;
  }

  if (result.error_message.empty() || result.parser_error_code < 0)
  {
    std::cerr << "[FAIL] parser exception should carry message and code\n";
    return 1;
  }

  if (result.parser_error_expr.empty())
  {
    std::cerr << "[FAIL] parser exception should carry source expression\n";
    return 1;
  }

  cxparser_ext::ParserValidationEngine validation;
  cxparser_ext::ParserValidationReport report;
  cxparser_ext::ParserEvidenceBundle evidence;
  if (!validation.CompareExecutionAndEvidence(result, evidence, report))
  {
    std::cerr << "[FAIL] validation compare should complete\n";
    return 1;
  }

  bool saw_parser_issue = false;
  for (size_t i = 0; i < report.issues.size(); ++i)
  {
    if (report.issues[i].code == "parser_execution_failed")
    {
      saw_parser_issue = true;
      break;
    }
  }

  if (!saw_parser_issue)
  {
    std::cerr << "[FAIL] validation should classify parser_execution_failed\n";
    return 1;
  }

  return 0;
}

int RunCxcoreContractCompileBridgeCase()
{
  cxparser_ext::ExecutionTarget target;
  target.task_id = "runtime_error.cxcore_contract_bridge";
  target.task_name = target.task_id;
  target.trace_id = "trace.runtime_error.cxcore_contract_bridge";
  target.target_class = "cxcore_contract_script";
  target.target_method = "execute";
  target.script_text =
    "double success=1;"
    "double runtime_ms=0;"
    "double contract_task_id_ok=1;"
    "double contract_result_object_ok=1;"
    "double contract_failure_mode_ok=1;"
    "double contract_summary_ok=1;"
    "double source_image=1;"
    "double roi_main=1;"
    "double region_result=1;"
    "analyze_region_boundary(source_image, roi_main);"
    "check(success == true);"
    "check(contract_task_id_ok);"
    "check(contract_result_object_ok);"
    "check(runtime_ms >= 0.000000);"
    "check(contract_failure_mode_ok);"
    "check(contract_summary_ok);"
    "print(contract_task_id_ok);"
    "print(contract_summary_ok);";

  cxparser_ext::ParserRuntimeFacade runtime;
  if (!runtime.LoadScript(target))
  {
    std::cerr << "[FAIL] runtime refused cxcore contract bridge target\n";
    return 1;
  }

  cxparser_ext::ExecutionResult result;
  if (!runtime.Execute(result) || !result.success)
  {
    std::cerr << "[FAIL] cxcore contract compile bridge should execute successfully\n";
    return 1;
  }

  return 0;
}
}

int main()
{
  if (RunBadScriptCase() != 0)
    return 1;
  if (RunCxcoreContractCompileBridgeCase() != 0)
    return 1;

  std::cout << "[PASS] parser runtime error smoke preserved parser exception details\n";
  return 0;
}
