#ifndef CXIMAGE_CXTORCH_RESULT_PROJECTOR_H
#define CXIMAGE_CXTORCH_RESULT_PROJECTOR_H

#include "CxExecutionTypes.h"
#include "CxRuntimeProjectionTypes.h"
#include "FastMatchTransform.h"

class CxTorchResultProjector
{
public:
    static bool Project(
        const CxInferenceResult& inference_result,
        const std::string& owner_type,
        const std::string& owner_ref,
        std::vector<CxShapeElementSnapshot>& shapes);

    // Selects the highest-confidence explicitly oriented Torch detection and
    // converts it to FastMatch's value-only seed.  It deliberately rejects a
    // plain axis-aligned bbox rather than inventing an OBB angle.
    static bool TryBuildFastMatchTransform(
        const CxInferenceResult& inference_result,
        FastMatchTransform& transform,
        int& detection_index,
        int& class_id,
        double& confidence,
        std::string& angle_convention,
        std::string& reason);

private:
    static void ProjectDetections(
        const std::vector<CxTorchDetection>& detections,
        const std::string& owner_type,
        const std::string& owner_ref,
        std::vector<CxShapeElementSnapshot>& shapes);

    static void ProjectMask(
        const CxTorchMask& mask,
        const std::string& owner_type,
        const std::string& owner_ref,
        std::vector<CxShapeElementSnapshot>& shapes);
};

#endif
