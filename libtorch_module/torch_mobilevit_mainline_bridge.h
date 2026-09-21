#ifndef TORCH_MOBILEVIT_MAINLINE_BRIDGE_H
#define TORCH_MOBILEVIT_MAINLINE_BRIDGE_H

#include <cmath>
#include <string>
#include <vector>

#include "torch_mobilevitv2.h"
#include "torch_tuning_profiles.h"

enum class MobileViTDevicePolicy {
    PreferCUDA,
    ForceCPU,
    ForceCUDA
};

inline const char* mobilevit_device_policy_name(MobileViTDevicePolicy policy) {
    switch (policy) {
    case MobileViTDevicePolicy::PreferCUDA:
        return "prefer-cuda";
    case MobileViTDevicePolicy::ForceCPU:
        return "force-cpu";
    case MobileViTDevicePolicy::ForceCUDA:
        return "force-cuda";
    default:
        return "unknown";
    }
}

inline torch::Device resolve_mobilevit_device(MobileViTDevicePolicy policy) {
    switch (policy) {
    case MobileViTDevicePolicy::ForceCPU:
        return torch::Device(torch::kCPU);
    case MobileViTDevicePolicy::ForceCUDA:
        TORCH_CHECK(torch::cuda::is_available(),
            "MobileViT runner requested CUDA, but CUDA is not available");
        return torch::Device(torch::kCUDA);
    case MobileViTDevicePolicy::PreferCUDA:
    default:
        return torch::cuda::is_available() ? torch::Device(torch::kCUDA) : torch::Device(torch::kCPU);
    }
}

struct MobileViTOptimizerConfig {
    float lr = 1e-4f;
    float weight_decay = 0.0f;

    void validate() const {
        TORCH_CHECK(lr > 0.0f, "MobileViT optimizer lr must be positive");
        TORCH_CHECK(weight_decay >= 0.0f, "MobileViT optimizer weight_decay must be non-negative");
    }
};

struct MobileViTRuntimeConfig {
    int max_train_batches = 1;
    int log_interval = 1;

    void validate() const {
        TORCH_CHECK(max_train_batches > 0, "MobileViT runtime max_train_batches must be positive");
        TORCH_CHECK(log_interval > 0, "MobileViT runtime log_interval must be positive");
    }
};

struct MobileViTMainlineRunnerConfig {
    std::string run_name = "mobilevit_mainline";
    int num_classes = 10;
    int input_size = 256;
    int batch_size = 4;
    MobileViTOptimizerConfig optimizer;
    MobileViTRuntimeConfig runtime;
    MobileViTDevicePolicy device_policy = MobileViTDevicePolicy::PreferCUDA;
    bool enable_smoke_train = true;
    bool enable_eval = true;

    void validate() const {
        TORCH_CHECK(!run_name.empty(), "MobileViT runner run_name cannot be empty");
        TORCH_CHECK(num_classes > 0, "MobileViT runner num_classes must be positive");
        TORCH_CHECK(input_size > 0, "MobileViT runner input_size must be positive");
        TORCH_CHECK(batch_size > 0, "MobileViT runner batch_size must be positive");
        optimizer.validate();
        runtime.validate();
        TORCH_CHECK(enable_smoke_train || enable_eval,
            "MobileViT runner must enable at least one stage");
    }
};

inline MobileViTMainlineRunnerConfig make_mobilevit_mainline_runner_config(
    int num_classes = 10,
    int input_size = 256,
    int batch_size = 4)
{
    MobileViTMainlineRunnerConfig config;
    config.num_classes = num_classes;
    config.input_size = input_size;
    config.batch_size = batch_size;
    return config;
}

struct MobileViTSmokeStepResult {
    double loss = 0.0;
    double grad_mean = 0.0;
    int batch_size = 0;
    int input_size = 0;

    void validate() const {
        TORCH_CHECK(std::isfinite(loss), "MobileViT smoke loss must be finite");
        TORCH_CHECK(std::isfinite(grad_mean), "MobileViT smoke grad_mean must be finite");
        TORCH_CHECK(batch_size > 0, "MobileViT smoke batch_size must be positive");
        TORCH_CHECK(input_size > 0, "MobileViT smoke input_size must be positive");
    }
};

