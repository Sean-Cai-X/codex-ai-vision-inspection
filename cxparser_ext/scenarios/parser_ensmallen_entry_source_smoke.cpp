#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

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

struct EntryScriptCase
{
  const char *relative_path;
  const char *must_contain_a;
  const char *must_contain_b;
  const char *must_contain_c;
  const char *must_contain_d;
  const char *must_contain_e;
  const char *must_contain_f;
  const char *must_contain_g;
  const char *must_contain_h;
  const char *must_contain_i;
  const char *must_contain_j;
  const char *must_contain_k;
  const char *must_contain_l;
  const char *must_contain_m;
  const char *must_contain_n;
  const char *must_contain_o;
  const char *must_contain_p;
};

bool ReadTextFile(const std::string &path,
                  std::string &text)
{
  std::ifstream input(path.c_str(), std::ios::in | std::ios::binary);
  if (!input.is_open())
    return false;

  std::ostringstream buffer;
  buffer << input.rdbuf();
  text = buffer.str();
  return true;
}

bool ContainsText(const std::string &text,
                  const char *pattern)
{
  return pattern == 0 || text.find(pattern) != std::string::npos;
}

bool RunCase(const EntryScriptCase &script_case)
{
  const std::string workspace_root = ResolveWorkspaceRoot();
  if (workspace_root.empty())
  {
    std::cerr << "[FAIL] workspace root is unavailable\n";
    return false;
  }

  const std::string script_path = workspace_root + "/" + script_case.relative_path;
  std::string script_text;
  if (!ReadTextFile(script_path, script_text))
  {
    std::cerr << "[FAIL] entry script load failed: " << script_path << "\n";
    return false;
  }

  if (!ContainsText(script_text, "flow=CxCoreFlowHost();"))
  {
    std::cerr << "[FAIL] entry script should use CxCoreFlowHost: "
              << script_path << "\n";
    return false;
  }

  if (!ContainsText(script_text, script_case.must_contain_a) ||
      !ContainsText(script_text, script_case.must_contain_b) ||
      !ContainsText(script_text, script_case.must_contain_c) ||
      !ContainsText(script_text, script_case.must_contain_d) ||
      !ContainsText(script_text, script_case.must_contain_e) ||
      !ContainsText(script_text, script_case.must_contain_f) ||
      !ContainsText(script_text, script_case.must_contain_g) ||
      !ContainsText(script_text, script_case.must_contain_h) ||
      !ContainsText(script_text, script_case.must_contain_i) ||
      !ContainsText(script_text, script_case.must_contain_j) ||
      !ContainsText(script_text, script_case.must_contain_k) ||
      !ContainsText(script_text, script_case.must_contain_l) ||
      !ContainsText(script_text, script_case.must_contain_m) ||
      !ContainsText(script_text, script_case.must_contain_n) ||
      !ContainsText(script_text, script_case.must_contain_o) ||
      !ContainsText(script_text, script_case.must_contain_p))
  {
    std::cerr << "[FAIL] expected entry markers missing: "
              << script_path << "\n";
    return false;
  }

  std::cout << "[PASS] entry=" << script_path << "\n";
  return true;
}
}

