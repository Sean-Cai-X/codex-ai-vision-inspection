#pragma once

#include "torch_runtime_core.h"

TorchTaskResultCpp ExecuteTorchYoloV8SegTask(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request);

TorchTaskResultCpp ExecuteTorchYoloV8SegBackwardSmokeTask(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request);
