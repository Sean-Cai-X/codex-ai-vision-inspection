#ifndef TORCH_ALLTEST_2_H
#define TORCH_ALLTEST_2_H

#include <torch/torch.h>
#include <torch/nn/functional/upsampling.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <opencv2/opencv.hpp>
#include <sstream>
#include <string>
#include <vector>

#include "torch_detect.h"
#include "torch_mobilevitv2.h"
#include "torch_mobilevit_mainline_bridge.h"
#include "torch_mobilevitv2_dataset.h"
#include "torch_modelconfig.h"
#include "torch_segmentation_mainline_bridge.h"
#include "torch_v8.h"

namespace fs = std::filesystem;

#ifndef TORCH_FULL_ENABLE_MOBILEVIT_LOAD
#define TORCH_FULL_ENABLE_MOBILEVIT_LOAD 1
#endif

#ifndef TORCH_FULL_ENABLE_MOBILEVIT_TRAIN
#define TORCH_FULL_ENABLE_MOBILEVIT_TRAIN 1
#endif

#ifndef TORCH_FULL_ENABLE_MOBILEVIT_DATASET
#define TORCH_FULL_ENABLE_MOBILEVIT_DATASET 1
#endif

#ifndef TORCH_FULL_ENABLE_DATASET_STAGE
#define TORCH_FULL_ENABLE_DATASET_STAGE 1
#endif

#ifndef TORCH_FULL_ENABLE_MOBILEVIT_FULLTRAIN
#define TORCH_FULL_ENABLE_MOBILEVIT_FULLTRAIN 0
#endif

#ifndef TORCH_FULL_ENABLE_TWOSTAGE_INFER
#define TORCH_FULL_ENABLE_TWOSTAGE_INFER 1
#endif

#ifndef TORCH_FULL_ENABLE_TWOSTAGE_TRAIN
#define TORCH_FULL_ENABLE_TWOSTAGE_TRAIN 1
#endif

#ifndef TORCH_FULL_ENABLE_OCC_STAGE
#define TORCH_FULL_ENABLE_OCC_STAGE 0
#endif


inline torch::Device get_mobilevit_test_device() {
    if (const char* use_cuda = std::getenv("LIBTORCH_MODULE_USE_CUDA")) {
        if (std::string(use_cuda) == "0") {
            return torch::Device(torch::kCPU);
        }
        if (std::string(use_cuda) == "1" && torch::cuda::is_available()) {
            return torch::Device(torch::kCUDA);
        }
    }
    return torch::cuda::is_available() ? torch::Device(torch::kCUDA) : torch::Device(torch::kCPU);
}

inline std::string torch_device_kind_name(const torch::Device& device) {
    return device.is_cuda() ? "gpu" : "cpu";
}

inline std::string resolve_env_or_default(const char* env_name, const char* fallback) {
    const char* value = std::getenv(env_name);
    if (value != nullptr && value[0] != '\0') {
        const std::string resolved = value;
        std::error_code ec;
        if (fs::is_directory(fs::path(resolved), ec)) {
            return (fs::path(resolved) / fallback).string();
        }
        return resolved;
    }
    return std::string(fallback);
}

inline std::string resolve_mobilevit_weight_path() {
    return resolve_env_or_default("LIBTORCH_MODULE_MOBILEVIT_WEIGHTS", "mobilevitv2_weights.pt");
}

inline bool has_zip_file_signature(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return false;
    }

    unsigned char signature[4] = {0, 0, 0, 0};
    input.read(reinterpret_cast<char*>(signature), sizeof(signature));
    if (input.gcount() < 4) {
        return false;
    }

    return signature[0] == 0x50 && signature[1] == 0x4B &&
        (signature[2] == 0x03 || signature[2] == 0x05 || signature[2] == 0x07) &&
        (signature[3] == 0x04 || signature[3] == 0x06 || signature[3] == 0x08);
}

inline bool try_load_mobilevit_external_weights(MobileViTv2& model,
                                                const std::string& weight_path,
                                                std::string& status) {
    if (weight_path.empty()) {
        status = "weight_path_empty";
        std::cout << "[MobileViTv2] Skip external weights: empty path" << std::endl;
        return false;
    }

    if (!fs::exists(weight_path)) {
        status = "weight_file_missing";
        std::cout << "[MobileViTv2] Skip external weights: missing file " << weight_path << std::endl;
        return false;
    }

    try {
        if (has_zip_file_signature(weight_path)) {
            std::cout << "[MobileViTv2] Zip-container signature detected, attempting GenericDict pickle_load: "
                      << weight_path << std::endl;
        }
        model->load_weights(weight_path);
        status = "weight_loaded";
        std::cout << "[MobileViTv2] External weights loaded from " << weight_path << std::endl;
        return true;
    }
    catch (const std::exception& e) {
        status = "weight_load_failed";
        std::cout << "[MobileViTv2] Skip external weights after load failure: "
                  << weight_path << " error=" << e.what() << std::endl;
        return false;
    }
}

inline std::string resolve_mobilevit_missing_dataset_probe_path() {
    return resolve_env_or_default("LIBTORCH_MODULE_MOBILEVIT_MISSING_DATASET",
                                  "D:/datasets/mobilevitv2_missing");
}

inline std::string resolve_mobilevit_train_dataset_path() {
    return resolve_env_or_default("LIBTORCH_MODULE_MOBILEVIT_DATASET",
                                  "D:/datasets/imagenet_small");
}

inline int64_t count_imagefolder_classes(const std::string& root_dir) {
    if (!fs::exists(root_dir)) {
        return 0;
    }

    int64_t class_count = 0;
    for (const auto& entry : fs::directory_iterator(root_dir)) {
        if (entry.is_directory()) {
            ++class_count;
        }
    }
    return class_count;
}

inline int64_t resolve_mobilevit_env_int(const char* env_name, int64_t fallback) {
    const char* value = std::getenv(env_name);
    if (value == nullptr || value[0] == '\0') {
        return fallback;
    }

    try {
        return std::stoll(value);
    }
    catch (...) {
        return fallback;
    }
}

inline int64_t resolve_mobilevit_roi_input_size() {
    return resolve_mobilevit_env_int("LIBTORCH_MODULE_MOBILEVIT_ROI_SIZE", 256);
}

inline int64_t resolve_twostage_image_size() {
    return resolve_mobilevit_env_int("LIBTORCH_MODULE_TWOSTAGE_IMAGE_SIZE", 640);
}

