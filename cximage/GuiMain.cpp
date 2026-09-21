#include "CxAutomaticDiagnosticClosure.h"
#include "CxBusinessWorkflowRuntime.h"
#include "CxCrashLogHandler.h"
#include "CxEvidenceSelfTestRuntime.h"
#include "CxGeometryReferenceEvaluator.h"
#include "CxGeometryIncrementalAutoTune.h"
#include "CxGeometryYoloMaskPackage.h"
#include "CxMaskDiagnosticSelfTest.h"
#include "CxParamProbeRunner.h"
#include "CxParamRegressionRuntime.h"
#include "CxParserRuntimeOwner.h"
#include "CxPredictiveGeometryGate.h"
#include "CxScriptCatalogRuntime.h"
#include "CxScriptEvidenceChainRuntime.h"
#include "CxScriptGeometryFrameProbe.h"
#include "CxScriptImageManifestRuntime.h"
#include "CxScriptSuiteRunner.h"
#include "CxScriptSuiteRuntime.h"
#include "CxShapeInteractionTest.h"
#include "CxTorchRuntimeService.h"
#include "CxTypedLabelProposalGenerator.h"
#include "CxUnifiedLog.h"
#include "CxUnifiedLogOptions.h"
#include "CxUnifiedLogStreamBuf.h"
#include "CxYoloV8nGeometryAssociation.h"
#include "CxYoloV8nPairedInference.h"
#include "CxYoloV8nTrainingLifecycle.h"
#include "Main.h"
#include "ManualConsoleUtils.h"
#include "ManualStateTestConsole.h"
#include "ViewController.h"
#include "gwy_reference/CxExternalGwyReferenceBackend.h"
#include "metrology_analytics/CxMetrologyAnalyticsSmoke.h"
#include "metrology_analytics/CxSurfaceBasicStats.h"
#include "metrology_analytics/CxSurfaceField.h"

#if defined(CXVISION_ENABLE_LEGACY_STAGE25_CPP)
#include "CxScriptStage25Runner.h"
#endif

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>

#include <sstream>
#include <vector>

int RunModelLineageAssetScanSmoke(const std::string &scanRoot,
                                  const std::string &outputDirectory);
int RunModelLineageOperationSelfTest(const std::string &scanRoot,
                                     const std::string &outputDirectory);

namespace {
int RunEvidenceConfigurationProjectionSmokeCli(int argc, char **argv);

bool HasCliArg(int argc, char **argv, const std::string &name) {
  for (int i = 1; i < argc; ++i) {
    if (argv[i] != nullptr && name == argv[i])
      return true;
  }
  return false;
}

bool TryGetCliValue(int argc, char **argv, const std::string &name,
                    std::string &value) {
  const std::string prefix = name + "=";
  for (int i = 1; i < argc; ++i) {
    if (argv[i] == nullptr)
      continue;
    const std::string arg = argv[i];
    if (arg == name) {
      if (i + 1 < argc && argv[i + 1] != nullptr) {
        value = argv[i + 1];
        return true;
      }
      value.clear();
      return true;
    }
    if (arg.rfind(prefix, 0) == 0) {
      value = arg.substr(prefix.size());
      return true;
    }
  }
  return false;
}

std::string CliValueAfter(int argc, char **argv, const std::string &name) {
  for (int i = 1; i + 1 < argc; ++i) {
    if (argv[i] != nullptr && name == argv[i] && argv[i + 1] != nullptr)
      return argv[i + 1];
  }
  return std::string();
}

std::string PipelineJsonEscape(const std::string &text) {
  std::string out;
  for (char c : text) {
    switch (c) {
    case '\\':
      out += "\\\\";
      break;
    case '"':
      out += "\\\"";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      out += c;
      break;
    }
  }
  return out;
}

struct EvidenceLockPipelineCheck {
  std::string case_id;
  std::string expected;
  std::string actual;
  std::string conclusion;
  std::string reason;
  std::string report_path;
};

bool PipelineHasStep(const CxEvidenceSelfTestResult &result,
                     const std::string &code,
                     const std::string &status = std::string()) {
  for (const auto &step : result.steps) {
    if (step.code == code && (status.empty() || step.status == status))
      return true;
  }
  return false;
}

std::string PipelineReasonForStep(const CxEvidenceSelfTestResult &result,
                                  const std::string &code) {
  for (const auto &step : result.steps) {
    if (step.code == code)
      return step.reason;
  }
  return result.final_reason;
}

bool PipelineParamInt(const std::string &parameterSummary,
                      const std::string &key, int &value) {
  const std::string pattern = key + "=";
  const std::size_t pos = parameterSummary.find(pattern);
  if (pos == std::string::npos)
    return false;

  std::size_t begin = pos + pattern.size();
  std::size_t end = begin;
  while (end < parameterSummary.size() &&
         (std::isdigit(static_cast<unsigned char>(parameterSummary[end])) ||
          parameterSummary[end] == '-' || parameterSummary[end] == '+')) {
    ++end;
  }

  if (end == begin)
    return false;

  try {
    value = std::stoi(parameterSummary.substr(begin, end - begin));
    return true;
  } catch (...) {
    return false;
  }
}

bool PipelineFastMatchSearchContainsLearn(const std::string &parameterSummary,
                                          std::string &reason) {
  int learnX = 0;
  int learnY = 0;
  int learnW = 0;
  int learnH = 0;
  int searchX = 0;
  int searchY = 0;
  int searchW = 0;
  int searchH = 0;

  const bool ok = PipelineParamInt(parameterSummary, "learn_roi_x", learnX) &&
                  PipelineParamInt(parameterSummary, "learn_roi_y", learnY) &&
                  PipelineParamInt(parameterSummary, "learn_roi_w", learnW) &&
                  PipelineParamInt(parameterSummary, "learn_roi_h", learnH) &&
                  PipelineParamInt(parameterSummary, "search_roi_x", searchX) &&
                  PipelineParamInt(parameterSummary, "search_roi_y", searchY) &&
                  PipelineParamInt(parameterSummary, "search_roi_w", searchW) &&
                  PipelineParamInt(parameterSummary, "search_roi_h", searchH);

  if (!ok) {
    reason = "FastMatch locked params missing learn/search ROI fields";
    return false;
  }

  if (learnW <= 0 || learnH <= 0 || searchW <= 0 || searchH <= 0) {
    reason = "FastMatch learn/search ROI width and height must be positive";
    return false;
  }

  if (searchW <= learnW || searchH <= learnH) {
    reason = "FastMatch search ROI must be larger than learn ROI: learn=" +
             std::to_string(learnW) + "x" + std::to_string(learnH) +
             ", search=" + std::to_string(searchW) + "x" +
             std::to_string(searchH);
    return false;
  }

  if (searchX > learnX || searchY > learnY ||
      searchX + searchW < learnX + learnW ||
      searchY + searchH < learnY + learnH) {
    reason = "FastMatch search ROI must contain the learn ROI";
    return false;
  }

  reason = "FastMatch learn/search ROI relation is valid";
  return true;
}

bool PipelineFileContainsText(const std::filesystem::path &path,
                              const std::string &needle) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open())
    return false;
  std::ostringstream ss;
  ss << file.rdbuf();
  return ss.str().find(needle) != std::string::npos;
}

void WriteSyntheticGaugeAnnotation(const std::filesystem::path &caseDir,
                                   const CxEvidenceSelfTestRequest &request) {
  std::filesystem::create_directories(caseDir);
  std::ofstream file(caseDir / "gauge_annotation.json", std::ios::binary);
  file << "{\n"
       << "  \"schema_version\": 1,\n"
       << "  \"case_id\": \"" << PipelineJsonEscape(request.case_id) << "\",\n"
       << "  \"image_id\": \"" << PipelineJsonEscape(request.image_id)
       << "\",\n"
       << "  \"target_id\": \"" << PipelineJsonEscape(request.target_id)
       << "\",\n"
       << "  \"tool\": \"" << PipelineJsonEscape(request.tool) << "\",\n"
       << "  \"source\": \"evidence_lock_pipeline\",\n"
       << "  \"review_status\": \"manual_accepted\",\n"
       << "  \"accepted\": true,\n"
       << "  \"parameter_summary\": \""
       << PipelineJsonEscape(request.parameter_summary) << "\"\n"
       << "}\n";
}

std::vector<std::pair<std::string, int>>
PipelineRuntimeGlobalsFromParam(const std::string &parameterSummary) {
  auto getInt = [&](const std::string &key, int fallback) -> int {
    const std::string pattern = key + "=";
    const std::size_t pos = parameterSummary.find(pattern);
    if (pos == std::string::npos)
      return fallback;
    std::size_t begin = pos + pattern.size();
    std::size_t end = begin;
    while (end < parameterSummary.size() &&
           (std::isdigit(static_cast<unsigned char>(parameterSummary[end])) ||
            parameterSummary[end] == '-' || parameterSummary[end] == '+')) {
      ++end;
    }
    if (end == begin)
      return fallback;
    try {
      return std::stoi(parameterSummary.substr(begin, end - begin));
    } catch (...) {
      return fallback;
    }
  };

  return {{"global_method", getInt("method", 0)},
          {"global_threshold", getInt("threshold", 20)},
          {"global_gap", getInt("gap", 0)},
          {"global_linegap", getInt("linegap", 0)},
          {"global_circle_cx", getInt("circle_cx", 0)},
          {"global_circle_cy", getInt("circle_cy", 0)},
          {"global_circle_px", getInt("circle_px", 0)},
          {"global_circle_py", getInt("circle_py", 0)},
          {"global_roi_x0", getInt("roi_x0", 0)},
          {"global_roi_y0", getInt("roi_y0", 0)},
          {"global_roi_x1", getInt("roi_x1", 0)},
          {"global_roi_y1", getInt("roi_y1", 0)},
          {"global_wgap", getInt("wgap", 0)},
          {"global_hgap", getInt("hgap", 0)},
          {"global_tool_half_width", getInt("tool_half_width", 0)},
          {"global_compare_gap", getInt("compare_gap", 0)},
          {"global_learn_roi_x", getInt("learn_roi_x", 0)},
          {"global_learn_roi_y", getInt("learn_roi_y", 0)},
          {"global_learn_roi_w", getInt("learn_roi_w", 0)},
          {"global_learn_roi_h", getInt("learn_roi_h", 0)},
          {"global_search_roi_x", getInt("search_roi_x", 0)},
          {"global_search_roi_y", getInt("search_roi_y", 0)},
          {"global_search_roi_w", getInt("search_roi_w", 0)},
          {"global_search_roi_h", getInt("search_roi_h", 0)},
          {"global_find_num", getInt("find_num", 0)}};
}

void WriteSyntheticEvidenceReview(const std::filesystem::path &caseDir,
                                  const CxEvidenceSelfTestRequest &request,
                                  const std::string &runtimeStatus,
                                  const std::string &runtimeReason) {
  std::filesystem::create_directories(caseDir);
  std::ofstream file(caseDir / "evidence_review.json", std::ios::binary);
  file << "{\n"
       << "  \"schema_version\": 1,\n"
       << "  \"review_status\": \"manual_accepted\",\n"
       << "  \"case_id\": \"" << PipelineJsonEscape(request.case_id) << "\",\n"
       << "  \"script_id\": \"" << PipelineJsonEscape(request.script_id)
       << "\",\n"
       << "  \"script_path\": \"" << PipelineJsonEscape(request.script_path)
       << "\",\n"
       << "  \"image_id\": \"" << PipelineJsonEscape(request.image_id)
       << "\",\n"
       << "  \"image_path\": \"" << PipelineJsonEscape(request.image_path)
       << "\",\n"
       << "  \"target_id\": \"" << PipelineJsonEscape(request.target_id)
       << "\",\n"
       << "  \"tool\": \"" << PipelineJsonEscape(request.tool) << "\",\n"
       << "  \"parameter_summary\": \""
       << PipelineJsonEscape(request.parameter_summary) << "\",\n"
       << "  \"gauge_annotation_path\": \""
       << PipelineJsonEscape((caseDir / "gauge_annotation.json").string())
       << "\",\n"
       << "  \"runtime_status\": \"" << PipelineJsonEscape(runtimeStatus)
       << "\",\n"
       << "  \"runtime_reason\": \"" << PipelineJsonEscape(runtimeReason)
       << "\",\n"
       << "  \"binding_policy\": \"evidence_locked_only\",\n"
       << "  \"runtime_int_globals\": {\n";
  const auto globals =
      PipelineRuntimeGlobalsFromParam(request.parameter_summary);
  for (std::size_t i = 0; i < globals.size(); ++i) {
    file << "    \"" << PipelineJsonEscape(globals[i].first)
         << "\": " << globals[i].second;
    if (i + 1 < globals.size())
      file << ",";
    file << "\n";
  }
  file << "  }\n"
       << "}\n";
}

bool WriteEvidenceLockPipelineReports(
    const std::string &outDir, const std::string &runId,
    const std::vector<EvidenceLockPipelineCheck> &checks, std::string &reason) {
  reason.clear();
  std::filesystem::create_directories(outDir);
  const int total = static_cast<int>(checks.size());
  int pass = 0;
  for (const auto &c : checks)
    if (c.conclusion == "PASS")
      ++pass;
  const int fail = total - pass;

  {
    std::ofstream file(std::filesystem::path(outDir) /
                           "evidence_lock_pipeline_summary.json",
                       std::ios::binary);
    if (!file.is_open()) {
      reason = "failed to open evidence_lock_pipeline_summary.json";
      return false;
    }
    file << "{\n"
         << "  \"run_id\": \"" << PipelineJsonEscape(runId) << "\",\n"
         << "  \"total_cases\": " << total << ",\n"
         << "  \"pass_count\": " << pass << ",\n"
         << "  \"fail_count\": " << fail << ",\n"
         << "  \"final_code\": \""
         << (fail == 0 ? "EVIDENCE_LOCK_PIPELINE_PASS"
                       : "EVIDENCE_LOCK_PIPELINE_FAIL")
         << "\",\n"
         << "  \"cases\": [\n";
    for (std::size_t i = 0; i < checks.size(); ++i) {
      const auto &c = checks[i];
      file << "    {\n"
           << "      \"case_id\": \"" << PipelineJsonEscape(c.case_id)
           << "\",\n"
           << "      \"expected\": \"" << PipelineJsonEscape(c.expected)
           << "\",\n"
           << "      \"actual\": \"" << PipelineJsonEscape(c.actual) << "\",\n"
           << "      \"conclusion\": \"" << PipelineJsonEscape(c.conclusion)
           << "\",\n"
           << "      \"reason\": \"" << PipelineJsonEscape(c.reason) << "\",\n"
           << "      \"report_path\": \"" << PipelineJsonEscape(c.report_path)
           << "\"\n"
           << "    }";
      if (i + 1 < checks.size())
        file << ",";
      file << "\n";
    }
    file << "  ]\n"
         << "}\n";
  }

  {
    std::ofstream file(std::filesystem::path(outDir) /
                           "evidence_lock_pipeline_report.md",
                       std::ios::binary);
    if (!file.is_open()) {
      reason = "failed to open evidence_lock_pipeline_report.md";
      return false;
    }
    file << "# Evidence Lock Pipeline Report\n\n";
    file << "- run_id: " << runId << "\n";
    file << "- total_cases: " << total << "\n";
    file << "- pass_count: " << pass << "\n";
    file << "- fail_count: " << fail << "\n";
    file << "- final_code: "
         << (fail == 0 ? "EVIDENCE_LOCK_PIPELINE_PASS"
                       : "EVIDENCE_LOCK_PIPELINE_FAIL")
         << "\n\n";
    file << "| Case | Expected | Actual | Conclusion | Reason | Report |\n";
    file << "|---|---|---|---|---|---|\n";
    for (const auto &c : checks) {
      file << "| " << c.case_id << " | " << c.expected << " | " << c.actual
           << " | " << c.conclusion << " | " << c.reason << " | "
           << c.report_path << " |\n";
    }
  }

  return true;
}
} // namespace

bool ParseShapeInteractionTestArgs(int argc, char **argv,
                                   ShapeInteractionTestOptions &options) {
  options = {};

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];

    if (arg == "--shape-interaction-smoke" ||
        arg == "--shape_interaction_smoke") {
      options.enabled = true;
      continue;
    }

    if (arg == "--annotation-tool-manifest" ||
        arg == "--shape-interaction-manifest" ||
        arg == "--shape_interaction_manifest") {
      options.enabled = true;

      if (i + 1 >= argc) {
        options.parse_ok = false;
        options.parse_reason = arg + " requires a path";
        return false;
      }

      options.manifest_path = argv[++i];
      continue;
    }

    if (arg == "--shape-interaction-suite" ||
        arg == "--shape_interaction_suite") {
      options.enabled = true;

      if (i + 1 >= argc) {
        options.parse_ok = false;
        options.parse_reason = arg + " requires a path";
        return false;
      }

      options.suite_path = argv[++i];
      continue;
    }

    if (arg == "--out") {
      if (i + 1 >= argc) {
        options.parse_ok = false;
        options.parse_reason = "--out requires a path";
        return false;
      }

      options.out_dir = argv[++i];
      continue;
    }

    if (arg == "--image-manifest") {
      if (i + 1 >= argc) {
        options.parse_ok = false;
        options.parse_reason = "--image-manifest requires a path";
        return false;
      }

      options.image_manifest_path = argv[++i];
      continue;
    }

    if (arg == "--shape_suite" || arg == "--out_dir") {
      options.parse_ok = false;
      options.parse_reason = "unsupported option '" + arg +
                             "'; use --shape-interaction-suite and --out";
      return false;
    }

    if (arg.find("--shape-interaction") == 0 ||
        arg.find("--shape_interaction") == 0) {
      options.parse_ok = false;
      options.parse_reason = "unknown shape interaction option: " + arg;
      return false;
    }
  }

  return true;
}

