#include <iostream>
#include <string>

#include "../pipeline/parser_cxscript_runtime.h"

namespace
{
std::string ParentPath(const std::string &path)
{
  const std::string::size_type pos = path.find_last_of("/\\");
  if (pos == std::string::npos)
    return std::string();
  return path.substr(0, pos);
}

std::string RepoRoot()
{
  std::string path = __FILE__;
  path = ParentPath(path);
  path = ParentPath(path);
  path = ParentPath(path);
  return path;
}

bool HasExactDetail(const cxparser_ext::CxScriptExecutionResult &result,
                    const std::string &value)
{
  for (size_t i = 0; i < result.details.size(); ++i)
  {
    if (result.details[i] == value)
      return true;
  }
  return false;
}

bool HasNamedResultField(const cxparser_ext::CxScriptExecutionResult &result,
                         const std::string &result_name,
                         const std::string &field_name,
                         const std::string &value)
{
  for (size_t i = 0; i < result.result_fields.size(); ++i)
  {
    if (result.result_fields[i].result_name == result_name &&
        result.result_fields[i].field_name == field_name &&
        result.result_fields[i].value == value)
      return true;
  }
  return false;
}

struct RuntimeCase
{
  const char *label;
  const char *relative_path;
  const char *expected_summary;
  const char *expected_result_object;
  const char *expected_model_path;
  const char *expected_predictions_csv;
  const char *expected_output_summary_csv;
  const char *expected_detail;
  bool expect_train_ok;
  bool expect_infer_ok;
  bool expect_score_ok;
  bool expect_feature_bundle;
  bool expect_scalar_equals_feature_dim;
  bool expect_prediction_count_positive;
  bool expect_baseline_class_ref;
  const char *expected_cluster_ref;
  const char *expected_distance_ref;
  const char *expected_anomaly_ref;
};

bool RunRuntimeCase(const std::string &repo_root,
                    const RuntimeCase &runtime_case)
{
  cxparser_ext::ParserCxScriptRuntime runtime;
  cxparser_ext::CxScriptExecutionResult result;
  const std::string path = repo_root + runtime_case.relative_path;
  if (!runtime.ExecuteScriptFile(path, result))
  {
    std::cerr << "[FAIL] " << runtime_case.label
              << " summary=" << result.summary << "\n";
    return false;
  }

  if (result.summary != runtime_case.expected_summary)
  {
    std::cerr << "[FAIL] " << runtime_case.label << " summary mismatch\n";
    return false;
  }

  if (runtime_case.expect_feature_bundle)
  {
    if (result.result_object != runtime_case.expected_result_object ||
        result.feature_dim <= 0.0 ||
        result.scalar_result != result.feature_dim)
    {
      std::cerr << "[FAIL] " << runtime_case.label << " feature contract mismatch\n";
      return false;
    }
  }

  if (runtime_case.expect_train_ok)
  {
    if (!result.train_ok ||
        result.fit_time_ms < 0.0 ||
        result.feature_dim != 50.0 ||
        result.model_path != runtime_case.expected_model_path ||
        !HasExactDetail(result, runtime_case.expected_detail))
    {
      std::cerr << "[FAIL] " << runtime_case.label << " train contract mismatch\n";
      return false;
    }
  }

  if (runtime_case.expect_infer_ok)
  {
    if (!result.infer_ok ||
        result.infer_time_ms < 0.0 ||
        !runtime_case.expect_prediction_count_positive ||
        result.prediction_count <= 0.0 ||
        result.predictions_csv != runtime_case.expected_predictions_csv ||
        (runtime_case.expect_baseline_class_ref &&
         !HasNamedResultField(result, "refs", "baseline_class_ref",
                              runtime_case.expected_predictions_csv)) ||
        !HasExactDetail(result, runtime_case.expected_detail))
    {
      std::cerr << "[FAIL] " << runtime_case.label << " infer contract mismatch\n";
      return false;
    }
  }

  if (runtime_case.expect_score_ok)
  {
    if (!result.score_ok ||
        result.accuracy < 0.0 ||
        result.macro_f1 < 0.0 ||
        result.output_summary_csv != runtime_case.expected_output_summary_csv ||
        !HasExactDetail(result, "[MLPACK_CALL] score_classification") ||
        !HasExactDetail(result, runtime_case.expected_detail))
    {
      std::cerr << "[FAIL] " << runtime_case.label << " score contract mismatch\n";
      return false;
    }
  }

  if (runtime_case.expected_cluster_ref != 0 &&
      !HasNamedResultField(result, "refs", "cluster_ref",
                           runtime_case.expected_cluster_ref))
  {
    std::cerr << "[FAIL] " << runtime_case.label << " cluster ref mismatch\n";
    return false;
  }

  if (runtime_case.expected_distance_ref != 0 &&
      !HasNamedResultField(result, "refs", "distance_ref",
                           runtime_case.expected_distance_ref))
  {
    std::cerr << "[FAIL] " << runtime_case.label << " distance ref mismatch\n";
    return false;
  }

  if (runtime_case.expected_anomaly_ref != 0 &&
      !HasNamedResultField(result, "refs", "anomaly_ref",
                           runtime_case.expected_anomaly_ref))
  {
    std::cerr << "[FAIL] " << runtime_case.label << " anomaly ref mismatch\n";
    return false;
  }

  if (runtime_case.expect_scalar_equals_feature_dim &&
      result.scalar_result != result.feature_dim)
  {
    std::cerr << "[FAIL] " << runtime_case.label << " scalar_result mismatch\n";
    return false;
  }

  return true;
}
}