inline int64_t resolve_twostage_infer_num_classes() {
    return resolve_mobilevit_env_int("LIBTORCH_MODULE_TWOSTAGE_INFER_CLASSES", 5);
}

inline int64_t resolve_twostage_train_num_classes() {
    return resolve_mobilevit_env_int("LIBTORCH_MODULE_TWOSTAGE_TRAIN_CLASSES", 4);
}

inline int64_t resolve_twostage_train_batch_size() {
    return resolve_mobilevit_env_int("LIBTORCH_MODULE_TWOSTAGE_TRAIN_BATCH", 2);
}

inline std::string resolve_twostage_input_image_path() {
    return resolve_env_or_default("LIBTORCH_MODULE_YOLO_INFER_IMAGE", "");
}

inline std::string resolve_twostage_output_image_path() {
    return resolve_env_or_default("LIBTORCH_MODULE_YOLO_INFER_OUTPUT", "");
}

inline std::string resolve_mobilevit_unified_input_image_path() {
    return resolve_env_or_default("LIBTORCH_MODULE_MOBILEVIT_INFER_IMAGE", "");
}

inline std::string resolve_mobilevit_unified_output_image_path() {
    return resolve_env_or_default("LIBTORCH_MODULE_MOBILEVIT_INFER_OUTPUT", "");
}

inline std::string resolve_deeplab_template_image_path() {
    return resolve_env_or_default("LIBTORCH_MODULE_DEEPLAB_TEMPLATE_IMAGE", "");
}

inline std::string resolve_deeplab_test_image_path() {
    return resolve_env_or_default("LIBTORCH_MODULE_DEEPLAB_TEST_IMAGE", "");
}

inline std::string resolve_deeplab_output_image_path() {
    return resolve_env_or_default("LIBTORCH_MODULE_DEEPLAB_OUTPUT", "");
}

inline std::string make_attach_back_meta_path(const std::string& output_path) {
    return output_path.empty() ? std::string() : (output_path + ".meta.txt");
}

inline void ensure_parent_dir_exists(const std::string& file_path) {
    if (file_path.empty()) {
        return;
    }

    std::error_code ec;
    const fs::path parent = fs::path(file_path).parent_path();
    if (!parent.empty()) {
        fs::create_directories(parent, ec);
    }
}

inline bool build_twostage_real_image_input(const std::string& image_path,
                                            int64_t image_size,
                                            torch::Device device,
                                            torch::Tensor& image_bchw,
                                            cv::Mat& source_bgr,
                                            cv::Mat& resized_bgr) {
    if (image_path.empty() || !fs::exists(image_path)) {
        return false;
    }

    source_bgr = cv::imread(image_path, cv::IMREAD_COLOR);
    if (source_bgr.empty()) {
        return false;
    }

    cv::resize(source_bgr, resized_bgr, cv::Size(static_cast<int>(image_size),
                                                 static_cast<int>(image_size)));
    cv::Mat resized_rgb;
    cv::cvtColor(resized_bgr, resized_rgb, cv::COLOR_BGR2RGB);

    image_bchw = torch::from_blob(resized_rgb.data,
                                  {1, resized_rgb.rows, resized_rgb.cols, 3},
                                  torch::kUInt8)
                   .permute({0, 3, 1, 2})
                   .to(torch::kFloat32)
                   .div_(255.0f)
                   .clone()
                   .to(device);
    return true;
}

inline std::string format_attach_back_score(float confidence) {
    std::ostringstream os;
    os << std::fixed << std::setprecision(4) << confidence;
    return os.str();
}

inline void write_attach_back_meta(const std::string& meta_path,
                                   const std::string& status,
                                   const std::string& input_image_path,
                                   const std::string& output_image_path,
                                   const std::string& top1_class,
                                   const std::string& confidence,
                                   const BBox& bbox) {
    if (meta_path.empty()) {
        return;
    }

    ensure_parent_dir_exists(meta_path);
    std::ofstream output(meta_path, std::ios::out | std::ios::trunc);
    if (!output.is_open()) {
        return;
    }

    output << "attach_back_overlay_status=" << status << "\n";
    output << "attach_back_input_image=" << input_image_path << "\n";
    output << "attach_back_output_image=" << output_image_path << "\n";
    output << "attach_back_top1_class=" << top1_class << "\n";
    output << "attach_back_confidence=" << confidence << "\n";
    output << "attach_back_roi_xyxy="
           << static_cast<int>(std::round(bbox.x1)) << ","
           << static_cast<int>(std::round(bbox.y1)) << ","
           << static_cast<int>(std::round(bbox.x2)) << ","
           << static_cast<int>(std::round(bbox.y2)) << "\n";
}

inline void write_review_visual_meta(const std::string& meta_path,
                                     const std::string& status,
                                     const std::string& input_image_path,
                                     const std::string& output_image_path,
                                     const std::string& actual_device,
                                     const std::string& primary_label,
                                     const std::string& secondary_value) {
    if (meta_path.empty()) {
        return;
    }

    ensure_parent_dir_exists(meta_path);
    std::ofstream output(meta_path, std::ios::out | std::ios::trunc);
    if (!output.is_open()) {
        return;
    }

    output << "review_visual_status=" << status << "\n";
    output << "review_input_image=" << input_image_path << "\n";
    output << "review_output_image=" << output_image_path << "\n";
    output << "actual_device=" << actual_device << "\n";
    output << "review_primary_label=" << primary_label << "\n";
    output << "review_secondary_value=" << secondary_value << "\n";
}

inline BBox make_center_bbox(int64_t width, int64_t height, float scale = 0.5f) {
    const float roi_w = static_cast<float>(width) * scale;
    const float roi_h = static_cast<float>(height) * scale;
    const float x1 = (static_cast<float>(width) - roi_w) * 0.5f;
    const float y1 = (static_cast<float>(height) - roi_h) * 0.5f;
    return {x1, y1, x1 + roi_w, y1 + roi_h, 1.0f, 0};
}

