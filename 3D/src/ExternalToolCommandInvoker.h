#pragma once

#include "ThreeDTypes.h"

namespace codex_lan_agent_3d {

struct ExternalToolCommandConfig {
    std::string executable_path;
    std::vector<std::string> fixed_arguments;
};

class ExternalToolCommandInvoker {
public:
    explicit ExternalToolCommandInvoker(ExternalToolCommandConfig config);

    CommandResult Invoke(
        const std::string & tool_name,
        const std::map<std::string, std::string> & arguments) const;

private:
    static std::string QuoteArgument(const std::string & value);
    static std::string EncodeValue(const std::string & value);
    static std::string DecodeValue(const std::string & value);
    static std::string Trim(const std::string & value);
    static bool TryParseBool(const std::string & value, bool * parsed);
    static void ParseOutputLine(const std::string & line, CommandResult * result);

    ExternalToolCommandConfig config_;
};

}  // namespace codex_lan_agent_3d
