#ifndef TORCH_YOLO_MAINLINE_BRIDGE_H
#define TORCH_YOLO_MAINLINE_BRIDGE_H

#include <vector>

#include "torch_tuning_profiles.h"
#include "torch_v8.h"

struct YoloSmokeObservation {
    int input_size = 640;
    int batch_size = 1;
    int max_train_batches = 1;
    double step_loss = 0.0;
    bool gradients_ok = false;

    void validate() const {
        TORCH_CHECK(input_size > 0, "YOLO smoke observation input_size must be positive");
        TORCH_CHECK(batch_size > 0, "YOLO smoke observation batch_size must be positive");
        TORCH_CHECK(max_train_batches > 0, "YOLO smoke observation max_train_batches must be positive");
        TORCH_CHECK(std::isfinite(step_loss), "YOLO smoke observation step_loss must be finite");
    }
};

enum class YoloDevicePolicy {
    PreferCUDA,
    ForceCPU,
    ForceCUDA
};

inline const char* yolo_device_policy_name(YoloDevicePolicy policy) {
    switch (policy) {
    case YoloDevicePolicy::PreferCUDA:
        return "prefer-cuda";
    case YoloDevicePolicy::ForceCPU:
        return "force-cpu";
    case YoloDevicePolicy::ForceCUDA:
        return "force-cuda";
    default:
        return "unknown";
    }
}

inline torch::Device resolve_yolo_mainline_device(YoloDevicePolicy policy) {
    switch (policy) {
    case YoloDevicePolicy::ForceCPU:
        return torch::Device(torch::kCPU);
    case YoloDevicePolicy::ForceCUDA:
        TORCH_CHECK(torch::cuda::is_available(),
            "YOLO mainline runner config requested CUDA, but CUDA is not available");
        return torch::Device(torch::kCUDA);
    case YoloDevicePolicy::PreferCUDA:
    default:
        return torch::cuda::is_available() ? torch::Device(torch::kCUDA) : torch::Device(torch::kCPU);
    }
}

enum class YoloSchedulerPolicy {
    None,
    StepDecay,
    CosineStub
};

inline const char* yolo_scheduler_policy_name(YoloSchedulerPolicy policy) {
    switch (policy) {
    case YoloSchedulerPolicy::None:
        return "none";
    case YoloSchedulerPolicy::StepDecay:
        return "step";
    case YoloSchedulerPolicy::CosineStub:
        return "cosine";
    default:
        return "unknown";
    }
}

struct YoloOptimizerStageConfig {
    YoloOptimizerConfig base;
    int warmup_steps = 0;

    void validate() const {
        base.validate();
        TORCH_CHECK(warmup_steps >= 0, "YOLO optimizer stage warmup_steps must be non-negative");
    }
};

struct YoloSchedulerStageConfig {
    YoloSchedulerPolicy policy = YoloSchedulerPolicy::None;
    int step_size = 1;
    float gamma = 0.1f;
    int total_steps = 0;

    void validate() const {
        TORCH_CHECK(step_size > 0, "YOLO scheduler stage step_size must be positive");
        TORCH_CHECK(gamma > 0.0f, "YOLO scheduler stage gamma must be positive");
        TORCH_CHECK(total_steps >= 0, "YOLO scheduler stage total_steps must be non-negative");
    }
};

struct YoloMainlineRunnerConfig {
    std::string run_name = "yolo_mainline";
    ModelConfig model = ModelConfig::get_config("nano");
    YoloModelBuildConfig build;
    YoloOptimizerConfig optimizer;
    YoloOptimizerStageConfig optimizer_stage;
    YoloSchedulerStageConfig scheduler_stage;
    YoloTrainRuntimeConfig runtime;
    YoloValidationConfig validation;
    YoloDevicePolicy device_policy = YoloDevicePolicy::PreferCUDA;
    bool enable_smoke_train = true;
    bool enable_eval = true;
    bool reuse_trained_model_for_eval = false;

