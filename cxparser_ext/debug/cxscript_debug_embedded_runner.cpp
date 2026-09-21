#include "cxscript_debug_embedded_runner.h"

#include "cxscript_debug_log_builder.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace cxparser_ext {
namespace debug {
namespace {

std::string Trim(const std::string& text)
{
  std::size_t begin = 0;
  while (begin < text.size() &&
         std::isspace(static_cast<unsigned char>(text[begin])))
    ++begin;

  std::size_t end = text.size();
  while (end > begin &&
         std::isspace(static_cast<unsigned char>(text[end - 1])))
    --end;

  return text.substr(begin, end - begin);
}

bool IsIdentifierChar(char ch)
{
  return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_';
}

bool IsIdentifier(const std::string& text)
{
  if (text.empty())
    return false;
  if (!std::isalpha(static_cast<unsigned char>(text[0])) && text[0] != '_')
    return false;
  for (char ch : text)
  {
    if (!IsIdentifierChar(ch))
      return false;
  }
  return true;
}

bool IsIgnoredStatement(const std::string& statement)
{
  return statement.empty() ||
         statement == "{" ||
         statement == "}" ||
         statement == "};" ||
         statement == "else" ||
         statement.find("//") == 0 ||
         statement.find("/*") == 0 ||
         statement.find("*") == 0 ||
         statement.find("*/") == 0;
}

std::string StripTrailingSemicolon(std::string statement)
{
  statement = Trim(statement);
  if (!statement.empty() && statement.back() == ';')
    statement.pop_back();
  return Trim(statement);
}

std::string RemoveInlineComment(const std::string& line)
{
  const std::size_t comment = line.find("//");
  if (comment == std::string::npos)
    return line;
  return line.substr(0, comment);
}

std::vector<std::string> SplitArguments(const std::string& args)
{
  std::vector<std::string> result;
  std::string current;
  int paren_depth = 0;
  bool in_string = false;

  for (std::size_t i = 0; i < args.size(); ++i)
  {
    const char ch = args[i];
    if (ch == '"' && (i == 0 || args[i - 1] != '\\'))
      in_string = !in_string;
    if (!in_string)
    {
      if (ch == '(')
        ++paren_depth;
      else if (ch == ')' && paren_depth > 0)
        --paren_depth;
      else if (ch == ',' && paren_depth == 0)
      {
        const std::string item = Trim(current);
        if (!item.empty())
          result.push_back(item);
        current.clear();
        continue;
      }
    }
    current.push_back(ch);
  }

  const std::string tail = Trim(current);
  if (!tail.empty())
    result.push_back(tail);
  return result;
}

bool ParseMethodExpression(const std::string& expression,
                           std::string& source_object,
                           std::string& method_name,
                           std::vector<std::string>& args)
{
  source_object.clear();
  method_name.clear();
  args.clear();

  const std::size_t open = expression.find('(');
  const std::size_t close = expression.rfind(')');
  if (open == std::string::npos || close == std::string::npos || close < open)
    return false;

  const std::string callable = Trim(expression.substr(0, open));
  const std::size_t dot = callable.rfind('.');
  if (dot == std::string::npos)
    return false;

  source_object = Trim(callable.substr(0, dot));
  method_name = Trim(callable.substr(dot + 1));
  args = SplitArguments(expression.substr(open + 1, close - open - 1));

  return IsIdentifier(source_object) && IsIdentifier(method_name);
}

bool ParseDeclaration(const std::string& statement,
                      std::string& type,
                      std::string& name)
{
  type.clear();
  name.clear();
  const std::string text = StripTrailingSemicolon(statement);
  if (text.find('(') != std::string::npos || text.find('=') != std::string::npos)
    return false;

  std::istringstream in(text);
  std::string extra;
  in >> type >> name >> extra;
  if (!extra.empty())
    return false;
  return IsIdentifier(type) && IsIdentifier(name) &&
         type != "if" && type != "else" && type != "return";
}

bool ParseObjectAssignment(const std::string& statement,
                           std::string& lhs_type,
                           std::string& lhs_variable,
                           std::string& source_object,
                           std::string& method_name,
                           std::vector<std::string>& args)
{
  lhs_type.clear();
  lhs_variable.clear();
  source_object.clear();
  method_name.clear();
  args.clear();

  const std::string text = StripTrailingSemicolon(statement);
  const std::size_t equal = text.find('=');
  if (equal == std::string::npos)
    return false;

  std::istringstream lhs_stream(Trim(text.substr(0, equal)));
  std::string extra;
  lhs_stream >> lhs_type >> lhs_variable >> extra;
  if (!extra.empty() || !IsIdentifier(lhs_type) || !IsIdentifier(lhs_variable))
    return false;

  return ParseMethodExpression(Trim(text.substr(equal + 1)),
                               source_object,
                               method_name,
                               args);
}

bool LoadScriptText(const EmbeddedDebugRunRequest& request,
                    std::string& text,
                    std::string& reason)
{
  if (!request.script_text.empty())
  {
    text = request.script_text;
    return true;
  }

  if (request.script_path.empty())
  {
    reason = "script_path and script_text are empty";
    return false;
  }

  std::ifstream input(request.script_path.c_str(), std::ios::in | std::ios::binary);
  if (!input.is_open())
  {
    reason = "failed to open script: " + request.script_path;
    return false;
  }

  std::ostringstream buffer;
  buffer << input.rdbuf();
  text = buffer.str();
  return true;
}

void AddRuntimeObject(EmbeddedDebugRunResult& result,
                      const std::string& name,
                      const std::string& type,
                      const std::string& object_ref,
                      const std::string& state,
                      const std::string& status,
                      const std::string& reason)
{
  EmbeddedDebugRuntimeObjectSnapshot object;
  object.object_name = name;
  object.object_type = type;
  object.object_ref = object_ref;
  object.lifecycle_state = state;
  object.status = status;
  object.reason = reason;
  result.runtime_objects.push_back(object);
}

}  // namespace

EmbeddedDebugRunResult RunCxScriptDebugEmbedded(
  const EmbeddedDebugRunRequest& request)
{
  EmbeddedDebugRunResult result;
  std::string script_text;
  if (!LoadScriptText(request, script_text, result.reason))
  {
    result.ok = false;
    result.status = "load_failed";
    result.raw_log = BuildCxScriptDebugRawLog(result);
    return result;
  }

  std::istringstream input(script_text);
  std::string raw_line;
  int line_no = 0;
  int statement_id = 0;

  while (std::getline(input, raw_line))
  {
    ++line_no;
    const std::string source_line = raw_line;
    const std::string statement = StripTrailingSemicolon(
      RemoveInlineComment(raw_line));
    if (IsIgnoredStatement(statement))
      continue;

    EmbeddedDebugLineView line_view;
    line_view.line_no = line_no;
    line_view.source_line = source_line;
    line_view.normalized_statement = statement;
    line_view.status = "observed";
    line_view.reason = "source parsed by cxparser_ext debug layer";

    std::string type;
    std::string name;
    std::string source_object;
    std::string method_name;
    std::vector<std::string> args;

    if (ParseObjectAssignment(statement,
                              type,
                              name,
                              source_object,
                              method_name,
                              args))
    {
      const std::string returned_ref = name + "<=" + source_object + "." + method_name;
      line_view.statement_type = "object_assignment";

      EmbeddedDebugStatementView stmt;
      stmt.statement_id = ++statement_id;
      stmt.line_no = line_no;
      stmt.statement_type = "object_assignment";
      stmt.lhs_variable = name;
      stmt.lhs_type = type;
      stmt.source_object = source_object;
      stmt.method_name = method_name;
      stmt.argument_refs = args;
      stmt.returned_object_ref = returned_ref;
      stmt.status = "produced";
      stmt.reason = "recognized Class lhs = source.method(args)";
      result.statement_views.push_back(stmt);

      EmbeddedDebugObjectAssignment assignment;
      assignment.lhs_variable = name;
      assignment.lhs_type = type;
      assignment.source_object = source_object;
      assignment.method_name = method_name;
      assignment.returned_object_ref = returned_ref;
      assignment.source_line = source_line;
      assignment.line_no = line_no;
      assignment.status = "produced";
      assignment.reason = "object assignment observed";
      result.object_assignments.push_back(assignment);

      EmbeddedDebugMethodCall call;
      call.source_object = source_object;
      call.method_name = method_name;
      call.input_refs = args;
      call.output_ref = returned_ref;
      call.source_line = source_line;
      call.line_no = line_no;
      call.status = "observed";
      call.reason = "method call observed through object assignment";
      result.method_calls.push_back(call);

      EmbeddedDebugReturnObject ret;
      ret.returned_object_ref = returned_ref;
      ret.returned_type = type;
      ret.source_object = source_object;
      ret.method_name = method_name;
      ret.line_no = line_no;
      ret.status = "produced";
      ret.reason = "return object observed";
      result.return_objects.push_back(ret);

      result.refs[name] = returned_ref;
      result.outputs[name] = returned_ref;
      AddRuntimeObject(result, name, type, returned_ref,
                       "cxparser_ext_debug_assigned",
                       "produced",
                       "object assigned from method return");
    }
    else if (ParseDeclaration(statement, type, name))
    {
      line_view.statement_type = "object_declaration";

      EmbeddedDebugStatementView stmt;
      stmt.statement_id = ++statement_id;
      stmt.line_no = line_no;
      stmt.statement_type = "object_declaration";
      stmt.lhs_variable = name;
      stmt.lhs_type = type;
      stmt.status = "observed";
      stmt.reason = "recognized object declaration";
      result.statement_views.push_back(stmt);

      AddRuntimeObject(result, name, type, name,
                       "cxparser_ext_debug_declared",
                       "observed",
                       "object declaration observed");
    }
    else if (ParseMethodExpression(statement, source_object, method_name, args))
    {
      line_view.statement_type = "method_call";

      EmbeddedDebugStatementView stmt;
      stmt.statement_id = ++statement_id;
      stmt.line_no = line_no;
      stmt.statement_type = "method_call";
      stmt.source_object = source_object;
      stmt.method_name = method_name;
      stmt.argument_refs = args;
      stmt.status = "observed";
      stmt.reason = "recognized source.method(args)";
      result.statement_views.push_back(stmt);

      EmbeddedDebugMethodCall call;
      call.source_object = source_object;
      call.method_name = method_name;
      call.input_refs = args;
      call.source_line = source_line;
      call.line_no = line_no;
      call.status = "observed";
      call.reason = "method call observed";
      result.method_calls.push_back(call);
    }
    else
    {
      line_view.statement_type = "semantic_operation";
      line_view.status = "observed_unclassified";
      line_view.reason = "not an object declaration, method call, or object assignment";

      EmbeddedDebugStatementView stmt;
      stmt.statement_id = ++statement_id;
      stmt.line_no = line_no;
      stmt.statement_type = "semantic_operation";
      stmt.status = "observed_unclassified";
      stmt.reason = line_view.reason;
      result.statement_views.push_back(stmt);
    }

    result.line_views.push_back(line_view);
  }

  result.ok = true;
  result.status = "debug_parse_ok";
  result.reason = "cxparser_ext debug layer parsed script source";
  result.raw_log = BuildCxScriptDebugRawLog(result);
  return result;
}

}  // namespace debug
}  // namespace cxparser_ext
