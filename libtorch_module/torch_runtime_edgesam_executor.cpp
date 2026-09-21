#include "torch_runtime_edgesam_executor.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <sstream>
#include <torch/cuda.h>
#include <torch/script.h>
#include <torch/torch.h>

namespace
{

std::string ReadText(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return std::string(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
}

std::string JsonString(const std::string& json, const std::string& key)
{
    const std::string marker = "\"" + key + "\"";
    std::size_t pos = json.find(marker);
    if (pos == std::string::npos)
        return {};
    pos = json.find(':', pos + marker.size());
    pos = json.find('"', pos == std::string::npos ? 0 : pos + 1);
    if (pos == std::string::npos)
        return {};
    const std::size_t end = json.find('"', pos + 1);
    return end == std::string::npos
        ? std::string()
        : json.substr(pos + 1, end - pos - 1);
}

double JsonNumber(
    const std::string& json,
    const std::string& key,
    double fallback)
{
    const std::string marker = "\"" + key + "\"";
    std::size_t pos = json.find(marker);
    if (pos == std::string::npos)
        return fallback;
    pos = json.find(':', pos + marker.size());
    if (pos == std::string::npos)
        return fallback;
    try
    {
        return std::stod(json.substr(pos + 1));
    }
    catch (...)
    {
        return fallback;
    }
}

std::string Quote(const std::string& value)
{
    std::ostringstream output;
    output << '"';
    for (const char ch : value)
    {
        if (ch == '\\' || ch == '"')
            output << '\\';
        output << ch;
    }
    output << '"';
    return output.str();
}

TorchTaskResultCpp Fail(const std::string& stage, const std::string& reason)
{
    TorchTaskResultCpp result;
    result.ok = false;
    result.status = "failed";
    result.error_code = -1;
    result.error_message = reason;
    result.result_json =
        "{\"schema\":\"cxvision.torch.edgesam.result.v1\","
        "\"status\":\"failed\",\"failure_stage\":" +
        Quote(stage) + ",\"reason\":" + Quote(reason) + "}";
    return result;
}

} // namespace

TorchTaskResultCpp ExecuteTorchEdgeSamTask(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request)
{
    try
    {
        if (request.manifest_path.empty())
            return Fail("manifest", "EdgeSAM manifest_path is empty");
        const std::filesystem::path manifest_path(request.manifest_path);
        const std::string manifest = ReadText(manifest_path);
        if (manifest.find("\"architecture\": \"edge_sam\"") ==
                std::string::npos &&
            manifest.find("\"architecture\":\"edge_sam\"") ==
                std::string::npos)
        {
            return Fail("manifest", "manifest architecture is not edge_sam");
        }
        const std::string encoder_relative =
            JsonString(manifest, "encoder_weights");
        const std::string decoder_relative =
            JsonString(manifest, "decoder_weights");
        const std::filesystem::path encoder_path =
            manifest_path.parent_path() / encoder_relative;
        const std::filesystem::path decoder_path =
            manifest_path.parent_path() / decoder_relative;
        if (!std::filesystem::exists(encoder_path) ||
            !std::filesystem::exists(decoder_path))
        {
            return Fail(
                "model_loading",
                "EdgeSAM TorchScript encoder or decoder is missing");
        }

        cv::Mat bgr = cv::imread(request.input_image, cv::IMREAD_COLOR);
        if (bgr.empty())
            return Fail("input", "input image is unreadable");
        const double scale =
            1024.0 / static_cast<double>(std::max(bgr.rows, bgr.cols));
        const int resized_width =
            std::max(1, static_cast<int>(std::lround(bgr.cols * scale)));
        const int resized_height =
            std::max(1, static_cast<int>(std::lround(bgr.rows * scale)));
        cv::Mat resized;
        cv::resize(
            bgr,
            resized,
            cv::Size(resized_width, resized_height),
            0.0,
            0.0,
            cv::INTER_LINEAR);
        cv::Mat rgb;
        cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);
        cv::Mat padded =
            cv::Mat::zeros(1024, 1024, CV_32FC3);
        cv::Mat rgb_float;
        rgb.convertTo(rgb_float, CV_32FC3);
        rgb_float.copyTo(
            padded(cv::Rect(0, 0, resized_width, resized_height)));
        torch::Tensor image = torch::from_blob(
            padded.data, {1, 1024, 1024, 3}, torch::kFloat32).clone();
        image = image.permute({0, 3, 1, 2});
        const torch::Tensor mean =
            torch::tensor({123.675f, 116.28f, 103.53f})
                .view({1, 3, 1, 1});
        const torch::Tensor std =
            torch::tensor({58.395f, 57.12f, 57.375f})
                .view({1, 3, 1, 1});
        image = (image - mean) / std;

        const std::string device_name =
            config.device == "cuda" && torch::cuda::is_available()
                ? "cuda"
                : "cpu";
        const torch::Device device(device_name);
        torch::jit::Module encoder =
            torch::jit::load(encoder_path.string(), device);
        torch::jit::Module decoder =
            torch::jit::load(decoder_path.string(), device);
        encoder.eval();
        decoder.eval();

        double consistency_max_abs = -1.0;
        const std::string consistency_embedding =
            JsonString(manifest, "embedding");
        const std::string consistency_coords =
            JsonString(manifest, "point_coords");
        const std::string consistency_labels =
            JsonString(manifest, "point_labels");
        const std::string consistency_scores =
            JsonString(manifest, "expected_scores");
        const std::string consistency_masks =
            JsonString(manifest, "expected_masks");
        if (!consistency_embedding.empty() &&
            !consistency_coords.empty() &&
            !consistency_labels.empty() &&
            !consistency_scores.empty() &&
            !consistency_masks.empty())
        {
            torch::Tensor reference_embedding;
            torch::Tensor reference_coords;
            torch::Tensor reference_labels;
            torch::Tensor expected_scores;
            torch::Tensor expected_masks;
            torch::load(
                reference_embedding,
                (manifest_path.parent_path() /
                    consistency_embedding).string());
            torch::load(
                reference_coords,
                (manifest_path.parent_path() /
                    consistency_coords).string());
            torch::load(
                reference_labels,
                (manifest_path.parent_path() /
                    consistency_labels).string());
            torch::load(
                expected_scores,
                (manifest_path.parent_path() /
                    consistency_scores).string());
            torch::load(
                expected_masks,
                (manifest_path.parent_path() /
                    consistency_masks).string());
            const auto consistency_output = decoder.forward({
                reference_embedding.to(device),
                reference_coords.to(device),
                reference_labels.to(device)}).toTuple();
            const torch::Tensor actual_scores =
                consistency_output->elements()[0].toTensor().cpu();
            const torch::Tensor actual_masks =
                consistency_output->elements()[1].toTensor().cpu();
            consistency_max_abs = std::max(
                (actual_scores - expected_scores.cpu())
                    .abs().max().item<double>(),
                (actual_masks - expected_masks.cpu())
                    .abs().max().item<double>());
            const double tolerance = JsonNumber(
                manifest, "max_abs_tolerance", 0.0001);
            if (consistency_max_abs > tolerance)
            {
                return Fail(
                    "python_cpp_consistency",
                    "EdgeSAM decoder consistency max abs exceeded tolerance");
            }
        }

        const double positive_x =
            JsonNumber(request.extra_json, "positive_x", bgr.cols * 0.5);
        const double positive_y =
            JsonNumber(request.extra_json, "positive_y", bgr.rows * 0.5);
        const double negative_x =
            JsonNumber(request.extra_json, "negative_x", 0.0);
        const double negative_y =
            JsonNumber(request.extra_json, "negative_y", 0.0);
        torch::Tensor coords = torch::tensor(
            {{{static_cast<float>(positive_x * scale),
               static_cast<float>(positive_y * scale)},
              {static_cast<float>(negative_x * scale),
               static_cast<float>(negative_y * scale)}}},
            torch::kFloat32).to(device);
        torch::Tensor labels =
            torch::tensor({{1.0f, 0.0f}}, torch::kFloat32).to(device);

        const auto start = std::chrono::steady_clock::now();
        torch::NoGradGuard no_grad;
        torch::Tensor embeddings =
            encoder.forward({image.to(device)}).toTensor();
        const auto decoder_output =
            decoder.forward({embeddings, coords, labels}).toTuple();
        torch::Tensor scores =
            decoder_output->elements()[0].toTensor();
        torch::Tensor masks =
            decoder_output->elements()[1].toTensor();
        const int64_t best = scores.flatten().argmax().item<int64_t>();
        torch::Tensor selected = masks.index({0, best}).unsqueeze(0).unsqueeze(0);
        selected = torch::nn::functional::interpolate(
            selected,
            torch::nn::functional::InterpolateFuncOptions()
                .size(std::vector<int64_t>{1024, 1024})
                .mode(torch::kBilinear)
                .align_corners(false));
        selected = selected.index({
            0, 0,
            torch::indexing::Slice(0, resized_height),
            torch::indexing::Slice(0, resized_width)});
        selected = torch::nn::functional::interpolate(
            selected.unsqueeze(0).unsqueeze(0),
            torch::nn::functional::InterpolateFuncOptions()
                .size(std::vector<int64_t>{bgr.rows, bgr.cols})
                .mode(torch::kBilinear)
                .align_corners(false)).squeeze();
        selected = selected.gt(0).to(torch::kUInt8).mul(255).cpu().contiguous();
        const auto end = std::chrono::steady_clock::now();

        cv::Mat mask(
            bgr.rows, bgr.cols, CV_8UC1, selected.data_ptr<unsigned char>());
        mask = mask.clone();
        std::filesystem::path output_dir(request.output_dir);
        std::filesystem::create_directories(output_dir);
        const std::filesystem::path mask_path =
            output_dir / "edgesam_mask.png";
        const std::filesystem::path overlay_path =
            output_dir / "edgesam_overlay.png";
        const std::filesystem::path result_path =
            output_dir / "edgesam_result.json";
        const std::filesystem::path evidence_path =
            output_dir / "edgesam_evidence.json";
        cv::imwrite(mask_path.string(), mask);
        cv::Mat overlay = bgr.clone();
        cv::Mat tint(bgr.size(), bgr.type(), cv::Scalar(0, 190, 0));
        tint.copyTo(overlay, mask);
        cv::addWeighted(bgr, 0.55, overlay, 0.45, 0.0, overlay);
        cv::imwrite(overlay_path.string(), overlay);
        const double elapsed_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                end - start).count();
        const double ratio =
            static_cast<double>(cv::countNonZero(mask)) /
            static_cast<double>(mask.rows * mask.cols);

