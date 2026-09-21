#ifndef TORCH_TEST_HOST_H
#define TORCH_TEST_HOST_H

#include <chrono>
#include <sstream>
#include <string>
#include <vector>

#include "torch_alltest.h"
#include "torch_alltest_2.h"

enum class TorchTestProfile {
    PreprocessContract,
    PostprocessContract,
    FullDataset,
    FullImage,
    FullTrain,
    FullAll
};

enum class TorchTaskLayer {
    Smoke,
    Feature,
    Scenario,
    Train,
    Infer
};

struct TorchCheckResult {
    const char* name = "";
    bool passed = false;
};

struct TorchTaskSpec {
    const char* task_id = "";
    TorchTaskLayer layer = TorchTaskLayer::Smoke;
    TorchTestProfile profile = TorchTestProfile::FullAll;
    const char* summary = "";
    const char* geometry_input_prior = "";
    const char* geometry_label_align = "";
    const char* attach_back_result = "";
    std::vector<const char*> inputs;
    std::vector<const char*> outputs;
    std::vector<const char*> dependencies;
    std::vector<const char*> script_params;
    std::vector<const char*> flow_steps;
    std::vector<const char*> check_points;
    const char* conclusion_pass = "";
    const char* conclusion_attach = "";
    const char* contract_entry = "";
    std::vector<const char*> bridge_dependencies;
    std::vector<const char*> blocking_points;
    const char* validation_sequence = "";
    const char* cxcore_takeover_level = "";
    const char* integration_status = "";
    std::vector<const char*> integration_gaps;
    const char* intake_priority = "";
    const char* required_label_contract = "";
    const char* next_minimal_uplift = "";
    std::vector<const char*> required_label_fields;
    std::vector<const char*> required_label_checks;
    const char* required_input_contract = "";
    std::vector<const char*> required_input_fields;
    std::vector<const char*> required_input_checks;
    std::vector<const char*> handoff_types;
    std::vector<const char*> downstream_threads;
    bool mlpack_feature_prepare_source = false;

    TorchTaskSpec() = default;

    TorchTaskSpec(
        const char* task_id_,
        TorchTaskLayer layer_,
        TorchTestProfile profile_,
        const char* summary_,
        std::vector<const char*> inputs_,
        std::vector<const char*> outputs_,
        std::vector<const char*> dependencies_,
        std::vector<const char*> script_params_)
        : task_id(task_id_),
          layer(layer_),
          profile(profile_),
          summary(summary_),
          inputs(std::move(inputs_)),
          outputs(std::move(outputs_)),
          dependencies(std::move(dependencies_)),
          script_params(std::move(script_params_)) {}
};

struct TorchStageReport {
    TorchTestProfile profile = TorchTestProfile::FullAll;
    int failures = 0;
    bool passed = false;
    const char* summary = "";
    std::string selected_task_id;
    std::string requested_device = "auto";
    std::string actual_device = "cpu";
    long long runtime_ms = 0;
    std::vector<TorchCheckResult> checks;
};

struct TorchGeometryHandoff {
    std::string source_hash;
    std::string result_ref;
    std::string evidence_ref;
    std::string log_path;
    std::string model_version;
    std::string bbox_ref;
    std::string mask_ref;
    std::string roi_ref;
    std::string region_ref;
    std::string contour_ref;
    std::string geometry_ref;
    std::string measurement_ref;
    std::string bbox_candidate_list_ref;
    std::string next_action;

    std::string describe() const {
        std::ostringstream os;
        os << "source_hash=" << source_hash
           << " result_ref=" << result_ref
           << " evidence_ref=" << evidence_ref
           << " bbox_ref=" << bbox_ref
           << " mask_ref=" << mask_ref
           << " roi_ref=" << roi_ref
           << " region_ref=" << region_ref
           << " contour_ref=" << contour_ref
           << " geometry_ref=" << geometry_ref
           << " measurement_ref=" << measurement_ref
           << " bbox_candidate_list_ref=" << bbox_candidate_list_ref
           << " model_version=" << model_version
           << " next_action=" << next_action;
        return os.str();
    }
};

struct TorchFeatureSemanticHandoff {
    std::string source_hash;
    std::string result_ref;
    std::string evidence_ref;
    std::string log_path;
    std::string model_version;
    std::string roi_ref;
    std::string roi_stats_ref;
    std::string geometry_ref;
    std::string embedding_ref;
    std::string feature_vector_ref;
    std::string feature_set_ref;
    int feature_dim = 0;
    std::string top1_class_ref;
    std::string class_confidence_ref;
    std::string template_alignment_ref;
    std::string template_test_alignment_status;
    std::string roi_diff_candidate_ref;
    std::string roi_diff_candidate_count;
    std::string prior_roi_region_ref;
    std::string roi_crop_packet_ref;
    std::string roi_crop_count;
    std::string roi_crop_spatial_size;
    std::string roi_crop_policy_ref;
    double confidence = 0.0;
    std::string next_action;

    std::string describe() const {
        std::ostringstream os;
        os << "source_hash=" << source_hash
           << " result_ref=" << result_ref
           << " evidence_ref=" << evidence_ref
           << " roi_ref=" << roi_ref
           << " roi_stats_ref=" << roi_stats_ref
           << " geometry_ref=" << geometry_ref
           << " embedding_ref=" << embedding_ref
           << " feature_vector_ref=" << feature_vector_ref
           << " feature_set_ref=" << feature_set_ref
           << " feature_dim=" << feature_dim
           << " top1_class_ref=" << top1_class_ref
           << " class_confidence_ref=" << class_confidence_ref
           << " template_alignment_ref=" << template_alignment_ref
           << " template_test_alignment_status=" << template_test_alignment_status
           << " roi_diff_candidate_ref=" << roi_diff_candidate_ref
           << " roi_diff_candidate_count=" << roi_diff_candidate_count
           << " prior_roi_region_ref=" << prior_roi_region_ref
           << " roi_crop_packet_ref=" << roi_crop_packet_ref
           << " roi_crop_count=" << roi_crop_count
           << " roi_crop_spatial_size=" << roi_crop_spatial_size
           << " roi_crop_policy_ref=" << roi_crop_policy_ref
           << " confidence=" << confidence
           << " model_version=" << model_version
           << " next_action=" << next_action;
        return os.str();
    }
};

struct TorchOptimizationHandoff {
    std::string source_hash;
    std::string result_ref;
    std::string evidence_ref;
    std::string log_path;
    std::string model_version;
    std::string geometry_ref;
    std::string objective_ref;
    std::string threshold_ref;
    std::string crop_policy_ref;
    std::string boundary_error_ref;
    std::string alignment_error_ref;
    std::string optimization_result_ref;
    std::string next_action;

    std::string describe() const {
        std::ostringstream os;
        os << "source_hash=" << source_hash
           << " result_ref=" << result_ref
           << " evidence_ref=" << evidence_ref
           << " geometry_ref=" << geometry_ref
           << " objective_ref=" << objective_ref
           << " threshold_ref=" << threshold_ref
           << " crop_policy_ref=" << crop_policy_ref
           << " boundary_error_ref=" << boundary_error_ref
           << " alignment_error_ref=" << alignment_error_ref
           << " optimization_result_ref=" << optimization_result_ref
           << " model_version=" << model_version
           << " next_action=" << next_action;
        return os.str();
    }
};

inline std::string torch_make_handoff_ref(const char* task_id, const char* suffix) {
    std::string value = task_id != nullptr ? task_id : "torch.unknown";
    for (char& ch : value) {
        if (ch == '.') {
            ch = '_';
        }
    }
    value += ".";
    value += suffix;
    return value;
}

inline bool torch_task_id_contains(const char* task_id, const char* token) {
    if (task_id == nullptr || token == nullptr) {
        return false;
    }
    return std::string(task_id).find(token) != std::string::npos;
}

class TorchTestHost {
public:
    int run_core() const {
        return run_core_tests();
    }

    int run_mobilevit() const {
        return run_mobilevit_tests();
    }

    int run_preprocess_contract() const {
        return run_preprocess_contract_tests();
    }

    int run_postprocess_contract() const {
        return run_postprocess_contract_tests();
    }

    int run_full_dataset() const {
        return run_core() + run_mobilevit();
    }

    int run_full_image() const {
        return run_core() + run_mobilevit();
    }

    int run_full_train() const {
        return run_core() + run_mobilevit();
    }

    int run_all() const {
        return run_core() + run_mobilevit();
    }

    TorchStageReport run_profile_report(TorchTestProfile profile) const {
        TorchStageReport report;
        report.profile = profile;
        report.failures = run_profile(profile);
        report.passed = (report.failures == 0);
        report.summary = summary_for(profile, report.passed);
        report.checks = checks_for(profile, report.passed);
        return report;
    }

    TorchStageReport run_preprocess_report() const {
        return run_profile_report(TorchTestProfile::PreprocessContract);
    }

    TorchStageReport run_postprocess_report() const {
        return run_profile_report(TorchTestProfile::PostprocessContract);
    }

    TorchStageReport run_full_dataset_report() const {
        return run_profile_report(TorchTestProfile::FullDataset);
    }

    TorchStageReport run_full_image_report() const {
        return run_profile_report(TorchTestProfile::FullImage);
    }

    TorchStageReport run_full_train_report() const {
        return run_profile_report(TorchTestProfile::FullTrain);
    }

    TorchStageReport run_current_profile_report() const {
        return run_profile_report(current_profile());
    }

    TorchStageReport run_task_report(const std::string& task_id,
                                     const std::string& requested_device = "auto") const {
        const auto* spec = find_task_spec(task_id);
        TORCH_CHECK(spec != nullptr, "Unknown torch task: ", task_id);

        TorchStageReport report;
        const auto begin_time = std::chrono::steady_clock::now();
        report.profile = spec->profile;
        report.failures = run_task(task_id);
        const auto end_time = std::chrono::steady_clock::now();
        report.passed = (report.failures == 0);
        report.summary = task_summary_for(*spec, report.passed);
        report.selected_task_id = spec->task_id;
        report.requested_device = requested_device;
        report.actual_device = materialize_device_mode(requested_device);
        report.runtime_ms = static_cast<long long>(
            std::chrono::duration_cast<std::chrono::milliseconds>(end_time - begin_time).count());
        report.checks = task_checks_for(*spec, report.passed);
        return report;
    }

    static std::string materialize_device_mode(const std::string& requested_device) {
        const bool cuda_available = torch::cuda::is_available();
        if (requested_device == "cpu") {
            return "cpu";
        }
        if (requested_device == "gpu") {
            return cuda_available ? "gpu" : "cpu";
        }
        return cuda_available ? "gpu" : "cpu";
    }

    int run_task(const std::string& task_id) const {
        if (task_id == "torch.smoke.yolo.cpu") {
            return test_YoloSmokeRuntimeConfig();
        }
        if (task_id == "torch.smoke.yolo.gpu") {
            return test_YoloGpuSmoke();
        }
        if (task_id == "torch.smoke.mobilevit") {
            return test_MobileViTv2_Train();
        }
        if (task_id == "torch.smoke.mobilenetv3") {
            return test_Inspect_Load();
        }
        if (task_id == "torch.smoke.deeplab") {
            return test_Segmentation_MainlineSession();
        }
        if (task_id == "torch.smoke.resnet18") {
            return test_ResNet_Load();
        }
        if (task_id == "torch.smoke.resnet50") {
            return test_ResNet_Load();
        }
        if (task_id == "torch.feature.yolo.eval") {
            return test_YoloEvalSummaryContract();
        }
        if (task_id == "torch.feature.mobilevit.session") {
            return test_MobileViTv2_MainlineSession();
        }
        if (task_id == "torch.feature.mobilenetv3.baseline") {
            return test_Inspect_Load();
        }
        if (task_id == "torch.feature.deeplab.contract") {
            return test_Segmentation_MainlineSession();
        }
        if (task_id == "torch.feature.resnet18.baseline") {
            return test_ResNet_Load();
        }
        if (task_id == "torch.feature.resnet50.baseline") {
            return test_ResNet_Load();
        }
        if (task_id == "torch.feature.segmentation.contract") {
            return test_Segmentation_MainlineSession();
        }
        if (task_id == "torch.scenario.yolo_mobilevit.infer") {
            return test_YOLOv8_MobileViTv2_TwoStageInference();
        }
        if (task_id == "torch.scenario.yolo_mobilevit.train_mock") {
            return test_YOLOv8_MobileViTv2_TwoStageTrainMock();
        }
        if (task_id == "torch.train.yolo.mainline") {
            return test_YoloTrainerSession();
        }
        if (task_id == "torch.train.mobilevit.mainline") {
            return test_MobileViTv2_MainlineSession();
        }
        if (task_id == "torch.train.deeplab.mainline") {
            return test_Segmentation_MainlineSession();
        }
        if (task_id == "torch.train.mobilenetv3_deeplab.mainline") {
            return test_Segmentation_MainlineSession();
        }
        if (task_id == "torch.infer.yolo.unified") {
            return test_YoloEvalSummaryContract();
        }
        if (task_id == "torch.infer.mobilevit.unified") {
            return test_MobileViTv2_UnifiedInferReview();
        }
        if (task_id == "torch.infer.mobilenetv3.baseline") {
            return test_Inspect_Load();
        }
        if (task_id == "torch.infer.resnet18.baseline") {
            return test_ResNet_Load();
        }
        if (task_id == "torch.infer.resnet50.baseline") {
            return test_ResNet_Load();
        }
        if (task_id == "torch.infer.deeplab.unified") {
            return test_Segmentation_UnifiedInferReview();
        }
        if (task_id == "torch.infer.mobilenetv3_deeplab.unified") {
            return test_Segmentation_MainlineSession();
        }

        TORCH_CHECK(false, "Unknown torch task execution binding: ", task_id);
    }

