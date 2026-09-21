#pragma once

#include "CxScriptImageManifestRuntime.h"
#include "CxShapeTestRuntime.h"
#include "CxRuntimeProjectionTypes.h"

bool ResolveManifestProjectionRequest(
    const CxScriptImageManifestRuntime& manifest,
    const CxShapeTestCase& test_case,
    CxRuntimeProjectionRequest& out_request,
    std::string& out_reason);