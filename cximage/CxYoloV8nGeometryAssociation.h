#pragma once

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_set>
#include <vector>

namespace cxvision_yolov8n_geometry {
namespace fs = std::filesystem;

struct Sample {
  std::string review_item;
  std::string split;
  std::string geometry_type;
  std::string case_path;
  fs::path image_path;
  int image_width = 0;
  int image_height = 0;
  std::array<double, 4> bbox_xywh{{0.0, 0.0, 0.0, 0.0}};
  double rotation_deg = 0.0;
  double scale_factor = 1.0;
  bool rotation_signal = false;
  bool scale_signal = false;
  bool deformation_signal = false;
  bool generated = false;
};

struct Rejection {
  std::string case_path;
  std::string reason;
};

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

inline std::string Escape(const std::string &text) {
  std::string out;
  for (const char c : text) {
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

inline std::string NodeString(const cv::FileNode &node,
                              const std::string &fallback = {}) {
  return node.empty() || !node.isString() ? fallback
                                          : static_cast<std::string>(node);
}

inline double NodeDouble(const cv::FileNode &node, double fallback) {
  return node.empty() || (!node.isInt() && !node.isReal())
             ? fallback
             : static_cast<double>(node);
}

inline int NodeInt(const cv::FileNode &node, int fallback) {
  return node.empty() || (!node.isInt() && !node.isReal())
             ? fallback
             : static_cast<int>(node);
}

inline bool ReadJson(const fs::path &path, cv::FileStorage &storage,
                     std::string &reason) {
  storage.open(path.string(),
               cv::FileStorage::READ | cv::FileStorage::FORMAT_JSON);
  if (storage.isOpened())
    return true;
  reason = "JSON_PARSE_FAILED: " + path.generic_string();
  return false;
}

inline bool IsWithin(const fs::path &root, const fs::path &candidate) {
  std::error_code ec;
  const fs::path canonical_root = fs::weakly_canonical(root, ec);
  if (ec)
    return false;
  const fs::path canonical_candidate = fs::weakly_canonical(candidate, ec);
  if (ec)
    return false;
  const fs::path relative = fs::relative(canonical_candidate, canonical_root, ec);
  if (ec)
    return false;
  return relative.empty() || relative.begin() == relative.end() ||
         *relative.begin() != "..";
}

inline bool WriteText(const fs::path &path, const std::string &content,
                      std::string &reason) {
  std::error_code ec;
  fs::create_directories(path.parent_path(), ec);
  if (ec) {
    reason = "OUTPUT_DIRECTORY_CREATE_FAILED: " +
             path.parent_path().generic_string();
    return false;
  }
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << content;
  if (output.good())
    return true;
  reason = "OUTPUT_WRITE_FAILED: " + path.generic_string();
  return false;
}

inline std::string SafeStem(const std::string &value) {
  std::string out;
  for (const unsigned char c : value) {
    if (out.size() >= 40)
      break;
    if (std::isalnum(c))
      out.push_back(static_cast<char>(std::tolower(c)));
    else if (!out.empty() && out.back() != '_')
      out.push_back('_');
  }
  while (!out.empty() && out.back() == '_')
    out.pop_back();
  return out.empty() ? "sample" : out;
}

inline std::string StableSuffix(const std::string &value) {
  const std::size_t hash = std::hash<std::string>{}(value);
  std::ostringstream stream;
  stream << std::hex << std::setw(12) << std::setfill('0')
         << static_cast<unsigned long long>(hash & 0xffffffffffffULL);
  return stream.str();
}

inline bool ReadBBox(const cv::FileNode &node,
                     std::array<double, 4> &bbox) {
  if (!node.isSeq() || node.size() != 4)
    return false;
  std::size_t index = 0;
  for (const auto &value : node)
    bbox[index++] = static_cast<double>(value);
  return bbox[2] > 0.0 && bbox[3] > 0.0;
}

inline bool ReadImageSize(const cv::FileNode &node, int &width, int &height) {
  if (!node.isSeq() || node.size() != 2)
    return false;
  auto it = node.begin();
  width = static_cast<int>(*it);
  height = static_cast<int>(*++it);
  return width > 0 && height > 0;
}

inline bool LoadScalePlan(const fs::path &path, std::vector<double> &factors,
                          int &max_sources_per_group, std::string &reason) {
  cv::FileStorage plan;
  if (!ReadJson(path, plan, reason))
    return false;
  if (NodeString(plan["schema"]) !=
      "cxvision.yolov8n_geometry_scale_plan.v1") {
    reason = "SCALE_PLAN_SCHEMA_UNSUPPORTED";
    return false;
  }
  max_sources_per_group =
      NodeInt(plan["max_sources_per_split_geometry"], 0);
  const cv::FileNode entries = plan["scale_factors"];
  if (!entries.isSeq() || max_sources_per_group <= 0) {
    reason = "SCALE_PLAN_FIELDS_INVALID";
    return false;
  }
  for (const auto &entry : entries) {
    const double factor = static_cast<double>(entry);
    if (std::isfinite(factor) && factor > 0.0 &&
        std::abs(factor - 1.0) > 1e-6)
      factors.push_back(factor);
  }
  if (!factors.empty())
    return true;
  reason = "SCALE_PLAN_HAS_NO_EFFECTIVE_FACTOR";
  return false;
}

inline bool LoadSourceSamples(const fs::path &root,
                              std::vector<Sample> &samples,
                              std::vector<Rejection> &rejections,
                              std::string &reason) {
  cv::FileStorage manifest;
  if (!ReadJson(root / "dataset_manifest.json", manifest, reason))
    return false;
  if (NodeString(manifest["schema"]) !=
      "cxvision.geometry_augmentation_dataset.v1") {
    reason = "DATASET_MANIFEST_SCHEMA_UNSUPPORTED";
    return false;
  }
  const cv::FileNode entries = manifest["samples"];
  if (!entries.isSeq()) {
    reason = "DATASET_MANIFEST_SAMPLES_MISSING";
    return false;
  }

  std::unordered_set<std::string> accepted_paths;
  for (const auto &entry : entries) {
    const std::string case_path = NodeString(entry["case_path"]);
    auto reject = [&](const std::string &why) {
      rejections.push_back({case_path, why});
    };
    const std::string split = NodeString(entry["split"]);
    if (case_path.empty()) {
      reject("CASE_PATH_MISSING");
      continue;
    }
    if (split != "train" && split != "validation") {
      reject("SPLIT_UNSUPPORTED");
      continue;
    }
    if ((split == "train" &&
         NodeInt(entry["training_eligible"], 0) == 0) ||
        (split == "validation" && NodeInt(entry["identifiable"], 0) == 0)) {
      reject(split == "train" ? "TRAINING_NOT_ELIGIBLE"
                              : "VALIDATION_NOT_IDENTIFIABLE");
      continue;
    }

    const fs::path case_dir = root / fs::path(case_path);
    std::error_code ec;
    if (!fs::is_directory(case_dir, ec) || ec || !IsWithin(root, case_dir)) {
      reject("CASE_DIRECTORY_INVALID");
      continue;
    }
    const std::string normalized =
        fs::weakly_canonical(case_dir, ec).generic_string();
    if (ec || !accepted_paths.insert(normalized).second) {
      reject(ec ? "CASE_PATH_CANONICALIZATION_FAILED" :
                  "DUPLICATE_CASE_PATH");
      continue;
    }

    const fs::path target_path =
        case_dir / NodeString(entry["metrology_target_ref"],
                              "metrology_target.json");
    if (!fs::is_regular_file(target_path, ec) || ec ||
        !IsWithin(case_dir, target_path)) {
      reject("ASSET_MISSING: metrology_target");
      continue;
    }
    cv::FileStorage target;
    std::string target_reason;
    if (!ReadJson(target_path, target, target_reason)) {
      reject(target_reason);
      continue;
    }

    const fs::path image_path =
        case_dir / NodeString(target["image_ref"], "source_image.png");
    if (!fs::is_regular_file(image_path, ec) || ec ||
        !IsWithin(case_dir, image_path)) {
      reject("ASSET_MISSING: source_image");
      continue;
    }

    Sample sample;
    sample.review_item = NodeString(entry["review_item"], case_path);
    sample.split = split;
    sample.geometry_type = NodeString(entry["geometry_type"]);
    sample.case_path = case_path;
    sample.image_path = image_path;
    if (sample.geometry_type.empty() ||
        !ReadImageSize(target["image_size_px"], sample.image_width,
                       sample.image_height) ||
        !ReadBBox(target["bbox_xywh_px"], sample.bbox_xywh)) {
      reject("TARGET_GEOMETRY_FIELDS_INVALID");
      continue;
    }
    const cv::Mat image = cv::imread(image_path.string(), cv::IMREAD_COLOR);
    if (image.empty() || image.cols != sample.image_width ||
        image.rows != sample.image_height) {
      reject("SOURCE_IMAGE_DECODE_OR_SIZE_FAILED");
      continue;
    }

    sample.rotation_deg = NodeDouble(target["rotation_deg"], 0.0);
    sample.scale_factor = NodeDouble(target["scale_factor"], 1.0);
    sample.rotation_signal = std::abs(sample.rotation_deg) > 1e-6;
    sample.scale_signal = std::abs(sample.scale_factor - 1.0) > 1e-6;
    cv::FileStorage case_manifest;
    if (ReadJson(case_dir / "case_manifest.json", case_manifest,
                 target_reason)) {
      sample.deformation_signal =
          NodeString(case_manifest["degradation_bucket"]) ==
              "structural_defect" ||
          !case_manifest["deformation_parameters"].empty();
    }
    samples.push_back(std::move(sample));
  }
  if (!samples.empty())
    return true;
  reason = "NO_VALID_SOURCE_SAMPLE";
  return false;
}

inline std::array<double, 4>
ScaleBBox(const std::array<double, 4> &bbox, int width, int height,
          double factor) {
  const double image_cx = width * 0.5;
  const double image_cy = height * 0.5;
  const double source_cx = bbox[0] + bbox[2] * 0.5;
  const double source_cy = bbox[1] + bbox[3] * 0.5;
  const double scaled_cx = image_cx + (source_cx - image_cx) * factor;
  const double scaled_cy = image_cy + (source_cy - image_cy) * factor;
  const double scaled_w = bbox[2] * factor;
  const double scaled_h = bbox[3] * factor;
  const double x0 = std::clamp(scaled_cx - scaled_w * 0.5, 0.0,
                               static_cast<double>(width - 1));
  const double y0 = std::clamp(scaled_cy - scaled_h * 0.5, 0.0,
                               static_cast<double>(height - 1));
  const double x1 = std::clamp(scaled_cx + scaled_w * 0.5, x0 + 1.0,
                               static_cast<double>(width));
  const double y1 = std::clamp(scaled_cy + scaled_h * 0.5, y0 + 1.0,
                               static_cast<double>(height));
  return {x0, y0, x1 - x0, y1 - y0};
}

inline bool GenerateScaleSamples(const fs::path &out_root,
                                 const std::vector<Sample> &sources,
                                 const std::vector<double> &factors,
                                 int max_sources_per_group,
                                 std::vector<Sample> &generated,
                                 std::string &reason) {
  std::vector<const Sample *> ordered;
  for (const auto &sample : sources)
    ordered.push_back(&sample);
  std::sort(ordered.begin(), ordered.end(),
            [](const Sample *left, const Sample *right) {
              return std::tie(left->split, left->geometry_type,
                              left->case_path) <
                     std::tie(right->split, right->geometry_type,
                              right->case_path);
            });

  std::map<std::pair<std::string, std::string>, int> group_counts;
  for (const Sample *source : ordered) {
    const auto group = std::make_pair(source->split, source->geometry_type);
    if (group_counts[group] >= max_sources_per_group)
      continue;
    ++group_counts[group];
    const cv::Mat image =
        cv::imread(source->image_path.string(), cv::IMREAD_COLOR);
    if (image.empty()) {
      reason = "SCALE_SOURCE_IMAGE_DECODE_FAILED";
      return false;
    }
    for (const double factor : factors) {
      std::ostringstream factor_text;
      factor_text << std::fixed << std::setprecision(3) << factor;
      const std::string identity =
          source->case_path + "|scale|" + factor_text.str();
      const fs::path case_dir =
          out_root / "generated_scale_cases" / source->split /
          SafeStem(source->geometry_type) /
          ("scale_" + SafeStem(factor_text.str()) + "_" +
           StableSuffix(identity));
      std::error_code ec;
      fs::create_directories(case_dir, ec);
      if (ec) {
        reason = "SCALE_OUTPUT_DIRECTORY_CREATE_FAILED";
        return false;
      }
      const cv::Point2f center(image.cols * 0.5F, image.rows * 0.5F);
      const cv::Mat matrix = cv::getRotationMatrix2D(center, 0.0, factor);
      cv::Mat scaled;
      cv::warpAffine(image, scaled, matrix, image.size(), cv::INTER_LINEAR,
                     cv::BORDER_CONSTANT, cv::mean(image(cv::Rect(0, 0, 1, 1))));
      const fs::path image_path = case_dir / "source_image.png";
      if (!cv::imwrite(image_path.string(), scaled)) {
        reason = "SCALE_IMAGE_WRITE_FAILED";
        return false;
      }

      Sample sample = *source;
      sample.review_item = source->review_item +
                           " | Scale smoke | factor " + factor_text.str();
      sample.case_path = fs::relative(case_dir, out_root).generic_string();
      sample.image_path = image_path;
      sample.bbox_xywh = ScaleBBox(source->bbox_xywh, source->image_width,
                                   source->image_height, factor);
      sample.scale_factor = factor;
      sample.scale_signal = true;
      sample.generated = true;

      std::ostringstream target;
      target << "{\n"
             << "  \"schema\": \"cxvision.metrology_target.v1\",\n"
             << "  \"review_item\": \"" << Escape(sample.review_item)
             << "\",\n"
             << "  \"split\": \"" << Escape(sample.split) << "\",\n"
             << "  \"geometry_type\": \""
             << Escape(sample.geometry_type) << "\",\n"
             << "  \"image_ref\": \"source_image.png\",\n"
             << "  \"image_size_px\": [" << sample.image_width << ", "
             << sample.image_height << "],\n"
             << "  \"bbox_xywh_px\": [" << sample.bbox_xywh[0] << ", "
             << sample.bbox_xywh[1] << ", " << sample.bbox_xywh[2] << ", "
             << sample.bbox_xywh[3] << "],\n"
             << "  \"rotation_deg\": " << sample.rotation_deg << ",\n"
             << "  \"scale_factor\": " << sample.scale_factor << ",\n"
             << "  \"training_eligible\": 1,\n"
             << "  \"training_enabled\": 0,\n"
             << "  \"promotion_allowed\": 0\n"
             << "}\n";
      if (!WriteText(case_dir / "metrology_target.json", target.str(), reason))
        return false;
      generated.push_back(std::move(sample));
    }
  }
  if (!generated.empty())
    return true;
  reason = "NO_SCALE_SAMPLE_GENERATED";
  return false;
}

inline bool ExportYoloPackage(const fs::path &out_root,
                              const std::vector<Sample> &samples,
                              std::map<std::string, int> &class_ids,
                              std::string &reason) {
  std::set<std::string> classes;
  for (const auto &sample : samples)
    classes.insert(sample.geometry_type);
  for (const auto &name : classes)
    class_ids[name] = static_cast<int>(class_ids.size());

  std::ostringstream index;
  index << "{\n  \"schema\": \"cxvision.yolov8n_aabb_package.v1\",\n"
        << "  \"samples\": [\n";
  bool first = true;
  for (const auto &sample : samples) {
    const std::string split_dir =
        sample.split == "validation" ? "val" : "train";
    const std::string identity = sample.case_path + "|" + sample.review_item;
    const std::string stem =
        SafeStem(sample.geometry_type) + "_" + StableSuffix(identity);
    const fs::path image_out = out_root / "yolo_dataset" / "images" /
                               split_dir / (stem + ".png");
    const fs::path label_out = out_root / "yolo_dataset" / "labels" /
                               split_dir / (stem + ".txt");
    std::error_code ec;
    fs::create_directories(image_out.parent_path(), ec);
    fs::copy_file(sample.image_path, image_out,
                  fs::copy_options::overwrite_existing, ec);
    if (ec) {
      reason = "YOLO_IMAGE_COPY_FAILED";
      return false;
    }
    const double cx =
        (sample.bbox_xywh[0] + sample.bbox_xywh[2] * 0.5) /
        sample.image_width;
    const double cy =
        (sample.bbox_xywh[1] + sample.bbox_xywh[3] * 0.5) /
        sample.image_height;
    std::ostringstream label;
    label << class_ids[sample.geometry_type] << " " << std::fixed
          << std::setprecision(8) << std::clamp(cx, 0.0, 1.0) << " "
          << std::clamp(cy, 0.0, 1.0) << " "
          << std::clamp(sample.bbox_xywh[2] / sample.image_width, 0.0, 1.0)
          << " "
          << std::clamp(sample.bbox_xywh[3] / sample.image_height, 0.0, 1.0)
          << "\n";
    if (!WriteText(label_out, label.str(), reason))
      return false;
    if (!first)
      index << ",\n";
    first = false;
    index << "    {\"review_item\": \"" << Escape(sample.review_item)
          << "\", \"split\": \"" << sample.split
          << "\", \"geometry_type\": \""
          << Escape(sample.geometry_type) << "\", \"image\": \""
          << Escape(fs::relative(image_out, out_root / "yolo_dataset")
                        .generic_string())
          << "\", \"label\": \""
          << Escape(fs::relative(label_out, out_root / "yolo_dataset")
                        .generic_string())
          << "\", \"generated\": "
          << (sample.generated ? "true" : "false")
          << ", \"rotation_signal\": "
          << (sample.rotation_signal ? "true" : "false")
          << ", \"scale_signal\": "
          << (sample.scale_signal ? "true" : "false")
          << ", \"deformation_signal\": "
          << (sample.deformation_signal ? "true" : "false") << "}";
  }
  index << "\n  ]\n}\n";
  if (!WriteText(out_root / "yolo_dataset" / "package_manifest.json",
                 index.str(), reason))
    return false;
  std::ostringstream yaml;
  yaml << "path: " << (out_root / "yolo_dataset").generic_string() << "\n"
       << "train: images/train\nval: images/val\nnames:\n";
  for (const auto &entry : class_ids)
    yaml << "  " << entry.second << ": " << entry.first << "\n";
  return WriteText(out_root / "yolo_dataset" / "dataset.yaml", yaml.str(),
                   reason);
}

inline bool WriteReport(const fs::path &source_root,
                        const fs::path &plan_path, const fs::path &out_root,
                        const std::vector<Sample> &sources,
                        const std::vector<Sample> &generated,
                        const std::vector<Rejection> &rejections,
                        const std::map<std::string, int> &class_ids,
                        std::string &status, std::string &reason) {
  std::vector<Sample> all = sources;
  all.insert(all.end(), generated.begin(), generated.end());
  std::size_t train = 0, validation = 0, rotation = 0, scale = 0,
              deformation = 0;
  for (const auto &sample : all) {
    train += sample.split == "train" ? 1U : 0U;
    validation += sample.split == "validation" ? 1U : 0U;
    rotation += sample.rotation_signal ? 1U : 0U;
    scale += sample.scale_signal ? 1U : 0U;
    deformation += sample.deformation_signal ? 1U : 0U;
  }
  const bool preflight = !sources.empty() && rejections.empty();
  const bool coverage = rotation > 0 && scale > 0 && deformation > 0 &&
                        train > 0 && validation > 0;
  status = !preflight
               ? "CXX_YOLOV8N_DATASET_PREFLIGHT_FAIL"
               : coverage
                     ? "CXX_YOLOV8N_GEOMETRY_ASSOCIATION_ASSET_PASS"
                     : "CXX_YOLOV8N_GEOMETRY_ASSOCIATION_ASSET_PARTIAL";
  std::ostringstream json;
  json << "{\n"
       << "  \"schema\": \"cxvision.yolov8n_cpp_geometry_association.v1\",\n"
       << "  \"source_run\": \"" << Escape(source_root.generic_string())
       << "\",\n"
       << "  \"scale_plan\": \"" << Escape(plan_path.generic_string())
       << "\",\n"
       << "  \"output_dir\": \"" << Escape(out_root.generic_string())
       << "\",\n"
       << "  \"source_sample_count\": " << sources.size() << ",\n"
       << "  \"generated_scale_sample_count\": " << generated.size()
       << ",\n"
       << "  \"accepted_sample_count\": " << all.size() << ",\n"
       << "  \"rejected_sample_count\": " << rejections.size() << ",\n"
       << "  \"train_sample_count\": " << train << ",\n"
       << "  \"validation_sample_count\": " << validation << ",\n"
       << "  \"class_count\": " << class_ids.size() << ",\n"
       << "  \"rotation_signal_count\": " << rotation << ",\n"
       << "  \"scale_signal_count\": " << scale << ",\n"
       << "  \"deformation_signal_count\": " << deformation << ",\n"
       << "  \"asset_preflight_status\": \""
       << (preflight ? "ASSET_PREFLIGHT_PASS" : "ASSET_PREFLIGHT_FAIL")
       << "\",\n"
       << "  \"training_status\": \"CXX_YOLOV8N_TRAINING_NOT_RUN\",\n"
       << "  \"learning_curve_status\": \"CXX_YOLOV8N_LEARNING_CURVE_NOT_RUN\",\n"
       << "  \"inference_comparison_status\": \"CXX_YOLOV8N_INFERENCE_COMPARISON_NOT_RUN\",\n"
       << "  \"promotion_allowed\": false,\n"
       << "  \"human_review_required\": true,\n"
       << "  \"final_status\": \"" << status << "\",\n"
       << "  \"rejections\": [";
  for (std::size_t i = 0; i < rejections.size(); ++i) {
    if (i != 0)
      json << ",";
    json << "\n    {\"case_path\": \""
         << Escape(rejections[i].case_path) << "\", \"reason\": \""
         << Escape(rejections[i].reason) << "\"}";
  }
  json << (rejections.empty() ? "" : "\n  ") << "]\n}\n";
  if (!WriteText(out_root / "geometry_association_report.json", json.str(),
                 reason))
    return false;
  std::ostringstream markdown;
  markdown << "# YOLOv8n C++ Geometry Association Asset Report\n\n"
           << "- Final status: " << status << "\n"
           << "- Asset preflight: "
           << (preflight ? "ASSET_PREFLIGHT_PASS" : "ASSET_PREFLIGHT_FAIL")
           << "\n- Source samples: " << sources.size()
           << "\n- Generated scale samples: " << generated.size()
           << "\n- Train / validation: " << train << " / " << validation
           << "\n- Rotation / scale / deformation signals: " << rotation
           << " / " << scale << " / " << deformation
           << "\n- Training: CXX_YOLOV8N_TRAINING_NOT_RUN"
           << "\n- Learning curve: CXX_YOLOV8N_LEARNING_CURVE_NOT_RUN"
           << "\n- Base vs incremental inference: "
              "CXX_YOLOV8N_INFERENCE_COMPARISON_NOT_RUN"
           << "\n- Promotion allowed: false\n";
  return WriteText(out_root / "geometry_association_report.md",
                   markdown.str(), reason);
}

inline int RunYoloV8nGeometryAssociationCli(int argc, char **argv) {
  const fs::path source_root = ArgValue(argc, argv, "--source-run");
  const fs::path out_root = ArgValue(argc, argv, "--out");
  const fs::path plan_path = ArgValue(argc, argv, "--geometry-plan");
  if (source_root.empty() || out_root.empty() || plan_path.empty()) {
    std::cout << "conclusion=ASSET_PREFLIGHT_FAIL\n"
              << "reason=--source-run, --geometry-plan and --out are required\n";
    return 2;
  }
  std::error_code ec;
  if (!fs::is_directory(source_root, ec) || ec ||
      !fs::is_regular_file(plan_path, ec) || ec) {
    std::cout << "conclusion=ASSET_PREFLIGHT_FAIL\n"
              << "reason=source run or geometry plan does not exist\n";
    return 2;
  }
  if (fs::exists(out_root, ec)) {
    std::cout << "conclusion=ASSET_PREFLIGHT_FAIL\n"
              << "reason=output directory already exists; use a new RUN_ID\n";
    return 2;
  }
  fs::create_directories(out_root, ec);
  if (ec) {
    std::cout << "conclusion=ASSET_PREFLIGHT_FAIL\n"
              << "reason=output directory cannot be created\n";
    return 2;
  }

  std::ostringstream command_line;
  for (int i = 0; i < argc; ++i) {
    if (i != 0)
      command_line << " ";
    command_line << "\"" << Escape(argv[i] == nullptr ? "" : argv[i])
                 << "\"";
  }
  const fs::path binary_path =
      fs::absolute(argv[0] == nullptr ? "" : argv[0], ec);
  const auto binary_write_time = fs::last_write_time(binary_path, ec);
  const auto binary_write_ticks =
      ec ? 0LL : static_cast<long long>(
                       binary_write_time.time_since_epoch().count());
  ec.clear();
  const fs::path working_directory = fs::current_path(ec);
  const fs::path build_dir = binary_path.parent_path().parent_path();
  std::ostringstream context;
  context << "{\n"
          << "  \"schema\": \"cxvision.execution_context.v1\",\n"
          << "  \"repo_root\": \""
          << Escape(working_directory.generic_string()) << "\",\n"
          << "  \"build_dir\": \"" << Escape(build_dir.generic_string())
          << "\",\n"
          << "  \"binary_path\": \""
          << Escape(binary_path.generic_string()) << "\",\n"
          << "  \"binary_last_write_ticks\": " << binary_write_ticks
          << ",\n"
          << "  \"working_directory\": \""
          << Escape(working_directory.generic_string()) << "\",\n"
          << "  \"source_revision\": \"WORKTREE_STATUS_RECORDED_EXTERNALLY\",\n"
          << "  \"suite_path\": \"NOT_APPLICABLE\",\n"
          << "  \"catalog_path\": \"NOT_APPLICABLE\",\n"
          << "  \"manifest_path\": \""
          << Escape((source_root / "dataset_manifest.json").generic_string())
          << "\",\n"
          << "  \"output_dir\": \"" << Escape(out_root.generic_string())
          << "\",\n"
          << "  \"unified_log_path\": \""
          << Escape(ArgValue(argc, argv, "--unified-log")) << "\",\n"
          << "  \"command_line\": \"" << Escape(command_line.str())
          << "\"\n"
          << "}\n";
  std::string context_reason;
  if (!WriteText(out_root / "execution_context.json", context.str(),
                 context_reason)) {
    std::cout << "conclusion=ASSET_PREFLIGHT_FAIL\nreason=" << context_reason
              << "\n";
    return 2;
  }

  std::string reason;
  std::vector<double> factors;
  int max_sources_per_group = 0;
  if (!LoadScalePlan(plan_path, factors, max_sources_per_group, reason)) {
    std::cout << "conclusion=ASSET_PREFLIGHT_FAIL\nreason=" << reason << "\n";
    return 2;
  }
  std::vector<Sample> sources;
  std::vector<Rejection> rejections;
  if (!LoadSourceSamples(source_root, sources, rejections, reason)) {
    std::cout << "conclusion=ASSET_PREFLIGHT_FAIL\nreason=" << reason << "\n";
    return 2;
  }
  std::vector<Sample> generated;
  if (!GenerateScaleSamples(out_root, sources, factors,
                            max_sources_per_group, generated, reason)) {
    std::cout << "conclusion=CXX_YOLOV8N_GEOMETRY_ASSOCIATION_ASSET_FAIL\n"
              << "reason=" << reason << "\n";
    return 3;
  }
  std::vector<Sample> package_samples = sources;
  package_samples.insert(package_samples.end(), generated.begin(),
                         generated.end());
  std::map<std::string, int> class_ids;
  if (!ExportYoloPackage(out_root, package_samples, class_ids, reason)) {
    std::cout << "conclusion=CXX_YOLOV8N_GEOMETRY_ASSOCIATION_ASSET_FAIL\n"
              << "reason=" << reason << "\n";
    return 3;
  }
  std::string status;
  if (!WriteReport(source_root, plan_path, out_root, sources, generated,
                   rejections, class_ids, status, reason)) {
    std::cout << "conclusion=CXX_YOLOV8N_GEOMETRY_ASSOCIATION_ASSET_FAIL\n"
              << "reason=" << reason << "\n";
    return 3;
  }
  std::cout << "conclusion=" << status << "\n"
            << "source_sample_count=" << sources.size() << "\n"
            << "generated_scale_sample_count=" << generated.size() << "\n"
            << "rejected_sample_count=" << rejections.size() << "\n"
            << "training_status=CXX_YOLOV8N_TRAINING_NOT_RUN\n"
            << "inference_comparison_status="
               "CXX_YOLOV8N_INFERENCE_COMPARISON_NOT_RUN\n"
            << "report_path="
            << (out_root / "geometry_association_report.json").generic_string()
            << "\n";
  return status == "CXX_YOLOV8N_GEOMETRY_ASSOCIATION_ASSET_PASS" ? 0 : 4;
}

} // namespace cxvision_yolov8n_geometry
