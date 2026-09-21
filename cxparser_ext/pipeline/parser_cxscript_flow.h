#ifndef CXPARSER_EXT_PARSER_CXSCRIPT_FLOW_H
#define CXPARSER_EXT_PARSER_CXSCRIPT_FLOW_H

#include <string>
#include <vector>

#include "parser_cxscript_types.h"

namespace cxparser_ext
{
enum CxScriptStmtKind
{
  cxssk_unknown,
  cxssk_header_metadata,
  cxssk_step,
  cxssk_block_boundary,
  cxssk_type_decl,
  cxssk_use,
  cxssk_var_decl,
  cxssk_input,
  cxssk_call,
  cxssk_compile,
  cxssk_action,
  cxssk_expect,
  cxssk_emit,
  cxssk_breakpoint,
  cxssk_checkpoint
};

struct CxScriptStatement
{
  int step_id = 0;
  int frame_id = 0;
  CxScriptStmtKind kind = cxssk_unknown;
  std::string name;
  std::string step_name;
  std::string text;
  std::string callee_name;
  std::string argument_text;
  std::string declared_type;
  std::string initializer_text;
  std::string lhs_object_name;
  std::string lhs_type_name;
  std::string source_object_name;
  std::string method_name;
  std::string return_object_ref;
  std::string lhs_text;
  std::string operator_text;
  std::string rhs_text;
  CxScriptSourceSpan span;
  int block_depth = 0;
  bool returns_object_assignment = false;
};

struct CxScriptFlow
{
  std::vector<CxScriptStatement> statements;
  std::vector<CxScriptTypeSpec> declared_types;
  std::vector<CxScriptVariableDecl> variables;
};
}

#endif
