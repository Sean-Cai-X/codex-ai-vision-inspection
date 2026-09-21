#ifndef CXPARSER_EXT_PARSER_CXSCRIPT_RUNTIME_RESULT_HELPERS_H
#define CXPARSER_EXT_PARSER_CXSCRIPT_RUNTIME_RESULT_HELPERS_H

#include <cstddef>
#include <vector>

#include "parser_cxscript_types.h"

namespace cxparser_ext
{
namespace runtime_result_detail
{
inline void RefreshExecutionDiagnostics(CxScriptExecutionResult &result)
{
  if (!result.execution_steps.empty())
  {
    const CxScriptExecutionStepView &last_step = result.execution_steps.back();
    result.last_step_id = last_step.step_id;
    result.last_frame_id = last_step.frame_id;
    result.last_sequence = last_step.sequence;
    result.last_source_line = last_step.span.line_begin;
  }
  else if (!result.source_map.empty())
  {
    const CxScriptSourceMapEntry &last_source = result.source_map.back();
    result.last_step_id = last_source.step_id;
    result.last_frame_id = last_source.frame_id;
    result.last_source_line = last_source.span.line_begin;
  }

  if (result.parse_error.line > 0)
  {
    result.failure_line = result.parse_error.line;
    if (result.failure_phase.empty())
      result.failure_phase = "parse";
  }

  if (!result.success)
  {
    if (result.failure_step_id == 0)
      result.failure_step_id = result.last_step_id;
    if (result.failure_frame_id == 0)
      result.failure_frame_id = result.last_frame_id;
    if (result.failure_sequence == 0)
      result.failure_sequence = result.last_sequence;
    if (result.failure_line == 0)
      result.failure_line = result.last_source_line;
    if (result.failure_phase.empty())
      result.failure_phase = "execute";
  }
}

inline void BindVariableExecutionIds(CxScriptExecutionResult &result)
{
  for (size_t variable_index = 0; variable_index < result.variables.size(); ++variable_index)
  {
    CxScriptVariableDecl &variable = result.variables[variable_index];
    for (size_t source_index = 0; source_index < result.source_map.size(); ++source_index)
    {
      const CxScriptSourceMapEntry &entry = result.source_map[source_index];
      if (entry.step_name != variable.step_name ||
          entry.span.line_begin != variable.span.line_begin)
        continue;

      variable.step_id = entry.step_id;
      variable.frame_id = entry.frame_id;
      break;
    }
  }
}

inline void RefreshExecutionSummary(CxScriptExecutionResult &result)
{
  result.execution_summary = CxScriptExecutionSummary();
  result.execution_summary.step_count = static_cast<int>(result.execution_steps.size());
  result.execution_summary.replay_frame_count = static_cast<int>(result.replay_frames.size());
  result.execution_summary.source_entry_count = static_cast<int>(result.source_map.size());

  for (size_t index = 0; index < result.execution_steps.size(); ++index)
  {
    const CxScriptExecutionStepView &step = result.execution_steps[index];
    if (step.step_id > result.execution_summary.max_step_id)
      result.execution_summary.max_step_id = step.step_id;
    if (step.frame_id > result.execution_summary.max_frame_id)
      result.execution_summary.max_frame_id = step.frame_id;
    if (step.sequence > result.execution_summary.max_sequence)
      result.execution_summary.max_sequence = step.sequence;
    if (step.block_depth > result.execution_summary.max_block_depth)
      result.execution_summary.max_block_depth = step.block_depth;
    if (result.execution_summary.entry_step_id == 0 && step.kind == cxsesk_step)
      result.execution_summary.entry_step_id = step.step_id;
    if (result.execution_summary.check_step_id == 0 &&
        step.kind == cxsesk_step &&
        step.step_name == "check")
      result.execution_summary.check_step_id = step.step_id;

    switch (step.kind)
    {
    case cxsesk_header_metadata:
      ++result.execution_summary.header_step_count;
      break;
    case cxsesk_frame_enter:
    case cxsesk_frame_exit:
      ++result.execution_summary.frame_step_count;
      break;
    case cxsesk_call:
      ++result.execution_summary.call_step_count;
      break;
    case cxsesk_compile:
      ++result.execution_summary.compile_step_count;
      break;
    case cxsesk_check:
      ++result.execution_summary.check_step_count;
      break;
    case cxsesk_print:
      ++result.execution_summary.print_step_count;
      break;
    case cxsesk_breakpoint:
      ++result.execution_summary.breakpoint_step_count;
      break;
    case cxsesk_checkpoint:
      ++result.execution_summary.checkpoint_step_count;
      break;
    case cxsesk_unknown:
    case cxsesk_step:
    case cxsesk_type_decl:
    case cxsesk_type_use:
    case cxsesk_var_decl:
    case cxsesk_input:
    case cxsesk_action:
    default:
      break;
    }
  }

  RefreshExecutionDiagnostics(result);
}

inline void ReleaseLightweightDebugArtifacts(CxScriptExecutionResult &result)
{
  std::vector<CxScriptStepTrace>().swap(result.step_traces);
  std::vector<CxScriptSourceMapEntry>().swap(result.source_map);
  std::vector<CxScriptCheckpointRecord>().swap(result.checkpoints);
  std::vector<CxScriptBreakpointRecord>().swap(result.breakpoints);
  std::vector<CxScriptExecutionOp>().swap(result.execution_ops);
  std::vector<CxScriptReplayFrame>().swap(result.replay_frames);
  std::vector<CxScriptExecutionStepView>().swap(result.execution_steps);
  std::vector<CxScriptVariableDecl>().swap(result.variables);
  result.debug_view = CxScriptDebugView();
}
}
}

#endif
