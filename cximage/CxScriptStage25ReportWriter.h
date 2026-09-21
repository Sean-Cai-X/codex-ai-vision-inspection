#ifndef CXIMAGE_CXSCRIPT_STAGE25_REPORT_WRITER_H
#define CXIMAGE_CXSCRIPT_STAGE25_REPORT_WRITER_H

#include <filesystem>
#include <vector>
#include "CxScriptStage25Runner.h"
#include "CxScriptStage25Manifest.h"
#include "CxScriptStage25PolicyValidator.h"
#include "CxScriptStage25CaseMatrix.h"
#include "CxScriptImagePreflight.h"

class Stage25ReportWriter
{
public:
    static void WriteBatchReport(
        const std::filesystem::path& out_root,
        const std::vector<Stage25CaseResult>& results);

    static void WritePreflightReport(
        const std::filesystem::path& out_root,
        const std::vector<Stage25ImagePreflightResult>& results);

    static void WriteCoverageReport(
        const std::filesystem::path& out_root,
        const Stage25Manifest& manifest);

    static void WriteStabilityReport(
        const std::filesystem::path& out_root,
        const std::vector<Stage25CaseResult>& results,
        const Stage25Manifest& manifest);

    static void WritePolicyReport(
        const std::filesystem::path& out_root,
        const std::vector<Stage25CaseResult>& results,
        const Stage25Manifest& manifest);

    static void WritePolicyValidationReport(
        const std::filesystem::path& out_root,
        const Stage25PolicyValidationResult& validation);

    static void WriteCaseMatrixReport(
        const std::filesystem::path& out_root,
        const std::vector<Stage25CaseMatrixEntry>& matrix);

    static void WriteFastMatchReadinessReport(
        const std::filesystem::path& out_root,
        const std::vector<Stage25CaseResult>& results);

    static void WriteCaseFileIndex(
        const std::filesystem::path& out_root,
        const std::vector<Stage25CaseResult>& results);

    static void WriteDiagnosticReport(
        const std::filesystem::path& out_root,
        const std::vector<Stage25CaseResult>& results);
};

#endif