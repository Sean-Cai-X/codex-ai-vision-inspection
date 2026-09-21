#ifndef TORCH_TUNING_PROFILES_H
#define TORCH_TUNING_PROFILES_H

#include <torch/torch.h>

#include <cmath>
#include <string>
#include <vector>

#include "torch_modelconfig.h"


struct OptimizerTuningConfig {
    int epochs = 100;
    int batch_size = 16;
    float lr = 0.001f;
    float weight_decay = 0.0005f;
    float momentum = 0.9f;
    int warmup_epochs = 3;

    void validate() const {
        TORCH_CHECK(epochs > 0, "epochs must be positive");
        TORCH_CHECK(batch_size > 0, "batch_size must be positive");
        TORCH_CHECK(lr > 0.0f, "learning rate must be positive");
        TORCH_CHECK(weight_decay >= 0.0f, "weight_decay must be non-negative");
        TORCH_CHECK(momentum >= 0.0f && momentum < 1.0f, "momentum must be in [0, 1)");
        TORCH_CHECK(warmup_epochs >= 0, "warmup_epochs must be non-negative");
    }
};

struct AugmentationTuningConfig {
    int input_size = 640;
    float hsv_h = 0.015f;
    float hsv_s = 0.7f;
    float hsv_v = 0.4f;
    float flip_prob = 0.5f;
    bool use_letterbox = false;

    void validate() const {
        TORCH_CHECK(input_size > 0, "input_size must be positive");
        TORCH_CHECK(hsv_h >= 0.0f, "hsv_h must be non-negative");
        TORCH_CHECK(hsv_s >= 0.0f, "hsv_s must be non-negative");
        TORCH_CHECK(hsv_v >= 0.0f, "hsv_v must be non-negative");
        TORCH_CHECK(flip_prob >= 0.0f && flip_prob <= 1.0f, "flip_prob must be in [0, 1]");
    }
};

struct YoloTuningProfile {
    std::string name = "yolov8_baseline";
    std::string model_type = "nano";
    ModelConfig model = ModelConfig::get_config("nano");
    OptimizerTuningConfig optimizer;
    AugmentationTuningConfig augmentation;
    int assigner_topk = 10;
    float assigner_alpha = 1.0f;
    float assigner_beta = 6.0f;
    float box_loss_weight = 1.0f;
    float cls_loss_weight = 1.0f;

    void validate() const {
        TORCH_CHECK(!name.empty(), "YOLO profile name cannot be empty");
        TORCH_CHECK(!model_type.empty(), "YOLO model_type cannot be empty");
        model.validate();
        optimizer.validate();
        augmentation.validate();
        TORCH_CHECK(assigner_topk > 0, "assigner_topk must be positive");
        TORCH_CHECK(assigner_alpha > 0.0f, "assigner_alpha must be positive");
        TORCH_CHECK(assigner_beta > 0.0f, "assigner_beta must be positive");
        TORCH_CHECK(box_loss_weight > 0.0f, "box_loss_weight must be positive");
        TORCH_CHECK(cls_loss_weight > 0.0f, "cls_loss_weight must be positive");
    }

    static YoloTuningProfile smoke() {
        YoloTuningProfile cfg;
        cfg.name = "yolov8_smoke";
        cfg.model_type = "nano";
        cfg.model = ModelConfig::get_config("nano", 3);
        cfg.optimizer.epochs = 1;
        cfg.optimizer.batch_size = 2;
        cfg.optimizer.lr = 0.001f;
        cfg.augmentation.input_size = 320;
        cfg.augmentation.flip_prob = 0.0f;
        return cfg;
    }

    static YoloTuningProfile baseline() {
        YoloTuningProfile cfg;
        cfg.name = "yolov8_baseline";
        cfg.model_type = "small";
        cfg.model = ModelConfig::get_config("small", 3);
        cfg.optimizer.epochs = 100;
        cfg.optimizer.batch_size = 16;
        cfg.optimizer.lr = 0.01f;
        cfg.optimizer.weight_decay = 0.0005f;
        cfg.optimizer.momentum = 0.937f;
        cfg.augmentation.input_size = 640;
        cfg.augmentation.flip_prob = 0.5f;
        return cfg;
    }
};

struct ClassificationTuningProfile {
    std::string name = "classifier_baseline";
    std::string backbone = "mobilevitv2";
    int input_size = 256;
    int num_classes = 1000;
    int embedding_dim = 640;
    int patch_size = 2;
    OptimizerTuningConfig optimizer;
    float dropout = 0.0f;

    void validate() const {
        TORCH_CHECK(!name.empty(), "classification profile name cannot be empty");
        TORCH_CHECK(!backbone.empty(), "classification backbone cannot be empty");
        TORCH_CHECK(input_size > 0, "classification input_size must be positive");
        TORCH_CHECK(num_classes > 0, "classification num_classes must be positive");
        TORCH_CHECK(embedding_dim > 0, "classification embedding_dim must be positive");
        TORCH_CHECK(patch_size > 0, "classification patch_size must be positive");
        TORCH_CHECK(dropout >= 0.0f && dropout < 1.0f, "classification dropout must be in [0, 1)");
        optimizer.validate();
    }

    static ClassificationTuningProfile mobilevitv2_baseline() {
        ClassificationTuningProfile cfg;
        cfg.name = "mobilevitv2_baseline";
        cfg.backbone = "mobilevitv2";
        cfg.input_size = 256;
        cfg.embedding_dim = 640;
        cfg.patch_size = 2;
        cfg.optimizer.epochs = 60;
        cfg.optimizer.batch_size = 32;
        cfg.optimizer.lr = 0.001f;
        return cfg;
    }

    static ClassificationTuningProfile resnet18_baseline() {
        ClassificationTuningProfile cfg;
        cfg.name = "resnet18_baseline";
        cfg.backbone = "resnet18";
        cfg.input_size = 224;
        cfg.embedding_dim = 512;
        cfg.optimizer.epochs = 60;
        cfg.optimizer.batch_size = 32;
        cfg.optimizer.lr = 0.01f;
        return cfg;
    }

    static ClassificationTuningProfile resnet50_baseline() {
        ClassificationTuningProfile cfg;
        cfg.name = "resnet50_baseline";
        cfg.backbone = "resnet50";
        cfg.input_size = 224;
        cfg.embedding_dim = 2048;
        cfg.optimizer.epochs = 90;
        cfg.optimizer.batch_size = 32;
        cfg.optimizer.lr = 0.01f;
        return cfg;
    }
};

struct SegmentationTuningProfile {
    std::string name = "deeplabv3plus_baseline";
    std::string backbone = "mobilenet_v3_large";
    std::string decoder = "deeplabv3plus";
    int input_size = 512;
    int output_stride = 16;
    int num_classes = 2;
    int low_level_channels = 48;
    int decoder_channels = 256;
    std::vector<int64_t> aspp_rates = {12, 24, 36};
    OptimizerTuningConfig optimizer;

    void validate() const {
        TORCH_CHECK(!name.empty(), "segmentation profile name cannot be empty");
        TORCH_CHECK(!backbone.empty(), "segmentation backbone cannot be empty");
        TORCH_CHECK(!decoder.empty(), "segmentation decoder cannot be empty");
        TORCH_CHECK(input_size > 0, "segmentation input_size must be positive");
        TORCH_CHECK(output_stride > 0, "segmentation output_stride must be positive");
        TORCH_CHECK(num_classes > 0, "segmentation num_classes must be positive");
        TORCH_CHECK(low_level_channels > 0, "segmentation low_level_channels must be positive");
        TORCH_CHECK(decoder_channels > 0, "segmentation decoder_channels must be positive");
        TORCH_CHECK(!aspp_rates.empty(), "segmentation aspp_rates cannot be empty");
        for (auto rate : aspp_rates) {
            TORCH_CHECK(rate > 0, "segmentation aspp rates must be positive");
        }
        optimizer.validate();
    }

    static SegmentationTuningProfile mobilenetv3_deeplabv3plus() {
        SegmentationTuningProfile cfg;
        cfg.name = "deeplabv3plus_mobilenetv3";
        cfg.backbone = "mobilenet_v3_large";
        cfg.decoder = "deeplabv3plus";
        cfg.input_size = 512;
        cfg.output_stride = 16;
        cfg.low_level_channels = 40;
        cfg.decoder_channels = 256;
        cfg.optimizer.epochs = 80;
        cfg.optimizer.batch_size = 8;
        cfg.optimizer.lr = 0.001f;
        return cfg;
    }

    static SegmentationTuningProfile resnet18_deeplabv3plus() {
        SegmentationTuningProfile cfg;
        cfg.name = "deeplabv3plus_resnet18";
        cfg.backbone = "resnet18";
        cfg.decoder = "deeplabv3plus";
        cfg.input_size = 512;
        cfg.output_stride = 16;
        cfg.low_level_channels = 128;
        cfg.decoder_channels = 256;
        cfg.optimizer.epochs = 80;
        cfg.optimizer.batch_size = 8;
        cfg.optimizer.lr = 0.001f;
        return cfg;
    }

