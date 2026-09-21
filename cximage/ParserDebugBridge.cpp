#include "ParserDebugBridge.h"

#include "FindCircle.h"
#include "FindLine.h"
#include "Image.h"
#include "CircleRingGauge.h"
#include "FastMatchDiagnostic.h"
#include "FastMatch.h"
#include "CxCrashLogHandler.h"
#include "CxUnifiedLog.h"
#include "imagemanager.h"

#if defined(CXVISION_ENABLE_CXPARSER_EXT_DEBUG_INPROC)
#include "cxscript_debug_embedded_runner.h"
#endif

#include <sstream>
#include <utility>
#include <exception>

namespace
{
std::string CanonicalGlobalName(const std::string& name)
{
  if (name.rfind("global_", 0) == 0)
    return name;
  return "global_" + name;
}
}

bool ParserDebugBridge::CompileScript(const std::string& scriptText)
{
  myLastError.clear();
  if (myOwner == nullptr || scriptText.empty()) return false;
  ResetRuntime();
  if (!RebindGlobalInputs()) return false;
  const std::string prepared = PrepareScript(scriptText);
  return myOwner->ExecuteScript(prepared, myLastError);
}

bool ParserDebugBridge::CompileScriptOnly(const std::string& scriptText)
{
    myLastError.clear();

    if (myOwner == nullptr)
    {
        myLastError = "parser owner is not bound";
        return false;
    }

    if (scriptText.empty())
    {
        myLastError = "script text is empty";
        return false;
    }

    SetCxCrashBreadcrumb("ParserDebugBridge::CompileScriptOnly:reset_runtime");
    ResetRuntime();

    SetCxCrashBreadcrumb("ParserDebugBridge::CompileScriptOnly:rebind_globals");
    if (!RebindGlobalInputs())
        return false;

    if (scriptText.find("global_matInput") != std::string::npos)
    {
        Image* inputImage = QueryImage("global_matInput");
        if (inputImage == nullptr)
        {
            myLastError = "global_matInput is required by script but is not bound";
            return false;
        }

        if (inputImage->getmat().empty())
        {
            myLastError = "global_matInput image is empty";
            return false;
        }
    }

    const std::string prepared = PrepareScript(scriptText);

    SetCxCrashBreadcrumb("ParserDebugBridge::CompileScriptOnly:compile_only");
    return myOwner->CompileScriptOnly(prepared, myLastError);
}

bool ParserDebugBridge::RunScript(const std::string& scriptText)
{
  myLastError.clear();

  if (myOwner == nullptr || scriptText.empty())
    return false;

  ResetRuntime();

  if (!RebindGlobalInputs())
    return false;

  if (scriptText.find("global_matInput") != std::string::npos)
  {
    if (!EnsureAlgorithmRuntimeResources())
      return false;
  }

  const std::string prepared =
      PrepareScript(scriptText);

  return myOwner->ExecuteScript(
      prepared,
      myLastError);
}

bool ParserDebugBridge::RunPrefixToLine(const std::string& scriptText, int lineNo)
{
  myLastError.clear();
  if (myOwner == nullptr || lineNo <= 0) return false;
  ResetRuntime();
  if (!RebindGlobalInputs()) return false;
  std::istringstream input(scriptText);
  std::ostringstream prefix;
  std::string line;
  int current = 0;
  while (current < lineNo && std::getline(input, line))
  {
    ++current;
    if (line.find("log_alg") != std::string::npos)
      continue;
    prefix << line << '\n';
  }
  std::string prefixText = prefix.str();
  int braceDepth = 0;
  for (char value : prefixText)
  {
    if (value == '{') ++braceDepth;
    else if (value == '}' && braceDepth > 0) --braceDepth;
  }
  while (braceDepth-- > 0) prefixText += "}\n";
  prefixText = PrepareScript(prefixText);
  return current > 0 && myOwner->ExecuteScript(prefixText, myLastError);
}