    int run_profile(TorchTestProfile profile) const {
        switch (profile) {
            case TorchTestProfile::PreprocessContract:
                return run_preprocess_contract();
            case TorchTestProfile::PostprocessContract:
                return run_postprocess_contract();
            case TorchTestProfile::FullDataset:
                return run_full_dataset();
            case TorchTestProfile::FullImage:
                return run_full_image();
            case TorchTestProfile::FullTrain:
                return run_full_train();
            case TorchTestProfile::FullAll:
            default:
                return run_all();
        }
    }

    static const char* summary_for(TorchTestProfile profile, bool passed) {
        switch (profile) {
            case TorchTestProfile::PreprocessContract:
                return passed ? "preprocess contract checks passed" : "preprocess contract checks failed";
            case TorchTestProfile::PostprocessContract:
                return passed ? "postprocess contract checks passed" : "postprocess contract checks failed";
            case TorchTestProfile::FullDataset:
                return passed ? "full-dataset stage passed" : "full-dataset stage failed";
            case TorchTestProfile::FullImage:
                return passed ? "full-image stage passed" : "full-image stage failed";
            case TorchTestProfile::FullTrain:
                return passed ? "full-train stage passed" : "full-train stage failed";
            case TorchTestProfile::FullAll:
            default:
                return passed ? "full validation passed" : "full validation failed";
        }
    }

    static std::vector<TorchCheckResult> checks_for(TorchTestProfile profile, bool passed) {
        switch (profile) {
            case TorchTestProfile::PreprocessContract:
                return {
                    {"opencv_image_stage", passed},
                    {"opencv_tensor_contract", passed},
                    {"opencv_annotation_sync", passed},
                    {"yolo_dataset_contract", passed},
                };
            case TorchTestProfile::PostprocessContract:
                return {
                    {"yolov8_loss_contract", passed},
                    {"yolov8_build_config", passed},
                };
            case TorchTestProfile::FullDataset:
                return {
                    {"backend_routing_contract", passed},
                    {"core_dataset_stage", passed},
                    {"mobilevit_dataset_stage", passed},
                };
            case TorchTestProfile::FullImage:
                return {
                    {"torch_infer_io_contract", passed},
                    {"core_image_stage", passed},
                    {"mobilevit_two_stage_infer", passed},
                };
            case TorchTestProfile::FullTrain:
                return {
                    {"torch_train_io_contract", passed},
                    {"core_train_stage", passed},
                    {"mobilevit_train_stage", passed},
                };
            case TorchTestProfile::FullAll:
            default:
                return {
                    {"core_full_validation", passed},
                    {"mobilevit_full_validation", passed},
                };
        }
    }

    static const char* task_summary_for(const TorchTaskSpec& spec, bool passed) {
        if (passed) {
            return spec.conclusion_pass != nullptr && spec.conclusion_pass[0] != '\0'
                ? spec.conclusion_pass
                : "torch task passed";
        }
        return "torch task failed";
    }

    static std::vector<TorchCheckResult> task_checks_for(const TorchTaskSpec& spec, bool passed) {
        std::vector<TorchCheckResult> checks;
        if (spec.check_points.empty()) {
            checks.push_back({"task_contract", passed});
            return checks;
        }

        checks.reserve(spec.check_points.size());
        for (const auto* check : spec.check_points) {
            checks.push_back({check != nullptr ? check : "task_check", passed});
        }
        return checks;
    }

    static const char* profile_name(TorchTestProfile profile) {
        switch (profile) {
            case TorchTestProfile::PreprocessContract:
                return "preprocess-contract";
            case TorchTestProfile::PostprocessContract:
                return "postprocess-contract";
            case TorchTestProfile::FullDataset:
                return "full-dataset";
            case TorchTestProfile::FullImage:
                return "full-image";
            case TorchTestProfile::FullTrain:
                return "full-train";
            case TorchTestProfile::FullAll:
            default:
                return "full-all";
        }
    }

    static const char* profile_stage_id(TorchTestProfile profile) {
        switch (profile) {
            case TorchTestProfile::PreprocessContract:
                return "torch.preprocess.contract";
            case TorchTestProfile::PostprocessContract:
                return "torch.postprocess.contract";
            case TorchTestProfile::FullDataset:
                return "torch.full.dataset";
            case TorchTestProfile::FullImage:
                return "torch.full.image";
            case TorchTestProfile::FullTrain:
                return "torch.full.train";
            case TorchTestProfile::FullAll:
            default:
                return "torch.full.all";
        }
    }

    static bool try_parse_profile_name(const std::string& value, TorchTestProfile& profile) {
        if (value == "preprocess-contract") {
            profile = TorchTestProfile::PreprocessContract;
            return true;
        }
        if (value == "postprocess-contract") {
            profile = TorchTestProfile::PostprocessContract;
            return true;
        }
        if (value == "full-dataset") {
            profile = TorchTestProfile::FullDataset;
            return true;
        }
        if (value == "full-image") {
            profile = TorchTestProfile::FullImage;
            return true;
        }
        if (value == "full-train") {
            profile = TorchTestProfile::FullTrain;
            return true;
        }
        if (value == "full-all") {
            profile = TorchTestProfile::FullAll;
            return true;
        }
        return false;
    }

    static const TorchTaskSpec* find_task_spec(const std::string& task_id) {
        static const auto specs = task_specs();
        for (const auto& spec : specs) {
            if (task_id == spec.task_id) {
                return &spec;
            }
        }
        return nullptr;
    }

    static TorchTestProfile profile_for_task(const std::string& task_id) {
        const auto* spec = find_task_spec(task_id);
        TORCH_CHECK(spec != nullptr, "Unknown torch task: ", task_id);
        return spec->profile;
    }

    static bool try_normalize_device_mode(const std::string& value, std::string& normalized) {
        if (value == "cpu") {
            normalized = "cpu";
            return true;
        }
        if (value == "gpu" || value == "cuda") {
            normalized = "gpu";
            return true;
        }
        if (value == "auto") {
            normalized = "auto";
            return true;
        }
        return false;
    }

    static std::string device_contract_for_task(const TorchTaskSpec& spec) {
        for (const auto* param : spec.script_params) {
            if (std::string(param) == "--device=cpu") {
                return "cpu_only";
            }
            if (std::string(param) == "--device=cuda") {
                return "gpu_only";
            }
        }
        return "cpu_gpu_selectable";
    }

    static std::vector<std::string> format_task_binding_lines(const TorchTaskSpec& spec) {
        return {
            "task_id=" + std::string(spec.task_id),
            "task_layer=" + std::string(layer_name(spec.layer)),
            "task_profile=" + std::string(profile_name(spec.profile)),
            "task_stage_id=" + std::string(profile_stage_id(spec.profile)),
            "task_device_contract=" + device_contract_for_task(spec),
        };
    }

    static const char* layer_name(TorchTaskLayer layer) {
        switch (layer) {
            case TorchTaskLayer::Smoke:
                return "smoke";
            case TorchTaskLayer::Feature:
                return "feature";
            case TorchTaskLayer::Scenario:
                return "scenario";
            case TorchTaskLayer::Train:
                return "train";
            case TorchTaskLayer::Infer:
                return "infer";
            default:
                return "unknown";
        }
    }

