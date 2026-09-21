#ifndef TORCH_ALLTEST_H
#define TORCH_ALLTEST_H


#include <torch/torch.h>
#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <string>
#include <opencv2/opencv.hpp>

#include "torch_modelconfig.h"
#include "torch_nnmodule.h"
#include "torch_taskalignedassigner.h"
#include "torch_v8loss.h"

#include "torch_v8.h"
#include "torch_yolo_mainline_bridge.h"
#include "torch_yolo_dataset.h"
#include "torch_data_augmenter.h"
#include "torch_parser.h"
#include "torch_tuning_profiles.h"

#include "torch_resnet18.h"
#include "torch_resnet50.h"

#include <fstream>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

#ifndef TORCH_FULL_ENABLE_OPENCV_IMAGE
#define TORCH_FULL_ENABLE_OPENCV_IMAGE 1
#endif

#ifndef TORCH_FULL_ENABLE_OPENCV_ANNOTATION
#define TORCH_FULL_ENABLE_OPENCV_ANNOTATION 0
#endif

#ifndef TORCH_FULL_ENABLE_DATASET_STAGE
#define TORCH_FULL_ENABLE_DATASET_STAGE 0
#endif

#ifndef TORCH_FULL_ENABLE_EXTERNAL_WEIGHTS
#define TORCH_FULL_ENABLE_EXTERNAL_WEIGHTS 0
#endif

#ifndef TORCH_FULL_ENABLE_TRAIN_STAGE
#define TORCH_FULL_ENABLE_TRAIN_STAGE 0
#endif

#ifndef TORCH_FULL_ENABLE_INFER_STAGE
#define TORCH_FULL_ENABLE_INFER_STAGE 0
#endif

