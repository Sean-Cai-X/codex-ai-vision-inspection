#include "pch.h"
#include "ManualConsoleScriptDebugPanel.h"
#include "ManualConsoleCxScriptDebug.h"
#include "ManualConsoleGauge.h"
#include "ManualConsoleUtils.h"
#include "ManualStateTestConsole.h"
#include "CxCrashLogHandler.h"
#include "CxScriptCasePackageWriter.h"
#include "CxUnifiedLog.h"

#include <cctype>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <cstring>
#include <unordered_set>

namespace
{
bool FastMatchScriptSupportsNormalTrace(const std::string& scriptText)
{
  return scriptText.find("fastmatch_normal_trace_binding_version: 3") !=
             std::string::npos &&
         scriptText.find("setnormaltraceenabled") != std::string::npos &&
         scriptText.find("setnormaltraceparams") != std::string::npos &&
         scriptText.find("setnormaltracedomain") != std::string::npos &&
         scriptText.find("setnormaltracejunction") != std::string::npos &&
         scriptText.find("setnormaltraceknn") != std::string::npos &&
         scriptText.find("setnormaltraceann") != std::string::npos;
}
std::string NewFastMatchBindingUpgradeCandidateId()
{
  const auto now = std::chrono::system_clock::now();
  const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
      now.time_since_epoch()).count();
  return "fastmatch_script_binding_upgrade_" + std::to_string(milliseconds);
}

bool CreateNewFastMatchBindingUpgradeRun(ManualTestContext& context)
{
  const std::filesystem::path scriptPath = ResolveWorkspaceFile(
      "cxparser/cxscript/module/cximage/fastmatch_direct_test.cxsc");
  std::string scriptText;
  if (scriptPath.empty() || !std::filesystem::exists(scriptPath) ||
      !ReadTextFile(scriptPath.string(), scriptText) || scriptText.empty())
  {
    context.debug_action = "Create New FastMatch CxScript Run";
    context.debug_status = "FASTMATCH_SCRIPT_BINDING_MISSING";
    context.debug_reason = "current FastMatch CxScript is missing or unreadable";
    RecordManualOperationTraceEvent(context, "create_fastmatch_script_run",
                                    "failed", context.debug_reason);
    return false;
  }
  if (!FastMatchScriptSupportsNormalTrace(scriptText))
  {
    context.debug_action = "Create New FastMatch CxScript Run";
    context.debug_status = "FASTMATCH_SCRIPT_BINDING_MISSING";
    context.debug_reason = "current FastMatch CxScript does not expose the required Normal-Trace setters";
    RecordManualOperationTraceEvent(context, "create_fastmatch_script_run",
                                    "failed", context.debug_reason);
    return false;
  }

  context.editor_text = scriptText;
  context.loaded_script_path = scriptPath.string();
  context.script_file_path = scriptPath.string();
  context.editor_source = "fastmatch_current_workspace_new_run";
  context.editor_dirty = false;
  context.debug_action = "Create New FastMatch CxScript Run";
  context.debug_status = "FASTMATCH_NEW_RUN_CREATING";
  context.debug_reason =
      "creating a new Evidence run from the current workspace FastMatch CxScript; "
      "the previously selected Evidence snapshot remains immutable";
  RecordManualOperationTraceEvent(context, "create_fastmatch_script_run",
                                  "creating", context.debug_reason);

  CxEvidenceCandidateSaveOptions options;
  options.candidate_id = NewFastMatchBindingUpgradeCandidateId();
  options.mode = "fastmatch_script_binding_upgrade";
  options.request_run = false;
  options.source_evidence_script_path_override = scriptPath.string();
  CxEvidenceCandidateSaveResult result;
  if (!SaveEvidenceCandidatePackage(context, options, result))
  {
    context.debug_action = "Create New FastMatch CxScript Run";
    context.debug_status = "FASTMATCH_NEW_RUN_FAILED";
    context.debug_reason = result.reason;
    return false;
  }

  context.debug_action = "Create New FastMatch CxScript Run";
  context.debug_status = "FASTMATCH_NEW_RUN_READY";
  context.debug_reason =
      "new Evidence run created from current workspace FastMatch CxScript: " +
      result.candidate_dir +
      "; the prior Evidence snapshot, test image, parameters, approval, and history were not modified";
  return true;
}

// A catalog/flow script is a file-backed asset.  When the editor has not been
// changed by the operator, execute the current file content rather than a
// stale copy retained from an earlier catalog load.  This is especially
// important after a CxScript calling-convention migration: otherwise Run can
// recreate geometry using the old method argument order and then appear to
// "undo" the Key Parameter Controls values.
bool RefreshCleanEditorFromSource(ManualTestContext& context,
                                  std::string& reason)
{
  reason.clear();
  if (context.editor_dirty || context.loaded_script_path.empty())
    return true;

  const std::filesystem::path sourcePath =
      ResolveWorkspaceFile(context.loaded_script_path);
  std::string diskText;
  if (!ReadTextFile(sourcePath.string(), diskText))
  {
    reason = "cannot refresh clean Script Editor from " +
             sourcePath.string();
    return false;
  }

  if (diskText != context.editor_text)
  {
    context.editor_text = diskText;
    context.analyzed_text.clear();
    reason = "clean Script Editor refreshed from current file before Run";
  }
  return true;
}

// These snapshots were produced before the public CxScript FindCircle
// convention was normalized to (cx, cy, innerRadius, outerRadius) and
// (cx, cy, px, py), and before scan-sector globals were part of the direct
// script contract.  Preserve the historical file as evidence, but do not run
// it with a different parser convention.  Migration is deliberately narrow:
// rewrite the two known generated statements and, when absent, add the public
// execution-control calls immediately before measure().
bool MigrateLegacyFindCircleCallsForRun(ManualTestContext& context,
                                        std::string& reason)
{
  reason.clear();
  if (context.editor_text.find("FindCircle") == std::string::npos)
    return false;

  struct Replacement
  {
    const char* legacy;
    const char* canonical;
  };
  static const Replacement replacements[] = {
      {"m_circle.setannulus(outer_radius, inner_radius, cy, cx);",
       "m_circle.setannulus(cx, cy, inner_radius, outer_radius);"},
      {"m_circle.setcircle(py, px, cy, cx);",
       "m_circle.setcircle(cx, cy, px, py);"},
  };

  bool changed = false;
  bool callOrderChanged = false;
  bool scanArcAdded = false;
  bool selectedEdgeAdded = false;
  bool pointConsistencyAdded = false;
  bool objectPrefilterAdded = false;
  bool minEdgeRunWidthAdded = false;
  for (const Replacement& replacement : replacements)
  {
    std::size_t pos = 0;
    while ((pos = context.editor_text.find(replacement.legacy, pos)) !=
           std::string::npos)
    {
      context.editor_text.replace(
          pos,
          std::strlen(replacement.legacy),
          replacement.canonical);
      pos += std::strlen(replacement.canonical);
      changed = true;
      callOrderChanged = true;
    }
  }

  // Historical Evidence snapshots may not know about the selected candidate
  // edge or scan-sector inputs.  Running such a snapshot otherwise leaves
  // FindCircle::m_iselectedgenum at its default zero (All edges), even though
  // Key Parameter Controls and runtime globals contain Edge 1/2/3/....
  // Resolve the declared object once and add only the missing public calls.
  const std::size_t typePos = context.editor_text.find("FindCircle");
  std::size_t nameBegin = typePos == std::string::npos
      ? std::string::npos
      : typePos + std::strlen("FindCircle");
  if (nameBegin != std::string::npos)
  {
    while (nameBegin < context.editor_text.size() &&
           std::isspace(static_cast<unsigned char>(context.editor_text[nameBegin])))
      ++nameBegin;
  }

  std::size_t nameEnd = nameBegin;
  if (nameEnd != std::string::npos)
  {
    while (nameEnd < context.editor_text.size())
    {
      const unsigned char ch =
          static_cast<unsigned char>(context.editor_text[nameEnd]);
      if (!std::isalnum(ch) && ch != '_')
        break;
      ++nameEnd;
    }
  }

  const std::string objectName =
      nameBegin != std::string::npos && nameEnd > nameBegin
          ? context.editor_text.substr(nameBegin, nameEnd - nameBegin)
          : std::string();
  const std::string measureCall = objectName.empty()
      ? std::string()
      : objectName + ".measure(";

  if (!objectName.empty() &&
      context.editor_text.find(objectName + ".setfindsetting(") ==
          std::string::npos)
  {
    const std::size_t measurePos = context.editor_text.find(measureCall);
    if (measurePos != std::string::npos)
    {
      const std::string findSettingCall =
          objectName +
          ".setfindsetting(global_findcircle_findsetting);\n";
      context.editor_text.insert(measurePos, findSettingCall);
      changed = true;
      objectPrefilterAdded = true;
    }
  }

  if (!objectName.empty() &&
      context.editor_text.find(".setselectedgenum(") == std::string::npos)
  {
    const std::size_t measurePos = context.editor_text.find(measureCall);
    if (measurePos != std::string::npos)
    {
      const std::string selectedEdgeCall =
          objectName +
          ".setselectedgenum(global_findcircle_selected_edge);\n";
      context.editor_text.insert(measurePos, selectedEdgeCall);
      changed = true;
      selectedEdgeAdded = true;
    }
  }

  // Historical Evidence snapshots do not know about the scan-sector input.
  // Running such a snapshot creates a fresh full-circle runtime object, which
  // then replaces the correctly edited sector in Image View.  Resolve the
  // declared FindCircle object name and make the missing public method call
  // explicit in the in-memory working revision.  Do not modify the immutable
  // evidence snapshot on disk.
  if (context.editor_text.find(".setscanarc(") == std::string::npos)
  {
    if (!objectName.empty())
    {
      const std::size_t measurePos = context.editor_text.find(measureCall);
      if (measurePos != std::string::npos)
      {
        const std::string scanArcCall =
            objectName +
            ".setscanarc(global_findcircle_arc_start_deg, "
            "global_findcircle_arc_end_deg, "
            "global_findcircle_arc_enabled);\n\n";
        context.editor_text.insert(measurePos, scanArcCall);
        changed = true;
        scanArcAdded = true;
      }
    }
  }

  if (!objectName.empty() &&
      context.editor_text.find(objectName + ".setpointconsistency(") ==
          std::string::npos)
  {
    const std::size_t measurePos = context.editor_text.find(measureCall);
    if (measurePos != std::string::npos)
    {
      const std::string consistencyCall =
          objectName +
          ".setpointconsistency("
          "global_findcircle_point_consistency_enabled, "
          "global_findcircle_point_consistency_range);\n";
      context.editor_text.insert(measurePos, consistencyCall);
      changed = true;
      pointConsistencyAdded = true;
    }
  }

  if (!objectName.empty() &&
      context.editor_text.find(objectName + ".setminedgerunwidth(") ==
          std::string::npos)
  {
    const std::size_t measurePos = context.editor_text.find(measureCall);
    if (measurePos != std::string::npos)
    {
      const std::string minEdgeRunWidthCall =
          objectName +
          ".setminedgerunwidth(global_min_edge_run_width_px);\n";
      context.editor_text.insert(measurePos, minEdgeRunWidthCall);
      changed = true;
      minEdgeRunWidthAdded = true;
    }
  }

  if (!changed)
    return false;

  // This text now differs from the immutable evidence snapshot.  It must not
  // be silently treated as a saved/verified script.  Save As Candidate creates
  // a new evidence package if the operator wants to retain it.
  context.editor_dirty = true;
  context.analyzed_text.clear();
  reason = "legacy FindCircle working revision migrated in memory (call_order=" +
           std::string(callOrderChanged ? "yes" : "no") +
           ", selected_edge_call=" +
           std::string(selectedEdgeAdded ? "added" : "present") +
           ", scan_arc_call=" + std::string(scanArcAdded ? "added" : "present") +
           ", point_consistency_call=" +
           std::string(pointConsistencyAdded ? "added" : "present") +
           ", object_prefilter_call=" +
           std::string(objectPrefilterAdded ? "added" : "present") +
           ", min_edge_run_width_call=" +
           std::string(minEdgeRunWidthAdded ? "added" : "present") +
           "); historical script_snapshot.cxsc was not modified";
  return true;
}

