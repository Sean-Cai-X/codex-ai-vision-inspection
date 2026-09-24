#pragma once

#include "torch_runtime_core.h"

#include <functional>

// This is an internal, typed progress boundary for the local offline business
// service.  It contains only numeric optimizer evidence; dataset paths and
// image data remain inside the local broker/runtime process.
struct TorchYoloV8SegBusinessEpoch
{
    unsigned epoch = 0;
    unsigned completed_iterations = 0;
    double training_loss = 0.0;
    double validation_score = 0.0;
    bool has_validation_score = false;
    double effective_learning_rate = 0.0;
};

using TorchYoloV8SegBusinessEpochSink =
    std::function<bool(unsigned progress_percent,
                       const TorchYoloV8SegBusinessEpoch&)>;
using TorchYoloV8SegBusinessCancelProbe = std::function<bool()>;

TorchTaskResultCpp ExecuteTorchYoloV8SegTask(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request);

TorchTaskResultCpp ExecuteTorchYoloV8SegBackwardSmokeTask(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request);

// Deliberately independent from the generic dispatcher and the backward smoke
// task.  Only the local business service invokes it after it has resolved a
// controlled asset binding.  The handler preserves the fixed seven-class
// ontology even when the selected project supplies one class only.
TorchTaskResultCpp ExecuteTorchYoloV8SegBusinessTrialTask(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request,
    const TorchYoloV8SegBusinessEpochSink& on_epoch,
    const TorchYoloV8SegBusinessCancelProbe& is_cancelled);

// Business inference is a separate typed entry.  Its raw result remains in
// the local service process and must be filtered before any UI/bridge receipt.
TorchTaskResultCpp ExecuteTorchYoloV8SegBusinessInferenceTask(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request);