inline torch::Device get_full_test_device() {
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

inline std::string resolve_fulltest_env_or_default(const char* env_name, const char* fallback) {
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

inline std::string resolve_resnet18_weight_path() {
    return resolve_fulltest_env_or_default("LIBTORCH_MODULE_RESNET18_WEIGHTS", "resnet18_weights.pt");
}

inline std::string resolve_resnet50_weight_path() {
    return resolve_fulltest_env_or_default("LIBTORCH_MODULE_RESNET50_WEIGHTS", "resnet50_weights.pt");
}

inline std::string resolve_inspect_small_backbone_weight_path() {
    return resolve_fulltest_env_or_default("LIBTORCH_MODULE_INSPECT_SMALL_WEIGHTS",
                                           "small_backbone_weights_cpp.pt");
}

inline std::string resolve_inspect_large_backbone_weight_path() {
    return resolve_fulltest_env_or_default("LIBTORCH_MODULE_INSPECT_LARGE_WEIGHTS",
                                           "large_backbone_weights_cpp.pt");
}

inline std::string resolve_transfer_learning_data_root() {
    return resolve_fulltest_env_or_default("LIBTORCH_MODULE_TRANSFER_DATA_ROOT", "D:/project");
}

inline std::string resolve_yolo_weight_path() {
    return resolve_fulltest_env_or_default("LIBTORCH_MODULE_YOLO_WEIGHTS", "d:/yolov8n_dict.pt");
}

inline std::string resolve_yolo_data_root() {
    return resolve_fulltest_env_or_default("LIBTORCH_MODULE_YOLO_DATA_ROOT", "D:/project");
}

inline std::string resolve_yolo_train_image_dir(const std::string& data_root) {
    return resolve_fulltest_env_or_default("LIBTORCH_MODULE_YOLO_TRAIN_IMAGES",
                                           (data_root + "/images/train").c_str());
}

inline std::string resolve_yolo_train_label_dir(const std::string& data_root) {
    return resolve_fulltest_env_or_default("LIBTORCH_MODULE_YOLO_TRAIN_LABELS",
                                           (data_root + "/labels/train").c_str());
}

inline std::string resolve_yolo_pretrained_path() {
    return resolve_fulltest_env_or_default("LIBTORCH_MODULE_YOLO_PRETRAINED", "yolov8n_dict.pt");
}

inline std::string resolve_yolo_infer_image_path() {
    return resolve_fulltest_env_or_default("LIBTORCH_MODULE_YOLO_INFER_IMAGE", "D:/test.jpg");
}

inline std::string resolve_yolo_infer_output_path() {
    return resolve_fulltest_env_or_default("LIBTORCH_MODULE_YOLO_INFER_OUTPUT", "result.jpg");
}

inline std::string resolve_yolo_export_output_path() {
    return resolve_fulltest_env_or_default("LIBTORCH_MODULE_YOLO_EXPORT_OUTPUT", "yolo_exported.pt");
}

int test_ModelConfigTest() {
    std::cout << "=== Testing ModelConfig ===" << std::endl;

    try {
        ModelConfig::get_config("invalid_type");
        std::cerr << "[FAIL] Failed to catch invalid model type" << std::endl;
    }
    catch (...) {
        std::cout << "[PASS] Caught invalid model type as expected" << std::endl;
    }

    const float eps = 1e-6f;

    ModelConfig nano_cfg = ModelConfig::get_config("nano");
    assert(std::abs(nano_cfg.width_multiple - 0.25f) < eps && "nano width error");
    assert(std::abs(nano_cfg.depth_multiple - 0.33f) < eps && "nano depth error");
    assert(nano_cfg.num_classes == 80 && "nano classes error");

    ModelConfig small_cfg = ModelConfig::get_config("small");
    assert(std::abs(small_cfg.width_multiple - 0.50f) < eps && "small width error");

    std::cout << "[PASS] ModelConfig validation passed" << std::endl;
    return 0;
}

int test_UtilsTest() {
    std::cout << "=== Testing Utils ===" << std::endl;

    assert(make_divisible(64, 8) == 64);
    assert(make_divisible(65, 8) == 72);

    std::vector<int64_t> base_channels = { 64, 128, 256 };
    float width_multiple = 0.25f;
    float depth_multiple = 0.33f;

    auto scaled_channels = scale_channels(base_channels, width_multiple);
    assert(scaled_channels[0] == 16);
    assert(scaled_channels[1] == 32);
    assert(scaled_channels[2] == 64);

    int scaled_blocks = scale_blocks(9, depth_multiple);
    assert(scaled_blocks == 3);

    std::cout << "[PASS] Utils validation passed" << std::endl;
    return 0;
}

int test_NetworkShapeTest() {
    std::cout << "=== Testing Network Shapes ===" << std::endl;
    auto device = get_full_test_device();

    if (device.is_cuda()) {
        std::cout << "CUDA available. Device count: " << torch::cuda::device_count() << std::endl;
        torch::Tensor t = torch::ones({ 2, 2 }, device);
        std::cout << "Tensor on GPU: " << t.device() << std::endl;
    }
    else {
        std::cout << "Running full tests on CPU." << std::endl;
    }

    try {
        Stem stem(3, 32);
        stem->to(device);
        torch::Tensor x = torch::randn({ 1, 3, 640, 640 }, device);
        auto out_stem = stem->forward(x);
        assert(out_stem.size(2) == 320 && out_stem.size(3) == 320 && out_stem.size(1) == 32);
        std::cout << "[PASS] Stem shape passed: " << out_stem.sizes() << std::endl;

        C2f c2f(32, 64, 2, true);
        c2f->to(device);
        auto out_c2f = c2f->forward(out_stem);
        assert(out_c2f.size(1) == 64 && out_c2f.size(2) == 320);
        std::cout << "[PASS] C2f shape passed: " << out_c2f.sizes() << std::endl;

        SPPF sppf(64, 64, 5);
        sppf->to(device);
        auto out_sppf = sppf->forward(out_c2f);
        assert(out_sppf.size(1) == 64 && out_sppf.size(2) == 320);
        std::cout << "[PASS] SPPF shape passed: " << out_sppf.sizes() << std::endl;

    }
    catch (const std::exception& e) {
        std::cerr << "[FAIL] Network shape test failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}

int test_AssignerTest() {
    std::cout << "=== Testing TaskAlignedAssigner ===" << std::endl;

    torch::Device device = get_full_test_device();

    int64_t num_classes = 80;
    TaskAlignedAssigner assigner(10, num_classes, 1.0f, 6.0f);
    assigner->to(device);

    int64_t batch_size = 2;
    int64_t num_anchors = 8400;

    auto pd_scores = torch::rand({ batch_size, num_anchors, num_classes }, device);
    auto pd_bboxes = torch::rand({ batch_size, num_anchors, 4 }, device) * 640;

    int64_t max_gt = 5;
    auto gt_labels = torch::randint(0, num_classes, { batch_size, max_gt, 1 }, device).to(torch::kLong);
    auto gt_bboxes = torch::rand({ batch_size, max_gt, 4 }, device) * 640;
    auto gt_mask = torch::ones({ batch_size, max_gt, 1 }, device);


    try {
        auto assigned_inds = assigner->forward(
            pd_scores.unsqueeze(1),
            pd_bboxes.unsqueeze(1),
            gt_labels,
            gt_bboxes,
            gt_mask
        );

        std::cout << "Assigned indices shape: " << assigned_inds.sizes() << std::endl;

        int pos_num = (assigned_inds > 0).sum().item<int>();
        std::cout << "Total positive samples: " << pos_num << std::endl;

    }
    catch (const std::exception& e) {
        std::cerr << "[FAIL] Assigner test failed: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "[PASS] Assigner test passed" << std::endl;
    return 0;
}

int test_LossTest() {
    std::cout << "=== Testing YOLOv8Loss ===" << std::endl;

    try {
        int64_t num_classes = 80;
        YOLOv8Loss loss_fn(num_classes);
        torch::Device device = get_full_test_device();
        loss_fn->to(device);

        int64_t B = 2;
        std::vector<torch::Tensor> preds;
        preds.push_back(torch::randn({ B, 6400, 4 + num_classes }, device));
        preds.push_back(torch::randn({ B, 1600, 4 + num_classes }, device));
        preds.push_back(torch::randn({ B, 400, 4 + num_classes }, device));

        int64_t N = 10;
        auto targets = torch::zeros({ B, N, 6 }, device);
        for (int b = 0; b < B; ++b) targets[b].select(1, 0).fill_(b);
        targets.select(2, 1).random_(0, num_classes);
        targets.slice(2, 2, 6).uniform_(0.1, 0.9);

        auto result = loss_fn->forward(preds, targets);
        torch::Tensor total_loss = std::get<0>(result);
        auto loss_items = std::get<1>(result);

        std::cout << "Total Loss: " << total_loss.item<float>() << std::endl;
        std::cout << "Box Loss: " << loss_items["box_loss"] << std::endl;
        std::cout << "Cls Loss: " << loss_items["cls_loss"] << std::endl;

        assert(total_loss.item<float>() > 0);
        assert(loss_items["box_loss"] >= 0);
        assert(loss_items["cls_loss"] >= 0);

    }
    catch (const std::exception& e) {
        std::cerr << "[FAIL] Loss test failed: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "[PASS] Loss test passed" << std::endl;
    return 0;
}

int test_YoloModelBuildConfig() {
    std::cout << "=== Testing YOLOv8 Build Config ===" << std::endl;

    try {
        auto device = get_full_test_device();
        ModelConfig cfg = ModelConfig::get_config("nano", 3);

        YoloModelBuildConfig build_cfg;
        build_cfg.head.use_dfl = true;
        build_cfg.head.reg_max = 8;
        build_cfg.loss.enable_dfl = true;
        build_cfg.loss.decode_strategy = YoloBoxDecodeStrategy::DirectStrideScaled;

        YOLOv8 model(cfg, build_cfg);
        model->to(device);
        model->eval();

        auto x = torch::randn({ 1, 3, 640, 640 }, device);
        auto pred = model->forward(x);

        assert(model->get_build_config().head.use_dfl);
        assert(model->get_build_config().loss.enable_dfl);
        assert(pred.sizes() == torch::IntArrayRef({ 1, 8400, 35 }));
    }
    catch (const std::exception& e) {
        std::cerr << "[FAIL] YOLOv8 build config test failed: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "[PASS] YOLOv8 build config passed" << std::endl;
    return 0;
}

int test_OpenCVImageStage() {
    std::cout << "=== Testing OpenCV Image Processing ===" << std::endl;

    DataAugmenter augmenter;

    cv::Mat img(100, 100, CV_8UC3, cv::Scalar(100, 100, 100));

    cv::Mat img_hsv = augmenter.hsv_augment(img);
    assert(!img_hsv.empty());

    cv::Mat img_resized = augmenter.resize_image(img, 64);
    assert(!img_resized.empty());

    std::cout << "[PASS] OpenCV image processing passed" << std::endl;
    return 0;
}

int test_OpenCVTensorContract() {
    std::cout << "=== Testing OpenCV BGR->RGB Tensor Contract ===" << std::endl;

    cv::Mat img_bgr(2, 2, CV_8UC3);
    img_bgr.at<cv::Vec3b>(0, 0) = cv::Vec3b(10, 20, 30);
    img_bgr.at<cv::Vec3b>(0, 1) = cv::Vec3b(40, 50, 60);
    img_bgr.at<cv::Vec3b>(1, 0) = cv::Vec3b(70, 80, 90);
    img_bgr.at<cv::Vec3b>(1, 1) = cv::Vec3b(100, 110, 120);

    cv::Mat img_rgb;
    cv::cvtColor(img_bgr, img_rgb, cv::COLOR_BGR2RGB);

    torch::Tensor img_tensor = torch::from_blob(img_rgb.data, { img_rgb.rows, img_rgb.cols, 3 }, torch::kUInt8)
        .permute({ 2, 0, 1 })
        .contiguous()
        .clone()
        .to(torch::kFloat32)
        .div_(255.0f);

    TORCH_CHECK(img_tensor.dim() == 3, "Expected CHW tensor");
    TORCH_CHECK(img_tensor.size(0) == 3 && img_tensor.size(1) == 2 && img_tensor.size(2) == 2,
        "Unexpected tensor shape: ", img_tensor.sizes());
    TORCH_CHECK(img_tensor.dtype() == torch::kFloat32, "Expected float32 tensor");
    TORCH_CHECK(img_tensor.min().item<float>() >= 0.0f && img_tensor.max().item<float>() <= 1.0f,
        "Expected normalized tensor in [0, 1]");

    const float r00 = img_tensor[0][0][0].item<float>();
    const float g00 = img_tensor[1][0][0].item<float>();
    const float b00 = img_tensor[2][0][0].item<float>();
    TORCH_CHECK(std::abs(r00 - (30.0f / 255.0f)) < 1e-6f, "Unexpected red channel value after BGR->RGB conversion");
    TORCH_CHECK(std::abs(g00 - (20.0f / 255.0f)) < 1e-6f, "Unexpected green channel value after conversion");
    TORCH_CHECK(std::abs(b00 - (10.0f / 255.0f)) < 1e-6f, "Unexpected blue channel value after conversion");

    std::cout << "[PASS] OpenCV tensor contract passed" << std::endl;
    return 0;
}

int test_OpenCVAnnotationStage() {
    std::cout << "=== Testing OpenCV Image+Annotation Sync ===" << std::endl;

    DataAugmenter augmenter;

    cv::Mat img(100, 100, CV_8UC3, cv::Scalar(100, 100, 100));

    std::vector<std::vector<float>> labels = {{0, 0.1f, 0.1f, 0.5f, 0.5f}};

    auto [img_flip, labels_flip] = augmenter.random_flip(img, labels);
    assert(!img_flip.empty());
    assert(labels_flip.size() == labels.size());
    assert(!labels_flip.empty());
    assert(std::abs(labels_flip[0][1] - (1.0f - labels[0][3])) < 1e-6f);
    assert(std::abs(labels_flip[0][3] - (1.0f - labels[0][1])) < 1e-6f);
    assert(labels_flip[0][1] >= 0.0f && labels_flip[0][3] <= 1.0f);
    assert(labels_flip[0][1] <= labels_flip[0][3]);

    std::cout << "[PASS] OpenCV image+annotation sync passed" << std::endl;
    return 0;
}

int test_YoloDatasetContract() {
    std::cout << "=== Testing YOLO Dataset Contract ===" << std::endl;

    const fs::path base_dir = fs::temp_directory_path() / "libtorch_module_yolo_dataset_contract";
    const fs::path img_dir = base_dir / "images";
    const fs::path label_dir = base_dir / "labels";
    fs::create_directories(img_dir);
    fs::create_directories(label_dir);

    const fs::path img_path = img_dir / "sample.jpg";
    const fs::path label_path = label_dir / "sample.txt";

    cv::Mat img(8, 8, CV_8UC3, cv::Scalar(16, 32, 64));
    TORCH_CHECK(cv::imwrite(img_path.string(), img), "Failed to write temporary test image");

    {
        std::ofstream label_file(label_path.string(), std::ios::trunc);
        TORCH_CHECK(label_file.is_open(), "Failed to write temporary label file");
        label_file << "3 0.5 0.5 0.4 0.2\n";
    }

    YoloDataset dataset(img_dir.string(), label_dir.string(), 32, false);
    TORCH_CHECK(dataset.size().value_or(0) == 1, "Expected one dataset sample");

    auto sample = dataset.get(0);
    TORCH_CHECK(sample.data.sizes() == torch::IntArrayRef({3, 32, 32}),
        "Unexpected image tensor shape: ", sample.data.sizes());
    TORCH_CHECK(sample.data.dtype() == torch::kFloat32, "Expected float32 image tensor");
    TORCH_CHECK(sample.data.min().item<float>() >= 0.0f && sample.data.max().item<float>() <= 1.0f,
        "Expected normalized image tensor");

    TORCH_CHECK(sample.target.sizes() == torch::IntArrayRef({50, 6}),
        "Unexpected target tensor shape: ", sample.target.sizes());
    TORCH_CHECK(sample.target[0][0].item<float>() == 0.0f, "Expected placeholder batch index");
    TORCH_CHECK(sample.target[0][1].item<float>() == 3.0f, "Expected class id 3");
    TORCH_CHECK(std::abs(sample.target[0][2].item<float>() - 0.3f) < 1e-5f, "Unexpected x1 value");
    TORCH_CHECK(std::abs(sample.target[0][3].item<float>() - 0.4f) < 1e-5f, "Unexpected y1 value");
    TORCH_CHECK(std::abs(sample.target[0][4].item<float>() - 0.7f) < 1e-5f, "Unexpected x2 value");
    TORCH_CHECK(std::abs(sample.target[0][5].item<float>() - 0.6f) < 1e-5f, "Unexpected y2 value");
    TORCH_CHECK(sample.target[1].eq(-1.0f).all().item<bool>(), "Expected padded rows after first target");

    fs::remove_all(base_dir);

    std::cout << "[PASS] YOLO dataset contract passed" << std::endl;
    return 0;
}

int test_YoloEvalSummaryContract() {
    std::cout << "=== Testing YOLO Eval Summary Contract ===" << std::endl;

    const fs::path base_dir = fs::temp_directory_path() / "libtorch_module_yolo_eval_contract";
    const fs::path img_dir = base_dir / "images" / "val";
    const fs::path label_dir = base_dir / "labels" / "val";
    fs::create_directories(img_dir);
    fs::create_directories(label_dir);

    const fs::path img_path = img_dir / "sample.jpg";
    const fs::path label_path = label_dir / "sample.txt";

    cv::Mat img(16, 16, CV_8UC3, cv::Scalar(32, 64, 96));
    TORCH_CHECK(cv::imwrite(img_path.string(), img), "Failed to write eval test image");

    {
        std::ofstream label_file(label_path.string(), std::ios::trunc);
        TORCH_CHECK(label_file.is_open(), "Failed to write eval test label");
        label_file << "1 0.5 0.5 0.25 0.25\n";
    }

    try {
        ModelConfig cfg = ModelConfig::get_config("nano", 3);
        YOLOv8 model(cfg);
        auto device = get_full_test_device();
        model->to(device);
        model->eval();

        YoloValidationConfig val_cfg;
        val_cfg.batch_size = 1;
        val_cfg.img_size = 32;
        val_cfg.max_gt = 10;

        YoloMainlineRunnerConfig runner_cfg;
        runner_cfg.model = cfg;
        runner_cfg.validation = val_cfg;
        runner_cfg.enable_smoke_train = false;
        runner_cfg.enable_eval = true;

        auto summary = run_yolo_eval_summary(model, base_dir.string(), runner_cfg);
        TORCH_CHECK(summary.loss >= 0.0f, "Validation summary loss must be non-negative");
        TORCH_CHECK(summary.predicted_boxes >= 0, "Validation summary predicted_boxes must be non-negative");
        TORCH_CHECK(summary.target_boxes >= 1, "Validation summary target_boxes must reflect GT labels");
        TORCH_CHECK(summary.map50_proxy >= 0.0f, "Validation summary score proxy must be non-negative");
        TORCH_CHECK(summary.precision >= 0.0f && summary.precision <= 1.0f, "Validation summary precision out of range");
        TORCH_CHECK(summary.recall >= 0.0f && summary.recall <= 1.0f, "Validation summary recall out of range");
        TORCH_CHECK(summary.f1 >= 0.0f && summary.f1 <= 1.0f, "Validation summary f1 out of range");
        TORCH_CHECK(summary.true_positives >= 0, "Validation summary TP must be non-negative");
        TORCH_CHECK(summary.false_positives >= 0, "Validation summary FP must be non-negative");
        TORCH_CHECK(summary.false_negatives >= 0, "Validation summary FN must be non-negative");
        TORCH_CHECK(summary.true_positives + summary.false_negatives == summary.target_boxes,
            "Validation summary TP/FN must partition target boxes");
        std::vector<TuningClassSummary> per_class_report;
        for (const auto& [cls, stats] : summary.per_class) {
            TORCH_CHECK(cls >= 0, "Validation summary per-class key must be non-negative");
            TORCH_CHECK(stats.predicted_boxes >= 0, "Validation summary per-class predicted count must be non-negative");
            TORCH_CHECK(stats.target_boxes >= 0, "Validation summary per-class target count must be non-negative");
            TORCH_CHECK(stats.true_positives >= 0, "Validation summary per-class TP must be non-negative");
            TORCH_CHECK(stats.false_positives >= 0, "Validation summary per-class FP must be non-negative");
            TORCH_CHECK(stats.false_negatives >= 0, "Validation summary per-class FN must be non-negative");
            TORCH_CHECK(stats.precision >= 0.0f && stats.precision <= 1.0f, "Validation summary per-class precision out of range");
            TORCH_CHECK(stats.recall >= 0.0f && stats.recall <= 1.0f, "Validation summary per-class recall out of range");
            TORCH_CHECK(stats.f1 >= 0.0f && stats.f1 <= 1.0f, "Validation summary per-class f1 out of range");
            TORCH_CHECK(stats.true_positives + stats.false_negatives == stats.target_boxes,
                "Validation summary per-class TP/FN must partition class targets");
            per_class_report.push_back({
                cls,
                stats.predicted_boxes,
                stats.target_boxes,
                stats.true_positives,
                stats.false_positives,
                stats.false_negatives,
                stats.precision,
                stats.recall,
                stats.f1
            });
        }

        auto eval_report = build_yolo_eval_run_report(
            summary.loss,
            summary.precision,
            summary.recall,
            summary.f1,
            summary.matched_iou,
            summary.true_positives,
            summary.false_positives,
            summary.false_negatives,
            summary.predicted_boxes,
            summary.target_boxes,
            per_class_report);
        eval_report.validate();
        std::cout << "Eval report summary: " << eval_report.run.summary << std::endl;
        for (const auto& cls : eval_report.per_class) {
            std::cout << "  Eval report [Class " << cls.class_id << "]"
                      << " Pred=" << cls.predicted_boxes
                      << " Target=" << cls.target_boxes
                      << " TP=" << cls.true_positives
                      << " FP=" << cls.false_positives
                      << " FN=" << cls.false_negatives
                      << " Precision=" << cls.precision
                      << " Recall=" << cls.recall
                      << " F1=" << cls.f1
                      << std::endl;
        }
    }
    catch (...) {
        fs::remove_all(base_dir);
        throw;
    }

    fs::remove_all(base_dir);
    std::cout << "[PASS] YOLO eval summary contract passed" << std::endl;
    return 0;
}

int test_TrainDebug() {
    std::cout << "=== Testing Training Loop Debug ===" << std::endl;

    try {
        ModelConfig cfg = ModelConfig::get_config("nano");
        YOLOv8 model(cfg);

        torch::Device device = get_full_test_device();
        model->to(device);

        int64_t B = 2;
        auto imgs = torch::randn({ B, 3, 640, 640 }, device);
        auto targets = torch::zeros({ B, 5, 6 }, device);
        for (int b = 0; b < B; ++b) targets[b].select(1, 0).fill_(b);
        targets.slice(2, 2, 6).uniform_(0.1, 0.9);

        TrainConfig train_cfg;
        train_cfg.batch_size = static_cast<int>(B);
        train_cfg.img_size = 640;
        auto runner_cfg = make_yolo_mainline_runner_config(cfg, train_cfg, "", 2);
        runner_cfg.enable_eval = false;

        auto smoke_step = run_yolo_smoke_train_step(model, imgs, targets, runner_cfg);
        smoke_step.validate();
        bool has_grad = smoke_step.observation.gradients_ok;
        float grad_mean = static_cast<float>(smoke_step.grad_mean);
        std::cout << "Param grad mean: " << grad_mean << std::endl;

        try {
            torch::save(model, "y8_model.pt");
            std::cout << "Model saved to y8_model.pt" << std::endl;
        }
        catch (const std::exception& e) {
            std::cerr << "Failed to save model: " << e.what() << std::endl;
        }

        const float step_loss = static_cast<float>(smoke_step.observation.step_loss);
        std::cout << "Step Loss: " << step_loss << std::endl;

        auto smoke_report = build_yolo_smoke_run_report(smoke_step.observation);
        smoke_report.validate();

        const auto& outcome = smoke_report.outcomes.front();
        std::cout << "Smoke report summary: " << smoke_report.summary << std::endl;
        std::cout << "Smoke report stage: " << outcome.stage_name
                  << " loss=" << outcome.metrics.front().value
                  << " max_train_batches=" << outcome.selected_knobs[2].second
                  << " gradients_ok=" << (has_grad ? "yes" : "no")
                  << std::endl;
        std::cout << "[PASS] Training loop debug passed" << std::endl;
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "[FAIL] Train debug failed: " << e.what() << std::endl;
        return 1;
    }
}

int test_YoloMainlineRoundReport() {
    std::cout << "=== Testing YOLOv8 Mainline Round Report ===" << std::endl;

    try {
        YoloSmokeObservation smoke_observation;
        smoke_observation.input_size = 640;
        smoke_observation.batch_size = 2;
        smoke_observation.max_train_batches = 2;
        smoke_observation.step_loss = 12.5;
        smoke_observation.gradients_ok = true;

        YOLOv8Impl::ValidationSummary eval_summary;
        eval_summary.loss = 9.25f;
        eval_summary.precision = 0.6666667f;
        eval_summary.recall = 1.0f;
        eval_summary.f1 = 0.8f;
        eval_summary.matched_iou = 0.71f;
        eval_summary.true_positives = 2;
        eval_summary.false_positives = 1;
        eval_summary.false_negatives = 0;
        eval_summary.predicted_boxes = 3;
        eval_summary.target_boxes = 2;
        eval_summary.per_class[1] = {3, 2, 2, 1, 0, 0.6666667f, 1.0f, 0.8f};

        auto bundle = build_yolo_mainline_bundle(smoke_observation, eval_summary);
        bundle.validate();

        TORCH_CHECK(bundle.round_report.round_passed, "YOLO mainline round report should pass");
        TORCH_CHECK(bundle.round_report.eval_run.per_class.size() == 1, "YOLO mainline round per-class size mismatch");
        std::cout << "Mainline round summary: " << bundle.round_report.summary << std::endl;
        std::cout << "Mainline flat run: " << bundle.flat_run.run_name
                  << " outcomes=" << bundle.flat_run.outcomes.size()
                  << " passed=" << (bundle.flat_run.all_passed ? "yes" : "no")
                  << std::endl;
        std::cout << "Mainline comparison rows: " << bundle.comparison_rows.size()
                  << " first_stage=" << bundle.comparison_rows.front().stage_name
                  << " second_stage=" << bundle.comparison_rows.back().stage_name
                  << std::endl;
        std::cout << "Mainline recommendation: " << bundle.recommendation.track_name
                  << " selections=" << bundle.recommendation.selected_experiments.size()
                  << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "[FAIL] YOLO mainline round report failed: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "[PASS] YOLOv8 mainline round report passed" << std::endl;
    return 0;
}

int test_YoloMainlineRunnerConfig() {
    std::cout << "=== Testing YOLOv8 Mainline Runner Config ===" << std::endl;

    try {
        TrainConfig train_cfg;
        train_cfg.epochs = 1;
        train_cfg.batch_size = 2;
        train_cfg.img_size = 640;
        train_cfg.max_gt = 32;
        train_cfg.resize_policy = YoloResizePolicy::Letterbox;

        auto model_cfg = ModelConfig::get_config("nano", 3);
        YoloModelBuildConfig build_cfg;
        build_cfg.head.use_dfl = true;
        build_cfg.head.reg_max = 8;
        build_cfg.loss.enable_dfl = true;

        auto runner_cfg = make_yolo_mainline_runner_config(
            model_cfg,
            train_cfg,
            "",
            2,
            build_cfg);
        runner_cfg.device_policy = YoloDevicePolicy::ForceCPU;
        runner_cfg.optimizer_stage.warmup_steps = 2;
        runner_cfg.scheduler_stage.policy = YoloSchedulerPolicy::StepDecay;
        runner_cfg.scheduler_stage.step_size = 1;
        runner_cfg.scheduler_stage.gamma = 0.5f;
        runner_cfg.scheduler_stage.total_steps = 4;
        runner_cfg.validate();

        YoloSmokeObservation smoke_observation;
        smoke_observation.input_size = train_cfg.img_size;
        smoke_observation.batch_size = train_cfg.batch_size;
        smoke_observation.max_train_batches = runner_cfg.runtime.max_train_batches;
        smoke_observation.step_loss = 8.75;
        smoke_observation.gradients_ok = true;

        YOLOv8Impl::ValidationSummary eval_summary;
        eval_summary.loss = 6.5f;
        eval_summary.precision = 0.5f;
        eval_summary.recall = 1.0f;
        eval_summary.f1 = 0.6666667f;
        eval_summary.matched_iou = 0.72f;
        eval_summary.true_positives = 1;
        eval_summary.false_positives = 1;
        eval_summary.false_negatives = 0;
        eval_summary.predicted_boxes = 2;
        eval_summary.target_boxes = 1;
        eval_summary.per_class[1] = {2, 1, 1, 1, 0, 0.5f, 1.0f, 0.6666667f};

        auto execution_result = build_yolo_mainline_execution_result(
            runner_cfg,
            smoke_observation,
            eval_summary);
        execution_result.validate();

        TORCH_CHECK(execution_result.config.runtime.max_train_batches == 2,
            "YOLO runner config should preserve smoke batch limit");
          TORCH_CHECK(execution_result.config.validation.resize_policy == YoloResizePolicy::Letterbox,
              "YOLO runner config should preserve validation resize policy");
          TORCH_CHECK(execution_result.config.build.head.use_dfl,
              "YOLO runner config should preserve DFL head flag");
          TORCH_CHECK(execution_result.config.optimizer_stage.warmup_steps == 2,
              "YOLO runner config should preserve optimizer warmup steps");
          TORCH_CHECK(execution_result.config.scheduler_stage.policy == YoloSchedulerPolicy::StepDecay,
              "YOLO runner config should preserve scheduler policy");
          TORCH_CHECK(execution_result.bundle.round_report.round_passed,
              "YOLO execution result should produce a passing round report");

        std::cout << "Runner config summary: run=" << execution_result.config.run_name
                  << " smoke_batches=" << execution_result.config.runtime.max_train_batches
                  << " resize_policy="
                  << (execution_result.config.validation.resize_policy == YoloResizePolicy::Letterbox
                        ? "letterbox" : "plain")
                  << " dfl=" << (execution_result.config.build.head.use_dfl ? "on" : "off")
                  << " optimizer_lr=" << execution_result.config.optimizer.lr
                  << " warmup_steps=" << execution_result.config.optimizer_stage.warmup_steps
                  << " scheduler=" << yolo_scheduler_policy_name(execution_result.config.scheduler_stage.policy)
                  << " device_policy=" << yolo_device_policy_name(execution_result.config.device_policy)
                  << " reuse_eval=" << (execution_result.config.reuse_trained_model_for_eval ? "yes" : "no")
                  << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "[FAIL] YOLO mainline runner config failed: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "[PASS] YOLOv8 mainline runner config passed" << std::endl;
    return 0;
}

int test_YoloGpuSmoke() {
    std::cout << "=== Testing YOLOv8 GPU Smoke ===" << std::endl;

    if (!torch::cuda::is_available()) {
        std::cout << "SKIP CUDA not available for YOLOv8 GPU smoke" << std::endl;
        return 0;
    }

    try {
        auto device = torch::Device(torch::kCUDA);
        auto model_cfg = ModelConfig::get_config("nano", 3);
        YOLOv8 model(model_cfg);
        model->to(device);

        TrainConfig train_cfg;
        train_cfg.batch_size = 2;
        train_cfg.img_size = 64;
        train_cfg.max_gt = 4;

        auto runner_cfg = make_yolo_mainline_runner_config(model_cfg, train_cfg, "", 1);
        runner_cfg.enable_eval = false;
        runner_cfg.optimizer.lr = 1e-4f;
        runner_cfg.optimizer_stage.base = runner_cfg.optimizer;
        runner_cfg.device_policy = YoloDevicePolicy::ForceCUDA;

        auto imgs = torch::randn({2, 3, 64, 64}, device);
        auto targets = torch::full({2, 4, 6}, -1.0f, device);
        for (int b = 0; b < 2; ++b) {
            targets[b][0] = torch::tensor(
                {static_cast<float>(b), 1.0f, 0.20f, 0.20f, 0.60f, 0.60f},
                torch::TensorOptions().dtype(torch::kFloat32).device(device));
        }

        auto smoke_step = run_yolo_smoke_train_step(model, imgs, targets, runner_cfg);
        smoke_step.validate();

        auto pred = model->forward(imgs);
        TORCH_CHECK(pred.device().is_cuda(), "YOLO GPU smoke prediction must stay on CUDA");
        TORCH_CHECK(torch::isfinite(pred).all().item<bool>(), "YOLO GPU smoke prediction must be finite");

        std::cout << "YOLO GPU smoke device: " << pred.device() << std::endl;
        std::cout << "YOLO GPU smoke loss: " << smoke_step.observation.step_loss << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "[FAIL] YOLOv8 GPU smoke failed: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "[PASS] YOLOv8 GPU smoke passed" << std::endl;
    return 0;
}

int test_YoloMainlineSession() {
    std::cout << "=== Testing YOLOv8 Mainline Session ===" << std::endl;

    const fs::path base_dir = fs::temp_directory_path() / "libtorch_module_yolo_mainline_session";
    const fs::path img_dir = base_dir / "images" / "val";
    const fs::path label_dir = base_dir / "labels" / "val";
    fs::create_directories(img_dir);
    fs::create_directories(label_dir);

    const fs::path img_path = img_dir / "sample.jpg";
    const fs::path label_path = label_dir / "sample.txt";

    cv::Mat img(32, 32, CV_8UC3, cv::Scalar(24, 48, 96));
    TORCH_CHECK(cv::imwrite(img_path.string(), img), "Failed to write YOLO mainline session image");

    {
        std::ofstream label_file(label_path.string(), std::ios::trunc);
        TORCH_CHECK(label_file.is_open(), "Failed to write YOLO mainline session label");
        label_file << "1 0.5 0.5 0.25 0.25\n";
    }

    try {
        try {
            auto device = get_full_test_device();
            auto model_cfg = ModelConfig::get_config("nano", 3);
            YOLOv8 model(model_cfg);
            model->to(device);

            TrainConfig train_cfg;
            train_cfg.batch_size = 2;
            train_cfg.img_size = 32;
            train_cfg.max_gt = 10;
            train_cfg.dataloader_workers = 0;
            train_cfg.resize_policy = YoloResizePolicy::Letterbox;

            auto runner_cfg = make_yolo_mainline_runner_config(model_cfg, train_cfg, "", 2);
            runner_cfg.enable_smoke_train = true;
            runner_cfg.enable_eval = true;
            runner_cfg.optimizer.lr = 1e-4f;
            runner_cfg.optimizer_stage.base = runner_cfg.optimizer;
            runner_cfg.optimizer_stage.warmup_steps = 1;
            runner_cfg.scheduler_stage.policy = YoloSchedulerPolicy::StepDecay;
            runner_cfg.scheduler_stage.step_size = 1;
            runner_cfg.scheduler_stage.gamma = 0.5f;
            runner_cfg.scheduler_stage.total_steps = 2;
            runner_cfg.device_policy = device.is_cuda()
                ? YoloDevicePolicy::ForceCUDA
                : YoloDevicePolicy::ForceCPU;

            auto imgs = torch::randn({2, 3, 32, 32}, device);
            auto targets = torch::full({2, 5, 6}, -1.0f, device);
            for (int b = 0; b < 2; ++b) {
                targets[b][0] = torch::tensor(
                    {static_cast<float>(b), 1.0f, 0.20f, 0.20f, 0.60f, 0.60f},
                    torch::TensorOptions().dtype(torch::kFloat32).device(device));
            }

            auto session = run_yolo_mainline_session(model, imgs, targets, base_dir.string(), runner_cfg);
            session.validate();

            const auto& eval_outcome = session.execution.bundle.round_report.eval_run.run.outcomes.front();
            TORCH_CHECK(
                std::isfinite(eval_outcome.metrics.front().value),
                "YOLO mainline session eval loss must remain finite");
            TORCH_CHECK(session.execution.bundle.round_report.round_passed,
                "YOLO mainline session should produce a passing round report");
            TORCH_CHECK(!session.execution.bundle.round_report.eval_run.per_class.empty(),
                "YOLO mainline session should preserve per-class evaluation summary");
            TORCH_CHECK(
                session.execution.bundle.round_report.eval_run.per_class.front().target_boxes >= 0,
                "YOLO mainline session per-class target count must be non-negative");

            bool has_label_class = false;
            for (const auto& cls : session.execution.bundle.round_report.eval_run.per_class) {
                if (cls.class_id == 1) {
                    TORCH_CHECK(cls.target_boxes == 1,
                        "YOLO mainline session should keep one target for the labeled class");
                    has_label_class = true;
                }
            }
            TORCH_CHECK(has_label_class, "YOLO mainline session should preserve the labeled class summary");

            std::cout << "Mainline session summary: " << session.execution.bundle.round_report.summary << std::endl;
            std::cout << "Mainline session smoke loss: " << session.smoke_step.observation.step_loss
                      << " eval_classes=" << session.execution.bundle.round_report.eval_run.per_class.size()
                      << " scheduler=" << yolo_scheduler_policy_name(session.execution.config.scheduler_stage.policy)
                      << " device_policy=" << yolo_device_policy_name(session.execution.config.device_policy)
                      << std::endl;
        }
        catch (...) {
            fs::remove_all(base_dir);
            throw;
        }

        fs::remove_all(base_dir);
        std::cout << "[PASS] YOLOv8 mainline session passed" << std::endl;
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "[FAIL] YOLO mainline session failed: " << e.what() << std::endl;
        return 1;
    }
}

int test_YoloTrainerSession() {
    std::cout << "=== Testing YOLOv8 Trainer Session ===" << std::endl;

    const fs::path base_dir = fs::temp_directory_path() / "libtorch_module_yolo_trainer_session";
    const fs::path img_dir = base_dir / "images" / "val";
    const fs::path label_dir = base_dir / "labels" / "val";
    fs::create_directories(img_dir);
    fs::create_directories(label_dir);

    const fs::path img_path = img_dir / "sample.jpg";
    const fs::path label_path = label_dir / "sample.txt";

    cv::Mat img(32, 32, CV_8UC3, cv::Scalar(24, 48, 96));
    TORCH_CHECK(cv::imwrite(img_path.string(), img), "Failed to write YOLO trainer session image");

    {
        std::ofstream label_file(label_path.string(), std::ios::trunc);
        TORCH_CHECK(label_file.is_open(), "Failed to write YOLO trainer session label");
        label_file << "1 0.5 0.5 0.25 0.25\n";
    }

    try {
        try {
            auto device = get_full_test_device();
            auto model_cfg = ModelConfig::get_config("nano", 3);
            YOLOv8 model(model_cfg);
            model->to(device);

            TrainConfig train_cfg;
            train_cfg.batch_size = 2;
            train_cfg.img_size = 32;
            train_cfg.max_gt = 10;
            train_cfg.dataloader_workers = 0;
            train_cfg.resize_policy = YoloResizePolicy::Letterbox;

            auto runner_cfg = make_yolo_mainline_runner_config(model_cfg, train_cfg, "", 2);
            runner_cfg.enable_smoke_train = true;
            runner_cfg.enable_eval = true;
            runner_cfg.optimizer.lr = 1e-4f;
            runner_cfg.optimizer_stage.base = runner_cfg.optimizer;
            runner_cfg.optimizer_stage.warmup_steps = 1;
            runner_cfg.scheduler_stage.policy = YoloSchedulerPolicy::StepDecay;
            runner_cfg.scheduler_stage.step_size = 1;
            runner_cfg.scheduler_stage.gamma = 0.5f;
            runner_cfg.scheduler_stage.total_steps = 2;
            runner_cfg.device_policy = device.is_cuda()
                ? YoloDevicePolicy::ForceCUDA
                : YoloDevicePolicy::ForceCPU;

            auto imgs = torch::randn({2, 3, 32, 32}, device);
            auto targets = torch::full({2, 5, 6}, -1.0f, device);
            for (int b = 0; b < 2; ++b) {
                targets[b][0] = torch::tensor(
                    {static_cast<float>(b), 1.0f, 0.20f, 0.20f, 0.60f, 0.60f},
                    torch::TensorOptions().dtype(torch::kFloat32).device(device));
            }

            auto trainer_session = run_yolo_trainer_session(model, imgs, targets, base_dir.string(), runner_cfg);
            trainer_session.validate();
            auto trainer_timeline = build_yolo_trainer_timeline(trainer_session);
            trainer_timeline.validate();
            auto trainer_analysis = build_yolo_trainer_analysis(trainer_session);
            trainer_analysis.validate();
            auto unified_bundle = build_yolo_unified_mainline_bundle(
                trainer_session.session.execution.bundle,
                trainer_analysis);
            unified_bundle.validate();
            auto unified_summary = build_yolo_unified_mainline_summary(unified_bundle);
            unified_summary.validate();

            TORCH_CHECK(trainer_session.passed, "YOLO trainer session should pass");
            TORCH_CHECK(trainer_session.summary.find("completed smoke training and evaluation") != std::string::npos,
                "YOLO trainer session summary mismatch");
            TORCH_CHECK(trainer_timeline.stages.size() == 7,
                "YOLO trainer timeline should contain resume/export/logging/checkpoint/smoke/eval/report stages");
            TORCH_CHECK(trainer_analysis.comparison_rows.size() == 2,
                "YOLO trainer analysis should contain smoke/eval comparison rows");
            TORCH_CHECK(trainer_analysis.lifecycle_summary.all_passed,
                "YOLO trainer lifecycle summary should report a passing lifecycle");
            TORCH_CHECK(trainer_analysis.flat_run.outcomes.size() == 2,
                "YOLO trainer flat run should contain smoke/eval outcomes");
            TORCH_CHECK(unified_bundle.comparison_rows.size() == 4,
                "YOLO unified mainline bundle should contain mainline and trainer comparison rows");
            TORCH_CHECK(unified_bundle.flat_run.outcomes.size() == 4,
                "YOLO unified mainline bundle should aggregate mainline and trainer outcomes");
            TORCH_CHECK(unified_summary.all_passed,
                "YOLO unified mainline summary should report a passing bundle");

            std::cout << "Trainer session summary: " << trainer_session.summary << std::endl;
            std::cout << "Trainer session run: " << trainer_session.config.run_name
                      << " scheduler=" << yolo_scheduler_policy_name(trainer_session.config.scheduler_stage.policy)
                      << " device_policy=" << yolo_device_policy_name(trainer_session.config.device_policy)
                      << " smoke_loss=" << trainer_session.session.smoke_step.observation.step_loss
                      << std::endl;
            for (const auto& stage : trainer_timeline.stages) {
                std::cout << "Trainer stage: " << stage.stage_name
                          << " passed=" << (stage.passed ? "yes" : "no")
                          << " detail=" << stage.detail
                          << std::endl;
            }
            std::cout << "Trainer comparison rows: " << trainer_analysis.comparison_rows.size()
                      << " first_stage=" << trainer_analysis.comparison_rows.front().stage_name
                      << " second_stage=" << trainer_analysis.comparison_rows.back().stage_name
                      << std::endl;
            std::cout << "Trainer recommendation: " << trainer_analysis.recommendation.track_name
                      << " selections=" << trainer_analysis.recommendation.selected_experiments.size()
                      << std::endl;
            std::cout << "Trainer lifecycle summary: " << trainer_analysis.lifecycle_summary.summary << std::endl;
            std::cout << "Trainer flat run: " << trainer_analysis.flat_run.run_name
                      << " outcomes=" << trainer_analysis.flat_run.outcomes.size()
                      << " passed=" << (trainer_analysis.flat_run.all_passed ? "yes" : "no")
                      << std::endl;
            std::cout << "Unified mainline bundle: " << unified_bundle.flat_run.run_name
                      << " outcomes=" << unified_bundle.flat_run.outcomes.size()
                      << " comparisons=" << unified_bundle.comparison_rows.size()
                      << " selections=" << unified_bundle.recommendation.selected_experiments.size()
                      << std::endl;
            std::cout << "Unified mainline summary: " << unified_summary.summary << std::endl;
        }
        catch (...) {
            fs::remove_all(base_dir);
            throw;
        }

        fs::remove_all(base_dir);
        std::cout << "[PASS] YOLOv8 trainer session passed" << std::endl;
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "[FAIL] YOLO trainer session failed: " << e.what() << std::endl;
        return 1;
    }
}

int test_YoloSmokeRuntimeConfig() {
    std::cout << "=== Testing YOLOv8 Smoke Runtime Config ===" << std::endl;

    TrainConfig train_cfg;
    train_cfg.epochs = 1;
    train_cfg.batch_size = 2;
    train_cfg.save_interval = 1;

    auto runtime_cfg = train_cfg.smoke_runtime_config("", 2);
    runtime_cfg.validate();

    TORCH_CHECK(runtime_cfg.log_interval == 1, "Smoke runtime log interval mismatch");
    TORCH_CHECK(runtime_cfg.max_train_batches == 2, "Smoke runtime max_train_batches mismatch");
    TORCH_CHECK(runtime_cfg.checkpoint.save_interval == 1, "Smoke runtime checkpoint interval mismatch");

    YoloTrainProgress progress;
    TORCH_CHECK(!should_stop_yolo_train_epoch(progress, runtime_cfg), "Smoke runtime should not stop before any batch");
    progress.record_batch(1.0f);
    TORCH_CHECK(!should_stop_yolo_train_epoch(progress, runtime_cfg), "Smoke runtime should not stop after first batch");
    progress.record_batch(0.9f);
    TORCH_CHECK(should_stop_yolo_train_epoch(progress, runtime_cfg), "Smoke runtime should stop at configured batch count");

    std::cout << "[PASS] YOLOv8 smoke runtime config passed" << std::endl;
    return 0;
}

int test_Weight_Loading() {
    std::cout << "=== Testing Weight Parser ===" << std::endl;

    try {
        ModelConfig cfg = ModelConfig::get_config("nano");
        YOLOv8 model(cfg);

        std::cout << "Sample C++ Keys:" << std::endl;
        int count = 0;
        for (auto& pair : model->named_parameters()) {
            if (count++ < 5) {
                std::cout << "  " << pair.key() << std::endl;
            }
        }

        std::string weight_path = resolve_yolo_weight_path();
        if (!fs::exists(weight_path)) {
            std::cout << "SKIP missing YOLO weights: " << weight_path << std::endl;
            return 0;
        }

        WeightParser parser(weight_path);

        parser.load_to(*model);

        model->eval();
        torch::Tensor x = torch::randn({ 1, 3, 640, 640 });
        auto out = model->forward(x);

        std::cout << "Inference Output Shape: " << out.sizes() << std::endl;
        std::cout << "Output Mean: " << out.mean().item<float>() << std::endl;

        std::cout << "[PASS] Weight loading test completed." << std::endl;
    }
    catch (const c10::Error& e) {
        std::cerr << "Error loading weights: " << e.what() << std::endl;
        return 1;
    }
    return 0;

}
int test_ResNet_Load()
{
    bool ran_any = false;
    try {
        std::cout << "=== Testing ResNet18 Loading ===" << std::endl;
        ResNet18 r18(1000, true);
        const std::string resnet18_weights = resolve_resnet18_weight_path();
        if (!fs::exists(resnet18_weights)) {
            std::cout << "SKIP missing ResNet18 weights: " << resnet18_weights << std::endl;
        } else {
            r18->load_weights(resnet18_weights);

            r18->eval();
            torch::Tensor x = torch::randn({ 1, 3, 224, 224 });
            auto out = r18->forward(x);
            auto probs = torch::softmax(out, 1);
            std::cout << "Top val: " << std::get<0>(probs.max(1)).item<float>() << std::endl;

            r18->export_structure();
            ran_any = true;
        }
    }
    catch (const c10::Error& e) {
        std::cerr << "Error loading ResNet18: " << e.what() << std::endl;
        return 1;
    }
    try {
        std::cout << "\n=== Testing ResNet50 Loading ===" << std::endl;
        ResNet50 r50(1000, true);
        const std::string resnet50_weights = resolve_resnet50_weight_path();
        if (!fs::exists(resnet50_weights)) {
            std::cout << "SKIP missing ResNet50 weights: " << resnet50_weights << std::endl;
        } else {
            r50->load_weights(resnet50_weights);

            r50->eval();
            torch::Tensor x = torch::randn({ 1, 3, 224, 224 });
            auto out = r50->forward(x);
            std::cout << "R50 Output shape: " << out.sizes() << std::endl;
            r50->export_structure();
            ran_any = true;
        }
    }
    catch (const c10::Error& e) {
        std::cerr << "Error loading ResNet18: " << e.what() << std::endl;
        return 1;
    }

    if (!ran_any) {
        std::cout << "SKIP no ResNet weight files available" << std::endl;
    }
    return 0;
}
int test_Inspect_Load()
{
    bool ran_any = false;

    try {
        std::cout << "=== Testing Inspect small bone Loading ===" << std::endl;
        ResNet18 r18(1000, true);
        const std::string small_backbone_weights = resolve_inspect_small_backbone_weight_path();
        if (!fs::exists(small_backbone_weights)) {
            std::cout << "SKIP missing inspect small-backbone weights: "
                      << small_backbone_weights << std::endl;
        } else {
            r18->load_weights(small_backbone_weights);

            r18->eval();
            torch::Tensor x = torch::randn({ 1, 3, 224, 224 });
            auto out = r18->forward(x);
            auto probs = torch::softmax(out, 1);
            std::cout << "Top val: " << std::get<0>(probs.max(1)).item<float>() << std::endl;
            r18->export_structure("inspectai_small_bone.md");
            ran_any = true;
        }
    }
    catch (const c10::Error& e) {
        std::cerr << "Error loading  Inspect small bone : " << e.what() << std::endl;
    }
    try {
        std::cout << "\n=== Testing Inspect large backbone weights Loading ===" << std::endl;
        ResNet50 r50(1000, true);
        const std::string large_backbone_weights = resolve_inspect_large_backbone_weight_path();
        if (!fs::exists(large_backbone_weights)) {
            std::cout << "SKIP missing inspect large-backbone weights: "
                      << large_backbone_weights << std::endl;
        } else {
            r50->load_weights(large_backbone_weights);

            r50->eval();
            torch::Tensor x = torch::randn({ 1, 3, 224, 224 });
            auto out = r50->forward(x);
            std::cout << "Inspect large backbone Output shape: " << out.sizes() << std::endl;
            r50->export_structure("inspectai_large_bone.md");
            ran_any = true;
        }
    }
    catch (const c10::Error& e) {
        std::cerr << "Error loading large backbone: " << e.what() << std::endl;
        return 1;
    }

    if (!ran_any) {
        std::cout << "SKIP no inspect backbone weight files available" << std::endl;
    }
    return 0;
}

int test_amp_training() {
    torch::Device device = get_full_test_device();
    std::cout << "Training on: " << (device.is_cuda() ? "GPU" : "CPU") << std::endl;

    ResNet18 model(10);
    model->to(device);

    torch::optim::SGD optimizer(
        model->parameters(),
        torch::optim::SGDOptions(0.01).momentum(0.9)
    );

    ManualGradScaler scaler;

    auto imgs = torch::randn({ 16, 3, 224, 224 }, device);
    auto targets = torch::randint(0, 10, { 16 }, torch::kLong).to(device);

    for (int i = 0; i < 2; ++i) {
        float loss = model->train_step_amp(model, optimizer, scaler, imgs, targets, device);

        std::cout << "Step " << i << ", Loss: " << loss
            << ", Scale: " << scaler.get_scale() << std::endl;
    }

    return 0;
}

int test_Transfer_Learning() {
    std::string data_dir = resolve_transfer_learning_data_root();
    std::string pretrained_path = resolve_resnet18_weight_path();
    int batch_size = 32;
    int num_epochs = 2;
    int num_classes = 1;
    float learning_rate = 0.001;

    torch::Device device = get_full_test_device();
    std::cout << "Using device: " << (device.is_cuda() ? "CUDA" : "CPU") << std::endl;

    if (!fs::exists(pretrained_path)) {
        std::cout << "SKIP missing pretrained weights: " << pretrained_path << std::endl;
        return 0;
    }

    if (!fs::exists(data_dir)) {
        std::cout << "SKIP missing transfer-learning data root: " << data_dir << std::endl;
        return 0;
    }

    std::cout << "Warning: Using mock data for demonstration." << std::endl;

    ResNet18 model(1000);
    model->to(device);

    model->load_pretrained_and_reset_head(pretrained_path, num_classes);


    torch::optim::SGD optimizer(model->parameters(), torch::optim::SGDOptions(learning_rate).momentum(0.9));
    ManualGradScaler  scaler;

    for (int epoch = 0; epoch < num_epochs; ++epoch) {
        model->train();
        float epoch_loss = 0.0;
        int batches = 0;

        for (int i = 0; i < 2; ++i) {
            auto imgs = torch::randn({ batch_size, 3, 224, 224 }, device);
            auto targets = torch::randint(0, num_classes, { batch_size }, device);

            float loss = model->train_step_amp(model, optimizer, scaler, imgs, targets, device);
            epoch_loss += loss;
            batches++;

            std::cout << "\rEpoch " << epoch << " Batch " << i << " Loss: " << loss << std::flush;
        }

        std::cout << "\nEpoch " << epoch << " Avg Loss: " << epoch_loss / batches << std::endl;


        torch::save(model, "resnet18_finetuned.pt");
    }

    return 0;
}

int test_TrainTest() {

    TrainConfig train_config;
    train_config.data_path = resolve_yolo_data_root();
    train_config.epochs = 1;
    train_config.save_interval = 1;

    std::cout << "Starting training..." << std::endl;
    try{
        ModelConfig model_cfg = ModelConfig::get_config("nano");

        YOLOv8 model(model_cfg);
        torch::Device device = get_full_test_device();
        model->to(device);

        std::cout << "Model initialized on " << (device.is_cuda() ? "GPU" : "CPU") << std::endl;

        std::string weight_path = resolve_yolo_weight_path();
        const std::string train_img_dir = resolve_yolo_train_image_dir(train_config.data_path);
        const std::string train_label_dir = resolve_yolo_train_label_dir(train_config.data_path);

        if (!fs::exists(weight_path)) {
            std::cout << "SKIP missing YOLO weights: " << weight_path << std::endl;
            return 0;
        }
        if (!fs::exists(train_img_dir) || !fs::exists(train_label_dir)) {
            std::cout << "SKIP missing YOLO dataset dirs: " << train_img_dir << " / " << train_label_dir << std::endl;
            return 0;
        }

        WeightParser parser(weight_path);

        parser.load_to(*model);

        auto dataset = YoloDataset(
            train_img_dir,
            train_label_dir,
            640,
            true
        ).map(torch::data::transforms::Stack<>());

        auto data_loader = torch::data::make_data_loader(
            std::move(dataset),
            torch::data::DataLoaderOptions().batch_size(train_config.batch_size).workers(0)
        );

        torch::optim::SGD optimizer(
            model->parameters(),
            torch::optim::SGDOptions(train_config.lr).momentum(0.937).weight_decay(train_config.weight_decay)
        );

        for (int epoch = 0; epoch < train_config.epochs; ++epoch) {

            TrainConfig tcg;
            model->train(tcg);
            float epoch_loss = 0.0f;
            int batch_count = 0;

            for (auto& batch : *data_loader) {
                auto imgs = batch.data.to(device);
                auto targets = batch.target.to(device);

                for (int b = 0; b < targets.size(0); ++b) {
                    targets[b].select(1, 0).fill_(static_cast<float>(b));
                }

                optimizer.zero_grad();

                auto result = model->train_step(imgs, targets);
                torch::Tensor total_loss = std::get<0>(result);

                total_loss.backward();
                optimizer.step();

                epoch_loss += total_loss.item<float>();
                batch_count++;

                if (batch_count % 10 == 0) {
                    std::cout << "Epoch [" << epoch << "] Batch [" << batch_count << "] Loss: " << total_loss.item<float>() << "\r" << std::flush;
                }
            }

            std::cout << "\nEpoch " << epoch << " Average Loss: " << (batch_count > 0 ? epoch_loss / batch_count : 0.0f) << std::endl;

            if ((epoch + 1) % train_config.save_interval == 0) {
                std::string save_path = train_config.save_path + "/yolov8_epoch_" + std::to_string(epoch + 1) + ".pt";
                torch::save(model, save_path);
                std::cout << "Saved model to " << save_path << std::endl;
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "[FAIL] Training loop failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}

int test_Encapsulated_API() {
    ModelConfig model_cfg = ModelConfig::get_config("nano");

    YOLOv8 model(model_cfg);

    TrainConfig train_cfg;
    train_cfg.data_path = resolve_yolo_data_root();
    train_cfg.epochs = 1;
    train_cfg.batch_size = 8;
    train_cfg.save_path = "D:/runs/train/exp";
    train_cfg.save_interval = 1;

    std::string pretrained = resolve_yolo_pretrained_path();

    if (!fs::exists(pretrained)) {
        std::cout << "SKIP missing pretrained weights: " << pretrained << std::endl;
        return 0;
    }
    if (!fs::exists(train_cfg.data_path)) {
        std::cout << "SKIP missing encapsulated API data root: " << train_cfg.data_path << std::endl;
        return 0;
    }

    model->train(train_cfg, pretrained);

    auto metrics = model->val(train_cfg.data_path);

    std::cout << "Final Validation Loss: " << metrics["val/loss"] << std::endl;

    return 0;
}

bool check_module_device(const torch::nn::Module& module, const torch::Device& target_device) {
    bool all_match = true;
    for (const auto& param : module.parameters()) {
        if (param.device() != target_device) {
            std::cerr << "[MISMATCH] Param device: " << param.device().str()
                << ", Target device: " << target_device.str() << std::endl;
            all_match = false;
        }
    }
    return all_match;
}

int test_Pipeline() {
    std::cout << "===== Test 3: V8 Full Device Pipeline =====" << std::endl;

    ModelConfig cfg = ModelConfig::get_config("nano");
    YOLOv8 model(cfg);
    torch::Device device = get_full_test_device();
    std::cout << "--- V8 initialized on CPU ---" << std::endl;
    std::cout << "[CHECK] Stem device consistent: "
              << (check_module_device(*model->get_stem(), torch::kCPU) ? "YES" : "NO")
              << std::endl;

    try {
        std::cout << "\n--- Pipeline move to " << device.str() << " ---" << std::endl;
        model->to(device);
        auto dummy_input = torch::rand({ 1, 3, 640, 640 }, device);

        torch::Tensor output = model->forward(dummy_input);
        std::cout << "[PASS] Model forward succeeded on " << device.str()
                  << ", output shape: " << output.sizes() << std::endl;
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "[FAIL] Pipeline test failed on " << device.str()
                  << ": " << e.what() << std::endl;
        return 1;
    }
}

int test_Infer(){
    std::string img_path = resolve_yolo_infer_image_path();
    std::string weight_path = resolve_yolo_pretrained_path();
    std::string output_path = resolve_yolo_infer_output_path();

    std::cout << "Starting inference..." << std::endl;
    std::cout << "Infer image path: " << img_path << std::endl;
    std::cout << "Infer weight path: " << weight_path << std::endl;
    std::cout << "Infer output path: " << output_path << std::endl;

    if (!fs::exists(img_path)) {
        std::cout << "SKIP missing infer image: " << img_path << std::endl;
        return 0;
    }
    if (!fs::exists(weight_path)) {
        std::cout << "SKIP missing infer weights: " << weight_path << std::endl;
        return 0;
    }

    ModelConfig model_cfg = ModelConfig::get_config("nano");
    YOLOv8 model(model_cfg);

    try {
        torch::load(model, weight_path);
        std::cout << "Loaded weights from " << weight_path << std::endl;
    }
    catch (const c10::Error& e) {
        std::cerr << "Error loading weights: " << e.what() << std::endl;
        return 1;
    }

    torch::Device device = get_full_test_device();
    model->to(device);
    model->eval();

    cv::Mat img1 = cv::imread(img_path);
    if (img1.empty()) {
        std::cerr << "Failed to read image: " << img_path << std::endl;
        return 1;
    }

    DataAugmenter augmenter;
    cv::Mat img_resized = augmenter.resize_image(img1, 640);

    cv::Mat img_rgb;
    cv::cvtColor(img_resized, img_rgb, cv::COLOR_BGR2RGB);

    torch::Tensor img_tensor = torch::from_blob(img_rgb.data, { img_rgb.rows, img_rgb.cols, 3 }, torch::kUInt8)
        .permute({ 2, 0, 1 })
        .to(torch::kFloat32)
        .div_(255.0f)
        .unsqueeze(0)
        .to(device);

    torch::NoGradGuard no_grad;
    auto detections = model->forward(img_tensor);

    auto dets = detections[0].cpu();

    std::cout << "Detected " << dets.size(0) << " objects." << std::endl;

    float scale_x = (float)img1.cols / 640;
    float scale_y = (float)img1.rows / 640;

    for (int i = 0; i < dets.size(0); ++i) {
        float x1 = dets[i][0].item<float>() * scale_x;
        float y1 = dets[i][1].item<float>() * scale_y;
        float x2 = dets[i][2].item<float>() * scale_x;
        float y2 = dets[i][3].item<float>() * scale_y;
        float score = dets[i][4].item<float>();
        int cls_id = dets[i][5].item<int>();

        cv::rectangle(img1, cv::Point(x1, y1), cv::Point(x2, y2), cv::Scalar(0, 255, 0), 2);
        std::string label = std::to_string(cls_id) + ": " + std::to_string(score).substr(0, 4);
        cv::putText(img1, label, cv::Point(x1, y1 - 5), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);

        std::cout << "Obj: " << cls_id << " Conf: " << score << " Box: " << x1 << "," << y1 << "," << x2 << "," << y2 << std::endl;
    }

    TORCH_CHECK(cv::imwrite(output_path, img1), "Failed to write infer output: ", output_path);
    std::cout << "Result saved to " << output_path << std::endl;
    return 0;
}

void test_Export(const std::string& weight_path, const std::string& export_path) {
    std::cout << "Exporting model (copying weights)..." << std::endl;

    ModelConfig model_cfg = ModelConfig::get_config("nano");
    YOLOv8 model(model_cfg);
    torch::load(model, weight_path);

    torch::save(model, export_path);
    std::cout << "Model saved to " << export_path << std::endl;
}

inline int run_core_tests() {
    std::cout << "*********************************" << std::endl;
    std::cout << "* Running All Tests for YOLOv8 *" << std::endl;
    std::cout << "*********************************" << std::endl;
    const std::string yolo_weight_path = resolve_yolo_weight_path();
    const std::string yolo_data_root = resolve_yolo_data_root();
    const std::string yolo_train_images = resolve_yolo_train_image_dir(yolo_data_root);
    const std::string yolo_train_labels = resolve_yolo_train_label_dir(yolo_data_root);
    const std::string yolo_pretrained = resolve_yolo_pretrained_path();
    const std::string resnet18_weights = resolve_resnet18_weight_path();
    const std::string resnet50_weights = resolve_resnet50_weight_path();
    const std::string inspect_small_weights = resolve_inspect_small_backbone_weight_path();
    const std::string inspect_large_weights = resolve_inspect_large_backbone_weight_path();
    const std::string transfer_data_root = resolve_transfer_learning_data_root();
    const std::string yolo_infer_image = resolve_yolo_infer_image_path();
    const std::string yolo_infer_output = resolve_yolo_infer_output_path();
    std::cout << "  OPENCV_IMAGE=" << TORCH_FULL_ENABLE_OPENCV_IMAGE
              << " OPENCV_ANNOTATION=" << TORCH_FULL_ENABLE_OPENCV_ANNOTATION
              << " DATASET_STAGE=" << TORCH_FULL_ENABLE_DATASET_STAGE
              << " EXTERNAL_WEIGHTS=" << TORCH_FULL_ENABLE_EXTERNAL_WEIGHTS
              << " TRAIN_STAGE=" << TORCH_FULL_ENABLE_TRAIN_STAGE
              << " INFER_STAGE=" << TORCH_FULL_ENABLE_INFER_STAGE
              << std::endl;
    std::cout << "  YOLO_WEIGHT_PATH=" << yolo_weight_path
              << " YOLO_PRETRAINED_PATH=" << yolo_pretrained
              << std::endl;
    std::cout << "  YOLO_DATA_ROOT=" << yolo_data_root
              << " YOLO_TRAIN_IMAGES=" << yolo_train_images
              << " YOLO_TRAIN_LABELS=" << yolo_train_labels
              << std::endl;
    std::cout << "  YOLO_INFER_IMAGE=" << yolo_infer_image
              << " YOLO_INFER_OUTPUT=" << yolo_infer_output
              << std::endl;
    std::cout << "  RESNET18_WEIGHT_PATH=" << resnet18_weights
              << " RESNET50_WEIGHT_PATH=" << resnet50_weights
              << std::endl;
    std::cout << "  INSPECT_SMALL_WEIGHT_PATH=" << inspect_small_weights
              << " INSPECT_LARGE_WEIGHT_PATH=" << inspect_large_weights
              << std::endl;
    std::cout << "  TRANSFER_DATA_ROOT=" << transfer_data_root
              << std::endl;

    int failures = 0;
    failures += test_ModelConfigTest();
    failures += test_UtilsTest();
    failures += test_NetworkShapeTest();
    failures += test_AssignerTest();
    failures += test_LossTest();
    failures += test_YoloModelBuildConfig();
#if TORCH_FULL_ENABLE_OPENCV_IMAGE
    failures += test_OpenCVImageStage();
    failures += test_OpenCVTensorContract();
#endif
#if TORCH_FULL_ENABLE_OPENCV_ANNOTATION
    failures += test_OpenCVAnnotationStage();
#endif
#if TORCH_FULL_ENABLE_DATASET_STAGE
    failures += test_YoloDatasetContract();
    failures += test_YoloEvalSummaryContract();
#endif
#if TORCH_FULL_ENABLE_TRAIN_STAGE
    failures += test_YoloSmokeRuntimeConfig();
    failures += test_TrainDebug();
    failures += test_YoloMainlineRunnerConfig();
    failures += test_YoloGpuSmoke();
    failures += test_YoloMainlineSession();
    failures += test_YoloTrainerSession();
    failures += test_YoloMainlineRoundReport();
    failures += test_Pipeline();
#endif
#if TORCH_FULL_ENABLE_EXTERNAL_WEIGHTS
    failures += test_Weight_Loading();
    failures += test_ResNet_Load();
    failures += test_Inspect_Load();
#endif
#if TORCH_FULL_ENABLE_TRAIN_STAGE
    failures += test_amp_training();
    failures += test_Transfer_Learning();
    failures += test_Encapsulated_API();
#endif
#if TORCH_FULL_ENABLE_INFER_STAGE
    failures += test_Infer();
#endif

    if (failures == 0) {
        std::cout << "\n[PASS] ALL TESTS PASSED" << std::endl;
    }
    else {
        std::cerr << "\n[FAIL] SOME TESTS FAILED (" << failures << ")" << std::endl;
    }

    return failures;
}

inline int run_preprocess_contract_tests() {
    std::cout << "****************************************" << std::endl;
    std::cout << "* Running Preprocess Contract Tests    *" << std::endl;
    std::cout << "****************************************" << std::endl;

    int failures = 0;
    failures += test_OpenCVImageStage();
    failures += test_OpenCVTensorContract();
    failures += test_OpenCVAnnotationStage();
    failures += test_YoloDatasetContract();

    if (failures == 0) {
        std::cout << "\n[PASS] PREPROCESS CONTRACT TESTS PASSED" << std::endl;
    } else {
        std::cerr << "\n[FAIL] PREPROCESS CONTRACT TESTS FAILED (" << failures << ")" << std::endl;
    }

    return failures;
}

inline int run_postprocess_contract_tests() {
    std::cout << "****************************************" << std::endl;
    std::cout << "* Running Postprocess Contract Tests   *" << std::endl;
    std::cout << "****************************************" << std::endl;

    int failures = 0;
    failures += test_LossTest();
    failures += test_YoloModelBuildConfig();

    if (failures == 0) {
        std::cout << "\n[PASS] POSTPROCESS CONTRACT TESTS PASSED" << std::endl;
    } else {
        std::cerr << "\n[FAIL] POSTPROCESS CONTRACT TESTS FAILED (" << failures << ")" << std::endl;
    }

    return failures;
}

#endif