bool MigrateLegacyFindLineCallsForRun(ManualTestContext& context,
                                      std::string& reason)
{
  reason.clear();

  const std::size_t typePosUpper = context.editor_text.find("FindLine");
  const std::size_t typePosLower = context.editor_text.find("Findline");
  if (typePosUpper == std::string::npos && typePosLower == std::string::npos)
    return false;

  const bool useUpper =
      typePosUpper != std::string::npos &&
      (typePosLower == std::string::npos || typePosUpper < typePosLower);
  const std::size_t typePos = useUpper ? typePosUpper : typePosLower;
  const char* typeName = useUpper ? "FindLine" : "Findline";
  const std::size_t typeNameLen = std::strlen(typeName);

  std::size_t nameBegin = typePos + typeNameLen;
  while (nameBegin < context.editor_text.size() &&
         std::isspace(static_cast<unsigned char>(context.editor_text[nameBegin])))
  {
    ++nameBegin;
  }

  std::size_t nameEnd = nameBegin;
  while (nameEnd < context.editor_text.size())
  {
    const unsigned char ch =
        static_cast<unsigned char>(context.editor_text[nameEnd]);
    if (!std::isalnum(ch) && ch != '_')
      break;
    ++nameEnd;
  }

  const std::string objectName =
      nameEnd > nameBegin
          ? context.editor_text.substr(nameBegin, nameEnd - nameBegin)
          : std::string();
  if (objectName.empty())
    return false;

  const std::string measureCall = objectName + ".measure(";
  const std::string robustMeasureCall = objectName + ".measureRobust(";
  std::size_t insertPos = context.editor_text.find(measureCall);
  if (insertPos == std::string::npos)
    insertPos = context.editor_text.find(robustMeasureCall);
  if (insertPos == std::string::npos)
    return false;

  bool changed = false;
  bool scanDirectionAdded = false;
  bool selectedEdgeAdded = false;
  bool pointConsistencyAdded = false;
  bool objectPrefilterAdded = false;
  bool minEdgeRunWidthAdded = false;
  bool filterProfileBoundToGlobal = false;
  bool minEdgeRunWidthBoundToGlobal = false;

  const auto insertBeforeMeasure = [&](const std::string& statement)
  {
    context.editor_text.insert(insertPos, statement);
    insertPos += statement.size();
    changed = true;
  };

  const auto trimArgument = [](std::string value) {
    while (!value.empty() &&
           std::isspace(static_cast<unsigned char>(value.front())))
      value.erase(value.begin());
    while (!value.empty() &&
           std::isspace(static_cast<unsigned char>(value.back())))
      value.pop_back();
    return value;
  };

  const auto replaceSetterArgumentWithGlobal =
      [&](const char* setterName, const char* globalName,
          const char* localName) {
        const std::string callPrefix =
            objectName + "." + std::string(setterName) + "(";
        bool replaced = false;
        std::size_t searchPos = 0;
        while ((searchPos = context.editor_text.find(callPrefix, searchPos)) !=
               std::string::npos)
        {
          const std::size_t argBegin = searchPos + callPrefix.size();
          const std::size_t closePos = context.editor_text.find(')', argBegin);
          if (closePos == std::string::npos)
            break;

          const std::string argument =
              trimArgument(context.editor_text.substr(argBegin,
                                                      closePos - argBegin));
          if (argument == globalName || argument == localName)
          {
            searchPos = closePos + 1;
            continue;
          }

          std::size_t statementEnd = closePos + 1;
          while (statementEnd < context.editor_text.size() &&
                 context.editor_text[statementEnd] != ';' &&
                 context.editor_text[statementEnd] != '\n' &&
                 context.editor_text[statementEnd] != '\r')
          {
            ++statementEnd;
          }
          if (statementEnd < context.editor_text.size() &&
              context.editor_text[statementEnd] == ';')
          {
            ++statementEnd;
          }

          const std::string canonicalStatement =
              objectName + "." + std::string(setterName) + "(" +
              std::string(globalName) + ");";
          const std::size_t oldLength = statementEnd - searchPos;
          context.editor_text.replace(searchPos, oldLength,
                                      canonicalStatement);
          if (searchPos < insertPos)
          {
            const std::ptrdiff_t delta =
                static_cast<std::ptrdiff_t>(canonicalStatement.size()) -
                static_cast<std::ptrdiff_t>(oldLength);
            insertPos = static_cast<std::size_t>(
                static_cast<std::ptrdiff_t>(insertPos) + delta);
          }
          searchPos += canonicalStatement.size();
          changed = true;
          replaced = true;
        }
        return replaced;
      };

  filterProfileBoundToGlobal = replaceSetterArgumentWithGlobal(
      "setfilterprofile", "global_filterprofile", "filterprofile");
  minEdgeRunWidthBoundToGlobal = replaceSetterArgumentWithGlobal(
      "setminedgerunwidth", "global_min_edge_run_width_px",
      "min_edge_run_width_px");

  if (context.editor_text.find(objectName + ".setscandirection(") ==
      std::string::npos)
  {
    insertBeforeMeasure(objectName +
                        ".setscandirection(global_findline_scan_direction);\n");
    scanDirectionAdded = true;
  }

  if (context.editor_text.find(objectName + ".setselectedgenum(") ==
      std::string::npos)
  {
    insertBeforeMeasure(objectName +
                        ".setselectedgenum(global_findline_selected_edge);\n");
    selectedEdgeAdded = true;
  }

  if (context.editor_text.find(objectName + ".setobjfilter(") ==
      std::string::npos)
  {
    insertBeforeMeasure(objectName +
                        ".setobjfilter(global_findline_objfilter);\n");
    objectPrefilterAdded = true;
  }

  if (context.editor_text.find(objectName + ".setpointconsistency(") ==
      std::string::npos)
  {
    insertBeforeMeasure(
        objectName +
        ".setpointconsistency(global_findline_point_consistency_enabled, global_findline_point_consistency_range);\n");
    pointConsistencyAdded = true;
  }

  if (context.editor_text.find(objectName + ".setminedgerunwidth(") ==
      std::string::npos)
  {
    insertBeforeMeasure(objectName +
                        ".setminedgerunwidth(global_min_edge_run_width_px);\n");
    minEdgeRunWidthAdded = true;
  }

  if (!changed)
    return false;

  context.editor_dirty = true;
  context.analyzed_text.clear();
  reason = "legacy FindLine working revision migrated in memory "
           "(scan_direction_call=" +
           std::string(scanDirectionAdded ? "added" : "present") +
           ", selected_edge_call=" +
           std::string(selectedEdgeAdded ? "added" : "present") +
           ", point_consistency_call=" +
           std::string(pointConsistencyAdded ? "added" : "present") +
            ", object_prefilter_call=" +
            std::string(objectPrefilterAdded ? "added" : "present") +
            ", filterprofile_binding=" +
            std::string(filterProfileBoundToGlobal ? "globalized" :
                                                   "present") +
            ", min_edge_run_width_binding=" +
            std::string(minEdgeRunWidthAdded
                            ? "added"
                            : (minEdgeRunWidthBoundToGlobal ? "globalized"
                                                            : "present")) +
            "); historical script_snapshot.cxsc was not modified";
  return true;
}

