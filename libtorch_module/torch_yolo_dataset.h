#ifndef TORCH_YOLO_DATASET_H
#define TORCH_YOLO_DATASET_H


#include <torch/torch.h>
#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include <unordered_map>
#include <cmath>
#include <array>
#include <opencv2/core.hpp>
#include "torch_data_augmenter.h"
#include "torch_geometry_roi_head.h"

namespace fs = std::filesystem;


struct YoloDatasetConfig {
    int img_size = 640;
    bool is_train = true;
    int max_gt = 50;
    bool enable_hsv = true;
    bool enable_flip = true;
    YoloResizePolicy resize_policy = YoloResizePolicy::PlainResize;
    int letterbox_pad_value = 114;
    bool geometry_targets_enabled = false;
    int geometry_polygon_vertex_count = 8;
    std::string geometry_target_dir;

    void validate() const {
        TORCH_CHECK(img_size > 0, "img_size must be positive");
        TORCH_CHECK(max_gt > 0, "max_gt must be positive");
        TORCH_CHECK(letterbox_pad_value >= 0 && letterbox_pad_value <= 255,
            "letterbox_pad_value must be in [0, 255]");
        TORCH_CHECK(!geometry_targets_enabled || geometry_polygon_vertex_count >= 3,
            "geometry_polygon_vertex_count must be at least three when geometry targets are enabled");
        TORCH_CHECK(!geometry_targets_enabled || !geometry_target_dir.empty(),
            "geometry_target_dir is required when geometry targets are enabled");
        TORCH_CHECK(!geometry_targets_enabled || !enable_flip,
            "geometry targets currently require enable_flip=false until sidecar geometry transforms are implemented");
    }
};

struct YoloDatasetPaths {
    std::string image_dir;
    std::string label_dir;

    void validate() const {
        TORCH_CHECK(!image_dir.empty(), "image_dir must not be empty");
        TORCH_CHECK(!label_dir.empty(), "label_dir must not be empty");
    }
};

inline YoloDatasetPaths make_yolo_split_paths(const std::string& data_root, const std::string& split) {
    TORCH_CHECK(!data_root.empty(), "data_root must not be empty");
    TORCH_CHECK(!split.empty(), "split must not be empty");

    YoloDatasetPaths paths;
    paths.image_dir = data_root + "/images/" + split;
    paths.label_dir = data_root + "/labels/" + split;
    return paths;
}

inline torch::data::DataLoaderOptions make_yolo_loader_options(int batch_size, int workers) {
    TORCH_CHECK(batch_size > 0, "batch_size must be positive");
    TORCH_CHECK(workers >= 0, "workers must be non-negative");
    return torch::data::DataLoaderOptions().batch_size(batch_size).workers(workers);
}

inline YoloDatasetConfig make_yolo_dataset_config(const TrainConfig& train_config, bool is_train_override) {
    YoloDatasetConfig config;
    config.img_size = train_config.img_size;
    config.is_train = is_train_override;
    config.max_gt = train_config.max_gt;
    config.enable_hsv = is_train_override ? train_config.enable_hsv : false;
    config.enable_flip = is_train_override ? train_config.enable_flip : false;
    config.resize_policy = train_config.resize_policy;
    config.letterbox_pad_value = train_config.letterbox_pad_value;
    return config;
}

inline YoloDatasetConfig make_yolo_eval_config(
    int img_size,
    YoloResizePolicy resize_policy = YoloResizePolicy::PlainResize,
    int max_gt = 50,
    int letterbox_pad_value = 114) {

    YoloDatasetConfig config;
    config.img_size = img_size;
    config.is_train = false;
    config.max_gt = max_gt;
    config.enable_hsv = false;
    config.enable_flip = false;
    config.resize_policy = resize_policy;
    config.letterbox_pad_value = letterbox_pad_value;
    return config;
}

inline YoloDatasetConfig make_yolo_eval_config(const YoloValidationConfig& val_config) {
    val_config.validate();
    return make_yolo_eval_config(
        val_config.img_size,
        val_config.resize_policy,
        val_config.max_gt,
        val_config.letterbox_pad_value);
}

class YoloDataset : public torch::data::Dataset<YoloDataset> {
public:
    YoloDataset(const YoloDatasetPaths& paths, YoloDatasetConfig config)
        : YoloDataset(paths.image_dir, paths.label_dir, std::move(config)) {
        paths.validate();
    }

