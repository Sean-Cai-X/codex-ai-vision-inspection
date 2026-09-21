#include "torch_runtime_task_dispatcher.h"
#include "torch_runtime_task_types.h"
#include "torch_runtime_manifest.h"
#include "torch_runtime_artifact_writer.h"
#include <opencv2/imgcodecs.hpp>
#include <sstream>

namespace
{

bool CheckInputImage(
    const std::string& input_image_path,
    std::string& reason)
{
    if (input_image_path.empty())
    {
        reason = "input_image is empty";
        return false;
    }

    if (!std::filesystem::exists(input_image_path))
    {
        reason = "input image not found: " + input_image_path;
        return false;
    }

    cv::Mat image = cv::imread(input_image_path);
    if (image.empty())
    {
        reason = "input image is unreadable or empty";
        return false;
    }

    return true;
}

bool CheckDevice(
    const std::string& device,
    std::string& reason)
{
    if (device.empty() || device == "auto" || device == "cpu" || device == "gpu")
        return true;

    reason = "invalid device: " + device;
    return false;
}

bool CheckOutputDirectory(
    const std::filesystem::path& output_dir,
    std::string& reason)
{
    try
    {
        std::filesystem::create_directories(output_dir);
        return true;
    }
    catch (const std::exception& e)
    {
        reason = "failed to create output directory: " + std::string(e.what());
        return false;
    }
}

bool CheckWeightsFormatSupported(
    const std::string& weights_format,
    std::string& reason)
{
    if (weights_format == "cpp_archive" ||
        weights_format == "torchscript" ||
        weights_format == "jit_archive" ||
        weights_format == "cpp_state_dict" ||
        weights_format == "python_state_dict")
        return true;

    reason = "unsupported weights_format: " + weights_format;
    return false;
}

std::string BuildContractJson(
    const std::string& task,
    const std::string& status,
    bool input_readable,
    bool manifest_valid,
    bool weights_available,
    bool device_valid,
    const std::string& reason,
    bool forward_executed = false,
    const std::string& failure_stage = "")
{
    std::ostringstream os;
    os << "{";
    os << "\"schema\":\"cxvision.torch.contract.v1\",";
    os << "\"schema_version\":1,";
    os << "\"task\":" << QuoteTorchJsonString(task) << ",";
    os << "\"status\":" << QuoteTorchJsonString(status) << ",";
    os << "\"input_readable\":" << (input_readable ? "true" : "false") << ",";
    os << "\"manifest_valid\":" << (manifest_valid ? "true" : "false") << ",";
    os << "\"weights_available\":" << (weights_available ? "true" : "false") << ",";
    os << "\"device_valid\":" << (device_valid ? "true" : "false") << ",";
    os << "\"forward_executed\":" << (forward_executed ? "true" : "false");
    if (!failure_stage.empty())
    {
        os << ",\"failure_stage\":" << QuoteTorchJsonString(failure_stage);
    }
    if (!reason.empty())
    {
        os << ",\"reason\":" << QuoteTorchJsonString(reason);
    }
    os << "}";
    return os.str();
}

}

