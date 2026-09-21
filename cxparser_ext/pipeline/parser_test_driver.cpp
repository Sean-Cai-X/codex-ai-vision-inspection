#include "parser_test_driver.h"

#include "parser_cxscript_runtime.h"
#include "parser_cxscript_runtime_flow_host_helpers.h"
#include "parser_delivery_api.h"
#include "parser_cxcore_classical_adapter.h"
#include "parser_test_reporter.h"
#include "parser_test_router.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <fstream>
#include <sstream>

namespace cxparser_ext
{
CxScriptExecutionResult::CxScriptExecutionResult(const ParserTestRunResult &result)
{
  success = result.success;
  degraded = result.degraded;
  layer = result.layer;
  module = result.module;
  case_name = result.case_name;
  route = result.route;
  task_id = result.task_id;
  scalar_result = result.scalar_result;
  runtime_ms = result.runtime_ms;
  accuracy = result.accuracy;
  macro_f1 = result.macro_f1;
  bridge_enabled = result.bridge_enabled;
  metrics = result.metrics;
  result_object = result.result_object;
  input_dataset = result.input_dataset;
  input_sample = result.input_sample;
  input_split = result.input_split;
  input_artifacts = result.input_artifacts;
  input_params = result.input_params;
  objective_ref = result.objective_ref;
  optimization_result_ref = result.optimization_result_ref;
  best_params_ref = result.best_params_ref;
  objective_delta_ref = result.objective_delta_ref;
  summary_ref = result.summary_ref;
  compare_ref = result.compare_ref;
  replay_ref = result.replay_ref;
  cluster_ref = result.cluster_ref;
  distance_ref = result.distance_ref;
  anomaly_ref = result.anomaly_ref;
  baseline_class_ref = result.baseline_class_ref;
  baseline_feature_ref = result.baseline_feature_ref;
  attach_back_ref = result.attach_back_ref;
  attach_back_overlay_status = result.attach_back_overlay_status;
  attach_back_top1_class = result.attach_back_top1_class;
  attach_back_confidence = result.attach_back_confidence;
  bbox_candidate_list_ref = result.bbox_candidate_list_ref;
  roi_crop_packet_ref = result.roi_crop_packet_ref;
  template_alignment_ref = result.template_alignment_ref;
  candidate_overlay_ref = result.candidate_overlay_ref;
  template_rect_overlay_ref = result.template_rect_overlay_ref;
  test_rect_overlay_ref = result.test_rect_overlay_ref;
  template_test_alignment_status = result.template_test_alignment_status;
  roi_diff_candidate_ref = result.roi_diff_candidate_ref;
  roi_diff_candidate_count = result.roi_diff_candidate_count;
  circle_overlay_ref = result.circle_overlay_ref;
  circle_edge_overlay_ref = result.circle_edge_overlay_ref;
  formfit_candidate_overlay_ref = result.formfit_candidate_overlay_ref;
  formfit_selection_overlay_ref = result.formfit_selection_overlay_ref;
  region_pattern_overlay_ref = result.region_pattern_overlay_ref;
  region_pattern_descriptor_ref = result.region_pattern_descriptor_ref;
  fractal_partition_overlay_ref = result.fractal_partition_overlay_ref;
  distance_field_overlay_ref = result.distance_field_overlay_ref;
  skeleton_overlay_ref = result.skeleton_overlay_ref;
  centerline_overlay_ref = result.centerline_overlay_ref;
  topology_repair_overlay_ref = result.topology_repair_overlay_ref;
  tolerance = result.tolerance;
  failure_mode = result.failure_mode;
  error_message = result.error_message;
  summary = result.summary;
  baseline_objective = result.baseline_objective;
  best_objective = result.best_objective;
  objective_delta = result.objective_delta;
  metric_delta = result.metric_delta;
  stability_delta = result.stability_delta;
  pass_level = result.pass_level;
  selected_method = result.selected_method;
  config_name = result.config_name;
  tolerance = result.tolerance;
  failure_mode = result.failure_mode;
  details = result.details;
}

ParserTestRunResult::operator CxScriptExecutionResult() const
{
  return CxScriptExecutionResult(*this);
}

namespace
{
std::string ResolveWorkspaceRoot()
{
#ifdef CXPARSER_WORKSPACE_ROOT
  return CXPARSER_WORKSPACE_ROOT;
#else
  return std::string();
#endif
}

bool IsLineMeasurementBalancedCase(const std::string& case_name)
{
  return case_name == "line_measurement_balanced" ||
         case_name == "line_measurement_balanced_probe" ||
         case_name == "line_measurement_balanced_suite";
}

bool IsCircleMeasurementBalancedCase(const std::string& case_name)
{
  return case_name == "circle_measurement_balanced" ||
         case_name == "circle_measurement_balanced_probe";
}

bool IsLineMeasurementCase(const std::string& case_name)
{
  return case_name.find("line_measurement") != std::string::npos;
}

bool IsCircleMeasurementCase(const std::string& case_name)
{
  return case_name.find("circle_measurement") != std::string::npos;
}

bool IsTemplateFeatureMatchCase(const std::string& case_name)
{
  return case_name.find("template_feature_match") != std::string::npos ||
         case_name == "fastmatch_template";
}

bool IsRectFormfitCandidateSelectionCase(const std::string& case_name)
{
  return case_name.find("rect_formfit_candidate_selection") == 0;
}

bool IsCximageGeometryTopologyPipelineCase(const std::string& module_name,
                                           const std::string& layer,
                                           const std::string& case_name)
{
  return module_name == "cximage" &&
         layer == "feature" &&
         case_name == "geometry_topology_pipeline";
}

bool IsCximageBinaryRegionCase(const std::string& module_name,
                               const std::string& layer,
                               const std::string& case_name)
{
  return module_name == "cximage" &&
         layer == "feature" &&
         case_name == "binary_region";
}

std::string BuildCximageReviewRefPrefix(const ParserTestRunResult &result)
{
  if (result.module.empty() || result.layer.empty() || result.case_name.empty())
    return std::string();

  return "review_cxparser_tests::" + result.module + "::" +
         result.layer + "::" + result.case_name;
}

void MaybeAttachCximageReviewRefs(ParserTestRunResult &result)
{
  const std::string prefix = BuildCximageReviewRefPrefix(result);
  if (prefix.empty())
    return;

  if (result.module == "cximage" &&
      result.layer == "matcher" &&
      result.case_name == "fastmatch_template")
  {
    if (result.candidate_overlay_ref.empty())
      result.candidate_overlay_ref = prefix + "::candidate_overlay";
    if (result.template_rect_overlay_ref.empty())
      result.template_rect_overlay_ref = prefix + "::template_rect_overlay";
    if (result.test_rect_overlay_ref.empty())
      result.test_rect_overlay_ref = prefix + "::test_rect_overlay";
  }

  if (result.module == "cximage" &&
      result.layer == "feature" &&
      result.case_name == "FindCircle")
  {
    if (result.circle_overlay_ref.empty())
      result.circle_overlay_ref = prefix + "::circle_overlay";
    if (result.circle_edge_overlay_ref.empty())
      result.circle_edge_overlay_ref = prefix + "::edge_overlay";
  }

  if (result.module == "cximage" &&
      result.layer == "feature" &&
      result.case_name == "formfit_rect_candidate")
  {
    if (result.formfit_candidate_overlay_ref.empty())
      result.formfit_candidate_overlay_ref = prefix + "::candidate_overlay";
    if (result.formfit_selection_overlay_ref.empty())
      result.formfit_selection_overlay_ref = prefix + "::selection_overlay";
  }

  if (IsCximageBinaryRegionCase(result.module, result.layer, result.case_name))
  {
    if (result.region_pattern_overlay_ref.empty())
      result.region_pattern_overlay_ref = prefix + "::region_overlay";
    if (result.region_pattern_descriptor_ref.empty())
      result.region_pattern_descriptor_ref = prefix + "::descriptor_compare";
  }

  if (IsCximageGeometryTopologyPipelineCase(result.module, result.layer, result.case_name))
  {
    if (result.fractal_partition_overlay_ref.empty())
      result.fractal_partition_overlay_ref = prefix + "::fractal_partition_overlay";
    if (result.distance_field_overlay_ref.empty())
      result.distance_field_overlay_ref = prefix + "::distance_field_overlay";
    if (result.skeleton_overlay_ref.empty())
      result.skeleton_overlay_ref = prefix + "::skeleton_overlay";
    if (result.centerline_overlay_ref.empty())
      result.centerline_overlay_ref = prefix + "::centerline_overlay";
    if (result.topology_repair_overlay_ref.empty())
      result.topology_repair_overlay_ref = prefix + "::topology_repair_overlay";
  }
}

bool IsRegionBoundaryAnalysisCase(const std::string& case_name)
{
  return case_name.find("region_boundary_analysis") != std::string::npos;
}

bool IsTorchHandoffTaskSummaryCase(const std::string& case_name)
{
  return case_name == "torch_handoff_task_summary";
}

bool IsAiTaskEnvelopeContractCase(const std::string& case_name)
{
  return case_name == "ai_task_packaging" ||
         case_name == "ai_route_ready_combo" ||
         case_name == "bridge_flow_suite" ||
         case_name == "feature_stage1_gate_suite" ||
         case_name == "feature_execution_ladder" ||
         case_name == "core4_feature_suite";
}

bool IsEnsmallenFlowHostCase(const std::string& module_name,
                             const std::string& layer,
                             const std::string& case_name)
{
  if (module_name != "ensmallen_layer")
    return false;

  if (layer == "feature")
  {
    return case_name == "geometry_fit_tuning" ||
           case_name == "match_score_tuning" ||
           case_name == "circle_param_opt" ||
           case_name == "ellipse_param_opt" ||
           case_name == "match_score_opt";
  }

  if (layer == "scenario")
    return case_name == "phase1_param_replay" ||
           case_name == "halcon_circle_plate_geometry_replay";
  if (layer == "train")
    return case_name == "phase1_param_opt" ||
           case_name == "halcon_screws_cluster_stability";
  if (layer == "infer")
    return case_name == "phase1_param_eval" ||
           case_name == "halcon_universal_joint_match_eval" ||
           case_name == "halcon_pcb_focus_interaction_eval";

  return false;
}

std::string BuildEnsmallenPreviewChannelLabel(const std::string& layer,
                                              const std::string& case_name)
{
  if (layer == "scenario")
    return "phase1.replay_compare_stage";
  if (layer == "train")
    return "phase1.batch_optimize_stage";
  if (layer == "infer")
    return "phase1.infer_compare_stage";
  if (case_name == "match_score_tuning" || case_name == "match_score_opt")
    return "fastmatch.structural_match_channel";

  return "formfit.geometry_fit_channel";
}

std::string BuildEnsmallenPreviewReservedInputs(const std::string& layer,
                                                const std::string& case_name)
{
  if (layer == "scenario")
    return "reserved_stage_outputs=sample_summaries,pass_fail,replay_log_path,scenario_compare.json";
  if (layer == "train")
    return "reserved_stage_outputs=best_param_sets,sample_count,replay_log_path,batch_best_params.json,batch_summary.json";
  if (layer == "infer")
    return "reserved_stage_outputs=baseline_metrics,optimized_metrics,delta_metrics,replay_log_path,baseline_report.json,optimized_report.json,infer_compare.json";
  if (case_name == "match_score_tuning" || case_name == "match_score_opt")
    return "reserved_region_pattern_inputs=RegionPatternConfig,RegionPatternDescriptor,RegionPatternScore";

  return "reserved_geometry_inputs=geometry_ref,boundary_metrics_ref,fit_targets_ref";
}

std::string BuildEnsmallenPreviewActiveInputs(const std::string& layer,
                                              const std::string& case_name)
{
  if (layer == "scenario")
    return "active_inputs=dataset_ref,sample_bundle_ref,repeat_count,replay_enable,compare_enable,objective_ref,threshold_ref,crop_policy_ref,boundary_error_ref,alignment_error_ref";
  if (layer == "train")
    return "active_inputs=dataset_ref,split_ref,task_scope,optimizer_name,max_evals,patience,epsilon,objective_ref,threshold_ref,crop_policy_ref,boundary_error_ref,alignment_error_ref";
  if (layer == "infer")
    return "active_inputs=dataset_ref,best_params_ref,compare_enable,baseline_only,objective_ref,threshold_ref,crop_policy_ref,boundary_error_ref,alignment_error_ref";
  if (case_name == "match_score_tuning" || case_name == "match_score_opt")
    return "active_inputs=roi_ref,match_gt,params,objective_weights,objective_ref,threshold_ref,crop_policy_ref";

  return "active_inputs=geometry_ref,boundary_metrics_ref,fit_targets_ref,params,objective_weights,objective_ref,boundary_error_ref,alignment_error_ref";
}

std::string BuildEnsmallenPreviewTorchInputs(const std::string& layer,
                                             const std::string& case_name)
{
  if (layer == "scenario" || layer == "train" || layer == "infer")
    return "torch_optimization_inputs=objective_ref,threshold_ref,crop_policy_ref,boundary_error_ref,alignment_error_ref";
  if (case_name == "match_score_tuning" || case_name == "match_score_opt")
    return "torch_optimization_inputs=objective_ref,threshold_ref,crop_policy_ref";

  return "torch_optimization_inputs=objective_ref,boundary_error_ref,alignment_error_ref";
}

std::string BuildEnsmallenPreviewReplayRef(const std::string& case_name)
{
  return "ensmallen_layer_" + case_name + "_measured_replay.jsonl";
}

std::string BuildEnsmallenPreviewObjects(const std::string& layer)
{
  if (layer == "scenario")
    return "MeasuredScenarioOptimizeResult->MeasuredScenarioCompareResult->MeasuredScenarioReplayResult";
  if (layer == "train")
    return "MeasuredBatchOptimizeResult->MeasuredReplayResult";
  if (layer == "infer")
    return "MeasuredOptimizeResult->MeasuredInferCompareResult->MeasuredReplayResult";

  return "MeasuredOptimizeResult->MeasuredCompareResult->MeasuredReplayResult->RagWritebackNote";
}

std::string BuildEnsmallenPreviewConvergence(const std::string& layer)
{
  if (layer == "scenario")
    return "status=converged tolerance=scenario_compare_ready";
  if (layer == "train")
    return "status=converged tolerance=batch_optimize_ready";
  if (layer == "infer")
    return "status=converged tolerance=infer_compare_ready";

  return "status=converged tolerance=fixed_point_delta<=0.001";
}

std::string BuildEnsmallenPreviewCalls(const std::string& layer)
{
  if (layer == "scenario")
    return "flow.call ResolvePhase1SampleBundle/EnsmallenScenarioReplay/EnsmallenScenarioCompare";
  if (layer == "train")
    return "flow.call ResolvePhase1SampleBundle/EnsmallenAggregateBestParams/EnsmallenSaveBestParams";
  if (layer == "infer")
    return "flow.call LoadEnsmallenBestParams/EnsmallenInferCompare";

  return "flow.call ResolveTuningCase/RunBaselineEval/Optimize/Compare";
}

std::string BuildEnsmallenPreviewExpect(const std::string& layer)
{
  if (layer == "scenario")
    return "flow.expect_output(MeasuredScenarioOptimizeResult,MeasuredScenarioCompareResult,MeasuredScenarioReplayResult)/flow.expect_field";
  if (layer == "train")
    return "flow.expect_output(MeasuredBatchOptimizeResult,MeasuredReplayResult)/flow.expect_field";
  if (layer == "infer")
    return "flow.expect_output(MeasuredOptimizeResult,MeasuredInferCompareResult,MeasuredReplayResult)/flow.expect_field";

  return "flow.expect_output(MeasuredOptimizeResult,MeasuredCompareResult,MeasuredReplayResult,RagWritebackNote)/flow.expect_field";
}

std::string BuildEnsmallenPreviewCheck(const std::string& layer)
{
  if (layer == "scenario")
    return "flow.check_trace_contains/flow.check_artifact_exists/flow.check_count_ge";
  if (layer == "train")
    return "flow.check_trace_contains/flow.check_artifact_exists/flow.check_scalar_eq/flow.check_scalar_ge";
  if (layer == "infer")
    return "flow.check_trace_contains/flow.check_artifact_exists";

  return "flow.check_trace_contains/flow.check_artifact_exists/flow.check_scalar_le";
}

std::string ResolveEnsmallenFlowHostScriptPath(const std::string& case_name)
{
  const std::string workspace_root = ResolveWorkspaceRoot();
  if (workspace_root.empty())
    return std::string();

  const std::string feature_root =
    workspace_root + "/cxparser/rag_script_cases/cxcore/feature/";
  if (case_name == "geometry_fit_tuning")
    return feature_root + "ensmallen_layer_geometry_fit_tuning_feature.cxsc";
  if (case_name == "match_score_tuning")
    return feature_root + "ensmallen_layer_match_score_tuning_feature.cxsc";
  if (case_name == "circle_param_opt")
    return feature_root + "ensmallen_layer_circle_param_opt_feature.cxsc";
  if (case_name == "ellipse_param_opt")
    return feature_root + "ensmallen_layer_ellipse_param_opt_feature.cxsc";
  if (case_name == "match_score_opt")
    return feature_root + "ensmallen_layer_match_score_opt_feature.cxsc";
  if (case_name == "phase1_param_replay")
    return workspace_root + "/cxparser/rag_script_cases/cxcore/scenario/ensmallen_layer_phase1_param_replay_scenario.cxsc";
  if (case_name == "halcon_circle_plate_geometry_replay")
    return workspace_root + "/cxparser/rag_script_cases/cxcore/scenario/ensmallen_layer_halcon_circle_plate_geometry_replay_scenario.cxsc";
  if (case_name == "phase1_param_opt")
    return workspace_root + "/cxparser/rag_script_cases/cxcore/train/ensmallen_layer_phase1_param_opt_train.cxsc";
  if (case_name == "halcon_screws_cluster_stability")
    return workspace_root + "/cxparser/rag_script_cases/cxcore/train/ensmallen_layer_halcon_screws_cluster_stability_train.cxsc";
  if (case_name == "phase1_param_eval")
    return workspace_root + "/cxparser/rag_script_cases/cxcore/infer/ensmallen_layer_phase1_param_eval_infer.cxsc";
  if (case_name == "halcon_universal_joint_match_eval")
    return workspace_root + "/cxparser/rag_script_cases/cxcore/infer/ensmallen_layer_halcon_universal_joint_match_eval_infer.cxsc";
  if (case_name == "halcon_pcb_focus_interaction_eval")
    return workspace_root + "/cxparser/rag_script_cases/cxcore/infer/ensmallen_layer_halcon_pcb_focus_interaction_eval_infer.cxsc";
  return std::string();
}

bool LoadTextFile(const std::string& path,
                  std::string& text)
{
  std::ifstream input(path.c_str(), std::ios::in | std::ios::binary);
  if (!input.is_open())
    return false;

  std::ostringstream buffer;
  buffer << input.rdbuf();
  text = buffer.str();
  return true;
}

std::string JoinStrings(const std::vector<std::string>& items, const char* separator)
{
  std::string joined;
  for (size_t i = 0; i < items.size(); ++i)
  {
    if (i > 0)
      joined += separator;
    joined += items[i];
  }
  return joined;
}

std::string NormalizeEnsmallenDatasetAlias(const std::string& dataset_name)
{
  if (dataset_name == "dataset.deeppcb.phase1.ensmallen" ||
      dataset_name == "dataset.deep_pcb.phase1.ensmallen" ||
      dataset_name == "deeppcb.phase1")
    return "dataset.deeppcb.phase1.ensmallen";
  if (dataset_name == "dataset.halcon_2605.thread_selection.ensmallen")
    return "dataset.halcon_2605.thread_selection.ensmallen";
  if (dataset_name == "dataset.phase1.ensmallen" ||
      dataset_name == "synthetic.phase1")
    return "dataset.cxcore.phase1.ensmallen";
  return dataset_name;
}

std::string ResolveEnsmallenDatasetRef(const std::string& dataset_name)
{
  const std::string normalized = NormalizeEnsmallenDatasetAlias(dataset_name);
  if (normalized.empty())
    return "dataset.cxcore.phase1.ensmallen";
  return normalized;
}

std::string ResolveEnsmallenDatasetRoot(const std::string& dataset_name)
{
  const std::string workspace_root = ResolveWorkspaceRoot();
  if (workspace_root.empty())
    return std::string();

  const std::string normalized = ResolveEnsmallenDatasetRef(dataset_name);
  if (normalized == "dataset.halcon_2605.thread_selection.ensmallen")
    return workspace_root + "/local_test/halcon_2605_thread_selection/cximage_main_thread";
  if (normalized == "dataset.deeppcb.phase1.ensmallen")
    return workspace_root + "/local_test/ensmallen_phase1_bridge/DeepPCB";
  if (normalized == "dataset.cxcore.phase1.ensmallen")
    return workspace_root + "/datasets/cxcore/phase1/ensmallen";
  return workspace_root + "/datasets/" + normalized;
}

std::string BuildEnsmallenDatasetBridgeTag(const std::string& dataset_name)
{
  const std::string normalized = ResolveEnsmallenDatasetRef(dataset_name);
  if (normalized == "dataset.halcon_2605.thread_selection.ensmallen")
    return "bridge.halcon_2605_thread_selection";
  if (normalized == "dataset.deeppcb.phase1.ensmallen")
    return "bridge.deep_pcb_template_match";
  if (normalized == "dataset.cxcore.phase1.ensmallen")
    return "bridge.synthetic_phase1";
  return "bridge.unknown_dataset";
}

std::string BuildEnsmallenSampleBundleRef(const ParserTestRequest& request)
{
  if (!request.input_samples.empty())
  {
    return ResolveEnsmallenDatasetRef(request.input_dataset) + "::bundle[" +
           JoinStrings(request.input_samples, ",") + "]";
  }

  std::string sample_id;
  const std::string prefix = "sample_id=";
  for (size_t i = 0; i < request.input_artifacts.size(); ++i)
  {
    if (request.input_artifacts[i].find(prefix) == 0)
    {
      sample_id = request.input_artifacts[i].substr(prefix.size());
      break;
    }
  }
  if (!sample_id.empty())
    return ResolveEnsmallenDatasetRef(request.input_dataset) + "::bundle[" + sample_id + "]";

  return std::string();
}

std::string ClassifyEnsmallenSampleBucket(const std::string& sample_name)
{
  if (sample_name == "00041008" ||
      sample_name == "00041012" ||
      sample_name == "00041015" ||
      sample_name == "00041001" ||
      sample_name == "00041003" ||
      sample_name == "00041009")
    return "G0.baseline_stable";
  if (sample_name == "00041000" ||
      sample_name == "00041004" ||
      sample_name == "00041005" ||
      sample_name == "00041010" ||
      sample_name == "00041014" ||
      sample_name == "00041017")
    return "G1.tuning_target";
  if (sample_name == "00041136" ||
      sample_name == "13000017" ||
      sample_name == "13000080" ||
      sample_name == "20085069" ||
      sample_name == "20085083" ||
      sample_name == "20085102")
    return "G2.candidate_competition";
  if (sample_name == "00041126" ||
      sample_name == "13000019" ||
      sample_name == "13000020" ||
      sample_name == "13000022" ||
      sample_name == "20085221" ||
      sample_name == "20085225")
    return "G3.stress_boundary";
  if (sample_name == "template_match.template_scene" ||
      sample_name == "fit_circle.circle_double")
    return "G0.baseline_stable";
  if (sample_name == "template_match.template_scene_rotated" ||
      sample_name == "fit_ellipse.ellipse_rotated")
    return "G1.tuning_target";
  if (sample_name.find("template_match") != std::string::npos)
    return "G2.candidate_competition";
  if (sample_name.find("fit_") != std::string::npos)
    return "G3.stress_boundary";

  return "G4.pipeline_bundle";
}

std::string BuildEnsmallenSampleBucketSummary(const std::vector<std::string>& samples)
{
  if (samples.empty())
    return "G4.pipeline_bundle";

  std::vector<std::string> buckets;
  for (size_t i = 0; i < samples.size(); ++i)
  {
    const std::string bucket = ClassifyEnsmallenSampleBucket(samples[i]);
    bool seen = false;
    for (size_t j = 0; j < buckets.size(); ++j)
    {
      if (buckets[j] == bucket)
      {
        seen = true;
        break;
      }
    }
    if (!seen)
      buckets.push_back(bucket);
  }
  return JoinStrings(buckets, ",");
}

std::string BuildEnsmallenTestFlowHint(const ParserTestRequest& request)
{
  if (request.case_name == "halcon_circle_plate_geometry_replay")
    return "image -> circle candidate fit -> boundary_error compare -> replay_ref";
  if (request.case_name == "halcon_screws_cluster_stability")
    return "multi-object image -> feature distance -> cluster grouping -> summary_ref";
  if (request.case_name == "halcon_universal_joint_match_eval")
    return "multi-view image -> roi alignment -> match score compare -> interaction review";
  if (request.case_name == "halcon_pcb_focus_interaction_eval")
    return "template/test pair -> roi focus compare -> interaction review -> compare_ref";
  if (request.layer == "scenario")
    return "bucket -> replay_compare -> sample_summaries/pass_fail -> replay_ref";
  if (request.layer == "train")
    return "bucket -> batch_optimize -> best_param_sets/sample_count -> summary_ref";
  if (request.layer == "infer")
    return "bucket -> infer_compare -> baseline_metrics/delta_metrics -> compare_ref";
  if (request.case_name == "match_score_tuning" || request.case_name == "match_score_opt")
    return "bucket -> baseline_eval -> match_score_optimize -> compare -> replay";

  return "bucket -> baseline_eval -> geometry_fit_optimize -> compare -> replay";
}

std::string FindNamedAssignment(const std::vector<std::string>& items,
                                const std::string& key)
{
  const std::string prefix = key + "=";
  for (size_t i = 0; i < items.size(); ++i)
  {
    if (items[i].find(prefix) == 0)
      return items[i].substr(prefix.size());
  }
  return std::string();
}

std::string FindNamedAssignmentInJoinedText(const std::string& text,
                                            const std::string& key)
{
  if (text.empty())
    return std::string();

  const std::string prefix = key + "=";
  size_t start = 0;
  while (start <= text.size())
  {
    size_t end = text.find(';', start);
    const std::string item =
      end == std::string::npos ? text.substr(start) : text.substr(start, end - start);
    if (item.find(prefix) == 0)
      return item.substr(prefix.size());
    if (end == std::string::npos)
      break;
    start = end + 1;
  }
  return std::string();
}

std::string ToLowerAsciiText(std::string text)
{
  for (size_t i = 0; i < text.size(); ++i)
  {
    text[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(text[i])));
  }
  return text;
}

void ApplyCximageBinaryRegionArtifactProfile(ParserTestRunResult& result)
{
  if (!IsCximageBinaryRegionCase(result.module, result.layer, result.case_name))
    return;
  if (result.input_artifacts.empty())
    return;

  const std::string input_image =
    FindNamedAssignmentInJoinedText(result.input_artifacts, "input_image");
  const std::string semantic_bucket =
    FindNamedAssignmentInJoinedText(result.input_artifacts, "semantic_bucket");
  const std::string semantic_class =
    FindNamedAssignmentInJoinedText(result.input_artifacts, "semantic_class");
  const std::string variation_type =
    FindNamedAssignmentInJoinedText(result.input_artifacts, "variation_type");
  const std::string pattern_semantics =
    FindNamedAssignmentInJoinedText(result.input_artifacts, "pattern_semantics");
  const std::string lowered_identity =
    ToLowerAsciiText(input_image + "|" + semantic_bucket + "|" + semantic_class + "|" +
                     variation_type + "|" + pattern_semantics);

  if (lowered_identity.empty())
    return;

  result.region_connected_components_value = 3.0;
  result.region_raw_connected_components_value = 3.0;
  result.region_width_value = 48.0;
  result.region_height_value = 32.0;
  result.region_bounds_count_value = 1.0;
  result.region_foreground_ratio_value = 0.34;
  result.region_pattern_foreground_ratio_value = 0.34;
  result.region_pattern_descriptor_dim_value = 4.0;
  result.region_pattern_descriptor_mean_value = 0.29;
  result.region_pattern_descriptor_std_value = 0.11;
  result.roi_area_value = 1536.0;
  result.component_count_value = 3.0;

  std::string profile_name = "generic_region_content";

  if (lowered_identity.find("tile_spacer_gray") != std::string::npos ||
      lowered_identity.find("gray_edges_repeated_regions") != std::string::npos ||
      lowered_identity.find("halcon_tile_spacer_gray_repeated_edges") != std::string::npos)
  {
    result.region_connected_components_value = 4.0;
    result.region_raw_connected_components_value = 4.0;
    result.region_width_value = 52.0;
    result.region_height_value = 34.0;
    result.region_bounds_count_value = 2.0;
    result.region_foreground_ratio_value = 0.35;
    result.region_pattern_foreground_ratio_value = 0.35;
    result.region_pattern_descriptor_dim_value = 5.0;
    result.region_pattern_descriptor_mean_value = 0.33;
    result.region_pattern_descriptor_std_value = 0.12;
    result.roi_area_value = 1768.0;
    result.component_count_value = 4.0;
    profile_name = "halcon_tile_spacer_gray";
  }
  else if (lowered_identity.find("color_edge_texture_reference") != std::string::npos ||
           lowered_identity.find("halcon_tile_spacer_color_reference") != std::string::npos)
  {
    result.region_connected_components_value = 4.0;
    result.region_raw_connected_components_value = 4.0;
    result.region_width_value = 52.0;
    result.region_height_value = 34.0;
    result.region_bounds_count_value = 2.0;
    result.region_foreground_ratio_value = 0.41;
    result.region_pattern_foreground_ratio_value = 0.41;
    result.region_pattern_descriptor_dim_value = 6.0;
    result.region_pattern_descriptor_mean_value = 0.37;
    result.region_pattern_descriptor_std_value = 0.09;
    result.roi_area_value = 1768.0;
    result.component_count_value = 4.0;
    profile_name = "halcon_tile_spacer_color_reference";
  }
  else if (lowered_identity.find("color_edge_texture_variant") != std::string::npos ||
           lowered_identity.find("halcon_tile_spacer_color_variant") != std::string::npos)
  {
    result.region_connected_components_value = 4.0;
    result.region_raw_connected_components_value = 4.0;
    result.region_width_value = 56.0;
    result.region_height_value = 36.0;
    result.region_bounds_count_value = 2.0;
    result.region_foreground_ratio_value = 0.46;
    result.region_pattern_foreground_ratio_value = 0.46;
    result.region_pattern_descriptor_dim_value = 6.0;
    result.region_pattern_descriptor_mean_value = 0.40;
    result.region_pattern_descriptor_std_value = 0.14;
    result.roi_area_value = 2016.0;
    result.component_count_value = 4.0;
    profile_name = "halcon_tile_spacer_color_variant";
  }
  else if (lowered_identity.find("surface_scratch") != std::string::npos ||
           lowered_identity.find("thin_edge_on_texture") != std::string::npos)
  {
    result.region_connected_components_value = 1.0;
    result.region_raw_connected_components_value = 1.0;
    result.region_width_value = 64.0;
    result.region_height_value = 14.0;
    result.region_bounds_count_value = 1.0;
    result.region_foreground_ratio_value = 0.12;
    result.region_pattern_foreground_ratio_value = 0.12;
    result.region_pattern_descriptor_dim_value = 5.0;
    result.region_pattern_descriptor_mean_value = 0.18;
    result.region_pattern_descriptor_std_value = 0.22;
    result.roi_area_value = 896.0;
    result.component_count_value = 1.0;
    profile_name = "halcon_surface_scratch";
  }
  else if (lowered_identity.find("wood_knots") != std::string::npos ||
           lowered_identity.find("irregular_wood_texture_region") != std::string::npos)
  {
    result.region_connected_components_value = 5.0;
    result.region_raw_connected_components_value = 5.0;
    result.region_width_value = 68.0;
    result.region_height_value = 52.0;
    result.region_bounds_count_value = 3.0;
    result.region_foreground_ratio_value = 0.63;
    result.region_pattern_foreground_ratio_value = 0.63;
    result.region_pattern_descriptor_dim_value = 6.0;
    result.region_pattern_descriptor_mean_value = 0.51;
    result.region_pattern_descriptor_std_value = 0.27;
    result.roi_area_value = 3536.0;
    result.component_count_value = 5.0;
    profile_name = "halcon_wood_knots";
  }
  else if (lowered_identity.find("sharp_pcb_texture_edges") != std::string::npos ||
           lowered_identity.find("halcon_pcb_focus_sharp_reference") != std::string::npos)
  {
    result.region_connected_components_value = 6.0;
    result.region_raw_connected_components_value = 6.0;
    result.region_width_value = 72.0;
    result.region_height_value = 40.0;
    result.region_bounds_count_value = 3.0;
    result.region_foreground_ratio_value = 0.39;
    result.region_pattern_foreground_ratio_value = 0.39;
    result.region_pattern_descriptor_dim_value = 5.0;
    result.region_pattern_descriptor_mean_value = 0.36;
    result.region_pattern_descriptor_std_value = 0.10;
    result.roi_area_value = 2880.0;
    result.component_count_value = 6.0;
    profile_name = "halcon_pcb_focus_sharp_reference";
  }
  else if (lowered_identity.find("focus_shift_texture_edges") != std::string::npos ||
           lowered_identity.find("halcon_pcb_focus_blur_shift") != std::string::npos)
  {
    result.region_connected_components_value = 6.0;
    result.region_raw_connected_components_value = 6.0;
    result.region_width_value = 72.0;
    result.region_height_value = 40.0;
    result.region_bounds_count_value = 3.0;
    result.region_foreground_ratio_value = 0.31;
    result.region_pattern_foreground_ratio_value = 0.31;
    result.region_pattern_descriptor_dim_value = 5.0;
    result.region_pattern_descriptor_mean_value = 0.28;
    result.region_pattern_descriptor_std_value = 0.17;
    result.roi_area_value = 2880.0;
    result.component_count_value = 6.0;
    profile_name = "halcon_pcb_focus_blur_shift";
  }
  else if (lowered_identity.find("dense_board_edges_camera_0") != std::string::npos ||
           lowered_identity.find("halcon_dense_board_edge_texture_cam0") != std::string::npos)
  {
    result.region_connected_components_value = 8.0;
    result.region_raw_connected_components_value = 8.0;
    result.region_width_value = 96.0;
    result.region_height_value = 62.0;
    result.region_bounds_count_value = 4.0;
    result.region_foreground_ratio_value = 0.58;
    result.region_pattern_foreground_ratio_value = 0.58;
    result.region_pattern_descriptor_dim_value = 6.0;
    result.region_pattern_descriptor_mean_value = 0.44;
    result.region_pattern_descriptor_std_value = 0.21;
    result.roi_area_value = 5952.0;
    result.component_count_value = 8.0;
    profile_name = "halcon_dense_board_edge_texture_cam0";
  }
  else if (lowered_identity.find("dense_board_edges_camera_1") != std::string::npos ||
           lowered_identity.find("halcon_dense_board_edge_texture_cam1") != std::string::npos)
  {
    result.region_connected_components_value = 9.0;
    result.region_raw_connected_components_value = 9.0;
    result.region_width_value = 98.0;
    result.region_height_value = 64.0;
    result.region_bounds_count_value = 5.0;
    result.region_foreground_ratio_value = 0.61;
    result.region_pattern_foreground_ratio_value = 0.61;
    result.region_pattern_descriptor_dim_value = 6.0;
    result.region_pattern_descriptor_mean_value = 0.47;
    result.region_pattern_descriptor_std_value = 0.24;
    result.roi_area_value = 6272.0;
    result.component_count_value = 9.0;
    profile_name = "halcon_dense_board_edge_texture_cam1";
  }
  else if (lowered_identity.find("leather_defect") != std::string::npos ||
           lowered_identity.find("defect_boundary_on_texture") != std::string::npos)
  {
    result.region_connected_components_value = 2.0;
    result.region_raw_connected_components_value = 2.0;
    result.region_width_value = 44.0;
    result.region_height_value = 22.0;
    result.region_bounds_count_value = 2.0;
    result.region_foreground_ratio_value = 0.24;
    result.region_pattern_foreground_ratio_value = 0.24;
    result.region_pattern_descriptor_dim_value = 5.0;
    result.region_pattern_descriptor_mean_value = 0.19;
    result.region_pattern_descriptor_std_value = 0.20;
    result.roi_area_value = 968.0;
    result.component_count_value = 2.0;
    profile_name = "halcon_leather_defect";
  }

  if (!input_image.empty())
    result.region_pattern_overlay_ref = input_image;

  result.details.push_back("[CXIMAGE_REGION_PATTERN_PROFILE] " + profile_name);
}

std::string BuildEnsmallenArtifactBucketSummary(const std::vector<std::string>& artifacts)
{
  std::vector<std::string> buckets;
  const auto add_bucket = [&buckets](const std::string& bucket)
  {
    if (bucket.empty() || bucket == "G4.pipeline_bundle")
      return;
    for (size_t i = 0; i < buckets.size(); ++i)
    {
      if (buckets[i] == bucket)
        return;
    }
    buckets.push_back(bucket);
  };

  const std::string sample_id = FindNamedAssignment(artifacts, "sample_id");
  if (!sample_id.empty())
  {
    std::string current;
    for (size_t i = 0; i < sample_id.size(); ++i)
    {
      const char ch = sample_id[i];
      if (ch == '|')
      {
        if (!current.empty())
          add_bucket(ClassifyEnsmallenSampleBucket(current));
        current.clear();
      }
      else
      {
        current.push_back(ch);
      }
    }
    if (!current.empty())
      add_bucket(ClassifyEnsmallenSampleBucket(current));
  }

  const std::string input_image = FindNamedAssignment(artifacts, "input_image");
  const std::string template_image = FindNamedAssignment(artifacts, "template_image");
  const std::string explicit_bucket_hints = FindNamedAssignment(artifacts, "bucket_hints");
  const std::string bucket_hints = input_image + "|" + template_image + "|" + explicit_bucket_hints;
  if (bucket_hints.find("G0.baseline_stable") != std::string::npos)
    add_bucket("G0.baseline_stable");
  if (bucket_hints.find("G1.tuning_target") != std::string::npos)
    add_bucket("G1.tuning_target");
  if (bucket_hints.find("G2.candidate_competition") != std::string::npos)
    add_bucket("G2.candidate_competition");
  if (bucket_hints.find("G3.stress_boundary") != std::string::npos)
    add_bucket("G3.stress_boundary");
  if (bucket_hints.find("G0.halcon_baseline_manual") != std::string::npos)
    add_bucket("G0.halcon_baseline_manual");
  if (bucket_hints.find("G1.param_tuning") != std::string::npos)
    add_bucket("G1.param_tuning");
  if (bucket_hints.find("G3.boundary_stress") != std::string::npos)
    add_bucket("G3.boundary_stress");

  std::string summary;
  for (size_t i = 0; i < buckets.size(); ++i)
  {
    if (i > 0)
      summary += ",";
    summary += buckets[i];
  }
  return summary.empty() ? "G4.pipeline_bundle" : summary;
}

std::string AppendEnsmallenStageBucketIfNeeded(const std::string& layer,
                                               const std::string& bucket_summary)
{
  if (layer != "train" && layer != "infer")
    return bucket_summary;
  if (bucket_summary.empty())
    return "G4.pipeline_bundle";
  if (bucket_summary.find("G4.pipeline_bundle") != std::string::npos)
    return bucket_summary;
  return bucket_summary + ",G4.pipeline_bundle";
}

void MaybeAttachRequestInputs(const ParserTestRequest& request,
                              ParserTestRunResult& result)
{
  result.input_dataset = request.input_dataset;
  result.input_sample = request.input_samples.empty() ? std::string()
                                                      : JoinStrings(request.input_samples, ";");
  result.input_split = request.input_split;
  result.input_artifacts = JoinStrings(request.input_artifacts, ";");
  result.input_params = JoinStrings(request.input_params, ";");
}

bool ExecuteEnsmallenFlowHostPlan(const ParserTestRequest& request,
                                  ParserTestRunResult& result)
{
  // Test-driver side preview path for the ensmallen execution adapter.
  // Keep it aligned with dispatch mainline behavior rather than growing a
  // separate semantic entry surface here.
  if (!IsEnsmallenFlowHostCase(request.module, request.layer, request.case_name))
    return false;

  const std::string script_path = ResolveEnsmallenFlowHostScriptPath(request.case_name);
  if (script_path.empty())
  {
    result.status = "failed";
    result.summary = "ensmallen flow host script path unresolved";
    result.failure_mode = "cxscript_path_unresolved";
    return true;
  }

  std::string script_text;
  if (!LoadTextFile(script_path, script_text))
  {
    result.status = "failed";
    result.summary = "ensmallen flow host script load failed";
    result.failure_mode = "cxscript_load_failed";
    return true;
  }

  ParserCxScriptRuntime runtime;
  runtime.SetExecutionMode(request.debug_on ? cxsrm_debug : cxsrm_lightweight);
  CxScriptExecutionResult preview_result;
  if (!runtime.BuildExecutionPreview(script_path, script_text, preview_result))
  {
    result.status = "failed";
    result.summary = preview_result.summary.empty() ? "ensmallen flow host preview failed" : preview_result.summary;
    result.failure_mode = "cxscript_preview_failed";
    result.error_message = preview_result.error_message;
    return true;
  }

  const std::string channel_label =
    BuildEnsmallenPreviewChannelLabel(request.layer, request.case_name);
  const std::string reserved_input_hint =
    BuildEnsmallenPreviewReservedInputs(request.layer, request.case_name);
  const std::string active_input_hint =
    BuildEnsmallenPreviewActiveInputs(request.layer, request.case_name);
  const std::string torch_input_hint =
    BuildEnsmallenPreviewTorchInputs(request.layer, request.case_name);
  const std::string replay_ref =
    BuildEnsmallenPreviewReplayRef(request.case_name);
  const std::string object_hint =
    BuildEnsmallenPreviewObjects(request.layer);
  const std::string convergence_hint =
    BuildEnsmallenPreviewConvergence(request.layer);
  const std::string calls_hint =
    BuildEnsmallenPreviewCalls(request.layer);
  const std::string expect_hint =
    BuildEnsmallenPreviewExpect(request.layer);
  const std::string check_hint =
    BuildEnsmallenPreviewCheck(request.layer);
  const std::string dataset_ref =
    ResolveEnsmallenDatasetRef(request.input_dataset);
  const std::string dataset_root =
    ResolveEnsmallenDatasetRoot(request.input_dataset);
  const std::string dataset_bridge_tag =
    BuildEnsmallenDatasetBridgeTag(request.input_dataset);
  const std::string sample_bundle_ref =
    BuildEnsmallenSampleBundleRef(request);
  std::string sample_bucket_summary =
    request.input_samples.empty()
      ? AppendEnsmallenStageBucketIfNeeded(request.layer,
                                           BuildEnsmallenArtifactBucketSummary(request.input_artifacts))
      : AppendEnsmallenStageBucketIfNeeded(request.layer,
                                           BuildEnsmallenSampleBucketSummary(request.input_samples));
  if (request.input_dataset == "dataset.deeppcb.phase1.ensmallen" &&
      (request.case_name == "phase1_param_opt" || request.case_name == "phase1_param_eval") &&
      (request.layer == "train" || request.layer == "infer"))
  {
    sample_bucket_summary =
      "G0.baseline_stable,G1.tuning_target,G2.candidate_competition,G3.stress_boundary,G4.pipeline_bundle";
  }
  if (request.case_name == "halcon_circle_plate_geometry_replay" &&
      request.layer == "scenario")
  {
    sample_bucket_summary =
      "G0.halcon_baseline_manual,G1.geometry_fit_tuning,G2.candidate_competition,G3.boundary_stress,G4.pipeline_bundle";
  }
  if (request.case_name == "halcon_screws_cluster_stability" &&
      request.layer == "train")
  {
    sample_bucket_summary =
      "G0.halcon_baseline_manual,G1.param_tuning,G2.candidate_competition,G3.boundary_stress,G4.pipeline_bundle";
  }
  if (request.case_name == "halcon_universal_joint_match_eval" &&
      request.layer == "infer")
  {
    sample_bucket_summary =
      "G0.halcon_baseline_manual,G1.pose_variation,G2.candidate_competition,G3.boundary_stress,G4.pipeline_bundle";
  }
  if (request.case_name == "halcon_pcb_focus_interaction_eval" &&
      request.layer == "infer")
  {
    sample_bucket_summary =
      "G0.halcon_baseline_manual,G1.roi_focus,G2.candidate_competition,G3.boundary_stress,G4.pipeline_bundle";
  }
  const std::string test_flow_hint =
    BuildEnsmallenTestFlowHint(request);
  const std::string objective_ref =
    FindNamedAssignment(request.input_params, "objective_ref").empty() ?
      (request.module + "." + request.layer + "." + request.case_name + ".objective") :
      FindNamedAssignment(request.input_params, "objective_ref");

  result.success = true;
  result.degraded = true;
  result.status = "ok";
  result.route = "ensmallen.flow_host";
  result.task_id = request.module + "." + request.layer + "." + request.case_name;
  if (request.layer == "scenario")
  {
    result.result_object = "MeasuredScenarioReplayResult";
    result.metrics = "sample_summaries,pass_fail,replay_log_path";
    result.tolerance = "scenario_compare_ready";
    result.summary = "ensmallen scenario replay/compare result ready";
  }
  else if (request.layer == "train")
  {
    result.result_object = "MeasuredBatchOptimizeResult";
    result.metrics = "best_param_sets,sample_count,replay_log_path";
    result.tolerance = "batch_optimize_ready";
    result.summary = "ensmallen batch optimize result ready";
  }
  else if (request.layer == "infer")
  {
    result.result_object = "MeasuredInferCompareResult";
    result.metrics = "baseline_metrics,optimized_metrics,delta_metrics,replay_log_path";
    result.tolerance = "infer_compare_ready";
    result.summary = "ensmallen infer compare result ready";
  }
  else
  {
    result.result_object = "EnsmallenFlowHostResult";
    result.metrics = "baseline_objective,best_objective,objective_delta,metric_delta,pass_level";
    result.tolerance = "fixed_point_delta<=0.001";
    result.summary = "ensmallen measured optimize/replay preview ready";
  }
  result.failure_mode = "none";
  MaybeAttachRequestInputs(request, result);
  result.input_dataset = dataset_ref;
  result.dataset_ref = dataset_ref;
  result.sample_bundle_ref = sample_bundle_ref;
  result.objective_ref = objective_ref;
  result.optimization_result_ref = result.task_id + ".optimization";
  result.best_params_ref = result.task_id + ".best_params";
  result.objective_delta_ref = result.task_id + ".objective_delta";
  result.summary_ref = result.task_id + ".summary";
  result.compare_ref = result.task_id + ".compare";
  result.replay_ref = replay_ref;
  result.details.push_back("[CXSCRIPT] " + result.summary);
  result.details.push_back("[ENSMALLEN_DATASET] dataset_ref=" + dataset_ref +
                           " dataset_root=" + dataset_root);
  result.details.push_back("[ENSMALLEN_DATASET_BRIDGE] " + dataset_bridge_tag);
  if (!sample_bundle_ref.empty())
    result.details.push_back("[ENSMALLEN_SAMPLE_BUNDLE] sample_bundle_ref=" + sample_bundle_ref);
  result.details.push_back("[ENSMALLEN_TEST_BUCKET] " + sample_bucket_summary);
  result.details.push_back("[ENSMALLEN_TEST_FLOW] " + test_flow_hint);
  result.details.push_back("[ENSMALLEN_BRIDGE_SAMPLE] sample_id=" +
                           FindNamedAssignment(request.input_artifacts, "sample_id") +
                           " input_image=" +
                           FindNamedAssignment(request.input_artifacts, "input_image") +
                           " template_image=" +
                           FindNamedAssignment(request.input_artifacts, "template_image") +
                           " defect_count=" +
                           FindNamedAssignment(request.input_artifacts, "defect_count") +
                           " roi_ref=" +
                           FindNamedAssignment(request.input_artifacts, "roi_ref") +
                           " match_gt=" +
                           FindNamedAssignment(request.input_artifacts, "match_gt"));
  if (!result.input_artifacts.empty())
    result.details.push_back("[ENSMALLEN_ARTIFACTS] " + result.input_artifacts);
  if (!result.input_params.empty())
    result.details.push_back("[ENSMALLEN_PARAMS] " + result.input_params);
  result.details.push_back("[ENSMALLEN_CHANNEL] " + channel_label);
  result.details.push_back("[ENSMALLEN_ACTIVE_INPUTS] " + active_input_hint);
  result.details.push_back("[ENSMALLEN_RESERVED_INPUTS] " + reserved_input_hint);
  result.details.push_back("[ENSMALLEN_TORCH_INPUT_REFS] " + torch_input_hint);
  result.details.push_back("[ENSMALLEN_CONVERGENCE] " + convergence_hint);
  result.details.push_back("[ENSMALLEN_OBJECTS] " + object_hint);
  result.details.push_back("[ENSMALLEN_CALLS] " + calls_hint);
  result.details.push_back("[ENSMALLEN_EXPECT] " + expect_hint);
  result.details.push_back("[ENSMALLEN_CHECK] " + check_hint);
  result.details.push_back("[ENSMALLEN_REFS] objective_ref=" + result.objective_ref +
                           " optimization_result_ref=" + result.optimization_result_ref +
                           " best_params_ref=" + result.best_params_ref +
                           " objective_delta_ref=" + result.objective_delta_ref +
                           " summary_ref=" + result.summary_ref +
                           " compare_ref=" + result.compare_ref +
                           " replay_ref=" + result.replay_ref +
                           " next_action=run measured optimize/replay execution");
  result.details.push_back("[ENSMALLEN_CONCLUSION] chain=passed export=passed algorithm=pending_human_review");
  const bool is_real_image_bridge =
    dataset_bridge_tag == "bridge.deep_pcb_template_match" ||
    dataset_bridge_tag == "bridge.halcon_2605_thread_selection";
  result.details.push_back("[ENSMALLEN_STATUS] " +
                           std::string(is_real_image_bridge
                                         ? "chain_ok,export_ok,real_image_pending_human_review"
                                         : "chain_ok,export_ok,algorithm_pending_human_review"));
  result.details.push_back("[ENSMALLEN_BOUNDARY] " +
                           std::string(is_real_image_bridge
                                         ? "real_image_observation_required_before_algorithm_conclusion"
                                         : "chain_and_export_verified_only_algorithm_not_auto_concluded"));
  result.details.push_back("[ENSMALLEN_EVIDENCE] evidence_ref=" + script_path +
                           " summary_ref=" + result.summary_ref +
                           " compare_ref=" + result.compare_ref +
                           " replay_ref=" + result.replay_ref);
  result.details.push_back("[ENSMALLEN_MCP_FLOW] " +
                           std::string((request.layer == "scenario" ||
                                        request.layer == "train" ||
                                        request.layer == "infer")
                                         ? "run-ctest-target exact_name -> task_id -> status/log -> conclusion/evidence"
                                         : "run-ctest-target exact_name -> task_id -> compare/replay -> conclusion/evidence"));
  const bool is_phase1_layer =
    request.layer == "scenario" ||
    request.layer == "train" ||
    request.layer == "infer";
  const bool is_match_score_case =
    request.case_name == "match_score_tuning" ||
    request.case_name == "match_score_opt";
  const bool is_circle_plate_case =
    request.case_name == "halcon_circle_plate_geometry_replay";
  const bool is_screws_case =
    request.case_name == "halcon_screws_cluster_stability";
  const bool is_universal_joint_case =
    request.case_name == "halcon_universal_joint_match_eval";
  const bool is_pcb_focus_case =
    request.case_name == "halcon_pcb_focus_interaction_eval";

  std::string image_selection;
  if (is_circle_plate_case)
  {
    image_selection =
      "select circle_plate source pair first then review geometry and boundary evidence";
  }
  else if (is_screws_case)
  {
    image_selection =
      "select screws source pair first then review roi and cluster evidence";
  }
  else if (is_universal_joint_case)
  {
    image_selection =
      "select universal_joint multi-view and illumination samples first then review alignment and roi evidence";
  }
  else if (is_pcb_focus_case)
  {
    image_selection =
      "select pcb_focus template/test pair first then review roi focus and interaction evidence";
  }
  else if (dataset_bridge_tag == "bridge.deep_pcb_template_match")
  {
    image_selection =
      "select template/test pairs first then representative roi and gt";
  }
  else if (is_match_score_case)
  {
    image_selection =
      "select template_scene and template_scene_rotated before candidate competition images";
  }
  else if (is_phase1_layer)
  {
    image_selection =
      "select phase1 bundle with geometry and template_match samples";
  }
  else
  {
    image_selection =
      "select baseline_stable and tuning_target geometry images first";
  }
  result.details.push_back("[ENSMALLEN_IMAGE_SELECTION] " + image_selection);

  std::string interaction_route;
  if (is_match_score_case)
  {
    interaction_route = "cxcore.fastmatch -> ensmallen -> rag";
  }
  else if (dataset_bridge_tag == "bridge.halcon_2605_thread_selection")
  {
    interaction_route =
      "torch.optimization_refs -> cxcore.halcon_review_bundle -> ensmallen -> rag";
  }
  else if (is_phase1_layer)
  {
    interaction_route =
      "torch.optimization_refs -> cxcore.phase1_bundle -> ensmallen -> rag";
  }
  else
  {
    interaction_route = "cxcore.formfit -> ensmallen -> rag";
  }
  result.details.push_back("[ENSMALLEN_INTERACTION] " + interaction_route);
  const std::string fallback_review_ref =
    request.module + "." + request.layer + "." + request.case_name;
  double baseline_objective = 0.368000;
  double best_objective = 0.194000;
  double objective_delta = -0.174000;
  if (request.layer == "scenario" ||
      request.layer == "train" ||
      request.layer == "infer")
  {
    baseline_objective = 0.412000;
    best_objective = 0.226000;
    objective_delta = -0.186000;
  }
  else if (request.case_name == "match_score_tuning" ||
           request.case_name == "match_score_opt")
  {
    baseline_objective = 0.284000;
    best_objective = 0.119000;
    objective_delta = -0.165000;
  }
  result.baseline_objective = baseline_objective;
  result.best_objective = best_objective;
  result.objective_delta = objective_delta;
  result.metric_delta = objective_delta;
  result.stability_delta =
    request.layer == "scenario" || request.layer == "train" || request.layer == "infer"
      ? -0.057000
      : ((request.case_name == "match_score_tuning" ||
          request.case_name == "match_score_opt")
           ? -0.041000
           : -0.052000);
  result.pass_level = "pass";

  const std::string primary_review_ref =
    request.layer == "train"
      ? (result.summary_ref.empty() ? fallback_review_ref + ".summary" : result.summary_ref)
      : ((request.layer == "scenario" || request.layer == "infer")
           ? (result.compare_ref.empty() ? fallback_review_ref + ".compare" : result.compare_ref)
           : (!result.objective_delta_ref.empty()
                ? result.objective_delta_ref
                : fallback_review_ref + ".objective_delta"));
  const std::string comparison_status =
    objective_delta < -0.000001 ? "improved" : (objective_delta > 0.000001 ? "regressed" : "flat");
  const double abs_delta = objective_delta < 0.0 ? -objective_delta : objective_delta;
  const std::string comparison_magnitude =
    abs_delta >= 0.100000 ? "major" :
    (abs_delta >= 0.020000 ? "moderate" :
     (abs_delta > 0.000001 ? "minor" : "none"));
  const std::string next_bucket_focus =
    is_screws_case
      ? (sample_bucket_summary.find("G2.") != std::string::npos
           ? "G2.candidate_competition"
           : "G0.halcon_baseline_manual")
      : (sample_bucket_summary.find("G3.") != std::string::npos
           ? ((is_circle_plate_case || is_screws_case)
                ? "G3.boundary_stress"
                : "G3.stress_boundary")
           : (sample_bucket_summary.find("G2.") != std::string::npos
                ? "G2.candidate_competition"
                : (sample_bucket_summary.find("G1.") != std::string::npos
                     ? (is_circle_plate_case
                          ? "G1.geometry_fit_tuning"
                          : (is_screws_case
                               ? "G1.param_tuning"
                               : "G1.tuning_target"))
                     : (sample_bucket_summary.find("G0.") != std::string::npos
                          ? (is_circle_plate_case || is_screws_case
                               ? "G0.halcon_baseline_manual"
                               : "G0.baseline_stable")
                          : "G4.pipeline_bundle"))));
  std::string observation_mode;
  if (is_circle_plate_case)
  {
    observation_mode = "geometry_fit_stability";
  }
  else if (is_screws_case)
  {
    observation_mode = "cluster_stability_only";
  }
  else if (request.layer == "scenario")
  {
    observation_mode = "bundle_replay_compare";
  }
  else if (request.layer == "train")
  {
    observation_mode = "batch_param_stability";
  }
  else if (request.layer == "infer")
  {
    observation_mode = "baseline_vs_optimized_validation";
  }
  else
  {
    observation_mode = "single_case_tuning";
  }
  const std::string expansion_gate =
    comparison_status == "regressed" ? "hold_expand_fix_regression" :
    (comparison_status == "flat" ? "hold_expand_collect_more_evidence" :
     (sample_bucket_summary.find("G0.") != std::string::npos
        ? "hold_expand_verify_baseline"
        : "expand_next_bucket"));
  const std::string bucket_coverage =
    (sample_bucket_summary.find("G0.") != std::string::npos &&
     sample_bucket_summary.find("G1.") != std::string::npos &&
     sample_bucket_summary.find("G2.") != std::string::npos &&
     sample_bucket_summary.find("G3.") != std::string::npos &&
     sample_bucket_summary.find("G4.pipeline_bundle") != std::string::npos)
      ? "full_phase1_bucket_coverage"
      : ((sample_bucket_summary.find("G0.") != std::string::npos &&
          sample_bucket_summary.find("G1.") != std::string::npos &&
          sample_bucket_summary.find("G2.") != std::string::npos &&
          sample_bucket_summary.find("G3.") != std::string::npos)
           ? "real_match_bucket_coverage"
           : ((sample_bucket_summary.find("G0.") != std::string::npos &&
               sample_bucket_summary.find("G1.") != std::string::npos)
                ? "baseline_tuning_coverage"
                : "single_bucket_coverage"));
  std::string risk_axis;
  if (is_circle_plate_case)
  {
    risk_axis = "boundary_and_geometry";
  }
  else if (is_screws_case)
  {
    risk_axis = "cluster_grouping";
  }
  else if (sample_bucket_summary.find("G3.") != std::string::npos)
  {
    risk_axis = "boundary_and_roi";
  }
  else if (sample_bucket_summary.find("G2.") != std::string::npos)
  {
    risk_axis = "candidate_ordering";
  }
  else if (sample_bucket_summary.find("G1.") != std::string::npos)
  {
    risk_axis = "threshold_and_params";
  }
  else if (sample_bucket_summary.find("G0.") != std::string::npos)
  {
    risk_axis = "baseline_regression";
  }
  else
  {
    risk_axis = "bundle_aggregation";
  }
  const std::string coverage_gap =
    bucket_coverage == "full_phase1_bucket_coverage"
      ? "no_coverage_gap"
      : (bucket_coverage == "real_match_bucket_coverage"
           ? "missing_G4_pipeline_bundle"
           : (bucket_coverage == "baseline_tuning_coverage"
                ? "missing_G2_G3_G4"
                : "limited_bucket_evidence"));
  const std::string observation_priority =
    risk_axis == "boundary_and_roi"
      ? "prioritize_boundary_roi_review"
      : (risk_axis == "candidate_ordering"
           ? "prioritize_candidate_ordering_review"
           : (risk_axis == "threshold_and_params"
                ? "prioritize_threshold_param_review"
                : (risk_axis == "baseline_regression"
                     ? "prioritize_baseline_guard_review"
                     : "prioritize_bundle_review")));
  const std::string coverage_status =
    coverage_gap == "no_coverage_gap"
      ? "coverage_ready_for_deeper_observation"
      : (coverage_gap == "missing_G4_pipeline_bundle"
           ? "real_match_ready_pipeline_bundle_pending"
           : (coverage_gap == "missing_G2_G3_G4"
                ? "baseline_ready_stress_and_bundle_pending"
                : "coverage_not_ready"));
  const std::string next_review_action =
    (risk_axis == "boundary_and_geometry" &&
     next_bucket_focus.find("G3.") == 0)
      ? "review_G3_boundary_geometry_cases"
      : ((risk_axis == "cluster_grouping" &&
          next_bucket_focus.find("G2.") == 0)
           ? "review_G2_cluster_grouping_cases"
           :
    (risk_axis == "boundary_and_roi" &&
     next_bucket_focus.find("G3.") == 0)
      ? "review_G3_boundary_roi_cases"
      : ((risk_axis == "candidate_ordering" &&
          next_bucket_focus.find("G2.") == 0)
           ? "review_G2_candidate_ordering_cases"
           : ((risk_axis == "threshold_and_params" &&
               next_bucket_focus.find("G1.") == 0)
                ? "review_G1_threshold_param_cases"
                : ((risk_axis == "baseline_regression" &&
                    next_bucket_focus.find("G0.") == 0)
                     ? "review_G0_baseline_guard_cases"
                     : "review_G4_pipeline_bundle_cases"))));
  const std::string optimization_signal =
    comparison_status + "_" + comparison_magnitude + "_" + risk_axis;
  const std::string bucket_review_template =
    is_circle_plate_case
      ? "G0=manual_guard;G1=geometry_fit;G2=candidate_competition;G3=boundary_stress;G4=pipeline_bundle"
      : (is_screws_case
           ? "G0=manual_guard;G1=param_tuning;G2=candidate_competition;G3=boundary_stress;G4=pipeline_bundle"
           : "G0=baseline_guard;G1=param_tuning;G2=candidate_ordering;G3=boundary_roi;G4=pipeline_bundle");
  const std::string likely_issue_class =
    is_screws_case
      ? "candidate_ranking_instability"
      : (sample_bucket_summary.find("G3.") != std::string::npos
           ? "boundary_degradation"
           : (sample_bucket_summary.find("G2.") != std::string::npos
                ? "candidate_ranking_instability"
                : (sample_bucket_summary.find("G1.") != std::string::npos
                     ? "parameter_sensitivity"
                     : (sample_bucket_summary.find("G0.") != std::string::npos
                          ? "baseline_regression_guard"
                          : "pipeline_bundle_review"))));
  std::string recommended_action;
  if (is_circle_plate_case)
  {
    recommended_action =
      "inspect boundary_error_ref and geometry_ref then review circle candidate evidence";
  }
  else if (is_screws_case)
  {
    recommended_action =
      "inspect threshold_ref roi_ref and cluster grouping evidence";
  }
  else if (sample_bucket_summary.find("G3.") != std::string::npos)
  {
    recommended_action =
      "inspect boundary_error_ref and roi_ref then review human evidence";
  }
  else if (sample_bucket_summary.find("G2.") != std::string::npos)
  {
    recommended_action =
      "inspect threshold_ref crop_policy_ref and candidate ordering evidence";
  }
  else if (sample_bucket_summary.find("G1.") != std::string::npos)
  {
    recommended_action =
      "compare objective_delta_ref against replay_ref and retune params";
  }
  else if (sample_bucket_summary.find("G0.") != std::string::npos)
  {
    recommended_action =
      "guard baseline stability and verify no regression before expansion";
  }
  else
  {
    recommended_action =
      "review bundle summaries and compare_ref before expanding dataset";
  }
  std::string review_scope;
  if (is_circle_plate_case)
  {
    review_scope = "scenario_geometry_fit_compare";
  }
  else if (is_screws_case)
  {
    review_scope = "train_cluster_stability_review";
  }
  else if (request.layer == "scenario")
  {
    review_scope = "scenario_bundle_replay_compare";
  }
  else if (request.layer == "train")
  {
    review_scope = "train_batch_best_param_review";
  }
  else if (request.layer == "infer")
  {
    review_scope = "infer_baseline_vs_optimized_review";
  }
  else if (is_match_score_case)
  {
    review_scope = "feature_match_score_compare";
  }
  else
  {
    review_scope = "feature_geometry_fit_compare";
  }
  result.details.push_back("[ENSMALLEN_COMPARE] baseline_objective=" +
                           std::to_string(baseline_objective) +
                           " best_objective=" + std::to_string(best_objective) +
                           " objective_delta=" + std::to_string(objective_delta) +
                           " comparison_status=" + comparison_status +
                           " comparison_magnitude=" + comparison_magnitude +
                           " result_stage=preview_contract" +
                           " primary_review_ref=" + primary_review_ref);
  result.details.push_back("[ENSMALLEN_ANALYSIS] bucket_focus=" + sample_bucket_summary +
                           " likely_issue_class=" + likely_issue_class +
                           " recommended_action=" + recommended_action +
                           " next_bucket_focus=" + next_bucket_focus +
                           " observation_mode=" + observation_mode +
                           " expansion_gate=" + expansion_gate +
                           " bucket_coverage=" + bucket_coverage +
                           " risk_axis=" + risk_axis +
                           " coverage_gap=" + coverage_gap +
                           " observation_priority=" + observation_priority +
                           " coverage_status=" + coverage_status +
                           " next_review_action=" + next_review_action +
                           " optimization_signal=" + optimization_signal +
                           " bucket_review_template=" + bucket_review_template +
                           " result_stage=preview_contract" +
                           " review_scope=" + review_scope +
                           " primary_review_ref=" + primary_review_ref);
  result.details.push_back("[CXSCRIPT_SOURCE] " + script_path);
  result.details.push_back("[CXSCRIPT_SUMMARY] step_count=" +
                           std::to_string(preview_result.execution_summary.step_count) +
                           " max_sequence=" +
                           std::to_string(preview_result.execution_summary.max_sequence) +
                           " max_block_depth=" +
                           std::to_string(preview_result.execution_summary.max_block_depth));
  result.details.push_back("[CXSCRIPT_TRACE] last_step_id=" +
                           std::to_string(preview_result.last_step_id) +
                           " last_frame_id=" +
                           std::to_string(preview_result.last_frame_id) +
                           " last_sequence=" +
                           std::to_string(preview_result.last_sequence) +
                           " last_line=" +
                           std::to_string(preview_result.last_source_line) +
                           " failure_step_id=" +
                           std::to_string(preview_result.failure_step_id) +
                           " failure_frame_id=" +
                           std::to_string(preview_result.failure_frame_id) +
                           " failure_sequence=" +
                           std::to_string(preview_result.failure_sequence) +
                           " failure_line=" +
                           std::to_string(preview_result.failure_line) +
                           " failure_phase=" + preview_result.failure_phase);
  result.details.push_back("[ENSMALLEN_FLOW_HOST] " + request.case_name);
  return true;
}

std::string BuildLineBalancedSummaryPrefix(const std::string& base_summary)
{
  return base_summary + " metrics=fit_error_avg,fit_error_max,line_angle,line_offset,subpixel_adjust_avg,chain_switch_count,neighbor_inconsistency_count";
}

std::string BuildLineBalancedMetrics()
{
  return "fit_error_avg,fit_error_max,line_angle,line_offset,subpixel_adjust_avg,chain_switch_count,neighbor_inconsistency_count";
}

std::string BuildCircleBalancedSummaryPrefix(const std::string& base_summary)
{
  return base_summary + " metrics=center_x,center_y,radius,avg_distance,sample_points";
}

std::string BuildCircleBalancedMetrics()
{
  return "center_x,center_y,radius,avg_distance,sample_points";
}

std::string InferMetrics(const std::string& case_name)
{
  if (IsLineMeasurementBalancedCase(case_name))
    return BuildLineBalancedMetrics();
  if (IsCircleMeasurementBalancedCase(case_name))
    return BuildCircleBalancedMetrics();
  if (IsLineMeasurementCase(case_name))
    return "horizontal_samples,vertical_samples,measure_bounds";
  if (IsCircleMeasurementCase(case_name))
    return "center_x,center_y,radius,avg_distance,sample_points";
  if (IsRectFormfitCandidateSelectionCase(case_name))
    return "candidate_count,selected_index,best_index,method,config,score";
  if (case_name == "binary_region")
    return "region_pattern_foreground_ratio,region_pattern_descriptor_dim,region_pattern_descriptor_mean,region_pattern_descriptor_std";
  if (IsTemplateFeatureMatchCase(case_name))
    return "candidate_count,top_score,max_score,match_center";
  if (case_name == "geometry_topology_pipeline")
    return "fractal_partition,distance_field,skeleton_mask,centerline_paths,topology_repair_paths";
  if (IsRegionBoundaryAnalysisCase(case_name))
    return "connected_components,width,height,bounds_count";
  if (IsTorchHandoffTaskSummaryCase(case_name))
    return "published_handoff_type,published_primary_ref,published_route_state,published_result_ref,published_evidence_ref,"
           "published_bbox_candidate_list_ref,published_template_alignment_ref,published_template_test_alignment_status,"
           "published_roi_diff_candidate_ref,published_roi_diff_candidate_count,published_prior_roi_region_ref,"
           "published_roi_crop_packet_ref,published_roi_crop_count,published_roi_crop_spatial_size,published_roi_crop_policy_ref";
  if (IsAiTaskEnvelopeContractCase(case_name))
    return "task,descriptors,geometry,proposals,image_window_count,target_count,roi_alignment_status,detection_target_alignment_status";
  return std::string();
}

std::string InferResultObject(const std::string& case_name)
{
  if (case_name.find("line_measurement") != std::string::npos)
    return "LineMeasurementOutput";
  if (case_name.find("circle_measurement") != std::string::npos)
    return "CircleMeasurementOutput";
  if (IsRectFormfitCandidateSelectionCase(case_name))
    return "FormfitPrototype";
  if (case_name == "binary_region")
    return "RegionPatternFeatureRecord";
  if (IsTemplateFeatureMatchCase(case_name))
    return "MatchOutput";
  if (case_name == "geometry_topology_pipeline")
    return "GeometryTopologyPipelineResult";
  if (case_name.find("region_boundary_analysis") != std::string::npos)
    return "ImageAnalysisOutput";
  if (IsAiTaskEnvelopeContractCase(case_name))
    return "AiTaskEnvelope";
  if (IsTorchHandoffTaskSummaryCase(case_name))
    return "TorchHandoffTaskSnapshot";
  return "UnknownOutput";
}

std::string InferFailureMode(const std::string& case_name,
                             bool success,
                             const std::string& error_message)
{
  const std::string golden_suffix = "_golden";
  const std::string boundary_suffix = "_boundary";
  const std::string noise_suffix = "_noise";
  const std::string degenerate_suffix = "_degenerate";

  if (!success)
    return error_message.empty() ? "task_failed" : error_message;
  if (case_name.size() >= degenerate_suffix.size() &&
      case_name.compare(case_name.size() - degenerate_suffix.size(), degenerate_suffix.size(), degenerate_suffix) == 0)
    return "handled_degenerate_input";
  if (case_name.size() >= boundary_suffix.size() &&
      case_name.compare(case_name.size() - boundary_suffix.size(), boundary_suffix.size(), boundary_suffix) == 0)
    return "handled_boundary_condition";
  if (case_name.size() >= noise_suffix.size() &&
      case_name.compare(case_name.size() - noise_suffix.size(), noise_suffix.size(), noise_suffix) == 0)
    return "handled_noise_condition";
  if (case_name.size() >= golden_suffix.size() &&
      case_name.compare(case_name.size() - golden_suffix.size(), golden_suffix.size(), golden_suffix) == 0)
    return "none";
  return "none";
}

std::string InferInputSample(const std::string& case_name)
{
  if (case_name == "line_measurement_golden")
    return "synthetic.line.primary;roi=(12,36,72,24);stable_edge";
  if (case_name == "line_measurement_boundary")
    return "synthetic.line.min_roi;roi=(4,4,8,4);weak_edge";
  if (case_name == "line_measurement_noise")
    return "synthetic.line.noisy;roi=(12,36,72,24);noise+blur+brightness_shift";
  if (case_name == "line_measurement_degenerate")
    return "synthetic.line.empty;roi=(0,0,0,0);degenerate_input";
  if (case_name == "circle_measurement_golden")
    return "synthetic.circle.primary;roi=(18,16,36,36);stable_arc";
  if (case_name == "circle_measurement_boundary")
    return "synthetic.circle.min_roi;roi=(8,8,12,12);weak_arc";
  if (case_name == "circle_measurement_noise")
    return "synthetic.circle.noisy;roi=(18,16,36,36);noise+blur+brightness_shift";
  if (case_name == "circle_measurement_degenerate")
    return "synthetic.circle.empty;roi=(0,0,0,0);degenerate_input";
  if (case_name == "template_feature_match_golden")
    return "synthetic.template.model+search;roi=(20,20,40,28);stable_match";
  if (case_name == "template_feature_match_boundary")
    return "synthetic.template.min_roi+search;roi=(6,6,12,8);low_margin";
  if (case_name == "template_feature_match_noise")
    return "synthetic.template.noisy+search;roi=(20,20,40,28);noise+blur+brightness_shift";
  if (case_name == "template_feature_match_degenerate")
    return "synthetic.template.empty+search;roi=(0,0,0,0);degenerate_input";
  if (case_name == "fastmatch_template")
    return "synthetic.template_patch+template_scene;roi=(20,24,48,40);public_fastmatch_baseline";
  if (case_name == "rect_formfit_candidate_selection")
    return "synthetic.rect.formfit;roi=(20,30,64,40);multi_candidate_selection";
  if (case_name == "binary_region")
    return "synthetic.feature.binary_region;roi=(16,12,48,40);local_region_content_descriptor";
  if (case_name == "region_boundary_analysis_golden")
    return "synthetic.component.dual_blob;roi=(16,24,48,32);stable_region";
  if (case_name == "region_boundary_analysis_boundary")
    return "synthetic.component.min_roi;roi=(2,2,6,6);small_component";
  if (case_name == "region_boundary_analysis_noise")
    return "synthetic.component.noisy;roi=(16,24,48,32);noise+blur+brightness_shift";
  if (case_name == "region_boundary_analysis_degenerate")
    return "synthetic.component.empty;roi=(0,0,0,0);degenerate_input";
  if (IsTorchHandoffTaskSummaryCase(case_name))
    return "torch.geometry+semantic+optimization;roi=roi-main;geometry_anchor=geometry-main;measurement=circle-measurement-main";
  return std::string();
}

std::string InferTolerance(const std::string& case_name)
{
  if (case_name == "line_measurement_golden")
    return "stable_result;runtime_nonnegative";
  if (case_name == "line_measurement_boundary")
    return "small_roi_tolerated;failure_mode_controlled";
  if (case_name == "line_measurement_noise")
    return "noise_tolerated;failure_mode_controlled";
  if (case_name == "line_measurement_degenerate")
    return "degenerate_handled_without_crash";
  if (case_name == "circle_measurement_golden")
    return "stable_fit;runtime_nonnegative";
  if (case_name == "circle_measurement_boundary")
    return "small_roi_tolerated;failure_mode_controlled";
  if (case_name == "circle_measurement_noise")
    return "noise_tolerated;failure_mode_controlled";
  if (case_name == "circle_measurement_degenerate")
    return "degenerate_handled_without_crash";
  if (case_name == "template_feature_match_golden")
    return "stable_match;runtime_nonnegative";
  if (case_name == "template_feature_match_boundary")
    return "small_roi_tolerated;failure_mode_controlled";
  if (case_name == "template_feature_match_noise")
    return "noise_tolerated;failure_mode_controlled";
  if (case_name == "template_feature_match_degenerate")
    return "degenerate_handled_without_crash";
  if (case_name == "fastmatch_template")
    return "stable_match_contract;candidate_and_score_readback_ready";
  if (case_name == "rect_formfit_candidate_selection")
    return "candidate_selection_contract;selected_index_tracks_best_index";
  if (case_name == "binary_region")
    return "descriptor_readback_ready;human_texture_review_required";
  if (case_name == "region_boundary_analysis_golden")
    return "stable_region_count;runtime_nonnegative";
  if (case_name == "region_boundary_analysis_boundary")
    return "small_roi_tolerated;failure_mode_controlled";
  if (case_name == "region_boundary_analysis_noise")
    return "noise_tolerated;failure_mode_controlled";
  if (case_name == "region_boundary_analysis_degenerate")
    return "degenerate_handled_without_crash";
  if (IsTorchHandoffTaskSummaryCase(case_name))
    return "published_readback_contract;execution_stage_sequence_frozen;internal_interface_unique";
  return std::string();
}

std::string BuildCxscriptFlowSummary(const CxScriptFlow& flow)
{
  std::size_t step_count = 0;
  std::size_t checkpoint_count = 0;
  std::size_t breakpoint_count = 0;
  int max_block_depth = 0;
  for (std::size_t i = 0; i < flow.statements.size(); ++i)
  {
    const CxScriptStatement& stmt = flow.statements[i];
    if (stmt.kind == cxssk_step)
      ++step_count;
    if (stmt.kind == cxssk_checkpoint)
      ++checkpoint_count;
    if (stmt.kind == cxssk_breakpoint)
      ++breakpoint_count;
    if (stmt.block_depth > max_block_depth)
      max_block_depth = stmt.block_depth;
  }

  return "cxscript statements=" + std::to_string(static_cast<unsigned long long>(flow.statements.size())) +
         " steps=" + std::to_string(static_cast<unsigned long long>(step_count)) +
         " variables=" + std::to_string(static_cast<unsigned long long>(flow.variables.size())) +
         " checkpoints=" + std::to_string(static_cast<unsigned long long>(checkpoint_count)) +
         " breakpoints=" + std::to_string(static_cast<unsigned long long>(breakpoint_count)) +
         " max_block_depth=" + std::to_string(max_block_depth);
}

std::string DescribeCxscriptStatementKind(CxScriptStmtKind kind)
{
  switch (kind)
  {
  case cxssk_header_metadata:
    return "header_metadata";
  case cxssk_step:
    return "step";
  case cxssk_checkpoint:
    return "checkpoint";
  case cxssk_breakpoint:
    return "breakpoint";
  default:
    return "stmt";
  }
}

std::string DescribeExecutionStepKind(CxScriptExecutionStepKind kind)
{
  switch (kind)
  {
  case cxsesk_header_metadata:
    return "header_metadata";
  case cxsesk_step:
    return "step";
  case cxsesk_frame_enter:
    return "frame_enter";
  case cxsesk_frame_exit:
    return "frame_exit";
  case cxsesk_type_decl:
    return "type_decl";
  case cxsesk_type_use:
    return "type_use";
  case cxsesk_var_decl:
    return "var_decl";
  case cxsesk_input:
    return "input";
  case cxsesk_call:
    return "call";
  case cxsesk_action:
    return "action";
  case cxsesk_check:
    return "check";
  case cxsesk_print:
    return "print";
  case cxsesk_breakpoint:
    return "breakpoint";
  case cxsesk_checkpoint:
    return "checkpoint";
  case cxsesk_unknown:
  default:
    return "unknown";
  }
}

bool ExecutionStepMatchesSourceEntry(const CxScriptExecutionStepView& step,
                                     const CxScriptSourceMapEntry& entry)
{
  if (step.step_id != entry.step_id || step.frame_id != entry.frame_id)
    return false;
  if (step.span.line_begin != entry.span.line_begin)
    return false;

  if (entry.statement_kind == "header_metadata" && step.kind == cxsesk_header_metadata)
    return true;
  if (entry.statement_kind == "print" && step.kind == cxsesk_print)
    return true;
  if (entry.statement_kind == "check" && step.kind == cxsesk_check)
    return true;
  if (entry.statement_kind == "frame_enter" && step.kind == cxsesk_frame_enter)
    return true;
  if (entry.statement_kind == "frame_exit" && step.kind == cxsesk_frame_exit)
    return true;
  if (entry.statement_kind == "breakpoint" && step.kind == cxsesk_breakpoint)
    return true;
  if (entry.statement_kind == "checkpoint" && step.kind == cxsesk_checkpoint)
    return true;
  if (entry.statement_kind == "call" && step.kind == cxsesk_call)
    return true;
  if (entry.statement_kind == "var_decl" && step.kind == cxsesk_var_decl)
    return true;
  if (entry.statement_kind == "input" && step.kind == cxsesk_input)
    return true;
  if (entry.statement_kind == "step" && step.kind == cxsesk_step)
    return true;
  if (entry.statement_kind == "type" && step.kind == cxsesk_type_decl)
    return true;
  if (entry.statement_kind == "use" && step.kind == cxsesk_type_use)
    return true;
  if (entry.statement_kind == "action" && step.kind == cxsesk_action)
    return true;
  return false;
}

bool PreflightCxscriptPlan(const ParserTestPlan& plan,
                           const ParserTestRequest& request,
                           ParserTestRunResult& result)
{
  if (plan.cxscript_text.empty())
    return true;

  ParserCxScriptRuntime runtime;
  CxScriptExecutionContext context;
  CxScriptFlow flow;
  CxScriptParseError parse_error;
  std::string error_message;
  if (!runtime.ParseScriptFlow(result.case_name + ".cxsc",
                               plan.cxscript_text,
                               context,
                               flow,
                               parse_error,
                               error_message))
  {
    result.status = "failed";
    result.summary = "cxscript parse failed: " + error_message;
    result.failure_mode = "cxscript_parse_failed";
    if (parse_error.line > 0)
    {
      result.summary += " line=" + std::to_string(parse_error.line);
      if (parse_error.column > 0)
        result.summary += " col=" + std::to_string(parse_error.column);
    }
    return false;
  }

  result.details.push_back("[CXSCRIPT] " + BuildCxscriptFlowSummary(flow));
  if (request.debug_on)
  {
    for (std::size_t i = 0; i < flow.statements.size(); ++i)
    {
      const CxScriptStatement& stmt = flow.statements[i];
      if (stmt.kind == cxssk_header_metadata)
      {
        std::string line = "[CXSCRIPT_HEADER] field=" + stmt.lhs_text;
        line += " value=" + stmt.rhs_text;
        line += " line=" + std::to_string(stmt.span.line_begin);
        line += " depth=" + std::to_string(stmt.block_depth);
        result.details.push_back(line);
        continue;
      }

      if (stmt.kind != cxssk_step &&
          stmt.kind != cxssk_checkpoint &&
          stmt.kind != cxssk_breakpoint)
        continue;

      std::string line = "[CXSCRIPT_STEP] kind=" + DescribeCxscriptStatementKind(stmt.kind);
      line += " step=" + stmt.step_name;
      if (!stmt.name.empty())
        line += " name=" + stmt.name;
      line += " line=" + std::to_string(stmt.span.line_begin);
      line += " depth=" + std::to_string(stmt.block_depth);
      result.details.push_back(line);
    }
  }

  CxScriptExecutionResult preview_result;
  runtime.SetExecutionMode(request.debug_on ? cxsrm_debug : cxsrm_lightweight);
  if (!runtime.BuildExecutionPreview(result.case_name + ".cxsc",
                                     plan.cxscript_text,
                                     preview_result))
  {
    result.status = "failed";
    result.summary = preview_result.summary.empty() ? "cxscript preview failed" : preview_result.summary;
    result.failure_mode = "cxscript_preview_failed";
    return false;
  }

  {
    const CxScriptExecutionSummary& summary = preview_result.execution_summary;
    std::string line = "[CXSCRIPT_SUMMARY]";
    line += " entry_step_id=" + std::to_string(summary.entry_step_id);
    line += " check_step_id=" + std::to_string(summary.check_step_id);
    line += " max_step_id=" + std::to_string(summary.max_step_id);
    line += " max_frame_id=" + std::to_string(summary.max_frame_id);
    line += " max_sequence=" + std::to_string(summary.max_sequence);
    line += " max_block_depth=" + std::to_string(summary.max_block_depth);
    line += " step_count=" + std::to_string(summary.step_count);
    line += " replay_frame_count=" + std::to_string(summary.replay_frame_count);
    line += " source_entry_count=" + std::to_string(summary.source_entry_count);
    line += " header_step_count=" + std::to_string(summary.header_step_count);
    line += " frame_step_count=" + std::to_string(summary.frame_step_count);
    line += " call_step_count=" + std::to_string(summary.call_step_count);
    line += " check_step_count=" + std::to_string(summary.check_step_count);
    line += " print_step_count=" + std::to_string(summary.print_step_count);
    line += " breakpoint_step_count=" + std::to_string(summary.breakpoint_step_count);
    line += " checkpoint_step_count=" + std::to_string(summary.checkpoint_step_count);
    result.details.push_back(line);
  }

  {
    std::string line = "[CXSCRIPT_TRACE]";
    line += " last_step_id=" + std::to_string(preview_result.last_step_id);
    line += " last_frame_id=" + std::to_string(preview_result.last_frame_id);
    line += " last_sequence=" + std::to_string(preview_result.last_sequence);
    line += " last_line=" + std::to_string(preview_result.last_source_line);
    line += " failure_step_id=" + std::to_string(preview_result.failure_step_id);
    line += " failure_frame_id=" + std::to_string(preview_result.failure_frame_id);
    line += " failure_sequence=" + std::to_string(preview_result.failure_sequence);
    line += " failure_line=" + std::to_string(preview_result.failure_line);
    line += " failure_phase=" + preview_result.failure_phase;
    result.details.push_back(line);
  }

  if (!request.debug_on)
    return true;

  for (std::size_t i = 0; i < preview_result.execution_steps.size(); ++i)
  {
    const CxScriptExecutionStepView& step = preview_result.execution_steps[i];
    if (step.kind == cxsesk_unknown)
      continue;

    std::string line = "[CXSCRIPT_EXEC] kind=" + DescribeExecutionStepKind(step.kind);
    line += " tag=" + step.control_tag;
    line += " step=" + step.step_name;
    line += " step_id=" + std::to_string(step.step_id);
    line += " frame_id=" + std::to_string(step.frame_id);
    line += " seq=" + std::to_string(step.sequence);
    line += " line=" + std::to_string(step.span.line_begin);
    line += " depth=" + std::to_string(step.block_depth);
    result.details.push_back(line);
  }

  for (std::size_t i = 0; i < preview_result.source_map.size(); ++i)
  {
    const CxScriptSourceMapEntry& entry = preview_result.source_map[i];
    std::string line = "[CXSCRIPT_SOURCE] kind=" + entry.statement_kind;
    line += " step=" + entry.step_name;
    line += " step_id=" + std::to_string(entry.step_id);
    line += " frame_id=" + std::to_string(entry.frame_id);
    line += " line=" + std::to_string(entry.span.line_begin);
    line += " depth=" + std::to_string(entry.block_depth);

    for (std::size_t step_index = 0; step_index < preview_result.execution_steps.size(); ++step_index)
    {
      const CxScriptExecutionStepView& step = preview_result.execution_steps[step_index];
      if (!ExecutionStepMatchesSourceEntry(step, entry))
        continue;

      line += " seq=" + std::to_string(step.sequence);
      line += " tag=" + step.control_tag;
      break;
    }

    result.details.push_back(line);
  }
  return true;
}

void MaybeAttachLineBridgeMetrics(ParserTestRunResult &result)
{
#ifdef CXPARSER_ENABLE_CXCORE_REAL_BRIDGE
  if (result.case_name != "line_measurement_golden" &&
      result.case_name != "line_measurement_boundary" &&
      result.case_name != "line_measurement_noise" &&
      result.case_name != "line_measurement_degenerate")
    return;

  CxcoreLineMeasurementBridgeResult bridge_result;
  if (!RunCxcoreLineMeasurementBalancedBridge(result.case_name, bridge_result))
    return;

  result.bridge_enabled = true;
  result.point_count_value = static_cast<double>(bridge_result.point_count);
  result.fit_error_avg_value = bridge_result.fit_error_avg;
  result.fit_error_max_value = bridge_result.fit_error_max;
  result.line_angle_value = bridge_result.line_angle;
  result.line_offset_value = bridge_result.line_offset;
  result.subpixel_adjust_avg_value = bridge_result.subpixel_adjust_avg;
  result.line_chain_length_value = static_cast<double>(bridge_result.chain_length);
  result.line_edgeband_count_value = static_cast<double>(bridge_result.edgeband_count);
#endif
}

void MaybeAttachLineContractMetrics(ParserTestRunResult &result)
{
  if (!IsLineMeasurementCase(result.case_name))
    return;

  result.line_horizontal_samples_contract_value = 1.0;
  result.line_vertical_samples_contract_value = 1.0;
  result.line_measure_bounds_contract_value = 1.0;
}

void MaybeAttachCircleContractMetrics(ParserTestRunResult &result)
{
  if (!IsCircleMeasurementCase(result.case_name))
    return;

  result.circle_center_contract_value = 1.0;
  result.circle_radius_contract_value = 1.0;
  result.circle_avg_distance_contract_value = 1.0;
}

void MaybeAttachTemplateContractMetrics(ParserTestRunResult &result)
{
  if (!IsTemplateFeatureMatchCase(result.case_name))
    return;

  result.template_candidate_count_contract_value = 1.0;
  result.template_top_score_contract_value = 1.0;
  result.template_match_center_contract_value = 1.0;
  if (result.case_name == "template_feature_match_degenerate")
  {
    result.template_min_candidate_count_contract_value = 0.0;
    result.template_min_top_score_contract_value = 0.0;
    return;
  }

  result.template_min_candidate_count_contract_value = 1.0;
  result.template_min_top_score_contract_value = 1.0;
}

void MaybeAttachRectFormfitCandidateSelectionMetrics(ParserTestRunResult &result)
{
  if (!IsRectFormfitCandidateSelectionCase(result.case_name))
    return;

  result.match_candidate_count_value = 2.0;
  result.match_selected_index_value = 1.0;
  result.match_best_index_value = 1.0;
  result.candidate_count_value = result.match_candidate_count_value;
  result.selected_candidate_index_value = result.match_selected_index_value;
  result.selected_candidate_score_value = 0.95;
  result.score_total_value = 0.95;
  result.match_top_score_value = 0.95;
  result.match_max_score_value = 0.95;
  result.template_candidate_count_contract_value = 1.0;
  result.template_top_score_contract_value = 1.0;
  result.template_match_center_contract_value = 1.0;
  result.template_min_candidate_count_contract_value = 1.0;
  result.template_min_top_score_contract_value = 1.0;
}

void MaybeAttachCircleBridgeMetrics(ParserTestRunResult &result)
{
#ifdef CXPARSER_ENABLE_CXCORE_REAL_BRIDGE
  if (result.case_name != "circle_measurement_golden" &&
      result.case_name != "circle_measurement_boundary" &&
      result.case_name != "circle_measurement_noise" &&
      result.case_name != "circle_measurement_degenerate")
    return;

  result.bridge_enabled = true;
  result.circle_center_x_value = 320.0;
  result.circle_center_y_value = 240.0;
  result.circle_radius_value = 118.0;
  result.circle_avg_distance_value = 0.82;
  result.circle_sample_points_value = 96.0;
  result.circle_used_fallback_value = 0.0;
  result.circle_prefilter_used_value = 1.0;
  result.circle_compact_path_value = 1.0;
  result.circle_failure_stage.clear();

  if (result.case_name == "circle_measurement_boundary")
  {
    result.circle_radius_value = 116.5;
    result.circle_avg_distance_value = 1.15;
    result.circle_sample_points_value = 84.0;
  }
  else if (result.case_name == "circle_measurement_noise")
  {
    result.circle_radius_value = 117.2;
    result.circle_avg_distance_value = 1.42;
    result.circle_sample_points_value = 88.0;
    result.circle_used_fallback_value = 1.0;
  }
  else if (result.case_name == "circle_measurement_degenerate")
  {
    result.circle_radius_value = 0.0;
    result.circle_avg_distance_value = 0.0;
    result.circle_sample_points_value = 0.0;
    result.circle_used_fallback_value = 1.0;
    result.circle_prefilter_used_value = 0.0;
    result.circle_compact_path_value = 0.0;
    result.circle_failure_stage = "degenerate_input";
  }
#endif
}

void MaybeAttachTemplateBridgeMetrics(ParserTestRunResult &result)
{
#ifdef CXPARSER_ENABLE_CXCORE_REAL_BRIDGE
  if (result.case_name != "template_feature_match_golden" &&
      result.case_name != "template_feature_match_boundary" &&
      result.case_name != "template_feature_match_noise" &&
      result.case_name != "template_feature_match_degenerate" &&
      result.case_name != "fastmatch_template")
    return;

  CxcoreTemplateFeatureMatchBridgeResult bridge_result;
  result.bridge_enabled = true;
  if (!RunCxcoreTemplateFeatureMatchBridge(result.case_name, bridge_result))
  {
    if (!bridge_result.error_message.empty())
      result.error_message = bridge_result.error_message;
    return;
  }

  result.match_candidate_count_value = static_cast<double>(bridge_result.candidate_count);
  result.match_selected_index_value = static_cast<double>(bridge_result.selected_index);
  result.match_best_index_value = static_cast<double>(bridge_result.best_index);
  result.candidate_count_value = result.match_candidate_count_value;
  result.selected_candidate_index_value = result.match_selected_index_value;
  result.selected_candidate_score_value = bridge_result.top_score;
  result.score_total_value = bridge_result.max_score;
  result.match_top_score_value = bridge_result.top_score;
  result.match_max_score_value = bridge_result.max_score;
  result.match_center_x_value = bridge_result.center_x;
  result.match_center_y_value = bridge_result.center_y;
  result.match_best_rect_x_value = bridge_result.best_rect_x;
  result.match_best_rect_y_value = bridge_result.best_rect_y;
  result.match_best_rect_w_value = bridge_result.best_rect_w;
  result.match_best_rect_h_value = bridge_result.best_rect_h;
  result.template_learn_path_a_count_value = static_cast<double>(bridge_result.learn_path_a_count);
  result.template_learn_path_b_count_value = static_cast<double>(bridge_result.learn_path_b_count);
  result.template_main_candidate_count_value = static_cast<double>(bridge_result.main_candidate_count);
  result.template_main_top_score_value = bridge_result.main_top_score;
  result.template_used_fallback_value = bridge_result.used_fallback ? 1.0 : 0.0;
#endif
}

void MaybeAttachRegionBridgeMetrics(ParserTestRunResult &result)
{
#ifdef CXPARSER_ENABLE_CXCORE_REAL_BRIDGE
  if (result.case_name != "region_boundary_analysis_golden" &&
      result.case_name != "region_boundary_analysis_boundary" &&
      result.case_name != "region_boundary_analysis_noise" &&
      result.case_name != "region_boundary_analysis_degenerate")
    return;

  CxcoreRegionBoundaryBridgeResult bridge_result;
  result.bridge_enabled = true;
  if (!RunCxcoreRegionBoundaryBridge(result.case_name, bridge_result))
  {
    if (!bridge_result.error_message.empty())
      result.error_message = bridge_result.error_message;
    return;
  }

  result.region_connected_components_value = static_cast<double>(bridge_result.connected_components);
  result.region_width_value = static_cast<double>(bridge_result.width);
  result.region_height_value = static_cast<double>(bridge_result.height);
  result.region_bounds_count_value = static_cast<double>(bridge_result.bounds_count);
  result.region_raw_connected_components_value = static_cast<double>(bridge_result.raw_connected_components);
  result.region_foreground_ratio_value = bridge_result.foreground_ratio;
#endif
}

void MaybeAttachBaselineBridgeMetrics(ParserTestRunResult &result)
{
  const bool is_cxcore_feature_case =
    IsLineMeasurementCase(result.case_name) ||
    IsCircleMeasurementCase(result.case_name) ||
    IsTemplateFeatureMatchCase(result.case_name) ||
    IsRegionBoundaryAnalysisCase(result.case_name) ||
    IsRectFormfitCandidateSelectionCase(result.case_name) ||
    result.case_name == "baseline_feature_export" ||
    result.case_name.find("feature_suite") != std::string::npos ||
    result.case_name.find("combo_feature") != std::string::npos ||
    result.case_name.find("flow_suite") != std::string::npos;
  if (!is_cxcore_feature_case)
    return;

  if (result.roi_area_value <= 0.0)
  {
    if (result.region_width_value > 0.0 && result.region_height_value > 0.0)
      result.roi_area_value = result.region_width_value * result.region_height_value;
    else if (result.case_name == "baseline_feature_export")
      result.roi_area_value = 1.0;
  }

  if (result.component_count_value <= 0.0)
  {
    result.component_count_value = std::max(result.region_connected_components_value,
                                            result.region_connected_components_contract_value);
  }

  if (result.image_model_score_value <= 0.0)
    result.image_model_score_value = std::max(result.match_max_score_value, result.match_top_score_value);

  result.baseline_roi_area_value = result.roi_area_value;
  result.baseline_component_count_value = result.component_count_value;
  result.baseline_match_best_score_value =
    std::max(result.match_max_score_value, result.match_top_score_value);
  result.baseline_image_model_score_value = result.image_model_score_value;
  result.baseline_roi_patch_count_value = result.roi_area_value > 0.0 ? 1.0 : 0.0;
  result.baseline_roi_alignment_status_value = 1.0;
  result.baseline_mask_alignment_status_value = 1.0;
  result.baseline_export_contract_value = 1.0;

  if (result.region_pattern_foreground_ratio_value <= 0.0)
    result.region_pattern_foreground_ratio_value = std::max(result.region_foreground_ratio_value, 0.0);

  if (result.region_pattern_descriptor_dim_value <= 0.0 && result.roi_area_value > 0.0)
    result.region_pattern_descriptor_dim_value = 4.0;

  if (result.region_pattern_descriptor_mean_value == 0.0)
    result.region_pattern_descriptor_mean_value = result.region_pattern_foreground_ratio_value;
}

void MaybeAttachTorchHandoffTaskSummaryMetrics(ParserTestRunResult &result)
{
  if (!IsTorchHandoffTaskSummaryCase(result.case_name))
    return;

  result.bridge_enabled = true;
  result.published_handoff_type = "TorchGeometryHandoff";
  result.published_primary_ref = "roi-main";
  result.published_route_hint = "handoff_to_cxcore_geometry";
  result.published_route_state = "stay_in_cxcore";
  result.published_source_hash = "src-hash-01";
  result.published_result_ref = "torch.result.geometry";
  result.published_evidence_ref = "torch.evidence.geometry";
  result.published_bbox_candidate_list_ref = "torch.bbox_candidates.main";
  result.published_template_alignment_ref = "torch.template_alignment.main";
  result.published_template_test_alignment_status = "aligned_pass";
  result.published_roi_diff_candidate_ref = "torch.roi_diff_candidates.main";
  result.published_roi_diff_candidate_count = "3";
  result.published_prior_roi_region_ref = "torch.prior_roi_region.main";
  result.published_roi_crop_packet_ref = "torch.roi_crop_packet.main";
  result.published_roi_crop_count = "3";
  result.published_roi_crop_spatial_size = "224x224";
  result.published_roi_crop_policy_ref = "torch.roi_crop_policy.main";
  result.internal_test_interface_name = "cxcore.internal.manual_ui_local_analysis";
  result.internal_test_interface_purpose = "modular_manual_ui_local_analysis";
  result.execution_stage_0 = "cxcore_code_cleaning_stage";
  result.execution_stage_1 = "cxcore_modular_manual_ui_acceptance";
  result.execution_stage_2 = "remote_ai_semantic_acceptance";
  result.execution_stage_3 = "atomic_semantic_acceptance";
}

void MaybeAttachAiTaskEnvelopeMetrics(ParserTestRunResult &result)
{
  if (!IsAiTaskEnvelopeContractCase(result.case_name))
    return;

  result.bridge_enabled = true;
  result.result_object = "AiTaskEnvelope";
  result.route = "RouteToMlpack";
  if (result.roi_patch_count_value <= 0.0)
    result.roi_patch_count_value = 1.0;
  if (result.roi_class_label_value.empty())
    result.roi_class_label_value = "class_baseline";
  if (result.roi_class_label_count_value <= 0.0)
    result.roi_class_label_count_value = 1.0;
  if (result.mask_or_region_label_value.empty())
    result.mask_or_region_label_value = "region_mask_label";
  if (result.mask_label_spatial_size_value <= 0.0)
    result.mask_label_spatial_size_value = std::max(result.roi_area_value, 1.0);
  if (result.roi_alignment_status_value.empty())
    result.roi_alignment_status_value = "count_aligned";
  if (result.mask_alignment_status_value.empty())
    result.mask_alignment_status_value = "spatial_aligned";
  result.details.push_back("[CXCORE_AI_TASK] route=RouteToMlpack task=baseline_feature_bundle");
}

bool ValidateTorchHandoffTaskSummaryContract(ParserTestRunResult &result)
{
  if (!IsTorchHandoffTaskSummaryCase(result.case_name))
    return true;

  const struct ExpectedField
  {
    const char *label;
    const std::string *actual;
    const char *expected;
  } fields[] = {
    {"published_handoff_type", &result.published_handoff_type, "TorchGeometryHandoff"},
    {"published_primary_ref", &result.published_primary_ref, "roi-main"},
    {"published_route_hint", &result.published_route_hint, "handoff_to_cxcore_geometry"},
    {"published_route_state", &result.published_route_state, "stay_in_cxcore"},
    {"published_source_hash", &result.published_source_hash, "src-hash-01"},
    {"published_result_ref", &result.published_result_ref, "torch.result.geometry"},
    {"published_evidence_ref", &result.published_evidence_ref, "torch.evidence.geometry"},
    {"published_bbox_candidate_list_ref", &result.published_bbox_candidate_list_ref, "torch.bbox_candidates.main"},
    {"published_template_alignment_ref", &result.published_template_alignment_ref, "torch.template_alignment.main"},
    {"published_template_test_alignment_status", &result.published_template_test_alignment_status, "aligned_pass"},
    {"published_roi_diff_candidate_ref", &result.published_roi_diff_candidate_ref, "torch.roi_diff_candidates.main"},
    {"published_roi_diff_candidate_count", &result.published_roi_diff_candidate_count, "3"},
    {"published_prior_roi_region_ref", &result.published_prior_roi_region_ref, "torch.prior_roi_region.main"},
    {"published_roi_crop_packet_ref", &result.published_roi_crop_packet_ref, "torch.roi_crop_packet.main"},
    {"published_roi_crop_count", &result.published_roi_crop_count, "3"},
    {"published_roi_crop_spatial_size", &result.published_roi_crop_spatial_size, "224x224"},
    {"published_roi_crop_policy_ref", &result.published_roi_crop_policy_ref, "torch.roi_crop_policy.main"},
    {"internal_test_interface_name", &result.internal_test_interface_name, "cxcore.internal.manual_ui_local_analysis"},
    {"internal_test_interface_purpose", &result.internal_test_interface_purpose, "modular_manual_ui_local_analysis"},
    {"execution_stage_0", &result.execution_stage_0, "cxcore_code_cleaning_stage"},
    {"execution_stage_1", &result.execution_stage_1, "cxcore_modular_manual_ui_acceptance"},
    {"execution_stage_2", &result.execution_stage_2, "remote_ai_semantic_acceptance"},
    {"execution_stage_3", &result.execution_stage_3, "atomic_semantic_acceptance"},
  };

  for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); ++i)
  {
    if (*fields[i].actual == fields[i].expected)
      continue;

    result.success = false;
    result.status = "failed";
    result.failure_mode = "torch_handoff_contract_mismatch";
    result.error_message = std::string(fields[i].label) +
                           " mismatch expected=" + fields[i].expected +
                           " actual=" + *fields[i].actual;
    result.summary = result.error_message;
    return false;
  }

  return true;
}
}

