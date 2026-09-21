#pragma once

#include "CxBusinessWorkflowRuntime.h"

#include <array>
#include <cstddef>
#include <string>

// Read-only ImGui analysis for the external, asset-driven business workflow
// acceptance runtime. The product workflow lives in Vision-AI; this panel owns
// only test/evidence snapshots and has no product editing path of its own.
class CxBusinessWorkflowPanel
{
public:
    void Draw(bool* open);

private:
    static constexpr std::size_t kPathCapacity = 2048;
    static constexpr std::size_t kRunIdCapacity = 256;

    void RunExplicitSnapshot();
    void DrawConfiguration();
    void DrawBatchSummary() const;
    void DrawCaseSelector();
    void DrawSelectedCase();
    void DrawStateRail(const CxBusinessWorkflowStepResult* step) const;
    void DrawContext(const CxBusinessWorkflowCaseResult& case_result) const;
    void DrawLifecycle(
        const CxBusinessWorkflowCaseResult& case_result);
    void DrawGeometryAndDetections(
        const CxBusinessWorkflowCaseResult& case_result,
        const CxBusinessWorkflowStepResult* selected_step) const;
    void DrawRisksAndScan(
        const CxBusinessWorkflowCaseResult& case_result) const;

    const CxBusinessWorkflowCaseResult* SelectedCase() const;
    const CxBusinessWorkflowStepResult* SelectedStep(
        const CxBusinessWorkflowCaseResult& case_result) const;

    std::array<char, kPathCapacity> m_case_root{};
    std::array<char, kPathCapacity> m_out_dir{};
    std::array<char, kRunIdCapacity> m_run_id{};
    int m_max_cases = 0;

    CxBusinessWorkflowBatchResult m_result;
    bool m_has_snapshot = false;
    bool m_runtime_call_completed = false;
    std::string m_last_reason =
        "NOT_RUN: enter external cxscript_runs paths and click Run / Rescan.";
    std::size_t m_selected_case = 0;
    std::size_t m_selected_step = 0;
};
