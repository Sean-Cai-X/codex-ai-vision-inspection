#include "CxScriptRunTraceRuntime.h"
#include "CxUnifiedLog.h"
#include <iomanip>
#include <iostream>
#include <sstream>

std::string CxDebugJsonEscape(const std::string& s)
{
    std::string result;
    result.reserve(s.size());

    for (char c : s)
    {
        switch (c)
        {
        case '\\': result += "\\\\"; break;
        case '"': result += "\\\""; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default: result += c; break;
        }
    }

    return result;
}

bool CxScriptRunTraceRuntime::open(const std::filesystem::path& caseDir,
                                   const std::string& sessionId,
                                   const std::string& caseId,
                                   std::string& reason)
{
    std::lock_guard<std::mutex> lock(mutex_);

    std::filesystem::create_directories(caseDir);

    trace_jsonl_path_ = caseDir / "run_trace.jsonl";
    live_status_path_ = caseDir / "live_status.json";

    jsonl_.open(trace_jsonl_path_, std::ios::out | std::ios::app);
    if (!jsonl_.is_open())
    {
        reason = "Cannot open run_trace.jsonl: " + trace_jsonl_path_.string();
        return false;
    }

    session_id_ = sessionId;
    case_id_ = caseId;
    begin_ = std::chrono::steady_clock::now();

    CxUnifiedLogContext logContext;
    logContext.session_id = sessionId;
    logContext.case_id = caseId;
    CxScopedLogContext scopedContext(logContext);
    CXLOG_INFO("CxScriptRunTraceRuntime", "open", "success", 
               "case_dir=" + caseDir.string());

    return true;
}

void CxScriptRunTraceRuntime::set_case_context(
    const std::string& evidenceId,
    const std::string& imageId,
    const std::string& targetId,
    const std::string& scriptId,
    const std::string& parameterProfileId)
{
    std::lock_guard<std::mutex> lock(mutex_);

    evidence_id_ = evidenceId;
    image_id_ = imageId;
    target_id_ = targetId;
    script_id_ = scriptId;
    parameter_profile_id_ = parameterProfileId;
}

void CxScriptRunTraceRuntime::event(const std::string& phase,
                                    const std::string& status,
                                    const std::string& message)
{
    std::lock_guard<std::mutex> lock(mutex_);

    CxScriptRunTraceEvent e;

    auto now = std::chrono::steady_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - begin_).count();

    std::time_t now_c = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());
    std::tm now_tm;
    localtime_s(&now_tm, &now_c);
    std::stringstream ss;
    ss << std::put_time(&now_tm, "%Y-%m-%dT%H:%M:%S");
    e.time_iso = ss.str();

    e.session_id = session_id_;
    e.case_id = case_id_;
    e.evidence_id = evidence_id_;
    e.phase = phase;
    e.status = status;
    e.message = message;
    e.image_id = image_id_;
    e.target_id = target_id_;
    e.script_id = script_id_;
    e.parameter_profile_id = parameter_profile_id_;
    e.elapsed_ms = elapsed_ms;

    write_jsonl(e);
    write_live_status(e);

    CxUnifiedLogContext logContext;
    logContext.session_id = session_id_;
    logContext.case_id = case_id_;
    logContext.evidence_id = evidence_id_;
    logContext.image_id = image_id_;
    logContext.target_id = target_id_;
    logContext.script_id = script_id_;
    logContext.parameter_profile_id = parameter_profile_id_;
    CxScopedLogContext scopedContext(logContext);
    CXLOG_INFO("CxScriptRunTraceRuntime", phase.c_str(), status, message);
}

void CxScriptRunTraceRuntime::script_line(int lineNo,
                                          const std::string& statementType,
                                          const std::string& status,
                                          const std::string& message)
{
    std::lock_guard<std::mutex> lock(mutex_);

    CxScriptRunTraceEvent e;

    auto now = std::chrono::steady_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - begin_).count();

    std::time_t now_c = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());
    std::tm now_tm;
    localtime_s(&now_tm, &now_c);
    std::stringstream ss;
    ss << std::put_time(&now_tm, "%Y-%m-%dT%H:%M:%S");
    e.time_iso = ss.str();

    e.session_id = session_id_;
    e.case_id = case_id_;
    e.evidence_id = evidence_id_;
    e.phase = "cxscript";
    e.status = status;
    e.message = message;
    e.line_no = lineNo;
    e.statement_type = statementType;
    e.image_id = image_id_;
    e.target_id = target_id_;
    e.script_id = script_id_;
    e.parameter_profile_id = parameter_profile_id_;
    e.elapsed_ms = elapsed_ms;

    write_jsonl(e);
}

void CxScriptRunTraceRuntime::object_method(const std::string& objectName,
                                            const std::string& methodName,
                                            const std::string& status,
                                            const std::string& message)
{
    std::lock_guard<std::mutex> lock(mutex_);

    CxScriptRunTraceEvent e;

    auto now = std::chrono::steady_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - begin_).count();

    std::time_t now_c = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());
    std::tm now_tm;
    localtime_s(&now_tm, &now_c);
    std::stringstream ss;
    ss << std::put_time(&now_tm, "%Y-%m-%dT%H:%M:%S");
    e.time_iso = ss.str();

    e.session_id = session_id_;
    e.case_id = case_id_;
    e.evidence_id = evidence_id_;
    e.phase = "object_method";
    e.status = status;
    e.message = message;
    e.object_name = objectName;
    e.method_name = methodName;
    e.image_id = image_id_;
    e.target_id = target_id_;
    e.script_id = script_id_;
    e.parameter_profile_id = parameter_profile_id_;
    e.elapsed_ms = elapsed_ms;

    write_jsonl(e);
}

