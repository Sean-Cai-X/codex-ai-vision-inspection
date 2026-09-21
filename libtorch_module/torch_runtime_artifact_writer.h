#pragma once

#include "torch_runtime_core.h"
#include <filesystem>
#include <string>
#include <opencv2/opencv.hpp>

bool WriteTorchTextArtifact(
    const std::filesystem::path& path,
    const std::string& content,
    std::string& reason);

bool WriteTorchImageArtifact(
    const std::filesystem::path& path,
    const cv::Mat& image,
    std::string& reason);

std::filesystem::path BuildTorchCaseDirectory(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request,
    std::string& reason);

std::string EscapeTorchJsonString(const std::string& value);

std::string QuoteTorchJsonString(const std::string& value);