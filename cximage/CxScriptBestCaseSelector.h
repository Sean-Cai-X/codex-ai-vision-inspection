#pragma once

#include "CxScriptSuiteRunner.h"
#include <filesystem>

class CxScriptBestCaseSelector
{
public:
    static void SelectAndExportBestExamples(
        const std::filesystem::path& outRoot,
        const std::vector<CxScriptSuiteCaseResult>& caseResults);
};