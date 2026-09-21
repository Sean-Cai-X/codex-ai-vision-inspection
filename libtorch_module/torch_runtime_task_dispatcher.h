#pragma once

#include "torch_runtime_core.h"

TorchTaskResultCpp DispatchTorchRuntimeTask(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request);

TorchTaskResultCpp RunTorchCapabilitiesTask(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request);

TorchTaskResultCpp ValidateTorchSegmentationContract(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request);

TorchTaskResultCpp ValidateTorchDetectionContract(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request);

TorchTaskResultCpp ExecuteTorchSegmentationTask(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request);

TorchTaskResultCpp ExecuteTorchDetectionTask(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request);

TorchTaskResultCpp RunLegacyTorchTestHostTask(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request);
