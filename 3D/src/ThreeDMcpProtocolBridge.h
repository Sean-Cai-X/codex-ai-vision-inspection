#pragma once

#include "ThreeDMcpAdapter.h"
#include "ThreeDMcpResourceAdapter.h"

#include <memory>

namespace codex_lan_agent_3d {

struct McpRpcRequestEnvelope {
    std::string id_raw = "null";
    std::string method;
    std::string tool_name;
    std::string session_id;
    std::string resource_uri;
    std::map<std::string, std::string> arguments;
};

class ThreeDMcpProtocolBridge {
public:
    ThreeDMcpProtocolBridge(
        std::shared_ptr<ThreeDMcpAdapter> adapter,
        std::shared_ptr<ThreeDMcpResourceAdapter> resource_adapter);

    std::string BuildInitializeResponse(const std::string & id_raw) const;
    std::string BuildToolsListResponse(const std::string & id_raw) const;
    std::string BuildToolDescribeResponse(
        const std::string & id_raw,
        const std::string & tool_name) const;
    std::string BuildToolCallResponse(
        const std::string & id_raw,
        const std::string & tool_name,
        const std::map<std::string, std::string> & arguments) const;
    std::string BuildResourcesListResponse(
        const std::string & id_raw,
        const std::string & session_id) const;
    std::string BuildResourceReadResponse(
        const std::string & id_raw,
        const std::string & uri) const;
    std::string Dispatch(const McpRpcRequestEnvelope & request) const;

    static std::string BuildErrorResponse(
        const std::string & id_raw,
        int code,
        const std::string & message);

private:
    static std::string BuildCommandResultContentText(const CommandResult & result);
    static std::string BuildCommandResultJsonObject(const CommandResult & result);
    static std::string BuildToolDescriptorJson(const McpToolDescriptor & descriptor);
    static std::string BuildToolDescriptorListJson(const std::vector<McpToolDescriptor> & descriptors);
    static std::string BuildResourceDescriptorJson(const McpResourceDescriptor & descriptor);
    static std::string BuildResourceDescriptorListJson(const std::vector<McpResourceDescriptor> & descriptors);
    static std::string QuoteJson(const std::string & value);

    std::shared_ptr<ThreeDMcpAdapter> adapter_;
    std::shared_ptr<ThreeDMcpResourceAdapter> resource_adapter_;
};

}  // namespace codex_lan_agent_3d