        TorchTaskResultCpp result;
        result.ok = true;
        result.status = "success";
        result.requested_device = config.device;
        result.actual_device = device_name;
        result.infer_runtime_ms = elapsed_ms;
        result.algorithm_runtime_ms = elapsed_ms;
        result.input_image_ref = request.input_image;
        result.primary_visual_ref = overlay_path.string();
        result.visualization_refs =
            mask_path.string() + ";" + overlay_path.string();
        result.result_ref = result_path.string();
        result.evidence_ref = evidence_path.string();
        result.result_json =
            "{\"schema\":\"cxvision.torch.edgesam.result.v1\","
            "\"status\":\"success\",\"mask_available\":true,"
            "\"mask_ref\":" + Quote(mask_path.string()) +
            ",\"overlay_ref\":" + Quote(overlay_path.string()) +
            R"(,"foreground_ratio":)" + std::to_string(ratio) +
            R"(,"consistency_max_abs":)" +
            std::to_string(consistency_max_abs) +
            ",\"positive_hit\":" +
            std::string(mask.at<unsigned char>(
                std::clamp(static_cast<int>(positive_y), 0, mask.rows - 1),
                std::clamp(static_cast<int>(positive_x), 0, mask.cols - 1))
                    ? "true"
                    : "false") +
            ",\"negative_hit\":" +
            std::string(mask.at<unsigned char>(
                std::clamp(static_cast<int>(negative_y), 0, mask.rows - 1),
                std::clamp(static_cast<int>(negative_x), 0, mask.cols - 1))
                    ? "true"
                    : "false") +
            "}";
        std::ofstream(result_path) << result.result_json << "\n";
        std::ofstream(evidence_path)
            << "{\"schema\":\"cxvision.torch.edgesam.evidence.v1\","
            << "\"input_ref\":" << Quote(request.input_image) << ","
            << "\"manifest_ref\":" << Quote(request.manifest_path) << ","
            << "\"mask_ref\":" << Quote(mask_path.string()) << ","
            << "\"overlay_ref\":" << Quote(overlay_path.string()) << "}\n";
        return result;
    }
    catch (const std::exception& error)
    {
        return Fail("exception", error.what());
    }
}