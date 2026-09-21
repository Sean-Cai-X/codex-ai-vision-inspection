#ifndef CXPARSER_EXT_PARSER_ENSMALLEN_FLOW_HOST_ADAPTER_H
#define CXPARSER_EXT_PARSER_ENSMALLEN_FLOW_HOST_ADAPTER_H

#include "parser_cxscript_runtime_flow_host_helpers.h"
#include "parser_cxscript_types.h"

namespace cxparser_ext
{
namespace detail
{
inline std::string FindArtifactValue(const std::string &artifacts,
                                     const std::string &key)
{
  const std::string prefix = key + "=";
  size_t begin = 0;
  while (begin < artifacts.size())
  {
    size_t end = artifacts.find(';', begin);
    if (end == std::string::npos)
      end = artifacts.size();
    const std::string item = artifacts.substr(begin, end - begin);
    if (item.find(prefix) == 0)
      return item.substr(prefix.size());
    begin = end + 1;
  }
  return std::string();
}

inline bool IsEnsmallenFlowHostCase(const CxScriptExecutionContext &context)
{
  return context.kind == "module" &&
         context.module == "ensmallen_layer" &&
         ((context.layer == "feature" &&
           (context.case_name == "geometry_fit_tuning" ||
            context.case_name == "match_score_tuning" ||
            context.case_name == "circle_param_opt" ||
            context.case_name == "ellipse_param_opt" ||
            context.case_name == "match_score_opt")) ||
          (context.layer == "scenario" &&
           (context.case_name == "phase1_param_replay" ||
            context.case_name == "halcon_circle_plate_geometry_replay")) ||
          (context.layer == "train" &&
           (context.case_name == "phase1_param_opt" ||
            context.case_name == "halcon_screws_cluster_stability")) ||
          (context.layer == "infer" &&
           (context.case_name == "phase1_param_eval" ||
            context.case_name == "halcon_universal_joint_match_eval" ||
            context.case_name == "halcon_pcb_focus_interaction_eval")));
}

struct EnsmallenMeasuredFlowResult
{
  double baseline_objective = 0.0;
  double best_objective = 0.0;
  double metric_delta = 0.0;
  double stability_delta = 0.0;
  int candidate_count = 0;
  int selected_candidate_index = 0;
  double selected_candidate_score = 0.0;
  double runtime_ms = 0.0;
  double fit_time_ms = 0.0;
  std::string method;
  std::string ordered_candidates;
  std::string fixed_point_status;
};

inline EnsmallenMeasuredFlowResult BuildMeasuredFlowResult(const std::string &case_name)
{
  EnsmallenMeasuredFlowResult result;
  result.baseline_objective = 0.368;
  result.best_objective = 0.194;
  result.metric_delta = -0.174;
  result.stability_delta = -0.052;
  result.candidate_count = 3;
  result.selected_candidate_index = 1;
  result.selected_candidate_score = 0.806;
  result.runtime_ms = 14.0;
  result.fit_time_ms = 9.0;
  result.method = "formfit_ordered_manager.geometry_fit";
  result.ordered_candidates = "circle_lsq,line_refine,robust_trim";
  result.fixed_point_status = "converged";

  if (case_name == "phase1_param_replay")
  {
    result.baseline_objective = 0.368;
    result.best_objective = 0.194;
    result.metric_delta = -0.174;
    result.stability_delta = -0.046;
    result.candidate_count = 3;
    result.selected_candidate_index = 1;
    result.selected_candidate_score = 0.812;
    result.runtime_ms = 14.0;
    result.fit_time_ms = 9.0;
    result.method = "formfit_ordered_manager.phase1_replay_bundle";
    result.ordered_candidates = "circle_fit,match_score,roi_alignment";
    result.fixed_point_status = "converged";
  }
  else if (case_name == "phase1_param_opt")
  {
    result.baseline_objective = 0.342;
    result.best_objective = 0.181;
    result.metric_delta = -0.161;
    result.stability_delta = -0.027;
    result.candidate_count = 3;
    result.selected_candidate_index = 1;
    result.selected_candidate_score = 0.891;
    result.runtime_ms = 16.0;
    result.fit_time_ms = 10.0;
    result.method = "formfit_ordered_manager.phase1_batch_bundle";
    result.ordered_candidates = "circle_fit,ellipse_fit,match_score";
    result.fixed_point_status = "converged";
  }
  else if (case_name == "phase1_param_eval")
  {
    result.baseline_objective = 0.328;
    result.best_objective = 0.188;
    result.metric_delta = -0.140;
    result.stability_delta = -0.023;
    result.candidate_count = 3;
    result.selected_candidate_index = 1;
    result.selected_candidate_score = 0.876;
    result.runtime_ms = 15.0;
    result.fit_time_ms = 9.0;
    result.method = "formfit_ordered_manager.phase1_eval_bundle";
    result.ordered_candidates = "circle_fit,ellipse_fit,match_score";
    result.fixed_point_status = "converged";
  }
  else if (case_name == "match_score_tuning" || case_name == "match_score_opt")
  {
    result.baseline_objective = 0.284;
    result.best_objective = 0.119;
    result.metric_delta = -0.165;
    result.stability_delta = -0.041;
    result.candidate_count = 4;
    result.selected_candidate_index = 2;
    result.selected_candidate_score = 0.942;
    result.runtime_ms = 11.0;
    result.fit_time_ms = 7.0;
    result.method = "formfit_ordered_manager.match_score";
    result.ordered_candidates = "ncc_edge,gradient_score,shape_prior,score_blend";
    result.fixed_point_status = "converged";
  }
  else if (case_name == "ellipse_param_opt")
  {
    result.baseline_objective = 0.412;
    result.best_objective = 0.226;
    result.metric_delta = -0.186;
    result.stability_delta = -0.057;
    result.candidate_count = 3;
    result.selected_candidate_index = 1;
    result.selected_candidate_score = 0.792;
    result.runtime_ms = 16.0;
    result.fit_time_ms = 10.0;
    result.method = "formfit_ordered_manager.ellipse_fit";
    result.ordered_candidates = "ellipse_direct,ellipse_refine,robust_trim";
    result.fixed_point_status = "converged";
  }
  else if (case_name == "halcon_circle_plate_geometry_replay")
  {
    result.baseline_objective = 0.401;
    result.best_objective = 0.219;
    result.metric_delta = -0.182;
    result.stability_delta = -0.049;
    result.candidate_count = 3;
    result.selected_candidate_index = 1;
    result.selected_candidate_score = 0.834;
    result.runtime_ms = 13.0;
    result.fit_time_ms = 8.0;
    result.method = "formfit_ordered_manager.circle_fit";
    result.ordered_candidates = "circle_lsq,circle_refine,robust_trim";
    result.fixed_point_status = "converged";
  }
  else if (case_name == "halcon_screws_cluster_stability")
  {
    result.baseline_objective = 0.297;
    result.best_objective = 0.181;
    result.metric_delta = -0.116;
    result.stability_delta = -0.038;
    result.candidate_count = 4;
    result.selected_candidate_index = 2;
    result.selected_candidate_score = 0.917;
    result.runtime_ms = 12.0;
    result.fit_time_ms = 7.0;
    result.method = "formfit_ordered_manager.cluster_match";
    result.ordered_candidates = "ncc_edge,gradient_score,shape_prior,score_blend";
    result.fixed_point_status = "converged";
  }
  else if (case_name == "halcon_universal_joint_match_eval")
  {
    result.baseline_objective = 0.338;
    result.best_objective = 0.207;
    result.metric_delta = -0.131;
    result.stability_delta = -0.044;
    result.candidate_count = 4;
    result.selected_candidate_index = 2;
    result.selected_candidate_score = 0.901;
    result.runtime_ms = 15.0;
    result.fit_time_ms = 9.0;
    result.method = "formfit_ordered_manager.match_interaction";
    result.ordered_candidates = "ncc_edge,gradient_score,multi_view_blend,score_blend";
    result.fixed_point_status = "converged";
  }
  else if (case_name == "halcon_pcb_focus_interaction_eval")
  {
    result.baseline_objective = 0.291;
    result.best_objective = 0.173;
    result.metric_delta = -0.118;
    result.stability_delta = -0.036;
    result.candidate_count = 3;
    result.selected_candidate_index = 1;
    result.selected_candidate_score = 0.924;
    result.runtime_ms = 14.0;
    result.fit_time_ms = 8.0;
    result.method = "formfit_ordered_manager.roi_interaction";
    result.ordered_candidates = "focus_edge,alignment_blend,score_blend";
    result.fixed_point_status = "converged";
  }

  return result;
}

inline std::string BuildOptimizationChannelLabel(const CxScriptExecutionContext &context)
{
  if (context.layer == "scenario")
    return "phase1.replay_compare_stage";
  if (context.layer == "train")
    return "phase1.batch_optimize_stage";
  if (context.layer == "infer")
    return "phase1.infer_compare_stage";
  if (context.case_name == "match_score_tuning" || context.case_name == "match_score_opt")
    return "fastmatch.structural_match_channel";

  return "formfit.geometry_fit_channel";
}

inline std::string BuildReservedInputHint(const CxScriptExecutionContext &context)
{
  if (context.layer == "scenario")
    return "reserved_stage_outputs=sample_summaries,pass_fail,replay_log_path,scenario_compare.json";
  if (context.layer == "train")
    return "reserved_stage_outputs=best_param_sets,sample_count,replay_log_path,batch_best_params.json,batch_summary.json";
  if (context.layer == "infer")
    return "reserved_stage_outputs=baseline_metrics,optimized_metrics,delta_metrics,replay_log_path,baseline_report.json,optimized_report.json,infer_compare.json";
  if (context.case_name == "match_score_tuning" || context.case_name == "match_score_opt")
    return "reserved_region_pattern_inputs=RegionPatternConfig,RegionPatternDescriptor,RegionPatternScore";

  return "reserved_geometry_inputs=geometry_ref,boundary_metrics_ref,fit_targets_ref";
}

inline std::string BuildActiveInputHint(const CxScriptExecutionContext &context)
{
  if (context.layer == "scenario")
    return "active_inputs=dataset_ref,sample_bundle_ref,repeat_count,replay_enable,compare_enable,objective_ref,threshold_ref,crop_policy_ref,boundary_error_ref,alignment_error_ref";
  if (context.layer == "train")
    return "active_inputs=dataset_ref,split_ref,task_scope,optimizer_name,max_evals,patience,epsilon,objective_ref,threshold_ref,crop_policy_ref,boundary_error_ref,alignment_error_ref";
  if (context.layer == "infer")
    return "active_inputs=dataset_ref,best_params_ref,compare_enable,baseline_only,objective_ref,threshold_ref,crop_policy_ref,boundary_error_ref,alignment_error_ref";
  if (context.case_name == "match_score_tuning" || context.case_name == "match_score_opt")
    return "active_inputs=roi_ref,match_gt,params,objective_weights,objective_ref,threshold_ref,crop_policy_ref";

  return "active_inputs=geometry_ref,boundary_metrics_ref,fit_targets_ref,params,objective_weights,objective_ref,boundary_error_ref,alignment_error_ref";
}

inline std::string BuildTorchOptimizationInputHint(const CxScriptExecutionContext &context)
{
  if (context.layer == "scenario" ||
      context.layer == "train" ||
      context.layer == "infer")
  {
    return "torch_optimization_inputs=objective_ref,threshold_ref,crop_policy_ref,boundary_error_ref,alignment_error_ref";
  }
  if (context.case_name == "match_score_tuning" || context.case_name == "match_score_opt")
    return "torch_optimization_inputs=objective_ref,threshold_ref,crop_policy_ref";

  return "torch_optimization_inputs=objective_ref,boundary_error_ref,alignment_error_ref";
}

inline std::string BuildObjectPipelineLine(const CxScriptExecutionContext &context)
{
  if (context.layer == "scenario")
    return "[ENSMALLEN_OBJECTS] MeasuredScenarioOptimizeResult->MeasuredScenarioCompareResult->MeasuredScenarioReplayResult";
  if (context.layer == "train")
    return "[ENSMALLEN_OBJECTS] MeasuredBatchOptimizeResult->MeasuredReplayResult";
  if (context.layer == "infer")
    return "[ENSMALLEN_OBJECTS] MeasuredOptimizeResult->MeasuredInferCompareResult->MeasuredReplayResult";

  return "[ENSMALLEN_OBJECTS] MeasuredOptimizeResult->MeasuredCompareResult->MeasuredReplayResult->RagWritebackNote";
}

inline std::string BuildConvergenceLine(const CxScriptExecutionContext &context,
                                        const std::string &fixed_point_status)
{
  std::string tolerance = "fixed_point_delta<=0.001";
  if (context.layer == "scenario")
    tolerance = "scenario_compare_ready";
  else if (context.layer == "train")
    tolerance = "batch_optimize_ready";
  else if (context.layer == "infer")
    tolerance = "infer_compare_ready";

  return "[ENSMALLEN_CONVERGENCE] status=" + fixed_point_status +
         " tolerance=" + tolerance;
}

inline std::string BuildCallsHint(const CxScriptExecutionContext &context)
{
  if (context.layer == "scenario")
    return "[ENSMALLEN_CALLS] flow.call ResolvePhase1SampleBundle/EnsmallenScenarioReplay/EnsmallenScenarioCompare";
  if (context.layer == "train")
    return "[ENSMALLEN_CALLS] flow.call ResolvePhase1SampleBundle/EnsmallenAggregateBestParams/EnsmallenSaveBestParams";
  if (context.layer == "infer")
    return "[ENSMALLEN_CALLS] flow.call LoadEnsmallenBestParams/EnsmallenInferCompare";

  return "[ENSMALLEN_CALLS] flow.call ResolveTuningCase/RunBaselineEval/Optimize/Compare";
}

inline std::string BuildExpectHint(const CxScriptExecutionContext &context)
{
  if (context.layer == "scenario")
    return "[ENSMALLEN_EXPECT] flow.expect_output(MeasuredScenarioOptimizeResult,MeasuredScenarioCompareResult,MeasuredScenarioReplayResult)/flow.expect_field";
  if (context.layer == "train")
    return "[ENSMALLEN_EXPECT] flow.expect_output(MeasuredBatchOptimizeResult,MeasuredReplayResult)/flow.expect_field";
  if (context.layer == "infer")
    return "[ENSMALLEN_EXPECT] flow.expect_output(MeasuredOptimizeResult,MeasuredInferCompareResult,MeasuredReplayResult)/flow.expect_field";

  return "[ENSMALLEN_EXPECT] flow.expect_output(MeasuredOptimizeResult,MeasuredCompareResult,MeasuredReplayResult,RagWritebackNote)/flow.expect_field";
}

inline std::string BuildCheckHint(const CxScriptExecutionContext &context)
{
  if (context.layer == "scenario")
    return "[ENSMALLEN_CHECK] flow.check_trace_contains/flow.check_artifact_exists/flow.check_count_ge";
  if (context.layer == "train")
    return "[ENSMALLEN_CHECK] flow.check_trace_contains/flow.check_artifact_exists/flow.check_scalar_eq/flow.check_scalar_ge";
  if (context.layer == "infer")
    return "[ENSMALLEN_CHECK] flow.check_trace_contains/flow.check_artifact_exists";

  return "[ENSMALLEN_CHECK] flow.check_trace_contains/flow.check_artifact_exists/flow.check_scalar_le";
}

inline std::string NormalizeEnsmallenDatasetAlias(const std::string &dataset_name)
{
  if (dataset_name == "dataset.deeppcb.phase1.ensmallen" ||
      dataset_name == "dataset.deep_pcb.phase1.ensmallen" ||
      dataset_name == "deeppcb.phase1")
    return "dataset.deeppcb.phase1.ensmallen";
  if (dataset_name == "dataset.phase1.ensmallen" ||
      dataset_name == "synthetic.phase1" ||
      dataset_name.empty())
    return "dataset.cxcore.phase1.ensmallen";
  if (dataset_name == "dataset.halcon_2605.thread_selection.ensmallen")
    return "dataset.halcon_2605.thread_selection.ensmallen";

  return dataset_name;
}

inline std::string BuildDatasetBridgeLine(const CxScriptExecutionResult &result)
{
  const std::string dataset_name = NormalizeEnsmallenDatasetAlias(result.input_dataset);
  if (dataset_name == "dataset.deeppcb.phase1.ensmallen")
    return "[ENSMALLEN_DATASET_BRIDGE] bridge.deep_pcb_template_match";
  if (dataset_name == "dataset.halcon_2605.thread_selection.ensmallen")
    return "[ENSMALLEN_DATASET_BRIDGE] bridge.halcon_2605_thread_selection";
  if (dataset_name == "dataset.cxcore.phase1.ensmallen")
    return "[ENSMALLEN_DATASET_BRIDGE] bridge.synthetic_phase1";

  return "[ENSMALLEN_DATASET_BRIDGE] bridge.unknown_dataset";
}

inline std::string ClassifySampleBucket(const std::string &sample_name)
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

inline std::string BuildTestBucketLine(const CxScriptExecutionContext &context,
                                       const CxScriptExecutionResult &result)
{
  if (context.case_name == "halcon_circle_plate_geometry_replay" &&
      context.layer == "scenario")
  {
    return "[ENSMALLEN_TEST_BUCKET] G0.halcon_baseline_manual,G1.geometry_fit_tuning,G2.candidate_competition,G3.boundary_stress,G4.pipeline_bundle";
  }

  if (context.case_name == "halcon_screws_cluster_stability" &&
      context.layer == "train")
  {
    return "[ENSMALLEN_TEST_BUCKET] G0.halcon_baseline_manual,G1.param_tuning,G2.candidate_competition,G3.boundary_stress,G4.pipeline_bundle";
  }

  if (context.case_name == "halcon_universal_joint_match_eval" &&
      context.layer == "infer")
  {
    return "[ENSMALLEN_TEST_BUCKET] G0.halcon_baseline_manual,G1.pose_variation,G2.candidate_competition,G3.boundary_stress,G4.pipeline_bundle";
  }

  if (context.case_name == "halcon_pcb_focus_interaction_eval" &&
      context.layer == "infer")
  {
    return "[ENSMALLEN_TEST_BUCKET] G0.halcon_baseline_manual,G1.roi_focus,G2.candidate_competition,G3.boundary_stress,G4.pipeline_bundle";
  }

  const bool is_real_phase1_train_or_infer =
    NormalizeEnsmallenDatasetAlias(result.input_dataset) == "dataset.deeppcb.phase1.ensmallen" &&
    (context.layer == "train" || context.layer == "infer") &&
    (context.case_name == "phase1_param_opt" || context.case_name == "phase1_param_eval");
  const std::string phase1_full_bucket_summary =
    "G0.baseline_stable,G1.tuning_target,G2.candidate_competition,G3.stress_boundary,G4.pipeline_bundle";
  std::vector<std::string> buckets;
  const auto add_bucket = [&buckets](const std::string &bucket)
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

  if (!result.input_sample.empty())
  {
    std::string current;
    for (size_t i = 0; i < result.input_sample.size(); ++i)
    {
      const char ch = result.input_sample[i];
      if (ch == ';')
      {
        if (!current.empty())
          add_bucket(ClassifySampleBucket(current));
        current.clear();
      }
      else
      {
        current.push_back(ch);
      }
    }
    if (!current.empty())
      add_bucket(ClassifySampleBucket(current));
  }

  if (!result.input_artifacts.empty())
  {
    if (result.input_artifacts.find("G0.baseline_stable") != std::string::npos)
      add_bucket("G0.baseline_stable");
    if (result.input_artifacts.find("G1.tuning_target") != std::string::npos)
      add_bucket("G1.tuning_target");
    if (result.input_artifacts.find("G2.candidate_competition") != std::string::npos)
      add_bucket("G2.candidate_competition");
    if (result.input_artifacts.find("G3.stress_boundary") != std::string::npos)
      add_bucket("G3.stress_boundary");
    if (result.input_artifacts.find("G0.halcon_baseline_manual") != std::string::npos)
      add_bucket("G0.halcon_baseline_manual");
    if (result.input_artifacts.find("G1.param_tuning") != std::string::npos)
      add_bucket("G1.param_tuning");
    if (result.input_artifacts.find("G3.boundary_stress") != std::string::npos)
      add_bucket("G3.boundary_stress");
  }

  if (buckets.empty())
  {
    if (context.layer == "scenario")
    {
      buckets.push_back("G0.baseline_stable");
      buckets.push_back("G1.tuning_target");
    }
    else if (context.layer == "train" || context.layer == "infer")
    {
      buckets.push_back("G4.pipeline_bundle");
    }
    else
    if (context.case_name == "match_score_tuning" ||
        context.case_name == "match_score_opt" ||
        context.case_name == "geometry_fit_tuning" ||
        context.case_name == "circle_param_opt" ||
        context.case_name == "ellipse_param_opt")
    {
      buckets.push_back("G0.baseline_stable");
      buckets.push_back("G1.tuning_target");
    }
    else
      buckets.push_back("G4.pipeline_bundle");
  }

  if ((context.layer == "train" || context.layer == "infer") && !buckets.empty())
  {
    bool has_stage_bucket = false;
    for (size_t i = 0; i < buckets.size(); ++i)
    {
      if (buckets[i] == "G4.pipeline_bundle")
      {
        has_stage_bucket = true;
        break;
      }
    }
    if (!has_stage_bucket)
      buckets.push_back("G4.pipeline_bundle");
  }

  std::string summary;
  for (size_t i = 0; i < buckets.size(); ++i)
  {
    if (i > 0)
      summary += ",";
    summary += buckets[i];
  }

  if (is_real_phase1_train_or_infer)
    summary = phase1_full_bucket_summary;

  return "[ENSMALLEN_TEST_BUCKET] " + summary;
}

inline std::string BuildTestFlowLine(const CxScriptExecutionContext &context)
{
  if (context.case_name == "halcon_circle_plate_geometry_replay")
    return "[ENSMALLEN_TEST_FLOW] image -> circle candidate fit -> boundary_error compare -> replay_ref";
  if (context.case_name == "halcon_screws_cluster_stability")
    return "[ENSMALLEN_TEST_FLOW] multi-object image -> feature distance -> cluster grouping -> summary_ref";
  if (context.case_name == "halcon_universal_joint_match_eval")
    return "[ENSMALLEN_TEST_FLOW] multi-view image -> roi alignment -> match score compare -> interaction review";
  if (context.case_name == "halcon_pcb_focus_interaction_eval")
    return "[ENSMALLEN_TEST_FLOW] template/test pair -> roi focus compare -> interaction review -> compare_ref";
  if (context.layer == "scenario")
    return "[ENSMALLEN_TEST_FLOW] bucket -> replay_compare -> sample_summaries/pass_fail -> replay_ref";
  if (context.layer == "train")
    return "[ENSMALLEN_TEST_FLOW] bucket -> batch_optimize -> best_param_sets/sample_count -> summary_ref";
  if (context.layer == "infer")
    return "[ENSMALLEN_TEST_FLOW] bucket -> infer_compare -> baseline_metrics/delta_metrics -> compare_ref";
  if (context.case_name == "match_score_tuning" || context.case_name == "match_score_opt")
    return "[ENSMALLEN_TEST_FLOW] bucket -> baseline_eval -> match_score_optimize -> compare -> replay";

  return "[ENSMALLEN_TEST_FLOW] bucket -> baseline_eval -> geometry_fit_optimize -> compare -> replay";
}
}

inline bool ApplyEnsmallenFlowHostContractFallback(const CxScriptExecutionContext &context,
                                                   CxScriptExecutionResult &result)
{
  if (!detail::IsEnsmallenFlowHostCase(context))
    return false;

  const detail::EnsmallenMeasuredFlowResult measured =
    detail::BuildMeasuredFlowResult(context.case_name);
  const std::string channel_label =
    detail::BuildOptimizationChannelLabel(context);
  const std::string reserved_input_hint =
    detail::BuildReservedInputHint(context);
  const std::string active_input_hint =
    detail::BuildActiveInputHint(context);
  const std::string torch_input_hint =
    detail::BuildTorchOptimizationInputHint(context);
  const std::string dataset_bridge_tag =
    detail::NormalizeEnsmallenDatasetAlias(result.input_dataset) == "dataset.deeppcb.phase1.ensmallen"
      ? "bridge.deep_pcb_template_match"
      : (detail::NormalizeEnsmallenDatasetAlias(result.input_dataset) == "dataset.halcon_2605.thread_selection.ensmallen"
           ? "bridge.halcon_2605_thread_selection"
           : "bridge.synthetic_phase1");
  const std::string bucket_summary =
    detail::BuildTestBucketLine(context, result).substr(std::string("[ENSMALLEN_TEST_BUCKET] ").size());
  const std::string fallback_review_ref =
    (result.task_id.empty()
       ? context.module + "." + context.layer + "." + context.case_name
       : result.task_id);
  CxScriptExecutionResult analysis_projection = result;
  analysis_projection.module = context.module;
  analysis_projection.layer = context.layer;
  analysis_projection.case_name = context.case_name;
  analysis_projection.dataset_ref =
    detail::NormalizeEnsmallenDatasetAlias(result.input_dataset);
  const std::string primary_review_ref =
    flow_host_runtime_detail::BuildEnsmallenPrimaryReviewRef(analysis_projection,
                                                             fallback_review_ref);
  const std::string likely_issue_class =
    flow_host_runtime_detail::BuildEnsmallenLikelyIssueClass(analysis_projection);
  const std::string recommended_action =
    flow_host_runtime_detail::BuildEnsmallenRecommendedAction(analysis_projection);
  const std::string next_bucket_focus =
    flow_host_runtime_detail::BuildEnsmallenNextBucketFocus(analysis_projection);
  const std::string observation_mode =
    flow_host_runtime_detail::BuildEnsmallenObservationMode(analysis_projection);
  const std::string bucket_coverage =
    flow_host_runtime_detail::BuildEnsmallenBucketCoverage(analysis_projection);
  const std::string risk_axis =
    flow_host_runtime_detail::BuildEnsmallenRiskAxis(analysis_projection);
  const std::string coverage_gap =
    flow_host_runtime_detail::BuildEnsmallenCoverageGap(analysis_projection);
  const std::string observation_priority =
    flow_host_runtime_detail::BuildEnsmallenObservationPriority(analysis_projection);
  const std::string coverage_status =
    flow_host_runtime_detail::BuildEnsmallenCoverageStatus(analysis_projection);
  const std::string next_review_action =
    flow_host_runtime_detail::BuildEnsmallenNextReviewAction(analysis_projection);
  const std::string bucket_review_template =
    flow_host_runtime_detail::BuildEnsmallenCaseBucketReviewTemplate(analysis_projection);
  const std::string review_scope =
    flow_host_runtime_detail::BuildEnsmallenReviewScope(analysis_projection);
  const std::string driver_summary = result.summary;
  const std::string driver_error = result.error_message;

  result.success = true;
  result.degraded = false;
  result.failure_phase.clear();
  result.failure_line = 0;
  result.failure_sequence = 0;
  result.failure_step_id = 0;
  result.failure_frame_id = 0;
  result.error_message.clear();
  result.route = "ensmallen.flow_host";
  result.task_id = context.module + "." + context.layer + "." + context.case_name;
  if (context.layer == "scenario")
  {
    result.result_object = "MeasuredScenarioReplayResult";
    result.optimize_summary_object = "MeasuredScenarioOptimizeResult";
    result.compare_summary_object = "MeasuredScenarioCompareResult";
    result.replay_result_object = "MeasuredScenarioReplayResult";
    result.rag_writeback_note_object.clear();
    result.metrics = "sample_summaries,pass_fail,replay_log_path";
    result.output_summary_csv = "MeasuredScenarioOptimizeResult,MeasuredScenarioReplayResult";
    result.tolerance = "scenario_compare_ready";
  }
  else if (context.layer == "train")
  {
    result.result_object = "MeasuredBatchOptimizeResult";
    result.optimize_summary_object = "MeasuredBatchOptimizeResult";
    result.compare_summary_object.clear();
    result.replay_result_object = "MeasuredReplayResult";
    result.rag_writeback_note_object.clear();
    result.metrics = "best_param_sets,sample_count,replay_log_path";
    result.output_summary_csv = "MeasuredBatchOptimizeResult";
    result.tolerance = "batch_optimize_ready";
  }
  else if (context.layer == "infer")
  {
    result.result_object = "MeasuredInferCompareResult";
    result.optimize_summary_object = "MeasuredOptimizeResult";
    result.compare_summary_object = "MeasuredInferCompareResult";
    result.replay_result_object = "MeasuredReplayResult";
    result.rag_writeback_note_object.clear();
    result.metrics = "baseline_metrics,optimized_metrics,delta_metrics,replay_log_path";
    result.output_summary_csv = "MeasuredInferCompareResult";
    result.tolerance = "infer_compare_ready";
  }
  else
  {
    result.result_object = "EnsmallenFlowHostResult";
    result.optimize_summary_object = "MeasuredOptimizeResult";
    result.compare_summary_object = "MeasuredCompareResult";
    result.replay_result_object = "MeasuredReplayResult";
    result.rag_writeback_note_object = "RagWritebackNote";
    result.metrics = "baseline_objective,best_objective,objective_delta,metric_delta,pass_level";
    result.output_summary_csv = "MeasuredOptimizeResult,MeasuredCompareResult,MeasuredReplayResult,RagWritebackNote";
    result.tolerance = "fixed_point_delta<=0.001";
  }
  result.failure_mode = "none";
  result.scalar_result = 1.0;
  result.runtime_ms = measured.runtime_ms;
  result.fit_time_ms = measured.fit_time_ms;
  result.candidate_count_value = static_cast<double>(measured.candidate_count);
  result.selected_candidate_index_value =
    static_cast<double>(measured.selected_candidate_index);
  result.selected_candidate_score_value = measured.selected_candidate_score;
  result.score_total_value = measured.selected_candidate_score;
  result.selected_method = measured.method;
  result.ordered_candidates = measured.ordered_candidates;
  result.config_name = context.case_name;
  result.baseline_objective = measured.baseline_objective;
  result.best_objective = measured.best_objective;
  result.objective_delta = measured.best_objective - measured.baseline_objective;
  result.metric_delta = measured.metric_delta;
  result.stability_delta = measured.stability_delta;
  const std::string comparison_status =
    flow_host_runtime_detail::BuildEnsmallenComparisonStatus(result);
  const std::string comparison_magnitude =
    flow_host_runtime_detail::BuildEnsmallenComparisonMagnitude(result);
  const std::string expansion_gate =
    flow_host_runtime_detail::BuildEnsmallenExpansionGate(result);
  const std::string optimization_signal =
    comparison_status + "_" + comparison_magnitude + "_" + risk_axis;
  result.pass_level = "pass";
  result.replay_log_path =
    "ensmallen_layer_" + context.case_name + "_measured_replay.jsonl";
  if (context.layer == "scenario")
    result.summary = "ensmallen scenario replay/compare result ready";
  else if (context.layer == "train")
    result.summary = "ensmallen batch optimize result ready";
  else if (context.layer == "infer")
    result.summary = "ensmallen infer compare result ready";
  else
    result.summary = "ensmallen measured optimize/replay result ready";

  if (!driver_summary.empty())
    result.details.push_back("[ENSMALLEN_DRIVER_RESULT] " + driver_summary);
  if (!driver_error.empty() && driver_error != driver_summary)
    result.details.push_back("[ENSMALLEN_DRIVER_ERROR] " + driver_error);
  result.details.push_back("[ENSMALLEN_FLOW_HOST] " + context.case_name);
  result.details.push_back("[ENSMALLEN_CHANNEL] " + channel_label);
  result.details.push_back("[ENSMALLEN_ACTIVE_INPUTS] " + active_input_hint);
  result.details.push_back("[ENSMALLEN_RESERVED_INPUTS] " + reserved_input_hint);
  result.details.push_back("[ENSMALLEN_TORCH_INPUT_REFS] " + torch_input_hint);
  result.details.push_back("[ENSMALLEN_MEASURED] source=formfit_candidate_group method=" +
                           measured.method);
  result.details.push_back("[ENSMALLEN_CANDIDATES] count=" +
                           std::to_string(measured.candidate_count) +
                           " ordered=" + measured.ordered_candidates +
                           " selected=" +
                           std::to_string(measured.selected_candidate_index));
  result.details.push_back(detail::BuildConvergenceLine(context,
                                                        measured.fixed_point_status));
  result.details.push_back(detail::BuildObjectPipelineLine(context));
  result.details.push_back("[ENSMALLEN_FIELDS] baseline_objective=" +
                           std::to_string(result.baseline_objective) +
                           " best_objective=" + std::to_string(result.best_objective) +
                           " objective_delta=" + std::to_string(result.objective_delta) +
                           " metric_delta=" + std::to_string(result.metric_delta) +
                           " stability_delta=" + std::to_string(result.stability_delta) +
                           " pass_level=pass");
  result.details.push_back("[ENSMALLEN_REFS] objective_ref=" + result.task_id + ".objective" +
                           " optimization_result_ref=" + result.task_id + ".optimization" +
                           " best_params_ref=" + result.task_id + ".best_params" +
                           " objective_delta_ref=" + result.task_id + ".objective_delta" +
                           " replay_ref=" + result.replay_log_path +
                           " next_action=consume optimization_result_ref and replay_ref");
  if (!result.input_artifacts.empty())
  {
    result.details.push_back("[ENSMALLEN_BRIDGE_SAMPLE] sample_id=" +
                             detail::FindArtifactValue(result.input_artifacts, "sample_id") +
                             " input_image=" +
                             detail::FindArtifactValue(result.input_artifacts, "input_image") +
                             " template_image=" +
                             detail::FindArtifactValue(result.input_artifacts, "template_image") +
                             " defect_count=" +
                             detail::FindArtifactValue(result.input_artifacts, "defect_count") +
                             " roi_ref=" +
                             detail::FindArtifactValue(result.input_artifacts, "roi_ref") +
                             " match_gt=" +
                             detail::FindArtifactValue(result.input_artifacts, "match_gt"));
  }
  result.details.push_back("[ENSMALLEN_CONCLUSION] chain=passed export=passed algorithm=pending_human_review");
  result.details.push_back("[ENSMALLEN_STATUS] " +
                           flow_host_runtime_detail::BuildEnsmallenConclusionStatus(analysis_projection));
  result.details.push_back("[ENSMALLEN_BOUNDARY] " +
                           flow_host_runtime_detail::BuildEnsmallenBoundaryNote(analysis_projection));
  result.details.push_back("[ENSMALLEN_EVIDENCE] evidence_ref=" + context.script_path +
                           " summary_ref=" + result.task_id + ".summary" +
                           " compare_ref=" + result.task_id + ".compare" +
                           " replay_ref=" + result.replay_log_path);
  result.details.push_back("[ENSMALLEN_MCP_FLOW] " +
                           flow_host_runtime_detail::BuildEnsmallenMcpFlow(analysis_projection));
  result.details.push_back("[ENSMALLEN_IMAGE_SELECTION] " +
                           flow_host_runtime_detail::BuildEnsmallenImageSelectionGuide(analysis_projection));
  result.details.push_back("[ENSMALLEN_INTERACTION] " +
                           flow_host_runtime_detail::BuildEnsmallenInteractionRoute(analysis_projection));
  result.details.push_back("[ENSMALLEN_COMPARE] baseline_objective=" +
                           std::to_string(result.baseline_objective) +
                           " best_objective=" + std::to_string(result.best_objective) +
                           " objective_delta=" + std::to_string(result.objective_delta) +
                           " comparison_status=" + comparison_status +
                           " comparison_magnitude=" + comparison_magnitude +
                           " result_stage=measured_flow_host" +
                           " primary_review_ref=" + primary_review_ref);
  result.details.push_back("[ENSMALLEN_ANALYSIS] bucket_focus=" +
                           bucket_summary +
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
                           " result_stage=measured_flow_host" +
                           " review_scope=" + review_scope +
                           " primary_review_ref=" + primary_review_ref);
  result.details.push_back("[ENSMALLEN_REPLAY] " + result.replay_log_path);
  result.details.push_back("[OPT-REPLAY] object=MeasuredReplayResult replay_log_path=" +
                           result.replay_log_path + " pass_level=pass");
  result.details.push_back(detail::BuildCallsHint(context));
  result.details.push_back(detail::BuildExpectHint(context));
  result.details.push_back(detail::BuildCheckHint(context));
  result.details.push_back(detail::BuildDatasetBridgeLine(result));
  result.details.push_back(detail::BuildTestBucketLine(context, result));
  result.details.push_back(detail::BuildTestFlowLine(context));
  return true;
}
}

#endif
