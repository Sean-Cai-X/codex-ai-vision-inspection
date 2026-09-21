#ifndef CXIMAGE_CXSCRIPT_HEADLESS_RUNNER_H
#define CXIMAGE_CXSCRIPT_HEADLESS_RUNNER_H

#include <string>
#include <filesystem>
#include <opencv2/opencv.hpp>
#include "CxScriptHeadlessRuntime.h"

bool SaveCxScriptHeadlessSummaryJson(
    const CxScriptExecutionCapture& capture,
    const std::filesystem::path& outputPath,
    std::string& outReason);

bool ParseCxScriptHeadlessArgs(
    int argc,
    char** argv,
    CxScriptHeadlessOptions& options);

bool RunCxScriptHeadlessCapture(
    const CxScriptHeadlessOptions& options,
    CxScriptExecutionCapture& capture,
    std::string& reason);

bool RunCxScriptHeadless(const CxScriptHeadlessOptions& options, CxScriptHeadlessResult& result);

int RunCxScriptHeadless(int argc, char* argv[]);

#endif
