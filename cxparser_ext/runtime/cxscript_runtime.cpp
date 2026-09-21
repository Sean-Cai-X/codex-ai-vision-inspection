#include "cxscript_runtime.h"

#include <cctype>
#include <fstream>
#include <sstream>

#include "cxscript_c_frontend.h"

namespace cxparser_ext
{
bool ExtractCxscriptHeaderMetadata(const std::string &script_text,
                                   CxscriptHeaderMetadata &metadata);

namespace
{
std::string TrimCopy(const std::string &text)
{
  size_t begin = 0;
  while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])))
    ++begin;

  size_t end = text.size();
  while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])))
    --end;

  return text.substr(begin, end - begin);
}

bool StartsWithTrimmed(const std::string &text, const char *prefix)
{
  size_t begin = 0;
  while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])))
    ++begin;

  const std::string key(prefix);
  return text.size() >= begin + key.size() &&
         text.compare(begin, key.size(), key) == 0;
}

bool LooksLikeMemberCall(const std::string &text)
{
  bool in_string = false;
  bool escaped = false;
  for (size_t i = 1; i + 1 < text.size(); ++i)
  {
    const char ch = text[i];
    if (in_string)
    {
      if (escaped)
        escaped = false;
      else if (ch == '\\')
        escaped = true;
      else if (ch == '"')
        in_string = false;
      continue;
    }

    if (ch == '"')
    {
      in_string = true;
      escaped = false;
      continue;
    }

    if (ch != '.')
      continue;

    const unsigned char prev = static_cast<unsigned char>(text[i - 1]);
    const unsigned char next = static_cast<unsigned char>(text[i + 1]);
    const bool prev_ident = std::isalnum(prev) || prev == '_';
    const bool next_ident = std::isalpha(next) || next == '_';
    if (prev_ident && next_ident)
      return true;
  }
  return false;
}

bool IsAllowedImageProbeObjectStmt(const std::string &text)
{
  return StartsWithTrimmed(text, "ImageProbe ") ||
         StartsWithTrimmed(text, "probe.Load(") ||
         StartsWithTrimmed(text, "probe.Detect(") ||
         StartsWithTrimmed(text, "probe.Score(");
}

bool IsCxcoreContractUseStmt(const std::string &text)
{
  return StartsWithTrimmed(text, "use ");
}

bool IsAllowedCxcoreContractDecl(const std::string &text)
{
  return StartsWithTrimmed(text, "Image ") ||
         StartsWithTrimmed(text, "Roi ") ||
         StartsWithTrimmed(text, "LineMeasurementOutput ") ||
         StartsWithTrimmed(text, "CircleMeasurementOutput ") ||
         StartsWithTrimmed(text, "MatchOutput ") ||
         StartsWithTrimmed(text, "ImageAnalysisOutput ");
}

bool IsAllowedCxcoreContractCallStmt(const std::string &text)
{
  return StartsWithTrimmed(text, "learn_template_model(") ||
         StartsWithTrimmed(text, "measure_line(") ||
         StartsWithTrimmed(text, "measure_circle(") ||
         StartsWithTrimmed(text, "select_formfit_candidate(") ||
         StartsWithTrimmed(text, "match_template(") ||
         StartsWithTrimmed(text, "analyze_region_boundary(") ||
         StartsWithTrimmed(text, "check(") ||
         StartsWithTrimmed(text, "print(") ||
         StartsWithTrimmed(text, "emit(");
}

bool IsAllowedCxcoreContractStringStmt(const std::string &text)
{
  return (StartsWithTrimmed(text, "check(") &&
          (text.find("task_id ==") != std::string::npos ||
           text.find("result_object ==") != std::string::npos ||
           text.find("failure_mode ==") != std::string::npos ||
           text.find("summary ==") != std::string::npos)) ||
         text == "print(task_id);" ||
         text == "print(summary);" ||
         text == "emit(task_id);" ||
         text == "emit(summary);";
}

bool StartsWithUpperTypeDecl(const std::string &text)
{
  size_t begin = 0;
  while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])))
    ++begin;
  if (begin >= text.size() || !std::isupper(static_cast<unsigned char>(text[begin])))
    return false;

  const size_t first_space = text.find(' ', begin);
  const size_t first_paren = text.find('(', begin);
  return first_space != std::string::npos &&
         (first_paren == std::string::npos || first_space < first_paren);
}

bool LooksLikeAssignment(const std::string &text)
{
  const size_t pos = text.find('=');
  if (pos == std::string::npos)
    return false;
  if (pos > 0)
  {
    const char prev = text[pos - 1];
    if (prev == '=' || prev == '!' || prev == '<' || prev == '>')
      return false;
  }
  if (pos + 1 < text.size() && text[pos + 1] == '=')
    return false;
  return true;
}

bool IsMetadataHeaderFunctionStmt(const std::string &text)
{
  const std::string trimmed = TrimCopy(text);
  return StartsWithTrimmed(trimmed, "kind(") ||
         StartsWithTrimmed(trimmed, "layer(") ||
         StartsWithTrimmed(trimmed, "module(") ||
         StartsWithTrimmed(trimmed, "case_name(") ||
         StartsWithTrimmed(trimmed, "mode(") ||
         StartsWithTrimmed(trimmed, "report(");
}

bool ParseQuotedMetadataArg(const std::string &line,
                            const char *name,
                            std::string &value)
{
  const std::string prefix = std::string(name) + "(\"";
  if (!StartsWithTrimmed(line, prefix.c_str()))
    return false;

  const std::string trimmed = TrimCopy(line);
  if (trimmed.size() < prefix.size() + 3 || trimmed.rfind("\");") != trimmed.size() - 3)
    return false;

  value = trimmed.substr(prefix.size(), trimmed.size() - prefix.size() - 3);
  return true;
}

bool ParseBoolMetadataArg(const std::string &line,
                          const char *name,
                          bool &value)
{
  const std::string prefix = std::string(name) + "(";
  if (!StartsWithTrimmed(line, prefix.c_str()))
    return false;

  const std::string trimmed = TrimCopy(line);
  if (trimmed.size() < prefix.size() + 2 || trimmed.back() != ';' || trimmed[trimmed.size() - 2] != ')')
    return false;

  const std::string inner = TrimCopy(trimmed.substr(prefix.size(), trimmed.size() - prefix.size() - 2));
  if (inner == "true")
  {
    value = true;
    return true;
  }
  if (inner == "false")
  {
    value = false;
    return true;
  }
  return false;
}

bool ParseLegacyMetadataLine(const std::string &line,
                             std::string &normalized_line)
{
  const std::string trimmed = TrimCopy(line);
  const size_t pos = trimmed.find('=');
  if (pos == std::string::npos)
    return false;

  const std::string key = TrimCopy(trimmed.substr(0, pos));
  const std::string value = TrimCopy(trimmed.substr(pos + 1));
  if (key.empty() || value.empty())
    return false;

  const bool is_string_key =
    key == "kind" || key == "layer" || key == "module" || key == "case" || key == "mode";
  const bool is_report_key = key == "report";
  if (!is_string_key && !is_report_key)
    return false;

  if (is_report_key)
  {
    const bool enabled = value == "on" || value == "true" || value == "1";
    normalized_line = "report(" + std::string(enabled ? "true" : "false") + ");";
    return true;
  }

  const std::string mapped_key = (key == "case") ? "case_name" : key;
  normalized_line = mapped_key + "(\"" + value + "\");";
  return true;
}