    void validate() const {
        TORCH_CHECK(!run_name.empty(), "YOLO mainline runner config run_name cannot be empty");
        model.validate();
        optimizer.validate();
        optimizer_stage.validate();
        scheduler_stage.validate();
        runtime.validate();
        validation.validate();
        TORCH_CHECK(std::fabs(optimizer.lr - optimizer_stage.base.lr) < 1e-9f,
            "YOLO runner optimizer and optimizer_stage base lr must stay aligned");
        TORCH_CHECK(std::fabs(optimizer.momentum - optimizer_stage.base.momentum) < 1e-9f,
            "YOLO runner optimizer and optimizer_stage momentum must stay aligned");
        TORCH_CHECK(std::fabs(optimizer.weight_decay - optimizer_stage.base.weight_decay) < 1e-9f,
            "YOLO runner optimizer and optimizer_stage weight_decay must stay aligned");
        TORCH_CHECK(enable_smoke_train || enable_eval,
            "YOLO mainline runner config must enable at least one stage");
    }
};

inline YoloMainlineRunnerConfig make_yolo_mainline_runner_config(
    const ModelConfig& model,
    const TrainConfig& train_config,
    const std::string& pretrained_weights = "",
    int smoke_batches = 2,
    const YoloModelBuildConfig& build_config = {})
{
    YoloMainlineRunnerConfig config;
    config.model = model;
    config.build = build_config;
    config.optimizer = train_config.optimizer_config();
    config.optimizer_stage.base = config.optimizer;
    config.runtime = train_config.smoke_runtime_config(pretrained_weights, smoke_batches);
    config.validation = train_config.validation_config();
    return config;
}

inline TuningRunReport build_yolo_smoke_run_report(const YoloSmokeObservation& observation) {
    observation.validate();
    return build_yolo_smoke_run_report(
        observation.input_size,
        observation.batch_size,
        observation.max_train_batches,
        observation.step_loss,
        observation.gradients_ok);
}

inline std::vector<TuningClassSummary> build_yolo_class_summaries(
    const YOLOv8Impl::ValidationSummary& summary)
{
    std::vector<TuningClassSummary> out;
    out.reserve(summary.per_class.size());
    for (const auto& [class_id, stats] : summary.per_class) {
        out.push_back({
            class_id,
            stats.predicted_boxes,
            stats.target_boxes,
            stats.true_positives,
            stats.false_positives,
            stats.false_negatives,
            stats.precision,
            stats.recall,
            stats.f1
        });
    }
    return out;
}

inline YoloEvalRunReport build_yolo_eval_run_report(const YOLOv8Impl::ValidationSummary& summary) {
    return build_yolo_eval_run_report(
        summary.loss,
        summary.precision,
        summary.recall,
        summary.f1,
        summary.matched_iou,
        summary.true_positives,
        summary.false_positives,
        summary.false_negatives,
        summary.predicted_boxes,
        summary.target_boxes,
        build_yolo_class_summaries(summary));
}

inline YoloMainlineBundle build_yolo_mainline_bundle(
    const YoloSmokeObservation& smoke_observation,
    const YOLOv8Impl::ValidationSummary& summary)
{
    return build_yolo_mainline_bundle(
        build_yolo_smoke_run_report(smoke_observation),
        build_yolo_eval_run_report(summary));
}

struct YoloMainlineExecutionResult {
    YoloMainlineRunnerConfig config;
    YoloMainlineBundle bundle;

    void validate() const {
        config.validate();
        bundle.validate();
        TORCH_CHECK(bundle.flat_run.track_name == "yolo_mainline",
            "YOLO mainline execution result flat run must target yolo_mainline");
    }
};

inline YoloMainlineExecutionResult build_yolo_mainline_execution_result(
    const YoloMainlineRunnerConfig& config,
    const YoloSmokeObservation& smoke_observation,
    const YOLOv8Impl::ValidationSummary& summary)
{
    config.validate();
    YoloMainlineExecutionResult result;
    result.config = config;
    result.bundle = build_yolo_mainline_bundle(smoke_observation, summary);
    return result;
}

struct YoloSmokeStepResult {
    YoloSmokeObservation observation;
    double grad_mean = 0.0;

    void validate() const {
        observation.validate();
        TORCH_CHECK(std::isfinite(grad_mean), "YOLO smoke step grad_mean must be finite");
        TORCH_CHECK(grad_mean >= 0.0, "YOLO smoke step grad_mean must be non-negative");
    }
};

