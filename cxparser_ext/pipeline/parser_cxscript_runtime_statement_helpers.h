#ifndef CXPARSER_EXT_PARSER_CXSCRIPT_RUNTIME_STATEMENT_HELPERS_H
#define CXPARSER_EXT_PARSER_CXSCRIPT_RUNTIME_STATEMENT_HELPERS_H

#include <cstddef>
#include <string>

#include "parser_cxscript_flow.h"
#include "parser_cxscript_types.h"

namespace cxparser_ext
{
namespace runtime_statement_detail
{
inline std::string StatementKindName(CxScriptStmtKind kind)
{
  switch (kind)
  {
  case cxssk_header_metadata:
    return "header_metadata";
  case cxssk_step:
    return "step";
  case cxssk_block_boundary:
    return "block";
  case cxssk_type_decl:
    return "type";
  case cxssk_use:
    return "use";
  case cxssk_var_decl:
    return "var_decl";
  case cxssk_input:
    return "input";
  case cxssk_call:
    return "call";
  case cxssk_compile:
    return "compile";
  case cxssk_action:
    return "action";
  case cxssk_expect:
    return "expect";
  case cxssk_emit:
    return "emit";
  case cxssk_breakpoint:
    return "breakpoint";
  case cxssk_checkpoint:
    return "checkpoint";
  case cxssk_unknown:
  default:
    return "unknown";
  }
}

inline CxScriptExecutionOpcode StatementOpcode(CxScriptStmtKind kind)
{
  switch (kind)
  {
  case cxssk_header_metadata:
    return cxseo_header_metadata;
  case cxssk_step:
    return cxseo_step_enter;
  case cxssk_block_boundary:
    return cxseo_block;
  case cxssk_type_decl:
    return cxseo_type_decl;
  case cxssk_use:
    return cxseo_type_use;
  case cxssk_var_decl:
    return cxseo_var_decl;
  case cxssk_input:
    return cxseo_input;
  case cxssk_call:
    return cxseo_call;
  case cxssk_compile:
    return cxseo_compile;
  case cxssk_action:
    return cxseo_action;
  case cxssk_expect:
    return cxseo_expect;
  case cxssk_emit:
    return cxseo_emit;
  case cxssk_breakpoint:
    return cxseo_breakpoint;
  case cxssk_checkpoint:
    return cxseo_checkpoint;
  case cxssk_unknown:
  default:
    return cxseo_unknown;
  }
}

inline std::string StatementActionLabel(const CxScriptStatement &stmt)
{
  if (stmt.kind == cxssk_expect && stmt.name == "check")
    return "check";
  if (stmt.kind == cxssk_emit && stmt.name == "print")
    return "print";
  if (stmt.kind == cxssk_compile)
    return "compile";
  if (stmt.kind == cxssk_block_boundary)
  {
    if (stmt.text.find('{') != std::string::npos)
      return "frame_enter";
    if (stmt.text.find('}') != std::string::npos)
      return "frame_exit";
  }
  return StatementKindName(stmt.kind);
}

inline CxScriptExecutionStepKind StatementStepKind(const CxScriptStatement &stmt,
                                                   const std::string &action_label)
{
  switch (stmt.kind)
  {
  case cxssk_header_metadata:
    return cxsesk_header_metadata;
  case cxssk_step:
    return cxsesk_step;
  case cxssk_block_boundary:
    if (action_label == "frame_enter")
      return cxsesk_frame_enter;
    if (action_label == "frame_exit")
      return cxsesk_frame_exit;
    return cxsesk_unknown;
  case cxssk_type_decl:
    return cxsesk_type_decl;
  case cxssk_use:
    return cxsesk_type_use;
  case cxssk_var_decl:
    return cxsesk_var_decl;
  case cxssk_input:
    return cxsesk_input;
  case cxssk_call:
    return cxsesk_call;
  case cxssk_compile:
    return cxsesk_compile;
  case cxssk_action:
    return cxsesk_action;
  case cxssk_expect:
    if (action_label == "check")
      return cxsesk_check;
    return cxsesk_unknown;
  case cxssk_emit:
    if (action_label == "print")
      return cxsesk_print;
    return cxsesk_unknown;
  case cxssk_breakpoint:
    return cxsesk_breakpoint;
  case cxssk_checkpoint:
    return cxsesk_checkpoint;
  case cxssk_unknown:
  default:
    return cxsesk_unknown;
  }
}

inline std::string StatementControlTag(const CxScriptStatement &stmt,
                                       const std::string &action_label)
{
  if (stmt.kind == cxssk_header_metadata)
    return "header_metadata";
  if (stmt.kind == cxssk_step)
    return "step_enter";
  if (stmt.kind == cxssk_block_boundary)
    return action_label;
  if (stmt.kind == cxssk_call)
    return "call";
  if (stmt.kind == cxssk_compile)
    return "compile";
  if (stmt.kind == cxssk_expect && action_label == "check")
    return "check";
  if (stmt.kind == cxssk_emit && action_label == "print")
    return "print";
  if (stmt.kind == cxssk_breakpoint)
    return "breakpoint";
  if (stmt.kind == cxssk_checkpoint)
    return "checkpoint";
  return "linear";
}

inline void SetStatementError(const CxScriptStatement &stmt,
                              const std::string &message,
                              CxScriptExecutionResult &result,
                              std::string &error_message)
{
  error_message = message;
  result.parse_error.message = message;
  result.parse_error.token = stmt.text;
  result.parse_error.line = stmt.span.line_begin;
  result.parse_error.column = stmt.span.column_begin;
  result.parse_error.block_depth = stmt.block_depth;
  result.parse_error.step_name = stmt.step_name;
}
}
}

#endif
