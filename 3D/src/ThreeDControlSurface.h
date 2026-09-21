#pragma once

#include "ThreeDOrchestrator.h"

#include <map>
#include <memory>

namespace codex_lan_agent_3d {

struct ToolRouteContract {
    std::string tool_name;
    std::string request_type;
    std::string risk;
    std::string tags;
};

class ThreeDControlSurface {
public:
    explicit ThreeDControlSurface(std::shared_ptr<ThreeDOrchestrator> orchestrator);

    const std::vector<ToolSpec> & ToolCatalog() const;
    bool TryGetRouteContract(const std::string & tool_name, ToolRouteContract * contract) const;

    CommandResult BuildToolsCatalogResult() const;
    CommandResult ExecuteTool(
        const std::string & tool_name,
        const std::map<std::string, std::string> & arguments) const;

private:
    CommandResult HandleGenerateStructured(
        const std::map<std::string, std::string> & arguments) const;
    CommandResult HandleRegeneratePart(
        const std::map<std::string, std::string> & arguments) const;
    CommandResult HandleAddPart(
        const std::map<std::string, std::string> & arguments) const;
    CommandResult HandleArticulate(
        const std::map<std::string, std::string> & arguments) const;
    CommandResult HandleImportAsset(
        const std::map<std::string, std::string> & arguments) const;
    CommandResult HandleTransformObject(
        const std::map<std::string, std::string> & arguments) const;
    CommandResult HandleSceneSummary(
        const std::map<std::string, std::string> & arguments) const;
    CommandResult HandleSceneBridgeSummary(
        const std::map<std::string, std::string> & arguments) const;
    CommandResult HandleParserWorkflowSummary(
        const std::map<std::string, std::string> & arguments) const;

    static CommandResult BuildArgumentError(
        const std::string & tool_name,
        const std::string & message);
    static std::string GetArgumentOrDefault(
        const std::map<std::string, std::string> & arguments,
        const std::string & key,
        const std::string & fallback = std::string());
    static bool RequireArgument(
        const std::map<std::string, std::string> & arguments,
        const std::string & key,
        std::string * value,
        CommandResult * error,
        const std::string & tool_name);
    static void DecorateResult(
        const ToolRouteContract & contract,
        CommandResult * result);
    static const ToolRouteContract * FindRouteContract(const std::string & tool_name);

    std::shared_ptr<ThreeDOrchestrator> orchestrator_;
};

}  // namespace codex_lan_agent_3d
