#include "CxScriptHeadlessRuntime.h"
#include "CxUnifiedLog.h"
#include "ManualConsoleCxScriptDebug.h"
#include "ManualConsoleGauge.h"
#include "ManualConsoleUtils.h"
#include "pch.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <regex>
#include <sstream>

namespace {
std::string GaugeJsonEscape(const std::string &value) {
  std::string out;
  out.reserve(value.size());
  for (char ch : value) {
    if (ch == '\\' || ch == '"')
      out.push_back('\\');
    if (ch == '\n')
      out += "\\n";
    else if (ch != '\r')
      out.push_back(ch);
  }
  return out;
}

std::string SafePathComponent(const std::string &value) {
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

bool ExtractJsonString(const std::string &source, const char *key,
                       std::string &value) {
  const std::regex pattern(std::string("\\\"") + key +
                           "\\\"\\s*:\\s*\\\"([^\\\"]*)\\\"");
  std::smatch match;
  if (!std::regex_search(source, match, pattern))
    return false;
  value = match[1].str();
  return true;
}

bool ExtractJsonInt(const std::string &source, const char *key, int &value) {
  const std::regex pattern(std::string("\\\"") + key +
                           "\\\"\\s*:\\s*(-?[0-9]+)");
  std::smatch match;
  if (!std::regex_search(source, match, pattern))
    return false;
  value = std::stoi(match[1].str());
  return true;
}

bool ExtractJsonBool(const std::string &source, const char *key, bool &value) {
  const std::regex pattern(std::string("\\\"") + key +
                           "\\\"\\s*:\\s*(true|false)");
  std::smatch match;
  if (!std::regex_search(source, match, pattern))
    return false;
  value = match[1].str() == "true";
  return true;
}

int NormalizeFindLineSelectedEdgeForRuntime(int selectedEdge, int edgeCount) {
  const int count = std::max(1, std::min(16, edgeCount));
  const int edge = std::max(-1, std::min(selectedEdge, count));
  if (count == 2 && edge == 2)
    return -1;
  return edge;
}

bool ExtractSegmentationPromptPointArray(
    const std::string &source, const char *key,
    std::vector<ManualSegmentationPromptPoint> &points) {
  const std::regex arrayPattern(std::string("\\\"") + key +
                                "\\\"\\s*:\\s*\\[([^\\]]*)\\]");
  std::smatch arrayMatch;
  if (!std::regex_search(source, arrayMatch, arrayPattern))
    return false;
  points.clear();
  const std::string body = arrayMatch[1].str();
  const std::regex pointPattern(
      "\\{\\s*\\\"ref\\\"\\s*:\\s*\\\"([^\\\"]*)\\\"\\s*,\\s*\\\"x\\\"\\s*:\\s*"
      "(-?[0-9]+)\\s*,\\s*\\\"y\\\"\\s*:\\s*(-?[0-9]+)\\s*\\}");
  auto begin = std::sregex_iterator(body.begin(), body.end(), pointPattern);
  auto end = std::sregex_iterator();
  for (auto it = begin; it != end; ++it) {
    ManualSegmentationPromptPoint point;
    point.ref = (*it)[1].str();
    point.x = std::stoi((*it)[2].str());
    point.y = std::stoi((*it)[3].str());
    points.push_back(point);
  }
  return true;
}

int DefaultFindSettingForTool(const std::string &tool) {
  if (tool == "FindLine" || tool == "FindEllipse")
    return 1;
  if (tool == "FindCircle" || tool == "FindRect")
    return 0;
  return 1;
}

int ResolveManualGaugeFindSetting(const ManualGaugeState &gauge) {
  return std::max(0, gauge.findsetting >= 0
                         ? gauge.findsetting
                         : DefaultFindSettingForTool(gauge.tool));
}

void SyncSegmentationLegacyPointFromLists(ManualGaugeState &gauge) {
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

void WriteSegmentationPromptPointArray(
    std::ostream &out, const char *key,
    const std::vector<ManualSegmentationPromptPoint> &points,
    bool trailingComma) {
  out << "  \"" << key << "\": [";
  for (std::size_t i = 0; i < points.size(); ++i) {
    if (i != 0)
      out << ", ";
    out << "{\"ref\":\"" << GaugeJsonEscape(points[i].ref)
        << "\",\"x\":" << points[i].x << ",\"y\":" << points[i].y << "}";
  }
  out << "]" << (trailingComma ? "," : "") << "\n";
}

void ApplySegmentationPromptFallbacksFromGlobals(ManualTestContext &context,
                                                 ManualGaugeState &loaded) {
  auto readInt = [&context](const std::string &key, int fallback) {
    const auto it = context.runtime_int_vars.find(key);
    return it == context.runtime_int_vars.end() ? fallback : it->second;
  };

  loaded.has_segmentation_prompt_rect = true;
  loaded.segmentation_prompt_x0 = readInt(
      "global_roi_x0", readInt("global_roi_x", loaded.segmentation_prompt_x0));
  loaded.segmentation_prompt_y0 = readInt(
      "global_roi_y0", readInt("global_roi_y", loaded.segmentation_prompt_y0));
  const int roiW = readInt("global_roi_width", 860);
  const int roiH = readInt("global_roi_height", 700);
  loaded.segmentation_prompt_x1 =
      readInt("global_roi_x1", loaded.segmentation_prompt_x0 + roiW);
  loaded.segmentation_prompt_y1 =
      readInt("global_roi_y1", loaded.segmentation_prompt_y0 + roiH);
  loaded.segmentation_mode =
      readInt("global_segmentation_mode", loaded.segmentation_mode);
  loaded.segmentation_threshold_percent = readInt(
      "global_segmentation_threshold_percent",
      readInt("global_threshold", loaded.segmentation_threshold_percent));
  loaded.has_segmentation_positive_point =
      readInt("global_segmentation_positive_enabled", 0) != 0;
  loaded.segmentation_positive_x =
      readInt("global_segmentation_positive_x", loaded.segmentation_positive_x);
  loaded.segmentation_positive_y =
      readInt("global_segmentation_positive_y", loaded.segmentation_positive_y);
  loaded.has_segmentation_negative_point =
      readInt("global_segmentation_negative_enabled", 0) != 0;
  loaded.segmentation_negative_x =
      readInt("global_segmentation_negative_x", loaded.segmentation_negative_x);
  loaded.segmentation_negative_y =
      readInt("global_segmentation_negative_y", loaded.segmentation_negative_y);
  if (loaded.has_segmentation_positive_point &&
      loaded.segmentation_positive_points.empty()) {
    ManualSegmentationPromptPoint point;
    point.ref = "legacy_positive_0";
    point.x = loaded.segmentation_positive_x;
    point.y = loaded.segmentation_positive_y;
    loaded.segmentation_positive_points.push_back(point);
  }
  if (loaded.has_segmentation_negative_point &&
      loaded.segmentation_negative_points.empty()) {
    ManualSegmentationPromptPoint point;
    point.ref = "legacy_negative_0";
    point.x = loaded.segmentation_negative_x;
    point.y = loaded.segmentation_negative_y;
    loaded.segmentation_negative_points.push_back(point);
  }
  SyncSegmentationLegacyPointFromLists(loaded);
}

// Candidate packages written before the FindObject-specific gauge schema was
// introduced still contain the authoritative ROI in runtime_globals.json.
// Restore only from that candidate-local snapshot; never borrow the ROI from
// the currently selected Evidence row.
void ApplyFindObjectGaugeFallbacksFromGlobals(ManualTestContext &context,
                                              ManualGaugeState &loaded) {
  auto readInt = [&context](const std::string &key, int fallback) {
    const auto it = context.runtime_int_vars.find(key);
    return it == context.runtime_int_vars.end() ? fallback : it->second;
  };

  loaded.has_findobject_roi = true;
  loaded.findobject_x0 =
      readInt("global_roi_x0", readInt("global_roi_x", loaded.findobject_x0));
  loaded.findobject_y0 =
      readInt("global_roi_y0", readInt("global_roi_y", loaded.findobject_y0));
  const int roiWidth = readInt("global_roi_width", 0);
  const int roiHeight = readInt("global_roi_height", 0);
  loaded.findobject_x1 =
      readInt("global_roi_x1", loaded.findobject_x0 + roiWidth);
  loaded.findobject_y1 =
      readInt("global_roi_y1", loaded.findobject_y0 + roiHeight);
  loaded.findobject_foreground_mode = readInt(
      "global_object_foreground_mode", readInt("global_method", loaded.method));
  loaded.findobject_threshold = readInt(
      "global_object_threshold", readInt("global_threshold", loaded.threshold));
  loaded.findobject_min_area =
      readInt("global_object_min_area", loaded.findobject_min_area);
}

bool AtomicReplaceFile(const std::filesystem::path &temporary,
                       const std::filesystem::path &destination,
                       std::string &reason) {
#ifdef _WIN32
  if (!MoveFileExW(temporary.c_str(), destination.c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    reason = "cannot atomically replace file: " + destination.string();
    return false;
  }
#else
  std::error_code ec;
  std::filesystem::rename(temporary, destination, ec);
  if (ec) {
    reason = "cannot atomically replace file: " + ec.message();
    return false;
  }
#endif
  return true;
}
} // namespace

void RecordManualOperationTraceEvent(ManualTestContext &context,
                                     const std::string &operation,
                                     const std::string &status,
                                     const std::string &reason) {
  ManualOperationTraceEvent event;
  event.sequence = ++context.manual_operation_trace_sequence;
  event.key_parameter_edit_revision = context.key_parameter_edit_revision;
  event.operation = operation;
  event.status = status;
  event.reason = reason;
  event.summary = context.last_key_parameter_edit_summary;
  event.editor_source = context.editor_source;
  event.loaded_script_path = context.loaded_script_path;
  event.evidence_selection = context.current_evidence_selection;
  event.gauge = context.current_gauge;
  event.globals = context.runtime_int_vars;
  context.pending_manual_operation_trace_events.push_back(std::move(event));
  if (context.pending_manual_operation_trace_events.size() > 512) {
    context.pending_manual_operation_trace_events.erase(
        context.pending_manual_operation_trace_events.begin());
  }
  CXLOG_INFO("ManualOperationTrace", operation.c_str(), status,
             "sequence=" +
                 std::to_string(context.manual_operation_trace_sequence) +
                 " revision=" +
                 std::to_string(context.key_parameter_edit_revision) +
                 " reason=" + reason);
}

void ClearManualOperationTraceEvents(ManualTestContext &context) {
  context.pending_manual_operation_trace_events.clear();
}

void InjectManualGaugeInt(ManualTestContext &context, const std::string &key,
                          int value) {
  context.runtime_int_vars[key] = value;
  UpsertGlobalVariableView(context, "int", key, std::to_string(value), 0,
                           "manual_gauge_applied");
}

int ReadManualGaugeInt(const ManualTestContext &context, const std::string &key,
                       int fallback) {
  const auto it = context.runtime_int_vars.find(key);
  if (it == context.runtime_int_vars.end())
    return fallback;
  return it->second;
}

bool IsFastMatchGaugeTool(const std::string &tool) {
  return tool == "FastMatch" || tool == "fastmatch" || tool == "CFastMatch";
}

bool IsGridPatternGaugeTool(const std::string &tool) {
  return tool == "GridPatternClassTool";
}

bool IsRegionPatternGaugeTool(const std::string &tool) {
  return tool == "RegionPatternTool";
}

bool IsFindSegmentationGaugeTool(const std::string &tool) {
  return tool == "FindSegmentation" || tool == "findsegmentation" ||
         tool == "FindSegmentationTool";
}

bool IsFindObjectGaugeTool(const std::string &tool) {
  return tool == "FindObject" || tool == "findobject";
}

bool ValidateManualGaugeGeometryForEditing(const ManualGaugeState &gauge,
                                           std::string &reason) {
  if (gauge.tool == "FindLine" || gauge.tool == "FindRect") {
    if (!gauge.has_line_gauge)
      reason = "line gauge is unavailable";
    else if (gauge.line_x0 == gauge.line_x1 && gauge.line_y0 == gauge.line_y1)
      reason = "line gauge length is zero";
    else if (gauge.tool_half_width <= 0)
      reason = "tool_half_width must be positive";
    else
      reason.clear();
    return reason.empty();
  }
  if (gauge.tool == "FindCircle") {
    if (!gauge.has_circle_gauge)
      reason = "circle gauge is unavailable";
    else if (gauge.circle_px == gauge.circle_cx &&
             gauge.circle_py == gauge.circle_cy)
      reason = "circle radius is zero";
    else
      reason.clear();
    return reason.empty();
  }
  if (gauge.tool == "FindEllipse") {
    if (!gauge.has_ellipse_gauge)
      reason = "ellipse gauge is unavailable";
    else if (gauge.ellipse_x0 == gauge.ellipse_x1 ||
             gauge.ellipse_y0 == gauge.ellipse_y1)
      reason = "ellipse gauge radius is zero";
    else
      reason.clear();
    return reason.empty();
  }
  if (IsFastMatchGaugeTool(gauge.tool) || IsGridPatternGaugeTool(gauge.tool) ||
      IsRegionPatternGaugeTool(gauge.tool)) {
    reason.clear();
    return true;
  }
  if (IsFindSegmentationGaugeTool(gauge.tool)) {
    if (!gauge.has_segmentation_prompt_rect)
      reason = "FindSegmentation prompt ROI is unavailable";
    else if (gauge.segmentation_prompt_x1 <= gauge.segmentation_prompt_x0 ||
             gauge.segmentation_prompt_y1 <= gauge.segmentation_prompt_y0)
      reason =
          "FindSegmentation prompt ROI must have positive width and height";
    else
      reason.clear();
    return reason.empty();
  }
  if (IsFindObjectGaugeTool(gauge.tool)) {
    if (!gauge.has_findobject_roi)
      reason = "FindObject ROI is unavailable";
    else if (gauge.findobject_x1 <= gauge.findobject_x0 ||
             gauge.findobject_y1 <= gauge.findobject_y0)
      reason = "FindObject ROI must have positive width and height";
    else
      reason.clear();
    return reason.empty();
  }
  reason = "unsupported gauge tool";
  return false;
}

bool ValidateManualGaugeGeometry(const ManualGaugeState &gauge,
                                 std::string &reason) {
  if (!gauge.accepted) {
    reason = "gauge is not accepted";
    return false;
  }
  if (gauge.review_status != "manual_accepted") {
    reason = "review_status is not manual_accepted";
    return false;
  }
  if (gauge.dirty) {
    reason = "gauge was edited after acceptance";
    return false;
  }
  return ValidateManualGaugeGeometryForEditing(gauge, reason);
}

void NormalizeManualGaugeGeometry(ManualGaugeState &gauge) {
  if (gauge.has_circle_gauge) {
    const double dx = static_cast<double>(gauge.circle_px - gauge.circle_cx);
    const double dy = static_cast<double>(gauge.circle_py - gauge.circle_cy);
    gauge.radius = static_cast<int>(std::lround(std::sqrt(dx * dx + dy * dy)));
  }

  // FindEllipse persists an image-axis-aligned bounding box.  Normalize only
  // the endpoint order on each axis.  Never sort width/height or exchange the
  // X/Y axes: doing so turns a horizontal ellipse into a vertical ellipse
  // when a candidate is saved and restored.
  if (gauge.has_ellipse_gauge) {
    if (gauge.ellipse_x0 > gauge.ellipse_x1)
      std::swap(gauge.ellipse_x0, gauge.ellipse_x1);
    if (gauge.ellipse_y0 > gauge.ellipse_y1)
      std::swap(gauge.ellipse_y0, gauge.ellipse_y1);
    gauge.ellipse_inner_scale_percent =
        std::max(0, std::min(99, gauge.ellipse_inner_scale_percent));
  }
  gauge.min_edge_run_width_px =
      std::max(1, std::min(20, gauge.min_edge_run_width_px));
}

bool ApplyManualGaugeToGlobals(ManualTestContext &context,
                               const std::string &objectName) {
  (void)objectName;
  return ApplyManualGaugeToGlobals(context);
}

bool ApplyManualGaugeToGlobals(ManualTestContext &context) {
  ManualGaugeState &gauge = context.current_gauge;
  std::string reason;
  if (!ValidateManualGaugeGeometryForEditing(gauge, reason)) {
    context.debug_status = "gauge_apply_failed";
    context.debug_reason = reason;
    return false;
  }
  if (gauge.tool == "FindLine" || gauge.tool == "FindRect") {
    InjectManualGaugeInt(context, "global_roi_x0", gauge.line_x0);
    InjectManualGaugeInt(context, "global_roi_y0", gauge.line_y0);
    InjectManualGaugeInt(context, "global_roi_x1", gauge.line_x1);
    InjectManualGaugeInt(context, "global_roi_y1", gauge.line_y1);
    InjectManualGaugeInt(context, "global_roi_x",
                         std::min(gauge.line_x0, gauge.line_x1));
    InjectManualGaugeInt(context, "global_roi_y",
                         std::min(gauge.line_y0, gauge.line_y1));
    InjectManualGaugeInt(context, "global_roi_width",
                         std::abs(gauge.line_x1 - gauge.line_x0));
    InjectManualGaugeInt(context, "global_roi_height",
                         std::abs(gauge.line_y1 - gauge.line_y0));
    InjectManualGaugeInt(context, "global_tool_half_width",
                         gauge.tool_half_width);
    InjectManualGaugeInt(context, "global_wgap", gauge.wgap);
    InjectManualGaugeInt(context, "global_hgap", gauge.hgap);
    InjectManualGaugeInt(context, "global_compare_gap",
                         gauge.gap > 0 ? gauge.gap : gauge.linegap);
    InjectManualGaugeInt(context, "global_gauge", gauge.tool_half_width);
    gauge.scan_direction = gauge.scan_direction == 1 ? 1 : 2;
    InjectManualGaugeInt(context, "global_findline_scan_direction",
                         gauge.scan_direction);
    InjectManualGaugeInt(context, "global_linegap", gauge.linegap);
    if (gauge.tool == "FindLine" || gauge.tool == "FindCircle" ||
        gauge.tool == "FindEllipse") {
      gauge.min_edge_run_width_px =
          std::max(1, std::min(20, gauge.min_edge_run_width_px));
      InjectManualGaugeInt(context, "global_min_edge_run_width_px",
                           gauge.min_edge_run_width_px);
    }
    InjectManualGaugeInt(context, "global_threshold", gauge.threshold);
    InjectManualGaugeInt(context, "global_filterprofile", gauge.filterprofile);
    InjectManualGaugeInt(context, "global_method", gauge.method);
    const int findsetting = ResolveManualGaugeFindSetting(gauge);
    InjectManualGaugeInt(context, "global_findsetting", findsetting);
    InjectManualGaugeInt(context, "global_objfilter", findsetting);
    if (gauge.tool == "FindRect")
      InjectManualGaugeInt(context, "global_findrect_findsetting", findsetting);
    else {
      InjectManualGaugeInt(context, "global_findline_objfilter", findsetting);
      InjectManualGaugeInt(context, "global_findline_findsetting", findsetting);
    }
    InjectManualGaugeInt(context, "global_findline_point_consistency_enabled",
                         context.findline_point_consistency_enabled ? 1 : 0);
    const int pointConsistencyDefaultRange =
        std::max(1, gauge.tool_half_width / 2);
    const int pointConsistencyRange =
        context.findline_point_consistency_range > 0
            ? context.findline_point_consistency_range
            : pointConsistencyDefaultRange;
    context.findline_point_consistency_range = pointConsistencyRange;
    InjectManualGaugeInt(context, "global_findline_point_consistency_range",
                         pointConsistencyRange);

    context.findline_scan_edge_count =
        std::max(1, std::min(16, context.findline_scan_edge_count));
    context.findline_selected_scan_edge =
        std::max(-1, std::min(context.findline_selected_scan_edge,
                              context.findline_scan_edge_count));
    const int runtimeSelectedEdge = NormalizeFindLineSelectedEdgeForRuntime(
        context.findline_selected_scan_edge, context.findline_scan_edge_count);
    if (context.findline_edge_params.size() <
        static_cast<std::size_t>(context.findline_scan_edge_count + 1)) {
      context.findline_edge_params.resize(
          static_cast<std::size_t>(context.findline_scan_edge_count + 1));
    }

    InjectManualGaugeInt(context, "global_findline_edge_count",
                         context.findline_scan_edge_count);
    InjectManualGaugeInt(context, "global_findline_selected_edge",
                         runtimeSelectedEdge);
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
    InjectManualGaugeInt(context, "global_findline_best_edge",
                         context.findline_best_fit_edge);
    InjectManualGaugeInt(context, "global_findline_recommended_edge",
                         context.findline_recommended_fit_edge);
    InjectManualGaugeInt(context, "global_findline_relation_edge",
                         context.findline_relation_edge);
    InjectManualGaugeInt(context, "global_findline_attach_edge",
                         context.findline_attach_edge);

    for (int edge = 1; edge <= context.findline_scan_edge_count; ++edge) {
      ManualFindLineEdgeParamState &params =
          context.findline_edge_params[static_cast<std::size_t>(edge)];
      if (!params.initialized) {
        params.initialized = true;
        params.threshold = gauge.threshold;
        params.method = gauge.method;
        params.linegap = gauge.linegap;
        params.wgap = gauge.wgap;
        params.hgap = gauge.hgap;
        params.filterprofile = gauge.filterprofile;
      }
      const std::string prefix =
          "global_findline_edge" + std::to_string(edge) + "_";
      InjectManualGaugeInt(context, prefix + "threshold", params.threshold);
      InjectManualGaugeInt(context, prefix + "method", params.method);
      InjectManualGaugeInt(context, prefix + "linegap", params.linegap);
      InjectManualGaugeInt(context, prefix + "wgap", params.wgap);
      InjectManualGaugeInt(context, prefix + "hgap", params.hgap);
      InjectManualGaugeInt(context, prefix + "filterprofile",
                           params.filterprofile);
    }

    if (context.findline_selected_scan_edge > 0) {
      const ManualFindLineEdgeParamState &selected =
          context.findline_edge_params[static_cast<std::size_t>(
              context.findline_selected_scan_edge)];
      InjectManualGaugeInt(context, "global_findline_selected_threshold",
                           selected.threshold);
      InjectManualGaugeInt(context, "global_findline_selected_method",
                           selected.method);
      InjectManualGaugeInt(context, "global_findline_selected_linegap",
                           selected.linegap);
      InjectManualGaugeInt(context, "global_findline_selected_wgap",
                           selected.wgap);
      InjectManualGaugeInt(context, "global_findline_selected_hgap",
                           selected.hgap);
      InjectManualGaugeInt(context, "global_findline_selected_filterprofile",
                           selected.filterprofile);
    }
  } else if (gauge.tool == "FindCircle") {
    NormalizeManualGaugeGeometry(gauge);
    int effectiveOuterRadius =
        gauge.outer_radius > 0 ? gauge.outer_radius : gauge.radius;
    if (effectiveOuterRadius <= 0)
      effectiveOuterRadius = gauge.radius;
    if (effectiveOuterRadius <= 0)
      effectiveOuterRadius = 1;

    int effectiveInnerRadius = gauge.inner_radius > 0 ? gauge.inner_radius : 0;
    if (effectiveInnerRadius >= effectiveOuterRadius)
      effectiveInnerRadius = std::max(0, effectiveOuterRadius - 1);

    const double dx = static_cast<double>(gauge.circle_px - gauge.circle_cx);
    const double dy = static_cast<double>(gauge.circle_py - gauge.circle_cy);
    const double len = std::sqrt(dx * dx + dy * dy);
    const double ux = len > 1.0 ? dx / len : 1.0;
    const double uy = len > 1.0 ? dy / len : 0.0;
    gauge.radius = effectiveOuterRadius;
    gauge.circle_px = gauge.circle_cx +
                      static_cast<int>(std::lround(ux * effectiveOuterRadius));
    gauge.circle_py = gauge.circle_cy +
                      static_cast<int>(std::lround(uy * effectiveOuterRadius));
    gauge.inner_radius = effectiveInnerRadius;
    gauge.outer_radius = effectiveOuterRadius;

    InjectManualGaugeInt(context, "global_circle_cx", gauge.circle_cx);
    InjectManualGaugeInt(context, "global_circle_cy", gauge.circle_cy);
    InjectManualGaugeInt(context, "global_circle_px", gauge.circle_px);
    InjectManualGaugeInt(context, "global_circle_py", gauge.circle_py);
    InjectManualGaugeInt(context, "global_circle_inner_radius",
                         gauge.inner_radius);
    InjectManualGaugeInt(context, "global_circle_outer_radius",
                         gauge.outer_radius);
    InjectManualGaugeInt(context, "global_circle_ring_width",
                         std::max(0, gauge.outer_radius - gauge.inner_radius));
    gauge.circle_arc_start_deg =
        std::max(-359, std::min(360, gauge.circle_arc_start_deg));
    gauge.circle_arc_end_deg =
        std::max(-359, std::min(360, gauge.circle_arc_end_deg));
    InjectManualGaugeInt(context, "global_findcircle_arc_enabled",
                         gauge.circle_arc_enabled ? 1 : 0);
    InjectManualGaugeInt(context, "global_findcircle_arc_start_deg",
                         gauge.circle_arc_start_deg);
    InjectManualGaugeInt(context, "global_findcircle_arc_end_deg",
                         gauge.circle_arc_end_deg);
    InjectManualGaugeInt(context, "global_gap", gauge.gap);
    InjectManualGaugeInt(context, "global_linegap", gauge.linegap);
    InjectManualGaugeInt(context, "global_threshold", gauge.threshold);
    InjectManualGaugeInt(context, "global_method", gauge.method);
    const int findsetting = ResolveManualGaugeFindSetting(gauge);
    InjectManualGaugeInt(context, "global_findsetting", findsetting);
    InjectManualGaugeInt(context, "global_findcircle_findsetting", findsetting);
    const int circleConsistencyDefaultRange =
        std::max(1, std::max(1, gauge.outer_radius - gauge.inner_radius) / 2);
    const int circleConsistencyRange =
        context.findcircle_point_consistency_range > 0
            ? context.findcircle_point_consistency_range
            : circleConsistencyDefaultRange;
    context.findcircle_point_consistency_range = circleConsistencyRange;
    InjectManualGaugeInt(context, "global_findcircle_point_consistency_enabled",
                         context.findcircle_point_consistency_enabled ? 1 : 0);
    InjectManualGaugeInt(context, "global_findcircle_point_consistency_range",
                         circleConsistencyRange);

    context.findcircle_scan_edge_count =
        std::max(1, std::min(32, context.findcircle_scan_edge_count));
    context.findcircle_selected_scan_edge =
        std::max(-1, std::min(context.findcircle_selected_scan_edge,
                              context.findcircle_scan_edge_count));
    if (context.findcircle_edge_params.size() <
        static_cast<std::size_t>(context.findcircle_scan_edge_count + 1)) {
      context.findcircle_edge_params.resize(
          static_cast<std::size_t>(context.findcircle_scan_edge_count + 1));
    }

    InjectManualGaugeInt(context, "global_findcircle_edge_count",
                         context.findcircle_scan_edge_count);
    InjectManualGaugeInt(context, "global_findcircle_selected_edge",
                         context.findcircle_selected_scan_edge);
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
    InjectManualGaugeInt(context, "global_findcircle_best_edge",
                         context.findcircle_best_fit_edge);
    InjectManualGaugeInt(context, "global_findcircle_recommended_edge",
                         context.findcircle_recommended_fit_edge);
    InjectManualGaugeInt(context, "global_findcircle_relation_edge",
                         context.findcircle_relation_edge);
    InjectManualGaugeInt(context, "global_findcircle_attach_edge",
                         context.findcircle_attach_edge);

    for (int edge = 1; edge <= context.findcircle_scan_edge_count; ++edge) {
      ManualFindCircleEdgeParamState &params =
          context.findcircle_edge_params[static_cast<std::size_t>(edge)];
      // One FindCircle object owns one parameter set. Edge N is only a
      // boundary-candidate ordinal, so legacy edge globals mirror the
      // shared parameters instead of retaining divergent profiles.
      params.initialized = true;
      params.threshold = gauge.threshold;
      params.method = gauge.method;
      params.linegap = gauge.linegap;
      params.gap = gauge.gap;
      const std::string prefix =
          "global_findcircle_edge" + std::to_string(edge) + "_";
      InjectManualGaugeInt(context, prefix + "threshold", params.threshold);
      InjectManualGaugeInt(context, prefix + "method", params.method);
      InjectManualGaugeInt(context, prefix + "linegap", params.linegap);
      InjectManualGaugeInt(context, prefix + "gap", params.gap);
    }

    if (context.findcircle_selected_scan_edge > 0) {
      InjectManualGaugeInt(context, "global_findcircle_selected_threshold",
                           gauge.threshold);
      InjectManualGaugeInt(context, "global_findcircle_selected_method",
                           gauge.method);
      InjectManualGaugeInt(context, "global_findcircle_selected_linegap",
                           gauge.linegap);
      InjectManualGaugeInt(context, "global_findcircle_selected_gap",
                           gauge.gap);
    }
  } else if (gauge.tool == "FindEllipse") {
    const int x0 = std::min(gauge.ellipse_x0, gauge.ellipse_x1);
    const int y0 = std::min(gauge.ellipse_y0, gauge.ellipse_y1);
    const int x1 = std::max(gauge.ellipse_x0, gauge.ellipse_x1);
    const int y1 = std::max(gauge.ellipse_y0, gauge.ellipse_y1);
    gauge.ellipse_x0 = x0;
    gauge.ellipse_y0 = y0;
    gauge.ellipse_x1 = x1;
    gauge.ellipse_y1 = y1;

    InjectManualGaugeInt(context, "global_ellipse_x0", x0);
    InjectManualGaugeInt(context, "global_ellipse_y0", y0);
    InjectManualGaugeInt(context, "global_ellipse_x1", x1);
    InjectManualGaugeInt(context, "global_ellipse_y1", y1);
    gauge.ellipse_inner_scale_percent =
        std::max(0, std::min(99, gauge.ellipse_inner_scale_percent));
    InjectManualGaugeInt(context, "global_findellipse_inner_scale_percent",
                         gauge.ellipse_inner_scale_percent);
    // Compatibility only: FindEllipse scripts must read global_ellipse_*.
    // Mirror to global_roi_* so older working/candidate snapshots do not
    // jump back to a stale FindLine ROI during manual review.
    InjectManualGaugeInt(context, "global_roi_x0", x0);
    InjectManualGaugeInt(context, "global_roi_y0", y0);
    InjectManualGaugeInt(context, "global_roi_x1", x1);
    InjectManualGaugeInt(context, "global_roi_y1", y1);
    InjectManualGaugeInt(context, "global_roi_x", x0);
    InjectManualGaugeInt(context, "global_roi_y", y0);
    InjectManualGaugeInt(context, "global_roi_width", std::abs(x1 - x0));
    InjectManualGaugeInt(context, "global_roi_height", std::abs(y1 - y0));
    InjectManualGaugeInt(context, "global_gap", gauge.gap);
    InjectManualGaugeInt(context, "global_linegap", gauge.linegap);
    InjectManualGaugeInt(context, "global_threshold", gauge.threshold);
    InjectManualGaugeInt(context, "global_method", gauge.method);
    const int findsetting = ResolveManualGaugeFindSetting(gauge);
    InjectManualGaugeInt(context, "global_findsetting", findsetting);
    InjectManualGaugeInt(context, "global_findellipse_findsetting",
                         findsetting);

    const int ellipseConsistencyDefaultRange =
        std::max(1, std::max(1, gauge.gap) / 2);
    const int ellipseConsistencyRange =
        context.findellipse_point_consistency_range > 0
            ? context.findellipse_point_consistency_range
            : ellipseConsistencyDefaultRange;
    context.findellipse_point_consistency_range = ellipseConsistencyRange;
    InjectManualGaugeInt(context,
                         "global_findellipse_point_consistency_enabled",
                         context.findellipse_point_consistency_enabled ? 1 : 0);
    InjectManualGaugeInt(context, "global_findellipse_point_consistency_range",
                         ellipseConsistencyRange);

    context.findellipse_scan_edge_count =
        std::max(1, std::min(3, context.findellipse_scan_edge_count));
    context.findellipse_selected_scan_edge =
        std::max(-1, std::min(context.findellipse_selected_scan_edge,
                              context.findellipse_scan_edge_count));
    if (context.findellipse_edge_params.size() <
        static_cast<std::size_t>(context.findellipse_scan_edge_count + 1)) {
      context.findellipse_edge_params.resize(
          static_cast<std::size_t>(context.findellipse_scan_edge_count + 1));
    }

    InjectManualGaugeInt(context, "global_findellipse_edge_count",
                         context.findellipse_scan_edge_count);
    InjectManualGaugeInt(context, "global_findellipse_selected_edge",
                         context.findellipse_selected_scan_edge);
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
    InjectManualGaugeInt(context, "global_findellipse_best_edge",
                         context.findellipse_best_fit_edge);
    InjectManualGaugeInt(context, "global_findellipse_recommended_edge",
                         context.findellipse_recommended_fit_edge);
    InjectManualGaugeInt(context, "global_findellipse_relation_edge",
                         context.findellipse_relation_edge);
    InjectManualGaugeInt(context, "global_findellipse_attach_edge",
                         context.findellipse_attach_edge);

    for (int edge = 1; edge <= context.findellipse_scan_edge_count; ++edge) {
      ManualFindCircleEdgeParamState &params =
          context.findellipse_edge_params[static_cast<std::size_t>(edge)];
      params.initialized = true;
      params.threshold = gauge.threshold;
      params.method = gauge.method;
      params.linegap = gauge.linegap;
      params.gap = gauge.gap;
      const std::string prefix =
          "global_findellipse_edge" + std::to_string(edge) + "_";
      InjectManualGaugeInt(context, prefix + "threshold", params.threshold);
      InjectManualGaugeInt(context, prefix + "method", params.method);
      InjectManualGaugeInt(context, prefix + "linegap", params.linegap);
      InjectManualGaugeInt(context, prefix + "gap", params.gap);
    }

    if (context.findellipse_selected_scan_edge > 0) {
      InjectManualGaugeInt(context, "global_findellipse_selected_threshold",
                           gauge.threshold);
      InjectManualGaugeInt(context, "global_findellipse_selected_method",
                           gauge.method);
      InjectManualGaugeInt(context, "global_findellipse_selected_linegap",
                           gauge.linegap);
      InjectManualGaugeInt(context, "global_findellipse_selected_gap",
                           gauge.gap);
    }

    std::ostringstream ellipseGlobals;
    ellipseGlobals << "bbox=(" << x0 << "," << y0 << "," << x1 << "," << y1
                   << ")"
                   << " inner_scale_percent="
                   << gauge.ellipse_inner_scale_percent << " gap=" << gauge.gap
                   << " linegap=" << gauge.linegap
                   << " threshold=" << gauge.threshold
                   << " method=" << gauge.method
                   << " findsetting=" << findsetting << " selected_edge="
                   << context.findellipse_selected_scan_edge << " consistency="
                   << (context.findellipse_point_consistency_enabled ? 1 : 0)
                   << "/" << context.findellipse_point_consistency_range;
    CXLOG_INFO("ManualConsole", "findellipse_globals_injected", "updated",
               ellipseGlobals.str());
  } else if (IsRegionPatternGaugeTool(gauge.tool)) {
    InjectManualGaugeInt(
        context, "global_region_roi_x",
        std::max(0, ReadManualGaugeInt(context, "global_region_roi_x", 120)));
    InjectManualGaugeInt(
        context, "global_region_roi_y",
        std::max(0, ReadManualGaugeInt(context, "global_region_roi_y", 120)));
    InjectManualGaugeInt(
        context, "global_region_roi_w",
        std::max(1, ReadManualGaugeInt(context, "global_region_roi_w", 120)));
    InjectManualGaugeInt(
        context, "global_region_roi_h",
        std::max(1, ReadManualGaugeInt(context, "global_region_roi_h", 90)));
    InjectManualGaugeInt(
        context, "global_region_normalized_width",
        std::max(8, ReadManualGaugeInt(context,
                                       "global_region_normalized_width", 32)));
    InjectManualGaugeInt(
        context, "global_region_normalized_height",
        std::max(8, ReadManualGaugeInt(context,
                                       "global_region_normalized_height", 32)));
    InjectManualGaugeInt(
        context, "global_region_pooling_rows",
        std::max(1,
                 ReadManualGaugeInt(context, "global_region_pooling_rows", 4)));
    InjectManualGaugeInt(
        context, "global_region_pooling_cols",
        std::max(1,
                 ReadManualGaugeInt(context, "global_region_pooling_cols", 4)));
    InjectManualGaugeInt(
        context, "global_region_use_binary",
        std::max(0, std::min(1, ReadManualGaugeInt(
                                    context, "global_region_use_binary", 0))));
    InjectManualGaugeInt(
        context, "global_region_threshold",
        std::max(0,
                 std::min(255, ReadManualGaugeInt(
                                   context, "global_region_threshold", 128))));
    InjectManualGaugeInt(
        context, "global_region_foreground_dark",
        std::max(
            0, std::min(1, ReadManualGaugeInt(
                               context, "global_region_foreground_dark", 1))));
    InjectManualGaugeInt(
        context, "global_region_max_overlays",
        std::max(
            1, ReadManualGaugeInt(context, "global_region_max_overlays", 64)));
  } else if (IsFastMatchGaugeTool(gauge.tool) ||
             IsGridPatternGaugeTool(gauge.tool)) {
    const int learnX =
        std::max(0, ReadManualGaugeInt(context, "global_learn_roi_x", 120));
    const int learnY =
        std::max(0, ReadManualGaugeInt(context, "global_learn_roi_y", 120));
    const int learnW =
        std::max(1, ReadManualGaugeInt(context, "global_learn_roi_w", 120));
    const int learnH =
        std::max(1, ReadManualGaugeInt(context, "global_learn_roi_h", 90));
    int searchX =
        std::max(0, ReadManualGaugeInt(context, "global_search_roi_x", 0));
    int searchY =
        std::max(0, ReadManualGaugeInt(context, "global_search_roi_y", 0));
    int searchW =
        std::max(1, ReadManualGaugeInt(context, "global_search_roi_w", 640));
    int searchH =
        std::max(1, ReadManualGaugeInt(context, "global_search_roi_h", 480));

    if (IsFastMatchGaugeTool(gauge.tool)) {
      const int searchRight = std::max(searchX + searchW, learnX + learnW);
      const int searchBottom = std::max(searchY + searchH, learnY + learnH);
      searchX = std::min(searchX, learnX);
      searchY = std::min(searchY, learnY);
      searchW = std::max(1, searchRight - searchX);
      searchH = std::max(1, searchBottom - searchY);
    }

    InjectManualGaugeInt(context, "global_learn_roi_x", learnX);
    InjectManualGaugeInt(context, "global_learn_roi_y", learnY);
    InjectManualGaugeInt(context, "global_learn_roi_w", learnW);
    InjectManualGaugeInt(context, "global_learn_roi_h", learnH);
    InjectManualGaugeInt(context, "global_search_roi_x", searchX);
    InjectManualGaugeInt(context, "global_search_roi_y", searchY);
    InjectManualGaugeInt(context, "global_search_roi_w", searchW);
    InjectManualGaugeInt(context, "global_search_roi_h", searchH);

    InjectManualGaugeInt(context, "global_wgap", gauge.wgap);
    InjectManualGaugeInt(context, "global_hgap", gauge.hgap);
    InjectManualGaugeInt(context, "global_linegap", gauge.linegap);
    InjectManualGaugeInt(context, "global_threshold", gauge.threshold);
    InjectManualGaugeInt(context, "global_filterprofile", gauge.filterprofile);
    InjectManualGaugeInt(context, "global_method", gauge.method);
    InjectManualGaugeInt(
        context, "global_compare_gap",
        std::max(1, ReadManualGaugeInt(context, "global_compare_gap", 20)));
    InjectManualGaugeInt(
        context, "global_objfilter",
        std::max(0, ReadManualGaugeInt(context, "global_objfilter", 1)));
    InjectManualGaugeInt(
        context, "global_find_num",
        std::max(1, ReadManualGaugeInt(context, "global_find_num", 1)));
    InjectManualGaugeInt(
        context, "global_match_step_x",
        std::max(1, ReadManualGaugeInt(context, "global_match_step_x", 10)));
    InjectManualGaugeInt(
        context, "global_match_step_y",
        std::max(1, ReadManualGaugeInt(context, "global_match_step_y", 10)));
    InjectManualGaugeInt(
        context, "global_match_thre",
        std::max(0, ReadManualGaugeInt(context, "global_match_thre", 10)));
    const int minScorePercent =

        std::max(0,
                 std::min(100, ReadManualGaugeInt(

                                   context, "global_min_score_percent", 65)));

    InjectManualGaugeInt(context, "global_min_score_percent", minScorePercent);

    InjectManualGaugeInt(context, "global_min_score", minScorePercent);
  } else if (IsFindSegmentationGaugeTool(gauge.tool)) {
    SyncSegmentationLegacyPointFromLists(gauge);
    int x0 = std::max(0, gauge.segmentation_prompt_x0);
    int y0 = std::max(0, gauge.segmentation_prompt_y0);
    int x1 = std::max(x0 + 1, gauge.segmentation_prompt_x1);
    int y1 = std::max(y0 + 1, gauge.segmentation_prompt_y1);
    gauge.segmentation_prompt_x0 = x0;
    gauge.segmentation_prompt_y0 = y0;
    gauge.segmentation_prompt_x1 = x1;
    gauge.segmentation_prompt_y1 = y1;
    gauge.has_segmentation_prompt_rect = true;

    InjectManualGaugeInt(context, "global_roi_x0", x0);
    InjectManualGaugeInt(context, "global_roi_y0", y0);
    InjectManualGaugeInt(context, "global_roi_x1", x1);
    InjectManualGaugeInt(context, "global_roi_y1", y1);
    InjectManualGaugeInt(context, "global_roi_x", x0);
    InjectManualGaugeInt(context, "global_roi_y", y0);
    InjectManualGaugeInt(context, "global_roi_width", x1 - x0);
    InjectManualGaugeInt(context, "global_roi_height", y1 - y0);
    InjectManualGaugeInt(context, "global_segmentation_mode",
                         gauge.segmentation_mode);
    InjectManualGaugeInt(
        context, "global_segmentation_threshold_percent",
        std::max(0, std::min(100, gauge.segmentation_threshold_percent)));
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
    // Existing tool code still reads global_threshold. Keep it aligned to
    // the explicit percentage rather than silently using a line gauge.
    InjectManualGaugeInt(
        context, "global_threshold",
        std::max(0, std::min(100, gauge.segmentation_threshold_percent)));
  } else if (IsFindObjectGaugeTool(gauge.tool)) {
    const int x0 = std::max(0, gauge.findobject_x0);
    const int y0 = std::max(0, gauge.findobject_y0);
    const int x1 = std::max(x0 + 1, gauge.findobject_x1);
    const int y1 = std::max(y0 + 1, gauge.findobject_y1);
    gauge.findobject_x0 = x0;
    gauge.findobject_y0 = y0;
    gauge.findobject_x1 = x1;
    gauge.findobject_y1 = y1;
    gauge.has_findobject_roi = true;
    InjectManualGaugeInt(context, "global_roi_x0", x0);
    InjectManualGaugeInt(context, "global_roi_y0", y0);
    InjectManualGaugeInt(context, "global_roi_x1", x1);
    InjectManualGaugeInt(context, "global_roi_y1", y1);
    InjectManualGaugeInt(context, "global_roi_x", x0);
    InjectManualGaugeInt(context, "global_roi_y", y0);
    InjectManualGaugeInt(context, "global_roi_width", x1 - x0);
    InjectManualGaugeInt(context, "global_roi_height", y1 - y0);
    InjectManualGaugeInt(
        context, "global_object_foreground_mode",
        std::max(1, std::min(3, gauge.findobject_foreground_mode)));
    InjectManualGaugeInt(
        context, "global_object_threshold",
        std::max(0, std::min(255, gauge.findobject_threshold)));
    InjectManualGaugeInt(context, "global_object_min_area",
                         std::max(1, gauge.findobject_min_area));

    gauge.findobject_pixel_size_x =
        std::max(1e-9, gauge.findobject_pixel_size_x);
    gauge.findobject_pixel_size_y =
        std::max(1e-9, gauge.findobject_pixel_size_y);
    gauge.findobject_geometry_connectivity =
        gauge.findobject_geometry_connectivity == 4 ? 4 : 8;
    gauge.findobject_selected_measurement =
        std::max(0, gauge.findobject_selected_measurement);
    InjectManualGaugeInt(
        context, "global_object_pixel_size_x_milli",
        static_cast<int>(gauge.findobject_pixel_size_x * 1000.0 + 0.5));
    InjectManualGaugeInt(
        context, "global_object_pixel_size_y_milli",
        static_cast<int>(gauge.findobject_pixel_size_y * 1000.0 + 0.5));
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
    InjectManualGaugeInt(context, "global_object_conclusion_shape",
                         std::max(0, std::min(4, gauge.findobject_conclusion_shape)));
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
        static_cast<int>(gauge.findobject_subpixel_min_gradient * 1000.0 + 0.5));
    InjectManualGaugeInt(
        context, "global_object_subpixel_iso_threshold",
        static_cast<int>(gauge.findobject_subpixel_iso_threshold + 0.5));
    InjectManualGaugeInt(context, "global_object_geometry_basis",
                         gauge.findobject_geometry_basis);
    // find_object_direct_test.cxsc reads the standard cximage globals.
    // Keep them synchronized with the FindObject-specific UI fields so
    // Apply/Run uses exactly the rectangle and filtering shown in the UI.
    InjectManualGaugeInt(
        context, "global_threshold",
        std::max(0, std::min(255, gauge.findobject_threshold)));
    InjectManualGaugeInt(
        context, "global_method",
        std::max(0, std::min(3, gauge.findobject_foreground_mode)));
    InjectManualGaugeInt(context, "global_gap", std::max(0, gauge.gap));
    InjectManualGaugeInt(context, "global_filterprofile",
                         std::max(0, std::min(10, gauge.filterprofile)));
    const int findsetting = ResolveManualGaugeFindSetting(gauge);
    InjectManualGaugeInt(context, "global_findsetting", findsetting);
    InjectManualGaugeInt(context, "global_findobject_findsetting", findsetting);
    InjectManualGaugeInt(context, "global_objfilter", findsetting);
  } else {
    context.debug_status = "gauge_apply_failed";
    context.debug_reason = "unsupported gauge tool: " + gauge.tool;
    return false;
  }
  gauge.review_status = "applied_to_globals";
  gauge.accepted = false;
  context.debug_status = "gauge_applied";
  context.debug_reason.clear();
  return true;
}

std::filesystem::path ManualGaugeCaseDir(const ManualTestContext &context) {
  std::filesystem::path out;
  std::string reason;
  if (!ResolveManualGaugeCaseDir(context, out, reason))
    return {};
  return out;
}

bool ResolveManualGaugeCaseDir(const ManualTestContext &context,
                               std::filesystem::path &out,
                               std::string &reason) {
  const std::string case_id = context.current_gauge.case_id.empty()
                                  ? context.active_case_id
                                  : context.current_gauge.case_id;
  const std::string safe_case_id = SafePathComponent(case_id);
  if (safe_case_id.empty()) {
    reason = "case_id is empty";
    return false;
  }
  if (context.manual_gauge_output_root.empty()) {
    reason = "manual_gauge_output_root is empty";
    return false;
  }
  std::error_code ec;
  std::filesystem::path root =
      std::filesystem::absolute(context.manual_gauge_output_root, ec);
  if (ec) {
    reason = "cannot resolve manual gauge output root: " + ec.message();
    return false;
  }
  out = (root / safe_case_id).lexically_normal();
  const std::filesystem::path relative =
      out.lexically_relative(root.lexically_normal());
  if (relative.empty() || relative.native().find(L"..") == 0) {
    reason = "resolved case directory escapes output root";
    return false;
  }
  reason.clear();
  return true;
}

bool SaveManualGaugeAnnotation(ManualTestContext &context,
                               const std::string &objectName,
                               const std::string &gaugeName,
                               std::string &outPath, std::string &outReason) {
  (void)objectName;
  (void)gaugeName;
  ManualGaugeState &gauge = context.current_gauge;
  NormalizeManualGaugeGeometry(gauge);
  SyncSegmentationLegacyPointFromLists(gauge);
  if (!ValidateManualGaugeGeometry(gauge, outReason))
    return false;
  if (!ApplyManualGaugeToGlobals(context)) {
    outReason = "cannot apply gauge to runtime globals before save: " +
                context.debug_reason;
    return false;
  }

  std::filesystem::path dir;
  if (!ResolveManualGaugeCaseDir(context, dir, outReason))
    return false;
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  if (ec) {
    outReason = "cannot create gauge case directory: " + ec.message();
    return false;
  }

  const std::filesystem::path destination = dir / "gauge_annotation.json";
  const std::filesystem::path temporary = destination.string() + ".tmp";
  std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
  if (!file.is_open()) {
    outReason = "cannot open temporary gauge annotation";
    return false;
  }
  file << "{\n"
       << "  \"schema_version\": 1,\n"
       << "  \"case_id\": \"" << GaugeJsonEscape(gauge.case_id) << "\",\n"
       << "  \"image_id\": \"" << GaugeJsonEscape(gauge.image_id) << "\",\n"
       << "  \"target_id\": \"" << GaugeJsonEscape(gauge.target_id) << "\",\n"
       << "  \"tool\": \"" << GaugeJsonEscape(gauge.tool) << "\",\n"
       << "  \"source\": \"" << GaugeJsonEscape(gauge.source) << "\",\n"
       << "  \"review_status\": \"" << GaugeJsonEscape(gauge.review_status)
       << "\",\n"
       << "  \"accepted\": " << (gauge.accepted ? "true" : "false") << ",\n"
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
  WriteSegmentationPromptPointArray(file, "segmentation_positive_points",
                                    gauge.segmentation_positive_points, true);
  WriteSegmentationPromptPointArray(file, "segmentation_negative_points",
                                    gauge.segmentation_negative_points, true);
  file << "  \"segmentation_prompt_pick_mode\": "
       << gauge.segmentation_prompt_pick_mode << ",\n"
       << "  \"has_findobject_roi\": "
       << (gauge.has_findobject_roi ? "true" : "false") << ",\n"
       << "  \"findobject_x0\": " << gauge.findobject_x0 << ",\n"
       << "  \"findobject_y0\": " << gauge.findobject_y0 << ",\n"
       << "  \"findobject_x1\": " << gauge.findobject_x1 << ",\n"
       << "  \"findobject_y1\": " << gauge.findobject_y1 << ",\n"
       << "  \"findobject_foreground_mode\": "
       << gauge.findobject_foreground_mode << ",\n"
       << "  \"findobject_threshold\": " << gauge.findobject_threshold << ",\n"
       << "  \"findobject_min_area\": " << gauge.findobject_min_area << ",\n"
       << "  \"findobject_selected_measurement\": "
       << gauge.findobject_selected_measurement << ",\n"
       << "  \"findobject_show_all_conclusions\": "
       << (gauge.findobject_show_all_conclusions ? "true" : "false") << ",\n"
       << "  \"findobject_conclusion_shape\": "
       << gauge.findobject_conclusion_shape << ",\n"
       << "  \"wgap\": " << gauge.wgap << ",\n"
       << "  \"hgap\": " << gauge.hgap << ",\n"
       << "  \"scan_direction\": " << gauge.scan_direction << ",\n"
       << "  \"gap\": " << gauge.gap << ",\n"
       << "  \"linegap\": " << gauge.linegap << ",\n"
       << "  \"min_edge_run_width_px\": " << gauge.min_edge_run_width_px
       << ",\n"
       << "  \"threshold\": " << gauge.threshold << ",\n"
       << "  \"filterprofile\": " << gauge.filterprofile << ",\n"
       << "  \"method\": " << gauge.method << ",\n"
       << "  \"findsetting\": " << ResolveManualGaugeFindSetting(gauge) << "\n"
       << "}\n";
  file.flush();
  const bool write_ok = file.good();
  file.close();
  if (!write_ok) {
    outReason = "failed while writing gauge annotation";
    std::filesystem::remove(temporary, ec);
    return false;
  }
  if (!AtomicReplaceFile(temporary, destination, outReason)) {
    std::filesystem::remove(temporary, ec);
    return false;
  }
  outPath = destination.string();
  outReason.clear();
  return true;
}

bool LoadManualGaugeAnnotation(ManualTestContext &context,
                               const std::string &objectName,
                               const std::string &gaugeName,
                               std::string &outPath, std::string &outReason) {
  (void)objectName;
  (void)gaugeName;
  std::filesystem::path dir;
  if (!ResolveManualGaugeCaseDir(context, dir, outReason))
    return false;
  const std::filesystem::path source_path = dir / "gauge_annotation.json";
  if (!LoadManualGaugeAnnotationFromPath(context, source_path, outReason))
    return false;
  outPath = source_path.string();
  return true;
}

static bool LoadManualGaugeAnnotationFromPathImpl(
    ManualTestContext &context, const std::filesystem::path &source_path,
    bool requireManualAcceptance, std::string &outReason) {
  std::ifstream file(source_path, std::ios::binary);
  if (!file.is_open()) {
    outReason = "cannot open gauge annotation: " + source_path.string();
    return false;
  }
  const std::string source{std::istreambuf_iterator<char>(file),
                           std::istreambuf_iterator<char>()};
  ManualGaugeState loaded;
  int schema_version = 0;
  if (!ExtractJsonInt(source, "schema_version", schema_version) ||
      schema_version != 1 ||
      !ExtractJsonString(source, "case_id", loaded.case_id) ||
      !ExtractJsonString(source, "image_id", loaded.image_id) ||
      !ExtractJsonString(source, "target_id", loaded.target_id) ||
      !ExtractJsonString(source, "tool", loaded.tool) ||
      !ExtractJsonString(source, "source", loaded.source) ||
      !ExtractJsonString(source, "review_status", loaded.review_status) ||
      !ExtractJsonBool(source, "accepted", loaded.accepted) ||
      !ExtractJsonBool(source, "has_line_gauge", loaded.has_line_gauge) ||
      !ExtractJsonBool(source, "has_circle_gauge", loaded.has_circle_gauge)) {
    outReason = "gauge annotation schema is incomplete";
    return false;
  }
  // Backward compatible for annotations saved before FindEllipse gauge fields.
  ExtractJsonBool(source, "has_ellipse_gauge", loaded.has_ellipse_gauge);
  // Backward compatible for annotations saved before FindSegmentation
  // prompt ROI fields.  These are optional unless the loaded tool is
  // FindSegmentation.
  ExtractJsonBool(source, "has_segmentation_prompt_rect",
                  loaded.has_segmentation_prompt_rect);
  ExtractJsonBool(source, "has_segmentation_positive_point",
                  loaded.has_segmentation_positive_point);
  ExtractJsonBool(source, "has_segmentation_negative_point",
                  loaded.has_segmentation_negative_point);
  ExtractSegmentationPromptPointArray(source, "segmentation_positive_points",
                                      loaded.segmentation_positive_points);
  ExtractSegmentationPromptPointArray(source, "segmentation_negative_points",
                                      loaded.segmentation_negative_points);
  ExtractJsonBool(source, "has_findobject_roi", loaded.has_findobject_roi);
  const bool hasCircleArcEnabled =
      ExtractJsonBool(source, "circle_arc_enabled", loaded.circle_arc_enabled);
  if (!hasCircleArcEnabled) {
    const auto it =
        context.runtime_int_vars.find("global_findcircle_arc_enabled");
    if (it != context.runtime_int_vars.end())
      loaded.circle_arc_enabled = it->second != 0;
  }
  const char *integer_keys[] = {"line_x0",
                                "line_y0",
                                "line_x1",
                                "line_y1",
                                "tool_half_width",
                                "circle_cx",
                                "circle_cy",
                                "circle_px",
                                "circle_py",
                                "radius",
                                "inner_radius",
                                "outer_radius",
                                "circle_arc_start_deg",
                                "circle_arc_end_deg",
                                "ellipse_x0",
                                "ellipse_y0",
                                "ellipse_x1",
                                "ellipse_y1",
                                "ellipse_inner_scale_percent",
                                "segmentation_prompt_x0",
                                "segmentation_prompt_y0",
                                "segmentation_prompt_x1",
                                "segmentation_prompt_y1",
                                "segmentation_mode",
                                "segmentation_threshold_percent",
                                "segmentation_positive_x",
                                "segmentation_positive_y",
                                "segmentation_negative_x",
                                "segmentation_negative_y",
                                "segmentation_prompt_pick_mode",
                                "findobject_x0",
                                "findobject_y0",
                                "findobject_x1",
                                "findobject_y1",
                                "findobject_foreground_mode",
                                "findobject_threshold",
                                "findobject_min_area",
                                "wgap",
                                "hgap",
                                "scan_direction",
                                "gap",
                                "linegap",
                                "min_edge_run_width_px",
                                "threshold",
                                "filterprofile",
                                "method",
                                "findsetting"};
  int *integer_values[] = {&loaded.line_x0,
                           &loaded.line_y0,
                           &loaded.line_x1,
                           &loaded.line_y1,
                           &loaded.tool_half_width,
                           &loaded.circle_cx,
                           &loaded.circle_cy,
                           &loaded.circle_px,
                           &loaded.circle_py,
                           &loaded.radius,
                           &loaded.inner_radius,
                           &loaded.outer_radius,
                           &loaded.circle_arc_start_deg,
                           &loaded.circle_arc_end_deg,
                           &loaded.ellipse_x0,
                           &loaded.ellipse_y0,
                           &loaded.ellipse_x1,
                           &loaded.ellipse_y1,
                           &loaded.ellipse_inner_scale_percent,
                           &loaded.segmentation_prompt_x0,
                           &loaded.segmentation_prompt_y0,
                           &loaded.segmentation_prompt_x1,
                           &loaded.segmentation_prompt_y1,
                           &loaded.segmentation_mode,
                           &loaded.segmentation_threshold_percent,
                           &loaded.segmentation_positive_x,
                           &loaded.segmentation_positive_y,
                           &loaded.segmentation_negative_x,
                           &loaded.segmentation_negative_y,
                           &loaded.segmentation_prompt_pick_mode,
                           &loaded.findobject_x0,
                           &loaded.findobject_y0,
                           &loaded.findobject_x1,
                           &loaded.findobject_y1,
                           &loaded.findobject_foreground_mode,
                           &loaded.findobject_threshold,
                           &loaded.findobject_min_area,
                           &loaded.wgap,
                           &loaded.hgap,
                           &loaded.scan_direction,
                           &loaded.gap,
                           &loaded.linegap,
                           &loaded.min_edge_run_width_px,
                           &loaded.threshold,
                           &loaded.filterprofile,
                           &loaded.method,
                           &loaded.findsetting};
  bool segmentationFallbackApplied = false;
  bool findObjectFallbackApplied = false;
  for (std::size_t i = 0; i < std::size(integer_keys); ++i) {
    if (!ExtractJsonInt(source, integer_keys[i], *integer_values[i])) {
      if (std::string(integer_keys[i]) == "ellipse_inner_scale_percent") {
        // This field was added after the first FindEllipse candidate
        // packages had already been persisted.  It is an optional
        // annulus extension, not part of the outer ROI geometry, so
        // an older package must remain selectable.  Do not inherit
        // the currently selected row's runtime global here: that
        // would leak one Evidence item's inner ellipse into another.
        // Packages predating this field semantically have no inner
        // ellipse.
        loaded.ellipse_inner_scale_percent = 0;
        continue;
      }
      if (std::string(integer_keys[i]) == "scan_direction") {
        // Older accepted annotations predate the exclusive W/H
        // selector.  Keep ManualGaugeState's H-only default.
        continue;
      }
      if (std::string(integer_keys[i]) == "min_edge_run_width_px") {
        const auto it =
            context.runtime_int_vars.find("global_min_edge_run_width_px");
        loaded.min_edge_run_width_px =
            it != context.runtime_int_vars.end()
                ? std::max(1, std::min(20, it->second))
                : 3;
        continue;
      }
      if (std::string(integer_keys[i]) == "findsetting") {
        // Older candidate packages may not contain object-prefilter
        // settings.  Only then use the tool default; never overwrite
        // an explicitly saved findsetting value.
        const auto globalIt = context.runtime_int_vars.find(
            loaded.tool == "FindCircle"
                ? "global_findcircle_findsetting"
                : (loaded.tool == "FindEllipse"
                       ? "global_findellipse_findsetting"
                       : (loaded.tool == "FindRect"
                              ? "global_findrect_findsetting"
                              : "global_findline_objfilter")));
        loaded.findsetting = globalIt != context.runtime_int_vars.end()
                                 ? std::max(0, globalIt->second)
                                 : DefaultFindSettingForTool(loaded.tool);
        continue;
      }
      if (std::string(integer_keys[i]).find("ellipse_") == 0 &&
          !loaded.has_ellipse_gauge) {
        continue;
      }
      if (std::string(integer_keys[i]).find("segmentation_") == 0 &&
          loaded.tool == "FindSegmentation") {
        if (!segmentationFallbackApplied) {
          ApplySegmentationPromptFallbacksFromGlobals(context, loaded);
          segmentationFallbackApplied = true;
        }
        continue;
      }
      if (std::string(integer_keys[i]).find("segmentation_") == 0 &&
          !loaded.has_segmentation_prompt_rect) {
        continue;
      }
      if (std::string(integer_keys[i]).find("findobject_") == 0 &&
          !loaded.has_findobject_roi && loaded.tool != "FindObject") {
        continue;
      }
      if (std::string(integer_keys[i]).find("findobject_") == 0 &&
          loaded.tool == "FindObject") {
        if (!findObjectFallbackApplied) {
          ApplyFindObjectGaugeFallbacksFromGlobals(context, loaded);
          findObjectFallbackApplied = true;
        }
        continue;
      }
      if (std::string(integer_keys[i]).find("circle_arc_") == 0) {
        // Candidate packages written before the sector fields were
        // added still contain the exact values in runtime_globals.
        // Restore from that already-loaded value instead of replacing
        // it with the full-circle struct defaults.
        const std::string globalName =
            std::string(integer_keys[i]) == "circle_arc_start_deg"
                ? "global_findcircle_arc_start_deg"
                : "global_findcircle_arc_end_deg";
        const auto it = context.runtime_int_vars.find(globalName);
        if (it != context.runtime_int_vars.end())
          *integer_values[i] = it->second;
        continue;
      }
      outReason = std::string("missing gauge field: ") + integer_keys[i];
      return false;
    }
  }
  if (loaded.tool == "FindSegmentation" &&
      (!loaded.has_segmentation_prompt_rect || segmentationFallbackApplied)) {
    ApplySegmentationPromptFallbacksFromGlobals(context, loaded);
  }
  if (loaded.tool == "FindSegmentation") {
    SyncSegmentationLegacyPointFromLists(loaded);
  }
  loaded.scan_direction = loaded.scan_direction == 1 ? 1 : 2;
  loaded.dirty = false;
  NormalizeManualGaugeGeometry(loaded);
  const bool valid =
      requireManualAcceptance
          ? ValidateManualGaugeGeometry(loaded, outReason)
          : ValidateManualGaugeGeometryForEditing(loaded, outReason);
  if (!valid)
    return false;
  context.current_gauge = loaded;
  outReason.clear();
  return true;
}

bool LoadManualGaugeAnnotationFromPath(ManualTestContext &context,
                                       const std::filesystem::path &source_path,
                                       std::string &outReason) {
  return LoadManualGaugeAnnotationFromPathImpl(context, source_path, true,
                                               outReason);
}

bool LoadManualGaugeWorkingCopyFromPath(
    ManualTestContext &context, const std::filesystem::path &source_path,
    std::string &outReason) {
  return LoadManualGaugeAnnotationFromPathImpl(context, source_path, false,
                                               outReason);
}

bool ExportManualGaugeManifestCandidate(ManualTestContext &context,
                                        const std::string &objectName,
                                        const std::string &gaugeName,
                                        std::string &outPath,
                                        std::string &outReason) {
  (void)objectName;
  (void)gaugeName;
  ManualGaugeState gauge = context.current_gauge;
  NormalizeManualGaugeGeometry(gauge);
  if (!ValidateManualGaugeGeometry(gauge, outReason))
    return false;
  std::filesystem::path dir;
  if (!ResolveManualGaugeCaseDir(context, dir, outReason))
    return false;
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  if (ec) {
    outReason = "cannot create gauge case directory: " + ec.message();
    return false;
  }
  const std::filesystem::path destination =
      dir / "gauge_manifest_candidate.cxsc";
  const std::filesystem::path temporary = destination.string() + ".tmp";
  std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
  if (!file.is_open()) {
    outReason = "cannot open temporary manifest candidate";
    return false;
  }
  file << "// candidate only; pending manual review; do not promote "
          "automatically\n"
       << "Stage25Manifest m;\n"
       << "m.reset();\n"
       << "m.setname(\"manual_gauge_candidate_"
       << GaugeJsonEscape(gauge.case_id) << "\");\n"
       << "m.addimage(\"" << GaugeJsonEscape(gauge.image_id)
       << "\", \"manual_candidate\", \""
       << GaugeJsonEscape(context.image_file_path) << "\");\n";
  if (gauge.tool == "FindLine")
    file << "m.image_addfindlinetarget(\"" << GaugeJsonEscape(gauge.target_id)
         << "\", " << gauge.line_x0 << ", " << gauge.line_y0 << ", "
         << gauge.line_x1 << ", " << gauge.line_y1 << ", " << gauge.wgap << ", "
         << gauge.hgap << ");\n";
  else if (gauge.tool == "FindCircle")
    file << "m.image_addfindcircletarget(\"" << GaugeJsonEscape(gauge.target_id)
         << "\", " << gauge.circle_cx << ", " << gauge.circle_cy << ", "
         << gauge.circle_px << ", " << gauge.circle_py << ", " << gauge.gap
         << ", " << gauge.linegap << ");\n";
  else if (gauge.tool == "FindEllipse") {
    const int cx = (gauge.ellipse_x0 + gauge.ellipse_x1) / 2;
    const int cy = (gauge.ellipse_y0 + gauge.ellipse_y1) / 2;
    const int rx = std::abs(gauge.ellipse_x1 - gauge.ellipse_x0) / 2;
    const int ry = std::abs(gauge.ellipse_y1 - gauge.ellipse_y0) / 2;
    file << "m.image_addfindellipsetarget(\""
         << GaugeJsonEscape(gauge.target_id) << "\", " << cx << ", " << cy
         << ", " << rx << ", " << ry << ", 0);\n";
  }
  file.flush();
  const bool write_ok = file.good();
  file.close();
  if (!write_ok) {
    outReason = "failed while writing manifest candidate";
    std::filesystem::remove(temporary, ec);
    return false;
  }
  if (!AtomicReplaceFile(temporary, destination, outReason)) {
    std::filesystem::remove(temporary, ec);
    return false;
  }
  outPath = destination.string();
  outReason.clear();
  return true;
}

bool ManualGaugeAcceptedForParamRegression(const ManualGaugeState &gauge) {
  std::string reason;
  return ValidateManualGaugeGeometry(gauge, reason);
}

bool ValidateParamRegressionPrerequisites(const ManualTestContext &context,
                                          std::string &reason) {
  if (!ValidateManualGaugeGeometry(context.current_gauge, reason))
    return false;
  const ManualGaugeState &gauge = context.current_gauge;
  if (gauge.case_id.empty() || gauge.image_id.empty() ||
      gauge.target_id.empty()) {
    reason = "case_id, image_id and target_id are required";
    return false;
  }
  if (context.image_file_path.empty() ||
      !std::filesystem::is_regular_file(context.image_file_path)) {
    reason = "image_file_path is unavailable";
    return false;
  }
  const std::string script_path = !context.loaded_script_path.empty()
                                      ? context.loaded_script_path
                                      : context.active_script_case_path;
  if (script_path.empty() || !std::filesystem::is_regular_file(script_path)) {
    reason = "script path is unavailable";
    return false;
  }
  std::filesystem::path case_dir;
  if (!ResolveManualGaugeCaseDir(context, case_dir, reason))
    return false;
  if (!std::filesystem::is_regular_file(case_dir / "gauge_annotation.json")) {
    reason = "gauge_annotation.json has not been saved";
    return false;
  }
  reason.clear();
  return true;
}
