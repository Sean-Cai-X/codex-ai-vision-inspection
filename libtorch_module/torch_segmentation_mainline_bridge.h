#ifndef TORCH_SEGMENTATION_MAINLINE_BRIDGE_H
#define TORCH_SEGMENTATION_MAINLINE_BRIDGE_H

#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>

#include "torch_DeepLabV3.h"
#include "torch_deeplabv3_plus.h"
#include "torch_tuning_profiles.h"

enum class SegmentationDevicePolicy {
    PreferCUDA,
    ForceCPU,
    ForceCUDA
};

inline const char* segmentation_device_policy_name(SegmentationDevicePolicy policy) {
    switch (policy) {
    case SegmentationDevicePolicy::PreferCUDA:
        return "prefer-cuda";
    case SegmentationDevicePolicy::ForceCPU:
        return "force-cpu";
    case SegmentationDevicePolicy::ForceCUDA:
        return "force-cuda";
    default:
        return "unknown";
    }
}

inline torch::Device resolve_segmentation_device(SegmentationDevicePolicy policy) {
    switch (policy) {
    case SegmentationDevicePolicy::ForceCPU:
        return torch::Device(torch::kCPU);
    case SegmentationDevicePolicy::ForceCUDA:
        TORCH_CHECK(torch::cuda::is_available(),
            "Segmentation runner requested CUDA, but CUDA is not available");
        return torch::Device(torch::kCUDA);
    case SegmentationDevicePolicy::PreferCUDA:
    default:
        return torch::cuda::is_available() ? torch::Device(torch::kCUDA) : torch::Device(torch::kCPU);
    }
}

inline std::string resolve_segmentation_env_or_default(const char* env_name,
                                                       const char* fallback)
{
    const char* value = std::getenv(env_name);
    if (value != nullptr && value[0] != '\0') {
        return std::string(value);
    }
    return std::string(fallback);
}

inline std::string resolve_deeplab_weight_path()
{
    return resolve_segmentation_env_or_default(
        "LIBTORCH_MODULE_DEEPLAB_WEIGHTS",
        "deeplabv3_mobilenet_v3_large-fc3c493d.pth");
}

struct SegmentationOptimizerConfig {
    float lr = 1e-3f;
    float weight_decay = 0.0f;

    void validate() const {
        TORCH_CHECK(lr > 0.0f, "Segmentation optimizer lr must be positive");
        TORCH_CHECK(weight_decay >= 0.0f, "Segmentation optimizer weight_decay must be non-negative");
    }
};

struct SegmentationRuntimeConfig {
    int max_train_batches = 1;
    int log_interval = 1;

    void validate() const {
        TORCH_CHECK(max_train_batches > 0, "Segmentation runtime max_train_batches must be positive");
        TORCH_CHECK(log_interval > 0, "Segmentation runtime log_interval must be positive");
    }
};

struct SegmentationMainlineRunnerConfig {
    std::string run_name = "segmentation_mainline";
    std::string decoder = "deeplabv3plus";
    std::string backbone = "mobilenet_v3_large";
    int num_classes = 2;
    int input_size = 128;
    int batch_size = 2;
    SegmentationOptimizerConfig optimizer;
    SegmentationRuntimeConfig runtime;
    SegmentationDevicePolicy device_policy = SegmentationDevicePolicy::PreferCUDA;
    bool enable_smoke_train = true;
    bool enable_eval = true;

    void validate() const {
        TORCH_CHECK(!run_name.empty(), "Segmentation runner run_name cannot be empty");
        TORCH_CHECK(decoder == "deeplabv3" || decoder == "deeplabv3plus",
            "Segmentation decoder must be deeplabv3 or deeplabv3plus");
        if (decoder == "deeplabv3plus") {
            TORCH_CHECK(backbone == "mobilenet_v3_large" || backbone == "resnet18",
                "DeepLabV3Plus backbone must be mobilenet_v3_large or resnet18");
        } else {
            TORCH_CHECK(backbone == "resnet18" || backbone == "resnet50",
                "DeepLabV3 backbone must use resnet18 or resnet50 baseline naming");
        }
        TORCH_CHECK(num_classes > 1, "Segmentation runner num_classes must be greater than one");
        TORCH_CHECK(input_size > 0, "Segmentation runner input_size must be positive");
        TORCH_CHECK(batch_size > 0, "Segmentation runner batch_size must be positive");
        optimizer.validate();
        runtime.validate();
        TORCH_CHECK(enable_smoke_train || enable_eval,
            "Segmentation runner must enable at least one stage");
    }
};

