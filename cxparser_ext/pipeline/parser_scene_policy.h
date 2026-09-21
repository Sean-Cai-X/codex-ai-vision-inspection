#ifndef CXPARSER_EXT_PARSER_SCENE_POLICY_H
#define CXPARSER_EXT_PARSER_SCENE_POLICY_H

#include <string>

#include "parser_task_types.h"

namespace cxparser_ext
{
inline ParserRoutePolicy MakeDefaultRoutePolicy()
{
  ParserRoutePolicy policy;
  policy.route_key = "default";
  policy.lane_name = "default";
  policy.guard_profile = egp_default;
  policy.deadline_ms = 500;
  policy.timeout_ms = 3000;
  policy.allow_degraded_result = true;
  return policy;
}

inline ParserRoutePolicy MakeRealtimeRoutePolicy()
{
  ParserRoutePolicy policy;
  policy.route_key = "realtime";
  policy.lane_name = "realtime";
  policy.guard_profile = egp_strict;
  policy.deadline_ms = 33;
  policy.timeout_ms = 200;
  policy.allow_degraded_result = false;
  return policy;
}

inline ParserRoutePolicy MakeBatchRoutePolicy()
{
  ParserRoutePolicy policy;
  policy.route_key = "batch";
  policy.lane_name = "batch";
  policy.guard_profile = egp_debug;
  policy.deadline_ms = 1000;
  policy.timeout_ms = 10000;
  policy.allow_degraded_result = true;
  return policy;
}

inline bool RouteHintEquals(const std::string &lhs, const char *rhs)
{
  return lhs == rhs;
}

inline ParserRoutePolicy ResolveRoutePolicy(const ExecutionTarget &target)
{
  if (RouteHintEquals(target.route_hint, "realtime") ||
      RouteHintEquals(target.priority_hint, "realtime") ||
      RouteHintEquals(target.module_call.capability_name, "stream_frame"))
  {
    return MakeRealtimeRoutePolicy();
  }

  if (RouteHintEquals(target.route_hint, "batch") ||
      RouteHintEquals(target.priority_hint, "background"))
  {
    return MakeBatchRoutePolicy();
  }

  return MakeDefaultRoutePolicy();
}
}

#endif