    YoloDataset(const std::string& img_dir, const std::string& label_dir,
        int img_size = 640, bool is_train = true)
        : YoloDataset(img_dir, label_dir, YoloDatasetConfig{img_size, is_train}) {
    }

    YoloDataset(const std::string& img_dir, const std::string& label_dir,
        YoloDatasetConfig config)
        : img_dir_(img_dir), label_dir_(label_dir), config_(config) {

        config_.validate();

        augmenter_ = DataAugmenter();
        label_loader_ = LabelLoader();

        if (fs::exists(img_dir_)) {
            for (const auto& entry : fs::directory_iterator(img_dir_)) {
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".jpg" || ext == ".png" || ext == ".jpeg" || ext == ".bmp") {
                    img_paths_.push_back(entry.path().string());
                }
            }
        }
        else {
            std::cerr << "Error: Image directory not found: " << img_dir_ << std::endl;
        }
    }

    torch::data::Example<> get(size_t index) override {
        std::string img_path = img_paths_[index];
        std::string label_filename = fs::path(img_path).stem().string() + ".txt";
        std::string label_path = (fs::path(label_dir_) / label_filename).string();

        cv::Mat img = cv::imread(img_path);
        if (img.empty()) {
            std::cerr << "Error: Image empty :  " << img_dir_ << std::endl;
            auto empty_img = torch::zeros({3, config_.img_size, config_.img_size}, torch::kFloat32);
            auto empty_target = torch::full({config_.max_gt, 6}, -1.0f, torch::kFloat32);
            return { empty_img, empty_target };
        }

        std::vector<Annotation> anns = label_loader_.load_labels(label_path);

        const std::vector<GeometrySidecarTarget> geometry_targets =
            config_.geometry_targets_enabled
                ? load_geometry_targets(fs::path(img_path).stem().string(), anns.size())
                : std::vector<GeometrySidecarTarget>{};

        std::vector<std::vector<float>> labels;
        for (const auto& ann : anns) {
            labels.push_back({ (float)ann.class_id, ann.x1, ann.y1, ann.x2, ann.y2 });
        }

        if (config_.is_train) {
            if (config_.enable_hsv) {
                img = augmenter_.hsv_augment(img);
            }
            if (config_.enable_flip) {
                auto [flipped_img, flipped_labels] = augmenter_.random_flip(img, labels);
                img = flipped_img;
                labels = flipped_labels;
            }
        }

        if (config_.resize_policy == YoloResizePolicy::Letterbox) {
            std::tie(img, labels) = letterbox_image_and_labels(img, labels);
        } else {
            img = augmenter_.resize_image(img, config_.img_size);
        }

        cv::Mat img_rgb;
        cv::cvtColor(img, img_rgb, cv::COLOR_BGR2RGB);
        torch::Tensor img_tensor = torch::from_blob(img_rgb.data, { img_rgb.rows, img_rgb.cols, 3 }, torch::kUInt8)
            .permute({ 2, 0, 1 })
            .contiguous()
            .clone()
            .to(torch::kFloat32)
            .div_(255.0f);

        const int64_t target_columns = config_.geometry_targets_enabled
            ? GeometryRoiTargetLayout::total_columns(config_.geometry_polygon_vertex_count)
            : GeometryRoiTargetLayout::DetectorColumns;
        torch::Tensor target_tensor = torch::full(
            { config_.max_gt, target_columns }, -1.0f, torch::kFloat32);

        int count = 0;
        for (const auto& l : labels) {
            if (count >= config_.max_gt) break;
            target_tensor[count][0] = 0.0f;
            target_tensor[count][1] = l[0];
            target_tensor[count][2] = l[1];
            target_tensor[count][3] = l[2];
            target_tensor[count][4] = l[3];
            target_tensor[count][5] = l[4];
            if (config_.geometry_targets_enabled) {
                const GeometrySidecarTarget& geometry = geometry_targets.at(count);
                target_tensor[count][GeometryRoiTargetLayout::Kind] =
                    static_cast<float>(geometry.kind);
                target_tensor[count][GeometryRoiTargetLayout::RoiX1] = geometry.roi[0];
                target_tensor[count][GeometryRoiTargetLayout::RoiY1] = geometry.roi[1];
                target_tensor[count][GeometryRoiTargetLayout::RoiX2] = geometry.roi[2];
                target_tensor[count][GeometryRoiTargetLayout::RoiY2] = geometry.roi[3];
                for (int64_t item = 0; item < GeometryRoiTargetLayout::EllipseColumns; ++item)
                    target_tensor[count][GeometryRoiTargetLayout::Ellipse + item] =
                        geometry.ellipse_parameters.at(static_cast<size_t>(item));
                for (int64_t item = 0; item < config_.geometry_polygon_vertex_count * 2; ++item)
                    target_tensor[count][GeometryRoiTargetLayout::polygon_offset() + item] =
                        geometry.polygon_vertices.at(static_cast<size_t>(item));
                target_tensor[count][GeometryRoiTargetLayout::continuity_offset(
                    config_.geometry_polygon_vertex_count)] = geometry.boundary_continuity;
                target_tensor[count][GeometryRoiTargetLayout::affine_channel_offset(
                    config_.geometry_polygon_vertex_count)] = static_cast<float>(geometry.affine_channel);
            }
            count++;
        }

        return { img_tensor, target_tensor };
    }

    torch::optional<size_t> size() const override {
        return img_paths_.size();
    }

