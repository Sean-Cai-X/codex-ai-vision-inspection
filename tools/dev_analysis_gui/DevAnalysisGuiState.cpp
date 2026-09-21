#include "DevAnalysisGuiState.h"

#include "adapters/DevAnalysisGeomAttachmentDemo.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
std::string TrimTrailingNewlines(std::string text)
{
  while (!text.empty() && (text.back() == '\r' || text.back() == '\n'))
  {
    text.pop_back();
  }
  return text;
}

std::string ExtractToken(const std::string& text, const std::initializer_list<std::string>& keys)
{
  for (const std::string& key : keys)
  {
    const std::size_t start = text.find(key);
    if (start == std::string::npos)
    {
      continue;
    }

    std::size_t index = start + key.size();
    while (index < text.size() &&
           (text[index] == ' ' || text[index] == '"' || text[index] == ':' || text[index] == '='))
    {
      ++index;
    }

    std::size_t end = index;
    while (end < text.size() && text[end] != '\r' && text[end] != '\n' && text[end] != ',' && text[end] != '"')
    {
      ++end;
    }

    if (end > index)
    {
      return text.substr(index, end - index);
    }
  }

  return "missing";
}

int ExtractInteger(const std::string& text, const std::initializer_list<std::string>& keys)
{
  for (const std::string& key : keys)
  {
    const std::size_t start = text.find(key);
    if (start == std::string::npos)
    {
      continue;
    }

    std::size_t index = start + key.size();
    while (index < text.size() &&
           (text[index] == ' ' || text[index] == '"' || text[index] == ':' || text[index] == '='))
    {
      ++index;
    }

    std::size_t end = index;
    while (end < text.size() && std::isdigit(static_cast<unsigned char>(text[end])) != 0)
    {
      ++end;
    }

    if (end > index)
    {
      return std::stoi(text.substr(index, end - index));
    }
  }

  return 0;
}

std::string RunPowerShellCapture(const std::string& command_text)
{
  const std::string command =
    "powershell -NoProfile -Command \"" + command_text + "\"";

  FILE* pipe = _popen(command.c_str(), "r");
  if (pipe == nullptr)
  {
    return {};
  }

  std::string output;
  std::array<char, 256> chunk{};
  while (fgets(chunk.data(), static_cast<int>(chunk.size()), pipe) != nullptr)
  {
    output += chunk.data();
  }
  _pclose(pipe);
  return TrimTrailingNewlines(output);
}

std::string ReadSemanticActionMap()
{
  const std::string body =
    "{\\\"jsonrpc\\\":\\\"2.0\\\",\\\"id\\\":\\\"dev-analysis-gui-actions\\\","
    "\\\"method\\\":\\\"tools/call\\\",\\\"params\\\":{\\\"name\\\":\\\"semantic_action_map\\\",\\\"arguments\\\":{}}}";
  return RunPowerShellCapture(
    "$headers=@{}; "
    "$headers['X-Trigger']='manual'; "
    "$headers['X-Source-Thread']='dev_analysis_gui'; "
    "$body='" + body + "'; "
    "$response=Invoke-WebRequest -UseBasicParsing -Uri 'http://192.168.9.100:18080/mcp' "
    "-Method Post -ContentType 'application/json' -Headers $headers -Body $body; "
    "$response.Content");
}

void AssignBuffer(char* buffer, std::size_t buffer_size, const std::string& text)
{
  if (buffer_size == 0)
  {
    return;
  }

  std::snprintf(buffer, buffer_size, "%s", text.c_str());
  buffer[buffer_size - 1] = '\0';
}