inline torch::Tensor crop_and_resize_roi(const torch::Tensor& image_bchw, const BBox& bbox, int64_t out_size = 256) {
    TORCH_CHECK(image_bchw.dim() == 4 && image_bchw.size(0) == 1, "Expected a single image tensor in BCHW format");

    const int64_t height = image_bchw.size(2);
    const int64_t width = image_bchw.size(3);

    const int64_t x1 = std::clamp<int64_t>(static_cast<int64_t>(std::floor(bbox.x1)), 0, width - 1);
    const int64_t y1 = std::clamp<int64_t>(static_cast<int64_t>(std::floor(bbox.y1)), 0, height - 1);
    const int64_t x2 = std::clamp<int64_t>(static_cast<int64_t>(std::ceil(bbox.x2)), x1 + 1, width);
    const int64_t y2 = std::clamp<int64_t>(static_cast<int64_t>(std::ceil(bbox.y2)), y1 + 1, height);

    auto roi = image_bchw.slice(2, y1, y2).slice(3, x1, x2).contiguous();

    return torch::nn::functional::interpolate(
        roi,
        torch::nn::functional::InterpolateFuncOptions()
            .size(std::vector<int64_t>{out_size, out_size})
            .mode(torch::kBilinear)
            .align_corners(false));
}


inline int test_MobileViTv2_Shape() {
    std::cout << "=== Testing MobileViTv2 Shapes ===" << std::endl;
    try {
        MobileViTv2 model(1000);
        torch::Device device = get_mobilevit_test_device();
        model->to(device);
        model->eval();

        torch::Tensor x = torch::randn({1, 3, 256, 256}, device);
        auto out = model->forward(x);

        TORCH_CHECK(out.size(0) == 1 && out.size(1) == 1000, "Unexpected MobileViTv2 output shape: ", out.sizes());
        std::cout << "OK MobileViTv2 shape: " << out.sizes() << std::endl;
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "FAIL MobileViTv2 shape: " << e.what() << std::endl;
        return 1;
    }
}

inline int test_MobileViTv2_Load() {
    std::cout << "=== Testing MobileViTv2 Weight Loading ===" << std::endl;
    try {
        MobileViTv2 model(1000);
        torch::Device device = get_mobilevit_test_device();
        model->to(device);
        model->eval();

        std::string weight_path = resolve_mobilevit_weight_path();
        if (!fs::exists(weight_path)) {
            std::cout << "SKIP missing weight file: " << weight_path << std::endl;
            return 0;
        }

        std::string weight_status;
        try_load_mobilevit_external_weights(model, weight_path, weight_status);
        torch::Tensor x = torch::randn({1, 3, 256, 256}, device);
        auto out = model->forward(x);
        TORCH_CHECK(out.size(1) == 1000, "Loaded model output channel mismatch: ", out.sizes());

        std::cout << "OK MobileViTv2 load, status=" << weight_status
                  << " output mean: " << out.mean().item<float>() << std::endl;
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "FAIL MobileViTv2 load: " << e.what() << std::endl;
        return 1;
    }
}

inline int test_MobileViTv2_Train() {
    std::cout << "=== Testing MobileViTv2 Training Loop ===" << std::endl;
    try {
        int num_classes = 10;
        MobileViTv2 model(num_classes);

        torch::Device device = get_mobilevit_test_device();
        model->to(device);
        model->train();

        torch::optim::Adam optimizer(model->parameters(), torch::optim::AdamOptions(1e-4));
        torch::nn::CrossEntropyLoss loss_fn;

        auto imgs = torch::randn({4, 3, 256, 256}, device);
        auto targets = torch::randint(0, num_classes, {4}, torch::TensorOptions().dtype(torch::kLong).device(device));

        optimizer.zero_grad();
        auto outputs = model->forward(imgs);
        auto loss = loss_fn(outputs, targets);

        loss.backward();
        optimizer.step();

        TORCH_CHECK(torch::isfinite(loss).item<bool>(), "MobileViTv2 loss is not finite");
        std::cout << "OK MobileViTv2 train step, loss: " << loss.item<float>() << std::endl;
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "FAIL MobileViTv2 train step: " << e.what() << std::endl;
        return 1;
    }
}

inline int test_MobileViTv2_MainlineSession() {
    std::cout << "=== Testing MobileViTv2 Mainline Session ===" << std::endl;
    try {
        torch::Device device = get_mobilevit_test_device();

        auto runner = make_mobilevit_mainline_runner_config(5, 256, 4);
        runner.device_policy = device.is_cuda() ? MobileViTDevicePolicy::ForceCUDA : MobileViTDevicePolicy::ForceCPU;

        MobileViTv2 model(5);
        model->to(device);

        auto train_imgs = torch::randn({4, 3, 256, 256}, device);
        auto train_targets = torch::randint(0, 5, {4}, torch::TensorOptions().dtype(torch::kLong).device(device));
        auto eval_imgs = torch::randn({4, 3, 256, 256}, device);
        auto eval_targets = torch::randint(0, 5, {4}, torch::TensorOptions().dtype(torch::kLong).device(device));

        auto session = run_mobilevit_mainline_session(
            model, train_imgs, train_targets, eval_imgs, eval_targets, runner);
        session.validate();
        auto trainer = run_mobilevit_trainer_session(
            model, train_imgs, train_targets, eval_imgs, eval_targets, runner);
        trainer.validate();
        auto analysis = build_mobilevit_trainer_analysis(trainer);
        analysis.validate();
        auto unified = build_mobilevit_unified_mainline_bundle(session, analysis);
        unified.validate();
        auto unified_summary = build_mobilevit_unified_mainline_summary(unified);
        unified_summary.validate();

        TORCH_CHECK(session.passed, "MobileViT mainline session should pass");
        TORCH_CHECK(session.flat_run.outcomes.size() == 2,
            "MobileViT mainline session should expose smoke/eval outcomes");
        TORCH_CHECK(analysis.timeline.stages.size() == 4,
            "MobileViT trainer timeline should expose four lifecycle stages");
        TORCH_CHECK(analysis.comparison_rows.size() == 2,
            "MobileViT trainer analysis should expose smoke/eval comparison rows");

        std::cout << "MobileViT session summary: " << session.summary << std::endl;
        std::cout << "MobileViT session run: " << session.config.run_name
                  << " device_policy=" << mobilevit_device_policy_name(session.config.device_policy)
                  << " smoke_loss=" << session.smoke.loss
                  << " eval_top1=" << session.eval.top1
                  << std::endl;
        std::cout << "MobileViT flat run: " << session.flat_run.run_name
                  << " outcomes=" << session.flat_run.outcomes.size()
                  << " passed=" << (session.flat_run.all_passed ? "yes" : "no")
                  << std::endl;
        std::cout << "MobileViT trainer summary: " << trainer.summary << std::endl;
        for (const auto& stage : analysis.timeline.stages) {
            std::cout << "MobileViT trainer stage: " << stage.stage_name
                      << " passed=" << (stage.passed ? "yes" : "no")
                      << " detail=" << stage.detail
                      << std::endl;
        }
        std::cout << "MobileViT trainer comparison rows: " << analysis.comparison_rows.size()
                  << " first_stage=" << analysis.comparison_rows.front().stage_name
                  << " second_stage=" << analysis.comparison_rows.back().stage_name
                  << std::endl;
        std::cout << "MobileViT trainer recommendation: " << analysis.recommendation.track_name
                  << " selections=" << analysis.recommendation.selected_experiments.size()
                  << std::endl;
        std::cout << "MobileViT trainer lifecycle summary: " << analysis.lifecycle_summary.summary
                  << std::endl;
        std::cout << "MobileViT trainer flat run: " << analysis.flat_run.run_name
                  << " outcomes=" << analysis.flat_run.outcomes.size()
                  << " passed=" << (analysis.flat_run.all_passed ? "yes" : "no")
                  << std::endl;
        std::cout << "MobileViT unified bundle: " << unified.flat_run.run_name
                  << " outcomes=" << unified.flat_run.outcomes.size()
                  << " comparisons=" << unified.comparison_rows.size()
                  << " selections=" << unified.recommendation.selected_experiments.size()
                  << std::endl;
        std::cout << "MobileViT unified summary: " << unified_summary.summary
                  << std::endl;
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "FAIL MobileViTv2 mainline session: " << e.what() << std::endl;
        return 1;
    }
}

