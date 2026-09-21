#ifndef TORCH_TASK_ROUTING_H
#define TORCH_TASK_ROUTING_H

#include <algorithm>
#include <string>
#include <utility>

#include <torch/torch.h>

enum class TorchTaskKind {
    Unknown = 0,
    NumericClassification,
    NumericClustering,
    GeometryMatching,
    GeometryClustering,
    VisualClassification,
    VisualDetection,
    VisualSegmentation,
    VisualEmbedding
};

enum class TorchBackendRoute {
    Torch = 0,
    Mlpack,
    ManualReview
};

struct TorchTaskBoundary {
    TorchTaskKind kind = TorchTaskKind::Unknown;
    bool has_image_tensor = false;
    bool has_dense_feature_tensor = false;
    bool requires_gradient_training = false;
    bool requires_end_to_end_learning = false;
    bool geometry_or_descriptor_only = false;
    int64_t batch_size = 0;
    int64_t feature_dim = 0;
    int64_t num_classes = 0;
};

struct TorchRoutingDecision {
    TorchBackendRoute route = TorchBackendRoute::ManualReview;
    const char* reason = "insufficient task boundary information";
};

struct TorchTrainIO {
    torch::Tensor features;
    torch::Tensor labels;
};

struct TorchTrainStepOutput {
    torch::Tensor logits;
    torch::Tensor loss;
    int64_t batch_size = 0;
    int64_t num_classes = 0;
};

struct TorchInferIO {
    torch::Tensor features;
    int64_t topk = 1;
};

struct TorchInferOutput {
    torch::Tensor logits;
    torch::Tensor scores;
    torch::Tensor labels;
};

inline const char* torch_task_kind_name(TorchTaskKind kind) {
    switch (kind) {
        case TorchTaskKind::NumericClassification:
            return "numeric_classification";
        case TorchTaskKind::NumericClustering:
            return "numeric_clustering";
        case TorchTaskKind::GeometryMatching:
            return "geometry_matching";
        case TorchTaskKind::GeometryClustering:
            return "geometry_clustering";
        case TorchTaskKind::VisualClassification:
            return "visual_classification";
        case TorchTaskKind::VisualDetection:
            return "visual_detection";
        case TorchTaskKind::VisualSegmentation:
            return "visual_segmentation";
        case TorchTaskKind::VisualEmbedding:
            return "visual_embedding";
        case TorchTaskKind::Unknown:
        default:
            return "unknown";
    }
}

inline const char* torch_backend_route_name(TorchBackendRoute route) {
    switch (route) {
        case TorchBackendRoute::Torch:
            return "torch";
        case TorchBackendRoute::Mlpack:
            return "mlpack";
        case TorchBackendRoute::ManualReview:
        default:
            return "manual_review";
    }
}

inline TorchRoutingDecision route_torch_task(const TorchTaskBoundary& boundary) {
    const bool has_tensor_signal = boundary.has_image_tensor || boundary.has_dense_feature_tensor;
    const bool is_visual_task =
        boundary.kind == TorchTaskKind::VisualClassification ||
        boundary.kind == TorchTaskKind::VisualDetection ||
        boundary.kind == TorchTaskKind::VisualSegmentation ||
        boundary.kind == TorchTaskKind::VisualEmbedding;
    const bool is_mlpack_native_task =
        boundary.kind == TorchTaskKind::NumericClassification ||
        boundary.kind == TorchTaskKind::NumericClustering ||
        boundary.kind == TorchTaskKind::GeometryMatching ||
        boundary.kind == TorchTaskKind::GeometryClustering;

    if (is_mlpack_native_task &&
        boundary.geometry_or_descriptor_only &&
        has_tensor_signal &&
        (boundary.requires_gradient_training ||
         boundary.requires_end_to_end_learning)) {
        return {TorchBackendRoute::ManualReview, "structured geometry task with trainable tensor signal needs explicit backend selection"};
    }

    if (is_visual_task && has_tensor_signal) {
        return {TorchBackendRoute::Torch, "visual tensor task with learned representation"};
    }

    if (is_mlpack_native_task &&
        boundary.geometry_or_descriptor_only &&
        !boundary.requires_gradient_training &&
        !boundary.requires_end_to_end_learning &&
        !boundary.has_image_tensor) {
        return {TorchBackendRoute::Mlpack, "structured geometry or descriptor task without end-to-end learning"};
    }

    if (has_tensor_signal &&
        (boundary.requires_gradient_training || boundary.requires_end_to_end_learning)) {
        return {TorchBackendRoute::Torch, "gradient-based tensor training/inference path"};
    }

    if (!has_tensor_signal && is_mlpack_native_task) {
        return {TorchBackendRoute::Mlpack, "non-visual structured task defaults to mlpack"};
    }

    return {TorchBackendRoute::ManualReview, "task mixes signals and needs explicit backend selection"};
}

