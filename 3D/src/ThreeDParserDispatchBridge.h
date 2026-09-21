#pragma once

#include "ThreeDSceneBridge.h"

namespace codex_lan_agent_3d {

struct ParserDispatchCaseConcept {
    std::string layer;
    std::string module;
    std::string case_id;
    std::string script_path;
    std::string route;
    std::string task_subtype;
    std::string target_class;
    std::string target_method;
    std::string state;
    bool active_runtime = false;
    bool replay_after_run = false;
};

struct ParserDispatchExecutionConcept {
    std::string task_id;
    std::string trace_id;
    std::string module;
    std::string layer;
    std::string case_id;
    std::string route;
    std::string execution_mode;
    std::string result_object;
    std::string summary;
    int executed_task_count = 0;
    int replay_count = 0;
    bool success = false;
};

class ThreeDParserDispatchBridge {
public:
    void UpsertCase(const ParserDispatchCaseConcept & spec);
    void UpsertExecution(const ParserDispatchExecutionConcept & execution);

    CommandResult BuildCaseCatalogSummary() const;
    CommandResult BuildExecutionSummary() const;

    static ParserWorkflowConceptRecord ToWorkflowRecord(const ParserDispatchExecutionConcept & execution);

private:
    std::vector<ParserDispatchCaseConcept> cases_;
    std::vector<ParserDispatchExecutionConcept> executions_;
};

}  // namespace codex_lan_agent_3d