inline int test_MobileViTv2_UnifiedInferReview() {
    std::cout << "=== Testing MobileViTv2 Unified Infer Review ===" << std::endl;
    try {
        const torch::Device device = get_mobilevit_test_device();
        const int64_t classifier_classes = resolve_twostage_infer_num_classes();
        const int64_t image_size = resolve_twostage_image_size();
        const int64_t roi_size = resolve_mobilevit_roi_input_size();
        const std::string input_image_path = resolve_mobilevit_unified_input_image_path();
        const std::string output_image_path = resolve_mobilevit_unified_output_image_path();
        const std::string meta_path = make_attach_back_meta_path(output_image_path);

        MobileViTv2 classifier(classifier_classes);
        classifier->to(device);
        classifier->eval();

        const std::string classifier_weight_path = resolve_mobilevit_weight_path();
        std::string classifier_weight_status;
        try_load_mobilevit_external_weights(classifier,
                                            classifier_weight_path,
                                            classifier_weight_status);

        torch::Tensor image;
        cv::Mat source_bgr;
        cv::Mat resized_bgr;
        const bool has_real_input = build_twostage_real_image_input(input_image_path,
                                                                    image_size,
                                                                    device,
                                                                    image,
                                                                    source_bgr,
                                                                    resized_bgr);
        if (!has_real_input) {
            image = torch::randn({1, 3, image_size, image_size}, device);
        }

        const BBox roi_bbox = make_center_bbox(image_size, image_size);
        auto roi = crop_and_resize_roi(image, roi_bbox, roi_size);
        auto cls_logits = classifier->forward(roi);
        TORCH_CHECK(cls_logits.size(0) == 1 && cls_logits.size(1) == classifier_classes,
                    "Unexpected classifier output shape: ", cls_logits.sizes());

        const auto cls_prob = torch::softmax(cls_logits, 1).to(torch::kCPU);
        const auto topk = cls_prob.max(1);
        const int64_t top1_index = std::get<1>(topk).item<int64_t>();
        const float top1_confidence = std::get<0>(topk).item<float>();
        const std::string top1_class = "class_" + std::to_string(top1_index);
        const std::string confidence_text = format_attach_back_score(top1_confidence);
        std::string visual_status = has_real_input ? "real_roi_crop_ready" : "synthetic_roi_crop_only";

        if (has_real_input && !output_image_path.empty()) {
            ensure_parent_dir_exists(output_image_path);
            const float scale_x = static_cast<float>(source_bgr.cols) / static_cast<float>(image_size);
            const float scale_y = static_cast<float>(source_bgr.rows) / static_cast<float>(image_size);
            const int x1 = std::max(0, static_cast<int>(std::floor(roi_bbox.x1 * scale_x)));
            const int y1 = std::max(0, static_cast<int>(std::floor(roi_bbox.y1 * scale_y)));
            const int x2 = std::min(source_bgr.cols, static_cast<int>(std::ceil(roi_bbox.x2 * scale_x)));
            const int y2 = std::min(source_bgr.rows, static_cast<int>(std::ceil(roi_bbox.y2 * scale_y)));
            cv::Rect roi_rect(x1, y1, std::max(1, x2 - x1), std::max(1, y2 - y1));
            roi_rect &= cv::Rect(0, 0, source_bgr.cols, source_bgr.rows);
            cv::Mat roi_bgr = source_bgr(roi_rect).clone();
            cv::resize(roi_bgr, roi_bgr, cv::Size(static_cast<int>(roi_size), static_cast<int>(roi_size)));
            const std::string label = top1_class + " " + confidence_text;
            cv::putText(roi_bgr, label, cv::Point(10, 22),
                        cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 2);
            if (cv::imwrite(output_image_path, roi_bgr)) {
                visual_status = "real_roi_crop_written";
            } else {
                visual_status = "real_roi_crop_write_failed";
            }
        }

        write_review_visual_meta(meta_path,
                                 visual_status,
                                 input_image_path,
                                 output_image_path,
                                 torch_device_kind_name(device),
                                 top1_class,
                                 confidence_text);

        std::cout << "ACTUAL_DEVICE=" << torch_device_kind_name(device) << std::endl;
        std::cout << "REVIEW_VISUAL_STATUS=" << visual_status << std::endl;
        std::cout << "REVIEW_INPUT_IMAGE=" << input_image_path << std::endl;
        std::cout << "REVIEW_OUTPUT_IMAGE=" << output_image_path << std::endl;
        std::cout << "REVIEW_OUTPUT_META=" << meta_path << std::endl;
        std::cout << "REVIEW_TOP1_CLASS=" << top1_class << std::endl;
        std::cout << "REVIEW_CONFIDENCE=" << confidence_text << std::endl;
        std::cout << "MOBILEVIT_WEIGHT_STATUS=" << classifier_weight_status << std::endl;
        std::cout << "OK mobilevit unified infer review"
                  << " image_size=" << image_size
                  << " roi_size=" << roi_size
                  << " input_mode=" << (has_real_input ? "real_image" : "synthetic")
                  << std::endl;
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "FAIL MobileViTv2 unified infer review: " << e.what() << std::endl;
        return 1;
    }
}