inline SegmentationMainlineRunnerConfig make_segmentation_mainline_runner_config(
    const std::string& decoder = "deeplabv3plus",
    const std::string& backbone = "mobilenet_v3_large",
    int num_classes = 2,
    int input_size = 128,
    int batch_size = 2)
{
    SegmentationMainlineRunnerConfig config;
    config.decoder = decoder;
    config.backbone = backbone;
    config.num_classes = num_classes;
    config.input_size = input_size;
    config.batch_size = batch_size;
    return config;
}

inline torch::Tensor forward_segmentation_logits(
    const SegmentationMainlineRunnerConfig& config,
    torch::Tensor imgs,
    std::vector<torch::Tensor>* parameters = nullptr)
{
    config.validate();
    auto device = resolve_segmentation_device(config.device_policy);
    imgs = imgs.to(device);

    if (config.decoder == "deeplabv3") {
        DeepLabV3 model(config.num_classes);
        model->to(device);
        if (parameters != nullptr) {
            *parameters = model->parameters();
        }
        return model->forward(imgs);
    }

    DeepLabV3Plus model(config.backbone, config.num_classes);
    model->to(device);
    if (parameters != nullptr) {
        *parameters = model->parameters();
    }
    return model->forward(imgs).at("out");
}

struct SegmentationSmokeStepResult {
    double loss = 0.0;
    double grad_mean = 0.0;
    int batch_size = 0;
    int input_size = 0;

    void validate() const {
        TORCH_CHECK(std::isfinite(loss), "Segmentation smoke loss must be finite");
        TORCH_CHECK(std::isfinite(grad_mean), "Segmentation smoke grad_mean must be finite");
        TORCH_CHECK(batch_size > 0, "Segmentation smoke batch_size must be positive");
        TORCH_CHECK(input_size > 0, "Segmentation smoke input_size must be positive");
    }
};

struct SegmentationEvalSummary {
    double loss = 0.0;
    double foreground_iou = 0.0;
    double avg_confidence = 0.0;
    int sample_count = 0;

    void validate() const {
        TORCH_CHECK(std::isfinite(loss), "Segmentation eval loss must be finite");
        TORCH_CHECK(std::isfinite(foreground_iou), "Segmentation eval foreground_iou must be finite");
        TORCH_CHECK(std::isfinite(avg_confidence), "Segmentation eval avg_confidence must be finite");
        TORCH_CHECK(foreground_iou >= 0.0 && foreground_iou <= 1.0,
            "Segmentation eval foreground_iou must be in [0, 1]");
        TORCH_CHECK(avg_confidence >= 0.0 && avg_confidence <= 1.0,
            "Segmentation eval avg_confidence must be in [0, 1]");
        TORCH_CHECK(sample_count > 0, "Segmentation eval sample_count must be positive");
    }
};

inline SegmentationSmokeStepResult run_segmentation_smoke_train_step(
    torch::Tensor imgs,
    torch::Tensor masks,
    const SegmentationMainlineRunnerConfig& config)
{
    config.validate();
    TORCH_CHECK(config.enable_smoke_train, "Segmentation smoke train requires enable_smoke_train");
    TORCH_CHECK(imgs.dim() == 4, "Segmentation smoke imgs must be BCHW");
    TORCH_CHECK(masks.dim() == 3, "Segmentation smoke masks must be BHW");

    auto device = resolve_segmentation_device(config.device_policy);
    imgs = imgs.to(device);
    masks = masks.to(device, torch::kLong);

    std::vector<torch::Tensor> parameters;
    auto logits = forward_segmentation_logits(config, imgs, &parameters);
    auto loss = torch::nn::functional::cross_entropy(logits, masks);
    TORCH_CHECK(torch::isfinite(loss).item<bool>(), "Segmentation smoke loss must be finite");
    loss.backward();

    bool has_grad = false;
    double grad_mean = 0.0;
    for (const auto& parameter : parameters) {
        if (parameter.grad().defined()) {
            grad_mean = parameter.grad().abs().mean().item<double>();
            has_grad = true;
            break;
        }
    }
    TORCH_CHECK(has_grad, "Segmentation smoke train must produce gradients");

    SegmentationSmokeStepResult result;
    result.loss = loss.item<double>();
    result.grad_mean = grad_mean;
    result.batch_size = static_cast<int>(imgs.size(0));
    result.input_size = static_cast<int>(imgs.size(2));
    return result;
}

