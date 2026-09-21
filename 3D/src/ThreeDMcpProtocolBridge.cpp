#include "ThreeDMcpProtocolBridge.h"

#include <sstream>

namespace codex_lan_agent_3d {

ThreeDMcpProtocolBridge::ThreeDMcpProtocolBridge(
    std::shared_ptr<ThreeDMcpAdapter> adapter,
    std::shared_ptr<ThreeDMcpResourceAdapter> resource_adapter)
    : adapter_(std::move(adapter)),
      resource_adapter_(std::move(resource_adapter)) {
}

std::string ThreeDMcpProtocolBridge::BuildInitializeResponse(const std::string & id_raw) const {
    std::ostringstream stream;
    stream << "{"
           << "\"jsonrpc\":\"2.0\","
           << "\"id\":" << id_raw << ","
           << "\"result\":{"
           << "\"protocolVersion\":\"2025-03-26\","
           << "\"capabilities\":{\"tools\":{}},"
           << "\"serverInfo\":{\"name\":\"codex-lan-agent-3d\",\"version\":\"0.1.0\"}"
           << "}"
           << "}";
    return stream.str();
}

std::string ThreeDMcpProtocolBridge::BuildToolsListResponse(const std::string & id_raw) const {
    const std::vector<McpToolDescriptor> descriptors = adapter_->ListTools();
    std::ostringstream stream;
    stream << "{"
           << "\"jsonrpc\":\"2.0\","
           << "\"id\":" << id_raw << ","
           << "\"result\":{"
           << "\"tools\":" << BuildToolDescriptorListJson(descriptors)
           << "}"
           << "}";
    return stream.str();
}

std::string ThreeDMcpProtocolBridge::BuildToolDescribeResponse(
    const std::string & id_raw,
    const std::string & tool_name) const {
    CommandResult result = adapter_->DescribeTool(tool_name);
    if (!result.ok) {
        return BuildErrorResponse(id_raw, -32602, result.fields.at("error"));
    }

    std::ostringstream stream;
    stream << "{"
           << "\"jsonrpc\":\"2.0\","
           << "\"id\":" << id_raw << ","
           << "\"result\":{"
           << "\"content\":[{\"type\":\"text\",\"text\":"
           << QuoteJson(BuildCommandResultContentText(result))
           << "}],"
           << "\"structuredContent\":" << BuildCommandResultJsonObject(result)
           << ",\"isError\":false"
           << "}"
           << "}";
    return stream.str();
}

std::string ThreeDMcpProtocolBridge::BuildToolCallResponse(
    const std::string & id_raw,
    const std::string & tool_name,
    const std::map<std::string, std::string> & arguments) const {
    CommandResult result = adapter_->CallTool(tool_name, arguments);
    if (!result.ok && result.fields.find("error") != result.fields.end() &&
        result.fields.find("mcp_method") == result.fields.end()) {
        return BuildErrorResponse(id_raw, -32602, result.fields.at("error"));
    }

    std::ostringstream stream;
    stream << "{"
           << "\"jsonrpc\":\"2.0\","
           << "\"id\":" << id_raw << ","
           << "\"result\":{"
           << "\"content\":[{\"type\":\"text\",\"text\":"
           << QuoteJson(BuildCommandResultContentText(result))
           << "}],"
           << "\"structuredContent\":" << BuildCommandResultJsonObject(result)
           << ",\"isError\":" << (result.ok ? "false" : "true")
           << "}"
           << "}";
    return stream.str();
}

std::string ThreeDMcpProtocolBridge::BuildResourcesListResponse(
    const std::string & id_raw,
    const std::string & session_id) const {
    if (resource_adapter_ == nullptr) {
        return BuildErrorResponse(id_raw, -32601, "resource surface unavailable");
    }
    const std::vector<McpResourceDescriptor> descriptors = resource_adapter_->ListResources(session_id);
    std::ostringstream stream;
    stream << "{"
           << "\"jsonrpc\":\"2.0\","
           << "\"id\":" << id_raw << ","
           << "\"result\":{"
           << "\"resources\":" << BuildResourceDescriptorListJson(descriptors)
           << "}"
           << "}";
    return stream.str();
}

std::string ThreeDMcpProtocolBridge::BuildResourceReadResponse(
    const std::string & id_raw,
    const std::string & uri) const {
    if (resource_adapter_ == nullptr) {
        return BuildErrorResponse(id_raw, -32601, "resource surface unavailable");
    }
    CommandResult result = resource_adapter_->ReadResource(uri);
    if (!result.ok) {
        return BuildErrorResponse(id_raw, -32602, result.fields.at("error"));
    }

    std::ostringstream stream;
    stream << "{"
           << "\"jsonrpc\":\"2.0\","
           << "\"id\":" << id_raw << ","
           << "\"result\":{"
           << "\"contents\":[{"
           << "\"uri\":" << QuoteJson(result.fields["uri"]) << ","
           << "\"mimeType\":" << QuoteJson(result.fields["mime_type"]) << ","
           << "\"text\":" << QuoteJson(result.fields["content"])
           << "}],"
           << "\"isError\":false"
           << "}"
           << "}";
    return stream.str();
}

std::string ThreeDMcpProtocolBridge::Dispatch(const McpRpcRequestEnvelope & request) const {
    if (request.method == "initialize") {
        return BuildInitializeResponse(request.id_raw);
    }
    if (request.method == "tools/list") {
        return BuildToolsListResponse(request.id_raw);
    }
    if (request.method == "tools/describe") {
        return BuildToolDescribeResponse(request.id_raw, request.tool_name);
    }
    if (request.method == "tools/call") {
        return BuildToolCallResponse(request.id_raw, request.tool_name, request.arguments);
    }
    if (request.method == "resources/list") {
        return BuildResourcesListResponse(request.id_raw, request.session_id);
    }
    if (request.method == "resources/read") {
        return BuildResourceReadResponse(request.id_raw, request.resource_uri);
    }
    return BuildErrorResponse(request.id_raw, -32601, "method not found");
}

std::string ThreeDMcpProtocolBridge::BuildErrorResponse(
    const std::string & id_raw,
    int code,
    const std::string & message) {
    std::ostringstream stream;
    stream << "{"
           << "\"jsonrpc\":\"2.0\","
           << "\"id\":" << id_raw << ","
           << "\"error\":{"
           << "\"code\":" << code << ","
           << "\"message\":" << QuoteJson(message)
           << "}"
           << "}";
    return stream.str();
}

std::string ThreeDMcpProtocolBridge::BuildCommandResultContentText(const CommandResult & result) {
    std::vector<std::string> keys;
    for (const auto & entry : result.fields) {
        keys.push_back(entry.first);
    }
    std::sort(keys.begin(), keys.end());

    std::ostringstream stream;
    for (const std::string & key : keys) {
        const auto it = result.fields.find(key);
        if (it == result.fields.end()) {
            continue;
        }
        stream << key << "=" << it->second << "\n";
    }
    return stream.str();
}

std::string ThreeDMcpProtocolBridge::BuildCommandResultJsonObject(const CommandResult & result) {
    std::vector<std::string> keys;
    for (const auto & entry : result.fields) {
        keys.push_back(entry.first);
    }
    std::sort(keys.begin(), keys.end());

    std::ostringstream stream;
    stream << "{"
           << "\"ok\":" << (result.ok ? "true" : "false") << ","
           << "\"exit_code\":" << result.exit_code << ","
           << "\"fields\":{";
    for (std::size_t index = 0; index < keys.size(); ++index) {
        if (index > 0) {
            stream << ",";
        }
        const auto it = result.fields.find(keys[index]);
        stream << QuoteJson(keys[index]) << ":" << QuoteJson(it == result.fields.end() ? "" : it->second);
    }
    stream << "}}";
    return stream.str();
}

std::string ThreeDMcpProtocolBridge::BuildToolDescriptorJson(const McpToolDescriptor & descriptor) {
    std::ostringstream stream;
    stream << "{"
           << "\"name\":" << QuoteJson(descriptor.name) << ","
           << "\"description\":" << QuoteJson(descriptor.description) << ","
           << "\"inputSchema\":{"
           << "\"type\":\"object\","
           << "\"properties\":{";
    for (std::size_t index = 0; index < descriptor.properties.size(); ++index) {
        if (index > 0) {
            stream << ",";
        }
        const McpToolInputProperty & property = descriptor.properties[index];
        stream << QuoteJson(property.name) << ":{"
               << "\"type\":" << QuoteJson(property.type) << ","
               << "\"description\":" << QuoteJson(property.description)
               << "}";
    }
    stream << "},\"required\":[";
    bool wrote_required = false;
    for (const McpToolInputProperty & property : descriptor.properties) {
        if (!property.required) {
            continue;
        }
        if (wrote_required) {
            stream << ",";
        }
        stream << QuoteJson(property.name);
        wrote_required = true;
    }
    stream << "],"
           << "\"additionalProperties\":false"
           << "}"
           << "}";
    return stream.str();
}

std::string ThreeDMcpProtocolBridge::BuildToolDescriptorListJson(
    const std::vector<McpToolDescriptor> & descriptors) {
    std::ostringstream stream;
    stream << "[";
    for (std::size_t index = 0; index < descriptors.size(); ++index) {
        if (index > 0) {
            stream << ",";
        }
        stream << BuildToolDescriptorJson(descriptors[index]);
    }
    stream << "]";
    return stream.str();
}

std::string ThreeDMcpProtocolBridge::BuildResourceDescriptorJson(const McpResourceDescriptor & descriptor) {
    std::ostringstream stream;
    stream << "{"
           << "\"uri\":" << QuoteJson(descriptor.uri) << ","
           << "\"name\":" << QuoteJson(descriptor.name) << ","
           << "\"description\":" << QuoteJson(descriptor.description) << ","
           << "\"mimeType\":" << QuoteJson(descriptor.mime_type)
           << "}";
    return stream.str();
}

std::string ThreeDMcpProtocolBridge::BuildResourceDescriptorListJson(
    const std::vector<McpResourceDescriptor> & descriptors) {
    std::ostringstream stream;
    stream << "[";
    for (std::size_t index = 0; index < descriptors.size(); ++index) {
        if (index > 0) {
            stream << ",";
        }
        stream << BuildResourceDescriptorJson(descriptors[index]);
    }
    stream << "]";
    return stream.str();
}

std::string ThreeDMcpProtocolBridge::QuoteJson(const std::string & value) {
    return "\"" + JsonEscape(value) + "\"";
}

}  // namespace codex_lan_agent_3d