private:
    struct GeometrySidecarTarget {
        int kind = 0;
        std::array<float, 4> roi{};
        std::array<float, 6> ellipse_parameters{};
        std::vector<float> polygon_vertices;
        float boundary_continuity = 0.0f;
        int affine_channel = 0; // 0=unclassified, 1=rotation, 2=scale, 3=compound
    };

    static float read_finite_number(const cv::FileNode& node, const char* field) {
        double value = 0.0;
        TORCH_CHECK(!node.empty(), "geometry sidecar is missing ", field);
        node >> value;
        TORCH_CHECK(std::isfinite(value), "geometry sidecar field is not finite: ", field);
        return static_cast<float>(value);
    }

    std::vector<GeometrySidecarTarget> load_geometry_targets(
        const std::string& image_stem, size_t expected_count) const {
        const fs::path sidecar_path = fs::path(config_.geometry_target_dir) / (image_stem + ".json");
        TORCH_CHECK(fs::is_regular_file(sidecar_path),
            "geometry sidecar is missing for image: ", image_stem);
        cv::FileStorage sidecar(sidecar_path.string(), cv::FileStorage::READ | cv::FileStorage::FORMAT_JSON);
        TORCH_CHECK(sidecar.isOpened(), "geometry sidecar cannot be parsed: ", sidecar_path.string());
        std::string schema;
        sidecar["schema"] >> schema;
        TORCH_CHECK(schema == "cxvision.geometry_roi_target_sidecar.v1",
            "geometry sidecar schema is unsupported: ", sidecar_path.string());
        std::string affine_channel_name;
        if (!sidecar["affine_channel"].empty())
            sidecar["affine_channel"] >> affine_channel_name;
        int affine_channel = 0;
        if (affine_channel_name == "rotation") affine_channel = 1;
        else if (affine_channel_name == "scale") affine_channel = 2;
        else if (affine_channel_name == "compound") affine_channel = 3;
        else TORCH_CHECK(affine_channel_name.empty(),
            "geometry sidecar affine_channel must be rotation, scale, or compound: ", sidecar_path.string());
        const cv::FileNode instances = sidecar["instances"];
        TORCH_CHECK(instances.isSeq(), "geometry sidecar instances must be an array: ", sidecar_path.string());
        TORCH_CHECK(instances.size() == expected_count,
            "geometry sidecar instance count must match YOLO label count: ", sidecar_path.string());
        std::vector<GeometrySidecarTarget> targets(expected_count);
        std::vector<bool> seen(expected_count, false);
        for (const auto& instance : instances) {
            int instance_index = -1;
            instance["instance_index"] >> instance_index;
            TORCH_CHECK(instance_index >= 0 && static_cast<size_t>(instance_index) < expected_count &&
                    !seen[static_cast<size_t>(instance_index)],
                "geometry sidecar instance_index must be unique and within label range: ", sidecar_path.string());
            seen[static_cast<size_t>(instance_index)] = true;
            std::string kind;
            instance["geometry_kind"] >> kind;
            GeometrySidecarTarget target;
            target.affine_channel = affine_channel;
            if (kind == "ellipse") target.kind = 1;
            else if (kind == "polygon") target.kind = 2;
            else TORCH_CHECK(false, "geometry sidecar geometry_kind must be ellipse or polygon: ", sidecar_path.string());
            const cv::FileNode roi = instance["roi"];
            TORCH_CHECK(roi.isSeq() && roi.size() == 4,
                "geometry sidecar roi must be [x1,y1,x2,y2]");
            for (int component = 0; component < 4; ++component) {
                target.roi[static_cast<size_t>(component)] = read_finite_number(roi[component], "roi");
                TORCH_CHECK(target.roi[static_cast<size_t>(component)] >= 0.0f &&
                        target.roi[static_cast<size_t>(component)] <= 1.0f,
                    "geometry sidecar ROI must be normalized to [0,1]");
            }
            TORCH_CHECK(target.roi[2] > target.roi[0] && target.roi[3] > target.roi[1],
                "geometry sidecar ROI must have positive area");
            const cv::FileNode ellipse = instance["ellipse_parameters"];
            TORCH_CHECK(ellipse.isSeq() && ellipse.size() == 6,
                "geometry sidecar ellipse_parameters must be [cx,cy,rx,ry,sin2a,cos2a]");
            for (int component = 0; component < 6; ++component)
                target.ellipse_parameters[static_cast<size_t>(component)] =
                    read_finite_number(ellipse[component], "ellipse_parameters");
            const cv::FileNode polygon = instance["polygon_vertices"];
            TORCH_CHECK(polygon.isSeq() && polygon.size() == config_.geometry_polygon_vertex_count * 2,
                "geometry sidecar polygon_vertices count must equal 2*geometry_polygon_vertex_count");
            target.polygon_vertices.reserve(static_cast<size_t>(polygon.size()));
            for (const auto& coordinate : polygon)
                target.polygon_vertices.push_back(read_finite_number(coordinate, "polygon_vertices"));
            target.boundary_continuity = read_finite_number(instance["boundary_continuity"], "boundary_continuity");
            TORCH_CHECK(target.boundary_continuity >= 0.0f && target.boundary_continuity <= 1.0f,
                "geometry sidecar boundary_continuity must be in [0,1]");
            targets[static_cast<size_t>(instance_index)] = std::move(target);
        }
        for (bool present : seen)
            TORCH_CHECK(present, "geometry sidecar is missing an instance_index");
        return targets;
    }

    std::tuple<cv::Mat, std::vector<std::vector<float>>> letterbox_image_and_labels(
        const cv::Mat& img,
        const std::vector<std::vector<float>>& labels) const {

        int original_w = img.cols;
        int original_h = img.rows;
        int target = config_.img_size;

        float scale = std::min(
            static_cast<float>(target) / std::max(1, original_w),
            static_cast<float>(target) / std::max(1, original_h));

        int resized_w = std::max(1, static_cast<int>(std::round(original_w * scale)));
        int resized_h = std::max(1, static_cast<int>(std::round(original_h * scale)));

        cv::Mat resized;
        cv::resize(img, resized, cv::Size(resized_w, resized_h));

        cv::Mat canvas(
            target,
            target,
            img.type(),
            cv::Scalar(config_.letterbox_pad_value, config_.letterbox_pad_value, config_.letterbox_pad_value));

        int pad_x = (target - resized_w) / 2;
        int pad_y = (target - resized_h) / 2;
        resized.copyTo(canvas(cv::Rect(pad_x, pad_y, resized_w, resized_h)));

        std::vector<std::vector<float>> adjusted_labels = labels;
        for (auto& label : adjusted_labels) {
            float x1 = label[1] * original_w;
            float y1 = label[2] * original_h;
            float x2 = label[3] * original_w;
            float y2 = label[4] * original_h;

            x1 = (x1 * scale + pad_x) / target;
            y1 = (y1 * scale + pad_y) / target;
            x2 = (x2 * scale + pad_x) / target;
            y2 = (y2 * scale + pad_y) / target;

            label[1] = std::clamp(x1, 0.0f, 1.0f);
            label[2] = std::clamp(y1, 0.0f, 1.0f);
            label[3] = std::clamp(x2, 0.0f, 1.0f);
            label[4] = std::clamp(y2, 0.0f, 1.0f);
        }

        return { canvas, adjusted_labels };
    }

    std::string img_dir_;
    std::string label_dir_;
    YoloDatasetConfig config_;
    DataAugmenter augmenter_;
    LabelLoader label_loader_;
    std::vector<std::string> img_paths_;
};

#endif
