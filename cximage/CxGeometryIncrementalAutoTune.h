#pragma once

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/photo.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// Evidence-driven auto tuning for the controlled seven-class geometry track.
// The executor intentionally separates selection from confirmation:
// validation selects an inference profile, while holdout is read exactly once
// after that profile is frozen.  Architecture and model promotion remain
// explicit follow-up operations and are never performed by this executor.
namespace cxvision_geometry_auto_tune {
namespace fs = std::filesystem;

struct OntologyClass {
  int id = -1;
  std::string name;
  std::string display_name;
};

struct LegacyAlias {
  std::string source;
  std::string target;
  int class_id = -1;
  bool requires_receipt = false;
  std::string reason;
};

struct Ontology {
  std::string schema;
  std::string version;
  bool frozen = false;
  std::vector<OntologyClass> classes;
  std::map<std::string, LegacyAlias> aliases;
};

struct TrainingRecipeDefaults {
  int minimum_complete_epochs = 0;
  int recommended_max_epochs = 0;
  int max_train_batches_per_epoch = 0;
  std::string assignment_scope;
  std::vector<int> assignment_topk_candidates;
  std::vector<double> focal_gamma_candidates;
  bool use_assignment_quality_targets = false;
  bool use_dfl_box_regression = false;
  int dfl_reg_max = 0;
  bool class_balanced_sampling = false;
  double hard_confusion_oversample_factor = 0.0;
  std::vector<std::string> unfreeze_sequence;
  std::string validation_metric;
  int validation_interval = 0;
  int early_stop_patience = 0;
  double minimum_f1_delta = 0.0;
  bool restore_best_validation_checkpoint = false;
};

struct StageDefinition {
  int order = -1;
  std::string id;
  std::string action;
};

struct Policy {
  std::string schema;
  std::string policy_id;
  std::string required_ontology_schema;
  int required_class_count = 0;
  int expected_instances_per_roi = 0;
  std::string selection_split;
  std::string confirmation_split;
  bool holdout_used_for_selection = true;
  std::string selection_metric;
  double match_iou_threshold = 0.0;
  double source_confidence_threshold = 0.0;
  int source_topk = 0;
  std::vector<double> confidence_candidates;
  std::vector<int> topk_candidates;
  double minimum_validation_f1_gain = 0.0;
  double maximum_holdout_f1_drop_vs_parent = 0.0;
  double maximum_holdout_miss_rate = 0.0;
  double maximum_holdout_false_alarm_case_rate = 0.0;
  bool architecture_auto_mutation_allowed = true;
  bool model_auto_promotion_allowed = true;
  TrainingRecipeDefaults training;
  std::vector<StageDefinition> stages;
};

struct PackageSample {
  std::string review_item;
  std::string split;
  std::string source_geometry_type;
  std::string canonical_geometry_type;
  fs::path image_path;
  fs::path label_path;
  fs::path target_mask_path;
  bool alias_used = false;
};

struct Detection {
  double x1 = 0.0;
  double y1 = 0.0;
  double x2 = 0.0;
  double y2 = 0.0;
  double confidence = 0.0;
  int class_id = -1;
};

struct EvidenceCase {
  std::string review_item;
  std::string split;
  std::string source_geometry_type;
  std::string canonical_geometry_type;
  std::string signal_family;
  fs::path image_path;
  fs::path label_path;
  int target_class = -1;
  double target_x1 = 0.0;
  double target_y1 = 0.0;
  double target_x2 = 0.0;
  double target_y2 = 0.0;
  bool alias_used = false;
  std::vector<Detection> parent;
  std::vector<Detection> candidate;
};

struct PerClassMetrics {
  int ground_truth = 0;
  int true_positive = 0;
  int false_positive = 0;
  int false_negative = 0;
};

struct FailureObservation {
  std::string review_item;
  std::string split;
  std::string geometry_type;
  std::string failure_class;
  int predicted_class = -1;
  double confidence = 0.0;
  double iou = 0.0;
};

struct Metrics {
  int case_count = 0;
  int true_positive = 0;
  int false_positive = 0;
  int false_negative = 0;
  int false_alarm_cases = 0;
  int selected_detection_count = 0;
  int raw_detection_count = 0;
  int no_detection_count = 0;
  int wrong_class_count = 0;
  int localization_failure_count = 0;
  double precision = 0.0;
  double recall = 0.0;
  double f1 = 0.0;
  double miss_rate = 0.0;
  double false_alarm_case_rate = 0.0;
  double mean_match_confidence = 0.0;
  double raw_detections_per_case = 0.0;
  std::vector<PerClassMetrics> per_class;
  std::vector<std::vector<int>> confusion;
  std::vector<FailureObservation> failures;
};

struct SweepRow {
  double confidence_threshold = 0.0;
  int topk = 0;
  Metrics parent;
  Metrics candidate;
};

inline std::string ArgValue(int argc, char **argv,
                            const std::string &name) {
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

inline std::string JsonEscape(const std::string &value) {
  std::string out;
  for (const char ch : value) {
    switch (ch) {
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
      out += ch;
      break;
    }
  }
  return out;
}

inline std::string ReadString(const cv::FileNode &node, const char *key) {
  const cv::FileNode value = node[key];
  return value.empty() ? std::string() : static_cast<std::string>(value);
}

inline bool ReadBool(const cv::FileNode &node, const char *key,
                     bool default_value = false) {
  const cv::FileNode value = node[key];
  if (value.empty())
    return default_value;
  if (value.isString()) {
    std::string text = static_cast<std::string>(value);
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char ch) {
                     return static_cast<char>(std::tolower(ch));
                   });
    return text == "true" || text == "yes" || text == "1";
  }
  return static_cast<int>(value) != 0;
}

inline std::string NormalizeToken(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char ch) {
                   return static_cast<char>(std::tolower(ch));
                 });
  return value;
}

inline std::string PathKey(const fs::path &path) {
  std::error_code ec;
  fs::path normalized = fs::weakly_canonical(path, ec);
  if (ec) {
    ec.clear();
    normalized = fs::absolute(path, ec).lexically_normal();
  }
  std::string key = normalized.generic_string();
#ifdef _WIN32
  key = NormalizeToken(key);
#endif
  return key;
}

inline bool IsRegularFile(const fs::path &path) {
  std::error_code ec;
  return !path.empty() && fs::is_regular_file(path, ec) && !ec;
}

inline fs::path ResolveReference(const fs::path &owner,
                                 const std::string &reference) {
  fs::path path(reference);
  if (path.is_relative())
    path = owner.parent_path() / path;
  return path.lexically_normal();
}

inline bool WriteText(const fs::path &path, const std::string &text,
                      std::string &reason) {
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream.is_open()) {
    reason = "cannot open output file: " + path.string();
    return false;
  }
  stream << text;
  if (!stream.good()) {
    reason = "cannot write output file: " + path.string();
    return false;
  }
  return true;
}

inline std::string FileDigest(const fs::path &path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream.is_open())
    return {};
  unsigned long long value = 1469598103934665603ULL;
  char buffer[8192];
  while (stream.good()) {
    stream.read(buffer, sizeof(buffer));
    const std::streamsize count = stream.gcount();
    for (std::streamsize index = 0; index < count; ++index) {
      value ^= static_cast<unsigned char>(buffer[index]);
      value *= 1099511628211ULL;
    }
  }
  std::ostringstream out;
  out << "fnv1a64:" << std::hex << std::setw(16) << std::setfill('0')
      << value;
  return out.str();
}

inline std::string CanonicalGeometryType(const Ontology &ontology,
                                         const std::string &source,
                                         bool &alias_used) {
  alias_used = false;
  const std::string normalized = NormalizeToken(source);
  for (const OntologyClass &item : ontology.classes) {
    if (item.name == normalized)
      return item.name;
  }
  const auto alias = ontology.aliases.find(normalized);
  if (alias != ontology.aliases.end()) {
    alias_used = true;
    return alias->second.target;
  }
  return {};
}

inline int ClassIdForName(const Ontology &ontology,
                          const std::string &name) {
  for (const OntologyClass &item : ontology.classes) {
    if (item.name == name)
      return item.id;
  }
  return -1;
}

inline bool LoadOntology(const fs::path &path, Ontology &ontology,
                         std::string &reason) {
  cv::FileStorage storage(path.string(), cv::FileStorage::READ);
  if (!storage.isOpened()) {
    reason = "ontology cannot be opened";
    return false;
  }
  Ontology parsed;
  const cv::FileNode root = storage.root();
  parsed.schema = ReadString(root, "schema");
  parsed.version = ReadString(root, "version");
  parsed.frozen = ReadBool(root, "delivery_frozen");
  int declared_count = 0;
  root["class_count"] >> declared_count;
  const cv::FileNode classes = root["classes"];
  if (parsed.schema.empty() || parsed.version.empty() || !parsed.frozen ||
      !classes.isSeq()) {
    reason = "ontology schema, version, frozen flag or classes is invalid";
    return false;
  }
  std::map<int, std::string> ids;
  std::map<std::string, int> names;
  for (const cv::FileNode &node : classes) {
    OntologyClass item;
    node["id"] >> item.id;
    item.name = NormalizeToken(ReadString(node, "name"));
    item.display_name = ReadString(node, "display_name");
    if (item.id < 0 || item.name.empty() || ids.count(item.id) != 0 ||
        names.count(item.name) != 0) {
      reason = "ontology has an invalid or duplicate class id/name";
      return false;
    }
    ids[item.id] = item.name;
    names[item.name] = item.id;
    parsed.classes.push_back(std::move(item));
  }
  std::sort(parsed.classes.begin(), parsed.classes.end(),
            [](const OntologyClass &left, const OntologyClass &right) {
              return left.id < right.id;
            });
  if (declared_count <= 0 ||
      declared_count != static_cast<int>(parsed.classes.size())) {
    reason = "ontology class_count does not match classes";
    return false;
  }
  for (int id = 0; id < declared_count; ++id) {
    if (parsed.classes[static_cast<std::size_t>(id)].id != id) {
      reason = "ontology class ids must be contiguous and ordered from zero";
      return false;
    }
  }
  const cv::FileNode aliases = root["legacy_aliases"];
  if (!aliases.empty()) {
    if (!aliases.isSeq()) {
      reason = "legacy_aliases must be a sequence";
      return false;
    }
    for (const cv::FileNode &node : aliases) {
      LegacyAlias alias;
      alias.source = NormalizeToken(ReadString(node, "source"));
      alias.target = NormalizeToken(ReadString(node, "target"));
      node["class_id"] >> alias.class_id;
      alias.requires_receipt =
          ReadBool(node, "requires_migration_receipt");
      alias.reason = ReadString(node, "reason");
      if (alias.source.empty() || alias.target.empty() ||
          ClassIdForName(parsed, alias.target) != alias.class_id ||
          parsed.aliases.count(alias.source) != 0) {
        reason = "legacy alias does not resolve to the declared class id";
        return false;
      }
      parsed.aliases[alias.source] = std::move(alias);
    }
  }
  ontology = std::move(parsed);
  reason = "frozen ontology loaded";
  return true;
}

inline bool LoadIntList(const cv::FileNode &node, std::vector<int> &values) {
  if (!node.isSeq())
    return false;
  for (const cv::FileNode &item : node) {
    int value = 0;
    item >> value;
    values.push_back(value);
  }
  return !values.empty();
}

inline bool LoadDoubleList(const cv::FileNode &node,
                           std::vector<double> &values) {
  if (!node.isSeq())
    return false;
  for (const cv::FileNode &item : node) {
    double value = 0.0;
    item >> value;
    values.push_back(value);
  }
  return !values.empty();
}

inline bool LoadStringList(const cv::FileNode &node,
                           std::vector<std::string> &values) {
  if (!node.isSeq())
    return false;
  for (const cv::FileNode &item : node)
    values.push_back(static_cast<std::string>(item));
  return !values.empty();
}