std::vector<DevAnalysisSemanticActionRow> BuildSemanticActionRows()
{
  std::vector<DevAnalysisSemanticActionRow> rows;
  const std::string content = ReadSemanticActionMap();
  const int action_count = ExtractInteger(content, {"\"action_count\":"});

  for (int index = 0; index < action_count && index < 24; ++index)
  {
    const std::string prefix = "\"action_" + std::to_string(index) + "_";
    DevAnalysisSemanticActionRow row;
    row.action_id = ExtractToken(content, {prefix + "action_id\":"});
    row.display_name = ExtractToken(content, {prefix + "display_name\":"});
    row.intent = ExtractToken(content, {prefix + "intent\":", prefix + "description\":"});
    row.required_args = ExtractToken(content, {prefix + "required_args\":", prefix + "arguments_schema\":"});
    row.success_rule = ExtractToken(content, {prefix + "success_rule\":"});
    row.risk_text = ExtractToken(content, {prefix + "risk\":"});
    row.fallback = ExtractToken(content, {prefix + "fallback\":"});

    if (row.action_id == "missing")
    {
      continue;
    }
    if (row.display_name == "missing")
    {
      row.display_name = row.action_id;
    }
    if (row.intent == "missing")
    {
      row.intent = "agent_field_missing";
    }
    if (row.required_args == "missing")
    {
      row.required_args = "agent_field_missing";
    }
    if (row.success_rule == "missing")
    {
      row.success_rule = "agent_field_missing";
    }
    if (row.risk_text == "missing")
    {
      row.risk_text = "agent_field_missing";
    }
    if (row.fallback == "missing")
    {
      row.fallback = "agent_field_missing";
    }
    rows.push_back(row);
  }

  if (rows.empty())
  {
    rows.push_back({
      "agent_action_catalog_pending",
      "agent_action_catalog_pending",
      "semantic_action_map tool returned no actions",
      "agent_field_missing",
      "agent_field_missing",
      "agent_field_missing",
      "check codex_lan_agent semantic_action_map on the remote MCP endpoint"
    });
  }

  return rows;
}

std::vector<DevAnalysisAiCapabilityRow> BuildAiCapabilityRows()
{
  std::vector<DevAnalysisAiCapabilityRow> rows;
  const std::filesystem::path root("D:/Codex-WorkDir/Sean_WorkDir/docs/notes/ai_thread_messages");
  if (std::filesystem::exists(root))
  {
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(root))
    {
      if (entry.is_regular_file() && entry.path().extension() == ".jsonl")
      {
        files.push_back(entry.path());
      }
    }
    std::sort(files.begin(), files.end(), std::greater<std::filesystem::path>());

    for (const auto& file_path : files)
    {
      std::ifstream input(file_path);
      std::string line;
      while (std::getline(input, line))
      {
        const bool looks_like_capability_record =
          line.find("capability_level") != std::string::npos ||
          line.find("ai_capability_level") != std::string::npos ||
          line.find("supervisor_ready") != std::string::npos ||
          line.find("management_mode") != std::string::npos ||
          line.find("fusion") != std::string::npos;
        if (!looks_like_capability_record)
        {
          continue;
        }

        DevAnalysisAiCapabilityRow row;
        row.task_type = ExtractToken(line, {"\"task_type\":", "\"task\":"});
        row.capability_level = ExtractToken(line, {"\"capability_level\":", "\"ai_capability_level\":", "\"level\":"});
        row.evidence_ref = ExtractToken(line, {"\"evidence_ref\":", "evidence_ref="});
        row.risk_text = ExtractToken(line, {"\"risk\":", "\"risk_text\":", "risk="});
        row.strengthen_thread = ExtractToken(line, {"\"strengthen_thread\":", "\"suggested_thread\":", "\"target_thread\":"});
        row.supervisor_ready = ExtractToken(line, {"\"supervisor_ready\":", "\"can_upgrade_to_supervisor\":", "\"management_mode_ready\":"});
        row.next_step = ExtractToken(line, {"\"next_action\":", "\"next_step\":"});

        if (row.task_type == "missing")
        {
          row.task_type = "fusion capability evaluation";
        }
        if (row.capability_level == "missing")
        {
          row.capability_level = "pending";
        }
        if (row.evidence_ref == "missing")
        {
          row.evidence_ref = file_path.string();
        }
        if (row.risk_text == "missing")
        {
          row.risk_text = "risk field missing in structured record";
        }
        if (row.strengthen_thread == "missing")
        {
          row.strengthen_thread = "fusion-test-thread";
        }
        if (row.supervisor_ready == "missing")
        {
          row.supervisor_ready = "pending";
        }
        if (row.next_step == "missing")
        {
          row.next_step = "write capability_level/evidence_ref/risk/supervisor_ready/next_action";
        }
        rows.push_back(row);
        if (rows.size() >= 8)
        {
          return rows;
        }
      }
    }
  }

  rows.push_back({
    "fusion capability evaluation",
    "pending",
    "D:\\Codex-WorkDir\\Sean_WorkDir\\docs\\notes\\ai_thread_messages\\*.jsonl",
    "no structured capability record found",
    "fusion-test-thread",
    "pending",
    "write a structured AI capability record with evidence_ref before GUI can promote this row"
  });
  return rows;
}