struct EvidenceChainSelfTestCliOptions {
  bool enabled = false;
  bool torch_training_label_package_smoke = false;
  bool evidence_lock_pipeline = false;
  bool standard_chain_gate = false;
  bool baseline_lock = false;
  bool param_regression_loop = false;
  std::string annotation_tool_manifest;
  std::string suite_path;
  std::string image_manifest_path;
  std::string param_regression_script;
  std::string param_regression_tool;
  std::string out_dir;
  std::string evidence_tool_filter;
  std::string torch_training_label_script;
  int max_cases = 0;
};

bool ParseEvidenceChainSelfTestArgs(int argc, char **argv,
                                    EvidenceChainSelfTestCliOptions &options) {
  options = {};

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];

    if (arg == "--evidence-chain-selftest") {
      options.enabled = true;
      continue;
    }

    if (arg == "--evidence-lock-pipeline") {
      options.enabled = true;
      options.evidence_lock_pipeline = true;
      continue;
    }

    if (arg == "--torch-training-label-package-smoke") {
      options.enabled = true;
      options.torch_training_label_package_smoke = true;
      continue;
    }

    if (arg == "--torch-training-label-script" && i + 1 < argc) {
      options.torch_training_label_script = argv[++i];
      continue;
    }

    if (arg == "--standard-chain-gate") {
      options.enabled = true;
      options.standard_chain_gate = true;
      continue;
    }

    if (arg == "--baseline-lock") {
      options.enabled = true;
      options.baseline_lock = true;
      continue;
    }

    if (arg == "--param-regression-loop") {
      options.enabled = true;
      options.param_regression_loop = true;
      continue;
    }

    if (arg == "--cxscript-suite" && i + 1 < argc) {
      options.suite_path = argv[++i];
      continue;
    }

    if (arg == "--image-manifest" && i + 1 < argc) {
      options.image_manifest_path = argv[++i];
      continue;
    }

    if (arg == "--param-regression-script" && i + 1 < argc) {
      options.param_regression_script = argv[++i];
      continue;
    }

    if (arg == "--param-regression-tool" && i + 1 < argc) {
      options.param_regression_tool = argv[++i];
      continue;
    }

    if (arg == "--annotation-tool-manifest" ||
        arg == "--shape-interaction-manifest" ||
        arg == "--shape_interaction_manifest") {
      if (i + 1 >= argc)
        continue;
      options.annotation_tool_manifest = argv[++i];
      continue;
    }

    if (arg == "--out") {
      if (i + 1 >= argc)
        continue;
      options.out_dir = argv[++i];
      continue;
    }

    if (arg == "--evidence-tool-filter") {
      if (i + 1 >= argc)
        continue;
      options.evidence_tool_filter = argv[++i];
      continue;
    }

    if (arg == "--max-cases") {
      if (i + 1 >= argc)
        continue;
      try {
        options.max_cases = std::stoi(argv[++i]);
      } catch (...) {
        options.max_cases = 0;
      }
      continue;
    }
  }

  return true;
}

bool RunShapeInteractionSmokeCli(const std::string &manifest_path,
                                 const std::string &suite_path,
                                 const std::string &image_manifest_path,
                                 const std::string &out_dir,
                                 CxShapeInteractionBatchResult &result) {
  ViewController viewer;
  return viewer.RunShapeInteractionSmoke(manifest_path, suite_path,
                                         image_manifest_path, out_dir, result);
}

int RunMetrologyAnalyticsSmokeCli(int argc, char **argv) {
  const std::string outArg = CliValueAfter(argc, argv, "--out");
  const std::filesystem::path outDir =
      outArg.empty() ? std::filesystem::path(
                           "D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/"
                           "cxscript_runs/metrology_analytics/smoke_default")
                     : std::filesystem::path(outArg);

  cxvision::metrology_analytics::CxMetrologyAnalyticsSmokeResult result;
  std::string reason;
  const bool ok = cxvision::metrology_analytics::RunMetrologyAnalyticsSmoke(
      outDir, result, reason);

  std::cout << "metrology_analytics_smoke_ok="
            << ((ok && result.fail_count == 0) ? "true" : "false") << "\n";
  std::cout << "total_cases=" << result.total_cases << "\n";
  std::cout << "pass_count=" << result.pass_count << "\n";
  std::cout << "fail_count=" << result.fail_count << "\n";
  std::cout << "summary=" << result.summary_path.string() << "\n";
  std::cout << "report=" << result.report_path.string() << "\n";
  if (!reason.empty())
    std::cout << "reason=" << reason << "\n";

  return (ok && result.fail_count == 0) ? 0 : 1;
}

int RunGwyReferenceInterfaceSmokeCli(int argc, char **argv) {
  const std::string runId = CxUnifiedLog::Instance().GenerateRunId();
  const std::string outArg = CliValueAfter(argc, argv, "--out");
  const std::filesystem::path outDir =
      outArg.empty()
          ? std::filesystem::path(
                "D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxscript_runs/"
                "gwy_reference/run_" +
                runId + "_interface_null")
          : std::filesystem::path(outArg);

  cxvision::gwy_reference::CxGwyReferenceRequest request;
  request.request_id = runId;
  request.case_id = "gwy_reference_interface_null_smoke";
  request.algorithm_id = "surface.basic_stats";
  request.input_ref = "builtin:flat5";
  request.input_hash = "builtin-flat5-v1";
  request.mode = cxvision::gwy_reference::CxGwyExecutionMode::DualCompare;

  auto backend = cxvision::gwy_reference::CreateGwyReferenceBackend();
  cxvision::metrology_analytics::CxPhysUnit unit;
  cxvision::metrology_analytics::CxSurfaceField nativeField(5, 5, unit);
  nativeField.fillFromGenerator([](int, int) { return 5.0; });
  const auto nativeStats =
      cxvision::metrology_analytics::computeSurfaceBasicStats(nativeField);
  cxvision::gwy_reference::CxGwyNormalizedResult nativeResult;
  nativeResult.implementation = "cxvision.metrology_analytics";
  nativeResult.implementation_version = "1";
  nativeResult.status = "CX_NATIVE_EXECUTION_COMPLETE";
  nativeResult.conclusion = "PENDING_HUMAN_REVIEW";
  nativeResult.reason = "cxvision native flat5 basic statistics executed";
  nativeResult.backend_available = true;
  nativeResult.executed = true;
  nativeResult.algorithm_success = true;
  nativeResult.metrics = {
      {"basic_stats.min", nativeStats.min},
      {"basic_stats.max", nativeStats.max},
      {"basic_stats.mean", nativeStats.mean},
      {"basic_stats.ra", nativeStats.ra},
      {"basic_stats.rms", nativeStats.rms},
      {"basic_stats.skewness", nativeStats.skewness},
      {"basic_stats.kurtosis", nativeStats.kurtosis_excess}};
  cxvision::gwy_reference::CxGwyReferenceRunPackage package;
  std::string reason;
  const bool ok = cxvision::gwy_reference::RunReferenceInterfaceClosure(
      request, *backend, &nativeResult, outDir, package, reason);

  std::cout << "gwy_reference_interface_ok=" << (ok ? "true" : "false") << "\n";
  std::cout << "backend_available="
            << (package.reference_result.backend_available ? "true" : "false")
            << "\n";
  std::cout << "algorithm_executed="
            << (package.reference_result.executed ? "true" : "false") << "\n";
  std::cout << "reference_status=" << package.reference_result.status << "\n";
  std::cout << "conclusion=" << package.comparison.conclusion << "\n";
  std::cout << "promotion_allowed=false\n";
  std::cout << "out_dir=" << outDir.string() << "\n";
  std::cout << "report=" << package.report_path.string() << "\n";
  std::cout << "reason=" << reason << "\n";
  return ok ? 0 : 1;
}

int RunMetrologyAnalyticsSelfTestCli(int argc, char **argv,
                                     const std::string &filter) {
  const std::string outArg = CliValueAfter(argc, argv, "--out");
  const std::filesystem::path outDir =
      outArg.empty() ? std::filesystem::path(
                           "D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/"
                           "cxscript_runs/metrology_analytics/selftest_default")
                     : std::filesystem::path(outArg);

  cxvision::metrology_analytics::CxMetrologyAnalyticsSmokeResult smoke;
  std::string reason;
  const bool smokeOk =
      cxvision::metrology_analytics::RunMetrologyAnalyticsSmoke(outDir, smoke,
                                                                reason);

  const std::string prefix = "analytics.";
  std::string selector;
  bool namespaceSupported = false;
  bool selectAll = false;
  if (filter == "analytics" || filter == "analytics.*") {
    namespaceSupported = true;
    selectAll = true;
  } else if (filter.rfind(prefix, 0) == 0) {
    namespaceSupported = true;
    selector = filter.substr(prefix.size());
    selectAll = selector.empty() || selector == "*";
  }

  std::vector<
      cxvision::metrology_analytics::CxMetrologyAnalyticsSmokeCaseResult>
      selected;
  if (namespaceSupported && smokeOk) {
    for (const auto &c : smoke.cases) {
      if (selectAll || c.case_id == selector)
        selected.push_back(c);
    }
  }

  int selectedPass = 0;
  int selectedFail = 0;
  for (const auto &c : selected) {
    if (c.pass)
      ++selectedPass;
    else
      ++selectedFail;
  }

  std::string finalCode;
  if (!namespaceSupported) {
    finalCode = "SELFTEST_NAMESPACE_NOT_HANDLED";
    if (reason.empty())
      reason = "selftest namespace is not analytics: " + filter;
  } else if (!smokeOk) {
    finalCode = "ANALYTICS_SELFTEST_FAIL";
    if (reason.empty())
      reason = "analytics smoke failed";
  } else if (selected.empty()) {
    finalCode = "ANALYTICS_SELFTEST_NO_MATCH";
    reason = "no analytics selftest case matched filter: " + filter;
  } else if (selectedFail == 0) {
    finalCode = "ANALYTICS_SELFTEST_PASS";
    reason = "analytics selftest filter passed";
  } else {
    finalCode = "ANALYTICS_SELFTEST_FAIL";
    reason = "analytics selftest filter has failed cases";
  }

  std::filesystem::create_directories(outDir);
  const std::filesystem::path summaryPath =
      outDir / "analytics_selftest_summary.json";
  const std::filesystem::path reportPath =
      outDir / "analytics_selftest_report.md";

  {
    std::ofstream f(summaryPath, std::ios::binary | std::ios::trunc);
    f << "{\n"
      << "  \"schema\": \"cxvision.analytics_selftest.v1\",\n"
      << "  \"filter\": \"" << PipelineJsonEscape(filter) << "\",\n"
      << "  \"namespace_supported\": "
      << (namespaceSupported ? "true" : "false") << ",\n"
      << "  \"selected_case_count\": " << selected.size() << ",\n"
      << "  \"pass_count\": " << selectedPass << ",\n"
      << "  \"fail_count\": " << selectedFail << ",\n"
      << "  \"backing_smoke_summary\": \""
      << PipelineJsonEscape(smoke.summary_path.string()) << "\",\n"
      << "  \"backing_smoke_report\": \""
      << PipelineJsonEscape(smoke.report_path.string()) << "\",\n"
      << "  \"final_code\": \"" << PipelineJsonEscape(finalCode) << "\",\n"
      << "  \"reason\": \"" << PipelineJsonEscape(reason) << "\",\n"
      << "  \"cases\": [\n";
    for (std::size_t i = 0; i < selected.size(); ++i) {
      const auto &c = selected[i];
      f << "    {\"case_id\":\"" << PipelineJsonEscape(c.case_id)
        << "\", \"category\":\"" << PipelineJsonEscape(c.category)
        << "\", \"pass\":" << (c.pass ? "true" : "false")
        << ", \"observed\":" << c.observed << ", \"expected\":" << c.expected
        << ", \"tolerance\":" << c.tolerance << ", \"reason\":\""
        << PipelineJsonEscape(c.reason) << "\"}"
        << (i + 1 < selected.size() ? "," : "") << "\n";
    }
    f << "  ]\n"
      << "}\n";
  }

  {
    std::ofstream f(reportPath, std::ios::binary | std::ios::trunc);
    f << "# Analytics SelfTest Report\n\n";
    f << "- filter: " << filter << "\n";
    f << "- selected_case_count: " << selected.size() << "\n";
    f << "- pass_count: " << selectedPass << "\n";
    f << "- fail_count: " << selectedFail << "\n";
    f << "- final_code: " << finalCode << "\n";
    f << "- reason: " << reason << "\n";
    f << "- backing_smoke_summary: " << smoke.summary_path.string() << "\n";
    f << "- backing_smoke_report: " << smoke.report_path.string() << "\n\n";
    f << "| Case | Category | Pass | Observed | Expected | Tol | Reason |\n";
    f << "|---|---|---:|---:|---:|---:|---|\n";
    for (const auto &c : selected) {
      f << "| " << c.case_id << " | " << c.category << " | " << (c.pass ? 1 : 0)
        << " | " << c.observed << " | " << c.expected << " | " << c.tolerance
        << " | " << c.reason << " |\n";
    }
  }

  std::cout << "analytics_selftest_ok="
            << (finalCode == "ANALYTICS_SELFTEST_PASS" ? "true" : "false")
            << "\n";
  std::cout << "filter=" << filter << "\n";
  std::cout << "selected_case_count=" << selected.size() << "\n";
  std::cout << "pass_count=" << selectedPass << "\n";
  std::cout << "fail_count=" << selectedFail << "\n";
  std::cout << "summary=" << summaryPath.string() << "\n";
  std::cout << "report=" << reportPath.string() << "\n";
  std::cout << "final_code=" << finalCode << "\n";
  if (!reason.empty())
    std::cout << "reason=" << reason << "\n";

  return finalCode == "ANALYTICS_SELFTEST_PASS" ? 0 : 2;
}