TorchTaskResultCpp RunTorchCapabilitiesTask(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request)
{
    TorchTaskResultCpp result;

    result.ok = true;
    result.status = "success";
    result.requested_device = config.device.empty() ? "auto" : config.device;
    result.actual_device = "cpu";

    std::ostringstream json_os;
    json_os << "{";
    json_os << "\"schema\":" << QuoteTorchJsonString("cxvision.torch.capabilities.v1") << ",";
    json_os << "\"schema_version\":1,";
    json_os << "\"status\":" << QuoteTorchJsonString("success") << ",";
    json_os << "\"supported_tasks\":[";
    json_os << QuoteTorchJsonString(TorchRuntimeTaskIds::Capabilities) << ",";
    json_os << QuoteTorchJsonString(TorchRuntimeTaskIds::SegmentationContract) << ",";
    json_os << QuoteTorchJsonString(TorchRuntimeTaskIds::DetectionContract) << ",";
    json_os << QuoteTorchJsonString(TorchRuntimeTaskIds::DeepLabV3PlusSegmentation) << ",";
    json_os << QuoteTorchJsonString(TorchRuntimeTaskIds::YoloV8Detection);
    json_os << "],";
    json_os << "\"device\":" << QuoteTorchJsonString("cpu");
    json_os << "}";
    result.result_json = json_os.str();

    std::string reason;
    std::filesystem::path output_dir = BuildTorchCaseDirectory(config, request, reason);
    if (!output_dir.empty())
    {
        std::filesystem::path capabilities_path = output_dir / "torch_runtime_capabilities.json";
        WriteTorchTextArtifact(capabilities_path, result.result_json, reason);
        result.result_ref = capabilities_path.string();
    }

    return result;
}

