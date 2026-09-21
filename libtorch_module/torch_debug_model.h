#ifndef TORCH_DEBUG_MODEL_H
#define TORCH_DEBUG_MODEL_H


#include "torch_DeepLabV3.h"
#include "torch_utils_viz.h"

#include <opencv2/opencv.hpp>

inline torch::Tensor preprocess_image(const std::string& path) {
    cv::Mat img = cv::imread(path);
    if (img.empty()) {
        throw std::runtime_error("Cannot read image: " + path);
    }
    cv::cvtColor(img, img, cv::COLOR_BGR2RGB);
    cv::resize(img, img, cv::Size(224, 224));

    torch::Tensor tensor_img = torch::from_blob(img.data, {1, 224, 224, 3}, torch::kByte);
    tensor_img = tensor_img.permute({0, 3, 1, 2});
    tensor_img = tensor_img.contiguous().clone().to(torch::kFloat32).div(255.0);

    tensor_img[0][0] = (tensor_img[0][0] - 0.485) / 0.229;
    tensor_img[0][1] = (tensor_img[0][1] - 0.456) / 0.224;
    tensor_img[0][2] = (tensor_img[0][2] - 0.406) / 0.225;

    return tensor_img;
}

inline int debug_model() {
    std::cout << "Starting Slice Analysis for DeepLabV3..." << std::endl;

    try {
        int num_classes = 21;
        DeepLabV3 model(num_classes);
        model->eval();

        std::string img_path = "test_image.jpg";
        auto img_tensor = preprocess_image(img_path);

        torch_utils::FeatureExtractor extractor;
        extractor.register_hook(*model, "aspp");

        auto backbone_ptr = model->get_backbone();
        if (backbone_ptr) {
            extractor.register_hook(*backbone_ptr, "layer1");
            extractor.register_hook(*backbone_ptr, "layer2");
            extractor.register_hook(*backbone_ptr, "layer3");
            extractor.register_hook(*backbone_ptr, "layer4");
        }

        std::cout << "Running forward pass..." << std::endl;
        auto output = model->forward(img_tensor);
        std::cout << "Forward pass completed. Output shape: " << output.sizes() << std::endl;

        std::string save_dir = "runs/analysis/";
        const auto& features = extractor.get_features();

        for (const auto& item : features) {
            std::string layer_name = item.first;
            torch::Tensor feat = item.second;

            std::cout << "Processing layer: " << layer_name << " Shape: " << feat.sizes() << std::endl;
            torch_utils::Visualizer::save_feature_map(
                feat,
                save_dir + layer_name + "_heatmap.png"
            );
        }

        auto pred = output.argmax(1).to(torch::kFloat32);
        pred = pred * (255.0 / num_classes);
        cv::Mat pred_mat(pred.size(1), pred.size(2), CV_32FC1, pred.data_ptr<float>());
        pred_mat.convertTo(pred_mat, CV_8UC1);
        cv::applyColorMap(pred_mat, pred_mat, cv::COLORMAP_JET);
        cv::imwrite(save_dir + "final_prediction.png", pred_mat);

    } catch (const c10::Error& e) {
        std::cerr << "Error: " << e.msg() << "\n";
        return -1;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
        return -1;
    }

    std::cout << "Slice analysis completed. Check 'runs/analysis/' folder.\n";
    return 0;
}

#endif