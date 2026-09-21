#include "parser_ir_radare_matcher.h"

namespace cxparser_ext
{
namespace
{
std::string Trim(const std::string &text)
{
  size_t begin = 0;
  while (begin < text.size() &&
         (text[begin] == ' ' || text[begin] == '\t' || text[begin] == '\r' || text[begin] == '\n'))
    ++begin;

  size_t end = text.size();
  while (end > begin &&
         (text[end - 1] == ' ' || text[end - 1] == '\t' || text[end - 1] == '\r' || text[end - 1] == '\n'))
    --end;

  return text.substr(begin, end - begin);
}

std::string ExtractCallName(const std::string &call_text)
{
  const size_t pos = call_text.find('(');
  if (pos == std::string::npos)
    return Trim(call_text);
  return Trim(call_text.substr(0, pos));
}

bool ContainsSymbol(const RadareAnalysisResult &analysis,
                    const std::string &symbol_name,
                    std::string &matched_symbol)
{
  matched_symbol.clear();

  if (analysis.current_symbol == symbol_name)
  {
    matched_symbol = analysis.current_symbol;
    return true;
  }

  for (size_t i = 0; i < analysis.functions.size(); ++i)
  {
    const RadareFunctionInfo &fn = analysis.functions[i];
    if (fn.name == symbol_name)
    {
      matched_symbol = fn.name;
      return true;
    }

    for (size_t j = 0; j < fn.call_targets.size(); ++j)
    {
      if (fn.call_targets[j] == symbol_name)
      {
        matched_symbol = fn.call_targets[j];
        return true;
      }
    }
  }

  return false;
}

void AddEvidencePoints(const CxScriptFlow &flow,
                       const std::string &default_symbol,
                       ParserIrRadareMatchResult &result)
{
  for (size_t i = 0; i < flow.statements.size(); ++i)
  {
    const CxScriptStatement &stmt = flow.statements[i];
    if (stmt.kind != cxssk_breakpoint && stmt.kind != cxssk_checkpoint)
      continue;

    ParserIrRadareEvidencePoint point;
    point.point_kind = stmt.kind == cxssk_breakpoint ? "breakpoint" : "checkpoint";
    point.point_name = stmt.name;
    point.step_name = stmt.step_name;
    point.related_symbol = default_symbol;
    result.evidence_points.push_back(point);
    result.notes.push_back(point.point_kind + " note: " + point.point_name + " -> " + point.related_symbol);
  }
}

void FillBridgeMetrics(ParserAnalysisBridgeResult &bridge_result)
{
  bridge_result.metrics = ParserAnalysisBridgeMetrics();
  for (size_t i = 0; i < bridge_result.calls.size(); ++i)
  {
    if (bridge_result.calls[i].matched)
      ++bridge_result.metrics.matched_call_count;
    else
      ++bridge_result.metrics.unresolved_call_count;
  }
  bridge_result.metrics.step_count = static_cast<int>(bridge_result.steps.size());
  bridge_result.metrics.point_count = static_cast<int>(bridge_result.points.size());
}

void PopulateBridgeResult(const ParserIrRadareMatchResult &match_result,
                          ParserAnalysisBridgeOrigin origin,
                          const std::string &summary_resolved,
                          const std::string &summary_partial,
                          const std::string &reason_resolved,
                          const std::string &reason_partial,
                          ParserAnalysisBridgeResult &bridge_result)
{
  bridge_result = ParserAnalysisBridgeResult();
  bridge_result.bridge_name = "radare";
  bridge_result.bridge_stage = "radare_bridge";
  bridge_result.category = "binary_analysis";
  bridge_result.severity = pabs_info;
  bridge_result.status = match_result.unresolved_calls.empty() ? pabs_resolved : pabs_partial;
  bridge_result.origin = origin;
  bridge_result.summary = match_result.unresolved_calls.empty() ? summary_resolved : summary_partial;
  bridge_result.reason = match_result.unresolved_calls.empty() ? reason_resolved : reason_partial;

  for (size_t i = 0; i < match_result.matched_calls.size(); ++i)
  {
    const ParserIrRadareMatch &match = match_result.matched_calls[i];
    ParserAnalysisBridgeCall call;
    call.step_name = match.step_name;
    call.call_name = match.call_name;
    call.matched_symbol = match.matched_symbol;
    call.matched = match.matched;
    call.severity = match.matched ? pabs_info : pabs_warning;
    call.category = "call_match";
    call.bridge_stage = bridge_result.bridge_stage;
    bridge_result.calls.push_back(call);
  }

  for (size_t i = 0; i < match_result.matched_steps.size(); ++i)
  {
    const ParserIrRadareStepMatch &match = match_result.matched_steps[i];
    ParserAnalysisBridgeStep step;
    step.step_name = match.step_name;
    step.matched_symbol = match.matched_symbol;
    step.matched = match.matched;
    step.severity = match.matched ? pabs_info : pabs_warning;
    step.category = "step_match";
    step.bridge_stage = bridge_result.bridge_stage;
    bridge_result.steps.push_back(step);
  }

  for (size_t i = 0; i < match_result.evidence_points.size(); ++i)
  {
    const ParserIrRadareEvidencePoint &point = match_result.evidence_points[i];
    ParserAnalysisBridgePoint bridge_point;
    bridge_point.point_kind = point.point_kind;
    bridge_point.point_name = point.point_name;
    bridge_point.step_name = point.step_name;
    bridge_point.related_symbol = point.related_symbol;
    bridge_point.severity = pabs_info;
    bridge_point.category = "evidence_point";
    bridge_point.bridge_stage = bridge_result.bridge_stage;
    bridge_result.points.push_back(bridge_point);
  }

  bridge_result.unresolved_calls = match_result.unresolved_calls;
  bridge_result.notes = match_result.notes;
  FillBridgeMetrics(bridge_result);
}
}

bool ParserIrRadareMatcher::MatchFlow(const CxScriptFlow &flow,
                                      const RadareAnalysisResult &analysis,
                                      ParserIrRadareMatchResult &result) const
{
  result = ParserIrRadareMatchResult();
  const std::string default_symbol = !analysis.current_symbol.empty()
    ? analysis.current_symbol
    : (analysis.functions.empty() ? std::string() : analysis.functions[0].name);

  for (size_t i = 0; i < flow.statements.size(); ++i)
  {
    const CxScriptStatement &stmt = flow.statements[i];
    if (stmt.kind != cxssk_step)
      continue;

    ParserIrRadareStepMatch step_match;
    step_match.step_name = stmt.step_name.empty() ? stmt.name : stmt.step_name;
    step_match.matched_symbol = default_symbol;
    step_match.matched = !default_symbol.empty();
    result.matched_steps.push_back(step_match);

    if (step_match.matched)
      result.notes.push_back("matched step: " + step_match.step_name + " -> " + step_match.matched_symbol);
    else
      result.notes.push_back("unmatched step: " + step_match.step_name);
  }

  for (size_t i = 0; i < flow.statements.size(); ++i)
  {
    const CxScriptStatement &stmt = flow.statements[i];
    if (stmt.kind != cxssk_call)
      continue;

    ParserIrRadareMatch match;
    match.step_name = stmt.step_name;
    match.call_name = stmt.callee_name.empty() ? stmt.name : stmt.callee_name;

    std::string matched_symbol;
    match.matched = ContainsSymbol(analysis, match.call_name, matched_symbol);
    match.matched_symbol = matched_symbol;
    result.matched_calls.push_back(match);

    if (!match.matched)
    {
      result.unresolved_calls.push_back(match.call_name);
      result.notes.push_back("unresolved call: " + match.call_name);
    }
    else
    {
      result.notes.push_back("matched call: " + match.call_name + " -> " + matched_symbol);
    }
  }

  AddEvidencePoints(flow, default_symbol, result);

  return true;
}

bool ParserIrRadareMatcher::MatchExecution(const CxScriptExecutionResult &execution,
                                           const RadareAnalysisResult &analysis,
                                           ParserIrRadareMatchResult &result) const
{
  result = ParserIrRadareMatchResult();
  const std::string default_symbol = !analysis.current_symbol.empty()
    ? analysis.current_symbol
    : (analysis.functions.empty() ? std::string() : analysis.functions[0].name);

  for (size_t i = 0; i < execution.execution_ops.size(); ++i)
  {
    const CxScriptExecutionOp &op = execution.execution_ops[i];

    if (op.opcode == cxseo_step_enter)
    {
      ParserIrRadareStepMatch step_match;
      step_match.step_name = op.payload.empty() ? op.step_name : op.payload;
      step_match.matched_symbol = default_symbol;
      step_match.matched = !default_symbol.empty();
      result.matched_steps.push_back(step_match);

      if (step_match.matched)
        result.notes.push_back("matched exec step: " + step_match.step_name + " -> " + step_match.matched_symbol);
      else
        result.notes.push_back("unmatched exec step: " + step_match.step_name);
      continue;
    }

    if (op.opcode == cxseo_call)
    {
      ParserIrRadareMatch match;
      match.step_name = op.step_name;
      match.call_name = ExtractCallName(op.payload);

      std::string matched_symbol;
      match.matched = ContainsSymbol(analysis, match.call_name, matched_symbol);
      match.matched_symbol = matched_symbol;
      result.matched_calls.push_back(match);

      if (!match.matched)
      {
        result.unresolved_calls.push_back(match.call_name);
        result.notes.push_back("unresolved exec call: " + match.call_name);
      }
      else
      {
        result.notes.push_back("matched exec call: " + match.call_name + " -> " + matched_symbol);
      }
      continue;
    }

    if (op.opcode == cxseo_breakpoint || op.opcode == cxseo_checkpoint)
    {
      ParserIrRadareEvidencePoint point;
      point.point_kind = op.opcode == cxseo_breakpoint ? "breakpoint" : "checkpoint";
      point.point_name = op.payload;
      point.step_name = op.step_name;
      point.related_symbol = default_symbol;
      result.evidence_points.push_back(point);
      result.notes.push_back(point.point_kind + " exec note: " + point.point_name + " -> " + point.related_symbol);
    }
  }

  return true;
}

bool ParserIrRadareMatcher::BuildEvidence(const ParserIrRadareMatchResult &match_result,
                                          ParserEvidenceBundle &bundle) const
{
  for (size_t i = 0; i < match_result.matched_calls.size(); ++i)
  {
    const ParserIrRadareMatch &match = match_result.matched_calls[i];

    EvidenceEvent event;
    event.level = match.matched ? eel_info : eel_warning;
    event.stage = "radare_match";
    event.code = match.matched ? "radare_call_matched" : "radare_call_unresolved";
    event.message = match.matched
      ? ("matched call " + match.call_name + " -> " + match.matched_symbol)
      : ("unresolved call " + match.call_name);
    event.expected = "call should map to symbol";
    event.actual = match.matched ? match.matched_symbol : match.call_name;
    bundle.events.push_back(event);
  }

  for (size_t i = 0; i < match_result.matched_steps.size(); ++i)
  {
    const ParserIrRadareStepMatch &match = match_result.matched_steps[i];
    bundle.notes.push_back(match.matched
      ? ("matched step " + match.step_name + " -> " + match.matched_symbol)
      : ("unmatched step " + match.step_name));
  }

  for (size_t i = 0; i < match_result.evidence_points.size(); ++i)
  {
    const ParserIrRadareEvidencePoint &point = match_result.evidence_points[i];
    bundle.notes.push_back(point.point_kind + " " + point.point_name + " -> " + point.related_symbol);
  }

  for (size_t i = 0; i < match_result.notes.size(); ++i)
    bundle.notes.push_back(match_result.notes[i]);

  return true;
}

bool ParserIrRadareMatcher::BuildBridgeResult(const ParserIrRadareMatchResult &match_result,
                                              ParserAnalysisBridgeResult &bridge_result) const
{
  return BuildExecutionBridgeResult(match_result, bridge_result);
}

bool ParserIrRadareMatcher::BuildFlowBridgeResult(const ParserIrRadareMatchResult &match_result,
                                                  ParserAnalysisBridgeResult &bridge_result) const
{
  PopulateBridgeResult(match_result,
                       pabo_flow,
                       "radare flow bridge resolved all flow calls",
                       "radare flow bridge produced partial call coverage",
                       "all flow calls mapped to binary symbols",
                       "one or more flow calls could not be mapped to binary symbols",
                       bridge_result);
  return true;
}

bool ParserIrRadareMatcher::BuildExecutionBridgeResult(const ParserIrRadareMatchResult &match_result,
                                                       ParserAnalysisBridgeResult &bridge_result) const
{
  PopulateBridgeResult(match_result,
                       pabo_execution,
                       "radare execution bridge resolved all execution calls",
                       "radare execution bridge produced partial call coverage",
                       "all execution calls mapped to binary symbols",
                       "one or more execution calls could not be mapped to binary symbols",
                       bridge_result);
  return true;
}
}