void* ParserDebugBridge::QueryClassObject(const std::string& type,
                                          const std::string& name) const
{
  if (myOwner == nullptr) return nullptr;
  void* object = myOwner->GetClassObj(type, name);
  if (object != nullptr)
    return object;

  auto tryType = [&](const char* alias) -> void*
  {
    if (type == alias)
      return nullptr;
    return myOwner->GetClassObj(alias, name);
  };

  if (type == "FindLine" || type == "Findline" || type == "findline")
  {
    if ((object = tryType("FindLine")) != nullptr) return object;
    if ((object = tryType("Findline")) != nullptr) return object;
    if ((object = tryType("findline")) != nullptr) return object;
  }
  else if (type == "FindCircle" || type == "Findcircle" || type == "findcircle")
  {
    if ((object = tryType("FindCircle")) != nullptr) return object;
    if ((object = tryType("Findcircle")) != nullptr) return object;
    if ((object = tryType("findcircle")) != nullptr) return object;
  }
  else if (type == "FindEllipse" || type == "Findellipse" || type == "findellipse")
  {
    if ((object = tryType("FindEllipse")) != nullptr) return object;
    if ((object = tryType("Findellipse")) != nullptr) return object;
    if ((object = tryType("findellipse")) != nullptr) return object;
  }
  else if (type == "FastMatch" || type == "fastmatch" || type == "CFastMatch")
  {
    if ((object = tryType("FastMatch")) != nullptr) return object;
    if ((object = tryType("fastmatch")) != nullptr) return object;
    if ((object = tryType("CFastMatch")) != nullptr) return object;
  }
  else if (type == "FindRect" || type == "Findrect" || type == "findrect")
  {
    if ((object = tryType("FindRect")) != nullptr) return object;
    if ((object = tryType("Findrect")) != nullptr) return object;
    if ((object = tryType("findrect")) != nullptr) return object;
  }

  return nullptr;
}

bool ParserDebugBridge::QueryObjectExists(const std::string& type,
                                          const std::string& name) const
{
  return QueryClassObject(type, name) != nullptr;
}

std::vector<std::string> ParserDebugBridge::ListClassObjectNames(
  const std::string& type) const
{
  std::vector<std::string> names;
  if (myOwner == nullptr)
    return names;
  const int count = myOwner->ObjectCount(type);
  for (int i = 0; i < count; ++i)
  {
    std::string name = myOwner->ObjectName(type, i);
    if (!name.empty())
      names.push_back(name);
  }
  return names;
}

Image* ParserDebugBridge::QueryImage(const std::string& name) const
{
  return static_cast<Image*>(QueryClassObject("Image", name));
}

bool ParserDebugBridge::QueryDouble(const std::string& name, double& value) const
{
  if (myOwner == nullptr) return false;
  if (myOwner->QueryExternalDouble(name, value))
    return true;

  double* runtimeValue = static_cast<double*>(myOwner->GetDoubleValue(name));
  if (runtimeValue == nullptr) return false;
  value = *runtimeValue;
  return true;
}

bool ParserDebugBridge::SetDouble(const std::string& name, double value)
{
  if (myOwner == nullptr || !myOwner->IsObjectVar(name.c_str())) return false;
  return ApplyStatement(name + "=" + std::to_string(value) + ";");
}

bool ParserDebugBridge::SetGlobalInt(const std::string& name, int value)
{
  myGlobalNumericInputs[CanonicalGlobalName(name)] =
    static_cast<double>(value);
  return true;
}

bool ParserDebugBridge::SetGlobalDouble(const std::string& name, double value)
{
  myGlobalNumericInputs[CanonicalGlobalName(name)] = value;
  return true;
}

bool ParserDebugBridge::SetGlobalString(
  const std::string& name,
  const std::string& value)
{
  const std::string fullName = CanonicalGlobalName(name);
  myGlobalStringInputs[fullName] = value;
  return true;
}

void ParserDebugBridge::RemoveGlobalString(const std::string& name)
{
  myGlobalStringInputs.erase(CanonicalGlobalName(name));
}

bool ParserDebugBridge::ApplyStatement(const std::string& statement)
{
  return myOwner != nullptr && !statement.empty() &&
         myOwner->Compile(statement.c_str());
}

