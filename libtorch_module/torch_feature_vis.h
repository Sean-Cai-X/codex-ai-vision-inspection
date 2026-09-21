#ifndef TORCH_FEATURE_VIS_H
#define TORCH_FEATURE_VIS_H


#include <torch/torch.h>
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include <filesystem>
#include <iostream>

namespace torch_utils {

    class FeatureMapVisualizer {
    public:
        FeatureMapVisualizer(const std::string& output_dir = "feature_maps")
            : output_dir_(output_dir) {
            std::filesystem::create_directories(output_dir_);
        }

        void visualize(torch::nn::Module& model, torch::Tensor x, const std::string& prefix = "layer") {
            model.eval();
            torch::NoGradGuard no_grad;

            torch::Device device = x.device();

            int layer_idx = 0;
            torch::Tensor current_x = x;

            for (auto& child : model.named_children()) {
                std::string name = child.key();
                auto module = child.value();


                try {
                }
                catch (...) {
                    continue;
                }
            }
        }

        static void save_feature_map(const torch::Tensor& features, const std::string& save_path) {
            if (features.dim() != 4) return;

            auto f = features[0].detach().cpu();

            auto avg_map = torch::mean(f, 0);


            avg_map = avg_map - avg_map.min();
            avg_map = avg_map / avg_map.max() * 255;
            avg_map = avg_map.to(torch::kUInt8);

            cv::Mat img(avg_map.size(0), avg_map.size(1), CV_8UC1, avg_map.data_ptr());

            cv::Mat color_img;
            cv::applyColorMap(img, color_img, cv::COLORMAP_JET);

            cv::imwrite(save_path, color_img);
        }

    private:
        std::string output_dir_;
    };

}

#endif