int RunEvidenceLockPipelineCli(const EvidenceChainSelfTestCliOptions &options) {
  std::cout << "[MAIN] evidence lock pipeline mode begin\n" << std::flush;

  ViewController controller;
  std::string initReason;
  if (!controller.InitEvidenceSelfTestEnvironment(initReason)) {
    std::cout << "[MAIN] evidence lock pipeline init failed: " << initReason
              << "\n";
    return 2;
  }

  const std::string run_id = CxUnifiedLog::Instance().GenerateRunId();
  const std::string out_dir =
      options.out_dir.empty() ? "D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/"
                                "cxscript_runs/evidence_lock_pipeline/run_" +
                                    run_id
                              : options.out_dir;

  const std::string imagePath =
      "D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/01.jpg";
  const std::string circleScript =
      "cxparser/cxscript/module/cximage/find_circle_direct_test.cxsc";
  const std::string lineScript =
      "cxparser/cxscript/module/cximage/frozen/findline/"
      "findline_vertical_stage25_filter20_ok.cxsc";
  const std::string ellipseScript =
      "cxparser/cxscript/module/cximage/find_ellipse_direct_test.cxsc";
  const std::string rectScript =
      "cxparser/cxscript/module/cximage/find_rect_direct_test.cxsc";
  const std::string fastmatchScript =
      "cxparser/cxscript/module/cximage/frozen/fastmatch/"
      "fastmatch_stage26_direct_ok.cxsc";
  const std::string fastmatchLearnScript =
      "cxparser/cxscript/module/cximage/diagnostic/fastmatch/"
      "fastmatch_learn_points_direct_test.cxsc";

  auto makeReq = [&](const std::string &caseId, const std::string &script,
                     const std::string &imageId, const std::string &targetId,
                     const std::string &tool,
                     const std::string &param) -> CxEvidenceSelfTestRequest {
    CxEvidenceSelfTestRequest r;
    r.run_id = run_id;
    r.case_id = caseId;
    r.script_id = caseId + "_script";
    r.script_path = script;
    r.image_id = imageId;
    r.image_path = imagePath;
    r.target_id = targetId;
    r.tool = tool;
    r.parameter_summary = param;
    r.out_dir = out_dir + "/cases/" + caseId;
    return r;
  };

  struct PlannedCase {
    CxEvidenceSelfTestRequest request;
    std::string expected;
    bool expected_param_fail = false;
    bool expected_param_pass = false;
    bool require_learn_points = false;
  };

  const std::string circleFull =
      "method=0 threshold=20 gap=5 linegap=3 circle_cx=850 circle_cy=690 "
      "circle_px=0 circle_py=690";
  const std::string lineFull =
      "method=2 threshold=20 wgap=32 hgap=8 linegap=6 tool_half_width=20 "
      "roi_x0=82 roi_y0=183 roi_x1=1210 roi_y1=183 filterprofile=1 "
      "max_elapsed_ms=2000 max_scan_lines=256 max_samples=4096";
  const std::string fastmatchFull =
      "method=0 threshold=16 linegap=3 wgap=15 hgap=15 compare_gap=20 "
      "objfilter=0 min_score=0.65 learn_roi_x=100 learn_roi_y=100 "
      "learn_roi_w=420 learn_roi_h=320 search_roi_x=0 search_roi_y=0 "
      "search_roi_w=1280 search_roi_h=960 find_num=1";
  const std::string fastmatchSearchTooSmall =
      "method=0 threshold=16 linegap=3 wgap=15 hgap=15 compare_gap=20 "
      "objfilter=0 min_score=0.65 learn_roi_x=100 learn_roi_y=100 "
      "learn_roi_w=420 learn_roi_h=320 search_roi_x=100 search_roi_y=100 "
      "search_roi_w=300 search_roi_h=240 find_num=1";

  std::vector<PlannedCase> planned;
  planned.push_back({makeReq("A1_empty_params", circleScript, "baseline_01",
                             "circle_main", "FindCircle", ""),
                     "PARAM_BINDING_FAIL", true, false});
  planned.push_back(
      {makeReq("A2_profile_name_params", circleScript, "baseline_01",
               "circle_main", "FindCircle", "stage25_direct"),
       "PARAM_BINDING_FAIL", true, false});
  planned.push_back(
      {makeReq("A3_level_name_params", circleScript, "baseline_01",
               "circle_main", "FindCircle", "L1_high_contrast"),
       "PARAM_BINDING_FAIL", true, false});
  planned.push_back({makeReq("A4_findcircle_missing_gauge", circleScript,
                             "baseline_01", "circle_main", "FindCircle",
                             "method=0 threshold=20 gap=5 linegap=3"),
                     "PARAM_BINDING_FAIL", true, false});
  planned.push_back(
      {makeReq("A5_findcircle_locked", circleScript, "baseline_01",
               "circle_main", "FindCircle", circleFull),
       "PARAM_BINDING_PASS", false, true});
  planned.push_back({makeReq("A6_findline_locked", lineScript, "baseline_01",
                             "line_main", "FindLine", lineFull),
                     "PARAM_BINDING_PASS", false, true});
  planned.push_back(
      {makeReq("B1_seed_findcircle_locked", circleScript, "baseline_01",
               "circle_main", "FindCircle", circleFull),
       "PARAM_BINDING_PASS", false, true});
  planned.push_back({makeReq("B1_empty_findline_after_circle", lineScript,
                             "baseline_01", "line_main", "FindLine", ""),
                     "PARAM_BINDING_FAIL", true, false});
  planned.push_back(
      {makeReq(
           "B2_findcircle_missing_pxpy", circleScript, "baseline_01",
           "circle_main", "FindCircle",
           "method=0 threshold=20 gap=5 linegap=3 circle_cx=100 circle_cy=100"),
       "PARAM_BINDING_FAIL", true, false});
  planned.push_back(
      {makeReq("T_findellipse_locked", ellipseScript, "baseline_01",
               "ellipse_main", "FindEllipse",
               "method=1 threshold=8 gap=5 linegap=3 ellipse_x0=600 "
               "ellipse_y0=360 ellipse_x1=930 ellipse_y1=580 "
               "ellipse_inner_scale_percent=0 findellipse_findsetting=1"),
       "PARAM_BINDING_PASS", false, true});
  planned.push_back(
      {makeReq("T_findrect_locked", rectScript, "baseline_01", "rect_main",
               "FindRect",
               "method=0 threshold=20 gauge=20 linegap=3 roi_x=120 roi_y=120 "
               "roi_width=640 roi_height=480"),
       "PARAM_BINDING_PASS", false, true});
  planned.push_back(
      {makeReq("T_fastmatch_search_too_small", fastmatchScript, "baseline_01",
               "fastmatch_main", "FastMatch", fastmatchSearchTooSmall),
       "PARAM_BINDING_FAIL", true, false});
  planned.push_back(
      {makeReq("T_fastmatch_learn_points", fastmatchLearnScript, "baseline_01",
               "fastmatch_main", "FastMatch", fastmatchFull),
       "FASTMATCH_LEARN_POINTS_PASS", false, true, true});
  planned.push_back(
      {makeReq("T_fastmatch_locked", fastmatchScript, "baseline_01",
               "fastmatch_main", "FastMatch", fastmatchFull),
       "PARAM_BINDING_PASS", false, true});

  std::vector<EvidenceLockPipelineCheck> checks;

  for (const PlannedCase &pc : planned) {
    CxEvidenceSelfTestResult r;
    std::string runReason;
    std::string fastmatchParamReason;
    const bool fastmatchParamOk =
        pc.request.tool != "FastMatch" ||
        PipelineFastMatchSearchContainsLearn(pc.request.parameter_summary,
                                             fastmatchParamReason);

    if (!fastmatchParamOk) {
      r.run_id = run_id;
      r.case_id = pc.request.case_id;
      r.executed = true;
      r.script_id = pc.request.script_id;
      r.script_path = pc.request.script_path;
      r.image_id = pc.request.image_id;
      r.image_path = pc.request.image_path;
      r.target_id = pc.request.target_id;
      r.tool = pc.request.tool;
      r.parameter_summary = pc.request.parameter_summary;
      r.final_code = "PARAM_BINDING_FAIL";
      r.final_status = "FAIL";
      r.final_reason = fastmatchParamReason;
      AddEvidenceSelfTestStep(r, "PARAM_BINDING_FAIL", "FAIL",
                              fastmatchParamReason);
    } else {
      controller.RunEvidenceChainSelfTest(pc.request, r, runReason);
    }

    std::string writeReason;
    WriteEvidenceSelfTestSummaryJson(
        r, pc.request.out_dir + "/evidence_selftest_summary.json", writeReason);
    WriteEvidenceSelfTestReportMd(
        r, pc.request.out_dir + "/evidence_selftest_report.md", writeReason);

    EvidenceLockPipelineCheck check;
    check.case_id = pc.request.case_id;
    check.expected = pc.expected;
    check.actual = r.final_code;
    check.report_path = pc.request.out_dir + "/evidence_selftest_report.md";

    bool ok = false;
    const int fastmatchLearnPointCount =
        r.fastmatch_model_point_count + r.fastmatch_learn_a_count +
        r.fastmatch_learn_b_count + r.fastmatch_learn_a2_count +
        r.fastmatch_learn_b2_count;
    if (pc.expected_param_fail)
      ok = r.final_code == "PARAM_BINDING_FAIL";
    else if (pc.require_learn_points)
      ok = PipelineHasStep(r, "RUNTIME_EXECUTE_PASS", "PASS") &&
           fastmatchLearnPointCount > 0;
    else if (pc.expected_param_pass)
      ok = PipelineHasStep(r, "PARAM_BINDING_PASS", "PASS");
    if (!ok && pc.require_learn_points &&
        PipelineHasStep(r, "RUNTIME_EXECUTE_PASS", "PASS") &&
        fastmatchLearnPointCount <= 0) {
      check.actual = "FASTMATCH_LEARN_POINTS_FAIL";
      check.reason = "FastMatch learn produced zero model points; adjust "
                     "locked learn ROI/threshold before match.";
    } else {
      check.reason =
          ok ? (pc.require_learn_points
                    ? ("FastMatch learn points/model=" +
                       std::to_string(fastmatchLearnPointCount) + " model=" +
                       std::to_string(r.fastmatch_model_point_count))
                    : PipelineReasonForStep(r, pc.expected))
             : ("unexpected final=" + r.final_code +
                " reason=" + r.final_reason);
    }
    check.conclusion = ok ? "PASS" : "FAIL";
    checks.push_back(check);
  }

  auto writeSaveCase = [&](const std::string &caseId,
                           const std::string &expected, bool pass,
                           const std::string &reasonText, bool writeReview) {
    CxEvidenceSelfTestRequest request =
        makeReq(caseId, circleScript, writeReview ? "baseline_01" : "",
                writeReview ? "circle_main" : "", "FindCircle",
                writeReview ? circleFull : "");
    request.out_dir = out_dir + "/cases/" + caseId;

    CxEvidenceSelfTestResult r;
    r.run_id = run_id;
    r.case_id = caseId;
    r.executed = true;
    r.script_id = request.script_id;
    r.script_path = request.script_path;
    r.image_id = request.image_id;
    r.image_path = request.image_path;
    r.target_id = request.target_id;
    r.tool = request.tool;
    r.parameter_summary = request.parameter_summary;
    r.final_code =
        pass ? "SAVE_EVIDENCE_REVIEW_PASS" : "SAVE_EVIDENCE_REVIEW_FAIL";
    r.final_status = pass ? "PASS" : "FAIL";
    r.final_reason = reasonText;
    AddEvidenceSelfTestStep(r, r.final_code, r.final_status, reasonText);

    if (writeReview) {
      const std::filesystem::path caseDir(request.out_dir);
      WriteSyntheticGaugeAnnotation(caseDir, request);
      WriteSyntheticEvidenceReview(caseDir, request, "manual_accepted",
                                   "synthetic evidence lock save review");
    }

    std::string writeReason;
    WriteEvidenceSelfTestSummaryJson(
        r, request.out_dir + "/evidence_selftest_summary.json", writeReason);
    WriteEvidenceSelfTestReportMd(
        r, request.out_dir + "/evidence_selftest_report.md", writeReason);

    EvidenceLockPipelineCheck check;
    check.case_id = caseId;
    check.expected = expected;
    check.actual = r.final_code;
    check.conclusion = r.final_code == expected ? "PASS" : "FAIL";
    check.reason = reasonText;
    check.report_path = request.out_dir + "/evidence_selftest_report.md";
    checks.push_back(check);
  };

  writeSaveCase("C1_free_run_save_review", "SAVE_EVIDENCE_REVIEW_FAIL", false,
                "cannot save evidence review: no selected evidence row", false);
  writeSaveCase("C2_profile_param_save_review", "SAVE_EVIDENCE_REVIEW_FAIL",
                false,
                "cannot save evidence review: evidence parameter summary is "
                "not key=value locked data",
                false);
  writeSaveCase(
      "C3_unaccepted_gauge_save_review", "SAVE_EVIDENCE_REVIEW_FAIL", false,
      "cannot save evidence review: gauge is not manual_accepted", false);
  writeSaveCase("C4_locked_accepted_save_review", "SAVE_EVIDENCE_REVIEW_PASS",
                true, "evidence review saved", true);

  std::string reportReason;
  WriteEvidenceLockPipelineReports(out_dir, run_id, checks, reportReason);

  int passCount = 0;
  for (const auto &check : checks)
    if (check.conclusion == "PASS")
      ++passCount;
  const int failCount = static_cast<int>(checks.size()) - passCount;

  std::cout << "[MAIN] evidence lock pipeline end\n";
  std::cout << "evidence_lock_pipeline_ok="
            << (failCount == 0 ? "true" : "false") << "\n";
  std::cout << "total_cases=" << checks.size() << "\n";
  std::cout << "pass_count=" << passCount << "\n";
  std::cout << "fail_count=" << failCount << "\n";
  std::cout << "summary="
            << (std::filesystem::path(out_dir) /
                "evidence_lock_pipeline_summary.json")
                   .string()
            << "\n";
  std::cout << "report="
            << (std::filesystem::path(out_dir) /
                "evidence_lock_pipeline_report.md")
                   .string()
            << "\n";
  return failCount == 0 ? 0 : 1;
}

int RunBaselineLockCli(const EvidenceChainSelfTestCliOptions &options);
int RunParamRegressionLoopCli(const EvidenceChainSelfTestCliOptions &options);

struct StandardChainGateRow {
  std::string priority;
  std::string title;
  std::string code;
  std::string status;
  std::string evidence;
  std::string remaining;
  std::string next_step;
};

bool WriteStandardChainGateReports(
    const std::string &outDir, const std::string &runId,
    const std::vector<StandardChainGateRow> &rows, std::string &reason) {
  namespace fs = std::filesystem;
  fs::create_directories(outDir);
  int pass = 0;
  int pending = 0;
  int partial = 0;
  int fail = 0;
  for (const auto &row : rows) {
    if (row.status == "PASS")
      ++pass;
    else if (row.status == "PENDING")
      ++pending;
    else if (row.status == "PARTIAL")
      ++partial;
    else
      ++fail;
  }

  const std::string finalCode =
      fail > 0 ? "STANDARD_CHAIN_GATE_FAIL"
               : (pending > 0 || partial > 0 ? "STANDARD_CHAIN_GATE_PARTIAL"
                                             : "STANDARD_CHAIN_GATE_PASS");

  {
    std::ofstream file(fs::path(outDir) / "standard_chain_gate_summary.json",
                       std::ios::binary);
    if (!file.is_open()) {
      reason = "failed to open standard_chain_gate_summary.json";
      return false;
    }

    file << "{\n"
         << "  \"run_id\": \"" << PipelineJsonEscape(runId) << "\",\n"
         << "  \"final_code\": \"" << finalCode << "\",\n"
         << "  \"pass_count\": " << pass << ",\n"
         << "  \"partial_count\": " << partial << ",\n"
         << "  \"pending_count\": " << pending << ",\n"
         << "  \"fail_count\": " << fail << ",\n"
         << "  \"gates\": [\n";
    for (std::size_t i = 0; i < rows.size(); ++i) {
      const auto &row = rows[i];
      file << "    {\n"
           << "      \"priority\": \"" << PipelineJsonEscape(row.priority)
           << "\",\n"
           << "      \"title\": \"" << PipelineJsonEscape(row.title) << "\",\n"
           << "      \"code\": \"" << PipelineJsonEscape(row.code) << "\",\n"
           << "      \"status\": \"" << PipelineJsonEscape(row.status)
           << "\",\n"
           << "      \"evidence\": \"" << PipelineJsonEscape(row.evidence)
           << "\",\n"
           << "      \"remaining\": \"" << PipelineJsonEscape(row.remaining)
           << "\",\n"
           << "      \"next_step\": \"" << PipelineJsonEscape(row.next_step)
           << "\"\n"
           << "    }" << (i + 1 < rows.size() ? "," : "") << "\n";
    }
    file << "  ]\n"
         << "}\n";
  }

  {
    std::ofstream file(fs::path(outDir) / "standard_chain_gate_report.md",
                       std::ios::binary);
    if (!file.is_open()) {
      reason = "failed to open standard_chain_gate_report.md";
      return false;
    }

    file << "# Standard Chain Gate Report\n\n";
    file << "- run_id: " << runId << "\n";
    file << "- final_code: " << finalCode << "\n";
    file << "- pass_count: " << pass << "\n";
    file << "- partial_count: " << partial << "\n";
    file << "- pending_count: " << pending << "\n";
    file << "- fail_count: " << fail << "\n\n";
    file << "| Priority | Title | Code | Status | Evidence | Remaining | Next "
            "Step |\n";
    file << "|---|---|---|---|---|---|---|\n";
    for (const auto &row : rows) {
      file << "| " << row.priority << " | " << row.title << " | " << row.code
           << " | " << row.status << " | " << row.evidence << " | "
           << row.remaining << " | " << row.next_step << " |\n";
    }
    file << "\n";
    file << "Promotion is blocked unless P0-P4 are PASS and human review "
            "assets are accepted.\n";
  }

  reason.clear();
  return true;
}

