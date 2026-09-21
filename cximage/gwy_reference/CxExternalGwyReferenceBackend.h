#pragma once

#include "gwy_reference/CxGwyReferenceBridge.h"

#include <cstdlib>
#include <cctype>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace cxvision::gwy_reference
{
namespace detail
{
inline std::string ReadAll(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open())
        return {};
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

inline std::string ExtractString(const std::string& text,
                                 const std::string& key)
{
    const std::string token = "\"" + key + "\"";
    const std::size_t keyPos = text.find(token);
    if (keyPos == std::string::npos)
        return {};
    const std::size_t colon = text.find(':', keyPos + token.size());
    const std::size_t quote = text.find('"', colon + 1);
    if (colon == std::string::npos || quote == std::string::npos)
        return {};
    std::string out;
    bool escape = false;
    for (std::size_t i = quote + 1; i < text.size(); ++i)
    {
        const char c = text[i];
        if (escape)
        {
            out += c == 'n' ? '\n' : c;
            escape = false;
        }
        else if (c == '\\')
            escape = true;
        else if (c == '"')
            return out;
        else
            out += c;
    }
    return {};
}

inline bool ExtractBool(const std::string& text,
                        const std::string& key,
                        bool fallback)
{
    const std::size_t keyPos = text.find("\"" + key + "\"");
    if (keyPos == std::string::npos)
        return fallback;
    const std::size_t colon = text.find(':', keyPos);
    if (colon == std::string::npos)
        return fallback;
    const std::size_t value = text.find_first_not_of(" \t\r\n", colon + 1);
    if (value == std::string::npos)
        return fallback;
    if (text.compare(value, 4, "true") == 0)
        return true;
    if (text.compare(value, 5, "false") == 0)
        return false;
    return fallback;
}

inline double ExtractNumber(const std::string& text,
                            const std::string& key,
                            double fallback)
{
    const std::size_t keyPos = text.find("\"" + key + "\"");
    if (keyPos == std::string::npos)
        return fallback;
    const std::size_t colon = text.find(':', keyPos);
    if (colon == std::string::npos)
        return fallback;
    const std::size_t begin = text.find_first_not_of(" \t\r\n", colon + 1);
    if (begin == std::string::npos)
        return fallback;
    try
    {
        return std::stod(text.substr(begin));
    }
    catch (...)
    {
        return fallback;
    }
}

inline void ExtractMetrics(const std::string& text,
                           std::map<std::string, double>& metrics)
{
    const std::string key = std::string(1, static_cast<char>(34)) + "metrics" +
                            std::string(1, static_cast<char>(34));
    const std::size_t keyPos = text.find(key);
    const std::size_t begin = text.find('{', keyPos);
    const std::size_t end = text.find('}', begin);
    if (keyPos == std::string::npos || begin == std::string::npos ||
        end == std::string::npos)
        return;
    std::size_t pos = begin + 1;
    while (pos < end)
    {
        const std::size_t q0 = text.find('"', pos);
        if (q0 == std::string::npos || q0 >= end) break;
        const std::size_t q1 = text.find('"', q0 + 1);
        const std::size_t colon = text.find(':', q1 + 1);
        if (q1 == std::string::npos || colon == std::string::npos || colon >= end) break;
        try { metrics[text.substr(q0 + 1, q1 - q0 - 1)] = std::stod(text.substr(colon + 1)); }
        catch (...) {}
        pos = text.find(',', colon + 1);
        if (pos == std::string::npos || pos >= end) break;
        ++pos;
    }
}

inline void ExtractPoints(const std::string& text,
                          std::vector<CxGwyPoint>& points)
{
    const std::string key = std::string(1, static_cast<char>(34)) + "points" +
                            std::string(1, static_cast<char>(34));
    const std::size_t keyPos = text.find(key);
    const std::size_t begin = text.find('[', keyPos);
    const std::size_t end = text.find(']', begin);
    if (keyPos == std::string::npos || begin == std::string::npos ||
        end == std::string::npos)
        return;
    std::size_t pos = begin;
    while ((pos = text.find('{', pos)) != std::string::npos && pos < end)
    {
        const std::size_t close = text.find('}', pos);
        if (close == std::string::npos || close > end) break;
        const std::string item = text.substr(pos, close - pos + 1);
        points.push_back({ExtractNumber(item, "x", 0.0),
                          ExtractNumber(item, "y", 0.0),
                          ExtractNumber(item, "value", 0.0)});
        pos = close + 1;
    }
}

inline void ExtractArtifacts(const std::string& text,
                             std::map<std::string, std::string>& artifacts)
{
    const std::string key = std::string(1, static_cast<char>(34)) + "artifacts" +
                            std::string(1, static_cast<char>(34));
    const std::size_t keyPos = text.find(key);
    const std::size_t begin = text.find('{', keyPos);
    const std::size_t end = text.find('}', begin);
    if (keyPos == std::string::npos || begin == std::string::npos ||
        end == std::string::npos)
        return;
    std::size_t pos = begin + 1;
    while (pos < end)
    {
        const std::size_t q0 = text.find('"', pos);
        if (q0 == std::string::npos || q0 >= end) break;
        const std::size_t q1 = text.find('"', q0 + 1);
        const std::size_t q2 = text.find('"', text.find(':', q1 + 1) + 1);
        const std::size_t q3 = text.find('"', q2 + 1);
        if (q1 == std::string::npos || q2 == std::string::npos ||
            q3 == std::string::npos || q3 > end) break;
        artifacts[text.substr(q0 + 1, q1 - q0 - 1)] =
            text.substr(q2 + 1, q3 - q2 - 1);
        pos = q3 + 1;
    }
}
#if defined(_WIN32)
inline bool RunHiddenProcess(const std::filesystem::path& executable,
                             const std::filesystem::path& requestPath,
                             const std::filesystem::path& resultPath,
                             unsigned long timeoutMs,
                             unsigned long& exitCode,
                             std::string& reason)
{
    std::wstring command = L"\"" + executable.wstring() + L"\" --request \"" +
                           requestPath.wstring() + L"\" --result \"" +
                           resultPath.wstring() + L"\"";
    std::vector<wchar_t> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back(L'\0');
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    const BOOL created = CreateProcessW(
        executable.wstring().c_str(), mutableCommand.data(), nullptr, nullptr,
        FALSE, CREATE_NO_WINDOW, nullptr, executable.parent_path().wstring().c_str(),
        &startup, &process);
    if (!created)
    {
        reason = "CreateProcessW failed for external GWY runner; error=" +
                 std::to_string(GetLastError());
        return false;
    }
    const DWORD wait = WaitForSingleObject(process.hProcess, timeoutMs);
    if (wait == WAIT_TIMEOUT)
    {
        TerminateProcess(process.hProcess, 124);
        WaitForSingleObject(process.hProcess, 2000);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        reason = "external GWY runner exceeded timeout";
        return false;
    }
    DWORD code = 1;
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    exitCode = code;
    if (code != 0)
    {
        reason = "external GWY runner exit_code=" + std::to_string(code);
        return false;
    }
    return true;
}
#endif
} // namespace detail

class CxExternalGwyReferenceBackend final : public IGwyReferenceBackend
{
public:
    explicit CxExternalGwyReferenceBackend(std::filesystem::path runnerPath,
                                           unsigned long timeoutMs = 10000)
        : m_runner_path(std::move(runnerPath)), m_timeout_ms(timeoutMs)
    {
    }

    CxGwyReferenceBackendDescriptor descriptor() const override
    {
        const bool available = std::filesystem::is_regular_file(m_runner_path);
        return {
            "gwy_reference_external_process",
            "1",
            available,
            true,
            available ? "external GWY reference runner available"
                      : "external GWY reference runner executable not found"
        };
    }

    CxGwyNormalizedResult execute(const CxGwyReferenceRequest& request) override
    {
        CxGwyNormalizedResult result;
        result.implementation = "gwy_reference_external_process";
        result.implementation_version = "1";
        const auto d = descriptor();
        if (!d.available)
        {
            result.status = "GWY_REFERENCE_NOT_BOUND";
            result.conclusion = "PENDING_BINDING";
            result.failure_stage = "reference_backend_discovery";
            result.reason = d.reason + "; path=" + m_runner_path.string();
            return result;
        }

        std::string safeId = request.request_id.empty() ? "request" : request.request_id;
        for (char& c : safeId)
        {
            if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_'))
                c = '_';
        }
        const std::filesystem::path exchangeDir =
            std::filesystem::temp_directory_path() / "cxvision_gwy_reference" / safeId;
        std::error_code ec;
        std::filesystem::create_directories(exchangeDir, ec);
        if (ec)
        {
            result.status = "GWY_REFERENCE_TRANSPORT_FAIL";
            result.conclusion = "FAIL";
            result.failure_stage = "exchange_directory";
            result.reason = ec.message();
            return result;
        }
        const auto requestPath = exchangeDir / "request.json";
        const auto resultPath = exchangeDir / "result.json";
        std::string ioReason;
        if (!WriteText(requestPath, SerializeRequest(request), ioReason))
        {
            result.status = "GWY_REFERENCE_TRANSPORT_FAIL";
            result.conclusion = "FAIL";
            result.failure_stage = "request_write";
            result.reason = ioReason;
            return result;
        }

#if defined(_WIN32)
        unsigned long exitCode = 1;
        if (!detail::RunHiddenProcess(m_runner_path, requestPath, resultPath,
                                      m_timeout_ms, exitCode, ioReason))
        {
            result.status = "GWY_REFERENCE_EXECUTION_FAIL";
            result.conclusion = "FAIL";
            result.failure_stage = "external_runner";
            result.reason = ioReason;
            result.backend_available = true;
            return result;
        }
#else
        result.status = "GWY_REFERENCE_PLATFORM_NOT_SUPPORTED";
        result.conclusion = "PENDING_BINDING";
        result.failure_stage = "external_runner_platform";
        result.reason = "external runner process adapter currently supports Windows";
        return result;
#endif

        const std::string text = detail::ReadAll(resultPath);
        if (text.empty())
        {
            result.status = "GWY_REFERENCE_RESULT_MISSING";
            result.conclusion = "FAIL";
            result.failure_stage = "result_read";
            result.reason = "external runner did not produce a readable result";
            result.backend_available = true;
            return result;
        }
        result.implementation = detail::ExtractString(text, "implementation");
        result.implementation_version = detail::ExtractString(text, "implementation_version");
        result.source_hash = detail::ExtractString(text, "source_hash");
        result.status = detail::ExtractString(text, "status");
        result.conclusion = detail::ExtractString(text, "conclusion");
        result.failure_stage = detail::ExtractString(text, "failure_stage");
        result.reason = detail::ExtractString(text, "reason");
        result.backend_available = true;
        result.executed = detail::ExtractBool(text, "executed", true);
        result.algorithm_success = detail::ExtractBool(text, "algorithm_success", false);
        result.promotion_allowed = false;
        result.elapsed_ms = detail::ExtractNumber(text, "elapsed_ms", 0.0);
        detail::ExtractMetrics(text, result.metrics);
        result.artifacts["runner_raw_result"] = resultPath.string();
        return result;
    }

private:
    std::filesystem::path m_runner_path;
    unsigned long m_timeout_ms = 10000;
};

inline std::filesystem::path ResolveDefaultGwyReferenceRunnerPath()
{
    if (const char* configured = std::getenv("CXVISION_GWY_REF_RUNNER"))
    {
        if (*configured)
            return configured;
    }
    return std::filesystem::path("gwyddion") / "gwy_ref_runner" / "bin" /
           "gwy_ref_runner.exe";
}

inline std::unique_ptr<IGwyReferenceBackend> CreateGwyReferenceBackend()
{
    const auto path = ResolveDefaultGwyReferenceRunnerPath();
    if (std::filesystem::is_regular_file(path))
        return std::make_unique<CxExternalGwyReferenceBackend>(path);
    return std::make_unique<CxNullGwyReferenceBackend>();
}

} // namespace cxvision::gwy_reference