#include <iostream>

#include "../catalog/parser_case_catalog.h"
#include "../drivers/parser_dispatch_driver.h"

namespace
{
bool HasLineWithPrefix(const cxparser_ext::ParserDispatchResult &result,
                       const char *prefix);
bool HasLineFragment(const cxparser_ext::ParserDispatchResult &result,
                     const char *fragment);

cxparser_ext::ParserDispatchRequest MakeCxcoreFeatureRequest(const char *case_id)
{
  cxparser_ext::ParserDispatchRequest request;
  request.layer = "feature";
  request.module = "cxcore";
  request.case_id = case_id;
  request.mode = "build-run";
  return request;
}

bool RunCase(const cxparser_ext::ParserDispatchRequest &request)
{
  cxparser_ext::ParserDispatchResult result;
  if (!cxparser_ext::RunDispatchRequest(request, result))
  {
    std::cerr << "[FAIL] dispatch failed for " << request.module
              << "." << request.layer << "." << request.case_id << "\n";
    return false;
  }

  if (request.module == "cxcore" && request.layer == "feature" && result.tick.executed_task_count != 1)
  {
    std::cerr << "[FAIL] cxcore feature dispatch did not execute expected task count\n";
    return false;
  }

  const bool is_planned = result.skipped;

  if (!is_planned &&
      (request.module == "cxcore" || request.module == "rag" || request.module == "ensmallen_layer") &&
      result.report.flow_profile.script_style.empty())
  {
    std::cerr << "[FAIL] active dispatch should produce a flow profile\n";
    return false;
  }

  if (!is_planned && request.module == "rag" && result.report.script_origin != "catalog")
  {
    std::cerr << "[FAIL] rag dispatch should currently use catalog-backed execution\n";
    return false;
  }

  if (request.module == "cxcore" && request.case_id == "flow_numeric_bridge_v1")
  {
    if (result.report.script_origin != "file" ||
        !result.report.ir_valid ||
        result.report.flow_profile.script_style != "flow_style" ||
        result.report.layer_profile.execution_text_kind != "catalog_fallback" ||
        result.report.layer_profile.bridge_exec_safe ||
        result.report.binding_semantics.binding_scope != "native_only" ||
        result.report.layer_profile.fallback_reason != "bridge_subset_not_safe")
    {
      std::cerr << "[FAIL] cxcore flow bridge should preserve file-origin flow metadata\n";
      return false;
    }
  }

  if (request.module == "cxcore" && request.case_id == "flow_numeric_safe_bridge_v1")
  {
    if (result.report.script_origin != "file" ||
        !result.report.ir_valid ||
        result.report.flow_profile.script_style != "flow_style" ||
        !result.report.layer_profile.bridge_exec_safe ||
        !result.report.basic_semantics.has_assignment ||
        result.report.binding_semantics.binding_scope != "native_only" ||
        result.report.layer_profile.bridge_exec_subset != "numeric_stmt" ||
        result.report.layer_profile.execution_text_kind != "compile_bridge")
    {
      std::cerr << "[FAIL] cxcore safe flow bridge should execute through compile bridge\n";
      return false;
    }
  }

  if (request.module == "cxcore" && request.case_id == "image_probe_flow_exec_v1")
  {
    if (result.report.script_origin != "file" ||
        !result.report.ir_valid ||
        result.report.flow_profile.script_style != "flow_style" ||
        !result.report.layer_profile.bridge_exec_safe ||
        !result.report.basic_semantics.has_declaration ||
        !result.report.basic_semantics.has_call_stmt ||
        !result.report.binding_semantics.requires_registered_binding ||
        result.report.binding_semantics.binding_scope != "object_binding" ||
        result.report.layer_profile.bridge_exec_subset != "object_flow" ||
        result.report.layer_profile.bridge_exec_reason != "object_flow_subset" ||
        result.report.layer_profile.execution_text_kind != "compile_bridge")
    {
      std::cerr << "[FAIL] image probe flow should execute through object flow bridge\n";
      return false;
    }
  }

  if (!is_planned &&
      request.module == "ensmallen_layer" &&
      result.report.script_origin != "file")
  {
    std::cerr << "[FAIL] ensmallen dispatch should load flow script from file\n";
    return false;
  }

  if (!is_planned &&
      request.module == "ensmallen_layer" &&
      (request.layer == "feature" ||
       request.layer == "scenario" ||
       request.layer == "train" ||
       request.layer == "infer"))
  {
    const bool is_match_score_case =
      request.case_id == "match_score_tuning" || request.case_id == "match_score_opt";
    const bool is_scenario_layer = request.layer == "scenario";
    const bool is_train_layer = request.layer == "train";
    const bool is_infer_layer = request.layer == "infer";
    const char *expected_result_object =
      is_scenario_layer ? "MeasuredScenarioReplayResult" :
      (is_train_layer ? "MeasuredBatchOptimizeResult" :
       (is_infer_layer ? "MeasuredInferCompareResult" : "EnsmallenFlowHostResult"));
    const char *expected_summary =
      is_scenario_layer ? "ensmallen scenario replay/compare result ready" :
      (is_train_layer ? "ensmallen batch optimize result ready" :
       (is_infer_layer ? "ensmallen infer compare result ready" :
        "ensmallen measured optimize/replay result ready"));
    const char *expected_channel =
      is_scenario_layer ? "[ENSMALLEN_CHANNEL] phase1.replay_compare_stage" :
      (is_train_layer ? "[ENSMALLEN_CHANNEL] phase1.batch_optimize_stage" :
       (is_infer_layer ? "[ENSMALLEN_CHANNEL] phase1.infer_compare_stage" :
        (is_match_score_case
           ? "[ENSMALLEN_CHANNEL] fastmatch.structural_match_channel"
           : "[ENSMALLEN_CHANNEL] formfit.geometry_fit_channel")));
    const char *expected_active_inputs =
      is_scenario_layer ? "[ENSMALLEN_ACTIVE_INPUTS] active_inputs=dataset_ref,sample_bundle_ref,repeat_count,replay_enable,compare_enable,objective_ref,threshold_ref,crop_policy_ref,boundary_error_ref,alignment_error_ref" :
      (is_train_layer ? "[ENSMALLEN_ACTIVE_INPUTS] active_inputs=dataset_ref,split_ref,task_scope,optimizer_name,max_evals,patience,epsilon,objective_ref,threshold_ref,crop_policy_ref,boundary_error_ref,alignment_error_ref" :
       (is_infer_layer ? "[ENSMALLEN_ACTIVE_INPUTS] active_inputs=dataset_ref,best_params_ref,compare_enable,baseline_only,objective_ref,threshold_ref,crop_policy_ref,boundary_error_ref,alignment_error_ref" :
        (is_match_score_case
           ? "[ENSMALLEN_ACTIVE_INPUTS] active_inputs=roi_ref,match_gt,params,objective_weights,objective_ref,threshold_ref,crop_policy_ref"
           : "[ENSMALLEN_ACTIVE_INPUTS] active_inputs=geometry_ref,boundary_metrics_ref,fit_targets_ref,params,objective_weights,objective_ref,boundary_error_ref,alignment_error_ref")));
    const char *expected_reserved_inputs =
      is_scenario_layer ? "[ENSMALLEN_RESERVED_INPUTS] reserved_stage_outputs=sample_summaries,pass_fail,replay_log_path,scenario_compare.json" :
      (is_train_layer ? "[ENSMALLEN_RESERVED_INPUTS] reserved_stage_outputs=best_param_sets,sample_count,replay_log_path,batch_best_params.json,batch_summary.json" :
       (is_infer_layer ? "[ENSMALLEN_RESERVED_INPUTS] reserved_stage_outputs=baseline_metrics,optimized_metrics,delta_metrics,replay_log_path,baseline_report.json,optimized_report.json,infer_compare.json" :
        (is_match_score_case
           ? "[ENSMALLEN_RESERVED_INPUTS] reserved_region_pattern_inputs=RegionPatternConfig,RegionPatternDescriptor,RegionPatternScore"
           : "[ENSMALLEN_RESERVED_INPUTS] reserved_geometry_inputs=geometry_ref,boundary_metrics_ref,fit_targets_ref")));
    const char *expected_torch_inputs =
      (is_scenario_layer || is_train_layer || is_infer_layer)
        ? "[ENSMALLEN_TORCH_INPUT_REFS] torch_optimization_inputs=objective_ref,threshold_ref,crop_policy_ref,boundary_error_ref,alignment_error_ref"
        : (is_match_score_case
             ? "[ENSMALLEN_TORCH_INPUT_REFS] torch_optimization_inputs=objective_ref,threshold_ref,crop_policy_ref"
             : "[ENSMALLEN_TORCH_INPUT_REFS] torch_optimization_inputs=objective_ref,boundary_error_ref,alignment_error_ref");
    const char *expected_convergence =
      is_scenario_layer ? "[ENSMALLEN_CONVERGENCE] status=converged tolerance=scenario_compare_ready" :
      (is_train_layer ? "[ENSMALLEN_CONVERGENCE] status=converged tolerance=batch_optimize_ready" :
       (is_infer_layer ? "[ENSMALLEN_CONVERGENCE] status=converged tolerance=infer_compare_ready" :
        "[ENSMALLEN_CONVERGENCE] status=converged tolerance=fixed_point_delta<=0.001"));
    const char *expected_objects =
      is_scenario_layer ? "[ENSMALLEN_OBJECTS] MeasuredScenarioOptimizeResult->MeasuredScenarioCompareResult->MeasuredScenarioReplayResult" :
      (is_train_layer ? "[ENSMALLEN_OBJECTS] MeasuredBatchOptimizeResult->MeasuredReplayResult" :
       (is_infer_layer ? "[ENSMALLEN_OBJECTS] MeasuredOptimizeResult->MeasuredInferCompareResult->MeasuredReplayResult" :
        "[ENSMALLEN_OBJECTS] MeasuredOptimizeResult->MeasuredCompareResult->MeasuredReplayResult->RagWritebackNote"));
    const char *expected_calls =
      is_scenario_layer ? "[ENSMALLEN_CALLS] flow.call ResolvePhase1SampleBundle/EnsmallenScenarioReplay/EnsmallenScenarioCompare" :
      (is_train_layer ? "[ENSMALLEN_CALLS] flow.call ResolvePhase1SampleBundle/EnsmallenAggregateBestParams/EnsmallenSaveBestParams" :
       (is_infer_layer ? "[ENSMALLEN_CALLS] flow.call LoadEnsmallenBestParams/EnsmallenInferCompare" :
        "[ENSMALLEN_CALLS] flow.call ResolveTuningCase/RunBaselineEval/Optimize/Compare"));
    const char *expected_expect =
      is_scenario_layer ? "[ENSMALLEN_EXPECT] flow.expect_output(MeasuredScenarioOptimizeResult,MeasuredScenarioCompareResult,MeasuredScenarioReplayResult)/flow.expect_field" :
      (is_train_layer ? "[ENSMALLEN_EXPECT] flow.expect_output(MeasuredBatchOptimizeResult,MeasuredReplayResult)/flow.expect_field" :
       (is_infer_layer ? "[ENSMALLEN_EXPECT] flow.expect_output(MeasuredOptimizeResult,MeasuredInferCompareResult,MeasuredReplayResult)/flow.expect_field" :
        "[ENSMALLEN_EXPECT] flow.expect_output(MeasuredOptimizeResult,MeasuredCompareResult,MeasuredReplayResult,RagWritebackNote)/flow.expect_field"));
    const char *expected_check =
      is_scenario_layer ? "[ENSMALLEN_CHECK] flow.check_trace_contains/flow.check_artifact_exists/flow.check_count_ge" :
      (is_train_layer ? "[ENSMALLEN_CHECK] flow.check_trace_contains/flow.check_artifact_exists/flow.check_scalar_eq/flow.check_scalar_ge" :
       (is_infer_layer ? "[ENSMALLEN_CHECK] flow.check_trace_contains/flow.check_artifact_exists" :
        "[ENSMALLEN_CHECK] flow.check_trace_contains/flow.check_artifact_exists/flow.check_scalar_le"));
    const char *expected_test_bucket =
      is_scenario_layer ? "[ENSMALLEN_TEST_BUCKET] G0.baseline_stable,G1.tuning_target,G2.candidate_competition,G3.stress_boundary" :
      (is_train_layer ? "[ENSMALLEN_TEST_BUCKET] G0.baseline_stable,G1.tuning_target,G2.candidate_competition,G3.stress_boundary,G4.pipeline_bundle" :
       (is_infer_layer ? "[ENSMALLEN_TEST_BUCKET] G0.baseline_stable,G1.tuning_target,G2.candidate_competition,G3.stress_boundary,G4.pipeline_bundle" :
        (is_match_score_case
           ? "[ENSMALLEN_TEST_BUCKET] G0.baseline_stable,G1.tuning_target"
           : "[ENSMALLEN_TEST_BUCKET] G0.baseline_stable,G1.tuning_target")));
    const char *expected_test_flow =
      is_scenario_layer ? "[ENSMALLEN_TEST_FLOW] bucket -> replay_compare -> sample_summaries/pass_fail -> replay_ref" :
      (is_train_layer ? "[ENSMALLEN_TEST_FLOW] bucket -> batch_optimize -> best_param_sets/sample_count -> summary_ref" :
       (is_infer_layer ? "[ENSMALLEN_TEST_FLOW] bucket -> infer_compare -> baseline_metrics/delta_metrics -> compare_ref" :
        (is_match_score_case
           ? "[ENSMALLEN_TEST_FLOW] bucket -> baseline_eval -> match_score_optimize -> compare -> replay"
           : "[ENSMALLEN_TEST_FLOW] bucket -> baseline_eval -> geometry_fit_optimize -> compare -> replay")));
    const char *expected_dataset_bridge =
      (is_scenario_layer || is_train_layer || is_infer_layer)
        ? "[ENSMALLEN_DATASET_BRIDGE] bridge.deep_pcb_template_match"
        : "[ENSMALLEN_DATASET_BRIDGE] bridge.synthetic_phase1";
    const char *expected_conclusion =
      "[ENSMALLEN_CONCLUSION] chain=passed export=passed algorithm=pending_human_review";
    const char *expected_status =
      (is_scenario_layer || is_train_layer || is_infer_layer)
        ? "[ENSMALLEN_STATUS] chain_ok,export_ok,real_image_pending_human_review"
        : "[ENSMALLEN_STATUS] chain_ok,export_ok,algorithm_pending_human_review";
    const char *expected_boundary =
      (is_scenario_layer || is_train_layer || is_infer_layer)
        ? "[ENSMALLEN_BOUNDARY] real_image_observation_required_before_algorithm_conclusion"
        : "[ENSMALLEN_BOUNDARY] chain_and_export_verified_only_algorithm_not_auto_concluded";
    const char *expected_mcp_flow =
      is_scenario_layer || is_train_layer || is_infer_layer
        ? "[ENSMALLEN_MCP_FLOW] run-ctest-target exact_name -> task_id -> status/log -> conclusion/evidence"
        : "[ENSMALLEN_MCP_FLOW] run-ctest-target exact_name -> task_id -> compare/replay -> conclusion/evidence";
    const char *expected_image_selection =
      is_match_score_case
        ? "[ENSMALLEN_IMAGE_SELECTION] select template_scene and template_scene_rotated before candidate competition images"
        : ((is_scenario_layer || is_train_layer || is_infer_layer)
            ? "[ENSMALLEN_IMAGE_SELECTION] select template/test pairs first then representative roi and gt"
            : "[ENSMALLEN_IMAGE_SELECTION] select baseline_stable and tuning_target geometry images first");
    const char *expected_interaction =
      is_match_score_case
        ? "[ENSMALLEN_INTERACTION] cxcore.fastmatch -> ensmallen -> rag"
        : ((is_scenario_layer || is_train_layer || is_infer_layer)
            ? "[ENSMALLEN_INTERACTION] torch.optimization_refs -> cxcore.phase1_bundle -> ensmallen -> rag"
            : "[ENSMALLEN_INTERACTION] cxcore.formfit -> ensmallen -> rag");
    const char *expected_analysis =
      is_scenario_layer ? "[ENSMALLEN_ANALYSIS] bucket_focus=G0.baseline_stable,G1.tuning_target,G2.candidate_competition,G3.stress_boundary likely_issue_class=boundary_degradation" :
      (is_train_layer ? "[ENSMALLEN_ANALYSIS] bucket_focus=G0.baseline_stable,G1.tuning_target,G2.candidate_competition,G3.stress_boundary,G4.pipeline_bundle likely_issue_class=boundary_degradation" :
       (is_infer_layer ? "[ENSMALLEN_ANALYSIS] bucket_focus=G0.baseline_stable,G1.tuning_target,G2.candidate_competition,G3.stress_boundary,G4.pipeline_bundle likely_issue_class=boundary_degradation" :
        (is_match_score_case
           ? "[ENSMALLEN_ANALYSIS] bucket_focus=G0.baseline_stable,G1.tuning_target likely_issue_class=parameter_sensitivity"
           : "[ENSMALLEN_ANALYSIS] bucket_focus=G0.baseline_stable,G1.tuning_target likely_issue_class=parameter_sensitivity")));
    const char *expected_compare =
      is_scenario_layer ? "[ENSMALLEN_COMPARE] baseline_objective=0.412000 best_objective=0.226000 objective_delta=-0.186000 comparison_status=improved" :
      (is_train_layer ? "[ENSMALLEN_COMPARE] baseline_objective=0.412000 best_objective=0.226000 objective_delta=-0.186000 comparison_status=improved" :
       (is_infer_layer ? "[ENSMALLEN_COMPARE] baseline_objective=0.412000 best_objective=0.226000 objective_delta=-0.186000 comparison_status=improved" :
        (is_match_score_case
           ? "[ENSMALLEN_COMPARE] baseline_objective=0.284000 best_objective=0.119000 objective_delta=-0.165000 comparison_status=improved"
           : "[ENSMALLEN_COMPARE] baseline_objective=0.368000 best_objective=0.194000 objective_delta=-0.174000 comparison_status=improved")));

    if (!result.success ||
        result.report.layer_profile.execution_text_kind != "source" ||
        result.tick.accepted_task_count != 1 ||
        result.tick.executed_task_count != 1 ||
        result.report.result_object != expected_result_object ||
        result.report.metrics.empty() ||
        result.report.summary.find(expected_summary) == std::string::npos)
    {
      std::cerr << "[FAIL] ensmallen flow host mainline should execute from file source\n";
      return false;
    }

    if (!HasLineFragment(result, expected_channel) ||
        !HasLineFragment(result, expected_active_inputs) ||
        !HasLineFragment(result, expected_reserved_inputs) ||
        !HasLineFragment(result, expected_torch_inputs) ||
        !HasLineFragment(result, expected_convergence) ||
        !HasLineFragment(result, expected_objects) ||
        !HasLineFragment(result, expected_calls) ||
        !HasLineFragment(result, expected_expect) ||
        !HasLineFragment(result, expected_check) ||
        !HasLineFragment(result, expected_test_bucket) ||
        !HasLineFragment(result, expected_test_flow) ||
        !HasLineFragment(result, expected_dataset_bridge) ||
        !HasLineFragment(result, expected_conclusion) ||
        !HasLineFragment(result, expected_status) ||
        !HasLineFragment(result, expected_boundary) ||
        !HasLineFragment(result, expected_mcp_flow) ||
        !HasLineFragment(result, expected_image_selection) ||
        !HasLineFragment(result, expected_interaction) ||
        !HasLineFragment(result, expected_analysis) ||
        !HasLineFragment(result, expected_compare) ||
        !HasLineFragment(result, "[ENSMALLEN_REFS] objective_ref="))
    {
      std::cerr << "[FAIL] ensmallen dispatch should expose channel/input/ref contract lines\n";
      return false;
    }
  }

  if (request.module == "rag" && request.layer == "scenario" && result.replay.replay_count != 1)
  {
    std::cerr << "[FAIL] rag scenario dispatch did not replay\n";
    return false;
  }

  if ((request.module == "torch" || request.module == "mlpack") && !result.skipped)
  {
    std::cerr << "[FAIL] planned module should currently be skipped\n";
    return false;
  }

  if (is_planned)
  {
    if (request.module == "ensmallen_layer" && result.identity.file_path.empty())
    {
      std::cerr << "[FAIL] planned ensmallen case should still resolve script path\n";
      return false;
    }

    if (request.module == "rag" && result.identity.file_path.empty())
    {
      std::cerr << "[FAIL] planned rag case should still resolve script path\n";
      return false;
    }
  }

  return true;
}

bool RunFailureCase(const cxparser_ext::ParserDispatchRequest &request)
{
  cxparser_ext::ParserDispatchResult result;
  if (cxparser_ext::RunDispatchRequest(request, result))
  {
    std::cerr << "[FAIL] dispatch should fail for "
              << request.module << "." << request.layer << "." << request.case_id << "\n";
    return false;
  }

  if (result.report.error_kind != "parser_exception")
  {
    std::cerr << "[FAIL] parser failure should surface parser_exception in report\n";
    return false;
  }

  if (result.report.parser_error_code < 0 || result.report.parser_error_expr.empty())
  {
    std::cerr << "[FAIL] parser failure should preserve parser error code and expr\n";
    return false;
  }

  return true;
}

bool RunFlowOverlayCase(const cxparser_ext::ParserDispatchRequest &request)
{
  cxparser_ext::ParserDispatchResult result;
  if (!cxparser_ext::RunDispatchRequest(request, result))
  {
    std::cerr << "[FAIL] flow overlay dispatch failed for "
              << request.module << "." << request.layer << "." << request.case_id << "\n";
    return false;
  }

  if (result.identity.file_path.empty())
  {
    std::cerr << "[FAIL] flow overlay probe should still resolve script identity\n";
    return false;
  }

  if (!result.skipped ||
      result.report.script_origin.empty())
  {
    std::cerr << "[FAIL] planned flow overlay case should remain skipped but script-aware\n";
    return false;
  }

  return true;
}

bool RunPairedReplayCase(const cxparser_ext::ParserDispatchRequest &request)
{
  cxparser_ext::ParserDispatchResult result;
  if (!cxparser_ext::RunDispatchRequest(request, result))
  {
    std::cerr << "[FAIL] paired replay dispatch failed for "
              << request.integration << "." << request.layer << "." << request.case_id << "\n";
    return false;
  }

  if (!result.success ||
      result.skipped ||
      result.status != "run_with_replay" ||
      result.report.script_origin != "file" ||
      result.report.route != "replay" ||
      result.replay.replay_count != 1 ||
      result.report.replay_count != 1 ||
      result.identity.file_path.find("integration_paired_replay_transport_switch.cxsc") == std::string::npos)
  {
    std::cerr << "[FAIL] paired replay dispatch metadata mismatch\n";
    return false;
  }

  if (!HasLineWithPrefix(result, "[REPLAY]") ||
      result.report.flow_profile.script_style.empty() ||
      result.report.layer_profile.execution_text_kind == "catalog_fallback" ||
      result.report.layer_profile.execution_text_kind == "not_executed")
  {
    std::cerr << "[FAIL] paired replay dispatch mainline/replay summary mismatch\n";
    return false;
  }

  return true;
}

bool RunIfProbeCase()
{
  cxparser_ext::ParserDispatchRequest request;
  request.layer = "feature";
  request.module = "cxcore";
  request.case_id = "flow_numeric_if_probe_v1";
  request.mode = "build-run";

  cxparser_ext::ParserDispatchResult result;
  if (!cxparser_ext::RunDispatchRequest(request, result))
  {
    std::cerr << "[FAIL] if probe should execute through compile bridge\n";
    return false;
  }

  if (!result.report.layer_profile.bridge_exec_safe ||
      !result.report.basic_semantics.has_if_block ||
      result.report.binding_semantics.binding_scope != "native_only" ||
      result.report.layer_profile.bridge_exec_subset != "numeric_if" ||
      result.report.layer_profile.bridge_exec_reason != "numeric_if_subset" ||
      result.report.layer_profile.execution_text_kind != "compile_bridge")
  {
    std::cerr << "[FAIL] if probe should execute through numeric if bridge\n";
    return false;
  }

  return true;
}

bool RunPlannedMlpackBaselineCase(const cxparser_ext::ParserDispatchRequest &request)
{
  cxparser_ext::ParserDispatchResult result;
  if (!cxparser_ext::RunDispatchRequest(request, result))
  {
    std::cerr << "[FAIL] mlpack baseline dispatch failed for "
              << request.layer << "." << request.case_id << "\n";
    return false;
  }

  if (!result.skipped ||
      result.report.script_origin != "file" ||
      result.identity.file_path.empty())
  {
    std::cerr << "[FAIL] mlpack baseline planned case should stay skipped but file-backed\n";
    return false;
  }

  if (result.report.flow_profile.script_style != "flow_style" ||
      !result.report.flow_profile.has_prepare ||
      !result.report.flow_profile.has_action ||
      !result.report.flow_profile.has_check ||
      !result.report.flow_profile.has_report)
  {
    std::cerr << "[FAIL] mlpack baseline planned case should expose flow summary\n";
    return false;
  }

  return true;
}

bool HasLineWithPrefix(const cxparser_ext::ParserDispatchResult &result,
                       const char *prefix)
{
  for (size_t i = 0; i < result.lines.size(); ++i)
  {
    if (result.lines[i].find(prefix) == 0)
      return true;
  }
  return false;
}

bool HasLineFragment(const cxparser_ext::ParserDispatchResult &result,
                     const char *fragment)
{
  for (size_t i = 0; i < result.lines.size(); ++i)
  {
    if (result.lines[i].find(fragment) != std::string::npos)
      return true;
  }
  return false;
}

bool RunActiveMlpackBaselineCase(const cxparser_ext::ParserDispatchRequest &request,
                                 const char *expected_task_id)
{
  cxparser_ext::ParserDispatchCaseSpec spec;
  if (!cxparser_ext::ResolveDispatchCase(request, spec))
  {
    std::cerr << "[FAIL] mlpack baseline case not resolved for "
              << request.layer << "." << request.case_id << "\n";
    return false;
  }

  if (!spec.active_runtime)
  {
    std::cerr << "[FAIL] mlpack baseline case should be active for "
              << request.layer << "." << request.case_id << "\n";
    return false;
  }

  cxparser_ext::ParserDispatchResult result;
  if (!cxparser_ext::RunDispatchRequest(request, result))
  {
    std::cerr << "[FAIL] mlpack baseline active dispatch failed for "
              << request.layer << "." << request.case_id << "\n";
    return false;
  }

  if (!result.success || result.skipped || result.status != "run_ok")
  {
    std::cerr << "[FAIL] mlpack baseline active dispatch should run for "
              << request.layer << "." << request.case_id << "\n";
    return false;
  }

  if (result.report.task_id != expected_task_id)
  {
    std::cerr << "[FAIL] mlpack baseline task id mismatch for "
              << request.layer << "." << request.case_id << "\n";
    return false;
  }

  if (!HasLineWithPrefix(result, "[PASS] module=mlpack") ||
      !HasLineWithPrefix(result, "[CONTRACT]") ||
      !HasLineWithPrefix(result, "[SUMMARY]"))
  {
    std::cerr << "[FAIL] mlpack baseline active dispatch missing contract lines\n";
    return false;
  }

  return true;
}

bool RunPlannedTorchGeometryCase(const cxparser_ext::ParserDispatchRequest &request)
{
  cxparser_ext::ParserDispatchResult result;
  if (!cxparser_ext::RunDispatchRequest(request, result))
  {
    std::cerr << "[FAIL] torch geometry dispatch failed for "
              << request.layer << "." << request.case_id << "\n";
    return false;
  }

  if (!result.skipped ||
      result.report.script_origin != "file" ||
      result.identity.file_path.empty())
  {
    std::cerr << "[FAIL] torch geometry planned case should stay skipped but file-backed\n";
    return false;
  }

  if (result.report.integration_name != "torch_geometry" ||
      result.report.flow_profile.script_style != "flow_style" ||
      !result.report.flow_profile.has_prepare ||
      !result.report.flow_profile.has_action ||
      !result.report.flow_profile.has_check ||
      !result.report.flow_profile.has_report)
  {
    std::cerr << "[FAIL] torch geometry planned case should expose integration flow summary\n";
    return false;
  }

  return true;
}

bool RunPlannedVideoClangBridgeCase(const cxparser_ext::ParserDispatchRequest &request)
{
  cxparser_ext::ParserDispatchResult result;
  if (!cxparser_ext::RunDispatchRequest(request, result))
  {
    std::cerr << "[FAIL] video clang bridge dispatch failed for "
              << request.layer << "." << request.case_id << "\n";
    return false;
  }

  if (!result.skipped ||
      result.report.script_origin != "file" ||
      result.identity.file_path.empty())
  {
    std::cerr << "[FAIL] video clang bridge case should stay skipped but file-backed\n";
    return false;
  }

  bool saw_bridge_point = false;
  for (size_t i = 0; i < result.report.bridge_point_lines.size(); ++i)
  {
    if (result.report.bridge_point_lines[i].find("clang point type_decl VideoFrame -> torch::VideoFrame [matched]") != std::string::npos)
      saw_bridge_point = true;
  }

  if (result.report.bridge_summary.find("clang points=") == std::string::npos ||
      result.report.bridge_point_count < 1 ||
      result.report.bridge_matched_call_count < 1 ||
      !saw_bridge_point)
  {
    std::cerr << "[FAIL] video clang bridge report should expose matched type point\n";
    return false;
  }

  return true;
}

bool RunActiveTorchGeometryContractCase(const cxparser_ext::ParserDispatchRequest &request)
{
  cxparser_ext::ParserDispatchResult result;
  if (!cxparser_ext::RunDispatchRequest(request, result))
  {
    std::cerr << "[FAIL] torch geometry contract dispatch failed for "
              << request.layer << "." << request.case_id << "\n";
    return false;
  }

  if (result.skipped ||
      !result.success ||
      result.report.script_origin != "file" ||
      result.identity.file_path.empty())
  {
    std::cerr << "[FAIL] torch geometry contract case should execute from file\n";
    return false;
  }

  if (result.report.integration_name != "torch_geometry" ||
      result.report.flow_profile.script_style != "call_style" ||
      !result.report.ir_valid ||
      !result.report.layer_profile.bridge_exec_safe ||
      result.report.layer_profile.execution_text_kind != "compile_bridge" ||
      result.report.binding_semantics.binding_scope != "native_only" ||
      result.report.executed_task_count != 1)
  {
    std::cerr << "[FAIL] torch geometry contract execution metadata mismatch\n";
    return false;
  }

  if (request.case_id == "input_prior_contract" &&
      result.report.scalar_result <= 0.0)
  {
    std::cerr << "[FAIL] torch geometry input prior contract should produce a positive contract result\n";
    return false;
  }

  if (request.case_id == "label_align_contract" &&
      result.report.scalar_result <= 0.0)
  {
    std::cerr << "[FAIL] torch geometry label align contract should produce a positive contract result\n";
    return false;
  }

  if (request.case_id == "attach_back_contract" &&
      result.report.scalar_result <= 0.0)
  {
    std::cerr << "[FAIL] torch geometry attach contract should produce a positive contract result\n";
    return false;
  }

  if (request.case_id == "replay_contract" &&
      result.report.scalar_result <= 0.0)
  {
    std::cerr << "[FAIL] torch geometry replay contract should produce a positive contract result\n";
    return false;
  }

  return true;
}

bool RunCxcoreDefaultContractMainlineCase(const cxparser_ext::ParserDispatchRequest &request,
                                         const char *expected_result_object = 0,
                                         const char *expected_failure_mode = 0,
                                         const char *expected_summary_fragment = 0)
{
  cxparser_ext::ParserDispatchResult result;
  if (!cxparser_ext::RunDispatchRequest(request, result))
  {
    std::cerr << "[FAIL] cxcore default contract mainline dispatch failed for "
              << request.case_id << "\n";
    return false;
  }

  if (!result.success ||
      result.skipped ||
      result.report.script_origin != "file" ||
      result.report.layer_profile.execution_text_kind != "source" ||
      result.tick.accepted_task_count != 1 ||
      result.tick.executed_task_count != 1)
  {
    std::cerr << "[FAIL] cxcore default contract mainline should execute from file source\n";
    return false;
  }

  if (result.report.task_id.empty() ||
      result.report.result_object.empty() ||
      result.report.metrics.empty() ||
      result.report.summary.empty())
  {
    std::cerr << "[FAIL] cxcore default contract mainline should expose contract result fields\n";
    return false;
  }

  if (expected_result_object &&
      result.report.result_object != expected_result_object)
  {
    std::cerr << "[FAIL] cxcore contract result object mismatch for "
              << request.case_id
              << " actual=" << result.report.result_object
              << " expected=" << expected_result_object << "\n";
    return false;
  }

  if (expected_failure_mode &&
      result.report.failure_mode != expected_failure_mode)
  {
    std::cerr << "[FAIL] cxcore contract failure mode mismatch for "
              << request.case_id
              << " actual=" << result.report.failure_mode
              << " expected=" << expected_failure_mode << "\n";
    return false;
  }

  if (expected_summary_fragment &&
      result.report.summary.find(expected_summary_fragment) == std::string::npos)
  {
    std::cerr << "[FAIL] cxcore contract summary mismatch for "
              << request.case_id
              << " summary=" << result.report.summary
              << " missing=" << expected_summary_fragment << "\n";
    return false;
  }

  return true;
}

bool RunCxcoreContractCompileBridgeProbe()
{
  cxparser_ext::ParserDispatchRequest request;
  request.layer = "feature";
  request.module = "cxcore";
  request.case_id = "region_boundary_analysis_golden";
  request.mode = "build-run";
  request.route = "compile_bridge";

  cxparser_ext::ParserDispatchResult result;
  if (!cxparser_ext::RunDispatchRequest(request, result))
  {
    std::cerr << "[FAIL] cxcore compile bridge probe dispatch failed\n";
    return false;
  }

  if (!result.success ||
      result.skipped ||
      result.report.script_origin != "file" ||
      result.report.layer_profile.execution_text_kind != "compile_bridge" ||
      !result.report.layer_profile.bridge_exec_safe ||
      result.report.layer_profile.bridge_exec_subset != "cxcore_contract_call" ||
      result.tick.accepted_task_count != 1 ||
      result.tick.executed_task_count != 1 ||
      result.report.result_object != "ImageAnalysisOutput" ||
      result.report.failure_mode != "none" ||
      result.report.summary.find("compile bridge contract validated") == std::string::npos)
  {
    std::cerr << "[FAIL] cxcore compile bridge probe metadata mismatch\n";
    return false;
  }

  return true;
}

bool RunTorchContractMainlineCase(const char *layer,
                                  const char *case_id,
                                  const char *expected_metrics,
                                  const char *expected_summary_fragment)
{
  cxparser_ext::ParserDispatchRequest request;
  request.layer = layer;
  request.module = "torch_module";
  request.case_id = case_id;
  request.mode = "build-run";

  cxparser_ext::ParserDispatchResult result;
  if (!cxparser_ext::RunDispatchRequest(request, result))
  {
    std::cerr << "[FAIL] torch contract mainline dispatch failed for "
              << case_id << "\n";
    return false;
  }

  if (!result.success ||
      result.skipped ||
      result.report.script_origin != "file" ||
      result.report.layer_profile.execution_text_kind != "source" ||
      result.tick.accepted_task_count != 1 ||
      result.tick.executed_task_count != 1 ||
      result.report.result_object != "TorchStageReport" ||
      result.report.metrics != expected_metrics ||
      result.report.summary.find(expected_summary_fragment) == std::string::npos)
  {
    std::cerr << "[FAIL] torch contract mainline metadata mismatch for "
              << case_id << "\n";
    return false;
  }

  return true;
}

bool RunCxcoreDefaultContractMainlineMatrix()
{
  struct CaseExpectation
  {
    const char *case_id;
    const char *result_object;
    const char *failure_mode;
  };

  static const CaseExpectation cases[] = {
    {"line_measurement_golden", "LineMeasurementOutput", "none"},
    {"line_measurement_boundary", "LineMeasurementOutput", "handled_boundary_condition"},
    {"line_measurement_noise", "LineMeasurementOutput", "handled_noise_condition"},
    {"line_measurement_degenerate", "LineMeasurementOutput", "handled_degenerate_input"},
    {"circle_measurement_golden", "CircleMeasurementOutput", "none"},
    {"circle_measurement_boundary", "CircleMeasurementOutput", "handled_boundary_condition"},
    {"circle_measurement_noise", "CircleMeasurementOutput", "handled_noise_condition"},
    {"circle_measurement_degenerate", "CircleMeasurementOutput", "handled_degenerate_input"},
    {"template_feature_match_golden", "MatchOutput", "none"},
    {"template_feature_match_boundary", "MatchOutput", "handled_boundary_condition"},
    {"template_feature_match_noise", "MatchOutput", "handled_noise_condition"},
    {"template_feature_match_degenerate", "MatchOutput", "handled_degenerate_input"},
    {"region_boundary_analysis_golden", "ImageAnalysisOutput", "none"},
    {"region_boundary_analysis_boundary", "ImageAnalysisOutput", "handled_boundary_condition"},
    {"region_boundary_analysis_noise", "ImageAnalysisOutput", "handled_noise_condition"},
    {"region_boundary_analysis_degenerate", "ImageAnalysisOutput", "handled_degenerate_input"}
  };

  for (size_t index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index)
  {
    if (!RunCxcoreDefaultContractMainlineCase(MakeCxcoreFeatureRequest(cases[index].case_id),
                                              cases[index].result_object,
                                              cases[index].failure_mode,
                                              "task validated"))
      return false;
  }

  return true;
}
}

