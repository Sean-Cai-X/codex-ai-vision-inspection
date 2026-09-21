#include "cxscript_debug_log_builder.h"

#include <sstream>

namespace cxparser_ext {
namespace debug {

std::string BuildCxScriptDebugRawLog(const EmbeddedDebugRunResult& result)
{
  std::ostringstream out;
  out << "[CXPARSER_EXT_DEBUG] status=" << result.status
      << " ok=" << (result.ok ? "true" : "false")
      << " reason=" << result.reason << "\n";

  for (const EmbeddedDebugLineView& line : result.line_views)
  {
    out << "[LINE] line=" << line.line_no
        << " type=" << line.statement_type
        << " status=" << line.status
        << " source=" << line.source_line << "\n";
  }

  for (const EmbeddedDebugStatementView& stmt : result.statement_views)
  {
    out << "[STATEMENT] id=" << stmt.statement_id
        << " line=" << stmt.line_no
        << " type=" << stmt.statement_type;
    if (!stmt.lhs_variable.empty())
      out << " lhs=" << stmt.lhs_variable << ":" << stmt.lhs_type;
    if (!stmt.source_object.empty())
      out << " call=" << stmt.source_object << "." << stmt.method_name;
    if (!stmt.returned_object_ref.empty())
      out << " return=" << stmt.returned_object_ref;
    out << " status=" << stmt.status << "\n";
  }

  for (const EmbeddedDebugObjectAssignment& item : result.object_assignments)
  {
    out << "[OBJECT_ASSIGNMENT] line=" << item.line_no
        << " lhs=" << item.lhs_variable << ":" << item.lhs_type
        << " from=" << item.source_object << "." << item.method_name
        << " ref=" << item.returned_object_ref
        << " status=" << item.status << "\n";
  }

  for (const EmbeddedDebugMethodCall& call : result.method_calls)
  {
    out << "[METHOD_CALL] line=" << call.line_no
        << " call=" << call.source_object << "." << call.method_name
        << " output=" << call.output_ref
        << " status=" << call.status << "\n";
  }

  for (const EmbeddedDebugReturnObject& ret : result.return_objects)
  {
    out << "[RETURN_OBJECT] line=" << ret.line_no
        << " ref=" << ret.returned_object_ref
        << " type=" << ret.returned_type
        << " from=" << ret.source_object << "." << ret.method_name
        << " status=" << ret.status << "\n";
  }

  return out.str();
}

}  // namespace debug
}  // namespace cxparser_ext