void ReplaceAllScriptTextLocal(
    std::string& text,
    const std::string& from,
    const std::string& to)
{
  if (from.empty())
    return;
  std::size_t pos = 0;
  while ((pos = text.find(from, pos)) != std::string::npos)
  {
    text.replace(pos, from.size(), to);
    pos += to.size();
  }
}

bool MigrateLegacyFindSegmentationPromptCallsForRun(
    ManualTestContext& context,
    std::string& reason)
{
  reason.clear();
  if (context.editor_text.find("FindSegmentation") == std::string::npos)
    return false;

  // Fixed-int member calls travel through Param_int_2. The parser restores
  // source order before class dispatch, and classfunc::UseFUNC reverses the
  // fixed argument vector when invoking the registered C++ member. The
  // FindSegmentation *xy wrappers intentionally accept (y, x) and restore
  // (x, y) internally, so CxScript must remain human-readable (x, y).
  const std::string reversedPositive =
      "m_seg.setpositivepointxy(global_segmentation_positive_y, global_segmentation_positive_x);";
  const std::string canonicalPositive =
      "m_seg.setpositivepointxy(global_segmentation_positive_x, global_segmentation_positive_y);";
  const std::string reversedNegative =
      "m_seg.setnegativepointxy(global_segmentation_negative_y, global_segmentation_negative_x);";
  const std::string canonicalNegative =
      "m_seg.setnegativepointxy(global_segmentation_negative_x, global_segmentation_negative_y);";

  const bool hasReversedPositive =
      context.editor_text.find(reversedPositive) != std::string::npos;
  const bool hasReversedNegative =
      context.editor_text.find(reversedNegative) != std::string::npos;
  if (!hasReversedPositive && !hasReversedNegative)
    return false;

  ReplaceAllScriptTextLocal(
      context.editor_text, reversedPositive, canonicalPositive);
  ReplaceAllScriptTextLocal(
      context.editor_text, reversedNegative, canonicalNegative);
  context.editor_dirty = true;
  context.analyzed_text.clear();
  reason =
      "FindSegmentation prompt point calls corrected in memory to set*pointxy(x,y) after Param_int_2 dispatch verification; historical script_snapshot.cxsc was not modified";
  return true;
}
} // namespace

bool PrepareCurrentFastMatchScriptRun(ManualTestContext& context)
{
  return CreateNewFastMatchBindingUpgradeRun(context);
}