int main()
{
  const std::string repo_root = RepoRoot();
  const RuntimeCase cases[] = {
    {"feature",
     "/cxscript/module/mlpack/feature.baseline_feature_all_v1.cxs",
     "baseline_feature_ready",
     "BaselineFeatureBundle",
     0,
     0,
     0,
     "[MLPACK_CONTRACT] baseline_feature_all_v1",
     false,
     false,
     false,
     true,
     true,
     false,
     false,
     0,
     0,
     0},
    {"logreg_train",
     "/cxscript/module/mlpack/train/baseline_logreg_flow_min.cxscript",
     "baseline_logreg_train_ready",
     0,
     "artifacts/baseline/logreg_all_v1.bin",
     0,
     0,
     "[MLPACK_CALL] train_model",
     true,
     false,
     false,
     false,
     false,
     false,
     false,
     0,
     0,
     0},
    {"logreg_infer",
     "/cxscript/module/mlpack/infer/baseline_logreg_flow_min.cxscript",
     "baseline_logreg_infer_ready",
     0,
     0,
     "artifacts/baseline/logreg_all_v1_predictions.csv",
     0,
     "[MLPACK_CALL] infer_model",
     false,
     true,
     false,
     false,
     false,
     true,
     true,
     0,
     0,
     0},
    {"knn_train",
     "/cxscript/module/mlpack/train/baseline_knn_flow_min.cxscript",
     "baseline_knn_train_ready",
     0,
     "artifacts/baseline/knn_all_v1.bin",
     0,
     0,
     "[MLPACK_CALL] train_model",
     true,
     false,
     false,
     false,
     false,
     false,
     false,
     0,
     0,
     0},
    {"knn_infer",
     "/cxscript/module/mlpack/infer/baseline_knn_flow_min.cxscript",
     "baseline_knn_infer_ready",
     0,
     0,
     "artifacts/baseline/knn_all_v1_predictions.csv",
     0,
     "[MLPACK_CALL] infer_model",
     false,
     true,
     false,
     false,
     false,
     true,
     true,
     0,
     0,
     0},
    {"rf_train",
     "/cxscript/module/mlpack/train/baseline_rf_flow_min.cxscript",
     "baseline_rf_train_ready",
     0,
     "artifacts/baseline/rf_all_v1.bin",
     0,
     0,
     "[MLPACK_CALL] train_model",
     true,
     false,
     false,
     false,
     false,
     false,
     false,
     0,
     0,
     0},
    {"rf_infer",
     "/cxscript/module/mlpack/infer/baseline_rf_flow_min.cxscript",
     "baseline_rf_infer_ready",
     0,
     0,
     "artifacts/baseline/rf_all_v1_predictions.csv",
     0,
     "[MLPACK_CALL] infer_model",
     false,
     true,
     false,
     false,
     false,
     true,
     true,
     0,
     0,
     0},
    {"score",
     "/cxscript/module/mlpack/score/baseline_classification_flow_min.cxscript",
     "baseline_score_ready",
     0,
     0,
     0,
     "artifacts/baseline/baseline_summary.csv",
     "[MLPACK_CALL] write_summary",
     false,
     false,
     true,
     false,
     false,
     false,
     false,
     0,
     0,
     0},
    {"cluster_ref",
     "/cxscript/module/mlpack/score.baseline_cluster_ref_min.cxs",
     "baseline_cluster_ref_ready",
     "MlpackSemanticRefBundle",
     0,
     0,
     0,
     "[MLPACK_CONTRACT] baseline_cluster_ref_min",
     false,
     false,
     false,
     false,
     false,
     false,
     false,
     "artifacts/semantic/cluster_all_v1.json",
     0,
     0},
    {"distance_ref",
     "/cxscript/module/mlpack/score.baseline_distance_ref_min.cxs",
     "baseline_distance_ref_ready",
     "MlpackSemanticRefBundle",
     0,
     0,
     0,
     "[MLPACK_CONTRACT] baseline_distance_ref_min",
     false,
     false,
     false,
     false,
     false,
     false,
     false,
     0,
     "artifacts/semantic/distance_all_v1.json",
     0},
    {"anomaly_ref",
     "/cxscript/module/mlpack/score.baseline_anomaly_ref_min.cxs",
     "baseline_anomaly_ref_ready",
     "MlpackSemanticRefBundle",
     0,
     0,
     0,
     "[MLPACK_CONTRACT] baseline_anomaly_ref_min",
     false,
     false,
     false,
     false,
     false,
     false,
     false,
     0,
     0,
     "artifacts/semantic/anomaly_all_v1.json"}
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
  {
    if (!RunRuntimeCase(repo_root, cases[i]))
      return 1;
  }

  std::cout << "[PASS] parser_mlpack_baseline_pipeline_runtime_smoke\n";
  return 0;
}
