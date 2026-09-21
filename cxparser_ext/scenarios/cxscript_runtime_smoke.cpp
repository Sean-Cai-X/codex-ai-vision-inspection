#include <cstdio>
#include <iostream>
#include <fstream>

#include "../runtime/cxscript_runtime.h"

namespace
{
bool RunScriptLoadCase()
{
  const std::string temp_path = "cxscript_runtime_smoke_temp.cxsc";
  {
    std::ofstream output(temp_path.c_str());
    output << "41+1 // line comment";
  }

  cxparser_ext::CxscriptExecutionArgs args;
  args.script_type = "module";
  args.module_name = "rag";
  args.layer = "smoke";
  args.case_id = "parser_entry";
  args.script_path = temp_path;

  cxparser_ext::CxscriptIdentity identity;
  if (!cxparser_ext::BuildCxscriptIdentity(args, identity))
  {
    std::remove(temp_path.c_str());
    std::cerr << "[FAIL] script load identity build failed\n";
    return false;
  }

  std::string script_text;
  std::string script_origin;
  const bool ok = cxparser_ext::LoadCxscriptText(identity, "40+2", script_text, script_origin);
  std::remove(temp_path.c_str());
  if (!ok || script_text.find("41+1") == std::string::npos || script_text.find("line comment") != std::string::npos || script_origin != "file")
  {
    std::cerr << "[FAIL] script load file path did not take priority\n";
    return false;
  }

  return true;
}

bool RunCommentNormalizationCase()
{
  const std::string source_text =
    "Task.prepare_input(\"http://image\"); // prepare\n"
    "prepare_ok=1; /* block comment */\n"
    "report_text=\"keep // and /* inside string */\";\n"
    "action_ok=1;";

  std::string normalized_text;
  cxparser_ext::NormalizeCxscriptText(source_text, normalized_text);

  if (normalized_text.find("prepare_ok=1;") == std::string::npos ||
      normalized_text.find("action_ok=1;") == std::string::npos)
  {
    std::cerr << "[FAIL] comment normalization removed executable statements\n";
    return false;
  }

  if (normalized_text.find("prepare\n") != std::string::npos ||
      normalized_text.find("block comment") != std::string::npos)
  {
    std::cerr << "[FAIL] comment normalization left comment payload in script\n";
    return false;
  }

  if (normalized_text.find("\"http://image\"") == std::string::npos ||
      normalized_text.find("\"keep // and /* inside string */\"") == std::string::npos)
  {
    std::cerr << "[FAIL] comment normalization damaged string literals\n";
    return false;
  }

  cxparser_ext::CxscriptFlowProfile flow_profile;
  cxparser_ext::AnalyzeCxscriptFlow(normalized_text, flow_profile);
  if (flow_profile.script_style != "flow_style" ||
      !flow_profile.has_prepare ||
      !flow_profile.has_action ||
      !flow_profile.has_report)
  {
    std::cerr << "[FAIL] flow analysis should survive comment normalization\n";
    return false;
  }

  cxparser_ext::CxscriptExecutionResult result;
  cxparser_ext::BuildCxscriptStructureSummary(normalized_text, result);
  if (!result.ir_valid || result.ir_stmt_count < 3 || result.compile_text.empty())
  {
    std::cerr << "[FAIL] structure summary should produce IR for normalized script\n";
    return false;
  }

  return true;
}

bool RunMetadataHeaderNormalizationCase()
{
  const std::string source_text =
    "kind=module\n"
    "layer=feature\n"
    "module=cxcore\n"
    "case=region_boundary_analysis_golden\n"
    "mode=build-run\n"
    "report=on\n"
    "\n"
    "OutputRect bounds;\n"
    "check(bounds.area()>0);\n";

  std::string normalized_text;
  cxparser_ext::NormalizeCxscriptText(source_text, normalized_text);

  if (normalized_text.find("kind(\"module\");") == std::string::npos ||
      normalized_text.find("layer(\"feature\");") == std::string::npos ||
      normalized_text.find("module(\"cxcore\");") == std::string::npos ||
      normalized_text.find("case_name(\"region_boundary_analysis_golden\");") == std::string::npos ||
      normalized_text.find("mode(\"build-run\");") == std::string::npos ||
      normalized_text.find("report(true);") == std::string::npos)
  {
    std::cerr << "[FAIL] metadata header should normalize to function-style syntax\n";
    return false;
  }

  cxparser_ext::CxscriptExecutionResult result;
  cxparser_ext::BuildCxscriptStructureSummary(normalized_text, result);
  cxparser_ext::CxscriptHeaderMetadata metadata;
  if (!result.ir_valid ||
      result.ir_stmt_count != 2 ||
      !result.basic_semantics.has_declaration ||
      !result.basic_semantics.has_check_call ||
      !result.basic_semantics.has_call_stmt ||
      result.header_metadata.kind != "module" ||
      result.header_metadata.layer != "feature" ||
      result.header_metadata.module_name != "cxcore" ||
      result.header_metadata.case_name != "region_boundary_analysis_golden" ||
      result.header_metadata.mode != "build-run" ||
      !result.header_metadata.has_report ||
      !result.report_requested ||
      !cxparser_ext::ExtractCxscriptHeaderMetadata(normalized_text, metadata) ||
      metadata.kind != "module" ||
      metadata.layer != "feature" ||
      metadata.module_name != "cxcore" ||
      metadata.case_name != "region_boundary_analysis_golden" ||
      metadata.mode != "build-run" ||
      !metadata.has_report ||
      !metadata.report_on)
  {
    std::cerr << "[FAIL] metadata header should stay outside script-body semantics"
              << " ir_valid=" << (result.ir_valid ? "true" : "false")
              << " stmt_count=" << result.ir_stmt_count
              << " decl=" << (result.basic_semantics.has_declaration ? "true" : "false")
              << " check=" << (result.basic_semantics.has_check_call ? "true" : "false")
              << " call=" << (result.basic_semantics.has_call_stmt ? "true" : "false")
              << " kind=" << metadata.kind
              << " layer=" << metadata.layer
              << " module=" << metadata.module_name
              << " case=" << metadata.case_name
              << " mode=" << metadata.mode
              << " report=" << (metadata.report_on ? "true" : "false")
              << "\n";
    return false;
  }

  return true;
}

bool RunModuleCase()
{
  cxparser_ext::CxscriptExecutionArgs args;
  args.script_type = "module";
  args.module_name = "cxcore";
  args.layer = "smoke";
  args.case_id = "minimal_host";
  args.mode = "build-run";
  args.route = "default";

  cxparser_ext::CxscriptIdentity identity;
  if (!cxparser_ext::BuildCxscriptIdentity(args, identity))
  {
    std::cerr << "[FAIL] module identity build failed\n";
    return false;
  }

  cxparser_ext::CxscriptExecutionContext context;
  cxparser_ext::BuildCxscriptContext(args, context);

  cxparser_ext::CxscriptExecutionResult result;
  result.success = true;
  result.status = "run_ok";
  result.identity = identity;
  result.context = context;
  result.script_origin = "catalog";
  cxparser_ext::BuildCxscriptStructureSummary("ImageProbe probe;probe.Load(\"image.png\");probe.Detect(0.8);probe.Score();",
                                              result);
  result.modules.push_back("cxcore");
  result.accepted_task_count = 1;
  result.executed_task_count = 1;

  std::vector<std::string> lines;
  cxparser_ext::FormatCxscriptResult(result, lines);
  if (lines.size() != 11 || result.flow_profile.script_style != "call_style" || !result.ir_valid ||
      !result.layer_profile.has_source_text || !result.layer_profile.has_linear_ir ||
      !result.layer_profile.bridge_exec_safe ||
      result.layer_profile.bridge_exec_subset != "object_flow" ||
      !result.basic_semantics.has_declaration ||
      !result.basic_semantics.has_call_stmt ||
      !result.binding_semantics.requires_registered_binding ||
      !result.binding_semantics.uses_object_binding ||
      result.binding_semantics.binding_scope != "object_binding" ||
      result.layer_profile.bridge_exec_reason != "object_flow_subset")
  {
    std::cerr << "[FAIL] module semantic/binding summary mismatch\n";
    return false;
  }

  if (identity.file_path != "cxscript/module/cxcore/smoke.minimal_host.cxsc")
  {
    std::cerr << "[FAIL] module script path mismatch: " << identity.file_path << "\n";
    return false;
  }

  return true;
}

bool RunIntegrationCase()
{
  cxparser_ext::CxscriptExecutionArgs args;
  args.script_type = "integration";
  args.integration_name = "rag_torch";
  args.layer = "scenario";
  args.case_id = "test_host_replay";
  args.mode = "replay";
  args.route = "replay";

  cxparser_ext::CxscriptIdentity identity;
  if (!cxparser_ext::BuildCxscriptIdentity(args, identity))
  {
    std::cerr << "[FAIL] integration identity build failed\n";
    return false;
  }

  cxparser_ext::CxscriptExecutionContext context;
  cxparser_ext::BuildCxscriptContext(args, context);

  cxparser_ext::CxscriptExecutionResult result;
  result.success = true;
  result.status = "run_with_replay";
  result.identity = identity;
  result.context = context;
  result.script_origin = "catalog";
  cxparser_ext::BuildCxscriptStructureSummary(
    "Task.prepare_input(\"image.png\");prepare_ok=1;Task.run_detect();action_ok=1;check_ok=Task.score()>0.7;report_text=Task.summary();",
    result);
  result.modules.push_back("rag");
  result.modules.push_back("torch");
  result.accepted_task_count = 1;
  result.executed_task_count = 1;
  result.replay_count = 1;
  result.replay_source_task_id = "rag_torch.scenario.test_host_replay";
  result.replay_stage_count = 4;

  std::vector<std::string> lines;
  cxparser_ext::FormatCxscriptResult(result, lines);
  if (lines.size() != 11 || result.flow_profile.script_style != "flow_style" || !result.ir_valid ||
      !result.layer_profile.has_normalized_text || !result.layer_profile.has_compile_bridge ||
      result.layer_profile.bridge_exec_safe ||
      !result.basic_semantics.has_assignment ||
      !result.basic_semantics.has_call_stmt ||
      result.binding_semantics.binding_scope != "object_binding")
  {
    std::cerr << "[FAIL] integration semantic/binding summary mismatch\n";
    return false;
  }

  if (identity.file_path != "cxscript/integration/rag_torch/scenario.test_host_replay.cxsc")
  {
    std::cerr << "[FAIL] integration script path mismatch: " << identity.file_path << "\n";
    return false;
  }

  return true;
}

bool RunEnsmallenModuleCase()
{
  cxparser_ext::CxscriptExecutionArgs args;
  args.script_type = "module";
  args.module_name = "ensmallen_layer";
  args.layer = "feature";
  args.case_id = "circle_param_opt";
  args.mode = "build-run";
  args.route = "default";

  cxparser_ext::CxscriptIdentity identity;
  if (!cxparser_ext::BuildCxscriptIdentity(args, identity))
  {
    std::cerr << "[FAIL] ensmallen module identity build failed\n";
    return false;
  }

  cxparser_ext::CxscriptExecutionContext context;
  cxparser_ext::BuildCxscriptContext(args, context);

  cxparser_ext::CxscriptExecutionResult result;
  result.success = true;
  result.status = "run_ok";
  result.identity = identity;
  result.context = context;
  result.script_origin = "file";
  cxparser_ext::BuildCxscriptStructureSummary(
    "prepare_ok=1;circle.run_opt();action_ok=1;score=circle.score();check_ok=score<1.0;report_text=circle.summary();",
    result);
  result.modules.push_back("ensmallen_layer");
  result.accepted_task_count = 1;
  result.executed_task_count = 1;

  result.baseline_snapshot.defined = true;
  result.baseline_snapshot.objective = 0.42;
  result.baseline_snapshot.primary_metric_name = "circle_avg_dist";
  result.baseline_snapshot.primary_metric_value = 1.8;
  result.baseline_snapshot.stability_metric_name = "radius_std";
  result.baseline_snapshot.stability_metric_value = 0.25;

  result.best_snapshot.defined = true;
  result.best_snapshot.objective = 0.21;
  result.best_snapshot.primary_metric_name = "circle_avg_dist";
  result.best_snapshot.primary_metric_value = 0.9;
  result.best_snapshot.stability_metric_name = "radius_std";
  result.best_snapshot.stability_metric_value = 0.10;

  result.optimization_compare.defined = true;
  result.optimization_compare.objective_delta_abs = -0.21;
  result.optimization_compare.objective_delta_ratio = -0.50;
  result.optimization_compare.primary_metric_delta = -0.90;
  result.optimization_compare.stability_delta = -0.15;
  result.optimization_compare.eval_count = 41;
  result.optimization_compare.converged = true;
  result.optimization_compare.stop_reason = "patience";

  std::vector<std::string> lines;
  cxparser_ext::FormatCxscriptResult(result, lines);
  if (lines.size() != 14 || !result.flow_profile.has_prepare || !result.flow_profile.has_report || !result.ir_valid ||
      !result.layer_profile.has_compile_bridge || result.layer_profile.bridge_exec_safe ||
      !result.basic_semantics.has_assignment ||
      !result.basic_semantics.has_call_stmt ||
      result.binding_semantics.binding_scope != "object_binding")
  {
    std::cerr << "[FAIL] ensmallen semantic/binding summary mismatch\n";
    return false;
  }

  if (identity.file_path != "cxscript/module/ensmallen_layer/feature.circle_param_opt.cxsc")
  {
    std::cerr << "[FAIL] ensmallen script path mismatch: " << identity.file_path << "\n";
    return false;
  }

  return true;
}

bool RunIfBridgeClassificationCase()
{
  cxparser_ext::CxscriptExecutionResult result;
  cxparser_ext::BuildCxscriptStructureSummary(
    "scenario { double a=1; double d=0; if (a>0) { d=d+1; } check { d=d; } report { d; } }",
    result);

  if (!result.ir_valid ||
      !result.layer_profile.bridge_exec_safe ||
      !result.basic_semantics.has_if_block ||
      result.binding_semantics.binding_scope != "native_only" ||
      result.layer_profile.bridge_exec_subset != "numeric_if" ||
      result.layer_profile.bridge_exec_reason != "numeric_if_subset")
  {
    std::cerr << "[FAIL] if bridge classification mismatch\n";
    return false;
  }

  return true;
}

bool RunObjectFlowBridgeClassificationCase()
{
  cxparser_ext::CxscriptExecutionResult result;
  cxparser_ext::BuildCxscriptStructureSummary(
    "scenario { ImageProbe probe; probe.Load(\"image.png\"); probe.Detect(0.8); check { probe.Score(); } report { probe.Score(); } }",
    result);

  if (!result.ir_valid ||
      !result.layer_profile.bridge_exec_safe ||
      !result.basic_semantics.has_declaration ||
      !result.binding_semantics.requires_registered_binding ||
      result.binding_semantics.binding_scope != "object_binding" ||
      result.layer_profile.bridge_exec_subset != "object_flow" ||
      result.layer_profile.bridge_exec_reason != "object_flow_subset")
  {
    std::cerr << "[FAIL] object flow bridge classification mismatch\n";
    return false;
  }

  return true;
}

bool RunCxcoreContractCompileBridgeCase()
{
  cxparser_ext::CxscriptExecutionArgs args;
  args.script_type = "module";
  args.module_name = "cxcore";
  args.layer = "feature";
  args.case_id = "region_boundary_analysis_golden";

  cxparser_ext::CxscriptIdentity identity;
  if (!cxparser_ext::BuildCxscriptIdentity(args, identity))
  {
    std::cerr << "[FAIL] cxcore contract identity build failed\n";
    return false;
  }

  identity.file_path =
    "cxparser/rag_script_cases/cxcore/feature/cxcore_region_boundary_analysis_golden_cstyle_feature.cxsc";

  std::string script_text;
  std::string script_origin;
  if (!cxparser_ext::LoadCxscriptText(identity, "", script_text, script_origin))
  {
    std::cerr << "[FAIL] cxcore contract script load failed\n";
    return false;
  }

  cxparser_ext::CxscriptExecutionResult summary;
  cxparser_ext::BuildCxscriptStructureSummary(script_text, summary);
  if (!summary.ir_valid ||
      !summary.layer_profile.bridge_exec_safe ||
      summary.layer_profile.bridge_exec_subset != "cxcore_contract_call" ||
      summary.compile_text.find("use Image;") != std::string::npos ||
      summary.compile_text.find("double source_image=1;") == std::string::npos ||
      summary.compile_text.find("double roi_main=1;") == std::string::npos ||
      summary.compile_text.find("analyze_region_boundary(") == std::string::npos ||
      summary.compile_text.find("check(contract_task_id_ok);") == std::string::npos ||
      summary.compile_text.find("contract_result_object_ok") == std::string::npos ||
      summary.compile_text.find("contract_failure_mode_ok") == std::string::npos ||
      summary.compile_text.find("contract_summary_ok") == std::string::npos)
  {
    std::cerr << "[FAIL] cxcore contract compile bridge summary mismatch"
              << " ir_valid=" << (summary.ir_valid ? "true" : "false")
              << " safe=" << (summary.layer_profile.bridge_exec_safe ? "true" : "false")
              << " subset=" << summary.layer_profile.bridge_exec_subset
              << " reason=" << summary.layer_profile.bridge_exec_reason
              << " compile_text=" << summary.compile_text << "\n";
    return false;
  }

  return true;
}

bool RunPublicDottedEnsmallenCaseNormalizationCase()
{
  cxparser_ext::CxscriptExecutionArgs args;
  args.script_type = "module";
  args.module_name = "ensmallen_layer";
  args.layer = "scenario";
  args.case_id = "cxcore.ensmallen_layer.phase1_param_replay.scenario";

  cxparser_ext::CxscriptIdentity identity;
  if (!cxparser_ext::BuildCxscriptIdentity(args, identity))
  {
    std::cerr << "[FAIL] public dotted ensmallen identity build failed\n";
    return false;
  }

  cxparser_ext::CxscriptExecutionContext context;
  cxparser_ext::BuildCxscriptContext(args, context);

  if (identity.case_id != "phase1_param_replay" ||
      identity.layer != "scenario" ||
      identity.module_name != "ensmallen_layer" ||
      identity.file_path.find("phase1_param_replay") == std::string::npos ||
      context.trace_id.find(".scenario.phase1_param_replay") == std::string::npos)
  {
    std::cerr << "[FAIL] public dotted ensmallen normalization mismatch"
              << " case_id=" << identity.case_id
              << " file_path=" << identity.file_path
              << " trace_id=" << context.trace_id << "\n";
    return false;
  }

  return true;
}

bool RunExplicitRegistrationSemanticCase()
{
  const std::string source_text =
    "kind(\"module\");\n"
    "layer(\"feature\");\n"
    "module(\"cxcore\");\n"
    "case_name(\"registration_probe\");\n"
    "mode(\"build-run\");\n"
    "report(true);\n"
    "\n"
    "register_class(\"ImageProbe\");\n"
    "register_fun(\"ImageProbe\", \"Score\");\n"
    "check(true);\n";

  cxparser_ext::CxscriptExecutionResult result;
  cxparser_ext::BuildCxscriptStructureSummary(source_text, result);

  std::vector<std::string> lines;
  cxparser_ext::FormatCxscriptResult(result, lines);

  if (!result.ir_valid ||
      !result.basic_semantics.has_call_stmt ||
      !result.basic_semantics.has_check_call ||
      !result.basic_semantics.has_register_class ||
      !result.basic_semantics.has_register_fun ||
      !result.binding_semantics.requires_registered_binding ||
      !result.binding_semantics.uses_explicit_registration ||
      result.binding_semantics.uses_object_binding ||
      result.binding_semantics.binding_scope != "registration")
  {
    std::cerr << "[FAIL] explicit registration semantics mismatch\n";
    return false;
  }

  bool found_semantics = false;
  bool found_binding = false;
  for (size_t i = 0; i < lines.size(); ++i)
  {
    if (lines[i].find("register_class=true") != std::string::npos &&
        lines[i].find("register_fun=true") != std::string::npos)
      found_semantics = true;
    if (lines[i].find("registration=true") != std::string::npos &&
        lines[i].find("scope=registration") != std::string::npos)
      found_binding = true;
  }

  if (!found_semantics || !found_binding)
  {
    std::cerr << "[FAIL] explicit registration formatting mismatch\n";
    return false;
  }

  return true;
}
}

int main()
{
  if (!RunScriptLoadCase() || !RunCommentNormalizationCase() || !RunMetadataHeaderNormalizationCase() || !RunModuleCase() ||
      !RunIntegrationCase() || !RunEnsmallenModuleCase() ||
      !RunIfBridgeClassificationCase() || !RunObjectFlowBridgeClassificationCase() ||
      !RunCxcoreContractCompileBridgeCase() || !RunPublicDottedEnsmallenCaseNormalizationCase() ||
      !RunExplicitRegistrationSemanticCase())
    return 1;

  std::cout << "[PASS] cxscript_runtime comments+module+integration+ensmallen identity/context/result\n";
  return 0;
}