std::vector<DevAnalysisCaseRecord> BuildCasesForChain(DevAnalysisChainId chain_id)
{
  switch (chain_id)
  {
  case DevAnalysisChainId::CxcoreToEnsmallen:
    return {
      {
        "cxcore_circle_measurement_boundary_real_numeric",
        "Boundary-condition circle measurement with real numeric checks and handled fallback reporting.",
        "cxparser/rag_script_cases/cxcore/feature/cxcore_circle_measurement_boundary_real_numeric_cstyle_feature.cxsc",
        "review_cxcore_scripts -> review_ensmallen_scripts",
        "compile(sampling), compile(filter), bridge_enabled, CircleMeasurementOutput, runtime_ms, summary",
        "handled_boundary_condition",
        "Push back if fallback stays enabled but boundary fit remains visually unreasonable."
      },
      {
        "ensmallen_geometry_fit_tuning",
        "Optimization replay around geometry-fit tuning after cxcore measurement export.",
        "cxparser/rag_script_cases/cxcore/feature/ensmallen_layer_geometry_fit_tuning_feature.cxsc",
        "review_cxcore_tests -> review_ensmallen_scripts",
        "fit score, parameter delta, replay stability, summary",
        "none",
        "Push back if best-fit parameters diverge from baseline replay."
      }
    };
  case DevAnalysisChainId::CxcoreToMlpack:
    return {
      {
        "mlpack_handoff_case",
        "Business-facing mlpack handoff for keeping stage semantic, statistics evidence, problem entry, and image evidence on the same visible chain.",
        "cxscript/integration/mlpack/baseline_pair_compare_min.cxscript",
        "cxcore -> mlpack -> dev_analysis_gui :: feature -> distance -> cluster_or_anomaly -> baseline_compare",
        "sample_switch, stage_semantic_ref, statistics_evidence_ref, problem_entry_ref, supporting_image_refs, runtime_fillback_status",
        "review_only",
        "Push back if the issue cannot be assigned to input, feature, threshold, or compare_logic on the same image and same chain."
      },
      {
        "baseline_pair_compare_min",
        "Baseline pair compare replay for same-image, same-chain anomaly observation after cxcore feature export.",
        "cxscript/integration/mlpack/baseline_pair_compare_min.cxscript",
        "cxcore -> mlpack -> baseline_pair_compare",
        "baseline_class_ref, cluster_ref, distance_ref, anomaly_ref, compare summary",
        "pending_runtime_fillback",
        "Push back if compare replay cannot be explained from the same image and same anomaly family."
      }
    };
  case DevAnalysisChainId::CxcoreToTorch:
    return {
      {
        "torch_geometry_contract",
        "Geometry-to-label contract review before torch main model entry.",
        "cxscript/integration/torch_geometry/feature.label_align_contract.cxsc",
        "review_cxcore_scripts -> review_torch_tests",
        "label contract, sample alignment, blocker summary",
        "pending",
        "Push back if label alignment is marked ok but the observed geometry anchor is inconsistent."
      },
      {
        "torch_yolo_min_train",
        "Minimal train readiness case before full model execution.",
        "cxscript/module/torch/train.yolo_min_train.cxs",
        "review_torch_tests -> review_torch_programs",
        "sample_count, train readiness, summary",
        "pending",
        "Push back if pre-train state remains incomplete after contract checks pass."
      }
    };
  }

  return {};
}

DevAnalysisMetricSnapshot BuildMetricsForChain(DevAnalysisChainId chain_id)
{
  DevAnalysisMetricSnapshot metrics;
  switch (chain_id)
  {
  case DevAnalysisChainId::CxcoreToEnsmallen:
    metrics.elapsed_ms = 18.4f;
    metrics.memory_mb = 96.0f;
    metrics.key_fields = "roi_count=2, fit_score=0.91, param_delta=0.08";
    metrics.blocking_point = "Need side-by-side replay of baseline and best-fit parameters.";
    break;
  case DevAnalysisChainId::CxcoreToMlpack:
    metrics.elapsed_ms = 26.7f;
    metrics.memory_mb = 128.0f;
    metrics.key_fields = "sample_switch=G1.semantic_handoff, stage_semantic=feature->distance->cluster_or_anomaly->baseline_compare, problem_entry=input|feature|threshold|compare_logic";
    metrics.blocking_point = "Judge whether the current issue starts from input image, feature bucket, threshold gate, or compare logic before changing the baseline flow.";
    break;
  case DevAnalysisChainId::CxcoreToTorch:
    metrics.elapsed_ms = 41.2f;
    metrics.memory_mb = 256.0f;
    metrics.key_fields = "sample_count=8, label_state=aligned, infer_ready=pending";
    metrics.blocking_point = "Need smoke confirmation before enabling full train or infer handoff.";
    break;
  }
  return metrics;
}

