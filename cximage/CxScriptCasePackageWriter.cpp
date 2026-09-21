#include "CxScriptCasePackageWriter.h"
#include "CxUnifiedLog.h"
#include "ManualConsoleGauge.h"
#include "ManualConsoleUtils.h"
#include "pch.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {
std::string SafePathComponentForCandidate(const std::string &value) {
  std::string out;
  out.reserve(value.size());
  for (unsigned char ch : value) {
    if (std::isalnum(ch) || ch == '-' || ch == '_')
      out.push_back(static_cast<char>(ch));
    else
      out.push_back('_');
  }
  while (out.find("..") != std::string::npos)
    out.replace(out.find(".."), 2, "__");
  return out;
}

std::string StableEvidenceCaseIdFromSelection(
    const CxEvidenceSelectionSnapshot &selection) {
  if (!selection.case_id.empty())
    return selection.case_id;

  std::string scriptKey = selection.script_id;
  if (scriptKey.empty() && !selection.script_path.empty())
    scriptKey = std::filesystem::path(selection.script_path).stem().string();
  if (scriptKey.empty() && !selection.source_evidence_script_path.empty())
    scriptKey = std::filesystem::path(selection.source_evidence_script_path)
                    .stem()
                    .string();

  std::ostringstream key;
  if (!scriptKey.empty())
    key << scriptKey;
  if (!selection.image_id.empty())
    key << (key.tellp() > 0 ? "__" : "") << selection.image_id;
  if (!selection.target_id.empty())
    key << (key.tellp() > 0 ? "__" : "") << selection.target_id;

  return key.str();
}