inline bool LoadPolicy(const fs::path &path, Policy &policy,
                       std::string &reason) {
  cv::FileStorage storage(path.string(), cv::FileStorage::READ);
  if (!storage.isOpened()) {
    reason = "auto-tune policy cannot be opened";
    return false;
  }
  const cv::FileNode root = storage.root();
  Policy parsed;
  parsed.schema = ReadString(root, "schema");
  parsed.policy_id = ReadString(root, "policy_id");
  parsed.required_ontology_schema =
      ReadString(root, "required_ontology_schema");
  root["required_class_count"] >> parsed.required_class_count;
  root["expected_instances_per_roi"] >> parsed.expected_instances_per_roi;
  parsed.selection_split = ReadString(root, "selection_split");
  parsed.confirmation_split = ReadString(root, "confirmation_split");
  parsed.holdout_used_for_selection =
      ReadBool(root, "holdout_used_for_selection", true);
  parsed.selection_metric = ReadString(root, "selection_metric");
  root["match_iou_threshold"] >> parsed.match_iou_threshold;
  root["source_profile_confidence_threshold"] >>
      parsed.source_confidence_threshold;
  root["source_profile_topk"] >> parsed.source_topk;
  root["minimum_validation_f1_gain"] >>
      parsed.minimum_validation_f1_gain;
  root["maximum_holdout_f1_drop_vs_parent"] >>
      parsed.maximum_holdout_f1_drop_vs_parent;
  root["maximum_holdout_miss_rate"] >> parsed.maximum_holdout_miss_rate;
  root["maximum_holdout_false_alarm_case_rate"] >>
      parsed.maximum_holdout_false_alarm_case_rate;
  parsed.architecture_auto_mutation_allowed =
      ReadBool(root, "architecture_auto_mutation_allowed", true);
  parsed.model_auto_promotion_allowed =
      ReadBool(root, "model_auto_promotion_allowed", true);
  if (!LoadDoubleList(root["confidence_threshold_candidates"],
                      parsed.confidence_candidates) ||
      !LoadIntList(root["topk_candidates"], parsed.topk_candidates)) {
    reason = "auto-tune confidence or top-k search space is missing";
    return false;
  }
  const cv::FileNode training = root["training_recipe_defaults"];
  if (!training.isMap()) {
    reason = "training_recipe_defaults is missing";
    return false;
  }
  training["minimum_complete_epochs"] >>
      parsed.training.minimum_complete_epochs;
  training["recommended_max_epochs"] >>
      parsed.training.recommended_max_epochs;
  training["max_train_batches_per_epoch"] >>
      parsed.training.max_train_batches_per_epoch;
  parsed.training.assignment_scope =
      ReadString(training, "assignment_scope");
  LoadIntList(training["assignment_topk_candidates"],
              parsed.training.assignment_topk_candidates);
  LoadDoubleList(training["classification_focal_gamma_candidates"],
                 parsed.training.focal_gamma_candidates);
  parsed.training.use_assignment_quality_targets =
      ReadBool(training, "use_assignment_quality_targets");
  parsed.training.use_dfl_box_regression =
      ReadBool(training, "use_dfl_box_regression");
  training["dfl_reg_max"] >> parsed.training.dfl_reg_max;
  parsed.training.class_balanced_sampling =
      ReadBool(training, "class_balanced_sampling");
  training["hard_confusion_oversample_factor"] >>
      parsed.training.hard_confusion_oversample_factor;
  LoadStringList(training["unfreeze_sequence"],
                 parsed.training.unfreeze_sequence);
  parsed.training.validation_metric =
      ReadString(training, "validation_metric");
  training["validation_interval"] >> parsed.training.validation_interval;
  training["early_stop_patience"] >>
      parsed.training.early_stop_patience;
  training["minimum_f1_delta"] >> parsed.training.minimum_f1_delta;
  parsed.training.restore_best_validation_checkpoint =
      ReadBool(training, "restore_best_validation_checkpoint");

  const cv::FileNode stages = root["stages"];
  if (!stages.isSeq()) {
    reason = "auto-tune stages are missing";
    return false;
  }
  for (const cv::FileNode &node : stages) {
    StageDefinition stage;
    node["order"] >> stage.order;
    stage.id = ReadString(node, "id");
    stage.action = ReadString(node, "action");
    if (stage.order < 0 || stage.id.empty() || stage.action.empty()) {
      reason = "auto-tune stage is invalid";
      return false;
    }
    parsed.stages.push_back(std::move(stage));
  }
  std::sort(parsed.stages.begin(), parsed.stages.end(),
            [](const StageDefinition &left, const StageDefinition &right) {
              return left.order < right.order;
            });
  const bool invalid =
      parsed.schema.empty() || parsed.policy_id.empty() ||
      parsed.required_ontology_schema.empty() ||
      parsed.required_class_count <= 0 ||
      parsed.expected_instances_per_roi <= 0 ||
      parsed.selection_split.empty() || parsed.confirmation_split.empty() ||
      parsed.selection_split == parsed.confirmation_split ||
      parsed.holdout_used_for_selection || parsed.selection_metric != "f1" ||
      parsed.match_iou_threshold <= 0.0 ||
      parsed.match_iou_threshold > 1.0 ||
      parsed.source_confidence_threshold < 0.0 ||
      parsed.source_confidence_threshold > 1.0 || parsed.source_topk <= 0 ||
      parsed.architecture_auto_mutation_allowed ||
      parsed.model_auto_promotion_allowed || parsed.stages.empty() ||
      parsed.training.minimum_complete_epochs <= 0 ||
      parsed.training.recommended_max_epochs <
          parsed.training.minimum_complete_epochs ||
      parsed.training.assignment_scope.empty() ||
      parsed.training.assignment_topk_candidates.empty() ||
      parsed.training.focal_gamma_candidates.empty() ||
      parsed.training.validation_metric.empty();
  if (invalid) {
    std::ostringstream detail;
    detail << "auto-tune policy violates controlled contract: schema="
           << parsed.schema << " policy_id=" << parsed.policy_id
           << " ontology_schema=" << parsed.required_ontology_schema
           << " class_count=" << parsed.required_class_count
           << " expected_instances=" << parsed.expected_instances_per_roi
           << " selection=" << parsed.selection_split
           << " confirmation=" << parsed.confirmation_split
           << " holdout_selection=" << parsed.holdout_used_for_selection
           << " metric=" << parsed.selection_metric
           << " match_iou=" << parsed.match_iou_threshold
           << " source_conf=" << parsed.source_confidence_threshold
           << " source_topk=" << parsed.source_topk
           << " architecture_auto_mutation="
           << parsed.architecture_auto_mutation_allowed
           << " model_auto_promotion=" << parsed.model_auto_promotion_allowed
           << " stages=" << parsed.stages.size()
           << " minimum_epochs=" << parsed.training.minimum_complete_epochs
           << " maximum_epochs=" << parsed.training.recommended_max_epochs
           << " assignment_scope=" << parsed.training.assignment_scope
           << " assignment_candidates="
           << parsed.training.assignment_topk_candidates.size()
           << " focal_candidates=" << parsed.training.focal_gamma_candidates.size()
           << " validation_metric=" << parsed.training.validation_metric;
    reason = detail.str();
    return false;
  }
  for (const double threshold : parsed.confidence_candidates) {
    if (threshold < 0.0 || threshold > 1.0) {
      reason = "confidence search value is outside [0, 1]";
      return false;
    }
  }
  for (const int topk : parsed.topk_candidates) {
    if (topk <= 0) {
      reason = "top-k search value must be positive";
      return false;
    }
  }
  policy = std::move(parsed);
  reason = "controlled auto-tune policy loaded";
  return true;
}

inline bool LoadPackageSamples(
    const fs::path &manifest_path, const Ontology &ontology,
    const Policy &policy,
    std::unordered_map<std::string, PackageSample> &samples,
    int &legacy_package_class_count, std::string &reason) {
  cv::FileStorage storage(manifest_path.string(), cv::FileStorage::READ);
  if (!storage.isOpened()) {
    reason = "package manifest cannot be opened";
    return false;
  }
  const cv::FileNode root = storage.root();
  const std::string schema = ReadString(root, "schema");
  if (schema != "cxvision.yolov8n_aabb_package.v1" &&
      schema != "cxvision.yolov8n_aabb_mask_package.v1") {
    reason = "package schema is unsupported";
    return false;
  }
  const cv::FileNode package_classes = root["classes"];
  if (!package_classes.isSeq() ||
      static_cast<int>(package_classes.size()) != policy.required_class_count) {
    reason = "package class count does not match frozen policy";
    return false;
  }
  legacy_package_class_count = 0;
  int class_index = 0;
  for (const cv::FileNode &node : package_classes) {
    bool alias_used = false;
    const std::string source = static_cast<std::string>(node);
    const std::string canonical =
        CanonicalGeometryType(ontology, source, alias_used);
    if (canonical.empty() || ClassIdForName(ontology, canonical) != class_index) {
      reason = "package class order does not match frozen ontology";
      return false;
    }
    legacy_package_class_count += alias_used ? 1 : 0;
    ++class_index;
  }
  const cv::FileNode nodes = root["samples"];
  if (!nodes.isSeq() || nodes.empty()) {
    reason = "package samples are missing";
    return false;
  }
  for (const cv::FileNode &node : nodes) {
    PackageSample sample;
    sample.review_item = ReadString(node, "review_item");
    sample.split = ReadString(node, "split");
    sample.source_geometry_type = ReadString(node, "geometry_type");
    sample.canonical_geometry_type = CanonicalGeometryType(
        ontology, sample.source_geometry_type, sample.alias_used);
    sample.image_path = ResolveReference(manifest_path,
                                         ReadString(node, "image"));
    sample.label_path = ResolveReference(manifest_path,
                                         ReadString(node, "label"));
    const std::string target_mask = ReadString(node, "target_mask");
    if (!target_mask.empty())
      sample.target_mask_path = ResolveReference(manifest_path, target_mask);
    if (sample.review_item.empty() || sample.split.empty() ||
        sample.canonical_geometry_type.empty() ||
        !IsRegularFile(sample.image_path) || !IsRegularFile(sample.label_path)) {
      reason = "package sample has missing identity, class, image or label";
      return false;
    }
    const std::string key = PathKey(sample.image_path);
    if (key.empty() || samples.count(key) != 0) {
      reason = "package contains a duplicate normalized image identity";
      return false;
    }
    samples[key] = std::move(sample);
  }
  reason = "package samples and ordered classes validated";
  return true;
}

inline bool LoadDetections(const fs::path &path, int class_count,
                           std::vector<Detection> &detections,
                           std::string &reason) {
  if (!IsRegularFile(path)) {
    reason = "candidate list is missing: " + path.string();
    return false;
  }
  cv::FileStorage storage(path.string(), cv::FileStorage::READ);
  const cv::FileNode nodes = storage["detections"];
  if (!storage.isOpened() || !nodes.isSeq()) {
    reason = "candidate list has no detections sequence: " + path.string();
    return false;
  }
  for (const cv::FileNode &node : nodes) {
    Detection detection;
    node["x1"] >> detection.x1;
    node["y1"] >> detection.y1;
    node["x2"] >> detection.x2;
    node["y2"] >> detection.y2;
    node["confidence"] >> detection.confidence;
    node["class_id"] >> detection.class_id;
    if (detection.class_id < 0 || detection.class_id >= class_count ||
        !std::isfinite(detection.confidence) || detection.confidence < 0.0 ||
        detection.confidence > 1.0 || detection.x2 < detection.x1 ||
        detection.y2 < detection.y1) {
      reason = "candidate list contains an invalid detection: " +
               path.string();
      return false;
    }
    detections.push_back(detection);
  }
  return true;
}

inline bool LoadGroundTruth(const PackageSample &sample,
                            int expected_instances, int class_count,
                            EvidenceCase &item, std::string &reason) {
  const cv::Mat image = cv::imread(sample.image_path.string(),
                                   cv::IMREAD_GRAYSCALE);
  if (image.empty()) {
    reason = "image cannot be decoded: " + sample.image_path.string();
    return false;
  }
  std::ifstream label(sample.label_path);
  if (!label.is_open()) {
    reason = "label cannot be opened: " + sample.label_path.string();
    return false;
  }
  int count = 0;
  int target_class = -1;
  double cx = 0.0, cy = 0.0, width = 0.0, height = 0.0;
  while (label >> target_class >> cx >> cy >> width >> height) {
    ++count;
    if (count == 1) {
      item.target_class = target_class;
      item.target_x1 = (cx - width * 0.5) * image.cols;
      item.target_y1 = (cy - height * 0.5) * image.rows;
      item.target_x2 = (cx + width * 0.5) * image.cols;
      item.target_y2 = (cy + height * 0.5) * image.rows;
    }
  }
  if (count != expected_instances || item.target_class < 0 ||
      item.target_class >= class_count || item.target_x2 <= item.target_x1 ||
      item.target_y2 <= item.target_y1) {
    reason = "label instance count, class id or ROI is invalid: " +
             sample.label_path.string();
    return false;
  }
  return true;
}

