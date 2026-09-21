#ifndef CXPARSER_EXT_PARSER_FLOW_ROUTER_H
#define CXPARSER_EXT_PARSER_FLOW_ROUTER_H

#include "parser_task_types.h"

namespace cxparser_ext
{
ParserRoutePolicy BuildRoutePolicy(const std::string &route_hint,
                                   const std::string &priority_hint);
ExecutionTarget MakeExecutionTarget(const CxTaskEnvelope &envelope);
ExecutionTarget NormalizeExecutionTarget(const ExecutionTarget &target);
}

#endif