std::vector<DevAnalysisEntryRecord> BuildEntriesForChain(DevAnalysisChainId chain_id)
{
  switch (chain_id)
  {
  case DevAnalysisChainId::CxcoreToEnsmallen:
    return {
      {
        "cxcore -> ensmallen",
        "review_cxcore_tests -> review_ensmallen_scripts",
        "Read geometric inputs, compare replay and optimization outputs, and present fit deltas.",
        "Input mapping is readable and optimization replay is ready for human validation.",
        "Continue with the smallest replay case and confirm tuning stability."
      }
    };
  case DevAnalysisChainId::CxcoreToMlpack:
    return {
      {
        "cxcore -> mlpack",
        "cxparser_ext_cxscript_cli --script cxscript/integration/mlpack/baseline_pair_compare_min.cxscript",
        "Run the stable baseline compare script, then bind stage semantic, problem entry, and supporting images into the GUI result package.",
        "The public runtime and GUI capture chain are ready; the next value is to make issue bucketing and stage evidence explicit for human judgment.",
        "Keep one image, one main chain, and one issue bucket in focus before escalating to manual review."
      }
    };
  case DevAnalysisChainId::CxcoreToTorch:
    return {
      {
        "cxcore -> torch",
        "review_cxcore_scripts -> review_torch_tests -> review_torch_programs",
        "Expose the deep-model input handoff, pre-train state, and pre-infer readiness.",
        "The input contract looks shaped correctly, while full model entry remains to be smoke-verified.",
        "Continue with build_torch_tests for the smallest confirmation path."
      }
    };
  }

  return {};
}

DevAnalysisRemoteStatus BuildRemoteStatusForChain(DevAnalysisChainId chain_id,
                                                  const DevAnalysisCaseRecord& selected_case)
{
  DevAnalysisRemoteStatus status;
  switch (chain_id)
  {
  case DevAnalysisChainId::CxcoreToEnsmallen:
    status.entry =
      "healthz -> upload-file/download-file -> run-cli-profile(check_build_dir) -> run-cli-profile(run_case) -> download-file(log)";
    status.action =
      "Use the remote LAN agent to preview the cxcore boundary case, then read back the case log before deciding whether a heavier configure/build step is needed.";
    status.result =
      "Remote case preview is already represented by run_case_20260331_160156.log and confirms the boundary numeric script path, compile(sampling/filter), fallback checks, and summary fields.";
    status.next_step =
      "Run remote check_build_dir first, then prepare_build_dir and configure_project only if the scratch state still blocks build_target.";
    status.log_path = "codex-lan-agent/logs/run_case_20260331_160156.log";
    status.blocker =
      "Existing remote build logs show CMakeScratch cleanup failure under build_review_vs, so GUI should surface this blocker before offering heavy build actions.";
    if (selected_case.name == "cxcore_circle_measurement_boundary_real_numeric")
    {
      status.action =
        "Preview the remote boundary numeric circle case, confirm handled_boundary_condition and fallback summary from the downloaded log, then decide whether to escalate into tuning/build.";
      status.result =
        "Remote log preview shows the exact case path and expected checks for CircleMeasurementOutput, handled_boundary_condition, runtime_ms, and summary.";
    }
    break;
  case DevAnalysisChainId::CxcoreToMlpack:
    status.entry =
      "cxparser_ext_cxscript_cli --script cxscript/integration/mlpack/baseline_pair_compare_min.cxscript -> result.json -> dev_analysis_gui_shell --chain cxcore_to_mlpack --case mlpack_handoff_case --load-result <result.json> --capture-dir <dir>";
    status.action =
      "Run the stable baseline compare script, reuse the same-image result package, and let GUI capture screenshots for stage-based human review.";
    status.result =
      "Remote public runtime already passed for baseline_pair_compare_min, and GUI capture already produced four ppm screenshots plus result.json in cxparser/artifacts/gui_visual_review/mlpack/mlpack_handoff_case/.";
    status.next_step =
      "Make stage semantic, supporting images, and problem-entry bucketing first-class fields so AI review and human review can cut from the same issue entrance.";
    status.log_path = "cxparser/artifacts/gui_visual_review/mlpack/mlpack_handoff_case/result.json";
    status.blocker =
      "The remaining gap is semantic clarity: issue bucketing still needs to be expressed as input, feature, threshold, or compare_logic rather than as raw score-only output.";
    break;
  case DevAnalysisChainId::CxcoreToTorch:
    status.entry =
      "healthz -> upload-file/download-file -> run-cli-profile(check_build_dir) -> enqueue-cli-profile(build_target or tests) -> tasks/<task_id> -> download-file(log)";
    status.action =
      "For torch handoff on remote, stay with queue-based light checks first and only move to heavier build or test after log-backed readiness confirmation.";
    status.result =
      "Async queue support is available by process definition, but no torch-specific downloaded remote task log is attached in the current workspace snapshot.";
    status.next_step =
      "Use a light remote torch profile first, then poll task status and bind the final log result into the GUI.";
    status.log_path = "codex-lan-agent/logs/<remote_torch_task>.log";
    status.blocker =
      "This chain still lacks a concrete remote task log, so GUI should show the remote status as pending evidence rather than inferred ready.";
    break;
  }

  return status;
}