bool ViewController::ConsumePendingManualScriptRun(ManualTestContext& context,
                                                  const char* trigger)
{
  const bool runRequestedByCandidateSave =
      !context.pending_execution_candidate_id.empty() &&
      context.pending_execution_candidate_id ==
          context.last_evidence_candidate_id;
  const bool runRequestedByKeyParameterControls =
      context.debug_action == "Key Parameter Controls Run Script" &&
      context.debug_status == "MANUAL_RUN_REQUESTED";
  const bool runRequestedByEvidenceSelection =
      context.pending_evidence_selection_rehydrate &&
      context.debug_status == "EVIDENCE_SELECTION_REHYDRATE_QUEUED";
  const bool runRequestedByFastMatchAction =
      context.debug_status == "FASTMATCH_RUN_REQUESTED" ||
      context.debug_action == "FastMatch Learn" ||
      context.debug_action == "FastMatch Match" ||
      context.debug_action == "FastMatch Learn + Match";
  const bool runRequestedByTorchAction =
      context.editor_source == "torch_runtime_action" &&
      context.debug_status == "TORCH_ACTION_RUN_REQUESTED";
  const bool usePendingExecutionSnapshot =
      (runRequestedByCandidateSave || runRequestedByKeyParameterControls ||
       runRequestedByEvidenceSelection ||
       runRequestedByFastMatchAction) &&
      context.has_pending_execution_snapshot;
  const bool useStagedTorchSnapshot =
      runRequestedByTorchAction && context.has_pending_execution_snapshot;

  if (!usePendingExecutionSnapshot && !useStagedTorchSnapshot)
    return false;

  SetCxCrashBreadcrumb("ConsumePendingManualScriptRun:begin");
  CXLOG_INFO(
      "ManualConsole",
      "pending_manual_run_consume_begin",
      "running",
      std::string("trigger=") + (trigger == nullptr ? "(none)" : trigger) +
          " debug_status=" + context.debug_status +
          " debug_action=" + context.debug_action +
          " script=" + context.loaded_script_path);

  const ManualGaugeState frozenGauge = context.pending_execution_gauge;
  const std::unordered_map<std::string, int> frozenGlobals =
      context.pending_execution_globals;

  auto clearPendingSnapshot = [&context]() {
    context.has_pending_execution_snapshot = false;
    context.pending_evidence_selection_rehydrate = false;
    context.pending_execution_globals.clear();
    context.pending_execution_candidate_id.clear();
    context.last_evidence_candidate_id.clear();
    context.last_evidence_candidate_dir.clear();
    context.last_evidence_candidate_reason.clear();
  };

  // Historical Evidence snapshots are immutable and may contain an older
  // FastMatch Normal-Trace binding order.  Migration belongs to the execution
  // boundary, not Evidence loading and not a separate prerequisite button.
  // Only an explicit Learn action (1 or 3) may create a compatible candidate;
  // Match (2) must never hide an implicit Learn.
  const auto pendingActionIt =
      frozenGlobals.find("global_fastmatch_action");
  const int pendingFastMatchAction =
      pendingActionIt == frozenGlobals.end() ? 0 : pendingActionIt->second;
  const auto pendingNormalTraceIt =
      frozenGlobals.find("global_fastmatch_normaltrace_enabled");
  const bool pendingNormalTraceEnabled =
      pendingNormalTraceIt != frozenGlobals.end() &&
      pendingNormalTraceIt->second != 0;
  const bool explicitLearnRequest =
      (runRequestedByFastMatchAction || runRequestedByKeyParameterControls ||
       runRequestedByCandidateSave) &&
      (pendingFastMatchAction == 1 || pendingFastMatchAction == 3);
  const bool staleFastMatchBinding =
      context.editor_text.find("FastMatch") != std::string::npos &&
      !FastMatchScriptSupportsNormalTrace(context.editor_text);
  if (explicitLearnRequest && pendingNormalTraceEnabled &&
      staleFastMatchBinding)
  {
    if (context.editor_dirty)
    {
      context.run_state = "failed";
      context.debug_status = "FASTMATCH_COMPATIBILITY_EDIT_CONFLICT";
      context.debug_reason =
          "the loaded historical script has unsaved editor changes; save or "
          "discard them before automatic compatibility migration";
      clearPendingSnapshot();
      CXLOG_ERROR(
          "ManualConsole",
          "fastmatch_compatibility_auto_migration",
          "edit_conflict",
          context.debug_reason);
      return true;
    }
    const std::string requestedAction = context.debug_action;
    if (!CreateNewFastMatchBindingUpgradeRun(context))
    {
      context.run_state = "failed";
      clearPendingSnapshot();
      CXLOG_ERROR(
          "ManualConsole",
          "fastmatch_compatibility_auto_migration",
          "failed",
          context.debug_reason);
      return true;
    }
    context.debug_action = requestedAction;
    context.debug_status = "FASTMATCH_RUN_REQUESTED";
    context.debug_reason +=
        "; compatibility=auto_migrated_before_learn";
    RecordManualOperationTraceEvent(
        context,
        "fastmatch_compatibility_auto_migration",
        "ready",
        "current workspace CxScript staged for explicit Learn; original "
        "Evidence and source image remain immutable");
    CXLOG_INFO(
        "ManualConsole",
        "fastmatch_compatibility_auto_migration",
        "ready",
        "action=" + std::to_string(pendingFastMatchAction) +
            " script=" + context.loaded_script_path);
  }

  if (context.editor_text.empty())
  {
    context.debug_action = "Run";
    context.run_state = "failed";
    context.debug_status = "run_failed";
    context.debug_reason = "Script Editor is empty";
    clearPendingSnapshot();
    CXLOG_ERROR(
        "ManualConsole",
        "pending_manual_run_consume_end",
        "run_failed",
        "reason=Script Editor is empty");
    return true;
  }

  std::string sourceRefreshReason;
  if (!RefreshCleanEditorFromSource(context, sourceRefreshReason))
  {
    context.debug_action = "Run";
    context.run_state = "failed";
    context.debug_status = "run_failed";
    context.debug_reason = sourceRefreshReason;
    clearPendingSnapshot();
    CXLOG_ERROR(
        "ManualConsole",
        "pending_manual_run_consume_end",
        "run_failed",
        "reason=" + sourceRefreshReason);
    return true;
  }

  std::string legacyMigrationReason;
  const bool legacyFindCircleMigrated =
      MigrateLegacyFindCircleCallsForRun(context, legacyMigrationReason);
  if (legacyFindCircleMigrated)
  {
    CXLOG_WARN(
        "ManualConsole",
        "legacy_findcircle_script_migrated",
        "MIGRATED_IN_MEMORY",
        std::string("script=") + context.loaded_script_path +
            ", reason=" + legacyMigrationReason);
  }

  std::string legacyFindLineMigrationReason;
  const bool legacyFindLineMigrated =
      MigrateLegacyFindLineCallsForRun(context, legacyFindLineMigrationReason);
  if (legacyFindLineMigrated)
  {
    CXLOG_WARN(
        "ManualConsole",
        "legacy_findline_script_migrated",
        "MIGRATED_IN_MEMORY",
        std::string("script=") + context.loaded_script_path +
            ", reason=" + legacyFindLineMigrationReason);
  }

  std::string legacyFindSegmentationMigrationReason;
  const bool legacyFindSegmentationMigrated =
      MigrateLegacyFindSegmentationPromptCallsForRun(
          context, legacyFindSegmentationMigrationReason);
  if (legacyFindSegmentationMigrated)
  {
    CXLOG_WARN(
        "ManualConsole",
        "legacy_findsegmentation_prompt_migrated_for_run",
        "migrated",
        legacyFindSegmentationMigrationReason);
  }

  AnalyzeScript(context);
  context.current_gauge = frozenGauge;
  context.runtime_int_vars = frozenGlobals;
  context.debug_action = "Run";
  if (useStagedTorchSnapshot)
  {
    context.debug_reason = "Torch action input snapshot restored before execution.";
  }
  else if (runRequestedByKeyParameterControls)
  {
    context.debug_reason =
        "Key Parameter Controls input snapshot restored before execution.";
  }
  else if (runRequestedByFastMatchAction)
  {
    context.debug_reason =
        "FastMatch Learn/Match input snapshot restored before execution.";
  }
  else
  {
    context.debug_reason =
        "candidate input snapshot restored before execution: candidate_id=" +
        context.pending_execution_candidate_id;
  }

  for (const auto& input : context.runtime_int_vars)
  {
    if (input.first.rfind("global_", 0) == 0)
      m_parserDebugBridge.SetGlobalInt(input.first, input.second);
  }
  for (const ScriptVariableView& observed : context.global_variable_views)
  {
    if (observed.name == "global_matInput" ||
        observed.name.rfind("global_", 0) != 0)
      continue;
    if (context.runtime_int_vars.find(observed.name) ==
        context.runtime_int_vars.end())
    {
      context.runtime_int_vars.emplace(observed.name, 0);
      m_parserDebugBridge.SetGlobalInt(observed.name, 0);
    }
  }

  std::stringstream ss;
  ss << "\nManual effective globals:";
  ss << "\nimage_path="
     << (context.image_file_path.empty() ? "(untracked)"
                                         : context.image_file_path);
  ss << "\nimage=" << (m_imageViewImage.empty() ? "s_img0" : "m_imageViewImage");
  if (!m_imageViewImage.empty())
    ss << " size=" << m_imageViewImage.cols << "x" << m_imageViewImage.rows;
  else if (!s_img0.empty())
    ss << " size=" << s_img0.cols << "x" << s_img0.rows;
  ss << "\nscript=" << context.loaded_script_path;
  ss << "\neditor_source=" << context.editor_source;
  ss << "\nlearn_roi=("
     << context.runtime_int_vars["global_learn_roi_x"] << ","
     << context.runtime_int_vars["global_learn_roi_y"] << ","
     << context.runtime_int_vars["global_learn_roi_w"] << ","
     << context.runtime_int_vars["global_learn_roi_h"] << ")";
  ss << "\nsearch_roi=("
     << context.runtime_int_vars["global_search_roi_x"] << ","
     << context.runtime_int_vars["global_search_roi_y"] << ","
     << context.runtime_int_vars["global_search_roi_w"] << ","
     << context.runtime_int_vars["global_search_roi_h"] << ")";
  ss << "\nfastmatch_action="
     << context.runtime_int_vars["global_fastmatch_action"];
  ss << "\nroi=(" << context.runtime_int_vars["global_roi_x0"] << ","
     << context.runtime_int_vars["global_roi_y0"] << ","
     << context.runtime_int_vars["global_roi_x1"] << ","
     << context.runtime_int_vars["global_roi_y1"] << ")";
  ss << "\nsegmentation_mode="
     << context.runtime_int_vars["global_segmentation_mode"];
  ss << "\nsegmentation_threshold_percent="
     << context.runtime_int_vars["global_segmentation_threshold_percent"];
  ss << "\nsegmentation_positive=(enabled="
     << context.runtime_int_vars["global_segmentation_positive_enabled"]
     << ", x=" << context.runtime_int_vars["global_segmentation_positive_x"]
     << ", y=" << context.runtime_int_vars["global_segmentation_positive_y"]
     << ")";
  ss << "\nsegmentation_negative=(enabled="
     << context.runtime_int_vars["global_segmentation_negative_enabled"]
     << ", x=" << context.runtime_int_vars["global_segmentation_negative_x"]
     << ", y=" << context.runtime_int_vars["global_segmentation_negative_y"]
     << ")";
  ss << "\ngap=" << context.runtime_int_vars["global_gap"];
  ss << "\nlinegap=" << context.runtime_int_vars["global_linegap"];
  ss << "\nthreshold=" << context.runtime_int_vars["global_threshold"];
  ss << "\nmethod=" << context.runtime_int_vars["global_method"];
  const std::string effectiveGlobals = ss.str();

  CXLOG_INFO(
      "ManualConsole",
      "manual_run_input_snapshot",
      "READY",
      std::string("source=") +
          (sourceRefreshReason.empty() ? "pending_consumer" : "disk_refresh") +
          effectiveGlobals);

  if (legacyFindCircleMigrated)
    context.debug_reason = legacyMigrationReason;
  if (legacyFindLineMigrated)
    context.debug_reason = legacyFindLineMigrationReason;
  if (legacyFindSegmentationMigrated)
    context.debug_reason = legacyFindSegmentationMigrationReason;

  bool imageBound = true;
  if (!m_imageViewImage.empty())
    imageBound = m_parserDebugBridge.StageGlobalMatInput(m_imageViewImage);
  else if (!s_img0.empty())
    imageBound = m_parserDebugBridge.StageGlobalMatInput(s_img0);

  std::string torchRequestReason;
  const bool torchRequestReady = PrepareTorchUiRequestContext(
      context.editor_text,
      context.loaded_script_path,
      torchRequestReason);
  if (!torchRequestReady)
  {
    CXLOG_ERROR(
        "TorchUI",
        "torch_ui_request_blocked",
        "BLOCKED",
        torchRequestReason);
  }

  if (torchRequestReady)
    m_annotationLayer.RemoveRuntimeOwnersNotIn(std::unordered_set<std::string>{});

  if (context.current_gauge.tool == "FindSegmentation" ||
      context.current_gauge.primary_object_type == "FindSegmentation")
  {
    std::string promptSyncReason;
    SyncFindSegmentationPromptListsFromShapeElements(promptSyncReason);
  }

  context.run_state = "running";
  const bool ran = imageBound && torchRequestReady &&
      m_parserDebugBridge.RunScript(context.editor_text);
  context.run_state = ran ? "runtime_finished" : "failed";
  context.debug_status = ran ? "runtime_executed" : "run_failed";
  context.debug_reason = ran
      ? ("exact Script Editor text executed through ParserDebugBridge" +
         (torchRequestReason.empty()
              ? std::string()
              : " | " + torchRequestReason))
      : (!torchRequestReady
             ? torchRequestReason
             : (imageBound
                    ? ("ParserDebugBridge rejected the Script Editor text: " +
                       m_parserDebugBridge.LastError())
                    : "ParserDebugBridge rejected Run: no Image View/default image available for global_matInput"));

  if (ran)
  {
    CXLOG_INFO(
        "ManualConsole",
        "manual_run_finished",
        "runtime_executed",
        "script=" + context.loaded_script_path +
            " reason=" + context.debug_reason);
  }
  else
  {
    CXLOG_ERROR(
        "ManualConsole",
        "manual_run_finished",
        "run_failed",
        "script=" + context.loaded_script_path +
            " reason=" + context.debug_reason);
  }

  m_scriptResult.source = "manual_console_editor";
  m_scriptResult.script_path = context.loaded_script_path;
  m_scriptResult.status = ran ? "PENDING" : "BLOCKED";
  m_scriptResult.reason = context.debug_reason;
  m_scriptResult.runtime_fillback_status =
      ran ? "runtime_objects_queried" : "not_started";

  RefreshRuntimeObjectTable(
      "Manual Console Pending Run", ran ? "runtime_executed" : "BLOCKED");
  std::string manualConclusionStatus = m_manualTest.debug_status;
  if (ran &&
      m_manualTest.current_result_ref.result_type == "FindSegmentationResult" &&
      !m_manualTest.current_result_ref.status.empty())
  {
    manualConclusionStatus = m_manualTest.current_result_ref.status;
  }
  CXLOG_INFO(
      "ManualConsole",
      "manual_run_conclusion",
      manualConclusionStatus,
      "script=" + context.loaded_script_path +
          " debug_status=" + m_manualTest.debug_status +
          " script_result_status=" + m_scriptResult.status +
          " result_type=" + m_manualTest.current_result_ref.result_type +
          " result_status=" + m_manualTest.current_result_ref.status +
          " reason=" + m_manualTest.debug_reason);
  // RefreshRuntimeObjectTable() projects the newly-created runtime object and
  // may replace current_gauge.  A deferred Key Parameter/Candidate run must
  // keep the exact input snapshot that was applied and persisted, not the
  // runtime projection.  In particular this prevents a legacy FindEllipse
  // runtime object from exchanging bbox X/Y extents before the next save.
  if (usePendingExecutionSnapshot || useStagedTorchSnapshot)
  {
    context.current_gauge = frozenGauge;
    context.runtime_int_vars = frozenGlobals;
  }

  std::string snapshotPath;
  std::string snapshotReason;
  if (!SaveCxDebugSnapshotText(context, snapshotPath, snapshotReason))
  {
    context.debug_reason += " | debug snapshot save failed: " + snapshotReason;
  }

  if (runRequestedByCandidateSave &&
      !context.last_evidence_candidate_id.empty() &&
      !context.last_evidence_candidate_dir.empty())
  {
    std::filesystem::path candidateDir(context.last_evidence_candidate_dir);
    CxEvidenceCandidateSaveOptions options;
    if (candidateDir.has_parent_path() &&
        candidateDir.parent_path().has_parent_path())
    {
      options.root_dir = candidateDir.parent_path().parent_path().string();
    }
    options.candidate_id = context.last_evidence_candidate_id;
    options.case_id_override =
        context.current_evidence_selection.valid &&
                !context.current_evidence_selection.case_id.empty()
            ? context.current_evidence_selection.case_id
            : (candidateDir.has_parent_path()
                   ? candidateDir.parent_path().filename().string()
                   : std::string());
    options.mode = ran ? "runtime_finished" : "runtime_failed";
    options.request_run = false;
    options.add_to_evidence_chain = false;
    options.preserve_input_snapshots = true;
    CxEvidenceCandidateSaveResult candidateResult;
    if (!SaveEvidenceCandidatePackage(context, options, candidateResult))
    {
      context.debug_reason +=
          " | evidence candidate post-run update failed: " +
          candidateResult.reason;
    }
    else
    {
      context.last_evidence_candidate_dir = candidateResult.candidate_dir;
    }
  }

  clearPendingSnapshot();
  CXLOG_INFO(
      "ManualConsole",
      "pending_manual_run_consume_end",
      ran ? "runtime_executed" : "run_failed",
      std::string("trigger=") + (trigger == nullptr ? "(none)" : trigger) +
          " script=" + context.loaded_script_path +
          " reason=" + context.debug_reason);
  SetCxCrashBreadcrumb("ConsumePendingManualScriptRun:end");
  return true;
}