inline int test_Segmentation_MainlineSession() {
    std::cout << "=== Testing Segmentation Mainline Session ===" << std::endl;
    try {
        torch::Device device = get_mobilevit_test_device();
        auto runner = make_segmentation_mainline_runner_config("deeplabv3plus", "mobilenet_v3_large", 3, 128, 2);
        runner.device_policy = device.is_cuda() ? SegmentationDevicePolicy::ForceCUDA : SegmentationDevicePolicy::ForceCPU;

        auto train_imgs = torch::randn({2, 3, 128, 128}, device);
        auto train_masks = torch::randint(0, 3, {2, 128, 128},
            torch::TensorOptions().dtype(torch::kLong).device(device));
        auto eval_imgs = torch::randn({2, 3, 128, 128}, device);
        auto eval_masks = torch::randint(0, 3, {2, 128, 128},
            torch::TensorOptions().dtype(torch::kLong).device(device));

        auto session = run_segmentation_mainline_session(
            train_imgs, train_masks, eval_imgs, eval_masks, runner);
        session.validate();
        auto trainer = run_segmentation_trainer_session(
            train_imgs, train_masks, eval_imgs, eval_masks, runner);
        trainer.validate();
        auto analysis = build_segmentation_trainer_analysis(trainer);
        analysis.validate();
        auto unified = build_segmentation_unified_mainline_bundle(session, analysis);
        unified.validate();
        auto unified_summary = build_segmentation_unified_mainline_summary(unified);
        unified_summary.validate();

        TORCH_CHECK(session.passed, "Segmentation mainline session should pass");
        TORCH_CHECK(session.flat_run.outcomes.size() == 2,
            "Segmentation mainline session should expose smoke/eval outcomes");
        TORCH_CHECK(session.eval.foreground_iou >= 0.0 && session.eval.foreground_iou <= 1.0,
            "Segmentation mainline session foreground IoU must stay in range");
        TORCH_CHECK(analysis.timeline.stages.size() == 4,
            "Segmentation trainer timeline should expose four lifecycle stages");
        TORCH_CHECK(analysis.comparison_rows.size() == 2,
            "Segmentation trainer analysis should expose smoke/eval comparison rows");

        std::cout << "Segmentation session summary: " << session.summary << std::endl;
        std::cout << "Segmentation session run: " << session.config.run_name
                  << " decoder=" << session.config.decoder
                  << " backbone=" << session.config.backbone
                  << " device_policy=" << segmentation_device_policy_name(session.config.device_policy)
                  << " smoke_loss=" << session.smoke.loss
                  << " eval_iou=" << session.eval.foreground_iou
                  << std::endl;
        std::cout << "Segmentation flat run: " << session.flat_run.run_name
                  << " outcomes=" << session.flat_run.outcomes.size()
                  << " passed=" << (session.flat_run.all_passed ? "yes" : "no")
                  << std::endl;
        std::cout << "Segmentation trainer summary: " << trainer.summary << std::endl;
        for (const auto& stage : analysis.timeline.stages) {
            std::cout << "Segmentation trainer stage: " << stage.stage_name
                      << " passed=" << (stage.passed ? "yes" : "no")
                      << " detail=" << stage.detail
                      << std::endl;
        }
        std::cout << "Segmentation trainer comparison rows: " << analysis.comparison_rows.size()
                  << " first_stage=" << analysis.comparison_rows.front().stage_name
                  << " second_stage=" << analysis.comparison_rows.back().stage_name
                  << std::endl;
        std::cout << "Segmentation trainer recommendation: " << analysis.recommendation.track_name
                  << " selections=" << analysis.recommendation.selected_experiments.size()
                  << std::endl;
        std::cout << "Segmentation trainer lifecycle summary: " << analysis.lifecycle_summary.summary << std::endl;
        std::cout << "Segmentation trainer flat run: " << analysis.flat_run.run_name
                  << " outcomes=" << analysis.flat_run.outcomes.size()
                  << " passed=" << (analysis.flat_run.all_passed ? "yes" : "no")
                  << std::endl;
        std::cout << "Segmentation unified bundle: " << unified.flat_run.run_name
                  << " outcomes=" << unified.flat_run.outcomes.size()
                  << " comparisons=" << unified.comparison_rows.size()
                  << " selections=" << unified.recommendation.selected_experiments.size()
                  << std::endl;
        std::cout << "Segmentation unified summary: " << unified_summary.summary << std::endl;
        std::cout << "[PASS] Segmentation mainline session passed" << std::endl;
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "FAIL Segmentation mainline session: " << e.what() << std::endl;
        return 1;
    }
}

