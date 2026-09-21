#ifndef CXPARSER_EXT_DEBUG_CXSCRIPT_DEBUG_RESULT_H
#define CXPARSER_EXT_DEBUG_CXSCRIPT_DEBUG_RESULT_H

#include <map>
#include <string>
#include <vector>

namespace cxparser_ext {
namespace debug {

struct EmbeddedDebugLineView
{
  int line_no = 0;
  std::string source_line;
  std::string normalized_statement;
  std::string statement_type;
  std::string status;
  std::string reason;
};

struct EmbeddedDebugStatementView
{
  int statement_id = 0;
  int line_no = 0;
  std::string statement_type;
  std::string lhs_variable;
  std::string lhs_type;
  std::string source_object;
  std::string method_name;
  std::vector<std::string> argument_refs;
  std::string returned_object_ref;
  std::string status;
  std::string reason;
};

struct EmbeddedDebugObjectAssignment
{
  std::string lhs_variable;
  std::string lhs_type;
  std::string source_object;
  std::string method_name;
  std::string returned_object_ref;
  std::string source_line;
  int line_no = 0;
  std::string status;
  std::string reason;
};

struct EmbeddedDebugMethodCall
{
  std::string source_object;
  std::string method_name;
  std::vector<std::string> input_refs;
  std::string output_ref;
  std::string source_line;
  int line_no = 0;
  std::string status;
  std::string reason;
};

struct EmbeddedDebugReturnObject
{
  std::string returned_object_ref;
  std::string returned_type;
  std::string source_object;
  std::string method_name;
  int line_no = 0;
  std::string status;
  std::string reason;
};

struct EmbeddedDebugVariableSnapshot
{
  std::string variable_name;
  std::string variable_type;
  std::string value_ref;
  std::string status;
};

struct EmbeddedDebugRuntimeObjectSnapshot
{
  std::string object_name;
  std::string object_type;
  std::string object_ref;
  std::string lifecycle_state;
  std::string status;
  std::string reason;
};

struct EmbeddedDebugRunRequest
{
  std::string script_path;
  std::string script_text;
  std::string working_directory;
  bool capture_structured_log = true;
  bool enable_line_view = true;
  bool enable_statement_view = true;
  bool enable_object_assignment = true;
  bool enable_method_trace = true;
  bool enable_return_object_trace = true;
  bool enable_runtime_snapshot = true;
  bool enable_variable_snapshot = true;
};

struct EmbeddedDebugRunResult
{
  bool ok = false;
  std::string status;
  std::string reason;
  std::string raw_log;
  std::vector<EmbeddedDebugLineView> line_views;
  std::vector<EmbeddedDebugStatementView> statement_views;
  std::vector<EmbeddedDebugObjectAssignment> object_assignments;
  std::vector<EmbeddedDebugMethodCall> method_calls;
  std::vector<EmbeddedDebugReturnObject> return_objects;
  std::vector<EmbeddedDebugRuntimeObjectSnapshot> runtime_objects;
  std::vector<EmbeddedDebugVariableSnapshot> variables;
  std::map<std::string, std::string> refs;
  std::map<std::string, std::string> inputs;
  std::map<std::string, std::string> outputs;
};

}  // namespace debug
}  // namespace cxparser_ext

#endif
