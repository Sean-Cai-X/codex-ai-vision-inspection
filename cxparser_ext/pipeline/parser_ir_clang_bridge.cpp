#include "parser_ir_clang_bridge.h"

namespace cxparser_ext
{
namespace
{
bool ContainsMethod(const ApiSchema &schema,
                    const std::string &method_name,
                    std::string &qualified_name)
{
  qualified_name.clear();

  for (size_t i = 0; i < schema.classes.size(); ++i)
  {
    const ClangClassInfo &cls = schema.classes[i];
    for (size_t j = 0; j < cls.methods.size(); ++j)
    {
      const ClangMethodInfo &method = cls.methods[j];
      if (method.name == method_name || method.qualified_name == method_name)
      {
        qualified_name = method.qualified_name.empty() ? method.name : method.qualified_name;
        return true;
      }
    }
  }

  return false;
}

bool ContainsType(const ApiSchema &schema,
                  const std::string &type_name,
                  std::string &qualified_name)
{
  qualified_name.clear();

  for (size_t i = 0; i < schema.classes.size(); ++i)
  {
    const ClangClassInfo &cls = schema.classes[i];
    if (cls.name == type_name || cls.qualified_name == type_name)
    {
      qualified_name = cls.qualified_name.empty() ? cls.name : cls.qualified_name;
      return true;
    }
  }

  return false;
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
}

bool ParserIrClangBridge::BuildSchemaBridge(const ApiSchema &schema,
                                            ParserAnalysisBridgeResult &bridge_result) const
{
  bridge_result = ParserAnalysisBridgeResult();
  bridge_result.bridge_name = "clang";
  bridge_result.bridge_stage = "clang_bridge";
  bridge_result.category = "schema_analysis";
  bridge_result.severity = pabs_info;
  bridge_result.status = schema.classes.empty() ? pabs_unresolved : pabs_resolved;
  bridge_result.origin = pabo_schema;
  bridge_result.summary = schema.classes.empty()
    ? "clang schema bridge found no classes"
    : "clang schema bridge exported schema classes";
  bridge_result.reason = schema.classes.empty()
    ? "schema contains no classes to bridge"
    : "schema classes and methods were exported into analysis bridge results";

  for (size_t i = 0; i < schema.classes.size(); ++i)
  {
    const ClangClassInfo &cls = schema.classes[i];

    ParserAnalysisBridgeStep step;
    step.step_name = cls.name;
    step.matched_symbol = cls.qualified_name.empty() ? cls.name : cls.qualified_name;
    step.matched = true;
    step.severity = pabs_info;
    step.category = "schema_step";
    step.bridge_stage = bridge_result.bridge_stage;
    bridge_result.steps.push_back(step);

    for (size_t j = 0; j < cls.methods.size(); ++j)
    {
      const ClangMethodInfo &method = cls.methods[j];
      ParserAnalysisBridgeCall call;
      call.step_name = cls.name;
      call.call_name = method.name;
      call.matched_symbol = method.qualified_name.empty() ? method.name : method.qualified_name;
      call.matched = true;
      call.severity = pabs_info;
      call.category = "schema_call";
      call.bridge_stage = bridge_result.bridge_stage;
      bridge_result.calls.push_back(call);
    }

    ParserAnalysisBridgePoint point;
    point.point_kind = "type_decl";
    point.point_name = cls.name;
    point.step_name = cls.name;
    point.related_symbol = cls.qualified_name.empty() ? cls.name : cls.qualified_name;
    point.matched = true;
    point.severity = pabs_info;
    point.category = "schema_type";
    point.bridge_stage = bridge_result.bridge_stage;
    bridge_result.points.push_back(point);
  }

  bridge_result.notes.push_back("schema classes=" + std::to_string(schema.classes.size()));
  FillBridgeMetrics(bridge_result);
  return true;
}

bool ParserIrClangBridge::MatchFlow(const CxScriptFlow &flow,
                                    const ApiSchema &schema,
                                    ParserAnalysisBridgeResult &bridge_result) const
{
  bridge_result = ParserAnalysisBridgeResult();
  bridge_result.bridge_name = "clang";
  bridge_result.bridge_stage = "clang_bridge";
  bridge_result.category = "schema_analysis";
  bridge_result.severity = pabs_info;
  bridge_result.status = pabs_unknown;
  bridge_result.origin = pabo_flow;
  bridge_result.summary = "clang flow bridge pending match results";
  bridge_result.reason = "flow statements are being matched against schema classes and methods";

  int inferred_step_id = 0;
  int inferred_frame_id = 0;

  for (size_t i = 0; i < flow.statements.size(); ++i)
  {
    const CxScriptStatement &stmt = flow.statements[i];

    if (stmt.kind == cxssk_step)
    {
      ++inferred_step_id;
      inferred_frame_id = 0;
      ParserAnalysisBridgeStep step;
      step.step_name = stmt.step_name.empty() ? stmt.name : stmt.step_name;
      step.matched = false;
      step.severity = pabs_warning;
      step.category = "step_match";
      step.bridge_stage = bridge_result.bridge_stage;
      bridge_result.steps.push_back(step);
      continue;
    }

    if (stmt.kind == cxssk_call)
    {
      ParserAnalysisBridgeCall call;
      call.step_name = stmt.step_name;
      call.call_name = stmt.callee_name.empty() ? stmt.name : stmt.callee_name;
      call.matched = ContainsMethod(schema, call.call_name, call.matched_symbol);
      call.severity = call.matched ? pabs_info : pabs_warning;
      call.category = "call_match";
      call.bridge_stage = bridge_result.bridge_stage;
      bridge_result.calls.push_back(call);
      if (!call.matched)
        bridge_result.unresolved_calls.push_back(call.call_name);
      continue;
    }

    if (stmt.kind == cxssk_block_boundary)
    {
      if (stmt.text.find('{') != std::string::npos)
        ++inferred_frame_id;
      else if (stmt.text.find('}') != std::string::npos && inferred_frame_id > 0)
        --inferred_frame_id;
      continue;
    }

    if (stmt.kind == cxssk_var_decl)
    {
      ParserAnalysisBridgePoint point;
      point.point_kind = "type_decl";
      point.point_name = stmt.declared_type;
      point.step_name = stmt.step_name;
      point.step_id = stmt.step_id > 0 ? stmt.step_id : inferred_step_id;
      point.frame_id = stmt.frame_id > 0 ? stmt.frame_id : inferred_frame_id;
      point.block_depth = stmt.block_depth;
      point.span = stmt.span;
      point.matched = ContainsType(schema, stmt.declared_type, point.related_symbol);
      point.severity = point.matched ? pabs_info : pabs_warning;
      point.category = "type_decl";
      point.bridge_stage = bridge_result.bridge_stage;
      bridge_result.points.push_back(point);
    }
  }

  for (size_t i = 0; i < bridge_result.steps.size(); ++i)
  {
    ParserAnalysisBridgeStep &step = bridge_result.steps[i];
    std::string matched_symbol;
    step.matched = ContainsType(schema, step.step_name, matched_symbol);
    if (step.matched)
    {
      step.matched_symbol = matched_symbol;
      step.severity = pabs_info;
    }
  }

  bridge_result.notes.push_back("flow statements=" + std::to_string(flow.statements.size()));
  bridge_result.status = bridge_result.unresolved_calls.empty() ? pabs_resolved : pabs_partial;
  bridge_result.summary = bridge_result.unresolved_calls.empty()
    ? "clang flow bridge resolved all flow calls"
    : "clang flow bridge produced partial call coverage";
  bridge_result.reason = bridge_result.unresolved_calls.empty()
    ? "all flow calls mapped to schema methods"
    : "one or more flow calls could not be mapped to schema methods";
  FillBridgeMetrics(bridge_result);
  return true;
}
}
