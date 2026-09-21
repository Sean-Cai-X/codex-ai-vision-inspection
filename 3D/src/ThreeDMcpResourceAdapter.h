#pragma once

#include "ThreeDSessionStateStore.h"

namespace codex_lan_agent_3d {

struct McpResourceDescriptor {
    std::string uri;
    std::string name;
    std::string description;
    std::string mime_type;
};

class ThreeDMcpResourceAdapter {
public:
    explicit ThreeDMcpResourceAdapter(std::shared_ptr<ThreeDSessionStateStore> state_store);

    std::vector<McpResourceDescriptor> ListResources(const std::string & session_id) const;
    CommandResult BuildResourcesListResult(const std::string & session_id) const;
    CommandResult ReadResource(const std::string & uri) const;

private:
    std::shared_ptr<ThreeDSessionStateStore> state_store_;
};

}  // namespace codex_lan_agent_3d