int RunStandardChainGateCli(const EvidenceChainSelfTestCliOptions &options) {
  std::cout << "[MAIN] standard chain gate mode begin\n" << std::flush;

  const std::string run_id = CxUnifiedLog::Instance().GenerateRunId();
  const std::string out_dir = options.out_dir.empty()
                                  ? "D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/"
                                    "cxscript_runs/standard_chain_gate/run_" +
                                        run_id
                                  : options.out_dir;

  EvidenceChainSelfTestCliOptions evidenceOptions = options;
  evidenceOptions.standard_chain_gate = false;
  evidenceOptions.evidence_lock_pipeline = true;
  evidenceOptions.enabled = true;
  evidenceOptions.out_dir =
      (std::filesystem::path(out_dir) / "evidence_lock_pipeline").string();

  const int evidenceExit = RunEvidenceLockPipelineCli(evidenceOptions);
  const bool evidencePass =
      evidenceExit == 0 &&
      std::filesystem::exists(std::filesystem::path(evidenceOptions.out_dir) /
                              "evidence_lock_pipeline_summary.json");

  EvidenceChainSelfTestCliOptions baselineOptions = options;
  baselineOptions.standard_chain_gate = false;
  baselineOptions.evidence_lock_pipeline = false;
  baselineOptions.baseline_lock = true;
  baselineOptions.enabled = true;
  baselineOptions.suite_path =
      baselineOptions.suite_path.empty()
          ? "cxparser/cxscript/module/cximage/stage25/suites/"
            "stage25_l1_l3_parameter_consistency.cxsc"
          : baselineOptions.suite_path;
  baselineOptions.image_manifest_path =
      baselineOptions.image_manifest_path.empty()
          ? "cxparser/cxscript/module/cximage/stage25/manifests/"
            "stage25_l1_l3_manifest.json"
          : baselineOptions.image_manifest_path;
  baselineOptions.out_dir =
      (std::filesystem::path(out_dir) / "baseline_lock").string();

  const int baselineExit = RunBaselineLockCli(baselineOptions);
  const bool baselinePass =
      baselineExit == 0 &&
      std::filesystem::exists(std::filesystem::path(baselineOptions.out_dir) /
                              "baseline_lock_summary.json");

  EvidenceChainSelfTestCliOptions paramOptions = options;
  paramOptions.standard_chain_gate = false;
  paramOptions.evidence_lock_pipeline = false;
  paramOptions.baseline_lock = false;
  paramOptions.param_regression_loop = true;
  paramOptions.enabled = true;
  paramOptions.suite_path =
      paramOptions.suite_path.empty()
          ? "cxparser/cxscript/module/cximage/stage25/suites/"
            "stage25_l1_l3_parameter_consistency.cxsc"
          : paramOptions.suite_path;
  paramOptions.image_manifest_path =
      paramOptions.image_manifest_path.empty()
          ? "cxparser/cxscript/module/cximage/stage25/manifests/"
            "stage25_l1_l3_manifest.json"
          : paramOptions.image_manifest_path;
  paramOptions.param_regression_tool =
      paramOptions.param_regression_tool.empty()
          ? "FindLine"
          : paramOptions.param_regression_tool;
  paramOptions.out_dir =
      (std::filesystem::path(out_dir) / "param_regression_loop").string();

  const int paramExit = RunParamRegressionLoopCli(paramOptions);
  const bool paramRegressionLoopPass =
      paramExit == 0 &&
      std::filesystem::exists(std::filesystem::path(paramOptions.out_dir) /
                              "param_regression_loop_summary.json");

  const std::filesystem::path hitJsonPath =
      std::filesystem::path(paramOptions.out_dir) / "hit_distribution.json";
  const std::filesystem::path hitReportPath =
      std::filesystem::path(paramOptions.out_dir) /
      "param_hit_distribution_report.md";
  const bool realHitDistribution =
      paramRegressionLoopPass && std::filesystem::exists(hitJsonPath) &&
      std::filesystem::exists(hitReportPath) &&
      PipelineFileContainsText(hitJsonPath,
                               "param_hit_distribution_runtime_v2") &&
      !PipelineFileContainsText(hitReportPath, "placeholder");
  const std::filesystem::path caseMatrixPath =
      std::filesystem::path(paramOptions.out_dir) /
      "candidate_case_matrix.json";
  const std::filesystem::path stabilityMatrixPath =
      std::filesystem::path(paramOptions.out_dir) / "stability_matrix.json";
  const bool crossCaseMatrixReady =
      paramRegressionLoopPass && std::filesystem::exists(caseMatrixPath) &&
      std::filesystem::exists(stabilityMatrixPath) &&
      PipelineFileContainsText(caseMatrixPath,
                               "candidate_case_matrix_runtime_v1") &&
      PipelineFileContainsText(stabilityMatrixPath,
                               "param_stability_runtime_v1") &&
      PipelineFileContainsText(stabilityMatrixPath,
                               "\"coverage_complete\": true");

  const bool realCapture =
      std::filesystem::exists("cximage/CxScriptRuntimeResultCapture.cpp") &&
      std::filesystem::exists("cximage/CxShapeOverlayRenderer.cpp");
  const bool promotionGate =
      std::filesystem::exists("cximage/CxParamRegressionRuntime.cpp");

  std::vector<StandardChainGateRow> rows;
  rows.push_back(
      {"P0", "固化三条标准链路",
       evidencePass ? "STANDARD_CHAIN_P0_PASS" : "STANDARD_CHAIN_P0_FAIL",
       evidencePass ? "PASS" : "FAIL",
       evidencePass ? "Evidence Lock Pipeline 18-case completed through "
                      "Manual/Runtime/Evidence selftest path."
                    : "Evidence Lock Pipeline failed or summary missing.",
       evidencePass ? "Manual GUI human review is still separate."
                    : "Fix pipeline before expanding tools.",
       evidencePass ? "Use unified request/result gate for every new tool."
                    : "Run --evidence-lock-pipeline and inspect failures."});
  rows.push_back(
      {"P1", "固定 L1/L2/L3 基线",
       baselinePass ? "STANDARD_CHAIN_P1_PASS" : "STANDARD_CHAIN_P1_FAIL",
       baselinePass ? "PASS" : "FAIL",
       baselinePass ? "baseline_lock_summary.json confirms all L1/L2/L3 cases "
                      "are locked."
                    : "baseline_lock_summary.json reports missing or "
                      "inconsistent L1/L2/L3 assets.",
       baselinePass ? "No remaining baseline lock blocker."
                    : "Some suite image_id/target_id/script/expected assets "
                      "are not locked.",
       baselinePass ? "Proceed to Candidate -> Probe -> EvalRecord -> Review."
                    : "Fix manifest target ROI entries or suite bindings, then "
                      "rerun --baseline-lock."});
  rows.push_back(
      {"P2", "完成参数候选循环",
       paramRegressionLoopPass ? "STANDARD_CHAIN_P2_PASS"
                               : "STANDARD_CHAIN_P2_FAIL",
       paramRegressionLoopPass ? "PASS" : "FAIL",
       paramRegressionLoopPass
           ? "param_regression_loop_summary.json confirms Candidate -> Probe "
             "-> EvalRecord -> Review-pending assets."
           : "Param regression loop failed or summary missing.",
       paramRegressionLoopPass
           ? "Mini-regression and human review are still separate."
           : "Fix --param-regression-loop before expanding candidate search.",
       paramRegressionLoopPass
           ? "Proceed to P3 real hit bins from runtime points."
           : "Run --param-regression-loop and inspect candidate case "
             "failures."});
  rows.push_back(
      {"P3", "实现真实命中分布",
       realHitDistribution ? "STANDARD_CHAIN_P3_PASS"
                           : (realCapture ? "STANDARD_CHAIN_P3_PARTIAL"
                                          : "STANDARD_CHAIN_P3_FAIL"),
       realHitDistribution ? "PASS" : (realCapture ? "PARTIAL" : "FAIL"),
       realHitDistribution
           ? "hit_distribution.json is generated from runtime measure_points "
             "shape snapshots."
           : (realCapture
                  ? "Runtime result capture exists but hit_distribution is not "
                    "generated from current runtime points."
                  : "Runtime capture missing."),
       realHitDistribution ? "No P3 exporter blocker; zero points remain an "
                             "algorithm/probe diagnostic if recorded."
                           : "Need runtime hit bins from result_summary "
                             "measure_points snapshots.",
       realHitDistribution
           ? "Proceed to P4 L2/L3 cross-case matrix."
           : "Populate hit bins from "
             "measure_points/valid_points/rejected_points snapshots."});
  rows.push_back(
      {"P4", "完成跨案例准确性与稳定性",
       crossCaseMatrixReady ? "STANDARD_CHAIN_P4_PASS"
                            : "STANDARD_CHAIN_P4_PENDING",
       crossCaseMatrixReady ? "PASS" : "PENDING",
       crossCaseMatrixReady
           ? "candidate_case_matrix.json and stability_matrix.json cover "
             "locked L1/L2/L3 cases with real EvalRecord rows."
           : "No complete locked L1/L2/L3 candidate-case stability matrix from "
             "current run.",
       crossCaseMatrixReady
           ? "Human review is still required before promotion."
           : "Need cross-case matrix with geometry/evidence/human decisions.",
       crossCaseMatrixReady
           ? "Proceed to P5/P6 advisor binding without unlocking promotion."
           : "Run L2 mini-regression on locked L1/L2/L3 baselines."});
  rows.push_back({"P5", "接入真实模型排序", "STANDARD_CHAIN_P5_PENDING",
                  "PENDING",
                  "Current ranking is rule/placeholder style; no verified "
                  "historical model scorer.",
                  "Need model input schema and historical eval records.",
                  "Bind mlpack rank only as advisor; do not auto-promote."});
  rows.push_back({"P6", "接入真实优化目标", "STANDARD_CHAIN_P6_PENDING",
                  "PENDING",
                  "No verified ensmallen objective producing bounded "
                  "candidates in this run.",
                  "Need objective from real EvalRecord metrics.",
                  "Optimizer may output candidates only; formal params remain "
                  "locked until review."});
  rows.push_back({"P7", "统一模型结果和证据链", "STANDARD_CHAIN_P7_PARTIAL",
                  "PARTIAL",
                  "Torch/FastMatch result fields are entering Runtime/Evidence "
                  "structures.",
                  "Need every model result to publish Shape/Overlay/Evidence "
                  "references through one projector.",
                  "Add result_projector contract per model tool."});
  rows.push_back(
      {"P8", "打通 Mini Regression 和 Promotion Gate",
       promotionGate ? "STANDARD_CHAIN_P8_PARTIAL" : "STANDARD_CHAIN_P8_FAIL",
       promotionGate ? "PARTIAL" : "FAIL",
       promotionGate ? "Promotion gate report exists in param regression "
                       "exporter as blocked-by-default."
                     : "Promotion gate output missing.",
       "Need real L2/L3 pass matrix plus accepted human review before "
       "promotion_allowed=true.",
       "Keep promotion blocked until P0-P4 PASS."});

  std::string reason;
  if (!WriteStandardChainGateReports(out_dir, run_id, rows, reason)) {
    std::cout << "[MAIN] standard chain gate report failed: " << reason << "\n";
    return 2;
  }

  int pass = 0;
  int fail = 0;
  int notPass = 0;
  for (const auto &row : rows) {
    if (row.status == "FAIL")
      ++fail;
    if (row.status == "PASS")
      ++pass;
    else
      ++notPass;
  }

  std::cout << "[MAIN] standard chain gate mode end\n";
  std::cout << "standard_chain_gate_ok=" << (notPass == 0 ? "true" : "false")
            << "\n";
  std::cout << "standard_chain_gate_pass_count=" << pass << "\n";
  std::cout << "standard_chain_gate_not_pass_count=" << notPass << "\n";
  std::cout << "summary="
            << (std::filesystem::path(out_dir) /
                "standard_chain_gate_summary.json")
                   .string()
            << "\n";
  std::cout << "report="
            << (std::filesystem::path(out_dir) /
                "standard_chain_gate_report.md")
                   .string()
            << "\n";
  return notPass == 0 ? 0 : 1;
}

std::string PipelineFileHash64(const std::string &path) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open())
    return std::string();

  std::uint64_t hash = 1469598103934665603ull;
  char c = 0;
  while (file.get(c)) {
    hash ^= static_cast<unsigned char>(c);
    hash *= 1099511628211ull;
  }

  std::ostringstream oss;
  oss << "fnv1a64:" << std::hex << std::setw(16) << std::setfill('0') << hash;
  return oss.str();
}

const CxScriptCatalogEntry *
PipelineFindCatalogEntry(const CxScriptCatalogRuntime &catalog,
                         const std::string &script_id) {
  for (const auto &entry : catalog.scripts) {
    if (entry.script_id == script_id)
      return &entry;
  }
  return nullptr;
}

std::string PipelineTargetGeometryJson(const CxScriptImageTargetRoi &target) {
  std::ostringstream oss;
  oss << "{";
  oss << "\"tool\":\"" << PipelineJsonEscape(target.tool) << "\",";
  oss << "\"target_id\":\"" << PipelineJsonEscape(target.target_id) << "\"";
  if (target.has_line) {
    oss << ",\"x0\":" << target.x0 << ",\"y0\":" << target.y0
        << ",\"x1\":" << target.x1 << ",\"y1\":" << target.y1
        << ",\"tool_half_width\":" << target.tool_half_width;
  }
  if (target.has_circle) {
    oss << ",\"cx\":" << target.cx << ",\"cy\":" << target.cy
        << ",\"px\":" << target.px << ",\"py\":" << target.py;
  }
  if (target.has_ellipse) {
    oss << ",\"ellipse_major_radius\":" << target.ellipse_major_radius
        << ",\"ellipse_minor_radius\":" << target.ellipse_minor_radius
        << ",\"ellipse_angle_deg\":" << target.ellipse_angle_deg;
  }
  if (target.has_rect) {
    oss << ",\"rect_width\":" << target.rect_width
        << ",\"rect_height\":" << target.rect_height
        << ",\"rect_angle_deg\":" << target.rect_angle_deg;
  }
  oss << ",\"wgap\":" << target.wgap << ",\"hgap\":" << target.hgap
      << ",\"gap\":" << target.gap << ",\"linegap\":" << target.linegap
      << ",\"threshold\":" << target.threshold
      << ",\"method\":" << target.method;
  oss << "}";
  return oss.str();
}

bool PipelineWriteBaselineLockReports(
    const std::string &outDir, const std::string &runId,
    const std::string &suitePath, const std::string &manifestPath,
    const CxScriptSuiteRuntime &suite, const CxScriptCatalogRuntime &catalog,
    const CxScriptImageManifestRuntime &manifest, bool &lockPass,
    std::string &reason) {
  namespace fs = std::filesystem;
  const fs::path out(outDir);
  fs::create_directories(out);

  struct Row {
    std::string case_id;
    std::string level;
    std::string image_id;
    std::string target_id;
    std::string tool;
    std::string script_id;
    std::string script_path;
    std::string image_path;
    std::string image_hash;
    std::string script_hash;
    std::string target_json;
    std::string expected_result;
    std::string expected_policy_guard;
    std::string contract_path;
    std::string status;
    std::string reason;
  };

  std::vector<Row> rows;
  int pass = 0;
  int fail = 0;

  for (const auto &c : suite.cases) {
    Row row;
    row.case_id = c.case_id;
    row.level = c.level;
    row.image_id = c.image_id;
    row.target_id = c.target_id;
    row.script_id = c.script_id;
    row.expected_result = c.expected_result;
    row.expected_policy_guard = c.expected_policy_guard;
    row.status = "PASS";

    const CxScriptCatalogEntry *entry =
        PipelineFindCatalogEntry(catalog, c.script_id);
    if (!entry) {
      row.status = "FAIL";
      row.reason = "script_id not found in catalog";
    } else {
      row.tool = entry->tool;
      row.script_path = entry->path;
      row.contract_path = entry->contract_path;
      row.script_hash = PipelineFileHash64(entry->path);
      if (row.script_hash.empty()) {
        row.status = "FAIL";
        row.reason = "script file missing or unreadable";
      }
    }

    const CxScriptImageManifestEntry *image =
        FindImageById(manifest, c.image_id);
    if (!image) {
      row.status = "FAIL";
      if (!row.reason.empty())
        row.reason += "; ";
      row.reason += "image_id not found in manifest";
    } else {
      row.image_path = image->path;
      row.image_hash = !image->sha256.empty()
                           ? ("manifest_sha256:" + image->sha256)
                           : PipelineFileHash64(image->path);
      if (row.image_hash.empty()) {
        row.status = "FAIL";
        if (!row.reason.empty())
          row.reason += "; ";
        row.reason += "image file missing or unreadable";
      }
    }

    const CxScriptImageTargetRoi *target =
        FindTargetRoiByImageAndTargetId(manifest, c.image_id, c.target_id);
    if (!target) {
      row.status = "FAIL";
      if (!row.reason.empty())
        row.reason += "; ";
      row.reason += "target_id not found in manifest image";
    } else {
      row.target_json = PipelineTargetGeometryJson(*target);
      if (!row.tool.empty() && !target->tool.empty() &&
          row.tool != target->tool) {
        row.status = "FAIL";
        if (!row.reason.empty())
          row.reason += "; ";
        row.reason += "catalog tool and manifest target tool mismatch";
      }
    }

    if (row.expected_result.empty() || row.expected_policy_guard.empty()) {
      row.status = "FAIL";
      if (!row.reason.empty())
        row.reason += "; ";
      row.reason += "expected result or policy guard missing";
    }

    if (row.status == "PASS") {
      row.reason = "baseline case is locked";
      ++pass;
    } else {
      ++fail;
    }
    rows.push_back(row);
  }

  const std::string finalCode =
      fail == 0 ? "BASELINE_LOCK_PASS" : "BASELINE_LOCK_FAIL";
  lockPass = fail == 0;

  {
    std::ofstream file(out / "baseline_lock_summary.json", std::ios::binary);
    if (!file.is_open()) {
      reason = "failed to open baseline_lock_summary.json";
      return false;
    }

    file << "{\n";
    file << "  \"run_id\": \"" << PipelineJsonEscape(runId) << "\",\n";
    file << "  \"final_code\": \"" << finalCode << "\",\n";
    file << "  \"suite_path\": \"" << PipelineJsonEscape(suitePath) << "\",\n";
    file << "  \"manifest_path\": \"" << PipelineJsonEscape(manifestPath)
         << "\",\n";
    file << "  \"suite_id\": \"" << PipelineJsonEscape(suite.suite_id)
         << "\",\n";
    file << "  \"catalog_path\": \"" << PipelineJsonEscape(suite.catalog_path)
         << "\",\n";
    file << "  \"total_cases\": " << rows.size() << ",\n";
    file << "  \"pass_count\": " << pass << ",\n";
    file << "  \"fail_count\": " << fail << ",\n";
    file << "  \"cases\": [\n";
    for (std::size_t i = 0; i < rows.size(); ++i) {
      const auto &r = rows[i];
      file << "    {\n";
      file << "      \"case_id\": \"" << PipelineJsonEscape(r.case_id)
           << "\",\n";
      file << "      \"level\": \"" << PipelineJsonEscape(r.level) << "\",\n";
      file << "      \"image_id\": \"" << PipelineJsonEscape(r.image_id)
           << "\",\n";
      file << "      \"target_id\": \"" << PipelineJsonEscape(r.target_id)
           << "\",\n";
      file << "      \"tool\": \"" << PipelineJsonEscape(r.tool) << "\",\n";
      file << "      \"script_id\": \"" << PipelineJsonEscape(r.script_id)
           << "\",\n";
      file << "      \"script_path\": \"" << PipelineJsonEscape(r.script_path)
           << "\",\n";
      file << "      \"script_hash\": \"" << PipelineJsonEscape(r.script_hash)
           << "\",\n";
      file << "      \"image_path\": \"" << PipelineJsonEscape(r.image_path)
           << "\",\n";
      file << "      \"image_hash\": \"" << PipelineJsonEscape(r.image_hash)
           << "\",\n";
      file << "      \"target_geometry\": "
           << (r.target_json.empty() ? "{}" : r.target_json) << ",\n";
      file << "      \"expected_result\": \""
           << PipelineJsonEscape(r.expected_result) << "\",\n";
      file << "      \"expected_policy_guard\": \""
           << PipelineJsonEscape(r.expected_policy_guard) << "\",\n";
      file << "      \"contract_path\": \""
           << PipelineJsonEscape(r.contract_path) << "\",\n";
      file << "      \"status\": \"" << r.status << "\",\n";
      file << "      \"reason\": \"" << PipelineJsonEscape(r.reason) << "\"\n";
      file << "    }" << (i + 1 < rows.size() ? "," : "") << "\n";
    }
    file << "  ]\n";
    file << "}\n";
  }

  {
    std::ofstream file(out / "baseline_lock_report.md", std::ios::binary);
    if (!file.is_open()) {
      reason = "failed to open baseline_lock_report.md";
      return false;
    }

    file << "# Baseline Lock Report\n\n";
    file << "- run_id: " << runId << "\n";
    file << "- final_code: " << finalCode << "\n";
    file << "- suite_path: " << suitePath << "\n";
    file << "- manifest_path: " << manifestPath << "\n";
    file << "- total_cases: " << rows.size() << "\n";
    file << "- pass_count: " << pass << "\n";
    file << "- fail_count: " << fail << "\n\n";
    file << "| Case | Level | Tool | Image | Target | Script | Expected | "
            "Status | Reason |\n";
    file << "|---|---|---|---|---|---|---|---|---|\n";
    for (const auto &r : rows) {
      file << "| " << r.case_id << " | " << r.level << " | " << r.tool << " | "
           << r.image_id << " | " << r.target_id << " | " << r.script_id
           << " | " << r.expected_policy_guard << " | " << r.status << " | "
           << r.reason << " |\n";
    }
  }

  reason.clear();
  return true;
}