    static SegmentationTuningProfile resnet50_deeplabv3() {
        SegmentationTuningProfile cfg;
        cfg.name = "deeplabv3_resnet50";
        cfg.backbone = "resnet50";
        cfg.decoder = "deeplabv3";
        cfg.input_size = 512;
        cfg.output_stride = 16;
        cfg.low_level_channels = 512;
        cfg.decoder_channels = 256;
        cfg.optimizer.epochs = 100;
        cfg.optimizer.batch_size = 8;
        cfg.optimizer.lr = 0.001f;
        return cfg;
    }
};

enum class TuningTrack {
    YoloMainline,
    MobileViTMainline,
    LightweightSegmentation,
    BackboneBaseline,
    SegmentationAnalysis
};

enum class TuningPhase {
    Smoke,
    Baseline,
    Stabilize,
    Optimize,
    Compare
};

enum class TuningMetricKind {
    Loss,
    Map50,
    Top1,
    IoU,
    LatencyMs,
    MemoryMb,
    ForegroundIoU
};

struct TuningReadiness {
    bool dataset_available = false;
    bool labels_available = false;
    bool external_weights_available = false;
};

struct TuningMetricTarget {
    TuningMetricKind kind = TuningMetricKind::Loss;
    std::string name;
    bool higher_is_better = false;
    double warning_threshold = 0.0;

    void validate() const {
        TORCH_CHECK(!name.empty(), "tuning metric name cannot be empty");
    }
};

struct TuningKnob {
    std::string name;
    std::string start_value;
    std::string search_space;

    void validate() const {
        TORCH_CHECK(!name.empty(), "tuning knob name cannot be empty");
        TORCH_CHECK(!start_value.empty(), "tuning knob start_value cannot be empty");
        TORCH_CHECK(!search_space.empty(), "tuning knob search_space cannot be empty");
    }
};

struct TuningStepSpec {
    std::string step_name;
    std::string objective;
    std::vector<TuningKnob> knobs;
    std::vector<TuningMetricTarget> metrics;

    void validate() const {
        TORCH_CHECK(!step_name.empty(), "tuning step name cannot be empty");
        TORCH_CHECK(!objective.empty(), "tuning step objective cannot be empty");
        TORCH_CHECK(!knobs.empty(), "tuning step must define at least one knob");
        TORCH_CHECK(!metrics.empty(), "tuning step must define at least one metric");
        for (const auto& knob : knobs) {
            knob.validate();
        }
        for (const auto& metric : metrics) {
            metric.validate();
        }
    }
};

struct TuningPlanEntry {
    std::string name;
    TuningTrack track = TuningTrack::YoloMainline;
    TuningPhase phase = TuningPhase::Smoke;
    int priority = 0;
    bool requires_dataset = false;
    bool requires_labels = false;
    bool requires_external_weights = false;
    std::string objective;

    void validate() const {
        TORCH_CHECK(!name.empty(), "tuning plan entry name cannot be empty");
        TORCH_CHECK(priority > 0, "tuning plan priority must be positive");
        TORCH_CHECK(!objective.empty(), "tuning plan objective cannot be empty");
    }
};

struct TuningExecutionItem {
    TuningPlanEntry plan;
    TuningStepSpec spec;

    void validate() const {
        plan.validate();
        spec.validate();
        TORCH_CHECK(!spec.step_name.empty(), "execution item step_name cannot be empty");
    }
};

struct TuningBatch {
    std::string name;
    std::string objective;
    std::vector<TuningExecutionItem> items;

    void validate() const {
        TORCH_CHECK(!name.empty(), "tuning batch name cannot be empty");
        TORCH_CHECK(!objective.empty(), "tuning batch objective cannot be empty");
        TORCH_CHECK(!items.empty(), "tuning batch must contain at least one execution item");
        for (const auto& item : items) {
            item.validate();
        }
    }
};

struct TuningSweepCase {
    std::string name;
    std::string knob_name;
    std::vector<std::string> candidate_values;

    void validate() const {
        TORCH_CHECK(!name.empty(), "tuning sweep case name cannot be empty");
        TORCH_CHECK(!knob_name.empty(), "tuning sweep knob_name cannot be empty");
        TORCH_CHECK(!candidate_values.empty(), "tuning sweep must contain candidate values");
    }
};

struct TuningBatchSweep {
    std::string batch_name;
    std::vector<TuningSweepCase> sweeps;

    void validate() const {
        TORCH_CHECK(!batch_name.empty(), "tuning batch sweep name cannot be empty");
        TORCH_CHECK(!sweeps.empty(), "tuning batch sweep must contain at least one sweep case");
        for (const auto& sweep : sweeps) {
            sweep.validate();
        }
    }
};

struct TuningSweepStage {
    std::string batch_name;
    std::string stage_name;
    std::string objective;
    std::vector<TuningSweepCase> sweeps;

    void validate() const {
        TORCH_CHECK(!batch_name.empty(), "tuning sweep stage batch_name cannot be empty");
        TORCH_CHECK(!stage_name.empty(), "tuning sweep stage name cannot be empty");
        TORCH_CHECK(!objective.empty(), "tuning sweep stage objective cannot be empty");
        TORCH_CHECK(!sweeps.empty(), "tuning sweep stage must contain at least one sweep case");
        for (const auto& sweep : sweeps) {
            sweep.validate();
        }
    }
};

struct TuningStageGate {
    std::string stage_name;
    std::vector<TuningMetricTarget> pass_metrics;
    std::vector<std::string> stop_conditions;

    void validate() const {
        TORCH_CHECK(!stage_name.empty(), "tuning stage gate stage_name cannot be empty");
        TORCH_CHECK(!pass_metrics.empty(), "tuning stage gate must define pass metrics");
        TORCH_CHECK(!stop_conditions.empty(), "tuning stage gate must define stop conditions");
        for (const auto& metric : pass_metrics) {
            metric.validate();
        }
        for (const auto& condition : stop_conditions) {
            TORCH_CHECK(!condition.empty(), "tuning stage gate stop condition cannot be empty");
        }
    }
};

struct TuningResultMetric {
    TuningMetricKind kind = TuningMetricKind::Loss;
    std::string name;
    double value = 0.0;

    void validate() const {
        TORCH_CHECK(!name.empty(), "tuning result metric name cannot be empty");
    }
};

struct TuningStageOutcome {
    std::string stage_name;
    std::vector<std::pair<std::string, std::string>> selected_knobs;
    std::vector<TuningResultMetric> metrics;
    bool passed = false;
    std::string note;

    void validate() const {
        TORCH_CHECK(!stage_name.empty(), "tuning stage outcome name cannot be empty");
        TORCH_CHECK(!selected_knobs.empty(), "tuning stage outcome must record selected knobs");
        TORCH_CHECK(!metrics.empty(), "tuning stage outcome must record metrics");
        TORCH_CHECK(!note.empty(), "tuning stage outcome note cannot be empty");
        for (const auto& knob : selected_knobs) {
            TORCH_CHECK(!knob.first.empty(), "selected knob name cannot be empty");
            TORCH_CHECK(!knob.second.empty(), "selected knob value cannot be empty");
        }
        for (const auto& metric : metrics) {
            metric.validate();
        }
    }
};

struct TuningRunReport {
    std::string run_name;
    std::string track_name;
    std::vector<TuningStageOutcome> outcomes;
    bool all_passed = false;
    std::string summary;

    void validate() const {
        TORCH_CHECK(!run_name.empty(), "tuning run report name cannot be empty");
        TORCH_CHECK(!track_name.empty(), "tuning run report track_name cannot be empty");
        TORCH_CHECK(!outcomes.empty(), "tuning run report must contain at least one stage outcome");
        TORCH_CHECK(!summary.empty(), "tuning run report summary cannot be empty");
        for (const auto& outcome : outcomes) {
            outcome.validate();
        }
    }
};

struct TuningChecklistItem {
    std::string track_name;
    std::string stage_name;
    std::string action;
    std::string expected_artifact;
    bool requires_dataset = false;

    void validate() const {
        TORCH_CHECK(!track_name.empty(), "tuning checklist track_name cannot be empty");
        TORCH_CHECK(!stage_name.empty(), "tuning checklist stage_name cannot be empty");
        TORCH_CHECK(!action.empty(), "tuning checklist action cannot be empty");
        TORCH_CHECK(!expected_artifact.empty(), "tuning checklist expected_artifact cannot be empty");
    }
};

struct TuningExperimentRow {
    std::string track_name;
    std::string stage_name;
    std::string experiment_id;
    std::vector<std::pair<std::string, std::string>> settings;
    std::vector<std::string> target_metrics;

    void validate() const {
        TORCH_CHECK(!track_name.empty(), "tuning experiment track_name cannot be empty");
        TORCH_CHECK(!stage_name.empty(), "tuning experiment stage_name cannot be empty");
        TORCH_CHECK(!experiment_id.empty(), "tuning experiment id cannot be empty");
        TORCH_CHECK(!settings.empty(), "tuning experiment must contain settings");
        TORCH_CHECK(!target_metrics.empty(), "tuning experiment must contain target metrics");
        for (const auto& setting : settings) {
            TORCH_CHECK(!setting.first.empty(), "tuning experiment setting name cannot be empty");
            TORCH_CHECK(!setting.second.empty(), "tuning experiment setting value cannot be empty");
        }
        for (const auto& metric : target_metrics) {
            TORCH_CHECK(!metric.empty(), "tuning experiment target metric cannot be empty");
        }
    }
};