int main()
{
  const EntryScriptCase cases[] = {
    {
      "cxparser/rag_script_cases/cxcore/feature/ensmallen_layer_geometry_fit_tuning_feature.cxsc",
      "flow.set_feature(\"FormfitGeometryFitTuning\")",
      "flow.call(\"RunBaselineEval\")",
      "flow.call(\"RunGeometryFitTuning\")",
      "flow.call(\"CompareBaselineVsBest\")",
      "flow.expect_output(\"MeasuredOptimizeResult\")",
      "flow.expect_output(\"MeasuredCompareResult\")",
      "flow.expect_output(\"MeasuredReplayResult\")",
      "flow.call(\"BuildRagExplainPacket\")",
      "flow.expect_output(\"RagWritebackNote\")",
      "flow.expect_field(\"pass_level\")",
      "flow.expect_field(\"replay_log_path\")",
      "flow.check_trace_contains(\"ExportReplayLog\")",
      "flow.input_param(\"objective_ref\",\"torch.optimization.objective_ref\")",
      "flow.input_param(\"boundary_error_ref\",\"torch.optimization.boundary_error_ref\")",
      "flow.input_param(\"alignment_error_ref\",\"torch.optimization.alignment_error_ref\")",
      0
    },
    {
      "cxparser/rag_script_cases/cxcore/feature/ensmallen_layer_match_score_tuning_feature.cxsc",
      "flow.set_feature(\"FastMatchScoreTuning\")",
      "flow.input_dataset(\"dataset.cxcore.phase1.ensmallen\")",
      "flow.input_sample(\"template_match.template_scene\")",
      "flow.call(\"RunBaselineEval\")",
      "flow.call(\"RunMatchScoreTuning\")",
      "flow.call(\"CompareBaselineVsBest\")",
      "flow.expect_output(\"MeasuredOptimizeResult\")",
      "flow.expect_output(\"MeasuredCompareResult\")",
      "flow.expect_output(\"MeasuredReplayResult\")",
      "flow.call(\"BuildRagExplainPacket\")",
      "flow.expect_output(\"RagWritebackNote\")",
      "flow.expect_field(\"pass_level\")",
      "flow.expect_field(\"replay_log_path\")",
      "flow.input_artifact(\"template_image\",\"phase1.template_match.template_patch\")",
      "flow.input_param(\"objective_ref\",\"torch.optimization.objective_ref\")",
      "flow.input_param(\"threshold_ref\",\"torch.optimization.threshold_ref\")",
      "flow.input_param(\"crop_policy_ref\",\"torch.optimization.crop_policy_ref\")",
      0
    },
    {
      "cxparser/rag_script_cases/cxcore/feature/ensmallen_layer_circle_param_opt_feature.cxsc",
      "flow.set_feature(\"FormfitGeometryFitTuning\")",
      "flow.input_gt(\"circle_gt\"",
      "flow.call(\"RunBaselineEval\")",
      "flow.call(\"RunGeometryFitTuning\")",
      "flow.call(\"CompareBaselineVsBest\")",
      "flow.expect_output(\"MeasuredOptimizeResult\")",
      "flow.expect_output(\"MeasuredReplayResult\")",
      "flow.check_trace_contains(\"ExportReplayLog\")",
      "flow.check_scalar_le(\"best_objective\",\"baseline_objective\")",
      "flow.expect_field(\"replay_log_path\")",
      0,
      0,
      0,
      0,
      0,
      0
    },
    {
      "cxparser/rag_script_cases/cxcore/feature/ensmallen_layer_ellipse_param_opt_feature.cxsc",
      "flow.set_feature(\"FormfitGeometryFitTuning\")",
      "flow.input_gt(\"ellipse_gt\"",
      "flow.call(\"RunBaselineEval\")",
      "flow.call(\"RunGeometryFitTuning\")",
      "flow.call(\"CompareBaselineVsBest\")",
      "flow.expect_output(\"MeasuredOptimizeResult\")",
      "flow.expect_output(\"MeasuredReplayResult\")",
      "flow.check_trace_contains(\"ExportReplayLog\")",
      "flow.check_scalar_le(\"best_objective\",\"baseline_objective\")",
      "flow.expect_field(\"replay_log_path\")",
      0,
      0,
      0,
      0,
      0,
      0
    },
    {
      "cxparser/rag_script_cases/cxcore/feature/ensmallen_layer_match_score_opt_feature.cxsc",
      "flow.set_feature(\"FastMatchScoreTuning\")",
      "flow.input_gt(\"match_gt\"",
      "flow.call(\"RunBaselineEval\")",
      "flow.call(\"RunMatchScoreTuning\")",
      "flow.call(\"CompareBaselineVsBest\")",
      "flow.expect_output(\"MeasuredOptimizeResult\")",
      "flow.expect_output(\"MeasuredReplayResult\")",
      "flow.check_trace_contains(\"ExportReplayLog\")",
      "flow.check_scalar_le(\"best_objective\",\"baseline_objective\")",
      "flow.expect_field(\"replay_log_path\")",
      0,
      0,
      0,
      0,
      0,
      0
    },
    {
      "cxparser/rag_script_cases/cxcore/scenario/ensmallen_layer_phase1_param_replay_scenario.cxsc",
      "flow.set_feature(\"Phase1ParamReplayScenario\")",
      "flow.input_dataset(\"dataset.deeppcb.phase1.ensmallen\")",
      "flow.call(\"EnsmallenScenarioReplay\")",
      "flow.call(\"EnsmallenScenarioCompare\")",
      "flow.expect_output(\"MeasuredScenarioOptimizeResult\")",
      "flow.expect_output(\"MeasuredScenarioCompareResult\")",
      "flow.expect_output(\"MeasuredScenarioReplayResult\")",
      "flow.expect_field(\"sample_summaries\")",
      "flow.expect_field(\"pass_fail\")",
      "flow.expect_field(\"replay_log_path\")",
      "flow.check_count_ge(\"sample_summaries\",24)",
      "flow.check_trace_contains(\"EnsmallenScenarioReplay\")",
      "flow.check_trace_contains(\"EnsmallenScenarioCompare\")",
      "flow.input_artifact(\"bridge_manifest\",\"sample_bundle_draft.json\")",
      "flow.input_artifact(\"defect_count\",\"bridge_samples.tsv::defect_count\")",
      "flow.input_param(\"objective_ref\",\"torch.optimization.objective_ref\")",
      "flow.input_param(\"threshold_ref\",\"torch.optimization.threshold_ref\")",
      "flow.input_param(\"boundary_error_ref\",\"torch.optimization.boundary_error_ref\")",
      "flow.input_param(\"crop_policy_ref\",\"torch.optimization.crop_policy_ref\")",
      "flow.input_param(\"alignment_error_ref\",\"torch.optimization.alignment_error_ref\")",
      0
    },
    {
      "cxparser/rag_script_cases/cxcore/scenario/ensmallen_layer_halcon_circle_plate_geometry_replay_scenario.cxsc",
      "flow.set_feature(\"HalconCirclePlateGeometryReplayScenario\")",
      "flow.input_dataset(\"dataset.halcon_2605.thread_selection.ensmallen\")",
      "flow.input_sample(\"halcon_findcircle_circle_plate_01\")",
      "flow.call(\"ResolveHalconReviewBundle\")",
      "flow.call(\"EnsmallenCircleOptimize\",\"halcon_findcircle_circle_plate_01\")",
      "flow.call(\"EnsmallenScenarioReplay\")",
      "flow.call(\"EnsmallenScenarioCompare\")",
      "flow.expect_output(\"MeasuredScenarioOptimizeResult\")",
      "flow.expect_output(\"MeasuredScenarioCompareResult\")",
      "flow.expect_output(\"MeasuredScenarioReplayResult\")",
      "flow.expect_field(\"sample_summaries\")",
      "flow.expect_field(\"replay_log_path\")",
      "flow.expect_field(\"compare_ref\")",
      "flow.check_count_ge(\"sample_summaries\",2)",
      "flow.check_artifact_exists(\"scenario_compare.json\")",
      "flow.input_artifact(\"bridge_manifest\",\"halcon_ensmallen_review_bundle.json\")",
      "flow.input_param(\"boundary_error_ref\",\"torch.optimization.boundary_error_ref\")"
    },
    {
      "cxparser/rag_script_cases/cxcore/train/ensmallen_layer_phase1_param_opt_train.cxsc",
      "flow.set_feature(\"Phase1ParamOptimizeTrain\")",
      "flow.input_dataset(\"dataset.deeppcb.phase1.ensmallen\")",
      "flow.call(\"EnsmallenSaveBestParams\")",
      "flow.expect_output(\"MeasuredBatchOptimizeResult\")",
      "flow.expect_output(\"MeasuredReplayResult\")",
      "flow.expect_field(\"task_scope\")",
      "flow.expect_field(\"best_param_sets\")",
      "flow.expect_field(\"sample_count\")",
      "flow.expect_field(\"replay_log_path\")",
      "flow.expect_field(\"best_params_ref\")",
      "flow.expect_field(\"optimization_result_ref\")",
      "flow.expect_field(\"summary_ref\")",
      "flow.check_artifact_exists(\"batch_best_params.json\")",
      "flow.check_scalar_ge(\"sample_count\",24)",
      "flow.input_artifact(\"bridge_manifest\",\"sample_bundle_draft.json\")",
      "flow.input_artifact(\"defect_count\",\"bridge_samples.tsv::defect_count\")",
      "flow.input_param(\"objective_ref\",\"torch.optimization.objective_ref\")",
      "flow.input_param(\"threshold_ref\",\"torch.optimization.threshold_ref\")",
      "flow.input_param(\"boundary_error_ref\",\"torch.optimization.boundary_error_ref\")",
      "flow.input_param(\"crop_policy_ref\",\"torch.optimization.crop_policy_ref\")",
      "flow.input_param(\"alignment_error_ref\",\"torch.optimization.alignment_error_ref\")",
      0
    },
    {
      "cxparser/rag_script_cases/cxcore/train/ensmallen_layer_halcon_screws_cluster_stability_train.cxsc",
      "flow.set_feature(\"HalconScrewsClusterStabilityTrain\")",
      "flow.input_dataset(\"dataset.halcon_2605.thread_selection.ensmallen\")",
      "flow.input_split(\"halcon_cluster\")",
      "flow.call(\"ResolveHalconReviewBundle\")",
      "flow.call(\"EnsmallenMatchOptimize\",\"halcon_findcircle_screw_cluster_015\")",
      "flow.call(\"EnsmallenAggregateBestParams\")",
      "flow.call(\"EnsmallenSaveBestParams\")",
      "flow.expect_output(\"MeasuredBatchOptimizeResult\")",
      "flow.expect_output(\"MeasuredReplayResult\")",
      "flow.expect_field(\"best_param_sets\")",
      "flow.expect_field(\"sample_count\")",
      "flow.expect_field(\"best_params_ref\")",
      "flow.expect_field(\"summary_ref\")",
      "flow.check_scalar_eq(\"task_scope\",\"cluster_stability_only\")",
      "flow.check_scalar_ge(\"sample_count\",2)",
      "flow.check_artifact_exists(\"batch_best_params.json\")",
      "flow.check_artifact_exists(\"batch_summary.json\")"
    },
    {
      "cxparser/rag_script_cases/cxcore/infer/ensmallen_layer_phase1_param_eval_infer.cxsc",
      "flow.set_feature(\"Phase1ParamEvalInfer\")",
      "flow.input_dataset(\"dataset.deeppcb.phase1.ensmallen\")",
      "flow.call(\"EnsmallenInferCompare\")",
      "flow.expect_output(\"MeasuredOptimizeResult\")",
      "flow.expect_output(\"MeasuredInferCompareResult\")",
      "flow.expect_output(\"MeasuredReplayResult\")",
      "flow.expect_field(\"baseline_metrics\")",
      "flow.expect_field(\"optimized_metrics\")",
      "flow.expect_field(\"delta_metrics\")",
      "flow.expect_field(\"replay_log_path\")",
      "flow.expect_field(\"best_params_ref\")",
      "flow.expect_field(\"compare_ref\")",
      "flow.expect_field(\"optimization_result_ref\")",
      "flow.check_artifact_exists(\"baseline_report.json\")",
      "flow.input_artifact(\"bridge_manifest\",\"sample_bundle_draft.json\")",
      "flow.input_artifact(\"defect_count\",\"bridge_samples.tsv::defect_count\")",
      "flow.input_param(\"objective_ref\",\"torch.optimization.objective_ref\")",
      "flow.input_param(\"threshold_ref\",\"torch.optimization.threshold_ref\")",
      "flow.input_param(\"boundary_error_ref\",\"torch.optimization.boundary_error_ref\")",
      "flow.input_param(\"crop_policy_ref\",\"torch.optimization.crop_policy_ref\")",
      "flow.check_artifact_exists(\"optimized_report.json\")",
      "flow.input_param(\"alignment_error_ref\",\"torch.optimization.alignment_error_ref\")"
    },
    {
      "cxparser/rag_script_cases/cxcore/infer/ensmallen_layer_halcon_universal_joint_match_eval_infer.cxsc",
      "flow.set_feature(\"HalconUniversalJointMatchEvalInfer\")",
      "flow.input_dataset(\"dataset.halcon_2605.thread_selection.ensmallen\")",
      "flow.call(\"ResolveHalconReviewBundle\")",
      "flow.call(\"LoadEnsmallenBestParams\")",
      "flow.call(\"EnsmallenMatchInferEval\")",
      "flow.call(\"EnsmallenInferCompare\")",
      "flow.expect_output(\"MeasuredOptimizeResult\")",
      "flow.expect_output(\"MeasuredInferCompareResult\")",
      "flow.expect_output(\"MeasuredReplayResult\")",
      "flow.expect_field(\"baseline_metrics\")",
      "flow.expect_field(\"optimized_metrics\")",
      "flow.expect_field(\"delta_metrics\")",
      "flow.expect_field(\"replay_log_path\")",
      "flow.expect_field(\"compare_ref\")",
      "flow.check_artifact_exists(\"infer_compare.json\")",
      "flow.input_artifact(\"alignment_error_ref\",\"torch.optimization.alignment_error_ref\")",
      "flow.input_param(\"task_scope\",\"match_score_and_interaction\")"
    },
    {
      "cxparser/rag_script_cases/cxcore/infer/ensmallen_layer_halcon_pcb_focus_interaction_eval_infer.cxsc",
      "flow.set_feature(\"HalconPcbFocusInteractionEvalInfer\")",
      "flow.input_dataset(\"dataset.halcon_2605.thread_selection.ensmallen\")",
      "flow.call(\"ResolveHalconReviewBundle\")",
      "flow.call(\"LoadEnsmallenBestParams\")",
      "flow.call(\"EnsmallenMatchInferEval\")",
      "flow.call(\"EnsmallenInferCompare\")",
      "flow.expect_output(\"MeasuredOptimizeResult\")",
      "flow.expect_output(\"MeasuredInferCompareResult\")",
      "flow.expect_output(\"MeasuredReplayResult\")",
      "flow.expect_field(\"baseline_metrics\")",
      "flow.expect_field(\"optimized_metrics\")",
      "flow.expect_field(\"delta_metrics\")",
      "flow.expect_field(\"replay_log_path\")",
      "flow.expect_field(\"compare_ref\")",
      "flow.check_artifact_exists(\"infer_compare.json\")",
      "flow.input_artifact(\"template_image\",\"cximage_main_thread/pcb_focus/pcb_focus_telecentric_001.png\")",
      "flow.input_param(\"task_scope\",\"layer_interaction_eval\")"
    }
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
  {
    if (!RunCase(cases[i]))
      return 1;
  }

  std::cout << "[PASS] parser_ensmallen_entry_source_smoke\n";
  return 0;
}