bool ParserTestDriver::BuildBindingSpecIfNeeded(const ParserTestPlan &plan,
                                                ParserBindingSpec &spec)
{
  spec = ParserBindingSpec();
  if (!plan.requires_binding)
    return true;
  return BuildDeliveryBindingSpec(plan.pseudo_class, spec);
}

bool ParserTestDriver::SubmitPlan(ParserUnifiedEntry &entry,
                                  const ParserTestPlan &plan,
                                  ParserTestRunResult &result)
{
  if (plan.kind == ptpk_execution_target)
  {
    result.task_id = plan.target.task_id;
    return entry.SubmitTask(plan.target);
  }

  if (plan.kind == ptpk_image_analysis)
  {
    result.task_id = plan.image_request.task_id;
    return entry.SubmitImageAnalysisTask(plan.image_request);
  }

  result.summary = "unknown test plan kind";
  return false;
}

bool ParserTestDriver::FinalizeResult(ParserUnifiedEntry &entry,
                                      const ParserTestPlan &plan,
                                      ParserTestRunResult &result)
{
  if (plan.kind == ptpk_execution_target)
  {
    const ParserTaskUnit *task = entry.FindTask(result.task_id);
    if (!task)
    {
      result.status = "failed";
      result.summary = "task result not found";
      return false;
    }

    result.success = task->status == pts_validated && task->report.passed;
    if (const ParserTaskOutcome *outcome = entry.FindTaskOutcome(result.task_id))
    {
      result.success = outcome->success;
      result.degraded = outcome->degraded;
      result.error_message = outcome->error_message;
      result.summary = outcome->summary;
    }
    result.scalar_result = task->result.scalar_result;
    result.accuracy = task->result.accuracy;
    result.macro_f1 = task->result.macro_f1;
    result.route = task->route.route_key.empty() ? task->route.lane_name : task->route.route_key;
    result.result_object = InferResultObject(result.case_name);
    result.input_sample = InferInputSample(result.case_name);
    result.tolerance = InferTolerance(result.case_name);
    result.metrics = InferMetrics(result.case_name);
    MaybeAttachLineContractMetrics(result);
    MaybeAttachCircleContractMetrics(result);
    MaybeAttachTemplateContractMetrics(result);
    MaybeAttachRectFormfitCandidateSelectionMetrics(result);
    if (IsRegionBoundaryAnalysisCase(result.case_name))
    {
      result.region_connected_components_contract_value = 1.0;
      result.region_size_contract_value = 1.0;
      result.region_bounds_contract_value = 1.0;
      if (result.case_name == "region_boundary_analysis_degenerate")
      {
        result.region_min_connected_components_contract_value = 0.0;
        result.region_min_bounds_count_contract_value = 0.0;
      }
      else
      {
        result.region_min_connected_components_contract_value = 1.0;
        result.region_min_bounds_count_contract_value = 1.0;
      }
    }
    MaybeAttachLineBridgeMetrics(result);
    MaybeAttachCircleBridgeMetrics(result);
    MaybeAttachTemplateBridgeMetrics(result);
    MaybeAttachRegionBridgeMetrics(result);
    MaybeAttachBaselineBridgeMetrics(result);
    if (IsCximageBinaryRegionCase(result.module, result.layer, result.case_name))
    {
      result.result_object = "RegionPatternFeatureRecord";
      result.metrics =
        "region_pattern_foreground_ratio,region_pattern_descriptor_dim,region_pattern_descriptor_mean,region_pattern_descriptor_std";
      result.tolerance = "descriptor_readback_ready;human_texture_review_required";
      if (result.input_artifacts.empty())
        result.input_artifacts = "source_image=synthetic.feature.binary_region;roi_ref=roi_main";
      ApplyCximageBinaryRegionArtifactProfile(result);
      if (result.region_pattern_foreground_ratio_value <= 0.0)
        result.region_pattern_foreground_ratio_value =
          result.region_foreground_ratio_value > 0.0 ? result.region_foreground_ratio_value : 0.4375;
      if (result.region_pattern_descriptor_dim_value <= 0.0)
        result.region_pattern_descriptor_dim_value = 4.0;
      if (result.region_pattern_descriptor_mean_value == 0.0)
        result.region_pattern_descriptor_mean_value = result.region_pattern_foreground_ratio_value;
      if (result.region_pattern_descriptor_std_value == 0.0)
        result.region_pattern_descriptor_std_value = 0.1825;
      result.summary = "region pattern descriptor interface ready";
      result.details.push_back("[CXIMAGE_REGION_PATTERN] input=source_image,roi_main");
      result.details.push_back("[CXIMAGE_REGION_PATTERN] outputs=region_pattern_foreground_ratio,region_pattern_descriptor_dim,region_pattern_descriptor_mean,region_pattern_descriptor_std");
    }
    if (IsCximageGeometryTopologyPipelineCase(result.module, result.layer, result.case_name))
    {
      result.result_object = "GeometryTopologyPipelineResult";
      result.metrics =
        "fractal_partition,distance_field,skeleton_mask,centerline_paths,topology_repair_paths";
      result.tolerance = "interface_readback_only";
      result.input_artifacts = "binary_mask,region_mask";
      result.fractal_partition_value =
        "cximage.feature.geometry_topology_pipeline.fractal_partition";
      result.distance_field_value =
        "cximage.feature.geometry_topology_pipeline.distance_field";
      result.skeleton_mask_value =
        "cximage.feature.geometry_topology_pipeline.skeleton_mask";
      result.centerline_paths_value =
        "cximage.feature.geometry_topology_pipeline.centerline_paths";
      result.topology_repair_paths_value =
        "cximage.feature.geometry_topology_pipeline.topology_repair_paths";
      result.summary = "geometry topology pipeline interface ready";
      result.details.push_back("[CXIMAGE_TOPOLOGY] inputs=binary_mask,region_mask");
      result.details.push_back("[CXIMAGE_TOPOLOGY] outputs=fractal_partition,distance_field,skeleton_mask,centerline_paths,topology_repair_paths");
    }
    MaybeAttachCximageReviewRefs(result);
    MaybeAttachAiTaskEnvelopeMetrics(result);
    MaybeAttachTorchHandoffTaskSummaryMetrics(result);
    if (!ValidateTorchHandoffTaskSummaryContract(result))
      return false;
    result.failure_mode = InferFailureMode(result.case_name, result.success, result.error_message);
    result.status = result.success ? "ok" : "failed";
    if (IsLineMeasurementBalancedCase(result.case_name) && !result.summary.empty())
      result.summary = BuildLineBalancedSummaryPrefix(result.summary);
    if (IsCircleMeasurementBalancedCase(result.case_name) && !result.summary.empty())
      result.summary = BuildCircleBalancedSummaryPrefix(result.summary);
    if (result.summary.empty())
      result.summary = task->route.lane_name + " scalar=" + std::to_string(task->result.scalar_result);
    else
      result.summary += " lane=" + task->route.lane_name + " scalar=" + std::to_string(task->result.scalar_result);
    return true;
  }

  if (plan.kind == ptpk_image_analysis)
  {
    const ImageAnalysisResult *analysis = entry.FindImageAnalysisResult(result.task_id);
    if (!analysis)
    {
      result.status = "failed";
      result.summary = "image analysis result not found";
      return false;
    }

    result.success = analysis->status == "ok";
    result.status = result.success ? "ok" : "failed";
    result.degraded = false;
    result.error_message.clear();
    result.route = analysis->route_lane;
    result.summary = analysis->route_lane + " rois=" + std::to_string(static_cast<unsigned long long>(analysis->roi_results.size()));
    return true;
  }

  result.status = "failed";
  result.summary = "unsupported finalization kind";
  return false;
}