struct TuningExperimentResult {
    std::string experiment_id;
    std::vector<TuningResultMetric> metrics;
    bool passed = false;
    std::string note;

    void validate() const {
        TORCH_CHECK(!experiment_id.empty(), "tuning experiment result id cannot be empty");
        TORCH_CHECK(!metrics.empty(), "tuning experiment result must contain metrics");
        TORCH_CHECK(!note.empty(), "tuning experiment result note cannot be empty");
        for (const auto& metric : metrics) {
            metric.validate();
        }
    }
};

struct TuningComparisonRow {
    std::string stage_name;
    std::string best_experiment_id;
    std::string selection_reason;
    std::vector<TuningExperimentResult> compared_results;

    void validate() const {
        TORCH_CHECK(!stage_name.empty(), "tuning comparison stage_name cannot be empty");
        TORCH_CHECK(!best_experiment_id.empty(), "tuning comparison best_experiment_id cannot be empty");
        TORCH_CHECK(!selection_reason.empty(), "tuning comparison selection_reason cannot be empty");
        TORCH_CHECK(!compared_results.empty(), "tuning comparison must contain compared results");
        for (const auto& result : compared_results) {
            result.validate();
        }
    }
};

struct TuningRecommendation {
    std::string track_name;
    std::vector<std::pair<std::string, std::string>> selected_experiments;
    std::string summary;

    void validate() const {
        TORCH_CHECK(!track_name.empty(), "tuning recommendation track_name cannot be empty");
        TORCH_CHECK(!selected_experiments.empty(), "tuning recommendation must contain selected experiments");
        TORCH_CHECK(!summary.empty(), "tuning recommendation summary cannot be empty");
        for (const auto& item : selected_experiments) {
            TORCH_CHECK(!item.first.empty(), "tuning recommendation stage cannot be empty");
            TORCH_CHECK(!item.second.empty(), "tuning recommendation experiment id cannot be empty");
        }
    }
};

struct TuningClassSummary {
    int64_t class_id = -1;
    int64_t predicted_boxes = 0;
    int64_t target_boxes = 0;
    int64_t true_positives = 0;
    int64_t false_positives = 0;
    int64_t false_negatives = 0;
    double precision = 0.0;
    double recall = 0.0;
    double f1 = 0.0;

    void validate() const {
        TORCH_CHECK(class_id >= 0, "tuning class summary class_id must be non-negative");
        TORCH_CHECK(predicted_boxes >= 0, "tuning class summary predicted_boxes must be non-negative");
        TORCH_CHECK(target_boxes >= 0, "tuning class summary target_boxes must be non-negative");
        TORCH_CHECK(true_positives >= 0, "tuning class summary true_positives must be non-negative");
        TORCH_CHECK(false_positives >= 0, "tuning class summary false_positives must be non-negative");
        TORCH_CHECK(false_negatives >= 0, "tuning class summary false_negatives must be non-negative");
        TORCH_CHECK(precision >= 0.0 && precision <= 1.0, "tuning class summary precision must be in [0,1]");
        TORCH_CHECK(recall >= 0.0 && recall <= 1.0, "tuning class summary recall must be in [0,1]");
        TORCH_CHECK(f1 >= 0.0 && f1 <= 1.0, "tuning class summary f1 must be in [0,1]");
    }
};

struct YoloEvalRunReport {
    TuningRunReport run;
    std::vector<TuningClassSummary> per_class;

    void validate() const {
        run.validate();
        TORCH_CHECK(run.track_name == "yolo_mainline", "YOLO eval run report track_name must be yolo_mainline");
        TORCH_CHECK(!per_class.empty(), "YOLO eval run report must contain per-class summaries");
        for (const auto& cls : per_class) {
            cls.validate();
        }
    }
};

struct YoloMainlineRoundReport {
    TuningRunReport smoke_run;
    YoloEvalRunReport eval_run;
    bool round_passed = false;
    std::string summary;

    void validate() const {
        smoke_run.validate();
        eval_run.validate();
        TORCH_CHECK(smoke_run.track_name == "yolo_mainline", "YOLO mainline round smoke track must be yolo_mainline");
        TORCH_CHECK(eval_run.run.track_name == "yolo_mainline", "YOLO mainline round eval track must be yolo_mainline");
        TORCH_CHECK(!summary.empty(), "YOLO mainline round summary cannot be empty");
    }
};

inline TuningStageOutcome build_yolo_smoke_stage_outcome(
    int input_size,
    int batch_size,
    int max_train_batches,
    double smoke_step_loss,
    bool gradients_ok)
{
    TuningStageOutcome outcome;
    outcome.stage_name = "yolo_train_smoke_stage";
    outcome.selected_knobs = {
        {"input_size", std::to_string(input_size)},
        {"batch_size", std::to_string(batch_size)},
        {"max_train_batches", std::to_string(max_train_batches)}
    };
    outcome.metrics = {
        {TuningMetricKind::Loss, "smoke_step_loss", smoke_step_loss}
    };
    outcome.passed = gradients_ok && std::isfinite(smoke_step_loss) && smoke_step_loss >= 0.0;
    outcome.note = gradients_ok
        ? "Gradient path validated during the smoke training step."
        : "Smoke training step failed to produce valid gradients.";
    return outcome;
}

inline TuningRunReport build_yolo_smoke_run_report(
    int input_size,
    int batch_size,
    int max_train_batches,
    double smoke_step_loss,
    bool gradients_ok)
{
    TuningRunReport report;
    report.run_name = "yolo_smoke_train_run";
    report.track_name = "yolo_mainline";
    report.outcomes = {
        build_yolo_smoke_stage_outcome(
            input_size,
            batch_size,
            max_train_batches,
            smoke_step_loss,
            gradients_ok)
    };
    report.all_passed = report.outcomes.front().passed;
    report.summary = report.all_passed
        ? "YOLOv8 smoke training run passed with finite loss and valid gradients."
        : "YOLOv8 smoke training run needs attention before longer training loops.";
    return report;
}

inline YoloEvalRunReport build_yolo_eval_run_report(
    double loss,
    double precision,
    double recall,
    double f1,
    double matched_iou,
    int64_t true_positives,
    int64_t false_positives,
    int64_t false_negatives,
    int64_t predicted_boxes,
    int64_t target_boxes,
    const std::vector<TuningClassSummary>& per_class)
{
    TuningStageOutcome outcome;
    outcome.stage_name = "yolo_eval_stage";
    outcome.selected_knobs = {
        {"eval_mode", "dataset_validation"},
        {"match_threshold", "0.5"}
    };
    outcome.metrics = {
        {TuningMetricKind::Loss, "val_loss", loss},
        {TuningMetricKind::Map50, "precision", precision},
        {TuningMetricKind::Map50, "recall", recall},
        {TuningMetricKind::Map50, "f1", f1},
        {TuningMetricKind::IoU, "matched_iou", matched_iou}
    };
    outcome.passed = std::isfinite(loss) && target_boxes >= 0 && predicted_boxes >= 0;
    outcome.note = "YOLOv8 evaluation summary captured from the current validation path.";

    YoloEvalRunReport report;
    report.run.run_name = "yolo_eval_run";
    report.run.track_name = "yolo_mainline";
    report.run.outcomes = {outcome};
    report.run.all_passed = outcome.passed;
    report.run.summary =
        "YOLOv8 eval run recorded TP=" + std::to_string(true_positives) +
        " FP=" + std::to_string(false_positives) +
        " FN=" + std::to_string(false_negatives) +
        " Pred=" + std::to_string(predicted_boxes) +
        " Target=" + std::to_string(target_boxes);
    report.per_class = per_class;
    return report;
}

inline YoloMainlineRoundReport build_yolo_mainline_round_report(
    const TuningRunReport& smoke_run,
    const YoloEvalRunReport& eval_run)
{
    YoloMainlineRoundReport report;
    report.smoke_run = smoke_run;
    report.eval_run = eval_run;
    report.round_passed = smoke_run.all_passed && eval_run.run.all_passed;
    report.summary = report.round_passed
        ? "YOLOv8 mainline round passed smoke training and evaluation checks."
        : "YOLOv8 mainline round requires follow-up in smoke training or evaluation.";
    return report;
}

inline TuningRunReport flatten_yolo_mainline_round_report(const YoloMainlineRoundReport& round_report)
{
    round_report.validate();

    TuningRunReport report;
    report.run_name = "yolo_mainline_round_run";
    report.track_name = "yolo_mainline";
    report.outcomes = round_report.smoke_run.outcomes;
    report.outcomes.insert(
        report.outcomes.end(),
        round_report.eval_run.run.outcomes.begin(),
        round_report.eval_run.run.outcomes.end());
    report.all_passed = round_report.round_passed;
    report.summary = round_report.summary;
    return report;
}