    static std::vector<TorchTaskSpec> task_specs() {
        auto specs = std::vector<TorchTaskSpec>{
            {
                "torch.smoke.yolo.cpu",
                TorchTaskLayer::Smoke,
                TorchTestProfile::FullTrain,
                "Prepare detection tensors, run YOLO CPU smoke train gate, and emit a smoke report for bbox candidate attach.",
                {"ModelConfig", "YoloModelBuildConfig", "random_bchw_tensor", "random_detection_targets"},
                {"finite_loss", "gradients_defined", "smoke_report"},
                {"libtorch", "torch_full_train_target"},
                {"--layer=smoke", "--task=torch.smoke.yolo.cpu", "--device=cpu", "--model=yolo", "--smoke-batches", "--input-size"}
            },
            {
                "torch.smoke.yolo.gpu",
                TorchTaskLayer::Smoke,
                TorchTestProfile::FullTrain,
                "Prepare detection tensors on CUDA, run YOLO GPU smoke gate, and verify bbox candidate generation on device.",
                {"ModelConfig", "YoloModelBuildConfig", "random_bchw_tensor", "random_detection_targets"},
                {"cuda_device", "finite_loss", "gpu_smoke_pass"},
                {"libtorch", "cuda_runtime", "torch_full_train_target"},
                {"--layer=smoke", "--task=torch.smoke.yolo.gpu", "--device=cuda", "--model=yolo", "--smoke-batches", "--input-size"}
            },
            {
                "torch.smoke.mobilevit",
                TorchTaskLayer::Smoke,
                TorchTestProfile::FullTrain,
                "Prepare ROI classification tensors, run MobileViT smoke train gate, and emit trainer lifecycle output for ROI attach.",
                {"runner_config", "train_bchw_tensor", "train_labels"},
                {"finite_loss", "gradients_defined", "trainer_lifecycle_summary"},
                {"libtorch", "torch_full_train_target"},
                {"--layer=smoke", "--task=torch.smoke.mobilevit", "--device=cpu", "--model=mobilevit", "--batch-size", "--input-size"}
            },
            {
                "torch.smoke.mobilenetv3",
                TorchTaskLayer::Smoke,
                TorchTestProfile::FullAll,
                "Prepare local structure patches, run MobileNetV3 backbone features, and verify low/high feature attach hints.",
                {"random_bchw_tensor"},
                {"low_level_feature_shape", "high_level_feature_shape"},
                {"libtorch", "torch_minimal_target"},
                {"--layer=smoke", "--task=torch.smoke.mobilenetv3", "--device=cpu", "--model=mobilenetv3"}
            },
            {
                "torch.smoke.deeplab",
                TorchTaskLayer::Smoke,
                TorchTestProfile::FullAll,
                "Prepare segmentation tensors, run DeepLab forward contracts, and verify mask-region attach outputs.",
                {"random_bchw_tensor", "segmentation_profile"},
                {"segmentation_output_shape", "finite_outputs"},
                {"libtorch", "torch_minimal_target"},
                {"--layer=smoke", "--task=torch.smoke.deeplab", "--device=cpu", "--model=deeplab"}
            },
            {
                "torch.smoke.resnet18",
                TorchTaskLayer::Smoke,
                TorchTestProfile::FullAll,
                "Prepare baseline patches, run ResNet18 classifier and feature maps, and emit baseline attach references.",
                {"random_bchw_tensor"},
                {"classifier_output_shape", "p3_p4_p5_feature_shapes"},
                {"libtorch", "torch_minimal_target"},
                {"--layer=smoke", "--task=torch.smoke.resnet18", "--device=cpu", "--model=resnet18"}
            },
            {
                "torch.smoke.resnet50",
                TorchTaskLayer::Smoke,
                TorchTestProfile::FullAll,
                "Prepare baseline patches, run ResNet50 classifier and feature maps, and emit baseline attach references.",
                {"random_bchw_tensor"},
                {"classifier_output_shape", "p3_p4_p5_feature_shapes"},
                {"libtorch", "torch_minimal_target"},
                {"--layer=smoke", "--task=torch.smoke.resnet50", "--device=cpu", "--model=resnet50"}
            },
            {
                "torch.feature.yolo.eval",
                TorchTaskLayer::Feature,
                TorchTestProfile::FullDataset,
                "Load validation images and labels, run YOLO eval summary, and emit per-class bbox metrics for geometry attach.",
                {"data_root", "val_images", "val_labels", "ModelConfig", "YoloValidationConfig"},
                {"tp_fp_fn", "precision_recall_f1", "per_class_metrics", "eval_report"},
                {"opencv", "dataset_layout", "torch_full_dataset_target"},
                {"--layer=feature", "--task=torch.feature.yolo.eval", "--device", "--data-root", "--val-images", "--val-labels", "--use-dfl"}
            },
            {
                "torch.feature.mobilevit.session",
                TorchTaskLayer::Feature,
                TorchTestProfile::FullTrain,
                "Load ROI patches and labels, run MobileViT session and trainer lifecycle, and emit ROI reclass feature output.",
                {"runner_config", "train_bchw_tensor", "train_labels", "eval_bchw_tensor", "eval_labels"},
                {"session_summary", "trainer_lifecycle_summary", "trainer_flat_run"},
                {"libtorch", "torch_full_train_target"},
                {"--layer=feature", "--task=torch.feature.mobilevit.session", "--device", "--model=mobilevit", "--batch-size", "--input-size"}
            },
            {
                "torch.feature.mobilenetv3.baseline",
                TorchTaskLayer::Feature,
                TorchTestProfile::FullAll,
                "Load local structure patches, run MobileNetV3 backbone features, and emit feature alignment hints.",
                {"random_bchw_tensor", "lightweight_patch_or_foreground_hint"},
                {"low_level_feature_shape", "high_level_feature_shape", "feature_alignment_hint"},
                {"libtorch", "torch_minimal_target"},
                {"--layer=feature", "--task=torch.feature.mobilenetv3.baseline", "--device=cpu", "--model=mobilenetv3"}
            },
            {
                "torch.feature.deeplab.contract",
                TorchTaskLayer::Feature,
                TorchTestProfile::FullAll,
                "Load segmentation tensors, run DeepLab decoder feature contracts, and emit mask-region attach references.",
                {"random_bchw_tensor", "mask_or_region_label"},
                {"segmentation_output_shape", "finite_outputs", "mask_region_boundary_attach"},
                {"libtorch", "torch_minimal_target"},
                {"--layer=feature", "--task=torch.feature.deeplab.contract", "--device=cpu", "--model=deeplab"}
            },
            {
                "torch.feature.resnet18.baseline",
                TorchTaskLayer::Feature,
                TorchTestProfile::FullAll,
                "Load baseline patches, run ResNet18 classifier and feature pyramid, and emit baseline feature references.",
                {"random_bchw_tensor", "patch_level_class_label"},
                {"classifier_output_shape", "p3_p4_p5_feature_shapes", "baseline_feature_reference"},
                {"libtorch", "torch_minimal_target"},
                {"--layer=feature", "--task=torch.feature.resnet18.baseline", "--device=cpu", "--model=resnet18"}
            },
            {
                "torch.feature.resnet50.baseline",
                TorchTaskLayer::Feature,
                TorchTestProfile::FullAll,
                "Load baseline patches, run ResNet50 classifier and feature pyramid, and emit baseline feature references.",
                {"random_bchw_tensor", "patch_level_class_label"},
                {"classifier_output_shape", "p3_p4_p5_feature_shapes", "baseline_feature_reference"},
                {"libtorch", "torch_minimal_target"},
                {"--layer=feature", "--task=torch.feature.resnet50.baseline", "--device=cpu", "--model=resnet50"}
            },
            {
                "torch.scenario.yolo_mobilevit.infer",
                TorchTaskLayer::Scenario,
                TorchTestProfile::FullImage,
                "Load a scene image, run YOLO detection plus MobileViT ROI reclass, and emit two-stage attach outputs.",
                {"image_tensor", "ModelConfig", "MobileViT_runner_config"},
                {"detector_output", "classifier_output", "scenario_pass"},
                {"libtorch", "opencv", "torch_full_image_target"},
                {"--layer=scenario", "--task=torch.scenario.yolo_mobilevit.infer", "--device", "--model=yolo", "--profile=full-image"}
            },
            {
                "torch.scenario.yolo_mobilevit.train_mock",
                TorchTaskLayer::Scenario,
                TorchTestProfile::FullTrain,
                "Load scene batches and ROI labels, run YOLO plus MobileViT train mock, and emit ROI training feedback.",
                {"image_tensor", "classification_labels", "ModelConfig", "MobileViT_runner_config"},
                {"finite_classification_loss", "scenario_pass"},
                {"libtorch", "opencv", "torch_full_train_target"},
                {"--layer=scenario", "--task=torch.scenario.yolo_mobilevit.train_mock", "--device", "--model=yolo", "--profile=full-train"}
            },
            {
                "torch.train.yolo.mainline",
                TorchTaskLayer::Train,
                TorchTestProfile::FullTrain,
                "Load detection data, run YOLO train and eval gates, and emit unified lifecycle and bbox attach outputs.",
                {"runner_config", "train_inputs", "eval_data_root"},
                {"trainer_lifecycle_summary", "unified_mainline_bundle", "unified_mainline_summary"},
                {"libtorch", "opencv", "dataset_layout", "torch_full_train_target"},
                {"--layer=train", "--task=torch.train.yolo.mainline", "--device", "--data-root", "--profile=full-train", "--smoke-batches", "--resize-policy", "--use-dfl"}
            },
            {
                "torch.train.mobilevit.mainline",
                TorchTaskLayer::Train,
                TorchTestProfile::FullTrain,
                "Load ROI train and eval tensors, run MobileViT mainline and trainer gates, and emit unified ROI attach outputs.",
                {"runner_config", "train_bchw_tensor", "train_labels", "eval_bchw_tensor", "eval_labels"},
                {"trainer_lifecycle_summary", "unified_mainline_bundle", "unified_mainline_summary"},
                {"libtorch", "torch_full_train_target"},
                {"--layer=train", "--task=torch.train.mobilevit.mainline", "--device", "--model=mobilevit", "--batch-size", "--input-size", "--profile=full-train"}
            },
            {
                "torch.train.deeplab.mainline",
                TorchTaskLayer::Train,
                TorchTestProfile::FullAll,
                "Load segmentation tensors and masks, run DeepLab train gates, and emit unified mask-region attach outputs.",
                {"segmentation_profile", "random_bchw_tensor", "segmentation_target"},
                {"trainer_lifecycle_summary", "segmentation_trainer_flat_run", "segmentation_unified_summary"},
                {"libtorch", "torch_minimal_target"},
                {"--layer=train", "--task=torch.train.deeplab.mainline", "--device=cpu", "--model=deeplab"}
            },
            {
                "torch.train.mobilenetv3_deeplab.mainline",
                TorchTaskLayer::Train,
                TorchTestProfile::FullAll,
                "Load local structure tensors and masks, run MobileNetV3 plus DeepLabV3Plus train gates, and emit unified foreground attach outputs.",
                {"segmentation_profile", "random_bchw_tensor", "segmentation_target"},
                {"low_high_feature_contract", "segmentation_trainer_flat_run", "segmentation_unified_summary"},
                {"libtorch", "torch_minimal_target"},
                {"--layer=train", "--task=torch.train.mobilenetv3_deeplab.mainline", "--device=cpu", "--model=mobilenetv3_deeplabv3plus"}
            },
            {
                "torch.infer.yolo.unified",
                TorchTaskLayer::Infer,
                TorchTestProfile::FullTrain,
                "Load validation images and labels, run YOLO infer and eval gates, and emit unified bbox attach outputs.",
                {"val_images", "val_labels", "eval_config"},
                {"eval_report", "unified_mainline_bundle", "unified_mainline_summary"},
                {"libtorch", "opencv", "dataset_layout", "torch_full_train_target"},
                {"--layer=infer", "--task=torch.infer.yolo.unified", "--device", "--val-images", "--val-labels", "--profile=full-train"}
            },
            {
                "torch.infer.mobilevit.unified",
                TorchTaskLayer::Infer,
                TorchTestProfile::FullTrain,
                "Load ROI eval tensors and labels, run MobileViT infer gates, and emit unified ROI attach outputs.",
                {"eval_bchw_tensor", "eval_labels", "runner_config"},
                {"session_summary", "unified_mainline_bundle", "unified_mainline_summary"},
                {"libtorch", "torch_full_train_target"},
                {"--layer=infer", "--task=torch.infer.mobilevit.unified", "--device", "--model=mobilevit", "--batch-size", "--input-size", "--profile=full-train"}
            },
            {
                "torch.infer.mobilenetv3.baseline",
                TorchTaskLayer::Infer,
                TorchTestProfile::FullAll,
                "Load local structure patches, run MobileNetV3 baseline feature path, and emit feature attach references.",
                {"random_bchw_tensor", "lightweight_patch_or_foreground_hint"},
                {"low_level_feature_shape", "high_level_feature_shape", "baseline_feature_reference"},
                {"libtorch", "torch_minimal_target"},
                {"--layer=infer", "--task=torch.infer.mobilenetv3.baseline", "--device=cpu", "--model=mobilenetv3"}
            },
            {
                "torch.infer.resnet18.baseline",
                TorchTaskLayer::Infer,
                TorchTestProfile::FullAll,
                "Load aligned patches, run ResNet18 baseline classifier path, and emit baseline class attach outputs.",
                {"random_bchw_tensor", "patch_level_class_label"},
                {"classifier_output_shape", "baseline_class_reference"},
                {"libtorch", "torch_minimal_target"},
                {"--layer=infer", "--task=torch.infer.resnet18.baseline", "--device=cpu", "--model=resnet18"}
            },
            {
                "torch.infer.resnet50.baseline",
                TorchTaskLayer::Infer,
                TorchTestProfile::FullAll,
                "Load aligned patches, run ResNet50 baseline classifier path, and emit baseline class attach outputs.",
                {"random_bchw_tensor", "patch_level_class_label"},
                {"classifier_output_shape", "baseline_class_reference"},
                {"libtorch", "torch_minimal_target"},
                {"--layer=infer", "--task=torch.infer.resnet50.baseline", "--device=cpu", "--model=resnet50"}
            },
            {
                "torch.infer.deeplab.unified",
                TorchTaskLayer::Infer,
                TorchTestProfile::FullAll,
                "Load segmentation tensors, run DeepLab infer and eval gates, and emit unified mask-region attach outputs.",
                {"random_bchw_tensor", "segmentation_profile"},
                {"mask_output_shape", "segmentation_eval_summary", "segmentation_unified_summary"},
                {"libtorch", "torch_minimal_target"},
                {"--layer=infer", "--task=torch.infer.deeplab.unified", "--device=cpu", "--model=deeplab"}
            },
            {
                "torch.infer.mobilenetv3_deeplab.unified",
                TorchTaskLayer::Infer,
                TorchTestProfile::FullAll,
                "Load local structure tensors, run MobileNetV3 plus DeepLabV3Plus infer and eval gates, and emit unified foreground attach outputs.",
                {"random_bchw_tensor", "segmentation_profile"},
                {"mask_output_shape", "feature_to_decoder_alignment", "segmentation_unified_summary"},
                {"libtorch", "torch_minimal_target"},
                {"--layer=infer", "--task=torch.infer.mobilenetv3_deeplab.unified", "--device=cpu", "--model=mobilenetv3_deeplabv3plus"}
            },
            {
                "torch.feature.segmentation.contract",
                TorchTaskLayer::Feature,
                TorchTestProfile::FullAll,
                "Load segmentation tensors, run MobileNetV3 and DeepLab feature contracts, and verify feature-to-mask attach alignment.",
                {"random_bchw_tensor", "segmentation_profile"},
                {"feature_shapes", "segmentation_output_shape", "finite_outputs"},
                {"libtorch", "torch_minimal_target"},
                {"--layer=feature", "--task=torch.feature.segmentation.contract", "--device", "--model=mobilenetv3_deeplabv3plus"}
            }
        };

        auto set_flow = [](
            TorchTaskSpec& spec,
            const char* geometry_input_prior,
            const char* geometry_label_align,
            const char* attach_back_result,
            std::vector<const char*> flow_steps,
            std::vector<const char*> check_points,
            const char* conclusion_pass,
            const char* conclusion_attach) {
            spec.geometry_input_prior = geometry_input_prior;
            spec.geometry_label_align = geometry_label_align;
            spec.attach_back_result = attach_back_result;
            spec.flow_steps = std::move(flow_steps);
            spec.check_points = std::move(check_points);
            spec.conclusion_pass = conclusion_pass;
            spec.conclusion_attach = conclusion_attach;
        };

        for (auto& spec : specs) {
            const std::string id = spec.task_id;
            if (id == "torch.smoke.yolo.cpu") {
                set_flow(
                    spec,
                    "whole_image_or_search_window",
                    "bbox_plus_class_aligned_to_image",
                    "bbox_class_score_candidates",
                    {"prepare_detection_tensor_and_targets",
                     "run_yolo_cpu_smoke_gate",
                     "verify_finite_loss_and_gradients",
                     "collect_smoke_report"},
                    {"loss_must_be_finite",
                     "gradients_must_be_defined",
                     "smoke_report_must_exist"},
                    "yolo_cpu_smoke_ready",
                    "attach_bbox_candidates_back_to_geometry_layer");
            } else if (id == "torch.smoke.yolo.gpu") {
                set_flow(
                    spec,
                    "whole_image_or_search_window",
                    "bbox_plus_class_aligned_to_image",
                    "bbox_class_score_candidates",
                    {"prepare_detection_tensor_and_targets_on_cuda",
                     "run_yolo_gpu_smoke_gate",
                     "verify_cuda_device_loss_and_backward",
                     "collect_gpu_smoke_report"},
                    {"device_must_be_cuda",
                     "loss_must_be_finite",
                     "backward_must_succeed"},
                    "yolo_gpu_smoke_ready",
                    "attach_gpu_verified_bbox_candidates_back_to_geometry_layer");
            } else if (id == "torch.smoke.mobilevit") {
                set_flow(
                    spec,
                    "roi_patch_or_candidate_crop",
                    "roi_class_label",
                    "roi_reclass_score",
                    {"prepare_roi_classification_batch",
                     "run_mobilevit_smoke_gate",
                     "verify_trainer_lifecycle",
                     "collect_roi_smoke_report"},
                    {"classification_loss_must_be_finite",
                     "trainer_lifecycle_must_pass",
                     "flat_run_must_exist"},
                    "mobilevit_smoke_ready",
                    "attach_roi_reclass_result_back_to_geometry_layer");
            } else if (id == "torch.smoke.mobilenetv3") {
                set_flow(
                    spec,
                    "local_structure_patch",
                    "lightweight_patch_or_foreground_hint",
                    "feature_alignment_hint",
                    {"prepare_local_patch_tensor",
                     "run_backbone_forward_features",
                     "verify_low_high_feature_contract",
                     "collect_feature_report"},
                    {"low_level_feature_must_exist",
                     "high_level_feature_must_exist",
                     "channel_contract_must_match"},
                    "mobilenetv3_smoke_ready",
                    "attach_feature_alignment_hint_back_to_geometry_layer");
            } else if (id == "torch.smoke.deeplab") {
                set_flow(
                    spec,
                    "whole_region_or_boundary_sensitive_patch",
                    "mask_or_region_label",
                    "mask_region_boundary_attach",
                    {"prepare_segmentation_tensor",
                     "run_deeplab_forward_contracts",
                     "verify_output_shape_and_finite_values",
                     "collect_segmentation_smoke_report"},
                    {"segmentation_logits_must_match_input_size",
                     "outputs_must_be_finite",
                     "multi_backbone_decoder_path_must_exist"},
                    "deeplab_smoke_ready",
                    "attach_mask_and_boundary_result_back_to_geometry_layer");
            } else if (id == "torch.smoke.resnet18") {
                set_flow(
                    spec,
                    "aligned_patch_or_baseline_roi",
                    "patch_level_class_label",
                    "baseline_class_and_feature_reference",
                    {"prepare_baseline_patch_tensor",
                     "run_classifier_forward",
                     "run_forward_features_p3_p4_p5",
                     "collect_baseline_report"},
                    {"classifier_output_must_match_num_classes",
                     "feature_pyramid_shapes_must_match_contract"},
                    "resnet18_smoke_ready",
                    "attach_baseline_class_and_feature_reference_back_to_geometry_layer");
            } else if (id == "torch.smoke.resnet50") {
                set_flow(
                    spec,
                    "aligned_patch_or_baseline_roi",
                    "patch_level_class_label",
                    "baseline_class_and_feature_reference",
                    {"prepare_baseline_patch_tensor",
                     "run_classifier_forward",
                     "run_forward_features_p3_p4_p5",
                     "collect_baseline_report"},
                    {"classifier_output_must_match_num_classes",
                     "feature_pyramid_shapes_must_match_contract"},
                    "resnet50_smoke_ready",
                    "attach_baseline_class_and_feature_reference_back_to_geometry_layer");
            } else if (id == "torch.feature.yolo.eval") {
                set_flow(
                    spec,
                    "whole_image_or_search_window",
                    "bbox_plus_class_aligned_to_image",
                    "bbox_class_score_candidates",
                    {"load_validation_images_and_labels",
                     "run_yolo_eval_summary_contract",
                     "collect_tp_fp_fn_and_per_class_metrics",
                     "emit_feature_report_for_geometry_attach"},
                    {"tp_fp_fn_must_be_reported",
                     "per_class_metrics_must_exist",
                     "eval_summary_must_be_finite"},
                    "yolo_eval_feature_ready",
                    "attach_bbox_class_score_candidates_back_to_geometry_layer");
            } else if (id == "torch.feature.mobilevit.session") {
                set_flow(
                    spec,
                    "roi_patch_or_candidate_crop",
                    "roi_class_label",
                    "roi_reclass_score",
                    {"load_roi_patches_and_labels",
                     "run_mobilevit_mainline_session",
                     "collect_session_and_lifecycle_result",
                     "emit_feature_report_for_geometry_attach"},
                    {"session_must_pass",
                     "trainer_lifecycle_must_pass",
                     "classification_metrics_must_be_finite"},
                    "mobilevit_session_feature_ready",
                    "attach_roi_reclass_score_back_to_geometry_layer");
            } else if (id == "torch.feature.mobilenetv3.baseline") {
                set_flow(
                    spec,
                    "local_structure_patch",
                    "lightweight_patch_or_foreground_hint",
                    "feature_alignment_hint",
                    {"load_local_structure_patches",
                     "run_mobilenetv3_backbone_forward_features",
                     "verify_low_high_feature_contract",
                     "emit_feature_alignment_report"},
                    {"low_level_feature_must_exist",
                     "high_level_feature_must_exist",
                     "channel_contract_must_match"},
                    "mobilenetv3_feature_baseline_ready",
                    "attach_feature_alignment_hint_back_to_geometry_layer");
            } else if (id == "torch.feature.deeplab.contract") {
                set_flow(
                    spec,
                    "whole_region_or_boundary_sensitive_patch",
                    "mask_or_region_label",
                    "mask_region_boundary_attach",
                    {"load_segmentation_tensor",
                     "run_deeplab_forward_contracts",
                     "verify_decoder_output_and_mask_alignment",
                     "emit_feature_report_for_geometry_attach"},
                    {"segmentation_output_shape_must_match",
                     "outputs_must_be_finite",
                     "mask_region_attach_reference_must_exist"},
                    "deeplab_feature_contract_ready",
                    "attach_mask_region_boundary_back_to_geometry_layer");
            } else if (id == "torch.feature.resnet18.baseline") {
                set_flow(
                    spec,
                    "aligned_patch_or_baseline_roi",
                    "patch_level_class_label",
                    "baseline_class_and_feature_reference",
                    {"load_baseline_patches",
                     "run_resnet18_classifier_and_feature_pyramid",
                     "verify_classifier_and_feature_shapes",
                     "emit_baseline_feature_report"},
                    {"classifier_output_must_match_num_classes",
                     "feature_pyramid_shapes_must_match_contract"},
                    "resnet18_feature_baseline_ready",
                    "attach_baseline_class_and_feature_reference_back_to_geometry_layer");
            } else if (id == "torch.feature.resnet50.baseline") {
                set_flow(
                    spec,
                    "aligned_patch_or_baseline_roi",
                    "patch_level_class_label",
                    "baseline_class_and_feature_reference",
                    {"load_baseline_patches",
                     "run_resnet50_classifier_and_feature_pyramid",
                     "verify_classifier_and_feature_shapes",
                     "emit_baseline_feature_report"},
                    {"classifier_output_must_match_num_classes",
                     "feature_pyramid_shapes_must_match_contract"},
                    "resnet50_feature_baseline_ready",
                    "attach_baseline_class_and_feature_reference_back_to_geometry_layer");
            } else if (id == "torch.feature.segmentation.contract") {
                set_flow(
                    spec,
                    "whole_region_or_local_structure_patch",
                    "mask_or_region_label",
                    "mask_region_boundary_attach",
                    {"load_segmentation_tensor",
                     "run_mobilenetv3_backbone_and_deeplab_contracts",
                     "verify_feature_and_decoder_alignment",
                     "emit_feature_report_for_geometry_attach"},
                    {"low_high_feature_contract_must_hold",
                     "segmentation_output_shape_must_match",
                     "outputs_must_be_finite"},
                    "segmentation_feature_contract_ready",
                    "attach_mask_region_boundary_back_to_geometry_layer");
            } else if (id == "torch.scenario.yolo_mobilevit.infer") {
                set_flow(
                    spec,
                    "whole_image_then_rect_roi",
                    "bbox_plus_roi_class_chain",
                    "bbox_class_score_plus_roi_reclass",
                    {"load_scene_image",
                     "run_yolo_detection_on_scene",
                     "crop_roi_and_run_mobilevit_reclass",
                     "emit_two_stage_attach_result"},
                    {"detector_output_must_exist",
                     "classifier_output_must_exist",
                     "two_stage_pipeline_must_pass"},
                    "yolo_mobilevit_infer_scenario_ready",
                    "attach_bbox_and_roi_reclass_back_to_geometry_layer");
            } else if (id == "torch.scenario.yolo_mobilevit.train_mock") {
                set_flow(
                    spec,
                    "whole_image_then_rect_roi",
                    "bbox_candidate_plus_roi_class_label",
                    "roi_reclass_training_feedback",
                    {"load_scene_batch_and_roi_class_targets",
                     "run_detector_forward_for_candidate_context",
                     "crop_roi_and_run_mobilevit_train_mock",
                     "emit_two_stage_train_feedback"},
                    {"classification_loss_must_be_finite",
                     "two_stage_train_mock_must_pass"},
                    "yolo_mobilevit_train_mock_scenario_ready",
                    "attach_roi_training_feedback_back_to_geometry_layer");
            } else if (id == "torch.train.yolo.mainline") {
                set_flow(
                    spec,
                    "whole_image_or_search_window",
                    "bbox_plus_class_aligned_to_image",
                    "bbox_class_score_candidates",
                    {"load_detection_images_and_labels",
                     "apply_resize_and_label_alignment",
                     "run_yolo_trainer_session_and_lifecycle",
                     "build_unified_mainline_bundle_and_summary"},
                    {"smoke_train_must_pass",
                     "eval_gate_must_pass",
                     "trainer_lifecycle_must_pass",
                     "unified_summary_must_exist"},
                    "yolo_train_mainline_ready",
                    "attach_detected_bbox_candidates_back_to_geometry_layer");
            } else if (id == "torch.train.mobilevit.mainline") {
                set_flow(
                    spec,
                    "roi_patch_or_candidate_crop",
                    "roi_class_label",
                    "roi_reclass_score",
                    {"load_roi_train_and_eval_tensors",
                     "run_mobilevit_trainer_session",
                     "build_mobilevit_trainer_lifecycle_and_flat_run",
                     "build_mobilevit_unified_bundle_and_summary"},
                    {"classification_smoke_train_must_pass",
                     "eval_top1_summary_must_exist",
                     "trainer_lifecycle_must_pass",
                     "unified_summary_must_exist"},
                    "mobilevit_train_mainline_ready",
                    "attach_roi_reclass_result_back_to_geometry_layer");
            } else if (id == "torch.train.deeplab.mainline") {
                set_flow(
                    spec,
                    "whole_region_or_boundary_sensitive_patch",
                    "mask_or_region_label",
                    "mask_region_boundary_attach",
                    {"load_segmentation_tensor_and_mask",
                     "run_segmentation_mainline_session",
                     "run_segmentation_trainer_lifecycle",
                     "build_segmentation_unified_bundle_and_summary"},
                    {"segmentation_smoke_train_must_pass",
                     "foreground_iou_must_be_reported",
                     "trainer_lifecycle_must_pass",
                     "unified_summary_must_exist"},
                    "deeplab_train_mainline_ready",
                    "attach_mask_region_result_back_to_geometry_layer");
            } else if (id == "torch.train.mobilenetv3_deeplab.mainline") {
                set_flow(
                    spec,
                    "local_structure_region_or_foreground_patch",
                    "foreground_mask_or_region_label",
                    "foreground_mask_boundary_attach",
                    {"load_local_structure_tensor_and_mask",
                     "run_segmentation_mainline_session_with_mobilenetv3_backbone",
                     "run_segmentation_trainer_lifecycle",
                     "build_segmentation_unified_bundle_and_summary"},
                    {"low_high_feature_contract_must_hold",
                     "foreground_iou_must_be_reported",
                     "trainer_lifecycle_must_pass",
                     "unified_summary_must_exist"},
                    "mobilenetv3_deeplab_train_mainline_ready",
                    "attach_foreground_mask_and_boundary_back_to_geometry_layer");
            } else if (id == "torch.infer.yolo.unified") {
                set_flow(
                    spec,
                    "whole_image_or_search_window",
                    "bbox_plus_class_aligned_to_image",
                    "bbox_class_score_candidates",
                    {"load_validation_images_and_labels",
                     "run_yolo_infer_and_eval_gates",
                     "build_unified_mainline_bundle",
                     "emit_unified_summary_for_attach"},
                    {"tp_fp_fn_must_be_reported",
                     "per_class_metrics_must_exist",
                     "unified_summary_must_exist"},
                    "yolo_infer_ready",
                    "attach_bbox_class_score_back_to_geometry_layer");
            } else if (id == "torch.infer.mobilevit.unified") {
                set_flow(
                    spec,
                    "roi_patch_or_candidate_crop",
                    "roi_class_label",
                    "roi_reclass_score",
                    {"load_roi_eval_tensors_and_labels",
                     "run_mobilevit_infer_gates",
                     "build_mobilevit_unified_bundle",
                     "emit_unified_summary_for_attach"},
                    {"top1_or_confidence_summary_must_exist",
                     "unified_bundle_must_exist",
                     "unified_summary_must_exist"},
                    "mobilevit_infer_ready",
                    "attach_roi_reclass_score_back_to_geometry_layer");
            } else if (id == "torch.infer.mobilenetv3.baseline") {
                set_flow(
                    spec,
                    "local_structure_patch",
                    "lightweight_patch_or_foreground_hint",
                    "feature_alignment_hint",
                    {"load_local_structure_patches",
                     "run_mobilenetv3_backbone_forward_features",
                     "verify_feature_alignment_reference",
                     "emit_baseline_infer_report"},
                    {"low_level_feature_must_exist",
                     "high_level_feature_must_exist",
                     "feature_alignment_reference_must_exist"},
                    "mobilenetv3_infer_baseline_ready",
                    "attach_feature_alignment_hint_back_to_geometry_layer");
            } else if (id == "torch.infer.resnet18.baseline") {
                set_flow(
                    spec,
                    "aligned_patch_or_baseline_roi",
                    "patch_level_class_label",
                    "baseline_class_reference",
                    {"load_aligned_patches",
                     "run_resnet18_classifier_forward",
                     "verify_baseline_class_output",
                     "emit_baseline_infer_report"},
                    {"classifier_output_must_match_num_classes",
                     "baseline_class_reference_must_exist"},
                    "resnet18_infer_baseline_ready",
                    "attach_baseline_class_reference_back_to_geometry_layer");
            } else if (id == "torch.infer.resnet50.baseline") {
                set_flow(
                    spec,
                    "aligned_patch_or_baseline_roi",
                    "patch_level_class_label",
                    "baseline_class_reference",
                    {"load_aligned_patches",
                     "run_resnet50_classifier_forward",
                     "verify_baseline_class_output",
                     "emit_baseline_infer_report"},
                    {"classifier_output_must_match_num_classes",
                     "baseline_class_reference_must_exist"},
                    "resnet50_infer_baseline_ready",
                    "attach_baseline_class_reference_back_to_geometry_layer");
            } else if (id == "torch.infer.deeplab.unified") {
                set_flow(
                    spec,
                    "whole_region_or_boundary_sensitive_patch",
                    "mask_or_region_label",
                    "mask_region_boundary_attach",
                    {"load_segmentation_tensor",
                     "run_segmentation_eval_summary",
                     "build_segmentation_unified_bundle",
                     "emit_unified_summary_for_attach"},
                    {"mask_output_shape_must_match_input",
                     "foreground_iou_must_be_reported",
                     "unified_summary_must_exist"},
                    "deeplab_infer_ready",
                    "attach_mask_region_boundary_back_to_geometry_layer");
            } else if (id == "torch.infer.mobilenetv3_deeplab.unified") {
                set_flow(
                    spec,
                    "local_structure_region_or_foreground_patch",
                    "foreground_mask_or_region_label",
                    "foreground_mask_boundary_attach",
                    {"load_local_structure_tensor",
                     "run_mobilenetv3_deeplab_eval_summary",
                     "build_segmentation_unified_bundle",
                     "emit_unified_summary_for_attach"},
                    {"feature_to_decoder_alignment_must_hold",
                     "foreground_iou_must_be_reported",
                     "unified_summary_must_exist"},
                    "mobilenetv3_deeplab_infer_ready",
                    "attach_foreground_mask_and_boundary_back_to_geometry_layer");
            }
        }

        for (auto& spec : specs) {
            const std::string id = spec.task_id;
            if (id == "torch.smoke.yolo.cpu") {
                spec.contract_entry = "run_yolo_smoke_train_step";
                spec.bridge_dependencies = {"TorchTestHost::run_full_train_report", "torch_yolo_mainline_bridge.h"};
                spec.blocking_points = {"cxscript_cannot_directly_construct_detection_targets", "main_train_session_still_uses_bridge"};
            } else if (id == "torch.smoke.yolo.gpu") {
                spec.contract_entry = "run_yolo_smoke_train_step";
                spec.bridge_dependencies = {"TorchTestHost::run_full_train_report", "torch_yolo_mainline_bridge.h"};
                spec.blocking_points = {"gpu_full_session_not_yet_scripted", "cxscript_still_relies_on_host_report"};
            } else if (id == "torch.feature.yolo.eval") {
                spec.contract_entry = "run_yolo_eval_summary / val_summary";
                spec.bridge_dependencies = {"TorchTestHost::run_full_dataset_report", "torch_yolo_mainline_bridge.h"};
                spec.blocking_points = {"cxscript_cannot_directly_mount_dataset_layout", "eval_still_enters_through_host"};
            } else if (id == "torch.train.yolo.mainline") {
                spec.contract_entry = "run_yolo_trainer_session";
                spec.bridge_dependencies = {"TorchTestHost::run_full_train_report", "torch_yolo_mainline_bridge.h"};
                spec.blocking_points = {"dataset_mounting_still_host_managed", "full_gpu_train_session_not_exposed"};
            } else if (id == "torch.infer.yolo.unified") {
                spec.contract_entry = "run_yolo_eval_summary / build_yolo_unified_mainline_bundle";
                spec.bridge_dependencies = {"TorchTestHost::run_full_train_report", "torch_yolo_mainline_bridge.h"};
                spec.blocking_points = {"dataset_reader_still_host_managed"};
            } else if (id == "torch.smoke.mobilevit") {
                spec.contract_entry = "run_mobilevit_mainline_session";
                spec.bridge_dependencies = {"TorchTestHost::run_full_train_report", "torch_mobilevit_mainline_bridge.h"};
                spec.blocking_points = {"cxscript_cannot_directly_supply_roi_tensor_batch", "loss_trend_not_scripted"};
            } else if (id == "torch.feature.mobilevit.session") {
                spec.contract_entry = "run_mobilevit_mainline_session";
                spec.bridge_dependencies = {"TorchTestHost::run_full_train_report", "torch_mobilevit_mainline_bridge.h"};
                spec.blocking_points = {"session_construction_still_hidden_behind_host"};
                spec.handoff_types = {"TorchFeatureSemanticHandoff"};
                spec.downstream_threads = {"mlpack基础模型", "cxcore几何层"};
                spec.mlpack_feature_prepare_source = true;
            } else if (id == "torch.train.mobilevit.mainline") {
                spec.contract_entry = "run_mobilevit_trainer_session";
                spec.bridge_dependencies = {"TorchTestHost::run_full_train_report", "torch_mobilevit_mainline_bridge.h"};
                spec.blocking_points = {"roi_batch_construction_still_host_managed"};
                spec.handoff_types = {"TorchFeatureSemanticHandoff"};
                spec.downstream_threads = {"mlpack基础模型"};
                spec.mlpack_feature_prepare_source = true;
            } else if (id == "torch.infer.mobilevit.unified") {
                spec.contract_entry = "run_mobilevit_eval_summary / build_mobilevit_unified_mainline_bundle";
                spec.bridge_dependencies = {"TorchTestHost::run_full_train_report", "torch_mobilevit_mainline_bridge.h"};
                spec.blocking_points = {"roi_eval_tensor_still_host_managed"};
                spec.handoff_types = {"TorchFeatureSemanticHandoff"};
                spec.downstream_threads = {"mlpack基础模型", "remote_ai重组层"};
                spec.mlpack_feature_prepare_source = true;
            } else if (id == "torch.smoke.mobilenetv3" || id == "torch.feature.mobilenetv3.baseline" || id == "torch.infer.mobilenetv3.baseline") {
                spec.contract_entry = "MobileNetV3::forward_features";
                spec.bridge_dependencies = {"TorchTestHost::run_current_profile_report"};
                spec.blocking_points = {"no_standalone_train_contract", "feature_only_baseline"};
            } else if (id == "torch.smoke.deeplab" || id == "torch.feature.deeplab.contract") {
                spec.contract_entry = "run_deeplab_forward_contracts";
                spec.bridge_dependencies = {"TorchTestHost::run_current_profile_report"};
                spec.blocking_points = {"decoder_variant_selection_not_scripted"};
                spec.handoff_types = {"TorchGeometryHandoff", "TorchOptimizationHandoff"};
                spec.downstream_threads = {"cxcore几何层", "ensmallen优化层"};
            } else if (id == "torch.train.deeplab.mainline") {
                spec.contract_entry = "run_segmentation_trainer_session";
                spec.bridge_dependencies = {"TorchTestHost::run_current_profile_report", "torch_segmentation_mainline_bridge.h"};
                spec.blocking_points = {"decoder_backbone_selection_still_bridge_managed"};
                spec.handoff_types = {"TorchGeometryHandoff", "TorchOptimizationHandoff"};
                spec.downstream_threads = {"cxcore几何层", "ensmallen优化层"};
            } else if (id == "torch.infer.deeplab.unified") {
                spec.contract_entry = "run_segmentation_eval_summary / build_segmentation_unified_mainline_bundle";
                spec.bridge_dependencies = {"TorchTestHost::run_current_profile_report", "torch_segmentation_mainline_bridge.h"};
                spec.blocking_points = {"decoder_backbone_selection_still_bridge_managed"};
                spec.handoff_types = {"TorchGeometryHandoff", "TorchOptimizationHandoff"};
                spec.downstream_threads = {"cxcore几何层", "ensmallen优化层", "remote_ai重组层"};
            } else if (id == "torch.train.mobilenetv3_deeplab.mainline" || id == "torch.infer.mobilenetv3_deeplab.unified" || id == "torch.feature.segmentation.contract") {
                spec.contract_entry = "run_mobilenetv3_deeplab_eval_summary / segmentation bridge";
                spec.bridge_dependencies = {"TorchTestHost::run_current_profile_report", "torch_segmentation_mainline_bridge.h"};
                spec.blocking_points = {"mobilenetv3_plus_deeplab_still_combined_contract"};
            } else if (id == "torch.smoke.resnet18" || id == "torch.feature.resnet18.baseline" || id == "torch.infer.resnet18.baseline") {
                spec.contract_entry = "ResNet18::forward / forward_features";
                spec.bridge_dependencies = {"TorchTestHost::run_current_profile_report"};
                spec.blocking_points = {"no_verified_train_contract", "baseline_only"};
                if (id == "torch.feature.resnet18.baseline" || id == "torch.infer.resnet18.baseline") {
                    spec.handoff_types = {"TorchFeatureSemanticHandoff"};
                    spec.downstream_threads = {"mlpack基础模型", "cxcore几何层"};
                    spec.mlpack_feature_prepare_source = true;
                }
            } else if (id == "torch.smoke.resnet50" || id == "torch.feature.resnet50.baseline" || id == "torch.infer.resnet50.baseline") {
                spec.contract_entry = "ResNet50::forward / forward_features";
                spec.bridge_dependencies = {"TorchTestHost::run_current_profile_report"};
                spec.blocking_points = {"no_verified_train_contract", "baseline_only"};
                if (id == "torch.feature.resnet50.baseline" || id == "torch.infer.resnet50.baseline") {
                    spec.handoff_types = {"TorchFeatureSemanticHandoff"};
                    spec.downstream_threads = {"mlpack基础模型", "cxcore几何层"};
                    spec.mlpack_feature_prepare_source = true;
                }
            } else if (id == "torch.scenario.yolo_mobilevit.infer") {
                spec.contract_entry = "test_YOLOv8_MobileViTv2_TwoStageInfer";
                spec.bridge_dependencies = {"TorchTestHost::run_full_image_report"};
                spec.blocking_points = {"two_stage_path_not_directly_script_callable"};
                spec.handoff_types = {"TorchGeometryHandoff", "TorchFeatureSemanticHandoff"};
                spec.downstream_threads = {"cxcore几何层", "mlpack基础模型", "remote_ai重组层"};
                spec.mlpack_feature_prepare_source = true;
            } else if (id == "torch.scenario.yolo_mobilevit.train_mock") {
                spec.contract_entry = "test_YOLOv8_MobileViTv2_TwoStageTrainMock";
                spec.bridge_dependencies = {"TorchTestHost::run_full_train_report"};
                spec.blocking_points = {"two_stage_train_mock_only", "not_a_standalone_session"};
            }

            if (id == "torch.smoke.mobilevit" ||
                id == "torch.feature.mobilevit.session" ||
                id == "torch.train.mobilevit.mainline" ||
                id == "torch.infer.mobilevit.unified") {
                spec.validation_sequence = "phase1_primary";
                spec.cxcore_takeover_level = "direct_roi";
            } else if (id == "torch.smoke.deeplab" ||
                       id == "torch.feature.deeplab.contract" ||
                       id == "torch.train.deeplab.mainline" ||
                       id == "torch.infer.deeplab.unified") {
                spec.validation_sequence = "phase1_primary";
                spec.cxcore_takeover_level = "direct_region";
            } else if (id == "torch.infer.yolo.unified") {
                spec.validation_sequence = "phase1_primary";
                spec.cxcore_takeover_level = "direct_image_window";
            } else if (id == "torch.smoke.yolo.cpu" || id == "torch.train.yolo.mainline") {
                spec.validation_sequence = "phase2_after_label_bridge";
                spec.cxcore_takeover_level = "image_window_with_label_bridge";
            } else if (id == "torch.smoke.mobilenetv3" || id == "torch.infer.mobilenetv3.baseline") {
                spec.validation_sequence = "phase2_auxiliary";
                spec.cxcore_takeover_level = "direct_local_patch";
            } else if (id == "torch.smoke.resnet18" ||
                       id == "torch.infer.resnet18.baseline" ||
                       id == "torch.smoke.resnet50" ||
                       id == "torch.infer.resnet50.baseline") {
                spec.validation_sequence = "phase2_baseline";
                spec.cxcore_takeover_level = "direct_aligned_patch";
            }

            if (id == "torch.smoke.mobilevit" ||
                id == "torch.feature.mobilevit.session" ||
                id == "torch.infer.mobilevit.unified") {
                spec.integration_status = "ready_now";
                spec.intake_priority = "p1";
                if (id == "torch.feature.mobilevit.session") {
                    spec.required_input_contract = "roi_patch_batch_for_feature_prepare";
                    spec.required_input_fields = {"roi_patch_tensor", "roi_patch_count", "roi_patch_spatial_size", "roi_stats_ref"};
                    spec.required_input_checks = {"roi_patch_tensor_must_be_bchw", "roi_patch_count_must_be_positive"};
                } else if (id == "torch.infer.mobilevit.unified") {
                    spec.required_input_contract = "roi_patch_batch_for_reclass";
                    spec.required_input_fields = {"roi_patch_tensor", "roi_patch_count", "roi_patch_spatial_size"};
                    spec.required_input_checks = {"roi_patch_tensor_must_be_bchw", "roi_patch_count_must_be_positive"};
                }
            } else if (id == "torch.train.mobilevit.mainline") {
                spec.integration_status = "gap_label_bridge";
                spec.integration_gaps = {"roi_class_label_bridge_pending", "roi_batch_construction_host_managed"};
                spec.intake_priority = "p1";
                spec.required_label_contract = "roi_patch_batch_plus_roi_class_label";
                spec.next_minimal_uplift = "expose_direct_roi_batch_and_label_contract";
                spec.required_label_fields = {"roi_patch_tensor", "roi_class_label", "roi_batch_size"};
                spec.required_label_checks = {"roi_patch_tensor_shape_must_match_batch", "roi_class_label_count_must_match_batch"};
            } else if (id == "torch.smoke.deeplab" || id == "torch.infer.deeplab.unified") {
                spec.integration_status = "ready_now";
                spec.intake_priority = "p2";
                if (id == "torch.infer.deeplab.unified") {
                    spec.required_input_contract = "region_tensor_for_segmentation_infer";
                    spec.required_input_fields = {"region_tensor", "region_spatial_size", "region_channel_layout"};
                    spec.required_input_checks = {"region_tensor_must_be_bchw", "region_spatial_size_must_be_positive"};
                }
            } else if (id == "torch.train.deeplab.mainline") {
                spec.integration_status = "gap_mask_label_bridge";
                spec.integration_gaps = {"mask_or_region_label_bridge_pending", "decoder_backbone_selection_bridge_managed"};
                spec.intake_priority = "p2";
                spec.required_label_contract = "region_tensor_plus_mask_or_region_label";
                spec.next_minimal_uplift = "expose_direct_mask_label_contract";
                spec.required_label_fields = {"region_tensor", "mask_or_region_label", "region_spatial_size"};
                spec.required_label_checks = {"mask_shape_must_match_region_tensor", "mask_label_must_be_non_empty"};
            } else if (id == "torch.infer.yolo.unified") {
                spec.integration_status = "ready_now";
                spec.intake_priority = "p3";
                spec.required_input_contract = "image_window_batch_for_detection_infer";
                spec.required_input_fields = {"image_window_tensor", "image_window_count", "resize_policy"};
                spec.required_input_checks = {"image_window_tensor_must_be_bchw", "resize_policy_must_match_letterbox_or_plain"};
            } else if (id == "torch.scenario.yolo_mobilevit.infer") {
                spec.integration_status = "ready_via_host_combo";
                spec.integration_gaps = {"two_stage_direct_contract_missing", "combo_path_still_host_managed"};
                spec.intake_priority = "p3";
            } else if (id == "torch.smoke.yolo.cpu" || id == "torch.train.yolo.mainline") {
                spec.integration_status = "gap_detection_label_bridge";
                spec.integration_gaps = {"bbox_class_label_bridge_pending", "detection_target_tensor_still_host_managed"};
                spec.intake_priority = "p4";
                spec.required_label_contract = "image_window_plus_bbox_class_targets";
                spec.next_minimal_uplift = "expose_direct_detection_target_contract";
                spec.required_label_fields = {"image_window_tensor", "bbox_xyxy_targets", "class_id_targets", "target_count"};
                spec.required_label_checks = {"bbox_target_count_must_match_class_count", "bbox_targets_must_be_xyxy_or_normalized_xyxy"};
            } else if (id == "torch.scenario.yolo_mobilevit.train_mock") {
                spec.integration_status = "mock_only_gap";
                spec.integration_gaps = {"two_stage_train_mock_not_standalone", "roi_label_chain_still_host_managed"};
                spec.intake_priority = "p4";
                spec.required_label_contract = "image_window_plus_bbox_candidates_plus_roi_class_label";
                spec.next_minimal_uplift = "expose_direct_two_stage_roi_train_mock_contract";
                spec.required_label_fields = {"image_window_tensor", "bbox_candidate_list", "roi_patch_tensor", "roi_class_label"};
                spec.required_label_checks = {"bbox_candidate_list_must_align_with_roi_patch_count", "roi_class_label_count_must_match_roi_patch_count"};
            } else if (id == "torch.smoke.mobilenetv3" || id == "torch.infer.mobilenetv3.baseline") {
                spec.integration_status = "auxiliary_ready";
                spec.integration_gaps = {"no_standalone_train_contract"};
                spec.intake_priority = "p5";
            } else if (id == "torch.smoke.resnet18" ||
                       id == "torch.feature.resnet18.baseline" ||
                       id == "torch.infer.resnet18.baseline" ||
                       id == "torch.smoke.resnet50" ||
                       id == "torch.feature.resnet50.baseline" ||
                       id == "torch.infer.resnet50.baseline") {
                spec.integration_status = "baseline_ready";
                spec.integration_gaps = {"no_verified_train_contract"};
                spec.intake_priority = "p6";
                if (id == "torch.feature.resnet18.baseline" ||
                    id == "torch.feature.resnet50.baseline") {
                    spec.required_input_contract = "aligned_patch_baseline_feature_prepare";
                    spec.required_input_fields = {"aligned_patch_tensor", "patch_count", "patch_spatial_size", "patch_level_class_label"};
                    spec.required_input_checks = {"aligned_patch_tensor_must_be_bchw", "patch_count_must_be_positive", "patch_level_class_label_count_must_match_patch_count"};
                } else if (id == "torch.infer.resnet18.baseline" ||
                           id == "torch.infer.resnet50.baseline") {
                    spec.required_input_contract = "aligned_patch_baseline_class_infer";
                    spec.required_input_fields = {"aligned_patch_tensor", "patch_count", "patch_spatial_size", "patch_level_class_label"};
                    spec.required_input_checks = {"aligned_patch_tensor_must_be_bchw", "patch_count_must_be_positive", "classifier_output_must_match_num_classes"};
                }
            }
        }

        return specs;
    }