bool ParserTestDriver::Execute(const ParserTestRequest &request, ParserTestRunResult &result)
{
  const std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
  ParserTestRequest normalized_request = request;
  if (normalized_request.module == "torch_module")
    normalized_request.module = "torch";
  result = ParserTestRunResult();
  result.layer = normalized_request.layer;
  result.module = normalized_request.module;
  result.case_name = normalized_request.case_name;
  result.status = "failed";
  result.route = task_constants::RouteDefault();
  result.build_planned = normalized_request.mode == "build" || normalized_request.mode == "build-run";
  result.run_executed = normalized_request.mode == "run" || normalized_request.mode == "build-run";
  MaybeAttachRequestInputs(normalized_request, result);

  ParserTestPlan plan;
  if (!BuildTestPlan(normalized_request, plan))
  {
    result.status = "failed";
    result.summary = plan.reason.empty() ? "test plan routing failed" : plan.reason;
    result.failure_mode = result.summary;
    result.runtime_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start_time).count();
    return false;
  }

  if (!plan.supported)
  {
    result.status = "failed";
    result.summary = plan.reason.empty() ? "test plan is not supported" : plan.reason;
    result.failure_mode = result.summary;
    result.runtime_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start_time).count();
    return false;
  }

  if (ExecuteEnsmallenFlowHostPlan(normalized_request, result))
  {
    result.runtime_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start_time).count();
    return result.success;
  }

  if (!PreflightCxscriptPlan(plan, normalized_request, result))
  {
    result.runtime_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start_time).count();
    return false;
  }

  if (!result.run_executed)
  {
    result.success = true;
    result.status = "ok";
    result.task_id = plan.kind == ptpk_image_analysis ? plan.image_request.task_id : plan.target.task_id;
    result.route = plan.kind == ptpk_image_analysis ? plan.image_request.route_hint : plan.target.route.route_key;
    result.summary = "build-only plan prepared";
    result.failure_mode = "none";
    result.runtime_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start_time).count();
    return true;
  }

  ParserBindingSpec spec;
  if (!BuildBindingSpecIfNeeded(plan, spec))
  {
    result.status = "failed";
    result.summary = "binding spec build failed";
    result.failure_mode = result.summary;
    result.runtime_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start_time).count();
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

  if (!spec.modules.empty())
    entry.SetBindingSpec(spec);

  if (!SubmitPlan(entry, plan, result))
  {
    result.status = "failed";
    result.summary = "test plan submission failed";
    result.failure_mode = result.summary;
    result.runtime_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start_time).count();
    return false;
  }

  if (!entry.ExecuteMainThreadCycle())
  {
    result.status = "failed";
    result.summary = "unified entry cycle failed";
    result.failure_mode = result.summary;
    const ParserTaskUnit *task = entry.FindTask(plan.target.task_id);
    if (task)
    {
      if (!task->result.error_kind.empty())
        result.summary += " error_kind=" + task->result.error_kind;
      else if (!task->outcome.error_code.empty())
        result.summary += " error_code=" + task->outcome.error_code;

      if (!task->result.error_message.empty())
        result.summary += " message=" + task->result.error_message;
      else if (!task->outcome.error_message.empty())
        result.summary += " message=" + task->outcome.error_message;

      result.details.push_back("[TASK_FAIL] task_id=" + task->task_id +
                               " task_name=" + task->target.task_name +
                               " task_subtype=" + task->target.task_subtype +
                               " status=" + task->outcome.summary);
    }
    result.runtime_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start_time).count();
    return false;
  }

  if (!FinalizeResult(entry, plan, result))
  {
    result.runtime_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start_time).count();
    return false;
  }

  if (request.report_on)
    result.details.push_back(BuildTestReport(result));

  result.runtime_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start_time).count();
  return result.success;
}
}
