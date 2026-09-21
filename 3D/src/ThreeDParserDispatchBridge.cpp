#include "ThreeDParserDispatchBridge.h"

namespace codex_lan_agent_3d {

namespace {

template <typename RecordType, typename Predicate>
void Upsert(std::vector<RecordType> & records, const RecordType & incoming, Predicate predicate) {
    for (RecordType & existing : records) {
        if (predicate(existing)) {
            existing = incoming;
            return;
        }
    }
    records.push_back(incoming);
}

}  // namespace

void ThreeDParserDispatchBridge::UpsertCase(const ParserDispatchCaseConcept & spec) {
    Upsert(
        cases_,
        spec,
        [&spec](const ParserDispatchCaseConcept & existing) {
            return existing.module == spec.module &&
                   existing.layer == spec.layer &&
                   existing.case_id == spec.case_id;
        });
}

void ThreeDParserDispatchBridge::UpsertExecution(const ParserDispatchExecutionConcept & execution) {
    Upsert(
        executions_,
        execution,
        [&execution](const ParserDispatchExecutionConcept & existing) {
            return existing.task_id == execution.task_id && existing.trace_id == execution.trace_id;
        });
}

CommandResult ThreeDParserDispatchBridge::BuildCaseCatalogSummary() const {
    CommandResult result;
    std::vector<std::string> records;
    int active_runtime_count = 0;
    for (const ParserDispatchCaseConcept & spec : cases_) {
        if (spec.active_runtime) {
            ++active_runtime_count;
        }
        records.push_back(
            spec.module + ":" + spec.layer + ":" + spec.case_id + ":" +
            (spec.active_runtime ? "active" : "planned"));
    }
    result.fields["dispatch_case_count"] = std::to_string(cases_.size());
    result.fields["dispatch_active_runtime_count"] = std::to_string(active_runtime_count);
    result.fields["dispatch_cases"] = JoinStrings(records, "|");
    return result;
}

CommandResult ThreeDParserDispatchBridge::BuildExecutionSummary() const {
    CommandResult result;
    std::vector<std::string> records;
    int success_count = 0;
    int replaying_count = 0;
    for (const ParserDispatchExecutionConcept & execution : executions_) {
        if (execution.success) {
            ++success_count;
        }
        if (execution.replay_count > 0) {
            ++replaying_count;
        }
        records.push_back(
            execution.task_id + ":" + execution.module + ":" + execution.layer + ":" +
            execution.case_id + ":" + (execution.success ? "ok" : "fail"));
    }
    result.fields["dispatch_execution_count"] = std::to_string(executions_.size());
    result.fields["dispatch_success_count"] = std::to_string(success_count);
    result.fields["dispatch_replay_count"] = std::to_string(replaying_count);
    result.fields["dispatch_executions"] = JoinStrings(records, "|");
    return result;
}

ParserWorkflowConceptRecord ThreeDParserDispatchBridge::ToWorkflowRecord(
    const ParserDispatchExecutionConcept & execution) {
    ParserWorkflowConceptRecord workflow;
    workflow.task_id = execution.task_id;
    workflow.trace_id = execution.trace_id;
    workflow.module_name = execution.module;
    workflow.layer_name = execution.layer;
    workflow.case_id = execution.case_id;
    workflow.route_key = execution.route;
    workflow.execution_mode = execution.execution_mode;
    workflow.result_object = execution.result_object;
    workflow.summary = execution.summary;
    workflow.executed_task_count = execution.executed_task_count;
    workflow.replay_count = execution.replay_count;
    workflow.success = execution.success;
    return workflow;
}

}  // namespace codex_lan_agent_3d
