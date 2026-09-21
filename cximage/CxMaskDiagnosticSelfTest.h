#pragma once

#include <filesystem>
#include <string>

struct CxImageReferenceCandidateCliOptions
{
    std::filesystem::path image_path;
    std::filesystem::path output_dir;
    std::string algorithm_id = "find_segmentation_opencv_smoke";
    double threshold = 0.5;
    bool has_roi = false;
    int roi_x0 = 0;
    int roi_y0 = 0;
    int roi_x1 = 0;
    int roi_y1 = 0;
};

int RunCxMaskDiagnosticSelfTest(const std::filesystem::path& out_dir);
int RunCxImageReferenceCandidateCli(const CxImageReferenceCandidateCliOptions& options);
