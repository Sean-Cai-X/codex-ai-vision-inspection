#pragma once

#include "CxScriptSuiteRunner.h"
#include <filesystem>

class CxScriptToolDisplayExporter
{
public:
    static std::string ExportToolDisplay(
        const std::string& original_path,
        const std::string& result_overlay_path,
        const std::string& evidence_overlay_path,
        const std::filesystem::path& output_path,
        const CxScriptSuiteCaseResult& result);
};