inline bool LoadEvidenceReport(
    const fs::path &report_path, const std::string &expected_split,
    const Ontology &ontology, const Policy &policy,
    const std::unordered_map<std::string, PackageSample> &samples,
    std::vector<EvidenceCase> &cases, int &legacy_case_count,
    std::string &reason) {
  cv::FileStorage storage(report_path.string(), cv::FileStorage::READ);
  if (!storage.isOpened()) {
    reason = "paired inference report cannot be opened: " +
             report_path.string();
    return false;
  }
  const cv::FileNode root = storage.root();
  const std::string schema = ReadString(root, "schema");
  const std::string split = ReadString(root, "evaluation_split");
  const cv::FileNode nodes = root["cases"];
  if (schema != "cxvision.yolov8n_cpp_paired_inference.v1" ||
      split != expected_split || !nodes.isSeq() || nodes.empty()) {
    reason = "paired inference schema, split or cases is invalid";
    return false;
  }
  std::map<std::string, int> report_identities;
  legacy_case_count = 0;
  for (const cv::FileNode &node : nodes) {
    const std::string input_reference = ReadString(node, "input_image");
    const fs::path image_path = ResolveReference(report_path, input_reference);
    const std::string key = PathKey(image_path);
    const auto sample_it = samples.find(key);
    if (sample_it == samples.end()) {
      reason = "report image is not owned by package manifest: " +
               image_path.string();
      return false;
    }
    const PackageSample &sample = sample_it->second;
    if (sample.split != expected_split || report_identities.count(key) != 0) {
      reason = "report split mismatch or duplicate image identity";
      return false;
    }
    report_identities[key] = 1;
    EvidenceCase item;
    item.review_item = ReadString(node, "review_item");
    item.split = split;
    item.source_geometry_type = ReadString(node, "geometry_type");
    bool report_alias_used = false;
    item.canonical_geometry_type = CanonicalGeometryType(
        ontology, item.source_geometry_type, report_alias_used);
    item.signal_family = ReadString(node, "signal_family");
    item.image_path = sample.image_path;
    item.label_path = sample.label_path;
    item.alias_used = sample.alias_used || report_alias_used;
    if (item.review_item.empty() ||
        item.review_item != sample.review_item ||
        item.canonical_geometry_type.empty() ||
        item.canonical_geometry_type != sample.canonical_geometry_type) {
      reason = "report identity or geometry type does not match package sample";
      return false;
    }
    if (!LoadGroundTruth(sample, policy.expected_instances_per_roi,
                         policy.required_class_count, item, reason))
      return false;
    if (ClassIdForName(ontology, item.canonical_geometry_type) !=
        item.target_class) {
      reason = "label class id does not match frozen ontology name";
      return false;
    }
    const fs::path parent_path = ResolveReference(
        report_path, ReadString(node, "base_candidates"));
    const fs::path candidate_path = ResolveReference(
        report_path, ReadString(node, "incremental_candidates"));
    if (!LoadDetections(parent_path, policy.required_class_count,
                        item.parent, reason) ||
        !LoadDetections(candidate_path, policy.required_class_count,
                        item.candidate, reason))
      return false;
    legacy_case_count += item.alias_used ? 1 : 0;
    cases.push_back(std::move(item));
  }
  reason = "paired inference cases and detection evidence validated";
  return true;
}

inline double IoU(const Detection &detection, const EvidenceCase &item) {
  const double ix1 = std::max(detection.x1, item.target_x1);
  const double iy1 = std::max(detection.y1, item.target_y1);
  const double ix2 = std::min(detection.x2, item.target_x2);
  const double iy2 = std::min(detection.y2, item.target_y2);
  const double intersection =
      std::max(0.0, ix2 - ix1) * std::max(0.0, iy2 - iy1);
  const double detection_area =
      std::max(0.0, detection.x2 - detection.x1) *
      std::max(0.0, detection.y2 - detection.y1);
  const double target_area =
      std::max(0.0, item.target_x2 - item.target_x1) *
      std::max(0.0, item.target_y2 - item.target_y1);
  const double union_area = detection_area + target_area - intersection;
  return union_area > 0.0 ? intersection / union_area : 0.0;
}

inline double SafeDivide(double numerator, double denominator) {
  return denominator > 0.0 ? numerator / denominator : 0.0;
}

inline Metrics Evaluate(const std::vector<EvidenceCase> &cases,
                        bool candidate_role, double confidence_threshold,
                        int topk, double match_iou_threshold,
                        int class_count, bool collect_details) {
  Metrics metrics;
  metrics.case_count = static_cast<int>(cases.size());
  metrics.per_class.resize(static_cast<std::size_t>(class_count));
  // The final column is NO_DETECTION. Rows are ground-truth classes.
  metrics.confusion.assign(
      static_cast<std::size_t>(class_count),
      std::vector<int>(static_cast<std::size_t>(class_count + 1), 0));
  double matched_confidence_sum = 0.0;
  for (const EvidenceCase &item : cases) {
    const std::vector<Detection> &raw =
        candidate_role ? item.candidate : item.parent;
    metrics.raw_detection_count += static_cast<int>(raw.size());
    std::vector<Detection> selected;
    for (const Detection &detection : raw) {
      if (detection.confidence >= confidence_threshold)
        selected.push_back(detection);
    }
    std::stable_sort(selected.begin(), selected.end(),
                     [](const Detection &left, const Detection &right) {
                       return left.confidence > right.confidence;
                     });
    if (static_cast<int>(selected.size()) > topk)
      selected.resize(static_cast<std::size_t>(topk));
    metrics.selected_detection_count += static_cast<int>(selected.size());
    PerClassMetrics &target_class =
        metrics.per_class[static_cast<std::size_t>(item.target_class)];
    ++target_class.ground_truth;

    int matched_index = -1;
    double matched_iou = 0.0;
    for (std::size_t index = 0; index < selected.size(); ++index) {
      const double iou = IoU(selected[index], item);
      if (selected[index].class_id == item.target_class &&
          iou >= match_iou_threshold) {
        matched_index = static_cast<int>(index);
        matched_iou = iou;
        break;
      }
    }
    if (matched_index >= 0) {
      ++metrics.true_positive;
      ++target_class.true_positive;
      matched_confidence_sum +=
          selected[static_cast<std::size_t>(matched_index)].confidence;
    } else {
      ++metrics.false_negative;
      ++target_class.false_negative;
    }

    bool case_has_false_alarm = false;
    for (std::size_t index = 0; index < selected.size(); ++index) {
      if (static_cast<int>(index) == matched_index)
        continue;
      ++metrics.false_positive;
      ++metrics.per_class[static_cast<std::size_t>(selected[index].class_id)]
            .false_positive;
      case_has_false_alarm = true;
    }
    metrics.false_alarm_cases += case_has_false_alarm ? 1 : 0;

    FailureObservation observation;
    observation.review_item = item.review_item;
    observation.split = item.split;
    observation.geometry_type = item.canonical_geometry_type;
    if (selected.empty()) {
      ++metrics.no_detection_count;
      ++metrics.confusion[static_cast<std::size_t>(item.target_class)]
                         [static_cast<std::size_t>(class_count)];
      observation.failure_class = "NO_DETECTION";
    } else {
      const Detection &first = selected.front();
      const double first_iou = IoU(first, item);
      ++metrics.confusion[static_cast<std::size_t>(item.target_class)]
                         [static_cast<std::size_t>(first.class_id)];
      observation.predicted_class = first.class_id;
      observation.confidence = first.confidence;
      observation.iou = first_iou;
      if (first.class_id != item.target_class) {
        ++metrics.wrong_class_count;
        observation.failure_class = "WRONG_CLASS";
      } else if (first_iou < match_iou_threshold) {
        ++metrics.localization_failure_count;
        observation.failure_class = "LOCALIZATION_IOU_LOW";
      }
    }
    if (collect_details && !observation.failure_class.empty())
      metrics.failures.push_back(std::move(observation));
    (void)matched_iou;
  }
  metrics.precision = SafeDivide(
      metrics.true_positive,
      static_cast<double>(metrics.true_positive + metrics.false_positive));
  metrics.recall = SafeDivide(
      metrics.true_positive,
      static_cast<double>(metrics.true_positive + metrics.false_negative));
  metrics.f1 = SafeDivide(2.0 * metrics.precision * metrics.recall,
                          metrics.precision + metrics.recall);
  metrics.miss_rate = 1.0 - metrics.recall;
  metrics.false_alarm_case_rate =
      SafeDivide(metrics.false_alarm_cases, metrics.case_count);
  metrics.mean_match_confidence =
      SafeDivide(matched_confidence_sum, metrics.true_positive);
  metrics.raw_detections_per_case =
      SafeDivide(metrics.raw_detection_count, metrics.case_count);
  return metrics;
}

inline bool BetterSweepRow(const SweepRow &candidate, const SweepRow &best) {
  constexpr double epsilon = 1e-12;
  if (candidate.candidate.f1 > best.candidate.f1 + epsilon)
    return true;
  if (std::abs(candidate.candidate.f1 - best.candidate.f1) > epsilon)
    return false;
  if (candidate.candidate.precision > best.candidate.precision + epsilon)
    return true;
  if (std::abs(candidate.candidate.precision - best.candidate.precision) >
      epsilon)
    return false;
  if (candidate.candidate.recall > best.candidate.recall + epsilon)
    return true;
  if (std::abs(candidate.candidate.recall - best.candidate.recall) > epsilon)
    return false;
  if (candidate.topk != best.topk)
    return candidate.topk < best.topk;
  return candidate.confidence_threshold > best.confidence_threshold;
}

inline std::string MetricJson(const Metrics &metrics) {
  std::ostringstream out;
  out << std::setprecision(9)
      << "{\"case_count\":" << metrics.case_count
      << ",\"true_positive\":" << metrics.true_positive
      << ",\"false_positive\":" << metrics.false_positive
      << ",\"false_negative\":" << metrics.false_negative
      << ",\"precision\":" << metrics.precision
      << ",\"recall\":" << metrics.recall << ",\"f1\":" << metrics.f1
      << ",\"miss_rate\":" << metrics.miss_rate
      << ",\"false_alarm_case_rate\":"
      << metrics.false_alarm_case_rate
      << ",\"mean_match_confidence\":"
      << metrics.mean_match_confidence
      << ",\"raw_detection_count\":" << metrics.raw_detection_count
      << ",\"selected_detection_count\":"
      << metrics.selected_detection_count
      << ",\"raw_detections_per_case\":"
      << metrics.raw_detections_per_case
      << ",\"no_detection_count\":" << metrics.no_detection_count
      << ",\"wrong_class_count\":" << metrics.wrong_class_count
      << ",\"localization_failure_count\":"
      << metrics.localization_failure_count << "}";
  return out.str();
}

inline std::string PerClassJson(const Metrics &metrics,
                                const Ontology &ontology) {
  std::ostringstream out;
  out << "[";
  for (std::size_t index = 0; index < ontology.classes.size(); ++index) {
    if (index > 0)
      out << ",";
    const PerClassMetrics &item = metrics.per_class[index];
    const double precision = SafeDivide(
        item.true_positive,
        static_cast<double>(item.true_positive + item.false_positive));
    const double recall = SafeDivide(
        item.true_positive,
        static_cast<double>(item.true_positive + item.false_negative));
    const double f1 = SafeDivide(2.0 * precision * recall, precision + recall);
    out << std::setprecision(9) << "{\"id\":" << ontology.classes[index].id
        << ",\"name\":\"" << JsonEscape(ontology.classes[index].name)
        << "\",\"ground_truth\":" << item.ground_truth
        << ",\"true_positive\":" << item.true_positive
        << ",\"false_positive\":" << item.false_positive
        << ",\"false_negative\":" << item.false_negative
        << ",\"precision\":" << precision << ",\"recall\":" << recall
        << ",\"f1\":" << f1 << "}";
  }
  out << "]";
  return out.str();
}

inline std::string ConfusionJson(const Metrics &metrics,
                                 const Ontology &ontology) {
  std::ostringstream out;
  out << "{\"predicted_columns\":[";
  for (std::size_t index = 0; index < ontology.classes.size(); ++index) {
    if (index > 0)
      out << ",";
    out << "\"" << JsonEscape(ontology.classes[index].name) << "\"";
  }
  out << ",\"NO_DETECTION\"],\"rows\":[";
  for (std::size_t row = 0; row < ontology.classes.size(); ++row) {
    if (row > 0)
      out << ",";
    out << "{\"ground_truth_id\":" << ontology.classes[row].id
        << ",\"ground_truth\":\""
        << JsonEscape(ontology.classes[row].name) << "\",\"counts\":[";
    for (std::size_t column = 0; column < metrics.confusion[row].size();
         ++column) {
      if (column > 0)
        out << ",";
      out << metrics.confusion[row][column];
    }
    out << "]}";
  }
  out << "]}";
  return out.str();
}

