#ifndef TORCH_EVAL_H
#define TORCH_EVAL_H

#include <torch/csrc/api/include/torch/torch.h>
#include <torch/csrc/api/include/torch/nn/module.h>
#include <torch/csrc/api/include/torch/optim/optimizer.h>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <cmath>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include <unordered_map>
#include "torch_detect.h"

namespace torch_eval {
    std::tuple<torch::Tensor, torch::Tensor> create_grids_single(
        int64_t h, int64_t w, int64_t stride, const torch::Device& device) {
        torch::Tensor grid_x = torch::arange(w, torch::dtype(torch::kFloat32).device(device));
        torch::Tensor grid_y = torch::arange(h, torch::dtype(torch::kFloat32).device(device));

        auto grids = torch::meshgrid({ grid_y, grid_x });
        TORCH_CHECK(grids.size() == 2, "meshgrid must return exactly 2 tensors");
        torch::Tensor y = grids[0];
        torch::Tensor x = grids[1];

        return { x, y };
    }
    inline torch::Tensor calculate_iou(
        const torch::Tensor& boxes1,
        const torch::Tensor& boxes2
    ) {
        TORCH_CHECK(boxes1.dim() == 2 && boxes1.size(1) == 4, "boxes1 must be [N,4]");
        TORCH_CHECK(boxes2.dim() == 2 && boxes2.size(1) == 4, "boxes2 must be [M,4]");

        auto [x1_inter, y1_inter] = std::make_tuple(
            torch::max(boxes1.select(1, 0).unsqueeze(1), boxes2.select(1, 0).unsqueeze(0)),
            torch::max(boxes1.select(1, 1).unsqueeze(1), boxes2.select(1, 1).unsqueeze(0))
        );
        auto [x2_inter, y2_inter] = std::make_tuple(
            torch::min(boxes1.select(1, 2).unsqueeze(1), boxes2.select(1, 2).unsqueeze(0)),
            torch::min(boxes1.select(1, 3).unsqueeze(1), boxes2.select(1, 3).unsqueeze(0))
        );

        auto w_inter = torch::clamp(x2_inter - x1_inter, 0.0f);
        auto h_inter = torch::clamp(y2_inter - y1_inter, 0.0f);
        auto area_inter = w_inter * h_inter;

        auto area1 = (boxes1.select(1, 2) - boxes1.select(1, 0)) * (boxes1.select(1, 3) - boxes1.select(1, 1));
        auto area2 = (boxes2.select(1, 2) - boxes2.select(1, 0)) * (boxes2.select(1, 3) - boxes2.select(1, 1));
        auto area_union = area1.unsqueeze(1) + area2.unsqueeze(0) - area_inter;

        return area_inter / (area_union + 1e-6f);
    }

    struct PRMetrics {
        std::vector<float> precision;
        std::vector<float> recall;
        float ap;
    };

    inline PRMetrics calculate_pr(
        const torch::Tensor& pred_boxes,
        const torch::Tensor& pred_scores,
        const torch::Tensor& gt_boxes,
        float iou_threshold = 0.5f
    ) {
        PRMetrics metrics;
        if (pred_boxes.numel() == 0 || gt_boxes.numel() == 0) {
            metrics.precision = { 0.0f };
            metrics.recall = { 0.0f };
            metrics.ap = 0.0f;
            return metrics;
        }

        auto [sorted_scores, indices] = torch::sort(pred_scores, 0, true);
        torch::Tensor sorted_boxes = pred_boxes.index_select(0, indices);

        torch::Tensor iou_matrix = calculate_iou(sorted_boxes, gt_boxes);
        int64_t num_pred = sorted_boxes.size(0);
        int64_t num_gt = gt_boxes.size(0);

        std::vector<bool> gt_matched(num_gt, false);
        std::vector<int> tp(num_pred, 0);
        std::vector<int> fp(num_pred, 0);

        for (int64_t i = 0; i < num_pred; i++) {
            float max_iou = 0.0f;
            int64_t max_gt_idx = -1;

            for (int64_t j = 0; j < num_gt; j++) {
                if (iou_matrix[i][j].item<float>() > max_iou) {
                    max_iou = iou_matrix[i][j].item<float>();
                    max_gt_idx = j;
                }
            }

            if (max_iou >= iou_threshold && !gt_matched[max_gt_idx]) {
                tp[i] = 1;
                gt_matched[max_gt_idx] = true;
            }
            else {
                fp[i] = 1;
            }
        }

        std::vector<int> tp_cumsum(num_pred, 0);
        std::vector<int> fp_cumsum(num_pred, 0);
        tp_cumsum[0] = tp[0];
        fp_cumsum[0] = fp[0];
        for (int64_t i = 1; i < num_pred; i++) {
            tp_cumsum[i] = tp_cumsum[i - 1] + tp[i];
            fp_cumsum[i] = fp_cumsum[i - 1] + fp[i];
        }

        metrics.precision.resize(num_pred);
        metrics.recall.resize(num_pred);
        int64_t total_gt = num_gt;
        for (int64_t i = 0; i < num_pred; i++) {
            metrics.precision[i] = static_cast<float>(tp_cumsum[i]) / (tp_cumsum[i] + fp_cumsum[i] + 1e-6f);
            metrics.recall[i] = static_cast<float>(tp_cumsum[i]) / (total_gt + 1e-6f);
        }

        std::vector<float> recall_levels = { 0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f };
        float ap = 0.0f;
        for (float r : recall_levels) {
            float max_p = 0.0f;
            for (int64_t i = 0; i < num_pred; i++) {
                if (metrics.recall[i] >= r) {
                    max_p = std::max(max_p, metrics.precision[i]);
                }
            }
            ap += max_p / 11.0f;
        }
        metrics.ap = ap;

        return metrics;
    }

