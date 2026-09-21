#include <cstdlib>
#include <iostream>
#include <string>

#include "torch_alltest_2.h"

namespace {

enum class MobileViTStageMode {
    Infer,
    Train,
    All
};

MobileViTStageMode parse_mode(int argc, char** argv) {
    MobileViTStageMode mode = MobileViTStageMode::All;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: libtorch_module_mobilevit_stage_tests [--mode infer|train|all]\n";
            std::exit(0);
        }
        if (arg == "--mode") {
            TORCH_CHECK(i + 1 < argc, "--mode requires a value");
            const std::string value = argv[++i];
            if (value == "infer") {
                mode = MobileViTStageMode::Infer;
            } else if (value == "train") {
                mode = MobileViTStageMode::Train;
            } else if (value == "all") {
                mode = MobileViTStageMode::All;
            } else {
                TORCH_CHECK(false, "Unknown MobileViT stage mode: ", value);
            }
            continue;
        }
        TORCH_CHECK(false, "Unknown argument: ", arg);
    }
    return mode;
}

const char* mode_name(MobileViTStageMode mode) {
    switch (mode) {
    case MobileViTStageMode::Infer:
        return "infer";
    case MobileViTStageMode::Train:
        return "train";
    case MobileViTStageMode::All:
    default:
        return "all";
    }
}

int run_mobilevit_infer_stage() {
    int failures = 0;
    failures += test_MobileViTv2_Shape();
#if TORCH_FULL_ENABLE_DATASET_STAGE && TORCH_FULL_ENABLE_MOBILEVIT_DATASET
    failures += test_MobileViTv2_DatasetSmoke();
#endif
#if TORCH_FULL_ENABLE_TWOSTAGE_INFER
    failures += test_YOLOv8_MobileViTv2_TwoStageInference();
#endif
    return failures;
}

int run_mobilevit_train_stage() {
    int failures = 0;
    failures += test_MobileViTv2_Shape();
#if TORCH_FULL_ENABLE_MOBILEVIT_TRAIN
    failures += test_MobileViTv2_Train();
    failures += test_MobileViTv2_MainlineSession();
#endif
#if TORCH_FULL_ENABLE_TWOSTAGE_TRAIN
    failures += test_YOLOv8_MobileViTv2_TwoStageTrainMock();
#endif
    return failures;
}

}

int main(int argc, char** argv) {
    const auto mode = parse_mode(argc, argv);
    std::cout << "=================================\n";
    std::cout << " libtorch_module MobileViT stage validation\n";
    std::cout << " active mode = " << mode_name(mode) << "\n";
    std::cout << "=================================\n";

    int failures = 0;
    if (mode == MobileViTStageMode::Infer) {
        failures = run_mobilevit_infer_stage();
    } else if (mode == MobileViTStageMode::Train) {
        failures = run_mobilevit_train_stage();
    } else {
        failures = run_mobilevit_tests();
    }

    if (failures == 0) {
        std::cout << "\nMOBILEVIT STAGE TESTS PASSED\n";
    } else {
        std::cerr << "\nMOBILEVIT STAGE TEST FAILURES: " << failures << "\n";
    }
    return failures;
}