std::string BuildRuntimeCommand(const DevAnalysisCaseRecord& selected_case)
{
  if (selected_case.script_path.empty())
  {
    return "cxparser_ext_cxscript_cli --script <pending_script_path>";
  }
  return "cxparser_ext_cxscript_cli --script " + selected_case.script_path;
}

std::string BuildTestImageRef(DevAnalysisChainId chain_id)
{
  switch (chain_id)
  {
  case DevAnalysisChainId::CxcoreToEnsmallen:
    return "cxcore boundary numeric circle sample / ROI replay bundle";
  case DevAnalysisChainId::CxcoreToMlpack:
    return "ELPV defect-normal + HALCON pcb anomaly + texture scratch sample switch bundle";
  case DevAnalysisChainId::CxcoreToTorch:
    return "geometry label alignment or YOLO readiness sample bundle";
  }
  return "case-linked sample bundle";
}

std::string BuildGeometrySettingRef(DevAnalysisChainId chain_id)
{
  switch (chain_id)
  {
  case DevAnalysisChainId::CxcoreToEnsmallen:
    return "Circle ROI, boundary sampling, fit tolerance, replay delta gate";
  case DevAnalysisChainId::CxcoreToMlpack:
    return "Sample switch, feature bucket, issue entry, and same-image baseline compare gate";
  case DevAnalysisChainId::CxcoreToTorch:
    return "Geometry anchor, label alignment window, sample route gate";
  }
  return "geometry gate pending";
}

std::string BuildAcquisitionModeRef(DevAnalysisChainId chain_id)
{
  switch (chain_id)
  {
  case DevAnalysisChainId::CxcoreToEnsmallen:
    return "Remote log replay + measured ROI re-read + parameter retune";
  case DevAnalysisChainId::CxcoreToMlpack:
    return "Prepared result package + GUI load-result capture replay";
  case DevAnalysisChainId::CxcoreToTorch:
    return "Remote queue probe + sample contract replay + staged task log readback";
  }
  return "remote sample replay";
}

std::string BuildIssueKindRef(const DevAnalysisCaseRecord& selected_case,
                              const DevAnalysisRemoteStatus& remote_status)
{
  if (!selected_case.expected_failure_mode.empty() && selected_case.expected_failure_mode != "none")
  {
    return selected_case.expected_failure_mode;
  }
  if (!remote_status.blocker.empty())
  {
    return remote_status.blocker;
  }
  return "none";
}

std::string BuildCurrentStatusRef(const DevAnalysisCaseRecord& selected_case,
                                  const DevAnalysisRemoteStatus& remote_status)
{
  if (remote_status.result.find("lacks") != std::string::npos ||
      remote_status.result.find("pending") != std::string::npos)
  {
    return "pending_remote_evidence";
  }
  if (!selected_case.expected_failure_mode.empty() && selected_case.expected_failure_mode != "none")
  {
    return "watch_" + selected_case.expected_failure_mode;
  }
  return "ready_for_semantic_step_run";
}

std::string BuildRemediationPriorityRef(DevAnalysisChainId chain_id)
{
  switch (chain_id)
  {
  case DevAnalysisChainId::CxcoreToEnsmallen:
    return "Judge fallback geometry first, then retune parameter injection, then escalate build or optimization replay";
  case DevAnalysisChainId::CxcoreToMlpack:
    return "Judge input, feature, threshold, or compare_logic first, then verify stage evidence, then continue GUI replay";
  case DevAnalysisChainId::CxcoreToTorch:
    return "Confirm geometry-label contract first, then adjust sample route, then promote queue-backed model run";
  }
  return "script first, parameter second";
}