struct MobileViTEvalSummary {
    double loss = 0.0;
    double top1 = 0.0;
    double avg_confidence = 0.0;
    int sample_count = 0;

    void validate() const {
        TORCH_CHECK(std::isfinite(loss), "MobileViT eval loss must be finite");
        TORCH_CHECK(std::isfinite(top1), "MobileViT eval top1 must be finite");
        TORCH_CHECK(std::isfinite(avg_confidence), "MobileViT eval avg_confidence must be finite");
        TORCH_CHECK(top1 >= 0.0 && top1 <= 1.0, "MobileViT eval top1 must be in [0, 1]");
        TORCH_CHECK(avg_confidence >= 0.0 && avg_confidence <= 1.0,
            "MobileViT eval avg_confidence must be in [0, 1]");
        TORCH_CHECK(sample_count > 0, "MobileViT eval sample_count must be positive");
    }
};

inline MobileViTSmokeStepResult run_mobilevit_smoke_train_step(
    MobileViTv2& model,
    torch::Tensor imgs,
    torch::Tensor targets,
    const MobileViTMainlineRunnerConfig& config)
{
    config.validate();
    TORCH_CHECK(config.enable_smoke_train, "MobileViT smoke train requires enable_smoke_train");
    TORCH_CHECK(imgs.dim() == 4, "MobileViT smoke imgs must be BCHW");
    TORCH_CHECK(targets.dim() == 1, "MobileViT smoke targets must be 1D");

    auto device = resolve_mobilevit_device(config.device_policy);
    imgs = imgs.to(device);
    targets = targets.to(device, torch::kLong);
    model->to(device);
    model->train();

    torch::optim::Adam optimizer(
        model->parameters(),
        torch::optim::AdamOptions(config.optimizer.lr).weight_decay(config.optimizer.weight_decay));
    torch::nn::CrossEntropyLoss loss_fn;

    optimizer.zero_grad();
    auto logits = model->forward(imgs);
    auto loss = loss_fn(logits, targets);
    TORCH_CHECK(torch::isfinite(loss).item<bool>(), "MobileViT smoke loss must be finite");
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
    TORCH_CHECK(has_grad, "MobileViT smoke train must produce gradients");
    optimizer.step();

    MobileViTSmokeStepResult result;
    result.loss = loss.item<double>();
    result.grad_mean = grad_mean;
    result.batch_size = static_cast<int>(imgs.size(0));
    result.input_size = static_cast<int>(imgs.size(2));
    return result;
}

inline MobileViTEvalSummary run_mobilevit_eval_summary(
    MobileViTv2& model,
    torch::Tensor imgs,
    torch::Tensor targets,
    const MobileViTMainlineRunnerConfig& config)
{
    config.validate();
    TORCH_CHECK(config.enable_eval, "MobileViT eval requires enable_eval");
    TORCH_CHECK(imgs.dim() == 4, "MobileViT eval imgs must be BCHW");
    TORCH_CHECK(targets.dim() == 1, "MobileViT eval targets must be 1D");

    auto device = resolve_mobilevit_device(config.device_policy);
    imgs = imgs.to(device);
    targets = targets.to(device, torch::kLong);
    model->to(device);
    model->eval();

    torch::NoGradGuard no_grad;
    torch::nn::CrossEntropyLoss loss_fn;
    auto logits = model->forward(imgs);
    auto loss = loss_fn(logits, targets);
    auto probs = torch::softmax(logits, 1);
    auto pred = probs.argmax(1);
    auto top1 = pred.eq(targets).to(torch::kFloat32).mean().item<double>();
    auto max_result = probs.max(1);
    auto avg_confidence = std::get<0>(max_result).mean().item<double>();

    MobileViTEvalSummary summary;
    summary.loss = loss.item<double>();
    summary.top1 = top1;
    summary.avg_confidence = avg_confidence;
    summary.sample_count = static_cast<int>(imgs.size(0));
    return summary;
}