inline YoloSmokeStepResult run_yolo_smoke_train_step(
    YOLOv8& model,
    torch::Tensor imgs,
    torch::Tensor targets,
    const YoloMainlineRunnerConfig& config)
{
    config.validate();
    TORCH_CHECK(config.enable_smoke_train, "YOLO smoke train step requires enable_smoke_train");
    TORCH_CHECK(imgs.dim() == 4, "YOLO smoke train step imgs must be BCHW");
    TORCH_CHECK(targets.dim() == 3 && targets.size(2) == 6, "YOLO smoke train step targets must be [B, N, 6]");

    auto device = resolve_yolo_mainline_device(config.device_policy);
    imgs = imgs.to(device);
    targets = targets.to(device);
    model->to(device);
    torch::optim::SGD optimizer(model->parameters(),
        torch::optim::SGDOptions(config.optimizer_stage.base.lr)
            .momentum(config.optimizer_stage.base.momentum)
            .weight_decay(config.optimizer_stage.base.weight_decay));

    optimizer.zero_grad();
    auto result = model->train_step(imgs, targets);
    auto loss = std::get<0>(result);
    TORCH_CHECK(torch::isfinite(loss).item<bool>(), "YOLO smoke train step loss must be finite");
    loss.backward();

    bool has_grad = false;
    double grad_mean = 0.0;
    for (const auto& parameter : model->parameters()) {
        if (parameter.grad().defined()) {
            grad_mean = parameter.grad().abs().mean().item<double>();
            has_grad = true;
            break;
        }
    }
    TORCH_CHECK(has_grad, "YOLO smoke train step must produce gradients");
    optimizer.step();

    YoloSmokeStepResult step_result;
    step_result.observation.input_size = static_cast<int>(imgs.size(2));
    step_result.observation.batch_size = static_cast<int>(imgs.size(0));
    step_result.observation.max_train_batches = config.runtime.max_train_batches;
    step_result.observation.step_loss = loss.item<double>();
    step_result.observation.gradients_ok = has_grad;
    step_result.grad_mean = grad_mean;
    return step_result;
}

inline YOLOv8Impl::ValidationSummary run_yolo_eval_summary(
    YOLOv8& model,
    const std::string& data_path,
    const YoloMainlineRunnerConfig& config)
{
    config.validate();
    TORCH_CHECK(config.enable_eval, "YOLO eval summary requires enable_eval");
    model->to(resolve_yolo_mainline_device(config.device_policy));
    return model->val_summary(data_path, make_yolo_eval_config(config.model, config.validation));
}

struct YoloMainlineSessionResult {
    YoloSmokeStepResult smoke_step;
    YoloMainlineExecutionResult execution;

    void validate() const {
        smoke_step.validate();
        execution.validate();
        TORCH_CHECK(
            static_cast<int>(smoke_step.observation.max_train_batches) ==
            execution.config.runtime.max_train_batches,
            "YOLO mainline session result smoke batch cap must match runner config");
    }
};

struct YoloTrainerSessionResult {
    YoloMainlineRunnerConfig config;
    YoloMainlineSessionResult session;
    bool passed = false;
    std::string summary;

    void validate() const {
        config.validate();
        session.validate();
        TORCH_CHECK(!summary.empty(), "YOLO trainer session result summary cannot be empty");
        TORCH_CHECK(passed == session.execution.bundle.round_report.round_passed,
            "YOLO trainer session result pass flag must match round report");
        TORCH_CHECK(config.run_name == session.execution.config.run_name,
            "YOLO trainer session result config run name mismatch");
    }
};

struct YoloTrainerStageStatus {
    std::string stage_name;
    bool passed = false;
    std::string detail;

    void validate() const {
        TORCH_CHECK(!stage_name.empty(), "YOLO trainer stage status stage_name cannot be empty");
        TORCH_CHECK(!detail.empty(), "YOLO trainer stage status detail cannot be empty");
    }
};

struct YoloTrainerTimeline {
    std::vector<YoloTrainerStageStatus> stages;

    void validate() const {
        TORCH_CHECK(!stages.empty(), "YOLO trainer timeline must contain at least one stage");
        for (const auto& stage : stages) {
            stage.validate();
        }
    }
};