int RunBaselineLockCli(const EvidenceChainSelfTestCliOptions &options) {
  const std::string suitePath =
      options.suite_path.empty()
          ? "cxparser/cxscript/module/cximage/stage25/suites/"
            "stage25_l1_l3_parameter_consistency.cxsc"
          : options.suite_path;
  const std::string manifestPath =
      options.image_manifest_path.empty()
          ? "cxparser/cxscript/module/cximage/stage25/manifests/"
            "stage25_l1_l3_manifest.json"
          : options.image_manifest_path;
  const std::string runId = CxUnifiedLog::Instance().GenerateRunId();
  const std::string outDir = options.out_dir.empty()
                                 ? "D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/"
                                   "cxscript_runs/baseline_lock/run_" +
                                       runId
                                 : options.out_dir;

  std::string reason;
  CxParserRuntimeOwner owner;
  if (!owner.Initialize(reason)) {
    std::cout << "baseline_lock_ok=false\nreason=" << reason << "\n";
    return 2;
  }

  CxScriptSuiteRuntime suite;
  if (!owner.ParseScriptSuite(suitePath, suite, reason)) {
    std::cout << "baseline_lock_ok=false\nreason=" << reason << "\n";
    return 2;
  }

  CxScriptCatalogRuntime catalog;
  const std::string catalogPath =
      suite.catalog_path.empty()
          ? "cxparser/cxscript/module/cximage/catalog/cximage_catalog.cxsc"
          : suite.catalog_path;
  if (!owner.ParseScriptCatalog(catalogPath, catalog, reason)) {
    std::cout << "baseline_lock_ok=false\nreason=" << reason << "\n";
    return 2;
  }

  CxScriptImageManifestRuntime manifest;
  if (!LoadStage25ImageManifestJson(manifestPath, manifest, reason)) {
    std::cout << "baseline_lock_ok=false\nreason=" << reason << "\n";
    return 2;
  }

  bool lockPass = false;
  if (!PipelineWriteBaselineLockReports(outDir, runId, suitePath, manifestPath,
                                        suite, catalog, manifest, lockPass,
                                        reason)) {
    std::cout << "baseline_lock_ok=false\nreason=" << reason << "\n";
    return 2;
  }

  const std::filesystem::path summaryPath =
      std::filesystem::path(outDir) / "baseline_lock_summary.json";
  const std::filesystem::path reportPath =
      std::filesystem::path(outDir) / "baseline_lock_report.md";
  std::cout << "baseline_lock_ok=" << (lockPass ? "true" : "false") << "\n";
  std::cout << "summary=" << summaryPath.string() << "\n";
  std::cout << "report=" << reportPath.string() << "\n";
  return lockPass ? 0 : 1;
}

namespace {
struct ParamRegressionLoopRow {
  std::string candidate_id;
  std::string case_id;
  std::string probe_dir;
  bool launched = false;
  bool executed = false;
  bool assets_complete = false;
  bool timeout = false;
  bool fit_available = false;
  int points = 0;
  double mean_distance = 0.0;
  std::string failure_stage;
  std::string status;
  std::string reason;
};

struct ParamRegressionLockedCase {
  const CxScriptSuiteCase *suite_case = nullptr;
  const CxScriptCatalogEntry *catalog_entry = nullptr;
  const CxScriptImageManifestEntry *image = nullptr;
  const CxScriptImageTargetRoi *target = nullptr;
};

std::string PipelineCaseLevel(const std::string &case_id) {
  if (case_id.rfind("L1_", 0) == 0)
    return "L1";
  if (case_id.rfind("L2_", 0) == 0)
    return "L2";
  if (case_id.rfind("L3_", 0) == 0)
    return "L3";
  if (case_id.rfind("L0_", 0) == 0)
    return "L0";
  return "unknown";
}

void PipelineApplyTargetToCandidate(const CxScriptImageTargetRoi &target,
                                    CxParamCandidate &candidate) {
  if (target.has_method)
    candidate.method = target.method;
  if (target.has_threshold)
    candidate.threshold = target.threshold;
  if (target.has_gap)
    candidate.gap = target.gap;
  if (target.has_linegap)
    candidate.linegap = target.linegap;
  if (target.has_min_edge_run_width_px)
    candidate.min_edge_run_width_px = target.min_edge_run_width_px;
  if (target.has_wgap)
    candidate.wgap = target.wgap;
  if (target.has_hgap)
    candidate.hgap = target.hgap;
}

void PipelineFillProbeGeometry(const CxScriptImageTargetRoi &target,
                               CxParamProbeRequest &request) {
  request.roi_x0 = target.x0;
  request.roi_y0 = target.y0;
  request.roi_x1 = target.x1;
  request.roi_y1 = target.y1;
  request.circle_cx = target.cx;
  request.circle_cy = target.cy;
  request.circle_px = target.px;
  request.circle_py = target.py;
  request.ellipse_x0 = target.x0;
  request.ellipse_y0 = target.y0;
  request.ellipse_x1 = target.x1;
  request.ellipse_y1 = target.y1;
  request.tool_half_width = target.tool_half_width;
}

bool PipelineWriteParamRegressionLoopReports(
    const std::string &outDir, const std::string &runId,
    const std::string &suitePath, const std::string &manifestPath,
    const std::string &paramScript, const CxParamRegressionTask &task,
    const std::vector<CxParamCandidate> &candidates,
    const std::vector<CxParamEvalRecord> &records,
    const std::vector<ParamRegressionLoopRow> &rows, bool pass,
    std::string &reason) {
  namespace fs = std::filesystem;
  fs::create_directories(outDir);
  const fs::path out(outDir);

  {
    std::ofstream file(out / "param_regression_loop_summary.json",
                       std::ios::binary);
    if (!file.is_open()) {
      reason = "failed to open param_regression_loop_summary.json";
      return false;
    }

    int rowPass = 0;
    int rowFail = 0;
    for (const auto &row : rows) {
      if (row.status == "PASS")
        ++rowPass;
      else
        ++rowFail;
    }

    file << "{\n";
    file << "  \"run_id\": \"" << PipelineJsonEscape(runId) << "\",\n";
    file << "  \"final_code\": \""
         << (pass ? "PARAM_REGRESSION_LOOP_PASS" : "PARAM_REGRESSION_LOOP_FAIL")
         << "\",\n";
    file << "  \"suite_path\": \"" << PipelineJsonEscape(suitePath) << "\",\n";
    file << "  \"manifest_path\": \"" << PipelineJsonEscape(manifestPath)
         << "\",\n";
    file << "  \"param_regression_script\": \""
         << PipelineJsonEscape(paramScript) << "\",\n";
    file << "  \"task_id\": \"" << PipelineJsonEscape(task.task_id) << "\",\n";
    file << "  \"tool\": \"" << PipelineJsonEscape(task.tool) << "\",\n";
    file << "  \"case_id\": \"" << PipelineJsonEscape(task.case_id) << "\",\n";
    file << "  \"image_id\": \"" << PipelineJsonEscape(task.image_id)
         << "\",\n";
    file << "  \"target_id\": \"" << PipelineJsonEscape(task.target_id)
         << "\",\n";
    file << "  \"candidate_count\": " << candidates.size() << ",\n";
    file << "  \"eval_record_count\": " << records.size() << ",\n";
    file << "  \"probe_pass_count\": " << rowPass << ",\n";
    file << "  \"probe_fail_count\": " << rowFail << ",\n";
    file << "  \"review_status\": \"PENDING_HUMAN_REVIEW\",\n";
    file << "  \"promotion_allowed\": false,\n";
    file << "  \"cases\": [\n";
    for (std::size_t i = 0; i < rows.size(); ++i) {
      const auto &row = rows[i];
      file << "    {\n";
      file << "      \"candidate_id\": \""
           << PipelineJsonEscape(row.candidate_id) << "\",\n";
      file << "      \"case_id\": \"" << PipelineJsonEscape(row.case_id)
           << "\",\n";
      file << "      \"probe_dir\": \"" << PipelineJsonEscape(row.probe_dir)
           << "\",\n";
      file << "      \"launched\": " << (row.launched ? "true" : "false")
           << ",\n";
      file << "      \"executed\": " << (row.executed ? "true" : "false")
           << ",\n";
      file << "      \"assets_complete\": "
           << (row.assets_complete ? "true" : "false") << ",\n";
      file << "      \"timeout\": " << (row.timeout ? "true" : "false")
           << ",\n";
      file << "      \"points\": " << row.points << ",\n";
      file << "      \"fit_available\": "
           << (row.fit_available ? "true" : "false") << ",\n";
      file << "      \"mean_distance\": " << row.mean_distance << ",\n";
      file << "      \"failure_stage\": \""
           << PipelineJsonEscape(row.failure_stage) << "\",\n";
      file << "      \"status\": \"" << PipelineJsonEscape(row.status)
           << "\",\n";
      file << "      \"reason\": \"" << PipelineJsonEscape(row.reason)
           << "\"\n";
      file << "    }" << (i + 1 < rows.size() ? "," : "") << "\n";
    }
    file << "  ]\n";
    file << "}\n";
  }

  {
    std::ofstream file(out / "param_regression_loop_report.md",
                       std::ios::binary);
    if (!file.is_open()) {
      reason = "failed to open param_regression_loop_report.md";
      return false;
    }

    file << "# Parameter Regression Loop Report\n\n";
    file << "- run_id: " << runId << "\n";
    file << "- final_code: "
         << (pass ? "PARAM_REGRESSION_LOOP_PASS" : "PARAM_REGRESSION_LOOP_FAIL")
         << "\n";
    file << "- suite_path: " << suitePath << "\n";
    file << "- manifest_path: " << manifestPath << "\n";
    file << "- param_regression_script: " << paramScript << "\n";
    file << "- tool: " << task.tool << "\n";
    file << "- image_id: " << task.image_id << "\n";
    file << "- target_id: " << task.target_id << "\n";
    file << "- review_status: PENDING_HUMAN_REVIEW\n";
    file << "- promotion_allowed: false\n\n";
    file << "| Candidate | Case | Launched | Executed | Assets | Timeout | "
            "Points | Fit | MeanDist | Status | Reason |\n";
    file << "|---|---|---|---|---|---|---:|---|---:|---|---|\n";
    for (const auto &row : rows) {
      file << "| " << row.candidate_id << " | " << row.case_id << " | "
           << (row.launched ? "yes" : "no") << " | "
           << (row.executed ? "yes" : "no") << " | "
           << (row.assets_complete ? "yes" : "no") << " | "
           << (row.timeout ? "yes" : "no") << " | " << row.points << " | "
           << (row.fit_available ? "yes" : "no") << " | " << row.mean_distance
           << " | " << row.status << " | " << row.reason << " |\n";
    }
    file << "\n";
    file << "This report verifies the P2 chain only: Candidate -> Probe -> "
            "EvalRecord -> Review-pending.\n";
    file << "It does not promote parameters and does not replace L2/L3 "
            "stability or human review.\n";
  }

  reason.clear();
  return true;
}
} // namespace