    static std::vector<TorchTaskSpec> task_specs_for_profile(TorchTestProfile profile) {
        const auto specs = task_specs();
        if (profile == TorchTestProfile::FullAll) {
            return specs;
        }

        std::vector<TorchTaskSpec> filtered;
        filtered.reserve(specs.size());
        for (const auto& spec : specs) {
            if (spec.profile == profile || spec.profile == TorchTestProfile::FullAll) {
                filtered.push_back(spec);
            }
        }
        return filtered;
    }

    static std::string format_task_spec_line(const TorchTaskSpec& spec) {
        std::ostringstream os;
        os << "task_id=" << spec.task_id
           << " layer=" << layer_name(spec.layer)
           << " profile=" << profile_name(spec.profile)
           << " geometry=" << spec.geometry_input_prior
           << " attach=" << spec.attach_back_result
           << " summary=" << spec.summary;
        return os.str();
    }

    static TorchGeometryHandoff build_geometry_handoff_stub(const TorchTaskSpec& spec) {
        TorchGeometryHandoff handoff;
        handoff.source_hash = torch_make_handoff_ref(spec.task_id, "source_hash");
        handoff.result_ref = torch_make_handoff_ref(spec.task_id, "result_ref");
        handoff.evidence_ref = torch_make_handoff_ref(spec.task_id, "evidence_ref");
        handoff.log_path = torch_make_handoff_ref(spec.task_id, "log_path");
        handoff.model_version = torch_make_handoff_ref(spec.task_id, "model_version");
        handoff.bbox_ref = torch_make_handoff_ref(spec.task_id, "bbox_ref");
        handoff.mask_ref = torch_make_handoff_ref(spec.task_id, "mask_ref");
        handoff.roi_ref = torch_make_handoff_ref(spec.task_id, "roi_ref");
        handoff.region_ref = torch_make_handoff_ref(spec.task_id, "region_ref");
        handoff.contour_ref = torch_make_handoff_ref(spec.task_id, "contour_ref");
        handoff.geometry_ref = torch_make_handoff_ref(spec.task_id, "geometry_ref");
        handoff.measurement_ref = torch_make_handoff_ref(spec.task_id, "measurement_ref");
        handoff.bbox_candidate_list_ref = torch_make_handoff_ref(spec.task_id, "bbox_candidate_list_ref");
        if (torch_task_id_contains(spec.task_id, "yolo_mobilevit")) {
            handoff.next_action = "handoff_to_cxcore_geometry_attach_then_roi_reclass_merge";
        } else if (torch_task_id_contains(spec.task_id, "deeplab") ||
                   torch_task_id_contains(spec.task_id, "segmentation")) {
            handoff.next_action = "handoff_to_cxcore_mask_boundary_attach";
        } else if (torch_task_id_contains(spec.task_id, "yolo")) {
            handoff.next_action = "handoff_to_cxcore_bbox_attach";
        } else {
            handoff.next_action = spec.conclusion_attach;
        }
        return handoff;
    }

