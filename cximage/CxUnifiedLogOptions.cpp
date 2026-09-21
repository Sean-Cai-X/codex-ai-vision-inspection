#include "pch.h"
#include "CxUnifiedLogOptions.h"
#include <sstream>
#include <cstring>
#include <algorithm>

bool ParseUnifiedLogArgs(
    int argc,
    char** argv,
    CxUnifiedLogOptions& options,
    std::string& reason)
{
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];

        if (arg == "--unified-log")
        {
            if (i + 1 >= argc)
            {
                reason = "--unified-log requires a path argument";
                return false;
            }
            options.path = argv[++i];
            continue;
        }

        if (arg == "--unified-log-level")
        {
            if (i + 1 >= argc)
            {
                reason = "--unified-log-level requires a level argument";
                return false;
            }
            std::string level = argv[++i];
            std::transform(level.begin(), level.end(), level.begin(), ::tolower);
            if (level == "trace") options.min_level = CxLogLevel::Trace;
            else if (level == "debug") options.min_level = CxLogLevel::Debug;
            else if (level == "info") options.min_level = CxLogLevel::Info;
            else if (level == "warning") options.min_level = CxLogLevel::Warning;
            else if (level == "error") options.min_level = CxLogLevel::Error;
            else if (level == "fatal") options.min_level = CxLogLevel::Fatal;
            else
            {
                reason = "invalid log level: " + level;
                return false;
            }
            continue;
        }

        if (arg == "--no-unified-log")
        {
            options.enabled = false;
            continue;
        }

        if (arg == "--no-unified-log-stdio")
        {
            options.capture_stdio = false;
            continue;
        }

        if (arg == "--unified-log-smoke")
        {
            options.smoke_mode = true;
            continue;
        }

        if (arg == "--unified-log-smoke-id")
        {
            if (i + 1 >= argc)
            {
                reason = "--unified-log-smoke-id requires an id argument";
                return false;
            }
            options.smoke_id = argv[++i];
            continue;
        }

        if (arg == "--torch-runtime-smoke")
        {
            options.torch_runtime_smoke.enabled = true;
            continue;
        }

        if (arg == "--torch-runtime-dll" && i + 1 < argc)
        {
            options.torch_runtime_smoke.runtime_dll = argv[++i];
            continue;
        }

        if (arg == "--torch-device" && i + 1 < argc)
        {
            options.torch_runtime_smoke.device = argv[++i];
            continue;
        }

        if (arg == "--torch-model-root" && i + 1 < argc)
        {
            options.torch_runtime_smoke.model_root = argv[++i];
            continue;
        }

        if (arg == "--out" && i + 1 < argc)
        {
            options.torch_runtime_smoke.output_dir = argv[++i];
            continue;
        }
    }

    if (options.path.empty())
    {
        const char* env_path = std::getenv("CXVISION_UNIFIED_LOG");
        if (env_path && strlen(env_path) > 0)
        {
            options.path = env_path;
        }
        else
        {
            std::filesystem::path cxvision_root = std::filesystem::current_path();
            while (!cxvision_root.empty() && cxvision_root.filename() != "cxvisionai")
            {
                cxvision_root = cxvision_root.parent_path();
            }
            if (!cxvision_root.empty())
            {
                options.path = cxvision_root / "cxscript_runs" / "_shared" / "cxvision_imgui_acceptance.jsonl";
            }
            else
            {
                options.path = std::filesystem::current_path() / "cxvision_imgui_acceptance.jsonl";
            }
        }
    }

    return true;
}

std::string DetectCxVisionRunMode(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];

        if (arg == "--business-workflow-acceptance")
            return "business_workflow_acceptance";
        if (arg == "--headless")
            return "headless";
        if (arg == "--suite")
            return "suite";
        if (arg == "--frame-probe")
            return "frame_probe";
        if (arg == "--shape-interaction-smoke")
            return "shape_interaction";
        if (arg == "--metrology-analytics-smoke")
            return "metrology_analytics";
        if (arg == "--mask-diagnostic-selftest")
            return "mask_diagnostic_selftest";
        if (arg == "--cximage-reference-candidate")
            return "cximage_reference_candidate";
        if (arg == "--automatic-diagnostic-closure")
            return "automatic_diagnostic_closure";
        if (arg == "--selftest" && i + 1 < argc && argv[i + 1] != nullptr)
        {
            const std::string filter = argv[i + 1];
            if (filter == "analytics" || filter == "analytics.*" ||
                filter.rfind("analytics.", 0) == 0)
                return "analytics_selftest";
        }
        if (arg.rfind("--selftest=analytics", 0) == 0)
            return "analytics_selftest";
        if (arg == "--contract")
            return "contract";
        if (arg == "--tool-display")
            return "tool_display";
        if (arg == "--parameter-regression")
            return "parameter_regression";
        if (arg == "--unified-log-smoke")
            return "unified_log_smoke";
        if (arg == "--torch-runtime-smoke")
            return "torch_runtime_smoke";
    }

    return "gui";
}

std::string RedactCommandLine(int argc, char** argv)
{
    std::vector<std::string> redacted_args;
    redacted_args.reserve(argc);

    for (int i = 0; i < argc; ++i)
    {
        std::string arg = argv[i];

        if (arg == "--token" || arg == "--password" || arg == "--secret" ||
            arg == "--api-key" || arg == "--authorization")
        {
            redacted_args.push_back(arg);
            if (i + 1 < argc)
            {
                redacted_args.push_back("<redacted>");
                ++i;
            }
        }
        else
        {
            size_t pos = arg.find('=');
            if (pos != std::string::npos)
            {
                std::string key = arg.substr(0, pos);
                if (key == "--token" || key == "--password" || key == "--secret" ||
                    key == "--api-key" || key == "--authorization")
                {
                    redacted_args.push_back(key + "=<redacted>");
                    continue;
                }
            }
            redacted_args.push_back(arg);
        }
    }

    std::ostringstream oss;
    for (size_t i = 0; i < redacted_args.size(); ++i)
    {
        if (i > 0) oss << " ";
        if (redacted_args[i].find(' ') != std::string::npos)
            oss << "\"" << redacted_args[i] << "\"";
        else
            oss << redacted_args[i];
    }

    return oss.str();
}
