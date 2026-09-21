#pragma once

#include "ThreeDHostedWorkflowCoordinator.h"

namespace codex_lan_agent_3d {

struct ValidationCheck {
    std::string id;
    bool ok = false;
    std::string detail;
};

class ThreeDHostValidationSuite {
public:
    ThreeDHostValidationSuite(
        std::shared_ptr<ThreeDHostedWorkflowCoordinator> coordinator,
        std::shared_ptr<ThreeDSessionStateStore> state_store);

    CommandResult RunNova3DValidationMatrix(
        const std::string & prompt,
        const std::string & preferred_model);
    CommandResult RunBlenderValidationMatrix();
    CommandResult RunEndToEndValidationMatrix(
        const std::string & task_intent,
        const std::string & trace_id,
        const std::string & prompt,
        const std::string & preferred_model);

private:
    static void AddCheck(
        std::vector<ValidationCheck> * checks,
        const std::string & id,
        bool ok,
        const std::string & detail);
    static CommandResult BuildValidationResult(
        const std::string & scope,
        const std::vector<ValidationCheck> & checks);
    static bool IsTruthyField(const CommandResult & result, const std::string & key);
    static std::string GetFieldOrDefault(
        const CommandResult & result,
        const std::string & key,
        const std::string & fallback = std::string());

    std::shared_ptr<ThreeDHostedWorkflowCoordinator> coordinator_;
    std::shared_ptr<ThreeDSessionStateStore> state_store_;
};

}  // namespace codex_lan_agent_3d
