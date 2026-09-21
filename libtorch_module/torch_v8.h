#ifndef TORCH_V8_H
#define TORCH_V8_H

#include <torch/torch.h>
#include <vector>
#include <tuple>
#include <string>
#include <map>
#include <filesystem>

#include <fstream>
#include <stdexcept>
#include <chrono>

#include "torch_modelconfig.h"
#include "torch_backbone.h"
#include "torch_pan.h"
#include "torch_v8loss.h"
#include "torch_yolo_head.h"
#include "torch_geometry_roi_head.h"

#include <torch/csrc/serialization.h>

#include "torch_yolo_dataset.h"
#include "torch_parser.h"
#include "torch_mermaid.h"


inline torch::optim::SGDOptions make_yolo_sgd_options(const TrainConfig& train_config) {
    auto optimizer_config = train_config.optimizer_config();
    optimizer_config.validate();
    return torch::optim::SGDOptions(optimizer_config.lr)
        .momentum(optimizer_config.momentum)
        .weight_decay(optimizer_config.weight_decay);
}

inline torch::Device resolve_yolo_runtime_device(const YoloTrainRuntimeConfig& runtime_config) {
    runtime_config.validate();
    if (runtime_config.prefer_cuda && torch::cuda::is_available()) {
        return torch::Device(torch::kCUDA);
    }
    return torch::Device(torch::kCPU);
}

struct YoloTrainProgress {
    float epoch_loss = 0.0f;
    int batch_count = 0;

    void record_batch(float loss_value) {
        epoch_loss += loss_value;
        batch_count += 1;
    }

    float average_loss() const {
        return batch_count > 0 ? epoch_loss / static_cast<float>(batch_count) : 0.0f;
    }
};

inline void print_yolo_train_batch_log(
    int epoch,
    int total_epochs,
    int batch_count,
    float loss_value,
    const YoloTrainRuntimeConfig& runtime_config) {

    if (batch_count % runtime_config.log_interval != 0) {
        return;
    }

    printf("\rEpoch [%d/%d] Batch [%d] Loss: %.4f",
        epoch + 1, total_epochs, batch_count, loss_value);
    fflush(stdout);
}

inline bool should_stop_yolo_train_epoch(
    const YoloTrainProgress& progress,
    const YoloTrainRuntimeConfig& runtime_config) {
    return runtime_config.max_train_batches > 0 &&
        progress.batch_count >= runtime_config.max_train_batches;
}

inline void maybe_save_yolo_checkpoint(
    const torch::nn::Module& module,
    int epoch,
    const YoloTrainRuntimeConfig& runtime_config) {

    if ((epoch + 1) % runtime_config.checkpoint.save_interval != 0) {
        return;
    }

    std::filesystem::create_directories(runtime_config.checkpoint.save_path);
    std::string save_file = runtime_config.checkpoint.save_path + "/epoch_" + std::to_string(epoch + 1) + ".pt";

    torch::serialize::OutputArchive archive;
    module.save(archive);
    archive.save_to(save_file);
    std::cout << "[YOLOv8] Checkpoint saved to " << save_file << std::endl;
}

struct YoloModelBuildConfig {
    YoloDetectHeadConfig head;
    YoloLossConfig loss;
    GeometryRoiHeadConfig geometry_head;
    GeometryRoiLossConfig geometry_loss;
};

struct YoloEvalConfig {
    YoloValidationConfig data;
    YoloPostProcessConfig postprocess;
    float match_iou_threshold = 0.5f;

    void validate() const {
        data.validate();
        postprocess.validate();
        TORCH_CHECK(match_iou_threshold >= 0.0f && match_iou_threshold <= 1.0f,
            "eval match_iou_threshold must be in [0,1]");
        TORCH_CHECK(postprocess.num_classes > 0, "eval postprocess num_classes must be positive");
    }
};

