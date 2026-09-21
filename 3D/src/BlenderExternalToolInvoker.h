#pragma once

#include "BlenderMcpSceneBackend.h"
#include "ExternalToolCommandInvoker.h"

namespace codex_lan_agent_3d {

class BlenderExternalToolInvoker : public IBlenderMcpInvoker {
public:
    explicit BlenderExternalToolInvoker(std::shared_ptr<ExternalToolCommandInvoker> command_invoker);

    CommandResult CallTool(
        const std::string & tool_name,
        const std::map<std::string, std::string> & arguments) override;

private:
    std::shared_ptr<ExternalToolCommandInvoker> command_invoker_;
};

}  // namespace codex_lan_agent_3d