void NormalizeMetadataHeaderSection(std::string &normalized_text)
{
  std::string rewritten;
  rewritten.reserve(normalized_text.size() + 64);

  size_t cursor = 0;
  bool in_header = true;
  while (cursor < normalized_text.size())
  {
    size_t line_end = normalized_text.find('\n', cursor);
    const bool has_newline = line_end != std::string::npos;
    if (!has_newline)
      line_end = normalized_text.size();

    const std::string line = normalized_text.substr(cursor, line_end - cursor);
    const std::string trimmed = TrimCopy(line);

    if (in_header && !trimmed.empty())
    {
      std::string normalized_line;
      if (ParseLegacyMetadataLine(trimmed, normalized_line))
      {
        rewritten += normalized_line;
      }
      else if (IsMetadataHeaderFunctionStmt(trimmed))
      {
        rewritten += trimmed;
      }
      else
      {
        in_header = false;
        rewritten += line;
      }
    }
    else
    {
      rewritten += line;
    }

    if (has_newline)
      rewritten.push_back('\n');
    cursor = has_newline ? (line_end + 1) : line_end;
  }

  normalized_text.swap(rewritten);
}

void ApplyHeaderMetadataToArgs(const CxscriptHeaderMetadata &metadata,
                               CxscriptExecutionArgs &args)
{
  if (!metadata.kind.empty())
    args.script_type = metadata.kind;
  if (!metadata.layer.empty())
    args.layer = metadata.layer;
  if (!metadata.module_name.empty())
    args.module_name = metadata.module_name;
  if (!metadata.case_name.empty())
    args.case_id = metadata.case_name;
  if (!metadata.mode.empty())
    args.mode = metadata.mode;
  if (metadata.has_report)
    args.report_on = metadata.report_on;
}

std::string ResolveSourceScriptPath(const CxscriptExecutionArgs &args)
{
  if (args.script_type == "module" && !args.module_name.empty())
  {
    return "cxscript/module/" + args.module_name + "/" +
           args.layer + "/" + args.case_id + ".cxscript";
  }

  if (args.script_type == "integration" && !args.integration_name.empty())
  {
    return "cxscript/integration/" + args.integration_name + "/" +
           args.case_id + ".cxscript";
  }

  return std::string();
}

std::string ResolveLegacySourceScriptPath(const CxscriptExecutionArgs &args)
{
  if (args.script_type == "module" && !args.module_name.empty())
  {
    return "cxscript/module/" + args.module_name + "/" +
           args.layer + "." + args.case_id + ".cxs";
  }

  if (args.script_type == "integration" && !args.integration_name.empty())
  {
    return "cxscript/integration/" + args.integration_name + "/" +
           args.layer + "." + args.case_id + ".cxs";
  }

  return std::string();
}

std::string ResolveDefaultScriptPath(const CxscriptExecutionArgs &args)
{
  if (!args.script_path.empty())
    return args.script_path;

  if (args.script_type == "module" && !args.module_name.empty())
  {
    return "cxscript/module/" + args.module_name + "/" +
           args.layer + "." + args.case_id + ".cxsc";
  }

  if (args.script_type == "integration" && !args.integration_name.empty())
  {
    return "cxscript/integration/" + args.integration_name + "/" +
           args.layer + "." + args.case_id + ".cxsc";
  }

  return std::string();
}

std::string ResolveWorkspaceAnchoredScriptPath(const std::string &script_path)
{
  if (script_path.empty())
    return std::string();

  const bool has_drive_prefix =
    script_path.size() > 1 &&
    std::isalpha(static_cast<unsigned char>(script_path[0])) &&
    script_path[1] == ':';
  const bool is_absolute =
    has_drive_prefix ||
    script_path[0] == '/' ||
    script_path[0] == '\\';
  if (is_absolute)
    return script_path;

#ifdef CXPARSER_WORKSPACE_ROOT
  return std::string(CXPARSER_WORKSPACE_ROOT) + "/" + script_path;
#else
  return script_path;
#endif
}

std::string ResolveLegacyExplicitScriptAliasPath(const std::string &script_path)
{
  if (script_path.empty())
    return std::string();

  std::string normalized = script_path;
  for (size_t i = 0; i < normalized.size(); ++i)
  {
    if (normalized[i] == '\\')
      normalized[i] = '/';
  }

  if (normalized.size() < 5 || normalized.substr(normalized.size() - 4) != ".cxs")
    return std::string();

  const size_t slash = normalized.find_last_of('/');
  const std::string parent = slash == std::string::npos ? std::string() : normalized.substr(0, slash);
  const std::string file_name = slash == std::string::npos ? normalized : normalized.substr(slash + 1);
  const std::string stem = file_name.substr(0, file_name.size() - 4);

  const size_t dot = stem.find('.');
  if (dot == std::string::npos || dot == 0 || dot + 1 >= stem.size())
    return std::string();

  const std::string layer = stem.substr(0, dot);
  const std::string case_id = stem.substr(dot + 1);
  if (layer.empty() || case_id.empty())
    return std::string();

  return parent + "/" + layer + "/" + case_id + ".cxscript";
}

std::vector<std::string> SplitPathTokens(const std::string &script_path)
{
  std::vector<std::string> tokens;
  std::string current;
  for (size_t i = 0; i < script_path.size(); ++i)
  {
    const char ch = script_path[i];
    if (ch == '/' || ch == '\\')
    {
      if (!current.empty())
      {
        tokens.push_back(current);
        current.clear();
      }
      continue;
    }

    current.push_back(ch);
  }

  if (!current.empty())
    tokens.push_back(current);
  return tokens;
}

bool TryInferExpectedHeaderFromScriptPath(const std::string &script_path,
                                         CxscriptHeaderMetadata &metadata)
{
  metadata = CxscriptHeaderMetadata();
  if (script_path.empty())
    return false;

  std::string normalized = script_path;
  for (size_t i = 0; i < normalized.size(); ++i)
  {
    if (normalized[i] == '\\')
      normalized[i] = '/';
  }

  std::vector<std::string> tokens = SplitPathTokens(normalized);
  for (size_t i = 0; i + 1 < tokens.size(); ++i)
  {
    if (tokens[i] == "module")
    {
      metadata.kind = "module";
      metadata.module_name = tokens[i + 1];
      break;
    }
    if (tokens[i] == "integration")
    {
      metadata.kind = "integration";
      break;
    }
  }

  const size_t slash = normalized.find_last_of('/');
  const std::string parent = slash == std::string::npos ? std::string() : normalized.substr(0, slash);
  const std::string file_name = slash == std::string::npos ? normalized : normalized.substr(slash + 1);

  if (file_name.size() > 4 && file_name.substr(file_name.size() - 4) == ".cxs")
  {
    const std::string stem = file_name.substr(0, file_name.size() - 4);
    const size_t dot = stem.find('.');
    if (dot == std::string::npos || dot == 0 || dot + 1 >= stem.size())
      return false;

    metadata.layer = stem.substr(0, dot);
    metadata.case_name = stem.substr(dot + 1);
    return !metadata.layer.empty() && !metadata.case_name.empty();
  }

  if (file_name.size() > 9 && file_name.substr(file_name.size() - 9) == ".cxscript")
  {
    metadata.case_name = file_name.substr(0, file_name.size() - 9);
    const size_t parent_slash = parent.find_last_of('/');
    if (parent_slash == std::string::npos)
      return false;

    metadata.layer = parent.substr(parent_slash + 1);
    return !metadata.layer.empty() && !metadata.case_name.empty();
  }

  return false;
}

