#pragma once

#include <string>

namespace cx_torch_mainline {

struct CxTorchWeightSet {
    std::string yolo_weight_ref;
    std::string mobilevit_weight_ref;
    std::string deeplab_weight_ref;
    std::string resnet50_weight_ref;
};

struct CxTorchModuleBoundary {
    std::string segmentation_engine_target;
    std::string libtorch_module_target;
    std::string mainline_adapter_target;
};

inline CxTorchWeightSet DefaultWeightNames() {
    return CxTorchWeightSet{
        "yolov8n_dict.pt",
        "mobilevitv2_weights.pt",
        "deeplabv3_mobilenet_v3_large-fc3c493d.pth",
        "resnet50_weights.pt",
    };
}

inline CxTorchModuleBoundary DefaultModuleBoundary() {
    return CxTorchModuleBoundary{
        "segmentation",
        "cx_libtorch_module_baseline",
        "cx_torch_mainline",
    };
}

}  // namespace cx_torch_mainline