    struct InferenceMetrics {
        float avg_latency_ms;
        float fps;
        float std_dev;
    };

    template <typename ModuleType>
    inline InferenceMetrics benchmark_inference(
        ModuleType& model,
        const torch::Tensor& input_tensor,
        int warmup_runs = 10,
        int test_runs = 100
    ) {
        InferenceMetrics metrics;
        model->eval();

        torch::Device device = input_tensor.device();

        model->to(device);
        torch::Tensor input = input_tensor.to(device);

        for (int i = 0; i < warmup_runs; i++) {
            torch::NoGradGuard no_grad;

            auto output = model->forward(input);


            if (device.type() == torch::kCUDA) {
                torch::cuda::synchronize();
            }
        }

        std::vector<float> latencies;
        for (int i = 0; i < test_runs; i++) {
            torch::NoGradGuard no_grad;
            auto start = std::chrono::high_resolution_clock::now();

            auto output = model->forward(input);

            if (device.type() == torch::kCUDA) {
                torch::cuda::synchronize();
            }
            auto end = std::chrono::high_resolution_clock::now();

            float latency_ms = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0f;
            latencies.push_back(latency_ms);
        }

        float sum = 0.0f, sum_sq = 0.0f;
        for (float l : latencies) {
            sum += l;
            sum_sq += l * l;
        }
        metrics.avg_latency_ms = sum / test_runs;
        metrics.fps = 1000.0f / metrics.avg_latency_ms;
        metrics.std_dev = std::sqrt((sum_sq / test_runs) - (metrics.avg_latency_ms * metrics.avg_latency_ms));

        return metrics;
    }

    inline void print_eval_metrics(
        const PRMetrics& pr_metrics,
        const InferenceMetrics& infer_metrics,
        const std::string& dataset_name = "test"
    ) {
        std::cout << "==================== " << dataset_name << " Evaluation Metrics ====================" << std::endl;
        std::cout << "Average Precision (AP@0.5): " << std::fixed << std::setprecision(4) << pr_metrics.ap << std::endl;
        std::cout << "Max Precision: " << *std::max_element(pr_metrics.precision.begin(), pr_metrics.precision.end()) << std::endl;
        std::cout << "Max Recall: " << *std::max_element(pr_metrics.recall.begin(), pr_metrics.recall.end()) << std::endl;
        std::cout << "----------------------------------------" << std::endl;
        std::cout << "Average Latency: " << std::fixed << std::setprecision(2) << infer_metrics.avg_latency_ms << " ms" << std::endl;
        std::cout << "FPS: " << std::fixed << std::setprecision(2) << infer_metrics.fps << std::endl;
        std::cout << "Latency Std Dev: " << std::fixed << std::setprecision(2) << infer_metrics.std_dev << " ms" << std::endl;
        std::cout << "====================================================================================" << std::endl;
    }

    inline void save_eval_metrics(
        const PRMetrics& pr_metrics,
        const InferenceMetrics& infer_metrics,
        const std::string& csv_path,
        const std::string& model_name = "yolov8"
    ) {
        std::ofstream file(csv_path, std::ios::app);
        TORCH_CHECK(file.is_open(), "Failed to open CSV file: ", csv_path);

        file.seekp(0, std::ios::end);
        if (file.tellp() == 0) {
            file << "model_name,ap,max_precision,max_recall,avg_latency_ms,fps,latency_std_dev\n";
        }

        float max_p = *std::max_element(pr_metrics.precision.begin(), pr_metrics.precision.end());
        float max_r = *std::max_element(pr_metrics.recall.begin(), pr_metrics.recall.end());
        file << model_name << ","
            << std::fixed << std::setprecision(4) << pr_metrics.ap << ","
            << std::fixed << std::setprecision(4) << max_p << ","
            << std::fixed << std::setprecision(4) << max_r << ","
            << std::fixed << std::setprecision(2) << infer_metrics.avg_latency_ms << ","
            << std::fixed << std::setprecision(2) << infer_metrics.fps << ","
            << std::fixed << std::setprecision(2) << infer_metrics.std_dev << "\n";

        file.close();
    }

    inline torch::Tensor normalize_boxes_by_grid(
        const torch::Tensor& boxes,
        int64_t h, int64_t w, int64_t stride
    ) {
        auto [grid_x, grid_y] = create_grids_single(h, w, stride, boxes.device());
        torch::Tensor grid_wh = torch::tensor({ w, h }, boxes.options()).repeat({ boxes.size(0), 2 });

        return boxes / grid_wh;
    }

}

#endif