inline void validate_torch_train_io(const TorchTrainIO& io) {
    TORCH_CHECK(io.features.defined(), "TorchTrainIO.features must be defined");
    TORCH_CHECK(io.features.dim() == 2, "TorchTrainIO.features must be rank-2 [B, F], got ", io.features.sizes());
    TORCH_CHECK(io.features.size(0) > 0, "TorchTrainIO.features batch must be positive");
    TORCH_CHECK(io.features.size(1) > 0, "TorchTrainIO.features feature dim must be positive");
    TORCH_CHECK(io.labels.defined(), "TorchTrainIO.labels must be defined");
    TORCH_CHECK(io.labels.dim() == 1, "TorchTrainIO.labels must be rank-1 [B], got ", io.labels.sizes());
    TORCH_CHECK(io.labels.size(0) == io.features.size(0), "TorchTrainIO.labels batch mismatch");
    TORCH_CHECK(io.labels.scalar_type() == torch::kLong, "TorchTrainIO.labels must be int64/Long");
}

inline void validate_torch_infer_io(const TorchInferIO& io) {
    TORCH_CHECK(io.features.defined(), "TorchInferIO.features must be defined");
    TORCH_CHECK(io.features.dim() == 2, "TorchInferIO.features must be rank-2 [B, F], got ", io.features.sizes());
    TORCH_CHECK(io.features.size(0) > 0, "TorchInferIO.features batch must be positive");
    TORCH_CHECK(io.features.size(1) > 0, "TorchInferIO.features feature dim must be positive");
    TORCH_CHECK(io.topk > 0, "TorchInferIO.topk must be positive");
}

inline TorchTrainStepOutput run_minimal_torch_train_step(
    torch::nn::Linear& head,
    torch::optim::Optimizer& optimizer,
    const TorchTrainIO& io) {

    validate_torch_train_io(io);
    TORCH_CHECK(head, "run_minimal_torch_train_step requires a valid Linear head");

    head->train();
    optimizer.zero_grad();

    auto logits = head->forward(io.features);
    TORCH_CHECK(logits.dim() == 2, "train logits must be rank-2 [B, C], got ", logits.sizes());
    TORCH_CHECK(logits.size(0) == io.features.size(0), "train logits batch mismatch");

    auto loss = torch::nn::functional::cross_entropy(logits, io.labels);
    TORCH_CHECK(torch::isfinite(loss).item<bool>(), "train loss must be finite");

    loss.backward();
    optimizer.step();

    TorchTrainStepOutput out;
    out.logits = logits.detach();
    out.loss = loss.detach();
    out.batch_size = logits.size(0);
    out.num_classes = logits.size(1);
    return out;
}

inline TorchInferOutput run_minimal_torch_infer(
    torch::nn::Linear& head,
    const TorchInferIO& io) {

    validate_torch_infer_io(io);
    TORCH_CHECK(head, "run_minimal_torch_infer requires a valid Linear head");

    head->eval();
    torch::NoGradGuard no_grad;

    auto logits = head->forward(io.features);
    TORCH_CHECK(logits.dim() == 2, "infer logits must be rank-2 [B, C], got ", logits.sizes());
    TORCH_CHECK(logits.size(0) == io.features.size(0), "infer logits batch mismatch");

    const auto capped_topk = std::min<int64_t>(io.topk, logits.size(1));
    auto probabilities = torch::softmax(logits, 1);
    auto topk = probabilities.topk(capped_topk, 1, true, true);

    TorchInferOutput out;
    out.logits = logits;
    out.scores = std::get<0>(topk);
    out.labels = std::get<1>(topk);
    return out;
}

#endif
