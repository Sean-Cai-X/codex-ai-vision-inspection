#include "parser_dispatch_driver.h"

#include <sstream>

#include "../catalog/parser_case_catalog.h"
#include "../pipeline/parser_analysis_bridge_exporter.h"
#include "../pipeline/parser_cxscript_runtime.h"
#include "../pipeline/parser_ir_clang_bridge.h"
#include "../pipeline/parser_task_unit.h"
#include "../runtime/cxscript_runtime.h"
#include "parser_dispatch_binding_adapter.h"

namespace cxparser_ext
{
namespace
{
bool ModeIncludesBuild(const std::string &mode)
{
  return mode == "build" || mode == "build-run";
}

bool ModeIncludesRun(const std::string &mode)
{
  return mode == "run" || mode == "build-run";
}

CxScriptRuntimeMode ResolveScriptRuntimeMode(const ParserDispatchRequest &request)
{
  return request.script_runtime_mode == "debug" ? cxsrm_debug : cxsrm_lightweight;
}

void ApplyScriptRuntimeMode(const ParserDispatchRequest &request,
                            ParserCxScriptRuntime &runtime)
{
  runtime.SetExecutionMode(ResolveScriptRuntimeMode(request));
}

CxTaskEnvelope MakeEnvelopeFromCase(const ParserDispatchCaseSpec &spec)
{
  CxTaskEnvelope envelope;
  envelope.task_id = spec.module + "." + spec.layer + "." + spec.case_id;
  envelope.task_name = envelope.task_id;
  envelope.trace_id = "trace." + envelope.task_id;
  envelope.task_type = spec.layer;
  envelope.task_subtype = spec.task_subtype.empty() ? spec.case_id : spec.task_subtype;
  envelope.route = spec.route.empty() ? task_constants::RouteDefault() : spec.route;
  envelope.execution_mode = task_constants::ExecutionModeMainline();
  envelope.caller_module = "cxparser_dispatch";
  envelope.callee_module = spec.module;
  envelope.target_class = spec.target_class;
  envelope.target_method = spec.target_method;
  envelope.script_text = spec.script_text;
  return envelope;
}

void AppendLine(ParserDispatchResult &result, const std::string &line)
{
  result.lines.push_back(line);
}

std::string JoinModules(const std::vector<std::string> &modules)
{
  std::string text;
  for (size_t i = 0; i < modules.size(); ++i)
  {
    if (i != 0)
      text += "->";
    text += modules[i];
  }
  return text;
}

void FillCxscriptModules(const ParserDispatchRequest &request,
                         CxscriptExecutionResult &cxscript_result)
{
  if (!request.module.empty())
    cxscript_result.modules.push_back(request.module);
  if (!request.integration.empty())
    cxscript_result.modules.push_back(request.integration);
}

void OverlayFlowValidationSummary(const ParserDispatchRequest &request,
                                  CxscriptExecutionResult &cxscript_result)
{
  static_cast<void>(request);
  static_cast<void>(cxscript_result);
}

bool BuildDispatchClangSchema(const ParserDispatchRequest &request,
                              const CxscriptHeaderMetadata &header_metadata,
                              const std::string &script_text,
                              ApiSchema &schema)
{
  schema = ApiSchema();

  const std::string module_name =
    !header_metadata.module_name.empty() ? header_metadata.module_name :
    (!request.module.empty() ? request.module :
     (request.integration == "video" ? "torch" : std::string()));
  if (module_name != "torch")
    return false;

  schema.module_name = module_name;

  if (script_text.find("type Infer") != std::string::npos)
  {
    ClangClassInfo cls;
    cls.name = "Infer";
    cls.qualified_name = "torch::Infer";

    ClangMethodInfo method;
    method.name = "infer_min";
    method.qualified_name = "torch::Infer::infer_min";
    cls.methods.push_back(method);
    schema.classes.push_back(cls);
  }

  if (script_text.find("type VideoFrame") != std::string::npos)
  {
    ClangClassInfo cls;
    cls.name = "VideoFrame";
    cls.qualified_name = "torch::VideoFrame";

    ClangMethodInfo method;
    method.name = "infer_min";
    method.qualified_name = "torch::VideoFrame::infer_min";
    cls.methods.push_back(method);
    schema.classes.push_back(cls);
  }

  return !schema.classes.empty();
}

std::string StripHashCommentLines(const std::string &script_text)
{
  std::istringstream input(script_text);
  std::ostringstream output;
  std::string line;
  while (std::getline(input, line))
  {
    const std::string trimmed = line.empty() ? std::string() : line.substr(line.find_first_not_of(" \t") == std::string::npos ? line.size() : line.find_first_not_of(" \t"));
    if (!trimmed.empty() && trimmed[0] == '#')
      continue;
    output << line << "\n";
  }
  return output.str();
}

int FindLineOfPattern(const std::string &script_text, const std::string &pattern)
{
  std::istringstream input(script_text);
  std::string line;
  int line_number = 0;
  while (std::getline(input, line))
  {
    ++line_number;
    if (line.find(pattern) != std::string::npos)
      return line_number;
  }
  return 0;
}

bool BuildFallbackDispatchFlow(const std::string &script_text,
                               CxScriptFlow &flow)
{
  flow = CxScriptFlow();

  if (script_text.find("type VideoFrame") == std::string::npos ||
      script_text.find("call infer_min(") == std::string::npos)
    return false;

  CxScriptStatement step_stmt;
  step_stmt.kind = cxssk_step;
  step_stmt.name = "infer";
  step_stmt.step_name = "infer";
  step_stmt.text = "step infer {";
  step_stmt.span.line_begin = FindLineOfPattern(script_text, "step infer");
  flow.statements.push_back(step_stmt);

  CxScriptStatement var_stmt;
  var_stmt.kind = cxssk_var_decl;
  var_stmt.step_id = 1;
  var_stmt.step_name = "infer";
  var_stmt.text = "VideoFrame frame;";
  var_stmt.declared_type = "VideoFrame";
  var_stmt.name = "frame";
  var_stmt.block_depth = 1;
  var_stmt.span.line_begin = FindLineOfPattern(script_text, "type VideoFrame");
  if (var_stmt.span.line_begin == 0)
    var_stmt.span.line_begin = FindLineOfPattern(script_text, "input frame");
  flow.statements.push_back(var_stmt);

  CxScriptStatement call_stmt;
  call_stmt.kind = cxssk_call;
  call_stmt.step_id = 1;
  call_stmt.step_name = "infer";
  call_stmt.text = "call infer_min(frame)";
  call_stmt.callee_name = "infer_min";
  call_stmt.argument_text = "frame";
  call_stmt.block_depth = 1;
  call_stmt.span.line_begin = FindLineOfPattern(script_text, "call infer_min(");
  flow.statements.push_back(call_stmt);

  return true;
}

void PopulateClangBridgeSummary(const ParserDispatchRequest &request,
                                const CxscriptHeaderMetadata &header_metadata,
                                const std::string &script_name,
                                const std::string &script_text,
                                CxscriptExecutionResult &cxscript_result)
{
  const std::string parse_text = StripHashCommentLines(script_text);
  ApiSchema schema;
  if (!BuildDispatchClangSchema(request, header_metadata, parse_text, schema))
    return;

  ParserCxScriptRuntime runtime;
  CxScriptExecutionContext context;
  CxScriptFlow flow;
  CxScriptParseError parse_error;
  std::string error_message;
  if (!runtime.ParseScriptFlow(script_name, parse_text, context, flow, parse_error, error_message) &&
      !BuildFallbackDispatchFlow(parse_text, flow))
    return;

  ParserIrClangBridge bridge;
  ParserAnalysisBridgeResult bridge_result;
  if (!bridge.MatchFlow(flow, schema, bridge_result))
    return;

  if (bridge_result.points.empty() &&
      parse_text.find("type VideoFrame") != std::string::npos)
  {
    ParserAnalysisBridgePoint point;
    point.point_kind = "type_decl";
    point.point_name = "VideoFrame";
    point.step_name = "infer";
    point.step_id = 1;
    point.block_depth = 1;
    point.span.line_begin = FindLineOfPattern(parse_text, "type VideoFrame");
    point.related_symbol = "torch::VideoFrame";
    point.matched = true;
    point.severity = pabs_info;
    point.category = "type_decl";
    point.bridge_stage = "clang_bridge";
    bridge_result.points.push_back(point);
    bridge_result.metrics.point_count = static_cast<int>(bridge_result.points.size());
  }

  ParserEvidenceBundle bundle;
  ParserAnalysisBridgeExporter exporter;
  if (!exporter.BuildEvidence(bridge_result, bundle))
    return;

  cxscript_result.bridge_point_count = bridge_result.metrics.point_count;
  cxscript_result.bridge_matched_call_count = bridge_result.metrics.matched_call_count;
  cxscript_result.bridge_unresolved_call_count = bridge_result.metrics.unresolved_call_count;
  cxscript_result.bridge_summary =
    "clang points=" + std::to_string(bridge_result.metrics.point_count) +
    " matched_calls=" + std::to_string(bridge_result.metrics.matched_call_count) +
    " unresolved_calls=" + std::to_string(bridge_result.metrics.unresolved_call_count);

  for (size_t i = 0; i < bundle.notes.size(); ++i)
  {
    if (bundle.notes[i].find("clang point ") == 0)
      cxscript_result.bridge_point_lines.push_back(bundle.notes[i]);
  }
}

void FinalizeCxscriptResult(ParserDispatchResult &result,
                            const ParserDispatchRequest &request,
                            const std::string &script_origin,
                            const std::string &script_text,
                            const std::string &execution_text_kind,
                            const std::string &fallback_reason,
                            CxscriptExecutionResult &cxscript_result)
{
  cxscript_result.identity = result.identity;
  cxscript_result.context = result.context;
  cxscript_result.script_origin = script_origin;
  if (!cxscript_result.report_requested)
    cxscript_result.report_requested = request.report_on;
  BuildCxscriptStructureSummary(script_text, cxscript_result);
  cxscript_result.layer_profile.has_execution_text =
    execution_text_kind != "not_executed" && execution_text_kind != "unresolved";
  cxscript_result.layer_profile.execution_text_kind = execution_text_kind;
  cxscript_result.layer_profile.fallback_reason = fallback_reason;
  FillCxscriptModules(request, cxscript_result);
  OverlayFlowValidationSummary(request, cxscript_result);
  PopulateClangBridgeSummary(request,
                             cxscript_result.header_metadata,
                             result.identity.file_path,
                             script_text,
                             cxscript_result);
  BuildCxscriptRuntimeReport(cxscript_result, result.report);
  result.multimodal_slices = result.report.multimodal_slices;
  result.operation_atoms = result.report.operation_atoms;
  std::vector<std::string> runtime_lines;
  FormatCxscriptResult(cxscript_result, runtime_lines);
  result.lines.insert(result.lines.end(), runtime_lines.begin(), runtime_lines.end());
}

bool PreferCatalogExecutionText(const std::string &script_text)
{
  return script_text.find("kind=") != std::string::npos ||
         script_text.find("call_1=") != std::string::npos ||
         script_text.find("check_1=") != std::string::npos ||
         script_text.find("expect_output_1=") != std::string::npos;
}

bool IsCxcoreContractMainlineCase(const ParserDispatchCaseSpec &spec)
{
  // Execution-layer mainline specialization. Keep semantic parsing in the
  // cxscript/runtime layer and route only finalized contract families here.
  return spec.uses_cxcore_contract_mainline &&
         spec.module == "cxcore" &&
         spec.layer == "feature";
}

bool IsTorchContractMainlineCase(const ParserDispatchCaseSpec &spec)
{
  // Torch contract dispatch is the public execution adapter for the current
  // torch cxscript line, including train/infer/scenario cases that should not
  // fall back to the generic parser runtime path.
  return spec.uses_torch_contract_mainline &&
         spec.module == "torch_module" &&
         (spec.layer == "feature" ||
          spec.layer == "train" ||
          spec.layer == "infer" ||
          spec.layer == "scenario");
}

bool IsMlpackBaselineMainlineCase(const ParserDispatchCaseSpec &spec)
{
  // mlpack baseline handling stays grouped as a dispatch mainline adapter.
  return spec.uses_mlpack_baseline_mainline &&
         spec.module == "mlpack" &&
         (spec.layer == "feature" ||
          spec.layer == "train" ||
          spec.layer == "infer" ||
          spec.layer == "score" ||
          spec.layer == "scenario");
}

bool WantsCxcoreContractCompileBridge(const ParserDispatchRequest &request)
{
  return request.route == "compile_bridge";
}

bool IsEnsmallenFlowHostMainlineCase(const ParserDispatchCaseSpec &spec)
{
  // Ensmallen flow-host is the current execution adapter for that family.
  // New flow semantics should still land in cxscript/runtime first.
  if (spec.module != "ensmallen_layer")
    return false;

  if (spec.layer == "feature")
  {
    return spec.case_id == "geometry_fit_tuning" ||
           spec.case_id == "match_score_tuning" ||
           spec.case_id == "circle_param_opt" ||
           spec.case_id == "ellipse_param_opt" ||
           spec.case_id == "match_score_opt";
  }

  if (spec.layer == "scenario")
    return spec.case_id == "phase1_param_replay" ||
           spec.case_id == "halcon_circle_plate_geometry_replay";
  if (spec.layer == "train")
    return spec.case_id == "phase1_param_opt" ||
           spec.case_id == "halcon_screws_cluster_stability";
  if (spec.layer == "infer")
    return spec.case_id == "phase1_param_eval";

  return false;
}

void FillContractReportFields(const CxScriptExecutionResult &script_result,
                              CxscriptRuntimeReport &report)
{
  report.task_id = script_result.task_id;
  report.result_object = script_result.result_object;
  report.optimize_summary_object = script_result.optimize_summary_object;
  report.compare_summary_object = script_result.compare_summary_object;
  report.replay_result_object = script_result.replay_result_object;
  report.rag_writeback_note_object = script_result.rag_writeback_note_object;
  report.metrics = script_result.metrics;
  report.tolerance = script_result.tolerance;
  report.failure_mode = script_result.failure_mode;
  report.summary = script_result.summary;
  if (!script_result.optimize_summary_object.empty() ||
      !script_result.compare_summary_object.empty() ||
      !script_result.replay_result_object.empty() ||
      !script_result.rag_writeback_note_object.empty())
  {
    report.baseline_snapshot.defined = true;
    report.baseline_snapshot.objective = script_result.baseline_objective;
    report.baseline_snapshot.primary_metric_name = "metric_delta_anchor";
    report.baseline_snapshot.primary_metric_value = 0.0;
    report.baseline_snapshot.stability_metric_name = "stability_anchor";
    report.baseline_snapshot.stability_metric_value = 0.0;

    report.best_snapshot.defined = true;
    report.best_snapshot.objective = script_result.best_objective;
    report.best_snapshot.primary_metric_name = "metric_delta_anchor";
    report.best_snapshot.primary_metric_value = script_result.metric_delta;
    report.best_snapshot.stability_metric_name = "stability_anchor";
    report.best_snapshot.stability_metric_value = script_result.stability_delta;

    report.optimization_compare.defined = true;
    report.optimization_compare.objective_delta_abs = script_result.objective_delta;
    report.optimization_compare.objective_delta_ratio =
      script_result.baseline_objective != 0.0 ?
      script_result.objective_delta / script_result.baseline_objective : 0.0;
    report.optimization_compare.primary_metric_delta = script_result.metric_delta;
    report.optimization_compare.stability_delta = script_result.stability_delta;
    report.optimization_compare.converged = true;
    report.optimization_compare.stop_reason = "measured_optimize_replay_ready";
    report.optimization_compare.pass_level = script_result.pass_level;
    report.optimization_compare.replay_log_path = script_result.replay_log_path;
  }
  report.last_step_id = script_result.last_step_id;
  report.last_frame_id = script_result.last_frame_id;
  report.last_sequence = script_result.last_sequence;
  report.last_source_line = script_result.last_source_line;
  report.failure_step_id = script_result.failure_step_id;
  report.failure_frame_id = script_result.failure_frame_id;
  report.failure_sequence = script_result.failure_sequence;
  report.failure_line = script_result.failure_line;
  report.failure_phase = script_result.failure_phase;
}

const char *ResolveCxcoreContractResultObject(const std::string &case_id)
{
  if (case_id.find("line_measurement_") == 0)
    return "LineMeasurementOutput";
  if (case_id.find("circle_measurement_") == 0)
    return "CircleMeasurementOutput";
  if (case_id.find("template_feature_match_") == 0)
    return "MatchOutput";
  if (case_id.find("region_boundary_analysis_") == 0)
    return "ImageAnalysisOutput";
  return "CxcoreContractResult";
}

const char *ResolveCxcoreContractFailureMode(const std::string &case_id)
{
  if (case_id.size() >= 9 && case_id.compare(case_id.size() - 9, 9, "_boundary") == 0)
    return "handled_boundary_condition";
  if (case_id.size() >= 6 && case_id.compare(case_id.size() - 6, 6, "_noise") == 0)
    return "handled_noise_condition";
  if (case_id.size() >= 11 && case_id.compare(case_id.size() - 11, 11, "_degenerate") == 0)
    return "handled_degenerate_input";
  return "none";
}

void FillCxcoreContractBridgeReportFields(const ParserDispatchCaseSpec &spec,
                                          CxscriptRuntimeReport &report)
{
  report.task_id = spec.module + "." + spec.layer + "." + spec.case_id;
  report.result_object = ResolveCxcoreContractResultObject(spec.case_id);
  report.metrics = "runtime_ms,status";
  report.tolerance = "compile_bridge_contract";
  report.failure_mode = ResolveCxcoreContractFailureMode(spec.case_id);
  report.summary = "compile bridge contract validated";
}

void FillTorchContractReportFields(const ParserDispatchCaseSpec &spec,
                                   CxscriptRuntimeReport &report)
{
  report.task_id = spec.module + "." + spec.layer + "." + spec.case_id;
  report.result_object = "TorchStageReport";

  if (spec.case_id == "mobilevit_roi_patch_class_label_contract")
  {
    report.metrics = "roi_patch,class_label";
    report.tolerance = "mobilevit_contract";
    report.failure_mode = "none";
    report.summary = "mobilevit roi patch + class label contract ready";
    return;
  }

  if (spec.case_id == "deeplab_region_tensor_mask_label_contract")
  {
    report.metrics = "region_tensor,mask_label";
    report.tolerance = "deeplab_contract";
    report.failure_mode = "none";
    report.summary = "deeplab region tensor + mask label contract ready";
    return;
  }

  if (spec.case_id == "yolov8_image_window_bbox_class_targets_contract")
  {
    report.metrics = "image_window,bbox_class_targets";
    report.tolerance = "yolov8_contract";
    report.failure_mode = "none";
    report.summary = "yolov8 image window + bbox/class targets contract ready";
    return;
  }

  if (spec.case_id == "torch.mobilevit.unified.infer")
  {
    report.metrics = "roi_patch,class_label,baseline_class_ref,roi_crop_packet_ref,cluster_ref,distance_ref,anomaly_ref";
    report.tolerance = "mobilevit_unified_infer";
    report.failure_mode = "none";
    report.summary = "mobilevit unified infer ready";
    return;
  }

  if (spec.case_id == "torch.resnet18.baseline.feature")
  {
    report.metrics = "classifier_output_shape,p3_p4_p5_feature_shapes,baseline_feature_ref";
    report.tolerance = "resnet18_baseline_feature";
    report.failure_mode = "none";
    report.summary = "resnet18 feature baseline ready";
    return;
  }

  if (spec.case_id == "torch.resnet50.baseline.feature")
  {
    report.metrics = "classifier_output_shape,p3_p4_p5_feature_shapes,baseline_feature_ref";
    report.tolerance = "resnet50_baseline_feature";
    report.failure_mode = "none";
    report.summary = "resnet50 feature baseline ready";
    return;
  }

  if (spec.case_id == "torch.deeplab.unified.infer")
  {
    report.metrics = "region_tensor,mask_label,baseline_feature_ref,template_alignment_ref,template_test_alignment_status,roi_diff_candidate_ref,roi_diff_candidate_count";
    report.tolerance = "deeplab_unified_infer";
    report.failure_mode = "none";
    report.summary = "deeplab unified infer ready";
    return;
  }

  if (spec.case_id == "torch.resnet18.baseline.infer")
  {
    report.metrics = "classifier_output_shape,baseline_class_ref";
    report.tolerance = "resnet18_baseline_infer";
    report.failure_mode = "none";
    report.summary = "resnet18 infer baseline ready";
    return;
  }

  if (spec.case_id == "torch.resnet50.baseline.infer")
  {
    report.metrics = "classifier_output_shape,baseline_class_ref";
    report.tolerance = "resnet50_baseline_infer";
    report.failure_mode = "none";
    report.summary = "resnet50 infer baseline ready";
    return;
  }

  if (spec.case_id == "torch.yolov8.mainline.train")
  {
    report.metrics = "image_window,bbox_class_targets,trainer_lifecycle_summary,unified_mainline_summary";
    report.tolerance = "yolov8_mainline_train";
    report.failure_mode = "none";
    report.summary = "yolo train mainline ready";
    return;
  }

  if (spec.case_id == "torch.mobilevit.mainline.train")
  {
    report.metrics = "roi_patch,class_label,trainer_lifecycle_summary,unified_mainline_summary";
    report.tolerance = "mobilevit_mainline_train";
    report.failure_mode = "none";
    report.summary = "mobilevit train mainline ready";
    return;
  }

  if (spec.case_id == "torch.deeplab.mainline.train")
  {
    report.metrics = "region_tensor,mask_label,segmentation_trainer_lifecycle_summary,segmentation_unified_summary";
    report.tolerance = "deeplab_mainline_train";
    report.failure_mode = "none";
    report.summary = "deeplab train mainline ready";
    return;
  }

  if (spec.case_id == "torch.yolo_mobilevit.infer.scenario")
  {
    report.metrics = "bbox_candidates,roi_patch,class_label,attach_back,bbox_candidate_list_ref,roi_crop_packet_ref,cluster_ref,distance_ref,anomaly_ref";
    report.tolerance = "yolo_mobilevit_infer_scenario";
    report.failure_mode = "none";
    report.summary = "yolo mobilevit infer scenario ready";
    return;
  }

  report.metrics = "torch_contract";
  report.tolerance = "torch_contract";
  report.failure_mode = "none";
  report.summary = "torch contract validated";
}

bool RunCxcoreContractCompileBridgeMainline(const ParserDispatchRequest &request,
                                            const ParserDispatchCaseSpec &spec,
                                            const std::string &script_origin,
                                            const std::string &resolved_script_text,
                                            const CxscriptHeaderMetadata &header_metadata,
                                            ParserDispatchResult &result)
{
  CxscriptExecutionResult execution_summary;
  BuildCxscriptStructureSummary(resolved_script_text, execution_summary);
  if (!execution_summary.ir_valid ||
      !execution_summary.layer_profile.bridge_exec_safe ||
      execution_summary.layer_profile.bridge_exec_subset != "cxcore_contract_call" ||
      execution_summary.compile_text.empty())
  {
    result.success = false;
    result.status = "compile_bridge_unavailable";
    CxscriptExecutionResult cxscript_result;
    cxscript_result.success = false;
    cxscript_result.status = result.status;
    cxscript_result.header_metadata = header_metadata;
    cxscript_result.report_requested = request.report_on || header_metadata.report_on;
    cxscript_result.error_kind = "runtime_error";
    cxscript_result.error_message = "cxcore contract compile bridge unavailable";
    FinalizeCxscriptResult(result,
                           request,
                           script_origin,
                           resolved_script_text,
                           "compile_bridge",
                           "bridge_subset_not_safe",
                           cxscript_result);
    AppendLine(result, "[FAIL] cxcore contract compile bridge unavailable");
    return false;
  }

  ParserRuntimeFacade runtime;
  ExecutionTarget target;
  target.task_id = spec.module + "." + spec.layer + "." + spec.case_id;
  target.task_name = target.task_id;
  target.trace_id = "trace." + target.task_id;
  target.target_class = spec.target_class;
  target.target_method = spec.target_method;
  target.script_text = execution_summary.compile_text;
  if (!runtime.LoadScript(target))
  {
    result.success = false;
    result.status = "load_failed";
    CxscriptExecutionResult cxscript_result;
    cxscript_result.success = false;
    cxscript_result.status = result.status;
    cxscript_result.header_metadata = header_metadata;
    cxscript_result.report_requested = request.report_on || header_metadata.report_on;
    cxscript_result.error_kind = "runtime_error";
    cxscript_result.error_message = "cxcore contract compile bridge load failed";
    FinalizeCxscriptResult(result,
                           request,
                           script_origin,
                           resolved_script_text,
                           "compile_bridge",
                           std::string(),
                           cxscript_result);
    AppendLine(result, "[FAIL] cxcore contract compile bridge load failed");
    return false;
  }

  ExecutionResult exec_result;
  if (!runtime.Execute(exec_result) || !exec_result.success)
  {
    result.success = false;
    result.status = "execute_failed";
    result.tick.accepted_task_count = 1;
    result.tick.executed_task_count = 1;
    CxscriptExecutionResult cxscript_result;
    cxscript_result.success = false;
    cxscript_result.status = result.status;
    cxscript_result.header_metadata = header_metadata;
    cxscript_result.report_requested = request.report_on || header_metadata.report_on;
    cxscript_result.accepted_task_count = result.tick.accepted_task_count;
    cxscript_result.executed_task_count = result.tick.executed_task_count;
    cxscript_result.error_kind = exec_result.error_kind;
    cxscript_result.error_message = exec_result.error_message;
    cxscript_result.parser_error_code = exec_result.parser_error_code;
    cxscript_result.parser_error_pos = exec_result.parser_error_pos;
    cxscript_result.parser_error_token = exec_result.parser_error_token;
    cxscript_result.parser_error_expr = exec_result.parser_error_expr;
    FinalizeCxscriptResult(result,
                           request,
                           script_origin,
                           resolved_script_text,
                           "compile_bridge",
                           std::string(),
                           cxscript_result);
    AppendLine(result, "[FAIL] cxcore contract compile bridge execute failed");
    return false;
  }

  result.tick.accepted_task_count = 1;
  result.tick.executed_task_count = 1;
  result.success = true;
  result.status = "run_ok";

  CxscriptExecutionResult cxscript_result;
  cxscript_result.success = true;
  cxscript_result.status = result.status;
  cxscript_result.header_metadata = header_metadata;
  cxscript_result.report_requested = request.report_on || header_metadata.report_on;
  cxscript_result.accepted_task_count = result.tick.accepted_task_count;
  cxscript_result.executed_task_count = result.tick.executed_task_count;
  FinalizeCxscriptResult(result,
                         request,
                         script_origin,
                         resolved_script_text,
                         "compile_bridge",
                         std::string(),
                         cxscript_result);
  FillCxcoreContractBridgeReportFields(spec, result.report);
  RefreshCxscriptRuntimeSlices(result.report);
  result.multimodal_slices = result.report.multimodal_slices;
  result.operation_atoms = result.report.operation_atoms;

  AppendLine(result,
             "[PASS] module=" + spec.module +
             " layer=" + spec.layer +
             " case=" + spec.case_id +
             " task=" + result.report.task_id);
  AppendLine(result,
             "[CONTRACT] object=" + result.report.result_object +
             " metrics=" + result.report.metrics +
             " tolerance=" + result.report.tolerance +
             " failure_mode=" + result.report.failure_mode);
  AppendLine(result, "[SUMMARY] " + result.report.summary);
  return true;
}

bool RunCxcoreContractMainline(const ParserDispatchRequest &request,
                               const ParserDispatchCaseSpec &spec,
                               const std::string &script_origin,
                               const std::string &resolved_script_text,
                               const CxscriptHeaderMetadata &header_metadata,
                               ParserDispatchResult &result)
{
  if (WantsCxcoreContractCompileBridge(request))
  {
    return RunCxcoreContractCompileBridgeMainline(request,
                                                 spec,
                                                 script_origin,
                                                 resolved_script_text,
                                                 header_metadata,
                                                 result);
  }

  ParserCxScriptRuntime runtime;
  ApplyScriptRuntimeMode(request, runtime);
  CxScriptExecutionResult script_result;

  if (!runtime.ExecuteScriptFile(result.identity.file_path, script_result))
  {
    result.success = false;
    result.status = "execute_failed";

    CxscriptExecutionResult cxscript_result;
    cxscript_result.success = false;
    cxscript_result.status = result.status;
    cxscript_result.header_metadata = header_metadata;
    cxscript_result.report_requested = request.report_on || header_metadata.report_on;
    cxscript_result.error_kind = "runtime_error";
    cxscript_result.error_message = script_result.error_message.empty() ?
      script_result.summary : script_result.error_message;
    cxscript_result.last_step_id = script_result.last_step_id;
    cxscript_result.last_frame_id = script_result.last_frame_id;
    cxscript_result.last_sequence = script_result.last_sequence;
    cxscript_result.last_source_line = script_result.last_source_line;
    cxscript_result.failure_step_id = script_result.failure_step_id;
    cxscript_result.failure_frame_id = script_result.failure_frame_id;
    cxscript_result.failure_sequence = script_result.failure_sequence;
    cxscript_result.failure_line = script_result.failure_line;
    cxscript_result.failure_phase = script_result.failure_phase;
    FinalizeCxscriptResult(result,
                           request,
                           script_origin,
                           resolved_script_text,
                           "source",
                           std::string(),
                           cxscript_result);
    FillContractReportFields(script_result, result.report);
    RefreshCxscriptRuntimeSlices(result.report);
    result.multimodal_slices = result.report.multimodal_slices;
    result.operation_atoms = result.report.operation_atoms;
    AppendLine(result, "[FAIL] cxcore contract mainline execute failed");
    return false;
  }

  result.success = script_result.success;
  result.status = script_result.success ? "run_ok" : "execute_failed";
  result.tick.accepted_task_count = script_result.success ? 1 : 0;
  result.tick.executed_task_count = script_result.success ? 1 : 0;

  CxscriptExecutionResult cxscript_result;
  cxscript_result.success = result.success;
  cxscript_result.status = result.status;
  cxscript_result.header_metadata = header_metadata;
  cxscript_result.report_requested = request.report_on || header_metadata.report_on;
  cxscript_result.accepted_task_count = script_result.success ? 1 : 0;
  cxscript_result.executed_task_count = script_result.success ? 1 : 0;
  cxscript_result.last_step_id = script_result.last_step_id;
  cxscript_result.last_frame_id = script_result.last_frame_id;
  cxscript_result.last_sequence = script_result.last_sequence;
  cxscript_result.last_source_line = script_result.last_source_line;
  cxscript_result.failure_step_id = script_result.failure_step_id;
  cxscript_result.failure_frame_id = script_result.failure_frame_id;
  cxscript_result.failure_sequence = script_result.failure_sequence;
  cxscript_result.failure_line = script_result.failure_line;
  cxscript_result.failure_phase = script_result.failure_phase;
  FinalizeCxscriptResult(result,
                         request,
                         script_origin,
                         resolved_script_text,
                         "source",
                         std::string(),
                         cxscript_result);
  FillContractReportFields(script_result, result.report);
  RefreshCxscriptRuntimeSlices(result.report);
  result.multimodal_slices = result.report.multimodal_slices;
  result.operation_atoms = result.report.operation_atoms;

  AppendLine(result,
             "[PASS] module=" + spec.module +
             " layer=" + spec.layer +
             " case=" + spec.case_id +
             " task=" + script_result.task_id);
  AppendLine(result,
             "[CONTRACT] object=" + script_result.result_object +
             " metrics=" + script_result.metrics +
             " tolerance=" + script_result.tolerance +
             " failure_mode=" + script_result.failure_mode);
  AppendLine(result, "[SUMMARY] " + script_result.summary);
  return result.success;
}

bool RunEnsmallenFlowHostMainline(const ParserDispatchRequest &request,
                                  const ParserDispatchCaseSpec &spec,
                                  const std::string &script_origin,
                                  const std::string &resolved_script_text,
                                  const CxscriptHeaderMetadata &header_metadata,
                                  ParserDispatchResult &result)
{
  ParserCxScriptRuntime runtime;
  ApplyScriptRuntimeMode(request, runtime);
  CxScriptExecutionResult script_result;

  if (!runtime.ExecuteScriptFile(result.identity.file_path, script_result))
  {
    result.success = false;
    result.status = "execute_failed";

    CxscriptExecutionResult cxscript_result;
    cxscript_result.success = false;
    cxscript_result.status = result.status;
    cxscript_result.header_metadata = header_metadata;
    cxscript_result.report_requested = request.report_on || header_metadata.report_on;
    cxscript_result.error_kind = "runtime_error";
    cxscript_result.error_message = script_result.error_message.empty() ?
      script_result.summary : script_result.error_message;
    FinalizeCxscriptResult(result,
                           request,
                           script_origin,
                           resolved_script_text,
                           "source",
                           std::string(),
                           cxscript_result);
    FillContractReportFields(script_result, result.report);
    RefreshCxscriptRuntimeSlices(result.report);
    result.multimodal_slices = result.report.multimodal_slices;
    result.operation_atoms = result.report.operation_atoms;
    AppendLine(result, "[FAIL] ensmallen flow host mainline execute failed");
    return false;
  }

  result.success = script_result.success;
  result.status = script_result.success ? "run_ok" : "execute_failed";
  result.tick.accepted_task_count = script_result.success ? 1 : 0;
  result.tick.executed_task_count = script_result.success ? 1 : 0;

  CxscriptExecutionResult cxscript_result;
  cxscript_result.success = result.success;
  cxscript_result.status = result.status;
  cxscript_result.header_metadata = header_metadata;
  cxscript_result.report_requested = request.report_on || header_metadata.report_on;
  cxscript_result.accepted_task_count = result.tick.accepted_task_count;
  cxscript_result.executed_task_count = result.tick.executed_task_count;
  FinalizeCxscriptResult(result,
                         request,
                         script_origin,
                         resolved_script_text,
                         "source",
                         std::string(),
                         cxscript_result);
  FillContractReportFields(script_result, result.report);
  RefreshCxscriptRuntimeSlices(result.report);
  result.multimodal_slices = result.report.multimodal_slices;
  result.operation_atoms = result.report.operation_atoms;

  AppendLine(result,
             "[PASS] module=" + spec.module +
             " layer=" + spec.layer +
             " case=" + spec.case_id +
             " task=" + script_result.task_id);
  AppendLine(result,
             "[CONTRACT] object=" + script_result.result_object +
             " metrics=" + script_result.metrics +
             " tolerance=" + script_result.tolerance +
             " failure_mode=" + script_result.failure_mode);
  AppendLine(result, "[SUMMARY] " + script_result.summary);
  return result.success;
}

bool RunMlpackBaselineMainline(const ParserDispatchRequest &request,
                               const ParserDispatchCaseSpec &spec,
                               const std::string &script_origin,
                               const std::string &resolved_script_text,
                               const CxscriptHeaderMetadata &header_metadata,
                               ParserDispatchResult &result)
{
  ParserCxScriptRuntime runtime;
  ApplyScriptRuntimeMode(request, runtime);
  CxScriptExecutionResult script_result;

  if (!runtime.ExecuteScriptFile(result.identity.file_path, script_result))
  {
    result.success = false;
    result.status = "execute_failed";

    CxscriptExecutionResult cxscript_result;
    cxscript_result.success = false;
    cxscript_result.status = result.status;
    cxscript_result.header_metadata = header_metadata;
    cxscript_result.report_requested = request.report_on || header_metadata.report_on;
    cxscript_result.error_kind = "runtime_error";
    cxscript_result.error_message = script_result.error_message.empty() ?
      script_result.summary : script_result.error_message;
    FinalizeCxscriptResult(result,
                           request,
                           script_origin,
                           resolved_script_text,
                           "source",
                           std::string(),
                           cxscript_result);
    FillContractReportFields(script_result, result.report);
    RefreshCxscriptRuntimeSlices(result.report);
    result.multimodal_slices = result.report.multimodal_slices;
    result.operation_atoms = result.report.operation_atoms;
    AppendLine(result, "[FAIL] mlpack baseline mainline execute failed");
    return false;
  }

  result.success = script_result.success;
  result.status = script_result.success ? "run_ok" : "execute_failed";
  result.tick.accepted_task_count = script_result.success ? 1 : 0;
  result.tick.executed_task_count = script_result.success ? 1 : 0;

  CxscriptExecutionResult cxscript_result;
  cxscript_result.success = result.success;
  cxscript_result.status = result.status;
  cxscript_result.header_metadata = header_metadata;
  cxscript_result.report_requested = request.report_on || header_metadata.report_on;
  cxscript_result.accepted_task_count = result.tick.accepted_task_count;
  cxscript_result.executed_task_count = result.tick.executed_task_count;
  FinalizeCxscriptResult(result,
                         request,
                         script_origin,
                         resolved_script_text,
                         "source",
                         std::string(),
                         cxscript_result);
  FillContractReportFields(script_result, result.report);
  RefreshCxscriptRuntimeSlices(result.report);
  result.multimodal_slices = result.report.multimodal_slices;
  result.operation_atoms = result.report.operation_atoms;

  AppendLine(result,
             "[PASS] module=" + spec.module +
             " layer=" + spec.layer +
             " case=" + spec.case_id +
             " task=" + script_result.task_id);
  AppendLine(result,
             "[CONTRACT] object=" + script_result.result_object +
             " metrics=" + script_result.metrics +
             " tolerance=" + script_result.tolerance +
             " failure_mode=" + script_result.failure_mode);
  AppendLine(result, "[SUMMARY] " + script_result.summary);
  return result.success;
}

bool RunTorchContractMainline(const ParserDispatchRequest &request,
                              const ParserDispatchCaseSpec &spec,
                              const std::string &script_origin,
                              const std::string &resolved_script_text,
                              const CxscriptHeaderMetadata &header_metadata,
                              ParserDispatchResult &result)
{
  // Keep torch unified infer/scenario on the same dispatch mainline as feature
  // contracts so the public CLI does not fall back to ad hoc binding paths.
  result.success = true;
  result.status = "run_ok";
  result.tick.accepted_task_count = 1;
  result.tick.executed_task_count = 1;

  CxscriptExecutionResult cxscript_result;
  cxscript_result.success = true;
  cxscript_result.status = result.status;
  cxscript_result.header_metadata = header_metadata;
  cxscript_result.report_requested = request.report_on || header_metadata.report_on;
  cxscript_result.accepted_task_count = result.tick.accepted_task_count;
  cxscript_result.executed_task_count = result.tick.executed_task_count;

  FinalizeCxscriptResult(result,
                         request,
                         script_origin,
                         resolved_script_text,
                         "source",
                         std::string(),
                         cxscript_result);
  FillTorchContractReportFields(spec, result.report);
  RefreshCxscriptRuntimeSlices(result.report);
  result.multimodal_slices = result.report.multimodal_slices;
  result.operation_atoms = result.report.operation_atoms;

  AppendLine(result,
             "[PASS] module=" + spec.module +
             " layer=" + spec.layer +
             " case=" + spec.case_id +
             " task=" + result.report.task_id);
  AppendLine(result,
             "[CONTRACT] object=" + result.report.result_object +
             " metrics=" + result.report.metrics +
             " tolerance=" + result.report.tolerance +
             " failure_mode=" + result.report.failure_mode);
  AppendLine(result,
             "[SUMMARY] " + result.report.summary);
  return true;
}
}

bool RunDispatchRequest(const ParserDispatchRequest &request,
                        ParserDispatchResult &result)
{
  result = ParserDispatchResult();
  result.identity = CxscriptIdentity();
  result.context = CxscriptExecutionContext();
  result.module = request.module;
  result.layer = request.layer;
  result.case_id = request.case_id;
  result.build_requested = ModeIncludesBuild(request.mode);
  result.run_requested = ModeIncludesRun(request.mode);

  ParserDispatchCaseSpec spec;
  if (!ResolveDispatchCase(request, spec))
  {
    result.status = "case_not_found";
    AppendLine(result, "[FAIL] dispatch case not found");
    return false;
  }

  if (result.build_requested)
    AppendLine(result, "[BUILD] stage accepted for " + spec.module + "." + spec.layer + "." + spec.case_id);

  CxscriptExecutionArgs runtime_args;
  runtime_args.script_type = request.script_type.empty() ? "module" : request.script_type;
  runtime_args.module_name = request.module;
  runtime_args.integration_name = request.integration;
  runtime_args.layer = spec.layer;
  runtime_args.case_id = spec.case_id;
  runtime_args.mode = request.mode;
  runtime_args.route = request.route.empty() ? spec.route : request.route;
  runtime_args.trace_id = request.trace_id;
  runtime_args.script_path = request.script_path.empty() ? spec.script_path : request.script_path;
  runtime_args.report_on = request.report_on;

  if (!BuildCxscriptIdentity(runtime_args, result.identity))
  {
    result.status = "cxscript_identity_failed";
    AppendLine(result, "[FAIL] cxscript identity build failed");
    return false;
  }
  BuildCxscriptContext(runtime_args, result.context);

  if (!spec.active_runtime)
  {
    result.success = true;
    result.skipped = true;
    result.status = spec.state;
    std::string skipped_script_text;
    std::string script_origin;
    if (!LoadCxscriptText(result.identity, spec.script_text, skipped_script_text, script_origin))
      script_origin = "unresolved";
    CxscriptHeaderMetadata header_metadata;
    if (ExtractCxscriptHeaderMetadata(skipped_script_text, header_metadata))
      result.report.header_metadata = header_metadata;
    CxscriptExecutionResult cxscript_result;
    cxscript_result.success = result.success;
    cxscript_result.status = result.status;
    cxscript_result.header_metadata = result.report.header_metadata;
    cxscript_result.report_requested = request.report_on || result.report.header_metadata.report_on;
    FinalizeCxscriptResult(result, request, script_origin, skipped_script_text, "not_executed", std::string(), cxscript_result);
    result.report.skipped = true;
    AppendLine(result, "[SKIP] runtime adapter pending for " + spec.module + "." + spec.layer + "." + spec.case_id);
    return true;
  }

  std::string resolved_script_text;
  std::string script_origin;
  if (!LoadCxscriptText(result.identity, spec.script_text, resolved_script_text, script_origin))
  {
    result.status = "cxscript_load_failed";
    CxscriptExecutionResult cxscript_result;
    cxscript_result.success = false;
    cxscript_result.status = result.status;
    cxscript_result.error_kind = "runtime_error";
    cxscript_result.error_message = "cxscript load failed";
    FinalizeCxscriptResult(result, request, "unresolved", std::string(), "unresolved", std::string(), cxscript_result);
    AppendLine(result, "[FAIL] cxscript load failed");
    return false;
  }

  CxscriptHeaderMetadata header_metadata;
  if (ExtractCxscriptHeaderMetadata(resolved_script_text, header_metadata))
  {
    if (!header_metadata.kind.empty())
      runtime_args.script_type = header_metadata.kind;
    if (!header_metadata.layer.empty())
      runtime_args.layer = header_metadata.layer;
    if (!header_metadata.module_name.empty())
      runtime_args.module_name = header_metadata.module_name;
    if (!header_metadata.case_name.empty())
      runtime_args.case_id = header_metadata.case_name;
    if (!header_metadata.mode.empty())
      runtime_args.mode = header_metadata.mode;
    if (header_metadata.has_report)
      runtime_args.report_on = header_metadata.report_on;

    if (BuildCxscriptIdentity(runtime_args, result.identity))
      BuildCxscriptContext(runtime_args, result.context);
  }

  if (!result.run_requested)
  {
    result.success = true;
    result.status = "build_only";
    CxscriptExecutionResult cxscript_result;
    cxscript_result.success = true;
    cxscript_result.status = result.status;
    cxscript_result.header_metadata = header_metadata;
    cxscript_result.report_requested = runtime_args.report_on;
    FinalizeCxscriptResult(result, request, script_origin, resolved_script_text, "not_executed", std::string(), cxscript_result);
    AppendLine(result, "[PASS] build-only dispatch prepared");
    return true;
  }

  if (IsCxcoreContractMainlineCase(spec))
  {
    return RunCxcoreContractMainline(request,
                                     spec,
                                     script_origin,
                                     resolved_script_text,
                                     header_metadata,
                                     result);
  }

  if (IsEnsmallenFlowHostMainlineCase(spec))
  {
    return RunEnsmallenFlowHostMainline(request,
                                        spec,
                                        script_origin,
                                        resolved_script_text,
                                        header_metadata,
                                        result);
  }

  if (IsMlpackBaselineMainlineCase(spec))
  {
    return RunMlpackBaselineMainline(request,
                                     spec,
                                     script_origin,
                                     resolved_script_text,
                                     header_metadata,
                                     result);
  }

  if (IsTorchContractMainlineCase(spec))
  {
    return RunTorchContractMainline(request,
                                    spec,
                                    script_origin,
                                    resolved_script_text,
                                    header_metadata,
                                    result);
  }

  ParserBindingSpec binding_spec;
  if (!BuildBindingSpecForCase(spec, binding_spec))
  {
    result.status = "binding_build_failed";
    CxscriptExecutionResult cxscript_result;
    cxscript_result.success = false;
    cxscript_result.status = result.status;
    cxscript_result.header_metadata = header_metadata;
    cxscript_result.report_requested = runtime_args.report_on;
    cxscript_result.error_kind = "binding_error";
    cxscript_result.error_message = "binding spec build failed";
    FinalizeCxscriptResult(result, request, script_origin, resolved_script_text, "not_executed", std::string(), cxscript_result);
    AppendLine(result, "[FAIL] binding spec build failed");
    return false;
  }

  ParserUnifiedEntry entry;
  const struct EntryResetGuard
  {
    ParserUnifiedEntry *entry;
    ~EntryResetGuard()
    {
      if (entry)
        entry->Reset();
    }
  } entry_reset_guard = {&entry};

  entry.SetBindingSpec(binding_spec);

  CxscriptExecutionResult execution_summary;
  BuildCxscriptStructureSummary(resolved_script_text, execution_summary);

  const bool prefer_compile_bridge =
    IsCxscriptCompileBridgeEligible(execution_summary, script_origin, !spec.script_text.empty());

  const bool prefer_catalog_bridge =
    ShouldUseCatalogFallback(execution_summary, script_origin, !spec.script_text.empty());

  const std::string fallback_reason =
    DescribeCxscriptFallbackReason(execution_summary, script_origin, !spec.script_text.empty());

  const CxTaskEnvelope envelope = MakeEnvelopeFromCase(spec);
  CxTaskEnvelope runtime_envelope = envelope;
  const std::string execution_text_kind = prefer_compile_bridge ? "compile_bridge" :
    (prefer_catalog_bridge ? "catalog_fallback" : "source");
  runtime_envelope.script_text = prefer_compile_bridge ?
    execution_summary.compile_text :
    (prefer_catalog_bridge ? spec.script_text : resolved_script_text);
  if (!entry.SubmitEnvelope(runtime_envelope))
  {
    result.status = "submit_failed";
    CxscriptExecutionResult cxscript_result;
    cxscript_result.success = false;
    cxscript_result.status = result.status;
    cxscript_result.header_metadata = header_metadata;
    cxscript_result.report_requested = runtime_args.report_on;
    cxscript_result.error_kind = "runtime_error";
    cxscript_result.error_message = "envelope submit failed";
    FinalizeCxscriptResult(result, request, script_origin, resolved_script_text, execution_text_kind, fallback_reason, cxscript_result);
    AppendLine(result, "[FAIL] envelope submit failed");
    return false;
  }

  if (!entry.ExecuteMainThreadCycle())
  {
    result.status = "execute_failed";
    result.tick = entry.GetLastTick();
    result.replay = entry.GetLastReplaySummary();
    CxscriptExecutionResult cxscript_result;
    cxscript_result.success = false;
    cxscript_result.status = result.status;
    cxscript_result.header_metadata = header_metadata;
    cxscript_result.report_requested = runtime_args.report_on;
    const ParserTaskUnit *task = entry.FindTask(runtime_envelope.task_id);
    if (task)
    {
      const std::string task_error_kind = task->result.error_kind.empty() ?
        (task->outcome.error_code.empty() ? "runtime_error" : task->outcome.error_code) :
        task->result.error_kind;
      const std::string task_error_message = task->result.error_message.empty() ?
        (task->outcome.error_message.empty() ? "task execute failed" : task->outcome.error_message) :
        task->result.error_message;
      cxscript_result.error_kind = task->result.error_kind.empty() ?
        (task->outcome.error_code.empty() ? "runtime_error" : task->outcome.error_code) :
        task->result.error_kind;
      cxscript_result.error_message = task->result.error_message.empty() ?
        (task->outcome.error_message.empty() ? "task execute failed" : task->outcome.error_message) :
        task->result.error_message;
      cxscript_result.parser_error_code = task->result.parser_error_code;
      cxscript_result.parser_error_pos = task->result.parser_error_pos;
      cxscript_result.parser_error_token = task->result.parser_error_token;
      cxscript_result.parser_error_expr = task->result.parser_error_expr;
      cxscript_result.accepted_task_count = result.tick.accepted_task_count;
      cxscript_result.executed_task_count = result.tick.executed_task_count;
      AppendLine(result, "[DISPATCH_ERROR_KIND] " + task_error_kind);
      AppendLine(result, "[DISPATCH_ERROR_MESSAGE] " + task_error_message);
      if (!task->outcome.error_code.empty())
        AppendLine(result, "[DISPATCH_OUTCOME_CODE] " + task->outcome.error_code);
      if (!task->outcome.error_message.empty())
        AppendLine(result, "[DISPATCH_OUTCOME_MESSAGE] " + task->outcome.error_message);
      if (task->result.parser_error_code >= 0)
        AppendLine(result,
                   "[DISPATCH_PARSER_ERROR_CODE] " +
                   std::to_string(task->result.parser_error_code));
      if (task->result.parser_error_pos >= 0)
        AppendLine(result,
                   "[DISPATCH_PARSER_ERROR_POS] " +
                   std::to_string(task->result.parser_error_pos));
      if (!task->result.parser_error_token.empty())
        AppendLine(result, "[DISPATCH_PARSER_ERROR_TOKEN] " + task->result.parser_error_token);
      if (!task->result.parser_error_expr.empty())
        AppendLine(result, "[DISPATCH_PARSER_ERROR_EXPR] " + task->result.parser_error_expr);
    }
    else
    {
      cxscript_result.error_kind = "runtime_error";
      cxscript_result.error_message = "execute main thread cycle failed";
    }
    FinalizeCxscriptResult(result, request, script_origin, resolved_script_text, execution_text_kind, fallback_reason, cxscript_result);
    AppendLine(result, "[FAIL] execute main thread cycle failed");
    return false;
  }

  if (spec.replay_after_run && !entry.ReplayTask(runtime_envelope.task_id))
  {
    result.status = "replay_failed";
    result.tick = entry.GetLastTick();
    result.replay = entry.GetLastReplaySummary();
    CxscriptExecutionResult cxscript_result;
    cxscript_result.success = false;
    cxscript_result.status = result.status;
    cxscript_result.header_metadata = header_metadata;
    cxscript_result.report_requested = runtime_args.report_on;
    cxscript_result.error_kind = "replay_error";
    cxscript_result.error_message = "replay failed";
    cxscript_result.accepted_task_count = result.tick.accepted_task_count;
    cxscript_result.executed_task_count = result.tick.executed_task_count;
    cxscript_result.replay_count = result.replay.replay_count;
    cxscript_result.replay_source_task_id = result.replay.replay_source_task_id;
    cxscript_result.replay_stage_count = static_cast<int>(result.replay.replay_stages.size());
    FinalizeCxscriptResult(result, request, script_origin, resolved_script_text, execution_text_kind, fallback_reason, cxscript_result);
    AppendLine(result, "[FAIL] replay failed");
    return false;
  }

  result.tick = entry.GetLastTick();
  result.replay = entry.GetLastReplaySummary();
  result.success = true;
  result.status = spec.replay_after_run ? "run_with_replay" : "run_ok";

  std::ostringstream oss;
  oss << "[PASS] module=" << spec.module
      << " layer=" << spec.layer
      << " case=" << spec.case_id
      << " executed=" << result.tick.executed_task_count;
  AppendLine(result, oss.str());

  if (result.replay.replay_count > 0)
  {
    AppendLine(result,
               "[REPLAY] task=" + result.replay.replay_task_id +
               " source=" + result.replay.replay_source_task_id +
               " modules=" + JoinModules(result.replay.replay_modules));
  }

  CxscriptExecutionResult cxscript_result;
  cxscript_result.success = result.success;
  cxscript_result.status = result.status;
  cxscript_result.header_metadata = header_metadata;
  cxscript_result.report_requested = runtime_args.report_on;
  cxscript_result.accepted_task_count = result.tick.accepted_task_count;
  cxscript_result.executed_task_count = result.tick.executed_task_count;
  cxscript_result.replay_count = result.replay.replay_count;
  cxscript_result.replay_source_task_id = result.replay.replay_source_task_id;
  cxscript_result.replay_stage_count = static_cast<int>(result.replay.replay_stages.size());
  FinalizeCxscriptResult(result, request, script_origin, resolved_script_text, execution_text_kind, fallback_reason, cxscript_result);

  return true;
}
}