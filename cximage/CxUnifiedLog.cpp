#include "pch.h"
#include "CxUnifiedLog.h"
#include <sstream>
#include <iomanip>
#include <ctime>
#include <iostream>
#include <Windows.h>

thread_local CxUnifiedLogContext CxUnifiedLog::thread_context_;

CxUnifiedLog::CxUnifiedLog() = default;

CxUnifiedLog::~CxUnifiedLog()
{
    if (file_stream_.is_open())
    {
        file_stream_.close();
    }
}

CxUnifiedLog& CxUnifiedLog::Instance()
{
    static CxUnifiedLog instance;
    return instance;
}

bool CxUnifiedLog::Initialize(
    const std::filesystem::path& path,
    const std::string& runMode,
    int argc,
    char** argv,
    std::string& reason)
{
    if (initialized_)
    {
        reason = "already initialized";
        return false;
    }

    log_path_ = path;
    mode_ = runMode;
    run_id_ = GenerateRunId();
    start_time_ = std::chrono::steady_clock::now();

    auto dir = log_path_.parent_path();
    if (!dir.empty() && !std::filesystem::exists(dir))
    {
        try
        {
            std::filesystem::create_directories(dir);
        }
        catch (const std::exception& e)
        {
            reason = "failed to create log directory: " + std::string(e.what());
            return false;
        }
    }

    file_stream_.open(
        log_path_,
        std::ios::out | std::ios::app | std::ios::binary);

    if (!file_stream_.is_open())
    {
        reason = "failed to open log file: " + log_path_.string();
        return false;
    }

    thread_context_.run_id = run_id_;
    thread_context_.mode = mode_;

    initialized_ = true;

    std::ostringstream oss;
    oss << "run_id=" << run_id_ << ", mode=" << mode_ << ", path=" << log_path_.string();
    WriteLine(SerializeEvent({
        FormatTimestamp(),
        run_id_,
        GetCurrentProcessId(),
        GetCurrentThreadId(),
        sequence_++,
        CxLogLevel::Info,
        "CxUnifiedLog",
        "initialize",
        "",
        "success",
        oss.str(),
        thread_context_,
        __FILE__,
        __FUNCTION__,
        __LINE__,
        0
    }));

    return true;
}

void CxUnifiedLog::RawFallbackWrite(const std::string& message)
{
    HANDLE stderr_handle = GetStdHandle(STD_ERROR_HANDLE);
    if (stderr_handle != INVALID_HANDLE_VALUE)
    {
        std::string msg = "[CxUnifiedLog] " + message + "\n";
        DWORD bytes_written;
        WriteFile(stderr_handle, msg.c_str(), (DWORD)msg.size(), &bytes_written, nullptr);
    }
    OutputDebugStringA(("[CxUnifiedLog] " + message).c_str());
}

void CxUnifiedLog::Shutdown(int exitCode, const std::string& conclusion)
{
    if (!initialized_)
        return;

    auto elapsed = std::chrono::steady_clock::now() - start_time_;
    long long elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    std::ostringstream oss;
    oss << "exit_code=" << exitCode << ", conclusion=" << conclusion << ", elapsed_ms=" << elapsed_ms;

    std::string status = exitCode == 0 ? "completed" : "failed";

    WriteLine(SerializeEvent({
        FormatTimestamp(),
        run_id_,
        GetCurrentProcessId(),
        GetCurrentThreadId(),
        sequence_++,
        exitCode == 0 ? CxLogLevel::Info : CxLogLevel::Warning,
        "CxUnifiedLog",
        "run_end",
        "",
        status,
        oss.str(),
        thread_context_,
        __FILE__,
        __FUNCTION__,
        __LINE__,
        elapsed_ms
    }));

    Flush();

    if (file_stream_.is_open())
    {
        file_stream_.close();
    }

    initialized_ = false;
}

bool CxUnifiedLog::IsInitialized() const
{
    return initialized_;
}

const std::string& CxUnifiedLog::RunId() const
{
    return run_id_;
}

const std::filesystem::path& CxUnifiedLog::Path() const
{
    return log_path_;
}

const std::string& CxUnifiedLog::Mode() const
{
    return mode_;
}

void CxUnifiedLog::SetContext(const CxUnifiedLogContext& context)
{
    thread_context_ = context;
    thread_context_.run_id = run_id_;
    thread_context_.mode = mode_;
}

CxUnifiedLogContext CxUnifiedLog::Context() const
{
    return thread_context_;
}

void CxUnifiedLog::Write(
    CxLogLevel level,
    const char* component,
    const char* event,
    const std::string& status,
    const std::string& message,
    const char* file,
    int line,
    const char* function)
{
    if (!initialized_)
        return;

    auto elapsed = std::chrono::steady_clock::now() - start_time_;
    long long elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    CxUnifiedLogEvent ev{
        FormatTimestamp(),
        run_id_,
        GetCurrentProcessId(),
        GetCurrentThreadId(),
        sequence_++,
        level,
        component,
        event,
        thread_context_.phase,
        status,
        message,
        thread_context_,
        file,
        function,
        line,
        elapsed_ms
    };

    WriteLine(SerializeEvent(ev));

    if (level >= CxLogLevel::Warning)
    {
        Flush();
    }
}

void CxUnifiedLog::Flush()
{
    if (!initialized_ || !file_stream_.is_open())
        return;

    std::lock_guard<std::mutex> lock(mutex_);
    file_stream_.flush();
}

std::string CxUnifiedLog::GenerateRunId() const
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    struct tm tm_buf;
    localtime_s(&tm_buf, &time_t);

    char buf[64];
    sprintf_s(buf, "run-%04d%02d%02d-%02d%02d%02d-p%lu",
        tm_buf.tm_year + 1900,
        tm_buf.tm_mon + 1,
        tm_buf.tm_mday,
        tm_buf.tm_hour,
        tm_buf.tm_min,
        tm_buf.tm_sec,
        GetCurrentProcessId());

    return buf;
}