inline std::string FailureJson(const Metrics &metrics) {
  std::ostringstream out;
  out << "[";
  for (std::size_t index = 0; index < metrics.failures.size(); ++index) {
    if (index > 0)
      out << ",";
    const FailureObservation &item = metrics.failures[index];
    out << std::setprecision(9) << "{\"review_item\":\""
        << JsonEscape(item.review_item) << "\",\"split\":\""
        << JsonEscape(item.split) << "\",\"geometry_type\":\""
        << JsonEscape(item.geometry_type) << "\",\"failure_class\":\""
        << JsonEscape(item.failure_class) << "\",\"predicted_class_id\":"
        << item.predicted_class << ",\"confidence\":" << item.confidence
        << ",\"iou\":" << item.iou << "}";
  }
  out << "]";
  return out.str();
}

inline bool WriteJson(const fs::path &directory, const std::string &name,
                      const std::string &body, std::string &reason) {
  return WriteText(directory / name, body + "\n", reason);
}

inline bool OutputDirectoryReady(const fs::path &directory,
                                 std::string &reason) {
  if (directory.empty()) {
    reason = "--out is required";
    return false;
  }
  std::error_code ec;
  if (fs::exists(directory, ec)) {
    if (ec || !fs::is_directory(directory, ec) || ec) {
      reason = "output path is not a directory";
      return false;
    }
    const fs::directory_iterator end;
    fs::directory_iterator iterator(directory, ec);
    if (ec) {
      reason = "cannot inspect output directory";
      return false;
    }
    if (iterator != end) {
      reason = "output directory must be new or empty to preserve evidence";
      return false;
    }
    return true;
  }
  fs::create_directories(directory, ec);
  if (ec) {
    reason = "cannot create output directory: " + ec.message();
    return false;
  }
  return true;
}

// This is deliberately a C++ runtime assembly step, not a scripting hook.
// It materializes only the package-owned training split and copies frozen
// evaluation assets into distinct read-only split directories.  It does not
// fabricate hard negatives when the package lacks a verifiable target mask.
inline bool AssembleControlledDataStrategy(
    const fs::path &output_dir,
    const std::unordered_map<std::string, PackageSample> &samples,
    const Policy &policy, const bool hard_negative_required,
    const bool confusion_context_required,
    const bool missed_positive_variant_required, std::string &reason) {
  const fs::path dataset_dir = output_dir / "controlled_data_strategy";
  std::error_code ec;
  fs::create_directories(dataset_dir / "images" / "train", ec);
  fs::create_directories(dataset_dir / "labels" / "train", ec);
  fs::create_directories(dataset_dir / "images" / "val", ec);
  fs::create_directories(dataset_dir / "labels" / "val", ec);
  fs::create_directories(dataset_dir / "images" / "holdout", ec);
  fs::create_directories(dataset_dir / "labels" / "holdout", ec);
  if (ec) {
    reason = "cannot create controlled data strategy directories: " + ec.message();
    return false;
  }

  std::vector<const PackageSample *> ordered;
  ordered.reserve(samples.size());
  for (const auto &entry : samples)
    ordered.push_back(&entry.second);
  std::sort(ordered.begin(), ordered.end(),
            [](const PackageSample *left, const PackageSample *right) {
              return left->image_path.generic_string() < right->image_path.generic_string();
            });

  int train_count = 0;
  int validation_count = 0;
  int holdout_count = 0;
  int hard_negative_count = 0;
  bool all_train_masks_available = true;
  std::ostringstream rows;
  bool first = true;
  for (std::size_t index = 0; index < ordered.size(); ++index) {
    const PackageSample &sample = *ordered[index];
    std::string destination_split;
    if (sample.split == "train") {
      destination_split = "train";
      ++train_count;
    } else if (sample.split == policy.selection_split) {
      // Torch's detector lifecycle consumes `val`; the receipt continues to
      // use the semantic term validation so the evaluation contract is clear.
      destination_split = "val";
      ++validation_count;
    } else if (sample.split == policy.confirmation_split) {
      destination_split = "holdout";
      ++holdout_count;
    } else {
      continue;
    }
    const std::string stem = std::to_string(index) + "_" + sample.image_path.filename().string();
    const fs::path image_target = dataset_dir / "images" / destination_split / stem;
    const fs::path label_target = dataset_dir / "labels" / destination_split /
                                  (std::to_string(index) + "_" + sample.label_path.filename().string());
    fs::copy_file(sample.image_path, image_target, fs::copy_options::none, ec);
    if (ec) {
      reason = "cannot copy controlled image asset: " + ec.message();
      return false;
    }
    fs::copy_file(sample.label_path, label_target, fs::copy_options::none, ec);
    if (ec) {
      reason = "cannot copy controlled label asset: " + ec.message();
      return false;
    }
    if (destination_split == "train" && hard_negative_required) {
      if (!IsRegularFile(sample.target_mask_path)) {
        all_train_masks_available = false;
      } else {
        const cv::Mat image = cv::imread(image_target.string(), cv::IMREAD_COLOR);
        const cv::Mat mask = cv::imread(sample.target_mask_path.string(), cv::IMREAD_GRAYSCALE);
        if (image.empty() || mask.empty() || image.size() != mask.size()) {
          all_train_masks_available = false;
        } else {
          cv::Mat binary_mask;
          cv::threshold(mask, binary_mask, 0, 255, cv::THRESH_BINARY);
          cv::Mat erased;
          cv::inpaint(image, binary_mask, erased, 3.0, cv::INPAINT_TELEA);
          const fs::path negative_image = dataset_dir / "images" / "train" /
              (std::to_string(index) + "_target_erased_" + sample.image_path.filename().string());
          const fs::path negative_label = dataset_dir / "labels" / "train" /
              (std::to_string(index) + "_target_erased.txt");
          if (!cv::imwrite(negative_image.string(), erased)) {
            reason = "cannot write C++ generated hard negative";
            return false;
          }
          std::ofstream empty_label(negative_label);
          if (!empty_label.good()) {
            reason = "cannot write C++ generated hard-negative label";
            return false;
          }
          ++hard_negative_count;
        }
      }
    }
    if (!first)
      rows << ",";
    first = false;
    rows << "{\"review_item\":\"" << JsonEscape(sample.review_item)
         << "\",\"split\":\"" << JsonEscape(destination_split)
         << "\",\"geometry_type\":\""
         << JsonEscape(sample.canonical_geometry_type)
         << "\",\"image\":\""
         << JsonEscape(image_target.lexically_relative(dataset_dir).generic_string())
         << "\",\"label\":\""
         << JsonEscape(label_target.lexically_relative(dataset_dir).generic_string())
         << "\",\"source_image_digest\":\"" << FileDigest(sample.image_path)
         << "\",\"source_label_digest\":\"" << FileDigest(sample.label_path)
         << "\"}";
  }
  if (train_count == 0 || validation_count == 0 || holdout_count == 0) {
    reason = "controlled data strategy has an empty required split";
    return false;
  }
  const bool masks_available = !hard_negative_required || all_train_masks_available;
  const bool training_allowed = !hard_negative_required ||
      (all_train_masks_available && hard_negative_count == train_count);
  std::ostringstream dataset_manifest;
  dataset_manifest << "{\"schema\":\"cxvision.torch_controlled_data_strategy_dataset.v1\""
                   << ",\"training_split\":\"train\",\"validation_split\":\"val\""
                   << ",\"holdout_split\":\"holdout\""
                   << ",\"train_positive_count\":" << train_count
                   << ",\"hard_negative_count\":" << hard_negative_count
                   << ",\"validation_count\":" << validation_count
                   << ",\"holdout_count\":" << holdout_count
                   << ",\"training_allowed\":" << (training_allowed ? "true" : "false")
                   << ",\"promotion_allowed\":false}";
  if (!WriteJson(dataset_dir, "package_manifest.json", dataset_manifest.str(), reason))
    return false;
  std::ostringstream receipt;
  receipt << "{\"schema\":\"cxvision.torch_controlled_data_strategy_receipt.v1\""
          << ",\"executor\":\"CXX_RUNTIME\""
          << ",\"status\":\""
          << (training_allowed ? "DATA_ASSEMBLY_COMPLETE" :
              "DATA_ASSEMBLY_PARTIAL_MASK_PROVENANCE_REQUIRED") << "\""
          << ",\"training_split\":\"train\""
          << ",\"immutable_evaluation_splits\":[\"val\",\"holdout\"]"
          << ",\"evaluation_to_training_copy_forbidden\":true"
          << ",\"external_script_execution_allowed\":false"
          << ",\"train_positive_count\":" << train_count
          << ",\"hard_negative_count\":" << hard_negative_count
          << ",\"validation_count\":" << validation_count
          << ",\"holdout_count\":" << holdout_count
          << ",\"hard_negative_mining_required\":"
          << (hard_negative_required ? "true" : "false")
          << ",\"confusion_context_required\":"
          << (confusion_context_required ? "true" : "false")
          << ",\"missed_positive_variant_required\":"
          << (missed_positive_variant_required ? "true" : "false")
          << ",\"target_mask_provenance_available\":"
          << (masks_available ? "true" : "false")
          << ",\"hard_negative_generation_status\":\""
          << (!hard_negative_required ? "NOT_REQUIRED" :
              training_allowed ? "CXX_INPAINT_TARGET_ERASURE_COMPLETE" :
              "BLOCKED_TARGET_MASK_PROVENANCE_REQUIRED")
          << "\",\"training_allowed\":" << (training_allowed ? "true" : "false")
          << ",\"promotion_allowed\":false"
          << ",\"samples\":[" << rows.str() << "]}";
  return WriteJson(output_dir, "data_strategy_assembly.json", receipt.str(), reason);
}

inline std::string RequestedConfigJson(const fs::path &ontology_path,
                                       const fs::path &policy_path,
                                       const fs::path &package_path,
                                       const fs::path &validation_path,
                                       const fs::path &holdout_path,
                                       const Ontology &ontology,
                                       const Policy &policy) {
  std::ostringstream out;
  out << std::setprecision(9)
      << "{\"schema\":\"cxvision.torch_geometry_auto_tune_request.v1\""
      << ",\"ontology_ref\":\"" << JsonEscape(ontology_path.string())
      << "\",\"ontology_schema\":\"" << JsonEscape(ontology.schema)
      << "\",\"ontology_version\":\"" << JsonEscape(ontology.version)
      << "\",\"policy_ref\":\"" << JsonEscape(policy_path.string())
      << "\",\"policy_id\":\"" << JsonEscape(policy.policy_id)
      << "\",\"package_manifest_ref\":\""
      << JsonEscape(package_path.string()) << "\",\"validation_report_ref\":\""
      << JsonEscape(validation_path.string()) << "\",\"holdout_report_ref\":\""
      << JsonEscape(holdout_path.string()) << "\",\"selection_split\":\""
      << JsonEscape(policy.selection_split) << "\",\"confirmation_split\":\""
      << JsonEscape(policy.confirmation_split)
      << "\",\"holdout_used_for_selection\":false"
      << ",\"match_iou_threshold\":" << policy.match_iou_threshold
      << ",\"confidence_threshold_candidates\":[";
  for (std::size_t index = 0; index < policy.confidence_candidates.size();
       ++index) {
    if (index > 0)
      out << ",";
    out << policy.confidence_candidates[index];
  }
  out << "],\"topk_candidates\":[";
  for (std::size_t index = 0; index < policy.topk_candidates.size(); ++index) {
    if (index > 0)
      out << ",";
    out << policy.topk_candidates[index];
  }
  out << "]}";
  return out.str();
}

