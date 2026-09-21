#pragma once

#include "ExternalToolCommandInvoker.h"
#include "Nova3DMcpHostedBackend.h"

namespace codex_lan_agent_3d {

class Nova3DExternalToolInvoker : public INova3DMcpInvoker {
public:
    explicit Nova3DExternalToolInvoker(std::shared_ptr<ExternalToolCommandInvoker> command_invoker);

    CommandResult CallTool(
        const std::string & tool_name,
        const std::map<std::string, std::string> & arguments) override;

private:
    std::shared_ptr<ExternalToolCommandInvoker> command_invoker_;
};

}  // namespace codex_lan_agent_3d
