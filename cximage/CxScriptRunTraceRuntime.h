#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

struct CxScriptRunTraceEvent
{
    std::string time_iso;
    std::string session_id;
    std::string case_id;
    std::string evidence_id;

    std::string phase;
    std::string status;
    std::string message;

    std::string image_id;
    std::string target_id;
    std::string script_id;
    std::string parameter_profile_id;

    int line_no = 0;
    std::string statement_type;
    std::string object_name;
    std::string method_name;

    long long elapsed_ms = 0;
};

class CxScriptRunTraceRuntime
{
public:
    bool open(const std::filesystem::path& caseDir,
              const std::string& sessionId,
              const std::string& caseId,
              std::string& reason);

    void set_case_context(
        const std::string& evidenceId,
        const std::string& imageId,
        const std::string& targetId,
        const std::string& scriptId,
        const std::string& parameterProfileId);

    void event(const std::string& phase,
               const std::string& status,
               const std::string& message);

    void script_line(int lineNo,
                     const std::string& statementType,
                     const std::string& status,
                     const std::string& message);

    void object_method(const std::string& objectName,
                       const std::string& methodName,
                       const std::string& status,
                       const std::string& message);

    void heartbeat(const std::string& phase,
                   const std::string& message);

    void close();

    std::filesystem::path trace_jsonl_path() const { return trace_jsonl_path_; }
    std::filesystem::path live_status_path() const { return live_status_path_; }

private:
    void write_jsonl(const CxScriptRunTraceEvent& e);
    void write_live_status(const CxScriptRunTraceEvent& e);

    std::mutex mutex_;
    std::ofstream jsonl_;
    std::filesystem::path trace_jsonl_path_;
    std::filesystem::path live_status_path_;

    std::string session_id_;
    std::string case_id_;
    std::string evidence_id_;
    std::string image_id_;
    std::string target_id_;
    std::string script_id_;
    std::string parameter_profile_id_;

    std::chrono::steady_clock::time_point begin_;
};

std::string CxDebugJsonEscape(const std::string& s);