std::string LowerCandidateIdentityKey(std::string value) {
  std::transform(
      value.begin(), value.end(), value.begin(),
      [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return value;
}

bool LooksLikeFindEllipseCandidate(
    const std::string &tool, const std::string &caseId,
    const std::string &scriptId, const std::string &scriptPath,
    const std::string &sourceEvidenceScriptPath) {
  if (tool == "FindEllipse" || tool == "Findellipse")
    return true;

  const std::string key =
      LowerCandidateIdentityKey(caseId + " " + scriptId + " " + scriptPath +
                                " " + sourceEvidenceScriptPath);
  return key.find("findellipse") != std::string::npos ||
         key.find("find_ellipse") != std::string::npos;
}

std::string StripEvidenceCandidateDisplaySuffix(std::string value) {
  const std::string marker = " [candidate_";
  std::size_t pos = value.find(marker);
  while (pos != std::string::npos && value.find(']', pos) != std::string::npos) {
    value.erase(pos);
    pos = value.find(marker);
  }
  return value;
}

std::string NormalizeEvidenceSourceForComparison(const std::string &pathOrId) {
  if (pathOrId.empty())
    return {};
  return ResolveWorkspaceFile(pathOrId).lexically_normal().string();
}


void SanitizeFindEllipseCandidateGlobals(
    const std::string &tool, const std::string &caseId,
    const std::string &scriptId, const std::string &scriptPath,
    const std::string &sourceEvidenceScriptPath,
    std::vector<std::pair<std::string, int>> &vars) {
  if (!LooksLikeFindEllipseCandidate(tool, caseId, scriptId, scriptPath,
                                     sourceEvidenceScriptPath)) {
    return;
  }

  auto setOrClamp = [&vars](const std::string &name, int minValue, int maxValue,
                            int defaultValue) {
    for (auto &kv : vars) {
      if (kv.first == name) {
        kv.second = std::max(minValue, std::min(maxValue, kv.second));
        return;
      }
    }
    vars.emplace_back(name,
                      std::max(minValue, std::min(maxValue, defaultValue)));
  };

  setOrClamp("global_findellipse_edge_count", 1, 3, 3);
  setOrClamp("global_findellipse_selected_edge", -1, 3, 0);
  setOrClamp("global_findellipse_best_edge", 0, 3, 0);
  setOrClamp("global_findellipse_recommended_edge", 0, 3, 0);
  setOrClamp("global_findellipse_relation_edge", 0, 3, 0);
  setOrClamp("global_findellipse_attach_edge", 0, 3, 0);
}

std::filesystem::path
CanonicalEvidenceCandidateRoot(const std::string &configuredRoot,
                               bool *outCanonicalized = nullptr) {
  std::filesystem::path root = ResolveCxVisionRunPath(configuredRoot);
  const std::string generic = root.generic_string();
  const bool isBuildReleaseEvidenceRoot =
      generic.find("/Release/cxscript_runs/evidence_candidates") !=
          std::string::npos ||
      generic.find("/Debug/cxscript_runs/evidence_candidates") !=
          std::string::npos ||
      generic.find("/RelWithDebInfo/cxscript_runs/evidence_candidates") !=
          std::string::npos ||
      generic.find("/MinSizeRel/cxscript_runs/evidence_candidates") !=
          std::string::npos;

  if (isBuildReleaseEvidenceRoot) {
    if (outCanonicalized)
      *outCanonicalized = true;
    return ResolveCxVisionRunPath("cxscript_runs/evidence_candidates");
  }

  if (outCanonicalized)
    *outCanonicalized = false;
  return root;
}

void ReplaceAllLocal(std::string &text, const std::string &from,
                     const std::string &to) {
  if (from.empty())
    return;
  std::size_t pos = 0;
  while ((pos = text.find(from, pos)) != std::string::npos) {
    text.replace(pos, from.size(), to);
    pos += to.size();
  }
}

std::string NormalizeFindSegmentationPromptCallOrderForSnapshot(
    const std::string &scriptText, bool *outChanged) {
  std::string normalized = scriptText;
  const std::string reversedPositive =
      "m_seg.setpositivepointxy(global_segmentation_positive_y, "
      "global_segmentation_positive_x);";
  const std::string canonicalPositive =
      "m_seg.setpositivepointxy(global_segmentation_positive_x, "
      "global_segmentation_positive_y);";
  const std::string reversedNegative =
      "m_seg.setnegativepointxy(global_segmentation_negative_y, "
      "global_segmentation_negative_x);";
  const std::string canonicalNegative =
      "m_seg.setnegativepointxy(global_segmentation_negative_x, "
      "global_segmentation_negative_y);";

  const bool hadReversed =
      normalized.find(reversedPositive) != std::string::npos ||
      normalized.find(reversedNegative) != std::string::npos;
  ReplaceAllLocal(normalized, reversedPositive, canonicalPositive);
  ReplaceAllLocal(normalized, reversedNegative, canonicalNegative);
  if (outChanged)
    *outChanged = hadReversed && normalized != scriptText;
  return normalized;
}

void AppendEvidenceCandidateStateProbeImpl(const ManualTestContext &context,
                                           const std::string &candidateDir,
                                           const std::string &candidateId,
                                           const std::string &phase,
                                           const std::string &status,
                                           const std::string &reason) {
  if (candidateDir.empty())
    return;

  const ManualGaugeState &g = context.current_gauge;
  auto globalValue = [&](const char *key) -> int {
    const auto it = context.runtime_int_vars.find(key);
    return it == context.runtime_int_vars.end() ? -2147483647 : it->second;
  };

  std::ostringstream line;
  line << "{"
       << "\"candidate_id\":\"" << JsonEscape(candidateId) << "\","
       << "\"phase\":\"" << JsonEscape(phase) << "\","
       << "\"status\":\"" << JsonEscape(status) << "\","
       << "\"reason\":\"" << JsonEscape(reason) << "\","
       << "\"selected_is_candidate\":"
       << (context.current_evidence_selection.is_candidate ? "true" : "false")
       << ","
       << "\"selected_candidate_id\":\""
       << JsonEscape(context.current_evidence_selection.candidate_id) << "\","
       << "\"script_path\":\"" << JsonEscape(context.loaded_script_path)
       << "\","
       << "\"gauge_source\":\"" << JsonEscape(g.source) << "\","
       << "\"gauge_dirty\":" << (g.dirty ? "true" : "false") << ","
       << "\"key_parameter_edit_revision\":"
       << context.key_parameter_edit_revision << ","
       << "\"last_key_parameter_edit_summary\":\""
       << JsonEscape(context.last_key_parameter_edit_summary) << "\","
       << "\"gauge\":{"
       << "\"x0\":" << g.line_x0 << ",\"y0\":" << g.line_y0
       << ",\"x1\":" << g.line_x1 << ",\"y1\":" << g.line_y1
       << ",\"half_width\":" << g.tool_half_width
       << ",\"threshold\":" << g.threshold << ",\"method\":" << g.method
       << ",\"linegap\":" << g.linegap << ",\"wgap\":" << g.wgap
       << ",\"min_edge_run_width_px\":" << g.min_edge_run_width_px
       << ",\"hgap\":" << g.hgap << ",\"filterprofile\":" << g.filterprofile
       << "},"
       << "\"globals\":{"
       << "\"global_roi_x0\":" << globalValue("global_roi_x0")
       << ",\"global_roi_y0\":" << globalValue("global_roi_y0")
       << ",\"global_roi_x1\":" << globalValue("global_roi_x1")
       << ",\"global_roi_y1\":" << globalValue("global_roi_y1")
       << ",\"global_tool_half_width\":"
       << globalValue("global_tool_half_width")
       << ",\"global_threshold\":" << globalValue("global_threshold")
       << ",\"global_method\":" << globalValue("global_method")
       << ",\"global_linegap\":" << globalValue("global_linegap")
       << ",\"global_min_edge_run_width_px\":"
       << globalValue("global_min_edge_run_width_px")
       << ",\"global_wgap\":" << globalValue("global_wgap")
       << ",\"global_hgap\":" << globalValue("global_hgap")
       << ",\"global_filterprofile\":" << globalValue("global_filterprofile")
       << ",\"global_findline_edge_count\":"
       << globalValue("global_findline_edge_count")
       << ",\"global_findline_selected_edge\":"
       << globalValue("global_findline_selected_edge")
       << ",\"global_findline_best_edge\":"
       << globalValue("global_findline_best_edge")
       << ",\"global_findline_recommended_edge\":"
       << globalValue("global_findline_recommended_edge")
       << ",\"global_findline_relation_edge\":"
       << globalValue("global_findline_relation_edge")
       << ",\"global_findline_attach_edge\":"
       << globalValue("global_findline_attach_edge")
       << ",\"global_segmentation_mode\":"
       << globalValue("global_segmentation_mode")
       << ",\"global_segmentation_threshold_percent\":"
       << globalValue("global_segmentation_threshold_percent")
       << ",\"global_segmentation_positive_enabled\":"
       << globalValue("global_segmentation_positive_enabled")
       << ",\"global_segmentation_positive_x\":"
       << globalValue("global_segmentation_positive_x")
       << ",\"global_segmentation_positive_y\":"
       << globalValue("global_segmentation_positive_y")
       << ",\"global_segmentation_negative_enabled\":"
       << globalValue("global_segmentation_negative_enabled")
       << ",\"global_segmentation_negative_x\":"
       << globalValue("global_segmentation_negative_x")
       << ",\"global_segmentation_negative_y\":"
       << globalValue("global_segmentation_negative_y") << "}}\n";

  const std::filesystem::path path =
      std::filesystem::path(candidateDir) / "candidate_state_probe.jsonl";
  std::ofstream file(path, std::ios::binary | std::ios::app);
  if (file.is_open())
    file << line.str();

  CXLOG_INFO(
      "EvidenceCandidate", phase.c_str(), status,
      "candidate_id=" + candidateId + " gauge={threshold=" +
          std::to_string(g.threshold) + ",method=" + std::to_string(g.method) +
          ",linegap=" + std::to_string(g.linegap) + ",wgap=" +
          std::to_string(g.wgap) +
          ",min_edge_run_width_px=" +
          std::to_string(g.min_edge_run_width_px) + ",hgap=" +
          std::to_string(g.hgap) +
          ",filterprofile=" + std::to_string(g.filterprofile) +
          "} globals={threshold=" +
          std::to_string(globalValue("global_threshold")) +

          ",method=" + std::to_string(globalValue("global_method")) +

          ",linegap=" + std::to_string(globalValue("global_linegap")) +

          ",min_edge_run_width_px=" +
          std::to_string(globalValue("global_min_edge_run_width_px")) +

          ",wgap=" + std::to_string(globalValue("global_wgap")) +

          ",hgap=" + std::to_string(globalValue("global_hgap")) +

          ",filterprofile=" +
          std::to_string(globalValue("global_filterprofile")) +

          ",learn_roi=(" + std::to_string(globalValue("global_learn_roi_x")) +

          "," + std::to_string(globalValue("global_learn_roi_y")) +

          "," + std::to_string(globalValue("global_learn_roi_w")) +

          "," + std::to_string(globalValue("global_learn_roi_h")) + ")" +

          ",search_roi=(" + std::to_string(globalValue("global_search_roi_x")) +

          "," + std::to_string(globalValue("global_search_roi_y")) +

          "," + std::to_string(globalValue("global_search_roi_w")) +

          "," + std::to_string(globalValue("global_search_roi_h")) + ")" +

          ",match_step=(" + std::to_string(globalValue("global_match_step_x")) +

          "," + std::to_string(globalValue("global_match_step_y")) + ")" +

          ",match_thre=" + std::to_string(globalValue("global_match_thre")) +

          ",min_score=" + std::to_string(globalValue("global_min_score")) +

          ",min_score_percent=" +
          std::to_string(globalValue("global_min_score_percent")) +

          ",find_num=" + std::to_string(globalValue("global_find_num")) +

          ",fastmatch_action=" +
          std::to_string(globalValue("global_fastmatch_action")) +

          ",compare_gap=" +
          std::to_string(globalValue("global_fastmatch_learn_compare_gap_3")) +

          ",objfilter=" +
          std::to_string(globalValue("global_fastmatch_learn_objfilter_3")) +

          ",edge_count=" +
          std::to_string(globalValue("global_findline_edge_count")) +

          ",selected_edge=" +
          std::to_string(globalValue("global_findline_selected_edge")) +

          ",best_edge=" +
          std::to_string(globalValue("global_findline_best_edge")) +

          ",recommended_edge=" +
          std::to_string(globalValue("global_findline_recommended_edge")) +

          ",relation_edge=" +
          std::to_string(globalValue("global_findline_relation_edge")) +

          ",attach_edge=" +
          std::to_string(globalValue("global_findline_attach_edge")) +

          "} reason=" + reason);
}

void AppendManualOperationTraceJsonl(
    const ManualTestContext &context, const std::string &candidateDir,
    const std::string &candidateId, const std::string &operation,
    const std::string &status, const std::string &reason,
    const std::string &caseId, const std::string &scriptId,
    const std::string &imageId, const std::string &imagePath,
    const std::string &targetId, const std::string &tool,
    const std::string &sourceEvidenceScriptPath,
    const std::string &scriptSnapshotPath,
    const std::string &parameterSnapshotPath,
    const std::string &runtimeGlobalsPath,
    const std::string &gaugeAnnotationPath,
    const std::string &resultSummaryPath, bool replayTrusted,
    const std::string &replayWarning) {
  if (candidateDir.empty())
    return;

  const char q = static_cast<char>(34);
  const ManualGaugeState &g = context.current_gauge;
  auto globalValue = [&](const char *key) -> int {
    const auto it = context.runtime_int_vars.find(key);
    return it == context.runtime_int_vars.end() ? -2147483647 : it->second;
  };
  auto writeString = [&](std::ostringstream &out, const char *key,
                         const std::string &value) {
    out << ',' << q << key << q << ':' << q << JsonEscape(value) << q;
  };

  std::ostringstream line;
  line << '{' << q << "schema_version" << q << ":1";
  writeString(line, "record_type", "manual_operation");
  writeString(line, "candidate_id", candidateId);
  writeString(line, "operation", operation);
  writeString(line, "status", status);
  writeString(line, "reason", reason);
  writeString(line, "case_id", caseId);
  writeString(line, "script_id", scriptId);
  writeString(line, "image_id", imageId);
  writeString(line, "image_path", imagePath);
  writeString(line, "target_id", targetId);
  writeString(line, "tool", tool);
  writeString(line, "editor_source", context.editor_source);
  writeString(line, "loaded_script_path", context.loaded_script_path);
  writeString(line, "source_evidence_script_path", sourceEvidenceScriptPath);
  writeString(line, "script_snapshot_path", scriptSnapshotPath);
  writeString(line, "parameter_snapshot_path", parameterSnapshotPath);
  writeString(line, "runtime_globals_path", runtimeGlobalsPath);
  writeString(line, "gauge_annotation_path", gaugeAnnotationPath);
  writeString(line, "result_summary_path", resultSummaryPath);
  line << ',' << q << "replay_trusted" << q << ':'
       << (replayTrusted ? "true" : "false");
  writeString(line, "replay_warning", replayWarning);
  line << ',' << q << "key_parameter_edit_revision" << q << ':'
       << context.key_parameter_edit_revision;
  writeString(line, "last_key_parameter_edit_summary",
              context.last_key_parameter_edit_summary);
  line << ',' << q << "gauge" << q << ":{" << q << "x0" << q << ':'
       << g.line_x0 << ',' << q << "y0" << q << ':' << g.line_y0 << ','
       << q << "x1" << q << ':' << g.line_x1 << ',' << q << "y1" << q
       << ':' << g.line_y1 << ',' << q << "half_width" << q << ':'
       << g.tool_half_width << ',' << q << "threshold" << q << ':'
       << g.threshold << ',' << q << "method" << q << ':' << g.method << ','
       << q << "linegap" << q << ':' << g.linegap << ',' << q << "wgap"
       << q << ':' << g.wgap << ',' << q << "hgap" << q << ':' << g.hgap
       << ',' << q << "min_edge_run_width_px" << q << ':'
       << g.min_edge_run_width_px << ',' << q << "filterprofile" << q << ':'
       << g.filterprofile << '}';
  line << ',' << q << "globals" << q << ":{" << q << "global_roi_x0" << q
       << ':' << globalValue("global_roi_x0") << ',' << q
       << "global_roi_y0" << q << ':' << globalValue("global_roi_y0") << ','
       << q << "global_roi_x1" << q << ':' << globalValue("global_roi_x1")
       << ',' << q << "global_roi_y1" << q << ':'
       << globalValue("global_roi_y1") << ',' << q
       << "global_tool_half_width" << q << ':'
       << globalValue("global_tool_half_width") << ',' << q
       << "global_threshold" << q << ':' << globalValue("global_threshold")
       << ',' << q << "global_method" << q << ':'
       << globalValue("global_method") << ',' << q << "global_linegap" << q
       << ':' << globalValue("global_linegap") << ',' << q
       << "global_min_edge_run_width_px" << q << ':'
       << globalValue("global_min_edge_run_width_px") << ',' << q
       << "global_wgap"
       << q << ':' << globalValue("global_wgap") << ',' << q << "global_hgap"
       << q << ':' << globalValue("global_hgap") << ',' << q
       << "global_filterprofile" << q << ':'
       << globalValue("global_filterprofile") << "}}";

  const std::filesystem::path path =
      std::filesystem::path(candidateDir) / "manual_operation_trace.jsonl";
  std::ofstream file(path, std::ios::binary | std::ios::app);
  if (file.is_open()) {
    file << line.str();
    file.put(10);
  }
}

void AppendManualOperationTraceEventJsonl(
    const ManualOperationTraceEvent &event, const std::string &candidateDir,
    const std::string &candidateId, const std::string &caseId,
    const std::string &scriptId, const std::string &imageId,
    const std::string &imagePath, const std::string &targetId,
    const std::string &tool, const std::string &sourceEvidenceScriptPath,
    const std::string &scriptSnapshotPath,
    const std::string &parameterSnapshotPath,
    const std::string &runtimeGlobalsPath,
    const std::string &gaugeAnnotationPath,
    const std::string &resultSummaryPath, bool replayTrusted,
    const std::string &replayWarning) {
  if (candidateDir.empty())
    return;

  const char q = static_cast<char>(34);
  const ManualGaugeState &g = event.gauge;
  auto writeString = [&](std::ostringstream &out, const char *key,
                         const std::string &value) {
    out << ',' << q << key << q << ':' << q << JsonEscape(value) << q;
  };
  auto writeBool = [&](std::ostringstream &out, const char *key, bool value) {
    out << ',' << q << key << q << ':' << (value ? "true" : "false");
  };

  std::ostringstream line;
  line << '{' << q << "schema_version" << q << ":1";
  writeString(line, "record_type", "manual_operation_event");
  line << ',' << q << "event_sequence" << q << ':' << event.sequence;
  writeString(line, "candidate_id", candidateId);
  writeString(line, "operation", event.operation);
  writeString(line, "status", event.status);
  writeString(line, "reason", event.reason);
  writeString(line, "event_summary", event.summary);
  line << ',' << q << "key_parameter_edit_revision" << q << ':'
       << event.key_parameter_edit_revision;
  writeString(line, "case_id", caseId);
  writeString(line, "script_id", scriptId);
  writeString(line, "image_id", imageId);
  writeString(line, "image_path", imagePath);
  writeString(line, "target_id", targetId);
  writeString(line, "tool", tool);
  writeString(line, "event_editor_source", event.editor_source);
  writeString(line, "event_loaded_script_path", event.loaded_script_path);
  writeString(line, "event_selection_case_id", event.evidence_selection.case_id);
  writeString(line, "event_selection_candidate_id",
              event.evidence_selection.candidate_id);
  writeString(line, "source_evidence_script_path", sourceEvidenceScriptPath);
  writeString(line, "script_snapshot_path", scriptSnapshotPath);
  writeString(line, "parameter_snapshot_path", parameterSnapshotPath);
  writeString(line, "runtime_globals_path", runtimeGlobalsPath);
  writeString(line, "gauge_annotation_path", gaugeAnnotationPath);
  writeString(line, "result_summary_path", resultSummaryPath);
  writeBool(line, "replay_trusted", replayTrusted);
  writeString(line, "replay_warning", replayWarning);

  line << ',' << q << "gauge" << q << ":{" << q << "tool" << q << ':' << q
       << JsonEscape(g.tool) << q << ',' << q << "source" << q << ':' << q
       << JsonEscape(g.source) << q << ',' << q << "review_status" << q << ':'
       << q << JsonEscape(g.review_status) << q << ',' << q << "dirty" << q
       << ':' << (g.dirty ? "true" : "false") << ',' << q << "x0" << q
       << ':' << g.line_x0 << ',' << q << "y0" << q << ':' << g.line_y0
       << ',' << q << "x1" << q << ':' << g.line_x1 << ',' << q << "y1"
       << q << ':' << g.line_y1 << ',' << q << "half_width" << q << ':'
       << g.tool_half_width << ',' << q << "threshold" << q << ':'
       << g.threshold << ',' << q << "method" << q << ':' << g.method << ','
       << q << "linegap" << q << ':' << g.linegap << ',' << q << "wgap"
       << q << ':' << g.wgap << ',' << q << "hgap" << q << ':' << g.hgap
       << ',' << q << "min_edge_run_width_px" << q << ':'
       << g.min_edge_run_width_px << ',' << q << "filterprofile" << q << ':'
       << g.filterprofile << '}';

  std::vector<std::pair<std::string, int>> globals(event.globals.begin(),
                                                   event.globals.end());
  std::sort(globals.begin(), globals.end());
  line << ',' << q << "globals" << q << ":{";
  bool firstGlobal = true;
  for (const auto &kv : globals) {
    if (kv.first.rfind("global_", 0) != 0)
      continue;
    if (!firstGlobal)
      line << ',';
    firstGlobal = false;
    line << q << JsonEscape(kv.first) << q << ':' << kv.second;
  }
  line << "}}";

  const std::filesystem::path path =
      std::filesystem::path(candidateDir) / "manual_operation_trace.jsonl";
  std::ofstream file(path, std::ios::binary | std::ios::app);
  if (file.is_open()) {
    file << line.str();
    file.put(10);
  }
}

void AppendPendingManualOperationTraceEventsJsonl(
    const ManualTestContext &context, const std::string &candidateDir,
    const std::string &candidateId, const std::string &caseId,
    const std::string &scriptId, const std::string &imageId,
    const std::string &imagePath, const std::string &targetId,
    const std::string &tool, const std::string &sourceEvidenceScriptPath,
    const std::string &scriptSnapshotPath,
    const std::string &parameterSnapshotPath,
    const std::string &runtimeGlobalsPath,
    const std::string &gaugeAnnotationPath,
    const std::string &resultSummaryPath, bool replayTrusted,
    const std::string &replayWarning) {
  for (const ManualOperationTraceEvent &event :
       context.pending_manual_operation_trace_events) {
    AppendManualOperationTraceEventJsonl(
        event, candidateDir, candidateId, caseId, scriptId, imageId, imagePath,
        targetId, tool, sourceEvidenceScriptPath, scriptSnapshotPath,
        parameterSnapshotPath, runtimeGlobalsPath, gaugeAnnotationPath,
        resultSummaryPath, replayTrusted, replayWarning);
  }
}

std::string GenerateCandidateId() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t t = std::chrono::system_clock::to_time_t(now);
  std::tm local_time = {};
#if defined(_WIN32)
  localtime_s(&local_time, &t);
#else
  localtime_r(&t, &local_time);
#endif
  char buffer[32] = {};
  std::strftime(buffer, sizeof(buffer), "candidate_%Y%m%d_%H%M%S", &local_time);
  return buffer;
}

void WriteSegmentationPromptPointArray(
    std::ostringstream &out, const char *key,
    const std::vector<ManualSegmentationPromptPoint> &points,
    bool trailingComma) {
  out << "  \"" << key << "\": [";
  for (std::size_t i = 0; i < points.size(); ++i) {
    if (i != 0)
      out << ", ";
    out << "{\"ref\":\"" << JsonEscape(points[i].ref)
        << "\",\"x\":" << points[i].x << ",\"y\":" << points[i].y << "}";
  }
  out << "]" << (trailingComma ? "," : "") << "\n";
}

std::string EvidenceToolGroupLabel(const std::string &tool) {
  if (tool == "FindLine" || tool == "Findline")
    return "FindLine";
  if (tool == "FindCircle" || tool == "Findcircle")
    return "FindCircle";
  if (tool == "FindEllipse" || tool == "Findellipse")
    return "FindEllipse";
  if (tool == "FindRect")
    return "FindRect";
  if (tool == "FastMatch" || tool == "fastmatch")
    return "FastMatch";
  if (tool == "FindSegmentation")
    return "FindSegmentation";
  return tool.empty() ? "Module" : tool;
}

std::string CurrentScriptText(const ManualTestContext &context) {
  if (!context.editor_text.empty())
    return context.editor_text;

  if (!context.loaded_script_path.empty()) {
    std::string text;
    if (ReadTextFile(context.loaded_script_path, text))
      return text;
  }
  if (!context.script_file_path.empty()) {
    std::string text;
    if (ReadTextFile(context.script_file_path, text))
      return text;
  }
  return {};
}

std::string EffectiveScriptPath(const ManualTestContext &context) {
  if (!context.loaded_script_path.empty())
    return context.loaded_script_path;
  if (!context.script_file_path.empty())
    return context.script_file_path;
  if (context.current_evidence_selection.valid)
    return context.current_evidence_selection.script_path;
  return {};
}

std::string EffectiveScriptId(const ManualTestContext &context) {
  if (context.current_evidence_selection.valid &&
      !context.current_evidence_selection.script_id.empty())
    return StripEvidenceCandidateDisplaySuffix(
        context.current_evidence_selection.script_id);
  const std::string scriptPath = EffectiveScriptPath(context);
  if (!scriptPath.empty())
    return StripEvidenceCandidateDisplaySuffix(
        std::filesystem::path(scriptPath).filename().string());
  return StripEvidenceCandidateDisplaySuffix(context.active_script_case_name);
}

std::string EffectiveCaseId(const ManualTestContext &context) {
  // Evidence Chain is the authoritative identity for candidate saves.  The
  // Manual/Run path can temporarily clear active_case_id while preserving the
  // selected Evidence snapshot; do not fall back to "manual_case" in that
  // situation or the candidate becomes invisible from the original case row.
  if (context.current_evidence_selection.valid) {
    const std::string evidenceCaseId =
        StableEvidenceCaseIdFromSelection(context.current_evidence_selection);
    if (!evidenceCaseId.empty())
      return evidenceCaseId;
  }
  if (!context.active_case_id.empty())
    return context.active_case_id;
  if (!context.current_gauge.case_id.empty())
    return context.current_gauge.case_id;
  if (!context.active_script_case_name.empty())
    return context.active_script_case_name;
  return "manual_case";
}

std::string EffectiveImagePath(const ManualTestContext &context) {
  if (!context.image_file_path.empty())
    return context.image_file_path;
  if (context.current_evidence_selection.valid)
    return context.current_evidence_selection.image_path;
  return {};
}

std::string EffectiveImageId(const ManualTestContext &context) {
  if (!context.active_image_id.empty())
    return context.active_image_id;
  if (context.current_evidence_selection.valid)
    return context.current_evidence_selection.image_id;
  if (!context.current_gauge.image_id.empty())
    return context.current_gauge.image_id;
  return {};
}

std::string EffectiveTargetId(const ManualTestContext &context) {
  if (!context.active_target_id.empty())
    return context.active_target_id;
  if (context.current_evidence_selection.valid)
    return context.current_evidence_selection.target_id;
  if (!context.current_gauge.target_id.empty())
    return context.current_gauge.target_id;
  return {};
}

std::string EffectiveTool(const ManualTestContext &context) {
  if (!context.current_gauge.tool.empty())
    return context.current_gauge.tool;
  if (context.current_evidence_selection.valid)
    return context.current_evidence_selection.tool;
  return {};
}

std::string BuildParameterSummaryFromGlobals(const ManualTestContext &context) {
  std::vector<std::pair<std::string, int>> vars(
      context.runtime_int_vars.begin(), context.runtime_int_vars.end());
  std::sort(vars.begin(), vars.end());

  std::ostringstream oss;
  bool first = true;
  for (const auto &kv : vars) {
    if (kv.first.rfind("global_", 0) != 0)
      continue;
    if (!first)
      oss << " ";
    oss << kv.first.substr(7) << "=" << kv.second;
    first = false;
  }
  return oss.str();
}

std::string EffectiveParameterSummary(const ManualTestContext &context) {
  const std::string fromGlobals = BuildParameterSummaryFromGlobals(context);
  if (!fromGlobals.empty())
    return fromGlobals;
  if (context.current_evidence_selection.valid)
    return context.current_evidence_selection.parameter_summary;
  return {};
}

bool WriteGaugeSnapshotJson(const ManualGaugeState &gauge,
                            const std::filesystem::path &path,
                            std::string &reason) {
  std::ostringstream json;
  json << "{\n"
       << "  \"schema_version\": 1,\n"
       << "  \"case_id\": \"" << JsonEscape(gauge.case_id) << "\",\n"
       << "  \"image_id\": \"" << JsonEscape(gauge.image_id) << "\",\n"
       << "  \"target_id\": \"" << JsonEscape(gauge.target_id) << "\",\n"
       << "  \"tool\": \"" << JsonEscape(gauge.tool) << "\",\n"
       << "  \"source\": \"" << JsonEscape(gauge.source) << "\",\n"
       << "  \"review_status\": \"" << JsonEscape(gauge.review_status)
       << "\",\n"
       << "  \"dirty\": " << (gauge.dirty ? "true" : "false") << ",\n"
       << "  \"accepted\": " << (gauge.accepted ? "true" : "false") << ",\n"
       << "  \"primary_object_type\": \""
       << JsonEscape(gauge.primary_object_type) << "\",\n"
       << "  \"primary_object_name\": \""
       << JsonEscape(gauge.primary_object_name) << "\",\n"
       << "  \"primary_object_status\": \""
       << JsonEscape(gauge.primary_object_status) << "\",\n"
       << "  \"has_line_gauge\": " << (gauge.has_line_gauge ? "true" : "false")
       << ",\n"
       << "  \"line_x0\": " << gauge.line_x0 << ",\n"
       << "  \"line_y0\": " << gauge.line_y0 << ",\n"
       << "  \"line_x1\": " << gauge.line_x1 << ",\n"
       << "  \"line_y1\": " << gauge.line_y1 << ",\n"
       << "  \"tool_half_width\": " << gauge.tool_half_width << ",\n"
       << "  \"has_circle_gauge\": "
       << (gauge.has_circle_gauge ? "true" : "false") << ",\n"
       << "  \"circle_cx\": " << gauge.circle_cx << ",\n"
       << "  \"circle_cy\": " << gauge.circle_cy << ",\n"
       << "  \"circle_px\": " << gauge.circle_px << ",\n"
       << "  \"circle_py\": " << gauge.circle_py << ",\n"
       << "  \"radius\": " << gauge.radius << ",\n"
       << "  \"inner_radius\": " << gauge.inner_radius << ",\n"
       << "  \"outer_radius\": " << gauge.outer_radius << ",\n"
       << "  \"circle_arc_enabled\": "
       << (gauge.circle_arc_enabled ? "true" : "false") << ",\n"
       << "  \"circle_arc_start_deg\": " << gauge.circle_arc_start_deg << ",\n"
       << "  \"circle_arc_end_deg\": " << gauge.circle_arc_end_deg << ",\n"
       << "  \"has_ellipse_gauge\": "
       << (gauge.has_ellipse_gauge ? "true" : "false") << ",\n"
       << "  \"ellipse_x0\": " << gauge.ellipse_x0 << ",\n"
       << "  \"ellipse_y0\": " << gauge.ellipse_y0 << ",\n"
       << "  \"ellipse_x1\": " << gauge.ellipse_x1 << ",\n"
       << "  \"ellipse_y1\": " << gauge.ellipse_y1 << ",\n"
       << "  \"ellipse_inner_scale_percent\": "
       << gauge.ellipse_inner_scale_percent << ",\n"
       << "  \"has_segmentation_prompt_rect\": "
       << (gauge.has_segmentation_prompt_rect ? "true" : "false") << ",\n"
       << "  \"segmentation_prompt_x0\": " << gauge.segmentation_prompt_x0
       << ",\n"
       << "  \"segmentation_prompt_y0\": " << gauge.segmentation_prompt_y0
       << ",\n"
       << "  \"segmentation_prompt_x1\": " << gauge.segmentation_prompt_x1
       << ",\n"
       << "  \"segmentation_prompt_y1\": " << gauge.segmentation_prompt_y1
       << ",\n"
       << "  \"segmentation_mode\": " << gauge.segmentation_mode << ",\n"
       << "  \"segmentation_threshold_percent\": "
       << gauge.segmentation_threshold_percent << ",\n"
       << "  \"has_segmentation_positive_point\": "
       << (gauge.has_segmentation_positive_point ? "true" : "false") << ",\n"
       << "  \"segmentation_positive_x\": " << gauge.segmentation_positive_x
       << ",\n"
       << "  \"segmentation_positive_y\": " << gauge.segmentation_positive_y
       << ",\n"
       << "  \"has_segmentation_negative_point\": "
       << (gauge.has_segmentation_negative_point ? "true" : "false") << ",\n"
       << "  \"segmentation_negative_x\": " << gauge.segmentation_negative_x
       << ",\n"
       << "  \"segmentation_negative_y\": " << gauge.segmentation_negative_y
       << ",\n";
  WriteSegmentationPromptPointArray(json, "segmentation_positive_points",
                                    gauge.segmentation_positive_points, true);
  WriteSegmentationPromptPointArray(json, "segmentation_negative_points",
                                    gauge.segmentation_negative_points, true);
  json << "  \"segmentation_prompt_pick_mode\": "
       << gauge.segmentation_prompt_pick_mode << ",\n"
       << "  \"wgap\": " << gauge.wgap << ",\n"
       << "  \"hgap\": " << gauge.hgap << ",\n"
       << "  \"gap\": " << gauge.gap << ",\n"
       << "  \"linegap\": " << gauge.linegap << ",\n"
       << "  \"min_edge_run_width_px\": "
       << gauge.min_edge_run_width_px << ",\n"
       << "  \"threshold\": " << gauge.threshold << ",\n"
       << "  \"filterprofile\": " << gauge.filterprofile << ",\n"
       << "  \"method\": " << gauge.method << "\n"
       << "}\n";
  if (!WriteTextFile(path, json.str())) {
    reason = "failed to write gauge_annotation.json";
    return false;
  }
  return true;
}

bool WriteRuntimeGlobalsJson(const ManualTestContext &context,
                             const std::string &tool, const std::string &caseId,
                             const std::string &scriptId,
                             const std::string &scriptPath,
                             const std::string &sourceEvidenceScriptPath,
                             const std::filesystem::path &path,
                             std::string &reason) {
  std::vector<std::pair<std::string, int>> vars(
      context.runtime_int_vars.begin(), context.runtime_int_vars.end());
  SanitizeFindEllipseCandidateGlobals(tool, caseId, scriptId, scriptPath,
                                      sourceEvidenceScriptPath, vars);
  std::sort(vars.begin(), vars.end());

  std::ostringstream json;
  json << "{\n"
       << "  \"schema_version\": 1,\n"
       << "  \"globals\": {";
  bool first = true;
  for (const auto &kv : vars) {
    if (!first)
      json << ",";
    json << "\n    \"" << JsonEscape(kv.first) << "\": " << kv.second;
    first = false;
  }
  if (!first)
    json << "\n  ";
  json << "}\n}\n";
  if (!WriteTextFile(path, json.str())) {
    reason = "failed to write runtime_globals.json";
    return false;
  }
  return true;
}

bool WriteLineTraceJson(const ManualTestContext &context,
                        const std::filesystem::path &path,
                        std::string &reason) {
  std::ostringstream json;
  json << "{\n"
       << "  \"schema_version\": 1,\n"
       << "  \"lines\": [";
  bool first = true;
  for (const auto &line : context.line_views) {
    if (!first)
      json << ",";
    json << "\n    {"
         << "\"line_no\": " << line.line_no << ", "
         << "\"statement\": \"" << JsonEscape(line.statement) << "\", "
         << "\"status\": \"" << JsonEscape(line.status) << "\""
         << "}";
    first = false;
  }
  if (!first)
    json << "\n  ";
  json << "]\n}\n";
  if (!WriteTextFile(path, json.str())) {
    reason = "failed to write line_trace.json";
    return false;
  }
  return true;
}

bool AppendCandidateToEvidenceChain(
    ManualTestContext &context, const CxEvidenceCandidateSaveResult &result,
    const std::string &caseId, const std::string &scriptId,
    const std::string &scriptPath, const std::string &sourceEvidenceScriptPath,
    const std::string &imageId, const std::string &imagePath,
    const std::string &targetId, const std::string &tool,
    const std::string &parameterSummary, bool allowNewRow) {
  auto bindCurrentCaseState = [&](auto &target) {
    target.candidate_id = result.candidate_id;
    target.candidate_dir = result.candidate_dir;
    target.evidence_binding_path = result.evidence_binding_path;
    target.parameter_snapshot_path = result.parameter_snapshot_path;
    target.runtime_globals_path =
        (std::filesystem::path(result.candidate_dir) / "runtime_globals.json")
            .string();
    target.gauge_annotation_path = result.gauge_annotation_path;
    target.working_script_snapshot_path = result.script_snapshot_path.empty()
                                            ? scriptPath
                                            : result.script_snapshot_path;
    target.has_saved_state = true;
    target.source_evidence_script_path = sourceEvidenceScriptPath;
    target.parameter_summary = parameterSummary;
    target.primary_object_type = context.current_gauge.primary_object_type;
    target.primary_object_name = context.current_gauge.primary_object_name;
    target.primary_object_status = context.current_gauge.primary_object_status;
    target.status = "pending_human_review";
    target.reason = "current case state updated; working_state=" +
                    result.candidate_id;
    target.is_candidate = false;
  };

  if (!context.current_evidence_selection.is_candidate) {
    context.current_evidence_selection.case_id = caseId;
    context.current_evidence_selection.script_id =
        !scriptId.empty() ? StripEvidenceCandidateDisplaySuffix(scriptId)
                          : caseId;
    context.current_evidence_selection.script_path =
        result.script_snapshot_path.empty() ? scriptPath
                                            : result.script_snapshot_path;
    context.current_evidence_selection.image_id = imageId;
    context.current_evidence_selection.image_path = imagePath;
    context.current_evidence_selection.target_id = targetId;
    context.current_evidence_selection.tool = tool;
    bindCurrentCaseState(context.current_evidence_selection);
    context.current_evidence_selection.parameter_profile_id = parameterSummary;
  }

  ScriptEvidenceThumb thumb;
  thumb.case_id = caseId;
  thumb.script_id = !scriptId.empty() ? StripEvidenceCandidateDisplaySuffix(scriptId)
                                      : (caseId.empty()
                                             ? std::string("current_case")
                                             : caseId);
  if (thumb.script_id.empty() && !sourceEvidenceScriptPath.empty())
    thumb.script_id = StripEvidenceCandidateDisplaySuffix(
        std::filesystem::path(sourceEvidenceScriptPath).stem().string());
  if (thumb.script_id.empty() && !scriptPath.empty())
    thumb.script_id = StripEvidenceCandidateDisplaySuffix(
        std::filesystem::path(scriptPath).stem().string());
  thumb.script_path = result.script_snapshot_path.empty()
                          ? scriptPath
                          : result.script_snapshot_path;
  thumb.source_evidence_script_path = sourceEvidenceScriptPath;
  thumb.image_id = imageId;
  thumb.image_path = imagePath;
  thumb.thumbnail_path = imagePath;
  thumb.target_id = targetId;
  thumb.tool = tool;
  bindCurrentCaseState(thumb);
  if (thumb.primary_object_status.empty())
    thumb.primary_object_status = "unresolved";

  // First bind a save back to the exact currently selected asset-backed row.
  // The logical case name alone is not sufficient because identical names may
  // exist in separate run directories.
  const std::string selectedOrigin =
      context.current_evidence_selection.evidence_output_root;
  for (auto &group : context.script_evidence_groups) {
    for (auto &existing : group.thumbs) {
      const bool sameCase = !thumb.case_id.empty() &&
                            thumb.case_id == existing.case_id;
      const bool sameOrigin = selectedOrigin.empty() ||
                              existing.evidence_output_root == selectedOrigin;
      if (sameCase && sameOrigin) {
        const std::string preservedOutputRoot = existing.evidence_output_root;
        const std::string preservedReviewItem = existing.review_item;
        const std::string preservedHead = existing.evidence_head_folder;
        const std::string preservedFolder = existing.evidence_case_folder;
        existing = thumb;
        existing.evidence_output_root = preservedOutputRoot;
        existing.review_item = preservedReviewItem;
        existing.evidence_head_folder = preservedHead;
        existing.evidence_case_folder = preservedFolder;
        context.script_evidence_row_refs_dirty = true;
        return false;
      }
    }
  }

  if (!allowNewRow)
    return false;

  const std::string groupLabel = EvidenceToolGroupLabel(tool);
  ScriptEvidenceGroup *groupPtr = nullptr;
  for (auto &group : context.script_evidence_groups) {
    if (group.label == groupLabel) {
      groupPtr = &group;
      break;
    }
  }
  if (groupPtr == nullptr) {
    ScriptEvidenceGroup group;
    group.label = groupLabel;
    context.script_evidence_groups.push_back(group);
    groupPtr = &context.script_evidence_groups.back();
  }

  for (auto &existing : groupPtr->thumbs) {
    const bool sameCase = !thumb.case_id.empty() && !existing.case_id.empty() &&
                         existing.case_id == thumb.case_id;
    const bool sameScript = !thumb.script_id.empty() &&
                           !existing.script_id.empty() &&
                           existing.script_id == thumb.script_id;
    const bool sameImageTarget = !thumb.image_id.empty() &&
                                 !thumb.target_id.empty() &&
                                 thumb.image_id == existing.image_id &&
                                 thumb.target_id == existing.target_id;
    if (sameCase || sameScript || sameImageTarget) {
      existing = thumb;
      context.script_evidence_row_refs_dirty = true;
      return false;
    }
  }

  groupPtr->thumbs.push_back(thumb);
  context.script_evidence_row_refs_dirty = true;
  return true;
}
}