inline TuningRunReport build_mobilevit_smoke_run_report(const MobileViTSmokeStepResult& smoke) {
    smoke.validate();

    TuningRunReport report;
    report.run_name = "mobilevit_smoke_run";
    report.track_name = "mobilevit_mainline";
    report.outcomes.push_back({
        "mobilevit_train_smoke_stage",
        {
            {"input_size", std::to_string(smoke.input_size)},
            {"batch_size", std::to_string(smoke.batch_size)}
        },
        {
            {TuningMetricKind::Loss, "loss", smoke.loss},
            {TuningMetricKind::MemoryMb, "grad_mean", smoke.grad_mean}
        },
        true,
        "MobileViT smoke training run kept finite loss and gradients."
    });
    report.all_passed = true;
    report.summary = "MobileViT smoke run passed.";
    report.validate();
    return report;
}

inline TuningRunReport build_mobilevit_eval_run_report(const MobileViTEvalSummary& eval) {
    eval.validate();

    TuningRunReport report;
    report.run_name = "mobilevit_eval_run";
    report.track_name = "mobilevit_mainline";
    report.outcomes.push_back({
        "mobilevit_eval_stage",
        {
            {"sample_count", std::to_string(eval.sample_count)}
        },
        {
            {TuningMetricKind::Loss, "loss", eval.loss},
            {TuningMetricKind::Top1, "top1", eval.top1},
            {TuningMetricKind::LatencyMs, "avg_confidence", eval.avg_confidence}
        },
        true,
        "MobileViT eval run produced finite loss and top1 statistics."
    });
    report.all_passed = true;
    report.summary = "MobileViT eval run passed.";
    report.validate();
    return report;
}

struct MobileViTMainlineSessionResult {
    MobileViTMainlineRunnerConfig config;
    MobileViTSmokeStepResult smoke;
    MobileViTEvalSummary eval;
    TuningRunReport flat_run;
    bool passed = false;
    std::string summary;

    void validate() const {
        config.validate();
        smoke.validate();
        eval.validate();
        flat_run.validate();
        TORCH_CHECK(flat_run.track_name == "mobilevit_mainline",
            "MobileViT session flat run must target mobilevit_mainline");
        TORCH_CHECK(!summary.empty(), "MobileViT session summary cannot be empty");
    }
};

inline MobileViTMainlineSessionResult run_mobilevit_mainline_session(
    MobileViTv2& model,
    torch::Tensor train_imgs,
    torch::Tensor train_targets,
    torch::Tensor eval_imgs,
    torch::Tensor eval_targets,
    const MobileViTMainlineRunnerConfig& config)
{
    config.validate();
    auto smoke = run_mobilevit_smoke_train_step(model, train_imgs, train_targets, config);
    auto eval = run_mobilevit_eval_summary(model, eval_imgs, eval_targets, config);

    MobileViTMainlineSessionResult result;
    result.config = config;
    result.smoke = smoke;
    result.eval = eval;
    result.flat_run.run_name = "mobilevit_mainline_session_run";
    result.flat_run.track_name = "mobilevit_mainline";
    result.flat_run.outcomes = build_mobilevit_smoke_run_report(smoke).outcomes;
    auto eval_run = build_mobilevit_eval_run_report(eval);
    result.flat_run.outcomes.insert(
        result.flat_run.outcomes.end(),
        eval_run.outcomes.begin(),
        eval_run.outcomes.end());
    result.flat_run.all_passed = true;
    result.flat_run.summary = "MobileViT mainline session aggregates smoke and eval outcomes.";
    result.passed = true;
    result.summary = "MobileViT mainline session completed smoke training and evaluation gates.";
    result.validate();
    return result;
}

struct MobileViTTrainerSessionResult {
    MobileViTMainlineRunnerConfig config;
    MobileViTMainlineSessionResult session;
    bool passed = false;
    std::string summary;

    void validate() const {
        config.validate();
        session.validate();
        TORCH_CHECK(!summary.empty(), "MobileViT trainer session summary cannot be empty");
        TORCH_CHECK(config.run_name == session.config.run_name,
            "MobileViT trainer session run_name must match mainline session");
    }
};

struct MobileViTTrainerStageStatus {
    std::string stage_name;
    bool passed = false;
    std::string detail;

