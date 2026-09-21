#ifndef CXIMAGE_CX_UNIFIED_LOG_H
#define CXIMAGE_CX_UNIFIED_LOG_H

#include <string>
#include <filesystem>
#include <mutex>
#include <atomic>
#include <chrono>
#include <fstream>

enum class CxLogLevel
{
    Trace,
    Debug,
    Info,
    Warning,
    Error,
    Fatal
};

struct CxUnifiedLogContext
{
    std::string run_id;
    std::string mode;
    std::string session_id;
    std::string suite_id;
    std::string case_id;
    std::string evidence_id;
    std::string image_id;
    std::string target_id;
    std::string script_id;
    std::string parameter_profile_id;
    std::string tool;
    std::string object_ref;
    std::string shape_ref;
    std::string phase;
};

struct CxUnifiedLogEvent
{
    std::string timestamp_iso;
    std::string run_id;
    unsigned long process_id = 0;
    unsigned long thread_id = 0;
    unsigned long long sequence = 0;
    CxLogLevel level = CxLogLevel::Info;
    std::string component;
    std::string event;
    std::string phase;
    std::string status;
    std::string message;
    CxUnifiedLogContext context;
    std::string source_file;
    std::string source_function;
    int source_line = 0;
    long long elapsed_ms = 0;
};

class CxUnifiedLog
{
public:
    static CxUnifiedLog& Instance();

    bool Initialize(
        const std::filesystem::path& path,
        const std::string& runMode,
        int argc,
        char** argv,
        std::string& reason);

    void Shutdown(int exitCode, const std::string& conclusion);

    bool IsInitialized() const;
    const std::string& RunId() const;
    std::string GenerateRunId() const;
    const std::filesystem::path& Path() const;
    const std::string& Mode() const;

    void SetContext(const CxUnifiedLogContext& context);
    CxUnifiedLogContext Context() const;

    void Write(
        CxLogLevel level,
        const char* component,
        const char* event,
        const std::string& status,
        const std::string& message,
        const char* file,
        int line,
        const char* function);

    void Flush();

private:
    CxUnifiedLog();
    ~CxUnifiedLog();

    CxUnifiedLog(const CxUnifiedLog&) = delete;
    CxUnifiedLog& operator=(const CxUnifiedLog&) = delete;

    std::string FormatTimestamp() const;
    std::string SerializeEvent(const CxUnifiedLogEvent& event) const;
    std::string EscapeJsonString(const std::string& s) const;
    std::string LevelToString(CxLogLevel level) const;
    void WriteLine(const std::string& line);
    bool WriteWithLock(const std::string& line);
    void RawFallbackWrite(const std::string& message);

private:
    bool initialized_ = false;
    std::filesystem::path log_path_;
    std::string run_id_;
    std::string mode_;
    std::chrono::steady_clock::time_point start_time_;

    std::ofstream file_stream_;
    std::mutex mutex_;
    std::atomic<unsigned long long> sequence_{0};

    thread_local static CxUnifiedLogContext thread_context_;
};

class CxScopedLogContext
{
public:
    explicit CxScopedLogContext(const CxUnifiedLogContext& patch);
    ~CxScopedLogContext();

private:
    CxUnifiedLogContext previous_;
};

#define CXLOG_INFO(component, event, status, message) \
    CxUnifiedLog::Instance().Write(                  \
        CxLogLevel::Info, component, event, status, message, \
        __FILE__, __LINE__, __FUNCTION__)

#define CXLOG_WARN(component, event, status, message) \
    CxUnifiedLog::Instance().Write(                   \
        CxLogLevel::Warning, component, event, status, message, \
        __FILE__, __LINE__, __FUNCTION__)

#define CXLOG_ERROR(component, event, status, message) \
    CxUnifiedLog::Instance().Write(                    \
        CxLogLevel::Error, component, event, status, message, \
        __FILE__, __LINE__, __FUNCTION__)

#define CXLOG_DEBUG(component, event, status, message) \
    CxUnifiedLog::Instance().Write(                    \
        CxLogLevel::Debug, component, event, status, message, \
        __FILE__, __LINE__, __FUNCTION__)

#define CXLOG_TRACE(component, event, status, message) \
    CxUnifiedLog::Instance().Write(                    \
        CxLogLevel::Trace, component, event, status, message, \
        __FILE__, __LINE__, __FUNCTION__)

#define CXLOG_FATAL(component, event, status, message) \
    CxUnifiedLog::Instance().Write(                    \
        CxLogLevel::Fatal, component, event, status, message, \
        __FILE__, __LINE__, __FUNCTION__)

#endif