inline YoloTrainerTimeline build_yolo_trainer_timeline(const YoloTrainerSessionResult& result) {
    result.validate();

    YoloTrainerTimeline timeline;
    timeline.stages.push_back({
        "yolo_resume",
        true,
        result.config.runtime.pretrained_weights.empty()
            ? "pretrained_weights=none"
            : std::string("pretrained_weights=") + result.config.runtime.pretrained_weights
    });
    timeline.stages.push_back({
        "yolo_export",
        true,
        std::string("export_stub=model_state_ready run=") + result.config.run_name
    });
    timeline.stages.push_back({
        "yolo_logging",
        result.config.runtime.log_interval > 0,
        std::string("log_interval=") + std::to_string(result.config.runtime.log_interval)
    });
    timeline.stages.push_back({
        "yolo_checkpoint",
        result.config.runtime.checkpoint.save_interval > 0,
        std::string("save_path=") + result.config.runtime.checkpoint.save_path +
            " save_interval=" + std::to_string(result.config.runtime.checkpoint.save_interval)
    });
    timeline.stages.push_back({
        "yolo_smoke_train",
        result.session.smoke_step.observation.gradients_ok,
        std::string("loss=") + std::to_string(result.session.smoke_step.observation.step_loss) +
            " grad_mean=" + std::to_string(result.session.smoke_step.grad_mean)
    });
    timeline.stages.push_back({
        "yolo_eval",
        result.session.execution.bundle.round_report.eval_run.run.all_passed,
        std::string("loss=") +
            std::to_string(result.session.execution.bundle.round_report.eval_run.run.outcomes.front().metrics.front().value) +
            " classes=" + std::to_string(result.session.execution.bundle.round_report.eval_run.per_class.size())
    });
    timeline.stages.push_back({
        "yolo_report",
        result.passed,
        result.summary
    });
    timeline.validate();
    return timeline;
}

struct YoloTrainerLifecycleSummary {
    int passed_stages = 0;
    int total_stages = 0;
    std::string final_stage_name;
    bool all_passed = false;
    std::string summary;

    void validate() const {
        TORCH_CHECK(total_stages > 0, "YOLO trainer lifecycle summary total_stages must be positive");
        TORCH_CHECK(passed_stages >= 0 && passed_stages <= total_stages,
            "YOLO trainer lifecycle summary passed_stages must be within range");
        TORCH_CHECK(!final_stage_name.empty(), "YOLO trainer lifecycle summary final_stage_name cannot be empty");
        TORCH_CHECK(!summary.empty(), "YOLO trainer lifecycle summary summary cannot be empty");
        TORCH_CHECK(all_passed == (passed_stages == total_stages),
            "YOLO trainer lifecycle summary all_passed must match stage counts");
    }
};

struct YoloTrainerAnalysis {
    YoloTrainerTimeline timeline;
    std::vector<TuningComparisonRow> comparison_rows;
    TuningRecommendation recommendation;
    YoloTrainerLifecycleSummary lifecycle_summary;
    TuningRunReport flat_run;

    void validate() const {
        timeline.validate();
        TORCH_CHECK(!comparison_rows.empty(), "YOLO trainer analysis must contain comparison rows");
        for (const auto& row : comparison_rows) {
            row.validate();
        }
        recommendation.validate();
        lifecycle_summary.validate();
        flat_run.validate();
        TORCH_CHECK(recommendation.track_name == "yolo_mainline",
            "YOLO trainer analysis recommendation track must be yolo_mainline");
        TORCH_CHECK(lifecycle_summary.total_stages == static_cast<int>(timeline.stages.size()),
            "YOLO trainer analysis lifecycle summary must align with timeline stage count");
        TORCH_CHECK(flat_run.track_name == "yolo_mainline",
            "YOLO trainer analysis flat run must target yolo_mainline");
    }
};

inline YoloTrainerLifecycleSummary build_yolo_trainer_lifecycle_summary(const YoloTrainerAnalysis& analysis) {
    analysis.timeline.validate();
    TORCH_CHECK(!analysis.comparison_rows.empty(),
        "YOLO trainer lifecycle summary requires trainer comparison rows");
    for (const auto& row : analysis.comparison_rows) {
        row.validate();
    }
    analysis.recommendation.validate();
    TORCH_CHECK(analysis.recommendation.track_name == "yolo_mainline",
        "YOLO trainer lifecycle summary requires yolo_mainline recommendation");

    YoloTrainerLifecycleSummary summary;
    summary.total_stages = static_cast<int>(analysis.timeline.stages.size());
    for (const auto& stage : analysis.timeline.stages) {
        if (stage.passed) {
            summary.passed_stages += 1;
        }
    }
    summary.final_stage_name = analysis.timeline.stages.back().stage_name;
    summary.all_passed = summary.passed_stages == summary.total_stages;
    summary.summary = "YOLO trainer lifecycle passed " +
        std::to_string(summary.passed_stages) + "/" +
        std::to_string(summary.total_stages) +
        " stages; final_stage=" + summary.final_stage_name;
    summary.validate();
    return summary;
}

