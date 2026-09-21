#include <cmath>
#include <iostream>

#include "../pipeline/parser_unified_entry.h"

int main()
{
  cxparser_ext::ExecutionTarget target;
  target.task_id = "pipeline_smoke_task";
  target.task_name = "pipeline_smoke";
  target.trace_id = "trace.pipeline_smoke";
  target.module_name = "cxparser_core";
  target.module_call.caller_module = "cxparser";
  target.module_call.callee_module = "cxparser_core";
  target.module_call.protocol_name = "cxparser.module.call";
  target.module_call.capability_name = "script_dispatch";
  target.module_call.method_name = "eval";
  target.script_text = "1+2";

  cxparser_ext::ParserBindingSpec spec;
  cxparser_ext::ParserUnifiedEntry entry;
  entry.SetBindingSpec(spec);

  if (!entry.SubmitTask(target))
  {
    std::cerr << "[FAIL] SubmitTask failed\n";
    return 1;
  }

  if (!entry.ExecuteMainThreadCycle())
  {
    const cxparser_ext::ParserTaskUnit *failed_task = entry.FindTask("pipeline_smoke_task");
    std::cerr << "[FAIL] ExecuteMainThreadCycle failed";
    if (failed_task)
      std::cerr << ": " << failed_task->failure_reason;
    std::cerr << "\n";
    return 1;
  }

  const cxparser_ext::ParserTaskUnit *task = entry.FindTask("pipeline_smoke_task");
  if (!task)
  {
    std::cerr << "[FAIL] task not found after unified dispatch\n";
    return 1;
  }

  if (!task->result.success)
  {
    std::cerr << "[FAIL] execution not successful\n";
    return 1;
  }

  if (std::fabs(task->result.scalar_result - 3.0) > 1e-9)
  {
    std::cerr << "[FAIL] unexpected result: " << task->result.scalar_result << "\n";
    return 1;
  }

  if (task->evidence.trace_entries.empty() || task->evidence.log_entries.empty())
  {
    std::cerr << "[FAIL] unified entry did not record trace/log\n";
    return 1;
  }

  const cxparser_ext::ParserMainThreadTick &tick = entry.GetLastTick();
  if (!tick.success || tick.executed_task_count != 1)
  {
    std::cerr << "[FAIL] main thread tick state mismatch\n";
    return 1;
  }

  std::cout << "[PASS] result=" << task->result.scalar_result
            << " validated=" << (task->report.passed ? "true" : "false")
            << " main_thread=" << tick.thread_name
            << " cycle=" << tick.cycle_index
            << "\n";
  return 0;
}
