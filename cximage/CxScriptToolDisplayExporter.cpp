#include "CxScriptToolDisplayExporter.h"
#include <opencv2/opencv.hpp>

static void DrawMeasurePoints(
    cv::Mat& image,
    const std::vector<std::pair<double, double>>& pts)
{
    for (const auto& p : pts)
    {
        cv::circle(
            image,
            cv::Point(
                static_cast<int>(std::round(p.first)),
                static_cast<int>(std::round(p.second))),
            3,
            cv::Scalar(0, 0, 255),
            -1,
            cv::LINE_AA);
    }
}

static void DrawFindlineGeometry(
    cv::Mat& image,
    const CxScriptSuiteCaseResult& r)
{
    if (r.roi_x0 != 0 || r.roi_y0 != 0 || r.roi_x1 != 0 || r.roi_y1 != 0)
    {
        cv::line(
            image,
            cv::Point(r.roi_x0, r.roi_y0),
            cv::Point(r.roi_x1, r.roi_y1),
            cv::Scalar(0, 255, 0),
            2,
            cv::LINE_AA);
    }

    DrawMeasurePoints(image, r.measure_points_xy);

    if (r.has_fit_line)
    {
        cv::line(
            image,
            cv::Point(
                static_cast<int>(std::round(r.fit_line_x0)),
                static_cast<int>(std::round(r.fit_line_y0))),
            cv::Point(
                static_cast<int>(std::round(r.fit_line_x1)),
                static_cast<int>(std::round(r.fit_line_y1))),
            cv::Scalar(0, 255, 255),
            2,
            cv::LINE_AA);
    }
}

static void DrawFindCircleGeometry(
    cv::Mat& image,
    const CxScriptSuiteCaseResult& r)
{
    if (r.circle_cx != 0 || r.circle_cy != 0 || r.circle_px != 0 || r.circle_py != 0)
    {
        const double roiRadius =
            std::hypot(
                static_cast<double>(r.circle_px - r.circle_cx),
                static_cast<double>(r.circle_py - r.circle_cy));

        if (roiRadius > 1.0)
        {
            cv::circle(
                image,
                cv::Point(r.circle_cx, r.circle_cy),
                static_cast<int>(std::round(roiRadius)),
                cv::Scalar(0, 255, 0),
                2,
                cv::LINE_AA);
        }
    }

    DrawMeasurePoints(image, r.measure_points_xy);

    if (r.has_fit_circle && r.circle_radius > 0)
    {
        double centerX = r.circle_center_x;
        double centerY = r.circle_center_y;
        if (centerX == 0 && centerY == 0)
        {
            centerX = r.circle_cx;
            centerY = r.circle_cy;
        }
        cv::circle(
            image,
            cv::Point(
                static_cast<int>(std::round(centerX)),
                static_cast<int>(std::round(centerY))),
            static_cast<int>(std::round(r.circle_radius)),
            cv::Scalar(0, 255, 255),
            2,
            cv::LINE_AA);
    }
}

