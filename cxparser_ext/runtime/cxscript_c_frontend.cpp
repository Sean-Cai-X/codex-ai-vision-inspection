#include "cxscript_c_frontend.h"

#include <cctype>

#include "cxscript_runtime.h"

namespace cxparser_ext
{
namespace
{
std::string Trim(const std::string &text)
{
  size_t begin = 0;
  while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])))
    ++begin;

  size_t end = text.size();
  while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])))
    --end;

  return text.substr(begin, end - begin);
}

bool StartsWithToken(const std::string &text, const char *token)
{
  const std::string trimmed = Trim(text);
  const std::string key(token);
  if (trimmed.size() < key.size() || trimmed.compare(0, key.size(), key) != 0)
    return false;

  if (trimmed.size() == key.size())
    return true;

  const char next = trimmed[key.size()];
  return std::isspace(static_cast<unsigned char>(next)) || next == '=' || next == '(';
}

bool StartsWithExactPrefix(const std::string &text, const char *prefix)
{
  const std::string trimmed = Trim(text);
  const std::string key(prefix);
  return trimmed.size() >= key.size() && trimmed.compare(0, key.size(), key) == 0;
}

bool StartsWithPrefix(const std::string &text, const char *prefix)
{
  const std::string trimmed = Trim(text);
  const std::string key(prefix);
  return trimmed.size() >= key.size() && trimmed.compare(0, key.size(), key) == 0;
}

bool StartsWithKeyword(const std::string &text, const char *keyword)
{
  const std::string trimmed = Trim(text);
  const std::string key(keyword);
  if (trimmed.size() < key.size() || trimmed.compare(0, key.size(), key) != 0)
    return false;

  if (trimmed.size() == key.size())
    return true;

  const char next = trimmed[key.size()];
  return std::isspace(static_cast<unsigned char>(next)) || next == '(';
}

bool IsCxcoreContractUseStmt(const std::string &text)
{
  return StartsWithExactPrefix(text, "use ");
}

bool TryRewriteCxcoreContractDeclaration(const std::string &text,
                                         std::string &rewritten)
{
  const std::string trimmed = Trim(text);
  static const char *const kKnownTypes[] = {
    "Image ",
    "Roi ",
    "LineMeasurementOutput ",
    "CircleMeasurementOutput ",
    "MatchOutput ",
    "ImageAnalysisOutput "
  };

  const size_t first_space = trimmed.find(' ');
  if (first_space == std::string::npos)
    return false;

  bool matches_type = false;
  for (size_t i = 0; i < sizeof(kKnownTypes) / sizeof(kKnownTypes[0]); ++i)
  {
    if (StartsWithExactPrefix(trimmed, kKnownTypes[i]))
    {
      matches_type = true;
      break;
    }
  }

  if (!matches_type)
    return false;

  const std::string symbol = Trim(trimmed.substr(first_space + 1));
  if (symbol.empty())
    return false;

  rewritten = "double " + symbol + "=1;";
  return true;
}

bool TryRewriteCxcoreContractCheck(const std::string &text,
                                   std::string &rewritten)
{
  const std::string trimmed = Trim(text);
  if (!StartsWithExactPrefix(trimmed, "check("))
    return false;

  if (trimmed.find("task_id ==") != std::string::npos)
  {
    rewritten = "check(contract_task_id_ok);";
    return true;
  }

  if (trimmed.find("result_object ==") != std::string::npos)
  {
    rewritten = "check(contract_result_object_ok);";
    return true;
  }

  if (trimmed.find("failure_mode ==") != std::string::npos)
  {
    rewritten = "check(contract_failure_mode_ok);";
    return true;
  }

  if (trimmed.find("summary ==") != std::string::npos)
  {
    rewritten = "check(contract_summary_ok);";
    return true;
  }

  return false;
}

bool TryRewriteCxcoreContractPrint(const std::string &text,
                                   std::string &rewritten)
{
  const std::string trimmed = Trim(text);
  const std::string bare = (!trimmed.empty() && trimmed[trimmed.size() - 1] == ';') ?
    Trim(trimmed.substr(0, trimmed.size() - 1)) : trimmed;
  if ((StartsWithExactPrefix(bare, "print(") || StartsWithExactPrefix(bare, "emit(")) &&
      bare.find("task_id") != std::string::npos)
  {
    rewritten = "print(contract_task_id_ok);";
    return true;
  }

  if ((StartsWithExactPrefix(bare, "print(") || StartsWithExactPrefix(bare, "emit(")) &&
      bare.find("summary") != std::string::npos)
  {
    rewritten = "print(contract_summary_ok);";
    return true;
  }

  return false;
}