inline int test_Segmentation_UnifiedInferReview() {
    std::cout << "=== Testing Segmentation Unified Infer Review ===" << std::endl;
    try {
        const torch::Device device = get_mobilevit_test_device();
        auto runner = make_segmentation_mainline_runner_config("deeplabv3plus", "mobilenet_v3_large", 3, 128, 2);
        runner.device_policy = device.is_cuda() ? SegmentationDevicePolicy::ForceCUDA : SegmentationDevicePolicy::ForceCPU;

        const std::string template_image_path = resolve_deeplab_template_image_path();
        const std::string test_image_path = resolve_deeplab_test_image_path();
        const std::string output_image_path = resolve_deeplab_output_image_path();
        const std::string meta_path = make_attach_back_meta_path(output_image_path);

        cv::Mat template_bgr = cv::imread(template_image_path, cv::IMREAD_COLOR);
        cv::Mat test_bgr = cv::imread(test_image_path, cv::IMREAD_COLOR);
        const bool has_real_pair = !template_bgr.empty() && !test_bgr.empty();

        auto train_imgs = torch::randn({2, 3, 128, 128}, device);
        auto train_masks = torch::randint(0, 3, {2, 128, 128},
            torch::TensorOptions().dtype(torch::kLong).device(device));
        auto eval_imgs = torch::randn({2, 3, 128, 128}, device);
        auto eval_masks = torch::randint(0, 3, {2, 128, 128},
            torch::TensorOptions().dtype(torch::kLong).device(device));

        if (has_real_pair) {
            cv::Mat template_rgb;
            cv::Mat test_rgb;
            cv::resize(template_bgr, template_bgr, cv::Size(128, 128));
            cv::resize(test_bgr, test_bgr, cv::Size(128, 128));
            cv::cvtColor(template_bgr, template_rgb, cv::COLOR_BGR2RGB);
            cv::cvtColor(test_bgr, test_rgb, cv::COLOR_BGR2RGB);

            auto template_tensor = torch::from_blob(template_rgb.data, {1, 128, 128, 3}, torch::kUInt8)
                                       .permute({0, 3, 1, 2})
                                       .to(torch::kFloat32)
                                       .div_(255.0f)
                                       .clone()
                                       .to(device);
            auto test_tensor = torch::from_blob(test_rgb.data, {1, 128, 128, 3}, torch::kUInt8)
                                   .permute({0, 3, 1, 2})
                                   .to(torch::kFloat32)
                                   .div_(255.0f)
                                   .clone()
                                   .to(device);
            train_imgs = torch::cat({template_tensor, test_tensor}, 0);
            eval_imgs = torch::cat({template_tensor, test_tensor}, 0);
            train_masks = torch::zeros({2, 128, 128},
                torch::TensorOptions().dtype(torch::kLong).device(device));
            eval_masks = train_masks.clone();
        }

        auto session = run_segmentation_mainline_session(
            train_imgs, train_masks, eval_imgs, eval_masks, runner);
        session.validate();
        auto trainer = run_segmentation_trainer_session(
            train_imgs, train_masks, eval_imgs, eval_masks, runner);
        trainer.validate();
        auto analysis = build_segmentation_trainer_analysis(trainer);
        analysis.validate();
        auto unified = build_segmentation_unified_mainline_bundle(session, analysis);
        unified.validate();
        auto unified_summary = build_segmentation_unified_mainline_summary(unified);
        unified_summary.validate();

        std::string visual_status = has_real_pair ? "real_template_pair_ready" : "synthetic_template_pair_only";
        int diff_region_count = 0;
        if (has_real_pair && !output_image_path.empty()) {
            ensure_parent_dir_exists(output_image_path);
            cv::Mat template_gray;
            cv::Mat test_gray;
            cv::cvtColor(template_bgr, template_gray, cv::COLOR_BGR2GRAY);
            cv::cvtColor(test_bgr, test_gray, cv::COLOR_BGR2GRAY);
            cv::Mat diff;
            cv::absdiff(template_gray, test_gray, diff);
            cv::Mat mask;
            cv::threshold(diff, mask, 24, 255, cv::THRESH_BINARY);
            cv::morphologyEx(mask, mask, cv::MORPH_CLOSE,
                             cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3)));
            std::vector<std::vector<cv::Point>> contours;
            cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
            cv::Mat overlay = test_bgr.clone();
            for (const auto& contour : contours) {
                const cv::Rect rect = cv::boundingRect(contour);
                if (rect.area() < 8) {
                    continue;
                }
                ++diff_region_count;
                cv::rectangle(overlay, rect, cv::Scalar(0, 0, 255), 2);
            }
            const std::string label = "diff_regions=" + std::to_string(diff_region_count);
            cv::putText(overlay, label, cv::Point(10, 22),
                        cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 255), 2);
            if (cv::imwrite(output_image_path, overlay)) {
                visual_status = "real_template_pair_overlay_written";
            } else {
                visual_status = "real_template_pair_overlay_write_failed";
            }
        }

        write_review_visual_meta(meta_path,
                                 visual_status,
                                 test_image_path,
                                 output_image_path,
                                 torch_device_kind_name(device),
                                 "diff_regions",
                                 std::to_string(diff_region_count));

        std::cout << "ACTUAL_DEVICE=" << torch_device_kind_name(device) << std::endl;
        std::cout << "REVIEW_VISUAL_STATUS=" << visual_status << std::endl;
        std::cout << "REVIEW_INPUT_IMAGE=" << test_image_path << std::endl;
        std::cout << "REVIEW_OUTPUT_IMAGE=" << output_image_path << std::endl;
        std::cout << "REVIEW_OUTPUT_META=" << meta_path << std::endl;
        std::cout << "DEEPLAB_TEMPLATE_IMAGE=" << template_image_path << std::endl;
        std::cout << "DEEPLAB_DIFF_COUNT=" << diff_region_count << std::endl;
        std::cout << "Segmentation unified summary: " << unified_summary.summary << std::endl;
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "FAIL Segmentation unified infer review: " << e.what() << std::endl;
        return 1;
    }
}

inline int test_MobileViTv2_DatasetSmoke() {
    std::cout << "=== Testing MobileViTv2 Dataset Smoke ===" << std::endl;
    try {
        const std::string missing_dir = resolve_mobilevit_missing_dataset_probe_path();
        MobileViTv2Dataset missing_dataset(missing_dir, true);
        TORCH_CHECK(missing_dataset.size().value_or(0) == 0, "Missing dataset path should yield zero samples");
        std::cout << "OK dataset smoke, missing path handled cleanly: " << missing_dir << std::endl;
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "FAIL MobileViTv2 dataset smoke: " << e.what() << std::endl;
        return 1;
    }
}

inline int test_MobileViTv2_FullTrain() {
    std::cout << "=== Testing MobileViTv2 Full Training (Mock) ===" << std::endl;
    try {
        std::string data_dir = resolve_mobilevit_train_dataset_path();
        if (!fs::exists(data_dir)) {
            std::cout << "SKIP missing dataset dir: " << data_dir << std::endl;
            return 0;
        }

        auto raw_dataset = MobileViTv2Dataset(data_dir, true);
        const auto dataset_size = raw_dataset.size().value_or(0);
        if (dataset_size == 0) {
            std::cout << "SKIP empty dataset: " << data_dir << std::endl;
            return 0;
        }
        const int64_t class_count = count_imagefolder_classes(data_dir);
        TORCH_CHECK(class_count > 0, "Dataset reports samples but no classes: ", data_dir);

        auto dataset = std::move(raw_dataset).map(torch::data::transforms::Stack<>());
        int batch_size = static_cast<int>(std::min<size_t>(4, dataset_size));

        MobileViTv2 model(static_cast<int64_t>(class_count));
        torch::Device device = get_mobilevit_test_device();
        model->to(device);
        model->train();

        auto data_loader = torch::data::make_data_loader(
            std::move(dataset),
            torch::data::DataLoaderOptions().batch_size(batch_size).workers(0)
        );

        torch::optim::SGD optimizer(model->parameters(), torch::optim::SGDOptions(0.01).momentum(0.9));
        torch::nn::CrossEntropyLoss loss_fn;

        int batches = 0;
        for (auto& batch : *data_loader) {
            auto imgs = batch.data.to(device);
            auto targets = batch.target.to(device, torch::kLong);

            optimizer.zero_grad();
            auto outputs = model->forward(imgs);
            auto loss = loss_fn(outputs, targets);
            loss.backward();
            optimizer.step();
            ++batches;
            break;
        }

        TORCH_CHECK(batches > 0, "Expected at least one batch from dataset");
        std::cout << "OK MobileViTv2 full-train smoke"
                  << " classes=" << class_count
                  << " batch_size=" << batch_size
                  << " samples=" << dataset_size
                  << std::endl;
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "FAIL MobileViTv2 full-train smoke: " << e.what() << std::endl;
        return 1;
    }
}