inline SegmentationEvalSummary run_segmentation_eval_summary(
    torch::Tensor imgs,
    torch::Tensor masks,
    const SegmentationMainlineRunnerConfig& config)
{
    config.validate();
    TORCH_CHECK(config.enable_eval, "Segmentation eval requires enable_eval");
    TORCH_CHECK(imgs.dim() == 4, "Segmentation eval imgs must be BCHW");
    TORCH_CHECK(masks.dim() == 3, "Segmentation eval masks must be BHW");

    auto device = resolve_segmentation_device(config.device_policy);
    imgs = imgs.to(device);
    masks = masks.to(device, torch::kLong);

    torch::NoGradGuard no_grad;
    auto logits = forward_segmentation_logits(config, imgs, nullptr);
    auto loss = torch::nn::functional::cross_entropy(logits, masks);
    auto probs = torch::softmax(logits, 1);
    auto pred = probs.argmax(1);
    auto foreground_pred = pred.gt(0);
    auto foreground_gt = masks.gt(0);
    auto intersection = foreground_pred.logical_and(foreground_gt).sum().item<double>();
    auto union_count = foreground_pred.logical_or(foreground_gt).sum().item<double>();
    auto foreground_iou = union_count > 0.0 ? intersection / union_count : 1.0;
    auto avg_confidence = std::get<0>(probs.max(1)).mean().item<double>();

    SegmentationEvalSummary summary;
    summary.loss = loss.item<double>();
    summary.foreground_iou = foreground_iou;
    summary.avg_confidence = avg_confidence;
    summary.sample_count = static_cast<int>(imgs.size(0));
    return summary;
}

struct SegmentationMainlineSessionResult {
    SegmentationMainlineRunnerConfig config;
    SegmentationSmokeStepResult smoke;
    SegmentationEvalSummary eval;
    TuningRunReport flat_run;
    bool passed = false;
    std::string summary;

    void validate() const {
        config.validate();
        smoke.validate();
        eval.validate();
        flat_run.validate();
        TORCH_CHECK(!summary.empty(), "Segmentation mainline session summary cannot be empty");
        TORCH_CHECK(flat_run.track_name == "segmentation_mainline",
            "Segmentation mainline session flat run must target segmentation_mainline");
    }
};

struct SegmentationTrainerStageStatus {
    std::string stage_name;
    bool passed = false;
    std::string detail;

    void validate() const {
        TORCH_CHECK(!stage_name.empty(), "Segmentation trainer stage_name cannot be empty");
        TORCH_CHECK(!detail.empty(), "Segmentation trainer detail cannot be empty");
    }
};

struct SegmentationTrainerTimeline {
    std::vector<SegmentationTrainerStageStatus> stages;

    void validate() const {
        TORCH_CHECK(!stages.empty(), "Segmentation trainer timeline must contain at least one stage");
        for (const auto& stage : stages) {
            stage.validate();
        }
    }
};

struct SegmentationTrainerLifecycleSummary {
    int passed_stages = 0;
    int total_stages = 0;
    std::string final_stage_name;
    bool all_passed = false;
    std::string summary;

    void validate() const {
        TORCH_CHECK(total_stages > 0, "Segmentation trainer lifecycle total_stages must be positive");
        TORCH_CHECK(passed_stages >= 0 && passed_stages <= total_stages,
            "Segmentation trainer lifecycle passed_stages out of range");
        TORCH_CHECK(!final_stage_name.empty(),
            "Segmentation trainer lifecycle final_stage_name cannot be empty");
        TORCH_CHECK(!summary.empty(), "Segmentation trainer lifecycle summary cannot be empty");
    }
};

struct SegmentationTrainerSessionResult {
    SegmentationMainlineRunnerConfig config;
    SegmentationMainlineSessionResult session;
    bool passed = false;
    std::string summary;

    void validate() const {
        config.validate();
        session.validate();
        TORCH_CHECK(!summary.empty(), "Segmentation trainer session summary cannot be empty");
    }
};