std::string CxUnifiedLog::FormatTimestamp() const
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - std::chrono::system_clock::from_time_t(time_t));

    struct tm tm_buf;
    localtime_s(&tm_buf, &time_t);

    char buf[64];
    sprintf_s(buf, "%.4d-%.2d-%.2dT%.2d:%.2d:%.2d.%.3d+08:00",
        tm_buf.tm_year + 1900,
        tm_buf.tm_mon + 1,
        tm_buf.tm_mday,
        tm_buf.tm_hour,
        tm_buf.tm_min,
        tm_buf.tm_sec,
        (int)(ms.count() % 1000));

    return buf;
}

std::string CxUnifiedLog::SerializeEvent(const CxUnifiedLogEvent& event) const
{
    std::ostringstream oss;
    oss << "{";
    oss << "\"timestamp\":\"" << EscapeJsonString(event.timestamp_iso) << "\",";
    oss << "\"run_id\":\"" << EscapeJsonString(event.run_id) << "\",";
    oss << "\"pid\":" << event.process_id << ",";
    oss << "\"tid\":" << event.thread_id << ",";
    oss << "\"seq\":" << event.sequence << ",";
    oss << "\"level\":\"" << LevelToString(event.level) << "\",";
    oss << "\"component\":\"" << EscapeJsonString(event.component) << "\",";
    oss << "\"event\":\"" << EscapeJsonString(event.event) << "\",";
    oss << "\"phase\":\"" << EscapeJsonString(event.phase) << "\",";
    oss << "\"status\":\"" << EscapeJsonString(event.status) << "\",";
    oss << "\"message\":\"" << EscapeJsonString(event.message) << "\",";

    oss << "\"context\":{";
    oss << "\"mode\":\"" << EscapeJsonString(event.context.mode) << "\",";
    oss << "\"session_id\":\"" << EscapeJsonString(event.context.session_id) << "\",";
    oss << "\"suite_id\":\"" << EscapeJsonString(event.context.suite_id) << "\",";
    oss << "\"case_id\":\"" << EscapeJsonString(event.context.case_id) << "\",";
    oss << "\"evidence_id\":\"" << EscapeJsonString(event.context.evidence_id) << "\",";
    oss << "\"image_id\":\"" << EscapeJsonString(event.context.image_id) << "\",";
    oss << "\"target_id\":\"" << EscapeJsonString(event.context.target_id) << "\",";
    oss << "\"script_id\":\"" << EscapeJsonString(event.context.script_id) << "\",";
    oss << "\"parameter_profile_id\":\"" << EscapeJsonString(event.context.parameter_profile_id) << "\",";
    oss << "\"tool\":\"" << EscapeJsonString(event.context.tool) << "\",";
    oss << "\"object_ref\":\"" << EscapeJsonString(event.context.object_ref) << "\",";
    oss << "\"shape_ref\":\"" << EscapeJsonString(event.context.shape_ref) << "\"";
    oss << "},";

    oss << "\"source_file\":\"" << EscapeJsonString(event.source_file) << "\",";
    oss << "\"source_function\":\"" << EscapeJsonString(event.source_function) << "\",";
    oss << "\"source_line\":" << event.source_line << ",";
    oss << "\"elapsed_ms\":" << event.elapsed_ms;
    oss << "}";

    return oss.str();
}

std::string CxUnifiedLog::EscapeJsonString(const std::string& s) const
{
    std::ostringstream oss;
    for (char c : s)
    {
        switch (c)
        {
        case '\\': oss << "\\\\"; break;
        case '"': oss << "\\\""; break;
        case '\n': oss << "\\n"; break;
        case '\r': oss << "\\r"; break;
        case '\t': oss << "\\t"; break;
        default:
            if (c >= 0 && c < 0x20)
                oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)c;
            else
                oss << c;
            break;
        }
    }
    return oss.str();
}

std::string CxUnifiedLog::LevelToString(CxLogLevel level) const
{
    switch (level)
    {
    case CxLogLevel::Trace: return "TRACE";
    case CxLogLevel::Debug: return "DEBUG";
    case CxLogLevel::Info: return "INFO";
    case CxLogLevel::Warning: return "WARNING";
    case CxLogLevel::Error: return "ERROR";
    case CxLogLevel::Fatal: return "FATAL";
    default: return "UNKNOWN";
    }
}

void CxUnifiedLog::WriteLine(const std::string& line)
{
    if (!initialized_ || !file_stream_.is_open())
        return;

    thread_local bool inside_write = false;
    if (inside_write)
    {
        RawFallbackWrite("recursive write attempt, dropping message");
        return;
    }

    inside_write = true;

    if (!WriteWithLock(line))
    {
        RawFallbackWrite("Write failed");
    }

    inside_write = false;
}

bool CxUnifiedLog::WriteWithLock(const std::string& line)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!file_stream_.is_open())
    {
        return false;
    }

    file_stream_ << line << '\n';
    const bool write_ok = file_stream_.good();

    if (!write_ok)
    {
        RawFallbackWrite("ofstream write failed");
    }

    file_stream_.flush();

    return write_ok;
}

CxScopedLogContext::CxScopedLogContext(const CxUnifiedLogContext& patch)
{
    previous_ = CxUnifiedLog::Instance().Context();
    CxUnifiedLog::Instance().SetContext(patch);
}

CxScopedLogContext::~CxScopedLogContext()
{
    CxUnifiedLog::Instance().SetContext(previous_);
}