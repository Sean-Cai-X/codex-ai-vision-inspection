#include "parser_task_coordinator.h"

#include <utility>

#include "parser_flow_router.h"

namespace cxparser_ext
{
namespace
{
bool HasDeadlineMissed(const ParserEvidenceBundle &evidence)
{
  for (size_t i = 0; i < evidence.events.size(); ++i)
  {
    if (evidence.events[i].code == "deadline_missed")
      return true;
  }
  return false;
}

void UpdateTaskOutcome(ParserTaskUnit &unit)
{
  ParserTaskOutcome outcome;

  ValidationLevel highest_level = vl_info;
  const ValidationIssue *primary_issue = 0;
  for (size_t i = 0; i < unit.report.issues.size(); ++i)
  {
    const ValidationIssue &issue = unit.report.issues[i];
    if (issue.level > highest_level)
    {
      highest_level = issue.level;
      primary_issue = &issue;
    }
  }

  outcome.degraded = HasDeadlineMissed(unit.evidence) && unit.route.allow_degraded_result;

  if (!unit.result.success)
  {
    outcome.hard_fail = true;
    outcome.error_stage = "parser_execute";
    outcome.error_code = unit.result.error_kind.empty() ? "execution_failed" : unit.result.error_kind;
    outcome.error_message = unit.result.error_message.empty() ? "execution failed" : unit.result.error_message;
    outcome.summary = outcome.error_message;
  }
  else if (primary_issue != 0 && primary_issue->level == vl_error)
  {
    outcome.hard_fail = true;
    outcome.error_stage = primary_issue->failure_stage;
    outcome.error_code = primary_issue->code;
    outcome.error_message = primary_issue->message;
    outcome.summary = primary_issue->message;
  }
  else if (!unit.failure_reason.empty())
  {
    outcome.hard_fail = true;
    outcome.error_stage = unit.status == pts_failed ? "task_lifecycle" : std::string();
    outcome.error_code = "task_failed";
    outcome.error_message = unit.failure_reason;
    outcome.summary = unit.failure_reason;
  }
  else if (outcome.degraded)
  {
    outcome.summary = "deadline missed but degraded result accepted";
  }
  else if (unit.status == pts_validated && unit.report.passed)
  {
    outcome.summary = "task validated";
  }

  outcome.success = (unit.status == pts_validated &&
                     unit.result.success &&
                     unit.report.passed &&
                     !outcome.hard_fail);

  if (outcome.summary.empty())
    outcome.summary = outcome.success ? "task completed" : "task not completed";

  unit.outcome = outcome;
}

void ApplyRoutePolicy(ParserTaskUnit &unit)
{
  unit.pipeline.SetGuardProfile(unit.route.guard_profile);

  ExecutionGuardLimits limits = unit.pipeline.GetGuardLimits();
  limits.deadline_ms = unit.route.deadline_ms;
  limits.timeout_ms = unit.route.timeout_ms;
  limits.allow_degraded_result = unit.route.allow_degraded_result;
  unit.pipeline.SetGuardLimits(limits);
}

void ApplyEnvelopeEvidenceFields(const ExecutionTarget &target,
                                 ParserEvidenceBundle &evidence)
{
  evidence.protocol_name = target.module_call.protocol_name;
  if (!target.task_type.empty())
    evidence.task_type = target.task_type;
  if (!target.task_subtype.empty())
    evidence.task_subtype = target.task_subtype;
  if (!target.execution_mode.empty())
    evidence.execution_mode = target.execution_mode;
  evidence.module_chain.clear();
  if (!target.module_call.caller_module.empty())
    evidence.module_chain.push_back(target.module_call.caller_module);
  if (!target.module_call.callee_module.empty())
    evidence.module_chain.push_back(target.module_call.callee_module);

  for (size_t i = 0; i < target.tags.size(); ++i)
  {
    const std::string &tag = target.tags[i];
    if (tag.find("task_type:") == 0)
      evidence.task_type = tag.substr(std::string("task_type:").size());
    else if (tag.find("task_subtype:") == 0)
      evidence.task_subtype = tag.substr(std::string("task_subtype:").size());
    else if (tag.find("execution_mode:") == 0)
      evidence.execution_mode = tag.substr(std::string("execution_mode:").size());
  }
}
}

void ParserTaskCoordinator::SetBindingSpec(const ParserBindingSpec &spec)
{
  binding_spec_ = spec;
}

void ParserTaskCoordinator::Reset()
{
  binding_spec_ = ParserBindingSpec();
  std::vector<ParserTaskUnit>().swap(tasks_);
  std::vector<TaskChainRecord>().swap(chain_records_);
}

bool ParserTaskCoordinator::AddTaskEnvelope(const CxTaskEnvelope &envelope)
{
  return AddTask(MakeExecutionTarget(envelope));
}

bool ParserTaskCoordinator::AddTask(const ExecutionTarget &target)
{
  const ExecutionTarget normalized = NormalizeExecutionTarget(target);
  if (normalized.task_id.empty())
    return false;
  if (FindTask(normalized.task_id) != 0)
    return false;

  ParserTaskUnit unit;
  unit.task_id = normalized.task_id;
  unit.route = normalized.route;
  unit.target = normalized;
  ApplyRoutePolicy(unit);
  unit.evidence.task_id = unit.task_id;
  unit.evidence.trace_id = unit.target.trace_id;
  unit.evidence.route_key = unit.route.route_key;
  unit.evidence.route_lane = unit.route.lane_name;
  ApplyEnvelopeEvidenceFields(unit.target, unit.evidence);
  tasks_.push_back(std::move(unit));
  UpdateTaskOutcome(tasks_.back());

  TaskChainRecord chain_record;
  chain_record.task_id = normalized.task_id;
  chain_record.task_type = tasks_.back().evidence.task_type;
  chain_record.task_subtype = tasks_.back().evidence.task_subtype;
  chain_record.route = normalized.route.lane_name;
  chain_record.execution_mode = tasks_.back().evidence.execution_mode;
  chain_record.replay_source_task_id.clear();
  chain_record.modules = tasks_.back().evidence.module_chain;
  chain_records_.push_back(chain_record);
  return true;
}

bool ParserTaskCoordinator::UpdateTaskRoute(const std::string &task_id, const ParserRoutePolicy &route)
{
  ParserTaskUnit *task = FindTask(task_id);
  if (!task)
    return false;

  task->route = route;
  task->target.route = route;
  ApplyRoutePolicy(*task);
  task->evidence.route_key = route.route_key;
  task->evidence.route_lane = route.lane_name;
  UpdateTaskOutcome(*task);
  return true;
}

bool ParserTaskCoordinator::PrepareTask(const std::string &task_id)
{
  ParserTaskUnit *task = FindTask(task_id);
  if (!task)
    return false;

  ApplyRoutePolicy(*task);

  if (!task->pipeline.PrepareTask(task->target))
  {
    task->status = pts_failed;
    task->failure_reason = "prepare failed";
    UpdateTaskOutcome(*task);
    return false;
  }

  if (!binding_spec_.modules.empty() &&
      !task->pipeline.MergeBindingSpec(binding_spec_))
  {
    task->status = pts_failed;
    task->failure_reason = "binding merge failed";
    UpdateTaskOutcome(*task);
    return false;
  }

  task->evidence = task->pipeline.GetEvidence();
  ApplyEnvelopeEvidenceFields(task->target, task->evidence);
  task->status = pts_prepared;
  task->failure_reason.clear();
  UpdateTaskOutcome(*task);
  return true;
}

bool ParserTaskCoordinator::RunTask(const std::string &task_id)
{
  ParserTaskUnit *task = FindTask(task_id);
  if (!task)
    return false;

  task->status = pts_running;
  if (!task->pipeline.Run(task->result))
  {
    task->status = pts_failed;
    task->failure_reason = task->result.error_message.empty() ? "run failed" : task->result.error_message;
    task->evidence = task->pipeline.GetEvidence();
    ApplyEnvelopeEvidenceFields(task->target, task->evidence);
    UpdateTaskOutcome(*task);
    return false;
  }

  task->evidence = task->pipeline.GetEvidence();
  ApplyEnvelopeEvidenceFields(task->target, task->evidence);
  task->status = pts_executed;
  task->failure_reason.clear();
  UpdateTaskOutcome(*task);
  return true;
}

bool ParserTaskCoordinator::ValidateTask(const std::string &task_id)
{
  ParserTaskUnit *task = FindTask(task_id);
  if (!task)
    return false;

  if (!task->pipeline.Validate(task->report))
  {
    task->status = pts_failed;
    task->failure_reason = "validate failed";
    task->evidence = task->pipeline.GetEvidence();
    ApplyEnvelopeEvidenceFields(task->target, task->evidence);
    UpdateTaskOutcome(*task);
    return false;
  }

  task->evidence = task->pipeline.GetEvidence();
  ApplyEnvelopeEvidenceFields(task->target, task->evidence);
  task->status = pts_validated;
  task->failure_reason.clear();
  UpdateTaskOutcome(*task);
  return true;
}

bool ParserTaskCoordinator::ExecuteTask(const std::string &task_id)
{
  return PrepareTask(task_id) &&
         RunTask(task_id) &&
         ValidateTask(task_id);
}

bool ParserTaskCoordinator::PrepareAll()
{
  bool ok = true;
  for (size_t i = 0; i < tasks_.size(); ++i)
    ok = PrepareTask(tasks_[i].task_id) && ok;
  return ok;
}

bool ParserTaskCoordinator::RunAll()
{
  bool ok = true;
  for (size_t i = 0; i < tasks_.size(); ++i)
    ok = RunTask(tasks_[i].task_id) && ok;
  return ok;
}

bool ParserTaskCoordinator::ValidateAll()
{
  bool ok = true;
  for (size_t i = 0; i < tasks_.size(); ++i)
    ok = ValidateTask(tasks_[i].task_id) && ok;
  return ok;
}

bool ParserTaskCoordinator::ExecuteAll()
{
  bool ok = true;
  for (size_t i = 0; i < tasks_.size(); ++i)
    ok = ExecuteTask(tasks_[i].task_id) && ok;
  return ok;
}

void ParserTaskCoordinator::RecordReplayChain(const std::string &task_id,
                                              const std::string &execution_mode,
                                              const std::string &replay_source_task_id)
{
  ParserTaskUnit *task = FindTask(task_id);
  if (!task)
    return;

  TaskChainRecord chain_record;
  chain_record.task_id = task->task_id;
  chain_record.task_type = task->evidence.task_type;
  chain_record.task_subtype = task->evidence.task_subtype;
  chain_record.route = task->evidence.route_lane;
  chain_record.execution_mode = execution_mode;
  chain_record.replay_source_task_id = replay_source_task_id;
  chain_record.modules = task->evidence.module_chain;
  chain_records_.push_back(chain_record);
}

ParserTaskUnit *ParserTaskCoordinator::FindTask(const std::string &task_id)
{
  for (size_t i = 0; i < tasks_.size(); ++i)
  {
    if (tasks_[i].task_id == task_id)
      return &tasks_[i];
  }
  return 0;
}

const ParserTaskUnit *ParserTaskCoordinator::FindTask(const std::string &task_id) const
{
  for (size_t i = 0; i < tasks_.size(); ++i)
  {
    if (tasks_[i].task_id == task_id)
      return &tasks_[i];
  }
  return 0;
}

const ParserTaskOutcome *ParserTaskCoordinator::FindTaskOutcome(const std::string &task_id) const
{
  const ParserTaskUnit *task = FindTask(task_id);
  if (!task)
    return 0;
  return &task->outcome;
}

const std::vector<TaskChainRecord> &ParserTaskCoordinator::GetChainRecords() const
{
  return chain_records_;
}
}