void RefreshSemanticOperationState(DevAnalysisState& state,
                                   const DevAnalysisCaseRecord& selected_case)
{
  state.operation_select.script_module_ref = BuildRuntimeCommand(selected_case);
  state.operation_select.task_case_ref = selected_case.name;
  state.operation_select.sample_switch_ref =
    state.selected_chain == DevAnalysisChainId::CxcoreToMlpack
      ? std::string("ELPV:G1.semantic_handoff -> HALCON:pcb_anomaly -> texture:surface_scratch")
      : std::string("sample switch not specialized for this chain");
  state.operation_select.test_image_ref = BuildTestImageRef(state.selected_chain);
  state.operation_select.primary_chain_ref = selected_case.entry_path;
  state.operation_select.script_action_ref =
    state.selected_action_id.empty()
      ? std::string("Parser Run / SetParserValue / Image Viewer semantics")
      : (state.selected_action_id + " -> " + state.selected_action_intent);

  state.operation_adjust.parameter_injection_ref =
    "SetParserValue / GetParserValue semantics -> inject threshold, ROI, fit, or route parameters into the selected script module";
  state.operation_adjust.geometry_setting_ref = BuildGeometrySettingRef(state.selected_chain);
  state.operation_adjust.acquisition_mode_ref = BuildAcquisitionModeRef(state.selected_chain);
  state.operation_adjust.runtime_arg_ref = BuildRuntimeCommand(selected_case);

  state.operation_run.step_run_ref =
    state.selected_action_id.empty() ? std::string("single step action pending") : state.selected_action_id;
  state.operation_run.segment_run_ref = selected_case.entry_path;
  state.operation_run.full_chain_run_ref = state.remote_status.entry;
  state.operation_run.replay_run_ref = state.remote_status.log_path;
  state.operation_run.trace_run_ref = selected_case.script_path + " -> " + state.remote_status.log_path;

  state.operation_observe.input_image_ref = state.operation_select.test_image_ref;
  state.operation_observe.element_layer_ref =
    "Input / ROI / edge / element / match overlays observed in the same work surface";
  state.operation_observe.supporting_image_refs_ref =
    state.selected_chain == DevAnalysisChainId::CxcoreToMlpack
      ? std::string("supporting_image_refs -> test_image | compare_image | anomaly_focus_image | roi evidence image")
      : std::string("supporting images not specialized for this chain");
  state.operation_observe.statistics_evidence_ref =
    state.selected_chain == DevAnalysisChainId::CxcoreToMlpack
      ? std::string("statistics_evidence_ref -> baseline_class_ref | cluster_ref | distance_ref | anomaly_ref")
      : std::string("statistics evidence not specialized for this chain");
  state.operation_observe.baseline_result_ref =
    state.selected_chain == DevAnalysisChainId::CxcoreToMlpack
      ? std::string("baseline_result_ref -> baseline_pair_compare_ready + review boards + roi evidence")
      : std::string("baseline result not specialized for this chain");
  state.operation_observe.stage_semantic_ref =
    state.selected_chain == DevAnalysisChainId::CxcoreToMlpack
      ? std::string("stage_semantic_ref -> feature.prepare -> distance_or_cluster_or_anomaly -> baseline_compare -> manual_review_entry")
      : std::string("stage semantic not specialized for this chain");
  state.operation_observe.problem_entry_ref =
    state.selected_chain == DevAnalysisChainId::CxcoreToMlpack
      ? std::string("problem_entry_ref -> input | feature | threshold | compare_logic")
      : std::string("problem entry not specialized for this chain");
  state.operation_observe.evidence_ref =
    state.selected_chain == DevAnalysisChainId::CxcoreToMlpack
      ? std::string("evidence_ref -> cxparser/artifacts/mlpack_semantic_refs_review/mlpack_gui_semantic_operation_packet_20260623v1.html")
      : state.remote_status.log_path;
  state.operation_observe.result_ref =
    state.selected_chain == DevAnalysisChainId::CxcoreToMlpack
      ? std::string("result_ref -> cxparser/artifacts/gui_visual_review/mlpack/mlpack_handoff_case/result.json")
      : state.remote_status.log_path;
  state.operation_observe.main_chain_ref = selected_case.entry_path;
  state.operation_observe.stage_ref = DevAnalysisStageLabel(state.selected_stage);
  state.operation_observe.conclusion_ref = state.metrics.key_fields;
  state.operation_observe.anomaly_ref =
    state.remote_status.blocker.empty() ? state.metrics.blocking_point : state.remote_status.blocker;

  state.operation_judge.current_status_ref = BuildCurrentStatusRef(selected_case, state.remote_status);
  state.operation_judge.issue_kind_ref = BuildIssueKindRef(selected_case, state.remote_status);
  state.operation_judge.remediation_priority_ref = BuildRemediationPriorityRef(state.selected_chain);
  state.operation_judge.manual_review_gate_ref = selected_case.pushback_hint;

  state.operation_record.runtime_fillback_status_ref =
    state.selected_chain == DevAnalysisChainId::CxcoreToMlpack
      ? std::string("capture_ready_for_ai_and_human_review")
      : std::string("runtime_fillback_not_applicable");
  state.operation_record.human_note_ref =
    "Record whether script semantics, parameter injection, and result trace stay aligned after replay.";
  state.operation_record.issue_entry_ref =
    state.selected_chain == DevAnalysisChainId::CxcoreToMlpack
      ? state.operation_observe.problem_entry_ref
      : state.metrics.blocking_point;
  state.operation_record.next_step_ref = state.remote_status.next_step;
  state.operation_record.replay_ref = state.remote_status.log_path;

  AssignBuffer(state.parameter_injection_buffer.data(),
               state.parameter_injection_buffer.size(),
               state.operation_adjust.parameter_injection_ref);
  AssignBuffer(state.geometry_setting_buffer.data(),
               state.geometry_setting_buffer.size(),
               state.operation_adjust.geometry_setting_ref);
  AssignBuffer(state.acquisition_mode_buffer.data(),
               state.acquisition_mode_buffer.size(),
               state.operation_adjust.acquisition_mode_ref);
  AssignBuffer(state.runtime_arg_buffer.data(),
               state.runtime_arg_buffer.size(),
               state.operation_adjust.runtime_arg_ref);
  AssignBuffer(state.human_note_buffer.data(),
               state.human_note_buffer.size(),
               state.operation_record.human_note_ref);
  AssignBuffer(state.issue_entry_buffer.data(),
               state.issue_entry_buffer.size(),
               state.operation_record.issue_entry_ref);
  AssignBuffer(state.next_step_buffer.data(),
               state.next_step_buffer.size(),
               state.operation_record.next_step_ref);
}
}

