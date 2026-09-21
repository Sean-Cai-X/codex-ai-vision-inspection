#include "DevAnalysisGuiPanels.h"

#include "../adapters/DevAnalysisGeomAttachmentDemo.h"

#include <imgui.h>
#include <cstdint>

namespace
{
ImVec4 StatusColor(bool active)
{
  return active ? ImVec4(0.95f, 0.62f, 0.19f, 1.0f) : ImVec4(0.45f, 0.48f, 0.52f, 1.0f);
}

ImVec4 StatusTone(const std::string& value)
{
  if (value == "L4" || value == "L3" || value == "true" || value == "ready")
  {
    return ImVec4(0.40f, 0.78f, 0.46f, 1.0f);
  }
  if (value == "L2" || value == "pending")
  {
    return ImVec4(0.95f, 0.72f, 0.19f, 1.0f);
  }
  if (value == "L1" || value == "L0" || value == "false")
  {
    return ImVec4(0.93f, 0.32f, 0.29f, 1.0f);
  }
  return ImVec4(0.79f, 0.82f, 0.86f, 1.0f);
}

void DrawFlowBadge(const char* label, bool active)
{
  ImGui::PushStyleColor(ImGuiCol_Button, StatusColor(active));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, StatusColor(active));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, StatusColor(active));
  ImGui::Button(label, ImVec2(140.0f, 0.0f));
  ImGui::PopStyleColor(3);
}

void DrawSectionTitle(const char* title, const char* subtitle)
{
  ImGui::TextUnformatted(title);
  ImGui::TextDisabled("%s", subtitle);
  ImGui::Separator();
}

void DrawViewportLegend()
{
  ImGui::TextColored(ImVec4(0.98f, 0.75f, 0.29f, 1.0f), "L3 Result Layer");
  ImGui::BulletText("Image / ROI / Circle / Line / Boundary");
  ImGui::BulletText("3D / OCCT element staging");
  ImGui::BulletText("Intermediate mapping overlays");
}

ImU32 ElementColor(const std::string& kind_label, bool selected)
{
  if (selected)
  {
    return IM_COL32(250, 213, 92, 255);
  }

  if (kind_label == "Circle")
  {
    return IM_COL32(239, 92, 88, 255);
  }
  if (kind_label == "Line" || kind_label == "Contract Line" || kind_label == "Vector Lane")
  {
    return IM_COL32(96, 208, 145, 255);
  }
  if (kind_label == "Boundary" || kind_label == "Model Gate")
  {
    return IM_COL32(88, 144, 255, 255);
  }

  return IM_COL32(172, 186, 201, 255);
}

