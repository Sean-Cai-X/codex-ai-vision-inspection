#ifndef TORCH_DATA_AUGMENTER_H
#define TORCH_DATA_AUGMENTER_H


#include <opencv2/opencv.hpp>
#include <torch/torch.h>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <tuple>
#include <algorithm>


enum class YoloResizePolicy {
    PlainResize,
    Letterbox
};

struct YoloOptimizerConfig {
    float lr = 0.01f;
    float momentum = 0.937f;
    float weight_decay = 0.0005f;

    void validate() const {
        TORCH_CHECK(lr > 0.0f, "optimizer lr must be positive");
        TORCH_CHECK(momentum >= 0.0f && momentum < 1.0f, "optimizer momentum must be in [0, 1)");
        TORCH_CHECK(weight_decay >= 0.0f, "optimizer weight_decay must be non-negative");
    }
};

struct YoloValidationConfig {
    int batch_size = 16;
    int img_size = 640;
    int max_gt = 50;
    YoloResizePolicy resize_policy = YoloResizePolicy::PlainResize;
    int letterbox_pad_value = 114;
    int dataloader_workers = 8;

    void validate() const {
        TORCH_CHECK(batch_size > 0, "validation batch_size must be positive");
        TORCH_CHECK(img_size > 0, "validation img_size must be positive");
        TORCH_CHECK(max_gt > 0, "validation max_gt must be positive");
        TORCH_CHECK(letterbox_pad_value >= 0 && letterbox_pad_value <= 255,
            "validation letterbox_pad_value must be in [0, 255]");
        TORCH_CHECK(dataloader_workers >= 0, "validation dataloader_workers must be non-negative");
    }
};

struct YoloCheckpointConfig {
    std::string save_path = "./weights";
    int save_interval = 10;

    void validate() const {
        TORCH_CHECK(save_interval > 0, "checkpoint save_interval must be positive");
    }
};

struct YoloTrainRuntimeConfig {
    std::string pretrained_weights;
    bool prefer_cuda = true;
    int log_interval = 10;
    int max_train_batches = 0;
    YoloCheckpointConfig checkpoint;

    void validate() const {
        TORCH_CHECK(log_interval > 0, "train log_interval must be positive");
        TORCH_CHECK(max_train_batches >= 0, "train max_train_batches must be non-negative");
        checkpoint.validate();
    }
};

struct TrainConfig {
    int epochs = 300;
    int batch_size = 16;
    float lr = 0.01f;
    float momentum = 0.937f;
    float weight_decay = 0.0005f;
    int img_size = 640;
    int max_gt = 50;
    bool enable_hsv = true;
    bool enable_flip = true;
    YoloResizePolicy resize_policy = YoloResizePolicy::PlainResize;
    int letterbox_pad_value = 114;
    int dataloader_workers = 8;
    std::string data_path = "D:/YOLOv8train";
    std::string save_path = "./weights";
    int save_interval = 10;

    YoloOptimizerConfig optimizer_config() const {
        YoloOptimizerConfig config;
        config.lr = lr;
        config.momentum = momentum;
        config.weight_decay = weight_decay;
        return config;
    }

    YoloValidationConfig validation_config() const {
        YoloValidationConfig config;
        config.batch_size = batch_size;
        config.img_size = img_size;
        config.max_gt = max_gt;
        config.resize_policy = resize_policy;
        config.letterbox_pad_value = letterbox_pad_value;
        config.dataloader_workers = dataloader_workers;
        return config;
    }

    YoloCheckpointConfig checkpoint_config() const {
        YoloCheckpointConfig config;
        config.save_path = save_path;
        config.save_interval = save_interval;
        return config;
    }

    YoloTrainRuntimeConfig runtime_config(const std::string& pretrained = "") const {
        YoloTrainRuntimeConfig config;
        config.pretrained_weights = pretrained;
        config.checkpoint = checkpoint_config();
        return config;
    }