inline TuningRunReport build_yolo_trainer_flat_run(
    const YoloTrainerSessionResult& result,
    const YoloTrainerLifecycleSummary& lifecycle_summary)
{
    result.validate();
    lifecycle_summary.validate();

    TuningRunReport report;
    report.run_name = "yolo_trainer_session_run";
    report.track_name = "yolo_mainline";
    report.outcomes.push_back(result.session.execution.bundle.round_report.smoke_run.outcomes.front());
    report.outcomes.push_back(result.session.execution.bundle.round_report.eval_run.run.outcomes.front());
    report.all_passed = result.passed && lifecycle_summary.all_passed;
    report.summary = "YOLO trainer flat run keeps smoke/eval outcomes aligned with lifecycle summary: " +
        lifecycle_summary.summary;
    report.validate();
    return report;
}

struct YoloUnifiedMainlineBundle {
    YoloMainlineBundle mainline;
    YoloTrainerAnalysis trainer;
    std::vector<TuningComparisonRow> comparison_rows;
    TuningRecommendation recommendation;
    TuningRunReport flat_run;

    void validate() const {
        mainline.validate();
        trainer.validate();
        TORCH_CHECK(!comparison_rows.empty(), "YOLO unified mainline bundle must contain comparison rows");
        for (const auto& row : comparison_rows) {
            row.validate();
        }
        recommendation.validate();
        flat_run.validate();
        TORCH_CHECK(recommendation.track_name == "yolo_mainline",
            "YOLO unified mainline recommendation track must be yolo_mainline");
        TORCH_CHECK(flat_run.track_name == "yolo_mainline",
            "YOLO unified mainline flat run must target yolo_mainline");
    }
};

struct YoloUnifiedMainlineSummary {
    int total_outcomes = 0;
    int total_comparisons = 0;
    int total_recommendations = 0;
    bool all_passed = false;
    std::string summary;

    void validate() const {
        TORCH_CHECK(total_outcomes > 0, "YOLO unified mainline summary total_outcomes must be positive");
        TORCH_CHECK(total_comparisons > 0, "YOLO unified mainline summary total_comparisons must be positive");
        TORCH_CHECK(total_recommendations > 0, "YOLO unified mainline summary total_recommendations must be positive");
        TORCH_CHECK(!summary.empty(), "YOLO unified mainline summary text cannot be empty");
    }
};

inline YoloUnifiedMainlineBundle build_yolo_unified_mainline_bundle(
    const YoloMainlineBundle& mainline,
    const YoloTrainerAnalysis& trainer)
{
    mainline.validate();
    trainer.validate();

    YoloUnifiedMainlineBundle bundle;
    bundle.mainline = mainline;
    bundle.trainer = trainer;
    bundle.comparison_rows = mainline.comparison_rows;
    bundle.comparison_rows.insert(
        bundle.comparison_rows.end(),
        trainer.comparison_rows.begin(),
        trainer.comparison_rows.end());

    bundle.recommendation.track_name = "yolo_mainline";
    bundle.recommendation.selected_experiments = mainline.recommendation.selected_experiments;
    bundle.recommendation.selected_experiments.insert(
        bundle.recommendation.selected_experiments.end(),
        trainer.recommendation.selected_experiments.begin(),
        trainer.recommendation.selected_experiments.end());
    bundle.recommendation.summary =
        "Unified YOLO mainline recommendation keeps both mainline smoke/eval gates and trainer smoke/eval gates.";

    bundle.flat_run.run_name = "yolo_unified_mainline_run";
    bundle.flat_run.track_name = "yolo_mainline";
    bundle.flat_run.outcomes = mainline.flat_run.outcomes;
    bundle.flat_run.outcomes.insert(
        bundle.flat_run.outcomes.end(),
        trainer.flat_run.outcomes.begin(),
        trainer.flat_run.outcomes.end());
    bundle.flat_run.all_passed = mainline.flat_run.all_passed && trainer.flat_run.all_passed;
    bundle.flat_run.summary =
        "Unified YOLO mainline run aggregates mainline round and trainer session outcomes.";

    bundle.validate();
    return bundle;
}