inline std::vector<BBox> collect_valid_target_boxes(const torch::Tensor& targets_for_image) {
    TORCH_CHECK(targets_for_image.dim() == 2 && targets_for_image.size(1) == 6,
        "targets_for_image must be [N, 6], got ", targets_for_image.sizes());

    std::vector<BBox> out;
    auto cpu = targets_for_image.to(torch::kCPU);
    for (int64_t i = 0; i < cpu.size(0); ++i) {
        if (cpu[i][1].item<float>() < 0.0f) {
            continue;
        }
        BBox box;
        box.cls = static_cast<int64_t>(cpu[i][1].item<float>());
        box.x1 = cpu[i][2].item<float>();
        box.y1 = cpu[i][3].item<float>();
        box.x2 = cpu[i][4].item<float>();
        box.y2 = cpu[i][5].item<float>();
        box.score = 1.0f;
        out.push_back(box);
    }
    return out;
}

inline YoloModelBuildConfig normalize_yolo_build_config(YoloModelBuildConfig build_config) {
    if (build_config.head.use_dfl || build_config.loss.enable_dfl) {
        TORCH_CHECK(build_config.head.use_dfl == build_config.loss.enable_dfl,
            "YOLO build config requires head.use_dfl and loss.enable_dfl to match");
        build_config.loss.reg_max = build_config.head.reg_max;
    }
    return build_config;
}

inline YoloEvalConfig make_yolo_eval_config(const ModelConfig& model_config, const YoloValidationConfig& validation_config) {
    YoloEvalConfig config;
    config.data = validation_config;
    config.postprocess.num_classes = model_config.num_classes;
    return config;
}

class YOLOv8Impl : public torch::nn::Module {
public:
    YOLOv8Impl(ModelConfig config)
        : YOLOv8Impl(config, YoloModelBuildConfig{}) {
    }

    YOLOv8Impl(ModelConfig config, YoloModelBuildConfig build_config)
        : config_(config), build_config_(normalize_yolo_build_config(std::move(build_config))) {
        std::vector<int64_t> base_channels = { 64, 128, 256, 512, 1024 };
        backbone_ = register_module("backbone",
            YOLOv8Backbone(base_channels, config.depth_multiple, config.width_multiple));

        auto backbone_out_ch = backbone_->get_out_channels();
        pan_ = register_module("neck",
            PAN(backbone_out_ch, config.depth_multiple, config.width_multiple));

        head_ = register_module("head",
            YOLOv8Detect(config.num_classes, backbone_out_ch, config.strides, build_config_.head));

        loss_fn_ = register_module("loss_fn", YOLOv8Loss(config.num_classes, build_config_.loss));
        if (build_config_.geometry_head.enabled) {
            geometry_head_ = register_module("geometry_roi_head",
                GeometryRoiHead(backbone_out_ch.at(0), build_config_.geometry_head));
            geometry_loss_fn_ = register_module("geometry_roi_loss",
                GeometryRoiLoss(build_config_.geometry_head.polygon_vertex_count,
                    build_config_.geometry_loss));
        }
    }

