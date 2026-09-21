#include "CxBusinessWorkflowPanel.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <utility>

namespace
{
namespace fs = std::filesystem;

std::string LowerAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) {
                       return static_cast<char>(std::tolower(c));
                   });
    return value;
}

std::string UpperAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) {
                       return static_cast<char>(std::toupper(c));
                   });
    return value;
}

bool HasPathComponent(const fs::path& path, const std::string& component)
{
    const std::string expected = LowerAscii(component);
    for (const fs::path& part : path)
    {
        if (LowerAscii(part.string()) == expected)
            return true;
    }
    return false;
}

bool IsPathWithin(const fs::path& child, const fs::path& parent)
{
    auto child_it = child.begin();
    auto parent_it = parent.begin();
    for (; parent_it != parent.end(); ++parent_it, ++child_it)
    {
        if (child_it == child.end() ||
            LowerAscii(child_it->string()) != LowerAscii(parent_it->string()))
        {
            return false;
        }
    }
    return true;
}

bool ValidateExternalPath(const char* raw,
                          bool must_exist,
                          const char* label,
                          fs::path& normalized,
                          std::string& reason)
{
    reason.clear();
    if (raw == nullptr || raw[0] == '\0')
    {
        reason = std::string(label) + " is required";
        return false;
    }

    std::error_code ec;
    fs::path candidate(raw);
    if (!candidate.is_absolute())
    {
        reason = std::string(label) + " must be an absolute path";
        return false;
    }

    normalized = fs::absolute(candidate, ec).lexically_normal();
    if (ec)
    {
        reason = std::string(label) + " cannot be normalized: " + ec.message();
        return false;
    }
    if (!HasPathComponent(normalized, "cxscript_runs") ||
        HasPathComponent(normalized, "cxvision_repo"))
    {
        reason = std::string(label) +
                 " must be external to cxvision_repo and under cxscript_runs";
        return false;
    }

    if (must_exist)
    {
        if (!fs::exists(normalized, ec) || ec)
        {
            reason = std::string(label) + " does not exist";
            return false;
        }
        if (!fs::is_directory(normalized, ec) || ec)
        {
            reason = std::string(label) + " is not a directory";
            return false;
        }
    }
    return true;
}

bool ValidateRunId(const char* raw, std::string& reason)
{
    reason.clear();
    if (raw == nullptr || raw[0] == '\0')
    {
        reason = "run_id is required";
        return false;
    }

    const std::string value(raw);
    if (value.size() > 200)
    {
        reason = "run_id is too long";
        return false;
    }
    for (unsigned char c : value)
    {
        if (!std::isalnum(c) && c != '-' && c != '_' && c != '.')
        {
            reason = "run_id may contain only letters, digits, '.', '-' and '_'";
            return false;
        }
    }
    if (value == "." || value == "..")
    {
        reason = "run_id cannot be a relative path token";
        return false;
    }
    return true;
}

const char* TextOr(const std::string& value, const char* fallback)
{
    return value.empty() ? fallback : value.c_str();
}

bool ContainsStatusToken(const std::string& value, const char* token)
{
    return UpperAscii(value).find(token) != std::string::npos;
}

bool IsPendingStatus(const std::string& value)
{
    return value.empty() || ContainsStatusToken(value, "PENDING") ||
           ContainsStatusToken(value, "NOT_AVAILABLE") ||
           ContainsStatusToken(value, "UNAVAILABLE") ||
           ContainsStatusToken(value, "NOT_EXECUTED") ||
           ContainsStatusToken(value, "NOT_EVALUATED") ||
           ContainsStatusToken(value, "NOT_DISPLAYED") ||
           ContainsStatusToken(value, "MODEL_MISSING") ||
           ContainsStatusToken(value, "NOT_RUN") ||
           ContainsStatusToken(value, "UNBOUND");
}

ImVec4 StatusColor(const std::string& status)
{
    const std::string upper = UpperAscii(status);
    if (upper == "CONTRACT_FIXTURE_PASS")
        return ImVec4(0.30f, 0.72f, 0.86f, 1.0f);
    if (upper.find("FAIL") != std::string::npos ||
        upper.find("ERROR") != std::string::npos ||
        upper.find("INVALID") != std::string::npos ||
        upper.find("REJECTED") != std::string::npos ||
        upper.find("ASSET_MISSING") != std::string::npos)
    {
        return ImVec4(0.90f, 0.28f, 0.27f, 1.0f);
    }
    if (IsPendingStatus(status))
        return ImVec4(0.96f, 0.65f, 0.20f, 1.0f);
    if (upper.find("PASS") != std::string::npos ||
        upper.find("AVAILABLE") != std::string::npos ||
        upper.find("COMPLETED") != std::string::npos ||
        upper.find("HEALTHY") != std::string::npos ||
        upper.find("DISPLAYED") != std::string::npos ||
        upper.find("ACCEPTED") != std::string::npos)
    {
        return ImVec4(0.25f, 0.78f, 0.54f, 1.0f);
    }
    return ImVec4(0.30f, 0.72f, 0.86f, 1.0f);
}

const char* StatusMarker(const std::string& status)
{
    const std::string upper = UpperAscii(status);
    if (upper == "CONTRACT_FIXTURE_PASS")
        return "[CONTRACT]";
    if (upper.find("FAIL") != std::string::npos ||
        upper.find("ERROR") != std::string::npos ||
        upper.find("INVALID") != std::string::npos ||
        upper.find("REJECTED") != std::string::npos ||
        upper.find("ASSET_MISSING") != std::string::npos)
        return "[FAIL]";
    if (IsPendingStatus(status))
        return "[WAIT]";
    if (upper.find("PASS") != std::string::npos ||
        upper.find("AVAILABLE") != std::string::npos ||
        upper.find("COMPLETED") != std::string::npos ||
        upper.find("HEALTHY") != std::string::npos ||
        upper.find("DISPLAYED") != std::string::npos ||
        upper.find("ACCEPTED") != std::string::npos)
        return "[OK]";
    return "[INFO]";
}

void DrawStatusText(const std::string& status)
{
    const std::string value = status.empty() ? "NOT_REPORTED" : status;
    ImGui::TextColored(StatusColor(value), "%s %s", StatusMarker(value),
                       value.c_str());
}