void DrawObservationCanvas(const DevAnalysisState& state)
{
  const ImVec2 canvas = ImGui::GetContentRegionAvail();
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  ImDrawList* draw_list = ImGui::GetWindowDrawList();

  draw_list->AddRectFilled(origin,
                           ImVec2(origin.x + canvas.x, origin.y + canvas.y),
                           IM_COL32(28, 31, 35, 255),
                           8.0f);
  draw_list->AddRect(origin,
                     ImVec2(origin.x + canvas.x, origin.y + canvas.y),
                     IM_COL32(210, 160, 52, 255),
                     8.0f,
                     0,
                     2.0f);

  if (state.active_visual_loaded && state.active_visual_texture_id != 0 &&
      state.active_visual_width > 0 && state.active_visual_height > 0)
  {
    const float width_scale = canvas.x / static_cast<float>(state.active_visual_width);
    const float height_scale = canvas.y / static_cast<float>(state.active_visual_height);
    const float scale = (width_scale < height_scale) ? width_scale : height_scale;
    const float draw_width = static_cast<float>(state.active_visual_width) * scale;
    const float draw_height = static_cast<float>(state.active_visual_height) * scale;
    const ImVec2 image_min(origin.x + (canvas.x - draw_width) * 0.5f,
                           origin.y + (canvas.y - draw_height) * 0.5f);
    const ImVec2 image_max(image_min.x + draw_width, image_min.y + draw_height);

    draw_list->AddImage(static_cast<ImTextureID>(static_cast<ImU64>(state.active_visual_texture_id)),
                        image_min,
                        image_max,
                        ImVec2(0.0f, 1.0f),
                        ImVec2(1.0f, 0.0f));

    if (state.show_detected_rect && state.detected_rect_valid)
    {
      const float rect_scale_x = draw_width / static_cast<float>(state.active_visual_width);
      const float rect_scale_y = draw_height / static_cast<float>(state.active_visual_height);
      const ImVec2 rect_min(image_min.x + state.detected_rect_x * rect_scale_x,
                            image_min.y + state.detected_rect_y * rect_scale_y);
      const ImVec2 rect_max(rect_min.x + state.detected_rect_width * rect_scale_x,
                            rect_min.y + state.detected_rect_height * rect_scale_y);
      draw_list->AddRect(rect_min, rect_max, IM_COL32(255, 111, 0, 255), 4.0f, 0, 3.0f);
      if (state.show_labels)
      {
        draw_list->AddRectFilled(ImVec2(rect_min.x, rect_min.y - 22.0f),
                                 ImVec2(rect_min.x + 180.0f, rect_min.y - 2.0f),
                                 IM_COL32(19, 23, 27, 220),
                                 4.0f);
        draw_list->AddText(ImVec2(rect_min.x + 6.0f, rect_min.y - 18.0f),
                           IM_COL32(255, 214, 102, 255),
                           state.detected_rect_label.empty() ? "top1_rect" : state.detected_rect_label.c_str());
      }
    }

    draw_list->AddText(ImVec2(origin.x + 18.0f, origin.y + 16.0f), IM_COL32(245, 245, 245, 255), DevAnalysisChainLabel(state.selected_chain));
    draw_list->AddText(ImVec2(origin.x + 18.0f, origin.y + 34.0f), IM_COL32(198, 203, 209, 255), DevAnalysisStageLabel(state.selected_stage));
    draw_list->AddText(ImVec2(origin.x + 18.0f, origin.y + 52.0f), IM_COL32(198, 203, 209, 255), state.active_visual_path.c_str());

    ImGui::Dummy(canvas);
    DrawViewportLegend();
    return;
  }

  const DevAnalysisGeomAttachmentScene scene = BuildDevAnalysisGeomAttachmentScene(state.selected_chain);
  for (const DevAnalysisDemoElement& element : scene.elements)
  {
    const ImVec2 min_point(origin.x + (element.x * canvas.x), origin.y + (element.y * canvas.y));
    const ImVec2 max_point(min_point.x + (element.width * canvas.x), min_point.y + (element.height * canvas.y));
    const bool selected = element.entity_id == state.selected_entity_id;
    const ImU32 color = ElementColor(element.kind_label, selected);
    draw_list->AddRect(min_point, max_point, color, 4.0f, 0, selected ? 3.0f : 2.0f);
    draw_list->AddText(ImVec2(min_point.x + 8.0f, min_point.y + 8.0f), IM_COL32(241, 243, 244, 255), element.name.c_str());
    draw_list->AddText(ImVec2(min_point.x + 8.0f, min_point.y + 26.0f), IM_COL32(188, 194, 201, 255), element.source_stage.c_str());
  }

  for (const DevAnalysisDemoAnnotation& annotation : scene.annotations)
  {
    if (!annotation.visible)
    {
      continue;
    }

    const ImVec2 anchor(origin.x + (annotation.anchor_x * canvas.x), origin.y + (annotation.anchor_y * canvas.y));
    const ImVec2 label(anchor.x + annotation.label_dx, anchor.y + annotation.label_dy);

    draw_list->AddCircleFilled(anchor, 4.0f, IM_COL32(248, 198, 62, 255));
    if (state.show_anchor_lines)
    {
      draw_list->AddLine(anchor, label, IM_COL32(248, 198, 62, 220), 1.5f);
    }

    if (state.show_labels)
    {
      draw_list->AddRectFilled(ImVec2(label.x - 6.0f, label.y - 4.0f),
                               ImVec2(label.x + 220.0f, label.y + 18.0f),
                               IM_COL32(19, 23, 27, 220),
                               4.0f);
      draw_list->AddText(label, IM_COL32(245, 245, 245, 255), annotation.text.c_str());
    }
  }

  draw_list->AddText(ImVec2(origin.x + 18.0f, origin.y + 16.0f), IM_COL32(245, 245, 245, 255), DevAnalysisChainLabel(state.selected_chain));
  draw_list->AddText(ImVec2(origin.x + 18.0f, origin.y + 34.0f), IM_COL32(198, 203, 209, 255), DevAnalysisStageLabel(state.selected_stage));

  ImGui::Dummy(canvas);
  DrawViewportLegend();
}