inline TuningExperimentResult build_experiment_result_from_outcome(
    const std::string& experiment_id,
    const TuningStageOutcome& outcome)
{
    outcome.validate();

    TuningExperimentResult result;
    result.experiment_id = experiment_id;
    result.metrics = outcome.metrics;
    result.passed = outcome.passed;
    result.note = outcome.note;
    return result;
}

inline std::vector<TuningComparisonRow> build_yolo_mainline_round_comparison_rows(
    const YoloMainlineRoundReport& round_report)
{
    round_report.validate();

    std::vector<TuningComparisonRow> rows;
    rows.push_back({
        round_report.smoke_run.outcomes.front().stage_name,
        "yolo_smoke_current_round",
        "Use the current smoke training outcome as the active low-iteration training gate.",
        {build_experiment_result_from_outcome("yolo_smoke_current_round", round_report.smoke_run.outcomes.front())}
    });
    rows.push_back({
        round_report.eval_run.run.outcomes.front().stage_name,
        "yolo_eval_current_round",
        "Use the current evaluation outcome as the active dataset-backed validation reference.",
        {build_experiment_result_from_outcome("yolo_eval_current_round", round_report.eval_run.run.outcomes.front())}
    });
    return rows;
}

inline TuningRecommendation build_yolo_mainline_recommendation(
    const std::vector<TuningComparisonRow>& rows)
{
    TORCH_CHECK(!rows.empty(), "YOLO mainline recommendation requires comparison rows");

    TuningRecommendation recommendation;
    recommendation.track_name = "yolo_mainline";
    for (const auto& row : rows) {
        row.validate();
        recommendation.selected_experiments.push_back({row.stage_name, row.best_experiment_id});
    }
    recommendation.summary =
        "Current YOLOv8 recommendation keeps the smoke training gate and dataset evaluation gate from the latest validated round.";
    return recommendation;
}

struct YoloMainlineBundle {
    YoloMainlineRoundReport round_report;
    TuningRunReport flat_run;
    std::vector<TuningComparisonRow> comparison_rows;
    TuningRecommendation recommendation;

    void validate() const {
        round_report.validate();
        flat_run.validate();
        TORCH_CHECK(flat_run.track_name == "yolo_mainline", "YOLO mainline bundle flat run must target yolo_mainline");
        TORCH_CHECK(!comparison_rows.empty(), "YOLO mainline bundle must contain comparison rows");
        for (const auto& row : comparison_rows) {
            row.validate();
        }
        recommendation.validate();
        TORCH_CHECK(recommendation.track_name == "yolo_mainline",
            "YOLO mainline bundle recommendation track must be yolo_mainline");
    }
};

inline YoloMainlineBundle build_yolo_mainline_bundle(
    const TuningRunReport& smoke_run,
    const YoloEvalRunReport& eval_run)
{
    YoloMainlineBundle bundle;
    bundle.round_report = build_yolo_mainline_round_report(smoke_run, eval_run);
    bundle.flat_run = flatten_yolo_mainline_round_report(bundle.round_report);
    bundle.comparison_rows = build_yolo_mainline_round_comparison_rows(bundle.round_report);
    bundle.recommendation = build_yolo_mainline_recommendation(bundle.comparison_rows);
    return bundle;
}

inline std::string to_string(TuningTrack track) {
    switch (track) {
    case TuningTrack::YoloMainline: return "yolo_mainline";
    case TuningTrack::MobileViTMainline: return "mobilevit_mainline";
    case TuningTrack::LightweightSegmentation: return "lightweight_segmentation";
    case TuningTrack::BackboneBaseline: return "backbone_baseline";
    case TuningTrack::SegmentationAnalysis: return "segmentation_analysis";
    }
    TORCH_CHECK(false, "Unknown tuning track");
}

inline std::string to_string(TuningPhase phase) {
    switch (phase) {
    case TuningPhase::Smoke: return "smoke";
    case TuningPhase::Baseline: return "baseline";
    case TuningPhase::Stabilize: return "stabilize";
    case TuningPhase::Optimize: return "optimize";
    case TuningPhase::Compare: return "compare";
    }
    TORCH_CHECK(false, "Unknown tuning phase");
}

inline std::string to_string(TuningMetricKind kind) {
    switch (kind) {
    case TuningMetricKind::Loss: return "loss";
    case TuningMetricKind::Map50: return "map50";
    case TuningMetricKind::Top1: return "top1";
    case TuningMetricKind::IoU: return "iou";
    case TuningMetricKind::LatencyMs: return "latency_ms";
    case TuningMetricKind::MemoryMb: return "memory_mb";
    case TuningMetricKind::ForegroundIoU: return "foreground_iou";
    }
    TORCH_CHECK(false, "Unknown tuning metric kind");
}

inline std::vector<TuningPlanEntry> build_mainline_tuning_plan() {
    return {
        {
            "yolov8_smoke_gate",
            TuningTrack::YoloMainline,
            TuningPhase::Smoke,
            1,
            false,
            false,
            false,
            "Keep YOLOv8 forward/loss/train-step stable before dataset-backed tuning."
        },
        {
            "yolov8_baseline_dataset",
            TuningTrack::YoloMainline,
            TuningPhase::Baseline,
            2,
            true,
            true,
            false,
            "Establish a fixed YOLOv8 train/val baseline with stable augmentation and loss settings."
        },
        {
            "mobilevit_roi_baseline",
            TuningTrack::MobileViTMainline,
            TuningPhase::Baseline,
            3,
            true,
            true,
            false,
            "Lock ROI crop/resize/normalize settings for MobileViTv2 classification."
        },
        {
            "yolov8_stabilize_schedule",
            TuningTrack::YoloMainline,
            TuningPhase::Stabilize,
            4,
            true,
            true,
            false,
            "Tune learning rate, batch size, assigner, and loss weights until YOLOv8 metrics stop drifting."
        },
        {
            "mobilevit_stabilize_roi",
            TuningTrack::MobileViTMainline,
            TuningPhase::Stabilize,
            5,
            true,
            true,
            false,
            "Tune MobileViTv2 ROI input size, patch size, and classifier regularization for stable convergence."
        },
        {
            "mobilenetv3_deeplab_support",
            TuningTrack::LightweightSegmentation,
            TuningPhase::Optimize,
            6,
            true,
            true,
            false,
            "Use MobileNetV3 + DeepLabV3Plus as the lightweight structure-analysis path after mainline baselines settle."
        },
        {
            "resnet_backbone_compare",
            TuningTrack::BackboneBaseline,
            TuningPhase::Compare,
            7,
            true,
            true,
            true,
            "Compare ResNet18 and ResNet50 as baseline and upper-bound backbones against the mainline models."
        },
        {
            "deeplab_analysis_compare",
            TuningTrack::SegmentationAnalysis,
            TuningPhase::Compare,
            8,
            true,
            true,
            false,
            "Use DeepLab variants to analyze foreground quality and structural error modes after YOLOv8 and MobileViTv2 are stable."
        }
    };
}

inline bool is_ready(const TuningPlanEntry& entry, const TuningReadiness& readiness) {
    entry.validate();
    if (entry.requires_dataset && !readiness.dataset_available) {
        return false;
    }
    if (entry.requires_labels && !readiness.labels_available) {
        return false;
    }
    if (entry.requires_external_weights && !readiness.external_weights_available) {
        return false;
    }
    return true;
}

inline std::vector<TuningPlanEntry> filter_ready_plan(
    const std::vector<TuningPlanEntry>& plan,
    const TuningReadiness& readiness)
{
    std::vector<TuningPlanEntry> ready;
    for (const auto& entry : plan) {
        if (is_ready(entry, readiness)) {
            ready.push_back(entry);
        }
    }
    return ready;
}

inline std::vector<TuningPlanEntry> filter_plan_by_track(
    const std::vector<TuningPlanEntry>& plan,
    TuningTrack track)
{
    std::vector<TuningPlanEntry> filtered;
    for (const auto& entry : plan) {
        if (entry.track == track) {
            filtered.push_back(entry);
        }
    }
    return filtered;
}

inline std::vector<TuningPlanEntry> build_yolo_mainline_plan() {
    return filter_plan_by_track(build_mainline_tuning_plan(), TuningTrack::YoloMainline);
}

inline std::vector<TuningPlanEntry> build_mobilevit_mainline_plan() {
    return filter_plan_by_track(build_mainline_tuning_plan(), TuningTrack::MobileViTMainline);
}

inline std::vector<TuningPlanEntry> build_segmentation_support_plan() {
    auto plan = build_mainline_tuning_plan();
    std::vector<TuningPlanEntry> filtered;
    for (const auto& entry : plan) {
        if (entry.track == TuningTrack::LightweightSegmentation ||
            entry.track == TuningTrack::BackboneBaseline ||
            entry.track == TuningTrack::SegmentationAnalysis) {
            filtered.push_back(entry);
        }
    }
    return filtered;
}

