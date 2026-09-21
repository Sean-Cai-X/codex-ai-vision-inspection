#ifndef CXIMAGE_CX_UNIFIED_LOG_OPTIONS_H
#define CXIMAGE_CX_UNIFIED_LOG_OPTIONS_H

#include <string>
#include <filesystem>
#include "CxUnifiedLog.h"

struct CxTorchRuntimeSmokeOptions
{
    bool enabled = false;
    std::filesystem::path runtime_dll;
    std::filesystem::path output_dir;
    std::string device = "cpu";
    std::string model_root;
};

struct CxUnifiedLogOptions
{
    std::filesystem::path path;
    bool enabled = true;
    bool capture_stdio = true;
    CxLogLevel min_level = CxLogLevel::Info;
    std::string smoke_id;
    bool smoke_mode = false;

    CxTorchRuntimeSmokeOptions torch_runtime_smoke;
};

bool ParseUnifiedLogArgs(
    int argc,
    char** argv,
    CxUnifiedLogOptions& options,
    std::string& reason);

std::string DetectCxVisionRunMode(int argc, char** argv);

std::string RedactCommandLine(int argc, char** argv);

#endif