bool HeaderMetadataMatchesExpected(const CxscriptHeaderMetadata &expected,
                                   const CxscriptHeaderMetadata &actual)
{
  if (!expected.kind.empty() && actual.kind != expected.kind)
    return false;
  if (!expected.module_name.empty() && actual.module_name != expected.module_name)
    return false;
  if (!expected.layer.empty() && actual.layer != expected.layer)
    return false;
  if (!expected.case_name.empty() && actual.case_name != expected.case_name)
    return false;
  return true;
}

bool LoadedScriptMatchesExpectedIdentity(const std::string &script_path,
                                        const std::string &normalized_text)
{
  CxscriptHeaderMetadata expected;
  if (!TryInferExpectedHeaderFromScriptPath(script_path, expected))
    return true;

  CxscriptHeaderMetadata actual;
  if (!ExtractCxscriptHeaderMetadata(normalized_text, actual))
    return true;

  return HeaderMetadataMatchesExpected(expected, actual);
}

std::string ResolveCalleeModule(const CxscriptExecutionArgs &args)
{
  if (!args.module_name.empty())
    return args.module_name;
  if (!args.integration_name.empty())
    return args.integration_name;
  return "cxscript";
}

std::string JoinModules(const std::vector<std::string> &modules)
{
  std::string text;
  for (size_t i = 0; i < modules.size(); ++i)
  {
    if (i != 0)
      text += "->";
    text += modules[i];
  }
  return text;
}

std::string JoinValues(const std::vector<std::string> &values)
{
  std::string text;
  for (size_t i = 0; i < values.size(); ++i)
  {
    if (i != 0)
      text += "->";
    text += values[i];
  }
  return text;
}

std::string FormatMetricPair(const std::string &name, double value)
{
  if (name.empty())
    return "n/a";

  std::ostringstream stream;
  stream << name << "=" << value;
  return stream.str();
}

void FillCxscriptIrSummary(const std::string &script_text,
                           CxscriptExecutionResult &result)
{
  CxscriptIrProgram program;
  if (!BuildCxscriptLinearIr(script_text, program))
  {
    result.ir_valid = false;
    result.ir_error_message = program.error_message;
    result.compile_text.clear();
    return;
  }

  result.ir_valid = true;
  result.ir_error_message.clear();
  result.ir_op_count = static_cast<int>(program.ops.size());
  result.ir_stmt_count = 0;
  result.ir_block_count = 0;
  for (size_t i = 0; i < program.ops.size(); ++i)
  {
    if (program.ops[i].kind == cxik_stmt)
      ++result.ir_stmt_count;
    if (program.ops[i].kind == cxik_block_begin || program.ops[i].kind == cxik_block_end)
      ++result.ir_block_count;
  }
  RenderCxscriptIrCompileText(program, result.compile_text);
}

bool ReadTextFile(const std::string &path, std::string &text)
{
  std::ifstream input(path.c_str(), std::ios::in | std::ios::binary);
  if (!input.is_open())
    return false;

  std::ostringstream buffer;
  buffer << input.rdbuf();
  text = buffer.str();
  return true;
}

bool ContainsAny(const std::string &text, const char *const *patterns, size_t count)
{
  for (size_t i = 0; i < count; ++i)
  {
    if (text.find(patterns[i]) != std::string::npos)
      return true;
  }
  return false;
}

std::vector<std::string> SplitCaseIdentityParts(const std::string &case_id)
{
  std::vector<std::string> parts;
  std::string current;
  for (size_t i = 0; i < case_id.size(); ++i)
  {
    if (case_id[i] == '.')
    {
      if (!current.empty())
        parts.push_back(current);
      current.clear();
      continue;
    }

    current.push_back(case_id[i]);
  }

  if (!current.empty())
    parts.push_back(current);
  return parts;
}

std::string NormalizePublicCaseId(const CxscriptExecutionArgs &args)
{
  if (args.case_id.empty())
    return std::string();

  const std::vector<std::string> parts = SplitCaseIdentityParts(args.case_id);
  if (parts.size() == 4 &&
      !args.module_name.empty() &&
      parts[1] == args.module_name &&
      parts[3] == args.layer)
    return parts[2];

  if (parts.size() == 4 &&
      !args.integration_name.empty() &&
      parts[1] == args.integration_name &&
      parts[3] == args.layer)
    return parts[2];

  return args.case_id;
}

void AppendCharWithNewlinePreservation(char ch,
                                       std::string &normalized_text)
{
  if (ch == '\r' || ch == '\n')
    normalized_text.push_back(ch);
  else
    normalized_text.push_back(' ');
}
}

bool BuildCxscriptIdentity(const CxscriptExecutionArgs &args,
                           CxscriptIdentity &identity)
{
  CxscriptExecutionArgs normalized_args = args;
  normalized_args.case_id = NormalizePublicCaseId(args);

  identity = CxscriptIdentity();
  identity.script_type = normalized_args.script_type;
  identity.module_name = normalized_args.module_name;
  identity.integration_name = normalized_args.integration_name;
  identity.layer = normalized_args.layer;
  identity.case_id = normalized_args.case_id;
  identity.file_path =
    ResolveWorkspaceAnchoredScriptPath(ResolveDefaultScriptPath(normalized_args));

  if (identity.script_type.empty() || identity.layer.empty() || identity.case_id.empty())
    return false;

  if (identity.script_type == "module")
    return !identity.module_name.empty() && !identity.file_path.empty();

  if (identity.script_type == "integration")
    return !identity.integration_name.empty() && !identity.file_path.empty();

  return false;
}

void BuildCxscriptContext(const CxscriptExecutionArgs &args,
                          CxscriptExecutionContext &context)
{
  CxscriptExecutionArgs normalized_args = args;
  normalized_args.case_id = NormalizePublicCaseId(args);

  context = CxscriptExecutionContext();
  context.caller_module = "cxparser";
  context.callee_module = ResolveCalleeModule(normalized_args);
  context.route = normalized_args.route.empty() ? "default" : normalized_args.route;
  context.execution_mode = normalized_args.mode.empty() ? "build-run" : normalized_args.mode;
  context.trace_id = normalized_args.trace_id.empty() ?
    ("trace." + context.callee_module + "." + normalized_args.layer + "." + normalized_args.case_id) :
    normalized_args.trace_id;
}

