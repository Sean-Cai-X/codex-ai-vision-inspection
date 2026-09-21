#include <iostream>
#include <string>

#include "../runtime/cxscript_runtime.h"

namespace
{
#ifdef CXPARSER_WORKSPACE_ROOT
std::string ResolveWorkspaceRoot()
{
  return CXPARSER_WORKSPACE_ROOT;
}
#else
std::string ResolveWorkspaceRoot()
{
  return std::string();
}
#endif

struct SourceScriptCase
{
  const char *module_name;
  const char *layer;
  const char *case_id;
  const char *relative_path;
  const char *feature_marker;
  const char *must_contain_a;
  const char *must_contain_b;
  const char *must_contain_c;
  const char *must_contain_d;
  const char *must_contain_e;
  const char *must_contain_f;
  const char *must_contain_g;
  const char *must_contain_h;
  const char *must_contain_i;
  const char *must_contain_j;
  const char *must_contain_k;
  const char *must_contain_l;
};

bool ContainsText(const std::string &text,
                  const char *pattern)
{
  return pattern == 0 || text.find(pattern) != std::string::npos;
}

bool RunCase(const SourceScriptCase &script_case)
{
  const std::string workspace_root = ResolveWorkspaceRoot();
  if (workspace_root.empty())
  {
    std::cerr << "[FAIL] workspace root is not configured for ensmallen tuning source smoke\n";
    return false;
  }

  cxparser_ext::CxscriptExecutionArgs args;
  args.script_type = "module";
  args.module_name = script_case.module_name;
  args.layer = script_case.layer;
  args.case_id = script_case.case_id;
  args.script_path = workspace_root + "/" + script_case.relative_path;

  cxparser_ext::CxscriptIdentity identity;
  if (!cxparser_ext::BuildCxscriptIdentity(args, identity))
  {
    std::cerr << "[FAIL] identity build failed for "
              << script_case.module_name << "." << script_case.case_id << "\n";
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

  if (!ContainsText(script_text, script_case.feature_marker) ||
      !ContainsText(script_text, script_case.must_contain_a) ||
      !ContainsText(script_text, script_case.must_contain_b) ||
      !ContainsText(script_text, script_case.must_contain_c) ||
      !ContainsText(script_text, script_case.must_contain_d) ||
      !ContainsText(script_text, script_case.must_contain_e) ||
      !ContainsText(script_text, script_case.must_contain_f) ||
      !ContainsText(script_text, script_case.must_contain_g) ||
      !ContainsText(script_text, script_case.must_contain_h) ||
      !ContainsText(script_text, script_case.must_contain_i) ||
      !ContainsText(script_text, script_case.must_contain_j) ||
      !ContainsText(script_text, script_case.must_contain_k) ||
      !ContainsText(script_text, script_case.must_contain_l))
  {
    std::cerr << "[FAIL] required contract markers missing: " << identity.file_path << "\n";
    return false;
  }

  cxparser_ext::CxscriptExecutionResult result;
  cxparser_ext::BuildCxscriptStructureSummary(script_text, result);

  if (!result.ir_valid)
  {
    std::cerr << "[FAIL] contract script should build source summary: "
              << identity.file_path << " error=" << result.ir_error_message << "\n";
    return false;
  }

  if (result.flow_profile.script_style != "flow_style" ||
      !result.flow_profile.has_prepare ||
      !result.flow_profile.has_action ||
      !result.flow_profile.has_check ||
      !result.flow_profile.has_report)
  {
    std::cerr << "[FAIL] contract flow stages incomplete: " << identity.file_path
              << " style=" << result.flow_profile.script_style
              << " prepare=" << result.flow_profile.has_prepare
              << " action=" << result.flow_profile.has_action
              << " check=" << result.flow_profile.has_check
              << " report=" << result.flow_profile.has_report << "\n";
    return false;
  }

  if (!result.layer_profile.has_source_text ||
      !result.layer_profile.has_normalized_text ||
      !result.layer_profile.has_linear_ir)
  {
    std::cerr << "[FAIL] layer profile incomplete: " << identity.file_path << "\n";
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
    {"ensmallen_layer", "feature", "geometry_fit_tuning",
     "cxparser/rag_script_cases/cxcore/feature/ensmallen_layer_geometry_fit_tuning_feature.cxsc",
     "flow.set_feature(\"FormfitGeometryFitTuning\")",
     "flow.call(\"RunBaselineEval\")",
     "flow.call(\"RunGeometryFitTuning\")",
     "flow.call(\"CompareBaselineVsBest\")",
     "flow.expect_output(\"MeasuredOptimizeResult\")",
     "flow.expect_output(\"MeasuredCompareResult\")",
     "flow.expect_output(\"MeasuredReplayResult\")",
     "flow.call(\"BuildRagExplainPacket\")",
     "flow.expect_output(\"RagWritebackNote\")",
     "flow.check_trace_contains(\"ExportReplayLog\")",
     "flow.input_param(\"objective_ref\",\"torch.optimization.objective_ref\")",
     "flow.input_param(\"boundary_error_ref\",\"torch.optimization.boundary_error_ref\")",
     "flow.input_param(\"alignment_error_ref\",\"torch.optimization.alignment_error_ref\")",
     "flow.expect_field(\"replay_log_path\")"},
    {"ensmallen_layer", "feature", "match_score_tuning",
     "cxparser/rag_script_cases/cxcore/feature/ensmallen_layer_match_score_tuning_feature.cxsc",
     "flow.set_feature(\"FastMatchScoreTuning\")",
     "flow.input_dataset(\"dataset.cxcore.phase1.ensmallen\")",
     "flow.input_sample(\"template_match.template_scene\")",
     "flow.call(\"RunBaselineEval\")",
     "flow.call(\"RunMatchScoreTuning\")",
     "flow.call(\"CompareBaselineVsBest\")",
     "flow.expect_output(\"MeasuredOptimizeResult\")",
     "flow.expect_output(\"MeasuredCompareResult\")",
     "flow.expect_output(\"MeasuredReplayResult\")",
     "flow.call(\"BuildRagExplainPacket\")",
     "flow.expect_output(\"RagWritebackNote\")",
     "flow.input_artifact(\"template_image\",\"phase1.template_match.template_patch\")",
     "flow.input_param(\"objective_ref\",\"torch.optimization.objective_ref\")",
     "flow.input_param(\"threshold_ref\",\"torch.optimization.threshold_ref\")",
     "flow.input_param(\"crop_policy_ref\",\"torch.optimization.crop_policy_ref\")",
     "flow.expect_field(\"replay_log_path\")"}
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
  {
    if (!RunCase(cases[i]))
      return 1;
  }

  std::cout << "[PASS] parser_ensmallen_tuning_cxscript_source_smoke\n";
  return 0;
}
