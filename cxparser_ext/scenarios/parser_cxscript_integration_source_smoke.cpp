#include <iostream>
#include <string>

#include "../runtime/cxscript_runtime.h"

namespace
{
struct SourceScriptCase
{
  const char *integration_name;
  const char *layer;
  const char *case_id;
  const char *must_contain_a;
  const char *must_contain_b;
};

bool ContainsText(const std::string &text,
                  const char *pattern)
{
  return pattern != 0 && text.find(pattern) != std::string::npos;
}

bool RunCase(const SourceScriptCase &script_case)
{
  cxparser_ext::CxscriptExecutionArgs args;
  args.script_type = "integration";
  args.integration_name = script_case.integration_name;
  args.layer = script_case.layer;
  args.case_id = script_case.case_id;

  cxparser_ext::CxscriptIdentity identity;
  if (!cxparser_ext::BuildCxscriptIdentity(args, identity))
  {
    std::cerr << "[FAIL] identity build failed for " << script_case.integration_name
              << "." << script_case.case_id << "\n";
    return false;
  }

  std::string script_text;
  std::string script_origin;
  if (!cxparser_ext::LoadCxscriptText(identity, "", script_text, script_origin))
  {
    std::cerr << "[FAIL] script load failed: " << identity.file_path << "\n";
    return false;
  }

  if (script_origin != "file")
  {
    std::cerr << "[FAIL] script origin should be file: " << identity.file_path << "\n";
    return false;
  }

  cxparser_ext::CxscriptExecutionResult result;
  cxparser_ext::BuildCxscriptStructureSummary(script_text, result);

  if (!result.ir_valid)
  {
    std::cerr << "[FAIL] linear IR invalid: " << identity.file_path
              << " error=" << result.ir_error_message << "\n";
    return false;
  }

  if (result.flow_profile.script_style != "flow_style" ||
      !result.flow_profile.has_prepare ||
      !result.flow_profile.has_action ||
      !result.flow_profile.has_check ||
      !result.flow_profile.has_report)
  {
    std::cerr << "[FAIL] flow stages incomplete: " << identity.file_path << "\n";
    return false;
  }

  if (!ContainsText(script_text, "chain_status=") ||
      !ContainsText(script_text, "artifact_ref=") ||
      !ContainsText(script_text, "report_text="))
  {
    std::cerr << "[FAIL] report fields incomplete: " << identity.file_path << "\n";
    return false;
  }

  if (!ContainsText(script_text, script_case.must_contain_a) ||
      !ContainsText(script_text, script_case.must_contain_b))
  {
    std::cerr << "[FAIL] chain markers missing: " << identity.file_path << "\n";
    return false;
  }

  std::cout << "[PASS] file=" << identity.file_path
            << " ops=" << result.ir_op_count
            << " stmts=" << result.ir_stmt_count
            << " style=" << result.flow_profile.script_style
            << " prepare=" << result.flow_profile.has_prepare
            << " action=" << result.flow_profile.has_action
            << " check=" << result.flow_profile.has_check
            << " report=" << result.flow_profile.has_report
            << "\n";
  return true;
}
}

int main()
{
  const SourceScriptCase cases[] = {
    {"parser_core", "smoke", "classical_output_min",
     "parser.dispatch(", "core.build_classical_output("},
    {"core_mlpack", "feature", "route_request_min",
     "mlpack.build_request(", "request_ref="},
    {"core_torch", "feature", "route_request_min",
     "torch_route.run_preprocess(", "request_ref="},
    {"result_management", "scenario", "stage_publish_min",
     "replay_key=", "packet_ref="},
    {"parser_core_torch_rag", "infer", "replay_stage_min",
     "torch_route.run_infer(", "gate.prepare_replay("}
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
  {
    if (!RunCase(cases[i]))
      return 1;
  }

  std::cout << "[PASS] parser_cxscript_integration_source_smoke\n";
  return 0;
}
