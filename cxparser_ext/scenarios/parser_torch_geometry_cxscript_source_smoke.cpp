#include <iostream>
#include <string>

#include "../runtime/cxscript_runtime.h"

namespace
{
struct SourceScriptCase
{
  const char *layer;
  const char *case_id;
  const char *must_contain_a;
  const char *must_contain_b;
  const char *must_contain_c;
};

bool ContainsText(const std::string &text, const char *pattern)
{
  return pattern != 0 && text.find(pattern) != std::string::npos;
}

bool RunCase(const SourceScriptCase &script_case)
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
    std::cerr << "[FAIL] script origin should be file: " << identity.file_path << "\n";
    return false;
  }

  cxparser_ext::CxscriptExecutionResult result;
  cxparser_ext::BuildCxscriptStructureSummary(script_text, result);

  if (!result.ir_valid)
  {
    std::cerr << "[FAIL] source summary invalid: " << identity.file_path
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

  if (!ContainsText(script_text, "trace_id=") ||
      !ContainsText(script_text, "chain_status=") ||
      !ContainsText(script_text, "artifact_ref=") ||
      !ContainsText(script_text, "report_text="))
  {
    std::cerr << "[FAIL] required input/report fields missing: " << identity.file_path << "\n";
    return false;
  }

  if (!ContainsText(script_text, script_case.must_contain_a) ||
      !ContainsText(script_text, script_case.must_contain_b) ||
      !ContainsText(script_text, script_case.must_contain_c))
  {
    std::cerr << "[FAIL] required flow markers missing: " << identity.file_path << "\n";
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
    {"feature", "input_prior_min",
     "geom_export_roi(", "geom_align_input_prior(", "geom_build_torch_request("},
    {"feature", "label_align_min",
     "geom_export_mask_label(", "geom_align_training_label(", "geom_build_torch_label_packet("},
    {"feature", "attach_back_min",
     "geom_attach_result_to_roi(", "geom_attach_result_boundary(", "geom_publish_attach_packet("},
    {"infer", "replay_min",
     "torch_run(", "geom_attach_result_keypoints(", "packet_ref="}
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
  {
    if (!RunCase(cases[i]))
      return 1;
  }

  std::cout << "[PASS] parser_torch_geometry_cxscript_source_smoke\n";
  return 0;
}
