#pragma once

#include <filesystem>
#include <string>

struct CxGeometryReferenceEvaluationOptions {
  std::filesystem::path index_path;
  std::filesystem::path output_dir;
  int threshold = 100;
};

struct CxGeometryReferenceEvaluationResult {
  bool complete = false;
  std::string status = "NOT_RUN";
  std::string reason;
  int discovered_cases = 0;
  int accepted_cases = 0;
  int rejected_cases = 0;
  std::filesystem::path report_json;
  std::filesystem::path report_markdown;
};

bool RunCxGeometryReferenceEvaluation(
    const CxGeometryReferenceEvaluationOptions &options,
    CxGeometryReferenceEvaluationResult &result, std::string &reason);

struct CxGeometryAugmentationDatasetOptions {
  std::filesystem::path reference_index_path;
  std::filesystem::path augmentation_plan_path;
  std::filesystem::path output_dir;
  // When enabled, each source asset must declare source_split=train,
  // source_split=validation, or source_split=holdout. A source may never
  // generate more than one split.
  bool require_source_disjoint_validation = false;
};

struct CxGeometryAugmentationDatasetResult {
  bool complete = false;
  std::string status = "NOT_RUN";
  std::string reason;
  int source_case_count = 0;
  int variant_count = 0;
  int generated_sample_count = 0;
  int rejected_sample_count = 0;
  int train_sample_count = 0;
  int validation_sample_count = 0;
  int holdout_sample_count = 0;
  std::filesystem::path dataset_manifest_path;
  std::filesystem::path report_json_path;
  std::filesystem::path report_markdown_path;
};

bool RunCxGeometryAugmentationDataset(
    const CxGeometryAugmentationDatasetOptions &options,
    CxGeometryAugmentationDatasetResult &result, std::string &reason);