inline int RunGeometryIncrementalAutoTuneCli(int argc, char **argv) {
  const fs::path package_path = ArgValue(argc, argv, "--package-manifest");
  const fs::path validation_path = ArgValue(argc, argv, "--validation-report");
  const fs::path holdout_path = ArgValue(argc, argv, "--holdout-report");
  const fs::path ontology_path = ArgValue(argc, argv, "--ontology");
  const fs::path policy_path = ArgValue(argc, argv, "--auto-tune-policy");
  const fs::path output_dir = ArgValue(argc, argv, "--out");
  std::string reason;
  const std::pair<const char *, fs::path> required[] = {
      {"--package-manifest", package_path},
      {"--validation-report", validation_path},
      {"--holdout-report", holdout_path},
      {"--ontology", ontology_path},
      {"--auto-tune-policy", policy_path}};
  for (const auto &item : required) {
    if (!IsRegularFile(item.second)) {
      std::cout << "conclusion=AUTO_TUNE_PREFLIGHT_FAIL\nreason=" << item.first
                << " must reference a regular file\n";
      return 2;
    }
  }
  if (!OutputDirectoryReady(output_dir, reason)) {
    std::cout << "conclusion=AUTO_TUNE_PREFLIGHT_FAIL\nreason=" << reason
              << "\n";
    return 2;
  }

  Ontology ontology;
  Policy policy;
  if (!LoadOntology(ontology_path, ontology, reason) ||
      !LoadPolicy(policy_path, policy, reason) ||
      ontology.schema != policy.required_ontology_schema ||
      static_cast<int>(ontology.classes.size()) != policy.required_class_count) {
    if (reason.empty())
      reason = "ontology does not satisfy the auto-tune policy";
    std::cout << "conclusion=AUTO_TUNE_PREFLIGHT_FAIL\nreason=" << reason
              << "\n";
    return 1;
  }

  std::unordered_map<std::string, PackageSample> samples;
  int legacy_package_class_count = 0;
  if (!LoadPackageSamples(package_path, ontology, policy, samples,
                          legacy_package_class_count, reason)) {
    std::cout << "conclusion=AUTO_TUNE_PREFLIGHT_FAIL\nreason=" << reason
              << "\n";
    return 1;
  }
  std::vector<EvidenceCase> validation_cases;
  std::vector<EvidenceCase> holdout_cases;
  int validation_alias_case_count = 0;
  int holdout_alias_case_count = 0;
  if (!LoadEvidenceReport(validation_path, policy.selection_split, ontology,
                          policy, samples, validation_cases,
                          validation_alias_case_count, reason) ||
      !LoadEvidenceReport(holdout_path, policy.confirmation_split, ontology,
                          policy, samples, holdout_cases,
                          holdout_alias_case_count, reason)) {
    std::cout << "conclusion=AUTO_TUNE_PREFLIGHT_FAIL\nreason=" << reason
              << "\n";
    return 1;
  }
  std::vector<int> validation_class_counts(ontology.classes.size(), 0);
  std::vector<int> holdout_class_counts(ontology.classes.size(), 0);
  for (const EvidenceCase &item : validation_cases)
    ++validation_class_counts[static_cast<std::size_t>(item.target_class)];
  for (const EvidenceCase &item : holdout_cases)
    ++holdout_class_counts[static_cast<std::size_t>(item.target_class)];
  for (std::size_t index = 0; index < ontology.classes.size(); ++index) {
    if (validation_class_counts[index] == 0 || holdout_class_counts[index] == 0) {
      std::cout << "conclusion=AUTO_TUNE_PREFLIGHT_FAIL\nreason=class coverage is incomplete for "
                << ontology.classes[index].name << "\n";
      return 1;
    }
  }

  std::vector<SweepRow> sweep;
  SweepRow best;
  bool best_available = false;
  for (const double confidence : policy.confidence_candidates) {
    for (const int topk : policy.topk_candidates) {
      SweepRow row;
      row.confidence_threshold = confidence;
      row.topk = topk;
      row.parent = Evaluate(validation_cases, false, confidence, topk,
                            policy.match_iou_threshold,
                            policy.required_class_count, false);
      row.candidate = Evaluate(validation_cases, true, confidence, topk,
                               policy.match_iou_threshold,
                               policy.required_class_count, false);
      if (!best_available || BetterSweepRow(row, best)) {
        best = row;
        best_available = true;
      }
      sweep.push_back(std::move(row));
    }
  }
  if (!best_available) {
    std::cout << "conclusion=AUTO_TUNE_PREFLIGHT_FAIL\nreason=no valid validation search row\n";
    return 1;
  }
  const Metrics selected_validation_parent = Evaluate(
      validation_cases, false, best.confidence_threshold, best.topk,
      policy.match_iou_threshold, policy.required_class_count, true);
  const Metrics selected_validation_candidate = Evaluate(
      validation_cases, true, best.confidence_threshold, best.topk,
      policy.match_iou_threshold, policy.required_class_count, true);
  const Metrics source_validation_candidate = Evaluate(
      validation_cases, true, policy.source_confidence_threshold,
      policy.source_topk, policy.match_iou_threshold, policy.required_class_count,
      false);
  const Metrics selected_holdout_parent = Evaluate(
      holdout_cases, false, best.confidence_threshold, best.topk,
      policy.match_iou_threshold, policy.required_class_count, true);
  const Metrics selected_holdout_candidate = Evaluate(
      holdout_cases, true, best.confidence_threshold, best.topk,
      policy.match_iou_threshold, policy.required_class_count, true);
  const Metrics source_holdout_candidate = Evaluate(
      holdout_cases, true, policy.source_confidence_threshold, policy.source_topk,
      policy.match_iou_threshold, policy.required_class_count, false);

  const double validation_gain = selected_validation_candidate.f1 -
                                 selected_validation_parent.f1;
  const double holdout_delta = selected_holdout_candidate.f1 -
                               selected_holdout_parent.f1;
  const bool validation_gate =
      validation_gain >= policy.minimum_validation_f1_gain;
  const bool holdout_regression_gate =
      holdout_delta >= -policy.maximum_holdout_f1_drop_vs_parent;
  const bool holdout_miss_gate = selected_holdout_candidate.miss_rate <=
                                 policy.maximum_holdout_miss_rate;
  const bool holdout_false_alarm_gate =
      selected_holdout_candidate.false_alarm_case_rate <=
      policy.maximum_holdout_false_alarm_case_rate;
  const bool quality_gate = validation_gate && holdout_regression_gate &&
                            holdout_miss_gate && holdout_false_alarm_gate;

  std::ostringstream preflight;
  preflight << "{\"schema\":\"cxvision.torch_geometry_auto_tune_preflight.v1\""
            << ",\"status\":\"AUTO_TUNE_PREFLIGHT_PASS\""
            << ",\"package_sample_count\":" << samples.size()
            << ",\"validation_case_count\":" << validation_cases.size()
            << ",\"holdout_case_count\":" << holdout_cases.size()
            << ",\"class_coverage\":[";
  for (std::size_t index = 0; index < ontology.classes.size(); ++index) {
    if (index > 0)
      preflight << ",";
    preflight << "{\"id\":" << ontology.classes[index].id
              << ",\"name\":\"" << JsonEscape(ontology.classes[index].name)
              << "\",\"validation_count\":" << validation_class_counts[index]
              << ",\"holdout_count\":" << holdout_class_counts[index] << "}";
  }
  preflight << "]}";
  if (!WriteJson(output_dir, "dataset_preflight.json", preflight.str(), reason) ||
      !WriteJson(output_dir, "requested_config.json",
                 RequestedConfigJson(ontology_path, policy_path, package_path,
                                     validation_path, holdout_path, ontology,
                                     policy), reason)) {
    std::cout << "conclusion=AUTO_TUNE_OUTPUT_FAIL\nreason=" << reason << "\n";
    return 1;
  }

  std::ostringstream ontology_audit;
  ontology_audit << "{\"schema\":\"cxvision.torch_geometry_ontology_audit.v1\""
                 << ",\"status\":\"ONTOLOGY_AUDIT_PASS\""
                 << ",\"ontology_schema\":\"" << JsonEscape(ontology.schema)
                 << "\",\"ontology_version\":\"" << JsonEscape(ontology.version)
                 << "\",\"classes\":[";
  for (std::size_t index = 0; index < ontology.classes.size(); ++index) {
    if (index > 0)
      ontology_audit << ",";
    ontology_audit << "{\"id\":" << ontology.classes[index].id
                   << ",\"name\":\""
                   << JsonEscape(ontology.classes[index].name) << "\"}";
  }
  ontology_audit << "],\"legacy_alias_receipt\":{\"package_class_alias_count\":"
                 << legacy_package_class_count
                 << ",\"validation_case_alias_count\":"
                 << validation_alias_case_count
                 << ",\"holdout_case_alias_count\":"
                 << holdout_alias_case_count << ",\"aliases\":[";
  std::size_t alias_index = 0;
  for (const auto &entry : ontology.aliases) {
    if (alias_index++ > 0)
      ontology_audit << ",";
    const LegacyAlias &alias = entry.second;
    ontology_audit << "{\"source\":\"" << JsonEscape(alias.source)
                   << "\",\"target\":\"" << JsonEscape(alias.target)
                   << "\",\"class_id\":" << alias.class_id
                   << ",\"requires_migration_receipt\":"
                   << (alias.requires_receipt ? "true" : "false")
                   << ",\"reason\":\"" << JsonEscape(alias.reason)
                   << "\"}";
  }
  ontology_audit << "]}}";
  if (!WriteJson(output_dir, "ontology_audit.json", ontology_audit.str(),
                 reason)) {
    std::cout << "conclusion=AUTO_TUNE_OUTPUT_FAIL\nreason=" << reason << "\n";
    return 1;
  }

  std::ostringstream sweep_json;
  sweep_json << "{\"schema\":\"cxvision.torch_geometry_threshold_topk_sweep.v1\""
             << ",\"selection_split\":\"" << JsonEscape(policy.selection_split)
             << "\",\"holdout_used_for_selection\":false"
             << ",\"selection_metric\":\"f1\""
             << ",\"match_iou_threshold\":" << policy.match_iou_threshold
             << ",\"rows\":[";
  for (std::size_t index = 0; index < sweep.size(); ++index) {
    if (index > 0)
      sweep_json << ",";
    const SweepRow &row = sweep[index];
    sweep_json << std::setprecision(9) << "{\"confidence_threshold\":"
               << row.confidence_threshold << ",\"topk\":" << row.topk
               << ",\"parent\":" << MetricJson(row.parent)
               << ",\"candidate\":" << MetricJson(row.candidate) << "}";
  }
  sweep_json << "]}";

  std::ostringstream effective;
  effective << std::setprecision(9)
            << "{\"schema\":\"cxvision.torch_geometry_auto_tune_effective.v1\""
            << ",\"status\":\"AUTO_TUNE_EVALUATED\""
            << ",\"selection_split\":\"" << JsonEscape(policy.selection_split)
            << "\",\"confirmation_split\":\""
            << JsonEscape(policy.confirmation_split)
            << "\",\"holdout_used_for_selection\":false"
            << ",\"effective_inference_profile\":{\"confidence_threshold\":"
            << best.confidence_threshold << ",\"topk_per_roi\":" << best.topk
            << ",\"match_iou_threshold\":" << policy.match_iou_threshold
            << ",\"source\":\"AUTO_SELECTED_VALIDATION\"}"
            << ",\"rollback_inference_profile\":{\"confidence_threshold\":"
            << policy.source_confidence_threshold << ",\"topk_per_roi\":"
            << policy.source_topk << ",\"source\":\"SOURCE_PROFILE_POLICY\"}"
            << ",\"validation_parent\":" << MetricJson(selected_validation_parent)
            << ",\"validation_candidate\":"
            << MetricJson(selected_validation_candidate)
            << ",\"validation_candidate_source_profile\":"
            << MetricJson(source_validation_candidate) << "}";

  std::ostringstream classification;
  classification << "{\"schema\":\"cxvision.torch_geometry_classification_observation.v1\""
                 << ",\"selection_profile\":{\"confidence_threshold\":"
                 << best.confidence_threshold << ",\"topk_per_roi\":"
                 << best.topk << "}"
                 << ",\"validation_candidate\":"
                 << MetricJson(selected_validation_candidate)
                 << ",\"holdout_candidate\":"
                 << MetricJson(selected_holdout_candidate)
                 << ",\"validation_per_class\":"
                 << PerClassJson(selected_validation_candidate, ontology)
                 << ",\"holdout_per_class\":"
                 << PerClassJson(selected_holdout_candidate, ontology)
                 << ",\"validation_failures\":"
                 << FailureJson(selected_validation_candidate)
                 << ",\"holdout_failures\":"
                 << FailureJson(selected_holdout_candidate) << "}";

  std::ostringstream holdout;
  holdout << std::setprecision(9)
          << "{\"schema\":\"cxvision.torch_geometry_holdout_confirmation.v1\""
          << ",\"status\":\"LOCKED_HOLDOUT_CONFIRMATION_COMPLETE\""
          << ",\"holdout_used_for_selection\":false"
          << ",\"locked_profile\":{\"confidence_threshold\":"
          << best.confidence_threshold << ",\"topk_per_roi\":" << best.topk
          << ",\"match_iou_threshold\":" << policy.match_iou_threshold
          << "}"
          << ",\"parent\":" << MetricJson(selected_holdout_parent)
          << ",\"candidate\":" << MetricJson(selected_holdout_candidate)
          << ",\"candidate_source_profile\":"
          << MetricJson(source_holdout_candidate)
          << ",\"candidate_f1_delta_vs_parent\":" << holdout_delta
          << ",\"gates\":{\"f1_regression_controlled\":"
          << (holdout_regression_gate ? "true" : "false")
          << ",\"miss_rate_pass\":" << (holdout_miss_gate ? "true" : "false")
          << ",\"false_alarm_pass\":"
          << (holdout_false_alarm_gate ? "true" : "false") << "}}";

  const bool classification_dominant =
      selected_validation_candidate.wrong_class_count >=
      selected_validation_candidate.localization_failure_count;
  const bool duplicate_dominant =
      selected_validation_candidate.raw_detections_per_case >
      static_cast<double>(policy.expected_instances_per_roi);
  int train_source_case_count = 0;
  for (const auto &sample : samples) {
    if (sample.second.split == "train")
      ++train_source_case_count;
  }
  const bool hard_negative_mining_required =
      selected_validation_candidate.false_alarm_case_rate > 0.0;
  const bool confusion_context_required =
      selected_validation_candidate.wrong_class_count > 0;
  const bool missed_positive_variant_required =
      selected_validation_candidate.miss_rate > 0.0;
  if (!AssembleControlledDataStrategy(
          output_dir, samples, policy, hard_negative_mining_required,
          confusion_context_required, missed_positive_variant_required,
          reason)) {
    std::cout << "conclusion=AUTO_TUNE_OUTPUT_FAIL\nreason=" << reason << "\n";
    return 1;
  }
  std::ostringstream recipe;
  recipe << std::setprecision(9)
         << "{\"schema\":\"cxvision.torch_geometry_next_training_recipe.v1\""
         << ",\"status\":\"TRAINING_RECIPE_REQUIRED\""
         << ",\"automatic_actions_complete\":[\"asset_preflight\",\"seven_class_observation\",\"validation_profile_search\",\"locked_holdout_confirmation\",\"evidence_index\"]"
         << ",\"training_actions_require_new_candidate_run\":true"
         << ",\"architecture_auto_mutation_allowed\":false"
         << ",\"classification_priority\":"
         << (classification_dominant ? "true" : "false")
         << ",\"duplicate_high_confidence_priority\":"
         << (duplicate_dominant ? "true" : "false")
         << ",\"data_strategy\":{\"executor\":\"CXX_RUNTIME_REQUIRED\""
         << ",\"training_split_only\":true"
         << ",\"immutable_evaluation_splits\":[\""
         << JsonEscape(policy.selection_split) << "\",\""
         << JsonEscape(policy.confirmation_split) << "\"]"
         << ",\"evaluation_to_training_copy_forbidden\":true"
         << ",\"controlled_train_source_case_count\":"
         << train_source_case_count
         << ",\"hard_negative_mining_required\":"
         << (hard_negative_mining_required ? "true" : "false")
         << ",\"confusion_context_required\":"
         << (confusion_context_required ? "true" : "false")
         << ",\"missed_positive_variant_required\":"
         << (missed_positive_variant_required ? "true" : "false")
         << ",\"external_script_execution_allowed\":false}"
         << ",\"observed_validation_wrong_class_count\":"
         << selected_validation_candidate.wrong_class_count
         << ",\"observed_validation_localization_failure_count\":"
         << selected_validation_candidate.localization_failure_count
         << ",\"observed_validation_raw_detections_per_case\":"
         << selected_validation_candidate.raw_detections_per_case
         << ",\"training_defaults\":{\"minimum_complete_epochs\":"
         << policy.training.minimum_complete_epochs
         << ",\"recommended_max_epochs\":"
         << policy.training.recommended_max_epochs
         << ",\"max_train_batches_per_epoch\":"
         << policy.training.max_train_batches_per_epoch
         << ",\"assignment_scope\":\""
         << JsonEscape(policy.training.assignment_scope)
         << "\",\"use_assignment_quality_targets\":"
         << (policy.training.use_assignment_quality_targets ? "true" : "false")
         << ",\"use_dfl_box_regression\":"
         << (policy.training.use_dfl_box_regression ? "true" : "false")
         << ",\"dfl_reg_max\":" << policy.training.dfl_reg_max
         << ",\"class_balanced_sampling\":"
         << (policy.training.class_balanced_sampling ? "true" : "false")
         << ",\"hard_confusion_oversample_factor\":"
         << policy.training.hard_confusion_oversample_factor
         << ",\"validation_metric\":\""
         << JsonEscape(policy.training.validation_metric)
         << "\",\"validation_interval\":"
         << policy.training.validation_interval
         << ",\"early_stop_patience\":"
         << policy.training.early_stop_patience
         << ",\"minimum_f1_delta\":"
         << policy.training.minimum_f1_delta
         << ",\"restore_best_validation_checkpoint\":"
         << (policy.training.restore_best_validation_checkpoint ? "true" : "false")
         << "},\"required_next_evidence\":[\"training_plan_runtime.json\",\"training_log.jsonl\",\"provider_receipt.json\",\"candidate_model_manifest.json\",\"fixed_validation_report.json\",\"locked_holdout_confirmation.json\",\"rollback_manifest.json\"]}"
         ;

  if (!WriteJson(output_dir, "threshold_topk_sweep.json", sweep_json.str(),
                 reason) ||
      !WriteJson(output_dir, "effective_config.json", effective.str(),
                 reason) ||
      !WriteJson(output_dir, "classification_observation.json",
                 classification.str(), reason) ||
      !WriteJson(output_dir, "confusion_matrix.json",
                 ConfusionJson(selected_validation_candidate, ontology), reason) ||
      !WriteJson(output_dir, "holdout_confirmation.json", holdout.str(),
                 reason) ||
      !WriteJson(output_dir, "next_training_recipe.json", recipe.str(),
                 reason)) {
    std::cout << "conclusion=AUTO_TUNE_OUTPUT_FAIL\nreason=" << reason << "\n";
    return 1;
  }

  std::ostringstream cfg;
  cfg << "# Projection only. JSON effective_config.json is authoritative.\n"
      << "[ontology]\nversion=" << ontology.version << "\nclasses="
      << ontology.classes.size() << "\n\n[auto_tune]\nselection_split="
      << policy.selection_split << "\nconfirmation_split="
      << policy.confirmation_split
      << "\nholdout_used_for_selection=0\nmatch_iou_threshold="
      << policy.match_iou_threshold << "\n\n[postprocess]\nconfidence_threshold="
      << best.confidence_threshold << "\ntopk_per_roi=" << best.topk
      << "\n\n[training_recipe]\nassignment_scope="
      << policy.training.assignment_scope << "\nuse_dfl_box_regression="
      << (policy.training.use_dfl_box_regression ? 1 : 0)
      << "\nclass_balanced_sampling="
      << (policy.training.class_balanced_sampling ? 1 : 0)
      << "\nvalidation_metric=" << policy.training.validation_metric << "\n";
  if (!WriteText(output_dir / "effective_config_projection.cfg", cfg.str(),
                 reason)) {
    std::cout << "conclusion=AUTO_TUNE_OUTPUT_FAIL\nreason=" << reason << "\n";
    return 1;
  }

  std::ostringstream summary;
  summary << std::setprecision(9)
          << "{\"schema\":\"cxvision.torch_geometry_auto_tune_summary.v1\""
          << ",\"status\":\"AUTO_TUNE_EVALUATED\""
          << ",\"candidate_state\":\"CANDIDATE\""
          << ",\"quality_state\":\""
          << (quality_gate ? "HEADLESS_QUALITY_GATE_PASS" : "QUALITY_GATE_FAILED")
          << "\",\"production_admission_state\":\""
          << (quality_gate ? "EVALUATED" : "REJECTED")
          << "\",\"promotion_allowed\":false"
          << ",\"active_allowed\":false"
          << ",\"approval_required_after_handoff\":true"
          << ",\"selection_profile\":{\"confidence_threshold\":"
          << best.confidence_threshold << ",\"topk_per_roi\":" << best.topk
          << ",\"match_iou_threshold\":" << policy.match_iou_threshold
          << "}"
          << ",\"validation\":{\"parent\":"
          << MetricJson(selected_validation_parent)
          << ",\"candidate\":" << MetricJson(selected_validation_candidate)
          << ",\"candidate_f1_delta_vs_parent\":" << validation_gain
          << ",\"candidate_f1_delta_vs_source_profile\":"
          << (selected_validation_candidate.f1 - source_validation_candidate.f1)
          << "}"
          << ",\"holdout\":{\"parent\":" << MetricJson(selected_holdout_parent)
          << ",\"candidate\":" << MetricJson(selected_holdout_candidate)
          << ",\"candidate_f1_delta_vs_parent\":" << holdout_delta
          << ",\"candidate_f1_delta_vs_source_profile\":"
          << (selected_holdout_candidate.f1 - source_holdout_candidate.f1)
          << "}"
          << ",\"quality_gates\":{\"validation_gain_pass\":"
          << (validation_gate ? "true" : "false")
          << ",\"holdout_regression_pass\":"
          << (holdout_regression_gate ? "true" : "false")
          << ",\"holdout_miss_rate_pass\":"
          << (holdout_miss_gate ? "true" : "false")
          << ",\"holdout_false_alarm_case_rate_pass\":"
          << (holdout_false_alarm_gate ? "true" : "false") << "}"
          << ",\"stages\":[";
  for (std::size_t index = 0; index < policy.stages.size(); ++index) {
    if (index > 0)
      summary << ",";
    const StageDefinition &stage = policy.stages[index];
    const char *stage_status = stage.order == 3 || stage.order == 4 ||
                                       stage.order == 5 || stage.order == 6
                                   ? "RECIPE_GENERATED"
                                   : "COMPLETE";
    summary << "{\"order\":" << stage.order << ",\"id\":\""
            << JsonEscape(stage.id) << "\",\"action\":\""
            << JsonEscape(stage.action) << "\",\"status\":\""
            << stage_status << "\"}";
  }
  summary << "]}";
  if (!WriteJson(output_dir, "auto_tune_summary.json", summary.str(), reason)) {
    std::cout << "conclusion=AUTO_TUNE_OUTPUT_FAIL\nreason=" << reason << "\n";
    return 1;
  }

  const std::vector<std::string> output_names = {
      "dataset_preflight.json", "requested_config.json", "ontology_audit.json",
      "threshold_topk_sweep.json", "effective_config.json",
      "classification_observation.json", "confusion_matrix.json",
      "holdout_confirmation.json", "next_training_recipe.json",
      "data_strategy_assembly.json", "effective_config_projection.cfg",
      "auto_tune_summary.json"};
  std::ostringstream index;
  index << "{\"schema\":\"cxvision.torch_geometry_auto_tune_evidence_index.v1\""
        << ",\"status\":\"EVIDENCE_INDEX_COMPLETE\""
        << ",\"inputs\":[";
  const std::vector<fs::path> inputs = {ontology_path, policy_path, package_path,
                                        validation_path, holdout_path};
  for (std::size_t item_index = 0; item_index < inputs.size(); ++item_index) {
    if (item_index > 0)
      index << ",";
    index << "{\"path\":\"" << JsonEscape(inputs[item_index].string())
          << "\",\"digest\":\"" << FileDigest(inputs[item_index]) << "\"}";
  }
  index << "],\"outputs\":[";
  for (std::size_t item_index = 0; item_index < output_names.size(); ++item_index) {
    if (item_index > 0)
      index << ",";
    const fs::path path = output_dir / output_names[item_index];
    index << "{\"path\":\"" << JsonEscape(path.string())
          << "\",\"digest\":\"" << FileDigest(path) << "\"}";
  }
  index << "]}";
  if (!WriteJson(output_dir, "evidence_index.json", index.str(), reason)) {
    std::cout << "conclusion=AUTO_TUNE_OUTPUT_FAIL\nreason=" << reason << "\n";
    return 1;
  }
  std::cout << "conclusion=AUTO_TUNE_EVALUATED\n"
            << "quality_state="
            << (quality_gate ? "HEADLESS_QUALITY_GATE_PASS" : "QUALITY_GATE_FAILED")
            << "\nproduction_admission_state="
            << (quality_gate ? "EVALUATED" : "REJECTED")
            << "\nselected_confidence_threshold=" << best.confidence_threshold
            << "\nselected_topk_per_roi=" << best.topk
            << "\nvalidation_candidate_f1=" << selected_validation_candidate.f1
            << "\nholdout_candidate_f1=" << selected_holdout_candidate.f1
            << "\nevidence_summary="
            << (output_dir / "auto_tune_summary.json").string() << "\n";
  return 0;
}

