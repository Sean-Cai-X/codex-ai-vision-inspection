#include "ExternalToolCommandInvoker.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace codex_lan_agent_3d {

namespace {

std::string BuildRequestPath() {
    const auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / ("codex_lan_agent_3d_request_" + std::to_string(now) + ".txt");
    return path.string();
}

}  // namespace

ExternalToolCommandInvoker::ExternalToolCommandInvoker(ExternalToolCommandConfig config)
    : config_(std::move(config)) {
}

CommandResult ExternalToolCommandInvoker::Invoke(
    const std::string & tool_name,
    const std::map<std::string, std::string> & arguments) const {
    CommandResult result;
    result.ok = false;

    if (config_.executable_path.empty()) {
        result.exit_code = 1;
        result.fields["error"] = "external tool executable_path is empty";
        return result;
    }

    const std::string request_path = BuildRequestPath();
    {
        std::ofstream request(request_path, std::ios::out | std::ios::trunc);
        if (!request.is_open()) {
            result.exit_code = 1;
            result.fields["error"] = "failed to create request file";
            return result;
        }
        request << "tool=" << EncodeValue(tool_name) << "\n";
        for (const auto & entry : arguments) {
            request << "arg." << entry.first << "=" << EncodeValue(entry.second) << "\n";
        }
    }

    std::string command = QuoteArgument(config_.executable_path);
    for (const std::string & fixed_argument : config_.fixed_arguments) {
        command += " " + QuoteArgument(fixed_argument);
    }
    command += " --request-file " + QuoteArgument(request_path) + " 2>&1";
    const std::string shell_command = "cmd /d /c \"" + command + "\"";

    FILE * pipe = _popen(shell_command.c_str(), "r");
    if (pipe == nullptr) {
        std::filesystem::remove(request_path);
        result.exit_code = 1;
        result.fields["error"] = "failed to start external tool process";
        result.fields["command"] = shell_command;
        return result;
    }

    std::string output;
    char buffer[512];
    while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    const int process_exit_code = _pclose(pipe);
    std::filesystem::remove(request_path);

    bool saw_ok = false;
    bool saw_exit_code = false;
    std::istringstream lines(output);
    std::string line;
    while (std::getline(lines, line)) {
        ParseOutputLine(line, &result);
        const std::string trimmed = Trim(line);
        if (trimmed.find("ok=") == 0) {
            saw_ok = true;
        }
        if (trimmed.find("exit_code=") == 0) {
            saw_exit_code = true;
        }
    }

    if (!saw_ok) {
        result.ok = process_exit_code == 0;
    }
    if (!saw_exit_code) {
        result.exit_code = process_exit_code;
    }
    result.fields["command"] = shell_command;
    result.fields["command_output"] = Trim(output);
    if (!result.ok && result.fields.find("error") == result.fields.end()) {
        result.fields["error"] = result.fields["command_output"].empty()
            ? "external tool call failed"
            : result.fields["command_output"];
    }
    return result;
}

std::string ExternalToolCommandInvoker::QuoteArgument(const std::string & value) {
    std::string quoted = "\"";
    for (const char ch : value) {
        if (ch == '"') {
            quoted += "\\\"";
            continue;
        }
        quoted.push_back(ch);
    }
    quoted += "\"";
    return quoted;
}

std::string ExternalToolCommandInvoker::EncodeValue(const std::string & value) {
    return JsonEscape(value);
}

std::string ExternalToolCommandInvoker::DecodeValue(const std::string & value) {
    std::string decoded;
    decoded.reserve(value.size());
    bool escaping = false;
    for (const char ch : value) {
        if (!escaping) {
            if (ch == '\\') {
                escaping = true;
            }
            else {
                decoded.push_back(ch);
            }
            continue;
        }

        switch (ch) {
        case 'n':
            decoded.push_back('\n');
            break;
        case 'r':
            decoded.push_back('\r');
            break;
        case 't':
            decoded.push_back('\t');
            break;
        case '\\':
            decoded.push_back('\\');
            break;
        case '"':
            decoded.push_back('"');
            break;
        default:
            decoded.push_back(ch);
            break;
        }
        escaping = false;
    }
    if (escaping) {
        decoded.push_back('\\');
    }
    return decoded;
}

std::string ExternalToolCommandInvoker::Trim(const std::string & value) {
    std::size_t start = 0;
    while (start < value.size() && (value[start] == '\r' || value[start] == '\n' || value[start] == ' ' || value[start] == '\t')) {
        ++start;
    }

    std::size_t end = value.size();
    while (end > start && (value[end - 1] == '\r' || value[end - 1] == '\n' || value[end - 1] == ' ' || value[end - 1] == '\t')) {
        --end;
    }
    return value.substr(start, end - start);
}

bool ExternalToolCommandInvoker::TryParseBool(const std::string & value, bool * parsed) {
    if (parsed == nullptr) {
        return false;
    }

    const std::string normalized = ToKey(value);
    if (normalized == "true" || normalized == "1" || normalized == "yes" || normalized == "ok") {
        *parsed = true;
        return true;
    }
    if (normalized == "false" || normalized == "0" || normalized == "no" || normalized == "fail") {
        *parsed = false;
        return true;
    }
    return false;
}

void ExternalToolCommandInvoker::ParseOutputLine(const std::string & line, CommandResult * result) {
    if (result == nullptr) {
        return;
    }

    const std::string trimmed = Trim(line);
    if (trimmed.empty()) {
        return;
    }

    const std::size_t separator = trimmed.find('=');
    if (separator == std::string::npos) {
        return;
    }

    const std::string key = trimmed.substr(0, separator);
    const std::string value = DecodeValue(trimmed.substr(separator + 1));
    if (key == "ok") {
        bool parsed = false;
        if (TryParseBool(value, &parsed)) {
            result->ok = parsed;
        }
        return;
    }
    if (key == "exit_code") {
        result->exit_code = std::atoi(value.c_str());
        return;
    }
    if (key == "error") {
        result->fields["error"] = value;
        result->ok = false;
        return;
    }
    if (key.find("field.") == 0) {
        result->fields[key.substr(6)] = value;
    }
}

}  // namespace codex_lan_agent_3d
