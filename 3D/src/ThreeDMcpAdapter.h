#pragma once

#include "ThreeDControlSurface.h"

#include <memory>

namespace codex_lan_agent_3d {

struct McpToolInputProperty {
    std::string name;
    std::string type;
    bool required = false;
    std::string description;
};

struct McpToolDescriptor {
    std::string name;
    std::string description;
    std::vector<McpToolInputProperty> properties;
    ToolRouteContract route_contract;
};

class ThreeDMcpAdapter {
public:
    explicit ThreeDMcpAdapter(std::shared_ptr<ThreeDControlSurface> control_surface);

    std::vector<McpToolDescriptor> ListTools() const;
    CommandResult BuildToolsListResult() const;
    CommandResult DescribeTool(const std::string & tool_name) const;
    CommandResult CallTool(
        const std::string & tool_name,
        const std::map<std::string, std::string> & arguments) const;

private:
    static bool IsRecognizedArgument(
        const McpToolDescriptor & descriptor,
        const std::string & key);
    static bool IsRequiredArgumentMissing(
        const McpToolDescriptor & descriptor,
        const std::map<std::string, std::string> & arguments,
        std::string * missing_key);
    static std::string BuildSchemaSummary(const McpToolDescriptor & descriptor);
    static CommandResult BuildSchemaError(
        const std::string & tool_name,
        const std::string & error);

    bool TryFindDescriptor(
        const std::string & tool_name,
        McpToolDescriptor * descriptor) const;

    std::shared_ptr<ThreeDControlSurface> control_surface_;
};

}  // namespace codex_lan_agent_3d