inline int test_YOLOv8_MobileViTv2_TwoStageInference() {
    std::cout << "=== Testing YOLOv8 + MobileViTv2 Two-Stage Inference ===" << std::endl;
    try {
        auto device = get_mobilevit_test_device();
        const int64_t classifier_classes = resolve_twostage_infer_num_classes();
        const int64_t image_size = resolve_twostage_image_size();
        const int64_t roi_size = resolve_mobilevit_roi_input_size();
        const std::string input_image_path = resolve_twostage_input_image_path();
        const std::string output_image_path = resolve_twostage_output_image_path();
        const std::string meta_path = make_attach_back_meta_path(output_image_path);
        TORCH_CHECK(classifier_classes > 0, "Two-stage infer classes must be positive");
        TORCH_CHECK(image_size > 0, "Two-stage infer image_size must be positive");
        TORCH_CHECK(roi_size > 0, "Two-stage infer roi_size must be positive");

        auto det_cfg = ModelConfig::get_config("nano", 3);
        YOLOv8 detector(det_cfg);
        MobileViTv2 classifier(classifier_classes);

        detector->to(device);
        classifier->to(device);
        detector->eval();
        classifier->eval();

        const std::string classifier_weight_path = resolve_mobilevit_weight_path();
        std::string classifier_weight_status;
        try_load_mobilevit_external_weights(classifier,
                                            classifier_weight_path,
                                            classifier_weight_status);

        torch::Tensor image;
        cv::Mat source_bgr;
        cv::Mat resized_bgr;
        const bool has_real_input = build_twostage_real_image_input(input_image_path,
                                                                    image_size,
                                                                    device,
                                                                    image,
                                                                    source_bgr,
                                                                    resized_bgr);
        if (!has_real_input) {
            image = torch::randn({1, 3, image_size, image_size}, device);
        }

        auto det_pred = detector->forward(image);
        TORCH_CHECK(det_pred.dim() == 3, "Unexpected detector output dim: ", det_pred.sizes());
        TORCH_CHECK(det_pred.size(2) == 4 + det_cfg.num_classes, "Unexpected detector channel layout: ", det_pred.sizes());

        const BBox roi_bbox = make_center_bbox(image_size, image_size);
        auto roi = crop_and_resize_roi(image, roi_bbox, roi_size);
        auto cls_logits = classifier->forward(roi);
        TORCH_CHECK(cls_logits.size(0) == 1 && cls_logits.size(1) == classifier_classes,
            "Unexpected classifier output shape: ", cls_logits.sizes());

        const auto cls_prob = torch::softmax(cls_logits, 1).to(torch::kCPU);
        const auto topk = cls_prob.max(1);
        const int64_t top1_index = std::get<1>(topk).item<int64_t>();
        const float top1_confidence = std::get<0>(topk).item<float>();
        const std::string top1_class = "class_" + std::to_string(top1_index);
        const std::string confidence_text = format_attach_back_score(top1_confidence);

        std::string overlay_status = has_real_input
            ? "real_image_center_roi_overlay"
            : "synthetic_center_roi_overlay";

        if (has_real_input && !output_image_path.empty()) {
            ensure_parent_dir_exists(output_image_path);
            cv::Mat overlay = source_bgr.clone();
            const float scale_x = static_cast<float>(source_bgr.cols) / static_cast<float>(image_size);
            const float scale_y = static_cast<float>(source_bgr.rows) / static_cast<float>(image_size);
            const cv::Point top_left(
                static_cast<int>(std::round(roi_bbox.x1 * scale_x)),
                static_cast<int>(std::round(roi_bbox.y1 * scale_y)));
            const cv::Point bottom_right(
                static_cast<int>(std::round(roi_bbox.x2 * scale_x)),
                static_cast<int>(std::round(roi_bbox.y2 * scale_y)));
            cv::rectangle(overlay, top_left, bottom_right, cv::Scalar(0, 255, 0), 2);

            const std::string label = top1_class + " " + confidence_text;
            const cv::Point text_anchor(top_left.x,
                                        std::max(20, top_left.y - 8));
            cv::putText(overlay, label, text_anchor,
                        cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 255, 0), 2);

            if (cv::imwrite(output_image_path, overlay)) {
                overlay_status = "real_image_center_roi_overlay_written";
            } else {
                overlay_status = "real_image_center_roi_overlay_write_failed";
            }
        }

        write_attach_back_meta(meta_path,
                               overlay_status,
                               input_image_path,
                               output_image_path,
                               top1_class,
                               confidence_text,
                               roi_bbox);

        std::cout << "ATTACH_BACK_OVERLAY_STATUS=" << overlay_status << std::endl;
        std::cout << "ATTACH_BACK_WEIGHT_STATUS=" << classifier_weight_status << std::endl;
        std::cout << "ATTACH_BACK_TOP1_CLASS=" << top1_class << std::endl;
        std::cout << "ATTACH_BACK_CONFIDENCE=" << confidence_text << std::endl;
        if (!input_image_path.empty()) {
            std::cout << "ATTACH_BACK_INPUT_IMAGE=" << input_image_path << std::endl;
        }
        if (!output_image_path.empty()) {
            std::cout << "ATTACH_BACK_OUTPUT_IMAGE=" << output_image_path << std::endl;
        }
        if (!meta_path.empty()) {
            std::cout << "ATTACH_BACK_META_PATH=" << meta_path << std::endl;
        }

        std::cout << "OK two-stage inference, det=" << det_pred.sizes()
                  << " cls=" << cls_logits.sizes()
                  << " image_size=" << image_size
                  << " roi_size=" << roi_size
                  << " classes=" << classifier_classes
                  << " input_mode=" << (has_real_input ? "real_image" : "synthetic")
                  << std::endl;
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "FAIL two-stage inference: " << e.what() << std::endl;
        return 1;
    }
}

