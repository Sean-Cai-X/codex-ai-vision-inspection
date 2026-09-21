#pragma once

#include "CxTorchRuntimeService.h"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

#include <filesystem>
#include <array>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace cxvision_yolov8n_paired_inference {
namespace fs = std::filesystem;

struct Case {
  std::string review_item;
  std::string geometry_type;
  std::string family;
  fs::path image_path;
  fs::path label_path;
};

struct DetectionMetrics {
  int detection_count = 0;
  int target_overlap_count = 0;
  int same_class_target_overlap_count = 0;
  int cross_class_target_overlap_count = 0;
  int background_detection_count = 0;
  int top_class_id = -1;
  int target_class_id = -1;
  double best_same_class_iou = 0.0;
  double best_same_class_confidence = 0.0;
  double best_any_class_iou = 0.0;
  double top_confidence = 0.0;
  bool matched_iou50 = false;
  bool exact_single_correct = false;
  bool duplicate_response = false;
  bool cross_class_competition = false;
  bool top_class_correct = false;
};

// Every timing is taken around the real runtime service call.  The residual is
// deliberately named rather than guessed: it contains preprocessing, model
// load, postprocess, and evidence writes that are not separately reported by
// the current runtime ABI.
struct RuntimeProfileSample {
  std::string review_item;
  std::string model_role;
  double service_execute_ms = 0.0;
  double forward_ms = 0.0;
  double residual_pipeline_ms = 0.0;
};

struct RuntimeProfileSummary {
  double min_ms = 0.0;
  double mean_ms = 0.0;
  double p50_ms = 0.0;
  double p95_ms = 0.0;
  double p99_ms = 0.0;
  double max_ms = 0.0;
};

inline RuntimeProfileSummary SummarizeRuntimeSamples(
    const std::vector<double> &samples) {
  RuntimeProfileSummary summary;
  if (samples.empty())
    return summary;
  std::vector<double> sorted = samples;
  std::sort(sorted.begin(), sorted.end());
  double total = 0.0;
  for (const double sample : sorted)
    total += sample;
  const auto percentile = [&sorted](const double q) {
    const double index = q * static_cast<double>(sorted.size() - 1);
    const std::size_t lower = static_cast<std::size_t>(std::floor(index));
    const std::size_t upper = static_cast<std::size_t>(std::ceil(index));
    const double fraction = index - static_cast<double>(lower);
    return sorted[lower] + (sorted[upper] - sorted[lower]) * fraction;
  };
  summary.min_ms = sorted.front();
  summary.mean_ms = total / static_cast<double>(sorted.size());
  summary.p50_ms = percentile(0.50);
  summary.p95_ms = percentile(0.95);
  summary.p99_ms = percentile(0.99);
  summary.max_ms = sorted.back();
  return summary;
}

inline std::string ArgValue(int argc, char **argv, const std::string &name) {
  const std::string prefix = name + "=";
  for (int i = 1; i < argc; ++i) {
    if (argv[i] == nullptr)
      continue;
    const std::string arg = argv[i];
    if (arg == name && i + 1 < argc && argv[i + 1] != nullptr)
      return argv[i + 1];
    if (arg.rfind(prefix, 0) == 0)
      return arg.substr(prefix.size());
  }
  return {};
}

inline std::string Escape(const std::string &value) {
  std::string out;
  for (const char ch : value) {
    if (ch == '\\' || ch == '"')
      out += '\\';
    if (ch == '\n') {
      out += "\\n";
      continue;
    }
    if (ch == '\r')
      continue;
    out += ch;
  }
  return out;
}

inline std::string SafeStem(const std::string &value) {
  std::string out;
  for (const unsigned char ch : value) {
    if (out.size() >= 36)
      break;
    if (std::isalnum(ch))
      out += static_cast<char>(std::tolower(ch));
    else if (!out.empty() && out.back() != '_')
      out += '_';
  }
  return out.empty() ? "case" : out;
}

inline std::string StableSuffix(const std::string &value) {
  std::ostringstream out;
  out << std::hex << std::setw(10) << std::setfill('0')
      << static_cast<unsigned long long>(
             std::hash<std::string>{}(value) & 0xffffffffffULL);
  return out.str();
}

inline bool NodeBool(const cv::FileNode &node) {
  return !node.empty() && static_cast<int>(node) != 0;
}

