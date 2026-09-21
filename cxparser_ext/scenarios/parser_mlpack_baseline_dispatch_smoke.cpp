#include <iostream>

#include "../catalog/parser_case_catalog.h"
#include "../drivers/parser_dispatch_driver.h"
#include "../runtime/cxscript_runtime.h"

namespace
{
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
    std::cerr << "[DEBUG] status=" << result.status
              << " success=" << (result.success ? "true" : "false")
              << " skipped=" << (result.skipped ? "true" : "false") << "\n";
    if (!result.report.error_kind.empty() || !result.report.error_message.empty())
    {
      std::cerr << "[DEBUG] error_kind=" << result.report.error_kind
                << " error_message=" << result.report.error_message << "\n";
    }
    if (!result.identity.file_path.empty())
      std::cerr << "[DEBUG] script=" << result.identity.file_path << "\n";
    for (size_t i = 0; i < result.lines.size(); ++i)
      std::cerr << "[DEBUG] line[" << i << "] " << result.lines[i] << "\n";
    return false;
  }

  if (!result.success || result.skipped || result.status != "run_ok")
  {
    std::cerr << "[FAIL] mlpack baseline active dispatch should run for "
              << request.layer << "." << request.case_id << "\n";
    std::cerr << "[DEBUG] status=" << result.status
              << " success=" << (result.success ? "true" : "false")
              << " skipped=" << (result.skipped ? "true" : "false") << "\n";
    for (size_t i = 0; i < result.lines.size(); ++i)
      std::cerr << "[DEBUG] line[" << i << "] " << result.lines[i] << "\n";
    return false;
  }

  if (result.report.task_id != expected_task_id)
  {
    std::cerr << "[FAIL] mlpack baseline task id mismatch for "
              << request.layer << "." << request.case_id << "\n";
    std::cerr << "[DEBUG] expected_task_id=" << expected_task_id
              << " actual_task_id=" << result.report.task_id << "\n";
    return false;
  }

  if (!HasLineWithPrefix(result, "[PASS] module=mlpack") ||
      !HasLineWithPrefix(result, "[CONTRACT]") ||
      !HasLineWithPrefix(result, "[SUMMARY]"))
  {
    std::cerr << "[FAIL] mlpack baseline active dispatch missing contract lines\n";
    for (size_t i = 0; i < result.lines.size(); ++i)
      std::cerr << "[DEBUG] line[" << i << "] " << result.lines[i] << "\n";
    return false;
  }

  if (result.multimodal_slices.size() < 2 ||
      result.operation_atoms.size() < 4)
  {
    std::cerr << "[FAIL] mlpack baseline active dispatch should export multimodal slices and operation atoms\n";
    std::cerr << "[DEBUG] multimodal_slices=" << result.multimodal_slices.size()
              << " operation_atoms=" << result.operation_atoms.size() << "\n";
    return false;
  }

  return true;
}

bool RunPlannedMlpackBaselineCase(const cxparser_ext::ParserDispatchRequest &request)
{
  cxparser_ext::ParserDispatchCaseSpec spec;
  if (!cxparser_ext::ResolveDispatchCase(request, spec))
  {
    std::cerr << "[FAIL] mlpack baseline case not resolved for "
              << request.layer << "." << request.case_id << "\n";
    return false;
  }

  if (spec.active_runtime)
  {
    std::cerr << "[FAIL] mlpack baseline case should still be planned for "
              << request.layer << "." << request.case_id << "\n";
    return false;
  }

  return true;
}
}

int main()
{
  cxparser_ext::ParserDispatchRequest mlpack_feature;
  mlpack_feature.layer = "feature";
  mlpack_feature.module = "mlpack";
  mlpack_feature.case_id = "minimal_model";
  mlpack_feature.mode = "build-run";

  cxparser_ext::ParserDispatchRequest mlpack_baseline_feature_all;
  mlpack_baseline_feature_all.layer = "feature";
  mlpack_baseline_feature_all.module = "mlpack";
  mlpack_baseline_feature_all.case_id = "baseline_feature_all_v1";
  mlpack_baseline_feature_all.mode = "build-run";

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

  cxparser_ext::ParserDispatchRequest mlpack_rf_train;
  mlpack_rf_train.layer = "train";
  mlpack_rf_train.module = "mlpack";
  mlpack_rf_train.case_id = "baseline_rf_flow_min";
  mlpack_rf_train.mode = "build-run";

  cxparser_ext::ParserDispatchRequest mlpack_rf_infer;
  mlpack_rf_infer.layer = "infer";
  mlpack_rf_infer.module = "mlpack";
  mlpack_rf_infer.case_id = "baseline_rf_flow_min";
  mlpack_rf_infer.mode = "build-run";

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

  cxparser_ext::ParserDispatchRequest mlpack_pair_compare;
  mlpack_pair_compare.script_type = "integration";
  mlpack_pair_compare.layer = "scenario";
  mlpack_pair_compare.integration = "mlpack";
  mlpack_pair_compare.case_id = "baseline_pair_compare_min";
  mlpack_pair_compare.mode = "build-run";

  if (!RunActiveMlpackBaselineCase(mlpack_feature,
                                   "mlpack.feature.minimal_model") ||
      !RunActiveMlpackBaselineCase(mlpack_baseline_feature_all,
                                   "mlpack.feature.baseline_feature_all_v1") ||
      !RunActiveMlpackBaselineCase(mlpack_minimal_train,
                                   "mlpack.train.minimal_train") ||
      !RunActiveMlpackBaselineCase(mlpack_minimal_infer,
                                   "mlpack.infer.minimal_infer") ||
      !RunActiveMlpackBaselineCase(mlpack_logreg_train,
                                   "mlpack.train.baseline_logreg_flow_min") ||
      !RunActiveMlpackBaselineCase(mlpack_logreg_infer,
                                   "mlpack.infer.baseline_logreg_flow_min") ||
      !RunActiveMlpackBaselineCase(mlpack_knn_train,
                                   "mlpack.train.baseline_knn_flow_min") ||
      !RunActiveMlpackBaselineCase(mlpack_knn_infer,
                                   "mlpack.infer.baseline_knn_flow_min") ||
      !RunActiveMlpackBaselineCase(mlpack_score,
                                   "mlpack.score.baseline_classification_flow_min") ||
      !RunActiveMlpackBaselineCase(mlpack_cluster_ref,
                                   "mlpack.score.baseline_cluster_ref_min") ||
      !RunActiveMlpackBaselineCase(mlpack_distance_ref,
                                   "mlpack.score.baseline_distance_ref_min") ||
      !RunActiveMlpackBaselineCase(mlpack_anomaly_ref,
                                   "mlpack.score.baseline_anomaly_ref_min") ||
      !RunActiveMlpackBaselineCase(mlpack_rf_train,
                                   "mlpack.train.baseline_rf_flow_min") ||
      !RunActiveMlpackBaselineCase(mlpack_rf_infer,
                                   "mlpack.infer.baseline_rf_flow_min") ||
      !RunActiveMlpackBaselineCase(mlpack_logreg_chain,
                                   "mlpack.scenario.baseline_logreg_chain_min") ||
      !RunActiveMlpackBaselineCase(mlpack_knn_chain,
                                   "mlpack.scenario.baseline_knn_chain_min") ||
      !RunActiveMlpackBaselineCase(mlpack_pair_compare,
                                   "mlpack.scenario.baseline_pair_compare_min"))
    return 1;

  std::cout << "[PASS] parser_mlpack_baseline_dispatch_smoke\n";
  return 0;
}