    YoloTrainRuntimeConfig smoke_runtime_config(
        const std::string& pretrained = "",
        int smoke_batches = 2) const {
        YoloTrainRuntimeConfig config = runtime_config(pretrained);
        config.log_interval = 1;
        config.max_train_batches = smoke_batches;
        config.checkpoint.save_interval = std::max(1, epochs);
        return config;
    }
};

struct Annotation {
    float x1;
    float y1;
    float x2;
    float y2;
    int class_id;
};

class LabelLoader {
public:
    std::vector<Annotation> load_labels(const std::string& label_path) {
        std::vector<Annotation> annotations;
        std::ifstream file(label_path);

        if (!file.is_open()) {
            return annotations;
        }

        std::string line;
        while (std::getline(file, line)) {
            std::istringstream iss(line);
            int class_id;
            float x_center, y_center, width, height;

            if (iss >> class_id >> x_center >> y_center >> width >> height) {
                Annotation ann;
                ann.class_id = class_id;
                ann.x1 = x_center - width / 2.0f;
                ann.y1 = y_center - height / 2.0f;
                ann.x2 = x_center + width / 2.0f;
                ann.y2 = y_center + height / 2.0f;

                ann.x1 = std::max(0.0f, std::min(1.0f, ann.x1));
                ann.y1 = std::max(0.0f, std::min(1.0f, ann.y1));
                ann.x2 = std::max(0.0f, std::min(1.0f, ann.x2));
                ann.y2 = std::max(0.0f, std::min(1.0f, ann.y2));

                annotations.push_back(ann);
            }
        }

        file.close();
        return annotations;
    }
};

class DataAugmenter {
public:
    DataAugmenter(float hsv_h = 0.015f, float hsv_s = 0.7f, float hsv_v = 0.4f,
        float flip_prob = 0.5f)
        : hsv_h_(hsv_h), hsv_s_(hsv_s), hsv_v_(hsv_v), flip_prob_(flip_prob) {
    }

    cv::Mat hsv_augment(const cv::Mat& img) {
        cv::Mat hsv;
        cv::cvtColor(img, hsv, cv::COLOR_BGR2HSV);

        float dh = (torch::rand(1).item<float>() * 2 - 1) * hsv_h_;
        float ds = (torch::rand(1).item<float>() * 2 - 1) * hsv_s_ + 1;
        float dv = (torch::rand(1).item<float>() * 2 - 1) * hsv_v_ + 1;

        hsv.forEach<cv::Vec3b>([&](cv::Vec3b& pixel, const int* position) -> void {
            float h = pixel[0] + dh * 180.0f;
            if (h < 0) {
                h += 180.0f;
            } else if (h >= 180.0f) {
                h -= 180.0f;
            }
            pixel[0] = cv::saturate_cast<uchar>(h);

            pixel[1] = cv::saturate_cast<uchar>(pixel[1] * ds);
            pixel[2] = cv::saturate_cast<uchar>(pixel[2] * dv);
        });

        cv::Mat aug_img;
        cv::cvtColor(hsv, aug_img, cv::COLOR_HSV2BGR);
        return aug_img;
    }

    std::tuple<cv::Mat, std::vector<std::vector<float>>> random_flip(
        const cv::Mat& img,
        const std::vector<std::vector<float>>& labels) {

        if (torch::rand(1).item<float>() < flip_prob_) {
            cv::Mat flipped_img;
            cv::flip(img, flipped_img, 1);

            std::vector<std::vector<float>> flipped_labels = labels;
            for (auto& label : flipped_labels) {
                float old_x1 = label[1];
                float old_x2 = label[3];

                label[1] = 1.0f - old_x2;
                label[3] = 1.0f - old_x1;
            }
            return { flipped_img, flipped_labels };
        }

        return { img.clone(), labels };
    }

    cv::Mat resize_image(const cv::Mat& img, int target_size) {
        cv::Mat resized;
        cv::resize(img, resized, cv::Size(target_size, target_size));
        return resized;
    }

private:
    float hsv_h_, hsv_s_, hsv_v_;
    float flip_prob_;
};

#endif