void ViewController::DrawScriptEditorBlock(ManualTestContext& context)
{
  SetCxCrashBreadcrumb("drawManualStateTestConsole:ScriptEditor:begin");
  if (!ImGui::CollapsingHeader("Script Editor", ImGuiTreeNodeFlags_DefaultOpen))
    return;

  ImGui::PushID("script_editor");

  const float toolbarHeight = 30.0f;
  ImGui::BeginChild("script_editor_toolbar", ImVec2(-1, toolbarHeight), false);

  ImGui::PushItemWidth(150.0f);
  int source_index = 0;
  if (context.editor_source == "catalog") source_index = 1;
  else if (context.editor_source == "direct_file") source_index = 2;
  else if (context.editor_source == "semantic_flow") source_index = 3;
  if (ImGui::Combo("##source", &source_index,
                   "manual\0catalog\0direct_file\0semantic_flow\0"))
  {
    if (source_index == 0) context.editor_source = "manual";
    else if (source_index == 1) context.editor_source = "catalog";
    else if (source_index == 2) context.editor_source = "direct_file";
    else if (source_index == 3) context.editor_source = "semantic_flow";
  }
  ImGui::PopItemWidth();

  ImGui::SameLine();
  if (ImGui::Button("Open"))
  {
    context.debug_action = "Open Script";
    context.debug_status = "PENDING";
    context.debug_reason = "Open file dialog placeholder";
  }

  ImGui::SameLine();
  if (ImGui::Button("Save"))
  {
    // There is no direct file write here.  Do not mark an unsaved in-memory
    // edit as clean, otherwise a later Run would legitimately reload the
    // catalog source and silently discard that edit.
    context.debug_action = "Save Script";
    context.debug_status = "EDITOR_SAVE_NOT_PERSISTED";
    context.debug_reason =
        "Script Editor has no direct source-file save; use Save As Candidate "
        "to preserve this edited text.";
  }

  ImGui::SameLine();
  if (ImGui::Button("Save As Candidate"))
  {
    CxEvidenceCandidateSaveOptions options;
    options.mode = "script_editor_draft";
    options.request_run = false;
    CxEvidenceCandidateSaveResult result;
    if (!SaveEvidenceCandidatePackage(context, options, result))
    {
      context.debug_action = "Save Script Candidate";
      context.debug_status = "EVIDENCE_CANDIDATE_SAVE_FAILED";
      context.debug_reason = result.reason;
    }
  }

  ImGui::SameLine();
  if (context.editor_dirty)
    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "*");

  if (!context.loaded_script_path.empty())
    ImGui::TextWrapped("Loaded: %s", context.loaded_script_path.c_str());

  ImGui::EndChild();

  const bool isFastMatchScript =
      context.editor_text.find("FastMatch") != std::string::npos;
  const bool normalTraceBindingMissing =
      isFastMatchScript && !FastMatchScriptSupportsNormalTrace(context.editor_text);
  if (normalTraceBindingMissing &&
      ImGui::CollapsingHeader("Script Compatibility", ImGuiTreeNodeFlags_DefaultOpen))
  {
    ImGui::TextColored(ImVec4(1.0f, 0.43f, 0.35f, 1.0f),
                       "SCRIPT_BINDING_MISSING");
    ImGui::TextWrapped(
        "The selected Evidence script predates the Normal-Trace CxScript API. "
        "This is not a FastMatch algorithm parameter and cannot be fixed in Key Parameter Controls.");
    ImGui::TextDisabled(
        "Learn or Learn + Match will automatically create a NEW compatible "
        "candidate run from the current workspace FastMatch CxScript.");
    ImGui::TextDisabled(
        "Loading stays read-only and does not edit the selected Evidence "
        "snapshot, test image, saved parameters, review state, or history. "
        "Match never performs this migration or an implicit Learn.");
  }

  // Keep the script editor as a bounded edit area.  Using (-1, -1) here makes
  // the multiline editor consume all remaining parent space, so long scripts
  // visually stretch the Manual State Test Console and push the compiler/result
  // blocks away.  Long code should scroll inside this child instead.
  const float editorHeight = ImGui::GetTextLineHeightWithSpacing() * 14.0f;
  ImGui::BeginChild("script_editor_text",
                    ImVec2(-1, editorHeight),
                    true,
                    ImGuiWindowFlags_HorizontalScrollbar);
  if (InputTextMultilineString(
        "##script_text", context.editor_text, ImVec2(-1, -1)))
  {
    context.editor_dirty = true;
    context.analyzed_text.clear();
  }
  ImGui::EndChild();

  ImGui::PopID();
  SetCxCrashBreadcrumb("drawManualStateTestConsole:ScriptEditor:end");
}

