#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "../runtime/cxscript_runtime.h"

namespace
{
struct SourceScriptCase
{
  const char *script_type;
  const char *module_name;
  const char *integration_name;
  const char *layer;
  const char *case_id;
  const char *must_contain_raw;
  const char *must_contain_a;
  const char *must_contain_b;
  bool require_flow_style;
};

bool ContainsText(const std::string &text,
                  const char *pattern)
{
  return pattern != 0 && text.find(pattern) != std::string::npos;
}

bool LoadRawText(const std::string &path,
                 std::string &text)
{
  std::ifstream input(path.c_str(), std::ios::in | std::ios::binary);
  if (!input)
    return false;

  std::ostringstream buffer;
  buffer << input.rdbuf();
  text = buffer.str();
  return true;
}

bool RunCase(const SourceScriptCase &script_case)
{
  cxparser_ext::CxscriptExecutionArgs args;
  args.script_type = script_case.script_type;
  args.module_name = script_case.module_name != 0 ? script_case.module_name : "";
  args.integration_name = script_case.integration_name != 0 ? script_case.integration_name : "";
  args.layer = script_case.layer;
  args.case_id = script_case.case_id;

  cxparser_ext::CxscriptIdentity identity;
  if (!cxparser_ext::BuildCxscriptIdentity(args, identity))
  {
    std::cerr << "[FAIL] identity build failed for "
              << args.script_type << "." << args.layer << "." << args.case_id << "\n";
    return false;
  }

  std::string script_text;
  std::string script_origin;
  if (!cxparser_ext::LoadCxscriptText(identity, "", script_text, script_origin))
  {
    std::cerr << "[FAIL] script load failed for "
              << args.script_type << "." << args.layer << "." << args.case_id << "\n";
    return false;
  }

  if (script_origin != "file")
  {
    std::cerr << "[FAIL] script origin should be file for " << args.case_id << "\n";
    return false;
  }

  std::string raw_text;
  if (!LoadRawText(identity.file_path, raw_text))
  {
    std::cerr << "[FAIL] raw file load failed for " << args.case_id
              << " path=" << identity.file_path << "\n";
    return false;
  }

  if (!ContainsText(raw_text, script_case.must_contain_raw))
  {
    std::cerr << "[FAIL] raw producer marker mismatch in " << args.case_id << "\n";
    return false;
  }

  if (!ContainsText(script_text, script_case.must_contain_a) ||
      !ContainsText(script_text, script_case.must_contain_b))
  {
    std::cerr << "[FAIL] missing required markers in " << args.case_id << "\n";
    return false;
  }

  cxparser_ext::CxscriptExecutionResult result;
  cxparser_ext::BuildCxscriptStructureSummary(script_text, result);

  if (script_case.require_flow_style &&
      (result.flow_profile.script_style != "flow_style" ||
       !result.flow_profile.has_prepare ||
       !result.flow_profile.has_action ||
       !result.flow_profile.has_check ||
       !result.flow_profile.has_report))
  {
    std::cerr << "[FAIL] flow stages incomplete for " << args.case_id
              << " style=" << result.flow_profile.script_style
              << " prepare=" << result.flow_profile.has_prepare
              << " action=" << result.flow_profile.has_action
              << " check=" << result.flow_profile.has_check
              << " report=" << result.flow_profile.has_report << "\n";
    return false;
  }

  if (!result.layer_profile.has_source_text || !result.layer_profile.has_normalized_text)
  {
    std::cerr << "[FAIL] layer profile incomplete for " << args.case_id << "\n";
    return false;
  }

  std::cout << "[PASS] case=" << args.case_id
            << " type=" << args.script_type
            << " layer=" << args.layer
            << " style=" << result.flow_profile.script_style
            << " prepare=" << result.flow_profile.has_prepare
            << " action=" << result.flow_profile.has_action
            << " check=" << result.flow_profile.has_check
            << " report=" << result.flow_profile.has_report
            << " ir_valid=" << result.ir_valid
            << "\n";
  return true;
}
}

int main()
{
  const SourceScriptCase cases[] = {
    {"module", "mlpack", 0, "train", "baseline_logreg_flow_min",
     "case baseline_logreg_flow_min;", "model_name = \"LogisticRegression\"",
     "feature_set = \"all_v1\"", true},
    {"module", "mlpack", 0, "infer", "baseline_logreg_flow_min",
     "case baseline_logreg_flow_min;", "model_path = \"artifacts/baseline/logreg_all_v1.bin\"",
     "action infer_model;", true},
    {"module", "mlpack", 0, "train", "baseline_knn_flow_min",
     "case baseline_knn_flow_min;", "model_name = \"kNN\"",
     "feature_set = \"all_v1\"", true},
    {"module", "mlpack", 0, "infer", "baseline_knn_flow_min",
     "case baseline_knn_flow_min;", "model_path = \"artifacts/baseline/knn_all_v1.bin\"",
     "action infer_model;", true},
    {"module", "mlpack", 0, "feature", "baseline_feature_all_v1",
     "case=baseline_feature_all_v1", "case=baseline_feature_all_v1", "layer=feature", false},
    {"module", "mlpack", 0, "train", "baseline_rf_flow_min",
     "case baseline_rf_flow_min;", "model_name = \"RandomForest\"",
     "feature_set = \"all_v1\"", true},
    {"module", "mlpack", 0, "infer", "baseline_rf_flow_min",
     "case baseline_rf_flow_min;", "model_path = \"artifacts/baseline/rf_all_v1.bin\"",
     "action infer_model;", true},
    {"module", "mlpack", 0, "score", "baseline_classification_flow_min",
     "case baseline_classification_flow_min;", "action score_classification;",
     "summary = \"baseline_score_ready\"", true},
    {"module", "mlpack", 0, "score", "baseline_cluster_ref_min",
     "# semantic_ref_kind=cluster_ref", "case=baseline_cluster_ref_min", "layer=score", false},
    {"module", "mlpack", 0, "score", "baseline_distance_ref_min",
     "# semantic_ref_kind=distance_ref", "case=baseline_distance_ref_min", "layer=score", false},
    {"module", "mlpack", 0, "score", "baseline_anomaly_ref_min",
     "# semantic_ref_kind=anomaly_ref", "case=baseline_anomaly_ref_min", "layer=score", false},
    {"integration", 0, "mlpack", "scenario", "baseline_logreg_chain_min",
     "case baseline_logreg_chain_min;", "call mlpack.train.baseline_logreg_flow_min;",
     "summary = \"baseline_logreg_chain_ready\"", true},
    {"integration", 0, "mlpack", "scenario", "baseline_knn_chain_min",
     "case baseline_knn_chain_min;", "call mlpack.train.baseline_knn_flow_min;",
     "summary = \"baseline_knn_chain_ready\"", true},
    {"integration", 0, "mlpack", "scenario", "baseline_pair_compare_min",
     "case baseline_pair_compare_min;", "call mlpack.train.baseline_logreg_flow_min;",
     "summary = \"baseline_pair_compare_ready\"", true}
  };

  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
  {
    if (!RunCase(cases[i]))
      return 1;
  }

  std::cout << "[PASS] parser_mlpack_baseline_cxscript_source_smoke\n";
  return 0;
}