inline DetectionMetrics EvaluateDetections(const fs::path &detections_path,
                                           const fs::path &label_path,
                                           const fs::path &image_path) {
  DetectionMetrics metrics;
  const cv::Mat image = cv::imread(image_path.string(), cv::IMREAD_COLOR);
  std::ifstream label(label_path);
  int target_class = -1;
  double cx = 0.0, cy = 0.0, width = 0.0, height = 0.0;
  if (image.empty() || !(label >> target_class >> cx >> cy >> width >> height))
    return metrics;
  metrics.target_class_id = target_class;
  const double target_x1 = (cx - width * 0.5) * image.cols;
  const double target_y1 = (cy - height * 0.5) * image.rows;
  const double target_x2 = (cx + width * 0.5) * image.cols;
  const double target_y2 = (cy + height * 0.5) * image.rows;
  const double target_area =
      std::max(0.0, target_x2 - target_x1) *
      std::max(0.0, target_y2 - target_y1);

  cv::FileStorage detections(detections_path.string(), cv::FileStorage::READ);
  const cv::FileNode nodes = detections["detections"];
  if (!detections.isOpened() || !nodes.isSeq())
    return metrics;
  metrics.detection_count = static_cast<int>(nodes.size());
  for (const auto &node : nodes) {
    int class_id = -1;
    double x1 = 0.0, y1 = 0.0, x2 = 0.0, y2 = 0.0, confidence = 0.0;
    node["class_id"] >> class_id;
    node["x1"] >> x1;
    node["y1"] >> y1;
    node["x2"] >> x2;
    node["y2"] >> y2;
    node["confidence"] >> confidence;
    if (confidence > metrics.top_confidence) {
      metrics.top_confidence = confidence;
      metrics.top_class_id = class_id;
    }
    const double intersection_x1 = std::max(x1, target_x1);
    const double intersection_y1 = std::max(y1, target_y1);
    const double intersection_x2 = std::min(x2, target_x2);
    const double intersection_y2 = std::min(y2, target_y2);
    const double intersection =
        std::max(0.0, intersection_x2 - intersection_x1) *
        std::max(0.0, intersection_y2 - intersection_y1);
    const double detection_area =
        std::max(0.0, x2 - x1) * std::max(0.0, y2 - y1);
    const double union_area = target_area + detection_area - intersection;
    const double iou = union_area > 0.0 ? intersection / union_area : 0.0;
    metrics.best_any_class_iou = std::max(metrics.best_any_class_iou, iou);
    if (iou >= 0.5) {
      ++metrics.target_overlap_count;
      if (class_id == target_class)
        ++metrics.same_class_target_overlap_count;
      else
        ++metrics.cross_class_target_overlap_count;
    } else {
      ++metrics.background_detection_count;
    }
    if (class_id != target_class)
      continue;
    if (iou > metrics.best_same_class_iou) {
      metrics.best_same_class_iou = iou;
      metrics.best_same_class_confidence = confidence;
    }
  }
  metrics.matched_iou50 = metrics.best_same_class_iou >= 0.5;
  metrics.duplicate_response = metrics.same_class_target_overlap_count > 1;
  metrics.cross_class_competition =
      metrics.cross_class_target_overlap_count > 0;
  metrics.top_class_correct =
      metrics.top_class_id >= 0 && metrics.top_class_id == target_class;
  metrics.exact_single_correct =
      metrics.detection_count == 1 && metrics.matched_iou50 &&
      metrics.top_class_correct;
  return metrics;
}

