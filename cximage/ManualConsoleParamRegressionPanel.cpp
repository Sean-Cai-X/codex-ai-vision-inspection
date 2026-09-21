#include "CxParameterProfileRuntime.h"
#include "CxScriptCasePackageWriter.h"
#include "CxUnifiedLog.h"
#include "FindCircle.h"
#include "FindEllipse.h"

#include "ManualConsoleGauge.h"
#include "ManualConsoleParamRegressionPanel.h"
#include "ManualConsoleScriptDebugPanel.h"
#include "ManualConsoleUtils.h"
#include "metrology_analytics/CxBoundaryResponse.h"
#include "metrology_analytics/CxMetrologyUiGlobals.h"
#include "metrology_analytics/CxSurfaceBasicStats.h"
#include "metrology_analytics/CxSurfaceLevelPlane.h"
#include "metrology_analytics/CxSyntheticSurfaceFactory.h"
#include "pch.h"

#include <algorithm>
#include <cctype>
#include <cfloat>

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <limits>
#include <map>
#include <random>
#include <sstream>
#include <vector>

static std::string NormalizeKeyParamToolTypeLocal(const std::string &type);
static bool FastMatchScriptSupportsNormalTrace(const ManualTestContext &context);

static void SyncSegmentationLegacyPointFromLists(ManualGaugeState &gauge) {
  gauge.has_segmentation_positive_point =
      !gauge.segmentation_positive_points.empty();
  if (gauge.has_segmentation_positive_point) {
    const auto &point = gauge.segmentation_positive_points.back();
    gauge.segmentation_positive_x = point.x;
    gauge.segmentation_positive_y = point.y;
  } else {
    gauge.segmentation_positive_x = 0;
    gauge.segmentation_positive_y = 0;
  }

  gauge.has_segmentation_negative_point =
      !gauge.segmentation_negative_points.empty();
  if (gauge.has_segmentation_negative_point) {
    const auto &point = gauge.segmentation_negative_points.back();
    gauge.segmentation_negative_x = point.x;
    gauge.segmentation_negative_y = point.y;
  } else {
    gauge.segmentation_negative_x = 0;
    gauge.segmentation_negative_y = 0;
  }
}

static void
SeedSegmentationPromptListsFromLegacyFields(ManualGaugeState &gauge) {
  if (gauge.has_segmentation_positive_point &&
      gauge.segmentation_positive_points.empty()) {
    ManualSegmentationPromptPoint point;
    point.ref = "legacy_positive_0";
    point.x = gauge.segmentation_positive_x;
    point.y = gauge.segmentation_positive_y;
    gauge.segmentation_positive_points.push_back(point);
  }
  if (gauge.has_segmentation_negative_point &&
      gauge.segmentation_negative_points.empty()) {
    ManualSegmentationPromptPoint point;
    point.ref = "legacy_negative_0";
    point.x = gauge.segmentation_negative_x;
    point.y = gauge.segmentation_negative_y;
    gauge.segmentation_negative_points.push_back(point);
  }
  SyncSegmentationLegacyPointFromLists(gauge);
}

static bool DrawSegmentationPromptPointList(
    const char *label, const ImVec4 &color,
    std::vector<ManualSegmentationPromptPoint> &points) {
  bool edited = false;
  ImGui::PushID(label);
  const std::string header =
      std::string(label) + " points (" + std::to_string(points.size()) + ")";
  if (!ImGui::CollapsingHeader(header.c_str(),
                               ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::PopID();
    return false;
  }

  if (points.empty()) {
    ImGui::TextDisabled("No %s prompt points yet.", label);
    ImGui::PopID();
    return false;
  }

  const std::string tableId =
      std::string("findsegmentation_") + label + "_point_list";
  if (ImGui::BeginTable(tableId.c_str(), 5,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_SizingStretchProp)) {
    ImGui::TableSetupColumn("type");
    ImGui::TableSetupColumn("#");
    ImGui::TableSetupColumn("x");
    ImGui::TableSetupColumn("y");
    ImGui::TableSetupColumn("action");
    ImGui::TableHeadersRow();

    int removeIndex = -1;
    for (int i = 0; i < static_cast<int>(points.size()); ++i) {
      ImGui::PushID(i);
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextColored(color, "%s", label);
      ImGui::TableSetColumnIndex(1);
      ImGui::Text("%d", i);
      ImGui::TableSetColumnIndex(2);
      ImGui::SetNextItemWidth(-FLT_MIN);
      edited |= ImGui::InputInt("##x", &points[i].x);
      points[i].x = std::max(0, points[i].x);
      ImGui::TableSetColumnIndex(3);
      ImGui::SetNextItemWidth(-FLT_MIN);
      edited |= ImGui::InputInt("##y", &points[i].y);
      points[i].y = std::max(0, points[i].y);
      ImGui::TableSetColumnIndex(4);
      if (ImGui::SmallButton("remove")) {
        removeIndex = i;
        edited = true;
      }
      ImGui::PopID();
    }
    ImGui::EndTable();
    if (removeIndex >= 0 && removeIndex < static_cast<int>(points.size())) {
      points.erase(points.begin() + removeIndex);
    }
  }

  ImGui::PopID();
  return edited;
}

CxParamRegressionTask
BuildParamRegressionTaskFromManualGauge(const ManualTestContext &context) {
  const ManualGaugeState &gauge = context.current_gauge;
  CxParamRegressionTask task;
  task.case_id = gauge.case_id.empty() ? "manual_case" : gauge.case_id;
  task.image_id = gauge.image_id;
  task.target_id = gauge.target_id;
  task.tool = gauge.tool.empty() ? "FindLine" : gauge.tool;
  task.task_id = "param_regression_" + task.case_id;
  task.gauge_annotation_path =
      (ManualGaugeCaseDir(context) / "gauge_annotation.json").string();
  task.base_script_id = task.tool == "FindCircle"
                            ? "findcircle_stage25_direct_ok"
                            : "findline_stage25_filter20_ok";
  task.base_parameter_profile_id = "manual_gauge_seed";
  task.max_candidates = context.param_regression.max_candidates;
  task.max_case_seconds = context.param_regression.max_case_seconds;
  task.max_total_seconds = context.param_regression.max_total_seconds;
  task.require_manual_gauge = true;
  task.allow_mlpack_rank = true;
  task.allow_ensmallen_opt = true;
  task.allow_promote = false;
  return task;
}

CxParamEvalRecord BuildManualSeedEvalRecord(const ManualTestContext &context,
                                            const CxParamRegressionTask &task) {
  const ResultRefView &result = context.current_result_ref;
  CxParamEvalRecord record;
  record.candidate_id = "manual_seed";
  record.case_id = task.case_id;
  record.tool = task.tool;
  record.executed = true;
  if (task.tool == "FindCircle") {
    record.points = result.valid_points_count > 0 ? result.valid_points_count
                                                  : result.points_count;
    record.fit_available = result.fit_radius > 0.0f ||
                           result.status == "geometry_result_available";
    record.mean_distance = result.avgdist;
  } else {
    record.points =
        result.valid_line_points_count > 0
            ? result.valid_line_points_count
            : (result.line_points_count > 0 ? result.line_points_count
                                            : result.valid_points_count);
    record.fit_available =
        result.line_result_status == "geometry_result_available" ||
        result.status == "geometry_result_available" ||
        (result.line_x0 != result.line_x1 || result.line_y0 != result.line_y1);
    record.mean_distance = result.line_avgdist;
  }
  record.support_score = record.points > 0 ? 1.0 : 0.0;
  record.failure_stage = record.fit_available ? "" : "pending_probe_or_no_fit";
  record.classification = record.fit_available
                              ? "manual_seed_geometry_available"
                              : "manual_seed_needs_probe";
  const std::filesystem::path case_dir = ManualGaugeCaseDir(context);
  record.result_summary_path = (case_dir / "result_summary.json").string();
  record.tool_display_path = (case_dir / "tool_display.png").string();
  record.replay_package_path = (case_dir / "replay_package.json").string();
  record.image_id = task.image_id;
  record.target_id = task.target_id;
  record.image_path = context.image_file_path;
  record.script_path = context.loaded_script_path.empty() ? context.script_file_path
                                                         : context.loaded_script_path;
  record.timeout_seconds = context.param_regression.max_case_seconds;
  record.roi_x0 = context.current_gauge.line_x0;
  record.roi_y0 = context.current_gauge.line_y0;
  record.roi_x1 = context.current_gauge.line_x1;
  record.roi_y1 = context.current_gauge.line_y1;
  record.tool_half_width = context.current_gauge.tool_half_width;
  record.max_elapsed_ms = 5000;
  record.max_scan_lines = 4096;
  record.max_samples = 200000;
  record.method = context.current_gauge.method;
  record.threshold = context.current_gauge.threshold;
  record.gap = context.current_gauge.gap;
  record.linegap = context.current_gauge.linegap;
  record.min_edge_run_width_px =
      context.current_gauge.min_edge_run_width_px;
  record.wgap = context.current_gauge.wgap;
  record.hgap = context.current_gauge.hgap;
  record.filterprofile = context.current_gauge.filterprofile;
  record.samplerate = 1;
  record.find_num = 1;

  return record;
}

CxParamAccuracyStats
BuildManualSeedAccuracyStats(const CxParamRegressionTask &task,
                             const CxParamEvalRecord &record,
                             const ManualGaugeState &gauge) {
  CxParamAccuracyStats stats;
  stats.candidate_id = "manual_seed";
  stats.tool = task.tool;
  stats.total_cases = 1;
  stats.executed_cases = record.executed ? 1 : 0;
  stats.timeout_cases = record.timeout ? 1 : 0;
  stats.geometry_pass = record.fit_available ? 1 : 0;
  stats.evidence_pass = record.points > 0 ? 1 : 0;
  stats.human_accept = ManualGaugeAcceptedForParamRegression(gauge) ? 1 : 0;
  stats.geometry_pass_rate = static_cast<double>(stats.geometry_pass);
  stats.evidence_pass_rate = static_cast<double>(stats.evidence_pass);
  stats.human_accept_rate = static_cast<double>(stats.human_accept);
  stats.avg_support_score = record.support_score;
  stats.avg_mean_distance = record.mean_distance;
  stats.avg_fit_offset = record.fit_offset;
  stats.stability_score = record.fit_available ? 0.5 : 0.0;
  stats.risk_score = record.fit_available ? 0.4 : 0.8;
  return stats;
}

bool InitializeParamRegressionFromGauge(ManualTestContext &context,
                                        std::string &reason) {
  ManualParamRegressionState &state = context.param_regression;
  if (!ValidateParamRegressionPrerequisites(context, reason)) {
    state.initialized = false;
    state.status = "blocked";
    state.reason = reason;
    return false;
  }

  state.task = BuildParamRegressionTaskFromManualGauge(context);
  state.range_set = MakeConservativeRangeSet(state.task.tool);
  state.range_set.max_candidates = state.max_candidates;
  state.range_set.max_case_seconds = state.max_case_seconds;
  state.range_set.max_total_seconds = state.max_total_seconds;
  state.candidates =
      GenerateBasicParamCandidates(state.range_set, state.max_candidates);
  state.records.clear();
  CxParamEvalRecord manual_seed =
      BuildManualSeedEvalRecord(context, state.task);
  state.records.push_back(manual_seed);
  state.accuracy_stats.clear();
  state.accuracy_stats.push_back(BuildManualSeedAccuracyStats(
      state.task, manual_seed, context.current_gauge));
  state.output_dir =
      (ManualGaugeCaseDir(context) / "param_regression").string();
  state.initialized = true;
  state.status = "ready";
  state.reason = "Manual gauge accepted. Phase 1 can export parameter range, "
                 "candidates, and evidence reports.";
  reason.clear();
  return true;
}

CxParamCandidate CandidateFromManualGauge(const ManualGaugeState &gauge,
                                          const std::string &id,
                                          const std::string &source) {
  CxParamCandidate c;
  c.candidate_id = id;
  c.source = source;
  c.method = gauge.method;
  c.threshold = gauge.threshold;
  c.gap = gauge.gap;
  c.linegap = gauge.linegap;
  c.min_edge_run_width_px =
      std::max(1, std::min(20, gauge.min_edge_run_width_px));
  c.wgap = gauge.wgap;
  c.hgap = gauge.hgap;
  c.filterprofile = gauge.filterprofile;
  c.samplerate = 1;
  c.predicted_quality = source == "manual_seed" ? 0.65 : 0.55;
  c.predicted_risk = source == "manual_seed" ? 0.30 : 0.45;
  c.predicted_failure_class = "pending_probe";
  c.selected_for_probe = true;
  return c;
}

void AddMlpackRankPlaceholderCandidates(ManualParamRegressionState &state) {
  (void)state;
}

void AddEnsmallenOptPlaceholderCandidates(ManualParamRegressionState &state) {
  (void)state;
}

void RefreshParamRegressionExportedFiles(ManualParamRegressionState &state) {
  state.exported_files.clear();
  const std::filesystem::path root(state.output_dir);
  const char *names[] = {
      "param_regression_task.json",     "param_range_report.json",
      "param_range_report.csv",         "param_range_report.md",
      "param_candidates.json",          "param_candidates.csv",
      "param_eval_records.jsonl",       "hit_distribution.json",
      "hit_distribution.csv",           "param_hit_distribution_report.md",
      "param_accuracy_matrix.json",     "param_accuracy_matrix.csv",
      "param_accuracy_matrix.md",       "param_candidate_distribution.md",
      "param_optimization_trace.json",  "param_stability_report.md",
      "param_recommendation_report.md", "param_profile_promotion_gate.md",
      "param_profile_candidate.cxsc",   "manual_acceptance_checklist.md"};
  for (const char *name : names) {
    const std::filesystem::path path = root / name;
    if (std::filesystem::exists(path))
      state.exported_files.push_back(path.string());
  }
}

bool ExportParamRegressionManualAcceptanceChecklist(
    const ManualTestContext &context, const std::filesystem::path &path,
    std::string &reason) {
  const ManualParamRegressionState &reg = context.param_regression;
  const ManualGaugeState &gauge = context.current_gauge;
  std::filesystem::create_directories(path.parent_path());
  std::ofstream file(path);
  if (!file.is_open()) {
    reason = "failed to write manual acceptance checklist";
    return false;
  }

  file << "# Parameter Regression Manual Acceptance Checklist\n\n";
  file << "## Current Context\n\n";
  file << "- Case: `" << gauge.case_id << "`\n";
  file << "- Image: `" << gauge.image_id << "`\n";
  file << "- Target: `" << gauge.target_id << "`\n";
  file << "- Tool: `" << gauge.tool << "`\n";
  file << "- Gauge Review Status: `" << gauge.review_status << "`\n";
  file << "- Gauge Accepted: `"
       << (ManualGaugeAcceptedForParamRegression(gauge) ? "yes" : "no")
       << "`\n";
  file << "- Output Dir: `" << reg.output_dir << "`\n\n";

  file << "## Checklist\n\n";
  file << "- [ ] Manual gauge position/direction/width or circle ring is "
          "correct.\n";
  file << "- [ ] `Apply Gauge To Globals` has been used before probe/replay.\n";
  file << "- [ ] Candidate table contains manual seed and selected probe "
          "candidates.\n";
  file << "- [ ] Candidate parameter values are visible and editable in UI.\n";
  file << "- [ ] `param_candidates.json/csv` matches UI candidate table.\n";
  file << "- [ ] `param_eval_records.jsonl` contains manual seed evidence or "
          "selected probe result after probe.\n";
  file << "- [ ] `param_hit_distribution_report.md` is present for evidence "
          "review.\n";
  file << "- [ ] `param_accuracy_matrix.md/json/csv` is present for stability "
          "review.\n";
  file << "- [ ] `param_profile_promotion_gate.md` says promotion is disabled "
          "unless mini-regression passes.\n";
  file << "- [ ] `param_profile_candidate.cxsc` is diagnostic-only, not "
          "baseline.\n\n";

  file << "## Candidate Snapshot\n\n";
  file << "| Index | Candidate | Source | Selected | Method | Threshold | Gap "
          "| LineGap | MinRunPx | WGap | HGap | Filter | Risk |\n";
  file << "|---:|---|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|\n";
  for (std::size_t i = 0; i < reg.candidates.size(); ++i) {
    const auto &c = reg.candidates[i];
    file << "| " << i << " | " << c.candidate_id << " | " << c.source << " | "
         << (c.selected_for_probe ? "yes" : "no") << " | " << c.method << " | "
         << c.threshold << " | " << c.gap << " | " << c.linegap << " | "
         << c.min_edge_run_width_px << " | " << c.wgap << " | "
         << c.hgap << " | " << c.filterprofile << " | "
         << c.predicted_risk << " |\n";
  }
  reason.clear();
  return true;
}

bool IsFindLineFindCircleContext(ManualTestContext &context) {
  const ManualGaugeState &g = context.current_gauge;
  return (g.tool == "FindLine" || g.tool == "FindCircle" ||
          g.tool == "FindEllipse" || g.tool == "FindRect" ||
          g.tool == "FindObject" || g.tool == "FindSegmentation" ||
          g.tool == "FastMatch" || g.tool == "fastmatch" ||
          g.tool == "CFastMatch" || g.tool == "GridPatternClassTool" ||
          g.tool == "RegionPatternTool") ||
         g.has_line_gauge || g.has_circle_gauge || g.has_ellipse_gauge ||
         g.has_findobject_roi;
}

static std::string ToLowerAsciiLocal(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return text;
}

static bool ContainsNoCaseLocal(const std::string &text,
                                const std::string &needle) {
  return ToLowerAsciiLocal(text).find(ToLowerAsciiLocal(needle)) !=
         std::string::npos;
}

static std::string ReadEvidenceParameterTokenLocal(const std::string &summary,
                                                   const std::string &key) {
  std::istringstream tokens(summary);
  std::string token;
  const std::string prefix = key + "=";
  while (tokens >> token) {
    if (token.rfind(prefix, 0) == 0)
      return token.substr(prefix.size());
  }
  return {};
}

static std::string
ExplicitTorchEvidenceFeatureLocal(const ManualTestContext &context) {
  const std::string feature = ToLowerAsciiLocal(ReadEvidenceParameterTokenLocal(
      context.current_evidence_selection.parameter_summary, "torch_feature"));
  if (feature == "instance_segmentation" ||
      feature == "semantic_segmentation" || feature == "object_detection" ||
      feature == "classification" || feature == "anomaly_detection" ||
      feature == "ocr" || feature == "training_lifecycle") {
    return feature;
  }
  return {};
}

static const RuntimeObjectView *
FindPrimaryTorchRuntimeObjectLocal(const ManualTestContext &context) {
  const std::string &primary = context.current_gauge.primary_object_name;
  if (!primary.empty()) {
    for (const RuntimeObjectView &object : context.runtime_objects) {
      if ((object.type == "TorchTask" || object.type == "FindSegmentation") &&
          object.name == primary) {
        return &object;
      }
    }
  }

  for (const RuntimeObjectView &object : context.runtime_objects) {
    if (object.type == "TorchTask")
      return &object;
  }
  for (const RuntimeObjectView &object : context.runtime_objects) {
    if (object.type == "FindSegmentation")
      return &object;
  }
  return nullptr;
}

bool IsTorchContext(const ManualTestContext &context) {
  const std::string tool = ToLowerAsciiLocal(context.current_gauge.tool);
  if (tool == "findsegmentation" || tool == "torchtask" || tool == "torch" ||
      tool == "segmentation")
    return true;

  for (const RuntimeObjectView &object : context.runtime_objects) {
    if (object.type == "TorchTask" || object.type == "FindSegmentation")
      return true;
  }

  return ContainsNoCaseLocal(context.loaded_script_path, "torch") ||
         ContainsNoCaseLocal(context.loaded_script_path, "find_segmentation") ||
         ContainsNoCaseLocal(context.editor_text, "TorchTask") ||
         ContainsNoCaseLocal(context.editor_text, "FindSegmentation") ||
         ContainsNoCaseLocal(context.editor_text, "torch.");
}

static const TorchTrainingImageItem *
CurrentTorchTrainingImageLocal(const ManualTestContext &context) {
  const int index = context.selected_torch_training_image;
  if (index < 0 ||
      index >= static_cast<int>(context.torch_training_images.size()))
    return nullptr;
  return &context.torch_training_images[static_cast<std::size_t>(index)];
}

static std::string TorchTaskFeatureLocal(const ManualTestContext &context) {
  if (context.editor_source == "torch_runtime_action") {
    const std::string actionKey = ToLowerAsciiLocal(
        context.loaded_script_path + " " + context.editor_text + " " +
        context.active_script_case_purpose);
    if (actionKey.find("train") != std::string::npos)
      return "training_lifecycle";
    if (actionKey.find("detect") != std::string::npos ||
        actionKey.find("yolo") != std::string::npos)
      return "object_detection";
    if (actionKey.find("segment") != std::string::npos ||
        actionKey.find("deeplab") != std::string::npos)
      return "semantic_segmentation";
  }

  const std::string explicitFeature =
      ExplicitTorchEvidenceFeatureLocal(context);
  if (!explicitFeature.empty())
    return explicitFeature;

  std::string source = context.current_evidence_selection.script_id + " " +
                       context.current_evidence_selection.script_path + " " +
                       context.current_evidence_selection.tool;
  if (source.find_first_not_of(" \t\r\n") == std::string::npos)
    source = context.loaded_script_path + " " + context.editor_text;
  const std::string key = ToLowerAsciiLocal(source);
  if (key.find("train") != std::string::npos)
    return "training_lifecycle";
  if (key.find("instance_seg") != std::string::npos)
    return "instance_segmentation";
  if (key.find("segment") != std::string::npos ||
      key.find("deeplab") != std::string::npos ||
      key.find("mask") != std::string::npos)
    return "semantic_segmentation";
  if (key.find("detect") != std::string::npos ||
      key.find("yolo") != std::string::npos)
    return "object_detection";
  if (key.find("classif") != std::string::npos ||
      key.find("resnet18") != std::string::npos ||
      key.find("resnet50") != std::string::npos)
    return "classification";
  if (key.find("anomaly") != std::string::npos)
    return "anomaly_detection";
  if (key.find("ocr") != std::string::npos)
    return "ocr";
  return "runtime_contract";
}

static std::string
TorchSelectedEvidenceFeatureLocal(const ManualTestContext &context) {
  const std::string explicitFeature =
      ExplicitTorchEvidenceFeatureLocal(context);
  if (!explicitFeature.empty())
    return explicitFeature;

  std::string source = context.current_evidence_selection.script_id + " " +
                       context.current_evidence_selection.script_path + " " +
                       context.current_evidence_selection.tool;
  if (source.find_first_not_of(" \t\r\n") == std::string::npos)
    return "runtime_contract";

  const std::string key = ToLowerAsciiLocal(source);
  if (key.find("train") != std::string::npos)
    return "training_lifecycle";
  if (key.find("instance_seg") != std::string::npos)
    return "instance_segmentation";
  if (key.find("segment") != std::string::npos ||
      key.find("deeplab") != std::string::npos ||
      key.find("mask") != std::string::npos)
    return "semantic_segmentation";
  if (key.find("detect") != std::string::npos ||
      key.find("yolo") != std::string::npos)
    return "object_detection";
  if (key.find("classif") != std::string::npos ||
      key.find("resnet18") != std::string::npos ||
      key.find("resnet50") != std::string::npos)
    return "classification";
  if (key.find("anomaly") != std::string::npos)
    return "anomaly_detection";
  if (key.find("ocr") != std::string::npos)
    return "ocr";
  return "runtime_contract";
}

static void ApplyTorchParameterDefaultsLocal(ManualTestContext &context,
                                             const std::string &feature) {
  const std::string profileKey =
      context.current_evidence_selection.case_id + "|" + feature;
  if (profileKey.empty() || context.torch_parameter_defaults_key == profileKey)
    return;

  int inputWidth = 512;
  int inputHeight = 512;
  int confidence = 50;
  int maskThreshold = 50;
  int maxDetections = 100;
  int epochs = 20;
  int batchSize = 4;
  const std::string evidenceKey =
      ToLowerAsciiLocal(context.current_evidence_selection.case_id + " " +
                        context.current_evidence_selection.script_id + " " +
                        context.current_evidence_selection.script_path);
  const int featurePyramid =
      evidenceKey.find("feature") != std::string::npos ? 1 : 0;

  if (feature == "object_detection") {
    inputWidth = 640;
    inputHeight = 640;
    confidence = 25;
    epochs = 50;
    batchSize = 8;
  } else if (feature == "instance_segmentation") {
    inputWidth = 640;
    inputHeight = 640;
    confidence = 25;
    maskThreshold = 50;
    epochs = 50;
    batchSize = 4;
  } else if (feature == "semantic_segmentation") {
    inputWidth = 512;
    inputHeight = 512;
    maskThreshold = 50;
    epochs = 40;
    batchSize = 4;
  } else if (feature == "classification") {
    inputWidth = 224;
    inputHeight = 224;
    epochs = 30;
    batchSize = 16;
  } else if (feature == "anomaly_detection") {
    inputWidth = 256;
    inputHeight = 256;
    epochs = 30;
    batchSize = 8;
  } else if (feature == "ocr") {
    inputWidth = 320;
    inputHeight = 96;
    epochs = 30;
    batchSize = 16;
  } else if (feature == "training_lifecycle") {
    inputWidth = 128;
    inputHeight = 128;
    epochs = 1;
    batchSize = 2;
  }

  auto injectIfMissing = [&](const std::string &key, int value) {
    if (context.runtime_int_vars.find(key) == context.runtime_int_vars.end())
      InjectManualGaugeInt(context, key, value);
  };
  injectIfMissing("global_torch_input_width", inputWidth);
  injectIfMissing("global_torch_input_height", inputHeight);
  injectIfMissing("global_torch_confidence_percent", confidence);
  injectIfMissing("global_torch_iou_threshold_percent", 45);
  injectIfMissing("global_torch_mask_threshold_percent", maskThreshold);
  injectIfMissing("global_torch_max_detections", maxDetections);
  injectIfMissing("global_torch_epochs", epochs);
  injectIfMissing("global_torch_batch_size", batchSize);
  injectIfMissing("global_torch_num_classes", 7);
  injectIfMissing("global_torch_top_k", 5);
  injectIfMissing("global_torch_normalize_input", 1);
  injectIfMissing("global_torch_feature_pyramid_enabled", featurePyramid);
  context.torch_parameter_defaults_key = profileKey;
  auto actualValue = [&](const std::string &key) {
    const auto it = context.runtime_int_vars.find(key);
    return it == context.runtime_int_vars.end() ? 0 : it->second;
  };
  const bool evidenceBound =
      !ExplicitTorchEvidenceFeatureLocal(context).empty();
  CXLOG_INFO(
      "TorchKeyParameters", "torch_parameter_defaults_applied", "ui_event",
      "profile=" + feature +
          " evidence_bound=" + (evidenceBound ? "true" : "false") +
          " input=" + std::to_string(actualValue("global_torch_input_width")) +
          "x" + std::to_string(actualValue("global_torch_input_height")) +
          " confidence_percent=" +
          std::to_string(actualValue("global_torch_confidence_percent")) +
          " iou_percent=" +
          std::to_string(actualValue("global_torch_iou_threshold_percent")) +
          " mask_percent=" +
          std::to_string(actualValue("global_torch_mask_threshold_percent")) +
          " max_detections=" +
          std::to_string(actualValue("global_torch_max_detections")) +
          " num_classes=" +
          std::to_string(actualValue("global_torch_num_classes")) + " epochs=" +
          std::to_string(actualValue("global_torch_epochs")) + " batch_size=" +
          std::to_string(actualValue("global_torch_batch_size")) +
          " result_count=" +
          std::to_string(actualValue("global_torch_result_count")) +
          " training_step=" +
          std::to_string(actualValue("global_torch_training_step_executed")));
}

static bool TorchAnnotationBoundsLocal(const TorchTrainingImageItem &item,
                                       int &x0, int &y0, int &x1, int &y1) {
  double minX = 0.0;
  double minY = 0.0;
  double maxX = 0.0;
  double maxY = 0.0;
  bool available = false;
  auto includePoint = [&](double x, double y) {
    if (!available) {
      minX = maxX = x;
      minY = maxY = y;
      available = true;
      return;
    }
    minX = std::min(minX, x);
    minY = std::min(minY, y);
    maxX = std::max(maxX, x);
    maxY = std::max(maxY, y);
  };

  for (const TorchTrainingAnnotationShapeSnapshot &shape :
       item.annotation_shapes) {
    if (!shape.visible || shape.result_element)
      continue;
    for (std::size_t i = 0; i + 1 < shape.points_xy.size(); i += 2)
      includePoint(shape.points_xy[i], shape.points_xy[i + 1]);
    if (shape.radius > 0.0) {
      includePoint(shape.center_x - shape.radius,
                   shape.center_y - shape.radius);
      includePoint(shape.center_x + shape.radius,
                   shape.center_y + shape.radius);
    }
    if (shape.radius_x > 0.0 || shape.radius_y > 0.0) {
      includePoint(shape.center_x - shape.radius_x,
                   shape.center_y - shape.radius_y);
      includePoint(shape.center_x + shape.radius_x,
                   shape.center_y + shape.radius_y);
    }
  }

  if (!available)
    return false;
  x0 = std::max(0, static_cast<int>(std::floor(minX)));
  y0 = std::max(0, static_cast<int>(std::floor(minY)));
  x1 = std::max(x0 + 1, static_cast<int>(std::ceil(maxX)));
  y1 = std::max(y0 + 1, static_cast<int>(std::ceil(maxY)));
  return true;
}

static std::string
TorchShapeGeometryTextLocal(const TorchTrainingAnnotationShapeSnapshot &shape) {
  std::ostringstream out;
  if (!shape.points_xy.empty()) {
    out << "points=" << shape.points_xy.size() / 2;
    if (shape.points_xy.size() >= 2)
      out << " first=(" << static_cast<int>(std::lround(shape.points_xy[0]))
          << "," << static_cast<int>(std::lround(shape.points_xy[1])) << ")";
  } else if (shape.radius > 0.0) {
    out << "C=(" << static_cast<int>(std::lround(shape.center_x)) << ","
        << static_cast<int>(std::lround(shape.center_y))
        << ") R=" << static_cast<int>(std::lround(shape.radius));
  } else if (shape.radius_x > 0.0 || shape.radius_y > 0.0) {
    out << "C=(" << static_cast<int>(std::lround(shape.center_x)) << ","
        << static_cast<int>(std::lround(shape.center_y))
        << ") Rx/Ry=" << static_cast<int>(std::lround(shape.radius_x)) << "/"
        << static_cast<int>(std::lround(shape.radius_y));
  } else {
    out << "geometry snapshot available";
  }
  return out.str();
}

static bool DrawRuntimeIntRow(ManualTestContext &context, const char *label,
                              const char *key, int fallback, int minValue,
                              int maxValue, float labelWidth);

static void DrawReadonlyFieldLocal(const char *label,
                                   const std::string &value) {
  ImGui::Text("%s: %s", label, value.empty() ? "-" : value.c_str());
}

static void DrawReadonlyFieldLocal(const char *label, int value) {
  ImGui::Text("%s: %d", label, value);
}

static void DrawReadonlyFieldLocal(const char *label, double value) {
  ImGui::Text("%s: %.3f", label, value);
}

static void DrawPendingBindingLineLocal(const char *label, const char *reason) {
  ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.25f, 1.0f), "%s: pending_binding",
                     label);
  if (reason != nullptr && reason[0] != '\0')
    ImGui::SameLine(), ImGui::TextDisabled("(%s)", reason);
}

struct TorchCurveSampleLocal {
  std::string label;
  double value = 0.0;
};

static bool ReadTorchTextFileLocal(const std::string &path, std::string &text) {
  text.clear();
  if (path.empty())
    return false;
  std::ifstream input(path, std::ios::binary);
  if (!input.is_open())
    return false;
  std::ostringstream buffer;
  buffer << input.rdbuf();
  text = buffer.str();
  return true;
}

static const RuntimeObjectView *
FindTorchRuntimeForCurrentFeatureLocal(const ManualTestContext &context,
                                       bool &staleFromDifferentTask) {
  staleFromDifferentTask = false;
  const RuntimeObjectView *object = FindPrimaryTorchRuntimeObjectLocal(context);
  if (object == nullptr)
    return nullptr;

  const std::string feature = TorchTaskFeatureLocal(context);
  if (object->type == "FindSegmentation") {
    const bool compatible = feature == "semantic_segmentation" ||
                            feature == "instance_segmentation" ||
                            feature == "runtime_contract";
    staleFromDifferentTask = !compatible;
    return compatible ? object : nullptr;
  }

  const bool isTrainingResult =
      !object->torch_trainer_lifecycle_summary.empty();
  if (isTrainingResult) {
    const bool compatible =
        feature == "training_lifecycle" || feature == "semantic_segmentation";
    staleFromDifferentTask = !compatible;
    return compatible ? object : nullptr;
  }

  std::string resultText;
  ReadTorchTextFileLocal(object->torch_result_ref, resultText);
  const std::string resultKey =
      ToLowerAsciiLocal(object->torch_result_ref + " " +
                        object->torch_evidence_ref + " " + resultText);
  bool compatible = true;
  if (feature == "object_detection")
    compatible = resultKey.find("detect") != std::string::npos ||
                 object->torch_result_count > 0;
  else if (feature == "semantic_segmentation" ||
           feature == "instance_segmentation")
    compatible = resultKey.find("segment") != std::string::npos ||
                 object->torch_mask_available != 0;
  else if (feature == "training_lifecycle")
    compatible = false;

  staleFromDifferentTask = !compatible;
  return compatible ? object : nullptr;
}

static bool ExtractTorchJsonNumberLocal(const std::string &json,
                                        const std::string &key, double &value) {
  const std::string needle = "\"" + key + "\"";
  std::size_t pos = json.find(needle);
  if (pos == std::string::npos)
    return false;
  pos = json.find(':', pos + needle.size());
  if (pos == std::string::npos)
    return false;
  ++pos;
  while (pos < json.size() &&
         std::isspace(static_cast<unsigned char>(json[pos]))) {
    ++pos;
  }
  const char *begin = json.c_str() + pos;
  char *end = nullptr;
  const double parsed = std::strtod(begin, &end);
  if (begin == end || !std::isfinite(parsed))
    return false;
  value = parsed;
  return true;
}

static bool ExtractTorchJsonBoolLocal(const std::string &json,
                                      const std::string &key, bool &value) {
  const std::string needle = "\"" + key + "\"";
  std::size_t pos = json.find(needle);
  if (pos == std::string::npos)
    return false;
  pos = json.find(':', pos + needle.size());
  if (pos == std::string::npos)
    return false;
  ++pos;
  while (pos < json.size() &&
         std::isspace(static_cast<unsigned char>(json[pos]))) {
    ++pos;
  }
  if (json.compare(pos, 4, "true") == 0) {
    value = true;
    return true;
  }
  if (json.compare(pos, 5, "false") == 0) {
    value = false;
    return true;
  }
  return false;
}

static std::vector<TorchCurveSampleLocal>
BuildTorchCurveSamplesLocal(const RuntimeObjectView &object) {
  std::vector<TorchCurveSampleLocal> samples;
  std::string json;
  double number = 0.0;
  bool flag = false;

  if (ReadTorchTextFileLocal(object.torch_result_ref, json)) {
    if (ExtractTorchJsonNumberLocal(json, "smoke_loss", number))
      samples.push_back({"loss", number});
    if (ExtractTorchJsonNumberLocal(json, "eval_loss", number))
      samples.push_back({"eval_loss", number});
    if (ExtractTorchJsonNumberLocal(json, "grad_mean", number))
      samples.push_back({"grad", number});
    if (ExtractTorchJsonNumberLocal(json, "foreground_iou", number))
      samples.push_back({"IoU", number});
    if (ExtractTorchJsonNumberLocal(json, "avg_confidence", number))
      samples.push_back({"confidence", number});
    if (ExtractTorchJsonNumberLocal(json, "effective_batch_size", number))
      samples.push_back({"batch_size", number});
    if (ExtractTorchJsonNumberLocal(json, "input_size", number))
      samples.push_back({"input_size", number});
    if (ExtractTorchJsonNumberLocal(json, "foreground_ratio", number))
      samples.push_back({"fg", number});
    if (ExtractTorchJsonNumberLocal(json, "infer_runtime_ms", number))
      samples.push_back({"infer_ms", number});
    if (ExtractTorchJsonNumberLocal(json, "train_runtime_ms", number))
      samples.push_back({"train_ms", number});
  }

  if (ReadTorchTextFileLocal(object.torch_evidence_ref, json)) {
    if (ExtractTorchJsonNumberLocal(json, "epochs", number))
      samples.push_back({"epochs", number});
    if (ExtractTorchJsonBoolLocal(json, "finite_loss", flag))
      samples.push_back({"finite_loss", flag ? 1.0 : 0.0});
    if (ExtractTorchJsonNumberLocal(json, "grad_mean", number))
      samples.push_back({"grad_evidence", number});
  }

  return samples;
}

static std::vector<TorchCurveSampleLocal>
BuildFindSegmentationCurveSamplesLocal(const RuntimeObjectView &object) {
  std::vector<TorchCurveSampleLocal> samples;
  std::string json;
  double number = 0.0;

  if (ReadTorchTextFileLocal(object.segmentation_result_ref, json)) {
    if (ExtractTorchJsonNumberLocal(json, "foreground_ratio", number))
      samples.push_back({"fg", number});
    if (ExtractTorchJsonNumberLocal(json, "contour_count", number))
      samples.push_back({"contours", number});
    if (ExtractTorchJsonNumberLocal(json, "changed_pixels", number))
      samples.push_back({"changed", number});
  }

  if (object.segmentation_contour_count > 0)
    samples.push_back({"contours_live",
                       static_cast<double>(object.segmentation_contour_count)});
  if (object.segmentation_primary_area > 0.0)
    samples.push_back({"area", object.segmentation_primary_area});

  return samples;
}

static bool StageTorchUiRunLocal(ManualTestContext &context,
                                 const std::string &relativeScriptPath,
                                 const std::string &action,
                                 std::string &reason) {
  const std::filesystem::path scriptPath =
      ResolveWorkspaceFile(relativeScriptPath);
  std::string scriptText;
  if (scriptPath.empty() || !std::filesystem::exists(scriptPath) ||
      !ReadTextFile(scriptPath.string(), scriptText)) {
    reason = "Torch action script is unavailable: " + relativeScriptPath;
    return false;
  }
  if (scriptText.empty()) {
    reason = "Torch action script is empty: " + scriptPath.string();
    return false;
  }

  context.editor_text = scriptText;
  context.loaded_script_path = scriptPath.string();
  context.script_file_path = scriptPath.string();
  context.editor_source = "torch_runtime_action";
  context.editor_dirty = false;
  context.active_script_case_name = scriptPath.stem().string();
  context.active_script_case_path = scriptPath.string();
  context.active_script_case_purpose = action;
  context.current_gauge.tool = "TorchTask";
  context.current_gauge.has_line_gauge = false;
  context.current_gauge.has_circle_gauge = false;
  context.current_gauge.has_ellipse_gauge = false;
  context.current_gauge.dirty = true;
  context.current_gauge.review_status = "editing";
  if (action == "torch_train_tiny_smoke")
    ApplyTorchParameterDefaultsLocal(context, "training_lifecycle");
  else if (action == "torch_infer_detection")
    ApplyTorchParameterDefaultsLocal(context, "object_detection");
  else if (action == "torch_infer_segmentation")
    ApplyTorchParameterDefaultsLocal(context, "semantic_segmentation");
  context.pending_execution_gauge = context.current_gauge;
  context.pending_execution_globals = context.runtime_int_vars;
  context.has_pending_execution_snapshot = true;
  context.debug_action = "Torch Action Staged";
  context.debug_status = "TORCH_ACTION_STAGED";
  context.debug_reason =
      action + " staged; use Run Staged Action to execute serially";
  context.run_state = "ready";
  reason = context.debug_reason;
  CXLOG_INFO("TorchUI", "torch_ui_action_staged", "ui_event",
             "action=" + action + " script=" + scriptPath.string());
  return true;
}

void DrawTorchKeyStatusPanel(ManualTestContext &context) {
  bool staleFromDifferentTask = false;
  const RuntimeObjectView *object =
      FindTorchRuntimeForCurrentFeatureLocal(context, staleFromDifferentTask);
  const std::string selectedFeature = TorchTaskFeatureLocal(context);
  const std::string evidenceFeature =
      TorchSelectedEvidenceFeatureLocal(context);

  if (ImGui::CollapsingHeader("Torch Actions",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::TextWrapped(
        "All actions load a canonical cxscript into Script Editor and use the "
        "existing single Parser owner. No background worker or parallel Parser "
        "is used.");
    const bool runBusy =
        context.run_state == "running" || context.run_state == "runtime_step";
    if (runBusy)
      ImGui::BeginDisabled();

    std::string actionReason;
    const bool stagedTorchAction =
        context.editor_source == "torch_runtime_action" &&
        !context.active_script_case_purpose.empty();
    const std::string actionFeature =
        stagedTorchAction
            ? selectedFeature
            : (evidenceFeature == "runtime_contract" ? selectedFeature
                                                     : evidenceFeature);
    const bool torchActionInputAvailable =
        !context.image_file_path.empty() ||
        !context.current_evidence_selection.image_path.empty();
    const bool torchActionAvailable = torchActionInputAvailable;
    ImGui::TextDisabled("Torch action input: %s%s",
                        torchActionInputAvailable ? "image_ready"
                                                  : "image_missing",
                        IsTorchContext(context) ? " | torch_context"
                                                : " | stage_will_bind_torch");
    if (ImGui::Button("Train Segmentation Tiny Smoke")) {
      if (!StageTorchUiRunLocal(context,
                                "cxparser/cxscript/module/torch/"
                                "torch_train_lifecycle_direct_test.cxsc",
                                "torch_train_tiny_smoke", actionReason)) {
        context.run_state = "failed";
        context.debug_status = "TORCH_UI_ACTION_BLOCKED";
        context.debug_reason = actionReason;
      }
    }
    ImGui::SameLine();
    if (!torchActionAvailable)
      ImGui::BeginDisabled();
    if (ImGui::Button("Infer Segmentation")) {
      if (!StageTorchUiRunLocal(
              context,
              "cxparser/cxscript/module/torch/"
              "torch_segmentation_cpp_state_dict_cpu_direct.cxsc",
              "torch_infer_segmentation", actionReason)) {
        context.run_state = "failed";
        context.debug_status = "TORCH_UI_ACTION_BLOCKED";
        context.debug_reason = actionReason;
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Infer Detection")) {
      if (!StageTorchUiRunLocal(context,
                                "cxparser/cxscript/module/torch/"
                                "torch_detection_yolov8_cpu_smoke_direct.cxsc",
                                "torch_infer_detection", actionReason)) {
        context.run_state = "failed";
        context.debug_status = "TORCH_UI_ACTION_BLOCKED";
        context.debug_reason = actionReason;
      }
    }
    if (!torchActionAvailable)
      ImGui::EndDisabled();

    if (runBusy)
      ImGui::EndDisabled();
    DrawReadonlyFieldLocal("run_state", context.run_state);
    DrawReadonlyFieldLocal("debug_status", context.debug_status);
    DrawReadonlyFieldLocal("reason", context.debug_reason);
    if (!torchActionAvailable) {
      ImGui::TextColored(
          ImVec4(1.0f, 0.72f, 0.25f, 1.0f),
          "Load a Torch evidence image before staging Tiny Smoke. selected=%s",
          actionFeature.c_str());
    }
    DrawReadonlyFieldLocal("evidence_feature", evidenceFeature);
    DrawReadonlyFieldLocal("selected_feature", selectedFeature);
    DrawReadonlyFieldLocal("action_feature", actionFeature);
    if (stagedTorchAction && evidenceFeature != actionFeature) {
      ImGui::TextDisabled(
          "Evidence image family is %s; the staged action is %s.",
          evidenceFeature.c_str(), actionFeature.c_str());
    }
    if (context.debug_status == "TORCH_ACTION_STAGED") {
      ImGui::TextColored(
          ImVec4(0.52f, 0.82f, 1.0f, 1.0f),
          "Action staged: requested epochs=%d, batch=%d. It has not run yet.",
          context.runtime_int_vars["global_torch_epochs"],
          context.runtime_int_vars["global_torch_batch_size"]);
      if (ImGui::Button("Run Staged Action (serial)")) {
        context.debug_action = "Torch Action Run Requested";
        context.debug_status = "TORCH_ACTION_RUN_REQUESTED";
        context.debug_reason = "operator requested staged Torch action through "
                               "the single serial Parser owner";
        context.run_state = "queued";
        CXLOG_INFO("TorchUI", "torch_ui_action_run_requested", "ui_event",
                   "action=" + context.active_script_case_purpose +
                       " script=" + context.loaded_script_path);
      }
    }
    if (staleFromDifferentTask) {
      ImGui::TextColored(
          ImVec4(1.0f, 0.62f, 0.32f, 1.0f),
          "Previous Torch result belongs to a different task and is hidden.");
    }
    ImGui::TextDisabled(
        "Tiny smoke validates lifecycle and artifacts only. Detection and "
        "segmentation model semantics still require evidence review.");
  }

  if (!ImGui::CollapsingHeader("Torch Inference Status",
                               ImGuiTreeNodeFlags_DefaultOpen)) {
  } else {
    ImGui::TextWrapped(
        "Read-only bridge for TorchTask / FindSegmentation runtime fields. "
        "This panel displays existing headless/runtime refs; it does not claim "
        "detection acceptance.");

    if (object == nullptr) {
      ImGui::TextColored(
          ImVec4(1.0f, 0.68f, 0.25f, 1.0f),
          "No TorchTask or FindSegmentation runtime object is available.");
      ImGui::Text("script: %s", UiTextOrDash(context.loaded_script_path));
    } else {
      ImGui::Separator();
      ImGui::Text("Primary Object: %s %s", object->type.c_str(),
                  object->name.c_str());

      if (object->type == "TorchTask") {
        DrawReadonlyFieldLocal("status", object->torch_status);
        DrawReadonlyFieldLocal("reason", object->torch_reason);
        DrawReadonlyFieldLocal("failure_stage", object->torch_failure_stage);
        DrawReadonlyFieldLocal("ok", object->torch_ok);
        DrawReadonlyFieldLocal("error_code", object->torch_error_code);
        DrawReadonlyFieldLocal("actual_device", object->torch_actual_device);
        DrawReadonlyFieldLocal("runtime infer_ms", object->torch_infer_ms);
        DrawReadonlyFieldLocal("runtime total_ms", object->torch_total_ms);
        DrawReadonlyFieldLocal("result_count", object->torch_result_count);
        DrawReadonlyFieldLocal("mask_available", object->torch_mask_available);
        DrawReadonlyFieldLocal("result_ref", object->torch_result_ref);
        DrawReadonlyFieldLocal("evidence_ref", object->torch_evidence_ref);
        DrawReadonlyFieldLocal("primary_visual_ref",
                               object->torch_primary_visual_ref);
        DrawReadonlyFieldLocal("mask_ref", object->torch_mask_ref);
        DrawReadonlyFieldLocal("overlay_ref", object->torch_overlay_ref);
        if (object->torch_result_count <= 0) {
          ImGui::TextColored(ImVec4(1.0f, 0.68f, 0.25f, 1.0f),
                             "detection rectangle projection: PENDING_DATA");
        }
        ImGui::TextColored(
            ImVec4(0.60f, 0.82f, 1.0f, 1.0f),
            "semantic_quality: NOT_CLAIMED (runtime smoke / model unverified)");
      } else if (object->type == "FindSegmentation") {
        DrawReadonlyFieldLocal("backend", object->segmentation_backend);
        DrawReadonlyFieldLocal("backend_status",
                               object->segmentation_backend_status);
        DrawReadonlyFieldLocal("device", object->segmentation_device);
        DrawReadonlyFieldLocal("model_path", object->segmentation_model_path);
        DrawReadonlyFieldLocal("status_code", object->segmentation_status_code);
        DrawReadonlyFieldLocal("reason", object->segmentation_reason);
        DrawReadonlyFieldLocal("result_ref", object->segmentation_result_ref);
        DrawReadonlyFieldLocal("mask_ref", object->segmentation_mask_ref);
        DrawReadonlyFieldLocal("overlay_ref", object->segmentation_overlay_ref);
        DrawReadonlyFieldLocal("contour_ref", object->segmentation_contour_ref);
        DrawReadonlyFieldLocal("contour_count",
                               object->segmentation_contour_count);
        DrawReadonlyFieldLocal("primary_area",
                               object->segmentation_primary_area);
        ImGui::TextColored(
            ImVec4(0.60f, 0.82f, 1.0f, 1.0f),
            "semantic_quality: NOT_CLAIMED (prompt/backend smoke)");
      }
    }
  }

  if (ImGui::CollapsingHeader("Prompt / ROI", ImGuiTreeNodeFlags_DefaultOpen)) {
    if (object == nullptr) {
      const ManualGaugeState &gauge = context.current_gauge;
      if (gauge.tool == "FindSegmentation" ||
          NormalizeKeyParamToolTypeLocal(gauge.primary_object_type) ==
              "FindSegmentation" ||
          gauge.has_segmentation_prompt_rect) {
        ImGui::Text("prompt_rect: (%d,%d)-(%d,%d)",
                    gauge.segmentation_prompt_x0, gauge.segmentation_prompt_y0,
                    gauge.segmentation_prompt_x1, gauge.segmentation_prompt_y1);
        ImGui::Text("mode=%d threshold=%d%%", gauge.segmentation_mode,
                    gauge.segmentation_threshold_percent);
        ImGui::Text(
            "positive_point: %s (%d,%d)",
            gauge.has_segmentation_positive_point ? "enabled" : "disabled",
            gauge.segmentation_positive_x, gauge.segmentation_positive_y);
        ImGui::Text(
            "negative_point: %s (%d,%d)",
            gauge.has_segmentation_negative_point ? "enabled" : "disabled",
            gauge.segmentation_negative_x, gauge.segmentation_negative_y);
        ImGui::TextDisabled("No runtime object yet; these values are staged "
                            "for the next Run Script.");
      } else {
        DrawPendingBindingLineLocal(
            "prompt_rect",
            "run or select a TorchTask/FindSegmentation script first");
        DrawPendingBindingLineLocal("positive_points",
                                    "pending runtime object");
        DrawPendingBindingLineLocal("negative_points",
                                    "pending runtime object");
      }
    } else if (object->type == "FindSegmentation") {
      ImGui::Text("prompt_rect: %s", object->segmentation_has_prompt_rect
                                         ? "available"
                                         : "pending_binding");
      ImGui::Text("backend: %s", object->segmentation_backend.empty()
                                     ? "-"
                                     : object->segmentation_backend.c_str());
      const ManualGaugeState &gauge = context.current_gauge;
      ImGui::Text("staged ROI: (%d,%d)-(%d,%d) mode=%d threshold=%d%%",
                  gauge.segmentation_prompt_x0, gauge.segmentation_prompt_y0,
                  gauge.segmentation_prompt_x1, gauge.segmentation_prompt_y1,
                  gauge.segmentation_mode,
                  gauge.segmentation_threshold_percent);
      ImGui::Text("positive_point: %s (%d,%d)",
                  gauge.has_segmentation_positive_point ? "enabled"
                                                        : "disabled",
                  gauge.segmentation_positive_x, gauge.segmentation_positive_y);
      ImGui::Text("negative_point: %s (%d,%d)",
                  gauge.has_segmentation_negative_point ? "enabled"
                                                        : "disabled",
                  gauge.segmentation_negative_x, gauge.segmentation_negative_y);
    } else {
      DrawPendingBindingLineLocal(
          "prompt_rect", "TorchTask does not own manual prompt geometry");
      DrawPendingBindingLineLocal("positive_points",
                                  "use FindSegmentation prompt tool");
      DrawPendingBindingLineLocal("negative_points",
                                  "use FindSegmentation prompt tool");
    }
  }

  if (ImGui::CollapsingHeader("Torch Training Status",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    const std::string &evidenceParams =
        context.current_evidence_selection.parameter_summary;
    const std::string trainingStep = ReadEvidenceParameterTokenLocal(
        evidenceParams, "torch_training_step_executed");
    const std::string trainingSamples = ReadEvidenceParameterTokenLocal(
        evidenceParams, "torch_training_sample_count");
    const std::string trainingInstances = ReadEvidenceParameterTokenLocal(
        evidenceParams, "torch_training_instance_count");
    if (!trainingStep.empty() || !trainingSamples.empty() ||
        !trainingInstances.empty()) {
      DrawReadonlyFieldLocal("evidence_feature", selectedFeature);
      DrawReadonlyFieldLocal("training_step_executed", trainingStep);
      DrawReadonlyFieldLocal("training_sample_count", trainingSamples);
      DrawReadonlyFieldLocal("training_instance_count", trainingInstances);
      DrawReadonlyFieldLocal(
          "loss_phase",
          ReadEvidenceParameterTokenLocal(evidenceParams, "torch_loss_phase"));
      DrawReadonlyFieldLocal(
          "total_loss",
          ReadEvidenceParameterTokenLocal(evidenceParams, "torch_total_loss"));
      DrawReadonlyFieldLocal(
          "box/class/dfl/mask loss",
          ReadEvidenceParameterTokenLocal(evidenceParams, "torch_box_loss") +
              " / " +
              ReadEvidenceParameterTokenLocal(evidenceParams,
                                              "torch_class_loss") +
              " / " +
              ReadEvidenceParameterTokenLocal(evidenceParams,
                                              "torch_dfl_loss") +
              " / " +
              ReadEvidenceParameterTokenLocal(evidenceParams,
                                              "torch_mask_loss"));
      ImGui::TextDisabled(
          "Values above are restored from Evidence assets; they are not a new "
          "training execution.");
      ImGui::Separator();
    }
    const CxTorchTrainingRunBinding &trainingRun = context.torch_training_run;
    if (trainingRun.available) {
      DrawReadonlyFieldLocal("training_mode", "DATASET_MULTI_EPOCH");
      DrawReadonlyFieldLocal("dataset_consumed", "true");
      DrawReadonlyFieldLocal("dataset_source", trainingRun.dataset_source);
      DrawReadonlyFieldLocal("optimizer", trainingRun.optimizer);
      DrawReadonlyFieldLocal("lr_schedule", trainingRun.lr_schedule);
      DrawReadonlyFieldLocal("initial_learning_rate",
                             trainingRun.learning_rate);
      DrawReadonlyFieldLocal("min_learning_rate",
                             trainingRun.min_learning_rate);
      DrawReadonlyFieldLocal("weight_decay", trainingRun.weight_decay);
      ImGui::Text("loss weights box/class/DFL/mask: %.4g / %.4g / %.4g / %.4g",
                  trainingRun.box_loss_weight, trainingRun.class_loss_weight,
                  trainingRun.dfl_loss_weight, trainingRun.mask_loss_weight);
      ImGui::Text("epochs: %d / %d | optimizer steps: %d",
                  trainingRun.completed_epochs, trainingRun.configured_epochs,
                  trainingRun.completed_epochs);
      ImGui::Text("samples: %d | instances: %d", trainingRun.train_sample_count,
                  trainingRun.train_instance_count);
      if (!trainingRun.epochs.empty()) {
        const CxTorchTrainingEpochMetric &finalMetric =
            trainingRun.epochs.back();
        ImGui::Text(
            "final total/box/class/DFL/mask: %.6g / %.6g / %.6g / %.6g / %.6g",
            finalMetric.total_loss, finalMetric.box_loss,
            finalMetric.class_loss, finalMetric.dfl_loss,
            finalMetric.mask_loss);
      }
      DrawReadonlyFieldLocal("training_trace", trainingRun.training_trace_path);
    } else if (object == nullptr) {
      DrawReadonlyFieldLocal("requested_feature", selectedFeature);
      DrawReadonlyFieldLocal("requested_epochs",
                             context.runtime_int_vars["global_torch_epochs"]);
      DrawReadonlyFieldLocal(
          "requested_batch_size",
          context.runtime_int_vars["global_torch_batch_size"]);
      DrawReadonlyFieldLocal("execution_state", context.debug_status);
      if (context.debug_status == "TORCH_ACTION_STAGED") {
        ImGui::TextColored(ImVec4(0.52f, 0.82f, 1.0f, 1.0f),
                           "Training request is staged. Click Run Staged "
                           "Action; no result exists yet.");
      } else if (context.debug_status == "TORCH_ACTION_RUN_REQUESTED" ||
                 context.run_state == "queued") {
        ImGui::TextColored(
            ImVec4(1.0f, 0.82f, 0.25f, 1.0f),
            "Training request is queued for the serial Parser owner.");
      } else {
        DrawPendingBindingLineLocal(
            "training lifecycle",
            "stage and run torch_train_lifecycle_direct_test.cxsc");
      }
    } else if (object->type == "TorchTask") {
      if (selectedFeature == "training_lifecycle") {
        DrawReadonlyFieldLocal("training_mode", "SYNTHETIC_LIFECYCLE_SMOKE");
        DrawReadonlyFieldLocal("dataset_consumed", "false");
        DrawReadonlyFieldLocal("optimizer_steps", 0);
        DrawReadonlyFieldLocal(
            "epoch",
            std::string("one forward/backward lifecycle validation step"));
        DrawReadonlyFieldLocal("completed_epochs", 1);
        DrawReadonlyFieldLocal("curve_state",
                               "ONE_SMOKE_STEP_NO_MULTI_EPOCH_SERIES");
        DrawReadonlyFieldLocal("trainer_summary",
                               object->torch_trainer_lifecycle_summary);
        DrawReadonlyFieldLocal("mainline_summary",
                               object->torch_unified_mainline_summary);
        DrawReadonlyFieldLocal("train_ms", object->torch_train_ms);
        DrawReadonlyFieldLocal("total_ms", object->torch_total_ms);
        if (object->torch_trainer_lifecycle_summary.empty())
          DrawPendingBindingLineLocal("epoch-loss detail",
                                      "not produced by this runtime object");
        ImGui::TextColored(ImVec4(1.0f, 0.76f, 0.24f, 1.0f),
                           "This smoke does not train from the Evidence image "
                           "set or annotation labels.");
        ImGui::TextDisabled(
            "A real curve requires an image/mask label adapter, persistent "
            "model/optimizer, and runtime epoch samples.");
      } else {
        DrawReadonlyFieldLocal("training_state",
                               "NOT_REQUESTED_FOR_CURRENT_TORCH_CASE");
        DrawReadonlyFieldLocal("current_feature", selectedFeature);
        ImGui::TextDisabled("This runtime object belongs to "
                            "inference/evidence. It is not a training result.");
      }
    } else {
      DrawPendingBindingLineLocal(
          "training lifecycle", "FindSegmentation is inference/prompt object");
    }
  }
}

void DrawTorchAnnotationKeyParameterPanel(ManualTestContext &context) {
  const CxEvidenceSelectionSnapshot &evidence =
      context.current_evidence_selection;
  const TorchTrainingImageItem *image = CurrentTorchTrainingImageLocal(context);
  const RuntimeObjectView *runtime =
      FindPrimaryTorchRuntimeObjectLocal(context);
  const std::string feature = TorchTaskFeatureLocal(context);
  ApplyTorchParameterDefaultsLocal(context, feature);

  ImGui::TextColored(ImVec4(0.42f, 0.78f, 1.0f, 1.0f),
                     "Torch Annotation / Feature Parameters");
  ImGui::TextWrapped(
      "Single debug chain: Evidence case -> Training Image Set item -> "
      "ImageAnnotationLayer shapes -> staged global_* values -> TorchTask "
      "run.");
  ImGui::Separator();

  if (ImGui::CollapsingHeader("Current Evidence Binding",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    DrawReadonlyFieldLocal("case_id", evidence.case_id);
    DrawReadonlyFieldLocal("script", evidence.script_id.empty()
                                         ? evidence.script_path
                                         : evidence.script_id);
    DrawReadonlyFieldLocal("task_feature", feature);
    DrawReadonlyFieldLocal("image_id", image == nullptr ? evidence.image_id
                                                        : image->image_id);
    DrawReadonlyFieldLocal("image_path", image == nullptr
                                             ? context.image_file_path
                                             : image->image_path);
    DrawReadonlyFieldLocal("split", image == nullptr ? "-" : image->split);
    DrawReadonlyFieldLocal("label", image == nullptr ? "-" : image->label);
    DrawReadonlyFieldLocal("annotation_status",
                           image == nullptr ? "no_training_image_selected"
                                            : image->annotation_status);
    DrawReadonlyFieldLocal("evidence_dataset_images",
                           std::to_string(evidence.dataset_images.size()));
    DrawReadonlyFieldLocal("evidence_annotations",
                           std::to_string(evidence.annotations.size()));
  }

  if (ImGui::CollapsingHeader("Annotated Regions / Features",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    if (image == nullptr) {
      ImGui::TextColored(ImVec4(1.0f, 0.68f, 0.25f, 1.0f),
                         "No Torch Training Image Set item is selected.");
      ImGui::TextWrapped(
          "Select the Evidence case and click a training thumbnail. The "
          "thumbnail must be loaded into Image View before annotations can "
          "be reflected here.");
    } else {
      int editableCount = 0;
      int resultCount = 0;
      int pointCount = 0;
      int rectCount = 0;
      int circleCount = 0;
      int contourCount = 0;
      int positivePromptCount = 0;
      int negativePromptCount = 0;
      for (const TorchTrainingAnnotationShapeSnapshot &shape :
           image->annotation_shapes) {
        editableCount += shape.editable ? 1 : 0;
        resultCount += shape.result_element ? 1 : 0;
        const std::string kind = ToLowerAsciiLocal(shape.shape_kind);
        const std::string role =
            ToLowerAsciiLocal(shape.semantic_role + " " + shape.tool_id + " " +
                              shape.owner_binding);
        if (kind.find("point") != std::string::npos)
          ++pointCount;
        if (kind.find("rect") != std::string::npos)
          ++rectCount;
        if (kind.find("circle") != std::string::npos ||
            kind.find("ellipse") != std::string::npos)
          ++circleCount;
        if (kind.find("polyline") != std::string::npos ||
            role.find("boundary") != std::string::npos ||
            role.find("contour") != std::string::npos)
          ++contourCount;
        if (role.find("positive") != std::string::npos)
          ++positivePromptCount;
        if (role.find("negative") != std::string::npos)
          ++negativePromptCount;
      }

      ImGui::Text("shapes=%d editable=%d results=%d points=%d rects=%d "
                  "circles/ellipses=%d contours=%d",
                  static_cast<int>(image->annotation_shapes.size()),
                  editableCount, resultCount, pointCount, rectCount,
                  circleCount, contourCount);
      ImGui::Text("prompt labels: positive=%d negative=%d", positivePromptCount,
                  negativePromptCount);

      int roiX0 = 0;
      int roiY0 = 0;
      int roiX1 = 0;
      int roiY1 = 0;
      const bool hasBounds =
          TorchAnnotationBoundsLocal(*image, roiX0, roiY0, roiX1, roiY1);
      if (hasBounds) {
        ImGui::Text("annotation_bounds: (%d,%d)-(%d,%d) size=%dx%d", roiX0,
                    roiY0, roiX1, roiY1, roiX1 - roiX0, roiY1 - roiY0);
        if (ImGui::Button("Stage Annotation Bounds As Torch ROI")) {
          InjectManualGaugeInt(context, "global_roi_x0", roiX0);
          InjectManualGaugeInt(context, "global_roi_y0", roiY0);
          InjectManualGaugeInt(context, "global_roi_x1", roiX1);
          InjectManualGaugeInt(context, "global_roi_y1", roiY1);
          InjectManualGaugeInt(context, "global_roi_x", roiX0);
          InjectManualGaugeInt(context, "global_roi_y", roiY0);
          InjectManualGaugeInt(context, "global_roi_width", roiX1 - roiX0);
          InjectManualGaugeInt(context, "global_roi_height", roiY1 - roiY0);
          context.current_gauge.dirty = true;
          context.current_gauge.review_status = "editing";
          context.debug_action = "Stage Torch Annotation ROI";
          context.debug_status = "TORCH_ANNOTATION_ROI_STAGED";
          context.debug_reason =
              "annotation bounds staged to global_roi_* for case=" +
              evidence.case_id + " image=" + image->image_id;
          CXLOG_INFO("TorchKeyParameters", "annotation_roi_staged", "ui_event",
                     context.debug_reason + " roi=" + std::to_string(roiX0) +
                         "," + std::to_string(roiY0) + "," +
                         std::to_string(roiX1) + "," + std::to_string(roiY1));
        }
        ImGui::SameLine();
        ImGui::TextDisabled("stages values only; the selected cxscript must "
                            "declare the matching global_* inputs");
      } else {
        ImGui::TextDisabled("No editable annotation geometry is available for "
                            "an ROI snapshot.");
      }

      if (ImGui::BeginTable("torch_annotation_shapes", 5,
                            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                ImGuiTableFlags_Resizable |
                                ImGuiTableFlags_ScrollY,
                            ImVec2(0.0f, 155.0f))) {
        ImGui::TableSetupColumn("Ref");
        ImGui::TableSetupColumn("Role");
        ImGui::TableSetupColumn("Kind");
        ImGui::TableSetupColumn("Geometry");
        ImGui::TableSetupColumn("State");
        ImGui::TableHeadersRow();
        for (const TorchTrainingAnnotationShapeSnapshot &shape :
             image->annotation_shapes) {
          ImGui::TableNextRow();
          ImGui::TableSetColumnIndex(0);
          ImGui::TextUnformatted(
              shape.stable_ref.empty() ? "-" : shape.stable_ref.c_str());
          ImGui::TableSetColumnIndex(1);
          ImGui::TextUnformatted(shape.semantic_role.empty()
                                     ? "annotation"
                                     : shape.semantic_role.c_str());
          ImGui::TableSetColumnIndex(2);
          ImGui::TextUnformatted(
              shape.shape_kind.empty() ? "-" : shape.shape_kind.c_str());
          ImGui::TableSetColumnIndex(3);
          const std::string geometry = TorchShapeGeometryTextLocal(shape);
          ImGui::TextUnformatted(geometry.c_str());
          ImGui::TableSetColumnIndex(4);
          ImGui::Text("%s%s", shape.editable ? "editable" : "read-only",
                      shape.result_element ? "/result" : "");
        }
        ImGui::EndTable();
      }
    }
  }

  if (ImGui::CollapsingHeader("Torch Runtime Parameters",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::TextDisabled(
        "Values marked request-bound enter CxTorchTaskSpec.extra_json through "
        "the selected script's request context. Backend consumption remains "
        "task/manifest-specific and is not implied by request binding.");
    const std::string selectedTorchKey = ToLowerAsciiLocal(
        evidence.case_id + " " + evidence.script_id + " " +
        evidence.script_path + " " + evidence.parameter_summary);
    const bool isResNet18 =
        selectedTorchKey.find("resnet18") != std::string::npos;
    const bool isResNet50 =
        selectedTorchKey.find("resnet50") != std::string::npos;
    if (feature == "classification" && (isResNet18 || isResNet50)) {
      const bool featureMode =
          selectedTorchKey.find("feature") != std::string::npos;
      DrawReadonlyFieldLocal("model family", "classification");
      DrawReadonlyFieldLocal("backbone", isResNet18 ? "ResNet18" : "ResNet50");
      DrawReadonlyFieldLocal("execution mode",
                             featureMode ? "feature" : "infer");
      DrawReadonlyFieldLocal(
          "weight dependency",
          isResNet18
              ? "LIBTORCH_MODULE_RESNET18_WEIGHTS / resnet18_weights.pt"
              : "LIBTORCH_MODULE_RESNET50_WEIGHTS / resnet50_weights.pt");
      DrawRuntimeIntRow(context, "class count", "global_torch_num_classes", 7,
                        1, 100000, 175.0f);
      DrawRuntimeIntRow(context, "top-k", "global_torch_top_k", 5, 1, 1000,
                        175.0f);
      DrawRuntimeIntRow(context, "ImageNet normalization (0/1)",
                        "global_torch_normalize_input", 1, 0, 1, 175.0f);
      DrawRuntimeIntRow(context, "feature pyramid (0/1)",
                        "global_torch_feature_pyramid_enabled",
                        featureMode ? 1 : 0, 0, 1, 175.0f);
      ImGui::TextDisabled(
          "ResNet controls are staged with the selected Evidence case. "
          "They do not change the fixed backbone declared by that case.");
      ImGui::Separator();
    }
    ImGui::TextDisabled("ROI (image coordinates)");
    DrawRuntimeIntRow(context, "roi x0", "global_roi_x0", 0, 0, 100000, 175.0f);
    DrawRuntimeIntRow(context, "roi y0", "global_roi_y0", 0, 0, 100000, 175.0f);
    DrawRuntimeIntRow(context, "roi x1", "global_roi_x1", 639, 0, 100000,
                      175.0f);
    DrawRuntimeIntRow(context, "roi y1", "global_roi_y1", 639, 0, 100000,
                      175.0f);

    ImGui::Separator();
    ImGui::TextDisabled("Model input and thresholds");
    DrawRuntimeIntRow(context, "input width", "global_torch_input_width", 512,
                      16, 4096, 175.0f);
    DrawRuntimeIntRow(context, "input height", "global_torch_input_height", 512,
                      16, 4096, 175.0f);
    DrawRuntimeIntRow(context, "confidence threshold %",
                      "global_torch_confidence_percent", 50, 0, 100, 175.0f);
    DrawRuntimeIntRow(context, "IoU threshold %",
                      "global_torch_iou_threshold_percent", 45, 0, 100, 175.0f);
    DrawRuntimeIntRow(context, "mask threshold %",
                      "global_torch_mask_threshold_percent", 50, 0, 100,
                      175.0f);
    DrawRuntimeIntRow(context, "max detections", "global_torch_max_detections",
                      100, 1, 1000, 175.0f);

    ImGui::Separator();
    ImGui::TextDisabled("Training request");
    DrawRuntimeIntRow(context, "epochs", "global_torch_epochs", 1, 1, 10000,
                      175.0f);
    DrawRuntimeIntRow(context, "batch size", "global_torch_batch_size", 1, 1,
                      1024, 175.0f);
    const std::string &evidenceParams = evidence.parameter_summary;
    const std::string evidenceResultCount =
        ReadEvidenceParameterTokenLocal(evidenceParams, "torch_result_count");
    if (!evidenceResultCount.empty()) {
      ImGui::Separator();
      ImGui::TextDisabled("Evidence stability context (read-only)");
      DrawReadonlyFieldLocal("result count", evidenceResultCount);
      DrawReadonlyFieldLocal("delta from baseline",
                             ReadEvidenceParameterTokenLocal(
                                 evidenceParams, "torch_result_count_delta"));
      DrawReadonlyFieldLocal("ROI shift dx/dy",
                             ReadEvidenceParameterTokenLocal(
                                 evidenceParams, "torch_roi_shift_dx_px") +
                                 " / " +
                                 ReadEvidenceParameterTokenLocal(
                                     evidenceParams, "torch_roi_shift_dy_px"));
      DrawReadonlyFieldLocal("model manifest",
                             ReadEvidenceParameterTokenLocal(
                                 evidenceParams, "torch_model_manifest"));
    }
    if (feature == "training_lifecycle") {
      ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.25f, 1.0f),
                         "Tiny Smoke effective profile: epochs=1, "
                         "batch_size=2, input=128x128.");
      ImGui::TextDisabled(
          "This validates forward/backward and gradients only; optimizer-step "
          "multi-epoch training is not bound to the runtime task yet.");
    }
    ImGui::TextDisabled(
        "All values above are staged into the next Torch request. The active "
        "task/manifest decides which fields it consumes.");

    ImGui::Separator();
    if (runtime == nullptr) {
      ImGui::TextDisabled("Runtime result: not executed. Run the selected "
                          "cxscript to populate TorchTask status/artifacts.");
    } else {
      DrawReadonlyFieldLocal("runtime object",
                             runtime->type + " " + runtime->name);
      DrawReadonlyFieldLocal("status",
                             runtime->type == "TorchTask"
                                 ? runtime->torch_status
                                 : runtime->segmentation_backend_status);
      DrawReadonlyFieldLocal("result_ref",
                             runtime->type == "TorchTask"
                                 ? runtime->torch_result_ref
                                 : runtime->segmentation_result_ref);
      DrawReadonlyFieldLocal("overlay_ref",
                             runtime->type == "TorchTask"
                                 ? runtime->torch_overlay_ref
                                 : runtime->segmentation_overlay_ref);
    }
  }
}

void DrawTorchEvidenceAndReviewPanel(const ManualTestContext &context) {
  bool staleFromDifferentTask = false;
  const RuntimeObjectView *object =
      FindTorchRuntimeForCurrentFeatureLocal(context, staleFromDifferentTask);

  if (staleFromDifferentTask) {
    ImGui::TextColored(ImVec4(1.0f, 0.62f, 0.32f, 1.0f),
                       "Previous Torch runtime result is not part of the "
                       "selected Evidence task.");
    ImGui::TextDisabled("Artifacts, shape attach and metric snapshots remain "
                        "hidden until the selected cxscript is run.");
  }

  if (ImGui::CollapsingHeader("Training Image Set",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    int trainCount = 0;
    int valCount = 0;
    int testCount = 0;
    int goodCount = 0;
    int anomalyCount = 0;
    int unlabeledCount = 0;
    for (const TorchTrainingImageItem &item : context.torch_training_images) {
      if (item.split == "train")
        ++trainCount;
      else if (item.split == "val")
        ++valCount;
      else if (item.split == "test")
        ++testCount;

      if (item.label == "good")
        ++goodCount;
      else if (item.label == "anomaly")
        ++anomalyCount;
      else
        ++unlabeledCount;
    }

    DrawReadonlyFieldLocal("dataset_status",
                           context.torch_training_image_status);
    DrawReadonlyFieldLocal("dataset_reason",
                           context.torch_training_image_reason);
    ImGui::Text("split_count: train=%d val=%d test=%d", trainCount, valCount,
                testCount);
    ImGui::Text("label_count: good=%d anomaly=%d unlabeled_or_pending=%d",
                goodCount, anomalyCount, unlabeledCount);
    ImGui::TextColored(ImVec4(0.60f, 0.82f, 1.0f, 1.0f),
                       "Open the 'Torch Training Image Set' window for "
                       "thumbnail rails and label editing.");
  }

  if (ImGui::CollapsingHeader("Torch Artifact Evidence",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    if (object == nullptr) {
      ImGui::TextDisabled("No torch runtime object selected.");
    } else if (object->type == "TorchTask") {
      DrawReadonlyFieldLocal("mask_binary", object->torch_mask_ref);
      DrawReadonlyFieldLocal("mask_overlay", object->torch_overlay_ref);
      DrawReadonlyFieldLocal("contours", object->torch_primary_visual_ref);
      DrawReadonlyFieldLocal("result_json", object->torch_result_ref);
      DrawReadonlyFieldLocal("evidence_json", object->torch_evidence_ref);
    } else {
      DrawReadonlyFieldLocal("mask_binary", object->segmentation_mask_ref);
      DrawReadonlyFieldLocal("mask_overlay", object->segmentation_overlay_ref);
      DrawReadonlyFieldLocal("contours", object->segmentation_contour_ref);
      DrawReadonlyFieldLocal("result_json", object->segmentation_result_ref);
    }
  }

  if (ImGui::CollapsingHeader("Shape Attach", ImGuiTreeNodeFlags_DefaultOpen)) {
    if (object == nullptr) {
      ImGui::TextDisabled("No shape attach state.");
    } else if (object->type == "FindSegmentation") {
      ImGui::Text("boundary_polyline: %s",
                  object->segmentation_has_boundary ? "available" : "pending");
      ImGui::Text("boundary_bbox: %s", object->segmentation_has_boundary
                                           ? "available_from_contour"
                                           : "pending");
      ImGui::Text("mask shape state: %s",
                  object->segmentation_real_mask_attach_ready
                      ? "attach_ready"
                      : "pending_binding");
      DrawReadonlyFieldLocal("contour_ref", object->segmentation_contour_ref);
    } else {
      ImGui::Text("boundary_polyline: %s",
                  object->torch_primary_visual_ref.empty()
                      ? "pending"
                      : object->torch_primary_visual_ref.c_str());
      ImGui::Text("boundary_bbox: %s", object->torch_result_count > 0
                                           ? "available_if_projected"
                                           : "PENDING_DATA");
      ImGui::Text("mask shape state: %s", object->torch_mask_available != 0
                                              ? "mask_available"
                                              : "pending");
    }
  }

  if (ImGui::CollapsingHeader("Contract / Review",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    DrawReadonlyFieldLocal("contract_status",
                           context.current_evidence_selection.status);
    DrawReadonlyFieldLocal("contract_reason",
                           context.current_evidence_selection.reason);
    DrawReadonlyFieldLocal("human_review_status",
                           context.current_gauge.review_status);
    const bool promotionAllowed =
        context.current_gauge.review_status == "accepted" &&
        object != nullptr &&
        ((object->type == "TorchTask" && object->torch_ok != 0) ||
         (object->type == "FindSegmentation" &&
          object->segmentation_real_mask_attach_ready));
    ImGui::Text("promotion_allowed: %s",
                promotionAllowed ? "candidate_ready_for_gate" : "false");
  }

  if (ImGui::CollapsingHeader("Training Curve / Param Map",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    const CxTorchTrainingRunBinding &boundTraining = context.torch_training_run;
    if (boundTraining.available) {
      std::vector<float> totalLoss;
      std::vector<float> boxLoss;
      std::vector<float> classLoss;
      std::vector<float> dflLoss;
      std::vector<float> maskLoss;
      std::vector<float> learningRates;
      for (const CxTorchTrainingEpochMetric &metric : boundTraining.epochs) {
        totalLoss.push_back(static_cast<float>(metric.total_loss));
        boxLoss.push_back(static_cast<float>(metric.box_loss));
        classLoss.push_back(static_cast<float>(metric.class_loss));
        dflLoss.push_back(static_cast<float>(metric.dfl_loss));
        maskLoss.push_back(static_cast<float>(metric.mask_loss));
        learningRates.push_back(static_cast<float>(metric.learning_rate));
      }
      const bool hasRealTrainingCurve =
          boundTraining.HasRealMultiEpochSeries();
      if (hasRealTrainingCurve) {
        ImGui::Text(
            "REAL_MULTI_EPOCH_SERIES | epochs %d/%d | samples %d | instances %d",
            boundTraining.completed_epochs, boundTraining.configured_epochs,
            boundTraining.train_sample_count,
            boundTraining.train_instance_count);
      } else {
        const std::string curveStatus = boundTraining.TrainingCurveStatus();
        ImGui::TextColored(ImVec4(1.0f, 0.76f, 0.28f, 1.0f),
                           "learning_curve_status: %s",
                           curveStatus.c_str());
        ImGui::TextDisabled(
            "Bound training data is not an effective incremental training "
            "curve until it contains a runtime-produced multi-epoch series.");
      }
      ImGui::Text("optimizer %s | schedule %s | LR %.8g -> %.8g | decay %.8g",
                  boundTraining.optimizer.c_str(),
                  boundTraining.lr_schedule.c_str(),
                  boundTraining.learning_rate, boundTraining.min_learning_rate,
                  boundTraining.weight_decay);
      if (hasRealTrainingCurve && !totalLoss.empty()) {
        ImGui::PlotLines("Total loss", totalLoss.data(),
                         static_cast<int>(totalLoss.size()), 0, nullptr,
                         FLT_MAX, FLT_MAX, ImVec2(-1.0f, 90.0f));
        ImGui::PlotLines("Box loss", boxLoss.data(),
                         static_cast<int>(boxLoss.size()), 0, nullptr, FLT_MAX,
                         FLT_MAX, ImVec2(-1.0f, 48.0f));
        ImGui::PlotLines("Class loss", classLoss.data(),
                         static_cast<int>(classLoss.size()), 0, nullptr,
                         FLT_MAX, FLT_MAX, ImVec2(-1.0f, 48.0f));
        ImGui::PlotLines("DFL loss", dflLoss.data(),
                         static_cast<int>(dflLoss.size()), 0, nullptr, FLT_MAX,
                         FLT_MAX, ImVec2(-1.0f, 48.0f));
        ImGui::PlotLines("Mask loss", maskLoss.data(),
                         static_cast<int>(maskLoss.size()), 0, nullptr, FLT_MAX,
                         FLT_MAX, ImVec2(-1.0f, 48.0f));
        ImGui::PlotLines("Learning rate", learningRates.data(),
                         static_cast<int>(learningRates.size()), 0, nullptr,
                         0.0f, FLT_MAX, ImVec2(-1.0f, 48.0f));
        int decreasingSteps = 0;
        for (std::size_t i = 1; i < boundTraining.epochs.size(); ++i) {
          if (boundTraining.epochs[i].total_loss <
              boundTraining.epochs[i - 1].total_loss)
            ++decreasingSteps;
        }
        const double firstLoss = boundTraining.epochs.front().total_loss;
        const double finalLoss = boundTraining.epochs.back().total_loss;
        ImGui::Text(
            "loss first/final: %.6g / %.6g | change %.2f%% | decreasing %d/%d",
            firstLoss, finalLoss,
            firstLoss != 0.0 ? (finalLoss - firstLoss) / firstLoss * 100.0
                             : 0.0,
            decreasingSteps, static_cast<int>(boundTraining.epochs.size() - 1));
      }
      if (hasRealTrainingCurve &&
          ImGui::BeginTable("bound_training_param_map", 6,
                            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                ImGuiTableFlags_ScrollY,
                            ImVec2(-1.0f, 220.0f))) {
        ImGui::TableSetupColumn("Epoch");
        ImGui::TableSetupColumn("Group");
        ImGui::TableSetupColumn("Grad mean");
        ImGui::TableSetupColumn("Grad norm");
        ImGui::TableSetupColumn("Update norm");
        ImGui::TableSetupColumn("Update/Param");
        ImGui::TableHeadersRow();
        for (const CxTorchTrainingEpochMetric &metric : boundTraining.epochs) {
          for (const auto &group : metric.parameter_groups) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%d", metric.epoch);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(group.name.c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.4g", group.grad_mean);
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.4g", group.grad_norm);
            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%.4g", group.update_norm);
            ImGui::TableSetColumnIndex(5);
            ImGui::Text("%.4g", group.param_norm > 0.0
                                    ? group.update_norm / group.param_norm
                                    : 0.0);
          }
        }
        ImGui::EndTable();
      }
      DrawReadonlyFieldLocal("training_trace",
                             boundTraining.training_trace_path);
    } else {
      std::vector<TorchCurveSampleLocal> samples;
      if (object != nullptr && object->type == "TorchTask")
        samples = BuildTorchCurveSamplesLocal(*object);
      else if (object != nullptr && object->type == "FindSegmentation")
        samples = BuildFindSegmentationCurveSamplesLocal(*object);

      double epochs = 0.0;
      for (const TorchCurveSampleLocal &sample : samples) {
        if (sample.label == "epochs")
          epochs = sample.value;
      }
      const bool isTrainingSnapshot =
          object != nullptr && object->type == "TorchTask" && epochs <= 1.0 &&
          std::any_of(samples.begin(), samples.end(),
                      [](const TorchCurveSampleLocal &sample) {
                        return sample.label == "loss";
                      });

      if (isTrainingSnapshot) {
        ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.25f, 1.0f),
                           "PENDING_REAL_MULTI_EPOCH_TRAINING");
        ImGui::TextDisabled("Tiny Smoke is one synthetic forward/backward "
                            "validation step. No learning curve is plotted.");
        ImGui::Separator();
      } else {
        const ImVec2 plotSize(520.0f, 180.0f);
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImVec2 p1(p0.x + plotSize.x, p0.y + plotSize.y);
        ImDrawList *draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(p0, p1, IM_COL32(18, 18, 18, 255));
        draw->AddRect(p0, p1, IM_COL32(230, 230, 230, 255));
        for (int gx = 0; gx <= 8; ++gx) {
          const float x = p0.x + plotSize.x * gx / 8.0f;
          draw->AddLine(ImVec2(x, p0.y), ImVec2(x, p1.y),
                        IM_COL32(75, 75, 75, 180));
        }
        for (int gy = 0; gy <= 4; ++gy) {
          const float y = p0.y + plotSize.y * gy / 4.0f;
          draw->AddLine(ImVec2(p0.x, y), ImVec2(p1.x, y),
                        IM_COL32(75, 75, 75, 180));
        }
        draw->AddText(ImVec2(p0.x + 8.0f, p0.y + 6.0f),
                      IM_COL32(240, 240, 240, 255),
                      isTrainingSnapshot
                          ? "tiny-smoke metric snapshot (not a learning curve)"
                          : "torch runtime metrics");
        if (isTrainingSnapshot) {
          draw->AddText(ImVec2(p0.x + 18.0f, p0.y + 58.0f),
                        IM_COL32(255, 210, 80, 255),
                        "PENDING_MULTI_EPOCH_SERIES");
          draw->AddText(ImVec2(p0.x + 18.0f, p0.y + 88.0f),
                        IM_COL32(205, 205, 205, 255),
                        "Runtime produced one smoke step only.");
          draw->AddText(
              ImVec2(p0.x + 18.0f, p0.y + 112.0f), IM_COL32(205, 205, 205, 255),
              "Loss/grad/runtime are listed below as separate metrics.");
        } else if (samples.empty()) {
          draw->AddText(ImVec2(p0.x + 8.0f, p1.y - 22.0f),
                        IM_COL32(255, 210, 80, 255),
                        "pending real curve samples");
        } else {
          double maxAbs = 0.0;
          for (const TorchCurveSampleLocal &sample : samples)
            maxAbs = std::max(maxAbs, std::abs(sample.value));
          if (maxAbs <= 0.0)
            maxAbs = 1.0;

          for (std::size_t i = 0; i < samples.size(); ++i) {
            const float t = samples.size() <= 1
                                ? 0.5f
                                : static_cast<float>(i) /
                                      static_cast<float>(samples.size() - 1);
            const double normalized =
                std::max(-1.0, std::min(1.0, samples[i].value / maxAbs));
            const float x = p0.x + 36.0f + (plotSize.x - 72.0f) * t;
            const float y = p1.y - 30.0f -
                            static_cast<float>((normalized + 1.0) * 0.5) *
                                (plotSize.y - 60.0f);
            const ImVec2 pt(x, y);
            draw->AddLine(ImVec2(x, p1.y - 30.0f), pt,
                          IM_COL32(90, 190, 255, 220), 3.0f);
            draw->AddCircleFilled(pt, 4.5f, IM_COL32(120, 255, 160, 255));
            draw->AddText(ImVec2(pt.x + 5.0f, pt.y - 12.0f),
                          IM_COL32(220, 255, 220, 255),
                          samples[i].label.c_str());
          }
          draw->AddText(ImVec2(p0.x + 8.0f, p1.y - 22.0f),
                        IM_COL32(120, 255, 160, 255),
                        "real values from torch result/evidence json");
        }
        ImGui::Dummy(plotSize);
      }
      if (!samples.empty()) {
        for (const TorchCurveSampleLocal &sample : samples) {
          ImGui::Text("%s: %.6g", sample.label.c_str(), sample.value);
        }
        if (epochs <= 1.0) {
          ImGui::TextDisabled(
              "Tiny-smoke metric snapshot only. Real training is pending "
              "dataset/label "
              "binding, optimizer steps, and a runtime-produced epoch series.");
        }
      }
      if (object != nullptr && object->type == "TorchTask") {
        DrawReadonlyFieldLocal("curve_samples_count",
                               static_cast<int>(samples.size()));
        DrawReadonlyFieldLocal("trainer_summary",
                               object->torch_trainer_lifecycle_summary);
        DrawReadonlyFieldLocal("mainline_summary",
                               object->torch_unified_mainline_summary);
      } else if (object != nullptr && object->type == "FindSegmentation") {
        DrawReadonlyFieldLocal("curve_samples_count",
                               static_cast<int>(samples.size()));
        DrawReadonlyFieldLocal("segmentation_result_ref",
                               object->segmentation_result_ref);
        DrawReadonlyFieldLocal("segmentation_overlay_ref",
                               object->segmentation_overlay_ref);
      }
    }
  }
}

void DrawKeyParameterUnavailableNotice(const ManualTestContext &context) {
  if (!ImGui::CollapsingHeader("关键参数 UI / 参数整定图",
                               ImGuiTreeNodeFlags_DefaultOpen))
    return;
  ImGui::TextColored(
      ImVec4(1.0f, 0.68f, 0.25f, 1.0f),
      "Current image/tool is not suitable for key-parameter tuning.");
  ImGui::TextWrapped("Select or create a Line/Circle annotation tool, or load "
                     "a script containing Findline/FindCircle/FastMatch. "
                     "FastMatch uses Learn ROI + Match ROI plus staged "
                     "Learn/Match parameters.");
  ImGui::Text(
      "current gauge tool=%s line_gauge=%s circle_gauge=%s fastmatch=%s",
      context.current_gauge.tool.c_str(),
      context.current_gauge.has_line_gauge ? "yes" : "no",
      context.current_gauge.has_circle_gauge ? "yes" : "no",
      (context.current_gauge.tool == "FastMatch" ||
       context.current_gauge.tool == "fastmatch" ||
       context.current_gauge.tool == "CFastMatch")
          ? "yes"
          : "no");
}

void DrawCxScriptWorkbenchOverview(ManualTestContext &context) {
  if (!ImGui::CollapsingHeader("CxScript Workbench / 人工验收总览",
                               ImGuiTreeNodeFlags_DefaultOpen))
    return;

  const ManualGaugeState &gauge = context.current_gauge;
  const ManualParamRegressionState &reg = context.param_regression;
  const bool gaugeAccepted = ManualGaugeAcceptedForParamRegression(gauge);
  const bool keyParamSuitable = IsFindLineFindCircleContext(context);

  ImGui::TextWrapped("This overview mirrors the design map: "
                     "Evidence/Annotation -> cxparser script template -> Key "
                     "Parameters -> Param Regression -> Conclusion/Evidence.");
  ImGui::Separator();

  if (ImGui::BeginTable("cxscript_workbench_map", 3,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable)) {
    ImGui::TableSetupColumn("证据链 / 标注工具集");
    ImGui::TableSetupColumn("cxparser script 基础模板");
    ImGui::TableSetupColumn("关键参数 / 结论");
    ImGui::TableHeadersRow();

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text("Image");
    ImGui::BulletText("image_file: %s", UiTextOrDash(context.image_file_path));
    ImGui::BulletText(
        "global_matInput: %s",
        context.global_variable_views.empty()
            ? "pending"
            : context.global_variable_views.front().status.c_str());
    ImGui::BulletText("annotation elements: %d", context.manual_elements_count);
    ImGui::BulletText("source preview: %s",
                      context.source_preview_enabled ? "on" : "off");
    ImGui::BulletText("gauge annotation: %s",
                      (ManualGaugeCaseDir(context) / "gauge_annotation.json")
                          .string()
                          .c_str());

    ImGui::TableSetColumnIndex(1);
    ImGui::Text("Template");
    ImGui::BulletText("script: %s", InferCurrentTemplatePath(context).c_str());
    ImGui::BulletText("tool: %s", InferCurrentTemplateTool(context).c_str());
    ImGui::BulletText("editor source: %s", context.editor_source.c_str());
    ImGui::BulletText("editor dirty: %s", context.editor_dirty ? "yes" : "no");
    ImGui::BulletText("catalog loaded: %s (%d entries)",
                      context.catalog_loaded ? "yes" : "no",
                      static_cast<int>(context.catalog_entries.size()));
    ImGui::BulletText("semantic lines: %d",
                      static_cast<int>(context.line_views.size()));

    ImGui::TableSetColumnIndex(2);
    ImGui::Text("Status");
    ImGui::BulletText("gauge: %s", gaugeAccepted ? "manual_accepted"
                                                 : gauge.review_status.c_str());
    ImGui::BulletText("param regression: %s", reg.status.c_str());
    ImGui::BulletText("key parameter UI: %s",
                      keyParamSuitable ? "visible"
                                       : "waiting for Findline/FindCircle");
    ImGui::BulletText("candidates: %d selected: %d",
                      static_cast<int>(reg.candidates.size()),
                      CountSelectedParamCandidates(context));
    ImGui::BulletText("result: %s", context.current_result_ref.status.c_str());
    ImGui::BulletText("debug: %s | %s", context.debug_status.c_str(),
                      context.debug_reason.c_str());

    ImGui::EndTable();
  }

  ImGui::Separator();
  if (ImGui::BeginTable("cxscript_design_flow", 6,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable)) {
    ImGui::TableSetupColumn("1 Evidence");
    ImGui::TableSetupColumn("2 Template");
    ImGui::TableSetupColumn("3 Gauge");
    ImGui::TableSetupColumn("4 Params");
    ImGui::TableSetupColumn("5 Conclusion");
    ImGui::TableSetupColumn("6 Export");
    ImGui::TableHeadersRow();
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextWrapped("%s", context.image_file_path.empty()
                                 ? "image pending"
                                 : "image selected");
    ImGui::TableSetColumnIndex(1);
    ImGui::TextWrapped("%s", context.editor_text.empty() ? "script pending"
                                                         : "script loaded");
    ImGui::TableSetColumnIndex(2);
    ImGui::TextWrapped("%s", gaugeAccepted ? "accepted" : "needs review");
    ImGui::TableSetColumnIndex(3);
    ImGui::TextWrapped("%s", reg.initialized ? "candidate table ready"
                                             : "initialize after gauge");
    ImGui::TableSetColumnIndex(4);
    ImGui::TextWrapped("%s", context.current_result_ref.status.empty()
                                 ? "no result"
                                 : context.current_result_ref.status.c_str());
    ImGui::TableSetColumnIndex(5);
    ImGui::TextWrapped("%d files indexed",
                       static_cast<int>(reg.exported_files.size()));
    ImGui::EndTable();
  }
}

void DrawEvidenceCaseListPanel(ManualTestContext &context) {
  if (!ImGui::CollapsingHeader("Evidence Case List"))
    return;

  ImGui::TextWrapped("Evidence cases organized by "
                     "case/image/target/tool/gauge_status/probe_status/"
                     "contract_status/review_status");

  const std::size_t catalogSeedCount = context.catalog_entries.size();
  const bool shouldSeedEvidenceItems =
      context.evidence_items.empty() &&
      (!context.evidence_items_seed_attempted ||
       context.evidence_items_seed_catalog_count != catalogSeedCount);

  if (shouldSeedEvidenceItems) {
    ImGui::TextDisabled("No evidence cases loaded.");
    ImGui::Text("Loading from catalog entries...");

    const std::size_t beforeCount = context.evidence_items.size();
    for (const auto &entry : context.catalog_entries) {
      bool isVisible = entry.manual_visible && entry.frozen &&
                       (entry.expected_result == "ok" ||
                        entry.expected_result == "ng_expected");
      if (!isVisible)
        continue;

      ManualEvidenceItem item;
      item.case_id = entry.script_id;
      item.tool = entry.tool;
      item.script_id = entry.script_id;
      item.gauge_status = "unannotated";
      item.probe_status = "pending";
      item.contract_status = "pending";
      item.review_status = "unreviewed";
      context.evidence_items.push_back(item);
    }
    context.evidence_items_seed_attempted = true;
    context.evidence_items_seed_catalog_count = catalogSeedCount;
    if (context.evidence_items.size() != beforeCount) {
      context.script_evidence_groups_dirty = true;
      context.workbench_assets_loaded = false;
    }
  } else if (context.evidence_items.empty()) {
    ImGui::TextDisabled("No evidence cases loaded.");
  }

  if (!context.evidence_items.empty()) {
    ImGui::BeginChild("evidence_case_list", ImVec2(-1, 200), true);

    if (ImGui::BeginTable("evidence_case_table", 10,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Resizable)) {
      ImGui::TableSetupColumn("Case", ImGuiTableColumnFlags_WidthFixed, 120.0f);
      ImGui::TableSetupColumn("Tool", ImGuiTableColumnFlags_WidthFixed, 80.0f);
      ImGui::TableSetupColumn("Image", ImGuiTableColumnFlags_WidthFixed,
                              100.0f);
      ImGui::TableSetupColumn("Target", ImGuiTableColumnFlags_WidthFixed,
                              100.0f);
      ImGui::TableSetupColumn("Gauge", ImGuiTableColumnFlags_WidthFixed, 90.0f);
      ImGui::TableSetupColumn("Probe", ImGuiTableColumnFlags_WidthFixed, 80.0f);
      ImGui::TableSetupColumn("Contract", ImGuiTableColumnFlags_WidthFixed,
                              80.0f);
      ImGui::TableSetupColumn("Review", ImGuiTableColumnFlags_WidthFixed,
                              90.0f);
      ImGui::TableSetupColumn("Script", ImGuiTableColumnFlags_WidthFixed,
                              100.0f);
      ImGui::TableSetupColumn("Param", ImGuiTableColumnFlags_WidthFixed,
                              100.0f);
      ImGui::TableHeadersRow();

      for (const auto &item : context.evidence_items) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(item.review_item.empty() ? item.case_id.c_str()
                                                       : item.review_item.c_str());
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(item.tool.c_str());
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted(item.image_id.empty() ? "-"
                                                     : item.image_id.c_str());
        ImGui::TableSetColumnIndex(3);
        ImGui::TextUnformatted(item.target_id.empty() ? "-"
                                                      : item.target_id.c_str());
        ImGui::TableSetColumnIndex(4);
        ImGui::TextUnformatted(item.gauge_status.c_str());
        ImGui::TableSetColumnIndex(5);
        ImGui::TextUnformatted(item.probe_status.c_str());
        ImGui::TableSetColumnIndex(6);
        ImGui::TextUnformatted(item.contract_status.c_str());
        ImGui::TableSetColumnIndex(7);
        ImGui::TextUnformatted(item.review_status.c_str());
        ImGui::TableSetColumnIndex(8);
        ImGui::TextUnformatted(item.script_id.empty() ? "-"
                                                      : item.script_id.c_str());
        ImGui::TableSetColumnIndex(9);
        ImGui::TextUnformatted(item.parameter_profile_id.empty()
                                   ? "-"
                                   : item.parameter_profile_id.c_str());
      }

      ImGui::EndTable();
    }

    ImGui::EndChild();
  }
}

void DrawCxScriptTemplateSummaryPanel(const ManualTestContext &context) {
  if (!ImGui::CollapsingHeader("cxparser script 基础模板 / 当前模板摘要",
                               ImGuiTreeNodeFlags_DefaultOpen))
    return;

  ImGui::Text("Current script: %s", InferCurrentTemplatePath(context).c_str());
  ImGui::Text("Tool: %s | Source: %s | Dirty: %s",
              InferCurrentTemplateTool(context).c_str(),
              context.editor_source.c_str(),
              context.editor_dirty ? "yes" : "no");
  ImGui::Text("Active case: %s | purpose: %s",
              UiTextOrDash(context.active_script_case_name),
              UiTextOrDash(context.active_script_case_purpose));
  ImGui::Separator();

  if (ImGui::BeginTable("template_object_method_summary", 4,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable,
                        ImVec2(-1, 120))) {
    ImGui::TableSetupColumn("Object");
    ImGui::TableSetupColumn("Type");
    ImGui::TableSetupColumn("Declared Line");
    ImGui::TableSetupColumn("Runtime State");
    ImGui::TableHeadersRow();
    for (const auto &object : context.object_views) {
      std::string runtime_state = "not_created";
      for (const auto &runtime : context.runtime_objects) {
        if (runtime.name == object.name) {
          runtime_state =
              runtime.exists_in_parser ? runtime.runtime_state : "stale";
          break;
        }
      }
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(object.name.c_str());
      ImGui::TableSetColumnIndex(1);
      ImGui::TextUnformatted(object.type.c_str());
      ImGui::TableSetColumnIndex(2);
      ImGui::Text("%d", object.declared_line);
      ImGui::TableSetColumnIndex(3);
      ImGui::TextUnformatted(runtime_state.c_str());
    }
    ImGui::EndTable();
  }
}

void DrawKeyParameterSummaryPanel(const ManualTestContext &context) {
  if (!ImGui::CollapsingHeader("关键参数表 / Key Parameter Summary",
                               ImGuiTreeNodeFlags_DefaultOpen))
    return;

  const ManualGaugeState &g = context.current_gauge;
  if (ImGui::BeginTable("key_parameter_summary", 4,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable)) {
    ImGui::TableSetupColumn("Group");
    ImGui::TableSetupColumn("Parameter");
    ImGui::TableSetupColumn("Value");
    ImGui::TableSetupColumn("Source");
    ImGui::TableHeadersRow();
    auto row = [&](const char *group, const char *name, int value,
                   const char *source) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(group);
      ImGui::TableSetColumnIndex(1);
      ImGui::TextUnformatted(name);
      ImGui::TableSetColumnIndex(2);
      ImGui::Text("%d", value);
      ImGui::TableSetColumnIndex(3);
      ImGui::TextUnformatted(source);
    };

    row("line", "x0", g.line_x0, g.source.c_str());
    row("line", "y0", g.line_y0, g.source.c_str());
    row("line", "x1", g.line_x1, g.source.c_str());
    row("line", "y1", g.line_y1, g.source.c_str());
    row("line", "tool_half_width", g.tool_half_width, g.source.c_str());
    row("line", "wgap", g.wgap, g.source.c_str());
    row("line", "hgap", g.hgap, g.source.c_str());
    row("common", "linegap", g.linegap, g.source.c_str());
    row("common", "threshold", g.threshold, g.source.c_str());
    row("common", "method", g.method, g.source.c_str());
    row("common", "filterprofile", g.filterprofile, g.source.c_str());
    if (NormalizeKeyParamToolTypeLocal(g.tool) == "FastMatch" ||
        NormalizeKeyParamToolTypeLocal(g.primary_object_type) == "FastMatch") {
      int scanRotationDeg = 0;
      const auto scanRotationIt =
          context.runtime_int_vars.find("global_fastmatch_scan_rotation_deg");
      if (scanRotationIt != context.runtime_int_vars.end())
        scanRotationDeg = scanRotationIt->second;
      row("fastmatch", "scan_rotation_deg", scanRotationDeg,
          g.source.c_str());
    }
    row("circle", "cx", g.circle_cx, g.source.c_str());
    row("circle", "cy", g.circle_cy, g.source.c_str());
    row("circle", "px", g.circle_px, g.source.c_str());


    ImGui::EndTable();
  }
}

void DrawConclusionSummaryPanel(const ManualTestContext &context) {
  if (!ImGui::CollapsingHeader("结论 UI / Result Conclusion",
                               ImGuiTreeNodeFlags_DefaultOpen))
    return;

  const ResultRefView &r = context.current_result_ref;
  ImGui::Text("result_ref: %s = %s", UiTextOrDash(r.name),
              UiTextOrDash(r.value));
  ImGui::Text("status: %s | reason: %s", UiTextOrDash(r.status),
              UiTextOrDash(r.reason));

  if (r.result_type == "FindSegmentationResult") {
    const bool boundaryAvailable = r.valid_points_count > 0;
    ImGui::Separator();
    ImGui::TextUnformatted("Tool: FindSegmentation");
    ImGui::Text("contours: %d", r.valid_points_count);
    ImGui::TextColored(boundaryAvailable ? ImVec4(0.35f, 0.85f, 0.45f, 1.0f)
                                         : ImVec4(1.0f, 0.45f, 0.30f, 1.0f),
                       "conclusion: %s",
                       boundaryAvailable
                           ? "boundary_available_pending_human_review"
                           : "boundary_unavailable");
    ImGui::TextWrapped("runtime reason: %s", UiTextOrDash(r.reason));
    ImGui::TextWrapped(
        "evidence: %s",
        context.image_overlay_summary.empty()
            ? "ASSET_MISSING: segmentation overlay/evidence unavailable"
            : context.image_overlay_summary.c_str());
    return;
  }

  if (ImGui::BeginTable("conclusion_summary_table", 4,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable)) {
    ImGui::TableSetupColumn("Metric");
    ImGui::TableSetupColumn("FindLine");
    ImGui::TableSetupColumn("FindCircle");
    ImGui::TableSetupColumn("Evidence");
    ImGui::TableHeadersRow();
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted("points");
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("%d", r.valid_line_points_count > 0 ? r.valid_line_points_count
                                                    : r.line_points_count);
    ImGui::TableSetColumnIndex(2);
    ImGui::Text("%d", r.valid_points_count > 0 ? r.valid_points_count
                                               : r.points_count);
    ImGui::TableSetColumnIndex(3);
    ImGui::TextUnformatted(context.image_overlay_summary.empty()
                               ? "pending"
                               : context.image_overlay_summary.c_str());
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted("fit");
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("(%.1f,%.1f)-(%.1f,%.1f)", r.line_x0, r.line_y0, r.line_x1,
                r.line_y1);
    ImGui::TableSetColumnIndex(2);
    ImGui::Text("(%.1f,%.1f) r=%.1f", r.fit_cx, r.fit_cy, r.fit_radius);
    ImGui::TableSetColumnIndex(3);
    ImGui::Text("avgdist line=%.2f circle=%.2f", r.line_avgdist, r.avgdist);
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted("failure hint");
    ImGui::TableSetColumnIndex(1);
    ImGui::TextUnformatted(r.line_measure_failure_hint.empty()
                               ? r.line_result_reason.c_str()
                               : r.line_measure_failure_hint.c_str());
    ImGui::TableSetColumnIndex(2);
    ImGui::TextUnformatted(r.reason.c_str());
    ImGui::TableSetColumnIndex(3);
    ImGui::TextUnformatted(context.debug_reason.c_str());
    ImGui::EndTable();
  }
}

void SyncKeyParameterUiToGauge(ManualTestContext &context) {
  ManualGaugeState &g = context.current_gauge;
  ManualParamRegressionState &ui = context.param_regression;
  if (g.tool == "FindObject") {
    // The generic tuning controls do not own FindObject's ROI.  In
    // particular, changing foreground polarity must not rehydrate or write
    // global_roi_* through this shared line/circle synchronization path.
    g.findobject_foreground_mode = std::max(1, std::min(3, ui.edge_mode));
    g.findobject_threshold = std::max(0, std::min(255, ui.contrast_percent));
    g.threshold = g.findobject_threshold;
    g.method = g.findobject_foreground_mode;
    g.dirty = true;
    return;
  }
  g.threshold = ui.contrast_percent;
  g.linegap = ui.measure_order;
  g.filterprofile = ui.enable_filter ? 1 : 0;
  if (g.has_line_gauge || g.tool != "FindCircle") {
    g.wgap = std::max(1, ui.sample_points);
    g.hgap = std::max(1, ui.valid_length_percent);
  }
  if (g.has_circle_gauge || g.tool == "FindCircle") {
    g.gap = std::max(1, ui.sample_points);
  }
  g.method = ui.edge_mode;
  g.dirty = true;
}

void ResetKeyParameterUiDefaults(ManualTestContext &context) {
  ManualParamRegressionState &ui = context.param_regression;
  ui.edge_mode = 0;
  ui.contrast_percent = 20;
  ui.valid_length_percent = 50;
  ui.interference_length_percent = 20;
  ui.roughness = 8;
  ui.burr_filter_percent = 0;
  ui.measure_order = 3;
  ui.black_index = 50;
  ui.sample_points = 8;
  ui.catch_method = 0;
  ui.enable_fast_measure = true;
  ui.enable_filter = true;
  SyncKeyParameterUiToGauge(context);
}

static std::string NormalizeKeyParamToolTypeLocal(const std::string &type) {
  if (type == "Findline" || type == "FindLine")
    return "FindLine";
  if (type == "Findcircle" || type == "FindCircle")
    return "FindCircle";
  if (type == "Findellipse" || type == "FindEllipse")
    return "FindEllipse";
  if (type == "Findrect" || type == "FindRect")
    return "FindRect";
  if (type == "FindSegmentation" || type == "findsegmentation" ||
      type == "Findsegmentation" || type == "segmentation")
    return "FindSegmentation";
  if (type == "FindObject" || type == "findobject")
    return "FindObject";
  if (type == "fastmatch" || type == "FastMatch" || type == "CFastMatch")
    return "FastMatch";
  if (type == "gridpatternclasstool" || type == "GridPatternClassTool")
    return "GridPatternClassTool";
  if (type == "regionpatterntool" || type == "RegionPatternTool")
    return "RegionPatternTool";
  return type;
}

static int RuntimeIntOr(const ManualTestContext &context,
                        const std::string &key, int fallback) {
  const auto it = context.runtime_int_vars.find(key);
  if (it == context.runtime_int_vars.end())
    return fallback;
  return it->second;
}

static std::string ToolFindSettingGlobalKey(const ManualGaugeState &gauge,
                                            const std::string &normalizedTool) {
  if (normalizedTool == "FindLine" || gauge.has_line_gauge)
    return "global_findline_objfilter";
  if (normalizedTool == "FindCircle" || gauge.has_circle_gauge)
    return "global_findcircle_findsetting";
  if (normalizedTool == "FindEllipse" || gauge.has_ellipse_gauge)
    return "global_findellipse_findsetting";
  if (normalizedTool == "FindRect")
    return "global_findrect_findsetting";
  if (normalizedTool == "FindObject")
    return "global_findobject_findsetting";
  if (normalizedTool == "FastMatch")
    return "global_objfilter";
  return "global_findsetting";
}

static void StageObjectPrefilterFindSetting(ManualTestContext &context,
                                            const std::string &reason) {
  ManualGaugeState &gauge = context.current_gauge;
  const std::string normalizedTool = NormalizeKeyParamToolTypeLocal(
      !gauge.primary_object_type.empty() ? gauge.primary_object_type
                                         : gauge.tool);
  gauge.findsetting = std::max(0, std::min(255, gauge.findsetting));

  InjectManualGaugeInt(context, "global_findsetting", gauge.findsetting);

  if (normalizedTool == "FindLine" || gauge.has_line_gauge) {
    InjectManualGaugeInt(context, "global_objfilter", gauge.findsetting);
    InjectManualGaugeInt(context, "global_findline_objfilter",
                         gauge.findsetting);
    InjectManualGaugeInt(context, "global_findline_findsetting",
                         gauge.findsetting);
  } else if (normalizedTool == "FindCircle" || gauge.has_circle_gauge) {
    InjectManualGaugeInt(context, "global_findcircle_findsetting",
                         gauge.findsetting);
  } else if (normalizedTool == "FindEllipse" || gauge.has_ellipse_gauge) {
    InjectManualGaugeInt(context, "global_findellipse_findsetting",
                         gauge.findsetting);
  } else if (normalizedTool == "FindRect") {
    InjectManualGaugeInt(context, "global_findrect_findsetting",
                         gauge.findsetting);
  } else if (normalizedTool == "FindObject") {
    InjectManualGaugeInt(context, "global_objfilter", gauge.findsetting);
    InjectManualGaugeInt(context, "global_findobject_findsetting",
                         gauge.findsetting);
  } else if (normalizedTool == "FastMatch") {
    InjectManualGaugeInt(context, "global_objfilter", gauge.findsetting);
  }

  context.debug_status = "OBJECT_PREFILTER_STAGED";
  context.debug_reason = reason +
                         ": findsetting=" + std::to_string(gauge.findsetting) +
                         " staged for " + normalizedTool;
}

static void
RestoreObjectPrefilterFindSettingFromStagedGlobals(ManualTestContext &context,
                                                   const std::string &reason) {
  ManualGaugeState &gauge = context.current_gauge;
  const std::string normalizedTool = NormalizeKeyParamToolTypeLocal(
      !gauge.primary_object_type.empty() ? gauge.primary_object_type
                                         : gauge.tool);
  const std::string toolKey = ToolFindSettingGlobalKey(gauge, normalizedTool);

  auto it = context.runtime_int_vars.find(toolKey);
  if (it == context.runtime_int_vars.end())
    it = context.runtime_int_vars.find("global_findsetting");
  if (it != context.runtime_int_vars.end())
    gauge.findsetting = std::max(0, std::min(255, it->second));

  StageObjectPrefilterFindSetting(context, reason);
}

static bool DrawRuntimeIntRow(ManualTestContext &context, const char *label,
                              const char *key, int fallback, int minValue,
                              int maxValue, float labelWidth = 110.0f) {
  int value = RuntimeIntOr(context, key, fallback);
  value = std::max(minValue, std::min(maxValue, value));
  bool edited = false;
  ImGui::TextUnformatted(label);
  ImGui::SameLine(labelWidth);
  ImGui::SetNextItemWidth(180.0f);
  edited |= ImGui::SliderInt((std::string("##slider_") + key).c_str(), &value,
                             minValue, maxValue);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(80.0f);
  edited |= ImGui::InputInt((std::string("##input_") + key).c_str(), &value);
  value = std::max(minValue, std::min(maxValue, value));
  if (edited ||
      context.runtime_int_vars.find(key) == context.runtime_int_vars.end())
    InjectManualGaugeInt(context, key, value);
  return edited;
}

static bool DrawFastMatchRoiControls(ManualTestContext &context) {
  bool edited = false;
  ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "FastMatch ROI ranges");
  ImGui::TextDisabled("Learn ROI builds the template model; Match ROI limits "
                      "the search range.");

  if (ImGui::BeginTable("fastmatch_roi_table", 5,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_SizingStretchProp)) {
    ImGui::TableSetupColumn("Range");
    ImGui::TableSetupColumn("x");
    ImGui::TableSetupColumn("y");
    ImGui::TableSetupColumn("w");
    ImGui::TableSetupColumn("h");
    ImGui::TableHeadersRow();

    auto drawRoiRow = [&](const char *rowLabel, const char *keyX,
                          const char *keyY, const char *keyW, const char *keyH,
                          int fallbackX, int fallbackY, int fallbackW,
                          int fallbackH) {
      int x = RuntimeIntOr(context, keyX, fallbackX);
      int y = RuntimeIntOr(context, keyY, fallbackY);
      int w = RuntimeIntOr(context, keyW, fallbackW);
      int h = RuntimeIntOr(context, keyH, fallbackH);
      x = std::max(0, std::min(10000, x));
      y = std::max(0, std::min(10000, y));
      w = std::max(1, std::min(10000, w));
      h = std::max(1, std::min(10000, h));

      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(rowLabel);
      ImGui::TableSetColumnIndex(1);
      ImGui::SetNextItemWidth(-FLT_MIN);
      edited |= ImGui::InputInt((std::string("##") + keyX).c_str(), &x);
      ImGui::TableSetColumnIndex(2);
      ImGui::SetNextItemWidth(-FLT_MIN);
      edited |= ImGui::InputInt((std::string("##") + keyY).c_str(), &y);
      ImGui::TableSetColumnIndex(3);
      ImGui::SetNextItemWidth(-FLT_MIN);
      edited |= ImGui::InputInt((std::string("##") + keyW).c_str(), &w);
      ImGui::TableSetColumnIndex(4);
      ImGui::SetNextItemWidth(-FLT_MIN);
      edited |= ImGui::InputInt((std::string("##") + keyH).c_str(), &h);

      x = std::max(0, std::min(10000, x));
      y = std::max(0, std::min(10000, y));
      w = std::max(1, std::min(10000, w));
      h = std::max(1, std::min(10000, h));
      InjectManualGaugeInt(context, keyX, x);
      InjectManualGaugeInt(context, keyY, y);
      InjectManualGaugeInt(context, keyW, w);
      InjectManualGaugeInt(context, keyH, h);
    };

    drawRoiRow("Learn ROI", "global_learn_roi_x", "global_learn_roi_y",
               "global_learn_roi_w", "global_learn_roi_h", 120, 120, 120, 90);
    drawRoiRow("Match ROI", "global_search_roi_x", "global_search_roi_y",
               "global_search_roi_w", "global_search_roi_h", 0, 0, 640, 480);
    ImGui::EndTable();
  }
  ImGui::SeparatorText("Template Geometry / Pose");
  edited |= DrawRuntimeIntRow(context, "FindObject source index",
                              "global_fastmatch_geometry_source_index", 0,
                              0, 10000, 190.0f);
  edited |= DrawRuntimeIntRow(context, "geometry weight percent",
                              "global_fastmatch_geometry_weight_percent", 25,
                              0, 100, 190.0f);
  edited |= DrawRuntimeIntRow(context, "maximum pose candidates",
                              "global_fastmatch_max_pose_candidates", 32,
                              1, 1000, 190.0f);
  ImGui::TextDisabled(
      "Template snapshot: source object ref + normalized boundary + centroid "
      "+ principal axes. Candidate score combines appearance and geometry.");
  return edited;
}

static bool DrawCxTextInspectControls(ManualTestContext &context) {
  bool edited = false;
  ImGui::SeparatorText("Detection / Component Split");

  ImGui::SeparatorText("Candidate Provenance");
  edited |= DrawRuntimeIntRow(context, "glyph source 0:auto 1:pose 2:object",
                              "global_ocr_glyph_source", 0, 0, 2, 245.0f);
  ImGui::TextDisabled(
      "Auto uses FastMatch pose when available, otherwise OCR-specific "
      "FindObject components. Area/distance apply to the object source.");


  edited |= DrawRuntimeIntRow(context, "binary threshold",
                              "global_ocr_threshold", 110, 0, 255, 160.0f);
  edited |= DrawRuntimeIntRow(context, "foreground mode",
                              "global_ocr_foreground_mode", 1, 1, 3, 160.0f);
  edited |= DrawRuntimeIntRow(context, "minimum glyph area",
                              "global_ocr_min_area", 1, 0, 100000, 160.0f);
  edited |= DrawRuntimeIntRow(context, "component distance",
                              "global_ocr_component_distance", 6, 0, 100,
                              160.0f);

  ImGui::SeparatorText("Geometry / Reading Layout");
  edited |= DrawRuntimeIntRow(context, "layout 0:auto 1:H 2:V",
                              "global_ocr_layout_direction", 0, 0, 2, 190.0f);
  edited |= DrawRuntimeIntRow(context, "line overlap percent",
                              "global_ocr_line_overlap_percent", 50, 1, 100,
                              190.0f);

  ImGui::SeparatorText("Matching / Decoding");
  edited |= DrawRuntimeIntRow(context, "match threshold",
                              "global_ocr_match_threshold", 3, 0, 255, 160.0f);
  edited |= DrawRuntimeIntRow(context, "minimum score percent",
                              "global_ocr_min_score_percent", 50, 0, 100,
                              180.0f);
  ImGui::TextDisabled(
      "Overlay: glyph bbox + main axis + reading order; object_state.json "
      "stores source object ref, line/order, label, confidence and failure.");
  return edited;
}

static bool DrawFindSegmentationPromptControls(ManualTestContext &context) {
  ManualGaugeState &gauge = context.current_gauge;
  bool edited = false;

  if (!gauge.has_segmentation_prompt_rect) {
    gauge.segmentation_prompt_x0 =
        RuntimeIntOr(context, "global_roi_x0", gauge.segmentation_prompt_x0);
    gauge.segmentation_prompt_y0 =
        RuntimeIntOr(context, "global_roi_y0", gauge.segmentation_prompt_y0);
    gauge.segmentation_prompt_x1 =
        RuntimeIntOr(context, "global_roi_x1", gauge.segmentation_prompt_x1);
    gauge.segmentation_prompt_y1 =
        RuntimeIntOr(context, "global_roi_y1", gauge.segmentation_prompt_y1);
    gauge.segmentation_mode = RuntimeIntOr(context, "global_segmentation_mode",
                                           gauge.segmentation_mode);
    gauge.segmentation_threshold_percent =
        RuntimeIntOr(context, "global_segmentation_threshold_percent",
                     gauge.segmentation_threshold_percent);
    gauge.has_segmentation_positive_point =
        RuntimeIntOr(context, "global_segmentation_positive_enabled", 0) != 0;
    gauge.segmentation_positive_x =
        RuntimeIntOr(context, "global_segmentation_positive_x",
                     gauge.segmentation_positive_x);
    gauge.segmentation_positive_y =
        RuntimeIntOr(context, "global_segmentation_positive_y",
                     gauge.segmentation_positive_y);
    gauge.has_segmentation_negative_point =
        RuntimeIntOr(context, "global_segmentation_negative_enabled", 0) != 0;
    gauge.segmentation_negative_x =
        RuntimeIntOr(context, "global_segmentation_negative_x",
                     gauge.segmentation_negative_x);
    gauge.segmentation_negative_y =
        RuntimeIntOr(context, "global_segmentation_negative_y",
                     gauge.segmentation_negative_y);
    gauge.has_segmentation_prompt_rect = true;
  }
  SeedSegmentationPromptListsFromLegacyFields(gauge);

  if (ImGui::CollapsingHeader("Prompt ROI / Mode / Threshold",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::TextColored(ImVec4(0.35f, 0.88f, 0.62f, 1.0f),
                       "FindSegmentation Prompt ROI");
    ImGui::TextDisabled("Prompt rect is the editable region shown in Image "
                        "View and exported to global_roi_*.");

    if (ImGui::BeginTable("findsegmentation_prompt_rect_table", 5,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_SizingStretchProp)) {
      ImGui::TableSetupColumn("Prompt");
      ImGui::TableSetupColumn("x0");
      ImGui::TableSetupColumn("y0");
      ImGui::TableSetupColumn("x1");
      ImGui::TableSetupColumn("y1");
      ImGui::TableHeadersRow();
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted("Prompt ROI");
      ImGui::TableSetColumnIndex(1);
      ImGui::SetNextItemWidth(-FLT_MIN);
      edited |=
          ImGui::InputInt("##seg_prompt_x0", &gauge.segmentation_prompt_x0);
      ImGui::TableSetColumnIndex(2);
      ImGui::SetNextItemWidth(-FLT_MIN);
      edited |=
          ImGui::InputInt("##seg_prompt_y0", &gauge.segmentation_prompt_y0);
      ImGui::TableSetColumnIndex(3);
      ImGui::SetNextItemWidth(-FLT_MIN);
      edited |=
          ImGui::InputInt("##seg_prompt_x1", &gauge.segmentation_prompt_x1);
      ImGui::TableSetColumnIndex(4);
      ImGui::SetNextItemWidth(-FLT_MIN);
      edited |=
          ImGui::InputInt("##seg_prompt_y1", &gauge.segmentation_prompt_y1);
      ImGui::EndTable();
    }

    ImGui::TextUnformatted("mode");
    ImGui::SameLine(80.0f);
    ImGui::SetNextItemWidth(120.0f);
    edited |= ImGui::InputInt("##segmentation_mode", &gauge.segmentation_mode);
    gauge.segmentation_mode =
        std::max(0, std::min(16, gauge.segmentation_mode));

    ImGui::SameLine();
    ImGui::TextUnformatted("threshold %");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.0f);
    edited |= ImGui::InputInt("##segmentation_threshold_percent",
                              &gauge.segmentation_threshold_percent);
    gauge.segmentation_threshold_percent =
        std::max(0, std::min(100, gauge.segmentation_threshold_percent));
  }

  gauge.segmentation_prompt_x0 = std::max(0, gauge.segmentation_prompt_x0);
  gauge.segmentation_prompt_y0 = std::max(0, gauge.segmentation_prompt_y0);
  gauge.segmentation_prompt_x1 =
      std::max(gauge.segmentation_prompt_x0 + 1, gauge.segmentation_prompt_x1);
  gauge.segmentation_prompt_y1 =
      std::max(gauge.segmentation_prompt_y0 + 1, gauge.segmentation_prompt_y1);

  ImGui::Separator();
  ImGui::TextUnformatted("Prompt point pick mode");
  ImGui::TextDisabled("Pick + / Pick - are mutually exclusive.  A point is "
                      "enabled only after clicking Image View.");
  const bool pickingPositive = gauge.segmentation_prompt_pick_mode == 1;
  const bool pickingNegative = gauge.segmentation_prompt_pick_mode == 2;

  if (pickingPositive)
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.68f, 0.34f, 1.0f));
  if (ImGui::Button("Pick + Point", ImVec2(118.0f, 0.0f))) {
    gauge.segmentation_prompt_pick_mode = pickingPositive ? 0 : 1;
    context.pending_annotation_tool_id =
        gauge.segmentation_prompt_pick_mode == 1
            ? "findsegmentation_positive_prompt"
            : "__pointer_pan__";
    context.pending_annotation_tool_reason =
        gauge.segmentation_prompt_pick_mode == 1
            ? "FindSegmentation positive prompt pick requested from Key "
              "Parameter Controls"
            : "FindSegmentation prompt pick cancelled from Key Parameter "
              "Controls";
    edited = true;
  }
  if (pickingPositive)
    ImGui::PopStyleColor();
  ImGui::SameLine();
  if (pickingNegative)
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.78f, 0.30f, 0.28f, 1.0f));
  if (ImGui::Button("Pick - Point", ImVec2(118.0f, 0.0f))) {
    gauge.segmentation_prompt_pick_mode = pickingNegative ? 0 : 2;
    context.pending_annotation_tool_id =
        gauge.segmentation_prompt_pick_mode == 2
            ? "findsegmentation_negative_prompt"
            : "__pointer_pan__";
    context.pending_annotation_tool_reason =
        gauge.segmentation_prompt_pick_mode == 2
            ? "FindSegmentation negative prompt pick requested from Key "
              "Parameter Controls"
            : "FindSegmentation prompt pick cancelled from Key Parameter "
              "Controls";
    edited = true;
  }
  if (pickingNegative)
    ImGui::PopStyleColor();
  ImGui::SameLine();
  if (ImGui::Button("Clear Prompt Points", ImVec2(150.0f, 0.0f))) {
    gauge.segmentation_prompt_pick_mode = 0;
    gauge.segmentation_positive_points.clear();
    gauge.segmentation_negative_points.clear();
    gauge.has_segmentation_positive_point = false;
    gauge.segmentation_positive_x = 0;
    gauge.segmentation_positive_y = 0;
    gauge.has_segmentation_negative_point = false;
    gauge.segmentation_negative_x = 0;
    gauge.segmentation_negative_y = 0;
    SyncSegmentationLegacyPointFromLists(gauge);
    context.pending_annotation_tool_id = "__pointer_pan__";
    context.pending_annotation_tool_reason =
        "FindSegmentation prompt points cleared from Key Parameter Controls";
    edited = true;
  }

  const char *activePromptMode =
      gauge.segmentation_prompt_pick_mode == 1
          ? "positive"
          : (gauge.segmentation_prompt_pick_mode == 2 ? "negative" : "none");
  ImGui::Text("active pick: %s", activePromptMode);

  ImGui::SameLine();
  ImGui::TextDisabled(
      "positive=%d negative=%d",
      static_cast<int>(gauge.segmentation_positive_points.size()),
      static_cast<int>(gauge.segmentation_negative_points.size()));

  if (ImGui::Button("Clear + List", ImVec2(118.0f, 0.0f))) {
    gauge.segmentation_positive_points.clear();
    gauge.has_segmentation_positive_point = false;
    gauge.segmentation_positive_x = 0;
    gauge.segmentation_positive_y = 0;
    if (gauge.segmentation_prompt_pick_mode == 1) {
      gauge.segmentation_prompt_pick_mode = 0;
      context.pending_annotation_tool_id = "__pointer_pan__";
    }
    context.pending_annotation_tool_reason =
        "FindSegmentation positive prompt list cleared from Key Parameter "
        "Controls";
    SyncSegmentationLegacyPointFromLists(gauge);
    edited = true;
  }
  ImGui::SameLine();
  if (ImGui::Button("Clear - List", ImVec2(118.0f, 0.0f))) {
    gauge.segmentation_negative_points.clear();
    gauge.has_segmentation_negative_point = false;
    gauge.segmentation_negative_x = 0;
    gauge.segmentation_negative_y = 0;
    if (gauge.segmentation_prompt_pick_mode == 2) {
      gauge.segmentation_prompt_pick_mode = 0;
      context.pending_annotation_tool_id = "__pointer_pan__";
    }
    context.pending_annotation_tool_reason =
        "FindSegmentation negative prompt list cleared from Key Parameter "
        "Controls";
    SyncSegmentationLegacyPointFromLists(gauge);
    edited = true;
  }

  edited |= DrawSegmentationPromptPointList("positive",
                                            ImVec4(0.40f, 1.0f, 0.45f, 1.0f),
                                            gauge.segmentation_positive_points);
  edited |= DrawSegmentationPromptPointList("negative",
                                            ImVec4(1.0f, 0.45f, 0.40f, 1.0f),
                                            gauge.segmentation_negative_points);
  SyncSegmentationLegacyPointFromLists(gauge);
  gauge.segmentation_mode = std::max(0, std::min(16, gauge.segmentation_mode));
  gauge.segmentation_threshold_percent =
      std::max(0, std::min(100, gauge.segmentation_threshold_percent));

  InjectManualGaugeInt(context, "global_roi_x0", gauge.segmentation_prompt_x0);
  InjectManualGaugeInt(context, "global_roi_y0", gauge.segmentation_prompt_y0);
  InjectManualGaugeInt(context, "global_roi_x1", gauge.segmentation_prompt_x1);
  InjectManualGaugeInt(context, "global_roi_y1", gauge.segmentation_prompt_y1);
  InjectManualGaugeInt(context, "global_roi_x", gauge.segmentation_prompt_x0);
  InjectManualGaugeInt(context, "global_roi_y", gauge.segmentation_prompt_y0);
  InjectManualGaugeInt(context, "global_roi_width",
                       gauge.segmentation_prompt_x1 -
                           gauge.segmentation_prompt_x0);
  InjectManualGaugeInt(context, "global_roi_height",
                       gauge.segmentation_prompt_y1 -
                           gauge.segmentation_prompt_y0);
  InjectManualGaugeInt(context, "global_segmentation_mode",
                       gauge.segmentation_mode);
  InjectManualGaugeInt(context, "global_segmentation_threshold_percent",
                       gauge.segmentation_threshold_percent);
  InjectManualGaugeInt(context, "global_segmentation_positive_enabled",
                       gauge.has_segmentation_positive_point ? 1 : 0);
  InjectManualGaugeInt(context, "global_segmentation_positive_x",
                       gauge.segmentation_positive_x);
  InjectManualGaugeInt(context, "global_segmentation_positive_y",
                       gauge.segmentation_positive_y);
  InjectManualGaugeInt(context, "global_segmentation_negative_enabled",
                       gauge.has_segmentation_negative_point ? 1 : 0);
  InjectManualGaugeInt(context, "global_segmentation_negative_x",
                       gauge.segmentation_negative_x);
  InjectManualGaugeInt(context, "global_segmentation_negative_y",
                       gauge.segmentation_negative_y);

  return edited;
}

static bool DrawFindObjectComponentControls(ManualTestContext &context) {
  ManualGaugeState &gauge = context.current_gauge;
  bool edited = false;
  if (!gauge.has_findobject_roi) {
    gauge.findobject_x0 =
        RuntimeIntOr(context, "global_roi_x0", gauge.findobject_x0);
    gauge.findobject_y0 =
        RuntimeIntOr(context, "global_roi_y0", gauge.findobject_y0);
    gauge.findobject_x1 =
        RuntimeIntOr(context, "global_roi_x1", gauge.findobject_x1);
    gauge.findobject_y1 =
        RuntimeIntOr(context, "global_roi_y1", gauge.findobject_y1);
    gauge.findobject_foreground_mode =
        RuntimeIntOr(context, "global_object_foreground_mode",
                     gauge.findobject_foreground_mode);
    gauge.findobject_threshold = RuntimeIntOr(
        context, "global_object_threshold", gauge.findobject_threshold);
    gauge.findobject_min_area = RuntimeIntOr(context, "global_object_min_area",
                                             gauge.findobject_min_area);
    gauge.has_findobject_roi = true;
  }

  ImGui::TextColored(
      ImVec4(0.95f, 0.75f, 0.35f, 1.0f),
      "FindObject: ROI -> thresholded components -> result boxes");
  ImGui::TextDisabled("Mode: 1 bright foreground, 2 dark foreground, 3 both. "
                      "Results are not editable.");
  if (ImGui::BeginTable("findobject_roi_table", 4,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_SizingStretchProp)) {
    ImGui::TableSetupColumn("x0");
    ImGui::TableSetupColumn("y0");
    ImGui::TableSetupColumn("x1");
    ImGui::TableSetupColumn("y1");
    ImGui::TableHeadersRow();
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::SetNextItemWidth(-FLT_MIN);
    edited |= ImGui::InputInt("##findobject_x0", &gauge.findobject_x0);
    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-FLT_MIN);
    edited |= ImGui::InputInt("##findobject_y0", &gauge.findobject_y0);
    ImGui::TableSetColumnIndex(2);
    ImGui::SetNextItemWidth(-FLT_MIN);
    edited |= ImGui::InputInt("##findobject_x1", &gauge.findobject_x1);
    ImGui::TableSetColumnIndex(3);
    ImGui::SetNextItemWidth(-FLT_MIN);
    edited |= ImGui::InputInt("##findobject_y1", &gauge.findobject_y1);
    ImGui::EndTable();
  }
  ImGui::SetNextItemWidth(120.0f);
  edited |=
      ImGui::InputInt("foreground mode", &gauge.findobject_foreground_mode);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(120.0f);
  edited |= ImGui::InputInt("binary threshold", &gauge.findobject_threshold);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(120.0f);
  edited |= ImGui::InputInt("minimum area", &gauge.findobject_min_area);
  ImGui::SeparatorText("Result Conclusion");
  const char* conclusionItems[] = {"Object contour (centroid)", "Bounding rectangle (box center)", "Moment ellipse (centroid)", "Maximum inscribed circle (circle center)", "Minimum enclosing circle (circle center)"};
  ImGui::SetNextItemWidth(320.0f);
  edited |= ImGui::Combo("conclusion shape", &gauge.findobject_conclusion_shape, conclusionItems, IM_ARRAYSIZE(conclusionItems));


  ImGui::SeparatorText("Geometry Reconstruction / Measurement");
  ImGui::TextDisabled(
      "Boundary: Gwyddion 2x2 pixel configuration; coordinates use a "
      "half-open bbox and value-semantic per-object snapshots.");
  ImGui::SetNextItemWidth(150.0f);
  edited |= ImGui::InputDouble("pixel size X",
                               &gauge.findobject_pixel_size_x,
                               0.01, 0.1, "%.6f");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(150.0f);
  edited |= ImGui::InputDouble("pixel size Y",
                               &gauge.findobject_pixel_size_y,
                               0.01, 0.1, "%.6f");
  const char* connectivity_items[] = {"4-connected", "8-connected"};
  int connectivity_index =
      gauge.findobject_geometry_connectivity == 4 ? 0 : 1;
  ImGui::SetNextItemWidth(150.0f);
  if (ImGui::Combo("component topology", &connectivity_index,
                   connectivity_items,
                   IM_ARRAYSIZE(connectivity_items)))
  {
    gauge.findobject_geometry_connectivity =
        connectivity_index == 0 ? 4 : 8;
    edited = true;
  }
  ImGui::SameLine();
  ImGui::SetNextItemWidth(120.0f);
  edited |= ImGui::InputInt(
      "selected object", &gauge.findobject_selected_measurement);
  ImGui::SameLine();
  edited |= ImGui::Checkbox("show all same-type conclusions",
                            &gauge.findobject_show_all_conclusions);

  edited |= ImGui::Checkbox("boundary overlay",
                            &gauge.findobject_show_boundary);
  ImGui::SameLine();
  edited |= ImGui::Checkbox("moment ellipse",
                            &gauge.findobject_show_moment_ellipse);
  ImGui::SameLine();
  edited |= ImGui::Checkbox("Feret",
                            &gauge.findobject_show_feret);
  ImGui::SameLine();
  edited |= ImGui::Checkbox("inscribed/enclosing circles",
                            &gauge.findobject_show_circles);
  ImGui::SeparatorText("Background Baseline");
  const char* background_methods[] = {
      "None (raw intensity)", "ROI border robust", "Morphological opening"};
  ImGui::SetNextItemWidth(220.0f);
  edited |= ImGui::Combo("background method",
                         &gauge.findobject_background_method,
                         background_methods, IM_ARRAYSIZE(background_methods));
  ImGui::SameLine();
  ImGui::SetNextItemWidth(120.0f);
  edited |= ImGui::InputInt("border width px",
                            &gauge.findobject_background_border_px);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(140.0f);
  edited |= ImGui::InputInt("opening radius px",
                            &gauge.findobject_background_radius_px);

  ImGui::SeparatorText("Subpixel Boundary Refinement");
  edited |= ImGui::Checkbox("enable subpixel contour",
                            &gauge.findobject_subpixel_enabled);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(150.0f);
  edited |= ImGui::InputDouble("minimum gradient",
                               &gauge.findobject_subpixel_min_gradient,
                               0.1, 1.0, "%.3f");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(150.0f);
  edited |= ImGui::InputDouble("iso threshold",
                               &gauge.findobject_subpixel_iso_threshold,
                               0.1, 1.0, "%.3f");
  const char* geometry_basis_items[] = {
      "GW discrete baseline", "Subpixel when valid"};
  ImGui::SetNextItemWidth(220.0f);
  edited |= ImGui::Combo("active geometry basis",
                         &gauge.findobject_geometry_basis,
                         geometry_basis_items,
                         IM_ARRAYSIZE(geometry_basis_items));
  ImGui::TextDisabled(
      "Topology is fixed by the binary mask. Subpixel refines coordinates; "
      "GW values remain immutable evidence.");

  ImGui::TextWrapped(
      "Evidence outputs: intensity min/max/mean/stddev/skewness; pixel and "
      "projected area; equivalent side/radius; Gwyddion and polygon "
      "perimeters; closed outer/hole boundaries; convex hull, circularity "
      "and solidity; moment major/minor axes, orientation, aspect ratio and "
      "eccentricity; min/max Feret; maximum inscribed and minimum enclosing "
      "circles.");
  gauge.findobject_pixel_size_x =
      std::max(1e-9, gauge.findobject_pixel_size_x);
  gauge.findobject_pixel_size_y =
      std::max(1e-9, gauge.findobject_pixel_size_y);
  gauge.findobject_selected_measurement =
      std::max(0, gauge.findobject_selected_measurement);

  gauge.findobject_background_method =
      std::max(0, std::min(2, gauge.findobject_background_method));
  gauge.findobject_background_border_px =
      std::max(1, gauge.findobject_background_border_px);
  gauge.findobject_background_radius_px =
      std::max(1, gauge.findobject_background_radius_px);
  gauge.findobject_subpixel_min_gradient =
      std::max(0.0, gauge.findobject_subpixel_min_gradient);
  gauge.findobject_geometry_basis =
      gauge.findobject_geometry_basis == 1 ? 1 : 0;

  gauge.findobject_x0 = std::max(0, gauge.findobject_x0);
  gauge.findobject_y0 = std::max(0, gauge.findobject_y0);
  gauge.findobject_x1 = std::max(gauge.findobject_x0 + 1, gauge.findobject_x1);
  gauge.findobject_y1 = std::max(gauge.findobject_y0 + 1, gauge.findobject_y1);
  gauge.findobject_foreground_mode =
      std::max(1, std::min(3, gauge.findobject_foreground_mode));
  gauge.findobject_threshold =
      std::max(0, std::min(255, gauge.findobject_threshold));
  gauge.findobject_min_area = std::max(1, gauge.findobject_min_area);
  gauge.findobject_conclusion_shape = std::max(0, std::min(4, gauge.findobject_conclusion_shape));
  gauge.findobject_show_boundary = gauge.findobject_conclusion_shape == 0;
  gauge.findobject_show_moment_ellipse = gauge.findobject_conclusion_shape == 2;
  gauge.findobject_show_circles = gauge.findobject_conclusion_shape >= 3;
  gauge.threshold = gauge.findobject_threshold;
  gauge.method = gauge.findobject_foreground_mode;

  InjectManualGaugeInt(
      context, "global_object_pixel_size_x_milli",
      static_cast<int>(std::lround(
          gauge.findobject_pixel_size_x * 1000.0)));
  InjectManualGaugeInt(
      context, "global_object_pixel_size_y_milli",
      static_cast<int>(std::lround(
          gauge.findobject_pixel_size_y * 1000.0)));
  InjectManualGaugeInt(context, "global_object_geometry_connectivity",
                       gauge.findobject_geometry_connectivity);
  InjectManualGaugeInt(context, "global_object_selected_measurement",
                       gauge.findobject_show_all_conclusions
                           ? -1 : gauge.findobject_selected_measurement);
  InjectManualGaugeInt(context, "global_object_show_boundary",
                       gauge.findobject_show_boundary ? 1 : 0);
  InjectManualGaugeInt(context, "global_object_show_moment_ellipse",
                       gauge.findobject_show_moment_ellipse ? 1 : 0);
  InjectManualGaugeInt(context, "global_object_show_feret",
                       gauge.findobject_show_feret ? 1 : 0);
  InjectManualGaugeInt(context, "global_object_show_circles",
                       gauge.findobject_show_circles ? 1 : 0);
  InjectManualGaugeInt(context, "global_object_conclusion_shape", gauge.findobject_conclusion_shape);
  InjectManualGaugeInt(context, "global_object_background_method",
                       gauge.findobject_background_method);
  InjectManualGaugeInt(context, "global_object_background_border_px",
                       gauge.findobject_background_border_px);
  InjectManualGaugeInt(context, "global_object_background_radius_px",
                       gauge.findobject_background_radius_px);
  InjectManualGaugeInt(context, "global_object_subpixel_enabled",
                       gauge.findobject_subpixel_enabled ? 1 : 0);
  InjectManualGaugeInt(
      context, "global_object_subpixel_min_gradient_milli",
      static_cast<int>(std::lround(
          gauge.findobject_subpixel_min_gradient * 1000.0)));
  InjectManualGaugeInt(
      context, "global_object_subpixel_iso_threshold",
      static_cast<int>(std::lround(gauge.findobject_subpixel_iso_threshold)));
  InjectManualGaugeInt(context, "global_object_geometry_basis",
                       gauge.findobject_geometry_basis);
  return edited;
}

static bool DrawFastMatchLearnParameterControls(ManualTestContext &context) {
  ManualGaugeState &gauge = context.current_gauge;
  bool edited = false;
  bool sharedParamsEdited = false;
  ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Learn Setup");
  ImGui::TextDisabled("Maps to: setrect + setscanrotation + setobjfilter + "
                      "SetWHgap + setthre + setlinegap + "
                      "setcomparegap.");


  ImGui::TextUnformatted("shared learn params");
  ImGui::TextDisabled("FastMatch learn internally uses four directional "
                      "FindLine probes: Top, Bottom, Left, Right.");

  ImGui::TextUnformatted("learn_threshold");
  ImGui::TextDisabled("Maps to: setrect + setscanrotation + setobjfilter + "
                      "SetWHgap + setthre + setlinegap + "
                      "setcomparegap.");

  ImGui::SameLine();
  ImGui::SetNextItemWidth(80.0f);
  const bool thresholdEdited =
      ImGui::InputInt("##fm_learn_threshold_value", &gauge.threshold);
  edited |= thresholdEdited;
  sharedParamsEdited |= thresholdEdited;
  gauge.threshold = std::max(0, std::min(255, gauge.threshold));

  ImGui::TextUnformatted("learn_method");
  ImGui::SameLine(130.0f);
  const char *methods[] = {"0", "1", "2", "3"};
  ImGui::SetNextItemWidth(100.0f);
  const bool methodEdited = ImGui::Combo(
      "##fm_learn_method", &gauge.method, methods, IM_ARRAYSIZE(methods));
  edited |= methodEdited;
  sharedParamsEdited |= methodEdited;

  ImGui::TextUnformatted("learn_linegap");
  ImGui::SameLine(130.0f);
  ImGui::SetNextItemWidth(180.0f);
  bool linegapEdited =
      ImGui::SliderInt("##fm_learn_linegap", &gauge.linegap, 0, 50);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(80.0f);
  linegapEdited |=
      ImGui::InputInt("##fm_learn_linegap_value", &gauge.linegap);
  edited |= linegapEdited;
  sharedParamsEdited |= linegapEdited;
  gauge.linegap = std::max(0, std::min(50, gauge.linegap));

  ImGui::TextUnformatted("learn_wgap");
  ImGui::SameLine(130.0f);
  ImGui::SetNextItemWidth(180.0f);
  bool wgapEdited = ImGui::SliderInt("##fm_learn_wgap", &gauge.wgap, 0, 100);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(80.0f);
  wgapEdited |= ImGui::InputInt("##fm_learn_wgap_value", &gauge.wgap);
  edited |= wgapEdited;
  sharedParamsEdited |= wgapEdited;
  gauge.wgap = std::max(0, std::min(100, gauge.wgap));

  ImGui::TextUnformatted("learn_hgap");
  ImGui::SameLine(130.0f);
  ImGui::SetNextItemWidth(180.0f);
  bool hgapEdited = ImGui::SliderInt("##fm_learn_hgap", &gauge.hgap, 0, 100);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(80.0f);
  hgapEdited |= ImGui::InputInt("##fm_learn_hgap_value", &gauge.hgap);
  edited |= hgapEdited;
  sharedParamsEdited |= hgapEdited;
  gauge.hgap = std::max(0, std::min(100, gauge.hgap));

  const bool objfilterEdited = DrawRuntimeIntRow(
      context, "objfilter", "global_objfilter", 1, 0, 10, 130.0f);
  edited |= objfilterEdited;
  sharedParamsEdited |= objfilterEdited;
  const bool compareGapEdited = DrawRuntimeIntRow(
      context, "compare_gap", "global_compare_gap", 20, 1, 200, 130.0f);
  edited |= compareGapEdited;
  sharedParamsEdited |= compareGapEdited;
  edited |= DrawRuntimeIntRow(context, "scan rotation deg",
                              "global_fastmatch_scan_rotation_deg", 0, -180,
                              180, 130.0f);

  ImGui::TextDisabled("0 keeps the original orthogonal FastMatch scan. Positive "
                      "and negative degrees rotate the four learn probe bands "
                      "around the Learn ROI center before learn/match.");
  ImGui::TextDisabled(
      "Rotation is template pose: ROI move/resize preserves it; only this "
      "control changes it. Image View shows the same orientation preview "
      "before Learn.");

  bool filterprofileEdited = ImGui::SliderInt(
      "##fm_learn_filterprofile", &gauge.filterprofile, 0, 10);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(80.0f);
  filterprofileEdited |= ImGui::InputInt("##fm_learn_filterprofile_value",
                                         &gauge.filterprofile);
  edited |= filterprofileEdited;
  sharedParamsEdited |= filterprofileEdited;
  gauge.filterprofile = std::max(0, std::min(10, gauge.filterprofile));

  InjectManualGaugeInt(context, "global_threshold", gauge.threshold);
  InjectManualGaugeInt(context, "global_method", gauge.method);
  InjectManualGaugeInt(context, "global_linegap", gauge.linegap);
  InjectManualGaugeInt(context, "global_wgap", gauge.wgap);
  InjectManualGaugeInt(context, "global_hgap", gauge.hgap);
  InjectManualGaugeInt(context, "global_filterprofile", gauge.filterprofile);

  int shared = RuntimeIntOr(context, "global_fastmatch_learn_shared", 0);
  shared = shared != 0 ? 1 : 0;
  bool sharedBool = shared != 0;
  const bool sharedModeChanged = ImGui::Checkbox(
      "Share learn values with paired exterior polarity", &sharedBool);
  if (sharedModeChanged) {
    shared = sharedBool ? 1 : 0;
    InjectManualGaugeInt(context, "global_fastmatch_learn_shared", shared);
    edited = true;
  } else {
    InjectManualGaugeInt(context, "global_fastmatch_learn_shared", shared);
  }

  const int sharedObjfilter = RuntimeIntOr(context, "global_objfilter", 1);
  const int sharedCompareGap = RuntimeIntOr(context, "global_compare_gap", 20);
  const char *directionLabels[4] = {"Top / A", "Bottom / B", "Left / A2",
                                    "Right / B2"};

  auto seedDirection = [&](int dir, int threshold, int method, int linegap,
                           int wgap, int hgap, int objfilter, int compareGap) {
    const std::string suffix = "_" + std::to_string(dir);
    InjectManualGaugeInt(context,
                         ("global_fastmatch_learn_threshold" + suffix).c_str(),
                         threshold);
    InjectManualGaugeInt(
        context, ("global_fastmatch_learn_method" + suffix).c_str(), method);
    InjectManualGaugeInt(
        context, ("global_fastmatch_learn_linegap" + suffix).c_str(), linegap);
    InjectManualGaugeInt(
        context, ("global_fastmatch_learn_wgap" + suffix).c_str(), wgap);
    InjectManualGaugeInt(
        context, ("global_fastmatch_learn_hgap" + suffix).c_str(), hgap);
    InjectManualGaugeInt(context,
                         ("global_fastmatch_learn_objfilter" + suffix).c_str(),
                         objfilter);
    InjectManualGaugeInt(
        context, ("global_fastmatch_learn_compare_gap" + suffix).c_str(),
        compareGap);
  };

  // Shared mode makes each shared edit one explicit write to all directions.
  // Physical widths are transposed for the vertical pair and transition
  // polarity is reversed for Bottom/Right. Never copy per frame because that
  // would destroy intentionally independent directional values.
  const bool copySharedButton =
      ImGui::Button("Copy Shared Learn Params To 4 Directions");
  const bool copySharedNow = copySharedButton ||
      (shared != 0 && (sharedModeChanged || sharedParamsEdited));
  if (copySharedNow) {
    for (int dir = 0; dir < 4; ++dir) {
      const bool horizontalEdge = dir < 2;
      const bool oppositeSide = dir == 1 || dir == 3;
      seedDirection(dir, gauge.threshold,
                    oppositeSide ? (gauge.method ^ 1) : gauge.method,
                    gauge.linegap,
                    horizontalEdge ? gauge.wgap : gauge.hgap,
                    horizontalEdge ? gauge.hgap : gauge.wgap,
                    sharedObjfilter, sharedCompareGap);
    }
    edited = true;
  }

  ImGui::SameLine();
  if (ImGui::Button("Apply Exterior-Edge Starter Profile")) {
    // This is an optional starting preset, never an implicit FastMatch
    // default.  A trained template owns its own saved per-direction values
    // through the Evidence Candidate runtime_globals/parameter_snapshot.
    // Top/Bottom scan horizontal bodies and Left/Right scan vertical bodies.
    shared = 0;
    InjectManualGaugeInt(context, "global_fastmatch_learn_shared", 0);
    const int oppositeMethod = std::clamp(gauge.method ^ 1, 0, 3);
    seedDirection(0, gauge.threshold, gauge.method, gauge.linegap, 8, 2,
                  sharedObjfilter, sharedCompareGap);
    seedDirection(1, gauge.threshold, oppositeMethod, gauge.linegap, 8, 2,
                  sharedObjfilter, sharedCompareGap);
    seedDirection(2, gauge.threshold, gauge.method, gauge.linegap, 2, 8,
                  sharedObjfilter, sharedCompareGap);
    seedDirection(3, gauge.threshold, oppositeMethod, gauge.linegap, 2, 8,
                  sharedObjfilter, sharedCompareGap);
    for (int dir = 0; dir < 4; ++dir) {
      const std::string suffix = "_" + std::to_string(dir);
      InjectManualGaugeInt(
          context, ("global_fastmatch_learn_edge_count" + suffix).c_str(), 2);
      InjectManualGaugeInt(
          context, ("global_fastmatch_learn_selected_edge" + suffix).c_str(),
          1);
    }
    edited = true;
  }
  ImGui::TextDisabled("Starter Profile only: paired normal windows plus Edge 1 for each physical side. Adjust freely, then Save Template Defaults to persist.");

  if (ImGui::CollapsingHeader("FastMatch Directional Edge Template",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
  ImGui::Indent(12.0f);
  ImGui::TextDisabled(
      "Four independent FastMatch template sides. FindLine is the probe "
      "implementation, not a separate parameter profile.");
  if (shared != 0) {
    ImGui::TextColored(
        ImVec4(1.0f, 0.75f, 0.20f, 1.0f),
        "Shared mode is active: change a tab only after disabling shared mode "
        "or explicitly copy the shared baseline.");
  }

  if (ImGui::BeginTabBar("fastmatch_directional_findline_tabs")) {
    for (int dir = 0; dir < 4; ++dir) {
      if (!ImGui::BeginTabItem(directionLabels[dir]))
        continue;

      // A selected tab is the one visualized by the independent Image View
      // diagnostic layer. This is intentionally not a hidden write to any of
      // the other three directional parameter buckets.
      context.fastmatch_selected_learn_edge = dir;
      const std::string suffix = "_" + std::to_string(dir);
      const std::string thresholdKey =
          "global_fastmatch_learn_threshold" + suffix;
      const std::string methodKey = "global_fastmatch_learn_method" + suffix;
      const std::string linegapKey =
          "global_fastmatch_learn_linegap" + suffix;
      const std::string wgapKey = "global_fastmatch_learn_wgap" + suffix;
      const std::string hgapKey = "global_fastmatch_learn_hgap" + suffix;
      const std::string objfilterKey =
          "global_fastmatch_learn_objfilter" + suffix;
      const std::string compareKey =
          "global_fastmatch_learn_compare_gap" + suffix;
      const std::string edgeCountKey =
          "global_fastmatch_learn_edge_count" + suffix;
      const std::string selectedEdgeKey =
          "global_fastmatch_learn_selected_edge" + suffix;

      int threshold = RuntimeIntOr(context, thresholdKey, gauge.threshold);
      int method = RuntimeIntOr(context, methodKey, gauge.method);
      int linegap = RuntimeIntOr(context, linegapKey, gauge.linegap);
      int wgap = RuntimeIntOr(context, wgapKey, gauge.wgap);
      int hgap = RuntimeIntOr(context, hgapKey, gauge.hgap);
      int objfilter = RuntimeIntOr(context, objfilterKey, sharedObjfilter);
      int compareGap = RuntimeIntOr(context, compareKey, sharedCompareGap);
      int edgeCount = RuntimeIntOr(context, edgeCountKey, 2);
      int selectedEdge = RuntimeIntOr(context, selectedEdgeKey, 0);

      threshold = std::clamp(threshold, 0, 255);
      method = std::clamp(method, 0, 3);
      linegap = std::clamp(linegap, 1, 200);
      wgap = std::clamp(wgap, 1, 500);
      hgap = std::clamp(hgap, 1, 500);
      objfilter = std::clamp(objfilter, 0, 10);
      compareGap = std::clamp(compareGap, 1, 500);
      edgeCount = std::clamp(edgeCount, 1, 16);
      selectedEdge = std::clamp(selectedEdge, -1, edgeCount);

      ImGui::PushID(dir);
      ImGui::TextColored(ImVec4(0.40f, 0.80f, 1.0f, 1.0f),
                         "FastMatch Direction Parameters");
      ImGui::TextDisabled(
          "Saved with this FastMatch template side; no value is copied to "
          "another direction automatically.");
      ImGui::TextColored(
          ImVec4(1.0f, 0.75f, 0.20f, 1.0f),
          "Learn composes all four physical sides. Full edge retains each "
          "accepted side candidate; Edge N or Last selects one candidate "
          "per Gauge Line for that direction.");
      if (ImGui::CollapsingHeader("Sampling & Detection",
                                  ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::Indent(10.0f);
      edited |= DrawRuntimeIntRow(context, "threshold", thresholdKey.c_str(),
                                  threshold, 0, 255, 170.0f);
      edited |= DrawRuntimeIntRow(context, "method", methodKey.c_str(),
                                  method, 0, 3, 170.0f);
      edited |= DrawRuntimeIntRow(context, "linegap", linegapKey.c_str(),
                                  linegap, 1, 200, 170.0f);
      edited |= DrawRuntimeIntRow(context, "wgap", wgapKey.c_str(), wgap,
                                  1, 500, 170.0f);
      edited |= DrawRuntimeIntRow(context, "hgap", hgapKey.c_str(), hgap,
                                  1, 500, 170.0f);
      edited |= DrawRuntimeIntRow(context, "object prefilter",
                                  objfilterKey.c_str(), objfilter, 0, 10,
                                  170.0f);
      edited |= DrawRuntimeIntRow(context, "compare gap", compareKey.c_str(),
                                  compareGap, 1, 500, 170.0f);
      ImGui::Unindent(10.0f);
      }
      if (ImGui::CollapsingHeader("Point Column & Model Contribution",
                                  ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::Indent(10.0f);
      ImGui::TextDisabled(
          "Choose the candidate ordinal for this directional FindLine Probe: "
          "0=Full edge, N=Nth edge, -1=Last edge.");
      ImGui::TextDisabled(
          "Top/Left count from FindLine's scan start. Bottom/Right count "
          "from the physical outer side (Edge 1 maps internally to Last). ");
      edited |= DrawRuntimeIntRow(context, "edge count", edgeCountKey.c_str(),
                                  edgeCount, 1, 16, 170.0f);
      std::string selectedEdgeLabel =
          selectedEdge == 0 ? "Full edge" :
          selectedEdge == -1 ? "Last edge" :
          "Edge " + std::to_string(selectedEdge);
      ImGui::TextUnformatted("selected edge");
      ImGui::SameLine(170.0f);
      ImGui::SetNextItemWidth(180.0f);
      if (ImGui::BeginCombo("##fastmatch_probe_selected_edge",
                            selectedEdgeLabel.c_str())) {
        if (ImGui::Selectable("Full edge", selectedEdge == 0)) {
          selectedEdge = 0;
          edited = true;
        }
        for (int edge = 1; edge <= edgeCount; ++edge) {
          const std::string label = "Edge " + std::to_string(edge);
          if (ImGui::Selectable(label.c_str(), selectedEdge == edge)) {
            selectedEdge = edge;
            edited = true;
          }
        }
        ImGui::Separator();
        if (ImGui::Selectable("Last edge", selectedEdge == -1)) {
          selectedEdge = -1;
          edited = true;
        }
        ImGui::EndCombo();
      }
      selectedEdge = std::clamp(selectedEdge, -1, edgeCount);
      InjectManualGaugeInt(context, selectedEdgeKey.c_str(), selectedEdge);
      if (edgeCount == 2 && selectedEdge == 2) {
        ImGui::TextDisabled(
            "Compatibility mapping: Edge 2 is executed as Last edge for this "
            "two-edge FindLine Probe.");
      }
      ImGui::Unindent(10.0f);
      }

      const bool horizontalEdge = dir < 2;
      if (ImGui::CollapsingHeader("Diagnostic Display")) {
      ImGui::Indent(10.0f);
      ImGui::TextDisabled(
          "%s normal default: wgap=%d hgap=%d. These values are saved for "
          "this probe and are not visual-only.",
          horizontalEdge ? "Vertical" : "Horizontal",
          horizontalEdge ? 8 : 2, horizontalEdge ? 2 : 8);
      ImGui::Checkbox("Show this direction's scan ticks",
                      &context.show_fastmatch_learn_scan_ticks);
      ImGui::TextDisabled(
          "Image View shows only this FastMatch direction diagnostic.");
      ImGui::Unindent(10.0f);
      }
      ImGui::PopID();
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
  ImGui::Unindent(12.0f);
  }
  if (ImGui::CollapsingHeader(
          "FastMatch Shape Model / FormfitGauge Bridge (P0-P6)",
          ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Indent(12.0f);
    bool formFitEnabled =
        RuntimeIntOr(context, "global_fastmatch_formfit_enabled", 0) != 0;
    if (ImGui::Checkbox("Enable dense bidirectional form-fit", &formFitEnabled)) {
      InjectManualGaugeInt(context, "global_fastmatch_formfit_enabled",
                           formFitEnabled ? 1 : 0);
      edited = true;
    }
    const bool scriptSupportsFormFit =
        context.editor_text.find("fastmatch_form_fit_binding_version: 1") !=
            std::string::npos &&
        context.editor_text.find("setformfitdenseparams") != std::string::npos &&
        context.editor_text.find("setformfitannparams") != std::string::npos;
    if (formFitEnabled && !scriptSupportsFormFit) {
      ImGui::TextColored(ImVec4(1.0f, 0.43f, 0.35f, 1.0f),
                         "SCRIPT_BINDING_STALE: create a compatible run before Learn.");
    }
    ImGui::TextDisabled(
        "FastMatch owns sparse conclusions, dense contour reconstruction and "
        "bidirectional correspondence. FormfitGauge receives a value-only "
        "PointSet/constraint snapshot; it does not rerun matching.");

    ImGui::SeparatorText("Dense contour and sub-pixel profile");
    edited |= DrawRuntimeIntRow(context, "dense sample step milli-px",
        "global_fastmatch_formfit_dense_step_milli_px", 1000, 100, 10000, 270.0f);
    edited |= DrawRuntimeIntRow(context, "normal profile half-width milli-px",
        "global_fastmatch_formfit_profile_half_width_milli_px", 2500, 250, 20000, 270.0f);
    edited |= DrawRuntimeIntRow(context, "normal profile step milli-px",
        "global_fastmatch_formfit_profile_step_milli_px", 250, 50, 5000, 270.0f);
    edited |= DrawRuntimeIntRow(context, "profile minimum gradient",
        "global_fastmatch_formfit_min_gradient", 4, 0, 255, 270.0f);
    edited |= DrawRuntimeIntRow(context, "curvature anchor threshold milli-deg",
        "global_fastmatch_formfit_curvature_threshold_millideg", 12000, 100, 180000, 270.0f);

    ImGui::SeparatorText("Bidirectional ANN correspondence");
    edited |= DrawRuntimeIntRow(context, "ANN search radius milli-px",
        "global_fastmatch_formfit_ann_radius_milli_px", 8000, 250, 100000, 270.0f);
    edited |= DrawRuntimeIntRow(context, "normal compatibility tolerance deg",
        "global_fastmatch_formfit_normal_tolerance_deg", 35, 1, 90, 270.0f);
    edited |= DrawRuntimeIntRow(context, "residual trim percent",
        "global_fastmatch_formfit_trim_percent", 20, 0, 80, 270.0f);
    edited |= DrawRuntimeIntRow(context, "minimum mutual pairs",
        "global_fastmatch_formfit_min_mutual_pairs", 6, 2, 100000, 270.0f);
    edited |= DrawRuntimeIntRow(context, "maximum refinement iterations",
        "global_fastmatch_formfit_max_iterations", 8, 1, 100, 270.0f);

    ImGui::SeparatorText("Budget and transform model");
    edited |= DrawRuntimeIntRow(context, "maximum elapsed ms",
        "global_fastmatch_formfit_max_elapsed_ms", 80, 1, 60000, 270.0f);
    edited |= DrawRuntimeIntRow(context, "maximum structural anchors",
        "global_fastmatch_formfit_max_anchors", 64, 4, 4096, 270.0f);
    edited |= DrawRuntimeIntRow(context, "allow non-uniform affine",
        "global_fastmatch_formfit_allow_affine", 1, 0, 1, 270.0f);
    ImGui::TextDisabled(
        "Robust defaults are intended to work without tuning. Manual controls "
        "are for low-contrast, curved, occluded or strongly anisotropic cases.");

    ImGui::SeparatorText("Last run evidence");
    ImGui::Text("status=%d reference=%d observed=%d mutual=%d anchors=%d",
        RuntimeIntOr(context, "global_fastmatch_formfit_status_code", 0),
        RuntimeIntOr(context, "global_fastmatch_formfit_reference_count", 0),
        RuntimeIntOr(context, "global_fastmatch_formfit_observed_count", 0),
        RuntimeIntOr(context, "global_fastmatch_formfit_mutual_count", 0),
        RuntimeIntOr(context, "global_fastmatch_formfit_anchor_count", 0));
    ImGui::Text("score=%.3f symmetric residual=%.3f px",
        RuntimeIntOr(context, "global_fastmatch_formfit_score_permille", 0) / 1000.0,
        RuntimeIntOr(context, "global_fastmatch_formfit_residual_milli_px", -1000) / 1000.0);
    ImGui::TextDisabled(
        "Evidence files: shape_model.json, formfit_correspondence.json and "
        "formfit_gauge.json. All overlays are render-only.");
    ImGui::Unindent(12.0f);
  }
  return edited;
}

static bool DrawGridPatternRoiControls(ManualTestContext &context) {
  bool edited = false;
  ImGui::TextColored(ImVec4(0.35f, 0.88f, 0.62f, 1.0f),
                     "GridPattern Analysis ROI");
  ImGui::TextDisabled("This ROI is the region-content input. FastMatch Search "
                      "ROI is not consumed by this CASE.");
  edited |= DrawRuntimeIntRow(context, "analysis x", "global_learn_roi_x", 120,
                              0, 10000, 130.0f);
  edited |= DrawRuntimeIntRow(context, "analysis y", "global_learn_roi_y", 120,
                              0, 10000, 130.0f);
  edited |= DrawRuntimeIntRow(context, "analysis width", "global_learn_roi_w",
                              120, 1, 10000, 130.0f);
  edited |= DrawRuntimeIntRow(context, "analysis height", "global_learn_roi_h",
                              90, 1, 10000, 130.0f);
  return edited;
}

static bool DrawRegionPatternRoiControls(ManualTestContext &context) {
  bool edited = false;
  ImGui::TextColored(ImVec4(0.9f, 0.72f, 0.32f, 1.0f),
                     "RegionPattern Analysis ROI");
  ImGui::TextDisabled("This ROI feeds the region-content descriptor. It is "
                      "independent from FastMatch learn/search ROI.");
  edited |= DrawRuntimeIntRow(context, "region x", "global_region_roi_x", 120,
                              0, 10000, 130.0f);
  edited |= DrawRuntimeIntRow(context, "region y", "global_region_roi_y", 120,
                              0, 10000, 130.0f);
  edited |= DrawRuntimeIntRow(context, "region width", "global_region_roi_w",
                              120, 1, 10000, 130.0f);
  edited |= DrawRuntimeIntRow(context, "region height", "global_region_roi_h",
                              90, 1, 10000, 130.0f);
  return edited;
}

static bool DrawRegionPatternParameterControls(ManualTestContext &context) {
  bool edited = false;
  ImGui::TextColored(ImVec4(0.9f, 0.72f, 0.32f, 1.0f),
                     "Region Pattern Content Descriptor");
  ImGui::TextDisabled(
      "Region-content direction: normalized gray/binary pooling. Classifier "
      "binding and semantic accuracy are not claimed here.");

  edited |=
      DrawRuntimeIntRow(context, "normalized width",
                        "global_region_normalized_width", 32, 8, 512, 160.0f);
  edited |=
      DrawRuntimeIntRow(context, "normalized height",
                        "global_region_normalized_height", 32, 8, 512, 160.0f);
  edited |= DrawRuntimeIntRow(context, "pooling rows",
                              "global_region_pooling_rows", 4, 1, 64, 160.0f);
  edited |= DrawRuntimeIntRow(context, "pooling cols",
                              "global_region_pooling_cols", 4, 1, 64, 160.0f);
  edited |= DrawRuntimeIntRow(context, "use binary", "global_region_use_binary",
                              0, 0, 1, 160.0f);
  edited |= DrawRuntimeIntRow(context, "threshold", "global_region_threshold",
                              128, 0, 255, 160.0f);
  edited |= DrawRuntimeIntRow(context, "foreground dark",
                              "global_region_foreground_dark", 1, 0, 1, 160.0f);
  edited |= DrawRuntimeIntRow(context, "max overlay blocks",
                              "global_region_max_overlays", 64, 1, 512, 160.0f);

  ImGui::TextDisabled("View chain: ROI -> pooled_region_block overlays -> "
                      "descriptor metrics -> manual texture review.");
  return edited;
}

static bool DrawFastMatchMatchParameterControls(ManualTestContext &context) {
  bool edited = false;
  if (ImGui::CollapsingHeader("FastMatch Match Evaluation",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
  ImGui::Indent(12.0f);
  ImGui::TextDisabled("Maps to: setmatchrect + matchstepgap + setmatchthre + "
                      "setminscore + setfindnum.");

  edited |= DrawRuntimeIntRow(context, "match_step_x", "global_match_step_x",
                              10, 1, 100, 130.0f);
  edited |= DrawRuntimeIntRow(context, "match_step_y", "global_match_step_y",
                              10, 1, 100, 130.0f);
  edited |= DrawRuntimeIntRow(context, "match_threshold", "global_match_thre",
                              10, 0, 255, 130.0f);
  edited |= DrawRuntimeIntRow(

      context, "min_score %", "global_min_score_percent", 65, 0, 100, 130.0f);

  const int minScorePercent =

      std::max(0, std::min(100, RuntimeIntOr(

                                    context, "global_min_score_percent", 65)));

  InjectManualGaugeInt(context, "global_min_score", minScorePercent);

  edited |= DrawRuntimeIntRow(

      context, "find_num", "global_find_num", 1, 1, 20, 130.0f);
  ImGui::Unindent(12.0f);
  }

  if (ImGui::CollapsingHeader("Affine Transform Search (OBB / external seed)",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
  ImGui::Indent(12.0f);
  edited |= DrawRuntimeIntRow(context, "enable affine search",
      "global_fastmatch_transform_enabled", 0, 0, 1, 210.0f);
  edited |= DrawRuntimeIntRow(context, "seed center x",
      "global_fastmatch_transform_center_x", 0, 0, 10000, 210.0f);
  edited |= DrawRuntimeIntRow(context, "seed center y",
      "global_fastmatch_transform_center_y", 0, 0, 10000, 210.0f);
  edited |= DrawRuntimeIntRow(context, "seed half-u",
      "global_fastmatch_transform_half_u", 0, 0, 10000, 210.0f);
  edited |= DrawRuntimeIntRow(context, "seed half-v",
      "global_fastmatch_transform_half_v", 0, 0, 10000, 210.0f);
  edited |= DrawRuntimeIntRow(context, "seed angle deg",
      "global_fastmatch_transform_angle_deg", 0, -180, 180, 210.0f);
  edited |= DrawRuntimeIntRow(context, "seed scale-x permille",
      "global_fastmatch_transform_scale_x_permille", 1000, 1, 3000, 210.0f);
  edited |= DrawRuntimeIntRow(context, "seed scale-y permille",
      "global_fastmatch_transform_scale_y_permille", 1000, 1, 3000, 210.0f);
  edited |= DrawRuntimeIntRow(context, "coarse scale range %",
      "global_fastmatch_transform_scale_range_percent", 15, 0, 50, 210.0f);
  edited |= DrawRuntimeIntRow(context, "coarse angle range deg",
      "global_fastmatch_transform_angle_range_deg", 15, 0, 90, 210.0f);
  edited |= DrawRuntimeIntRow(context, "coarse steps per axis",
      "global_fastmatch_transform_coarse_steps", 3, 1, 5, 210.0f);
  edited |= DrawRuntimeIntRow(context, "fine scale range %",
      "global_fastmatch_transform_fine_range_percent", 3, 0, 20, 210.0f);
  edited |= DrawRuntimeIntRow(context, "fine angle range deg",
      "global_fastmatch_transform_fine_angle_range_deg", 3, 0, 30, 210.0f);
  edited |= DrawRuntimeIntRow(context, "maximum candidates",
      "global_fastmatch_transform_max_candidates", 27, 1, 250, 210.0f);
  edited |= DrawRuntimeIntRow(context, "maximum samples",
      "global_fastmatch_transform_max_samples", 200000, 1, 1000000, 210.0f);
  edited |= DrawRuntimeIntRow(context, "maximum elapsed ms",
      "global_fastmatch_transform_max_elapsed_ms", 100, 1, 5000, 210.0f);
  ImGui::Unindent(12.0f);
  }
  if (ImGui::CollapsingHeader("P1 Shear / Projective Search (advanced)")) {
  ImGui::Indent(12.0f);
  edited |= DrawRuntimeIntRow(context, "shear range permille",
      "global_fastmatch_transform_shear_range_permille", 0, 0, 300, 210.0f);
  edited |= DrawRuntimeIntRow(context, "projective range permille",
      "global_fastmatch_transform_projective_range_permille", 0, 0, 50, 210.0f);
  ImGui::Unindent(12.0f);
  }
  if (ImGui::CollapsingHeader("Calibration to Physical Plane (test override)")) {
  ImGui::Indent(12.0f);
  edited |= DrawRuntimeIntRow(context, "enable test calibration",
      "global_fastmatch_calibration_test_enabled", 0, 0, 1, 250.0f);
  edited |= DrawRuntimeIntRow(context, "physical scale x ppm",
      "global_fastmatch_calibration_scale_x_ppm", 1000000, 1, 100000000, 250.0f);
  edited |= DrawRuntimeIntRow(context, "physical scale y ppm",
      "global_fastmatch_calibration_scale_y_ppm", 1000000, 1, 100000000, 250.0f);
  edited |= DrawRuntimeIntRow(context, "physical offset x milli-unit",
      "global_fastmatch_calibration_offset_x_milliunit", 0, -1000000, 1000000, 250.0f);
  edited |= DrawRuntimeIntRow(context, "physical offset y milli-unit",
      "global_fastmatch_calibration_offset_y_milliunit", 0, -1000000, 1000000, 250.0f);
  edited |= DrawRuntimeIntRow(context, "plane rotation milli-deg",
      "global_fastmatch_calibration_rotation_millideg", 0, -180000, 180000, 250.0f);
  edited |= DrawRuntimeIntRow(context, "plane shear x ppm",
      "global_fastmatch_calibration_shear_x_ppm", 0, -500000, 500000, 250.0f);
  edited |= DrawRuntimeIntRow(context, "plane shear y ppm",
      "global_fastmatch_calibration_shear_y_ppm", 0, -500000, 500000, 250.0f);
  edited |= DrawRuntimeIntRow(context, "reprojection RMSE milli-px",
      "global_fastmatch_calibration_reprojection_rmse_milli_px", 0, 0, 100000, 250.0f);
  ImGui::TextDisabled("ppm = physical units/pixel x 1,000,000; milli-unit and "
                      "milli-deg preserve decimal adjustment in CxScript. "
                      "This override is TEST ONLY: evidence marks its source "
                      "as manual_key_parameter_override and it cannot close "
                      "a production calibration receipt.");
  ImGui::TextDisabled("P0 scores learned A/B edge probes at the supplied seed; "
                      "center/extent can be copied from an OBB result. 0 center "
                      "or extent uses the current FastMatch ROI fallback.");
  ImGui::Unindent(12.0f);
  }
  if (ImGui::CollapsingHeader("Normal-Trace Learn (domain -> ANN cluster -> Dijkstra -> normal pair)")) {
  ImGui::Indent(12.0f);
  bool normalTraceEnabled =
      RuntimeIntOr(context, "global_fastmatch_normaltrace_enabled", 0) != 0;
  if (ImGui::Checkbox("Enable Normal-Trace Learn", &normalTraceEnabled)) {
    InjectManualGaugeInt(context, "global_fastmatch_normaltrace_enabled",
                         normalTraceEnabled ? 1 : 0);
    edited = true;
  }
  const bool scriptSupportsNormalTrace = FastMatchScriptSupportsNormalTrace(context);
  if (!normalTraceEnabled) {
    ImGui::TextColored(ImVec4(0.90f, 0.70f, 0.12f, 1.0f),
                       "NORMAL_TRACE_DISABLED: enable this option, then click Learn.");
  } else if (!scriptSupportsNormalTrace) {
    ImGui::TextColored(
        ImVec4(1.0f, 0.43f, 0.35f, 1.0f),
        "SCRIPT_BINDING_STALE: this saved script uses an older Normal-Trace binding.");
    ImGui::TextWrapped(
        "This is a CxScript compatibility block, not a FastMatch parameter. "
        "Learn and Learn + Match automatically create a compatible candidate "
        "run before execution. Loading this Evidence remains read-only; its "
        "script snapshot, test image, parameters, review state, and history "
        "remain unchanged.");
  } else {
    ImGui::TextColored(ImVec4(0.30f, 0.82f, 0.58f, 1.0f),
                       "READY: Learn will run selected FindLine conclusions -> "
                       "value-preserving XY compression -> Dijkstra -> derived "
                       "polyline -> domain-polarity normal pairs.");
    ImGui::TextDisabled(
        "Normal-Trace performs no image edge extraction. Dijkstra selects "
        "original conclusion references and generates zero new conclusions.");
  }
  edited |= DrawRuntimeIntRow(context, "conclusion de-duplication radius px",
      "global_fastmatch_normaltrace_overlap_radius_px", 3, 0, 64, 250.0f);
  edited |= DrawRuntimeIntRow(context, "Dijkstra maximum nodes",
      "global_fastmatch_normaltrace_max_nodes", 4096, 32, 200000, 250.0f);
  ImGui::TextDisabled(
      "Normal A/B offset uses Directional Learn -> compare gap. It is the "
      "distance from the keypoint to each polarity point; total A-B distance "
      "is 2 x compare gap. There is no second spacing value in Normal-Trace.");
  ImGui::TextDisabled(
      "Legacy minimum-gradient and gradient-cost values remain script-compatible "
      "but are ignored by the conclusion graph.");
  ImGui::SeparatorText("Dijkstra Path Costs");
  edited |= DrawRuntimeIntRow(context, "turn cost weight permille",
      "global_fastmatch_normaltrace_turn_weight_permille", 200, 0, 10000, 250.0f);
  edited |= DrawRuntimeIntRow(context, "gap cost weight permille",
      "global_fastmatch_normaltrace_gap_weight_permille", 100, 0, 10000, 250.0f);
  edited |= DrawRuntimeIntRow(context, "keypoint neighbour maximum gap px",
      "global_fastmatch_normaltrace_max_trace_gap_px", 3, 1, 64, 250.0f);
  edited |= DrawRuntimeIntRow(context, "ANN nearest candidates",
      "global_fastmatch_normaltrace_knn_neighbors", 6, 1, 32, 250.0f);
  ImGui::SeparatorText("Conclusion Track Connectivity");
  edited |= DrawRuntimeIntRow(context, "ANN base radius px (adaptive up to 2x)",
      "global_fastmatch_normaltrace_ann_radius_px", 32, 6, 256, 250.0f);
  edited |= DrawRuntimeIntRow(context, "path slope tolerance deg",
      "global_fastmatch_normaltrace_ann_tangent_deviation_deg", 35, 1, 89, 250.0f);
  edited |= DrawRuntimeIntRow(context, "domain-normal continuity tolerance deg",
      "global_fastmatch_normaltrace_ann_normal_deviation_deg", 35, 1, 89, 250.0f);
  ImGui::TextDisabled(
      "Edges connect only original conclusions with compatible path slope and "
      "the same signed FindLine domain-normal track.");
  edited |= DrawRuntimeIntRow(context, "minimum component points",
      "global_fastmatch_normaltrace_ann_min_component_points", 4, 2, 256, 250.0f);
  edited |= DrawRuntimeIntRow(context, "minimum component coverage %",
      "global_fastmatch_normaltrace_ann_min_component_coverage_percent", 55, 1, 100, 250.0f);
  ImGui::SeparatorText("Selected Conclusion XY Compression");
  edited |= DrawRuntimeIntRow(context, "track endpoint support radius px",
      "global_fastmatch_normaltrace_anchor_radius_px", 24, 4, 256, 250.0f);
  edited |= DrawRuntimeIntRow(context, "local XY compression bin px",
      "global_fastmatch_normaltrace_xy_compression_bin_px", 4, 1, 64, 250.0f);
  edited |= DrawRuntimeIntRow(context,
      "source-point gate: minimum valid keypoints per direction",
      "global_fastmatch_normaltrace_min_keypoints_per_domain", 4, 2, 256, 250.0f);
  ImGui::TextDisabled(
      "Each de-duplication/XY bucket keeps one medoid that is an existing "
      "FindLine conclusion; no averaged coordinate is created.");
  ImGui::TextDisabled(
      "If any direction is below this gate, ANN/Dijkstra and Normal A/B pairing "
      "stop. FastMatch may use its explicit compatibility fallback, but sparse "
      "evidence is never padded or forced into a trace.");
  ImGui::SeparatorText("Derived Junction Reconstruction");
  edited |= DrawRuntimeIntRow(context, "endpoint spike ratio % of median gap",
      "global_fastmatch_normaltrace_endpoint_spike_ratio_percent", 250, 110, 1000, 250.0f);
  edited |= DrawRuntimeIntRow(context, "junction tangent source points",
      "global_fastmatch_normaltrace_junction_tangent_window_points", 6, 2, 32, 250.0f);
  edited |= DrawRuntimeIntRow(context, "junction minimum cross angle deg",
      "global_fastmatch_normaltrace_junction_min_cross_angle_deg", 12, 1, 89, 250.0f);
  edited |= DrawRuntimeIntRow(context, "junction maximum extrapolation %",
      "global_fastmatch_normaltrace_junction_max_extrapolation_percent", 150, 25, 500, 250.0f);
  edited |= DrawRuntimeIntRow(context, "junction join spacing multiplier %",
      "global_fastmatch_normaltrace_junction_join_spacing_multiplier_percent", 800, 100, 2000, 250.0f);
  ImGui::TextDisabled(
      "Adjacent direction endpoints are not joined by a diagonal chord. A "
      "bounded operator-selected source-point tangent window reconstructs a sharp "
      "junction; near-parallel or excessive extrapolation uses a tangent blend.");
  ImGui::TextDisabled(
      "Junction reach follows track endpoint support radius and observed source "
      "spacing. Path slope tolerance controls neighbourhood direction stability. "
      "Orange junctions are derived geometry, never conclusion points.");
  ImGui::SeparatorText("Normal Pair Geometry");
  edited |= DrawRuntimeIntRow(context, "normal angle tolerance deg",
      "global_fastmatch_normaltrace_angle_tolerance_deg", 20, 1, 90, 250.0f);
  edited |= DrawRuntimeIntRow(context, "closed trace minimum length px",
      "global_fastmatch_normaltrace_min_length_px", 20, 2, 10000, 250.0f);
  ImGui::TextDisabled(
      "Polarity source: inherited from each directional FindLine Detection "
      "Edge. The legacy Normal-Trace polarity value is ignored.");
  edited |= DrawRuntimeIntRow(context, "corner rejection radius px",
      "global_fastmatch_normaltrace_corner_rejection_px", 4, 0, 64, 250.0f);
  edited |= DrawRuntimeIntRow(context, "normal pair source-point stride",
      "global_fastmatch_normaltrace_tangent_step_px", 2, 1, 64, 250.0f);
  ImGui::SeparatorText("Trace Evidence Overlay (render-only)");
  ImGui::Checkbox("Show selected/de-duplicated conclusions",
                  &context.show_fastmatch_normal_trace_domain);
  ImGui::SameLine();
  ImGui::Checkbox("Show selected conclusion track",
                  &context.show_fastmatch_normal_trace_ann_component);
  ImGui::Checkbox("Show Dijkstra sources + derived polyline",
                  &context.show_fastmatch_normal_trace_path);
  ImGui::SameLine();
  ImGui::Checkbox("Show normal A/B pairs",
                  &context.show_fastmatch_normal_trace_pairs);
  ImGui::TextDisabled(
      "Display selections persist across Learn. After each run, checked layers "
      "are rebound to that revision; an unavailable layer reports its failure "
      "reason instead of retaining an older result.");
  ImGui::TextDisabled("FindLine supplies directional anchors. Each anchor "
                      "neighbourhood is de-duplicated and compressed in local "
                      "u/v, ANN spatially re-clusters it, and Dijkstra runs only "
                      "on the selected component. Tangent/normal values are soft "
                      "costs by default, not brittle connectivity gates. Overlays "
                      "never modify the input image.");
  ImGui::Unindent(12.0f);
  }
  return edited;
}

static bool DrawGridPatternParameterControls(ManualTestContext &context) {
  bool edited = false;
  ImGui::TextColored(ImVec4(0.35f, 0.88f, 0.62f, 1.0f),
                     "Grid Pattern Class Network (experimental)");
  ImGui::TextDisabled("Region-content direction: normalized grid features and "
                      "3-5 pooled levels. FastMatch defaults are unchanged.");

  edited |=
      DrawRuntimeIntRow(context, "normalized width",
                        "global_grid_normalized_width", 48, 16, 512, 160.0f);
  edited |=
      DrawRuntimeIntRow(context, "normalized height",
                        "global_grid_normalized_height", 48, 16, 512, 160.0f);
  edited |= DrawRuntimeIntRow(context, "grid rows", "global_grid_rows", 12, 2,
                              64, 160.0f);
  edited |= DrawRuntimeIntRow(context, "grid cols", "global_grid_cols", 12, 2,
                              64, 160.0f);
  edited |= DrawRuntimeIntRow(context, "pooled levels", "global_grid_levels", 3,
                              3, 5, 160.0f);
  edited |= DrawRuntimeIntRow(context, "orientation bins",
                              "global_grid_orientation_bins", 8, 2, 36, 160.0f);
  edited |= DrawRuntimeIntRow(context, "foreground threshold",
                              "global_grid_foreground_threshold", -1, -1, 255,
                              160.0f);
  edited |= DrawRuntimeIntRow(context, "foreground dark",
                              "global_grid_foreground_dark", 1, 0, 1, 160.0f);
  edited |= DrawRuntimeIntRow(context, "equalize contrast",
                              "global_grid_equalize_contrast", 0, 0, 1, 160.0f);
  edited |= DrawRuntimeIntRow(context, "active foreground %",
                              "global_grid_active_foreground_percent", 5, 0,
                              100, 160.0f);
  edited |=
      DrawRuntimeIntRow(context, "active edge %",
                        "global_grid_active_edge_percent", 3, 0, 100, 160.0f);
  edited |= DrawRuntimeIntRow(context, "max overlay cells",
                              "global_grid_max_overlays", 96, 1, 512, 160.0f);
  edited |= DrawRuntimeIntRow(context, "fusion mode", "global_grid_fusion_mode",
                              2, 0, 3, 160.0f);

  ImGui::TextDisabled(
      "fusion mode: 0 structural-only, 1 grid-only, 2 cascade, 3 score fusion. "
      "This CASE records the value but does not replace FastMatch matching.");
  return edited;
}

static void RequestFastMatchRunAction(ManualTestContext &context,
                                      int actionCode, const char *actionLabel) {
  // A checked diagnostic is a persistent display preference. Ensure the
  // FastMatch debug projection remains enabled when Learn replaces the runtime
  // object, so the latest revision is rendered without another checkbox click.
  if (context.show_fastmatch_learn_result ||
      context.show_fastmatch_learn_scan_ticks ||
      context.show_fastmatch_normal_trace_domain ||
      context.show_fastmatch_normal_trace_ann_component ||
      context.show_fastmatch_normal_trace_path ||
      context.show_fastmatch_normal_trace_pairs) {
    context.show_fastmatch_debug_vectors = true;
  }
  InjectManualGaugeInt(context, "global_fastmatch_action", actionCode);
  context.current_gauge.dirty = true;
  context.current_gauge.review_status = "editing";
  context.apply_gauge_to_shape_requested = true;
  context.debug_action =
      actionLabel == nullptr ? "FastMatch Action" : actionLabel;
  if (ApplyManualGaugeToGlobals(context)) {
    context.pending_execution_gauge = context.current_gauge;
    context.pending_execution_globals = context.runtime_int_vars;
    context.has_pending_execution_snapshot = true;
    context.fastmatch_last_requested_revision =
        context.key_parameter_edit_revision;
    context.fastmatch_last_requested_action = actionCode;
    context.debug_status = "FASTMATCH_RUN_REQUESTED";
    context.debug_reason =
        std::string(actionLabel == nullptr ? "FastMatch action" : actionLabel) +
        " requested; cxscript may use global_fastmatch_action to branch "
        "learn/match.";
    context.run_state = "running";
    const int learnX = RuntimeIntOr(context, "global_learn_roi_x", 120);
    const int learnY = RuntimeIntOr(context, "global_learn_roi_y", 120);
    const int learnW = RuntimeIntOr(context, "global_learn_roi_w", 120);
    const int learnH = RuntimeIntOr(context, "global_learn_roi_h", 90);
    const int searchX = RuntimeIntOr(context, "global_search_roi_x", 0);
    const int searchY = RuntimeIntOr(context, "global_search_roi_y", 0);
    const int searchW = RuntimeIntOr(context, "global_search_roi_w", 640);
    const int searchH = RuntimeIntOr(context, "global_search_roi_h", 480);
    const int matchStepX = RuntimeIntOr(context, "global_match_step_x", 10);
    const int matchStepY = RuntimeIntOr(context, "global_match_step_y", 10);
    const int matchThre = RuntimeIntOr(context, "global_match_thre", 10);
    const int minScorePercent =
        RuntimeIntOr(context, "global_min_score_percent", 65);
    const int minScore =
        RuntimeIntOr(context, "global_min_score", minScorePercent);
    const int findNum = RuntimeIntOr(context, "global_find_num", 1);
    const int scanRotationDeg =
        RuntimeIntOr(context, "global_fastmatch_scan_rotation_deg", 0);
    const std::string runMessage =
        "action=" + std::to_string(actionCode) +
        " script=" + context.loaded_script_path + " learn_roi=(" +
        std::to_string(learnX) + "," + std::to_string(learnY) + "," +
        std::to_string(learnW) + "," + std::to_string(learnH) + ")" +
        " search_roi=(" + std::to_string(searchX) + "," +
        std::to_string(searchY) + "," + std::to_string(searchW) + "," +
        std::to_string(searchH) + ")" + " match_step=(" +
        std::to_string(matchStepX) + "," + std::to_string(matchStepY) + ")" +
        " match_thre=" + std::to_string(matchThre) +
        " min_score=" + std::to_string(minScore) +
        " min_score_percent=" + std::to_string(minScorePercent) +
        " find_num=" + std::to_string(findNum) +
        " scan_rotation_deg=" + std::to_string(scanRotationDeg) +
        " normal_trace_enabled=" +
        std::to_string(RuntimeIntOr(
            context, "global_fastmatch_normaltrace_enabled", 0));
    context.debug_reason = runMessage;
  }
}

static bool FastMatchScriptSupportsNormalTrace(const ManualTestContext &context) {
  return context.editor_text.find("fastmatch_normal_trace_binding_version: 3") !=
             std::string::npos &&
         context.editor_text.find("setnormaltraceenabled") != std::string::npos &&
         context.editor_text.find("setnormaltraceparams") != std::string::npos &&
         context.editor_text.find("setnormaltracedomain") != std::string::npos &&
         context.editor_text.find("setnormaltracejunction") != std::string::npos &&
         context.editor_text.find("setnormaltraceknn") != std::string::npos &&
         context.editor_text.find("setnormaltraceann") != std::string::npos;
}

static void ApplyPrimaryObjectToCurrentGauge(
    ManualTestContext &context, const CxEvidenceEditableObjectRef &ref,
    const char *status) {


  ManualGaugeState &gauge = context.current_gauge;
  const std::string tool = NormalizeKeyParamToolTypeLocal(ref.type);

  gauge.primary_object_type = tool;
  gauge.primary_object_name = ref.name;
  gauge.primary_object_status =
      status == nullptr ? "manual_object_selected" : status;
  gauge.tool = tool;
  gauge.has_line_gauge = tool == "FindLine";
  gauge.has_circle_gauge = tool == "FindCircle";
  gauge.has_ellipse_gauge = tool == "FindEllipse";
  gauge.has_segmentation_prompt_rect = tool == "FindSegmentation";
  gauge.has_findobject_roi = tool == "FindObject";
  if (tool == "FindSegmentation") {
    gauge.segmentation_prompt_x0 =
        RuntimeIntOr(context, "global_roi_x0", gauge.segmentation_prompt_x0);
    gauge.segmentation_prompt_y0 =
        RuntimeIntOr(context, "global_roi_y0", gauge.segmentation_prompt_y0);
    gauge.segmentation_prompt_x1 =
        RuntimeIntOr(context, "global_roi_x1", gauge.segmentation_prompt_x1);
    gauge.segmentation_prompt_y1 =
        RuntimeIntOr(context, "global_roi_y1", gauge.segmentation_prompt_y1);
    gauge.segmentation_mode = RuntimeIntOr(context, "global_segmentation_mode",
                                           gauge.segmentation_mode);
  }
  if (tool == "FindObject") {
    gauge.findobject_x0 =
        RuntimeIntOr(context, "global_roi_x0", gauge.findobject_x0);
    gauge.findobject_y0 =
        RuntimeIntOr(context, "global_roi_y0", gauge.findobject_y0);
    gauge.findobject_x1 =
        RuntimeIntOr(context, "global_roi_x1", gauge.findobject_x1);
    gauge.findobject_y1 =
        RuntimeIntOr(context, "global_roi_y1", gauge.findobject_y1);
    gauge.findobject_foreground_mode = RuntimeIntOr(
        context, "global_method", gauge.findobject_foreground_mode);
    gauge.findobject_threshold =
        RuntimeIntOr(context, "global_threshold", gauge.findobject_threshold);
    gauge.gap = RuntimeIntOr(context, "global_gap", gauge.gap);
    gauge.filterprofile =
        RuntimeIntOr(context, "global_filterprofile", gauge.filterprofile);
  }
  gauge.dirty = true;
  gauge.review_status = "editing";

  context.current_evidence_selection.primary_object_type = tool;
  context.current_evidence_selection.primary_object_name = ref.name;
  context.current_evidence_selection.primary_object_status =
      gauge.primary_object_status;
  context.debug_status = "PRIMARY_OBJECT_SELECTED";
  context.debug_reason =
      "Key Parameter Controls selected primary object: " + tool + " " +
      ref.name;
}

static void DrawPrimaryObjectSelector(ManualTestContext &context) {
  ManualGaugeState &gauge = context.current_gauge;
  const CxEvidenceSelectionSnapshot &snapshot =
      context.current_evidence_selection;

  ImGui::Text(
      "Primary Object: %s %s | %s",
      gauge.primary_object_type.empty() ? "-"
                                        : gauge.primary_object_type.c_str(),
      gauge.primary_object_name.empty() ? "-"
                                        : gauge.primary_object_name.c_str(),
      gauge.primary_object_status.empty()
          ? "-"
          : gauge.primary_object_status.c_str());

  if (!snapshot.valid || snapshot.editable_objects.empty()) {
    ImGui::TextDisabled(
        "No editable object candidates from selected Evidence row.");
    return;
  }

  int currentIndex = -1;
  for (int i = 0; i < static_cast<int>(snapshot.editable_objects.size()); ++i) {
    const auto &ref = snapshot.editable_objects[static_cast<std::size_t>(i)];
    if (ref.name == gauge.primary_object_name &&
        NormalizeKeyParamToolTypeLocal(ref.type) ==
            NormalizeKeyParamToolTypeLocal(gauge.primary_object_type)) {
      currentIndex = i;
      break;
    }
  }

  std::string preview =
      currentIndex >= 0
          ? NormalizeKeyParamToolTypeLocal(
                snapshot
                    .editable_objects[static_cast<std::size_t>(currentIndex)]
                    .type) +
                " " +
                snapshot
                    .editable_objects[static_cast<std::size_t>(currentIndex)]
                    .name
          : "Select primary editable object";

  ImGui::SetNextItemWidth(320.0f);
  if (ImGui::BeginCombo("Primary Object##evidence_primary_object",
                        preview.c_str())) {
    for (int i = 0; i < static_cast<int>(snapshot.editable_objects.size());
         ++i) {
      const auto &ref = snapshot.editable_objects[static_cast<std::size_t>(i)];
      const std::string label = NormalizeKeyParamToolTypeLocal(ref.type) + " " +
                                ref.name + "  line " +
                                std::to_string(ref.declared_line);
      const bool selected = i == currentIndex;
      if (ImGui::Selectable(label.c_str(), selected)) {
        ApplyPrimaryObjectToCurrentGauge(context, ref,
                                         "manual_object_selected");
      }
      if (selected)
        ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }

  if (gauge.primary_object_status == "needs_object_selection" ||
      gauge.primary_object_status == "needs_object_selection_no_tool_match") {
    ImGui::TextColored(ImVec4(1.0f, 0.68f, 0.25f, 1.0f),
                       "Multiple editable objects are present. Select the "
                       "object before tuning parameters.");
  }
}

static const RuntimeObjectView *
FindCurrentFindLineObject(const ManualTestContext &context) {
  const std::string &primary = context.current_gauge.primary_object_name;
  if (!primary.empty()) {
    for (const RuntimeObjectView &object : context.runtime_objects) {
      if (object.type == "FindLine" && object.name == primary)
        return &object;
    }
  }

  for (const RuntimeObjectView &object : context.runtime_objects) {
    if (object.type == "FindLine")
      return &object;
  }
  return nullptr;
}

static const RuntimeObjectView *
FindCurrentFindCircleObject(const ManualTestContext &context) {
  const std::string &primary = context.current_gauge.primary_object_name;
  if (!primary.empty()) {
    for (const RuntimeObjectView &object : context.runtime_objects) {
      if (object.type == "FindCircle" && object.name == primary)
        return &object;
    }
  }

  for (const RuntimeObjectView &object : context.runtime_objects) {
    if (object.type == "FindCircle")
      return &object;
  }
  return nullptr;
}

static const RuntimeObjectView *
FindCurrentFindEllipseObject(const ManualTestContext &context) {
  const std::string &primary = context.current_gauge.primary_object_name;
  if (!primary.empty()) {
    for (const RuntimeObjectView &object : context.runtime_objects) {
      if (object.type == "FindEllipse" && object.name == primary)
        return &object;
    }
  }

  for (const RuntimeObjectView &object : context.runtime_objects) {
    if (object.type == "FindEllipse")
      return &object;
  }
  return nullptr;
}

static const RuntimeObjectView *
FindCurrentFastMatchObject(const ManualTestContext &context) {
  const std::string &primary = context.current_gauge.primary_object_name;
  if (!primary.empty()) {
    for (const RuntimeObjectView &object : context.runtime_objects) {
      if (object.type == "FastMatch" && object.name == primary)
        return &object;
    }
  }

  for (const RuntimeObjectView &object : context.runtime_objects) {
    if (object.type == "FastMatch")
      return &object;
  }
  return nullptr;
}

static void DrawFastMatchTemplateStatusPanel(const ManualTestContext &context) {
  const RuntimeObjectView *object = FindCurrentFastMatchObject(context);

  ImGui::Separator();
  ImGui::TextUnformatted("FastMatch Template / Match Evidence");
  ImGui::TextDisabled("Learn must publish model points and template evidence; "
                      "Match must publish candidate/result evidence.");

  if (object == nullptr) {
    ImGui::TextDisabled("No FastMatch runtime object is available. Run a "
                        "FastMatch learn/match script first.");
    return;
  }

  ImGui::Text("model_points=%d learn_status=%d",
              object->fastmatch_model_point_count,
              object->fastmatch_learn_status_code);
  if (object->fastmatch_normal_trace_domain_count > 0 ||
      object->fastmatch_normal_trace_path_count > 0 ||
      !object->fastmatch_normal_trace_reason.empty()) {
    ImGui::TextUnformatted("Normal-Trace Evidence");
    ImGui::Text("FindLine conclusions=%d | retained=%d | derived trace=%d | normal pairs=%d",
                object->fastmatch_normal_trace_domain_count,
                object->fastmatch_normal_trace_deduplicated_count,
                object->fastmatch_normal_trace_path_count,
                object->fastmatch_normal_trace_pair_count);
    ImGui::Text("directional sides=%d/4 | trace segments=%d/4 | closure=%.2f px | max step=%.2f px | selected-anchor coverage=%.1f%%",
                object->fastmatch_normal_trace_side_count,
                object->fastmatch_normal_trace_segment_count,
                object->fastmatch_normal_trace_closure_error_px,
                object->fastmatch_normal_trace_max_consecutive_gap_px,
                object->fastmatch_normal_trace_selected_anchor_coverage * 100.0);
    ImGui::Text(
        "Dijkstra source conclusions=%d | derived polyline points=%d | generated conclusions=%d",
        object->fastmatch_normal_trace_dijkstra_source_count,
        object->fastmatch_normal_trace_derived_count,
        object->fastmatch_normal_trace_generated_conclusion_count);
    ImGui::Text("derived junctions: tangent intersection=%d | tangent blend=%d",
                object->fastmatch_normal_trace_junction_intersection_count,
                object->fastmatch_normal_trace_junction_blend_count);
    ImGui::Text("endpoint spikes pruned Top/Bottom/Left/Right=%d/%d/%d/%d",
                object->fastmatch_normal_trace_endpoint_spike_pruned_counts[0],
                object->fastmatch_normal_trace_endpoint_spike_pruned_counts[1],
                object->fastmatch_normal_trace_endpoint_spike_pruned_counts[2],
                object->fastmatch_normal_trace_endpoint_spike_pruned_counts[3]);
    ImGui::Text(
        "FindLine-bound pairs=%d | binding misses=%d | corner rejected=%d | loop erased=%d",
        object->fastmatch_normal_trace_findline_bound_pairs,
        object->fastmatch_normal_trace_binding_misses,
        object->fastmatch_normal_trace_corner_rejected,
        object->fastmatch_normal_trace_loop_erased_points);
    ImGui::Text("pair bindings Top/Bottom/Left/Right=%d/%d/%d/%d",
                object->fastmatch_normal_trace_pair_direction_counts[0],
                object->fastmatch_normal_trace_pair_direction_counts[1],
                object->fastmatch_normal_trace_pair_direction_counts[2],
                 object->fastmatch_normal_trace_pair_direction_counts[3]);
    static const char *prefilterDirectionNames[4] = {
        "Top", "Bottom", "Left", "Right"};
    for (int direction = 0; direction < 4; ++direction) {
      ImGui::Text(
          "%s selected/valid=%d/%d | rejected invalid slope/normal=%d/%d",
          prefilterDirectionNames[direction],
          object->fastmatch_normal_trace_anchor_near_counts[direction],
          object->fastmatch_normal_trace_anchor_accepted_counts[direction],
          object->fastmatch_normal_trace_anchor_slope_rejected_counts[direction],
          object->fastmatch_normal_trace_anchor_normal_rejected_counts[direction]);
    }
    ImGui::Text("Top domain/key=%d/%d | Bottom=%d/%d | Left=%d/%d | Right=%d/%d",
                object->fastmatch_normal_trace_domain_counts[0],
                object->fastmatch_normal_trace_keypoint_counts[0],
                object->fastmatch_normal_trace_domain_counts[1],
                object->fastmatch_normal_trace_keypoint_counts[1],
                object->fastmatch_normal_trace_domain_counts[2],
                object->fastmatch_normal_trace_keypoint_counts[2],
                object->fastmatch_normal_trace_domain_counts[3],
                object->fastmatch_normal_trace_keypoint_counts[3]);
    static const char *directionNames[4] = {"Top", "Bottom", "Left", "Right"};
    for (int direction = 0; direction < 4; ++direction) {
      ImGui::Text("%s ANN edges=%d components=%d selected=%d coverage=%.1f%%",
                  directionNames[direction],
                  object->fastmatch_normal_trace_ann_edge_counts[direction],
                  object->fastmatch_normal_trace_ann_component_counts[direction],
                  object->fastmatch_normal_trace_ann_selected_counts[direction],
                  object->fastmatch_normal_trace_ann_coverage[direction] * 100.0);
    }
    const bool normalTraceComplete =
        object->fastmatch_normal_trace_reason == "NORMAL_TRACE_COMPLETE" &&
        object->fastmatch_normal_trace_side_count == 4 &&
        object->fastmatch_normal_trace_segment_count == 4 &&
        object->fastmatch_normal_trace_closure_error_px >= 0.0 &&
        object->fastmatch_normal_trace_closure_error_px <= 1.5 &&
        object->fastmatch_normal_trace_max_consecutive_gap_px >= 0.0 &&
        object->fastmatch_normal_trace_max_consecutive_gap_px <= 1.5;
    ImGui::TextColored(
        normalTraceComplete
            ? ImVec4(0.36f, 0.88f, 0.56f, 1.0f)
            : ImVec4(1.0f, 0.58f, 0.30f, 1.0f),
        "trace conclusion: %s", object->fastmatch_normal_trace_reason.c_str());
  }
  if (object->fastmatch_learn_status_code == 34)
    ImGui::TextColored(ImVec4(0.45f, 0.90f, 0.55f, 1.0f),
                       "Learn source: selected directional Point Column normal pairs");
  ImGui::Text("learn sets: A=%d B=%d A2=%d B2=%d",
              object->fastmatch_learn_a_count, object->fastmatch_learn_b_count,
              object->fastmatch_learn_a2_count,
              object->fastmatch_learn_b2_count);
  ImGui::TextUnformatted("Directional FindLine Probe Evidence");
  ImGui::TextDisabled("One isolated Measure + SmartFilter run per tab. If a "
                      "tab chooses Edge N or Last, its accepted points are "
                      "the normal-pair source for Learn status 34.");
  const char* probeNames[] = {"Top", "Bottom", "Left", "Right"};
  if (ImGui::BeginTable("fastmatch_directional_probe_evidence", 7,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_SizingStretchProp)) {
    ImGui::TableSetupColumn("Probe");
    ImGui::TableSetupColumn("Scans");
    ImGui::TableSetupColumn("Raw");
    ImGui::TableSetupColumn("Accepted side");
    ImGui::TableSetupColumn("Point Column");
    ImGui::TableSetupColumn("Compare gap");
    ImGui::TableSetupColumn("Status");
    ImGui::TableHeadersRow();
    for (int direction = 0; direction < 4; ++direction) {
      const std::size_t index = static_cast<std::size_t>(direction);
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(probeNames[direction]);
      ImGui::TableSetColumnIndex(1);
      ImGui::Text("%d", object->fastmatch_directional_probe_scans[index]);
      ImGui::TableSetColumnIndex(2);
      ImGui::Text("%d", object->fastmatch_directional_probe_raw[index]);
      ImGui::TableSetColumnIndex(3);
      ImGui::Text("%d", object->fastmatch_directional_probe_accepted[index]);
      ImGui::TableSetColumnIndex(4);
      const int selected =
          object->fastmatch_directional_probe_selected_edge[index];
      const int edgeCount =
          object->fastmatch_directional_probe_edge_count[index];
      if (selected == 0)
        ImGui::TextUnformatted("Full edge");
      else if (selected < 0)
        ImGui::TextUnformatted("Last");
      else
        ImGui::Text("%d / %d", selected, edgeCount);
      const int runtimeSelected =
          object->fastmatch_directional_probe_runtime_selected_edge[index];
      const std::string runtimeLabel =
          runtimeSelected == -1 ? "reverse last" :
          runtimeSelected == 0 ? "all" :
          "native " + std::to_string(runtimeSelected);
      ImGui::SameLine();
      ImGui::TextDisabled("(%s)", runtimeLabel.c_str());
      ImGui::TableSetColumnIndex(5);
      ImGui::Text("%d px",
                  object->fastmatch_directional_probe_compare_gap[index]);
      ImGui::TableSetColumnIndex(6);
      const std::string& status =
          object->fastmatch_directional_probe_status[index];
      ImGui::TextUnformatted(status.empty() ? "NOT_RUN" : status.c_str());
    }
    ImGui::EndTable();
  }
  ImGui::Text("template patterns: A=%d B=%d", object->fastmatch_pattern_a_count,
              object->fastmatch_pattern_b_count);
  ImGui::Text("match candidates=%d best_score=%.3f",
              object->fastmatch_candidate_count, object->fastmatch_best_score);
  ImGui::Text("learn ROI=(%d,%d)-(%d,%d)", object->fastmatch_learn_rect_x0,
              object->fastmatch_learn_rect_y0, object->fastmatch_learn_rect_x1,
              object->fastmatch_learn_rect_y1);
  ImGui::Text("match ROI=(%d,%d)-(%d,%d)", object->fastmatch_match_rect_x0,
              object->fastmatch_match_rect_y0, object->fastmatch_match_rect_x1,
              object->fastmatch_match_rect_y1);

  const int pairCount = std::min(object->fastmatch_pattern_a_count,
                                 object->fastmatch_pattern_b_count);
  const int unpairedA = std::max(0, object->fastmatch_pattern_a_count - pairCount);
  const int unpairedB = std::max(0, object->fastmatch_pattern_b_count - pairCount);
  const bool learnHasPairs = pairCount > 0;
  ImGui::Text(
      "learn conclusion: applied compare gap Top/Bottom/Left/Right=%d/%d/%d/%d px | pairs=%d unpaired(A/B)=%d/%d",
      object->fastmatch_directional_probe_compare_gap[0],
      object->fastmatch_directional_probe_compare_gap[1],
      object->fastmatch_directional_probe_compare_gap[2],
      object->fastmatch_directional_probe_compare_gap[3], pairCount, unpairedA,
      unpairedB);
  ImGui::TextDisabled("Applied values above come from the completed Learn, not the current editor. "
                      "compare_gap is the keypoint-to-A/B offset; total A-B "
                      "distance is twice it. Tangents are adjacent keypoint-midpoint "
                      "vectors. Red points mean filtered/unpaired display "
                      "state, not a promoted PASS/FAIL conclusion.");
  if (!learnHasPairs)
    ImGui::TextColored(ImVec4(1.0f, 0.62f, 0.25f, 1.0f),
                       "FASTMATCH_PENDING_RESULT: learn has no drawable A/B "
                       "point-pair package yet.");


  const ImGuiTableFlags flags =
      ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
      ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp;
  if (ImGui::BeginTable("fastmatch_template_status_table", 4, flags)) {
    ImGui::TableSetupColumn("Asset");
    ImGui::TableSetupColumn("Count");
    ImGui::TableSetupColumn("State");
    ImGui::TableSetupColumn("Meaning");
    ImGui::TableHeadersRow();

    auto row = [](const char *asset, int count, const char *state,
                  const char *meaning) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(asset);
      ImGui::TableSetColumnIndex(1);
      ImGui::Text("%d", count);
      ImGui::TableSetColumnIndex(2);
      ImGui::TextUnformatted(state);
      ImGui::TableSetColumnIndex(3);
      ImGui::TextUnformatted(meaning);
    };

    row("Learn points", object->fastmatch_model_point_count,
        object->fastmatch_model_point_count > 0 ? "available" : "pending",
        "Image View should show learn ROI and model point evidence.");
    row("Template A", object->fastmatch_pattern_a_count,
        object->fastmatch_pattern_a_count > 0 ? "available" : "pending",
        "Template list source A.");
    row("Template B", object->fastmatch_pattern_b_count,
        object->fastmatch_pattern_b_count > 0 ? "available" : "pending",
        "Template list source B.");
    row("Match candidates", object->fastmatch_candidate_count,
        object->fastmatch_candidate_count > 0 ? "available" : "pending",
        "Image View should show search ROI and match result boxes.");

    ImGui::EndTable();
  }
}

static ManualFindLineEdgeParamState
MakeFindLineEdgeParamsFromGauge(const ManualGaugeState &gauge) {
  ManualFindLineEdgeParamState params;
  params.initialized = true;
  params.threshold = gauge.threshold;
  params.method = gauge.method;
  params.linegap = gauge.linegap;
  params.wgap = gauge.wgap;
  params.hgap = gauge.hgap;
  params.filterprofile = gauge.filterprofile;
  return params;
}

static void
ApplyFindLineEdgeParamsToGauge(const ManualFindLineEdgeParamState &params,
                               ManualGaugeState &gauge) {
  gauge.threshold = params.threshold;
  gauge.method = params.method;
  gauge.linegap = params.linegap;
  gauge.wgap = params.wgap;
  gauge.hgap = params.hgap;
  gauge.filterprofile = params.filterprofile;
}

static void EnsureFindLineEdgeParamStorage(ManualTestContext &context) {
  context.findline_scan_edge_count =
      std::max(1, std::min(16, context.findline_scan_edge_count));
  context.findline_selected_scan_edge =
      std::max(-1, std::min(context.findline_selected_scan_edge,
                            context.findline_scan_edge_count));
  context.findline_best_fit_edge =
      std::max(0, std::min(context.findline_best_fit_edge,
                           context.findline_scan_edge_count));
  context.findline_recommended_fit_edge =
      std::max(0, std::min(context.findline_recommended_fit_edge,
                           context.findline_scan_edge_count));
  context.findline_relation_edge =
      std::max(0, std::min(context.findline_relation_edge,
                           context.findline_scan_edge_count));
  context.findline_attach_edge =
      std::max(0, std::min(context.findline_attach_edge,
                           context.findline_scan_edge_count));

  const std::size_t required =
      static_cast<std::size_t>(context.findline_scan_edge_count + 1);
  if (context.findline_edge_params.size() < required)
    context.findline_edge_params.resize(required);

  for (int i = 1; i <= context.findline_scan_edge_count; ++i) {
    ManualFindLineEdgeParamState &params =
        context.findline_edge_params[static_cast<std::size_t>(i)];
    if (!params.initialized)
      params = MakeFindLineEdgeParamsFromGauge(context.current_gauge);
  }
}

static std::string FindLineEdgeLabel(int edge) {
  return edge == 0 ? "All edges" : ("Edge " + std::to_string(edge));
}

static int FindLineRuntimeSelectedEdgeForScript(int edge, int edgeCount) {
  const int count = std::max(1, std::min(16, edgeCount));
  const int clamped = std::max(-1, std::min(edge, count));
  if (count == 2 && clamped == 2)
    return -1;
  return clamped;
}

static std::string FindLineSelectedEdgeLabel(int edge, int edgeCount) {
  if (edge == -1)
    return "Last edge";
  if (edge == 0)
    return "All edges";
  if (FindLineRuntimeSelectedEdgeForScript(edge, edgeCount) == -1)
    return "Edge " + std::to_string(edge) + " (last edge)";
  return FindLineEdgeLabel(edge);
}

static std::string FindLineSelectedEdgeSummary(int edge, int edgeCount) {
  const int runtimeEdge = FindLineRuntimeSelectedEdgeForScript(edge, edgeCount);
  std::string summary = std::to_string(edge) + "/" + std::to_string(edgeCount);
  if (runtimeEdge != edge) {
    summary += "(runtime=" + std::to_string(runtimeEdge);
    if (runtimeEdge == -1)
      summary += ":last";
    summary += ")";
  }
  return summary;
}

static ManualFindCircleEdgeParamState
MakeFindCircleEdgeParamsFromGauge(const ManualGaugeState &gauge) {
  ManualFindCircleEdgeParamState params;
  params.initialized = true;
  params.threshold = gauge.threshold;
  params.method = gauge.method;
  params.linegap = gauge.linegap;
  params.gap = gauge.gap;
  return params;
}

static void EnsureFindCircleEdgeParamStorage(ManualTestContext &context) {
  context.findcircle_scan_edge_count =
      std::max(1, std::min(32, context.findcircle_scan_edge_count));
  context.findcircle_selected_scan_edge =
      std::max(-1, std::min(context.findcircle_selected_scan_edge,
                            context.findcircle_scan_edge_count));
  context.findcircle_best_fit_edge =
      std::max(0, std::min(context.findcircle_best_fit_edge,
                           context.findcircle_scan_edge_count));
  context.findcircle_recommended_fit_edge =
      std::max(0, std::min(context.findcircle_recommended_fit_edge,
                           context.findcircle_scan_edge_count));
  context.findcircle_relation_edge =
      std::max(0, std::min(context.findcircle_relation_edge,
                           context.findcircle_scan_edge_count));
  context.findcircle_attach_edge =
      std::max(0, std::min(context.findcircle_attach_edge,
                           context.findcircle_scan_edge_count));

  const std::size_t required =
      static_cast<std::size_t>(context.findcircle_scan_edge_count + 1);
  if (context.findcircle_edge_params.size() < required)
    context.findcircle_edge_params.resize(required);

  for (int i = 1; i <= context.findcircle_scan_edge_count; ++i) {
    ManualFindCircleEdgeParamState &params =
        context.findcircle_edge_params[static_cast<std::size_t>(i)];
    params = MakeFindCircleEdgeParamsFromGauge(context.current_gauge);
  }
}

static std::string FindCircleEdgeLabel(int edge) {
  return edge == 0 ? "All edges" : ("Edge " + std::to_string(edge));
}

static std::string FindCircleSelectedEdgeLabel(int edge, int edgeCount) {
  (void)edgeCount;
  if (edge == -1)
    return "Last edge";
  if (edge == 0)
    return "All edges";
  return FindCircleEdgeLabel(edge);
}

static void SyncFindCircleEdgeSelectionToGlobals(ManualTestContext &context,
                                                 const char *reason) {
  EnsureFindCircleEdgeParamStorage(context);
  InjectManualGaugeInt(context, "global_findcircle_edge_count",
                       context.findcircle_scan_edge_count);
  InjectManualGaugeInt(context, "global_findcircle_selected_edge",
                       context.findcircle_selected_scan_edge);
  InjectManualGaugeInt(context, "global_findcircle_best_edge",
                       context.findcircle_best_fit_edge);
  InjectManualGaugeInt(context, "global_findcircle_recommended_edge",
                       context.findcircle_recommended_fit_edge);
  InjectManualGaugeInt(context, "global_findcircle_relation_edge",
                       context.findcircle_relation_edge);
  InjectManualGaugeInt(context, "global_findcircle_attach_edge",
                       context.findcircle_attach_edge);

  context.current_gauge.dirty = true;
  context.current_gauge.review_status = "editing";
  context.debug_status = "FINDCIRCLE_EDGE_SELECTION_CHANGED";
  context.debug_reason =
      std::string(reason == nullptr ? "FindCircle edge selection changed"
                                    : reason) +
      "; globals updated, run script to refresh result points";
}

static bool DrawFindCircleEdgeRoleCombo(const char *label, int &edge,
                                        int edgeCount) {
  bool edited = false;
  edge = std::max(0, std::min(edge, edgeCount));
  const std::string currentLabel = FindCircleEdgeLabel(edge);
  ImGui::SetNextItemWidth(150.0f);
  if (ImGui::BeginCombo(label, currentLabel.c_str())) {
    if (ImGui::Selectable("All edges", edge == 0)) {
      edge = 0;
      edited = true;
    }
    for (int i = 1; i <= edgeCount; ++i) {
      const std::string item = FindCircleEdgeLabel(i);
      if (ImGui::Selectable(item.c_str(), edge == i)) {
        edge = i;
        edited = true;
      }
    }
    ImGui::EndCombo();
  }
  return edited;
}

static bool DrawFindLineEdgeRoleCombo(const char *label, int &edge,
                                      int edgeCount) {
  bool edited = false;
  edge = std::max(0, std::min(edge, edgeCount));
  const std::string currentLabel = FindLineEdgeLabel(edge);
  ImGui::SetNextItemWidth(150.0f);
  if (ImGui::BeginCombo(label, currentLabel.c_str())) {
    if (ImGui::Selectable("All edges", edge == 0)) {
      edge = 0;
      edited = true;
    }
    for (int i = 1; i <= edgeCount; ++i) {
      const std::string item = FindLineEdgeLabel(i);
      if (ImGui::Selectable(item.c_str(), edge == i)) {
        edge = i;
        edited = true;
      }
    }
    ImGui::EndCombo();
  }
  return edited;
}

static bool DrawFindCircleEdgeSelectorPanel(ManualTestContext &context) {
  if (!(context.current_gauge.tool == "FindCircle" ||
        context.current_gauge.has_circle_gauge)) {
    return false;
  }

  bool edited = false;
  EnsureFindCircleEdgeParamStorage(context);

  ImGui::Separator();
  ImGui::Text(
      "Algorithm scan sector: %s",
      context.current_gauge.circle_arc_enabled
          ? ("A0=" +
             std::to_string(context.current_gauge.circle_arc_start_deg) +
             " deg, A1=" +
             std::to_string(context.current_gauge.circle_arc_end_deg) + " deg")
                .c_str()
          : "full 360 deg");
  ImGui::TextDisabled("A0/A1 controls the angular scan domain. Edit it in "
                      "Geometry; Edge selection below does not change it.");
  ImGui::TextUnformatted("Detection Edge / Point Column");
  ImGui::TextDisabled("Edge N is the Nth eligible boundary crossing on every "
                      "radial Gauge line. It is not an angular sector.");
  ImGui::TextDisabled("Threshold/method/linegap/gap are one shared FindCircle "
                      "parameter set; changing Edge never changes them.");

  ImGui::SetNextItemWidth(100.0f);
  int edgeCount = context.findcircle_scan_edge_count;
  if (ImGui::InputInt("edge count", &edgeCount)) {
    context.findcircle_scan_edge_count = std::max(1, std::min(32, edgeCount));
    if (context.findcircle_selected_scan_edge >
        context.findcircle_scan_edge_count) {
      context.findcircle_selected_scan_edge =
          context.findcircle_scan_edge_count;
    }
    EnsureFindCircleEdgeParamStorage(context);
    SyncFindCircleEdgeSelectionToGlobals(context,
                                         "FindCircle edge count changed");
    edited = true;
  }

  ImGui::SameLine();
  ImGui::SetNextItemWidth(190.0f);
  const std::string currentLabel =
      FindCircleSelectedEdgeLabel(context.findcircle_selected_scan_edge,
                                  context.findcircle_scan_edge_count);
  if (ImGui::BeginCombo("selected edge", currentLabel.c_str())) {
    if (ImGui::Selectable("All edges",
                          context.findcircle_selected_scan_edge == 0)) {
      context.findcircle_selected_scan_edge = 0;
      SyncFindCircleEdgeSelectionToGlobals(context,
                                           "FindCircle selected All edges");
      edited = true;
    }
    for (int i = 1; i <= context.findcircle_scan_edge_count; ++i) {
      const std::string label = "Edge " + std::to_string(i);
      if (ImGui::Selectable(label.c_str(),
                            context.findcircle_selected_scan_edge == i)) {
        context.findcircle_selected_scan_edge = i;
        const std::string reason = "FindCircle selected " + label;
        SyncFindCircleEdgeSelectionToGlobals(context, reason.c_str());
        edited = true;
      }
    }
    ImGui::Separator();
    if (ImGui::Selectable("Last edge",
                          context.findcircle_selected_scan_edge == -1)) {
      context.findcircle_selected_scan_edge = -1;
      SyncFindCircleEdgeSelectionToGlobals(context,
                                           "FindCircle selected Last edge");
      edited = true;
    }
    ImGui::EndCombo();
  }

  if (context.findcircle_selected_scan_edge == -1) {
    ImGui::Text("Current: Last edge on every radial Gauge line");
    ImGui::TextDisabled("Last edge is resolved independently on each scan line "
                        "after all eligible crossings are collected.");
  } else if (context.findcircle_selected_scan_edge > 0) {
    ImGui::Text("Current: Edge %d / %d", context.findcircle_selected_scan_edge,
                context.findcircle_scan_edge_count);
    ImGui::TextDisabled("The shared Edge Params panel below remains unchanged; "
                        "only the candidate ordinal changes.");
  } else {
    ImGui::TextDisabled("All edges selected: Geometry/Edge Params below edit "
                        "the shared baseline.");
  }

  return edited;
}

static bool DrawFindCircleEdgeRolePanel(ManualTestContext &context) {
  if (!(context.current_gauge.tool == "FindCircle" ||
        context.current_gauge.has_circle_gauge)) {
    return false;
  }

  bool edited = false;
  EnsureFindCircleEdgeParamStorage(context);

  ImGui::Separator();
  ImGui::TextUnformatted("Edge Role Binding / 接入点");
  ImGui::TextDisabled(
      "Selection is an edge ordinal. Best/recommended/relation/attach are "
      "evidence metadata; none changes the scan sector.");

  ImGui::PushID("findcircle_edge_roles");
  edited |=
      DrawFindCircleEdgeRoleCombo("best edge", context.findcircle_best_fit_edge,
                                  context.findcircle_scan_edge_count);
  ImGui::SameLine();
  if (ImGui::Button("Use All Edges")) {
    context.findcircle_best_fit_edge = 0;
    edited = true;
  }

  edited |= DrawFindCircleEdgeRoleCombo("recommended edge",
                                        context.findcircle_recommended_fit_edge,
                                        context.findcircle_scan_edge_count);
  ImGui::SameLine();
  if (ImGui::Button("Recommend = Best")) {
    context.findcircle_recommended_fit_edge = context.findcircle_best_fit_edge;
    edited = true;
  }

  edited |= DrawFindCircleEdgeRoleCombo("relation edge",
                                        context.findcircle_relation_edge,
                                        context.findcircle_scan_edge_count);
  ImGui::SameLine();
  ImGui::TextDisabled("future: related/combined edge point-set");

  edited |=
      DrawFindCircleEdgeRoleCombo("attach edge", context.findcircle_attach_edge,
                                  context.findcircle_scan_edge_count);
  ImGui::SameLine();
  ImGui::TextDisabled("future: shape attach/binding");
  ImGui::PopID();

  ImGui::Text("edges: selected=%d best=%d recommended=%d relation=%d attach=%d",
              context.findcircle_selected_scan_edge,
              context.findcircle_best_fit_edge,
              context.findcircle_recommended_fit_edge,
              context.findcircle_relation_edge, context.findcircle_attach_edge);

  if (edited) {
    SyncFindCircleEdgeSelectionToGlobals(
        context, "FindCircle edge role binding changed");
  }

  return edited;
}

static void DrawFindCircleEdgeEvaluationPanel(ManualTestContext &context) {
  if (!(context.current_gauge.tool == "FindCircle" ||
        context.current_gauge.has_circle_gauge)) {
    return;
  }

  const RuntimeObjectView *object = FindCurrentFindCircleObject(context);

  ImGui::Separator();
  ImGui::TextUnformatted("Edge Result Evaluation");
  ImGui::TextDisabled(
      "The current runtime reports one selected-edge run. Per-edge scores "
      "require explicit runtime capture and are not inferred here.");

  if (object == nullptr) {
    ImGui::TextDisabled("Run script to collect circle result evidence.");
    return;
  }

  const int accepted = object->valid_points_count > 0
                           ? object->valid_points_count
                           : object->measure_points_count;
  const double score =
      object->has_fit_result
          ? std::max(0.0,
                     100.0 - static_cast<double>(object->fit_avgdist) * 10.0)
          : 0.0;

  ImGui::Text("ui_selected_edge=%d runtime_selected_edge=%d edge_count=%d "
              "fit_circle=%s score=%.2f",
              context.findcircle_selected_scan_edge,
              object->circle_selected_edge_index,
              context.findcircle_scan_edge_count,
              object->has_fit_result ? "true" : "false", score);
  if (object->circle_selected_edge_index !=
      context.findcircle_selected_scan_edge) {
    ImGui::TextColored(
        ImVec4(1.0f, 0.75f, 0.20f, 1.0f),
        "Runtime selected edge mismatch. Click Apply To Globals / Run Script "
        "again; otherwise global injection is not synchronized.");
  }
  ImGui::Text("candidate_runs_total=%d max_per_line=%d selected_hits=%d "
              "selected_misses=%d",
              object->circle_candidate_runs_total,
              object->circle_candidate_runs_max_per_line,
              object->circle_selected_edge_hits,
              object->circle_selected_edge_misses);
  ImGui::Text("selected radius avg/min/max = %.3f / %.3f / %.3f",
              object->circle_selected_edge_radius_avg,
              object->circle_selected_edge_radius_min,
              object->circle_selected_edge_radius_max);
  ImGui::Text("source boundary: clipped_lines=%d extended_samples=%d "
              "candidate_reject=%d",
              object->circle_scan_boundary_clipped_lines,
              object->circle_scan_boundary_extended_samples,
              object->circle_candidate_boundary_reject_count);

  const ImGuiTableFlags flags =
      ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
      ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp;

  if (ImGui::BeginTable("findcircle_edge_evaluation_table", 12, flags)) {
    ImGui::TableSetupColumn("Run");
    ImGui::TableSetupColumn("Sel");
    ImGui::TableSetupColumn("RtSel");
    ImGui::TableSetupColumn("Lines");
    ImGui::TableSetupColumn("Len");
    ImGui::TableSetupColumn("Points");
    ImGui::TableSetupColumn("MaxCand");
    ImGui::TableSetupColumn("Hits");
    ImGui::TableSetupColumn("Miss");
    ImGui::TableSetupColumn("Fit?");
    ImGui::TableSetupColumn("AvgDist");
    ImGui::TableSetupColumn("Stage");
    ImGui::TableHeadersRow();

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted(
        FindCircleEdgeLabel(context.findcircle_selected_scan_edge).c_str());
    ImGui::TableSetColumnIndex(1);
    ImGui::TextUnformatted("*");
    ImGui::TableSetColumnIndex(2);
    ImGui::Text("%d", object->circle_selected_edge_index);
    ImGui::TableSetColumnIndex(3);
    ImGui::Text("%d", object->circle_scan_line_count);
    ImGui::TableSetColumnIndex(4);
    ImGui::Text("%d", object->circle_scan_line_length);
    ImGui::TableSetColumnIndex(5);
    ImGui::Text("%d", accepted);
    ImGui::TableSetColumnIndex(6);
    ImGui::Text("%d", object->circle_candidate_runs_max_per_line);
    ImGui::TableSetColumnIndex(7);
    ImGui::Text("%d", object->circle_selected_edge_hits);
    ImGui::TableSetColumnIndex(8);
    ImGui::Text("%d", object->circle_selected_edge_misses);
    ImGui::TableSetColumnIndex(9);
    ImGui::TextUnformatted(object->has_fit_result ? "yes" : "no");
    ImGui::TableSetColumnIndex(10);
    ImGui::Text("%.3f", object->fit_avgdist);
    ImGui::TableSetColumnIndex(11);
    ImGui::TextUnformatted(object->circle_measure_failure_stage.empty()
                               ? "-"
                               : object->circle_measure_failure_stage.c_str());

    ImGui::EndTable();
  }
}

static void DrawFindCircleScanSemanticsPanel(ManualTestContext &context) {
  if (!(context.current_gauge.tool == "FindCircle" ||
        context.current_gauge.has_circle_gauge)) {
    return;
  }

  const RuntimeObjectView *object = FindCurrentFindCircleObject(context);
  const int linegap = std::max(1, context.current_gauge.linegap);
  const double circumference =
      std::max(0, context.current_gauge.radius) * 2.0 * 3.14159265358979323846;
  const int previewTicks =
      circumference > 1.0
          ? std::max(1, static_cast<int>(std::floor(circumference / linegap)))
          : 0;

  ImGui::Separator();
  ImGui::TextUnformatted("Gauge Scan Semantics");
  ImGui::TextColored(ImVec4(0.55f, 0.90f, 1.0f, 1.0f),
                     "cyan radial line = sampling opportunity");
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.18f, 1.0f),
                     "| yellow point = accepted algorithm result");
  ImGui::TextDisabled("Selected Edge is a candidate rank on each radial line; "
                      "A0/A1 is the independent angular scan sector.");

  if (object == nullptr) {
    ImGui::TextDisabled("Run script to show accepted/rejected counters.");
    ImGui::Text("preview_ticks=%d linegap=%d method=%d gap=%d", previewTicks,
                linegap, context.current_gauge.method,
                context.current_gauge.gap);
    return;
  }

  ImGui::Text("preview_ticks=%d scan_lines=%d scan_len=%d process_w=%d",
              previewTicks, object->circle_scan_line_count,
              object->circle_scan_line_length, object->circle_process_width);
  ImGui::Text("measure_points=%d valid_points=%d fit_circle=%s avgdist=%.3f",
              object->measure_points_count, object->valid_points_count,
              object->has_fit_result ? "true" : "false", object->fit_avgdist);
  ImGui::Text("precision refined=%d/%d coverage=%.3f sigma=%.3fpx",
              object->circle_boundary_refined_point_count,
              object->circle_boundary_accepted_point_count,
              object->circle_boundary_coverage_ratio,
              object->circle_boundary_localization_sigma_mean_px);
  ImGui::Text("residual rmse/p95/max=%.3f/%.3f/%.3fpx outliers=%.3f",
              object->circle_boundary_residual_rmse_px,
              object->circle_boundary_residual_p95_px,
              object->circle_boundary_residual_max_px,
              object->circle_boundary_outlier_ratio);
  ImGui::Text("reliability=%s score=%.3f offset mean/std=%.3f/%.3fpx",
              object->circle_boundary_reliability_level.empty()
                  ? "-"
                  : object->circle_boundary_reliability_level.c_str(),
              object->circle_boundary_reliability_score,
              object->circle_boundary_subpixel_offset_mean,
              object->circle_boundary_subpixel_offset_stddev);
  ImGui::Text("image_ready=%s backimage_ready=%s findobject_ready=%s",
              object->circle_measure_image_ready ? "true" : "false",
              object->circle_measure_backimage_ready ? "true" : "false",
              object->circle_measure_findobject_ready ? "true" : "false");
  ImGui::Text("object_prefilter requested=%d applied=%d restored=%d "
              "runs=%d->%d effective_min=%d",
              object->circle_object_prefilter_requested,
              object->circle_object_prefilter_applied,
              object->circle_object_prefilter_restored,
              object->circle_object_prefilter_runs_before,
              object->circle_object_prefilter_runs_after,
              object->circle_object_prefilter_effective_min);
  if (object->circle_object_prefilter_requested != 0 &&
      object->circle_object_prefilter_applied == 0 &&
      object->circle_object_prefilter_restored != 0) {
    ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.20f, 1.0f),
                       "object prefilter was requested but restored because it "
                       "removed every radial candidate.");
  }
  ImGui::Text("source=%s failure=%s", object->circle_measure_source.c_str(),
              object->circle_measure_failure_stage.empty()
                  ? "-"
                  : object->circle_measure_failure_stage.c_str());
  ImGui::Text("consistency=%s range=%.1f in/out/removed=%d/%d/%d",
              object->circle_point_consistency_enabled ? "on" : "off",
              object->circle_point_consistency_range,
              object->circle_point_consistency_input_points,
              object->circle_point_consistency_output_points,
              object->circle_point_consistency_removed_points);
  ImGui::Text("source boundary clipped=%d extended=%d candidate_reject=%d",
              object->circle_scan_boundary_clipped_lines,
              object->circle_scan_boundary_extended_samples,
              object->circle_candidate_boundary_reject_count);
}

static ManualFindCircleEdgeParamState
MakeFindEllipseEdgeParamsFromGauge(const ManualGaugeState &gauge) {
  ManualFindCircleEdgeParamState params;
  params.initialized = true;
  params.threshold = gauge.threshold;
  params.method = gauge.method;
  params.linegap = gauge.linegap;
  params.gap = gauge.gap;
  return params;
}

static void EnsureFindEllipseEdgeParamStorage(ManualTestContext &context) {
  context.findellipse_scan_edge_count =
      std::max(1, std::min(3, context.findellipse_scan_edge_count));
  context.findellipse_selected_scan_edge =
      std::max(-1, std::min(context.findellipse_selected_scan_edge,
                            context.findellipse_scan_edge_count));
  context.findellipse_best_fit_edge =
      std::max(0, std::min(context.findellipse_best_fit_edge,
                           context.findellipse_scan_edge_count));
  context.findellipse_recommended_fit_edge =
      std::max(0, std::min(context.findellipse_recommended_fit_edge,
                           context.findellipse_scan_edge_count));
  context.findellipse_relation_edge =
      std::max(0, std::min(context.findellipse_relation_edge,
                           context.findellipse_scan_edge_count));
  context.findellipse_attach_edge =
      std::max(0, std::min(context.findellipse_attach_edge,
                           context.findellipse_scan_edge_count));

  const std::size_t required =
      static_cast<std::size_t>(context.findellipse_scan_edge_count + 1);
  if (context.findellipse_edge_params.size() < required)
    context.findellipse_edge_params.resize(required);

  for (int i = 1; i <= context.findellipse_scan_edge_count; ++i) {
    ManualFindCircleEdgeParamState &params =
        context.findellipse_edge_params[static_cast<std::size_t>(i)];
    params = MakeFindEllipseEdgeParamsFromGauge(context.current_gauge);
  }
}

static std::string FindEllipseEdgeLabel(int edge) {
  if (edge == -1)
    return "Last edge";
  return edge == 0 ? "All edges" : ("Edge " + std::to_string(edge));
}

static void SyncFindEllipseEdgeSelectionToGlobals(ManualTestContext &context,
                                                  const char *reason) {
  EnsureFindEllipseEdgeParamStorage(context);
  InjectManualGaugeInt(context, "global_findellipse_edge_count",
                       context.findellipse_scan_edge_count);
  InjectManualGaugeInt(context, "global_findellipse_selected_edge",
                       context.findellipse_selected_scan_edge);
  InjectManualGaugeInt(context, "global_findellipse_best_edge",
                       context.findellipse_best_fit_edge);
  InjectManualGaugeInt(context, "global_findellipse_recommended_edge",
                       context.findellipse_recommended_fit_edge);
  InjectManualGaugeInt(context, "global_findellipse_relation_edge",
                       context.findellipse_relation_edge);
  InjectManualGaugeInt(context, "global_findellipse_attach_edge",
                       context.findellipse_attach_edge);

  context.current_gauge.dirty = true;
  context.current_gauge.review_status = "editing";
  context.debug_status = "FINDELLIPSE_EDGE_SELECTION_CHANGED";
  context.debug_reason =
      std::string(reason == nullptr ? "FindEllipse edge selection changed"
                                    : reason) +
      "; globals updated, run script to refresh result points";
}

static bool DrawFindEllipseEdgeRoleCombo(const char *label, int &edge,
                                         int edgeCount) {
  bool edited = false;
  edge = std::max(0, std::min(edge, edgeCount));
  const std::string currentLabel = FindEllipseEdgeLabel(edge);
  ImGui::SetNextItemWidth(150.0f);
  if (ImGui::BeginCombo(label, currentLabel.c_str())) {
    if (ImGui::Selectable("All edges", edge == 0)) {
      edge = 0;
      edited = true;
    }
    for (int i = 1; i <= edgeCount; ++i) {
      const std::string item = FindEllipseEdgeLabel(i);
      if (ImGui::Selectable(item.c_str(), edge == i)) {
        edge = i;
        edited = true;
      }
    }
    ImGui::EndCombo();
  }
  return edited;
}

static bool DrawFindEllipseEdgeSelectorPanel(ManualTestContext &context) {
  if (!(context.current_gauge.tool == "FindEllipse" ||
        context.current_gauge.has_ellipse_gauge)) {
    return false;
  }

  bool edited = false;
  EnsureFindEllipseEdgeParamStorage(context);

  ImGui::Separator();
  ImGui::TextUnformatted("Algorithm scan sector: full ellipse gauge");
  ImGui::TextDisabled(
      "FindEllipse currently scans the full ellipse. Edge selection below is "
      "candidate ordinal, not an angular sector.");
  ImGui::TextUnformatted("Detection Edge / Point Column");
  ImGui::TextDisabled(
      "Edge N is the Nth eligible boundary crossing on every ellipse Gauge "
      "line. It mirrors FindCircle selected-edge semantics.");

  ImGui::SetNextItemWidth(100.0f);
  int edgeCount = context.findellipse_scan_edge_count;
  if (ImGui::InputInt("edge count", &edgeCount)) {
    context.findellipse_scan_edge_count = std::max(1, std::min(3, edgeCount));
    if (context.findellipse_selected_scan_edge >
        context.findellipse_scan_edge_count) {
      context.findellipse_selected_scan_edge =
          context.findellipse_scan_edge_count;
    }
    EnsureFindEllipseEdgeParamStorage(context);
    SyncFindEllipseEdgeSelectionToGlobals(context,
                                          "FindEllipse edge count changed");
    edited = true;
  }

  ImGui::SameLine();
  ImGui::SetNextItemWidth(190.0f);
  const std::string currentLabel =
      FindEllipseEdgeLabel(context.findellipse_selected_scan_edge);
  if (ImGui::BeginCombo("selected edge", currentLabel.c_str())) {
    if (ImGui::Selectable("All edges",
                          context.findellipse_selected_scan_edge == 0)) {
      context.findellipse_selected_scan_edge = 0;
      SyncFindEllipseEdgeSelectionToGlobals(context,
                                            "FindEllipse selected All edges");
      edited = true;
    }
    for (int i = 1; i <= context.findellipse_scan_edge_count; ++i) {
      const std::string label = "Edge " + std::to_string(i);
      if (ImGui::Selectable(label.c_str(),
                            context.findellipse_selected_scan_edge == i)) {
        context.findellipse_selected_scan_edge = i;
        const std::string reason = "FindEllipse selected " + label;
        SyncFindEllipseEdgeSelectionToGlobals(context, reason.c_str());
        edited = true;
      }
    }
    ImGui::Separator();
    if (ImGui::Selectable("Last edge",
                          context.findellipse_selected_scan_edge == -1)) {
      context.findellipse_selected_scan_edge = -1;
      SyncFindEllipseEdgeSelectionToGlobals(context,
                                            "FindEllipse selected Last edge");
      edited = true;
    }
    ImGui::EndCombo();
  }

  if (context.findellipse_selected_scan_edge == -1)
    ImGui::Text("Current: Last edge on every ellipse Gauge line");
  else if (context.findellipse_selected_scan_edge > 0)
    ImGui::Text("Current: Edge %d / %d", context.findellipse_selected_scan_edge,
                context.findellipse_scan_edge_count);
  else
    ImGui::TextDisabled(
        "All edges selected: shared Geometry/Edge Params remain unchanged.");

  return edited;
}

static bool DrawFindEllipseEdgeRolePanel(ManualTestContext &context) {
  if (!(context.current_gauge.tool == "FindEllipse" ||
        context.current_gauge.has_ellipse_gauge)) {
    return false;
  }

  bool edited = false;
  EnsureFindEllipseEdgeParamStorage(context);

  ImGui::Separator();
  ImGui::TextUnformatted("Edge Role Binding / 接入点");
  ImGui::TextDisabled(
      "Selection is an edge ordinal. Best/recommended/relation/attach are "
      "evidence metadata and future attach points.");

  ImGui::PushID("findellipse_edge_roles");
  edited |= DrawFindEllipseEdgeRoleCombo("best edge",
                                         context.findellipse_best_fit_edge,
                                         context.findellipse_scan_edge_count);
  ImGui::SameLine();
  if (ImGui::Button("Use All Edges")) {
    context.findellipse_best_fit_edge = 0;
    edited = true;
  }

  edited |= DrawFindEllipseEdgeRoleCombo(
      "recommended edge", context.findellipse_recommended_fit_edge,
      context.findellipse_scan_edge_count);
  ImGui::SameLine();
  if (ImGui::Button("Recommend = Best")) {
    context.findellipse_recommended_fit_edge =
        context.findellipse_best_fit_edge;
    edited = true;
  }

  edited |= DrawFindEllipseEdgeRoleCombo("relation edge",
                                         context.findellipse_relation_edge,
                                         context.findellipse_scan_edge_count);
  ImGui::SameLine();
  ImGui::TextDisabled("future: related/combined edge point-set");

  edited |= DrawFindEllipseEdgeRoleCombo("attach edge",
                                         context.findellipse_attach_edge,
                                         context.findellipse_scan_edge_count);
  ImGui::SameLine();
  ImGui::TextDisabled("future: shape attach/binding");
  ImGui::PopID();

  ImGui::Text(
      "edges: selected=%d best=%d recommended=%d relation=%d attach=%d",
      context.findellipse_selected_scan_edge, context.findellipse_best_fit_edge,
      context.findellipse_recommended_fit_edge,
      context.findellipse_relation_edge, context.findellipse_attach_edge);

  if (edited) {
    SyncFindEllipseEdgeSelectionToGlobals(
        context, "FindEllipse edge role binding changed");
  }

  return edited;
}

static void DrawFindEllipseEdgeEvaluationPanel(ManualTestContext &context) {
  if (!(context.current_gauge.tool == "FindEllipse" ||
        context.current_gauge.has_ellipse_gauge)) {
    return;
  }

  const RuntimeObjectView *object = FindCurrentFindEllipseObject(context);

  ImGui::Separator();
  ImGui::TextUnformatted("Edge Result Evaluation");
  ImGui::TextDisabled("Runtime reports the current selected-edge run. Per-edge "
                      "ranking can be added later from captured evidence.");

  if (object == nullptr) {
    ImGui::TextDisabled("Run script to collect ellipse result evidence.");
    return;
  }

  const int accepted = object->valid_points_count > 0
                           ? object->valid_points_count
                           : object->measure_points_count;
  const double score =
      object->has_fit_ellipse
          ? std::max(0.0,
                     100.0 - static_cast<double>(object->fit_ellipse_avgdist) *
                                 10.0)
          : 0.0;

  ImGui::Text("ui_selected_edge=%d runtime_selected_edge=%d edge_count=%d "
              "fit_ellipse=%s score=%.2f",
              context.findellipse_selected_scan_edge,
              object->ellipse_selected_edge_index,
              context.findellipse_scan_edge_count,
              object->has_fit_ellipse ? "true" : "false", score);
  if (object->ellipse_selected_edge_index !=
      context.findellipse_selected_scan_edge) {
    ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.20f, 1.0f),
                       "Runtime selected edge mismatch. Click Apply To Globals "
                       "/ Run Script again.");
  }

  const ImGuiTableFlags flags =
      ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
      ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp;
  if (ImGui::BeginTable("findellipse_edge_evaluation_table", 8, flags)) {
    ImGui::TableSetupColumn("Run");
    ImGui::TableSetupColumn("Sel");
    ImGui::TableSetupColumn("Lines");
    ImGui::TableSetupColumn("Len");
    ImGui::TableSetupColumn("Points");
    ImGui::TableSetupColumn("Fit?");
    ImGui::TableSetupColumn("AvgDist");
    ImGui::TableSetupColumn("Stage");
    ImGui::TableHeadersRow();

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted(
        FindEllipseEdgeLabel(context.findellipse_selected_scan_edge).c_str());
    ImGui::TableSetColumnIndex(1);
    ImGui::TextUnformatted("*");
    ImGui::TableSetColumnIndex(2);
    ImGui::Text("%d", object->ellipse_scan_line_count);
    ImGui::TableSetColumnIndex(3);
    ImGui::Text("%d", object->ellipse_scan_line_length);
    ImGui::TableSetColumnIndex(4);
    ImGui::Text("%d", accepted);
    ImGui::TableSetColumnIndex(5);
    ImGui::TextUnformatted(object->has_fit_ellipse ? "yes" : "no");
    ImGui::TableSetColumnIndex(6);
    ImGui::Text("%.3f", object->fit_ellipse_avgdist);
    ImGui::TableSetColumnIndex(7);
    ImGui::TextUnformatted(object->ellipse_result_status.empty()
                               ? "-"
                               : object->ellipse_result_status.c_str());
    ImGui::EndTable();
  }
}

static void DrawFindEllipseScanSemanticsPanel(ManualTestContext &context) {
  if (!(context.current_gauge.tool == "FindEllipse" ||
        context.current_gauge.has_ellipse_gauge)) {
    return;
  }

  const RuntimeObjectView *object = FindCurrentFindEllipseObject(context);
  const int linegap = std::max(1, context.current_gauge.linegap);
  const double rx =
      std::abs(static_cast<double>(context.current_gauge.ellipse_x1 -
                                   context.current_gauge.ellipse_x0)) *
      0.5;
  const double ry =
      std::abs(static_cast<double>(context.current_gauge.ellipse_y1 -
                                   context.current_gauge.ellipse_y0)) *
      0.5;
  double circumference = 0.0;
  if (rx > 1.0 && ry > 1.0) {
    const double h = ((rx - ry) * (rx - ry)) / ((rx + ry) * (rx + ry));
    circumference =
        3.14159265358979323846 * (rx + ry) *
        (1.0 + (3.0 * h) / (10.0 + std::sqrt(std::max(0.0, 4.0 - 3.0 * h))));
  }
  const int previewTicks =
      circumference > 1.0
          ? std::max(1, static_cast<int>(std::floor(circumference / linegap)))
          : 0;

  ImGui::Separator();
  ImGui::TextUnformatted("Gauge Scan Semantics");
  ImGui::TextColored(ImVec4(0.55f, 0.90f, 1.0f, 1.0f),
                     "cyan ellipse line = sampling opportunity");
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.18f, 1.0f),
                     "| yellow point = accepted algorithm result");

  if (object == nullptr) {
    ImGui::TextDisabled("Run script to show accepted/rejected counters.");
    ImGui::Text("preview_ticks=%d linegap=%d method=%d gap=%d", previewTicks,
                linegap, context.current_gauge.method,
                context.current_gauge.gap);
    return;
  }

  ImGui::Text("preview_ticks=%d scan_lines=%d scan_len=%d selected_edge=%d",
              previewTicks, object->ellipse_scan_line_count,
              object->ellipse_scan_line_length,
              object->ellipse_selected_edge_index);
  ImGui::Text("measure_points=%d valid_points=%d fit_ellipse=%s avgdist=%.3f",
              object->measure_points_count, object->valid_points_count,
              object->has_fit_ellipse ? "true" : "false",
              object->fit_ellipse_avgdist);
  ImGui::Text("accepted_outside=%d accepted_norm=%.3f/%.3f/%.3f",
              object->ellipse_accepted_points_outside_ellipse_count,
              object->ellipse_accepted_point_norm_min,
              object->ellipse_accepted_point_norm_avg,
              object->ellipse_accepted_point_norm_max);
  ImGui::Text("rejected_boundary_band=%d rejected_norm=%.3f/%.3f/%.3f",
              object->ellipse_rejected_boundary_band_candidate_count,
              object->ellipse_rejected_boundary_band_norm_min,
              object->ellipse_rejected_boundary_band_norm_avg,
              object->ellipse_rejected_boundary_band_norm_max);
  ImGui::Text("consistency=%s range=%.1f in/out/removed=%d/%d/%d",
              object->ellipse_point_consistency_enabled ? "on" : "off",
              object->ellipse_point_consistency_range,
              object->ellipse_point_consistency_input_points,
              object->ellipse_point_consistency_output_points,
              object->ellipse_point_consistency_removed_points);
  ImGui::Text("scan_policy=%s candidate_policy=%s",
              object->ellipse_scan_geometry_policy.empty()
                  ? "-"
                  : object->ellipse_scan_geometry_policy.c_str(),
              object->ellipse_candidate_policy.empty()
                  ? "-"
                  : object->ellipse_candidate_policy.c_str());
}

static bool DrawFindLineEdgeRolePanel(ManualTestContext &context,
                                      const RuntimeObjectView *object) {
  bool edited = false;
  EnsureFindLineEdgeParamStorage(context);

  ImGui::Separator();
  ImGui::TextUnformatted("Edge Role Binding / 接入点");
  ImGui::TextDisabled(
      "Placeholders for annotation/evidence relation. selected=edge used now; "
      "best=runtime/manual best; recommended=future advisor; relation=combined "
      "point-set; attach=shape binding.");

  if (object != nullptr && object->line_best_edge_index > 0 &&
      object->line_best_edge_index <= context.findline_scan_edge_count &&
      context.findline_best_fit_edge == 0) {
    context.findline_best_fit_edge = object->line_best_edge_index;
  }

  ImGui::PushID("findline_edge_roles");
  edited |=
      DrawFindLineEdgeRoleCombo("best edge", context.findline_best_fit_edge,
                                context.findline_scan_edge_count);
  ImGui::SameLine();
  if (ImGui::Button("Use Runtime Best")) {
    if (object != nullptr && object->line_best_edge_index > 0 &&
        object->line_best_edge_index <= context.findline_scan_edge_count) {
      context.findline_best_fit_edge = object->line_best_edge_index;
      edited = true;
    }
  }

  edited |= DrawFindLineEdgeRoleCombo("recommended edge",
                                      context.findline_recommended_fit_edge,
                                      context.findline_scan_edge_count);
  ImGui::SameLine();
  if (ImGui::Button("Recommend = Best")) {
    context.findline_recommended_fit_edge = context.findline_best_fit_edge;
    edited = true;
  }

  edited |=
      DrawFindLineEdgeRoleCombo("relation edge", context.findline_relation_edge,
                                context.findline_scan_edge_count);
  ImGui::SameLine();
  ImGui::TextDisabled("future: related/combined point-set");

  edited |=
      DrawFindLineEdgeRoleCombo("attach edge", context.findline_attach_edge,
                                context.findline_scan_edge_count);
  ImGui::SameLine();
  ImGui::TextDisabled("future: shape attach/binding");
  ImGui::PopID();

  const int runtimeSelectedEdge = FindLineRuntimeSelectedEdgeForScript(
      context.findline_selected_scan_edge, context.findline_scan_edge_count);
  ImGui::Text("globals: selected=%d runtime=%d best=%d recommended=%d "
              "relation=%d attach=%d",
              context.findline_selected_scan_edge, runtimeSelectedEdge,
              context.findline_best_fit_edge,
              context.findline_recommended_fit_edge,
              context.findline_relation_edge, context.findline_attach_edge);

  return edited;
}

static bool DrawFindLineEdgeSelectorPanel(ManualTestContext &context) {
  if (!(context.current_gauge.tool == "FindLine" ||
        context.current_gauge.has_line_gauge)) {
    return false;
  }

  bool edited = false;
  EnsureFindLineEdgeParamStorage(context);

  ImGui::Separator();
  ImGui::TextUnformatted("Detection Edge / Point Column");
  ImGui::TextDisabled(
      "Select which gauge side/point column is being tuned.  Edge-specific "
      "values are saved to globals for evidence replay.");

  ImGui::SetNextItemWidth(100.0f);
  int edgeCount = context.findline_scan_edge_count;
  if (ImGui::InputInt("edge count", &edgeCount)) {
    context.findline_scan_edge_count = std::max(1, std::min(16, edgeCount));
    if (context.findline_selected_scan_edge >
        context.findline_scan_edge_count) {
      context.findline_selected_scan_edge = context.findline_scan_edge_count;
    }
    EnsureFindLineEdgeParamStorage(context);
    edited = true;
  }

  ImGui::SameLine();
  ImGui::SetNextItemWidth(190.0f);
  std::string currentLabel = FindLineSelectedEdgeLabel(
      context.findline_selected_scan_edge, context.findline_scan_edge_count);
  if (ImGui::BeginCombo("selected edge", currentLabel.c_str())) {
    const bool selectedAll = context.findline_selected_scan_edge == 0;
    if (ImGui::Selectable("All edges", selectedAll)) {
      context.findline_selected_scan_edge = 0;
      edited = true;
    }
    for (int i = 1; i <= context.findline_scan_edge_count; ++i) {
      const std::string label =
          FindLineSelectedEdgeLabel(i, context.findline_scan_edge_count);
      const bool selected = context.findline_selected_scan_edge == i;
      if (ImGui::Selectable(label.c_str(), selected)) {
        context.findline_selected_scan_edge = i;
        EnsureFindLineEdgeParamStorage(context);
        ApplyFindLineEdgeParamsToGauge(
            context.findline_edge_params[static_cast<std::size_t>(i)],
            context.current_gauge);
        edited = true;
      }
    }
    ImGui::Separator();
    if (ImGui::Selectable("Last edge",
                          context.findline_selected_scan_edge == -1)) {
      context.findline_selected_scan_edge = -1;
      edited = true;
    }
    ImGui::EndCombo();
  }

  if (context.findline_selected_scan_edge == -1) {
    ImGui::Text("Current: Last edge on every Gauge search line");
    ImGui::TextDisabled("Last edge is resolved after the complete line profile "
                        "is scanned; shared Edge Params remain active.");
  } else if (context.findline_selected_scan_edge > 0) {
    ManualFindLineEdgeParamState &params =
        context.findline_edge_params[static_cast<std::size_t>(
            context.findline_selected_scan_edge)];
    params.initialized = true;

    const int runtimeEdge = FindLineRuntimeSelectedEdgeForScript(
        context.findline_selected_scan_edge, context.findline_scan_edge_count);
    if (runtimeEdge == -1) {
      ImGui::Text("Current: Edge %d / %d (runtime Last edge)",
                  context.findline_selected_scan_edge,
                  context.findline_scan_edge_count);
    } else {
      ImGui::Text("Current: Edge %d / %d", context.findline_selected_scan_edge,
                  context.findline_scan_edge_count);
    }

    ImGui::SetNextItemWidth(100.0f);
    edited |= ImGui::InputInt("edge threshold", &params.threshold);
    params.threshold = std::max(0, std::min(255, params.threshold));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.0f);
    edited |= ImGui::InputInt("edge method", &params.method);
    params.method = std::max(0, std::min(3, params.method));

    ImGui::SetNextItemWidth(100.0f);
    edited |= ImGui::InputInt("edge linegap", &params.linegap);
    params.linegap = std::max(0, std::min(50, params.linegap));

    context.current_gauge.scan_direction =
        context.current_gauge.scan_direction == 1 ? 1 : 2;
    if (ImGui::RadioButton("W only##edge_scan_w",
                           context.current_gauge.scan_direction == 1)) {
      context.current_gauge.scan_direction = 1;
      edited = true;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("H only##edge_scan_h",
                           context.current_gauge.scan_direction == 2)) {
      context.current_gauge.scan_direction = 2;
      edited = true;
    }

    ImGui::BeginDisabled(context.current_gauge.scan_direction != 1);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.0f);
    edited |= ImGui::InputInt("edge wgap", &params.wgap);
    params.wgap = std::max(0, std::min(50, params.wgap));
    ImGui::EndDisabled();

    ImGui::BeginDisabled(context.current_gauge.scan_direction != 2);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.0f);
    edited |= ImGui::InputInt("edge hgap", &params.hgap);
    params.hgap = std::max(0, std::min(50, params.hgap));
    ImGui::EndDisabled();

    ImGui::SetNextItemWidth(100.0f);
    edited |= ImGui::InputInt("edge filterprofile", &params.filterprofile);
    params.filterprofile = std::max(0, std::min(10, params.filterprofile));

    if (edited)
      ApplyFindLineEdgeParamsToGauge(params, context.current_gauge);

    if (ImGui::Button("Copy Current Edge Params To All")) {
      for (int i = 1; i <= context.findline_scan_edge_count; ++i) {
        context.findline_edge_params[static_cast<std::size_t>(i)] = params;
        context.findline_edge_params[static_cast<std::size_t>(i)].initialized =
            true;
      }
      edited = true;
    }
  } else {
    ImGui::TextDisabled("All edges selected: Geometry/Edge Params below edit "
                        "the shared baseline.");
    if (ImGui::Button("Copy Shared Params To All Edges")) {
      const ManualFindLineEdgeParamState params =
          MakeFindLineEdgeParamsFromGauge(context.current_gauge);
      for (int i = 1; i <= context.findline_scan_edge_count; ++i)
        context.findline_edge_params[static_cast<std::size_t>(i)] = params;
      edited = true;
    }
  }

  return edited;
}

static void DrawFindLineEdgeEvaluationPanel(ManualTestContext &context) {
  if (!(context.current_gauge.tool == "FindLine" ||
        context.current_gauge.has_line_gauge)) {
    return;
  }

  const RuntimeObjectView *object = FindCurrentFindLineObject(context);

  ImGui::Separator();
  ImGui::TextUnformatted("Edge Result Evaluation");
  ImGui::TextDisabled("Runtime evidence for Edge 1 / Edge 2 / ... .  Best edge "
                      "is a basic score from accepted points and coverage; it "
                      "does not replace human review.");

  if (object == nullptr) {
    ImGui::TextDisabled("Run script to collect per-edge result evidence.");
    return;
  }

  ImGui::Text(
      "selected_edge=%d evaluated_edges=%d best_edge=%d best_score=%.2f",
      object->line_selected_edge_index, object->line_evaluated_edge_count,
      object->line_best_edge_index, object->line_best_edge_score);

  if (object->line_edge_evaluations.empty()) {
    ImGui::TextDisabled("No edge evaluation captured. Re-run a FindLine script "
                        "that calls measure().");
    return;
  }

  const ImGuiTableFlags flags =
      ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
      ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp;

  if (ImGui::BeginTable("findline_edge_evaluation_table", 9, flags)) {
    ImGui::TableSetupColumn("Edge");
    ImGui::TableSetupColumn("Sel");
    ImGui::TableSetupColumn("Best");
    ImGui::TableSetupColumn("Rows");
    ImGui::TableSetupColumn("Accepted");
    ImGui::TableSetupColumn("Coverage");
    ImGui::TableSetupColumn("Score");
    ImGui::TableSetupColumn("Reject");
    ImGui::TableSetupColumn("Fit?");
    ImGui::TableHeadersRow();

    for (const CxFindLineEdgeEvaluationSnapshot &edge :
         object->line_edge_evaluations) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::Text("Edge %d", edge.edge_index);
      ImGui::TableSetColumnIndex(1);
      ImGui::TextUnformatted(edge.selected ? "*" : "");
      ImGui::TableSetColumnIndex(2);
      ImGui::TextUnformatted(
          edge.edge_index == object->line_best_edge_index ? "best" : "");
      ImGui::TableSetColumnIndex(3);
      ImGui::Text("%d", edge.candidate_scan_rows);
      ImGui::TableSetColumnIndex(4);
      ImGui::Text("%d", edge.accepted_points);
      ImGui::TableSetColumnIndex(5);
      ImGui::Text("%.3f", edge.coverage);
      ImGui::TableSetColumnIndex(6);
      ImGui::Text("%.1f", edge.score);
      ImGui::TableSetColumnIndex(7);
      ImGui::Text("sel=%d end=%d len=%d", edge.rejected_by_selection,
                  edge.rejected_near_endpoint, edge.over_length_runs);
      ImGui::TableSetColumnIndex(8);
      ImGui::TextUnformatted(edge.fit_possible ? "yes" : "no");
    }

    ImGui::EndTable();
  }
}

static void DrawFindLineScanSemanticsPanel(ManualTestContext &context) {
  if (!(context.current_gauge.tool == "FindLine" ||
        context.current_gauge.has_line_gauge)) {
    return;
  }

  const RuntimeObjectView *object = FindCurrentFindLineObject(context);
  const int linegap = std::max(1, context.current_gauge.linegap);
  const double lineLength =
      object != nullptr && object->line_length > 0.0
          ? object->line_length
          : std::hypot(static_cast<double>(context.current_gauge.line_x1 -
                                           context.current_gauge.line_x0),
                       static_cast<double>(context.current_gauge.line_y1 -
                                           context.current_gauge.line_y0));
  const int previewTicks =
      lineLength > 1.0
          ? std::max(1, static_cast<int>(std::floor(lineLength / linegap)) + 1)
          : 0;

  ImGui::Separator();
  ImGui::TextUnformatted("Gauge Scan Semantics");
  ImGui::TextColored(ImVec4(1.0f, 0.92f, 0.45f, 1.0f),
                     "scan tick = sampling opportunity");
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.18f, 1.0f),
                     "| accepted point = algorithm result");

  if (object == nullptr) {
    ImGui::TextDisabled("Run script to show accepted/rejected counters.");
    ImGui::Text("preview_ticks=%d linegap=%d method=%d", previewTicks, linegap,
                context.current_gauge.method);
    return;
  }

  ImGui::Text("preview_ticks=%d scan_rows_examined=%d foreground_rows=%d",
              previewTicks, object->line_scan_rows_examined,
              object->line_scan_rows_with_foreground);
  ImGui::Text("scan_runs=%d emitted=%d accepted_points=%d valid_points=%d",
              object->line_scan_runs_total, object->line_scan_points_emitted,
              object->line_measure_points_count,
              object->valid_line_points_count);
  ImGui::Text(
      "rejected_near_endpoint=%d rejected_by_selection=%d over_length=%d",
      object->line_scan_runs_rejected_near_endpoint,
      object->line_scan_runs_rejected_by_selection,
      object->line_scan_runs_over_length_limit);
  ImGui::Text("consistency=%s range=%.1f in/out/removed=%d/%d/%d",
              object->line_point_consistency_enabled ? "on" : "off",
              object->line_point_consistency_range,
              object->line_point_consistency_input_points,
              object->line_point_consistency_output_points,
              object->line_point_consistency_removed_points);
  ImGui::Text("fit_line=%s linegap=%d method=%d threshold=%d",
              object->has_fit_line ? "true" : "false",
              object->line_measure_linegap, object->line_measure_method,
              object->line_measure_threshold);
}

static bool DrawMetrologySliderIntLocal(const char *label, int &value,
                                        int minValue, int maxValue,
                                        const char *hint = nullptr) {
  bool edited = false;
  value = std::max(minValue, std::min(maxValue, value));
  ImGui::TextUnformatted(label);
  ImGui::SameLine(170.0f);
  ImGui::SetNextItemWidth(190.0f);
  edited |=
      ImGui::SliderInt((std::string("##metrology_slider_") + label).c_str(),
                       &value, minValue, maxValue);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(80.0f);
  edited |= ImGui::InputInt((std::string("##metrology_input_") + label).c_str(),
                            &value);
  value = std::max(minValue, std::min(maxValue, value));
  if (hint != nullptr && hint[0] != '\0') {
    ImGui::SameLine();
    ImGui::TextDisabled("%s", hint);
  }
  return edited;
}

static bool DrawMetrologyComboLocal(const char *label, int &value,
                                    const char *const *items, int itemCount) {
  bool edited = false;
  value = std::max(0, std::min(itemCount - 1, value));
  ImGui::TextUnformatted(label);
  ImGui::SameLine(170.0f);
  ImGui::SetNextItemWidth(220.0f);
  edited |= ImGui::Combo((std::string("##metrology_combo_") + label).c_str(),
                         &value, items, itemCount);
  return edited;
}

static int
ResolveMetrologyGaugeLineCountLocal(const ManualTestContext &context,
                                    const ManualMetrologyUiState &m) {
  int count = std::max(1, m.scan_profile_max_lines);
  if (context.current_gauge.tool == "FindCircle" ||
      context.current_gauge.has_circle_gauge) {
    const RuntimeObjectView *object = FindCurrentFindCircleObject(context);
    if (object != nullptr && object->circle_scan_lines_processed > 0)
      count = object->circle_scan_lines_processed;
  } else if (context.current_gauge.tool == "FindLine" ||
             context.current_gauge.has_line_gauge) {
    const RuntimeObjectView *object = FindCurrentFindLineObject(context);
    if (object != nullptr && object->line_scan_rows_examined > 0)
      count = object->line_scan_rows_examined;
  } else if (context.current_gauge.tool == "FastMatch" ||
             context.current_gauge.primary_object_type == "FastMatch") {
    const RuntimeObjectView *object = FindCurrentFastMatchObject(context);
    const int direction = std::max(0, std::min(
        3, context.fastmatch_selected_learn_edge));
    if (object != nullptr &&
        object->fastmatch_directional_probe_scans[
            static_cast<std::size_t>(direction)] > 0)
      count = object->fastmatch_directional_probe_scans[
          static_cast<std::size_t>(direction)];
  }
  return std::max(1, std::min(4096, count));
}

static bool
DrawMetrologyGaugeLineSelectorLocal(const ManualTestContext &context,
                                    ManualMetrologyUiState &m) {
  const int lineCount = ResolveMetrologyGaugeLineCountLocal(context, m);
  const int previous = m.gauge_line_num;
  const bool edited = DrawMetrologySliderIntLocal(
      "Gauge Line NUM", m.gauge_line_num, 1, lineCount, "one-based");
  if (edited && m.gauge_line_num != previous) {
    m.height_peak_analysis_ready = false;
    m.height_peak_analysis_status = "PENDING";
    m.height_peak_analysis_reason =
        "Gauge Line NUM changed; run analysis for the selected line";
    m.height_peaks.clear();
  }
  ImGui::TextDisabled("Selected Gauge Line: NUM %d / %d", m.gauge_line_num,
                      lineCount);
  if (context.current_gauge.tool == "FastMatch" ||
      context.current_gauge.primary_object_type == "FastMatch") {
    static const char *kDirectionNames[] = {"Top", "Bottom", "Left", "Right"};
    const int direction = std::max(0, std::min(
        3, context.fastmatch_selected_learn_edge));
    ImGui::TextDisabled("FastMatch probe: %s (select its Directional FindLine tab)",
                        kDirectionNames[direction]);
    ImGui::TextColored(ImVec4(1.0f, 0.91f, 0.28f, 1.0f),
                       "Yellow: selected Gauge Line %d", m.gauge_line_num);
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.30f, 0.96f, 0.57f, 1.0f),
                       "Green: accepted model point");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 0.34f, 0.34f, 1.0f),
                       "Red: no accepted point");
    ImGui::TextDisabled("Cyan lines are all FastMatch scan ticks; the waveform and image highlight use the same one-based Gauge Line NUM.");
  }
  return edited;
}
static cxvision::metrology_analytics::CxMetrologyUiGlobalFields
BuildMetrologyUiGlobalFieldsLocal(const ManualMetrologyUiState &m) {
  cxvision::metrology_analytics::CxMetrologyUiGlobalFields fields;
  fields.enabled = m.enabled;
  fields.active_tab = m.active_tab;

  fields.gauge_line_num = m.gauge_line_num;

  fields.show_scan_profile = m.show_scan_profile;
  fields.scan_profile_source = m.scan_profile_source;
  fields.scan_profile_max_lines = m.scan_profile_max_lines;
  fields.scan_profile_sample_stride = m.scan_profile_sample_stride;
  fields.scan_profile_edge_band_index = m.scan_profile_edge_band_index;
  fields.scan_profile_smoothing_radius = m.scan_profile_smoothing_radius;

  fields.show_edge_band_candidates = m.show_edge_band_candidates;
  fields.candidate_rank = m.candidate_rank;
  fields.candidate_min_gradient = m.candidate_min_gradient;
  fields.candidate_max_width = m.candidate_max_width;
  fields.feature_map_mode = m.feature_map_mode;
  fields.feature_map_normalize = m.feature_map_normalize;

  fields.boundary_preview_enabled = m.boundary_preview_enabled;
  fields.boundary_baseline_mode = m.boundary_baseline_mode;
  fields.boundary_denoise_mode = m.boundary_denoise_mode;
  fields.boundary_smoothing_radius = m.boundary_smoothing_radius;
  fields.boundary_baseline_window = m.boundary_baseline_window;
  fields.boundary_response_mode = m.boundary_response_mode;
  fields.boundary_polarity = m.boundary_polarity;
  fields.boundary_wavelet_scale = m.boundary_wavelet_scale;
  fields.boundary_trigger_threshold_permille =
      m.boundary_trigger_threshold_permille;
  fields.boundary_level_permille = m.boundary_level_permille;
  fields.boundary_hysteresis_permille = m.boundary_hysteresis_permille;
  fields.boundary_gate_start_permille = m.boundary_gate_start_permille;
  fields.boundary_gate_end_permille = m.boundary_gate_end_permille;
  fields.boundary_selection_mode = m.boundary_selection_mode;
  fields.boundary_nth_candidate = m.boundary_nth_candidate;
  fields.boundary_min_plateau_width = m.boundary_min_plateau_width;
  fields.boundary_min_amplitude_permille =
      m.boundary_min_amplitude_permille;
  fields.boundary_pair_min_width = m.boundary_pair_min_width;
  fields.boundary_pair_max_width = m.boundary_pair_max_width;
  fields.boundary_pair_anchor_mode = m.boundary_pair_anchor_mode;
  fields.boundary_subpixel_mode = m.boundary_subpixel_mode;
  fields.boundary_show_conditioned = m.boundary_show_conditioned;
  fields.boundary_show_response = m.boundary_show_response;
  fields.boundary_show_scalogram = m.boundary_show_scalogram;
  fields.boundary_reference_mode = m.boundary_reference_mode;
  fields.boundary_reference_bound = m.boundary_reference_bound;
  fields.boundary_reference_position_permille =
      m.boundary_reference_position_permille;

  fields.surface_source = m.surface_source;
  fields.surface_width = m.surface_width;
  fields.surface_height = m.surface_height;
  fields.surface_stride = m.surface_stride;
  fields.surface_z_channel = m.surface_z_channel;
  fields.surface_area_method = m.surface_area_method;
  fields.histogram_bins = m.histogram_bins;
  fields.histogram_mode = m.histogram_mode;
  fields.histogram_log_scale = m.histogram_log_scale;

  fields.peak_max_count = m.peak_max_count;
  fields.peak_order = m.peak_order;
  fields.peak_min_prominence_permille = m.peak_min_prominence_permille;
  fields.peak_min_distance_bins = m.peak_min_distance_bins;
  fields.peak_background = m.peak_background;
  fields.peak_invert = m.peak_invert;

  fields.curve_fit_source = m.curve_fit_source;
  fields.curve_fit_function = m.curve_fit_function;
  fields.curve_fit_auto_estimate = m.curve_fit_auto_estimate;
  fields.curve_fit_auto_plot = m.curve_fit_auto_plot;
  fields.curve_fit_full_range = m.curve_fit_full_range;
  fields.curve_fit_output_residual = m.curve_fit_output_residual;
  fields.curve_fit_range_start_permille = m.curve_fit_range_start_permille;
  fields.curve_fit_range_end_permille = m.curve_fit_range_end_permille;

  fields.critical_dimension_source = m.critical_dimension_source;
  fields.critical_dimension_function = m.critical_dimension_function;
  fields.critical_dimension_auto_fit = m.critical_dimension_auto_fit;
  fields.critical_dimension_full_range = m.critical_dimension_full_range;
  fields.critical_dimension_range_start_permille =
      m.critical_dimension_range_start_permille;
  fields.critical_dimension_range_end_permille =
      m.critical_dimension_range_end_permille;
  fields.critical_dimension_draw_whole_circle =
      m.critical_dimension_draw_whole_circle;

  fields.enable_plane_correction = m.enable_plane_correction;
  fields.plane_method = m.plane_method;
  fields.plane_reference_mode = m.plane_reference_mode;
  fields.plane_huber_delta_permille = m.plane_huber_delta_permille;

  fields.x_unit = m.x_unit;
  fields.y_unit = m.y_unit;
  fields.z_unit = m.z_unit;
  fields.x_scale_permille = m.x_scale_permille;
  fields.y_scale_permille = m.y_scale_permille;
  fields.z_scale_permille = m.z_scale_permille;
  fields.enable_gaussian_z = m.enable_gaussian_z;
  fields.gaussian_z_sigma_permille = m.gaussian_z_sigma_permille;
  fields.gaussian_seed = m.gaussian_seed;

  fields.enable_iso_roughness_1d = m.enable_iso_roughness_1d;
  fields.roughness_profile_axis = m.roughness_profile_axis;
  fields.roughness_profile_index = m.roughness_profile_index;
  fields.roughness_cutoff_px = m.roughness_cutoff_px;
  fields.roughness_bins = m.roughness_bins;
  return fields;
}

static void InjectMetrologyUiGlobals(ManualTestContext &context) {
  const auto fields = BuildMetrologyUiGlobalFieldsLocal(context.metrology_ui);
  for (const auto &item :
       cxvision::metrology_analytics::BuildMetrologyUiGlobalSnapshot(fields)) {
    InjectManualGaugeInt(context, item.name, item.value);
  }
}

static std::string BuildMetrologyUiSummary(const ManualMetrologyUiState &m) {
  return cxvision::metrology_analytics::BuildMetrologyUiGlobalSummary(
      BuildMetrologyUiGlobalFieldsLocal(m));
}

static cxvision::metrology_analytics::CxLengthUnit
MetrologyLengthUnitLocal(int value) {
  using cxvision::metrology_analytics::CxLengthUnit;
  switch (value) {
  case 1:
    return CxLengthUnit::Nanometer;
  case 2:
    return CxLengthUnit::Micrometer;
  case 3:
    return CxLengthUnit::Millimeter;
  default:
    return CxLengthUnit::Pixel;
  }
}

static std::string
ResolveMetrologyImagePathLocal(const ManualTestContext &context) {
  if (!context.image_file_path.empty())
    return context.image_file_path;
  return context.current_evidence_selection.image_path;
}

static double SampleMetrologyScalarBilinearLocal(const cv::Mat &values,
                                                 double x, double y) {
  if (values.empty())
    return 0.0;
  x = std::max(0.0, std::min(static_cast<double>(values.cols - 1), x));
  y = std::max(0.0, std::min(static_cast<double>(values.rows - 1), y));
  const int x0 = static_cast<int>(std::floor(x));
  const int y0 = static_cast<int>(std::floor(y));
  const int x1 = std::min(x0 + 1, values.cols - 1);
  const int y1 = std::min(y0 + 1, values.rows - 1);
  const double tx = x - static_cast<double>(x0);
  const double ty = y - static_cast<double>(y0);
  const double v00 = values.at<double>(y0, x0);
  const double v10 = values.at<double>(y0, x1);
  const double v01 = values.at<double>(y1, x0);
  const double v11 = values.at<double>(y1, x1);
  return (1.0 - tx) * (1.0 - ty) * v00 + tx * (1.0 - ty) * v10 +
         (1.0 - tx) * ty * v01 + tx * ty * v11;
}

static bool MetrologyRequiresRuntimeGaugeLineLocal(
    const ManualTestContext &context) {
  const ManualGaugeState &gauge = context.current_gauge;
  return gauge.tool == "FindLine" || gauge.tool == "FindCircle" ||
         gauge.tool == "FindEllipse" || gauge.tool == "FastMatch" ||
         gauge.primary_object_type == "FastMatch" || gauge.has_line_gauge ||
         gauge.has_circle_gauge || gauge.has_ellipse_gauge;
}

static void *ResolveMetrologyRuntimeObjectLocal(
    const ManualTestContext &context, const ParserDebugBridge *parserDebugBridge,
    const std::string &toolType, std::string &ownerRef) {
  ownerRef.clear();
  if (parserDebugBridge == nullptr)
    return nullptr;

  std::vector<std::string> candidates;
  auto addCandidate = [&](const std::string &name) {
    if (!name.empty() &&
        std::find(candidates.begin(), candidates.end(), name) == candidates.end())
      candidates.push_back(name);
  };
  addCandidate(context.current_result_ref.source_object);
  addCandidate(context.current_gauge.primary_object_name);
  for (const RuntimeObjectView &object : context.runtime_objects) {
    if (object.type == toolType)
      addCandidate(object.name);
  }

  for (const std::string &candidate : candidates) {
    void *object = parserDebugBridge->QueryClassObject(toolType, candidate);
    if (object != nullptr) {
      ownerRef = candidate;
      return object;
    }
  }
  return nullptr;
}

static bool BuildMetrologyProfileFromRuntimeSegmentLocal(
    const cv::Mat &values, cxvision::metrology_analytics::CxPhysUnit unit,
    const CxShapePoint &a, const CxShapePoint &b,
    cxvision::metrology_analytics::CxSurfaceField &field,
    std::string &reason, double *profileLengthPx = nullptr) {
  const int sampleCount = field.xres();
  const double dx = b.x - a.x;
  const double dy = b.y - a.y;
  const double length = std::sqrt(dx * dx + dy * dy);
  if (sampleCount < 2 || length <= 1.0) {
    reason = "runtime Gauge Line scan segment is too small for profile sampling";
    return false;
  }
  if (profileLengthPx != nullptr)
    *profileLengthPx = length;

  for (int i = 0; i < sampleCount; ++i) {
    const double t = static_cast<double>(i) / static_cast<double>(sampleCount - 1);
    const double x = a.x + dx * t;
    const double y = a.y + dy * t;
    const double value = SampleMetrologyScalarBilinearLocal(values, x, y);
    field.setAt(i, 0, value * unit.z_scale_per_pixel);
  }
  return true;
}

static bool BuildMetrologyGaugeLineSurfaceFieldLocal(
    const ManualTestContext &context, const ParserDebugBridge *parserDebugBridge,
    const cv::Mat &values, cxvision::metrology_analytics::CxPhysUnit unit,
    cxvision::metrology_analytics::CxSurfaceField &field,
    std::string &sourceRef, std::string &reason,
    double *profileLengthPx = nullptr) {

  using cxvision::metrology_analytics::CxSurfaceField;
  const ManualGaugeState &gauge = context.current_gauge;
  const ManualMetrologyUiState &m = context.metrology_ui;
  const int sampleCount = std::max(16, std::min(4096, m.surface_width));

  field = CxSurfaceField(sampleCount, 1, unit);
  if (parserDebugBridge == nullptr) {
    reason = "runtime ParserDebugBridge is not available for Gauge Line sampling";
    return false;
  }

  if (gauge.tool == "FastMatch" ||
      gauge.primary_object_type == "FastMatch") {
    std::string ownerRef;
    FastMatch *fastMatch = static_cast<FastMatch *>(
        ResolveMetrologyRuntimeObjectLocal(context, parserDebugBridge,
                                           "FastMatch", ownerRef));
    if (fastMatch == nullptr) {
      reason = "runtime FastMatch object is not available for Gauge Line sampling";
      return false;
    }
    const int direction = std::max(0, std::min(
        3, context.fastmatch_selected_learn_edge));
    const FastMatch::DirectionalProbeEvidence &probe =
        fastMatch->getdirectionalprobeevidence(direction);
    if (!probe.executed || probe.scan_lines.empty()) {
      reason = "selected FastMatch directional FindLine Probe has no retained scan lines";
      return false;
    }
    const int scanIndex = std::max(
        0, std::min(static_cast<int>(probe.scan_lines.size()) - 1,
                    m.gauge_line_num - 1));
    const FastMatch::DirectionalProbeScanLine &segment =
        probe.scan_lines[static_cast<std::size_t>(scanIndex)];
    if (!BuildMetrologyProfileFromRuntimeSegmentLocal(
            values, unit, segment.p0, segment.p1, field, reason,
            profileLengthPx))
      return false;

    static const char *kDirectionNames[] = {"Top", "Bottom", "Left", "Right"};
    sourceRef = "runtime:gauge_line:FastMatch:object=" + ownerRef +
                "; direction=" + kDirectionNames[direction] +
                "; num=" + std::to_string(scanIndex + 1) + "/" +
                std::to_string(probe.scan_lines.size()) +
                "; scan_type=" + std::to_string(probe.scan_type) +
                "; source=directional_findline_probe";
    return true;
  }

  if ((gauge.tool == "FindLine" || gauge.has_line_gauge) &&
      gauge.has_line_gauge) {
    std::string ownerRef;
    FindLine *lineTool = static_cast<FindLine *>(
        ResolveMetrologyRuntimeObjectLocal(context, parserDebugBridge,
                                           "FindLine", ownerRef));
    if (lineTool == nullptr) {
      reason = "runtime FindLine object is not available for Gauge Line sampling";
      return false;
    }


    const int scanCountW = lineTool->getscanlinecount(0);
    const int scanCountH = lineTool->getscanlinecount(1);
    const int totalScanCount = scanCountW + scanCountH;
    if (totalScanCount <= 0) {
      reason = "runtime FindLine object has no scan lines";
      return false;
    }

    const int selectedOrdinal = std::max(
        0, std::min(totalScanCount - 1, m.gauge_line_num - 1));
    const int scanType = selectedOrdinal < scanCountW ? 0 : 1;
    const int scanIndex =
        scanType == 0 ? selectedOrdinal : selectedOrdinal - scanCountW;
    CxShapePoint a, b;
    if (!lineTool->getscanline(scanType, scanIndex, a, b)) {
      reason = "runtime FindLine getscanline failed for selected Gauge Line";
      return false;
    }

    if (!BuildMetrologyProfileFromRuntimeSegmentLocal(values, unit, a, b, field,
                                                      reason, profileLengthPx))

      return false;

    sourceRef = "runtime:gauge_line:FindLine:object=" + ownerRef +
                "; num=" + std::to_string(selectedOrdinal + 1) + "/" +
                std::to_string(totalScanCount) +
                "; scan_type=" + std::to_string(scanType) +
                "; scan_index=" + std::to_string(scanIndex) +
                "; source=getscanline";
    return true;
  }

  if ((gauge.tool == "FindCircle" || gauge.has_circle_gauge) &&
      gauge.has_circle_gauge) {
    std::string ownerRef;
    FindCircle *circleTool = static_cast<FindCircle *>(
        ResolveMetrologyRuntimeObjectLocal(context, parserDebugBridge,
                                           "FindCircle", ownerRef));
    if (circleTool == nullptr) {
      reason = "runtime FindCircle object is not available for Gauge Line sampling";
      return false;
    }

    const int scanCount = circleTool->getscanlinecount();
    if (scanCount <= 0) {
      reason = "runtime FindCircle object has no scan lines";
      return false;
    }

    const int scanIndex =
        std::max(0, std::min(scanCount - 1, m.gauge_line_num - 1));
    CxShapePoint a, b;
    if (!circleTool->getscanline(scanIndex, a, b)) {
      reason = "runtime FindCircle getscanline failed for selected Gauge Line";
      return false;
    }

    if (!BuildMetrologyProfileFromRuntimeSegmentLocal(values, unit, a, b, field,
                                                      reason, profileLengthPx))

      return false;

    sourceRef = "runtime:gauge_line:FindCircle:object=" + ownerRef +
                "; num=" + std::to_string(scanIndex + 1) + "/" +
                std::to_string(scanCount) +
                "; scan_index=" + std::to_string(scanIndex) +
                "; source=getscanline";
    return true;
  }

  if ((gauge.tool == "FindEllipse" || gauge.has_ellipse_gauge) &&
      gauge.has_ellipse_gauge) {
    std::string ownerRef;
    FindEllipse *ellipseTool = static_cast<FindEllipse *>(
        ResolveMetrologyRuntimeObjectLocal(context, parserDebugBridge,
                                           "FindEllipse", ownerRef));
    if (ellipseTool == nullptr) {
      reason = "runtime FindEllipse object is not available for Gauge Line sampling";
      return false;
    }

    const int scanCount = ellipseTool->getscanlinecount();
    if (scanCount <= 0) {
      reason = "runtime FindEllipse object has no scan lines";
      return false;
    }

    const int scanIndex =
        std::max(0, std::min(scanCount - 1, m.gauge_line_num - 1));
    CxShapePoint a, b;
    if (!ellipseTool->getscanline(scanIndex, a, b)) {
      reason = "runtime FindEllipse getscanline failed for selected Gauge Line";
      return false;
    }

    if (!BuildMetrologyProfileFromRuntimeSegmentLocal(values, unit, a, b, field,
                                                      reason, profileLengthPx))

      return false;

    sourceRef = "runtime:gauge_line:FindEllipse:object=" + ownerRef +
                "; num=" + std::to_string(scanIndex + 1) + "/" +
                std::to_string(scanCount) +
                "; scan_index=" + std::to_string(scanIndex) +
                "; source=getscanline";
    return true;
  }

  reason =
      "current gauge has no FindLine/FindCircle/FindEllipse runtime geometry for profile sampling";
  return false;
}

struct MetrologyConclusionMarkerLocal {
  int sample_index = 0;
  double axis_px = 0.0;
};

static std::vector<MetrologyConclusionMarkerLocal>
BuildMetrologyConclusionMarkersLocal(
    const ManualTestContext &context, const ParserDebugBridge *parserDebugBridge,
    int sampleCount) {
  std::vector<MetrologyConclusionMarkerLocal> markers;
  if (sampleCount < 2 || parserDebugBridge == nullptr)
    return markers;

  auto appendUniqueProjectedPoint =
      [&](const CxShapePoint &a, const CxShapePoint &b, double x, double y) {
        const double dx = b.x - a.x;
        const double dy = b.y - a.y;
        const double len2 = dx * dx + dy * dy;
        if (len2 <= 1.0e-9)
          return;
        double t = ((x - a.x) * dx + (y - a.y) * dy) / len2;
        t = std::max(0.0, std::min(1.0, t));
        const double axisPx = t * std::sqrt(len2);
        const int sampleIndex = std::max(
            0, std::min(sampleCount - 1,
                        static_cast<int>(std::lround(
                            t * static_cast<double>(sampleCount - 1)))));
        const auto duplicate = std::find_if(
            markers.begin(), markers.end(),
            [&](const MetrologyConclusionMarkerLocal &marker) {
              return std::abs(marker.axis_px - axisPx) <= 0.25;
            });
        if (duplicate == markers.end()) {
          MetrologyConclusionMarkerLocal marker;
          marker.sample_index = sampleIndex;
          marker.axis_px = axisPx;
          markers.push_back(marker);
        }
      };

  const ManualGaugeState &gauge = context.current_gauge;
  const ManualMetrologyUiState &m = context.metrology_ui;

  if (gauge.tool == "FastMatch" ||
      gauge.primary_object_type == "FastMatch") {
    std::string ownerRef;
    FastMatch *fastMatch = static_cast<FastMatch *>(
        ResolveMetrologyRuntimeObjectLocal(context, parserDebugBridge,
                                           "FastMatch", ownerRef));
    if (fastMatch == nullptr)
      return markers;
    const int direction = std::max(0, std::min(
        3, context.fastmatch_selected_learn_edge));
    const FastMatch::DirectionalProbeEvidence &probe =
        fastMatch->getdirectionalprobeevidence(direction);
    if (!probe.executed || probe.scan_lines.empty())
      return markers;
    const int scanIndex = std::max(
        0, std::min(static_cast<int>(probe.scan_lines.size()) - 1,
                    m.gauge_line_num - 1));
    const FastMatch::DirectionalProbeScanLine &segment =
        probe.scan_lines[static_cast<std::size_t>(scanIndex)];

    // FastMatch preserves the selected candidate by scan index.  Do not use
    // a nearest-point heuristic here: a neighbouring Gauge Line can be close
    // in pixel space but belongs to a different Point Column decision.
    if (scanIndex < static_cast<int>(probe.selected_point_valid_by_scan.size()) &&
        probe.selected_point_valid_by_scan[static_cast<std::size_t>(scanIndex)] != 0 &&
        scanIndex < static_cast<int>(probe.selected_point_by_scan.size())) {
      const CxShapePoint &point =
          probe.selected_point_by_scan[static_cast<std::size_t>(scanIndex)];
      appendUniqueProjectedPoint(segment.p0, segment.p1,
                                 point.x, point.y);
    }
    return markers;
  }

  if ((gauge.tool == "FindLine" || gauge.has_line_gauge) &&
      gauge.has_line_gauge) {
    std::string ownerRef;
    FindLine *lineTool = static_cast<FindLine *>(ResolveMetrologyRuntimeObjectLocal(
        context, parserDebugBridge, "FindLine", ownerRef));
    if (lineTool == nullptr)
      return markers;

    const int scanCountW = lineTool->getscanlinecount(0);
    const int scanCountH = lineTool->getscanlinecount(1);
    const int totalScanCount = scanCountW + scanCountH;
    if (totalScanCount <= 0)
      return markers;

    const int selectedOrdinal =
        std::max(0, std::min(totalScanCount - 1, m.gauge_line_num - 1));
    const int scanType = selectedOrdinal < scanCountW ? 0 : 1;
    const int scanIndex =
        scanType == 0 ? selectedOrdinal : selectedOrdinal - scanCountW;
    CxShapePoint a, b;
    if (!lineTool->getscanline(scanType, scanIndex, a, b))
      return markers;

    const int diagnosticCount = lineTool->getscandiagnosticcount();
    for (int i = 0; i < diagnosticCount; ++i) {
      FindLineMeasureInputDebug::ScanDiagnostic diag;
      if (!lineTool->getscandiagnostic(i, diag) || !diag.accepted)
        continue;
      const int diagOrdinal = diag.scan_type <= 0 ? diag.scan_index
                                                  : scanCountW + diag.scan_index;
      if (diagOrdinal != selectedOrdinal)
        continue;
      bool usedMultiPoint = false;
      for (std::size_t j = 0; j + 1 < diag.accepted_points_xy.size(); j += 2) {
        appendUniqueProjectedPoint(a, b, diag.accepted_points_xy[j],
                                   diag.accepted_points_xy[j + 1]);
        usedMultiPoint = true;
      }
      if (!usedMultiPoint)
        appendUniqueProjectedPoint(a, b, diag.accepted_x, diag.accepted_y);
    }

    std::sort(markers.begin(), markers.end(),
              [](const MetrologyConclusionMarkerLocal &lhs,
                 const MetrologyConclusionMarkerLocal &rhs) {
                return lhs.axis_px < rhs.axis_px;
              });
    return markers;
  }

  if ((gauge.tool == "FindCircle" || gauge.has_circle_gauge) &&
      gauge.has_circle_gauge) {
    std::string ownerRef;
    FindCircle *circleTool = static_cast<FindCircle *>(
        ResolveMetrologyRuntimeObjectLocal(context, parserDebugBridge,
                                           "FindCircle", ownerRef));
    if (circleTool == nullptr)
      return markers;

    const int scanCount = circleTool->getscanlinecount();
    if (scanCount <= 0)
      return markers;

    const int selectedScanIndex =
        std::max(0, std::min(scanCount - 1, m.gauge_line_num - 1));
    CxShapePoint a, b;
    if (!circleTool->getscanline(selectedScanIndex, a, b))
      return markers;

    const int diagnosticCount = circleTool->getscandiagnosticcount();
    for (int i = 0; i < diagnosticCount; ++i) {
      FindCircleMeasureGeometryDebug::ScanDiagnostic diag;
      if (!circleTool->getscandiagnostic(i, diag) || !diag.accepted ||
          diag.scan_index != selectedScanIndex)
        continue;
      bool usedMultiPoint = false;
      for (std::size_t j = 0; j + 1 < diag.accepted_points_xy.size(); j += 2) {
        appendUniqueProjectedPoint(a, b, diag.accepted_points_xy[j],
                                   diag.accepted_points_xy[j + 1]);
        usedMultiPoint = true;
      }
      if (!usedMultiPoint)
        appendUniqueProjectedPoint(a, b, diag.accepted_x, diag.accepted_y);
    }

    std::sort(markers.begin(), markers.end(),
              [](const MetrologyConclusionMarkerLocal &lhs,
                 const MetrologyConclusionMarkerLocal &rhs) {
                return lhs.axis_px < rhs.axis_px;
              });
    return markers;
  }

  if ((gauge.tool == "FindEllipse" || gauge.has_ellipse_gauge) &&
      gauge.has_ellipse_gauge) {
    std::string ownerRef;
    FindEllipse *ellipseTool = static_cast<FindEllipse *>(
        ResolveMetrologyRuntimeObjectLocal(context, parserDebugBridge,
                                           "FindEllipse", ownerRef));
    if (ellipseTool == nullptr)
      return markers;

    const int scanCount = ellipseTool->getscanlinecount();
    if (scanCount <= 0)
      return markers;

    const int selectedScanIndex =
        std::max(0, std::min(scanCount - 1, m.gauge_line_num - 1));
    CxShapePoint a, b;
    if (!ellipseTool->getscanline(selectedScanIndex, a, b))
      return markers;

    const int diagnosticCount = ellipseTool->getscandiagnosticcount();
    for (int i = 0; i < diagnosticCount; ++i) {
      FindEllipseMeasureGeometryDebug::ScanDiagnostic diag;
      if (!ellipseTool->getscandiagnostic(i, diag) || !diag.accepted ||
          diag.scan_index != selectedScanIndex)
        continue;
      bool usedMultiPoint = false;
      for (std::size_t j = 0; j + 1 < diag.accepted_points_xy.size(); j += 2) {
        appendUniqueProjectedPoint(a, b, diag.accepted_points_xy[j],
                                   diag.accepted_points_xy[j + 1]);
        usedMultiPoint = true;
      }
      if (!usedMultiPoint)
        appendUniqueProjectedPoint(a, b, diag.accepted_x, diag.accepted_y);
    }
  }

  std::sort(markers.begin(), markers.end(),
            [](const MetrologyConclusionMarkerLocal &lhs,
               const MetrologyConclusionMarkerLocal &rhs) {
              return lhs.axis_px < rhs.axis_px;
            });
  return markers;
}


static bool AnalyzeMetrologyHeightPeaksLocal(
    ManualTestContext &context, const ParserDebugBridge *parserDebugBridge) {
  using namespace cxvision::metrology_analytics;
  ManualMetrologyUiState &m = context.metrology_ui;
  m.height_peak_analysis_ready = false;
  m.height_peaks.clear();
  m.height_peak_analysis_status = "METROLOGY_HEIGHT_PEAKS_RUNNING";
  m.height_peak_analysis_reason.clear();

  try {
    CxPhysUnit unit;
    unit.x_unit = MetrologyLengthUnitLocal(m.x_unit);
    unit.y_unit = MetrologyLengthUnitLocal(m.y_unit);
    unit.z_unit = MetrologyLengthUnitLocal(m.z_unit);
    unit.x_scale_per_pixel = std::max(1, m.x_scale_permille) / 1000.0;
    unit.y_scale_per_pixel = std::max(1, m.y_scale_permille) / 1000.0;
    unit.z_scale_per_pixel = std::max(1, m.z_scale_permille) / 1000.0;

    const int requestedWidth = std::max(1, std::min(2048, m.surface_width));
    const int requestedHeight = std::max(1, std::min(2048, m.surface_height));
    const int stride = std::max(1, m.surface_stride);
    CxSurfaceField field;

    if (m.surface_source == 2) {
      field = CxSyntheticSurfaceFactory::bimodal(
          requestedWidth, requestedHeight, 0.5, -3.0, 1.0, 3.0, 1.0,
          static_cast<unsigned int>(std::max(0, m.gaussian_seed)), unit);
      m.height_peak_source_ref = "synthetic:bimodal";
    } else {
      if (m.surface_source == 1) {
        m.height_peak_analysis_status =
            "METROLOGY_SURFACE_SOURCE_PENDING_BINDING";
        m.height_peak_analysis_reason =
            "segmentation mask source is not bound to a real mask asset";
        return false;
      }

      const std::string imagePath = ResolveMetrologyImagePathLocal(context);
      if (imagePath.empty()) {
        m.height_peak_analysis_status = "METROLOGY_IMAGE_MISSING";
        m.height_peak_analysis_reason =
            "current Manual Review item has no image path";
        return false;
      }

      const cv::Mat source = cv::imread(imagePath, cv::IMREAD_UNCHANGED);
      if (source.empty()) {
        m.height_peak_analysis_status = "METROLOGY_IMAGE_LOAD_FAIL";
        m.height_peak_analysis_reason =
            "failed to load current image: " + imagePath;
        return false;
      }

      cv::Mat scalar;
      if (source.channels() == 1) {
        scalar = source;
      } else if (m.surface_z_channel > 0 &&
                 m.surface_z_channel <= source.channels()) {
        std::vector<cv::Mat> channels;
        cv::split(source, channels);
        scalar = channels[static_cast<std::size_t>(m.surface_z_channel - 1)];
      } else if (source.channels() == 3) {
        cv::cvtColor(source, scalar, cv::COLOR_BGR2GRAY);
      } else if (source.channels() == 4) {
        cv::cvtColor(source, scalar, cv::COLOR_BGRA2GRAY);
      } else {
        m.height_peak_analysis_status = "METROLOGY_IMAGE_CHANNEL_UNSUPPORTED";
        m.height_peak_analysis_reason = "unsupported image channel count";
        return false;
      }

      cv::Mat sourceValues;
      scalar.convertTo(sourceValues, CV_64F);
      std::string gaugeLineSourceRef;
      std::string gaugeLineReason;
      if (BuildMetrologyGaugeLineSurfaceFieldLocal(
              context, parserDebugBridge, sourceValues, unit, field,
              gaugeLineSourceRef, gaugeLineReason)) {
        m.height_peak_source_ref = gaugeLineSourceRef + "; image=" + imagePath;
      } else {
        if (MetrologyRequiresRuntimeGaugeLineLocal(context)) {
          m.height_peak_analysis_status =
              "METROLOGY_RUNTIME_GAUGE_LINE_MISSING";
          m.height_peak_analysis_reason = gaugeLineReason;
          m.height_peak_source_ref = "runtime:gauge_line:unavailable; reason=" +
                                     gaugeLineReason;
          return false;
        }
        const int targetWidth = std::min(requestedWidth, source.cols);
        const int targetHeight = std::min(requestedHeight, source.rows);

        cv::Mat resized;
        cv::resize(scalar, resized, cv::Size(targetWidth, targetHeight), 0.0,
                   0.0, cv::INTER_AREA);
        cv::Mat values;
        resized.convertTo(values, CV_64F);

        const int gridWidth = (targetWidth + stride - 1) / stride;
        const int gridHeight = (targetHeight + stride - 1) / stride;
        unit.x_scale_per_pixel *= static_cast<double>(source.cols) /
                                  static_cast<double>(targetWidth) * stride;
        unit.y_scale_per_pixel *= static_cast<double>(source.rows) /
                                  static_cast<double>(targetHeight) * stride;
        field = CxSurfaceField(gridWidth, gridHeight, unit);
        for (int y = 0; y < gridHeight; ++y) {
          const int sy = std::min(y * stride, targetHeight - 1);
          for (int x = 0; x < gridWidth; ++x) {
            const int sx = std::min(x * stride, targetWidth - 1);
            field.setAt(x, y,
                        values.at<double>(sy, sx) * unit.z_scale_per_pixel);
          }
        }
        m.height_peak_source_ref = "image_surface:" + imagePath +
                                   "; gauge_line_reason=" + gaugeLineReason;
      }
    }

    if (m.enable_plane_correction) {
      const PlaneLevelMethod method = static_cast<PlaneLevelMethod>(
          std::max(0, std::min(2, m.plane_method)));
      const CxPlaneCoeffs plane = fitPlane(field, method);
      subtractPlaneInPlace(field, plane);
    }

    if (m.enable_gaussian_z && m.gaussian_z_sigma_permille > 0) {
      const double sigma = m.gaussian_z_sigma_permille / 1000.0;
      std::mt19937 generator(
          static_cast<unsigned int>(std::max(0, m.gaussian_seed)));
      std::normal_distribution<double> perturbation(0.0, sigma);
      for (int y = 0; y < field.yres(); ++y) {
        for (int x = 0; x < field.xres(); ++x)
          field.setAt(x, y, field.at(x, y) + perturbation(generator));
      }
    }

    m.height_peak_stats =
        computeSurfaceBasicStats(field, std::max(8, m.histogram_bins));
    const CxHeightDistribution &distribution =
        m.height_peak_stats.height_distribution_primary;
    const double maxAdf = distribution.adf.empty()
                              ? 0.0
                              : *std::max_element(distribution.adf.begin(),
                                                  distribution.adf.end());

    CxHeightPeakOptions options;
    options.max_peaks = std::max(1, m.peak_max_count);
    options.min_prominence =
        maxAdf * std::max(0, m.peak_min_prominence_permille) / 1000.0;
    options.min_distance_bins = std::max(0, m.peak_min_distance_bins);
    options.invert = m.peak_invert;
    options.order = m.peak_order == 1 ? CxHeightPeakOrder::Prominence
                                      : CxHeightPeakOrder::Position;
    options.background = m.peak_background == 0
                             ? CxHeightPeakBackground::Zero
                             : CxHeightPeakBackground::BilateralMinimum;
    m.height_peaks = findHeightDistributionPeaks(distribution, options);

    m.height_peak_grid_width = field.xres();
    m.height_peak_grid_height = field.yres();
    m.height_peak_sample_count = field.valueCount();
    ++m.height_peak_analysis_revision;
    m.height_peak_analysis_ready = true;
    m.height_peak_analysis_status = "METROLOGY_HEIGHT_PEAKS_READY";
    m.height_peak_analysis_reason =
        "height distribution and peak facts computed; human review required";
    CXLOG_INFO("KeyParameterControls", "metrology_height_peaks", "ready",
               "source=" + m.height_peak_source_ref +
                   " samples=" + std::to_string(m.height_peak_sample_count) +
                   " bins=" + std::to_string(m.histogram_bins) +
                   " peaks=" + std::to_string(m.height_peaks.size()));
    return true;
  } catch (const std::exception &e) {
    m.height_peak_analysis_status = "METROLOGY_HEIGHT_PEAKS_FAIL";
    m.height_peak_analysis_reason = e.what();
    CXLOG_INFO("KeyParameterControls", "metrology_height_peaks", "failed",
               m.height_peak_analysis_reason);
    return false;
  }
}

static void DrawMetrologyHeightPeakPlotLocal(const ManualMetrologyUiState &m) {
  const auto &distribution = m.height_peak_stats.height_distribution_primary;
  if (distribution.bin_centers.size() < 2 ||
      distribution.adf.size() != distribution.bin_centers.size()) {
    ImGui::TextDisabled("No height distribution available.");
    return;
  }

  const ImVec2 size(std::max(280.0f, ImGui::GetContentRegionAvail().x), 230.0f);
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  ImGui::InvisibleButton("height_distribution_peak_plot", size);
  ImDrawList *draw = ImGui::GetWindowDrawList();
  draw->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y),
                      IM_COL32(20, 24, 29, 255));
  draw->AddRect(origin, ImVec2(origin.x + size.x, origin.y + size.y),
                IM_COL32(105, 115, 125, 255));

  const float left = origin.x + 48.0f;
  const float right = origin.x + size.x - 12.0f;
  const float top = origin.y + 12.0f;
  const float bottom = origin.y + size.y - 30.0f;
  for (int i = 1; i < 4; ++i) {
    const float y = top + (bottom - top) * i / 4.0f;
    draw->AddLine(ImVec2(left, y), ImVec2(right, y), IM_COL32(65, 72, 80, 180));
  }

  const double xMin = distribution.bin_centers.front();
  const double xMax = distribution.bin_centers.back();
  const double rawMax =
      *std::max_element(distribution.adf.begin(), distribution.adf.end());
  auto displayValue = [&m](double value) {
    return m.histogram_log_scale ? std::log1p(value * 1000.0) : value;
  };
  const double yMax = std::max(1.0e-12, displayValue(rawMax));
  auto mapX = [&](double x) {
    const double t = xMax > xMin ? (x - xMin) / (xMax - xMin) : 0.0;
    return left +
           static_cast<float>(std::max(0.0, std::min(1.0, t))) * (right - left);
  };
  auto mapY = [&](double y) {
    const double t = displayValue(y) / yMax;
    return bottom -
           static_cast<float>(std::max(0.0, std::min(1.0, t))) * (bottom - top);
  };

  std::vector<ImVec2> points;
  points.reserve(distribution.adf.size());
  for (std::size_t i = 0; i < distribution.adf.size(); ++i)
    points.emplace_back(mapX(distribution.bin_centers[i]),
                        mapY(distribution.adf[i]));
  draw->AddPolyline(points.data(), static_cast<int>(points.size()),
                    IM_COL32(225, 230, 235, 255), ImDrawFlags_None, 1.5f);

  for (std::size_t i = 0; i < m.height_peaks.size(); ++i) {
    const auto &peak = m.height_peaks[i];
    const float x = mapX(peak.position);
    const float y = mapY(peak.curve_value);
    const ImU32 color = m.peak_invert ? IM_COL32(70, 210, 255, 235)
                                      : IM_COL32(255, 95, 175, 235);
    draw->AddLine(ImVec2(x, top), ImVec2(x, bottom), color, 1.0f);
    draw->AddCircleFilled(ImVec2(x, y), 3.5f, color);
    const std::string label = std::to_string(i + 1);
    draw->AddText(ImVec2(x + 3.0f, top + 2.0f), color, label.c_str());
  }

  draw->AddText(ImVec2(left, bottom + 6.0f), IM_COL32(180, 188, 198, 255),
                ("z " + std::to_string(xMin)).c_str());
  const std::string maxLabel = std::to_string(xMax);
  const ImVec2 maxLabelSize = ImGui::CalcTextSize(maxLabel.c_str());
  draw->AddText(ImVec2(right - maxLabelSize.x, bottom + 6.0f),
                IM_COL32(180, 188, 198, 255), maxLabel.c_str());
  draw->AddText(ImVec2(origin.x + 4.0f, top), IM_COL32(180, 188, 198, 255),
                "ADF");

  if (ImGui::IsItemHovered()) {
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const cxvision::metrology_analytics::CxHeightPeak *nearest = nullptr;
    float nearestDistance = 8.0f;
    for (const auto &peak : m.height_peaks) {
      const float distance = std::abs(mouse.x - mapX(peak.position));
      if (distance < nearestDistance) {
        nearestDistance = distance;
        nearest = &peak;
      }
    }
    if (nearest != nullptr) {
      ImGui::BeginTooltip();
      ImGui::Text("z %.8g", nearest->position);
      ImGui::Text("height %.8g", nearest->curve_value);
      ImGui::Text("prominence %.8g", nearest->prominence);
      ImGui::Text("area %.8g", nearest->area);
      ImGui::Text("width %.8g", nearest->width);
      ImGui::EndTooltip();
    }
  }
}

static void
DrawMetrologyHeightPeakResultsLocal(const ManualMetrologyUiState &m) {
  ImGui::Text("status: %s", m.height_peak_analysis_status.c_str());
  ImGui::TextWrapped("reason: %s", m.height_peak_analysis_reason.c_str());
  if (!m.height_peak_analysis_ready)
    return;

  ImGui::TextWrapped("source: %s", m.height_peak_source_ref.c_str());
  ImGui::Text("grid %d x %d | samples %d | peaks %d", m.height_peak_grid_width,
              m.height_peak_grid_height, m.height_peak_sample_count,
              static_cast<int>(m.height_peaks.size()));
  ImGui::Text("min/max %.8g / %.8g | mean %.8g | RMS %.8g | Ra %.8g",
              m.height_peak_stats.min, m.height_peak_stats.max,
              m.height_peak_stats.mean, m.height_peak_stats.rms,
              m.height_peak_stats.ra);
  ImGui::Text("skewness %.6g | kurtosis(excess) %.6g",
              m.height_peak_stats.skewness,
              m.height_peak_stats.kurtosis_excess);

  DrawMetrologyHeightPeakPlotLocal(m);
  if (m.histogram_mode != 0) {
    const auto &bcdf = m.height_peak_stats.height_distribution_primary.bcdf;
    if (!bcdf.empty()) {
      std::vector<float> curve;
      curve.reserve(bcdf.size());
      for (double value : bcdf)
        curve.push_back(static_cast<float>(value));
      ImGui::PlotLines("Bearing curve (BCDF)", curve.data(),
                       static_cast<int>(curve.size()), 0, nullptr, 0.0f, 1.0f,
                       ImVec2(-1.0f, 90.0f));
    }
  }

  if (ImGui::BeginTable("metrology_height_peak_table", 7,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_SizingStretchProp |
                            ImGuiTableFlags_ScrollY,
                        ImVec2(-1.0f, 230.0f))) {
    ImGui::TableSetupColumn("#");
    ImGui::TableSetupColumn("z");
    ImGui::TableSetupColumn("height");
    ImGui::TableSetupColumn("area");
    ImGui::TableSetupColumn("width");
    ImGui::TableSetupColumn("prominence");
    ImGui::TableSetupColumn("sub-bin");
    ImGui::TableHeadersRow();
    for (std::size_t i = 0; i < m.height_peaks.size(); ++i) {
      const auto &peak = m.height_peaks[i];
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::Text("%d", static_cast<int>(i + 1));
      ImGui::TableSetColumnIndex(1);
      ImGui::Text("%.8g", peak.position);
      ImGui::TableSetColumnIndex(2);
      ImGui::Text("%.8g", peak.curve_value);
      ImGui::TableSetColumnIndex(3);
      ImGui::Text("%.8g", peak.area);
      ImGui::TableSetColumnIndex(4);
      ImGui::Text("%.8g", peak.width);
      ImGui::TableSetColumnIndex(5);
      ImGui::Text("%.8g", peak.prominence);
      ImGui::TableSetColumnIndex(6);
      ImGui::Text("%+.4f", peak.sub_bin_offset);
    }
    ImGui::EndTable();
  }
}

static std::string FormatMetrologyAxisPixelLabelLocal(double value) {
  char buffer[64] = {};
  if (std::abs(value) < 10.0 && std::abs(value - std::round(value)) > 0.05)
    std::snprintf(buffer, sizeof(buffer), "%.1f px", value);
  else
    std::snprintf(buffer, sizeof(buffer), "%.0f px", value);
  return std::string(buffer);
}

static double MetrologySampleIndexToAxisPxLocal(int sampleIndex,
                                                int sampleCount,
                                                double axisLengthPx) {
  if (sampleCount < 2 || axisLengthPx <= 0.0)
    return 0.0;
  const int clamped = std::max(0, std::min(sampleCount - 1, sampleIndex));
  return static_cast<double>(clamped) * axisLengthPx /
         static_cast<double>(sampleCount - 1);
}

static std::string FormatMetrologyYAxisValueLocal(float value) {
  char buffer[48] = {};
  const float magnitude = std::abs(value);
  if (magnitude >= 100.0f)
    std::snprintf(buffer, sizeof(buffer), "%.0f", value);
  else if (magnitude >= 10.0f)
    std::snprintf(buffer, sizeof(buffer), "%.1f", value);
  else if (magnitude >= 0.1f)
    std::snprintf(buffer, sizeof(buffer), "%.2f", value);
  else
    std::snprintf(buffer, sizeof(buffer), "%.4f", value);
  return std::string(buffer);
}

static float NiceMetrologyAxisStepLocal(float desiredStep) {
  if (!std::isfinite(desiredStep) || desiredStep <= 0.0f)
    return 1.0f;
  const float scale = std::pow(10.0f, std::floor(std::log10(desiredStep)));
  const float normalized = desiredStep / scale;
  const float rounded = normalized <= 1.0f ? 1.0f :
                        normalized <= 2.0f ? 2.0f :
                        normalized <= 5.0f ? 5.0f : 10.0f;
  return rounded * scale;
}

static void DrawMetrologyPreviewChartLocal(
    const char *id, const char *title, const std::vector<float> &source,
    const std::vector<float> &model, int rangeStartPermille,
    int rangeEndPermille, const char *sourceLabel, const char *modelLabel,
    const std::vector<MetrologyConclusionMarkerLocal> *conclusionMarkers,
    ManualMetrologyUiState *cursorState, double axisLengthPx = 0.0) {
  const ImVec2 size(std::max(320.0f, ImGui::GetContentRegionAvail().x), 210.0f);
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  ImGui::InvisibleButton(id, size);
  const bool chartHovered = ImGui::IsItemHovered();
  const bool chartActive = ImGui::IsItemActive();
  ImDrawList *draw = ImGui::GetWindowDrawList();
  const ImVec2 end(origin.x + size.x, origin.y + size.y);
  draw->AddRectFilled(origin, end, IM_COL32(20, 24, 29, 255));
  draw->AddRect(origin, end, IM_COL32(105, 115, 125, 255));

  // Reserve enough width for signed, non-integer response values.  This is
  // shared by FindLine and FastMatch because both use the same Gauge profile
  // chart, so the vertical scale is never an implicit pixel-only value.
  const float left = origin.x + 72.0f;
  const float right = origin.x + size.x - 14.0f;
  const float top = origin.y + 30.0f;
  const float bottom = origin.y + size.y - 28.0f;

  const float rangeLeft =
      left + (right - left) * std::max(0, std::min(1000, rangeStartPermille)) /
                 1000.0f;
  const float rangeRight =
      left +
      (right - left) * std::max(0, std::min(1000, rangeEndPermille)) / 1000.0f;
  draw->AddRectFilled(ImVec2(rangeLeft, top), ImVec2(rangeRight, bottom),
                      IM_COL32(72, 104, 132, 36));

  float yMin = 0.0f; // Keep a visible zero reference for intensity/response.
  float yMax = 0.0f;
  for (float value : source) {
    yMin = std::min(yMin, value);
    yMax = std::max(yMax, value);
  }
  for (float value : model) {
    yMin = std::min(yMin, value);
    yMax = std::max(yMax, value);
  }
  const float rawSpan = yMax - yMin;
  const float padding = rawSpan > 1.0e-6f
      ? rawSpan * 0.05f
      : std::max(1.0f, std::abs(yMax) * 0.10f);
  yMin -= padding;
  yMax += padding;
  const float yStep = NiceMetrologyAxisStepLocal((yMax - yMin) / 4.0f);
  yMin = std::floor(yMin / yStep) * yStep;
  yMax = std::ceil(yMax / yStep) * yStep;
  if (yMax - yMin < yStep) {
    yMin -= yStep * 2.0f;
    yMax += yStep * 2.0f;
  }
  const float span = std::max(1.0e-6f, yMax - yMin);
  constexpr int yTickCount = 4;
  for (int i = 0; i <= yTickCount; ++i) {
    const float value = yMax - span * static_cast<float>(i) /
                                static_cast<float>(yTickCount);
    const float y = top + (bottom - top) * static_cast<float>(i) /
                              static_cast<float>(yTickCount);
    const bool zeroReference = std::abs(value) <= yStep * 0.001f;
    const ImU32 gridColor = zeroReference ? IM_COL32(116, 130, 145, 220)
                                           : IM_COL32(62, 70, 79, 180);
    draw->AddLine(ImVec2(left, y), ImVec2(right, y), gridColor,
                  zeroReference ? 1.4f : 1.0f);
    draw->AddLine(ImVec2(left - 4.0f, y), ImVec2(left, y),
                  IM_COL32(160, 170, 181, 255), 1.0f);
    const std::string label = FormatMetrologyYAxisValueLocal(value);
    const ImVec2 labelSize = ImGui::CalcTextSize(label.c_str());
    draw->AddText(ImVec2(left - 7.0f - labelSize.x, y - labelSize.y * 0.5f),
                  zeroReference ? IM_COL32(205, 214, 223, 255)
                                : IM_COL32(160, 170, 181, 255),
                  label.c_str());
  }
  draw->AddLine(ImVec2(left, top), ImVec2(left, bottom),
                IM_COL32(160, 170, 181, 220), 1.0f);
  draw->AddText(ImVec2(origin.x + 7.0f, top - 15.0f),
                IM_COL32(160, 170, 181, 255), "Y");
  auto valueToScreenY = [&](float value) -> float {
    const float ty = (value - yMin) / span;
    return bottom - ty * (bottom - top);
  };
  auto sampleToScreenX = [&](int sampleIndex, int sampleCount) -> float {
    const int clamped = std::max(0, std::min(sampleCount - 1, sampleIndex));
    const float tx = static_cast<float>(clamped) /
                     static_cast<float>(std::max(1, sampleCount - 1));
    return left + tx * (right - left);
  };

  const int sourceCount = static_cast<int>(source.size());
  const double effectiveAxisLengthPx =
      axisLengthPx > 0.0
          ? axisLengthPx
          : static_cast<double>(std::max(0, sourceCount - 1));
  auto axisPxToScreenX = [&](double axisPx) -> float {
    if (effectiveAxisLengthPx <= 0.0)
      return left;
    const double clamped = std::max(0.0, std::min(effectiveAxisLengthPx, axisPx));
    const float tx = static_cast<float>(clamped / effectiveAxisLengthPx);
    return left + tx * (right - left);
  };
  auto drawCurve = [&](const std::vector<float> &values, ImU32 color,
                       float thickness) {
    if (values.size() < 2)
      return;
    std::vector<ImVec2> points;
    points.reserve(values.size());
    for (std::size_t i = 0; i < values.size(); ++i) {
      const float tx =
          static_cast<float>(i) / static_cast<float>(values.size() - 1);
      points.emplace_back(left + tx * (right - left), valueToScreenY(values[i]));
    }
    draw->AddPolyline(points.data(), static_cast<int>(points.size()), color,
                      ImDrawFlags_None, thickness);
  };

  if (cursorState != nullptr) {
    cursorState->profile_cursor_sample_count = sourceCount;
    std::string sourceKey = std::string(id != nullptr ? id : "") +
                            "|count=" + std::to_string(sourceCount) +
                            "|axis_px=" + std::to_string(effectiveAxisLengthPx) +
                            "|range=" + std::to_string(rangeStartPermille) +
                            ":" + std::to_string(rangeEndPermille) +
                            "|y=" + std::to_string(yMin) + ":" +
                            std::to_string(yMax);
    if (!source.empty()) {
      sourceKey += "|first=" + std::to_string(source.front()) +
                   "|last=" + std::to_string(source.back());
    }
    if (conclusionMarkers != nullptr && !conclusionMarkers->empty()) {
      sourceKey += "|result_px=" + std::to_string(conclusionMarkers->front().axis_px) +
                   "/" + std::to_string(conclusionMarkers->size());
    } else {
      sourceKey += "|result=none";
    }
    if (cursorState->profile_cursor_gauge_line_num != cursorState->gauge_line_num ||
        cursorState->profile_cursor_source_key != sourceKey) {
      cursorState->profile_cursor_gauge_line_num = cursorState->gauge_line_num;
      cursorState->profile_cursor_source_key = sourceKey;
      cursorState->profile_cursor_user_dragged = false;
      cursorState->profile_cursor_sample_index = -1;
    }
    if (sourceCount >= 2) {
      if (!cursorState->profile_cursor_user_dragged) {
        if (conclusionMarkers != nullptr && !conclusionMarkers->empty()) {
          cursorState->profile_cursor_sample_index = std::max(
              0, std::min(sourceCount - 1, conclusionMarkers->front().sample_index));
          cursorState->profile_cursor_visible = true;
        } else {
          cursorState->profile_cursor_sample_index = -1;
          cursorState->profile_cursor_visible = false;
        }
      }
      if (cursorState->profile_cursor_sample_index < 0 ||
          cursorState->profile_cursor_sample_index >= sourceCount) {
        cursorState->profile_cursor_sample_index = std::max(
            0, std::min(sourceCount - 1,
                        static_cast<int>(std::lround(
                            static_cast<double>(cursorState->profile_cursor_permille) *
                            static_cast<double>(sourceCount - 1) / 1000.0))));
      }
      if ((chartHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) ||
          (chartActive && ImGui::IsMouseDown(ImGuiMouseButton_Left))) {
        const float mouseX = std::max(left, std::min(right, ImGui::GetIO().MousePos.x));
        const float tx = (mouseX - left) / std::max(1.0f, right - left);
        cursorState->profile_cursor_sample_index = std::max(
            0, std::min(sourceCount - 1,
                        static_cast<int>(std::lround(
                            static_cast<double>(tx) *
                            static_cast<double>(sourceCount - 1)))));
        cursorState->profile_cursor_visible = true;
        cursorState->profile_cursor_user_dragged = true;
      }
      cursorState->profile_cursor_sample_index = std::max(
          0, std::min(sourceCount - 1, cursorState->profile_cursor_sample_index));
      cursorState->profile_cursor_permille = static_cast<int>(std::lround(
          static_cast<double>(cursorState->profile_cursor_sample_index) * 1000.0 /
          static_cast<double>(sourceCount - 1)));
    } else {
      cursorState->profile_cursor_sample_index = 0;
      cursorState->profile_cursor_permille = 0;
      cursorState->profile_cursor_visible = false;
      cursorState->profile_cursor_user_dragged = false;
    }
  }

  drawCurve(source, IM_COL32(225, 230, 235, 255), 1.5f);
  drawCurve(model, IM_COL32(69, 205, 150, 255), 2.0f);

  if (conclusionMarkers != nullptr && sourceCount >= 2) {
    for (std::size_t i = 0; i < conclusionMarkers->size(); ++i) {
      const MetrologyConclusionMarkerLocal &marker = (*conclusionMarkers)[i];
      const int sampleIndex = std::max(0, std::min(sourceCount - 1, marker.sample_index));
      const float resultX = axisPxToScreenX(marker.axis_px);
      const float resultY = valueToScreenY(source[static_cast<std::size_t>(sampleIndex)]);
      draw->AddLine(ImVec2(resultX, top), ImVec2(resultX, bottom),
                    IM_COL32(255, 128, 48, 255), 2.5f);
      draw->AddCircleFilled(ImVec2(resultX, resultY), 4.5f,
                            IM_COL32(255, 128, 48, 255), 16);
      if (i < 3) {
        const std::string label = "result " +
                                  FormatMetrologyAxisPixelLabelLocal(marker.axis_px);
        draw->AddText(ImVec2(std::min(resultX + 6.0f, right - 118.0f),
                             top + 20.0f + static_cast<float>(i) * 14.0f),
                      IM_COL32(255, 178, 118, 255), label.c_str());
      }
    }
  }

  if (cursorState != nullptr && cursorState->profile_cursor_visible &&
      sourceCount >= 2) {
    const int cursorIndex = std::max(
        0, std::min(sourceCount - 1, cursorState->profile_cursor_sample_index));
    double cursorAxisPx = MetrologySampleIndexToAxisPxLocal(
        cursorIndex, sourceCount, effectiveAxisLengthPx);
    if (!cursorState->profile_cursor_user_dragged && conclusionMarkers != nullptr &&
        !conclusionMarkers->empty()) {
      cursorAxisPx = conclusionMarkers->front().axis_px;
    }
    const float cursorX = axisPxToScreenX(cursorAxisPx);
    const float cursorY = valueToScreenY(source[static_cast<std::size_t>(cursorIndex)]);
    draw->AddLine(ImVec2(cursorX, top), ImVec2(cursorX, bottom),
                  IM_COL32(255, 210, 64, 220), 1.5f);
    draw->AddCircleFilled(ImVec2(cursorX, cursorY), 3.8f,
                          IM_COL32(255, 210, 64, 255), 16);
    const std::string cursorPx = FormatMetrologyAxisPixelLabelLocal(cursorAxisPx);
    const std::string cursorLabel = cursorState->profile_cursor_user_dragged
                                        ? "cursor " + cursorPx
                                        : "cursor=result " + cursorPx;
    draw->AddText(ImVec2(std::min(cursorX + 6.0f, right - 118.0f),
                         std::max(top + 4.0f, cursorY - 18.0f)),
                  IM_COL32(255, 226, 110, 255), cursorLabel.c_str());
  }

  draw->AddText(ImVec2(left, origin.y + 8.0f), IM_COL32(225, 230, 235, 255),
                title);
  const ImVec2 sourceLabelSize = ImGui::CalcTextSize(sourceLabel);
  const ImVec2 modelLabelSize = ImGui::CalcTextSize(modelLabel);
  const ImVec2 resultLabelSize = ImGui::CalcTextSize("result");
  const float legendGap = 14.0f;
  const float sourceLegendWidth = 30.0f + sourceLabelSize.x;
  const float modelLegendWidth = 30.0f + modelLabelSize.x;
  const float resultLegendWidth = 30.0f + resultLabelSize.x;
  const bool showResultLegend =
      conclusionMarkers != nullptr && !conclusionMarkers->empty();
  const float totalLegendWidth =
      sourceLegendWidth + legendGap + modelLegendWidth +
      (showResultLegend ? legendGap + resultLegendWidth : 0.0f);
  float legendX = std::max(left, right - totalLegendWidth);
  const float titleRight = left + ImGui::CalcTextSize(title).x + 16.0f;
  const bool compactLegend = legendX < titleRight;
  auto drawLegendItem = [&](const char *label, ImU32 color, float labelWidth) {
    draw->AddLine(ImVec2(legendX, origin.y + 16.0f),
                  ImVec2(legendX + 24.0f, origin.y + 16.0f), color, 2.0f);
    draw->AddText(ImVec2(legendX + 30.0f, origin.y + 8.0f),
                  IM_COL32(190, 198, 207, 255), label);
    legendX += 30.0f + labelWidth + legendGap;
  };
  if (!compactLegend) {
    drawLegendItem(sourceLabel, IM_COL32(225, 230, 235, 255),
                   sourceLabelSize.x);
    drawLegendItem(modelLabel, IM_COL32(69, 205, 150, 255), modelLabelSize.x);
  } else {
    legendX = std::max(left, right - resultLegendWidth);
  }
  if (showResultLegend) {
    drawLegendItem("result", IM_COL32(255, 128, 48, 255),
                   resultLabelSize.x);
  }

  constexpr int tickCount = 5;
  for (int i = 0; i <= tickCount; ++i) {
    const float tx = static_cast<float>(i) / static_cast<float>(tickCount);
    const float tickX = left + tx * (right - left);
    draw->AddLine(ImVec2(tickX, bottom), ImVec2(tickX, bottom + 4.0f),
                  IM_COL32(160, 170, 181, 255), 1.0f);
    const std::string tickLabel = FormatMetrologyAxisPixelLabelLocal(
        effectiveAxisLengthPx * static_cast<double>(i) /
        static_cast<double>(tickCount));
    const ImVec2 tickLabelSize = ImGui::CalcTextSize(tickLabel.c_str());
    const float labelX = std::max(
        left, std::min(right - tickLabelSize.x, tickX - tickLabelSize.x * 0.5f));
    draw->AddText(ImVec2(labelX, bottom + 6.0f),
                  IM_COL32(160, 170, 181, 255), tickLabel.c_str());
  }
}


static bool HasMetrologySelectedGaugeLineProfileLocal(const ManualMetrologyUiState &m) {
  const std::string &source = m.height_peak_source_ref;
  return source.rfind("runtime:gauge_line:", 0) == 0 ||
         source.rfind("gauge_line:", 0) == 0;
}

static bool IsMetrologyRuntimeGaugeLineSourceLocal(const std::string &source) {
  return source.rfind("runtime:gauge_line:", 0) == 0;
}


static std::vector<float>
BuildMetrologyPreviewSourceLocal(const ManualMetrologyUiState &m,
                                 int sourceMode) {
  const auto &distribution = m.height_peak_stats.height_distribution_primary;
  if (!m.height_peak_analysis_ready ||
      !HasMetrologySelectedGaugeLineProfileLocal(m))
    return {};

  const std::vector<double> &measured =
      sourceMode == 1 ? distribution.bcdf : distribution.adf;
  if (measured.size() < 2)
    return {};

  std::vector<float> values(measured.size(), 0.0f);
  const double maxValue =
      std::max(1.0e-12, *std::max_element(measured.begin(), measured.end()));
  for (std::size_t i = 0; i < measured.size(); ++i)
    values[i] = static_cast<float>(measured[i] / maxValue);
  return values;
}

static std::vector<float> BuildMetrologyLiveGaugeLineProfileLocal(
    const ManualTestContext &context, const ParserDebugBridge *parserDebugBridge,
    std::string &sourceRef, double *profileLengthPx = nullptr) {
  using namespace cxvision::metrology_analytics;
  sourceRef.clear();
  if (profileLengthPx != nullptr)
    *profileLengthPx = 0.0;

  const std::string imagePath = ResolveMetrologyImagePathLocal(context);
  if (imagePath.empty()) {
    sourceRef = "runtime:gauge_line:unavailable; reason=image path is empty";
    return {};
  }

  const cv::Mat source = cv::imread(imagePath, cv::IMREAD_UNCHANGED);
  if (source.empty()) {
    sourceRef = "runtime:gauge_line:unavailable; reason=image load failed; image=" +
                imagePath;
    return {};
  }

  const ManualMetrologyUiState &m = context.metrology_ui;
  cv::Mat scalar;
  if (source.channels() == 1) {
    scalar = source;
  } else if (m.surface_z_channel > 0 &&
             m.surface_z_channel <= source.channels()) {
    std::vector<cv::Mat> channels;
    cv::split(source, channels);
    scalar = channels[static_cast<std::size_t>(m.surface_z_channel - 1)];
  } else if (source.channels() == 3) {
    cv::cvtColor(source, scalar, cv::COLOR_BGR2GRAY);
  } else if (source.channels() == 4) {
    cv::cvtColor(source, scalar, cv::COLOR_BGRA2GRAY);
  } else {
    sourceRef =
        "runtime:gauge_line:unavailable; reason=unsupported image channel count";
    return {};
  }

  cv::Mat values;
  scalar.convertTo(values, CV_64F);
  CxPhysUnit unit;
  unit.x_unit = MetrologyLengthUnitLocal(m.x_unit);
  unit.y_unit = MetrologyLengthUnitLocal(m.y_unit);
  unit.z_unit = MetrologyLengthUnitLocal(m.z_unit);
  unit.x_scale_per_pixel = std::max(1, m.x_scale_permille) / 1000.0;
  unit.y_scale_per_pixel = std::max(1, m.y_scale_permille) / 1000.0;
  unit.z_scale_per_pixel = std::max(1, m.z_scale_permille) / 1000.0;

  CxSurfaceField field;
  std::string reason;
  if (!BuildMetrologyGaugeLineSurfaceFieldLocal(
          context, parserDebugBridge, values, unit, field, sourceRef, reason,
          profileLengthPx)) {
    sourceRef = "runtime:gauge_line:unavailable; reason=" + reason;
    return {};
  }

  const float *raw = field.rawData();
  if (raw == nullptr || field.valueCount() < 2) {
    sourceRef = "runtime:gauge_line:unavailable; reason=sampled profile is empty";
    return {};
  }

  std::vector<float> profile(raw, raw + field.valueCount());
  const auto minmax = std::minmax_element(profile.begin(), profile.end());
  const float span = std::max(1.0e-6f, *minmax.second - *minmax.first);
  for (float &value : profile)
    value = (value - *minmax.first) / span;

  if (profileLengthPx != nullptr && *profileLengthPx > 0.0)
    sourceRef += "; length_px=" + FormatMetrologyAxisPixelLabelLocal(*profileLengthPx);
  sourceRef += "; image=" + imagePath;
  return profile;
}



static void ResolveMetrologyPreviewRangeLocal(std::size_t valueCount,
                                              bool fullRange,
                                              int startPermille,
                                              int endPermille,
                                              std::size_t &startIndex,
                                              std::size_t &endIndex) {
  if (valueCount < 2) {
    startIndex = 0;
    endIndex = 0;
    return;
  }
  const int startP = fullRange ? 0 : std::max(0, std::min(999, startPermille));
  const int endP = fullRange ? 1000 : std::max(1, std::min(1000, endPermille));
  const std::size_t last = valueCount - 1;
  startIndex = static_cast<std::size_t>(
      std::floor(static_cast<double>(last) * startP / 1000.0));
  endIndex = static_cast<std::size_t>(
      std::ceil(static_cast<double>(last) * endP / 1000.0));
  endIndex = std::min(last, std::max(startIndex + 1, endIndex));
}

static bool SolveMetrologyQuadraticFitLocal(double matrix[3][4],
                                            double coeffs[3]) {
  for (int col = 0; col < 3; ++col) {
    int pivot = col;
    for (int row = col + 1; row < 3; ++row) {
      if (std::abs(matrix[row][col]) > std::abs(matrix[pivot][col]))
        pivot = row;
    }
    if (std::abs(matrix[pivot][col]) < 1.0e-12)
      return false;
    if (pivot != col) {
      for (int k = col; k < 4; ++k)
        std::swap(matrix[pivot][k], matrix[col][k]);
    }
    const double divisor = matrix[col][col];
    for (int k = col; k < 4; ++k)
      matrix[col][k] /= divisor;
    for (int row = 0; row < 3; ++row) {
      if (row == col)
        continue;
      const double factor = matrix[row][col];
      for (int k = col; k < 4; ++k)
        matrix[row][k] -= factor * matrix[col][k];
    }
  }
  coeffs[0] = matrix[0][3];
  coeffs[1] = matrix[1][3];
  coeffs[2] = matrix[2][3];
  return true;
}

struct MetrologyPreviewModelLocal {
  std::vector<float> values;
  std::string status;
  float primary = 0.0f;
  float secondary = 0.0f;
};


static MetrologyPreviewModelLocal BuildMetrologyCurveFitPreviewModelLocal(
    const ManualMetrologyUiState &m, const std::vector<float> &source,
    double profileLengthPx = 0.0) {
  MetrologyPreviewModelLocal result;
  if (!m.curve_fit_auto_plot || source.size() < 3)
    return result;

  std::size_t startIndex = 0;
  std::size_t endIndex = 0;
  ResolveMetrologyPreviewRangeLocal(source.size(), m.curve_fit_full_range,
                                    m.curve_fit_range_start_permille,
                                    m.curve_fit_range_end_permille, startIndex,
                                    endIndex);
  if (endIndex <= startIndex)
    return result;

  result.values.assign(source.size(), 0.0f);
  float baseline = source[startIndex];
  float peak = source[startIndex];
  std::size_t peakIndex = startIndex;
  for (std::size_t i = startIndex; i <= endIndex; ++i) {
    baseline = std::min(baseline, source[i]);
    if (source[i] > peak) {
      peak = source[i];
      peakIndex = i;
    }
  }
  const double amplitude =
      std::max(1.0e-6, static_cast<double>(peak - baseline));

  if (m.curve_fit_function == 2) {
    double sx[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
    double sy[3] = {0.0, 0.0, 0.0};
    for (std::size_t i = startIndex; i <= endIndex; ++i) {
      const double x = static_cast<double>(i) /
                       static_cast<double>(std::max<std::size_t>(1, source.size() - 1));
      const double y = static_cast<double>(source[i]);
      double xp = 1.0;
      for (int p = 0; p < 5; ++p) {
        sx[p] += xp;
        xp *= x;
      }
      sy[0] += y;
      sy[1] += x * y;
      sy[2] += x * x * y;
    }
    double matrix[3][4] = {{sx[0], sx[1], sx[2], sy[0]},
                           {sx[1], sx[2], sx[3], sy[1]},
                           {sx[2], sx[3], sx[4], sy[2]}};
    double coeffs[3] = {0.0, 0.0, 0.0};
    if (!SolveMetrologyQuadraticFitLocal(matrix, coeffs)) {
      result.values.clear();
      return result;
    }
    for (std::size_t i = 0; i < source.size(); ++i) {
      const double x = static_cast<double>(i) /
                       static_cast<double>(std::max<std::size_t>(1, source.size() - 1));
      result.values[i] = static_cast<float>(coeffs[0] + coeffs[1] * x +
                                            coeffs[2] * x * x);
    }
    result.status =
        "PREVIEW_DERIVED_FROM_RUNTIME_PROFILE - quadratic least squares";
    result.primary = static_cast<float>(coeffs[1]);
    result.secondary = static_cast<float>(coeffs[2]);
    return result;
  }

  double weightSum = 0.0;
  double centerSum = 0.0;
  for (std::size_t i = startIndex; i <= endIndex; ++i) {
    const double x = static_cast<double>(i) /
                     static_cast<double>(std::max<std::size_t>(1, source.size() - 1));
    const double weight =
        std::max(0.0, static_cast<double>(source[i] - baseline));
    centerSum += x * weight;
    weightSum += weight;
  }
  const double peakCenter = static_cast<double>(peakIndex) /
                            static_cast<double>(std::max<std::size_t>(1, source.size() - 1));
  const double center = weightSum > 1.0e-12 ? centerSum / weightSum : peakCenter;

  double variance = 0.0;
  for (std::size_t i = startIndex; i <= endIndex; ++i) {
    const double x = static_cast<double>(i) /
                     static_cast<double>(std::max<std::size_t>(1, source.size() - 1));
    const double weight =
        std::max(0.0, static_cast<double>(source[i] - baseline));
    const double dx = x - center;
    variance += dx * dx * weight;
  }
  const double sigma =
      std::max(0.01, std::sqrt(variance / std::max(1.0e-12, weightSum)));
  for (std::size_t i = 0; i < source.size(); ++i) {
    const double x = static_cast<double>(i) /
                     static_cast<double>(std::max<std::size_t>(1, source.size() - 1));
    const double dx = (x - center) / sigma;
    const double y =
        m.curve_fit_function == 1
            ? static_cast<double>(baseline) + amplitude / (1.0 + dx * dx)
            : static_cast<double>(baseline) + amplitude * std::exp(-0.5 * dx * dx);
    result.values[i] = static_cast<float>(y);
  }
  result.status = m.curve_fit_function == 1
                      ? "PREVIEW_DERIVED_FROM_RUNTIME_PROFILE - Lorentzian estimate"
                      : "PREVIEW_DERIVED_FROM_RUNTIME_PROFILE - Gaussian estimate";
  const double pixelScale =
      profileLengthPx > 0.0
          ? profileLengthPx
          : static_cast<double>(std::max<std::size_t>(1, source.size() - 1));
  result.primary = static_cast<float>(center * pixelScale);
  result.secondary = static_cast<float>(sigma * pixelScale);
  return result;
}



static MetrologyPreviewModelLocal BuildMetrologyCriticalDimensionPreviewModelLocal(
    const ManualMetrologyUiState &m, const std::vector<float> &source) {
  MetrologyPreviewModelLocal result;
  if (!m.critical_dimension_auto_fit || source.size() < 3)
    return result;

  std::size_t startIndex = 0;
  std::size_t endIndex = 0;
  ResolveMetrologyPreviewRangeLocal(source.size(), m.critical_dimension_full_range,
                                    m.critical_dimension_range_start_permille,
                                    m.critical_dimension_range_end_permille,
                                    startIndex, endIndex);
  if (endIndex <= startIndex)
    return result;

  result.values.assign(source.size(), 0.0f);
  if (m.critical_dimension_function >= 4) {
    const bool circleUp = m.critical_dimension_function == 5;
    std::size_t centerIndex = startIndex;
    float extremum = source[startIndex];
    for (std::size_t i = startIndex; i <= endIndex; ++i) {
      if ((circleUp && source[i] > extremum) || (!circleUp && source[i] < extremum)) {
        extremum = source[i];
        centerIndex = i;
      }
    }
    const float edgeLevel = (source[startIndex] + source[endIndex]) * 0.5f;
    const float amplitude = extremum - edgeLevel;
    const double radius = static_cast<double>(
        std::max(centerIndex - startIndex, endIndex - centerIndex));
    for (std::size_t i = 0; i < source.size(); ++i) {
      if (i < startIndex || i > endIndex || radius <= 1.0) {
        result.values[i] = edgeLevel;
        continue;
      }
      const double dx = static_cast<double>(i > centerIndex ? i - centerIndex
                                                             : centerIndex - i) /
                        radius;
      const double cap = std::sqrt(std::max(0.0, 1.0 - dx * dx));
      result.values[i] = edgeLevel + amplitude * static_cast<float>(cap);
    }
    result.status = circleUp
                        ? "PREVIEW_DERIVED_FROM_RUNTIME_PROFILE - circle up model"
                        : "PREVIEW_DERIVED_FROM_RUNTIME_PROFILE - circle down model";
    result.primary = static_cast<float>(centerIndex) * 100.0f /
                     static_cast<float>(std::max<std::size_t>(1, source.size() - 1));
    result.secondary = std::abs(amplitude);
    return result;
  }

  std::size_t edgeIndex = startIndex;
  double bestScore = -DBL_MAX;
  for (std::size_t i = startIndex + 1; i <= endIndex; ++i) {
    const double delta = static_cast<double>(source[i]) - static_cast<double>(source[i - 1]);
    double score = std::abs(delta);
    if (m.critical_dimension_function == 2)
      score = delta;
    else if (m.critical_dimension_function == 3)
      score = -delta;
    else if (m.critical_dimension_function == 0)
      score += static_cast<double>(i) * 1.0e-9;
    else if (m.critical_dimension_function == 1)
      score -= static_cast<double>(i) * 1.0e-9;
    if (score > bestScore) {
      bestScore = score;
      edgeIndex = i;
    }
  }

  const std::size_t leftEnd = edgeIndex > startIndex ? edgeIndex - 1 : startIndex;
  double leftSum = 0.0;
  double rightSum = 0.0;
  int leftCount = 0;
  int rightCount = 0;
  for (std::size_t i = startIndex; i <= leftEnd; ++i) {
    leftSum += source[i];
    ++leftCount;
  }
  for (std::size_t i = edgeIndex; i <= endIndex; ++i) {
    rightSum += source[i];
    ++rightCount;
  }
  const float leftLevel = static_cast<float>(leftSum / std::max(1, leftCount));
  const float rightLevel = static_cast<float>(rightSum / std::max(1, rightCount));
  for (std::size_t i = 0; i < source.size(); ++i)
    result.values[i] = i < edgeIndex ? leftLevel : rightLevel;

  result.status = "PREVIEW_DERIVED_FROM_RUNTIME_PROFILE - edge/step model";
  result.primary = static_cast<float>(edgeIndex) * 100.0f /
                   static_cast<float>(std::max<std::size_t>(1, source.size() - 1));
  result.secondary = rightLevel - leftLevel;
  return result;
}
static std::vector<float>
BuildMetrologyRuntimeProfilePeakMarkerSourceLocal(
    const ManualMetrologyUiState &m,
    const std::vector<float> &source) {
  if (source.size() < 3)
    return {};

  const auto minmax = std::minmax_element(source.begin(), source.end());
  const float span = std::max(1.0e-6f, *minmax.second - *minmax.first);
  const float minProminence =
      span * static_cast<float>(std::max(0, m.peak_min_prominence_permille)) /
      1000.0f;

  struct PeakCandidateLocal {
    int sample_index = -1;
    float prominence = 0.0f;
  };
  std::vector<PeakCandidateLocal> peaks;
  for (std::size_t i = 1; i + 1 < source.size(); ++i) {
    const float center = m.peak_invert ? -source[i] : source[i];
    const float left = m.peak_invert ? -source[i - 1] : source[i - 1];
    const float right = m.peak_invert ? -source[i + 1] : source[i + 1];
    if (center < left || center < right)
      continue;
    const float prominence = center - std::max(left, right);
    if (prominence < minProminence)
      continue;
    peaks.push_back({static_cast<int>(i), prominence});
  }

  std::sort(peaks.begin(), peaks.end(), [](const PeakCandidateLocal &a,
                                           const PeakCandidateLocal &b) {
    if (a.prominence != b.prominence)
      return a.prominence > b.prominence;
    return a.sample_index < b.sample_index;
  });

  std::vector<float> markers(source.size(), 0.0f);
  const int minDistance = std::max(0, m.peak_min_distance_bins);
  int selected = 0;
  for (const auto &peak : peaks) {
    bool tooClose = false;
    for (std::size_t i = 0; i < markers.size(); ++i) {
      if (markers[i] <= 0.0f)
        continue;
      if (std::abs(static_cast<int>(i) - peak.sample_index) <= minDistance) {
        tooClose = true;
        break;
      }
    }
    if (tooClose)
      continue;
    markers[static_cast<std::size_t>(peak.sample_index)] = 1.0f;
    ++selected;
    if (selected >= std::max(1, m.peak_max_count))
      break;
  }
  return markers;
}

static std::vector<float>
BuildMetrologyPeakMarkerSourceLocal(const ManualMetrologyUiState &m,
                                    const std::vector<float> &source) {
  if (!m.height_peak_analysis_ready || source.empty() || m.height_peaks.empty())
    return {};

  std::vector<float> markers(source.size(), 0.0f);
  const auto &distribution = m.height_peak_stats.height_distribution_primary;
  const std::size_t binCount = distribution.adf.size();
  if (binCount < 2)
    return markers;

  for (const auto &peak : m.height_peaks) {
    const int clampedBin =
        std::max(0, std::min(peak.bin_index, static_cast<int>(binCount) - 1));
    const std::size_t markerIndex = static_cast<std::size_t>(
        std::round(static_cast<double>(clampedBin) *
                   static_cast<double>(source.size() - 1) /
                   static_cast<double>(binCount - 1)));
    markers[std::min(markerIndex, source.size() - 1)] = 1.0f;
  }
  return markers;
}

static void DrawFindPeaksPreviewLocal(
    const ManualTestContext &context, const ParserDebugBridge *parserDebugBridge,
    ManualMetrologyUiState &m) {
  std::string sourceRef;
  double profileLengthPx = 0.0;
  std::vector<float> source = BuildMetrologyLiveGaugeLineProfileLocal(
      context, parserDebugBridge, sourceRef, &profileLengthPx);
  std::vector<float> markers;
  const char *sourceLabel = "runtime Gauge Line";

  if (source.empty() && !MetrologyRequiresRuntimeGaugeLineLocal(context)) {
    source = BuildMetrologyPreviewSourceLocal(m, m.histogram_mode == 1 ? 1 : 0);
    sourceRef = m.height_peak_source_ref;
    profileLengthPx = static_cast<double>(std::max<std::size_t>(1, source.size() - 1));
    markers = BuildMetrologyPeakMarkerSourceLocal(m, source);
    sourceLabel = "height distribution";
  } else if (!source.empty() &&
             IsMetrologyRuntimeGaugeLineSourceLocal(sourceRef)) {
    markers = BuildMetrologyRuntimeProfilePeakMarkerSourceLocal(m, source);
  }
  const std::vector<MetrologyConclusionMarkerLocal> conclusionMarkers =
      BuildMetrologyConclusionMarkersLocal(
          context, parserDebugBridge, static_cast<int>(source.size()));


  ImGui::Text("Gauge Line NUM %d", m.gauge_line_num);
  if (source.empty()) {
    ImGui::TextDisabled("NO_RUNTIME_PROFILE - %s", sourceRef.c_str());
  } else {
    ImGui::TextDisabled("RUNTIME_GAUGE_LINE_PROFILE - %s", sourceRef.c_str());
  }

  DrawMetrologyPreviewChartLocal("find_peaks_parameter_preview",
                                 "automated graph peak location", source,
                                 markers, 0, 1000, sourceLabel, "peaks",
                                 IsMetrologyRuntimeGaugeLineSourceLocal(sourceRef)
                                     ? &conclusionMarkers

                                     : nullptr,
                                 &m, profileLengthPx);
  if (m.profile_cursor_sample_count > 1) {
    const double cursorPx = MetrologySampleIndexToAxisPxLocal(
        m.profile_cursor_sample_index, m.profile_cursor_sample_count,
        profileLengthPx);
    ImGui::TextDisabled("cursor %s / length %s",
                        FormatMetrologyAxisPixelLabelLocal(cursorPx).c_str(),
                        FormatMetrologyAxisPixelLabelLocal(profileLengthPx).c_str());
  }
}

static void DrawCurveFitPreviewLocal(
    const ManualTestContext &context, const ParserDebugBridge *parserDebugBridge,
    ManualMetrologyUiState &m) {
  std::string sourceRef;
  double profileLengthPx = 0.0;
  std::vector<float> source = BuildMetrologyLiveGaugeLineProfileLocal(
      context, parserDebugBridge, sourceRef, &profileLengthPx);
  if (source.empty() && !MetrologyRequiresRuntimeGaugeLineLocal(context)) {
    source = BuildMetrologyPreviewSourceLocal(m, m.curve_fit_source);
    sourceRef = m.height_peak_source_ref;
    profileLengthPx = static_cast<double>(std::max<std::size_t>(1, source.size() - 1));
  }
  const MetrologyPreviewModelLocal fit =
      BuildMetrologyCurveFitPreviewModelLocal(m, source, profileLengthPx);
  const std::vector<MetrologyConclusionMarkerLocal> conclusionMarkers =
      BuildMetrologyConclusionMarkersLocal(
          context, parserDebugBridge, static_cast<int>(source.size()));


  ImGui::Text("Gauge Line NUM %d", m.gauge_line_num);
  if (source.empty()) {
    ImGui::TextDisabled("NO_RUNTIME_PROFILE - %s", sourceRef.c_str());
  } else {
    ImGui::TextDisabled("RUNTIME_GAUGE_LINE_PROFILE - %s", sourceRef.c_str());
    if (fit.values.empty()) {
      ImGui::TextDisabled("NO_FIT_PREVIEW - auto plot is off or profile range is "
                          "insufficient");
    } else {
      ImGui::TextDisabled("%s", fit.status.c_str());
      if (m.curve_fit_function == 2) {
        ImGui::TextDisabled("fit preview slope %.3f | curve %.3f",
                            fit.primary, fit.secondary);
      } else {
        ImGui::TextDisabled("fit preview center %.1f px | width %.1f px",
                            fit.primary, fit.secondary);
      }
    }
  }

  DrawMetrologyPreviewChartLocal(
      "curve_fit_parameter_preview", "Curve fitting module", source,
      fit.values,
      m.curve_fit_full_range ? 0 : m.curve_fit_range_start_permille,
      m.curve_fit_full_range ? 1000 : m.curve_fit_range_end_permille, "source",
      "fit",
      IsMetrologyRuntimeGaugeLineSourceLocal(sourceRef) ? &conclusionMarkers

                                                       : nullptr,
      &m, profileLengthPx);

  if (m.profile_cursor_sample_count > 1) {
    const double cursorPx = MetrologySampleIndexToAxisPxLocal(
        m.profile_cursor_sample_index, m.profile_cursor_sample_count,
        profileLengthPx);
    ImGui::TextDisabled("cursor %s / length %s",
                        FormatMetrologyAxisPixelLabelLocal(cursorPx).c_str(),
                        FormatMetrologyAxisPixelLabelLocal(profileLengthPx).c_str());
  }
}

static void DrawCriticalDimensionPreviewLocal(
    const ManualTestContext &context, const ParserDebugBridge *parserDebugBridge,
    ManualMetrologyUiState &m) {
  std::string sourceRef;
  double profileLengthPx = 0.0;
  std::vector<float> source = BuildMetrologyLiveGaugeLineProfileLocal(
      context, parserDebugBridge, sourceRef, &profileLengthPx);
  if (source.empty() && !MetrologyRequiresRuntimeGaugeLineLocal(context)) {
    source = BuildMetrologyPreviewSourceLocal(
        m, m.critical_dimension_source == 1 ? 0 : 2);
    sourceRef = m.height_peak_source_ref;
    profileLengthPx =
        static_cast<double>(std::max<std::size_t>(1, source.size() - 1));
  }

  const MetrologyPreviewModelLocal model =
      BuildMetrologyCriticalDimensionPreviewModelLocal(m, source);
  const std::vector<MetrologyConclusionMarkerLocal> conclusionMarkers =
      BuildMetrologyConclusionMarkersLocal(
          context, parserDebugBridge, static_cast<int>(source.size()));

  ImGui::Text("Gauge Line NUM %d", m.gauge_line_num);
  if (source.empty()) {
    ImGui::TextDisabled("NO_RUNTIME_PROFILE - %s", sourceRef.c_str());
  } else {
    ImGui::TextDisabled("RUNTIME_GAUGE_LINE_PROFILE - %s", sourceRef.c_str());
    if (model.values.empty()) {
      ImGui::TextDisabled("NO_CD_PREVIEW - auto fit is off or profile range is "
                          "insufficient");
    } else {
      ImGui::TextDisabled("%s", model.status.c_str());
      ImGui::TextDisabled("CD preview position %.3f%% | delta %.3f",
                          model.primary, model.secondary);
    }
  }

  DrawMetrologyPreviewChartLocal(
      "critical_dimension_parameter_preview", "Critical dimension module",
      source, model.values,
      m.critical_dimension_full_range
          ? 0
          : m.critical_dimension_range_start_permille,
      m.critical_dimension_full_range
          ? 1000
          : m.critical_dimension_range_end_permille,
      "profile", "model",
      IsMetrologyRuntimeGaugeLineSourceLocal(sourceRef) ? &conclusionMarkers
                                                       : nullptr,
      &m, profileLengthPx);

  if (m.profile_cursor_sample_count > 1) {
    const double cursorPx = MetrologySampleIndexToAxisPxLocal(
        m.profile_cursor_sample_index, m.profile_cursor_sample_count,
        profileLengthPx);
    ImGui::TextDisabled("cursor %s / length %s",
                        FormatMetrologyAxisPixelLabelLocal(cursorPx).c_str(),
                        FormatMetrologyAxisPixelLabelLocal(profileLengthPx).c_str());
  }
}

static cxvision::metrology_analytics::CxBoundaryResponseConfig
BuildBoundaryResponseConfigLocal(const ManualMetrologyUiState &m) {
  using namespace cxvision::metrology_analytics;
  CxBoundaryResponseConfig config;
  config.baseline_mode = static_cast<CxBoundaryBaselineMode>(
      std::max(0, std::min(4, m.boundary_baseline_mode)));
  config.denoise_mode = static_cast<CxBoundaryDenoiseMode>(
      std::max(0, std::min(5, m.boundary_denoise_mode)));
  config.response_mode = static_cast<CxBoundaryResponseMode>(
      std::max(0, std::min(17, m.boundary_response_mode)));
  config.polarity = static_cast<CxBoundaryPolarity>(
      std::max(0, std::min(2, m.boundary_polarity)));
  config.selection_mode = static_cast<CxBoundarySelectionMode>(
      std::max(0, std::min(5, m.boundary_selection_mode)));
  config.subpixel_mode = static_cast<CxBoundarySubpixelMode>(
      std::max(0, std::min(2, m.boundary_subpixel_mode)));
  config.smoothing_radius = m.boundary_smoothing_radius;
  config.baseline_window = m.boundary_baseline_window;
  config.wavelet_scale = m.boundary_wavelet_scale;
  config.trigger_threshold_permille =
      m.boundary_trigger_threshold_permille;
  config.level_permille = m.boundary_level_permille;
  config.hysteresis_permille = m.boundary_hysteresis_permille;
  config.gate_start_permille = m.boundary_gate_start_permille;
  config.gate_end_permille = m.boundary_gate_end_permille;
  config.nth_candidate = m.boundary_nth_candidate;
  config.min_plateau_width = m.boundary_min_plateau_width;
  config.min_amplitude_permille = m.boundary_min_amplitude_permille;
  config.pair_min_width = m.boundary_pair_min_width;
  config.pair_max_width = m.boundary_pair_max_width;
  config.pair_anchor_mode = static_cast<CxBoundaryPairAnchorMode>(
      std::max(0, std::min(2, m.boundary_pair_anchor_mode)));
  config.reference_position_permille =
      m.boundary_reference_position_permille;
  config.reference_profile = m.boundary_reference_profile;
  return config;
}

static std::vector<MetrologyConclusionMarkerLocal>
BuildBoundaryCandidateMarkersLocal(
    const cxvision::metrology_analytics::CxBoundaryResponseResult &result,
    double profileLengthPx) {
  std::vector<MetrologyConclusionMarkerLocal> markers;
  if (result.selected_candidate < 0 ||
      result.selected_candidate >= static_cast<int>(result.candidates.size()) ||
      result.conditioned.size() < 2)
    return markers;
  const auto &candidate =
      result.candidates[static_cast<std::size_t>(result.selected_candidate)];
  MetrologyConclusionMarkerLocal marker;
  marker.sample_index = candidate.sample_index;
  marker.axis_px = candidate.position_samples * profileLengthPx /
                   static_cast<double>(result.conditioned.size() - 1);
  markers.push_back(marker);
  return markers;
}

static bool BindBoundaryReferenceAtSampleLocal(
    ManualMetrologyUiState &m,
    const cxvision::metrology_analytics::CxBoundaryResponseResult &result,
    int sampleIndex, double positionSamples, double profileLengthPx,
    const char *labelPrefix) {
  if (result.conditioned.size() < 2)
    return false;
  const int sampleCount = static_cast<int>(result.conditioned.size());
  const int clampedSample = std::max(0, std::min(sampleCount - 1, sampleIndex));
  const double clampedPosition =
      std::max(0.0, std::min(static_cast<double>(sampleCount - 1),
                             positionSamples));
  const int halfWindow = std::max(4, m.boundary_wavelet_scale * 3);
  const int first = std::max(0, clampedSample - halfWindow);
  const int last = std::min(sampleCount, clampedSample + halfWindow + 1);
  m.boundary_reference_profile.assign(result.conditioned.begin() + first,
                                      result.conditioned.begin() + last);
  m.boundary_reference_position_permille = static_cast<int>(std::lround(
      clampedPosition * 1000.0 / static_cast<double>(sampleCount - 1)));
  m.boundary_reference_position_permille =
      std::max(0, std::min(1000, m.boundary_reference_position_permille));
  m.boundary_reference_bound = true;
  m.boundary_reference_mode = 1;
  m.boundary_selection_mode = static_cast<int>(
      cxvision::metrology_analytics::CxBoundarySelectionMode::NearestReference);
  const double axisPx =
      clampedPosition * profileLengthPx / static_cast<double>(sampleCount - 1);
  m.boundary_reference_label =
      std::string(labelPrefix != nullptr ? labelPrefix : "reference") + " @ " +
      FormatMetrologyAxisPixelLabelLocal(axisPx);
  return true;
}

static void DrawBoundaryCandidateTableLocal(
    const cxvision::metrology_analytics::CxBoundaryResponseResult &result,
    double profileLengthPx) {
  if (result.candidates.empty()) {
    ImGui::TextDisabled("No boundary candidates: %s", result.reason.c_str());
    return;
  }
  if (result.selected_candidate >= 0 &&
      result.selected_candidate < static_cast<int>(result.candidates.size())) {
    const auto &selected =
        result.candidates[static_cast<std::size_t>(result.selected_candidate)];
    const double selectedAxisPx =
        result.conditioned.size() > 1
            ? selected.position_samples * profileLengthPx /
                  static_cast<double>(result.conditioned.size() - 1)
            : selected.position_samples;
    ImGui::TextColored(
        ImVec4(1.0f, 0.68f, 0.30f, 1.0f),
        "Selected trigger #%d: %s | score %.4f | %s",
        result.selected_candidate + 1,
        FormatMetrologyAxisPixelLabelLocal(selectedAxisPx).c_str(),
        selected.score,
        selected.polarity > 0 ? "rising"
                              : (selected.polarity < 0 ? "falling" : "either"));
  } else {
    ImGui::TextDisabled("No selected trigger: %s", result.reason.c_str());
  }
  if (!ImGui::BeginTable("boundary_candidate_table", 7,
                         ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                             ImGuiTableFlags_SizingStretchProp |
                             ImGuiTableFlags_ScrollY,
                         ImVec2(-1.0f, 150.0f)))
    return;
  ImGui::TableSetupColumn("#");
  ImGui::TableSetupColumn("position");
  ImGui::TableSetupColumn("score");
  ImGui::TableSetupColumn("polarity");
  ImGui::TableSetupColumn("scale");
  ImGui::TableSetupColumn("width");
  ImGui::TableSetupColumn("decision");
  ImGui::TableHeadersRow();
  for (std::size_t i = 0; i < result.candidates.size(); ++i) {
    const auto &candidate = result.candidates[i];
    const double axisPx = result.conditioned.size() > 1
                              ? candidate.position_samples * profileLengthPx /
                                    static_cast<double>(result.conditioned.size() - 1)
                              : 0.0;
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text("%s%u", static_cast<int>(i) == result.selected_candidate ? ">" : "",
                static_cast<unsigned>(i + 1));
    ImGui::TableSetColumnIndex(1);
    ImGui::TextUnformatted(FormatMetrologyAxisPixelLabelLocal(axisPx).c_str());
    ImGui::TableSetColumnIndex(2);
    ImGui::Text("%.4f", candidate.score);
    ImGui::TableSetColumnIndex(3);
    ImGui::TextUnformatted(candidate.polarity > 0
                               ? "rising"
                               : (candidate.polarity < 0 ? "falling" : "either"));
    ImGui::TableSetColumnIndex(4);
    ImGui::Text("%d", candidate.scale);
    ImGui::TableSetColumnIndex(5);
    ImGui::Text("%.2f", candidate.width_samples);
    ImGui::TableSetColumnIndex(6);
    if (static_cast<int>(i) == result.selected_candidate)
      ImGui::TextColored(ImVec4(1.0f, 0.68f, 0.30f, 1.0f), "selected");
    else if (candidate.accepted)
      ImGui::TextUnformatted("accepted");
    else
      ImGui::TextDisabled("%s", candidate.reject_reason.c_str());
  }
  ImGui::EndTable();
}

static void DrawBoundaryScalogramLocal(
    const std::vector<float> &source,
    const cxvision::metrology_analytics::CxBoundaryResponseConfig &baseConfig) {
  using namespace cxvision::metrology_analytics;
  if (source.size() < 3)
    return;
  static constexpr int scales[] = {1, 2, 4, 8, 16};
  std::vector<CxBoundaryResponseResult> rows;
  rows.reserve(IM_ARRAYSIZE(scales));
  float maximum = 1.0e-9f;
  for (int scale : scales) {
    CxBoundaryResponseConfig config = baseConfig;
    config.wavelet_scale = scale;
    CxBoundaryResponseResult row = EvaluateBoundaryResponse(source, config);
    for (float value : row.response)
      maximum = std::max(maximum, std::abs(value));
    rows.push_back(std::move(row));
  }

  const ImVec2 size(std::max(320.0f, ImGui::GetContentRegionAvail().x), 126.0f);
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  ImGui::InvisibleButton("boundary_scalogram", size);
  ImDrawList *draw = ImGui::GetWindowDrawList();
  draw->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y),
                      IM_COL32(20, 24, 29, 255));
  draw->AddText(ImVec2(origin.x + 8.0f, origin.y + 5.0f),
                IM_COL32(225, 230, 235, 255),
                "Scale response map (position x scale)");
  const float left = origin.x + 42.0f;
  const float top = origin.y + 25.0f;
  const float right = origin.x + size.x - 8.0f;
  const float bottom = origin.y + size.y - 8.0f;
  const float rowHeight = (bottom - top) / static_cast<float>(rows.size());
  for (std::size_t row = 0; row < rows.size(); ++row) {
    char label[16] = {};
    std::snprintf(label, sizeof(label), "s%d", scales[row]);
    draw->AddText(ImVec2(origin.x + 7.0f, top + rowHeight * row + 1.0f),
                  IM_COL32(160, 170, 181, 255), label);
    const auto &values = rows[row].response;
    if (values.empty())
      continue;
    const float cellWidth = (right - left) / static_cast<float>(values.size());
    for (std::size_t i = 0; i < values.size(); ++i) {
      const float normalized = std::min(1.0f, std::abs(values[i]) / maximum);
      const bool positive = values[i] >= 0.0f;
      const ImU32 color = positive
                              ? IM_COL32(static_cast<int>(35 + 45 * normalized),
                                         static_cast<int>(55 + 180 * normalized),
                                         static_cast<int>(80 + 120 * normalized), 255)
                              : IM_COL32(static_cast<int>(80 + 175 * normalized),
                                         static_cast<int>(50 + 75 * normalized),
                                         static_cast<int>(45 + 35 * normalized), 255);
      const float x0 = left + cellWidth * static_cast<float>(i);
      draw->AddRectFilled(ImVec2(x0, top + rowHeight * row),
                          ImVec2(x0 + std::max(1.0f, cellWidth + 0.25f),
                                 top + rowHeight * (row + 1)),
                          color);
    }
  }
  draw->AddRect(ImVec2(left, top), ImVec2(right, bottom),
                IM_COL32(105, 115, 125, 255));
}

static bool DrawBoundaryTriggerPreviewLocal(
    const ManualTestContext &context, const ParserDebugBridge *parserDebugBridge,
    ManualMetrologyUiState &m) {
  using namespace cxvision::metrology_analytics;
  bool edited = false;
  std::string sourceRef;
  double profileLengthPx = 0.0;
  const std::vector<float> source = BuildMetrologyLiveGaugeLineProfileLocal(
      context, parserDebugBridge, sourceRef, &profileLengthPx);
  if (source.empty()) {
    ImGui::TextDisabled("NO_RUNTIME_PROFILE - %s", sourceRef.c_str());
    return false;
  }

  CxBoundaryResponseConfig config = BuildBoundaryResponseConfigLocal(m);
  const CxBoundaryResponseResult result =
      EvaluateBoundaryResponse(source, config);
  const auto previewMarkers =
      BuildBoundaryCandidateMarkersLocal(result, profileLengthPx);
  const auto committedMarkers = BuildMetrologyConclusionMarkersLocal(
      context, parserDebugBridge, static_cast<int>(source.size()));
  ImGui::TextDisabled("%s | effective model: %s | noise sigma %.5f",
                      result.status.c_str(),
                      CxBoundaryResponseModeName(result.effective_response_mode),
                      result.noise_sigma);
  if (!previewMarkers.empty()) {
    ImGui::TextDisabled("Preview trigger: %s", FormatMetrologyAxisPixelLabelLocal(
        previewMarkers.front().axis_px).c_str());
  }
  if (!committedMarkers.empty()) {
    ImGui::TextDisabled("Committed conclusion: %s", FormatMetrologyAxisPixelLabelLocal(
        committedMarkers.front().axis_px).c_str());
  } else {
    ImGui::TextDisabled("NO_COMMITTED_CONCLUSION - preview does not replace geometry.");
  }

  if (m.boundary_show_conditioned) {
    DrawMetrologyPreviewChartLocal(
        "boundary_conditioned_preview", "Raw and conditioned profile", source,
        result.conditioned, m.boundary_gate_start_permille,
        m.boundary_gate_end_permille, "raw", "conditioned",
        committedMarkers.empty() ? nullptr : &committedMarkers, &m, profileLengthPx);
  }
  if (m.boundary_show_response) {
    DrawMetrologyPreviewChartLocal(
        "boundary_response_preview", "Boundary response and trigger", result.response,
        result.trigger_level, m.boundary_gate_start_permille,
        m.boundary_gate_end_permille, "response", "trigger",
        committedMarkers.empty() ? nullptr : &committedMarkers, nullptr,
        profileLengthPx);
  }
  if (m.boundary_show_scalogram)
    DrawBoundaryScalogramLocal(source, config);

  const bool manualCursorReady =
      m.profile_cursor_visible && m.profile_cursor_user_dragged &&
      m.profile_cursor_sample_count > 1 && result.conditioned.size() > 1;
  if (manualCursorReady && !committedMarkers.empty()) {
    const double cursorPx = MetrologySampleIndexToAxisPxLocal(
        m.profile_cursor_sample_index, m.profile_cursor_sample_count,
        profileLengthPx);
    const double selectedPx = committedMarkers.front().axis_px;
    const double deltaPx = std::abs(cursorPx - selectedPx);
    if (deltaPx > 2.0) {
      ImGui::TextColored(
          ImVec4(1.0f, 0.72f, 0.30f, 1.0f),
          "Manual cursor differs from committed conclusion by %s.",
          FormatMetrologyAxisPixelLabelLocal(deltaPx).c_str());
    }
  }

  DrawBoundaryCandidateTableLocal(result, profileLengthPx);

  bool actionDrawn = false;
  if (manualCursorReady) {
    if (ImGui::Button("Bind Manual Cursor As Session Reference")) {
      edited |= BindBoundaryReferenceAtSampleLocal(
          m, result, m.profile_cursor_sample_index,
          static_cast<double>(std::max(
              0, std::min(m.profile_cursor_sample_count - 1,
                          m.profile_cursor_sample_index))),
          profileLengthPx, "Manual cursor");
    }
    actionDrawn = true;
  }
  if (result.selected_candidate >= 0 &&
      result.selected_candidate < static_cast<int>(result.candidates.size())) {
    if (actionDrawn)
      ImGui::SameLine();
    if (ImGui::Button("Bind Selected Candidate As Session Reference")) {
      const auto &candidate =
          result.candidates[static_cast<std::size_t>(result.selected_candidate)];
      edited |= BindBoundaryReferenceAtSampleLocal(
          m, result, candidate.sample_index, candidate.position_samples,
          profileLengthPx,
          CxBoundaryResponseModeName(result.effective_response_mode));
    }
    actionDrawn = true;
  }
  if (m.boundary_reference_bound) {
    if (actionDrawn)
      ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.35f, 0.88f, 0.62f, 1.0f), "Reference: %s",
                       m.boundary_reference_label.c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear##boundary_reference")) {
      m.boundary_reference_bound = false;
      m.boundary_reference_mode = 0;
      m.boundary_reference_label.clear();
      m.boundary_reference_profile.clear();
      edited = true;
    }
  } else {
    ImGui::TextDisabled("No session reference bound.");
  }
  return edited;
}


static bool DrawMetrologyExtensionPanel(
    ManualTestContext &context, const ParserDebugBridge *parserDebugBridge) {

  ManualMetrologyUiState &m = context.metrology_ui;
  bool edited = false;


  ImGui::SetNextItemOpen(false, ImGuiCond_FirstUseEver);
  if (!ImGui::CollapsingHeader("Metrology Extension / Surface "
                               "Analytics"))
    return false;

  ImGui::TextWrapped("Metrology parameter and review "
                     "surface. Values are "
                     "injected as global_metrology_*; "
                     "charts draw only when "
                     "runtime profile data is available "
                     "for the selected Gauge Line.");
  edited |= ImGui::Checkbox("Enable metrology extension", &m.enabled);
  ImGui::SameLine();
  ImGui::TextDisabled("No PASS is inferred "
                      "from this panel.");

  edited |= DrawMetrologyGaugeLineSelectorLocal(context, m);

  if (ImGui::BeginTabBar("metrology_extension_tabs")) {
    if (ImGui::BeginTabItem("Scan Profile")) {
      m.active_tab = 0;
      static const char *sources[] = {"runtime snapshot", "gauge preview",
                                      "saved evidence"};
      edited |=
          ImGui::Checkbox("Show scan profile overlay", &m.show_scan_profile);
      edited |= DrawMetrologyComboLocal("profile source", m.scan_profile_source,
                                        sources, IM_ARRAYSIZE(sources));
      edited |= DrawMetrologySliderIntLocal("max scan lines",
                                            m.scan_profile_max_lines, 1, 4096);
      edited |= DrawMetrologySliderIntLocal(
          "sample stride", m.scan_profile_sample_stride, 1, 64);
      edited |= DrawMetrologySliderIntLocal(
          "edge band index", m.scan_profile_edge_band_index, 0, 32);
      edited |= DrawMetrologySliderIntLocal(
          "smoothing radius", m.scan_profile_smoothing_radius, 0, 32);
      ImGui::TextDisabled("Expected source: "
                          "FindLine/FindCircle actual scan "
                          "diagnostics, not "
                          "display-generated ticks.");
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Boundary Trigger")) {
      m.active_tab = 1;
      ImGui::TextColored(ImVec4(0.35f, 0.88f, 0.62f, 1.0f),
                         "Waveform trigger preview");
      ImGui::SameLine();
      ImGui::TextDisabled("response candidates remain pending human review");
      ImGui::SameLine();
      if (ImGui::SmallButton("Restore Defaults##boundary_trigger")) {
        m.boundary_preview_enabled = true;
        m.boundary_baseline_mode = 1;
        m.boundary_denoise_mode = 2;
        m.boundary_smoothing_radius = 2;
        m.boundary_baseline_window = 12;
        m.boundary_response_mode = 0;
        m.boundary_polarity = 2;
        m.boundary_wavelet_scale = 4;
        m.boundary_trigger_threshold_permille = 120;
        m.boundary_level_permille = 500;
        m.boundary_hysteresis_permille = 50;
        m.boundary_gate_start_permille = 0;
        m.boundary_gate_end_permille = 1000;
        m.boundary_selection_mode = 4;
        m.boundary_nth_candidate = 1;
        m.boundary_min_plateau_width = 3;
        m.boundary_min_amplitude_permille = 80;
        m.boundary_pair_min_width = 2;
        m.boundary_pair_max_width = 80;
        m.boundary_pair_anchor_mode = 0;
        m.boundary_subpixel_mode = 2;
        m.boundary_show_conditioned = true;
        m.boundary_show_response = true;
        m.boundary_show_scalogram = false;
        edited = true;
      }
      ImGui::SameLine();
      if (ImGui::SmallButton("White Rim First Edge##boundary_trigger")) {
        m.boundary_preview_enabled = true;
        m.boundary_response_mode = 11;
        m.boundary_polarity = 2;
        m.boundary_wavelet_scale = 4;
        m.boundary_trigger_threshold_permille = 80;
        m.boundary_selection_mode = 4;
        m.boundary_min_plateau_width = 8;
        m.boundary_min_amplitude_permille = 80;
        m.boundary_pair_min_width = 8;
        m.boundary_pair_max_width = 96;
        m.boundary_pair_anchor_mode = 1;
        m.boundary_subpixel_mode = 2;
        m.boundary_show_conditioned = true;
        m.boundary_show_response = true;
        edited = true;
      }

      edited |= ImGui::Checkbox("Enable boundary response preview",
                                &m.boundary_preview_enabled);
      ImGui::SameLine();
      edited |= ImGui::Checkbox("Raw + conditioned",
                                &m.boundary_show_conditioned);
      ImGui::SameLine();
      edited |= ImGui::Checkbox("Response + trigger",
                                &m.boundary_show_response);
      ImGui::SameLine();
      edited |= ImGui::Checkbox("Scale map", &m.boundary_show_scalogram);

      if (ImGui::BeginTabBar("boundary_trigger_subtabs")) {
        if (ImGui::BeginTabItem("Conditioning")) {
          static const char *baselineModes[] = {
              "none", "offset", "linear endpoints", "robust linear",
              "rolling mean"};
          static const char *denoiseModes[] = {
              "none", "box", "Gaussian", "median", "Savitzky-Golay",
              "Haar soft threshold"};
          edited |= DrawMetrologyComboLocal(
              "baseline", m.boundary_baseline_mode, baselineModes,
              IM_ARRAYSIZE(baselineModes));
          edited |= DrawMetrologySliderIntLocal(
              "baseline window", m.boundary_baseline_window, 1, 128, "samples");
          edited |= DrawMetrologyComboLocal(
              "denoise", m.boundary_denoise_mode, denoiseModes,
              IM_ARRAYSIZE(denoiseModes));
          edited |= DrawMetrologySliderIntLocal(
              "smoothing radius", m.boundary_smoothing_radius, 0, 32,
              "samples");
          ImGui::TextDisabled(
              "Conditioning is previewed separately; polarity is preserved.");
          ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Response Model")) {
          static const char *responseModes[] = {
              "Auto",          "Level crossing", "Peak",
              "Valley",        "Rising gradient", "Falling gradient",
              "Curvature zero crossing", "Positive step", "Negative step",
              "Pull up",       "Pull down",      "Rise then fall",
              "Fall then rise", "Haar wavelet",  "Daubechies 4",
              "Daubechies 20", "Derivative of Gaussian",
              "Template correlation"};
          static const char *polarities[] = {"rising", "falling", "either"};
          static const char *subpixelModes[] = {
              "none", "linear crossing", "parabolic response"};
          static const char *pairAnchorModes[] = {
              "pair center", "first edge", "second edge"};
          edited |= DrawMetrologyComboLocal(
              "response model", m.boundary_response_mode, responseModes,
              IM_ARRAYSIZE(responseModes));
          edited |= DrawMetrologyComboLocal(
              "polarity", m.boundary_polarity, polarities,
              IM_ARRAYSIZE(polarities));
          edited |= DrawMetrologySliderIntLocal(
              "response scale", m.boundary_wavelet_scale, 1, 32, "samples");
          edited |= DrawMetrologyComboLocal(
              "subpixel", m.boundary_subpixel_mode, subpixelModes,
              IM_ARRAYSIZE(subpixelModes));
          edited |= DrawMetrologyComboLocal(
              "pair anchor", m.boundary_pair_anchor_mode, pairAnchorModes,
              IM_ARRAYSIZE(pairAnchorModes));
          if (m.boundary_response_mode == 17 &&
              !m.boundary_reference_bound)
            ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.32f, 1.0f),
                               "Template correlation requires a bound reference.");
          ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Trigger")) {
          static const char *selectionModes[] = {
              "strongest", "first", "last", "nth", "nearest gate center",
              "nearest reference"};
          edited |= DrawMetrologySliderIntLocal(
              "trigger threshold", m.boundary_trigger_threshold_permille, 0,
              1000, "permille of response max");
          edited |= DrawMetrologySliderIntLocal(
              "level crossing", m.boundary_level_permille, 0, 1000,
              "permille of conditioned range");
          edited |= DrawMetrologySliderIntLocal(
              "hysteresis", m.boundary_hysteresis_permille, 0, 1000,
              "permille");
          edited |= DrawMetrologySliderIntLocal(
              "gate start", m.boundary_gate_start_permille, 0, 999, "permille");
          edited |= DrawMetrologySliderIntLocal(
              "gate end", m.boundary_gate_end_permille, 1, 1000, "permille");
          if (m.boundary_gate_start_permille >= m.boundary_gate_end_permille)
            m.boundary_gate_end_permille =
                std::min(1000, m.boundary_gate_start_permille + 1);
          edited |= DrawMetrologyComboLocal(
              "candidate selection", m.boundary_selection_mode, selectionModes,
              IM_ARRAYSIZE(selectionModes));
          if (m.boundary_selection_mode == 3)
            edited |= DrawMetrologySliderIntLocal(
                "candidate ordinal", m.boundary_nth_candidate, 1, 64);
          edited |= DrawMetrologySliderIntLocal(
              "min plateau width", m.boundary_min_plateau_width, 1, 128,
              "samples");
          edited |= DrawMetrologySliderIntLocal(
              "min amplitude", m.boundary_min_amplitude_permille, 0, 1000,
              "permille");
          edited |= DrawMetrologySliderIntLocal(
              "pair min width", m.boundary_pair_min_width, 1, 1024, "samples");
          edited |= DrawMetrologySliderIntLocal(
              "pair max width", m.boundary_pair_max_width, 1, 2048, "samples");
          if (m.boundary_pair_min_width > m.boundary_pair_max_width)
            m.boundary_pair_max_width = m.boundary_pair_min_width;
          ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Reference")) {
          static const char *referenceModes[] = {"none", "session reference"};
          edited |= DrawMetrologyComboLocal(
              "reference source", m.boundary_reference_mode, referenceModes,
              IM_ARRAYSIZE(referenceModes));
          edited |= DrawMetrologySliderIntLocal(
              "reference position", m.boundary_reference_position_permille, 0,
              1000, "permille");
          if (m.boundary_reference_bound) {
            ImGui::Text("Bound model: %s", m.boundary_reference_label.c_str());
            ImGui::TextDisabled("Reference samples: %u",
                                static_cast<unsigned>(
                                    m.boundary_reference_profile.size()));
          } else {
            ImGui::TextDisabled(
                "Select a preview candidate below, then bind it as reference.");
          }
          ImGui::TextDisabled(
              "Session binding is value-semantic; asset promotion remains gated.");
          ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
      }

      if (ImGui::CollapsingHeader("Legacy EdgeBand diagnostics")) {
        static const char *featureModes[] = {"gradient", "component",
                                             "confidence"};
        edited |= ImGui::Checkbox("Show EdgeBandCandidate",
                                  &m.show_edge_band_candidates);
        edited |= DrawMetrologySliderIntLocal(
            "candidate rank", m.candidate_rank, 0, 64);
        edited |= DrawMetrologySliderIntLocal(
            "min gradient", m.candidate_min_gradient, 0, 255);
        edited |= DrawMetrologySliderIntLocal(
            "max band width", m.candidate_max_width, 1, 1024);
        edited |= DrawMetrologyComboLocal(
            "feature map", m.feature_map_mode, featureModes,
            IM_ARRAYSIZE(featureModes));
      }

      if (m.boundary_preview_enabled)
        edited |= DrawBoundaryTriggerPreviewLocal(context, parserDebugBridge, m);
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Surface Data")) {
      m.active_tab = 2;
      static const char *surfaceSources[] = {"image gray", "segmentation mask",
                                             "synthetic"};
      edited |=
          DrawMetrologyComboLocal("surface source", m.surface_source,
                                  surfaceSources, IM_ARRAYSIZE(surfaceSources));
      edited |= DrawMetrologySliderIntLocal("surface width", m.surface_width, 1,
                                            8192);
      edited |= DrawMetrologySliderIntLocal("surface height", m.surface_height,
                                            1, 8192);
      edited |= DrawMetrologySliderIntLocal("surface stride", m.surface_stride,
                                            1, 128);
      edited |=
          DrawMetrologySliderIntLocal("z channel", m.surface_z_channel, 0, 16);
      ImGui::TextDisabled("SurfaceField is value/snapshot "
                          "data. UI does not "
                          "own cv::Mat or parser objects.");
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Area")) {
      m.active_tab = 3;
      static const char *areaMethods[] = {"projected only", "four-triangle fan",
                                          "reserved"};
      edited |= DrawMetrologyComboLocal("area method", m.surface_area_method,
                                        areaMethods, IM_ARRAYSIZE(areaMethods));
      ImGui::Text("Projected area depends on x/y "
                  "unit scale. Surface area uses "
                  "Z scale when bound.");
      ImGui::TextDisabled("Current analytics "
                          "implementation exists under "
                          "cximage/metrology_analytics.");
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Stats / Histogram")) {
      m.active_tab = 4;
      static const char *histModes[] = {"ADF", "BCDF", "ADF + BCDF"};
      edited |= DrawMetrologySliderIntLocal("histogram bins", m.histogram_bins,
                                            8, 4096);
      edited |= DrawMetrologyComboLocal("histogram mode", m.histogram_mode,
                                        histModes, IM_ARRAYSIZE(histModes));
      edited |= ImGui::Checkbox("log scale", &m.histogram_log_scale);
      ImGui::TextDisabled("Run Find Peaks for the real ADF "
                          "curve, BCDF "
                          "bearing curve, peak markers and "
                          "measured peak table.");
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Find Peaks")) {
      m.active_tab = 8;
      ImGui::TextColored(ImVec4(0.35f, 0.88f, 0.62f, 1.0f), "Reliable default");
      ImGui::SameLine();
      ImGui::TextDisabled("12 peaks | position | bilateral "
                          "minimum | "
                          "prominence 20/1000 | distance 4 "
                          "bins");
      ImGui::SameLine();
      if (ImGui::SmallButton("Restore "
                             "Defaults##find_peaks")) {
        m.peak_max_count = 12;
        m.peak_order = 0;
        m.peak_min_prominence_permille = 20;
        m.peak_min_distance_bins = 4;
        m.peak_background = 1;
        m.peak_invert = false;
        m.height_peak_analysis_ready = false;
        edited = true;
      }

      if (!m.enabled)
        ImGui::BeginDisabled();
      if (ImGui::Button("Analyze Current Surface"))
        AnalyzeMetrologyHeightPeaksLocal(context, parserDebugBridge);
      if (!m.enabled)
        ImGui::EndDisabled();
      ImGui::SameLine();
      ImGui::TextDisabled("Result remains pending human "
                          "review.");

      if (ImGui::CollapsingHeader("Fine Tuning##find_peaks")) {
        static const char *peakOrders[] = {"position", "prominence"};
        static const char *peakBackgrounds[] = {"zero", "bilateral minimum"};
        edited |= DrawMetrologySliderIntLocal("number of peaks",
                                              m.peak_max_count, 1, 64);
        edited |= DrawMetrologyComboLocal("order peaks by", m.peak_order,
                                          peakOrders, IM_ARRAYSIZE(peakOrders));
        edited |= DrawMetrologySliderIntLocal(
            "min prominence permille", m.peak_min_prominence_permille, 0, 1000);
        edited |= DrawMetrologySliderIntLocal("min peak distance (bins)",
                                              m.peak_min_distance_bins, 0, 512);
        edited |= DrawMetrologyComboLocal("peak background", m.peak_background,
                                          peakBackgrounds,
                                          IM_ARRAYSIZE(peakBackgrounds));
        edited |= ImGui::Checkbox("invert (find valleys)", &m.peak_invert);
      }

      DrawMetrologyHeightPeakResultsLocal(m);
      if (!m.height_peak_analysis_ready)
        DrawFindPeaksPreviewLocal(context, parserDebugBridge, m);
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Curve Fitting")) {
      m.active_tab = 9;
      ImGui::TextColored(ImVec4(0.35f, 0.88f, 0.62f, 1.0f), "Reliable default");
      ImGui::SameLine();
      ImGui::TextDisabled("ADF | Gaussian | auto estimate "
                          "| auto plot | full range");
      ImGui::SameLine();
      if (ImGui::SmallButton("Restore "
                             "Defaults##curve_fit")) {
        m.curve_fit_source = 0;
        m.curve_fit_function = 0;
        m.curve_fit_auto_estimate = true;
        m.curve_fit_auto_plot = true;
        m.curve_fit_full_range = true;
        m.curve_fit_output_residual = false;
        m.curve_fit_range_start_permille = 0;
        m.curve_fit_range_end_permille = 1000;
        edited = true;
      }
      ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.32f, 1.0f), "PENDING_BINDING");
      ImGui::SameLine();
      ImGui::TextDisabled("parameters are projected; "
                          "runtime nonlinear fit is not "
                          "claimed");

      if (ImGui::CollapsingHeader("Fine Tuning##curve_fit")) {
        static const char *curveSources[] = {"height distribution (ADF)",
                                             "bearing curve (BCDF)",
                                             "scan profile"};
        static const char *fitFunctions[] = {"Gaussian", "Lorentzian",
                                             "Polynomial (order 2)"};
        edited |=
            DrawMetrologyComboLocal("fit curve source", m.curve_fit_source,
                                    curveSources, IM_ARRAYSIZE(curveSources));
        edited |=
            DrawMetrologyComboLocal("fit function", m.curve_fit_function,
                                    fitFunctions, IM_ARRAYSIZE(fitFunctions));
        edited |= ImGui::Checkbox("auto estimate parameters",
                                  &m.curve_fit_auto_estimate);
        edited |=
            ImGui::Checkbox("auto plot fitted curve", &m.curve_fit_auto_plot);
        edited |=
            ImGui::Checkbox("use full curve range", &m.curve_fit_full_range);
        edited |= ImGui::Checkbox("output residual curve",
                                  &m.curve_fit_output_residual);
        if (m.curve_fit_full_range)
          ImGui::BeginDisabled();
        edited |= DrawMetrologySliderIntLocal("fit range start permille",
                                              m.curve_fit_range_start_permille,
                                              0, 999);
        edited |= DrawMetrologySliderIntLocal(
            "fit range end permille", m.curve_fit_range_end_permille, 1, 1000);
        if (m.curve_fit_full_range)
          ImGui::EndDisabled();
        if (m.curve_fit_range_start_permille >=
            m.curve_fit_range_end_permille) {
          m.curve_fit_range_end_permille =
              std::min(1000, m.curve_fit_range_start_permille + 1);
        }
      }
      if (m.curve_fit_source == 2)
        ImGui::TextDisabled(
            "Scan-profile curve fitting does not select a boundary; use Boundary Trigger.");
      DrawCurveFitPreviewLocal(context, parserDebugBridge, m);
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Critical Dimension")) {
      m.active_tab = 10;
      ImGui::TextColored(ImVec4(0.35f, 0.88f, 0.62f, 1.0f), "Reliable default");
      ImGui::SameLine();
      ImGui::TextDisabled("scan profile | Edge height "
                          "right | auto fit | full range");
      ImGui::SameLine();
      if (ImGui::SmallButton("Restore "
                             "Defaults##critical_"
                             "dimension")) {
        m.critical_dimension_source = 0;
        m.critical_dimension_function = 0;
        m.critical_dimension_auto_fit = true;
        m.critical_dimension_full_range = true;
        m.critical_dimension_range_start_permille = 0;
        m.critical_dimension_range_end_permille = 1000;
        m.critical_dimension_draw_whole_circle = false;
        edited = true;
      }
      ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.32f, 1.0f), "PENDING_BINDING");
      ImGui::SameLine();
      ImGui::TextDisabled("parameters are projected; "
                          "critical-dimension fit is not "
                          "claimed");

      if (ImGui::CollapsingHeader("Fine "
                                  "Tuning##critical_"
                                  "dimension")) {
        static const char *criticalSources[] = {"scan profile",
                                                "height distribution "
                                                "(ADF)"};
        static const char *criticalFunctions[] = {
            "Edge height (right)",    "Edge height (left)",
            "Step height (positive)", "Step height (negative)",
            "Circle (down)",          "Circle (up)"};
        edited |= DrawMetrologyComboLocal(
            "CD curve source", m.critical_dimension_source, criticalSources,
            IM_ARRAYSIZE(criticalSources));
        edited |= DrawMetrologyComboLocal(
            "CD fit function", m.critical_dimension_function, criticalFunctions,
            IM_ARRAYSIZE(criticalFunctions));
        edited |= ImGui::Checkbox("auto fit from default "
                                  "estimate",
                                  &m.critical_dimension_auto_fit);
        edited |= ImGui::Checkbox("use full CD curve range",
                                  &m.critical_dimension_full_range);
        if (m.critical_dimension_full_range)
          ImGui::BeginDisabled();
        edited |= DrawMetrologySliderIntLocal(
            "CD range start permille",
            m.critical_dimension_range_start_permille, 0, 999);
        edited |= DrawMetrologySliderIntLocal(
            "CD range end permille", m.critical_dimension_range_end_permille, 1,
            1000);
        if (m.critical_dimension_full_range)
          ImGui::EndDisabled();
        if (m.critical_dimension_range_start_permille >=
            m.critical_dimension_range_end_permille) {
          m.critical_dimension_range_end_permille =
              std::min(1000, m.critical_dimension_range_start_permille + 1);
        }
        if (m.critical_dimension_function >= 4) {
          edited |= ImGui::Checkbox("draw whole fitted circle",
                                    &m.critical_dimension_draw_whole_circle);
        } else {
          m.critical_dimension_draw_whole_circle = false;
        }
      }
      DrawCriticalDimensionPreviewLocal(context, parserDebugBridge, m);
      ImGui::EndTabItem();
    }


    if (ImGui::BeginTabItem("Plane Level")) {
      m.active_tab = 5;
      static const char *planeMethods[] = {"three points", "OLS", "Huber WLS"};
      static const char *referenceModes[] = {"whole surface", "ROI", "mask"};
      edited |= ImGui::Checkbox("enable plane correction",
                                &m.enable_plane_correction);
      edited |=
          DrawMetrologyComboLocal("plane method", m.plane_method, planeMethods,
                                  IM_ARRAYSIZE(planeMethods));
      edited |=
          DrawMetrologyComboLocal("reference mode", m.plane_reference_mode,
                                  referenceModes, IM_ARRAYSIZE(referenceModes));
      edited |= DrawMetrologySliderIntLocal(
          "Huber delta permille", m.plane_huber_delta_permille, 1, 10000);
      ImGui::TextDisabled("Plane correction changes "
                          "analytic surface "
                          "interpretation, not original "
                          "image pixels.");
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Units + Z Noise")) {
      m.active_tab = 6;
      static const char *units[] = {"pixel", "nm", "um", "mm"};
      edited |= DrawMetrologyComboLocal("x unit", m.x_unit, units,
                                        IM_ARRAYSIZE(units));
      edited |= DrawMetrologyComboLocal("y unit", m.y_unit, units,
                                        IM_ARRAYSIZE(units));
      edited |= DrawMetrologyComboLocal("z unit", m.z_unit, units,
                                        IM_ARRAYSIZE(units));
      edited |= DrawMetrologySliderIntLocal("x scale permille",
                                            m.x_scale_permille, 1, 100000);
      edited |= DrawMetrologySliderIntLocal("y scale permille",
                                            m.y_scale_permille, 1, 100000);
      edited |= DrawMetrologySliderIntLocal("z scale permille",
                                            m.z_scale_permille, 1, 100000);
      edited |= ImGui::Checkbox("enable Gaussian Z perturbation",
                                &m.enable_gaussian_z);
      edited |= DrawMetrologySliderIntLocal(
          "Z sigma permille", m.gaussian_z_sigma_permille, 0, 100000);
      edited |= DrawMetrologySliderIntLocal("Gaussian seed", m.gaussian_seed, 0,
                                            2147483647);
      ImGui::TextDisabled("Z perturbation is an "
                          "uncertainty probe; it must be "
                          "traceable by seed.");
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("ISO 1D Roughness")) {
      m.active_tab = 7;
      static const char *axes[] = {"x profile", "y profile", "selected line"};
      edited |= ImGui::Checkbox("enable ISO 1D roughness",
                                &m.enable_iso_roughness_1d);
      edited |= DrawMetrologyComboLocal(
          "profile axis", m.roughness_profile_axis, axes, IM_ARRAYSIZE(axes));
      edited |= DrawMetrologySliderIntLocal("profile index",
                                            m.roughness_profile_index, 0, 8192);
      edited |= DrawMetrologySliderIntLocal("cutoff px", m.roughness_cutoff_px,
                                            0, 8192);
      edited |= DrawMetrologySliderIntLocal("roughness bins", m.roughness_bins,
                                            8, 4096);
      ImGui::TextDisabled("Outputs expected later: Ra / Rq "
                          "/ Rsk / Rku_std + ADF/BCDF.");
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }

  if (edited) {
    ++m.edit_revision;
    InjectMetrologyUiGlobals(context);
    m.last_summary = BuildMetrologyUiSummary(m);
    context.debug_status = "METROLOGY_UI_EDITED";
    context.debug_reason = m.last_summary;
    CXLOG_INFO("KeyParameterControls", "metrology_ui_edit", "edited",
               "revision=" + std::to_string(m.edit_revision) + " " +
                   m.last_summary);
  }

  if (!m.last_summary.empty())
    ImGui::TextWrapped("Last: %s", m.last_summary.c_str());
  else
    ImGui::TextDisabled("No metrology parameter edits "
                        "yet.");

  return edited;
}


static bool ContainsCaseInsensitiveLocal(const std::string &text,
                                         const std::string &needle) {
  if (needle.empty())
    return true;
  std::string haystack = text;
  std::string target = needle;
  std::transform(haystack.begin(), haystack.end(), haystack.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  std::transform(target.begin(), target.end(), target.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return haystack.find(target) != std::string::npos;
}

static bool KeyParamContextLooksFastMatchLocal(
    const ManualTestContext &context) {
  const ManualGaugeState &gauge = context.current_gauge;
  if (gauge.tool == "FastMatch" ||
      NormalizeKeyParamToolTypeLocal(gauge.primary_object_type) == "FastMatch")
    return true;
  if (context.runtime_int_vars.find("global_fastmatch_scan_rotation_deg") !=
      context.runtime_int_vars.end())
    return true;
  if (ContainsCaseInsensitiveLocal(context.loaded_script_path, "fastmatch") ||
      ContainsCaseInsensitiveLocal(context.script_file_path, "fastmatch") ||
      ContainsCaseInsensitiveLocal(context.active_script_case_name,
                                   "fastmatch"))
    return true;
  for (const RuntimeObjectView &object : context.runtime_objects) {
    if (NormalizeKeyParamToolTypeLocal(object.type) == "FastMatch" ||
        object.has_fastmatch_diagnostic ||
        ContainsCaseInsensitiveLocal(object.fastmatch_source_tool,
                                     "fastmatch"))
      return true;
  }
  return false;
}

void DrawKeyParameterControlPanel(
    ManualTestContext &context, const ParserDebugBridge *parserDebugBridge) {
  ManualGaugeState &gauge = context.current_gauge;
  bool gaugeEdited = false;

  ImGui::TextUnformatted("Tool: ");
  ImGui::SameLine();
  const bool isCxTextInspect =
      NormalizeKeyParamToolTypeLocal(gauge.tool) == "CxTextInspect" ||
      NormalizeKeyParamToolTypeLocal(gauge.primary_object_type) == "CxTextInspect" ||
      ContainsCaseInsensitiveLocal(context.loaded_script_path, "cxtextinspect") ||
      ContainsCaseInsensitiveLocal(context.script_file_path, "cxtextinspect");
  const bool isFastMatch =
      !isCxTextInspect && KeyParamContextLooksFastMatchLocal(context);
  const bool isFindLine =
      gauge.tool == "FindLine" ||
      NormalizeKeyParamToolTypeLocal(gauge.primary_object_type) == "FindLine";
  const bool isFindEllipse = gauge.tool == "FindEllipse" ||
                             NormalizeKeyParamToolTypeLocal(
                                 gauge.primary_object_type) == "FindEllipse" ||
                             gauge.has_ellipse_gauge;

  const bool isFindRect =
      gauge.tool == "FindRect" ||
      NormalizeKeyParamToolTypeLocal(gauge.primary_object_type) == "FindRect";
  const bool isFindCircle =
      gauge.tool == "FindCircle" || gauge.has_circle_gauge ||
      NormalizeKeyParamToolTypeLocal(gauge.primary_object_type) == "FindCircle";
  const bool isGridPattern =
      gauge.tool == "GridPatternClassTool" ||
      NormalizeKeyParamToolTypeLocal(gauge.primary_object_type) ==
          "GridPatternClassTool";
  const bool isRegionPattern =
      gauge.tool == "RegionPatternTool" ||
      NormalizeKeyParamToolTypeLocal(gauge.primary_object_type) ==
          "RegionPatternTool";
  const bool isFindSegmentation =
      gauge.tool == "FindSegmentation" ||
      NormalizeKeyParamToolTypeLocal(gauge.primary_object_type) ==
          "FindSegmentation";
  const bool isFindObject =
      gauge.tool == "FindObject" ||
      NormalizeKeyParamToolTypeLocal(gauge.primary_object_type) == "FindObject";
  if (gauge.tool == "FindLine" || gauge.has_line_gauge) {
    ImGui::Checkbox("Show single-line gauge scan ticks",
                    &context.show_line_gauge_scan_lines);
    gaugeEdited |= DrawFindLineEdgeSelectorPanel(context);
    gaugeEdited |=
        DrawFindLineEdgeRolePanel(context, FindCurrentFindLineObject(context));
    DrawFindLineEdgeEvaluationPanel(context);
    DrawFindLineScanSemanticsPanel(context);
  }
  if (gauge.tool == "FindCircle" || gauge.has_circle_gauge) {
    ImGui::Checkbox("Show circle gauge scan ticks",
                    &context.show_circle_gauge_scan_lines);
    gaugeEdited |= DrawFindCircleEdgeSelectorPanel(context);
    gaugeEdited |= DrawFindCircleEdgeRolePanel(context);
    DrawFindCircleEdgeEvaluationPanel(context);
    DrawFindCircleScanSemanticsPanel(context);
  }
  if (isFindEllipse) {
    ImGui::Checkbox("Show ellipse gauge scan ticks",
                    &context.show_ellipse_gauge_scan_lines);
    gaugeEdited |= DrawFindEllipseEdgeSelectorPanel(context);
    gaugeEdited |= DrawFindEllipseEdgeRolePanel(context);
    DrawFindEllipseEdgeEvaluationPanel(context);
    DrawFindEllipseScanSemanticsPanel(context);
  }
if (isCxTextInspect) {
    ImGui::TextColored(ImVec4(0.45f, 0.88f, 0.72f, 1.0f),
                       "CxTextInspect: component split -> glyph geometry -> "
                       "FastMatch classification -> reading-order decode");
  }
  if (isFastMatch) {
    ImGui::TextColored(ImVec4(1.0f, 0.86f, 0.35f, 1.0f),
                       "FastMatch: learn ROI + search ROI "
                       "+ matching params");
    ImGui::TextDisabled("Image View source is read-only. The controls below are temporary render layers and are never written into the test image.");

    ImGui::Checkbox("Show FastMatch debug overlay",
                    &context.show_fastmatch_debug_vectors);
    ImGui::SameLine();
    ImGui::Checkbox("Show FastMatch Learn Result",
                    &context.show_fastmatch_learn_result);
    ImGui::Checkbox("Show four-edge FindLine scan ticks",
                    &context.show_fastmatch_learn_scan_ticks);
    ImGui::SameLine();
    const char* fastMatchLearnEdges[] = {"Top", "Bottom", "Left", "Right"};
    const int activeProbe = std::clamp(context.fastmatch_selected_learn_edge,
                                       0, 3);
    ImGui::TextDisabled("active probe: %s (select its tab below)",
                        fastMatchLearnEdges[activeProbe]);
    ImGui::TextDisabled("Ticks and orange conclusion points are projected into the active directional bucket; use its tab to keep parameter tuning and visual analysis aligned.");
    ImGui::SameLine();
    ImGui::Checkbox("compare_gap point pairs",
                    &context.show_fastmatch_compare_gap_pairs);
    ImGui::Checkbox("keypoint tangents",
                    &context.show_fastmatch_keypoint_tangents);
    ImGui::SameLine();
    ImGui::Checkbox("filtered/unpaired points",
                    &context.show_fastmatch_filtered_points);
    gaugeEdited |= DrawRuntimeIntRow(context, "scan rotation deg",
                                     "global_fastmatch_scan_rotation_deg", 0,
                                     -180, 180, 150.0f);
    ImGui::TextDisabled(
        "Template pose is preserved during ROI move/resize. The purple "
        "outline in Image View is the exact orientation that Learn will use.");
    const int affineExecuted = RuntimeIntOr(
        context, "global_fastmatch_transform_executed", 0);
    if (affineExecuted != 0) {
      const int affineConverged = RuntimeIntOr(
          context, "global_fastmatch_transform_converged", 0);
      const int budgetExceeded = RuntimeIntOr(
          context, "global_fastmatch_transform_budget_exceeded", 0);
      ImGui::SeparatorText("Affine search analysis");
      ImGui::Text("status: %s", budgetExceeded != 0 ? "algorithm_budget_exceeded" :
          (affineConverged != 0 ? "converged" : "score_below_threshold"));
      ImGui::Text("candidates=%d samples=%d elapsed=%d ms",
          RuntimeIntOr(context, "global_fastmatch_transform_candidate_count", 0),
          RuntimeIntOr(context, "global_fastmatch_transform_sample_count", 0),
          RuntimeIntOr(context, "global_fastmatch_transform_elapsed_ms", 0));
      ImGui::Text("best score=%.3f scale=(%.3f, %.3f) angle=%d deg",
          RuntimeIntOr(context, "global_fastmatch_transform_best_score_permille", 0) / 1000.0,
          RuntimeIntOr(context, "global_fastmatch_transform_best_scale_x_permille", 0) / 1000.0,
          RuntimeIntOr(context, "global_fastmatch_transform_best_scale_y_permille", 0) / 1000.0,
          RuntimeIntOr(context, "global_fastmatch_transform_best_angle_deg", 0));
      ImGui::Text("P1 shear=%.3f projective=(%.4f, %.4f)",
          RuntimeIntOr(context, "global_fastmatch_transform_best_shear_permille", 0) / 1000.0,
          RuntimeIntOr(context, "global_fastmatch_transform_best_projective_u_permille", 0) / 1000.0,
          RuntimeIntOr(context, "global_fastmatch_transform_best_projective_v_permille", 0) / 1000.0);
      ImGui::Text("gradient=%.3f residual=%.3fpx rigid baseline=%.3f",
          RuntimeIntOr(context, "global_fastmatch_transform_gradient_score_permille", 0) / 1000.0,
          RuntimeIntOr(context, "global_fastmatch_transform_geometric_residual_milli_px", 0) / 1000.0,
          RuntimeIntOr(context, "global_fastmatch_transform_rigid_baseline_score_permille", 0) / 1000.0);
      const int calibrationApplied = RuntimeIntOr(
          context, "global_fastmatch_transform_calibration_applied", 0);
      ImGui::Text("calibration: %s  physical center=(%.3f, %.3f) unit",
          calibrationApplied != 0 ? "applied" : "not applied",
          RuntimeIntOr(context,
              "global_fastmatch_transform_best_physical_cx_milliunit", 0) / 1000.0,
          RuntimeIntOr(context,
              "global_fastmatch_transform_best_physical_cy_milliunit", 0) / 1000.0);
      const int calibrationRmseMilliPx = RuntimeIntOr(context,
          "global_fastmatch_transform_calibration_reprojection_rmse_milli_px", -1000);
      if (calibrationRmseMilliPx >= 0) {
        ImGui::Text("calibration reprojection RMSE=%.3f px",
                    calibrationRmseMilliPx / 1000.0);
      } else {
        ImGui::TextDisabled("calibration reprojection RMSE: not supplied");
      }
    }
    ImGui::TextDisabled("Image View colors: cyan=A side, orange=B side, "
                        "yellow=keypoint midpoint, magenta=adjacent-point "
                        "tangent, red=filtered/unpaired point. compare_gap is "
                        "the keypoint-to-A/B offset; total pair distance is "
                        "twice this value.");

    DrawFastMatchTemplateStatusPanel(context);
  }
  if (isFindSegmentation) {
    ImGui::TextColored(ImVec4(0.35f, 0.88f, 0.62f, 1.0f),
                       "FindSegmentation: prompt ROI -> "
                       "segment -> boundary/overlay");
  }
  if (isFindObject) {
    ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.35f, 1.0f),
                       "FindObject: editable ROI -> "
                       "binary components -> "
                       "non-editable result boxes");
  }

  if (isGridPattern) {
    ImGui::TextColored(ImVec4(0.35f, 0.88f, 0.62f, 1.0f),
                       "GridPattern: ROI -> grid cells -> "
                       "pooled hierarchy -> "
                       "evidence overlay");
  }
  if (isRegionPattern) {
    ImGui::TextColored(ImVec4(0.9f, 0.72f, 0.32f, 1.0f),
                       "RegionPattern: ROI -> gray/binary "
                       "pooled descriptor -> "
                       "texture review signal");
  }
  ImGui::Separator();

  ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
  if (ImGui::CollapsingHeader("Geometry")) {
    ImGui::PushID("geometry");
    if (gauge.tool == "FindCircle" || gauge.has_circle_gauge) {
      bool circleGeometryEdited = false;
      ImGui::SetNextItemWidth(120.0f);
      circleGeometryEdited |= ImGui::InputInt("cx", &gauge.circle_cx);
      ImGui::SameLine();
      ImGui::SetNextItemWidth(120.0f);
      circleGeometryEdited |= ImGui::InputInt("cy", &gauge.circle_cy);
      ImGui::SameLine();
      ImGui::TextDisabled("annulus gauge");

      ImGui::SetNextItemWidth(120.0f);
      circleGeometryEdited |=
          ImGui::InputInt("inner_radius (Rin)", &gauge.inner_radius);
      ImGui::SameLine();
      ImGui::SetNextItemWidth(120.0f);
      circleGeometryEdited |=
          ImGui::InputInt("outer_radius (Rout)", &gauge.outer_radius);
      ImGui::SameLine();
      ImGui::Text("band_width: %d",
                  std::max(0, gauge.outer_radius - gauge.inner_radius));

      const bool scanSectorToggled =
          ImGui::Checkbox("use scan sector", &gauge.circle_arc_enabled);
      circleGeometryEdited |= scanSectorToggled;
      if (scanSectorToggled && gauge.circle_arc_enabled &&
          std::abs(gauge.circle_arc_end_deg - gauge.circle_arc_start_deg) >=
              360) {
        gauge.circle_arc_end_deg = gauge.circle_arc_start_deg + 45;
      }
      if (gauge.circle_arc_enabled) {
        ImGui::SetNextItemWidth(120.0f);
        circleGeometryEdited |=
            ImGui::InputInt("arc_start_deg (A0)", &gauge.circle_arc_start_deg);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        circleGeometryEdited |=
            ImGui::InputInt("arc_end_deg (A1)", &gauge.circle_arc_end_deg);
        ImGui::SameLine();
        ImGui::TextDisabled("signed degrees allowed");
      }

      gauge.outer_radius = std::max(1, gauge.outer_radius);
      gauge.inner_radius =
          std::max(0, std::min(gauge.inner_radius, gauge.outer_radius - 1));
      gauge.circle_arc_start_deg =
          std::max(-359, std::min(360, gauge.circle_arc_start_deg));
      gauge.circle_arc_end_deg =
          std::max(-359, std::min(360, gauge.circle_arc_end_deg));
      gauge.radius = gauge.outer_radius;
      gauge.circle_px = gauge.circle_cx + gauge.outer_radius;
      gauge.circle_py = gauge.circle_cy;
      gaugeEdited |= circleGeometryEdited;
      if (circleGeometryEdited) {
        context.apply_gauge_to_shape_requested = true;
      }
    } else if (isFindEllipse) {
      bool ellipseGeometryEdited = false;
      ImGui::TextDisabled("FindEllipse geometry is stored "
                          "as bbox corners: "
                          "setellipse(x0, y0, x1, y1).");
      ImGui::TextDisabled("Image View handles expose the "
                          "same bbox as derived "
                          "center/radius values.");
      ImGui::SetNextItemWidth(120.0f);
      ellipseGeometryEdited |= ImGui::InputInt("bbox x0", &gauge.ellipse_x0);
      ImGui::SameLine();
      ImGui::SetNextItemWidth(120.0f);
      ellipseGeometryEdited |= ImGui::InputInt("bbox y0", &gauge.ellipse_y0);
      ImGui::SameLine();
      ImGui::TextDisabled("bbox corner 0");

      ImGui::SetNextItemWidth(120.0f);
      ellipseGeometryEdited |= ImGui::InputInt("bbox x1", &gauge.ellipse_x1);
      ImGui::SameLine();
      ImGui::SetNextItemWidth(120.0f);
      ellipseGeometryEdited |= ImGui::InputInt("bbox y1", &gauge.ellipse_y1);
      ImGui::SameLine();
      ImGui::TextDisabled("bbox corner 1");

      int ellipseCx = (gauge.ellipse_x0 + gauge.ellipse_x1) / 2;
      int ellipseCy = (gauge.ellipse_y0 + gauge.ellipse_y1) / 2;
      int ellipseRx =
          std::max(1, std::abs(gauge.ellipse_x1 - gauge.ellipse_x0) / 2);
      int ellipseRy =
          std::max(1, std::abs(gauge.ellipse_y1 - gauge.ellipse_y0) / 2);
      bool centerRadiusEdited = false;
      ImGui::SetNextItemWidth(120.0f);
      centerRadiusEdited |= ImGui::InputInt("center cx", &ellipseCx);
      ImGui::SameLine();
      ImGui::SetNextItemWidth(120.0f);
      centerRadiusEdited |= ImGui::InputInt("center cy", &ellipseCy);
      ImGui::SameLine();
      ImGui::TextDisabled("derived from bbox");

      ImGui::SetNextItemWidth(120.0f);
      centerRadiusEdited |= ImGui::InputInt("radius rx", &ellipseRx);
      ImGui::SameLine();
      ImGui::SetNextItemWidth(120.0f);
      centerRadiusEdited |= ImGui::InputInt("radius ry", &ellipseRy);
      ImGui::SameLine();
      ImGui::TextDisabled("edits rewrite bbox");
      ImGui::SetNextItemWidth(120.0f);
      ellipseGeometryEdited |=
          ImGui::InputInt("inner scale %", &gauge.ellipse_inner_scale_percent);
      ImGui::SameLine();
      ImGui::Text(
          "annulus: inner rx=%d ry=%d",
          std::max(0, ellipseRx * gauge.ellipse_inner_scale_percent / 100),
          std::max(0, ellipseRy * gauge.ellipse_inner_scale_percent / 100));
      if (centerRadiusEdited) {
        ellipseRx = std::max(1, ellipseRx);
        ellipseRy = std::max(1, ellipseRy);
        gauge.ellipse_x0 = ellipseCx - ellipseRx;
        gauge.ellipse_y0 = ellipseCy - ellipseRy;
        gauge.ellipse_x1 = ellipseCx + ellipseRx;
        gauge.ellipse_y1 = ellipseCy + ellipseRy;
        ellipseGeometryEdited = true;
      }
      gauge.ellipse_inner_scale_percent =
          std::max(0, std::min(99, gauge.ellipse_inner_scale_percent));

      gaugeEdited |= ellipseGeometryEdited;
      if (ellipseGeometryEdited) {
        context.apply_gauge_to_shape_requested = true;
      }
    } else if (isFindRect) {
      bool rectGeometryEdited = false;
      ImGui::TextDisabled("FindRect extends FindLine "
                          "controls: editable seed "
                          "line/box with line-like scan "
                          "params.");
      ImGui::SetNextItemWidth(120.0f);
      rectGeometryEdited |= ImGui::InputInt("x0", &gauge.line_x0);
      ImGui::SameLine();
      ImGui::SetNextItemWidth(120.0f);
      rectGeometryEdited |= ImGui::InputInt("y0", &gauge.line_y0);
      ImGui::SameLine();
      ImGui::SetNextItemWidth(120.0f);
      rectGeometryEdited |=
          ImGui::InputInt("half_width", &gauge.tool_half_width);

      ImGui::SetNextItemWidth(120.0f);
      rectGeometryEdited |= ImGui::InputInt("x1", &gauge.line_x1);
      ImGui::SameLine();
      ImGui::SetNextItemWidth(120.0f);
      rectGeometryEdited |= ImGui::InputInt("y1", &gauge.line_y1);
      gauge.tool_half_width = std::max(1, gauge.tool_half_width);

      gaugeEdited |= rectGeometryEdited;
      if (rectGeometryEdited) {
        context.apply_gauge_to_shape_requested = true;
      }
    } else if (isGridPattern) {
      gaugeEdited |= DrawGridPatternRoiControls(context);
    } else if (isRegionPattern) {
      gaugeEdited |= DrawRegionPatternRoiControls(context);
} else if (isCxTextInspect) {
      const bool ocrEdited = DrawCxTextInspectControls(context);
      gaugeEdited |= ocrEdited;
      if (ocrEdited)
        context.apply_gauge_to_shape_requested = true;
    } else if (isFastMatch) {
      const bool fastMatchRoiEdited = DrawFastMatchRoiControls(context);
      gaugeEdited |= fastMatchRoiEdited;
      if (fastMatchRoiEdited) {
        context.apply_gauge_to_shape_requested = true;
      }
    } else if (isFindSegmentation) {
      const bool segPromptEdited = DrawFindSegmentationPromptControls(context);
      gaugeEdited |= segPromptEdited;
      if (segPromptEdited) {
        context.apply_gauge_to_shape_requested = true;
      }
    } else if (isFindObject) {
      const bool objectEdited = DrawFindObjectComponentControls(context);
      gaugeEdited |= objectEdited;
      if (objectEdited) {
        context.apply_gauge_to_shape_requested = true;
      }
    } else {
      ImGui::SetNextItemWidth(120.0f);
      gaugeEdited |= ImGui::InputInt("x0", &gauge.line_x0);
      ImGui::SameLine();
      ImGui::SetNextItemWidth(120.0f);
      gaugeEdited |= ImGui::InputInt("y0", &gauge.line_y0);
      ImGui::SameLine();
      ImGui::SetNextItemWidth(120.0f);
      gaugeEdited |= ImGui::InputInt("half_width", &gauge.tool_half_width);

      ImGui::SetNextItemWidth(120.0f);
      gaugeEdited |= ImGui::InputInt("x1", &gauge.line_x1);
      ImGui::SameLine();
      ImGui::SetNextItemWidth(120.0f);
      gaugeEdited |= ImGui::InputInt("y1", &gauge.line_y1);
    }
    ImGui::PopID();
  }

  if (!isFindSegmentation) {
    ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
    if (ImGui::CollapsingHeader("Edge Params")) {
      ImGui::PushID("edge_params");

      if (isGridPattern) {
        gaugeEdited |= DrawGridPatternParameterControls(context);
      } else if (isRegionPattern) {
        gaugeEdited |= DrawRegionPatternParameterControls(context);
} else if (isCxTextInspect) {
        ImGui::TextDisabled(
            "CxTextInspect detection, geometry/layout and decoding controls are "
            "grouped in Geometry.");
      } else if (isFastMatch) {
        gaugeEdited |= DrawFastMatchLearnParameterControls(context);
        ImGui::Separator();
        gaugeEdited |= DrawFastMatchMatchParameterControls(context);
      } else {
        ImGui::TextUnformatted("threshold");
        ImGui::SameLine(80.0f);
        ImGui::SetNextItemWidth(180.0f);
        gaugeEdited |=
            ImGui::SliderInt("##threshold", &gauge.threshold, 0, 255);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70.0f);
        gaugeEdited |= ImGui::InputInt("##t_val", &gauge.threshold);
        gauge.threshold = std::max(0, std::min(255, gauge.threshold));

        ImGui::TextUnformatted("method");
        ImGui::SameLine(80.0f);
        const char *methods[] = {"0", "1", "2", "3"};
        ImGui::SetNextItemWidth(100.0f);
        gaugeEdited |= ImGui::Combo("##method", &gauge.method, methods,
                                    IM_ARRAYSIZE(methods));

        ImGui::TextUnformatted("linegap");
        ImGui::SameLine(80.0f);
        ImGui::SetNextItemWidth(180.0f);
        gaugeEdited |= ImGui::SliderInt("##linegap", &gauge.linegap, 0, 50);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70.0f);
        gaugeEdited |= ImGui::InputInt("##lg_val", &gauge.linegap);
        gauge.linegap = std::max(0, std::min(50, gauge.linegap));

        if (isFindLine || isFindCircle || isFindEllipse) {
          ImGui::TextUnformatted("min edge run px");
          ImGui::SameLine(120.0f);
          ImGui::SetNextItemWidth(140.0f);
          gaugeEdited |= ImGui::SliderInt("##min_edge_run_width_px",
                                          &gauge.min_edge_run_width_px, 1, 20);
          ImGui::SameLine();
          ImGui::SetNextItemWidth(70.0f);
          gaugeEdited |= ImGui::InputInt("##min_edge_run_width_px_val",
                                         &gauge.min_edge_run_width_px);
          gauge.min_edge_run_width_px =
              std::max(1, std::min(20, gauge.min_edge_run_width_px));
        }

        const int findsettingDefault =
            (gauge.tool == "FindLine" || gauge.has_line_gauge || isFindEllipse)
                ? 1
                : 0;
        if (gauge.findsetting < 0) {
          gauge.findsetting = findsettingDefault;
        }
        gauge.findsetting = std::max(0, std::min(255, gauge.findsetting));
        bool objectPrefilter = (gauge.findsetting & 0x01) != 0;
        const char *findsettingMethodName =
            (gauge.tool == "FindLine" || gauge.has_line_gauge)
                ? "setfindsetting/"
                  "setobjfilter"
                : "setfindsetting";
        ImGui::TextUnformatted("findsetting");
        ImGui::SameLine(110.0f);
        bool findsettingEdited = false;
        if (ImGui::Checkbox("object "
                            "prefilter##findsetting_"
                            "prefilter",
                            &objectPrefilter)) {
          if (objectPrefilter)
            gauge.findsetting |= 0x01;
          else
            gauge.findsetting &= ~0x01;
          findsettingEdited = true;
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70.0f);
        findsettingEdited |=
            ImGui::InputInt("##findsetting_value", &gauge.findsetting);
        gauge.findsetting = std::max(0, std::min(255, gauge.findsetting));
        if (findsettingEdited) {
          StageObjectPrefilterFindSetting(context, "key parameter object "
                                                   "prefilter edited");
        }
        gaugeEdited |= findsettingEdited;
        ImGui::SameLine();
        ImGui::TextDisabled("%s bit0, default=%d", findsettingMethodName,
                            findsettingDefault);
        {
          const std::string normalizedTool = NormalizeKeyParamToolTypeLocal(
              !gauge.primary_object_type.empty() ? gauge.primary_object_type
                                                 : gauge.tool);
          const std::string toolKey =
              ToolFindSettingGlobalKey(gauge, normalizedTool);
          ImGui::TextDisabled(
              "staged globals: "
              "global_findsetting=%d | "
              "%s=%d",
              RuntimeIntOr(context, "global_findsetting", gauge.findsetting),
              toolKey.c_str(),
              RuntimeIntOr(context, toolKey, gauge.findsetting));
          if (isFindCircle) {
            ImGui::TextWrapped("FindCircle test: 0=direct "
                               "radial scan, 1=use "
                               "FindObject "
                               "prefilter before measure. "
                               " Run the same image/Gauge "
                               "twice "
                               "with only this value "
                               "changed, then compare "
                               "points, avgdist "
                               "and false hits.");
          } else {
            ImGui::TextDisabled("Run Script reapplies full "
                                "gauge globals before "
                                "measure(); "
                                "compare result points and "
                                "residual after changing "
                                "this "
                                "value.");
          }
        }

        if (isFindCircle || isFindEllipse || isFindObject) {
          if (isFindObject) {
            gauge.findobject_threshold = gauge.threshold;
            gauge.findobject_foreground_mode =
                std::max(1, std::min(3, gauge.method));

            ImGui::TextUnformatted("gap");
            ImGui::SameLine(80.0f);
            ImGui::SetNextItemWidth(180.0f);
            gaugeEdited |=
                ImGui::SliderInt("##findobject_gap", &gauge.gap, 0, 200);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(70.0f);
            gaugeEdited |= ImGui::InputInt("##findobject_gap_val", &gauge.gap);
            gauge.gap = std::max(0, std::min(200, gauge.gap));

            ImGui::TextUnformatted("filterprofile");
            ImGui::SameLine(110.0f);
            ImGui::SetNextItemWidth(180.0f);
            gaugeEdited |= ImGui::SliderInt("##findobject_"
                                            "filterprofile",
                                            &gauge.filterprofile, 0, 10);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(70.0f);
            gaugeEdited |= ImGui::InputInt("##findobject_"
                                           "filterprofile_val",
                                           &gauge.filterprofile);
            gauge.filterprofile =
                std::max(0, std::min(10, gauge.filterprofile));
          } else {
            ImGui::TextUnformatted("gap");
            ImGui::SameLine(80.0f);
            ImGui::SetNextItemWidth(180.0f);
            gaugeEdited |= ImGui::SliderInt("##gap", &gauge.gap, 0, 200);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(70.0f);
            gaugeEdited |= ImGui::InputInt("##gap_val", &gauge.gap);
            gauge.gap = std::max(0, std::min(200, gauge.gap));

            if (gauge.tool == "FindCircle" || gauge.has_circle_gauge) {
              const int annulusWidth =
                  std::max(1, gauge.outer_radius - gauge.inner_radius);
              const int circleConsistencyDefaultRange =
                  std::max(1, annulusWidth / 2);
              if (context.findcircle_point_consistency_range <= 0) {
                context.findcircle_point_consistency_range =
                    circleConsistencyDefaultRange;
              }

              bool circleConsistencyEnabled =
                  context.findcircle_point_consistency_enabled;
              if (ImGui::Checkbox("enable point "
                                  "consistency##"
                                  "findcircle_"
                                  "consistency",
                                  &circleConsistencyEnabled)) {
                context.findcircle_point_consistency_enabled =
                    circleConsistencyEnabled;
                if (context.findcircle_point_consistency_range <= 0) {
                  context.findcircle_point_consistency_range =
                      circleConsistencyDefaultRange;
                }
                gaugeEdited = true;
              }
              ImGui::TextUnformatted("consistency");
              ImGui::SameLine(110.0f);
              ImGui::SetNextItemWidth(180.0f);
              gaugeEdited |=
                  ImGui::SliderInt("##findcircle_"
                                   "consistency_range",
                                   &context.findcircle_point_consistency_range,
                                   1, std::max(1, annulusWidth));
              ImGui::SameLine();
              ImGui::SetNextItemWidth(70.0f);
              gaugeEdited |=
                  ImGui::InputInt("##findcircle_"
                                  "consistency_range_val",
                                  &context.findcircle_point_consistency_range);
              context.findcircle_point_consistency_range = std::max(
                  1,
                  std::min(10000, context.findcircle_point_consistency_range));
              ImGui::SameLine();
              ImGui::TextDisabled("default=%d", circleConsistencyDefaultRange);
            } else if (isFindEllipse) {
              const int ellipseRx =
                  std::abs(gauge.ellipse_x1 - gauge.ellipse_x0) / 2;
              const int ellipseRy =
                  std::abs(gauge.ellipse_y1 - gauge.ellipse_y0) / 2;
              const int ellipseConsistencyMax =
                  std::max(1, std::max(ellipseRx, ellipseRy));
              const int ellipseConsistencyDefaultRange =
                  std::max(1, std::max(1, gauge.gap) / 2);
              if (context.findellipse_point_consistency_range <= 0) {
                context.findellipse_point_consistency_range =
                    ellipseConsistencyDefaultRange;
              }

              bool ellipseConsistencyEnabled =
                  context.findellipse_point_consistency_enabled;
              if (ImGui::Checkbox("enable point "
                                  "consistency##"
                                  "findellipse_"
                                  "consistency",
                                  &ellipseConsistencyEnabled)) {
                context.findellipse_point_consistency_enabled =
                    ellipseConsistencyEnabled;
                if (context.findellipse_point_consistency_range <= 0) {
                  context.findellipse_point_consistency_range =
                      ellipseConsistencyDefaultRange;
                }
                gaugeEdited = true;
              }
              ImGui::TextUnformatted("consistency");
              ImGui::SameLine(110.0f);
              ImGui::SetNextItemWidth(180.0f);
              gaugeEdited |=
                  ImGui::SliderInt("##findellipse_"
                                   "consistency_range",
                                   &context.findellipse_point_consistency_range,
                                   1, ellipseConsistencyMax);
              ImGui::SameLine();
              ImGui::SetNextItemWidth(70.0f);
              gaugeEdited |=
                  ImGui::InputInt("##findellipse_"
                                  "consistency_range_val",
                                  &context.findellipse_point_consistency_range);
              context.findellipse_point_consistency_range = std::max(
                  1,
                  std::min(10000, context.findellipse_point_consistency_range));
              ImGui::SameLine();
              ImGui::TextDisabled("default=%d", ellipseConsistencyDefaultRange);
            }
          }
        } else {
          gauge.scan_direction = gauge.scan_direction == 1 ? 1 : 2;
          ImGui::TextUnformatted("scan direction");
          ImGui::SameLine(110.0f);
          if (ImGui::RadioButton("W only##scan_w", gauge.scan_direction == 1)) {
            gauge.scan_direction = 1;
            gaugeEdited = true;
          }
          ImGui::SameLine();
          if (ImGui::RadioButton("H only##scan_h", gauge.scan_direction == 2)) {
            gauge.scan_direction = 2;
            gaugeEdited = true;
          }

          ImGui::BeginDisabled(gauge.scan_direction != 1);
          ImGui::TextUnformatted("wgap");
          ImGui::SameLine(80.0f);
          ImGui::SetNextItemWidth(180.0f);
          gaugeEdited |= ImGui::SliderInt("##wgap", &gauge.wgap, 0, 50);
          ImGui::SameLine();
          ImGui::SetNextItemWidth(70.0f);
          gaugeEdited |= ImGui::InputInt("##wg_val", &gauge.wgap);
          gauge.wgap = std::max(0, std::min(50, gauge.wgap));
          ImGui::EndDisabled();

          ImGui::BeginDisabled(gauge.scan_direction != 2);
          ImGui::TextUnformatted("hgap");
          ImGui::SameLine(80.0f);
          ImGui::SetNextItemWidth(180.0f);
          gaugeEdited |= ImGui::SliderInt("##hgap", &gauge.hgap, 0, 50);
          ImGui::SameLine();
          ImGui::SetNextItemWidth(70.0f);
          gaugeEdited |= ImGui::InputInt("##hg_val", &gauge.hgap);
          gauge.hgap = std::max(0, std::min(50, gauge.hgap));
          ImGui::EndDisabled();

          const int consistencyDefaultRange =
              std::max(1, gauge.tool_half_width / 2);
          if (context.findline_point_consistency_range <= 0) {
            context.findline_point_consistency_range = consistencyDefaultRange;
          }

          bool consistencyEnabled = context.findline_point_consistency_enabled;
          if (ImGui::Checkbox("enable point "
                              "consistency##findline_"
                              "consistency",
                              &consistencyEnabled)) {
            context.findline_point_consistency_enabled = consistencyEnabled;
            if (context.findline_point_consistency_range <= 0) {
              context.findline_point_consistency_range =
                  consistencyDefaultRange;
            }
            gaugeEdited = true;
          }
          ImGui::TextUnformatted("consistency");
          ImGui::SameLine(110.0f);
          ImGui::SetNextItemWidth(180.0f);
          gaugeEdited |=
              ImGui::SliderInt("##findline_consistency_"
                               "range",
                               &context.findline_point_consistency_range, 1,
                               std::max(1, gauge.tool_half_width));
          ImGui::SameLine();
          ImGui::SetNextItemWidth(70.0f);
          gaugeEdited |=
              ImGui::InputInt("##findline_consistency_"
                              "range_val",
                              &context.findline_point_consistency_range);
          context.findline_point_consistency_range = std::max(
              1, std::min(10000, context.findline_point_consistency_range));
          ImGui::SameLine();
          ImGui::TextDisabled("default=%d", consistencyDefaultRange);

          ImGui::TextUnformatted("filterprofile");
          ImGui::SameLine(80.0f);
          ImGui::SetNextItemWidth(180.0f);
          gaugeEdited |= ImGui::SliderInt("##fp", &gauge.filterprofile, 0, 10);
          ImGui::SameLine();
          ImGui::SetNextItemWidth(70.0f);
          gaugeEdited |= ImGui::InputInt("##fp_val", &gauge.filterprofile);
          gauge.filterprofile = std::max(0, std::min(10, gauge.filterprofile));
        }
      }
      ImGui::PopID();
    }
  } else {
    ImGui::TextDisabled("FindSegmentation uses Prompt ROI, "
                        "mode, threshold and "
                        "one active prompt point. "
                        "FindLine edge/gap/filter controls "
                        "are intentionally "
                        "hidden for this tool.");
  }

  (void)DrawMetrologyExtensionPanel(context, parserDebugBridge);

  if (gaugeEdited) {
    gauge.dirty = true;
    gauge.review_status = "editing";
    ++context.key_parameter_edit_revision;
    if (isFindSegmentation) {
      context.last_key_parameter_edit_summary =
          "prompt_roi=(" + std::to_string(gauge.segmentation_prompt_x0) + "," +
          std::to_string(gauge.segmentation_prompt_y0) + "," +
          std::to_string(gauge.segmentation_prompt_x1) + "," +
          std::to_string(gauge.segmentation_prompt_y1) + ")" +
          " mode=" + std::to_string(gauge.segmentation_mode) +
          " threshold_percent=" +
          std::to_string(gauge.segmentation_threshold_percent) + " positive=" +
          std::to_string(gauge.has_segmentation_positive_point ? 1 : 0) + "(" +
          std::to_string(gauge.segmentation_positive_x) + "," +
          std::to_string(gauge.segmentation_positive_y) + ")" + " negative=" +
          std::to_string(gauge.has_segmentation_negative_point ? 1 : 0) + "(" +
          std::to_string(gauge.segmentation_negative_x) + "," +
          std::to_string(gauge.segmentation_negative_y) + ")" +
          " active_pick=" + std::to_string(gauge.segmentation_prompt_pick_mode);
    } else {
      context.last_key_parameter_edit_summary =
          "threshold=" + std::to_string(gauge.threshold) +
          " method=" + std::to_string(gauge.method) +
          " linegap=" + std::to_string(gauge.linegap) +
          " findsetting=" + std::to_string(gauge.findsetting);
    }
    if (gauge.tool == "FindCircle" || gauge.has_circle_gauge) {
      context.last_key_parameter_edit_summary +=
          " gap=" + std::to_string(gauge.gap) + " consistency=" +
          std::to_string(context.findcircle_point_consistency_enabled ? 1 : 0) +
          "/" + std::to_string(context.findcircle_point_consistency_range) +
          " selected_edge=" +
          std::to_string(context.findcircle_selected_scan_edge) + "/" +
          std::to_string(context.findcircle_scan_edge_count) +
          " best_edge=" + std::to_string(context.findcircle_best_fit_edge) +
          " recommended_edge=" +
          std::to_string(context.findcircle_recommended_fit_edge) +
          " relation_edge=" + std::to_string(context.findcircle_relation_edge) +
          " attach_edge=" + std::to_string(context.findcircle_attach_edge) +
          " circle=(" + std::to_string(gauge.circle_cx) + "," +
          std::to_string(gauge.circle_cy) + "," +
          std::to_string(gauge.circle_px) + "," +
          std::to_string(gauge.circle_py) + ")";
    } else if (isFindEllipse) {
      context.last_key_parameter_edit_summary +=
          " gap=" + std::to_string(gauge.gap) + " consistency=" +
          std::to_string(context.findellipse_point_consistency_enabled ? 1
                                                                       : 0) +
          "/" + std::to_string(context.findellipse_point_consistency_range) +
          " selected_edge=" +
          std::to_string(context.findellipse_selected_scan_edge) + "/" +
          std::to_string(context.findellipse_scan_edge_count) +
          " best_edge=" + std::to_string(context.findellipse_best_fit_edge) +
          " recommended_edge=" +
          std::to_string(context.findellipse_recommended_fit_edge) +
          " relation_edge=" +
          std::to_string(context.findellipse_relation_edge) +
          " attach_edge=" + std::to_string(context.findellipse_attach_edge) +
          " ellipse=(" + std::to_string(gauge.ellipse_x0) + "," +
          std::to_string(gauge.ellipse_y0) + "," +
          std::to_string(gauge.ellipse_x1) + "," +
          std::to_string(gauge.ellipse_y1) + ")";
    } else if (isFastMatch || isGridPattern || isRegionPattern) {

      if (isRegionPattern) {
        context.last_key_parameter_edit_summary +=
            " region_roi=(" +
            std::to_string(RuntimeIntOr(context, "global_region_roi_x", 120)) +
            "," +
            std::to_string(RuntimeIntOr(context, "global_region_roi_y", 120)) +
            "," +
            std::to_string(RuntimeIntOr(context, "global_region_roi_w", 120)) +
            "," +
            std::to_string(RuntimeIntOr(context, "global_region_roi_h", 90)) +
            ")" + " pooling=" +
            std::to_string(RuntimeIntOr(context, "global_region_pooling_rows", 4)) +
            "x" +
            std::to_string(RuntimeIntOr(context, "global_region_pooling_cols", 4)) +
            " binary=" +
            std::to_string(RuntimeIntOr(context, "global_region_use_binary", 0)) +
            " threshold=" +
            std::to_string(RuntimeIntOr(context, "global_region_threshold", 128));
      } else {
        context.last_key_parameter_edit_summary +=
            " learn_roi=(" +
            std::to_string(RuntimeIntOr(context, "global_learn_roi_x", 120)) +
            "," +
            std::to_string(RuntimeIntOr(context, "global_learn_roi_y", 120)) +
            "," +
            std::to_string(RuntimeIntOr(context, "global_learn_roi_w", 120)) +
            "," +
            std::to_string(RuntimeIntOr(context, "global_learn_roi_h", 90)) +
            ")" + " search_roi=(" +
            std::to_string(RuntimeIntOr(context, "global_search_roi_x", 0)) +
            "," +
            std::to_string(RuntimeIntOr(context, "global_search_roi_y", 0)) +
            "," +
            std::to_string(RuntimeIntOr(context, "global_search_roi_w", 640)) +
            "," +
            std::to_string(RuntimeIntOr(context, "global_search_roi_h", 480)) +
            ")" + " wgap=" + std::to_string(gauge.wgap) +
            " hgap=" + std::to_string(gauge.hgap) +
            " filterprofile=" + std::to_string(gauge.filterprofile) +
            " match_step=(" +
            std::to_string(RuntimeIntOr(context, "global_match_step_x", 10)) +
            "," +
            std::to_string(RuntimeIntOr(context, "global_match_step_y", 10)) +
            ")" + " match_thre=" +
            std::to_string(RuntimeIntOr(context, "global_match_thre", 10)) +
            " min_score=" +
            std::to_string(RuntimeIntOr(
                context, "global_min_score",
                RuntimeIntOr(context, "global_min_score_percent", 65))) +
            " min_score_percent=" +
            std::to_string(RuntimeIntOr(context, "global_min_score_percent", 65)) +
            " find_num=" +
            std::to_string(RuntimeIntOr(context, "global_find_num", 1)) +
            " action=" +
            std::to_string(RuntimeIntOr(context, "global_fastmatch_action", 0));

        if (isFastMatch) {
          context.last_key_parameter_edit_summary +=
              " scan_rotation_deg=" +
              std::to_string(RuntimeIntOr(
                  context, "global_fastmatch_scan_rotation_deg", 0)) +
              " compare_gap_shared=" +
              std::to_string(RuntimeIntOr(context, "global_compare_gap", 20)) +
              " learn_shared=" +
              std::to_string(RuntimeIntOr(
                  context, "global_fastmatch_learn_shared", 1)) +
              " compare_gap_directions=(" +
              std::to_string(RuntimeIntOr(
                  context, "global_fastmatch_learn_compare_gap_0", 20)) +
              "," +
              std::to_string(RuntimeIntOr(
                  context, "global_fastmatch_learn_compare_gap_1", 20)) +
              "," +
              std::to_string(RuntimeIntOr(
                  context, "global_fastmatch_learn_compare_gap_2", 20)) +
              "," +
              std::to_string(RuntimeIntOr(
                  context, "global_fastmatch_learn_compare_gap_3", 20)) +
              ")";
        }
        if (isGridPattern) {
          context.last_key_parameter_edit_summary +=
              " grid=" +
              std::to_string(RuntimeIntOr(context, "global_grid_rows", 12)) +
              "x" +
              std::to_string(RuntimeIntOr(context, "global_grid_cols", 12)) +
              " orientation_bins=" +
              std::to_string(RuntimeIntOr(context,
                                          "global_grid_orientation_bins", 8)) +
              " fusion_mode=" +
              std::to_string(RuntimeIntOr(context, "global_grid_fusion_mode", 2));
        }
      }

    } else if (isFindSegmentation) {
    } else {
      context.last_key_parameter_edit_summary +=
          " wgap=" + std::to_string(gauge.wgap) +
          " hgap=" + std::to_string(gauge.hgap) +
          " scan_direction=" + std::to_string(gauge.scan_direction) +
          " filterprofile=" + std::to_string(gauge.filterprofile) +
          " findsetting=" + std::to_string(gauge.findsetting) +
          " selected_edge=" +
          FindLineSelectedEdgeSummary(context.findline_selected_scan_edge,
                                      context.findline_scan_edge_count) +
          " best_edge=" + std::to_string(context.findline_best_fit_edge) +
          " recommended_edge=" +
          std::to_string(context.findline_recommended_fit_edge) +
          " relation_edge=" + std::to_string(context.findline_relation_edge) +
          " attach_edge=" + std::to_string(context.findline_attach_edge) +
          " roi=(" + std::to_string(gauge.line_x0) + "," +
          std::to_string(gauge.line_y0) + "," + std::to_string(gauge.line_x1) +
          "," + std::to_string(gauge.line_y1) + ")";
    }
    CXLOG_INFO(
        "KeyParameterControls", "key_parameter_ui_edit", "edited",
        "revision=" + std::to_string(context.key_parameter_edit_revision) +
            " " + context.last_key_parameter_edit_summary);

    RecordManualOperationTraceEvent(
        context, "manual_parameter_edit", "edited",
        context.last_key_parameter_edit_summary);
  }

  ImGui::Separator();
  if (isFastMatch) {
    ImGui::TextDisabled(
        "Template defaults are the four directional values saved in this "
        "candidate's runtime_globals and parameter_snapshot. Loading the "
        "candidate restores them; no directional value is fixed in code.");
  }
  if (isFastMatch) {
    ImGui::TextUnformatted("FastMatch Actions");
    const float fmBtnWidth = (ImGui::GetContentRegionAvail().x - 20.0f) / 3.0f;
    const bool normalTraceBindingBlocked =
        RuntimeIntOr(context, "global_fastmatch_normaltrace_enabled", 0) != 0 &&
        !FastMatchScriptSupportsNormalTrace(context);
    if (normalTraceBindingBlocked) {
      ImGui::TextColored(
          ImVec4(0.90f, 0.70f, 0.12f, 1.0f),
          "Compatibility migration is pending and will run automatically on Learn.");
      ImGui::TextDisabled(
          "Evidence loading stays read-only. Match does not create a model or "
          "silently run Learn.");
    }
    ImGui::PushID("fastmatch_actions");
    if (ImGui::Button("Learn", ImVec2(fmBtnWidth, 0)))
      RequestFastMatchRunAction(context, 1, "FastMatch Learn");
    ImGui::SameLine();
    if (ImGui::Button("Match", ImVec2(fmBtnWidth, 0)))
      RequestFastMatchRunAction(context, 2, "FastMatch Match");
    ImGui::SameLine();
    if (ImGui::Button("Learn + Match", ImVec2(fmBtnWidth, 0)))
      RequestFastMatchRunAction(context, 3, "FastMatch Learn + Match");
    ImGui::PopID();
    const unsigned long long editedRevision =
        context.key_parameter_edit_revision;
    const unsigned long long submittedRevision =
        context.fastmatch_last_requested_revision;
    const bool revisionNotSubmitted = editedRevision > submittedRevision;
    const bool executionPending = context.has_pending_execution_snapshot ||
                                  context.run_state == "running";
    ImGui::Text("Parameter revision: edited=%llu | submitted=%llu",
                editedRevision, submittedRevision);
    if (revisionNotSubmitted) {
      ImGui::TextColored(
          ImVec4(0.95f, 0.68f, 0.15f, 1.0f),
          "PARAMETERS_NOT_RUN: revision %llu changed. Click Learn or Learn + Match to apply it.",
          editedRevision);
    } else if (submittedRevision > 0 && executionPending) {
      ImGui::TextColored(
          ImVec4(0.35f, 0.78f, 1.0f, 1.0f),
          "REVISION_QUEUED: revision %llu is queued/running (action=%d).",
          submittedRevision, context.fastmatch_last_requested_action);
    } else if (submittedRevision > 0) {
      ImGui::TextColored(
          ImVec4(0.42f, 0.85f, 0.52f, 1.0f),
          "REVISION_SUBMITTED: revision %llu was submitted. Runtime status: %s",
          submittedRevision, context.debug_status.c_str());
    } else {
      ImGui::TextDisabled(
          "No edited parameter revision has been submitted in this Evidence session.");
    }
    const int sharedLearnMode =
        RuntimeIntOr(context, "global_fastmatch_learn_shared", 1);
    ImGui::TextWrapped(
        "Effective compare gap: mode=%s | shared=%d | Top=%d Bottom=%d Left=%d Right=%d",
        sharedLearnMode != 0 ? "shared" : "directional",
        RuntimeIntOr(context, "global_compare_gap", 20),
        RuntimeIntOr(context, "global_fastmatch_learn_compare_gap_0", 20),
        RuntimeIntOr(context, "global_fastmatch_learn_compare_gap_1", 20),
        RuntimeIntOr(context, "global_fastmatch_learn_compare_gap_2", 20),
        RuntimeIntOr(context, "global_fastmatch_learn_compare_gap_3", 20));
    if (submittedRevision > 0 && !executionPending) {
      const RuntimeObjectView *latestObject = FindCurrentFastMatchObject(context);
      if (latestObject == nullptr) {
        ImGui::TextColored(
            ImVec4(0.95f, 0.45f, 0.35f, 1.0f),
            "DISPLAY_UNAVAILABLE: revision %llu has no current FastMatch runtime object.",
            submittedRevision);
      } else {
        ImGui::Text("Latest Learn display (revision %llu):",
                    submittedRevision);
        if (context.show_fastmatch_learn_result) {
          const int pairCount = std::min(latestObject->fastmatch_pattern_a_count,
                                         latestObject->fastmatch_pattern_b_count);
          ImGui::BulletText("Learn Result: %s (%d pairs, status=%d)",
                            pairCount > 0 ? "VISIBLE" : "UNAVAILABLE",
                            pairCount,
                            latestObject->fastmatch_learn_status_code);
        }
        if (context.show_fastmatch_normal_trace_domain) {
          ImGui::BulletText(
              "De-duplicated Domain: %s (%d points)",
              latestObject->fastmatch_normal_trace_domain_count > 0
                  ? "VISIBLE"
                  : "UNAVAILABLE",
              latestObject->fastmatch_normal_trace_domain_count);
        }
        if (context.show_fastmatch_normal_trace_ann_component) {
          int annSelected = 0;
          for (int count : latestObject->fastmatch_normal_trace_ann_selected_counts)
            annSelected += count;
          ImGui::BulletText("ANN Selected Component: %s (%d points)",
                            annSelected > 0 ? "VISIBLE" : "UNAVAILABLE",
                            annSelected);
        }
        if (context.show_fastmatch_normal_trace_path) {
          ImGui::BulletText(
              "Dijkstra Trace: %s (%d points)",
              latestObject->fastmatch_normal_trace_path_count >= 2
                  ? "VISIBLE"
                  : "UNAVAILABLE",
              latestObject->fastmatch_normal_trace_path_count);
        }
        if (context.show_fastmatch_normal_trace_pairs) {
          ImGui::BulletText(
              "Normal A/B Pairs: %s (%d pairs)",
              latestObject->fastmatch_normal_trace_pair_count > 0
                  ? "VISIBLE"
                  : "UNAVAILABLE",
              latestObject->fastmatch_normal_trace_pair_count);
        }
        if ((context.show_fastmatch_normal_trace_path &&
             latestObject->fastmatch_normal_trace_path_count < 2) ||
            (context.show_fastmatch_normal_trace_pairs &&
             latestObject->fastmatch_normal_trace_pair_count <= 0)) {
          ImGui::TextWrapped("Selected Normal-Trace layer failure: %s",
                             latestObject->fastmatch_normal_trace_reason.c_str());
          if (context.show_fastmatch_learn_result &&
              latestObject->fastmatch_pattern_a_count > 0 &&
              latestObject->fastmatch_pattern_b_count > 0) {
            ImGui::TextDisabled(
                "The latest fallback Learn model is shown by Learn Result; it "
                "is not mislabeled as a Dijkstra/Normal-Trace result.");
          }
        }
      }
    }
    if (!context.last_key_parameter_edit_summary.empty()) {
      ImGui::TextWrapped("Last edit: %s",
                         context.last_key_parameter_edit_summary.c_str());
    }
    ImGui::Separator();
  }
  ImGui::TextUnformatted("Actions");

  const float btnWidth = (ImGui::GetContentRegionAvail().x - 30.0f) / 3.0f;

  ImGui::PushID("actions");
  if (ImGui::Button("Apply To Gauge", ImVec2(btnWidth, 0))) {
    gauge.dirty = true;
    gauge.review_status = "editing";
    context.apply_gauge_to_shape_requested = true;
  }
  ImGui::SameLine();
  if (ImGui::Button("Apply To Globals", ImVec2(btnWidth, 0))) {
    RestoreObjectPrefilterFindSettingFromStagedGlobals(
        context, "apply globals restored "
                 "staged object prefilter");
    if (ApplyManualGaugeToGlobals(context)) {
      RecordManualOperationTraceEvent(
          context, "manual_apply_to_globals", "applied",
          context.last_key_parameter_edit_summary.empty()
              ? "Apply To Globals button"
              : context.last_key_parameter_edit_summary);
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Run Script", ImVec2(btnWidth, 0))) {
    gauge.dirty = true;
    gauge.review_status = "editing";
    context.debug_action = "Key Parameter Controls Run Script";
    RestoreObjectPrefilterFindSettingFromStagedGlobals(
        context, "run script restored "
                 "staged object prefilter");
    if (ApplyManualGaugeToGlobals(context)) {
      context.pending_execution_gauge = context.current_gauge;
      context.pending_execution_globals = context.runtime_int_vars;
      context.has_pending_execution_snapshot = true;
      context.debug_status = "MANUAL_RUN_REQUESTED";
      context.debug_reason = "Run requested from Key "
                             "Parameter Controls; Debug "
                             "Compiler will execute exact "
                             "Script Editor text";
      context.run_state = "running";
    }
  }

  const char *saveDraftLabel = isFastMatch
      ? "Save Template Defaults"
      : "Save Draft Candidate";
  if (ImGui::Button(saveDraftLabel, ImVec2(btnWidth, 0))) {
    gauge.dirty = true;
    gauge.review_status = "editing";
    context.debug_action = isFastMatch
        ? "Save FastMatch Template Defaults"
        : "Save Evidence Candidate";
    RestoreObjectPrefilterFindSettingFromStagedGlobals(
        context, "save draft restored "
                 "staged object prefilter");
    if (ApplyManualGaugeToGlobals(context)) {
      context.pending_candidate_save_gauge = context.current_gauge;
      context.pending_candidate_save_globals = context.runtime_int_vars;
      context.pending_candidate_save_requests_run = false;
      context.has_pending_candidate_save = true;
      context.debug_status = "EVIDENCE_CANDIDATE_SAVE_QUEUED";
      context.debug_reason =
          isFastMatch
              ? "Template defaults will persist to the active candidate "
                "runtime_globals and parameter_snapshot after this frame."
              : "Draft save will persist after the current Key Parameter "
                "Controls frame finishes.";
    }
  }
  ImGui::SameLine();
  const char *saveAndRunLabel = isFastMatch
      ? "Save Defaults + Run"
      : "Save And Run Candidate";
  if (ImGui::Button(saveAndRunLabel, ImVec2(btnWidth, 0))) {
    gauge.dirty = true;
    gauge.review_status = "editing";
    context.debug_action = isFastMatch
        ? "Save FastMatch Defaults And Run"
        : "Save And Run Evidence Candidate";
    RestoreObjectPrefilterFindSettingFromStagedGlobals(
        context, "save and run restored "
                 "staged object prefilter");
    if (ApplyManualGaugeToGlobals(context)) {
      // Freeze exactly what was saved.  The
      // Debug Compiler executes on a later
      // UI pass, after Evidence rows and
      // runtime results may refresh.
      context.pending_execution_gauge = context.current_gauge;
      context.pending_execution_globals = context.runtime_int_vars;
      context.has_pending_execution_snapshot = true;
      context.pending_candidate_save_gauge = context.current_gauge;
      context.pending_candidate_save_globals = context.runtime_int_vars;
      context.pending_candidate_save_requests_run = true;
      context.has_pending_candidate_save = true;
      context.debug_status = "EVIDENCE_CANDIDATE_SAVE_AND_RUN_QUEUED";
      context.debug_reason =
          "Candidate save/run will persist after the current Key Parameter "
          "Controls frame finishes.";
    }
  }

  const char *resetLabel = isFastMatch ? "Reset Generic UI" : "Reset";
  if (ImGui::Button(resetLabel, ImVec2(btnWidth, 0))) {
    ResetKeyParameterUiDefaults(context);
    SyncKeyParameterUiToGauge(context);
  }
  ImGui::PopID();
}

void DrawParamTuningScatterPanel(ManualTestContext &context) {
  if (!ImGui::CollapsingHeader("参数整定图 / Parameter Tuning "
                               "Map",
                               ImGuiTreeNodeFlags_DefaultOpen))
    return;

  ManualParamRegressionState &reg = context.param_regression;
  const char *tabs[] = {"Tuning", "Reading", "Test", "LastTest", "Search"};
  for (int i = 0; i < IM_ARRAYSIZE(tabs); ++i) {
    if (i > 0)
      ImGui::SameLine();
    if (ImGui::Selectable(tabs[i], reg.tuning_tab == i, 0, ImVec2(74, 0)))
      reg.tuning_tab = i;
  }

  ImGui::TextWrapped("Scatter view uses current parameter "
                     "candidates. X=threshold, "
                     "Y=predicted quality/risk score. "
                     "Selected candidates are "
                     "highlighted.");

  const ImVec2 plotSize(520.0f, 260.0f);
  ImVec2 p0 = ImGui::GetCursorScreenPos();
  ImVec2 p1(p0.x + plotSize.x, p0.y + plotSize.y);
  ImDrawList *draw = ImGui::GetWindowDrawList();
  draw->AddRectFilled(p0, p1, IM_COL32(18, 18, 18, 255));
  draw->AddRect(p0, p1, IM_COL32(230, 230, 230, 255));

  for (int gx = 0; gx <= 10; ++gx) {
    const float x = p0.x + plotSize.x * gx / 10.0f;
    draw->AddLine(ImVec2(x, p0.y), ImVec2(x, p1.y), IM_COL32(80, 80, 80, 180));
  }
  for (int gy = 0; gy <= 10; ++gy) {
    const float y = p0.y + plotSize.y * gy / 10.0f;
    draw->AddLine(ImVec2(p0.x, y), ImVec2(p1.x, y), IM_COL32(80, 80, 80, 180));
  }

  auto mapX = [&](int threshold) {
    const float t =
        static_cast<float>(std::max(0, std::min(100, threshold))) / 100.0f;
    return p0.x + 36.0f + t * (plotSize.x - 54.0f);
  };
  auto mapY = [&](double quality, double risk) {
    double score = quality > 0.0 ? quality : (1.0 - risk);
    score = std::max(0.0, std::min(1.0, score));
    return p1.y - 28.0f - static_cast<float>(score) * (plotSize.y - 52.0f);
  };

  const ImU32 colors[] = {
      IM_COL32(255, 90, 90, 255), IM_COL32(80, 210, 255, 255),
      IM_COL32(120, 255, 120, 255), IM_COL32(255, 230, 80, 255),
      IM_COL32(255, 150, 255, 255)};

  for (std::size_t i = 0; i < reg.candidates.size(); ++i) {
    const CxParamCandidate &c = reg.candidates[i];
    const float x = mapX(c.threshold);
    const float y = mapY(c.predicted_quality, c.predicted_risk);
    const float radius = c.selected_for_probe ? 5.5f : 3.5f;
    draw->AddCircleFilled(ImVec2(x, y), radius,
                          colors[i % IM_ARRAYSIZE(colors)]);
    if (static_cast<int>(i) == reg.selected_candidate_index)
      draw->AddCircle(ImVec2(x, y), radius + 4.0f, IM_COL32(255, 255, 255, 255),
                      16, 2.0f);
  }

  draw->AddText(ImVec2(p0.x + 8.0f, p0.y + 6.0f), IM_COL32(240, 240, 240, 255),
                "Quality / Risk");
  draw->AddText(ImVec2(p1.x - 110.0f, p1.y - 22.0f),
                IM_COL32(240, 240, 240, 255), "Threshold");
  ImGui::Dummy(plotSize);

  ImGui::SameLine();
  ImGui::BeginGroup();
  ImGui::TextUnformatted("After Parameter");
  ImGui::SetNextItemWidth(90.0f);
  ImGui::SliderInt("Edge", &reg.edge_mode, 0, 2);
  ImGui::SetNextItemWidth(90.0f);
  ImGui::SliderInt("Scale", &reg.contrast_percent, 0, 100);
  ImGui::SetNextItemWidth(90.0f);
  ImGui::SliderInt("X", &reg.valid_length_percent, 0, 100);
  ImGui::SetNextItemWidth(90.0f);
  ImGui::SliderInt("Y", &reg.interference_length_percent, 0, 100);
  ImGui::RadioButton("Visual", reg.tuning_tab == 0);
  ImGui::RadioButton("Param", reg.tuning_tab == 1);
  ImGui::RadioButton("Grid", reg.tuning_tab == 2);
  if (ImGui::Button("Reset Transform"))
    ResetKeyParameterUiDefaults(context);
  if (ImGui::Button("Animate")) {
    context.debug_action = "Tuning Map Animate";
    context.debug_status = "PENDING";
    context.debug_reason = "visual placeholder; no runtime "
                           "execution";
  }
  if (ImGui::Button("Show Source"))
    context.source_preview_enabled = true;
  if (ImGui::Button("What?")) {
    context.debug_action = "Tuning Map Help";
    context.debug_status = "PENDING";
    context.debug_reason = "Tuning map plots candidates. "
                           "White ring means focused "
                           "candidate.";
  }
  ImGui::EndGroup();

  ImGui::Text("Candidates=%d Selected=%d Focus=%d",
              static_cast<int>(reg.candidates.size()),
              CountSelectedParamCandidates(context),
              reg.selected_candidate_index);
}

namespace {

static std::filesystem::path GeometryAutoTunePolicyPathLocal() {
  return ResolveWorkspaceFile(
      "cxparser/cxscript/module/cximage/evidence/"
      "torch_geometry_incremental_auto_tune_policy_v1.json");
}

static std::filesystem::path GeometryAutoTuneOntologyPathLocal() {
  return ResolveWorkspaceFile(
      "cxparser/cxscript/module/cximage/evidence/"
      "torch_geometry_ontology_frozen.json");
}

static std::string GeometryAutoTuneDoubleListLocal(const cv::FileNode &node) {
  std::ostringstream text;
  bool first = true;
  if (!node.isSeq())
    return "ASSET_MISSING";
  for (const cv::FileNode &item : node) {
    double value = 0.0;
    item >> value;
    if (!first)
      text << ", ";
    first = false;
    text << std::setprecision(4) << value;
  }
  return first ? "ASSET_MISSING" : text.str();
}

static std::string GeometryAutoTuneIntListLocal(const cv::FileNode &node) {
  std::ostringstream text;
  bool first = true;
  if (!node.isSeq())
    return "ASSET_MISSING";
  for (const cv::FileNode &item : node) {
    int value = 0;
    item >> value;
    if (!first)
      text << ", ";
    first = false;
    text << value;
  }
  return first ? "ASSET_MISSING" : text.str();
}

static bool LoadGeometryAutoTunePolicyLocal(ManualTestContext &context,
                                            bool reset_values,
                                            std::string &reason) {
  const std::filesystem::path policyPath = GeometryAutoTunePolicyPathLocal();
  const std::filesystem::path ontologyPath = GeometryAutoTuneOntologyPathLocal();
  cv::FileStorage policy(policyPath.string(), cv::FileStorage::READ);
  cv::FileStorage ontology(ontologyPath.string(), cv::FileStorage::READ);
  if (!policy.isOpened() || !ontology.isOpened()) {
    reason = "AUTO_TUNE_POLICY_OR_ONTOLOGY_ASSET_MISSING";
    return false;
  }
  const std::string policySchema = static_cast<std::string>(policy["schema"]);
  const std::string ontologySchema = static_cast<std::string>(ontology["schema"]);
  const std::string requiredOntology =
      static_cast<std::string>(policy["required_ontology_schema"]);
  int requiredClassCount = 0;
  int sourceTopK = 0;
  double sourceConfidence = 0.0;
  double matchIou = 0.0;
  policy["required_class_count"] >> requiredClassCount;
  policy["source_profile_topk"] >> sourceTopK;
  policy["source_profile_confidence_threshold"] >> sourceConfidence;
  policy["match_iou_threshold"] >> matchIou;
  const cv::FileNode confidenceCandidates =
      policy["confidence_threshold_candidates"];
  const cv::FileNode topkCandidates = policy["topk_candidates"];
  const cv::FileNode classes = ontology["classes"];
  if (policySchema != "cxvision.torch_geometry_incremental_auto_tune_policy.v1" ||
      ontologySchema != requiredOntology || requiredClassCount <= 0 ||
      !confidenceCandidates.isSeq() || !topkCandidates.isSeq() ||
      !classes.isSeq() || static_cast<int>(classes.size()) != requiredClassCount ||
      sourceTopK <= 0 || sourceConfidence < 0.0 || sourceConfidence > 1.0 ||
      matchIou <= 0.0 || matchIou > 1.0) {
    reason = "AUTO_TUNE_POLICY_SCHEMA_OR_CLASS_MAPPING_INVALID";
    return false;
  }
  if (reset_values || !context.geometry_auto_tune_policy_loaded) {
    context.yolo_training_postprocess_confidence_threshold =
        static_cast<float>(sourceConfidence);
    context.geometry_auto_tune_topk_per_roi = sourceTopK;
    context.geometry_auto_tune_match_iou_threshold =
        static_cast<float>(matchIou);
    context.geometry_auto_tune_max_trials =
        static_cast<int>(confidenceCandidates.size() * topkCandidates.size());
  }
  context.geometry_auto_tune_policy_loaded = true;
  context.geometry_auto_tune_policy_source = policyPath.string();
  context.geometry_auto_tune_ontology_source = ontologyPath.string();
  reason = "versioned auto-tune policy and frozen seven-class ontology loaded";
  return true;
}

static bool LoadGeometryAutoTuneSummaryLocal(ManualTestContext &context,
                                             std::string &reason) {
  const std::filesystem::path root =
      ResolveCxVisionRunPath(context.geometry_auto_tune_scan_root);
  std::error_code ec;
  if (!std::filesystem::is_directory(root, ec) || ec) {
    reason = "AUTO_TUNE_EVIDENCE_ROOT_MISSING: " + root.string();
    return false;
  }
  std::filesystem::path latest;
  std::filesystem::file_time_type latestTime{};
  for (std::filesystem::recursive_directory_iterator it(
           root, std::filesystem::directory_options::skip_permission_denied,
           ec), end;
       !ec && it != end; it.increment(ec)) {
    std::error_code itemError;
    if (it->is_symlink(itemError)) {
      if (it->is_directory(itemError))
        it.disable_recursion_pending();
      continue;
    }
    if (itemError || !it->is_regular_file(itemError) ||
        it->path().filename() != "auto_tune_summary.json")
      continue;
    cv::FileStorage summary(it->path().string(), cv::FileStorage::READ);
    const std::string schema = summary.isOpened()
                                   ? static_cast<std::string>(summary["schema"])
                                   : std::string();
    if (schema != "cxvision.torch_geometry_auto_tune_summary.v1")
      continue;
    const auto writeTime = std::filesystem::last_write_time(it->path(), itemError);
    if (!itemError && (latest.empty() || writeTime > latestTime)) {
      latest = it->path();
      latestTime = writeTime;
    }
  }
  if (ec || latest.empty()) {
    reason = ec ? "AUTO_TUNE_EVIDENCE_SCAN_FAILED: " + ec.message()
                : "AUTO_TUNE_EVIDENCE_MISSING";
    return false;
  }
  cv::FileStorage summary(latest.string(), cv::FileStorage::READ);
  const cv::FileNode profile = summary["selection_profile"];
  const cv::FileNode validation = summary["validation"];
  const cv::FileNode holdout = summary["holdout"];
  const cv::FileNode validationCandidate = validation["candidate"];
  const cv::FileNode holdoutCandidate = holdout["candidate"];
  context.geometry_auto_tune_summary_path = latest.string();
  context.geometry_auto_tune_status = static_cast<std::string>(summary["status"]);
  context.geometry_auto_tune_quality_state =
      static_cast<std::string>(summary["quality_state"]);
  context.geometry_auto_tune_production_state =
      static_cast<std::string>(summary["production_admission_state"]);
  double selectedConfidence = 0.0;
  profile["confidence_threshold"] >> selectedConfidence;
  profile["topk_per_roi"] >> context.geometry_auto_tune_selected_topk;
  validationCandidate["f1"] >> context.geometry_auto_tune_validation_f1;
  holdoutCandidate["f1"] >> context.geometry_auto_tune_holdout_f1;
  context.geometry_auto_tune_selected_confidence =
      static_cast<float>(selectedConfidence);
  if (context.geometry_auto_tune_status.empty() ||
      context.geometry_auto_tune_quality_state.empty() ||
      context.geometry_auto_tune_production_state.empty() ||
      context.geometry_auto_tune_selected_topk <= 0) {
    reason = "AUTO_TUNE_SUMMARY_REQUIRED_FACT_MISSING";
    return false;
  }
  context.geometry_auto_tune_reason =
      "latest asset-driven auto-tune summary loaded";
  reason = context.geometry_auto_tune_reason;
  return true;
}

static void RefreshGeometryAutoTuneEvidenceLocal(ManualTestContext &context) {
  std::string reason;
  if (!LoadGeometryAutoTuneSummaryLocal(context, reason)) {
    context.geometry_auto_tune_status = "AUTO_TUNE_NOT_RUN";
    context.geometry_auto_tune_quality_state = "PENDING";
    context.geometry_auto_tune_production_state = "REFERENCE_ONLY";
    context.geometry_auto_tune_reason = reason;
    context.geometry_auto_tune_summary_path.clear();
  }
}

static ImVec4 GeometryAutoTuneStatusColorLocal(const std::string &status) {
  if (status.find("PASS") != std::string::npos ||
      status.find("COMPLETE") != std::string::npos)
    return ImVec4(0.36f, 0.82f, 0.49f, 1.0f);
  if (status.find("FAIL") != std::string::npos ||
      status.find("REJECT") != std::string::npos)
    return ImVec4(0.95f, 0.34f, 0.30f, 1.0f);
  return ImVec4(1.0f, 0.72f, 0.25f, 1.0f);
}

struct GeometryCandidateMetricLocal {
  std::string report_ref;
  int case_count = 0;
  double f1 = 0.0;
  double miss_rate = 0.0;
  double false_alarm_case_rate = 0.0;
  double exact_single_correct_rate = 0.0;
};

struct GeometryCandidateBundleLocal {
  bool available = false;
  std::string path;
  std::string status;
  bool published_with_incremental_strategy = false;
  bool candidate_allowed = false;
  bool approved = false;
  bool active_allowed = false;
  bool human_business_review_required = false;
  bool holdout_reused_after_diagnostic_feedback = false;
  bool fresh_business_acceptance_required = false;
  std::string model_manifest_ref;
  std::string model_checkpoint_ref;
  std::string model_checkpoint_digest;
  std::string training_strategy_ref;
  std::string postprocess_profile_ref;
  std::string policy_ref;
  std::vector<std::string> feedback_loop;
  GeometryCandidateMetricLocal validation;
  GeometryCandidateMetricLocal holdout;
  bool quality_gate_passed = false;
  double maximum_holdout_miss_rate = 0.0;
  double maximum_holdout_false_alarm_case_rate = 0.0;
  std::string failure_samples_ref;
  std::string rollback_checkpoint_ref;
  std::string rollback_checkpoint_digest;
  bool rollback_automatic_on_gate_failure = false;
  bool postprocess_profile_available = false;
  std::string postprocess_profile_id;
  std::string postprocess_problem_class;
  double postprocess_confidence_threshold = 0.0;
  double postprocess_iou_threshold = 0.0;
  int postprocess_max_detections = 0;
  bool postprocess_class_agnostic_nms = false;
  std::string postprocess_selection_split;
  bool postprocess_holdout_used_for_selection = false;
};

static bool GeometryCandidateBoolLocal(const cv::FileNode &node,
                                       bool fallback = false) {
  if (node.empty())
    return fallback;
  int value = fallback ? 1 : 0;
  node >> value;
  return value != 0;
}

static std::string GeometryCandidateStringLocal(const cv::FileNode &node) {
  return node.empty() ? std::string() : static_cast<std::string>(node);
}

static std::filesystem::path GeometryCandidateResolveRefLocal(
    const std::filesystem::path &bundlePath, const std::string &reference) {
  if (reference.empty())
    return {};
  std::filesystem::path resolved(reference);
  if (resolved.is_relative())
    resolved = bundlePath.parent_path() / resolved;
  return resolved.lexically_normal();
}

static void GeometryCandidateReadMetricLocal(
    const cv::FileNode &node, GeometryCandidateMetricLocal &metric) {
  node["case_count"] >> metric.case_count;
  metric.report_ref = GeometryCandidateStringLocal(node["report_ref"]);
  node["f1"] >> metric.f1;
  node["miss_rate"] >> metric.miss_rate;
  node["false_alarm_case_rate"] >> metric.false_alarm_case_rate;
  node["exact_single_correct_rate"] >> metric.exact_single_correct_rate;
}

static bool LoadLatestGeometryCandidateBundleLocal(
    const std::string &scanRoot, GeometryCandidateBundleLocal &bundle,
    std::string &reason) {
  bundle = GeometryCandidateBundleLocal{};
  const std::filesystem::path root = ResolveCxVisionRunPath(scanRoot);
  std::error_code ec;
  if (!std::filesystem::is_directory(root, ec) || ec) {
    reason = "CANDIDATE_BUNDLE_ROOT_MISSING: " + root.string();
    return false;
  }

  std::filesystem::path latest;
  std::filesystem::file_time_type latestTime{};
  std::filesystem::recursive_directory_iterator iterator(
      root, std::filesystem::directory_options::skip_permission_denied, ec);
  const std::filesystem::recursive_directory_iterator end;
  while (iterator != end) {
    if (ec) {
      ec.clear();
      iterator.increment(ec);
      continue;
    }
    std::error_code itemError;
    if (iterator->is_symlink(itemError)) {
      if (!itemError && iterator->is_directory(itemError))
        iterator.disable_recursion_pending();
    } else if (!itemError && iterator->is_regular_file(itemError) &&
               iterator->path().filename() == "candidate_bundle.json") {
      cv::FileStorage candidate(iterator->path().string(),
                                cv::FileStorage::READ);
      const std::string schema =
          candidate.isOpened()
              ? GeometryCandidateStringLocal(candidate["schema"])
              : std::string();
      if (schema == "cxvision.torch_incremental_candidate_bundle.v1") {
        const auto writeTime = iterator->last_write_time(itemError);
        if (!itemError && (latest.empty() || writeTime > latestTime)) {
          latest = iterator->path();
          latestTime = writeTime;
        }
      }
    }
    iterator.increment(ec);
  }
  if (latest.empty()) {
    reason = "CANDIDATE_BUNDLE_MISSING_UNDER: " + root.string();
    return false;
  }

  cv::FileStorage storage(latest.string(), cv::FileStorage::READ);
  if (!storage.isOpened() ||
      GeometryCandidateStringLocal(storage["schema"]) !=
          "cxvision.torch_incremental_candidate_bundle.v1") {
    reason = "CANDIDATE_BUNDLE_SCHEMA_INVALID: " + latest.string();
    return false;
  }
  bundle.path = latest.string();
  bundle.status = GeometryCandidateStringLocal(storage["status"]);
  bundle.published_with_incremental_strategy = GeometryCandidateBoolLocal(
      storage["published_with_incremental_strategy"]);
  bundle.candidate_allowed =
      GeometryCandidateBoolLocal(storage["candidate_allowed"]);
  bundle.approved = GeometryCandidateBoolLocal(storage["approved"]);
  bundle.active_allowed =
      GeometryCandidateBoolLocal(storage["active_allowed"]);
  bundle.human_business_review_required = GeometryCandidateBoolLocal(
      storage["human_business_review_required"]);
  bundle.holdout_reused_after_diagnostic_feedback = GeometryCandidateBoolLocal(
      storage["holdout_reused_after_diagnostic_feedback"]);
  bundle.fresh_business_acceptance_required = GeometryCandidateBoolLocal(
      storage["fresh_business_acceptance_required"]);

  const cv::FileNode model = storage["model"];
  bundle.model_manifest_ref =
      GeometryCandidateStringLocal(model["manifest_ref"]);
  bundle.model_checkpoint_ref =
      GeometryCandidateStringLocal(model["checkpoint_ref"]);
  bundle.model_checkpoint_digest =
      GeometryCandidateStringLocal(model["checkpoint_digest"]);

  const cv::FileNode strategy = storage["incremental_strategy"];
  bundle.training_strategy_ref =
      GeometryCandidateStringLocal(strategy["training_strategy_ref"]);
  bundle.postprocess_profile_ref =
      GeometryCandidateStringLocal(strategy["postprocess_profile_ref"]);
  bundle.policy_ref = GeometryCandidateStringLocal(strategy["policy_ref"]);
  const cv::FileNode feedbackLoop = strategy["feedback_loop"];
  if (feedbackLoop.isSeq()) {
    for (const cv::FileNode &stage : feedbackLoop)
      bundle.feedback_loop.push_back(GeometryCandidateStringLocal(stage));
  }

  const cv::FileNode evaluation = storage["evaluation"];
  GeometryCandidateReadMetricLocal(evaluation["validation"],
                                   bundle.validation);
  GeometryCandidateReadMetricLocal(evaluation["holdout_regression"],
                                   bundle.holdout);
  const cv::FileNode qualityGate = storage["quality_gate"];
  bundle.quality_gate_passed = GeometryCandidateBoolLocal(qualityGate["passed"]);
  qualityGate["maximum_holdout_miss_rate"] >> bundle.maximum_holdout_miss_rate;
  qualityGate["maximum_holdout_false_alarm_case_rate"] >>
      bundle.maximum_holdout_false_alarm_case_rate;
  bundle.failure_samples_ref =
      GeometryCandidateStringLocal(storage["failure_samples_ref"]);

  const cv::FileNode rollback = storage["rollback"];
  bundle.rollback_checkpoint_ref =
      GeometryCandidateStringLocal(rollback["checkpoint_ref"]);
  bundle.rollback_checkpoint_digest =
      GeometryCandidateStringLocal(rollback["checkpoint_digest"]);
  bundle.rollback_automatic_on_gate_failure = GeometryCandidateBoolLocal(
      rollback["automatic_on_gate_failure"]);

  const auto resolveStoredReference = [&latest](std::string &reference) {
    reference = GeometryCandidateResolveRefLocal(latest, reference).string();
  };
  resolveStoredReference(bundle.model_manifest_ref);
  resolveStoredReference(bundle.model_checkpoint_ref);
  resolveStoredReference(bundle.training_strategy_ref);
  resolveStoredReference(bundle.postprocess_profile_ref);
  resolveStoredReference(bundle.policy_ref);
  resolveStoredReference(bundle.rollback_checkpoint_ref);
  resolveStoredReference(bundle.validation.report_ref);
  resolveStoredReference(bundle.holdout.report_ref);
  resolveStoredReference(bundle.failure_samples_ref);

  const std::filesystem::path profilePath = GeometryCandidateResolveRefLocal(
      latest, bundle.postprocess_profile_ref);
  cv::FileStorage profile(profilePath.string(), cv::FileStorage::READ);
  if (profile.isOpened() &&
      GeometryCandidateStringLocal(profile["schema"]) ==
          "cxvision.yolov8n_postprocess_profile.v1") {
    const cv::FileNode postprocess = profile["postprocess"];
    bundle.postprocess_profile_available = true;
    bundle.postprocess_profile_id =
        GeometryCandidateStringLocal(profile["profile_id"]);
    bundle.postprocess_problem_class =
        GeometryCandidateStringLocal(profile["problem_class"]);
    postprocess["confidence_threshold"] >>
        bundle.postprocess_confidence_threshold;
    postprocess["iou_threshold"] >> bundle.postprocess_iou_threshold;
    postprocess["max_detections"] >> bundle.postprocess_max_detections;
    bundle.postprocess_class_agnostic_nms = GeometryCandidateBoolLocal(
        postprocess["class_agnostic_nms"]);
    bundle.postprocess_selection_split =
        GeometryCandidateStringLocal(profile["selection_split"]);
    bundle.postprocess_holdout_used_for_selection = GeometryCandidateBoolLocal(
        profile["holdout_used_for_selection"]);
  }

  const bool requiredFactsPresent =
      !bundle.status.empty() && !bundle.model_manifest_ref.empty() &&
      !bundle.model_checkpoint_ref.empty() &&
      !bundle.model_checkpoint_digest.empty() &&
      !bundle.training_strategy_ref.empty() &&
      !bundle.postprocess_profile_ref.empty() && !bundle.policy_ref.empty() &&
      !bundle.rollback_checkpoint_ref.empty() &&
      !bundle.rollback_checkpoint_digest.empty() &&
      !bundle.feedback_loop.empty() && bundle.validation.case_count > 0 &&
      bundle.holdout.case_count > 0 && !bundle.validation.report_ref.empty() &&
      !bundle.holdout.report_ref.empty() && !bundle.failure_samples_ref.empty();
  if (!requiredFactsPresent) {
    reason = "CANDIDATE_BUNDLE_REQUIRED_FACT_MISSING: " + latest.string();
    return false;
  }
  bundle.available = true;
  reason = bundle.postprocess_profile_available
               ? "latest candidate bundle and effective postprocess profile loaded"
               : "candidate bundle loaded; postprocess profile ASSET_MISSING";
  return true;
}

static bool GeometryCandidateRegularFileLocal(const std::string &reference) {
  std::error_code ec;
  return !reference.empty() &&
         std::filesystem::is_regular_file(std::filesystem::path(reference), ec) &&
         !ec;
}

static void DrawGeometryCandidateAssetRowLocal(const char *role,
                                               const std::string &reference) {
  ImGui::TableNextRow();
  ImGui::TableSetColumnIndex(0);
  ImGui::TextUnformatted(role);
  ImGui::TableSetColumnIndex(1);
  const bool available = GeometryCandidateRegularFileLocal(reference);
  ImGui::TextColored(available ? ImVec4(0.36f, 0.82f, 0.49f, 1.0f)
                               : ImVec4(0.95f, 0.34f, 0.30f, 1.0f),
                     "%s", available ? "AVAILABLE" : "ASSET_MISSING");
  ImGui::TableSetColumnIndex(2);
  ImGui::TextWrapped("%s", reference.empty() ? "ASSET_MISSING"
                                             : reference.c_str());
}

static void DrawGeometryCandidateBundleLocal(ManualTestContext &context) {
  static GeometryCandidateBundleLocal bundle;
  static std::string loadReason = "candidate bundle not scanned";
  static std::string loadedScanRoot;
  static bool scanAttempted = false;

  if (!scanAttempted || loadedScanRoot != context.geometry_auto_tune_scan_root) {
    scanAttempted = true;
    loadedScanRoot = context.geometry_auto_tune_scan_root;
    LoadLatestGeometryCandidateBundleLocal(loadedScanRoot, bundle, loadReason);
  }

  if (!ImGui::CollapsingHeader("Production Candidate / Strategy / Rollback",
                               ImGuiTreeNodeFlags_DefaultOpen))
    return;
  if (ImGui::Button("Reload Candidate Bundle##geometry_candidate_bundle")) {
    LoadLatestGeometryCandidateBundleLocal(
        context.geometry_auto_tune_scan_root, bundle, loadReason);
  }
  ImGui::SameLine();
  ImGui::TextColored(GeometryAutoTuneStatusColorLocal(
                         bundle.available ? bundle.status : "ASSET_MISSING"),
                     "%s", bundle.available ? bundle.status.c_str()
                                             : "ASSET_MISSING");
  if (!bundle.available) {
    ImGui::TextWrapped("%s", loadReason.c_str());
    return;
  }

  ImGui::TextWrapped("Evidence item: %s", bundle.path.c_str());
  if (ImGui::BeginTable("geometry_candidate_admission", 2,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_SizingStretchProp)) {
    ImGui::TableSetupColumn("Admission fact", ImGuiTableColumnFlags_WidthStretch,
                            1.7f);
    ImGui::TableSetupColumn("Recorded value",
                            ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableHeadersRow();
    const auto boolRow = [](const char *label, bool value,
                            const ImVec4 &trueColor,
                            const ImVec4 &falseColor) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(label);
      ImGui::TableSetColumnIndex(1);
      ImGui::TextColored(value ? trueColor : falseColor, "%s",
                         value ? "true" : "false");
    };
    const ImVec4 green(0.36f, 0.82f, 0.49f, 1.0f);
    const ImVec4 amber(1.0f, 0.72f, 0.25f, 1.0f);
    const ImVec4 red(0.95f, 0.34f, 0.30f, 1.0f);
    boolRow("published_with_incremental_strategy",
            bundle.published_with_incremental_strategy, green, red);
    boolRow("candidate_allowed", bundle.candidate_allowed, green, red);
    boolRow("approved", bundle.approved, green, amber);
    boolRow("active_allowed", bundle.active_allowed, green, amber);
    boolRow("human_business_review_required",
            bundle.human_business_review_required, amber, green);
    boolRow("fresh_business_acceptance_required",
            bundle.fresh_business_acceptance_required, amber, green);
    ImGui::EndTable();
  }
  if (bundle.holdout_reused_after_diagnostic_feedback) {
    ImGui::TextColored(
        ImVec4(1.0f, 0.72f, 0.25f, 1.0f),
        "HOLDOUT_REUSED_FOR_REGRESSION: not a fresh blind acceptance split.");
  }

  if (ImGui::CollapsingHeader("Candidate Model Artifact",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Text("checkpoint_digest: %s",
                bundle.model_checkpoint_digest.c_str());
    if (ImGui::BeginTable("geometry_candidate_model_assets", 3,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_SizingStretchProp)) {
      ImGui::TableSetupColumn("Role", ImGuiTableColumnFlags_WidthFixed, 145.0f);
      ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 110.0f);
      ImGui::TableSetupColumn("Asset", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableHeadersRow();
      DrawGeometryCandidateAssetRowLocal("model_manifest",
                                         bundle.model_manifest_ref);
      DrawGeometryCandidateAssetRowLocal("candidate_checkpoint",
                                         bundle.model_checkpoint_ref);
      ImGui::EndTable();
    }
  }

  if (ImGui::CollapsingHeader("Incremental Self-Tuning Strategy",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    if (ImGui::BeginTable("geometry_candidate_strategy_assets", 3,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_SizingStretchProp)) {
      ImGui::TableSetupColumn("Role", ImGuiTableColumnFlags_WidthFixed, 145.0f);
      ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 110.0f);
      ImGui::TableSetupColumn("Asset", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableHeadersRow();
      DrawGeometryCandidateAssetRowLocal("training_strategy",
                                         bundle.training_strategy_ref);
      DrawGeometryCandidateAssetRowLocal("postprocess_profile",
                                         bundle.postprocess_profile_ref);
      DrawGeometryCandidateAssetRowLocal("auto_tune_policy",
                                         bundle.policy_ref);
      ImGui::EndTable();
    }
    if (bundle.postprocess_profile_available) {
      ImGui::Text(
          "effective postprocess: conf %.4f | IoU %.4f | max %d | class-agnostic NMS %s",
          bundle.postprocess_confidence_threshold,
          bundle.postprocess_iou_threshold, bundle.postprocess_max_detections,
          bundle.postprocess_class_agnostic_nms ? "on" : "off");
      ImGui::Text("profile: %s | problem: %s | selection_split: %s",
                  bundle.postprocess_profile_id.c_str(),
                  bundle.postprocess_problem_class.c_str(),
                  bundle.postprocess_selection_split.c_str());
      ImGui::Text("holdout_used_for_selection: %s",
                  bundle.postprocess_holdout_used_for_selection ? "true"
                                                                : "false");
    } else {
      ImGui::TextColored(ImVec4(0.95f, 0.34f, 0.30f, 1.0f),
                         "POSTPROCESS_PROFILE_ASSET_MISSING");
    }
    if (ImGui::BeginTable("geometry_candidate_feedback_loop", 2,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_SizingStretchProp)) {
      ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 34.0f);
      ImGui::TableSetupColumn("Published feedback stage",
                              ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableHeadersRow();
      for (std::size_t index = 0; index < bundle.feedback_loop.size(); ++index) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("%d", static_cast<int>(index + 1));
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(bundle.feedback_loop[index].c_str());
      }
      ImGui::EndTable();
    }
  }

  if (ImGui::CollapsingHeader("Locked Evaluation",
                              ImGuiTreeNodeFlags_DefaultOpen) &&
      ImGui::BeginTable("geometry_candidate_evaluation", 6,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_SizingStretchProp)) {
    ImGui::TableSetupColumn("Split", ImGuiTableColumnFlags_WidthFixed, 150.0f);
    ImGui::TableSetupColumn("Cases", ImGuiTableColumnFlags_WidthFixed, 58.0f);
    ImGui::TableSetupColumn("F1");
    ImGui::TableSetupColumn("Miss rate");
    ImGui::TableSetupColumn("False-alarm case rate");
    ImGui::TableSetupColumn("Exact single correct");
    ImGui::TableHeadersRow();
    const auto metricRow = [](const char *split,
                              const GeometryCandidateMetricLocal &metric) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(split);
      ImGui::TableSetColumnIndex(1);
      ImGui::Text("%d", metric.case_count);
      ImGui::TableSetColumnIndex(2);
      ImGui::Text("%.6f", metric.f1);
      ImGui::TableSetColumnIndex(3);
      ImGui::Text("%.6f", metric.miss_rate);
      ImGui::TableSetColumnIndex(4);
      ImGui::Text("%.6f", metric.false_alarm_case_rate);
      ImGui::TableSetColumnIndex(5);
      ImGui::Text("%.6f", metric.exact_single_correct_rate);
    };
    metricRow("validation (selection)", bundle.validation);
    metricRow("holdout (regression)", bundle.holdout);
    ImGui::EndTable();
  }
  ImGui::Text("quality gate: %s | miss <= %.6f | false-alarm case <= %.6f",
              bundle.quality_gate_passed ? "passed" : "failed",
              bundle.maximum_holdout_miss_rate,
              bundle.maximum_holdout_false_alarm_case_rate);
  if (ImGui::BeginTable("geometry_candidate_evaluation_assets", 3,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_SizingStretchProp)) {
    ImGui::TableSetupColumn("Role", ImGuiTableColumnFlags_WidthFixed, 145.0f);
    ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 110.0f);
    ImGui::TableSetupColumn("Asset", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();
    DrawGeometryCandidateAssetRowLocal("validation_report",
                                       bundle.validation.report_ref);
    DrawGeometryCandidateAssetRowLocal("holdout_report",
                                       bundle.holdout.report_ref);
    DrawGeometryCandidateAssetRowLocal("failure_samples",
                                       bundle.failure_samples_ref);
    ImGui::EndTable();
  }

  if (ImGui::CollapsingHeader("Rollback Receipt",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Text("checkpoint_digest: %s",
                bundle.rollback_checkpoint_digest.c_str());
    ImGui::Text("automatic_on_gate_failure: %s",
                bundle.rollback_automatic_on_gate_failure ? "true" : "false");
    ImGui::TextWrapped("checkpoint: %s",
                       bundle.rollback_checkpoint_ref.c_str());
  }
  ImGui::TextColored(
      ImVec4(1.0f, 0.72f, 0.25f, 1.0f),
      "EVALUATED is not APPROVED or ACTIVE. Business acceptance remains external.");
}

} // namespace

void DrawGeometryAutoTuneParametersPanel(ManualTestContext &context) {
  std::string reason;
  if (!context.geometry_auto_tune_policy_loaded)
    LoadGeometryAutoTunePolicyLocal(context, false, reason);
  if (!ImGui::CollapsingHeader("Geometry Auto-Tune Parameters",
                               ImGuiTreeNodeFlags_DefaultOpen))
    return;
  ImGui::TextDisabled(
      "JSON policy is authoritative. The cfg projection is read-only and never a second configuration source.");
  if (!context.geometry_auto_tune_policy_loaded) {
    ImGui::TextColored(ImVec4(0.95f, 0.34f, 0.30f, 1.0f), "%s", reason.c_str());
    return;
  }
  if (ImGui::Button("Reload Auto-Tune Defaults")) {
    if (!LoadGeometryAutoTunePolicyLocal(context, true, reason)) {
      context.geometry_auto_tune_reason = reason;
    } else {
      context.geometry_auto_tune_reason = reason;
      context.debug_status = "AUTO_TUNE_DEFAULTS_RELOADED";
      context.debug_reason = reason;
    }
  }
  ImGui::SameLine();
  ImGui::TextDisabled("requested values remain drafts until headless snapshot export");
  cv::FileStorage policy(context.geometry_auto_tune_policy_source,
                         cv::FileStorage::READ);
  cv::FileStorage ontology(context.geometry_auto_tune_ontology_source,
                           cv::FileStorage::READ);
  if (!policy.isOpened() || !ontology.isOpened()) {
    ImGui::TextColored(ImVec4(0.95f, 0.34f, 0.30f, 1.0f),
                       "AUTO_TUNE_PARAMETER_ASSET_MISSING");
    return;
  }
  ImGui::TextWrapped("Policy: %s", context.geometry_auto_tune_policy_source.c_str());
  ImGui::TextWrapped("Ontology: %s", context.geometry_auto_tune_ontology_source.c_str());
  if (ImGui::BeginTable("geometry_auto_tune_request", 3,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_SizingStretchProp)) {
    ImGui::TableSetupColumn("Parameter", ImGuiTableColumnFlags_WidthStretch, 1.5f);
    ImGui::TableSetupColumn("Requested", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableSetupColumn("Source / search space", ImGuiTableColumnFlags_WidthStretch, 2.0f);
    ImGui::TableHeadersRow();
    const auto row = [](const char *name, const char *source) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(name);
      ImGui::TableSetColumnIndex(2);
      ImGui::TextWrapped("%s", source);
    };
    row("confidence_threshold", "policy confidence_threshold_candidates");
    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputFloat("##geometry_auto_confidence",
                      &context.yolo_training_postprocess_confidence_threshold,
                      0.0f, 0.0f, "%.4f");
    ImGui::TableSetColumnIndex(2);
    ImGui::TextWrapped("%s", GeometryAutoTuneDoubleListLocal(
                                       policy["confidence_threshold_candidates"]).c_str());
    row("topk_per_roi", "policy topk_candidates");
    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputInt("##geometry_auto_topk", &context.geometry_auto_tune_topk_per_roi);
    ImGui::TableSetColumnIndex(2);
    ImGui::TextWrapped("%s", GeometryAutoTuneIntListLocal(
                                       policy["topk_candidates"]).c_str());
    row("match_iou_threshold", "policy fixed match threshold");
    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputFloat("##geometry_auto_match_iou",
                      &context.geometry_auto_tune_match_iou_threshold,
                      0.0f, 0.0f, "%.4f");
    ImGui::TableSetColumnIndex(2);
    double matchIou = 0.0;
    policy["match_iou_threshold"] >> matchIou;
    ImGui::Text("%.4f", matchIou);
    row("max_detections", "training plan runtime parameter");
    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputInt("##geometry_auto_max_det",
                    &context.yolo_training_postprocess_max_detections);
    ImGui::TableSetColumnIndex(2);
    ImGui::TextDisabled("candidate lists are bounded before Top-K selection");
    row("maximum_trials", "derived confidence candidates x top-k candidates");
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("%d", context.geometry_auto_tune_max_trials);
    ImGui::EndTable();
  }
  if (ImGui::CollapsingHeader("Frozen Seven-Class Ontology")) {
    const cv::FileNode classes = ontology["classes"];
    if (ImGui::BeginTable("geometry_auto_tune_classes", 3,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_SizingStretchProp)) {
      ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 45.0f);
      ImGui::TableSetupColumn("Class", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("Display", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableHeadersRow();
      for (const cv::FileNode &node : classes) {
        int id = -1;
        node["id"] >> id;
        const std::string name = static_cast<std::string>(node["name"]);
        const std::string display = static_cast<std::string>(node["display_name"]);
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("%d", id);
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(name.c_str());
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted(display.c_str());
      }
      ImGui::EndTable();
    }
    ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.25f, 1.0f),
                       "Historical rectangle assets require an explicit closed_curve migration receipt.");
  }
}

void DrawGeometryAutoTuneDiagnosticsPanel(ManualTestContext &context) {
  if (!ImGui::CollapsingHeader("Geometry Auto-Tune Execution Rail",
                               ImGuiTreeNodeFlags_DefaultOpen))
    return;
  if (ImGui::Button("Reload Run Evidence##geometry_auto_tune_map"))
    RefreshGeometryAutoTuneEvidenceLocal(context);
  ImGui::SameLine();
  ImGui::TextColored(GeometryAutoTuneStatusColorLocal(context.geometry_auto_tune_status),
                     "%s", context.geometry_auto_tune_status.c_str());
  ImGui::TextWrapped(
      "Selection is validation-only. Holdout is confirmation-only and cannot reorder threshold or Top-K trials.");
  if (context.geometry_auto_tune_summary_path.empty()) {
    ImGui::TextDisabled("%s", context.geometry_auto_tune_reason.c_str());
    return;
  }
  cv::FileStorage summary(context.geometry_auto_tune_summary_path,
                          cv::FileStorage::READ);
  cv::FileStorage policy(context.geometry_auto_tune_policy_source,
                         cv::FileStorage::READ);
  const cv::FileNode stages = summary["stages"];
  if (!summary.isOpened() || !stages.isSeq()) {
    ImGui::TextColored(ImVec4(0.95f, 0.34f, 0.30f, 1.0f),
                       "AUTO_TUNE_STAGE_EVIDENCE_INVALID");
    return;
  }
  if (ImGui::BeginTable("geometry_auto_tune_stages", 4,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_SizingStretchProp,
                        ImVec2(-1.0f, 215.0f))) {
    ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 34.0f);
    ImGui::TableSetupColumn("Stage", ImGuiTableColumnFlags_WidthFixed, 170.0f);
    ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 130.0f);
    ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();
    for (const cv::FileNode &node : stages) {
      int order = -1;
      node["order"] >> order;
      const std::string id = static_cast<std::string>(node["id"]);
      const std::string status = static_cast<std::string>(node["status"]);
      const std::string action = static_cast<std::string>(node["action"]);
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::Text("%d", order);
      ImGui::TableSetColumnIndex(1);
      ImGui::TextUnformatted(id.c_str());
      ImGui::TableSetColumnIndex(2);
      ImGui::TextColored(GeometryAutoTuneStatusColorLocal(status), "%s",
                         status.c_str());
      ImGui::TableSetColumnIndex(3);
      ImGui::TextWrapped("%s", action.c_str());
    }
    ImGui::EndTable();
  }
  ImGui::Text("Validation F1 %.6f | Holdout F1 %.6f | profile conf %.4f / Top-K %d",
              context.geometry_auto_tune_validation_f1,
              context.geometry_auto_tune_holdout_f1,
              context.geometry_auto_tune_selected_confidence,
              context.geometry_auto_tune_selected_topk);
  ImGui::TextDisabled(
      "Recipe-generated stages request a new candidate training run; they do not alter the active checkpoint.");
}

void DrawGeometryAutoTuneEvidencePanel(ManualTestContext &context) {
  if (!ImGui::CollapsingHeader("Geometry Auto-Tune / Evidence Receipt",
                               ImGuiTreeNodeFlags_DefaultOpen))
    return;
  if (ImGui::Button("Reload Run Evidence##geometry_auto_tune_evidence"))
    RefreshGeometryAutoTuneEvidenceLocal(context);
  if (context.geometry_auto_tune_summary_path.empty())
    RefreshGeometryAutoTuneEvidenceLocal(context);
  ImGui::Text("status: %s", context.geometry_auto_tune_status.c_str());
  ImGui::TextColored(
      GeometryAutoTuneStatusColorLocal(context.geometry_auto_tune_quality_state),
      "quality_state: %s", context.geometry_auto_tune_quality_state.c_str());
  ImGui::TextColored(
      GeometryAutoTuneStatusColorLocal(context.geometry_auto_tune_production_state),
      "production_admission_state: %s",
      context.geometry_auto_tune_production_state.c_str());
  ImGui::Text("promotion_allowed: false | active_allowed: false");
  ImGui::TextWrapped("summary: %s",
                     context.geometry_auto_tune_summary_path.empty()
                         ? context.geometry_auto_tune_reason.c_str()
                         : context.geometry_auto_tune_summary_path.c_str());
  if (!context.geometry_auto_tune_summary_path.empty()) {
    ImGui::Text("effective profile: confidence %.4f | Top-K %d | validation F1 %.6f | holdout F1 %.6f",
                context.geometry_auto_tune_selected_confidence,
                context.geometry_auto_tune_selected_topk,
                context.geometry_auto_tune_validation_f1,
                context.geometry_auto_tune_holdout_f1);
    ImGui::TextDisabled(
        "Open the evidence directory for requested_config, effective_config, confusion_matrix, next_training_recipe and evidence_index receipts.");
  }
  DrawGeometryCandidateBundleLocal(context);
}
