#pragma once

#include "torch_runtime_core.h"
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace TorchRuntimeTaskIds
{
inline constexpr const char* Capabilities =
    "torch.runtime.capabilities.v1";

inline constexpr const char* SegmentationContract =
    "torch.contract.segmentation.v1";

inline constexpr const char* DetectionContract =
    "torch.contract.detection.v1";

inline constexpr const char* DeepLabV3PlusSegmentation =
    "torch.infer.segmentation.deeplabv3plus.v1";

inline constexpr const char* EdgeSamPromptSegmentation =
    "torch.infer.segmentation.edgesam.v1";

inline constexpr const char* EdgeSamIncrementalPackage =
    "torch.train.segmentation.edgesam.decoder.v1";

inline constexpr const char* YoloV8IncrementalPackage =
    "torch.train.detection.yolov8.package.v1";

inline constexpr const char* YoloV8TrainingLifecycle =
    "torch.train.detection.yolov8.lifecycle.v1";

inline constexpr const char* PrototypeIncrementalLifecycle =
    "torch.incremental.prototype.lifecycle.v1";

inline constexpr const char* YoloV8Detection =
    "torch.infer.detection.yolov8.v1";

inline constexpr const char* YoloV8InstanceSegmentation =
    "torch.infer.instance_segmentation.yolov8.v1";

inline constexpr const char* YoloV8SegBackwardSmoke =
    "torch.train.instance_segmentation.yolov8.backward_smoke.v1";

inline constexpr const char* SegmentationTrainingLifecycle =
    "torch.train.segmentation.lifecycle_smoke.v1";

// Business trials are a separate, opt-in capability.  They are not an alias
// for the smoke task and are never a production activation capability.
inline constexpr const char* SegmentationBusinessIncremental =
    "torch.train.segmentation.business_incremental.v1";
}

enum class TorchProductionTaskKind
{
    Unknown = 0,
    RuntimeCapabilities,
    SegmentationContract,
    DetectionContract,
    SegmentationInference,
    DetectionInference,
    LegacyTestHost
};

enum class TorchRuntimeErrorCode
{
    Ok = 0,

    InvalidRequest = 1001,
    UnsupportedTask = 1002,

    InputImageMissing = 1101,
    InputImageUnreadable = 1102,
    InputImageInvalid = 1103,

    ManifestMissing = 1201,
    ManifestInvalid = 1202,
    ManifestSchemaMismatch = 1203,

    ModelFileMissing = 1301,
    ModelNotFound = 1302,
    WeightFormatUnsupported = 1303,
    WeightLoadFailed = 1304,
    ModelLoadFailed = 1305,

    DeviceUnavailable = 1401,
    DeviceFallback = 1402,

    PreprocessFailed = 1501,
    ForwardFailed = 1601,
    TorchRuntimeError = 1602,
    PostprocessFailed = 1701,

    OutputPathInvalid = 1801,
    ArtifactWriteFailed = 1802,
    ResultSerializeFailed = 1901
};

struct TorchRuntimeTaskContext
{
    TorchRuntimeCoreConfig config;
    TorchTaskRequestCpp request;

    TorchProductionTaskKind kind =
        TorchProductionTaskKind::Unknown;

    std::filesystem::path output_dir;
};

struct TorchRuntimeDetection
{
    int class_id = -1;
    std::string class_name;

    float confidence = 0.0f;

    float x1 = 0.0f;
    float y1 = 0.0f;
    float x2 = 0.0f;
    float y2 = 0.0f;
};

struct TorchRuntimeSegmentationResult
{
    bool mask_available = false;

    int width = 0;
    int height = 0;
    int target_class_id = -1;

    std::uint64_t foreground_pixels = 0;
    double foreground_ratio = 0.0;

    int contour_count = 0;

    std::string label_mask_path;
    std::string binary_mask_path;
    std::string overlay_path;
    std::string contours_path;
};

struct TorchRuntimeDetectionResult
{
    std::vector<TorchRuntimeDetection> detections;

    std::string detections_path;
    std::string overlay_path;
    std::string candidate_list_path;
};