void NormalizeCxscriptText(const std::string &source_text,
                           std::string &normalized_text)
{
  normalized_text.clear();
  normalized_text.reserve(source_text.size());

  bool in_string = false;
  bool escaped = false;
  bool line_comment = false;
  bool block_comment = false;

  for (size_t i = 0; i < source_text.size(); ++i)
  {
    const char ch = source_text[i];
    const char next = (i + 1 < source_text.size()) ? source_text[i + 1] : '\0';

    if (line_comment)
    {
      AppendCharWithNewlinePreservation(ch, normalized_text);
      if (ch == '\n')
        line_comment = false;
      continue;
    }

    if (block_comment)
    {
      AppendCharWithNewlinePreservation(ch, normalized_text);
      if (ch == '*' && next == '/')
      {
        AppendCharWithNewlinePreservation(next, normalized_text);
        ++i;
        block_comment = false;
      }
      continue;
    }

    if (in_string)
    {
      normalized_text.push_back(ch);
      if (escaped)
      {
        escaped = false;
      }
      else if (ch == '\\')
      {
        escaped = true;
      }
      else if (ch == '"')
      {
        in_string = false;
      }
      continue;
    }

    if (ch == '"')
    {
      normalized_text.push_back(ch);
      in_string = true;
      escaped = false;
      continue;
    }

    if (ch == '/' && next == '/')
    {
      normalized_text.push_back(' ');
      normalized_text.push_back(' ');
      ++i;
      line_comment = true;
      continue;
    }

    if (ch == '/' && next == '*')
    {
      normalized_text.push_back(' ');
      normalized_text.push_back(' ');
      ++i;
      block_comment = true;
      continue;
    }

    normalized_text.push_back(ch);
  }

  // Metadata header lines are normalized after comment stripping so both legacy
  // key-value headers and new function-style headers converge on one syntax.
  NormalizeMetadataHeaderSection(normalized_text);
}

bool ExtractCxscriptHeaderMetadata(const std::string &script_text,
                                   CxscriptHeaderMetadata &metadata)
{
  metadata = CxscriptHeaderMetadata();

  size_t cursor = 0;
  bool found_any = false;
  while (cursor < script_text.size())
  {
    size_t line_end = script_text.find('\n', cursor);
    const bool has_newline = line_end != std::string::npos;
    if (!has_newline)
      line_end = script_text.size();

    const std::string line = script_text.substr(cursor, line_end - cursor);
    const std::string trimmed = TrimCopy(line);
    if (!trimmed.empty())
    {
      std::string value;
      bool bool_value = false;
      if (ParseQuotedMetadataArg(trimmed, "kind", value))
      {
        metadata.kind = value;
        found_any = true;
      }
      else if (ParseQuotedMetadataArg(trimmed, "layer", value))
      {
        metadata.layer = value;
        found_any = true;
      }
      else if (ParseQuotedMetadataArg(trimmed, "module", value))
      {
        metadata.module_name = value;
        found_any = true;
      }
      else if (ParseQuotedMetadataArg(trimmed, "case_name", value))
      {
        metadata.case_name = value;
        found_any = true;
      }
      else if (ParseQuotedMetadataArg(trimmed, "mode", value))
      {
        metadata.mode = value;
        found_any = true;
      }
      else if (ParseBoolMetadataArg(trimmed, "report", bool_value))
      {
        metadata.has_report = true;
        metadata.report_on = bool_value;
        found_any = true;
      }
      else
      {
        break;
      }
    }

    cursor = has_newline ? (line_end + 1) : line_end;
  }

  return found_any;
}

bool LoadCxscriptText(const CxscriptIdentity &identity,
                      const std::string &fallback_text,
                      std::string &script_text,
                      std::string &script_origin)
{
  std::string raw_text;
  script_origin.clear();

  if (!identity.file_path.empty() && ReadTextFile(identity.file_path, raw_text))
  {
    NormalizeCxscriptText(raw_text, script_text);
    if (LoadedScriptMatchesExpectedIdentity(identity.file_path, script_text))
    {
      script_origin = "file";
      return true;
    }
  }

  if (!identity.file_path.empty())
  {
    const std::string aliased_explicit_path =
      ResolveLegacyExplicitScriptAliasPath(identity.file_path);
    if (!aliased_explicit_path.empty() &&
        ReadTextFile(aliased_explicit_path, raw_text))
    {
      NormalizeCxscriptText(raw_text, script_text);
      if (LoadedScriptMatchesExpectedIdentity(aliased_explicit_path, script_text))
      {
        script_origin = "file";
        return true;
      }
    }
  }

  CxscriptExecutionArgs source_args;
  source_args.script_type = identity.script_type;
  source_args.module_name = identity.module_name;
  source_args.integration_name = identity.integration_name;
  source_args.layer = identity.layer;
  source_args.case_id = identity.case_id;

  const std::string source_path = ResolveSourceScriptPath(source_args);
  const std::string anchored_source_path = ResolveWorkspaceAnchoredScriptPath(source_path);
  if (!anchored_source_path.empty() && ReadTextFile(anchored_source_path, raw_text))
  {
    NormalizeCxscriptText(raw_text, script_text);
    if (LoadedScriptMatchesExpectedIdentity(anchored_source_path, script_text))
    {
      script_origin = "file";
      return true;
    }
  }

  const std::string legacy_source_path = ResolveLegacySourceScriptPath(source_args);
  const std::string anchored_legacy_source_path = ResolveWorkspaceAnchoredScriptPath(legacy_source_path);
  if (!anchored_legacy_source_path.empty() && ReadTextFile(anchored_legacy_source_path, raw_text))
  {
    NormalizeCxscriptText(raw_text, script_text);
    if (LoadedScriptMatchesExpectedIdentity(anchored_legacy_source_path, script_text))
    {
      script_origin = "file";
      return true;
    }
  }

  if (!fallback_text.empty())
  {
    NormalizeCxscriptText(fallback_text, script_text);
    script_origin = "catalog";
    return true;
  }

  return false;
}