inline int RunControlledDataStrategyAssemblyCli(int argc, char **argv) {
  const fs::path package_path = ArgValue(argc, argv, "--package-manifest");
  const fs::path ontology_path = ArgValue(argc, argv, "--ontology");
  const fs::path policy_path = ArgValue(argc, argv, "--auto-tune-policy");
  const fs::path output_dir = ArgValue(argc, argv, "--out");
  const std::string hard_negative_text = ArgValue(argc, argv, "--hard-negative-required");
  if (!IsRegularFile(package_path) || !IsRegularFile(ontology_path) ||
      !IsRegularFile(policy_path) ||
      (hard_negative_text != "0" && hard_negative_text != "1")) {
    std::cout << "conclusion=DATA_STRATEGY_PREFLIGHT_FAIL\n";
    return 2;
  }
  std::string reason;
  if (!OutputDirectoryReady(output_dir, reason)) {
    std::cout << "conclusion=DATA_STRATEGY_PREFLIGHT_FAIL\nreason=" << reason << "\n";
    return 2;
  }
  Ontology ontology;
  Policy policy;
  std::unordered_map<std::string, PackageSample> samples;
  int alias_count = 0;
  if (!LoadOntology(ontology_path, ontology, reason) ||
      !LoadPolicy(policy_path, policy, reason) ||
      !LoadPackageSamples(package_path, ontology, policy, samples, alias_count,
                          reason) ||
      !AssembleControlledDataStrategy(output_dir, samples, policy,
                                      hard_negative_text == "1", false, false,
                                      reason)) {
    std::cout << "conclusion=DATA_STRATEGY_ASSEMBLY_FAIL\nreason=" << reason << "\n";
    return 1;
  }
  std::cout << "conclusion=DATA_STRATEGY_ASSEMBLY_COMPLETE\nreceipt="
            << (output_dir / "data_strategy_assembly.json").string() << "\n";
  return 0;
}

