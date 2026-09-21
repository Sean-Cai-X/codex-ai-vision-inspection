#include "FindSegmentationOpenCvSmokeBackend.h"

bool FindSegmentationOpenCvSmokeBackend::Run(
    const FindSegmentationInput& input,
    FindSegmentationResult& output,
    std::string& reason)
{
    if (input.image.empty())
    {
        reason = "input image is empty";
        return false;
    }

    cv::Mat bgr = input.image;
    cv::Mat gray;
    if (bgr.channels() == 3)
        cv::cvtColor(bgr, gray, cv::COLOR_BGR2GRAY);
    else
        gray = bgr.clone();

    cv::Rect roi(0, 0, gray.cols, gray.rows);
    if (input.has_rect)
    {
        roi = input.rect & cv::Rect(0, 0, gray.cols, gray.rows);
        if (roi.width <= 0 || roi.height <= 0)
        {
            reason = "prompt rect outside image";
            return false;
        }
    }

    cv::Mat work = cv::Mat::zeros(gray.size(), CV_8UC1);
    cv::Mat roiGray = gray(roi);

    cv::Mat blurred;
    cv::GaussianBlur(roiGray, blurred, cv::Size(3, 3), 0);

    cv::Mat edges;
    cv::Canny(blurred, edges, 50, 150);

    edges.copyTo(work(roi));

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(work, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    output.mask = cv::Mat::zeros(gray.size(), CV_8UC1);
    output.contours.clear();

    double bestArea = 0.0;
    int bestIndex = -1;

    for (int i = 0; i < static_cast<int>(contours.size()); ++i)
    {
        double area = std::abs(cv::contourArea(contours[i]));
        if (area < 10.0)
            continue;

        FindSegmentationContour c;
        c.points = contours[i];
        c.area = area;
        c.perimeter = cv::arcLength(contours[i], true);

        output.contours.push_back(c);
    }

    // This is a deterministic OpenCV baseline, not EdgeSAM. Prompt polarity
    // is nevertheless applied so the UI/Headless chain can be verified:
    // a negative point excludes its component and a positive point selects
    // its containing component.
    for (int i = 0; i < static_cast<int>(output.contours.size()); ++i)
    {
        const std::vector<cv::Point>& contour = output.contours[i].points;
        if (input.has_negative_point &&
            cv::pointPolygonTest(contour, input.negative_point, false) >= 0.0)
            continue;
        if (input.has_positive_point &&
            cv::pointPolygonTest(contour, input.positive_point, false) < 0.0)
            continue;
        if (output.contours[i].area > bestArea)
        {
            bestArea = output.contours[i].area;
            bestIndex = i;
        }
    }

    if (bestIndex >= 0)
    {
        FindSegmentationContour selected = output.contours[bestIndex];
        output.contours.clear();
        output.contours.push_back(std::move(selected));
        bestIndex = 0;
    }
    else
    {
        // A rejected positive/negative prompt must not leak unrelated Canny
        // contours into the capture/overlay result.  No selected component
        // means no segmentation boundary is available for this request.
        output.contours.clear();
    }

    if (bestIndex >= 0)
    {
        std::vector<std::vector<cv::Point>> drawContours;
        drawContours.push_back(output.contours[bestIndex].points);
        cv::drawContours(output.mask, drawContours, 0, cv::Scalar(255), cv::FILLED);
        output.primary_area = output.contours[bestIndex].area;
    }

    output.overlay = bgr.clone();
    for (const auto& c : output.contours)
    {
        std::vector<std::vector<cv::Point>> one;
        one.push_back(c.points);
        cv::drawContours(output.overlay, one, 0, cv::Scalar(0, 255, 255), 2);
    }

    output.ok = bestIndex >= 0;
    output.backend = "opencv_smoke";
    output.backend_status = "smoke_ready";
    output.status = output.ok ? "boundary_available" : "no_boundary";
    output.reason = output.ok
        ? "opencv smoke boundary generated; prompt polarity applied"
        : (input.has_positive_point
            ? "no boundary contains the positive prompt"
            : "no contour found");
    output.mask_width = output.mask.cols;
    output.mask_height = output.mask.rows;
    output.contour_count = static_cast<int>(output.contours.size());

    reason = output.reason;
    return output.ok;
}
