#include "parser_flow_router.h"

namespace cxparser_ext
{
namespace
{
ParserRoutePolicy MakeBaseRoute(const char *lane_name)
{
  ParserRoutePolicy route;
  route.route_key = lane_name;
  route.lane_name = lane_name;
  route.guard_profile = egp_default;
  route.deadline_ms = 0;
  route.timeout_ms = 0;
  route.allow_degraded_result = true;
  return route;
}
}

ParserRoutePolicy BuildRoutePolicy(const std::string &route_hint,
                                   const std::string &priority_hint)
{
  const std::string hint = !route_hint.empty() ? route_hint : priority_hint;
  if (hint == task_constants::RouteRealtime())
  {
    ParserRoutePolicy route = MakeBaseRoute(task_constants::RouteRealtime());
    route.allow_degraded_result = false;
    return route;
  }

  if (hint == task_constants::RouteBatch())
  {
    ParserRoutePolicy route = MakeBaseRoute(task_constants::RouteBatch());
    route.allow_degraded_result = true;
    return route;
  }

  if (hint == task_constants::RouteReplay())
  {
    ParserRoutePolicy route = MakeBaseRoute(task_constants::RouteReplay());
    route.allow_degraded_result = true;
    return route;
  }

  return MakeBaseRoute(task_constants::RouteDefault());
}

ExecutionTarget MakeExecutionTarget(const CxTaskEnvelope &envelope)
{
  ExecutionTarget target;
  target.task_id = envelope.task_id;
  target.task_name = envelope.task_name.empty() ? envelope.task_id : envelope.task_name;
  target.trace_id = envelope.trace_id.empty() ? ("trace." + target.task_id) : envelope.trace_id;
  target.task_type = envelope.task_type;
  target.task_subtype = envelope.task_subtype;
  target.execution_mode = envelope.execution_mode.empty() ?
    task_constants::ExecutionModeMainline() : envelope.execution_mode;
  target.route_hint = envelope.route;
  target.priority_hint = envelope.route;
  target.module_name = envelope.callee_module;
  target.target_class = envelope.target_class;
  target.target_method = envelope.target_method;
  target.script_text = envelope.script_text;
  target.module_call.caller_module = envelope.caller_module;
  target.module_call.callee_module = envelope.callee_module;
  target.module_call.protocol_name = "cxparser.module.call";
  target.module_call.capability_name = "script_dispatch";
  target.module_call.class_name = envelope.target_class;
  target.module_call.method_name = envelope.target_method;
  target.route = BuildRoutePolicy(target.route_hint, target.priority_hint);
  target.tags = envelope.tags;
  return target;
}

ExecutionTarget NormalizeExecutionTarget(const ExecutionTarget &target)
{
  ExecutionTarget normalized = target;
  if (normalized.task_name.empty())
    normalized.task_name = normalized.task_id;
  if (normalized.trace_id.empty())
    normalized.trace_id = "trace." + normalized.task_id;
  if (normalized.task_type.empty())
    normalized.task_type = task_constants::TaskTypeCoreTest();
  if (normalized.execution_mode.empty())
    normalized.execution_mode = task_constants::ExecutionModeMainline();
  if (normalized.module_call.caller_module.empty())
    normalized.module_call.caller_module = "cxparser";
  if (normalized.module_call.callee_module.empty())
    normalized.module_call.callee_module = normalized.module_name;
  if (normalized.module_call.protocol_name.empty())
    normalized.module_call.protocol_name = "cxparser.module.call";
  if (normalized.module_call.capability_name.empty())
    normalized.module_call.capability_name = "script_dispatch";
  if (normalized.module_call.class_name.empty())
    normalized.module_call.class_name = normalized.target_class;
  if (normalized.module_call.method_name.empty())
    normalized.module_call.method_name = normalized.target_method;
  if (normalized.route.route_key.empty() || normalized.route.lane_name.empty())
    normalized.route = BuildRoutePolicy(normalized.route_hint, normalized.priority_hint);
  return normalized;
}
}
