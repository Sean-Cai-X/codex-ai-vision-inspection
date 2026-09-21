#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include <torch/torch.h>

#include "torch_segmentation_mainline_bridge.h"

namespace {

enum class SegmentationStageMode {
    Infer,
    Train,
    ExportCppStateDict,
    All
};

struct SegmentationStageOptions {
    SegmentationStageMode mode = SegmentationStageMode::All;
    std::string export_path;
};

SegmentationStageOptions parse_options(int argc, char** argv) {
    SegmentationStageOptions options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: libtorch_module_segmentation_stage_tests [--mode infer|train|all|export-cpp-state-dict] [--export-path path]\n";
            std::exit(0);
        }
        if (arg == "--mode") {
            TORCH_CHECK(i + 1 < argc, "--mode requires a value");
            const std::string value = argv[++i];
            if (value == "infer") {
                options.mode = SegmentationStageMode::Infer;
            } else if (value == "train") {
                options.mode = SegmentationStageMode::Train;
            } else if (value == "all") {
                options.mode = SegmentationStageMode::All;
            } else if (value == "export-cpp-state-dict") {
                options.mode = SegmentationStageMode::ExportCppStateDict;
            } else {
                TORCH_CHECK(false, "Unknown segmentation stage mode: ", value);
            }
            continue;
        }
        if (arg == "--export-path") {
            TORCH_CHECK(i + 1 < argc, "--export-path requires a value");
            options.export_path = argv[++i];
            continue;
        }
        TORCH_CHECK(false, "Unknown argument: ", arg);
    }
    return options;
}

const char* mode_name(SegmentationStageMode mode) {
    switch (mode) {
    case SegmentationStageMode::Infer:
        return "infer";
    case SegmentationStageMode::Train:
        return "train";
    case SegmentationStageMode::ExportCppStateDict:
        return "export-cpp-state-dict";
    case SegmentationStageMode::All:
    default:
        return "all";
    }
}

SegmentationMainlineRunnerConfig make_test_config() {
    auto config = make_segmentation_mainline_runner_config("deeplabv3plus", "mobilenet_v3_large", 3, 128, 2);
    if (const char* use_cuda = std::getenv("LIBTORCH_MODULE_USE_CUDA")) {
        const std::string value = use_cuda;
        if (value == "0") {
            config.device_policy = SegmentationDevicePolicy::ForceCPU;
            return config;
        }
        if (value == "1" && torch::cuda::is_available()) {
            config.device_policy = SegmentationDevicePolicy::ForceCUDA;
            return config;
        }
    }
    config.device_policy = torch::cuda::is_available() ? SegmentationDevicePolicy::ForceCUDA : SegmentationDevicePolicy::ForceCPU;
    return config;
}

torch::Tensor make_images(const SegmentationMainlineRunnerConfig& config) {
    const auto device = resolve_segmentation_device(config.device_policy);
    return torch::randn(
        {config.batch_size, 3, config.input_size, config.input_size},
        torch::TensorOptions().dtype(torch::kFloat32).device(device));
}

torch::Tensor make_masks(const SegmentationMainlineRunnerConfig& config) {
    const auto device = resolve_segmentation_device(config.device_policy);
    return torch::randint(
        0,
        config.num_classes,
        {config.batch_size, config.input_size, config.input_size},
        torch::TensorOptions().dtype(torch::kLong).device(device));
}

int run_segmentation_infer_stage() {
    try {
        auto config = make_test_config();
        config.enable_smoke_train = false;
        config.enable_eval = true;
        const auto eval = run_segmentation_eval_summary(make_images(config), make_masks(config), config);
        eval.validate();
        std::cout << "OK segmentation infer/eval, loss=" << eval.loss
                  << " foreground_iou=" << eval.foreground_iou
                  << " avg_confidence=" << eval.avg_confidence << "\n";
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "FAIL segmentation infer/eval: " << e.what() << "\n";
        return 1;
    }
}

int run_segmentation_train_stage() {
    try {
        auto config = make_test_config();
        config.enable_smoke_train = true;
        config.enable_eval = true;
        const auto smoke = run_segmentation_smoke_train_step(make_images(config), make_masks(config), config);
        smoke.validate();

        const auto session = run_segmentation_trainer_session(
            make_images(config), make_masks(config), make_images(config), make_masks(config), config);
        session.validate();
        const auto analysis = build_segmentation_trainer_analysis(session);
        analysis.validate();
        const auto unified = build_segmentation_unified_mainline_bundle(session.session, analysis);
        unified.validate();
        const auto summary = build_segmentation_unified_mainline_summary(unified);
        summary.validate();

        std::cout << "OK segmentation train, smoke_loss=" << smoke.loss
                  << " grad_mean=" << smoke.grad_mean
                  << " summary=" << summary.summary << "\n";
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "FAIL segmentation train: " << e.what() << "\n";
        return 1;
    }
}

int export_cpp_state_dict_stage(const std::string& export_path) {
    try {
        TORCH_CHECK(!export_path.empty(), "--export-path is required for export-cpp-state-dict mode");

        const std::filesystem::path output_path(export_path);
        if (!output_path.parent_path().empty()) {
            std::filesystem::create_directories(output_path.parent_path());
        }

        DeepLabV3Plus model("mobilenet_v3_large", 2);
        model->eval();
        torch::save(model, output_path.string());

        DeepLabV3Plus reload_model("mobilenet_v3_large", 2);
        torch::load(reload_model, output_path.string());
        reload_model->eval();

        torch::NoGradGuard no_grad;
        const auto smoke_input = torch::randn({1, 3, 64, 64}, torch::kFloat32);
        const auto output = reload_model->forward(smoke_input);
        TORCH_CHECK(output.count("out") == 1, "Exported DeepLabV3Plus state_dict reload missing out tensor");
        const auto out_sizes = output.at("out").sizes();
        TORCH_CHECK(out_sizes.size() == 4,
                    "Exported DeepLabV3Plus output must be NCHW");
        TORCH_CHECK(out_sizes[0] == smoke_input.size(0) &&
                    out_sizes[1] == 2 &&
                    out_sizes[2] == smoke_input.size(2) &&
                    out_sizes[3] == smoke_input.size(3),
                    "Exported DeepLabV3Plus output shape mismatch");

        std::cout << "OK exported cpp_state_dict deeplab smoke weight: "
                  << output_path.string() << "\n";
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "FAIL export cpp_state_dict deeplab smoke weight: " << e.what() << "\n";
        return 1;
    }
}

}

int main(int argc, char** argv) {
    const auto options = parse_options(argc, argv);
    const auto mode = options.mode;
    std::cout << "=================================\n";
    std::cout << " libtorch_module segmentation stage validation\n";
    std::cout << " active mode = " << mode_name(mode) << "\n";
    std::cout << "=================================\n";

    int failures = 0;
    if (mode == SegmentationStageMode::Infer) {
        failures = run_segmentation_infer_stage();
    } else if (mode == SegmentationStageMode::Train) {
        failures = run_segmentation_train_stage();
    } else if (mode == SegmentationStageMode::ExportCppStateDict) {
        failures = export_cpp_state_dict_stage(options.export_path);
    } else {
        failures += run_segmentation_infer_stage();
        failures += run_segmentation_train_stage();
    }

    if (failures == 0) {
        std::cout << "\nSEGMENTATION STAGE TESTS PASSED\n";
    } else {
        std::cerr << "\nSEGMENTATION STAGE TEST FAILURES: " << failures << "\n";
    }
    return failures;
}
