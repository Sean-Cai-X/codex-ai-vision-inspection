#include "ThreeDMcpAdapter.h"

namespace codex_lan_agent_3d {

ThreeDMcpAdapter::ThreeDMcpAdapter(std::shared_ptr<ThreeDControlSurface> control_surface)
    : control_surface_(std::move(control_surface)) {
}

std::vector<McpToolDescriptor> ThreeDMcpAdapter::ListTools() const {
    std::vector<McpToolDescriptor> descriptors;
    for (const ToolSpec & tool : control_surface_->ToolCatalog()) {
        ToolRouteContract route_contract;
        if (!control_surface_->TryGetRouteContract(tool.name, &route_contract)) {
            continue;
        }

        McpToolDescriptor descriptor;
        descriptor.name = tool.name;
        descriptor.description = tool.description;
        descriptor.route_contract = route_contract;
        for (const ToolSpec::ArgumentSpec & argument : tool.arguments) {
            descriptor.properties.push_back(McpToolInputProperty{
                argument.name,
                argument.type,
                argument.required,
                argument.description.empty() ? (argument.name + " as " + argument.type) : argument.description
            });
        }
        descriptors.push_back(descriptor);
    }
    return descriptors;
}

CommandResult ThreeDMcpAdapter::BuildToolsListResult() const {
    const std::vector<McpToolDescriptor> descriptors = ListTools();

    CommandResult result;
    result.fields["mcp_method"] = "tools/list";
    result.fields["tool_count"] = std::to_string(descriptors.size());

    int mutation_tool_count = 0;
    int observe_tool_count = 0;
    std::vector<std::string> names;
    std::vector<std::string> rows;
    for (const McpToolDescriptor & descriptor : descriptors) {
        names.push_back(descriptor.name);
        rows.push_back(
            descriptor.name + ":" +
            descriptor.route_contract.request_type + ":" +
            descriptor.route_contract.risk + ":" +
            BuildSchemaSummary(descriptor));
        if (descriptor.route_contract.request_type == "read_observe") {
            ++observe_tool_count;
        }
        else {
            ++mutation_tool_count;
        }
    }

    result.fields["tools"] = JoinStrings(names, "|");
    result.fields["tool_contract_rows"] = JoinStrings(rows, "|");
    result.fields["mutation_tool_count"] = std::to_string(mutation_tool_count);
    result.fields["observe_tool_count"] = std::to_string(observe_tool_count);
    result.fields["input_schema_policy"] = "additional_properties_false";
    return result;
}

CommandResult ThreeDMcpAdapter::DescribeTool(const std::string & tool_name) const {
    McpToolDescriptor descriptor;
    if (!TryFindDescriptor(tool_name, &descriptor)) {
        return BuildSchemaError(tool_name, "tool not found");
    }

    CommandResult result;
    result.fields["mcp_method"] = "tools/list";
    result.fields["tool_name"] = descriptor.name;
    result.fields["description"] = descriptor.description;
    result.fields["request_type"] = descriptor.route_contract.request_type;
    result.fields["risk"] = descriptor.route_contract.risk;
    result.fields["tags"] = descriptor.route_contract.tags;
    result.fields["input_schema"] = BuildSchemaSummary(descriptor);
    result.fields["required_argument_count"] = std::to_string(descriptor.properties.size());
    return result;
}

CommandResult ThreeDMcpAdapter::CallTool(
    const std::string & tool_name,
    const std::map<std::string, std::string> & arguments) const {
    McpToolDescriptor descriptor;
    if (!TryFindDescriptor(tool_name, &descriptor)) {
        return BuildSchemaError(tool_name, "tool not found");
    }

    for (const auto & entry : arguments) {
        if (!IsRecognizedArgument(descriptor, entry.first)) {
            return BuildSchemaError(tool_name, "unexpected argument: " + entry.first);
        }
    }

    std::string missing_key;
    if (IsRequiredArgumentMissing(descriptor, arguments, &missing_key)) {
        return BuildSchemaError(tool_name, "missing required argument: " + missing_key);
    }

    CommandResult result = control_surface_->ExecuteTool(tool_name, arguments);
    result.fields["mcp_method"] = "tools/call";
    result.fields["input_schema_policy"] = "additional_properties_false";
    return result;
}

bool ThreeDMcpAdapter::IsRecognizedArgument(
    const McpToolDescriptor & descriptor,
    const std::string & key) {
    for (const McpToolInputProperty & property : descriptor.properties) {
        if (property.name == key) {
            return true;
        }
    }
    return false;
}

bool ThreeDMcpAdapter::IsRequiredArgumentMissing(
    const McpToolDescriptor & descriptor,
    const std::map<std::string, std::string> & arguments,
    std::string * missing_key) {
    for (const McpToolInputProperty & property : descriptor.properties) {
        if (!property.required) {
            continue;
        }
        const auto it = arguments.find(property.name);
        if (it == arguments.end() || it->second.empty()) {
            if (missing_key != nullptr) {
                *missing_key = property.name;
            }
            return true;
        }
    }
    return false;
}

std::string ThreeDMcpAdapter::BuildSchemaSummary(const McpToolDescriptor & descriptor) {
    std::vector<std::string> rows;
    for (const McpToolInputProperty & property : descriptor.properties) {
        rows.push_back(
            property.name + ":" + property.type + ":" + (property.required ? "required" : "optional"));
    }
    return JoinStrings(rows, ",");
}

CommandResult ThreeDMcpAdapter::BuildSchemaError(
    const std::string & tool_name,
    const std::string & error) {
    CommandResult result;
    result.ok = false;
    result.exit_code = 2;
    result.fields["mcp_method"] = "tools/call";
    result.fields["tool_name"] = tool_name;
    result.fields["error"] = error;
    return result;
}

bool ThreeDMcpAdapter::TryFindDescriptor(
    const std::string & tool_name,
    McpToolDescriptor * descriptor) const {
    const std::vector<McpToolDescriptor> descriptors = ListTools();
    for (const McpToolDescriptor & item : descriptors) {
        if (item.name == tool_name) {
            if (descriptor != nullptr) {
                *descriptor = item;
            }
            return true;
        }
    }
    return false;
}

}  // namespace codex_lan_agent_3d
