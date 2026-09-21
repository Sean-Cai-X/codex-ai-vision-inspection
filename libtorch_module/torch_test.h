#ifndef TORCH_TEST_H
#define TORCH_TEST_H


#include <torch/torch.h>
#include <torch/script.h>
#include <iostream>
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>

#include "torch_v8.h"
#include "torch_yolo_dataset.h"
#include "torch_data_augmenter.h"
#include "torch_detect.h"

inline void Test_Train(const TrainConfig& train_config) {
    std::cout << "Starting training..." << std::endl;
    ModelConfig model_cfg = ModelConfig::get_config("nano");
    YOLOv8 model(model_cfg);
    model->train(train_config);
}

inline void Test_Infer(const std::string& img_path, const std::string& weight_path) {
    std::cout << "Starting inference..." << std::endl;

    ModelConfig model_cfg = ModelConfig::get_config("nano");
    YOLOv8 model(model_cfg);

    try {
        model->load_checkpoint(weight_path);
        std::cout << "Loaded weights from " << weight_path << std::endl;
    }
    catch (const c10::Error& e) {
        std::cerr << "Error loading weights: " << e.what() << std::endl;
        return;
    }

    torch::Device device = torch::cuda::is_available() ? torch::kCUDA : torch::kCPU;
    model->to(device);
    model->eval();

    cv::Mat img = cv::imread(img_path);
    if (img.empty()) {
        std::cerr << "Failed to read image: " << img_path << std::endl;
        return;
    }

    DataAugmenter augmenter;
    cv::Mat img_resized = augmenter.resize_image(img, 640);

    cv::Mat img_rgb;
    cv::cvtColor(img_resized, img_rgb, cv::COLOR_BGR2RGB);

    torch::Tensor img_tensor = torch::from_blob(img_rgb.data, { img_rgb.rows, img_rgb.cols, 3 }, torch::kUInt8)
        .permute({ 2, 0, 1 })
        .contiguous()
        .clone()
        .to(torch::kFloat32)
        .div_(255.0f)
        .unsqueeze(0)
        .to(device);

    torch::NoGradGuard no_grad;
    auto pred = model->forward(img_tensor);
    auto dets = post_process(pred, 0.25f, 0.45f, model_cfg.num_classes);

    std::cout << "Detected " << dets.size() << " objects." << std::endl;

    float scale_x = (float)img.cols / 640;
    float scale_y = (float)img.rows / 640;

    for (const auto & det : dets) {
        float x1 = det.x1 * scale_x;
        float y1 = det.y1 * scale_y;
        float x2 = det.x2 * scale_x;
        float y2 = det.y2 * scale_y;
        float score = det.score;
        int cls_id = static_cast<int>(det.cls);

        cv::rectangle(img, cv::Point(x1, y1), cv::Point(x2, y2), cv::Scalar(0, 255, 0), 2);
        std::string label = std::to_string(cls_id) + ": " + std::to_string(score).substr(0, 4);
        cv::putText(img, label, cv::Point(x1, y1 - 5), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);

        std::cout << "Obj: " << cls_id << " Conf: " << score << " Box: " << x1 << "," << y1 << "," << x2 << "," << y2 << std::endl;
    }

    cv::imwrite("result.jpg", img);
    std::cout << "Result saved to result.jpg" << std::endl;
}

inline void Test_Export(const std::string& weight_path, const std::string& export_path) {
    std::cout << "Exporting model (copying weights)..." << std::endl;

    ModelConfig model_cfg = ModelConfig::get_config("nano");
    YOLOv8 model(model_cfg);
    model->load_checkpoint(weight_path);

    torch::serialize::OutputArchive archive;
    model->save(archive);
    archive.save_to(export_path);
    std::cout << "Model saved to " << export_path << std::endl;
}

#endif