#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct TorchModelManifest
{
    std::string schema;
    int schema_version = 0;

    std::string model_id;
    std::string model_name;
    std::string model_version;
    std::string task;
    std::string architecture;
    std::string backbone;
    std::string variant;

    std::string weights;
    std::string weights_format;

    int num_classes = 0;
    std::vector<std::string> class_names;

    int input_width = 0;
    int input_height = 0;
    std::string input_color;
    double input_scale = 0.0;
    std::vector<double> mean;
    std::vector<double> std;
    bool letterbox = false;

    int target_class_id = -1;
    int min_component_area = 20;

    float confidence_threshold = 0.25f;
    float iou_threshold = 0.45f;
    int max_detections = 100;
    bool class_agnostic_nms = false;
    int mask_channels = 0;
    int prototype_channels = 0;
    int configured_prototype_channels = 0;
    float mask_threshold = 0.5f;
    std::string weights_hash;

    std::filesystem::path manifest_directory;
    std::filesystem::path weights_path;
    std::filesystem::path model_path;
};

bool LoadTorchModelManifest(
    const std::filesystem::path& manifest_path,
    const std::filesystem::path& model_root,
    TorchModelManifest& manifest,
    std::string& reason);

bool ValidateTorchModelManifest(
    const TorchModelManifest& manifest,
    std::string& reason);

bool ValidateSegmentationManifest(
    const TorchModelManifest& manifest,
    std::string& reason);

bool ValidateDetectionManifest(
    const TorchModelManifest& manifest,
    std::string& reason);

bool ValidateInstanceSegmentationManifest(
    const TorchModelManifest& manifest,
    std::string& reason);