    std::vector<char> get_the_bytes(const std::string& path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);

        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file: " + path);
        }

        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<char> buffer(size);
        if (!file.read(buffer.data(), size)) {
            throw std::runtime_error("Failed to read file: " + path);
        }

        return buffer;
    }

    void load_weights(const std::string& path) {
        torch::NoGradGuard no_grad;

        std::vector<char> f = get_the_bytes(path);
        torch::IValue data = torch::pickle_load(f);

        auto dict = data.toGenericDict();

        auto params = this->named_parameters();
        auto buffers = this->named_buffers();

        for (auto& item : dict) {
            std::string key = item.key().toStringRef();
            torch::Tensor val = item.value().toTensor();

            auto p_it = params.find(key);
            if (p_it != nullptr) {
                p_it->copy_(val);
                std::cout << "Loaded param: " << key << std::endl;
                continue;
            }

            auto b_it = buffers.find(key);
            if (b_it != nullptr) {
                b_it->copy_(val);
                std::cout << "Loaded buffer: " << key << std::endl;
                continue;
            }

            std::cerr << "Warning: Key not found in C++ model: " << key << std::endl;
        }
    }

    void load_checkpoint(const std::string & path) {
        torch::serialize::InputArchive archive;
        archive.load_from(path);
        this->load(archive);
    }

    void export_structure(const std::string& path = "yolov8.mmd") {
        torch_utils::MermaidGenerator::generate(*this, path, "YOLOv8");

    }

    void train(const TrainConfig& train_config, const std::string& pretrained_weights = "") {
        train(train_config, train_config.runtime_config(pretrained_weights));
    }

    void train(const TrainConfig& train_config, const YoloTrainRuntimeConfig& runtime_config) {
        std::cout << "\n[YOLOv8] Starting training..." << std::endl;

        torch::Device device = resolve_yolo_runtime_device(runtime_config);
        this->to(device);
        std::cout << "[YOLOv8] Device: " << (device.is_cuda() ? "GPU" : "CPU") << std::endl;

        if (!runtime_config.pretrained_weights.empty()) {
            std::cout << "[YOLOv8] Loading pretrained weights from: " << runtime_config.pretrained_weights << std::endl;
            try {
                WeightParser parser(runtime_config.pretrained_weights);
                parser.load_to(*this);
            }
            catch (const std::exception& e) {
                std::cerr << "[Warn] Failed to load weights: " << e.what() << ". Training from scratch." << std::endl;
            }
        }

        std::cout << "[YOLOv8] Loading dataset from: " << train_config.data_path << std::endl;
        auto train_paths = make_yolo_split_paths(train_config.data_path, "train");
        auto dataset_config = make_yolo_dataset_config(train_config, true);
        auto dataset = YoloDataset(train_paths, dataset_config).map(torch::data::transforms::Stack<>());

        auto data_loader = torch::data::make_data_loader(
            std::move(dataset),
            make_yolo_loader_options(train_config.batch_size, train_config.dataloader_workers)
        );

        torch::optim::SGD optimizer(
            this->parameters(),
            make_yolo_sgd_options(train_config)
        );

        for (int epoch = 0; epoch < train_config.epochs; ++epoch) {
            torch::nn::Module::train(true);

            YoloTrainProgress progress;

            for (auto& batch : *data_loader) {
                auto imgs = batch.data.to(device);
                auto targets = batch.target.to(device);

                for (int b = 0; b < targets.size(0); ++b) {
                    targets[b].select(1, 0).fill_(static_cast<float>(b));
                }

                optimizer.zero_grad();

                auto result = this->train_step_internal(imgs, targets);
                torch::Tensor total_loss = std::get<0>(result);

                total_loss.backward();
                optimizer.step();

                float loss_value = total_loss.item<float>();
                progress.record_batch(loss_value);
                print_yolo_train_batch_log(epoch, train_config.epochs, progress.batch_count, loss_value, runtime_config);
                if (should_stop_yolo_train_epoch(progress, runtime_config)) {
                    std::cout << "\n[YOLOv8] Smoke stop after "
                              << progress.batch_count
                              << " batch(es) in epoch " << epoch + 1 << std::endl;
                    break;
                }
            }

            float avg_loss = progress.average_loss();
            std::cout << "\nEpoch " << epoch + 1 << " Finished. Avg Loss: " << avg_loss << std::endl;

            maybe_save_yolo_checkpoint(*this, epoch, runtime_config);
        }
        std::cout << "[YOLOv8] Training completed." << std::endl;
    }
    std::map<std::string, float> val(
        const std::string& data_path,
        int batch_size = 16,
        int img_size = 640,
        YoloResizePolicy resize_policy = YoloResizePolicy::PlainResize,
        int max_gt = 50) {
        YoloValidationConfig config;
        config.batch_size = batch_size;
        config.img_size = img_size;
        config.max_gt = max_gt;
        config.resize_policy = resize_policy;
        return val(data_path, config);
    }

    std::map<std::string, float> val(const std::string& data_path, const YoloValidationConfig& val_config) {
        auto eval_summary = val_summary(data_path, make_yolo_eval_config(config_, val_config));
        return {
            {"val/loss", eval_summary.loss},
            {"val/box_loss", eval_summary.box_loss},
            {"val/cls_loss", eval_summary.cls_loss},
            {"val/dfl_loss", eval_summary.dfl_loss},
            {"val/mAP50", eval_summary.map50_proxy},
            {"val/avg_conf", eval_summary.avg_confidence},
            {"val/precision", eval_summary.precision},
            {"val/recall", eval_summary.recall},
            {"val/f1", eval_summary.f1},
            {"val/matched_iou", eval_summary.matched_iou},
            {"val/tp", static_cast<float>(eval_summary.true_positives)},
            {"val/fp", static_cast<float>(eval_summary.false_positives)},
            {"val/fn", static_cast<float>(eval_summary.false_negatives)},
            {"val/pred_boxes", static_cast<float>(eval_summary.predicted_boxes)},
            {"val/target_boxes", static_cast<float>(eval_summary.target_boxes)}
        };
    }

    struct ValidationSummary {
        using ClassStats = YoloEvalSummary::ClassStats;

        float loss = 0.0f;
        float box_loss = 0.0f;
        float cls_loss = 0.0f;
        float dfl_loss = 0.0f;
        float map50_proxy = 0.0f;
        float avg_confidence = 0.0f;
        float precision = 0.0f;
        float recall = 0.0f;
        float f1 = 0.0f;
        float matched_iou = 0.0f;
        int64_t true_positives = 0;
        int64_t false_positives = 0;
        int64_t false_negatives = 0;
        int64_t predicted_boxes = 0;
        int64_t target_boxes = 0;
        std::map<int64_t, ClassStats> per_class;
    };

    struct GeometryRoiChannelMetrics {
        int64_t instance_count = 0;
        int64_t ellipse_count = 0;
        int64_t polygon_count = 0;
        double ellipse_parameter_mae_px = 0.0;
        double polygon_vertex_mae_px = 0.0;
        double continuity_brier = 0.0;
        double continuity_accuracy = 0.0;
        double ellipse_predictive_variance = 0.0;
        double polygon_predictive_variance = 0.0;
        double ellipse_parameter_sum = 0.0;
        double polygon_vertex_sum = 0.0;
        double continuity_brier_sum = 0.0;
        double continuity_correct_sum = 0.0;
        double ellipse_variance_sum = 0.0;
        double polygon_variance_sum = 0.0;

        void finalize() {
            if (ellipse_count > 0) {
                ellipse_parameter_mae_px = ellipse_parameter_sum / ellipse_count;
                ellipse_predictive_variance = ellipse_variance_sum / ellipse_count;
            }
            if (polygon_count > 0) {
                polygon_vertex_mae_px = polygon_vertex_sum / polygon_count;
                polygon_predictive_variance = polygon_variance_sum / polygon_count;
            }
            if (instance_count > 0) {
                continuity_brier = continuity_brier_sum / instance_count;
                continuity_accuracy = continuity_correct_sum / instance_count;
            }
        }
    };

    struct GeometryRoiValidationSummary {
        int64_t instance_count = 0;
        int64_t ellipse_count = 0;
        int64_t polygon_count = 0;
        double ellipse_parameter_mae_px = 0.0;
        double polygon_vertex_mae_px = 0.0;
        double continuity_brier = 0.0;
        double continuity_accuracy = 0.0;
        double ellipse_predictive_variance = 0.0;
        double polygon_predictive_variance = 0.0;
        double inference_ms = 0.0;
        std::map<int64_t, GeometryRoiChannelMetrics> per_affine_channel;
        std::string roi_source = "TEACHER_SIDECAR_ROI";
        std::string claim_status = "TEACHER_ROI_GEOMETRY_HEAD_EVALUATED_NOT_END_TO_END";
    };

    GeometryRoiValidationSummary geometry_roi_val_summary(
        const std::string& data_path, const std::string& split,
        YoloDatasetConfig dataset_config) {
        TORCH_CHECK(geometry_head_enabled(), "geometry ROI evaluation requires an enabled geometry head");
        TORCH_CHECK(!split.empty(), "geometry ROI evaluation split must not be empty");
        dataset_config.is_train = false;
        dataset_config.enable_hsv = false;
        dataset_config.enable_flip = false;
        dataset_config.geometry_targets_enabled = true;
        dataset_config.geometry_polygon_vertex_count = build_config_.geometry_head.polygon_vertex_count;
        dataset_config.validate();
        torch::NoGradGuard no_grad;
        torch::nn::Module::eval();
        const torch::Device device = this->parameters().begin()->device();
        auto dataset = YoloDataset(make_yolo_split_paths(data_path, split), dataset_config)
            .map(torch::data::transforms::Stack<>());
        auto data_loader = torch::data::make_data_loader(
            std::move(dataset), make_yolo_loader_options(dataset_config.max_gt > 0 ?
                std::min<int>(dataset_config.max_gt, 16) : 1, 0));
        GeometryRoiValidationSummary summary;
        GeometryRoiChannelMetrics total_metrics;
        const auto start = std::chrono::steady_clock::now();

        auto accumulate_metrics = [&](GeometryRoiChannelMetrics& metrics, const torch::Tensor& mask,
                                      const GeometryRoiPrediction& prediction, const GeometryRoiTargets& targets) {
            if (!mask.any().item<bool>()) return;
            const torch::Tensor kind = targets.kind;
            const torch::Tensor ellipse_mask = mask & (kind == 1);
            const torch::Tensor polygon_mask = mask & (kind == 2);
            if (ellipse_mask.any().item<bool>()) {
                const int64_t count = ellipse_mask.sum().item<int64_t>();
                const torch::Tensor error = (prediction.ellipse_parameters.index({ellipse_mask}) -
                    targets.ellipse_parameters.index({ellipse_mask})).abs().mean();
                metrics.ellipse_parameter_sum += error.item<double>() * dataset_config.img_size * count;
                metrics.ellipse_variance_sum += torch::exp(prediction.log_variance.index({ellipse_mask})
                    .select(1, 0)).mean().item<double>() * count;
                metrics.ellipse_count += count;
            }
            if (polygon_mask.any().item<bool>()) {
                const int64_t count = polygon_mask.sum().item<int64_t>();
                const torch::Tensor delta = prediction.polygon_vertices.index({polygon_mask}) -
                    targets.polygon_vertices.index({polygon_mask});
                const torch::Tensor point_error = torch::sqrt(torch::pow(delta.reshape({-1, 2}), 2).sum(1)).mean();
                metrics.polygon_vertex_sum += point_error.item<double>() * dataset_config.img_size * count;
                metrics.polygon_variance_sum += torch::exp(prediction.log_variance.index({polygon_mask})
                    .select(1, 1)).mean().item<double>() * count;
                metrics.polygon_count += count;
            }
            const torch::Tensor probability = torch::sigmoid(prediction.boundary_continuity_logits.index({mask}));
            const torch::Tensor continuity = targets.boundary_continuity.index({mask}).reshape({-1, 1});
            const int64_t count = continuity.size(0);
            metrics.continuity_brier_sum += torch::pow(probability - continuity, 2).sum().item<double>();
            metrics.continuity_correct_sum += ((probability >= 0.5) == (continuity >= 0.5)).sum().item<double>();
            metrics.instance_count += count;
        };

        for (auto& batch : *data_loader) {
            const torch::Tensor images = batch.data.to(device);
            const torch::Tensor batch_targets = batch.target.to(device);
            const GeometryRoiTargets targets = geometry_roi_targets_from_batch(
                batch_targets, build_config_.geometry_head.polygon_vertex_count);
            const auto backbone_outs = backbone_->forward(images);
            const auto neck_outs = pan_->forward(backbone_outs);
            const GeometryRoiPrediction prediction = geometry_head_->forward(neck_outs.at(0), targets.rois);
            accumulate_metrics(total_metrics, torch::ones_like(targets.kind, torch::kBool), prediction, targets);
            const torch::Tensor affine_channel = targets.affine_channel.to(torch::kLong);
            for (const int64_t channel_id : {0, 1, 2, 3}) {
                const torch::Tensor channel_mask = affine_channel == channel_id;
                if (channel_mask.any().item<bool>()) {
                    accumulate_metrics(summary.per_affine_channel[channel_id], channel_mask, prediction, targets);
                }
            }
        }
        const auto end = std::chrono::steady_clock::now();
        summary.inference_ms = static_cast<double>(std::chrono::duration_cast<
            std::chrono::milliseconds>(end - start).count());
        total_metrics.finalize();
        summary.instance_count = total_metrics.instance_count;
        summary.ellipse_count = total_metrics.ellipse_count;
        summary.polygon_count = total_metrics.polygon_count;
        summary.ellipse_parameter_mae_px = total_metrics.ellipse_parameter_mae_px;
        summary.polygon_vertex_mae_px = total_metrics.polygon_vertex_mae_px;
        summary.continuity_brier = total_metrics.continuity_brier;
        summary.continuity_accuracy = total_metrics.continuity_accuracy;
        summary.ellipse_predictive_variance = total_metrics.ellipse_predictive_variance;
        summary.polygon_predictive_variance = total_metrics.polygon_predictive_variance;
        for (auto& channel : summary.per_affine_channel) channel.second.finalize();
        return summary;
    }


    ValidationSummary val_summary(const std::string& data_path, const YoloEvalConfig& eval_config) {
        eval_config.validate();

        std::cout << "\n[YOLOv8] Starting validation..." << std::endl;
        torch::NoGradGuard no_grad;
        torch::nn::Module::eval();

        torch::Device device = this->parameters().begin()->device();

        auto val_paths = make_yolo_split_paths(data_path, "val");
        auto dataset_config = make_yolo_eval_config(eval_config.data);

        auto dataset = YoloDataset(val_paths, dataset_config).map(torch::data::transforms::Stack<>());

        auto data_loader = torch::data::make_data_loader(
            std::move(dataset),
            make_yolo_loader_options(eval_config.data.batch_size, eval_config.data.dataloader_workers)
        );

        ValidationSummary summary;
        int batch_count = 0;
        std::vector<std::vector<BBox>> all_detections;
        std::vector<std::vector<BBox>> all_targets;

        for (auto& batch : *data_loader) {
            auto imgs = batch.data.to(device);
            auto targets = batch.target.to(device);

            for (int b = 0; b < targets.size(0); ++b) {
                targets[b].select(1, 0).fill_(static_cast<float>(b));
            }

            auto result = this->train_step_internal(imgs, targets);

            summary.loss += std::get<0>(result).item<float>();
            auto loss_items = std::get<1>(result);
            summary.box_loss += loss_items["box_loss"];
            summary.cls_loss += loss_items["cls_loss"];
            summary.dfl_loss += loss_items["dfl_loss"];

            auto pred = this->forward(imgs);
            auto batch_detections = post_process_batch(pred, eval_config.postprocess);
            for (auto& dets : batch_detections) {
                all_detections.push_back(std::move(dets));
            }

            for (int64_t b = 0; b < targets.size(0); ++b) {
                auto image_targets = collect_valid_target_boxes(targets[b]);
                summary.target_boxes += static_cast<int64_t>(image_targets.size());
                all_targets.push_back(std::move(image_targets));
            }

            batch_count++;
            printf("\r[Val] Processing batch %d...", batch_count);
        }
        std::cout << std::endl;

        if (batch_count > 0) {
            summary.loss /= batch_count;
            summary.box_loss /= batch_count;
            summary.cls_loss /= batch_count;
            summary.dfl_loss /= batch_count;
        }

        auto detection_summary = summarize_yolo_detections(all_detections, summary.target_boxes);
        auto match_summary = summarize_yolo_matches(
            all_detections, all_targets, eval_config.match_iou_threshold);
        summary.avg_confidence = detection_summary.avg_confidence;
        summary.predicted_boxes = detection_summary.predicted_boxes;
        summary.target_boxes = detection_summary.target_boxes;
        summary.map50_proxy = match_summary.recall;
        summary.precision = match_summary.precision;
        summary.recall = match_summary.recall;
        summary.f1 = match_summary.f1;
        summary.matched_iou = match_summary.matched_iou;
        summary.true_positives = match_summary.true_positives;
        summary.false_positives = match_summary.false_positives;
        summary.false_negatives = match_summary.false_negatives;
        summary.per_class = match_summary.per_class;

        std::cout << "[YOLOv8] Validation Results: Loss=" << summary.loss
                  << " AvgConf=" << summary.avg_confidence
                  << " TP=" << summary.true_positives
                  << " FP=" << summary.false_positives
                  << " FN=" << summary.false_negatives
                  << " Precision=" << summary.precision
                  << " Recall=" << summary.recall
                  << " F1=" << summary.f1
                  << " MatchIoU=" << summary.matched_iou << std::endl;
        for (const auto& [cls, stats] : summary.per_class) {
            std::cout << "  [Class " << cls << "]"
                      << " Pred=" << stats.predicted_boxes
                      << " Target=" << stats.target_boxes
                      << " TP=" << stats.true_positives
                      << " FP=" << stats.false_positives
                      << " FN=" << stats.false_negatives
                      << " Precision=" << stats.precision
                      << " Recall=" << stats.recall
                      << " F1=" << stats.f1
                      << std::endl;
        }
        return summary;
    }

    torch::Tensor forward(torch::Tensor x) {
        auto backbone_outs = backbone_->forward(x);
        auto neck_outs = pan_->forward(backbone_outs);
        return head_->forward(neck_outs);
    }

    std::tuple<torch::Tensor, std::unordered_map<std::string, float>>
        train_step(torch::Tensor imgs, torch::Tensor targets) {
        auto backbone_outs = backbone_->forward(imgs);
        auto neck_outs = pan_->forward(backbone_outs);
        auto preds = head_->forward_train(neck_outs);
        return loss_fn_->forward(preds, targets);
    }

    // This path is deliberately explicit.  A plan must provide a real geometry
    // target manifest and pass its decoded targets here; bbox-only YOLO labels
    // are not silently converted into ellipse or polygon targets.
    std::tuple<torch::Tensor, std::unordered_map<std::string, float>>
        train_step_with_geometry(torch::Tensor imgs, torch::Tensor targets,
                                 const GeometryRoiTargets& geometry_targets) {
        TORCH_CHECK(geometry_head_ && geometry_loss_fn_,
            "geometry ROI training was requested but the geometry head is disabled");
        auto backbone_outs = backbone_->forward(imgs);
        auto neck_outs = pan_->forward(backbone_outs);
        auto preds = head_->forward_train(neck_outs);
        auto detector = loss_fn_->forward(
            preds, targets.slice(2, 0, GeometryRoiTargetLayout::DetectorColumns));
        const GeometryRoiPrediction geometry_prediction =
            geometry_head_->forward(neck_outs.at(0), geometry_targets.rois);
        auto geometry = geometry_loss_fn_->forward(geometry_prediction, geometry_targets);
        torch::Tensor total = std::get<0>(detector) + std::get<0>(geometry);
        auto metrics = std::get<1>(detector);
        for (const auto& item : std::get<1>(geometry)) metrics[item.first] = item.second;
        metrics["total_loss"] = total.item<float>();
        return std::make_tuple(total, std::move(metrics));
    }

    bool geometry_head_enabled() const {
        return static_cast<bool>(geometry_head_) &&
            static_cast<bool>(geometry_loss_fn_);
    }

    const ModelConfig& get_config() const { return config_; }
    const YoloModelBuildConfig& get_build_config() const { return build_config_; }

    const Stem& get_stem() const { return backbone_->get_stem(); }
private:
    std::tuple<torch::Tensor, std::unordered_map<std::string, float>>
        train_step_internal(torch::Tensor imgs, torch::Tensor targets) {
        auto backbone_outs = backbone_->forward(imgs);
        auto neck_outs = pan_->forward(backbone_outs);
        auto preds = head_->forward_train(neck_outs);
        return loss_fn_->forward(preds, targets);
    }

    ModelConfig config_;
    YoloModelBuildConfig build_config_;
    YOLOv8Backbone backbone_{ nullptr };
    PAN pan_{ nullptr };
    YOLOv8Detect head_{ nullptr };
    YOLOv8Loss loss_fn_{ nullptr };
    GeometryRoiHead geometry_head_{ nullptr };
    GeometryRoiLoss geometry_loss_fn_{ nullptr };
};
TORCH_MODULE(YOLOv8);

#endif