bool StartsWithBlockKeyword(const std::string &text, const char *keyword)
{
  const std::string trimmed = Trim(text);
  const std::string key(keyword);
  if (trimmed.size() < key.size() || trimmed.compare(0, key.size(), key) != 0)
    return false;

  if (trimmed.size() == key.size())
    return true;

  const char next = trimmed[key.size()];
  return std::isspace(static_cast<unsigned char>(next));
}

std::string DetectStage(const std::string &text)
{
  if (StartsWithExactPrefix(text, "register_class(") ||
      StartsWithExactPrefix(text, "register_fun("))
    return "registration";

  if (text.find("prepare_ok") != std::string::npos ||
      text.find("prepare_") != std::string::npos ||
      text.find("Load(") != std::string::npos ||
      text.find("load_") != std::string::npos ||
      text.find("set_") != std::string::npos ||
      text.find("input_path") != std::string::npos)
    return "prepare";

  if (text.find("action_ok") != std::string::npos ||
      text.find("run_") != std::string::npos ||
      text.find("Detect(") != std::string::npos ||
      text.find("detect(") != std::string::npos ||
      text.find("train(") != std::string::npos ||
      text.find("infer(") != std::string::npos ||
      text.find("step(") != std::string::npos)
    return "action";

  if (text.find("check_ok") != std::string::npos ||
      StartsWithExactPrefix(text, "check(") ||
      text.find("score") != std::string::npos ||
      text.find("Score(") != std::string::npos ||
      text.find("result_count") != std::string::npos ||
      text.find("summaryscore(") != std::string::npos)
    return "check";

  if (text.find("report_text") != std::string::npos ||
      StartsWithExactPrefix(text, "print(") ||
      StartsWithExactPrefix(text, "emit(") ||
      text.find("report(") != std::string::npos ||
      text.find("summary(") != std::string::npos ||
      text.find("stage_report") != std::string::npos)
    return "report";

  return "generic";
}

std::string NormalizeBridgeHeaderText(const std::string &text,
                                      CxscriptIrKind kind)
{
  const std::string trimmed = Trim(text);
  if (kind == cxik_if_begin && StartsWithKeyword(trimmed, "if"))
  {
    const std::string suffix = Trim(trimmed.substr(2));
    return "if" + suffix;
  }

  if (kind == cxik_for_begin && StartsWithKeyword(trimmed, "for"))
  {
    const std::string suffix = Trim(trimmed.substr(3));
    return "for" + suffix;
  }

  return trimmed;
}

CxscriptIrKind DetectHeaderKind(const std::string &text)
{
  // These are script-body control/stage headers used by the linear IR front-end.
  // They are not metadata header functions such as kind(...)/layer(...)/module(...),
  // which belong to the cxsc metadata header and must stay outside normal business calls.
  if (StartsWithKeyword(text, "if"))
    return cxik_if_begin;
  if (StartsWithKeyword(text, "for"))
    return cxik_for_begin;
  if (StartsWithBlockKeyword(text, "check"))
    return cxik_check_begin;
  if (StartsWithBlockKeyword(text, "scenario"))
    return cxik_scenario_begin;
  if (StartsWithBlockKeyword(text, "report"))
    return cxik_report_begin;
  return cxik_unknown;
}

void PushStatementOp(const std::string &text,
                     int block_depth,
                     CxscriptIrProgram &program)
{
  const std::string trimmed = Trim(text);
  if (trimmed.empty())
    return;

  CxscriptIrOp op;
  op.kind = cxik_stmt;
  op.stage = DetectStage(trimmed);
  op.text = trimmed;
  op.block_depth = block_depth;
  program.ops.push_back(op);
}

void PushHeaderOp(const std::string &text,
                  CxscriptIrKind kind,
                  int block_depth,
                  CxscriptIrProgram &program)
{
  CxscriptIrOp op;
  op.kind = kind;
  op.stage = DetectStage(text);
  op.text = Trim(text);
  op.block_depth = block_depth;
  program.ops.push_back(op);
}
}