struct SegmentationTrainerAnalysis {
    SegmentationTrainerTimeline timeline;
    std::vector<TuningComparisonRow> comparison_rows;
    TuningRecommendation recommendation;
    SegmentationTrainerLifecycleSummary lifecycle_summary;
    TuningRunReport flat_run;

    void validate() const {
        timeline.validate();
        TORCH_CHECK(!comparison_rows.empty(), "Segmentation trainer analysis must contain comparison rows");
        for (const auto& row : comparison_rows) {
            row.validate();
        }
        recommendation.validate();
        lifecycle_summary.validate();
        flat_run.validate();
        TORCH_CHECK(lifecycle_summary.total_stages == static_cast<int>(timeline.stages.size()),
            "Segmentation trainer lifecycle summary must align with timeline");
        TORCH_CHECK(recommendation.track_name == "segmentation_mainline",
            "Segmentation trainer recommendation track must be segmentation_mainline");
        TORCH_CHECK(flat_run.track_name == "segmentation_mainline",
            "Segmentation trainer flat run must target segmentation_mainline");
    }
};

struct SegmentationUnifiedMainlineBundle {
    SegmentationMainlineSessionResult mainline;
    SegmentationTrainerAnalysis trainer;
    std::vector<TuningComparisonRow> comparison_rows;
    TuningRecommendation recommendation;
    TuningRunReport flat_run;

    void validate() const {
        mainline.validate();
        trainer.validate();
        TORCH_CHECK(!comparison_rows.empty(), "Segmentation unified bundle must contain comparison rows");
        for (const auto& row : comparison_rows) {
            row.validate();
        }
        recommendation.validate();
        flat_run.validate();
        TORCH_CHECK(recommendation.track_name == "segmentation_mainline",
            "Segmentation unified recommendation track must be segmentation_mainline");
        TORCH_CHECK(flat_run.track_name == "segmentation_mainline",
            "Segmentation unified flat run must target segmentation_mainline");
    }
};

struct SegmentationUnifiedMainlineSummary {
    int total_outcomes = 0;
    int total_comparisons = 0;
    int total_recommendations = 0;
    bool all_passed = false;
    std::string summary;

    void validate() const {
        TORCH_CHECK(total_outcomes > 0, "Segmentation unified summary total_outcomes must be positive");
        TORCH_CHECK(total_comparisons > 0, "Segmentation unified summary total_comparisons must be positive");
        TORCH_CHECK(total_recommendations > 0,
            "Segmentation unified summary total_recommendations must be positive");
        TORCH_CHECK(!summary.empty(), "Segmentation unified summary cannot be empty");
    }
};

inline SegmentationMainlineSessionResult run_segmentation_mainline_session(
    torch::Tensor train_imgs,
    torch::Tensor train_masks,
    torch::Tensor eval_imgs,
    torch::Tensor eval_masks,
    const SegmentationMainlineRunnerConfig& config)
{
    config.validate();

    SegmentationMainlineSessionResult result;
    result.config = config;
    result.smoke = run_segmentation_smoke_train_step(train_imgs, train_masks, config);
    result.eval = run_segmentation_eval_summary(eval_imgs, eval_masks, config);
    result.flat_run.run_name = "segmentation_mainline_session_run";
    result.flat_run.track_name = "segmentation_mainline";
    result.flat_run.outcomes = {
        {
            "segmentation_train_smoke_stage",
            {
                {"decoder", config.decoder},
                {"backbone", config.backbone}
            },
            {
                {TuningMetricKind::Loss, "loss", result.smoke.loss},
                {TuningMetricKind::MemoryMb, "grad_mean", result.smoke.grad_mean}
            },
            true,
            "Segmentation smoke training run kept finite loss and gradients."
        },
        {
            "segmentation_eval_stage",
            {
                {"sample_count", std::to_string(result.eval.sample_count)}
            },
            {
                {TuningMetricKind::Loss, "loss", result.eval.loss},
                {TuningMetricKind::ForegroundIoU, "foreground_iou", result.eval.foreground_iou},
                {TuningMetricKind::LatencyMs, "avg_confidence", result.eval.avg_confidence}
            },
            true,
            "Segmentation eval run produced finite loss and foreground IoU."
        }
    };
    result.flat_run.all_passed = true;
    result.flat_run.summary = "Segmentation mainline session aggregates smoke training and eval outcomes.";
    result.passed = true;
    result.summary = "Segmentation mainline session completed smoke training and evaluation gates.";
    result.validate();
    return result;
}