void AnalyzeCxscriptFlow(const std::string &script_text,
                         CxscriptFlowProfile &flow_profile)
{
  // Header metadata is part of the cxsc metadata header, not ordinary business calls.
  // During the transition we still accept legacy key-value header lines so old scripts
  // remain valid, but new scripts should move to function-style header metadata.
  static const char *const prepare_patterns[] = {
    "prepare_ok", "prepare_", "Load(", "load_", "set_", "input_path", "input {"
  };
  static const char *const action_patterns[] = {
    "action_ok", "run_", "Detect(", "detect(", "train(", "infer(", "step(", "flow {", "action ", "call "
  };
  static const char *const check_patterns[] = {
    "check_ok", "score", "Score(", "result_count", "summaryscore(", "check {", "require "
  };
  static const char *const report_patterns[] = {
    "report_text", "report(", "summary(", "summary();", "stage_report", "report {",
    "output {", "emit ", "conclusion {"
  };

  flow_profile = CxscriptFlowProfile();
  flow_profile.has_prepare = ContainsAny(script_text, prepare_patterns, sizeof(prepare_patterns) / sizeof(prepare_patterns[0]));
  flow_profile.has_action = ContainsAny(script_text, action_patterns, sizeof(action_patterns) / sizeof(action_patterns[0]));
  flow_profile.has_check = ContainsAny(script_text, check_patterns, sizeof(check_patterns) / sizeof(check_patterns[0]));
  flow_profile.has_report = ContainsAny(script_text, report_patterns, sizeof(report_patterns) / sizeof(report_patterns[0]));

  const bool explicit_flow =
    script_text.find("prepare_ok") != std::string::npos ||
    script_text.find("action_ok") != std::string::npos ||
    script_text.find("check_ok") != std::string::npos ||
    script_text.find("report_text") != std::string::npos ||
    script_text.find("scenario {") != std::string::npos ||
    script_text.find("check {") != std::string::npos ||
    script_text.find("report {") != std::string::npos ||
    script_text.find("input {") != std::string::npos ||
    script_text.find("flow {") != std::string::npos ||
    script_text.find("output {") != std::string::npos ||
    script_text.find("conclusion {") != std::string::npos ||
    // Legacy metadata-header compatibility. These markers belong to the header/body
    // boundary and are kept only so existing key-value cxsc files still normalize.
    script_text.find("kind=") != std::string::npos ||
    script_text.find("call_1=") != std::string::npos ||
    script_text.find("check_1=") != std::string::npos ||
    script_text.find("expect_output_1=") != std::string::npos;

  flow_profile.script_style = explicit_flow ? "flow_style" : "call_style";
}

void AnalyzeCxscriptSemantics(const std::string &script_text,
                              const CxscriptIrProgram &program,
                              CxscriptBasicSemanticProfile &basic_profile,
                              CxscriptBindingSemanticProfile &binding_profile)
{
  static_cast<void>(script_text);
  basic_profile = CxscriptBasicSemanticProfile();
  binding_profile = CxscriptBindingSemanticProfile();

  for (size_t i = 0; i < program.ops.size(); ++i)
  {
    const CxscriptIrOp &op = program.ops[i];
    if (op.kind == cxik_if_begin)
    {
      basic_profile.has_if_block = true;
      continue;
    }

    if (op.kind != cxik_stmt)
      continue;

    const std::string text = op.text;
    const bool is_check_call = StartsWithTrimmed(text, "check(");
    const bool is_print_call = StartsWithTrimmed(text, "print(") || StartsWithTrimmed(text, "emit(");
    const bool is_register_class = StartsWithTrimmed(text, "register_class(");
    const bool is_register_fun = StartsWithTrimmed(text, "register_fun(");
    const bool is_declaration =
      StartsWithTrimmed(text, "double ") ||
      StartsWithUpperTypeDecl(text) ||
      IsCxcoreContractUseStmt(text);
    const bool is_assignment = LooksLikeAssignment(text);
    const bool is_call_stmt = text.find('(') != std::string::npos;
    const bool uses_object_binding =
      LooksLikeMemberCall(text) ||
      StartsWithUpperTypeDecl(text) ||
      IsCxcoreContractUseStmt(text);
    const bool uses_explicit_registration = is_register_class || is_register_fun;

    if (is_declaration)
      basic_profile.has_declaration = true;
    if (is_assignment)
      basic_profile.has_assignment = true;
    if (is_call_stmt)
      basic_profile.has_call_stmt = true;
    if (is_check_call)
      basic_profile.has_check_call = true;
    if (is_print_call)
      basic_profile.has_print_call = true;
    if (is_register_class)
      basic_profile.has_register_class = true;
    if (is_register_fun)
      basic_profile.has_register_fun = true;
    if (!is_declaration && !is_assignment && !is_call_stmt)
      basic_profile.has_expression_stmt = true;

    if (uses_object_binding)
    {
      binding_profile.requires_registered_binding = true;
      binding_profile.uses_object_binding = true;
    }
    if (uses_explicit_registration)
    {
      binding_profile.requires_registered_binding = true;
      binding_profile.uses_explicit_registration = true;
    }
  }

  if (binding_profile.uses_object_binding && binding_profile.uses_explicit_registration)
    binding_profile.binding_scope = "object_binding+registration";
  else if (binding_profile.uses_object_binding)
    binding_profile.binding_scope = "object_binding";
  else if (binding_profile.uses_explicit_registration)
    binding_profile.binding_scope = "registration";
  else
    binding_profile.binding_scope = "native_only";
}

void BuildCxscriptLayerProfile(const std::string &script_text,
                               CxscriptLayerProfile &layer_profile,
                               std::string *normalized_text)
{
  layer_profile = CxscriptLayerProfile();
  layer_profile.has_source_text = !script_text.empty();

  std::string local_normalized_text;
  NormalizeCxscriptText(script_text, local_normalized_text);
  layer_profile.has_normalized_text = !local_normalized_text.empty();

  CxscriptIrProgram program;
  if (BuildCxscriptLinearIr(local_normalized_text, program))
  {
    layer_profile.has_linear_ir = true;

    std::string compile_text;
    RenderCxscriptIrCompileText(program, compile_text);
    layer_profile.has_compile_bridge = !compile_text.empty();
    EvaluateCxscriptCompileBridgeSafety(local_normalized_text, program, layer_profile);
  }
  else
  {
    layer_profile.bridge_exec_safe = false;
    layer_profile.bridge_exec_reason = "linear_ir_invalid";
  }

  if (normalized_text)
    *normalized_text = local_normalized_text;
}