int main()
{
  cxparser_ext::ParserDispatchRequest cxcore_smoke;
  cxcore_smoke.layer = "smoke";
  cxcore_smoke.module = "cxcore";
  cxcore_smoke.case_id = "minimal_host";
  cxcore_smoke.mode = "build-run";

  cxparser_ext::ParserDispatchRequest cxcore_feature;
  cxcore_feature.layer = "feature";
  cxcore_feature.module = "cxcore";
  cxcore_feature.case_id = "image_probe_score";
  cxcore_feature.mode = "build-run";

  cxparser_ext::ParserDispatchRequest cxcore_flow_feature;
  cxcore_flow_feature.layer = "feature";
  cxcore_flow_feature.module = "cxcore";
  cxcore_flow_feature.case_id = "flow_numeric_bridge_v1";
  cxcore_flow_feature.mode = "build-run";

  cxparser_ext::ParserDispatchRequest cxcore_flow_safe_feature;
  cxcore_flow_safe_feature.layer = "feature";
  cxcore_flow_safe_feature.module = "cxcore";
  cxcore_flow_safe_feature.case_id = "flow_numeric_safe_bridge_v1";
  cxcore_flow_safe_feature.mode = "build-run";

  cxparser_ext::ParserDispatchRequest cxcore_object_flow_feature;
  cxcore_object_flow_feature.layer = "feature";
  cxcore_object_flow_feature.module = "cxcore";
  cxcore_object_flow_feature.case_id = "image_probe_flow_exec_v1";
  cxcore_object_flow_feature.mode = "build-run";

  cxparser_ext::ParserDispatchRequest rag_scenario;
  rag_scenario.layer = "scenario";
  rag_scenario.module = "rag";
  rag_scenario.case_id = "replay_probe";
  rag_scenario.mode = "build-run";

  cxparser_ext::ParserDispatchRequest rag_paired_replay;
  rag_paired_replay.script_type = "integration";
  rag_paired_replay.integration = "rag_torch";
  rag_paired_replay.layer = "scenario";
  rag_paired_replay.case_id = "paired_replay_transport_switch";
  rag_paired_replay.mode = "build-run";

  cxparser_ext::ParserDispatchRequest video_clang_bridge;
  video_clang_bridge.script_type = "integration";
  video_clang_bridge.integration = "video";
  video_clang_bridge.layer = "infer";
  video_clang_bridge.case_id = "video_frame_chain";
  video_clang_bridge.mode = "build-run";

  cxparser_ext::ParserDispatchRequest rag_parser_error;
  rag_parser_error.layer = "feature";
  rag_parser_error.module = "rag";
  rag_parser_error.case_id = "parser_error_probe";
  rag_parser_error.mode = "build-run";

  cxparser_ext::ParserDispatchRequest torch_smoke;
  torch_smoke.layer = "smoke";
  torch_smoke.module = "torch";
  torch_smoke.case_id = "test_host";
  torch_smoke.mode = "build-run";

  cxparser_ext::ParserDispatchRequest mlpack_feature;
  mlpack_feature.layer = "feature";
  mlpack_feature.module = "mlpack";
  mlpack_feature.case_id = "minimal_model";
  mlpack_feature.mode = "build-run";

  cxparser_ext::ParserDispatchRequest mlpack_minimal_train;
  mlpack_minimal_train.layer = "train";
  mlpack_minimal_train.module = "mlpack";
  mlpack_minimal_train.case_id = "minimal_train";
  mlpack_minimal_train.mode = "build-run";

  cxparser_ext::ParserDispatchRequest mlpack_minimal_infer;
  mlpack_minimal_infer.layer = "infer";
  mlpack_minimal_infer.module = "mlpack";
  mlpack_minimal_infer.case_id = "minimal_infer";
  mlpack_minimal_infer.mode = "build-run";

  cxparser_ext::ParserDispatchRequest mlpack_logreg_train;
  mlpack_logreg_train.layer = "train";
  mlpack_logreg_train.module = "mlpack";
  mlpack_logreg_train.case_id = "baseline_logreg_flow_min";
  mlpack_logreg_train.mode = "build-run";

  cxparser_ext::ParserDispatchRequest mlpack_logreg_infer;
  mlpack_logreg_infer.layer = "infer";
  mlpack_logreg_infer.module = "mlpack";
  mlpack_logreg_infer.case_id = "baseline_logreg_flow_min";
  mlpack_logreg_infer.mode = "build-run";

  cxparser_ext::ParserDispatchRequest mlpack_knn_train;
  mlpack_knn_train.layer = "train";
  mlpack_knn_train.module = "mlpack";
  mlpack_knn_train.case_id = "baseline_knn_flow_min";
  mlpack_knn_train.mode = "build-run";

  cxparser_ext::ParserDispatchRequest mlpack_knn_infer;
  mlpack_knn_infer.layer = "infer";
  mlpack_knn_infer.module = "mlpack";
  mlpack_knn_infer.case_id = "baseline_knn_flow_min";
  mlpack_knn_infer.mode = "build-run";

  cxparser_ext::ParserDispatchRequest mlpack_score;
  mlpack_score.layer = "score";
  mlpack_score.module = "mlpack";
  mlpack_score.case_id = "baseline_classification_flow_min";
  mlpack_score.mode = "build-run";

  cxparser_ext::ParserDispatchRequest mlpack_cluster_ref;
  mlpack_cluster_ref.layer = "score";
  mlpack_cluster_ref.module = "mlpack";
  mlpack_cluster_ref.case_id = "baseline_cluster_ref_min";
  mlpack_cluster_ref.mode = "build-run";

  cxparser_ext::ParserDispatchRequest mlpack_distance_ref;
  mlpack_distance_ref.layer = "score";
  mlpack_distance_ref.module = "mlpack";
  mlpack_distance_ref.case_id = "baseline_distance_ref_min";
  mlpack_distance_ref.mode = "build-run";

  cxparser_ext::ParserDispatchRequest mlpack_anomaly_ref;
  mlpack_anomaly_ref.layer = "score";
  mlpack_anomaly_ref.module = "mlpack";
  mlpack_anomaly_ref.case_id = "baseline_anomaly_ref_min";
  mlpack_anomaly_ref.mode = "build-run";

  cxparser_ext::ParserDispatchRequest mlpack_logreg_chain;
  mlpack_logreg_chain.script_type = "integration";
  mlpack_logreg_chain.layer = "scenario";
  mlpack_logreg_chain.integration = "mlpack";
  mlpack_logreg_chain.case_id = "baseline_logreg_chain_min";
  mlpack_logreg_chain.mode = "build-run";

  cxparser_ext::ParserDispatchRequest mlpack_knn_chain;
  mlpack_knn_chain.script_type = "integration";
  mlpack_knn_chain.layer = "scenario";
  mlpack_knn_chain.integration = "mlpack";
  mlpack_knn_chain.case_id = "baseline_knn_chain_min";
  mlpack_knn_chain.mode = "build-run";

  cxparser_ext::ParserDispatchRequest torch_geometry_input_prior;
  torch_geometry_input_prior.script_type = "integration";
  torch_geometry_input_prior.layer = "feature";
  torch_geometry_input_prior.integration = "torch_geometry";
  torch_geometry_input_prior.case_id = "input_prior_min";
  torch_geometry_input_prior.mode = "build-run";

  cxparser_ext::ParserDispatchRequest torch_geometry_label_align;
  torch_geometry_label_align.script_type = "integration";
  torch_geometry_label_align.layer = "feature";
  torch_geometry_label_align.integration = "torch_geometry";
  torch_geometry_label_align.case_id = "label_align_min";
  torch_geometry_label_align.mode = "build-run";

  cxparser_ext::ParserDispatchRequest torch_geometry_attach_back;
  torch_geometry_attach_back.script_type = "integration";
  torch_geometry_attach_back.layer = "feature";
  torch_geometry_attach_back.integration = "torch_geometry";
  torch_geometry_attach_back.case_id = "attach_back_min";
  torch_geometry_attach_back.mode = "build-run";

  cxparser_ext::ParserDispatchRequest torch_geometry_input_prior_contract;
  torch_geometry_input_prior_contract.script_type = "integration";
  torch_geometry_input_prior_contract.layer = "feature";
  torch_geometry_input_prior_contract.integration = "torch_geometry";
  torch_geometry_input_prior_contract.case_id = "input_prior_contract";
  torch_geometry_input_prior_contract.mode = "build-run";

  cxparser_ext::ParserDispatchRequest torch_geometry_label_align_contract;
  torch_geometry_label_align_contract.script_type = "integration";
  torch_geometry_label_align_contract.layer = "feature";
  torch_geometry_label_align_contract.integration = "torch_geometry";
  torch_geometry_label_align_contract.case_id = "label_align_contract";
  torch_geometry_label_align_contract.mode = "build-run";

  cxparser_ext::ParserDispatchRequest torch_geometry_attach_back_contract;
  torch_geometry_attach_back_contract.script_type = "integration";
  torch_geometry_attach_back_contract.layer = "feature";
  torch_geometry_attach_back_contract.integration = "torch_geometry";
  torch_geometry_attach_back_contract.case_id = "attach_back_contract";
  torch_geometry_attach_back_contract.mode = "build-run";

  cxparser_ext::ParserDispatchRequest torch_geometry_replay;
  torch_geometry_replay.script_type = "integration";
  torch_geometry_replay.layer = "infer";
  torch_geometry_replay.integration = "torch_geometry";
  torch_geometry_replay.case_id = "replay_min";
  torch_geometry_replay.mode = "build-run";

  cxparser_ext::ParserDispatchRequest torch_geometry_replay_contract;
  torch_geometry_replay_contract.script_type = "integration";
  torch_geometry_replay_contract.layer = "infer";
  torch_geometry_replay_contract.integration = "torch_geometry";
  torch_geometry_replay_contract.case_id = "replay_contract";
  torch_geometry_replay_contract.mode = "build-run";

  cxparser_ext::ParserDispatchRequest ensmallen_feature;
  ensmallen_feature.layer = "feature";
  ensmallen_feature.module = "ensmallen_layer";
  ensmallen_feature.case_id = "circle_param_opt";
  ensmallen_feature.mode = "build-run";

  cxparser_ext::ParserDispatchRequest ensmallen_ellipse_redirect;
  ensmallen_ellipse_redirect.layer = "feature";
  ensmallen_ellipse_redirect.module = "ensmallen_layer";
  ensmallen_ellipse_redirect.case_id = "ellipse_param_opt";
  ensmallen_ellipse_redirect.mode = "build-run";

  cxparser_ext::ParserDispatchRequest ensmallen_match_redirect;
  ensmallen_match_redirect.layer = "feature";
  ensmallen_match_redirect.module = "ensmallen_layer";
  ensmallen_match_redirect.case_id = "match_score_opt";
  ensmallen_match_redirect.mode = "build-run";

  cxparser_ext::ParserDispatchRequest ensmallen_geometry_tuning;
  ensmallen_geometry_tuning.layer = "feature";
  ensmallen_geometry_tuning.module = "ensmallen_layer";
  ensmallen_geometry_tuning.case_id = "geometry_fit_tuning";
  ensmallen_geometry_tuning.mode = "build-run";

  cxparser_ext::ParserDispatchRequest ensmallen_match_tuning;
  ensmallen_match_tuning.layer = "feature";
  ensmallen_match_tuning.module = "ensmallen_layer";
  ensmallen_match_tuning.case_id = "match_score_tuning";
  ensmallen_match_tuning.mode = "build-run";

  cxparser_ext::ParserDispatchRequest ensmallen_phase1_replay;
  ensmallen_phase1_replay.layer = "scenario";
  ensmallen_phase1_replay.module = "ensmallen_layer";
  ensmallen_phase1_replay.case_id = "phase1_param_replay";
  ensmallen_phase1_replay.mode = "build-run";

  cxparser_ext::ParserDispatchRequest ensmallen_phase1_train;
  ensmallen_phase1_train.layer = "train";
  ensmallen_phase1_train.module = "ensmallen_layer";
  ensmallen_phase1_train.case_id = "phase1_param_opt";
  ensmallen_phase1_train.mode = "build-run";

  cxparser_ext::ParserDispatchRequest ensmallen_phase1_infer;
  ensmallen_phase1_infer.layer = "infer";
  ensmallen_phase1_infer.module = "ensmallen_layer";
  ensmallen_phase1_infer.case_id = "phase1_param_eval";
  ensmallen_phase1_infer.mode = "build-run";

  cxparser_ext::ParserDispatchRequest rag_geometry_writeback;
  rag_geometry_writeback.layer = "feature";
  rag_geometry_writeback.module = "rag";
  rag_geometry_writeback.case_id = "geometry_fit_writeback";
  rag_geometry_writeback.mode = "build-run";

  cxparser_ext::ParserDispatchRequest rag_match_writeback;
  rag_match_writeback.layer = "feature";
  rag_match_writeback.module = "rag";
  rag_match_writeback.case_id = "match_score_writeback";
  rag_match_writeback.mode = "build-run";

  cxparser_ext::ParserDispatchRequest cxgeom_flow_smoke;
  cxgeom_flow_smoke.layer = "smoke";
  cxgeom_flow_smoke.module = "cxgeom";
  cxgeom_flow_smoke.case_id = "cxgeom_bulk_create_presentation_release";
  cxgeom_flow_smoke.mode = "build-run";

  cxparser_ext::ParserDispatchRequest cxcore_flow_scenario;
  cxcore_flow_scenario.layer = "scenario";
  cxcore_flow_scenario.module = "cxcore";
  cxcore_flow_scenario.case_id = "cxcore_mixed_scene_refresh_latency_watch";
  cxcore_flow_scenario.mode = "build-run";

  cxparser_ext::ParserDispatchRequest torch_module_train;
  torch_module_train.layer = "train";
  torch_module_train.module = "torch_module";
  torch_module_train.case_id = "torch_module_minimal_image_train";
  torch_module_train.mode = "build-run";

  if (!RunCase(cxcore_smoke) ||
      !RunCase(cxcore_feature) ||
      !RunCase(cxcore_flow_feature) ||
      !RunCase(cxcore_flow_safe_feature) ||
      !RunCase(cxcore_object_flow_feature) ||
      !RunCxcoreDefaultContractMainlineMatrix() ||
      !RunCxcoreContractCompileBridgeProbe() ||
      !RunTorchContractMainlineCase("feature",
                                    "mobilevit_roi_patch_class_label_contract",
                                    "roi_patch,class_label",
                                    "mobilevit roi patch + class label") ||
      !RunTorchContractMainlineCase("feature",
                                    "torch.resnet18.baseline.feature",
                                    "classifier_output_shape,p3_p4_p5_feature_shapes,baseline_feature_ref",
                                    "resnet18 feature baseline ready") ||
      !RunTorchContractMainlineCase("feature",
                                    "torch.resnet50.baseline.feature",
                                    "classifier_output_shape,p3_p4_p5_feature_shapes,baseline_feature_ref",
                                    "resnet50 feature baseline ready") ||
      !RunTorchContractMainlineCase("feature",
                                    "deeplab_region_tensor_mask_label_contract",
                                    "region_tensor,mask_label",
                                    "deeplab region tensor + mask label") ||
      !RunTorchContractMainlineCase("feature",
                                    "yolov8_image_window_bbox_class_targets_contract",
                                    "image_window,bbox_class_targets",
                                    "yolov8 image window + bbox/class targets") ||
      !RunTorchContractMainlineCase("infer",
                                    "torch.mobilevit.unified.infer",
                                    "roi_patch,class_label,baseline_class_ref,roi_crop_packet_ref,cluster_ref,distance_ref,anomaly_ref",
                                    "mobilevit unified infer ready") ||
      !RunTorchContractMainlineCase("infer",
                                    "torch.deeplab.unified.infer",
                                    "region_tensor,mask_label,baseline_feature_ref",
                                    "deeplab unified infer ready") ||
      !RunTorchContractMainlineCase("infer",
                                    "torch.resnet18.baseline.infer",
                                    "classifier_output_shape,baseline_class_ref",
                                    "resnet18 infer baseline ready") ||
      !RunTorchContractMainlineCase("infer",
                                    "torch.resnet50.baseline.infer",
                                    "classifier_output_shape,baseline_class_ref",
                                    "resnet50 infer baseline ready") ||
      !RunTorchContractMainlineCase("train",
                                    "torch.yolov8.mainline.train",
                                    "image_window,bbox_class_targets,trainer_lifecycle_summary,unified_mainline_summary",
                                    "yolo train mainline ready") ||
      !RunTorchContractMainlineCase("train",
                                    "torch.mobilevit.mainline.train",
                                    "roi_patch,class_label,trainer_lifecycle_summary,unified_mainline_summary",
                                    "mobilevit train mainline ready") ||
      !RunTorchContractMainlineCase("train",
                                    "torch.deeplab.mainline.train",
                                    "region_tensor,mask_label,segmentation_trainer_lifecycle_summary,segmentation_unified_summary",
                                    "deeplab train mainline ready") ||
      !RunTorchContractMainlineCase("scenario",
                                    "torch.yolo_mobilevit.infer.scenario",
                                    "bbox_candidates,roi_patch,class_label,attach_back,bbox_candidate_list_ref,roi_crop_packet_ref,cluster_ref,distance_ref,anomaly_ref",
                                    "yolo mobilevit infer scenario ready") ||
      !RunIfProbeCase() ||
      !RunCase(rag_scenario) ||
      !RunPairedReplayCase(rag_paired_replay) ||
      !RunPlannedVideoClangBridgeCase(video_clang_bridge) ||
      !RunFailureCase(rag_parser_error) ||
      !RunCase(torch_smoke) ||
      !RunActiveMlpackBaselineCase(mlpack_feature,
                                   "mlpack.feature.minimal_model") ||
      !RunActiveMlpackBaselineCase(mlpack_minimal_train,
                                   "mlpack.train.minimal_train") ||
      !RunActiveMlpackBaselineCase(mlpack_minimal_infer,
                                   "mlpack.infer.minimal_infer") ||
      !RunActiveMlpackBaselineCase(mlpack_logreg_train,
                                   "mlpack.train.baseline_train_logreg_min") ||
      !RunActiveMlpackBaselineCase(mlpack_logreg_infer,
                                   "mlpack.infer.baseline_infer_logreg_min") ||
      !RunActiveMlpackBaselineCase(mlpack_knn_train,
                                   "mlpack.train.baseline_knn_flow_min") ||
      !RunActiveMlpackBaselineCase(mlpack_knn_infer,
                                   "mlpack.infer.baseline_knn_flow_min") ||
      !RunActiveMlpackBaselineCase(mlpack_score,
                                   "mlpack.score.baseline_score_classification_min") ||
      !RunActiveMlpackBaselineCase(mlpack_cluster_ref,
                                   "mlpack.score.baseline_cluster_ref_min") ||
      !RunActiveMlpackBaselineCase(mlpack_distance_ref,
                                   "mlpack.score.baseline_distance_ref_min") ||
      !RunActiveMlpackBaselineCase(mlpack_anomaly_ref,
                                   "mlpack.score.baseline_anomaly_ref_min") ||
      !RunActiveMlpackBaselineCase(mlpack_logreg_chain,
                                   "mlpack.scenario.baseline_logreg_chain_min") ||
      !RunActiveMlpackBaselineCase(mlpack_knn_chain,
                                   "mlpack.scenario.baseline_knn_chain_min") ||
      !RunPlannedTorchGeometryCase(torch_geometry_input_prior) ||
      !RunPlannedTorchGeometryCase(torch_geometry_label_align) ||
      !RunPlannedTorchGeometryCase(torch_geometry_attach_back) ||
      !RunActiveTorchGeometryContractCase(torch_geometry_input_prior_contract) ||
      !RunActiveTorchGeometryContractCase(torch_geometry_label_align_contract) ||
      !RunActiveTorchGeometryContractCase(torch_geometry_attach_back_contract) ||
      !RunActiveTorchGeometryContractCase(torch_geometry_replay_contract) ||
      !RunPlannedTorchGeometryCase(torch_geometry_replay) ||
      !RunCase(ensmallen_feature) ||
      !RunCase(ensmallen_ellipse_redirect) ||
      !RunCase(ensmallen_match_redirect) ||
      !RunCase(ensmallen_geometry_tuning) ||
      !RunCase(ensmallen_match_tuning) ||
      !RunCase(ensmallen_phase1_replay) ||
      !RunCase(ensmallen_phase1_train) ||
      !RunCase(ensmallen_phase1_infer) ||
      !RunCase(rag_geometry_writeback) ||
      !RunCase(rag_match_writeback) ||
      !RunFlowOverlayCase(cxgeom_flow_smoke) ||
      !RunFlowOverlayCase(cxcore_flow_scenario) ||
      !RunFlowOverlayCase(torch_module_train))
  {
    return 1;
  }

  std::cout << "[PASS] dispatch_smoke cxcore+rag+paired_replay+ensmallen+torch_contract active torch+mlpack planned\n";
  return 0;
}