inline std::vector<TuningStepSpec> build_yolo_mainline_specs() {
    return {
        {
            "yolo_smoke_gate",
            "Confirm the detector stays numerically stable before dataset-backed tuning.",
            {
                {"batch_size", "2", "{2,4}"},
                {"input_size", "320", "{320,416}"},
                {"lr", "0.001", "[1e-4,1e-3]"}
            },
            {
                {TuningMetricKind::Loss, "total_loss", false, 100.0},
                {TuningMetricKind::LatencyMs, "train_step_latency_ms", false, 2000.0}
            }
        },
        {
            "yolo_baseline_dataset",
            "Lock the first dataset-backed baseline before schedule tuning.",
            {
                {"input_size", "640", "{512,640,768}"},
                {"use_letterbox", "false", "{false,true}"},
                {"flip_prob", "0.5", "[0.0,0.5]"}
            },
            {
                {TuningMetricKind::Loss, "val_loss", false, 10.0},
                {TuningMetricKind::Map50, "map50", true, 0.1}
            }
        },
        {
            "yolo_stabilize_schedule",
            "Tune schedule and assigner settings after the baseline is repeatable.",
            {
                {"lr", "0.01", "[1e-4,1e-2]"},
                {"batch_size", "16", "{8,16,32}"},
                {"assigner_topk", "10", "{5,10,13}"},
                {"box_loss_weight", "1.0", "[0.5,2.0]"},
                {"cls_loss_weight", "1.0", "[0.5,2.0]"}
            },
            {
                {TuningMetricKind::Loss, "train_val_gap", false, 2.0},
                {TuningMetricKind::Map50, "map50", true, 0.2},
                {TuningMetricKind::MemoryMb, "peak_memory_mb", false, 12000.0}
            }
        }
    };
}

inline std::vector<TuningStepSpec> build_mobilevit_mainline_specs() {
    return {
        {
            "mobilevit_roi_baseline",
            "Lock ROI crop and classifier input policy before fine tuning the backbone.",
            {
                {"input_size", "256", "{224,256,288}"},
                {"patch_size", "2", "{2,4}"},
                {"dropout", "0.0", "[0.0,0.2]"}
            },
            {
                {TuningMetricKind::Loss, "cls_loss", false, 5.0},
                {TuningMetricKind::Top1, "top1", true, 0.5}
            }
        },
        {
            "mobilevit_stabilize_roi",
            "Tune ROI classification convergence after the input contract is fixed.",
            {
                {"lr", "0.001", "[1e-4,1e-3]"},
                {"batch_size", "32", "{16,32,64}"},
                {"embedding_dim", "640", "{512,640,768}"}
            },
            {
                {TuningMetricKind::Loss, "val_loss", false, 3.0},
                {TuningMetricKind::Top1, "top1", true, 0.7},
                {TuningMetricKind::LatencyMs, "infer_latency_ms", false, 100.0}
            }
        }
    };
}

inline std::vector<TuningStepSpec> build_segmentation_support_specs() {
    return {
        {
            "mobilenetv3_deeplab_support",
            "Establish a lightweight segmentation support path for structure-aware ROI cleanup.",
            {
                {"backbone", "mobilenet_v3_large", "{mobilenet_v3_large}"},
                {"output_stride", "16", "{8,16}"},
                {"decoder_channels", "256", "{128,256}"}
            },
            {
                {TuningMetricKind::ForegroundIoU, "foreground_iou", true, 0.5},
                {TuningMetricKind::LatencyMs, "seg_latency_ms", false, 200.0}
            }
        },
        {
            "resnet_backbone_compare",
            "Compare heavier backbones after the lightweight support path is stable.",
            {
                {"backbone", "resnet18", "{resnet18,resnet50}"},
                {"input_size", "512", "{512,640}"},
                {"output_stride", "16", "{8,16}"}
            },
            {
                {TuningMetricKind::ForegroundIoU, "foreground_iou", true, 0.55},
                {TuningMetricKind::MemoryMb, "peak_memory_mb", false, 16000.0}
            }
        },
        {
            "deeplab_analysis_compare",
            "Use segmentation to analyze foreground quality after detection and ROI classification are stable.",
            {
                {"decoder", "deeplabv3plus", "{deeplabv3,deeplabv3plus}"},
                {"aspp_rates", "12,24,36", "{6,12,18|12,24,36}"},
                {"low_level_channels", "128", "{48,128,256,960}"}
            },
            {
                {TuningMetricKind::ForegroundIoU, "foreground_iou", true, 0.6},
                {TuningMetricKind::IoU, "boundary_iou", true, 0.5}
            }
        }
    };
}

inline std::vector<TuningExecutionItem> build_current_priority_execution_plan(
    const TuningReadiness& readiness)
{
    std::vector<TuningExecutionItem> execution;

    const auto ready_plan = filter_ready_plan(build_mainline_tuning_plan(), readiness);
    const auto yolo_specs = build_yolo_mainline_specs();
    const auto mobilevit_specs = build_mobilevit_mainline_specs();
    const auto segmentation_specs = build_segmentation_support_specs();

    for (const auto& entry : ready_plan) {
        const TuningStepSpec* matched_spec = nullptr;

        if (entry.track == TuningTrack::YoloMainline) {
            for (const auto& spec : yolo_specs) {
                if (entry.name == "yolov8_smoke_gate" && spec.step_name == "yolo_smoke_gate") {
                    matched_spec = &spec;
                    break;
                }
                if (entry.name == "yolov8_baseline_dataset" && spec.step_name == "yolo_baseline_dataset") {
                    matched_spec = &spec;
                    break;
                }
                if (entry.name == "yolov8_stabilize_schedule" && spec.step_name == "yolo_stabilize_schedule") {
                    matched_spec = &spec;
                    break;
                }
            }
        } else if (entry.track == TuningTrack::MobileViTMainline) {
            for (const auto& spec : mobilevit_specs) {
                if (entry.name == "mobilevit_roi_baseline" && spec.step_name == "mobilevit_roi_baseline") {
                    matched_spec = &spec;
                    break;
                }
                if (entry.name == "mobilevit_stabilize_roi" && spec.step_name == "mobilevit_stabilize_roi") {
                    matched_spec = &spec;
                    break;
                }
            }
        } else {
            for (const auto& spec : segmentation_specs) {
                if (entry.name == "mobilenetv3_deeplab_support" &&
                    spec.step_name == "mobilenetv3_deeplab_support") {
                    matched_spec = &spec;
                    break;
                }
                if (entry.name == "resnet_backbone_compare" &&
                    spec.step_name == "resnet_backbone_compare") {
                    matched_spec = &spec;
                    break;
                }
                if (entry.name == "deeplab_analysis_compare" &&
                    spec.step_name == "deeplab_analysis_compare") {
                    matched_spec = &spec;
                    break;
                }
            }
        }

        TORCH_CHECK(matched_spec != nullptr, "No tuning spec mapped for ready plan entry: ", entry.name);
        execution.push_back({entry, *matched_spec});
    }

    return execution;
}

inline std::vector<TuningBatch> build_execution_batches(const TuningReadiness& readiness) {
    const auto execution = build_current_priority_execution_plan(readiness);
    std::vector<TuningBatch> batches;

    std::vector<TuningExecutionItem> yolo_items;
    std::vector<TuningExecutionItem> mobilevit_items;
    std::vector<TuningExecutionItem> segmentation_items;

    for (const auto& item : execution) {
        if (item.plan.track == TuningTrack::YoloMainline) {
            yolo_items.push_back(item);
        } else if (item.plan.track == TuningTrack::MobileViTMainline) {
            mobilevit_items.push_back(item);
        } else {
            segmentation_items.push_back(item);
        }
    }

    if (!yolo_items.empty()) {
        batches.push_back({
            "yolo_mainline_batch",
            "Run YOLOv8 smoke, baseline, and schedule stabilization before branching out.",
            yolo_items
        });
    }

    if (!mobilevit_items.empty()) {
        batches.push_back({
            "mobilevit_mainline_batch",
            "Tune ROI classification after detector settings are stable enough to provide usable crops.",
            mobilevit_items
        });
    }

    if (!segmentation_items.empty()) {
        batches.push_back({
            "segmentation_support_batch",
            "Use segmentation and backbone comparisons only after the two mainlines are defined.",
            segmentation_items
        });
    }

    return batches;
}

inline std::vector<TuningBatchSweep> build_execution_batch_sweeps(const TuningReadiness& readiness) {
    const auto batches = build_execution_batches(readiness);
    std::vector<TuningBatchSweep> batch_sweeps;

    for (const auto& batch : batches) {
        if (batch.name == "yolo_mainline_batch") {
            batch_sweeps.push_back({
                batch.name,
                {
                    {"yolo_input_size_sweep", "input_size", {"320", "416", "512", "640"}},
                    {"yolo_lr_sweep", "lr", {"0.0001", "0.001", "0.01"}},
                    {"yolo_batch_size_sweep", "batch_size", {"8", "16", "32"}}
                }
            });
        } else if (batch.name == "mobilevit_mainline_batch") {
            batch_sweeps.push_back({
                batch.name,
                {
                    {"mobilevit_roi_size_sweep", "input_size", {"224", "256", "288"}},
                    {"mobilevit_patch_sweep", "patch_size", {"2", "4"}},
                    {"mobilevit_dropout_sweep", "dropout", {"0.0", "0.1", "0.2"}}
                }
            });
        } else if (batch.name == "segmentation_support_batch") {
            batch_sweeps.push_back({
                batch.name,
                {
                    {"seg_output_stride_sweep", "output_stride", {"8", "16"}},
                    {"seg_decoder_channels_sweep", "decoder_channels", {"128", "256"}},
                    {"seg_backbone_sweep", "backbone", {"mobilenet_v3_large", "resnet18", "resnet50"}}
                }
            });
        }
    }

    return batch_sweeps;
}