void EvaluateCxscriptCompileBridgeSafety(const std::string &script_text,
                                         const CxscriptIrProgram &program,
                                         CxscriptLayerProfile &layer_profile)
{
  layer_profile.bridge_exec_safe = false;
  layer_profile.bridge_exec_subset.clear();
  layer_profile.bridge_exec_reason.clear();

  if (!layer_profile.has_linear_ir || !layer_profile.has_compile_bridge)
  {
    layer_profile.bridge_exec_reason = "bridge_unavailable";
    return;
  }

  if (script_text.find("prepare_ok") != std::string::npos ||
      script_text.find("action_ok") != std::string::npos ||
      script_text.find("check_ok") != std::string::npos ||
      script_text.find("report_text") != std::string::npos)
  {
    layer_profile.bridge_exec_reason = "flow_markers_require_guarded_bridge";
    return;
  }

  bool uses_allowed_object_subset = false;
  bool uses_cxcore_contract_subset = false;

  for (size_t i = 0; i < program.ops.size(); ++i)
  {
    const CxscriptIrOp &op = program.ops[i];
    if (op.kind == cxik_for_begin)
    {
      layer_profile.bridge_exec_reason = "for_loop_bridge_not_enabled";
      return;
    }

    if (op.kind == cxik_stmt)
    {
      if (IsCxcoreContractUseStmt(op.text))
      {
        uses_cxcore_contract_subset = true;
        continue;
      }

      if (IsAllowedCxcoreContractDecl(op.text))
      {
        uses_cxcore_contract_subset = true;
        continue;
      }

      if (IsAllowedCxcoreContractCallStmt(op.text))
      {
        uses_cxcore_contract_subset = true;
      }

      if (IsAllowedCxcoreContractStringStmt(op.text))
      {
        uses_cxcore_contract_subset = true;
        continue;
      }

      if (LooksLikeMemberCall(op.text))
      {
        if (!IsAllowedImageProbeObjectStmt(op.text))
        {
          layer_profile.bridge_exec_reason = "member_call_bridge_not_enabled";
          return;
        }
        uses_allowed_object_subset = true;
      }

      if (op.text.find('"') != std::string::npos)
      {
        if (!IsAllowedImageProbeObjectStmt(op.text) &&
            !IsAllowedCxcoreContractStringStmt(op.text))
        {
          layer_profile.bridge_exec_reason = "string_bridge_not_enabled";
          return;
        }
        if (IsAllowedImageProbeObjectStmt(op.text))
          uses_allowed_object_subset = true;
        else
          uses_cxcore_contract_subset = true;
      }

      if (StartsWithUpperTypeDecl(op.text) &&
          !IsAllowedCxcoreContractDecl(op.text) &&
          !IsAllowedImageProbeObjectStmt(op.text))
      {
        layer_profile.bridge_exec_reason = "type_decl_bridge_not_enabled";
        return;
      }
    }
  }

  layer_profile.bridge_exec_safe = true;
  if (uses_cxcore_contract_subset)
  {
    layer_profile.bridge_exec_subset = "cxcore_contract_call";
    layer_profile.bridge_exec_reason = "cxcore_contract_call_subset";
  }
  else if (uses_allowed_object_subset)
  {
    layer_profile.bridge_exec_subset = "object_flow";
    layer_profile.bridge_exec_reason = "object_flow_subset";
  }
  else
  {
    layer_profile.bridge_exec_subset =
      (script_text.find("if") != std::string::npos) ? "numeric_if" : "numeric_stmt";
    layer_profile.bridge_exec_reason =
      (script_text.find("if") != std::string::npos) ? "numeric_if_subset" : "numeric_stmt_subset";
  }
}

void BuildCxscriptStructureSummary(const std::string &script_text,
                                   CxscriptExecutionResult &result)
{
  std::string normalized_text;
  BuildCxscriptLayerProfile(script_text, result.layer_profile, &normalized_text);
  ExtractCxscriptHeaderMetadata(normalized_text, result.header_metadata);
  if (result.header_metadata.has_report)
    result.report_requested = result.header_metadata.report_on;
  AnalyzeCxscriptFlow(normalized_text, result.flow_profile);
  FillCxscriptIrSummary(normalized_text, result);
  if (result.ir_valid)
  {
    CxscriptIrProgram program;
    if (BuildCxscriptLinearIr(normalized_text, program))
      AnalyzeCxscriptSemantics(normalized_text, program, result.basic_semantics, result.binding_semantics);
  }
}

void BuildRuntimeMultimodalSlices(CxscriptRuntimeReport &report)
{
  report.multimodal_slices.clear();
  report.operation_atoms.clear();

  MultimodalSlice semantic_slice;
  semantic_slice.slice_id =
    report.case_id.empty() ? "cxscript.code_semantic_slice_v1"
                           : report.case_id + ".code_semantic_slice_v1";
  semantic_slice.source_ref = report.file_path.empty() ? report.case_id : report.file_path;
  semantic_slice.source_hash = BuildPseudoSourceHash(
    semantic_slice.source_ref + "|" + report.layer + "|" + report.module_name + "|" + report.integration_name);
  semantic_slice.modality = "code";
  semantic_slice.analysis_kind = "code_semantic";
  semantic_slice.result_ref = report.task_id;
  semantic_slice.evidence_ref = report.file_path;
  semantic_slice.log_path = report.file_path;
  semantic_slice.confidence = report.ir_valid ? 1.0 : 0.5;
  semantic_slice.next_action = report.success
    ? "consume operation atoms or dispatch slices"
    : "inspect parser error and replay flow";
  semantic_slice.tags.push_back("code_semantic_slice_v1");

  for (size_t i = 0; i < report.fragment_ids.size(); ++i)
  {
    MultimodalSliceObject object;
    object.object_id = report.fragment_ids[i];
    object.object_kind = "fragment";
    object.geometry_ref = report.layer;
    object.semantic_label = "cxscript_fragment";
    object.summary = "registered cxscript fragment participating in the flow";
    object.confidence = 1.0;
    semantic_slice.objects.push_back(object);
  }

  for (size_t i = 0; i < report.bundle_ids.size(); ++i)
  {
    MultimodalSliceObject object;
    object.object_id = report.bundle_ids[i];
    object.object_kind = "bundle";
    object.geometry_ref = report.layer;
    object.semantic_label = "cxscript_bundle";
    object.summary = "bundle grouping reusable outputs for current flow";
    object.confidence = 1.0;
    semantic_slice.objects.push_back(object);
  }

  for (size_t i = 0; i < report.workflow_path_groups.size(); ++i)
  {
    MultimodalSliceRelation relation;
    relation.relation_kind = "workflow_path_group";
    relation.source_object_id = report.case_id;
    relation.target_object_id = report.workflow_path_groups[i];
    relation.summary = "flow grouped into workflow path bucket";
    semantic_slice.relations.push_back(relation);
  }
  report.multimodal_slices.push_back(semantic_slice);

  MultimodalSlice operation_slice;
  operation_slice.slice_id =
    report.case_id.empty() ? "cxscript.operation_chain_v1"
                           : report.case_id + ".operation_chain_v1";
  operation_slice.source_ref = semantic_slice.source_ref;
  operation_slice.source_hash = BuildPseudoSourceHash(operation_slice.slice_id + semantic_slice.source_hash);
  operation_slice.modality = "operation_chain";
  operation_slice.analysis_kind = "operation_atom";
  operation_slice.result_ref = report.task_id;
  operation_slice.evidence_ref = report.summary;
  operation_slice.log_path = report.file_path;
  operation_slice.model_ref = report.result_object;
  operation_slice.confidence = report.success ? 1.0 : 0.5;
  operation_slice.next_action = report.flow_profile.has_check
    ? "consume result checks"
    : "extend result checks before promotion";
  operation_slice.tags.push_back("operation_chain_v1");

  OperationAtom prepare_atom;
  prepare_atom.atom_id = operation_slice.slice_id + ".prepare";
  prepare_atom.stage = "prepare";
  prepare_atom.action_kind = "prepare_inputs";
  prepare_atom.input_ref = report.file_path;
  prepare_atom.output_ref = report.case_id + ".prepare";
  prepare_atom.status = report.flow_profile.has_prepare ? "ok" : "missing";
  prepare_atom.summary = "prepare-stage presence derived from cxscript flow profile";
  report.operation_atoms.push_back(prepare_atom);

  OperationAtom action_atom;
  action_atom.atom_id = operation_slice.slice_id + ".action";
  action_atom.stage = "action";
  action_atom.action_kind = report.layer_profile.execution_text_kind.empty()
    ? "execute_flow"
    : report.layer_profile.execution_text_kind;
  action_atom.input_ref = report.case_id + ".prepare";
  action_atom.output_ref = report.result_object;
  action_atom.status = report.flow_profile.has_action ? "ok" : "missing";
  action_atom.summary = "main execution atom derived from runtime execution kind";
  report.operation_atoms.push_back(action_atom);

  OperationAtom check_atom;
  check_atom.atom_id = operation_slice.slice_id + ".check";
  check_atom.stage = "check";
  check_atom.action_kind = "result_check";
  check_atom.input_ref = report.result_object;
  check_atom.output_ref = report.summary;
  check_atom.status = report.flow_profile.has_check ? "ok" : "missing";
  check_atom.summary = "check-stage presence derived from flow profile";
  report.operation_atoms.push_back(check_atom);

  OperationAtom report_atom;
  report_atom.atom_id = operation_slice.slice_id + ".report";
  report_atom.stage = "report";
  report_atom.action_kind = "publish_summary";
  report_atom.input_ref = report.summary;
  report_atom.output_ref = report.task_id;
  report_atom.status = report.report_requested ? "ok" : "minimal";
  report_atom.summary = "report-stage export derived from runtime report settings";
  report.operation_atoms.push_back(report_atom);

  for (size_t i = 0; i < report.operation_atoms.size(); ++i)
  {
    MultimodalSliceObject object;
    object.object_id = report.operation_atoms[i].atom_id;
    object.object_kind = "operation_atom";
    object.geometry_ref = report.operation_atoms[i].stage;
    object.semantic_label = report.operation_atoms[i].action_kind;
    object.summary = report.operation_atoms[i].summary;
    object.confidence = report.operation_atoms[i].status == "ok" ? 1.0 : 0.5;
    operation_slice.objects.push_back(object);
  }
  report.multimodal_slices.push_back(operation_slice);
}

