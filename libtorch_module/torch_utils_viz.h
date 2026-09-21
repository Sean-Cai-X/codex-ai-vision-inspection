#ifndef TORCH_UTILS_VIZ_H
#define TORCH_UTILS_VIZ_H


#include <torch/torch.h>
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include <map>
#include <filesystem>

namespace torch_utils {

    class FeatureExtractor {
    public:
        void register_hook(torch::nn::Module& model, const std::string& layer_name) {
            std::shared_ptr<torch::nn::Module> target_layer;

            for (const auto& child : model.named_children()) {
                if (child.key() == layer_name) {
                    target_layer = child.value();
                    break;
                }
            }

            if (!target_layer) {
                std::cerr << "[Warn] Layer not found: " << layer_name << std::endl;
                return;
            }

            std::string name_copy = layer_name;
            target_layer->register_forward_hook(
                [this, name_copy](const torch::nn::Module& module, const torch::jit::IValue& input, const torch::jit::IValue& output) {
                    if (output.isTensor()) {
                        this->features_[name_copy] = output.toTensor().detach().clone();
                    } else if (output.isTuple()) {
                        this->features_[name_copy] = output.toTuple()->elements()[0].toTensor().detach().clone();
                    }
                }
            );
            std::cout << "[Info] Hook registered for layer: " << layer_name << std::endl;
        }

        const std::map<std::string, torch::Tensor>& get_features() const {
            return features_;
        }

        void clear() { features_.clear(); }

    private:
        std::map<std::string, torch::Tensor> features_;
    };

    class Visualizer {
    public:
        static void save_feature_map(const torch::Tensor& feature, const std::string& save_path) {
            if (feature.dim() != 4) return;

            torch::Tensor heatmap = torch::mean(feature, 1, true).squeeze(0);

            heatmap = heatmap.to(torch::kFloat32);
            heatmap = heatmap - heatmap.min();
            heatmap = heatmap / heatmap.max();
            heatmap = heatmap * 255;

            heatmap = heatmap.permute({1, 2, 0}).to(torch::kCPU);

            cv::Mat img_map(heatmap.size(0), heatmap.size(1), CV_32FC1, heatmap.data_ptr<float>());
            img_map.convertTo(img_map, CV_8UC1);

            cv::Mat color_map;
            cv::applyColorMap(img_map, color_map, cv::COLORMAP_JET);

            std::filesystem::path path(save_path);
            if (path.has_parent_path()) {
                std::filesystem::create_directories(path.parent_path());
            }
            cv::imwrite(save_path, color_map);
            std::cout << "Saved feature map to: " << save_path << " (Shape: " << feature.sizes() << ")" << std::endl;
        }
    };
}

#endif