    static TorchFeatureSemanticHandoff build_feature_semantic_handoff_stub(const TorchTaskSpec& spec) {
        TorchFeatureSemanticHandoff handoff;
        handoff.source_hash = torch_make_handoff_ref(spec.task_id, "source_hash");
        handoff.result_ref = torch_make_handoff_ref(spec.task_id, "result_ref");
        handoff.evidence_ref = torch_make_handoff_ref(spec.task_id, "evidence_ref");
        handoff.log_path = torch_make_handoff_ref(spec.task_id, "log_path");
        handoff.model_version = torch_make_handoff_ref(spec.task_id, "model_version");
        handoff.roi_ref = torch_make_handoff_ref(spec.task_id, "roi_ref");
        handoff.roi_stats_ref = torch_make_handoff_ref(spec.task_id, "roi_stats_ref");
        handoff.geometry_ref = torch_make_handoff_ref(spec.task_id, "geometry_ref");
        handoff.embedding_ref = torch_make_handoff_ref(spec.task_id, "embedding_ref");
        handoff.feature_vector_ref = torch_make_handoff_ref(spec.task_id, "feature_vector_ref");
        handoff.feature_set_ref = torch_make_handoff_ref(spec.task_id, "feature_set_ref");
        handoff.feature_dim = 256;
        handoff.top1_class_ref = torch_make_handoff_ref(spec.task_id, "top1_class_ref");
        handoff.class_confidence_ref = torch_make_handoff_ref(spec.task_id, "class_confidence_ref");
        handoff.template_alignment_ref = torch_make_handoff_ref(spec.task_id, "template_alignment_ref");
        handoff.template_test_alignment_status = torch_make_handoff_ref(spec.task_id, "template_test_alignment_status");
        handoff.roi_diff_candidate_ref = torch_make_handoff_ref(spec.task_id, "roi_diff_candidate_ref");
        handoff.roi_diff_candidate_count = torch_make_handoff_ref(spec.task_id, "roi_diff_candidate_count");
        handoff.prior_roi_region_ref = torch_make_handoff_ref(spec.task_id, "prior_roi_region_ref");
        handoff.roi_crop_packet_ref = torch_make_handoff_ref(spec.task_id, "roi_crop_packet_ref");
        handoff.roi_crop_count = torch_make_handoff_ref(spec.task_id, "roi_crop_count");
        handoff.roi_crop_spatial_size = torch_make_handoff_ref(spec.task_id, "roi_crop_spatial_size");
        handoff.roi_crop_policy_ref = torch_make_handoff_ref(spec.task_id, "roi_crop_policy_ref");
        handoff.confidence = 0.95;
        if (torch_task_id_contains(spec.task_id, "yolo_mobilevit")) {
            handoff.next_action = "handoff_to_mlpack_feature_prepare_then_remote_ai_compare";
        } else if (torch_task_id_contains(spec.task_id, "mobilevit")) {
            handoff.next_action = "handoff_to_mlpack_feature_prepare";
        } else if (torch_task_id_contains(spec.task_id, "resnet")) {
            handoff.next_action = "handoff_to_mlpack_baseline_compare";
        } else if (spec.mlpack_feature_prepare_source) {
            handoff.next_action = "handoff_to_mlpack_feature_prepare";
        } else {
            handoff.next_action = spec.conclusion_attach;
        }
        return handoff;
    }