void DrawPathRow(const char* label, const fs::path& path, const char* id)
{
    ImGui::PushID(id);
    ImGui::TextDisabled("%s", label);
    ImGui::SameLine();
    if (path.empty())
    {
        ImGui::TextDisabled("NOT_AVAILABLE");
    }
    else
    {
        const std::string text = path.generic_string();
        ImGui::TextWrapped("%s", text.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Copy"))
            ImGui::SetClipboardText(text.c_str());
    }
    ImGui::PopID();
}

void DrawRequiredAssets(const std::vector<fs::path>& assets)
{
    ImGui::SeparatorText("Required assets");
    ImGui::TextDisabled(
        "Existence reflects runtime validation at the explicit New Evidence Run "
        "snapshot; the draw loop does not re-scan disk.");
    if (assets.empty())
    {
        ImGui::TextDisabled("NOT_AVAILABLE: no required assets were reported.");
        return;
    }

    if (!ImGui::BeginTable("BusinessRequiredAssets", 3,
                           ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                               ImGuiTableFlags_Resizable |
                               ImGuiTableFlags_SizingStretchProp))
    {
        return;
    }

    ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 42.0f);
    ImGui::TableSetupColumn("Exists at snapshot",
                            ImGuiTableColumnFlags_WidthFixed, 155.0f);
    ImGui::TableSetupColumn("Validated path",
                            ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();
    for (std::size_t index = 0; index < assets.size(); ++index)
    {
        const std::string path = assets[index].generic_string();
        ImGui::PushID(static_cast<int>(index));
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("%zu", index + 1);
        ImGui::TableSetColumnIndex(1);
        ImGui::TextColored(ImVec4(0.25f, 0.78f, 0.54f, 1.0f),
                           "[OK] YES");
        ImGui::TextDisabled("runtime validated");
        ImGui::TableSetColumnIndex(2);
        ImGui::TextWrapped("%s", path.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Copy"))
            ImGui::SetClipboardText(path.c_str());
        ImGui::PopID();
    }
    ImGui::EndTable();
}

void DrawAssetPreflight(
    const std::vector<CxBusinessWorkflowAssetPreflightRecord>& records)
{
    ImGui::SeparatorText("Asset content preflight");
    ImGui::TextDisabled(
        "Observed at run time: JSON parse, CxScript compile, or image decode/hash."
        " These observations are immutable evidence, not live disk state.");
    if (records.empty())
    {
        ImGui::TextDisabled("NOT_AVAILABLE: no content-preflight records reported.");
        return;
    }

    if (!ImGui::BeginTable("BusinessAssetPreflight", 6,
                           ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                               ImGuiTableFlags_Resizable |
                               ImGuiTableFlags_SizingStretchProp))
    {
        return;
    }

    ImGui::TableSetupColumn("Kind / status");
    ImGui::TableSetupColumn("Observed dimensions");
    ImGui::TableSetupColumn("Bytes");
    ImGui::TableSetupColumn("Content hash");
    ImGui::TableSetupColumn("Code / reason");
    ImGui::TableSetupColumn("Validated path");
    ImGui::TableHeadersRow();
    for (std::size_t index = 0; index < records.size(); ++index)
    {
        const CxBusinessWorkflowAssetPreflightRecord& record = records[index];
        ImGui::PushID(static_cast<int>(index));
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextWrapped("%s", TextOr(record.kind, "UNKNOWN"));
        DrawStatusText(record.status);
        ImGui::TableSetColumnIndex(1);
        if (record.width > 0 && record.height > 0)
        {
            ImGui::Text("%d x %d x %d", record.width, record.height,
                        record.channels);
        }
        else
        {
            ImGui::TextDisabled("N/A");
        }
        ImGui::TableSetColumnIndex(2);
        ImGui::Text("%llu",
                    static_cast<unsigned long long>(record.byte_size));
        ImGui::TableSetColumnIndex(3);
        ImGui::TextWrapped("%s", TextOr(record.content_hash, "N/A"));
        ImGui::TableSetColumnIndex(4);
        ImGui::TextWrapped("%s\n%s", TextOr(record.code, "NO_CODE"),
                           TextOr(record.reason, "NO_REASON"));
        ImGui::TableSetColumnIndex(5);
        ImGui::TextWrapped("%s", record.path.generic_string().c_str());
        ImGui::PopID();
    }
    ImGui::EndTable();
}

void DrawScopedReferenceRow(const char* label,
                            const std::string& contractValue,
                            const std::string& providerValue)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextDisabled("%s", label);
    ImGui::TableSetColumnIndex(1);
    if (contractValue.empty())
        ImGui::TextDisabled("NOT_AVAILABLE");
    else
        ImGui::TextWrapped("%s", contractValue.c_str());
    ImGui::TableSetColumnIndex(2);
    if (providerValue.empty())
        ImGui::TextDisabled("NOT_AVAILABLE");
    else
        ImGui::TextWrapped("%s", providerValue.c_str());
}

std::string LatestLifecycleRef(const CxBusinessWorkflowCaseResult& case_result,
                               const char* action_token,
                               bool contractOnly)
{
    const std::string token = LowerAscii(action_token);
    for (auto it = case_result.steps.rbegin(); it != case_result.steps.rend();
         ++it)
    {
        if (LowerAscii(it->action).find(token) == std::string::npos)
            continue;
        if (contractOnly ? !it->contract_only : !it->provider_executed)
            continue;
        if (!it->output_ref.empty())
            return it->output_ref;
        if (!it->input_ref.empty())
            return it->input_ref;
    }
    return std::string();
}

std::string CaseDisplayName(const CxBusinessWorkflowCaseResult& case_result)
{
    if (!case_result.display_name.empty())
        return case_result.display_name;
    if (!case_result.case_id.empty())
        return case_result.case_id;
    if (!case_result.case_directory.empty())
        return case_result.case_directory.filename().string();
    return "Unnamed case";
}

std::size_t PreferredStepIndex(
    const CxBusinessWorkflowCaseResult& caseResult)
{
    if (caseResult.steps.empty())
        return 0;
    for (std::size_t index = 0; index < caseResult.steps.size(); ++index)
    {
        const CxBusinessWorkflowStepResult& step = caseResult.steps[index];
        if (step.verification_scope == "BUSINESS_EXECUTION" &&
            (IsPendingStatus(step.status) ||
             IsPendingStatus(step.capability_status) ||
             (!step.provider_executed && step.requires_provider)))
        {
            return index;
        }
    }
    for (std::size_t index = 0; index < caseResult.steps.size(); ++index)
    {
        const CxBusinessWorkflowStepResult& step = caseResult.steps[index];
        if (step.provider_executed ||
            !step.observation.detection_elements.empty())
        {
            return index;
        }
    }
    for (std::size_t index = 0; index < caseResult.steps.size(); ++index)
    {
        if (!caseResult.steps[index].contract_fixture_elements.empty())
            return index;
    }
    return caseResult.steps.size() - 1;
}

void DrawEmptyCollection(const char* noun)
{
    ImGui::TextDisabled("NOT_AVAILABLE: no %s reported by this case.", noun);
}
} // namespace

