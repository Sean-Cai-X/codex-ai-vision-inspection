#ifndef CXIMAGE_ICXRUNTIME_PROJECTION_EXECUTOR_H
#define CXIMAGE_ICXRUNTIME_PROJECTION_EXECUTOR_H

#include "CxRuntimeProjectionTypes.h"
#include "ImageAnnotationLayer.h"

class ICxRuntimeProjectionExecutor
{
public:
    virtual ~ICxRuntimeProjectionExecutor() = default;

    virtual bool Execute(
        const CxRuntimeProjectionRequest& request,
        ImageAnnotationLayer& output_layer,
        CxRuntimeProjectionResult& result) = 0;
};

#endif