void DrawOperationField(const char* label, const std::string& value)
{
  ImGui::TextUnformatted(label);
  ImGui::TextWrapped("%s", value.c_str());
  ImGui::Spacing();
}

void DrawEditableOperationField(const char* label, char* buffer, std::size_t buffer_size, float height)
{
  ImGui::TextUnformatted(label);
  ImGui::InputTextMultiline(label,
                            buffer,
                            buffer_size,
                            ImVec2(-1.0f, height));
  ImGui::Spacing();
}

ImGuiTabItemFlags WorkspaceTabFlags(const DevAnalysisState& state, const char* label)
{
  if (!state.forced_workspace_tab.empty() && state.forced_workspace_tab == label)
  {
    return ImGuiTabItemFlags_SetSelected;
  }
  return ImGuiTabItemFlags_None;
}

void DrawAiCapabilityTable(const DevAnalysisState& state)
{
  if (ImGui::BeginTable("ai_capability_matrix", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
  {
    ImGui::TableSetupColumn("Task Type");
    ImGui::TableSetupColumn("Level");
    ImGui::TableSetupColumn("Evidence");
    ImGui::TableSetupColumn("Risk");
    ImGui::TableSetupColumn("Strengthen Thread");
    ImGui::TableSetupColumn("Supervisor");
    ImGui::TableSetupColumn("Next");
    ImGui::TableHeadersRow();

    for (const DevAnalysisAiCapabilityRow& row : state.ai_capability_rows)
    {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextWrapped("%s", row.task_type.c_str());
      ImGui::TableSetColumnIndex(1);
      ImGui::TextColored(StatusTone(row.capability_level), "%s", row.capability_level.c_str());
      ImGui::TableSetColumnIndex(2);
      ImGui::TextWrapped("%s", row.evidence_ref.c_str());
      ImGui::TableSetColumnIndex(3);
      ImGui::TextWrapped("%s", row.risk_text.c_str());
      ImGui::TableSetColumnIndex(4);
      ImGui::TextWrapped("%s", row.strengthen_thread.c_str());
      ImGui::TableSetColumnIndex(5);
      ImGui::TextWrapped("%s", row.supervisor_ready.c_str());
      ImGui::TableSetColumnIndex(6);
      ImGui::TextWrapped("%s", row.next_step.c_str());
    }

    ImGui::EndTable();
  }
}

void DrawSemanticActionTable(DevAnalysisState& state)
{
  if (ImGui::BeginTable("semantic_action_map", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
  {
    ImGui::TableSetupColumn("action_id");
    ImGui::TableSetupColumn("display_name");
    ImGui::TableSetupColumn("intent");
    ImGui::TableSetupColumn("required_args");
    ImGui::TableSetupColumn("success_rule");
    ImGui::TableSetupColumn("risk");
    ImGui::TableSetupColumn("fallback");
    ImGui::TableHeadersRow();

    for (int index = 0; index < static_cast<int>(state.semantic_action_rows.size()); ++index)
    {
      const DevAnalysisSemanticActionRow& row = state.semantic_action_rows[static_cast<std::size_t>(index)];
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      if (ImGui::Selectable(row.action_id.c_str(),
                            state.selected_action_index == index,
                            ImGuiSelectableFlags_SpanAllColumns))
      {
        state.selected_action_index = index;
        state.selected_action_id = row.action_id;
        state.selected_action_intent = row.intent;
        state.operation_select.script_action_ref = row.action_id + " -> " + row.intent;
        state.operation_run.step_run_ref = row.action_id;
      }
      ImGui::TableSetColumnIndex(1);
      ImGui::TextWrapped("%s", row.display_name.c_str());
      ImGui::TableSetColumnIndex(2);
      ImGui::TextWrapped("%s", row.intent.c_str());
      ImGui::TableSetColumnIndex(3);
      ImGui::TextWrapped("%s", row.required_args.c_str());
      ImGui::TableSetColumnIndex(4);
      ImGui::TextWrapped("%s", row.success_rule.c_str());
      ImGui::TableSetColumnIndex(5);
      ImGui::TextWrapped("%s", row.risk_text.c_str());
      ImGui::TableSetColumnIndex(6);
      ImGui::TextWrapped("%s", row.fallback.c_str());
    }

    ImGui::EndTable();
  }
}
}

void DrawDevAnalysisTopFlow(const DevAnalysisState& state)
{
  ImGui::Begin("Flow");
  DrawSectionTitle("Semantic Operation Flow",
                   "The work surface stays action-first: select, adjust, run, observe, judge, and record.");
  DrawFlowBadge("Select", true);
  ImGui::SameLine();
  DrawFlowBadge("Adjust", true);
  ImGui::SameLine();
  DrawFlowBadge("Run", true);
  ImGui::SameLine();
  DrawFlowBadge("Observe", true);
  ImGui::SameLine();
  DrawFlowBadge("Judge", true);
  ImGui::SameLine();
  DrawFlowBadge("Record", true);
  ImGui::Spacing();
  ImGui::Text("Current chain: %s", DevAnalysisChainLabel(state.selected_chain));
  ImGui::Text("Current stage: %s", DevAnalysisStageLabel(state.selected_stage));
  ImGui::Text("Current case: %s", state.cases[state.selected_case_index].name.c_str());
  ImGui::TextWrapped("Script module: %s", state.operation_select.script_module_ref.c_str());
  ImGui::TextWrapped("Selected action: %s", state.selected_action_id.empty() ? "none" : state.selected_action_id.c_str());
  ImGui::End();
}

void DrawDevAnalysisControlPanel(DevAnalysisState& state)
{
  ImGui::Begin("Controls");
  DrawSectionTitle("Select What / Change What",
                   "Keep only the current chain, current case, and the current semantic adjustment surface in focus.");

  int selected_chain = static_cast<int>(state.selected_chain);
  if (ImGui::RadioButton("cxcore -> ensmallen", selected_chain == 0))
  {
    selected_chain = 0;
  }
  if (ImGui::RadioButton("cxcore -> mlpack", selected_chain == 1))
  {
    selected_chain = 1;
  }
  if (ImGui::RadioButton("cxcore -> torch", selected_chain == 2))
  {
    selected_chain = 2;
  }

  const DevAnalysisChainId next_chain = static_cast<DevAnalysisChainId>(selected_chain);
  if (next_chain != state.selected_chain)
  {
    state.selected_chain = next_chain;
    RefreshDevAnalysisChainData(state);
  }

  ImGui::Spacing();
  ImGui::TextUnformatted("Case");
  if (ImGui::BeginCombo("##case", state.cases[state.selected_case_index].name.c_str()))
  {
    for (int index = 0; index < static_cast<int>(state.cases.size()); ++index)
    {
      const bool is_selected = state.selected_case_index == index;
      if (ImGui::Selectable(state.cases[index].name.c_str(), is_selected))
      {
        state.selected_case_index = index;
        RefreshDevAnalysisChainData(state);
      }
      if (is_selected)
      {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }
  ImGui::TextWrapped("%s", state.cases[state.selected_case_index].summary.c_str());

  ImGui::Spacing();
  ImGui::TextUnformatted("Stage");
  for (int stage = 0; stage <= static_cast<int>(DevAnalysisStageId::TrainOrInferReady); ++stage)
  {
    const bool selected = static_cast<int>(state.selected_stage) == stage;
    if (ImGui::Selectable(DevAnalysisStageLabel(static_cast<DevAnalysisStageId>(stage)), selected))
    {
      state.selected_stage = static_cast<DevAnalysisStageId>(stage);
      state.operation_observe.stage_ref = DevAnalysisStageLabel(state.selected_stage);
    }
  }

  ImGui::Spacing();
  DrawOperationField("Selected script module", state.operation_select.script_module_ref);
  DrawOperationField("Sample switch", state.operation_select.sample_switch_ref);
  DrawOperationField("Selected test image", state.operation_select.test_image_ref);
  DrawOperationField("Selected primary chain", state.operation_select.primary_chain_ref);

  ImGui::Checkbox("Show overlay mapping", &state.show_overlay);
  ImGui::Checkbox("Show primary visual", &state.show_primary_visual);
  ImGui::Checkbox("Show detected rect", &state.show_detected_rect);
  ImGui::Checkbox("Show text labels", &state.show_labels);
  ImGui::Checkbox("Show anchor lines", &state.show_anchor_lines);
  ImGui::End();
}

void DrawDevAnalysisViewportPanel(const DevAnalysisState& state)
{
  ImGui::Begin("Observation");
  DrawSectionTitle("Image / Elements", "Legacy direct observation window.");
  DrawObservationCanvas(state);
  ImGui::End();
}

void DrawDevAnalysisWorkspaceTabs(DevAnalysisState& state)
{
  ImGui::Begin("Workspace");
  DrawSectionTitle("Semantic Operation Desk",
                   "This work area stays thin and action-semantic: script module operation, replay, observation, judgment, and traceability.");

  if (ImGui::BeginTabBar("semantic_operation_tabs"))
  {
    if (ImGui::BeginTabItem("Select What", nullptr, WorkspaceTabFlags(state, "Select What")))
    {
      DrawOperationField("Script module", state.operation_select.script_module_ref);
      DrawOperationField("Task / case", state.operation_select.task_case_ref);
      DrawOperationField("Sample switch", state.operation_select.sample_switch_ref);
      DrawOperationField("Test image", state.operation_select.test_image_ref);
      DrawOperationField("Primary chain", state.operation_select.primary_chain_ref);
      DrawOperationField("Current script action", state.operation_select.script_action_ref);
      ImGui::SeparatorText("Semantic Action Catalog");
      DrawSemanticActionTable(state);
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Adjust What", nullptr, WorkspaceTabFlags(state, "Adjust What")))
    {
      DrawSectionTitle("Parameter Injection and Settings",
                       "This tab carries ViewController-like semantics: direct parameter injection, geometry settings, acquisition mode, and runtime entry arguments.");
      DrawEditableOperationField("Parameter injection", state.parameter_injection_buffer.data(), state.parameter_injection_buffer.size(), 86.0f);
      DrawEditableOperationField("Geometry settings", state.geometry_setting_buffer.data(), state.geometry_setting_buffer.size(), 86.0f);
      DrawEditableOperationField("Acquisition mode", state.acquisition_mode_buffer.data(), state.acquisition_mode_buffer.size(), 86.0f);
      DrawEditableOperationField("Runtime arguments", state.runtime_arg_buffer.data(), state.runtime_arg_buffer.size(), 86.0f);
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Run What", nullptr, WorkspaceTabFlags(state, "Run What")))
    {
      DrawSectionTitle("Single Step / Segment / Full Chain / Replay",
                       "This tab exposes the script operation route instead of only showing a result shell.");
      DrawOperationField("Single-step script run", state.operation_run.step_run_ref);
      DrawOperationField("Single-segment run", state.operation_run.segment_run_ref);
      DrawOperationField("Full-chain run", state.operation_run.full_chain_run_ref);
      DrawOperationField("Replay / reproduce", state.operation_run.replay_run_ref);
      DrawOperationField("Trace route", state.operation_run.trace_run_ref);
      ImGui::SeparatorText("Remote Evidence");
      DrawOperationField("Remote entry", state.remote_status.entry);
      DrawOperationField("Remote action", state.remote_status.action);
      DrawOperationField("Remote result", state.remote_status.result);
      DrawOperationField("Remote blocker", state.remote_status.blocker);
      DrawOperationField("Remote next", state.remote_status.next_step);
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Observe What", nullptr, WorkspaceTabFlags(state, "Observe What")))
    {
      DrawSectionTitle("Image / Elements / Chain / Conclusion",
                       "Observation stays in the same work surface as the action semantics so issues can be traced back quickly.");
      if (ImGui::BeginChild("observe_canvas", ImVec2(0.0f, 280.0f), true))
      {
        DrawObservationCanvas(state);
      }
      ImGui::EndChild();
      ImGui::Spacing();
      DrawOperationField("Input image", state.operation_observe.input_image_ref);
      DrawOperationField("Primary visual", state.operation_observe.primary_visual_ref);
      DrawOperationField("Supporting images", state.operation_observe.supporting_image_refs_ref);
      DrawOperationField("Statistics evidence", state.operation_observe.statistics_evidence_ref);
      DrawOperationField("Baseline result", state.operation_observe.baseline_result_ref);
      DrawOperationField("Stage semantic", state.operation_observe.stage_semantic_ref);
      DrawOperationField("Problem entry", state.operation_observe.problem_entry_ref);
      DrawOperationField("Evidence ref", state.operation_observe.evidence_ref);
      DrawOperationField("Result ref", state.operation_observe.result_ref);
      DrawOperationField("Elements", state.operation_observe.elements_ref);
      DrawOperationField("Element summary", state.operation_observe.element_summary_ref);
      DrawOperationField("Element chains", state.operation_observe.element_chains_ref);
      DrawOperationField("Element chain summary", state.operation_observe.element_chain_summary_ref);
      DrawOperationField("Element status summary", state.operation_observe.element_status_summary_ref);
      DrawOperationField("Element layer", state.operation_observe.element_layer_ref);
      DrawOperationField("Visualization refs", state.operation_observe.visualization_refs);
      DrawOperationField("GUI visual refs", state.operation_observe.gui_visual_refs);
      DrawOperationField("Main chain", state.operation_observe.main_chain_ref);
      DrawOperationField("Stage", state.operation_observe.stage_ref);
      DrawOperationField("Stage execution", state.operation_observe.stage_execution_entry_ref);
      DrawOperationField("Conclusion", state.operation_observe.conclusion_ref);
      DrawOperationField("Anomaly / issue", state.operation_observe.anomaly_ref);
      DrawOperationField("Notes", state.operation_observe.notes_ref);
      DrawOperationField("Thread handoff", state.operation_observe.thread_handoff_ref);
      ImGui::SeparatorText("AI Capability Evidence");
      DrawAiCapabilityTable(state);
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Judge What", nullptr, WorkspaceTabFlags(state, "Judge What")))
    {
      DrawSectionTitle("Current Judgment Surface",
                       "Judge whether the issue is more like script, parameter, acquisition, matching, or template trouble before escalating.");
      DrawOperationField("Current status", state.operation_judge.current_status_ref);
      DrawOperationField("Issue kind", state.operation_judge.issue_kind_ref);
      DrawOperationField("Remediation priority", state.operation_judge.remediation_priority_ref);
      DrawOperationField("Manual review gate", state.operation_judge.manual_review_gate_ref);

      ImGui::TextUnformatted("Human Judgment");
      if (ImGui::RadioButton("Continue Advance", state.judgment == DevAnalysisJudgment::ContinueAdvance))
      {
        state.judgment = DevAnalysisJudgment::ContinueAdvance;
      }
      ImGui::SameLine();
      if (ImGui::RadioButton("Questionable", state.judgment == DevAnalysisJudgment::Questionable))
      {
        state.judgment = DevAnalysisJudgment::Questionable;
      }
      ImGui::SameLine();
      if (ImGui::RadioButton("Push Back Thread", state.judgment == DevAnalysisJudgment::PushBackThread))
      {
        state.judgment = DevAnalysisJudgment::PushBackThread;
      }
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Record What", nullptr, WorkspaceTabFlags(state, "Record What")))
    {
      DrawSectionTitle("Human Notes and Replay Trace",
                       "Record the manual note, issue entry point, next step, and replay reference directly on the same semantic surface.");
      DrawOperationField("Runtime fillback status", state.operation_record.runtime_fillback_status_ref);
      DrawEditableOperationField("Human note", state.human_note_buffer.data(), state.human_note_buffer.size(), 120.0f);
      DrawEditableOperationField("Issue entry point", state.issue_entry_buffer.data(), state.issue_entry_buffer.size(), 90.0f);
      DrawEditableOperationField("Next-step note", state.next_step_buffer.data(), state.next_step_buffer.size(), 90.0f);
      DrawOperationField("Replay reference", state.operation_record.replay_ref);
      ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
  }

  ImGui::End();
}

void DrawDevAnalysisResultPanel(const DevAnalysisState& state)
{
  ImGui::Begin("Results");
  DrawSectionTitle("Current Semantic Result",
                   "Keep the current result readable as script semantics, checks, blocking point, and next-step consequence.");

  ImGui::Text("Elapsed: %.1f ms", state.metrics.elapsed_ms);
  ImGui::Text("Memory: %.1f MB", state.metrics.memory_mb);
  ImGui::Spacing();
  ImGui::TextWrapped("Key fields: %s", state.metrics.key_fields.c_str());
  ImGui::TextWrapped("Blocking point: %s", state.metrics.blocking_point.c_str());
  ImGui::TextWrapped("Statistics evidence: %s", state.operation_observe.statistics_evidence_ref.c_str());
  ImGui::TextWrapped("Baseline result: %s", state.operation_observe.baseline_result_ref.c_str());
  ImGui::TextWrapped("Key checks: %s", state.selected_case_checks.c_str());
  ImGui::TextWrapped("Expected failure mode: %s", state.selected_case_failure_mode.c_str());
  ImGui::TextWrapped("Push-back hint: %s", state.selected_case_pushback_hint.c_str());

  if (state.show_summary)
  {
    ImGui::Spacing();
    for (const DevAnalysisEntryRecord& entry : state.entries)
    {
      ImGui::SeparatorText(entry.module.c_str());
      DrawOperationField("Entry", entry.entry);
      DrawOperationField("Action", entry.action);
      DrawOperationField("Result", entry.result);
      DrawOperationField("Next", entry.next_step);
    }
  }

  ImGui::End();
}

void DrawDevAnalysisActionPanel(DevAnalysisState& state)
{
  ImGui::Begin("Actions");
  DrawSectionTitle("Semantic Quick Actions",
                   "Buttons bind the current action_id, but the surrounding decision stays on script semantics and replayability.");

  for (int index = 0; index < static_cast<int>(state.semantic_action_rows.size()) && index < 6; ++index)
  {
    const DevAnalysisSemanticActionRow& row = state.semantic_action_rows[static_cast<std::size_t>(index)];
    if (ImGui::Button(row.action_id.c_str(), ImVec2(180.0f, 0.0f)))
    {
      state.selected_action_index = index;
      state.selected_action_id = row.action_id;
      state.selected_action_intent = row.intent;
      state.operation_select.script_action_ref = row.action_id + " -> " + row.intent;
      state.operation_run.step_run_ref = row.action_id;
    }
    if ((index % 3) != 2)
    {
      ImGui::SameLine();
    }
  }

  ImGui::Spacing();
  ImGui::TextWrapped("Bound action_id: %s", state.selected_action_id.empty() ? "none" : state.selected_action_id.c_str());
  ImGui::TextWrapped("Intent: %s", state.selected_action_intent.empty() ? "none" : state.selected_action_intent.c_str());
  ImGui::TextWrapped("Replay ref: %s", state.operation_record.replay_ref.c_str());
  ImGui::End();
}

void DrawDevAnalysisAttachmentPanel(DevAnalysisState& state)
{
  ImGui::Begin("Attachment");
  DrawSectionTitle("Traceback / Attachment",
                   "Keep the element body, label attachment, and interaction path visible so the semantic operation route stays explainable.");

  const DevAnalysisGeomAttachmentScene scene = BuildDevAnalysisGeomAttachmentScene(state.selected_chain);
  ImGui::TextUnformatted("Elements");
  for (const DevAnalysisDemoElement& element : scene.elements)
  {
    const bool selected = state.selected_entity_id == element.entity_id;
    if (ImGui::Selectable(element.name.c_str(), selected))
    {
      state.selected_entity_id = element.entity_id;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("[%s / %s]", element.kind_label.c_str(), element.source_stage.c_str());
  }

  ImGui::Spacing();
  ImGui::TextUnformatted("Display Layers");
  for (const std::string& layer : state.display_layers)
  {
    ImGui::BulletText("%s", layer.c_str());
  }

  ImGui::Spacing();
  ImGui::TextUnformatted("Interaction Steps");
  for (const std::string& step : state.display_interactions)
  {
    ImGui::BulletText("%s", step.c_str());
  }

  ImGui::End();
}
