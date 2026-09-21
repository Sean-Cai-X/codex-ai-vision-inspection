#include "parser_analysis_bridge_exporter.h"

namespace cxparser_ext
{
namespace
{
std::string OriginText(ParserAnalysisBridgeOrigin origin)
{
  switch (origin)
  {
  case pabo_flow:
    return "flow";
  case pabo_execution:
    return "execution";
  case pabo_schema:
    return "schema";
  case pabo_unknown:
  default:
    return "unknown";
  }
}

EvidenceEventLevel ToEvidenceLevel(ParserAnalysisBridgeSeverity severity)
{
  switch (severity)
  {
  case pabs_error:
    return eel_error;
  case pabs_warning:
    return eel_warning;
  case pabs_info:
  default:
    return eel_info;
  }
}

EvidenceEventLevel ResolveCallEvidenceLevel(const ParserAnalysisBridgeCall &call)
{
  if (!call.matched && call.severity == pabs_info)
    return eel_warning;
  return ToEvidenceLevel(call.severity);
}

std::string ResolveStage(const ParserAnalysisBridgeResult &bridge_result,
                         const std::string &item_stage)
{
  if (!item_stage.empty())
    return item_stage;
  if (!bridge_result.bridge_stage.empty())
    return bridge_result.bridge_stage;
  return bridge_result.bridge_name + "_bridge";
}

std::string ResolveCategory(const ParserAnalysisBridgeResult &bridge_result,
                            const std::string &item_category)
{
  if (!item_category.empty())
    return item_category;
  return bridge_result.category;
}

std::string ResolveBridgeExpectation(const ParserAnalysisBridgeResult &bridge_result,
                                     const std::string &item_category)
{
  const std::string category = ResolveCategory(bridge_result, item_category);
  std::string expectation = "call should map through analysis bridge";
  if (!category.empty())
    expectation += " (" + category + ")";

  switch (bridge_result.status)
  {
  case pabs_resolved:
    expectation += " [resolved]";
    break;
  case pabs_partial:
    expectation += " [partial]";
    break;
  case pabs_unresolved:
    expectation += " [unresolved]";
    break;
  case pabs_unknown:
  default:
    break;
  }

  return expectation;
}
}

bool ParserAnalysisBridgeExporter::BuildEvidence(const ParserAnalysisBridgeResult &bridge_result,
                                                 ParserEvidenceBundle &bundle) const
{
  if (!bridge_result.summary.empty())
    bundle.notes.push_back(bridge_result.bridge_name + " summary: " + bridge_result.summary);

  if (!bridge_result.reason.empty())
    bundle.notes.push_back(bridge_result.bridge_name + " reason: " + bridge_result.reason);

  bundle.notes.push_back(bridge_result.bridge_name + " origin: " + OriginText(bridge_result.origin));

  bundle.notes.push_back(bridge_result.bridge_name + " metrics: matched_calls=" +
                         std::to_string(bridge_result.metrics.matched_call_count) +
                         " unresolved_calls=" +
                         std::to_string(bridge_result.metrics.unresolved_call_count) +
                         " steps=" + std::to_string(bridge_result.metrics.step_count) +
                         " points=" + std::to_string(bridge_result.metrics.point_count));

  for (size_t i = 0; i < bridge_result.calls.size(); ++i)
  {
    const ParserAnalysisBridgeCall &call = bridge_result.calls[i];

    EvidenceEvent event;
    event.level = ResolveCallEvidenceLevel(call);
    event.stage = ResolveStage(bridge_result, call.bridge_stage);
    event.code = call.matched
      ? (bridge_result.bridge_name + "_call_matched")
      : (bridge_result.bridge_name + "_call_unresolved");
    event.message = call.matched
      ? ("matched call " + call.call_name + " -> " + call.matched_symbol)
      : ("unresolved call " + call.call_name);
    event.expected = ResolveBridgeExpectation(bridge_result, call.category);
    event.actual = call.matched ? call.matched_symbol : call.call_name;
    bundle.events.push_back(event);
  }

  for (size_t i = 0; i < bridge_result.steps.size(); ++i)
  {
    const ParserAnalysisBridgeStep &step = bridge_result.steps[i];
    const std::string category = ResolveCategory(bridge_result, step.category);
    bundle.notes.push_back(step.matched
      ? ("bridge step " + step.step_name + " -> " + step.matched_symbol +
         (category.empty() ? std::string() : (" [" + category + "]")))
      : ("bridge unresolved step " + step.step_name +
         (category.empty() ? std::string() : (" [" + category + "]"))));
  }

  for (size_t i = 0; i < bridge_result.points.size(); ++i)
  {
    const ParserAnalysisBridgePoint &point = bridge_result.points[i];
    const std::string category = ResolveCategory(bridge_result, point.category);
    std::string note = bridge_result.bridge_name + " point " + point.point_kind +
                       " " + point.point_name + " -> " + point.related_symbol;
    if (point.matched)
      note += " [matched]";
    else
      note += " [unresolved]";
    if (!point.step_name.empty())
      note += " step=" + point.step_name;
    if (point.step_id > 0)
      note += " step_id=" + std::to_string(point.step_id);
    if (point.frame_id > 0)
      note += " frame_id=" + std::to_string(point.frame_id);
    if (point.span.line_begin > 0)
      note += " line=" + std::to_string(point.span.line_begin);
    if (point.block_depth > 0)
      note += " depth=" + std::to_string(point.block_depth);
    if (!category.empty())
      note += " [" + category + "]";
    bundle.notes.push_back(note);
  }

  for (size_t i = 0; i < bridge_result.notes.size(); ++i)
    bundle.notes.push_back(bridge_result.notes[i]);

  return true;
}
}