void CxBusinessWorkflowPanel::Draw(bool* open)
{
    if (open != nullptr && !*open)
        return;

    ImGui::SetNextWindowSize(ImVec2(1160.0f, 800.0f),
                             ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Business Workflow Acceptance Analysis", open,
                      ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        return;
    }

    ImGui::TextColored(ImVec4(0.30f, 0.78f, 0.92f, 1.0f),
                       "ACCEPTANCE ANALYSIS / EXTERNAL CASE EVIDENCE");
    ImGui::TextWrapped(
        "Read-only analysis of an external case run. Contract fixtures remain "
        "distinct from provider execution; product operations live in Vision-AI.");

    DrawConfiguration();
    ImGui::Separator();
    DrawBatchSummary();

    if (!m_has_snapshot)
    {
        ImGui::Spacing();
        ImGui::TextDisabled(
            "No in-memory snapshot. The draw loop does not scan cases or write "
            "evidence; use New Evidence Run explicitly.");
        ImGui::End();
        return;
    }

    ImGui::Spacing();
    const float selector_width = 285.0f;
    ImGui::BeginChild("BusinessCaseSelector", ImVec2(selector_width, 0.0f),
                      true);
    DrawCaseSelector();
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("BusinessCaseDetail", ImVec2(0.0f, 0.0f), false);
    DrawSelectedCase();
    ImGui::EndChild();

    ImGui::End();
}

void CxBusinessWorkflowPanel::RunExplicitSnapshot()
{
    fs::path case_root;
    fs::path out_dir;
    std::string validation_reason;
    if (!ValidateExternalPath(m_case_root.data(), true, "case root", case_root,
                              validation_reason) ||
        !ValidateExternalPath(m_out_dir.data(), false, "output directory",
                              out_dir, validation_reason) ||
        !ValidateRunId(m_run_id.data(), validation_reason))
    {
        m_runtime_call_completed = false;
        m_last_reason = "INPUT_REJECTED: " + validation_reason;
        return;
    }

    if (IsPathWithin(out_dir, case_root) ||
        IsPathWithin(case_root, out_dir))
    {
        m_runtime_call_completed = false;
        m_last_reason =
            "INPUT_REJECTED: case root and output directory cannot overlap";
        return;
    }

    CxBusinessWorkflowBatchRequest request;
    request.run_id = m_run_id.data();
    request.case_root = std::move(case_root);
    request.out_dir = std::move(out_dir);
    request.max_cases = static_cast<std::size_t>(std::max(0, m_max_cases));

    // No provider callback is invented here.  Provider-required steps are
    // deliberately surfaced as PENDING / NOT_AVAILABLE by the shared runtime.
    CxBusinessWorkflowBatchResult next_result;
    std::string runtime_reason;
    const bool completed =
        RunCxBusinessWorkflowAcceptance(request, next_result, runtime_reason);

    if (!completed && next_result.out_dir.empty())
    {
        m_runtime_call_completed = false;
        m_last_reason = runtime_reason.empty()
            ? "Runtime call did not start; prior in-memory snapshot was preserved."
            : runtime_reason + " Prior in-memory snapshot was preserved.";
        return;
    }

    m_result = std::move(next_result);
    m_has_snapshot = true;
    m_runtime_call_completed = completed;
    m_last_reason = runtime_reason.empty()
                        ? (completed ? "Runtime call completed."
                                     : "Runtime call did not complete.")
                        : runtime_reason;
    m_selected_case = 0;
    m_selected_step = m_result.case_results.empty()
        ? 0
        : PreferredStepIndex(m_result.case_results.front());
}

void CxBusinessWorkflowPanel::DrawConfiguration()
{
    if (!ImGui::CollapsingHeader("Run configuration",
                                 ImGuiTreeNodeFlags_DefaultOpen))
        return;

    ImGui::TextColored(ImVec4(0.96f, 0.65f, 0.20f, 1.0f),
                       "[WAIT] Provider binding: NOT_AVAILABLE in this UI seam");
    ImGui::TextDisabled(
        "Provider-required steps remain PENDING; no model or human result is "
        "simulated.");

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputText("Case root##BusinessWorkflow", m_case_root.data(),
                     m_case_root.size());
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputText("Output directory##BusinessWorkflow", m_out_dir.data(),
                     m_out_dir.size());
    ImGui::SetNextItemWidth(360.0f);
    ImGui::InputText("Run ID##BusinessWorkflow", m_run_id.data(),
                     m_run_id.size());
    ImGui::SameLine();
    ImGui::SetNextItemWidth(140.0f);
    if (ImGui::InputInt("Max cases##BusinessWorkflow", &m_max_cases))
        m_max_cases = std::max(0, m_max_cases);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("0 means all accepted cases");

    if (ImGui::Button("New Evidence Run", ImVec2(190.0f, 34.0f)))
        RunExplicitSnapshot();
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip(
            "Explicitly invokes RunCxBusinessWorkflowAcceptance and writes to "
            "a new external output directory. Existing evidence is "
            "never overwritten.");
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear in-memory snapshot", ImVec2(190.0f, 34.0f)))
    {
        m_result = CxBusinessWorkflowBatchResult();
        m_has_snapshot = false;
        m_runtime_call_completed = false;
        m_last_reason = "NOT_RUN: in-memory snapshot cleared; disk unchanged.";
        m_selected_case = 0;
        m_selected_step = 0;
    }
    ImGui::SameLine();
    ImGui::TextDisabled(
        "Paths must be absolute and under external cxscript_runs; output must be "
        "new and must not already exist.");

    const ImVec4 call_color = m_runtime_call_completed
                                  ? ImVec4(0.25f, 0.78f, 0.54f, 1.0f)
                                  : ImVec4(0.96f, 0.65f, 0.20f, 1.0f);
    ImGui::TextColored(call_color, "Call: %s",
                       m_runtime_call_completed ? "COMPLETED" : "NOT_COMPLETED");
    ImGui::SameLine();
    ImGui::TextWrapped("%s", m_last_reason.c_str());
}

