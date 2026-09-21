#include <iostream>
#include <string>

#include "../runtime/cxscript_runtime.h"

namespace
{
struct ContractScriptCase
{
  const char *layer;
  const char *case_id;
  const char *must_contain_check;
  const char *must_contain_value;
};

bool ContainsText(const std::string &text, const char *pattern)
{
  return pattern != 0 && text.find(pattern) != std::string::npos;
}

bool RunCase(const ContractScriptCase &script_case)
{
  cxparser_ext::CxscriptExecutionArgs args;
  args.script_type = "integration";
  args.integration_name = "torch_geometry";
  args.layer = script_case.layer;
  args.case_id = script_case.case_id;

  cxparser_ext::CxscriptIdentity identity;
  if (!cxparser_ext::BuildCxscriptIdentity(args, identity))
  {
    std::cerr << "[FAIL] identity build failed for torch_geometry."
              << script_case.layer << "." << script_case.case_id << "\n";
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
    std::cerr << "[FAIL] contract script origin should be file: "
              << identity.file_path << "\n";
    return false;
  }

  cxparser_ext::CxscriptExecutionResult result;
  cxparser_ext::BuildCxscriptStructureSummary(script_text, result);

  if (!result.ir_valid)
  {
    std::cerr << "[FAIL] contract summary invalid: " << identity.file_path
              << " error=" << result.ir_error_message << "\n";
    return false;
  }

  if (result.flow_profile.script_style != "call_style" ||
      !result.layer_profile.has_compile_bridge ||
      !result.layer_profile.bridge_exec_safe ||
      result.layer_profile.bridge_exec_subset != "numeric_stmt" ||
      !result.basic_semantics.has_assignment ||
      !result.basic_semantics.has_call_stmt ||
      result.binding_semantics.binding_scope != "native_only")
  {
    std::cerr << "[FAIL] contract bridge classification mismatch: "
              << identity.file_path << "\n";
    return false;
  }

  if (!ContainsText(script_text, script_case.must_contain_check) ||
      !ContainsText(script_text, script_case.must_contain_value))
  {
    std::cerr << "[FAIL] contract check marker missing: "
              << identity.file_path << "\n";
    return false;
  }

  std::cout << "[PASS] file=" << identity.file_path
            << " style=" << result.flow_profile.script_style
            << " safe=" << result.layer_profile.bridge_exec_safe
            << " subset=" << result.layer_profile.bridge_exec_subset
            << " stmt=" << result.ir_stmt_count
            << " check=" << script_case.must_contain_check
            << "\n";
  return true;
}
}

int main(int argc, char **argv)
{
  const ContractScriptCase cases[] = {
    {"feature", "input_prior_contract",
     "input_contract_ok=geom_check_input_ready(",
     "input_contract_ok;"},
    {"feature", "label_align_contract",
     "label_contract_ok=geom_check_label_ready(",
     "label_contract_ok;"},
    {"feature", "attach_back_contract",
     "attach_contract_ok=geom_check_attach_ready(",
     "attach_contract_ok;"},
    {"infer", "replay_contract",
     "replay_contract_ok=geom_check_replay_ready(",
     "replay_contract_ok;"}
  };

  const char *requested_case_id = argc > 1 ? argv[1] : 0;
  bool matched_requested_case = requested_case_id == 0;

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
  {
    if (requested_case_id != 0 &&
        std::string(cases[i].case_id) != requested_case_id)
    {
      continue;
    }

    matched_requested_case = true;
    if (!RunCase(cases[i]))
      return 1;
  }

  if (!matched_requested_case)
  {
    std::cerr << "[FAIL] unknown torch_geometry contract case: "
              << requested_case_id << "\n";
    return 1;
  }

  std::cout << "[PASS] parser_torch_geometry_contract_cxscript_smoke\n";
  return 0;
}