std::string CxScriptToolDisplayExporter::ExportToolDisplay(
    const std::string& original_path,
    const std::string& result_overlay_path,
    const std::string& evidence_overlay_path,
    const std::filesystem::path& output_path,
    const CxScriptSuiteCaseResult& result)
{
    cv::Mat original;
    cv::Mat resultOverlay;
    cv::Mat evidenceOverlay;
    cv::Mat gaugePreview;

    if (!original_path.empty())
        original = cv::imread(original_path);
    if (!result_overlay_path.empty())
        resultOverlay = cv::imread(result_overlay_path);
    if (!evidence_overlay_path.empty())
        evidenceOverlay = cv::imread(evidence_overlay_path);
    const std::filesystem::path gaugePreviewPath =
        output_path.parent_path() / "gauge_preview.png";
    if (std::filesystem::exists(gaugePreviewPath))
        gaugePreview = cv::imread(gaugePreviewPath.string());

    if (original.empty())
        return "";

    if (resultOverlay.empty())
        resultOverlay = original.clone();

    if (evidenceOverlay.empty())
        evidenceOverlay = original.clone();

    cv::Mat resultView = resultOverlay.clone();

    if (result.tool == "FindLine")
    {
        DrawFindlineGeometry(resultView, result);
    }
    else if (result.tool == "FindCircle")
    {
        DrawFindCircleGeometry(resultView, result);
    }

    if (result.headless_ok)
    {
        cv::putText(
            resultView,
            result.contract_pass ? "PASS" : "FAIL",
            {20, 40},
            cv::FONT_HERSHEY_SIMPLEX,
            1.0,
            result.contract_pass ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 0, 255),
            2,
            cv::LINE_AA);
    }

    else
    {
        cv::putText(
            resultView,
            "HEADLESS_FAILED",
            {20, 40},
            cv::FONT_HERSHEY_SIMPLEX,
            1.0,
            cv::Scalar(0, 0, 255),
            2,
            cv::LINE_AA);

        if (!result.failure_stage.empty())
        {
            cv::putText(
                resultView,
                result.failure_stage,
                {20, 80},
                cv::FONT_HERSHEY_SIMPLEX,
                0.7,
                cv::Scalar(0, 0, 255),
                2,
                cv::LINE_AA);
        }
    }

    if (!result.gauge_source.empty())
    {
        cv::putText(
            resultView,
            "GaugeSource=" + result.gauge_source,
            {20, 78},
            cv::FONT_HERSHEY_SIMPLEX,
            0.65,
            cv::Scalar(0, 255, 255),
            2,
            cv::LINE_AA);
        if (!result.gauge_review_status.empty())
        {
            cv::putText(
                resultView,
                "ReviewStatus=" + result.gauge_review_status,
                {20, 108},
                cv::FONT_HERSHEY_SIMPLEX,
                0.65,
                cv::Scalar(0, 255, 255),
                2,
                cv::LINE_AA);
        }
    }

    const int targetW = 480;

    auto resizeKeep = [](const cv::Mat& src, int width)
    {
        cv::Mat dst;
        const double scale = static_cast<double>(width) / src.cols;
        cv::resize(src, dst, cv::Size(width, static_cast<int>(src.rows * scale)));
        return dst;
    };

    cv::Mat a = resizeKeep(original, targetW);
    cv::Mat b = resizeKeep(resultView, targetW);
    cv::Mat c = resizeKeep(evidenceOverlay, targetW);
    cv::Mat d;
    if (!gaugePreview.empty())
        d = resizeKeep(gaugePreview, targetW);

    const int h = d.empty()
        ? std::max({a.rows, b.rows, c.rows})
        : std::max({a.rows, b.rows, c.rows, d.rows});

    auto padToHeight = [](const cv::Mat& src, int height)
    {
        if (src.rows == height)
            return src;

        cv::Mat dst(height, src.cols, src.type(), cv::Scalar(20, 20, 20));
        src.copyTo(dst(cv::Rect(0, 0, src.cols, src.rows)));
        return dst;
    };

    a = padToHeight(a, h);
    b = padToHeight(b, h);
    c = padToHeight(c, h);
    if (!d.empty())
        d = padToHeight(d, h);

    cv::putText(a, "Original", {20, 30}, cv::FONT_HERSHEY_SIMPLEX, 0.8, {0,255,0}, 2);
    cv::putText(b, "Result Overlay", {20, 30}, cv::FONT_HERSHEY_SIMPLEX, 0.8, {0,255,0}, 2);
    cv::putText(c, "Evidence Overlay", {20, 30}, cv::FONT_HERSHEY_SIMPLEX, 0.8, {0,255,0}, 2);
    if (!d.empty())
    {
        cv::putText(d, "Gauge Preview", {20, 30}, cv::FONT_HERSHEY_SIMPLEX, 0.8, {0,255,0}, 2);
        cv::putText(d, "GaugeSource=gauge_preview/manual_annotation", {20, 65},
                    cv::FONT_HERSHEY_SIMPLEX, 0.55, {0,255,255}, 2);
    }

    cv::Mat row;
    if (d.empty())
        cv::hconcat(std::vector<cv::Mat>{a, b, c}, row);
    else
        cv::hconcat(std::vector<cv::Mat>{a, b, c, d}, row);

    std::filesystem::create_directories(output_path.parent_path());
    cv::imwrite(output_path.string(), row);

    return output_path.string();
}