std::string ParserDebugBridge::PrepareScript(const std::string& scriptText) const
{
  std::string prepared = scriptText;

  // CxScript does not support declaring a general-purpose `string` local.
  // For externally supplied string inputs, expand the identifier at the call
  // site to a normal quoted literal.  This keeps the executed statement
  // visible in the line trace and avoids relying on parser string-constant
  // state across object declarations.  Headless may continue to use its
  // already proven DefineStrConst path.
  auto escapeStringLiteral = [](const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size() + 16);
    for (char ch : value)
    {
      switch (ch)
      {
      case '\\': escaped += "\\\\"; break;
      case '"': escaped += "\\\""; break;
      case '\n': escaped += "\\n"; break;
      case '\r': escaped += "\\r"; break;
      case '\t': escaped += "\\t"; break;
      default: escaped.push_back(ch); break;
      }
    }
    return escaped;
  };

  for (const auto& input : myGlobalStringInputs)
  {
    const std::string& identifier = input.first;
    const std::string replacement =
        "\"" + escapeStringLiteral(input.second) + "\"";
    std::size_t stringPosition = 0;
    while ((stringPosition = prepared.find(identifier, stringPosition)) !=
           std::string::npos)
    {
      const bool leftBoundary = stringPosition == 0 ||
          !(std::isalnum(static_cast<unsigned char>(prepared[stringPosition - 1])) ||
            prepared[stringPosition - 1] == '_');
      const std::size_t after = stringPosition + identifier.size();
      const bool rightBoundary = after >= prepared.size() ||
          !(std::isalnum(static_cast<unsigned char>(prepared[after])) ||
            prepared[after] == '_');
      if (!leftBoundary || !rightBoundary)
      {
        stringPosition += identifier.size();
        continue;
      }
      prepared.replace(stringPosition, identifier.size(), replacement);
      stringPosition += replacement.size();
    }
  }

  const std::string source = "global_matInput";
  const std::string runtimeName = "global_matInput";
  std::size_t position = 0;
  while ((position = prepared.find(source, position)) != std::string::npos)
  {
    prepared.replace(position, source.size(), runtimeName);
    position += runtimeName.size();
  }

  for (size_t i = 0; i < prepared.size(); ++i)
  {
    if (prepared[i] != '&')
      continue;

    size_t previous = i;
    while (previous > 0 && std::isspace(static_cast<unsigned char>(prepared[previous - 1])))
      --previous;
    size_t next = i + 1;
    while (next < prepared.size() && std::isspace(static_cast<unsigned char>(prepared[next])))
      ++next;

    const bool argument_position =
        previous > 0 && (prepared[previous - 1] == '(' || prepared[previous - 1] == ',');
    const bool object_identifier =
        next < prepared.size() &&
        (std::isalpha(static_cast<unsigned char>(prepared[next])) || prepared[next] == '_');
    if (argument_position && object_identifier)
    {
      prepared.erase(i, 1);
      --i;
    }
  }

  return prepared;
}

bool ParserDebugBridge::StageGlobalMatInput(const cv::Mat& image)
{
  if (image.empty())
  {
    myLastError = "cannot stage empty global_matInput image";
    return false;
  }
  myGlobalMatInput = image.clone();
  myGlobalMatInputBoundSerial = 0;
  return !myGlobalMatInput.empty();
}

bool ParserDebugBridge::SetGlobalMatInput(const cv::Mat& image)
{
  if (!StageGlobalMatInput(image)) return false;
  ResetRuntime();
  return BindStagedGlobalMatInput();
}

