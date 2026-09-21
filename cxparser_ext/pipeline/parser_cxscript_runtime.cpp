#include "parser_cxscript_runtime.h"

#include <chrono>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include "parser_cxscript_call_alias.h"
#include "parser_cxscript_runtime_bridge_helpers.h"
#include "parser_cxscript_runtime_flow_host_helpers.h"
#include "parser_cxscript_runtime_result_helpers.h"
#include "parser_cxscript_runtime_statement_helpers.h"
#include "parser_cxcore_classical_adapter.h"
#include "parser_cxcore_feature_action_bridge.h"
#include "parser_ensmallen_flow_host_adapter.h"
#include "parser_mlpack_baseline_adapter.h"
#include "../drivers/parser_dispatch_driver.h"

namespace cxparser_ext
{
namespace
{
std::string Trim(const std::string &text);
std::string StripQuotes(const std::string &value);
std::string StripTrailingSemicolon(const std::string &value);
bool StartsWith(const std::string &text, const char *prefix);
std::string ExtractCallArguments(const std::string &call_text);
bool TryParseReadResultExpression(const std::string &text, std::string &path);
bool TryParseCheckMethodCall(const std::string &text,
                             std::string &method_name,
                             std::vector<std::string> &args);
std::string ResolveReadResultPath(const CxScriptExecutionResult &result, const std::string &path);
void RefreshNamedResultViews(CxScriptExecutionResult &result);
void RefreshExecutionMultimodalSlices(CxScriptExecutionResult &result);
void RefreshPhase0UnifiedObjects(CxScriptExecutionResult &result);
void RefreshUnifiedReviewFoundation(CxScriptExecutionResult &result);
bool TryParseDouble(const std::string &text, double &value);
bool IsIdentifier(const std::string &name);
bool LooksLikeAssignmentExpr(const std::string &text,
                             std::string &lhs_name,
                             std::string &rhs_expr);
std::string ResultFieldValue(const CxScriptExecutionResult &result, const std::string &field);
std::string ResolveLooseFieldValue(const CxScriptExecutionResult &result, const std::string &field);
bool EvaluateEquals(const std::string &lhs, const std::string &rhs, const char *op);
bool EvaluateNumeric(double lhs, double rhs, const char *op);
bool EvaluateCheckMethod(const CxScriptStatement &stmt,
                         const CxScriptExecutionResult &result,
                         const std::map<std::string, std::string> &variables,
                         bool &ok,
                         std::string &detail);
bool WantsTorchDispatchMainline(const CxScriptExecutionContext &context);
bool TryExecuteTorchDispatchMainline(const CxScriptExecutionContext &context,
                                     const std::string &script_text,
                                     CxScriptExecutionResult &result);
void ConvertDispatchResult(const CxScriptExecutionContext &context,
                           const ParserDispatchResult &dispatch_result,
                           CxScriptExecutionResult &script_result);
bool ApplyTorchContractCaseBridge(const CxScriptExecutionContext &context,
                                  CxScriptExecutionResult &result);
bool TryHandleTorchHostStatement(const CxScriptStatement &stmt,
                                 CxScriptExecutionResult &result,
                                 std::map<std::string, std::string> &variables,
                                 std::string &detail_line);
bool HasEnsmallenOptimizationEvidence(const CxScriptExecutionResult &result);
std::string BuildEnsmallenObjectiveCurveValue(const CxScriptExecutionResult &result);
std::string BuildEnsmallenFeatureDistanceDeltaValue(const CxScriptExecutionResult &result);
std::string BuildEnsmallenCandidateRankValue(const CxScriptExecutionResult &result);
std::string BuildEnsmallenStabilityScoreValue(const CxScriptExecutionResult &result);
std::string BuildEnsmallenBestCandidateConfidenceValue(const CxScriptExecutionResult &result);
std::string BuildEnsmallenConvergenceStatusValue(const CxScriptExecutionResult &result);
void AppendRuntimeDetail(CxScriptExecutionResult &result,
                         const std::string &line,
                         bool collect_debug,
                         bool keep_in_lightweight);
void AddNamedResultObject(CxScriptExecutionResult &result,
                          const std::string &result_name,
                          const std::string &stage_name,
                          const std::string &object_name,
                          const std::string &status,
                          const std::string &failure_stage);
void AddNamedResultField(CxScriptExecutionResult &result,
                         const std::string &result_name,
                         const std::string &stage_name,
                         const std::string &field_name,
                         const std::string &value);

struct TorchScenarioHelperRun
{
  bool available = false;
  bool launched = false;
  bool finished = false;
  int exit_code = -1;
  long long runtime_ms = 0;
  std::string helper_path;
  std::string log_path;
  std::string status;
  std::string actual_device;
  std::string input_image_path;
  std::string output_image_path;
  std::string output_meta_path;
  std::string visual_status;
  std::string overlay_status;
  std::string top1_class;
  std::string confidence;
  std::vector<std::map<std::string, std::string> > unified_review_objects;
};

bool IsAiTaskEnvelopeContractCaseName(const std::string &case_name);

using flow_host_runtime_detail::BuildBridgeSampleId;
using flow_host_runtime_detail::BuildEnsmallenBoundaryNote;
using flow_host_runtime_detail::BuildEnsmallenBucketCoverage;
using flow_host_runtime_detail::BuildEnsmallenBatchLabel;
using flow_host_runtime_detail::BuildEnsmallenConclusionStatus;
using flow_host_runtime_detail::BuildEnsmallenComparisonMagnitude;
using flow_host_runtime_detail::BuildEnsmallenComparisonStatus;
using flow_host_runtime_detail::BuildEnsmallenCoverageGap;
using flow_host_runtime_detail::BuildEnsmallenDatasetBridgeTag;

std::string GetParentPath(const std::string &path)
{
  const size_t split = path.find_last_of("\\/");
  if (split == std::string::npos)
    return std::string();
  return path.substr(0, split);
}

std::string GetFilenameOnly(const std::string &path)
{
  const size_t split = path.find_last_of("\\/");
  if (split == std::string::npos)
    return path;
  return path.substr(split + 1);
}

bool StartsWithLiteral(const std::string &text, const char *prefix)
{
  const std::string prefix_text = prefix == nullptr ? std::string() : std::string(prefix);
  return text.size() >= prefix_text.size() &&
         text.compare(0, prefix_text.size(), prefix_text) == 0;
}

bool TryParseTorchRunTaskReportExpr(const std::string &rhs_expr,
                                    std::string &task_id)
{
  task_id.clear();
  static const char kPrefix[] = "host.run_task_report(";
  if (!StartsWithLiteral(rhs_expr, kPrefix) || rhs_expr.empty() ||
      rhs_expr[rhs_expr.size() - 1] != ')')
    return false;

  const size_t first_quote = rhs_expr.find('"');
  if (first_quote == std::string::npos)
    return false;
  const size_t second_quote = rhs_expr.find('"', first_quote + 1);
  if (second_quote == std::string::npos || second_quote <= first_quote + 1)
    return false;

  task_id = rhs_expr.substr(first_quote + 1, second_quote - first_quote - 1);
  return !task_id.empty();
}

std::string CurrentExecutableDirectory()
{
#ifdef _WIN32
  char buffer[MAX_PATH] = {};
  const DWORD length = ::GetModuleFileNameA(NULL, buffer, MAX_PATH);
  if (length == 0 || length >= MAX_PATH)
    return std::string();
  return GetParentPath(std::string(buffer, buffer + length));
#else
  return std::string();
#endif
}

std::string WorkspaceRootDirectory()
{
#ifdef CXPARSER_WORKSPACE_ROOT
  return std::string(CXPARSER_WORKSPACE_ROOT);
#else
  return std::string();
#endif
}

std::string ResolveTorchScenarioHelperPath(std::string &availability_status)
{
  availability_status.clear();

  const std::string explicit_helper =
    detail::ResolveOptionalEnvValueBridgeHelper("CXPARSER_TORCH_SCENARIO_HELPER_EXE");
  if (!explicit_helper.empty())
  {
    if (detail::PathExistsBridgeHelper(explicit_helper))
    {
      availability_status = "env_override";
      return explicit_helper;
    }

    availability_status = "env_override_missing";
    return std::string();
  }

  const std::string exe_dir = CurrentExecutableDirectory();
  const std::string workspace_root = WorkspaceRootDirectory();

  if (!exe_dir.empty())
  {
    const std::string scoped_libtorchsegmentation_helper =
      exe_dir + "\\libtorchsegmentation_demo.exe";
    if (detail::PathExistsBridgeHelper(scoped_libtorchsegmentation_helper))
    {
      availability_status = "same_dir_libtorchsegmentation_helper_present";
      return scoped_libtorchsegmentation_helper;
    }

    const std::string scoped_legacy_helper = exe_dir + "\\libtorch_module_demo.exe";
    if (detail::PathExistsBridgeHelper(scoped_legacy_helper))
    {
      availability_status = "same_dir_legacy_helper_present";
      return scoped_legacy_helper;
    }
  }

  if (!workspace_root.empty())
  {
    const std::string workspace_libtorchsegmentation_helper =
      workspace_root +
      "\\cxparser\\build\\Release\\libtorchsegmentation_demo.exe";
    if (detail::PathExistsBridgeHelper(workspace_libtorchsegmentation_helper))
    {
      availability_status = "workspace_libtorchsegmentation_helper";
      return workspace_libtorchsegmentation_helper;
    }

    const std::string workspace_legacy_helper =
      workspace_root + "\\libtorch_module\\build\\Release\\libtorch_module_demo.exe";
    if (detail::PathExistsBridgeHelper(workspace_legacy_helper))
    {
      availability_status = "workspace_legacy_helper_fallback";
      return workspace_legacy_helper;
    }
  }

  if (!exe_dir.empty())
  {
    const std::string monolithic_helper = exe_dir + "\\LibTorchTest.exe";
    if (detail::PathExistsBridgeHelper(monolithic_helper))
      availability_status = "monolithic_helper_only";
    else if (workspace_root.empty())
      availability_status = "helper_missing";
    else
      availability_status = "workspace_helper_missing";

    return std::string();
  }

  availability_status = workspace_root.empty() ? "exe_dir_unknown" : "workspace_helper_missing";
  return std::string();
}

bool TryParseUnifiedReviewObjectLine(const std::string &line,
                                     std::map<std::string, std::string> &fields)
{
  fields.clear();
  const std::string marker = "unified review = ";
  const size_t marker_pos = line.find(marker);
  if (marker_pos == std::string::npos)
    return false;

  std::istringstream input(line.substr(marker_pos + marker.size()));
  std::string token;
  while (input >> token)
  {
    const size_t split = token.find('=');
    if (split == std::string::npos || split == 0)
      continue;
    const std::string key = token.substr(0, split);
    const std::string value = token.substr(split + 1);
    if (!key.empty())
      fields[key] = value;
  }

  return fields.find("object") != fields.end() && !fields["object"].empty();
}

std::string UnifiedReviewObjectResultName(const std::string &object_name)
{
  if (object_name == "UnifiedImageReviewRecord")
    return "review_image";
  if (object_name == "UnifiedTaskReviewBundle")
    return "review_task";
  if (object_name == "UnifiedCompareSlice")
    return "review_compare";
  if (object_name == "UnifiedAnomalyFocusBundle")
    return "review_anomaly";
  return std::string();
}

std::string GetUnifiedReviewField(const std::map<std::string, std::string> &fields,
                                  const std::string &key)
{
  const std::map<std::string, std::string>::const_iterator found = fields.find(key);
  return found == fields.end() ? std::string() : found->second;
}

bool ShouldPromoteUnifiedReviewField(const std::string &field_name)
{
  return field_name == "primary_visual_ref" ||
         field_name == "visualization_refs" ||
         field_name == "status" ||
         field_name == "metrics" ||
         field_name == "metric_summary" ||
         field_name == "anomaly_flags" ||
         field_name == "baseline_refs" ||
         field_name == "sequence_family" ||
         field_name == "sequence_stage" ||
         field_name == "sequence_index" ||
         field_name == "upstream_ref" ||
         field_name == "downstream_ref" ||
         field_name == "attach_back_ref" ||
         field_name == "stage_transition_status" ||
         field_name == "sequence_trace_ref" ||
         field_name == "runtime_status" ||
         field_name == "gui_visual_refs" ||
         field_name == "bbox_overlay_ref" ||
         field_name == "roi_crop_ref" ||
         field_name == "template_alignment_ref" ||
         field_name == "roi_diff_ref" ||
         field_name == "single_image_conclusion" ||
         field_name == "roi_stage_conclusion" ||
         field_name == "template_diff_conclusion" ||
         field_name == "next_action" ||
         field_name == "issue_focus_image" ||
         field_name == "issue_focus_element" ||
         field_name == "issue_focus_chain" ||
         field_name == "issue_focus_stage" ||
         field_name == "issue_likely_problem" ||
         field_name == "script_module_ref" ||
         field_name == "script_action_entry" ||
         field_name == "parameter_injection_entry" ||
         field_name == "stage_execution_entry" ||
         field_name == "result_object_ref" ||
         field_name == "replay_ref" ||
         field_name == "operation_select_targets" ||
         field_name == "operation_modify_targets" ||
         field_name == "operation_run_targets" ||
         field_name == "operation_view_targets" ||
         field_name == "operation_judge_targets" ||
         field_name == "operation_record_targets" ||
         field_name == "semantic_operation_summary" ||
         field_name == "elements" ||
         field_name == "element_summary" ||
         field_name == "element_chains" ||
         field_name == "element_chain_summary" ||
         field_name == "element_status_summary" ||
         field_name == "element_findings" ||
         field_name == "element_level_focus" ||
         field_name == "stage_summary" ||
         field_name == "current_conclusion" ||
         field_name == "next_attention_points" ||
         field_name == "stage_transition_summary" ||
         field_name == "compare_dimensions" ||
         field_name == "delta_summary" ||
         field_name == "risk_level" ||
         field_name == "focus_recommendation" ||
         field_name == "anomaly_type_summary" ||
         field_name == "top_focus_objects";
}

void ApplyTorchUnifiedReviewHelperObjects(const TorchScenarioHelperRun &helper_run,
                                          CxScriptExecutionResult &result,
                                          bool append_detail)
{
  if (helper_run.unified_review_objects.empty())
    return;

  AddNamedResultObject(result,
                       "torch_unified_review_helper",
                       "helper",
                       "LibtorchSegmentationUnifiedReviewProducer",
                       helper_run.exit_code == 0 ? "ready" : "partial",
                       std::string());
  AddNamedResultField(result,
                      "torch_unified_review_helper",
                      "helper",
                      "helper_path",
                      helper_run.helper_path);
  AddNamedResultField(result,
                      "torch_unified_review_helper",
                      "helper",
                      "log_path",
                      helper_run.log_path);
  AddNamedResultField(result,
                      "torch_unified_review_helper",
                      "helper",
                      "object_count",
                      std::to_string(helper_run.unified_review_objects.size()));

  for (size_t i = 0; i < helper_run.unified_review_objects.size(); ++i)
  {
    const std::map<std::string, std::string> &fields =
      helper_run.unified_review_objects[i];
    const std::string object_name = GetUnifiedReviewField(fields, "object");
    const std::string result_name = UnifiedReviewObjectResultName(object_name);
    if (result_name.empty())
      continue;

    const std::string status =
      !GetUnifiedReviewField(fields, "status").empty()
        ? GetUnifiedReviewField(fields, "status")
        : (!GetUnifiedReviewField(fields, "risk_level").empty()
             ? GetUnifiedReviewField(fields, "risk_level")
             : std::string("ready"));
    AddNamedResultObject(result,
                         result_name,
                         "review",
                         object_name,
                         status,
                         std::string());

    for (std::map<std::string, std::string>::const_iterator it = fields.begin();
         it != fields.end();
         ++it)
    {
      if (it->first == "object")
        continue;
      AddNamedResultField(result,
                          "torch_unified_review_helper",
                          "helper",
                          result_name + "." + it->first,
                          it->second);
      if (ShouldPromoteUnifiedReviewField(it->first))
        AddNamedResultField(result, result_name, "review", it->first, it->second);
    }

    const std::string sequence_family = GetUnifiedReviewField(fields, "sequence_family");
    const std::string sequence_stage = GetUnifiedReviewField(fields, "sequence_stage");
    const std::string sequence_index = GetUnifiedReviewField(fields, "sequence_index");
    const std::string sequence_trace_ref = GetUnifiedReviewField(fields, "sequence_trace_ref");
    if (object_name == "UnifiedImageReviewRecord")
    {
      if (!sequence_family.empty())
      {
        if (append_detail)
          result.details.push_back("[TORCH_UNIFIED_REVIEW] object=" + object_name +
                                   " sequence_family=" + sequence_family +
                                   " sequence_stage=" + sequence_stage +
                                   " sequence_index=" + sequence_index);
      }
      const std::string input_image_ref = GetUnifiedReviewField(fields, "input_image_ref");
      const std::string primary_visual_ref = GetUnifiedReviewField(fields, "primary_visual_ref");
      const std::string runtime_status = GetUnifiedReviewField(fields, "runtime_status");
      if (result.input_sample.empty() && !input_image_ref.empty())
        result.input_sample = input_image_ref;
      if (result.dataset_ref.empty() && !input_image_ref.empty())
        result.dataset_ref = input_image_ref;
      if (result.published_primary_ref.empty() && !primary_visual_ref.empty())
        result.published_primary_ref = primary_visual_ref;
      if (!helper_run.input_image_path.empty())
        AddNamedResultField(result,
                            "review_image",
                            "review",
                            "input_image_ref",
                            helper_run.input_image_path);
      if (!helper_run.output_image_path.empty())
      {
        AddNamedResultField(result,
                            "review_image",
                            "review",
                            "primary_visual_ref",
                            helper_run.output_image_path);
        AddNamedResultField(result,
                            "review_image",
                            "review",
                            "visualization_refs",
                            helper_run.output_image_path);
      }
      if (!helper_run.input_image_path.empty() || !helper_run.output_image_path.empty())
      {
        std::string gui_visual_refs;
        if (!helper_run.input_image_path.empty())
          gui_visual_refs = helper_run.input_image_path;
        if (!helper_run.output_image_path.empty())
        {
          if (!gui_visual_refs.empty())
            gui_visual_refs += "|";
          gui_visual_refs += helper_run.output_image_path;
        }
        AddNamedResultField(result,
                            "review_image",
                            "review",
                            "gui_visual_refs",
                            gui_visual_refs);
      }
      if (!runtime_status.empty())
        AddNamedResultField(result,
                            "review_image",
                            "review",
                            "runtime_status",
                            runtime_status);
      if (!sequence_trace_ref.empty())
        AddNamedResultField(result,
                            "sequence_link_trace",
                            "phase0",
                            "producer_sequence_trace_ref",
                            sequence_trace_ref);
    }
  }
}

void ReadTorchScenarioHelperSignals(const std::string &log_path,
                                    TorchScenarioHelperRun &helper_run)
{
  if (log_path.empty())
    return;

  std::ifstream input(log_path.c_str());
  if (!input)
    return;

  std::string line;
  while (std::getline(input, line))
  {
    std::map<std::string, std::string> unified_review_fields;
    if (TryParseUnifiedReviewObjectLine(line, unified_review_fields))
    {
      helper_run.unified_review_objects.push_back(unified_review_fields);
      continue;
    }
    if (StartsWithLiteral(line, "ATTACH_BACK_OVERLAY_STATUS="))
    {
      helper_run.overlay_status =
        line.substr(std::string("ATTACH_BACK_OVERLAY_STATUS=").size());
      continue;
    }
    if (StartsWithLiteral(line, "ATTACH_BACK_TOP1_CLASS="))
    {
      helper_run.top1_class =
        line.substr(std::string("ATTACH_BACK_TOP1_CLASS=").size());
      continue;
    }
    if (StartsWithLiteral(line, "ATTACH_BACK_CONFIDENCE="))
    {
      helper_run.confidence =
        line.substr(std::string("ATTACH_BACK_CONFIDENCE=").size());
      continue;
    }
    if (StartsWithLiteral(line, "ATTACH_BACK_INPUT_IMAGE="))
    {
      helper_run.input_image_path =
        line.substr(std::string("ATTACH_BACK_INPUT_IMAGE=").size());
      continue;
    }
    if (StartsWithLiteral(line, "ATTACH_BACK_OUTPUT_IMAGE="))
    {
      helper_run.output_image_path =
        line.substr(std::string("ATTACH_BACK_OUTPUT_IMAGE=").size());
      continue;
    }
    if (StartsWithLiteral(line, "ATTACH_BACK_META_PATH="))
    {
      helper_run.output_meta_path =
        line.substr(std::string("ATTACH_BACK_META_PATH=").size());
      continue;
    }
    if (StartsWithLiteral(line, "ACTUAL_DEVICE="))
    {
      helper_run.actual_device =
        line.substr(std::string("ACTUAL_DEVICE=").size());
      continue;
    }
    if (StartsWithLiteral(line, "REVIEW_VISUAL_STATUS="))
    {
      helper_run.visual_status =
        line.substr(std::string("REVIEW_VISUAL_STATUS=").size());
      continue;
    }
    if (StartsWithLiteral(line, "REVIEW_INPUT_IMAGE="))
    {
      helper_run.input_image_path =
        line.substr(std::string("REVIEW_INPUT_IMAGE=").size());
      continue;
    }
    if (StartsWithLiteral(line, "REVIEW_OUTPUT_IMAGE="))
    {
      helper_run.output_image_path =
        line.substr(std::string("REVIEW_OUTPUT_IMAGE=").size());
      continue;
    }
    if (StartsWithLiteral(line, "REVIEW_OUTPUT_META="))
    {
      helper_run.output_meta_path =
        line.substr(std::string("REVIEW_OUTPUT_META=").size());
      continue;
    }
  }
}

bool TryRunTorchScenarioHelperTask(const std::string &task_id,
                                   const std::string &requested_device,
                                   const std::string &log_path,
                                   TorchScenarioHelperRun &helper_run,
                                   TorchExecutionProfileBridge *execution_bridge = nullptr)
{
  helper_run = TorchScenarioHelperRun();
  helper_run.log_path = log_path;
  helper_run.helper_path = ResolveTorchScenarioHelperPath(helper_run.status);
  helper_run.available = !helper_run.helper_path.empty();
  if (!helper_run.available)
    return false;

#ifndef _WIN32
  helper_run.status = "unsupported_platform";
  return false;
#else
  helper_run.launched = true;

  SECURITY_ATTRIBUTES security_attributes;
  security_attributes.nLength = sizeof(security_attributes);
  security_attributes.lpSecurityDescriptor = NULL;
  security_attributes.bInheritHandle = TRUE;

  HANDLE log_handle = ::CreateFileA(log_path.c_str(),
                                    GENERIC_WRITE,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    &security_attributes,
                                    CREATE_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL,
                                    NULL);
  if (log_handle == INVALID_HANDLE_VALUE)
  {
    helper_run.status = "log_create_failed";
    return false;
  }

  STARTUPINFOA startup_info;
  ::ZeroMemory(&startup_info, sizeof(startup_info));
  startup_info.cb = sizeof(startup_info);
  startup_info.dwFlags = STARTF_USESTDHANDLES;
  startup_info.hStdOutput = log_handle;
  startup_info.hStdError = log_handle;
  startup_info.hStdInput = ::GetStdHandle(STD_INPUT_HANDLE);

  PROCESS_INFORMATION process_info;
  ::ZeroMemory(&process_info, sizeof(process_info));

  std::string command_line =
    "\"" + helper_run.helper_path + "\" --task " + task_id + " --device " +
    (requested_device.empty() ? std::string("auto") : requested_device);
  if (GetFilenameOnly(helper_run.helper_path) == "libtorchsegmentation_demo.exe")
    command_line += " --unified-review";
  std::vector<char> mutable_command(command_line.begin(), command_line.end());
  mutable_command.push_back('\0');

  std::vector<char> env_block;
  char *environment = NULL;
  {
    LPCH base_environment = ::GetEnvironmentStringsA();
    if (base_environment != NULL)
    {
      for (LPCH cursor = base_environment; *cursor != '\0'; cursor += std::strlen(cursor) + 1)
      {
        const std::string entry(cursor);
        if (StartsWithLiteral(entry, "LIBTORCH_MODULE_USE_CUDA=") ||
            StartsWithLiteral(entry, "PATH="))
          continue;
        env_block.insert(env_block.end(), entry.begin(), entry.end());
        env_block.push_back('\0');
      }
      ::FreeEnvironmentStringsA(base_environment);
    }

    std::vector<std::string> path_items;
    path_items.push_back(GetParentPath(helper_run.helper_path));
    if (!CurrentExecutableDirectory().empty())
      path_items.push_back(CurrentExecutableDirectory());
    path_items.push_back("D:\\libtorch\\lib");
    path_items.push_back("D:\\opencv\\build\\x64\\vc17\\bin");
    path_items.push_back("D:\\opencv\\build\\x64\\vc16\\bin");
    path_items.push_back("D:\\opencv\\build\\bin\\Release");
    path_items.push_back("D:\\opencv\\build\\bin");

    const std::string workspace_root = WorkspaceRootDirectory();
    if (!workspace_root.empty())
      path_items.push_back(workspace_root + "\\libtorch_module\\third_party_shims\\NvToolsExt");

    const char *existing_path = std::getenv("PATH");
    if (existing_path != NULL && existing_path[0] != '\0')
      path_items.push_back(existing_path);

    std::string path_value;
    for (size_t i = 0; i < path_items.size(); ++i)
    {
      if (path_items[i].empty())
        continue;
      if (!path_value.empty())
        path_value += ";";
      path_value += path_items[i];
    }

    const std::string path_entry = "PATH=" + path_value;
    env_block.insert(env_block.end(), path_entry.begin(), path_entry.end());
    env_block.push_back('\0');

    const std::string override_entry =
      std::string("LIBTORCH_MODULE_USE_CUDA=") +
      ((requested_device == "cpu") ? "0" :
       (requested_device == "gpu") ? "1" : "1");
    env_block.insert(env_block.end(), override_entry.begin(), override_entry.end());
    env_block.push_back('\0');

    const auto append_env_entry =
      [&env_block](const char *name, const std::string &value)
      {
        if (name == NULL || value.empty())
          return;
        const std::string entry = std::string(name) + "=" + value;
        env_block.insert(env_block.end(), entry.begin(), entry.end());
        env_block.push_back('\0');
      };

    if (!workspace_root.empty() && task_id == "torch.infer.mobilevit.unified")
    {
      const std::string input_path =
        (execution_bridge != nullptr &&
         !execution_bridge->manifest_image_ref.empty())
          ? execution_bridge->manifest_image_ref
          : workspace_root +
              "\\local_test\\torch_main_thread\\directional_selection\\D2_local_texture_tolerance\\defect\\cell2105.png";
      const std::string output_path =
        (execution_bridge != nullptr &&
         !execution_bridge->manifest_output_ref.empty())
          ? execution_bridge->manifest_output_ref
          : workspace_root +
              "\\docs\\notes\\tmp\\mobilevit_unified_review\\cell2105.mobilevit_roi.jpg";
      append_env_entry("LIBTORCH_MODULE_MOBILEVIT_INFER_IMAGE", input_path);
      append_env_entry("LIBTORCH_MODULE_MOBILEVIT_INFER_OUTPUT", output_path);
    }
    if (!workspace_root.empty() && task_id == "torch.infer.deeplab.unified")
    {
      const std::string template_path =
        (execution_bridge != nullptr &&
         !execution_bridge->manifest_template_image_ref.empty())
          ? execution_bridge->manifest_template_image_ref
          : workspace_root +
              "\\local_test\\torch_main_thread\\directional_selection\\D3_repeat_region_precision\\deeppcb_pairs\\template\\20085291.jpg";
      const std::string test_path =
        (execution_bridge != nullptr &&
         !execution_bridge->manifest_test_image_ref.empty())
          ? execution_bridge->manifest_test_image_ref
          : (execution_bridge != nullptr &&
             !execution_bridge->manifest_image_ref.empty())
              ? execution_bridge->manifest_image_ref
              : workspace_root +
                  "\\local_test\\torch_main_thread\\directional_selection\\D3_repeat_region_precision\\deeppcb_pairs\\test\\20085291.jpg";
      const std::string output_path =
        (execution_bridge != nullptr &&
         !execution_bridge->manifest_output_ref.empty())
          ? execution_bridge->manifest_output_ref
          : workspace_root +
              "\\docs\\notes\\tmp\\deeplab_unified_review\\20085291.diff_overlay.jpg";
      append_env_entry("LIBTORCH_MODULE_DEEPLAB_TEMPLATE_IMAGE", template_path);
      append_env_entry("LIBTORCH_MODULE_DEEPLAB_TEST_IMAGE", test_path);
      append_env_entry("LIBTORCH_MODULE_DEEPLAB_OUTPUT", output_path);
    }
    env_block.push_back('\0');
    environment = env_block.empty() ? NULL : &env_block[0];
  }

  const std::string working_directory = GetParentPath(helper_run.helper_path);
  const auto begin_time = std::chrono::steady_clock::now();
  const BOOL created =
    ::CreateProcessA(NULL,
                     &mutable_command[0],
                     NULL,
                     NULL,
                     TRUE,
                     CREATE_NO_WINDOW,
                     environment,
                     working_directory.empty() ? NULL : working_directory.c_str(),
                     &startup_info,
                     &process_info);

  if (!created)
  {
    helper_run.status = "launch_failed";
    ::CloseHandle(log_handle);
    return false;
  }

  const DWORD wait_code = ::WaitForSingleObject(process_info.hProcess, 600000);
  helper_run.finished = (wait_code == WAIT_OBJECT_0);
  if (!helper_run.finished)
  {
    helper_run.status = wait_code == WAIT_TIMEOUT ? "timeout" : "wait_failed";
    ::TerminateProcess(process_info.hProcess, 124);
    helper_run.exit_code = 124;
  }
  else
  {
    DWORD exit_code = 0;
    if (::GetExitCodeProcess(process_info.hProcess, &exit_code))
      helper_run.exit_code = static_cast<int>(exit_code);
    helper_run.status = helper_run.exit_code == 0 ? "helper_exit_0" : "helper_exit_nonzero";
  }

  const auto end_time = std::chrono::steady_clock::now();
  helper_run.runtime_ms =
    static_cast<long long>(
      std::chrono::duration_cast<std::chrono::milliseconds>(end_time - begin_time).count());

  ::CloseHandle(process_info.hThread);
  ::CloseHandle(process_info.hProcess);
  ::CloseHandle(log_handle);

  ReadTorchScenarioHelperSignals(log_path, helper_run);
  return helper_run.finished && helper_run.exit_code == 0;
#endif
}
using flow_host_runtime_detail::BuildEnsmallenExpansionGate;
using flow_host_runtime_detail::BuildEnsmallenImageSelectionGuide;
using flow_host_runtime_detail::BuildEnsmallenInteractionRoute;
using flow_host_runtime_detail::BuildEnsmallenLikelyIssueClass;
using flow_host_runtime_detail::BuildEnsmallenMcpFlow;
using flow_host_runtime_detail::BuildEnsmallenNextBucketFocus;
using flow_host_runtime_detail::BuildEnsmallenObservationMode;
using flow_host_runtime_detail::BuildEnsmallenObservationPriority;
using flow_host_runtime_detail::BuildEnsmallenPrimaryReviewRef;
using flow_host_runtime_detail::BuildEnsmallenRecommendedAction;
using flow_host_runtime_detail::BuildEnsmallenReviewScope;
using flow_host_runtime_detail::BuildEnsmallenRiskAxis;
using flow_host_runtime_detail::BuildEnsmallenTestFlowGuide;
using flow_host_runtime_detail::ClassifyEnsmallenInputSampleBucket;
using flow_host_runtime_detail::CollectFlowHostInputs;
using flow_host_runtime_detail::FindAssignmentValue;
using flow_host_runtime_detail::JoinStrings;
using flow_host_runtime_detail::NormalizeEnsmallenDatasetAlias;
using flow_host_runtime_detail::NormalizeFlowHostCaseName;
using flow_host_runtime_detail::SummarizeEnsmallenInputBuckets;
using flow_host_runtime_detail::TryParseFlowHostCall;
using flow_host_runtime_detail::TryParseLegacyFlowCall;
using runtime_result_detail::BindVariableExecutionIds;
using runtime_result_detail::RefreshExecutionDiagnostics;
using runtime_result_detail::RefreshExecutionSummary;
using runtime_result_detail::ReleaseLightweightDebugArtifacts;
using runtime_statement_detail::SetStatementError;
using runtime_statement_detail::StatementActionLabel;
using runtime_statement_detail::StatementControlTag;
using runtime_statement_detail::StatementKindName;
using runtime_statement_detail::StatementOpcode;
using runtime_statement_detail::StatementStepKind;

std::string ToLowerText(const std::string &text);
bool IsCximageClassicalReviewCase(const CxScriptExecutionResult &result);
std::string NormalizeLoweredPathForTokenMatch(const std::string &text);
bool ContainsLoweredPathToken(const std::string &lowered_text,
                              const char *windows_token,
                              const char *unix_token);
std::string ResolveRuntimeDatasetBridgeTag(const CxScriptExecutionResult &result);

std::vector<std::string> SplitCallArguments(const std::string &text)
{
  std::vector<std::string> args;
  std::string current;
  bool in_quotes = false;

  for (size_t i = 0; i < text.size(); ++i)
  {
    const char ch = text[i];
    if (ch == '"')
    {
      in_quotes = !in_quotes;
      current.push_back(ch);
      continue;
    }

    if (ch == ',' && !in_quotes)
    {
      args.push_back(Trim(current));
      current.clear();
      continue;
    }

    current.push_back(ch);
  }

  if (!current.empty())
    args.push_back(Trim(current));
  return args;
}

std::string AppendKeyValueAssignment(const std::string &text,
                                     const std::string &key,
                                     const std::string &value)
{
  if (key.empty() || value.empty())
    return text;
  if (!FindAssignmentValue(text, key).empty())
    return text;

  if (text.empty())
    return key + "=" + value;
  return text + ";" + key + "=" + value;
}

std::string BuildSampleIdFromPath(const std::string &path_text)
{
  const std::string filename = detail::FilenameOnlyBridgeHelper(path_text);
  if (filename.empty())
    return std::string();

  const size_t dot = filename.find_last_of('.');
  if (dot == std::string::npos)
    return filename;
  return filename.substr(0, dot);
}

std::string ResolveRuntimeDatasetBridgeTag(const CxScriptExecutionResult &result)
{
  const std::string dataset_bridge = BuildEnsmallenDatasetBridgeTag(result);
  if (dataset_bridge != "bridge.unknown_dataset")
    return dataset_bridge;

  const std::string input_image_lower =
    ToLowerText(FindAssignmentValue(result.input_artifacts, "input_image"));
  const std::string template_image_lower =
    ToLowerText(FindAssignmentValue(result.input_artifacts, "template_image"));

  if (ContainsLoweredPathToken(input_image_lower,
                               "local_test\\cximage_main_thread\\real_industrial_selection_v4_natural_only",
                               "local_test/cximage_main_thread/real_industrial_selection_v4_natural_only") ||
      ContainsLoweredPathToken(template_image_lower,
                               "local_test\\cximage_main_thread\\real_industrial_selection_v4_natural_only",
                               "local_test/cximage_main_thread/real_industrial_selection_v4_natural_only"))
  {
    return "bridge.cximage_natural_only";
  }

  if (ContainsLoweredPathToken(input_image_lower,
                               "local_test\\halcon_2605_thread_selection",
                               "local_test/halcon_2605_thread_selection") ||
      ContainsLoweredPathToken(input_image_lower,
                               "local_test\\halcon_2605_texture_region_selection",
                               "local_test/halcon_2605_texture_region_selection") ||
      ContainsLoweredPathToken(template_image_lower,
                               "local_test\\halcon_2605_thread_selection",
                               "local_test/halcon_2605_thread_selection") ||
      ContainsLoweredPathToken(template_image_lower,
                               "local_test\\halcon_2605_texture_region_selection",
                               "local_test/halcon_2605_texture_region_selection"))
  {
    return "bridge.halcon_2605_thread_selection";
  }

  if (!IsCximageClassicalReviewCase(result))
    return dataset_bridge;

  return dataset_bridge;
}

std::string ResolveLegacyExplicitScriptAliasPath(const std::string &script_path)
{
  if (script_path.empty())
    return std::string();

  std::string normalized = script_path;
  for (size_t i = 0; i < normalized.size(); ++i)
  {
    if (normalized[i] == '\\')
      normalized[i] = '/';
  }

  if (normalized.size() < 5 || normalized.substr(normalized.size() - 4) != ".cxs")
    return std::string();

  const size_t slash = normalized.find_last_of('/');
  const std::string parent = slash == std::string::npos ? std::string() : normalized.substr(0, slash);
  const std::string file_name = slash == std::string::npos ? normalized : normalized.substr(slash + 1);
  const std::string stem = file_name.substr(0, file_name.size() - 4);

  const size_t dot = stem.find('.');
  if (dot == std::string::npos || dot == 0 || dot + 1 >= stem.size())
    return std::string();

  const std::string layer = stem.substr(0, dot);
  const std::string case_id = stem.substr(dot + 1);
  if (layer.empty() || case_id.empty())
    return std::string();

  return parent + "/" + layer + "/" + case_id + ".cxscript";
}

bool TryInferExpectedScriptIdentity(const std::string &script_path,
                                    std::string &expected_module,
                                    std::string &expected_layer,
                                    std::string &expected_case_name)
{
  expected_module.clear();
  expected_layer.clear();
  expected_case_name.clear();
  if (script_path.empty())
    return false;

  std::string normalized = script_path;
  for (size_t i = 0; i < normalized.size(); ++i)
  {
    if (normalized[i] == '\\')
      normalized[i] = '/';
  }

  std::vector<std::string> tokens;
  std::string current;
  for (size_t i = 0; i < normalized.size(); ++i)
  {
    const char ch = normalized[i];
    if (ch == '/')
    {
      if (!current.empty())
      {
        tokens.push_back(current);
        current.clear();
      }
      continue;
    }
    current.push_back(ch);
  }
  if (!current.empty())
    tokens.push_back(current);

  for (size_t i = 0; i + 1 < tokens.size(); ++i)
  {
    if (tokens[i] == "module")
    {
      expected_module = tokens[i + 1];
      break;
    }
  }

  const size_t slash = normalized.find_last_of('/');
  const std::string parent = slash == std::string::npos ? std::string() : normalized.substr(0, slash);
  const std::string file_name = slash == std::string::npos ? normalized : normalized.substr(slash + 1);

  if (file_name.size() > 4 && file_name.substr(file_name.size() - 4) == ".cxs")
  {
    const std::string stem = file_name.substr(0, file_name.size() - 4);
    const size_t dot = stem.find('.');
    if (dot == std::string::npos || dot == 0 || dot + 1 >= stem.size())
      return false;
    expected_layer = stem.substr(0, dot);
    expected_case_name = stem.substr(dot + 1);
    return !expected_layer.empty() && !expected_case_name.empty();
  }

  if (file_name.size() > 9 && file_name.substr(file_name.size() - 9) == ".cxscript")
  {
    expected_case_name = file_name.substr(0, file_name.size() - 9);
    for (size_t i = 0; i + 1 < tokens.size(); ++i)
    {
      if (tokens[i] == "integration")
      {
        expected_module = tokens[i + 1];
        expected_layer.clear();
        return !expected_module.empty() && !expected_case_name.empty();
      }
    }

    const size_t parent_slash = parent.find_last_of('/');
    if (parent_slash == std::string::npos)
      return false;
    expected_layer = parent.substr(parent_slash + 1);
    return !expected_layer.empty() && !expected_case_name.empty();
  }

  return false;
}

bool IsAiTaskEnvelopeContractCaseName(const std::string &case_name)
{
  return case_name == "ai_task_packaging" ||
         case_name == "ai_route_ready_combo" ||
         case_name == "bridge_flow_suite" ||
         case_name == "feature_stage1_gate_suite" ||
         case_name == "feature_execution_ladder" ||
         case_name == "core4_feature_suite";
}

bool IsEnsmallenPublicLayer(const std::string &layer)
{
  return layer == "feature" ||
         layer == "scenario" ||
         layer == "train" ||
         layer == "infer";
}

bool ContainsEnsmallenHint(const std::string &text)
{
  return text.find("ensmallen") != std::string::npos ||
         text.find("phase1_param_") != std::string::npos ||
         text.find("geometry_fit_tuning") != std::string::npos ||
         text.find("match_score_tuning") != std::string::npos ||
         text.find("circle_param_opt") != std::string::npos ||
         text.find("ellipse_param_opt") != std::string::npos ||
         text.find("match_score_opt") != std::string::npos;
}

bool IsEnsmallenPublicResult(const CxScriptExecutionResult &result)
{
  if (!IsEnsmallenPublicLayer(result.layer))
    return false;

  if (result.module == "ensmallen_layer")
    return true;

  return ContainsEnsmallenHint(result.case_name) ||
         ContainsEnsmallenHint(result.task_id) ||
         ContainsEnsmallenHint(result.summary) ||
         result.result_object == "EnsmallenFlowHostResult";
}

bool StartsWith(const std::string &text, const char *prefix)
{
  const std::string p = prefix;
  return text.size() >= p.size() && text.substr(0, p.size()) == p;
}

std::string Trim(const std::string &text)
{
  size_t begin = 0;
  while (begin < text.size() &&
         (text[begin] == ' ' || text[begin] == '\t' || text[begin] == '\r' || text[begin] == '\n'))
    ++begin;

  size_t end = text.size();
  while (end > begin &&
         (text[end - 1] == ' ' || text[end - 1] == '\t' || text[end - 1] == '\r' || text[end - 1] == '\n'))
    --end;

  return text.substr(begin, end - begin);
}

bool ParseBool(const std::string &value)
{
  return value == "on" || value == "true" || value == "1";
}

int CountChar(const std::string &text, char ch)
{
  int count = 0;
  for (size_t i = 0; i < text.size(); ++i)
  {
    if (text[i] == ch)
      ++count;
  }
  return count;
}

std::string StripQuotes(const std::string &value)
{
  if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
    return value.substr(1, value.size() - 2);
  return value;
}

std::string StripTrailingSemicolon(const std::string &value)
{
  std::string text = Trim(value);
  if (!text.empty() && text[text.size() - 1] == ';')
    text = Trim(text.substr(0, text.size() - 1));
  return text;
}

bool TryParseReadResultExpression(const std::string &text, std::string &path)
{
  path.clear();
  const std::string normalized = StripTrailingSemicolon(Trim(text));
  if (!StartsWith(normalized, "readresult("))
    return false;

  const std::string args = ExtractCallArguments(normalized);
  if (args.empty())
    return false;

  path = StripQuotes(Trim(args));
  return !path.empty();
}

bool TryParseCheckMethodCall(const std::string &text,
                             std::string &method_name,
                             std::vector<std::string> &args)
{
  method_name.clear();
  args.clear();

  std::string normalized = StripTrailingSemicolon(Trim(text));
  if (StartsWith(normalized, "require "))
    normalized = Trim(normalized.substr(8));
  if (StartsWith(normalized, "Action.") ||
      StartsWith(normalized, "Input.") ||
      StartsWith(normalized, "Output.") ||
      StartsWith(normalized, "Flow."))
    return false;
  const size_t open = normalized.find('(');
  const size_t close = normalized.rfind(')');
  if (open == std::string::npos || close == std::string::npos || close <= open)
    return false;

  if (StartsWith(normalized, "Check."))
    method_name = Trim(normalized.substr(6, open - 6));
  else
    method_name = Trim(normalized.substr(0, open));
  if (method_name.empty())
    return false;

  if (method_name == "input_dataset" ||
      method_name == "input_split" ||
      method_name == "input_sample" ||
      method_name == "input_artifact" ||
      method_name == "input_param")
    return false;

  args = SplitCallArguments(normalized.substr(open + 1, close - open - 1));
  for (size_t i = 0; i < args.size(); ++i)
    args[i] = Trim(args[i]);
  return true;
}

std::string ExtractCallName(const std::string &call_text)
{
  const size_t pos = call_text.find('(');
  if (pos == std::string::npos)
    return Trim(call_text);
  return Trim(call_text.substr(0, pos));
}

std::string ExtractCallArguments(const std::string &call_text)
{
  const size_t begin = call_text.find('(');
  const size_t end = call_text.rfind(')');
  if (begin == std::string::npos || end == std::string::npos || end <= begin)
    return std::string();
  return Trim(call_text.substr(begin + 1, end - begin - 1));
}

std::string ExtractLegacySingleArgumentValue(const std::string &call_text)
{
  const std::vector<std::string> args = SplitCallArguments(ExtractCallArguments(call_text));
  if (args.empty())
    return std::string();
  return StripQuotes(Trim(args[0]));
}

bool LooksLikePlainCallStatement(const std::string &trimmed, std::string &call_text)
{
  call_text.clear();

  std::string text = StripTrailingSemicolon(trimmed);
  if (text.empty() || text == trimmed)
    return false;

  if (StartsWith(text, "if(") ||
      StartsWith(text, "if (") ||
      StartsWith(text, "for(") ||
      StartsWith(text, "for (") ||
      StartsWith(text, "while(") ||
      StartsWith(text, "while (") ||
      StartsWith(text, "switch(") ||
      StartsWith(text, "switch (") ||
      StartsWith(text, "check(") ||
      StartsWith(text, "print("))
    return false;

  const size_t begin = text.find('(');
  const size_t end = text.rfind(')');
  if (begin == std::string::npos || end != text.size() - 1 || end <= begin)
    return false;

  const std::string callee = Trim(text.substr(0, begin));
  if (callee.empty())
    return false;

  const char first = callee[0];
  if (!((first >= 'a' && first <= 'z') ||
        (first >= 'A' && first <= 'Z') ||
        first == '_'))
    return false;

  for (size_t i = 1; i < callee.size(); ++i)
  {
    const char ch = callee[i];
    if (!((ch >= 'a' && ch <= 'z') ||
          (ch >= 'A' && ch <= 'Z') ||
          (ch >= '0' && ch <= '9') ||
          ch == '_'))
      return false;
  }

  if (callee == "check" || callee == "print")
    return false;

  call_text = text;
  return true;
}

bool IsHeaderMetadataField(const std::string &field)
{
  return field == "kind" ||
         field == "layer" ||
         field == "module" ||
         field == "case" ||
         field == "case_id" ||
         field == "case_name" ||
         field == "mode" ||
         field == "report";
}

bool ApplyHeaderMetadataField(CxScriptExecutionContext &context,
                              const std::string &field,
                              const std::string &value)
{
  // Metadata header step: shared compatibility point for both key=value and name(...).
  // These lines update header context but are not normal call/action statements.
  const std::string normalized_value = StripQuotes(StripTrailingSemicolon(Trim(value)));

  if (field == "kind")
    context.kind = normalized_value;
  else if (field == "layer")
    context.layer = normalized_value;
  else if (field == "module")
    context.module = normalized_value;
  else if (field == "case" || field == "case_id" || field == "case_name")
    context.case_name = normalized_value;
  else if (field == "mode")
    context.mode = normalized_value;
  else if (field == "report")
    context.report_on = ParseBool(normalized_value);
  else
    return false;

  return true;
}

bool TryParseHeaderMetadataFunction(const std::string &trimmed,
                                    std::string &field,
                                    std::string &value)
{
  field.clear();
  value.clear();

  std::string text = StripTrailingSemicolon(trimmed);
  const size_t begin = text.find('(');
  const size_t end = text.rfind(')');
  if (begin == std::string::npos || end == std::string::npos || end != text.size() - 1 || end <= begin)
    return false;

  field = Trim(text.substr(0, begin));
  if (!IsHeaderMetadataField(field))
    return false;

  value = Trim(text.substr(begin + 1, end - begin - 1));
  value = StripQuotes(value);
  return !value.empty();
}

CxScriptStatement MakeStatement(CxScriptStmtKind kind,
                                const std::string &text,
                                const std::string &name,
                                const std::string &step_name,
                                int line_number,
                                int block_depth);

CxScriptStatement MakeHeaderMetadataStatement(const std::string &field,
                                              const std::string &value,
                                              const std::string &text,
                                              int line_number)
{
  CxScriptStatement stmt = MakeStatement(cxssk_header_metadata,
                                         text,
                                         field,
                                         "__header__",
                                         line_number,
                                         0);
  stmt.lhs_text = field;
  stmt.rhs_text = value;
  return stmt;
}

bool ParseExpectExpression(const std::string &text,
                           std::string &lhs_text,
                           std::string &operator_text,
                           std::string &rhs_text)
{
  lhs_text.clear();
  operator_text.clear();
  rhs_text.clear();

  const char *ops[] = {"==", "!=", ">=", "<=", ">", "<"};
  for (size_t i = 0; i < sizeof(ops) / sizeof(ops[0]); ++i)
  {
    const std::string op = ops[i];
    const size_t pos = text.find(op);
    if (pos == std::string::npos)
      continue;

    lhs_text = Trim(text.substr(0, pos));
    operator_text = op;
    rhs_text = Trim(text.substr(pos + op.size()));
    return !lhs_text.empty() && !rhs_text.empty();
  }

  return false;
}

void AddNamedResultObject(CxScriptExecutionResult &result,
                          const std::string &result_name,
                          const std::string &stage_name,
                          const std::string &object_name,
                          const std::string &status,
                          const std::string &failure_stage)
{
  for (size_t i = 0; i < result.named_results.size(); ++i)
  {
    if (result.named_results[i].result_name == result_name)
      return;
  }

  CxScriptNamedResultObject item;
  item.result_name = result_name;
  item.stage_name = stage_name;
  item.object_name = object_name;
  item.status = status;
  item.failure_stage = failure_stage;
  result.named_results.push_back(item);
}

void AddNamedResultField(CxScriptExecutionResult &result,
                         const std::string &result_name,
                         const std::string &stage_name,
                         const std::string &field_name,
                         const std::string &value)
{
  for (size_t i = 0; i < result.result_fields.size(); ++i)
  {
    if (result.result_fields[i].result_name == result_name &&
        result.result_fields[i].field_name == field_name)
    {
      result.result_fields[i].value = value;
      return;
    }
  }

  CxScriptNamedResultField item;
  item.result_name = result_name;
  item.stage_name = stage_name;
  item.field_name = field_name;
  item.value = value;
  result.result_fields.push_back(item);
}

std::string FindNamedResultFieldValue(const CxScriptExecutionResult &result,
                                      const char *result_name,
                                      const char *field_name)
{
  if (result_name == 0 || field_name == 0)
    return std::string();

  for (size_t i = 0; i < result.result_fields.size(); ++i)
  {
    const CxScriptNamedResultField &field = result.result_fields[i];
    if (field.result_name == result_name && field.field_name == field_name)
      return field.value;
  }

  return std::string();
}

void PushUniqueText(std::vector<std::string> &items, const std::string &value)
{
  if (value.empty())
    return;

  for (size_t i = 0; i < items.size(); ++i)
  {
    if (items[i] == value)
      return;
  }
  items.push_back(value);
}

std::string JoinTextItems(const std::vector<std::string> &items, const char *delimiter)
{
  const std::string joiner = delimiter ? delimiter : ";";
  std::string text;
  for (size_t i = 0; i < items.size(); ++i)
  {
    if (items[i].empty())
      continue;
    if (!text.empty())
      text += joiner;
    text += items[i];
  }
  return text;
}

std::string FindFirstNonEmptyText(const std::vector<std::string> &items)
{
  for (size_t i = 0; i < items.size(); ++i)
  {
    if (!items[i].empty())
      return items[i];
  }
  return std::string();
}

size_t CountDelimitedItems(const std::string &text, char delimiter)
{
  size_t count = 0;
  std::string current;
  for (size_t i = 0; i <= text.size(); ++i)
  {
    const char ch = i < text.size() ? text[i] : delimiter;
    if (ch == delimiter)
    {
      if (!Trim(current).empty())
        ++count;
      current.clear();
      continue;
    }
    current.push_back(ch);
  }
  return count;
}

std::string NormalizeReviewSourceThread(const CxScriptExecutionResult &result)
{
  if (result.module == "torch" || result.module == "torch_module")
    return "torch";
  if (result.module == "mlpack")
    return "mlpack";
  if (result.module == "ensmallen_layer")
    return "ensmallen";
  if (result.module == "cximage" || result.module == "cxcore")
    return "cximage";
  return result.module.empty() ? "unknown" : result.module;
}

bool RequiresUnifiedImageReviewRecord(const std::string &source_thread)
{
  return source_thread == "torch" ||
         source_thread == "mlpack" ||
         source_thread == "ensmallen" ||
         source_thread == "cximage";
}

std::string BuildEnsmallenSampleCountText(const CxScriptExecutionResult &result);
std::string BuildEnsmallenBestParamSetsText(const CxScriptExecutionResult &result);

void CollectUnifiedImageReviewMissingFields(const UnifiedImageReviewRecord &image_review,
                                            std::vector<std::string> &missing_fields)
{
  if (image_review.source_thread.empty() || image_review.source_thread == "unknown")
    PushUniqueText(missing_fields, "source_thread");
  if (image_review.task_id.empty())
    PushUniqueText(missing_fields, "task_id");
  if (image_review.case_name.empty())
    PushUniqueText(missing_fields, "case_name");
  if (image_review.stage.empty())
    PushUniqueText(missing_fields, "stage");
  if (image_review.image_id.empty() || image_review.image_id == "image.unknown")
    PushUniqueText(missing_fields, "image_id");
  if (image_review.input_image_ref.empty())
    PushUniqueText(missing_fields, "input_image_ref");
  if (image_review.primary_visual_ref.empty())
    PushUniqueText(missing_fields, "primary_visual_ref");
  if (image_review.detection_elements.empty())
    PushUniqueText(missing_fields, "detection_elements");
  if (image_review.element_chains.empty())
    PushUniqueText(missing_fields, "element_chains");
  if (image_review.primary_detection_semantic.empty())
    PushUniqueText(missing_fields, "primary_detection_semantic");
  if (image_review.refresh_mode.empty())
    PushUniqueText(missing_fields, "refresh_mode");
  if (image_review.changed_fields.empty())
    PushUniqueText(missing_fields, "changed_fields");
  if (image_review.refresh_priority.empty())
    PushUniqueText(missing_fields, "refresh_priority");
}

void PushUniqueDetectionElement(std::vector<UnifiedDetectionElement> &elements,
                                const UnifiedDetectionElement &element)
{
  if (element.element_id.empty() ||
      element.element_type.empty() ||
      element.semantic_role.empty())
    return;

  for (size_t i = 0; i < elements.size(); ++i)
  {
    if (elements[i].element_id == element.element_id)
      return;
  }

  elements.push_back(element);
}

std::string DetermineDetectionProvenance(const std::string &source_thread,
                                         const std::string &preferred)
{
  const std::string lowered_preferred = ToLowerText(preferred);
  if (lowered_preferred.find("edgesam") != std::string::npos)
    return "edgesam";
  if (!preferred.empty())
    return preferred;
  if (source_thread == "torch" || source_thread == "mlpack")
    return "model";
  if (source_thread == "cximage")
    return "manual";
  if (source_thread == "ensmallen")
    return "mixed";
  return "mixed";
}
std::string ResolveDetectionTemplateIdentity(const CxScriptExecutionResult &result)
{
  if (!result.geometry_template_specs.empty() &&
      !result.geometry_template_specs[0].template_identity.empty())
    return result.geometry_template_specs[0].template_identity;
  return FindNamedResultFieldValue(result, "geometry_template_spec", "template_identity");
}

std::string ResolveDetectionTemplateProvenance(const CxScriptExecutionResult &result)
{
  if (!result.geometry_template_specs.empty() &&
      !result.geometry_template_specs[0].template_provenance.empty())
    return result.geometry_template_specs[0].template_provenance;
  return FindNamedResultFieldValue(result, "geometry_template_spec", "template_provenance");
}

std::string ResolveDetectionExecutionMode(const CxScriptExecutionResult &result)
{
  if (!result.image_acquisition_specs.empty() &&
      !result.image_acquisition_specs[0].execution_mode.empty())
    return result.image_acquisition_specs[0].execution_mode;
  return FindNamedResultFieldValue(result, "image_acquisition_spec", "execution_mode");
}

std::string DetermineDetectionConsistencyStatus(const CxScriptExecutionResult &result,
                                                const std::string &element_ref,
                                                const std::string &fallback_status)
{
  std::string normalized_element_ref = ToLowerText(element_ref);
  if (IsEnsmallenPublicResult(result))
  {
    const std::string boundary_token = "boundary_error_ref";
    const std::string alignment_token = "alignment_error_ref";
    const size_t boundary_pos = normalized_element_ref.find(boundary_token);
    if (boundary_pos != std::string::npos)
      normalized_element_ref.replace(boundary_pos,
                                     boundary_token.size(),
                                     "boundary_metric_ref");
    const size_t alignment_pos = normalized_element_ref.find(alignment_token);
    if (alignment_pos != std::string::npos)
      normalized_element_ref.replace(alignment_pos,
                                     alignment_token.size(),
                                     "alignment_metric_ref");
  }

  const std::string alignment_status =
    ToLowerText(!result.published_template_test_alignment_status.empty()
                  ? result.published_template_test_alignment_status
                  : result.template_test_alignment_status);
  const std::string status_text =
    normalized_element_ref + " " +
    ToLowerText(fallback_status + " " + result.summary + " " +
                result.error_message + " " + result.failure_mode + " " + alignment_status);

  if (status_text.find("missing") != std::string::npos ||
      status_text.find("not_found") != std::string::npos)
    return "missing";
  if (status_text.find("drift") != std::string::npos ||
      status_text.find("delta") != std::string::npos)
    return "drifted";
  if (status_text.find("abnormal") != std::string::npos ||
      status_text.find("fail") != std::string::npos ||
      status_text.find("error") != std::string::npos)
    return "abnormal";
  if ((!result.roi_diff_candidate_count.empty() && result.roi_diff_candidate_count != "0") ||
      (!result.published_roi_diff_candidate_count.empty() &&
       result.published_roi_diff_candidate_count != "0"))
    return "drifted";
  if (!alignment_status.empty() &&
      (alignment_status.find("fail") != std::string::npos ||
       alignment_status.find("mismatch") != std::string::npos ||
       alignment_status.find("drift") != std::string::npos))
    return "drifted";
  return fallback_status.empty() ? std::string("matched") : fallback_status;
}

std::string DetermineDetectionConfidence(const std::string &semantic_role,
                                         const std::string &consistency_status)
{
  if (consistency_status == "missing")
    return "0.25";
  if (consistency_status == "abnormal")
    return "0.45";
  if (consistency_status == "drifted")
    return "0.60";
  if (semantic_role == "candidate")
    return "0.65";
  if (semantic_role == "auxiliary")
    return "0.75";
  return "0.90";
}

std::string BuildDetectionGeometryPayload(const std::vector<std::string> &refs)
{
  std::vector<std::string> payload_items;
  for (size_t i = 0; i < refs.size(); ++i)
  {
    if (refs[i].empty())
      continue;
    payload_items.push_back("ref=" + refs[i]);
  }
  return JoinTextItems(payload_items, ";");
}

std::string SelectFirstNonEmptyText(const std::vector<std::string> &values)
{
  for (size_t i = 0; i < values.size(); ++i)
  {
    if (!values[i].empty())
      return values[i];
  }
  return std::string();
}

std::string FormatElementNumber(double value)
{
  std::string text = std::to_string(value);
  while (!text.empty() && text[text.size() - 1] == '0')
    text.erase(text.size() - 1);
  if (!text.empty() && text[text.size() - 1] == '.')
    text.erase(text.size() - 1);
  if (text.empty() || text == "-0")
    return "0";
  return text;
}

std::string ResolveNamedOrDirectRef(const CxScriptExecutionResult &result,
                                    const char *result_name,
                                    const char *field_name,
                                    const std::string &direct_value)
{
  const std::string named_value = FindNamedResultFieldValue(result, result_name, field_name);
  return named_value.empty() ? direct_value : named_value;
}

std::string ResolveEnsmallenBridgeRef(const CxScriptExecutionResult &result,
                                      const char *field_name)
{
  const std::string bridge_value = FindNamedResultFieldValue(result, "bridge", field_name);
  if (!bridge_value.empty())
    return bridge_value;
  return FindAssignmentValue(result.input_params, field_name);
}

bool IsEnsmallenHumanReviewRequired(const CxScriptExecutionResult &result)
{
  return result.degraded ||
         !result.error_message.empty() ||
         result.objective_delta > 0.000001 ||
         result.metric_delta > 0.000001 ||
         result.stability_delta > 0.000001;
}

bool IsEnsmallenTemplateBridgeCase(const CxScriptExecutionResult &result)
{
  if (!IsEnsmallenPublicResult(result))
    return false;

  const std::string dataset_bridge = ResolveRuntimeDatasetBridgeTag(result);
  if (dataset_bridge == "bridge.deep_pcb_template_match" ||
      dataset_bridge == "bridge.halcon_2605_thread_selection")
    return true;

  return !FindNamedResultFieldValue(result, "bridge", "template_image").empty() ||
         !FindAssignmentValue(result.input_artifacts, "template_image").empty();
}

bool IsEnsmallenRoiBridgeCase(const CxScriptExecutionResult &result)
{
  if (!IsEnsmallenPublicResult(result))
    return false;

  return !FindNamedResultFieldValue(result, "bridge", "roi_ref").empty() ||
         !FindAssignmentValue(result.input_artifacts, "roi_ref").empty();
}

bool IsEnsmallenGeometryOnlyReplayCase(const CxScriptExecutionResult &result)
{
  if (!IsEnsmallenPublicResult(result))
    return false;

  const std::string declared_task_scope =
    FindAssignmentValue(result.input_params, "task_scope");
  const std::string task_scope =
    ToLowerText(declared_task_scope + " " + result.input_task);
  const bool geometry_scope =
    task_scope.find("geometry_fit") != std::string::npos ||
    task_scope.find("geometry") != std::string::npos;
  const bool has_geometry_ref =
    !FindNamedResultFieldValue(result, "bridge", "geometry_ref").empty() ||
    !FindAssignmentValue(result.input_artifacts, "geometry_ref").empty();
  const bool has_roi_ref =
    !FindNamedResultFieldValue(result, "bridge", "roi_ref").empty() ||
    !FindAssignmentValue(result.input_artifacts, "roi_ref").empty();
  const bool has_template_image =
    !FindNamedResultFieldValue(result, "bridge", "template_image").empty() ||
    !FindAssignmentValue(result.input_artifacts, "template_image").empty();

  return geometry_scope && has_geometry_ref && !has_roi_ref && !has_template_image;
}

std::string DetermineEnsmallenElementFallbackStatus(const CxScriptExecutionResult &result,
                                                    const std::string &element_type)
{
  if (!result.success)
    return "abnormal";

  const std::string comparison_status = BuildEnsmallenComparisonStatus(result);
  const std::string comparison_magnitude = BuildEnsmallenComparisonMagnitude(result);
  const std::string coverage_gap = BuildEnsmallenCoverageGap(result);
  const bool review_required = IsEnsmallenHumanReviewRequired(result);
  const bool has_coverage_gap =
    !coverage_gap.empty() && coverage_gap != "no_coverage_gap";
  const bool regression = comparison_status == "regressed";

  if (element_type == "optimization_compare")
    return regression || (review_required && comparison_magnitude != "none")
             ? "drifted"
             : "matched";
  if (element_type == "optimization_summary" ||
      element_type == "optimization_result")
    return regression ? "drifted" : "matched";
  if (element_type == "replay_trace")
    return (result.replay_ref.empty() && result.replay_log_path.empty())
             ? "missing"
             : (review_required ? "drifted" : "matched");
  if (element_type == "parameter_set")
    return regression || review_required ? "drifted" : "matched";
  if (element_type == "objective_target")
    return regression ? "drifted" : "matched";
  if (element_type == "threshold_policy" ||
      element_type == "crop_policy")
    return (review_required && comparison_status != "improved")
             ? "drifted"
             : "matched";
  if (element_type == "boundary_metric" ||
      element_type == "alignment_metric")
    return (review_required || regression) ? "drifted" : "matched";
  if (element_type == "cluster_group" ||
      element_type == "distance_measure" ||
      element_type == "anomaly_focus")
    return (review_required || regression || has_coverage_gap)
             ? "drifted"
             : "matched";
  if (element_type == "baseline_feature" ||
      element_type == "baseline_class")
    return regression ? "drifted" : "matched";

  return (review_required && comparison_status != "improved")
           ? "drifted"
           : "matched";
}

std::string BuildDetectionTemplateRelation(const CxScriptExecutionResult &result,
                                           const std::string &element_type,
                                           const std::string &semantic_role,
                                           const std::string &linked_template_element_id)
{
  if (IsEnsmallenPublicResult(result))
  {
    if (element_type == "optimization_compare")
      return "baseline_vs_optimized";
    if (element_type == "optimization_summary")
      return "stage_summary";
    if (element_type == "optimization_result")
      return "optimized_result";
    if (element_type == "replay_trace")
      return "replay_evidence";
    if (element_type == "parameter_set")
      return "selected_best_params";
    if (element_type == "objective_target")
      return "optimization_target";
    if (element_type == "threshold_policy")
      return "threshold_control";
    if (element_type == "crop_policy")
      return "crop_policy_control";
    if (element_type == "boundary_metric")
      return "boundary_error_guard";
    if (element_type == "alignment_metric")
      return "alignment_error_guard";
    if (element_type == "cluster_group")
      return "candidate_cluster_support";
    if (element_type == "distance_measure")
      return "distance_support";
    if (element_type == "anomaly_focus")
      return "manual_review_focus";
    if (element_type == "baseline_feature")
      return "baseline_feature_anchor";
    if (element_type == "baseline_class")
      return "baseline_class_anchor";
  }

  if (element_type == "click_point")
    return linked_template_element_id.empty() ? "interactive_seed" : "interactive_seed_to_region";
  if (element_type == "point")
    return linked_template_element_id.empty() ? "measurement_point" : "measurement_point_to_region";
  if (semantic_role == "template_anchor")
    return "template_anchor";
  if (!linked_template_element_id.empty())
  {
    if (result.case_name == "fast_template_match" ||
        result.case_name == "fastmatch_template")
      return "matched_against_template";
    if (result.case_name == "formfit_rect_candidate")
      return "selected_from_candidate_pool";
    return "linked_template";
  }
  if (result.case_name == "FindCircle" || result.case_name == "circle_measure_fit")
  {
    if (element_type == "arc")
      return "arc_supports_circle_fit";
    if (element_type == "circle")
      return "circle_fit_target";
  }
  if (result.case_name == "line_measure_roi")
  {
    if (element_type == "line_segment")
      return "line_bounds_support";
    if (element_type == "open_polyline")
      return "line_point_chain";
  }
  if (result.case_name == "binary_region")
  {
    if (semantic_role == "auxiliary")
      return "descriptor_support";
    return "region_descriptor_target";
  }
  if (result.case_name == "findobject_region")
    return "region_candidate_pool";
  return semantic_role == "candidate" ? "candidate_pool" : "standalone";
}

std::string BuildDetectionDriftSummary(const CxScriptExecutionResult &result,
                                       const std::string &element_type,
                                       const std::string &consistency_status)
{
  if (consistency_status == "missing")
    return "evidence_missing";
  if (consistency_status == "abnormal")
    return "runtime_or_contract_abnormal";
  if (consistency_status == "matched")
    return "stable";

  if (IsEnsmallenPublicResult(result))
  {
    const std::string comparison_status = BuildEnsmallenComparisonStatus(result);
    const std::string comparison_magnitude = BuildEnsmallenComparisonMagnitude(result);
    const std::string bucket_coverage = BuildEnsmallenBucketCoverage(result);
    const std::string coverage_gap = BuildEnsmallenCoverageGap(result);
    const std::string risk_axis = BuildEnsmallenRiskAxis(result);
    const std::string objective_delta = FormatElementNumber(result.objective_delta);

    if (element_type == "cluster_group" ||
        element_type == "distance_measure" ||
        element_type == "anomaly_focus")
    {
      return "comparison_status=" + comparison_status +
             ",comparison_magnitude=" + comparison_magnitude +
             ",bucket_coverage=" + bucket_coverage +
             ",coverage_gap=" + coverage_gap +
             ",risk_axis=" + risk_axis;
    }

    return "objective_delta=" + objective_delta +
           ",comparison_status=" + comparison_status +
           ",comparison_magnitude=" + comparison_magnitude +
           ",coverage_gap=" + coverage_gap +
           ",risk_axis=" + risk_axis;
  }

  if ((result.case_name == "FindCircle" || result.case_name == "circle_measure_fit") &&
      (element_type == "circle" || element_type == "arc"))
  {
    return "circle_avg_distance=" + FormatElementNumber(result.circle_avg_distance_value) +
           ",radius=" + FormatElementNumber(result.circle_radius_value);
  }
  if (result.case_name == "line_measure_roi" &&
      (element_type == "line_segment" || element_type == "open_polyline"))
  {
    return "fit_error_max=" + FormatElementNumber(result.fit_error_max_value) +
           ",subpixel_adjust_avg=" + FormatElementNumber(result.subpixel_adjust_avg_value);
  }
  if (result.case_name == "formfit_rect_candidate")
  {
    return "best_score=" + FormatElementNumber(result.match_top_score_value) +
           ",center_delta=" + FormatElementNumber(result.formfit_compare_rect_center_delta_value);
  }
  if (result.case_name == "binary_region")
  {
    return "foreground_ratio=" + FormatElementNumber(result.region_pattern_foreground_ratio_value) +
           ",descriptor_std=" + FormatElementNumber(result.region_pattern_descriptor_std_value);
  }
  if (result.case_name == "findobject_region" ||
      result.case_name == "fast_template_match" ||
      result.case_name == "fastmatch_template")
  {
    return "candidate_count=" + FormatElementNumber(result.match_candidate_count_value) +
           ",top1_score=" + FormatElementNumber(result.match_top_score_value);
  }
  return "metric_attention_required";
}

std::string BuildDetectionElementFindings(const CxScriptExecutionResult &result,
                                          const std::string &element_type,
                                          const std::string &semantic_role)
{
  if (IsEnsmallenPublicResult(result))
  {
    const std::string dataset_bridge = ResolveRuntimeDatasetBridgeTag(result);
    const std::string test_bucket = SummarizeEnsmallenInputBuckets(result);
    const std::string test_flow = BuildEnsmallenTestFlowGuide(result);
    const std::string review_scope = BuildEnsmallenReviewScope(result);
    const std::string comparison_status = BuildEnsmallenComparisonStatus(result);
    const std::string comparison_magnitude = BuildEnsmallenComparisonMagnitude(result);
    const std::string coverage_gap = BuildEnsmallenCoverageGap(result);
    const std::string coverage_status =
      flow_host_runtime_detail::BuildEnsmallenCoverageStatus(result);
    const std::string optimization_signal =
      flow_host_runtime_detail::BuildEnsmallenOptimizationSignal(result);
    const std::string best_params_ref =
      ResolveNamedOrDirectRef(result, "refs", "best_params_ref", result.best_params_ref);
    const std::string objective_ref =
      ResolveNamedOrDirectRef(result, "refs", "objective_ref", result.objective_ref);
    const std::string optimization_result_ref =
      ResolveNamedOrDirectRef(result, "refs", "optimization_result_ref",
                              result.optimization_result_ref);
    const std::string summary_ref =
      ResolveNamedOrDirectRef(result, "refs", "summary_ref", result.summary_ref);
    const std::string compare_ref =
      ResolveNamedOrDirectRef(result, "refs", "compare_ref", result.compare_ref);
    const std::string replay_ref =
      ResolveNamedOrDirectRef(result, "refs", "replay_ref",
                              result.replay_ref.empty() ? result.replay_log_path
                                                        : result.replay_ref);
    const std::string threshold_ref = ResolveEnsmallenBridgeRef(result, "threshold_ref");
    const std::string crop_policy_ref = ResolveEnsmallenBridgeRef(result, "crop_policy_ref");
    const std::string boundary_error_ref =
      ResolveEnsmallenBridgeRef(result, "boundary_error_ref");
    const std::string alignment_error_ref =
      ResolveEnsmallenBridgeRef(result, "alignment_error_ref");
    const std::string defect_count =
      FindNamedResultFieldValue(result, "bridge", "defect_count");
    const std::string objective_curve =
      BuildEnsmallenObjectiveCurveValue(result);
    const std::string feature_distance_delta =
      BuildEnsmallenFeatureDistanceDeltaValue(result);
    const std::string candidate_rank =
      BuildEnsmallenCandidateRankValue(result);
    const std::string stability_score =
      BuildEnsmallenStabilityScoreValue(result);
    const std::string selected_method =
      result.selected_method;
    const std::string candidate_ordering =
      result.ordered_candidates;
    const std::string best_candidate_confidence =
      BuildEnsmallenBestCandidateConfidenceValue(result);
    const std::string convergence_status =
      BuildEnsmallenConvergenceStatusValue(result);

    if (element_type == "optimization_compare")
      return "compare_ref=" + compare_ref +
             ";objective_curve=" + objective_curve +
             ";baseline_objective=" + FormatElementNumber(result.baseline_objective) +
             ";best_objective=" + FormatElementNumber(result.best_objective) +
             ";objective_delta=" + FormatElementNumber(result.objective_delta) +
             ";feature_distance_delta=" + feature_distance_delta +
             ";comparison_status=" + comparison_status +
             ";comparison_magnitude=" + comparison_magnitude +
             ";selected_method=" + selected_method +
             ";candidate_ordering=" + candidate_ordering;
    if (element_type == "optimization_summary" ||
        element_type == "optimization_result")
      return "summary_ref=" + summary_ref +
             ";optimization_result_ref=" + optimization_result_ref +
             ";dataset_bridge=" + dataset_bridge +
             ";test_bucket=" + test_bucket +
             ";coverage_gap=" + coverage_gap +
             ";selected_method=" + selected_method +
             ";candidate_ordering=" + candidate_ordering;
    if (element_type == "replay_trace")
      return "replay_ref=" + replay_ref +
             ";review_scope=" + review_scope +
             ";test_flow=" + test_flow +
             ";coverage_status=" + coverage_status;
    if (element_type == "parameter_set")
      return "best_params_ref=" + best_params_ref +
             ";sample_count=" + BuildEnsmallenSampleCountText(result) +
             ";best_param_sets=" + BuildEnsmallenBestParamSetsText(result) +
             ";selected_method=" + selected_method +
             ";candidate_ordering=" + candidate_ordering +
             ";candidate_rank=" + candidate_rank +
             ";stability_score=" + stability_score +
             ";best_candidate_confidence=" + best_candidate_confidence +
             ";optimization_signal=" + optimization_signal;
    if (element_type == "objective_target")
      return "objective_ref=" + objective_ref +
             ";dataset_bridge=" + dataset_bridge +
             ";review_scope=" + review_scope +
             ";optimization_signal=" + optimization_signal;
    if (element_type == "closed_region")
      return "roi_ref=" + ResolveNamedOrDirectRef(result,
                                                  "bridge",
                                                  "roi_ref",
                                                  FindAssignmentValue(result.input_artifacts, "roi_ref")) +
             ";dataset_bridge=" + dataset_bridge +
             ";objective_ref=" + objective_ref;
    if (element_type == "threshold_policy")
      return "threshold_ref=" + threshold_ref +
             ";comparison_status=" + comparison_status +
             ";test_bucket=" + test_bucket;
    if (element_type == "crop_policy")
      return "crop_policy_ref=" + crop_policy_ref +
             ";review_scope=" + review_scope +
             ";dataset_bridge=" + dataset_bridge;
    if (element_type == "boundary_metric")
      return "boundary_error_ref=" + boundary_error_ref +
             ";coverage_status=" + coverage_status +
             ";optimization_signal=" + optimization_signal;
    if (element_type == "alignment_metric")
      return "alignment_error_ref=" + alignment_error_ref +
             ";coverage_status=" + coverage_status +
             ";optimization_signal=" + optimization_signal;
    if (element_type == "cluster_group")
      return "cluster_ref=" + result.cluster_ref +
             ";sample_count=" + BuildEnsmallenSampleCountText(result) +
             ";test_bucket=" + test_bucket +
             ";coverage_gap=" + coverage_gap +
             ";stability_score=" + stability_score;
    if (element_type == "distance_measure")
      return "distance_ref=" + result.distance_ref +
              ";objective_delta=" + FormatElementNumber(result.objective_delta) +
              ";feature_distance_delta=" + feature_distance_delta +
              ";comparison_status=" + comparison_status +
              ";defect_count=" + defect_count;
    if (element_type == "anomaly_focus")
      return "anomaly_ref=" + result.anomaly_ref +
             ";coverage_gap=" + coverage_gap +
             ";defect_count=" + defect_count +
             ";review_scope=" + review_scope +
             ";convergence_status=" + convergence_status;
    if (element_type == "baseline_feature")
      return "baseline_feature_ref=" + result.baseline_feature_ref +
             ";dataset_bridge=" + dataset_bridge +
             ";test_bucket=" + test_bucket;
    if (element_type == "baseline_class")
      return "baseline_class_ref=" + result.baseline_class_ref +
             ";dataset_bridge=" + dataset_bridge +
             ";semantic_role=" + semantic_role;
  }

  if (result.case_name == "line_measure_roi")
  {
    if (element_type == "open_polyline")
      return "point_count=" + FormatElementNumber(result.point_count_value) +
             ";chain_length=" + FormatElementNumber(result.line_chain_length_value);
    return "fit_error_avg=" + FormatElementNumber(result.fit_error_avg_value) +
           ";fit_error_max=" + FormatElementNumber(result.fit_error_max_value) +
           ";line_angle=" + FormatElementNumber(result.line_angle_value);
  }

  if (result.case_name == "FindCircle" || result.case_name == "circle_measure_fit")
  {
    if (element_type == "circle")
      return "center=" + FormatElementNumber(result.circle_center_x_value) + "," +
             FormatElementNumber(result.circle_center_y_value) +
             ";radius=" + FormatElementNumber(result.circle_radius_value) +
             ";avg_distance=" + FormatElementNumber(result.circle_avg_distance_value);
    return "sample_points=" + FormatElementNumber(result.circle_sample_points_value) +
           ";failure_stage=" + (result.circle_failure_stage.empty() ? "none"
                                                                   : result.circle_failure_stage);
  }

  if (result.case_name == "fast_template_match" ||
      result.case_name == "fastmatch_template")
  {
    if (semantic_role == "template_anchor")
      return "template_rect_ref=" + result.template_rect_overlay_ref +
             ";template_identity=" + ResolveDetectionTemplateIdentity(result) +
             ";template_provenance=" + ResolveDetectionTemplateProvenance(result);
    return "candidate_count=" + FormatElementNumber(result.match_candidate_count_value) +
           ";top1_score=" + FormatElementNumber(result.match_top_score_value) +
           ";top1_rect=" + FormatElementNumber(result.match_best_rect_x_value) + "," +
           FormatElementNumber(result.match_best_rect_y_value) + "," +
           FormatElementNumber(result.match_best_rect_w_value) + "," +
           FormatElementNumber(result.match_best_rect_h_value) +
           ";template_identity=" + ResolveDetectionTemplateIdentity(result) +
           ";acquisition=" + ResolveDetectionExecutionMode(result);
  }

  if (result.case_name == "formfit_rect_candidate")
  {
    return "candidate_count=" + FormatElementNumber(result.match_candidate_count_value) +
           ";selected_index=" + FormatElementNumber(result.match_selected_index_value) +
           ";best_score=" + FormatElementNumber(result.match_top_score_value) +
           ";fit_mode=" + FindNamedResultFieldValue(result, "analysis", "fit_mode") +
           ";template_identity=" + ResolveDetectionTemplateIdentity(result) +
           ";acquisition=" + ResolveDetectionExecutionMode(result);
  }

  if (result.case_name == "binary_region")
  {
    if (semantic_role == "auxiliary")
      return "descriptor_dim=" + FormatElementNumber(result.region_pattern_descriptor_dim_value) +
             ";descriptor_mean=" + FormatElementNumber(result.region_pattern_descriptor_mean_value) +
             ";descriptor_std=" + FormatElementNumber(result.region_pattern_descriptor_std_value);
    return "foreground_ratio=" + FormatElementNumber(result.region_pattern_foreground_ratio_value) +
           ";components=" + FormatElementNumber(result.region_connected_components_value) +
           ";bounds_count=" + FormatElementNumber(result.region_bounds_count_value);
  }

  if (result.case_name == "findobject_region")
  {
    return "result_count=" + FormatElementNumber(result.match_candidate_count_value) +
           ";top1_rect=" + FormatElementNumber(result.match_best_rect_x_value) + "," +
           FormatElementNumber(result.match_best_rect_y_value) + "," +
           FormatElementNumber(result.match_best_rect_w_value) + "," +
           FormatElementNumber(result.match_best_rect_h_value) +
           ";foreground_ratio=" + FormatElementNumber(result.region_foreground_ratio_value) +
           ";acquisition=" + ResolveDetectionExecutionMode(result);
  }

  return element_type + ";role=" + semantic_role;
}

std::string BuildDetectionElementLevelFocus(const CxScriptExecutionResult &result,
                                            const std::string &element_type,
                                            const std::string &semantic_role,
                                            const std::string &consistency_status)
{
  if (IsEnsmallenPublicResult(result))
  {
    if (element_type == "optimization_compare")
      return "inspect baseline-vs-best delta and which tuned elements moved";
    if (element_type == "optimization_summary" ||
        element_type == "optimization_result")
      return "review per-bucket optimization outcome and element drift coverage";
    if (element_type == "replay_trace")
      return "replay scenario/train/infer evidence and verify element-level repeatability";
    if (element_type == "parameter_set")
      return "check selected params against bucket stability and downstream element drift";
    if (element_type == "objective_target")
      return "confirm objective still maps to visible geometry or match elements";
    if (element_type == "closed_region")
      return "verify roi_ref remains the actual optimization landing zone";
    if (element_type == "threshold_policy")
      return "review threshold sensitivity across G1 tuning and G2 competition cases";
    if (element_type == "crop_policy")
      return "verify crop policy does not hide drifted roi or candidate elements";
    if (element_type == "boundary_metric")
      return "inspect boundary-sensitive elements and geometry-fit drift";
    if (element_type == "alignment_metric")
      return "inspect alignment-sensitive elements and interaction drift";
    if (element_type == "cluster_group")
      return "inspect candidate grouping order and cluster stability across samples";
    if (element_type == "distance_measure")
      return "check feature-distance spread and nearest-neighbor separation";
    if (element_type == "anomaly_focus")
      return "promote drifted or missing optimization elements to manual review";
    if (element_type == "baseline_feature" ||
        element_type == "baseline_class")
      return "compare optimized result against baseline anchors before expansion";
  }

  if (result.case_name == "line_measure_roi")
    return "verify point-chain continuity and bounds alignment";
  if (result.case_name == "FindCircle" || result.case_name == "circle_measure_fit")
    return element_type == "arc"
             ? "inspect arc completeness and ellipse rejection boundary"
             : "verify center and radius stability under partial edges";
  if (result.case_name == "fast_template_match" || result.case_name == "fastmatch_template")
    return semantic_role == "template_anchor"
             ? "compare template anchor with test rect and candidate ordering"
             : "check top1 rect alignment and rotation-scale tolerance";
  if (result.case_name == "formfit_rect_candidate")
    return "inspect candidate ranking, selection mode, and rect-center drift";
  if (result.case_name == "binary_region")
    return semantic_role == "auxiliary"
             ? "review descriptor separability against brightness and texture drift"
             : "check region mask stability and descriptor support";
  if (result.case_name == "findobject_region")
    return "inspect region thresholding and candidate multiplicity";
  if (consistency_status != "matched")
    return "manual review required for unmatched element";
  return element_type + "_stable_watch";
}

std::string DetermineDetectionCandidateStatus(const CxScriptExecutionResult &result,
                                              const std::string &element_type,
                                              const std::string &semantic_role,
                                              const std::string &consistency_status)
{
  if (consistency_status == "missing")
    return "candidate_missing";
  if (consistency_status == "abnormal")
    return "candidate_abnormal";
  if (semantic_role == "template_anchor")
    return "template_anchor";
  if (semantic_role == "candidate")
    return consistency_status == "drifted" ? "candidate_competing" : "candidate_ready";
  if (semantic_role == "review_target")
    return consistency_status == "drifted" ? "candidate_selected_with_drift"
                                           : "candidate_selected";
  if (semantic_role == "primary")
    return element_type == "circle" ? "fit_primary" : "primary_candidate";
  if (semantic_role == "auxiliary")
    return "support_candidate";
  if (result.case_name == "geometry_topology_pipeline")
    return "topology_stage_observed";
  return "standalone_candidate";
}

std::string DetermineDetectionMatchStatus(const CxScriptExecutionResult &result,
                                          const std::string &element_type,
                                          const std::string &semantic_role,
                                          const std::string &consistency_status)
{
  if (consistency_status == "missing")
    return "match_missing";
  if (consistency_status == "abnormal")
    return "match_abnormal";

  if (result.case_name == "fast_template_match" ||
      result.case_name == "fastmatch_template")
  {
    if (semantic_role == "template_anchor")
      return "template_reference";
    if (semantic_role == "candidate")
      return consistency_status == "drifted" ? "candidate_pool_unstable"
                                             : "candidate_pool_ranked";
    if (element_type == "match_region")
      return consistency_status == "drifted" ? "match_region_drifted"
                                             : "match_region_confirmed";
    return consistency_status == "drifted" ? "match_drifted" : "match_confirmed";
  }

  if (result.case_name == "formfit_rect_candidate")
  {
    if (semantic_role == "candidate")
      return consistency_status == "drifted" ? "candidate_selection_drifted"
                                             : "candidate_selection_ranked";
    if (element_type == "match_region")
      return consistency_status == "drifted" ? "selection_region_drifted"
                                             : "selection_region_confirmed";
    return consistency_status == "drifted" ? "selection_drifted" : "selection_confirmed";
  }

  if (result.case_name == "findobject_region")
  {
    if (semantic_role == "candidate")
      return consistency_status == "drifted" ? "region_candidate_unstable"
                                             : "region_candidate_ranked";
    if (element_type == "match_region")
      return consistency_status == "drifted" ? "region_match_region_drifted"
                                             : "region_match_region_confirmed";
    return consistency_status == "drifted" ? "region_match_drifted"
                                           : "region_match_confirmed";
  }

  if (result.case_name == "FindCircle" || result.case_name == "circle_measure_fit")
  {
    if (element_type == "arc")
      return consistency_status == "drifted" ? "arc_support_drifted" : "arc_support_ready";
    if (element_type == "circle")
      return consistency_status == "drifted" ? "circle_fit_drifted" : "circle_fit_confirmed";
    return "fit_anchor";
  }

  if (result.case_name == "line_measure_roi")
  {
    if (element_type == "open_polyline")
      return consistency_status == "drifted" ? "edge_chain_drifted" : "edge_chain_measured";
    if (element_type == "line_segment")
      return consistency_status == "drifted" ? "line_fit_drifted" : "line_fit_confirmed";
    return "sample_support";
  }

  if (result.case_name == "binary_region")
    return semantic_role == "auxiliary" ? "descriptor_support_compared"
                                        : "region_pattern_compared";

  if (result.case_name == "geometry_topology_pipeline")
    return consistency_status == "drifted" ? "topology_stage_drifted"
                                           : "topology_stage_visible";

  return consistency_status == "drifted" ? "match_drifted" : "match_ready";
}

std::string BuildDetectionManualReviewSignal(const CxScriptExecutionResult &result,
                                             const std::string &element_type,
                                             const std::string &semantic_role,
                                             const std::string &consistency_status)
{
  if (consistency_status == "abnormal")
    return "manual_review_runtime_or_contract";
  if (consistency_status == "missing")
    return "manual_review_missing_evidence";
  if (consistency_status == "drifted")
  {
    if (result.case_name == "fast_template_match" ||
        result.case_name == "fastmatch_template")
      return "manual_review_match_drift";
    if (result.case_name == "FindCircle" || result.case_name == "circle_measure_fit")
      return "manual_review_circle_fit_drift";
    if (result.case_name == "formfit_rect_candidate")
      return "manual_review_candidate_selection_drift";
    if (result.case_name == "binary_region")
      return "manual_review_texture_descriptor_drift";
    if (result.case_name == "findobject_region")
      return "manual_review_region_match_drift";
    if (result.case_name == "line_measure_roi")
      return "manual_review_line_fit_drift";
    if (result.case_name == "geometry_topology_pipeline")
      return "manual_review_topology_stage_drift";
    return "manual_review_drift";
  }
  if (semantic_role == "candidate")
    return "manual_review_candidate_spotcheck";
  if (semantic_role == "review_target" || semantic_role == "primary")
    return "manual_review_primary_confirmation";
  if (semantic_role == "template_anchor")
    return "manual_review_template_anchor";
  if (element_type == "point" || element_type == "click_point")
    return "manual_review_seed_spotcheck";
  return "manual_review_not_required";
}

std::string BuildDetectionElementGroupId(const CxScriptExecutionResult &result,
                                         const std::string &element_type,
                                         const std::string &semantic_role)
{
  const std::string case_prefix =
    (result.module.empty() ? std::string("review") : result.module) + "." + result.case_name;

  if (result.case_name == "fast_template_match" ||
      result.case_name == "fastmatch_template")
  {
    if (semantic_role == "template_anchor")
      return case_prefix + ".template_anchor";
    if (semantic_role == "candidate")
      return case_prefix + ".candidate_pool";
    return case_prefix + ".match_result";
  }
  if (result.case_name == "formfit_rect_candidate")
    return semantic_role == "candidate" ? case_prefix + ".candidate_pool"
                                        : case_prefix + ".selected_geometry";
  if (result.case_name == "FindCircle" || result.case_name == "circle_measure_fit")
    return element_type == "circle" ? case_prefix + ".circle_fit" : case_prefix + ".fit_support";
  if (result.case_name == "line_measure_roi")
    return element_type == "line_segment" ? case_prefix + ".line_fit" : case_prefix + ".sample_chain";
  if (result.case_name == "binary_region")
    return semantic_role == "auxiliary" ? case_prefix + ".descriptor_support"
                                        : case_prefix + ".region_pattern";
  if (result.case_name == "findobject_region")
    return semantic_role == "candidate" ? case_prefix + ".candidate_pool"
                                        : case_prefix + ".region_match";
  if (result.case_name == "geometry_topology_pipeline")
    return case_prefix + ".topology_stage_chain";
  return case_prefix + ".geometry_review";
}

std::string BuildDetectionElementGroupLabel(const CxScriptExecutionResult &result,
                                            const std::string &element_type,
                                            const std::string &semantic_role)
{
  if (result.case_name == "fast_template_match" ||
      result.case_name == "fastmatch_template")
  {
    if (semantic_role == "template_anchor")
      return "template_anchor";
    if (semantic_role == "candidate")
      return "match_candidate_pool";
    return "match_result";
  }
  if (result.case_name == "formfit_rect_candidate")
    return semantic_role == "candidate" ? "formfit_candidate_pool" : "formfit_selected_geometry";
  if (result.case_name == "FindCircle" || result.case_name == "circle_measure_fit")
    return element_type == "circle" ? "circle_fit" : "circle_fit_support";
  if (result.case_name == "line_measure_roi")
    return element_type == "line_segment" ? "line_fit" : "line_sample_chain";
  if (result.case_name == "binary_region")
    return semantic_role == "auxiliary" ? "descriptor_support" : "region_pattern";
  if (result.case_name == "findobject_region")
    return semantic_role == "candidate" ? "region_candidate_pool" : "region_match";
  if (result.case_name == "geometry_topology_pipeline")
    return "topology_stage_chain";
  return semantic_role.empty() ? element_type : semantic_role;
}

std::string BuildDetectionFocusRegionRef(const std::string &element_id,
                                         const std::string &source_ref,
                                         const std::string &primary_overlay_ref,
                                         const std::string &linked_template_element_id)
{
  if (!primary_overlay_ref.empty())
    return primary_overlay_ref;
  if (!source_ref.empty())
    return source_ref;
  if (!linked_template_element_id.empty())
    return linked_template_element_id;
  return element_id;
}

std::string BuildDetectionLocalDeltaRef(const std::string &element_id,
                                        const std::string &source_ref,
                                        const std::string &primary_overlay_ref,
                                        const std::string &linked_template_element_id,
                                        const std::string &consistency_status,
                                        const std::string &semantic_role)
{
  if (consistency_status == "matched" &&
      semantic_role != "candidate" &&
      semantic_role != "review_target")
    return std::string();
  if (!primary_overlay_ref.empty())
    return primary_overlay_ref;
  if (!source_ref.empty())
    return source_ref;
  if (!linked_template_element_id.empty())
    return linked_template_element_id;
  return element_id;
}

std::string BuildDetectionElementStatusSummary(const UnifiedDetectionElement &element)
{
  std::vector<std::string> items;
  items.push_back("type=" + element.element_type);
  items.push_back("role=" + element.semantic_role);
  items.push_back("status=" + element.consistency_status);
  if (!element.candidate_status.empty())
    items.push_back("candidate=" + element.candidate_status);
  if (!element.match_status.empty())
    items.push_back("match=" + element.match_status);
  if (!element.manual_review_signal.empty())
    items.push_back("manual=" + element.manual_review_signal);
  items.push_back("source=" + element.provenance);
  if (!element.template_relation.empty())
    items.push_back("template=" + element.template_relation);
  if (!element.element_group_label.empty())
    items.push_back("group=" + element.element_group_label);
  if (!element.drift_summary.empty())
    items.push_back("drift=" + element.drift_summary);
  items.push_back("confidence=" + element.confidence);
  return JoinTextItems(items, ",");
}

#include "parser_cxscript_runtime_review_helpers.h"

#include "parser_cxscript_runtime_named_result_views.h"

#include "parser_cxscript_runtime_review_surfaces.h"

#include "parser_cxscript_runtime_execution_helpers.h"

bool IsBuiltinTypeName(const std::string &name)
{
  return name == "int" ||
         name == "double" ||
         name == "bool" ||
         name == "string" ||
         name == "float" ||
         name == "long";
}

bool IsIdentifier(const std::string &name)
{
  if (name.empty())
    return false;

  const char first = name[0];
  if (!((first >= 'a' && first <= 'z') ||
        (first >= 'A' && first <= 'Z') ||
        first == '_'))
    return false;

  for (size_t i = 1; i < name.size(); ++i)
  {
    const char ch = name[i];
    if (!((ch >= 'a' && ch <= 'z') ||
          (ch >= 'A' && ch <= 'Z') ||
          (ch >= '0' && ch <= '9') ||
          ch == '_'))
      return false;
  }

  return true;
}

bool IsKnownTypeName(const std::vector<CxScriptTypeSpec> &types, const std::string &name)
{
  for (size_t i = 0; i < types.size(); ++i)
  {
    if (types[i].name == name)
      return true;
  }
  return false;
}

bool TryParseVariableDeclaration(const std::string &trimmed,
                                 const std::vector<CxScriptTypeSpec> &types,
                                 std::string &type_name,
                                 std::string &variable_name,
                                 bool &initialized)
{
  initialized = false;
  type_name.clear();
  variable_name.clear();

  std::string text = trimmed;
  if (text.empty() || text[text.size() - 1] != ';')
    return false;
  text = Trim(text.substr(0, text.size() - 1));

  const size_t first_space = text.find(' ');
  if (first_space == std::string::npos)
    return false;

  type_name = Trim(text.substr(0, first_space));
  if (!IsKnownTypeName(types, type_name))
    return false;

  std::string rest = Trim(text.substr(first_space + 1));
  const size_t eq = rest.find('=');
  if (eq != std::string::npos)
  {
    variable_name = Trim(rest.substr(0, eq));
    initialized = true;
  }
  else
  {
    variable_name = Trim(rest);
  }

  return IsIdentifier(variable_name);
}

bool TryParseObjectReturnAssignment(const std::string &trimmed,
                                    const std::vector<CxScriptTypeSpec> &types,
                                    std::string &lhs_type,
                                    std::string &lhs_object,
                                    std::string &source_object,
                                    std::string &method_name,
                                    std::string &argument_text,
                                    std::string &initializer_text,
                                    bool &declares_object)
{
  lhs_type.clear();
  lhs_object.clear();
  source_object.clear();
  method_name.clear();
  argument_text.clear();
  initializer_text.clear();
  declares_object = false;

  std::string text = trimmed;
  if (text.empty() || text[text.size() - 1] != ';')
    return false;
  text = Trim(text.substr(0, text.size() - 1));

  const size_t eq = text.find('=');
  if (eq == std::string::npos || eq == 0)
    return false;

  const std::string lhs = Trim(text.substr(0, eq));
  const std::string rhs = Trim(text.substr(eq + 1));
  if (lhs.empty() || rhs.empty())
    return false;

  const size_t open = rhs.find('(');
  const size_t close = rhs.rfind(')');
  if (open == std::string::npos || close != rhs.size() - 1 || close <= open)
    return false;

  const std::string callee = Trim(rhs.substr(0, open));
  const size_t dot = callee.rfind('.');
  if (dot == std::string::npos || dot == 0 || dot + 1 >= callee.size())
    return false;

  source_object = Trim(callee.substr(0, dot));
  method_name = Trim(callee.substr(dot + 1));
  if (!IsIdentifier(source_object) || !IsIdentifier(method_name))
    return false;

  argument_text = Trim(rhs.substr(open + 1, close - open - 1));
  initializer_text = rhs;

  const size_t lhs_space = lhs.find(' ');
  if (lhs_space != std::string::npos)
  {
    lhs_type = Trim(lhs.substr(0, lhs_space));
    lhs_object = Trim(lhs.substr(lhs_space + 1));
    declares_object = true;
    return IsKnownTypeName(types, lhs_type) && IsIdentifier(lhs_object);
  }

  lhs_object = lhs;
  return IsIdentifier(lhs_object);
}
bool LooksLikeTypeDeclaration(const std::string &trimmed,
                              std::string &type_name,
                              std::string &variable_name)
{
  type_name.clear();
  variable_name.clear();

  std::string text = trimmed;
  if (text.empty() || text[text.size() - 1] != ';')
    return false;
  text = Trim(text.substr(0, text.size() - 1));

  const size_t first_space = text.find(' ');
  if (first_space == std::string::npos)
    return false;

  type_name = Trim(text.substr(0, first_space));
  if (!IsIdentifier(type_name))
    return false;

  std::string rest = Trim(text.substr(first_space + 1));
  if (rest.empty())
    return false;

  const size_t eq = rest.find('=');
  if (eq != std::string::npos)
    rest = Trim(rest.substr(0, eq));

  variable_name = rest;
  return IsIdentifier(variable_name);
}

void AddTypeSpec(std::vector<CxScriptTypeSpec> &types,
                 const std::string &name,
                 bool builtin,
                 bool user_defined)
{
  for (size_t i = 0; i < types.size(); ++i)
  {
    if (types[i].name == name)
      return;
  }

  CxScriptTypeSpec spec;
  spec.name = name;
  spec.type_name = name;
  spec.builtin = builtin;
  spec.user_defined = user_defined;
  types.push_back(spec);
}

void AddBuiltinTypes(std::vector<CxScriptTypeSpec> &types)
{
  AddTypeSpec(types, "int", true, false);
  AddTypeSpec(types, "double", true, false);
  AddTypeSpec(types, "bool", true, false);
  AddTypeSpec(types, "string", true, false);
  AddTypeSpec(types, "float", true, false);
  AddTypeSpec(types, "long", true, false);
}

CxScriptStatement MakeStatement(CxScriptStmtKind kind,
                                const std::string &text,
                                const std::string &name,
                                const std::string &step_name,
                                int line_number,
                                int block_depth)
{
  CxScriptStatement stmt;
  stmt.kind = kind;
  stmt.text = text;
  stmt.name = name;
  stmt.step_name = step_name;
  stmt.block_depth = block_depth;
  stmt.span.line_begin = line_number;
  stmt.span.column_begin = 1;
  stmt.span.line_end = line_number;
  stmt.span.column_end = static_cast<int>(text.size()) + 1;
  return stmt;
}
}

bool ParserCxScriptRuntime::ParseScriptFlow(const std::string &script_name,
                                            const std::string &script_text,
                                            CxScriptExecutionContext &context,
                                            CxScriptFlow &flow,
                                            CxScriptParseError &parse_error,
                                            std::string &error_message)
{
  return ParseScriptText(script_name, script_text, context, flow, parse_error, error_message);
}

bool ParserCxScriptRuntime::ParseScriptText(const std::string &script_name,
                                            const std::string &script_text,
                                            CxScriptExecutionContext &context,
                                            CxScriptFlow &flow,
                                            CxScriptParseError &parse_error,
                                            std::string &error_message)
{
  context = CxScriptExecutionContext();
  flow = CxScriptFlow();
  parse_error = CxScriptParseError();
  context.script_name = script_name;
  std::string first_call_name;
  std::string current_step;
  int block_depth = 0;
  int line_number = 0;
  AddBuiltinTypes(flow.declared_types);

  std::istringstream input(script_text);
  std::string line;
  while (std::getline(input, line))
  {
    ++line_number;
    const std::string trimmed = Trim(line);
    if (trimmed.empty() || trimmed[0] == '#')
      continue;

    const int close_count = CountChar(trimmed, '}');
    const int open_count = CountChar(trimmed, '{');
    if (close_count > block_depth + open_count)
    {
      parse_error.message = "unexpected closing block boundary";
      parse_error.token = "}";
      parse_error.line = line_number;
      parse_error.column = static_cast<int>(trimmed.find('}')) + 1;
      parse_error.block_depth = block_depth;
      parse_error.step_name = current_step;
      error_message = parse_error.message;
      return false;
    }

    if (StartsWith(trimmed, "step "))
    {
      std::string step_text = Trim(trimmed.substr(5));
      if (!step_text.empty() && step_text[step_text.size() - 1] == '{')
        step_text = Trim(step_text.substr(0, step_text.size() - 1));
      current_step = step_text;
      CxScriptStatement stmt = MakeStatement(cxssk_step,
                                             step_text,
                                             step_text,
                                             current_step,
                                             line_number,
                                             block_depth);
      flow.statements.push_back(stmt);
      block_depth += open_count - close_count;
      continue;
    }

    bool boundary_only = true;
    for (size_t i = 0; i < trimmed.size(); ++i)
    {
      const char ch = trimmed[i];
      if (ch != '{' && ch != '}' && ch != ' ' && ch != '\t')
      {
        boundary_only = false;
        break;
      }
    }
    if (boundary_only)
    {
      flow.statements.push_back(MakeStatement(cxssk_block_boundary,
                                              trimmed,
                                              trimmed,
                                              current_step,
                                              line_number,
                                              block_depth));
      block_depth += open_count - close_count;
      continue;
    }

    if (StartsWith(trimmed, "type "))
    {
      const std::string type_name = StripTrailingSemicolon(trimmed.substr(5));
      if (type_name.empty())
      {
        parse_error.message = "custom type name is missing";
        parse_error.token = "type";
        parse_error.line = line_number;
        parse_error.column = 1;
        parse_error.block_depth = block_depth;
        parse_error.step_name = current_step;
        error_message = parse_error.message;
        return false;
      }

      AddTypeSpec(flow.declared_types, type_name, false, true);
      flow.statements.push_back(MakeStatement(cxssk_type_decl,
                                              type_name,
                                              type_name,
                                              current_step,
                                              line_number,
                                              block_depth));
      block_depth += open_count - close_count;
      continue;
    }

    if (StartsWith(trimmed, "module "))
    {
      context.kind = "module";
      context.module = StripTrailingSemicolon(trimmed.substr(7));
      flow.statements.push_back(MakeStatement(cxssk_header_metadata,
                                              "module=" + context.module,
                                              "module",
                                              "__header__",
                                              line_number,
                                              block_depth));
      block_depth += open_count - close_count;
      continue;
    }

    if (StartsWith(trimmed, "layer "))
    {
      context.layer = StripTrailingSemicolon(trimmed.substr(6));
      flow.statements.push_back(MakeStatement(cxssk_header_metadata,
                                              "layer=" + context.layer,
                                              "layer",
                                              "__header__",
                                              line_number,
                                              block_depth));
      block_depth += open_count - close_count;
      continue;
    }

    if (StartsWith(trimmed, "case "))
    {
      context.case_name = StripTrailingSemicolon(trimmed.substr(5));
      flow.statements.push_back(MakeStatement(cxssk_header_metadata,
                                              "case_name=" + context.case_name,
                                              "case_name",
                                              "__header__",
                                              line_number,
                                              block_depth));
      block_depth += open_count - close_count;
      continue;
    }

    if (trimmed == "input {" || trimmed == "flow {" ||
        trimmed == "check {" || trimmed == "output {" ||
        trimmed == "conclusion {")
    {
      const std::string step_text = Trim(trimmed.substr(0, trimmed.size() - 1));
      current_step = step_text;
      CxScriptStatement stmt = MakeStatement(cxssk_step,
                                             step_text,
                                             step_text,
                                             current_step,
                                             line_number,
                                             block_depth);
      flow.statements.push_back(stmt);
      block_depth += open_count - close_count;
      continue;
    }

    if (StartsWith(trimmed, "use "))
    {
      const std::string use_name = StripTrailingSemicolon(trimmed.substr(4));
      if (use_name.empty())
      {
        parse_error.message = "type use name is missing";
        parse_error.token = "use";
        parse_error.line = line_number;
        parse_error.column = 1;
        parse_error.block_depth = block_depth;
        parse_error.step_name = current_step;
        error_message = parse_error.message;
        return false;
      }

      AddTypeSpec(flow.declared_types, use_name, IsBuiltinTypeName(use_name), !IsBuiltinTypeName(use_name));
      flow.statements.push_back(MakeStatement(cxssk_use,
                                              use_name,
                                              use_name,
                                              current_step,
                                              line_number,
                                              block_depth));
      block_depth += open_count - close_count;
      continue;
    }

    if (trimmed == "flow=CxCoreFlowHost();")
    {
      AddTypeSpec(flow.declared_types, "CxCoreFlowHost", false, true);
      context.kind = "module";
      flow.statements.push_back(MakeStatement(cxssk_var_decl,
                                              "CxCoreFlowHost flow",
                                              "flow",
                                              current_step,
                                              line_number,
                                              block_depth));
      flow.statements.back().declared_type = "CxCoreFlowHost";
      flow.statements.back().initializer_text = "CxCoreFlowHost()";

      CxScriptVariableDecl variable;
      variable.type_name = "CxCoreFlowHost";
      variable.variable_name = "flow";
      variable.step_name = current_step;
      variable.span = flow.statements.back().span;
      variable.block_depth = block_depth;
      variable.initialized = true;
      variable.known_type = true;
      flow.variables.push_back(variable);

      block_depth += open_count - close_count;
      continue;
    }

    if (StartsWith(trimmed, "input "))
    {
      CxScriptStatement stmt = MakeStatement(cxssk_input,
                                             StripTrailingSemicolon(trimmed.substr(6)),
                                             std::string(),
                                             current_step,
                                             line_number,
                                             block_depth);
      flow.statements.push_back(stmt);
      block_depth += open_count - close_count;
      continue;
    }

    if (StartsWith(trimmed, "Input."))
    {
      flow.statements.push_back(MakeStatement(cxssk_input,
                                              StripTrailingSemicolon(trimmed),
                                              std::string(),
                                              current_step,
                                              line_number,
                                              block_depth));
      block_depth += open_count - close_count;
      continue;
    }

    std::string legacy_flow_method_name;
    std::vector<std::string> legacy_flow_args;
    if (TryParseLegacyFlowCall(trimmed, legacy_flow_method_name, legacy_flow_args))
    {
      if (legacy_flow_method_name == "Begin")
      {
        const std::string flow_name = legacy_flow_args.empty() ? std::string() : legacy_flow_args[0];
        if (!flow_name.empty())
        {
          flow.statements.push_back(MakeStatement(cxssk_header_metadata,
                                                  "trace_id=" + flow_name,
                                                  "trace_id",
                                                  "__header__",
                                                  line_number,
                                                  block_depth));
          if (context.trace_id.empty())
            context.trace_id = flow_name;
        }

        current_step = "flow";
        flow.statements.push_back(MakeStatement(cxssk_step,
                                                "flow",
                                                "flow",
                                                current_step,
                                                line_number,
                                                block_depth));
      }
      else if (legacy_flow_method_name == "End")
      {
        flow.statements.push_back(MakeStatement(cxssk_emit,
                                                "flow.end",
                                                "print",
                                                current_step.empty() ? "flow" : current_step,
                                                line_number,
                                                block_depth));
      }
      else
      {
        flow.statements.push_back(MakeStatement(cxssk_action,
                                                StripTrailingSemicolon(trimmed),
                                                legacy_flow_method_name,
                                                current_step,
                                                line_number,
                                                block_depth));
      }

      block_depth += open_count - close_count;
      continue;
    }

    std::string flow_method_name;
    std::vector<std::string> flow_args;
    if (TryParseFlowHostCall(trimmed, flow_method_name, flow_args))
    {
      if (flow_method_name == "set_case" && !flow_args.empty())
      {
        context.case_name = NormalizeFlowHostCaseName(flow_args[0]);
        flow.statements.push_back(MakeStatement(cxssk_header_metadata,
                                                "case_name=" + context.case_name,
                                                "case_name",
                                                "__header__",
                                                line_number,
                                                block_depth));
      }
      else if (flow_method_name == "set_layer" && !flow_args.empty())
      {
        context.layer = flow_args[0];
        flow.statements.push_back(MakeStatement(cxssk_header_metadata,
                                                "layer=" + context.layer,
                                                "layer",
                                                "__header__",
                                                line_number,
                                                block_depth));
      }
      else if (flow_method_name == "set_module" && !flow_args.empty())
      {
        context.kind = "module";
        context.module = flow_args[0];
        flow.statements.push_back(MakeStatement(cxssk_header_metadata,
                                                "module=" + context.module,
                                                "module",
                                                "__header__",
                                                line_number,
                                                block_depth));
      }
      else if (flow_method_name == "call" && !flow_args.empty())
      {
        const std::string call_name = flow_args[0];
        CxScriptStatement stmt = MakeStatement(cxssk_call,
                                               "call " + call_name,
                                               call_name,
                                               current_step,
                                               line_number,
                                               block_depth);
        stmt.callee_name = call_name;
        stmt.argument_text = call_name;
        if (first_call_name.empty())
          first_call_name = call_name;
        flow.statements.push_back(stmt);
      }
      else if (flow_method_name == "finish")
      {
        flow.statements.push_back(MakeStatement(cxssk_emit,
                                                "flow.finish",
                                                "print",
                                                current_step,
                                                line_number,
                                                block_depth));
      }
      else
      {
        flow.statements.push_back(MakeStatement(cxssk_action,
                                                StripTrailingSemicolon(trimmed),
                                                flow_method_name,
                                                current_step,
                                                line_number,
                                                block_depth));
      }

      block_depth += open_count - close_count;
      continue;
    }

    if (StartsWith(trimmed, "expect "))
    {
      CxScriptStatement stmt = MakeStatement(cxssk_expect,
                                             StripTrailingSemicolon(trimmed.substr(7)),
                                             std::string(),
                                             current_step,
                                             line_number,
                                             block_depth);
      ParseExpectExpression(stmt.text, stmt.lhs_text, stmt.operator_text, stmt.rhs_text);
      flow.statements.push_back(stmt);
      block_depth += open_count - close_count;
      continue;
    }

    std::string check_method_name;
    std::vector<std::string> check_method_args;
    if (TryParseCheckMethodCall(trimmed, check_method_name, check_method_args))
    {
      CxScriptStatement stmt = MakeStatement(cxssk_expect,
                                             StripTrailingSemicolon(trimmed),
                                             check_method_name,
                                             current_step,
                                             line_number,
                                             block_depth);
      stmt.argument_text = ExtractCallArguments(trimmed);
      flow.statements.push_back(stmt);
      block_depth += open_count - close_count;
      continue;
    }

    if (StartsWith(trimmed, "call "))
    {
      const std::string call_text = StripTrailingSemicolon(trimmed.substr(5));
      CxScriptStatement stmt = MakeStatement(cxssk_call,
                                             call_text,
                                             ExtractCallName(call_text),
                                             current_step,
                                             line_number,
                                             block_depth);
      stmt.callee_name = stmt.name;
      stmt.argument_text = ExtractCallArguments(call_text);
      if (first_call_name.empty())
        first_call_name = stmt.name;
      flow.statements.push_back(stmt);
      block_depth += open_count - close_count;
      continue;
    }

    if (StartsWith(trimmed, "compile("))
    {
      const std::string compile_target = StripQuotes(ExtractCallArguments(trimmed));
      if (compile_target.empty())
      {
        parse_error.message = "compile target is missing";
        parse_error.token = "compile";
        parse_error.line = line_number;
        parse_error.column = 1;
        parse_error.block_depth = block_depth;
        parse_error.step_name = current_step;
        error_message = parse_error.message;
        return false;
      }

      CxScriptStatement stmt = MakeStatement(cxssk_compile,
                                             compile_target,
                                             compile_target,
                                             current_step,
                                             line_number,
                                             block_depth);
      stmt.callee_name = "compile";
      stmt.argument_text = compile_target;
      flow.statements.push_back(stmt);
      block_depth += open_count - close_count;
      continue;
    }

    if (StartsWith(trimmed, "Action.Call(") || StartsWith(trimmed, "Action.UseModuleCase("))
    {
      const std::string call_text = StripTrailingSemicolon(trimmed);
      const std::string action_name = ExtractLegacySingleArgumentValue(call_text);
      CxScriptStatement stmt = MakeStatement(cxssk_action,
                                             call_text,
                                             action_name.empty() ? ExtractCallName(call_text) : action_name,
                                             current_step,
                                             line_number,
                                             block_depth);
      stmt.callee_name = stmt.name;
      stmt.argument_text = action_name.empty() ? ExtractCallArguments(call_text) : action_name;
      if (first_call_name.empty())
        first_call_name = stmt.name;
      flow.statements.push_back(stmt);
      block_depth += open_count - close_count;
      continue;
    }

    if (StartsWith(trimmed, "check("))
    {
      const std::string expect_text = ExtractCallArguments(trimmed);
      if (expect_text.empty())
      {
        parse_error.message = "check expression is missing";
        parse_error.token = "check";
        parse_error.line = line_number;
        parse_error.column = 1;
        parse_error.block_depth = block_depth;
        parse_error.step_name = current_step;
        error_message = parse_error.message;
        return false;
      }

      CxScriptStatement stmt = MakeStatement(cxssk_expect,
                                             expect_text,
                                             "check",
                                             current_step,
                                             line_number,
                                             block_depth);
      ParseExpectExpression(stmt.text, stmt.lhs_text, stmt.operator_text, stmt.rhs_text);
      flow.statements.push_back(stmt);
      block_depth += open_count - close_count;
      continue;
    }

    if (StartsWith(trimmed, "print("))
    {
      const std::string emit_text = ExtractCallArguments(trimmed);
      if (emit_text.empty())
      {
        parse_error.message = "print expression is missing";
        parse_error.token = "print";
        parse_error.line = line_number;
        parse_error.column = 1;
        parse_error.block_depth = block_depth;
        parse_error.step_name = current_step;
        error_message = parse_error.message;
        return false;
      }

      flow.statements.push_back(MakeStatement(cxssk_emit,
                                              emit_text,
                                              "print",
                                              current_step,
                                              line_number,
                                              block_depth));
      block_depth += open_count - close_count;
      continue;
    }

    if (StartsWith(trimmed, "emit "))
    {
      flow.statements.push_back(MakeStatement(cxssk_emit,
                                              StripTrailingSemicolon(trimmed.substr(5)),
                                              std::string(),
                                              current_step,
                                              line_number,
                                              block_depth));
      block_depth += open_count - close_count;
      continue;
    }

    if (StartsWith(trimmed, "action "))
    {
      const std::string action_text = StripTrailingSemicolon(trimmed.substr(7));
      if (action_text.empty())
      {
        parse_error.message = "action name is missing";
        parse_error.token = "action";
        parse_error.line = line_number;
        parse_error.column = 1;
        parse_error.block_depth = block_depth;
        parse_error.step_name = current_step;
        error_message = parse_error.message;
        return false;
      }

      flow.statements.push_back(MakeStatement(cxssk_action,
                                              action_text,
                                              action_text,
                                              current_step,
                                              line_number,
                                              block_depth));
      block_depth += open_count - close_count;
      continue;
    }

    if (StartsWith(trimmed, "require "))
    {
      const std::string require_text = StripTrailingSemicolon(trimmed.substr(8));
      if (require_text.empty())
      {
        parse_error.message = "require expression is missing";
        parse_error.token = "require";
        parse_error.line = line_number;
        parse_error.column = 1;
        parse_error.block_depth = block_depth;
        parse_error.step_name = current_step;
        error_message = parse_error.message;
        return false;
      }

      CxScriptStatement stmt = MakeStatement(cxssk_expect,
                                             require_text,
                                             std::string(),
                                             current_step,
                                             line_number,
                                             block_depth);
      std::vector<std::string> require_args;
      if (TryParseCheckMethodCall(require_text, stmt.name, require_args))
      {
        stmt.argument_text = ExtractCallArguments(require_text);
      }
      else
      {
        stmt.name = require_text;
      }
      ParseExpectExpression(stmt.text, stmt.lhs_text, stmt.operator_text, stmt.rhs_text);
      flow.statements.push_back(stmt);
      block_depth += open_count - close_count;
      continue;
    }

    if (StartsWith(trimmed, "Output.Emit(") || StartsWith(trimmed, "Output.Conclusion("))
    {
      const std::string emit_text = StripTrailingSemicolon(trimmed);
      const std::string emit_name = ExtractLegacySingleArgumentValue(emit_text);
      CxScriptStatement stmt = MakeStatement(cxssk_emit,
                                             emit_name.empty() ? emit_text : emit_name,
                                             emit_name,
                                             current_step,
                                             line_number,
                                             block_depth);
      stmt.argument_text = emit_name;
      flow.statements.push_back(stmt);
      block_depth += open_count - close_count;
      continue;
    }

    if (StartsWith(trimmed, "breakpoint "))
    {
      const std::string breakpoint_name = StripQuotes(StripTrailingSemicolon(trimmed.substr(11)));
      flow.statements.push_back(MakeStatement(cxssk_breakpoint,
                                              breakpoint_name,
                                              breakpoint_name,
                                              current_step,
                                              line_number,
                                              block_depth));
      block_depth += open_count - close_count;
      continue;
    }

    if (StartsWith(trimmed, "checkpoint "))
    {
      const std::string checkpoint_name = StripQuotes(StripTrailingSemicolon(trimmed.substr(11)));
      flow.statements.push_back(MakeStatement(cxssk_checkpoint,
                                              checkpoint_name,
                                              checkpoint_name,
                                              current_step,
                                              line_number,
                                              block_depth));
      block_depth += open_count - close_count;
      continue;
    }

    std::string plain_call_text;
    if (LooksLikePlainCallStatement(trimmed, plain_call_text))
    {
      CxScriptStatement stmt = MakeStatement(cxssk_call,
                                             plain_call_text,
                                             ExtractCallName(plain_call_text),
                                             current_step,
                                             line_number,
                                             block_depth);
      stmt.callee_name = stmt.name;
      stmt.argument_text = ExtractCallArguments(plain_call_text);
      if (first_call_name.empty())
        first_call_name = stmt.name;
      flow.statements.push_back(stmt);
      block_depth += open_count - close_count;
      continue;
    }

    std::string object_lhs_type;
    std::string object_lhs_name;
    std::string object_source_name;
    std::string object_method_name;
    std::string object_argument_text;
    std::string object_initializer_text;
    bool object_declares_value = false;
    if (TryParseObjectReturnAssignment(trimmed,
                                       flow.declared_types,
                                       object_lhs_type,
                                       object_lhs_name,
                                       object_source_name,
                                       object_method_name,
                                       object_argument_text,
                                       object_initializer_text,
                                       object_declares_value))
    {
      CxScriptStatement stmt = MakeStatement(cxssk_var_decl,
                                             (object_lhs_type.empty()
                                                ? object_lhs_name
                                                : object_lhs_type + " " + object_lhs_name),
                                             object_lhs_name,
                                             current_step,
                                             line_number,
                                             block_depth);
      stmt.declared_type = object_lhs_type;
      stmt.initializer_text = object_initializer_text;
      stmt.lhs_object_name = object_lhs_name;
      stmt.lhs_type_name = object_lhs_type;
      stmt.source_object_name = object_source_name;
      stmt.method_name = object_method_name;
      stmt.argument_text = object_argument_text;
      stmt.callee_name = object_source_name + "." + object_method_name;
      stmt.return_object_ref = object_lhs_name + "<=" + object_source_name + "." + object_method_name;
      stmt.returns_object_assignment = true;
      flow.statements.push_back(stmt);

      if (object_declares_value)
      {
        CxScriptVariableDecl variable;
        variable.type_name = object_lhs_type;
        variable.variable_name = object_lhs_name;
        variable.step_name = current_step;
        variable.span = flow.statements.back().span;
        variable.block_depth = block_depth;
        variable.initialized = true;
        variable.known_type = true;
        flow.variables.push_back(variable);
      }

      block_depth += open_count - close_count;
      continue;
    }

    std::string declared_type;
    std::string declared_variable;
    bool initialized = false;
    if (TryParseVariableDeclaration(trimmed,
                                    flow.declared_types,
                                    declared_type,
                                    declared_variable,
                                    initialized))
    {
      flow.statements.push_back(MakeStatement(cxssk_var_decl,
                                              declared_type + " " + declared_variable,
                                              declared_variable,
                                              current_step,
                                              line_number,
                                              block_depth));
      flow.statements.back().declared_type = declared_type;
      flow.statements.back().initializer_text = initialized ?
        Trim(Trim(trimmed.substr(0, trimmed.size() - 1)).substr(Trim(trimmed.substr(0, trimmed.size() - 1)).find('=') + 1)) :
        std::string();

      CxScriptVariableDecl variable;
      variable.type_name = declared_type;
      variable.variable_name = declared_variable;
      variable.step_name = current_step;
      variable.span = flow.statements.back().span;
      variable.block_depth = block_depth;
      variable.initialized = initialized;
      variable.known_type = true;
      flow.variables.push_back(variable);

      block_depth += open_count - close_count;
      continue;
    }

    std::string unknown_type_name;
    std::string unknown_variable_name;
    if (LooksLikeTypeDeclaration(trimmed, unknown_type_name, unknown_variable_name))
    {
      parse_error.message = "unknown type in declaration";
      parse_error.token = unknown_type_name;
      parse_error.line = line_number;
      parse_error.column = 1;
      parse_error.block_depth = block_depth;
      parse_error.step_name = current_step;
      error_message = parse_error.message;
      return false;
    }

    const size_t split = trimmed.find('=');
    std::string metadata_field;
    std::string metadata_value;
    if (TryParseHeaderMetadataFunction(trimmed, metadata_field, metadata_value))
    {
      ApplyHeaderMetadataField(context, metadata_field, metadata_value);
      flow.statements.push_back(MakeHeaderMetadataStatement(metadata_field,
                                                            metadata_value,
                                                            StripTrailingSemicolon(trimmed),
                                                            line_number));
      block_depth += open_count - close_count;
      continue;
    }

    if (split == std::string::npos)
    {
      flow.statements.push_back(MakeStatement(cxssk_action,
                                              trimmed,
                                              std::string(),
                                              current_step,
                                              line_number,
                                              block_depth));
      block_depth += open_count - close_count;
      continue;
    }

    const std::string key = Trim(trimmed.substr(0, split));
    const std::string value = Trim(trimmed.substr(split + 1));
    bool handled_metadata = ApplyHeaderMetadataField(context, key, value);

    if (key == "route")
      context.route = value;
    else if (key == "trace_id")
      context.trace_id = value;

    if (handled_metadata)
    {
      flow.statements.push_back(MakeHeaderMetadataStatement(key,
                                                            StripQuotes(value),
                                                            key + "=" + value,
                                                            line_number));
    }
    else
    {
      flow.statements.push_back(MakeStatement(cxssk_action,
                                              StripTrailingSemicolon(trimmed),
                                              std::string(),
                                              current_step,
                                              line_number,
                                              block_depth));
    }

    block_depth += open_count - close_count;
  }

  if (context.layer.empty())
  {
    error_message = "cxscript layer is missing";
    return false;
  }
  if (context.kind.empty() && !context.module.empty())
    context.kind = "module";
  if (context.kind.empty())
  {
    error_message = "cxscript kind is missing";
    return false;
  }
  if (context.mode.empty())
    context.mode = "build-run";

  if (context.kind == "module" && context.module.empty())
  {
    error_message = "module cxscript requires module name";
    return false;
  }

  if (context.kind == "integration" && context.module.empty())
  {
    error_message = "integration cxscript requires owner module in first-stage runtime";
    return false;
  }

  if (context.case_name.empty() && !context.module.empty() && !first_call_name.empty())
    context.case_name = ResolveCxScriptCallAlias(context.module, first_call_name);

  if (context.case_name.empty())
  {
    error_message = "cxscript case is missing";
    return false;
  }

  if (block_depth != 0)
  {
    parse_error.message = "unterminated block boundary";
    parse_error.token = "{";
    parse_error.line = line_number;
    parse_error.column = 1;
    parse_error.block_depth = block_depth;
    parse_error.step_name = current_step;
    error_message = parse_error.message;
    return false;
  }

  return true;
}

bool ParserCxScriptRuntime::BuildTestRequest(const CxScriptExecutionContext &context,
                                             const CxScriptFlow &flow,
                                             ParserTestRequest &request,
                                             std::string &error_message)
{
  if (context.kind != "module" && context.kind != "integration")
  {
    error_message = "unsupported cxscript kind";
    return false;
  }

  request = ParserTestRequest();
  request.layer = context.layer;
  request.module = context.module == "torch_module" ? "torch" : context.module;
  if (request.module.empty() &&
      context.case_name.find("torch.") == 0)
    request.module = "torch";
  request.case_name = context.case_name;
  request.mode = context.mode;
  request.report_on = context.report_on;
  CollectFlowHostInputs(flow, request);
  return true;
}

bool ApplyFlowImpl(const CxScriptFlow &flow,
                   CxScriptExecutionResult &result,
                   std::string &error_message,
                   bool preview_only,
                   bool collect_debug)
{
  std::map<std::string, std::string> variable_values;
  int next_step_id = 1;
  int next_frame_id = 1;
  int current_step_id = 0;
  std::vector<int> frame_stack;
  frame_stack.push_back(0);
  result.execution_summary = CxScriptExecutionSummary();

  for (size_t i = 0; i < flow.statements.size(); ++i)
  {
    CxScriptStatement stmt = flow.statements[i];
    const std::string action_label = StatementActionLabel(stmt);
    const CxScriptExecutionStepKind step_kind = StatementStepKind(stmt, action_label);
    const std::string control_tag = StatementControlTag(stmt, action_label);

    // Metadata header step: kept in execution/source/replay for debugability.
    // It is not a normal call/action, and currently accepts both key=value and name(...).
    // In replay/debug, read it as "header metadata applied here" rather than runtime work.
    if (stmt.kind == cxssk_step)
      current_step_id = next_step_id++;

    if (stmt.kind == cxssk_block_boundary && action_label == "frame_enter")
      frame_stack.push_back(next_frame_id++);

    stmt.step_id = current_step_id;
    stmt.frame_id = frame_stack.empty() ? 0 : frame_stack.back();
    result.last_step_id = stmt.step_id;
    result.last_frame_id = stmt.frame_id;
    result.last_source_line = stmt.span.line_begin;

    if (stmt.step_id > result.execution_summary.max_step_id)
      result.execution_summary.max_step_id = stmt.step_id;
    if (stmt.frame_id > result.execution_summary.max_frame_id)
      result.execution_summary.max_frame_id = stmt.frame_id;
    if (stmt.block_depth > result.execution_summary.max_block_depth)
      result.execution_summary.max_block_depth = stmt.block_depth;
    if (result.execution_summary.entry_step_id == 0 && step_kind == cxsesk_step)
      result.execution_summary.entry_step_id = stmt.step_id;
    if (result.execution_summary.check_step_id == 0 &&
        step_kind == cxsesk_step &&
        stmt.step_name == "check")
      result.execution_summary.check_step_id = stmt.step_id;
    ++result.execution_summary.step_count;

    CxScriptReplayFrame replay_frame;
    replay_frame.sequence = static_cast<int>(result.replay_frames.size()) + 1;
    replay_frame.previous_sequence = replay_frame.sequence > 1 ? replay_frame.sequence - 1 : 0;
    replay_frame.next_sequence = 0;
    replay_frame.step_id = stmt.step_id;
    replay_frame.frame_id = stmt.frame_id;
    replay_frame.step_name = stmt.step_name;
    replay_frame.action = action_label;
    replay_frame.status = "ok";
    replay_frame.span = stmt.span;

    CxScriptExecutionStepView execution_step;
    execution_step.sequence = replay_frame.sequence;
    execution_step.step_id = stmt.step_id;
    execution_step.frame_id = stmt.frame_id;
    execution_step.previous_sequence = replay_frame.previous_sequence;
    execution_step.next_sequence = replay_frame.next_sequence;
    execution_step.kind = step_kind;
    execution_step.step_name = stmt.step_name;
    execution_step.action = action_label;
    execution_step.control_tag = control_tag;
    execution_step.payload = stmt.text;
    execution_step.span = stmt.span;
    execution_step.block_depth = stmt.block_depth;

    CxScriptExecutionOp exec_op;
    exec_op.sequence = static_cast<int>(result.execution_ops.size()) + 1;
    exec_op.step_id = stmt.step_id;
    exec_op.frame_id = stmt.frame_id;
    exec_op.opcode = StatementOpcode(stmt.kind);
    exec_op.step_name = stmt.step_name;
    exec_op.payload = stmt.text;
    if (action_label == "check")
    {
      exec_op.payload = "check(" + stmt.text + ")";
      execution_step.payload = exec_op.payload;
    }
    else if (action_label == "print")
    {
      exec_op.payload = "print(" + stmt.text + ")";
      execution_step.payload = exec_op.payload;
    }
    exec_op.span = stmt.span;
    exec_op.block_depth = stmt.block_depth;
    result.last_sequence = exec_op.sequence;
    if (exec_op.sequence > result.execution_summary.max_sequence)
      result.execution_summary.max_sequence = exec_op.sequence;

    if (collect_debug)
    {
      CxScriptSourceMapEntry source_entry;
      source_entry.step_id = stmt.step_id;
      source_entry.frame_id = stmt.frame_id;
      source_entry.step_name = stmt.step_name;
      source_entry.statement_kind = action_label;
      source_entry.span = stmt.span;
      source_entry.block_depth = stmt.block_depth;
      result.source_map.push_back(source_entry);
    }

    switch (step_kind)
    {
    case cxsesk_header_metadata:
      ++result.execution_summary.header_step_count;
      break;
    case cxsesk_frame_enter:
    case cxsesk_frame_exit:
      ++result.execution_summary.frame_step_count;
      break;
    case cxsesk_call:
      ++result.execution_summary.call_step_count;
      break;
    case cxsesk_compile:
      ++result.execution_summary.compile_step_count;
      break;
    case cxsesk_check:
      ++result.execution_summary.check_step_count;
      break;
    case cxsesk_print:
      ++result.execution_summary.print_step_count;
      break;
    case cxsesk_breakpoint:
      ++result.execution_summary.breakpoint_step_count;
      break;
    case cxsesk_checkpoint:
      ++result.execution_summary.checkpoint_step_count;
      break;
    default:
      break;
    }

    if (stmt.kind == cxssk_step)
    {
      if (collect_debug)
      {
        CxScriptStepTrace trace;
        trace.step_id = stmt.step_id;
        trace.frame_id = stmt.frame_id;
        trace.step_name = stmt.name;
        trace.span = stmt.span;
        trace.block_depth = stmt.block_depth;
        trace.status = "entered";
        result.step_traces.push_back(trace);
        result.execution_ops.push_back(exec_op);
        if (!result.replay_frames.empty())
        {
          result.replay_frames.back().next_sequence = replay_frame.sequence;
          result.execution_steps.back().next_sequence = execution_step.sequence;
        }
        result.replay_frames.push_back(replay_frame);
        result.execution_steps.push_back(execution_step);
      }
      AppendRuntimeDetail(result, "[STEP] " + stmt.text, collect_debug, false);
      continue;
    }

    if (stmt.kind == cxssk_header_metadata)
    {
      if (collect_debug)
      {
        result.execution_ops.push_back(exec_op);
        if (!result.replay_frames.empty())
        {
          result.replay_frames.back().next_sequence = replay_frame.sequence;
          result.execution_steps.back().next_sequence = execution_step.sequence;
        }
        result.replay_frames.push_back(replay_frame);
        result.execution_steps.push_back(execution_step);
      }
      AppendRuntimeDetail(result, "[HEADER] " + stmt.lhs_text + "=" + stmt.rhs_text, collect_debug, false);
      continue;
    }

    if (stmt.kind == cxssk_block_boundary)
    {
      replay_frame.status = action_label == "frame_exit" ? "exit" : "enter";
      if (collect_debug)
      {
        result.execution_ops.push_back(exec_op);
        if (!result.replay_frames.empty())
        {
          result.replay_frames.back().next_sequence = replay_frame.sequence;
          result.execution_steps.back().next_sequence = execution_step.sequence;
        }
        result.replay_frames.push_back(replay_frame);
        result.execution_steps.push_back(execution_step);
      }
      AppendRuntimeDetail(result,
                          action_label == "frame_exit" ? "[FRAME_EXIT] " + stmt.text : "[FRAME_ENTER] " + stmt.text,
                          collect_debug,
                          false);
      if (action_label == "frame_exit" && frame_stack.size() > 1)
        frame_stack.pop_back();
      continue;
    }

    if (stmt.kind == cxssk_type_decl)
    {
      if (collect_debug)
      {
        result.execution_ops.push_back(exec_op);
        if (!result.replay_frames.empty())
        {
          result.replay_frames.back().next_sequence = replay_frame.sequence;
          result.execution_steps.back().next_sequence = execution_step.sequence;
        }
        result.replay_frames.push_back(replay_frame);
        result.execution_steps.push_back(execution_step);
      }
      AppendRuntimeDetail(result, "[TYPE] " + stmt.text, collect_debug, false);
      continue;
    }

    if (stmt.kind == cxssk_use)
    {
      if (collect_debug)
      {
        result.execution_ops.push_back(exec_op);
        if (!result.replay_frames.empty())
        {
          result.replay_frames.back().next_sequence = replay_frame.sequence;
          result.execution_steps.back().next_sequence = execution_step.sequence;
        }
        result.replay_frames.push_back(replay_frame);
        result.execution_steps.push_back(execution_step);
      }
      AppendRuntimeDetail(result, "[USE] " + stmt.text, collect_debug, false);
      continue;
    }

    if (stmt.kind == cxssk_var_decl)
    {
      if (!stmt.name.empty())
      {
        std::string value;
        if (stmt.returns_object_assignment)
        {
          value = stmt.return_object_ref.empty()
                    ? (stmt.name + "<=" + stmt.callee_name)
                    : stmt.return_object_ref;
        }
        else if (!stmt.initializer_text.empty())
        {
          value = ResolveScriptValue(result, variable_values, stmt.initializer_text);
        }
        variable_values[stmt.name] = value;
      }
      if (collect_debug)
      {
        result.execution_ops.push_back(exec_op);
        if (!result.replay_frames.empty())
        {
          result.replay_frames.back().next_sequence = replay_frame.sequence;
          result.execution_steps.back().next_sequence = execution_step.sequence;
        }
        result.replay_frames.push_back(replay_frame);
        result.execution_steps.push_back(execution_step);
      }
      if (stmt.returns_object_assignment)
      {
        AppendRuntimeDetail(result,
                            "[OBJECT_ASSIGN] lhs=" + stmt.name +
                              (stmt.lhs_type_name.empty() ? std::string() :
                               " type=" + stmt.lhs_type_name) +
                              " source=" + stmt.source_object_name +
                              " method=" + stmt.method_name +
                              (stmt.argument_text.empty() ? std::string() :
                               " args=" + stmt.argument_text) +
                              " return_ref=" + variable_values[stmt.name],
                            collect_debug,
                            true);
      }
      AppendRuntimeDetail(result,
                          "[VAR] " + stmt.text +
                            (stmt.name.empty() ? std::string() :
                             " => " + stmt.name + "=" + variable_values[stmt.name]),
                          collect_debug,
                          false);
      continue;
    }
    if (stmt.kind == cxssk_input)
    {
      std::string assignment_lhs;
      std::string assignment_rhs;
      if (LooksLikeAssignmentExpr(stmt.text, assignment_lhs, assignment_rhs))
        variable_values[assignment_lhs] = ResolveScriptValue(result, variable_values, assignment_rhs);
      if (collect_debug)
      {
        result.execution_ops.push_back(exec_op);
        if (!result.replay_frames.empty())
        {
          result.replay_frames.back().next_sequence = replay_frame.sequence;
          result.execution_steps.back().next_sequence = execution_step.sequence;
        }
        result.replay_frames.push_back(replay_frame);
        result.execution_steps.push_back(execution_step);
      }
      AppendRuntimeDetail(result,
                          "[INPUT] " + stmt.text +
                            (assignment_lhs.empty() ? std::string() :
                             " => " + assignment_lhs + "=" + variable_values[assignment_lhs]),
                          collect_debug,
                          false);
      continue;
    }

    if (stmt.kind == cxssk_call)
    {
      bool bridge_applied = false;
      if (!preview_only)
      {
        bridge_applied =
          ApplyCxcoreFeatureActionBridge(detail::BuildResultContext(result), stmt, result) ||
          ApplyMlpackBaselineActionBridge(detail::BuildResultContext(result), stmt, result);
        if (bridge_applied)
          RefreshNamedResultViews(result);
      }
      if (collect_debug)
      {
        result.execution_ops.push_back(exec_op);
        if (!result.replay_frames.empty())
        {
          result.replay_frames.back().next_sequence = replay_frame.sequence;
          result.execution_steps.back().next_sequence = execution_step.sequence;
        }
        result.replay_frames.push_back(replay_frame);
        result.execution_steps.push_back(execution_step);
      }
      AppendRuntimeDetail(result, "[CALL] " + stmt.text, collect_debug, false);
      continue;
    }

    if (stmt.kind == cxssk_compile)
    {
      bool known_stage = false;
      for (size_t stage_index = 0; stage_index < result.compiled_stage_names.size(); ++stage_index)
      {
        if (result.compiled_stage_names[stage_index] == stmt.argument_text)
        {
          known_stage = true;
          break;
        }
      }
      if (!known_stage)
        result.compiled_stage_names.push_back(stmt.argument_text);
      RefreshNamedResultViews(result);
      if (collect_debug)
      {
        result.execution_ops.push_back(exec_op);
        if (!result.replay_frames.empty())
        {
          result.replay_frames.back().next_sequence = replay_frame.sequence;
          result.execution_steps.back().next_sequence = execution_step.sequence;
        }
        result.replay_frames.push_back(replay_frame);
        result.execution_steps.push_back(execution_step);
      }
      AppendRuntimeDetail(result, "[COMPILE] " + stmt.argument_text, collect_debug, true);
      continue;
    }

    if (stmt.kind == cxssk_action)
    {
      bool bridge_applied = false;
      if (!preview_only)
      {
        CxScriptStatement action_stmt = stmt;
        action_stmt.callee_name = stmt.name;
        bridge_applied =
          ApplyCxcoreFeatureActionBridge(detail::BuildResultContext(result), action_stmt, result) ||
          ApplyMlpackBaselineActionBridge(detail::BuildResultContext(result), action_stmt, result);
        if (bridge_applied)
          RefreshNamedResultViews(result);
      }
      std::string assignment_lhs;
      std::string assignment_rhs;
      if (LooksLikeAssignmentExpr(stmt.text, assignment_lhs, assignment_rhs))
      {
        variable_values[assignment_lhs] = ResolveScriptValue(result, variable_values, assignment_rhs);
      }
      if (collect_debug)
      {
        result.execution_ops.push_back(exec_op);
        if (!result.replay_frames.empty())
        {
          result.replay_frames.back().next_sequence = replay_frame.sequence;
          result.execution_steps.back().next_sequence = execution_step.sequence;
        }
        result.replay_frames.push_back(replay_frame);
        result.execution_steps.push_back(execution_step);
      }
      AppendRuntimeDetail(result,
                          "[ACTION] " + stmt.text +
                            (assignment_lhs.empty() ? std::string() :
                             " => " + assignment_lhs + "=" + variable_values[assignment_lhs]),
                          collect_debug,
                          false);
      continue;
    }

    if (stmt.kind == cxssk_emit)
    {
      std::string emitted = stmt.text;
      if (preview_only)
      {
        emitted = stmt.text;
      }
      else if (emitted.size() >= 2 && emitted.front() == '"' && emitted.back() == '"')
        emitted = StripQuotes(emitted);
      else
      {
        std::string field_value = ResolveScriptValue(result, variable_values, emitted);
        if (!field_value.empty())
          emitted = field_value;
      }
      if (collect_debug)
      {
        result.execution_ops.push_back(exec_op);
        if (!result.replay_frames.empty())
        {
          result.replay_frames.back().next_sequence = replay_frame.sequence;
          result.execution_steps.back().next_sequence = execution_step.sequence;
        }
        result.replay_frames.push_back(replay_frame);
        result.execution_steps.push_back(execution_step);
      }
      AppendRuntimeDetail(result,
                          action_label == "print" ? "[PRINT] " + emitted : "[EMIT] " + emitted,
                          collect_debug,
                          true);
      continue;
    }

    if (stmt.kind == cxssk_expect)
    {
      if (preview_only)
      {
        replay_frame.status = "preview";
        if (collect_debug)
        {
          result.execution_ops.push_back(exec_op);
          if (!result.replay_frames.empty())
          {
            result.replay_frames.back().next_sequence = replay_frame.sequence;
            result.execution_steps.back().next_sequence = execution_step.sequence;
          }
          result.replay_frames.push_back(replay_frame);
          result.execution_steps.push_back(execution_step);
        }
        AppendRuntimeDetail(result,
                            action_label == "check" ? "[CHECK] " + stmt.text : "[EXPECT] " + stmt.text,
                            collect_debug,
                            false);
        continue;
      }

      if (stmt.operator_text.empty() && stmt.name != "check")
      {
        bool method_ok = false;
        std::string method_detail;
        if (TryHandleTorchHostStatement(stmt, result, variable_values, method_detail))
        {
          AppendRuntimeDetail(result, method_detail, collect_debug, true);
          replay_frame.status = "ok";
          execution_step.next_sequence = replay_frame.next_sequence;
          if (collect_debug)
          {
            result.execution_ops.push_back(exec_op);
            if (!result.replay_frames.empty())
            {
              result.replay_frames.back().next_sequence = replay_frame.sequence;
              result.execution_steps.back().next_sequence = execution_step.sequence;
            }
            result.replay_frames.push_back(replay_frame);
            result.execution_steps.push_back(execution_step);
          }
          continue;
        }

        if (!EvaluateCheckMethod(stmt, result, variable_values, method_ok, method_detail))
        {
          SetStatementError(stmt,
                            method_detail.empty() ? "unsupported Check.* expression" : method_detail,
                            result,
                            error_message);
          return false;
        }

        AppendRuntimeDetail(result,
                            "[CHECK] " + stmt.text + (method_ok ? " => pass" : " => fail"),
                            collect_debug,
                            !method_ok);
        replay_frame.status = method_ok ? "ok" : "failed";
        execution_step.next_sequence = replay_frame.next_sequence;
        if (collect_debug)
        {
          result.execution_ops.push_back(exec_op);
          if (!result.replay_frames.empty())
          {
            result.replay_frames.back().next_sequence = replay_frame.sequence;
            result.execution_steps.back().next_sequence = execution_step.sequence;
          }
          result.replay_frames.push_back(replay_frame);
          result.execution_steps.push_back(execution_step);
        }
        if (!method_ok)
        {
          SetStatementError(stmt,
                            "cxscript Check.* failed: " + stmt.text,
                            result,
                            error_message);
          result.success = false;
          result.summary = error_message;
          return false;
        }
        continue;
      }

      const size_t eq = stmt.text.find("==");
      const size_t ne = stmt.text.find("!=");
      const size_t ge = stmt.text.find(">=");
      const size_t le = stmt.text.find("<=");
      const size_t gt = stmt.text.find('>');
      const size_t lt = stmt.text.find('<');
      size_t op_pos = std::string::npos;
      const char *op = 0;
      if (eq != std::string::npos)
      {
        op_pos = eq;
        op = "==";
      }
      else if (ne != std::string::npos)
      {
        op_pos = ne;
        op = "!=";
      }
      else if (ge != std::string::npos)
      {
        op_pos = ge;
        op = ">=";
      }
      else if (le != std::string::npos)
      {
        op_pos = le;
        op = "<=";
      }
      else if (gt != std::string::npos)
      {
        op_pos = gt;
        op = ">";
      }
      else if (lt != std::string::npos)
      {
        op_pos = lt;
        op = "<";
      }

      if (op_pos == std::string::npos || !op)
      {
        SetStatementError(stmt,
                          "unsupported expect expression: " + stmt.text,
                          result,
                          error_message);
        return false;
      }

      const std::string lhs_key = Trim(stmt.text.substr(0, op_pos));
      const std::string lhs_value = ResolveScriptValue(result, variable_values, lhs_key);
      std::string rhs_value = Trim(stmt.text.substr(op_pos + std::string(op).size()));
      rhs_value = ResolveScriptValue(result, variable_values, rhs_value);

      bool ok = false;
      if (std::string(op) == "==" || std::string(op) == "!=")
      {
        ok = EvaluateEquals(lhs_value, rhs_value, op);
      }
      else
      {
        double lhs_number = 0.0;
        double rhs_number = 0.0;
        if (!TryParseDouble(lhs_value, lhs_number) || !TryParseDouble(rhs_value, rhs_number))
        {
          SetStatementError(stmt,
                            "numeric expect requires numeric field/value: " + stmt.text,
                            result,
                            error_message);
          return false;
        }
        ok = EvaluateNumeric(lhs_number, rhs_number, op);
      }

      AppendRuntimeDetail(result,
                          std::string(action_label == "check" ? "[CHECK] " : "[EXPECT] ") +
                            stmt.text +
                            (ok ? " => pass" : " => fail"),
                          collect_debug,
                          !ok);
      replay_frame.status = ok ? "ok" : "failed";
      execution_step.next_sequence = replay_frame.next_sequence;
      if (collect_debug)
      {
        result.execution_ops.push_back(exec_op);
        if (!result.replay_frames.empty())
        {
          result.replay_frames.back().next_sequence = replay_frame.sequence;
          result.execution_steps.back().next_sequence = execution_step.sequence;
        }
        result.replay_frames.push_back(replay_frame);
        result.execution_steps.push_back(execution_step);
      }
      if (!ok)
      {
        SetStatementError(stmt,
                          "cxscript expect failed: " + stmt.text,
                          result,
                          error_message);
        result.success = false;
        result.summary = error_message;
        return false;
      }
      continue;
    }

    if (stmt.kind == cxssk_breakpoint)
    {
      CxScriptBreakpointRecord breakpoint;
      breakpoint.step_id = stmt.step_id;
      breakpoint.frame_id = stmt.frame_id;
      breakpoint.name = stmt.name;
      breakpoint.step_name = stmt.step_name;
      breakpoint.span = stmt.span;
      breakpoint.block_depth = stmt.block_depth;
      if (collect_debug)
      {
        result.breakpoints.push_back(breakpoint);
        result.execution_ops.push_back(exec_op);
        if (!result.replay_frames.empty())
        {
          result.replay_frames.back().next_sequence = replay_frame.sequence;
          result.execution_steps.back().next_sequence = execution_step.sequence;
        }
        result.replay_frames.push_back(replay_frame);
        result.execution_steps.push_back(execution_step);
      }
      AppendRuntimeDetail(result, "[BREAKPOINT] " + stmt.text, collect_debug, false);
      continue;
    }

    if (stmt.kind == cxssk_checkpoint)
    {
      CxScriptCheckpointRecord checkpoint;
      checkpoint.step_id = stmt.step_id;
      checkpoint.frame_id = stmt.frame_id;
      checkpoint.name = stmt.name;
      checkpoint.step_name = stmt.step_name;
      checkpoint.span = stmt.span;
      checkpoint.block_depth = stmt.block_depth;
      if (collect_debug)
      {
        result.checkpoints.push_back(checkpoint);
        result.execution_ops.push_back(exec_op);
        if (!result.replay_frames.empty())
        {
          result.replay_frames.back().next_sequence = replay_frame.sequence;
          result.execution_steps.back().next_sequence = execution_step.sequence;
        }
        result.replay_frames.push_back(replay_frame);
        result.execution_steps.push_back(execution_step);
      }
      AppendRuntimeDetail(result, "[CHECKPOINT] " + stmt.text, collect_debug, false);
      continue;
    }
  }
  if (collect_debug)
    RefreshExecutionSummary(result);
  else
    RefreshExecutionDiagnostics(result);
  return true;
}

bool ParserCxScriptRuntime::ConvertTestResult(const CxScriptExecutionContext &context,
                                              const ParserTestRunResult &test_result,
                                              CxScriptExecutionResult &script_result)
{
  script_result = CxScriptExecutionResult();
  script_result.success = test_result.success;
  script_result.train_ok = false;
  script_result.infer_ok = false;
  script_result.score_ok = false;
  script_result.script_path = context.script_path;
  script_result.script_name = context.script_name;
  script_result.kind = context.kind;
  script_result.layer = test_result.layer.empty() ? context.layer : test_result.layer;
  script_result.module = test_result.module.empty() ? context.module : test_result.module;
  script_result.case_name = test_result.case_name.empty() ? context.case_name : test_result.case_name;
  script_result.route = test_result.route.empty() ? context.route : test_result.route;
  script_result.task_id = test_result.task_id;
  script_result.scalar_result = test_result.scalar_result;
  script_result.runtime_ms = test_result.runtime_ms;
  script_result.bridge_enabled = test_result.bridge_enabled;
  script_result.line_horizontal_samples_contract_value = test_result.line_horizontal_samples_contract_value;
  script_result.line_vertical_samples_contract_value = test_result.line_vertical_samples_contract_value;
  script_result.line_measure_bounds_contract_value = test_result.line_measure_bounds_contract_value;
  script_result.circle_center_contract_value = test_result.circle_center_contract_value;
  script_result.circle_radius_contract_value = test_result.circle_radius_contract_value;
  script_result.circle_avg_distance_contract_value = test_result.circle_avg_distance_contract_value;
  script_result.template_candidate_count_contract_value = test_result.template_candidate_count_contract_value;
  script_result.template_top_score_contract_value = test_result.template_top_score_contract_value;
  script_result.template_match_center_contract_value = test_result.template_match_center_contract_value;
  script_result.template_min_candidate_count_contract_value =
    test_result.template_min_candidate_count_contract_value;
  script_result.template_min_top_score_contract_value =
    test_result.template_min_top_score_contract_value;
  script_result.region_connected_components_contract_value = test_result.region_connected_components_contract_value;
  script_result.region_size_contract_value = test_result.region_size_contract_value;
  script_result.region_bounds_contract_value = test_result.region_bounds_contract_value;
  script_result.region_min_connected_components_contract_value =
    test_result.region_min_connected_components_contract_value;
  script_result.region_min_bounds_count_contract_value =
    test_result.region_min_bounds_count_contract_value;
  script_result.point_count_value = test_result.point_count_value;
  script_result.line_chain_length_value = test_result.line_chain_length_value;
  script_result.line_edgeband_count_value = test_result.line_edgeband_count_value;
  script_result.fit_error_avg_value = test_result.fit_error_avg_value;
  script_result.fit_error_max_value = test_result.fit_error_max_value;
  script_result.line_angle_value = test_result.line_angle_value;
  script_result.line_offset_value = test_result.line_offset_value;
  script_result.subpixel_adjust_avg_value = test_result.subpixel_adjust_avg_value;
  script_result.circle_center_x_value = test_result.circle_center_x_value;
  script_result.circle_center_y_value = test_result.circle_center_y_value;
  script_result.circle_radius_value = test_result.circle_radius_value;
  script_result.circle_avg_distance_value = test_result.circle_avg_distance_value;
  script_result.circle_sample_points_value = test_result.circle_sample_points_value;
  script_result.circle_used_fallback_value = test_result.circle_used_fallback_value;
  script_result.circle_prefilter_used_value = test_result.circle_prefilter_used_value;
  script_result.circle_compact_path_value = test_result.circle_compact_path_value;
  script_result.circle_failure_stage = test_result.circle_failure_stage;
  script_result.match_candidate_count_value = test_result.match_candidate_count_value;
  script_result.match_selected_index_value = test_result.match_selected_index_value;
  script_result.match_best_index_value = test_result.match_best_index_value;
  script_result.candidate_count_value = test_result.candidate_count_value;
  script_result.selected_candidate_index_value = test_result.selected_candidate_index_value;
  script_result.selected_candidate_score_value = test_result.selected_candidate_score_value;
  script_result.score_total_value = test_result.score_total_value;
  script_result.match_top_score_value = test_result.match_top_score_value;
  script_result.match_max_score_value = test_result.match_max_score_value;
  script_result.match_center_x_value = test_result.match_center_x_value;
  script_result.match_center_y_value = test_result.match_center_y_value;
  script_result.match_best_rect_x_value = test_result.match_best_rect_x_value;
  script_result.match_best_rect_y_value = test_result.match_best_rect_y_value;
  script_result.match_best_rect_w_value = test_result.match_best_rect_w_value;
  script_result.match_best_rect_h_value = test_result.match_best_rect_h_value;
  script_result.template_used_fallback_value = test_result.template_used_fallback_value;
  script_result.roi_area_value = test_result.roi_area_value;
  script_result.component_count_value = test_result.component_count_value;
  script_result.image_model_score_value = test_result.image_model_score_value;
  script_result.baseline_roi_area_value = test_result.baseline_roi_area_value;
  script_result.baseline_component_count_value = test_result.baseline_component_count_value;
  script_result.baseline_match_best_score_value = test_result.baseline_match_best_score_value;
  script_result.baseline_image_model_score_value = test_result.baseline_image_model_score_value;
  script_result.roi_patch_tensor_value = test_result.roi_patch_tensor_value;
  script_result.roi_patch_count_value = test_result.roi_patch_count_value;
  script_result.roi_patch_spatial_size_value = test_result.roi_patch_spatial_size_value;
  script_result.roi_class_label_value = test_result.roi_class_label_value;
  script_result.roi_class_label_count_value = test_result.roi_class_label_count_value;
  script_result.region_tensor_value = test_result.region_tensor_value;
  script_result.region_spatial_size_value = test_result.region_spatial_size_value;
  script_result.region_channel_layout_value = test_result.region_channel_layout_value;
  script_result.mask_or_region_label_value = test_result.mask_or_region_label_value;
  script_result.mask_label_spatial_size_value = test_result.mask_label_spatial_size_value;
  script_result.roi_alignment_status_value = test_result.roi_alignment_status_value;
  script_result.mask_alignment_status_value = test_result.mask_alignment_status_value;
  script_result.fractal_partition_value = test_result.fractal_partition_value;
  script_result.distance_field_value = test_result.distance_field_value;
  script_result.skeleton_mask_value = test_result.skeleton_mask_value;
  script_result.centerline_paths_value = test_result.centerline_paths_value;
  script_result.topology_repair_paths_value = test_result.topology_repair_paths_value;
  script_result.baseline_roi_patch_count_value = test_result.baseline_roi_patch_count_value;
  script_result.baseline_roi_alignment_status_value =
    test_result.baseline_roi_alignment_status_value;
  script_result.baseline_mask_alignment_status_value =
    test_result.baseline_mask_alignment_status_value;
  script_result.baseline_export_contract_value = test_result.baseline_export_contract_value;
  script_result.region_pattern_foreground_ratio_value = test_result.region_pattern_foreground_ratio_value;
  script_result.region_pattern_descriptor_dim_value = test_result.region_pattern_descriptor_dim_value;
  script_result.region_pattern_descriptor_mean_value = test_result.region_pattern_descriptor_mean_value;
  script_result.region_pattern_descriptor_std_value = test_result.region_pattern_descriptor_std_value;
  script_result.template_learn_path_a_count_value = test_result.template_learn_path_a_count_value;
  script_result.template_learn_path_b_count_value = test_result.template_learn_path_b_count_value;
  script_result.template_main_candidate_count_value = test_result.template_main_candidate_count_value;
  script_result.template_main_top_score_value = test_result.template_main_top_score_value;
  script_result.region_connected_components_value = test_result.region_connected_components_value;
  script_result.region_width_value = test_result.region_width_value;
  script_result.region_height_value = test_result.region_height_value;
  script_result.region_bounds_count_value = test_result.region_bounds_count_value;
  script_result.region_raw_connected_components_value = test_result.region_raw_connected_components_value;
  script_result.region_foreground_ratio_value = test_result.region_foreground_ratio_value;
  script_result.published_handoff_type = test_result.published_handoff_type;
  script_result.published_primary_ref = test_result.published_primary_ref;
  script_result.published_route_hint = test_result.published_route_hint;
  script_result.published_route_state = test_result.published_route_state;
  script_result.published_source_hash = test_result.published_source_hash;
  script_result.published_result_ref = test_result.published_result_ref;
  script_result.published_evidence_ref = test_result.published_evidence_ref;
  script_result.published_bbox_candidate_list_ref = test_result.published_bbox_candidate_list_ref;
  script_result.published_template_alignment_ref = test_result.published_template_alignment_ref;
  script_result.published_template_test_alignment_status = test_result.published_template_test_alignment_status;
  script_result.published_roi_diff_candidate_ref = test_result.published_roi_diff_candidate_ref;
  script_result.published_roi_diff_candidate_count = test_result.published_roi_diff_candidate_count;
  script_result.published_prior_roi_region_ref = test_result.published_prior_roi_region_ref;
  script_result.published_roi_crop_packet_ref = test_result.published_roi_crop_packet_ref;
  script_result.published_roi_crop_count = test_result.published_roi_crop_count;
  script_result.published_roi_crop_spatial_size = test_result.published_roi_crop_spatial_size;
  script_result.published_roi_crop_policy_ref = test_result.published_roi_crop_policy_ref;
  script_result.internal_test_interface_name = test_result.internal_test_interface_name;
  script_result.internal_test_interface_purpose = test_result.internal_test_interface_purpose;
  script_result.execution_stage_0 = test_result.execution_stage_0;
  script_result.execution_stage_1 = test_result.execution_stage_1;
  script_result.execution_stage_2 = test_result.execution_stage_2;
  script_result.execution_stage_3 = test_result.execution_stage_3;
  script_result.metrics = test_result.metrics;
  script_result.result_object = test_result.result_object;
  script_result.input_dataset = test_result.input_dataset;
  script_result.input_sample = test_result.input_sample;
  script_result.input_split = test_result.input_split;
  script_result.input_artifacts = test_result.input_artifacts;
  script_result.input_params = test_result.input_params;
  script_result.dataset_ref = test_result.dataset_ref;
  script_result.sample_bundle_ref = test_result.sample_bundle_ref;
  script_result.objective_ref = test_result.objective_ref;
  script_result.optimization_result_ref = test_result.optimization_result_ref;
  script_result.best_params_ref = test_result.best_params_ref;
  script_result.objective_delta_ref = test_result.objective_delta_ref;
  script_result.summary_ref = test_result.summary_ref;
  script_result.compare_ref = test_result.compare_ref;
  script_result.replay_ref = test_result.replay_ref;
  script_result.cluster_ref = test_result.cluster_ref;
  script_result.distance_ref = test_result.distance_ref;
  script_result.anomaly_ref = test_result.anomaly_ref;
  script_result.baseline_class_ref = test_result.baseline_class_ref;
  script_result.baseline_feature_ref = test_result.baseline_feature_ref;
  script_result.attach_back_ref = test_result.attach_back_ref;
  script_result.bbox_candidate_list_ref = test_result.bbox_candidate_list_ref;
  script_result.roi_crop_packet_ref = test_result.roi_crop_packet_ref;
  script_result.template_alignment_ref = test_result.template_alignment_ref;
  script_result.candidate_overlay_ref = test_result.candidate_overlay_ref;
  script_result.template_rect_overlay_ref = test_result.template_rect_overlay_ref;
  script_result.test_rect_overlay_ref = test_result.test_rect_overlay_ref;
  script_result.template_test_alignment_status = test_result.template_test_alignment_status;
  script_result.roi_diff_candidate_ref = test_result.roi_diff_candidate_ref;
  script_result.roi_diff_candidate_count = test_result.roi_diff_candidate_count;
  script_result.circle_overlay_ref = test_result.circle_overlay_ref;
  script_result.circle_edge_overlay_ref = test_result.circle_edge_overlay_ref;
  script_result.formfit_candidate_overlay_ref = test_result.formfit_candidate_overlay_ref;
  script_result.formfit_selection_overlay_ref = test_result.formfit_selection_overlay_ref;
  script_result.region_pattern_overlay_ref = test_result.region_pattern_overlay_ref;
  script_result.region_pattern_descriptor_ref = test_result.region_pattern_descriptor_ref;
  script_result.fractal_partition_overlay_ref = test_result.fractal_partition_overlay_ref;
  script_result.distance_field_overlay_ref = test_result.distance_field_overlay_ref;
  script_result.skeleton_overlay_ref = test_result.skeleton_overlay_ref;
  script_result.centerline_overlay_ref = test_result.centerline_overlay_ref;
  script_result.topology_repair_overlay_ref = test_result.topology_repair_overlay_ref;
  script_result.tolerance = test_result.tolerance;
  script_result.selected_method = test_result.selected_method;
  script_result.config_name = test_result.config_name;
  script_result.metric_delta = test_result.metric_delta;
  script_result.stability_delta = test_result.stability_delta;
  script_result.accuracy = test_result.accuracy;
  script_result.macro_f1 = test_result.macro_f1;
  script_result.pass_level = test_result.pass_level;
  script_result.failure_mode = test_result.failure_mode;
  script_result.degraded = test_result.degraded;
  script_result.error_message = test_result.error_message;
  script_result.summary = test_result.summary;
  script_result.details = test_result.details;
  RefreshNamedResultViews(script_result);
  return true;
}

bool ParserCxScriptRuntime::ExecuteScriptText(const std::string &script_name,
                                              const std::string &script_text,
                                              CxScriptExecutionResult &result)
{
  const bool collect_debug = execution_mode_ == cxsrm_debug;
  CxScriptExecutionContext context;
  CxScriptFlow flow;
  CxScriptParseError parse_error;
  std::string error_message;
  if (!ParseScriptText(script_name, script_text, context, flow, parse_error, error_message))
  {
    result = CxScriptExecutionResult();
    result.script_name = script_name;
    result.success = false;
    result.parse_error = parse_error;
    result.error_message = error_message;
    result.summary = error_message;
    result.failure_phase = "parse";
    result.failure_line = parse_error.line;
    if (parse_error.line > 0)
    {
      result.summary += " line=" + std::to_string(parse_error.line);
      if (parse_error.column > 0)
        result.summary += " col=" + std::to_string(parse_error.column);
    }
    return false;
  }

  if (WantsTorchDispatchMainline(context))
  {
    const bool ok = TryExecuteTorchDispatchMainline(context, script_text, result);
    result.declared_types = flow.declared_types;
    result.variables = flow.variables;
    BindVariableExecutionIds(result);
    RefreshExecutionDiagnostics(result);
    RefreshNamedResultViews(result);
    RefreshExecutionMultimodalSlices(result);
    if (collect_debug)
      BuildDebugView(result, result.debug_view);
    else
      ReleaseLightweightDebugArtifacts(result);
    return ok;
  }

  ParserTestRequest request;
  if (!BuildTestRequest(context, flow, request, error_message))
  {
    result = CxScriptExecutionResult();
    result.script_name = script_name;
    result.kind = context.kind;
    result.layer = context.layer;
    result.module = context.module;
    result.case_name = context.case_name;
    result.success = false;
    result.failure_phase = "build_request";
    result.summary = error_message;
    result.error_message = error_message;
    return false;
  }

  ParserTestRunResult test_result;
  bool ok = test_driver_.Execute(request, test_result);
  ConvertTestResult(context, test_result, result);
  const bool ensmallen_contract_applied = ApplyEnsmallenFlowHostContractFallback(context, result);
  const bool mlpack_contract_applied = ApplyMlpackBaselineCaseBridge(context, result);
  const bool torch_contract_applied = ApplyTorchContractCaseBridge(context, result);
  if (!ok)
    ok = ensmallen_contract_applied || mlpack_contract_applied || torch_contract_applied;
  result.declared_types = flow.declared_types;
  result.variables = flow.variables;
  if (collect_debug)
    BuildDebugView(result, result.debug_view);
  if (!ApplyFlowImpl(flow, result, error_message, false, collect_debug))
  {
    if (result.summary.empty())
      result.summary = error_message;
    if (result.error_message.empty())
      result.error_message = error_message;
    if (result.failure_phase.empty())
      result.failure_phase = "flow_apply";
    RefreshExecutionDiagnostics(result);
    if (collect_debug)
      BuildDebugView(result, result.debug_view);
    else
      ReleaseLightweightDebugArtifacts(result);
    return false;
  }
  BindVariableExecutionIds(result);
  RefreshExecutionDiagnostics(result);
  RefreshNamedResultViews(result);
  RefreshExecutionMultimodalSlices(result);
  if (collect_debug)
    BuildDebugView(result, result.debug_view);
  else
    ReleaseLightweightDebugArtifacts(result);
  const bool is_cximage_classical_bridge_case =
    context.module == "cximage" &&
    ((context.layer == "feature" &&
      (context.case_name == "line_measure_roi" ||
       context.case_name == "FindCircle" ||
       context.case_name == "circle_measure_fit" ||
       context.case_name == "formfit_rect_candidate")) ||
     (context.layer == "matcher" &&
      (context.case_name == "fastmatch_template" ||
       context.case_name == "fast_template_match" ||
       context.case_name == "findobject_region")));
  bool has_bridge_ready_summary = false;
  if (is_cximage_classical_bridge_case)
  {
    for (size_t i = 0; i < result.details.size(); ++i)
    {
      const std::string &detail = result.details[i];
      if (detail.find("[SUMMARY] cximage.") == 0 &&
          detail.find(".bridge_ready") != std::string::npos)
      {
        has_bridge_ready_summary = true;
        break;
      }
    }
  }

  if (is_cximage_classical_bridge_case &&
      has_bridge_ready_summary &&
      (result.error_message.empty() ||
       result.summary.find("task validated lane=default scalar=") == 0))
  {
    result.success = true;
    result.failure_mode = "none";
    result.error_message.clear();
    if (result.summary.empty())
      result.summary = "task validated lane=default scalar=" + std::to_string(result.scalar_result);
    ok = true;
  }
  else if (!ok && result.success && is_cximage_classical_bridge_case)
  {
    ok = true;
  }

  return ok && result.success;
}

bool ParserCxScriptRuntime::BuildExecutionPreview(const std::string &script_name,
                                                  const std::string &script_text,
                                                  CxScriptExecutionResult &result)
{
  const bool collect_debug = execution_mode_ == cxsrm_debug;
  CxScriptExecutionContext context;
  CxScriptFlow flow;
  CxScriptParseError parse_error;
  std::string error_message;

  result = CxScriptExecutionResult();
  if (!ParseScriptText(script_name, script_text, context, flow, parse_error, error_message))
  {
    result.script_name = script_name;
    result.success = false;
    result.error_message = error_message;
    result.summary = error_message;
    result.parse_error = parse_error;
    result.failure_phase = "parse";
    result.failure_line = parse_error.line;
    if (collect_debug)
      BuildDebugView(result, result.debug_view);
    else
      ReleaseLightweightDebugArtifacts(result);
    return false;
  }

  result.script_name = script_name;
  result.kind = context.kind;
  result.layer = context.layer;
  result.module = context.module;
  result.case_name = context.case_name;
  result.route = context.route;
  result.success = true;
  result.declared_types = flow.declared_types;
  result.variables = flow.variables;
  RefreshNamedResultViews(result);
  if (collect_debug)
    BuildDebugView(result, result.debug_view);
  if (!ApplyFlowImpl(flow, result, error_message, true, collect_debug))
  {
    if (result.summary.empty())
      result.summary = error_message;
    if (result.error_message.empty())
      result.error_message = error_message;
    if (result.failure_phase.empty())
      result.failure_phase = "preview_flow";
    RefreshExecutionDiagnostics(result);
    if (collect_debug)
      BuildDebugView(result, result.debug_view);
    else
      ReleaseLightweightDebugArtifacts(result);
    return false;
  }
  BindVariableExecutionIds(result);
  RefreshExecutionDiagnostics(result);
  RefreshNamedResultViews(result);
  RefreshExecutionMultimodalSlices(result);
  if (collect_debug)
    BuildDebugView(result, result.debug_view);
  else
    ReleaseLightweightDebugArtifacts(result);
  return true;
}

bool ParserCxScriptRuntime::ExecuteScriptFile(const std::string &script_path,
                                              CxScriptExecutionResult &result)
{
  std::ifstream input(script_path.c_str(), std::ios::in | std::ios::binary);
  std::string effective_script_path = script_path;
  if (!input)
  {
    const std::string aliased_script_path = ResolveLegacyExplicitScriptAliasPath(script_path);
    if (!aliased_script_path.empty())
    {
      input.open(aliased_script_path.c_str(), std::ios::in | std::ios::binary);
      if (input)
        effective_script_path = aliased_script_path;
    }

    if (!input)
    {
      result = CxScriptExecutionResult();
      result.script_path = script_path;
      result.success = false;
      result.failure_phase = "file_open";
      result.summary = "cxscript file open failed";
      result.error_message = result.summary;
      return false;
    }
  }

  std::ostringstream buffer;
  buffer << input.rdbuf();

  const size_t slash = effective_script_path.find_last_of("/\\");
  const std::string script_name = slash == std::string::npos ? effective_script_path : effective_script_path.substr(slash + 1);
  const bool ok = ExecuteScriptText(script_name, buffer.str(), result);
  result.script_path = effective_script_path;
  if (ok)
  {
    std::string expected_module;
    std::string expected_layer;
    std::string expected_case_name;
    if (TryInferExpectedScriptIdentity(effective_script_path,
                                       expected_module,
                                       expected_layer,
                                       expected_case_name))
    {
      const bool module_mismatch =
        !expected_module.empty() &&
        !result.module.empty() &&
        expected_module != result.module;
      const bool layer_mismatch =
        !expected_layer.empty() &&
        !result.layer.empty() &&
        expected_layer != result.layer;
      const bool case_mismatch =
        !expected_case_name.empty() &&
        !result.case_name.empty() &&
        expected_case_name != result.case_name;
      if (module_mismatch || layer_mismatch || case_mismatch)
      {
        result.success = false;
        result.failure_phase = "script_identity_mismatch";
        result.summary = "cxscript content identity mismatch";
        result.error_message = result.summary;
        return false;
      }
    }
  }
  return ok;
}

bool ParserCxScriptRuntime::BuildDebugView(const CxScriptExecutionResult &result,
                                           CxScriptDebugView &debug_view) const
{
  debug_view = CxScriptDebugView();
  debug_view.parse_error = result.parse_error;
  debug_view.execution_summary = result.execution_summary;
  debug_view.named_results = result.named_results;
  debug_view.result_fields = result.result_fields;
  debug_view.step_traces = result.step_traces;
  debug_view.source_map = result.source_map;
  debug_view.checkpoints = result.checkpoints;
  debug_view.breakpoints = result.breakpoints;
  debug_view.execution_ops = result.execution_ops;
  debug_view.replay_frames = result.replay_frames;
  debug_view.execution_steps = result.execution_steps;
  debug_view.variables = result.variables;
  return true;
}

bool ParserCxScriptRuntime::QueryDebugByStep(const CxScriptDebugView &debug_view,
                                             const std::string &step_name,
                                             CxScriptDebugQueryResult &query_result) const
{
  query_result = CxScriptDebugQueryResult();
  query_result.execution_summary = debug_view.execution_summary;
  query_result.named_results = debug_view.named_results;
  query_result.result_fields = debug_view.result_fields;
  query_result.step_name = step_name;
  query_result.current_block_depth = 0;

  for (size_t i = 0; i < debug_view.step_traces.size(); ++i)
  {
    if (debug_view.step_traces[i].step_name == step_name)
    {
      query_result.step_id = debug_view.step_traces[i].step_id;
      query_result.frame_id = debug_view.step_traces[i].frame_id;
      query_result.matched_step_trace = debug_view.step_traces[i];
      query_result.current_block_depth = debug_view.step_traces[i].block_depth;
      break;
    }
  }

  for (size_t i = 0; i < debug_view.source_map.size(); ++i)
  {
    if (debug_view.source_map[i].step_name == step_name)
    {
      query_result.source_entries.push_back(debug_view.source_map[i]);
      if (query_result.matched_source_entry.span.line_begin == 0)
        query_result.matched_source_entry = debug_view.source_map[i];
      if (query_result.step_id == 0)
        query_result.step_id = debug_view.source_map[i].step_id;
      if (query_result.frame_id == 0)
        query_result.frame_id = debug_view.source_map[i].frame_id;
      if (debug_view.source_map[i].block_depth > query_result.current_block_depth)
        query_result.current_block_depth = debug_view.source_map[i].block_depth;
    }
  }

  for (size_t i = 0; i < debug_view.replay_frames.size(); ++i)
  {
    if (debug_view.replay_frames[i].step_name == step_name)
    {
      query_result.replay_frames.push_back(debug_view.replay_frames[i]);
      query_result.nearest_replay_frame = debug_view.replay_frames[i];
      if (query_result.step_id == 0)
        query_result.step_id = debug_view.replay_frames[i].step_id;
      if (query_result.frame_id == 0)
        query_result.frame_id = debug_view.replay_frames[i].frame_id;
    }
  }

  for (size_t i = 0; i < debug_view.execution_ops.size(); ++i)
  {
    if (debug_view.execution_ops[i].step_name == step_name)
    {
      query_result.execution_ops.push_back(debug_view.execution_ops[i]);
      if (query_result.matched_execution_op.sequence == 0)
        query_result.matched_execution_op = debug_view.execution_ops[i];
    }
  }

  for (size_t i = 0; i < debug_view.execution_steps.size(); ++i)
  {
    if (debug_view.execution_steps[i].step_name != step_name)
      continue;

    query_result.execution_steps.push_back(debug_view.execution_steps[i]);
    if (query_result.matched_execution_step.sequence == 0)
      query_result.matched_execution_step = debug_view.execution_steps[i];
  }

  for (size_t i = 0; i < debug_view.checkpoints.size(); ++i)
  {
    if (debug_view.checkpoints[i].step_name == step_name)
    {
      query_result.checkpoints.push_back(debug_view.checkpoints[i]);
      query_result.nearest_checkpoint = debug_view.checkpoints[i];
    }
  }

  for (size_t i = 0; i < debug_view.breakpoints.size(); ++i)
  {
    if (debug_view.breakpoints[i].step_name == step_name)
    {
      query_result.breakpoints.push_back(debug_view.breakpoints[i]);
      query_result.nearest_breakpoint = debug_view.breakpoints[i];
    }
  }

  for (size_t i = 0; i < debug_view.variables.size(); ++i)
  {
    if (debug_view.variables[i].step_name == step_name)
      query_result.variables.push_back(debug_view.variables[i]);
  }

    query_result.found = !query_result.source_entries.empty() ||
                         !query_result.replay_frames.empty() ||
                         !query_result.checkpoints.empty() ||
                         !query_result.breakpoints.empty() ||
                         !query_result.variables.empty();
  return query_result.found;
}

bool ParserCxScriptRuntime::QueryDebugByLine(const CxScriptDebugView &debug_view,
                                             int line,
                                             CxScriptDebugQueryResult &query_result) const
{
  query_result = CxScriptDebugQueryResult();
  query_result.execution_summary = debug_view.execution_summary;
  query_result.line = line;
  query_result.current_block_depth = 0;

  for (size_t i = 0; i < debug_view.source_map.size(); ++i)
  {
    const CxScriptSourceMapEntry &entry = debug_view.source_map[i];
    if (entry.span.line_begin == line)
    {
      if (query_result.step_name.empty())
        query_result.step_name = entry.step_name;
      query_result.step_id = entry.step_id;
      query_result.frame_id = entry.frame_id;
      query_result.source_entries.push_back(entry);
      query_result.matched_source_entry = entry;
      query_result.current_block_depth = entry.block_depth;
    }
  }

  if (query_result.step_name.empty())
    return false;

  const std::string step_name = query_result.step_name;
  QueryDebugByStep(debug_view, step_name, query_result);
  query_result.line = line;

  std::vector<CxScriptSourceMapEntry> filtered_source;
  for (size_t i = 0; i < query_result.source_entries.size(); ++i)
  {
    if (query_result.source_entries[i].span.line_begin == line)
    {
      filtered_source.push_back(query_result.source_entries[i]);
      query_result.matched_source_entry = query_result.source_entries[i];
      query_result.current_block_depth = query_result.source_entries[i].block_depth;
    }
  }
  query_result.source_entries = filtered_source;

  std::vector<CxScriptExecutionOp> filtered_ops;
  for (size_t i = 0; i < query_result.execution_ops.size(); ++i)
  {
    if (query_result.execution_ops[i].span.line_begin <= line)
    {
      filtered_ops.push_back(query_result.execution_ops[i]);
      query_result.matched_execution_op = query_result.execution_ops[i];
    }
  }
  query_result.execution_ops = filtered_ops;

  std::vector<CxScriptExecutionStepView> filtered_steps;
  for (size_t i = 0; i < query_result.execution_steps.size(); ++i)
  {
    const CxScriptExecutionStepView &step = query_result.execution_steps[i];
    if (step.span.line_begin < line)
      query_result.previous_execution_step = step;
    if (step.span.line_begin <= line)
    {
      filtered_steps.push_back(step);
      query_result.matched_execution_step = step;
    }
    else if (query_result.next_execution_step.sequence == 0)
    {
      query_result.next_execution_step = step;
    }
  }
  query_result.execution_steps = filtered_steps;

  for (size_t i = 0; i < query_result.replay_frames.size(); ++i)
  {
    if (query_result.replay_frames[i].span.line_begin <= line)
    {
      query_result.previous_replay_frame = query_result.nearest_replay_frame;
      query_result.nearest_replay_frame = query_result.replay_frames[i];
    }
    else if (query_result.next_replay_frame.sequence == 0)
    {
      query_result.next_replay_frame = query_result.replay_frames[i];
    }
  }

  for (size_t i = 0; i < query_result.checkpoints.size(); ++i)
  {
    const CxScriptCheckpointRecord &checkpoint = query_result.checkpoints[i];
    if (checkpoint.span.line_begin <= line)
    {
      query_result.nearest_checkpoint = checkpoint;
      query_result.previous_checkpoint = checkpoint;
    }
    else if (query_result.next_checkpoint.name.empty())
    {
      query_result.next_checkpoint = checkpoint;
    }
  }

  for (size_t i = 0; i < query_result.breakpoints.size(); ++i)
  {
    const CxScriptBreakpointRecord &breakpoint = query_result.breakpoints[i];
    if (breakpoint.span.line_begin <= line)
    {
      query_result.nearest_breakpoint = breakpoint;
      query_result.previous_breakpoint = breakpoint;
    }
    else if (query_result.next_breakpoint.name.empty())
    {
      query_result.next_breakpoint = breakpoint;
    }
  }

  query_result.found = !query_result.source_entries.empty();
  return query_result.found;
}

bool ParserCxScriptRuntime::QueryDebugBySequence(const CxScriptDebugView &debug_view,
                                                 int sequence,
                                                 CxScriptDebugQueryResult &query_result) const
{
  query_result = CxScriptDebugQueryResult();
  query_result.execution_summary = debug_view.execution_summary;
  query_result.sequence = sequence;

  for (size_t index = 0; index < debug_view.execution_steps.size(); ++index)
  {
    const CxScriptExecutionStepView &step = debug_view.execution_steps[index];
    if (step.sequence == sequence)
    {
      query_result.found = true;
      query_result.step_id = step.step_id;
      query_result.frame_id = step.frame_id;
      query_result.step_name = step.step_name;
      query_result.current_block_depth = step.block_depth;
      query_result.matched_execution_step = step;
      if (index > 0)
        query_result.previous_execution_step = debug_view.execution_steps[index - 1];
      if (index + 1 < debug_view.execution_steps.size())
        query_result.next_execution_step = debug_view.execution_steps[index + 1];
      break;
    }
  }

  if (!query_result.found)
    return false;

  for (size_t index = 0; index < debug_view.replay_frames.size(); ++index)
  {
    const CxScriptReplayFrame &frame = debug_view.replay_frames[index];
    if (frame.sequence == sequence)
    {
      query_result.nearest_replay_frame = frame;
      if (index > 0)
        query_result.previous_replay_frame = debug_view.replay_frames[index - 1];
      if (index + 1 < debug_view.replay_frames.size())
        query_result.next_replay_frame = debug_view.replay_frames[index + 1];
    }

    if (frame.step_id == query_result.step_id && frame.frame_id == query_result.frame_id)
      query_result.replay_frames.push_back(frame);
  }

  for (size_t index = 0; index < debug_view.step_traces.size(); ++index)
  {
    const CxScriptStepTrace &trace = debug_view.step_traces[index];
    if (trace.step_id == query_result.step_id)
    {
      query_result.matched_step_trace = trace;
      break;
    }
  }

  for (size_t index = 0; index < debug_view.source_map.size(); ++index)
  {
    const CxScriptSourceMapEntry &entry = debug_view.source_map[index];
    if (entry.step_id == query_result.step_id && entry.frame_id == query_result.frame_id)
    {
      query_result.source_entries.push_back(entry);
      if (query_result.matched_source_entry.span.line_begin == 0)
        query_result.matched_source_entry = entry;
      if (query_result.line == 0)
        query_result.line = entry.span.line_begin;
      if (entry.span.line_begin == query_result.matched_execution_step.span.line_begin)
        query_result.matched_source_entry = entry;
    }
  }

  for (size_t index = 0; index < debug_view.execution_ops.size(); ++index)
  {
    const CxScriptExecutionOp &op = debug_view.execution_ops[index];
    if (op.step_id == query_result.step_id && op.frame_id == query_result.frame_id)
    {
      query_result.execution_ops.push_back(op);
      if (query_result.matched_execution_op.sequence == 0)
        query_result.matched_execution_op = op;
      if (op.sequence == sequence)
        query_result.matched_execution_op = op;
    }
  }

  for (size_t index = 0; index < debug_view.execution_steps.size(); ++index)
  {
    const CxScriptExecutionStepView &step = debug_view.execution_steps[index];
    if (step.step_id == query_result.step_id && step.frame_id == query_result.frame_id)
      query_result.execution_steps.push_back(step);
  }

  for (size_t index = 0; index < debug_view.checkpoints.size(); ++index)
  {
    const CxScriptCheckpointRecord &checkpoint = debug_view.checkpoints[index];
    if (checkpoint.step_id == query_result.step_id && checkpoint.frame_id == query_result.frame_id)
    {
      query_result.checkpoints.push_back(checkpoint);
      query_result.nearest_checkpoint = checkpoint;
    }
  }

  for (size_t index = 0; index < debug_view.breakpoints.size(); ++index)
  {
    const CxScriptBreakpointRecord &breakpoint = debug_view.breakpoints[index];
    if (breakpoint.step_id == query_result.step_id && breakpoint.frame_id == query_result.frame_id)
    {
      query_result.breakpoints.push_back(breakpoint);
      query_result.nearest_breakpoint = breakpoint;
    }
  }

  for (size_t index = 0; index < debug_view.variables.size(); ++index)
  {
    const CxScriptVariableDecl &variable = debug_view.variables[index];
    if (variable.step_id == query_result.step_id && variable.frame_id == query_result.frame_id)
      query_result.variables.push_back(variable);
  }

  return true;
}

bool ParserCxScriptRuntime::QueryDebugByBreakpoint(const CxScriptDebugView &debug_view,
                                                   const std::string &breakpoint_name,
                                                   CxScriptDebugQueryResult &query_result) const
{
  query_result = CxScriptDebugQueryResult();

  for (size_t i = 0; i < debug_view.breakpoints.size(); ++i)
  {
    const CxScriptBreakpointRecord &breakpoint = debug_view.breakpoints[i];
    if (breakpoint.name != breakpoint_name)
      continue;

    if (!QueryDebugByLine(debug_view, breakpoint.span.line_begin, query_result))
      return false;

    if (query_result.nearest_breakpoint.name != breakpoint_name)
      return false;

    return true;
  }

  return false;
}

bool ParserCxScriptRuntime::QueryDebugByCheckpoint(const CxScriptDebugView &debug_view,
                                                   const std::string &checkpoint_name,
                                                   CxScriptDebugQueryResult &query_result) const
{
  query_result = CxScriptDebugQueryResult();

  for (size_t i = 0; i < debug_view.checkpoints.size(); ++i)
  {
    const CxScriptCheckpointRecord &checkpoint = debug_view.checkpoints[i];
    if (checkpoint.name != checkpoint_name)
      continue;

    if (!QueryDebugByLine(debug_view, checkpoint.span.line_begin, query_result))
      return false;

    if (query_result.nearest_checkpoint.name != checkpoint_name)
      return false;

    return true;
  }

  return false;
}

bool ParserCxScriptRuntime::BuildBuiltinFunctionFragments(std::vector<CxScriptFunctionFragment> &fragments) const
{
  fragments.clear();

  const struct FragmentSeed
  {
    const char *function_name;
    const char *category;
    const char *flow_role;
    const char *summary;
    const char *output_name;
    const char *source_hint;
    bool preferred_for_stage_test;
    const char *inputs[3];
    size_t input_count;
    const char *checks[2];
    size_t check_count;
  } seeds[] = {
    {"image_prepare_basic", "operator", "input_prepare",
     "load image and prepare a normalized image snapshot",
     "prepared_image", "cxcore/core/Image.cpp", true,
     {"image_path", "roi_rect", 0}, 2,
     {"prepared image exists", "roi bounds valid"}, 2},
    {"image_prepare_roi", "operator", "input_prepare",
     "crop a prepared image into a reusable ROI",
     "roi_image", "cxcore/core/Image.cpp", true,
     {"prepared_image", "roi_rect", 0}, 2,
     {"roi image exists", 0}, 1},
    {"feature_line_measure", "feature", "operator_action",
     "measure a dominant line feature from a prepared ROI",
     "line_result", "cxcore/core/Findline.cpp", true,
     {"roi_image", 0, 0}, 1,
     {"line_result points exist", "line_result length > 0"}, 2},
    {"feature_circle_measure", "feature", "operator_action",
     "measure a circle candidate from prepared image content",
     "circle_result", "cxcore/core/Findcircle.cpp", true,
     {"prepared_image", "circle_seed", 0}, 2,
     {"circle_result radius > 0", 0}, 1},
    {"feature_circle_fit", "feature", "result_check",
     "fit and validate circle parameters from measured circle data",
     "circle_fit", "cxcore/core/Findcircle.cpp", true,
     {"circle_result", 0, 0}, 1,
     {"circle_fit radius > 0", "circle_fit center valid"}, 2},
    {"feature_ellipse_measure", "feature", "operator_action",
     "measure an ellipse candidate from prepared image content",
     "ellipse_result", "cxcore/core/Findellipse.cpp", false,
     {"prepared_image", "ellipse_seed", 0}, 2,
     {"ellipse_result points exist", 0}, 1},
    {"matcher_fast_run", "matcher", "operator_action",
     "run fast template matching with prepared image and ROI",
     "match_result", "cxcore/core/FastMatch.cpp", true,
     {"model_file", "prepared_image", "match_roi"}, 3,
     {"match score >= min_score", "match box valid"}, 2},
    {"model_mobilevit_train_mainline", "embedded_model", "operator_action",
     "run the embedded mobilevit training mainline session",
     "training_summary", "libtorch_module/torch_minimal_smoke.cpp", true,
     {"train_data", "eval_data", "runner_config"}, 3,
     {"session stages passed", "summary metrics exist"}, 2},
    {"build_curve", "feature", "operator_action",
     "build an OCCT curve object from input points or line-like references",
     "curve_object", "cxgeom/src/CxCurveBuilder.cpp", true,
     {"point_set", "reference_object", 0}, 2,
     {"success == true", "output_object != \"\""}, 2},
    {"build_wire", "feature", "operator_action",
     "build an OCCT wire object from curve input",
     "wire_object", "cxgeom/src/CxWireBuilder.cpp", true,
     {"curve_object", 0, 0}, 1,
     {"success == true", "output_object != \"\""}, 2},
    {"build_face", "feature", "operator_action",
     "build an OCCT face object from wire input",
     "face_object", "cxgeom/src/CxFaceBuilder.cpp", true,
     {"wire_object", 0, 0}, 1,
     {"success == true", "measure_value > 0"}, 2},
    {"measure_shape", "feature", "result_check",
     "measure OCCT shape length or area",
     "measure_result", "cxgeom/src/CxGeomMeasure.cpp", true,
     {"shape_object", 0, 0}, 1,
     {"measure_value > 0", "success == true"}, 2},
    {"measure_distance", "feature", "result_check",
     "measure OCCT shape distance to reference object",
     "distance_result", "cxgeom/src/CxGeomMeasure.cpp", true,
     {"shape_object", "reference_object", 0}, 2,
     {"distance_value >= 0", "success == true"}, 2},
    {"make_visual_object", "operator", "result_check",
     "build a lightweight visual object from an OCCT shape result",
     "visual_object", "cxgeom/src/CxGeomPresentation.cpp", true,
     {"shape_object", 0, 0}, 1,
     {"output_object != \"\"", "success == true"}, 2}
  };

  for (size_t seed_index = 0; seed_index < sizeof(seeds) / sizeof(seeds[0]); ++seed_index)
  {
    CxScriptFunctionFragment fragment;
    fragment.function_name = seeds[seed_index].function_name;
    fragment.category = seeds[seed_index].category;
    fragment.flow_role = seeds[seed_index].flow_role;
    fragment.summary = seeds[seed_index].summary;
    fragment.output_name = seeds[seed_index].output_name;
    fragment.source_hint = seeds[seed_index].source_hint;
    fragment.preferred_for_stage_test = seeds[seed_index].preferred_for_stage_test;
    for (size_t input_index = 0; input_index < seeds[seed_index].input_count; ++input_index)
      fragment.input_names.push_back(seeds[seed_index].inputs[input_index]);
    for (size_t check_index = 0; check_index < seeds[seed_index].check_count; ++check_index)
      fragment.check_points.push_back(seeds[seed_index].checks[check_index]);
    fragments.push_back(fragment);
  }

  return !fragments.empty();
}

bool ParserCxScriptRuntime::BuildBuiltinFlowSnippets(std::vector<CxScriptFlowSnippet> &snippets) const
{
  snippets.clear();

  const struct SnippetSeed
  {
    const char *snippet_id;
    const char *summary;
    const char *functions[4];
    size_t function_count;
    const char *prepare[2];
    size_t prepare_count;
    const char *operators[1];
    size_t operator_count;
    const char *checks[1];
    size_t check_count;
    bool reusable_for_cxcore;
  } seeds[] = {
    {"image_to_line_feature",
     "prepare image and ROI, then measure a reusable line feature",
     {"image_prepare_basic", "image_prepare_roi", "feature_line_measure"}, 3,
     {"image_prepare_basic", "image_prepare_roi"}, 2,
     {"feature_line_measure"}, 1,
     {"feature_line_measure"}, 1,
     true},
    {"image_to_circle_feature",
     "prepare image, measure a circle, and fit validated circle parameters",
     {"image_prepare_basic", "feature_circle_measure", "feature_circle_fit"}, 3,
     {"image_prepare_basic", 0}, 1,
     {"feature_circle_measure"}, 1,
     {"feature_circle_fit"}, 1,
     true},
    {"image_to_fast_match",
     "prepare image and execute fast template matching",
     {"image_prepare_basic", "matcher_fast_run", 0}, 2,
     {"image_prepare_basic", 0}, 1,
     {"matcher_fast_run"}, 1,
     {"matcher_fast_run"}, 1,
     true},
    {"embedded_model_train_mainline",
     "run the embedded model training mainline and validate summary output",
     {"model_mobilevit_train_mainline", 0, 0}, 1,
     {0, 0}, 0,
     {"model_mobilevit_train_mainline"}, 1,
     {"model_mobilevit_train_mainline"}, 1,
     true},
    {"occt_curve_wire_face_measure",
     "build OCCT curve, wire, and face objects then measure the final shape",
     {"build_curve", "build_wire", "build_face", "measure_shape"}, 4,
     {0, 0}, 0,
     {"build_curve"}, 1,
     {"measure_shape"}, 1,
     true},
    {"occt_shape_measure_visualize",
     "measure a shape, compute distance, and build a lightweight visual result",
     {"measure_shape", "measure_distance", "make_visual_object"}, 3,
     {0, 0}, 0,
     {"measure_shape"}, 1,
     {"make_visual_object"}, 1,
     true}
  };

  for (size_t seed_index = 0; seed_index < sizeof(seeds) / sizeof(seeds[0]); ++seed_index)
  {
    CxScriptFlowSnippet snippet;
    snippet.snippet_id = seeds[seed_index].snippet_id;
    snippet.summary = seeds[seed_index].summary;
    snippet.reusable_for_cxcore = seeds[seed_index].reusable_for_cxcore;
    for (size_t function_index = 0; function_index < seeds[seed_index].function_count; ++function_index)
      snippet.function_names.push_back(seeds[seed_index].functions[function_index]);
    for (size_t prepare_index = 0; prepare_index < seeds[seed_index].prepare_count; ++prepare_index)
      snippet.input_prepare_functions.push_back(seeds[seed_index].prepare[prepare_index]);
    for (size_t operator_index = 0; operator_index < seeds[seed_index].operator_count; ++operator_index)
      snippet.operator_functions.push_back(seeds[seed_index].operators[operator_index]);
    for (size_t check_index = 0; check_index < seeds[seed_index].check_count; ++check_index)
      snippet.result_check_functions.push_back(seeds[seed_index].checks[check_index]);
    snippets.push_back(snippet);
  }

  return !snippets.empty();
}

bool ParserCxScriptRuntime::BuildBuiltinCStyleSnippets(std::vector<CxScriptCStyleSnippet> &snippets) const
{
  snippets.clear();

  {
    CxScriptCStyleSnippet snippet;
    snippet.snippet_id = "point_cloud_filter_measure";
    snippet.action_family = "filter+measure";
    snippet.input_objects.push_back("point_cloud");
    snippet.input_objects.push_back("roi");
    snippet.output_objects.push_back("filtered_cloud");
    snippet.output_objects.push_back("measure_result");
    snippet.check_points.push_back("point_count > 0");
    snippet.check_points.push_back("success == true");
    snippet.script_text =
      "input point_cloud = \"cloud_a\";\n"
      "input roi = \"roi_a\";\n"
      "\n"
      "step action {\n"
      "  call filter(point_cloud, roi);\n"
      "  call measure(filtered_cloud);\n"
      "}\n"
      "\n"
      "step check {\n"
      "  expect point_count > 0;\n"
      "  expect success == true;\n"
      "}\n"
      "\n"
      "step output {\n"
      "  emit result;\n"
      "  emit summary;\n"
      "}\n";
    snippets.push_back(snippet);
  }

  {
    CxScriptCStyleSnippet snippet;
    snippet.snippet_id = "point_set_fit_measure";
    snippet.action_family = "fit+measure";
    snippet.input_objects.push_back("point_set");
    snippet.input_objects.push_back("reference_object");
    snippet.output_objects.push_back("fit_result");
    snippet.output_objects.push_back("measure_result");
    snippet.check_points.push_back("error <= max_error");
    snippet.check_points.push_back("success == true");
    snippet.script_text =
      "input point_set = \"points_a\";\n"
      "input reference_object = \"ref_plane\";\n"
      "\n"
      "step action {\n"
      "  call fit(point_set, reference_object);\n"
      "  call measure(fit_result, reference_object);\n"
      "}\n"
      "\n"
      "step check {\n"
      "  expect error <= 0.05;\n"
      "  expect success == true;\n"
      "}\n"
      "\n"
      "step output {\n"
      "  emit result;\n"
      "  emit summary;\n"
      "}\n";
    snippets.push_back(snippet);
  }

  {
    CxScriptCStyleSnippet snippet;
    snippet.snippet_id = "point_cloud_align_visualize";
    snippet.action_family = "align+visualize";
    snippet.input_objects.push_back("point_cloud");
    snippet.input_objects.push_back("reference_object");
    snippet.output_objects.push_back("aligned_cloud");
    snippet.output_objects.push_back("visual_result");
    snippet.check_points.push_back("success == true");
    snippet.check_points.push_back("output_object != \"\"");
    snippet.script_text =
      "input point_cloud = \"cloud_a\";\n"
      "input reference_object = \"cloud_ref\";\n"
      "\n"
      "step action {\n"
      "  call align(point_cloud, reference_object);\n"
      "  call visualize(aligned_cloud);\n"
      "}\n"
      "\n"
      "step check {\n"
      "  expect success == true;\n"
      "  expect output_object != \"\";\n"
      "}\n"
      "\n"
      "step output {\n"
      "  emit result;\n"
      "  emit export;\n"
      "}\n";
    snippets.push_back(snippet);
  }

  {
    CxScriptCStyleSnippet snippet;
    snippet.snippet_id = "occt_curve_wire_face_measure";
    snippet.action_family = "build+measure";
    snippet.input_objects.push_back("point_set");
    snippet.output_objects.push_back("curve_object");
    snippet.output_objects.push_back("wire_object");
    snippet.output_objects.push_back("face_object");
    snippet.output_objects.push_back("measure_result");
    snippet.check_points.push_back("success == true");
    snippet.check_points.push_back("measure_value > 0");
    snippet.script_text =
      "input point_set = \"points_a\";\n"
      "\n"
      "step action {\n"
      "  call build_curve(point_set);\n"
      "  call build_wire(curve_object);\n"
      "  call build_face(wire_object);\n"
      "  call measure(face_object);\n"
      "}\n"
      "\n"
      "step check {\n"
      "  expect success == true;\n"
      "  expect measure_value > 0;\n"
      "}\n"
      "\n"
      "step output {\n"
      "  emit result;\n"
      "  emit summary;\n"
      "}\n";
    snippets.push_back(snippet);
  }

  {
    CxScriptCStyleSnippet snippet;
    snippet.snippet_id = "occt_shape_distance_visualize";
    snippet.action_family = "measure+visualize";
    snippet.input_objects.push_back("shape_object");
    snippet.input_objects.push_back("reference_object");
    snippet.output_objects.push_back("distance_result");
    snippet.output_objects.push_back("visual_object");
    snippet.check_points.push_back("distance_value >= 0");
    snippet.check_points.push_back("output_object != \"\"");
    snippet.script_text =
      "input shape_object = \"face_a\";\n"
      "input reference_object = \"ref_plane\";\n"
      "\n"
      "step action {\n"
      "  call measure(shape_object);\n"
      "  call measure_distance(shape_object, reference_object);\n"
      "  call visualize(shape_object);\n"
      "}\n"
      "\n"
      "step check {\n"
      "  expect success == true;\n"
      "  expect distance_value >= 0;\n"
      "  expect output_object != \"\";\n"
      "}\n"
      "\n"
      "step output {\n"
      "  emit result;\n"
      "  emit export;\n"
      "}\n";
    snippets.push_back(snippet);
  }

  return !snippets.empty();
}

bool ParserCxScriptRuntime::BuildCurrentMainlineReadiness(CxScriptMainlineReadiness &readiness) const
{
  readiness = CxScriptMainlineReadiness();

  std::vector<CxScriptFunctionFragment> fragments;
  std::vector<CxScriptFlowSnippet> snippets;
  std::vector<CxScriptCStyleSnippet> cstyle_snippets;
  if (!BuildBuiltinFunctionFragments(fragments) ||
      !BuildBuiltinFlowSnippets(snippets) ||
      !BuildBuiltinCStyleSnippets(cstyle_snippets))
  {
    return false;
  }

  readiness.ready_flow_ids.push_back("image_prepare_line_measure");
  readiness.ready_flow_ids.push_back("image_prepare_circle_fit");
  readiness.ready_flow_ids.push_back("image_prepare_fast_match");
  readiness.ready_flow_ids.push_back("occt_curve_wire_face_measure");
  readiness.ready_flow_ids.push_back("occt_shape_distance_visualize");
  readiness.ready_flow_ids.push_back("minimal_3d_filter_fit_align_measure_visualize");
  readiness.ready_flow_ids.push_back("mobilevit_train_mainline");

  readiness.gap_flow_ids.push_back("cxcloud_action_word_alignment");
  readiness.gap_reasons.push_back("filter/align/measure/visualize actions are not fully aligned to stable cxcloud implementations yet");
  readiness.gap_flow_ids.push_back("occt_case_binding_to_main_entry");
  readiness.gap_reasons.push_back("flow fragments are registered, but final case-to-entry binding remains owned by cxparser main thread");
  readiness.gap_flow_ids.push_back("script_result_object_contract");
  readiness.gap_reasons.push_back("shape_object/measure_result/visual_object minimal field contract is still pending final mainline convergence");

  readiness.handoff_order.push_back("image_prepare_line_measure");
  readiness.handoff_order.push_back("image_prepare_circle_fit");
  readiness.handoff_order.push_back("image_prepare_fast_match");
  readiness.handoff_order.push_back("occt_curve_wire_face_measure");
  readiness.handoff_order.push_back("occt_shape_distance_visualize");
  readiness.handoff_order.push_back("minimal_3d_filter_fit_align_measure_visualize");
  readiness.handoff_order.push_back("mobilevit_train_mainline");

  {
    CxScriptMainlineReadiness::Group group;
    group.group_name = "operator";
    group.ready_flow_ids.push_back("image_prepare_line_measure");
    group.ready_flow_ids.push_back("minimal_3d_filter_fit_align_measure_visualize");
    readiness.groups.push_back(group);
  }

  {
    CxScriptMainlineReadiness::Group group;
    group.group_name = "matcher";
    group.ready_flow_ids.push_back("image_prepare_fast_match");
    readiness.groups.push_back(group);
  }

  {
    CxScriptMainlineReadiness::Group group;
    group.group_name = "feature";
    group.ready_flow_ids.push_back("image_prepare_circle_fit");
    readiness.groups.push_back(group);
  }

  {
    CxScriptMainlineReadiness::Group group;
    group.group_name = "embedded_model";
    group.ready_flow_ids.push_back("mobilevit_train_mainline");
    readiness.groups.push_back(group);
  }

  {
    CxScriptMainlineReadiness::Group group;
    group.group_name = "occt_fragment";
    group.ready_flow_ids.push_back("occt_curve_wire_face_measure");
    group.ready_flow_ids.push_back("occt_shape_distance_visualize");
    group.gap_flow_ids.push_back("occt_case_binding_to_main_entry");
    group.gap_flow_ids.push_back("script_result_object_contract");
    readiness.groups.push_back(group);
  }

  {
    CxScriptMainlineReadiness::Group group;
    group.group_name = "cxcloud_fragment";
    group.gap_flow_ids.push_back("cxcloud_action_word_alignment");
    readiness.groups.push_back(group);
  }

  return true;
}

bool ParserCxScriptRuntime::BuildBuiltinFunctionFragmentReport(std::vector<std::string> &lines) const
{
  lines.clear();

  std::vector<CxScriptFunctionFragment> fragments;
  std::vector<CxScriptFlowSnippet> snippets;
  std::vector<CxScriptCStyleSnippet> cstyle_snippets;
  CxScriptMainlineReadiness readiness;
  if (!BuildBuiltinFunctionFragments(fragments) ||
      !BuildBuiltinFlowSnippets(snippets) ||
      !BuildBuiltinCStyleSnippets(cstyle_snippets) ||
      !BuildCurrentMainlineReadiness(readiness))
    return false;

  for (size_t index = 0; index < fragments.size(); ++index)
  {
    const CxScriptFunctionFragment &fragment = fragments[index];
    lines.push_back("[FUNCTION] " + fragment.function_name +
                    " category=" + fragment.category +
                    " role=" + fragment.flow_role +
                    " output=" + fragment.output_name);
  }

  for (size_t index = 0; index < snippets.size(); ++index)
  {
    const CxScriptFlowSnippet &snippet = snippets[index];
    lines.push_back("[SNIPPET] " + snippet.snippet_id +
                    " reusable_for_cxcore=" + std::string(snippet.reusable_for_cxcore ? "true" : "false"));
  }

  for (size_t index = 0; index < cstyle_snippets.size(); ++index)
  {
    const CxScriptCStyleSnippet &snippet = cstyle_snippets[index];
    lines.push_back("[C-SNIPPET] " + snippet.snippet_id +
                    " action_family=" + snippet.action_family);
  }

  for (size_t index = 0; index < readiness.ready_flow_ids.size(); ++index)
  {
    lines.push_back("[READY-FLOW] " + readiness.ready_flow_ids[index]);
  }

  for (size_t index = 0; index < readiness.gap_flow_ids.size() && index < readiness.gap_reasons.size(); ++index)
  {
    lines.push_back("[GAP-FLOW] " + readiness.gap_flow_ids[index] +
                    " reason=" + readiness.gap_reasons[index]);
  }

  for (size_t index = 0; index < readiness.handoff_order.size(); ++index)
  {
    lines.push_back("[HANDOFF-ORDER] " + readiness.handoff_order[index]);
  }

  for (size_t index = 0; index < readiness.groups.size(); ++index)
  {
    const CxScriptMainlineReadiness::Group &group = readiness.groups[index];
    lines.push_back("[READINESS-GROUP] " + group.group_name);
    for (size_t ready_index = 0; ready_index < group.ready_flow_ids.size(); ++ready_index)
      lines.push_back("  [READY] " + group.ready_flow_ids[ready_index]);
    for (size_t gap_index = 0; gap_index < group.gap_flow_ids.size(); ++gap_index)
      lines.push_back("  [GAP] " + group.gap_flow_ids[gap_index]);
  }

  return true;
}
}