// Converts a reviewed, editable training-template asset into one immutable
// execution plan for the controlled dataset.  This deliberately owns no
// parameter defaults: every tunable value originates in the supplied template
// or in the policy/dataset evidence, so the same values can be projected in UI
// and reproduced by the headless Torch lifecycle.
inline int RunControlledL1TrainingPlanCli(int argc, char **argv) {
  const fs::path dataset_root = ArgValue(argc, argv, "--dataset-root");
  const fs::path template_path = ArgValue(argc, argv, "--training-template");
  const fs::path parent_checkpoint = ArgValue(argc, argv, "--parent-checkpoint");
  const fs::path ontology_path = ArgValue(argc, argv, "--ontology");
  const fs::path policy_path = ArgValue(argc, argv, "--auto-tune-policy");
  const fs::path output_dir = ArgValue(argc, argv, "--out");
  const std::string geometry_head =
      ArgValue(argc, argv, "--geometry-roi-head-enabled");
  if (!fs::is_directory(dataset_root) || !IsRegularFile(template_path) ||
      !IsRegularFile(parent_checkpoint) || !IsRegularFile(ontology_path) ||
      !IsRegularFile(policy_path) ||
      (geometry_head != "0" && geometry_head != "1")) {
    std::cout << "conclusion=CONTROLLED_TRAINING_PLAN_PREFLIGHT_FAIL\n";
    return 2;
  }
  std::string reason;
  if (!OutputDirectoryReady(output_dir, reason)) {
    std::cout << "conclusion=CONTROLLED_TRAINING_PLAN_PREFLIGHT_FAIL\nreason="
              << reason << "\n";
    return 2;
  }
  Ontology ontology;
  Policy policy;
  if (!LoadOntology(ontology_path, ontology, reason) ||
      !LoadPolicy(policy_path, policy, reason)) {
    std::cout << "conclusion=CONTROLLED_TRAINING_PLAN_PREFLIGHT_FAIL\nreason="
              << reason << "\n";
    return 2;
  }
  cv::FileStorage dataset_storage((dataset_root / "package_manifest.json").string(),
                                  cv::FileStorage::READ);
  const cv::FileNode dataset = dataset_storage.root();
  if (!dataset_storage.isOpened() ||
      ReadString(dataset, "schema") !=
          "cxvision.torch_controlled_data_strategy_dataset.v1" ||
      !ReadBool(dataset, "training_allowed", false) ||
      ReadBool(dataset, "promotion_allowed", true) ||
      ReadString(dataset, "training_split") != "train" ||
      ReadString(dataset, "validation_split") != "val" ||
      ReadString(dataset, "holdout_split") != "holdout") {
    std::cout << "conclusion=CONTROLLED_TRAINING_PLAN_PREFLIGHT_FAIL\n"
              << "reason=controlled dataset manifest is not training-eligible\n";
    return 2;
  }
  int train_positive_count = 0, hard_negative_count = 0;
  int validation_count = 0, holdout_count = 0;
  dataset["train_positive_count"] >> train_positive_count;
  dataset["hard_negative_count"] >> hard_negative_count;
  dataset["validation_count"] >> validation_count;
  dataset["holdout_count"] >> holdout_count;
  if (train_positive_count <= 0 || validation_count <= 0 || holdout_count <= 0 ||
      !fs::is_directory(dataset_root / "images" / "train") ||
      !fs::is_directory(dataset_root / "images" / "val") ||
      !fs::is_directory(dataset_root / "images" / "holdout")) {
    std::cout << "conclusion=CONTROLLED_TRAINING_PLAN_PREFLIGHT_FAIL\n"
              << "reason=controlled dataset split assets are incomplete\n";
    return 2;
  }
  cv::FileStorage template_storage(template_path.string(), cv::FileStorage::READ);
  const cv::FileNode plan_template = template_storage.root();
  if (!template_storage.isOpened() ||
      ReadString(plan_template, "schema") != "cxvision.yolov8n_cpp_training_plan.v1") {
    std::cout << "conclusion=CONTROLLED_TRAINING_PLAN_PREFLIGHT_FAIL\n"
              << "reason=training template schema is invalid\n";
    return 2;
  }
  const std::vector<std::string> required_scalars = {
      "epochs", "batch_size", "input_size", "max_train_batches_per_epoch",
      "learning_rate", "assignment_topk", "classification_focal_gamma",
      "use_assignment_quality_targets", "global_multiscale_assignment",
      "validation_interval", "early_stop_patience", "minimum_complete_epochs",
      "minimum_f1_delta", "restore_best_validation_checkpoint",
      "postprocess_confidence_threshold", "postprocess_iou_threshold",
      "postprocess_max_detections", "postprocess_class_agnostic_nms",
      "evaluation_match_iou_threshold", "seed", "num_classes"};
  for (const std::string &key : required_scalars) {
    if (plan_template[key].empty()) {
      std::cout << "conclusion=CONTROLLED_TRAINING_PLAN_PREFLIGHT_FAIL\n"
                << "reason=training template missing field:" << key << "\n";
      return 2;
    }
  }
  int num_classes = 0;
  plan_template["num_classes"] >> num_classes;
  std::vector<std::string> class_names;
  std::vector<std::string> frozen_prefixes;
  std::vector<double> class_weights;
  LoadStringList(plan_template["class_names"], class_names);
  LoadStringList(plan_template["frozen_parameter_prefixes"], frozen_prefixes);
  LoadDoubleList(plan_template["class_loss_weights"], class_weights);
  if (num_classes != policy.required_class_count ||
      num_classes != static_cast<int>(ontology.classes.size()) ||
      class_names.size() != ontology.classes.size() || frozen_prefixes.empty() ||
      class_weights.size() != ontology.classes.size()) {
    std::cout << "conclusion=CONTROLLED_TRAINING_PLAN_PREFLIGHT_FAIL\n"
              << "reason=template class or freeze contract disagrees with frozen ontology\n";
    return 2;
  }
  for (std::size_t index = 0; index < ontology.classes.size(); ++index) {
    if (class_names[index] != ontology.classes[index].name) {
      std::cout << "conclusion=CONTROLLED_TRAINING_PLAN_PREFLIGHT_FAIL\n"
                << "reason=template class order disagrees with frozen ontology\n";
      return 2;
    }
  }
  std::ostringstream plan;
  plan << std::setprecision(12)
       << "{\"schema\":\"cxvision.yolov8n_cpp_training_plan.v1\"";
  for (const std::string &key : required_scalars) {
    const cv::FileNode node = plan_template[key];
    plan << ",\"" << key << "\":";
    if (key == "learning_rate" || key == "classification_focal_gamma" ||
        key == "minimum_f1_delta" ||
        key == "postprocess_confidence_threshold" ||
        key == "postprocess_iou_threshold" ||
        key == "evaluation_match_iou_threshold") {
      double value = 0.0;
      node >> value;
      plan << value;
    } else {
      int value = 0;
      node >> value;
      plan << value;
    }
  }
  plan << ",\"class_loss_weights\":[";
  for (std::size_t index = 0; index < class_weights.size(); ++index) {
    if (index > 0) plan << ',';
    plan << class_weights[index];
  }
  plan << "],\"frozen_parameter_prefixes\":[";
  for (std::size_t index = 0; index < frozen_prefixes.size(); ++index) {
    if (index > 0) plan << ',';
    plan << '\"' << JsonEscape(frozen_prefixes[index]) << '\"';
  }
  plan << "],\"class_names\":[";
  for (std::size_t index = 0; index < ontology.classes.size(); ++index) {
    if (index > 0) plan << ',';
    plan << '\"' << JsonEscape(ontology.classes[index].name) << '\"';
  }
  plan << "],\"geometry_roi_head_enabled\":" << geometry_head
       << ",\"parent_checkpoint\":\""
       << JsonEscape(parent_checkpoint.string()) << "\""
       << ",\"controlled_dataset_manifest\":\""
       << JsonEscape((dataset_root / "package_manifest.json").string()) << "\""
       << ",\"training_template_ref\":\""
       << JsonEscape(template_path.string()) << "\""
       << ",\"auto_tune_policy_ref\":\""
       << JsonEscape(policy_path.string()) << "\""
       << ",\"ontology_ref\":\"" << JsonEscape(ontology_path.string()) << "\""
       << ",\"controlled_train_positive_count\":" << train_positive_count
       << ",\"controlled_hard_negative_count\":" << hard_negative_count
       << ",\"fixed_validation_count\":" << validation_count
       << ",\"locked_holdout_count\":" << holdout_count
       << ",\"selection_split\":\"" << JsonEscape(policy.selection_split) << "\""
       << ",\"confirmation_split\":\"" << JsonEscape(policy.confirmation_split) << "\""
       << ",\"human_review_required\":true,\"promotion_allowed\":false"
       << ",\"active_allowed\":false,\"external_script_execution_allowed\":false} ";
  if (!WriteJson(output_dir, "yolov8n_cpp_training_plan.json", plan.str(), reason)) {
    std::cout << "conclusion=CONTROLLED_TRAINING_PLAN_WRITE_FAIL\nreason="
              << reason << "\n";
    return 1;
  }
  std::ostringstream receipt;
  receipt << "{\"schema\":\"cxvision.torch_controlled_training_plan_receipt.v1\""
          << ",\"executor\":\"CXX_RUNTIME\""
          << ",\"status\":\"TRAINING_PLAN_READY\""
          << ",\"parent_checkpoint_digest\":\"" << FileDigest(parent_checkpoint) << "\""
          << ",\"dataset_manifest_digest\":\""
          << FileDigest(dataset_root / "package_manifest.json") << "\""
          << ",\"template_digest\":\"" << FileDigest(template_path) << "\""
          << ",\"geometry_roi_head_enabled\":" << geometry_head
          << ",\"promotion_allowed\":false,\"active_allowed\":false}";
  if (!WriteJson(output_dir, "controlled_training_plan_receipt.json", receipt.str(), reason)) {
    std::cout << "conclusion=CONTROLLED_TRAINING_PLAN_WRITE_FAIL\nreason="
              << reason << "\n";
    return 1;
  }
  std::cout << "conclusion=CONTROLLED_TRAINING_PLAN_READY\ntraining_plan="
            << (output_dir / "yolov8n_cpp_training_plan.json").string()
            << "\nreceipt=" << (output_dir / "controlled_training_plan_receipt.json").string()
            << "\n";
  return 0;
}