bool ParserDebugBridge::BindStagedGlobalMatInput()
{
  if (myGlobalMatInput.empty())
  {
    myLastError = "global_matInput image is empty";
    return false;
  }
  if (myOwner == nullptr)
  {
    myLastError = "parser owner is not bound";
    return false;
  }

  Image* runtimeImage = nullptr;
  const bool mustDeclareAfterReset =
      myGlobalMatInputBoundSerial != myRuntimeResetSerial;

  if (mustDeclareAfterReset)
  {
    SetCxCrashBreadcrumb("ParserDebugBridge::BindStagedGlobalMatInput:declare_after_reset");
    if (!myOwner->Compile("Image global_matInput;"))
    {
      SetCxCrashBreadcrumb("ParserDebugBridge::BindStagedGlobalMatInput:declare_failed_query_existing");
      runtimeImage = QueryImage("global_matInput");
      if (runtimeImage == nullptr)
      {
        myLastError = "failed to declare global_matInput after parser reset";
        return false;
      }
    }
  }
  else
  {
    SetCxCrashBreadcrumb("ParserDebugBridge::BindStagedGlobalMatInput:query");
    runtimeImage = QueryImage("global_matInput");
    if (runtimeImage == nullptr)
    {
      SetCxCrashBreadcrumb("ParserDebugBridge::BindStagedGlobalMatInput:declare_missing");
      if (!myOwner->Compile("Image global_matInput;"))
      {
        myLastError = "failed to declare missing global_matInput";
        return false;
      }
    }
  }

  SetCxCrashBreadcrumb("ParserDebugBridge::BindStagedGlobalMatInput:get_image");
  runtimeImage = QueryImage("global_matInput");
  if (runtimeImage == nullptr)
  {
    myLastError = "global_matInput object is unavailable after declaration";
    return false;
  }
  if (myGlobalMatInput.cols <= 0 || myGlobalMatInput.rows <= 0)
  {
    myLastError = "staged global_matInput has invalid size";
    return false;
  }

  SetCxCrashBreadcrumb("ParserDebugBridge::BindStagedGlobalMatInput:copy");
  try
  {
    runtimeImage->copyFromMat(myGlobalMatInput);
  }
  catch (const cv::Exception& error)
  {
    myLastError = std::string("copy global_matInput failed: ") + error.what();
    return false;
  }
  catch (const std::exception& error)
  {
    myLastError = std::string("copy global_matInput failed: ") + error.what();
    return false;
  }
  myGlobalMatInputBoundSerial = myRuntimeResetSerial;
  return true;
}

bool ParserDebugBridge::EnsureAlgorithmRuntimeResources()
{
  Image* input_image = QueryImage("global_matInput");

  if (input_image == nullptr)
  {
    myLastError = "global_matInput is not bound";
    return false;
  }

  const cv::Mat& mat = input_image->getmat();

  if (mat.empty())
  {
    myLastError = "global_matInput image is empty";
    return false;
  }

  if (!ImageManager::EnsureAlgorithmRuntimeResources(
          mat.cols,
          mat.rows))
  {
    myLastError = "failed to initialize cximage algorithm resources";
    return false;
  }

  return true;
}

bool ParserDebugBridge::RebindGlobalInputs()
{
  if (myOwner == nullptr)
  {
    myLastError = "parser owner is not bound";
    return false;
  }

  std::string reason;
  for (auto& input : myGlobalNumericInputs)
  {
    if (!myOwner->DefineExternalDouble(
          input.first, &input.second, reason))
    {
      myLastError = reason;
      return false;
    }
  }

  // Numeric globals and the parser-owned Image use real runtime bindings.
  // GUI string inputs are intentionally different: PrepareScript() expands
  // them to quoted literals at their visible call sites.  Registering those
  // same strings as parser constants here makes the second serial run fail
  // with a duplicate-name error because ClearAll() does not remove constants.
  if (!myGlobalMatInput.empty() && !BindStagedGlobalMatInput())
  {
    if (myLastError.empty())
      myLastError = "failed to bind global_matInput image";
    return false;
  }

  return true;
}

void ParserDebugBridge::ClearGlobalInputs()
{
  myGlobalMatInput.release();
  myGlobalNumericInputs.clear();
  myGlobalStringInputs.clear();
  myGlobalMatInputBoundSerial = 0;
}