void BuildCxscriptRuntimeReport(const CxscriptExecutionResult &result,
                                CxscriptRuntimeReport &report)
{
  report = CxscriptRuntimeReport();
  report.script_type = result.identity.script_type;
  report.module_name = result.identity.module_name;
  report.integration_name = result.identity.integration_name;
  report.layer = result.identity.layer;
  report.case_id = result.identity.case_id;
  report.mode = result.context.execution_mode;
  report.route = result.context.route;
  report.file_path = result.identity.file_path;
  report.script_origin = result.script_origin;
  report.header_metadata = result.header_metadata;
  report.status = result.status;
  report.success = result.success;
  report.report_requested = result.report_requested;
  report.error_kind = result.error_kind;
  report.error_message = result.error_message;
  report.parser_error_code = result.parser_error_code;
  report.parser_error_pos = result.parser_error_pos;
  report.parser_error_token = result.parser_error_token;
  report.parser_error_expr = result.parser_error_expr;
  report.last_step_id = result.last_step_id;
  report.last_frame_id = result.last_frame_id;
  report.last_sequence = result.last_sequence;
  report.last_source_line = result.last_source_line;
  report.failure_step_id = result.failure_step_id;
  report.failure_frame_id = result.failure_frame_id;
  report.failure_sequence = result.failure_sequence;
  report.failure_line = result.failure_line;
  report.failure_phase = result.failure_phase;
  report.accepted_task_count = result.accepted_task_count;
  report.executed_task_count = result.executed_task_count;
  report.replay_count = result.replay_count;
  report.replay_source_task_id = result.replay_source_task_id;
  report.replay_stage_count = result.replay_stage_count;
  report.flow_profile = result.flow_profile;
  report.basic_semantics = result.basic_semantics;
  report.binding_semantics = result.binding_semantics;
  report.layer_profile = result.layer_profile;
  report.ir_valid = result.ir_valid;
  report.ir_op_count = result.ir_op_count;
  report.ir_stmt_count = result.ir_stmt_count;
  report.ir_block_count = result.ir_block_count;
  report.ir_error_message = result.ir_error_message;
  report.compile_text = result.compile_text;
  report.baseline_snapshot = result.baseline_snapshot;
  report.best_snapshot = result.best_snapshot;
  report.optimization_compare = result.optimization_compare;
  report.bridge_point_count = result.bridge_point_count;
  report.bridge_matched_call_count = result.bridge_matched_call_count;
  report.bridge_unresolved_call_count = result.bridge_unresolved_call_count;
  report.bridge_summary = result.bridge_summary;
  report.bridge_point_lines = result.bridge_point_lines;
  report.modules = result.modules;
  report.fragment_ids = result.fragment_ids;
  report.bundle_ids = result.bundle_ids;
  report.workflow_path_groups = result.workflow_path_groups;
  BuildRuntimeMultimodalSlices(report);
}

void RefreshCxscriptRuntimeSlices(CxscriptRuntimeReport &report)
{
  BuildRuntimeMultimodalSlices(report);
}

