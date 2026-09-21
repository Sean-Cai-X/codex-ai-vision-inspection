#include "CxScriptImagePreflight.h"
#include <opencv2/opencv.hpp>
#include <algorithm>

Stage25ImagePreflightResult Stage25ImagePreflight::Run(
    const std::string& image_id,
    const std::string& target_id,
    const std::string& tool,
    const std::string& level,
    const std::filesystem::path& imagePath,
    int x0, int y0, int x1, int y1,
    int wgap, int hgap,
    int gap, int linegap)
{
    Stage25ImagePreflightResult result;
    result.image_id = image_id;
    result.target_id = target_id;
    result.tool = tool;
    result.level = level;

    if (!std::filesystem::exists(imagePath))
    {
        result.preflight_class = "IMAGE_NOT_FOUND";
        return result;
    }

    cv::Mat img = cv::imread(imagePath.string());
    if (img.empty())
    {
        result.preflight_class = "IMAGE_LOAD_FAILED";
        return result;
    }

    result.image_loaded = true;
    result.image_width = img.cols;
    result.image_height = img.rows;

    cv::Mat gray;
    cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);

    int roi_x, roi_y, roi_w, roi_h;

    if (tool == "FindLine")
    {
        int min_x = std::min(x0, x1) - wgap;
        int max_x = std::max(x0, x1) + wgap;
        int min_y = std::min(y0, y1) - hgap;
        int max_y = std::max(y0, y1) + hgap;

        roi_x = std::max(0, min_x);
        roi_y = std::max(0, min_y);
        roi_w = std::min(result.image_width - roi_x, max_x - min_x);
        roi_h = std::min(result.image_height - roi_y, max_y - min_y);

        result.roi_inside_image =
            (min_x >= 0 && max_x <= result.image_width &&
             min_y >= 0 && max_y <= result.image_height);
    }
    else if (tool == "FindCircle")
    {
        double radius = std::sqrt((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
        int min_x = static_cast<int>(x0 - radius - gap - linegap);
        int max_x = static_cast<int>(x0 + radius + gap + linegap);
        int min_y = static_cast<int>(y0 - radius - gap - linegap);
        int max_y = static_cast<int>(y0 + radius + gap + linegap);

        roi_x = std::max(0, min_x);
        roi_y = std::max(0, min_y);
        roi_w = std::min(result.image_width - roi_x, max_x - min_x);
        roi_h = std::min(result.image_height - roi_y, max_y - min_y);

        result.roi_inside_image =
            (min_x >= 0 && max_x <= result.image_width &&
             min_y >= 0 && max_y <= result.image_height);
    }
    else
    {
        result.preflight_class = "UNKNOWN_TOOL";
        return result;
    }

    result.roi_x = roi_x;
    result.roi_y = roi_y;
    result.roi_w = roi_w;
    result.roi_h = roi_h;

    if (!result.roi_inside_image)
    {
        result.preflight_class = "ROI_OUT_OF_IMAGE";
        result.roi_valid = false;
        return result;
    }

    cv::Mat roi_gray = gray(cv::Rect(roi_x, roi_y, roi_w, roi_h));

    cv::Scalar mean, stddev;
    cv::meanStdDev(roi_gray, mean, stddev);
    result.gray_mean = mean[0];
    result.gray_std = stddev[0];

    cv::Mat sobel_x, sobel_y;
    cv::Sobel(roi_gray, sobel_x, CV_64F, 1, 0, 3);
    cv::Sobel(roi_gray, sobel_y, CV_64F, 0, 1, 3);

    cv::Mat gradient_mag;
    cv::magnitude(sobel_x, sobel_y, gradient_mag);

    cv::Scalar grad_mean, grad_std;
    cv::meanStdDev(gradient_mag, grad_mean, grad_std);
    result.gradient_mean = grad_mean[0];
    result.blur_score = grad_mean[0];

    std::vector<double> grad_values;
    gradient_mag.reshape(1, 1).copyTo(grad_values);
    std::sort(grad_values.begin(), grad_values.end());

    if (!grad_values.empty())
    {
        result.gradient_max = grad_values.back();
        size_t p90_idx = static_cast<size_t>(grad_values.size() * 0.9);
        result.gradient_p90 = grad_values[std::min(p90_idx, grad_values.size() - 1)];
    }

    int saturation_low = cv::countNonZero(roi_gray <= 5);
    int saturation_high = cv::countNonZero(roi_gray >= 250);
    int total_pixels = roi_gray.total();

    result.saturation_low_ratio = static_cast<double>(saturation_low) / total_pixels;
    result.saturation_high_ratio = static_cast<double>(saturation_high) / total_pixels;

    if (result.gradient_p90 < 8.0)
        result.warnings.push_back("LOW_GRADIENT_WARNING");

    if (result.gray_std < 5.0)
        result.warnings.push_back("LOW_CONTRAST_WARNING");

    if (result.saturation_high_ratio > 0.05)
        result.warnings.push_back("SATURATION_WARNING");

    if (result.blur_score < 40.0)
        result.warnings.push_back("BLUR_WARNING");

    if (result.warnings.empty())
        result.preflight_class = "OK";
    else
        result.preflight_class = "WARNING";

    result.roi_valid = true;
    return result;
}

bool ShouldRunCaseAfterPreflight(
    const Stage25ImagePreflightResult& preflight,
    std::string& reason)
{
    if (!preflight.image_loaded)
    {
        reason = "image load failed";
        return false;
    }

    if (!preflight.roi_valid)
    {
        reason = "roi invalid";
        return false;
    }

    if (!preflight.roi_inside_image)
    {
        reason = "roi outside image";
        return false;
    }

    reason.clear();
    return true;
}