inline SegmentationTrainerSessionResult run_segmentation_trainer_session(
    torch::Tensor train_imgs,
    torch::Tensor train_masks,
    torch::Tensor eval_imgs,
    torch::Tensor eval_masks,
    const SegmentationMainlineRunnerConfig& config)
{
    config.validate();

    SegmentationTrainerSessionResult result;
    result.config = config;
    result.session = run_segmentation_mainline_session(train_imgs, train_masks, eval_imgs, eval_masks, config);
    result.passed = result.session.passed;
    result.summary = "Segmentation trainer session completed smoke training and evaluation gates.";
    result.validate();
    return result;
}

inline SegmentationTrainerTimeline build_segmentation_trainer_timeline(
    const SegmentationTrainerSessionResult& result)
{
    result.validate();

    SegmentationTrainerTimeline timeline;
    timeline.stages.push_back({
        "segmentation_logging",
        result.config.runtime.log_interval > 0,
        std::string("log_interval=") + std::to_string(result.config.runtime.log_interval)
    });
    timeline.stages.push_back({
        "segmentation_smoke_train",
        result.session.smoke.grad_mean >= 0.0,
        std::string("loss=") + std::to_string(result.session.smoke.loss) +
            " grad_mean=" + std::to_string(result.session.smoke.grad_mean)
    });
    timeline.stages.push_back({
        "segmentation_eval",
        result.session.eval.sample_count > 0,
        std::string("loss=") + std::to_string(result.session.eval.loss) +
            " foreground_iou=" + std::to_string(result.session.eval.foreground_iou)
    });
    timeline.stages.push_back({
        "segmentation_report",
        result.passed,
        result.summary
    });
    timeline.validate();
    return timeline;
}

inline std::vector<TuningComparisonRow> build_segmentation_trainer_comparison_rows(
    const SegmentationTrainerSessionResult& result)
{
    result.validate();

    std::vector<TuningComparisonRow> rows;
    rows.push_back({
        "segmentation_trainer_smoke_stage",
        "segmentation_trainer_smoke",
        "Selected smoke stage because finite segmentation loss and gradients were observed.",
        {{
            "segmentation_trainer_smoke",
            {
                {TuningMetricKind::Loss, "loss", result.session.smoke.loss},
                {TuningMetricKind::MemoryMb, "grad_mean", result.session.smoke.grad_mean}
            },
            true,
            "Segmentation trainer smoke result"
        }}
    });
    rows.push_back({
        "segmentation_trainer_eval_stage",
        "segmentation_trainer_eval",
        "Selected eval stage because finite segmentation metrics were produced.",
        {{
            "segmentation_trainer_eval",
            {
                {TuningMetricKind::Loss, "loss", result.session.eval.loss},
                {TuningMetricKind::ForegroundIoU, "foreground_iou", result.session.eval.foreground_iou},
                {TuningMetricKind::LatencyMs, "avg_confidence", result.session.eval.avg_confidence}
            },
            true,
            "Segmentation trainer eval result"
        }}
    });
    for (const auto& row : rows) {
        row.validate();
    }
    return rows;
}

inline TuningRecommendation build_segmentation_trainer_recommendation(
    const SegmentationTrainerSessionResult& result)
{
    result.validate();

    TuningRecommendation recommendation;
    recommendation.track_name = "segmentation_mainline";
    recommendation.selected_experiments = {
        {"segmentation_trainer_smoke_stage", "segmentation_trainer_smoke"},
        {"segmentation_trainer_eval_stage", "segmentation_trainer_eval"}
    };
    recommendation.summary =
        "Segmentation trainer recommendation keeps smoke training and eval gates as the current minimum path.";
    recommendation.validate();
    return recommendation;
}

inline SegmentationTrainerLifecycleSummary build_segmentation_trainer_lifecycle_summary(
    const SegmentationTrainerTimeline& timeline)
{
    timeline.validate();

    SegmentationTrainerLifecycleSummary summary;
    summary.total_stages = static_cast<int>(timeline.stages.size());
    for (const auto& stage : timeline.stages) {
        if (stage.passed) {
            summary.passed_stages += 1;
        }
    }
    summary.final_stage_name = timeline.stages.back().stage_name;
    summary.all_passed = summary.passed_stages == summary.total_stages;
    summary.summary = "Segmentation trainer lifecycle passed " +
        std::to_string(summary.passed_stages) + "/" +
        std::to_string(summary.total_stages) +
        " stages; final_stage=" + summary.final_stage_name;
    summary.validate();
    return summary;
}