void FormatCxscriptResult(const CxscriptExecutionResult &result,
                          std::vector<std::string> &lines)
{
  lines.clear();
  CxscriptRuntimeReport report;
  BuildCxscriptRuntimeReport(result, report);

  std::string header = "[CXSCRIPT] type=" + report.script_type;
  if (!report.module_name.empty())
    header += " module=" + report.module_name;
  if (!report.integration_name.empty())
    header += " integration=" + report.integration_name;
  header += " layer=" + report.layer;
  header += " case=" + report.case_id;
  header += " mode=" + report.mode;
  lines.push_back(header);

  lines.push_back("[STATUS] success=" + std::string(report.success ? "true" : "false") +
                  " route=" + report.route +
                  " status=" + report.status);

  if (!report.success && (!report.error_kind.empty() || !report.error_message.empty()))
  {
    lines.push_back("[ERROR] kind=" + report.error_kind +
                    " message=" + report.error_message +
                    " code=" + std::to_string(report.parser_error_code) +
                    " pos=" + std::to_string(report.parser_error_pos) +
                    " token=" + report.parser_error_token +
                    " expr=" + report.parser_error_expr);
  }

  lines.push_back("[SCRIPT] file=" + report.file_path +
                  " origin=" + report.script_origin);

  lines.push_back("[FLOW] style=" + report.flow_profile.script_style +
                  " prepare=" + std::string(report.flow_profile.has_prepare ? "true" : "false") +
                  " action=" + std::string(report.flow_profile.has_action ? "true" : "false") +
                  " check=" + std::string(report.flow_profile.has_check ? "true" : "false") +
                  " report=" + std::string(report.flow_profile.has_report ? "true" : "false"));

  lines.push_back("[SEMANTICS] decl=" + std::string(report.basic_semantics.has_declaration ? "true" : "false") +
                  " assign=" + std::string(report.basic_semantics.has_assignment ? "true" : "false") +
                  " expr=" + std::string(report.basic_semantics.has_expression_stmt ? "true" : "false") +
                  " call=" + std::string(report.basic_semantics.has_call_stmt ? "true" : "false") +
                  " if=" + std::string(report.basic_semantics.has_if_block ? "true" : "false") +
                  " check_call=" + std::string(report.basic_semantics.has_check_call ? "true" : "false") +
                  " print_call=" + std::string(report.basic_semantics.has_print_call ? "true" : "false") +
                  " register_class=" + std::string(report.basic_semantics.has_register_class ? "true" : "false") +
                  " register_fun=" + std::string(report.basic_semantics.has_register_fun ? "true" : "false"));

  lines.push_back("[BINDING] required=" + std::string(report.binding_semantics.requires_registered_binding ? "true" : "false") +
                  " object=" + std::string(report.binding_semantics.uses_object_binding ? "true" : "false") +
                  " registration=" + std::string(report.binding_semantics.uses_explicit_registration ? "true" : "false") +
                  " scope=" + report.binding_semantics.binding_scope);

  lines.push_back("[LAYERS] source=" + std::string(report.layer_profile.has_source_text ? "true" : "false") +
                  " normalized=" + std::string(report.layer_profile.has_normalized_text ? "true" : "false") +
                  " ir=" + std::string(report.layer_profile.has_linear_ir ? "true" : "false") +
                  " bridge=" + std::string(report.layer_profile.has_compile_bridge ? "true" : "false") +
                  " execution=" + std::string(report.layer_profile.has_execution_text ? "true" : "false") +
                  " kind=" + report.layer_profile.execution_text_kind +
                  " safe=" + std::string(report.layer_profile.bridge_exec_safe ? "true" : "false") +
                  " subset=" + report.layer_profile.bridge_exec_subset +
                  " reason=" + report.layer_profile.bridge_exec_reason +
                  " fallback=" + report.layer_profile.fallback_reason);

  lines.push_back("[IR] valid=" + std::string(report.ir_valid ? "true" : "false") +
                  " ops=" + std::to_string(report.ir_op_count) +
                  " stmts=" + std::to_string(report.ir_stmt_count) +
                  " blocks=" + std::to_string(report.ir_block_count) +
                  " error=" + report.ir_error_message);

  if (!report.bridge_summary.empty())
    lines.push_back("[BRIDGE] " + report.bridge_summary);

  for (size_t i = 0; i < report.bridge_point_lines.size(); ++i)
    lines.push_back("[BRIDGE_POINT] " + report.bridge_point_lines[i]);

  lines.push_back("[EXEC] accepted=" + std::to_string(report.accepted_task_count) +
                  " executed=" + std::to_string(report.executed_task_count));

  if (report.last_sequence > 0 || !report.failure_phase.empty())
  {
    lines.push_back("[EXEC_TRACE] last_step=" + std::to_string(report.last_step_id) +
                    " last_frame=" + std::to_string(report.last_frame_id) +
                    " last_seq=" + std::to_string(report.last_sequence) +
                    " last_line=" + std::to_string(report.last_source_line) +
                    " failure_step=" + std::to_string(report.failure_step_id) +
                    " failure_frame=" + std::to_string(report.failure_frame_id) +
                    " failure_seq=" + std::to_string(report.failure_sequence) +
                    " failure_line=" + std::to_string(report.failure_line) +
                    " phase=" + report.failure_phase);
  }

  lines.push_back("[MODULES] " + JoinModules(report.modules));

  if (!report.fragment_ids.empty())
  {
    lines.push_back("[FRAGMENTS] " + JoinValues(report.fragment_ids));
  }

  if (!report.bundle_ids.empty())
  {
    lines.push_back("[BUNDLES] " + JoinValues(report.bundle_ids));
  }

  if (!report.workflow_path_groups.empty())
  {
    lines.push_back("[WORKFLOW-PATHS] " + JoinValues(report.workflow_path_groups));
  }

  if (!report.multimodal_slices.empty())
    lines.push_back("[SLICES] count=" + std::to_string(report.multimodal_slices.size()));

  if (!report.operation_atoms.empty())
    lines.push_back("[ATOMS] count=" + std::to_string(report.operation_atoms.size()));

  lines.push_back("[REPLAY] count=" + std::to_string(report.replay_count) +
                  " source=" + report.replay_source_task_id +
                  " stages=" + std::to_string(report.replay_stage_count));

  if (report.baseline_snapshot.defined)
  {
    lines.push_back("[OPT-BASELINE] objective=" + std::to_string(report.baseline_snapshot.objective) +
                    " primary=" + FormatMetricPair(report.baseline_snapshot.primary_metric_name,
                                                  report.baseline_snapshot.primary_metric_value) +
                    " stability=" + FormatMetricPair(report.baseline_snapshot.stability_metric_name,
                                                     report.baseline_snapshot.stability_metric_value));
  }

  if (report.best_snapshot.defined)
  {
    lines.push_back("[OPT-BEST] objective=" + std::to_string(report.best_snapshot.objective) +
                    " primary=" + FormatMetricPair(report.best_snapshot.primary_metric_name,
                                                  report.best_snapshot.primary_metric_value) +
                    " stability=" + FormatMetricPair(report.best_snapshot.stability_metric_name,
                                                     report.best_snapshot.stability_metric_value));
  }

  if (report.optimization_compare.defined)
  {
    lines.push_back("[OPT-COMPARE] objective_delta_abs=" + std::to_string(report.optimization_compare.objective_delta_abs) +
                    " objective_delta_ratio=" + std::to_string(report.optimization_compare.objective_delta_ratio) +
                    " primary_delta=" + std::to_string(report.optimization_compare.primary_metric_delta) +
                    " stability_delta=" + std::to_string(report.optimization_compare.stability_delta) +
                    " eval_count=" + std::to_string(report.optimization_compare.eval_count) +
                    " converged=" + std::string(report.optimization_compare.converged ? "true" : "false") +
                    " stop_reason=" + report.optimization_compare.stop_reason);
    if (!report.optimization_compare.replay_log_path.empty())
    {
      lines.push_back("[OPT-REPLAY] object=" + report.replay_result_object +
                      " replay_log_path=" + report.optimization_compare.replay_log_path +
                      " pass_level=" + report.optimization_compare.pass_level);
    }
  }
}

bool IsCxscriptCompileBridgeEligible(const CxscriptExecutionResult &summary,
                                     const std::string &script_origin,
                                     bool has_catalog_fallback)
{
  return script_origin == "file" &&
         summary.ir_valid &&
         !summary.compile_text.empty() &&
         (summary.flow_profile.script_style == "call_style" ||
          summary.layer_profile.bridge_exec_safe) &&
         !has_catalog_fallback;
}

bool ShouldUseCatalogFallback(const CxscriptExecutionResult &summary,
                              const std::string &script_origin,
                              bool has_catalog_fallback)
{
  return script_origin == "file" &&
         has_catalog_fallback &&
         !IsCxscriptCompileBridgeEligible(summary, script_origin, has_catalog_fallback);
}

std::string DescribeCxscriptFallbackReason(const CxscriptExecutionResult &summary,
                                           const std::string &script_origin,
                                           bool has_catalog_fallback)
{
  if (!ShouldUseCatalogFallback(summary, script_origin, has_catalog_fallback))
    return std::string();

  if (!summary.ir_valid)
    return "ir_invalid";

  if (summary.compile_text.empty())
    return "compile_bridge_unavailable";

  if (summary.flow_profile.script_style != "call_style" &&
      !summary.layer_profile.bridge_exec_safe)
    return "bridge_subset_not_safe";

  return "catalog_fallback_required";
}
}