inline std::vector<TuningSweepStage> build_execution_sweep_stages(const TuningReadiness& readiness) {
    const auto execution = build_current_priority_execution_plan(readiness);
    const auto batches = build_execution_batches(readiness);
    std::vector<TuningSweepStage> stages;

    bool has_yolo_shape = false;
    bool has_yolo_schedule = false;
    bool has_yolo_loss = false;
    bool has_mobilevit_input = false;
    bool has_mobilevit_stabilize = false;
    bool has_segmentation_geometry = false;
    bool has_segmentation_backbone = false;

    for (const auto& item : execution) {
        if (item.plan.name == "yolov8_smoke_gate") {
            has_yolo_shape = true;
        } else if (item.plan.name == "yolov8_baseline_dataset") {
            has_yolo_schedule = true;
        } else if (item.plan.name == "yolov8_stabilize_schedule") {
            has_yolo_loss = true;
        } else if (item.plan.name == "mobilevit_roi_baseline") {
            has_mobilevit_input = true;
        } else if (item.plan.name == "mobilevit_stabilize_roi") {
            has_mobilevit_stabilize = true;
        } else if (item.plan.name == "mobilenetv3_deeplab_support") {
            has_segmentation_geometry = true;
        } else if (item.plan.name == "resnet_backbone_compare" ||
                   item.plan.name == "deeplab_analysis_compare") {
            has_segmentation_backbone = true;
        }
    }

    for (const auto& batch : batches) {
        if (batch.name == "yolo_mainline_batch") {
            if (has_yolo_shape) {
                stages.push_back({
                    batch.name,
                    "yolo_shape_stage",
                    "Stabilize detector input geometry before schedule and loss tuning.",
                    {
                        {"yolo_input_size_sweep", "input_size", {"320", "416", "512", "640"}}
                    }
                });
            }
            if (has_yolo_schedule) {
                stages.push_back({
                    batch.name,
                    "yolo_schedule_stage",
                    "Tune learning dynamics after the input contract is fixed.",
                    {
                        {"yolo_lr_sweep", "lr", {"0.0001", "0.001", "0.01"}},
                        {"yolo_batch_size_sweep", "batch_size", {"8", "16", "32"}}
                    }
                });
            }
            if (has_yolo_loss) {
                stages.push_back({
                    batch.name,
                    "yolo_loss_stage",
                    "Tune assigner and loss weights after schedule drift is controlled.",
                    {
                        {"yolo_assigner_topk_sweep", "assigner_topk", {"5", "10", "13"}},
                        {"yolo_box_loss_sweep", "box_loss_weight", {"0.5", "1.0", "1.5", "2.0"}},
                        {"yolo_cls_loss_sweep", "cls_loss_weight", {"0.5", "1.0", "1.5", "2.0"}}
                    }
                });
            }
        } else if (batch.name == "mobilevit_mainline_batch") {
            if (has_mobilevit_input) {
                stages.push_back({
                    batch.name,
                    "mobilevit_input_stage",
                    "Lock ROI input geometry before transformer-specific tuning.",
                    {
                        {"mobilevit_roi_size_sweep", "input_size", {"224", "256", "288"}}
                    }
                });
            }
            if (has_mobilevit_stabilize) {
                stages.push_back({
                    batch.name,
                    "mobilevit_regularization_stage",
                    "Tune tokenization and regularization after ROI sizing is stable.",
                    {
                        {"mobilevit_patch_sweep", "patch_size", {"2", "4"}},
                        {"mobilevit_dropout_sweep", "dropout", {"0.0", "0.1", "0.2"}}
                    }
                });
                stages.push_back({
                    batch.name,
                    "mobilevit_capacity_stage",
                    "Tune embedding capacity after ROI shape and regularization settle.",
                    {
                        {"mobilevit_embedding_sweep", "embedding_dim", {"512", "640", "768"}},
                        {"mobilevit_batch_size_sweep", "batch_size", {"16", "32", "64"}}
                    }
                });
            }
        } else if (batch.name == "segmentation_support_batch") {
            if (has_segmentation_geometry) {
                stages.push_back({
                    batch.name,
                    "segmentation_geometry_stage",
                    "Tune segmentation spatial resolution before decoder and backbone comparisons.",
                    {
                        {"seg_output_stride_sweep", "output_stride", {"8", "16"}},
                        {"seg_decoder_channels_sweep", "decoder_channels", {"128", "256"}}
                    }
                });
            }
            if (has_segmentation_backbone) {
                stages.push_back({
                    batch.name,
                    "segmentation_backbone_stage",
                    "Compare lightweight and heavy backbones after geometry stabilizes.",
                    {
                        {"seg_backbone_sweep", "backbone", {"mobilenet_v3_large", "resnet18", "resnet50"}}
                    }
                });
            }
        }
    }

    return stages;
}

inline std::vector<TuningStageGate> build_execution_stage_gates(const TuningReadiness& readiness) {
    const auto stages = build_execution_sweep_stages(readiness);
    std::vector<TuningStageGate> gates;

    for (const auto& stage : stages) {
        if (stage.stage_name == "yolo_shape_stage") {
            gates.push_back({
                stage.stage_name,
                {
                    {TuningMetricKind::Loss, "total_loss", false, 50.0},
                    {TuningMetricKind::LatencyMs, "train_step_latency_ms", false, 1500.0}
                },
                {
                    "stop if loss becomes non-finite",
                    "stop if image-size change does not improve stability after one full sweep"
                }
            });
        } else if (stage.stage_name == "yolo_schedule_stage") {
            gates.push_back({
                stage.stage_name,
                {
                    {TuningMetricKind::Map50, "map50", true, 0.2},
                    {TuningMetricKind::Loss, "train_val_gap", false, 2.0}
                },
                {
                    "stop if train-val gap widens for two consecutive runs",
                    "stop if no map50 improvement appears across the lr and batch sweep"
                }
            });
        } else if (stage.stage_name == "yolo_loss_stage") {
            gates.push_back({
                stage.stage_name,
                {
                    {TuningMetricKind::Map50, "map50", true, 0.25},
                    {TuningMetricKind::MemoryMb, "peak_memory_mb", false, 12000.0}
                },
                {
                    "stop if loss-weight changes only trade mAP for instability",
                    "stop if assigner or loss settings exceed memory budget"
                }
            });
        } else if (stage.stage_name == "mobilevit_input_stage") {
            gates.push_back({
                stage.stage_name,
                {
                    {TuningMetricKind::Top1, "top1", true, 0.55},
                    {TuningMetricKind::Loss, "cls_loss", false, 4.0}
                },
                {
                    "stop if ROI size changes no longer improve top1",
                    "stop if larger ROI inputs add latency without accuracy gain"
                }
            });
        } else if (stage.stage_name == "mobilevit_regularization_stage") {
            gates.push_back({
                stage.stage_name,
                {
                    {TuningMetricKind::Top1, "top1", true, 0.65},
                    {TuningMetricKind::LatencyMs, "infer_latency_ms", false, 100.0}
                },
                {
                    "stop if patch or dropout changes reduce stability on repeated runs",
                    "stop if regularization gains are offset by unacceptable latency"
                }
            });
        } else if (stage.stage_name == "mobilevit_capacity_stage") {
            gates.push_back({
                stage.stage_name,
                {
                    {TuningMetricKind::Top1, "top1", true, 0.7},
                    {TuningMetricKind::MemoryMb, "peak_memory_mb", false, 8000.0}
                },
                {
                    "stop if larger embeddings do not improve top1",
                    "stop if capacity increases exceed memory budget"
                }
            });
        } else if (stage.stage_name == "segmentation_geometry_stage") {
            gates.push_back({
                stage.stage_name,
                {
                    {TuningMetricKind::ForegroundIoU, "foreground_iou", true, 0.55},
                    {TuningMetricKind::LatencyMs, "seg_latency_ms", false, 200.0}
                },
                {
                    "stop if output stride changes do not improve foreground IoU",
                    "stop if decoder width increases only add latency"
                }
            });
        } else if (stage.stage_name == "segmentation_backbone_stage") {
            gates.push_back({
                stage.stage_name,
                {
                    {TuningMetricKind::ForegroundIoU, "foreground_iou", true, 0.6},
                    {TuningMetricKind::MemoryMb, "peak_memory_mb", false, 16000.0}
                },
                {
                    "stop if heavier backbones do not improve foreground IoU enough",
                    "stop if backbone choice breaks deployment memory constraints"
                }
            });
        }
    }

    return gates;
}