std::vector<ParserDebugObjectSnapshot> ParserDebugBridge::SnapshotRuntimeObjects(
  const std::string& lastMethod, int lastUpdateLine,
  const std::string& runtimeStatus) const
{
  const std::pair<const char*, const char*> objects[] = {
    {"Image", "global_matInput"}, {"Image", "m_occtimage"},
    {"FindCircle", "afindcircle0"},
    {"FindCircle", "afindcircle1"},
    {"CircleRingGauge", "ring_gauge"},
    {"FastMatchDiagnostic", "fm"},
    {"FastMatch", "m_match"}
  };
  std::vector<ParserDebugObjectSnapshot> snapshots;
  for (const auto& item : objects)
  {
    ParserDebugObjectSnapshot snapshot;
    snapshot.type = item.first;
    snapshot.name = item.second;
    snapshot.exists_in_parser = QueryObjectExists(snapshot.type, snapshot.name);
    snapshot.runtime_state = snapshot.exists_in_parser ? "alive" : "PENDING";
    snapshot.last_method = lastMethod;
    snapshot.last_update_line = lastUpdateLine;
    snapshot.visualizable = true;
    snapshot.stale = !snapshot.exists_in_parser ||
      (runtimeStatus != "runtime_executed" && runtimeStatus != "runtime_queried");
    snapshot.visual_source = snapshot.stale ? "stale_runtime" : "runtime_object";
    snapshot.value_summary = snapshot.exists_in_parser ? "runtime object available" :
                                                          "not found in parser";
    if (snapshot.type == "Image" && snapshot.exists_in_parser)
    {
      Image* image = QueryImage(snapshot.name);
      if (image != nullptr)
        snapshot.value_summary = "runtime image " +
          std::to_string(image->getmat().cols) + "x" +
          std::to_string(image->getmat().rows);
    }
    else if (snapshot.type == "FindCircle" && snapshot.exists_in_parser)
    {
      FindCircle* circle = static_cast<FindCircle*>(
        QueryClassObject(snapshot.type, snapshot.name));
      if (circle != nullptr)
      {
        snapshot.has_circle = true;
        snapshot.circle_cx = static_cast<float>(circle->getcirclecentx());
        snapshot.circle_cy = static_cast<float>(circle->getcirclecenty());
        snapshot.circle_inner = static_cast<float>(circle->getcirclepax());
        snapshot.circle_radius = static_cast<float>(circle->getcirclepay());
        snapshot.value_summary = "circle=" +
          std::to_string(circle->getcirclecentx()) + "," +
          std::to_string(circle->getcirclecenty()) + "," +
          std::to_string(circle->getcirclepax()) + "," +
          std::to_string(circle->getcirclepay());
      }
    }
    else if (snapshot.type == "CircleRingGauge" && snapshot.exists_in_parser)
    {
      CircleRingGauge* ring_gauge = static_cast<CircleRingGauge*>(
        QueryClassObject(snapshot.type, snapshot.name));
      if (ring_gauge != nullptr)
      {
        snapshot.ring_outer_radius = ring_gauge->m_outer_radius;
        snapshot.ring_inner_radius = ring_gauge->m_inner_radius;
        snapshot.ring_thickness = ring_gauge->m_thickness;
        snapshot.ring_center_distance = ring_gauge->m_center_distance;
        snapshot.ring_concentric_ok = ring_gauge->m_concentric_ok;
        snapshot.ring_inside_ok = ring_gauge->m_inside_ok;
        snapshot.ring_thickness_ok = ring_gauge->m_thickness_ok;
        snapshot.ring_score = ring_gauge->m_score;
        snapshot.ring_status = ring_gauge->m_status;
        snapshot.ring_reason = ring_gauge->m_reason;
        snapshot.ring_result_ref = ring_gauge->m_result_ref;
        snapshot.value_summary = "CircleRingGauge: status=" + ring_gauge->m_status +
          ", score=" + std::to_string(ring_gauge->m_score);
      }
    }
    else if (snapshot.type == "FastMatchDiagnostic" && snapshot.exists_in_parser)
    {
      FastMatchDiagnostic* fm = static_cast<FastMatchDiagnostic*>(
        QueryClassObject(snapshot.type, snapshot.name));
      if (fm != nullptr)
      {
        snapshot.has_fastmatch_diagnostic = true;
        snapshot.fastmatch_allowed = fm->allowed() != 0;
        snapshot.fastmatch_status = std::to_string(fm->status_code());
        snapshot.fastmatch_reason = std::to_string(fm->reason_code());
        snapshot.fastmatch_result_ref = fm->result_ref();
        snapshot.value_summary = "FastMatchDiagnostic: allowed=" +
          std::string(fm->allowed() ? "true" : "false") +
          ", status_code=" + std::to_string(fm->status_code());
      }
    }
    else if (snapshot.type == "FastMatch" && snapshot.exists_in_parser)
    {
      FastMatch* fm = static_cast<FastMatch*>(
        QueryClassObject(snapshot.type, snapshot.name));
      if (fm != nullptr)
      {
        snapshot.has_fastmatch = true;
        const int model_point_count = fm->getmodelpointcount();
        snapshot.fastmatch_model_available = model_point_count > 0;
        snapshot.fastmatch_model_point_count = model_point_count;
        snapshot.fastmatch_candidate_count = fm->getresultcandidatecount();
        snapshot.fastmatch_best_index = fm->getresultbestindex();
        snapshot.fastmatch_best_score = fm->getresultbestscore();
        const int candidate_count = fm->getresultcandidatecount();
        const int best_idx = fm->getresultbestindex();
        if (best_idx >= 0 && best_idx < candidate_count)
        {
          snapshot.fastmatch_best_x = fm->getresultcentx(best_idx);
          snapshot.fastmatch_best_y = fm->getresultcenty(best_idx);
        }
        const RectsShape& result_rects = *fm->getresultrects();
        snapshot.fastmatch_has_result_box = result_rects.size() > 0;
        snapshot.fastmatch_result_status = candidate_count > 0 ? "geometry_result_available" : "PENDING";
        snapshot.value_summary = "FastMatch: candidates=" + std::to_string(candidate_count) +
          ", best_score=" + std::to_string(fm->getresultbestscore());
      }
    }
    snapshots.push_back(snapshot);
  }
  return snapshots;
}