void CxBusinessWorkflowPanel::DrawBatchSummary() const
{
    if (!m_has_snapshot)
        return;

    if (ImGui::BeginTable("BusinessBatchSummary", 10,
                          ImGuiTableFlags_Borders |
                              ImGuiTableFlags_SizingStretchSame))
    {
        const char* labels[] = {"Discovered", "Accepted", "Rejected", "Skipped",
                                "Processed", "Business pass", "Contract pass",
                                "Provider pass", "Pending", "Fail"};
        const std::size_t values[] = {
            m_result.discovered_count, m_result.accepted_count,
            m_result.rejected_count,   m_result.skipped_count,
            m_result.processed_count,  m_result.pass_count,
            m_result.contract_fixture_pass_count,
            m_result.provider_execution_step_pass_count,
            m_result.pending_count,    m_result.fail_count};
        ImGui::TableNextRow();
        for (int column = 0; column < 10; ++column)
        {
            ImGui::TableSetColumnIndex(column);
            ImGui::TextDisabled("%s", labels[column]);
            ImGui::Text("%zu", values[column]);
        }
        ImGui::EndTable();
    }

    ImGui::TextUnformatted("Batch status:");
    ImGui::SameLine();
    DrawStatusText(m_result.final_status);
    if (!m_result.final_code.empty())
    {
        ImGui::SameLine();
        ImGui::TextDisabled("code=%s", m_result.final_code.c_str());
    }
    if (!m_result.final_reason.empty())
        ImGui::TextWrapped("%s", m_result.final_reason.c_str());
}

void CxBusinessWorkflowPanel::DrawCaseSelector()
{
    ImGui::TextUnformatted("CASE RAIL");
    ImGui::TextDisabled("%zu accepted result snapshots",
                        m_result.case_results.size());
    ImGui::Separator();

    for (std::size_t index = 0; index < m_result.case_results.size(); ++index)
    {
        const CxBusinessWorkflowCaseResult& item = m_result.case_results[index];
        const std::string visible = CaseDisplayName(item);
        ImGui::PushID(static_cast<int>(index));
        if (ImGui::Selectable(visible.c_str(), m_selected_case == index,
                              ImGuiSelectableFlags_AllowDoubleClick))
        {
            m_selected_case = index;
            m_selected_step = PreferredStepIndex(item);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::Text("case_id: %s", TextOr(item.case_id, "NOT_AVAILABLE"));
            ImGui::Text("internal_case_id: %s",
                        TextOr(item.internal_case_id, "NOT_AVAILABLE"));
            ImGui::TextWrapped("%s", item.case_directory.generic_string().c_str());
            ImGui::EndTooltip();
        }
        ImGui::SameLine();
        DrawStatusText(item.final_status);
        ImGui::PopID();
    }

    if (m_result.case_results.empty())
        DrawEmptyCollection("accepted cases");

    std::size_t rejected = 0;
    for (const CxBusinessWorkflowScanRecord& record : m_result.scan_records)
    {
        if (UpperAscii(record.outcome) == "REJECTED")
            ++rejected;
    }
    ImGui::Separator();
    ImGui::TextDisabled("Rejected manifests: %zu", rejected);
    ImGui::TextDisabled("See Risks & Scan for evidence-backed reasons.");
}

const CxBusinessWorkflowCaseResult* CxBusinessWorkflowPanel::SelectedCase() const
{
    if (m_result.case_results.empty())
        return nullptr;
    const std::size_t index =
        std::min(m_selected_case, m_result.case_results.size() - 1);
    return &m_result.case_results[index];
}

const CxBusinessWorkflowStepResult* CxBusinessWorkflowPanel::SelectedStep(
    const CxBusinessWorkflowCaseResult& case_result) const
{
    if (case_result.steps.empty())
        return nullptr;
    const std::size_t index =
        std::min(m_selected_step, case_result.steps.size() - 1);
    return &case_result.steps[index];
}

