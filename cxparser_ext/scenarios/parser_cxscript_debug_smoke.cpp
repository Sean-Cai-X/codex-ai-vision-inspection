#include <iostream>
#include <string>

#include "../pipeline/parser_cxscript_runtime.h"

namespace
{
cxparser_ext::ParserCxScriptRuntime MakeDebugRuntime()
{
  cxparser_ext::ParserCxScriptRuntime runtime;
  runtime.SetExecutionMode(cxparser_ext::cxsrm_debug);
  return runtime;
}

bool HasOpcode(const std::vector<cxparser_ext::CxScriptExecutionOp>& ops,
               cxparser_ext::CxScriptExecutionOpcode opcode)
{
  for (size_t i = 0; i < ops.size(); ++i)
  {
    if (ops[i].opcode == opcode)
      return true;
  }
  return false;
}

bool HasReplayAction(const std::vector<cxparser_ext::CxScriptReplayFrame>& frames,
                     const std::string& action)
{
  for (size_t i = 0; i < frames.size(); ++i)
  {
    if (frames[i].action == action)
      return true;
  }
  return false;
}

bool HasSourceKind(const std::vector<cxparser_ext::CxScriptSourceMapEntry>& entries,
                   const std::string& kind)
{
  for (size_t i = 0; i < entries.size(); ++i)
  {
    if (entries[i].statement_kind == kind)
      return true;
  }
  return false;
}

bool HasFunctionFragment(const std::vector<cxparser_ext::CxScriptFunctionFragment>& fragments,
                         const std::string& function_name)
{
  for (size_t i = 0; i < fragments.size(); ++i)
  {
    if (fragments[i].function_name == function_name)
      return true;
  }
  return false;
}

bool HasCStyleSnippet(const std::vector<cxparser_ext::CxScriptCStyleSnippet>& snippets,
                      const std::string& snippet_id)
{
  for (size_t i = 0; i < snippets.size(); ++i)
  {
    if (snippets[i].snippet_id == snippet_id)
      return true;
  }
  return false;
}

bool HasReadinessGroup(const cxparser_ext::CxScriptMainlineReadiness& readiness,
                       const std::string& group_name)
{
  for (size_t i = 0; i < readiness.groups.size(); ++i)
  {
    if (readiness.groups[i].group_name == group_name)
      return true;
  }
  return false;
}

bool HasExecutionStepKind(const std::vector<cxparser_ext::CxScriptExecutionStepView>& steps,
                          cxparser_ext::CxScriptExecutionStepKind kind)
{
  for (size_t i = 0; i < steps.size(); ++i)
  {
    if (steps[i].kind == kind)
      return true;
  }
  return false;
}

bool HasCompiledStage(const cxparser_ext::CxScriptExecutionResult &result,
                      const std::string &stage_name)
{
  for (size_t i = 0; i < result.compiled_stage_names.size(); ++i)
  {
    if (result.compiled_stage_names[i] == stage_name)
      return true;
  }
  return false;
}

bool HasNamedResultField(const cxparser_ext::CxScriptExecutionResult &result,
                         const std::string &result_name,
                         const std::string &field_name,
                         const std::string &value)
{
  for (size_t i = 0; i < result.result_fields.size(); ++i)
  {
    if (result.result_fields[i].result_name == result_name &&
        result.result_fields[i].field_name == field_name &&
        result.result_fields[i].value == value)
      return true;
  }
  return false;
}

std::string GetNamedResultFieldValue(const cxparser_ext::CxScriptExecutionResult &result,
                                     const std::string &result_name,
                                     const std::string &field_name)
{
  for (size_t i = 0; i < result.result_fields.size(); ++i)
  {
    if (result.result_fields[i].result_name == result_name &&
        result.result_fields[i].field_name == field_name)
      return result.result_fields[i].value;
  }
  return std::string();
}

bool RunHeaderMetadataFunctionCase()
{
  cxparser_ext::ParserCxScriptRuntime runtime = MakeDebugRuntime();
  cxparser_ext::CxScriptExecutionResult result;

  const char *script =
    "kind(module)\n"
    "layer(feature)\n"
    "module(cxcore)\n"
    "case_name(line_measurement_balanced)\n"
    "mode(build-run)\n"
    "report(on)\n"
    "step prepare {\n"
    "print(\"prepare.header\");\n"
    "}\n"
    "step check {\n"
    "check(success == true);\n"
    "}\n";

  if (!runtime.BuildExecutionPreview("header_metadata_functions.cxs", script, result))
  {
    std::cerr << "[FAIL] header metadata preview summary=" << result.summary << "\n";
    return false;
  }

  if (result.kind != "module" ||
      result.layer != "feature" ||
      result.module != "cxcore" ||
      result.case_name != "line_measurement_balanced" ||
      result.execution_summary.entry_step_id <= 0 ||
      result.execution_summary.header_step_count < 6 ||
      result.execution_summary.print_step_count < 1)
  {
    std::cerr << "[FAIL] header metadata context/summary mismatch\n";
    return false;
  }

  if (!HasSourceKind(result.debug_view.source_map, "header_metadata") ||
      !HasReplayAction(result.debug_view.replay_frames, "header_metadata") ||
      !HasExecutionStepKind(result.debug_view.execution_steps, cxparser_ext::cxsesk_header_metadata))
  {
    std::cerr << "[FAIL] header metadata execution mapping missing\n";
    return false;
  }

  cxparser_ext::CxScriptDebugQueryResult line_query;
  if (!runtime.QueryDebugByLine(result.debug_view, 1, line_query))
  {
    std::cerr << "[FAIL] header metadata line query failed\n";
    return false;
  }

  if (line_query.matched_execution_step.kind != cxparser_ext::cxsesk_header_metadata ||
      line_query.matched_execution_step.control_tag != "header_metadata" ||
      line_query.matched_execution_step.step_name != "__header__" ||
      line_query.nearest_replay_frame.action != "header_metadata" ||
      line_query.matched_source_entry.statement_kind != "header_metadata" ||
      line_query.matched_execution_op.opcode != cxparser_ext::cxseo_header_metadata)
  {
    std::cerr << "[FAIL] header metadata debug/replay view mismatch\n";
    return false;
  }

  if (line_query.matched_execution_step.kind == cxparser_ext::cxsesk_call ||
      line_query.matched_execution_step.kind == cxparser_ext::cxsesk_action ||
      line_query.matched_execution_step.kind == cxparser_ext::cxsesk_step)
  {
    std::cerr << "[FAIL] header metadata should not be treated as normal execution step\n";
    return false;
  }

  cxparser_ext::CxScriptDebugQueryResult sequence_query;
  if (!runtime.QueryDebugBySequence(result.debug_view,
                                    line_query.matched_execution_step.sequence,
                                    sequence_query))
  {
    std::cerr << "[FAIL] header metadata sequence query failed\n";
    return false;
  }

  if (sequence_query.matched_execution_step.kind != cxparser_ext::cxsesk_header_metadata ||
      sequence_query.matched_execution_step.control_tag != "header_metadata" ||
      sequence_query.nearest_replay_frame.action != "header_metadata" ||
      sequence_query.step_id != 0 ||
      sequence_query.frame_id != 0)
  {
    std::cerr << "[FAIL] header metadata sequence query mismatch\n";
    return false;
  }

  return true;
}

bool RunHeaderMetadataLegacyCase()
{
  cxparser_ext::ParserCxScriptRuntime runtime = MakeDebugRuntime();
  cxparser_ext::CxScriptExecutionResult result;

  const char *script =
    "kind=module\n"
    "layer=feature\n"
    "module=cxcore\n"
    "case=line_measurement_balanced\n"
    "mode=build-run\n"
    "report=on\n"
    "step prepare {\n"
    "print(\"prepare.legacy_header\");\n"
    "}\n"
    "step check {\n"
    "check(success == true);\n"
    "}\n";

  if (!runtime.BuildExecutionPreview("header_metadata_legacy.cxs", script, result))
  {
    std::cerr << "[FAIL] legacy header metadata preview summary=" << result.summary << "\n";
    return false;
  }

  if (result.kind != "module" ||
      result.layer != "feature" ||
      result.module != "cxcore" ||
      result.case_name != "line_measurement_balanced")
  {
    std::cerr << "[FAIL] legacy header metadata context mismatch\n";
    return false;
  }

  if (!HasSourceKind(result.debug_view.source_map, "header_metadata") ||
      !HasReplayAction(result.debug_view.replay_frames, "header_metadata") ||
      !HasExecutionStepKind(result.debug_view.execution_steps, cxparser_ext::cxsesk_header_metadata) ||
      !HasOpcode(result.debug_view.execution_ops, cxparser_ext::cxseo_header_metadata))
  {
    std::cerr << "[FAIL] legacy header metadata execution mapping missing\n";
    return false;
  }

  cxparser_ext::CxScriptDebugQueryResult line_query;
  if (!runtime.QueryDebugByLine(result.debug_view, 1, line_query))
  {
    std::cerr << "[FAIL] legacy header metadata line query failed\n";
    return false;
  }

  cxparser_ext::CxScriptDebugQueryResult sequence_query;
  if (!runtime.QueryDebugBySequence(result.debug_view,
                                    line_query.matched_execution_step.sequence,
                                    sequence_query))
  {
    std::cerr << "[FAIL] legacy header metadata sequence query failed\n";
    return false;
  }

  if (line_query.matched_execution_step.kind != cxparser_ext::cxsesk_header_metadata ||
      sequence_query.matched_execution_step.kind != cxparser_ext::cxsesk_header_metadata ||
      line_query.matched_execution_step.control_tag != "header_metadata" ||
      sequence_query.matched_execution_step.control_tag != "header_metadata" ||
      line_query.nearest_replay_frame.action != "header_metadata" ||
      sequence_query.nearest_replay_frame.action != "header_metadata")
  {
    std::cerr << "[FAIL] legacy header metadata debug/replay mismatch\n";
    return false;
  }

  return true;
}

bool RunSuccessCase()
{
  cxparser_ext::ParserCxScriptRuntime runtime = MakeDebugRuntime();
  cxparser_ext::CxScriptExecutionResult result;
  std::vector<cxparser_ext::CxScriptFunctionFragment> fragments;
  std::vector<cxparser_ext::CxScriptFlowSnippet> snippets;
  std::vector<cxparser_ext::CxScriptCStyleSnippet> cstyle_snippets;
  cxparser_ext::CxScriptMainlineReadiness readiness;
  std::vector<std::string> fragment_report;

  if (!runtime.BuildBuiltinFunctionFragments(fragments) ||
      !runtime.BuildBuiltinFlowSnippets(snippets) ||
      !runtime.BuildBuiltinCStyleSnippets(cstyle_snippets) ||
      !runtime.BuildCurrentMainlineReadiness(readiness) ||
      !runtime.BuildBuiltinFunctionFragmentReport(fragment_report))
  {
    std::cerr << "[FAIL] builtin cxscript fragments/snippets should be available\n";
    return false;
  }

  if (fragments.size() < 8 || snippets.size() < 4 || cstyle_snippets.size() < 3 ||
      fragment_report.size() < fragments.size())
  {
    std::cerr << "[FAIL] builtin fragment registry size mismatch\n";
    return false;
  }

  if (fragments.front().function_name.empty() ||
      fragments.front().category.empty() ||
      fragments.front().flow_role.empty())
  {
    std::cerr << "[FAIL] builtin fragment metadata missing\n";
    return false;
  }

  if (snippets.front().snippet_id.empty() ||
      snippets.front().function_names.empty() ||
      !snippets.front().reusable_for_cxcore)
  {
    std::cerr << "[FAIL] builtin snippet metadata missing\n";
    return false;
  }

  if (cstyle_snippets.front().snippet_id.empty() ||
      cstyle_snippets.front().action_family.empty() ||
      cstyle_snippets.front().script_text.empty())
  {
    std::cerr << "[FAIL] builtin c-style snippet metadata missing\n";
    return false;
  }

  if (!HasFunctionFragment(fragments, "build_curve") ||
      !HasFunctionFragment(fragments, "build_wire") ||
      !HasFunctionFragment(fragments, "build_face") ||
      !HasFunctionFragment(fragments, "measure_shape") ||
      !HasFunctionFragment(fragments, "measure_distance") ||
      !HasFunctionFragment(fragments, "make_visual_object"))
  {
    std::cerr << "[FAIL] occt capability fragments missing\n";
    return false;
  }

  if (!HasCStyleSnippet(cstyle_snippets, "occt_curve_wire_face_measure") ||
      !HasCStyleSnippet(cstyle_snippets, "occt_shape_distance_visualize"))
  {
    std::cerr << "[FAIL] occt c-style snippets missing\n";
    return false;
  }

  if (readiness.ready_flow_ids.empty() ||
      readiness.gap_flow_ids.empty() ||
      readiness.gap_reasons.empty() ||
      readiness.handoff_order.empty())
  {
    std::cerr << "[FAIL] mainline readiness summary missing\n";
    return false;
  }

  if (!HasReadinessGroup(readiness, "operator") ||
      !HasReadinessGroup(readiness, "matcher") ||
      !HasReadinessGroup(readiness, "feature") ||
      !HasReadinessGroup(readiness, "embedded_model") ||
      !HasReadinessGroup(readiness, "occt_fragment") ||
      !HasReadinessGroup(readiness, "cxcloud_fragment"))
  {
    std::cerr << "[FAIL] readiness groups missing\n";
    return false;
  }

  const char *script =
    "kind=module\n"
    "layer=smoke\n"
    "module=cxcore\n"
    "mode=build-run\n"
    "report=on\n"
    "type ImageFrame;\n"
    "use ImageFrame;\n"
    "step prepare {\n"
    "ImageFrame frame = frame;\n"
    "double threshold = 0.8;\n"
    "threshold = threshold + 1;\n"
    "breakpoint prepare_bp;\n"
    "call minimal_binding();\n"
    "}\n"
    "step check\n"
    "checkpoint after_prepare;\n"
    "expect success == true;\n";

  if (!runtime.ExecuteScriptText("debug_success.cxs", script, result))
  {
    std::cerr << "[FAIL] success case summary=" << result.summary << "\n";
    return false;
  }

  if (result.debug_view.step_traces.size() < 2)
  {
    std::cerr << "[FAIL] step trace count too small\n";
    return false;
  }

  if (result.debug_view.source_map.empty() ||
      result.debug_view.replay_frames.empty() ||
      result.debug_view.execution_ops.empty())
  {
    std::cerr << "[FAIL] debug view missing source/replay/execution data\n";
    return false;
  }

  if (result.debug_view.execution_summary.entry_step_id <= 0 ||
      result.debug_view.execution_summary.check_step_id <= 0 ||
      result.debug_view.execution_summary.max_sequence != static_cast<int>(result.debug_view.execution_steps.size()) ||
      result.debug_view.execution_summary.max_frame_id < 0 ||
      result.debug_view.execution_summary.max_block_depth < 1 ||
      result.debug_view.execution_summary.frame_step_count < 1 ||
      result.debug_view.execution_summary.call_step_count < 1 ||
      result.debug_view.execution_summary.breakpoint_step_count < 1 ||
      result.debug_view.execution_summary.checkpoint_step_count < 1)
  {
    std::cerr << "[FAIL] execution summary mismatch\n";
    return false;
  }

  if (result.last_step_id <= 0 ||
      result.last_sequence != result.debug_view.execution_summary.max_sequence ||
      result.last_source_line <= 0 ||
      !result.failure_phase.empty())
  {
    std::cerr << "[FAIL] execution diagnostics mismatch\n";
    return false;
  }

  if (result.debug_view.variables.size() != 2)
  {
    std::cerr << "[FAIL] variable declaration count mismatch\n";
    return false;
  }

  if (!result.debug_view.variables[0].initialized ||
      !result.debug_view.variables[1].initialized)
  {
    std::cerr << "[FAIL] variable initialization flag mismatch\n";
    return false;
  }

  cxparser_ext::CxScriptDebugQueryResult step_query;
  if (!runtime.QueryDebugByStep(result.debug_view, "prepare", step_query))
  {
    std::cerr << "[FAIL] step query failed\n";
    return false;
  }

  if (step_query.variables.size() != 2 || step_query.replay_frames.empty())
  {
    std::cerr << "[FAIL] step query missing variable/replay data\n";
    return false;
  }

  if (step_query.step_id <= 0 || step_query.frame_id < 0)
  {
    std::cerr << "[FAIL] step query missing stable step/frame ids\n";
    return false;
  }

  if (step_query.matched_execution_step.sequence <= 0 ||
      step_query.matched_execution_step.step_id != step_query.step_id ||
      step_query.matched_execution_step.kind != cxparser_ext::cxsesk_step ||
      step_query.matched_execution_step.control_tag != "step_enter" ||
      step_query.matched_source_entry.statement_kind != "step" ||
      step_query.matched_execution_op.opcode != cxparser_ext::cxseo_step_enter)
  {
    std::cerr << "[FAIL] step query missing execution-step view\n";
    return false;
  }

  if (!HasExecutionStepKind(step_query.execution_steps, cxparser_ext::cxsesk_step) ||
      !HasExecutionStepKind(step_query.execution_steps, cxparser_ext::cxsesk_var_decl) ||
      !HasExecutionStepKind(step_query.execution_steps, cxparser_ext::cxsesk_action) ||
      !HasExecutionStepKind(step_query.execution_steps, cxparser_ext::cxsesk_breakpoint) ||
      !HasExecutionStepKind(step_query.execution_steps, cxparser_ext::cxsesk_call))
  {
    std::cerr << "[FAIL] step query execution-step kind coverage mismatch\n";
    return false;
  }

  if (!HasOpcode(step_query.execution_ops, cxparser_ext::cxseo_var_decl) ||
      !HasOpcode(step_query.execution_ops, cxparser_ext::cxseo_action) ||
      !HasOpcode(step_query.execution_ops, cxparser_ext::cxseo_breakpoint) ||
      !HasOpcode(step_query.execution_ops, cxparser_ext::cxseo_call))
  {
    std::cerr << "[FAIL] step query execution opcode coverage mismatch\n";
    return false;
  }

  if (step_query.breakpoints.size() != 1 ||
      step_query.nearest_breakpoint.name != "prepare_bp")
  {
    std::cerr << "[FAIL] step query breakpoint mismatch\n";
    return false;
  }

  if (step_query.matched_step_trace.step_name != "prepare")
  {
    std::cerr << "[FAIL] step query trace mismatch\n";
    return false;
  }

  if (!HasReplayAction(step_query.replay_frames, "frame_exit") &&
      !HasReplayAction(step_query.replay_frames, "block"))
  {
    std::cerr << "[FAIL] step query nearest replay mismatch\n";
    return false;
  }

  cxparser_ext::CxScriptDebugQueryResult line_query;
  if (!runtime.QueryDebugByLine(result.debug_view, 9, line_query))
  {
    std::cerr << "[FAIL] line query failed:";
    for (size_t i = 0; i < result.debug_view.source_map.size(); ++i)
      std::cerr << " (" << result.debug_view.source_map[i].span.line_begin
                << "," << result.debug_view.source_map[i].statement_kind
                << "," << result.debug_view.source_map[i].step_name << ")";
    std::cerr << "\n";
    return false;
  }

  if (line_query.step_name != "prepare" || line_query.source_entries.empty())
  {
    std::cerr << "[FAIL] line query result mismatch\n";
    return false;
  }

  if (line_query.step_id != step_query.step_id || line_query.frame_id != step_query.frame_id)
  {
    std::cerr << "[FAIL] line query step/frame id mismatch\n";
    return false;
  }

  if (line_query.matched_execution_step.sequence <= 0 ||
      line_query.matched_execution_step.step_id != line_query.step_id ||
      line_query.matched_execution_step.kind != cxparser_ext::cxsesk_var_decl ||
      line_query.matched_execution_step.control_tag != "linear" ||
      line_query.matched_source_entry.statement_kind != "var_decl" ||
      line_query.matched_execution_op.opcode != cxparser_ext::cxseo_var_decl)
  {
    std::cerr << "[FAIL] line query missing execution-step view\n";
    return false;
  }

  if (line_query.current_block_depth != 1 ||
      line_query.matched_step_trace.step_name != "prepare")
  {
    std::cerr << "[FAIL] line query trace/depth mismatch\n";
    return false;
  }

  if (line_query.nearest_replay_frame.action != "var_decl" ||
      !line_query.nearest_checkpoint.name.empty())
  {
    std::cerr << "[FAIL] line query nearest replay/checkpoint mismatch\n";
    return false;
  }

  cxparser_ext::CxScriptDebugQueryResult action_line_query;
  if (!runtime.QueryDebugByLine(result.debug_view, 11, action_line_query))
  {
    std::cerr << "[FAIL] action line query failed\n";
    return false;
  }

  if (action_line_query.step_name != "prepare" ||
      action_line_query.nearest_replay_frame.action != "action" ||
      action_line_query.matched_execution_step.kind != cxparser_ext::cxsesk_action ||
      action_line_query.matched_execution_step.control_tag != "linear" ||
      action_line_query.matched_source_entry.statement_kind != "action" ||
      action_line_query.matched_execution_op.opcode != cxparser_ext::cxseo_action)
  {
    std::cerr << "[FAIL] action line query mismatch\n";
    return false;
  }

  if (action_line_query.execution_ops.empty() ||
      action_line_query.execution_ops.back().opcode != cxparser_ext::cxseo_action)
  {
    std::cerr << "[FAIL] action line execution op mismatch\n";
    return false;
  }

  if (!line_query.previous_breakpoint.name.empty() ||
      line_query.next_breakpoint.name != "prepare_bp")
  {
    std::cerr << "[FAIL] line query breakpoint before/after mismatch\n";
    return false;
  }

  if (!line_query.previous_checkpoint.name.empty() ||
      !line_query.next_checkpoint.name.empty())
  {
    std::cerr << "[FAIL] line query checkpoint before/after mismatch\n";
    return false;
  }

  cxparser_ext::CxScriptDebugQueryResult breakpoint_line_query;
  if (!runtime.QueryDebugByLine(result.debug_view, 12, breakpoint_line_query))
  {
    std::cerr << "[FAIL] breakpoint line query failed\n";
    return false;
  }

  if (breakpoint_line_query.nearest_breakpoint.name != "prepare_bp" ||
      breakpoint_line_query.nearest_replay_frame.action != "breakpoint" ||
      breakpoint_line_query.matched_execution_step.kind != cxparser_ext::cxsesk_breakpoint ||
      breakpoint_line_query.matched_execution_step.control_tag != "breakpoint")
  {
    std::cerr << "[FAIL] breakpoint line query mismatch\n";
    return false;
  }

  if (breakpoint_line_query.previous_breakpoint.name != "prepare_bp" ||
      !breakpoint_line_query.next_breakpoint.name.empty())
  {
    std::cerr << "[FAIL] breakpoint line query before/after mismatch\n";
    return false;
  }

  cxparser_ext::CxScriptDebugQueryResult check_line_query;
  if (!runtime.QueryDebugByLine(result.debug_view, 15, check_line_query))
  {
    std::cerr << "[FAIL] check line query failed\n";
    return false;
  }

  if (check_line_query.step_name != "check" ||
      check_line_query.next_checkpoint.name != "after_prepare" ||
      !check_line_query.previous_checkpoint.name.empty())
  {
    std::cerr << "[FAIL] check line query checkpoint window mismatch\n";
    return false;
  }

  cxparser_ext::CxScriptDebugQueryResult checkpoint_line_query;
  if (!runtime.QueryDebugByLine(result.debug_view, 16, checkpoint_line_query))
  {
    std::cerr << "[FAIL] checkpoint line query failed\n";
    return false;
  }

  if (checkpoint_line_query.previous_checkpoint.name != "after_prepare" ||
      !checkpoint_line_query.next_checkpoint.name.empty())
  {
    std::cerr << "[FAIL] checkpoint line query before/after mismatch\n";
    return false;
  }

  if (checkpoint_line_query.nearest_replay_frame.action != "checkpoint" ||
      checkpoint_line_query.matched_execution_step.kind != cxparser_ext::cxsesk_checkpoint ||
      checkpoint_line_query.matched_execution_step.control_tag != "checkpoint")
  {
    std::cerr << "[FAIL] checkpoint line query replay mismatch\n";
    return false;
  }

  cxparser_ext::CxScriptDebugQueryResult breakpoint_name_query;
  if (!runtime.QueryDebugByBreakpoint(result.debug_view, "prepare_bp", breakpoint_name_query))
  {
    std::cerr << "[FAIL] breakpoint name query failed\n";
    return false;
  }

  if (breakpoint_name_query.step_name != "prepare" ||
      breakpoint_name_query.step_id <= 0 ||
      breakpoint_name_query.frame_id < 0 ||
      breakpoint_name_query.matched_execution_step.sequence <= 0 ||
      breakpoint_name_query.matched_execution_step.kind != cxparser_ext::cxsesk_breakpoint ||
      breakpoint_name_query.matched_execution_step.control_tag != "breakpoint" ||
      breakpoint_name_query.previous_breakpoint.name != "prepare_bp" ||
      breakpoint_name_query.nearest_replay_frame.action != "breakpoint")
  {
    std::cerr << "[FAIL] breakpoint name query mismatch\n";
    return false;
  }

  cxparser_ext::CxScriptDebugQueryResult sequence_query;
  if (!runtime.QueryDebugBySequence(result.debug_view,
                                    breakpoint_line_query.matched_execution_step.sequence,
                                    sequence_query))
  {
    std::cerr << "[FAIL] sequence query failed\n";
    return false;
  }

  if (sequence_query.sequence != breakpoint_line_query.matched_execution_step.sequence ||
      sequence_query.step_id != breakpoint_line_query.step_id ||
      sequence_query.frame_id != breakpoint_line_query.frame_id ||
      sequence_query.matched_execution_step.kind != cxparser_ext::cxsesk_breakpoint ||
      sequence_query.nearest_replay_frame.action != "breakpoint" ||
      sequence_query.matched_source_entry.statement_kind != "breakpoint" ||
      sequence_query.matched_execution_op.opcode != cxparser_ext::cxseo_breakpoint ||
      sequence_query.previous_execution_step.sequence <= 0)
  {
    std::cerr << "[FAIL] sequence query mismatch\n";
    return false;
  }

  cxparser_ext::CxScriptDebugQueryResult checkpoint_name_query;
  if (!runtime.QueryDebugByCheckpoint(result.debug_view, "after_prepare", checkpoint_name_query))
  {
    std::cerr << "[FAIL] checkpoint name query failed\n";
    return false;
  }

  if (checkpoint_name_query.step_name != "check" ||
      checkpoint_name_query.step_id <= 0 ||
      checkpoint_name_query.frame_id < 0 ||
      checkpoint_name_query.matched_execution_step.sequence <= 0 ||
      checkpoint_name_query.matched_execution_step.kind != cxparser_ext::cxsesk_checkpoint ||
      checkpoint_name_query.matched_execution_step.control_tag != "checkpoint" ||
      checkpoint_name_query.previous_checkpoint.name != "after_prepare" ||
      checkpoint_name_query.nearest_replay_frame.action != "checkpoint")
  {
    std::cerr << "[FAIL] checkpoint name query mismatch\n";
    return false;
  }

  std::cout << "[PASS] debug success traces=" << result.debug_view.step_traces.size()
            << " builtin_fragments=" << fragments.size()
            << " builtin_snippets=" << snippets.size()
            << " builtin_cstyle=" << cstyle_snippets.size()
            << " ready_flows=" << readiness.ready_flow_ids.size()
            << " gap_flows=" << readiness.gap_flow_ids.size()
            << " source=" << result.debug_view.source_map.size()
            << " replay=" << result.debug_view.replay_frames.size()
            << " exec=" << result.debug_view.execution_ops.size()
            << " vars=" << result.debug_view.variables.size()
            << " step_query_vars=" << step_query.variables.size()
            << " line_step=" << line_query.step_name
            << " line_depth=" << line_query.current_block_depth
            << " line_replay=" << line_query.nearest_replay_frame.action
            << " action_replay=" << action_line_query.nearest_replay_frame.action
            << " next_breakpoint=" << line_query.next_breakpoint.name
            << " breakpoint=" << breakpoint_line_query.nearest_breakpoint.name
            << " checkpoint=" << checkpoint_line_query.previous_checkpoint.name
            << " bp_query_step=" << breakpoint_name_query.step_name
            << " cp_query_step=" << checkpoint_name_query.step_name << "\n";
  std::cout << "[BREAKWIN] name=" << breakpoint_line_query.nearest_breakpoint.name
            << " step=" << breakpoint_line_query.step_name
            << " before_line=" << line_query.line
            << " before_next=" << line_query.next_breakpoint.name
            << " current_prev=" << breakpoint_line_query.previous_breakpoint.name
            << " current_next=" << breakpoint_line_query.next_breakpoint.name
            << "\n";
  std::cout << "[CHECKWIN] name=" << checkpoint_line_query.previous_checkpoint.name
            << " step=" << checkpoint_line_query.step_name
            << " before_line=" << check_line_query.line
            << " before_next=" << check_line_query.next_checkpoint.name
            << " current_prev=" << checkpoint_line_query.previous_checkpoint.name
            << " current_next=" << checkpoint_line_query.next_checkpoint.name
            << "\n";
  return true;
}

bool RunUnknownTypeCase()
{
  cxparser_ext::ParserCxScriptRuntime runtime = MakeDebugRuntime();
  cxparser_ext::CxScriptExecutionResult result;

  const char *script =
    "kind=module\n"
    "layer=smoke\n"
    "module=cxcore\n"
    "mode=build-run\n"
    "report=on\n"
    "step broken {\n"
    "UnknownType item;\n"
    "call minimal_binding()\n"
    "}\n";

  if (runtime.ExecuteScriptText("unknown_type.cxs", script, result))
  {
    std::cerr << "[FAIL] unknown type case should fail\n";
    return false;
  }

  if (result.summary.find("unknown type in declaration") != 0 ||
      result.parse_error.token != "UnknownType" ||
      result.parse_error.line != 7 ||
      result.parse_error.column != 1 ||
      result.parse_error.step_name != "broken")
  {
    std::cerr << "[FAIL] unknown type parse error mismatch"
              << " summary=" << result.summary
              << " token=" << result.parse_error.token
              << " line=" << result.parse_error.line
              << " col=" << result.parse_error.column
              << " step=" << result.parse_error.step_name << "\n";
    return false;
  }

  if (result.error_message != "unknown type in declaration")
  {
    std::cerr << "[FAIL] unknown type error message mismatch:"
              << result.error_message << "\n";
    return false;
  }

  std::cout << "[PASS] unknown type token=" << result.parse_error.token
            << " line=" << result.parse_error.line
            << " step=" << result.parse_error.step_name << "\n";
  return true;
}

bool RunUnterminatedBlockCase()
{
  cxparser_ext::ParserCxScriptRuntime runtime = MakeDebugRuntime();
  cxparser_ext::CxScriptExecutionResult result;

  const char *script =
    "kind=module\n"
    "layer=smoke\n"
    "module=cxcore\n"
    "mode=build-run\n"
    "report=on\n"
    "step broken {\n"
    "call minimal_binding()\n";

  if (runtime.ExecuteScriptText("unterminated_block.cxs", script, result))
  {
    std::cerr << "[FAIL] unterminated block case should fail\n";
    return false;
  }

  if (result.summary.find("unterminated block boundary") != 0 ||
      result.parse_error.token != "{" ||
      result.parse_error.column != 1 ||
      result.parse_error.step_name != "broken")
  {
    std::cerr << "[FAIL] unterminated block parse error mismatch"
              << " summary=" << result.summary
              << " token=" << result.parse_error.token
              << " col=" << result.parse_error.column
              << " step=" << result.parse_error.step_name
              << " depth=" << result.parse_error.block_depth << "\n";
    return false;
  }

  if (result.error_message != "unterminated block boundary")
  {
    std::cerr << "[FAIL] unterminated block error message mismatch:"
              << result.error_message << "\n";
    return false;
  }

  std::cout << "[PASS] unterminated block token=" << result.parse_error.token
            << " step=" << result.parse_error.step_name
            << " depth=" << result.parse_error.block_depth << "\n";
  return true;
}

bool RunCStyleTraceCase()
{
  cxparser_ext::ParserCxScriptRuntime runtime = MakeDebugRuntime();
  cxparser_ext::CxScriptExecutionResult result;

  const char *script =
    "kind=module\n"
    "layer=feature\n"
    "module=cxcore\n"
    "case=line_measurement_balanced\n"
    "mode=build-run\n"
    "report=on\n"
    "step prepare {\n"
    "double threshold = 0.8;\n"
    "{\n"
    "print(\"prepare.inner\");\n"
    "checkpoint inner_cp;\n"
    "}\n"
    "breakpoint prepare_bp;\n"
    "}\n"
    "step check {\n"
    "check(success == true);\n"
    "print(summary);\n"
    "}\n";

  if (!runtime.ExecuteScriptText("cstyle_trace.cxs", script, result))
  {
    std::cerr << "[FAIL] cstyle trace case summary=" << result.summary << "\n";
    return false;
  }

  bool saw_print = false;
  bool saw_check = false;
  bool saw_frame_enter = false;
  bool saw_frame_exit = false;
  for (size_t i = 0; i < result.debug_view.source_map.size(); ++i)
  {
    const std::string &kind = result.debug_view.source_map[i].statement_kind;
    if (kind == "print")
      saw_print = true;
    if (kind == "check")
      saw_check = true;
    if (kind == "frame_enter")
      saw_frame_enter = true;
    if (kind == "frame_exit")
      saw_frame_exit = true;
  }

  if (!saw_print || !saw_check || !saw_frame_enter || !saw_frame_exit)
  {
    std::cerr << "[FAIL] cstyle trace source map labels mismatch\n";
    return false;
  }

  if (result.debug_view.execution_summary.entry_step_id <= 0 ||
      result.debug_view.execution_summary.check_step_id <= 0 ||
      result.debug_view.execution_summary.max_sequence < 10 ||
      result.debug_view.execution_summary.max_block_depth != 2 ||
      result.debug_view.execution_summary.header_step_count < 6 ||
      result.debug_view.execution_summary.frame_step_count < 2 ||
      result.debug_view.execution_summary.check_step_count < 1 ||
      result.debug_view.execution_summary.print_step_count < 2 ||
      result.debug_view.execution_summary.breakpoint_step_count < 1 ||
      result.debug_view.execution_summary.checkpoint_step_count < 1)
  {
    std::cerr << "[FAIL] cstyle execution summary mismatch\n";
    return false;
  }

  cxparser_ext::CxScriptDebugQueryResult print_query;
  if (!runtime.QueryDebugByLine(result.debug_view, 10, print_query))
  {
    std::cerr << "[FAIL] cstyle print line query failed\n";
    return false;
  }

  if (!HasSourceKind(print_query.source_entries, "print") ||
      print_query.nearest_replay_frame.action != "print" ||
      print_query.matched_execution_step.action != "print" ||
      print_query.matched_execution_step.kind != cxparser_ext::cxsesk_print ||
      print_query.matched_execution_step.control_tag != "print" ||
      print_query.matched_source_entry.statement_kind != "print" ||
      print_query.matched_execution_op.opcode != cxparser_ext::cxseo_emit ||
      print_query.previous_replay_frame.sequence <= 0 ||
      print_query.step_id <= 0 ||
      print_query.frame_id <= 0 ||
      print_query.current_block_depth != 2)
  {
    std::cerr << "[FAIL] cstyle print line query mismatch\n";
    return false;
  }

  cxparser_ext::CxScriptDebugQueryResult checkpoint_query;
  if (!runtime.QueryDebugByLine(result.debug_view, 11, checkpoint_query))
  {
    std::cerr << "[FAIL] cstyle checkpoint line query failed\n";
    return false;
  }

  if (checkpoint_query.nearest_checkpoint.name != "inner_cp" ||
      checkpoint_query.matched_execution_step.action != "checkpoint" ||
      checkpoint_query.matched_execution_step.kind != cxparser_ext::cxsesk_checkpoint ||
      checkpoint_query.matched_execution_step.control_tag != "checkpoint" ||
      checkpoint_query.previous_replay_frame.sequence <= 0 ||
      checkpoint_query.next_replay_frame.sequence <= 0 ||
      checkpoint_query.step_id <= 0 ||
      checkpoint_query.nearest_replay_frame.action != "checkpoint")
  {
    std::cerr << "[FAIL] cstyle checkpoint line query mismatch\n";
    return false;
  }

  cxparser_ext::CxScriptDebugQueryResult print_sequence_query;
  if (!runtime.QueryDebugBySequence(result.debug_view,
                                    print_query.matched_execution_step.sequence,
                                    print_sequence_query))
  {
    std::cerr << "[FAIL] cstyle print sequence query failed\n";
    return false;
  }

  if (print_sequence_query.step_id != print_query.step_id ||
      print_sequence_query.frame_id != print_query.frame_id ||
      print_sequence_query.matched_execution_step.kind != cxparser_ext::cxsesk_print ||
      print_sequence_query.execution_summary.max_block_depth != 2)
  {
    std::cerr << "[FAIL] cstyle print sequence query mismatch\n";
    return false;
  }

  cxparser_ext::CxScriptDebugQueryResult check_query;
  if (!runtime.QueryDebugByLine(result.debug_view, 16, check_query))
  {
    std::cerr << "[FAIL] cstyle check line query failed\n";
    return false;
  }

  if (!HasSourceKind(check_query.source_entries, "check") ||
      check_query.step_id <= 0 ||
      check_query.matched_execution_step.action != "check" ||
      check_query.matched_execution_step.kind != cxparser_ext::cxsesk_check ||
      check_query.matched_execution_step.control_tag != "check" ||
      check_query.matched_source_entry.statement_kind != "check" ||
      check_query.matched_execution_op.opcode != cxparser_ext::cxseo_expect ||
      check_query.nearest_replay_frame.action != "check")
  {
    std::cerr << "[FAIL] cstyle check line query mismatch"
              << " step_id=" << check_query.step_id
              << " frame_id=" << check_query.frame_id
              << " replay=" << check_query.nearest_replay_frame.action;
    for (size_t i = 0; i < check_query.source_entries.size(); ++i)
      std::cerr << " source=" << check_query.source_entries[i].statement_kind;
    for (size_t i = 0; i < result.debug_view.source_map.size(); ++i)
      std::cerr << " map=(" << result.debug_view.source_map[i].span.line_begin
                << "," << result.debug_view.source_map[i].statement_kind
                << "," << result.debug_view.source_map[i].step_name << ")";
    std::cerr << "\n";
    return false;
  }

  cxparser_ext::CxScriptDebugQueryResult summary_print_query;
  if (!runtime.QueryDebugByLine(result.debug_view, 17, summary_print_query))
  {
    std::cerr << "[FAIL] cstyle summary print line query failed\n";
    return false;
  }

  if (!HasSourceKind(summary_print_query.source_entries, "print") ||
      summary_print_query.matched_execution_step.action != "print" ||
      summary_print_query.matched_execution_step.kind != cxparser_ext::cxsesk_print ||
      summary_print_query.matched_execution_step.control_tag != "print" ||
      summary_print_query.nearest_replay_frame.action != "print")
  {
    std::cerr << "[FAIL] cstyle summary print line query mismatch\n";
    return false;
  }

  if (result.result_object != "LineMeasurementOutput" ||
      result.failure_mode != "none" ||
      result.summary.empty())
  {
    std::cerr << "[FAIL] cstyle trace result summary/object mismatch\n";
    return false;
  }

  return true;
}

bool RunEnsmallenFlowHostCase()
{
  cxparser_ext::ParserCxScriptRuntime runtime = MakeDebugRuntime();
  cxparser_ext::CxScriptExecutionResult result;

  const char *script =
    "flow=CxCoreFlowHost();\n"
    "flow.set_case(\"cxcore.ensmallen_layer.geometry_fit_tuning.feature\");\n"
    "flow.set_layer(\"feature\");\n"
    "flow.set_module(\"ensmallen_layer\");\n"
    "flow.set_feature(\"FormfitGeometryFitTuning\");\n"
    "flow.input_image(\"phase1.fit_circle.circle_double\");\n"
    "flow.input_roi(\"roi_main\",40,52,176,136);\n"
    "flow.input_gt(\"geometry_gt\",\"resolved_by_case\");\n"
    "flow.input_param_json(\"params\",\"resolved_by_case\");\n"
    "flow.call(\"ResolveTuningCase\");\n"
    "flow.call(\"RunBaselineEval\");\n"
    "flow.call(\"RunGeometryFitTuning\");\n"
    "flow.call(\"CompareBaselineVsBest\");\n"
    "flow.expect_output(\"MeasuredOptimizeResult\");\n"
    "flow.expect_output(\"MeasuredCompareResult\");\n"
    "flow.expect_output(\"MeasuredReplayResult\");\n"
    "flow.expect_output(\"RagWritebackNote\");\n"
    "flow.expect_field(\"baseline_objective\");\n"
    "flow.expect_field(\"replay_log_path\");\n"
    "flow.check_trace_contains(\"RunGeometryFitTuning\");\n"
    "flow.finish();\n";

  if (!runtime.ExecuteScriptText("ensmallen_flow_host.cxs", script, result))
  {
    std::cerr << "[FAIL] ensmallen flow host case summary=" << result.summary << "\n";
    return false;
  }

  if (!result.success ||
      result.degraded ||
      result.kind != "module" ||
      result.layer != "feature" ||
      result.module != "ensmallen_layer" ||
      result.case_name != "geometry_fit_tuning" ||
      result.route != "ensmallen.flow_host")
  {
    std::cerr << "[FAIL] ensmallen flow host context/result mismatch\n";
    return false;
  }

  if (result.result_object != "EnsmallenFlowHostResult" ||
      result.optimize_summary_object != "MeasuredOptimizeResult" ||
      result.compare_summary_object != "MeasuredCompareResult" ||
      result.replay_result_object != "MeasuredReplayResult" ||
      result.rag_writeback_note_object != "RagWritebackNote" ||
      result.baseline_objective != 0.368 ||
      result.best_objective != 0.194 ||
      result.objective_delta != -0.174 ||
      result.metric_delta != -0.174 ||
      result.stability_delta != -0.052 ||
      result.pass_level != "pass" ||
      result.replay_log_path != "ensmallen_layer_geometry_fit_tuning_measured_replay.jsonl")
  {
    std::cerr << "[FAIL] ensmallen flow host result contract fields mismatch\n";
    return false;
  }

  if (!HasExecutionStepKind(result.debug_view.execution_steps, cxparser_ext::cxsesk_call) ||
      !HasExecutionStepKind(result.debug_view.execution_steps, cxparser_ext::cxsesk_var_decl))
  {
    std::cerr << "[FAIL] ensmallen flow host execution-step coverage mismatch\n";
    return false;
  }

  bool saw_flow_contract = false;
  bool saw_objects = false;
  bool saw_fields = false;
  bool saw_replay_line = false;
  bool saw_channel_line = false;
  bool saw_active_inputs_line = false;
  bool saw_reserved_inputs_line = false;
  bool saw_torch_input_refs_line = false;
  bool saw_convergence_line = false;
  bool saw_calls_line = false;
  bool saw_expect_line = false;
  bool saw_check_line = false;
  bool saw_refs_line = false;
  for (size_t i = 0; i < result.details.size(); ++i)
  {
    if (result.details[i].find("[ENSMALLEN_FLOW_HOST] geometry_fit_tuning") != std::string::npos)
      saw_flow_contract = true;
    if (result.details[i].find("[ENSMALLEN_CHANNEL] formfit.geometry_fit_channel") != std::string::npos)
      saw_channel_line = true;
    if (result.details[i].find("[ENSMALLEN_ACTIVE_INPUTS] active_inputs=geometry_ref,boundary_metrics_ref,fit_targets_ref,params,objective_weights,objective_ref,boundary_error_ref,alignment_error_ref") != std::string::npos)
      saw_active_inputs_line = true;
    if (result.details[i].find("[ENSMALLEN_RESERVED_INPUTS] reserved_geometry_inputs=geometry_ref,boundary_metrics_ref,fit_targets_ref") != std::string::npos)
      saw_reserved_inputs_line = true;
    if (result.details[i].find("[ENSMALLEN_TORCH_INPUT_REFS] torch_optimization_inputs=objective_ref,boundary_error_ref,alignment_error_ref") != std::string::npos)
      saw_torch_input_refs_line = true;
    if (result.details[i].find("[ENSMALLEN_CONVERGENCE] status=converged tolerance=fixed_point_delta<=0.001") != std::string::npos)
      saw_convergence_line = true;
    if (result.details[i].find("[ENSMALLEN_CALLS] flow.call ResolveTuningCase/RunBaselineEval/Optimize/Compare") != std::string::npos)
      saw_calls_line = true;
    if (result.details[i].find("[ENSMALLEN_EXPECT] flow.expect_output(MeasuredOptimizeResult,MeasuredCompareResult,MeasuredReplayResult,RagWritebackNote)/flow.expect_field") != std::string::npos)
      saw_expect_line = true;
    if (result.details[i].find("[ENSMALLEN_CHECK] flow.check_trace_contains/flow.check_artifact_exists/flow.check_scalar_le") != std::string::npos)
      saw_check_line = true;
    if (result.details[i].find("[ENSMALLEN_OBJECTS] MeasuredOptimizeResult->MeasuredCompareResult->MeasuredReplayResult->RagWritebackNote") != std::string::npos)
      saw_objects = true;
    if (result.details[i].find("[ENSMALLEN_FIELDS] baseline_objective=0.368000") != std::string::npos)
      saw_fields = true;
    if (result.details[i].find("[ENSMALLEN_REFS] objective_ref=ensmallen_layer.feature.geometry_fit_tuning.objective") != std::string::npos)
      saw_refs_line = true;
  }

  for (size_t i = 0; i < result.details.size(); ++i)
  {
    if (result.details[i].find("[OPT-REPLAY] object=MeasuredReplayResult replay_log_path=ensmallen_layer_geometry_fit_tuning_measured_replay.jsonl pass_level=pass") != std::string::npos)
      saw_replay_line = true;
  }
  if (!saw_flow_contract || !saw_channel_line || !saw_active_inputs_line ||
      !saw_reserved_inputs_line || !saw_torch_input_refs_line || !saw_convergence_line ||
      !saw_calls_line || !saw_expect_line || !saw_check_line ||
      !saw_objects || !saw_fields ||
      !saw_refs_line || !saw_replay_line)
  {
    std::cerr << "[FAIL] ensmallen flow host fallback marker missing\n";
    return false;
  }

  if (!HasNamedResultField(result, "optimize", "baseline_objective", "0.368000") ||
      !HasNamedResultField(result, "compare", "pass_level", "pass") ||
      !HasNamedResultField(result, "replay", "replay_log_path", "ensmallen_layer_geometry_fit_tuning_measured_replay.jsonl") ||
      !HasNamedResultField(result, "refs", "objective_ref", "ensmallen_layer.feature.geometry_fit_tuning.objective") ||
      !HasNamedResultField(result, "refs", "optimization_result_ref", "ensmallen_layer.feature.geometry_fit_tuning.optimization") ||
      !HasNamedResultField(result, "refs", "best_params_ref", "ensmallen_layer.feature.geometry_fit_tuning.best_params") ||
      !HasNamedResultField(result, "refs", "objective_delta_ref", "ensmallen_layer.feature.geometry_fit_tuning.objective_delta") ||
      !HasNamedResultField(result, "refs", "summary_ref", "ensmallen_layer.feature.geometry_fit_tuning.summary") ||
      !HasNamedResultField(result, "refs", "compare_ref", "ensmallen_layer.feature.geometry_fit_tuning.compare") ||
      !HasNamedResultField(result, "refs", "replay_ref", "ensmallen_layer_geometry_fit_tuning_measured_replay.jsonl") ||
      !HasNamedResultField(result, "refs", "next_action", "consume optimization_result_ref and replay_ref") ||
      !HasNamedResultField(result, "inputs", "channel", "formfit.geometry_fit_channel") ||
      !HasNamedResultField(result, "inputs", "active_inputs", "geometry_ref,boundary_metrics_ref,fit_targets_ref,params,objective_weights,objective_ref,boundary_error_ref,alignment_error_ref") ||
      !HasNamedResultField(result, "inputs", "reserved_inputs", "geometry_ref,boundary_metrics_ref,fit_targets_ref") ||
      !HasNamedResultField(result, "inputs", "torch_optimization_inputs", "objective_ref,boundary_error_ref,alignment_error_ref") ||
      !HasNamedResultField(result, "inputs", "dataset_bridge", "bridge.synthetic_phase1") ||
      !HasNamedResultField(result, "inputs", "test_bucket", "G0.baseline_stable,G1.tuning_target") ||
      !HasNamedResultField(result, "inputs", "test_flow", "bucket -> baseline_eval -> geometry_fit_optimize -> compare -> replay") ||
      !HasNamedResultField(result, "conclusion", "chain_status", "passed") ||
      !HasNamedResultField(result, "conclusion", "export_status", "passed") ||
      !HasNamedResultField(result, "conclusion", "algorithm_status", "pending_human_review") ||
      !HasNamedResultField(result, "conclusion", "human_review_required", "required") ||
      !HasNamedResultField(result, "report_header", "entry", "cxparser_ext_cxscript_cli") ||
      !HasNamedResultField(result, "report_header", "batch", "geometry") ||
      !HasNamedResultField(result, "test_plan", "mcp_flow", "run-ctest-target exact_name -> task_id -> compare/replay -> conclusion/evidence") ||
      !HasNamedResultField(result, "interaction", "route", "cxcore.formfit -> ensmallen -> rag"))
  {
    std::cerr << "[FAIL] ensmallen flow host named measured result mismatch\n";
    return false;
  }

  if (GetNamedResultFieldValue(result, "optimize", "baseline_objective").empty())
  {
    std::cerr << "[FAIL] ensmallen flow host baseline objective field missing\n";
    return false;
  }

  return true;
}

bool RunEnsmallenMatchScoreFlowHostCase()
{
  cxparser_ext::ParserCxScriptRuntime runtime = MakeDebugRuntime();
  cxparser_ext::CxScriptExecutionResult result;

  const char *script =
    "flow=CxCoreFlowHost();\n"
    "flow.set_case(\"cxcore.ensmallen_layer.match_score_tuning.feature\");\n"
    "flow.set_layer(\"feature\");\n"
    "flow.set_module(\"ensmallen_layer\");\n"
    "flow.set_feature(\"FastMatchScoreTuning\");\n"
    "flow.input_image(\"phase1.template_match.template_scene\");\n"
    "flow.input_model(\"phase1.template_match.template_patch\");\n"
    "flow.input_roi(\"roi_match\",72,60,96,84);\n"
    "flow.input_gt(\"match_gt\",\"resolved_by_case\");\n"
    "flow.input_param_json(\"params\",\"resolved_by_case\");\n"
    "flow.call(\"ResolveTuningCase\");\n"
    "flow.call(\"RunBaselineEval\");\n"
    "flow.call(\"RunMatchScoreTuning\");\n"
    "flow.call(\"CompareBaselineVsBest\");\n"
    "flow.expect_output(\"MeasuredOptimizeResult\");\n"
    "flow.expect_output(\"MeasuredCompareResult\");\n"
    "flow.expect_output(\"MeasuredReplayResult\");\n"
    "flow.expect_output(\"RagWritebackNote\");\n"
    "flow.expect_field(\"baseline_objective\");\n"
    "flow.expect_field(\"replay_log_path\");\n"
    "flow.check_trace_contains(\"RunMatchScoreTuning\");\n"
    "flow.finish();\n";

  if (!runtime.ExecuteScriptText("ensmallen_match_score_flow_host.cxs", script, result))
  {
    std::cerr << "[FAIL] ensmallen match score flow host case summary=" << result.summary << "\n";
    return false;
  }

  if (!result.success ||
      result.degraded ||
      result.kind != "module" ||
      result.layer != "feature" ||
      result.module != "ensmallen_layer" ||
      result.case_name != "match_score_tuning" ||
      result.route != "ensmallen.flow_host")
  {
    std::cerr << "[FAIL] ensmallen match score context/result mismatch\n";
    return false;
  }

  if (result.result_object != "EnsmallenFlowHostResult" ||
      result.optimize_summary_object != "MeasuredOptimizeResult" ||
      result.compare_summary_object != "MeasuredCompareResult" ||
      result.replay_result_object != "MeasuredReplayResult" ||
      result.rag_writeback_note_object != "RagWritebackNote" ||
      result.baseline_objective != 0.284 ||
      result.best_objective != 0.119 ||
      result.objective_delta != -0.165 ||
      result.metric_delta != -0.165 ||
      result.stability_delta != -0.041 ||
      result.pass_level != "pass" ||
      result.replay_log_path != "ensmallen_layer_match_score_tuning_measured_replay.jsonl")
  {
    std::cerr << "[FAIL] ensmallen match score contract fields mismatch\n";
    return false;
  }

  bool saw_flow_contract = false;
  bool saw_channel_line = false;
  bool saw_active_inputs_line = false;
  bool saw_reserved_inputs_line = false;
  bool saw_torch_input_refs_line = false;
  bool saw_convergence_line = false;
  bool saw_calls_line = false;
  bool saw_expect_line = false;
  bool saw_check_line = false;
  bool saw_refs_line = false;
  bool saw_replay_line = false;
  for (size_t i = 0; i < result.details.size(); ++i)
  {
    if (result.details[i].find("[ENSMALLEN_FLOW_HOST] match_score_tuning") != std::string::npos)
      saw_flow_contract = true;
    if (result.details[i].find("[ENSMALLEN_CHANNEL] fastmatch.structural_match_channel") != std::string::npos)
      saw_channel_line = true;
    if (result.details[i].find("[ENSMALLEN_ACTIVE_INPUTS] active_inputs=roi_ref,match_gt,params,objective_weights,objective_ref,threshold_ref,crop_policy_ref") != std::string::npos)
      saw_active_inputs_line = true;
    if (result.details[i].find("[ENSMALLEN_RESERVED_INPUTS] reserved_region_pattern_inputs=RegionPatternConfig,RegionPatternDescriptor,RegionPatternScore") != std::string::npos)
      saw_reserved_inputs_line = true;
    if (result.details[i].find("[ENSMALLEN_TORCH_INPUT_REFS] torch_optimization_inputs=objective_ref,threshold_ref,crop_policy_ref") != std::string::npos)
      saw_torch_input_refs_line = true;
    if (result.details[i].find("[ENSMALLEN_CONVERGENCE] status=converged tolerance=fixed_point_delta<=0.001") != std::string::npos)
      saw_convergence_line = true;
    if (result.details[i].find("[ENSMALLEN_CALLS] flow.call ResolveTuningCase/RunBaselineEval/Optimize/Compare") != std::string::npos)
      saw_calls_line = true;
    if (result.details[i].find("[ENSMALLEN_EXPECT] flow.expect_output(MeasuredOptimizeResult,MeasuredCompareResult,MeasuredReplayResult,RagWritebackNote)/flow.expect_field") != std::string::npos)
      saw_expect_line = true;
    if (result.details[i].find("[ENSMALLEN_CHECK] flow.check_trace_contains/flow.check_artifact_exists/flow.check_scalar_le") != std::string::npos)
      saw_check_line = true;
    if (result.details[i].find("[ENSMALLEN_REFS] objective_ref=ensmallen_layer.feature.match_score_tuning.objective") != std::string::npos)
      saw_refs_line = true;
    if (result.details[i].find("[OPT-REPLAY] object=MeasuredReplayResult replay_log_path=ensmallen_layer_match_score_tuning_measured_replay.jsonl pass_level=pass") != std::string::npos)
      saw_replay_line = true;
  }

  if (!saw_flow_contract || !saw_channel_line || !saw_active_inputs_line ||
      !saw_reserved_inputs_line || !saw_torch_input_refs_line || !saw_convergence_line ||
      !saw_calls_line || !saw_expect_line || !saw_check_line ||
      !saw_refs_line || !saw_replay_line)
  {
    std::cerr << "[FAIL] ensmallen match score fallback marker missing\n";
    return false;
  }

  if (!HasNamedResultField(result, "optimize", "baseline_objective", "0.284000") ||
      !HasNamedResultField(result, "compare", "pass_level", "pass") ||
      !HasNamedResultField(result, "replay", "replay_log_path", "ensmallen_layer_match_score_tuning_measured_replay.jsonl") ||
      !HasNamedResultField(result, "refs", "objective_ref", "ensmallen_layer.feature.match_score_tuning.objective") ||
      !HasNamedResultField(result, "refs", "optimization_result_ref", "ensmallen_layer.feature.match_score_tuning.optimization") ||
      !HasNamedResultField(result, "refs", "best_params_ref", "ensmallen_layer.feature.match_score_tuning.best_params") ||
      !HasNamedResultField(result, "refs", "objective_delta_ref", "ensmallen_layer.feature.match_score_tuning.objective_delta") ||
      !HasNamedResultField(result, "refs", "summary_ref", "ensmallen_layer.feature.match_score_tuning.summary") ||
      !HasNamedResultField(result, "refs", "compare_ref", "ensmallen_layer.feature.match_score_tuning.compare") ||
      !HasNamedResultField(result, "refs", "replay_ref", "ensmallen_layer_match_score_tuning_measured_replay.jsonl") ||
      !HasNamedResultField(result, "refs", "next_action", "consume optimization_result_ref and replay_ref") ||
      !HasNamedResultField(result, "inputs", "channel", "fastmatch.structural_match_channel") ||
      !HasNamedResultField(result, "inputs", "active_inputs", "roi_ref,match_gt,params,objective_weights,objective_ref,threshold_ref,crop_policy_ref") ||
      !HasNamedResultField(result, "inputs", "reserved_inputs", "RegionPatternConfig,RegionPatternDescriptor,RegionPatternScore") ||
      !HasNamedResultField(result, "inputs", "torch_optimization_inputs", "objective_ref,threshold_ref,crop_policy_ref") ||
      !HasNamedResultField(result, "inputs", "dataset_bridge", "bridge.synthetic_phase1") ||
      !HasNamedResultField(result, "inputs", "test_bucket", "G0.baseline_stable,G1.tuning_target") ||
      !HasNamedResultField(result, "inputs", "test_flow", "bucket -> baseline_eval -> match_score_optimize -> compare -> replay") ||
      !HasNamedResultField(result, "test_plan", "image_selection", "select template_scene and template_scene_rotated before candidate competition images") ||
      !HasNamedResultField(result, "interaction", "route", "cxcore.fastmatch -> ensmallen -> rag"))
  {
    std::cerr << "[FAIL] ensmallen match score named measured result mismatch\n";
    return false;
  }

  return true;
}

bool RunEnsmallenScenarioFlowHostCase()
{
  cxparser_ext::ParserCxScriptRuntime runtime = MakeDebugRuntime();
  cxparser_ext::CxScriptExecutionResult result;

  const char *script =
    "flow=CxCoreFlowHost();\n"
    "flow.set_case(\"cxcore.ensmallen_layer.phase1_param_replay.scenario\");\n"
    "flow.set_layer(\"scenario\");\n"
    "flow.set_module(\"ensmallen_layer\");\n"
    "flow.set_feature(\"Phase1ParamReplayScenario\");\n"
    "flow.input_dataset(\"dataset.cxcore.phase1.ensmallen\");\n"
    "flow.input_param(\"repeat_count\",3);\n"
    "flow.input_param(\"replay_enable\",1);\n"
    "flow.input_param(\"compare_enable\",1);\n"
    "flow.input_param(\"objective_ref\",\"torch.optimization.objective_ref\");\n"
    "flow.input_param(\"threshold_ref\",\"torch.optimization.threshold_ref\");\n"
    "flow.input_param(\"crop_policy_ref\",\"torch.optimization.crop_policy_ref\");\n"
    "flow.input_param(\"boundary_error_ref\",\"torch.optimization.boundary_error_ref\");\n"
    "flow.input_param(\"alignment_error_ref\",\"torch.optimization.alignment_error_ref\");\n"
    "flow.call(\"ResolvePhase1SampleBundle\");\n"
    "flow.call(\"EnsmallenScenarioReplay\");\n"
    "flow.call(\"EnsmallenScenarioCompare\");\n"
    "flow.expect_output(\"MeasuredScenarioOptimizeResult\");\n"
    "flow.expect_output(\"MeasuredScenarioCompareResult\");\n"
    "flow.expect_output(\"MeasuredScenarioReplayResult\");\n"
    "flow.expect_field(\"sample_summaries\");\n"
    "flow.expect_field(\"pass_fail\");\n"
    "flow.expect_field(\"replay_log_path\");\n"
    "flow.finish();\n";

  if (!runtime.ExecuteScriptText("ensmallen_scenario_flow_host.cxs", script, result))
  {
    std::cerr << "[FAIL] ensmallen scenario flow host case summary=" << result.summary << "\n";
    return false;
  }

  if (!result.success ||
      result.degraded ||
      result.layer != "scenario" ||
      result.module != "ensmallen_layer" ||
      result.case_name != "phase1_param_replay" ||
      result.route != "ensmallen.flow_host" ||
      result.result_object != "MeasuredScenarioReplayResult" ||
      result.optimize_summary_object != "MeasuredScenarioOptimizeResult" ||
      result.compare_summary_object != "MeasuredScenarioCompareResult" ||
      result.replay_result_object != "MeasuredScenarioReplayResult" ||
      result.summary != "ensmallen scenario replay/compare result ready")
  {
    std::cerr << "[FAIL] ensmallen scenario flow host context/result mismatch\n";
    return false;
  }

  bool saw_channel_line = false;
  bool saw_active_inputs_line = false;
  bool saw_reserved_inputs_line = false;
  bool saw_torch_input_refs_line = false;
  bool saw_convergence_line = false;
  bool saw_calls_line = false;
  bool saw_expect_line = false;
  bool saw_check_line = false;
  bool saw_objects_line = false;
  bool saw_replay_line = false;
  for (size_t i = 0; i < result.details.size(); ++i)
  {
    if (result.details[i].find("[ENSMALLEN_CHANNEL] phase1.replay_compare_stage") != std::string::npos)
      saw_channel_line = true;
    if (result.details[i].find("[ENSMALLEN_ACTIVE_INPUTS] active_inputs=dataset_ref,sample_bundle_ref,repeat_count,replay_enable,compare_enable,objective_ref,threshold_ref,crop_policy_ref,boundary_error_ref,alignment_error_ref") != std::string::npos)
      saw_active_inputs_line = true;
    if (result.details[i].find("[ENSMALLEN_RESERVED_INPUTS] reserved_stage_outputs=sample_summaries,pass_fail,replay_log_path,scenario_compare.json") != std::string::npos)
      saw_reserved_inputs_line = true;
    if (result.details[i].find("[ENSMALLEN_TORCH_INPUT_REFS] torch_optimization_inputs=objective_ref,threshold_ref,crop_policy_ref,boundary_error_ref,alignment_error_ref") != std::string::npos)
      saw_torch_input_refs_line = true;
    if (result.details[i].find("[ENSMALLEN_CONVERGENCE] status=converged tolerance=scenario_compare_ready") != std::string::npos)
      saw_convergence_line = true;
    if (result.details[i].find("[ENSMALLEN_CALLS] flow.call ResolvePhase1SampleBundle/EnsmallenScenarioReplay/EnsmallenScenarioCompare") != std::string::npos)
      saw_calls_line = true;
    if (result.details[i].find("[ENSMALLEN_EXPECT] flow.expect_output(MeasuredScenarioOptimizeResult,MeasuredScenarioCompareResult,MeasuredScenarioReplayResult)/flow.expect_field") != std::string::npos)
      saw_expect_line = true;
    if (result.details[i].find("[ENSMALLEN_CHECK] flow.check_trace_contains/flow.check_artifact_exists/flow.check_count_ge") != std::string::npos)
      saw_check_line = true;
    if (result.details[i].find("[ENSMALLEN_OBJECTS] MeasuredScenarioOptimizeResult->MeasuredScenarioCompareResult->MeasuredScenarioReplayResult") != std::string::npos)
      saw_objects_line = true;
    if (result.details[i].find("[OPT-REPLAY] object=MeasuredReplayResult replay_log_path=ensmallen_layer_phase1_param_replay_measured_replay.jsonl pass_level=pass") != std::string::npos)
      saw_replay_line = true;
  }

  if (!saw_channel_line || !saw_active_inputs_line || !saw_reserved_inputs_line ||
      !saw_torch_input_refs_line || !saw_convergence_line || !saw_calls_line ||
      !saw_expect_line || !saw_check_line || !saw_objects_line || !saw_replay_line)
  {
    std::cerr << "[FAIL] ensmallen scenario flow host marker missing\n";
    return false;
  }

  if (!HasNamedResultField(result, "inputs", "channel", "phase1.replay_compare_stage") ||
      !HasNamedResultField(result, "inputs", "active_inputs", "dataset_ref,sample_bundle_ref,repeat_count,replay_enable,compare_enable,objective_ref,threshold_ref,crop_policy_ref,boundary_error_ref,alignment_error_ref") ||
      !HasNamedResultField(result, "inputs", "reserved_inputs", "sample_summaries,pass_fail,replay_log_path,scenario_compare.json") ||
      !HasNamedResultField(result, "inputs", "torch_optimization_inputs", "objective_ref,threshold_ref,crop_policy_ref,boundary_error_ref,alignment_error_ref") ||
      !HasNamedResultField(result, "inputs", "dataset_bridge", "bridge.synthetic_phase1") ||
      !HasNamedResultField(result, "inputs", "test_bucket", "G0.baseline_stable,G1.tuning_target") ||
      !HasNamedResultField(result, "inputs", "test_flow", "bucket -> replay_compare -> sample_summaries/pass_fail -> replay_ref") ||
      !HasNamedResultField(result, "conclusion", "chain_status", "passed") ||
      !HasNamedResultField(result, "conclusion", "export_status", "passed") ||
      !HasNamedResultField(result, "conclusion", "algorithm_status", "pending_human_review") ||
      !HasNamedResultField(result, "test_plan", "mcp_flow", "run-ctest-target exact_name -> task_id -> status/log -> conclusion/evidence") ||
      !HasNamedResultField(result, "interaction", "route", "torch.optimization_refs -> cxcore.phase1_bundle -> ensmallen -> rag") ||
      !HasNamedResultField(result, "analysis", "bucket_focus", "G0.baseline_stable,G1.tuning_target,G2.candidate_competition,G3.stress_boundary") ||
      !HasNamedResultField(result, "analysis", "likely_issue_class", "boundary_degradation") ||
      !HasNamedResultField(result, "analysis", "next_bucket_focus", "G3.stress_boundary") ||
      !HasNamedResultField(result, "analysis", "observation_mode", "bundle_replay_compare") ||
      !HasNamedResultField(result, "analysis", "expansion_gate", "expand_next_bucket") ||
      !HasNamedResultField(result, "analysis", "bucket_coverage", "real_match_bucket_coverage") ||
      !HasNamedResultField(result, "analysis", "risk_axis", "boundary_and_roi") ||
      !HasNamedResultField(result, "analysis", "coverage_gap", "missing_G4_pipeline_bundle") ||
      !HasNamedResultField(result, "analysis", "observation_priority", "prioritize_boundary_roi_review") ||
      !HasNamedResultField(result, "analysis", "coverage_status", "real_match_ready_pipeline_bundle_pending") ||
      !HasNamedResultField(result, "analysis", "next_review_action", "review_G3_boundary_roi_cases") ||
      !HasNamedResultField(result, "analysis", "optimization_signal", "improved_major_boundary_and_roi") ||
      !HasNamedResultField(result, "analysis", "bucket_review_template", "G0=baseline_guard;G1=param_tuning;G2=candidate_ordering;G3=boundary_roi;G4=pipeline_bundle") ||
      !HasNamedResultField(result, "analysis", "review_scope", "scenario_bundle_replay_compare") ||
      !HasNamedResultField(result, "analysis", "primary_review_ref", "ensmallen_layer.scenario.phase1_param_replay.compare") ||
      !HasNamedResultField(result, "comparison", "comparison_status", "improved") ||
      !HasNamedResultField(result, "comparison", "comparison_magnitude", "major") ||
      !HasNamedResultField(result, "comparison", "primary_review_ref", "ensmallen_layer.scenario.phase1_param_replay.compare") ||
      !HasNamedResultField(result, "refs", "summary_ref", "ensmallen_layer.scenario.phase1_param_replay.summary") ||
      !HasNamedResultField(result, "refs", "compare_ref", "ensmallen_layer.scenario.phase1_param_replay.compare") ||
      !HasNamedResultField(result, "refs", "replay_ref", "ensmallen_layer_phase1_param_replay_measured_replay.jsonl"))
  {
    std::cerr << "[FAIL] ensmallen scenario named result mismatch\n";
    return false;
  }

  return true;
}

bool RunEnsmallenTrainFlowHostCase()
{
  cxparser_ext::ParserCxScriptRuntime runtime = MakeDebugRuntime();
  cxparser_ext::CxScriptExecutionResult result;

  const char *script =
    "flow=CxCoreFlowHost();\n"
    "flow.set_case(\"cxcore.ensmallen_layer.phase1_param_opt.train\");\n"
    "flow.set_layer(\"train\");\n"
    "flow.set_module(\"ensmallen_layer\");\n"
    "flow.set_feature(\"Phase1ParamOptimizeTrain\");\n"
    "flow.input_dataset(\"dataset.cxcore.phase1.ensmallen\");\n"
    "flow.input_param(\"task_scope\",\"parameter_optimization_only\");\n"
    "flow.input_param(\"optimizer_name\",\"lbfgs\");\n"
    "flow.input_param(\"max_evals\",100);\n"
    "flow.input_param(\"patience\",10);\n"
    "flow.input_param(\"epsilon\",0.0005);\n"
    "flow.input_param(\"objective_ref\",\"torch.optimization.objective_ref\");\n"
    "flow.input_param(\"threshold_ref\",\"torch.optimization.threshold_ref\");\n"
    "flow.input_param(\"crop_policy_ref\",\"torch.optimization.crop_policy_ref\");\n"
    "flow.input_param(\"boundary_error_ref\",\"torch.optimization.boundary_error_ref\");\n"
    "flow.input_param(\"alignment_error_ref\",\"torch.optimization.alignment_error_ref\");\n"
    "flow.call(\"ResolvePhase1SampleBundle\");\n"
    "flow.call(\"EnsmallenAggregateBestParams\");\n"
    "flow.call(\"EnsmallenSaveBestParams\");\n"
    "flow.expect_output(\"MeasuredBatchOptimizeResult\");\n"
    "flow.expect_output(\"MeasuredReplayResult\");\n"
    "flow.expect_field(\"best_param_sets\");\n"
    "flow.expect_field(\"sample_count\");\n"
    "flow.expect_field(\"replay_log_path\");\n"
    "flow.finish();\n";

  if (!runtime.ExecuteScriptText("ensmallen_train_flow_host.cxs", script, result))
  {
    std::cerr << "[FAIL] ensmallen train flow host case summary=" << result.summary << "\n";
    return false;
  }

  if (!result.success ||
      result.degraded ||
      result.layer != "train" ||
      result.module != "ensmallen_layer" ||
      result.case_name != "phase1_param_opt" ||
      result.route != "ensmallen.flow_host" ||
      result.result_object != "MeasuredBatchOptimizeResult" ||
      result.optimize_summary_object != "MeasuredBatchOptimizeResult" ||
      result.replay_result_object != "MeasuredReplayResult" ||
      result.summary != "ensmallen batch optimize result ready")
  {
    std::cerr << "[FAIL] ensmallen train flow host context/result mismatch\n";
    return false;
  }

  bool saw_channel_line = false;
  bool saw_active_inputs_line = false;
  bool saw_reserved_inputs_line = false;
  bool saw_torch_input_refs_line = false;
  bool saw_convergence_line = false;
  bool saw_calls_line = false;
  bool saw_expect_line = false;
  bool saw_check_line = false;
  bool saw_objects_line = false;
  for (size_t i = 0; i < result.details.size(); ++i)
  {
    if (result.details[i].find("[ENSMALLEN_CHANNEL] phase1.batch_optimize_stage") != std::string::npos)
      saw_channel_line = true;
    if (result.details[i].find("[ENSMALLEN_ACTIVE_INPUTS] active_inputs=dataset_ref,split_ref,task_scope,optimizer_name,max_evals,patience,epsilon,objective_ref,threshold_ref,crop_policy_ref,boundary_error_ref,alignment_error_ref") != std::string::npos)
      saw_active_inputs_line = true;
    if (result.details[i].find("[ENSMALLEN_RESERVED_INPUTS] reserved_stage_outputs=best_param_sets,sample_count,replay_log_path,batch_best_params.json,batch_summary.json") != std::string::npos)
      saw_reserved_inputs_line = true;
    if (result.details[i].find("[ENSMALLEN_TORCH_INPUT_REFS] torch_optimization_inputs=objective_ref,threshold_ref,crop_policy_ref,boundary_error_ref,alignment_error_ref") != std::string::npos)
      saw_torch_input_refs_line = true;
    if (result.details[i].find("[ENSMALLEN_CONVERGENCE] status=converged tolerance=batch_optimize_ready") != std::string::npos)
      saw_convergence_line = true;
    if (result.details[i].find("[ENSMALLEN_CALLS] flow.call ResolvePhase1SampleBundle/EnsmallenAggregateBestParams/EnsmallenSaveBestParams") != std::string::npos)
      saw_calls_line = true;
    if (result.details[i].find("[ENSMALLEN_EXPECT] flow.expect_output(MeasuredBatchOptimizeResult,MeasuredReplayResult)/flow.expect_field") != std::string::npos)
      saw_expect_line = true;
    if (result.details[i].find("[ENSMALLEN_CHECK] flow.check_trace_contains/flow.check_artifact_exists/flow.check_scalar_eq/flow.check_scalar_ge") != std::string::npos)
      saw_check_line = true;
    if (result.details[i].find("[ENSMALLEN_OBJECTS] MeasuredBatchOptimizeResult->MeasuredReplayResult") != std::string::npos)
      saw_objects_line = true;
  }

  if (!saw_channel_line || !saw_active_inputs_line || !saw_reserved_inputs_line ||
      !saw_torch_input_refs_line || !saw_convergence_line || !saw_calls_line ||
      !saw_expect_line || !saw_check_line || !saw_objects_line)
  {
    std::cerr << "[FAIL] ensmallen train flow host marker missing\n";
    return false;
  }

  if (!HasNamedResultField(result, "inputs", "channel", "phase1.batch_optimize_stage") ||
      !HasNamedResultField(result, "inputs", "active_inputs", "dataset_ref,split_ref,task_scope,optimizer_name,max_evals,patience,epsilon,objective_ref,threshold_ref,crop_policy_ref,boundary_error_ref,alignment_error_ref") ||
      !HasNamedResultField(result, "inputs", "reserved_inputs", "best_param_sets,sample_count,replay_log_path,batch_best_params.json,batch_summary.json") ||
      !HasNamedResultField(result, "inputs", "torch_optimization_inputs", "objective_ref,threshold_ref,crop_policy_ref,boundary_error_ref,alignment_error_ref") ||
      !HasNamedResultField(result, "inputs", "dataset_bridge", "bridge.synthetic_phase1") ||
      !HasNamedResultField(result, "inputs", "test_bucket", "G0.baseline_stable,G1.tuning_target,G2.candidate_competition,G3.stress_boundary,G4.pipeline_bundle") ||
      !HasNamedResultField(result, "inputs", "test_flow", "bucket -> batch_optimize -> best_param_sets/sample_count -> summary_ref") ||
      !HasNamedResultField(result, "analysis", "bucket_focus", "G0.baseline_stable,G1.tuning_target,G2.candidate_competition,G3.stress_boundary,G4.pipeline_bundle") ||
      !HasNamedResultField(result, "analysis", "likely_issue_class", "boundary_degradation") ||
      !HasNamedResultField(result, "analysis", "next_bucket_focus", "G3.stress_boundary") ||
      !HasNamedResultField(result, "analysis", "observation_mode", "batch_param_stability") ||
      !HasNamedResultField(result, "analysis", "expansion_gate", "expand_next_bucket") ||
      !HasNamedResultField(result, "analysis", "bucket_coverage", "full_phase1_bucket_coverage") ||
      !HasNamedResultField(result, "analysis", "risk_axis", "boundary_and_roi") ||
      !HasNamedResultField(result, "analysis", "coverage_gap", "no_coverage_gap") ||
      !HasNamedResultField(result, "analysis", "observation_priority", "prioritize_boundary_roi_review") ||
      !HasNamedResultField(result, "analysis", "coverage_status", "coverage_ready_for_deeper_observation") ||
      !HasNamedResultField(result, "analysis", "next_review_action", "review_G3_boundary_roi_cases") ||
      !HasNamedResultField(result, "analysis", "optimization_signal", "improved_major_boundary_and_roi") ||
      !HasNamedResultField(result, "analysis", "bucket_review_template", "G0=baseline_guard;G1=param_tuning;G2=candidate_ordering;G3=boundary_roi;G4=pipeline_bundle") ||
      !HasNamedResultField(result, "analysis", "review_scope", "train_batch_best_param_review") ||
      !HasNamedResultField(result, "analysis", "primary_review_ref", "ensmallen_layer.train.phase1_param_opt.summary") ||
      !HasNamedResultField(result, "comparison", "comparison_status", "improved") ||
      !HasNamedResultField(result, "comparison", "comparison_magnitude", "major") ||
      !HasNamedResultField(result, "comparison", "primary_review_ref", "ensmallen_layer.train.phase1_param_opt.summary") ||
      !HasNamedResultField(result, "refs", "summary_ref", "ensmallen_layer.train.phase1_param_opt.summary") ||
      !HasNamedResultField(result, "refs", "replay_ref", "ensmallen_layer_phase1_param_opt_measured_replay.jsonl"))
  {
    std::cerr << "[FAIL] ensmallen train named result mismatch\n";
    return false;
  }

  return true;
}

bool RunEnsmallenInferFlowHostCase()
{
  cxparser_ext::ParserCxScriptRuntime runtime = MakeDebugRuntime();
  cxparser_ext::CxScriptExecutionResult result;

  const char *script =
    "flow=CxCoreFlowHost();\n"
    "flow.set_case(\"cxcore.ensmallen_layer.phase1_param_eval.infer\");\n"
    "flow.set_layer(\"infer\");\n"
    "flow.set_module(\"ensmallen_layer\");\n"
    "flow.set_feature(\"Phase1ParamEvalInfer\");\n"
    "flow.input_dataset(\"dataset.cxcore.phase1.ensmallen\");\n"
    "flow.input_artifact(\"best_params\",\"batch_best_params.json\");\n"
    "flow.input_param(\"compare_enable\",1);\n"
    "flow.input_param(\"baseline_only\",0);\n"
    "flow.input_param(\"objective_ref\",\"torch.optimization.objective_ref\");\n"
    "flow.input_param(\"threshold_ref\",\"torch.optimization.threshold_ref\");\n"
    "flow.input_param(\"crop_policy_ref\",\"torch.optimization.crop_policy_ref\");\n"
    "flow.input_param(\"boundary_error_ref\",\"torch.optimization.boundary_error_ref\");\n"
    "flow.input_param(\"alignment_error_ref\",\"torch.optimization.alignment_error_ref\");\n"
    "flow.call(\"LoadEnsmallenBestParams\");\n"
    "flow.call(\"EnsmallenInferCompare\");\n"
    "flow.expect_output(\"MeasuredOptimizeResult\");\n"
    "flow.expect_output(\"MeasuredInferCompareResult\");\n"
    "flow.expect_output(\"MeasuredReplayResult\");\n"
    "flow.expect_field(\"baseline_metrics\");\n"
    "flow.expect_field(\"optimized_metrics\");\n"
    "flow.expect_field(\"delta_metrics\");\n"
    "flow.finish();\n";

  if (!runtime.ExecuteScriptText("ensmallen_infer_flow_host.cxs", script, result))
  {
    std::cerr << "[FAIL] ensmallen infer flow host case summary=" << result.summary << "\n";
    return false;
  }

  if (!result.success ||
      result.degraded ||
      result.layer != "infer" ||
      result.module != "ensmallen_layer" ||
      result.case_name != "phase1_param_eval" ||
      result.route != "ensmallen.flow_host" ||
      result.result_object != "MeasuredInferCompareResult" ||
      result.compare_summary_object != "MeasuredInferCompareResult" ||
      result.replay_result_object != "MeasuredReplayResult" ||
      result.summary != "ensmallen infer compare result ready")
  {
    std::cerr << "[FAIL] ensmallen infer flow host context/result mismatch\n";
    return false;
  }

  bool saw_channel_line = false;
  bool saw_active_inputs_line = false;
  bool saw_reserved_inputs_line = false;
  bool saw_torch_input_refs_line = false;
  bool saw_convergence_line = false;
  bool saw_calls_line = false;
  bool saw_expect_line = false;
  bool saw_check_line = false;
  bool saw_objects_line = false;
  for (size_t i = 0; i < result.details.size(); ++i)
  {
    if (result.details[i].find("[ENSMALLEN_CHANNEL] phase1.infer_compare_stage") != std::string::npos)
      saw_channel_line = true;
    if (result.details[i].find("[ENSMALLEN_ACTIVE_INPUTS] active_inputs=dataset_ref,best_params_ref,compare_enable,baseline_only,objective_ref,threshold_ref,crop_policy_ref,boundary_error_ref,alignment_error_ref") != std::string::npos)
      saw_active_inputs_line = true;
    if (result.details[i].find("[ENSMALLEN_RESERVED_INPUTS] reserved_stage_outputs=baseline_metrics,optimized_metrics,delta_metrics,replay_log_path,baseline_report.json,optimized_report.json,infer_compare.json") != std::string::npos)
      saw_reserved_inputs_line = true;
    if (result.details[i].find("[ENSMALLEN_TORCH_INPUT_REFS] torch_optimization_inputs=objective_ref,threshold_ref,crop_policy_ref,boundary_error_ref,alignment_error_ref") != std::string::npos)
      saw_torch_input_refs_line = true;
    if (result.details[i].find("[ENSMALLEN_CONVERGENCE] status=converged tolerance=infer_compare_ready") != std::string::npos)
      saw_convergence_line = true;
    if (result.details[i].find("[ENSMALLEN_CALLS] flow.call LoadEnsmallenBestParams/EnsmallenInferCompare") != std::string::npos)
      saw_calls_line = true;
    if (result.details[i].find("[ENSMALLEN_EXPECT] flow.expect_output(MeasuredOptimizeResult,MeasuredInferCompareResult,MeasuredReplayResult)/flow.expect_field") != std::string::npos)
      saw_expect_line = true;
    if (result.details[i].find("[ENSMALLEN_CHECK] flow.check_trace_contains/flow.check_artifact_exists") != std::string::npos)
      saw_check_line = true;
    if (result.details[i].find("[ENSMALLEN_OBJECTS] MeasuredOptimizeResult->MeasuredInferCompareResult->MeasuredReplayResult") != std::string::npos)
      saw_objects_line = true;
  }

  if (!saw_channel_line || !saw_active_inputs_line || !saw_reserved_inputs_line ||
      !saw_torch_input_refs_line || !saw_convergence_line || !saw_calls_line ||
      !saw_expect_line || !saw_check_line || !saw_objects_line)
  {
    std::cerr << "[FAIL] ensmallen infer flow host marker missing\n";
    return false;
  }

  if (!HasNamedResultField(result, "inputs", "channel", "phase1.infer_compare_stage") ||
      !HasNamedResultField(result, "inputs", "active_inputs", "dataset_ref,best_params_ref,compare_enable,baseline_only,objective_ref,threshold_ref,crop_policy_ref,boundary_error_ref,alignment_error_ref") ||
      !HasNamedResultField(result, "inputs", "reserved_inputs", "baseline_metrics,optimized_metrics,delta_metrics,replay_log_path,baseline_report.json,optimized_report.json,infer_compare.json") ||
      !HasNamedResultField(result, "inputs", "torch_optimization_inputs", "objective_ref,threshold_ref,crop_policy_ref,boundary_error_ref,alignment_error_ref") ||
      !HasNamedResultField(result, "inputs", "dataset_bridge", "bridge.synthetic_phase1") ||
      !HasNamedResultField(result, "inputs", "test_bucket", "G0.baseline_stable,G1.tuning_target,G2.candidate_competition,G3.stress_boundary,G4.pipeline_bundle") ||
      !HasNamedResultField(result, "inputs", "test_flow", "bucket -> infer_compare -> baseline_metrics/delta_metrics -> compare_ref") ||
      !HasNamedResultField(result, "analysis", "bucket_focus", "G0.baseline_stable,G1.tuning_target,G2.candidate_competition,G3.stress_boundary,G4.pipeline_bundle") ||
      !HasNamedResultField(result, "analysis", "likely_issue_class", "boundary_degradation") ||
      !HasNamedResultField(result, "analysis", "next_bucket_focus", "G3.stress_boundary") ||
      !HasNamedResultField(result, "analysis", "observation_mode", "baseline_vs_optimized_validation") ||
      !HasNamedResultField(result, "analysis", "expansion_gate", "expand_next_bucket") ||
      !HasNamedResultField(result, "analysis", "bucket_coverage", "full_phase1_bucket_coverage") ||
      !HasNamedResultField(result, "analysis", "risk_axis", "boundary_and_roi") ||
      !HasNamedResultField(result, "analysis", "coverage_gap", "no_coverage_gap") ||
      !HasNamedResultField(result, "analysis", "observation_priority", "prioritize_boundary_roi_review") ||
      !HasNamedResultField(result, "analysis", "coverage_status", "coverage_ready_for_deeper_observation") ||
      !HasNamedResultField(result, "analysis", "next_review_action", "review_G3_boundary_roi_cases") ||
      !HasNamedResultField(result, "analysis", "optimization_signal", "improved_major_boundary_and_roi") ||
      !HasNamedResultField(result, "analysis", "bucket_review_template", "G0=baseline_guard;G1=param_tuning;G2=candidate_ordering;G3=boundary_roi;G4=pipeline_bundle") ||
      !HasNamedResultField(result, "analysis", "review_scope", "infer_baseline_vs_optimized_review") ||
      !HasNamedResultField(result, "analysis", "primary_review_ref", "ensmallen_layer.infer.phase1_param_eval.compare") ||
      !HasNamedResultField(result, "comparison", "comparison_status", "improved") ||
      !HasNamedResultField(result, "comparison", "comparison_magnitude", "major") ||
      !HasNamedResultField(result, "comparison", "primary_review_ref", "ensmallen_layer.infer.phase1_param_eval.compare") ||
      !HasNamedResultField(result, "refs", "summary_ref", "ensmallen_layer.infer.phase1_param_eval.summary") ||
      !HasNamedResultField(result, "refs", "compare_ref", "ensmallen_layer.infer.phase1_param_eval.compare") ||
      !HasNamedResultField(result, "refs", "replay_ref", "ensmallen_layer_phase1_param_eval_measured_replay.jsonl"))
  {
    std::cerr << "[FAIL] ensmallen infer named result mismatch\n";
    return false;
  }

  return true;
}

bool RunCompileReadResultPreviewCase()
{
  cxparser_ext::ParserCxScriptRuntime runtime = MakeDebugRuntime();
  cxparser_ext::CxScriptExecutionResult result;

  const char *script =
    "kind=module\n"
    "layer=feature\n"
    "module=cxcore\n"
    "case=circle_measurement_boundary\n"
    "mode=build-run\n"
    "step prepare {\n"
    "compile(\"sampling\");\n"
    "print(readresult(\"sampling.status\"));\n"
    "check(readresult(\"sampling.status\") == \"compiled\");\n"
    "}\n";

  if (!runtime.BuildExecutionPreview("compile_readresult_preview.cxs", script, result))
  {
    std::cerr << "[FAIL] compile/readresult preview summary=" << result.summary << "\n";
    return false;
  }

  if (!HasCompiledStage(result, "sampling") ||
      !HasNamedResultField(result, "sampling", "status", "compiled") ||
      result.debug_view.named_results.empty() ||
      result.debug_view.result_fields.empty() ||
      !HasExecutionStepKind(result.debug_view.execution_steps, cxparser_ext::cxsesk_compile) ||
      result.execution_summary.compile_step_count < 1)
  {
    std::cerr << "[FAIL] compile/readresult preview semantics mismatch\n";
    return false;
  }

  return true;
}

bool RunNamedResultRuntimeCase()
{
  cxparser_ext::ParserCxScriptRuntime runtime = MakeDebugRuntime();
  cxparser_ext::CxScriptExecutionResult result;

  const char *script =
    "kind=module\n"
    "layer=feature\n"
    "module=cxcore\n"
    "case=circle_measurement_boundary\n"
    "mode=build-run\n"
    "step check {\n"
    "check(readresult(\"output.failure_mode\") == \"handled_boundary_condition\");\n"
    "print(readresult(\"output.failure_stage\"));\n"
    "}\n";

  if (!runtime.ExecuteScriptText("named_result_runtime.cxs", script, result))
  {
    std::cerr << "[FAIL] named result runtime summary=" << result.summary << "\n";
    return false;
  }

  if (!HasNamedResultField(result, "output", "failure_mode", "handled_boundary_condition") ||
      result.debug_view.named_results.empty() ||
      result.debug_view.result_fields.empty() ||
      !HasExecutionStepKind(result.debug_view.execution_steps, cxparser_ext::cxsesk_check) ||
      result.failure_mode != "handled_boundary_condition")
  {
    std::cerr << "[FAIL] named result runtime semantics mismatch\n";
    return false;
  }

  return true;
}

bool RunMultiVariableReadCase()
{
  cxparser_ext::ParserCxScriptRuntime runtime = MakeDebugRuntime();
  cxparser_ext::CxScriptExecutionResult result;

  const char *script =
    "kind=module\n"
    "layer=feature\n"
    "module=cxcore\n"
    "case=circle_measurement_boundary\n"
    "mode=build-run\n"
    "step check {\n"
    "string mode = readresult(\"output.failure_mode\");\n"
    "string stage = readresult(\"output.failure_stage\");\n"
    "mode_copy = mode;\n"
    "check(mode == \"handled_boundary_condition\");\n"
    "check(mode_copy == \"handled_boundary_condition\");\n"
    "print(stage);\n"
    "}\n";

  if (!runtime.ExecuteScriptText("multi_variable_read_runtime.cxs", script, result))
  {
    std::cerr << "[FAIL] multi variable runtime summary=" << result.summary << "\n";
    return false;
  }

  bool saw_mode_var = false;
  bool saw_stage_var = false;
  bool saw_assignment = false;
  bool saw_two_checks = false;
  for (size_t i = 0; i < result.details.size(); ++i)
  {
    if (result.details[i].find("mode=handled_boundary_condition") != std::string::npos)
      saw_mode_var = true;
    if (result.details[i].find("stage=") != std::string::npos)
      saw_stage_var = true;
    if (result.details[i].find("mode_copy=handled_boundary_condition") != std::string::npos)
      saw_assignment = true;
  }
  saw_two_checks = result.debug_view.execution_summary.check_step_count >= 2;

  if (!saw_mode_var || !saw_stage_var || !saw_assignment || !saw_two_checks)
  {
    std::cerr << "[FAIL] multi variable read/step check semantics mismatch\n";
    return false;
  }

  return true;
}
}

int main()
{
  if (!RunHeaderMetadataFunctionCase())
    return 1;
  if (!RunHeaderMetadataLegacyCase())
    return 1;

  if (!RunSuccessCase())
    return 1;
  if (!RunUnknownTypeCase())
    return 1;
  if (!RunUnterminatedBlockCase())
    return 1;
  if (!RunCStyleTraceCase())
    return 1;
  if (!RunEnsmallenFlowHostCase())
    return 1;
  if (!RunEnsmallenMatchScoreFlowHostCase())
    return 1;
  if (!RunEnsmallenScenarioFlowHostCase())
    return 1;
  if (!RunEnsmallenTrainFlowHostCase())
    return 1;
  if (!RunEnsmallenInferFlowHostCase())
    return 1;
  if (!RunCompileReadResultPreviewCase())
    return 1;
  if (!RunNamedResultRuntimeCase())
    return 1;
  if (!RunMultiVariableReadCase())
    return 1;

  std::cout << "[PASS] parser_cxscript_debug_smoke\n";
  return 0;
}