std::vector<ParserDebugVariableSnapshot>
ParserDebugBridge::SnapshotRuntimeVariables() const
{
  const char* names[] = {
    "doutputvalue",
    "current_status",
    "global_roi_x0",
    "global_roi_y0",
    "global_roi_x1",
    "global_roi_y1",
    "global_tool_half_width",
    "global_wgap",
    "global_hgap",
    "global_linegap",
    "global_min_edge_run_width_px",
    "global_threshold",
    "global_filterprofile",
    "global_method",
    "global_max_elapsed_ms",
    "global_max_scan_lines",
    "global_max_samples",
    "global_circle_cx",
    "global_circle_cy",
    "global_circle_px",
    "global_circle_py",
    "global_circle_inner_radius",
    "global_circle_outer_radius",
    "global_circle_ring_width",
    "global_findcircle_arc_enabled",
    "global_findcircle_arc_start_deg",
    "global_findcircle_arc_end_deg",
    "global_circle_effective_annulus",
    "global_circle_effective_inner_radius",
    "global_circle_effective_outer_radius",
    "global_circle_effective_ring_width",
    "global_valid_points_count",
    "global_has_fit_circle",
    "global_circle_radius",
    "global_avgdist",
    "global_gap",
    "global_learn_a_count",
    "global_learn_b_count",
    "global_learn_a2_count",
    "global_learn_b2_count",
    "global_learn_status_code",
    "global_match_count",
    "global_best_score",
    "global_model_point_count",
    "global_fastmatch_learn_shared",
    "global_fastmatch_learn_wgap_0",
    "global_fastmatch_learn_hgap_0",
    "global_fastmatch_learn_method_0",
    "global_fastmatch_learn_threshold_0",
    "global_fastmatch_learn_linegap_0",
    "global_fastmatch_learn_objfilter_0",
    "global_fastmatch_learn_compare_gap_0",
    "global_fastmatch_learn_edge_count_0",
    "global_fastmatch_learn_selected_edge_0",
    "global_fastmatch_learn_wgap_1",
    "global_fastmatch_learn_hgap_1",
    "global_fastmatch_learn_method_1",
    "global_fastmatch_learn_threshold_1",
    "global_fastmatch_learn_linegap_1",
    "global_fastmatch_learn_objfilter_1",
    "global_fastmatch_learn_compare_gap_1",
    "global_fastmatch_learn_edge_count_1",
    "global_fastmatch_learn_selected_edge_1",
    "global_fastmatch_learn_wgap_2",
    "global_fastmatch_learn_hgap_2",
    "global_fastmatch_learn_method_2",
    "global_fastmatch_learn_threshold_2",
    "global_fastmatch_learn_linegap_2",
    "global_fastmatch_learn_objfilter_2",
    "global_fastmatch_learn_compare_gap_2",
    "global_fastmatch_learn_edge_count_2",
    "global_fastmatch_learn_selected_edge_2",
    "global_fastmatch_learn_wgap_3",
    "global_fastmatch_learn_hgap_3",
    "global_fastmatch_learn_method_3",
    "global_fastmatch_learn_threshold_3",
    "global_fastmatch_learn_linegap_3",
    "global_fastmatch_learn_objfilter_3",
    "global_fastmatch_learn_compare_gap_3",
    "global_fastmatch_learn_edge_count_3",
    "global_fastmatch_learn_selected_edge_3"
  };
  std::vector<ParserDebugVariableSnapshot> snapshots;
  for (const char* name : names)
  {
    ParserDebugVariableSnapshot snapshot;
    snapshot.name = name;
    snapshot.exists_in_parser = QueryDouble(name, snapshot.value);
    snapshots.push_back(snapshot);
  }
  return snapshots;
}


