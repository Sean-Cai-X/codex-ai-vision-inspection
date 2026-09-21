#ifndef CXIMAGE_CX_UNIFIED_LOG_STREAM_BUF_H
#define CXIMAGE_CX_UNIFIED_LOG_STREAM_BUF_H

#include <streambuf>
#include <string>
#include "CxUnifiedLog.h"

class CxUnifiedLogStreamBuf : public std::streambuf
{
public:
    CxUnifiedLogStreamBuf(
        std::streambuf* original,
        CxLogLevel level,
        const std::string& component);

protected:
    int overflow(int ch) override;
    int sync() override;

private:
    void flushBuffer();

private:
    std::streambuf* original_;
    CxLogLevel level_;
    std::string component_;
    std::string buffer_;
    static const size_t MAX_LINE_SIZE = 32 * 1024;
};

void InstallUnifiedStdStreamCapture();
void RestoreUnifiedStdStreamCapture();

#endif