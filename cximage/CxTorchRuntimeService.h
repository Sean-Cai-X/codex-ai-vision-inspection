#pragma once

#include "TorchRuntimeTypes.h"
#include "TorchRuntimeBridge.h"

#include <string>

struct CxTorchRuntimeConfig
{
    std::string model_root;
    std::string output_root;
    std::string device;
    std::string log_level;

    std::string runtime_dll_path;
};

struct CxTorchTaskRequest
{
    std::string task;
    std::string device;
    std::string input_image;
    std::string dataset_root;
    std::string manifest_path;
    std::string case_name;
    std::string extra_json;
    std::string output_dir;
};

struct CxTorchTaskResponse
{
    bool ok = false;
    int error_code = 0;

    std::string status;
    std::string error_message;

    std::string requested_device;
    std::string actual_device;

    double train_runtime_ms = 0.0;
    double infer_runtime_ms = 0.0;
    double algorithm_runtime_ms = 0.0;
    double placeholder_runtime_ms = 0.0;

    std::string result_json;
    std::string evidence_ref;
    std::string result_ref;

    std::string input_image_ref;
    std::string primary_visual_ref;
    std::string visualization_refs;

    std::string bbox_candidate_list_ref;
    std::string roi_crop_packet_ref;
    std::string attach_back_ref;
    std::string template_alignment_ref;
    std::string roi_diff_candidate_ref;

    std::string trainer_lifecycle_summary;
    std::string unified_mainline_summary;
};

class CxTorchRuntimeService
{
public:
    CxTorchRuntimeService();
    ~CxTorchRuntimeService();

    CxTorchRuntimeService(const CxTorchRuntimeService&) = delete;
    CxTorchRuntimeService& operator=(const CxTorchRuntimeService&) = delete;

    bool Initialize(const CxTorchRuntimeConfig& config, std::string& reason);
    bool Execute(const CxTorchTaskRequest& request, CxTorchTaskResponse& response, std::string& reason);
    void Shutdown();

    bool IsReady() const noexcept;
    std::string RuntimeVersion() const;

private:
    TorchRuntimeBridge bridge_;
    bool initialized_ = false;
};