TorchTaskResultCpp ValidateTorchSegmentationContract(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request)
{
    TorchTaskResultCpp result;

    result.requested_device = config.device.empty() ? "auto" : config.device;
    result.actual_device = "cpu";

    std::string reason;

    if (request.task.empty())
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::InvalidRequest);
        result.status = "failed";
        result.error_message = "task is empty";
        result.result_json = BuildContractJson("segmentation", "invalid", false, false, false, true, "task is empty", false, "runtime_request_validation");
        return result;
    }

    if (!CheckInputImage(request.input_image, reason))
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::InputImageMissing);
        result.status = "failed";
        result.error_message = reason;
        result.result_json = BuildContractJson("segmentation", "invalid", false, false, false, true, reason, false, "runtime_input_open");
        return result;
    }

    cv::Mat image = cv::imread(request.input_image);
    if (image.empty())
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::InputImageUnreadable);
        result.status = "failed";
        result.error_message = "input image is unreadable";
        result.result_json = BuildContractJson("segmentation", "invalid", false, false, false, true, "input image is unreadable", false, "runtime_input_open");
        return result;
    }

    if (request.manifest_path.empty())
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::ManifestMissing);
        result.status = "failed";
        result.error_message = "manifest_path is empty";
        result.result_json = BuildContractJson("segmentation", "invalid", true, false, false, true, "manifest_path is empty", false, "runtime_manifest_parse");
        return result;
    }

    TorchModelManifest manifest;
    if (!LoadTorchModelManifest(request.manifest_path, config.model_root, manifest, reason))
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::ManifestInvalid);
        result.status = "failed";
        result.error_message = reason;
        result.result_json = BuildContractJson("segmentation", "invalid", true, false, false, true, reason, false, "runtime_manifest_parse");
        return result;
    }

    if (manifest.schema != "cxvision.torch_model_manifest")
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::ManifestSchemaMismatch);
        result.status = "failed";
        result.error_message = "invalid manifest schema";
        result.result_json = BuildContractJson("segmentation", "invalid", true, false, false, true, "invalid manifest schema: " + manifest.schema, false, "runtime_manifest_parse");
        return result;
    }

    if (manifest.schema_version != 1)
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::ManifestSchemaMismatch);
        result.status = "failed";
        result.error_message = "unsupported schema version";
        result.result_json = BuildContractJson("segmentation", "invalid", true, false, false, true, "unsupported schema version: " + std::to_string(manifest.schema_version), false, "runtime_manifest_parse");
        return result;
    }

    if (manifest.task != "segmentation")
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::ManifestSchemaMismatch);
        result.status = "failed";
        result.error_message = "task must be 'segmentation'";
        result.result_json = BuildContractJson("segmentation", "invalid", true, false, false, true, "task must be 'segmentation', got: " + manifest.task, false, "runtime_manifest_parse");
        return result;
    }

    if (manifest.architecture != "deeplabv3plus")
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::ManifestSchemaMismatch);
        result.status = "failed";
        result.error_message = "architecture must be 'deeplabv3plus'";
        result.result_json = BuildContractJson("segmentation", "invalid", true, false, false, true, "architecture must be 'deeplabv3plus', got: " + manifest.architecture, false, "runtime_manifest_parse");
        return result;
    }

    if (manifest.backbone.empty())
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::ManifestSchemaMismatch);
        result.status = "failed";
        result.error_message = "backbone is required";
        result.result_json = BuildContractJson("segmentation", "invalid", true, false, false, true, "backbone is required", false, "runtime_manifest_parse");
        return result;
    }

    if (manifest.weights.empty())
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::ModelFileMissing);
        result.status = "failed";
        result.error_message = "weights path is empty";
        result.result_json = BuildContractJson("segmentation", "invalid", true, false, false, true, "weights path is empty", false, "runtime_model_resolve");
        return result;
    }

    if (!std::filesystem::exists(manifest.weights_path))
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::ModelFileMissing);
        result.status = "failed";
        result.error_message = "weights file not found";
        result.result_json = BuildContractJson("segmentation", "invalid", true, true, false, true, "weights file not found: " + manifest.weights_path.string(), false, "runtime_model_resolve");

        std::filesystem::path output_dir = BuildTorchCaseDirectory(config, request, reason);
        if (!output_dir.empty())
        {
            std::filesystem::path contract_path = output_dir / "torch_segmentation_contract.json";
            WriteTorchTextArtifact(contract_path, result.result_json, reason);
            result.result_ref = contract_path.string();
        }

        return result;
    }

    if (!CheckWeightsFormatSupported(manifest.weights_format, reason))
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::WeightFormatUnsupported);
        result.status = "failed";
        result.error_message = reason;
        result.result_json = BuildContractJson("segmentation", "invalid", true, true, true, true, reason, false, "runtime_weight_load");
        return result;
    }

    if (manifest.num_classes < 2)
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::ManifestSchemaMismatch);
        result.status = "failed";
        result.error_message = "num_classes must be >= 2";
        result.result_json = BuildContractJson("segmentation", "invalid", true, true, true, true, "num_classes must be >= 2, got: " + std::to_string(manifest.num_classes), false, "runtime_manifest_parse");
        return result;
    }

    if (manifest.input_width <= 0 || manifest.input_height <= 0)
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::ManifestSchemaMismatch);
        result.status = "failed";
        result.error_message = "input width/height must be > 0";
        result.result_json = BuildContractJson("segmentation", "invalid", true, true, true, true, "input width=" + std::to_string(manifest.input_width) + ", height=" + std::to_string(manifest.input_height), false, "runtime_manifest_parse");
        return result;
    }

    if (manifest.target_class_id < 0 || manifest.target_class_id >= manifest.num_classes)
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::ManifestSchemaMismatch);
        result.status = "failed";
        result.error_message = "target_class_id out of range";
        result.result_json = BuildContractJson("segmentation", "invalid", true, true, true, true, "target_class_id=" + std::to_string(manifest.target_class_id) + ", num_classes=" + std::to_string(manifest.num_classes), false, "runtime_manifest_parse");
        return result;
    }

    if (!CheckDevice(config.device, reason))
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::DeviceUnavailable);
        result.status = "failed";
        result.error_message = reason;
        result.result_json = BuildContractJson("segmentation", "invalid", true, true, true, false, reason, false, "runtime_device_resolve");
        return result;
    }

    std::filesystem::path output_dir = BuildTorchCaseDirectory(config, request, reason);
    if (output_dir.empty())
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::ArtifactWriteFailed);
        result.status = "failed";
        result.error_message = reason;
        result.result_json = BuildContractJson("segmentation", "invalid", true, true, true, true, reason, false, "runtime_artifact_write");
        return result;
    }

    result.ok = true;
    result.status = "success";
    result.result_json = BuildContractJson("segmentation", "valid", true, true, true, true, "", false, "");

    std::filesystem::path contract_path = output_dir / "torch_segmentation_contract.json";
    WriteTorchTextArtifact(contract_path, result.result_json, reason);
    result.result_ref = contract_path.string();

    return result;
}

