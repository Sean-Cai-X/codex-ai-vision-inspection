#pragma once

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

// Converts asset-driven geometry augmentation cases into a Torch package.
// Case identity, review labels and masks are read exclusively from assets.
namespace cxvision_geometry_yolo_mask_package {
namespace fs = std::filesystem;

inline std::string ArgValue(int argc, char **argv, const std::string &name) {
  for (int index = 1; index + 1 < argc; ++index)
    if (argv[index] != nullptr && name == argv[index] && argv[index + 1] != nullptr)
      return argv[index + 1];
  return {};
}

inline std::string EscapeJson(const std::string &value) {
  std::string out;
  for (const char ch : value) {
    if (ch == '\\') out += "\\\\";
    else if (ch == '"') out += "\\\"";
    else if (ch == '\n') out += "\\n";
    else if (ch != '\r') out += ch;
  }
  return out;
}

inline std::string Digest(const fs::path &path) {
  std::ifstream input(path, std::ios::binary);
  uint64_t value = 1469598103934665603ull;
  char buffer[4096];
  while (input.good()) {
    input.read(buffer, sizeof(buffer));
    for (std::streamsize index = 0; index < input.gcount(); ++index) {
      value ^= static_cast<unsigned char>(buffer[index]);
      value *= 1099511628211ull;
    }
  }
  std::ostringstream out;
  out << "fnv1a64:" << std::hex << std::setw(16) << std::setfill('0') << value;
  return out.str();
}

inline bool IsFile(const fs::path &path) {
  std::error_code error;
  return fs::is_regular_file(path, error) && !error;
}

inline fs::path Resolve(const fs::path &manifest, const std::string &reference) {
  const fs::path path(reference);
  return path.is_absolute() ? path : manifest.parent_path() / path;
}

inline int RunGeometryYoloMaskPackageCli(int argc, char **argv) {
  const fs::path dataset_manifest_path = ArgValue(argc, argv, "--geometry-dataset-manifest");
  const fs::path ontology_path = ArgValue(argc, argv, "--ontology");
  const fs::path output_dir = ArgValue(argc, argv, "--out");
  if (!IsFile(dataset_manifest_path) || !IsFile(ontology_path) || output_dir.empty()) {
    std::cout << "conclusion=GEOMETRY_MASK_PACKAGE_PREFLIGHT_FAIL\n";
    return 2;
  }
  std::error_code error;
  if (fs::exists(output_dir, error)) {
    std::cout << "conclusion=GEOMETRY_MASK_PACKAGE_PREFLIGHT_FAIL\nreason=output_must_be_new\n";
    return 2;
  }
  cv::FileStorage dataset(dataset_manifest_path.string(), cv::FileStorage::READ);
  cv::FileStorage ontology(ontology_path.string(), cv::FileStorage::READ);
  if (!dataset.isOpened() || !ontology.isOpened() ||
      static_cast<std::string>(dataset["schema"]) != "cxvision.geometry_augmentation_dataset.v1" ||
      static_cast<std::string>(ontology["schema"]) != "cxvision.torch_geometry_ontology.v2") {
    std::cout << "conclusion=GEOMETRY_MASK_PACKAGE_PREFLIGHT_FAIL\nreason=schema_invalid\n";
    return 2;
  }
  std::map<std::string, int> class_ids;
  std::vector<std::string> class_names;
  for (const cv::FileNode &node : ontology["classes"]) {
    const std::string name = static_cast<std::string>(node["name"]);
    int id = -1;
    node["id"] >> id;
    if (name.empty() || id != static_cast<int>(class_names.size())) {
      std::cout << "conclusion=GEOMETRY_MASK_PACKAGE_PREFLIGHT_FAIL\nreason=ontology_not_contiguous\n";
      return 2;
    }
    class_ids[name] = id;
    class_names.push_back(name);
  }
  if (class_names.size() != 7) {
    std::cout << "conclusion=GEOMETRY_MASK_PACKAGE_PREFLIGHT_FAIL\nreason=seven_class_ontology_required\n";
    return 2;
  }
  std::map<std::string, std::string> legacy_aliases;
  for (const cv::FileNode &node : ontology["legacy_aliases"]) {
    const std::string source = static_cast<std::string>(node["source"]);
    const std::string target = static_cast<std::string>(node["target"]);
    int requires_receipt = 0;
    node["requires_migration_receipt"] >> requires_receipt;
    if (!source.empty() && class_ids.count(target) != 0 && requires_receipt != 0)
      legacy_aliases[source] = target;
  }
  fs::create_directories(output_dir / "images" / "train", error);
  fs::create_directories(output_dir / "labels" / "train", error);
  fs::create_directories(output_dir / "masks" / "train", error);
  fs::create_directories(output_dir / "targets" / "train", error);
  fs::create_directories(output_dir / "images" / "validation", error);
  fs::create_directories(output_dir / "labels" / "validation", error);
  fs::create_directories(output_dir / "masks" / "validation", error);
  fs::create_directories(output_dir / "targets" / "validation", error);
  fs::create_directories(output_dir / "images" / "holdout", error);
  fs::create_directories(output_dir / "labels" / "holdout", error);
  fs::create_directories(output_dir / "masks" / "holdout", error);
  fs::create_directories(output_dir / "targets" / "holdout", error);
  if (error) {
    std::cout << "conclusion=GEOMETRY_MASK_PACKAGE_OUTPUT_FAIL\nreason=" << error.message() << "\n";
    return 1;
  }
  std::ostringstream samples;
  bool first = true;
  int ordinal = 0, train = 0, validation = 0, holdout = 0;
  for (const cv::FileNode &node : dataset["samples"]) {
    const std::string split = static_cast<std::string>(node["split"]);
    const std::string source_geometry = static_cast<std::string>(node["geometry_type"]);
    const auto alias = legacy_aliases.find(source_geometry);
    const std::string geometry = alias == legacy_aliases.end() ? source_geometry : alias->second;
    const std::string review_item = static_cast<std::string>(node["review_item"]);
    const std::string case_ref = static_cast<std::string>(node["case_path"]);
    if (class_ids.count(geometry) == 0 || (split != "train" && split != "validation" && split != "holdout")) {
      std::cout << "conclusion=GEOMETRY_MASK_PACKAGE_PREFLIGHT_FAIL\nreason=sample_class_or_split_invalid\n";
      return 2;
    }
    const fs::path case_dir = Resolve(dataset_manifest_path, case_ref);
    const fs::path image = case_dir / "source_image.png";
    const fs::path mask = case_dir / "typed_label.png";
    const fs::path position = case_dir / "position_annotation.json";
    const fs::path geometry_facts = case_dir / "geometry_facts.json";
    const fs::path training_target = case_dir / "training_target.json";
    const fs::path metrology_target = case_dir / "metrology_target.json";
    const fs::path boundary_map = case_dir / "latent_boundary_map.png";
    if (!IsFile(image) || !IsFile(mask) || !IsFile(position) ||
        !IsFile(geometry_facts) || !IsFile(training_target) ||
        !IsFile(metrology_target)) {
      std::cout << "conclusion=GEOMETRY_MASK_PACKAGE_PREFLIGHT_FAIL\nreason=required_case_asset_missing\n";
      return 2;
    }
    cv::FileStorage position_file(position.string(), cv::FileStorage::READ);
    const cv::FileNode bbox = position_file["bbox_xywh"];
    int width = 0, height = 0;
    position_file["image_width"] >> width;
    position_file["image_height"] >> height;
    if (!position_file.isOpened() || !bbox.isSeq() || bbox.size() != 4 || width <= 0 || height <= 0) {
      std::cout << "conclusion=GEOMETRY_MASK_PACKAGE_PREFLIGHT_FAIL\nreason=position_annotation_invalid\n";
      return 2;
    }
    double x = 0.0, y = 0.0, w = 0.0, h = 0.0;
    bbox[0] >> x; bbox[1] >> y; bbox[2] >> w; bbox[3] >> h;
    if (w <= 0.0 || h <= 0.0) {
      std::cout << "conclusion=GEOMETRY_MASK_PACKAGE_PREFLIGHT_FAIL\nreason=bbox_invalid\n";
      return 2;
    }
    const std::string stem = std::to_string(ordinal++) + "_" + geometry;
    const fs::path image_target = output_dir / "images" / split / (stem + image.extension().string());
    const fs::path mask_target = output_dir / "masks" / split / (stem + ".png");
    const fs::path label_target = output_dir / "labels" / split / (stem + ".txt");
    const fs::path geometry_target = output_dir / "targets" / split / (stem + "_geometry_facts.json");
    const fs::path training_target_copy = output_dir / "targets" / split / (stem + "_training_target.json");
    const fs::path metrology_target_copy = output_dir / "targets" / split / (stem + "_metrology_target.json");
    const fs::path boundary_target = output_dir / "targets" / split / (stem + "_boundary.png");
    fs::copy_file(image, image_target, fs::copy_options::none, error);
    fs::copy_file(mask, mask_target, fs::copy_options::none, error);
    fs::copy_file(geometry_facts, geometry_target, fs::copy_options::none, error);
    fs::copy_file(training_target, training_target_copy, fs::copy_options::none, error);
    fs::copy_file(metrology_target, metrology_target_copy, fs::copy_options::none, error);
    const bool boundary_derived_from_typed_label = !IsFile(boundary_map);
    if (boundary_derived_from_typed_label) {
      const cv::Mat typed_label = cv::imread(mask.string(), cv::IMREAD_GRAYSCALE);
      if (typed_label.empty()) {
        std::cout << "conclusion=GEOMETRY_MASK_PACKAGE_OUTPUT_FAIL\nreason=boundary_target_derive_failed\n";
        return 1;
      }
      cv::Mat boundary;
      cv::morphologyEx(typed_label, boundary, cv::MORPH_GRADIENT,
                       cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3)));
      if (boundary.empty() || !cv::imwrite(boundary_target.string(), boundary)) {
        std::cout << "conclusion=GEOMETRY_MASK_PACKAGE_OUTPUT_FAIL\nreason=boundary_target_derive_failed\n";
        return 1;
      }
    } else {
      fs::copy_file(boundary_map, boundary_target, fs::copy_options::none, error);
    }
    if (error) {
      std::cout << "conclusion=GEOMETRY_MASK_PACKAGE_OUTPUT_FAIL\nreason=asset_copy_failed\n";
      return 1;
    }
    std::ofstream label(label_target);
    label << class_ids[geometry] << " " << (x + w * 0.5) / width << " " << (y + h * 0.5) / height
          << " " << w / width << " " << h / height << "\n";
    if (!label.good()) {
      std::cout << "conclusion=GEOMETRY_MASK_PACKAGE_OUTPUT_FAIL\nreason=label_write_failed\n";
      return 1;
    }
    if (split == "train") ++train; else if (split == "validation") ++validation; else ++holdout;
    if (!first) samples << ",";
    first = false;
    samples << "{\"review_item\":\"" << EscapeJson(review_item) << "\",\"split\":\"" << split
            << "\",\"geometry_type\":\"" << geometry << "\",\"image\":\""
            << image_target.lexically_relative(output_dir).generic_string() << "\",\"label\":\""
             << label_target.lexically_relative(output_dir).generic_string() << "\",\"target_mask\":\""
             << mask_target.lexically_relative(output_dir).generic_string() << "\",\"geometry_facts\":\""
             << geometry_target.lexically_relative(output_dir).generic_string() << "\",\"training_target\":\""
             << training_target_copy.lexically_relative(output_dir).generic_string() << "\",\"metrology_target\":\""
             << metrology_target_copy.lexically_relative(output_dir).generic_string() << "\",\"boundary_map\":\""
            << boundary_target.lexically_relative(output_dir).generic_string() << "\",\"geometry_target_status\":\"READY\",\"source_case_ref\":\""
            << EscapeJson(case_ref) << "\",\"source_image_digest\":\"" << Digest(image)
            << "\",\"source_mask_digest\":\"" << Digest(mask)
            << "\",\"boundary_map_provenance\":\""
            << (boundary_derived_from_typed_label ? "derived_from_typed_label" : "source_asset")
            << "\",\"geometry_type_source\":\"" << EscapeJson(source_geometry)
            << "\",\"geometry_type_migration\":\""
            << (alias == legacy_aliases.end() ? "none" : "ontology_legacy_alias") << "\"}";
  }
  std::ofstream manifest(output_dir / "package_manifest.json");
  manifest << "{\"schema\":\"cxvision.yolov8n_aabb_mask_package.v1\",\"classes\":[";
  for (std::size_t index = 0; index < class_names.size(); ++index) {
    if (index) manifest << ",";
    manifest << "\"" << EscapeJson(class_names[index]) << "\"";
  }
  manifest << "],\"train_count\":" << train << ",\"validation_count\":" << validation
           << ",\"holdout_count\":" << holdout << ",\"training_enabled\":false"
           << ",\"promotion_allowed\":false,\"samples\":[" << samples.str() << "]}";
  if (!manifest.good()) {
    std::cout << "conclusion=GEOMETRY_MASK_PACKAGE_OUTPUT_FAIL\nreason=manifest_write_failed\n";
    return 1;
  }
  std::cout << "conclusion=GEOMETRY_MASK_PACKAGE_COMPLETE\ntrain=" << train
            << "\nvalidation=" << validation << "\nholdout=" << holdout
            << "\npackage_manifest=" << (output_dir / "package_manifest.json").string() << "\n";
  return 0;
}
} // namespace cxvision_geometry_yolo_mask_package
