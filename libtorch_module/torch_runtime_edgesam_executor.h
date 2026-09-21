#pragma once

#include "torch_runtime_core.h"

TorchTaskResultCpp ExecuteTorchEdgeSamTask(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request);
