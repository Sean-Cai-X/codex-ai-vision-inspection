#include "pch.h"
#include "CxUnifiedLogStreamBuf.h"
#include "CxUnifiedLog.h"
#include <iostream>

static CxUnifiedLogStreamBuf* g_cout_buf = nullptr;
static CxUnifiedLogStreamBuf* g_cerr_buf = nullptr;
static std::streambuf* g_original_cout_buf = nullptr;
static std::streambuf* g_original_cerr_buf = nullptr;

CxUnifiedLogStreamBuf::CxUnifiedLogStreamBuf(
    std::streambuf* original,
    CxLogLevel level,
    const std::string& component)
    : original_(original), level_(level), component_(component)
{
}

int CxUnifiedLogStreamBuf::overflow(int ch)
{
    if (ch == '\n')
    {
        flushBuffer();
        return original_->sputc(ch);
    }

    if (buffer_.size() < MAX_LINE_SIZE)
    {
        buffer_ += static_cast<char>(ch);
    }

    return original_->sputc(ch);
}

int CxUnifiedLogStreamBuf::sync()
{
    flushBuffer();
    return original_->pubsync();
}

void CxUnifiedLogStreamBuf::flushBuffer()
{
    if (buffer_.empty())
        return;

    if (CxUnifiedLog::Instance().IsInitialized())
    {
        CxUnifiedLog::Instance().Write(
            level_,
            component_.c_str(),
            "legacy_stdio",
            "logged",
            buffer_,
            "",
            0,
            "");
    }

    buffer_.clear();
}

void InstallUnifiedStdStreamCapture()
{
    if (g_cout_buf != nullptr)
        return;

    g_original_cout_buf = std::cout.rdbuf();
    g_original_cerr_buf = std::cerr.rdbuf();

    g_cout_buf = new CxUnifiedLogStreamBuf(g_original_cout_buf, CxLogLevel::Info, "stdout");
    g_cerr_buf = new CxUnifiedLogStreamBuf(g_original_cerr_buf, CxLogLevel::Error, "stderr");

    std::cout.rdbuf(g_cout_buf);
    std::cerr.rdbuf(g_cerr_buf);
}

void RestoreUnifiedStdStreamCapture()
{
    if (g_cout_buf == nullptr)
        return;

    std::cout.rdbuf(g_original_cout_buf);
    std::cerr.rdbuf(g_original_cerr_buf);

    delete g_cout_buf;
    delete g_cerr_buf;

    g_cout_buf = nullptr;
    g_cerr_buf = nullptr;
    g_original_cout_buf = nullptr;
    g_original_cerr_buf = nullptr;
}