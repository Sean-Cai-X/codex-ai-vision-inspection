#ifndef CXIMAGE_CXRUNTIME_PROJECTION_EXECUTOR_H
#define CXIMAGE_CXRUNTIME_PROJECTION_EXECUTOR_H

#include "ICxRuntimeProjectionExecutor.h"
#include <functional>
#include <unordered_map>

class CxRuntimeProjectionExecutor : public ICxRuntimeProjectionExecutor
{
public:
    using ProjectionHandler = std::function<bool(
        const CxRuntimeProjectionRequest&,
        ImageAnnotationLayer&,
        CxRuntimeProjectionResult&)>;

    CxRuntimeProjectionExecutor();

    void Register(const std::string& tool_id, const ProjectionHandler& handler);

    bool Execute(
        const CxRuntimeProjectionRequest& request,
        ImageAnnotationLayer& output_layer,
        CxRuntimeProjectionResult& result) override;

private:
    std::unordered_map<std::string, ProjectionHandler> m_handlers;
};

#endif