    static TorchOptimizationHandoff build_optimization_handoff_stub(const TorchTaskSpec& spec) {
        TorchOptimizationHandoff handoff;
        handoff.source_hash = torch_make_handoff_ref(spec.task_id, "source_hash");
        handoff.result_ref = torch_make_handoff_ref(spec.task_id, "result_ref");
        handoff.evidence_ref = torch_make_handoff_ref(spec.task_id, "evidence_ref");
        handoff.log_path = torch_make_handoff_ref(spec.task_id, "log_path");
        handoff.model_version = torch_make_handoff_ref(spec.task_id, "model_version");
        handoff.geometry_ref = torch_make_handoff_ref(spec.task_id, "geometry_ref");
        handoff.objective_ref = torch_make_handoff_ref(spec.task_id, "objective_ref");
        handoff.threshold_ref = torch_make_handoff_ref(spec.task_id, "threshold_ref");
        handoff.crop_policy_ref = torch_make_handoff_ref(spec.task_id, "crop_policy_ref");
        handoff.boundary_error_ref = torch_make_handoff_ref(spec.task_id, "boundary_error_ref");
        handoff.alignment_error_ref = torch_make_handoff_ref(spec.task_id, "alignment_error_ref");
        handoff.optimization_result_ref = torch_make_handoff_ref(spec.task_id, "optimization_result_ref");
        if (torch_task_id_contains(spec.task_id, "deeplab") ||
            torch_task_id_contains(spec.task_id, "segmentation")) {
            handoff.next_action = "handoff_to_ensmallen_mask_boundary_optimize";
        } else if (torch_task_id_contains(spec.task_id, "mobilevit")) {
            handoff.next_action = "handoff_to_ensmallen_roi_crop_optimize";
        } else {
            handoff.next_action = "handoff_to_ensmallen_optimize";
        }
        return handoff;
    }