inline int RunControlledCandidateBundleCli(int argc, char **argv) {
  const fs::path model_manifest = ArgValue(argc, argv, "--model-manifest");
  const fs::path checkpoint = ArgValue(argc, argv, "--checkpoint");
  const fs::path rollback_checkpoint = ArgValue(argc, argv, "--rollback-checkpoint");
  const fs::path training_strategy = ArgValue(argc, argv, "--training-strategy");
  const fs::path postprocess_profile = ArgValue(argc, argv, "--postprocess-profile");
  const fs::path validation_report = ArgValue(argc, argv, "--validation-report");
  const fs::path holdout_report = ArgValue(argc, argv, "--holdout-report");
  const fs::path policy_path = ArgValue(argc, argv, "--auto-tune-policy");
  const fs::path output_dir = ArgValue(argc, argv, "--out");
  const std::vector<fs::path> required = {
      model_manifest, checkpoint, rollback_checkpoint, training_strategy,
      postprocess_profile, validation_report, holdout_report, policy_path};
  for (const fs::path &path : required) {
    if (!IsRegularFile(path)) {
      std::cout << "conclusion=CANDIDATE_BUNDLE_PREFLIGHT_FAIL\nreason=missing asset:"
                << path.string() << "\n";
      return 2;
    }
  }
  std::string reason;
  if (!OutputDirectoryReady(output_dir, reason)) {
    std::cout << "conclusion=CANDIDATE_BUNDLE_PREFLIGHT_FAIL\nreason=" << reason << "\n";
    return 2;
  }
  Policy policy;
  if (!LoadPolicy(policy_path, policy, reason)) {
    std::cout << "conclusion=CANDIDATE_BUNDLE_PREFLIGHT_FAIL\nreason=" << reason << "\n";
    return 2;
  }
  auto read_report = [](const fs::path &path, std::string &split,
                         double &precision, double &recall, double &miss_rate,
                         double &false_alarm_rate, double &single_rate,
                         int &case_count,
                         std::vector<std::string> &failure_rows) {
    cv::FileStorage storage(path.string(), cv::FileStorage::READ);
    if (!storage.isOpened()) return false;
    const cv::FileNode root = storage.root();
    if (ReadString(root, "schema") != "cxvision.yolov8n_cpp_paired_inference.v1" ||
        ReadString(root, "status") != "CXX_YOLOV8N_PAIRED_INFERENCE_EXECUTION_PASS")
      return false;
    split = ReadString(root, "evaluation_split");
    root["selected_case_count"] >> case_count;
    const cv::FileNode metrics = root["metrics"];
    metrics["candidate_precision"] >> precision;
    metrics["candidate_recall"] >> recall;
    metrics["candidate_miss_rate"] >> miss_rate;
    metrics["candidate_false_alarm_case_rate"] >> false_alarm_rate;
    metrics["candidate_exact_single_correct_rate"] >> single_rate;
    const cv::FileNode cases = root["cases"];
    if (!cases.isSeq())
      return false;
    for (const cv::FileNode &node : cases) {
      const bool incremental_ok = ReadBool(node, "incremental_ok");
      const bool exact_single_correct =
          ReadBool(node, "incremental_exact_single_correct");
      const bool iou50_match = ReadBool(node, "incremental_iou50_match");
      if (incremental_ok && exact_single_correct && iou50_match)
        continue;
      int detection_count = 0, background_count = 0, cross_class_count = 0;
      int top_class_id = -1, target_class_id = -1;
      double best_iou = 0.0, best_confidence = 0.0;
      node["incremental_detection_count"] >> detection_count;
      node["incremental_background_detection_count"] >> background_count;
      node["incremental_cross_class_target_overlap_count"] >> cross_class_count;
      node["incremental_top_class_id"] >> top_class_id;
      node["incremental_target_class_id"] >> target_class_id;
      node["incremental_best_same_class_iou"] >> best_iou;
      node["incremental_best_same_class_confidence"] >> best_confidence;
      std::ostringstream row;
      row << std::setprecision(9)
          << "{\"review_item\":\"" << JsonEscape(ReadString(node, "review_item"))
          << "\",\"split\":\"" << JsonEscape(split)
          << "\",\"geometry_type\":\"" << JsonEscape(ReadString(node, "geometry_type"))
          << "\",\"signal_family\":\"" << JsonEscape(ReadString(node, "signal_family"))
          << "\",\"input_image\":\"" << JsonEscape(ReadString(node, "input_image"))
          << "\",\"incremental_overlay\":\"" << JsonEscape(ReadString(node, "incremental_overlay"))
          << "\",\"incremental_candidates\":\"" << JsonEscape(ReadString(node, "incremental_candidates"))
          << "\",\"detection_count\":" << detection_count
          << ",\"background_detection_count\":" << background_count
          << ",\"cross_class_overlap_count\":" << cross_class_count
          << ",\"top_class_id\":" << top_class_id
          << ",\"target_class_id\":" << target_class_id
          << ",\"best_same_class_iou\":" << best_iou
          << ",\"best_same_class_confidence\":" << best_confidence
          << ",\"failure_reasons\":[";
      bool first_reason = true;
      const auto add_reason = [&row, &first_reason](const char *value) {
        if (!first_reason) row << ',';
        first_reason = false;
        row << '"' << value << '"';
      };
      if (!incremental_ok) add_reason("inference_execution_failed");
      if (!exact_single_correct) add_reason("single_target_contract_failed");
      if (!iou50_match) add_reason("iou50_match_failed");
      if (background_count > 0) add_reason("background_false_positive");
      if (cross_class_count > 0) add_reason("cross_class_competition");
      row << "]}";
      failure_rows.push_back(row.str());
    }
    return case_count > 0;
  };
  std::string validation_split, holdout_split;
  double val_precision = 0, val_recall = 0, val_miss = 1, val_fa = 1, val_single = 0;
  double hold_precision = 0, hold_recall = 0, hold_miss = 1, hold_fa = 1, hold_single = 0;
  int validation_count = 0, holdout_count = 0;
  std::vector<std::string> validation_failures, holdout_failures;
  if (!read_report(validation_report, validation_split, val_precision, val_recall,
                   val_miss, val_fa, val_single, validation_count,
                   validation_failures) ||
       !read_report(holdout_report, holdout_split, hold_precision, hold_recall,
                    hold_miss, hold_fa, hold_single, holdout_count,
                    holdout_failures) ||
      validation_split != policy.selection_split ||
      holdout_split != policy.confirmation_split) {
    std::cout << "conclusion=CANDIDATE_BUNDLE_PREFLIGHT_FAIL\n"
              << "reason=evaluation reports violate split contract\n";
    return 2;
  }
  const bool quality_gate =
      hold_miss <= policy.maximum_holdout_miss_rate &&
      hold_fa <= policy.maximum_holdout_false_alarm_case_rate;
  const double val_f1 = val_precision + val_recall > 0
      ? 2.0 * val_precision * val_recall / (val_precision + val_recall) : 0.0;
  const double hold_f1 = hold_precision + hold_recall > 0
      ? 2.0 * hold_precision * hold_recall / (hold_precision + hold_recall) : 0.0;
  std::ostringstream failure_samples;
  failure_samples << "{\"schema\":\"cxvision.torch_candidate_failure_samples.v1\""
                  << ",\"status\":\"FAILURE_SAMPLES_RECORDED\""
                  << ",\"validation_report_ref\":\"" << JsonEscape(validation_report.string())
                  << "\",\"holdout_report_ref\":\"" << JsonEscape(holdout_report.string())
                  << "\",\"failure_count\":"
                  << (validation_failures.size() + holdout_failures.size())
                  << ",\"uncertainty_rule\":\"NOT_DEFINED_BY_CURRENT_POLICY\""
                  << ",\"handling_conclusion\":\"FEED_BACK_TO_DATA_AND_TRAINING_STRATEGY\""
                  << ",\"samples\":[";
  bool first_failure = true;
  for (const std::string &row : validation_failures) {
    if (!first_failure) failure_samples << ',';
    first_failure = false;
    failure_samples << row;
  }
  for (const std::string &row : holdout_failures) {
    if (!first_failure) failure_samples << ',';
    first_failure = false;
    failure_samples << row;
  }
  failure_samples << "]}";
  if (!WriteJson(output_dir, "failure_samples.json", failure_samples.str(), reason)) {
    std::cout << "conclusion=CANDIDATE_BUNDLE_WRITE_FAIL\nreason=" << reason << "\n";
    return 1;
  }
  std::ostringstream bundle;
  bundle << std::setprecision(9)
         << "{\"schema\":\"cxvision.torch_incremental_candidate_bundle.v1\""
         << ",\"status\":\"" << (quality_gate ? "EVALUATED" : "REJECTED") << "\""
         << ",\"published_with_incremental_strategy\":true"
         << ",\"candidate_allowed\":" << (quality_gate ? "true" : "false")
         << ",\"approved\":false,\"active_allowed\":false"
         << ",\"human_business_review_required\":true"
         << ",\"holdout_reused_after_diagnostic_feedback\":true"
         << ",\"fresh_business_acceptance_required\":true"
         << ",\"model\":{\"manifest_ref\":\"" << JsonEscape(model_manifest.string())
         << "\",\"checkpoint_ref\":\"" << JsonEscape(checkpoint.string())
         << "\",\"checkpoint_digest\":\"" << FileDigest(checkpoint) << "\"}"
         << ",\"incremental_strategy\":{\"training_strategy_ref\":\""
         << JsonEscape(training_strategy.string()) << "\",\"postprocess_profile_ref\":\""
         << JsonEscape(postprocess_profile.string()) << "\",\"policy_ref\":\""
         << JsonEscape(policy_path.string())
         << "\",\"feedback_loop\":[\"classify_problem\",\"select_strategy_asset\","
            "\"materialize_controlled_dataset\",\"train_from_parent\","
            "\"select_validation_best\",\"run_regression_holdout\","
            "\"accept_or_rollback\"]}"
          << ",\"evaluation\":{\"validation\":{\"report_ref\":\""
          << JsonEscape(validation_report.string()) << "\",\"case_count\":" << validation_count
         << ",\"f1\":" << val_f1 << ",\"miss_rate\":" << val_miss
         << ",\"false_alarm_case_rate\":" << val_fa
         << ",\"exact_single_correct_rate\":" << val_single << "}"
          << ",\"holdout_regression\":{\"report_ref\":\""
          << JsonEscape(holdout_report.string()) << "\",\"case_count\":" << holdout_count
         << ",\"f1\":" << hold_f1 << ",\"miss_rate\":" << hold_miss
         << ",\"false_alarm_case_rate\":" << hold_fa
          << ",\"exact_single_correct_rate\":" << hold_single << "}}"
          << ",\"quality_gate\":{\"passed\":" << (quality_gate ? "true" : "false")
          << ",\"maximum_holdout_miss_rate\":" << policy.maximum_holdout_miss_rate
          << ",\"maximum_holdout_false_alarm_case_rate\":"
          << policy.maximum_holdout_false_alarm_case_rate << "}"
          << ",\"failure_samples_ref\":\""
          << JsonEscape((output_dir / "failure_samples.json").string()) << "\""
         << ",\"rollback\":{\"checkpoint_ref\":\""
         << JsonEscape(rollback_checkpoint.string()) << "\",\"checkpoint_digest\":\""
         << FileDigest(rollback_checkpoint) << "\",\"automatic_on_gate_failure\":true}}";
  if (!WriteJson(output_dir, "candidate_bundle.json", bundle.str(), reason)) {
    std::cout << "conclusion=CANDIDATE_BUNDLE_WRITE_FAIL\nreason=" << reason << "\n";
    return 1;
  }
  std::cout << "conclusion=CANDIDATE_BUNDLE_COMPLETE\nstatus="
            << (quality_gate ? "EVALUATED" : "REJECTED")
            << "\nbundle=" << (output_dir / "candidate_bundle.json").string() << "\n";
  return 0;
}

} // namespace cxvision_geometry_auto_tune
