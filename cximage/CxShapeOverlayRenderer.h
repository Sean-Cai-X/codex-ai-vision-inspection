#ifndef CXIMAGE_CX_SHAPE_OVERLAY_RENDERER_H
#define CXIMAGE_CX_SHAPE_OVERLAY_RENDERER_H

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

struct CxShapeElementSnapshot;

enum class CxOverlayLayer
{
    EVIDENCE,
    RESULT,
    TOOL_DISPLAY
};

struct CxOverlayRenderResult
{
    bool ok = false;

    int rendered_element_count = 0;
    int rendered_roi_count = 0;
    int rendered_scan_count = 0;
    int rendered_measure_points_count = 0;
    int rendered_result_count = 0;

    int changed_pixel_count = 0;
    cv::Rect changed_bbox;

    std::string reason;
};

bool RenderCxShapeOverlay(
    const cv::Mat& source,
    const std::vector<CxShapeElementSnapshot>& shapes,
    CxOverlayLayer layer,
    cv::Mat& output,
    CxOverlayRenderResult& result);

#endif