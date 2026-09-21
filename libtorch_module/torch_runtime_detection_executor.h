#pragma once

#include "torch_runtime_core.h"

TorchTaskResultCpp ExecuteTorchDetectionTask(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request);