inline int test_YOLOv8_MobileViTv2_TwoStageTrainMock() {
    std::cout << "=== Testing YOLOv8 + MobileViTv2 Two-Stage Train Mock ===" << std::endl;
    try {
        auto device = get_mobilevit_test_device();
        const int64_t classifier_classes = resolve_twostage_train_num_classes();
        const int64_t image_size = resolve_twostage_image_size();
        const int64_t roi_size = resolve_mobilevit_roi_input_size();
        const int64_t batch_size = resolve_twostage_train_batch_size();
        TORCH_CHECK(classifier_classes > 1, "Two-stage train classes must be greater than one");
        TORCH_CHECK(image_size > 0, "Two-stage train image_size must be positive");
        TORCH_CHECK(roi_size > 0, "Two-stage train roi_size must be positive");
        TORCH_CHECK(batch_size > 0, "Two-stage train batch_size must be positive");

        auto det_cfg = ModelConfig::get_config("nano", 3);
        YOLOv8 detector(det_cfg);
        MobileViTv2 classifier(classifier_classes);

        detector->to(device);
        classifier->to(device);
        detector->eval();
        classifier->train();

        auto images = torch::randn({batch_size, 3, image_size, image_size}, device);
        auto det_pred = detector->forward(images);
        TORCH_CHECK(det_pred.size(0) == batch_size, "Detector batch mismatch: ", det_pred.sizes());

        std::vector<torch::Tensor> rois;
        rois.reserve(static_cast<size_t>(batch_size));
        for (int64_t i = 0; i < images.size(0); ++i) {
            rois.push_back(crop_and_resize_roi(
                images[i].unsqueeze(0), make_center_bbox(image_size, image_size), roi_size));
        }

        auto roi_batch = torch::cat(rois, 0);
        auto targets = torch::arange(batch_size,
            torch::TensorOptions().dtype(torch::kLong).device(device)).remainder(classifier_classes);

        torch::optim::Adam optimizer(classifier->parameters(), torch::optim::AdamOptions(1e-4));
        torch::nn::CrossEntropyLoss loss_fn;

        optimizer.zero_grad();
        auto logits = classifier->forward(roi_batch);
        auto loss = loss_fn(logits, targets);
        loss.backward();
        optimizer.step();

        TORCH_CHECK(torch::isfinite(loss).item<bool>(), "Two-stage training loss is not finite");
        std::cout << "OK two-stage train mock, loss: " << loss.item<float>()
                  << " batch_size=" << batch_size
                  << " image_size=" << image_size
                  << " roi_size=" << roi_size
                  << " classes=" << classifier_classes
                  << std::endl;
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "FAIL two-stage train mock: " << e.what() << std::endl;
        return 1;
    }
}

inline int run_mobilevit_tests() {
    std::cout << "***************************************" << std::endl;
    std::cout << "* Running MobileViTv2 and Two-Stage   *" << std::endl;
    std::cout << "* full-validation stage only          *" << std::endl;
    std::cout << "***************************************" << std::endl;
    const std::string weight_path = resolve_mobilevit_weight_path();
    const std::string missing_dataset_probe = resolve_mobilevit_missing_dataset_probe_path();
    const std::string train_dataset_path = resolve_mobilevit_train_dataset_path();
    const int64_t roi_size = resolve_mobilevit_roi_input_size();
    const int64_t twostage_image_size = resolve_twostage_image_size();
    const int64_t twostage_infer_classes = resolve_twostage_infer_num_classes();
    const int64_t twostage_train_classes = resolve_twostage_train_num_classes();
    const int64_t twostage_train_batch = resolve_twostage_train_batch_size();
    std::cout << "  MOBILEVIT_LOAD=" << TORCH_FULL_ENABLE_MOBILEVIT_LOAD
              << " MOBILEVIT_TRAIN=" << TORCH_FULL_ENABLE_MOBILEVIT_TRAIN
              << " DATASET_STAGE=" << TORCH_FULL_ENABLE_DATASET_STAGE
              << " MOBILEVIT_DATASET=" << TORCH_FULL_ENABLE_MOBILEVIT_DATASET
              << " MOBILEVIT_FULLTRAIN=" << TORCH_FULL_ENABLE_MOBILEVIT_FULLTRAIN
              << " TWOSTAGE_INFER=" << TORCH_FULL_ENABLE_TWOSTAGE_INFER
              << " TWOSTAGE_TRAIN=" << TORCH_FULL_ENABLE_TWOSTAGE_TRAIN
              << " OCC_STAGE=" << TORCH_FULL_ENABLE_OCC_STAGE
              << std::endl;
    std::cout << "  MOBILEVIT_WEIGHT_PATH=" << weight_path << std::endl;
    std::cout << "  MOBILEVIT_DATASET_PATH=" << train_dataset_path << std::endl;
    std::cout << "  MOBILEVIT_MISSING_DATASET_PROBE=" << missing_dataset_probe << std::endl;
    std::cout << "  MOBILEVIT_ROI_SIZE=" << roi_size
              << " TWOSTAGE_IMAGE_SIZE=" << twostage_image_size
              << " TWOSTAGE_INFER_CLASSES=" << twostage_infer_classes
              << " TWOSTAGE_TRAIN_CLASSES=" << twostage_train_classes
              << " TWOSTAGE_TRAIN_BATCH=" << twostage_train_batch
              << std::endl;

    int failures = 0;
    failures += test_MobileViTv2_Shape();
#if TORCH_FULL_ENABLE_MOBILEVIT_LOAD
    failures += test_MobileViTv2_Load();
#endif
#if TORCH_FULL_ENABLE_MOBILEVIT_TRAIN
    failures += test_MobileViTv2_Train();
    failures += test_MobileViTv2_MainlineSession();
    failures += test_Segmentation_MainlineSession();
#endif
#if TORCH_FULL_ENABLE_DATASET_STAGE && TORCH_FULL_ENABLE_MOBILEVIT_DATASET
    failures += test_MobileViTv2_DatasetSmoke();
#endif
#if TORCH_FULL_ENABLE_TWOSTAGE_INFER
    failures += test_YOLOv8_MobileViTv2_TwoStageInference();
#endif
#if TORCH_FULL_ENABLE_TWOSTAGE_TRAIN
    failures += test_YOLOv8_MobileViTv2_TwoStageTrainMock();
#endif
#if TORCH_FULL_ENABLE_DATASET_STAGE && TORCH_FULL_ENABLE_MOBILEVIT_FULLTRAIN
    failures += test_MobileViTv2_FullTrain();
#endif

    if (failures == 0) {
        std::cout << "\nALL MOBILEVIT TESTS PASSED" << std::endl;
    }
    else {
        std::cerr << "\nMOBILEVIT TEST FAILURES: " << failures << std::endl;
    }

    return failures;
}

#endif
