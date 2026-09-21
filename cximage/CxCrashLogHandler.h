#ifndef CXIMAGE_CX_CRASH_LOG_HANDLER_H
#define CXIMAGE_CX_CRASH_LOG_HANDLER_H

#include <string>

void InstallCxCrashLogHandlers();

void SetCxCrashBreadcrumb(const std::string& breadcrumb);

void WriteCrashLog(
    const std::string& event,
    const std::string& message,
    const std::string& run_id,
    unsigned long thread_id);

#endif