void ViewController::DrawScriptDebugCompilerBlock(ManualTestContext& context)
{
  SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:begin");
  // Some parameter-panel actions (FastMatch Learn/Match, Save+Run candidate,
  // Key Parameter Controls Run Script) stage a serial parser execution for the
  // Debug Compiler block to consume on the next UI pass.  That execution must
  // not depend on the operator keeping the Debug Compiler header expanded;
  // otherwise the button only records a request and no runtime objects/model
  // points are produced.
  const bool deferredCandidateSaveRun =
      !context.pending_execution_candidate_id.empty() &&
      context.pending_execution_candidate_id ==
          context.last_evidence_candidate_id &&
      context.has_pending_execution_snapshot;
  const bool deferredKeyParameterRun =
      context.debug_action == "Key Parameter Controls Run Script" &&
      context.debug_status == "MANUAL_RUN_REQUESTED" &&
      context.has_pending_execution_snapshot;
  const bool deferredEvidenceSelectionRun =
      context.pending_evidence_selection_rehydrate &&
      context.debug_status == "EVIDENCE_SELECTION_REHYDRATE_QUEUED" &&
      context.has_pending_execution_snapshot;
  const bool deferredFastMatchRun =
      (context.debug_status == "FASTMATCH_RUN_REQUESTED" ||
       context.debug_action == "FastMatch Learn" ||
       context.debug_action == "FastMatch Match" ||
       context.debug_action == "FastMatch Learn + Match") &&
      context.has_pending_execution_snapshot;
  const bool deferredTorchRun =
      context.editor_source == "torch_runtime_action" &&
      context.debug_status == "TORCH_ACTION_RUN_REQUESTED" &&
      context.has_pending_execution_snapshot;
  const bool hasDeferredRun =
      deferredCandidateSaveRun || deferredKeyParameterRun ||
      deferredEvidenceSelectionRun ||
      deferredFastMatchRun || deferredTorchRun;
  const bool debugCompilerOpen =
      ImGui::CollapsingHeader("Debug Compiler", ImGuiTreeNodeFlags_DefaultOpen);
  if (!debugCompilerOpen && !hasDeferredRun)
    return;

  ImGui::PushID("debug_compiler");
  SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:push_id");

  const float btnWidth = 90.0f;

  SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:header_text");
  ImGui::TextDisabled(
    "Compile: source preflight/line analysis; Run: execute exact editor text via CxParserRuntime::Compile");

  SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:compile_button");
  if (ImGui::Button("Compile", ImVec2(btnWidth, 0)))
  {
    context.debug_action = "Compile";
    if (context.editor_text.empty())
    {
      context.debug_status = "compile_failed";
      context.debug_reason = "Script Editor is empty";
    }
    else
    {
      AnalyzeScript(context);
      context.debug_status = "source_analyzed";
      context.debug_reason =
        "CxScript source analyzed; Run executes the exact editor text";
    }
  }

  ImGui::SameLine();
  SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:run_button");
  const bool runRequestedByCandidateSave =
      !context.pending_execution_candidate_id.empty() &&
      context.pending_execution_candidate_id ==
          context.last_evidence_candidate_id;
  const bool runRequestedByKeyParameterControls =
      context.debug_action == "Key Parameter Controls Run Script" &&
      context.debug_status == "MANUAL_RUN_REQUESTED";
  const bool runRequestedByEvidenceSelection =
      context.pending_evidence_selection_rehydrate &&
      context.debug_status == "EVIDENCE_SELECTION_REHYDRATE_QUEUED";
  const bool runRequestedByFastMatchAction =
      context.debug_status == "FASTMATCH_RUN_REQUESTED" ||
      context.debug_action == "FastMatch Learn" ||
      context.debug_action == "FastMatch Match" ||
      context.debug_action == "FastMatch Learn + Match";
  const bool runRequestedByTorchAction =
      context.editor_source == "torch_runtime_action" &&
      context.debug_status == "TORCH_ACTION_RUN_REQUESTED";
  const bool usePendingExecutionSnapshot =
      (runRequestedByCandidateSave || runRequestedByKeyParameterControls ||
       runRequestedByEvidenceSelection ||
       runRequestedByFastMatchAction) &&
      context.has_pending_execution_snapshot;
  const bool runButtonClicked = ImGui::Button("Run", ImVec2(btnWidth, 0));
  const bool runClickedForStagedTorchAction =
      runButtonClicked &&
      context.editor_source == "torch_runtime_action" &&
      context.debug_status == "TORCH_ACTION_STAGED";
  const bool useStagedTorchSnapshot =
      (runClickedForStagedTorchAction || runRequestedByTorchAction) &&
      context.has_pending_execution_snapshot;

  if (runButtonClicked ||
       runRequestedByCandidateSave ||
       runRequestedByKeyParameterControls ||
       runRequestedByEvidenceSelection ||
       runRequestedByFastMatchAction ||
      runRequestedByTorchAction)
  {
    SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:Run:begin");
    bool ran = false;
    if (context.editor_text.empty())
    {
      context.debug_action = "Run";
      context.run_state = "failed";
      context.debug_status = "run_failed";
      context.debug_reason = "Script Editor is empty";
    }
    else
    {
      std::string sourceRefreshReason;
      if (!RefreshCleanEditorFromSource(context, sourceRefreshReason))
      {
        context.debug_action = "Run";
        context.run_state = "failed";
        context.debug_status = "run_failed";
        context.debug_reason = sourceRefreshReason;
        ImGui::PopID();
        SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:Run:source_refresh_failed");
        return;
      }

      std::string legacyMigrationReason;
      const bool legacyFindCircleMigrated =
          MigrateLegacyFindCircleCallsForRun(context, legacyMigrationReason);
      if (legacyFindCircleMigrated)
      {
        CXLOG_WARN(
            "ManualConsole",
            "legacy_findcircle_script_migrated",
            "MIGRATED_IN_MEMORY",
            std::string("script=") + context.loaded_script_path +
                ", reason=" + legacyMigrationReason);
      }

      std::string legacyFindLineMigrationReason;
      const bool legacyFindLineMigrated =
          MigrateLegacyFindLineCallsForRun(context, legacyFindLineMigrationReason);
      if (legacyFindLineMigrated)
      {
        CXLOG_WARN(
            "ManualConsole",
            "legacy_findline_script_migrated",
            "MIGRATED_IN_MEMORY",
            std::string("script=") + context.loaded_script_path +
                ", reason=" + legacyFindLineMigrationReason);
      }

      std::string legacyFindSegmentationMigrationReason;
      const bool legacyFindSegmentationMigrated =
          MigrateLegacyFindSegmentationPromptCallsForRun(
              context, legacyFindSegmentationMigrationReason);
      if (legacyFindSegmentationMigrated)
      {
        CXLOG_WARN(
            "ManualConsole",
            "legacy_findsegmentation_prompt_migrated_for_run",
            "migrated",
            legacyFindSegmentationMigrationReason);
      }

      // A candidate run must use the values captured by the Save/Run action.
      // Do not let an Evidence refresh or runtime-object refresh replace them
      // between the button click and this deferred compiler pass.
      const ManualGaugeState frozenGauge = context.pending_execution_gauge;
      const std::unordered_map<std::string, int> frozenGlobals =
          context.pending_execution_globals;

      SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:Run:analyze");
      AnalyzeScript(context);
      if (usePendingExecutionSnapshot || useStagedTorchSnapshot)
      {
        context.current_gauge = frozenGauge;
        context.runtime_int_vars = frozenGlobals;
        if (useStagedTorchSnapshot)
        {
          context.debug_reason =
              "Torch action input snapshot restored before execution.";
          CXLOG_INFO(
              "TorchUI",
              "torch_ui_action_execute_begin",
              "serial_parser",
              "action=" + context.active_script_case_purpose +
                  " script=" + context.loaded_script_path);
        }
        else if (runRequestedByKeyParameterControls)
        {
          context.debug_reason =
              "Key Parameter Controls input snapshot restored before execution.";
        }
        else if (runRequestedByFastMatchAction)
        {
          context.debug_reason =
              "FastMatch Learn/Match input snapshot restored before execution.";
        }
        else
        {
          context.debug_reason =
              "candidate input snapshot restored before execution: candidate_id=" +
              context.pending_execution_candidate_id;
        }
      }
      else if (context.current_gauge.has_line_gauge ||
               context.current_gauge.has_circle_gauge ||
               context.current_gauge.has_ellipse_gauge ||
               context.current_gauge.tool == "FindSegmentation" ||
               context.current_gauge.tool == "FindObject")
      {
        std::string promptSyncReason;
        SyncFindSegmentationPromptListsFromShapeElements(promptSyncReason);
        SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:Run:apply_gauge");
        ApplyManualGaugeToGlobals(context);
      }
      context.debug_action = "Run";
      SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:Run:set_globals");
      for (const auto& input : context.runtime_int_vars)
      {
        if (input.first.rfind("global_", 0) == 0)
          m_parserDebugBridge.SetGlobalInt(input.first, input.second);
      }
      // CxScript result statements (for example
      // global_valid_points_count = m_line.getvalidpointcount();) are external
      // numeric destinations too.  They are discovered by AnalyzeScript but
      // were never registered with the parser, so a successful tool call could
      // still fail at its first result write.  Bind only missing global_* names;
      // global_matInput is an Image object and must never enter this numeric
      // external-variable path.
      for (const ScriptVariableView& observed : context.global_variable_views)
      {
        if (observed.name == "global_matInput" ||
            observed.name.rfind("global_", 0) != 0)
          continue;
        if (context.runtime_int_vars.find(observed.name) ==
            context.runtime_int_vars.end())
        {
          context.runtime_int_vars.emplace(observed.name, 0);
          m_parserDebugBridge.SetGlobalInt(observed.name, 0);
        }
      }

      std::stringstream ss;
      ss << "\nManual effective globals:";
      ss << "\nimage=" << (m_imageViewImage.empty() ? "s_img0" : "m_imageViewImage");
      if (!m_imageViewImage.empty())
        ss << " size=" << m_imageViewImage.cols << "x" << m_imageViewImage.rows;
      else if (!s_img0.empty())
        ss << " size=" << s_img0.cols << "x" << s_img0.rows;
      ss << "\nscript=" << context.loaded_script_path;
      ss << "\neditor_source=" << context.editor_source;
      ss << "\nellipse_roi=(" << context.runtime_int_vars["global_ellipse_x0"] << ","
         << context.runtime_int_vars["global_ellipse_y0"] << ","
         << context.runtime_int_vars["global_ellipse_x1"] << ","
         << context.runtime_int_vars["global_ellipse_y1"] << ")";
      ss << "\nellipse_inner_scale_percent="
         << context.runtime_int_vars["global_findellipse_inner_scale_percent"];
      ss << "\nroi=(" << context.runtime_int_vars["global_roi_x0"] << ","
         << context.runtime_int_vars["global_roi_y0"] << ","
         << context.runtime_int_vars["global_roi_x1"] << ","
         << context.runtime_int_vars["global_roi_y1"] << ")";
      ss << "\ncircle=(" << context.runtime_int_vars["global_circle_cx"] << ","
         << context.runtime_int_vars["global_circle_cy"] << ")";
      ss << "\ncircle_point=(" << context.runtime_int_vars["global_circle_px"] << ","
         << context.runtime_int_vars["global_circle_py"] << ")";
      ss << "\ncircle_inner_radius="
         << context.runtime_int_vars["global_circle_inner_radius"];
      ss << "\ncircle_outer_radius="
         << context.runtime_int_vars["global_circle_outer_radius"];
      ss << "\ncircle_ring_width="
         << context.runtime_int_vars["global_circle_ring_width"];
      ss << "\nfindcircle_arc=(enabled="
         << context.runtime_int_vars["global_findcircle_arc_enabled"]
         << ", start=" << context.runtime_int_vars["global_findcircle_arc_start_deg"]
         << ", end=" << context.runtime_int_vars["global_findcircle_arc_end_deg"]
         << ")";
      ss << "\nfindcircle_selected_edge="
         << context.runtime_int_vars["global_findcircle_selected_edge"];
      ss << "\nfindcircle_edge_count="
         << context.runtime_int_vars["global_findcircle_edge_count"];
      ss << "\nfindline_scan_direction="
         << context.runtime_int_vars["global_findline_scan_direction"];
      ss << "\nfindline_selected_edge="
         << context.runtime_int_vars["global_findline_selected_edge"];
      ss << "\nfindline_edge_count="
         << context.runtime_int_vars["global_findline_edge_count"];
      ss << "\nfindline_point_consistency=("
         << context.runtime_int_vars["global_findline_point_consistency_enabled"]
         << ", range="
         << context.runtime_int_vars["global_findline_point_consistency_range"]
         << ")";
      ss << "\nfindsetting=" << context.runtime_int_vars["global_findsetting"];
      ss << "\nfindline_objfilter="
         << context.runtime_int_vars["global_findline_objfilter"];
      ss << "\nfindline_findsetting="
         << context.runtime_int_vars["global_findline_findsetting"];
      ss << "\nfindcircle_findsetting="
         << context.runtime_int_vars["global_findcircle_findsetting"];
      ss << "\nsegmentation_mode="
         << context.runtime_int_vars["global_segmentation_mode"];
      ss << "\nsegmentation_threshold_percent="
         << context.runtime_int_vars["global_segmentation_threshold_percent"];
      ss << "\nsegmentation_positive=(enabled="
         << context.runtime_int_vars["global_segmentation_positive_enabled"]
         << ", x=" << context.runtime_int_vars["global_segmentation_positive_x"]
         << ", y=" << context.runtime_int_vars["global_segmentation_positive_y"]
         << ")";
      ss << "\nsegmentation_negative=(enabled="
         << context.runtime_int_vars["global_segmentation_negative_enabled"]
         << ", x=" << context.runtime_int_vars["global_segmentation_negative_x"]
         << ", y=" << context.runtime_int_vars["global_segmentation_negative_y"]
         << ")";
      ss << "\ngap=" << context.runtime_int_vars["global_gap"];
      ss << "\nlinegap=" << context.runtime_int_vars["global_linegap"];
      ss << "\nthreshold=" << context.runtime_int_vars["global_threshold"];
      ss << "\nmethod=" << context.runtime_int_vars["global_method"];
      const std::string effectiveGlobals = ss.str();

      CXLOG_INFO(
          "ManualConsole",
          "manual_run_input_snapshot",
          "READY",
          std::string("source=") +
              (sourceRefreshReason.empty() ? "editor" : "disk_refresh") +
              effectiveGlobals);

      if (legacyFindCircleMigrated)
        context.debug_reason = legacyMigrationReason;
      if (legacyFindLineMigrated)
        context.debug_reason = legacyFindLineMigrationReason;
      if (legacyFindSegmentationMigrated)
        context.debug_reason = legacyFindSegmentationMigrationReason;

      SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:Run:bind_image");
      bool imageBound = true;
      if (!m_imageViewImage.empty())
        imageBound = m_parserDebugBridge.StageGlobalMatInput(m_imageViewImage);
      else if (!s_img0.empty())
        imageBound = m_parserDebugBridge.StageGlobalMatInput(s_img0);

      // The Debug Compiler and RunCxScript must stage the same Torch request
      // context.  Keep this in the existing serial Parser-owner chain: the UI
      // action only requests a run, and this compiler block performs it.
      std::string torchRequestReason;
      const bool torchRequestReady = PrepareTorchUiRequestContext(
          context.editor_text,
          context.loaded_script_path,
          torchRequestReason);
      if (!torchRequestReady)
      {
        CXLOG_ERROR(
            "TorchUI",
            "torch_ui_request_blocked",
            "BLOCKED",
            torchRequestReason);
      }

      SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:Run:clear_old_runtime_shapes");
      if (torchRequestReady)
        m_annotationLayer.RemoveRuntimeOwnersNotIn(std::unordered_set<std::string>{});

      SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:Run:run_script");
      context.run_state = "running";
      ran = imageBound && torchRequestReady &&
          m_parserDebugBridge.RunScript(context.editor_text);
      context.run_state = ran ? "runtime_finished" : "failed";
      context.debug_status = ran ? "runtime_executed" : "run_failed";
      context.debug_reason = ran
        ? ("exact Script Editor text executed through ParserDebugBridge" +
           (torchRequestReason.empty()
               ? std::string()
               : " | " + torchRequestReason))
        : (!torchRequestReady
          ? torchRequestReason
          : (imageBound
          ? ("ParserDebugBridge rejected the Script Editor text: " +
             m_parserDebugBridge.LastError())
          : "ParserDebugBridge rejected Run: no Image View/default image available for global_matInput"));
      // The complete input snapshot is already preserved in the unified log
      // and debug snapshot.  Keep the operator-facing reason compact so that
      // it does not hide Torch status and artifact panels after a run.

      if (ran)
      {
        CXLOG_INFO(
            "ManualConsole",
            "manual_run_finished",
            "runtime_executed",
            "script=" + context.loaded_script_path +
                " reason=" + context.debug_reason);
      }
      else
      {
        CXLOG_ERROR(
            "ManualConsole",
            "manual_run_finished",
            "run_failed",
            "script=" + context.loaded_script_path +
                " reason=" + context.debug_reason);
      }

      m_scriptResult.source = "manual_console_editor";
      m_scriptResult.script_path = context.loaded_script_path;
      m_scriptResult.status = ran ? "PENDING" : "BLOCKED";
      m_scriptResult.reason = context.debug_reason;
      m_scriptResult.runtime_fillback_status = ran
        ? "runtime_objects_queried"
        : "not_started";

      SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:Run:refresh_runtime_objects");
      RefreshRuntimeObjectTable(
        "Manual Console Run", ran ? "runtime_executed" : "BLOCKED");
      std::string manualConclusionStatus = context.debug_status;
      if (ran &&
          context.current_result_ref.result_type == "FindSegmentationResult" &&
          !context.current_result_ref.status.empty())
      {
        manualConclusionStatus = context.current_result_ref.status;
      }
      CXLOG_INFO(
          "ManualConsole",
          "manual_run_conclusion",
          manualConclusionStatus,
          "script=" + context.loaded_script_path +
              " debug_status=" + context.debug_status +
              " script_result_status=" + m_scriptResult.status +
              " result_type=" + context.current_result_ref.result_type +
              " result_status=" + context.current_result_ref.status +
              " reason=" + context.debug_reason);
      // Runtime projection is allowed to update result objects, but it must
      // not visually roll the candidate editor back to values from the
      // selected Evidence row.  Preserve the input snapshot until the run has
      // been fully packaged.
      if (usePendingExecutionSnapshot || useStagedTorchSnapshot)
      {
        context.current_gauge = frozenGauge;
        context.runtime_int_vars = frozenGlobals;
      }

      SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:Run:save_snapshot");
      std::string snapshotPath;
      std::string snapshotReason;
      if (!SaveCxDebugSnapshotText(context, snapshotPath, snapshotReason))
      {
        context.debug_reason += " | debug snapshot save failed: " + snapshotReason;
      }
      if (runRequestedByCandidateSave &&
          !context.last_evidence_candidate_id.empty() &&
          !context.last_evidence_candidate_dir.empty())
      {
        std::filesystem::path candidateDir(context.last_evidence_candidate_dir);
        CxEvidenceCandidateSaveOptions options;
        if (candidateDir.has_parent_path() &&
            candidateDir.parent_path().has_parent_path())
        {
          options.root_dir = candidateDir.parent_path().parent_path().string();
        }
        options.candidate_id = context.last_evidence_candidate_id;
        options.case_id_override =
            context.current_evidence_selection.valid &&
                    !context.current_evidence_selection.case_id.empty()
                ? context.current_evidence_selection.case_id
                : (candidateDir.has_parent_path()
                       ? candidateDir.parent_path().filename().string()
                       : std::string());
        options.mode = ran ? "runtime_finished" : "runtime_failed";
        options.request_run = false;
        options.add_to_evidence_chain = false;
        options.preserve_input_snapshots = true;
        CxEvidenceCandidateSaveResult candidateResult;
        if (!SaveEvidenceCandidatePackage(context, options, candidateResult))
        {
          context.debug_reason +=
              " | evidence candidate post-run update failed: " +
              candidateResult.reason;
        }
        else
        {
          context.last_evidence_candidate_dir = candidateResult.candidate_dir;
        }
      }
      SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:Run:end");
      if (usePendingExecutionSnapshot || useStagedTorchSnapshot)
      {
        context.has_pending_execution_snapshot = false;
        context.pending_evidence_selection_rehydrate = false;
        context.pending_execution_globals.clear();
        context.pending_execution_candidate_id.clear();
        context.last_evidence_candidate_id.clear();
        context.last_evidence_candidate_dir.clear();
        context.last_evidence_candidate_reason.clear();
      }
    }
  }

  ImGui::SameLine();
  SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:step_button");
  if (ImGui::Button("Step", ImVec2(btnWidth, 0)))
  {
    SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:Step:begin");
    DebugStepOnceWithSnapshot(context);
    RequestRuntimeShapeSync("DebugCompiler:Step");
    std::string snapshotPath;
    std::string snapshotReason;
    if (!SaveCxDebugSnapshotText(context, snapshotPath, snapshotReason))
    {
      context.debug_reason += " | debug snapshot save failed: " + snapshotReason;
    }
    SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:Step:end");
  }

  ImGui::SameLine();
  SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:continue_button");
  if (ImGui::Button("Continue", ImVec2(btnWidth, 0)))
  {
    SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:Continue:begin");
    context.run_state = "running";
    AnalyzeScript(context);
    const int maxContinueSteps = 512;
    int executedSteps = 0;
    while (context.run_state == "running" ||
           context.run_state == "runtime_step")
    {
      if (context.current_line >= static_cast<int>(context.line_views.size()))
      {
        context.run_state = "runtime_finished";
        context.debug_status = "PENDING";
        context.debug_reason =
          "script finished; global_current_status remains PENDING; judge/rule not executed";
        break;
      }

      const int beforeLine = context.current_line;
      DebugStepOnceWithSnapshot(context);
      ++executedSteps;

      if (context.run_state == "blocked" ||
          context.run_state == "failed" ||
          context.run_state == "runtime_finished")
      {
        break;
      }

      if (executedSteps >= maxContinueSteps)
      {
        context.run_state = "blocked";
        context.debug_status = "BLOCKED";
        context.debug_reason =
          "Continue stopped by step budget; possible debug cursor stall";
        break;
      }

      if (context.current_line == beforeLine)
      {
        context.run_state = "blocked";
        context.debug_status = "BLOCKED";
        context.debug_reason =
          "Continue stopped because debug cursor did not advance";
        break;
      }
    }

    RequestRuntimeShapeSync("DebugCompiler:Continue");
    std::string snapshotPath;
    std::string snapshotReason;
    if (!SaveCxDebugSnapshotText(context, snapshotPath, snapshotReason))
    {
      context.debug_reason += " | debug snapshot save failed: " + snapshotReason;
    }
    SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:Continue:end");
  }

  ImGui::SameLine();
  SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:reset_button");
  if (ImGui::Button("Reset", ImVec2(btnWidth, 0)))
  {
    ResetDebugRuntimeForReplay(context);
    context.debug_action = "Reset";
    context.debug_status = "PENDING";
    context.debug_reason = "Debug runtime reset";
  }

  ImGui::Separator();

  SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:status_text");
  ImGui::Text("Line: %d/%d",
              context.current_line + 1,
              static_cast<int>(context.line_views.size()));
  ImGui::Text("State: %s", context.run_state.c_str());
  ImGui::Text("Status: %s", context.debug_status.c_str());

  if (!context.debug_reason.empty())
  {
    std::string displayReason = context.debug_reason;
    constexpr std::size_t kMaxReasonDisplay = 4096;
    if (displayReason.size() > kMaxReasonDisplay)
    {
      displayReason.resize(kMaxReasonDisplay);
      displayReason += "\n... [reason truncated in UI; full text is saved in debug snapshot/log]";
    }
    ImGui::TextWrapped("Reason: %s", displayReason.c_str());
  }

  ImGui::PopID();
  SetCxCrashBreadcrumb("drawManualStateTestConsole:DebugCompiler:end");
}