inline std::vector<TuningStageOutcome> build_example_stage_outcomes(const TuningReadiness& readiness) {
    const auto stages = build_execution_sweep_stages(readiness);
    std::vector<TuningStageOutcome> outcomes;

    for (const auto& stage : stages) {
        if (stage.stage_name == "yolo_shape_stage") {
            outcomes.push_back({
                stage.stage_name,
                {{"input_size", "512"}},
                {
                    {TuningMetricKind::Loss, "total_loss", 18.5},
                    {TuningMetricKind::LatencyMs, "train_step_latency_ms", 920.0}
                },
                true,
                "Selected the largest stable input before schedule tuning."
            });
        } else if (stage.stage_name == "yolo_schedule_stage") {
            outcomes.push_back({
                stage.stage_name,
                {{"lr", "0.001"}, {"batch_size", "16"}},
                {
                    {TuningMetricKind::Map50, "map50", 0.28},
                    {TuningMetricKind::Loss, "train_val_gap", 1.3}
                },
                true,
                "Learning-rate and batch-size pair reduced drift while improving validation accuracy."
            });
        } else if (stage.stage_name == "yolo_loss_stage") {
            outcomes.push_back({
                stage.stage_name,
                {{"assigner_topk", "10"}, {"box_loss_weight", "1.0"}, {"cls_loss_weight", "1.0"}},
                {
                    {TuningMetricKind::Map50, "map50", 0.31},
                    {TuningMetricKind::MemoryMb, "peak_memory_mb", 7420.0}
                },
                true,
                "Default loss weighting remained the best tradeoff under the current memory budget."
            });
        } else if (stage.stage_name == "mobilevit_input_stage") {
            outcomes.push_back({
                stage.stage_name,
                {{"input_size", "256"}},
                {
                    {TuningMetricKind::Top1, "top1", 0.62},
                    {TuningMetricKind::Loss, "cls_loss", 2.8}
                },
                true,
                "ROI size 256 balanced context and classifier stability."
            });
        } else if (stage.stage_name == "mobilevit_regularization_stage") {
            outcomes.push_back({
                stage.stage_name,
                {{"patch_size", "2"}, {"dropout", "0.1"}},
                {
                    {TuningMetricKind::Top1, "top1", 0.68},
                    {TuningMetricKind::LatencyMs, "infer_latency_ms", 58.0}
                },
                true,
                "Conservative regularization improved repeatability without hurting latency."
            });
        } else if (stage.stage_name == "mobilevit_capacity_stage") {
            outcomes.push_back({
                stage.stage_name,
                {{"embedding_dim", "640"}, {"batch_size", "32"}},
                {
                    {TuningMetricKind::Top1, "top1", 0.71},
                    {TuningMetricKind::MemoryMb, "peak_memory_mb", 3890.0}
                },
                true,
                "Baseline embedding size remained the best balance between accuracy and memory."
            });
        } else if (stage.stage_name == "segmentation_geometry_stage") {
            outcomes.push_back({
                stage.stage_name,
                {{"output_stride", "16"}, {"decoder_channels", "256"}},
                {
                    {TuningMetricKind::ForegroundIoU, "foreground_iou", 0.57},
                    {TuningMetricKind::LatencyMs, "seg_latency_ms", 144.0}
                },
                true,
                "Geometry stage favored the lower-latency stride/decoder combination."
            });
        } else if (stage.stage_name == "segmentation_backbone_stage") {
            outcomes.push_back({
                stage.stage_name,
                {{"backbone", "mobilenet_v3_large"}},
                {
                    {TuningMetricKind::ForegroundIoU, "foreground_iou", 0.61},
                    {TuningMetricKind::MemoryMb, "peak_memory_mb", 5120.0}
                },
                true,
                "The lightweight backbone delivered enough foreground quality for support usage."
            });
        }
    }

    return outcomes;
}

inline std::vector<TuningRunReport> build_example_run_reports(const TuningReadiness& readiness) {
    const auto outcomes = build_example_stage_outcomes(readiness);
    std::vector<TuningRunReport> reports;

    std::vector<TuningStageOutcome> yolo_outcomes;
    std::vector<TuningStageOutcome> mobilevit_outcomes;
    std::vector<TuningStageOutcome> segmentation_outcomes;

    for (const auto& outcome : outcomes) {
        if (outcome.stage_name.find("yolo_") == 0) {
            yolo_outcomes.push_back(outcome);
        } else if (outcome.stage_name.find("mobilevit_") == 0) {
            mobilevit_outcomes.push_back(outcome);
        } else if (outcome.stage_name.find("segmentation_") == 0) {
            segmentation_outcomes.push_back(outcome);
        }
    }

    if (!yolo_outcomes.empty()) {
        reports.push_back({
            "yolo_mainline_run",
            "yolo_mainline",
            yolo_outcomes,
            true,
            "YOLOv8 finished shape, schedule, and loss tuning with stable mAP and acceptable memory usage."
        });
    }

    if (!mobilevit_outcomes.empty()) {
        reports.push_back({
            "mobilevit_mainline_run",
            "mobilevit_mainline",
            mobilevit_outcomes,
            true,
            "MobileViTv2 stabilized ROI sizing, regularization, and embedding capacity for classifier tuning."
        });
    }

    if (!segmentation_outcomes.empty()) {
        reports.push_back({
            "segmentation_support_run",
            "segmentation_support",
            segmentation_outcomes,
            true,
            "Segmentation support settled on a lightweight backbone while keeping foreground quality acceptable."
        });
    }

    return reports;
}

inline std::vector<TuningChecklistItem> build_mainline_execution_checklist(const TuningReadiness& readiness) {
    const auto stages = build_execution_sweep_stages(readiness);
    std::vector<TuningChecklistItem> checklist;

    for (const auto& stage : stages) {
        if (stage.stage_name == "yolo_shape_stage") {
            checklist.push_back({
                "yolo_mainline",
                stage.stage_name,
                "Sweep detector input sizes and keep the largest numerically stable configuration.",
                "A selected input_size and a shape-stage stability note.",
                false
            });
        } else if (stage.stage_name == "yolo_schedule_stage") {
            checklist.push_back({
                "yolo_mainline",
                stage.stage_name,
                "Sweep learning rate and batch size on the fixed detector input.",
                "A preferred lr/batch_size pair with map50 and train_val_gap measurements.",
                true
            });
        } else if (stage.stage_name == "yolo_loss_stage") {
            checklist.push_back({
                "yolo_mainline",
                stage.stage_name,
                "Sweep assigner_topk and loss weights after schedule drift is controlled.",
                "A stable assigner/loss-weight combination with memory usage recorded.",
                true
            });
        } else if (stage.stage_name == "mobilevit_input_stage") {
            checklist.push_back({
                "mobilevit_mainline",
                stage.stage_name,
                "Sweep ROI input size while keeping the classifier backbone fixed.",
                "A preferred ROI size with top1 and cls_loss measurements.",
                true
            });
        } else if (stage.stage_name == "mobilevit_regularization_stage") {
            checklist.push_back({
                "mobilevit_mainline",
                stage.stage_name,
                "Sweep patch size and dropout after ROI sizing stabilizes.",
                "A regularization choice with top1 and inference latency recorded.",
                true
            });
        } else if (stage.stage_name == "mobilevit_capacity_stage") {
            checklist.push_back({
                "mobilevit_mainline",
                stage.stage_name,
                "Sweep embedding_dim and batch_size once ROI input and regularization are fixed.",
                "A final MobileViTv2 capacity selection with top1 and memory usage recorded.",
                true
            });
        }
    }

    return checklist;
}