inline YoloUnifiedMainlineSummary build_yolo_unified_mainline_summary(
    const YoloUnifiedMainlineBundle& bundle)
{
    bundle.validate();

    YoloUnifiedMainlineSummary summary;
    summary.total_outcomes = static_cast<int>(bundle.flat_run.outcomes.size());
    summary.total_comparisons = static_cast<int>(bundle.comparison_rows.size());
    summary.total_recommendations = static_cast<int>(bundle.recommendation.selected_experiments.size());
    summary.all_passed = bundle.mainline.flat_run.all_passed && bundle.trainer.flat_run.all_passed;
    summary.summary =
        "YOLO unified mainline bundle aggregates " +
        std::to_string(summary.total_outcomes) + " outcomes, " +
        std::to_string(summary.total_comparisons) + " comparisons, and " +
        std::to_string(summary.total_recommendations) +
        " recommended selections across mainline and trainer gates.";
    summary.validate();
    return summary;
}

inline YoloTrainerAnalysis build_yolo_trainer_analysis(const YoloTrainerSessionResult& result) {
    result.validate();

    YoloTrainerAnalysis analysis;
    analysis.timeline = build_yolo_trainer_timeline(result);

    const auto& smoke_stage = result.session.execution.bundle.round_report.smoke_run.outcomes.front();
    const auto& eval_stage = result.session.execution.bundle.round_report.eval_run.run.outcomes.front();

    analysis.comparison_rows.push_back({
        "yolo_trainer_smoke_stage",
        "yolo_trainer_smoke_current",
        "Use the current trainer smoke stage as the active optimizer/device gate.",
        {build_experiment_result_from_outcome("yolo_trainer_smoke_current", smoke_stage)}
    });
    analysis.comparison_rows.push_back({
        "yolo_trainer_eval_stage",
        "yolo_trainer_eval_current",
        "Use the current trainer eval stage as the active dataset-backed trainer validation gate.",
        {build_experiment_result_from_outcome("yolo_trainer_eval_current", eval_stage)}
    });

    analysis.recommendation.track_name = "yolo_mainline";
    analysis.recommendation.selected_experiments = {
        {"yolo_trainer_smoke_stage", "yolo_trainer_smoke_current"},
        {"yolo_trainer_eval_stage", "yolo_trainer_eval_current"}
    };
    analysis.recommendation.summary =
        "Current YOLO trainer recommendation keeps the latest trainer smoke and trainer eval stages.";
    analysis.lifecycle_summary = build_yolo_trainer_lifecycle_summary(analysis);
    analysis.flat_run = build_yolo_trainer_flat_run(result, analysis.lifecycle_summary);
    analysis.validate();
    return analysis;
}

inline YoloMainlineSessionResult run_yolo_mainline_session(
    YOLOv8& model,
    torch::Tensor imgs,
    torch::Tensor targets,
    const std::string& eval_data_path,
    const YoloMainlineRunnerConfig& config)
{
    config.validate();
    TORCH_CHECK(config.enable_smoke_train, "YOLO mainline session requires smoke train to be enabled");
    TORCH_CHECK(config.enable_eval, "YOLO mainline session requires eval to be enabled");

    auto smoke_step = run_yolo_smoke_train_step(model, imgs, targets, config);
    YOLOv8Impl::ValidationSummary eval_summary;
    if (config.reuse_trained_model_for_eval) {
        eval_summary = run_yolo_eval_summary(model, eval_data_path, config);
    } else {
        YOLOv8 eval_model(config.model, config.build);
        auto eval_device = resolve_yolo_mainline_device(config.device_policy);
        eval_model->to(eval_device);
        eval_summary = run_yolo_eval_summary(eval_model, eval_data_path, config);
    }

    YoloMainlineSessionResult session;
    session.smoke_step = smoke_step;
    session.execution = build_yolo_mainline_execution_result(config, smoke_step.observation, eval_summary);
    return session;
}

inline YoloTrainerSessionResult run_yolo_trainer_session(
    YOLOv8& model,
    torch::Tensor imgs,
    torch::Tensor targets,
    const std::string& eval_data_path,
    const YoloMainlineRunnerConfig& config)
{
    config.validate();
    auto session = run_yolo_mainline_session(model, imgs, targets, eval_data_path, config);

    YoloTrainerSessionResult result;
    result.config = config;
    result.session = session;
    result.passed = session.execution.bundle.round_report.round_passed;
    result.summary = result.passed
        ? "YOLO trainer session completed smoke training and evaluation gates."
        : "YOLO trainer session failed one or more smoke/eval gates.";
    return result;
}

#endif