inline int RunYoloV8nPairedInferenceCli(int argc, char **argv) {
  const fs::path package_manifest =
      ArgValue(argc, argv, "--package-manifest");
  const fs::path base_manifest =
      ArgValue(argc, argv, "--base-model-manifest");
  const fs::path incremental_manifest =
      ArgValue(argc, argv, "--incremental-model-manifest");
  const fs::path comparison_plan =
      ArgValue(argc, argv, "--comparison-plan");
  const fs::path output_dir = ArgValue(argc, argv, "--out");
  std::string candidate_postprocess_override =
      ArgValue(argc, argv, "--candidate-postprocess-json");
  const fs::path candidate_postprocess_file =
      ArgValue(argc, argv, "--candidate-postprocess-file");
  if (!candidate_postprocess_file.empty()) {
    std::ifstream input(candidate_postprocess_file, std::ios::binary);
    if (!input) {
      std::cout << "conclusion=ASSET_PREFLIGHT_FAIL\n"
                << "reason=candidate postprocess policy file cannot be read\n";
      return 2;
    }
    candidate_postprocess_override.assign(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
    if (candidate_postprocess_override.empty()) {
      std::cout << "conclusion=ASSET_PREFLIGHT_FAIL\n"
                << "reason=candidate postprocess policy file is empty\n";
      return 2;
    }
  }
  const std::string requested_device =
      ArgValue(argc, argv, "--torch-device").empty()
          ? "cpu"
          : ArgValue(argc, argv, "--torch-device");
  fs::path runtime_dll = ArgValue(argc, argv, "--torch-runtime-dll");
  if (runtime_dll.empty())
    runtime_dll = fs::absolute(argv[0]).parent_path() /
                  "libtorch_module_runtime.dll";

  std::error_code ec;
  if (!fs::is_regular_file(package_manifest, ec) || ec ||
      !fs::is_regular_file(base_manifest, ec) || ec ||
      !fs::is_regular_file(incremental_manifest, ec) || ec ||
      !fs::is_regular_file(comparison_plan, ec) || ec ||
      !fs::is_regular_file(runtime_dll, ec) || ec) {
    std::cout << "conclusion=ASSET_PREFLIGHT_FAIL\n"
              << "reason=paired inference input asset is missing\n";
    return 2;
  }
  if (fs::exists(output_dir, ec)) {
    std::cout << "conclusion=ASSET_PREFLIGHT_FAIL\n"
              << "reason=output directory already exists; use a new RUN_ID\n";
    return 2;
  }

  cv::FileStorage plan(comparison_plan.string(), cv::FileStorage::READ);
  cv::FileStorage package(package_manifest.string(), cv::FileStorage::READ);
  std::string plan_schema;
  plan["schema"] >> plan_schema;
  int max_per_family = 0;
  int max_total = 0;
  std::string selection_mode;
  std::string target_split;
  plan["max_cases_per_signal_family"] >> max_per_family;
  plan["max_total_cases"] >> max_total;
  plan["selection_mode"] >> selection_mode;
  plan["target_split"] >> target_split;
  if (target_split.empty())
    target_split = "validation";
  const cv::FileNode family_nodes = plan["signal_families"];
  const cv::FileNode sample_nodes = package["samples"];
  if (!plan.isOpened() || !package.isOpened() ||
      plan_schema != "cxvision.yolov8n_cpp_comparison_plan.v1" ||
      !family_nodes.isSeq() || !sample_nodes.isSeq() || max_per_family <= 0 ||
      max_total <= 0 ||
      (target_split != "validation" && target_split != "holdout")) {
    std::cout << "conclusion=ASSET_PREFLIGHT_FAIL\n"
              << "reason=paired inference plan or package schema is invalid\n";
    return 2;
  }

  std::vector<std::string> families;
  for (const auto &node : family_nodes)
    families.push_back(static_cast<std::string>(node));
  std::map<std::string, std::vector<Case>> candidates;
  const fs::path package_root = package_manifest.parent_path();
  for (const auto &node : sample_nodes) {
    std::string split;
    std::string review_item;
    std::string geometry_type;
    std::string image_ref;
    std::string label_ref;
    std::string source_case_ref;
    node["split"] >> split;
    node["review_item"] >> review_item;
    node["geometry_type"] >> geometry_type;
    node["image"] >> image_ref;
    node["label"] >> label_ref;
    node["source_case_ref"] >> source_case_ref;
    if (split != target_split || review_item.empty() ||
        geometry_type.empty() || image_ref.empty() || label_ref.empty())
      continue;
    const fs::path image_path = package_root / fs::path(image_ref);
    const fs::path label_path = package_root / fs::path(label_ref);
    if (!fs::is_regular_file(image_path, ec) || ec ||
        !fs::is_regular_file(label_path, ec) || ec)
      continue;
    // Signal flags are preferred.  Asset packages created from the controlled
    // geometry manifest carry the same semantics in source_case_ref instead;
    // use that schema field as a generic fallback, never a case-name table.
    const std::string signal_text = source_case_ref.empty()
        ? review_item : source_case_ref;
    std::string normalized_signal;
    for (const unsigned char ch : signal_text)
      normalized_signal += static_cast<char>(std::tolower(ch));
    const bool rotation_signal = NodeBool(node["rotation_signal"]) ||
        normalized_signal.find("rotation") != std::string::npos;
    const bool scale_signal = NodeBool(node["scale_signal"]) ||
        normalized_signal.find("scale") != std::string::npos;
    const bool deformation_signal = NodeBool(node["deformation_signal"]) ||
        normalized_signal.find("compound") != std::string::npos ||
        normalized_signal.find("deformation") != std::string::npos;
    if (rotation_signal)
      candidates["rotation"].push_back(
          {review_item, geometry_type, "rotation", image_path, label_path});
    if (scale_signal)
      candidates["scale"].push_back(
          {review_item, geometry_type, "scale", image_path, label_path});
    if (deformation_signal)
      candidates["deformation"].push_back(
          {review_item, geometry_type, "deformation", image_path, label_path});
  }

  std::vector<Case> selected;
  std::set<std::string> selected_images;
  for (const auto &family : families) {
    std::set<std::string> used_geometry;
    int family_count = 0;
    for (const auto &candidate : candidates[family]) {
      if (family_count >= max_per_family ||
          static_cast<int>(selected.size()) >= max_total)
        break;
      if (selection_mode != "all_matching_cases" &&
          !used_geometry.insert(candidate.geometry_type).second)
        continue;
      if (!selected_images.insert(candidate.image_path.generic_string()).second)
        continue;
      selected.push_back(candidate);
      ++family_count;
    }
  }
  if (selected.empty()) {
    std::cout << "conclusion=ASSET_PREFLIGHT_FAIL\n"
              << "reason=no validation case matches requested signal families\n";
    return 2;
  }
  fs::create_directories(output_dir, ec);
  if (ec)
    return 2;

#ifdef _WIN32
  const char *old_path = std::getenv("PATH");
  const std::string process_path =
      runtime_dll.parent_path().string() +
      (old_path == nullptr ? "" : ";" + std::string(old_path));
  SetEnvironmentVariableA("PATH", process_path.c_str());
#endif
  CxTorchRuntimeConfig runtime_config;
  runtime_config.runtime_dll_path = runtime_dll.string();
  runtime_config.device = requested_device;
  runtime_config.output_root = output_dir.string();
  CxTorchRuntimeService service;
  std::string reason;
  const auto service_initialize_started = std::chrono::steady_clock::now();
  if (!service.Initialize(runtime_config, reason)) {
    std::cout << "conclusion=CXX_YOLOV8N_PAIRED_INFERENCE_FAIL\nreason="
              << reason << "\n";
    return 3;
  }
  const double service_initialize_ms = static_cast<double>(
      std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::steady_clock::now() - service_initialize_started)
          .count()) /
      1000.0;

  std::ostringstream rows;
  bool all_ok = true;
  int total_base_detections = 0;
  int total_incremental_detections = 0;
  int total_base_matches = 0;
  int total_incremental_matches = 0;
  int total_base_false_alarm_cases = 0;
  int total_incremental_false_alarm_cases = 0;
  int total_incremental_exact_single_correct_cases = 0;
  int total_incremental_duplicate_cases = 0;
  int total_incremental_cross_class_cases = 0;
  int total_incremental_background_fp_cases = 0;
  int total_incremental_miss_cases = 0;
  int total_incremental_target_overlap_boxes = 0;
  int total_incremental_cross_class_overlap_boxes = 0;
  int total_incremental_background_boxes = 0;
  double total_base_match_confidence = 0.0;
  double total_incremental_match_confidence = 0.0;
  std::map<std::string, std::array<double, 4>> group_metrics;
  std::vector<RuntimeProfileSample> profile_samples;
  for (std::size_t i = 0; i < selected.size(); ++i) {
    const Case &item = selected[i];
    const std::string case_dir_name =
        SafeStem(item.family + "_" + item.geometry_type) + "_" +
        StableSuffix(item.review_item);
    const fs::path case_dir = output_dir / "cases" / case_dir_name;
    CxTorchTaskResponse base_response;
    CxTorchTaskResponse incremental_response;
    auto execute = [&](const fs::path &manifest, const fs::path &out,
                       const char *model_role, CxTorchTaskResponse &response,
                       RuntimeProfileSample &profile) {
      CxTorchTaskRequest request;
      request.task = "torch.infer.detection.yolov8.v1";
      request.device = requested_device;
      request.input_image = item.image_path.string();
      request.manifest_path = manifest.string();
      request.case_name = item.review_item;
      request.output_dir = out.string();
      if (std::string(model_role) == "candidate")
        request.extra_json = candidate_postprocess_override;
      std::string execute_reason;
      const auto execute_started = std::chrono::steady_clock::now();
      const bool ok = service.Execute(request, response, execute_reason);
      profile.review_item = item.review_item;
      profile.model_role = model_role;
      profile.service_execute_ms = static_cast<double>(
          std::chrono::duration_cast<std::chrono::microseconds>(
              std::chrono::steady_clock::now() - execute_started)
              .count()) /
          1000.0;
      profile.forward_ms = response.infer_runtime_ms;
      profile.residual_pipeline_ms =
          std::max(0.0, profile.service_execute_ms - profile.forward_ms);
      if (!ok && reason.empty())
        reason = execute_reason;
      return ok;
    };
    RuntimeProfileSample base_profile;
    RuntimeProfileSample incremental_profile;
    const bool base_ok =
        execute(base_manifest, case_dir / "base", "parent", base_response,
                base_profile);
    const bool incremental_ok = execute(incremental_manifest,
                                        case_dir / "incremental",
                                        "candidate", incremental_response,
                                        incremental_profile);
    profile_samples.push_back(base_profile);
    profile_samples.push_back(incremental_profile);
    all_ok = all_ok && base_ok && incremental_ok;
    const DetectionMetrics base_metrics = EvaluateDetections(
        case_dir / "base" / "detections.json", item.label_path,
        item.image_path);
    const DetectionMetrics incremental_metrics = EvaluateDetections(
        case_dir / "incremental" / "detections.json", item.label_path,
        item.image_path);
    total_base_detections += base_metrics.detection_count;
    total_incremental_detections += incremental_metrics.detection_count;
    total_base_matches += base_metrics.matched_iou50 ? 1 : 0;
    total_incremental_matches += incremental_metrics.matched_iou50 ? 1 : 0;
    total_base_false_alarm_cases +=
        base_metrics.detection_count > (base_metrics.matched_iou50 ? 1 : 0) ? 1 : 0;
    total_incremental_false_alarm_cases +=
        incremental_metrics.detection_count >
                (incremental_metrics.matched_iou50 ? 1 : 0)
            ? 1
            : 0;
    total_incremental_exact_single_correct_cases +=
        incremental_metrics.exact_single_correct ? 1 : 0;
    total_incremental_duplicate_cases +=
        incremental_metrics.duplicate_response ? 1 : 0;
    total_incremental_cross_class_cases +=
        incremental_metrics.cross_class_competition ? 1 : 0;
    total_incremental_background_fp_cases +=
        incremental_metrics.background_detection_count > 0 ? 1 : 0;
    total_incremental_miss_cases +=
        incremental_metrics.matched_iou50 ? 0 : 1;
    total_incremental_target_overlap_boxes +=
        incremental_metrics.target_overlap_count;
    total_incremental_cross_class_overlap_boxes +=
        incremental_metrics.cross_class_target_overlap_count;
    total_incremental_background_boxes +=
        incremental_metrics.background_detection_count;
    total_base_match_confidence += base_metrics.best_same_class_confidence;
    total_incremental_match_confidence +=
        incremental_metrics.best_same_class_confidence;
    auto &family_metrics = group_metrics["family:" + item.family];
    family_metrics[0] += 1.0;
    family_metrics[1] += incremental_metrics.detection_count > 0 ? 1.0 : 0.0;
    family_metrics[2] += incremental_metrics.matched_iou50 ? 1.0 : 0.0;
    family_metrics[3] += incremental_metrics.best_same_class_iou;
    auto &geometry_metrics =
        group_metrics["geometry:" + item.geometry_type];
    geometry_metrics[0] += 1.0;
    geometry_metrics[1] +=
        incremental_metrics.detection_count > 0 ? 1.0 : 0.0;
    geometry_metrics[2] += incremental_metrics.matched_iou50 ? 1.0 : 0.0;
    geometry_metrics[3] += incremental_metrics.best_same_class_iou;
    if (i != 0)
      rows << ",\n";
    rows << "    {\"review_item\": \"" << Escape(item.review_item)
         << "\", \"geometry_type\": \"" << Escape(item.geometry_type)
         << "\", \"signal_family\": \"" << Escape(item.family)
         << "\", \"input_image\": \""
         << Escape(item.image_path.generic_string())
         << "\", \"base_ok\": " << (base_ok ? "true" : "false")
         << ", \"incremental_ok\": "
         << (incremental_ok ? "true" : "false")
         << ", \"base_detection_count\": "
         << base_metrics.detection_count
         << ", \"incremental_detection_count\": "
         << incremental_metrics.detection_count
         << ", \"incremental_target_overlap_count\": "
         << incremental_metrics.target_overlap_count
         << ", \"incremental_same_class_target_overlap_count\": "
         << incremental_metrics.same_class_target_overlap_count
         << ", \"incremental_cross_class_target_overlap_count\": "
         << incremental_metrics.cross_class_target_overlap_count
         << ", \"incremental_background_detection_count\": "
         << incremental_metrics.background_detection_count
         << ", \"incremental_top_class_id\": "
         << incremental_metrics.top_class_id
         << ", \"incremental_target_class_id\": "
         << incremental_metrics.target_class_id
         << ", \"incremental_exact_single_correct\": "
         << (incremental_metrics.exact_single_correct ? "true" : "false")
         << ", \"incremental_duplicate_response\": "
         << (incremental_metrics.duplicate_response ? "true" : "false")
         << ", \"incremental_cross_class_competition\": "
         << (incremental_metrics.cross_class_competition ? "true" : "false")
         << ", \"base_best_same_class_iou\": "
         << base_metrics.best_same_class_iou
         << ", \"incremental_best_same_class_iou\": "
         << incremental_metrics.best_same_class_iou
         << ", \"base_best_same_class_confidence\": "
         << base_metrics.best_same_class_confidence
         << ", \"incremental_best_same_class_confidence\": "
         << incremental_metrics.best_same_class_confidence
         << ", \"base_iou50_match\": "
         << (base_metrics.matched_iou50 ? "true" : "false")
         << ", \"incremental_iou50_match\": "
         << (incremental_metrics.matched_iou50 ? "true" : "false")
         << ", \"base_overlay\": \""
         << Escape(base_response.primary_visual_ref)
         << "\", \"incremental_overlay\": \""
         << Escape(incremental_response.primary_visual_ref)
         << "\", \"base_candidates\": \""
         << Escape(base_response.bbox_candidate_list_ref)
         << "\", \"incremental_candidates\": \""
         << Escape(incremental_response.bbox_candidate_list_ref)
         << "\", \"parent_service_execute_ms\": "
         << base_profile.service_execute_ms
         << ", \"parent_forward_ms\": " << base_profile.forward_ms
         << ", \"candidate_service_execute_ms\": "
         << incremental_profile.service_execute_ms
         << ", \"candidate_forward_ms\": "
         << incremental_profile.forward_ms << "}";
  }
  service.Shutdown();

  std::vector<double> execute_samples;
  std::vector<double> forward_samples;
  std::vector<double> residual_samples;
  execute_samples.reserve(profile_samples.size());
  forward_samples.reserve(profile_samples.size());
  residual_samples.reserve(profile_samples.size());
  for (const RuntimeProfileSample &sample : profile_samples) {
    execute_samples.push_back(sample.service_execute_ms);
    forward_samples.push_back(sample.forward_ms);
    residual_samples.push_back(sample.residual_pipeline_ms);
  }
  const RuntimeProfileSummary execute_summary =
      SummarizeRuntimeSamples(execute_samples);
  const RuntimeProfileSummary forward_summary =
      SummarizeRuntimeSamples(forward_samples);
  const RuntimeProfileSummary residual_summary =
      SummarizeRuntimeSamples(residual_samples);
  const fs::path performance_profile_path = output_dir / "performance_profile.json";
  std::ofstream performance_profile(performance_profile_path);
  performance_profile << "{\n"
      << "  \"schema\": \"cxvision.model_performance_profile.v1\",\n"
      << "  \"status\": \"PERFORMANCE_PROFILE_EXECUTION_PASS\",\n"
      << "  \"profile_scope\": \"asset-selected parent/candidate paired detection inference\",\n"
      << "  \"requested_device\": \"" << Escape(requested_device) << "\",\n"
      << "  \"service_initialize_ms\": " << service_initialize_ms << ",\n"
      << "  \"sample_count\": " << profile_samples.size() << ",\n"
      << "  \"measurement_notes\": [\n"
      << "    \"service_execute is measured around the full runtime service call\",\n"
      << "    \"forward is reported by the runtime model execution\",\n"
      << "    \"residual_pipeline is service_execute minus forward and includes preprocess, model load, postprocess, and evidence writes\"\n"
      << "  ],\n"
      << "  \"stages\": {\n";
  const auto write_summary = [&performance_profile](const char *name,
                                                      const RuntimeProfileSummary &summary,
                                                      const bool comma) {
    performance_profile << "    \"" << name << "\": {\"min_ms\": "
        << summary.min_ms << ", \"mean_ms\": " << summary.mean_ms
        << ", \"p50_ms\": " << summary.p50_ms << ", \"p95_ms\": "
        << summary.p95_ms << ", \"p99_ms\": " << summary.p99_ms
        << ", \"max_ms\": " << summary.max_ms << "}"
        << (comma ? ",\n" : "\n");
  };
  write_summary("service_execute", execute_summary, true);
  write_summary("model_forward", forward_summary, true);
  write_summary("residual_pipeline", residual_summary, false);
  performance_profile << "  },\n  \"samples\": [\n";
  for (std::size_t sample_index = 0; sample_index < profile_samples.size();
       ++sample_index) {
    const RuntimeProfileSample &sample = profile_samples[sample_index];
    performance_profile << "    {\"review_item\": \""
        << Escape(sample.review_item) << "\", \"model_role\": \""
        << Escape(sample.model_role) << "\", \"service_execute_ms\": "
        << sample.service_execute_ms << ", \"forward_ms\": "
        << sample.forward_ms << ", \"residual_pipeline_ms\": "
        << sample.residual_pipeline_ms << "}"
        << (sample_index + 1U < profile_samples.size() ? ",\n" : "\n");
  }
  performance_profile << "  ],\n  \"promotion_allowed\": false\n}\n";

  const double incremental_mean_boxes_per_image =
      selected.empty() ? 0.0
                       : static_cast<double>(total_incremental_detections) /
                             static_cast<double>(selected.size());
  const double incremental_exact_single_correct_rate =
      selected.empty() ? 0.0
                       : static_cast<double>(
                             total_incremental_exact_single_correct_cases) /
                             static_cast<double>(selected.size());
  const double incremental_duplicate_response_rate =
      selected.empty() ? 0.0
                       : static_cast<double>(total_incremental_duplicate_cases) /
                             static_cast<double>(selected.size());
  const double incremental_cross_class_competition_rate =
      selected.empty() ? 0.0
                       : static_cast<double>(total_incremental_cross_class_cases) /
                             static_cast<double>(selected.size());
  const double incremental_background_fp_case_rate =
      selected.empty() ? 0.0
                       : static_cast<double>(total_incremental_background_fp_cases) /
                             static_cast<double>(selected.size());
  const double incremental_miss_rate =
      selected.empty() ? 0.0
                       : static_cast<double>(total_incremental_miss_cases) /
                             static_cast<double>(selected.size());
  std::string primary_issue = "CONTRACT_SATISFIED";
  if (total_incremental_background_fp_cases > 0)
    primary_issue = "BACKGROUND_FALSE_POSITIVE";
  else if (total_incremental_cross_class_cases > 0)
    primary_issue = "CROSS_CLASS_COMPETITION";
  else if (total_incremental_duplicate_cases > 0)
    primary_issue = "NEARBY_DUPLICATE_RESPONSE";
  else if (total_incremental_miss_cases > 0)
    primary_issue = "MISS";
  const fs::path contract_observation_path =
      output_dir / "detection_contract_observation.json";
  std::ofstream contract_observation(contract_observation_path);
  contract_observation
      << "{\n"
      << "  \"schema\": \"cxvision.detection_contract_observation.v1\",\n"
      << "  \"contract_scope\": \"single_target_observation_only\",\n"
      << "  \"raw_feature_level_evidence\": "
         "\"NOT_AVAILABLE_IN_FINAL_DETECTIONS_ONLY\",\n"
      << "  \"multiscale_duplicate_claim\": "
         "\"NOT_ESTABLISHED_FROM_FINAL_DETECTIONS_ONLY\",\n"
      << "  \"expected_targets_per_image\": 1,\n"
      << "  \"selected_image_count\": " << selected.size() << ",\n"
      << "  \"candidate_mean_boxes_per_image\": "
      << incremental_mean_boxes_per_image << ",\n"
      << "  \"candidate_exact_single_correct_rate\": "
      << incremental_exact_single_correct_rate << ",\n"
      << "  \"candidate_duplicate_response_rate\": "
      << incremental_duplicate_response_rate << ",\n"
      << "  \"candidate_cross_class_competition_rate\": "
      << incremental_cross_class_competition_rate << ",\n"
      << "  \"candidate_background_fp_case_rate\": "
      << incremental_background_fp_case_rate << ",\n"
      << "  \"candidate_miss_rate\": " << incremental_miss_rate << ",\n"
      << "  \"candidate_target_overlap_boxes\": "
      << total_incremental_target_overlap_boxes << ",\n"
      << "  \"candidate_cross_class_overlap_boxes\": "
      << total_incremental_cross_class_overlap_boxes << ",\n"
      << "  \"candidate_background_boxes\": "
      << total_incremental_background_boxes << ",\n"
      << "  \"primary_issue\": \"" << primary_issue << "\",\n"
      << "  \"localization_gate_enabled\": "
      << (incremental_exact_single_correct_rate == 1.0 ? "true" : "false")
      << ",\n"
      << "  \"promotion_allowed\": false\n"
      << "}\n";

  const std::string status =
      all_ok ? "CXX_YOLOV8N_PAIRED_INFERENCE_EXECUTION_PASS"
             : "CXX_YOLOV8N_PAIRED_INFERENCE_FAIL";
  const std::string quality_status =
      all_ok && total_incremental_matches == static_cast<int>(selected.size())
          ? "CXX_YOLOV8N_DETECTION_EFFECT_PENDING_HUMAN_REVIEW"
          : (total_incremental_matches > 0
                 ? "CXX_YOLOV8N_DETECTION_EFFECT_PARTIAL"
                 : "CXX_YOLOV8N_DETECTION_EFFECT_NOT_ESTABLISHED");
  const double selected_count = static_cast<double>(selected.size());
  const double base_precision = total_base_detections > 0
                                    ? static_cast<double>(total_base_matches) /
                                          total_base_detections
                                    : 0.0;
  const double candidate_precision = total_incremental_detections > 0
                                         ? static_cast<double>(total_incremental_matches) /
                                               total_incremental_detections
                                         : 0.0;
  const double base_recall = total_base_matches / selected_count;
  const double candidate_recall = total_incremental_matches / selected_count;
  std::ostringstream groups;
  bool first_group = true;
  for (const auto &entry : group_metrics) {
    if (!first_group)
      groups << ",\n";
    first_group = false;
    groups << "    {\"group\": \"" << Escape(entry.first)
           << "\", \"case_count\": " << entry.second[0]
           << ", \"detected_case_count\": " << entry.second[1]
           << ", \"iou50_match_count\": " << entry.second[2]
           << ", \"average_best_same_class_iou\": "
           << (entry.second[0] > 0.0 ? entry.second[3] / entry.second[0]
                                     : 0.0)
           << "}";
  }
  const fs::path report_path = output_dir / "paired_inference_report.json";
  std::ofstream report(report_path);
  report << "{\n"
         << "  \"schema\": \"cxvision.yolov8n_cpp_paired_inference.v1\",\n"
         << "  \"status\": \"" << status << "\",\n"
         << "  \"quality_status\": \"" << quality_status << "\",\n"
         << "  \"evaluation_split\": \"" << target_split << "\",\n"
         << "  \"selected_case_count\": " << selected.size() << ",\n"
         << "  \"total_base_detections\": " << total_base_detections
         << ",\n"
         << "  \"total_incremental_detections\": "
         << total_incremental_detections << ",\n"
         << "  \"total_base_iou50_matches\": " << total_base_matches
         << ",\n"
         << "  \"total_incremental_iou50_matches\": "
         << total_incremental_matches << ",\n"
         << "  \"metrics\": {\n"
         << "    \"base_precision\": " << base_precision << ",\n"
         << "    \"base_recall\": " << base_recall << ",\n"
         << "    \"base_miss_rate\": " << (1.0 - base_recall) << ",\n"
         << "    \"base_false_alarm_case_rate\": "
         << total_base_false_alarm_cases / selected_count << ",\n"
         << "    \"base_mean_match_confidence\": "
         << total_base_match_confidence / selected_count << ",\n"
         << "    \"candidate_precision\": " << candidate_precision << ",\n"
         << "    \"candidate_recall\": " << candidate_recall << ",\n"
         << "    \"candidate_miss_rate\": " << (1.0 - candidate_recall)
         << ",\n"
         << "    \"candidate_false_alarm_case_rate\": "
         << total_incremental_false_alarm_cases / selected_count << ",\n"
         << "    \"candidate_mean_match_confidence\": "
         << total_incremental_match_confidence / selected_count << ",\n"
         << "    \"candidate_mean_boxes_per_image\": "
         << incremental_mean_boxes_per_image << ",\n"
         << "    \"candidate_exact_single_correct_rate\": "
         << incremental_exact_single_correct_rate << ",\n"
         << "    \"candidate_duplicate_response_rate\": "
         << incremental_duplicate_response_rate << ",\n"
         << "    \"candidate_cross_class_competition_rate\": "
         << incremental_cross_class_competition_rate << ",\n"
         << "    \"candidate_background_fp_case_rate\": "
         << incremental_background_fp_case_rate << ",\n"
         << "    \"candidate_contract_miss_rate\": "
         << incremental_miss_rate << "\n"
         << "  },\n"
         << "  \"performance_profile_ref\": \""
         << Escape(performance_profile_path.string()) << "\",\n"
         << "  \"detection_contract_observation_ref\": \""
         << Escape(contract_observation_path.string()) << "\",\n"
         << "  \"primary_issue\": \"" << primary_issue << "\",\n"
         << "  \"promotion_allowed\": false,\n"
         << "  \"groups\": [\n" << groups.str() << "\n  ],\n"
         << "  \"cases\": [\n" << rows.str() << "\n  ]\n}\n";
  std::cout << "conclusion=" << status << "\n"
            << "selected_case_count=" << selected.size() << "\n"
            << "quality_status=" << quality_status << "\n"
            << "total_incremental_iou50_matches="
            << total_incremental_matches << "\n"
            << "report_path=" << report_path.generic_string() << "\n"
            << "reason=" << reason << "\n";
  return all_ok ? 0 : 3;
}

} // namespace cxvision_yolov8n_paired_inference
