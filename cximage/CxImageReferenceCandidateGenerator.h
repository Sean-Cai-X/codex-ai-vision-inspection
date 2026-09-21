#pragma once

#include "CxExecutionTypes.h"

#include <filesystem>
#include <string>

struct CxImageReferenceCandidateRequest
{
    std::string algorithm_id;
    std::filesystem::path input_image_path;
    std::filesystem::path output_dir;
    double threshold = 0.5;
    bool has_roi = false;
    int roi_x0 = 0;
    int roi_y0 = 0;
    int roi_x1 = 0;
    int roi_y1 = 0;
    bool has_positive_point = false;
    int positive_x = 0;
    int positive_y = 0;
    bool has_negative_point = false;
    int negative_x = 0;
    int negative_y = 0;
};

struct CxImageReferenceCandidateResult
{
    bool executed = false;
    bool ok = false;
    std::string status = "NOT_RUN";
    std::string reason;
    std::string provenance;
    std::string mask_ref;
    std::string overlay_ref;
    std::string instances_ref;
    std::string summary_ref;
    CxMaskFactsSnapshot mask_facts;
};

class CxImageReferenceCandidateGenerator
{
public:
    bool Generate(
        const CxImageReferenceCandidateRequest& request,
        CxImageReferenceCandidateResult& result,
        std::string& reason) const;
};