    void validate() const {
        TORCH_CHECK(!stage_name.empty(), "MobileViT trainer stage_name cannot be empty");
        TORCH_CHECK(!detail.empty(), "MobileViT trainer stage detail cannot be empty");
    }
};

struct MobileViTTrainerTimeline {
    std::vector<MobileViTTrainerStageStatus> stages;

    void validate() const {
        TORCH_CHECK(!stages.empty(), "MobileViT trainer timeline must contain at least one stage");
        for (const auto& stage : stages) {
            stage.validate();
        }
    }
};

struct MobileViTTrainerLifecycleSummary {
    int passed_stages = 0;
    int total_stages = 0;
    std::string final_stage_name;
    bool all_passed = false;
    std::string summary;

    void validate() const {
        TORCH_CHECK(total_stages > 0, "MobileViT trainer lifecycle total_stages must be positive");
        TORCH_CHECK(passed_stages >= 0 && passed_stages <= total_stages,
            "MobileViT trainer lifecycle passed_stages must be within range");
        TORCH_CHECK(!final_stage_name.empty(), "MobileViT trainer lifecycle final_stage_name cannot be empty");
        TORCH_CHECK(!summary.empty(), "MobileViT trainer lifecycle summary cannot be empty");
        TORCH_CHECK(all_passed == (passed_stages == total_stages),
            "MobileViT trainer lifecycle all_passed must match stage counts");
    }
};

struct MobileViTTrainerAnalysis {
    MobileViTTrainerTimeline timeline;
    std::vector<TuningComparisonRow> comparison_rows;
    TuningRecommendation recommendation;
    MobileViTTrainerLifecycleSummary lifecycle_summary;
    TuningRunReport flat_run;

    void validate() const {
        timeline.validate();
        TORCH_CHECK(!comparison_rows.empty(), "MobileViT trainer analysis must contain comparison rows");
        for (const auto& row : comparison_rows) {
            row.validate();
        }
        recommendation.validate();
        lifecycle_summary.validate();
        flat_run.validate();
        TORCH_CHECK(recommendation.track_name == "mobilevit_mainline",
            "MobileViT trainer analysis recommendation track must be mobilevit_mainline");
        TORCH_CHECK(lifecycle_summary.total_stages == static_cast<int>(timeline.stages.size()),
            "MobileViT trainer lifecycle summary must align with timeline");
        TORCH_CHECK(flat_run.track_name == "mobilevit_mainline",
            "MobileViT trainer analysis flat run must target mobilevit_mainline");
    }
};

struct MobileViTUnifiedMainlineBundle {
    MobileViTMainlineSessionResult mainline;
    MobileViTTrainerAnalysis trainer;
    std::vector<TuningComparisonRow> comparison_rows;
    TuningRecommendation recommendation;
    TuningRunReport flat_run;

    void validate() const {
        mainline.validate();
        trainer.validate();
        TORCH_CHECK(!comparison_rows.empty(), "MobileViT unified mainline bundle must contain comparison rows");
        for (const auto& row : comparison_rows) {
            row.validate();
        }
        recommendation.validate();
        flat_run.validate();
        TORCH_CHECK(recommendation.track_name == "mobilevit_mainline",
            "MobileViT unified recommendation track must be mobilevit_mainline");
        TORCH_CHECK(flat_run.track_name == "mobilevit_mainline",
            "MobileViT unified flat run must target mobilevit_mainline");
    }
};

struct MobileViTUnifiedMainlineSummary {
    int total_outcomes = 0;
    int total_comparisons = 0;
    int total_recommendations = 0;
    bool all_passed = false;
    std::string summary;

    void validate() const {
        TORCH_CHECK(total_outcomes > 0, "MobileViT unified summary total_outcomes must be positive");
        TORCH_CHECK(total_comparisons > 0, "MobileViT unified summary total_comparisons must be positive");
        TORCH_CHECK(total_recommendations > 0,
            "MobileViT unified summary total_recommendations must be positive");
        TORCH_CHECK(!summary.empty(), "MobileViT unified summary text cannot be empty");
    }
};

