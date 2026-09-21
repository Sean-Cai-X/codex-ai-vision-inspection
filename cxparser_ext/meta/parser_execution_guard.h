#ifndef CXPARSER_EXT_PARSER_EXECUTION_GUARD_H
#define CXPARSER_EXT_PARSER_EXECUTION_GUARD_H

#include <string>

namespace cxparser_ext
{
enum ExecutionGuardProfile
{
  egp_strict,
  egp_default,
  egp_debug
};

struct ExecutionGuardLimits
{
  int max_stage_count = 8;
  int max_event_count = 32;
  int max_object_calls = 8;
  int deadline_ms = 100;
  int timeout_ms = 1000;
  bool allow_degraded_result = false;
};

struct ExecutionGuardState
{
  int stage_count = 0;
  int event_count = 0;
  int object_call_count = 0;
  long long elapsed_ms = 0;
  bool deadline_missed = false;
  bool timed_out = false;
  bool aborted = false;
  std::string abort_reason;
  std::string failure_stage;
};

struct ExecutionGuardResult
{
  bool allowed = true;
  std::string failure_stage;
  std::string message;
};

struct ExecutionGuardContext
{
  ExecutionGuardLimits limits;
  ExecutionGuardState state;
};

inline ExecutionGuardLimits MakeExecutionGuardLimits(ExecutionGuardProfile profile)
{
  ExecutionGuardLimits limits;
  switch (profile)
  {
  case egp_strict:
    limits.max_stage_count = 8;
    limits.max_event_count = 32;
    limits.max_object_calls = 8;
    limits.deadline_ms = 50;
    limits.timeout_ms = 1000;
    limits.allow_degraded_result = false;
    break;
  case egp_debug:
    limits.max_stage_count = 32;
    limits.max_event_count = 256;
    limits.max_object_calls = 64;
    limits.deadline_ms = 1000;
    limits.timeout_ms = 10000;
    limits.allow_degraded_result = true;
    break;
  case egp_default:
  default:
    limits.max_stage_count = 16;
    limits.max_event_count = 128;
    limits.max_object_calls = 32;
    limits.deadline_ms = 100;
    limits.timeout_ms = 3000;
    limits.allow_degraded_result = false;
    break;
  }
  return limits;
}

inline void SetExecutionGuardProfile(ExecutionGuardContext &ctx, ExecutionGuardProfile profile)
{
  ctx.limits = MakeExecutionGuardLimits(profile);
}

inline void SetExecutionGuardLimits(ExecutionGuardContext &ctx, const ExecutionGuardLimits &limits)
{
  ctx.limits = limits;
}

inline void ResetExecutionGuard(ExecutionGuardContext &ctx)
{
  ctx.state = ExecutionGuardState();
}

inline ExecutionGuardResult GuardEnterStage(ExecutionGuardContext &ctx, const std::string &stage)
{
  ExecutionGuardResult result;
  ++ctx.state.stage_count;
  if (ctx.state.stage_count > ctx.limits.max_stage_count)
  {
    ctx.state.aborted = true;
    ctx.state.failure_stage = stage;
    ctx.state.abort_reason = "stage count limit exceeded";
    result.allowed = false;
    result.failure_stage = stage;
    result.message = ctx.state.abort_reason;
  }
  return result;
}

inline ExecutionGuardResult GuardRecordEvent(ExecutionGuardContext &ctx, const std::string &stage)
{
  ExecutionGuardResult result;
  ++ctx.state.event_count;
  if (ctx.state.event_count > ctx.limits.max_event_count)
  {
    ctx.state.aborted = true;
    ctx.state.failure_stage = stage;
    ctx.state.abort_reason = "event count limit exceeded";
    result.allowed = false;
    result.failure_stage = stage;
    result.message = ctx.state.abort_reason;
  }
  return result;
}

inline ExecutionGuardResult GuardRecordObjectCall(ExecutionGuardContext &ctx, const std::string &stage)
{
  ExecutionGuardResult result;
  ++ctx.state.object_call_count;
  if (ctx.state.object_call_count > ctx.limits.max_object_calls)
  {
    ctx.state.aborted = true;
    ctx.state.failure_stage = stage;
    ctx.state.abort_reason = "object call limit exceeded";
    result.allowed = false;
    result.failure_stage = stage;
    result.message = ctx.state.abort_reason;
  }
  return result;
}

inline ExecutionGuardResult GuardCheckTimeout(ExecutionGuardContext &ctx,
                                              long long elapsed_ms,
                                              const std::string &stage)
{
  ExecutionGuardResult result;
  if (elapsed_ms > ctx.limits.timeout_ms)
  {
    ctx.state.timed_out = true;
    ctx.state.aborted = true;
    ctx.state.failure_stage = stage;
    ctx.state.abort_reason = "execution timeout";
    result.allowed = false;
    result.failure_stage = stage;
    result.message = ctx.state.abort_reason;
  }
  return result;
}

inline ExecutionGuardResult GuardCheckDeadline(ExecutionGuardContext &ctx,
                                               long long elapsed_ms,
                                               const std::string &stage)
{
  ExecutionGuardResult result;
  ctx.state.elapsed_ms = elapsed_ms;
  if (elapsed_ms > ctx.limits.deadline_ms)
  {
    ctx.state.deadline_missed = true;
    result.allowed = false;
    result.failure_stage = stage;
    result.message = "execution missed scenario deadline";
  }
  return result;
}
}

#endif
