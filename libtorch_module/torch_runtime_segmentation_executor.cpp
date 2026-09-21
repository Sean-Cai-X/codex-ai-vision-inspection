#include "torch_runtime_segmentation_executor.h"
#include "torch_runtime_task_types.h"
#include "torch_runtime_manifest.h"
#include "torch_runtime_artifact_writer.h"
#include "torch_deeplabv3_plus.h"
#include <opencv2/imgcodecs.hpp>
#include <torch/torch.h>
#include <torch/csrc/jit/serialization/import.h>
#include <filesystem>
#include <sstream>

namespace
{

bool WriteSegmentationRequestArtifact(
    const std::filesystem::path& output_dir,
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request,
    const TorchModelManifest& manifest,
    std::string& reason)
{
    std::filesystem::path request_path = output_dir / "torch_segmentation_task_request.json";
    std::ostringstream request_os;
    request_os << "{";
    request_os << "\"schema\":\"cxvision.torch.segmentation.request.v1\",";
    request_os << "\"schema_version\":1,";
    request_os << "\"task\":" << QuoteTorchJsonString(request.task) << ",";
    request_os << "\"case_name\":" << QuoteTorchJsonString(request.case_name) << ",";
    request_os << "\"input_image\":" << QuoteTorchJsonString(request.input_image) << ",";
    request_os << "\"manifest_path\":" << QuoteTorchJsonString(request.manifest_path) << ",";
    request_os << "\"model_id\":" << QuoteTorchJsonString(manifest.model_id) << ",";
    request_os << "\"architecture\":" << QuoteTorchJsonString(manifest.architecture) << ",";
    request_os << "\"weights_format\":" << QuoteTorchJsonString(manifest.weights_format) << ",";
    request_os << "\"device\":" << QuoteTorchJsonString(config.device);
    request_os << "}";
    return WriteTorchTextArtifact(request_path, request_os.str(), reason);
}

bool WriteSegmentationFailureArtifacts(
    const std::filesystem::path& output_dir,
    const TorchTaskRequestCpp& request,
    const TorchModelManifest& manifest,
    const std::string& failure_stage,
    const std::string& failure_reason,
    TorchTaskResultCpp& result,
    std::string& reason)
{
    try
    {
        std::filesystem::create_directories(output_dir);

        std::filesystem::path result_path = output_dir / "torch_segmentation_task_result.json";
        std::ostringstream result_os;
        result_os << "{";
        result_os << "\"schema\":\"cxvision.torch.segmentation.result.v1\",";
        result_os << "\"schema_version\":1,";
        result_os << "\"status\":\"failed\",";
        result_os << "\"model_id\":" << QuoteTorchJsonString(manifest.model_id) << ",";
        result_os << "\"task\":" << QuoteTorchJsonString("segmentation") << ",";
        result_os << "\"architecture\":" << QuoteTorchJsonString(manifest.architecture) << ",";
        result_os << "\"weights_format\":" << QuoteTorchJsonString(manifest.weights_format) << ",";
        result_os << "\"failure_stage\":" << QuoteTorchJsonString(failure_stage) << ",";
        result_os << "\"reason\":" << QuoteTorchJsonString(failure_reason);
        result_os << "}";
        if (!WriteTorchTextArtifact(result_path, result_os.str(), reason))
            return false;

        std::filesystem::path evidence_path = output_dir / "torch_runtime_evidence.json";
        std::ostringstream evidence_os;
        evidence_os << "{";
        evidence_os << "\"schema\":\"cxvision.torch.evidence.v1\",";
        evidence_os << "\"schema_version\":1,";
        evidence_os << "\"task\":" << QuoteTorchJsonString("segmentation") << ",";
        evidence_os << "\"status\":\"failed\",";
        evidence_os << "\"failure_stage\":" << QuoteTorchJsonString(failure_stage) << ",";
        evidence_os << "\"reason\":" << QuoteTorchJsonString(failure_reason) << ",";
        evidence_os << "\"evidence_items\":[";
        evidence_os << "{\"role\":\"input_image\",\"path\":" << QuoteTorchJsonString(request.input_image) << "},";
        evidence_os << "{\"role\":\"manifest\",\"path\":" << QuoteTorchJsonString(request.manifest_path) << "},";
        evidence_os << "{\"role\":\"model\",\"path\":" << QuoteTorchJsonString(manifest.model_path.string()) << "}";
        evidence_os << "]";
        evidence_os << "}";
        if (!WriteTorchTextArtifact(evidence_path, evidence_os.str(), reason))
            return false;

        result.result_json = result_os.str();
        result.result_ref = result_path.string();
        result.evidence_ref = evidence_path.string();
        result.input_image_ref = request.input_image;
        reason.clear();
        return true;
    }
    catch (const std::exception& e)
    {
        reason = "failed to write segmentation failure artifacts: " + std::string(e.what());
        return false;
    }
}

bool IsTorchScriptSegmentationWeights(const std::string& weights_format)
{
    return weights_format == "cpp_archive" ||
           weights_format == "torchscript" ||
           weights_format == "jit_archive";
}

bool IsCppStateDictSegmentationWeights(const std::string& weights_format)
{
    return weights_format == "cpp_state_dict";
}

bool LoadAndPreprocessImage(
    const std::string& image_path,
    const TorchModelManifest& manifest,
    torch::Tensor& tensor,
    cv::Mat& original_image,
    std::string& reason)
{
    original_image = cv::imread(image_path);
    if (original_image.empty())
    {
        reason = "failed to load input image: " + image_path;
        return false;
    }

    cv::Mat resized;
    cv::resize(original_image, resized, cv::Size(manifest.input_width, manifest.input_height));

    cv::Mat rgb_img;
    cv::cvtColor(resized, rgb_img, cv::COLOR_BGR2RGB);

    cv::Mat float_img;
    rgb_img.convertTo(float_img, CV_32F);
    float_img *= manifest.input_scale;

    tensor = torch::from_blob(
        float_img.data,
        {1, manifest.input_height, manifest.input_width, 3},
        torch::kFloat32).clone();
    tensor = tensor.permute({0, 3, 1, 2});

    if (!manifest.mean.empty())
    {
        tensor = tensor - torch::tensor(manifest.mean).view({1, 3, 1, 1}).to(tensor.device());
    }
    if (!manifest.std.empty())
    {
        tensor = tensor / torch::tensor(manifest.std).view({1, 3, 1, 1}).to(tensor.device());
    }

    return true;
}

torch::Tensor RunSegmentationInference(
    std::shared_ptr<torch::jit::Module> jit_model,
    std::shared_ptr<torch::nn::Module> cpp_model,
    const torch::Tensor& input_tensor,
    const std::string& device,
    bool is_jit_model)
{
    torch::Device torch_device(device == "cuda" ? torch::kCUDA : torch::kCPU);

    auto input = input_tensor.to(torch_device);
    torch::NoGradGuard no_grad;

    torch::Tensor output;
    if (is_jit_model) {
        if (!jit_model) {
            throw std::runtime_error("JIT model is null");
        }
        jit_model->to(torch_device);
        jit_model->eval();
        
        std::vector<torch::jit::IValue> inputs;
        inputs.push_back(input);
        auto jit_output = jit_model->forward(inputs);
        if (jit_output.isTensor()) {
            output = jit_output.toTensor();
        } else if (jit_output.isGenericDict()) {
            auto dict = jit_output.toGenericDict();
            if (dict.find("out") != dict.end()) {
                output = dict.at("out").toTensor();
            } else {
                throw std::runtime_error("JIT model output has no 'out' tensor");
            }
        } else {
            throw std::runtime_error("Unknown JIT output type");
        }
    } else if (auto deeplab_model = std::dynamic_pointer_cast<DeepLabV3PlusImpl>(cpp_model)) {
        deeplab_model->to(torch_device);
        deeplab_model->eval();
        
        auto output_map = deeplab_model->forward(input);
        output = output_map["out"];
    } else {
        throw std::runtime_error("Unsupported model type for segmentation");
    }

    return output.to(torch::kCPU);
}

bool PostprocessAndWriteOutput(
    const torch::Tensor& output_tensor,
    const cv::Mat& original_image,
    const TorchModelManifest& manifest,
    const std::filesystem::path& output_dir,
    std::string& reason,
    std::string& result_ref,
    std::string& evidence_ref,
    std::string& primary_visual_ref)
{
    try
    {
        std::filesystem::create_directories(output_dir);

        auto mask = output_tensor.argmax(1).squeeze().to(torch::kInt32);
        cv::Mat mask_mat(mask.size(0), mask.size(1), CV_32S, mask.data_ptr<int32_t>());

        cv::Mat mask_labels_8u;
        mask_mat.convertTo(mask_labels_8u, CV_8U);

        cv::Mat mask_labels_resized;
        cv::resize(mask_labels_8u, mask_labels_resized, cv::Size(original_image.cols, original_image.rows), 0, 0, cv::INTER_NEAREST);

        cv::Mat mask_binary = (mask_labels_resized == manifest.target_class_id);
        mask_binary.convertTo(mask_binary, CV_8U, 255);

        cv::Mat mask_overlay = original_image.clone();
        cv::Mat mask_color;
        cv::applyColorMap(mask_binary, mask_color, cv::COLORMAP_JET);
        cv::addWeighted(mask_color, 0.5, mask_overlay, 0.5, 0, mask_overlay);

        std::vector<std::vector<cv::Point>> contours;
        std::vector<cv::Vec4i> hierarchy;
        cv::findContours(mask_binary, contours, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

        std::vector<std::vector<cv::Point>> filtered_contours;
        for (const auto& contour : contours)
        {
            double area = cv::contourArea(contour);
            if (area >= manifest.min_component_area)
            {
                filtered_contours.push_back(contour);
            }
        }

        int foreground_pixels = cv::countNonZero(mask_binary);
        double foreground_ratio = static_cast<double>(foreground_pixels) / (original_image.rows * original_image.cols);
        int changed_pixels = foreground_pixels;

        std::filesystem::path mask_labels_path = output_dir / "mask_labels.png";
        cv::imwrite(mask_labels_path.string(), mask_labels_resized);

        std::filesystem::path mask_binary_path = output_dir / "mask_binary.png";
        cv::imwrite(mask_binary_path.string(), mask_binary);

        std::filesystem::path mask_overlay_path = output_dir / "mask_overlay.png";
        cv::imwrite(mask_overlay_path.string(), mask_overlay);

        std::ostringstream contours_os;
        contours_os << "{";
        contours_os << "\"contour_count\":" << filtered_contours.size() << ",";
        contours_os << "\"contours\":[";
        for (size_t i = 0; i < filtered_contours.size(); ++i)
        {
            if (i > 0) contours_os << ",";
            contours_os << "{";
            contours_os << "\"area\":" << cv::contourArea(filtered_contours[i]) << ",";
            contours_os << "\"point_count\":" << filtered_contours[i].size() << ",";
            contours_os << "\"points\":[";
            for (size_t j = 0; j < filtered_contours[i].size(); ++j)
            {
                if (j > 0) contours_os << ",";
                contours_os << "[" << filtered_contours[i][j].x << "," << filtered_contours[i][j].y << "]";
            }
            contours_os << "]";
            contours_os << "}";
        }
        contours_os << "]";
        contours_os << "}";
        std::filesystem::path contours_path = output_dir / "contours.json";
        WriteTorchTextArtifact(contours_path, contours_os.str(), reason);

        std::ostringstream metrics_os;
        metrics_os << "{";
        metrics_os << "\"foreground_pixels\":" << foreground_pixels << ",";
        metrics_os << "\"foreground_ratio\":" << foreground_ratio << ",";
        metrics_os << "\"changed_pixels\":" << changed_pixels << ",";
        metrics_os << "\"contour_count\":" << filtered_contours.size() << ",";
        metrics_os << "\"target_class_id\":" << manifest.target_class_id << ",";
        metrics_os << "\"min_component_area\":" << manifest.min_component_area << ",";
        metrics_os << "\"original_width\":" << original_image.cols << ",";
        metrics_os << "\"original_height\":" << original_image.rows << ",";
        metrics_os << "\"output_width\":" << output_tensor.size(3) << ",";
        metrics_os << "\"output_height\":" << output_tensor.size(2);
        metrics_os << "}";
        std::filesystem::path metrics_path = output_dir / "segmentation_metrics.json";
        WriteTorchTextArtifact(metrics_path, metrics_os.str(), reason);

        std::filesystem::path result_path = output_dir / "torch_segmentation_task_result.json";
        std::ostringstream result_os;
        result_os << "{";
        result_os << "\"schema\":\"cxvision.torch.segmentation.result.v1\",";
        result_os << "\"schema_version\":1,";
        result_os << "\"status\":\"success\",";
        result_os << "\"model_id\":" << QuoteTorchJsonString(manifest.model_id) << ",";
        result_os << "\"task\":" << QuoteTorchJsonString("segmentation") << ",";
        result_os << "\"architecture\":" << QuoteTorchJsonString(manifest.architecture) << ",";
        result_os << "\"num_classes\":" << manifest.num_classes << ",";
        result_os << "\"target_class_id\":" << manifest.target_class_id << ",";
        result_os << "\"foreground_pixels\":" << foreground_pixels << ",";
        result_os << "\"foreground_ratio\":" << foreground_ratio << ",";
        result_os << "\"contour_count\":" << filtered_contours.size() << ",";
        result_os << "\"changed_pixels\":" << changed_pixels;
        result_os << "}";
        WriteTorchTextArtifact(result_path, result_os.str(), reason);
        result_ref = result_path.string();

        std::filesystem::path evidence_path = output_dir / "torch_runtime_evidence.json";
        std::ostringstream evidence_os;
        evidence_os << "{";
        evidence_os << "\"schema\":\"cxvision.torch.evidence.v1\",";
        evidence_os << "\"schema_version\":1,";
        evidence_os << "\"task\":" << QuoteTorchJsonString("segmentation") << ",";
        evidence_os << "\"evidence_items\":[";
        evidence_os << "{\"role\":\"input_image\",\"path\":" << QuoteTorchJsonString("input") << "},";
        evidence_os << "{\"role\":\"mask_labels\",\"path\":" << QuoteTorchJsonString("mask_labels.png") << "},";
        evidence_os << "{\"role\":\"mask_binary\",\"path\":" << QuoteTorchJsonString("mask_binary.png") << "},";
        evidence_os << "{\"role\":\"mask_overlay\",\"path\":" << QuoteTorchJsonString("mask_overlay.png") << "},";
        evidence_os << "{\"role\":\"contours\",\"path\":" << QuoteTorchJsonString("contours.json") << "},";
        evidence_os << "{\"role\":\"metrics\",\"path\":" << QuoteTorchJsonString("segmentation_metrics.json") << "}";
        evidence_os << "]";
        evidence_os << "}";
        WriteTorchTextArtifact(evidence_path, evidence_os.str(), reason);
        evidence_ref = evidence_path.string();

        primary_visual_ref = mask_overlay_path.string();

        return true;
    }
    catch (const std::exception& e)
    {
        reason = "postprocess exception: " + std::string(e.what());
        return false;
    }
}

}

TorchTaskResultCpp ExecuteTorchSegmentationTask(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request)
{
    TorchTaskResultCpp result;
    std::string reason;

    try
    {
        if (request.input_image.empty())
        {
            result.ok = false;
            result.error_code = static_cast<int>(TorchRuntimeErrorCode::InputImageMissing);
            result.status = "failed";
            result.error_message = "input image is required for segmentation";
            result.result_json =
                "{\"schema\":\"cxvision.torch.error.v1\","
                "\"failure_stage\":\"runtime_input_validation\","
                "\"reason\":\"input_image_missing\"}";
            return result;
        }

        if (request.manifest_path.empty())
        {
            result.ok = false;
            result.error_code = static_cast<int>(TorchRuntimeErrorCode::ManifestMissing);
            result.status = "failed";
            result.error_message = "manifest_path is required for segmentation";
            result.result_json =
                "{\"schema\":\"cxvision.torch.error.v1\","
                "\"failure_stage\":\"runtime_input_validation\","
                "\"reason\":\"manifest_missing\"}";
            return result;
        }

        TorchModelManifest manifest;
        if (!LoadTorchModelManifest(
            std::filesystem::path(request.manifest_path),
            std::filesystem::path(config.model_root),
            manifest,
            reason))
        {
            result.ok = false;
            result.error_code = static_cast<int>(TorchRuntimeErrorCode::ManifestInvalid);
            result.status = "failed";
            result.error_message = "failed to load manifest: " + reason;
            result.result_json =
                "{\"schema\":\"cxvision.torch.error.v1\","
                "\"failure_stage\":\"runtime_manifest_validation\","
                "\"reason\":\"" + reason + "\"}";
            return result;
        }

        std::filesystem::path output_dir = BuildTorchCaseDirectory(config, request, reason);
        if (output_dir.empty())
        {
            result.ok = false;
            result.error_code = static_cast<int>(TorchRuntimeErrorCode::OutputPathInvalid);
            result.status = "failed";
            result.error_message = "failed to create output directory: " + reason;
            result.result_json =
                "{\"schema\":\"cxvision.torch.error.v1\","
                "\"failure_stage\":\"runtime_output_path\","
                "\"reason\":\"" + EscapeTorchJsonString(reason) + "\"}";
            return result;
        }

        if (!WriteSegmentationRequestArtifact(output_dir, config, request, manifest, reason))
        {
            result.ok = false;
            result.error_code = static_cast<int>(TorchRuntimeErrorCode::ArtifactWriteFailed);
            result.status = "failed";
            result.error_message = "failed to write segmentation request artifact: " + reason;
            result.result_json =
                "{\"schema\":\"cxvision.torch.error.v1\","
                "\"failure_stage\":\"runtime_artifact_write\","
                "\"reason\":\"" + EscapeTorchJsonString(reason) + "\"}";
            return result;
        }

        if (!std::filesystem::exists(manifest.model_path))
        {
            result.ok = false;
            result.error_code = static_cast<int>(TorchRuntimeErrorCode::ModelNotFound);
            result.status = "failed";
            result.error_message = "model not found: " + manifest.model_path.string();
            WriteSegmentationFailureArtifacts(
                output_dir,
                request,
                manifest,
                "runtime_model_loading",
                "model_not_found",
                result,
                reason);
            return result;
        }

        std::shared_ptr<torch::jit::Module> jit_model = nullptr;
        std::shared_ptr<torch::nn::Module> cpp_model = nullptr;
        bool is_jit_model = false;
        
        if (IsTorchScriptSegmentationWeights(manifest.weights_format))
        {
            try
            {
                jit_model = std::make_shared<torch::jit::Module>(torch::jit::load(manifest.model_path.string()));
                is_jit_model = true;
            }
            catch (const c10::Error& e)
            {
                result.ok = false;
                result.error_code = static_cast<int>(TorchRuntimeErrorCode::ModelLoadFailed);
                result.status = "failed";
                result.error_message = "failed to load cpp_archive model: " + std::string(e.what());
                result.result_json =
                    "{\"schema\":\"cxvision.torch.error.v1\","
                    "\"failure_stage\":\"runtime_model_loading\","
                    "\"reason\":\"model_load_failed\"}";
                return result;
            }
        }
        else if (IsCppStateDictSegmentationWeights(manifest.weights_format))
        {
            if (manifest.architecture != "deeplabv3plus")
            {
                result.ok = false;
                result.error_code = static_cast<int>(TorchRuntimeErrorCode::WeightFormatUnsupported);
                result.status = "failed";
                result.error_message = "cpp_state_dict format not supported for architecture: " + manifest.architecture;
                WriteSegmentationFailureArtifacts(
                    output_dir,
                    request,
                    manifest,
                    "runtime_weight_load",
                    "unsupported_cpp_state_dict_architecture",
                    result,
                    reason);
                return result;
            }

            try
            {
                DeepLabV3Plus model(manifest.backbone, manifest.num_classes);
                torch::load(model, manifest.model_path.string());
                cpp_model = model.ptr();
                is_jit_model = false;
            }
            catch (const c10::Error& e)
            {
                result.ok = false;
                result.error_code = static_cast<int>(TorchRuntimeErrorCode::ModelLoadFailed);
                result.status = "failed";
                result.error_message = "failed to load cpp_state_dict deeplabv3plus model: " + std::string(e.what());
                WriteSegmentationFailureArtifacts(
                    output_dir,
                    request,
                    manifest,
                    "runtime_model_loading",
                    "model_load_failed",
                    result,
                    reason);
                return result;
            }
            catch (const std::exception& e)
            {
                result.ok = false;
                result.error_code = static_cast<int>(TorchRuntimeErrorCode::ModelLoadFailed);
                result.status = "failed";
                result.error_message = "failed to load cpp_state_dict deeplabv3plus model: " + std::string(e.what());
                WriteSegmentationFailureArtifacts(
                    output_dir,
                    request,
                    manifest,
                    "runtime_model_loading",
                    "model_load_failed",
                    result,
                    reason);
                return result;
            }
        }
        else if (manifest.weights_format == "python_state_dict")
        {
            if (manifest.architecture == "deeplabv3plus")
            {
                try
                {
                    result.ok = false;
                    result.error_code = static_cast<int>(TorchRuntimeErrorCode::WeightFormatUnsupported);
                    result.status = "failed";
                    result.error_message =
                        "python_state_dict requires conversion to a C++ loadable archive before segmentation inference";
                    WriteSegmentationFailureArtifacts(
                        output_dir,
                        request,
                        manifest,
                        "runtime_weight_load",
                        "python_state_dict_requires_cpp_archive_conversion",
                        result,
                        reason);
                    return result;
                }
                catch (const c10::Error& e)
                {
                    result.ok = false;
                    result.error_code = static_cast<int>(TorchRuntimeErrorCode::ModelLoadFailed);
                    result.status = "failed";
                    result.error_message = "failed to load deeplabv3plus python_state_dict: " + std::string(e.what());
                    WriteSegmentationFailureArtifacts(
                        output_dir,
                        request,
                        manifest,
                        "runtime_model_loading",
                        "model_load_failed",
                        result,
                        reason);
                    return result;
                }
            }
            else
            {
                result.ok = false;
                result.error_code = static_cast<int>(TorchRuntimeErrorCode::WeightFormatUnsupported);
                result.status = "failed";
                result.error_message = "python_state_dict format not supported for architecture: " + manifest.architecture;
                WriteSegmentationFailureArtifacts(
                    output_dir,
                    request,
                    manifest,
                    "runtime_weight_load",
                    "unsupported_weights_format",
                    result,
                    reason);
                return result;
            }
        }
        else
        {
            result.ok = false;
            result.error_code = static_cast<int>(TorchRuntimeErrorCode::WeightFormatUnsupported);
            result.status = "failed";
            result.error_message = "unsupported weights format: " + manifest.weights_format;
            WriteSegmentationFailureArtifacts(
                output_dir,
                request,
                manifest,
                "runtime_weight_load",
                "unsupported_weights_format",
                result,
                reason);
            return result;
        }

        cv::Mat original_image;
        torch::Tensor input_tensor;
        if (!LoadAndPreprocessImage(request.input_image, manifest, input_tensor, original_image, reason))
        {
            result.ok = false;
            result.error_code = static_cast<int>(TorchRuntimeErrorCode::InputImageInvalid);
            result.status = "failed";
            result.error_message = "failed to preprocess image: " + reason;
            result.result_json =
                "{\"schema\":\"cxvision.torch.error.v1\","
                "\"failure_stage\":\"runtime_preprocessing\","
                "\"reason\":\"" + reason + "\"}";
            return result;
        }

        std::string device_str = config.device.empty() ? "cpu" : config.device;
        auto start_time = std::chrono::steady_clock::now();
        torch::Tensor output_tensor = RunSegmentationInference(
            jit_model, cpp_model, input_tensor, device_str, is_jit_model);
        auto end_time = std::chrono::steady_clock::now();
        double elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

        std::string result_ref_str;
        std::string evidence_ref_str;
        std::string primary_visual_ref_str;
        if (!PostprocessAndWriteOutput(output_tensor, original_image, manifest, output_dir, reason, result_ref_str, evidence_ref_str, primary_visual_ref_str))
        {
            result.ok = false;
            result.error_code = static_cast<int>(TorchRuntimeErrorCode::ArtifactWriteFailed);
            result.status = "failed";
            result.error_message = "failed to write output: " + reason;
            result.result_json =
                "{\"schema\":\"cxvision.torch.error.v1\","
                "\"failure_stage\":\"runtime_artifact_write\","
                "\"reason\":\"" + reason + "\"}";
            return result;
        }

        result.ok = true;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::Ok);
        result.status = "success";
        result.requested_device = device_str;
        result.actual_device = "cpu";
        result.infer_runtime_ms = elapsed_ms;
        result.algorithm_runtime_ms = elapsed_ms;

        std::ostringstream json_os;
        json_os << "{";
        json_os << "\"schema\":\"cxvision.torch.segmentation.v1\",";
        json_os << "\"schema_version\":1,";
        json_os << "\"status\":\"success\",";
        json_os << "\"model_id\":" << QuoteTorchJsonString(manifest.model_id) << ",";
        json_os << "\"task\":" << QuoteTorchJsonString("segmentation") << ",";
        json_os << "\"architecture\":" << QuoteTorchJsonString(manifest.architecture) << ",";
        json_os << "\"num_classes\":" << manifest.num_classes << ",";
        json_os << "\"target_class_id\":" << manifest.target_class_id << ",";
        json_os << "\"infer_runtime_ms\":" << elapsed_ms << ",";
        json_os << "\"algorithm_runtime_ms\":" << elapsed_ms << ",";
        json_os << "\"output_dir\":" << QuoteTorchJsonString(output_dir.string());
        json_os << "}";
        result.result_json = json_os.str();

        result.result_ref = result_ref_str;
        result.evidence_ref = evidence_ref_str;
        result.primary_visual_ref = primary_visual_ref_str;
        result.input_image_ref = request.input_image;

    }
    catch (const c10::Error& e)
    {
        result.ok = false;
        result.error_code = static_cast<int>(TorchRuntimeErrorCode::TorchRuntimeError);
        result.status = "torch_exception";
        result.error_message = e.what();
        result.result_json =
            "{\"schema\":\"cxvision.torch.error.v1\","
            "\"failure_stage\":\"runtime_torch_execution\","
            "\"reason\":\"" + std::string(e.what()) + "\"}";
    }
    catch (const std::exception& e)
    {
        result.ok = false;
        result.error_code = -1;
        result.status = "exception";
        result.error_message = e.what();
        result.result_json =
            "{\"schema\":\"cxvision.torch.error.v1\","
            "\"failure_stage\":\"runtime_general_exception\","
            "\"reason\":\"" + std::string(e.what()) + "\"}";
    }
    catch (...)
    {
        result.ok = false;
        result.error_code = -2;
        result.status = "unknown_exception";
        result.error_message = "Unknown exception during segmentation";
        result.result_json =
            "{\"schema\":\"cxvision.torch.error.v1\","
            "\"failure_stage\":\"runtime_unknown_exception\"}";
    }

    return result;
}