inline std::vector<TuningExperimentRow> build_first_pass_mainline_matrix(const TuningReadiness& readiness) {
    std::vector<TuningExperimentRow> rows;
    const auto checklist = build_mainline_execution_checklist(readiness);

    for (const auto& item : checklist) {
        if (item.stage_name == "yolo_shape_stage") {
            rows.push_back({
                item.track_name,
                item.stage_name,
                "yolo_shape_320",
                {{"input_size", "320"}},
                {"total_loss", "train_step_latency_ms"}
            });
            rows.push_back({
                item.track_name,
                item.stage_name,
                "yolo_shape_512",
                {{"input_size", "512"}},
                {"total_loss", "train_step_latency_ms"}
            });
            rows.push_back({
                item.track_name,
                item.stage_name,
                "yolo_shape_640",
                {{"input_size", "640"}},
                {"total_loss", "train_step_latency_ms"}
            });
        } else if (item.stage_name == "yolo_schedule_stage") {
            rows.push_back({
                item.track_name,
                item.stage_name,
                "yolo_schedule_lr1e3_b16",
                {{"lr", "0.001"}, {"batch_size", "16"}},
                {"map50", "train_val_gap"}
            });
            rows.push_back({
                item.track_name,
                item.stage_name,
                "yolo_schedule_lr1e4_b32",
                {{"lr", "0.0001"}, {"batch_size", "32"}},
                {"map50", "train_val_gap"}
            });
        } else if (item.stage_name == "yolo_loss_stage") {
            rows.push_back({
                item.track_name,
                item.stage_name,
                "yolo_loss_default",
                {{"assigner_topk", "10"}, {"box_loss_weight", "1.0"}, {"cls_loss_weight", "1.0"}},
                {"map50", "peak_memory_mb"}
            });
            rows.push_back({
                item.track_name,
                item.stage_name,
                "yolo_loss_heavier_box",
                {{"assigner_topk", "13"}, {"box_loss_weight", "1.5"}, {"cls_loss_weight", "1.0"}},
                {"map50", "peak_memory_mb"}
            });
        } else if (item.stage_name == "mobilevit_input_stage") {
            rows.push_back({
                item.track_name,
                item.stage_name,
                "mobilevit_input_224",
                {{"input_size", "224"}},
                {"top1", "cls_loss"}
            });
            rows.push_back({
                item.track_name,
                item.stage_name,
                "mobilevit_input_256",
                {{"input_size", "256"}},
                {"top1", "cls_loss"}
            });
        } else if (item.stage_name == "mobilevit_regularization_stage") {
            rows.push_back({
                item.track_name,
                item.stage_name,
                "mobilevit_reg_patch2_drop0",
                {{"patch_size", "2"}, {"dropout", "0.0"}},
                {"top1", "infer_latency_ms"}
            });
            rows.push_back({
                item.track_name,
                item.stage_name,
                "mobilevit_reg_patch2_drop01",
                {{"patch_size", "2"}, {"dropout", "0.1"}},
                {"top1", "infer_latency_ms"}
            });
        } else if (item.stage_name == "mobilevit_capacity_stage") {
            rows.push_back({
                item.track_name,
                item.stage_name,
                "mobilevit_cap_640_b32",
                {{"embedding_dim", "640"}, {"batch_size", "32"}},
                {"top1", "peak_memory_mb"}
            });
            rows.push_back({
                item.track_name,
                item.stage_name,
                "mobilevit_cap_768_b16",
                {{"embedding_dim", "768"}, {"batch_size", "16"}},
                {"top1", "peak_memory_mb"}
            });
        }
    }

    return rows;
}

inline std::vector<TuningExperimentResult> build_example_first_pass_results(const TuningReadiness& readiness) {
    const auto matrix = build_first_pass_mainline_matrix(readiness);
    std::vector<TuningExperimentResult> results;

    for (const auto& row : matrix) {
        if (row.experiment_id == "yolo_shape_320") {
            results.push_back({
                row.experiment_id,
                {
                    {TuningMetricKind::Loss, "total_loss", 24.0},
                    {TuningMetricKind::LatencyMs, "train_step_latency_ms", 640.0}
                },
                true,
                "Stable but left detector capacity unused."
            });
        } else if (row.experiment_id == "yolo_shape_512") {
            results.push_back({
                row.experiment_id,
                {
                    {TuningMetricKind::Loss, "total_loss", 18.5},
                    {TuningMetricKind::LatencyMs, "train_step_latency_ms", 920.0}
                },
                true,
                "Best balance between stability and input capacity."
            });
        } else if (row.experiment_id == "yolo_shape_640") {
            results.push_back({
                row.experiment_id,
                {
                    {TuningMetricKind::Loss, "total_loss", 18.2},
                    {TuningMetricKind::LatencyMs, "train_step_latency_ms", 1420.0}
                },
                false,
                "Slightly lower loss, but the latency headroom became too tight."
            });
        } else if (row.experiment_id == "yolo_schedule_lr1e3_b16") {
            results.push_back({
                row.experiment_id,
                {
                    {TuningMetricKind::Map50, "map50", 0.28},
                    {TuningMetricKind::Loss, "train_val_gap", 1.3}
                },
                true,
                "Improved mAP while keeping drift controlled."
            });
        } else if (row.experiment_id == "yolo_schedule_lr1e4_b32") {
            results.push_back({
                row.experiment_id,
                {
                    {TuningMetricKind::Map50, "map50", 0.22},
                    {TuningMetricKind::Loss, "train_val_gap", 0.9}
                },
                false,
                "Lower drift but the accuracy gain was insufficient."
            });
        } else if (row.experiment_id == "yolo_loss_default") {
            results.push_back({
                row.experiment_id,
                {
                    {TuningMetricKind::Map50, "map50", 0.31},
                    {TuningMetricKind::MemoryMb, "peak_memory_mb", 7420.0}
                },
                true,
                "Default weighting stayed within memory budget and kept the best mAP."
            });
        } else if (row.experiment_id == "yolo_loss_heavier_box") {
            results.push_back({
                row.experiment_id,
                {
                    {TuningMetricKind::Map50, "map50", 0.30},
                    {TuningMetricKind::MemoryMb, "peak_memory_mb", 7810.0}
                },
                false,
                "Extra box emphasis did not justify the extra memory use."
            });
        } else if (row.experiment_id == "mobilevit_input_224") {
            results.push_back({
                row.experiment_id,
                {
                    {TuningMetricKind::Top1, "top1", 0.59},
                    {TuningMetricKind::Loss, "cls_loss", 3.1}
                },
                false,
                "Smaller crop reduced context too much."
            });
        } else if (row.experiment_id == "mobilevit_input_256") {
            results.push_back({
                row.experiment_id,
                {
                    {TuningMetricKind::Top1, "top1", 0.62},
                    {TuningMetricKind::Loss, "cls_loss", 2.8}
                },
                true,
                "Best first-pass balance for ROI classification."
            });
        } else if (row.experiment_id == "mobilevit_reg_patch2_drop0") {
            results.push_back({
                row.experiment_id,
                {
                    {TuningMetricKind::Top1, "top1", 0.66},
                    {TuningMetricKind::LatencyMs, "infer_latency_ms", 55.0}
                },
                false,
                "Good latency, but less stable than a small dropout."
            });
        } else if (row.experiment_id == "mobilevit_reg_patch2_drop01") {
            results.push_back({
                row.experiment_id,
                {
                    {TuningMetricKind::Top1, "top1", 0.68},
                    {TuningMetricKind::LatencyMs, "infer_latency_ms", 58.0}
                },
                true,
                "Slight dropout improved repeatability with acceptable latency."
            });
        } else if (row.experiment_id == "mobilevit_cap_640_b32") {
            results.push_back({
                row.experiment_id,
                {
                    {TuningMetricKind::Top1, "top1", 0.71},
                    {TuningMetricKind::MemoryMb, "peak_memory_mb", 3890.0}
                },
                true,
                "Baseline embedding stayed memory-efficient and accurate."
            });
        } else if (row.experiment_id == "mobilevit_cap_768_b16") {
            results.push_back({
                row.experiment_id,
                {
                    {TuningMetricKind::Top1, "top1", 0.715},
                    {TuningMetricKind::MemoryMb, "peak_memory_mb", 5120.0}
                },
                false,
                "Accuracy gain was too small for the extra memory cost."
            });
        }
    }

    return results;
}

inline std::vector<TuningComparisonRow> build_first_pass_comparison_table(const TuningReadiness& readiness) {
    const auto results = build_example_first_pass_results(readiness);
    std::vector<TuningComparisonRow> table;

    auto select_by_prefix = [&](const std::string& stage_name,
                                const std::string& best_id,
                                const std::string& reason,
                                const std::vector<std::string>& ids) {
        std::vector<TuningExperimentResult> selected;
        for (const auto& result : results) {
            for (const auto& id : ids) {
                if (result.experiment_id == id) {
                    selected.push_back(result);
                    break;
                }
            }
        }
        if (!selected.empty()) {
            table.push_back({stage_name, best_id, reason, selected});
        }
    };

    select_by_prefix(
        "yolo_shape_stage",
        "yolo_shape_512",
        "Pick the largest stable input size before the latency budget becomes tight.",
        {"yolo_shape_320", "yolo_shape_512", "yolo_shape_640"});

    select_by_prefix(
        "yolo_schedule_stage",
        "yolo_schedule_lr1e3_b16",
        "Prefer the schedule that improves mAP without widening train/val drift.",
        {"yolo_schedule_lr1e3_b16", "yolo_schedule_lr1e4_b32"});

    select_by_prefix(
        "yolo_loss_stage",
        "yolo_loss_default",
        "Keep the default loss weighting when heavier box emphasis does not improve the tradeoff.",
        {"yolo_loss_default", "yolo_loss_heavier_box"});

    select_by_prefix(
        "mobilevit_input_stage",
        "mobilevit_input_256",
        "Use the ROI size that preserves context without destabilizing classification.",
        {"mobilevit_input_224", "mobilevit_input_256"});

    select_by_prefix(
        "mobilevit_regularization_stage",
        "mobilevit_reg_patch2_drop01",
        "Choose the smallest regularization change that improves repeatability.",
        {"mobilevit_reg_patch2_drop0", "mobilevit_reg_patch2_drop01"});

    select_by_prefix(
        "mobilevit_capacity_stage",
        "mobilevit_cap_640_b32",
        "Prefer the smaller embedding when the accuracy gain of a larger one is marginal.",
        {"mobilevit_cap_640_b32", "mobilevit_cap_768_b16"});

    return table;
}

#endif // TORCH_TUNING_PROFILES_H
