#include "CircleShape.h"
#include "CxCrashLogHandler.h"
#include "CxRuntimePaths.h"
#include "CxFastMatchRuntimeCapture.h"
#include "CxUnifiedLog.h"

#include "FastMatch.h"
#include "FindCircle.h"
#include "FindEllipse.h"
#include "FindObject.h"
#include "LineGaugeShape.h"
#include "ManualConsoleGauge.h"
#include "ManualConsoleParamRegressionPanel.h"
#include "ManualConsoleUtils.h"
#include "ManualStateTestConsole.h"
#include "RegionPatternTool.h"
#include "ViewController.h"
#include <glad/gl.h>

#include "occtinclude.h"
#include <GLFW/glfw3.h>

#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <AIS_InteractiveContext.hxx>
#include <AIS_Manipulator.hxx>
#include <AIS_Shape.hxx>
#include <AIS_TexturedShape.hxx>
#include <Aspect_DisplayConnection.hxx>
#include <Aspect_Handle.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <Geom_RectangularTrimmedSurface.hxx>
#include <Image_AlienPixMap.hxx>
#include <Message.hxx>
#include <Message_Messenger.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <V3d_View.hxx>
#include <opencv2/opencv.hpp>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

#ifndef CXCORE_ENABLE_VIEWCONTROLLER_CUDA
#define CXCORE_ENABLE_VIEWCONTROLLER_CUDA 0
#endif
#if CXCORE_ENABLE_VIEWCONTROLLER_CUDA
#include "AI_kernels.h"
#include <cuda_runtime.h>

#include <cuda_gl_interop.h>

#include "GPU\imageDenoising.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <helper_cuda.h>
#include <helper_functions.h>
#endif

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <math.h>
#include <vector>

#include <string>

#include <opencv2/core/version.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/video/tracking.hpp>

#include "Sysctl.h"

#if CXCORE_ENABLE_VIEWCONTROLLER_CUDA
#include "CalibPolyModel.h"
#include "yolov8all_1_23.h"
#endif

namespace {
namespace fs = std::filesystem;

fs::path findRepositoryRoot() {
  const fs::path candidates[] = {fs::path(__FILE__).parent_path().parent_path(),
                                 fs::current_path()};
  for (const fs::path &candidate : candidates) {
    fs::path current = candidate;
    while (!current.empty()) {
      if (fs::exists(current / "cxparser" / "cxscript"))
        return current;
      const fs::path parent = current.parent_path();
      if (parent == current)
        break;
      current = parent;
    }
  }
  return fs::path();
}

bool pathIsWithin(const fs::path &child, const fs::path &parent) {
  const fs::path normalizedChild = fs::weakly_canonical(child);
  const fs::path normalizedParent = fs::weakly_canonical(parent);
  auto childIt = normalizedChild.begin();
  for (auto parentIt = normalizedParent.begin();
       parentIt != normalizedParent.end(); ++parentIt, ++childIt) {
    if (childIt == normalizedChild.end() || *childIt != *parentIt)
      return false;
  }
  return true;
}

const char *emptyAsNone(const std::string &value) {
  return value.empty() ? "(none)" : value.c_str();
}

void DrawCxImGuiFrameBackground() {
  ImGuiViewport *viewport = ImGui::GetMainViewport();
  if (viewport == nullptr)
    return;

  ImDrawList *background = ImGui::GetBackgroundDrawList();
  if (background == nullptr)
    return;

  const ImVec2 min = viewport->Pos;
  const ImVec2 max(viewport->Pos.x + viewport->Size.x,
                   viewport->Pos.y + viewport->Size.y);
  background->AddRectFilled(min, max, IM_COL32(55, 55, 55, 255));
}

bool evidencePathHasAny(const std::string &scriptPath,
                        std::initializer_list<const char *> tokens) {
  for (const char *token : tokens) {
    if (token != nullptr && scriptPath.find(token) != std::string::npos)
      return true;
  }
  return false;
}

bool evidencePathIsFindCircle(const std::string &scriptPath) {
  return evidencePathHasAny(scriptPath,
                            {"find_circle", "findcircle", "FindCircle"});
}

bool evidencePathIsFindLine(const std::string &scriptPath) {
  return evidencePathHasAny(scriptPath, {"find_line", "findline", "FindLine"});
}

bool evidencePathIsFindEllipse(const std::string &scriptPath) {
  return evidencePathHasAny(scriptPath,
                            {"find_ellipse", "findellipse", "FindEllipse"});
}

bool evidencePathIsFindRect(const std::string &scriptPath) {
  return evidencePathHasAny(scriptPath, {"find_rect", "findrect", "FindRect"});
}

bool evidencePathIsFastMatch(const std::string &scriptPath) {
  return evidencePathHasAny(scriptPath, {"fastmatch", "FastMatch"});
}

bool evidencePathIsGridPatternClass(const std::string &scriptPath) {
  return evidencePathHasAny(scriptPath,
                            {"grid_pattern_class", "GridPatternClassTool"});
}

bool evidencePathIsRegionPattern(const std::string &scriptPath) {
  return evidencePathHasAny(scriptPath,
                            {"region_pattern", "RegionPatternTool"});
}

bool evidencePathIsFindSegmentation(const std::string &scriptPath) {
  return evidencePathHasAny(
      scriptPath,
      {"find_segmentation", "findsegmentation", "FindSegmentation"});
}

bool evidencePathIsFindObject(const std::string &scriptPath) {
  return evidencePathHasAny(scriptPath,
                            {"find_object", "findobject", "FindObject"});
}

bool evidencePathIsTorchTask(const std::string &scriptPath) {
  return evidencePathHasAny(scriptPath,
                            {"torch", "TorchTask", "deeplab", "yolo"});
}

std::string inferEvidenceToolFromScriptPath(const std::string &scriptPath) {
  if (evidencePathIsGridPatternClass(scriptPath))
    return "GridPatternClassTool";
  if (evidencePathIsRegionPattern(scriptPath))
    return "RegionPatternTool";
  if (evidencePathIsFindCircle(scriptPath))
    return "FindCircle";
  if (evidencePathIsFindLine(scriptPath))
    return "FindLine";
  if (evidencePathIsFindEllipse(scriptPath))
    return "FindEllipse";
  if (evidencePathIsFindRect(scriptPath))
    return "FindRect";
  if (evidencePathIsFastMatch(scriptPath))
    return "FastMatch";
  if (evidencePathIsFindObject(scriptPath))
    return "FindObject";
  if (evidencePathIsFindSegmentation(scriptPath))
    return "FindSegmentation";
  if (evidencePathIsTorchTask(scriptPath))
    return "TorchTask";
  return "";
}

std::string evidenceLowerAscii(std::string value) {
  std::transform(
      value.begin(), value.end(), value.begin(),
      [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return value;
}

std::string evidenceCompactToolText(const std::string &value) {
  std::string compact;
  compact.reserve(value.size());
  for (unsigned char ch : value) {
    if (std::isalnum(ch))
      compact.push_back(static_cast<char>(std::tolower(ch)));
  }
  return compact;
}

bool evidenceKnownToolFilter(const std::string &filter,
                             std::string &canonicalTool) {
  const std::string compact = evidenceCompactToolText(filter);
  if (compact == "findline" || compact == "findcircle" ||
      compact == "findellipse" || compact == "findrect" ||
      compact == "fastmatch" || compact == "findsegmentation" ||
      compact == "findobject" || compact == "torchtask") {
    canonicalTool = compact;
    return true;
  }
  canonicalTool.clear();
  return false;
}

bool evidenceExplicitToolMatches(const std::string &toolText,
                                 const std::string &canonicalTool) {
  if (canonicalTool.empty())
    return false;
  return evidenceCompactToolText(toolText).find(canonicalTool) !=
         std::string::npos;
}

void evidenceResolveGeometryToolFlags(const std::string &toolText,
                                      const std::string &scriptPath,
                                      bool &isFindLine, bool &isFindCircle,
                                      bool &isFindEllipse) {
  const bool hasExplicitTool = !evidenceCompactToolText(toolText).empty();
  isFindLine = evidenceExplicitToolMatches(toolText, "findline") ||
               (!hasExplicitTool && evidencePathIsFindLine(scriptPath));
  isFindCircle = evidenceExplicitToolMatches(toolText, "findcircle") ||
                 (!hasExplicitTool && evidencePathIsFindCircle(scriptPath));
  isFindEllipse = evidenceExplicitToolMatches(toolText, "findellipse") ||
                  (!hasExplicitTool && evidencePathIsFindEllipse(scriptPath));
}

bool scriptContainsIdentifier(const std::string &scriptText,
                              const std::string &identifier) {
  std::size_t pos = scriptText.find(identifier);
  while (pos != std::string::npos) {
    const bool leftOk =
        pos == 0 ||
        !(std::isalnum(static_cast<unsigned char>(scriptText[pos - 1])) ||
          scriptText[pos - 1] == '_');
    const std::size_t end = pos + identifier.size();
    const bool rightOk =
        end >= scriptText.size() ||
        !(std::isalnum(static_cast<unsigned char>(scriptText[end])) ||
          scriptText[end] == '_');
    if (leftOk && rightOk)
      return true;
    pos = scriptText.find(identifier, end);
  }
  return false;
}

bool stageEvidenceSelfTestScriptGlobals(
    ParserDebugBridge &bridge,
    std::unordered_map<std::string, int> &runtimeIntVars,
    const std::string &scriptText, const std::string &parameterSummary,
    int imageWidth, int imageHeight, bool allowFallbackDefaults,
    std::string &reason) {
  reason.clear();
  if (scriptText.empty()) {
    reason = "script text is empty";
    return false;
  }

  auto existingOr = [&](const char *key, int fallback) -> int {
    const auto it = runtimeIntVars.find(key);
    return it == runtimeIntVars.end() ? fallback : it->second;
  };

  auto setInt = [&](const char *key, int value) {
    runtimeIntVars[key] = value;
    bridge.SetGlobalInt(key, value);
  };

  std::vector<std::string> missingLockedGlobals;

  auto setIntIfUsed = [&](const char *key, int value) -> bool {
    if (!scriptContainsIdentifier(scriptText, key))
      return false;
    setInt(key, value);
    return true;
  };

  auto setRequiredIntIfUsed = [&](const char *key, int fallback) -> bool {
    if (!scriptContainsIdentifier(scriptText, key))
      return false;
    const auto it = runtimeIntVars.find(key);
    if (it == runtimeIntVars.end() && !allowFallbackDefaults) {
      missingLockedGlobals.push_back(key);
      return false;
    }
    setInt(key, it == runtimeIntVars.end() ? fallback : it->second);
    return true;
  };

  auto parseDoubleToken = [&](const char *key, double &value) -> bool {
    const std::string pattern = std::string(key) + "=";
    const std::size_t pos = parameterSummary.find(pattern);
    if (pos == std::string::npos)
      return false;
    std::size_t begin = pos + pattern.size();
    std::size_t end = begin;
    while (end < parameterSummary.size() &&
           (std::isdigit(static_cast<unsigned char>(parameterSummary[end])) ||
            parameterSummary[end] == '-' || parameterSummary[end] == '+' ||
            parameterSummary[end] == '.')) {
      ++end;
    }
    if (end == begin)
      return false;
    try {
      value = std::stod(parameterSummary.substr(begin, end - begin));
    } catch (...) {
      return false;
    }
    return std::isfinite(value);
  };

  auto setDoubleIfUsed = [&](const char *key, double value) -> bool {
    if (!scriptContainsIdentifier(scriptText, key))
      return false;
    bridge.SetGlobalDouble(key, value);
    return true;
  };

  auto setRequiredDoubleIfUsed = [&](const char *key, const char *summaryKey,
                                     double fallback) -> bool {
    if (!scriptContainsIdentifier(scriptText, key))
      return false;
    double value = fallback;
    if (!parseDoubleToken(summaryKey, value) && !allowFallbackDefaults) {
      missingLockedGlobals.push_back(key);
      return false;
    }
    bridge.SetGlobalDouble(key, value);
    return true;
  };

  auto parseStringToken = [&](const char *key, std::string &value) -> bool {
    const std::string pattern = std::string(key) + "=";
    const std::size_t pos = parameterSummary.find(pattern);
    if (pos == std::string::npos)
      return false;
    std::size_t begin = pos + pattern.size();
    if (begin >= parameterSummary.size())
      return false;
    if (parameterSummary[begin] == '"') {
      ++begin;
      std::size_t end = begin;
      while (end < parameterSummary.size() && parameterSummary[end] != '"')
        ++end;
      if (end == begin)
        return false;
      value = parameterSummary.substr(begin, end - begin);
      return true;
    }
    std::size_t end = begin;
    while (end < parameterSummary.size() &&
           !std::isspace(static_cast<unsigned char>(parameterSummary[end]))) {
      ++end;
    }
    if (end == begin)
      return false;
    value = parameterSummary.substr(begin, end - begin);
    return true;
  };

  auto setRequiredStringIfUsed = [&](const char *key,
                                     const char *summaryKey) -> bool {
    if (!scriptContainsIdentifier(scriptText, key))
      return false;
    std::string value;
    if (!parseStringToken(summaryKey, value)) {
      missingLockedGlobals.push_back(key);
      return false;
    }
    bridge.SetGlobalString(key, value);
    return true;
  };

  const int safeWidth = std::max(1, imageWidth);
  const int safeHeight = std::max(1, imageHeight);
  const int roiX0 = existingOr("global_roi_x0", std::max(0, safeWidth / 4));
  const int roiY0 = existingOr("global_roi_y0", std::max(0, safeHeight / 4));
  const int roiX1 =
      existingOr("global_roi_x1", std::max(roiX0 + 8, (safeWidth * 3) / 4));
  const int roiY1 =
      existingOr("global_roi_y1", std::max(roiY0 + 8, (safeHeight * 3) / 4));
  const int roiW = std::max(8, roiX1 - roiX0);
  const int roiH = std::max(8, roiY1 - roiY0);

  // Older evidence packages predate these script globals. Seed stable defaults
  // so historical cases remain reviewable without rewriting archived assets.
  if (runtimeIntVars.find("global_min_edge_run_width_px") ==
      runtimeIntVars.end())
    runtimeIntVars["global_min_edge_run_width_px"] = 3;
  if (runtimeIntVars.find("global_findcircle_findsetting") ==
      runtimeIntVars.end())
    runtimeIntVars["global_findcircle_findsetting"] = 0;

  int applied = 0;

  applied += setRequiredIntIfUsed("global_roi_x0", roiX0) ? 1 : 0;
  applied += setRequiredIntIfUsed("global_roi_y0", roiY0) ? 1 : 0;
  applied += setRequiredIntIfUsed("global_roi_x1", roiX1) ? 1 : 0;
  applied += setRequiredIntIfUsed("global_roi_y1", roiY1) ? 1 : 0;
  applied +=
      setRequiredIntIfUsed("global_roi_x", existingOr("global_roi_x", roiX0))
          ? 1
          : 0;
  applied +=
      setRequiredIntIfUsed("global_roi_y", existingOr("global_roi_y", roiY0))
          ? 1
          : 0;
  applied += setRequiredIntIfUsed("global_roi_width",
                                  existingOr("global_roi_width", roiW))
                 ? 1
                 : 0;
  applied += setRequiredIntIfUsed("global_roi_height",
                                  existingOr("global_roi_height", roiH))
                 ? 1
                 : 0;

  applied += setRequiredIntIfUsed("global_learn_roi_x",
                                  existingOr("global_learn_roi_x", roiX0))
                 ? 1
                 : 0;
  applied += setRequiredIntIfUsed("global_learn_roi_y",
                                  existingOr("global_learn_roi_y", roiY0))
                 ? 1
                 : 0;
  applied += setRequiredIntIfUsed("global_learn_roi_w",
                                  existingOr("global_learn_roi_w", roiW))
                 ? 1
                 : 0;
  applied += setRequiredIntIfUsed("global_learn_roi_h",
                                  existingOr("global_learn_roi_h", roiH))
                 ? 1
                 : 0;
  applied += setRequiredIntIfUsed("global_search_roi_x",
                                  existingOr("global_search_roi_x", 0))
                 ? 1
                 : 0;
  applied += setRequiredIntIfUsed("global_search_roi_y",
                                  existingOr("global_search_roi_y", 0))
                 ? 1
                 : 0;
  applied += setRequiredIntIfUsed("global_search_roi_w",
                                  existingOr("global_search_roi_w", safeWidth))
                 ? 1
                 : 0;
  applied += setRequiredIntIfUsed("global_search_roi_h",
                                  existingOr("global_search_roi_h", safeHeight))
                 ? 1
                 : 0;

  applied += setRequiredIntIfUsed("global_circle_cx",
                                  existingOr("global_circle_cx", safeWidth / 2))
                 ? 1
                 : 0;
  applied +=
      setRequiredIntIfUsed("global_circle_cy",
                           existingOr("global_circle_cy", safeHeight / 2))
          ? 1
          : 0;
  applied +=
      setRequiredIntIfUsed("global_circle_px",
                           existingOr("global_circle_px", (safeWidth * 3) / 4))
          ? 1
          : 0;
  applied +=
      setRequiredIntIfUsed("global_circle_py",
                           existingOr("global_circle_py", safeHeight / 2))
          ? 1
          : 0;
  applied += setRequiredIntIfUsed("global_circle_inner_radius",
                                  existingOr("global_circle_inner_radius", 0))
                 ? 1
                 : 0;
  applied += setRequiredIntIfUsed("global_circle_outer_radius",
                                  existingOr("global_circle_outer_radius", 0))
                 ? 1
                 : 0;
  applied += setRequiredIntIfUsed("global_circle_ring_width",
                                  existingOr("global_circle_ring_width", 0))
                 ? 1
                 : 0;
  applied += setRequiredIntIfUsed("global_ellipse_x0",
                                  existingOr("global_ellipse_x0", roiX0))
                 ? 1
                 : 0;
  applied += setRequiredIntIfUsed("global_ellipse_y0",
                                  existingOr("global_ellipse_y0", roiY0))
                 ? 1
                 : 0;
  applied += setRequiredIntIfUsed("global_ellipse_x1",
                                  existingOr("global_ellipse_x1", roiX1))
                 ? 1
                 : 0;
  applied += setRequiredIntIfUsed("global_ellipse_y1",
                                  existingOr("global_ellipse_y1", roiY1))
                 ? 1
                 : 0;
  applied += setRequiredIntIfUsed(
                 "global_findellipse_inner_scale_percent",
                 existingOr("global_findellipse_inner_scale_percent", 0))
                 ? 1
                 : 0;

  applied +=
      setRequiredIntIfUsed("global_method", existingOr("global_method", 0)) ? 1
                                                                            : 0;
  applied += setRequiredIntIfUsed("global_threshold",
                                  existingOr("global_threshold", 20))
                 ? 1
                 : 0;
  applied +=
      setRequiredIntIfUsed("global_gap", existingOr("global_gap", 5)) ? 1 : 0;
  applied +=
      setRequiredIntIfUsed("global_linegap", existingOr("global_linegap", 3))
          ? 1
          : 0;
  applied += setRequiredIntIfUsed(
                 "global_min_edge_run_width_px",
                 existingOr("global_min_edge_run_width_px", 3))
                 ? 1
                 : 0;
  applied += setRequiredIntIfUsed("global_wgap", existingOr("global_wgap", 32))
                 ? 1
                 : 0;
  applied +=
      setRequiredIntIfUsed("global_hgap", existingOr("global_hgap", 8)) ? 1 : 0;
  applied += setRequiredIntIfUsed("global_tool_half_width",
                                  existingOr("global_tool_half_width", 32))
                 ? 1
                 : 0;
  applied += setRequiredIntIfUsed("global_filterprofile",
                                  existingOr("global_filterprofile", 1))
                 ? 1
                 : 0;
  applied +=
      setRequiredIntIfUsed("global_gauge", existingOr("global_gauge", 20)) ? 1
                                                                           : 0;
  applied +=
      setRequiredIntIfUsed("global_find_num", existingOr("global_find_num", 5))
          ? 1
          : 0;
  applied += setRequiredIntIfUsed("global_compare_gap",
                                  existingOr("global_compare_gap", 20))
                 ? 1
                 : 0;
  applied += setRequiredIntIfUsed("global_objfilter",
                                  existingOr("global_objfilter", 0))
                 ? 1
                 : 0;
  applied += setRequiredIntIfUsed("global_findline_objfilter",
                                  existingOr("global_findline_objfilter", 1))
                 ? 1
                 : 0;
  applied += setRequiredIntIfUsed(
                 "global_findline_findsetting",
                 existingOr("global_findline_findsetting",
                            existingOr("global_findline_objfilter", 1)))
                 ? 1
                 : 0;
  applied +=
      setRequiredIntIfUsed("global_findcircle_findsetting",
                           existingOr("global_findcircle_findsetting", 0))
          ? 1
          : 0;
  applied +=
      setRequiredIntIfUsed("global_findellipse_findsetting",
                           existingOr("global_findellipse_findsetting", 1))
          ? 1
          : 0;
  applied += setRequiredIntIfUsed("global_findrect_findsetting",
                                  existingOr("global_findrect_findsetting", 1))
                 ? 1
                 : 0;
  applied += setRequiredIntIfUsed("global_max_elapsed_ms",
                                  existingOr("global_max_elapsed_ms", 2000))
                 ? 1
                 : 0;
  applied += setRequiredIntIfUsed("global_max_scan_lines",
                                  existingOr("global_max_scan_lines", 256))
                 ? 1
                 : 0;
  applied += setRequiredIntIfUsed("global_max_samples",
                                  existingOr("global_max_samples", 4096))
                 ? 1
                 : 0;
  applied +=
      setRequiredIntIfUsed("global_grid_normalized_width",
                           existingOr("global_grid_normalized_width", 48))
          ? 1
          : 0;
  applied +=
      setRequiredIntIfUsed("global_grid_normalized_height",
                           existingOr("global_grid_normalized_height", 48))
          ? 1
          : 0;
  applied += setRequiredIntIfUsed("global_grid_rows",
                                  existingOr("global_grid_rows", 12))
                 ? 1
                 : 0;
  applied += setRequiredIntIfUsed("global_grid_cols",
                                  existingOr("global_grid_cols", 12))
                 ? 1
                 : 0;
  applied += setRequiredIntIfUsed("global_grid_levels",
                                  existingOr("global_grid_levels", 3))
                 ? 1
                 : 0;
  applied += setRequiredIntIfUsed("global_grid_orientation_bins",
                                  existingOr("global_grid_orientation_bins", 8))
                 ? 1
                 : 0;
  applied +=
      setRequiredIntIfUsed("global_grid_foreground_threshold",
                           existingOr("global_grid_foreground_threshold", -1))
          ? 1
          : 0;
  applied += setRequiredIntIfUsed("global_grid_foreground_dark",
                                  existingOr("global_grid_foreground_dark", 1))
                 ? 1
                 : 0;
  applied +=
      setRequiredIntIfUsed("global_grid_equalize_contrast",
                           existingOr("global_grid_equalize_contrast", 0))
          ? 1
          : 0;
  applied += setRequiredIntIfUsed(
                 "global_grid_active_foreground_percent",
                 existingOr("global_grid_active_foreground_percent", 5))
                 ? 1
                 : 0;
  applied +=
      setRequiredIntIfUsed("global_grid_active_edge_percent",
                           existingOr("global_grid_active_edge_percent", 3))
          ? 1
          : 0;
  applied += setRequiredIntIfUsed("global_grid_max_overlays",
                                  existingOr("global_grid_max_overlays", 96))
                 ? 1
                 : 0;
  applied += setRequiredIntIfUsed("global_grid_fusion_mode",
                                  existingOr("global_grid_fusion_mode", 2))
                 ? 1
                 : 0;
  applied += setRequiredIntIfUsed("global_region_roi_x",
                                  existingOr("global_region_roi_x", roiX0))
                 ? 1
                 : 0;
  applied += setRequiredIntIfUsed("global_region_roi_y",
                                  existingOr("global_region_roi_y", roiY0))
                 ? 1
                 : 0;
  applied += setRequiredIntIfUsed("global_region_roi_w",
                                  existingOr("global_region_roi_w", roiW))
                 ? 1
                 : 0;
  applied += setRequiredIntIfUsed("global_region_roi_h",
                                  existingOr("global_region_roi_h", roiH))
                 ? 1
                 : 0;
  applied +=
      setRequiredIntIfUsed("global_region_normalized_width",
                           existingOr("global_region_normalized_width", 32))
          ? 1
          : 0;
  applied +=
      setRequiredIntIfUsed("global_region_normalized_height",
                           existingOr("global_region_normalized_height", 32))
          ? 1
          : 0;
  applied += setRequiredIntIfUsed("global_region_pooling_rows",
                                  existingOr("global_region_pooling_rows", 4))
                 ? 1
                 : 0;
  applied += setRequiredIntIfUsed("global_region_pooling_cols",
                                  existingOr("global_region_pooling_cols", 4))
                 ? 1
                 : 0;
  applied += setRequiredIntIfUsed("global_region_use_binary",
                                  existingOr("global_region_use_binary", 0))
                 ? 1
                 : 0;
  applied += setRequiredIntIfUsed("global_region_threshold",
                                  existingOr("global_region_threshold", 128))
                 ? 1
                 : 0;
  applied +=
      setRequiredIntIfUsed("global_region_foreground_dark",
                           existingOr("global_region_foreground_dark", 1))
          ? 1
          : 0;
  applied += setRequiredIntIfUsed("global_region_max_overlays",
                                  existingOr("global_region_max_overlays", 64))
                 ? 1
                 : 0;

  /*
   * Evidence self-test may replay older script snapshots before a candidate
   * has saved edge-role globals.  These are execution controls, not locked
   * evidence parameters: missing selected_edge must mean "All edges" instead
   * of rejecting every scan run.
   */
  applied += setIntIfUsed("global_findline_selected_edge",
                          existingOr("global_findline_selected_edge", 0))
                 ? 1
                 : 0;
  applied += setIntIfUsed("global_findline_edge_count",
                          existingOr("global_findline_edge_count", 1))
                 ? 1
                 : 0;
  applied += setIntIfUsed("global_findline_scan_direction",
                          existingOr("global_findline_scan_direction", 2))
                 ? 1
                 : 0;
  applied +=
      setIntIfUsed("global_findline_point_consistency_enabled",
                   existingOr("global_findline_point_consistency_enabled", 0))
          ? 1
          : 0;
  applied +=
      setIntIfUsed("global_findline_point_consistency_range",
                   existingOr("global_findline_point_consistency_range", 0))
          ? 1
          : 0;
  applied += setIntIfUsed("global_findline_best_edge",
                          existingOr("global_findline_best_edge", 0))
                 ? 1
                 : 0;
  applied += setIntIfUsed("global_findline_recommended_edge",
                          existingOr("global_findline_recommended_edge", 0))
                 ? 1
                 : 0;
  applied += setIntIfUsed("global_findline_relation_edge",
                          existingOr("global_findline_relation_edge", 0))
                 ? 1
                 : 0;
  applied += setIntIfUsed("global_findline_attach_edge",
                          existingOr("global_findline_attach_edge", 0))
                 ? 1
                 : 0;
  applied += setIntIfUsed("global_findcircle_selected_edge",
                          existingOr("global_findcircle_selected_edge", 0))
                 ? 1
                 : 0;
  applied += setIntIfUsed("global_findcircle_edge_count",
                          existingOr("global_findcircle_edge_count", 1))
                 ? 1
                 : 0;
  applied += setIntIfUsed("global_findcircle_best_edge",
                          existingOr("global_findcircle_best_edge", 0))
                 ? 1
                 : 0;
  applied += setIntIfUsed("global_findcircle_recommended_edge",
                          existingOr("global_findcircle_recommended_edge", 0))
                 ? 1
                 : 0;
  applied += setIntIfUsed("global_findcircle_relation_edge",
                          existingOr("global_findcircle_relation_edge", 0))
                 ? 1
                 : 0;
  applied += setIntIfUsed("global_findcircle_attach_edge",
                          existingOr("global_findcircle_attach_edge", 0))
                 ? 1
                 : 0;
  applied += setIntIfUsed("global_findcircle_arc_enabled",
                          existingOr("global_findcircle_arc_enabled", 0))
                 ? 1
                 : 0;
  applied += setIntIfUsed("global_findcircle_arc_start_deg",
                          existingOr("global_findcircle_arc_start_deg", 0))
                 ? 1
                 : 0;
  applied += setIntIfUsed("global_findcircle_arc_end_deg",
                          existingOr("global_findcircle_arc_end_deg", 360))
                 ? 1
                 : 0;
  applied +=
      setIntIfUsed("global_findcircle_point_consistency_enabled",
                   existingOr("global_findcircle_point_consistency_enabled", 0))
          ? 1
          : 0;
  applied +=
      setIntIfUsed("global_findcircle_point_consistency_range",
                   existingOr("global_findcircle_point_consistency_range", 0))
          ? 1
          : 0;
  const int findEllipseEdgeCount =
      std::max(1, std::min(3, existingOr("global_findellipse_edge_count", 3)));
  applied +=
      setIntIfUsed(
          "global_findellipse_selected_edge",
          std::max(-1,
                   std::min(findEllipseEdgeCount,
                            existingOr("global_findellipse_selected_edge", 0))))
          ? 1
          : 0;
  applied += setIntIfUsed("global_findellipse_edge_count", findEllipseEdgeCount)
                 ? 1
                 : 0;
  applied +=
      setIntIfUsed(
          "global_findellipse_best_edge",
          std::max(0, std::min(findEllipseEdgeCount,
                               existingOr("global_findellipse_best_edge", 0))))
          ? 1
          : 0;
  applied +=
      setIntIfUsed(
          "global_findellipse_recommended_edge",
          std::max(0, std::min(findEllipseEdgeCount,
                               existingOr("global_findellipse_recommended_edge",
                                          0))))
          ? 1
          : 0;
  applied +=
      setIntIfUsed(
          "global_findellipse_relation_edge",
          std::max(0,
                   std::min(findEllipseEdgeCount,
                            existingOr("global_findellipse_relation_edge", 0))))
          ? 1
          : 0;
  applied +=
      setIntIfUsed(
          "global_findellipse_attach_edge",
          std::max(0,
                   std::min(findEllipseEdgeCount,
                            existingOr("global_findellipse_attach_edge", 0))))
          ? 1
          : 0;
  applied += setIntIfUsed(
                 "global_findellipse_point_consistency_enabled",
                 existingOr("global_findellipse_point_consistency_enabled", 0))
                 ? 1
                 : 0;
  applied +=
      setIntIfUsed("global_findellipse_point_consistency_range",
                   existingOr("global_findellipse_point_consistency_range", 0))
          ? 1
          : 0;

  /*
   * These globals are result echo/output variables used by older frozen,
   * suite snapshot, and contract scripts.  They must exist before script
   * execution, but their initial zero value must not be interpreted as a
   * runtime result.  Runtime capture remains the fact source after execution.
   */
  applied += setIntIfUsed("global_algorithm_executed",
                          existingOr("global_algorithm_executed", 1))
                 ? 1
                 : 0;
  applied += setIntIfUsed("global_selected_method", 0) ? 1 : 0;
  applied += setIntIfUsed("global_selected_threshold", 0) ? 1 : 0;
  applied += setIntIfUsed("global_selected_wgap", 0) ? 1 : 0;
  applied += setIntIfUsed("global_selected_hgap", 0) ? 1 : 0;
  applied += setIntIfUsed("global_selected_linegap", 0) ? 1 : 0;
  applied += setIntIfUsed("global_selected_filterprofile", 0) ? 1 : 0;
  applied += setIntIfUsed("global_actual_findsetting", 0) ? 1 : 0;
  applied += setIntIfUsed("global_findcircle_actual_findsetting", 0) ? 1 : 0;
  applied += setIntIfUsed("global_findcircle_actual_selected_edge", 0) ? 1 : 0;
  applied += setIntIfUsed("global_findcircle_selected_gap", 0) ? 1 : 0;
  applied += setIntIfUsed("global_result_valid", 0) ? 1 : 0;
  applied += setIntIfUsed("global_valid_points_count", 0) ? 1 : 0;
  applied += setIntIfUsed("global_has_fit_line", 0) ? 1 : 0;
  applied += setIntIfUsed("global_has_fit_circle", 0) ? 1 : 0;
  applied += setIntIfUsed("global_has_fit_ellipse", 0) ? 1 : 0;
  applied += setIntIfUsed("global_runtime_valid_points_count", 0) ? 1 : 0;
  applied += setIntIfUsed("global_runtime_has_fit_line", 0) ? 1 : 0;
  applied += setIntIfUsed("global_runtime_has_fit_circle", 0) ? 1 : 0;
  applied +=
      setIntIfUsed("global_runtime_global_valid_points_count_mismatch", 0) ? 1
                                                                           : 0;
  applied +=
      setIntIfUsed("global_runtime_global_has_fit_line_mismatch", 0) ? 1 : 0;
  applied +=
      setIntIfUsed("global_runtime_global_has_fit_circle_mismatch", 0) ? 1 : 0;
  applied += setIntIfUsed("global_runtime_global_result_mismatch", 0) ? 1 : 0;
  applied += setIntIfUsed("global_grid_status", 0) ? 1 : 0;
  applied += setIntIfUsed("global_grid_active_cell_count", 0) ? 1 : 0;
  applied += setIntIfUsed("global_grid_descriptor_dim", 0) ? 1 : 0;
  applied += setIntIfUsed("global_grid_level_count", 0) ? 1 : 0;
  applied += setIntIfUsed("global_grid_overlay_count", 0) ? 1 : 0;
  applied += setIntIfUsed("global_grid_overlay_truncated", 0) ? 1 : 0;
  applied += setIntIfUsed("global_region_status", 0) ? 1 : 0;
  applied += setIntIfUsed("global_region_descriptor_dim", 0) ? 1 : 0;
  applied += setIntIfUsed("global_region_foreground_permille", 0) ? 1 : 0;
  applied += setIntIfUsed("global_region_mean_permille", 0) ? 1 : 0;
  applied += setIntIfUsed("global_region_std_permille", 0) ? 1 : 0;
  applied += setIntIfUsed("global_region_pooling_rows_out", 0) ? 1 : 0;
  applied += setIntIfUsed("global_region_pooling_cols_out", 0) ? 1 : 0;
  applied += setIntIfUsed("global_region_overlay_count", 0) ? 1 : 0;
  applied += setIntIfUsed("global_region_overlay_truncated", 0) ? 1 : 0;
  applied += setDoubleIfUsed("global_avgdist", 0.0) ? 1 : 0;
  applied += setDoubleIfUsed("global_circle_radius", 0.0) ? 1 : 0;
  applied += setDoubleIfUsed("global_circle_effective_annulus", 0.0) ? 1 : 0;
  applied += setDoubleIfUsed("global_circle_effective_inner_radius", 0.0) ? 1
                                                                          : 0;
  applied += setDoubleIfUsed("global_circle_effective_outer_radius", 0.0) ? 1
                                                                          : 0;
  applied += setDoubleIfUsed("global_circle_effective_ring_width", 0.0) ? 1
                                                                        : 0;
  applied += setDoubleIfUsed("global_local_support", 1.0) ? 1 : 0;

  applied +=
      setRequiredDoubleIfUsed("global_min_score", "min_score", 0.5) ? 1 : 0;
  applied += setDoubleIfUsed("global_match_count", 0.0) ? 1 : 0;
  applied += setDoubleIfUsed("global_best_score", 0.0) ? 1 : 0;
  applied += setDoubleIfUsed("global_model_point_count", 0.0) ? 1 : 0;
  applied += setDoubleIfUsed("global_learn_a_count", 0.0) ? 1 : 0;
  applied += setDoubleIfUsed("global_learn_b_count", 0.0) ? 1 : 0;
  applied += setDoubleIfUsed("global_learn_a2_count", 0.0) ? 1 : 0;
  applied += setDoubleIfUsed("global_learn_b2_count", 0.0) ? 1 : 0;
  applied += setDoubleIfUsed("global_learn_status_code", 0.0) ? 1 : 0;
  applied += setDoubleIfUsed("global_circle_ref", 0.0) ? 1 : 0;
  applied += setDoubleIfUsed("global_line_ref", 0.0) ? 1 : 0;
  applied += setDoubleIfUsed("global_ellipse_ref", 0.0) ? 1 : 0;
  applied += setDoubleIfUsed("global_rect_ref", 0.0) ? 1 : 0;
  applied += setDoubleIfUsed("global_match_ref", 0.0) ? 1 : 0;
  applied += setDoubleIfUsed("global_segmentation_ref", 0.0) ? 1 : 0;
  applied +=
      setRequiredStringIfUsed("global_mlpack_model_path", "model_path") ? 1 : 0;

  if (!missingLockedGlobals.empty()) {
    reason = "evidence locked parameter summary is missing required globals:";
    for (const auto &key : missingLockedGlobals)
      reason += " " + key;
    return false;
  }

  reason = std::string(allowFallbackDefaults ? "fallback_default "
                                             : "evidence_locked ") +
           "staged " + std::to_string(applied) +
           " script global bindings for evidence selftest";
  return true;
}

void syncEvidenceLockedGlobalsToManualGauge(ManualTestContext &context,
                                            const std::string &scriptPath,
                                            const std::string &toolText,
                                            const std::string &source) {
  auto getInt = [&](const std::string &key, int fallback) -> int {
    const auto it = context.runtime_int_vars.find(key);
    return it == context.runtime_int_vars.end() ? fallback : it->second;
  };

  bool isLineScript = false;
  bool isCircleScript = false;
  bool isEllipseScript = false;
  evidenceResolveGeometryToolFlags(toolText, scriptPath, isLineScript,
                                   isCircleScript, isEllipseScript);
  const bool isFindSegmentationScript =
      evidencePathIsFindSegmentation(scriptPath);
  const bool isFindObjectScript = evidencePathIsFindObject(scriptPath);

  ManualGaugeState gauge;
  gauge.case_id = context.active_case_id;
  gauge.image_id = context.active_image_id;
  gauge.target_id = context.active_target_id;
  gauge.source = source;
  gauge.review_status = "editing";
  gauge.threshold = getInt("global_threshold", 20);
  gauge.method = getInt("global_method", 0);
  gauge.linegap = getInt("global_linegap", 3);
  gauge.min_edge_run_width_px = getInt("global_min_edge_run_width_px", 3);
  gauge.wgap = getInt("global_wgap", 32);
  gauge.hgap = getInt("global_hgap", 8);
  gauge.gap = getInt("global_gap", 5);
  gauge.tool_half_width = getInt("global_tool_half_width", 32);
  gauge.filterprofile = getInt("global_filterprofile", 1);

  if (isCircleScript) {
    gauge.tool = "FindCircle";
    gauge.has_circle_gauge = true;
    gauge.circle_cx = getInt("global_circle_cx", 0);
    gauge.circle_cy = getInt("global_circle_cy", 0);
    gauge.circle_px = getInt("global_circle_px", gauge.circle_cx);
    gauge.circle_py = getInt("global_circle_py", gauge.circle_cy);
    gauge.radius = static_cast<int>(std::lround(
        std::hypot(static_cast<double>(gauge.circle_px - gauge.circle_cx),
                   static_cast<double>(gauge.circle_py - gauge.circle_cy))));
  } else if (isEllipseScript) {
    gauge.tool = "FindEllipse";
    gauge.has_ellipse_gauge = true;
    gauge.ellipse_x0 = getInt("global_ellipse_x0", 0);
    gauge.ellipse_y0 = getInt("global_ellipse_y0", 0);
    gauge.ellipse_x1 = getInt("global_ellipse_x1", 0);
    gauge.ellipse_y1 = getInt("global_ellipse_y1", 0);
    gauge.ellipse_inner_scale_percent =
        getInt("global_findellipse_inner_scale_percent", 0);
  } else if (isLineScript) {
    gauge.tool = "FindLine";
    gauge.has_line_gauge = true;
    gauge.line_x0 = getInt("global_roi_x0", 0);
    gauge.line_y0 = getInt("global_roi_y0", 0);
    gauge.line_x1 = getInt("global_roi_x1", 0);
    gauge.line_y1 = getInt("global_roi_y1", 0);
  } else if (isFindSegmentationScript) {
    gauge.tool = "FindSegmentation";
    gauge.has_segmentation_prompt_rect = true;
    gauge.segmentation_prompt_x0 = getInt("global_roi_x0", 120);
    gauge.segmentation_prompt_y0 = getInt("global_roi_y0", 120);
    gauge.segmentation_prompt_x1 = getInt("global_roi_x1", 980);
    gauge.segmentation_prompt_y1 = getInt("global_roi_y1", 820);
    gauge.segmentation_mode = getInt("global_segmentation_mode", 2);
  } else if (isFindObjectScript) {
    gauge.tool = "FindObject";
    gauge.has_findobject_roi = true;
    gauge.findobject_x0 = getInt("global_roi_x0", 80);
    gauge.findobject_y0 = getInt("global_roi_y0", 80);
    gauge.findobject_x1 = getInt("global_roi_x1", 1180);
    gauge.findobject_y1 = getInt("global_roi_y1", 880);
    gauge.findobject_foreground_mode = getInt("global_method", 1);
    gauge.findobject_threshold = getInt("global_threshold", 20);
    gauge.findobject_min_area = getInt("global_object_min_area", 10);
  }

  if (gauge.has_circle_gauge || gauge.has_line_gauge ||
      gauge.has_ellipse_gauge || gauge.has_segmentation_prompt_rect ||
      gauge.has_findobject_roi) {
    context.current_gauge = gauge;
  }
}

bool evidenceSnapshotHasLockedParamSummary(
    const CxEvidenceSelectionSnapshot &snapshot, std::string &reason) {
  if (!snapshot.valid) {
    reason = "invalid evidence snapshot";
    return false;
  }
  if (snapshot.parameter_summary.empty() || snapshot.parameter_summary == "-") {
    reason = "evidence parameter summary is empty";
    return false;
  }
  if (snapshot.parameter_summary.find('=') == std::string::npos) {
    reason = "evidence parameter summary is not key=value locked data: " +
             snapshot.parameter_summary;
    return false;
  }
  reason.clear();
  return true;
}

std::string jsonEscapeEvidenceReview(const std::string &value) {
  std::ostringstream oss;
  for (char ch : value) {
    switch (ch) {
    case '\\':
      oss << "\\\\";
      break;
    case '"':
      oss << "\\\"";
      break;
    case '\n':
      oss << "\\n";
      break;
    case '\r':
      oss << "\\r";
      break;
    case '\t':
      oss << "\\t";
      break;
    default:
      oss << ch;
      break;
    }
  }
  return oss.str();
}

bool saveLockedEvidenceReview(ManualTestContext &context, std::string &outPath,
                              std::string &reason) {
  outPath.clear();
  reason.clear();

  const CxEvidenceSelectionSnapshot &snapshot =
      context.current_evidence_selection;
  if (!snapshot.valid) {
    reason = "cannot save evidence review: no selected evidence row";
    return false;
  }
  if (snapshot.case_id.empty() || snapshot.script_path.empty() ||
      snapshot.image_path.empty() || snapshot.target_id.empty()) {
    reason = "cannot save evidence review: evidence case/script/image/target "
             "is incomplete";
    return false;
  }

  std::string paramReason;
  if (!evidenceSnapshotHasLockedParamSummary(snapshot, paramReason)) {
    reason = "cannot save evidence review: " + paramReason;
    return false;
  }

  if (context.image_file_path != snapshot.image_path) {
    reason = "cannot save evidence review: current image differs from locked "
             "evidence image";
    return false;
  }

  if (context.current_gauge.review_status != "manual_accepted" ||
      !context.current_gauge.accepted) {
    reason = "cannot save evidence review: gauge is not manual_accepted";
    return false;
  }

  std::string gaugePath;
  std::string gaugeReason;
  if (!SaveManualGaugeAnnotation(context, "", "", gaugePath, gaugeReason)) {
    reason = "cannot save evidence review: " + gaugeReason;
    return false;
  }

  std::filesystem::path dir;
  if (!ResolveManualGaugeCaseDir(context, dir, reason))
    return false;

  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  if (ec) {
    reason = "cannot create evidence review directory: " + ec.message();
    return false;
  }

  const std::filesystem::path destination = dir / "evidence_review.json";
  const std::filesystem::path temporary = destination.string() + ".tmp";
  std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
  if (!file.is_open()) {
    reason = "cannot open temporary evidence review file";
    return false;
  }

  file << "{\n"
       << "  \"schema_version\": 1,\n"
       << "  \"review_status\": \""
       << jsonEscapeEvidenceReview(context.current_gauge.review_status)
       << "\",\n"
       << "  \"case_id\": \"" << jsonEscapeEvidenceReview(snapshot.case_id)
       << "\",\n"
       << "  \"script_id\": \"" << jsonEscapeEvidenceReview(snapshot.script_id)
       << "\",\n"
       << "  \"script_path\": \""
       << jsonEscapeEvidenceReview(snapshot.script_path) << "\",\n"
       << "  \"image_id\": \"" << jsonEscapeEvidenceReview(snapshot.image_id)
       << "\",\n"
       << "  \"image_path\": \""
       << jsonEscapeEvidenceReview(snapshot.image_path) << "\",\n"
       << "  \"target_id\": \"" << jsonEscapeEvidenceReview(snapshot.target_id)
       << "\",\n"
       << "  \"tool\": \"" << jsonEscapeEvidenceReview(snapshot.tool) << "\",\n"
       << "  \"parameter_summary\": \""
       << jsonEscapeEvidenceReview(snapshot.parameter_summary) << "\",\n"
       << "  \"gauge_annotation_path\": \""
       << jsonEscapeEvidenceReview(gaugePath) << "\",\n"
       << "  \"runtime_status\": \""
       << jsonEscapeEvidenceReview(context.debug_status) << "\",\n"
       << "  \"runtime_reason\": \""
       << jsonEscapeEvidenceReview(context.debug_reason) << "\",\n"
       << "  \"binding_policy\": \"evidence_locked_only\",\n"
       << "  \"runtime_int_globals\": {";

  bool first = true;
  for (const auto &kv : context.runtime_int_vars) {
    if (!first)
      file << ",";
    file << "\n    \"" << jsonEscapeEvidenceReview(kv.first)
         << "\": " << kv.second;
    first = false;
  }
  if (!first)
    file << "\n  ";
  file << "}\n"
       << "}\n";
  file.flush();
  const bool writeOk = file.good();
  file.close();
  if (!writeOk) {
    reason = "failed while writing evidence review";
    return false;
  }

  std::filesystem::rename(temporary, destination, ec);
  if (ec) {
    std::filesystem::remove(destination, ec);
    ec.clear();
    std::filesystem::rename(temporary, destination, ec);
  }
  if (ec) {
    reason = "cannot finalize evidence review: " + ec.message();
    return false;
  }

  outPath = destination.string();
  reason = "evidence review saved";
  return true;
}

void CopyRuntimeObjectDiagnosticsToEvidenceResult(
    const RuntimeObjectView &object, CxEvidenceSelfTestResult &result) {
  result.algorithm_status = object.runtime_state;
  result.measure_points_count = object.measure_points_count;
  result.valid_points_count = object.valid_points_count;
  result.has_measure_points =
      object.has_measure_points || object.has_line_measure_points;
  result.has_fit_result =
      object.has_fit_result || object.has_fit_line || object.has_fit_ellipse;

  if (object.type == "FindCircle") {
    result.algorithm_status =
        object.has_fit_result
            ? "fitcircle_available"
            : (object.valid_points_count > 0 ? "fitcircle_unavailable"
                                             : "no_measure_points");
    if (!object.circle_measure_failure_stage.empty()) {
      result.algorithm_reason = object.circle_measure_failure_stage + ": " +
                                object.circle_measure_detail;
    } else if (object.valid_points_count <= 0) {
      result.algorithm_reason = "FindCircle produced zero measure points.";
    } else if (!object.has_fit_result) {
      result.algorithm_reason = "FindCircle produced measure points, but "
                                "fitcircle result is unavailable.";
    } else {
      result.algorithm_reason = object.display_summary;
    }
    result.avgdist = object.fit_avgdist;
    result.fit_cx = object.fit_cx;
    result.fit_cy = object.fit_cy;
    result.fit_radius = object.fit_radius;
  } else if (object.type == "FindLine") {
    result.algorithm_status = object.line_result_status.empty()
                                  ? object.runtime_state
                                  : object.line_result_status;
    result.algorithm_reason = object.line_result_reason.empty()
                                  ? object.line_measure_failure_hint
                                  : object.line_result_reason;
    result.measure_points_count = object.line_measure_points_count;
    result.valid_points_count = object.valid_line_points_count;
    result.has_measure_points = object.has_line_measure_points;
    result.has_fit_result = object.has_fit_line;
    result.avgdist = object.line_avgdist;
    result.fit_cx = object.fit_line_x0;
    result.fit_cy = object.fit_line_y0;
    result.fit_radius_x = object.fit_line_x1;
    result.fit_radius_y = object.fit_line_y1;
  } else if (object.type == "FindEllipse") {
    result.algorithm_status = object.ellipse_result_status.empty()
                                  ? object.runtime_state
                                  : object.ellipse_result_status;
    result.algorithm_reason = object.ellipse_result_reason;
    result.avgdist = object.fit_ellipse_avgdist;
    result.fit_cx = object.fit_ellipse_cx;
    result.fit_cy = object.fit_ellipse_cy;
    result.fit_radius_x = object.fit_ellipse_rx;
    result.fit_radius_y = object.fit_ellipse_ry;
    result.fit_angle_deg = object.fit_ellipse_angle_deg;
  } else if (object.type == "FastMatch") {
    result.algorithm_status = object.fastmatch_status.empty()
                                  ? object.runtime_state
                                  : object.fastmatch_status;
    result.algorithm_reason = object.fastmatch_reason.empty()
                                  ? object.display_summary
                                  : object.fastmatch_reason;
    result.measure_points_count = object.measure_points_count;
    result.valid_points_count = object.valid_points_count;
    result.has_measure_points = object.measure_points_count > 0;
    result.has_fit_result = object.valid_points_count > 0;
    result.fastmatch_model_point_count = object.fastmatch_model_point_count;
    result.fastmatch_learn_a_count = object.fastmatch_learn_a_count;
    result.fastmatch_learn_b_count = object.fastmatch_learn_b_count;
    result.fastmatch_learn_a2_count = object.fastmatch_learn_a2_count;
    result.fastmatch_learn_b2_count = object.fastmatch_learn_b2_count;
    result.fastmatch_learn_status_code = object.fastmatch_learn_status_code;
    result.fastmatch_pattern_a_count = object.fastmatch_pattern_a_count;
    result.fastmatch_pattern_b_count = object.fastmatch_pattern_b_count;
    result.fastmatch_candidate_count = object.fastmatch_candidate_count;
    result.fastmatch_best_score = object.fastmatch_best_score;
  } else if (object.type == "FindRect") {
    result.algorithm_reason = object.display_summary;
  } else if (object.type == "FindSegmentation") {
    result.algorithm_status = object.segmentation_backend_status.empty()
                                  ? object.runtime_state
                                  : object.segmentation_backend_status;
    result.algorithm_reason = object.display_summary;
    result.measure_points_count = object.measure_points_count;
    result.valid_points_count = object.valid_points_count;
    result.has_measure_points = object.has_measure_points;
    result.has_fit_result = object.segmentation_has_boundary;
    result.result_ref = object.segmentation_result_ref;
    result.evidence_ref = object.segmentation_contour_ref;
    result.segmentation_backend_status = object.segmentation_backend_status;
    result.segmentation_result_ref = object.segmentation_result_ref;
    result.segmentation_mask_ref = object.segmentation_mask_ref;
    result.segmentation_contour_ref = object.segmentation_contour_ref;
    result.segmentation_overlay_ref = object.segmentation_overlay_ref;
    result.segmentation_contour_count = object.segmentation_contour_count;
    result.segmentation_primary_area = object.segmentation_primary_area;
  } else if (object.type == "TorchTask") {
    result.algorithm_status = object.torch_status.empty() ? object.runtime_state
                                                          : object.torch_status;
    result.algorithm_reason =
        "TorchTask status=" + result.algorithm_status +
        " device=" + object.torch_actual_device +
        " ok=" + std::to_string(object.torch_ok) +
        " error_code=" + std::to_string(object.torch_error_code) +
        " result_ref=" + object.torch_result_ref +
        " evidence_ref=" + object.torch_evidence_ref +
        " primary_visual_ref=" + object.torch_primary_visual_ref +
        " mask_ref=" + object.torch_mask_ref +
        " overlay_ref=" + object.torch_overlay_ref +
        " result_count=" + std::to_string(object.torch_result_count) +
        " mask_available=" + std::to_string(object.torch_mask_available) +
        " semantic_quality=NOT_CLAIMED";
    if (!object.torch_failure_stage.empty()) {
      result.algorithm_reason += " failure_stage=" + object.torch_failure_stage;
    }
    if (!object.torch_reason.empty()) {
      result.algorithm_reason += " reason=" + object.torch_reason;
    }
    if (!object.torch_trainer_lifecycle_summary.empty()) {
      result.algorithm_reason += " trainer_lifecycle_summary=" +
                                 object.torch_trainer_lifecycle_summary;
    }
    if (!object.torch_unified_mainline_summary.empty()) {
      result.algorithm_reason +=
          " unified_mainline_summary=" + object.torch_unified_mainline_summary;
    }
    result.measure_points_count = object.torch_result_count;
    result.valid_points_count = object.torch_result_count;
    result.has_measure_points =
        object.torch_result_count > 0 || object.torch_mask_available != 0 ||
        !object.torch_mask_ref.empty() || !object.torch_overlay_ref.empty();
    result.has_fit_result = object.torch_ok != 0;
    result.result_ref = object.torch_result_ref;
    result.evidence_ref = object.torch_evidence_ref;
    result.torch_result_ref = object.torch_result_ref;
    result.torch_evidence_ref = object.torch_evidence_ref;
    result.torch_primary_visual_ref = object.torch_primary_visual_ref;
    result.torch_mask_ref = object.torch_mask_ref;
    result.torch_overlay_ref = object.torch_overlay_ref;
    result.torch_actual_device = object.torch_actual_device;
    result.torch_trainer_lifecycle_summary =
        object.torch_trainer_lifecycle_summary;
    result.torch_unified_mainline_summary =
        object.torch_unified_mainline_summary;
    result.torch_ok = object.torch_ok;
    result.torch_error_code = object.torch_error_code;
    result.torch_result_count = object.torch_result_count;
    result.torch_mask_available = object.torch_mask_available;
    result.torch_infer_ms = object.torch_infer_ms;
    result.torch_train_ms = object.torch_train_ms;
    result.torch_total_ms = object.torch_total_ms;
  }
}

static Aspect_VKeyMouse mouseButtonFromGlfw(int theButton) {
  switch (theButton) {
  case GLFW_MOUSE_BUTTON_LEFT:
    return Aspect_VKeyMouse_LeftButton;
  case GLFW_MOUSE_BUTTON_RIGHT:
    return Aspect_VKeyMouse_RightButton;
  case GLFW_MOUSE_BUTTON_MIDDLE:
    return Aspect_VKeyMouse_MiddleButton;
  }
  return Aspect_VKeyMouse_NONE;
}

static Aspect_VKeyFlags keyFlagsFromGlfw(int theFlags) {
  Aspect_VKeyFlags aFlags = Aspect_VKeyFlags_NONE;
  if ((theFlags & GLFW_MOD_SHIFT) != 0) {
    aFlags |= Aspect_VKeyFlags_SHIFT;
  }
  if ((theFlags & GLFW_MOD_CONTROL) != 0) {
    aFlags |= Aspect_VKeyFlags_CTRL;
  }
  if ((theFlags & GLFW_MOD_ALT) != 0) {
    aFlags |= Aspect_VKeyFlags_ALT;
  }
  if ((theFlags & GLFW_MOD_SUPER) != 0) {
    aFlags |= Aspect_VKeyFlags_META;
  }
  return aFlags;
}
} // namespace

bool ViewController::opencvSW = false;
bool ViewController::opencvblur = false;
bool ViewController::opencvreset = false;
bool ViewController::gpublur = false;

bool ViewController::irunedge = false;

bool ViewController::ipythre = false;
bool ViewController::iotsuThreshold = false;
bool ViewController::ihedge = false;
bool ViewController::iwedge = false;

bool ViewController::ib2wedge = false;
bool ViewController::iw2bedge = false;

int ViewController::ivalue1 = 0;
int ViewController::ivalue2 = 0;

int ViewController::ivalue3 = 4;
int ViewController::ivalue4 = 11;

int ViewController::ivalue5 = 11;
int ViewController::ivalue6 = 2;

int ViewController::ivalue7 = 0;
int ViewController::ivalue8 = 0;

int ViewController::ivalue9 = 0;
int ViewController::ivalue10 = 0;

int ViewController::ivalue11 = 0;
int ViewController::ivalue12 = 0;

std::string formatNumber(double dvalue) {
  std::stringstream ss;
  ss << dvalue << ";";
  return ss.str();
}

ViewController::ViewController() {
  mouseDownPT.SetX(0);
  mouseDownPT.SetY(0);

  m_Pt.SetX(0);
  m_Pt.SetY(0);
  m_scale = 1.0;
}

ViewController::~ViewController() {}

gp_Pnt ViewController::ConvertClickToPoint(Standard_Real theX,
                                           Standard_Real theY,
                                           Handle(V3d_View) theView) {
  Standard_Real XEye, YEye, ZEye, XAt, YAt, ZAt;
  theView->Eye(XEye, YEye, ZEye);
  theView->At(XAt, YAt, ZAt);
  gp_Pnt EyePoint(XEye, YEye, ZEye);
  gp_Pnt AtPoint(XAt, YAt, ZAt);

  gp_Vec EyeVector(EyePoint, AtPoint);
  gp_Dir EyeDir(EyeVector);

  gp_Pln PlaneOfTheView = gp_Pln(AtPoint, EyeDir);
  Standard_Real X, Y, Z;
  theView->Convert(int(theX), int(theY), X, Y, Z);
  gp_Pnt ConvertedPoint(X, Y, Z);
  gp_Pnt ResultPoint;
  return ResultPoint;
}

ViewController *ViewController::toView(GLFWwindow *theWin) {
  return static_cast<ViewController *>(glfwGetWindowUserPointer(theWin));
}

void ViewController::errorCallback(int theError, const char *theDescription) {
  std::cerr << "GLFW error " << theError << ": "
            << (theDescription != nullptr ? theDescription : "") << std::endl;
}

GLuint gl_PBO, gl_Tex;
#if CXCORE_ENABLE_VIEWCONTROLLER_CUDA
struct cudaGraphicsResource *cuda_pbo_resource;
uchar4 *h_Src;
#endif
int imageW, imageH;
GLuint shader;

#if CXCORE_ENABLE_VIEWCONTROLLER_CUDA
int g_Kernel = 0;
bool g_FPS = false;
bool g_Diag = false;
StopWatchInterface *timer = NULL;

const float noiseStep = 0.025f;
const float lerpStep = 0.025f;
static float knnNoise = 0.32f;
static float nlmNoise = 1.45f;
static float lerpC = 0.2f;

const int frameN = 24;
int frameCounter = 0;

#define BUFFER_DATA(i) ((char *)0 + i)

const int frameCheckNumber = 4;
int fpsCount = 0;
int fpsLimit = 1;
unsigned int frameCount = 0;
unsigned int g_TotalErrors = 0;

int *pArgc = NULL;
char **pArgv = NULL;

#define MAX_EPSILON_ERROR 5
#define REFRESH_DELAY 10
#define BUFFER_DATA(i) ((char *)0 + i)

void runImageFiltersx(TColor *d_dst, int imageW, int imageH, int g_Kernel,
                      cudaTextureObject_t texImagex) {
  switch (g_Kernel) {
  case 0:
    cuda_Copy(d_dst, imageW, imageH, texImagex);
    break;

  case 1:
    if (!g_Diag) {
      cuda_KNN(d_dst, imageW, imageH, 1.0f / (knnNoise * knnNoise), lerpC,
               texImagex);
    } else {
      cuda_KNNdiag(d_dst, imageW, imageH, 1.0f / (knnNoise * knnNoise), lerpC,
                   texImagex);
    }

    break;

  case 2:
    if (!g_Diag) {
      cuda_NLM(d_dst, imageW, imageH, 1.0f / (nlmNoise * nlmNoise), lerpC,
               texImagex);
    } else {
      cuda_NLMdiag(d_dst, imageW, imageH, 1.0f / (nlmNoise * nlmNoise), lerpC,
                   texImagex);
    }

    break;

  case 3:
    if (!g_Diag) {
      cuda_NLM2(d_dst, imageW, imageH, 1.0f / (nlmNoise * nlmNoise), lerpC,
                texImagex);
    } else {
      cuda_NLM2diag(d_dst, imageW, imageH, 1.0f / (nlmNoise * nlmNoise), lerpC,
                    texImagex);
    }

    break;
  }
  cudaDeviceSynchronize();
  getLastCudaError("Filtering kernel execution failed.\n");
}
void runImageFilters(TColor *d_dst) {
  switch (g_Kernel) {
  case 0:
    cuda_Copy(d_dst, imageW, imageH, texImage);
    break;

  case 1:
    if (!g_Diag) {
      cuda_KNN(d_dst, imageW, imageH, 1.0f / (knnNoise * knnNoise), lerpC,
               texImage);
    } else {
      cuda_KNNdiag(d_dst, imageW, imageH, 1.0f / (knnNoise * knnNoise), lerpC,
                   texImage);
    }

    break;

  case 2:
    if (!g_Diag) {
      cuda_NLM(d_dst, imageW, imageH, 1.0f / (nlmNoise * nlmNoise), lerpC,
               texImage);
    } else {
      cuda_NLMdiag(d_dst, imageW, imageH, 1.0f / (nlmNoise * nlmNoise), lerpC,
                   texImage);
    }

    break;

  case 3:
    if (!g_Diag) {
      cuda_NLM2(d_dst, imageW, imageH, 1.0f / (nlmNoise * nlmNoise), lerpC,
                texImage);
    } else {
      cuda_NLM2diag(d_dst, imageW, imageH, 1.0f / (nlmNoise * nlmNoise), lerpC,
                    texImage);
    }

    break;
  }
  cudaDeviceSynchronize();
  getLastCudaError("Filtering kernel execution failed.\n");
}

void InitializeTextureAndPBO(int imageW, int imageH) {
  glGenBuffers(1, &gl_PBO);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, gl_PBO);
  glBufferData(GL_PIXEL_UNPACK_BUFFER, imageW * imageH * sizeof(uchar4), NULL,
               GL_DYNAMIC_DRAW);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

  cudaGraphicsResource *cuda_pbo_resource;
  checkCudaErrors(cudaGraphicsGLRegisterBuffer(
      &cuda_pbo_resource, gl_PBO, cudaGraphicsMapFlagsWriteDiscard));

  GLuint textureID;
  glGenTextures(1, &textureID);
  glBindTexture(GL_TEXTURE_2D, textureID);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, imageW, imageH, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, NULL);
  glBindTexture(GL_TEXTURE_2D, 0);
}
GLuint UpdateTextureWithCuda(int imageW, int imageH) {
  TColor *d_dst = NULL;
  size_t num_bytes;

  checkCudaErrors(cudaGraphicsMapResources(1, &cuda_pbo_resource, 0));
  getLastCudaError("cudaGraphicsMapResources failed");

  checkCudaErrors(cudaGraphicsResourceGetMappedPointer(
      (void **)&d_dst, &num_bytes, cuda_pbo_resource));
  getLastCudaError("cudaGraphicsResourceGetMappedPointer failed");

  runImageFilters(d_dst);

  checkCudaErrors(cudaDeviceSynchronize());

  checkCudaErrors(cudaGraphicsUnmapResources(1, &cuda_pbo_resource, 0));

  glBindTexture(GL_TEXTURE_2D, texImage);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, imageW, imageH, GL_RGBA,
                  GL_UNSIGNED_BYTE, BUFFER_DATA(0));
  glBindTexture(GL_TEXTURE_2D, 0);

  return texImage;
}

#define cudaCheckErrors(ans)                                                   \
  { gpuAssert((ans), __FILE__, __LINE__); }
inline void gpuAssert(cudaError_t code, const char *file, int line,
                      bool abort = true) {
  if (code != cudaSuccess) {
    fprintf(stderr, "GPUassert: %s %s %d\n", cudaGetErrorString(code), file,
            line);
    if (abort)
      exit(code);
  }
}

GLuint CreateTextureCube(cv::Mat &src) {
  if (src.empty()) {
    std::cerr << "Input image is empty!" << std::endl;
    return 0;
  }

  cv::Mat src_rgba;
  if (src.channels() == 3) {
    cv::cvtColor(src, src_rgba, cv::COLOR_BGR2RGBA);
  } else if (src.channels() == 4) {
    src_rgba = src;
  } else {
    std::cerr << "Unsupported number of channels: " << src.channels()
              << std::endl;
    return 0;
  }

  GLuint gl_texture_id;
  glGenTextures(1, &gl_texture_id);
  glBindTexture(GL_TEXTURE_2D, gl_texture_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, src.cols, src.rows, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, src_rgba.data);
  glBindTexture(GL_TEXTURE_2D, 0);

  cudaGraphicsResource *cuda_tex_resource;
  cudaCheckErrors(cudaGraphicsGLRegisterImage(
      &cuda_tex_resource, gl_texture_id, GL_TEXTURE_2D,
      cudaGraphicsMapFlagsWriteDiscard));

  void *dev_ptr;
  size_t num_bytes;
  cudaCheckErrors(cudaGraphicsMapResources(1, &cuda_tex_resource, 0));
  cudaCheckErrors(cudaGraphicsSubResourceGetMappedArray(
      (cudaArray **)&dev_ptr, cuda_tex_resource, 0, 0));

  cudaChannelFormatDesc channel_desc = cudaCreateChannelDesc<uchar4>();
  cudaMemcpyToArray((cudaArray *)dev_ptr, 0, 0, src_rgba.data,
                    src.cols * src.rows * sizeof(uchar4),
                    cudaMemcpyHostToDevice);

  cudaCheckErrors(cudaGraphicsUnmapResources(1, &cuda_tex_resource, 0));

  return gl_texture_id;
}
#else
GLuint CreateTextureCube(cv::Mat &src) {
  if (src.empty()) {
    return 0;
  }

  cv::Mat src_rgba;
  if (src.channels() == 3) {
    cv::cvtColor(src, src_rgba, cv::COLOR_BGR2RGBA);
  } else if (src.channels() == 4) {
    src_rgba = src;
  } else {
    return 0;
  }

  GLuint gl_texture_id = 0;
  glGenTextures(1, &gl_texture_id);
  glBindTexture(GL_TEXTURE_2D, gl_texture_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, src.cols, src.rows, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, src_rgba.data);
  glBindTexture(GL_TEXTURE_2D, 0);
  return gl_texture_id;
}

GLuint UpdateTextureWithCuda(int, int) { return 0; }
#endif

void ViewController::run() {
  int iw = 2048 / 2;
  int ih = 1536 / 2;
  std::cerr << "[cxvision_imgui_acceptance] run OK"
      << std::endl;
  SetCxCrashBreadcrumb("ViewController::run:begin");
  CXLOG_INFO("ViewController", "run_stage", "running", "stage=begin");
  SetCxCrashBreadcrumb("ViewController::run:initWindow");
  CXLOG_INFO("ViewController", "run_stage", "running", "stage=initWindow");

 
  initWindow(iw, ih, "glfw occt image ai");
  
  SetCxCrashBreadcrumb("ViewController::run:initViewer");
  CXLOG_INFO("ViewController", "run_stage", "running", "stage=initViewer");
  initViewer(iw, ih);
  SetCxCrashBreadcrumb("ViewController::run:initDemoScene");
  CXLOG_INFO("ViewController", "run_stage", "running", "stage=initDemoScene");
  initDemoScene();

#if CXCORE_ENABLE_VIEWCONTROLLER_CUDA
  InitializeTextureAndPBO(2048, 1536);
#endif
  SetCxCrashBreadcrumb("ViewController::run:Imgui_OpenCV_Ini0");
  CXLOG_INFO("ViewController", "run_stage", "running",
             "stage=Imgui_OpenCV_Ini0");
  Imgui_OpenCV_Ini0();
  SetCxCrashBreadcrumb("ViewController::run:initScriptCatalog");
  CXLOG_INFO("ViewController", "run_stage", "running",
             "stage=initScriptCatalog");
  initScriptCatalog();
  SetCxCrashBreadcrumb("ViewController::run:initManualStateTestConsole");
  CXLOG_INFO("ViewController", "run_stage", "running",
             "stage=initManualStateTestConsole");
  initManualStateTestConsole();
  SetCxCrashBreadcrumb("ViewController::run:initImageEvidenceLayer");
  CXLOG_INFO("ViewController", "run_stage", "running",
             "stage=initImageEvidenceLayer");
  initImageEvidenceLayer();
  SetCxCrashBreadcrumb("ViewController::run:semanticFlowInitialize");
  CXLOG_INFO("ViewController", "run_stage", "running",
             "stage=semanticFlowInitialize");
  m_semanticFlowGraph.Initialize(findRepositoryRoot().generic_string());
  SetCxCrashBreadcrumb("ViewController::run:mainloop");
  CXLOG_INFO("ViewController", "run_stage", "running", "stage=mainloop");
  mainloop();
  SetCxCrashBreadcrumb("ViewController::run:cleanup");
  CXLOG_INFO("ViewController", "run_stage", "running", "stage=cleanup");
  cleanup();
  CXLOG_INFO("ViewController", "run_stage", "finished", "stage=end");
}

bool ViewController::InitEvidenceSelfTestEnvironment(std::string &reason) {
  reason.clear();

  std::string init_reason;
  m_parserOwner.ConfigureStreams(&m_os, &m_createcodeos);
  m_parserDebugBridge.Bind(&m_parserOwner);
  if (!m_parserOwner.Initialize(init_reason)) {
    reason = "parser initialization failed: " + init_reason;
    return false;
  }

  initScriptCatalog();
  initManualStateTestConsole();
  initImageEvidenceLayer();
  EnsureCxScriptWorkbenchAssetsLoaded();

  if (m_scriptCatalog.empty() && m_manualTest.script_evidence_groups.empty()) {
    reason = "evidence selftest environment has no scripts or evidence rows";
    return false;
  }

  reason = "evidence selftest environment initialized";
  return true;
}

void ViewController::initImGui() {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  ImGui::StyleColorsDark();

  ImGui_ImplGlfw_InitForOpenGL(myOcctWindow->getGlfwWindow(), true);
  ImGui_ImplOpenGL3_Init("#version 450");
}

void ViewController::initScriptCatalog() {
  m_scriptCatalog.clear();
  m_selectedScript = -1;
  const fs::path root = findRepositoryRoot();
  if (root.empty()) {
    m_scriptResult.status = "BLOCKED";
    m_scriptResult.reason = "cxparser/cxscript directory not found";
    m_scriptResult.runtime_fillback_status = "not_started";
    return;
  }

  const std::pair<const char *, const char *> scanRoots[] = {
      {"module", "cxparser/cxscript/module"},
      {"integration", "cxparser/cxscript/integration"}};
  for (const auto &scanRoot : scanRoots) {
    const fs::path directory = root / scanRoot.second;
    if (!fs::exists(directory))
      continue;
    for (const fs::directory_entry &entry :
         fs::recursive_directory_iterator(directory)) {
      if (!entry.is_regular_file())
        continue;
      const std::string extension = entry.path().extension().string();
      if (extension != ".cxs" && extension != ".cxsc")
        continue;
      ScriptCatalogEntry item;
      item.name = entry.path().filename().string();
      item.path = fs::relative(entry.path(), root).generic_string();
      item.type = scanRoot.first;
      item.status = "ready";
      const bool isDirectLike =
          item.name.find("direct_test") != std::string::npos ||
          item.name.find("_direct") != std::string::npos ||
          item.name.find("_smoke") != std::string::npos ||
          item.path.find("/headless/") != std::string::npos;
      item.description = isDirectLike
                             ? "C/C++ statement-level direct CxScript case."
                             : "CxScript " + item.type + " case.";
      m_scriptCatalog.push_back(item);
    }
  }
  std::sort(
      m_scriptCatalog.begin(), m_scriptCatalog.end(),
      [](const ScriptCatalogEntry &left, const ScriptCatalogEntry &right) {
        return left.path < right.path;
      });
  for (std::size_t i = 0; i < m_scriptCatalog.size(); ++i) {
    if (m_scriptCatalog[i].path ==
        "cxparser/cxscript/module/cximage/headless/fastmatch_l1_direct.cxsc") {
      m_selectedScript = static_cast<int>(i);
      break;
    }
  }
  if (m_selectedScript < 0 && !m_scriptCatalog.empty())
    m_selectedScript = 0;
  m_scriptResult.status = "PENDING";
  m_scriptResult.reason = "select a script and run it";
  m_scriptResult.runtime_fillback_status = "not_started";
}

bool ViewController::GetSelectedCatalogScript(std::string &outPath,
                                              std::string &outName) const {
  outPath.clear();
  outName.clear();

  if (m_selectedScript < 0 ||
      m_selectedScript >= static_cast<int>(m_scriptCatalog.size())) {
    return false;
  }

  const ScriptCatalogEntry &entry =
      m_scriptCatalog[static_cast<std::size_t>(m_selectedScript)];

  outPath = entry.path;
  outName = entry.name;
  return !outPath.empty();
}

static SemanticEvidenceBinding
MakeSemanticEvidenceBinding(const CxEvidenceSelectionSnapshot &snapshot) {
  SemanticEvidenceBinding binding;
  binding.valid = snapshot.valid;

  binding.case_id = snapshot.case_id;

  binding.script_id = snapshot.script_id;
  binding.script_path = snapshot.script_path;

  binding.image_id = snapshot.image_id;
  binding.image_path = snapshot.image_path;

  binding.target_id = snapshot.target_id;
  binding.tool = snapshot.tool;

  binding.parameter_profile_id = snapshot.parameter_profile_id;
  binding.parameter_summary = snapshot.parameter_summary;

  binding.status = snapshot.status;
  binding.reason = snapshot.reason;
  binding.source =
      snapshot.source.empty() ? "evidence_selection" : snapshot.source;

  return binding;
}

static std::string
NormalizeSemanticEvidenceObjectTypeLocal(const std::string &typeOrTool) {
  std::string lowered = typeOrTool;
  std::transform(
      lowered.begin(), lowered.end(), lowered.begin(),
      [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

  if (lowered == "findline")
    return "FindLine";
  if (lowered == "findcircle")
    return "FindCircle";
  if (lowered == "findellipse")
    return "FindEllipse";
  if (lowered == "findrect")
    return "FindRect";
  if (lowered == "findobject" || lowered == "find_object")
    return "FindObject";
  if (lowered == "fastmatch" || lowered == "cfastmatch")
    return "FastMatch";
  if (lowered == "gridpatternclasstool" || lowered == "gridpatternclass")
    return "GridPatternClassTool";
  if (lowered == "regionpatterntool" || lowered == "regionpattern")
    return "RegionPatternTool";
  if (lowered == "findsegmentation")
    return "FindSegmentation";
  return typeOrTool;
}

static bool IsSemanticEvidenceEditableObjectTypeLocal(const std::string &type) {
  const std::string normalized = NormalizeSemanticEvidenceObjectTypeLocal(type);
  return normalized == "FindLine" || normalized == "FindCircle" ||
         normalized == "FindEllipse" || normalized == "FindRect" ||
         normalized == "FindObject" || normalized == "FastMatch" ||
         normalized == "GridPatternClassTool" ||
         normalized == "RegionPatternTool" || normalized == "FindSegmentation";
}

static std::string
StripSemanticEvidenceLineCommentLocal(const std::string &line) {
  const std::size_t comment = line.find("//");
  if (comment == std::string::npos)
    return line;
  return line.substr(0, comment);
}

static void AnalyzeSemanticEvidenceEditableObjectsLocal(
    const std::string &scriptText,
    std::vector<CxEvidenceEditableObjectRef> &outObjects) {
  outObjects.clear();

  std::istringstream input(scriptText);
  std::string raw;
  int lineNo = 1;
  while (std::getline(input, raw)) {
    const std::string statement =
        TrimLine(StripSemanticEvidenceLineCommentLocal(raw));
    if (statement.empty() || statement.find('(') != std::string::npos ||
        statement.find('=') != std::string::npos) {
      ++lineNo;
      continue;
    }

    std::istringstream tokens(statement);
    std::string type;
    std::string name;
    tokens >> type >> name;
    if (type.empty() || name.empty()) {
      ++lineNo;
      continue;
    }

    const std::size_t suffix = name.find_first_of(";");
    if (suffix != std::string::npos)
      name.erase(suffix);

    if (!name.empty() && IsSemanticEvidenceEditableObjectTypeLocal(type)) {
      CxEvidenceEditableObjectRef ref;
      ref.type = NormalizeSemanticEvidenceObjectTypeLocal(type);
      ref.name = name;
      ref.declared_line = lineNo;
      outObjects.push_back(ref);
    }

    ++lineNo;
  }
}

static std::string
ReadSemanticEvidenceParamValueLocal(const std::string &summary,
                                    const std::string &key) {
  std::istringstream input(summary);
  std::string line;
  while (std::getline(input, line)) {
    line = TrimLine(line);
    const std::string prefix = key + "=";
    if (line.rfind(prefix, 0) == 0)
      return TrimLine(line.substr(prefix.size()));
  }
  return {};
}

static void ResolveSemanticEvidencePrimaryObjectLocal(
    const CxEvidenceSelectionSnapshot &snapshot, std::string &outType,
    std::string &outName, std::string &outStatus) {
  outType.clear();
  outName.clear();
  outStatus.clear();

  if (snapshot.editable_objects.empty()) {
    outStatus = snapshot.script_path.empty() ? "script_path_empty"
                                             : "no_editable_object";
    return;
  }

  std::string requested = ReadSemanticEvidenceParamValueLocal(
      snapshot.parameter_summary, "primary_object_ref");
  if (requested.empty()) {
    requested = ReadSemanticEvidenceParamValueLocal(snapshot.parameter_summary,
                                                    "primary_object");
  }
  if (requested.empty()) {
    requested = ReadSemanticEvidenceParamValueLocal(snapshot.parameter_summary,
                                                    "object_ref");
  }

  if (!requested.empty()) {
    for (const auto &ref : snapshot.editable_objects) {
      if (ref.name == requested) {
        outType = ref.type;
        outName = ref.name;
        outStatus = "parameter_primary_object";
        return;
      }
    }
    outStatus = "parameter_primary_object_not_found";
    return;
  }

  if (!snapshot.target_id.empty()) {
    for (const auto &ref : snapshot.editable_objects) {
      if (ref.name == snapshot.target_id) {
        outType = ref.type;
        outName = ref.name;
        outStatus = "target_object_match";
        return;
      }
    }
  }

  const std::string expectedTool =
      NormalizeSemanticEvidenceObjectTypeLocal(snapshot.tool);
  std::vector<CxEvidenceEditableObjectRef> toolMatches;
  for (const auto &ref : snapshot.editable_objects) {
    if (NormalizeSemanticEvidenceObjectTypeLocal(ref.type) == expectedTool) {
      toolMatches.push_back(ref);
    }
  }

  if (toolMatches.size() == 1) {
    outType = toolMatches.front().type;
    outName = toolMatches.front().name;
    outStatus = "single_matching_tool_object";
    return;
  }

  if (snapshot.editable_objects.size() == 1) {
    outType = snapshot.editable_objects.front().type;
    outName = snapshot.editable_objects.front().name;
    outStatus = "single_editable_object";
    return;
  }

  outStatus = toolMatches.empty() ? "needs_object_selection_no_tool_match"
                                  : "needs_object_selection";
}

static void EnrichSemanticEvidenceSnapshotEditableObjectsLocal(
    CxEvidenceSelectionSnapshot &snapshot) {
  snapshot.editable_objects.clear();
  snapshot.primary_object_type.clear();
  snapshot.primary_object_name.clear();
  snapshot.primary_object_status.clear();

  if (snapshot.script_path.empty()) {
    snapshot.primary_object_status = "script_path_empty";
    return;
  }

  std::string scriptText;
  if (!ReadTextFile(snapshot.script_path, scriptText)) {
    snapshot.primary_object_status = "script_read_failed";
    return;
  }

  AnalyzeSemanticEvidenceEditableObjectsLocal(scriptText,
                                              snapshot.editable_objects);
  ResolveSemanticEvidencePrimaryObjectLocal(
      snapshot, snapshot.primary_object_type, snapshot.primary_object_name,
      snapshot.primary_object_status);
}

static CxEvidenceSelectionSnapshot
MakeEvidenceSelectionSnapshot(const SemanticEvidenceBinding &binding) {
  CxEvidenceSelectionSnapshot snapshot;
  snapshot.valid = binding.valid;

  snapshot.group_index = -1;
  snapshot.thumb_index = -1;

  snapshot.case_id = binding.case_id;

  snapshot.script_id = binding.script_id;
  snapshot.script_path = binding.script_path;

  snapshot.image_id = binding.image_id;
  snapshot.image_path = binding.image_path;

  snapshot.target_id = binding.target_id;
  snapshot.tool = binding.tool;

  snapshot.parameter_profile_id = binding.parameter_profile_id;
  snapshot.parameter_summary = binding.parameter_summary;

  snapshot.status = binding.status;
  snapshot.reason = binding.reason;
  snapshot.source =
      binding.source.empty() ? "semantic_node_evidence" : binding.source;

  EnrichSemanticEvidenceSnapshotEditableObjectsLocal(snapshot);

  return snapshot;
}

bool ViewController::LoadSemanticEvidenceBindingToManualConsole(
    const std::string &nodeId, const SemanticEvidenceBinding &binding,
    bool loadImageToView, std::string &reason) {
  reason.clear();

  if (!binding.valid) {
    reason = "semantic evidence binding is invalid";
    return false;
  }

  if (binding.script_path.empty()) {
    reason = "semantic evidence binding has empty script_path";
    return false;
  }

  CxEvidenceSelectionSnapshot snapshot = MakeEvidenceSelectionSnapshot(binding);

  if (!ApplyEvidenceSelectionSnapshotToManualContext(snapshot, loadImageToView,
                                                     reason)) {
    return false;
  }

  m_manualTest.bound_state_node_id = nodeId;
  m_manualTest.bound_state_script_path = binding.script_path;
  m_manualTest.active_script_case_name = nodeId;
  m_manualTest.active_script_case_path = binding.script_path;
  m_manualTest.active_script_case_purpose = "semantic_evidence_binding";

  m_manualTest.debug_status = loadImageToView
                                  ? "SEMANTIC_EVIDENCE_LOADED_WITH_IMAGE"
                                  : "SEMANTIC_EVIDENCE_LOADED";
  m_manualTest.debug_reason =
      "node=" + nodeId + " script=" + binding.script_id +
      " image=" + binding.image_id + " target=" + binding.target_id +
      " param=" + binding.parameter_summary;

  reason = m_manualTest.debug_reason;
  return true;
}

bool ViewController::BuildSemanticExecutionContext(
    const SemanticFlowAction &action, SemanticExecutionContext &out,
    std::string &reason) const {
  reason.clear();
  out = SemanticExecutionContext{};

  out.node_id = action.node_id;

  if (action.has_evidence_binding && action.evidence_binding.valid) {
    const SemanticEvidenceBinding &binding = action.evidence_binding;

    if (binding.script_path.empty()) {
      reason = "evidence execution context has empty script_path";
      return false;
    }

    out.valid = true;
    out.from_evidence = true;

    out.case_id = binding.case_id;
    out.script_id = binding.script_id;
    out.script_path = binding.script_path;
    out.image_id = binding.image_id;
    out.image_path = binding.image_path;
    out.target_id = binding.target_id;
    out.tool = binding.tool;
    out.parameter_profile_id = binding.parameter_profile_id;
    out.parameter_summary = binding.parameter_summary;

    out.reason =
        "execution context from semantic evidence: script=" + out.script_id +
        " image=" + out.image_id + " target=" + out.target_id +
        " param=" + out.parameter_summary;

    return true;
  }

  if (action.script_path.empty()) {
    reason = "legacy execution context has empty script_path";
    return false;
  }

  out.valid = true;
  out.from_evidence = false;
  out.script_path = action.script_path;
  out.script_id = action.script_path;
  out.reason = "execution context from legacy node script";

  return true;
}

bool ViewController::ApplySemanticExecutionContextBeforeRun(
    const SemanticExecutionContext &context, std::string &reason) {
  reason.clear();

  if (!context.valid) {
    reason = "invalid semantic execution context";
    return false;
  }

  if (context.script_path.empty()) {
    reason = "semantic execution context script_path is empty";
    return false;
  }

  if (context.from_evidence) {
    const std::string previousImagePath = m_manualTest.image_file_path;

    m_manualTest.active_case_id = context.case_id;
    m_manualTest.active_image_id = context.image_id;
    m_manualTest.active_target_id = context.target_id;

    m_manualTest.loaded_script_path = context.script_path;
    m_manualTest.bound_state_node_id = context.node_id;
    m_manualTest.bound_state_script_path = context.script_path;
    m_manualTest.active_script_case_name = context.node_id;
    m_manualTest.active_script_case_path = context.script_path;
    m_manualTest.active_script_case_purpose = "semantic_evidence_run";

    m_manualTest.runtime_int_vars.clear();

    if (context.parameter_summary.empty() || context.parameter_summary == "-" ||
        context.parameter_summary.find('=') == std::string::npos) {
      reason = "semantic evidence run requires locked key=value parameters; "
               "current param=" +
               context.parameter_summary;
      m_manualTest.debug_status = "EVIDENCE_PARAM_LOCK_REQUIRED";
      m_manualTest.debug_reason = reason;
      return false;
    }

    std::string paramReason;
    if (!ApplyEvidenceParameterSummaryToRuntimeGlobals(
            context.parameter_summary, paramReason)) {
      reason = "failed to apply locked evidence parameters: " + paramReason;
      m_manualTest.debug_status = "EVIDENCE_PARAM_BIND_FAIL";
      m_manualTest.debug_reason = reason;
      return false;
    }

    syncEvidenceLockedGlobalsToManualGauge(
        m_manualTest, context.script_path, context.tool,
        "semantic_evidence_locked");

    if (!context.image_path.empty()) {
      const bool needLoadImage =
          m_imageViewImage.empty() || previousImagePath != context.image_path;

      if (needLoadImage) {
        if (!LoadImageIntoImageView(context.image_path, reason))
          return false;
      }

      m_manualTest.image_file_path = context.image_path;
    }

    std::string scriptTextForGlobals;
    std::string scriptGlobalReason;
    if (!ReadTextFile(context.script_path, scriptTextForGlobals)) {
      reason = "failed to read locked evidence script: " + context.script_path;
      m_manualTest.debug_status = "EVIDENCE_SCRIPT_READ_FAIL";
      m_manualTest.debug_reason = reason;
      return false;
    }
    const int imageWidth = m_imageViewImage.empty() ? 0 : m_imageViewImage.cols;
    const int imageHeight =
        m_imageViewImage.empty() ? 0 : m_imageViewImage.rows;
    if (!stageEvidenceSelfTestScriptGlobals(
            m_parserDebugBridge, m_manualTest.runtime_int_vars,
            scriptTextForGlobals, context.parameter_summary, imageWidth,
            imageHeight, false, scriptGlobalReason)) {
      reason = scriptGlobalReason;
      m_manualTest.debug_status = "EVIDENCE_PARAM_BIND_FAIL";
      m_manualTest.debug_reason = reason;
      return false;
    }

    m_manualTest.debug_status = "SEMANTIC_EXECUTION_CONTEXT_READY";
    m_manualTest.debug_reason =
        context.reason + " | " + paramReason + " | " + scriptGlobalReason;
  } else {
    m_manualTest.loaded_script_path = context.script_path;
    m_manualTest.bound_state_node_id = context.node_id;
    m_manualTest.bound_state_script_path = context.script_path;
    m_manualTest.debug_status = "SEMANTIC_LEGACY_EXECUTION_CONTEXT_READY";
    m_manualTest.debug_reason = context.reason;
  }

  reason = context.reason;
  return true;
}

bool ViewController::ApplyEvidenceParameterSummaryToRuntimeGlobals(
    const std::string &parameterSummary, std::string &reason) {
  return ApplyEvidenceParameterSummaryToRuntimeGlobals(
      m_manualTest, parameterSummary, reason);
}

bool ViewController::ApplyEvidenceParameterSummaryToRuntimeGlobals(
    ManualTestContext &context, const std::string &parameterSummary,
    std::string &reason) {
  reason.clear();

  if (parameterSummary.empty() || parameterSummary == "-") {
    reason = "parameter summary is empty";
    return false;
  }

  bool supportedTokenPresent = false;

  auto applyIntToken = [&](const std::string &key,
                           const std::string &globalName) -> bool {
    const std::string pattern = key + "=";
    const std::size_t pos = parameterSummary.find(pattern);
    if (pos == std::string::npos)
      return false;

    supportedTokenPresent = true;

    std::size_t begin = pos + pattern.size();
    std::size_t end = begin;
    while (end < parameterSummary.size() &&
           (std::isdigit(static_cast<unsigned char>(parameterSummary[end])) ||
            parameterSummary[end] == '-' || parameterSummary[end] == '+')) {
      ++end;
    }

    if (end == begin)
      return false;

    int value = 0;
    try {
      value = std::stoi(parameterSummary.substr(begin, end - begin));
    } catch (...) {
      return false;
    }

    context.runtime_int_vars[globalName] = value;
    return true;
  };

  auto applyRectToken = [&](const std::string &key, const std::string &globalX,
                            const std::string &globalY,
                            const std::string &globalW,
                            const std::string &globalH) -> bool {
    const std::string pattern = key + "=";
    const std::size_t pos = parameterSummary.find(pattern);
    if (pos == std::string::npos)
      return false;

    supportedTokenPresent = true;

    std::size_t cursor = pos + pattern.size();
    int values[4] = {0, 0, 0, 0};
    for (int i = 0; i < 4; ++i) {
      while (
          cursor < parameterSummary.size() &&
          (std::isspace(static_cast<unsigned char>(parameterSummary[cursor])) ||
           parameterSummary[cursor] == ',')) {
        ++cursor;
      }

      std::size_t end = cursor;
      while (end < parameterSummary.size() &&
             (std::isdigit(static_cast<unsigned char>(parameterSummary[end])) ||
              parameterSummary[end] == '-' || parameterSummary[end] == '+')) {
        ++end;
      }

      if (end == cursor)
        return false;

      try {
        values[i] = std::stoi(parameterSummary.substr(cursor, end - cursor));
      } catch (...) {
        return false;
      }

      cursor = end;
      if (i < 3) {
        if (cursor >= parameterSummary.size() ||
            parameterSummary[cursor] != ',')
          return false;
        ++cursor;
      }
    }

    context.runtime_int_vars[globalX] = values[0];
    context.runtime_int_vars[globalY] = values[1];
    context.runtime_int_vars[globalW] = values[2];
    context.runtime_int_vars[globalH] = values[3];
    return true;
  };

  int applied = 0;

  // A scan rotation belongs to the incoming FastMatch parameter snapshot.
  // Never let a summary which predates this field inherit an angle from an
  // earlier working case.  A snapshot can still explicitly override this
  // zero-degree default below.
  context.runtime_int_vars["global_fastmatch_scan_rotation_deg"] = 0;

  applied += applyIntToken("method", "global_method") ? 1 : 0;
  applied += applyIntToken("threshold", "global_threshold") ? 1 : 0;
  applied += applyIntToken("thr", "global_threshold") ? 1 : 0;
  applied += applyIntToken("wgap", "global_wgap") ? 1 : 0;
  applied += applyIntToken("hgap", "global_hgap") ? 1 : 0;
  applied += applyIntToken("gap", "global_gap") ? 1 : 0;
  applied += applyIntToken("linegap", "global_linegap") ? 1 : 0;
  applied += applyIntToken("lgap", "global_linegap") ? 1 : 0;
  applied += applyIntToken("filterprofile", "global_filterprofile") ? 1 : 0;
  applied += applyIntToken("tool_half_width", "global_tool_half_width") ? 1 : 0;
  applied += applyIntToken("half_width", "global_tool_half_width") ? 1 : 0;
  applied += applyIntToken("gauge", "global_gauge") ? 1 : 0;
  applied += applyIntToken("find_num", "global_find_num") ? 1 : 0;
  applied += applyIntToken("compare_gap", "global_compare_gap") ? 1 : 0;
  applied += applyIntToken("objfilter", "global_objfilter") ? 1 : 0;
  applied +=
      applyIntToken("fastmatch_action", "global_fastmatch_action") ? 1 : 0;
  applied += applyIntToken("fastmatch_scan_rotation_deg",
                           "global_fastmatch_scan_rotation_deg")
                 ? 1
                 : 0;
  applied += applyIntToken("fastmatch_learn_shared",
                           "global_fastmatch_learn_shared")
                 ? 1
                 : 0;

  const std::pair<const char *, const char *> fastMatchNormalTraceParams[] = {
      {"fastmatch_normaltrace_enabled",
       "global_fastmatch_normaltrace_enabled"},
      {"fastmatch_normaltrace_overlap_radius_px",
       "global_fastmatch_normaltrace_overlap_radius_px"},
      {"fastmatch_normaltrace_xy_compression_bin_px",
       "global_fastmatch_normaltrace_xy_compression_bin_px"},
      {"fastmatch_normaltrace_knn_neighbors",
       "global_fastmatch_normaltrace_knn_neighbors"},
      {"fastmatch_normaltrace_ann_radius_px",
       "global_fastmatch_normaltrace_ann_radius_px"},
      {"fastmatch_normaltrace_ann_min_component_points",
       "global_fastmatch_normaltrace_ann_min_component_points"},
      {"fastmatch_normaltrace_ann_min_component_coverage_percent",
       "global_fastmatch_normaltrace_ann_min_component_coverage_percent"},
      {"fastmatch_normaltrace_pair_offset_px",
       "global_fastmatch_normaltrace_pair_offset_px"},
      {"fastmatch_normaltrace_min_keypoints_per_domain",
       "global_fastmatch_normaltrace_min_keypoints_per_domain"},
      {"fastmatch_normaltrace_min_length_px",
       "global_fastmatch_normaltrace_min_length_px"},
      {"fastmatch_normaltrace_min_gradient",
       "global_fastmatch_normaltrace_min_gradient"},
      {"fastmatch_normaltrace_max_nodes",
       "global_fastmatch_normaltrace_max_nodes"},
      {"fastmatch_normaltrace_max_trace_gap_px",
       "global_fastmatch_normaltrace_max_trace_gap_px"},
      {"fastmatch_normaltrace_anchor_radius_px",
       "global_fastmatch_normaltrace_anchor_radius_px"},
      {"fastmatch_normaltrace_ann_tangent_deviation_deg",
       "global_fastmatch_normaltrace_ann_tangent_deviation_deg"},
      {"fastmatch_normaltrace_ann_normal_deviation_deg",
       "global_fastmatch_normaltrace_ann_normal_deviation_deg"},
      {"fastmatch_normaltrace_polarity",
       "global_fastmatch_normaltrace_polarity"}};
  for (const auto &entry : fastMatchNormalTraceParams)
    applied += applyIntToken(entry.first, entry.second) ? 1 : 0;

  for (int direction = 0; direction < 4; ++direction) {
    const std::string suffix = std::to_string(direction);
    const std::pair<std::string, std::string> directionalParams[] = {
        {"fastmatch_learn_threshold_" + suffix,
         "global_fastmatch_learn_threshold_" + suffix},
        {"fastmatch_learn_method_" + suffix,
         "global_fastmatch_learn_method_" + suffix},
        {"fastmatch_learn_wgap_" + suffix,
         "global_fastmatch_learn_wgap_" + suffix},
        {"fastmatch_learn_hgap_" + suffix,
         "global_fastmatch_learn_hgap_" + suffix},
        {"fastmatch_learn_linegap_" + suffix,
         "global_fastmatch_learn_linegap_" + suffix},
        {"fastmatch_learn_compare_gap_" + suffix,
         "global_fastmatch_learn_compare_gap_" + suffix},
        {"fastmatch_learn_objfilter_" + suffix,
         "global_fastmatch_learn_objfilter_" + suffix},
        {"fastmatch_learn_edge_count_" + suffix,
         "global_fastmatch_learn_edge_count_" + suffix},
        {"fastmatch_learn_selected_edge_" + suffix,
         "global_fastmatch_learn_selected_edge_" + suffix}};
    for (const auto &entry : directionalParams)
      applied += applyIntToken(entry.first, entry.second) ? 1 : 0;
  }
  applied += applyIntToken("max_elapsed_ms", "global_max_elapsed_ms") ? 1 : 0;
  applied += applyIntToken("max_scan_lines", "global_max_scan_lines") ? 1 : 0;
  applied += applyIntToken("max_samples", "global_max_samples") ? 1 : 0;
  applied += applyIntToken("min_edge_run_width",
                           "global_min_edge_run_width_px")
                 ? 1
                 : 0;
  applied += applyIntToken("min_edge_run_width_px",
                           "global_min_edge_run_width_px")
                 ? 1
                 : 0;
  applied += applyIntToken("minedgerunwidth",
                           "global_min_edge_run_width_px")
                 ? 1
                 : 0;

  applied += applyIntToken("roi_x0", "global_roi_x0") ? 1 : 0;
  applied += applyIntToken("roi_y0", "global_roi_y0") ? 1 : 0;
  applied += applyIntToken("roi_x1", "global_roi_x1") ? 1 : 0;
  applied += applyIntToken("roi_y1", "global_roi_y1") ? 1 : 0;
  applied += applyIntToken("x0", "global_roi_x0") ? 1 : 0;
  applied += applyIntToken("y0", "global_roi_y0") ? 1 : 0;
  applied += applyIntToken("x1", "global_roi_x1") ? 1 : 0;
  applied += applyIntToken("y1", "global_roi_y1") ? 1 : 0;

  applied += applyIntToken("roi_x", "global_roi_x") ? 1 : 0;
  applied += applyIntToken("roi_y", "global_roi_y") ? 1 : 0;
  applied += applyIntToken("roi_width", "global_roi_width") ? 1 : 0;
  applied += applyIntToken("roi_height", "global_roi_height") ? 1 : 0;
  applied += applyIntToken("roi_w", "global_roi_width") ? 1 : 0;
  applied += applyIntToken("roi_h", "global_roi_height") ? 1 : 0;

  applied += applyIntToken("circle_cx", "global_circle_cx") ? 1 : 0;
  applied += applyIntToken("circle_cy", "global_circle_cy") ? 1 : 0;
  applied += applyIntToken("circle_px", "global_circle_px") ? 1 : 0;
  applied += applyIntToken("circle_py", "global_circle_py") ? 1 : 0;
  applied += applyIntToken("cx", "global_circle_cx") ? 1 : 0;
  applied += applyIntToken("cy", "global_circle_cy") ? 1 : 0;
  applied += applyIntToken("px", "global_circle_px") ? 1 : 0;
  applied += applyIntToken("py", "global_circle_py") ? 1 : 0;
  applied += applyIntToken("circle_inner_radius", "global_circle_inner_radius")
                 ? 1
                 : 0;
  applied += applyIntToken("circle_outer_radius", "global_circle_outer_radius")
                 ? 1
                 : 0;
  applied +=
      applyIntToken("circle_ring_width", "global_circle_ring_width") ? 1 : 0;
  applied +=
      applyIntToken("inner_radius", "global_circle_inner_radius") ? 1 : 0;
  applied +=
      applyIntToken("outer_radius", "global_circle_outer_radius") ? 1 : 0;
  applied +=
      applyIntToken("findcircle_findsetting", "global_findcircle_findsetting")
          ? 1
          : 0;
  applied +=
      applyIntToken("circle_findsetting", "global_findcircle_findsetting") ? 1
                                                                            : 0;
  applied +=
      applyIntToken("findcircle_edge_count", "global_findcircle_edge_count") ? 1
                                                                             : 0;
  applied +=
      applyIntToken("circle_edge_count", "global_findcircle_edge_count") ? 1
                                                                         : 0;
  applied += applyIntToken("findcircle_selected_edge",
                           "global_findcircle_selected_edge")
                 ? 1
                 : 0;
  applied +=
      applyIntToken("circle_selected_edge", "global_findcircle_selected_edge")
          ? 1
          : 0;
  applied +=
      applyIntToken("findcircle_best_edge", "global_findcircle_best_edge") ? 1
                                                                           : 0;
  applied += applyIntToken("circle_best_edge", "global_findcircle_best_edge")
                 ? 1
                 : 0;
  applied += applyIntToken("findcircle_recommended_edge",
                           "global_findcircle_recommended_edge")
                 ? 1
                 : 0;
  applied += applyIntToken("circle_recommended_edge",
                           "global_findcircle_recommended_edge")
                 ? 1
                 : 0;
  applied +=
      applyIntToken("findcircle_relation_edge", "global_findcircle_relation_edge")
          ? 1
          : 0;
  applied +=
      applyIntToken("circle_relation_edge", "global_findcircle_relation_edge")
          ? 1
          : 0;
  applied +=
      applyIntToken("findcircle_attach_edge", "global_findcircle_attach_edge")
          ? 1
          : 0;
  applied += applyIntToken("circle_attach_edge", "global_findcircle_attach_edge")
                 ? 1
                 : 0;
  applied +=
      applyIntToken("findcircle_arc_enabled", "global_findcircle_arc_enabled")
          ? 1
          : 0;
  applied +=
      applyIntToken("circle_arc_enabled", "global_findcircle_arc_enabled") ? 1
                                                                           : 0;
  applied += applyIntToken("findcircle_arc_start_deg",
                           "global_findcircle_arc_start_deg")
                 ? 1
                 : 0;
  applied += applyIntToken("circle_arc_start_deg",
                           "global_findcircle_arc_start_deg")
                 ? 1
                 : 0;
  applied +=
      applyIntToken("findcircle_arc_end_deg", "global_findcircle_arc_end_deg")
          ? 1
          : 0;
  applied +=
      applyIntToken("circle_arc_end_deg", "global_findcircle_arc_end_deg") ? 1
                                                                           : 0;
  applied += applyIntToken("findcircle_point_consistency_enabled",
                           "global_findcircle_point_consistency_enabled")
                 ? 1
                 : 0;
  applied += applyIntToken("circle_point_consistency_enabled",
                           "global_findcircle_point_consistency_enabled")
                 ? 1
                 : 0;
  applied += applyIntToken("findcircle_point_consistency_range",
                           "global_findcircle_point_consistency_range")
                 ? 1
                 : 0;
  applied += applyIntToken("circle_point_consistency_range",
                           "global_findcircle_point_consistency_range")
                 ? 1
                 : 0;

  applied += applyIntToken("ellipse_x0", "global_ellipse_x0") ? 1 : 0;
  applied += applyIntToken("ellipse_y0", "global_ellipse_y0") ? 1 : 0;
  applied += applyIntToken("ellipse_x1", "global_ellipse_x1") ? 1 : 0;
  applied += applyIntToken("ellipse_y1", "global_ellipse_y1") ? 1 : 0;
  applied += applyIntToken("ellipse_inner_scale_percent",
                           "global_findellipse_inner_scale_percent")
                 ? 1
                 : 0;
  applied += applyIntToken("inner_scale_percent",
                           "global_findellipse_inner_scale_percent")
                 ? 1
                 : 0;
  applied +=
      applyIntToken("findellipse_findsetting", "global_findellipse_findsetting")
          ? 1
          : 0;
  applied +=
      applyIntToken("ellipse_findsetting", "global_findellipse_findsetting")
          ? 1
          : 0;
  applied +=
      applyIntToken("findellipse_edge_count", "global_findellipse_edge_count")
          ? 1
          : 0;
  applied +=
      applyIntToken("ellipse_edge_count", "global_findellipse_edge_count") ? 1
                                                                           : 0;
  applied += applyIntToken("findellipse_selected_edge",
                           "global_findellipse_selected_edge")
                 ? 1
                 : 0;
  applied +=
      applyIntToken("ellipse_selected_edge", "global_findellipse_selected_edge")
          ? 1
          : 0;
  applied +=
      applyIntToken("findellipse_best_edge", "global_findellipse_best_edge")
          ? 1
          : 0;
  applied += applyIntToken("ellipse_best_edge", "global_findellipse_best_edge")
                 ? 1
                 : 0;
  applied += applyIntToken("findellipse_recommended_edge",
                           "global_findellipse_recommended_edge")
                 ? 1
                 : 0;
  applied += applyIntToken("ellipse_recommended_edge",
                           "global_findellipse_recommended_edge")
                 ? 1
                 : 0;
  applied += applyIntToken("findellipse_relation_edge",
                           "global_findellipse_relation_edge")
                 ? 1
                 : 0;
  applied +=
      applyIntToken("ellipse_relation_edge", "global_findellipse_relation_edge")
          ? 1
          : 0;
  applied +=
      applyIntToken("findellipse_attach_edge", "global_findellipse_attach_edge")
          ? 1
          : 0;
  applied +=
      applyIntToken("ellipse_attach_edge", "global_findellipse_attach_edge")
          ? 1
          : 0;
  applied += applyIntToken("findellipse_point_consistency_enabled",
                           "global_findellipse_point_consistency_enabled")
                 ? 1
                 : 0;
  applied += applyIntToken("ellipse_point_consistency_enabled",
                           "global_findellipse_point_consistency_enabled")
                 ? 1
                 : 0;
  applied += applyIntToken("findellipse_point_consistency_range",
                           "global_findellipse_point_consistency_range")
                 ? 1
                 : 0;
  applied += applyIntToken("ellipse_point_consistency_range",
                           "global_findellipse_point_consistency_range")
                 ? 1
                 : 0;

  applied += applyIntToken("torch_input_width", "global_torch_input_width")
                 ? 1
                 : 0;
  applied += applyIntToken("torch_input_height", "global_torch_input_height")
                 ? 1
                 : 0;
  applied += applyIntToken("torch_confidence_percent",
                           "global_torch_confidence_percent")
                 ? 1
                 : 0;
  applied += applyIntToken("torch_iou_threshold_percent",
                           "global_torch_iou_threshold_percent")
                 ? 1
                 : 0;
  applied += applyIntToken("torch_mask_threshold_percent",
                           "global_torch_mask_threshold_percent")
                 ? 1
                 : 0;
  applied += applyIntToken("torch_max_detections",
                           "global_torch_max_detections")
                 ? 1
                 : 0;
  applied += applyIntToken("torch_num_classes", "global_torch_num_classes")
                 ? 1
                 : 0;
  applied += applyIntToken("torch_training_sample_count",
                           "global_torch_training_sample_count")
                 ? 1
                 : 0;
  applied += applyIntToken("torch_training_instance_count",
                           "global_torch_training_instance_count")
                 ? 1
                 : 0;
  applied += applyIntToken("torch_training_step_executed",
                           "global_torch_training_step_executed")
                 ? 1
                 : 0;
  applied += applyIntToken("torch_inference_ok", "global_torch_ok") ? 1 : 0;
  applied += applyIntToken("torch_result_count", "global_torch_result_count")
                 ? 1
                 : 0;
  applied += applyIntToken("torch_result_count_delta",
                           "global_torch_result_count_delta")
                 ? 1
                 : 0;
  applied += applyIntToken("torch_roi_shift_dx_px",
                           "global_torch_roi_shift_dx_px")
                 ? 1
                 : 0;
  applied += applyIntToken("torch_roi_shift_dy_px",
                           "global_torch_roi_shift_dy_px")
                 ? 1
                 : 0;

  applied += applyIntToken("learn_roi_x", "global_learn_roi_x") ? 1 : 0;
  applied += applyIntToken("learn_roi_y", "global_learn_roi_y") ? 1 : 0;
  applied += applyIntToken("learn_roi_w", "global_learn_roi_w") ? 1 : 0;
  applied += applyIntToken("learn_roi_h", "global_learn_roi_h") ? 1 : 0;
  applied += applyIntToken("search_roi_x", "global_search_roi_x") ? 1 : 0;
  applied += applyIntToken("search_roi_y", "global_search_roi_y") ? 1 : 0;
  applied += applyIntToken("search_roi_w", "global_search_roi_w") ? 1 : 0;
  applied += applyIntToken("search_roi_h", "global_search_roi_h") ? 1 : 0;

  applied += applyRectToken("learn_roi_xywh", "global_learn_roi_x",
                            "global_learn_roi_y", "global_learn_roi_w",
                            "global_learn_roi_h")
                 ? 4
                 : 0;
  applied += applyRectToken("search_roi_xywh", "global_search_roi_x",
                            "global_search_roi_y", "global_search_roi_w",
                            "global_search_roi_h")
                 ? 4
                 : 0;

  if (applied == 0) {
    reason =
        supportedTokenPresent
            ? "supported key=value token has an invalid value in parameter "
              "summary: " +
                  parameterSummary
            : "no supported key=value token found in parameter summary: " +
                  parameterSummary;
    return false;
  }

  reason = "applied " + std::to_string(applied) +
           " runtime globals from parameter summary";
  return true;
}

bool ViewController::ResolveEvidenceSelfTestSnapshot(
    const CxEvidenceSelfTestRequest &request,
    CxEvidenceSelectionSnapshot &snapshot, std::string &reason) {
  reason.clear();
  snapshot = CxEvidenceSelectionSnapshot{};

  EnsureCxScriptWorkbenchAssetsLoaded();

  if (request.group_index >= 0 && request.thumb_index >= 0) {
    if (request.group_index >=
        static_cast<int>(m_manualTest.script_evidence_groups.size())) {
      reason = "selftest group_index out of range";
      return false;
    }

    const ScriptEvidenceGroup &group =
        m_manualTest.script_evidence_groups[request.group_index];

    if (request.thumb_index >= static_cast<int>(group.thumbs.size())) {
      reason = "selftest thumb_index out of range";
      return false;
    }

    return BuildEvidenceSnapshotFromThumb(
        request.group_index, request.thumb_index,
        group.thumbs[request.thumb_index], snapshot, reason);
  }

  if (!request.script_path.empty() || !request.image_path.empty() ||
      !request.parameter_summary.empty()) {
    std::string scriptPath = request.script_path;
    if (scriptPath.empty() && !request.script_id.empty())
      scriptPath = ResolveCatalogScriptPathById(request.script_id);

    if (scriptPath.empty()) {
      reason = "synthetic evidence request has empty script_path";
      return false;
    }

    snapshot.valid = true;
    snapshot.case_id = request.case_id;
    snapshot.script_id =
        request.script_id.empty() ? scriptPath : request.script_id;
    snapshot.script_path = scriptPath;
    snapshot.image_id = request.image_id;
    snapshot.image_path = request.image_path;
    snapshot.target_id = request.target_id;
    snapshot.tool = request.tool;
    snapshot.parameter_summary = request.parameter_summary;
    snapshot.parameter_profile_id = request.parameter_summary;
    snapshot.status = "synthetic";
    snapshot.reason = "synthetic evidence lock pipeline case";
    snapshot.source = "evidence_lock_pipeline";
    EnrichSemanticEvidenceSnapshotEditableObjectsLocal(snapshot);
    return true;
  }

  if (!request.script_id.empty()) {
    for (int gi = 0;
         gi < static_cast<int>(m_manualTest.script_evidence_groups.size());
         ++gi) {
      const ScriptEvidenceGroup &group =
          m_manualTest.script_evidence_groups[gi];

      for (int ti = 0; ti < static_cast<int>(group.thumbs.size()); ++ti) {
        const ScriptEvidenceThumb &thumb = group.thumbs[ti];

        if (thumb.script_id != request.script_id)
          continue;

        if (!request.image_id.empty() && thumb.image_id != request.image_id)
          continue;

        if (!request.target_id.empty() && thumb.target_id != request.target_id)
          continue;

        return BuildEvidenceSnapshotFromThumb(gi, ti, thumb, snapshot, reason);
      }
    }

    reason = "no evidence thumb matched request script/image/target";
    return false;
  }

  return GetSelectedEvidenceSnapshot(snapshot, reason);
}

bool ViewController::CheckEvidenceSelfTestImageBinding(
    const CxEvidenceSelectionSnapshot &snapshot, std::string &reason) const {
  reason.clear();

  if (snapshot.image_path.empty()) {
    reason = "evidence image_path is empty";
    return false;
  }

  if (!std::filesystem::exists(snapshot.image_path)) {
    reason = "evidence image file not found: " + snapshot.image_path;
    return false;
  }

  if (m_imageViewImage.empty()) {
    reason = "Image View image is empty after evidence load";
    return false;
  }

  if (m_manualTest.image_file_path.empty()) {
    reason = "manual context image_file_path is empty after evidence load";
    return false;
  }

  reason = "image binding ok: " + snapshot.image_path;
  return true;
}

bool ViewController::CheckEvidenceSelfTestParamBinding(
    const CxEvidenceSelectionSnapshot &snapshot, std::string &reason) const {
  reason.clear();

  auto hasInt = [&](const char *key) -> bool {
    return m_manualTest.runtime_int_vars.find(key) !=
           m_manualTest.runtime_int_vars.end();
  };

  bool isFindLine = false;
  bool isFindCircle = false;
  bool isFindEllipse = false;
  evidenceResolveGeometryToolFlags(snapshot.tool + " " +
                                       snapshot.primary_object_type,
                                   snapshot.script_path, isFindLine,
                                   isFindCircle, isFindEllipse);

  if (isFindCircle) {
    if (!hasInt("global_circle_cx") || !hasInt("global_circle_cy") ||
        !hasInt("global_circle_px") || !hasInt("global_circle_py") ||
        !hasInt("global_gap") || !hasInt("global_linegap") ||
        !hasInt("global_min_edge_run_width_px") ||
        !hasInt("global_threshold") || !hasInt("global_method")) {
      reason = "missing required FindCircle global_* bindings";
      return false;
    }

    reason = "FindCircle parameter globals available";
    return true;
  }

  if (isFindLine) {
    if (!hasInt("global_roi_x0") || !hasInt("global_roi_y0") ||
        !hasInt("global_roi_x1") || !hasInt("global_roi_y1") ||
        !hasInt("global_tool_half_width") || !hasInt("global_wgap") ||
        !hasInt("global_hgap") || !hasInt("global_linegap") ||
        !hasInt("global_threshold") || !hasInt("global_method")) {
      reason = "missing required Findline global_* bindings";
      return false;
    }

    reason = "Findline parameter globals available";
    return true;
  }

  if (isFindEllipse) {
    if (!hasInt("global_ellipse_x0") || !hasInt("global_ellipse_y0") ||
        !hasInt("global_ellipse_x1") || !hasInt("global_ellipse_y1") ||
        !hasInt("global_findellipse_inner_scale_percent") ||
        !hasInt("global_gap") || !hasInt("global_linegap") ||
        !hasInt("global_min_edge_run_width_px") ||
        !hasInt("global_threshold") || !hasInt("global_method")) {
      reason = "missing required FindEllipse global_* bindings";
      return false;
    }

    reason = "FindEllipse parameter globals available";
    return true;
  }

  if (evidencePathIsFindRect(snapshot.script_path)) {
    if (!hasInt("global_roi_x") || !hasInt("global_roi_y") ||
        !hasInt("global_roi_width") || !hasInt("global_roi_height") ||
        !hasInt("global_gauge") || !hasInt("global_linegap") ||
        !hasInt("global_threshold") || !hasInt("global_method")) {
      reason = "missing required FindRect global_* bindings";
      return false;
    }

    reason = "FindRect parameter globals available";
    return true;
  }

  if (evidencePathIsFindObject(snapshot.script_path)) {
    if (!hasInt("global_roi_x0") || !hasInt("global_roi_y0") ||
        !hasInt("global_roi_x1") || !hasInt("global_roi_y1") ||
        !hasInt("global_gap") || !hasInt("global_threshold") ||
        !hasInt("global_method") || !hasInt("global_filterprofile")) {
      reason = "missing required FindObject global_* bindings";
      return false;
    }

    reason = "FindObject parameter globals available";
    return true;
  }

  if (evidencePathIsFindSegmentation(snapshot.script_path)) {
    if (!hasInt("global_roi_x0") || !hasInt("global_roi_y0") ||
        !hasInt("global_roi_x1") || !hasInt("global_roi_y1")) {
      reason = "missing required FindSegmentation global_* bindings";
      return false;
    }

    reason = "FindSegmentation parameter globals available";
    return true;
  }

  if (evidencePathIsGridPatternClass(snapshot.script_path)) {
    const std::vector<std::string> required = {
        "global_learn_roi_x",
        "global_learn_roi_y",
        "global_learn_roi_w",
        "global_learn_roi_h",
        "global_grid_normalized_width",
        "global_grid_normalized_height",
        "global_grid_rows",
        "global_grid_cols",
        "global_grid_levels",
        "global_grid_orientation_bins",
        "global_grid_foreground_threshold",
        "global_grid_active_foreground_percent",
        "global_grid_active_edge_percent",
        "global_grid_max_overlays",
        "global_grid_fusion_mode"};
    for (const std::string &key : required) {
      if (!hasInt(key.c_str())) {
        reason = "missing required GridPattern global: " + key;
        return false;
      }
    }
    if (m_manualTest.runtime_int_vars.at("global_grid_levels") < 3 ||
        m_manualTest.runtime_int_vars.at("global_grid_levels") > 5) {
      reason = "GridPattern pooled levels must be in [3,5]";
      return false;
    }
    reason = "GridPattern parameter globals available";
    return true;
  }

  if (evidencePathIsRegionPattern(snapshot.script_path)) {
    const std::vector<std::string> required = {
        "global_region_roi_x",
        "global_region_roi_y",
        "global_region_roi_w",
        "global_region_roi_h",
        "global_region_normalized_width",
        "global_region_normalized_height",
        "global_region_pooling_rows",
        "global_region_pooling_cols",
        "global_region_use_binary",
        "global_region_threshold",
        "global_region_foreground_dark",
        "global_region_max_overlays"};
    for (const std::string &key : required) {
      if (!hasInt(key.c_str())) {
        reason = "missing required RegionPattern global: " + key;
        return false;
      }
    }
    if (m_manualTest.runtime_int_vars.at("global_region_pooling_rows") <= 0 ||
        m_manualTest.runtime_int_vars.at("global_region_pooling_cols") <= 0) {
      reason = "RegionPattern pooling rows/cols must be positive";
      return false;
    }
    reason = "RegionPattern parameter globals available";
    return true;
  }

  if (evidencePathIsFastMatch(snapshot.script_path)) {
    if (!hasInt("global_learn_roi_x") || !hasInt("global_learn_roi_y") ||
        !hasInt("global_learn_roi_w") || !hasInt("global_learn_roi_h") ||
        !hasInt("global_search_roi_x") || !hasInt("global_search_roi_y") ||
        !hasInt("global_search_roi_w") || !hasInt("global_search_roi_h") ||
        !hasInt("global_threshold") || !hasInt("global_wgap") ||
        !hasInt("global_hgap") || !hasInt("global_linegap") ||
        !hasInt("global_compare_gap") || !hasInt("global_objfilter") ||
        !hasInt("global_find_num")) {
      reason = "missing required FastMatch global_* bindings";
      return false;
    }

    const int learn_w = m_manualTest.runtime_int_vars.at("global_learn_roi_w");
    const int learn_h = m_manualTest.runtime_int_vars.at("global_learn_roi_h");
    const int search_w =
        m_manualTest.runtime_int_vars.at("global_search_roi_w");
    const int search_h =
        m_manualTest.runtime_int_vars.at("global_search_roi_h");

    if (learn_w <= 0 || learn_h <= 0) {
      reason = "FastMatch learn ROI must have positive width and height";
      return false;
    }

    if (search_w <= 0 || search_h <= 0) {
      reason = "FastMatch search ROI must have positive width and height";
      return false;
    }

    if (search_w <= learn_w || search_h <= learn_h) {
      reason = "FastMatch search ROI must be larger than learn ROI: learn=" +
               std::to_string(learn_w) + "x" + std::to_string(learn_h) +
               " search=" + std::to_string(search_w) + "x" +
               std::to_string(search_h);
      return false;
    }

    reason = "FastMatch parameter globals available; search ROI larger than "
             "learn ROI";
    return true;
  }

  if (snapshot.parameter_summary.empty() || snapshot.parameter_summary == "-") {
    reason = "parameter summary is empty; defaults may be present but evidence "
             "param binding is absent";
    return false;
  }

  reason = "generic parameter summary available";
  return true;
}

bool ViewController::RunEvidenceChainSelfTest(
    const CxEvidenceSelfTestRequest &request, CxEvidenceSelfTestResult &result,
    std::string &reason) {
  reason.clear();
  result = CxEvidenceSelfTestResult{};

  result.run_id = request.run_id;
  result.case_id =
      request.case_id.empty() ? "evidence_selftest" : request.case_id;

  result.executed = true;
  result.final_status = "RUNNING";
  result.final_code = "EVIDENCE_SELFTEST_RUNNING";

  CxEvidenceSelectionSnapshot snapshot;
  std::string stepReason;

  if (!ResolveEvidenceSelfTestSnapshot(request, snapshot, stepReason)) {
    AddEvidenceSelfTestStep(result, "EVIDENCE_SELECTION_FAIL", "FAIL",
                            stepReason);

    result.final_code = "EVIDENCE_SELECTION_FAIL";
    result.final_status = "FAIL";
    result.final_reason = stepReason;
    reason = stepReason;
    return false;
  }

  result.script_id = snapshot.script_id;
  result.script_path = snapshot.script_path;
  result.image_id = snapshot.image_id;
  result.image_path = snapshot.image_path;
  result.target_id = snapshot.target_id;
  result.tool = snapshot.tool;
  result.primary_object_type = snapshot.primary_object_type;
  result.primary_object_name = snapshot.primary_object_name;
  result.primary_object_status = snapshot.primary_object_status;
  result.editable_object_count =
      static_cast<int>(snapshot.editable_objects.size());
  const std::string inferredTool =
      inferEvidenceToolFromScriptPath(snapshot.script_path);
  if (!inferredTool.empty() &&
      (result.tool.empty() || result.tool == "module" ||
       result.tool == "integration")) {
    result.tool = inferredTool;
  }
  result.parameter_summary = snapshot.parameter_summary;

  AddEvidenceSelfTestStep(result, "EVIDENCE_SELECTION_PASS", "PASS",
                          "snapshot resolved from evidence chain");

  if (!ApplyEvidenceSelectionSnapshotToManualContext(snapshot, false,
                                                     stepReason)) {
    AddEvidenceSelfTestStep(result, "EVIDENCE_CONTEXT_APPLY_FAIL", "FAIL",
                            stepReason);

    result.final_code = "EVIDENCE_CONTEXT_APPLY_FAIL";
    result.final_status = "FAIL";
    result.final_reason = stepReason;
    reason = stepReason;
    return false;
  }

  if (!LoadImageForEvidenceSelfTest(snapshot.image_path, stepReason)) {
    AddEvidenceSelfTestStep(result, "IMAGE_BINDING_FAIL", "FAIL", stepReason);

    result.final_code = "IMAGE_BINDING_FAIL";
    result.final_status = "FAIL";
    result.final_reason = stepReason;
    reason = stepReason;
    return false;
  }

  if (!CheckEvidenceSelfTestImageBinding(snapshot, stepReason)) {
    AddEvidenceSelfTestStep(result, "IMAGE_BINDING_FAIL", "FAIL", stepReason);

    result.final_code = "IMAGE_BINDING_FAIL";
    result.final_status = "FAIL";
    result.final_reason = stepReason;
    reason = stepReason;
    return false;
  }

  AddEvidenceSelfTestStep(result, "IMAGE_BINDING_PASS", "PASS", stepReason);

  m_manualTest.runtime_int_vars.clear();

  std::string paramApplyReason;
  if (!evidenceSnapshotHasLockedParamSummary(snapshot, paramApplyReason)) {
    AddEvidenceSelfTestStep(result, "PARAM_BINDING_FAIL", "FAIL",
                            paramApplyReason);

    result.final_code = "PARAM_BINDING_FAIL";
    result.final_status = "FAIL";
    result.final_reason = paramApplyReason;
    reason = paramApplyReason;
    return false;
  }

  if (!ApplyEvidenceParameterSummaryToRuntimeGlobals(snapshot.parameter_summary,
                                                     paramApplyReason)) {
    AddEvidenceSelfTestStep(result, "PARAM_BINDING_FAIL", "FAIL",
                            paramApplyReason);

    result.final_code = "PARAM_BINDING_FAIL";
    result.final_status = "FAIL";
    result.final_reason = paramApplyReason;
    reason = paramApplyReason;
    return false;
  }

  syncEvidenceLockedGlobalsToManualGauge(
      m_manualTest, snapshot.script_path,
      snapshot.tool + " " + snapshot.primary_object_type,
      "evidence_selftest_locked");

  std::string scriptTextForGlobals;
  std::string scriptGlobalReason;
  if (ReadTextFile(snapshot.script_path, scriptTextForGlobals)) {
    const int imageWidth = m_imageViewImage.empty() ? 0 : m_imageViewImage.cols;
    const int imageHeight =
        m_imageViewImage.empty() ? 0 : m_imageViewImage.rows;
    if (!stageEvidenceSelfTestScriptGlobals(
            m_parserDebugBridge, m_manualTest.runtime_int_vars,
            scriptTextForGlobals, snapshot.parameter_summary, imageWidth,
            imageHeight, false, scriptGlobalReason)) {
      AddEvidenceSelfTestStep(result, "PARAM_BINDING_FAIL", "FAIL",
                              scriptGlobalReason);

      result.final_code = "PARAM_BINDING_FAIL";
      result.final_status = "FAIL";
      result.final_reason = scriptGlobalReason;
      reason = scriptGlobalReason;
      return false;
    } else {
      paramApplyReason += " | " + scriptGlobalReason;
    }

    auto stageTorchOutputInt = [&](const char *name) {
      if (scriptContainsIdentifier(scriptTextForGlobals, name) &&
          m_manualTest.runtime_int_vars.find(name) ==
              m_manualTest.runtime_int_vars.end()) {
        m_manualTest.runtime_int_vars[name] = 0;
      }
    };

    auto stageTorchOutputDouble = [&](const char *name) {
      if (scriptContainsIdentifier(scriptTextForGlobals, name)) {
        m_parserDebugBridge.SetGlobalDouble(name, 0.0);
      }
    };

    stageTorchOutputInt("global_headless_ok");
    stageTorchOutputInt("global_torch_ok");
    stageTorchOutputInt("global_torch_mask_available");
    stageTorchOutputInt("global_torch_result_count");
    stageTorchOutputInt("global_torch_error_code");
    stageTorchOutputDouble("global_torch_infer_ms");
    stageTorchOutputDouble("global_torch_train_ms");
    stageTorchOutputDouble("global_torch_total_ms");
  } else {
    paramApplyReason +=
        " | script global staging skipped: script file could not be read";
  }

  if (!CheckEvidenceSelfTestParamBinding(snapshot, stepReason)) {
    AddEvidenceSelfTestStep(result, "PARAM_BINDING_FAIL", "FAIL", stepReason);

    result.final_code = "PARAM_BINDING_FAIL";
    result.final_status = "FAIL";
    result.final_reason = stepReason;
    reason = stepReason;
    return false;
  }

  AddEvidenceSelfTestStep(result, "PARAM_BINDING_PASS", "PASS",
                          stepReason + " | " + paramApplyReason);

  if (!RunEvidenceSelfTestParserCompileStage(snapshot, result, stepReason)) {
    AddEvidenceSelfTestStep(result, "SCRIPT_COMPILE_FAIL", "FAIL", stepReason);

    result.final_code = "SCRIPT_COMPILE_FAIL";
    result.final_status = "FAIL";
    result.final_reason = stepReason;
    reason = stepReason;
    return false;
  }

  AddEvidenceSelfTestStep(result, "SCRIPT_COMPILE_PASS", "PASS",
                          "script accepted by compile-only parser path");

  AddEvidenceSelfTestStep(result, "GLOBAL_INJECTION_PASS", "PASS", stepReason);

  result.parser_binding_ok = true;

  if (!RunEvidenceSelfTestRuntimeExecuteStage(snapshot, result, stepReason)) {
    AddEvidenceSelfTestStep(result, "RUNTIME_EXECUTE_FAIL", "FAIL", stepReason);

    result.final_code = "EVIDENCE_SELFTEST_L2_RUNTIME_EXECUTE_FAIL";
    result.final_status = "FAIL";
    result.final_reason = stepReason;
    reason = stepReason;
    return false;
  }

  AddEvidenceSelfTestStep(result, "RUNTIME_EXECUTE_PASS", "PASS", stepReason);

  if (!CheckEvidenceSelfTestRuntimeObjectStage(snapshot, result, stepReason)) {
    if (result.runtime_status == "primary_object_selection_required") {
      AddEvidenceSelfTestStep(result, "PRIMARY_OBJECT_SELECTION_PENDING",
                              "PENDING", stepReason);

      result.final_code = "PRIMARY_OBJECT_SELECTION_PENDING";
      result.final_status = "PENDING_HUMAN_REVIEW";
      result.final_reason = stepReason;
      result.passed_to_human_review = true;
      reason = stepReason;
      return true;
    }

    AddEvidenceSelfTestStep(result, "RUNTIME_OBJECT_FAIL", "FAIL", stepReason);

    result.final_code = "EVIDENCE_SELFTEST_L2_RUNTIME_OBJECT_FAIL";
    result.final_status = "FAIL";
    result.final_reason = stepReason;
    reason = stepReason;
    return false;
  }

  AddEvidenceSelfTestStep(result, "RUNTIME_OBJECT_PASS", "PASS", stepReason);

  for (const RuntimeObjectView &object : m_manualTest.runtime_objects) {
    if (object.name == result.runtime_object_name &&
        object.type == result.runtime_object_type) {
      CopyRuntimeObjectDiagnosticsToEvidenceResult(object, result);
      break;
    }
  }

  if (result.runtime_object_type == "TorchTask" && result.torch_ok == 0 &&
      snapshot.script_path.find("edgesam") != std::string::npos &&
      result.algorithm_reason.find("encoder or decoder is missing") !=
          std::string::npos) {
    AddEvidenceSelfTestStep(
        result, "MODEL_BINDING_PENDING", "PENDING",
        "EdgeSAM split TorchScript encoder/decoder binding is missing");
    result.final_code = "PENDING_BINDING";
    result.final_status = "PENDING_BINDING";
    result.final_reason =
        "EdgeSAM requires exported encoder.ts and decoder.ts; ONNX runtime "
        "fallback is disabled.";
    result.passed_to_human_review = false;
    reason = result.final_reason;
    return true;
  }

  if (!RunEvidenceSelfTestProjectionStage(snapshot, result, stepReason)) {
    AddEvidenceSelfTestStep(result, "GAUGE_PROJECTION_FAIL", "FAIL",
                            stepReason);

    result.final_code = "EVIDENCE_SELFTEST_L2_GAUGE_PROJECTION_FAIL";
    result.final_status = "FAIL";
    result.final_reason = stepReason;
    reason = stepReason;
    return false;
  }

  AddEvidenceSelfTestStep(result, "GAUGE_PROJECTION_PASS", "PASS", stepReason);

  if (!CheckEvidenceSelfTestResultProjectionStage(snapshot, result,
                                                  stepReason)) {
    AddEvidenceSelfTestStep(result, "RESULT_PROJECTION_PENDING", "PENDING",
                            stepReason);

    result.final_code = "RESULT_PROJECTION_PENDING";
    result.final_status = "PENDING_HUMAN_REVIEW";
    result.final_reason = stepReason;
    result.passed_to_human_review = true;
    reason = stepReason;
    return true;
  }

  AddEvidenceSelfTestStep(result, "RESULT_PROJECTION_PASS", "PASS", stepReason);

  result.final_code = "EVIDENCE_SELFTEST_L2_RUNTIME_PROJECTION_PASS";
  result.final_status = "PENDING_HUMAN_REVIEW";
  result.final_reason =
      "Evidence chain runtime object, gauge projection and result projection "
      "are available; pending human final review.";
  result.passed_to_human_review = true;

  reason = result.final_reason;
  return true;
}

bool ViewController::RunEvidenceSelfTestParserCompileStage(
    const CxEvidenceSelectionSnapshot &snapshot,
    CxEvidenceSelfTestResult &result, std::string &reason) {
  reason.clear();

  if (snapshot.script_path.empty()) {
    reason = "snapshot script_path is empty";
    return false;
  }

  std::string scriptText;
  if (!ReadTextFile(snapshot.script_path, scriptText)) {
    reason = "failed to read script for parser compile stage: " +
             snapshot.script_path;
    return false;
  }

  for (const auto &input : m_manualTest.runtime_int_vars) {
    if (input.first.rfind("global_", 0) == 0)
      m_parserDebugBridge.SetGlobalInt(input.first, input.second);
  }

  bool imageBound = true;
  if (!m_imageViewImage.empty())
    imageBound = m_parserDebugBridge.StageGlobalMatInput(m_imageViewImage);
  else if (!s_img0.empty())
    imageBound = m_parserDebugBridge.StageGlobalMatInput(s_img0);
  else
    imageBound = false;

  if (!imageBound) {
    reason = "failed to stage global_matInput for parser compile stage";
    return false;
  }

  if (!m_parserDebugBridge.CompileScriptOnly(scriptText)) {
    reason = "compile-only rejected script: " + m_parserDebugBridge.LastError();
    return false;
  }

  std::string injectionReason;
  if (!CheckEvidenceSelfTestGlobalInjection(snapshot, injectionReason)) {
    reason = injectionReason;
    return false;
  }

  reason = "compile-only accepted script; " + injectionReason;
  return true;
}

bool ViewController::CheckEvidenceSelfTestGlobalInjection(
    const CxEvidenceSelectionSnapshot &snapshot, std::string &reason) {
  reason.clear();

  auto hasRuntimeInt = [&](const char *key) -> bool {
    return m_manualTest.runtime_int_vars.find(key) !=
           m_manualTest.runtime_int_vars.end();
  };

  auto hasParserVar = [&](const char *key) -> bool {
    double value = 0.0;
    return m_parserDebugBridge.QueryDouble(key, value);
  };

  std::vector<std::string> required;
  bool isFindLine = false;
  bool isFindCircle = false;
  bool isFindEllipse = false;
  evidenceResolveGeometryToolFlags(snapshot.tool + " " +
                                       snapshot.primary_object_type,
                                   snapshot.script_path, isFindLine,
                                   isFindCircle, isFindEllipse);

  if (isFindCircle) {
    required = {"global_circle_cx", "global_circle_cy", "global_circle_px",
                "global_circle_py", "global_gap",       "global_linegap",
                "global_min_edge_run_width_px",
                "global_threshold", "global_method"};
  } else if (isFindLine) {
    required = {"global_roi_x0", "global_roi_y0",          "global_roi_x1",
                "global_roi_y1", "global_tool_half_width", "global_wgap",
                "global_hgap",   "global_linegap",         "global_threshold",
                "global_method"};
  } else if (isFindEllipse) {
    required = {"global_ellipse_x0",
                "global_ellipse_y0",
                "global_ellipse_x1",
                "global_ellipse_y1",
                "global_findellipse_inner_scale_percent",
                "global_gap",
                "global_linegap",
                "global_min_edge_run_width_px",
                "global_threshold",
                "global_method"};
  } else if (evidencePathIsFindRect(snapshot.script_path)) {
    required = {"global_roi_x",      "global_roi_y", "global_roi_width",
                "global_roi_height", "global_gauge", "global_linegap",
                "global_threshold",  "global_method"};
  } else if (evidencePathIsGridPatternClass(snapshot.script_path)) {
    required = {"global_learn_roi_x",
                "global_learn_roi_y",
                "global_learn_roi_w",
                "global_learn_roi_h",
                "global_grid_normalized_width",
                "global_grid_normalized_height",
                "global_grid_rows",
                "global_grid_cols",
                "global_grid_levels",
                "global_grid_orientation_bins",
                "global_grid_foreground_threshold",
                "global_grid_foreground_dark",
                "global_grid_equalize_contrast",
                "global_grid_active_foreground_percent",
                "global_grid_active_edge_percent",
                "global_grid_max_overlays",
                "global_grid_fusion_mode",
                "global_grid_status",
                "global_grid_active_cell_count",
                "global_grid_descriptor_dim",
                "global_grid_level_count",
                "global_grid_overlay_count",
                "global_grid_overlay_truncated"};
  } else if (evidencePathIsRegionPattern(snapshot.script_path)) {
    required = {"global_region_roi_x",
                "global_region_roi_y",
                "global_region_roi_w",
                "global_region_roi_h",
                "global_region_normalized_width",
                "global_region_normalized_height",
                "global_region_pooling_rows",
                "global_region_pooling_cols",
                "global_region_use_binary",
                "global_region_threshold",
                "global_region_foreground_dark",
                "global_region_max_overlays",
                "global_region_status",
                "global_region_descriptor_dim",
                "global_region_foreground_permille",
                "global_region_mean_permille",
                "global_region_std_permille",
                "global_region_pooling_rows_out",
                "global_region_pooling_cols_out",
                "global_region_overlay_count",
                "global_region_overlay_truncated"};
  } else if (evidencePathIsFastMatch(snapshot.script_path)) {
    required = {
        "global_learn_roi_x",  "global_learn_roi_y",  "global_learn_roi_w",
        "global_learn_roi_h",  "global_search_roi_x", "global_search_roi_y",
        "global_search_roi_w", "global_search_roi_h", "global_threshold",
        "global_wgap",         "global_hgap",         "global_linegap",
        "global_compare_gap",  "global_objfilter",    "global_find_num"};
  } else {
    reason = "generic script has no required global injection rule";
    return true;
  }

  for (const std::string &key : required) {
    if (!hasRuntimeInt(key.c_str())) {
      reason = "runtime_int_vars missing " + key;
      return false;
    }

    if (!hasParserVar(key.c_str())) {
      reason = "parser variable missing " + key;
      return false;
    }
  }

  reason = "required global_* vars are available in runtime and parser";
  return true;
}

bool ViewController::RunEvidenceSelfTestRuntimeExecuteStage(
    const CxEvidenceSelectionSnapshot &snapshot,
    CxEvidenceSelfTestResult &result, std::string &reason) {
  reason.clear();

  if (snapshot.script_path.empty()) {
    reason = "runtime execute rejected: snapshot script_path is empty";
    return false;
  }

  std::string scriptText;
  if (!ReadTextFile(snapshot.script_path, scriptText)) {
    reason = "runtime execute rejected: failed to read script " +
             snapshot.script_path;
    return false;
  }

  for (const auto &input : m_manualTest.runtime_int_vars) {
    if (input.first.rfind("global_", 0) == 0)
      m_parserDebugBridge.SetGlobalInt(input.first, input.second);
  }

  bool imageBound = false;
  if (!m_imageViewImage.empty())
    imageBound = m_parserDebugBridge.StageGlobalMatInput(m_imageViewImage);
  else if (!s_img0.empty())
    imageBound = m_parserDebugBridge.StageGlobalMatInput(s_img0);

  if (!imageBound) {
    reason = "runtime execute rejected: failed to stage global_matInput";
    return false;
  }

  if (!PrepareTorchUiRequestContext(
          scriptText, snapshot.script_path, reason)) {
    reason = "runtime execute rejected: failed to stage Torch request "
             "context: " +
             reason;
    return false;
  }

  m_annotationLayer.RemoveRuntimeOwnersNotIn(std::unordered_set<std::string>{});

  const bool ran = m_parserDebugBridge.RunScript(scriptText);
  result.runtime_executed = ran;

  if (!ran) {
    reason = "runtime execute failed: " + m_parserDebugBridge.LastError();
    return false;
  }

  CxFastMatchRuntimeCapture fastmatch_capture;
  if (snapshot.tool == "FastMatch") {
    CaptureFastMatchRuntime(m_parserDebugBridge, "m_match", fastmatch_capture);

    result.fastmatch_model_point_count =
        fastmatch_capture.object_model_point_count;
    result.fastmatch_learn_a_count = fastmatch_capture.object_learn_a_count;
    result.fastmatch_learn_b_count = fastmatch_capture.object_learn_b_count;
    result.fastmatch_learn_a2_count = fastmatch_capture.object_learn_a2_count;
    result.fastmatch_learn_b2_count = fastmatch_capture.object_learn_b2_count;
    result.fastmatch_learn_status_code =
        fastmatch_capture.object_learn_status_code;
  }

  RefreshRuntimeObjectTable("Evidence SelfTest Runtime Execute",
                            "runtime_executed");

  result.runtime_object_count =
      static_cast<int>(m_manualTest.runtime_objects.size());

  reason = "runtime executed and runtime object table refreshed";
  return true;
}

bool ViewController::CheckEvidenceSelfTestRuntimeObjectStage(
    const CxEvidenceSelectionSnapshot &snapshot,
    CxEvidenceSelfTestResult &result, std::string &reason) {
  reason.clear();

  if (m_manualTest.runtime_objects.empty()) {
    reason = "runtime object check failed: no runtime objects queried";
    return false;
  }

  std::string expectedType;
  bool isFindLine = false;
  bool isFindCircle = false;
  bool isFindEllipse = false;
  evidenceResolveGeometryToolFlags(snapshot.tool + " " +
                                       snapshot.primary_object_type,
                                   snapshot.script_path, isFindLine,
                                   isFindCircle, isFindEllipse);
  if (isFindCircle)
    expectedType = "FindCircle";
  else if (isFindLine)
    expectedType = "FindLine";
  else if (isFindEllipse)
    expectedType = "FindEllipse";
  else if (evidencePathIsFindRect(snapshot.script_path))
    expectedType = "FindRect";
  else if (evidencePathIsGridPatternClass(snapshot.script_path))
    expectedType = "GridPatternClassTool";
  else if (evidencePathIsRegionPattern(snapshot.script_path))
    expectedType = "RegionPatternTool";
  else if (evidencePathIsFastMatch(snapshot.script_path))
    expectedType = "FastMatch";
  else if (evidencePathIsFindSegmentation(snapshot.script_path))
    expectedType = "FindSegmentation";
  else if (snapshot.tool == "TorchTask" ||
           evidencePathIsTorchTask(snapshot.script_path))
    expectedType = "TorchTask";

  if (expectedType.empty()) {
    reason = "runtime object check skipped: generic script has no expected "
             "tool type";
    result.runtime_object_ok = true;
    return true;
  }

  if (snapshot.primary_object_status == "needs_object_selection" ||
      snapshot.primary_object_status ==
          "needs_object_selection_no_tool_match") {
    result.runtime_object_ok = false;
    result.runtime_object_type = expectedType;
    result.runtime_object_name.clear();
    result.runtime_status = "primary_object_selection_required";
    reason = "runtime object check pending: multiple editable objects require "
             "primary object selection";
    return false;
  }

  if (!snapshot.primary_object_name.empty()) {
    const std::string primaryType = snapshot.primary_object_type.empty()
                                        ? expectedType
                                        : snapshot.primary_object_type;
    for (const RuntimeObjectView &object : m_manualTest.runtime_objects) {
      if (object.name == snapshot.primary_object_name &&
          object.type == primaryType) {
        result.runtime_object_ok = true;
        result.runtime_object_type = object.type;
        result.runtime_object_name = object.name;
        result.runtime_status = object.runtime_state;
        reason = "primary runtime object available: " + object.type + " " +
                 object.name + " status=" + snapshot.primary_object_status;
        return true;
      }
    }

    reason = "runtime object check failed: primary object " + primaryType +
             " " + snapshot.primary_object_name + " not found";
    return false;
  }

  for (const RuntimeObjectView &object : m_manualTest.runtime_objects) {
    if (object.type == expectedType) {
      result.runtime_object_ok = true;
      result.runtime_object_type = object.type;
      result.runtime_object_name = object.name;
      result.runtime_status = object.runtime_state;
      reason = "runtime object available: " + object.type + " " + object.name;
      return true;
    }
  }

  reason = "runtime object check failed: expected " + expectedType +
           " but not found";
  return false;
}

bool ViewController::RunEvidenceSelfTestProjectionStage(
    const CxEvidenceSelectionSnapshot &snapshot,
    CxEvidenceSelfTestResult &result, std::string &reason) {
  reason.clear();

  result.shape_element_count_before =
      static_cast<int>(m_annotationLayer.ShapeElements().size());

  RequestRuntimeShapeSync("evidence_selftest_runtime_projection");

  m_runtimeShapeSyncPending = false;
  m_runtimeShapeSyncReason.clear();
  m_runtimeShapeSyncExecuting = true;
  SyncRuntimeObjectsToShapeElements();
  m_runtimeShapeSyncExecuting = false;

  result.shape_element_count_after =
      static_cast<int>(m_annotationLayer.ShapeElements().size());

  result.projected_shape_count =
      result.shape_element_count_after - result.shape_element_count_before;

  if (result.shape_element_count_after <= 0) {
    if ((snapshot.tool == "TorchTask" ||
         evidencePathIsTorchTask(snapshot.script_path)) &&
        result.runtime_object_type == "TorchTask") {
      result.gauge_projection_ok = true;
      result.shape_projection_ok = true;
      result.shape_element_count = 0;
      result.gauge_shape_count = 0;
      result.result_shape_count = 0;
      result.projection_reason =
          "TorchTask artifact evidence projection available; no editable "
          "geometry gauge is required";
      result.gauge_status = "torch_artifact_only";
      reason = result.projection_reason;
      return true;
    }

    reason = "projection failed: annotation layer has no shape elements";
    return false;
  }

  bool hasEditableGauge = false;
  bool hasResultElement = false;
  const bool isFindSegmentationScript =
      evidencePathIsFindSegmentation(snapshot.script_path);
  const bool isGridPatternScript =
      evidencePathIsGridPatternClass(snapshot.script_path);
  const bool isRegionPatternScript =
      evidencePathIsRegionPattern(snapshot.script_path);
  int visibleCount = 0;
  int gaugeCount = 0;
  int resultCount = 0;

  for (const CxShapeElement &element : m_annotationLayer.ShapeElements()) {
    if (!element.visible || !element.shape)
      continue;

    ++visibleCount;

    if (element.editable && !element.result_element) {
      hasEditableGauge = true;
      ++gaugeCount;
    }

    const bool isStandardResult =
        element.result_element && element.semantic_role == "result";
    const bool isSegmentationAttachResult =
        isFindSegmentationScript && element.result_element &&
        (element.semantic_role == "boundary" ||
         element.semantic_role == "boundary_bbox");
    const bool isGridPatternResult =
        isGridPatternScript && element.result_element &&
        (element.semantic_role == "active_grid_cell" ||
         element.semantic_role == "cell_orientation");
    const bool isRegionPatternResult =
        isRegionPatternScript && element.result_element &&
        element.semantic_role == "pooled_region_block";

    if (isStandardResult || isSegmentationAttachResult || isGridPatternResult ||
        isRegionPatternResult) {
      hasResultElement = true;
      ++resultCount;
    }
  }

  result.shape_element_count = visibleCount;
  result.gauge_shape_count = gaugeCount;
  result.result_shape_count = resultCount;

  if (!hasEditableGauge) {
    if (evidencePathIsFindSegmentation(snapshot.script_path) &&
        visibleCount > 0) {
      result.gauge_projection_ok = true;
      result.shape_projection_ok = true;
      reason = "segmentation shape projection available; editable prompt gauge "
               "pending";
      result.projection_reason = reason;
      result.gauge_status = "segmentation_editable_prompt_pending";
      return true;
    }

    reason = "projection failed: no editable gauge/ROI shape found";
    return false;
  }

  result.gauge_projection_ok = true;
  result.shape_projection_ok = true;

  reason = hasResultElement ? "gauge and result shape projection available"
                            : "gauge projection available; result shape "
                              "pending runtime algorithm output";

  result.projection_reason = reason;
  result.gauge_status =
      hasEditableGauge ? "gauge_shape_available" : "gauge_shape_missing";

  return true;
}

bool ViewController::CheckEvidenceSelfTestResultProjectionStage(
    const CxEvidenceSelectionSnapshot &snapshot,
    CxEvidenceSelfTestResult &result, std::string &reason) {
  reason.clear();

  bool isFindLine = false;
  bool isFindCircle = false;
  bool isFindEllipse = false;
  evidenceResolveGeometryToolFlags(snapshot.tool + " " +
                                       snapshot.primary_object_type,
                                   snapshot.script_path, isFindLine,
                                   isFindCircle, isFindEllipse);
  bool requiresResult = isFindCircle || isFindLine || isFindEllipse ||
                        evidencePathIsFindRect(snapshot.script_path) ||
                        evidencePathIsFindObject(snapshot.script_path) ||
                        evidencePathIsFindSegmentation(snapshot.script_path) ||
                        snapshot.tool == "TorchTask" ||
                        evidencePathIsTorchTask(snapshot.script_path) ||
                        evidencePathIsFastMatch(snapshot.script_path);

  if (!requiresResult) {
    result.result_projection_ok = true;
    reason = "generic script does not require geometry result projection";
    return true;
  }

  if (result.result_shape_count <= 0) {
    if ((snapshot.tool == "TorchTask" ||
         evidencePathIsTorchTask(snapshot.script_path)) &&
        result.runtime_object_type == "TorchTask") {
      bool hasTorchArtifactEvidence = false;
      std::string artifactReason;

      for (const RuntimeObjectView &object : m_manualTest.runtime_objects) {
        if (object.name == result.runtime_object_name &&
            object.type == "TorchTask") {
          hasTorchArtifactEvidence =
              object.torch_ok != 0 &&
              (!object.torch_result_ref.empty() ||
               !object.torch_evidence_ref.empty() ||
               !object.torch_primary_visual_ref.empty() ||
               !object.torch_mask_ref.empty() ||
               !object.torch_overlay_ref.empty() ||
               !object.torch_trainer_lifecycle_summary.empty() ||
               !object.torch_unified_mainline_summary.empty());

          artifactReason =
              "TorchTask artifact evidence: status=" + object.torch_status +
              " device=" + object.torch_actual_device +
              " result_ref=" + object.torch_result_ref +
              " evidence_ref=" + object.torch_evidence_ref +
              " primary_visual_ref=" + object.torch_primary_visual_ref +
              " mask_ref=" + object.torch_mask_ref +
              " overlay_ref=" + object.torch_overlay_ref +
              " semantic_quality=NOT_CLAIMED";
          break;
        }
      }

      if (hasTorchArtifactEvidence) {
        result.result_ref = m_manualTest.current_result_ref.value;
        result.evidence_ref = m_scriptResult.evidence_ref;
        result.result_projection_ok = true;
        reason = artifactReason;
        return true;
      }
    }

    reason = "result projection pending: runtime object and gauge shape exist, "
             "but no result shape was published";
    return false;
  }

  result.result_ref = m_manualTest.current_result_ref.source_object;
  result.evidence_ref = m_scriptResult.evidence_ref;
  result.result_projection_ok = true;

  if (m_manualTest.current_result_ref.source_object.empty()) {
    reason = "result shape projection available, but current_result_ref is "
             "empty; result shapes=" +
             std::to_string(result.result_shape_count);
  } else {
    reason = "result projection available: " +
             m_manualTest.current_result_ref.source_object +
             " status=" + m_manualTest.current_result_ref.status;
  }

  return true;
}

bool ViewController::BuildEvidenceSelfTestBatchFromCurrentEvidenceRows(
    CxEvidenceSelfTestBatchRequest &request, std::string &reason) const {
  reason.clear();
  request.cases.clear();

  int count = 0;
  const std::string filter = evidenceLowerAscii(request.tool_filter);
  std::string canonicalToolFilter;
  const bool hasKnownToolFilter =
      evidenceKnownToolFilter(filter, canonicalToolFilter);

  for (std::size_t gi = 0; gi < m_manualTest.script_evidence_groups.size();
       ++gi) {
    const ScriptEvidenceGroup &group = m_manualTest.script_evidence_groups[gi];

    for (std::size_t ti = 0; ti < group.thumbs.size(); ++ti) {
      if (request.max_cases > 0 && count >= request.max_cases)
        break;

      const ScriptEvidenceThumb &thumb = group.thumbs[ti];

      if (thumb.script_path.empty())
        continue;

      if (thumb.image_path.empty())
        continue;

      if (!filter.empty()) {
        if (hasKnownToolFilter) {
          const std::string toolKey =
              group.label + " " + thumb.tool + " " + thumb.primary_object_type;
          if (!evidenceExplicitToolMatches(toolKey, canonicalToolFilter))
            continue;
        } else {
          const std::string key =
              evidenceLowerAscii(group.label + " " + thumb.tool + " " +
                                 thumb.script_id + " " + thumb.script_path +
                                 " " + thumb.status + " " + thumb.reason +
                                 " " + thumb.parameter_summary);
          if (key.find(filter) == std::string::npos)
            continue;
        }
      }

      const std::string toolText =
          group.label + " " + thumb.tool + " " + thumb.primary_object_type;
      const bool explicitFindLine =
          evidenceExplicitToolMatches(toolText, "findline");
      const bool explicitFindCircle =
          evidenceExplicitToolMatches(toolText, "findcircle");
      const bool explicitFindEllipse =
          evidenceExplicitToolMatches(toolText, "findellipse");
      const bool explicitFindRect =
          evidenceExplicitToolMatches(toolText, "findrect");
      const bool explicitFastMatch =
          evidenceExplicitToolMatches(toolText, "fastmatch");
      const bool explicitFindSegmentation =
          evidenceExplicitToolMatches(toolText, "findsegmentation");
      const bool hasExplicitCximageTool =
          explicitFindLine || explicitFindCircle || explicitFindEllipse ||
          explicitFindRect || explicitFastMatch || explicitFindSegmentation;

      auto pathFallback = [&](bool (*pathCheck)(const std::string &)) -> bool {
        if (hasExplicitCximageTool)
          return false;
        return pathCheck(thumb.script_path);
      };

      const bool isTorchTaskCatalogScript = thumb.tool == "TorchTask";
      const bool isFindSegmentationTorchScript =
          (explicitFindSegmentation ||
           pathFallback(evidencePathIsFindSegmentation)) &&
          (thumb.script_path.find("libtorch") != std::string::npos ||
           thumb.script_path.find("edgesam") != std::string::npos);

      const bool isCximageToolScript =
          explicitFindLine || explicitFindCircle || explicitFindEllipse ||
          explicitFindRect || explicitFastMatch || explicitFindSegmentation ||
          pathFallback(evidencePathIsFindLine) ||
          pathFallback(evidencePathIsFindCircle) ||
          pathFallback(evidencePathIsFindEllipse) ||
          pathFallback(evidencePathIsFindRect) ||
          pathFallback(evidencePathIsFastMatch) ||
          pathFallback(evidencePathIsFindSegmentation) ||
          isTorchTaskCatalogScript;
      const bool allowGenericForExplicitToolFilter =
          !filter.empty() && filter.find("torch") == std::string::npos;

      if (!filter.empty() && filter.find("torch") != std::string::npos &&
          !isTorchTaskCatalogScript && !isFindSegmentationTorchScript) {
          continue;
      }

      if (!request.include_generic_scripts &&
          !allowGenericForExplicitToolFilter && !isCximageToolScript)
        continue;

      CxEvidenceSelfTestRequest item;
      item.run_id = request.run_id;
      item.case_id =
          thumb.case_id.empty()
              ? ("evidence_" + std::to_string(gi) + "_" + std::to_string(ti))
              : thumb.case_id;
      item.group_index = static_cast<int>(gi);
      item.thumb_index = static_cast<int>(ti);
      item.script_id = thumb.script_id;
      item.script_path = thumb.script_path;
      item.image_id = thumb.image_id;
      item.image_path = thumb.image_path;
      item.target_id = thumb.target_id;
      item.tool = thumb.tool;
      item.parameter_summary = thumb.parameter_summary;
      item.primary_object_type = thumb.primary_object_type;
      item.primary_object_name = thumb.primary_object_name;
      item.primary_object_status = thumb.primary_object_status;
      item.editable_object_count = thumb.primary_object_name.empty() ? 0 : 1;
      item.out_dir = request.out_dir + "/cases/" + item.case_id;

      request.cases.push_back(item);
      ++count;
    }
  }

  if (request.cases.empty()) {
    reason = "no evidence rows with script_path and image_path";
    return false;
  }

  reason = "built evidence selftest batch cases=" +
           std::to_string(request.cases.size());
  return true;
}

bool ViewController::RunEvidenceSelfTestBatch(
    const CxEvidenceSelfTestBatchRequest &request,
    CxEvidenceSelfTestBatchResult &result, std::string &reason) {
  reason.clear();
  result = CxEvidenceSelfTestBatchResult{};
  result.run_id = request.run_id;
  result.out_dir = request.out_dir;
  result.total_cases = static_cast<int>(request.cases.size());

  for (const CxEvidenceSelfTestRequest &item : request.cases) {
    CxEvidenceSelfTestResult caseResult;
    std::string caseReason;

    const bool ok = RunEvidenceChainSelfTest(item, caseResult, caseReason);

    result.case_results.push_back(caseResult);
    ++result.executed_cases;

    std::string writeReason;
    WriteEvidenceSelfTestSummaryJson(
        caseResult, item.out_dir + "/evidence_selftest_summary.json",
        writeReason);
    WriteEvidenceSelfTestReportMd(
        caseResult, item.out_dir + "/evidence_selftest_report.md", writeReason);

    if (!ok || caseResult.final_status == "FAIL")
      ++result.fail_count;
    else if (caseResult.final_status.find("PENDING") != std::string::npos)
      ++result.pending_count;
    else
      ++result.pass_count;
  }

  if (result.fail_count > 0) {
    result.final_code = "EVIDENCE_SELFTEST_L3_BATCH_RUNNER_FAIL";
    result.final_status = "FAIL";
  } else {
    result.final_code = "EVIDENCE_SELFTEST_L3_BATCH_RUNNER_PASS";
    result.final_status = "PENDING_HUMAN_REVIEW";
  }

  result.final_reason = "batch evidence selftest executed cases=" +
                        std::to_string(result.executed_cases) +
                        " fail=" + std::to_string(result.fail_count) +
                        " pending=" + std::to_string(result.pending_count);

  std::string writeReason;
  WriteEvidenceSelfTestBatchSummaryJson(
      result, request.out_dir + "/evidence_selftest_batch_summary.json",
      writeReason);

  WriteEvidenceSelfTestBatchReportMd(
      result, request.out_dir + "/evidence_selftest_batch_report.md",
      writeReason);

  WriteTorchArtifactAuditReport(result, request.out_dir, writeReason);

  reason = result.final_reason;
  return result.fail_count == 0;
}

void ViewController::HandleSemanticFlowAction(
    const SemanticFlowAction &action) {
  if (action.type == SemanticFlowActionType::None)
    return;

  if (action.type == SemanticFlowActionType::BindCatalogScriptToSelectedNode) {
    CxEvidenceSelectionSnapshot snapshot;
    std::string evidenceReason;

    if (GetSelectedEvidenceSnapshot(snapshot, evidenceReason)) {
      SemanticEvidenceBinding binding = MakeSemanticEvidenceBinding(snapshot);

      std::string reason;
      if (!m_semanticFlowGraph.BindEvidenceToNode(action.node_index, binding,
                                                  reason)) {
        m_scriptResult.source = "semantic_flow";
        m_scriptResult.status = "PENDING";
        m_scriptResult.reason = reason;
        m_scriptResult.runtime_fillback_status = "semantic_bind_blocked";
        return;
      }

      m_scriptResult.source = "semantic_flow";
      m_scriptResult.status = "PENDING";
      m_scriptResult.script_path = binding.script_path;
      m_scriptResult.image_ref = binding.image_path;
      m_scriptResult.reason =
          "bound evidence to semantic node: script=" + binding.script_id +
          " image=" + binding.image_id + " target=" + binding.target_id +
          " param=" + binding.parameter_summary;
      m_scriptResult.runtime_fillback_status =
          "semantic_node_bound_from_evidence";

      m_manualTest.debug_status = "SEMANTIC_EVIDENCE_BOUND";
      m_manualTest.debug_reason = m_scriptResult.reason;

      std::string applyReason;
      if (!ApplyEvidenceSelectionSnapshotToManualContext(snapshot, false,
                                                         applyReason)) {
        m_manualTest.debug_status = "EVIDENCE_APPLY_FAIL";
        m_manualTest.debug_reason = applyReason;
      }

      return;
    }

    std::string scriptPath;
    std::string scriptName;

    if (!GetSelectedCatalogScript(scriptPath, scriptName)) {
      m_scriptResult.source = "semantic_flow";
      m_scriptResult.status = "PENDING";
      m_scriptResult.reason = "no selected Evidence row and no legacy Script "
                              "Catalog item selected: " +
                              evidenceReason;
      m_scriptResult.runtime_fillback_status = "semantic_bind_blocked";
      return;
    }

    std::string reason;
    if (!m_semanticFlowGraph.BindScriptToNode(action.node_index, scriptPath,
                                              scriptName, reason)) {
      m_scriptResult.source = "semantic_flow";
      m_scriptResult.status = "PENDING";
      m_scriptResult.reason = reason;
      m_scriptResult.runtime_fillback_status = "semantic_bind_blocked";
      return;
    }

    m_scriptResult.source = "semantic_flow";
    m_scriptResult.status = "PENDING";
    m_scriptResult.script_path = scriptPath;
    m_scriptResult.reason =
        "semantic node bound from legacy catalog fallback: " + reason;
    m_scriptResult.runtime_fillback_status =
        "semantic_node_bound_from_catalog_fallback";
    return;
  }

  if (action.type == SemanticFlowActionType::LoadBoundScript) {
    std::string reason;
    bool loaded = false;

    if (action.has_evidence_binding && action.evidence_binding.valid) {
      loaded = LoadSemanticEvidenceBindingToManualConsole(
          action.node_id, action.evidence_binding, true, reason);
    } else {
      loaded = LoadBoundStateToManualConsole(action.node_id, action.script_path,
                                             reason);
    }

    m_scriptResult.source = "semantic_flow";
    m_scriptResult.status = loaded ? "PENDING" : "FAIL";
    m_scriptResult.script_path = action.has_evidence_binding
                                     ? action.evidence_binding.script_path
                                     : action.script_path;
    m_scriptResult.image_ref =
        action.has_evidence_binding ? action.evidence_binding.image_path : "";
    m_scriptResult.reason = reason;
    m_scriptResult.runtime_fillback_status =
        loaded ? (action.has_evidence_binding ? "semantic_evidence_node_loaded"
                                              : "semantic_node_loaded")
               : "semantic_node_load_failed";
    return;
  }

  if (action.type == SemanticFlowActionType::RunBoundScript) {
    SemanticExecutionContext execContext;
    std::string contextReason;

    if (!BuildSemanticExecutionContext(action, execContext, contextReason)) {
      m_scriptResult.source = "semantic_flow";
      m_scriptResult.status = "FAIL";
      m_scriptResult.script_path = action.script_path;
      m_scriptResult.reason = contextReason;
      m_scriptResult.runtime_fillback_status =
          "semantic_execution_context_failed";
      return;
    }

    if (!ApplySemanticExecutionContextBeforeRun(execContext, contextReason)) {
      m_scriptResult.source = "semantic_flow";
      m_scriptResult.status = "FAIL";
      m_scriptResult.script_path = execContext.script_path;
      m_scriptResult.reason = contextReason;
      m_scriptResult.runtime_fillback_status =
          "semantic_execution_context_apply_failed";
      return;
    }

    m_scriptResult = RunCxScript(execContext.script_path);

    m_scriptResult.source = "semantic_flow";
    m_scriptResult.script_path = execContext.script_path;

    if (execContext.from_evidence) {
      m_scriptResult.image_ref = execContext.image_path;
      m_scriptResult.reason =
          m_scriptResult.reason +
          " | evidence_context: case=" + execContext.case_id +
          " image=" + execContext.image_id +
          " target=" + execContext.target_id +
          " param=" + execContext.parameter_summary;
      m_scriptResult.runtime_fillback_status =
          "semantic_evidence_bound_script_run_requested";
    } else {
      m_scriptResult.runtime_fillback_status =
          "semantic_bound_script_run_requested";
    }

    m_scriptResult.log_lines.push_back("semantic execution context: " +
                                       execContext.reason);

    RefreshRuntimeObjectTable("Flow Run", m_scriptResult.status == "BLOCKED"
                                              ? "BLOCKED"
                                              : "runtime_executed");

    m_scriptResult.log_lines.push_back(
        "after RefreshRuntimeObjectTable: runtime_objects=" +
        std::to_string(m_manualTest.runtime_objects.size()) +
        " result_status=" + m_manualTest.current_result_ref.status +
        " result_source=" + m_manualTest.current_result_ref.source_object);

    if (m_manualTest.runtime_objects.empty()) {
      m_manualTest.debug_status = "SEMANTIC_RUN_NO_RUNTIME_OBJECTS";
      m_manualTest.debug_reason =
          "script executed but no Findline/FindCircle/FindEllipse runtime "
          "object was queried";
    } else if (m_manualTest.current_result_ref.source_object.empty()) {
      m_manualTest.debug_status = "SEMANTIC_RUN_NO_RESULT_REF";
      m_manualTest.debug_reason =
          "runtime objects exist but no result ref was produced";
    } else {
      m_manualTest.debug_status = "SEMANTIC_RUN_RESULT_AVAILABLE";
      m_manualTest.debug_reason =
          "result from " + m_manualTest.current_result_ref.source_object +
          " status=" + m_manualTest.current_result_ref.status;
    }

    m_semanticFlowGraph.ApplyScriptResult(
        action.node_index, m_scriptResult.status, m_scriptResult.result_ref,
        m_scriptResult.evidence_ref, m_scriptResult.issue_entry_ref,
        m_scriptResult.reason);

    return;
  }
}
bool ViewController::PrepareTorchUiRequestContext(
    const std::string &scriptText, const std::string &scriptPathText,
    std::string &reason) {
  reason.clear();
  if (!scriptContainsIdentifier(scriptText, "global_torch_request_context")) {
    m_parserDebugBridge.RemoveGlobalString("global_torch_request_context");
    return true;
  }

  const fs::path inputPath = fs::path(m_manualTest.image_file_path);
  if (inputPath.empty() || !fs::exists(inputPath)) {
    reason =
        "Torch UI run requires an Image View image with a valid source path";
    return false;
  }

  const fs::path root = findRepositoryRoot();
  if (root.empty()) {
    reason = "Torch UI run cannot resolve repository root";
    return false;
  }

  const fs::path scriptPath = ResolveWorkspaceFile(scriptPathText);
  const std::string caseId = scriptPath.stem().string().empty()
                                 ? std::string("torch_ui_case")
                                 : scriptPath.stem().string();
  std::string runId = CxUnifiedLog::Instance().RunId();
  if (runId.empty())
    runId = "ui_session";
  const fs::path outputPath =
      root.parent_path() / "cxscript_runs" / "manual_torch_ui" / runId / caseId;
  auto runtimeInt = [&](const char *name, int fallback) -> int {
    const auto it = m_manualTest.runtime_int_vars.find(name);
    return it == m_manualTest.runtime_int_vars.end() ? fallback : it->second;
  };
  const TorchTrainingImageItem *selectedTrainingImage = nullptr;
  if (m_manualTest.selected_torch_training_image >= 0 &&
      m_manualTest.selected_torch_training_image <
          static_cast<int>(m_manualTest.torch_training_images.size())) {
    selectedTrainingImage =
        &m_manualTest.torch_training_images[static_cast<std::size_t>(
            m_manualTest.selected_torch_training_image)];
  }

  std::ostringstream annotationContext;
  annotationContext
      << "{\"schema\":\"cx.torch.annotation_context.v1\""
      << ",\"image_id\":\""
      << jsonEscapeEvidenceReview(selectedTrainingImage == nullptr
                                      ? m_manualTest.active_image_id
                                      : selectedTrainingImage->image_id)
      << "\""
      << ",\"split\":\""
      << jsonEscapeEvidenceReview(selectedTrainingImage == nullptr
                                      ? std::string()
                                      : selectedTrainingImage->split)
      << "\""
      << ",\"label\":\""
      << jsonEscapeEvidenceReview(selectedTrainingImage == nullptr
                                      ? std::string()
                                      : selectedTrainingImage->label)
      << "\""
      << ",\"annotation_shape_count\":"
      << (selectedTrainingImage == nullptr
              ? 0
              : selectedTrainingImage->annotation_shape_count)
      << ",\"roi\":{\"x0\":" << runtimeInt("global_roi_x0", 0)
      << ",\"y0\":" << runtimeInt("global_roi_y0", 0)
      << ",\"x1\":" << runtimeInt("global_roi_x1", 0)
      << ",\"y1\":" << runtimeInt("global_roi_y1", 0) << "}"
      << ",\"input_width\":" << runtimeInt("global_torch_input_width", 512)
      << ",\"input_height\":" << runtimeInt("global_torch_input_height", 512)
      << ",\"confidence_percent\":"
      << runtimeInt("global_torch_confidence_percent", 50)
      << ",\"mask_threshold_percent\":"
      << runtimeInt("global_torch_mask_threshold_percent", 50)
      << ",\"max_detections\":"
      << runtimeInt("global_torch_max_detections", 100)
      << ",\"epochs\":" << runtimeInt("global_torch_epochs", 1)
      << R"(,"batch_size":)" << runtimeInt("global_torch_batch_size", 1)
      << R"(,"num_classes":)" << runtimeInt("global_torch_num_classes", 7)
      << R"(,"top_k":)" << runtimeInt("global_torch_top_k", 5)
      << R"(,"normalize_input":)"
      << runtimeInt("global_torch_normalize_input", 1)
      << R"(,"feature_pyramid_enabled":)"
      << runtimeInt("global_torch_feature_pyramid_enabled", 0) << "}";

  // Use forward-slash paths in the CxScript string literal.  The parser's
  // char* call path preserves backslashes literally, so escaping a Windows
  // path would otherwise reach TorchTask as D:\\\\... and create/display the
  // wrong artifact path.
  const std::string requestContext = caseId + "|" + inputPath.generic_string() +
                                     "|" + outputPath.generic_string() + "|" +
                                     annotationContext.str();
  m_parserDebugBridge.SetGlobalString("global_torch_request_context",
                                      requestContext);

  reason = "input=" + inputPath.generic_string() +
           " output=" + outputPath.generic_string() + " annotation_shapes=" +
           std::to_string(selectedTrainingImage == nullptr
                              ? 0
                              : selectedTrainingImage->annotation_shape_count);
  CXLOG_INFO(
      "TorchUI", "torch_annotation_request_staged", "ui_event",
      "case_id=" + caseId + " " + reason + " input_size=" +
          std::to_string(runtimeInt("global_torch_input_width", 512)) + "x" +
          std::to_string(runtimeInt("global_torch_input_height", 512)) +
          " epochs=" + std::to_string(runtimeInt("global_torch_epochs", 1)) +
          " batch_size=" +
          std::to_string(runtimeInt("global_torch_batch_size", 1)) +
          " roi=" + std::to_string(runtimeInt("global_roi_x0", 0)) + "," +
          std::to_string(runtimeInt("global_roi_y0", 0)) + "," +
          std::to_string(runtimeInt("global_roi_x1", 0)) + "," +
          std::to_string(runtimeInt("global_roi_y1", 0)));
  return true;
}

ViewController::ScriptResult
ViewController::RunCxScript(const std::string &theScriptPath) {
  ScriptResult result;
  result.source = "file";
  result.script_path = theScriptPath;
  result.runtime_fillback_status = "pending_real_runtime_fillback";
  if (theScriptPath.empty()) {
    result.status = "PENDING";
    result.reason = "no script selected";
    result.runtime_fillback_status = "not_started";
    return result;
  }
  const fs::path root = findRepositoryRoot();
  const fs::path scriptPath = root / fs::path(theScriptPath);
  if (root.empty() || !fs::exists(scriptPath)) {
    result.status = "FAIL";
    result.reason = "script file not found";
    result.log_lines.push_back("RunCxScript rejected a missing script path.");
    return result;
  }
  const fs::path moduleRoot = root / "cxparser" / "cxscript" / "module";
  const fs::path integrationRoot =
      root / "cxparser" / "cxscript" / "integration";
  if (!pathIsWithin(scriptPath, moduleRoot) &&
      !pathIsWithin(scriptPath, integrationRoot)) {
    result.status = "BLOCKED";
    result.reason = "script path is outside the allowed cxscript runtime roots";
    result.log_lines.push_back("rag_script_cases is semantic_reference_only.");
    return result;
  }
  std::ifstream stream(scriptPath);
  const std::string scriptText((std::istreambuf_iterator<char>(stream)),
                               std::istreambuf_iterator<char>());
  if (scriptText.find("dev_analysis_gui_shell.exe") != std::string::npos) {
    const fs::path localShell =
        root / "cxparser" / "build" / "Release" / "dev_analysis_gui_shell.exe";
    const fs::path workspaceShell = root.parent_path().parent_path() /
                                    "cxparser" / "build" / "Release" /
                                    "dev_analysis_gui_shell.exe";
    if (!fs::exists(localShell) && !fs::exists(workspaceShell)) {
      result.status = "BLOCKED";
      result.reason = "dev_analysis_gui_shell.exe missing";
      result.log_lines.push_back(
          "External GUI capture dependency is unavailable.");
      result.log_lines.push_back("No runtime result package was produced.");
      return result;
    }
  }

  std::string torchRequestReason;
  if (!PrepareTorchUiRequestContext(scriptText, scriptPath.string(),
                                    torchRequestReason)) {
    result.status = "BLOCKED";
    result.reason = torchRequestReason;
    result.runtime_fillback_status = "torch_ui_input_path_missing";
    return result;
  }
  if (!torchRequestReason.empty())
    result.log_lines.push_back("Torch UI request context: " +
                               torchRequestReason);
  // Semantic Flow and Manual Console must execute with the same external
  // input snapshot.  The editor Run button already performs this binding;
  // bind it here as well for "Run Bound Script".
  if (m_manualTest.current_gauge.has_line_gauge ||
      m_manualTest.current_gauge.has_circle_gauge ||
      m_manualTest.current_gauge.has_ellipse_gauge ||
      m_manualTest.current_gauge.has_findobject_roi) {
    ApplyManualGaugeToGlobals(m_manualTest);
  }
  // A pending ROI edit intentionally survives repaint, but a real CxScript
  // Run is the boundary where the parser-owned tool becomes authoritative
  // again.  Clear the preservation flag before publishing this run's shapes.
  m_annotationLayer.ConfirmAllRuntimeWritebacks();
  for (const auto &input : m_manualTest.runtime_int_vars) {
    if (input.first.rfind("global_", 0) == 0)
      m_parserDebugBridge.SetGlobalInt(input.first, input.second);
  }
  bool imageBound = true;
  if (!m_imageViewImage.empty())
    imageBound = m_parserDebugBridge.SetGlobalMatInput(m_imageViewImage);
  else if (!s_img0.empty())
    imageBound = m_parserDebugBridge.SetGlobalMatInput(s_img0);

  const bool ran = imageBound && m_parserDebugBridge.RunScript(scriptText);
  result.status = ran ? "PENDING" : "BLOCKED";
  result.reason =
      ran ? "parser runtime executed; runtime objects require query; no PASS "
            "inferred"
          : (imageBound
                 ? ("parser runtime rejected script: " +
                    m_parserDebugBridge.LastError())
                 : "no Image View/default image available for global_matInput");
  result.runtime_fillback_status =
      ran ? "runtime_objects_queried" : "not_started";
  result.log_lines.push_back(ran ? "Script executed through ParserDebugBridge."
                                 : "ParserDebugBridge execution failed.");
  if (!ran && imageBound)
    result.log_lines.push_back("ParserDebugBridge error: " +
                               m_parserDebugBridge.LastError());
  return result;
}

static bool
SyncRuntimeObjectToManualGaugeState(ManualTestContext &context,
                                    const RuntimeObjectView &object) {
  ManualGaugeState &gauge = context.current_gauge;

  if (object.type == "FindLine" && object.has_line_roi) {
    gauge.tool = "FindLine";
    gauge.source = "runtime_object";
    gauge.review_status = "editing";

    gauge.has_line_gauge = true;
    gauge.has_circle_gauge = false;

    gauge.line_x0 = static_cast<int>(std::round(object.line_x0));
    gauge.line_y0 = static_cast<int>(std::round(object.line_y0));
    gauge.line_x1 = static_cast<int>(std::round(object.line_x1));
    gauge.line_y1 = static_cast<int>(std::round(object.line_y1));

    int half_width =
        static_cast<int>(std::round(object.effective_tool_half_width));
    if (half_width <= 0)
      half_width =
          static_cast<int>(std::round(object.requested_tool_half_width));
    if (half_width <= 0)
      half_width = static_cast<int>(std::round(object.line_scan_half_width));
    if (half_width <= 0)
      half_width = 20;

    gauge.tool_half_width = half_width;
    gauge.wgap = object.line_tool_wgap;
    gauge.hgap = object.line_tool_hgap;
    gauge.linegap = object.linegap;

    gauge.dirty = false;
    gauge.accepted = false;
    return true;
  }

  if (object.type == "FindCircle" && object.has_circle) {
    gauge.tool = "FindCircle";
    gauge.source = "runtime_object";
    gauge.review_status = "editing";

    gauge.has_circle_gauge = true;
    gauge.has_line_gauge = false;

    gauge.circle_cx = static_cast<int>(std::round(object.circle_cx));
    gauge.circle_cy = static_cast<int>(std::round(object.circle_cy));

    const int px = static_cast<int>(std::round(object.circle_px));
    const int py = static_cast<int>(std::round(object.circle_py));
    int radius = static_cast<int>(
        std::lround(std::hypot(static_cast<double>(px - gauge.circle_cx),
                               static_cast<double>(py - gauge.circle_cy))));
    if (radius <= 0)
      radius = 50;

    const int runtimeOuterRadius =
        object.ring_outer_radius > 0.0
            ? static_cast<int>(std::lround(object.ring_outer_radius))
            : radius;
    const int runtimeInnerRadius =
        object.ring_inner_radius > 0.0
            ? static_cast<int>(std::lround(object.ring_inner_radius))
            : 0;

    gauge.radius = std::max(1, runtimeOuterRadius);
    gauge.outer_radius = gauge.radius;
    gauge.inner_radius =
        std::max(0, std::min(runtimeInnerRadius, gauge.outer_radius - 1));
    gauge.circle_px = gauge.circle_cx + gauge.outer_radius;
    gauge.circle_py = gauge.circle_cy;

    gauge.dirty = false;
    gauge.accepted = false;
    return true;
  }

  if (object.type == "FindEllipse" && object.has_ellipse_roi) {
    gauge.tool = "FindEllipse";
    gauge.source = "runtime_object";
    gauge.review_status = "editing";

    gauge.has_ellipse_gauge = true;
    gauge.has_line_gauge = false;
    gauge.has_circle_gauge = false;

    const int rx =
        std::max(1, static_cast<int>(std::lround(object.ellipse_rx)));
    const int ry =
        std::max(1, static_cast<int>(std::lround(object.ellipse_ry)));
    const int cx = static_cast<int>(std::lround(object.ellipse_cx));
    const int cy = static_cast<int>(std::lround(object.ellipse_cy));
    gauge.ellipse_x0 = cx - rx;
    gauge.ellipse_y0 = cy - ry;
    gauge.ellipse_x1 = cx + rx;
    gauge.ellipse_y1 = cy + ry;
    gauge.ellipse_inner_scale_percent =
        std::max(0, std::min(99, object.ellipse_inner_scale_percent));

    gauge.dirty = false;
    gauge.accepted = false;
    return true;
  }

  return false;
}

void ViewController::drawScriptAcceptancePanels() {
  ImGuiIO &panelIo = ImGui::GetIO();
  const ImGuiCond layoutCondition =
      m_detachablePanels ? ImGuiCond_FirstUseEver : ImGuiCond_Always;
  const ImGuiWindowFlags panelFlags =
      m_detachablePanels ? ImGuiWindowFlags_None : ImGuiWindowFlags_NoMove;
  const ImVec2 displaySize = panelIo.DisplaySize;
  const float margin = 4.0f;
  const float contentHeight = std::max(300.0f, displaySize.y - margin * 2.0f);
  const float leftWidth = std::max(260.0f, displaySize.x * 0.28f);
  const float rightWidth = std::max(300.0f, displaySize.x * 0.30f);
  const float middleWidth =
      std::max(280.0f, displaySize.x - leftWidth - rightWidth - margin * 4.0f);
  const float catalogHeight = (m_evidenceChainUiSection == 2)
                                  ? contentHeight * 0.78f
                                  : contentHeight * 0.55f;
  const float runHeight = 155.0f;
  const float middleX = margin * 2.0f + leftWidth;
  const float rightX = middleX + middleWidth + margin;

  ImGui::SetNextWindowPos(ImVec2(margin, margin), layoutCondition);
  ImGui::SetNextWindowSize(ImVec2(leftWidth, catalogHeight), layoutCondition);
  ImGui::Begin("Evidence Chain UI", nullptr, panelFlags);
  ImGui::TextUnformatted("证据链 UI");
  ImGui::TextDisabled("(图像集 / case / evidence / review)");
  ImGui::Separator();

  const char *sectionLabels[] = {"Image Set", "Case", "Evidence", "Review"};
  const char *sectionHints[] = {"图像集", "case", "evidence", "review"};
  const char *sectionIcons[] = {"[IMG]", "[DOC]", "[OK]", "[USR]"};
  const float buttonWidth =
      std::max(54.0f, (ImGui::GetContentRegionAvail().x - 18.0f) / 4.0f);
  for (int i = 0; i < 4; ++i) {
    if (i > 0)
      ImGui::SameLine();
    ImGui::PushID(i);
    const bool selectedBeforeClick = m_evidenceChainUiSection == i;

    const AiGuiDestination sectionDestination =
        i == 0   ? AiGuiDestination::EvidenceImageSet
        : i == 1 ? AiGuiDestination::EvidenceCase
        : i == 2 ? AiGuiDestination::ManualReviewEvidence
                 : AiGuiDestination::ManualReviewDecision;
    const char* sectionLocation =
        i == 0   ? "Evidence Chain UI > Image Set tab/list"
        : i == 1 ? "Evidence Chain UI > Case tab/list"
        : i == 2 ? "Manual Review / Evidence > Evidence tab/items"
                 : "Manual Review / Evidence > Review decision controls";
    ApplyAiGuiFocusHere(sectionDestination, sectionLocation);
    if (selectedBeforeClick)
      ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(70, 130, 190, 255));
    if (ImGui::Button(
            (std::string(sectionIcons[i]) + "\n" + sectionLabels[i]).c_str(),
            ImVec2(buttonWidth, 48.0f))) {
      m_evidenceChainUiSection = i;
      if (i == 2) {
        // Switching to the Evidence page must stay instant.  The page body
        // performs lazy asset/group preparation and thumbnail loading with a
        // per-frame budget; doing a synchronous rebuild here makes the tab
        // click feel frozen on large evidence sets.
        if (m_manualTest.script_evidence_groups.empty())
          m_manualTest.script_evidence_groups_dirty = true;
      }
    }
    if (selectedBeforeClick)
      ImGui::PopStyleColor();
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("%s", sectionHints[i]);
    ImGui::PopID();
  }

  ImGui::Separator();
  if (m_evidenceChainUiSection == 0) {
    ImGui::TextUnformatted("Image Set / 图像集");
    ImGui::Text("manifest: %s",
                m_manualTest.manifest_loaded ? "loaded" : "not loaded");
    ImGui::TextWrapped("%s", m_manualTest.manifest_path.empty()
                                 ? "(no manifest path)"
                                 : m_manualTest.manifest_path.c_str());
    if (!m_manualTest.image_manifest_items.empty()) {
      ImGui::BeginChild("evidence_image_set_list", ImVec2(-1, 0), true);
      for (std::size_t i = 0; i < m_manualTest.image_manifest_items.size();
           ++i) {
        const ManifestImageItem &item = m_manualTest.image_manifest_items[i];
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::Selectable(item.image_id.c_str(),
                              m_manualTest.active_image_id == item.image_id)) {
          m_manualTest.active_image_id = item.image_id;
          m_manualTest.image_file_path = item.image_path;
          if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            std::string reason;
            if (!LoadImageIntoImageView(item.image_path, reason)) {
              m_manualTest.debug_status = "LOAD_IMAGE_FAIL";
              m_manualTest.debug_reason = reason;
            } else {
              m_manualTest.debug_status = "IMAGE_VIEW_LOADED";
              m_manualTest.debug_reason =
                  "double clicked image set item: " + item.image_path;
            }
          }
        }
        ImGui::TextDisabled("%s | %s", item.level.c_str(), item.status.c_str());
        ImGui::PopID();
      }
      ImGui::EndChild();
    } else {
      ImGui::TextDisabled("No image manifest entries loaded.");
    }
  } else if (m_evidenceChainUiSection == 1) {
    ImGui::TextUnformatted("Case / 用例");
    // Case and Evidence are two views of the same file-driven source. Use the
    // interactive asset tree here as well: expanding a directory is inert,
    // while selecting one concrete case atomically applies its script,
    // parameter snapshot and read-only image.
    EnsureCxScriptWorkbenchAssetsLoaded();
    ImGui::TextDisabled(
        "Expand a folder to inspect cases. Click a case row to load its "
        "project, saved parameters and read-only image.");
    DrawScriptEvidenceThumbnailRailByGroup();
  } else if (m_evidenceChainUiSection == 2) {
    ImGui::TextUnformatted("Evidence / 脚本 + 参数 + 图片");

    EnsureCxScriptWorkbenchAssetsLoaded();

    ImGui::TextUnformatted("Trace Binding / 脚本 + 图片 + 参数");
    DrawScriptEvidenceThumbnailRailByGroup();

    {
      const CxEvidenceSelectionSnapshot &sel =
          m_manualTest.current_evidence_selection;

      ImGui::Separator();
      ImGui::Text("Current Evidence Selection:");
      ImGui::Text("review=%s", sel.review_item.empty()
                                  ? "-"
                                  : sel.review_item.c_str());
      ImGui::Text("valid=%d group=%d thumb=%d", sel.valid ? 1 : 0,
                  sel.group_index, sel.thumb_index);
      ImGui::Text("script=%s",
                  sel.script_id.empty() ? "-" : sel.script_id.c_str());
      ImGui::Text("image=%s",
                  sel.image_id.empty() ? "-" : sel.image_id.c_str());
      ImGui::Text("target=%s",
                  sel.target_id.empty() ? "-" : sel.target_id.c_str());
      ImGui::Text("param=%s", sel.parameter_summary.empty()
                                  ? "-"
                                  : sel.parameter_summary.c_str());
      DrawEvidenceWorkflowPanel();
      DrawTorchEvidenceTrainingPanel();

      if (ImGui::Button("Run Evidence Self Test L0")) {
        CxEvidenceSelfTestRequest request;
        request.run_id = "manual_ui_evidence_selftest";
        request.case_id = "selected_evidence_l0";
        request.group_index = m_manualTest.selected_evidence_group;
        request.thumb_index = m_manualTest.selected_evidence_thumb;
        request.out_dir = "cxscript_runs/evidence_selftest/manual_ui_selected";

        CxEvidenceSelfTestResult selftest;
        std::string selftestReason;

        const bool ok =
            RunEvidenceChainSelfTest(request, selftest, selftestReason);

        std::string writeReason;
        WriteEvidenceSelfTestSummaryJson(
            selftest, request.out_dir + "/evidence_selftest_summary.json",
            writeReason);

        WriteEvidenceSelfTestReportMd(
            selftest, request.out_dir + "/evidence_selftest_report.md",
            writeReason);

        m_manualTest.debug_status =
            ok ? "EVIDENCE_SELFTEST_L0_READY" : "EVIDENCE_SELFTEST_L0_FAIL";
        m_manualTest.debug_reason = selftestReason;
      }

      if (ImGui::Button("Run Evidence Chain Self Test")) {
        CxEvidenceSelfTestRequest request;
        request.run_id = "manual_ui_evidence_chain_selftest";
        request.case_id = "selected_evidence_chain";
        request.group_index = m_manualTest.selected_evidence_group;
        request.thumb_index = m_manualTest.selected_evidence_thumb;
        request.out_dir = "cxscript_runs/evidence_selftest/manual_ui_chain";

        std::string selftestReason;

        if (!RunEvidenceChainSelfTest(
                request, m_manualTest.last_evidence_selftest_result,
                selftestReason)) {
          m_manualTest.debug_status =
              m_manualTest.last_evidence_selftest_result.final_code.empty()
                  ? "EVIDENCE_SELFTEST_FAIL"
                  : m_manualTest.last_evidence_selftest_result.final_code;
          m_manualTest.debug_reason = selftestReason;
        } else {
          m_manualTest.debug_status =
              m_manualTest.last_evidence_selftest_result.final_code;
          m_manualTest.debug_reason =
              m_manualTest.last_evidence_selftest_result.final_reason;
        }

        std::string writeReason;
        WriteEvidenceSelfTestSummaryJson(
            m_manualTest.last_evidence_selftest_result,
            request.out_dir + "/evidence_selftest_summary.json", writeReason);

        WriteEvidenceSelfTestReportMd(
            m_manualTest.last_evidence_selftest_result,
            request.out_dir + "/evidence_selftest_report.md", writeReason);
      }

      if (ImGui::Button("Run Evidence Batch Self Test")) {
        CxEvidenceSelfTestBatchRequest request;
        request.run_id = "manual_ui_evidence_batch_selftest";
        request.out_dir = "cxscript_runs/evidence_selftest/manual_ui_batch";
        request.max_cases = 0;

        std::string reason;
        if (!BuildEvidenceSelfTestBatchFromCurrentEvidenceRows(request,
                                                               reason)) {
          m_manualTest.debug_status = "EVIDENCE_BATCH_BUILD_FAIL";
          m_manualTest.debug_reason = reason;
        } else {
          CxEvidenceSelfTestBatchResult batch;
          const bool ok = RunEvidenceSelfTestBatch(request, batch, reason);

          m_manualTest.debug_status = batch.final_code;
          m_manualTest.debug_reason = batch.final_reason;
        }
      }

      ImGui::Text(
          "SelfTest: %s",
          m_manualTest.last_evidence_selftest_result.final_code.c_str());

      for (const auto &step :
           m_manualTest.last_evidence_selftest_result.steps) {
        ImGui::BulletText("%s | %s | %s", step.code.c_str(),
                          step.status.c_str(), step.reason.c_str());
      }
    }

    ImGui::Separator();
    ImGui::Text("thumb_groups=%d",
                static_cast<int>(m_manualTest.script_evidence_groups.size()));
    ImGui::Text("evidence_items=%d catalog_entries=%d script_catalog=%d "
                "image_manifest=%d",
                static_cast<int>(m_manualTest.evidence_items.size()),
                static_cast<int>(m_manualTest.catalog_entries.size()),
                static_cast<int>(m_scriptCatalog.size()),
                static_cast<int>(m_manualTest.image_manifest_items.size()));

    ImGui::Separator();
    ImGui::Checkbox("Show all catalog scripts", &m_showAllScripts);

    if (ImGui::CollapsingHeader("Legacy CxScript File List / 调试用旧列表")) {
      for (std::size_t i = 0; i < m_scriptCatalog.size(); ++i) {
        const ScriptCatalogEntry &item = m_scriptCatalog[i];
        const bool isDirectLike =
            item.name.find("direct_test") != std::string::npos ||
            item.name.find("_direct") != std::string::npos ||
            item.name.find("_smoke") != std::string::npos ||
            item.path.find("/headless/") != std::string::npos;
        if (!m_showAllScripts && !isDirectLike)
          continue;
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::Selectable(item.name.c_str(),
                              m_selectedScript == static_cast<int>(i))) {
          m_selectedScript = static_cast<int>(i);
          m_manualTest.loaded_script_path = item.path;
        }
        ImGui::TextWrapped("path: %s", item.path.c_str());
        ImGui::Text("type: %s | status: %s", item.type.c_str(),
                    item.status.c_str());
        ImGui::TextWrapped("description: %s", item.description.c_str());
        ImGui::Separator();
        ImGui::PopID();
      }
      if (m_scriptCatalog.empty())
        ImGui::TextDisabled("No runnable scripts found.");
    }
  } else {
    ImGui::TextUnformatted("Review / 人工复核");
    ImGui::Text("active_case: %s", m_manualTest.active_case_id.empty()
                                       ? "(none)"
                                       : m_manualTest.active_case_id.c_str());
    ImGui::Text("active_image: %s", m_manualTest.active_image_id.empty()
                                        ? "(none)"
                                        : m_manualTest.active_image_id.c_str());
    ImGui::Text("active_target: %s",
                m_manualTest.active_target_id.empty()
                    ? "(none)"
                    : m_manualTest.active_target_id.c_str());
    ImGui::Text("gauge_review: %s",
                m_manualTest.current_gauge.review_status.c_str());
    if (ImGui::Button("Mark Review Accepted")) {
      m_manualTest.current_gauge.review_status = "manual_accepted";
      m_manualTest.current_gauge.accepted = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Mark Review Rejected")) {
      m_manualTest.current_gauge.review_status = "manual_rejected";
      m_manualTest.current_gauge.accepted = false;
    }
    if (ImGui::Button("Save Evidence Review")) {
      std::string path;
      std::string saveReason;
      if (saveLockedEvidenceReview(m_manualTest, path, saveReason)) {
        m_manualTest.debug_status = "evidence_review_saved";
        m_manualTest.debug_reason = path;
      } else {
        m_manualTest.debug_status = "evidence_review_save_failed";
        m_manualTest.debug_reason = saveReason;
      }
    }
  }
  ImGui::End();

  ImGui::SetNextWindowPos(ImVec2(margin, margin + catalogHeight + margin),
                          layoutCondition);
  ImGui::SetNextWindowSize(
      ImVec2(leftWidth, contentHeight - catalogHeight - margin),
      layoutCondition);
  ImGui::Begin("Capability Status", nullptr, panelFlags);
  ImGui::Text("cxscript:");
  ImGui::BulletText("ScriptCatalog: %s",
                    m_scriptCatalog.empty() ? "blocked" : "available");
  ImGui::BulletText("RunCxScript: pending");
  ImGui::Text("cximage:");
  ImGui::BulletText("ImageView: available");
  ImGui::BulletText("OverlayView: pending");
  ImGui::Text("mlpack:");
  ImGui::BulletText("observe_handoff: pending");
  ImGui::Text("ensmallen:");
  ImGui::BulletText("semantic_lifecycle: listed_only / not_executable");
  ImGui::Text("torch:");
  ImGui::BulletText("listed_only / not_executable / pending");
  ImGui::Text("rag_script_cases:");
  ImGui::BulletText("semantic_reference_only / not_runtime_target");
  ImGui::End();

  ImGui::SetNextWindowPos(ImVec2(middleX, margin), layoutCondition);
  ImGui::SetNextWindowSize(ImVec2(middleWidth, contentHeight), layoutCondition);
  ImGui::Begin("Image View", nullptr, panelFlags);
  const bool parserImage =
      m_scriptResult.image_ref.rfind("runtime_object:", 0) == 0;
  const bool baseImageValid =
      m_imageViewTexture != 0 && !m_imageViewImage.empty();
  const char *baseImageSource =
      parserImage ? "parser_image" : (baseImageValid ? "view_image" : "none");
  ImGui::Text("Layer 0 Base Image");
  ImGui::Text(
      "source: %s | name: %s | %s", baseImageSource,
      parserImage
          ? m_scriptResult.image_ref.c_str()
          : (baseImageValid ? m_manualTest.image_file_path.c_str() : "(none)"),
      baseImageValid ? "valid" : "invalid");
  int runtimeVisualCount = 0;
  std::string runtimeVisualNames;

  if (ImGui::Button("Reset view")) {
    m_imageViewZoom = 1.0f;
    m_imageViewPanX = 0.0f;
    m_imageViewPanY = 0.0f;
  }
  ImGui::SameLine();
  ImGui::Text("Zoom: %.0f%%  Move: middle-drag  Zoom: wheel",
              m_imageViewZoom * 100.0f);
  ApplyAiGuiFocusHere(
      AiGuiDestination::ImageAnnotation,
      "Image Evidence / Annotation Tools > annotation session toggle and tool palette");
  DrawAnnotationToolButtonStrip(true);
  ImGui::Separator();

  const ImVec2 canvasSize(
      std::max(120.0f, ImGui::GetContentRegionAvail().x),
      std::max(160.0f, ImGui::GetContentRegionAvail().y -
                           ImGui::GetTextLineHeightWithSpacing() * 3.0f));
  ImGui::InvisibleButton("image_view_canvas", canvasSize,
                         ImGuiButtonFlags_MouseButtonLeft |
                             ImGuiButtonFlags_MouseButtonMiddle |
                             ImGuiButtonFlags_MouseButtonRight);
  ImGuiIO &io = ImGui::GetIO();
  const bool canvasHovered = ImGui::IsItemHovered();
  const bool canvasActive = ImGui::IsItemActive();
  const ImVec2 canvasMin = ImGui::GetItemRectMin();
  const ImVec2 canvasMax = ImGui::GetItemRectMax();
  ImDrawList *drawList = ImGui::GetWindowDrawList();
  drawList->AddRectFilled(canvasMin, canvasMax, IM_COL32(18, 18, 18, 255));
  drawList->AddRect(canvasMin, canvasMax, IM_COL32(90, 90, 90, 255));

  if (canvasHovered && io.MouseWheel != 0.0f && baseImageValid) {
    const float oldZoom = m_imageViewZoom;
    const float newZoom =
        std::clamp(oldZoom * (1.0f + io.MouseWheel * 0.1f), 0.05f, 20.0f);
    if (std::fabs(newZoom - oldZoom) > 0.0001f) {
      const float localX = io.MousePos.x - canvasMin.x - m_imageViewPanX;
      const float localY = io.MousePos.y - canvasMin.y - m_imageViewPanY;
      const float imageXAtMouse = localX / oldZoom;
      const float imageYAtMouse = localY / oldZoom;
      m_imageViewZoom = newZoom;
      m_imageViewPanX = (io.MousePos.x - canvasMin.x) - imageXAtMouse * newZoom;
      m_imageViewPanY = (io.MousePos.y - canvasMin.y) - imageYAtMouse * newZoom;
    }
  }
  if (canvasHovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
    const ImVec2 delta = io.MouseDelta;
    m_imageViewPanX += delta.x;
    m_imageViewPanY += delta.y;
  }

  const ImVec2 imageSize =
      baseImageValid
          ? ImVec2(static_cast<float>(m_imageViewImage.cols) * m_imageViewZoom,
                   static_cast<float>(m_imageViewImage.rows) * m_imageViewZoom)
          : ImVec2(0.0f, 0.0f);
  const ImVec2 imagePos(canvasMin.x + m_imageViewPanX,
                        canvasMin.y + m_imageViewPanY);
  const ImVec2 imageEnd(imagePos.x + imageSize.x, imagePos.y + imageSize.y);
  m_annotationImagePosX = imagePos.x;
  m_annotationImagePosY = imagePos.y;
  m_annotationImageWidth = std::max(1.0f, imageSize.x);
  m_annotationImageHeight = std::max(1.0f, imageSize.y);
  drawList->PushClipRect(canvasMin, canvasMax, true);
  if (baseImageValid) {
    drawList->AddImage((ImTextureID)(uint64_t)m_imageViewTexture, imagePos,
                       imageEnd, ImVec2(0, 0), ImVec2(1, 1));
  } else {
    drawList->AddText(ImVec2(canvasMin.x + 8.0f, canvasMin.y + 8.0f),
                      IM_COL32(200, 200, 200, 255),
                      "Image View: no image loaded");
  }
  const float sx = m_imageViewZoom;
  const float sy = m_imageViewZoom;

  auto ImageToScreen = [&](float x, float y) -> ImVec2 {
    return ImVec2(imagePos.x + x * sx, imagePos.y + y * sy);
  };

  auto DrawImagePolylineClosed = [&](const std::vector<float> &xy, ImU32 color,
                                     float thickness) {
    const std::size_t count = xy.size() / 2;
    if (count < 2)
      return;

    for (std::size_t i = 0; i < count; ++i) {
      const std::size_t j = (i + 1) % count;
      drawList->AddLine(ImageToScreen(xy[i * 2], xy[i * 2 + 1]),
                        ImageToScreen(xy[j * 2], xy[j * 2 + 1]), color,
                        thickness);
    }
  };

  // Layer 2: Manual Element / Annotation Tool Layer.

  // Manual tools are not runtime result.
  drawImageEvidenceOnCanvas(canvasHovered, canvasActive, drawList);

  // Sync Runtime Object to ManualGaugeState if not already set
  if (!m_manualTest.current_gauge.has_line_gauge &&
      !m_manualTest.current_gauge.has_circle_gauge &&
      !m_manualTest.current_gauge.dirty) {
    const std::string activeName =
        m_manualTest.current_result_ref.source_object;

    if (!activeName.empty()) {
      for (const RuntimeObjectView &object : m_manualTest.runtime_objects) {
        if (object.name == activeName && object.visualizable && !object.stale &&
            SyncRuntimeObjectToManualGaugeState(m_manualTest, object)) {
          break;
        }
      }
    } else {
      for (const RuntimeObjectView &object : m_manualTest.runtime_objects) {
        if (!object.visualizable || object.stale)
          continue;

        if (SyncRuntimeObjectToManualGaugeState(m_manualTest, object))
          break;
      }
    }
  }

  // Layer 3: Shape Elements from ImageAnnotationLayer.
  //
  // Draw immutable runtime evidence first, then editable ROI shapes.
  // A FindCircle annulus is one editable CircleShape (outer radius plus
  // inner radius).  Its Rin/Rout handles must remain above the read-only
  // scan circles published by the same tool; otherwise the inner ring is
  // visible but its real handle is visually obscured.
  {
    std::vector<std::string> visibleShapeRefs;
    visibleShapeRefs.reserve(m_annotationLayer.ShapeElements().size());
    for (const CxShapeElement &element : m_annotationLayer.ShapeElements()) {
      if (element.visible && element.shape && !element.editable)
        visibleShapeRefs.push_back(element.stable_ref);
    }

    for (const std::string &ref : visibleShapeRefs) {
      const CxShapeElement *element = nullptr;
      for (const CxShapeElement &candidate :
           m_annotationLayer.ShapeElements()) {
        if (candidate.stable_ref == ref) {
          element = &candidate;
          break;
        }
      }
      if (element != nullptr)
        DrawShapeElementOnImageView(*element, drawList);
    }

    visibleShapeRefs.clear();
    for (const CxShapeElement &element : m_annotationLayer.ShapeElements()) {
      if (element.visible && element.shape && element.editable)
        visibleShapeRefs.push_back(element.stable_ref);
    }

    for (const std::string &ref : visibleShapeRefs) {
      const CxShapeElement *element = nullptr;
      for (const CxShapeElement &candidate :
           m_annotationLayer.ShapeElements()) {
        if (candidate.stable_ref == ref) {
          element = &candidate;
          break;
        }
      }
      if (element != nullptr)
        DrawShapeElementOnImageView(*element, drawList);
    }
  }

  // Layer 3.4: FastMatch compare_gap point-pair and tangent overlay.
  // This is a read-only FastMatch / FindLine pattern projection, not the
  // editable Line Gauge scan overlay.
  {
    const RuntimeObjectView *fastMatchObject = nullptr;
    FastMatch *fastMatchTool = nullptr;
    auto looksFastMatchText = [](std::string text) {
      std::transform(text.begin(), text.end(), text.begin(),
                     [](unsigned char c) {
                       return static_cast<char>(std::tolower(c));
                     });
      return text.find("fastmatch") != std::string::npos;
    };
    auto findFastMatchSnapshotByName = [&](const std::string &name) {
      if (name.empty())
        return static_cast<const RuntimeObjectView *>(nullptr);
      for (const RuntimeObjectView &object : m_manualTest.runtime_objects) {
        if (object.name == name &&
            (object.type == "FastMatch" || object.has_fastmatch_diagnostic))
          return &object;
      }
      return static_cast<const RuntimeObjectView *>(nullptr);
    };

    std::vector<std::string> ownerCandidates;
    auto addOwnerCandidate = [&](const std::string &name) {
      if (!name.empty() && std::find(ownerCandidates.begin(),
                                     ownerCandidates.end(), name) ==
                               ownerCandidates.end())
        ownerCandidates.push_back(name);
    };
    addOwnerCandidate(m_manualTest.current_result_ref.source_object);
    addOwnerCandidate(m_manualTest.current_gauge.primary_object_name);
    for (const RuntimeObjectView &object : m_manualTest.runtime_objects) {
      if (object.type == "FastMatch" || object.has_fastmatch_diagnostic)
        addOwnerCandidate(object.name);
    }

    for (const std::string &candidate : ownerCandidates) {
      if (fastMatchObject == nullptr)
        fastMatchObject = findFastMatchSnapshotByName(candidate);
      fastMatchTool = static_cast<FastMatch *>(
          m_parserDebugBridge.QueryClassObject("FastMatch", candidate));
      if (fastMatchTool != nullptr)
        break;
    }
    if (fastMatchObject == nullptr) {
      for (const RuntimeObjectView &object : m_manualTest.runtime_objects) {
        if (object.type == "FastMatch" || object.has_fastmatch_diagnostic) {
          fastMatchObject = &object;
          break;
        }
      }
    }

    const bool fastMatchContext =
        m_manualTest.current_gauge.tool == "FastMatch" ||
        m_manualTest.current_gauge.primary_object_type == "FastMatch" ||
        looksFastMatchText(m_manualTest.loaded_script_path) ||
        looksFastMatchText(m_manualTest.script_file_path) ||
        fastMatchObject != nullptr || fastMatchTool != nullptr;
    auto runtimeInt = [&](const char *key, int fallback) {
      const auto it = m_manualTest.runtime_int_vars.find(key);
      return it == m_manualTest.runtime_int_vars.end() ? fallback : it->second;
    };

    // The editable Learn ROI is an axis-aligned interaction shape, whereas
    // FastMatch applies its saved scan rotation when it constructs the four
    // directional FindLine probes.  Render that exact future orientation
    // before Learn as a read-only preview, so a click on Learn cannot appear
    // to introduce a hidden second rotation.
    if (fastMatchContext &&
        (m_manualTest.show_fastmatch_learn_scan_ticks ||
         m_manualTest.show_fastmatch_debug_vectors)) {
      const int previewRotationDeg = std::clamp(
          runtimeInt("global_fastmatch_scan_rotation_deg", 0), -180, 180);
      if (previewRotationDeg != 0) {
        const int learnX = runtimeInt("global_learn_roi_x", 0);
        const int learnY = runtimeInt("global_learn_roi_y", 0);
        const int learnW = std::max(1, runtimeInt("global_learn_roi_w", 1));
        const int learnH = std::max(1, runtimeInt("global_learn_roi_h", 1));
        const double centerX = static_cast<double>(learnX) + learnW * 0.5;
        const double centerY = static_cast<double>(learnY) + learnH * 0.5;
        const double radians =
            static_cast<double>(previewRotationDeg) * CV_PI / 180.0;
        const double cosine = std::cos(radians);
        const double sine = std::sin(radians);
        const auto rotatePreviewPoint = [&](double x, double y) {
          const double dx = x - centerX;
          const double dy = y - centerY;
          return CxShapePoint{centerX + cosine * dx - sine * dy,
                              centerY + sine * dx + cosine * dy};
        };
        const CxShapePoint corners[4] = {
            rotatePreviewPoint(learnX, learnY),
            rotatePreviewPoint(learnX + learnW, learnY),
            rotatePreviewPoint(learnX + learnW, learnY + learnH),
            rotatePreviewPoint(learnX, learnY + learnH)};
        const auto imageToScreenPreview = [&](const CxShapePoint &point) {
          return ImVec2(imagePos.x + static_cast<float>(point.x) * sx,
                        imagePos.y + static_cast<float>(point.y) * sy);
        };
        const ImU32 previewColor = IM_COL32(190, 116, 255, 210);
        for (int corner = 0; corner < 4; ++corner) {
          drawList->AddLine(imageToScreenPreview(corners[corner]),
                            imageToScreenPreview(corners[(corner + 1) % 4]),
                            previewColor, 1.6f);
        }
        const CxShapePoint directionTip = rotatePreviewPoint(
            centerX + std::max(16.0, static_cast<double>(learnW) * 0.16),
            centerY);
        drawList->AddLine(imageToScreenPreview(CxShapePoint{centerX, centerY}),
                          imageToScreenPreview(directionTip), previewColor,
                          2.0f);
        const ImVec2 previewLabel = imageToScreenPreview(corners[0]);
        const std::string previewText =
            "FM orientation preview " + std::to_string(previewRotationDeg) +
            " deg (saved parameter)";
        drawList->AddText(ImVec2(previewLabel.x + 6.0f, previewLabel.y - 18.0f),
                          previewColor, previewText.c_str());
      }
    }

    if (fastMatchContext && m_manualTest.show_fastmatch_debug_vectors &&
        m_manualTest.show_fastmatch_learn_result) {
      auto ImageToScreenD = [&](double x, double y) -> ImVec2 {
        return ImVec2(imagePos.x + static_cast<float>(x) * sx,
                      imagePos.y + static_cast<float>(y) * sy);
      };
      auto midpoint = [](const gp_Pnt &a, const gp_Pnt &b) -> gp_Pnt {
        return gp_Pnt((a.X() + b.X()) * 0.5, (a.Y() + b.Y()) * 0.5,
                      (a.Z() + b.Z()) * 0.5);
      };

      const ImU32 pairColor = IM_COL32(255, 206, 64, 145);
      const ImU32 pointAColor = IM_COL32(70, 220, 255, 235);
      const ImU32 pointBColor = IM_COL32(255, 140, 80, 235);
      const ImU32 tangentColor = IM_COL32(255, 80, 220, 235);
      const ImU32 midpointColor = IM_COL32(255, 235, 80, 245);
      const ImU32 filteredColor = IM_COL32(255, 70, 70, 235);
      bool drewPattern = false;

      if (fastMatchTool != nullptr) {
        const bool selectedColumnLearn =
            fastMatchObject != nullptr &&
            fastMatchObject->fastmatch_learn_status_code == 34;
        const double learnOriginX = fastMatchTool->getlearnmodeloriginx();
        const double learnOriginY = fastMatchTool->getlearnmodeloriginy();
        gp_Path &pathA = fastMatchTool->getpatternpathA();
        gp_Path &pathB = fastMatchTool->getpatternpathB();
        const int countA = static_cast<int>(pathA.ElementCount());
        const int countB = static_cast<int>(pathB.ElementCount());
        const int pairCount = std::min(countA, countB);
        const int stride = std::max(1, pairCount / 160);

        if (pairCount > 0) {
          for (int i = 0; i < pairCount; i += stride) {
            const gp_Pnt pa = pathA.ElementAt(i);
            const gp_Pnt pb = pathB.ElementAt(i);
            const gp_Pnt mid = midpoint(pa, pb);
            const ImVec2 sa = ImageToScreenD(pa.X() + learnOriginX,
                                              pa.Y() + learnOriginY);
            const ImVec2 sb = ImageToScreenD(pb.X() + learnOriginX,
                                              pb.Y() + learnOriginY);
            const ImVec2 sm = ImageToScreenD(mid.X() + learnOriginX,
                                              mid.Y() + learnOriginY);

            if (selectedColumnLearn &&
                !m_manualTest.show_fastmatch_compare_gap_pairs) {
              // Edge N / Last denotes the physical boundary crossing.  A/B
              // are internal contrast samples for FastMatch. Keep the compact
              // midpoint view unless the operator explicitly asks to inspect
              // compare-gap pairs.
              drawList->AddCircleFilled(sm, 3.2f, midpointColor, 12);
              drawList->AddCircle(sm, 4.8f, pointBColor, 12, 1.0f);
            } else {
              if (m_manualTest.show_fastmatch_compare_gap_pairs)
                drawList->AddLine(sa, sb, pairColor, 1.4f);
              drawList->AddCircleFilled(sa, 2.6f, pointAColor, 10);
              drawList->AddCircleFilled(sb, 2.6f, pointBColor, 10);
              drawList->AddCircleFilled(sm, 2.0f, midpointColor, 10);
            }

            if (!selectedColumnLearn &&
                m_manualTest.show_fastmatch_keypoint_tangents &&
                pairCount >= 2) {
              const int prevIndex = std::max(0, i - stride);
              const int nextIndex = std::min(pairCount - 1, i + stride);
              if (nextIndex != prevIndex) {
                const gp_Pnt prevMid = midpoint(pathA.ElementAt(prevIndex),
                                                pathB.ElementAt(prevIndex));
                const gp_Pnt nextMid = midpoint(pathA.ElementAt(nextIndex),
                                                pathB.ElementAt(nextIndex));
                const ImVec2 sp = ImageToScreenD(prevMid.X() + learnOriginX, prevMid.Y() + learnOriginY);
                const ImVec2 sn = ImageToScreenD(nextMid.X() + learnOriginX, nextMid.Y() + learnOriginY);
                float vx = sn.x - sp.x;
                float vy = sn.y - sp.y;
                const float len = std::sqrt(vx * vx + vy * vy);
                if (len > 0.5f) {
                  vx /= len;
                  vy /= len;
                  const float halfLen = 8.0f;
                  drawList->AddLine(ImVec2(sm.x - vx * halfLen,
                                           sm.y - vy * halfLen),
                                    ImVec2(sm.x + vx * halfLen,
                                           sm.y + vy * halfLen),
                                    tangentColor, 2.0f);
                }
              }
            }
          }
          const gp_Pnt firstMid =
              midpoint(pathA.ElementAt(0), pathB.ElementAt(0));
          const gp_Pnt firstA = pathA.ElementAt(0);
          const gp_Pnt firstB = pathB.ElementAt(0);
          const double actualGap = std::hypot(firstA.X() - firstB.X(),
                                               firstA.Y() - firstB.Y());
          const bool showingSelectedColumnPairs =
              selectedColumnLearn &&
              m_manualTest.show_fastmatch_compare_gap_pairs;
          const std::string label = std::string(
                                    showingSelectedColumnPairs
                                        ? "FM selected Point Column compare_gap pairs="
                                        : selectedColumnLearn
                                            ? "FM selected Point Column samples="
                                            : "FM compare_gap pairs=") +
                                    std::to_string(pairCount) + " actual_gap=" +
                                    std::to_string(actualGap) + "px A=" +
                                    std::to_string(countA) + " B=" +
                                    std::to_string(countB) + " stride=" +
                                    std::to_string(stride);
          drawList->AddText(ImageToScreenD(firstMid.X() + learnOriginX + 8.0,
                                           firstMid.Y() + learnOriginY + 8.0),
                            midpointColor, label.c_str());
          drewPattern = true;
        }

        if (m_manualTest.show_fastmatch_filtered_points && countA != countB) {
          auto drawUnpaired = [&](gp_Path &path, int beginIndex, int endIndex,
                                  const char *label) {
            const int total = std::max(0, endIndex - beginIndex);
            const int orphanStride = std::max(1, total / 80);
            bool placedLabel = false;
            for (int i = beginIndex; i < endIndex; i += orphanStride) {
              const gp_Pnt p = path.ElementAt(i);
              const ImVec2 sp = ImageToScreenD(p.X() + learnOriginX,
                                                p.Y() + learnOriginY);
              drawList->AddCircleFilled(sp, 3.2f, filteredColor, 12);
              drawList->AddCircle(sp, 5.0f, filteredColor, 12, 1.0f);
              if (!placedLabel) {
                drawList->AddText(ImVec2(sp.x + 7.0f, sp.y + 4.0f),
                                  filteredColor, label);
                placedLabel = true;
              }
            }
          };
          if (countA > pairCount)
            drawUnpaired(pathA, pairCount, countA, "FM unpaired/filtered A");
          if (countB > pairCount)
            drawUnpaired(pathB, pairCount, countB, "FM unpaired/filtered B");
          drewPattern = drewPattern || countA > 0 || countB > 0;
        }
      }

      if (!drewPattern) {
        int learnX = runtimeInt("global_learn_roi_x", 120);
        int learnY = runtimeInt("global_learn_roi_y", 120);
        int learnW = runtimeInt("global_learn_roi_w", 120);
        int learnH = runtimeInt("global_learn_roi_h", 90);
        if (fastMatchObject != nullptr &&
            fastMatchObject->fastmatch_learn_rect_x1 >
                fastMatchObject->fastmatch_learn_rect_x0 &&
            fastMatchObject->fastmatch_learn_rect_y1 >
                fastMatchObject->fastmatch_learn_rect_y0) {
          learnX = fastMatchObject->fastmatch_learn_rect_x0;
          learnY = fastMatchObject->fastmatch_learn_rect_y0;
          learnW = fastMatchObject->fastmatch_learn_rect_x1 -
                   fastMatchObject->fastmatch_learn_rect_x0;
          learnH = fastMatchObject->fastmatch_learn_rect_y1 -
                   fastMatchObject->fastmatch_learn_rect_y0;
        }
        const double learnCx = learnX + learnW * 0.5;
        const double learnCy = learnY + learnH * 0.5;
        const ImVec2 center = ImageToScreenD(learnCx, learnCy);
        drawList->AddCircle(center, 7.0f, IM_COL32(255, 235, 80, 205), 18,
                            1.5f);
        drawList->AddText(ImVec2(center.x + 10.0f, center.y + 8.0f),
                          IM_COL32(255, 235, 80, 225),
                          "FM compare_gap pattern pending");
      }
    }
    // Each directional tab owns an isolated FindLine Probe. Project only the
    // selected probe's captured lines and accepted points; do not filter one
    // global FastMatch edgepattern and present it as four independent runs.
    if (fastMatchContext && m_manualTest.show_fastmatch_learn_scan_ticks &&
        fastMatchTool != nullptr) {
      auto ImageToScreenD = [&](double x, double y) -> ImVec2 {
        return ImVec2(imagePos.x + static_cast<float>(x) * sx,
                      imagePos.y + static_cast<float>(y) * sy);
      };
      const int selectedEdge = std::clamp(m_manualTest.fastmatch_selected_learn_edge, 0, 3);
      const char* edgeName[] = {"Top", "Bottom", "Left", "Right"};
      int learnX = runtimeInt("global_learn_roi_x", 0);
      int learnY = runtimeInt("global_learn_roi_y", 0);
      const ImU32 tickColor = IM_COL32(100, 215, 255, 125);
      const ImU32 conclusionColor = IM_COL32(255, 142, 58, 255);
      const ImU32 selectedGaugeColor = IM_COL32(255, 232, 72, 255);
      const ImU32 selectedAcceptedColor = IM_COL32(75, 245, 145, 255);
      const ImU32 selectedMissingColor = IM_COL32(255, 86, 86, 255);
      const FastMatch::DirectionalProbeEvidence& probe =
          fastMatchTool->getdirectionalprobeevidence(selectedEdge);
      const ManualMetrologyUiState& metrology = m_manualTest.metrology_ui;
      // Gauge Line NUM is a diagnostic selection, not an algorithm enable
      // switch.  Keep the image-side highlight live whenever FastMatch scan
      // ticks are visible; otherwise the waveform selector can advance while
      // the image remains on an indistinguishable cyan-only scan set.
      const int selectedGaugeIndex = probe.scan_lines.empty()
          ? -1
          : std::max(0, std::min(static_cast<int>(probe.scan_lines.size()) - 1,
                                 metrology.gauge_line_num - 1));
      const int scanRenderStride = std::max(
          1, static_cast<int>(probe.scan_lines.size()) / 128);
      for (std::size_t scanIndex = 0; scanIndex < probe.scan_lines.size();
           scanIndex += static_cast<std::size_t>(scanRenderStride)) {
        const FastMatch::DirectionalProbeScanLine& line =
            probe.scan_lines[scanIndex];
        drawList->AddLine(ImageToScreenD(line.p0.x, line.p0.y),
                          ImageToScreenD(line.p1.x, line.p1.y), tickColor,
                          1.0f);
      }
      // The highlighted line is the exact segment consumed by the Gauge
      // waveform.  It is deliberately drawn after the cyan scan ticks so an
      // operator can identify the one-based Gauge Line NUM without guessing.
      if (selectedGaugeIndex >= 0 &&
          selectedGaugeIndex < static_cast<int>(probe.scan_lines.size())) {
        const FastMatch::DirectionalProbeScanLine& selectedLine =
            probe.scan_lines[static_cast<std::size_t>(selectedGaugeIndex)];
        const ImVec2 a = ImageToScreenD(selectedLine.p0.x, selectedLine.p0.y);
        const ImVec2 b = ImageToScreenD(selectedLine.p1.x, selectedLine.p1.y);
        drawList->AddLine(a, b, selectedGaugeColor, 3.0f);
        const bool selectedAccepted =
            static_cast<std::size_t>(selectedGaugeIndex) <
                probe.selected_point_valid_by_scan.size() &&
            probe.selected_point_valid_by_scan[
                static_cast<std::size_t>(selectedGaugeIndex)] != 0 &&
            static_cast<std::size_t>(selectedGaugeIndex) <
                probe.selected_point_by_scan.size();
        const ImVec2 labelAnchor = ImVec2((a.x + b.x) * 0.5f + 8.0f,
                                          (a.y + b.y) * 0.5f - 12.0f);
        const std::string gaugeLabel =
            "GL " + std::to_string(selectedGaugeIndex + 1) + "/" +
            std::to_string(probe.scan_line_count) +
            (selectedAccepted ? "  ACCEPTED" : "  NO ACCEPTED POINT");
        drawList->AddRectFilled(ImVec2(labelAnchor.x - 4.0f,
                                       labelAnchor.y - 3.0f),
                                ImVec2(labelAnchor.x +
                                           ImGui::CalcTextSize(gaugeLabel.c_str()).x +
                                           5.0f,
                                       labelAnchor.y + 16.0f),
                                IM_COL32(16, 20, 24, 215), 2.0f);
        drawList->AddText(labelAnchor,
                          selectedAccepted ? selectedAcceptedColor
                                           : selectedMissingColor,
                          gaugeLabel.c_str());
        if (selectedAccepted) {
          const CxShapePoint& point = probe.selected_point_by_scan[
              static_cast<std::size_t>(selectedGaugeIndex)];
          const ImVec2 p = ImageToScreenD(point.x, point.y);
          drawList->AddCircleFilled(p, 6.5f, selectedAcceptedColor, 16);
          drawList->AddCircle(p, 8.5f, IM_COL32(10, 52, 30, 255), 16, 1.6f);
        } else {
          const ImVec2 midpoint((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f);
          drawList->AddLine(ImVec2(midpoint.x - 6.0f, midpoint.y - 6.0f),
                            ImVec2(midpoint.x + 6.0f, midpoint.y + 6.0f),
                            selectedMissingColor, 2.0f);
          drawList->AddLine(ImVec2(midpoint.x - 6.0f, midpoint.y + 6.0f),
                            ImVec2(midpoint.x + 6.0f, midpoint.y - 6.0f),
                            selectedMissingColor, 2.0f);
        }
      }
      for (const CxShapePoint& point : probe.accepted_points) {
        const ImVec2 p = ImageToScreenD(point.x, point.y);
        drawList->AddCircleFilled(p, 4.5f, conclusionColor, 16);
        drawList->AddCircle(p, 6.0f, IM_COL32(80, 32, 0, 230), 16, 1.2f);
      }
      if (metrology.enabled && metrology.show_scan_profile &&
          metrology.profile_cursor_visible &&
          metrology.profile_cursor_sample_count > 1 &&
          !probe.scan_lines.empty()) {
        const int profileScanIndex = std::max(
            0, std::min(static_cast<int>(probe.scan_lines.size()) - 1,
                        metrology.gauge_line_num - 1));
        const FastMatch::DirectionalProbeScanLine& profileLine =
            probe.scan_lines[static_cast<std::size_t>(profileScanIndex)];
        const int profileSampleIndex = std::max(
            0, std::min(metrology.profile_cursor_sample_count - 1,
                        metrology.profile_cursor_sample_index));
        const double t = static_cast<double>(profileSampleIndex) /
                         static_cast<double>(metrology.profile_cursor_sample_count - 1);
        const CxShapePoint profilePoint{
            profileLine.p0.x + (profileLine.p1.x - profileLine.p0.x) * t,
            profileLine.p0.y + (profileLine.p1.y - profileLine.p0.y) * t};
        const ImVec2 cursor = ImageToScreenD(profilePoint.x, profilePoint.y);
        drawList->AddCircleFilled(cursor, 5.0f, IM_COL32(255, 210, 64, 255), 18);
        drawList->AddCircle(cursor, 7.0f, IM_COL32(30, 22, 0, 240), 18, 1.5f);
        drawList->AddLine(ImVec2(cursor.x - 8.0f, cursor.y),
                          ImVec2(cursor.x + 8.0f, cursor.y),
                          IM_COL32(255, 245, 190, 255), 1.5f);
        drawList->AddLine(ImVec2(cursor.x, cursor.y - 8.0f),
                          ImVec2(cursor.x, cursor.y + 8.0f),
                          IM_COL32(255, 245, 190, 255), 1.5f);
      }
      const ImVec2 label = ImageToScreenD(learnX, learnY - 10.0);
      const std::string text = std::string("FM ") + edgeName[selectedEdge] +
          " probe scans=" + std::to_string(probe.scan_line_count) +
          " accepted=" + std::to_string(probe.accepted_side_count) +
          (selectedGaugeIndex >= 0
               ? " | Gauge Line " + std::to_string(selectedGaugeIndex + 1) +
                     "/" + std::to_string(probe.scan_line_count)
               : "") +
          " status=" + probe.status;
      drawList->AddText(label, conclusionColor, text.c_str());
    }
    if (fastMatchContext && fastMatchTool != nullptr &&
        (m_manualTest.show_fastmatch_normal_trace_domain ||
         m_manualTest.show_fastmatch_normal_trace_ann_component ||
         m_manualTest.show_fastmatch_normal_trace_path ||
         m_manualTest.show_fastmatch_normal_trace_pairs)) {
      // Normal-trace diagnostics are read-only: original FindLine conclusions
      // -> value-preserving compression -> Dijkstra source references ->
      // derived polyline -> domain-polarity A/B. Nothing is written to input.
      const FastMatch::NormalTraceEvidence& evidence =
          fastMatchTool->getnormaltraceevidence();
      auto ImageToScreenD = [&](double x, double y) -> ImVec2 {
        return ImVec2(imagePos.x + static_cast<float>(x) * sx,
                      imagePos.y + static_cast<float>(y) * sy);
      };
      if (evidence.executed) {
        const ImVec2 evidenceLabel = ImageToScreenD(
            fastMatchTool->getlearnmodeloriginx(),
            fastMatchTool->getlearnmodeloriginy() - 24.0);
        const std::string prefilterText =
            "Conclusion graph valid=" +
            std::to_string(evidence.anchor_prefilter_accepted_counts[0]) + "," +
            std::to_string(evidence.anchor_prefilter_accepted_counts[3]) + "," +
            std::to_string(evidence.anchor_prefilter_accepted_counts[1]) + "," +
            std::to_string(evidence.anchor_prefilter_accepted_counts[2]) +
            " compressed=" +
            std::to_string(evidence.compressed_keypoint_counts[0]) + "," +
            std::to_string(evidence.compressed_keypoint_counts[3]) + "," +
            std::to_string(evidence.compressed_keypoint_counts[1]) + "," +
            std::to_string(evidence.compressed_keypoint_counts[2]) +
            " generated conclusions=" +
            std::to_string(evidence.generated_conclusion_count);
        drawList->AddText(evidenceLabel, IM_COL32(255, 220, 110, 255),
                          prefilterText.c_str());
      }
      if (m_manualTest.show_fastmatch_normal_trace_domain) {
        const ImU32 domainColors[4] = {
            IM_COL32(255, 190, 75, 145), IM_COL32(95, 225, 255, 145),
            IM_COL32(255, 115, 210, 145), IM_COL32(125, 235, 145, 145)};
        const ImU32 keypointColors[4] = {
            IM_COL32(255, 230, 100, 255), IM_COL32(95, 235, 255, 255),
            IM_COL32(255, 130, 220, 255), IM_COL32(135, 245, 155, 255)};
        for (int direction = 0; direction < 4; ++direction) {
          const std::vector<CxShapePoint>& domainPoints =
              evidence.domain_points_by_direction[direction];
          const int stride = std::max(
              1, static_cast<int>(domainPoints.size()) / 180);
          for (std::size_t i = 0; i < domainPoints.size();
               i += static_cast<std::size_t>(stride)) {
            const CxShapePoint& point = domainPoints[i];
            drawList->AddCircleFilled(ImageToScreenD(point.x, point.y), 2.2f,
                                      domainColors[direction], 8);
          }
          for (const CxShapePoint& point :
               evidence.compressed_points_by_direction[direction]) {
            const ImVec2 p = ImageToScreenD(point.x, point.y);
            drawList->AddRectFilled(ImVec2(p.x - 2.6f, p.y - 2.6f),
                                    ImVec2(p.x + 2.6f, p.y + 2.6f),
                                    keypointColors[direction]);
          }
        }
      }
      if (m_manualTest.show_fastmatch_normal_trace_ann_component) {
        const ImU32 selectedColors[4] = {
            IM_COL32(255, 225, 80, 255), IM_COL32(80, 235, 255, 255),
            IM_COL32(255, 105, 220, 255), IM_COL32(100, 255, 135, 255)};
        for (int direction = 0; direction < 4; ++direction) {
          for (const CxShapePoint &point :
               evidence.ann_selected_points_by_direction[direction]) {
            const ImVec2 p = ImageToScreenD(point.x, point.y);
            drawList->AddCircle(p, 4.3f, selectedColors[direction], 10, 1.8f);
          }
        }
      }
      if (m_manualTest.show_fastmatch_normal_trace_path &&
          evidence.derived_trace_points.size() >= 2) {
        ImVec2 previous = ImageToScreenD(evidence.derived_trace_points.front().x,
                                         evidence.derived_trace_points.front().y);
        for (std::size_t i = 1; i < evidence.derived_trace_points.size(); ++i) {
          const CxShapePoint& point = evidence.derived_trace_points[i];
          const ImVec2 current = ImageToScreenD(point.x, point.y);
          drawList->AddLine(previous, current, IM_COL32(8, 18, 24, 225), 3.2f);
          drawList->AddLine(previous, current, IM_COL32(40, 225, 255, 255),
                            1.25f);
          previous = current;
        }
        for (const CxShapePoint &point : evidence.dijkstra_trace_points) {
          const ImVec2 source = ImageToScreenD(point.x, point.y);
          drawList->AddCircleFilled(source, 2.4f,
                                    IM_COL32(255, 230, 95, 255), 8);
        }
        for (const CxShapePoint &point : evidence.derived_junction_points) {
          const ImVec2 junction = ImageToScreenD(point.x, point.y);
          drawList->AddCircleFilled(junction, 4.8f,
                                    IM_COL32(255, 155, 45, 255), 10);
          drawList->AddCircle(junction, 5.6f, IM_COL32(20, 24, 28, 255),
                              10, 1.2f);
        }
      }
      if (m_manualTest.show_fastmatch_normal_trace_pairs) {
        const std::size_t count = std::min(evidence.normal_pair_a.size(),
                                           evidence.normal_pair_b.size());
        // Keep key normals legible; the complete A/B arrays remain available
        // in runtime evidence and line_trace.json.
        const int stride = std::max(1, static_cast<int>(count) / 72);
        for (std::size_t i = 0; i < count; i += static_cast<std::size_t>(stride)) {
          const CxShapePoint& a = evidence.normal_pair_a[i];
          const CxShapePoint& b = evidence.normal_pair_b[i];
          const ImVec2 pa = ImageToScreenD(a.x, a.y);
          const ImVec2 pb = ImageToScreenD(b.x, b.y);
          drawList->AddLine(pa, pb, IM_COL32(12, 14, 18, 235), 3.0f);
          drawList->AddLine(pa, pb, IM_COL32(255, 226, 74, 255), 1.15f);
          drawList->AddCircleFilled(pa, 3.2f, IM_COL32(65, 235, 135, 255), 10);
          drawList->AddCircle(pa, 3.2f, IM_COL32(8, 18, 24, 255), 10, 1.0f);
          drawList->AddCircleFilled(pb, 3.2f, IM_COL32(255, 85, 205, 255), 10);
          drawList->AddCircle(pb, 3.2f, IM_COL32(8, 18, 24, 255), 10, 1.0f);
        }
      }
      const int learnX = runtimeInt("global_learn_roi_x", 0);
      const int learnY = runtimeInt("global_learn_roi_y", 0);
      int annComponents = 0;
      int annSelected = 0;
      for (int direction = 0; direction < 4; ++direction) {
        annComponents += evidence.ann_component_counts[direction];
        annSelected += evidence.ann_selected_point_counts[direction];
      }
      std::string label =
          evidence.reason == "NORMAL_TRACE_DISABLED" ||
                  evidence.reason == "NOT_RUN"
              ? "Normal trace NOT_RUN: enable Normal-Trace Learn, then click Learn"
               : "Normal trace: conclusions=" +
                     std::to_string(evidence.domain_points.size()) +
                    " ANN=" + std::to_string(annSelected) + "/" +
                    std::to_string(annComponents) + " path=" +
                     std::to_string(evidence.dijkstra_trace_points.size()) +
                     " derived=" +
                     std::to_string(evidence.derived_trace_points.size()) +
                     " junctions=" +
                     std::to_string(evidence.derived_junction_points.size()) +
                     " pairs=" + std::to_string(evidence.normal_pair_a.size()) +
                    " segments=" +
                    std::to_string(evidence.trace_segment_count) + "/4 closed=" +
                    std::string(evidence.closure_error_px >= 0.0 &&
                                        evidence.closure_error_px <= 1.5
                                    ? "yes"
                                    : "no") +
                    " coverage=" +
                    std::to_string(static_cast<int>(std::lround(
                         evidence.selected_anchor_coverage * 100.0))) +
                    "% " + evidence.reason;
      label = "Learn rev=" +
              std::to_string(m_manualTest.fastmatch_last_requested_revision) +
              " | " + label;
      if (m_manualTest.show_fastmatch_normal_trace_path &&
          evidence.derived_trace_points.size() < 2) {
        label += " | selected Dijkstra=UNAVAILABLE";
      }
      if (m_manualTest.show_fastmatch_normal_trace_pairs &&
          evidence.normal_pair_a.empty()) {
        label += " | selected NormalPairs=UNAVAILABLE";
      }
      drawList->AddText(ImageToScreenD(learnX, learnY - 28.0),
                        evidence.succeeded ? IM_COL32(90, 235, 170, 255)
                                           : IM_COL32(255, 135, 95, 255),
                        label.c_str());
      if (evidence.succeeded &&
          (m_manualTest.show_fastmatch_normal_trace_path ||
           m_manualTest.show_fastmatch_normal_trace_pairs)) {
        drawList->AddText(
            ImageToScreenD(learnX, learnY - 44.0),
            IM_COL32(245, 245, 230, 255),
            "Trace cyan | Normal yellow | A green | B magenta");
      }
    }
  }

  // Layer 3.5: FindLine runtime scan debug overlay.

  // This intentionally does not create ShapeElements.  It is a read-only
  // debug projection of the native FindLine scan geometry (m_lines_w/h)
  // and scan diagnostics.  Keeping it here avoids the previous fragile
  // dependency on whether the .roi_axis ShapeElement happened to be
  // visible or drawn through the LineGauge branch.
  if (m_manualTest.show_line_gauge_scan_lines ||
      (m_manualTest.metrology_ui.enabled &&
       m_manualTest.metrology_ui.show_scan_profile &&
       (m_manualTest.current_gauge.tool == "FindLine" ||
        m_manualTest.current_gauge.has_line_gauge))) {
    std::string ownerRef;
    FindLine *lineTool = nullptr;
    std::vector<std::string> ownerCandidates;
    auto addOwnerCandidate = [&](const std::string &name) {
      if (!name.empty() && std::find(ownerCandidates.begin(),
                                     ownerCandidates.end(), name) ==
                               ownerCandidates.end())
        ownerCandidates.push_back(name);
    };
    addOwnerCandidate(m_manualTest.current_result_ref.source_object);
    addOwnerCandidate(m_manualTest.current_gauge.primary_object_name);
    for (const RuntimeObjectView &object : m_manualTest.runtime_objects) {
      if (object.type == "FindLine")
        addOwnerCandidate(object.name);
    }
    for (const std::string &candidate : ownerCandidates) {
      lineTool = static_cast<FindLine *>(
          m_parserDebugBridge.QueryClassObject("FindLine", candidate));
      if (lineTool != nullptr) {
        ownerRef = candidate;
        break;
      }
    }

    const int displayMethod = m_manualTest.current_gauge.method;
    const bool reverseArrow = (displayMethod & 1) != 0;
    auto ImageToScreenD = [&](double x, double y) -> ImVec2 {
      return ImVec2(imagePos.x + static_cast<float>(x) * sx,
                    imagePos.y + static_cast<float>(y) * sy);
    };

    auto drawArrowHead = [&](CxShapePoint from, CxShapePoint to) {
      if (reverseArrow)
        std::swap(from, to);

      const ImVec2 pFrom = ImageToScreenD(from.x, from.y);
      const ImVec2 pTo = ImageToScreenD(to.x, to.y);
      const float vx = pTo.x - pFrom.x;
      const float vy = pTo.y - pFrom.y;
      const float vlen = std::sqrt(vx * vx + vy * vy);
      if (vlen < 1.0f)
        return;

      const float ux = vx / vlen;
      const float uy = vy / vlen;
      const float arrowLen = 7.0f;
      const float arrowHalf = 4.0f;
      const ImVec2 base(pTo.x - ux * arrowLen, pTo.y - uy * arrowLen);
      const ImVec2 left(base.x - uy * arrowHalf, base.y + ux * arrowHalf);
      const ImVec2 right(base.x + uy * arrowHalf, base.y - ux * arrowHalf);
      const ImU32 arrowColor = reverseArrow ? IM_COL32(80, 190, 255, 235)
                                            : IM_COL32(255, 160, 48, 235);

      drawList->AddLine(pTo, left, arrowColor, 1.8f);
      drawList->AddLine(pTo, right, arrowColor, 1.8f);
    };

    const bool drawAllLineGaugeScans =
        m_manualTest.show_line_gauge_scan_lines;
    int scanCountW = 0;
    int scanCountH = 0;
    int diagnosticCount = 0;
    // The checkbox is an object display command, not a second UI-only
    // visibility state.  Bit 0x04 is FindLine's native scan-line show
    // bit; preserve all other object display bits.
    if (lineTool != nullptr && drawAllLineGaugeScans) {
      const int currentShowMask = lineTool->show();
      const int requestedShowMask = currentShowMask | 0x04;
      if (requestedShowMask != currentShowMask)
        lineTool->setshow(requestedShowMask);
    }
    if (lineTool != nullptr) {
      scanCountW = lineTool->getscanlinecount(0);
      scanCountH = lineTool->getscanlinecount(1);
      diagnosticCount = lineTool->getscandiagnosticcount();

      const int totalScanCount = scanCountW + scanCountH;

      const int selectedScanOrdinal =
          totalScanCount > 0
              ? std::max(0, std::min(totalScanCount - 1,
                                     m_manualTest.metrology_ui.gauge_line_num -
                                         1))
              : -1;
      const std::string selectedScanLabel =
          "Gauge Line " + std::to_string(selectedScanOrdinal + 1);
      const int arrowSourceCount =
          diagnosticCount > 0 ? diagnosticCount : totalScanCount;
      const int arrowStride = std::max(1, arrowSourceCount / 48);

      auto drawProfileCursorPoint = [&](int scanType, int scanIndex,
                                        const CxShapePoint &a,
                                        const CxShapePoint &b) {
        const ManualMetrologyUiState &metrology = m_manualTest.metrology_ui;
        if (!metrology.profile_cursor_visible ||
            metrology.profile_cursor_sample_count < 2)
          return;

        const int cursorIndex = std::max(
            0, std::min(metrology.profile_cursor_sample_count - 1,
                        metrology.profile_cursor_sample_index));
        const double t = static_cast<double>(cursorIndex) /
                         static_cast<double>(metrology.profile_cursor_sample_count - 1);
        CxShapePoint point;
        if (!lineTool->getscanlineprofilepoint(scanType, scanIndex, t, point)) {
          // Defensive fallback only. Normal FindLine overlays use the same
          // sampled LineShape path as the measurement result.
          point = {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
        }
        const ImVec2 p = ImageToScreenD(point.x, point.y);
        drawList->AddCircleFilled(p, 5.0f, IM_COL32(255, 210, 64, 255), 18);
        drawList->AddCircle(p, 7.0f, IM_COL32(30, 22, 0, 240), 18, 1.5f);
        drawList->AddLine(ImVec2(p.x - 8.0f, p.y), ImVec2(p.x + 8.0f, p.y),
                          IM_COL32(255, 245, 190, 255), 1.5f);
        drawList->AddLine(ImVec2(p.x, p.y - 8.0f), ImVec2(p.x, p.y + 8.0f),
                          IM_COL32(255, 245, 190, 255), 1.5f);
        const std::string label =
            std::string(metrology.profile_cursor_user_dragged ? "Manual cursor "
                                                              : "Preview cursor ") +
            std::to_string(metrology.profile_cursor_permille) + " /1000";

        drawList->AddText(ImVec2(p.x + 8.0f, p.y + 8.0f),
                          IM_COL32(255, 226, 110, 255), label.c_str());
      };


      auto drawActualScanLine = [&](int scanType, int scanIndex,
                                    const CxShapePoint &a,
                                    const CxShapePoint &b, ImU32 color,
                                    float thickness, int ordinal) {
        const bool selected = ordinal == selectedScanOrdinal;
        const ImU32 drawColor =
            selected ? IM_COL32(255, 64, 210, 255) : color;
        const float drawThickness = selected ? std::max(3.0f, thickness)
                                             : thickness;
        const ImVec2 pa = ImageToScreenD(a.x, a.y);
        const ImVec2 pb = ImageToScreenD(b.x, b.y);
        drawList->AddLine(pa, pb, drawColor, drawThickness);
        if (selected) {
          const ImVec2 mid((pa.x + pb.x) * 0.5f + 6.0f,
                           (pa.y + pb.y) * 0.5f + 6.0f);
          drawList->AddCircleFilled(pa, 3.8f, drawColor, 12);
          drawList->AddCircleFilled(pb, 3.8f, drawColor, 12);
          drawList->AddText(mid, drawColor, selectedScanLabel.c_str());
          drawArrowHead(a, b);

          drawProfileCursorPoint(scanType, scanIndex, a, b);

        } else if (drawAllLineGaugeScans && (ordinal % arrowStride) == 0) {
          drawArrowHead(a, b);
        }
        return true;
      };

      int ordinal = 0;
      for (int scanType = 0; scanType <= 1; ++scanType) {
        const int scanCount = lineTool->getscanlinecount(scanType);
        for (int scanIndex = 0; scanIndex < scanCount; ++scanIndex) {
          CxShapePoint a, b;
          if (!lineTool->getscanline(scanType, scanIndex, a, b))
            continue;

          const int currentOrdinal = ordinal++;
          if (!drawAllLineGaugeScans && currentOrdinal != selectedScanOrdinal)
            continue;
          drawActualScanLine(scanType, scanIndex, a, b,
                             IM_COL32(140, 230, 255, 145), 1.0f,
                             currentOrdinal);
        }
      }

      if (diagnosticCount > 0) {
        for (int i = 0; i < diagnosticCount; ++i) {
          FindLineMeasureInputDebug::ScanDiagnostic diag;
          CxShapePoint a, b;
          if (!lineTool->getscandiagnostic(i, diag) ||
              !lineTool->getscandiagnosticline(diag.scan_type, diag.scan_index,
                                               a, b)) {
            continue;
          }

          const int diagOrdinal =
              diag.scan_type <= 0 ? diag.scan_index : scanCountW + diag.scan_index;
          if (!drawAllLineGaugeScans && diagOrdinal != selectedScanOrdinal)
            continue;

          ImU32 lineColor = IM_COL32(255, 230, 120, 115);
          float lineThickness = 1.0f;
          if (diag.accepted) {
            lineColor = IM_COL32(255, 235, 64, 240);
            lineThickness = 1.6f;
          } else if (!diag.reject_reason.empty() &&
                     diag.reject_reason != "no_threshold_crossing") {
            lineColor = IM_COL32(255, 96, 96, 170);
          }

          drawActualScanLine(diag.scan_type, diag.scan_index, a, b,
                             lineColor, lineThickness, diagOrdinal);

          if (diag.accepted) {
            int conclusionIndex = 0;
            auto drawConclusionPoint = [&](double x, double y) {
              const ImVec2 ap = ImageToScreenD(x, y);
              drawList->AddCircleFilled(ap, 4.5f, IM_COL32(255, 128, 48, 255), 16);
              drawList->AddCircle(ap, 6.0f, IM_COL32(90, 35, 0, 230), 16, 1.2f);
              drawList->AddLine(ImVec2(ap.x - 7.0f, ap.y), ImVec2(ap.x + 7.0f, ap.y),
                                IM_COL32(255, 230, 180, 255), 1.3f);
              drawList->AddLine(ImVec2(ap.x, ap.y - 7.0f), ImVec2(ap.x, ap.y + 7.0f),
                                IM_COL32(255, 230, 180, 255), 1.3f);
              if (diagOrdinal == selectedScanOrdinal) {
                const std::string label =
                    "Conclusion " + std::to_string(++conclusionIndex);
                drawList->AddText(ImVec2(ap.x + 8.0f, ap.y + 8.0f),
                                  IM_COL32(255, 178, 118, 255), label.c_str());
              }
            };

            bool usedMultiPoint = false;
            for (std::size_t p = 0; p + 1 < diag.accepted_points_xy.size(); p += 2) {
              drawConclusionPoint(diag.accepted_points_xy[p],
                                  diag.accepted_points_xy[p + 1]);
              usedMultiPoint = true;
            }
            if (!usedMultiPoint)
              drawConclusionPoint(diag.accepted_x, diag.accepted_y);
          }

        }
      }
    }
  }

  if (m_manualTest.show_circle_gauge_scan_lines ||
      (m_manualTest.metrology_ui.enabled &&
       m_manualTest.metrology_ui.show_scan_profile &&
       (m_manualTest.current_gauge.tool == "FindCircle" ||
        m_manualTest.current_gauge.has_circle_gauge))) {
    std::string ownerRef;
    FindCircle *circleTool = nullptr;
    std::vector<std::string> ownerCandidates;
    auto addOwnerCandidate = [&](const std::string &name) {
      if (!name.empty() && std::find(ownerCandidates.begin(),
                                     ownerCandidates.end(), name) ==
                               ownerCandidates.end())
        ownerCandidates.push_back(name);
    };
    addOwnerCandidate(m_manualTest.current_result_ref.source_object);
    addOwnerCandidate(m_manualTest.current_gauge.primary_object_name);
    for (const RuntimeObjectView &object : m_manualTest.runtime_objects) {
      if (object.type == "FindCircle")
        addOwnerCandidate(object.name);
    }
    for (const std::string &candidate : ownerCandidates) {
      circleTool = static_cast<FindCircle *>(
          m_parserDebugBridge.QueryClassObject("FindCircle", candidate));
      if (circleTool != nullptr) {
        ownerRef = candidate;
        break;
      }
    }
    auto ImageToScreenD = [&](double x, double y) -> ImVec2 {
      return ImVec2(imagePos.x + static_cast<float>(x) * sx,
                    imagePos.y + static_cast<float>(y) * sy);
    };


    auto drawArrowHead = [&](CxShapePoint from, CxShapePoint to,
                             ImU32 arrowColor) {
      const ImVec2 pFrom = ImageToScreenD(from.x, from.y);
      const ImVec2 pTo = ImageToScreenD(to.x, to.y);
      const float vx = pTo.x - pFrom.x;
      const float vy = pTo.y - pFrom.y;
      const float vlen = std::sqrt(vx * vx + vy * vy);
      if (vlen < 1.0f)
        return;

      const float ux = vx / vlen;
      const float uy = vy / vlen;
      const float arrowLen = 7.0f;
      const float arrowHalf = 4.0f;
      const ImVec2 base(pTo.x - ux * arrowLen, pTo.y - uy * arrowLen);
      const ImVec2 left(base.x - uy * arrowHalf, base.y + ux * arrowHalf);
      const ImVec2 right(base.x + uy * arrowHalf, base.y - ux * arrowHalf);

      drawList->AddLine(pTo, left, arrowColor, 1.8f);
      drawList->AddLine(pTo, right, arrowColor, 1.8f);
    };

    if (circleTool != nullptr) {
      const bool drawAllCircleGaugeScans =
          m_manualTest.show_circle_gauge_scan_lines;
      const FindCircleMeasureGeometryDebug &debug =
          circleTool->lastmeasuregeometrydebug();
      const int scanCount =
          std::max(debug.scan_line_count, circleTool->getscanlinecount());
      const int diagnosticCount = circleTool->getscandiagnosticcount();
      const int selectableScanCount = std::max(scanCount, diagnosticCount);
      const int selectedScanIndex =
          selectableScanCount > 0
              ? std::max(0, std::min(selectableScanCount - 1,
                                     m_manualTest.metrology_ui.gauge_line_num -
                                         1))
              : -1;
      const std::string selectedScanLabel =
          "Gauge Line " + std::to_string(selectedScanIndex + 1);
      const int arrowStride = std::max(2, std::max(1, selectableScanCount) / 32);


      auto drawProfileCursorPoint = [&](const CxShapePoint &a,
                                        const CxShapePoint &b) {
        const ManualMetrologyUiState &metrology = m_manualTest.metrology_ui;
        if (!metrology.profile_cursor_visible ||
            metrology.profile_cursor_sample_count < 2)
          return;
        const int cursorIndex = std::max(
            0, std::min(metrology.profile_cursor_sample_count - 1,
                        metrology.profile_cursor_sample_index));
        const double t = static_cast<double>(cursorIndex) /
                         static_cast<double>(metrology.profile_cursor_sample_count - 1);
        const CxShapePoint point{a.x + (b.x - a.x) * t,
                                 a.y + (b.y - a.y) * t};
        const ImVec2 p = ImageToScreenD(point.x, point.y);
        drawList->AddCircleFilled(p, 5.0f, IM_COL32(255, 210, 64, 255), 18);
        drawList->AddCircle(p, 7.0f, IM_COL32(30, 22, 0, 240), 18, 1.5f);
        drawList->AddLine(ImVec2(p.x - 8.0f, p.y), ImVec2(p.x + 8.0f, p.y),
                          IM_COL32(255, 245, 190, 255), 1.5f);
        drawList->AddLine(ImVec2(p.x, p.y - 8.0f), ImVec2(p.x, p.y + 8.0f),
                          IM_COL32(255, 245, 190, 255), 1.5f);
        const std::string label =
            std::string(metrology.profile_cursor_user_dragged ? "Manual cursor "
                                                              : "Preview cursor ") +
            std::to_string(metrology.profile_cursor_permille) + " /1000";
        drawList->AddText(ImVec2(p.x + 8.0f, p.y + 8.0f),
                          IM_COL32(255, 226, 110, 255), label.c_str());
      };


      auto drawCircleScanLine = [&](const CxShapePoint &a,
                                    const CxShapePoint &b, ImU32 color,
                                    float thickness, int ordinal) {
        const bool selected = ordinal == selectedScanIndex;
        const ImU32 drawColor =
            selected ? IM_COL32(255, 64, 210, 255) : color;
        const float drawThickness = selected ? std::max(3.0f, thickness)
                                             : thickness;
        const ImVec2 pa = ImageToScreenD(a.x, a.y);
        const ImVec2 pb = ImageToScreenD(b.x, b.y);
        drawList->AddLine(pa, pb, drawColor, drawThickness);

        if (selected) {
          const ImVec2 mid((pa.x + pb.x) * 0.5f + 6.0f,
                           (pa.y + pb.y) * 0.5f + 6.0f);
          drawList->AddCircleFilled(pa, 3.8f, drawColor, 12);
          drawList->AddCircleFilled(pb, 3.8f, drawColor, 12);
          drawList->AddText(mid, drawColor, selectedScanLabel.c_str());
          drawArrowHead(a, b, drawColor);

          drawProfileCursorPoint(a, b);

        } else if (drawAllCircleGaugeScans && (ordinal % arrowStride) == 0) {
          drawArrowHead(a, b, IM_COL32(140, 230, 255, 175));
        }
      };

      if (diagnosticCount > 0) {
        for (int i = 0; i < diagnosticCount; ++i) {
          FindCircleMeasureGeometryDebug::ScanDiagnostic diag;
          CxShapePoint a, b;
          if (!circleTool->getscandiagnostic(i, diag) ||
              !circleTool->getscandiagnosticline(diag.scan_index, a, b)) {
            continue;
          }
          if (!drawAllCircleGaugeScans && diag.scan_index != selectedScanIndex)
            continue;

          ImU32 lineColor = IM_COL32(140, 230, 255, 105);
          float lineThickness = 1.0f;
          if (diag.accepted) {
            lineColor = IM_COL32(255, 235, 64, 210);
            lineThickness = 1.4f;
          } else if (!diag.reject_reason.empty() &&
                     diag.reject_reason != "no_candidate") {
            lineColor = IM_COL32(255, 96, 96, 150);
          }

          drawCircleScanLine(a, b, lineColor, lineThickness, diag.scan_index);

          if (diag.accepted) {
            int conclusionIndex = 0;
            auto drawConclusionPoint = [&](double x, double y) {
              const ImVec2 ap = ImageToScreenD(x, y);
              drawList->AddCircleFilled(ap, 4.5f, IM_COL32(255, 128, 48, 255), 16);
              drawList->AddCircle(ap, 6.0f, IM_COL32(90, 35, 0, 230), 16, 1.2f);
              drawList->AddLine(ImVec2(ap.x - 7.0f, ap.y), ImVec2(ap.x + 7.0f, ap.y),
                                IM_COL32(255, 230, 180, 255), 1.3f);
              drawList->AddLine(ImVec2(ap.x, ap.y - 7.0f), ImVec2(ap.x, ap.y + 7.0f),
                                IM_COL32(255, 230, 180, 255), 1.3f);
              if (diag.scan_index == selectedScanIndex) {
                const std::string label =
                    "Conclusion " + std::to_string(++conclusionIndex);
                drawList->AddText(ImVec2(ap.x + 8.0f, ap.y + 8.0f),
                                  IM_COL32(255, 178, 118, 255), label.c_str());
              }
            };

            bool usedMultiPoint = false;
            for (std::size_t p = 0; p + 1 < diag.accepted_points_xy.size(); p += 2) {
              drawConclusionPoint(diag.accepted_points_xy[p],
                                  diag.accepted_points_xy[p + 1]);
              usedMultiPoint = true;
            }
            if (!usedMultiPoint)
              drawConclusionPoint(diag.accepted_x, diag.accepted_y);
          }
        }
      } else {
        for (int scanIndex = 0; scanIndex < scanCount; ++scanIndex) {
          CxShapePoint a, b;
          if (!circleTool->getscanline(scanIndex, a, b))
            continue;
          if (!drawAllCircleGaugeScans && scanIndex != selectedScanIndex)
            continue;

          drawCircleScanLine(a, b, IM_COL32(140, 230, 255, 105), 1.0f,
                             scanIndex);
        }
      }
      const FindCircleBoundaryAnalysisSnapshot precision =
          circleTool->boundaryanalysissnapshot();
      for (const FindCircleBoundaryPointSnapshot &point : precision.points) {
        if (!point.interpolation_valid ||
            !std::isfinite(point.measured_x) ||
            !std::isfinite(point.measured_y) ||
            !std::isfinite(point.refined_x) ||
            !std::isfinite(point.refined_y)) {
          continue;
        }

        ImU32 precisionColor = IM_COL32(80, 210, 255, 245);
        if (point.fit_residual_valid) {
          if (point.fit_residual_px <= 0.5)
            precisionColor = IM_COL32(70, 235, 135, 245);
          else if (point.fit_residual_px <= 1.0)
            precisionColor = IM_COL32(255, 190, 60, 245);
          else
            precisionColor = IM_COL32(255, 85, 85, 245);
        }

        const ImVec2 raw =
            ImageToScreenD(point.measured_x, point.measured_y);
        const ImVec2 refined =
            ImageToScreenD(point.refined_x, point.refined_y);
        const float correctionLength =
            std::hypot(refined.x - raw.x, refined.y - raw.y);
        if (correctionLength > 0.25f)
          drawList->AddLine(raw, refined, precisionColor, 1.4f);

        drawList->AddCircle(refined, 5.5f, precisionColor, 14, 1.6f);
        drawList->AddCircleFilled(refined, 1.5f, precisionColor);
      }
    }
  }
  if (m_manualTest.show_ellipse_gauge_scan_lines ||
      (m_manualTest.metrology_ui.enabled &&
       m_manualTest.metrology_ui.show_scan_profile &&
       (m_manualTest.current_gauge.tool == "FindEllipse" ||
        m_manualTest.current_gauge.has_ellipse_gauge))) {
    std::string ownerRef;
    FindEllipse *ellipseTool = nullptr;
    std::vector<std::string> ownerCandidates;
    auto addOwnerCandidate = [&](const std::string &name) {
      if (!name.empty() && std::find(ownerCandidates.begin(),
                                     ownerCandidates.end(), name) ==
                               ownerCandidates.end())
        ownerCandidates.push_back(name);
    };
    addOwnerCandidate(m_manualTest.current_result_ref.source_object);
    addOwnerCandidate(m_manualTest.current_gauge.primary_object_name);
    for (const RuntimeObjectView &object : m_manualTest.runtime_objects) {
      if (object.type == "FindEllipse")
        addOwnerCandidate(object.name);
    }
    for (const std::string &candidate : ownerCandidates) {
      ellipseTool = static_cast<FindEllipse *>(
          m_parserDebugBridge.QueryClassObject("FindEllipse", candidate));
      if (ellipseTool != nullptr) {
        ownerRef = candidate;
        break;
      }
    }

    auto ImageToScreenD = [&](double x, double y) -> ImVec2 {
      return ImVec2(imagePos.x + static_cast<float>(x) * sx,
                    imagePos.y + static_cast<float>(y) * sy);
    };

    auto drawArrowHead = [&](CxShapePoint from, CxShapePoint to,
                             ImU32 arrowColor) {
      const ImVec2 pFrom = ImageToScreenD(from.x, from.y);
      const ImVec2 pTo = ImageToScreenD(to.x, to.y);
      const float vx = pTo.x - pFrom.x;
      const float vy = pTo.y - pFrom.y;
      const float vlen = std::sqrt(vx * vx + vy * vy);
      if (vlen < 1.0f)
        return;

      const float ux = vx / vlen;
      const float uy = vy / vlen;
      const float arrowLen = 7.0f;
      const float arrowHalf = 4.0f;
      const ImVec2 base(pTo.x - ux * arrowLen, pTo.y - uy * arrowLen);
      const ImVec2 left(base.x - uy * arrowHalf, base.y + ux * arrowHalf);
      const ImVec2 right(base.x + uy * arrowHalf, base.y - ux * arrowHalf);
      drawList->AddLine(pTo, left, arrowColor, 1.8f);
      drawList->AddLine(pTo, right, arrowColor, 1.8f);
    };

    if (ellipseTool != nullptr) {
      const bool drawAllEllipseGaugeScans =
          m_manualTest.show_ellipse_gauge_scan_lines;
      const int scanCount = ellipseTool->getscanlinecount();
      const int diagnosticCount = ellipseTool->getscandiagnosticcount();
      const int selectableScanCount = std::max(scanCount, diagnosticCount);
      const int selectedScanIndex =
          selectableScanCount > 0
              ? std::max(0, std::min(selectableScanCount - 1,
                                     m_manualTest.metrology_ui.gauge_line_num -
                                         1))
              : -1;
      const std::string selectedScanLabel =
          "Gauge Line " + std::to_string(selectedScanIndex + 1);
      const int arrowStride = std::max(2, std::max(1, selectableScanCount) / 32);

      auto drawProfileCursorPoint = [&](const CxShapePoint &a,
                                        const CxShapePoint &b) {
        const ManualMetrologyUiState &metrology = m_manualTest.metrology_ui;
        if (!metrology.profile_cursor_visible ||
            metrology.profile_cursor_sample_count < 2)
          return;
        const int cursorIndex = std::max(
            0, std::min(metrology.profile_cursor_sample_count - 1,
                        metrology.profile_cursor_sample_index));
        const double t = static_cast<double>(cursorIndex) /
                         static_cast<double>(metrology.profile_cursor_sample_count - 1);
        const CxShapePoint point{a.x + (b.x - a.x) * t,
                                 a.y + (b.y - a.y) * t};
        const ImVec2 p = ImageToScreenD(point.x, point.y);
        drawList->AddCircleFilled(p, 5.0f, IM_COL32(255, 210, 64, 255), 18);
        drawList->AddCircle(p, 7.0f, IM_COL32(30, 22, 0, 240), 18, 1.5f);
        drawList->AddLine(ImVec2(p.x - 8.0f, p.y), ImVec2(p.x + 8.0f, p.y),
                          IM_COL32(255, 245, 190, 255), 1.5f);
        drawList->AddLine(ImVec2(p.x, p.y - 8.0f), ImVec2(p.x, p.y + 8.0f),
                          IM_COL32(255, 245, 190, 255), 1.5f);
        const std::string label =
            std::string(metrology.profile_cursor_user_dragged ? "Manual cursor "
                                                              : "Preview cursor ") +
            std::to_string(metrology.profile_cursor_permille) + " /1000";
        drawList->AddText(ImVec2(p.x + 8.0f, p.y + 8.0f),
                          IM_COL32(255, 226, 110, 255), label.c_str());
      };

      auto drawEllipseScanLine = [&](const CxShapePoint &a,
                                     const CxShapePoint &b, ImU32 color,
                                     float thickness, int ordinal) {
        const bool selected = ordinal == selectedScanIndex;
        const ImU32 drawColor =
            selected ? IM_COL32(255, 64, 210, 255) : color;
        const float drawThickness = selected ? std::max(3.0f, thickness)
                                             : thickness;
        const ImVec2 pa = ImageToScreenD(a.x, a.y);
        const ImVec2 pb = ImageToScreenD(b.x, b.y);
        drawList->AddLine(pa, pb, drawColor, drawThickness);

        if (selected) {
          const ImVec2 mid((pa.x + pb.x) * 0.5f + 6.0f,
                           (pa.y + pb.y) * 0.5f + 6.0f);
          drawList->AddCircleFilled(pa, 3.8f, drawColor, 12);
          drawList->AddCircleFilled(pb, 3.8f, drawColor, 12);
          drawList->AddText(mid, drawColor, selectedScanLabel.c_str());
          drawArrowHead(a, b, drawColor);
          drawProfileCursorPoint(a, b);
        } else if (drawAllEllipseGaugeScans && (ordinal % arrowStride) == 0) {
          drawArrowHead(a, b, IM_COL32(140, 230, 255, 175));
        }
      };

      if (diagnosticCount > 0) {
        for (int i = 0; i < diagnosticCount; ++i) {
          FindEllipseMeasureGeometryDebug::ScanDiagnostic diag;
          CxShapePoint a, b;
          if (!ellipseTool->getscandiagnostic(i, diag) ||
              !ellipseTool->getscandiagnosticline(diag.scan_index, a, b)) {
            continue;
          }
          if (!drawAllEllipseGaugeScans && diag.scan_index != selectedScanIndex)
            continue;

          ImU32 lineColor = IM_COL32(140, 230, 255, 105);
          float lineThickness = 1.0f;
          if (diag.accepted) {
            lineColor = IM_COL32(255, 235, 64, 210);
            lineThickness = 1.4f;
          } else if (!diag.reject_reason.empty() &&
                     diag.reject_reason != "no_candidate") {
            lineColor = IM_COL32(255, 96, 96, 150);
          }

          drawEllipseScanLine(a, b, lineColor, lineThickness, diag.scan_index);

          if (diag.accepted) {
            int conclusionIndex = 0;
            auto drawConclusionPoint = [&](double x, double y) {
              const ImVec2 ap = ImageToScreenD(x, y);
              drawList->AddCircleFilled(ap, 4.5f, IM_COL32(255, 128, 48, 255), 16);
              drawList->AddCircle(ap, 6.0f, IM_COL32(90, 35, 0, 230), 16, 1.2f);
              drawList->AddLine(ImVec2(ap.x - 7.0f, ap.y), ImVec2(ap.x + 7.0f, ap.y),
                                IM_COL32(255, 230, 180, 255), 1.3f);
              drawList->AddLine(ImVec2(ap.x, ap.y - 7.0f), ImVec2(ap.x, ap.y + 7.0f),
                                IM_COL32(255, 230, 180, 255), 1.3f);
              if (diag.scan_index == selectedScanIndex) {
                const std::string label =
                    "Conclusion " + std::to_string(++conclusionIndex);
                drawList->AddText(ImVec2(ap.x + 8.0f, ap.y + 8.0f),
                                  IM_COL32(255, 178, 118, 255), label.c_str());
              }
            };

            bool usedMultiPoint = false;
            for (std::size_t p = 0; p + 1 < diag.accepted_points_xy.size(); p += 2) {
              drawConclusionPoint(diag.accepted_points_xy[p],
                                  diag.accepted_points_xy[p + 1]);
              usedMultiPoint = true;
            }
            if (!usedMultiPoint)
              drawConclusionPoint(diag.accepted_x, diag.accepted_y);
          }
        }
      } else {
        for (int scanIndex = 0; scanIndex < scanCount; ++scanIndex) {
          CxShapePoint a, b;
          if (!ellipseTool->getscanline(scanIndex, a, b))
            continue;
          if (!drawAllEllipseGaugeScans && scanIndex != selectedScanIndex)
            continue;

          drawEllipseScanLine(a, b, IM_COL32(140, 230, 255, 105), 1.0f,
                              scanIndex);
        }
      }
    }
  }
  if (m_showTestPoints) {
    const ImU32 color = IM_COL32(255, 64, 64, 255);
    const ImVec2 points[] = {imagePos, ImVec2(imageEnd.x, imagePos.y),
                             ImVec2(imagePos.x, imageEnd.y), imageEnd,
                             ImVec2((imagePos.x + imageEnd.x) * 0.5f,
                                    (imagePos.y + imageEnd.y) * 0.5f)};
    for (const ImVec2 &point : points) {
      drawList->AddLine(ImVec2(point.x - 6.0f, point.y),
                        ImVec2(point.x + 6.0f, point.y), color, 2.0f);
      drawList->AddLine(ImVec2(point.x, point.y - 6.0f),
                        ImVec2(point.x, point.y + 6.0f), color, 2.0f);
    }
  }
  if (m_showTestRectangle) {
    drawList->AddRect(ImVec2(imagePos.x + imageSize.x * 0.2f,
                             imagePos.y + imageSize.y * 0.2f),
                      ImVec2(imagePos.x + imageSize.x * 0.7f,
                             imagePos.y + imageSize.y * 0.7f),
                      IM_COL32(64, 255, 64, 255), 0.0f, 0, 2.0f);
  }
  if (m_showTestScanLine) {
    const float y = imagePos.y + imageSize.y * 0.5f;
    drawList->AddLine(ImVec2(imagePos.x, y), ImVec2(imageEnd.x, y),
                      IM_COL32(64, 160, 255, 255), 2.0f);
  }
  drawList->PopClipRect();

  const bool mouseOnImage = canvasHovered && io.MousePos.x >= imagePos.x &&
                            io.MousePos.x < imageEnd.x &&
                            io.MousePos.y >= imagePos.y &&
                            io.MousePos.y < imageEnd.y;
  if (mouseOnImage) {
    const int imageX = static_cast<int>((io.MousePos.x - imagePos.x) *
                                        m_imageViewImage.cols / imageSize.x);
    const int imageY = static_cast<int>((io.MousePos.y - imagePos.y) *
                                        m_imageViewImage.rows / imageSize.y);
    ImGui::SetTooltip("image coordinate: %d, %d", imageX, imageY);
  }
  ImGui::TextWrapped("image_ref: %s", emptyAsNone(m_scriptResult.image_ref));
  ImGui::TextWrapped("overlay_ref: %s",
                     emptyAsNone(m_scriptResult.overlay_ref));
  ImGui::Text("result_attach_status: %s",
              m_scriptResult.result_ref.empty() ? "not_attached" : "attached");
  ImGui::End();

  ImGui::SetNextWindowPos(ImVec2(rightX, margin), layoutCondition);
  ImGui::SetNextWindowSize(ImVec2(rightWidth, runHeight), layoutCondition);
  ImGui::Begin("Run Control", nullptr, panelFlags);
  ImGui::Text("Options");
  ImGui::Checkbox("Detachable panels (inside host)", &m_detachablePanels);
  ImGui::SameLine();
  ImGui::Checkbox("Manual State Test Console", &m_showManualStateTestConsole);
  ImGui::SameLine();
  ImGui::Checkbox("Show legacy GPU work (debug)", &m_showLegacyGpuWork);
  ImGui::Checkbox("Business Workflow Acceptance Analysis",
                  &m_showBusinessWorkflowPanel);
  if (m_selectedScript >= 0 &&
      m_selectedScript < static_cast<int>(m_scriptCatalog.size()))
    ImGui::TextWrapped("Selected: %s",
                       m_scriptCatalog[m_selectedScript].path.c_str());
  else
    ImGui::TextDisabled("Selected: none");
  if (ImGui::Button("Run Selected Script") && m_selectedScript >= 0) {
    const ScriptCatalogEntry &selectedEntry =
        m_scriptCatalog[static_cast<std::size_t>(m_selectedScript)];

    std::string selectedText;
    if (ReadTextFile(selectedEntry.path, selectedText)) {
      m_manualTest.editor_text = selectedText;
      m_manualTest.loaded_script_path = selectedEntry.path;
      m_manualTest.script_file_path = selectedEntry.path;
      m_manualTest.editor_source = "run_control_selected_script";
      m_manualTest.editor_dirty = false;
      m_manualTest.active_script_case_name = selectedEntry.name;
      m_manualTest.active_script_case_path = selectedEntry.path;
      m_manualTest.active_script_case_purpose = "run_control_selected_script";
      SeedDefaultManualGlobals(m_manualTest, selectedEntry.path);
    } else {
      m_manualTest.debug_status = "RUN_SELECTED_SCRIPT_LOAD_FAIL";
      m_manualTest.debug_reason =
          "failed to load selected script before run: " + selectedEntry.path;
    }

    m_scriptRunRequested = true;
    m_scriptResult = RunCxScript(selectedEntry.path);
    RefreshRuntimeObjectTable(
        "Run Selected Script",
        m_scriptResult.status == "BLOCKED" ? "BLOCKED" : "runtime_executed");
    m_scriptRunRequested = false;
  }
  ImGui::SameLine();
  if (ImGui::Button("Stop")) {
    m_scriptRunRequested = false;
    m_scriptResult.status = "PENDING";
    m_scriptResult.reason = "run stopped by user";
    m_scriptResult.runtime_fillback_status = "stopped";
    m_scriptResult.log_lines.push_back("Stop requested.");
  }
  ImGui::SameLine();
  if (ImGui::Button("Clear Result")) {
    m_scriptResult = ScriptResult();
    m_scriptResult.status = "PENDING";
    m_scriptResult.reason = "result cleared";
    m_scriptResult.runtime_fillback_status = "not_started";
  }
  ImGui::Text("Run state: %s", m_scriptRunRequested ? "running" : "idle");
  ImGui::End();

  ImGui::SetNextWindowPos(ImVec2(rightX, margin + runHeight + margin),
                          layoutCondition);
  ImGui::SetNextWindowSize(ImVec2(rightWidth, contentHeight * 0.25f),
                           layoutCondition);
  ImGui::Begin("Result / Log", nullptr, panelFlags);
  ImGui::TextWrapped("Source: %s", emptyAsNone(m_scriptResult.source));
  ImGui::TextWrapped("Script: %s", emptyAsNone(m_scriptResult.script_path));
  ImGui::Text("Status: %s", emptyAsNone(m_scriptResult.status));
  ImGui::TextWrapped("Reason: %s", emptyAsNone(m_scriptResult.reason));
  ImGui::TextWrapped("runtime_fillback_status: %s",
                     emptyAsNone(m_scriptResult.runtime_fillback_status));
  ImGui::Text("elapsed_ms: %.3f", m_scriptResult.elapsed_ms);
  ImGui::TextWrapped("image_ref: %s", emptyAsNone(m_scriptResult.image_ref));
  ImGui::TextWrapped("overlay_ref: %s",
                     emptyAsNone(m_scriptResult.overlay_ref));
  ImGui::TextWrapped("result_ref: %s", emptyAsNone(m_scriptResult.result_ref));
  ImGui::TextWrapped("evidence_ref: %s",
                     emptyAsNone(m_scriptResult.evidence_ref));
  ImGui::TextWrapped("issue_entry_ref: %s",
                     emptyAsNone(m_scriptResult.issue_entry_ref));
  ImGui::Separator();
  ImGui::Text("Log:");
  for (const std::string &line : m_scriptResult.log_lines)
    ImGui::BulletText("%s", line.c_str());
  ImGui::End();

  ImGui::SetNextWindowPos(ImVec2(rightX, margin + runHeight + margin +
                                             contentHeight * 0.25f + margin),
                          layoutCondition);
  ImGui::SetNextWindowSize(ImVec2(rightWidth, contentHeight * 0.45f),
                           layoutCondition);
  ImGui::Begin("Gauge Workbench", nullptr, panelFlags);

  ManualGaugeState &gauge = m_manualTest.current_gauge;

  ImGui::Text("=== Current Evidence ===");
  ImGui::Text("case_id: %s", emptyAsNone(gauge.case_id));
  ImGui::Text("image_id: %s", emptyAsNone(gauge.image_id));
  ImGui::Text("target_id: %s", emptyAsNone(gauge.target_id));
  ImGui::Text("tool: %s", emptyAsNone(gauge.tool));
  ImGui::Text("source: %s", emptyAsNone(gauge.source));
  ImGui::Text("review_status: %s", emptyAsNone(gauge.review_status));
  ImGui::Text("dirty: %s | accepted: %s", gauge.dirty ? "yes" : "no",
              gauge.accepted ? "yes" : "no");
  ImGui::Separator();

  ImGui::Text("=== Current Gauge ===");
  if (gauge.has_line_gauge) {
    if (ImGui::InputInt("line_x0", &gauge.line_x0)) {
      gauge.dirty = true;
      gauge.review_status = "editing";
    }
    if (ImGui::InputInt("line_y0", &gauge.line_y0)) {
      gauge.dirty = true;
      gauge.review_status = "editing";
    }
    if (ImGui::InputInt("line_x1", &gauge.line_x1)) {
      gauge.dirty = true;
      gauge.review_status = "editing";
    }
    if (ImGui::InputInt("line_y1", &gauge.line_y1)) {
      gauge.dirty = true;
      gauge.review_status = "editing";
    }
    if (ImGui::InputInt("tool_half_width", &gauge.tool_half_width)) {
      gauge.dirty = true;
      gauge.review_status = "editing";
    }
    if (ImGui::InputInt("wgap", &gauge.wgap)) {
      gauge.dirty = true;
      gauge.review_status = "editing";
    }
    if (ImGui::InputInt("hgap", &gauge.hgap)) {
      gauge.dirty = true;
      gauge.review_status = "editing";
    }
    if (ImGui::InputInt("linegap", &gauge.linegap)) {
      gauge.dirty = true;
      gauge.review_status = "editing";
    }
    if (ImGui::InputInt("threshold", &gauge.threshold)) {
      gauge.dirty = true;
      gauge.review_status = "editing";
    }
    if (ImGui::InputInt("filterprofile", &gauge.filterprofile)) {
      gauge.dirty = true;
      gauge.review_status = "editing";
    }
    if (ImGui::InputInt("method", &gauge.method)) {
      gauge.dirty = true;
      gauge.review_status = "editing";
    }
  } else if (gauge.has_circle_gauge) {
    if (ImGui::InputInt("circle_cx", &gauge.circle_cx)) {
      gauge.dirty = true;
      gauge.review_status = "editing";
    }
    if (ImGui::InputInt("circle_cy", &gauge.circle_cy)) {
      gauge.dirty = true;
      gauge.review_status = "editing";
    }
    if (ImGui::InputInt("circle_px", &gauge.circle_px)) {
      gauge.dirty = true;
      gauge.review_status = "editing";
    }
    if (ImGui::InputInt("circle_py", &gauge.circle_py)) {
      gauge.dirty = true;
      gauge.review_status = "editing";
    }
    if (ImGui::InputInt("radius", &gauge.radius)) {
      gauge.dirty = true;
      gauge.review_status = "editing";
    }
    if (ImGui::InputInt("inner_radius", &gauge.inner_radius)) {
      gauge.dirty = true;
      gauge.review_status = "editing";
    }
    if (ImGui::InputInt("outer_radius", &gauge.outer_radius)) {
      gauge.dirty = true;
      gauge.review_status = "editing";
    }
    if (ImGui::InputInt("gap", &gauge.gap)) {
      gauge.dirty = true;
      gauge.review_status = "editing";
    }
    if (ImGui::InputInt("linegap", &gauge.linegap)) {
      gauge.dirty = true;
      gauge.review_status = "editing";
    }
    if (ImGui::InputInt("threshold", &gauge.threshold)) {
      gauge.dirty = true;
      gauge.review_status = "editing";
    }
    if (ImGui::InputInt("method", &gauge.method)) {
      gauge.dirty = true;
      gauge.review_status = "editing";
    }
  } else if (gauge.has_ellipse_gauge) {
    if (ImGui::InputInt("ellipse_x0", &gauge.ellipse_x0)) {
      gauge.dirty = true;
      gauge.review_status = "editing";
    }
    if (ImGui::InputInt("ellipse_y0", &gauge.ellipse_y0)) {
      gauge.dirty = true;
      gauge.review_status = "editing";
    }
    if (ImGui::InputInt("ellipse_x1", &gauge.ellipse_x1)) {
      gauge.dirty = true;
      gauge.review_status = "editing";
    }
    if (ImGui::InputInt("ellipse_y1", &gauge.ellipse_y1)) {
      gauge.dirty = true;
      gauge.review_status = "editing";
    }
    if (ImGui::InputInt("inner_scale_percent",
                        &gauge.ellipse_inner_scale_percent)) {
      gauge.ellipse_inner_scale_percent =
          std::max(0, std::min(99, gauge.ellipse_inner_scale_percent));
      gauge.dirty = true;
      gauge.review_status = "editing";
    }
    if (ImGui::InputInt("gap", &gauge.gap)) {
      gauge.dirty = true;
      gauge.review_status = "editing";
    }
    if (ImGui::InputInt("linegap", &gauge.linegap)) {
      gauge.dirty = true;
      gauge.review_status = "editing";
    }
    if (ImGui::InputInt("threshold", &gauge.threshold)) {
      gauge.dirty = true;
      gauge.review_status = "editing";
    }
    if (ImGui::InputInt("method", &gauge.method)) {
      gauge.dirty = true;
      gauge.review_status = "editing";
    }
    ImGui::TextDisabled("fitellipse: pending_binding; current runtime exposes "
                        "ROI and measure points only.");
  } else {
    ImGui::TextDisabled(
        "No gauge active. Run a script or select from catalog.");
  }
  ImGui::Separator();

  ImGui::Text("=== Actions ===");
  if (ImGui::Button("Apply Gauge To Globals")) {
    std::string promptSyncReason;
    SyncFindSegmentationPromptListsFromShapeElements(promptSyncReason);
    ApplyManualGaugeToGlobals(m_manualTest);
  }
  ImGui::SameLine();
  if (ImGui::Button("Accept Manual Gauge")) {
    std::string reason;
    if (ValidateManualGaugeGeometryForEditing(gauge, reason)) {
      NormalizeManualGaugeGeometry(gauge);
      gauge.accepted = true;
      gauge.dirty = false;
      gauge.review_status = "manual_accepted";
      m_manualTest.debug_status = "gauge_accepted";
      m_manualTest.debug_reason.clear();
    } else {
      gauge.accepted = false;
      m_manualTest.debug_status = "gauge_accept_failed";
      m_manualTest.debug_reason = reason;
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Save Gauge Annotation")) {
    std::string promptSyncReason;
    SyncFindSegmentationPromptListsFromShapeElements(promptSyncReason);
    std::string path;
    std::string reason;
    if (SaveManualGaugeAnnotation(m_manualTest, "", "", path, reason)) {
      m_manualTest.debug_status = "gauge_annotation_saved";
      m_manualTest.debug_reason = path;
    } else {
      m_manualTest.debug_status = "gauge_annotation_save_failed";
      m_manualTest.debug_reason = reason;
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Save Evidence Review")) {
    std::string path;
    std::string reason;
    if (saveLockedEvidenceReview(m_manualTest, path, reason)) {
      m_manualTest.debug_status = "evidence_review_saved";
      m_manualTest.debug_reason = path;
    } else {
      m_manualTest.debug_status = "evidence_review_save_failed";
      m_manualTest.debug_reason = reason;
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Reset From Runtime")) {
    gauge.has_line_gauge = false;
    gauge.has_circle_gauge = false;
    gauge.has_ellipse_gauge = false;
    gauge.dirty = false;
    gauge.accepted = false;
  }
  ImGui::Separator();

  ImGui::Text("=== Param Regression ===");
  if (!gauge.accepted) {
    ImGui::TextDisabled(
        "Parameter regression disabled: manual gauge not accepted.");
  } else {
    ImGui::Text("ready for candidate generation");
  }
  ImGui::Button("Generate Candidates (pending_binding)");
  ImGui::SameLine();
  ImGui::Button("Run Selected Probe (pending_binding)");
  ImGui::SameLine();
  ImGui::Button("Export Candidate (pending_binding)");

  ImGui::End();
}
void ViewController::initWindow(int theWidth, int theHeight,
                                const char *theTitle) {
  glfwSetErrorCallback(ViewController::errorCallback);
#if defined(__linux__)
  const char *requestedPlatform = std::getenv("CXVISION_GLFW_PLATFORM");
  if (requestedPlatform != nullptr && *requestedPlatform != 0 &&
      std::string(requestedPlatform) != "auto") {
    if (std::string(requestedPlatform) == "x11") {
      glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
    } else if (std::string(requestedPlatform) == "wayland") {
      glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
    } else if (std::string(requestedPlatform) == "null") {
      glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_NULL);
    } else {
      throw std::runtime_error("Unsupported CXVISION_GLFW_PLATFORM.");
    }
    std::cerr << "[cxvision] GLFW platform=" << requestedPlatform << std::endl;
  }
#endif
  if (glfwInit() != GLFW_TRUE) {
    throw std::runtime_error(
        "GLFW initialization failed. OpenGL/WGL is unavailable on this "
        "machine; install a GPU driver, avoid Remote Desktop/basic display "
        "drivers, enable VM 3D acceleration, or run the non-GUI headless "
        "test path.");
  }
  m_glfwInitialized = true;

  const bool toAskCoreProfile = true;
  if (toAskCoreProfile) {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
#if defined(__APPLE__)
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  }

  // The operator console owns dense evidence, annotation, and training
  // surfaces. Start it maximized so those canonical panels have stable
  // screen coordinates from the first rendered frame.
  glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

  myOcctWindow = new Window(theWidth, theHeight, theTitle);
  GLFWwindow *glfwWindow =
      !myOcctWindow.IsNull() ? myOcctWindow->getGlfwWindow() : nullptr;
  if (glfwWindow == nullptr) {
    myOcctWindow.Nullify();
    glfwTerminate();
    m_glfwInitialized = false;
    throw std::runtime_error(
        "Failed to create GLFW/OpenGL window. Required OpenGL 3.3 WGL "
        "context is unavailable; install the vendor GPU driver, avoid "
        "Remote Desktop/basic display drivers, enable VM 3D acceleration, "
        "or run with the headless test path.");
  }

  glfwSetWindowUserPointer(glfwWindow, this);
  glfwSetWindowSizeCallback(glfwWindow, ViewController::onResizeCallback);
  glfwSetFramebufferSizeCallback(glfwWindow,
                                 ViewController::onFBResizeCallback);
  glfwSetScrollCallback(glfwWindow, ViewController::onMouseScrollCallback);
  glfwSetMouseButtonCallback(glfwWindow,
                             ViewController::onMouseButtonCallback);
  glfwSetCursorPosCallback(glfwWindow, ViewController::onMouseMoveCallback);

  glfwMakeContextCurrent(glfwWindow);
  glfwSwapInterval(1);

  if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress)) {
    cleanup();
    throw std::runtime_error(
        "Failed to initialize GLAD/OpenGL loader after creating the GLFW "
        "window. The active display driver may not expose a usable OpenGL "
        "context.");
  }

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  m_imguiContextInitialized = true;
  ImGuiIO &io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

  ImGui::StyleColorsDark();
  if (!ImGui_ImplGlfw_InitForOpenGL(glfwWindow, true)) {
    cleanup();
    throw std::runtime_error("Failed to initialize ImGui GLFW backend.");
  }
  m_imguiGlfwInitialized = true;

  const char *glsl_version = "#version 330";
  ImGui_ImplOpenGL3_Init(glsl_version);
  m_imguiOpenGlInitialized = true;

  ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
  (void)clear_color;
}

double ViewController::GetScale() {
  if (!m_myView.IsNull()) {
    m_myView->FitAll(0.1, false);
    return m_myView->Scale();
  } else {
    return 1;
  }
}

void ViewController::FitAll() {
  try {
    if (!m_myView.IsNull()) {
      m_myView->FitAll(0.1);
      m_myView->ZFitAll();
      m_myView->Update();
    }
  } catch (const Standard_Failure &) {
    return;
  }
};

void ViewController::ViewerUpDate() {}

void ViewController::DisplayShape(const Handle(AIS_InteractiveObject) & AShape,
                                  Standard_Boolean isShow) {
  (void)AShape;
  (void)isShow;
}

void ViewController::RemoveAllShapes(Standard_Boolean isUpDate) {
  (void)isUpDate;
}

#include <Graphic3d_Texture2Dmanual.hxx>
#include <Image_PixMap.hxx>
unsigned int ViewController::CreateTextureFromMat0(const cv::Mat &mat) {
  unsigned int textureID;
  glGenTextures(1, &textureID);

  glBindTexture(GL_TEXTURE_2D, textureID);

  //
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  cv::Mat img;
  if (mat.channels() == 3)
    cv::cvtColor(mat, img, cv::COLOR_BGR2RGB);
  else
    img = mat;

  GLenum format = (img.channels() == 1)   ? GL_RED
                  : (img.channels() == 3) ? GL_RGB
                                          : GL_RGBA;

  glTexImage2D(GL_TEXTURE_2D, 0, format, img.cols, img.rows, 0, format,
               GL_UNSIGNED_BYTE, img.data);
  glGenerateMipmap(GL_TEXTURE_2D);

  return textureID;
}

void ViewController::UpdateImageViewImage(const cv::Mat &image) {
  if (image.empty())
    return;
  if (!m_imageViewImage.empty() && m_imageViewImage.size() != image.size()) {
    m_annotationLayer.Clear();
    m_annotationStatus = "image size changed; annotation refs cleared";
  }
  image.copyTo(m_imageViewImage);
  if (!m_parserDebugBridge.StageGlobalMatInput(m_imageViewImage)) {
    m_manualTest.debug_status = "image_bind_failed";
    m_manualTest.debug_reason =
        "Image View loaded, but global_matInput staging failed";
  }
  if (m_imageViewTexture != 0) {
    glDeleteTextures(1, &m_imageViewTexture);
    m_imageViewTexture = 0;
  }
  m_imageViewTexture = CreateTextureFromMat0(m_imageViewImage);
}

bool ViewController::LoadImageIntoImageView(const std::string &imagePath,
                                            std::string &reason) {
  reason.clear();

  if (imagePath.empty()) {
    reason = "image path is empty";
    return false;
  }

  std::filesystem::path path = ResolveWorkspaceFile(imagePath);
  if (!std::filesystem::exists(path)) {
    reason = "image file not found: " + imagePath;
    return false;
  }

  cv::Mat image = cv::imread(path.string(), cv::IMREAD_COLOR);
  if (image.empty()) {
    reason = "failed to read image: " + path.string();
    return false;
  }

  UpdateImageViewImage(image);
  m_manualTest.image_file_path = path.string();
  m_scriptResult.image_ref = path.string();
  m_scriptResult.reason = "image loaded from evidence chain";
  m_annotationStatus = "image loaded from evidence chain";

  m_imageViewZoom = 1.0f;
  m_imageViewPanX = 0.0f;
  m_imageViewPanY = 0.0f;

  return true;
}

bool ViewController::LoadImageForEvidenceSelfTest(const std::string &imagePath,
                                                  std::string &reason) {
  reason.clear();

  if (imagePath.empty()) {
    reason = "image path is empty";
    return false;
  }

  std::filesystem::path path = ResolveWorkspaceFile(imagePath);
  if (!std::filesystem::exists(path)) {
    reason = "image file not found: " + imagePath;
    return false;
  }

  cv::Mat image = cv::imread(path.string(), cv::IMREAD_COLOR);
  if (image.empty()) {
    reason = "failed to read image: " + path.string();
    return false;
  }

  if (!m_imageViewImage.empty() && m_imageViewImage.size() != image.size()) {
    m_annotationLayer.Clear();
    m_annotationStatus = "image size changed; annotation refs cleared";
  }

  image.copyTo(m_imageViewImage);
  m_annotationImageWidth = static_cast<float>(image.cols);
  m_annotationImageHeight = static_cast<float>(image.rows);
  if (!m_parserDebugBridge.SetGlobalMatInput(m_imageViewImage)) {
    reason = "failed to bind global_matInput for evidence selftest";
    return false;
  }

  m_manualTest.image_file_path = path.string();
  m_scriptResult.image_ref = path.string();
  m_annotationStatus = "image loaded for evidence selftest";

  // Headless/CLI selftest has no OpenGL context; keep texture untouched.
  reason = "image loaded for evidence selftest: " + path.string();
  return true;
}
#include <BRep_Tool.hxx>
#include <Geom_Surface.hxx>
#include <Graphic3d_AspectFillArea3d.hxx>
#include <Prs3d_Drawer.hxx>
#include <Prs3d_ShadingAspect.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>
void ViewController::SetTexturedtoBoxFace(const cv::Mat &image) {
  Handle(Image_AlienPixMap) occtImage = new Image_AlienPixMap();

  if (!occtImage->InitZero(Image_Format_RGB, image.cols, image.rows)) {
    std::cerr << "Failed to initialize Image_AlienPixMap." << std::endl;
    return;
  }
  for (int row = 0; row < image.rows; ++row) {
    const uchar *sourceRow = image.ptr<uchar>(row);
    memcpy(occtImage->ChangeRow(row), sourceRow,
           image.cols * image.channels() * sizeof(uchar));
  }
  Handle(Graphic3d_Texture2D) texture =
      new Graphic3d_Texture2D("MemoryTexture");
  texture->SetImage(occtImage);
  texture->EnableRepeat();
  texture->EnableSmooth();
  texture->EnableModulate();

  gp_Ax2 anAxis;
  anAxis.SetLocation(gp_Pnt(0.0, 0.0, 0.0));
  TopoDS_Shape boxShape = BRepPrimAPI_MakeBox(anAxis, 1280, 1024, 50).Shape();

  TopExp_Explorer explorer(boxShape, TopAbs_FACE);

  const TopoDS_Face &frontFace = TopoDS::Face(explorer.Current());

  if (frontFace.IsNull()) {
    std::cerr << "Failed to find the front face of the box." << std::endl;
    return;
  }

  Handle(AIS_Shape) aBoxAIS = new AIS_Shape(boxShape);

  Handle(Graphic3d_AspectFillArea3d) fillAspect =
      new Graphic3d_AspectFillArea3d();
  fillAspect->SetTextureMapOn(true);
  fillAspect->SetTextureMap(texture);

  Handle(Prs3d_Drawer) drawer = new Prs3d_Drawer();
  Handle(Prs3d_ShadingAspect) shadingAspect = new Prs3d_ShadingAspect();
  shadingAspect->SetAspect(fillAspect);
  drawer->SetShadingAspect(shadingAspect);

  Handle(AIS_Shape) frontFaceAIS = new AIS_Shape(frontFace);
  frontFaceAIS->SetAttributes(drawer);

  myContext->Display(aBoxAIS, Standard_True);
  myContext->Display(frontFaceAIS, Standard_True);
  myContext->UpdateCurrentViewer();

  myContext->SetDisplayMode(AIS_Shaded, true);
}

void ViewController::SetTexturedtoPlane(const cv::Mat &image) {
  Handle(Image_AlienPixMap) occtImage = new Image_AlienPixMap();

  if (!occtImage->InitZero(Image_Format_RGB, image.cols, image.rows)) {
    std::cerr << "Failed to initialize Image_AlienPixMap." << std::endl;
    return;
  }

  for (int row = 0; row < image.rows; ++row) {
    const uchar *sourceRow = image.ptr<uchar>(row);
    memcpy(occtImage->ChangeRow(row), sourceRow,
           image.cols * image.channels() * sizeof(uchar));
  }

  Handle(Graphic3d_Texture2D) texture = CreateTextureFromImage(occtImage);

  gp_Pln plane(gp::XOY());

  BRepBuilderAPI_MakeEdge mkEdge1(gp_Pnt(0, 0, 0), gp_Pnt(1024, 0, 0));
  BRepBuilderAPI_MakeEdge mkEdge2(gp_Pnt(1024, 0, 0), gp_Pnt(1024, 768, 0));
  BRepBuilderAPI_MakeEdge mkEdge3(gp_Pnt(1024, 768, 0), gp_Pnt(0, 768, 0));
  BRepBuilderAPI_MakeEdge mkEdge4(gp_Pnt(0, 768, 0), gp_Pnt(0, 0, 0));

  BRepBuilderAPI_MakeWire mkWire;
  mkWire.Add(mkEdge1.Edge());
  mkWire.Add(mkEdge2.Edge());
  mkWire.Add(mkEdge3.Edge());
  mkWire.Add(mkEdge4.Edge());

  BRepBuilderAPI_MakeFace mkFace(plane, mkWire.Wire());
  TopoDS_Shape planeShape = mkFace.Face();

  Handle(AIS_Shape) planeAIS = new AIS_Shape(planeShape);

  Handle(Graphic3d_AspectFillArea3d) fillAspect =
      new Graphic3d_AspectFillArea3d();
  fillAspect->SetTextureMapOn();
  fillAspect->SetTextureMap(texture);

  Handle(Prs3d_Drawer) drawer = new Prs3d_Drawer();
  Handle(Prs3d_ShadingAspect) shadingAspect = new Prs3d_ShadingAspect();
  shadingAspect->SetAspect(fillAspect);
  drawer->SetShadingAspect(shadingAspect);

  planeAIS->SetAttributes(drawer);

  myContext->Display(planeAIS, Standard_True);
}

Handle(Graphic3d_Texture2D)
    ViewController::CreateTextureFromImage(const Handle(Image_PixMap) & image) {
  if (image.IsNull()) {
    std::cerr << "Invalid Image_PixMap provided." << std::endl;
    return nullptr;
  }

  Handle(Graphic3d_Texture2D) texture =
      new Graphic3d_Texture2D("MemoryTexture");
  texture->SetImage(image);

  return texture;
}

Handle(Image_PixMap)
    ViewController::ConvertCvMatToOcctImage(const cv::Mat &mat) {
  Handle(Image_AlienPixMap) pixMap = new Image_AlienPixMap();

  if (mat.type() == CV_8UC3) {
    if (!pixMap->InitZero(Image_Format_RGB, mat.cols, mat.rows))
      return nullptr;
    for (int row = 0; row < mat.rows; ++row) {
      const uchar *sourceRow = mat.ptr<uchar>(row);
      memcpy(pixMap->ChangeRow(row), sourceRow,
             mat.cols * mat.channels() * sizeof(uchar));
    }
  } else if (mat.type() == CV_8UC4) {
    if (!pixMap->InitZero(Image_Format_RGB32, mat.cols, mat.rows))
      return nullptr;
    for (int row = 0; row < mat.rows; ++row) {
      const uchar *sourceRow = mat.ptr<uchar>(row);
      uchar *destRow = pixMap->ChangeRow(row);
      for (int col = 0; col < mat.cols; ++col) {
        destRow[col * 4 + 0] = sourceRow[col * 3 + 2];
        destRow[col * 4 + 1] = sourceRow[col * 3 + 1];
        destRow[col * 4 + 2] = sourceRow[col * 3 + 0];
        destRow[col * 4 + 3] = 255;
      }
    }
  }

  return pixMap;
}

#include <Aspect_Window.hxx>
#include <Bnd_Box.hxx>
#include <Graphic3d_ArrayOfTriangles.hxx>
#include <V3d_View.hxx>

void ViewController::AdjustModelBoundingBoxToImageSize(
    const Handle(V3d_View) & myView, const Standard_CString imagePath) {
  (void)imagePath;
  Standard_Integer viewWidth, viewHeight;
  myView->Window()->Size(viewWidth, viewHeight);

  int imageWidth = 2048;
  int imageHeight = 1536;

  Bnd_Box boundingBox;
  double modelWidth = imageWidth;
  double modelHeight = imageHeight;

  boundingBox.Update(0, 0, 0, modelWidth, modelHeight, 0);

  double centerX = modelWidth / 2.0;
  double centerY = modelHeight / 2.0;
  double centerZ = 0.0;
  (void)centerZ;

  myView->SetSize(std::max(modelWidth, modelHeight));
  myView->ZFitAll();
  myView->SetCenter(centerX, centerY);

  myView->Redraw();
}

void ViewController::SetBackgroundInView(Handle(V3d_View) & view,
                                         const cv::Mat &image) {
  UpdateImageViewImage(image);
  if (!m_renderImageInOcctBackground)
    return;
  Handle(Image_PixMap) occtImage = ConvertCvMatToOcctImage(image);

  if (!occtImage.IsNull()) {
    Handle(Graphic3d_Texture2Dmanual) texture =
        CreateTextureFromImage(occtImage);

    if (!texture.IsNull()) {

      Standard_Real scale = 1;
      view->SetScale(scale);

      view->SetProj(V3d_XposYposZpos);

      view->SetEye(0, 0, -100);

      view->SetAt(0, 0, 0);

      view->SetUp(0, -1, 0);

      view->SetDepth(100);

      view->SetZoom(1);

      Standard_Integer viewWidth, viewHeight;
      view->Window()->Size(viewWidth, viewHeight);

      int imageWidth = image.cols;
      int imageHeight = image.rows;

      m_dscalex = (1.0 * imageWidth) / (1.0 * viewWidth);
      m_dscaley = (1.0 * imageHeight) / (1.0 * viewHeight);

      view->SetBackgroundImage(texture, Aspect_FM_STRETCH, true);
      double modelWidth = imageWidth;
      double modelHeight = imageHeight;
      double centerX = modelWidth / 2.0;
      double centerY = modelHeight / 2.0;
      double centerZ = 0.0;

      view->SetSize(std::max(modelWidth, modelHeight));
      view->ZFitAll();
      view->SetCenter(centerX, centerY);

      view->Redraw();
      myContext->UpdateCurrentViewer();
      myContext->UpdateCurrent();
    } else {
      std::cerr << "Failed to create texture from image." << std::endl;
    }
  } else {
    std::cerr << "Failed to convert image." << std::endl;
  }
}

void ViewController::initViewer(int theWidth, int theHeight) {
  if (myOcctWindow.IsNull() || myOcctWindow->getGlfwWindow() == nullptr) {
    return;
  }

  Handle(OpenGl_GraphicDriver) aGraphicDriver =
      new OpenGl_GraphicDriver(myOcctWindow->GetDisplay(), false);
  Handle(V3d_Viewer) aViewer = new V3d_Viewer(aGraphicDriver);
  aViewer->SetDefaultLights();
  aViewer->SetLightOn();
  aViewer->SetDefaultTypeOfView(V3d_ORTHOGRAPHIC);
  if (0)
    aViewer->ActivateGrid(Aspect_GT_Rectangular, Aspect_GDM_Lines);

  m_myView = new V3d_CustomView(aViewer);

  m_myView->SetImmediateUpdate(false);
  m_myView->SetWindow(myOcctWindow, myOcctWindow->NativeGlContext());
  m_myView->ChangeRenderingParams().ToShowStats = true;
  myContext = new AIS_InteractiveContext(aViewer);

  Handle(Prs3d_Drawer) aSelectionStyle = myContext->SelectionStyle();
  aSelectionStyle->SetColor(Quantity_NOC_WHITE);
  myContext->SetSelectionStyle(aSelectionStyle);

  m_myView->SetProj(V3d_TypeOfOrientation::V3d_XposYposZpos);

  Standard_Real eyeX = 0.0, eyeY = 0.0, eyeZ = -500.0;
  m_myView->SetEye(eyeX, eyeY, eyeZ);

  Standard_Real atX = 0.0, atY = 0.0, atZ = 0.0;
  m_myView->SetAt(atX, atY, atZ);

  Standard_Real upX = 0.0, upY = -1.0, upZ = 0.0;
  m_myView->SetUp(upX, upY, upZ);

  m_myView->SetDepth(100);
  m_myView->SetCenter(theWidth, theHeight);

  Standard_Real scale = 1;
  m_myView->SetScale(scale);

  m_myView->SetProj(V3d_XposYposZpos);

  m_myView->SetEye(0, 0, -10000);

  m_myView->SetAt(0, 0, 0);

  m_myView->SetUp(0, -1, 0);

  m_myView->SetDepth(10000);

  m_myView->SetZoom(1);

  m_myView->SetCenter(theWidth, theHeight);

  m_myView->ZFitAll();
  m_myView->Redraw();
}
void ViewController::drawline() {
  gp_Pnt pn_Start;
  pn_Start.SetX(10);
  pn_Start.SetY(20);
  pn_Start.SetZ(0);

  gp_Pnt pn_End;
  pn_End.SetX(50);
  pn_End.SetY(60);
  pn_End.SetZ(0);

  TopoDS_Vertex V1 = BRepBuilderAPI_MakeVertex(pn_Start);
  TopoDS_Vertex V2 = BRepBuilderAPI_MakeVertex(pn_End);

  TopoDS_Shape aShape = BRepBuilderAPI_MakeEdge(V1, V2);

  Handle(AIS_Shape) aisLine = new AIS_Shape(aShape);

  myContext->Display(aisLine, AIS_Shaded, 0, false);
}

void ViewController::initDemoScene() {
  if (myContext.IsNull()) {
    return;
  }
  SetAllowZooming(Standard_False);
  SetAllowRotation(Standard_False);

  if (1) {
    if (0) {
      gp_Trsf shapeTrsf = aBox->LocalTransformation();

      gp_Pnt position = gp_Pnt(shapeTrsf.TranslationPart());

      gp_Dir xAxis;
      gp_Dir yAxis;

      if (shapeTrsf.Form() == gp_Identity ||
          shapeTrsf.Form() == gp_Translation) {
        xAxis = gp::DX();
        yAxis = gp::DY();
      } else {
        gp_Mat rotationMatrix = shapeTrsf.VectorialPart();

        xAxis = gp_Dir(rotationMatrix.Column(1));
        yAxis = gp_Dir(rotationMatrix.Column(2));
      }

      gp_Ax2 axis(position, yAxis, xAxis);
    }
  }

  gp_Path m_gpath;
  m_gpath.SetContext(myContext);
  m_gpath.SetView(m_myView);

  std::string init_reason;
  if (!m_parserOwner.Initialize(init_reason)) {
    m_os << "parser initialization failed: " << init_reason << std::endl;
  } else {
    m_parserOwner.ConfigureStreams(&m_os, &m_createcodeos);
    m_parserDebugBridge.Bind(&m_parserOwner);
  }

  initialparser();

  myContext->SetDisplayMode(AIS_Shaded, true);

  TCollection_AsciiString aGlInfo;
  {
    TColStd_IndexedDataMapOfStringString aRendInfo;
    m_myView->DiagnosticInformation(aRendInfo, Graphic3d_DiagnosticInfo_Basic);
    for (TColStd_IndexedDataMapOfStringString::Iterator aValueIter(aRendInfo);
         aValueIter.More(); aValueIter.Next()) {
      if (!aGlInfo.IsEmpty()) {
        aGlInfo += "\n";
      }
      aGlInfo += TCollection_AsciiString("  ") + aValueIter.Key() + ": " +
                 aValueIter.Value();
    }
  }
  Message::DefaultMessenger()->Send(
      TCollection_AsciiString("OpenGL info:\n") + aGlInfo, Message_Info);
}

static ImVec2 last_window_pos = ImVec2(0, 0);

const char* ViewController::AiGuiShortcutName(
    AiGuiDestination destination) const {
  switch (destination) {
  case AiGuiDestination::EvidenceImageSet:
    return "F2";
  case AiGuiDestination::EvidenceCase:
    return "F3";
  case AiGuiDestination::ManualReviewEvidence:
    return "F4";
  case AiGuiDestination::ManualReviewDecision:
    return "F5";
  case AiGuiDestination::ImageAnnotation:
    return "F6";
  case AiGuiDestination::KeyParameters:
    return "F7";
  case AiGuiDestination::TorchRuntimeEvidence:
    return "F8";
  case AiGuiDestination::TorchTrainingImageSet:
    return "F9";
  case AiGuiDestination::ParameterConclusion:
    return "F10";
  case AiGuiDestination::ManualScriptConsole:
    return "F11";
  case AiGuiDestination::AnalyticsSmoke:
    return "F12";
  default:
    return "-";
  }
}

const char* ViewController::AiGuiWindowName(
    AiGuiDestination destination) const {
  switch (destination) {
  case AiGuiDestination::EvidenceImageSet:
  case AiGuiDestination::EvidenceCase:
  case AiGuiDestination::ManualReviewEvidence:
  case AiGuiDestination::ManualReviewDecision:
    return "Evidence Chain UI";
  case AiGuiDestination::ImageAnnotation:
    return "Image View";
  case AiGuiDestination::KeyParameters:
    return "Key Parameter Controls";
  case AiGuiDestination::TorchRuntimeEvidence:
    return "Torch Runtime / Evidence";
  case AiGuiDestination::TorchTrainingImageSet:
    return "Torch Training Image Set";
  case AiGuiDestination::ParameterConclusion:
    return "Parameter Tuning Map / Result Conclusion";
  case AiGuiDestination::ManualScriptConsole:
    return "Manual State Test Console";
  case AiGuiDestination::AnalyticsSmoke:
    return "Analytics Smoke / Metrology Bridge";
  default:
    return "";
  }
}

void ViewController::RequestAiGuiDestination(
    AiGuiDestination destination) {
  if (destination == AiGuiDestination::None)
    return;

  switch (destination) {
  case AiGuiDestination::EvidenceImageSet:
    m_evidenceChainUiSection = 0;
    break;
  case AiGuiDestination::EvidenceCase:
    m_evidenceChainUiSection = 1;
    break;
  case AiGuiDestination::ManualReviewEvidence:
    m_evidenceChainUiSection = 2;
    if (m_manualTest.script_evidence_groups.empty())
      m_manualTest.script_evidence_groups_dirty = true;
    break;
  case AiGuiDestination::ManualReviewDecision:
    m_evidenceChainUiSection = 3;
    break;
  default:
    break;
  }

  m_aiGuiPendingFocus = destination;
  m_aiGuiCurrentDestination = destination;
  m_aiGuiSemanticLocation =
      std::string(AiGuiShortcutName(destination)) + " -> " +
      AiGuiWindowName(destination) + " (focus pending)";
  ImGui::SetWindowCollapsed(AiGuiWindowName(destination), false,
                            ImGuiCond_Always);
  ImGui::SetWindowFocus(AiGuiWindowName(destination));
  m_forceRepaintFrames = std::max(m_forceRepaintFrames, 3);

  CXLOG_INFO("AiGuiNavigation", "shortcut_destination_requested", "ui_event",
             m_aiGuiSemanticLocation);
}

bool ViewController::ApplyAiGuiFocusHere(
    AiGuiDestination destination, const char* semanticLocation) {
  if (m_aiGuiPendingFocus != destination)
    return false;

  ImGui::SetWindowFocus();
  ImGui::SetScrollY(0.0f);
  ImGui::SetKeyboardFocusHere(0);
  ImGui::SetNavCursorVisible(true);
  m_aiGuiPendingFocus = AiGuiDestination::None;
  m_aiGuiCurrentDestination = destination;
  m_aiGuiSemanticLocation =
      std::string(AiGuiShortcutName(destination)) + " -> " +
      (semanticLocation == nullptr ? AiGuiWindowName(destination)
                                   : semanticLocation);

  CXLOG_INFO("AiGuiNavigation", "shortcut_focus_applied", "ui_event",
             m_aiGuiSemanticLocation);
  return true;
}

void ViewController::ProcessAiGuiShortcuts() {
  if (ImGui::IsKeyPressed(ImGuiKey_F1, false)) {
    m_showAiGuiShortcutHelp = !m_showAiGuiShortcutHelp;
    m_aiGuiPendingFocus = AiGuiDestination::None;
    m_aiGuiSemanticLocation =
        m_showAiGuiShortcutHelp
            ? "F1 -> AI GUI Shortcut Help > shortcut list"
            : "F1 -> AI GUI Shortcut Help (closed)";
    if (m_showAiGuiShortcutHelp) {
      ImGui::SetWindowCollapsed("AI GUI Shortcut Help", false,
                                ImGuiCond_Always);
      ImGui::SetWindowFocus("AI GUI Shortcut Help");
    }
    m_forceRepaintFrames = std::max(m_forceRepaintFrames, 3);
    CXLOG_INFO("AiGuiNavigation", "shortcut_help_toggled", "ui_event",
               m_aiGuiSemanticLocation);
  }

  struct ShortcutBinding {
    ImGuiKey key;
    AiGuiDestination destination;
  };
  static const ShortcutBinding bindings[] = {
      {ImGuiKey_F2, AiGuiDestination::EvidenceImageSet},
      {ImGuiKey_F3, AiGuiDestination::EvidenceCase},
      {ImGuiKey_F4, AiGuiDestination::ManualReviewEvidence},
      {ImGuiKey_F5, AiGuiDestination::ManualReviewDecision},
      {ImGuiKey_F6, AiGuiDestination::ImageAnnotation},
      {ImGuiKey_F7, AiGuiDestination::KeyParameters},
      {ImGuiKey_F8, AiGuiDestination::TorchRuntimeEvidence},
      {ImGuiKey_F9, AiGuiDestination::TorchTrainingImageSet},
      {ImGuiKey_F10, AiGuiDestination::ParameterConclusion},
      {ImGuiKey_F11, AiGuiDestination::ManualScriptConsole},
      {ImGuiKey_F12, AiGuiDestination::AnalyticsSmoke},
  };

  for (const ShortcutBinding& binding : bindings) {
    if (ImGui::IsKeyPressed(binding.key, false)) {
      RequestAiGuiDestination(binding.destination);
      break;
    }
  }

  if (m_showAiGuiShortcutHelp &&
      ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    m_showAiGuiShortcutHelp = false;
}

void ViewController::DrawAiGuiNavigationOverlay() {
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(
      ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - 8.0f,
             viewport->WorkPos.y + 8.0f),
      ImGuiCond_Always, ImVec2(1.0f, 0.0f));
  ImGui::SetNextWindowBgAlpha(0.88f);
  const ImGuiWindowFlags statusFlags =
      ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs;
  if (ImGui::Begin("AI GUI Navigation Status", nullptr, statusFlags)) {
    ImGui::TextUnformatted("AI GUI NAVIGATION");
    ImGui::Text("Location: %s", m_aiGuiSemanticLocation.c_str());
    ImGui::TextDisabled("F1 help | Tab/Shift+Tab move | Enter/Space act | Esc cancel");
  }
  ImGui::End();

  if (!m_showAiGuiShortcutHelp)
    return;

  ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing,
                          ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(660.0f, 520.0f), ImGuiCond_Appearing);
  if (ImGui::Begin("AI GUI Shortcut Help", &m_showAiGuiShortcutHelp,
                   ImGuiWindowFlags_NoCollapse)) {
    ImGui::TextWrapped(
        "Shortcuts navigate only: they expose the named window/page, bring it "
        "to the front, scroll to its anchor, and focus its first actionable "
        "control. They never run, save, accept, or reject automatically.");
    ImGui::Separator();
    ImGui::TextUnformatted("F2  Evidence Chain UI > Image Set list");
    ImGui::TextUnformatted("F3  Evidence Chain UI > Case list");
    ImGui::TextUnformatted(
        "F4  Manual Review / Evidence > visible evidence items");
    ImGui::TextUnformatted(
        "F5  Manual Review / Evidence > Review decision controls");
    ImGui::TextUnformatted(
        "F6  Image Evidence / Annotation Tools > manifest and tool palette");
    ImGui::TextUnformatted(
        "F7  Key Parameter Controls > active tool parameters");
    ImGui::TextUnformatted(
        "F8  Torch Runtime / Evidence > runtime and review controls");
    ImGui::TextUnformatted(
        "F9  Torch Training Image Set > dataset actions and image rails");
    ImGui::TextUnformatted(
        "F10 Parameter Tuning Map / Result Conclusion");
    ImGui::TextUnformatted(
        "F11 Manual State Test Console > script editor/debug compiler");
    ImGui::TextUnformatted(
        "F12 Analytics Smoke / Metrology Bridge");
    ImGui::Separator();
    ImGui::TextWrapped(
        "After arrival: Tab or Shift+Tab moves between controls; arrow keys "
        "move inside lists; Enter activates a button or selected row; Space "
        "toggles; Escape cancels/closes. Read the always-visible Location "
        "line before acting.");
    ImGui::TextWrapped(
        "Human project analysis and decisions belong to F4/F5 Manual Review / "
        "Evidence. Suite/dry-run views are not human review destinations.");
  }
  ImGui::End();
}

void ViewController::mainloop() {
  int ifirstrun = 1;
  int frameLogBudget = 8;
  while (!glfwWindowShouldClose(myOcctWindow->getGlfwWindow())) {
    glfwWaitEvents();

    if (!m_myView.IsNull()) {
      const bool logThisFrame = frameLogBudget > 0;
      if (logThisFrame) {
        --frameLogBudget;
        CXLOG_INFO("ViewController", "mainloop_stage", "running",
                   "stage=frame_begin");
      }

      ImGui_ImplOpenGL3_NewFrame();
      ImGui_ImplGlfw_NewFrame();
      ImGui::NewFrame();
      DrawCxImGuiFrameBackground();
      ProcessAiGuiShortcuts();


      if (logThisFrame)
        CXLOG_INFO("ViewController", "mainloop_stage", "running",
                   "stage=process_deferred_sync");
      ProcessDeferredRuntimeShapeSync("frame_begin");

      ImGuiIO &io = ImGui::GetIO();
      const bool imguiCapturesMouse =
          io.WantCaptureMouse ||
          ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) ||
          ImGui::IsAnyItemHovered() || ImGui::IsAnyItemActive();

      const bool canvasInteraction =
          !imguiCapturesMouse && (ImGui::IsMouseDown(ImGuiMouseButton_Left) ||
                                  ImGui::IsMouseDown(ImGuiMouseButton_Right) ||
                                  ImGui::IsMouseDown(ImGuiMouseButton_Middle) ||
                                  io.MouseWheel != 0.0f);

      if (canvasInteraction || ifirstrun == 1) {
        ifirstrun = 0;
        FlushViewEvents(myContext, m_myView, true);
        m_myView->Redraw();
      }

      if (m_showLegacyGpuWork) {
        ImGui::Begin("GPU work");

        {
          ImVec2 current_window_pos = ImGui::GetWindowPos();
          m_current_window_posx = current_window_pos.x;
          m_current_window_posy = current_window_pos.y;

          m_imguiw = ImGui::GetWindowWidth();
          m_imguih = ImGui::GetWindowHeight();
          if (current_window_pos.x != last_window_pos.x ||
              current_window_pos.y != last_window_pos.y) {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Window Moved!");
            ifirstrun = 0;
            FlushViewEvents(myContext, m_myView, true);
            m_myView->Redraw();
          }
          last_window_pos = current_window_pos;
        }

        ImGui::Text("Parser code input here (%s) (%d)", IMGUI_VERSION,
                    IMGUI_VERSION_NUM);
        ImGui::Checkbox("Show Image", &m_imageshow);
        ImGui::Checkbox("Pick Points", &m_ipickpoints);
        ImGui::Checkbox("Line Scan", &m_ilinescan);
        ImGui::Checkbox("Attach Line", &m_iattachline);
        ImGui::Spacing();
        static char text[512 * 106] =
            "if(0){aimage1.load(\"1.bmp\");}\n"
            "aimage1.Show(1);\n"
            "amatch0.getshape(ashape0);\n"
            "amatch0.setobjfilter(1);\n"
            "amatch0.setwhgap(5, 5); \n"
            "amatch0.setthre(35);\n"
            "amatch0.setlinegap(3);\n"
            "amatch0.setcompgap(20);\n"
            "amatch0.learn(aimage1);\n"
            "amatch0.savemodel(\"D:\\test.pat\");\n"
            "amatch0.pattern2org(); \n"
            "if(0){amatch0.pattern2org();}\n"
            "if(0){amatch0.reorgpattern();}\n"
            "if(0){amatch0.patterngap(0.5);}\n"
            "if(0){amatch0.patternsample(3);}\n"
            "if(0){amatch0.modelzero();amatch0.modelrotate(15.0);}\n"
            "amatch0.Show(8);\n";

        static char showtext[512 * 106];
        static ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput;
        ImGui::InputTextMultiline(
            "##source", text, IM_ARRAYSIZE(text),
            ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 6), flags);

        static int clicked = 0;
        if (ImGui::Button("Parser Run")) {
          clicked++;
          clearos();
          clearcreateos();

          auto start = std::chrono::high_resolution_clock::now();

          if (0)
            m_parserOwner.Compile(text);

          auto end = std::chrono::high_resolution_clock::now();
          std::chrono::duration<int, std::milli> elapsed_time =
              std::chrono::duration_cast<std::chrono::milliseconds>(end -
                                                                    start);
          m_iruntimes = elapsed_time.count();

          ifirstrun = 0;
          FlushViewEvents(myContext, m_myView, true);
          m_myView->Redraw();
        }
        static char text2[512 * 106] =
            "amatch0.loadmodel(\"D:\\test.pat\");\n"
            "amatch0.loadrotatemodel(\"D:\\test.pat\");\n"
            "amatch0.samplemodel(100);\n"
            "amatch0.setmatchrect(50,50,2200,1900);\n"
            "amatch0.matchstepgap(10, 10);\n"
            "amatch0.setmatchthre(10);\n"
            "amatch0.setminscore(0.65);\n"
            "amatch0.setfindnum(1);\n"
            "if(0){\n"
            "amatch0.match(aimage1);\n"
            "}\n"
            "\n"
            "if(0){\n"
            "amatch0.setanglescale(-10,10);\n"
            "amatch0.rotatematchAB(aimage1);\n"
            "}\n"
            "if(0){\n"
            "amatch0.reorgpattern();\n"
            "amatch0.patterngap(0.2); \n"
            "amatch0.matchmore(aimage1);\n"
            "}\n"
            "amatch0.Show(8);\n"
            "dvalue1 = amatch0.getmaxresult();\n"
            "dvalue2 = amatch0.getresultcentx(-1);\n"
            "dvalue3 = amatch0.getresultcenty(-1);\n";

        static char showtext2[512 * 106];
        static ImGuiInputTextFlags flags2 = ImGuiInputTextFlags_AllowTabInput;
        ImGui::InputTextMultiline(
            "##source2", text2, IM_ARRAYSIZE(text2),
            ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 6), flags2);
        if (ImGui::Button("Parser Run2")) {
          clicked++;
          clearos();
          clearcreateos();

          auto start = std::chrono::high_resolution_clock::now();

          m_parserOwner.Compile(text2);

          auto end = std::chrono::high_resolution_clock::now();
          std::chrono::duration<int, std::milli> elapsed_time =
              std::chrono::duration_cast<std::chrono::milliseconds>(end -
                                                                    start);
          m_iruntimes = elapsed_time.count();

          ifirstrun = 0;
          FlushViewEvents(myContext, m_myView, true);
          m_myView->Redraw();
          myContext->UpdateCurrentViewer();
          myContext->UpdateCurrent();
        }

        static char text3[512 * 106] =
            "aimage1.loadfiles(\"D:\\TestImage\\135\");\n"
            "aimage1.getshape(ashape0);\n"
            "if(0){\n"
            "aimage1.roipyrdown(5);\n"
            "aimage1.roieasythre(255);\n"
            "}\n"
            "aimage1.Show(1); \n"
            "if(0){aimage1.rotate(10);}\n"
            "if(0){aimage1.roi_5bgmb(3,1,0,2);}\n"
            "if(0){aimage1.roi_5bgmbh(3,1,0,2);}\n"
            "if(0){aimage1.roi_7bgmb(3,1,0,2);}\n"
            "if(0){aimage1.roi_7bgmbh(3,1,0,2);}\n"
            "if(0){aimage1.roisobel(0,1,7);}\n"
            "if(0){aimage1.roischarr(0,1);}\n";

        static char showtext3[512 * 106];
        static ImGuiInputTextFlags flags3 = ImGuiInputTextFlags_AllowTabInput;
        ImGui::InputTextMultiline(
            "##source3", text3, IM_ARRAYSIZE(text3),
            ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 6), flags3);
        if (ImGui::Button("Parser Run3")) {
          clicked++;
          clearos();
          clearcreateos();

          auto start = std::chrono::high_resolution_clock::now();

          m_parserOwner.Compile(text3);

          auto end = std::chrono::high_resolution_clock::now();
          std::chrono::duration<int, std::milli> elapsed_time =
              std::chrono::duration_cast<std::chrono::milliseconds>(end -
                                                                    start);
          m_iruntimes = elapsed_time.count();

          ifirstrun = 0;
          FlushViewEvents(myContext, m_myView, true);
          m_myView->Redraw();
          myContext->UpdateCurrentViewer();
          myContext->UpdateCurrent();
        }

        static char text4[512 * 106] = "if(0){aimage1.reload();}\n"
                                       "aimage2.CopyFrom(aimage1);\n"
                                       "aimage1.getshape(ashape0);\n"
                                       "aimage1.Show(1);\n"
                                       "if(0){\n"
                                       "asam.setp(apoints0);\n"
                                       "asam.setn(apoints1);\n"
                                       "asam.run(aimage1);\n"
                                       "aimage1.Or(aimage2);\n"
                                       "}\n"
                                       "if(0){\n"
                                       "afindline.getshape(ashape0);\n"
                                       "afindline.setwhgap(5, 5);\n"
                                       "afindline.setlinegap(3);\n"
                                       "afindline.setthre(38);\n"
                                       "afindline.setmethod(0);\n"
                                       "afindline.setobjfilter(0);\n"
                                       "afindline.measure(aimage1);\n"
                                       "afindline.sfilter(-1, -1); \n"
                                       "afindline.Show(1);\n"
                                       "afindline.inflectionpoint(apoints1);\n"
                                       "apoints1.setcolor(255,0,0);\n"
                                       "apoints1.Show(1);\n"
                                       "}\n"
                                       "if(0){\n"
                                       "afindcircle.setgap(5);\n"
                                       "afindcircle.getshape(ashape0);\n"
                                       "afindcircle.setmethod(0);\n"
                                       "afindcircle.setthre(20);\n"
                                       "afindcircle.setlinegap(3);\n"
                                       "afindcircle.setcirclegap(380);\n"
                                       "afindcircle.measure(aimage1);\n"
                                       "afindcircle.fitcircle();\n"
                                       "afindcircle.setfitmeasuregap(80);\n"
                                       "afindcircle.fitmeasure(aimage1);\n"
                                       "afindcircle.Show(1);\n"
                                       "dvalue1=afindcircle.getavgdist();\n"
                                       "}\n";
        static char showtext4[512 * 106];
        static ImGuiInputTextFlags flags4 = ImGuiInputTextFlags_AllowTabInput;
        ImGui::InputTextMultiline(
            "##source4", text4, IM_ARRAYSIZE(text4),
            ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 5), flags4);
        if (ImGui::Button("Parser Run4")) {
          clicked++;
          clearos();
          clearcreateos();
          auto start = std::chrono::high_resolution_clock::now();
          m_parserOwner.Compile(text4);
          auto end = std::chrono::high_resolution_clock::now();
          std::chrono::duration<int, std::milli> elapsed_time =
              std::chrono::duration_cast<std::chrono::milliseconds>(end -
                                                                    start);
          m_iruntimes = elapsed_time.count();
          ifirstrun = 0;
          FlushViewEvents(myContext, m_myView, true);
          m_myView->Redraw();
          myContext->UpdateCurrentViewer();
          myContext->UpdateCurrent();
        }
        static char text5[512 * 106] =
            "if(1){apoints0.load(\"D:\\26.data\");apoints0.Show(1);}\n"
            "if(0){apoints0.save(\"D:\\2.data\");apoints0.Show(1);}\n"
            "if(0){apoints0.aptfilter(5);apoints0.Show(1);}\n"
            "if(0){apoints0.cluster(10,8);apoints0.Show(16);}\n"
            "if(0){apoints0.sortpoints(80,0,10,45);apoints0.Show(16);}\n"
            "if(0){apoints0.clear();apoints0.Show(1);}\n"
            "if(0){apoints0.obbanglecenter(apoints1);}\n"
            "if(0){apoints0.filter(20, 1); apoints0.Show(1);}\n"
            "if(0){apoints0.findcross(apoints1);apoints1.Show(1);}\n";
        static char showtext5[512 * 106];
        static ImGuiInputTextFlags flags5 = ImGuiInputTextFlags_AllowTabInput;
        ImGui::InputTextMultiline(
            "##source5", text5, IM_ARRAYSIZE(text5),
            ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 5), flags5);
        if (ImGui::Button("Parser Run5")) {
          clicked++;
          clearos();
          clearcreateos();

          auto start = std::chrono::high_resolution_clock::now();

          m_parserOwner.Compile(text5);

          auto end = std::chrono::high_resolution_clock::now();
          std::chrono::duration<int, std::milli> elapsed_time =
              std::chrono::duration_cast<std::chrono::milliseconds>(end -
                                                                    start);
          m_iruntimes = elapsed_time.count();

          ifirstrun = 0;
          FlushViewEvents(myContext, m_myView, true);
          m_myView->Redraw();
          myContext->UpdateCurrentViewer();
          myContext->UpdateCurrent();
        }

        static char text6[512 * 106] = "TestRun arun;\n"
                                       "arun.testrun();\n"
                                       "ahttp.runserver(); \n"
                                       "ahttp.runclient(); \n";
        static char showtext6[512 * 106];
        static ImGuiInputTextFlags flags6 = ImGuiInputTextFlags_AllowTabInput;
        ImGui::InputTextMultiline(
            "##source6", text6, IM_ARRAYSIZE(text6),
            ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 2), flags6);
        if (ImGui::Button("Parser Run6")) {
          clicked++;
          clearos();
          clearcreateos();

          auto start = std::chrono::high_resolution_clock::now();

          m_parserOwner.Compile(text6);

          auto end = std::chrono::high_resolution_clock::now();
          std::chrono::duration<int, std::milli> elapsed_time =
              std::chrono::duration_cast<std::chrono::milliseconds>(end -
                                                                    start);
          m_iruntimes = elapsed_time.count();

          ifirstrun = 0;
          FlushViewEvents(myContext, m_myView, true);
          m_myView->Redraw();
          myContext->UpdateCurrentViewer();
          myContext->UpdateCurrent();
        }

        if (clicked & 1) {
          clicked = 0;
          m_imageshow = 1;
          strcpy(showtext, getoutputstring().c_str());
          strcat(showtext, "\n");
        }
        ImGui::SameLine();
        ImGui::Text("Parser Run result output here!");
        ImGui::Text(showtext);
        std::string strtime = string("elapsed time:") +
                              std::to_string(m_iruntimes) + string(" ms");
        ImGui::Text(strtime.c_str());
        ImGuiIO &io = ImGui::GetIO();
        ImGui::TextWrapped(" ");
        if (ImGui::Button("Run")) {
          std::cout << "run!" << std::endl;
        }
        ImGui::Text("run gpu and ai.");

        if (ImGui::CollapsingHeader("OpenCV Editor")) {
          ImGui::Checkbox("Show OpenCV Editor", &opencvSW);
          ImGui::Checkbox("Show OpenCV Blur", &opencvblur);
          ImGui::SliderInt("slider thre", &ivalue1, 1, 255);
          ImGui::SliderInt("slider gap", &ivalue2, 1, 255);
          ImGui::Checkbox("Show GPU Blur and HSV augmentation", &gpublur);
          ImGui::Checkbox("reset OpenCV ", &opencvreset);
          ImGui::Checkbox("Edge Image", &irunedge);

          ImGui::Checkbox("pyramid thre Image", &ipythre);
          ImGui::Checkbox("iotsuThreshold thre Image", &iotsuThreshold);

          ImGui::Checkbox("H run", &ihedge);
          ImGui::Checkbox("V run", &iwedge);
          ImGui::Checkbox("B2W", &ib2wedge);
          ImGui::Checkbox("W2B", &iw2bedge);
          ImGui::SliderInt("pyramidDynamicThreshold levels", &ivalue3, 1, 8);
          ImGui::SliderInt("pyramidDynamicThreshold blockSize", &ivalue4, 1,
                           19);
          ImGui::SliderInt("value 5", &ivalue5, 1, 255);
          ImGui::SliderInt("value 6", &ivalue6, 1, 255);
          ImGui::SliderInt("value 7", &ivalue7, 0, 255);
          ImGui::SliderInt("value 8", &ivalue8, 0, 255);
          ImGui::SliderInt("value 9", &ivalue9, 0, 255);
          ImGui::SliderInt("value 10", &ivalue10, 0, 255);
        }

        ImGui::End();
      }

      SetCxCrashBreadcrumb("mainloop:drawScriptAcceptancePanels");
      if (logThisFrame)
        CXLOG_INFO("ViewController", "mainloop_stage", "running",
                   "stage=drawScriptAcceptancePanels");
      drawScriptAcceptancePanels();
      if (m_showBusinessWorkflowPanel) {
        SetCxCrashBreadcrumb("mainloop:CxBusinessWorkflowPanel.Draw");
        m_businessWorkflowPanel.Draw(&m_showBusinessWorkflowPanel);
      }
      SetCxCrashBreadcrumb("mainloop:SemanticFlowGraph.Draw");
      if (logThisFrame)
        CXLOG_INFO("ViewController", "mainloop_stage", "running",
                   "stage=SemanticFlowGraph.Draw");
      const SemanticFlowAction flowAction = m_semanticFlowGraph.Draw();
      SetCxCrashBreadcrumb("mainloop:HandleSemanticFlowAction");
      if (logThisFrame)
        CXLOG_INFO("ViewController", "mainloop_stage", "running",
                   "stage=HandleSemanticFlowAction");
      HandleSemanticFlowAction(flowAction);
      SetCxCrashBreadcrumb("mainloop:drawEvidenceAlbumWindow");
      if (logThisFrame)
        CXLOG_INFO("ViewController", "mainloop_stage", "running",
                   "stage=drawEvidenceAlbumWindow");
      drawEvidenceAlbumWindow();
      SetCxCrashBreadcrumb("mainloop:drawAnnotationToolWindow");
      if (logThisFrame)
        CXLOG_INFO("ViewController", "mainloop_stage", "running",
                   "stage=drawAnnotationToolWindow");
      drawAnnotationToolWindow();
      SetCxCrashBreadcrumb("mainloop:drawKeyParameterControlsWindow");
      if (logThisFrame)
        CXLOG_INFO("ViewController", "mainloop_stage", "running",
                   "stage=drawKeyParameterControlsWindow");
      drawKeyParameterControlsWindow();
      ConsumePendingManualScriptRun(m_manualTest,
                                    "mainloop_after_key_parameter_controls");
      SetCxCrashBreadcrumb("mainloop:drawMetrologyAnalyticsSmokeWindow");
      if (logThisFrame)
        CXLOG_INFO("ViewController", "mainloop_stage", "running",
                   "stage=drawMetrologyAnalyticsSmokeWindow");
      drawMetrologyAnalyticsSmokeWindow();
      SetCxCrashBreadcrumb("mainloop:drawTorchRuntimeEvidenceWindow");
      if (logThisFrame)
        CXLOG_INFO("ViewController", "mainloop_stage", "running",
                   "stage=drawTorchRuntimeEvidenceWindow");
      drawTorchRuntimeEvidenceWindow();
      SetCxCrashBreadcrumb("mainloop:drawTorchTrainingImageSetWindow");
      if (logThisFrame)
        CXLOG_INFO("ViewController", "mainloop_stage", "running",
                   "stage=drawTorchTrainingImageSetWindow");
      drawTorchTrainingImageSetWindow();
      SetCxCrashBreadcrumb("mainloop:drawParameterTuningAndConclusionWindow");
      if (logThisFrame)
        CXLOG_INFO("ViewController", "mainloop_stage", "running",
                   "stage=drawParameterTuningAndConclusionWindow");
      drawParameterTuningAndConclusionWindow();
      SetCxCrashBreadcrumb("mainloop:drawManualStateTestConsole");
      if (logThisFrame)
        CXLOG_INFO("ViewController", "mainloop_stage", "running",
                   "stage=drawManualStateTestConsole");
      drawManualStateTestConsole();

      // Key Parameter Controls, FastMatch action buttons, evidence
      // candidate save+run, and Torch action panels can stage a serial
      // parser run from different windows.  Consume the pending request
      // after all UI panels have had a chance to write their snapshot so
      // the actual script execution does not depend on window ordering
      // or on the Debug Compiler header being visible.
      ConsumePendingManualScriptRun(m_manualTest,
                                    "mainloop_after_all_manual_panels");

      ImGui::SetNextWindowPos(
          ImVec2(650, 20),
          ImGuiCond_FirstUseEver); // Normally user code doesn't need/want to
                                   // call this because positions are saved in
                                   // .ini file anyway. Here we just want to
                                   // make the demo initial state a bit more
                                   // friendly!

      SetCxCrashBreadcrumb("mainloop:image_show_update");
      if (1 == m_imageshow) {
        m_imageshow = 0;
        Image *pshowimage = nullptr;
        for (int i = 0; i < m_parserOwner.ObjectCount("Image"); i++) {
          Image *pimage = (Image *)m_parserOwner.GetClassObj("Image", i);
          if (pimage->getshow() == 1)
            pshowimage = pimage;
        }

        ImageManager *pmodule =
            (ImageManager *)m_parserOwner.GetClassObj("Module", "amodule");
        Image *pmoduleimage = nullptr;
        int imoduleshow = 0;
        if (nullptr != pmodule) {
          pmoduleimage = pmodule->GetBackImage();
          imoduleshow = pmodule->getshow();
        }
        if (0 != pmodule && imoduleshow > 0) {
          if (1 == imoduleshow)
            SetBackgroundInView(m_myView, pmodule->GetBackImage()->getmat());
          else if (2 == imoduleshow)
            SetBackgroundInView(m_myView, pmodule->GetMapImage()->getmat());
        } else if (nullptr != pshowimage) {
          SetBackgroundInView(m_myView, pshowimage->getmat());
        }
      }
      if (opencvSW)
        Imgui_OpenCV_Window0(&opencvSW);

      DrawAiGuiNavigationOverlay();

      SetCxCrashBreadcrumb("mainloop:ImGui::Render");
      if (logThisFrame)
        CXLOG_INFO("ViewController", "mainloop_stage", "running",
                   "stage=ImGui::Render");
      ImGui::Render();
      SetCxCrashBreadcrumb("mainloop:RenderDrawData");
      if (logThisFrame)
        CXLOG_INFO("ViewController", "mainloop_stage", "running",
                   "stage=RenderDrawData");
      int framebufferWidth = 0;
      int framebufferHeight = 0;
      glfwGetFramebufferSize(myOcctWindow->getGlfwWindow(), &framebufferWidth,
                             &framebufferHeight);
      glViewport(0, 0, framebufferWidth, framebufferHeight);
      glClearColor(0.215f, 0.215f, 0.215f, 1.0f);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

      SetCxCrashBreadcrumb("mainloop:glfwSwapBuffers");
      if (logThisFrame)
        CXLOG_INFO("ViewController", "mainloop_stage", "running",
                   "stage=glfwSwapBuffers");
      glfwSwapBuffers(myOcctWindow->getGlfwWindow());
      SetCxCrashBreadcrumb("mainloop:frame_complete");
      if (logThisFrame)
        CXLOG_INFO("ViewController", "mainloop_stage", "finished",
                   "stage=frame_complete");
    }
  }
}

void ViewController::cleanup() {
  GLFWwindow *glfwWindow =
      !myOcctWindow.IsNull() ? myOcctWindow->getGlfwWindow() : nullptr;
  if (m_glfwInitialized && glfwWindow != nullptr) {
    glfwMakeContextCurrent(glfwWindow);
    glfwSetWindowUserPointer(glfwWindow, nullptr);
  }

  if (m_glfwInitialized && m_imageViewTexture != 0) {
    glDeleteTextures(1, &m_imageViewTexture);
  }
  m_imageViewTexture = 0;

  if (!m_myView.IsNull()) {
    m_myView->Remove();
    m_myView.Nullify();
  }

  if (m_imguiOpenGlInitialized) {
    ImGui_ImplOpenGL3_Shutdown();
    m_imguiOpenGlInitialized = false;
  }
  if (m_imguiGlfwInitialized) {
    ImGui_ImplGlfw_Shutdown();
    m_imguiGlfwInitialized = false;
  }
  if (m_imguiContextInitialized) {
    ImGui::DestroyContext();
    m_imguiContextInitialized = false;
  }

  if (!myOcctWindow.IsNull()) {
    myOcctWindow->Close();
    myOcctWindow.Nullify();
  }

  if (m_glfwInitialized) {
    glfwMakeContextCurrent(nullptr);
    glfwTerminate();
    m_glfwInitialized = false;
  }
}

void ViewController::Imgui_OpenCV_Ini0() {
  const std::string imagePath = CxRuntimeInitialImagePath().string();
  s_img0 = cv::imread(imagePath);
  if (s_img0.empty()) {
    std::cerr << "Failed to load initial Image View image: " << imagePath
              << std::endl;
    return;
  }

  UpdateImageViewImage(s_img0);
  m_scriptResult.image_ref = imagePath;
  m_scriptResult.log_lines.push_back("Initial Image View image loaded.");
}

vector<float> ViewController::createGaussianKernel(int kernelSize,
                                                   double sigma) {
  int kernelRadius = kernelSize / 2;
  vector<float> kernel(kernelSize * kernelSize, 0.0f);
  float sum = 0.0f;

  for (int j = -kernelRadius; j <= kernelRadius; ++j) {
    for (int i = -kernelRadius; i <= kernelRadius; ++i) {
      float r = sqrt(static_cast<float>(i * i + j * j));
      float val = exp(-(r * r) / (2 * sigma * sigma));
      kernel[(j + kernelRadius) * kernelSize + (i + kernelRadius)] = val;
      sum += val;
    }
  }

  for (float &val : kernel) {
    val /= sum;
  }

  return kernel;
}

#if CXCORE_ENABLE_VIEWCONTROLLER_CUDA
GLuint CreateTextureCubeX(cv::Mat &src) {
  if (src.empty()) {
    std::cerr << "Input image is empty!" << std::endl;
    return 0;
  }

  cv::Mat src_rgba;
  if (src.channels() == 3) {
    cv::cvtColor(src, src_rgba, cv::COLOR_BGR2RGBA);
  } else if (src.channels() == 4) {
    src_rgba = src;
  } else {
    std::cerr << "Unsupported number of channels: " << src.channels()
              << std::endl;
    return 0;
  }

  GLuint gl_texture_id;
  glGenTextures(1, &gl_texture_id);
  glBindTexture(GL_TEXTURE_2D, gl_texture_id);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, src.cols, src.rows, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, src_rgba.data);

  glBindTexture(GL_TEXTURE_2D, 0);

  cudaGraphicsResource *cuda_tex_resource;
  cudaCheckErrors(cudaGraphicsGLRegisterImage(
      &cuda_tex_resource, gl_texture_id, GL_TEXTURE_2D,
      cudaGraphicsMapFlagsWriteDiscard));

  void *dev_ptr;
  size_t num_bytes;
  cudaCheckErrors(cudaGraphicsMapResources(1, &cuda_tex_resource, 0));
  cudaCheckErrors(cudaGraphicsSubResourceGetMappedArray(
      (cudaArray **)&dev_ptr, cuda_tex_resource, 0, 0));

  cudaChannelFormatDesc channel_desc = cudaCreateChannelDesc<uchar4>();
  cudaMemcpyToArray((cudaArray *)dev_ptr, 0, 0, src_rgba.data,
                    src.cols * src.rows * sizeof(uchar4),
                    cudaMemcpyHostToDevice);

  cudaCheckErrors(cudaGraphicsUnmapResources(1, &cuda_tex_resource, 0));

  return gl_texture_id;
}

GLuint CreateTextureCubeY(cv::Mat &src) {
  if (src.empty()) {
    std::cerr << "Input image is empty!" << std::endl;
    return 0;
  }

  cv::Mat src_rgba;
  if (src.channels() == 3) {
    cv::cvtColor(src, src_rgba, cv::COLOR_BGR2RGBA);
  } else if (src.channels() == 4) {
    src_rgba = src.clone();
  } else {
    std::cerr << "Unsupported number of channels: " << src.channels()
              << std::endl;
    return 0;
  }

  const int imageW = src.cols;
  const int imageH = src.rows;

  GLuint gl_texture_id;
  glGenTextures(1, &gl_texture_id);
  glBindTexture(GL_TEXTURE_2D, gl_texture_id);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, imageW, imageH, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, nullptr);

  glBindTexture(GL_TEXTURE_2D, 0);

  cudaGraphicsResource *cuda_tex_resource;
  cudaCheckErrors(cudaGraphicsGLRegisterImage(
      &cuda_tex_resource, gl_texture_id, GL_TEXTURE_2D,
      cudaGraphicsMapFlagsWriteDiscard));

  cudaChannelFormatDesc channelDesc = cudaCreateChannelDesc<uchar4>();
  cudaArray *cu_array = nullptr;
  cudaCheckErrors(cudaMallocArray(&cu_array, &channelDesc, imageW, imageH));
  cudaCheckErrors(cudaMemcpyToArray(cu_array, 0, 0, src_rgba.data,
                                    imageW * imageH * sizeof(uchar4),
                                    cudaMemcpyHostToDevice));

  cudaTextureObject_t texImage0 = 0;
  {
    cudaResourceDesc resDesc = {};
    resDesc.resType = cudaResourceTypeArray;
    resDesc.res.array.array = cu_array;
    cudaTextureDesc texDesc = {};
    texDesc.addressMode[0] = cudaAddressModeClamp;
    texDesc.addressMode[1] = cudaAddressModeClamp;
    texDesc.filterMode = cudaFilterModePoint;
    texDesc.readMode = cudaReadModeElementType;
    texDesc.normalizedCoords = 0;
    cudaCheckErrors(
        cudaCreateTextureObject(&texImage0, &resDesc, &texDesc, nullptr));
  }

  uchar4 *d_dst = nullptr;
  cudaCheckErrors(cudaGraphicsMapResources(1, &cuda_tex_resource, 0));
  cudaCheckErrors(cudaGraphicsSubResourceGetMappedArray(
      (cudaArray **)&d_dst, cuda_tex_resource, 0, 0));

  runImageFiltersx((TColor *)d_dst, imageW, imageH, 0, texImage0);

  cudaCheckErrors(cudaGraphicsUnmapResources(1, &cuda_tex_resource, 0));

  cudaCheckErrors(cudaDestroyTextureObject(texImage0));
  cudaCheckErrors(cudaFreeArray(cu_array));
  cudaCheckErrors(cudaGraphicsUnregisterResource(cuda_tex_resource));

  return gl_texture_id;
}

GLuint CreateTextureCubeZ(cv::Mat &src) {
  if (src.empty()) {
    std::cerr << "Input image is empty!" << std::endl;
    return 0;
  }

  cv::Mat src_rgba;
  if (src.channels() == 3) {
    cv::cvtColor(src, src_rgba, cv::COLOR_BGR2RGBA);
  } else if (src.channels() == 4) {
    src_rgba = src.clone();
  } else {
    std::cerr << "Unsupported number of channels: " << src.channels()
              << std::endl;
    return 0;
  }

  const int imageW = src.cols;
  const int imageH = src.rows;

  GLuint gl_texture_id;
  glGenTextures(1, &gl_texture_id);
  glBindTexture(GL_TEXTURE_2D, gl_texture_id);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, imageW, imageH, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, nullptr);

  glBindTexture(GL_TEXTURE_2D, 0);

  cudaGraphicsResource *cuda_tex_resource;
  checkCudaErrors(cudaGraphicsGLRegisterImage(
      &cuda_tex_resource, gl_texture_id, GL_TEXTURE_2D,
      cudaGraphicsMapFlagsWriteDiscard));

  cudaTextureObject_t texImage = 0;
  uchar4 *h_Src = (uchar4 *)src_rgba.data;
  checkCudaErrors(CUDA_MallocArray(&h_Src, imageW, imageH));

  uchar4 *d_dst = nullptr;
  checkCudaErrors(cudaGraphicsMapResources(1, &cuda_tex_resource, 0));
  checkCudaErrors(cudaGraphicsSubResourceGetMappedArray(
      (cudaArray **)&d_dst, cuda_tex_resource, 0, 0));

  runImageFiltersx((TColor *)d_dst, imageW, imageH, 0, texImage);
  cudaDeviceSynchronize();

  checkCudaErrors(cudaGraphicsUnmapResources(1, &cuda_tex_resource, 0));

  checkCudaErrors(CUDA_FreeArray());
  checkCudaErrors(cudaGraphicsUnregisterResource(cuda_tex_resource));

  return gl_texture_id;
}

void ViewController::Imgui_GPU_NLM_main(cv::Mat &src_host, cv::Mat &dst_host) {
  if (src_host.empty() || src_host.channels() != 4) {

    if (src_host.empty() || src_host.channels() != 3) {
      throw std::invalid_argument("Source image must be 24-bit (3 channels).");
    }
    cv::Mat rgba;

    cv::cvtColor(src_host, rgba, cv::COLOR_BGR2BGRA);
  }

  int imageW = src_host.cols;
  int imageH = src_host.rows;
  size_t num_bytes = imageW * imageH * sizeof(uchar4);

  cudaChannelFormatDesc channelDesc = cudaCreateChannelDesc<uchar4>();
  cudaArray *cu_array = nullptr;
  checkCudaErrors(cudaMallocArray(&cu_array, &channelDesc, imageW, imageH));
  if (!src_host.isContinuous()) {
    cv::Mat tmp_src = src_host.clone();
    checkCudaErrors(cudaMemcpyToArray(cu_array, 0, 0, tmp_src.data, num_bytes,
                                      cudaMemcpyHostToDevice));
  } else {
    checkCudaErrors(cudaMemcpyToArray(cu_array, 0, 0, src_host.data, num_bytes,
                                      cudaMemcpyHostToDevice));
  }

  //
  struct cudaResourceDesc resDesc = {};
  memset(&resDesc, 0, sizeof(resDesc));
  resDesc.resType = cudaResourceTypeArray;
  resDesc.res.array.array = cu_array;

  struct cudaTextureDesc texDesc = {};
  memset(&texDesc, 0, sizeof(texDesc));
  texDesc.addressMode[0] = cudaAddressModeClamp;
  texDesc.addressMode[1] = cudaAddressModeClamp;
  texDesc.filterMode = cudaFilterModePoint;
  texDesc.readMode = cudaReadModeElementType;
  texDesc.normalizedCoords = 0;

  checkCudaErrors(
      cudaCreateTextureObject(&texImage, &resDesc, &texDesc, nullptr));

  TColor *d_dst = nullptr;
  checkCudaErrors(cudaMalloc(&d_dst, num_bytes));

  cuda_Copy(d_dst, imageW, imageH, texImage);

  size_t imageSize = src_host.step * src_host.rows;
  cv::Mat augmented_host(src_host.size(), src_host.type());
  checkCudaErrors(cudaMemcpy(augmented_host.data, d_dst, imageSize,
                             cudaMemcpyDeviceToHost));

  checkCudaErrors(cudaDestroyTextureObject(texImage));
  checkCudaErrors(cudaFreeArray(cu_array));
  checkCudaErrors(cudaFree(d_dst));
  dst_host = augmented_host;
}

void ViewController::Imgui_GPU_NLM_main0(cv::Mat &src_host, cv::Mat &dst_host) {
  TColor *d_dst = NULL;
  size_t num_bytes;
  uchar4 *h_Src = matToUchar4(src_host);

  imageW = src_host.cols;
  imageH = src_host.rows;
  CUDA_MallocArray(&h_Src, imageW, imageH);

  unsigned char *h_dst = NULL;
  cudaMalloc((void **)&d_dst, imageW * imageH * sizeof(TColor));
  h_dst = (unsigned char *)malloc(imageH * imageW * 4);

  int kernel = 1;

  runImageFiltersx(d_dst, imageH, imageW, 0, texImage);

  cudaDeviceSynchronize();

  cudaMemcpy(dst_host.data, d_dst, imageW * imageH * sizeof(TColor),
             cudaMemcpyDeviceToHost);

  CUDA_FreeArray();
  free(h_Src);
  cudaFree(d_dst);
  free(h_dst);
}
void ViewController::Imgui_GPU_Gauss_main(cv::Mat &src_host,
                                          cv::Mat &dst_host) {

  int kernelSize = 11;
  int kernelRadius = kernelSize / 2;
  double sigma = 0;

  if (sigma == 0) {
    sigma = 0.3 * ((kernelRadius - 1) * 0.5 - 1) + 0.8;
  }

  vector<float> h_kernel = createGaussianKernel(kernelSize, sigma);
  float *kernelData = h_kernel.data();

  unsigned char *d_input = nullptr, *d_output = nullptr;
  float *d_kernel = nullptr;
  size_t imageSize = src_host.step * src_host.rows;
  cudaMalloc(&d_input, imageSize);
  cudaMalloc(&d_output, imageSize);
  cudaMalloc(&d_kernel, sizeof(float) * h_kernel.size());
  cudaMemcpy(d_input, src_host.data, imageSize, cudaMemcpyHostToDevice);
  cudaMemcpy(d_kernel, kernelData, sizeof(float) * h_kernel.size(),
             cudaMemcpyHostToDevice);

  applyGaussianBlurOnGPU(d_input, d_output, src_host.cols, src_host.rows,
                         src_host.channels(), d_kernel, kernelRadius);

  cv::Mat blurred_host(src_host.size(), src_host.type());
  cudaMemcpy(blurred_host.data, d_output, imageSize, cudaMemcpyDeviceToHost);

  float hueDelta = 30.0f;
  float saturationScale = 1.2f;
  float valueScale = 1.1f;

  cudaMemcpy(d_input, blurred_host.data, imageSize, cudaMemcpyHostToDevice);

  applyHSVAugmentationOnGPU(d_input, src_host.cols, src_host.rows, hueDelta,
                            saturationScale, valueScale);

  cv::Mat augmented_host(src_host.size(), src_host.type());
  cudaMemcpy(augmented_host.data, d_input, imageSize, cudaMemcpyDeviceToHost);

  cudaFree(d_input);
  cudaFree(d_output);
  cudaFree(d_kernel);
  dst_host = augmented_host;
}

#else
void ViewController::Imgui_GPU_NLM_main(cv::Mat &src_host, cv::Mat &dst_host) {
  src_host.copyTo(dst_host);
}

void ViewController::Imgui_GPU_NLM_main0(cv::Mat &src_host, cv::Mat &dst_host) {
  src_host.copyTo(dst_host);
}

void ViewController::Imgui_GPU_Gauss_main(cv::Mat &src_host,
                                          cv::Mat &dst_host) {
  if (src_host.empty()) {
    dst_host.release();
    return;
  }

  cv::GaussianBlur(src_host, dst_host, cv::Size(11, 11), 0.0);
}
#endif

void ViewController::Imgui_OpenCV_Window0(bool *p_open) {
  (void)p_open;
  ImGui::Begin("Image Viewer");
  ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
              1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

  ImVec2 win_size = ImGui::GetWindowSize();
  float aspect_ratio = static_cast<float>(s_img0.cols) / s_img0.rows;
  ImVec2 img_display_size = ImVec2(win_size.x * 0.9f, win_size.y * 0.9f);
  if (img_display_size.x / img_display_size.y > aspect_ratio)
    img_display_size.x = img_display_size.y * aspect_ratio;
  else
    img_display_size.y = img_display_size.x / aspect_ratio;

  static bool use_text_color_for_tint = false;
  (void)use_text_color_for_tint;
  ImVec2 pos = ImGui::GetCursorScreenPos();
  ImVec2 uv_min = ImVec2(0.0f, 0.0f); // Top-left
  ImVec2 uv_max = ImVec2(1.0f, 1.0f); // Lower-right
  ImVec4 tint_col = use_text_color_for_tint
                        ? ImGui::GetStyleColorVec4(ImGuiCol_Text)
                        : ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // No tint
  ImVec4 border_col = ImGui::GetStyleColorVec4(ImGuiCol_Border);

  if (irunedge) {
    m_occtimage.copyFromMat(s_img0);
    m_occtimage.colorizeROI(cv::Scalar(0, 0, 0));
    if (iwedge) {
      if (ib2wedge) {
        Image octimageX;
        octimageX.copyFromMat(s_img0);
        octimageX.roi_7blur_gap_mud_thre_bw(ivalue1, 0, ivalue2, 1);
        m_occtimage.bitwiseOr(octimageX);
      }
      if (iw2bedge) {
        Image octimage0;
        octimage0.copyFromMat(s_img0);
        octimage0.roi_7blur_gap_mud_thre_bw(ivalue1, 0, ivalue2, 0);
        m_occtimage.bitwiseOr(octimage0);
      }
    }
    if (ihedge) {

      if (ib2wedge) {
        Image octimage1;
        octimage1.copyFromMat(s_img0);
        octimage1.roi_7blur_gap_mud_thre_bw_h(ivalue1, 0, ivalue2, 1);
        m_occtimage.bitwiseOr(octimage1);
      }
      if (iw2bedge) {
        Image octimage2;
        octimage2.copyFromMat(s_img0);
        octimage2.roi_7blur_gap_mud_thre_bw_h(ivalue1, 0, ivalue2, 0);
        m_occtimage.bitwiseOr(octimage2);
      }
    }

    SetBackgroundInView(m_myView, m_occtimage.getmat());
  }
  if (ipythre) {
    m_occtimage.copyFromMat(s_img0);

    Image octimagez0 = m_occtimage.getROI();
    Image binaryImage =
        octimagez0.pyramidDynamicThresholding(ivalue3, ivalue4, 0);

    m_occtimage.colorizeROI(cv::Scalar(0, 0, 0));
    m_occtimage.copyResizedToROI(binaryImage.getmat());

    //
    texture_id0 = CreateTextureFromMat0(m_occtimage.getmat());
    SetBackgroundInView(m_myView, m_occtimage.getmat());
  }
  if (iotsuThreshold) {
    m_occtimage.copyFromMat(s_img0);

    Image octimagez0 = m_occtimage.getROI();

    Image otsuBinaryImage =
        octimagez0.otsuThresholding(255, cv::THRESH_BINARY | cv::THRESH_OTSU);

    m_occtimage.colorizeROI(cv::Scalar(0, 0, 0));
    m_occtimage.copyResizedToROI(otsuBinaryImage.getmat());
    texture_id0 = CreateTextureFromMat0(m_occtimage.getmat());
    SetBackgroundInView(m_myView, m_occtimage.getmat());
  }
  if (0) {
    m_occtimage.copyFromMat(s_img0);

    Image octimagez0 = m_occtimage.getROI();
    Image adaptiveBinaryImage =
        octimagez0.adaptiveThresholding(255, cv::ADAPTIVE_THRESH_GAUSSIAN_C,
                                        cv::THRESH_BINARY, ivalue5, ivalue6);

    m_occtimage.colorizeROI(cv::Scalar(0, 0, 0));
    m_occtimage.copyResizedToROI(adaptiveBinaryImage.getmat());
    texture_id0 = CreateTextureFromMat0(m_occtimage.getmat());
    SetBackgroundInView(m_myView, m_occtimage.getmat());
  }

  if (1 == opencvblur) {
    if (1) {
      s_img0 = cv::imread("0.jpg");
      SetTexturedtoBoxFace(s_img0);
    }
    if (0) {
      m_occtimage.copyFromMat(s_img0);
      Image adaptiveBinaryImage = m_occtimage.adaptiveThresholding(
          255, cv::ADAPTIVE_THRESH_GAUSSIAN_C, cv::THRESH_BINARY, 11, 2);

      texture_id0 = CreateTextureFromMat0(adaptiveBinaryImage.getmat());
      SetBackgroundInView(m_myView, adaptiveBinaryImage.getmat());

      if (0) {
        m_occtimage.copyFromMat(s_img0);
        texture_id0 = CreateTextureFromMat0(m_occtimage.getmat());
        SetBackgroundInView(m_myView, m_occtimage.getmat());
      }
    }
    if (0) {
      cv::Mat blurred;
      GaussianBlur(s_img0, blurred, cv::Size(17, 17), 0);
      texture_id0 = CreateTextureFromMat0(blurred);
      SetBackgroundInView(m_myView, blurred);
    } else {
      if (0) {
        Image octimagez;
        octimagez.copyFromMat(s_img0);
        octimagez.analyzeConnectedComponentsColor(50.0, 100, 0.5, 2.0);
        octimagez.analyzeConnectedComponentsPyramid(100, 0.5, 2.0);
        texture_id0 = CreateTextureFromMat0(octimagez.getmat());

        SetBackgroundInView(m_myView, octimagez.getmat());
      }
      if (0) {
        Image octimagez;
        octimagez.copyFromMat(s_img0);
        Image binaryImage = octimagez.pyramidDynamicThresholding(4, 11, 0);
        texture_id0 = CreateTextureFromMat0(binaryImage.getmat());
        SetBackgroundInView(m_myView, binaryImage.getmat());
      }
      if (0) {
        Image octimagez;
        octimagez.copyFromMat(s_img0);
        Image octimagez0 = octimagez.getROI();
        octimagez0.colorFillConnectedComponents(100, 50, 50,
                                                cv::Scalar(0, 0, 255));
        //
        texture_id0 = CreateTextureFromMat0(octimagez0.getmat());
        SetBackgroundInView(m_myView, octimagez0.getmat());
      }
    }
    opencvblur = 0;
  } else if (1 == gpublur) {
    cv::Mat blurred;

    texture_id0 = CreateTextureCube(s_img0);
#if CXCORE_ENABLE_VIEWCONTROLLER_CUDA
    UpdateTextureWithCuda(2048, 1536);
#else
    SetBackgroundInView(m_myView, s_img0);
#endif
    gpublur = 0;
  } else if (0 == opencvblur && 0 == texture_id0) {
    //
    texture_id0 = CreateTextureFromMat0(s_img0);
    SetBackgroundInView(m_myView, s_img0);
  } else if (1 == opencvreset) {
    //
    texture_id0 = CreateTextureFromMat0(s_img0);
    SetBackgroundInView(m_myView, s_img0);
    opencvreset = 0;
  }

  ImGui::Image((ImTextureID)(uint64_t)texture_id0, img_display_size,
               ImVec2(0, 0), ImVec2(1, 1));

  if (ImGui::IsItemHovered()) {
    ImGui::BeginTooltip();
    ImGuiIO &io0 = ImGui::GetIO();
    float my_tex_w = img_display_size.x;
    float my_tex_h = img_display_size.y;
    float region_sz = 32.0f;
    float region_x = io0.MousePos.x - pos.x - region_sz * 0.5f;
    float region_y = io0.MousePos.y - pos.y - region_sz * 0.5f;
    float zoom = 10.0f;

    if (region_x < 0.0f) {
      region_x = 0.0f;
    } else if (region_x > my_tex_w - region_sz) {
      region_x = my_tex_w - region_sz;
    }
    if (region_y < 0.0f) {
      region_y = 0.0f;
    } else if (region_y > my_tex_h - region_sz) {
      region_y = my_tex_h - region_sz;
    }
    ImGui::Text("Min: (%.2f, %.2f)", region_x, region_y);
    ImGui::Text("Max: (%.2f, %.2f)", region_x + region_sz,
                region_y + region_sz);
    ImVec2 uv0 = ImVec2((region_x) / my_tex_w, (region_y) / my_tex_h);
    ImVec2 uv1 = ImVec2((region_x + region_sz) / my_tex_w,
                        (region_y + region_sz) / my_tex_h);

    ImGui::Image((ImTextureID)(uint64_t)texture_id0,
                 ImVec2(region_sz * zoom, region_sz * zoom), uv0, uv1, tint_col,
                 border_col); //
    ImGui::EndTooltip();
  }
  ImGui::End();
}
void ViewController::onResize(int theWidth, int theHeight) {
  if (theWidth != 0 && theHeight != 0 && !m_myView.IsNull()) {
    m_myView->Window()->DoResize();
    m_myView->MustBeResized();
    m_myView->Invalidate();
    m_myView->Redraw();
  }
}
void ViewController::onMouseScroll(double theOffsetX, double theOffsetY) {
  if (ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse)
    return;
  (void)theOffsetX;
  if (!m_myView.IsNull()) {
    UpdateZoom(Aspect_ScrollDelta(myOcctWindow->CursorPosition(),
                                  int(theOffsetY * 8.0)));
  }
}
void ViewController::SetParserValue(const string &codestr, double dvalue) {
  if (!m_parserOwner.IsObjectVar(codestr.c_str()))
    return;
  string astr(codestr + "=" + formatNumber(dvalue) + ";");
  m_parserOwner.Compile(astr.c_str());
}
double ViewController::GetParserValue(const string &codestr) {
  double *pdouble = NULL;
  if (!m_parserOwner.IsObjectVar((const char *)codestr.c_str()))
    return 0x00;
  pdouble =
      (double *)m_parserOwner.GetDoubleValue((const char *)codestr.c_str());
  if (NULL == pdouble)
    return 0x00;
  double dvalue = 0;
  dvalue = (*pdouble);
  if (dvalue < 0.0000001 && dvalue > 0)
    return 0;
  return dvalue;
}
string ViewController::initialparser() {
  string str;
  bool bresult = m_parserOwner.Compile("Module amodule;");
  if (!bresult) {
    str = str + "build Module fail!\r\n";
    str = str + getoutputstring();
  } else {
    str = str + "build Module ok\r\n";
  }
  clearos();
  string strfile = getlocationstring("./static.h");
  string strcode = loadfilestring(strfile);
  bresult = m_parserOwner.Compile(strcode.c_str());
  if (!bresult) {
    str = str + "build " + strfile + " fail!\r\n";
    str = str + getoutputstring();
  } else {
    str = str + "build " + strfile + " ok\r\n";
  }
  clearos();
  vector<string> files;
  files = DirFileFind(getlocationstring(string("./")), string("*.ums"));
  for (int i = 0; i < files.size(); ++i) {
    string filename = files[i];

    string getfilename = getFullFileName(filename);

    m_strcode = loadfilestring(filename);

    bresult = m_parserOwner.Compile(m_strcode.c_str());
    if (!bresult) {
      str = str + "build " + getfilename + " fail!\r\n";
      str = str + getoutputstring();
    } else {
      str = str + "build " + getfilename + " ok\r\n";
    }
    clearos();
  }
  for (int i = 0; i < m_parserOwner.ObjectCount("Image"); i++) {
    Image *pimage = (Image *)m_parserOwner.GetClassObj("Image", i);
    if (nullptr != pimage)
      *pimage = Image(2048, 1536, CV_32FC3);
  }

  m_dzoomx = GetParserValue("m_dzoomx");
  m_dzoomy = GetParserValue("m_dzoomy");
  if (0.0 == m_dzoomx)
    m_dzoomx = 1;
  if (0.0 == m_dzoomy)
    m_dzoomy = 1;
  return str;
}
void ViewController::clearos() {
  m_os.str("");
  m_os.clear();
}
void ViewController::clearcreateos() {
  m_createcodeos.str("");
  m_createcodeos.clear();
}
void ViewController::clearparserobject() { m_parserOwner.ClearAll(); }
void ViewController::resetparser() {
  clearparserobject();
  initialparser();
}
Shape *ViewController::indexAt(const gp_Pnt &pos) {
  int isize = m_parserOwner.ObjectCount("Shape");
  for (int i = 0; i < isize; i++) {
    Shape *pshape = (Shape *)m_parserOwner.GetClassObj("Shape", i);

    if (pshape->rect().contains(pos))
      if (pshape->show())
        return pshape;
  }
  isize = m_parserOwner.ObjectCount("FindLine");
  for (int i = 0; i < isize; i++) {
    Shape *pshape = (Shape *)m_parserOwner.GetClassObj("FindLine", i);

    if (pshape->rect().contains(pos))
      if (pshape->show())
        return pshape;
  }
  isize = m_parserOwner.ObjectCount("FindCircle");
  for (int i = 0; i < isize; i++) {
    Shape *pshape = (Shape *)m_parserOwner.GetClassObj("FindCircle", i);

    if (pshape->rect().contains(pos))
      if (pshape->show())
        return pshape;
  }
  isize = m_parserOwner.ObjectCount("ASR");
  for (int i = 0; i < isize; i++) {
    Shape *pshape = (Shape *)m_parserOwner.GetClassObj("ASR", i);

    if (pshape->rect().contains(pos))
      if (pshape->show())
        return pshape;
  }
  isize = m_parserOwner.ObjectCount("FindObject");
  for (int i = 0; i < isize; i++) {
    Shape *pshape = (Shape *)m_parserOwner.GetClassObj("FindObject", i);

    if (pshape->rect().contains(pos))
      if (pshape->show())
        return pshape;
  }
  isize = m_parserOwner.ObjectCount("Imageroi");
  for (int i = 0; i < isize; i++) {
    Shape *pshape = (Shape *)m_parserOwner.GetClassObj("Imageroi", i);

    if (pshape->rect().contains(pos))
      if (pshape->show())
        return pshape;
  }
  isize = m_parserOwner.ObjectCount("imagecodeparser");
  for (int i = 0; i < isize; i++) {
    Shape *pshape = (Shape *)m_parserOwner.GetClassObj("imagecodeparser", i);

    if (pshape->rect().contains(pos))
      if (pshape->show())
        return pshape;
  }
  isize = m_parserOwner.ObjectCount("gridobject");
  for (int i = 0; i < isize; i++) {
    Shape *pshape = (Shape *)m_parserOwner.GetClassObj("gridobject", i);

    if (pshape->rect().contains(pos))
      if (pshape->show())
        return pshape;
  }
  isize = m_parserOwner.ObjectCount("FastMatch");
  for (int i = 0; i < isize; i++) {
    Shape *pshape = (Shape *)m_parserOwner.GetClassObj("FastMatch", i);

    if (pshape->rect().contains(pos))
      if (pshape->show())
        return pshape;
  }
  isize = m_parserOwner.ObjectCount("easyorc");
  for (int i = 0; i < isize; i++) {
    Shape *pshape = (Shape *)m_parserOwner.GetClassObj("easyorc", i);

    if (pshape->rect().contains(pos))
      if (pshape->show())
        return pshape;
  }
  return 0;
}

void ViewController::onMouseButton(int theButton, int theAction, int theMods) {
  const bool imguiCapturesMouse =
      ImGui::GetCurrentContext() != nullptr &&
      (ImGui::GetIO().WantCaptureMouse ||
       ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) ||
       ImGui::IsAnyItemHovered() || ImGui::IsAnyItemActive());
  if (imguiCapturesMouse) {
    isDragging = false;
    return;
  }
  if (!m_myView.IsNull()) {
    const Graphic3d_Vec2i aPos = myOcctWindow->CursorPosition();
    if (theAction == GLFW_PRESS) {
      m_mousePressPos = gp_Pnt(aPos.x(), aPos.y(), 0);
      isDragging = true;
      myContext->Select(true);
      if (true == m_ilinescan && 0 == theButton) {
        if (m_mousePressPos.X() >= m_current_window_posx &&
            m_mousePressPos.Y() >= m_current_window_posy &&
            m_mousePressPos.X() < m_current_window_posx + m_imguiw &&
            m_mousePressPos.Y() < m_current_window_posy + m_imguih) {
        } else if (0 == m_ibtntimes) {
          m_point0 = gp_Pnt(aPos.x(), aPos.y(), 0);
        }
      } else if (true == m_ipickpoints && 0 == theButton) {
        if (m_mousePressPos.X() >= m_current_window_posx &&
            m_mousePressPos.Y() >= m_current_window_posy &&
            m_mousePressPos.X() < m_current_window_posx + m_imguiw &&
            m_mousePressPos.Y() < m_current_window_posy + m_imguih) {
        } else {
          PointsShape *points = static_cast<PointsShape *>(
              m_parserOwner.GetClassObj("PointsShape", "apoints0"));
          if (points != nullptr) {
            points->addpoint((int)(m_mousePressPos.X() * m_dscalex),
                             (int)(m_mousePressPos.Y() * m_dscaley));
            points->setcolor(0, 0, 250);
            points->setshow(1);
          }
        }
      } else if (true == m_ipickpoints && 1 == theButton) {
        if (m_mousePressPos.X() >= m_current_window_posx &&
            m_mousePressPos.Y() >= m_current_window_posy &&
            m_mousePressPos.X() < m_current_window_posx + m_imguiw &&
            m_mousePressPos.Y() < m_current_window_posy + m_imguih) {
        } else {
          PointsShape *points = static_cast<PointsShape *>(
              m_parserOwner.GetClassObj("PointsShape", "apoints1"));
          if (points != nullptr) {
            points->addpoint((int)(m_mousePressPos.X() * m_dscalex),
                             (int)(m_mousePressPos.Y() * m_dscaley));
            points->setcolor(250, 0, 0);
            points->setshow(1);
          }
        }
      } else if (true == m_ipickpoints && 2 == theButton) {
        if (m_mousePressPos.X() >= m_current_window_posx &&
            m_mousePressPos.Y() >= m_current_window_posy &&
            m_mousePressPos.X() < m_current_window_posx + m_imguiw &&
            m_mousePressPos.Y() < m_current_window_posy + m_imguih) {
        } else {
          PointsShape *pointsA = static_cast<PointsShape *>(
              m_parserOwner.GetClassObj("PointsShape", "apoints0"));
          PointsShape *pointsB = static_cast<PointsShape *>(
              m_parserOwner.GetClassObj("PointsShape", "apoints1"));
          if (pointsA != nullptr)
            pointsA->clear();
          if (pointsB != nullptr)
            pointsB->clear();
        }
      } else if (true == m_iattachline && 0 == theButton) {
        if (m_mousePressPos.X() >= m_current_window_posx &&
            m_mousePressPos.Y() >= m_current_window_posy &&
            m_mousePressPos.X() < m_current_window_posx + m_imguiw &&
            m_mousePressPos.Y() < m_current_window_posy + m_imguih) {
        } else {
          PointsShape *points = static_cast<PointsShape *>(
              m_parserOwner.GetClassObj("PointsShape", "apoints0"));
          if (points != nullptr) {
            points->addpoint((int)(m_mousePressPos.X() * m_dscalex),
                             (int)(m_mousePressPos.Y() * m_dscaley));
            points->setcolor(0, 250, 0);
            points->setshow(8);
          }
        }
      }

      if (myContext->HasDetected()) {
        Handle(AIS_InteractiveObject) detectedShape =
            myContext->DetectedInteractive();
        selectedShape = Handle(AIS_Shape)::DownCast(detectedShape);
      }
      PressMouseButton(aPos, mouseButtonFromGlfw(theButton),
                       keyFlagsFromGlfw(theMods), false);
    } else {

      ReleaseMouseButton(aPos, mouseButtonFromGlfw(theButton),
                         keyFlagsFromGlfw(theMods), false);

      if (m_mousePressPos.X() >= m_current_window_posx &&
          m_mousePressPos.Y() >= m_current_window_posy &&
          m_mousePressPos.X() < m_current_window_posx + m_imguiw &&
          m_mousePressPos.Y() < m_current_window_posy + m_imguih) {
      } else if (true == m_ilinescan && 1 == theButton) {
        if (0 == m_ibtntimes) {
          m_ibtntimes = 1;
        } else if (1 == m_ibtntimes) {
          m_ibtntimes = 0;
          m_point1 = gp_Pnt(aPos.x(), aPos.y(), 0);
          FindLine *findLine = static_cast<FindLine *>(
              m_parserOwner.GetClassObj("FindLine", "afindline"));
          if (findLine != nullptr) {
            findLine->setlinesegment(
                m_point0.X() * m_dscalex, m_point0.Y() * m_dscaley,
                m_point1.X() * m_dscalex, m_point1.Y() * m_dscaley, 80);
            findLine->setshow(1);
          }
        }
      }
      if (m_mousePressPos.X() >= m_current_window_posx &&
          m_mousePressPos.Y() >= m_current_window_posy &&
          m_mousePressPos.X() < m_current_window_posx + m_imguiw &&
          m_mousePressPos.Y() < m_current_window_posy + m_imguih) {
      } else if (true != m_ipickpoints)
        if (aPos.x() - m_mousePressPos.X() > 100 &&
            aPos.y() - m_mousePressPos.Y() > 100) {
          Shape *shape = static_cast<Shape *>(
              m_parserOwner.GetClassObj("Shape", "ashape0"));
          if (shape != nullptr) {
            shape->settype(Shape::Rectangle);
            shape->setrect(
                m_mousePressPos.X() * m_dscalex,
                m_mousePressPos.Y() * m_dscaley,
                aPos.x() * m_dscalex - m_mousePressPos.X() * m_dscalex,
                aPos.y() * m_dscaley - m_mousePressPos.Y() * m_dscaley);
            shape->setshow(1);
          }
        }
      isDragging = false;
    }

    if (0) {
      double dx = (aPos.x() / m_dzoomx) - m_dmovx;
      double dy = (aPos.y() / m_dzoomy) - m_dmovy;
      gp_Pnt curpos(dx, dy, 0);
      Shape *pshape = indexAt(curpos);
      if (pshape != 0) {
        m_resizeHandlePressed =
            pshape->resizeHandlez(m_dzoomx, m_dzoomy).contains(curpos);
        if (m_resizeHandlePressed) {
          m_mousePressOffset.SetX(pshape->rect().BottomRight().X() -
                                  curpos.X());
          m_mousePressOffset.SetY(pshape->rect().BottomRight().Y() -
                                  curpos.Y());
        } else {
          m_mousePressOffset.SetX(curpos.X() - pshape->rect().TopLeft().X());
          m_mousePressOffset.SetY(curpos.Y() - pshape->rect().TopLeft().Y());
        }
        m_pmousepressshape = pshape;
      }
    }
  }
}

void ViewController::onMouseMove(int thePosX, int thePosY) {
  const bool imguiCapturesMouse =
      ImGui::GetCurrentContext() != nullptr &&
      (ImGui::GetIO().WantCaptureMouse ||
       ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) ||
       ImGui::IsAnyItemHovered() || ImGui::IsAnyItemActive());
  if (imguiCapturesMouse) {
    isDragging = false;
    return;
  }
  const Graphic3d_Vec2i aNewPos(thePosX, thePosY);
  if (true == isDragging) {
    if (m_mousePressPos.X() >= m_current_window_posx &&
        m_mousePressPos.Y() >= m_current_window_posy &&
        m_mousePressPos.X() < m_current_window_posx + m_imguiw &&
        m_mousePressPos.Y() < m_current_window_posy + m_imguih) {
    } else if (false == m_ipickpoints)
      if (thePosX - m_mousePressPos.X() > 10 &&
          thePosY - m_mousePressPos.Y() > 10) {
        Shape *shape = static_cast<Shape *>(
            m_parserOwner.GetClassObj("Shape", "ashape0"));
        if (shape != nullptr) {
          shape->settype(Shape::Rectangle);
          shape->setrect(
              m_mousePressPos.X() * m_dscalex,
              m_mousePressPos.Y() * m_dscaley,
              thePosX * m_dscalex - m_mousePressPos.X() * m_dscalex,
              thePosY * m_dscaley - m_mousePressPos.Y() * m_dscaley);
          shape->setshow(1);
        }
      }
    if (0)
      if (false == ismove) {
        ismove = true;
        m_shape0.setcontext(myContext);
        m_shape0.settype(Shape::Rectangle);
        m_shape0.setrect(m_mousePressPos.X(), m_mousePressPos.Y(),
                         thePosX - m_mousePressPos.X(),
                         thePosY - m_mousePressPos.Y());
        ismove = false;
      }
  }
  if (!m_myView.IsNull()) {
    if (myContext->HasDetected()) {
      UpdateMousePosition(aNewPos, PressedMouseButtons(), LastMouseFlags(),
                          false);
    }
  }
}

#include "CxRuntimeProjectionExecutor.h"
#include "CxShapeInteractionRunner.h"
#include "CxUnifiedLog.h"
#include "FastMatch.h"
#include "FindCircle.h"
#include "FindEllipse.h"
#include "FindLine.h"
#include "FindRect.h"
#include "FindSegmentation.h"
#include "GridPatternClassTool.h"
#include "RegionPatternTool.h"

#include <chrono>
#include <iomanip>
#include <sstream>

static std::string GenerateLocalShapeRunId() {
  auto now = std::chrono::system_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      now.time_since_epoch());
  std::stringstream ss;
  ss << "shape_local_" << std::setw(16) << std::setfill('0') << ms.count();
  return ss.str();
}

bool ViewController::RunShapeInteractionSmoke(
    const std::string &tool_manifest_path, const std::string &suite_path,
    const std::string &image_manifest_path, const std::string &out_dir,
    CxShapeInteractionBatchResult &result) {
  std::string init_reason;
  if (!m_parserOwner.Initialize(init_reason)) {
    result.pass = false;
    CXLOG_ERROR("ViewController", "shape_smoke_dispatch", "failed",
                "parser initialize failed: " + init_reason);
    return false;
  }

  CxShapeInteractionRunner runner;
  CxShapeInteractionOptions options;
  options.tool_manifest_path = tool_manifest_path;
  options.test_suite_path = suite_path;
  options.out_dir = out_dir;

  options.run_id = CxUnifiedLog::Instance().IsInitialized()
                       ? CxUnifiedLog::Instance().RunId()
                       : GenerateLocalShapeRunId();

  options.unified_log_enabled = CxUnifiedLog::Instance().IsInitialized();
  options.unified_log_path = CxUnifiedLog::Instance().IsInitialized()
                                 ? CxUnifiedLog::Instance().Path().string()
                                 : "";
  options.unified_log_status =
      options.unified_log_enabled ? "initialized" : "not_initialized";

  CXLOG_INFO("ViewController", "shape_smoke_dispatch", "running",
             "run_id=" + options.run_id + ", suite=" + suite_path +
                 ", manifest=" + tool_manifest_path + ", image_manifest=" +
                 image_manifest_path + ", out=" + out_dir);

  CxAnnotationToolManifestSnapshot manifest;
  CxShapeTestSuiteSnapshot suite;
  CxScriptImageManifestRuntime image_manifest;
  std::string reason;

  result.manifest_path = tool_manifest_path;
  result.suite_path = suite_path;

  bool manifest_exists = std::filesystem::exists(tool_manifest_path);
  bool suite_exists = std::filesystem::exists(suite_path);

  if (!m_parserOwner.ParseAnnotationToolManifest(tool_manifest_path, manifest,
                                                 reason)) {
    result.pass = false;
    result.failure_stage = "manifest_parse";
    result.reason = reason;

    CXLOG_ERROR("ViewController", "shape_smoke_dispatch", "failed",
                "parse annotation manifest failed: " + reason);

    WriteShapeSuiteLoadReport(out_dir, tool_manifest_path, suite_path,
                              manifest_exists, false, suite_exists, false, 0,
                              result.failure_stage, result.reason);

    return false;
  }

  if (!m_parserOwner.ParseShapeInteractionSuite(suite_path, suite, reason)) {
    result.pass = false;
    result.failure_stage = "suite_parse";
    result.reason = reason;

    CXLOG_ERROR("ViewController", "shape_smoke_dispatch", "failed",
                "parse shape suite failed: " + reason);

    WriteShapeSuiteLoadReport(out_dir, tool_manifest_path, suite_path,
                              manifest_exists, true, suite_exists, false, 0,
                              result.failure_stage, result.reason);

    return false;
  }

  bool image_manifest_loaded = false;
  if (!image_manifest_path.empty()) {
    if (!LoadStage25ImageManifestJson(image_manifest_path, image_manifest,
                                      reason)) {
      result.pass = false;
      result.failure_stage = "image_manifest_load";
      result.reason = reason;

      CXLOG_ERROR("ViewController", "shape_smoke_dispatch", "failed",
                  "load image manifest failed: " + reason);

      return false;
    }

    auto validation = ValidateStage25ImageManifest(image_manifest);
    WriteManifestDryRunReport(image_manifest, out_dir);

    if (!validation.ok) {
      result.pass = false;
      result.failure_stage = "image_manifest_validation";
      result.reason = "image manifest validation failed";

      CXLOG_ERROR("ViewController", "shape_smoke_dispatch", "failed",
                  "image manifest validation failed");

      return false;
    }

    image_manifest_loaded = true;
  }

  CxRuntimeProjectionExecutor executor;
  CxShapeInteractionBatchResultEx ex_result;
  const bool ok = runner.RunSuite(manifest, suite, image_manifest, executor,
                                  options, ex_result);

  result.pass = ex_result.pass;
  for (const auto &ex_case : ex_result.extended_cases) {
    CxShapeInteractionCaseResult cr;
    cr.case_id = ex_case.case_id;
    cr.tool_id = ex_case.tool_id;
    cr.shape_kind = ex_case.shape_kind;
    cr.pass = ex_case.pass;
    cr.conclusion = ex_case.status;
    cr.reason = ex_case.reason;
    result.cases.push_back(cr);
  }

  return ok;
}

void ViewController::SyncRuntimeObjectsToShapeElements() {
  if (!m_runtimeShapeSyncExecuting) {
    RequestRuntimeShapeSync("direct_sync_call_redirected");
    return;
  }

  if (m_annotationLayer.HasActiveDrag() || m_annotationDragging) {
    RequestRuntimeShapeSync("active_drag");
    return;
  }

  if (m_annotationImageWidth <= 1.0f || m_annotationImageHeight <= 1.0f) {
    RequestRuntimeShapeSync("image_canvas_not_ready");
    return;
  }

  if (m_manualTest.runtime_objects.empty()) {
    return;
  }

  for (const RuntimeObjectView &object : m_manualTest.runtime_objects) {
    if (object.type == "FindLine") {
      FindLine *tool = static_cast<FindLine *>(
          m_parserDebugBridge.QueryClassObject("FindLine", object.name));
      if (tool != nullptr) {
        const uint64_t generation =
            m_annotationLayer.BeginRuntimeOwnerPublish("FindLine", object.name);
        tool->PublishDisplayShapes(m_annotationLayer, object.name);
        m_annotationLayer.EndRuntimeOwnerPublish("FindLine", object.name,
                                                 generation);
      }
    } else if (object.type == "FindCircle") {
      FindCircle *tool = static_cast<FindCircle *>(
          m_parserDebugBridge.QueryClassObject("FindCircle", object.name));
      if (tool != nullptr) {
        const uint64_t generation = m_annotationLayer.BeginRuntimeOwnerPublish(
            "FindCircle", object.name);
        tool->PublishDisplayShapes(m_annotationLayer, object.name);
        m_annotationLayer.EndRuntimeOwnerPublish("FindCircle", object.name,
                                                 generation);
      }
    } else if (object.type == "FindEllipse") {
      FindEllipse *tool = static_cast<FindEllipse *>(
          m_parserDebugBridge.QueryClassObject("FindEllipse", object.name));
      if (tool != nullptr) {
        const uint64_t generation = m_annotationLayer.BeginRuntimeOwnerPublish(
            "FindEllipse", object.name);
        tool->PublishDisplayShapes(m_annotationLayer, object.name);
        m_annotationLayer.EndRuntimeOwnerPublish("FindEllipse", object.name,
                                                 generation);
      }
    } else if (object.type == "FindRect") {
      FindRect *tool = static_cast<FindRect *>(
          m_parserDebugBridge.QueryClassObject("FindRect", object.name));
      if (tool != nullptr) {
        const uint64_t generation =
            m_annotationLayer.BeginRuntimeOwnerPublish("FindRect", object.name);
        tool->PublishDisplayShapes(m_annotationLayer, object.name);
        m_annotationLayer.EndRuntimeOwnerPublish("FindRect", object.name,
                                                 generation);
      }
    } else if (object.type == "FastMatch") {
      FastMatch *tool = static_cast<FastMatch *>(
          m_parserDebugBridge.QueryClassObject("FastMatch", object.name));
      if (tool != nullptr) {
        const uint64_t generation = m_annotationLayer.BeginRuntimeOwnerPublish(
            "FastMatch", object.name);
        tool->PublishDisplayShapes(m_annotationLayer, object.name);
        m_annotationLayer.EndRuntimeOwnerPublish("FastMatch", object.name,
                                                 generation);
      }
    } else if (object.type == "GridPatternClassTool") {
      GridPatternClassTool *tool = static_cast<GridPatternClassTool *>(
          m_parserDebugBridge.QueryClassObject("GridPatternClassTool",
                                               object.name));
      if (tool != nullptr) {
        const uint64_t generation = m_annotationLayer.BeginRuntimeOwnerPublish(
            "GridPatternClassTool", object.name);
        tool->PublishDisplayShapes(m_annotationLayer, object.name);
        m_annotationLayer.EndRuntimeOwnerPublish("GridPatternClassTool",
                                                 object.name, generation);
      }
    } else if (object.type == "RegionPatternTool") {
      RegionPatternTool *tool =
          static_cast<RegionPatternTool *>(m_parserDebugBridge.QueryClassObject(
              "RegionPatternTool", object.name));
      if (tool != nullptr) {
        const uint64_t generation = m_annotationLayer.BeginRuntimeOwnerPublish(
            "RegionPatternTool", object.name);
        tool->PublishDisplayShapes(m_annotationLayer, object.name);
        m_annotationLayer.EndRuntimeOwnerPublish("RegionPatternTool",
                                                 object.name, generation);
      }
    } else if (object.type == "FindSegmentation") {
      FindSegmentation *tool =
          static_cast<FindSegmentation *>(m_parserDebugBridge.QueryClassObject(
              "FindSegmentation", object.name));
      if (tool != nullptr) {
        const uint64_t generation = m_annotationLayer.BeginRuntimeOwnerPublish(
            "FindSegmentation", object.name);
        tool->PublishDisplayShapes(m_annotationLayer, object.name);
        m_annotationLayer.EndRuntimeOwnerPublish("FindSegmentation",
                                                 object.name, generation);
      }
    } else if (object.type == "FindObject") {
      FindObject *tool = static_cast<FindObject *>(
          m_parserDebugBridge.QueryClassObject("FindObject", object.name));
      if (tool != nullptr) {
        const uint64_t generation = m_annotationLayer.BeginRuntimeOwnerPublish(
            "FindObject", object.name);
        tool->PublishDisplayShapes(m_annotationLayer, object.name);
        m_annotationLayer.EndRuntimeOwnerPublish("FindObject", object.name,
                                                 generation);
      }
    }
  }
}

void ViewController::RequestRuntimeShapeSync(const std::string &reason) {
  m_runtimeShapeSyncPending = true;
  if (!reason.empty()) {
    if (!m_runtimeShapeSyncReason.empty())
      m_runtimeShapeSyncReason += "|";
    m_runtimeShapeSyncReason += reason;
  }
  SetCxCrashBreadcrumb("RequestRuntimeShapeSync");
}

void ViewController::ProcessDeferredRuntimeShapeSync(const char *phase) {
  if (!m_runtimeShapeSyncPending)
    return;

  SetCxCrashBreadcrumb("ProcessDeferredRuntimeShapeSync:begin");

  if (m_annotationLayer.HasActiveDrag() || m_annotationDragging) {
    ++m_runtimeShapeSyncDeferCount;
    SetCxCrashBreadcrumb("ProcessDeferredRuntimeShapeSync:defer_active_drag");
    return;
  }

  if (m_annotationImageWidth <= 1.0f || m_annotationImageHeight <= 1.0f) {
    ++m_runtimeShapeSyncDeferCount;
    SetCxCrashBreadcrumb(
        "ProcessDeferredRuntimeShapeSync:defer_canvas_not_ready");
    return;
  }

  const std::string reason =
      m_runtimeShapeSyncReason.empty()
          ? std::string("phase=") + (phase ? phase : "unknown")
          : (m_runtimeShapeSyncReason +
             ", phase=" + (phase ? phase : "unknown"));

  m_runtimeShapeSyncPending = false;
  m_runtimeShapeSyncReason.clear();
  m_runtimeShapeSyncDeferCount = 0;

  SetCxCrashBreadcrumb("ProcessDeferredRuntimeShapeSync:execute");
  CXLOG_INFO("ViewController", "runtime_shape_sync", "running", reason);
  m_runtimeShapeSyncExecuting = true;
  SyncRuntimeObjectsToShapeElements();
  m_runtimeShapeSyncExecuting = false;
  CXLOG_INFO("ViewController", "runtime_shape_sync", "finished", reason);
  SetCxCrashBreadcrumb("ProcessDeferredRuntimeShapeSync:end");
}

static const char *ShapeHandleLabel(CxShapeHandleRole role, int vertexIndex) {
  switch (role) {
  case CxShapeHandleRole::Center:
    return "C";
  case CxShapeHandleRole::Start:
    return "P0";
  case CxShapeHandleRole::End:
    return "P1";
  case CxShapeHandleRole::Corner0:
    return "K0";
  case CxShapeHandleRole::Corner1:
    return "K1";
  case CxShapeHandleRole::Corner2:
    return "K2";
  case CxShapeHandleRole::Corner3:
    return "K3";
  case CxShapeHandleRole::Radius:
    return "R";
  case CxShapeHandleRole::InnerRadius:
    return "Rin";
  case CxShapeHandleRole::WidthPositive:
    return "W+";
  case CxShapeHandleRole::WidthNegative:
    return "W-";
  case CxShapeHandleRole::RadiusX:
    return "Rx";
  case CxShapeHandleRole::RadiusY:
    return "Ry";
  case CxShapeHandleRole::Vertex: {
    static char buf[16];
    std::snprintf(buf, sizeof(buf), "V%d", vertexIndex);
    return buf;
  }
  case CxShapeHandleRole::Body:
    return "Body";
  default:
    return "?";
  }
}

static ImU32 ShapeHandleColor(CxShapeHandleRole role,
                              const CxShapeElement &element) {
  if (element.result_element)
    return IM_COL32(255, 180, 64, 255);
  if (element.semantic_role == "prompt_positive")
    return IM_COL32(92, 235, 120, 255);
  if (element.semantic_role == "prompt_negative")
    return IM_COL32(250, 92, 92, 255);
  if (element.semantic_role == "measure_points")
    return IM_COL32(255, 235, 64, 255);
  // A0/A1 are angle handles of the same FindCircle ROI.  Make them blue so
  // they remain distinct from center/radius controls and accepted points.
  if (role == CxShapeHandleRole::Vertex && element.owner_type == "FindCircle")
    return IM_COL32(80, 190, 255, 255);
  if (element.editable && element.selected)
    return IM_COL32(80, 255, 170, 255);
  return IM_COL32(160, 160, 200, 255);
}

static ImU32 ShapeElementStrokeColor(const CxShapeElement &element) {
  if (element.result_element)
    return IM_COL32(255, 180, 64, 255);
  if (element.semantic_role == "prompt_positive")
    return IM_COL32(92, 235, 120, 220);
  if (element.semantic_role == "prompt_negative")
    return IM_COL32(250, 92, 92, 220);
  if (element.semantic_role == "scan_tick")
    return IM_COL32(140, 230, 255, 105);
  if (element.editable)
    return IM_COL32(80, 255, 170, 200);
  return IM_COL32(160, 160, 200, 200);
}

static bool IsFiniteShapePoint(const CxShapePoint &p) {
  return std::isfinite(p.x) && std::isfinite(p.y);
}

static bool IsRenderableShapeGeometry(const CxShapeElement &element) {
  if (!element.shape)
    return false;

  CxShapeGeometrySnapshot snap;
  if (!element.shape->snapshot(snap))
    return false;

  if (!IsFiniteShapePoint(snap.center))
    return false;
  if (!std::isfinite(snap.radius) || !std::isfinite(snap.inner_radius) ||
      !std::isfinite(snap.half_width) || !std::isfinite(snap.radius_x) ||
      !std::isfinite(snap.radius_y) || !std::isfinite(snap.angle))
    return false;

  for (const CxShapePoint &p : snap.points) {
    if (!IsFiniteShapePoint(p))
      return false;
  }

  switch (element.shape->kind()) {
  case CxShapeKind::Circle:
    return snap.radius >= 0.0 && snap.radius < 1000000.0 &&
           snap.inner_radius >= 0.0 && snap.inner_radius < 1000000.0;
  case CxShapeKind::Ellipse:
    return snap.radius_x >= 0.0 && snap.radius_y >= 0.0 &&
           snap.radius_x < 1000000.0 && snap.radius_y < 1000000.0;
  case CxShapeKind::LineGauge:
    return snap.half_width >= 0.0 && snap.half_width < 1000000.0;
  default:
    return true;
  }
}

static bool ShouldReverseFindLineScanDirection(int method) {
  return (method & 1) != 0;
}

void ViewController::DrawShapeElementOnImageView(const CxShapeElement &element,
                                                 ImDrawList *drawList) {
  if (!element.shape || !element.visible)
    return;

  if (element.owner_type == "FindCircle" &&
      element.semantic_role == "scan_tick") {
    // FindCircle result points are generated from the live m_lines scan
    // geometry.  Skip the ShapeElement copy of scan ticks here, otherwise
    // Image View can draw a reconstructed Gauge line that does not pass
    // through the algorithm's accepted point.
    return;
  }
  if (element.owner_type == "FindCircle" &&
      element.semantic_role == "measure_points" &&
      m_manualTest.show_circle_gauge_scan_lines) {
    // In scan-line debug mode, FindCircle points are drawn together with
    // their runtime ScanDiagnostic lines below, matching FindLine's display
    // path.  Avoid mixing in a potentially stale ShapeElement point copy.
    return;
  }
  if (element.owner_type == "FindEllipse" &&
      element.semantic_role == "scan_tick") {
    return;
  }
  if (element.owner_type == "FindEllipse" &&
      element.semantic_role == "measure_points" &&
      (m_manualTest.show_ellipse_gauge_scan_lines ||
       (m_manualTest.metrology_ui.enabled &&
        m_manualTest.metrology_ui.show_scan_profile &&
        (m_manualTest.current_gauge.tool == "FindEllipse" ||
         m_manualTest.current_gauge.has_ellipse_gauge)))) {
    return;
  }

  if (m_annotationImageWidth <= 1.0f || m_annotationImageHeight <= 1.0f ||
      !std::isfinite(m_imageViewZoom) || m_imageViewZoom <= 0.001f ||
      !IsRenderableShapeGeometry(element)) {
    return;
  }

  const float sx = m_imageViewZoom;
  const float sy = m_imageViewZoom;

  auto ImageToScreen = [&](double x, double y) -> ImVec2 {
    return ImVec2(m_annotationImagePosX + (float)x * sx,
                  m_annotationImagePosY + (float)y * sy);
  };

  const ImU32 color = ShapeElementStrokeColor(element);
  const ImU32 selectedColor = IM_COL32(255, 64, 64, 255);
  const float thickness = element.selected ? 3.0f : 2.0f;

  switch (element.shape->kind()) {
  case CxShapeKind::Line:
  case CxShapeKind::LineGauge: {
    CxShapePoint p0, p1;
    if (element.shape->exportLine(p0, p1)) {
      drawList->AddLine(ImageToScreen(p0.x, p0.y), ImageToScreen(p1.x, p1.y),
                        element.selected ? selectedColor : color, thickness);

      if (element.shape->kind() == CxShapeKind::LineGauge) {
        const auto *gauge =
            dynamic_cast<const LineGaugeShape *>(element.shape.get());
        if (gauge != nullptr) {
          const double dx = p1.x - p0.x;
          const double dy = p1.y - p0.y;
          const double length = std::hypot(dx, dy);
          if (length > 1.0) {
            const double nx = -dy / length;
            const double ny = dx / length;
            const double halfWidth = gauge->halfWidth();
            const CxShapePoint corners[4] = {
                {p0.x + nx * halfWidth, p0.y + ny * halfWidth},
                {p1.x + nx * halfWidth, p1.y + ny * halfWidth},
                {p1.x - nx * halfWidth, p1.y - ny * halfWidth},
                {p0.x - nx * halfWidth, p0.y - ny * halfWidth}};
            for (int i = 0; i < 4; ++i) {
              const int next = (i + 1) % 4;
              drawList->AddLine(ImageToScreen(corners[i].x, corners[i].y),
                                ImageToScreen(corners[next].x, corners[next].y),
                                element.selected ? selectedColor : color,
                                thickness);
            }

            if (false && m_manualTest.show_line_gauge_scan_lines) {
              const std::string ownerRef =
                  !element.owner_ref.empty()
                      ? element.owner_ref
                      : m_manualTest.current_gauge.primary_object_name;
              FindLine *lineTool =
                  ownerRef.empty() ? nullptr
                                   : static_cast<FindLine *>(
                                         m_parserDebugBridge.QueryClassObject(
                                             "FindLine", ownerRef));
              if (lineTool != nullptr) {
                const int displayMethod = m_manualTest.current_gauge.method;
                const bool reverseArrow =
                    ShouldReverseFindLineScanDirection(displayMethod);
                const int scanCountW = lineTool->getscanlinecount(0);
                const int scanCountH = lineTool->getscanlinecount(1);
                const int totalScanCount = scanCountW + scanCountH;
                const int diagnosticCount = lineTool->getscandiagnosticcount();
                const int arrowSourceCount =
                    diagnosticCount > 0 ? diagnosticCount : totalScanCount;
                const int arrowStride = std::max(1, arrowSourceCount / 48);
                const ImU32 arrowColor = reverseArrow
                                             ? IM_COL32(80, 190, 255, 230)
                                             : IM_COL32(255, 160, 48, 230);

                auto drawArrowHead = [&](CxShapePoint from, CxShapePoint to) {
                  if (reverseArrow)
                    std::swap(from, to);

                  const ImVec2 pFrom = ImageToScreen(from.x, from.y);
                  const ImVec2 pTo = ImageToScreen(to.x, to.y);
                  const float vx = pTo.x - pFrom.x;
                  const float vy = pTo.y - pFrom.y;
                  const float vlen = std::sqrt(vx * vx + vy * vy);
                  if (vlen < 1.0f)
                    return;

                  const float ux = vx / vlen;
                  const float uy = vy / vlen;
                  const float arrowLen = 7.0f;
                  const float arrowHalf = 4.0f;
                  const ImVec2 base(pTo.x - ux * arrowLen,
                                    pTo.y - uy * arrowLen);
                  const ImVec2 left(base.x - uy * arrowHalf,
                                    base.y + ux * arrowHalf);
                  const ImVec2 right(base.x + uy * arrowHalf,
                                     base.y - ux * arrowHalf);

                  drawList->AddLine(pTo, left, arrowColor, 1.5f);
                  drawList->AddLine(pTo, right, arrowColor, 1.5f);
                };

                auto drawActualScanLine =
                    [&](const CxShapePoint &a, const CxShapePoint &b,
                        ImU32 scanColor, float scanThickness, int ordinal) {
                      drawList->AddLine(ImageToScreen(a.x, a.y),
                                        ImageToScreen(b.x, b.y), scanColor,
                                        scanThickness);

                      if ((ordinal % arrowStride) == 0)
                        drawArrowHead(a, b);
                    };

                int ordinal = 0;
                if (diagnosticCount <= 0) {
                  for (int scanType = 0; scanType <= 1; ++scanType) {
                    const int scanCount = lineTool->getscanlinecount(scanType);
                    for (int scanIndex = 0; scanIndex < scanCount;
                         ++scanIndex) {
                      CxShapePoint a, b;
                      if (!lineTool->getscanline(scanType, scanIndex, a, b)) {
                        continue;
                      }

                      drawActualScanLine(a, b, IM_COL32(255, 230, 120, 95),
                                         1.0f, ordinal++);
                    }
                  }
                } else {
                  for (int i = 0; i < diagnosticCount; ++i) {
                    FindLineMeasureInputDebug::ScanDiagnostic diag;
                    CxShapePoint a, b;
                    if (!lineTool->getscandiagnostic(i, diag) ||
                        !lineTool->getscandiagnosticline(
                            diag.scan_type, diag.scan_index, a, b)) {
                      continue;
                    }

                    ImU32 actualColor = IM_COL32(255, 230, 120, 85);
                    float actualThickness = 1.0f;
                    if (diag.accepted) {
                      actualColor = IM_COL32(255, 235, 64, 230);
                      actualThickness = 1.5f;
                    } else if (!diag.reject_reason.empty() &&
                               diag.reject_reason != "no_threshold_crossing") {
                      actualColor = IM_COL32(255, 96, 96, 150);
                    }

                    drawActualScanLine(a, b, actualColor, actualThickness,
                                       ordinal++);

                    if (diag.accepted) {
                      const ImVec2 ap =
                          ImageToScreen(diag.accepted_x, diag.accepted_y);
                      drawList->AddCircleFilled(ap, 3.5f,
                                                IM_COL32(255, 235, 64, 255));
                      drawList->AddCircle(ap, 4.5f, IM_COL32(80, 40, 0, 220),
                                          12, 1.0f);
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
    break;
  }
  case CxShapeKind::Rect: {
    std::vector<CxShapePoint> points;
    bool closed;
    element.shape->exportPolyline(points, closed);
    if (points.size() >= 4) {
      for (size_t i = 0; i < 4; ++i) {
        const size_t j = (i + 1) % 4;
        drawList->AddLine(ImageToScreen(points[i].x, points[i].y),
                          ImageToScreen(points[j].x, points[j].y),
                          element.selected ? selectedColor : color, thickness);
      }
    }
    break;
  }
  case CxShapeKind::Circle: {
    CxShapePoint center;
    double radius, inner_radius;
    if (element.shape->exportCircle(center, radius, inner_radius)) {
      const ImVec2 c = ImageToScreen(center.x, center.y);
      const float r = (float)radius * sx;

      const ImU32 circleColor =
          element.owner_type == "FindCircle"
              ? color
              : (element.selected ? selectedColor : color);
      drawList->AddCircle(c, r, circleColor, 32, thickness);

      if (inner_radius > 0) {
        const float ir = (float)inner_radius * sx;
        drawList->AddCircle(c, ir, IM_COL32(255, 180, 64, 200), 32, thickness);
      }

      // Draw the two real annulus-sector boundaries from Rin to Rout.
      // A0/A1 are the Rout endpoints of this same CircleShape.
      const CircleShape *circle =
          dynamic_cast<const CircleShape *>(element.shape.get());
      if (circle != nullptr && circle->hasScanSector() && element.editable) {
        const ImU32 sectorColor = IM_COL32(80, 190, 255, 235);
        for (int endpoint = 0; endpoint < 2; ++endpoint) {
          const CxShapePoint inner = circle->scanSectorInnerPoint(endpoint);
          const CxShapePoint outer = circle->scanSectorBoundaryPoint(endpoint);
          drawList->AddLine(ImageToScreen(inner.x, inner.y),
                            ImageToScreen(outer.x, outer.y), sectorColor,
                            1.75f);
        }
      }
    }
    break;
  }
  case CxShapeKind::Ellipse: {
    CxShapePoint center;
    double radius_x, radius_y, angle;
    if (element.shape->exportEllipse(center, radius_x, radius_y, angle)) {
      std::vector<ImVec2> ellipsePoints;
      ellipsePoints.reserve(96);
      const double angleRad = RADIAN(angle);
      const double ca = std::cos(angleRad);
      const double sa = std::sin(angleRad);
      for (int i = 0; i < 96; ++i) {
        const double t = 2.0 * kCxPi * static_cast<double>(i) / 96.0;
        const double localX = radius_x * std::cos(t);
        const double localY = radius_y * std::sin(t);
        ellipsePoints.push_back(
            ImageToScreen(center.x + localX * ca - localY * sa,
                          center.y + localX * sa + localY * ca));
      }
      if (!ellipsePoints.empty()) {
        drawList->AddPolyline(ellipsePoints.data(),
                              static_cast<int>(ellipsePoints.size()),
                              element.selected ? selectedColor : color,
                              ImDrawFlags_Closed, thickness);
      }
    }
    break;
  }
  case CxShapeKind::Polyline: {
    std::vector<CxShapePoint> points;
    bool closed;
    element.shape->exportPolyline(points, closed);
    if (points.size() >= 2) {
      for (size_t i = 0; i < points.size() - 1; ++i) {
        drawList->AddLine(ImageToScreen(points[i].x, points[i].y),
                          ImageToScreen(points[i + 1].x, points[i + 1].y),
                          element.selected ? selectedColor : color, thickness);
      }
      if (closed && points.size() >= 3) {
        drawList->AddLine(ImageToScreen(points.back().x, points.back().y),
                          ImageToScreen(points[0].x, points[0].y),
                          element.selected ? selectedColor : color, thickness);
      }
    }
    break;
  }
  case CxShapeKind::Points: {
    std::vector<CxShapePoint> points;
    element.shape->exportPoints(points);
    for (const auto &pt : points) {
      const ImVec2 p = ImageToScreen(pt.x, pt.y);
      drawList->AddCircleFilled(p, 4.0f, color);
      drawList->AddCircle(p, 5.0f, IM_COL32(255, 255, 255, 255), 8, 1.5f);
    }
    break;
  }
  }

  if (element.editable) {
    std::vector<CxShapeHandle> handles;
    element.shape->enumerateHandles(handles);

    for (const CxShapeHandle &h : handles) {
      if (h.role == CxShapeHandleRole::Body)
        continue;
      if (!IsFiniteShapePoint(h.p))
        continue;

      ImVec2 p = ImageToScreen(h.p.x, h.p.y);
      const char *label = h.label.empty()
                              ? ShapeHandleLabel(h.role, h.vertex_index)
                              : h.label.c_str();
      ImU32 fill = ShapeHandleColor(h.role, element);

      drawList->AddCircleFilled(p, 8.0f, IM_COL32(0, 0, 0, 220));
      drawList->AddCircleFilled(p, 6.0f, fill);
      drawList->AddCircle(p, 8.0f, IM_COL32(255, 255, 255, 255), 20, 1.5f);
      drawList->AddText(ImVec2(p.x + 8.0f, p.y - 12.0f),
                        IM_COL32(255, 255, 255, 255), label);
    }
  }
}