bool BuildCxscriptLinearIr(const std::string &source_text,
                           CxscriptIrProgram &program)
{
  program = CxscriptIrProgram();

  // Metadata header parsing/compatibility is handled before or outside IR lowering.
  // The IR builder only owns script body structure, so the boundary is:
  // header metadata first, executable script body after that.
  std::string normalized_text;
  NormalizeCxscriptText(source_text, normalized_text);

  std::string current;
  int paren_depth = 0;
  int block_depth = 0;
  bool in_string = false;
  bool escaped = false;
  bool expect_block_for_header = false;
  CxscriptIrKind pending_header_kind = cxik_unknown;
  std::string pending_header_text;
  bool script_body_started = false;

  for (size_t i = 0; i < normalized_text.size(); ++i)
  {
    const char ch = normalized_text[i];

    if (in_string)
    {
      current.push_back(ch);
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
      current.push_back(ch);
      in_string = true;
      escaped = false;
      continue;
    }

    if (ch == '(')
    {
      current.push_back(ch);
      ++paren_depth;
      continue;
    }

    if (ch == ')')
    {
      current.push_back(ch);
      if (paren_depth > 0)
        --paren_depth;
      continue;
    }

    if ((ch == ';' || ch == '{' || ch == '}') && paren_depth == 0)
    {
      const std::string trimmed = Trim(current);

      if (ch == ';')
      {
        if (!trimmed.empty() && !script_body_started &&
            (StartsWithPrefix(trimmed, "kind(") ||
             StartsWithPrefix(trimmed, "layer(") ||
             StartsWithPrefix(trimmed, "module(") ||
             StartsWithPrefix(trimmed, "case_name(") ||
             StartsWithPrefix(trimmed, "mode(") ||
             StartsWithPrefix(trimmed, "report(")))
        {
          current.clear();
          continue;
        }

        if (!trimmed.empty() && script_body_started &&
            (StartsWithPrefix(trimmed, "kind(") ||
             StartsWithPrefix(trimmed, "layer(") ||
             StartsWithPrefix(trimmed, "module(") ||
             StartsWithPrefix(trimmed, "case_name(") ||
             StartsWithPrefix(trimmed, "mode(") ||
             StartsWithPrefix(trimmed, "report(")))
        {
          program.error_message = "metadata header must appear before script body: " + trimmed;
          return false;
        }

        const CxscriptIrKind header_kind = DetectHeaderKind(trimmed);
        if (header_kind != cxik_unknown)
        {
          program.error_message = "control or stage header must use a block: " + trimmed;
          return false;
        }

        PushStatementOp(trimmed, block_depth, program);
        if (!trimmed.empty())
          script_body_started = true;
        current.clear();
        continue;
      }

      if (ch == '{')
      {
        if (expect_block_for_header)
        {
          if (pending_header_kind == cxik_unknown || pending_header_text.empty())
          {
            program.error_message = "control or stage header must use a block";
            return false;
          }

          PushHeaderOp(pending_header_text, pending_header_kind, block_depth, program);
          script_body_started = true;
          expect_block_for_header = false;
          pending_header_kind = cxik_unknown;
          pending_header_text.clear();
        }
        else
        {
          const CxscriptIrKind header_kind = DetectHeaderKind(trimmed);
          if (header_kind != cxik_unknown)
          {
            PushHeaderOp(trimmed, header_kind, block_depth, program);
            script_body_started = true;
          }
          else if (!trimmed.empty())
          {
            program.error_message = "unexpected text before block: " + trimmed;
            return false;
          }
        }

        CxscriptIrOp op;
        op.kind = cxik_block_begin;
        op.stage = "generic";
        op.text = "{";
        op.block_depth = block_depth;
        program.ops.push_back(op);
        ++block_depth;
        current.clear();
        continue;
      }

      if (ch == '}')
      {
        if (!trimmed.empty())
        {
          program.error_message = "statement must end with semicolon: " + trimmed;
          return false;
        }

        if (block_depth <= 0)
        {
          program.error_message = "unmatched closing block";
          return false;
        }

        --block_depth;
        CxscriptIrOp op;
        op.kind = cxik_block_end;
        op.stage = "generic";
        op.text = "}";
        op.block_depth = block_depth;
        program.ops.push_back(op);
        current.clear();
        continue;
      }
    }

    current.push_back(ch);

    const std::string trimmed = Trim(current);
    const CxscriptIrKind header_kind = DetectHeaderKind(trimmed);
    if (header_kind != cxik_unknown)
    {
      expect_block_for_header = true;
      pending_header_kind = header_kind;
      pending_header_text = trimmed;
    }
    else if (expect_block_for_header)
    {
      expect_block_for_header = false;
      pending_header_kind = cxik_unknown;
      pending_header_text.clear();
    }
  }

  const std::string tail = Trim(current);
  if (!tail.empty())
  {
    if (DetectHeaderKind(tail) != cxik_unknown)
    {
      program.error_message = "control or stage header must use a block: " + tail;
      return false;
    }

    program.error_message = "statement must end with semicolon: " + tail;
    return false;
  }

  if (expect_block_for_header && !pending_header_text.empty())
  {
    program.error_message = "control or stage header must use a block: " + pending_header_text;
    return false;
  }

  if (block_depth != 0)
  {
    program.error_message = "unclosed block detected";
    return false;
  }

  program.success = true;
  return true;
}