void ViewController::DrawCxParserExtLineViewsPanel(ManualTestContext& context)
{
  if (!ImGui::CollapsingHeader("Line Views", ImGuiTreeNodeFlags_DefaultOpen))
    return;

  if (context.line_views.empty())
  {
    ImGui::TextDisabled("No line views available. Compile a script first.");
    return;
  }

  ImGui::BeginChild("line_views", ImVec2(-1, 150), true);

  if (ImGui::BeginTable("line_views_table", 4,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
  {
    ImGui::TableSetupColumn("Line");
    ImGui::TableSetupColumn("Type");
    ImGui::TableSetupColumn("Content");
    ImGui::TableSetupColumn("Status");
    ImGui::TableHeadersRow();

    for (std::size_t i = 0; i < context.line_views.size(); ++i)
    {
      const ScriptLineView& lv = context.line_views[i];
      bool isCurrent = static_cast<int>(i) == context.current_line;

      ImGui::TableNextRow();
      if (isCurrent)
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(40, 80, 140, 255));

      ImGui::TableSetColumnIndex(0); ImGui::Text("%d", lv.line_no);
      ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(lv.statement.c_str());
      ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(lv.statement.c_str());
      ImGui::TableSetColumnIndex(3); ImGui::TextUnformatted(lv.status.c_str());
    }

    ImGui::EndTable();
  }

  ImGui::EndChild();
}

void ViewController::DrawCxParserExtStatementViewsPanel(ManualTestContext& context)
{
  if (!ImGui::CollapsingHeader("Statement Views", ImGuiTreeNodeFlags_DefaultOpen))
    return;

  if (context.cxparser_ext_statement_views.empty())
  {
    ImGui::TextDisabled("No statement views available.");
    return;
  }

  ImGui::BeginChild("statement_views", ImVec2(-1, 150), true);

  if (ImGui::BeginTable("statement_views_table", 3,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
  {
    ImGui::TableSetupColumn("Line");
    ImGui::TableSetupColumn("Type");
    ImGui::TableSetupColumn("Details");
    ImGui::TableHeadersRow();

    for (const auto& sv : context.cxparser_ext_statement_views)
    {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0); ImGui::Text("%d", sv.line_no);
      ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(sv.statement_type.c_str());
      ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(sv.reason.c_str());
    }

    ImGui::EndTable();
  }

  ImGui::EndChild();
}

void ViewController::DrawCxParserExtObjectAssignmentsPanel(ManualTestContext& context)
{
  if (!ImGui::CollapsingHeader("Object Assignments", ImGuiTreeNodeFlags_DefaultOpen))
    return;

  if (context.cxparser_ext_object_assignments.empty())
  {
    ImGui::TextDisabled("No object assignments available.");
    return;
  }

  ImGui::BeginChild("object_assignments", ImVec2(-1, 150), true);

  if (ImGui::BeginTable("object_assignments_table", 4,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
  {
    ImGui::TableSetupColumn("Object");
    ImGui::TableSetupColumn("Property");
    ImGui::TableSetupColumn("Value");
    ImGui::TableSetupColumn("Line");
    ImGui::TableHeadersRow();

    for (const auto& oa : context.cxparser_ext_object_assignments)
    {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(oa.lhs_variable.c_str());
      ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(oa.lhs_type.c_str());
      ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(oa.returned_object_ref.c_str());
      ImGui::TableSetColumnIndex(3); ImGui::Text("%d", oa.line_no);
    }

    ImGui::EndTable();
  }

  ImGui::EndChild();
}

const char* UiTextOrDash(const std::string& text)
{
  return text.empty() ? "-" : text.c_str();
}

std::string InferCurrentTemplateTool(const ManualTestContext& context)
{
  for (const auto& entry : context.catalog_entries)
  {
    if (entry.script_id == context.active_script_case_name)
      return entry.tool;
  }
  return "unknown";
}

std::string InferCurrentTemplatePath(const ManualTestContext& context)
{
  for (const auto& entry : context.catalog_entries)
  {
    if (entry.script_id == context.active_script_case_name)
      return entry.path;
  }
  return "";
}

int CountSelectedParamCandidates(const ManualTestContext& context)
{
  int count = 0;
  for (const auto& candidate : context.param_regression.candidates)
  {
    if (candidate.selected_for_probe)
      ++count;
  }
  return count;
}