void CxScriptRunTraceRuntime::heartbeat(const std::string& phase,
                                        const std::string& message)
{
    std::lock_guard<std::mutex> lock(mutex_);

    CxScriptRunTraceEvent e;

    auto now = std::chrono::steady_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - begin_).count();

    std::time_t now_c = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());
    std::tm now_tm;
    localtime_s(&now_tm, &now_c);
    std::stringstream ss;
    ss << std::put_time(&now_tm, "%Y-%m-%dT%H:%M:%S");
    e.time_iso = ss.str();

    e.session_id = session_id_;
    e.case_id = case_id_;
    e.evidence_id = evidence_id_;
    e.phase = phase;
    e.status = "heartbeat";
    e.message = message;
    e.image_id = image_id_;
    e.target_id = target_id_;
    e.script_id = script_id_;
    e.parameter_profile_id = parameter_profile_id_;
    e.elapsed_ms = elapsed_ms;

    write_jsonl(e);
    write_live_status(e);
}

void CxScriptRunTraceRuntime::close()
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (jsonl_.is_open())
    {
        jsonl_.close();
    }

    CxUnifiedLogContext logContext;
    logContext.session_id = session_id_;
    logContext.case_id = case_id_;
    CxScopedLogContext scopedContext(logContext);
    CXLOG_INFO("CxScriptRunTraceRuntime", "close", "completed", 
               "case_id=" + case_id_);
}

void CxScriptRunTraceRuntime::write_jsonl(const CxScriptRunTraceEvent& e)
{
    if (!jsonl_.is_open())
        return;

    jsonl_ << "{";
    jsonl_ << "\"time_iso\":\"" << CxDebugJsonEscape(e.time_iso) << "\",";
    jsonl_ << "\"session_id\":\"" << CxDebugJsonEscape(e.session_id) << "\",";
    jsonl_ << "\"case_id\":\"" << CxDebugJsonEscape(e.case_id) << "\",";
    jsonl_ << "\"evidence_id\":\"" << CxDebugJsonEscape(e.evidence_id) << "\",";
    jsonl_ << "\"phase\":\"" << CxDebugJsonEscape(e.phase) << "\",";
    jsonl_ << "\"status\":\"" << CxDebugJsonEscape(e.status) << "\",";
    jsonl_ << "\"message\":\"" << CxDebugJsonEscape(e.message) << "\",";
    jsonl_ << "\"image_id\":\"" << CxDebugJsonEscape(e.image_id) << "\",";
    jsonl_ << "\"target_id\":\"" << CxDebugJsonEscape(e.target_id) << "\",";
    jsonl_ << "\"script_id\":\"" << CxDebugJsonEscape(e.script_id) << "\",";
    jsonl_ << "\"parameter_profile_id\":\"" << CxDebugJsonEscape(e.parameter_profile_id) << "\",";
    jsonl_ << "\"line_no\":" << e.line_no << ",";
    jsonl_ << "\"statement_type\":\"" << CxDebugJsonEscape(e.statement_type) << "\",";
    jsonl_ << "\"object_name\":\"" << CxDebugJsonEscape(e.object_name) << "\",";
    jsonl_ << "\"method_name\":\"" << CxDebugJsonEscape(e.method_name) << "\",";
    jsonl_ << "\"elapsed_ms\":" << e.elapsed_ms;
    jsonl_ << "}\n";
    jsonl_.flush();
}

void CxScriptRunTraceRuntime::write_live_status(const CxScriptRunTraceEvent& e)
{
    std::ofstream liveFile(live_status_path_);
    if (!liveFile.is_open())
        return;

    liveFile << "{\n";
    liveFile << "  \"time_iso\": \"" << CxDebugJsonEscape(e.time_iso) << "\",\n";
    liveFile << "  \"session_id\": \"" << CxDebugJsonEscape(e.session_id) << "\",\n";
    liveFile << "  \"case_id\": \"" << CxDebugJsonEscape(e.case_id) << "\",\n";
    liveFile << "  \"evidence_id\": \"" << CxDebugJsonEscape(e.evidence_id) << "\",\n";
    liveFile << "  \"phase\": \"" << CxDebugJsonEscape(e.phase) << "\",\n";
    liveFile << "  \"status\": \"" << CxDebugJsonEscape(e.status) << "\",\n";
    liveFile << "  \"message\": \"" << CxDebugJsonEscape(e.message) << "\",\n";
    liveFile << "  \"image_id\": \"" << CxDebugJsonEscape(e.image_id) << "\",\n";
    liveFile << "  \"target_id\": \"" << CxDebugJsonEscape(e.target_id) << "\",\n";
    liveFile << "  \"script_id\": \"" << CxDebugJsonEscape(e.script_id) << "\",\n";
    liveFile << "  \"parameter_profile_id\": \"" << CxDebugJsonEscape(e.parameter_profile_id) << "\",\n";
    liveFile << "  \"elapsed_ms\": " << e.elapsed_ms << "\n";
    liveFile << "}\n";
}