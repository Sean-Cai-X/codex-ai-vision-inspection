#ifndef CXIMAGE_MANUAL_CONSOLE_CXSCRIPT_DEBUG_H
#define CXIMAGE_MANUAL_CONSOLE_CXSCRIPT_DEBUG_H

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <filesystem>
#include "ViewController.h"
#include "FindCircle.h"
#include "FindLine.h"

struct ParsedMethodCall
{
    bool valid = false;
    std::string object;
    std::string method;
    std::string params;
    std::vector<std::string> args;
};

struct DebugCximageRuntime
{
    std::unordered_map<std::string, std::unique_ptr<Image>> images;
    std::unordered_map<std::string, std::unique_ptr<FindCircle>> circles;
    std::unordered_map<std::string, std::unique_ptr<FindLine>> lines;
};

extern std::unordered_map<ManualTestContext*, DebugCximageRuntime> g_cximageRuntime;

DebugCximageRuntime& CxRuntime(ManualTestContext& context);

bool IsBraceOpenLine(const std::string& line);

bool IsBraceCloseLine(const std::string& line);

bool IsIfLine(const std::string& line);

std::string ExtractIfCondition(const std::string& line);

ParsedMethodCall ParseMethodCall(const std::string& statement);

void PrepareFindCircleDebugRuntime();

std::string GetGlobalMatInputPath(const ManualTestContext& context);

std::string StripAddressPrefix(std::string s);

void UpsertGlobalVariableViewCore(
    ManualTestContext& context,
    const std::string& type,
    const std::string& name,
    const std::string& value,
    int lineNo,
    const std::string& status,
    const std::string& imagePath,
    bool imageInitialized);

void UpsertGlobalVariableView(
    ManualTestContext& context,
    const std::string& type,
    const std::string& name,
    const std::string& value,
    int lineNo,
    const std::string& status);

void UpsertGlobalImageVariableView(
    ManualTestContext& context,
    const std::string& name,
    const std::string& imagePath,
    int lineNo);

void UpsertVariableView(
    ManualTestContext& context,
    const std::string& type,
    const std::string& name,
    const std::string& value,
    int lineNo,
    const std::string& status);

void ResetDebugRuntimeForReplay(ManualTestContext& context);

int FindNextNonEmptyLine(const ManualTestContext& context, int fromIndex);

int FindMatchingBraceLine(const ManualTestContext& context, int openBraceIndex);

int FindIfBodyStartLine(const ManualTestContext& context, int ifIndex);

int FindIfAfterBlockLine(const ManualTestContext& context, int ifIndex);

void MarkDebugRunFinishedIfAtEnd(ManualTestContext& context);

void MarkLineAsStructural(ManualTestContext& context,
    int lineIndex,
    const std::string& reason);

void MarkIfBlockBracesStructural(ManualTestContext& context,
    int ifLineIndex,
    bool markCloseBrace);

bool ReadRuntimeInt(ManualTestContext& context,
    const std::string& name,
    int& value);

std::string StripCxScriptQuotes(std::string value);

bool ReadRuntimeVariableValue(const ManualTestContext& context,
    const std::string& name,
    std::string& value);

bool ReadRuntimeNumber(ManualTestContext& context,
    const std::string& token,
    double& value);

bool ReadRuntimeString(ManualTestContext& context,
    const std::string& token,
    std::string& value);

bool EvalSimpleCondition(ManualTestContext& context,
    const std::string& condition,
    bool& value);

bool TryExecuteIntDeclarationAssignment(ManualTestContext& context,
    int lineIndex,
    const std::string& statement);

bool TryExecuteSimpleAssignment(ManualTestContext& context,
    int lineIndex,
    const std::string& statement);

bool TryExecuteCurrentStatusAssignment(ManualTestContext& context,
    int lineIndex,
    const std::string& statement);

bool TryExecuteDeclaration(ManualTestContext& context,
    int lineIndex,
    const std::string& statement);

void AddObservedGlobalVariables(ManualTestContext& context, const std::string& statement);

void AnalyzeScript(ManualTestContext& context);

void DebugScriptLineEnd(ManualTestContext& context, int lineIndex, const std::string& status);

void DebugStepOnce(ManualTestContext& context);

void CaptureDebugStepSnapshot(ManualTestContext& context, int lineIndex);

void DebugStepOnceWithSnapshot(ManualTestContext& context);

void SetTraceStatus(ManualTestContext& context,
                    const std::string& status,
                    const std::string& reason);

std::filesystem::path CxDebugLogDirectory();

std::filesystem::path CxDebugRuntimeLogPath();

std::filesystem::path CxDebugSnapshotPath();

void ResetCxDebugRuntimeLog(const ManualTestContext& context,
    const std::string& reason);

void AppendCxDebugEvent(const ManualTestContext& context,
    const std::string& event,
    int lineNo,
    const std::string& statement,
    const std::string& object,
    const std::string& method,
    const std::string& status,
    const std::string& reason,
    const std::string& summary);

void AppendCxDebugRuntimeObjectsSnapshot(const ManualTestContext& context,
    const std::string& event);

bool SaveCxDebugSnapshotText(ManualTestContext& context,
    const std::filesystem::path& path,
    std::string& outReason);

bool SaveCxDebugSnapshotText(ManualTestContext& context,
    std::string& outPath,
    std::string& outReason);

void ApplyCxParserExtDebugResultToManualConsole(
    ManualTestContext& context,
    const CxScriptSemanticBridgeResult& result);

#endif