void AppendEvidenceCandidateStateProbe(
    const ManualTestContext &context, const std::string &candidateDir,
    const std::string &candidateId, const std::string &phase,
    const std::string &status, const std::string &reason) {
  AppendEvidenceCandidateStateProbeImpl(context, candidateDir, candidateId,
                                        phase, status, reason);
}

bool SaveEvidenceCandidatePackage(ManualTestContext &context,
                                  const CxEvidenceCandidateSaveOptions &options,
                                  CxEvidenceCandidateSaveResult &result) {
  result = CxEvidenceCandidateSaveResult{};

  const std::string caseId = options.case_id_override.empty()
                                 ? EffectiveCaseId(context)
                                 : options.case_id_override;
  const std::string safeCaseId = SafePathComponentForCandidate(caseId);
  if (safeCaseId.empty()) {
    result.reason = "case_id is empty";
    return false;
  }
  const std::string candidateId = SafePathComponentForCandidate(
      options.candidate_id.empty() ? std::string("current")
                                   : options.candidate_id);
  if (candidateId.empty()) {
    result.reason = "case working state id is empty";
    return false;
  }
  const std::string originalScriptText = CurrentScriptText(context);

  bool normalizedFindSegmentationPromptOrder = false;

  const std::string scriptText =
      NormalizeFindSegmentationPromptCallOrderForSnapshot(
          originalScriptText, &normalizedFindSegmentationPromptOrder);

  if (scriptText.empty())

  {

    result.reason = "script text is empty";

    return false;
  }

  const std::string imagePath = EffectiveImagePath(context);
  if (imagePath.empty()) {
    result.reason = "image_path is empty";
    return false;
  }

  const std::string scriptPath = EffectiveScriptPath(context);
  const std::string sourceEvidenceScriptPath =
      !options.source_evidence_script_path_override.empty()
          ? options.source_evidence_script_path_override
          : (context.current_evidence_selection.source_evidence_script_path.empty()
                 ? scriptPath
                 : context.current_evidence_selection.source_evidence_script_path);
  const std::string scriptId = EffectiveScriptId(context);
  const std::string imageId = EffectiveImageId(context);
  const std::string targetId = EffectiveTargetId(context);
  const std::string tool = EffectiveTool(context);
  const std::string parameterSummary = EffectiveParameterSummary(context);

  if (context.current_gauge.case_id.empty())
    context.current_gauge.case_id = caseId;
  if (context.current_gauge.image_id.empty())
    context.current_gauge.image_id = imageId;
  if (context.current_gauge.target_id.empty())
    context.current_gauge.target_id = targetId;

  NormalizeManualGaugeGeometry(context.current_gauge);
  ApplyManualGaugeToGlobals(context);
  std::string gaugeReason;
  if (!ValidateManualGaugeGeometryForEditing(context.current_gauge,
                                             gaugeReason)) {
    result.reason = "invalid evidence candidate gauge: " + gaugeReason;
    CXLOG_WARN("EvidenceCandidate", "candidate_save_blocked", "invalid_gauge",
               "case_id=" + caseId + " candidate_id=" + candidateId +
                   " tool=" + tool + " reason=" + result.reason);
    return false;
  }

  std::error_code ec;
  bool rootCanonicalized = false;
  const std::filesystem::path root =
      CanonicalEvidenceCandidateRoot(options.root_dir, &rootCanonicalized);
  if (rootCanonicalized) {
    CXLOG_INFO("EvidenceCandidate", "candidate_root_canonicalized", "available",
               "configured_root=" + options.root_dir +
                   " canonical_root=" + root.string());
  }

  const std::filesystem::path candidateDir =
      (root / safeCaseId / candidateId).lexically_normal();
  std::filesystem::create_directories(candidateDir, ec);
  if (ec) {
    result.reason = "cannot create evidence candidate dir: " + ec.message();
    return false;
  }

  std::string bindingProbeReason =
      "candidate values captured after ApplyManualGaugeToGlobals";
  if (context.current_gauge.has_ellipse_gauge) {
    bindingProbeReason +=
        "; ellipse_bbox=(" + std::to_string(context.current_gauge.ellipse_x0) +
        "," + std::to_string(context.current_gauge.ellipse_y0) + "," +
        std::to_string(context.current_gauge.ellipse_x1) + "," +
        std::to_string(context.current_gauge.ellipse_y1) + ")";
  }
  if (normalizedFindSegmentationPromptOrder) {
    bindingProbeReason +=
        "; FindSegmentation prompt point calls normalized to set*pointxy(y,x)";
    CXLOG_INFO("EvidenceCandidate",
               "findsegmentation_prompt_call_order_normalized", "normalized",
               "candidate_id=" + candidateId +
                   " script=" + EffectiveScriptPath(context));
  }
  const bool filterProfileCallPresent =
      scriptText.find("setfilterprofile(") != std::string::npos;
  const bool filterProfileBound =
      !filterProfileCallPresent ||
      (scriptText.find("global_filterprofile") != std::string::npos &&
       (scriptText.find("setfilterprofile(filterprofile)") !=
            std::string::npos ||
        scriptText.find("setfilterprofile(global_filterprofile)") !=
            std::string::npos));
  const bool filterProfileBindingConflict =
      filterProfileCallPresent && !filterProfileBound;
  const bool minEdgeRunWidthCallPresent =
      scriptText.find("setminedgerunwidth(") != std::string::npos;
  const bool minEdgeRunWidthBound =
      !minEdgeRunWidthCallPresent ||
      scriptText.find("setminedgerunwidth(global_min_edge_run_width_px)") !=
          std::string::npos;
  const bool minEdgeRunWidthBindingConflict =
      minEdgeRunWidthCallPresent && !minEdgeRunWidthBound;
  std::string replayWarning;
  if (filterProfileBindingConflict)
    replayWarning =
        "SCRIPT_BINDING_CONFLICT: setfilterprofile uses a literal or "
        "non-global value";
  if (minEdgeRunWidthBindingConflict) {
    if (!replayWarning.empty())
      replayWarning += "; ";
    replayWarning +=
        "SCRIPT_BINDING_CONFLICT: setminedgerunwidth uses a literal or "
        "non-global value";
  }
  const bool replayTrusted =
      !filterProfileBindingConflict && !minEdgeRunWidthBindingConflict;
  if (!replayWarning.empty())
    bindingProbeReason += "; " + replayWarning;
  const std::filesystem::path scriptSnapshotPath =
      candidateDir / "script_snapshot.cxsc";
  const std::filesystem::path parameterSnapshotPath =
      candidateDir / "parameter_snapshot.cxsc";
  const std::filesystem::path gaugeAnnotationPath =
      candidateDir / "gauge_annotation.json";
  const std::filesystem::path runtimeGlobalsPath =
      candidateDir / "runtime_globals.json";
  const std::filesystem::path analysisStatePath =
      candidateDir / "analysis_state.json";
  const std::filesystem::path resultSummaryPath =
      candidateDir / "result_summary.json";
  const std::filesystem::path evidenceBindingPath =
      candidateDir / "evidence_binding.json";
  const std::filesystem::path lineTracePath = candidateDir / "line_trace.json";
  const std::filesystem::path logPath = candidateDir / "log.txt";
  const std::filesystem::path manualOperationTracePath =
      candidateDir / "manual_operation_trace.jsonl";


  AppendPendingManualOperationTraceEventsJsonl(
      context, candidateDir.string(), candidateId, caseId, scriptId, imageId,
      imagePath, targetId, tool,
      ResolveWorkspaceFile(sourceEvidenceScriptPath).string(),
      scriptSnapshotPath.string(), parameterSnapshotPath.string(),
      runtimeGlobalsPath.string(), gaugeAnnotationPath.string(),
      resultSummaryPath.string(), replayTrusted, replayWarning);

  AppendEvidenceCandidateStateProbe(
      context, candidateDir.string(), candidateId, "candidate_save_input",
      replayTrusted ? "captured" : "captured_with_binding_warning",
      bindingProbeReason);
  AppendManualOperationTraceJsonl(
      context, candidateDir.string(), candidateId, "candidate_save_input",
      replayTrusted ? "captured" : "captured_with_binding_warning",
      bindingProbeReason, caseId, scriptId, imageId, imagePath, targetId, tool,
      ResolveWorkspaceFile(sourceEvidenceScriptPath).string(),
      scriptSnapshotPath.string(), parameterSnapshotPath.string(),
      runtimeGlobalsPath.string(), gaugeAnnotationPath.string(),
      resultSummaryPath.string(), replayTrusted, replayWarning);

  const bool preserveInputs = options.preserve_input_snapshots;

  if (!preserveInputs && !WriteTextFile(scriptSnapshotPath, scriptText)) {
    result.reason = "failed to write script_snapshot.cxsc";
    return false;
  }

  std::ostringstream params;
  params << "// Evidence candidate parameter snapshot\n"
         << "// candidate_id: " << candidateId << "\n"
         << "// source: runtime globals from current analysis session\n";
  std::vector<std::pair<std::string, int>> vars(
      context.runtime_int_vars.begin(), context.runtime_int_vars.end());
  SanitizeFindEllipseCandidateGlobals(tool, caseId, scriptId, scriptPath,
                                      sourceEvidenceScriptPath, vars);
  std::sort(vars.begin(), vars.end());
  for (const auto &kv : vars) {
    if (kv.first.rfind("global_", 0) == 0)
      params << "int " << kv.first << " = " << kv.second << ";\n";
  }
  if (!preserveInputs && !WriteTextFile(parameterSnapshotPath, params.str())) {
    result.reason = "failed to write parameter_snapshot.cxsc";
    return false;
  }

  std::string writeReason;
  if (!preserveInputs &&
      !WriteGaugeSnapshotJson(context.current_gauge, gaugeAnnotationPath,
                              writeReason)) {
    result.reason = writeReason;
    return false;
  }

  if (!preserveInputs &&
      !WriteRuntimeGlobalsJson(context, tool, caseId, scriptId, scriptPath,
                               sourceEvidenceScriptPath, runtimeGlobalsPath,
                               writeReason)) {
    result.reason = writeReason;
    return false;
  }

  if (!WriteLineTraceJson(context, lineTracePath, writeReason)) {
    result.reason = writeReason;
    return false;
  }

  std::ostringstream analysisJson;
  analysisJson
      << "{\n"
      << "  \"schema_version\": 1,\n"
      << "  \"candidate_id\": \"" << JsonEscape(candidateId) << "\",\n"
      << "  \"mode\": \"" << JsonEscape(options.mode) << "\",\n"
      << "  \"request_run\": " << (options.request_run ? "true" : "false")
      << ",\n"
      << "  \"editor_dirty\": " << (context.editor_dirty ? "true" : "false")
      << ",\n"
      << "  \"editor_source\": \"" << JsonEscape(context.editor_source)
      << "\",\n"
      << "  \"loaded_script_path\": \""
      << JsonEscape(context.loaded_script_path) << "\",\n"
      << "  \"debug_action\": \"" << JsonEscape(context.debug_action) << "\",\n"
      << "  \"debug_status\": \"" << JsonEscape(context.debug_status) << "\",\n"
      << "  \"debug_reason\": \"" << JsonEscape(context.debug_reason) << "\",\n"
      << "  \"run_state\": \"" << JsonEscape(context.run_state) << "\",\n"
      << "  \"primary_object_type\": \""
      << JsonEscape(context.current_gauge.primary_object_type) << "\",\n"
      << "  \"primary_object_name\": \""
      << JsonEscape(context.current_gauge.primary_object_name) << "\",\n"
      << "  " << static_cast<char>(34) << "primary_object_status"
      << static_cast<char>(34) << ": " << static_cast<char>(34)
      << JsonEscape(context.current_gauge.primary_object_status)
      << static_cast<char>(34) << ',' << static_cast<char>(10)
      << "  " << static_cast<char>(34) << "manual_operation_trace_path"
      << static_cast<char>(34) << ": " << static_cast<char>(34)
      << JsonEscape(manualOperationTracePath.string()) << static_cast<char>(34)
      << ',' << static_cast<char>(10)
      << "  " << static_cast<char>(34) << "replay_trusted"
      << static_cast<char>(34) << ": " << (replayTrusted ? "true" : "false")
      << ',' << static_cast<char>(10)
      << "  " << static_cast<char>(34) << "replay_warning"
      << static_cast<char>(34) << ": " << static_cast<char>(34)
      << JsonEscape(replayWarning) << static_cast<char>(34)
      << static_cast<char>(10) << '}' << static_cast<char>(10);
  if (!WriteTextFile(analysisStatePath, analysisJson.str())) {
    result.reason = "failed to write analysis_state.json";
    return false;
  }

  const bool linkedResultAvailable =
      !options.linked_result_summary_path.empty();
  const bool linkedOverlayAvailable =
      !options.linked_result_overlay_path.empty() &&
      !options.linked_evidence_overlay_path.empty() &&
      !options.linked_tool_display_path.empty();

  std::ostringstream summaryJson;
  summaryJson << "{\n"
              << "  \"schema_version\": 1,\n"
              << "  \"candidate_id\": \"" << JsonEscape(candidateId) << "\",\n"
              << "  \"runtime_result\": \""
              << (linkedResultAvailable
                      ? "linked_runtime_result"
                      : (options.request_run ? "run_requested" : "pending"))
              << "\",\n"
              << "  \"status\": \""
              << JsonEscape(context.current_result_ref.status) << "\",\n"
              << "  \"reason\": \""
              << JsonEscape(context.current_result_ref.reason) << "\",\n"
              << "  \"debug_status\": \"" << JsonEscape(context.debug_status)
              << "\",\n"
              << "  \"debug_reason\": \"" << JsonEscape(context.debug_reason)
              << "\",\n"
              << "  \"linked_result_summary_path\": \""
              << JsonEscape(options.linked_result_summary_path) << "\",\n"
              << "  \"linked_result_overlay_path\": \""
              << JsonEscape(options.linked_result_overlay_path) << "\",\n"
              << "  \"linked_evidence_overlay_path\": \""
              << JsonEscape(options.linked_evidence_overlay_path) << "\",\n"
              << "  \"linked_tool_display_path\": \""
              << JsonEscape(options.linked_tool_display_path) << "\",\n"
              << "  \"asset_missing\": "
              << (linkedResultAvailable && linkedOverlayAvailable ? "false"
                                                                  : "true")
              << ",\n"
              << "  \"missing_assets\": "
              << (linkedResultAvailable && linkedOverlayAvailable
                      ? "[]"
                      : "[\"result_overlay.png\", \"evidence_overlay.png\", "
                        "\"tool_display.png\", \"object_state.json\", "
                        "\"variable_snapshot.json\"]")
              << ',' << static_cast<char>(10)
              << "  " << static_cast<char>(34) << "manual_operation_trace_path"
              << static_cast<char>(34) << ": " << static_cast<char>(34)
              << JsonEscape(manualOperationTracePath.string()) << static_cast<char>(34)
              << ',' << static_cast<char>(10)
              << "  " << static_cast<char>(34) << "replay_trusted"
              << static_cast<char>(34) << ": " << (replayTrusted ? "true" : "false")
              << ',' << static_cast<char>(10)
              << "  " << static_cast<char>(34) << "replay_warning"
              << static_cast<char>(34) << ": " << static_cast<char>(34)
              << JsonEscape(replayWarning) << static_cast<char>(34)
              << static_cast<char>(10) << '}' << static_cast<char>(10);
  if (!WriteTextFile(resultSummaryPath, summaryJson.str())) {
    result.reason = "failed to write result_summary.json";
    return false;
  }

  std::ostringstream bindingJson;
  bindingJson << "{\n"
              << "  \"schema_version\": 1,\n"
              << "  \"evidence_status\": \"to_review\",\n"
              << "  \"review_status\": \"pending_human_review\",\n"
              << "  \"candidate_id\": \"" << JsonEscape(candidateId) << "\",\n"
              << "  \"case_id\": \"" << JsonEscape(caseId) << "\",\n"
              << "  \"script_id\": \"" << JsonEscape(scriptId) << "\",\n"
              << "  \"script_path\": \"" << JsonEscape(scriptPath) << "\",\n"
              << "  \"script_snapshot_path\": \""
              << JsonEscape(scriptSnapshotPath.string()) << "\",\n"
              << "  \"source_evidence_script_path\": \""
              << JsonEscape(
                     ResolveWorkspaceFile(sourceEvidenceScriptPath).string())
              << "\",\n"
              << "  \"image_id\": \"" << JsonEscape(imageId) << "\",\n"
              << "  \"image_path\": \"" << JsonEscape(imagePath) << "\",\n"
              << "  \"target_id\": \"" << JsonEscape(targetId) << "\",\n"
              << "  \"tool\": \"" << JsonEscape(tool) << "\",\n"
              << "  \"parameter_summary\": \"" << JsonEscape(parameterSummary)
              << "\",\n"
              << "  \"parameter_snapshot_path\": \""
              << JsonEscape(parameterSnapshotPath.string()) << "\",\n"
              << "  \"gauge_annotation_path\": \""
              << JsonEscape(gaugeAnnotationPath.string()) << "\",\n"
              << "  \"analysis_state_path\": \""
              << JsonEscape(analysisStatePath.string()) << "\",\n"
              << "  \"result_summary_path\": \""
              << JsonEscape(resultSummaryPath.string()) << "\",\n"
              << "  \"linked_result_summary_path\": \""
              << JsonEscape(options.linked_result_summary_path) << "\",\n"
              << "  \"linked_result_overlay_path\": \""
              << JsonEscape(options.linked_result_overlay_path) << "\",\n"
              << "  \"linked_evidence_overlay_path\": \""
              << JsonEscape(options.linked_evidence_overlay_path) << "\",\n"
              << "  \"linked_tool_display_path\": \""
              << JsonEscape(options.linked_tool_display_path) << "\",\n"
              << "  \"runtime_globals_path\": \""
              << JsonEscape(runtimeGlobalsPath.string()) << "\",\n"
              << "  " << static_cast<char>(34) << "line_trace_path"
              << static_cast<char>(34) << ": " << static_cast<char>(34)
              << JsonEscape(lineTracePath.string()) << static_cast<char>(34)
              << ',' << static_cast<char>(10)
              << "  " << static_cast<char>(34) << "manual_operation_trace_path"
              << static_cast<char>(34) << ": " << static_cast<char>(34)
              << JsonEscape(manualOperationTracePath.string())
              << static_cast<char>(34) << ',' << static_cast<char>(10)
              << "  \"source\": \"manual_or_headless_analysis_save\",\n"
              << "  \"promotion_allowed\": false\n"
              << "}\n";
  if (!preserveInputs &&
      !WriteTextFile(evidenceBindingPath, bindingJson.str())) {
    result.reason = "failed to write evidence_binding.json";
    return false;
  }

  std::ostringstream log;
  log << "Evidence candidate saved\n"
      << "candidate_id=" << candidateId << "\n"
      << "case_id=" << caseId << "\n"
      << "script=" << scriptPath << "\n"
      << "image=" << imagePath << "\n"
      << "tool=" << tool << "\n"
      << "mode=" << options.mode << "\n";
  WriteTextFile(logPath, log.str());

  result.ok = true;
  result.candidate_id = candidateId;
  result.candidate_dir = candidateDir.string();
  result.evidence_binding_path = evidenceBindingPath.string();
  result.script_snapshot_path = scriptSnapshotPath.string();
  result.parameter_snapshot_path = parameterSnapshotPath.string();
  result.gauge_annotation_path = gaugeAnnotationPath.string();
  result.analysis_state_path = analysisStatePath.string();
  result.result_summary_path = resultSummaryPath.string();

  AppendEvidenceCandidateStateProbe(
      context, result.candidate_dir, result.candidate_id,
      "candidate_package_written", "written",
      "script_snapshot, parameter_snapshot, runtime_globals, gauge_annotation "
      "and evidence_binding written");
  AppendManualOperationTraceJsonl(
      context, result.candidate_dir, result.candidate_id,
      "candidate_package_written", "written",
      "script_snapshot, parameter_snapshot, runtime_globals, gauge_annotation "
      "and evidence_binding written",
      caseId, scriptId, imageId, imagePath, targetId, tool,
      ResolveWorkspaceFile(sourceEvidenceScriptPath).string(),
      result.script_snapshot_path, result.parameter_snapshot_path,
      runtimeGlobalsPath.string(), result.gauge_annotation_path,
      result.result_summary_path, replayTrusted, replayWarning);

  ClearManualOperationTraceEvents(context);

  if (context.current_evidence_selection.valid) {
    context.current_evidence_selection.case_id = caseId;
    context.current_evidence_selection.candidate_id = result.candidate_id;
    context.current_evidence_selection.candidate_dir = result.candidate_dir;
    context.current_evidence_selection.evidence_binding_path =
        result.evidence_binding_path;
    context.current_evidence_selection.parameter_snapshot_path =
        result.parameter_snapshot_path;
    context.current_evidence_selection.runtime_globals_path =
        runtimeGlobalsPath.string();
    context.current_evidence_selection.gauge_annotation_path =
        result.gauge_annotation_path;
    context.current_evidence_selection.working_script_snapshot_path =
        result.script_snapshot_path;
    context.current_evidence_selection.has_saved_state = true;
    context.current_evidence_selection.source_evidence_script_path =
        ResolveWorkspaceFile(sourceEvidenceScriptPath).string();
    context.current_evidence_selection.parameter_summary = parameterSummary;
    context.current_evidence_selection.parameter_profile_id = parameterSummary;
    context.current_evidence_selection.primary_object_type =
        context.current_gauge.primary_object_type;
    context.current_evidence_selection.primary_object_name =
        context.current_gauge.primary_object_name;
    context.current_evidence_selection.primary_object_status =
        context.current_gauge.primary_object_status;
    context.current_evidence_selection.status = "pending_human_review";
    context.current_evidence_selection.reason =
        "current case state updated; working_state=" + result.candidate_id;
  }

  {
    const bool caseRowAdded = AppendCandidateToEvidenceChain(
        context, result, caseId, scriptId, scriptSnapshotPath.string(),
        ResolveWorkspaceFile(sourceEvidenceScriptPath).string(), imageId,
        imagePath, targetId, tool, parameterSummary,
        options.add_to_evidence_chain);
    AppendEvidenceCandidateStateProbe(
        context, result.candidate_dir, result.candidate_id,
        "current_case_updated", "available",
        "Development case current state was written and bound by case_id");
    if (caseRowAdded) {
      AppendEvidenceCandidateStateProbe(
          context, result.candidate_dir, result.candidate_id,
          "case_row_added", "available",
          "current Development case was added to Evidence Chain");
    } else {
      AppendEvidenceCandidateStateProbe(
        context, result.candidate_dir, result.candidate_id,
        "case_row_updated", "stable_list",
          "current Development case was bound to the existing Evidence row; "
          "case list remains unchanged");
    }
  }
  context.editor_dirty = false;
  context.debug_action = options.request_run ? "Save And Run Current Case"
                                             : "Save Current Case";
  context.debug_status = options.request_run
                             ? "CURRENT_CASE_RUN_REQUESTED"
                             : "CURRENT_CASE_SAVED";
  context.debug_reason = result.candidate_dir;
  // Only an initial save owns the pending-candidate slot.  A post-run update
  // must not replace it or make later ordinary Run actions update a stale
  // candidate package.
  if (options.add_to_evidence_chain || options.request_run) {
    context.last_evidence_candidate_id = result.candidate_id;
    context.last_evidence_candidate_dir = result.candidate_dir;
    context.last_evidence_candidate_reason = result.reason;
  }
  if (options.request_run)
    context.run_state = "running";

  return true;
}

bool SaveCasePackage(ManualTestContext &context, const std::string &caseName,
                     const std::string &outputDir, std::string &outPath,
                     std::string &outReason) {
  CxEvidenceCandidateSaveOptions options;
  options.root_dir =
      outputDir.empty() ? "cxscript_runs/evidence_candidates" : outputDir;
  options.candidate_id = caseName.empty() ? std::string() : caseName;
  options.mode = "legacy_case_package";
  options.request_run = false;
  options.add_to_evidence_chain = false;

  CxEvidenceCandidateSaveResult result;
  if (!SaveEvidenceCandidatePackage(context, options, result)) {
    outPath.clear();
    outReason = result.reason;
    return false;
  }
  outPath = result.candidate_dir;
  outReason.clear();
  return true;
}