inline MobileViTTrainerSessionResult run_mobilevit_trainer_session(
    MobileViTv2& model,
    torch::Tensor train_imgs,
    torch::Tensor train_targets,
    torch::Tensor eval_imgs,
    torch::Tensor eval_targets,
    const MobileViTMainlineRunnerConfig& config)
{
    config.validate();

    MobileViTTrainerSessionResult result;
    result.config = config;
    result.session = run_mobilevit_mainline_session(
        model, train_imgs, train_targets, eval_imgs, eval_targets, config);
    result.passed = result.session.passed;
    result.summary = "MobileViT trainer session completed smoke training and evaluation gates.";
    result.validate();
    return result;
}

inline MobileViTTrainerTimeline build_mobilevit_trainer_timeline(const MobileViTTrainerSessionResult& result) {
    result.validate();

    MobileViTTrainerTimeline timeline;
    timeline.stages.push_back({
        "mobilevit_logging",
        result.config.runtime.log_interval > 0,
        std::string("log_interval=") + std::to_string(result.config.runtime.log_interval)
    });
    timeline.stages.push_back({
        "mobilevit_smoke_train",
        result.session.smoke.grad_mean >= 0.0,
        std::string("loss=") + std::to_string(result.session.smoke.loss) +
            " grad_mean=" + std::to_string(result.session.smoke.grad_mean)
    });
    timeline.stages.push_back({
        "mobilevit_eval",
        result.session.eval.sample_count > 0,
        std::string("loss=") + std::to_string(result.session.eval.loss) +
            " top1=" + std::to_string(result.session.eval.top1)
    });
    timeline.stages.push_back({
        "mobilevit_report",
        result.passed,
        result.summary
    });
    timeline.validate();
    return timeline;
}

inline std::vector<TuningComparisonRow> build_mobilevit_trainer_comparison_rows(
    const MobileViTTrainerSessionResult& result)
{
    result.validate();

    std::vector<TuningComparisonRow> rows;
    rows.push_back({
        "mobilevit_trainer_smoke_stage",
        "mobilevit_trainer_smoke",
        "Selected smoke stage because gradients and finite loss were observed.",
        {{
            "mobilevit_trainer_smoke",
            {
                {TuningMetricKind::Loss, "loss", result.session.smoke.loss},
                {TuningMetricKind::MemoryMb, "grad_mean", result.session.smoke.grad_mean}
            },
            true,
            "MobileViT trainer smoke result"
        }}
    });
    rows.push_back({
        "mobilevit_trainer_eval_stage",
        "mobilevit_trainer_eval",
        "Selected eval stage because finite classification metrics were produced.",
        {{
            "mobilevit_trainer_eval",
            {
                {TuningMetricKind::Loss, "loss", result.session.eval.loss},
                {TuningMetricKind::Top1, "top1", result.session.eval.top1},
                {TuningMetricKind::LatencyMs, "avg_confidence", result.session.eval.avg_confidence}
            },
            true,
            "MobileViT trainer eval result"
        }}
    });
    for (const auto& row : rows) {
        row.validate();
    }
    return rows;
}

inline TuningRecommendation build_mobilevit_trainer_recommendation(
    const MobileViTTrainerSessionResult& result)
{
    result.validate();

    TuningRecommendation recommendation;
    recommendation.track_name = "mobilevit_mainline";
    recommendation.selected_experiments = {
        {"mobilevit_trainer_smoke_stage", "mobilevit_trainer_smoke"},
        {"mobilevit_trainer_eval_stage", "mobilevit_trainer_eval"}
    };
    recommendation.summary =
        "MobileViT trainer recommendation keeps smoke training and eval gates as the current minimum path.";
    recommendation.validate();
    return recommendation;
}

inline MobileViTTrainerLifecycleSummary build_mobilevit_trainer_lifecycle_summary(
    const MobileViTTrainerTimeline& timeline)
{
    timeline.validate();

    MobileViTTrainerLifecycleSummary summary;
    summary.total_stages = static_cast<int>(timeline.stages.size());
    for (const auto& stage : timeline.stages) {
        if (stage.passed) {
            summary.passed_stages += 1;
        }
    }
    summary.final_stage_name = timeline.stages.back().stage_name;
    summary.all_passed = summary.passed_stages == summary.total_stages;
    summary.summary = "MobileViT trainer lifecycle passed " +
        std::to_string(summary.passed_stages) + "/" +
        std::to_string(summary.total_stages) +
        " stages; final_stage=" + summary.final_stage_name;
    summary.validate();
    return summary;
}

