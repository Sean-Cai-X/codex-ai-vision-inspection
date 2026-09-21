#include "parser_unified_entry.h"

namespace cxparser_ext
{
namespace
{
void CollectReplayStages(const ParserEvidenceBundle &evidence,
                         std::vector<std::string> &stages)
{
  stages.clear();
  for (size_t i = 0; i < evidence.trace_entries.size(); ++i)
  {
    const std::string &stage = evidence.trace_entries[i].stage;
    bool seen = false;
    for (size_t j = 0; j < stages.size(); ++j)
    {
      if (stages[j] == stage)
      {
        seen = true;
        break;
      }
    }
    if (!seen && !stage.empty())
      stages.push_back(stage);
  }
}
}

void ParserUnifiedEntry::SetBindingSpec(const ParserBindingSpec &spec)
{
  coordinator_.SetBindingSpec(spec);
}

void ParserUnifiedEntry::Reset()
{
  coordinator_.Reset();
  last_tick_ = ParserMainThreadTick();
  std::vector<std::string>().swap(submission_order_);
  std::vector<ImageAnalysisTaskRecord>().swap(image_analysis_tasks_);
}

bool ParserUnifiedEntry::SubmitImageAnalysisTask(const ImageAnalysisRequest &request)
{
  if (request.task_id.empty())
    return false;

  for (size_t i = 0; i < image_analysis_tasks_.size(); ++i)
  {
    if (image_analysis_tasks_[i].request.task_id == request.task_id)
      return false;
  }

  ImageAnalysisTaskRecord record;
  record.request = request;
  image_analysis_tasks_.push_back(record);
  submission_order_.push_back(request.task_id);
  ++last_tick_.accepted_task_count;
  return true;
}

bool ParserUnifiedEntry::SubmitEnvelope(const CxTaskEnvelope &envelope)
{
  if (!coordinator_.AddTaskEnvelope(envelope))
    return false;

  const std::string lookup_id = envelope.task_id.empty() ? envelope.task_name : envelope.task_id;
  const ParserTaskUnit *task = coordinator_.FindTask(lookup_id);
  if (!task)
    return false;

  submission_order_.push_back(task->task_id);
  ++last_tick_.accepted_task_count;
  return true;
}

bool ParserUnifiedEntry::SubmitTask(const ExecutionTarget &target)
{
  if (!coordinator_.AddTask(target))
    return false;

  const ParserTaskUnit *task = coordinator_.FindTask(target.task_id.empty() ? target.task_name : target.task_id);
  if (!task)
    return false;

  submission_order_.push_back(task->task_id);
  ++last_tick_.accepted_task_count;
  return true;
}

bool ParserUnifiedEntry::ExecuteMainThreadCycle()
{
  last_tick_.thread_name = "cxparser_main";
  last_tick_.thread_role = "unified_dispatch";
  ++last_tick_.cycle_index;
  last_tick_.executed_task_count = 0;

  bool ok = true;
  for (size_t i = 0; i < submission_order_.size(); ++i)
  {
    const std::string &task_id = submission_order_[i];
    if (FindImageAnalysisResult(task_id) != 0)
      ok = ExecuteImageAnalysisTask(task_id) && ok;
    else
      ok = coordinator_.ExecuteTask(task_id) && ok;
    ++last_tick_.executed_task_count;
    RecordMainThreadTrace(task_id);
  }

  last_tick_.success = ok;
  return ok;
}

const ParserMainThreadTick &ParserUnifiedEntry::GetLastTick() const
{
  return last_tick_;
}

ParserReplaySummary ParserUnifiedEntry::GetLastReplaySummary() const
{
  ParserReplaySummary summary;
  summary.replay_count = last_tick_.replay_count;
  summary.replay_task_id = last_tick_.last_replay_task_id;
  summary.replay_source_task_id = last_tick_.last_replay_source_task_id;
  summary.replay_modules = last_tick_.last_replay_modules;
  summary.replay_stages = last_tick_.last_replay_stages;
  return summary;
}

const ParserEvidenceBundle *ParserUnifiedEntry::FindTaskEvidence(const std::string &task_id) const
{
  const ParserTaskUnit *task = coordinator_.FindTask(task_id);
  if (!task)
    return 0;
  return &task->evidence;
}

const ParserTaskUnit *ParserUnifiedEntry::FindTask(const std::string &task_id) const
{
  return coordinator_.FindTask(task_id);
}

const ParserTaskOutcome *ParserUnifiedEntry::FindTaskOutcome(const std::string &task_id) const
{
  return coordinator_.FindTaskOutcome(task_id);
}

const ImageAnalysisResult *ParserUnifiedEntry::FindImageAnalysisResult(const std::string &task_id) const
{
  for (size_t i = 0; i < image_analysis_tasks_.size(); ++i)
  {
    if (image_analysis_tasks_[i].request.task_id == task_id)
      return &image_analysis_tasks_[i].result;
  }
  return 0;
}

const std::vector<TaskChainRecord> &ParserUnifiedEntry::GetChainRecords() const
{
  return coordinator_.GetChainRecords();
}

bool ParserUnifiedEntry::ReplayTask(const std::string &task_id)
{
  ParserTaskUnit *task = coordinator_.FindTask(task_id);
  if (!task)
    return false;

  const std::string previous_mode = task->target.execution_mode;
  task->target.execution_mode = task_constants::ExecutionModeReplay();

  const bool ok = coordinator_.ExecuteTask(task_id);
  coordinator_.RecordReplayChain(task_id,
                                 task_constants::ExecutionModeReplay(),
                                 task_id);

  ParserTaskUnit *replayed_task = coordinator_.FindTask(task_id);
  if (replayed_task)
  {
    replayed_task->target.execution_mode = previous_mode;

    ParserTraceEntry trace_entry;
    trace_entry.sequence = static_cast<int>(replayed_task->evidence.trace_entries.size()) + 1;
    trace_entry.trace_id = replayed_task->evidence.trace_id;
    trace_entry.stage = "task_replay";
    trace_entry.action = "replay";
    trace_entry.status = ok ? "ok" : "failed";
    trace_entry.detail = "replay source=" + task_id;
    replayed_task->evidence.trace_entries.push_back(trace_entry);

    ParserLogEntry log_entry;
    log_entry.trace_id = replayed_task->evidence.trace_id;
    log_entry.level = ok ? "info" : "error";
    log_entry.stage = "task_replay";
    log_entry.code = "task_replay";
    log_entry.message = ok ? "task replay completed" : "task replay failed";
    replayed_task->evidence.log_entries.push_back(log_entry);

    ++last_tick_.replay_count;
    last_tick_.last_replay_task_id = replayed_task->task_id;
    last_tick_.last_replay_source_task_id = task_id;
    last_tick_.last_replay_modules = replayed_task->evidence.module_chain;
    CollectReplayStages(replayed_task->evidence, last_tick_.last_replay_stages);
  }

  return ok;
}

bool ParserUnifiedEntry::ExecuteImageAnalysisTask(const std::string &task_id)
{
  for (size_t i = 0; i < image_analysis_tasks_.size(); ++i)
  {
    ImageAnalysisTaskRecord &record = image_analysis_tasks_[i];
    if (record.request.task_id != task_id)
      continue;

    record.executed = true;
    record.success = image_analysis_node_.Execute(record.request, record.result);
    return record.success;
  }

  return false;
}

void ParserUnifiedEntry::RecordMainThreadTrace(const std::string &task_id)
{
  ParserTaskUnit *task = coordinator_.FindTask(task_id);
  if (!task)
  {
    for (size_t i = 0; i < image_analysis_tasks_.size(); ++i)
    {
      ImageAnalysisTaskRecord &record = image_analysis_tasks_[i];
      if (record.request.task_id != task_id)
        continue;

      ImageAnalysisTraceEntry trace_entry;
      trace_entry.sequence = static_cast<int>(record.result.trace_entries.size()) + 1;
      trace_entry.trace_id = record.result.trace_id;
      trace_entry.stage = "main_thread_cycle";
      trace_entry.status = record.success ? "ok" : "failed";
      trace_entry.message = last_tick_.thread_name + " cycle=" + std::to_string(last_tick_.cycle_index);
      record.result.trace_entries.push_back(trace_entry);

      ImageAnalysisLogEntry log_entry;
      log_entry.trace_id = record.result.trace_id;
      log_entry.level = record.success ? "info" : "error";
      log_entry.code = "main_thread_dispatch";
      log_entry.message = last_tick_.thread_name + " completed unified dispatch cycle";
      record.result.log_entries.push_back(log_entry);
      return;
    }

    return;
  }

  ParserTraceEntry trace_entry;
  trace_entry.sequence = static_cast<int>(task->evidence.trace_entries.size()) + 1;
  trace_entry.trace_id = task->evidence.trace_id;
  trace_entry.stage = "main_thread_cycle";
  trace_entry.action = "dispatch";
  trace_entry.status = task->status == pts_validated ? "ok" : "failed";
  trace_entry.detail = last_tick_.thread_name + " cycle=" + std::to_string(last_tick_.cycle_index);
  task->evidence.trace_entries.push_back(trace_entry);

  ParserLogEntry log_entry;
  log_entry.trace_id = task->evidence.trace_id;
  log_entry.level = task->status == pts_validated ? "info" : "error";
  log_entry.stage = "main_thread_cycle";
  log_entry.code = "main_thread_dispatch";
  log_entry.message = last_tick_.thread_name + " completed unified dispatch cycle";
  task->evidence.log_entries.push_back(log_entry);
}
}