bool ParserDebugBridge::RunCxParserExtDebugInProcess(
  const std::string& scriptPath,
  CxScriptSemanticBridgeResult& outResult)
{
  outResult = CxScriptSemanticBridgeResult{};

#if !defined(CXVISION_ENABLE_CXPARSER_EXT_DEBUG_INPROC)
  outResult.ok = false;
  outResult.status = "disabled";
  outResult.reason =
    "CXVISION_ENABLE_CXPARSER_EXT_DEBUG_INPROC is not enabled";
  return false;
#else
  cxparser_ext::debug::EmbeddedDebugRunRequest request;
  request.script_path = scriptPath;
  request.capture_structured_log = true;
  request.enable_line_view = true;
  request.enable_statement_view = true;
  request.enable_object_assignment = true;
  request.enable_method_trace = true;
  request.enable_return_object_trace = true;

  const cxparser_ext::debug::EmbeddedDebugRunResult result =
    cxparser_ext::debug::RunCxScriptDebugEmbedded(request);

  outResult.ok = result.ok;
  outResult.status = result.status;
  outResult.reason = result.reason;
  outResult.raw_log = result.raw_log;

  for (const cxparser_ext::debug::EmbeddedDebugLineView& item :
       result.line_views)
  {
    CxScriptLineView view;
    view.line_no = item.line_no;
    view.source_line = item.source_line;
    view.statement_type = item.statement_type;
    view.status = item.status;
    view.reason = item.reason;
    outResult.line_views.push_back(view);
  }

  for (const cxparser_ext::debug::EmbeddedDebugStatementView& item :
       result.statement_views)
  {
    CxScriptStatementView view;
    view.statement_id = item.statement_id;
    view.line_no = item.line_no;
    view.statement_type = item.statement_type;
    view.lhs_variable = item.lhs_variable;
    view.lhs_type = item.lhs_type;
    view.source_object = item.source_object;
    view.method_name = item.method_name;
    view.returned_object_ref = item.returned_object_ref;
    view.status = item.status;
    view.reason = item.reason;
    outResult.statement_views.push_back(view);
  }

  for (const cxparser_ext::debug::EmbeddedDebugObjectAssignment& item :
       result.object_assignments)
  {
    CxScriptObjectAssignmentView view;
    view.lhs_variable = item.lhs_variable;
    view.lhs_type = item.lhs_type;
    view.source_object = item.source_object;
    view.method_name = item.method_name;
    view.returned_object_ref = item.returned_object_ref;
    view.source_line = item.source_line;
    view.line_no = item.line_no;
    view.status = item.status;
    view.reason = item.reason;
    outResult.object_assignments.push_back(view);
  }

  return outResult.ok;
#endif
}
void ParserDebugBridge::Stop()
{
  if (myOwner != nullptr) myOwner->StopRun();
}

void ParserDebugBridge::ResetRuntime()
{
  ++myRuntimeResetSerial;
  myGlobalMatInputBoundSerial = 0;
  if (myOwner != nullptr) myOwner->ClearAll();
}