TorchTaskResultCpp ValidateTorchDetectionContract(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request)
{
    TorchTaskResultCpp result;

    result.requested_device = config.device.empty() ? "auto" : config.device;
    result.actual_device = "cpu";

    std::string reason;

    if (request.task.empty())
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::InvalidRequest);
        result.status = "failed";
        result.error_message = "task is empty";
        result.result_json = BuildContractJson("detection", "invalid", false, false, false, true, "task is empty", false, "runtime_request_validation");
        return result;
    }

    if (!CheckInputImage(request.input_image, reason))
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::InputImageMissing);
        result.status = "failed";
        result.error_message = reason;
        result.result_json = BuildContractJson("detection", "invalid", false, false, false, true, reason, false, "runtime_input_open");
        return result;
    }

    cv::Mat image = cv::imread(request.input_image);
    if (image.empty())
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::InputImageUnreadable);
        result.status = "failed";
        result.error_message = "input image is unreadable";
        result.result_json = BuildContractJson("detection", "invalid", false, false, false, true, "input image is unreadable", false, "runtime_input_open");
        return result;
    }

    if (request.manifest_path.empty())
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::ManifestMissing);
        result.status = "failed";
        result.error_message = "manifest_path is empty";
        result.result_json = BuildContractJson("detection", "invalid", true, false, false, true, "manifest_path is empty", false, "runtime_manifest_parse");
        return result;
    }

    TorchModelManifest manifest;
    if (!LoadTorchModelManifest(request.manifest_path, config.model_root, manifest, reason))
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::ManifestInvalid);
        result.status = "failed";
        result.error_message = reason;
        result.result_json = BuildContractJson("detection", "invalid", true, false, false, true, reason, false, "runtime_manifest_parse");
        return result;
    }

    if (manifest.schema != "cxvision.torch_model_manifest")
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::ManifestSchemaMismatch);
        result.status = "failed";
        result.error_message = "invalid manifest schema";
        result.result_json = BuildContractJson("detection", "invalid", true, false, false, true, "invalid manifest schema: " + manifest.schema, false, "runtime_manifest_parse");
        return result;
    }

    if (manifest.schema_version != 1)
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::ManifestSchemaMismatch);
        result.status = "failed";
        result.error_message = "unsupported schema version";
        result.result_json = BuildContractJson("detection", "invalid", true, false, false, true, "unsupported schema version: " + std::to_string(manifest.schema_version), false, "runtime_manifest_parse");
        return result;
    }

    if (manifest.task != "detection")
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::ManifestSchemaMismatch);
        result.status = "failed";
        result.error_message = "task must be 'detection'";
        result.result_json = BuildContractJson("detection", "invalid", true, false, false, true, "task must be 'detection', got: " + manifest.task, false, "runtime_manifest_parse");
        return result;
    }

    if (manifest.architecture != "yolov8")
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::ManifestSchemaMismatch);
        result.status = "failed";
        result.error_message = "architecture must be 'yolov8'";
        result.result_json = BuildContractJson("detection", "invalid", true, false, false, true, "architecture must be 'yolov8', got: " + manifest.architecture, false, "runtime_manifest_parse");
        return result;
    }

    std::vector<std::string> valid_variants = {"nano", "small", "medium", "large", "xlarge"};
    bool valid_variant = false;
    for (const auto& v : valid_variants)
    {
        if (manifest.variant == v)
        {
            valid_variant = true;
            break;
        }
    }
    if (!valid_variant)
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::ManifestSchemaMismatch);
        result.status = "failed";
        result.error_message = "invalid variant";
        result.result_json = BuildContractJson("detection", "invalid", true, false, false, true, "invalid variant: " + manifest.variant, false, "runtime_manifest_parse");
        return result;
    }

    if (!manifest.class_names.empty() &&
        static_cast<int>(manifest.class_names.size()) != manifest.num_classes)
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::ManifestSchemaMismatch);
        result.status = "failed";
        result.error_message = "class_names count mismatch";
        result.result_json = BuildContractJson("detection", "invalid", true, false, false, true, "class_names count=" + std::to_string(manifest.class_names.size()) + ", num_classes=" + std::to_string(manifest.num_classes), false, "runtime_manifest_parse");
        return result;
    }

    if (manifest.confidence_threshold < 0.0f || manifest.confidence_threshold > 1.0f)
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::ManifestSchemaMismatch);
        result.status = "failed";
        result.error_message = "confidence_threshold out of range";
        result.result_json = BuildContractJson("detection", "invalid", true, false, false, true, "confidence_threshold=" + std::to_string(manifest.confidence_threshold), false, "runtime_manifest_parse");
        return result;
    }

    if (manifest.iou_threshold < 0.0f || manifest.iou_threshold > 1.0f)
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::ManifestSchemaMismatch);
        result.status = "failed";
        result.error_message = "iou_threshold out of range";
        result.result_json = BuildContractJson("detection", "invalid", true, false, false, true, "iou_threshold=" + std::to_string(manifest.iou_threshold), false, "runtime_manifest_parse");
        return result;
    }

    if (manifest.max_detections <= 0)
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::ManifestSchemaMismatch);
        result.status = "failed";
        result.error_message = "max_detections must be > 0";
        result.result_json = BuildContractJson("detection", "invalid", true, false, false, true, "max_detections=" + std::to_string(manifest.max_detections), false, "runtime_manifest_parse");
        return result;
    }

    if (manifest.weights.empty())
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::ModelFileMissing);
        result.status = "failed";
        result.error_message = "weights path is empty";
        result.result_json = BuildContractJson("detection", "invalid", true, false, false, true, "weights path is empty", false, "runtime_model_resolve");
        return result;
    }

    if (!std::filesystem::exists(manifest.weights_path))
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::ModelFileMissing);
        result.status = "failed";
        result.error_message = "weights file not found";
        result.result_json = BuildContractJson("detection", "invalid", true, true, false, true, "weights file not found: " + manifest.weights_path.string(), false, "runtime_model_resolve");

        std::filesystem::path output_dir = BuildTorchCaseDirectory(config, request, reason);
        if (!output_dir.empty())
        {
            std::filesystem::path contract_path = output_dir / "torch_detection_contract.json";
            WriteTorchTextArtifact(contract_path, result.result_json, reason);
            result.result_ref = contract_path.string();
        }

        return result;
    }

    if (!CheckWeightsFormatSupported(manifest.weights_format, reason))
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::WeightFormatUnsupported);
        result.status = "failed";
        result.error_message = reason;
        result.result_json = BuildContractJson("detection", "invalid", true, true, true, true, reason, false, "runtime_weight_load");
        return result;
    }

    if (!CheckDevice(config.device, reason))
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::DeviceUnavailable);
        result.status = "failed";
        result.error_message = reason;
        result.result_json = BuildContractJson("detection", "invalid", true, true, true, false, reason, false, "runtime_device_resolve");
        return result;
    }

    std::filesystem::path output_dir = BuildTorchCaseDirectory(config, request, reason);
    if (output_dir.empty())
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::ArtifactWriteFailed);
        result.status = "failed";
        result.error_message = reason;
        result.result_json = BuildContractJson("detection", "invalid", true, true, true, true, reason, false, "runtime_artifact_write");
        return result;
    }

    result.ok = true;
    result.status = "success";
    result.result_json = BuildContractJson("detection", "valid", true, true, true, true, "", false, "");

    std::filesystem::path contract_path = output_dir / "torch_detection_contract.json";
    WriteTorchTextArtifact(contract_path, result.result_json, reason);
    result.result_ref = contract_path.string();

    return result;
}