    static std::vector<std::string> make_task_handoff_summary_lines(const TorchTaskSpec& spec) {
        std::vector<std::string> lines;
        lines.reserve(spec.handoff_types.size());
        for (const char* handoff_type : spec.handoff_types) {
            if (std::string(handoff_type) == "TorchGeometryHandoff") {
                lines.push_back(
                    "task_id=" + std::string(spec.task_id) +
                    " handoff_stub_type=TorchGeometryHandoff summary=" +
                    build_geometry_handoff_stub(spec).describe());
            } else if (std::string(handoff_type) == "TorchFeatureSemanticHandoff") {
                lines.push_back(
                    "task_id=" + std::string(spec.task_id) +
                    " handoff_stub_type=TorchFeatureSemanticHandoff summary=" +
                    build_feature_semantic_handoff_stub(spec).describe());
            } else if (std::string(handoff_type) == "TorchOptimizationHandoff") {
                lines.push_back(
                    "task_id=" + std::string(spec.task_id) +
                    " handoff_stub_type=TorchOptimizationHandoff summary=" +
                    build_optimization_handoff_stub(spec).describe());
            }
        }
        return lines;
    }

    static std::vector<std::string> make_task_handoff_role_lines(const TorchTaskSpec& spec) {
        std::vector<std::string> lines;
        if (spec.downstream_threads.empty()) {
            return lines;
        }

        auto push_role = [&](const char* handoff_type, const char* primary_consumer, const char* handoff_role) {
            lines.push_back(
                "task_id=" + std::string(spec.task_id) +
                " handoff_role_type=" + std::string(handoff_type) +
                " primary_consumer=" + primary_consumer +
                " handoff_role=" + handoff_role);
        };

        for (const char* handoff_type : spec.handoff_types) {
            const std::string type = handoff_type != nullptr ? handoff_type : "";
            if (type == "TorchGeometryHandoff") {
                if (torch_task_id_contains(spec.task_id, "deeplab") ||
                    torch_task_id_contains(spec.task_id, "segmentation")) {
                    push_role(handoff_type, "cxcore_geometry_layer", "mask_boundary_region_attach");
                } else {
                    push_role(handoff_type, "cxcore_geometry_layer", "bbox_roi_geometry_attach");
                }
            } else if (type == "TorchFeatureSemanticHandoff") {
                if (torch_task_id_contains(spec.task_id, "resnet")) {
                    push_role(handoff_type, "mlpack_baseline_model", "baseline_feature_compare");
                } else if (torch_task_id_contains(spec.task_id, "yolo_mobilevit")) {
                    push_role(handoff_type, "mlpack_baseline_model", "roi_feature_prepare_then_remote_compare");
                } else {
                    push_role(handoff_type, "mlpack_baseline_model", "feature_prepare");
                }
            } else if (type == "TorchOptimizationHandoff") {
                if (torch_task_id_contains(spec.task_id, "deeplab") ||
                    torch_task_id_contains(spec.task_id, "segmentation")) {
                    push_role(handoff_type, "ensmallen_optimization_layer", "mask_boundary_threshold_optimize");
                } else {
                    push_role(handoff_type, "ensmallen_optimization_layer", "roi_crop_threshold_optimize");
                }
            }
        }

        return lines;
    }

    static std::vector<std::string> format_task_detail_lines(const TorchTaskSpec& spec) {
        auto join = [](const std::vector<const char*>& items) {
            std::ostringstream os;
            for (size_t i = 0; i < items.size(); ++i) {
                if (i > 0) {
                    os << ",";
                }
                os << items[i];
            }
            return os.str();
        };

        auto lines = std::vector<std::string>{
            "task_id=" + std::string(spec.task_id) + " inputs=" + join(spec.inputs),
            "task_id=" + std::string(spec.task_id) + " outputs=" + join(spec.outputs),
            "task_id=" + std::string(spec.task_id) + " deps=" + join(spec.dependencies),
            "task_id=" + std::string(spec.task_id) + " params=" + join(spec.script_params),
            "task_id=" + std::string(spec.task_id) + " contract_entry=" + spec.contract_entry,
            "task_id=" + std::string(spec.task_id) + " bridge_dependencies=" + join(spec.bridge_dependencies),
            "task_id=" + std::string(spec.task_id) + " blocking_points=" + join(spec.blocking_points),
            "task_id=" + std::string(spec.task_id) + " validation_sequence=" + spec.validation_sequence,
            "task_id=" + std::string(spec.task_id) + " cxcore_takeover_level=" + spec.cxcore_takeover_level,
            "task_id=" + std::string(spec.task_id) + " integration_status=" + spec.integration_status,
            "task_id=" + std::string(spec.task_id) + " integration_gaps=" + join(spec.integration_gaps),
            "task_id=" + std::string(spec.task_id) + " intake_priority=" + spec.intake_priority,
            "task_id=" + std::string(spec.task_id) + " required_label_contract=" + spec.required_label_contract,
            "task_id=" + std::string(spec.task_id) + " next_minimal_uplift=" + spec.next_minimal_uplift,
            "task_id=" + std::string(spec.task_id) + " required_label_fields=" + join(spec.required_label_fields),
            "task_id=" + std::string(spec.task_id) + " required_label_checks=" + join(spec.required_label_checks),
            "task_id=" + std::string(spec.task_id) + " required_input_contract=" + spec.required_input_contract,
            "task_id=" + std::string(spec.task_id) + " required_input_fields=" + join(spec.required_input_fields),
            "task_id=" + std::string(spec.task_id) + " required_input_checks=" + join(spec.required_input_checks),
            "task_id=" + std::string(spec.task_id) + " handoff_types=" + join(spec.handoff_types),
            "task_id=" + std::string(spec.task_id) + " downstream_threads=" + join(spec.downstream_threads),
            "task_id=" + std::string(spec.task_id) + " mlpack_feature_prepare_source=" + std::string(spec.mlpack_feature_prepare_source ? "true" : "false"),
            "task_id=" + std::string(spec.task_id) + " geometry_input_prior=" + spec.geometry_input_prior,
            "task_id=" + std::string(spec.task_id) + " geometry_label_align=" + spec.geometry_label_align,
            "task_id=" + std::string(spec.task_id) + " attach_back_result=" + spec.attach_back_result,
            "task_id=" + std::string(spec.task_id) + " flow=" + join(spec.flow_steps),
            "task_id=" + std::string(spec.task_id) + " checks=" + join(spec.check_points),
            "task_id=" + std::string(spec.task_id) + " conclusion_pass=" + spec.conclusion_pass,
            "task_id=" + std::string(spec.task_id) + " conclusion_attach=" + spec.conclusion_attach
        };
        auto handoff_lines = make_task_handoff_summary_lines(spec);
        lines.insert(lines.end(), handoff_lines.begin(), handoff_lines.end());
        auto handoff_role_lines = make_task_handoff_role_lines(spec);
        lines.insert(lines.end(), handoff_role_lines.begin(), handoff_role_lines.end());
        return lines;
    }

    static std::string format_report_line(const TorchStageReport& report) {
        std::ostringstream os;
        os << "stage_id=" << profile_stage_id(report.profile)
           << " profile=" << profile_name(report.profile)
           << " passed=" << (report.passed ? "true" : "false")
           << " failures=" << report.failures
           << " summary=" << report.summary;
        if (!report.selected_task_id.empty()) {
            os << " task_id=" << report.selected_task_id;
        }
        if (!report.requested_device.empty()) {
            os << " requested_device=" << report.requested_device;
        }
        if (!report.actual_device.empty()) {
            os << " actual_device=" << report.actual_device;
        }
        os << " runtime_ms=" << report.runtime_ms;
        return os.str();
    }

    static std::vector<std::string> format_check_lines(const TorchStageReport& report) {
        std::vector<std::string> lines;
        lines.reserve(report.checks.size());
        for (const auto& check : report.checks) {
            std::ostringstream os;
            os << "stage_id=" << profile_stage_id(report.profile)
               << " check=" << check.name
               << " passed=" << (check.passed ? "true" : "false");
            if (!report.selected_task_id.empty()) {
                os << " task_id=" << report.selected_task_id;
            }
            if (!report.requested_device.empty()) {
                os << " requested_device=" << report.requested_device;
            }
            if (!report.actual_device.empty()) {
                os << " actual_device=" << report.actual_device;
            }
            os << " runtime_ms=" << report.runtime_ms;
            lines.push_back(os.str());
        }
        return lines;
    }

    static std::string mobilevit_dataset_focus_line() {
        std::ostringstream os;
        os << "focus=mobilevit dataset stage";
#if TORCH_FULL_ENABLE_DATASET_STAGE && TORCH_FULL_ENABLE_MOBILEVIT_DATASET
        os << " dataset_smoke=enabled";
#else
        os << " dataset_smoke=disabled";
#endif
#if TORCH_FULL_ENABLE_DATASET_STAGE && TORCH_FULL_ENABLE_MOBILEVIT_FULLTRAIN
        os << " dataset_fulltrain=enabled";
#else
        os << " dataset_fulltrain=disabled";
#endif
        os << " dataset_env=LIBTORCH_MODULE_MOBILEVIT_DATASET";
        os << " missing_probe_env=LIBTORCH_MODULE_MOBILEVIT_MISSING_DATASET";
        return os.str();
    }