const char* DevAnalysisChainLabel(DevAnalysisChainId chain_id)
{
  switch (chain_id)
  {
  case DevAnalysisChainId::CxcoreToEnsmallen:
    return "cxcore -> ensmallen";
  case DevAnalysisChainId::CxcoreToMlpack:
    return "cxcore -> mlpack";
  case DevAnalysisChainId::CxcoreToTorch:
    return "cxcore -> torch";
  }
  return "unknown";
}

const char* DevAnalysisStageLabel(DevAnalysisStageId stage_id)
{
  switch (stage_id)
  {
  case DevAnalysisStageId::Preprocess:
    return "Preprocess";
  case DevAnalysisStageId::MainAlgorithm:
    return "Main Algorithm";
  case DevAnalysisStageId::Postprocess:
    return "Postprocess";
  case DevAnalysisStageId::LearningInput:
    return "Learning Input";
  case DevAnalysisStageId::DeepModelInput:
    return "Deep Model Input";
  case DevAnalysisStageId::TrainOrInferReady:
    return "Train / Infer Ready";
  }
  return "unknown";
}

const char* DevAnalysisJudgmentLabel(DevAnalysisJudgment judgment)
{
  switch (judgment)
  {
  case DevAnalysisJudgment::ContinueAdvance:
    return "Continue Advance";
  case DevAnalysisJudgment::Questionable:
    return "Questionable";
  case DevAnalysisJudgment::PushBackThread:
    return "Push Back Thread";
  }
  return "unknown";
}

void InitializeDevAnalysisState(DevAnalysisState& state)
{
  state.cases = BuildCasesForChain(state.selected_chain);
  state.selected_case_index = 0;
  state.show_overlay = true;
  state.show_summary = true;
  state.show_guidance = true;
  state.show_labels = true;
  state.show_anchor_lines = true;
  state.selected_stage = DevAnalysisStageId::Preprocess;
  state.judgment = DevAnalysisJudgment::ContinueAdvance;
  RefreshDevAnalysisChainData(state);
}