int RunParamRegressionLoopCli(const EvidenceChainSelfTestCliOptions &options) {
  namespace fs = std::filesystem;
  std::cout << "[MAIN] param regression loop mode begin\n" << std::flush;

  const std::string tool = options.param_regression_tool.empty()
                               ? "FindLine"
                               : options.param_regression_tool;
  const std::string suitePath =
      options.suite_path.empty()
          ? "cxparser/cxscript/module/cximage/stage25/suites/"
            "stage25_l1_l3_parameter_consistency.cxsc"
          : options.suite_path;
  const std::string manifestPath =
      options.image_manifest_path.empty()
          ? "cxparser/cxscript/module/cximage/stage25/manifests/"
            "stage25_l1_l3_manifest.json"
          : options.image_manifest_path;
  const std::string paramScript =
      options.param_regression_script.empty()
          ? (tool == "FindCircle"
                 ? "cxparser/cxscript/module/cximage/stage25/param_regression/"
                   "ranges/findcircle_range_conservative.cxsc"
                 : "cxparser/cxscript/module/cximage/stage25/param_regression/"
                   "ranges/findline_range_conservative.cxsc")
          : options.param_regression_script;
  const std::string runId = CxUnifiedLog::Instance().GenerateRunId();
  const std::string outDir = options.out_dir.empty()
                                 ? ("D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/"
                                    "cxscript_runs/param_regression_loop/run_" +
                                    runId)
                                 : options.out_dir;

  std::string reason;
  CxParserRuntimeOwner owner;
  if (!owner.Initialize(reason)) {
    std::cout << "param_regression_loop_ok=false\nreason=" << reason << "\n";
    return 2;
  }

  CxScriptSuiteRuntime suite;
  if (!owner.ParseScriptSuite(suitePath, suite, reason)) {
    std::cout << "param_regression_loop_ok=false\nreason=" << reason << "\n";
    return 2;
  }

  CxScriptCatalogRuntime catalog;
  const std::string catalogPath =
      suite.catalog_path.empty()
          ? "cxparser/cxscript/module/cximage/catalog/cximage_catalog.cxsc"
          : suite.catalog_path;
  if (!owner.ParseScriptCatalog(catalogPath, catalog, reason)) {
    std::cout << "param_regression_loop_ok=false\nreason=" << reason << "\n";
    return 2;
  }

  CxScriptImageManifestRuntime manifest;
  if (!LoadStage25ImageManifestJson(manifestPath, manifest, reason)) {
    std::cout << "param_regression_loop_ok=false\nreason=" << reason << "\n";
    return 2;
  }

  std::vector<ParamRegressionLockedCase> matchingCases;

  for (const auto &c : suite.cases) {
    const CxScriptCatalogEntry *entry =
        PipelineFindCatalogEntry(catalog, c.script_id);
    if (!entry || entry->tool != tool)
      continue;
    const CxScriptImageManifestEntry *image =
        FindImageById(manifest, c.image_id);
    const CxScriptImageTargetRoi *target =
        FindTargetRoiByImageAndTargetId(manifest, c.image_id, c.target_id);
    if (!image || !target || target->tool != tool)
      continue;
    ParamRegressionLockedCase locked;
    locked.suite_case = &c;
    locked.catalog_entry = entry;
    locked.image = image;
    locked.target = target;
    matchingCases.push_back(locked);
  }

  if (matchingCases.empty()) {
    std::cout << "param_regression_loop_ok=false\nreason=no locked baseline "
                 "case found for tool="
              << tool << "\n";
    return 2;
  }

  const int requestedCaseLimit = options.max_cases > 0 ? options.max_cases : 3;
  std::vector<ParamRegressionLockedCase> selectedCases;
  auto addFirstLevel = [&](const std::string &level) {
    if (static_cast<int>(selectedCases.size()) >= requestedCaseLimit)
      return;
    for (const auto &locked : matchingCases) {
      if (!locked.suite_case)
        continue;
      if (PipelineCaseLevel(locked.suite_case->case_id) != level)
        continue;
      bool exists = false;
      for (const auto &current : selectedCases) {
        if (current.suite_case &&
            current.suite_case->case_id == locked.suite_case->case_id) {
          exists = true;
          break;
        }
      }
      if (!exists) {
        selectedCases.push_back(locked);
        return;
      }
    }
  };
  addFirstLevel("L1");
  addFirstLevel("L2");
  addFirstLevel("L3");
  for (const auto &locked : matchingCases) {
    if (static_cast<int>(selectedCases.size()) >= requestedCaseLimit)
      break;
    bool exists = false;
    for (const auto &current : selectedCases) {
      if (current.suite_case && locked.suite_case &&
          current.suite_case->case_id == locked.suite_case->case_id) {
        exists = true;
        break;
      }
    }
    if (!exists)
      selectedCases.push_back(locked);
  }

  const auto &anchor = selectedCases.front();

  CxParamRegressionRuntime runtime;
  if (!LoadCxParamRegressionFile(paramScript, runtime, reason)) {
    std::cout << "param_regression_loop_ok=false\nreason=" << reason << "\n";
    return 2;
  }

  runtime.task.task_id = runtime.task.task_id.empty()
                             ? ("p2_param_regression_loop_" + tool)
                             : runtime.task.task_id;
  runtime.task.case_id = anchor.suite_case ? anchor.suite_case->case_id : "";
  runtime.task.image_id = anchor.suite_case ? anchor.suite_case->image_id : "";
  runtime.task.target_id =
      anchor.suite_case ? anchor.suite_case->target_id : "";
  runtime.task.tool = tool;
  runtime.task.base_script_id =
      anchor.suite_case ? anchor.suite_case->script_id : "";
  runtime.task.base_parameter_profile_id =
      anchor.suite_case ? anchor.suite_case->expected_policy_guard : "";
  runtime.task.require_manual_gauge = false;
  runtime.task.allow_promote = false;
  runtime.task.max_case_seconds = runtime.range_set.max_case_seconds;
  runtime.task.max_total_seconds = runtime.range_set.max_total_seconds;

  if (runtime.range_set.tool.empty())
    runtime.range_set = MakeConservativeRangeSet(tool);

  if (runtime.candidates.empty()) {
    runtime.candidates = GenerateBasicParamCandidates(
        runtime.range_set, runtime.range_set.max_candidates);
  }

  if (!runtime.candidates.empty()) {
    PipelineApplyTargetToCandidate(*anchor.target, runtime.candidates[0]);
    runtime.candidates[0].candidate_id = "manual_seed_locked";
    runtime.candidates[0].source = "locked_baseline";
    runtime.candidates[0].selected_for_probe = true;
  }

  int probedCandidates = 0;
  std::vector<ParamRegressionLoopRow> rows;
  runtime.records.clear();

  for (const auto &candidate : runtime.candidates) {
    if (!candidate.selected_for_probe)
      continue;
    if (probedCandidates >= 1)
      break;
    ++probedCandidates;

    for (const auto &locked : selectedCases) {
      if (!locked.suite_case || !locked.catalog_entry || !locked.image ||
          !locked.target)
        continue;

      CxParamCandidate caseCandidate = candidate;
      if (caseCandidate.source == "locked_baseline")
        PipelineApplyTargetToCandidate(*locked.target, caseCandidate);

      CxParamRegressionTask caseTask = runtime.task;
      caseTask.case_id = locked.suite_case->case_id;
      caseTask.image_id = locked.suite_case->image_id;
      caseTask.target_id = locked.suite_case->target_id;
      caseTask.base_script_id = locked.suite_case->script_id;
      caseTask.base_parameter_profile_id =
          locked.suite_case->expected_policy_guard;

      CxParamProbeRequest probe;
      probe.task = caseTask;
      probe.candidate = caseCandidate;
      probe.image_path = locked.image->path;
      probe.target_id = locked.suite_case->target_id;
      probe.script_path = locked.catalog_entry->path;
      probe.contract_path = locked.catalog_entry->contract_path;
      probe.out_dir = (fs::path(outDir) / "cases" / caseCandidate.candidate_id /
                       locked.suite_case->case_id)
                          .string();
      probe.timeout_seconds = runtime.task.max_case_seconds > 0
                                  ? runtime.task.max_case_seconds
                                  : 10;
      probe.max_elapsed_ms = 2000;
      probe.max_scan_lines = 512;
      probe.max_samples = 20000;
      PipelineFillProbeGeometry(*locked.target, probe);

      CxParamProbeResult probeResult;
      RunSingleParamProbe(probe, probeResult);

      CxParamEvalRecord record;
      record.candidate_id = caseCandidate.candidate_id;
      record.case_id = caseTask.case_id;
      record.tool = caseTask.tool;
      record.executed = probeResult.executed;
      record.timeout = probeResult.timeout;
      record.points = probeResult.candidate_points;
      record.fit_available = probeResult.fit_available;
      record.support_score = probeResult.support_score;
      record.mean_distance = probeResult.mean_distance;
      record.fit_offset = probeResult.fit_offset;
      record.failure_stage = probeResult.failure_stage;

      record.classification = probeResult.fit_available
                                  ? "geometry_available"
                                  : "geometry_unavailable";
      record.result_summary_path = probeResult.result_summary_path;
      record.tool_display_path = probeResult.tool_display_path;
      record.replay_package_path = probe.out_dir;
      record.image_id = caseTask.image_id;
      record.target_id = caseTask.target_id;
      record.image_path = probe.image_path;
      record.script_path = probe.script_path;
      record.contract_path = probe.contract_path;
      record.timeout_seconds = probe.timeout_seconds;
      record.roi_x0 = probe.roi_x0;
      record.roi_y0 = probe.roi_y0;
      record.roi_x1 = probe.roi_x1;
      record.roi_y1 = probe.roi_y1;
      record.tool_half_width = probe.tool_half_width;
      record.max_elapsed_ms = probe.max_elapsed_ms;
      record.max_scan_lines = probe.max_scan_lines;
      record.max_samples = probe.max_samples;
      record.method = caseCandidate.method;
      record.threshold = caseCandidate.threshold;
      record.gap = caseCandidate.gap;
      record.linegap = caseCandidate.linegap;
      record.wgap = caseCandidate.wgap;
      record.hgap = caseCandidate.hgap;
      record.filterprofile = caseCandidate.filterprofile;
      record.samplerate = caseCandidate.samplerate;
      record.min_score = caseCandidate.min_score;
      record.find_num = caseCandidate.find_num;
      record.compare_gap = caseCandidate.compare_gap;

      runtime.records.push_back(record);

      ParamRegressionLoopRow row;
      row.candidate_id = caseCandidate.candidate_id;
      row.case_id = caseTask.case_id;
      row.probe_dir = probe.out_dir;
      row.launched = probeResult.launched;
      row.executed = probeResult.executed;
      row.assets_complete = probeResult.assets_complete;
      row.timeout = probeResult.timeout;
      row.fit_available = probeResult.fit_available;
      row.points = probeResult.candidate_points;
      row.mean_distance = probeResult.mean_distance;
      row.failure_stage = probeResult.failure_stage;
      row.status = (probeResult.launched && probeResult.executed &&
                    probeResult.assets_complete && !probeResult.timeout)
                       ? "PASS"
                       : "FAIL";
      row.reason = probeResult.reason;
      rows.push_back(row);
    }
  }

  std::vector<CxParamAccuracyStats> stats;
  std::map<std::string, int> metric_counts;
  for (const auto &record : runtime.records) {
    CxParamAccuracyStats *stat = nullptr;
    for (auto &existing : stats) {
      if (existing.candidate_id == record.candidate_id) {
        stat = &existing;
        break;
      }
    }
    if (!stat) {
      CxParamAccuracyStats s;
      s.candidate_id = record.candidate_id;
      s.tool = record.tool;
      stats.push_back(s);
      stat = &stats.back();
    }

    stat->total_cases += 1;
    stat->executed_cases += record.executed ? 1 : 0;
    stat->timeout_cases += record.timeout ? 1 : 0;
    stat->geometry_pass += record.fit_available ? 1 : 0;
    stat->evidence_pass += record.points > 0 ? 1 : 0;
    if (record.fit_available && record.points > 0) {
      stat->avg_support_score += record.support_score;
      stat->avg_mean_distance += record.mean_distance;
      stat->avg_fit_offset += record.fit_offset;
      metric_counts[record.candidate_id] += 1;
    }
  }

  for (auto &s : stats) {
    const double total =
        s.total_cases > 0 ? static_cast<double>(s.total_cases) : 1.0;
    const int metric_count = metric_counts[s.candidate_id];
    const double metric_total =
        metric_count > 0 ? static_cast<double>(metric_count) : 1.0;
    s.geometry_pass_rate = static_cast<double>(s.geometry_pass) / total;
    s.evidence_pass_rate = static_cast<double>(s.evidence_pass) / total;
    s.human_accept_rate =
        s.total_cases > 0 ? static_cast<double>(s.human_accept) / total : 0.0;
    s.avg_support_score /= metric_total;
    s.avg_mean_distance /= metric_total;
    s.avg_fit_offset /= metric_total;
    const double timeout_penalty = static_cast<double>(s.timeout_cases) / total;
    s.stability_score = (s.geometry_pass_rate + s.evidence_pass_rate) * 0.5;
    s.risk_score = std::min(1.0, (1.0 - s.stability_score) + timeout_penalty);
  }


  std::string exportReason;
  const bool exportOk = ExportParamRegressionReports(
      outDir, runtime.task, runtime.range_set, runtime.candidates,
      runtime.records, stats, exportReason);

  bool loopPass = exportOk && !rows.empty();
  for (const auto &row : rows) {
    if (row.status != "PASS")
      loopPass = false;
  }

  std::string reportReason;
  const bool reportOk = PipelineWriteParamRegressionLoopReports(
      outDir, runId, suitePath, manifestPath, paramScript, runtime.task,
      runtime.candidates, runtime.records, rows, loopPass, reportReason);

  if (!exportOk || !reportOk) {
    std::cout << "param_regression_loop_ok=false\nreason="
              << (!exportOk ? exportReason : reportReason) << "\n";
    return 2;
  }

  std::cout << "[MAIN] param regression loop mode end\n";
  std::cout << "param_regression_loop_ok=" << (loopPass ? "true" : "false")
            << "\n";
  std::cout << "tool=" << tool << "\n";
  std::cout << "candidate_count=" << runtime.candidates.size() << "\n";
  std::cout << "eval_record_count=" << runtime.records.size() << "\n";
  std::cout
      << "summary="
      << (fs::path(outDir) / "param_regression_loop_summary.json").string()
      << "\n";
  std::cout << "report="
            << (fs::path(outDir) / "param_regression_loop_report.md").string()
            << "\n";
  return loopPass ? 0 : 1;
}

int RunMaskDiagnosticSelfTestCli(int argc, char **argv) {
  const std::filesystem::path outDir = CliValueAfter(argc, argv, "--out");
  if (outDir.empty()) {
    std::cout << "mask_diagnostic_selftest_ok=false\n"
              << "conclusion=ASSET_PREFLIGHT_FAIL\n"
              << "reason=--out is required\n";
    return 2;
  }
  return RunCxMaskDiagnosticSelfTest(outDir);
}

int RunAutomaticDiagnosticClosureCli(int argc, char **argv) {
  CxAutomaticDiagnosticClosureOptions options;
  options.matrix_path = CliValueAfter(argc, argv, "--matrix");
  options.output_dir =
      ResolveCxVisionRunPath(CliValueAfter(argc, argv, "--out"));
  if (options.matrix_path.empty() || options.output_dir.empty()) {
    std::cout << "automatic_diagnostic_closure_ok=false\n"
              << "conclusion=ASSET_PREFLIGHT_FAIL\n"
              << "reason=--matrix and --out are required\n";
    return 2;
  }

  CxAutomaticDiagnosticClosureResult result;
  std::string reason;
  const bool ran = RunCxAutomaticDiagnosticClosure(options, result, reason);
  std::cout << "automatic_diagnostic_closure_ok="
            << (ran && result.complete ? "true" : "false") << "\n"
            << "conclusion=" << result.status << "\n"
            << "reason=" << (result.reason.empty() ? reason : result.reason)
            << "\n"
            << "discovered_rows=" << result.discovered_rows << "\n"
            << "bound_rows=" << result.bound_rows << "\n"
            << "executed_cases=" << result.executed_cases << "\n"
            << "completed_cases=" << result.completed_cases << "\n"
            << "binding_preflight=" << result.preflight_ref.string() << "\n";
  if (!result.process_status_ref.empty())
    std::cout << "closure_process_status=" << result.process_status_ref.string()
              << "\n";
  if (!result.aggregate_ref.empty())
    std::cout << "frozen_validation_aggregate=" << result.aggregate_ref.string()
              << "\n";
  if (!result.stability_ref.empty())
    std::cout << "stability_analysis=" << result.stability_ref.string() << "\n";
  if (!result.promotion_gate_ref.empty())
    std::cout << "promotion_gate=" << result.promotion_gate_ref.string()
              << "\n";

  if (!ran)
    return 2;
  return result.status == "PENDING_BINDING" ? 3 : (result.complete ? 0 : 1);
}

int RunGeometryReferenceEvaluationCli(int argc, char **argv) {
  CxGeometryReferenceEvaluationOptions options;
  options.index_path = CliValueAfter(argc, argv, "--geometry-reference-index");
  options.output_dir = CliValueAfter(argc, argv, "--out");
  std::string threshold;
  if (TryGetCliValue(argc, argv, "--threshold", threshold) &&
      !threshold.empty()) {
    try {
      options.threshold = std::stoi(threshold);
    } catch (...) {
      std::cout << "geometry_reference_evaluation_ok=false\n"
                << "conclusion=ASSET_PREFLIGHT_FAIL\n"
                << "reason=invalid --threshold\n";
      return 2;
    }
  }
  CxGeometryReferenceEvaluationResult result;
  std::string reason;
  const bool ok = RunCxGeometryReferenceEvaluation(options, result, reason);
  std::cout << "geometry_reference_evaluation_ok=" << (ok ? "true" : "false")
            << "\n"
            << "conclusion=" << result.status << "\n"
            << "reason=" << (result.reason.empty() ? reason : result.reason)
            << "\n"
            << "discovered_cases=" << result.discovered_cases << "\n"
            << "accepted_cases=" << result.accepted_cases << "\n"
            << "rejected_cases=" << result.rejected_cases << "\n"
            << "report_json=" << result.report_json.string() << "\n"
            << "report_markdown=" << result.report_markdown.string() << "\n";
  return ok ? 0 : 1;
}

int RunGeometryAugmentationDatasetCli(int argc, char **argv) {
  CxGeometryAugmentationDatasetOptions options;
  options.reference_index_path =
      CliValueAfter(argc, argv, "--geometry-reference-index");
  options.augmentation_plan_path =
      CliValueAfter(argc, argv, "--augmentation-plan");
  options.output_dir =
      ResolveCxVisionRunPath(CliValueAfter(argc, argv, "--out"));
  options.require_source_disjoint_validation =
      HasCliArg(argc, argv, "--require-source-disjoint-validation");
  if (options.reference_index_path.empty() ||
      options.augmentation_plan_path.empty() || options.output_dir.empty()) {
    std::cout << "geometry_augmentation_dataset_ok=false\n"
              << "conclusion=ASSET_PREFLIGHT_FAIL\n"
              << "reason=--geometry-reference-index, --augmentation-plan, and "
                 "--out are required\n";
    return 2;
  }

  CxGeometryAugmentationDatasetResult result;
  std::string reason;
  const bool ok = RunCxGeometryAugmentationDataset(options, result, reason);
  std::cout << "geometry_augmentation_dataset_ok=" << (ok ? "true" : "false")
            << "\n"
            << "conclusion=" << result.status << "\n"
            << "reason=" << (result.reason.empty() ? reason : result.reason)
            << "\n"
            << "source_case_count=" << result.source_case_count << "\n"
            << "variant_count=" << result.variant_count << "\n"
            << "generated_sample_count=" << result.generated_sample_count
            << "\n"
            << "rejected_sample_count=" << result.rejected_sample_count << "\n"
            << "train_sample_count=" << result.train_sample_count << "\n"
            << "validation_sample_count=" << result.validation_sample_count
            << "\n"
            << "holdout_sample_count=" << result.holdout_sample_count << "\n"
            << "dataset_manifest=" << result.dataset_manifest_path.string()
            << "\n"
            << "report_json=" << result.report_json_path.string() << "\n"
            << "report_markdown=" << result.report_markdown_path.string()
            << "\n";
  return ok ? 0 : 1;
}

int RunCxImageReferenceCandidateCliFromArgs(int argc, char **argv) {
  CxImageReferenceCandidateCliOptions options;
  options.image_path = CliValueAfter(argc, argv, "--image");
  options.output_dir = CliValueAfter(argc, argv, "--out");

  std::string value;
  if (TryGetCliValue(argc, argv, "--algorithm-id", value) && !value.empty())
    options.algorithm_id = value;
  if (TryGetCliValue(argc, argv, "--threshold", value) && !value.empty()) {
    try {
      options.threshold = std::stod(value);
    } catch (...) {
      std::cout
          << "conclusion=ASSET_PREFLIGHT_FAIL\nreason=invalid --threshold\n";
      return 2;
    }
  }

  const char *roi_names[] = {"--roi-x0", "--roi-y0", "--roi-x1", "--roi-y1"};
  int *roi_values[] = {&options.roi_x0, &options.roi_y0, &options.roi_x1,
                       &options.roi_y1};
  int roi_count = 0;
  for (int index = 0; index < 4; ++index) {
    if (TryGetCliValue(argc, argv, roi_names[index], value) && !value.empty()) {
      try {
        *roi_values[index] = std::stoi(value);
        ++roi_count;
      } catch (...) {
        std::cout
            << "conclusion=ASSET_PREFLIGHT_FAIL\nreason=invalid ROI value\n";
        return 2;
      }
    }
  }
  if (roi_count != 0 && roi_count != 4) {
    std::cout << "conclusion=ASSET_PREFLIGHT_FAIL\n"
              << "reason=ROI requires --roi-x0 --roi-y0 --roi-x1 --roi-y1\n";
    return 2;
  }
  options.has_roi = roi_count == 4;
  if (options.has_roi &&
      (options.roi_x1 <= options.roi_x0 || options.roi_y1 <= options.roi_y0)) {
    std::cout << "conclusion=ASSET_PREFLIGHT_FAIL\nreason=ROI must have "
                 "positive width and height\n";
    return 2;
  }
  return RunCxImageReferenceCandidateCli(options);
}