    static std::string mobilevit_train_focus_line() {
        std::ostringstream os;
        os << "focus=mobilevit train stage";
#if TORCH_FULL_ENABLE_MOBILEVIT_TRAIN
        os << " mobilevit_mainline=enabled";
#else
        os << " mobilevit_mainline=disabled";
#endif
#if TORCH_FULL_ENABLE_TWOSTAGE_INFER
        os << " two_stage_infer=enabled";
#else
        os << " two_stage_infer=disabled";
#endif
#if TORCH_FULL_ENABLE_TWOSTAGE_TRAIN
        os << " two_stage_train=enabled";
#else
        os << " two_stage_train=disabled";
#endif
#if TORCH_FULL_ENABLE_MOBILEVIT_TRAIN
        os << " segmentation_mainline=enabled";
#else
        os << " segmentation_mainline=disabled";
#endif
        os << " weights_env=LIBTORCH_MODULE_MOBILEVIT_WEIGHTS";
        os << " roi_env=LIBTORCH_MODULE_MOBILEVIT_ROI_SIZE";
        os << " twostage_envs=LIBTORCH_MODULE_TWOSTAGE_IMAGE_SIZE,LIBTORCH_MODULE_TWOSTAGE_INFER_CLASSES,LIBTORCH_MODULE_TWOSTAGE_TRAIN_CLASSES,LIBTORCH_MODULE_TWOSTAGE_TRAIN_BATCH";
        return os.str();
    }

    static std::string yolo_resource_focus_line() {
        std::ostringstream os;
        os << "focus=yolo resource envs";
        os << " weights=LIBTORCH_MODULE_YOLO_WEIGHTS";
        os << " pretrained=LIBTORCH_MODULE_YOLO_PRETRAINED";
        os << " data_root=LIBTORCH_MODULE_YOLO_DATA_ROOT";
        os << " train_images=LIBTORCH_MODULE_YOLO_TRAIN_IMAGES";
        os << " train_labels=LIBTORCH_MODULE_YOLO_TRAIN_LABELS";
        os << " infer_image=LIBTORCH_MODULE_YOLO_INFER_IMAGE";
        os << " infer_output=LIBTORCH_MODULE_YOLO_INFER_OUTPUT";
        return os.str();
    }

    static std::string torch_multimodal_role_line() {
        std::ostringstream os;
        os << "focus=torch multimodal role";
        os << " thread_scope=feature_semantic+model_inference+simulation_handoff";
        os << " primary_slices=feature_semantic_slice_v1,visual_geometry_slice_v1,semantic_simulation_trace_v1";
        os << " public_entry=cxparser_ext_cxscript_cli";
        return os.str();
    }

    static std::string torch_multimodal_contract_line() {
        std::ostringstream os;
        os << "focus=torch multimodal contract";
        os << " outputs=result_ref,evidence_ref,log_path,source_hash,model_version,next_action";
        os << " model_families=yolo,mobilevit,deeplab,mobilenetv3_deeplab,resnet";
        os << " handoff=remote_ai_reads_structured_slices_not_raw_private_images";
        return os.str();
    }

    static std::string torch_multimodal_plan_line() {
        std::ostringstream os;
        os << "focus=torch multimodal plan";
        os << " stage1=roi_or_scene_tensor_to_detection_classification_mask_evidence";
        os << " stage2=attach_geometry_feature_relations_for_cxparser_atoms";
        os << " stage3=emit_next_action_for_visual_compare_or_semantic_simulation";
        return os.str();
    }

    static std::vector<std::string> multimodal_handoff_overview_lines() {
        auto lines = std::vector<std::string>{
            "focus=multimodal handoff overview key_tasks=torch.feature.mobilevit.session,torch.scenario.yolo_mobilevit.infer,torch.infer.deeplab.unified"
        };

        const auto specs = task_specs();
        for (const auto& spec : specs) {
            const std::string id = spec.task_id;
            if (id == "torch.feature.mobilevit.session" ||
                id == "torch.scenario.yolo_mobilevit.infer" ||
                id == "torch.infer.deeplab.unified") {
                std::ostringstream os;
                os << "overview task_id=" << spec.task_id
                   << " handoff_types=";
                for (size_t i = 0; i < spec.handoff_types.size(); ++i) {
                    if (i > 0) {
                        os << ",";
                    }
                    os << spec.handoff_types[i];
                }
                os << " downstream_threads=";
                for (size_t i = 0; i < spec.downstream_threads.size(); ++i) {
                    if (i > 0) {
                        os << ",";
                    }
                    os << spec.downstream_threads[i];
                }
                os << " mlpack_feature_prepare_source="
                   << (spec.mlpack_feature_prepare_source ? "true" : "false");
                os << " handoff_priority=" << spec.intake_priority;
                os << " integration_readiness=" << spec.integration_status;
                lines.push_back(os.str());

                const auto handoff_lines = make_task_handoff_summary_lines(spec);
                lines.insert(lines.end(), handoff_lines.begin(), handoff_lines.end());

                const auto role_lines = make_task_handoff_role_lines(spec);
                lines.insert(lines.end(), role_lines.begin(), role_lines.end());

                if (!spec.integration_gaps.empty()) {
                    std::ostringstream gap_os;
                    gap_os << "overview task_id=" << spec.task_id << " integration_gaps=";
                    for (size_t i = 0; i < spec.integration_gaps.size(); ++i) {
                        if (i > 0) {
                            gap_os << ",";
                        }
                        gap_os << spec.integration_gaps[i];
                    }
                    lines.push_back(gap_os.str());
                }

                if (spec.required_input_contract != nullptr && spec.required_input_contract[0] != '\0') {
                    lines.push_back(
                        "overview task_id=" + std::string(spec.task_id) +
                        " required_input_contract=" + spec.required_input_contract);
                }

                if (spec.required_label_contract != nullptr && spec.required_label_contract[0] != '\0') {
                    lines.push_back(
                        "overview task_id=" + std::string(spec.task_id) +
                        " required_label_contract=" + spec.required_label_contract);
                }
            }
        }

        return lines;
    }

    static std::vector<std::string> dataset_contract_overview_lines() {
        auto lines = std::vector<std::string>{
            "focus=dataset contract overview groups=detection,segmentation,roi_reclass,baseline_patch"
        };

        const auto specs = task_specs();
        for (const auto& spec : specs) {
            const bool has_input_contract =
                spec.required_input_contract != nullptr &&
                spec.required_input_contract[0] != '\0';
            const bool has_label_contract =
                spec.required_label_contract != nullptr &&
                spec.required_label_contract[0] != '\0';
            if (!has_input_contract && !has_label_contract) {
                continue;
            }

            std::string dataset_group;
            const std::string task_id = spec.task_id;
            if (task_id.find("torch.train.yolo.") == 0 ||
                task_id.find("torch.infer.yolo.") == 0 ||
                task_id.find("torch.feature.yolo.") == 0 ||
                task_id.find("torch.scenario.yolo_mobilevit.") == 0) {
                dataset_group = "detection";
            } else if (task_id.find("torch.train.deeplab.") == 0 ||
                       task_id.find("torch.infer.deeplab.") == 0 ||
                       task_id.find("torch.feature.deeplab.") == 0 ||
                       task_id.find("torch.train.mobilenetv3_deeplab.") == 0 ||
                       task_id.find("torch.infer.mobilenetv3_deeplab.") == 0 ||
                       task_id.find("torch.feature.segmentation.") == 0) {
                dataset_group = "segmentation";
            } else if (task_id.find("torch.train.mobilevit.") == 0 ||
                       task_id.find("torch.infer.mobilevit.") == 0 ||
                       task_id.find("torch.feature.mobilevit.") == 0) {
                dataset_group = "roi_reclass";
            } else if (task_id.find("torch.feature.resnet") == 0 ||
                       task_id.find("torch.infer.resnet") == 0) {
                dataset_group = "baseline_patch";
            } else {
                continue;
            }

            std::ostringstream os;
            os << "dataset_overview task_id=" << spec.task_id
               << " dataset_group=" << dataset_group;
            if (has_input_contract) {
                os << " required_input_contract=" << spec.required_input_contract;
            }
            if (has_label_contract) {
                os << " required_label_contract=" << spec.required_label_contract;
            }
            if (!spec.required_input_fields.empty()) {
                os << " input_fields=";
                for (size_t i = 0; i < spec.required_input_fields.size(); ++i) {
                    if (i > 0) {
                        os << ",";
                    }
                    os << spec.required_input_fields[i];
                }
            }
            if (!spec.required_input_checks.empty()) {
                os << " input_checks=";
                for (size_t i = 0; i < spec.required_input_checks.size(); ++i) {
                    if (i > 0) {
                        os << ",";
                    }
                    os << spec.required_input_checks[i];
                }
            }
            lines.push_back(os.str());
        }

        return lines;
    }

    static std::vector<std::string> demo_focus_lines(TorchTestProfile profile) {
        switch (profile) {
            case TorchTestProfile::PreprocessContract:
            {
                auto lines = std::vector<std::string>{
                    "focus=opencv_image_stage -> opencv_tensor_contract -> opencv_annotation_sync -> yolo_dataset_contract",
                    yolo_resource_focus_line(),
                    torch_multimodal_role_line()
                };
                const auto dataset_overview = dataset_contract_overview_lines();
                lines.insert(lines.end(), dataset_overview.begin(), dataset_overview.end());
                return lines;
            }
            case TorchTestProfile::PostprocessContract:
            {
                auto lines = std::vector<std::string>{
                    "focus=yolov8_loss_contract -> yolov8_build_config",
                    yolo_resource_focus_line(),
                    torch_multimodal_contract_line()
                };
                const auto dataset_overview = dataset_contract_overview_lines();
                lines.insert(lines.end(), dataset_overview.begin(), dataset_overview.end());
                return lines;
            }
            case TorchTestProfile::FullDataset:
            {
                auto lines = std::vector<std::string>{
                    "focus=yolo image/annotation/dataset validation",
                    yolo_resource_focus_line(),
                    mobilevit_dataset_focus_line(),
                    torch_multimodal_role_line(),
                    torch_multimodal_contract_line(),
                    "focus=stage report/check report"
                };
                const auto dataset_overview = dataset_contract_overview_lines();
                lines.insert(lines.end(), dataset_overview.begin(), dataset_overview.end());
                return lines;
            }
            case TorchTestProfile::FullImage:
            {
                auto lines = std::vector<std::string>{
                    "focus=full-dataset path",
                    yolo_resource_focus_line(),
                    torch_multimodal_role_line(),
                    torch_multimodal_plan_line(),
                    "focus=two-stage inference smoke",
                    "focus=inference-side stage report"
                };
                const auto dataset_overview = dataset_contract_overview_lines();
                lines.insert(lines.end(), dataset_overview.begin(), dataset_overview.end());
                const auto overview = multimodal_handoff_overview_lines();
                lines.insert(lines.end(), overview.begin(), overview.end());
                return lines;
            }
            case TorchTestProfile::FullTrain:
            {
                auto lines = std::vector<std::string>{
                    "focus=yolo smoke runtime config + training loop debug + gpu smoke",
                    "focus=yolo trainer lifecycle summary + unified mainline bundle + unified mainline summary",
                    yolo_resource_focus_line(),
                    mobilevit_train_focus_line(),
                    torch_multimodal_contract_line(),
                    torch_multimodal_plan_line()
                };
                const auto dataset_overview = dataset_contract_overview_lines();
                lines.insert(lines.end(), dataset_overview.begin(), dataset_overview.end());
                const auto overview = multimodal_handoff_overview_lines();
                lines.insert(lines.end(), overview.begin(), overview.end());
                return lines;
            }
            case TorchTestProfile::FullAll:
            default:
            {
                auto lines = std::vector<std::string>{
                    "focus=full validation aggregate",
                    yolo_resource_focus_line(),
                    mobilevit_train_focus_line(),
                    torch_multimodal_role_line(),
                    torch_multimodal_contract_line(),
                    torch_multimodal_plan_line()
                };
                const auto dataset_overview = dataset_contract_overview_lines();
                lines.insert(lines.end(), dataset_overview.begin(), dataset_overview.end());
                const auto overview = multimodal_handoff_overview_lines();
                lines.insert(lines.end(), overview.begin(), overview.end());
                return lines;
            }
        }
    }

    static TorchTestProfile current_profile() {
#if TORCH_FULL_ENABLE_TRAIN_STAGE
        return TorchTestProfile::FullTrain;
#elif TORCH_FULL_ENABLE_TWOSTAGE_INFER
        return TorchTestProfile::FullImage;
#elif TORCH_FULL_ENABLE_DATASET_STAGE
        return TorchTestProfile::FullDataset;
#else
        return TorchTestProfile::FullAll;
#endif
    }
};

#endif
