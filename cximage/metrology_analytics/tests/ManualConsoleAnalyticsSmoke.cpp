#include "pch.h"
#include "metrology_analytics/tests/ManualConsoleAnalyticsSmoke.h"

#include "metrology_analytics/CxAnalyticsObservationBridgeDraft.h"
#include "gwy_reference/CxExternalGwyReferenceBackend.h"
#include "metrology_analytics/CxSurfaceBasicStats.h"
#include "metrology_analytics/CxSurfaceField.h"

#include <chrono>
#include <ctime>
#include <cstring>
#include <iomanip>
#include <sstream>

#include <imgui.h>

namespace cxvision::metrology_analytics
{
namespace
{
std::string MakeManualAnalyticsRunId()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tmv{};
#if defined(_WIN32)
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    std::ostringstream out;
    out << "run_" << std::put_time(&tmv, "%Y%m%d_%H%M%S")
        << "_manual_analytics_smoke";
    return out.str();
}
}

CxManualConsoleAnalyticsSmokePanelContract
ManualConsoleAnalyticsSmokePanelContract()
{
    return {};
}

void DrawManualConsoleAnalyticsSmokePanel(
    ManualConsoleAnalyticsSmokeUiState& state)
{
    if (!state.initialized)
    {
        state.initialized = true;
        state.last_bridge_status =
            CxAnalyticsObservationBridgeDraft::DraftStatus();
    }

    const auto contract = ManualConsoleAnalyticsSmokePanelContract();
    ImGui::TextWrapped(
        "S3-9 independent Manual Console analytics smoke. This window does "
        "not run CI, does not touch Parser, and does not claim Find* "
        "algorithm acceptance.");
    ImGui::Text("status: %s", state.status.c_str());
    ImGui::TextWrapped("reason: %s", state.reason.c_str());
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Analytics Smoke", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("panel: %s", contract.tab_label.c_str());
        ImGui::Text("minimum expected cases: %d", contract.minimum_expected_cases);
        ImGui::Text("trigger CI: %s", contract.triggers_ci ? "yes" : "no");
        ImGui::SetNextItemWidth(520.0f);
        static std::string outputRootBuffer;
        if (outputRootBuffer.empty())
            outputRootBuffer = state.output_root.string();
        char buf[1024] = {};
#if defined(_MSC_VER)
        strncpy_s(buf, outputRootBuffer.c_str(), sizeof(buf) - 1);
#else
        std::strncpy(buf, outputRootBuffer.c_str(), sizeof(buf) - 1);
#endif
        if (ImGui::InputText("output root", buf, sizeof(buf)))
        {
            outputRootBuffer = buf;
            state.output_root = outputRootBuffer;
        }

        if (ImGui::Button("Run Analytics Smoke"))
        {
            const std::filesystem::path outDir =
                state.output_root / MakeManualAnalyticsRunId();
            std::string runReason;
            CxMetrologyAnalyticsSmokeResult result;
            const bool ok = RunMetrologyAnalyticsSmoke(outDir, result, runReason);
            state.last_result = result;
            state.last_output_dir = outDir.string();
            state.last_summary_path = result.summary_path.string();
            state.last_report_path = result.report_path.string();
            if (ok && result.fail_count == 0)
            {
                state.status = "METROLOGY_ANALYTICS_SMOKE_PASS";
                state.reason = "Manual Console analytics smoke completed.";
            }
            else
            {
                state.status = "METROLOGY_ANALYTICS_SMOKE_FAIL";
                state.reason = runReason.empty()
                    ? "one or more analytics smoke cases failed"
                    : runReason;
            }
        }

        if (!state.last_output_dir.empty())
        {
            ImGui::TextWrapped("out: %s", state.last_output_dir.c_str());
            ImGui::TextWrapped("summary: %s", state.last_summary_path.c_str());
            ImGui::TextWrapped("report: %s", state.last_report_path.c_str());
            ImGui::Text("cases=%d pass=%d fail=%d",
                        state.last_result.total_cases,
                        state.last_result.pass_count,
                        state.last_result.fail_count);
        }

        if (!state.last_result.cases.empty() &&
            ImGui::BeginTable("analytics_smoke_cases", 4,
                              ImGuiTableFlags_Borders |
                                  ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_ScrollY,
                              ImVec2(0.0f, 260.0f)))
        {
            ImGui::TableSetupColumn("Case");
            ImGui::TableSetupColumn("Category");
            ImGui::TableSetupColumn("Pass");
            ImGui::TableSetupColumn("Reason");
            ImGui::TableHeadersRow();
            for (int i = 0; i < static_cast<int>(state.last_result.cases.size()); ++i)
            {
                const auto& c = state.last_result.cases[static_cast<std::size_t>(i)];
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                if (ImGui::Selectable(c.case_id.c_str(), state.selected_case == i,
                                      ImGuiSelectableFlags_SpanAllColumns))
                    state.selected_case = i;
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(c.category.c_str());
                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(c.pass ? "yes" : "no");
                ImGui::TableSetColumnIndex(3);
                ImGui::TextWrapped("%s", c.reason.c_str());
            }
            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader("S3 -> S4 Draft Bridge", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("status: %s", state.last_bridge_status.c_str());
        ImGui::TextWrapped(
            "Draft-only value bridge. S4 may later consume surface_stats, "
            "area_result and roughness_1d refs. Unknown IDs are pending "
            "binding, not algorithm failure.");
        const auto ids = CxAnalyticsObservationBridgeDraft::SupportedObservationIds();
        for (const auto& id : ids)
            ImGui::BulletText("%s", id.c_str());
    }

    if (ImGui::CollapsingHeader("GWY Reference Interface",
                                ImGuiTreeNodeFlags_DefaultOpen))
    {
        static const char* modes[] = {
            "Cx Native", "GWY Reference", "Dual Compare"
        };
        ImGui::Combo("execution mode", &state.gwy_execution_mode, modes, 3);
        ImGui::Text("status: %s", state.gwy_interface_status.c_str());
        ImGui::TextWrapped("reason: %s", state.gwy_interface_reason.c_str());
        ImGui::TextWrapped(
            "The published interface is dependency-free. Until an external "
            "runner is bound, closure artifacts are generated with "
            "PENDING_BINDING and promotion_allowed=false.");

        if (ImGui::Button("Run GWY Interface Closure"))
        {
            const std::filesystem::path outDir =
                state.output_root / MakeManualAnalyticsRunId() /
                "gwy_reference_interface";
            gwy_reference::CxGwyReferenceRequest request;
            request.request_id = outDir.parent_path().filename().string();
            request.case_id = "gwy_reference_manual_interface_null";
            request.algorithm_id = "surface.basic_stats";
            request.input_ref = "builtin:flat5";
            request.input_hash = "builtin-flat5-v1";
            request.mode = static_cast<gwy_reference::CxGwyExecutionMode>(
                std::max(0, std::min(2, state.gwy_execution_mode)));

            auto backend = gwy_reference::CreateGwyReferenceBackend();
            CxPhysUnit unit;
            CxSurfaceField nativeField(5, 5, unit);
            nativeField.fillFromGenerator([](int, int) { return 5.0; });
            const auto nativeStats = computeSurfaceBasicStats(nativeField);
            gwy_reference::CxGwyNormalizedResult nativeResult;
            nativeResult.implementation = "cxvision.metrology_analytics";
            nativeResult.implementation_version = "1";
            nativeResult.status = "CX_NATIVE_EXECUTION_COMPLETE";
            nativeResult.conclusion = "PENDING_HUMAN_REVIEW";
            nativeResult.reason =
                "cxvision native flat5 basic statistics executed";
            nativeResult.backend_available = true;
            nativeResult.executed = true;
            nativeResult.algorithm_success = true;
            nativeResult.metrics = {
                {"basic_stats.min", nativeStats.min},
                {"basic_stats.max", nativeStats.max},
                {"basic_stats.mean", nativeStats.mean},
                {"basic_stats.ra", nativeStats.ra},
                {"basic_stats.rms", nativeStats.rms},
                {"basic_stats.skewness", nativeStats.skewness},
                {"basic_stats.kurtosis", nativeStats.kurtosis_excess}
            };
            gwy_reference::CxGwyReferenceRunPackage package;
            std::string closureReason;
            const bool ok = gwy_reference::RunReferenceInterfaceClosure(
                request, *backend, &nativeResult, outDir, package, closureReason);
            state.gwy_interface_status = ok
                ? package.comparison.conclusion
                : "GWY_REFERENCE_INTERFACE_FAIL";
            state.gwy_interface_reason = closureReason;
            state.gwy_interface_output_dir = outDir.string();
            state.gwy_interface_report_path = package.report_path.string();
        }

        if (!state.gwy_interface_output_dir.empty())
        {
            ImGui::TextWrapped("out: %s", state.gwy_interface_output_dir.c_str());
            ImGui::TextWrapped("report: %s",
                               state.gwy_interface_report_path.c_str());
        }
    }
}

} // namespace cxvision::metrology_analytics