inline TuningRunReport build_mobilevit_trainer_flat_run(const MobileViTTrainerSessionResult& result) {
    result.validate();

    TuningRunReport report;
    report.run_name = "mobilevit_trainer_session_run";
    report.track_name = "mobilevit_mainline";
    report.outcomes = result.session.flat_run.outcomes;
    report.all_passed = result.passed;
    report.summary = "MobileViT trainer flat run aggregates smoke and eval outcomes.";
    report.validate();
    return report;
}

inline MobileViTTrainerAnalysis build_mobilevit_trainer_analysis(const MobileViTTrainerSessionResult& result) {
    result.validate();

    MobileViTTrainerAnalysis analysis;
    analysis.timeline = build_mobilevit_trainer_timeline(result);
    analysis.comparison_rows = build_mobilevit_trainer_comparison_rows(result);
    analysis.recommendation = build_mobilevit_trainer_recommendation(result);
    analysis.lifecycle_summary = build_mobilevit_trainer_lifecycle_summary(analysis.timeline);
    analysis.flat_run = build_mobilevit_trainer_flat_run(result);
    analysis.validate();
    return analysis;
}

inline MobileViTUnifiedMainlineBundle build_mobilevit_unified_mainline_bundle(
    const MobileViTMainlineSessionResult& mainline,
    const MobileViTTrainerAnalysis& trainer)
{
    mainline.validate();
    trainer.validate();

    MobileViTUnifiedMainlineBundle bundle;
    bundle.mainline = mainline;
    bundle.trainer = trainer;
    bundle.comparison_rows = trainer.comparison_rows;

    bundle.recommendation.track_name = "mobilevit_mainline";
    bundle.recommendation.selected_experiments = {
        {"mobilevit_mainline_smoke_stage", "mobilevit_mainline_smoke_current"},
        {"mobilevit_mainline_eval_stage", "mobilevit_mainline_eval_current"}
    };
    bundle.recommendation.selected_experiments.insert(
        bundle.recommendation.selected_experiments.end(),
        trainer.recommendation.selected_experiments.begin(),
        trainer.recommendation.selected_experiments.end());
    bundle.recommendation.summary =
        "Unified MobileViT mainline recommendation keeps both mainline smoke/eval gates and trainer smoke/eval gates.";

    bundle.flat_run.run_name = "mobilevit_unified_mainline_run";
    bundle.flat_run.track_name = "mobilevit_mainline";
    bundle.flat_run.outcomes = mainline.flat_run.outcomes;
    bundle.flat_run.outcomes.insert(
        bundle.flat_run.outcomes.end(),
        trainer.flat_run.outcomes.begin(),
        trainer.flat_run.outcomes.end());
    bundle.flat_run.all_passed = mainline.flat_run.all_passed && trainer.flat_run.all_passed;
    bundle.flat_run.summary =
        "Unified MobileViT mainline run aggregates mainline session and trainer session outcomes.";

    bundle.validate();
    return bundle;
}

inline MobileViTUnifiedMainlineSummary build_mobilevit_unified_mainline_summary(
    const MobileViTUnifiedMainlineBundle& bundle)
{
    bundle.validate();

    MobileViTUnifiedMainlineSummary summary;
    summary.total_outcomes = static_cast<int>(bundle.flat_run.outcomes.size());
    summary.total_comparisons = static_cast<int>(bundle.comparison_rows.size());
    summary.total_recommendations = static_cast<int>(bundle.recommendation.selected_experiments.size());
    summary.all_passed = bundle.mainline.flat_run.all_passed && bundle.trainer.flat_run.all_passed;
    summary.summary =
        "MobileViT unified mainline bundle aggregates " +
        std::to_string(summary.total_outcomes) + " outcomes, " +
        std::to_string(summary.total_comparisons) + " comparisons, and " +
        std::to_string(summary.total_recommendations) +
        " recommended selections across mainline and trainer gates.";
    summary.validate();
    return summary;
}

#endif
