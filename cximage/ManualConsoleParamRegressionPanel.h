#ifndef CXIMAGE_MANUAL_CONSOLE_PARAM_REGRESSION_PANEL_H
#define CXIMAGE_MANUAL_CONSOLE_PARAM_REGRESSION_PANEL_H

#include <string>
#include "ViewController.h"

void BuildParamRegressionTaskFromManualGauge(
    ManualTestContext& context,
    const std::string& objectName,
    const std::string& gaugeName);

void BuildManualSeedEvalRecord(
    ManualTestContext& context,
    const std::string& objectName,
    const std::string& gaugeName);

void BuildManualSeedAccuracyStats(ManualTestContext& context);

void InitializeParamRegressionFromGauge(
    ManualTestContext& context,
    const std::string& objectName,
    const std::string& gaugeName);

void CandidateFromManualGauge(
    ManualTestContext& context,
    const std::string& objectName,
    const std::string& gaugeName);

void AddMlpackRankPlaceholderCandidates(ManualTestContext& context);

void AddEnsmallenOptPlaceholderCandidates(ManualTestContext& context);

void RefreshParamRegressionExportedFiles(ManualTestContext& context);

void ExportParamRegressionManualAcceptanceChecklist(
    ManualTestContext& context,
    const std::string& objectName,
    const std::string& gaugeName);

bool IsFindLineFindCircleContext(ManualTestContext& context);

bool IsTorchContext(const ManualTestContext& context);

void DrawKeyParameterUnavailableNotice(const ManualTestContext& context);

void DrawCxScriptWorkbenchOverview(const ManualTestContext& context);

void DrawEvidenceCaseListPanel(ManualTestContext& context);

void DrawCxScriptTemplateSummaryPanel(const ManualTestContext& context);

void DrawKeyParameterSummaryPanel(const ManualTestContext& context);

void DrawConclusionSummaryPanel(const ManualTestContext& context);

void SyncKeyParameterUiToGauge(ManualTestContext& context);

void ResetKeyParameterUiDefaults(ManualTestContext& context);

void DrawKeyParameterControlPanel(ManualTestContext& context,
                                  const ParserDebugBridge* parserDebugBridge);

void DrawTorchAnnotationKeyParameterPanel(ManualTestContext& context);

void DrawParamTuningScatterPanel(ManualTestContext& context);

void DrawTorchKeyStatusPanel(ManualTestContext& context);

void DrawTorchEvidenceAndReviewPanel(const ManualTestContext& context);

void DrawGeometryAutoTuneParametersPanel(ManualTestContext& context);

void DrawGeometryAutoTuneDiagnosticsPanel(ManualTestContext& context);

void DrawGeometryAutoTuneEvidencePanel(ManualTestContext& context);

#endif