void RefreshDevAnalysisChainData(DevAnalysisState& state)
{
  const std::vector<DevAnalysisCaseRecord> cases_for_chain = BuildCasesForChain(state.selected_chain);
  if (cases_for_chain.empty())
  {
    InitializeDevAnalysisState(state);
    return;
  }

  const std::string previous_case_name =
    (state.selected_case_index >= 0 && state.selected_case_index < static_cast<int>(state.cases.size()))
      ? state.cases[static_cast<std::size_t>(state.selected_case_index)].name
      : std::string();

  state.cases = cases_for_chain;
  state.selected_case_index = 0;
  for (int index = 0; index < static_cast<int>(state.cases.size()); ++index)
  {
    if (state.cases[static_cast<std::size_t>(index)].name == previous_case_name)
    {
      state.selected_case_index = index;
      break;
    }
  }

  if (state.selected_case_index < 0 || state.selected_case_index >= static_cast<int>(state.cases.size()))
  {
    state.selected_case_index = 0;
  }

  const DevAnalysisCaseRecord& selected_case =
    state.cases[static_cast<std::size_t>(state.selected_case_index)];
  state.selected_case_script_path = selected_case.script_path;
  state.selected_case_entry_path = selected_case.entry_path;
  state.selected_case_checks = selected_case.key_checks;
  state.selected_case_failure_mode = selected_case.expected_failure_mode;
  state.selected_case_pushback_hint = selected_case.pushback_hint;

  state.entries = BuildEntriesForChain(state.selected_chain);
  state.metrics = BuildMetricsForChain(state.selected_chain);

  if (state.selected_chain == DevAnalysisChainId::CxcoreToEnsmallen &&
      selected_case.name == "cxcore_circle_measurement_boundary_real_numeric")
  {
    state.selected_stage = DevAnalysisStageId::Postprocess;
    state.metrics.elapsed_ms = 12.8f;
    state.metrics.memory_mb = 84.0f;
    state.metrics.key_fields =
      "result_object=CircleMeasurementOutput, failure_mode=handled_boundary_condition, bridge_enabled=true";
    state.metrics.blocking_point =
      "Need human confirmation that fallback geometry is still visually acceptable on boundary input.";
    state.entries = {
      {
        "cxcore -> ensmallen",
        "review_cxcore_scripts -> review_ensmallen_scripts",
        "Visualize circle boundary measurement output, compile sampling/filter stages, and inspect fallback summary.",
        "sampling and filter stages compile, CircleMeasurementOutput is produced, and boundary failure mode is handled.",
        "Compare fallback circle overlay with the measured boundary before tuning is promoted."
      }
    };
  }

  state.remote_status = BuildRemoteStatusForChain(state.selected_chain, selected_case);

  const DevAnalysisGeomAttachmentScene scene = BuildDevAnalysisGeomAttachmentScene(state.selected_chain);
  state.selected_entity_id = scene.elements.empty() ? 0 : scene.elements.front().entity_id;
  state.display_attachment_summary = scene.attachment_rule;
  state.display_interaction_summary = scene.interaction_rule;
  state.display_layers = {
    "L0 Element body: entity / kind / bbox / stage",
    "L1 Anchor layer: entity_id -> anchor point",
    "L2 Label layer: text and summary attached by target_entity_id",
    "L3 Interaction layer: hover / select / judgment overlays"
  };
  state.display_interactions = {
    "Pick element by entity_id",
    "Resolve anchor and leader line",
    "Open summary and current blocker",
    "Apply continue / questionable / push-back judgment"
  };

  state.ai_capability_rows = BuildAiCapabilityRows();
  state.semantic_action_rows = BuildSemanticActionRows();
  if (!state.semantic_action_rows.empty())
  {
    if (state.selected_action_index < 0 ||
        state.selected_action_index >= static_cast<int>(state.semantic_action_rows.size()))
    {
      state.selected_action_index = 0;
    }
    state.selected_action_id =
      state.semantic_action_rows[static_cast<std::size_t>(state.selected_action_index)].action_id;
    state.selected_action_intent =
      state.semantic_action_rows[static_cast<std::size_t>(state.selected_action_index)].intent;
  }
  else
  {
    state.selected_action_index = -1;
    state.selected_action_id.clear();
    state.selected_action_intent.clear();
  }

  RefreshSemanticOperationState(state, selected_case);
}
