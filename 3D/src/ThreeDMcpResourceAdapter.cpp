#include "ThreeDMcpResourceAdapter.h"

namespace codex_lan_agent_3d {

ThreeDMcpResourceAdapter::ThreeDMcpResourceAdapter(std::shared_ptr<ThreeDSessionStateStore> state_store)
    : state_store_(std::move(state_store)) {
}

std::vector<McpResourceDescriptor> ThreeDMcpResourceAdapter::ListResources(const std::string & session_id) const {
    std::vector<McpResourceDescriptor> descriptors;
    for (const auto & entry : std::vector<std::pair<std::string, std::string>>{
             {"summary", "Session summary and current active ids"},
             {"asset/current", "Current structured asset snapshot"},
             {"scene/current", "Current Blender scene snapshot"},
             {"scene/viewport_latest", "Latest captured Blender viewport artifact"},
             {"workflow/nova_status", "Current Nova3D workflow state and progress"},
             {"hosts/nova3d", "Nova3D host binding and workspace state"},
             {"hosts/blender", "Blender host binding and scene state"}}) {
        descriptors.push_back(McpResourceDescriptor{
            "session://" + session_id + "/" + entry.first,
            entry.first,
            entry.second,
            "text/plain"
        });
    }
    return descriptors;
}

CommandResult ThreeDMcpResourceAdapter::BuildResourcesListResult(const std::string & session_id) const {
    CommandResult result = state_store_->BuildResourceCatalog(session_id);
    result.fields["mcp_method"] = "resources/list";
    return result;
}

CommandResult ThreeDMcpResourceAdapter::ReadResource(const std::string & uri) const {
    CommandResult result = state_store_->ReadResource(uri);
    result.fields["mcp_method"] = "resources/read";
    return result;
}

}  // namespace codex_lan_agent_3d
