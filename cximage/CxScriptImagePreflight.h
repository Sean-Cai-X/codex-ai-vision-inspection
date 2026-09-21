#ifndef CXIMAGE_CXSCRIPT_IMAGE_PREFLIGHT_H
#define CXIMAGE_CXSCRIPT_IMAGE_PREFLIGHT_H

#include <string>
#include <vector>
#include <filesystem>

struct Stage25ImagePreflightResult
{
    std::string image_id;
    std::string target_id;
    std::string tool;
    std::string level;

    bool image_loaded = false;
    bool roi_valid = false;
    bool roi_inside_image = false;

    int image_width = 0;
    int image_height = 0;

    int roi_x = 0;
    int roi_y = 0;
    int roi_w = 0;
    int roi_h = 0;

    double gray_mean = 0.0;
    double gray_std = 0.0;

    double gradient_mean = 0.0;
    double gradient_p90 = 0.0;
    double gradient_max = 0.0;

    double saturation_low_ratio = 0.0;
    double saturation_high_ratio = 0.0;

    double blur_score = 0.0;

    std::string preflight_class;
    std::vector<std::string> warnings;
};

class Stage25ImagePreflight
{
public:
    static Stage25ImagePreflightResult Run(
        const std::string& image_id,
        const std::string& target_id,
        const std::string& tool,
        const std::string& level,
        const std::filesystem::path& imagePath,
        int x0, int y0, int x1, int y1,
        int wgap = 8, int hgap = 32,
        int gap = 5, int linegap = 3);
};

bool ShouldRunCaseAfterPreflight(
    const Stage25ImagePreflightResult& preflight,
    std::string& reason);

#endif