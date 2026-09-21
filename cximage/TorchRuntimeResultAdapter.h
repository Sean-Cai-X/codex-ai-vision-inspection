#pragma once

#include "TorchRuntimeTypes.h"
#include "CxExecutionTypes.h"
#include <vector>

struct TorchRuntimeStageRef
{
    std::string stage_id;
    std::string stage_name;
    std::string stage_status;
};

struct TorchRuntimeImageRef
{
    std::string ref_id;
    std::string file_path;
    std::string label;
};

struct TorchRuntimeResultField
{
    std::string name;
    std::string value;
    std::string unit;
};

struct TorchRuntimeGuiReview
{
    std::vector<TorchRuntimeStageRef> stages;
    std::vector<TorchRuntimeImageRef> image_refs;
    std::vector<TorchRuntimeResultField> result_fields;
    std::vector<std::string> issue_refs;
};

class TorchRuntimeResultAdapter
{
public:
    static bool AdaptToInferenceResult(
        const TorchRuntimeGuiResult& source,
        const CxTorchTaskSpec& task,
        CxInferenceResult& target,
        std::string& reason);

    static TorchRuntimeGuiReview AdaptToGuiReview(const CxInferenceResult& result);
    static TorchRuntimeGuiReview Adapt(const TorchRuntimeGuiResult& result);
};