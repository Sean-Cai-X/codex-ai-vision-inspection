#include "parser_pipeline.h"

#include <chrono>

namespace cxparser_ext
{
namespace
{
EvidenceEvent MakeGuardEvent(const ExecutionGuardResult &guard_result)
{
  EvidenceEvent event;
  event.level = eel_error;
  event.stage = guard_result.failure_stage.empty() ? "guard" : guard_result.failure_stage;
  event.code = "guard_triggered";
  event.message = guard_result.message;
  event.expected = "execution should remain within configured limits";
  event.actual = guard_result.message;
  return event;
}

EvidenceEvent MakeDeadlineEvent(const ExecutionGuardContext &guard_context,
                                const std::string &stage)
{
  EvidenceEvent event;
  event.level = guard_context.limits.allow_degraded_result ? eel_warning : eel_error;
  event.stage = stage;
  event.code = "deadline_missed";
  event.message = "execution missed scenario deadline";
  event.expected = "elapsed <= deadline_ms(" + std::to_string(guard_context.limits.deadline_ms) + ")";
  event.actual = "elapsed=" + std::to_string(guard_context.state.elapsed_ms);
  return event;
}
}

ParserPipeline::ParserPipeline()
{
  SetGuardProfile(egp_strict);
}

void ParserPipeline::Reset()
{
  target_ = ExecutionTarget();
  binding_spec_ = ParserBindingSpec();
  evidence_ = ParserEvidenceBundle();
  last_result_ = ExecutionResult();
  last_report_ = ParserValidationReport();
  ResetExecutionGuard(guard_);
  runtime_.Reset();
}

void ParserPipeline::AppendTrace(const std::string &stage,
                                 const std::string &action,
                                 const std::string &status,
                                 const std::string &detail)
{
  ParserTraceEntry entry;
  entry.sequence = static_cast<int>(evidence_.trace_entries.size()) + 1;
  entry.trace_id = evidence_.trace_id;
  entry.stage = stage;
  entry.action = action;
  entry.status = status;
  entry.detail = detail;
  evidence_.trace_entries.push_back(entry);
}

void ParserPipeline::AppendLog(const std::string &level,
                               const std::string &stage,
                               const std::string &code,
                               const std::string &message)
{
  ParserLogEntry entry;
  entry.trace_id = evidence_.trace_id;
  entry.level = level;
  entry.stage = stage;
  entry.code = code;
  entry.message = message;
  evidence_.log_entries.push_back(entry);
}

void ParserPipeline::SetGuardProfile(ExecutionGuardProfile profile)
{
  SetExecutionGuardProfile(guard_, profile);
}

void ParserPipeline::SetGuardLimits(const ExecutionGuardLimits &limits)
{
  SetExecutionGuardLimits(guard_, limits);
}

ExecutionGuardLimits ParserPipeline::GetGuardLimits() const
{
  return guard_.limits;
}

bool ParserPipeline::PrepareTask(const ExecutionTarget &target)
{
  ResetExecutionGuard(guard_);
  evidence_ = ParserEvidenceBundle();
  evidence_.task_id = target.task_id;
  evidence_.trace_id = target.trace_id;
  evidence_.route_key = target.route.route_key;
  evidence_.route_lane = target.route.lane_name;
  evidence_.protocol_name = target.module_call.protocol_name;
  AppendTrace("prepare_task", "prepare", "start", target.task_name);
  const ExecutionGuardResult guard_result = GuardEnterStage(guard_, "prepare_task");
  if (!guard_result.allowed)
  {
    evidence_.events.push_back(MakeGuardEvent(guard_result));
    AppendTrace("prepare_task", "prepare", "blocked", guard_result.message);
    AppendLog("error", "prepare_task", "guard_triggered", guard_result.message);
    return false;
  }

  if (target.script_text.empty())
  {
    AppendTrace("prepare_task", "prepare", "failed", "script is empty");
    AppendLog("error", "prepare_task", "script_empty", "task script text is empty");
    return false;
  }

  target_ = target;
  evidence_.task_id = target_.task_id;
  AppendTrace("prepare_task", "prepare", "ok", target_.route.route_key);
  AppendLog("info", "prepare_task", "task_prepared", "task prepared for pipeline execution");
  return true;
}

bool ParserPipeline::MergeBindingSpec(const ParserBindingSpec &spec)
{
  const ExecutionGuardResult guard_result = GuardEnterStage(guard_, "merge_binding");
  if (!guard_result.allowed)
  {
    evidence_.events.push_back(MakeGuardEvent(guard_result));
    AppendTrace("merge_binding", "merge_binding", "blocked", guard_result.message);
    AppendLog("error", "merge_binding", "guard_triggered", guard_result.message);
    return false;
  }

  binding_spec_ = spec;
  AppendTrace("merge_binding", "merge_binding", "ok", "binding spec accepted");
  AppendLog("info", "merge_binding", "binding_loaded", "binding spec merged into pipeline");
  return true;
}

bool ParserPipeline::MergeEvidence(const ParserEvidenceBundle &bundle)
{
  const ExecutionGuardResult stage_guard = GuardEnterStage(guard_, "merge_evidence");
  if (!stage_guard.allowed)
  {
    evidence_.events.push_back(MakeGuardEvent(stage_guard));
    AppendTrace("merge_evidence", "merge_evidence", "blocked", stage_guard.message);
    AppendLog("error", "merge_evidence", "guard_triggered", stage_guard.message);
    return false;
  }

  if (evidence_.task_id.empty())
    evidence_.task_id = bundle.task_id;
  if (evidence_.trace_id.empty())
    evidence_.trace_id = bundle.trace_id;
  if (evidence_.route_key.empty())
    evidence_.route_key = bundle.route_key;
  if (evidence_.route_lane.empty())
    evidence_.route_lane = bundle.route_lane;
  if (evidence_.protocol_name.empty())
    evidence_.protocol_name = bundle.protocol_name;

  for (size_t i = 0; i < bundle.events.size(); ++i)
  {
    const ExecutionGuardResult event_guard = GuardRecordEvent(guard_, "merge_evidence");
    if (!event_guard.allowed)
    {
      evidence_.events.push_back(MakeGuardEvent(event_guard));
      AppendTrace("merge_evidence", "merge_evidence", "blocked", event_guard.message);
      AppendLog("error", "merge_evidence", "guard_triggered", event_guard.message);
      return false;
    }
    evidence_.events.push_back(bundle.events[i]);
  }

  evidence_.trace_entries.insert(evidence_.trace_entries.end(),
                                 bundle.trace_entries.begin(),
                                 bundle.trace_entries.end());
  evidence_.log_entries.insert(evidence_.log_entries.end(),
                               bundle.log_entries.begin(),
                               bundle.log_entries.end());
  evidence_.calls.insert(evidence_.calls.end(), bundle.calls.begin(), bundle.calls.end());
  evidence_.backtrace.insert(evidence_.backtrace.end(), bundle.backtrace.begin(), bundle.backtrace.end());
  evidence_.graph_nodes.insert(evidence_.graph_nodes.end(), bundle.graph_nodes.begin(), bundle.graph_nodes.end());
  evidence_.graph_edges.insert(evidence_.graph_edges.end(), bundle.graph_edges.begin(), bundle.graph_edges.end());
  evidence_.notes.insert(evidence_.notes.end(), bundle.notes.begin(), bundle.notes.end());

  if (evidence_.disasm_text.empty())
    evidence_.disasm_text = bundle.disasm_text;
  if (evidence_.decompile_text.empty())
    evidence_.decompile_text = bundle.decompile_text;
  if (evidence_.debug_state.current_address.empty())
    evidence_.debug_state = bundle.debug_state;

  AppendTrace("merge_evidence", "merge_evidence", "ok", "runtime evidence merged");
  return true;
}

bool ParserPipeline::Run(ExecutionResult &result)
{
  AppendTrace("parser_execute", "execute", "start", target_.route.route_key);
  const ExecutionGuardResult stage_guard = GuardEnterStage(guard_, "parser_execute");
  if (!stage_guard.allowed)
  {
    evidence_.events.push_back(MakeGuardEvent(stage_guard));
    result.error_message = stage_guard.message;
    AppendTrace("parser_execute", "execute", "blocked", stage_guard.message);
    AppendLog("error", "parser_execute", "guard_triggered", stage_guard.message);
    return false;
  }

  if (target_.script_text.empty())
  {
    AppendTrace("parser_execute", "execute", "failed", "script is empty");
    AppendLog("error", "parser_execute", "script_empty", "task script text is empty");
    return false;
  }

  if (!runtime_.LoadBindingSpec(binding_spec_))
  {
    AppendTrace("parser_execute", "load_binding", "failed", "binding spec rejected by runtime");
    return false;
  }

  if (!runtime_.LoadScript(target_))
  {
    AppendTrace("parser_execute", "load_script", "failed", "runtime refused task request");
    return false;
  }

  const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
  if (!runtime_.Execute(last_result_))
  {
    result = last_result_;
    AppendTrace("parser_execute", "execute", "failed", result.error_message);
    AppendLog("error", "parser_execute", "runtime_execute_failed", result.error_message);
    return false;
  }
  const std::chrono::steady_clock::time_point finish = std::chrono::steady_clock::now();
  const long long elapsed_ms =
    std::chrono::duration_cast<std::chrono::milliseconds>(finish - start).count();
  const ExecutionGuardResult deadline_guard = GuardCheckDeadline(guard_, elapsed_ms, "parser_execute");
  if (!deadline_guard.allowed)
  {
    evidence_.events.push_back(MakeDeadlineEvent(guard_, "parser_execute"));
    AppendTrace("parser_execute",
                "deadline",
                guard_.limits.allow_degraded_result ? "warning" : "failed",
                deadline_guard.message);
    AppendLog(guard_.limits.allow_degraded_result ? "warning" : "error",
              "parser_execute",
              "deadline_missed",
              deadline_guard.message);
  }
  const ExecutionGuardResult timeout_guard = GuardCheckTimeout(guard_, elapsed_ms, "parser_execute");
  if (!timeout_guard.allowed)
  {
    evidence_.events.push_back(MakeGuardEvent(timeout_guard));
    result.success = false;
    result.error_message = timeout_guard.message;
    AppendTrace("parser_execute", "execute", "timeout", timeout_guard.message);
    AppendLog("error", "parser_execute", "execution_timeout", timeout_guard.message);
    return false;
  }

  ParserEvidenceBundle runtime_bundle;
  runtime_.CollectRuntimeEvidence(runtime_bundle);
  MergeEvidence(runtime_bundle);

  result = last_result_;
  AppendTrace("parser_execute", "execute", "ok", "runtime execution completed");
  AppendLog("info", "parser_execute", "runtime_execute_ok", "runtime execution completed");
  return true;
}

bool ParserPipeline::Validate(ParserValidationReport &report)
{
  AppendTrace("validate_report", "validate", "start", target_.task_id);
  const ExecutionGuardResult guard_result = GuardEnterStage(guard_, "validate_report");
  if (!guard_result.allowed)
  {
    evidence_.events.push_back(MakeGuardEvent(guard_result));
    AppendTrace("validate_report", "validate", "blocked", guard_result.message);
    AppendLog("error", "validate_report", "guard_triggered", guard_result.message);
    return false;
  }

  report = ParserValidationReport();
  if (!validation_.CompareExecutionAndEvidence(last_result_, evidence_, report))
    return false;

  last_report_ = report;
  AppendTrace("validate_report", "validate", report.passed ? "ok" : "failed", report.passed ? "validation passed" : "validation failed");
  AppendLog(report.passed ? "info" : "error",
            "validate_report",
            report.passed ? "validation_passed" : "validation_failed",
            report.passed ? "validation passed" : "validation failed");
  return true;
}

void *ParserPipeline::GetClassObject(const std::string &class_name,
                                    const std::string &object_name)
{
  const ExecutionGuardResult guard_result = GuardRecordObjectCall(guard_, "parser_object_check");
  if (!guard_result.allowed)
  {
    evidence_.events.push_back(MakeGuardEvent(guard_result));
    AppendLog("error", "parser_object_check", "guard_triggered", guard_result.message);
    return 0;
  }

  return runtime_.GetClassObject(class_name, object_name);
}

const ParserEvidenceBundle &ParserPipeline::GetEvidence() const
{
  return evidence_;
}
}