inline TuningRunReport build_segmentation_trainer_flat_run(const SegmentationTrainerSessionResult& result) {
    result.validate();

    TuningRunReport report;
    report.run_name = "segmentation_trainer_session_run";
    report.track_name = "segmentation_mainline";
    report.outcomes = result.session.flat_run.outcomes;
    report.all_passed = result.passed;
    report.summary = "Segmentation trainer flat run aggregates smoke and eval outcomes.";
    report.validate();
    return report;
}

inline SegmentationTrainerAnalysis build_segmentation_trainer_analysis(
    const SegmentationTrainerSessionResult& result)
{
    result.validate();

    SegmentationTrainerAnalysis analysis;
    analysis.timeline = build_segmentation_trainer_timeline(result);
    analysis.comparison_rows = build_segmentation_trainer_comparison_rows(result);
    analysis.recommendation = build_segmentation_trainer_recommendation(result);
    analysis.lifecycle_summary = build_segmentation_trainer_lifecycle_summary(analysis.timeline);
    analysis.flat_run = build_segmentation_trainer_flat_run(result);
    analysis.validate();
    return analysis;
}

inline SegmentationUnifiedMainlineBundle build_segmentation_unified_mainline_bundle(
    const SegmentationMainlineSessionResult& mainline,
    const SegmentationTrainerAnalysis& trainer)
{
    mainline.validate();
    trainer.validate();

    SegmentationUnifiedMainlineBundle bundle;
    bundle.mainline = mainline;
    bundle.trainer = trainer;
    bundle.comparison_rows = trainer.comparison_rows;

    bundle.recommendation.track_name = "segmentation_mainline";
    bundle.recommendation.selected_experiments = {
        {"segmentation_mainline_smoke_stage", "segmentation_mainline_smoke_current"},
        {"segmentation_mainline_eval_stage", "segmentation_mainline_eval_current"}
    };
    bundle.recommendation.selected_experiments.insert(
        bundle.recommendation.selected_experiments.end(),
        trainer.recommendation.selected_experiments.begin(),
        trainer.recommendation.selected_experiments.end());
    bundle.recommendation.summary =
        "Unified segmentation mainline recommendation keeps both mainline smoke/eval gates and trainer smoke/eval gates.";

    bundle.flat_run.run_name = "segmentation_unified_mainline_run";
    bundle.flat_run.track_name = "segmentation_mainline";
    bundle.flat_run.outcomes = mainline.flat_run.outcomes;
    bundle.flat_run.outcomes.insert(
        bundle.flat_run.outcomes.end(),
        trainer.flat_run.outcomes.begin(),
        trainer.flat_run.outcomes.end());
    bundle.flat_run.all_passed = mainline.flat_run.all_passed && trainer.flat_run.all_passed;
    bundle.flat_run.summary =
        "Unified segmentation mainline run aggregates mainline session and trainer session outcomes.";

    bundle.validate();
    return bundle;
}

inline SegmentationUnifiedMainlineSummary build_segmentation_unified_mainline_summary(
    const SegmentationUnifiedMainlineBundle& bundle)
{
    bundle.validate();

    SegmentationUnifiedMainlineSummary summary;
    summary.total_outcomes = static_cast<int>(bundle.flat_run.outcomes.size());
    summary.total_comparisons = static_cast<int>(bundle.comparison_rows.size());
    summary.total_recommendations = static_cast<int>(bundle.recommendation.selected_experiments.size());
    summary.all_passed = bundle.mainline.flat_run.all_passed && bundle.trainer.flat_run.all_passed;
    summary.summary =
        "Segmentation unified mainline bundle aggregates " +
        std::to_string(summary.total_outcomes) + " outcomes, " +
        std::to_string(summary.total_comparisons) + " comparisons, and " +
        std::to_string(summary.total_recommendations) +
        " recommended selections across mainline and trainer gates.";
    summary.validate();
    return summary;
}

#endif