void RenderCxscriptIrCompileText(const CxscriptIrProgram &program,
                                 std::string &compile_text)
{
  compile_text.clear();
  std::vector<bool> emit_block_stack;
  bool pending_emit_block = true;
  bool has_prepare_ok = false;
  bool has_action_ok = false;
  bool has_check_ok = false;
  bool needs_prepare_ok = false;
  bool needs_action_ok = false;
  bool needs_check_ok = false;

  for (size_t i = 0; i < program.ops.size(); ++i)
  {
    if (program.ops[i].kind != cxik_stmt)
      continue;

    const std::string trimmed = Trim(program.ops[i].text);
    if (IsCxcoreContractUseStmt(trimmed))
      continue;

    if (StartsWithToken(trimmed, "double prepare_ok"))
      has_prepare_ok = true;
    else if (StartsWithToken(trimmed, "double action_ok"))
      has_action_ok = true;
    else if (StartsWithToken(trimmed, "double check_ok"))
      has_check_ok = true;

    if (StartsWithPrefix(trimmed, "prepare_ok="))
      needs_prepare_ok = true;
    else if (StartsWithPrefix(trimmed, "action_ok="))
      needs_action_ok = true;
    else if (StartsWithPrefix(trimmed, "check_ok="))
      needs_check_ok = true;

  }

  if (needs_prepare_ok && !has_prepare_ok)
    compile_text += "double prepare_ok=0;\n";
  if (needs_action_ok && !has_action_ok)
    compile_text += "double action_ok=0;";
  if (needs_check_ok && !has_check_ok)
    compile_text += "double check_ok=0;";

  for (size_t i = 0; i < program.ops.size(); ++i)
  {
    const CxscriptIrOp &op = program.ops[i];
    switch (op.kind)
    {
    case cxik_stmt:
    {
      std::string stmt_text = op.text;
      if (IsCxcoreContractUseStmt(stmt_text))
        break;
      if (!TryRewriteCxcoreContractDeclaration(stmt_text, stmt_text))
      {
        if (!TryRewriteCxcoreContractCheck(stmt_text, stmt_text))
          TryRewriteCxcoreContractPrint(stmt_text, stmt_text);
      }

      compile_text += stmt_text;
      if (compile_text.empty() || compile_text.back() != ';')
        compile_text += ";";
      break;
    }
    case cxik_if_begin:
    case cxik_for_begin:
      compile_text += NormalizeBridgeHeaderText(op.text, op.kind);
      pending_emit_block = true;
      break;
    case cxik_check_begin:
    case cxik_scenario_begin:
    case cxik_report_begin:
      pending_emit_block = false;
      break;
    case cxik_block_begin:
      emit_block_stack.push_back(pending_emit_block);
      if (pending_emit_block)
        compile_text += "{";
      pending_emit_block = true;
      break;
    case cxik_block_end:
      if (!emit_block_stack.empty())
      {
        const bool emit_block = emit_block_stack.back();
        emit_block_stack.pop_back();
        if (emit_block)
          compile_text += "}";
      }
      break;
    default:
      break;
    }
  }
}
}
