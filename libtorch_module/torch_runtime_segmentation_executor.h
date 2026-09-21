#pragma once

#include "torch_runtime_core.h"

TorchTaskResultCpp ExecuteTorchSegmentationTask(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request);