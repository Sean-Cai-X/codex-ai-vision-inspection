#ifndef CXIMAGE_MANUAL_CONSOLE_GAUGE_H
#define CXIMAGE_MANUAL_CONSOLE_GAUGE_H

#include <string>
#include "ViewController.h"

void InjectManualGaugeInt(ManualTestContext& context, const std::string& key, int value);

bool ValidateManualGaugeGeometry(
    const ManualGaugeState& gauge,
    std::string& reason);

bool ValidateManualGaugeGeometryForEditing(
    const ManualGaugeState& gauge,
    std::string& reason);

void NormalizeManualGaugeGeometry(ManualGaugeState& gauge);

bool ValidateParamRegressionPrerequisites(
    const ManualTestContext& context,
    std::string& reason);

bool ApplyManualGaugeToGlobals(ManualTestContext& context, const std::string& objectName);
bool ApplyManualGaugeToGlobals(ManualTestContext& context);

void RecordManualOperationTraceEvent(
    ManualTestContext& context,
    const std::string& operation,
    const std::string& status,
    const std::string& reason);

void ClearManualOperationTraceEvents(ManualTestContext& context);

std::filesystem::path ManualGaugeCaseDir(const ManualTestContext& context);

bool ResolveManualGaugeCaseDir(
    const ManualTestContext& context,
    std::filesystem::path& out,
    std::string& reason);

bool SaveManualGaugeAnnotation(
    ManualTestContext& context,
    const std::string& objectName,
    const std::string& gaugeName,
    std::string& outPath,
    std::string& outReason);

bool LoadManualGaugeAnnotation(
    ManualTestContext& context,
    const std::string& objectName,
    const std::string& gaugeName,
    std::string& outPath,
    std::string& outReason);

bool LoadManualGaugeAnnotationFromPath(
    ManualTestContext& context,
    const std::filesystem::path& sourcePath,
    std::string& outReason);

// Candidate/working revisions are intentionally editable and therefore are
// not required to carry the manual_accepted promotion state.
bool LoadManualGaugeWorkingCopyFromPath(
    ManualTestContext& context,
    const std::filesystem::path& sourcePath,
    std::string& outReason);

bool ExportManualGaugeManifestCandidate(
    ManualTestContext& context,
    const std::string& objectName,
    const std::string& gaugeName,
    std::string& outPath,
    std::string& outReason);

bool ManualGaugeAcceptedForParamRegression(const ManualGaugeState& gauge);

#endif