int RunBusinessWorkflowAcceptanceCli(int argc, char **argv) {
  CxBusinessWorkflowBatchRequest request;

  const auto read_required_value = [argc, argv](const std::string &name,
                                                std::string &value) {
    return TryGetCliValue(argc, argv, name, value) && !value.empty() &&
           value.rfind("--", 0) != 0;
  };

  std::string case_root;
  std::string out_dir;
  if (!read_required_value("--business-case-root", case_root)) {
    std::cerr << "business_workflow_parameter_status=FAIL\n"
              << "business_workflow_parameter_reason=missing or empty "
                 "--business-case-root\n";
    return 2;
  }
  if (!read_required_value("--out", out_dir)) {
    std::cerr << "business_workflow_parameter_status=FAIL\n"
              << "business_workflow_parameter_reason=missing or empty "
                 "--out\n";
    return 2;
  }
  if (!read_required_value("--run-id", request.run_id)) {
    std::cerr << "business_workflow_parameter_status=FAIL\n"
              << "business_workflow_parameter_reason=missing or empty "
                 "--run-id\n";
    return 2;
  }

  request.case_root = case_root;
  request.out_dir = out_dir;

  std::string max_cases;
  if (TryGetCliValue(argc, argv, "--max-cases", max_cases)) {
    if (max_cases.empty() || max_cases.rfind("--", 0) == 0) {
      std::cerr << "business_workflow_parameter_status=FAIL\n"
                << "business_workflow_parameter_reason=--max-cases requires "
                   "a nonnegative integer\n";
      return 2;
    }

    std::size_t parsed_max_cases = 0;
    for (const unsigned char ch : max_cases) {
      if (ch < '0' || ch > '9') {
        std::cerr
            << "business_workflow_parameter_status=FAIL\n"
            << "business_workflow_parameter_reason=--max-cases requires a "
               "nonnegative integer\n";
        return 2;
      }
      const std::size_t digit = static_cast<std::size_t>(ch - '0');
      if (parsed_max_cases >
          (std::numeric_limits<std::size_t>::max() - digit) / 10) {
        std::cerr
            << "business_workflow_parameter_status=FAIL\n"
            << "business_workflow_parameter_reason=--max-cases is out of "
               "range\n";
        return 2;
      }
      parsed_max_cases = parsed_max_cases * 10 + digit;
    }
    request.max_cases = parsed_max_cases;
  }

  CxBusinessWorkflowBatchResult result;
  std::string reason;
  const bool run_completed =
      RunCxBusinessWorkflowAcceptance(request, result, reason);
  const bool accepted =
      run_completed &&
      (result.final_status == "PASS" ||
       result.final_status == "CONTRACT_FIXTURE_PASS" ||
       result.final_status == "BUSINESS_WORKFLOW_ACCEPTED_WITH_PENDING" ||
       result.final_status == "ACCEPTED_WITH_PENDING");

  std::cout << "business_workflow_runtime_call_status="
            << (run_completed ? "COMPLETED" : "FAIL") << "\n"
            << "business_workflow_acceptance_status=" << result.final_status
            << "\n"
            << "business_workflow_acceptance_code=" << result.final_code
            << "\n"
            << "business_workflow_acceptance_reason=" << result.final_reason
            << "\n"
            << "business_workflow_runtime_reason=" << reason << "\n"
            << "business_workflow_run_id=" << result.run_id << "\n"
            << "business_workflow_case_root=" << result.case_root.string()
            << "\n"
            << "business_workflow_out_dir=" << result.out_dir.string() << "\n"
            << "business_workflow_scan_discovered_count="
            << result.discovered_count << "\n"
            << "business_workflow_scan_accepted_count=" << result.accepted_count
            << "\n"
            << "business_workflow_scan_rejected_count=" << result.rejected_count
            << "\n"
            << "business_workflow_scan_skipped_count=" << result.skipped_count
            << "\n"
            << "business_workflow_case_processed_count=" << result.processed_count
            << "\n"
            << "business_workflow_case_pass_count=" << result.pass_count << "\n"
            << "business_workflow_contract_fixture_pass_count="
            << result.contract_fixture_pass_count << "\n"
            << "business_workflow_provider_execution_step_pass_count="
            << result.provider_execution_step_pass_count << "\n"
            << "business_workflow_case_pending_count=" << result.pending_count
            << "\n"
            << "business_workflow_case_fail_count=" << result.fail_count << "\n"
            << "business_workflow_evidence_summary="
            << result.summary_path.string() << "\n"
            << "business_workflow_evidence_report_markdown="
            << result.report_md_path.string() << "\n"
            << "business_workflow_evidence_report_html="
            << result.report_html_path.string() << "\n"
            << "business_workflow_evidence_audit="
            << result.audit_jsonl_path.string() << "\n"
            << "business_workflow_evidence_scan_debug="
            << result.scan_debug_path.string() << "\n"
            << "business_workflow_evidence_risk_register="
            << result.risk_register_path.string() << "\n"
            << "business_workflow_output_error_count="
            << result.output_errors.size() << "\n";
  for (std::size_t index = 0; index < result.output_errors.size(); ++index) {
    std::cout << "business_workflow_output_error_" << (index + 1) << "="
              << result.output_errors[index] << "\n";
  }

  return accepted ? 0 : 1;
}

int RunCxVisionApplication(int argc, char **argv) {
  if (HasCliArg(argc, argv, "--evidence-configuration-projection-smoke"))
    return RunEvidenceConfigurationProjectionSmokeCli(argc, argv);

  if (HasCliArg(argc, argv, "--business-workflow-acceptance"))
    return RunBusinessWorkflowAcceptanceCli(argc, argv);

  if (HasCliArg(argc, argv, "--yolov8n-paired-inference"))
    return cxvision_yolov8n_paired_inference::
        RunYoloV8nPairedInferenceCli(argc, argv);

  if (HasCliArg(argc, argv, "--geometry-incremental-auto-tune"))
    return cxvision_geometry_auto_tune::RunGeometryIncrementalAutoTuneCli(
        argc, argv);

  if (HasCliArg(argc, argv, "--controlled-data-strategy-assembly"))
    return cxvision_geometry_auto_tune::RunControlledDataStrategyAssemblyCli(
        argc, argv);

  if (HasCliArg(argc, argv, "--controlled-l1-training-plan"))
    return cxvision_geometry_auto_tune::RunControlledL1TrainingPlanCli(
        argc, argv);

  if (HasCliArg(argc, argv, "--controlled-candidate-bundle"))
    return cxvision_geometry_auto_tune::RunControlledCandidateBundleCli(
        argc, argv);

  if (HasCliArg(argc, argv, "--geometry-yolo-mask-package"))
    return cxvision_geometry_yolo_mask_package::RunGeometryYoloMaskPackageCli(
        argc, argv);

  if (HasCliArg(argc, argv, "--yolov8n-training-lifecycle"))
    return cxvision_yolov8n_training::RunYoloV8nTrainingLifecycleCli(argc,
                                                                     argv);

  if (HasCliArg(argc, argv, "--yolov8n-geometry-association"))
    return cxvision_yolov8n_geometry::RunYoloV8nGeometryAssociationCli(argc,
                                                                       argv);

  if (HasCliArg(argc, argv, "--predictive-geometry-gate-selftest"))
    return RunCxPredictiveGeometryGateSelfTest(
        CliValueAfter(argc, argv, "--out"));

  if (HasCliArg(argc, argv, "--geometry-reference-evaluation"))
    return RunGeometryReferenceEvaluationCli(argc, argv);

  if (HasCliArg(argc, argv, "--geometry-augmentation-dataset"))
    return RunGeometryAugmentationDatasetCli(argc, argv);

  if (HasCliArg(argc, argv, "--typed-label-proposal")) {
    CxTypedLabelProposalOptions options;
    options.image_path = CliValueAfter(argc, argv, "--image");
    options.output_dir = CliValueAfter(argc, argv, "--out");
    options.label_kind = CliValueAfter(argc, argv, "--typed-label-kind");
    return RunCxTypedLabelProposalCli(options);
  }

  if (HasCliArg(argc, argv, "--automatic-diagnostic-closure")) {
    return RunAutomaticDiagnosticClosureCli(argc, argv);
  }

  if (HasCliArg(argc, argv, "--cximage-reference-candidate")) {
    return RunCxImageReferenceCandidateCliFromArgs(argc, argv);
  }

  if (HasCliArg(argc, argv, "--mask-diagnostic-selftest")) {
    return RunMaskDiagnosticSelfTestCli(argc, argv);
  }

  if (HasCliArg(argc, argv, "--gwy-reference-smoke")) {
    return RunGwyReferenceInterfaceSmokeCli(argc, argv);
  }

  if (HasCliArg(argc, argv, "--metrology-analytics-smoke")) {
    return RunMetrologyAnalyticsSmokeCli(argc, argv);
  }

  std::string selftestFilter;
  if (TryGetCliValue(argc, argv, "--selftest", selftestFilter)) {
    if (selftestFilter == "analytics" || selftestFilter == "analytics.*" ||
        selftestFilter.rfind("analytics.", 0) == 0) {
      return RunMetrologyAnalyticsSelfTestCli(argc, argv, selftestFilter);
    }
    std::cout << "selftest_ok=false\n";
    std::cout << "filter=" << selftestFilter << "\n";
    std::cout << "final_code=SELFTEST_NAMESPACE_NOT_HANDLED\n";
    std::cout << "reason=unsupported selftest namespace; analytics runner was "
                 "not invoked\n";
    return 2;
  }

  EvidenceChainSelfTestCliOptions evidenceOptions;
  ParseEvidenceChainSelfTestArgs(argc, argv, evidenceOptions);

  if (evidenceOptions.torch_training_label_package_smoke) {
    std::cout << "[MAIN] Torch training dataset smoke begin\n";
    ViewController controller;
    std::string initReason;
    if (!controller.InitEvidenceSelfTestEnvironment(initReason)) {
      std::cout << "torch_training_label_package_smoke_ok=false\n"
                << "reason=environment initialization failed: " << initReason
                << "\n";
      return 2;
    }

    const std::string runId = CxUnifiedLog::Instance().GenerateRunId();
    const std::string outDir =
        evidenceOptions.out_dir.empty()
            ? "D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxscript_runs/"
              "torch_training_label_package_smoke/run_" +
                  runId
            : evidenceOptions.out_dir;
    std::string packagePath;
    std::string reason;
    const bool ok = controller.RunTorchTrainingLabelPackageSmoke(
        evidenceOptions.torch_training_label_script, outDir, packagePath,
        reason);
    std::cout << "torch_training_label_package_smoke_ok="
              << (ok ? "true" : "false") << "\n"
              << "package_path=" << packagePath << "\n"
              << "out_dir=" << outDir << "\n"
              << "conclusion="
              << (ok ? "TRAINING_DATASET_EXPORT_PASS_TO_VERIFY"
                     : "TRAINING_DATASET_EXPORT_FAIL")
              << "\n"
              << "reason=" << reason << "\n";
    return ok ? 0 : 1;
  }

  if (evidenceOptions.param_regression_loop) {
    return RunParamRegressionLoopCli(evidenceOptions);
  }

  if (evidenceOptions.baseline_lock) {
    return RunBaselineLockCli(evidenceOptions);
  }

  if (evidenceOptions.standard_chain_gate) {
    return RunStandardChainGateCli(evidenceOptions);
  }

  if (evidenceOptions.evidence_lock_pipeline) {
    return RunEvidenceLockPipelineCli(evidenceOptions);
  }

  if (evidenceOptions.enabled) {
    std::cout << "[MAIN] evidence chain selftest mode begin\n" << std::flush;

    ViewController controller;
    std::string initReason;
    if (!controller.InitEvidenceSelfTestEnvironment(initReason)) {
      std::cout << "[MAIN] evidence chain selftest init failed: " << initReason
                << "\n";
      return 2;
    }

    const std::string manifest_path =
        evidenceOptions.annotation_tool_manifest.empty()
            ? "cxparser/cxscript/module/cximage/tool_annotation_basic.cxsc"
            : evidenceOptions.annotation_tool_manifest;

    const std::string run_id = CxUnifiedLog::Instance().GenerateRunId();
    const std::string out_dir =
        evidenceOptions.out_dir.empty()
            ? "D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxscript_runs/"
              "evidence_selftest/run_" +
                  run_id
            : evidenceOptions.out_dir;

    std::string semanticReason;
    if (!controller.WriteEvidenceChainCatalogSemanticSelfTest(out_dir,
                                                              semanticReason)) {
      std::cout << "[MAIN] evidence chain catalog semantic selftest failed: "
                << semanticReason << "\n";
      return 2;
    }
    std::cout << "[MAIN] evidence chain catalog semantic selftest: "
              << semanticReason << "\n";

    CxEvidenceSelfTestBatchRequest request;
    request.run_id = run_id;
    request.out_dir = out_dir;
    request.max_cases = evidenceOptions.max_cases;
    request.tool_filter = evidenceOptions.evidence_tool_filter;

    std::string reason;
    if (!controller.BuildEvidenceSelfTestBatchFromCurrentEvidenceRows(request,
                                                                      reason)) {
      std::cout << "[MAIN] evidence chain selftest build failed: " << reason
                << "\n";
      return 2;
    }

    std::cout << "[MAIN] evidence chain selftest cases: "
              << request.cases.size() << "\n";

    CxEvidenceSelfTestBatchResult result;
    if (!controller.RunEvidenceSelfTestBatch(request, result, reason)) {
      std::cout << "[MAIN] evidence chain selftest failed: " << reason << "\n";
      return 3;
    }

    std::cout << "[MAIN] evidence chain selftest end\n";
    std::cout << "evidence_selftest_ok="
              << (result.fail_count == 0 ? "true" : "false") << "\n";
    std::cout << "total_cases=" << result.total_cases << "\n";
    std::cout << "executed_cases=" << result.executed_cases << "\n";
    std::cout << "pass_count=" << result.pass_count << "\n";
    std::cout << "pending_count=" << result.pending_count << "\n";
    std::cout << "fail_count=" << result.fail_count << "\n";
    std::cout << "final_code=" << result.final_code << "\n";
    std::cout << "final_status=" << result.final_status << "\n";
    std::cout << "final_reason=" << result.final_reason << "\n";

    return result.fail_count == 0 ? 0 : 1;
  }

  ShapeInteractionTestOptions shapeOptions;

  if (!ParseShapeInteractionTestArgs(argc, argv, shapeOptions)) {
    std::cerr << "shape argument error: " << shapeOptions.parse_reason << "\n";
    return 2;
  }

  if (shapeOptions.enabled) {
    const std::string manifest_path =
        shapeOptions.manifest_path.empty()
            ? "cxparser/cxscript/module/cximage/tool_annotation_basic.cxsc"
            : shapeOptions.manifest_path;
    const std::string suite_path =
        shapeOptions.suite_path.empty()
            ? "cxparser/cxscript/module/cximage/tests/"
              "shape_interaction_acceptance.cxsc"
            : shapeOptions.suite_path;
    const std::string image_manifest_path = shapeOptions.image_manifest_path;
    const std::string out_dir =
        shapeOptions.out_dir.empty()
            ? "D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxscript_runs/"
              "shape_interaction_smoke"
            : shapeOptions.out_dir;

    CxShapeInteractionBatchResult result;
    const bool ok = RunShapeInteractionSmokeCli(
        manifest_path, suite_path, image_manifest_path, out_dir, result);

    std::cout << "shape_interaction_smoke_ok=" << (ok ? "true" : "false")
              << "\n";
    std::cout << "total_cases=" << result.cases.size() << "\n";
    std::cout << "pass_count="
              << std::count_if(result.cases.begin(), result.cases.end(),
                               [](const auto &c) { return c.pass; })
              << "\n";
    std::cout << "fail_count="
              << std::count_if(result.cases.begin(), result.cases.end(),
                               [](const auto &c) { return !c.pass; })
              << "\n";

    if (!result.failure_stage.empty()) {
      std::cout << "failure_stage=" << result.failure_stage << "\n";
      std::cout << "reason=" << result.reason << "\n";
    }

    for (const auto &c : result.cases) {
      std::cout << "case=" << c.case_id
                << " pass=" << (c.pass ? "true" : "false")
                << " conclusion=" << c.conclusion << "\n";
    }

    int exit_code = ok ? 0 : 1;
    if (result.cases.empty()) {
      exit_code = 3;
    }
    return exit_code;
  }

  CxScriptHeadlessOptions headlessOptions;
  GaugeFrameProbeOptions frameProbeOptions;
  if (ParseGaugeFrameProbeArgs(argc, argv, frameProbeOptions) &&
      frameProbeOptions.enabled) {
    GaugeFrameProbeResult result;
    const bool ok = RunGaugeFrameProbe(frameProbeOptions, result);
    std::cout << "frame_probe_ok=" << (ok ? "true" : "false") << "\n";
    std::cout << "reason=" << result.reason << "\n";
    std::cout << "tool=" << result.tool << "\n";
    std::cout << "frame_black=" << result.frame_black_path.string() << "\n";
    std::cout << "frame_on_image=" << result.frame_on_image_path.string()
              << "\n";
    std::cout << "frame_geometry=" << result.frame_geometry_path.string()
              << "\n";
    std::cout << "frame_report=" << result.frame_report_path.string() << "\n";
    return result.exit_code;
  }
  if (ParseCxScriptHeadlessArgs(argc, argv, headlessOptions) &&
      headlessOptions.enabled) {
    CxScriptHeadlessResult result;

    const bool ok = RunCxScriptHeadless(headlessOptions, result);

    std::cout << "cxscript_headless_ok=" << (ok ? "true" : "false") << "\n";

    std::cout << "reason=" << result.reason << "\n";

    std::cout << "snapshot=" << result.snapshot_path << "\n";

    std::cout << "overlay=" << result.overlay_path << "\n";

    std::cout << "summary=" << result.summary_path << "\n";

    std::cout << "run_state=" << result.run_state << "\n";

    return result.exit_code;
  }

#if defined(CXVISION_ENABLE_LEGACY_STAGE25_CPP)
  Stage25RunOptions stage25Options;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--cxscript-stage25") {
      stage25Options.out_root = "D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/"
                                "cxscript_runs/stage_2_5_l1_l3";
    } else if (arg == "--manifest" && i + 1 < argc) {
      stage25Options.manifest_path = argv[++i];
    } else if (arg == "--out" && i + 1 < argc) {
      stage25Options.out_root = argv[++i];
    }
  }

  if (!stage25Options.manifest_path.empty()) {
    Stage25RunResult result;
    const bool ok = RunStage25ManifestFile(stage25Options, result);

    std::cout << "stage25_ok=" << (ok ? "true" : "false") << "\n";

    std::cout << "reason=" << result.reason << "\n";

    std::cout << "total_cases=" << result.total_cases << "\n";

    std::cout << "t0_pass=" << result.t0_pass << "\n";

    std::cout << "t1_pass=" << result.t1_pass << "\n";

    std::cout << "t2_pass=" << result.t2_pass << "\n";

    return ok ? 0 : 1;
  }