void CxBusinessWorkflowPanel::DrawSelectedCase()
{
    const CxBusinessWorkflowCaseResult* case_result = SelectedCase();
    if (case_result == nullptr)
    {
        ImGui::TextDisabled(
            "No accepted case result. Review scan findings and rejected paths.");
        ImGui::SeparatorText("Batch evidence reports");
        DrawPathRow("Summary", m_result.summary_path, "empty-summary");
        DrawPathRow("Markdown", m_result.report_md_path, "empty-report-md");
        DrawPathRow("HTML", m_result.report_html_path, "empty-report-html");
        DrawPathRow("Audit JSONL", m_result.audit_jsonl_path,
                    "empty-audit-jsonl");
        DrawPathRow("Scan debug", m_result.scan_debug_path,
                    "empty-scan-debug");
        DrawPathRow("Risk register", m_result.risk_register_path,
                    "empty-risk-register");
        if (ImGui::CollapsingHeader("Scan findings",
                                    ImGuiTreeNodeFlags_DefaultOpen))
        {
            for (const CxBusinessWorkflowScanRecord& record :
                 m_result.scan_records)
            {
                DrawStatusText(record.outcome);
                ImGui::SameLine();
                ImGui::TextWrapped("%s | %s | %s",
                                   record.path.generic_string().c_str(),
                                   TextOr(record.code, "NO_CODE"),
                                   TextOr(record.reason, "NO_REASON"));
            }
        }
        return;
    }

    const CxBusinessWorkflowStepResult* selected_step =
        SelectedStep(*case_result);
    ImGui::Text("%s", CaseDisplayName(*case_result).c_str());
    ImGui::SameLine();
    DrawStatusText(case_result->final_status);
    ImGui::TextDisabled("case_id=%s | internal=%s | scope=%s | final_state=%s",
                         TextOr(case_result->case_id, "NOT_AVAILABLE"),
                         TextOr(case_result->internal_case_id, "NOT_AVAILABLE"),
                         TextOr(case_result->verification_scope, "NOT_REPORTED"),
                         TextOr(case_result->final_state, "NOT_REPORTED"));

    DrawStateRail(selected_step);

    if (selected_step != nullptr &&
        (IsPendingStatus(selected_step->status) ||
         IsPendingStatus(selected_step->capability_status)))
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg,
                              ImVec4(0.22f, 0.15f, 0.05f, 0.72f));
        ImGui::BeginChild("BusinessPendingCallout", ImVec2(0.0f, 62.0f), true);
        ImGui::TextColored(ImVec4(0.96f, 0.65f, 0.20f, 1.0f),
                           "PENDING / NOT_AVAILABLE");
        ImGui::TextWrapped("Step %zu %s: %s", selected_step->index,
                           TextOr(selected_step->action, "unnamed action"),
                           TextOr(selected_step->reason,
                                  "No provider-backed result was reported."));
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    if (ImGui::BeginTabBar("BusinessWorkflowDetailTabs"))
    {
        if (ImGui::BeginTabItem("Overview"))
        {
            DrawContext(*case_result);
            ImGui::SeparatorText("Case and evidence paths");
            DrawPathRow("Manifest", case_result->manifest_path, "manifest");
            DrawPathRow("Case directory", case_result->case_directory,
                        "case-directory");
            DrawPathRow("Normalized identity path", case_result->normalized_path,
                        "normalized-path");
            DrawPathRow("Output directory", case_result->output_directory,
                        "output-directory");
            DrawPathRow("Case result", case_result->case_result_path,
                        "case-result");
            DrawRequiredAssets(case_result->required_assets);
            DrawAssetPreflight(case_result->asset_preflight);
            ImGui::SeparatorText("Batch evidence reports");
            DrawPathRow("Summary", m_result.summary_path, "summary");
            DrawPathRow("Markdown", m_result.report_md_path, "report-md");
            DrawPathRow("HTML", m_result.report_html_path, "report-html");
            DrawPathRow("Audit JSONL", m_result.audit_jsonl_path, "audit-jsonl");
            DrawPathRow("Scan debug", m_result.scan_debug_path, "scan-debug");
            DrawPathRow("Risk register", m_result.risk_register_path,
                        "risk-register");

            ImGui::SeparatorText("Pending capabilities");
            bool found_pending = false;
            for (const CxBusinessWorkflowStepResult& step : case_result->steps)
            {
                if (!IsPendingStatus(step.status) &&
                    !IsPendingStatus(step.capability_status))
                {
                    continue;
                }
                found_pending = true;
                ImGui::BulletText("#%zu %s | status=%s | capability=%s", step.index,
                                  TextOr(step.action, "unnamed action"),
                                  TextOr(step.status, "NOT_REPORTED"),
                                  TextOr(step.capability_status, "NOT_REPORTED"));
                if (!step.reason.empty())
                {
                    ImGui::Indent();
                    ImGui::TextWrapped("%s", step.reason.c_str());
                    ImGui::Unindent();
                }
            }
            if (!found_pending)
                ImGui::TextDisabled("No pending capability was reported.");
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Lifecycle timeline"))
        {
            DrawLifecycle(*case_result);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Image / ROI / detection"))
        {
            DrawGeometryAndDetections(*case_result, selected_step);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Risks & scan"))
        {
            DrawRisksAndScan(*case_result);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

void CxBusinessWorkflowPanel::DrawStateRail(
    const CxBusinessWorkflowStepResult* step) const
{
    const std::pair<const char*, std::string> channels[] = {
        {"EXECUTION", step == nullptr ? "NOT_REPORTED"
                                      : step->observation.execution_status},
        {"DETECTION", step == nullptr ? "NOT_REPORTED"
                                      : step->observation.detection_status},
        {"DISPLAY", step == nullptr ? "NOT_REPORTED"
                                    : step->observation.display_status},
        {"EVALUATION", step == nullptr ? "NOT_REPORTED"
                                       : step->observation.evaluation_status}};

    ImGui::TextDisabled(
        "FOUR-CHANNEL STATE RAIL%s",
        step == nullptr ? "" : " / selected lifecycle step");
    if (ImGui::BeginTable("BusinessFourChannelRail", 4,
                          ImGuiTableFlags_SizingStretchSame))
    {
        ImGui::TableNextRow();
        for (int index = 0; index < 4; ++index)
        {
            ImGui::TableSetColumnIndex(index);
            const std::string value =
                channels[index].second.empty() ? "NOT_REPORTED"
                                               : channels[index].second;
            const bool controlled_rejection =
                index == 0 && step != nullptr && step->expected_rejection &&
                UpperAscii(value) == "REJECTED";
            const ImVec4 accent =
                controlled_rejection
                    ? ImVec4(0.25f, 0.78f, 0.54f, 1.0f)
                    : StatusColor(value);
            const ImVec4 background(accent.x * 0.18f, accent.y * 0.18f,
                                    accent.z * 0.18f, 0.92f);
            ImGui::PushID(index);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, background);
            ImGui::PushStyleColor(ImGuiCol_Border, accent);
            ImGui::BeginChild("Channel", ImVec2(0.0f, 72.0f), true,
                              ImGuiWindowFlags_NoScrollbar);
            ImGui::TextDisabled("%s", channels[index].first);
            ImGui::TextColored(accent, "%s",
                               controlled_rejection ? "[OK]"
                                                    : StatusMarker(value));
            ImGui::SameLine();
            ImGui::TextWrapped("%s%s", value.c_str(),
                               controlled_rejection ? " (EXPECTED)" : "");
            ImGui::EndChild();
            ImGui::PopStyleColor(2);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (step != nullptr && !step->observation.reason.empty())
        ImGui::TextWrapped("Observation: %s", step->observation.reason.c_str());
}

void CxBusinessWorkflowPanel::DrawContext(
    const CxBusinessWorkflowCaseResult& case_result) const
{
    ImGui::SeparatorText("Lifecycle context");
    ImGui::TextDisabled(
        "Contract fixture refs and actually observed provider refs are kept "
        "separate; missing refs remain NOT_AVAILABLE.");
    if (ImGui::BeginTable("BusinessLifecycleContext", 3,
                          ImGuiTableFlags_Borders |
                              ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Context", ImGuiTableColumnFlags_WidthFixed,
                                150.0f);
        ImGui::TableSetupColumn("Contract fixture ref",
                                ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Observed provider ref",
                                ImGuiTableColumnFlags_WidthStretch);
        DrawScopedReferenceRow(
            "Project", LatestLifecycleRef(case_result, "project", true),
            LatestLifecycleRef(case_result, "project", false));
        DrawScopedReferenceRow(
            "Recipe", LatestLifecycleRef(case_result, "recipe", true),
            LatestLifecycleRef(case_result, "recipe", false));
        DrawScopedReferenceRow(
            "Dataset", LatestLifecycleRef(case_result, "dataset", true),
            LatestLifecycleRef(case_result, "dataset", false));
        DrawScopedReferenceRow(
            "Model", LatestLifecycleRef(case_result, "model", true),
            LatestLifecycleRef(case_result, "model", false));
        ImGui::EndTable();
    }

    if (!case_result.description.empty())
        ImGui::TextWrapped("Description: %s", case_result.description.c_str());
    ImGui::TextWrapped("Identity key: %s",
                       TextOr(case_result.identity_key, "NOT_AVAILABLE"));
    ImGui::Text("Expected final: %s | Actual final: %s",
                TextOr(case_result.expected_final_status, "NOT_REPORTED"),
                TextOr(case_result.final_status, "NOT_REPORTED"));
    if (!case_result.final_reason.empty())
        ImGui::TextWrapped("Final reason: %s", case_result.final_reason.c_str());
}

void CxBusinessWorkflowPanel::DrawLifecycle(
    const CxBusinessWorkflowCaseResult& case_result)
{
    ImGui::Text("%zu lifecycle steps. Select a row to drive the state rail and "
                "detection detail.",
                case_result.steps.size());
    if (case_result.steps.empty())
    {
        DrawEmptyCollection("lifecycle steps");
        return;
    }

    if (ImGui::BeginTable(
            "BusinessLifecycleTable", 7,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY |
                ImGuiTableFlags_SizingStretchProp,
            ImVec2(0.0f, std::max(260.0f, ImGui::GetContentRegionAvail().y))))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 42.0f);
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch,
                                1.3f);
        ImGui::TableSetupColumn("Transition",
                                ImGuiTableColumnFlags_WidthStretch, 1.2f);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthStretch,
                                1.0f);
        ImGui::TableSetupColumn("Capability",
                                ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Input / output",
                                ImGuiTableColumnFlags_WidthStretch, 1.7f);
        ImGui::TableSetupColumn("Evidence / reason",
                                ImGuiTableColumnFlags_WidthStretch, 1.8f);
        ImGui::TableHeadersRow();

        for (std::size_t row = 0; row < case_result.steps.size(); ++row)
        {
            const CxBusinessWorkflowStepResult& step = case_result.steps[row];
            ImGui::PushID(static_cast<int>(row));
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            const std::string row_label = std::to_string(step.index);
            if (ImGui::Selectable(row_label.c_str(), m_selected_step == row,
                                  ImGuiSelectableFlags_SpanAllColumns))
                m_selected_step = row;

            ImGui::TableSetColumnIndex(1);
            ImGui::TextWrapped("%s", TextOr(step.action, "unnamed action"));
            if (step.expected_rejection)
                ImGui::TextColored(ImVec4(0.96f, 0.65f, 0.20f, 1.0f),
                                   "expected rejection");
            if (step.contract_only)
                ImGui::TextDisabled("contract only");
            ImGui::TextDisabled("scope=%s | contract=%s",
                                TextOr(step.verification_scope, "NOT_REPORTED"),
                                TextOr(step.contract_validation_status,
                                       "NOT_REPORTED"));

            ImGui::TableSetColumnIndex(2);
            ImGui::TextWrapped("%s -> %s",
                               TextOr(step.state_before, "NOT_REPORTED"),
                               TextOr(step.state_after, "NOT_REPORTED"));
            ImGui::TextDisabled("changed=%s | dedup=%s",
                                step.state_changed ? "yes" : "no",
                                step.deduplicated ? "yes" : "no");

            ImGui::TableSetColumnIndex(3);
            if (step.expected_rejection && step.status == "PASS")
            {
                ImGui::TextColored(ImVec4(0.25f, 0.78f, 0.54f, 1.0f),
                                   "[OK] EXPECTED REJECTION");
            }
            else
            {
                DrawStatusText(step.status);
            }
            if (!step.code.empty())
                ImGui::TextDisabled("%s", step.code.c_str());

            ImGui::TableSetColumnIndex(4);
            DrawStatusText(step.capability_status);
            if (!step.provider.empty())
                ImGui::TextDisabled("provider=%s", step.provider.c_str());
            ImGui::TextDisabled("executed=%s",
                                step.provider_executed ? "yes" : "no");

            ImGui::TableSetColumnIndex(5);
            ImGui::TextWrapped("in: %s", TextOr(step.input_ref, "NOT_AVAILABLE"));
            ImGui::TextWrapped("out: %s",
                               TextOr(step.output_ref, "NOT_AVAILABLE"));

            ImGui::TableSetColumnIndex(6);
            ImGui::TextWrapped("evidence: %s",
                               TextOr(step.evidence_ref, "NOT_AVAILABLE"));
            ImGui::TextWrapped("%s", TextOr(step.reason, "No reason reported."));
            if (!step.human_decision.decision.empty())
            {
                ImGui::TextColored(ImVec4(0.30f, 0.72f, 0.86f, 1.0f),
                                   step.contract_only
                                       ? "contract decision metadata (not human "
                                         "execution): %s"
                                       : "human decision recorded: %s",
                                    step.human_decision.decision.c_str());
                ImGui::TextWrapped(
                    "event=%s | operator=%s | occurred=%s | input=%s | output=%s",
                    TextOr(step.human_decision.event_id,
                           step.contract_only ? "CONTRACT_FIXTURE" : "NOT_AVAILABLE"),
                    TextOr(step.human_decision.operator_id, "NOT_AVAILABLE"),
                    TextOr(step.human_decision.occurred_at, "NOT_AVAILABLE"),
                    TextOr(step.human_decision.input_version, "NOT_AVAILABLE"),
                    TextOr(step.human_decision.output_version, "NOT_AVAILABLE"));
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}

void CxBusinessWorkflowPanel::DrawGeometryAndDetections(
    const CxBusinessWorkflowCaseResult& case_result,
    const CxBusinessWorkflowStepResult* selected_step) const
{
    ImGui::SeparatorText("ImageRef and FrameRef");
    if (case_result.image_refs.empty())
    {
        DrawEmptyCollection("ImageRef records");
    }
    else if (ImGui::BeginTable("BusinessImageRefs", 5,
                               ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                   ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("Image / revision");
        ImGui::TableSetupColumn("Source / URI");
        ImGui::TableSetupColumn("Coordinate space");
        ImGui::TableSetupColumn("Dimensions");
        ImGui::TableSetupColumn("Content hash");
        ImGui::TableHeadersRow();
        for (const CxBusinessWorkflowImageRef& image : case_result.image_refs)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextWrapped("%s\nrev=%s", TextOr(image.image_id, "unnamed"),
                               TextOr(image.image_revision, "NOT_REPORTED"));
            ImGui::TableSetColumnIndex(1);
            ImGui::TextWrapped("%s\n%s", TextOr(image.source_type, "unknown"),
                               TextOr(image.uri, "NOT_AVAILABLE"));
            ImGui::TableSetColumnIndex(2);
            ImGui::TextWrapped("%s",
                               TextOr(image.coordinate_space, "NOT_REPORTED"));
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%d x %d x %d", image.width, image.height,
                        image.channels);
            ImGui::TableSetColumnIndex(4);
            ImGui::TextWrapped("%s",
                               TextOr(image.content_hash, "NOT_AVAILABLE"));
        }
        ImGui::EndTable();
    }

    if (!case_result.frame_refs.empty() &&
        ImGui::BeginTable("BusinessFrameRefs", 4,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("Frame");
        ImGui::TableSetupColumn("Image / source");
        ImGui::TableSetupColumn("Sequence / captured");
        ImGui::TableSetupColumn("URI");
        ImGui::TableHeadersRow();
        for (const CxBusinessWorkflowFrameRef& frame : case_result.frame_refs)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextWrapped("%s", TextOr(frame.frame_id, "unnamed"));
            ImGui::TableSetColumnIndex(1);
            ImGui::TextWrapped("%s\n%s", TextOr(frame.image_id, "NOT_AVAILABLE"),
                               TextOr(frame.source_type, "unknown"));
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%lld", frame.sequence);
            ImGui::TextWrapped("%s", TextOr(frame.captured_at, "NOT_REPORTED"));
            ImGui::TableSetColumnIndex(3);
            ImGui::TextWrapped("%s", TextOr(frame.uri, "NOT_AVAILABLE"));
        }
        ImGui::EndTable();
    }

    ImGui::SeparatorText("Declared and validated ROI / annotation contract");
    if (case_result.rois.empty())
    {
        DrawEmptyCollection("ROI records");
    }
    else if (ImGui::BeginTable("BusinessRois", 5,
                               ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                   ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("ROI");
        ImGui::TableSetupColumn("Source image / revision");
        ImGui::TableSetupColumn("Recipe revision");
        ImGui::TableSetupColumn("Coordinate / transform");
        ImGui::TableSetupColumn("Geometry");
        ImGui::TableHeadersRow();
        for (const CxBusinessWorkflowRoi& roi : case_result.rois)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextWrapped("%s", TextOr(roi.roi_id, "unnamed"));
            ImGui::TableSetColumnIndex(1);
            ImGui::TextWrapped("%s\nrev=%s",
                               TextOr(roi.source_image_id, "NOT_AVAILABLE"),
                               TextOr(roi.image_revision, "NOT_REPORTED"));
            ImGui::TableSetColumnIndex(2);
            ImGui::TextWrapped("%s",
                               TextOr(roi.recipe_revision, "NOT_REPORTED"));
            ImGui::TableSetColumnIndex(3);
            ImGui::TextWrapped("%s\ntransform=%s",
                               TextOr(roi.coordinate_space, "NOT_REPORTED"),
                               TextOr(roi.transform_ref, "NOT_AVAILABLE"));
            ImGui::TableSetColumnIndex(4);
            ImGui::TextWrapped("%s", TextOr(roi.geometry, "NOT_AVAILABLE"));
        }
        ImGui::EndTable();
    }

    if (case_result.annotations.empty())
    {
        DrawEmptyCollection("annotation records");
    }
    else if (ImGui::BeginTable("BusinessAnnotations", 6,
                               ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                   ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("Object / type");
        ImGui::TableSetupColumn("Image / ROI");
        ImGui::TableSetupColumn("Coordinate / geometry");
        ImGui::TableSetupColumn("Creation / creator");
        ImGui::TableSetupColumn("Status / confidence");
        ImGui::TableSetupColumn("Parent / evidence");
        ImGui::TableHeadersRow();
        for (const CxBusinessWorkflowAnnotation& annotation :
             case_result.annotations)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextWrapped("%s\n%s", TextOr(annotation.object_id, "unnamed"),
                               TextOr(annotation.object_type, "unknown"));
            ImGui::TableSetColumnIndex(1);
            ImGui::TextWrapped("%s\nroi=%s",
                               TextOr(annotation.source_image_id, "NOT_AVAILABLE"),
                               TextOr(annotation.roi_id, "NOT_AVAILABLE"));
            ImGui::TableSetColumnIndex(2);
            ImGui::TextWrapped("%s\n%s",
                               TextOr(annotation.coordinate_space,
                                      "NOT_REPORTED"),
                               TextOr(annotation.geometry, "NOT_AVAILABLE"));
            ImGui::TableSetColumnIndex(3);
            ImGui::TextWrapped("%s\n%s",
                               TextOr(annotation.creation_method,
                                      "NOT_REPORTED"),
                               TextOr(annotation.creator, "NOT_REPORTED"));
            ImGui::TableSetColumnIndex(4);
            DrawStatusText(annotation.status);
            ImGui::Text("confidence=%.4f", annotation.confidence);
            ImGui::TableSetColumnIndex(5);
            ImGui::TextWrapped("parent=%s\nevidence=%s",
                               TextOr(annotation.parent_ref, "NOT_AVAILABLE"),
                               TextOr(annotation.evidence_ref, "NOT_AVAILABLE"));
        }
        ImGui::EndTable();
    }

    ImGui::SeparatorText("Detection elements / selected step");
    if (selected_step == nullptr)
    {
        DrawEmptyCollection("selected lifecycle steps");
        return;
    }
    ImGui::Text("Step #%zu %s", selected_step->index,
                TextOr(selected_step->action, "unnamed action"));
    ImGui::Text("Detection status: %s | Display status: %s",
                TextOr(selected_step->observation.detection_status,
                       "NOT_REPORTED"),
                TextOr(selected_step->observation.display_status,
                       "NOT_REPORTED"));
    if (!selected_step->contract_fixture_elements.empty())
    {
        ImGui::TextColored(
            ImVec4(0.96f, 0.65f, 0.20f, 1.0f),
            "EXPECTED CONTRACT FIXTURE - schema/UI preview; never observed provider output.");
        if (ImGui::BeginTable("BusinessContractFixtureElements", 6,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_Resizable))
        {
            ImGui::TableSetupColumn("Element / type");
            ImGui::TableSetupColumn("Name / status");
            ImGui::TableSetupColumn("Score");
            ImGui::TableSetupColumn("Source / coordinate");
            ImGui::TableSetupColumn("Geometry");
            ImGui::TableSetupColumn("Evidence");
            ImGui::TableHeadersRow();
            for (const CxBusinessWorkflowDetectionElement& element :
                 selected_step->contract_fixture_elements)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextWrapped("%s\n%s", TextOr(element.element_id, "unnamed"),
                                   TextOr(element.element_type, "unknown"));
                ImGui::TableSetColumnIndex(1);
                ImGui::TextWrapped("%s", TextOr(element.name, "unnamed"));
                DrawStatusText(element.status);
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%.4f", element.score);
                ImGui::TableSetColumnIndex(3);
                ImGui::TextWrapped("%s\n%s", TextOr(element.source, "NOT_REPORTED"),
                                   TextOr(element.coordinate_space, "NOT_REPORTED"));
                ImGui::TableSetColumnIndex(4);
                ImGui::TextWrapped("%s", TextOr(element.geometry, "NOT_AVAILABLE"));
                ImGui::TableSetColumnIndex(5);
                ImGui::TextWrapped("%s", TextOr(element.evidence_ref, "NOT_AVAILABLE"));
            }
            ImGui::EndTable();
        }
    }
    ImGui::SeparatorText("Observed provider detection elements");
    if (selected_step->observation.detection_elements.empty())
    {
        ImGui::TextDisabled(
            "No detection elements reported. Zero detections do not imply a "
            "display failure.");
        return;
    }

    if (ImGui::BeginTable("BusinessDetectionElements", 6,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("Element / type");
        ImGui::TableSetupColumn("Name / status");
        ImGui::TableSetupColumn("Score");
        ImGui::TableSetupColumn("Source / coordinate");
        ImGui::TableSetupColumn("Geometry");
        ImGui::TableSetupColumn("Evidence");
        ImGui::TableHeadersRow();
        for (const CxBusinessWorkflowDetectionElement& element :
             selected_step->observation.detection_elements)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextWrapped("%s\n%s", TextOr(element.element_id, "unnamed"),
                               TextOr(element.element_type, "unknown"));
            ImGui::TableSetColumnIndex(1);
            ImGui::TextWrapped("%s", TextOr(element.name, "unnamed"));
            DrawStatusText(element.status);
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.4f", element.score);
            ImGui::TableSetColumnIndex(3);
            ImGui::TextWrapped("%s\n%s", TextOr(element.source, "NOT_REPORTED"),
                               TextOr(element.coordinate_space,
                                      "NOT_REPORTED"));
            ImGui::TableSetColumnIndex(4);
            ImGui::TextWrapped("%s",
                               TextOr(element.geometry, "NOT_AVAILABLE"));
            ImGui::TableSetColumnIndex(5);
            ImGui::TextWrapped("%s",
                               TextOr(element.evidence_ref, "NOT_AVAILABLE"));
        }
        ImGui::EndTable();
    }
}

void CxBusinessWorkflowPanel::DrawRisksAndScan(
    const CxBusinessWorkflowCaseResult& case_result) const
{
    ImGui::SeparatorText("Case risks");
    if (case_result.risks.empty())
    {
        DrawEmptyCollection("case risk records");
    }
    else if (ImGui::BeginTable("BusinessCaseRisks", 5,
                               ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                   ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("Code");
        ImGui::TableSetupColumn("Severity / blocking");
        ImGui::TableSetupColumn("Stage");
        ImGui::TableSetupColumn("Disposition");
        ImGui::TableSetupColumn("Reason");
        ImGui::TableHeadersRow();
        for (const CxBusinessRiskRecord& risk : case_result.risks)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextWrapped("%s", TextOr(risk.code, "NO_CODE"));
            ImGui::TableSetColumnIndex(1);
            ImGui::TextWrapped("%s\nblocking=%s",
                               TextOr(risk.severity, "NOT_REPORTED"),
                               risk.blocking ? "yes" : "no");
            ImGui::TableSetColumnIndex(2);
            ImGui::TextWrapped("%s", TextOr(risk.stage, "NOT_REPORTED"));
            ImGui::TableSetColumnIndex(3);
            ImGui::TextWrapped("%s",
                               TextOr(risk.disposition, "NOT_REPORTED"));
            ImGui::TableSetColumnIndex(4);
            ImGui::TextWrapped("%s", TextOr(risk.reason, "NO_REASON"));
        }
        ImGui::EndTable();
    }

    ImGui::SeparatorText("Batch risks");
    for (const CxBusinessRiskRecord& risk : m_result.risks)
    {
        ImGui::BulletText("%s | severity=%s | blocking=%s | %s",
                          TextOr(risk.code, "NO_CODE"),
                          TextOr(risk.severity, "NOT_REPORTED"),
                          risk.blocking ? "yes" : "no",
                          TextOr(risk.reason, "NO_REASON"));
    }
    if (m_result.risks.empty())
        DrawEmptyCollection("batch risk records");

    ImGui::SeparatorText("Manifest scan findings");
    if (m_result.scan_records.empty())
    {
        DrawEmptyCollection("scan records");
    }
    else if (ImGui::BeginTable("BusinessScanRecords", 5,
                               ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                   ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
                               ImVec2(0.0f, 260.0f)))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Outcome");
        ImGui::TableSetupColumn("case_id / internal");
        ImGui::TableSetupColumn("Code");
        ImGui::TableSetupColumn("Path");
        ImGui::TableSetupColumn("Reason");
        ImGui::TableHeadersRow();
        for (const CxBusinessWorkflowScanRecord& record : m_result.scan_records)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            DrawStatusText(record.outcome);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextWrapped("%s\n%s", TextOr(record.case_id, "NOT_AVAILABLE"),
                               TextOr(record.internal_case_id, "NOT_AVAILABLE"));
            ImGui::TableSetColumnIndex(2);
            ImGui::TextWrapped("%s", TextOr(record.code, "NO_CODE"));
            ImGui::TableSetColumnIndex(3);
            ImGui::TextWrapped("%s", record.path.generic_string().c_str());
            ImGui::TableSetColumnIndex(4);
            ImGui::TextWrapped("%s", TextOr(record.reason, "NO_REASON"));
        }
        ImGui::EndTable();
    }

    if (!m_result.output_errors.empty())
    {
        ImGui::SeparatorText("Evidence write errors");
        for (const std::string& error : m_result.output_errors)
            ImGui::BulletText("%s", error.c_str());
    }
}
