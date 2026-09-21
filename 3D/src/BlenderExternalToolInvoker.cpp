#include "BlenderExternalToolInvoker.h"

namespace codex_lan_agent_3d {

BlenderExternalToolInvoker::BlenderExternalToolInvoker(std::shared_ptr<ExternalToolCommandInvoker> command_invoker)
    : command_invoker_(std::move(command_invoker)) {
}

CommandResult BlenderExternalToolInvoker::CallTool(
    const std::string & tool_name,
    const std::map<std::string, std::string> & arguments) {
    if (command_invoker_ == nullptr) {
        CommandResult result;
        result.ok = false;
        result.exit_code = 1;
        result.fields["error"] = "Blender external command invoker is not configured";
        return result;
    }
    return command_invoker_->Invoke(tool_name, arguments);
}

}  // namespace codex_lan_agent_3d