#endif

  CxScriptSuiteRunOptions suiteOptions;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];

    if (arg == "--cxscript-suite" && i + 1 < argc) {
      suiteOptions.enabled = true;
      suiteOptions.suite_path = argv[++i];
    } else if (arg == "--image-manifest" && i + 1 < argc) {
      suiteOptions.image_manifest_path = argv[++i];
    } else if (arg == "--catalog" && i + 1 < argc) {
      suiteOptions.catalog_path_override = argv[++i];
    } else if (arg == "--out" && i + 1 < argc) {
      suiteOptions.out_root_override = argv[++i];
    } else if (arg == "--cxscript-parameter-profile" && i + 1 < argc) {
      suiteOptions.parameter_profile_path = argv[++i];
    } else if (arg == "--require-review") {
      suiteOptions.require_human_review = true;
    } else if (arg == "--review-stage" && i + 1 < argc) {
      suiteOptions.review_stage = argv[++i];
    } else if (arg == "--review-decision" && i + 1 < argc) {
      suiteOptions.review_decision = argv[++i];
    } else if (arg == "--resume-review" && i + 1 < argc) {
      suiteOptions.resume_review_id = argv[++i];
    } else if (arg == "--suite-dry-run") {
      suiteOptions.dry_run = true;
    } else if (arg == "--suite-preview-only") {
      suiteOptions.preview_only = true;
    } else if (arg == "--use-manual-gauge") {
      suiteOptions.use_manual_gauge = true;
    } else if (arg == "--gauge-annotation" && i + 1 < argc) {
      suiteOptions.gauge_annotation_path = argv[++i];
    } else if (arg == "--probe-only") {
      suiteOptions.probe_only = true;
      suiteOptions.run_contract = false;
    } else if (arg == "--only-case" && i + 1 < argc) {
      suiteOptions.only_case_id = argv[++i];
    } else if (arg == "--case-timeout-sec" && i + 1 < argc) {
      suiteOptions.case_timeout_sec = std::stoi(argv[++i]);
    } else if (arg == "--trace-run") {
      suiteOptions.trace_run = true;
    } else if (arg == "--dump-replay-package") {
      suiteOptions.dump_replay_package = true;
    } else if (arg == "--no-replay-package") {
      suiteOptions.dump_replay_package = false;
    } else if (arg == "--dump-cxparser-ext-trace") {
      suiteOptions.dump_cxparser_ext_trace = true;
    } else if (arg == "--heartbeat-ms" && i + 1 < argc) {
      suiteOptions.heartbeat_ms = std::stoi(argv[++i]);
    } else if (arg == "--trace-dir" && i + 1 < argc) {
      suiteOptions.trace_dir = argv[++i];
    } else if (arg == "--no-contract") {
      suiteOptions.run_contract = false;
    } else if (arg == "--no-tool-display") {
      suiteOptions.export_tool_display = false;
    } else if (arg == "--no-evidence-summary") {
      suiteOptions.export_evidence_summary = false;
    } else if (arg == "--no-final-report") {
      suiteOptions.export_final_report = false;
    } else if (arg == "--no-best-gallery") {
      suiteOptions.export_best_examples = false;
    } else if (arg == "--suite-stop-after-headless") {
      suiteOptions.stop_after_headless = true;
    }
  }

  if (suiteOptions.enabled) {
    std::cout << "[MAIN] suite mode begin\n" << std::flush;
    CxScriptSuiteRunResult result;
    const bool ok = RunCxScriptSuite(suiteOptions, result);

    std::cout << "[MAIN] suite mode end ok=" << (ok ? "true" : "false")
              << " reason=" << result.reason << "\n"
              << std::flush;

    std::cout << "suite_run_ok=" << (ok ? "true" : "false") << "\n";
    std::cout << "reason=" << result.reason << "\n";
    std::cout << "total_cases=" << result.total_cases << "\n";
    std::cout << "executed_cases=" << result.executed_cases << "\n";
    std::cout << "contract_pass=" << result.contract_pass << "\n";
    std::cout << "contract_fail=" << result.contract_fail << "\n";
    std::cout << "report_root=" << result.report_root << "\n";

    if (result.reason == "REVIEW_REQUIRED") {
      std::cout
          << "[MAIN] exiting process from suite mode with review required\n"
          << std::flush;
      return 10;
    }

    std::cout << "[MAIN] exiting process from suite mode\n" << std::flush;
    return ok ? 0 : 1;
  }

  std::cout << "[MAIN] entering GUI mode\n" << std::flush;
  return glfw_occ_main();
}

int RunUnifiedLogSmoke(const CxUnifiedLogOptions &options) {
  CXLOG_INFO("CxUnifiedLog", "smoke_begin", "running",
             "smoke_id=" + options.smoke_id);

  std::atomic<int> event_count{0};

  auto worker = [&](int id) {
    for (int i = 0; i < 100; ++i) {
      std::ostringstream oss;
      oss << "worker_thread_" << id << " event " << i;
      CXLOG_INFO("CxUnifiedLogSmoke", "worker_event", "ok", oss.str());
      event_count++;
    }
  };

  std::thread t1(worker, 0);
  std::thread t2(worker, 1);

  t1.join();
  t2.join();

  CXLOG_INFO("CxUnifiedLog", "smoke_end", "completed",
             "events=" + std::to_string(event_count.load()));

  std::cout << "unified_log_smoke_ok=true\n";
  std::cout << "written_events=" << event_count.load() + 2 << "\n";
  std::cout << "run_id=" << CxUnifiedLog::Instance().RunId() << "\n";
  std::cout << "log_path=" << CxUnifiedLog::Instance().Path().string() << "\n";

  return 0;
}

void SaveTorchRuntimeSmokeJson(const CxUnifiedLogOptions &options,
                               bool initialized, bool ready, bool shutdown_ok,
                               const std::string &version,
                               const std::string &reason) {
  if (options.torch_runtime_smoke.output_dir.empty())
    return;

  std::filesystem::create_directories(options.torch_runtime_smoke.output_dir);

  std::ofstream file(options.torch_runtime_smoke.output_dir /
                     "torch_runtime_service_smoke.json");
  file << "{\n";
  file << "  \"schema_version\": 1,\n";
  file << "  \"run_id\": \"" << CxUnifiedLog::Instance().RunId() << "\",\n";
  file << "  \"runtime_dll\": \""
       << options.torch_runtime_smoke.runtime_dll.string() << "\",\n";
  file << "  \"device\": \"" << options.torch_runtime_smoke.device << "\",\n";
  file << "  \"model_root\": \"" << options.torch_runtime_smoke.model_root
       << "\",\n";
  file << "  \"initialized\": " << (initialized ? "true" : "false") << ",\n";
  file << "  \"ready\": " << (ready ? "true" : "false") << ",\n";
  file << "  \"shutdown_ok\": " << (shutdown_ok ? "true" : "false") << ",\n";
  file << "  \"version\": \"" << version << "\",\n";
  file << "  \"reason\": \"" << reason << "\"\n";
  file << "}\n";
}

int RunCxTorchRuntimeSmoke(const CxUnifiedLogOptions &options) {
  CXLOG_INFO("CxTorchRuntime", "smoke_begin", "running",
             "mode=torch_runtime_smoke");

  std::filesystem::create_directories(options.torch_runtime_smoke.output_dir);

  const std::filesystem::path runtime_dll =
      options.torch_runtime_smoke.runtime_dll;
  const std::filesystem::path runtime_dir = runtime_dll.parent_path();

  if (runtime_dir.empty()) {
    std::string reason = "Runtime DLL directory is empty";
    SaveTorchRuntimeSmokeJson(options, false, false, false, "", reason);
    CXLOG_ERROR("CxTorchRuntime", "smoke_end", "failed", "reason=" + reason);
    std::cout << "torch_runtime_smoke_ok=false\n";
    std::cout << "reason=" << reason << "\n";
    return 1;
  }

  const char *old_path = std::getenv("PATH");
  std::string new_path;
  if (old_path) {
    new_path = runtime_dir.string() + ";" + old_path;
  } else {
    new_path = runtime_dir.string();
  }
  SetEnvironmentVariableA("PATH", new_path.c_str());

  std::cout << "runtime_dll=" << runtime_dll.string() << "\n";
  std::cout << "runtime_dir=" << runtime_dir.string() << "\n";
  std::cout << "device=" << options.torch_runtime_smoke.device << "\n";
  std::cout << "model_root=" << options.torch_runtime_smoke.model_root << "\n";

  CxTorchRuntimeService service;
  CxTorchRuntimeConfig config;

  config.runtime_dll_path = runtime_dll.string();
  config.device = options.torch_runtime_smoke.device;
  config.model_root = options.torch_runtime_smoke.model_root;

  std::string reason;
  bool initialized = false;
  bool ready = false;
  std::string version;

  try {
    initialized = service.Initialize(config, reason);
    ready = service.IsReady();
    version = service.RuntimeVersion();
  } catch (const std::exception &e) {
    reason = "exception during initialization: " + std::string(e.what());
    initialized = false;
    ready = false;
    version = "";
  }

  service.Shutdown();
  const bool shutdown_ok = !service.IsReady();

  SaveTorchRuntimeSmokeJson(options, initialized, ready, shutdown_ok, version,
                            reason);

  CXLOG_INFO("CxTorchRuntime", "smoke_end",
             initialized && ready && shutdown_ok && !version.empty()
                 ? "completed"
                 : "failed",
             "initialized=" + std::string(initialized ? "true" : "false") +
                 ",ready=" + std::string(ready ? "true" : "false") +
                 ",shutdown_ok=" + std::string(shutdown_ok ? "true" : "false") +
                 ",version=" + version + ",reason=" + reason);

  std::cout << "torch_runtime_smoke_ok="
            << (initialized && ready && shutdown_ok && !version.empty()
                    ? "true"
                    : "false")
            << "\n";
  std::cout << "initialized=" << (initialized ? "true" : "false") << "\n";
  std::cout << "ready=" << (ready ? "true" : "false") << "\n";
  std::cout << "shutdown_ok=" << (shutdown_ok ? "true" : "false") << "\n";
  std::cout << "version=" << version << "\n";
  std::cout << "reason=" << reason << "\n";
  std::cout << "run_id=" << CxUnifiedLog::Instance().RunId() << "\n";

  return (initialized && ready && shutdown_ok && !version.empty()) ? 0 : 1;
}

int main(int argc, char **argv) {
  if (HasCliArg(argc, argv, "--model-lineage-operation-selftest")) {
    std::string scanRoot;
    std::string outputDirectory;
    for (int i = 1; i + 1 < argc; ++i) {
      const std::string arg = argv[i] == nullptr ? "" : argv[i];
      if (arg == "--model-lineage-root")
        scanRoot = argv[++i];
      else if (arg == "--out")
        outputDirectory = argv[++i];
    }
    if (scanRoot.empty() || outputDirectory.empty()) {
      std::cerr << "model_lineage_operation_selftest_ok=false\n";
      std::cerr << "reason=--model-lineage-root and --out are required\n";
      return 2;
    }
    return RunModelLineageOperationSelfTest(scanRoot, outputDirectory);
  }
  if (HasCliArg(argc, argv, "--model-lineage-scan-smoke")) {
    std::string scanRoot;
    std::string outputDirectory;
    for (int i = 1; i + 1 < argc; ++i) {
      const std::string arg = argv[i] == nullptr ? "" : argv[i];
      if (arg == "--model-lineage-root")
        scanRoot = argv[++i];
      else if (arg == "--out")
        outputDirectory = argv[++i];
    }
    if (scanRoot.empty() || outputDirectory.empty()) {
      std::cerr << "model_lineage_scan_smoke_ok=false\n";
      std::cerr << "reason=--model-lineage-root and --out are required\n";
      return 2;
    }
    return RunModelLineageAssetScanSmoke(scanRoot, outputDirectory);
  }
  CxUnifiedLogOptions logOptions;
  std::string logReason;

  ParseUnifiedLogArgs(argc, argv, logOptions, logReason);

  const std::string mode = DetectCxVisionRunMode(argc, argv);

  ShapeInteractionTestOptions shapeOptions;
  ParseShapeInteractionTestArgs(argc, argv, shapeOptions);

  bool should_enable_unified_log = logOptions.enabled || shapeOptions.enabled;
  if (!logOptions.path.empty() || shapeOptions.enabled) {
    if (logOptions.path.empty()) {
      logOptions.path = "D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/"
                        "cxscript_runs/_shared/cxvision_imgui_acceptance.jsonl";
    }
  }

  if (should_enable_unified_log) {
    bool init_ok = CxUnifiedLog::Instance().Initialize(logOptions.path, mode,
                                                       argc, argv, logReason);
    if (!init_ok) {
      std::cerr << "[GuiMain] Unified log init failed: " << logReason << "\n";
      std::cerr << "[GuiMain] Path: " << logOptions.path.string() << "\n";
    } else {
      std::cout << "[GuiMain] Unified log initialized: "
                << logOptions.path.string() << "\n";
    }
  }

  InstallCxCrashLogHandlers();

  if (logOptions.enabled && logOptions.capture_stdio) {
    InstallUnifiedStdStreamCapture();
  }

  if (logOptions.enabled) {
    CXLOG_INFO("GuiMain", "run_start", "started",
               "mode=" + mode + ", args=" + RedactCommandLine(argc, argv));
  }

  int exitCode = 0;

  if (logOptions.torch_runtime_smoke.enabled) {
    exitCode = RunCxTorchRuntimeSmoke(logOptions);
  } else if (logOptions.smoke_mode) {
    exitCode = RunUnifiedLogSmoke(logOptions);
  } else {
    exitCode = RunCxVisionApplication(argc, argv);
  }

  if (logOptions.enabled) {
    CxUnifiedLog::Instance().Shutdown(exitCode, exitCode == 0 ? "normal_exit"
                                                              : "failed_exit");
  }

  if (logOptions.enabled && logOptions.capture_stdio) {
    RestoreUnifiedStdStreamCapture();
  }

  return exitCode;
}
namespace {
int RunEvidenceConfigurationProjectionSmokeCli(int argc, char **argv) {
  std::string scriptPath;
  std::string outputDirectory;
  for (int index = 1; index + 1 < argc; ++index) {
    if (std::string(argv[index]) == "--evidence-chain-script") {
      scriptPath = argv[index + 1];
    } else if (std::string(argv[index]) == "--out") {
      outputDirectory = argv[index + 1];
    }
  }
  if (scriptPath.empty()) {
    std::cout << "conclusion=ASSET_PREFLIGHT_FAIL\n"
              << "reason=--evidence-chain-script is required\n";
    return 2;
  }
  CxScriptEvidenceChainRuntime chain;
  std::string reason;
  if (!LoadCxScriptEvidenceChainFile(scriptPath, chain, reason)) {
    std::cout << "conclusion=EVIDENCE_CONFIGURATION_PROJECTION_FAIL\n"
              << "reason=" << reason << "\n";
    return 1;
  }
  int complete = 0;
  int missing = 0;
  for (const CxScriptEvidenceCase &item : chain.cases) {
    const std::string *references[] = {
        &item.training_config_ref, &item.model_manifest_ref,
        &item.inference_config_ref, &item.quality_policy_ref,
        &item.ontology_ref, &item.failure_samples_ref};
    bool caseComplete = true;
    for (const std::string *reference : references) {
      std::error_code ec;
      if (reference->empty() ||
          !std::filesystem::is_regular_file(*reference, ec) || ec) {
        caseComplete = false;
        ++missing;
      }
    }
    complete += caseComplete ? 1 : 0;
  }
  const bool pass = !chain.cases.empty() &&
                    complete == static_cast<int>(chain.cases.size()) &&
                    missing == 0;
  if (!outputDirectory.empty()) {
    std::error_code ec;
    if (std::filesystem::exists(outputDirectory, ec)) {
      std::cout << "conclusion=ASSET_PREFLIGHT_FAIL\n"
                << "reason=output directory already exists; use a new RUN_ID\n";
      return 2;
    }
    std::filesystem::create_directories(outputDirectory, ec);
    if (ec) {
      std::cout << "conclusion=ASSET_PREFLIGHT_FAIL\n"
                << "reason=output directory cannot be created\n";
      return 2;
    }
    std::ofstream report(std::filesystem::path(outputDirectory) /
                         "g08_configuration_projection_report.json");
    report << "{\n"
           << "  \"schema\": \"cxvision.evidence_configuration_projection.v1\",\n"
           << "  \"status\": \""
           << (pass ? "EVIDENCE_CONFIGURATION_PROJECTION_PASS"
                    : "EVIDENCE_CONFIGURATION_PROJECTION_FAIL")
           << "\",\n"
           << "  \"source_script\": \"" << PipelineJsonEscape(scriptPath)
           << "\",\n"
           << "  \"chain_id\": \"" << PipelineJsonEscape(chain.chain_id)
           << "\",\n"
           << "  \"case_count\": " << chain.cases.size() << ",\n"
           << "  \"complete_configuration_case_count\": " << complete
           << ",\n"
           << "  \"missing_configuration_reference_count\": " << missing
           << "\n}\n";
  }
  std::cout << "conclusion="
            << (pass ? "EVIDENCE_CONFIGURATION_PROJECTION_PASS"
                     : "EVIDENCE_CONFIGURATION_PROJECTION_FAIL")
            << "\nchain_id=" << chain.chain_id
            << "\ncase_count=" << chain.cases.size()
            << "\ncomplete_configuration_case_count=" << complete
            << "\nmissing_configuration_reference_count=" << missing
            << "\nreason=" << reason << "\n";
  return pass ? 0 : 1;
}
} // namespace
