#pragma once

#include "FindSegmentationBackend.h"

class FindSegmentationOpenCvSmokeBackend : public IFindSegmentationBackend
{
public:
    bool Run(
        const FindSegmentationInput& input,
        FindSegmentationResult& output,
        std::string& reason) override;
};