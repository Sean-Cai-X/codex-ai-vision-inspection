#include "pch.h"

#include "FindLine.h"
#include "LineGaugeShape.h"
#include "PolylineShape.h"
#include "ImageAnnotationLayer.h"
#include "CxUnifiedLog.h"
#include "../cxgeom/include/CxSetLineBuild.h"
#include "occtinclude.h"
#include "imagemanager.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <sstream>
#include <vector>


#include <opencv2/opencv.hpp>
#include <opencv2/core/version.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/video/tracking.hpp>


#if defined USE_AI
#include "mlpackrun.h"
#endif

namespace
{
void LogFindlineMeasureProbe(
    const char* phase,
    const char* status,
    const std::string& message)
{
    CXLOG_INFO("FindLine", phase, status, message);
    CxUnifiedLog::Instance().Flush();
}

std::string FindlineMeasureMessage(
    const char* detail,
    int image_w,
    int image_h,
    int image_channels,
    int roi_x0,
    int roi_y0,
    int roi_x1,
    int roi_y1,
    int half_width,
    int scan_w,
    int scan_h,
    int process_w,
    int back_w,
    int back_h)
{
    return std::string("detail=") + detail +
        ", image=" + std::to_string(image_w) + "x" + std::to_string(image_h) +
        "x" + std::to_string(image_channels) +
        ", roi=(" + std::to_string(roi_x0) + "," + std::to_string(roi_y0) +
        ")->(" + std::to_string(roi_x1) + "," + std::to_string(roi_y1) + ")" +
        ", half_width=" + std::to_string(half_width) +
        ", scan_w_count=" + std::to_string(scan_w) +
        ", scan_h_count=" + std::to_string(scan_h) +
        ", process_w=" + std::to_string(process_w) +
        ", back=" + std::to_string(back_w) + "x" + std::to_string(back_h);
}

int ClampSizeToInt(std::size_t value)
{
    const std::size_t max_value = static_cast<std::size_t>(std::numeric_limits<int>::max());
    return value > max_value ? std::numeric_limits<int>::max() : static_cast<int>(value);
}

int RoundToInt(double value)
{
    if (!std::isfinite(value))
        return 0;
    const double clamped = std::min(
        std::max(value, static_cast<double>(std::numeric_limits<int>::min())),
        static_cast<double>(std::numeric_limits<int>::max()));
    return static_cast<int>(std::lround(clamped));
}

void RotatePointAround(gp_Pnt& point, double center_x, double center_y, double angle_radians)
{
    if (!std::isfinite(angle_radians) || std::abs(angle_radians) <= 1.0e-12)
        return;

    const double dx = point.X() - center_x;
    const double dy = point.Y() - center_y;
    const double c = std::cos(angle_radians);
    const double s = std::sin(angle_radians);
    point.SetX(center_x + dx * c - dy * s);
    point.SetY(center_y + dx * s + dy * c);
}

template <typename EnumT>
EnumT ClampEnumInt(int value, int min_value, int max_value, EnumT fallback)
{
    if (value < min_value || value > max_value)
        return fallback;
    return static_cast<EnumT>(value);
}

int ClampPermille(int value)
{
    return std::max(0, std::min(1000, value));
}

std::vector<float> BuildFindLineGrayProfile(
    const cv::Mat& gray,
    LineShape& scan_line,
    int line_length)
{
    std::vector<float> profile;
    if (gray.empty() || line_length <= 0)
        return profile;

    profile.reserve(static_cast<std::size_t>(line_length));
    for (int i = 0; i < line_length; ++i)
    {
        const gp_Pnt point = scan_line.getlinepoint(i);
        const int x = static_cast<int>(std::lround(point.X()));
        const int y = static_cast<int>(std::lround(point.Y()));
        if (x < 0 || x >= gray.cols || y < 0 || y >= gray.rows)
        {
            profile.push_back(0.0f);
            continue;
        }
        profile.push_back(static_cast<float>(gray.at<uchar>(y, x)));
    }
    return profile;
}

gp_Pnt InterpolateFindLinePoint(LineShape& scan_line,
                                double position_samples,
                                int line_length)
{
    if (!std::isfinite(position_samples) || line_length <= 1)
        return scan_line.getlinepoint(0);

    const double clamped = std::max(
        0.0,
        std::min(position_samples, static_cast<double>(line_length - 1)));
    const int lo = static_cast<int>(std::floor(clamped));
    const int hi = std::min(line_length - 1, lo + 1);
    const double t = clamped - static_cast<double>(lo);

    const gp_Pnt p0 = scan_line.getlinepoint(lo);
    const gp_Pnt p1 = scan_line.getlinepoint(hi);
    gp_Pnt out;
    out.SetX(p0.X() + (p1.X() - p0.X()) * t);
    out.SetY(p0.Y() + (p1.Y() - p0.Y()) * t);
    out.SetZ(p0.Z() + (p1.Z() - p0.Z()) * t);
    return out;
}

int ClampLongLongToInt(long long value)
{
    const long long min_value = static_cast<long long>(std::numeric_limits<int>::min());
    const long long max_value = static_cast<long long>(std::numeric_limits<int>::max());
    return static_cast<int>(std::min(std::max(value, min_value), max_value));
}

FindLineMeasureInputDebug::EdgeEvaluation& EnsureFindLineEdgeEvaluation(
    FindLineMeasureInputDebug& debug,
    int edge_index)
{
    if (edge_index < 1)
        edge_index = 1;

    const std::size_t required =
        static_cast<std::size_t>(edge_index + 1);
    if (debug.edge_evaluations.size() < required)
        debug.edge_evaluations.resize(required);

    auto& eval = debug.edge_evaluations[static_cast<std::size_t>(edge_index)];
    eval.edge_index = edge_index;
    return eval;
}

void RecordFindLineEdgeCandidate(
    FindLineMeasureInputDebug& debug,
    int edge_index,
    bool accepted,
    bool rejected_by_selection,
    bool rejected_near_endpoint,
    bool over_length)
{
    auto& eval = EnsureFindLineEdgeEvaluation(debug, edge_index);
    ++eval.candidate_scan_rows;
    if (accepted)
        ++eval.accepted_points;
    if (rejected_by_selection)
        ++eval.rejected_by_selection;
    if (rejected_near_endpoint)
        ++eval.rejected_near_endpoint;
    if (over_length)
        ++eval.over_length_runs;
}

void RecordFindLineAcceptedDiagnosticPoint(
    FindLineMeasureInputDebug::ScanDiagnostic& diag,
    const gp_Pnt& point)
{
    diag.accepted = true;
    diag.accepted_x = point.X();
    diag.accepted_y = point.Y();
    diag.accepted_points_xy.push_back(point.X());
    diag.accepted_points_xy.push_back(point.Y());
    diag.reject_reason.clear();
}

bool ShouldLogFindLineFocusedScan(int scan_index)
{
    return scan_index >= 18 && scan_index <= 26;
}

void LogFindLineFocusedScan(
    const char* phase,
    const LineShape& scan_line,
    const FindLineMeasureInputDebug::ScanDiagnostic& diag,
    int edge_ordinal,
    int edge_position,
    const gp_Pnt* accepted_point)
{
    if (!ShouldLogFindLineFocusedScan(diag.scan_index))
        return;

    CxShapePoint p0;
    CxShapePoint p1;
    if (!scan_line.exportLine(p0, p1))
        return;

    const double dx = p1.x - p0.x;
    const double dy = p1.y - p0.y;
    const double len2 = dx * dx + dy * dy;
    const double len = len2 > 0.0 ? std::sqrt(len2) : 0.0;

    double accepted_axis_px = -1.0;
    double accepted_normal_px = 0.0;
    double accepted_t = 0.0;
    int accepted_round_x = 0;
    int accepted_round_y = 0;
    if (accepted_point != nullptr && len > 0.0)
    {
        const double px = accepted_point->X();
        const double py = accepted_point->Y();
        const double rel_x = px - p0.x;
        const double rel_y = py - p0.y;
        accepted_t = (rel_x * dx + rel_y * dy) / len2;
        accepted_axis_px = accepted_t * len;
        accepted_normal_px = (rel_x * dy - rel_y * dx) / len;
        accepted_round_x = static_cast<int>(std::lround(px));
        accepted_round_y = static_cast<int>(std::lround(py));
    }

    std::ostringstream ss;
    ss << "scan_type=" << diag.scan_type
       << ", scan_index0=" << diag.scan_index
       << ", gauge_line_num1=" << (diag.scan_index + 1)
       << ", edge_ordinal=" << edge_ordinal
       << ", edge_position_px=" << edge_position
       << ", line_p0=(" << p0.x << "," << p0.y << ")"
       << ", line_p1=(" << p1.x << "," << p1.y << ")"
       << ", line_length_px=" << len
       << ", binary_logical_row=" << diag.binary_logical_row
       << ", binary_sample_row=" << diag.binary_sample_row
       << ", fg_logical=" << diag.binary_logical_foreground_count
       << ", fg_plus1=" << diag.binary_logical_plus1_foreground_count
       << ", fg_plus2=" << diag.binary_logical_plus2_foreground_count
       << ", fg_sample=" << diag.binary_sample_foreground_count
           << ", threshold_runs=" << diag.threshold_run_count
           << ", first_run=(" << diag.threshold_first_run_start
           << "," << diag.threshold_first_run_end << ")"
           << ", last_run=(" << diag.threshold_last_run_start
           << "," << diag.threshold_last_run_end << ")"
           << ", max_run_width=" << diag.threshold_max_run_width
           << ", min_edge_run_width_px=" << diag.min_edge_run_width_px
           << ", rejected_min_edge_width="
           << diag.rejected_min_edge_run_width
           << ", candidate_count=" << diag.candidate_count
           << ", accepted=" << (diag.accepted ? 1 : 0);

    if (accepted_point != nullptr)
    {
        ss << ", accepted_xy=(" << accepted_point->X()
           << "," << accepted_point->Y() << ")"
           << ", accepted_round_xy=(" << accepted_round_x
           << "," << accepted_round_y << ")"
           << ", accepted_axis_px=" << accepted_axis_px
           << ", accepted_t=" << accepted_t
           << ", accepted_normal_px=" << accepted_normal_px
           << ", edge_to_axis_delta_px="
           << (edge_position >= 0 ? accepted_axis_px - edge_position : 0.0);
    }

    LogFindlineMeasureProbe(phase, "debug", ss.str());
}

bool IsFindLineNearAcceptedCandidate(
    const FindLineMeasureInputDebug::ScanDiagnostic& diag,
    const gp_Pnt& point,
    double threshold)
{
    if (!diag.accepted)
        return false;
    threshold = std::min(2.0, std::max(1.0, threshold));
    const double limit2 = threshold * threshold;
    for (std::size_t i = 0; i + 1 < diag.accepted_points_xy.size(); i += 2)
    {
        const double dx = point.X() - diag.accepted_points_xy[i];
        const double dy = point.Y() - diag.accepted_points_xy[i + 1];
        if ((dx * dx + dy * dy) <= limit2)
            return true;
    }
    const double dx = point.X() - diag.accepted_x;
    const double dy = point.Y() - diag.accepted_y;
    return (dx * dx + dy * dy) <= limit2;
}

bool DoesFindLinePointRemain(
    const PointsShape& points,
    double x,
    double y,
    double tolerance)
{
    if (!std::isfinite(x) || !std::isfinite(y))
        return false;

    const double limit2 = tolerance * tolerance;
    const int count = points.size();
    for (int i = 0; i < count; ++i)
    {
        const double dx = points.getx(i) - x;
        const double dy = points.gety(i) - y;
        if ((dx * dx + dy * dy) <= limit2)
            return true;
    }
    return false;
}

int CountFindLineBinaryForegroundInRow(
    Image* image,
    int row,
    int line_length)
{
    if (image == nullptr || line_length <= 0 ||
        row < 0 || row >= image->getHeight())
    {
        return 0;
    }

    const int max_x = std::min(line_length, image->getWidth());
    int count = 0;
    for (int x = 0; x < max_x; ++x)
    {
        if (image->pixel(x, row)[0] > 0)
            ++count;
    }
    return count;
}

int ResolveFindLineBinarySampleRow(
    Image* image,
    int logical_row,
    int scan_index,
    int scan_count,
    int line_length)
{
    (void)scan_index;
    (void)scan_count;
    (void)line_length;
    if (image == nullptr || image->getHeight() <= 0)
        return 0;

    return std::min(std::max(logical_row, 0), image->getHeight() - 1);
}


void PopulateFindLineBinaryScanDiagnostics(
    Image* image,
    FindLineMeasureInputDebug::ScanDiagnostic& diag,
    int logical_row,
    int scan_index,
    int scan_count,
    int line_length)
{
    diag.binary_logical_row = logical_row;
    diag.binary_sample_row = ResolveFindLineBinarySampleRow(
        image, logical_row, scan_index, scan_count, line_length);
    diag.binary_logical_foreground_count =
        CountFindLineBinaryForegroundInRow(image, logical_row, line_length);
    diag.binary_logical_plus1_foreground_count =
        CountFindLineBinaryForegroundInRow(image, logical_row + 1, line_length);
    diag.binary_logical_plus2_foreground_count =
        CountFindLineBinaryForegroundInRow(image, logical_row + 2, line_length);
    diag.binary_sample_foreground_count = CountFindLineBinaryForegroundInRow(
        image, diag.binary_sample_row, line_length);

    if (image == nullptr || line_length <= 0 ||
        diag.binary_sample_row < 0 || diag.binary_sample_row >= image->getHeight())
    {
        return;
    }

    const int maxX = std::min(line_length, image->getWidth());
    int runStart = -1;
    const auto closeRun = [&](int endInclusive)
    {
        if (runStart < 0)
            return;
        const int width = endInclusive - runStart + 1;
        ++diag.threshold_run_count;
        if (diag.threshold_first_run_start < 0)
        {
            diag.threshold_first_run_start = runStart;
            diag.threshold_first_run_end = endInclusive;
        }
        diag.threshold_last_run_start = runStart;
        diag.threshold_last_run_end = endInclusive;
        if (width > diag.threshold_max_run_width)
            diag.threshold_max_run_width = width;
        runStart = -1;
    };

    for (int x = 0; x < maxX; ++x)
    {
        const bool foreground = image->pixel(x, diag.binary_sample_row)[0] > 0;
        if (foreground)
        {
            if (runStart < 0)
                runStart = x;
        }
        else
        {
            closeRun(x - 1);
        }
    }
    closeRun(maxX - 1);
}

bool ReadFindLineGraySample(

    Image& image,
    LineShape& line,
    int sample_index,
    double& gray)
{
    if (sample_index < 0)
        return false;

    const gp_Pnt p = line.getlinepoint(sample_index);
    const int x = static_cast<int>(std::lround(p.X()));
    const int y = static_cast<int>(std::lround(p.Y()));
    if (x < 0 || y < 0 ||
        x >= image.getWidth() || y >= image.getHeight())
    {
        return false;
    }

    const cv::Vec3b c = image.pixel(x, y);
    gray = 0.299 * static_cast<double>(c[2]) +
        0.587 * static_cast<double>(c[1]) +
        0.114 * static_cast<double>(c[0]);
    return true;
}

double FindLineGradientResponse(
    int method,
    double gray0,
    double gray1)
{
    if (method == 0)
        return gray1 - gray0;
    if (method == 1)
        return gray0 - gray1;
    return std::abs(gray1 - gray0);
}

bool FindBestTerminalGradientPoint(
    Image& image,
    LineShape& line,
    int line_length,
    int gap,
    int threshold,
    int method,
    int need_fix,
    int& out_position,
    double& out_response)
{
    out_position = -1;
    out_response = 0.0;
    if (line_length <= 0)
        return false;

    const int safe_gap = std::max(1, gap);
    const int start = std::max(safe_gap + 3, 1);
    const int end = std::max(start, line_length - safe_gap - 3);
    double best_response = -1.0e100;
    int best_position = -1;

    for (int x = start; x < end; ++x)
    {
        double gray0 = 0.0;
        double gray1 = 0.0;
        if (!ReadFindLineGraySample(image, line, x, gray0) ||
            !ReadFindLineGraySample(image, line, x + safe_gap, gray1))
        {
            continue;
        }

        const double response = FindLineGradientResponse(method, gray0, gray1);
        if (response > best_response)
        {
            best_response = response;
            best_position = x;
        }
    }

    if (best_position < 0 ||
        best_response < static_cast<double>(threshold))
    {
        return false;
    }

    out_position = std::min(
        std::max(need_fix + best_position, 1),
        std::max(1, line_length - 1));
    out_response = best_response;
    return true;
}

void FinalizeFindLineEdgeEvaluations(FindLineMeasureInputDebug& debug)
{
    debug.evaluated_edge_count = 0;
    debug.best_edge_index = 0;
    debug.best_edge_score = 0.0;

    const int denominator =
        debug.scan_rows_examined > 0 ? debug.scan_rows_examined : 1;
    for (std::size_t i = 1; i < debug.edge_evaluations.size(); ++i)
    {
        auto& eval = debug.edge_evaluations[i];
        if (eval.edge_index <= 0 || eval.candidate_scan_rows <= 0)
            continue;

        ++debug.evaluated_edge_count;
        eval.selected =
            debug.selected_edge_index == 0 ||
            debug.selected_edge_index == eval.edge_index ||
            (debug.selected_edge_index == -1 && eval.accepted_points > 0);
        eval.fit_possible = eval.accepted_points >= 2;
        eval.coverage =
            static_cast<double>(eval.accepted_points) /
            static_cast<double>(denominator);
        eval.score =
            static_cast<double>(eval.accepted_points) * 10.0 +
            eval.coverage * 100.0 -
            static_cast<double>(eval.rejected_near_endpoint) * 2.0 -
            static_cast<double>(eval.over_length_runs) * 1.0;

        if (debug.best_edge_index == 0 ||
            eval.score > debug.best_edge_score)
        {
            debug.best_edge_index = eval.edge_index;
            debug.best_edge_score = eval.score;
        }
    }
}

int PositiveGap(int gap)
{
    return std::max(1, gap);
}

int PositiveExtent(int extent)
{
    return std::max(1, extent);
}

int CeilPositiveToInt(double value)
{
    if (!std::isfinite(value) || value <= 0.0)
        return 1;
    const double max_value = static_cast<double>(std::numeric_limits<int>::max());
    return static_cast<int>(std::ceil(std::min(value, max_value)));
}

bool CxRectIntersectsImage(double minX,
                           double minY,
                           double maxX,
                           double maxY,
                           int width,
                           int height)
{
    if (width <= 0 || height <= 0)
        return false;

    if (maxX < 0.0 || maxY < 0.0)
        return false;

    if (minX >= static_cast<double>(width) ||
        minY >= static_cast<double>(height))
        return false;

    return true;
}
cxgeom::CxSetLineBuildMeta BuildLineScanMeta(double x0,
                                             double y0,
                                             double x1,
                                             double y1,
                                             int gap,
                                             int roi_width,
                                             int roi_height)
{
    cxgeom::CxSetLineBuildMeta meta;
    cxgeom::CxSetLineRequest request;
    request.entity_id = 0;
    request.line_name = "findline_scan";
    request.x0 = x0;
    request.y0 = y0;
    request.z0 = 0.0;
    request.x1 = x1;
    request.y1 = y1;
    request.z1 = 0.0;
    request.gap = PositiveGap(gap);
    request.roi_width = PositiveExtent(roi_width);
    request.roi_height = PositiveExtent(roi_height);

    const cxgeom::CxSetLineBuild builder;
    const cxgeom::CxSetLineBuildResult result = builder.Build(request);
    if (result.success)
        meta = result.meta;
    return meta;
}

int ComputeLineScanCount(double length_hint,
                         int gap,
                         const cxgeom::CxSetLineBuildMeta& meta)
{
    if (gap <= 0)
        return 0;

    int scan_count = ClampSizeToInt(meta.scan_count_hint);
    if (scan_count <= 0 && std::isfinite(length_hint) && length_hint > 0.0)
        scan_count = CeilPositiveToInt(length_hint / static_cast<double>(gap));
    if (scan_count <= 0)
        scan_count = 1;

    if (meta.compact_roi_hint)
        scan_count = std::min(scan_count, 16);

    return std::max(1, scan_count);
}

double ElapsedMilliseconds(const std::chrono::steady_clock::time_point &begin,
                           const std::chrono::steady_clock::time_point &end)
{
    return std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(end - begin).count();
}

struct EdgeChainState
{
    double cost = 0.0;
    int prev_index = -1;
    bool valid = false;
};

double ComputeEdgeTransitionCost(const EdgeBandCandidate& previous,
                                 const EdgeBandCandidate& current)
{
    const double position_delta = previous.scan_type == 0
        ? std::abs(previous.x - current.x)
        : std::abs(previous.y - current.y);
    const double width_delta = std::abs(previous.width - current.width);
    const double polarity_penalty = previous.polarity == current.polarity ? 0.0 : 2.0;
    const double rank_penalty = std::abs(previous.edge_rank - current.edge_rank) * 0.5;
    const double strength_penalty = 1.0 / std::max(0.25, current.response_strength);

    return position_delta + width_delta + polarity_penalty + rank_penalty + strength_penalty;
}
}

int FindLine::m_curfindlinenum = 0;
FindLine::FindLine() :Shape(),
m_ihgap(6),
m_iwgap(6),
m_scan_direction(0),
m_iSelectPointGap(3),
m_iMethod(1),
m_iThreshold(8),
m_igamarate(0),
m_dsamplerate(0.004),
m_iobjfilterset(1),
m_ifilterborw(21),
m_ifiltermin(50),
m_ifiltermax(100000),
m_iselectedgenum(0),
m_ineedfixs(2),
m_min_edge_run_width_px(3),
m_icomparegap(2),
m_ishowlines(1),
m_measurepointsboundingRect(gp_Pnt(0,0,0),0,0)
{
    string strname = string("fline%1");
    setname(strname.c_str());
    m_curfindlinenum = m_curfindlinenum + 1;
    int icurmodule = ImageManager::GetCurMode();
    if (icurmodule == 0)
    {
        ImageManager::CurMode();
        icurmodule = ImageManager::GetCurMode();
    }
    g_pbackimage = ImageManager::GetBackImage(icurmodule);
    g_pbackfindobject = ImageManager::Getbackfindobject(icurmodule);
    g_pyrimage0 = ImageManager::GetPyrDownImage(icurmodule, 0);
    g_pyrimage1 = ImageManager::GetPyrDownImage(icurmodule, 1);
    g_pyrimage2 = ImageManager::GetPyrDownImage(icurmodule, 2);
}
FindLine::~FindLine()
{

}
void FindLine::setcomparegap(int igap)
{
    m_icomparegap = igap;
}
void FindLine::setmeasurefallback(int mode)
{
    if (mode < 0)
        mode = 0;

    if (mode > 2)
        mode = 2;

    m_measure_fallback_mode = mode;
}
void FindLine::setscanrotation(double angle_degrees)
{
    if (!std::isfinite(angle_degrees))
        return;

    if (angle_degrees > 360.0 || angle_degrees < -360.0)
        angle_degrees = std::fmod(angle_degrees, 360.0);

    if (std::abs(m_scan_rotation_degrees - angle_degrees) <= 1.0e-9)
        return;

    m_scan_rotation_degrees = angle_degrees;

    if (m_has_display_line_roi)
    {
        setlinesegment(m_display_line_x0, m_display_line_y0,
            m_display_line_x1, m_display_line_y1,
            m_display_line_scale);
        return;
    }

    MarkMeasureGeometryDirty();
    InvalidateMeasureAndFitAfterParamChange("setscanrotation_changed_before_setline");
}

void FindLine::setshow(int ishow)
{
    if (ishow & 0x02)
    {
        m_measurepoints_w.setshow(2);
        m_measurepoints_h.setshow(2);
    }
    if (1 == ishow)
    {
        m_measurepoints_w.setshow(1);
        m_measurepoints_h.setshow(1);
    }
    if (ishow & 0x04)
    {
        for (int i = 0; i < m_lines_w.size(); i++)
        {
            if (i == 0)
                m_lines_w[i].setcolor(255, 255, 0);
            if (i == m_lines_w.size() - 1)
                m_lines_w[i].setcolor(255, 0, 0);
            m_lines_w[i].setshow(true);
        }
        for (int i = 0; i < m_lines_h.size(); i++)
        {
            if (i == 0)
                m_lines_h[i].setcolor(255, 255, 0);
            if (i == m_lines_h.size() - 1)
                m_lines_h[i].setcolor(255, 0, 0);
            m_lines_h[i].setshow(true);
        }
    }
    else
    {
        for (int i = 0; i < m_lines_w.size(); i++)
            m_lines_w[i].setshow(false);
        for (int i = 0; i < m_lines_h.size(); i++)
            m_lines_h[i].setshow(false);
    }
    if (ishow & 0x08)
    {

        m_modelpoints.setshow(2);

    }
    if (ishow & 0x10)
    {
        m_measurepointsA.setcolor(0, 0, 100);
        m_measurepointsA.setshow(1);
        m_measurepointsB.setcolor(0, 100, 150);
        m_measurepointsB.setshow(1);
        m_measurepointsA_.setcolor(100, 0, 100);
        m_measurepointsA_.setshow(1);
        m_measurepointsB_.setcolor(200, 0, 250);
        m_measurepointsB_.setshow(1);
    }
    if (ishow & 0x20)
    {
        int isize0 = ClampSizeToInt(m_l_measure_w_seek.size());
        int isize1 = ClampSizeToInt(m_l_measure_h_seek.size());
        for (int i = 0; i < isize0; i++)
        {
            if (m_ishowlines == i)
                m_l_measure_w_seek[i].setshow(1);
            else
                m_l_measure_w_seek[i].setshow(false);

        }
        for (int i = 0; i < isize1; i++)
        {
            if (m_ishowlines == -i)
                m_l_measure_h_seek[i].setshow(1);
            else
                m_l_measure_h_seek[i].setshow(false);
        }
    }
    Shape::setshow(ishow);
}
void FindLine::setselectedgenum(int iedgenum)
{
    m_iselectedgenum = iedgenum;
}

void FindLine::setminedgerunwidth(int width)
{
    const int newWidth = std::max(1, std::min(20, width));
    if (newWidth == m_min_edge_run_width_px)
        return;

    m_min_edge_run_width_px = newWidth;

    if (m_measure_geometry_request.valid)
    {
        m_measure_geometry_request.min_edge_run_width_px =
            m_min_edge_run_width_px;
        MarkMeasureGeometryDirty();
        m_measure_geometry_request.version = m_measure_geometry_version;
    }
    else
    {
        MarkMeasureGeometryDirty();
    }

    InvalidateMeasureAndFitAfterParamChange(
        "setminedgerunwidth_changed");
}

void FindLine::setboundaryresponseenabled(int enabled)
{
    const bool newEnabled = enabled != 0;
    if (m_boundary_response_enabled == newEnabled)
        return;
    m_boundary_response_enabled = newEnabled;
    MarkMeasureGeometryDirty();
    InvalidateMeasureAndFitAfterParamChange(
        "setboundaryresponseenabled_changed");
}

void FindLine::setboundaryresponsemode(int mode)
{
    const auto newMode = ClampEnumInt(
        mode,
        0,
        17,
        cxvision::metrology_analytics::CxBoundaryResponseMode::Auto);
    if (m_boundary_response_enabled &&
        m_boundary_response_config.response_mode == newMode)
        return;
    m_boundary_response_config.response_mode = newMode;
    m_boundary_response_enabled = true;
    MarkMeasureGeometryDirty();
    InvalidateMeasureAndFitAfterParamChange(
        "setboundaryresponsemode_changed");
}

int FindLine::getboundaryresponsemode() const
{
    return static_cast<int>(m_boundary_response_config.response_mode);
}

void FindLine::setboundarypolarity(int polarity)
{
    const auto newPolarity = ClampEnumInt(
        polarity,
        0,
        2,
        cxvision::metrology_analytics::CxBoundaryPolarity::Either);
    if (m_boundary_response_config.polarity == newPolarity)
        return;
    m_boundary_response_config.polarity = newPolarity;
    MarkMeasureGeometryDirty();
    InvalidateMeasureAndFitAfterParamChange(
        "setboundarypolarity_changed");
}

int FindLine::getboundarypolarity() const
{
    return static_cast<int>(m_boundary_response_config.polarity);
}

void FindLine::setboundaryselection(int selection)
{
    const auto newSelection = ClampEnumInt(
        selection,
        0,
        5,
        cxvision::metrology_analytics::CxBoundarySelectionMode::Strongest);
    if (m_boundary_response_selection_explicit &&
        m_boundary_response_config.selection_mode == newSelection)
        return;
    m_boundary_response_config.selection_mode = newSelection;
    m_boundary_response_selection_explicit = true;
    MarkMeasureGeometryDirty();
    InvalidateMeasureAndFitAfterParamChange(
        "setboundaryselection_changed");
}

int FindLine::getboundaryselection() const
{
    return static_cast<int>(m_boundary_response_config.selection_mode);
}

void FindLine::setboundarynthcandidate(int nth)
{
    const int newNth = std::max(1, nth);
    if (m_boundary_response_config.nth_candidate == newNth)
        return;
    m_boundary_response_config.nth_candidate = newNth;
    MarkMeasureGeometryDirty();
    InvalidateMeasureAndFitAfterParamChange(
        "setboundarynthcandidate_changed");
}

void FindLine::setboundaryreferencepositionpermille(int permille)
{
    const int newValue = ClampPermille(permille);
    if (m_boundary_response_config.reference_position_permille == newValue)
        return;
    m_boundary_response_config.reference_position_permille = newValue;
    MarkMeasureGeometryDirty();
    InvalidateMeasureAndFitAfterParamChange(
        "setboundaryreferencepositionpermille_changed");
}

void FindLine::setboundarysubpixel(int mode)
{
    const auto newMode = ClampEnumInt(
        mode,
        0,
        2,
        cxvision::metrology_analytics::CxBoundarySubpixelMode::ParabolicResponse);
    if (m_boundary_response_config.subpixel_mode == newMode)
        return;
    m_boundary_response_config.subpixel_mode = newMode;
    MarkMeasureGeometryDirty();
    InvalidateMeasureAndFitAfterParamChange(
        "setboundarysubpixel_changed");
}

void FindLine::setboundarybaseline(int mode)
{
    const auto newMode = ClampEnumInt(
        mode,
        0,
        4,
        cxvision::metrology_analytics::CxBoundaryBaselineMode::Offset);
    if (m_boundary_response_config.baseline_mode == newMode)
        return;
    m_boundary_response_config.baseline_mode = newMode;
    MarkMeasureGeometryDirty();
    InvalidateMeasureAndFitAfterParamChange(
        "setboundarybaseline_changed");
}

void FindLine::setboundarydenoise(int mode)
{
    const auto newMode = ClampEnumInt(
        mode,
        0,
        5,
        cxvision::metrology_analytics::CxBoundaryDenoiseMode::Gaussian);
    if (m_boundary_response_config.denoise_mode == newMode)
        return;
    m_boundary_response_config.denoise_mode = newMode;
    MarkMeasureGeometryDirty();
    InvalidateMeasureAndFitAfterParamChange(
        "setboundarydenoise_changed");
}

void FindLine::setboundarysmoothingradius(int radius)
{
    const int newRadius = std::max(0, std::min(20, radius));
    if (m_boundary_response_config.smoothing_radius == newRadius)
        return;
    m_boundary_response_config.smoothing_radius = newRadius;
    MarkMeasureGeometryDirty();
    InvalidateMeasureAndFitAfterParamChange(
        "setboundarysmoothingradius_changed");
}

void FindLine::setboundarybaselinewindow(int window)
{
    const int newWindow = std::max(1, std::min(256, window));
    if (m_boundary_response_config.baseline_window == newWindow)
        return;
    m_boundary_response_config.baseline_window = newWindow;
    MarkMeasureGeometryDirty();
    InvalidateMeasureAndFitAfterParamChange(
        "setboundarybaselinewindow_changed");
}

void FindLine::setboundarywaveletscale(int scale)
{
    const int newScale = std::max(1, std::min(64, scale));
    if (m_boundary_response_config.wavelet_scale == newScale)
        return;
    m_boundary_response_config.wavelet_scale = newScale;
    MarkMeasureGeometryDirty();
    InvalidateMeasureAndFitAfterParamChange(
        "setboundarywaveletscale_changed");
}

void FindLine::setboundarythresholdpermille(int value)
{
    const int newValue = ClampPermille(value);
    if (m_boundary_response_config.trigger_threshold_permille == newValue)
        return;
    m_boundary_response_config.trigger_threshold_permille = newValue;
    MarkMeasureGeometryDirty();
    InvalidateMeasureAndFitAfterParamChange(
        "setboundarythresholdpermille_changed");
}

void FindLine::setboundarylevelpermille(int value)
{
    const int newValue = ClampPermille(value);
    if (m_boundary_response_config.level_permille == newValue)
        return;
    m_boundary_response_config.level_permille = newValue;
    MarkMeasureGeometryDirty();
    InvalidateMeasureAndFitAfterParamChange(
        "setboundarylevelpermille_changed");
}

void FindLine::setboundaryhysteresispermille(int value)
{
    const int newValue = ClampPermille(value);
    if (m_boundary_response_config.hysteresis_permille == newValue)
        return;
    m_boundary_response_config.hysteresis_permille = newValue;
    MarkMeasureGeometryDirty();
    InvalidateMeasureAndFitAfterParamChange(
        "setboundaryhysteresispermille_changed");
}

void FindLine::setboundarygatestartpermille(int value)
{
    const int newValue = ClampPermille(value);
    if (m_boundary_response_config.gate_start_permille == newValue)
        return;
    m_boundary_response_config.gate_start_permille = newValue;
    MarkMeasureGeometryDirty();
    InvalidateMeasureAndFitAfterParamChange(
        "setboundarygatestartpermille_changed");
}

void FindLine::setboundarygateendpermille(int value)
{
    const int newValue = ClampPermille(value);
    if (m_boundary_response_config.gate_end_permille == newValue)
        return;
    m_boundary_response_config.gate_end_permille = newValue;
    MarkMeasureGeometryDirty();
    InvalidateMeasureAndFitAfterParamChange(
        "setboundarygateendpermille_changed");
}

void FindLine::setboundaryminplateauwidth(int width)
{
    const int newWidth = std::max(1, std::min(256, width));
    if (m_boundary_response_config.min_plateau_width == newWidth)
        return;
    m_boundary_response_config.min_plateau_width = newWidth;
    MarkMeasureGeometryDirty();
    InvalidateMeasureAndFitAfterParamChange(
        "setboundaryminplateauwidth_changed");
}

void FindLine::setboundaryminamplitudepermille(int value)
{
    const int newValue = ClampPermille(value);
    if (m_boundary_response_config.min_amplitude_permille == newValue)
        return;
    m_boundary_response_config.min_amplitude_permille = newValue;
    MarkMeasureGeometryDirty();
    InvalidateMeasureAndFitAfterParamChange(
        "setboundaryminamplitudepermille_changed");
}

void FindLine::setboundarypairminwidth(int width)
{
    const int newWidth = std::max(1, std::min(1024, width));
    if (m_boundary_response_config.pair_min_width == newWidth)
        return;
    m_boundary_response_config.pair_min_width = newWidth;
    if (m_boundary_response_config.pair_max_width <
        m_boundary_response_config.pair_min_width)
    {
        m_boundary_response_config.pair_max_width =
            m_boundary_response_config.pair_min_width;
    }
    MarkMeasureGeometryDirty();
    InvalidateMeasureAndFitAfterParamChange(
        "setboundarypairminwidth_changed");
}

void FindLine::setboundarypairmaxwidth(int width)
{
    const int newWidth = std::max(1, std::min(4096, width));
    if (m_boundary_response_config.pair_max_width == newWidth)
        return;
    m_boundary_response_config.pair_max_width =
        std::max(newWidth, m_boundary_response_config.pair_min_width);
    MarkMeasureGeometryDirty();
    InvalidateMeasureAndFitAfterParamChange(
        "setboundarypairmaxwidth_changed");
}

void FindLine::setboundarypairanchor(int anchor)
{
    const auto newAnchor = ClampEnumInt(
        anchor,
        0,
        2,
        cxvision::metrology_analytics::CxBoundaryPairAnchorMode::Center);
    if (m_boundary_response_config.pair_anchor_mode == newAnchor)
        return;
    m_boundary_response_config.pair_anchor_mode = newAnchor;
    MarkMeasureGeometryDirty();
    InvalidateMeasureAndFitAfterParamChange(
        "setboundarypairanchor_changed");
}

void FindLine::clear()
{
    m_lines_w.clear();
    m_lines_h.clear();

}
void FindLine::SetWHgap(int wgap, int hgap)
{
    const int newWgap = std::max(1, wgap);
    const int newHgap = std::max(1, hgap);

    const bool changed =
        (newWgap != m_iwgap) ||
        (newHgap != m_ihgap);

    m_iwgap = newWgap;
    m_ihgap = newHgap;

    if (!m_has_display_line_roi)
    {
        if (changed)
        {
            MarkMeasureGeometryDirty();
            InvalidateMeasureAndFitAfterParamChange(
                "SetWHgap_changed_before_setline");
        }

        return;
    }

    UpdateMeasureGeometryRequest(
        m_display_line_x0,
        m_display_line_y0,
        m_display_line_x1,
        m_display_line_y1,
        m_display_line_scale);

    if (changed)
    {
        InvalidateMeasureAndFitAfterParamChange(
            "SetWHgap_changed");
    }
}
void FindLine::setlinesegment(double ix0, double iy0,
    double ix1, double iy1, double dscale)
{
    if (!std::isfinite(ix0) || !std::isfinite(iy0) || !std::isfinite(ix1) || !std::isfinite(iy1) ||
        !std::isfinite(dscale) || dscale <= 0.0)
        return;

    const double dx = ix1 - ix0;
    const double dy = iy1 - iy0;
    const double dDist = std::sqrt(dx * dx + dy * dy);
    if (!std::isfinite(dDist) || dDist <= 1.0e-9)
        return;

    m_iwgap = PositiveGap(m_iwgap);
    m_ihgap = PositiveGap(m_ihgap);

    gp_Pnt ptRect[4];
    double kk2;

    if (iy0 == iy1)
    {
        ptRect[0].SetX(ix0);
        ptRect[0].SetY(iy0 - dscale);
        ptRect[1].SetX(ix0);
        ptRect[1].SetY(iy0 + dscale);
        ptRect[2].SetX(ix1);
        ptRect[2].SetY(iy1 + dscale);
        ptRect[3].SetX(ix1);
        ptRect[3].SetY(iy1 - dscale);

    }
    else
    {
        kk2 = (ix0 - ix1) / (iy0 - iy1);
        double theta = std::atan(kk2);
        if (theta < 0)
        {
            theta += CV_PI;
        }

        ptRect[0].SetX(ix0 + dscale * std::cos(theta));
        ptRect[0].SetY( iy0 - dscale * std::sin(theta));
        ptRect[1].SetX(ix0 - dscale * std::cos(theta));
        ptRect[1].SetY(iy0 + dscale * std::sin(theta));
        ptRect[2].SetX(ix1 - dscale * std::cos(theta));
        ptRect[2].SetY( iy1 + dscale * std::sin(theta));
        ptRect[3].SetX(ix1 + dscale * std::cos(theta));
        ptRect[3].SetY( iy1 - dscale * std::sin(theta));
    }
    const double scan_rotation_radians = m_scan_rotation_degrees * CV_PI / 180.0;
    if (std::abs(scan_rotation_radians) > 1.0e-12)
    {
        const double center_x = (ptRect[0].X() + ptRect[2].X()) * 0.5;
        const double center_y = (ptRect[0].Y() + ptRect[2].Y()) * 0.5;
        for (gp_Pnt& point : ptRect)
        {
            RotatePointAround(point, center_x, center_y, scan_rotation_radians);
        }
    }

    m_LineA.setline(ptRect[0].X(), ptRect[0].Y(),
        ptRect[1].X(), ptRect[1].Y());
    m_LineB.setline(ptRect[1].X(), ptRect[1].Y(),
        ptRect[2].X(), ptRect[2].Y());
    m_LineA.setshow(0);
    m_LineB.setshow(0);
    for (int i = 0; i < m_lines_w.size(); i++)
    {
        m_lines_w[i].clear();
    }
    for (int i = 0; i < m_lines_h.size(); i++)
    {
        m_lines_h[i].clear();
    }
    m_lines_w.clear();
    m_lines_h.clear();

    for (int i = 0; i < m_l_measure_w_seek.size(); i++)
    {
        m_l_measure_w_seek[i].clear();
    }

    for (int i = 0; i < m_l_measure_h_seek.size(); i++)
    {
        m_l_measure_h_seek[i].clear();
    }
    m_l_measure_w_seek.clear();
    m_l_measure_h_seek.clear();

    double ilinesizeA = m_LineA.getlinedistance();
    double ilinesizeB = m_LineB.getlinedistance();
    const int roi_width_hint = CeilPositiveToInt(std::abs(ptRect[2].X() - ptRect[0].X()));
    const int roi_height_hint = CeilPositiveToInt(std::abs(ptRect[2].Y() - ptRect[0].Y()));
    const cxgeom::CxSetLineBuildMeta line_a_meta =
        BuildLineScanMeta(ptRect[0].X(), ptRect[0].Y(), ptRect[1].X(), ptRect[1].Y(), m_iwgap, roi_width_hint, roi_height_hint);
    const cxgeom::CxSetLineBuildMeta line_b_meta =
        BuildLineScanMeta(ptRect[1].X(), ptRect[1].Y(), ptRect[2].X(), ptRect[2].Y(), m_ihgap, roi_width_hint, roi_height_hint);
    const int dplinewbase = ComputeLineScanCount(ilinesizeA, m_iwgap, line_a_meta);
    const int dplinehbase = ComputeLineScanCount(ilinesizeB, m_ihgap, line_b_meta);
    const int dplinewsize = dplinewbase > 1 ? dplinewbase + 1 : dplinewbase;
    const int dplinehsize = dplinehbase > 1 ? dplinehbase + 1 : dplinehbase;
    const int dplinewdenom = std::max(1, dplinewsize - 1);
    const int dplinehdenom = std::max(1, dplinehsize - 1);

    double dlineax = (ptRect[1].X() - ptRect[0].X()) / dplinewdenom;
    double dlineay = (ptRect[1].Y() - ptRect[0].Y()) / dplinewdenom;

    double dlinebx = (ptRect[2].X() - ptRect[1].X()) / dplinehdenom;
    double dlineby = (ptRect[2].Y() - ptRect[1].Y()) / dplinehdenom;


    LineShape aline1, aline2;
    for (int i = 0; i < dplinewsize; i++)
    {
        m_lines_w.push_back(aline1);
        m_lines_w[i].copy(m_LineB);
        m_lines_w[i].MoveBy(-dlineax * static_cast<double>(i),
            -dlineay * static_cast<double>(i));
        m_lines_w[i].setPercent(m_dsamplerate);
        m_lines_w[i].setshow(0);
    }
    for (int i = 0; i < dplinehsize; i++)
    {
        m_lines_h.push_back(aline2);
        m_lines_h[i].copy(m_LineA);
        m_lines_h[i].MoveBy(dlinebx * static_cast<double>(i),
            dlineby * static_cast<double>(i));
        m_lines_h[i].setPercent(m_dsamplerate);
        m_lines_h[i].setshow(0);
    }

    Shape::setrect2(ptRect[0].X(), ptRect[0].Y(), ptRect[1].X(), ptRect[1].Y(),
        ptRect[2].X(), ptRect[2].Y(), ptRect[3].X(), ptRect[3].Y()
    );

    m_has_display_line_roi = true;
    m_display_line_x0 = ix0;
    m_display_line_y0 = iy0;
    m_display_line_x1 = ix1;
    m_display_line_y1 = iy1;
    m_display_line_scale = dscale;

    UpdateMeasureGeometryRequest(ix0, iy0, ix1, iy1, dscale);

    SyncMeasureGeometryCacheAfterNativeBuild(dscale);

    InvalidateMeasureAndFitAfterParamChange(
        "setline_changed");
}

bool FindLine::HasOriginalMeasureScanGeometry() const
{
    return !m_lines_w.empty() || !m_lines_h.empty();
}

void FindLine::SyncMeasureGeometryCacheAfterNativeBuild(
    double nativeHalfWidth)
{
    if (!m_measure_geometry_request.valid)
        return;

    const double expectedHalfWidth =
        m_measure_geometry_request.measure_half_width;

    const bool halfWidthMatched =
        std::abs(nativeHalfWidth - expectedHalfWidth) <= 0.5;

    const bool scanGeometryReady =
        HasOriginalMeasureScanGeometry();

    if (halfWidthMatched && scanGeometryReady)
    {
        m_measure_geometry_ready = true;
        m_measure_geometry_dirty = false;
        m_measure_geometry_built_version =
            m_measure_geometry_request.version;
    }
    else
    {
        m_measure_geometry_ready = false;
        m_measure_geometry_dirty = true;
    }
}

void FindLine::InvalidateMeasureAndFitAfterParamChange(
    const char* reason)
{
    m_measurepoints_w.clear();
    m_measurepoints_h.clear();

    m_scanEdgeBands.clear();
    m_bestEdgeChain.clear();

    m_lastMeasureProfile = FindLineMeasureProfileStats();

    clearfitresult();

    m_lastMeasureInputDebug.measure_source =
        "measure_invalidated_by_parameter_change";

    m_lastMeasureInputDebug.failure_stage =
        reason != nullptr ? reason : "parameter_changed";

    m_lastMeasureInputDebug.detail =
        "Findline parameter changed; previous measure points and fitline were invalidated. Run measure and fitline again.";
}

void FindLine::setrect(int ix, int iy, int iw, int ih)
{
    m_iwgap = PositiveGap(m_iwgap);
    m_ihgap = PositiveGap(m_ihgap);
    if (iw < 0)
    {
        ix += iw;
        iw = -iw;
    }
    if (ih < 0)
    {
        iy += ih;
        ih = -ih;
    }

    m_LineA.setline(ix, iy, ix + iw, iy);
    m_LineB.setline(ix, iy, ix, iy + ih);
    m_LineA.setshow(false);
    m_LineB.setshow(false);
    for (int i = 0; i < m_lines_w.size(); i++)
    {
        m_lines_w[i].clear();
    }
    for (int i = 0; i < m_lines_h.size(); i++)
    {
        m_lines_h[i].clear();
    }
    m_lines_w.clear();
    m_lines_h.clear();

    for (int i = 0; i < m_l_measure_w_seek.size(); i++)
    {
        m_l_measure_w_seek[i].clear();
    }

    for (int i = 0; i < m_l_measure_h_seek.size(); i++)
    {
        m_l_measure_h_seek[i].clear();
    }
    m_l_measure_w_seek.clear();
    m_l_measure_h_seek.clear();

    if (iw <= 0 || ih <= 0)
    {
        Shape::setrect(ix, iy, iw, ih);
        return;
    }

    const cxgeom::CxSetLineBuildMeta line_w_meta =
        BuildLineScanMeta(ix, iy, ix + iw, iy, m_iwgap, iw, ih);
    const cxgeom::CxSetLineBuildMeta line_h_meta =
        BuildLineScanMeta(ix, iy, ix, iy + ih, m_ihgap, iw, ih);
    const int iplinewsize = ComputeLineScanCount(static_cast<double>(iw), m_iwgap, line_w_meta);
    const int iplinehsize = ComputeLineScanCount(static_cast<double>(ih), m_ihgap, line_h_meta);
    LineShape aline1, aline2;
    for (int i = 0; i < iplinewsize; i++)
    {
        m_lines_w.push_back(aline1);
        m_lines_w[i].copy(m_LineB);
        m_lines_w[i].Move(RoundToInt(static_cast<double>(m_iwgap) * static_cast<double>(i)), 0);
        m_lines_w[i].setPercent(m_dsamplerate);
        m_lines_w[i].setshow(false);
    }
    for (int i = 0; i < iplinehsize; i++)
    {
        m_lines_h.push_back(aline2);
        m_lines_h[i].copy(m_LineA);
        m_lines_h[i].Move(0, RoundToInt(static_cast<double>(m_ihgap) * static_cast<double>(i)));
        m_lines_h[i].setPercent(m_dsamplerate);
        m_lines_h[i].setshow(false);
    }

    Shape::setrect(ix, iy, iw, ih);
}
void FindLine::getshape(void* pshape)
{
    Shape* pshape0 = (Shape*)pshape;
    if (pshape0 == nullptr)
        return;

    const gp_Rectangle arect = rect();
    pshape0->setrect(RoundToInt(arect.TopLeft().X()),
        RoundToInt(arect.TopLeft().Y()),
        RoundToInt(arect.Width()),
        RoundToInt(arect.Height()));
}
void FindLine::translate(int ix,int iy)
{
    Translate(gp_Vec(ix, iy, 0));
}
void FindLine::Translate(const gp_Vec& translationVector)
{
   int ix0 = RoundToInt(translationVector.X());
   int iy0 = RoundToInt(translationVector.Y());
   getpath().Translate(translationVector);
    m_LineA.Move(ix0, iy0);
    m_LineB.Move(ix0, iy0);
    LineShape aline1, aline2;
    for (int i = 0; i < m_lines_w.size(); i++)
    {
        m_lines_w[i].Move(ix0, iy0);
    }
    for (int i = 0; i < m_lines_h.size(); i++)
    {
        m_lines_h[i].Move(ix0, iy0);
    }
}
void FindLine::drawpattern()
{
    m_modelpoints.setshow(8);
    m_modelpoints.drawshape(getpath());
    m_measurepointsA.drawshape(getpath());
    m_measurepointsB.drawshape(getpath());
    m_measurepointsA_.drawshape(getpath());
    m_measurepointsB_.drawshape(getpath());

}
void FindLine::drawpatternx(double dmovx, double dmovy,
    double dangle,double dzoomx, double dzoomy)
{
    m_modelpoints.setshow(8);
    m_modelpoints.drawshapex(getpath(), dmovx, dmovy,
        dangle, dzoomx, dzoomy);
    m_measurepointsA.drawshapex(getpath(), dmovx, dmovy,
        dangle, dzoomx, dzoomy);
    m_measurepointsB.drawshapex(getpath(), dmovx, dmovy,
        dangle, dzoomx, dzoomy);
    m_measurepointsA_.drawshapex(getpath(), dmovx, dmovy,
        dangle, dzoomx, dzoomy);
    m_measurepointsB_.drawshapex(getpath(), dmovx, dmovy,
        dangle, dzoomx, dzoomy);
}
void FindLine::edgepattern(Image& image)
{
    m_measurepointsA.clear();
    m_measurepointsB.clear();
    m_measurepointsA_.clear();
    m_measurepointsB_.clear();
    m_modelpoints.clear();

    SetWHgap(wgap(), hgap());
    setselectedgenum(0);
    setmethod(0);
    Measure(image);
    SmartFilter(-1, -1);
    m_measurepointsA.addpoints(getresultpointsw());
    m_measurepointsB.addpoints(getresultpointsh());

    m_measurepointsA.doublepattern(m_icomparegap, 12, m_modelpoints);
    m_measurepointsB.doublepattern(m_icomparegap, 9, m_modelpoints);

    setselectedgenum(0);
    setmethod(1);
    Measure(image);
    SmartFilter(-1, -1);
    m_measurepointsA_.addpoints(getresultpointsw());
    m_measurepointsB_.addpoints(getresultpointsh());

    m_measurepointsA_.doublepattern(m_icomparegap, 6, m_modelpoints);
    m_measurepointsB_.doublepattern(m_icomparegap, 3, m_modelpoints);

}
void FindLine::patternzeroposition()
{
    gp_Rectangle arect1 = m_modelpoints.boundingRectAB();
    m_modelpoints.MoveAB(RoundToInt(-arect1.TopLeft().X()), RoundToInt(-arect1.TopLeft().Y()));

}
void FindLine::savepatternfile(const char* pchar)
{
    saveABpatternfile(pchar);
}
void FindLine::loadpatternfile(const char* pchar)
{
    loadABpatternfile(pchar);
}
void FindLine::samplemodelAB(int inum)
{
    m_modelpoints.resampleAB(inum);
}
void FindLine::ABtoShape(std::vector<cv::Point2f>& points)
{
    m_modelpoints.ABtoShape(points);
}
void FindLine::saveABpatternfile(const char* pchar)
{
    m_modelpoints.saveAB(pchar);
}
void FindLine::loadABpatternfile(const char* pchar)
{
    m_modelpoints.loadAB(pchar);
}
int FindLine::ABpatternsize()
{
    return m_modelpoints.ABsize();
}
int FindLine::getlearnacount()
{
    return static_cast<int>(m_measurepointsA.size());
}
int FindLine::getlearnbcount()
{
    return static_cast<int>(m_measurepointsB.size());
}
int FindLine::getlearna2count()
{
    return static_cast<int>(m_measurepointsA_.size());
}
int FindLine::getlearnb2count()
{
    return static_cast<int>(m_measurepointsB_.size());
}
gp_Rectangle FindLine::patternboundingrect()
{
    return m_modelpoints.boundingRect();
}
gp_Rectangle FindLine::patternboundingrectAB()
{
    return m_modelpoints.boundingRectAB();
}
gp_Rectangle FindLine::patternboundingrectA()
{
    return m_modelpoints.boundingRectA();
}
gp_Rectangle FindLine::patternboundingrectB()
{
    return m_modelpoints.boundingRect();
}

void FindLine::patterngap2gap(int inewgap)
{
    m_modelpoints.patterngap2gap(inewgap);
}
void FindLine::patternABgap2gap(double dnewgaprate)
{
    m_modelpoints.patternABgap2gap(dnewgaprate);
}
void FindLine::patternABsample(int irate)
{
    m_modelpoints.patternABsample(irate);
}
void FindLine::pattern2org()
{
    m_modelpoints_org.copy(m_modelpoints);
}
void FindLine::org2pattern()
{
    m_modelpoints.copy(m_modelpoints_org);
}
void FindLine::patternrootgrid(double itype, double drate, double ilevel)
{
    m_modelpoints.keysrootgrid(RoundToInt(itype), drate, RoundToInt(ilevel));
}
void FindLine::patterntranform(int igap, int itype, int isgap, int iline)
{
    m_modelpoints.patterntranform(igap, itype, isgap, iline);
}
void FindLine::patternzoom(double dx, double dy, double igap, double itype)
{
    m_modelpoints.patternzoom(dx, dy, RoundToInt(igap), RoundToInt(itype));
}
void FindLine::patternrotate(double dangle)
{
    m_modelpoints.RotateAB(dangle);
}
void FindLine::modelzoom(double dx, double dy)
{
    m_modelpoints.Zoom(dx, dy);
}
gp_Path& FindLine::getpatternpath()
{
    return m_modelpoints.getpath();
}
gp_Path& FindLine::getpatternpathA()
{
    return m_modelpoints.getpathA();
}
gp_Path& FindLine::getpatternpathB()
{
    return m_modelpoints.getpathB();
}
PointsShape& FindLine::getpattern()
{
    return m_modelpoints;
}

void FindLine::patternfilter(double distanceThreshold , double waveletThreshold)
{
    (void)distanceThreshold;
    (void)waveletThreshold;
#if defined USE_AI___
    gp_Path& path = m_modelpoints.getpath();
    size_t numPoints = path.getpoints().size();
    arma::mat points(2, numPoints);
    for (size_t i = 0; i < numPoints; ++i)
    {
        points(0, i) = path.getpoints()[i].X();
        points(1, i) = path.getpoints()[i].Y();
    }
    auto clusters = mlpackclass::ClusterPointCloud_(points, distanceThreshold);
    std::cout << "        " << std::endl;
    for (int i = 0; i < m_modelsegments.size(); i++)
    {
        m_modelsegments[i].clear();
    }
    m_modelsegments.clear();
    for (const auto& [clusterId, clusterPoints] : clusters)
    {
        std::cout << "Cluster " << clusterId << ":" << std::endl;

        std::vector<std::vector<size_t>> segments = mlpackclass::SegmentPathByWaveletAnalysis(points, clusterPoints, waveletThreshold);

        PointsShape segmentspoints;
        for (size_t segIdx = 0; segIdx < segments.size(); ++segIdx)
        {
            std::cout << "   侄  " << segIdx + 1 << ": ";
            for (size_t pointIdx : segments[segIdx])
            {
                segmentspoints.addpoint(gp_Pnt(points(0, pointIdx), points(1, pointIdx), 0));
                std::cout << "(" << points(0, pointIdx) << ", " << points(1, pointIdx) << ") ";
            }
            std::cout << std::endl;
        }
        m_modelsegments.push_back(segmentspoints);
    }
#endif
}
void FindLine::findpattern(void* pimage)
{
    Image* pgetimage = (Image*)pimage;
    if (pgetimage == nullptr)
        return;
    edgepattern(*pgetimage);
}
void FindLine::drawshape()
{
    if (show() & 0x04)
    {
        m_LineA.setPen(255, 0, 0);
        m_LineB.setPen(255, 0, 0);
        m_LineA.drawshape(getpath());
        m_LineB.drawshape(getpath());
        for (int i = 0; i < m_lines_w.size(); i++)
        {
            m_lines_w[i].drawshape(getpath());
        }
        for (int i = 0; i < m_lines_h.size(); i++)
        {
            m_lines_h[i].drawshape(getpath());
        }
    }
    if (show() & 0x02)
    {
        m_measurepoints_w.drawshape(getpath());
        m_measurepoints_h.drawshape(getpath());
    }
    if (show() & 0x08)
    {
        drawpattern();
    }
    Shape::drawshape();
}
void FindLine::drawshapex(double dmovx,double dmovy,double dangle,double dzoomx, double dzoomy)
{
    if (show() & 0x04)
    {
        m_LineA.setPen(255, 0, 0);
        m_LineB.setPen(255, 0, 0);
        m_LineA.drawshape(getpath());
        m_LineB.drawshape(getpath());
        for (int i = 0; i < m_lines_w.size(); i++)
        {
            m_lines_w[i].drawshape(getpath());
        }
        for (int i = 0; i < m_lines_h.size(); i++)
        {
            m_lines_h[i].drawshape(getpath());
        }
    }
    if (show() & 0x02)
    {
        m_measurepoints_w.drawshapex(getpath(), dmovx, dmovy,
            dangle, dzoomx, dzoomy);
        m_measurepoints_h.drawshapex(getpath(), dmovx, dmovy,
            dangle, dzoomx, dzoomy);
    }
    if (show() & 0x08)
    {
        drawpatternx( dmovx, dmovy,
            dangle, dzoomx, dzoomy);
    }
    Shape::drawshapex( dmovx, dmovy, dangle, dzoomx, dzoomy);
}
void FindLine::setlinesamplerate(double dsamplerate)
{
    m_dsamplerate = dsamplerate;
}
void FindLine::setlinegap(int igap)
{
    m_iSelectPointGap = std::max(1, igap);

    if (m_measure_geometry_request.valid)
    {
        m_measure_geometry_request.linegap = m_iSelectPointGap;
    }

    MarkMeasureGeometryDirty();
}
void FindLine::setmethod(int imethod)
{
    m_iMethod = imethod;
}
void FindLine::setthre(int ithre)
{
    m_iThreshold = ithre;
}
int FindLine::thre()
{
    return m_iThreshold;
}
void FindLine::setgamarate(int igama)
{
    m_igamarate = igama;
}
void FindLine::setobjfilter(int ifindset)
{
    m_iobjfilterset = ifindset;
}
void FindLine::setfilter(int ifilterborw, int ifiltermin, int ifiltermax)
{
    m_ifilterborw = ifilterborw;
    m_ifiltermin = ifiltermin;
    m_ifiltermax = ifiltermax;
    m_filter_explicit = true;
}

void FindLine::setfilterprofile(int profile)
{
    if (profile < 0)
        profile = 0;
    if (profile > 2)
        profile = 2;
    m_filter_profile = profile;
}

void FindLine::setscandirection(int direction)
{
    const int normalized = direction == 1 ? 1 : (direction == 2 ? 2 : 0);
    if (m_scan_direction == normalized)
        return;
    m_scan_direction = normalized;
    m_measurepoints_w.clear();
    m_measurepoints_h.clear();
    InvalidateMeasureAndFitAfterParamChange("scan_direction_changed");
}

void FindLine::setobjectfilterstrategy(int strategy)
{
    if (strategy < 0)
        strategy = 0;
    if (strategy > 4)
        strategy = 4;
    m_findobject_strategy_id = strategy;
}

void FindLine::setpointconsistency(int enabled, int range)
{
    const int normalizedEnabled = enabled != 0 ? 1 : 0;
    const int normalizedRange = std::max(0, range);
    if (m_point_consistency_enabled == normalizedEnabled &&
        m_point_consistency_range == normalizedRange)
    {
        return;
    }

    m_point_consistency_enabled = normalizedEnabled;
    m_point_consistency_range = normalizedRange;
    m_point_consistency_removed_points = 0;
    InvalidateMeasureAndFitAfterParamChange(
        "point_consistency_changed");
}

int FindLine::effectivefiltermin() const
{
    if (m_filter_explicit)
        return static_cast<int>(m_ifiltermin);
    if (m_filter_profile == 1)
        return 20;
    return static_cast<int>(m_ifiltermin);
}

int FindLine::effectivefiltermax() const
{
    return static_cast<int>(m_ifiltermax);
}

int FindLine::effectivefilterborw() const
{
    return m_ifilterborw;
}

void FindLine::RunFindObjectPrefilter(Image& process_image)
{
    if (g_pbackfindobject == nullptr)
        return;

    m_lastMeasureInputDebug.findobject_strategy_id = m_findobject_strategy_id;

    switch (m_findobject_strategy_id)
    {
    case 1:
        g_pbackfindobject->Measure(process_image);
        break;
    case 2:
        g_pbackfindobject->MeasureFast(process_image);
        break;
    case 3:
        g_pbackfindobject->MeasureConnectedComponents(process_image);
        break;
    case 4:
        g_pbackfindobject->MeasurePeakLocalBFS(process_image);
        break;
    default:
        if (m_effective_filter_borw == 21 || m_effective_filter_borw == 22)
            g_pbackfindobject->MeasureConnectedComponents(process_image);
        else
            g_pbackfindobject->Measure(process_image);
        break;
    }

    m_lastMeasureInputDebug.findobject_algorithm_branch =
        g_pbackfindobject->getdebugalgorithmbranch();
    if (m_findobject_strategy_id == 0 &&
        !m_lastMeasureInputDebug.findobject_algorithm_branch.empty())
    {
        m_lastMeasureInputDebug.findobject_algorithm_branch =
            "auto_by_filter:" + m_lastMeasureInputDebug.findobject_algorithm_branch;
    }
}
void FindLine::MeasureT(void *pimage)
{
    Image* image = (Image*)pimage;
    if (image == nullptr || image->getmat().empty() || g_pbackimage == nullptr || g_pbackimage == image)
        return;

    if (rect().TopLeft().X() < 0 || rect().TopLeft().Y() < 0)
        return;
    m_measurepoints_w.clear();
    m_measurepoints_h.clear();
    int iwsize = wscanenabled() ? ClampSizeToInt(m_lines_w.size()) : 0;
    int ihsize = hscanenabled() ? ClampSizeToInt(m_lines_h.size()) : 0;
    if (iwsize <= 0 && ihsize <= 0)
        return;

    int ilineslen1 = 0;
    int ilineslen2 = 0;

    if (iwsize > 0)
        ilineslen1 = m_lines_w[0].getlinesize();
    if (ihsize > 0)
        ilineslen2 = m_lines_h[0].getlinesize();

    int iprocessw = ilineslen1 > ilineslen2 ? ilineslen1 : ilineslen2;
    if (iprocessw <= 0)
        return;

    const int binaryPaddingRows = 2;
    const int binaryRoiHeight = iwsize + ihsize + binaryPaddingRows;

    if (binaryRoiHeight > g_pbackimage->getHeight() ||
        iprocessw > g_pbackimage->getWidth())
        return;


    {
        cv::Mat& backMat = g_pbackimage->getmat();
        if (!backMat.empty())
        {
            const int clearWidth = std::min(iprocessw + 3, backMat.cols);
            const int clearHeight = std::min(binaryRoiHeight, backMat.rows);
            if (clearWidth > 0 && clearHeight > 0)
            {
                backMat(cv::Rect(0, 0, clearWidth, clearHeight))
                    .setTo(cv::Scalar(0, 0, 0));
            }
        }
    }

    for (int i = 0; i < iwsize; i++)
    {
        m_lines_w[i].linecopyex(*image, *g_pbackimage, 0, i);
    }
    for (int i = 0; i < ihsize; i++)
    {
        m_lines_h[i].linecopyex(*image, *g_pbackimage, 0, i + iwsize);
    }

    g_pbackimage->setroi(0, 0, iprocessw, binaryRoiHeight);
    g_pbackimage->roi_7blur_gap_mud_thre_bw(m_iThreshold, m_igamarate, m_iSelectPointGap, m_iMethod);
}
void FindLine::Measure(Image& image)
{
    m_measurepoints_w.clear();
    m_measurepoints_h.clear();

    LogFindlineMeasureProbe(
        "measure_image_enter",
        "running",
        FindlineMeasureMessage(
            "FindLine::Measure(Image&) enter",
            image.getmat().empty() ? 0 : image.getmat().cols,
            image.getmat().empty() ? 0 : image.getmat().rows,
            image.getmat().empty() ? 0 : image.getmat().channels(),
            RoundToInt(rect().TopLeft().X()),
            RoundToInt(rect().TopLeft().Y()),
            RoundToInt(rect().TopLeft().X() + rect().Width()),
            RoundToInt(rect().TopLeft().Y() + rect().Height()),
            static_cast<int>(m_measure_geometry_request.measure_half_width),
            0,
            0,
            0,
            g_pbackimage != nullptr ? g_pbackimage->getWidth() : 0,
            g_pbackimage != nullptr ? g_pbackimage->getHeight() : 0));

    m_lastMeasureInputDebug.backimage_ready =
        g_pbackimage != nullptr;

    m_lastMeasureInputDebug.findobject_ready =
        g_pbackfindobject != nullptr;

    m_lastMeasureInputDebug.objfilterset = m_iobjfilterset;
    m_lastMeasureInputDebug.filter_borw = m_ifilterborw;
    m_lastMeasureInputDebug.filter_min = m_ifiltermin;
    m_lastMeasureInputDebug.filter_max = m_ifiltermax;

    m_lastMeasureInputDebug.filter_profile = m_filter_profile;
    m_lastMeasureInputDebug.filter_explicit = m_filter_explicit;
    m_lastMeasureInputDebug.boundary_response_enabled =
        m_boundary_response_enabled;
    m_lastMeasureInputDebug.boundary_response_mode =
        static_cast<int>(m_boundary_response_config.response_mode);
    m_lastMeasureInputDebug.boundary_response_polarity =
        static_cast<int>(m_boundary_response_config.polarity);
    m_lastMeasureInputDebug.boundary_response_selection =
        static_cast<int>(m_boundary_response_config.selection_mode);

    m_lastMeasureInputDebug.findobject_measure_called = false;
    m_lastMeasureInputDebug.findobject_measure_skipped = false;
    m_lastMeasureInputDebug.binary_foreground_pixels = 0;
    m_lastMeasureInputDebug.binary_roi_width = 0;
    m_lastMeasureInputDebug.binary_roi_height = 0;
    m_lastMeasureInputDebug.result_empty_reason.clear();

    if (image.getmat().empty())
    {
        m_lastMeasureInputDebug.measure_source =
            "original_measure_pipeline_empty_image";

        m_lastMeasureInputDebug.failure_stage =
            "image_mat_empty";

        m_lastMeasureInputDebug.detail =
            "Findline Measure received an empty input image.";

        LogFindlineMeasureProbe(
            "measure_image_fail",
            "failed",
            "failure_stage=image_mat_empty");
        return;
    }

    if (!ImageManager::EnsureAlgorithmRuntimeResources(
            image.getWidth(),
            image.getHeight()))
    {
        m_lastMeasureInputDebug.measure_source =
            "original_measure_pipeline_no_workspace";

        m_lastMeasureInputDebug.failure_stage =
            "scan_workspace_unavailable";

        m_lastMeasureInputDebug.detail =
            "Findline Measure failed to initialize ImageManager work buffers.";

        LogFindlineMeasureProbe(
            "measure_image_fail",
            "failed",
            "failure_stage=scan_workspace_unavailable, reason=EnsureAlgorithmRuntimeResources failed");
        return;
    }

    g_pbackimage = ImageManager::GetBackImage(1);
    g_pbackfindobject = ImageManager::Getbackfindobject(1);

    if (g_pbackimage == nullptr || g_pbackimage == &image ||
        g_pbackimage->getmat().empty())
    {
        m_lastMeasureInputDebug.measure_source =
            "original_measure_pipeline_no_workspace";

        m_lastMeasureInputDebug.failure_stage =
            "scan_workspace_unavailable";

        m_lastMeasureInputDebug.detail =
            "Findline Measure requires an independent ImageManager BackImage workspace.";

        LogFindlineMeasureProbe(
            "measure_image_fail",
            "failed",
            FindlineMeasureMessage(
                "failure_stage=scan_workspace_unavailable",
                image.getWidth(),
                image.getHeight(),
                image.getmat().channels(),
                RoundToInt(rect().TopLeft().X()),
                RoundToInt(rect().TopLeft().Y()),
                RoundToInt(rect().TopLeft().X() + rect().Width()),
                RoundToInt(rect().TopLeft().Y() + rect().Height()),
                static_cast<int>(m_measure_geometry_request.measure_half_width),
                0,
                0,
                0,
                g_pbackimage != nullptr ? g_pbackimage->getWidth() : 0,
                g_pbackimage != nullptr ? g_pbackimage->getHeight() : 0));
        return;
    }

    if (image.getWidth() < rect().TopLeft().X() + rect().Width()
        || image.getHeight() < rect().TopLeft().Y() + rect().Height())
    {
        LogFindlineMeasureProbe(
            "measure_image_fail",
            "failed",
            FindlineMeasureMessage(
                "failure_stage=roi_outside_image",
                image.getWidth(),
                image.getHeight(),
                image.getmat().channels(),
                RoundToInt(rect().TopLeft().X()),
                RoundToInt(rect().TopLeft().Y()),
                RoundToInt(rect().TopLeft().X() + rect().Width()),
                RoundToInt(rect().TopLeft().Y() + rect().Height()),
                static_cast<int>(m_measure_geometry_request.measure_half_width),
                0,
                0,
                0,
                g_pbackimage->getWidth(),
                g_pbackimage->getHeight()));
        return;
    }
    if (rect().TopLeft().X() < 0 || rect().TopLeft().Y() < 0)
    {
        LogFindlineMeasureProbe(
            "measure_image_fail",
            "failed",
            "failure_stage=roi_negative");
        return;
    }

    int iwsize = wscanenabled() ? ClampSizeToInt(m_lines_w.size()) : 0;
    int ihsize = hscanenabled() ? ClampSizeToInt(m_lines_h.size()) : 0;

    m_lastMeasureInputDebug.original_scan_w_count = iwsize;
    m_lastMeasureInputDebug.original_scan_h_count = ihsize;
    m_lastMeasureInputDebug.profile_count = iwsize + ihsize;

    m_lastMeasureInputDebug.measure_geometry_request_valid =
        m_measure_geometry_request.valid;

    m_lastMeasureInputDebug.measure_geometry_dirty =
        m_measure_geometry_dirty;

    m_lastMeasureInputDebug.measure_geometry_ready =
        m_measure_geometry_ready;

    m_lastMeasureInputDebug.measure_geometry_version =
        m_measure_geometry_version;

    m_lastMeasureInputDebug.measure_geometry_built_version =
        m_measure_geometry_built_version;

    m_lastMeasureInputDebug.measure_geometry_half_width =
        m_measure_geometry_request.measure_half_width;

    if (iwsize <= 0 && ihsize <= 0)
    {
        m_lastMeasureInputDebug.measure_source =
            "original_measure_pipeline_no_scan_geometry";

        m_lastMeasureInputDebug.failure_stage =
            "original_scan_lines_empty";

        m_lastMeasureInputDebug.detail =
            "Findline original Measure has zero m_lines_w/m_lines_h. "
            "Use native width script or make request-cache build ready before measure.";

        LogFindlineMeasureProbe(
            "measure_geometry_fail",
            "failed",
            FindlineMeasureMessage(
                "failure_stage=original_scan_lines_empty",
                image.getWidth(),
                image.getHeight(),
                image.getmat().channels(),
                RoundToInt(rect().TopLeft().X()),
                RoundToInt(rect().TopLeft().Y()),
                RoundToInt(rect().TopLeft().X() + rect().Width()),
                RoundToInt(rect().TopLeft().Y() + rect().Height()),
                static_cast<int>(m_measure_geometry_request.measure_half_width),
                iwsize,
                ihsize,
                0,
                g_pbackimage->getWidth(),
                g_pbackimage->getHeight()));
        return;
    }

    int ilineslen1 = 0;
    int ilineslen2 = 0;

    if (iwsize > 0)
        ilineslen1 = m_lines_w[0].getlinesize();
    if (ihsize > 0)
        ilineslen2 = m_lines_h[0].getlinesize();

    int iprocessw = ilineslen1 > ilineslen2 ? ilineslen1 : ilineslen2;

    m_lastMeasureInputDebug.original_scan_w_length = ilineslen1;
    m_lastMeasureInputDebug.original_scan_h_length = ilineslen2;
    m_lastMeasureInputDebug.original_process_width = iprocessw;

    if (iprocessw <= 0)
    {
        m_lastMeasureInputDebug.measure_source =
            "original_measure_pipeline_invalid_scan_width";

        m_lastMeasureInputDebug.failure_stage =
            "original_process_width_zero";

        m_lastMeasureInputDebug.detail =
            "Findline original Measure scan lines exist but process width is zero.";

        LogFindlineMeasureProbe(
            "measure_geometry_fail",
            "failed",
            FindlineMeasureMessage(
                "failure_stage=original_process_width_zero",
                image.getWidth(),
                image.getHeight(),
                image.getmat().channels(),
                RoundToInt(rect().TopLeft().X()),
                RoundToInt(rect().TopLeft().Y()),
                RoundToInt(rect().TopLeft().X() + rect().Width()),
                RoundToInt(rect().TopLeft().Y() + rect().Height()),
                static_cast<int>(m_measure_geometry_request.measure_half_width),
                iwsize,
                ihsize,
                iprocessw,
                g_pbackimage->getWidth(),
                g_pbackimage->getHeight()));
        return;
    }

    LogFindlineMeasureProbe(
        "measure_before_capacity_check",
        "running",
        FindlineMeasureMessage(
            "before workspace capacity check",
            image.getWidth(),
            image.getHeight(),
            image.getmat().channels(),
            RoundToInt(rect().TopLeft().X()),
            RoundToInt(rect().TopLeft().Y()),
            RoundToInt(rect().TopLeft().X() + rect().Width()),
            RoundToInt(rect().TopLeft().Y() + rect().Height()),
            static_cast<int>(m_measure_geometry_request.measure_half_width),
            iwsize,
            ihsize,
            iprocessw,
            g_pbackimage->getWidth(),
            g_pbackimage->getHeight()));

    const int binaryPaddingRows = 2;
    const int binaryRoiHeight = iwsize + ihsize + binaryPaddingRows;

    if (binaryRoiHeight > g_pbackimage->getHeight() ||
        iprocessw > g_pbackimage->getWidth())
    {
        m_lastMeasureInputDebug.measure_source =
            "original_measure_pipeline_workspace_capacity";

        m_lastMeasureInputDebug.failure_stage =
            "scan_workspace_capacity_exceeded";

        m_lastMeasureInputDebug.detail =
            "Findline Measure skipped because scan geometry exceeds the BackImage workspace.";

        LogFindlineMeasureProbe(
            "measure_capacity_fail",
            "failed",
            FindlineMeasureMessage(
                "failure_stage=scan_workspace_capacity_exceeded",
                image.getWidth(),
                image.getHeight(),
                image.getmat().channels(),
                RoundToInt(rect().TopLeft().X()),
                RoundToInt(rect().TopLeft().Y()),
                RoundToInt(rect().TopLeft().X() + rect().Width()),
                RoundToInt(rect().TopLeft().Y() + rect().Height()),
                static_cast<int>(m_measure_geometry_request.measure_half_width),
                iwsize,
                ihsize,
                iprocessw,
                g_pbackimage->getWidth(),
                g_pbackimage->getHeight()));
        return;
    }

    LogFindlineMeasureProbe(
        "measure_before_linecopyex",
        "running",
        FindlineMeasureMessage(
            "before linecopyex",
            image.getWidth(),
            image.getHeight(),
            image.getmat().channels(),
            RoundToInt(rect().TopLeft().X()),
            RoundToInt(rect().TopLeft().Y()),
            RoundToInt(rect().TopLeft().X() + rect().Width()),
            RoundToInt(rect().TopLeft().Y() + rect().Height()),
            static_cast<int>(m_measure_geometry_request.measure_half_width),
            iwsize,
            ihsize,
            iprocessw,
            g_pbackimage->getWidth(),
            g_pbackimage->getHeight()));


    {
        cv::Mat& backMat = g_pbackimage->getmat();
        if (!backMat.empty())
        {
            const int clearWidth = std::min(iprocessw + 3, backMat.cols);
            const int clearHeight = std::min(binaryRoiHeight, backMat.rows);
            if (clearWidth > 0 && clearHeight > 0)
            {
                backMat(cv::Rect(0, 0, clearWidth, clearHeight))
                    .setTo(cv::Scalar(0, 0, 0));
            }
        }
    }

    for (int i = 0; i < iwsize; i++)
    {
        m_lines_w[i].linecopyex(image, *g_pbackimage, 0, i);
    }
    for (int i = 0; i < ihsize; i++)
    {
        m_lines_h[i].linecopyex(image, *g_pbackimage, 0, i + iwsize);
    }

    LogFindlineMeasureProbe(
        "measure_after_linecopyex",
        "running",
        "linecopyex complete");

    LogFindlineMeasureProbe(
        "measure_before_backimage_roi",
        "running",
        "setroi=(0,0," + std::to_string(iprocessw) + "," +
            std::to_string(binaryRoiHeight) + ")");
    g_pbackimage->setroi(0, 0, iprocessw, binaryRoiHeight);

    LogFindlineMeasureProbe(
        "measure_before_blur_threshold",
        "running",
        "threshold=" + std::to_string(m_iThreshold) +
            ", gamma=" + std::to_string(m_igamarate) +
            ", linegap=" + std::to_string(m_iSelectPointGap) +
            ", min_edge_run_width_px=" +
            std::to_string(m_min_edge_run_width_px) +
            ", method=" + std::to_string(m_iMethod));
    g_pbackimage->roi_7blur_gap_mud_thre_bw(
        m_iThreshold, m_igamarate, m_iSelectPointGap, m_iMethod);

    LogFindlineMeasureProbe(
        "measure_after_blur_threshold",

        "running",
        "blur/threshold complete");

    m_lastMeasureInputDebug.binary_roi_width = iprocessw;
    m_lastMeasureInputDebug.binary_roi_height = binaryRoiHeight;

    try
    {
        const cv::Mat& bw = g_pbackimage->getmat();

        if (!bw.empty())
        {
            cv::Mat roi = bw;

            if (bw.channels() > 1)
            {
                cv::cvtColor(bw, roi, cv::COLOR_BGR2GRAY);
            }

            m_lastMeasureInputDebug.binary_foreground_pixels =
                cv::countNonZero(roi);
        }
    }
    catch (...)
    {
        m_lastMeasureInputDebug.binary_foreground_pixels = -1;
    }

    try
    {
        const cv::Mat& bw = g_pbackimage->getmat();
        if (!bw.empty())
        {
            cv::Mat roi = bw;
            if (bw.channels() > 1)
            {
                cv::cvtColor(bw, roi, cv::COLOR_BGR2GRAY);
            }

            m_effective_filter_borw = effectivefilterborw();
            m_effective_filter_min = effectivefiltermin();
            m_effective_filter_max = effectivefiltermax();

            auto ComputeComponentStats = [&](const cv::Mat& binary, bool whiteForeground, int minArea, int maxArea) -> FindLineMeasureInputDebug::ComponentStats {
                FindLineMeasureInputDebug::ComponentStats statsOut;

                cv::Mat mask;
                if (whiteForeground)
                {
                    cv::threshold(binary, mask, 0, 255, cv::THRESH_BINARY);
                }
                else
                {
                    cv::threshold(binary, mask, 0, 255, cv::THRESH_BINARY_INV);
                }

                cv::Mat labels, stats, centroids;
                int numComponents = cv::connectedComponentsWithStats(mask, labels, stats, centroids, 8);

                std::vector<int> areas;
                for (int i = 1; i < numComponents; i++)
                {
                    int area = stats.at<int>(i, cv::CC_STAT_AREA);
                    if (area <= 0)
                        continue;

                    areas.push_back(area);

                    if (area < minArea)
                        statsOut.rejected_by_min++;
                    else if (area > maxArea)
                        statsOut.rejected_by_max++;
                    else
                        statsOut.accepted_by_area++;
                }

                statsOut.component_total = static_cast<int>(areas.size());

                if (!areas.empty())
                {
                    std::sort(areas.begin(), areas.end());
                    statsOut.area_min = areas.front();
                    statsOut.area_max = areas.back();

                    long long sum = 0;
                    for (int area : areas)
                        sum += area;
                    statsOut.area_mean = static_cast<double>(sum) / static_cast<double>(areas.size());
                    statsOut.area_median = areas[areas.size() / 2];

                    const std::size_t p90Index = std::min<std::size_t>(
                        areas.size() - 1,
                        static_cast<std::size_t>(std::floor(static_cast<double>(areas.size() - 1) * 0.9)));
                    statsOut.area_p90 = areas[p90Index];

                    for (auto it = areas.rbegin(); it != areas.rend() && statsOut.top_areas.size() < 10; ++it)
                    {
                        statsOut.top_areas.push_back(*it);
                    }
                }

                return statsOut;
            };

            m_lastMeasureInputDebug.cc_white = ComputeComponentStats(roi, true, m_effective_filter_min, m_effective_filter_max);
            m_lastMeasureInputDebug.cc_black = ComputeComponentStats(roi, false, m_effective_filter_min, m_effective_filter_max);

            if (m_effective_filter_borw == 21)
            {
                m_lastMeasureInputDebug.cc_selected = m_lastMeasureInputDebug.cc_white;
                m_lastMeasureInputDebug.cc_selected_foreground = "white";
            }
            else
            {
                m_lastMeasureInputDebug.cc_selected = m_lastMeasureInputDebug.cc_black;
                m_lastMeasureInputDebug.cc_selected_foreground = "black";
            }

            m_lastMeasureInputDebug.findobject_component_total = m_lastMeasureInputDebug.cc_selected.component_total;
            m_lastMeasureInputDebug.findobject_component_accepted = m_lastMeasureInputDebug.cc_selected.accepted_by_area;
            m_lastMeasureInputDebug.findobject_component_rejected_by_min = m_lastMeasureInputDebug.cc_selected.rejected_by_min;
            m_lastMeasureInputDebug.findobject_component_rejected_by_max = m_lastMeasureInputDebug.cc_selected.rejected_by_max;
            m_lastMeasureInputDebug.findobject_component_rejected_by_borw = 0;

            m_lastMeasureInputDebug.findobject_area_min_observed = m_lastMeasureInputDebug.cc_selected.area_min;
            m_lastMeasureInputDebug.findobject_area_max_observed = m_lastMeasureInputDebug.cc_selected.area_max;
            m_lastMeasureInputDebug.findobject_area_mean_observed = m_lastMeasureInputDebug.cc_selected.area_mean;
            m_lastMeasureInputDebug.findobject_area_median_observed = m_lastMeasureInputDebug.cc_selected.area_median;
            m_lastMeasureInputDebug.findobject_area_p90_observed = m_lastMeasureInputDebug.cc_selected.area_p90;

            m_lastMeasureInputDebug.findobject_top_component_areas = m_lastMeasureInputDebug.cc_selected.top_areas;
            m_lastMeasureInputDebug.findobject_top_component_x.clear();
            m_lastMeasureInputDebug.findobject_top_component_y.clear();
        }
    }
    catch (...)
    {
    }

    if ((m_iobjfilterset & 0x01) && g_pbackfindobject != nullptr)
    {
        m_lastMeasureInputDebug.findobject_measure_called = true;
        const auto countScanRowsWithForeground = [&]() -> int
        {
            int rowsWithForeground = 0;
            for (int y = 0; y < iwsize; ++y)
            {
                bool rowHasForeground = false;
                const int sampleY = ResolveFindLineBinarySampleRow(
                    g_pbackimage, y, y, iwsize, ilineslen1);
                for (int x = 0; x < ilineslen1; ++x)
                {
                    if (g_pbackimage->pixel(x, sampleY)[0] > 0)
                    {
                        rowHasForeground = true;
                        break;
                    }
                }
                if (rowHasForeground)
                    ++rowsWithForeground;
            }
            for (int y = iwsize; y < iwsize + ihsize; ++y)
            {
                bool rowHasForeground = false;
                const int scanIndex = y - iwsize;
                const int sampleY = ResolveFindLineBinarySampleRow(
                    g_pbackimage, y, scanIndex, ihsize, ilineslen2);
                for (int x = 0; x < ilineslen2; ++x)
                {
                    if (g_pbackimage->pixel(x, sampleY)[0] > 0)
                    {
                        rowHasForeground = true;
                        break;
                    }
                }
                if (rowHasForeground)
                    ++rowsWithForeground;
            }
            return rowsWithForeground;
        };

        const int scanRowsWithForegroundBeforeFilter = countScanRowsWithForeground();
        cv::Mat binaryBeforeFindObject;
        g_pbackimage->getmat().copyTo(binaryBeforeFindObject);

        m_lastMeasureInputDebug.findobject_foreground_before =
            m_lastMeasureInputDebug.binary_foreground_pixels;

        g_pbackfindobject->setrect(0, 0, iprocessw, binaryRoiHeight);

        m_effective_filter_borw = effectivefilterborw();
        m_effective_filter_min = effectivefiltermin();
        m_effective_filter_max = effectivefiltermax();

        m_lastMeasureInputDebug.effective_filter_borw = m_effective_filter_borw;
        m_lastMeasureInputDebug.effective_filter_min = m_effective_filter_min;
        m_lastMeasureInputDebug.effective_filter_max = m_effective_filter_max;

        g_pbackfindobject->setbrow(m_effective_filter_borw);
        g_pbackfindobject->setminmaxarea(ClampLongLongToInt(static_cast<long long>(m_effective_filter_min)),
            ClampLongLongToInt(static_cast<long long>(m_effective_filter_max)));

        LogFindlineMeasureProbe(
            "measure_before_findobject_measure",
            "running",
            "findobject_roi=(0,0," + std::to_string(iprocessw) + "," +
                std::to_string(binaryRoiHeight) + "), borw=" +
                std::to_string(m_effective_filter_borw) + ", min=" +
                std::to_string(m_effective_filter_min) + ", max=" +
                std::to_string(m_effective_filter_max));
        RunFindObjectPrefilter(*g_pbackimage);

        const cv::Mat& afterFindObject = g_pbackimage->getmat();
        cv::Mat foreground_gray;
        if (afterFindObject.channels() > 1)
            cv::cvtColor(afterFindObject, foreground_gray, cv::COLOR_BGR2GRAY);
        else
            foreground_gray = afterFindObject;
        m_lastMeasureInputDebug.findobject_foreground_after =
            cv::countNonZero(foreground_gray);
        const int scanRowsWithForegroundAfterFilter = countScanRowsWithForeground();
        if (scanRowsWithForegroundBeforeFilter > 0 &&
            scanRowsWithForegroundAfterFilter == 0 &&
            !binaryBeforeFindObject.empty())
        {
            binaryBeforeFindObject.copyTo(g_pbackimage->getmat());
            m_lastMeasureInputDebug.findobject_algorithm_branch +=
                ":restored_original_scan_rows";
            m_lastMeasureInputDebug.findobject_foreground_after =
                m_lastMeasureInputDebug.findobject_foreground_before;
        }

        {
            int comp_count = g_pbackfindobject->getdebugcomponentcount();
            int accepted = g_pbackfindobject->getdebugacceptedcount();
            int rejected = g_pbackfindobject->getdebugrejectedcount();
            int max_area = g_pbackfindobject->getdebugmaxcomponentarea();
            int max_w = g_pbackfindobject->getdebugmaxcomponentw();
            int max_h = g_pbackfindobject->getdebugmaxcomponenth();
            m_lastMeasureInputDebug.findobject_component_total = comp_count;
            m_lastMeasureInputDebug.findobject_component_accepted = accepted;
            m_lastMeasureInputDebug.findobject_component_rejected_by_min = rejected;
            m_lastMeasureInputDebug.findobject_component_rejected_by_max = 0;
            m_lastMeasureInputDebug.findobject_component_rejected_by_borw = 0;
            m_lastMeasureInputDebug.findobject_area_max_observed = max_area;
        }

        LogFindlineMeasureProbe(
            "measure_after_findobject_measure",
            "running",
            "findobject_results=" + std::to_string(g_pbackfindobject->getresultobjsnum()));
    }
    else
    {
        m_lastMeasureInputDebug.findobject_measure_skipped = true;

        if ((m_iobjfilterset & 0x01) == 0)
        {
            m_lastMeasureInputDebug.result_empty_reason =
                "findobject skipped because objfilterset bit0 is disabled";
        }
        else if (g_pbackfindobject == nullptr)
        {
            m_lastMeasureInputDebug.result_empty_reason =
                "findobject skipped because g_pbackfindobject is null";
        }
    }

    cv::Mat boundaryGray;
    if (m_boundary_response_enabled)
    {
        const cv::Mat src = image.getmat();
        if (!src.empty())
        {
            if (src.channels() == 1)
            {
                if (src.depth() == CV_8U)
                    boundaryGray = src;
                else
                    src.convertTo(boundaryGray, CV_8U);
            }
            else
            {
                cv::cvtColor(src, boundaryGray, cv::COLOR_BGR2GRAY);
            }
        }
    }
    const bool boundaryResponseActive =
        m_boundary_response_enabled && !boundaryGray.empty();

    int irecordpoint[100];
    int irecordnum = 0;
    bool bcollectBegin = false;

    int icurlinenum = 0;
    int icurlineposition = 0;

    cv::Vec3b icolor = 0;
    const bool selectedComponentsRejected =
        m_lastMeasureInputDebug.findobject_measure_called &&
        m_lastMeasureInputDebug.findobject_component_total > 0 &&
        m_lastMeasureInputDebug.findobject_component_accepted <= 0;
    auto tryTerminalGradientFallback = [&](
        int scan_type,
        int line_index,
        int line_length,
        std::size_t diagIndex) -> bool
    {
        if (diagIndex >= m_lastMeasureInputDebug.scan_diagnostics.size())
            return false;

        auto& diag = m_lastMeasureInputDebug.scan_diagnostics[diagIndex];
        if (diag.accepted)
            return false;

        const int scan_count = scan_type == 0 ? iwsize : ihsize;
        if (scan_count <= 0 ||
            !(line_index <= 1 || line_index >= scan_count - 2))
        {
            return false;
        }


        if (m_iselectedgenum != -1 &&
            m_iselectedgenum != 0 &&
            m_iselectedgenum != 1)
            return false;

        LineShape& scan_line = scan_type == 0
            ? m_lines_w[line_index]
            : m_lines_h[line_index];
        int position = -1;
        double response = 0.0;
        if (!FindBestTerminalGradientPoint(
                image,
                scan_line,
                line_length,
                m_iSelectPointGap,
                m_iThreshold,
                m_iMethod,
                m_ineedfixs,
                position,
                response))
        {
            return false;
        }

        gp_Pnt point = scan_line.getlinepoint(position);
        if (scan_type == 0)
            m_measurepoints_w.addpoint(point);
        else
            m_measurepoints_h.addpoint(point);

        ++m_lastMeasureInputDebug.scan_runs_total;
        ++m_lastMeasureInputDebug.scan_runs_within_length_limit;
        ++m_lastMeasureInputDebug.scan_points_emitted;
        RecordFindLineAcceptedDiagnosticPoint(diag, point);
        LogFindLineFocusedScan(
            "accepted_terminal_fallback",
            scan_line,
            diag,
            1,
            position,
            &point);

        diag.reject_reason.clear();
        RecordFindLineEdgeCandidate(
            m_lastMeasureInputDebug,
            1,
            true,
            false,
            false,
            false);
        return true;
    };

    auto tryBoundaryResponseScan = [&](
        int scan_type,
        int line_index,
        int line_length,
        std::size_t diagIndex) -> bool
    {
        if (!boundaryResponseActive ||
            diagIndex >= m_lastMeasureInputDebug.scan_diagnostics.size())
            return false;

        auto& diag = m_lastMeasureInputDebug.scan_diagnostics[diagIndex];
        LineShape& scan_line = scan_type == 0
            ? m_lines_w[line_index]
            : m_lines_h[line_index];

        ++m_lastMeasureInputDebug.boundary_response_scan_rows_evaluated;
        diag.boundary_response_used = true;
        diag.boundary_response_mode =
            static_cast<int>(m_boundary_response_config.response_mode);

        std::vector<float> profile =
            BuildFindLineGrayProfile(boundaryGray, scan_line, line_length);
        if (profile.size() < 3)
        {
            ++m_lastMeasureInputDebug.boundary_response_rejected_no_candidate;
            diag.boundary_response_status = "PROFILE_EMPTY";
            diag.boundary_response_reason = "scan profile has fewer than 3 samples";
            diag.reject_reason = "boundary_response_profile_empty";
            return false;
        }

        cxvision::metrology_analytics::CxBoundaryResponseConfig config =
            m_boundary_response_config;
        config.min_plateau_width =
            std::max(config.min_plateau_width, m_min_edge_run_width_px);

        if (!m_boundary_response_selection_explicit)
        {
            if (m_iselectedgenum > 0)
            {
                config.selection_mode =
                    cxvision::metrology_analytics::CxBoundarySelectionMode::Nth;
                config.nth_candidate = m_iselectedgenum;
            }
            else if (m_iselectedgenum == -1)
            {
                config.selection_mode =
                    cxvision::metrology_analytics::CxBoundarySelectionMode::Last;
            }
        }

        const cxvision::metrology_analytics::CxBoundaryResponseResult response =
            cxvision::metrology_analytics::EvaluateBoundaryResponse(
                profile,
                config);

        diag.boundary_response_mode =
            static_cast<int>(response.effective_response_mode);
        diag.boundary_response_status = response.status;
        diag.boundary_response_reason = response.reason;
        diag.boundary_response_candidate_count =
            static_cast<int>(response.candidates.size());
        diag.candidate_count = diag.boundary_response_candidate_count;
        m_lastMeasureInputDebug.boundary_response_candidate_count +=
            diag.boundary_response_candidate_count;

        if (response.selected_candidate < 0 ||
            response.selected_candidate >=
                static_cast<int>(response.candidates.size()))
        {
            ++m_lastMeasureInputDebug.boundary_response_rejected_no_candidate;
            diag.reject_reason = response.reason.empty()
                ? "boundary_response_no_selected_candidate"
                : response.reason;
            return false;
        }

        const auto& candidate =
            response.candidates[static_cast<std::size_t>(
                response.selected_candidate)];
        diag.boundary_response_selected_index = response.selected_candidate;
        diag.boundary_response_selected_position = candidate.position_samples;
        diag.boundary_response_selected_score = candidate.score;

        const int roundedPosition =
            static_cast<int>(std::lround(candidate.position_samples));
        const bool endpointOk =
            roundedPosition < (line_length - m_iSelectPointGap - 3) &&
            roundedPosition > (m_iSelectPointGap + 3);
        if (!endpointOk)
        {
            ++m_lastMeasureInputDebug.boundary_response_rejected_endpoint;
            ++m_lastMeasureInputDebug.scan_runs_rejected_near_endpoint;
            diag.reject_reason = "boundary_response_endpoint_rejected";
            RecordFindLineEdgeCandidate(
                m_lastMeasureInputDebug,
                response.selected_candidate + 1,
                false,
                false,
                true,
                false);
            return false;
        }

        gp_Pnt point = InterpolateFindLinePoint(
            scan_line,
            candidate.position_samples,
            line_length);
        if (scan_type == 0)
            m_measurepoints_w.addpoint(point);
        else
            m_measurepoints_h.addpoint(point);

        ++m_lastMeasureInputDebug.scan_points_emitted;
        ++m_lastMeasureInputDebug.boundary_response_points_emitted;
        RecordFindLineAcceptedDiagnosticPoint(diag, point);
        LogFindLineFocusedScan(
            "accepted_boundary_response",
            scan_line,
            diag,
            response.selected_candidate + 1,
            roundedPosition,
            &point);
        RecordFindLineEdgeCandidate(
            m_lastMeasureInputDebug,
            response.selected_candidate + 1,
            true,
            false,
            false,
            false);
        return true;
    };

    for (int inumy = 0; inumy < iwsize; inumy++)
    {
        ++m_lastMeasureInputDebug.scan_rows_examined;
        bool rowHasForeground = false;
        FindLineMeasureInputDebug::ScanDiagnostic diag;
        diag.scan_index = inumy;
        diag.scan_type = 0;
        diag.min_edge_run_width_px = m_min_edge_run_width_px;
        diag.reject_reason = selectedComponentsRejected
            ? "component_rejected"
            : "no_threshold_crossing";
        PopulateFindLineBinaryScanDiagnostics(
            g_pbackimage, diag, inumy, inumy, iwsize, ilineslen1);
        m_lastMeasureInputDebug.scan_diagnostics.push_back(diag);
        const std::size_t diagIndex =
            m_lastMeasureInputDebug.scan_diagnostics.size() - 1;
        LogFindLineFocusedScan(
            "scanline_geometry",
            m_lines_w[inumy],
            m_lastMeasureInputDebug.scan_diagnostics[diagIndex],
            -1,
            -1,
            nullptr);
        if (boundaryResponseActive)
        {
            rowHasForeground =
                tryBoundaryResponseScan(0, inumy, ilineslen1, diagIndex);
            if (rowHasForeground)
                ++m_lastMeasureInputDebug.scan_rows_with_foreground;
            continue;
        }
        irecordnum = 0;
        icurlinenum = 0;
        bcollectBegin = false;
        int lastEdgePosition = -1;
        int lastEdgeOrdinal = 0;


        for (int inumx = 0; inumx < ilineslen1; inumx++)
        {
            const int sample_y = ResolveFindLineBinarySampleRow(
                g_pbackimage, inumy, inumy, iwsize, ilineslen1);
            icolor = g_pbackimage->pixel(inumx, sample_y);
            if ((icolor[0]) > 0)
            {
                rowHasForeground = true;
                if (irecordnum < 100)
                {
                    irecordpoint[irecordnum] = inumx;
                    irecordnum++;
                }
                else
                {
                    irecordnum = 0;
                    break;
                }
                bcollectBegin = true;
            }
            else
            {
                if (true == bcollectBegin
                    && irecordnum > 0
                    && irecordnum < m_min_edge_run_width_px)
                {
                    ++m_lastMeasureInputDebug.scan_runs_total;
                    ++m_lastMeasureInputDebug.scan_runs_rejected_by_min_edge_width;
                    ++m_lastMeasureInputDebug.scan_diagnostics[diagIndex]
                          .rejected_min_edge_run_width;
                    if (!m_lastMeasureInputDebug.scan_diagnostics[diagIndex].accepted)
                        m_lastMeasureInputDebug.scan_diagnostics[diagIndex].reject_reason =
                            "min_edge_run_width_rejected";
                }
                else if (true == bcollectBegin
                    && irecordnum > 0
                    && irecordnum <= 70)
                {
                    ++m_lastMeasureInputDebug.scan_runs_total;
                    ++m_lastMeasureInputDebug.scan_runs_within_length_limit;
                    ++m_lastMeasureInputDebug.scan_diagnostics[diagIndex].candidate_count;
                    icurlineposition = m_ineedfixs + irecordpoint[(irecordnum >> 1)];

                    const bool endpointOk =
                        icurlineposition < (ilineslen1 - m_iSelectPointGap - 3) &&
                        icurlineposition > (m_iSelectPointGap + 3);
                    bool edgeCandidateRecorded = false;
                    if (endpointOk)
                    {
                        ++icurlinenum;
                        if (m_iselectedgenum == -1)
                        {
                            lastEdgePosition = icurlineposition;
                            lastEdgeOrdinal = icurlinenum;
                            edgeCandidateRecorded = true;
                        }
                        else if (icurlinenum == m_iselectedgenum
                            || m_iselectedgenum == 0)
                        {
                            gp_Pnt apoint = m_lines_w[inumy].getlinepoint(icurlineposition);
                            auto& currentDiag =
                                m_lastMeasureInputDebug.scan_diagnostics[diagIndex];
                            if (m_iselectedgenum == 0 &&
                                IsFindLineNearAcceptedCandidate(
                                    currentDiag,
                                    apoint,
                                    static_cast<double>(m_iSelectPointGap)))
                            {
                                ++m_lastMeasureInputDebug.scan_runs_rejected_by_selection;
                                RecordFindLineEdgeCandidate(
                                    m_lastMeasureInputDebug,
                                    icurlinenum,
                                    false,
                                    true,
                                    false,
                                    false);
                            }
                            else
                            {
                                m_measurepoints_w.addpoint(apoint);
                                ++m_lastMeasureInputDebug.scan_points_emitted;
                                RecordFindLineAcceptedDiagnosticPoint(currentDiag, apoint);
                                LogFindLineFocusedScan(
                                    "accepted_point",
                                    m_lines_w[inumy],
                                    currentDiag,
                                    icurlinenum,
                                    icurlineposition,
                                    &apoint);

                                RecordFindLineEdgeCandidate(
                                    m_lastMeasureInputDebug,
                                    icurlinenum,
                                    true,
                                    false,
                                    false,
                                    false);
                            }
                            edgeCandidateRecorded = true;
                            if (icurlinenum == m_iselectedgenum)
                                break;
                        }
                        else
                        {
                            ++m_lastMeasureInputDebug.scan_runs_rejected_by_selection;
                            if (!m_lastMeasureInputDebug.scan_diagnostics[diagIndex].accepted)
                                m_lastMeasureInputDebug.scan_diagnostics[diagIndex].reject_reason =
                                    "scan_run_selection_rejected";
                            RecordFindLineEdgeCandidate(
                                m_lastMeasureInputDebug,
                                icurlinenum,
                                false,
                                true,
                                false,
                                false);
                            edgeCandidateRecorded = true;
                        }
                    }
                    else
                    {
                        ++m_lastMeasureInputDebug.scan_runs_rejected_near_endpoint;
                        if (!m_lastMeasureInputDebug.scan_diagnostics[diagIndex].accepted)
                            m_lastMeasureInputDebug.scan_diagnostics[diagIndex].reject_reason =
                                "endpoint_rejected";
                        RecordFindLineEdgeCandidate(
                            m_lastMeasureInputDebug,
                            m_lastMeasureInputDebug.scan_diagnostics[diagIndex].candidate_count,
                            false,
                            false,
                            true,
                            false);
                        edgeCandidateRecorded = true;
                    }
                    if (!edgeCandidateRecorded)
                    {
                        RecordFindLineEdgeCandidate(
                            m_lastMeasureInputDebug,
                            icurlinenum,
                            false,
                            false,
                            false,
                            false);
                    }
                }
                else if (true == bcollectBegin && irecordnum > 70)
                {
                    ++m_lastMeasureInputDebug.scan_runs_total;
                    ++m_lastMeasureInputDebug.scan_runs_over_length_limit;
                    ++m_lastMeasureInputDebug.scan_diagnostics[diagIndex].candidate_count;
                    RecordFindLineEdgeCandidate(
                        m_lastMeasureInputDebug,
                        m_lastMeasureInputDebug.scan_diagnostics[diagIndex].candidate_count,
                        false,
                        false,
                        false,
                        true);
                    if (!m_lastMeasureInputDebug.scan_diagnostics[diagIndex].accepted)
                        m_lastMeasureInputDebug.scan_diagnostics[diagIndex].reject_reason =
                            "scan_run_over_length";
                }
                irecordnum = 0;
                bcollectBegin = false;
            }
        }
        if (true == bcollectBegin
            && irecordnum > 0)
        {
            ++m_lastMeasureInputDebug.scan_runs_total;
            if (irecordnum < m_min_edge_run_width_px)
            {
                ++m_lastMeasureInputDebug.scan_runs_rejected_by_min_edge_width;
                ++m_lastMeasureInputDebug.scan_diagnostics[diagIndex]
                      .rejected_min_edge_run_width;
                if (!m_lastMeasureInputDebug.scan_diagnostics[diagIndex].accepted)
                    m_lastMeasureInputDebug.scan_diagnostics[diagIndex].reject_reason =
                        "min_edge_run_width_rejected";
                irecordnum = 0;
                bcollectBegin = false;
            }
            else
            {
            ++m_lastMeasureInputDebug.scan_diagnostics[diagIndex].candidate_count;
            if (irecordnum <= 70)
                ++m_lastMeasureInputDebug.scan_runs_within_length_limit;
            else
                ++m_lastMeasureInputDebug.scan_runs_over_length_limit;
            icurlineposition = m_ineedfixs + irecordpoint[(irecordnum >> 1)];
            const bool endpointOk =
                icurlineposition < (ilineslen1 - m_iSelectPointGap - 3) &&
                icurlineposition > (m_iSelectPointGap + 3);
            bool edgeCandidateRecorded = false;
            if (endpointOk)
            {
                ++icurlinenum;
                if (m_iselectedgenum == -1)
                {
                    lastEdgePosition = icurlineposition;
                    lastEdgeOrdinal = icurlinenum;
                    edgeCandidateRecorded = true;
                }
                else if (icurlinenum == m_iselectedgenum
                    || m_iselectedgenum == 0)
                {
                    gp_Pnt apoint = m_lines_w[inumy].getlinepoint(icurlineposition);
                    auto& currentDiag =
                        m_lastMeasureInputDebug.scan_diagnostics[diagIndex];
                    if (m_iselectedgenum == 0 &&
                        IsFindLineNearAcceptedCandidate(
                            currentDiag,
                            apoint,
                            static_cast<double>(m_iSelectPointGap)))
                    {
                        ++m_lastMeasureInputDebug.scan_runs_rejected_by_selection;
                        RecordFindLineEdgeCandidate(
                            m_lastMeasureInputDebug,
                            icurlinenum,
                            false,
                            true,
                            false,
                            false);
                    }
                    else
                    {
                        m_measurepoints_w.addpoint(apoint);
                        ++m_lastMeasureInputDebug.scan_points_emitted;
                        RecordFindLineAcceptedDiagnosticPoint(currentDiag, apoint);
                        LogFindLineFocusedScan(
                            "accepted_point",
                            m_lines_w[inumy],
                            currentDiag,
                            icurlinenum,
                            icurlineposition,
                            &apoint);

                        RecordFindLineEdgeCandidate(
                            m_lastMeasureInputDebug,
                            icurlinenum,
                            true,
                            false,
                            false,
                            false);
                    }
                    edgeCandidateRecorded = true;
                    if (icurlinenum == m_iselectedgenum)
                        break;
                }
                else
                {
                    ++m_lastMeasureInputDebug.scan_runs_rejected_by_selection;
                    if (!m_lastMeasureInputDebug.scan_diagnostics[diagIndex].accepted)
                        m_lastMeasureInputDebug.scan_diagnostics[diagIndex].reject_reason =
                            "scan_run_selection_rejected";
                    RecordFindLineEdgeCandidate(
                        m_lastMeasureInputDebug,
                        icurlinenum,
                        false,
                        true,
                        false,
                        false);
                    edgeCandidateRecorded = true;
                }
            }
            else
            {
                ++m_lastMeasureInputDebug.scan_runs_rejected_near_endpoint;
                if (!m_lastMeasureInputDebug.scan_diagnostics[diagIndex].accepted)
                    m_lastMeasureInputDebug.scan_diagnostics[diagIndex].reject_reason =
                        "endpoint_rejected";
                RecordFindLineEdgeCandidate(
                    m_lastMeasureInputDebug,
                    m_lastMeasureInputDebug.scan_diagnostics[diagIndex].candidate_count,
                    false,
                    false,
                    true,
                    false);
                edgeCandidateRecorded = true;
            }
            if (!edgeCandidateRecorded)
            {
                RecordFindLineEdgeCandidate(
                    m_lastMeasureInputDebug,
                    icurlinenum,
                    false,
                    false,
                    false,
                    false);
            }
            irecordnum = 0;
            bcollectBegin = false;
            }
        }

        if (m_iselectedgenum == -1 && lastEdgePosition >= 0)
        {
            gp_Pnt apoint =
                m_lines_w[inumy].getlinepoint(lastEdgePosition);
            m_measurepoints_w.addpoint(apoint);
            ++m_lastMeasureInputDebug.scan_points_emitted;
            auto& currentDiag =
                m_lastMeasureInputDebug.scan_diagnostics[diagIndex];
                                RecordFindLineAcceptedDiagnosticPoint(currentDiag, apoint);
            LogFindLineFocusedScan(
                "accepted_last_edge",
                m_lines_w[inumy],
                currentDiag,
                lastEdgeOrdinal,
                lastEdgePosition,
                &apoint);

            RecordFindLineEdgeCandidate(
                m_lastMeasureInputDebug,
                lastEdgeOrdinal,
                true,
                false,
                false,
                false);
        }

        const bool terminalFallbackAccepted =
            tryTerminalGradientFallback(0, inumy, ilineslen1, diagIndex);
        if (terminalFallbackAccepted)
            rowHasForeground = true;

        if (rowHasForeground)
        {
            ++m_lastMeasureInputDebug.scan_rows_with_foreground;
            auto& currentDiag =
                m_lastMeasureInputDebug.scan_diagnostics[diagIndex];
            if (!currentDiag.accepted &&
                currentDiag.candidate_count == 0 &&
                !selectedComponentsRejected)
            {
                currentDiag.reject_reason = "polarity_mismatch";
            }
        }

    }

    bcollectBegin = false;
    for (int inumy = iwsize; inumy < iwsize + ihsize; inumy++)
    {
        ++m_lastMeasureInputDebug.scan_rows_examined;
        bool rowHasForeground = false;
        FindLineMeasureInputDebug::ScanDiagnostic diag;
        diag.scan_index = inumy - iwsize;
        diag.scan_type = 1;
        diag.min_edge_run_width_px = m_min_edge_run_width_px;
        diag.reject_reason = selectedComponentsRejected
            ? "component_rejected"
            : "no_threshold_crossing";
        PopulateFindLineBinaryScanDiagnostics(
            g_pbackimage, diag, inumy, inumy - iwsize, ihsize, ilineslen2);
        m_lastMeasureInputDebug.scan_diagnostics.push_back(diag);
        const std::size_t diagIndex =
            m_lastMeasureInputDebug.scan_diagnostics.size() - 1;
        LogFindLineFocusedScan(
            "scanline_geometry",
            m_lines_h[inumy - iwsize],
            m_lastMeasureInputDebug.scan_diagnostics[diagIndex],
            -1,
            -1,
            nullptr);
        if (boundaryResponseActive)
        {
            rowHasForeground = tryBoundaryResponseScan(
                1,
                inumy - iwsize,
                ilineslen2,
                diagIndex);
            if (rowHasForeground)
                ++m_lastMeasureInputDebug.scan_rows_with_foreground;
            continue;
        }
        irecordnum = 0;
        icurlinenum = 0;
        bcollectBegin = false;
        int lastEdgePosition = -1;
        int lastEdgeOrdinal = 0;



        for (int inumx = 0; inumx < ilineslen2; inumx++)
        {
            const int scanIndex = inumy - iwsize;
            const int sample_y = ResolveFindLineBinarySampleRow(
                g_pbackimage, inumy, scanIndex, ihsize, ilineslen2);
            icolor = g_pbackimage->pixel(inumx, sample_y);
            if ((icolor[0]) > 0)
            {
                rowHasForeground = true;
                if (irecordnum < 100)
                {
                    irecordpoint[irecordnum] = inumx;
                    irecordnum++;
                }
                else
                {
                    irecordnum = 0;
                    break;
                }
                bcollectBegin = true;
            }
            else
            {
                if (true == bcollectBegin
                    && irecordnum > 0
                    && irecordnum < m_min_edge_run_width_px)
                {
                    ++m_lastMeasureInputDebug.scan_runs_total;
                    ++m_lastMeasureInputDebug.scan_runs_rejected_by_min_edge_width;
                    ++m_lastMeasureInputDebug.scan_diagnostics[diagIndex]
                          .rejected_min_edge_run_width;
                    if (!m_lastMeasureInputDebug.scan_diagnostics[diagIndex].accepted)
                        m_lastMeasureInputDebug.scan_diagnostics[diagIndex].reject_reason =
                            "min_edge_run_width_rejected";
                }
                else if (true == bcollectBegin
                    && irecordnum > 0
                    && irecordnum <= 70)
                {
                    ++m_lastMeasureInputDebug.scan_runs_total;
                    ++m_lastMeasureInputDebug.scan_runs_within_length_limit;
                    ++m_lastMeasureInputDebug.scan_diagnostics[diagIndex].candidate_count;
                    icurlineposition = m_ineedfixs + irecordpoint[(irecordnum >> 1)];
                    const bool endpointOk =
                        icurlineposition < (ilineslen2 - m_iSelectPointGap - 3) &&
                        icurlineposition > (m_iSelectPointGap + 3);
                    bool edgeCandidateRecorded = false;
                    if (endpointOk)
                    {
                        ++icurlinenum;
                        if (m_iselectedgenum == -1)
                        {
                            lastEdgePosition = icurlineposition;
                            lastEdgeOrdinal = icurlinenum;
                            edgeCandidateRecorded = true;
                        }
                        else if (icurlinenum == m_iselectedgenum
                            || m_iselectedgenum == 0)
                        {
                            gp_Pnt apoint = m_lines_h[inumy - iwsize].getlinepoint(icurlineposition);
                            auto& currentDiag =
                                m_lastMeasureInputDebug.scan_diagnostics[diagIndex];
                            if (m_iselectedgenum == 0 &&
                                IsFindLineNearAcceptedCandidate(
                                    currentDiag,
                                    apoint,
                                    static_cast<double>(m_iSelectPointGap)))
                            {
                                ++m_lastMeasureInputDebug.scan_runs_rejected_by_selection;
                                RecordFindLineEdgeCandidate(
                                    m_lastMeasureInputDebug,
                                    icurlinenum,
                                    false,
                                    true,
                                    false,
                                    false);
                            }
                            else
                            {
                                m_measurepoints_h.addpoint(apoint);
                                ++m_lastMeasureInputDebug.scan_points_emitted;
                                RecordFindLineAcceptedDiagnosticPoint(currentDiag, apoint);
                                LogFindLineFocusedScan(
                                    "accepted_point",
                                    m_lines_h[inumy - iwsize],
                                    currentDiag,
                                    icurlinenum,
                                    icurlineposition,
                                    &apoint);

                                RecordFindLineEdgeCandidate(
                                    m_lastMeasureInputDebug,
                                    icurlinenum,
                                    true,
                                    false,
                                    false,
                                    false);
                            }
                            edgeCandidateRecorded = true;

                            if (icurlinenum == m_iselectedgenum)
                                break;
                        }
                        else
                        {
                            ++m_lastMeasureInputDebug.scan_runs_rejected_by_selection;
                            if (!m_lastMeasureInputDebug.scan_diagnostics[diagIndex].accepted)
                                m_lastMeasureInputDebug.scan_diagnostics[diagIndex].reject_reason =
                                    "scan_run_selection_rejected";
                            RecordFindLineEdgeCandidate(
                                m_lastMeasureInputDebug,
                                icurlinenum,
                                false,
                                true,
                                false,
                                false);
                            edgeCandidateRecorded = true;
                        }
                    }
                    else
                    {
                        ++m_lastMeasureInputDebug.scan_runs_rejected_near_endpoint;
                        if (!m_lastMeasureInputDebug.scan_diagnostics[diagIndex].accepted)
                            m_lastMeasureInputDebug.scan_diagnostics[diagIndex].reject_reason =
                                "endpoint_rejected";
                        RecordFindLineEdgeCandidate(
                            m_lastMeasureInputDebug,
                            m_lastMeasureInputDebug.scan_diagnostics[diagIndex].candidate_count,
                            false,
                            false,
                            true,
                            false);
                        edgeCandidateRecorded = true;
                    }
                    if (!edgeCandidateRecorded)
                    {
                        RecordFindLineEdgeCandidate(
                            m_lastMeasureInputDebug,
                            icurlinenum,
                            false,
                            false,
                            false,
                            false);
                    }
                }
                else if (true == bcollectBegin && irecordnum > 70)
                {
                    ++m_lastMeasureInputDebug.scan_runs_total;
                    ++m_lastMeasureInputDebug.scan_runs_over_length_limit;
                    ++m_lastMeasureInputDebug.scan_diagnostics[diagIndex].candidate_count;
                    RecordFindLineEdgeCandidate(
                        m_lastMeasureInputDebug,
                        m_lastMeasureInputDebug.scan_diagnostics[diagIndex].candidate_count,
                        false,
                        false,
                        false,
                        true);
                    if (!m_lastMeasureInputDebug.scan_diagnostics[diagIndex].accepted)
                        m_lastMeasureInputDebug.scan_diagnostics[diagIndex].reject_reason =
                            "scan_run_over_length";
                }
                irecordnum = 0;
                bcollectBegin = false;
            }
        }
        if (true == bcollectBegin
            && irecordnum > 0)
        {
            ++m_lastMeasureInputDebug.scan_runs_total;
            if (irecordnum < m_min_edge_run_width_px)
            {
                ++m_lastMeasureInputDebug.scan_runs_rejected_by_min_edge_width;
                ++m_lastMeasureInputDebug.scan_diagnostics[diagIndex]
                      .rejected_min_edge_run_width;
                if (!m_lastMeasureInputDebug.scan_diagnostics[diagIndex].accepted)
                    m_lastMeasureInputDebug.scan_diagnostics[diagIndex].reject_reason =
                        "min_edge_run_width_rejected";
                irecordnum = 0;
                bcollectBegin = false;
            }
            else
            {
            ++m_lastMeasureInputDebug.scan_diagnostics[diagIndex].candidate_count;
            if (irecordnum <= 70)
                ++m_lastMeasureInputDebug.scan_runs_within_length_limit;
            else
                ++m_lastMeasureInputDebug.scan_runs_over_length_limit;
            icurlineposition = m_ineedfixs + irecordpoint[(irecordnum >> 1)];

            const bool endpointOk =
                icurlineposition < (ilineslen2 - m_iSelectPointGap - 3) &&
                icurlineposition > (m_iSelectPointGap + 3);
            bool edgeCandidateRecorded = false;
            if (endpointOk)
            {
                ++icurlinenum;
                if (m_iselectedgenum == -1)
                {
                    lastEdgePosition = icurlineposition;
                    lastEdgeOrdinal = icurlinenum;
                    edgeCandidateRecorded = true;
                }
                else if (icurlinenum == m_iselectedgenum
                    || m_iselectedgenum == 0)
                {
                    gp_Pnt apoint = m_lines_h[inumy - iwsize].getlinepoint(icurlineposition);
                    auto& currentDiag =
                        m_lastMeasureInputDebug.scan_diagnostics[diagIndex];
                    if (m_iselectedgenum == 0 &&
                        IsFindLineNearAcceptedCandidate(
                            currentDiag,
                            apoint,
                            static_cast<double>(m_iSelectPointGap)))
                    {
                        ++m_lastMeasureInputDebug.scan_runs_rejected_by_selection;
                        RecordFindLineEdgeCandidate(
                            m_lastMeasureInputDebug,
                            icurlinenum,
                            false,
                            true,
                            false,
                            false);
                    }
                    else
                    {
                        m_measurepoints_h.addpoint(apoint);
                        ++m_lastMeasureInputDebug.scan_points_emitted;
                        RecordFindLineAcceptedDiagnosticPoint(currentDiag, apoint);
                        LogFindLineFocusedScan(
                            "accepted_point",
                            m_lines_h[inumy - iwsize],
                            currentDiag,
                            icurlinenum,
                            icurlineposition,
                            &apoint);

                        RecordFindLineEdgeCandidate(
                            m_lastMeasureInputDebug,
                            icurlinenum,
                            true,
                            false,
                            false,
                            false);
                    }
                    edgeCandidateRecorded = true;

                    if (icurlinenum == m_iselectedgenum)
                        break;
                }
                else
                {
                    ++m_lastMeasureInputDebug.scan_runs_rejected_by_selection;
                    if (!m_lastMeasureInputDebug.scan_diagnostics[diagIndex].accepted)
                        m_lastMeasureInputDebug.scan_diagnostics[diagIndex].reject_reason =
                            "scan_run_selection_rejected";
                    RecordFindLineEdgeCandidate(
                        m_lastMeasureInputDebug,
                        icurlinenum,
                        false,
                        true,
                        false,
                        false);
                    edgeCandidateRecorded = true;
                }
            }
            else
            {
                ++m_lastMeasureInputDebug.scan_runs_rejected_near_endpoint;
                if (!m_lastMeasureInputDebug.scan_diagnostics[diagIndex].accepted)
                    m_lastMeasureInputDebug.scan_diagnostics[diagIndex].reject_reason =
                        "endpoint_rejected";
                RecordFindLineEdgeCandidate(
                    m_lastMeasureInputDebug,
                    m_lastMeasureInputDebug.scan_diagnostics[diagIndex].candidate_count,
                    false,
                    false,
                    true,
                    false);
                edgeCandidateRecorded = true;
            }
            if (!edgeCandidateRecorded)
            {
                RecordFindLineEdgeCandidate(
                    m_lastMeasureInputDebug,
                    icurlinenum,
                    false,
                    false,
                    false,
                    false);
            }
            irecordnum = 0;
            bcollectBegin = false;
            }
        }

        if (m_iselectedgenum == -1 && lastEdgePosition >= 0)
        {
            gp_Pnt apoint =
                m_lines_h[inumy - iwsize].getlinepoint(lastEdgePosition);
            m_measurepoints_h.addpoint(apoint);
            ++m_lastMeasureInputDebug.scan_points_emitted;
            auto& currentDiag =
                m_lastMeasureInputDebug.scan_diagnostics[diagIndex];
                                RecordFindLineAcceptedDiagnosticPoint(currentDiag, apoint);
            LogFindLineFocusedScan(
                "accepted_last_edge",
                m_lines_h[inumy - iwsize],
                currentDiag,
                lastEdgeOrdinal,
                lastEdgePosition,
                &apoint);

            RecordFindLineEdgeCandidate(
                m_lastMeasureInputDebug,
                lastEdgeOrdinal,
                true,
                false,
                false,
                false);
        }

        const bool terminalFallbackAccepted =
            tryTerminalGradientFallback(1, inumy - iwsize, ilineslen2, diagIndex);
        if (terminalFallbackAccepted)
            rowHasForeground = true;

        if (rowHasForeground)
        {
            ++m_lastMeasureInputDebug.scan_rows_with_foreground;
            auto& currentDiag =
                m_lastMeasureInputDebug.scan_diagnostics[diagIndex];
            if (!currentDiag.accepted &&
                currentDiag.candidate_count == 0 &&
                !selectedComponentsRejected)
            {
                currentDiag.reject_reason = "polarity_mismatch";
            }
        }
    }

    FinalizeFindLineEdgeEvaluations(m_lastMeasureInputDebug);

    const int resultPointCount =
        ClampSizeToInt(m_measurepoints_w.size()) +
        ClampSizeToInt(m_measurepoints_h.size());

    m_lastMeasureInputDebug.original_point_count = resultPointCount;

    m_lastMeasureInputDebug.original_edgeband_count = 0;
    m_lastMeasureInputDebug.original_chain_length = 0;

    if (resultPointCount > 0)
    {
        m_lastMeasureInputDebug.measure_source =
            "original_measure_pipeline";

        m_lastMeasureInputDebug.failure_stage =
            "result_points_available";

        m_lastMeasureInputDebug.detail =
            "Findline original Measure produced result points.";
    }
    else
    {
        m_lastMeasureInputDebug.measure_source =
            "original_measure_pipeline_no_result";

        if (!m_lastMeasureInputDebug.backimage_ready)
        {
            m_lastMeasureInputDebug.failure_stage =
                "backimage_not_ready";

            m_lastMeasureInputDebug.detail =
                "Findline original Measure failed because g_pbackimage is null.";
        }
        else if ((m_iobjfilterset & 0x01) &&
                 !m_lastMeasureInputDebug.findobject_ready)
        {
            m_lastMeasureInputDebug.failure_stage =
                "findobject_not_ready";

            m_lastMeasureInputDebug.detail =
                "Findline original Measure requires FindObject, but g_pbackfindobject is null.";
        }
        else if (iprocessw <= 0 ||
                 (iwsize <= 0 && ihsize <= 0))
        {
            m_lastMeasureInputDebug.failure_stage =
                "original_scan_geometry_empty";

            m_lastMeasureInputDebug.detail =
                "Findline original Measure has no valid scan geometry.";
        }
        else if (m_lastMeasureInputDebug.boundary_response_enabled)
        {
            if (m_lastMeasureInputDebug.boundary_response_scan_rows_evaluated <= 0)
            {
                m_lastMeasureInputDebug.failure_stage =
                    "boundary_response_not_evaluated";
                m_lastMeasureInputDebug.detail =
                    "Findline BoundaryResponse was enabled but no scan row was evaluated.";
            }
            else if (m_lastMeasureInputDebug.boundary_response_candidate_count <= 0)
            {
                m_lastMeasureInputDebug.failure_stage =
                    "boundary_response_no_candidates";
                m_lastMeasureInputDebug.detail =
                    "Findline BoundaryResponse evaluated scan profiles but produced no candidates.";
            }
            else
            {
                m_lastMeasureInputDebug.failure_stage =
                    "boundary_response_no_measure_points";
                m_lastMeasureInputDebug.detail =
                    "Findline BoundaryResponse produced candidates, but trigger gates or endpoint checks rejected them.";
            }
        }
        else if (m_lastMeasureInputDebug.binary_foreground_pixels == 0)
        {
            m_lastMeasureInputDebug.failure_stage =
                "binary_threshold_no_foreground";

            m_lastMeasureInputDebug.detail =
                "Findline scan image has gradient, but thresholded binary image has no foreground. Check threshold, method, and gamma.";
        }
        else if (m_lastMeasureInputDebug.findobject_measure_called &&
                 m_lastMeasureInputDebug.cc_selected.accepted_by_area == 0)
        {
            m_lastMeasureInputDebug.failure_stage =
                "findobject_filter_no_accepted_components";

            m_lastMeasureInputDebug.detail =
                "Findline prefilter ran but rejected every selected-polarity component. "
                "Check filter_min/filter_max/filter_borw and method polarity.";
        }
        else if (m_lastMeasureInputDebug.findobject_measure_called &&
                 m_lastMeasureInputDebug.scan_rows_with_foreground > 0 &&
                 m_lastMeasureInputDebug.scan_runs_total == 0)
        {
            m_lastMeasureInputDebug.failure_stage =
                "findline_fail_binary_saturated_or_no_segment_boundary";

            m_lastMeasureInputDebug.detail =
                "Findline binary foreground exists, but scan run extraction produced no acceptable segment boundary. "
                "Check method, polarity, and filterprofile before changing fitting.";
        }
        else if (m_lastMeasureInputDebug.findobject_measure_called)
        {
            m_lastMeasureInputDebug.failure_stage =
                "findline_scan_no_measure_points_after_prefilter";

            m_lastMeasureInputDebug.detail =
                "Findline prefilter accepted selected-polarity components, "
                "but scan-line run extraction produced no measure points.";
        }
        else
        {
            m_lastMeasureInputDebug.failure_stage =
                "original_measure_result_points_empty";

            m_lastMeasureInputDebug.detail =
                "Findline original Measure had valid scan geometry and image response, but produced zero result points.";
        }
    }



}
void FindLine::MeasureBalanced(Image& image)
{
    FindLineMeasureProfileStats stats;
    const std::chrono::steady_clock::time_point total_begin = std::chrono::steady_clock::now();

    m_budget_state = CxAlgorithmBudgetState();

    ClearMeasureState();
    BuildScanProfiles(image, stats, total_begin);

    if (m_budget_state.exceeded)
        return;
    CollectAllEdgeBands(image, stats);
    BuildEdgeBandGraph(stats);
    SolveBestEdgeChain(stats);
    RefineBestChainSubpixel(image, stats);
    RefineJointConsistency(stats);
    ConvertBestChainToMeasurePoints(stats);
    FilterMeasurePoints(stats);
    FitWeightedLeastSquares(stats);

    stats.point_count = m_measurepoints_w.size() + m_measurepoints_h.size();
    stats.chain_length = static_cast<int>(m_bestEdgeChain.size());

    m_lastMeasureInputDebug.original_point_count = stats.point_count;
    m_lastMeasureInputDebug.original_edgeband_count = stats.edgeband_count;
    m_lastMeasureInputDebug.original_chain_length = stats.chain_length;

    m_lastMeasureInputDebug.original_failure_stage =
        m_lastMeasureInputDebug.failure_stage;

    m_lastMeasureInputDebug.original_detail =
        m_lastMeasureInputDebug.detail;

    m_lastMeasureInputDebug.fallback_allowed =
        (m_measure_fallback_mode > 0);

    m_lastMeasureInputDebug.fallback_used = false;

    if (stats.point_count > 0)
    {
        m_lastMeasureInputDebug.measure_source = "original_measure_pipeline";
    }
    else
    {
        m_lastMeasureInputDebug.measure_source = "original_measure_pipeline_no_result";
    }

    if (stats.point_count <= 0 &&
        stats.edgeband_count <= 0 &&
        m_measure_fallback_mode == 2)
    {
        FindLineMeasureProfileStats fallbackStats;

        if (MeasureSimpleRoiGradientPoints(image, fallbackStats))
        {
            stats = fallbackStats;

            m_lastMeasureInputDebug.fallback_used = true;
            m_lastMeasureInputDebug.measure_source =
                "simple_roi_gradient_fallback";

            m_lastMeasureInputDebug.failure_stage =
                "simple_roi_gradient_fallback_used";

            m_lastMeasureInputDebug.detail =
                "Original Findline Measure produced zero points; "
                "fallback generated measure points from ROI normal gradient.";
        }
    }
    else if (stats.point_count <= 0 &&
             stats.edgeband_count <= 0 &&
             m_measure_fallback_mode == 1)
    {
        m_lastMeasureInputDebug.measure_source =
            "original_measure_pipeline_diagnostics_only";
    }

    stats.total_ms = ElapsedMilliseconds(total_begin, std::chrono::steady_clock::now());
    m_lastMeasureProfile = stats;

}

void FindLine::ClearMeasureState()
{
    m_scanEdgeBands.clear();
    m_bestEdgeChain.clear();
    m_lastMeasureProfile = FindLineMeasureProfileStats();
    m_measurepoints_w.clear();
    m_measurepoints_h.clear();
}

void FindLine::BuildScanProfiles(Image& image, FindLineMeasureProfileStats& stats, const std::chrono::steady_clock::time_point& total_begin)
{
    const std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    if (g_pbackimage == nullptr)
    {
        LogFindlineMeasureProbe("build_scan_profiles", "skipped", "g_pbackimage is null");
        stats.profile_ms = ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
        return;
    }

    if (image.getWidth() < rect().TopLeft().X() + rect().Width()
        || image.getHeight() < rect().TopLeft().Y() + rect().Height()
        || rect().TopLeft().X() < 0
        || rect().TopLeft().Y() < 0)
    {
        LogFindlineMeasureProbe("build_scan_profiles", "skipped", "roi out of bounds");
        stats.profile_ms = ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
        return;
    }

    const int iwsize = wscanenabled() ? ClampSizeToInt(m_lines_w.size()) : 0;
    const int ihsize = hscanenabled() ? ClampSizeToInt(m_lines_h.size()) : 0;

    if (iwsize <= 0 && ihsize <= 0)
    {
        LogFindlineMeasureProbe("build_scan_profiles", "skipped", "scan lines empty");
        m_lastMeasureInputDebug.failure_stage = "scan_lines_empty";
        stats.profile_ms = ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
        return;
    }

    int ilineslen1 = 0;
    int ilineslen2 = 0;
    if (iwsize > 0)
        ilineslen1 = m_lines_w[0].getlinesize();
    if (ihsize > 0)
        ilineslen2 = m_lines_h[0].getlinesize();

    const int iprocessw = ilineslen1 > ilineslen2 ? ilineslen1 : ilineslen2;
    if (iprocessw <= 0)
    {
        LogFindlineMeasureProbe("build_scan_profiles", "skipped", "process width zero");
        m_lastMeasureInputDebug.failure_stage = "process_width_zero";
        stats.profile_ms = ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
        return;
    }

    const int binaryPaddingRows = 2;
    const int binaryRoiHeight = iwsize + ihsize + binaryPaddingRows;

    if (binaryRoiHeight > g_pbackimage->getHeight() ||
        iprocessw > g_pbackimage->getWidth())
    {
        LogFindlineMeasureProbe(
            "build_scan_profiles",
            "skipped",
            "scan_workspace_capacity_exceeded: w=" +
                std::to_string(iprocessw) +
                " h=" + std::to_string(iwsize + ihsize) +
                " backimage=" + std::to_string(g_pbackimage->getWidth()) +
                "x" + std::to_string(g_pbackimage->getHeight()));
        m_lastMeasureInputDebug.failure_stage = "scan_workspace_capacity_exceeded";
        stats.profile_ms = ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
        return;
    }

    {
        cv::Mat& backMat = g_pbackimage->getmat();
        if (!backMat.empty())
        {
            const int clearWidth = std::min(iprocessw + 3, backMat.cols);
            const int clearHeight = std::min(binaryRoiHeight, backMat.rows);
            if (clearWidth > 0 && clearHeight > 0)
            {
                backMat(cv::Rect(0, 0, clearWidth, clearHeight))
                    .setTo(cv::Scalar(0, 0, 0));
            }
        }
    }

    for (int i = 0; i < iwsize; ++i)
    {
        m_budget_state.scan_line_count++;
        m_budget_state.elapsed_ms = static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - total_begin).count());

        if (m_budget_state.elapsed_ms >= m_budget.max_elapsed_ms)
        {
            m_budget_state.exceeded = true;
            m_budget_state.exceeded_kind = "algorithm_budget_exceeded";
            m_lastMeasureInputDebug.failure_stage = m_budget_state.exceeded_kind;
            return;
        }

        if (m_budget_state.scan_line_count >= m_budget.max_scan_lines)
        {
            m_budget_state.exceeded = true;
            m_budget_state.exceeded_kind = "scan_line_budget_exceeded";
            m_lastMeasureInputDebug.failure_stage = m_budget_state.exceeded_kind;
            return;
        }

        m_lines_w[i].linecopyex(image, *g_pbackimage, 0, i);
    }
    for (int i = 0; i < ihsize; ++i)
    {
        m_budget_state.scan_line_count++;
        m_budget_state.elapsed_ms = static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - total_begin).count());

        if (m_budget_state.elapsed_ms >= m_budget.max_elapsed_ms)
        {
            m_budget_state.exceeded = true;
            m_budget_state.exceeded_kind = "algorithm_budget_exceeded";
            m_lastMeasureInputDebug.failure_stage = m_budget_state.exceeded_kind;
            return;
        }

        if (m_budget_state.scan_line_count >= m_budget.max_scan_lines)
        {
            m_budget_state.exceeded = true;
            m_budget_state.exceeded_kind = "scan_line_budget_exceeded";
            m_lastMeasureInputDebug.failure_stage = m_budget_state.exceeded_kind;
            return;
        }

        m_lines_h[i].linecopyex(image, *g_pbackimage, 0, i + iwsize);
    }

    g_pbackimage->setroi(0, 0, iprocessw, binaryRoiHeight);
    g_pbackimage->roi_7blur_gap_mud_thre_bw(m_iThreshold, m_igamarate, m_iSelectPointGap, m_iMethod);

    if ((m_iobjfilterset & 0x01) && g_pbackfindobject != nullptr)
    {
        g_pbackfindobject->setrect(0, 0, iprocessw, binaryRoiHeight);

        m_effective_filter_borw = effectivefilterborw();
        m_effective_filter_min = effectivefiltermin();
        m_effective_filter_max = effectivefiltermax();

        m_lastMeasureInputDebug.effective_filter_borw = m_effective_filter_borw;
        m_lastMeasureInputDebug.effective_filter_min = m_effective_filter_min;
        m_lastMeasureInputDebug.effective_filter_max = m_effective_filter_max;

        g_pbackfindobject->setbrow(m_effective_filter_borw);
        g_pbackfindobject->setminmaxarea(ClampLongLongToInt(static_cast<long long>(m_effective_filter_min)),
            ClampLongLongToInt(static_cast<long long>(m_effective_filter_max)));
        RunFindObjectPrefilter(*g_pbackimage);
    }

    m_lastMeasureInputDebug.profile_count = iwsize + ihsize;
    stats.profile_ms = ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
}

void FindLine::CollectAllEdgeBands(Image& image, FindLineMeasureProfileStats& stats)
{
    (void)image;
    const std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    if (g_pbackimage == nullptr)
    {
        stats.edgeband_ms = ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
        return;
    }

    const int max_band_width = 70;
    const int iwsize = wscanenabled() ? ClampSizeToInt(m_lines_w.size()) : 0;
    const int ihsize = hscanenabled() ? ClampSizeToInt(m_lines_h.size()) : 0;
    const int ilineslen1 = iwsize > 0 ? m_lines_w[0].getlinesize() : 0;
    const int ilineslen2 = ihsize > 0 ? m_lines_h[0].getlinesize() : 0;

    auto append_scan_band = [&](int scan_type, int line_index, int row_index, int line_length)
    {
        ScanLineEdgeBands scan_bands;
        scan_bands.scan_index = static_cast<int>(m_scanEdgeBands.size());
        scan_bands.scan_type = scan_type;

        bool collecting = false;
        int band_start = 0;
        int band_rank = 0;

        auto finalize_band = [&](int band_end)
        {
            if (!collecting)
                return;

            const int band_width = band_end - band_start + 1;
            collecting = false;
            if (band_width <= 0 || band_width > max_band_width)
                return;

            const int center_index = m_ineedfixs + band_start + (band_width >> 1);
            if (center_index <= m_iSelectPointGap + 3 ||
                center_index >= line_length - m_iSelectPointGap - 3)
                return;

            EdgeBandCandidate candidate;
            candidate.scan_index = scan_bands.scan_index;
            candidate.scan_type = scan_type;
            candidate.line_index = line_index;
            candidate.candidate_index = static_cast<int>(scan_bands.bands.size());
            candidate.start_index = band_start;
            candidate.end_index = band_end;
            candidate.center_index = center_index;
            candidate.width = static_cast<double>(band_width);
            candidate.edge_rank = band_rank++;
            candidate.response_strength = static_cast<double>(band_width);
            candidate.polarity = m_iMethod == 0 ? 1.0 : -1.0;
            candidate.valid = (m_iselectedgenum == 0 || (candidate.edge_rank + 1) == m_iselectedgenum);

            const gp_Pnt point = scan_type == 0
                ? m_lines_w[line_index].getlinepoint(center_index)
                : m_lines_h[line_index].getlinepoint(center_index);
            candidate.x = point.X();
            candidate.y = point.Y();
            scan_bands.bands.push_back(candidate);
        };

        for (int col_index = 0; col_index < line_length; ++col_index)
        {
            const int sample_row = ResolveFindLineBinarySampleRow(
                g_pbackimage, row_index, line_index,
                scan_type == 0 ? iwsize : ihsize,
                line_length);
            const cv::Vec3b color = g_pbackimage->pixel(col_index, sample_row);
            const bool active = color[0] > 0;
            if (active && !collecting)
            {
                collecting = true;
                band_start = col_index;
            }
            else if (!active && collecting)
            {
                finalize_band(col_index - 1);
            }
        }

        if (collecting)
            finalize_band(line_length - 1);

        if (m_iselectedgenum == -1 && !scan_bands.bands.empty())
        {
            for (auto& candidate : scan_bands.bands)
                candidate.valid = false;
            scan_bands.bands.back().valid = true;
        }

        if (!scan_bands.bands.empty())
            m_scanEdgeBands.push_back(scan_bands);
    };

    for (int line_index = 0; line_index < iwsize; ++line_index)
        append_scan_band(0, line_index, line_index, ilineslen1);

    for (int line_index = 0; line_index < ihsize; ++line_index)
        append_scan_band(1, line_index, iwsize + line_index, ilineslen2);

    stats.edgeband_count = 0;
    for (size_t i = 0; i < m_scanEdgeBands.size(); ++i)
        stats.edgeband_count += static_cast<int>(m_scanEdgeBands[i].bands.size());

    if (stats.edgeband_count <= 0)
    {
        m_lastMeasureInputDebug.failure_stage = "no_edge_band_candidates";
        m_lastMeasureInputDebug.detail =
            "CollectAllEdgeBands produced zero candidates; check image binding, threshold, polarity, method, ROI width";
    }
    else
    {
        m_lastMeasureInputDebug.failure_stage = "edge_band_candidates_available";
    }

    stats.edgeband_ms = ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
}

void FindLine::BuildEdgeBandGraph(FindLineMeasureProfileStats& stats)
{
    const std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    stats.graph_ms = ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
}

void FindLine::SolveBestEdgeChain(FindLineMeasureProfileStats& stats)
{
    const std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    m_bestEdgeChain.clear();
    if (m_scanEdgeBands.empty())
    {
        stats.path_ms = ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
        return;
    }

    std::vector<std::vector<EdgeChainState>> states(m_scanEdgeBands.size());
    for (size_t scan_idx = 0; scan_idx < m_scanEdgeBands.size(); ++scan_idx)
    {
        const ScanLineEdgeBands& scan_bands = m_scanEdgeBands[scan_idx];
        states[scan_idx].resize(scan_bands.bands.size());

        for (size_t band_idx = 0; band_idx < scan_bands.bands.size(); ++band_idx)
        {
            EdgeChainState& state = states[scan_idx][band_idx];
            const EdgeBandCandidate& current = scan_bands.bands[band_idx];
            if (!current.valid)
                continue;

            const double base_cost = 1.0 / std::max(0.25, current.response_strength);
            if (scan_idx == 0)
            {
                state.cost = base_cost;
                state.prev_index = -1;
                state.valid = true;
                continue;
            }

            const ScanLineEdgeBands& previous_scan = m_scanEdgeBands[scan_idx - 1];
            for (size_t prev_idx = 0; prev_idx < previous_scan.bands.size(); ++prev_idx)
            {
                const EdgeChainState& previous_state = states[scan_idx - 1][prev_idx];
                if (!previous_state.valid)
                    continue;

                const double transition_cost = ComputeEdgeTransitionCost(previous_scan.bands[prev_idx], current);
                const double total_cost = previous_state.cost + transition_cost + base_cost;
                if (!state.valid || total_cost < state.cost)
                {
                    state.cost = total_cost;
                    state.prev_index = static_cast<int>(prev_idx);
                    state.valid = true;
                }
            }
        }
    }

    int best_final_index = -1;
    double best_final_cost = 0.0;
    const size_t last_scan_idx = m_scanEdgeBands.size() - 1;
    for (size_t band_idx = 0; band_idx < states[last_scan_idx].size(); ++band_idx)
    {
        const EdgeChainState& state = states[last_scan_idx][band_idx];
        if (!state.valid)
            continue;
        if (best_final_index < 0 || state.cost < best_final_cost)
        {
            best_final_index = static_cast<int>(band_idx);
            best_final_cost = state.cost;
        }
    }

    if (best_final_index >= 0)
    {
        int current_index = best_final_index;
        for (int scan_idx = static_cast<int>(last_scan_idx); scan_idx >= 0 && current_index >= 0; --scan_idx)
        {
            m_bestEdgeChain.push_back(m_scanEdgeBands[scan_idx].bands[current_index]);
            current_index = states[scan_idx][current_index].prev_index;
        }
        std::reverse(m_bestEdgeChain.begin(), m_bestEdgeChain.end());
    }

    stats.path_ms = ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
}

void FindLine::ConvertBestChainToMeasurePoints(FindLineMeasureProfileStats& stats)
{
    const std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    m_measurepoints_w.clear();
    m_measurepoints_h.clear();

    for (size_t i = 0; i < m_bestEdgeChain.size(); ++i)
    {
        const EdgeBandCandidate& candidate = m_bestEdgeChain[i];
        if (!candidate.valid)
            continue;

        if (candidate.scan_type == 0)
        {
            Standard_Real x = candidate.x;
            Standard_Real y = candidate.y;
            m_measurepoints_w.addpoint(x, y);
        }
        else
        {
            Standard_Real x = candidate.x;
            Standard_Real y = candidate.y;
            m_measurepoints_h.addpoint(x, y);
        }
    }

    stats.graph_ms += ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
}

void FindLine::RefineBestChainSubpixel(Image& image, FindLineMeasureProfileStats& stats)
{
    const std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    double total_adjust = 0.0;
    int adjusted_count = 0;
    auto sample_intensity = [&](const EdgeBandCandidate& candidate, int sample_index) -> double
    {
        gp_Pnt point = candidate.scan_type == 0
            ? m_lines_w[candidate.line_index].getlinepoint(sample_index)
            : m_lines_h[candidate.line_index].getlinepoint(sample_index);

        const int px = static_cast<int>(std::round(point.X()));
        const int py = static_cast<int>(std::round(point.Y()));
        if (px < 0 || py < 0 || px >= image.getWidth() || py >= image.getHeight())
            return 0.0;
        const cv::Vec3b pixel = image.pixel(px, py);
        return static_cast<double>(pixel[0]) + static_cast<double>(pixel[1]) + static_cast<double>(pixel[2]);
    };

    for (size_t i = 0; i < m_bestEdgeChain.size(); ++i)
    {
        EdgeBandCandidate& candidate = m_bestEdgeChain[i];
        if (!candidate.valid)
            continue;

        if (candidate.scan_type == 0)
        {
            if (candidate.line_index < 0 || candidate.line_index >= static_cast<int>(m_lines_w.size()))
                continue;
        }
        else
        {
            if (candidate.line_index < 0 || candidate.line_index >= static_cast<int>(m_lines_h.size()))
                continue;
        }

        const int line_size = candidate.scan_type == 0
            ? m_lines_w[candidate.line_index].getlinesize()
            : m_lines_h[candidate.line_index].getlinesize();
        const int center = candidate.center_index;
        if (center - 2 < 0 || center + 2 >= line_size)
            continue;

        const double intensity_m2 = sample_intensity(candidate, center - 2);
        const double intensity_m1 = sample_intensity(candidate, center - 1);
        const double intensity_0 = sample_intensity(candidate, center);
        const double intensity_p1 = sample_intensity(candidate, center + 1);
        const double intensity_p2 = sample_intensity(candidate, center + 2);

        const double grad_left = std::abs(intensity_0 - intensity_m2);
        const double grad_center = std::abs(intensity_p1 - intensity_m1);
        const double grad_right = std::abs(intensity_p2 - intensity_0);
        const double denom = grad_left - 2.0 * grad_center + grad_right;
        double delta = 0.0;
        if (std::abs(denom) > 1e-6)
            delta = 0.5 * (grad_left - grad_right) / denom;
        delta = std::max(-0.5, std::min(0.5, delta));
        total_adjust += std::abs(delta);
        ++adjusted_count;

        gp_Pnt base_point = candidate.scan_type == 0
            ? m_lines_w[candidate.line_index].getlinepoint(center)
            : m_lines_h[candidate.line_index].getlinepoint(center);
        gp_Pnt neighbor_point = delta >= 0.0
            ? (candidate.scan_type == 0
                ? m_lines_w[candidate.line_index].getlinepoint(center + 1)
                : m_lines_h[candidate.line_index].getlinepoint(center + 1))
            : (candidate.scan_type == 0
                ? m_lines_w[candidate.line_index].getlinepoint(center - 1)
                : m_lines_h[candidate.line_index].getlinepoint(center - 1));

        const double blend = std::abs(delta);
        candidate.x = base_point.X() + (neighbor_point.X() - base_point.X()) * blend;
        candidate.y = base_point.Y() + (neighbor_point.Y() - base_point.Y()) * blend;
    }
    stats.subpixel_adjust_avg = adjusted_count > 0 ? total_adjust / adjusted_count : 0.0;
    stats.subpixel_ms = ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
}


void FindLine::BuildBoundaryAnalysisSnapshot(Image& image)
{
    m_boundaryAnalysis = FindLineBoundaryAnalysisSnapshot();
    m_boundaryAnalysis.expected_scan_count =
        m_lastMeasureInputDebug.scan_rows_examined > 0
            ? m_lastMeasureInputDebug.scan_rows_examined
            : static_cast<int>(m_lastMeasureInputDebug.scan_diagnostics.size());

    cv::Mat source = image.getmat();
    if (source.empty())
    {
        m_boundaryAnalysis.status = "INPUT_IMAGE_UNAVAILABLE";
        return;
    }

    cv::Mat gray;
    if (source.channels() == 1)
        gray = source;
    else
        cv::cvtColor(source, gray, cv::COLOR_BGR2GRAY);

    const double lineDx =
        m_lastMeasureInputDebug.roi_x1 - m_lastMeasureInputDebug.roi_x0;
    const double lineDy =
        m_lastMeasureInputDebug.roi_y1 - m_lastMeasureInputDebug.roi_y0;
    const double lineLength = std::hypot(lineDx, lineDy);
    if (lineLength <= 1.0e-12)
    {
        m_boundaryAnalysis.status = "DEGENERATE_GAUGE_DIRECTION";
        return;
    }
    const double normalX = -lineDy / lineLength;
    const double normalY = lineDx / lineLength;

    auto sampleBilinear = [&gray](double x, double y, double& value) -> bool
    {
        if (x < 0.0 || y < 0.0 ||
            x > static_cast<double>(gray.cols - 1) ||
            y > static_cast<double>(gray.rows - 1))
            return false;
        cv::Mat sample;
        cv::getRectSubPix(
            gray, cv::Size(1, 1),
            cv::Point2f(static_cast<float>(x), static_cast<float>(y)),
            sample, CV_32F);
        if (sample.empty())
            return false;
        value = static_cast<double>(sample.at<float>(0, 0));
        return std::isfinite(value);
    };

    auto appendBoundaryPoint = [&](int scanType, int scanIndex,
                                   double measuredX, double measuredY)
    {
        FindLineBoundaryPointSnapshot point;
        point.scan_index = scanIndex;
        point.scan_type = scanType;
        point.measured_x = measuredX;
        point.measured_y = measuredY;
        point.refined_x = measuredX;
        point.refined_y = measuredY;

        bool profileReady = true;
        for (int i = 0; i < 5; ++i)
        {
            const double offset = static_cast<double>(i - 2);
            if (!sampleBilinear(
                    point.measured_x + normalX * offset,
                    point.measured_y + normalY * offset,
                    point.profile[static_cast<std::size_t>(i)]))
            {
                profileReady = false;
                break;
            }
        }

        if (profileReady)
        {
            const double gradLeft =
                std::abs(point.profile[2] - point.profile[0]);
            const double gradCenter =
                std::abs(point.profile[3] - point.profile[1]);
            const double gradRight =
                std::abs(point.profile[4] - point.profile[2]);
            const double denominator =
                gradLeft - 2.0 * gradCenter + gradRight;

            point.response_strength = gradCenter;
            point.local_noise =
                0.5 * (
                    std::abs(point.profile[0] - 2.0 * point.profile[1] + point.profile[2]) +
                    std::abs(point.profile[2] - 2.0 * point.profile[3] + point.profile[4]));
            point.polarity = point.profile[3] > point.profile[1]
                ? 1
                : (point.profile[3] < point.profile[1] ? -1 : 0);
            point.localization_sigma_px = std::min(
                10.0,
                point.local_noise /
                    std::max(1.0e-9, point.response_strength));

            if (std::abs(denominator) > 1.0e-9 &&
                point.response_strength > 1.0e-9)
            {
                point.subpixel_offset = std::max(
                    -0.5,
                    std::min(
                        0.5,
                        0.5 * (gradLeft - gradRight) / denominator));
                point.refined_x =
                    point.measured_x + normalX * point.subpixel_offset;
                point.refined_y =
                    point.measured_y + normalY * point.subpixel_offset;
                point.interpolation_valid = true;
            }
        }
        m_boundaryAnalysis.points.push_back(point);
    };

    // Boundary analysis is a post-measurement quality layer.  Its input must
    // therefore be the exact point set consumed by fitline(), rather than the
    // larger diagnostic candidate stream (which can contain accepted points
    // from scan branches that were not selected for the final measurement).
    const PointsShape& finalW = getresultpointsw();
    for (int i = 0; i < finalW.size(); ++i)
        appendBoundaryPoint(0, i, finalW.getx(i), finalW.gety(i));

    const PointsShape& finalH = getresultpointsh();
    for (int i = 0; i < finalH.size(); ++i)
        appendBoundaryPoint(1, i, finalH.getx(i), finalH.gety(i));

    if (m_boundaryAnalysis.expected_scan_count <= 0)
    {
        m_boundaryAnalysis.expected_scan_count =
            static_cast<int>(m_boundaryAnalysis.points.size());
    }

    m_boundaryAnalysis.status = m_boundaryAnalysis.points.empty()
        ? "NO_ACCEPTED_BOUNDARY_POINTS"
        : "BOUNDARY_POINTS_CAPTURED";
}

FindLineBoundaryAnalysisSnapshot FindLine::boundaryanalysissnapshot() const
{
    FindLineBoundaryAnalysisSnapshot result = m_boundaryAnalysis;
    result.accepted_point_count =
        static_cast<int>(result.points.size());

    std::vector<double> responses;
    std::vector<double> offsets;
    std::vector<double> sigmas;
    std::vector<double> residuals;
    for (auto& point : result.points)
    {
        responses.push_back(point.response_strength);
        sigmas.push_back(point.localization_sigma_px);
        if (point.interpolation_valid)
        {
            ++result.interpolation_valid_count;
            offsets.push_back(point.subpixel_offset);
        }
    }

    const double fitDx = m_result_x1 - m_result_x0;
    const double fitDy = m_result_y1 - m_result_y0;
    const double fitLength = std::hypot(fitDx, fitDy);
    const bool fitValid = m_has_fit_result && fitLength > 1.0e-12;
    if (fitValid)
    {
        const double nx = -fitDy / fitLength;
        const double ny = fitDx / fitLength;
        for (auto& point : result.points)
        {
            point.fit_residual_px = std::abs(
                (point.refined_x - m_result_x0) * nx +
                (point.refined_y - m_result_y0) * ny);
            point.fit_residual_valid = true;
            residuals.push_back(point.fit_residual_px);
        }
        result.fit_residual_count =
            static_cast<int>(residuals.size());
    }

    auto mean = [](const std::vector<double>& values) -> double
    {
        if (values.empty())
            return 0.0;
        double sum = 0.0;
        for (double value : values)
            sum += value;
        return sum / static_cast<double>(values.size());
    };
    auto percentile = [](std::vector<double> values, double p) -> double
    {
        if (values.empty())
            return 0.0;
        std::sort(values.begin(), values.end());
        const double position =
            std::max(0.0, std::min(1.0, p)) *
            static_cast<double>(values.size() - 1);
        const std::size_t lo =
            static_cast<std::size_t>(std::floor(position));
        const std::size_t hi =
            static_cast<std::size_t>(std::ceil(position));
        const double t = position - static_cast<double>(lo);
        return values[lo] * (1.0 - t) + values[hi] * t;
    };
    auto stddev = [&mean](const std::vector<double>& values) -> double
    {
        if (values.size() < 2)
            return 0.0;
        const double average = mean(values);
        double sum = 0.0;
        for (double value : values)
        {
            const double delta = value - average;
            sum += delta * delta;
        }
        return std::sqrt(sum /
            static_cast<double>(values.size() - 1));
    };

    result.coverage_ratio = result.expected_scan_count > 0
        ? static_cast<double>(result.accepted_point_count) /
          static_cast<double>(result.expected_scan_count)
        : 0.0;
    result.response_mean = mean(responses);
    result.response_median = percentile(responses, 0.5);
    result.response_cv = result.response_mean > 1.0e-12
        ? stddev(responses) / result.response_mean
        : 0.0;
    result.subpixel_offset_mean = mean(offsets);
    result.subpixel_offset_stddev = stddev(offsets);
    result.localization_sigma_mean_px = mean(sigmas);

    if (!residuals.empty())
    {
        double squaredSum = 0.0;
        for (double residual : residuals)
            squaredSum += residual * residual;
        result.residual_rmse_px = std::sqrt(
            squaredSum / static_cast<double>(residuals.size()));
        result.residual_p95_px = percentile(residuals, 0.95);
        result.residual_max_px =
            *std::max_element(residuals.begin(), residuals.end());
        const double outlierLimit =
            std::max(1.5, 3.0 * percentile(residuals, 0.5));
        int outlierCount = 0;
        for (double residual : residuals)
        {
            if (residual > outlierLimit)
                ++outlierCount;
        }
        result.outlier_ratio =
            static_cast<double>(outlierCount) /
            static_cast<double>(residuals.size());
    }

    if (result.accepted_point_count < 2)
    {
        result.status = "INSUFFICIENT_BOUNDARY_POINTS";
        result.reliability_level = "UNAVAILABLE";
        result.reliability_score = 0.0;
        return result;
    }

    const double coverageScore =
        std::max(0.0, std::min(1.0, result.coverage_ratio));
    const double interpolationScore =
        static_cast<double>(result.interpolation_valid_count) /
        static_cast<double>(result.accepted_point_count);
    const double signalScore =
        1.0 / (1.0 + std::max(0.0, result.response_cv));
    const double fitScore = fitValid
        ? 1.0 / (1.0 + std::max(0.0, result.residual_p95_px))
        : 0.0;
    result.reliability_score = std::max(
        0.0,
        std::min(
            1.0,
            0.30 * coverageScore +
            0.20 * interpolationScore +
            0.20 * signalScore +
            0.30 * fitScore));
    result.status = fitValid
        ? "BOUNDARY_ANALYSIS_AVAILABLE"
        : "BOUNDARY_POINTS_AVAILABLE_FIT_PENDING";
    result.reliability_level = result.reliability_score >= 0.80
        ? "HIGH"
        : (result.reliability_score >= 0.55 ? "MEDIUM" : "LOW");
    return result;
}

void FindLine::FilterMeasurePoints(FindLineMeasureProfileStats& stats)
{
    const std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    SmartFilter(-1, -1);
    stats.graph_ms += 0.0;
    ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
}

int FindLine::ApplyPointConsistencyConstraintToPoints(
    PointsShape& points,
    double ux,
    double uy,
    double nx,
    double ny,
    double consistencyRange)
{
    const int count = points.size();
    if (count < 3 || consistencyRange <= 0.0)
        return 0;

    std::vector<double> normalOffsets;
    normalOffsets.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i)
    {
        const double x = points.getx(i);
        const double y = points.gety(i);
        if (!std::isfinite(x) || !std::isfinite(y) || x < -90000.0 || y < -90000.0)
            continue;
        normalOffsets.push_back(x * nx + y * ny);
    }
    if (normalOffsets.size() < 3)
        return 0;

    std::nth_element(
        normalOffsets.begin(),
        normalOffsets.begin() + normalOffsets.size() / 2,
        normalOffsets.end());
    const double medianNormal =
        normalOffsets[normalOffsets.size() / 2];

    PointsShape kept;
    int removed = 0;
    for (int i = 0; i < count; ++i)
    {
        double x = points.getx(i);
        double y = points.gety(i);
        if (!std::isfinite(x) || !std::isfinite(y) || x < -90000.0 || y < -90000.0)
        {
            ++removed;
            continue;
        }

        const double normalDelta =
            std::abs((x * nx + y * ny) - medianNormal);
        if (normalDelta <= consistencyRange)
        {
            Standard_Real sx = static_cast<Standard_Real>(x);
            Standard_Real sy = static_cast<Standard_Real>(y);
            kept.addpoint(sx, sy);
        }
        else
        {
            ++removed;
        }
    }

    if (removed > 0)
    {
        points.clear();
        points.addpoints(kept);
    }
    return removed;
}


void FindLine::ApplyPointConsistencyConstraint()
{
    m_point_consistency_removed_points = 0;
    m_lastMeasureInputDebug.point_consistency_enabled =
        m_point_consistency_enabled != 0 ? 1 : 0;
    m_lastMeasureInputDebug.point_consistency_range =
        static_cast<double>(m_point_consistency_range);
    m_lastMeasureInputDebug.point_consistency_input_points =
        ClampSizeToInt(m_measurepoints_w.size()) +
        ClampSizeToInt(m_measurepoints_h.size());

    if (m_point_consistency_enabled == 0)
    {
        m_lastMeasureInputDebug.point_consistency_output_points =
            m_lastMeasureInputDebug.point_consistency_input_points;
        return;
    }

    if (!m_measure_geometry_request.valid)
    {
        m_lastMeasureInputDebug.point_consistency_output_points =
            m_lastMeasureInputDebug.point_consistency_input_points;
        return;
    }

    const double dx =
        m_measure_geometry_request.x1 - m_measure_geometry_request.x0;
    const double dy =
        m_measure_geometry_request.y1 - m_measure_geometry_request.y0;
    const double length = std::sqrt(dx * dx + dy * dy);
    if (length <= 1e-6)
    {
        m_lastMeasureInputDebug.point_consistency_output_points =
            m_lastMeasureInputDebug.point_consistency_input_points;
        return;
    }

    const double ux = dx / length;
    const double uy = dy / length;
    const double nx = -uy;
    const double ny = ux;
    const double range = m_point_consistency_range > 0
        ? static_cast<double>(m_point_consistency_range)
        : std::max(1.0, m_measure_geometry_request.measure_half_width * 0.5);

    m_lastMeasureInputDebug.point_consistency_range = range;
    m_point_consistency_removed_points +=
        ApplyPointConsistencyConstraintToPoints(m_measurepoints_w, ux, uy, nx, ny, range);
    m_point_consistency_removed_points +=
        ApplyPointConsistencyConstraintToPoints(m_measurepoints_h, ux, uy, nx, ny, range);

    m_lastMeasureInputDebug.point_consistency_removed_points =
        m_point_consistency_removed_points;
    m_lastMeasureInputDebug.point_consistency_output_points =
        ClampSizeToInt(m_measurepoints_w.size()) +
        ClampSizeToInt(m_measurepoints_h.size());
}


void FindLine::ReconcilePointConsistencyDiagnostics()
{
    if (m_lastMeasureInputDebug.point_consistency_enabled == 0 ||
        m_lastMeasureInputDebug.point_consistency_removed_points <= 0)
    {
        return;
    }

    const double tolerance = std::max(
        0.25,
        std::min(1.0, m_lastMeasureInputDebug.point_consistency_range * 0.25));

    for (auto& diag : m_lastMeasureInputDebug.scan_diagnostics)
    {
        if (!diag.accepted)
            continue;

        const PointsShape& keptPoints =
            diag.scan_type == 0 ? m_measurepoints_w : m_measurepoints_h;

        std::vector<double> keptAcceptedPoints;
        keptAcceptedPoints.reserve(diag.accepted_points_xy.size());
        for (std::size_t i = 0; i + 1 < diag.accepted_points_xy.size(); i += 2)
        {
            const double x = diag.accepted_points_xy[i];
            const double y = diag.accepted_points_xy[i + 1];
            if (DoesFindLinePointRemain(keptPoints, x, y, tolerance))
            {
                keptAcceptedPoints.push_back(x);
                keptAcceptedPoints.push_back(y);
            }
        }

        if (keptAcceptedPoints.empty())
        {
            diag.accepted = false;
            diag.accepted_x = 0.0;
            diag.accepted_y = 0.0;
            diag.accepted_points_xy.clear();
            diag.reject_reason = "point_consistency_rejected";
            continue;
        }

        diag.accepted_points_xy.swap(keptAcceptedPoints);
        diag.accepted_x = diag.accepted_points_xy[0];
        diag.accepted_y = diag.accepted_points_xy[1];
        diag.reject_reason.clear();
    }
}

void FindLine::FitWeightedLeastSquares(FindLineMeasureProfileStats& stats)
{
    const std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    if (m_bestEdgeChain.size() < 2)
    {
        stats.fit_ms = ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
        return;
    }

    double weight_sum = 0.0;
    double mean_x = 0.0;
    double mean_y = 0.0;
    for (size_t i = 0; i < m_bestEdgeChain.size(); ++i)
    {
        const EdgeBandCandidate& candidate = m_bestEdgeChain[i];
        if (!candidate.valid)
            continue;
        const double weight = std::max(0.25, candidate.response_strength);
        weight_sum += weight;
        mean_x += candidate.x * weight;
        mean_y += candidate.y * weight;
    }
    if (weight_sum <= 0.0)
    {
        stats.fit_ms = ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
        return;
    }

    mean_x /= weight_sum;
    mean_y /= weight_sum;

    double cov_xx = 0.0;
    double cov_xy = 0.0;
    double cov_yy = 0.0;
    for (size_t i = 0; i < m_bestEdgeChain.size(); ++i)
    {
        const EdgeBandCandidate& candidate = m_bestEdgeChain[i];
        if (!candidate.valid)
            continue;
        const double weight = std::max(0.25, candidate.response_strength);
        const double dx = candidate.x - mean_x;
        const double dy = candidate.y - mean_y;
        cov_xx += weight * dx * dx;
        cov_xy += weight * dx * dy;
        cov_yy += weight * dy * dy;
    }

    const double trace = cov_xx + cov_yy;
    const double det_term = std::sqrt(std::max(0.0, (cov_xx - cov_yy) * (cov_xx - cov_yy) + 4.0 * cov_xy * cov_xy));
    const double lambda = 0.5 * (trace + det_term);

    double dir_x = cov_xy;
    double dir_y = lambda - cov_xx;
    if (std::abs(dir_x) < 1e-9 && std::abs(dir_y) < 1e-9)
    {
        dir_x = 1.0;
        dir_y = 0.0;
    }
    const double norm = std::sqrt(dir_x * dir_x + dir_y * dir_y);
    dir_x /= norm;
    dir_y /= norm;

    const double normal_x = -dir_y;
    const double normal_y = dir_x;
    double weighted_error_sum = 0.0;
    double weighted_error_norm = 0.0;
    double max_error = 0.0;
    for (size_t i = 0; i < m_bestEdgeChain.size(); ++i)
    {
        const EdgeBandCandidate& candidate = m_bestEdgeChain[i];
        if (!candidate.valid)
            continue;
        const double weight = std::max(0.25, candidate.response_strength);
        const double residual = std::abs((candidate.x - mean_x) * normal_x + (candidate.y - mean_y) * normal_y);
        weighted_error_sum += residual * weight;
        weighted_error_norm += weight;
        max_error = std::max(max_error, residual);
    }

    stats.fit_error_avg = weighted_error_norm > 0.0 ? weighted_error_sum / weighted_error_norm : 0.0;
    stats.fit_error_max = max_error;
    stats.line_angle = std::atan2(dir_y, dir_x);
    stats.line_offset = mean_x * normal_x + mean_y * normal_y;

    const gp_Rectangle bbox = m_measurepoints_w.size() > 0 || m_measurepoints_h.size() > 0
        ? ([&]() {
            PointsShape points;
            points.addpoints(m_measurepoints_w);
            points.addpoints(m_measurepoints_h);
            return points.boundingRect();
        })()
        : gp_Rectangle(gp_Pnt(0, 0, 0), 0, 0);
    m_measurepointsboundingRect = bbox;

    stats.fit_ms = ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
}

void FindLine::RefineJointConsistency(FindLineMeasureProfileStats& stats)
{
    const std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    if (m_bestEdgeChain.size() < 3)
    {
        stats.joint_refine_ms = ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
        return;
    }

    std::vector<EdgeBandCandidate> refined_chain;
    refined_chain.reserve(m_bestEdgeChain.size());
    refined_chain.push_back(m_bestEdgeChain.front());

    for (size_t i = 1; i + 1 < m_bestEdgeChain.size(); ++i)
    {
        EdgeBandCandidate current = m_bestEdgeChain[i];
        const EdgeBandCandidate& previous = m_bestEdgeChain[i - 1];
        const EdgeBandCandidate& next = m_bestEdgeChain[i + 1];

        if (!current.valid || !previous.valid || !next.valid)
            continue;

        if (previous.scan_type != current.scan_type || current.scan_type != next.scan_type)
        {
            refined_chain.push_back(current);
            continue;
        }

        const double expected_x = 0.5 * (previous.x + next.x);
        const double expected_y = 0.5 * (previous.y + next.y);
        const double dx = current.x - expected_x;
        const double dy = current.y - expected_y;
        const double deviation = std::sqrt(dx * dx + dy * dy);
        const double smooth_limit = std::max(2.5, 0.5 * (previous.width + next.width));
        const bool rank_switched = previous.edge_rank != current.edge_rank || current.edge_rank != next.edge_rank;
        if (rank_switched)
            ++stats.chain_switch_count;

        if (deviation > smooth_limit)
        {
            ++stats.neighbor_inconsistency_count;
            const double current_strength = std::max(0.25, current.response_strength);
            const double neighbor_strength = 0.5 * (std::max(0.25, previous.response_strength) + std::max(0.25, next.response_strength));

            if (current_strength <= neighbor_strength)
            {
                current.x = expected_x;
                current.y = expected_y;
                current.response_strength = std::max(current.response_strength, neighbor_strength);
            }
        }

        refined_chain.push_back(current);
    }

    refined_chain.push_back(m_bestEdgeChain.back());
    m_bestEdgeChain.swap(refined_chain);
    stats.joint_refine_ms = ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
}

void FindLine::SmartFilter(double dist, double filtnum)
{
    int igapvalue_h = m_ihgap;
    int igapvalue_w = m_iwgap;
    if (-1 == dist && -1 == filtnum)
    {
        m_measurepoints_h.FilterPoints(igapvalue_h *2, 3);
        m_measurepoints_w.FilterPoints(igapvalue_w *2, 3);
    }
    else if(dist>0 && filtnum>0)
    {
        m_measurepoints_h.FilterPoints(dist, filtnum);
        m_measurepoints_w.FilterPoints(dist, filtnum);
    }


}
void FindLine::PyrImage(Image& image)
{
    (void)image;
}
PointsShape& FindLine::getresultpointsw()
{
    return m_measurepoints_w;
}

const PointsShape& FindLine::getresultpointsw() const
{
    return m_measurepoints_w;
}
PointsShape& FindLine::getresultpointsh()
{
    return m_measurepoints_h;
}

const PointsShape& FindLine::getresultpointsh() const
{
    return m_measurepoints_h;
}
void FindLine::measure(void* pimage)
{
    LogFindlineMeasureProbe(
        "measure_void_enter",
        "running",
        std::string("pimage_null=") + (pimage == nullptr ? "true" : "false"));

    m_lastMeasureInputDebug = FindLineMeasureInputDebug();
    m_lastMeasureInputDebug.image_ptr_valid = (pimage != nullptr);
    m_lastMeasureInputDebug.method = m_iMethod;
    m_lastMeasureInputDebug.threshold = m_iThreshold;
    m_lastMeasureInputDebug.linegap = m_iSelectPointGap;
    m_lastMeasureInputDebug.wgap = m_iwgap;
    m_lastMeasureInputDebug.hgap = m_ihgap;
    m_lastMeasureInputDebug.selected_edge_index = m_iselectedgenum;
    m_lastMeasureInputDebug.point_consistency_enabled =
        m_point_consistency_enabled != 0 ? 1 : 0;
    m_lastMeasureInputDebug.point_consistency_range =
        static_cast<double>(m_point_consistency_range);
    m_lastMeasureInputDebug.boundary_response_enabled =
        m_boundary_response_enabled;
    m_lastMeasureInputDebug.boundary_response_mode =
        static_cast<int>(m_boundary_response_config.response_mode);
    m_lastMeasureInputDebug.boundary_response_polarity =
        static_cast<int>(m_boundary_response_config.polarity);
    m_lastMeasureInputDebug.boundary_response_selection =
        static_cast<int>(m_boundary_response_config.selection_mode);

    m_lastMeasureInputDebug.measure_geometry_request_valid =
        m_measure_geometry_request.valid;

    m_lastMeasureInputDebug.measure_geometry_dirty =
        m_measure_geometry_dirty;

    m_lastMeasureInputDebug.measure_geometry_ready =
        m_measure_geometry_ready;

    m_lastMeasureInputDebug.measure_geometry_version =
        m_measure_geometry_version;

    m_lastMeasureInputDebug.measure_geometry_built_version =
        m_measure_geometry_built_version;

    m_lastMeasureInputDebug.measure_geometry_half_width =
        m_measure_geometry_request.measure_half_width;

    if (pimage == nullptr)
    {
        m_lastMeasureInputDebug.failure_stage = "image_pointer_null";
        m_lastMeasureInputDebug.detail = "Findline.measure received null image pointer";
        ClearMeasureState();
        LogFindlineMeasureProbe(
            "measure_void_fail",
            "failed",
            "failure_stage=image_pointer_null");
        return;
    }

    Image* image = static_cast<Image*>(pimage);
    cv::Mat mat = image->getmat();
    m_lastMeasureInputDebug.image_mat_ready = !mat.empty();

    if (mat.empty())
    {
        m_lastMeasureInputDebug.failure_stage = "image_mat_empty";
        m_lastMeasureInputDebug.detail = "Image.getmat() is empty in Findline.measure";
        ClearMeasureState();
        LogFindlineMeasureProbe(
            "measure_void_fail",
            "failed",
            "failure_stage=image_mat_empty");
        return;
    }

    m_lastMeasureInputDebug.image_width = mat.cols;
    m_lastMeasureInputDebug.image_height = mat.rows;
    m_lastMeasureInputDebug.image_channels = mat.channels();
    m_lastMeasureInputDebug.image_type = mat.type();
    m_lastMeasureInputDebug.image_source = "Findline.measure(void*) parameter";

    if (!ImageManager::EnsureAlgorithmRuntimeResources(mat.cols, mat.rows))
    {
        m_lastMeasureInputDebug.failure_stage = "scan_workspace_unavailable";
        m_lastMeasureInputDebug.detail =
            "Findline.measure failed to initialize ImageManager work buffers.";
        ClearMeasureState();
        LogFindlineMeasureProbe(
            "measure_void_fail",
            "failed",
            "failure_stage=scan_workspace_unavailable, reason=EnsureAlgorithmRuntimeResources failed");
        return;
    }
    g_pbackimage = ImageManager::GetBackImage(1);
    g_pbackfindobject = ImageManager::Getbackfindobject(1);

    if (g_pbackimage == nullptr || g_pbackimage == image ||
        g_pbackimage->getmat().empty())
    {
        m_lastMeasureInputDebug.failure_stage = "scan_workspace_unavailable";
        m_lastMeasureInputDebug.detail =
            "Findline.measure requires an independent ImageManager BackImage workspace.";
        ClearMeasureState();
        LogFindlineMeasureProbe(
            "measure_void_fail",
            "failed",
            "failure_stage=scan_workspace_unavailable, backimage_null=" +
                std::string(g_pbackimage == nullptr ? "true" : "false") +
                ", backimage_alias_input=" +
                std::string(g_pbackimage == image ? "true" : "false"));
        return;
    }

    FindLineDisplaySnapshot displaySnapshot;
    if (getdisplaysnapshot(displaySnapshot))
    {
        m_lastMeasureInputDebug.has_line_roi = true;
        m_lastMeasureInputDebug.roi_x0 = displaySnapshot.x0;
        m_lastMeasureInputDebug.roi_y0 = displaySnapshot.y0;
        m_lastMeasureInputDebug.roi_x1 = displaySnapshot.x1;
        m_lastMeasureInputDebug.roi_y1 = displaySnapshot.y1;
        m_lastMeasureInputDebug.roi_scan_half_width = displaySnapshot.scan_half_width;
        m_lastMeasureInputDebug.line_dx =
            displaySnapshot.x1 - displaySnapshot.x0;
        m_lastMeasureInputDebug.line_dy =
            displaySnapshot.y1 - displaySnapshot.y0;
        m_lastMeasureInputDebug.line_length = std::sqrt(
            m_lastMeasureInputDebug.line_dx * m_lastMeasureInputDebug.line_dx +
            m_lastMeasureInputDebug.line_dy * m_lastMeasureInputDebug.line_dy);
        if (std::abs(m_lastMeasureInputDebug.line_dx) >=
            std::abs(m_lastMeasureInputDebug.line_dy))
        {
            m_lastMeasureInputDebug.line_orientation = "horizontal_like";
        }
        else
        {
            m_lastMeasureInputDebug.line_orientation = "vertical_like";
        }
        m_lastMeasureInputDebug.requested_tool_half_width =
            displaySnapshot.scale;
        m_lastMeasureInputDebug.effective_tool_half_width =
            displaySnapshot.scan_half_width;

        const double minX =
            std::min(displaySnapshot.scan_box_xy[0],
            std::min(displaySnapshot.scan_box_xy[2],
            std::min(displaySnapshot.scan_box_xy[4],
                     displaySnapshot.scan_box_xy[6])));

        const double maxX =
            std::max(displaySnapshot.scan_box_xy[0],
            std::max(displaySnapshot.scan_box_xy[2],
            std::max(displaySnapshot.scan_box_xy[4],
                     displaySnapshot.scan_box_xy[6])));

        const double minY =
            std::min(displaySnapshot.scan_box_xy[1],
            std::min(displaySnapshot.scan_box_xy[3],
            std::min(displaySnapshot.scan_box_xy[5],
                     displaySnapshot.scan_box_xy[7])));

        const double maxY =
            std::max(displaySnapshot.scan_box_xy[1],
            std::max(displaySnapshot.scan_box_xy[3],
            std::max(displaySnapshot.scan_box_xy[5],
                     displaySnapshot.scan_box_xy[7])));

        m_lastMeasureInputDebug.roi_intersects_image =
            CxRectIntersectsImage(minX, minY, maxX, maxY, mat.cols, mat.rows);

        m_lastMeasureInputDebug.roi_fully_inside_image =
            minX >= 0.0 &&
            minY >= 0.0 &&
            maxX < static_cast<double>(mat.cols) &&
            maxY < static_cast<double>(mat.rows);
    }
    else
    {
        m_lastMeasureInputDebug.failure_stage = "line_roi_not_initialized";
        m_lastMeasureInputDebug.detail = "Findline display snapshot unavailable before measure";
    }

    if (!m_lastMeasureInputDebug.has_line_roi)
    {
        ClearMeasureState();
        LogFindlineMeasureProbe(
            "measure_void_fail",
            "failed",
            "failure_stage=line_roi_not_initialized");
        return;
    }

    if (m_lastMeasureInputDebug.line_length <= 1.0e-6)
    {
        m_lastMeasureInputDebug.failure_stage = "line_roi_degenerate";
        m_lastMeasureInputDebug.detail =
            "Findline measure skipped because the ROI line length is zero.";
        ClearMeasureState();
        LogFindlineMeasureProbe(
            "measure_void_fail",
            "failed",
            "failure_stage=line_roi_degenerate");
        return;
    }

    if (!m_lastMeasureInputDebug.roi_intersects_image)
    {
        m_lastMeasureInputDebug.failure_stage = "line_roi_outside_image";
        m_lastMeasureInputDebug.detail =
            "Findline measure skipped because the scan box does not intersect the image.";
        ClearMeasureState();
        LogFindlineMeasureProbe(
            "measure_void_fail",
            "failed",
            FindlineMeasureMessage(
                "failure_stage=line_roi_outside_image",
                mat.cols,
                mat.rows,
                mat.channels(),
                RoundToInt(m_lastMeasureInputDebug.roi_x0),
                RoundToInt(m_lastMeasureInputDebug.roi_y0),
                RoundToInt(m_lastMeasureInputDebug.roi_x1),
                RoundToInt(m_lastMeasureInputDebug.roi_y1),
                static_cast<int>(m_lastMeasureInputDebug.roi_scan_half_width),
                ClampSizeToInt(m_lines_w.size()),
                ClampSizeToInt(m_lines_h.size()),
                0,
                g_pbackimage->getWidth(),
                g_pbackimage->getHeight()));
        return;
    }

    if (!m_lastMeasureInputDebug.roi_fully_inside_image)
    {
        m_lastMeasureInputDebug.failure_stage = "line_roi_partially_outside_image";
        m_lastMeasureInputDebug.detail =
            "Findline measure skipped because the complete scan box must be inside the image.";
        ClearMeasureState();
        LogFindlineMeasureProbe(
            "measure_void_fail",
            "failed",
            FindlineMeasureMessage(
                "failure_stage=line_roi_partially_outside_image",
                mat.cols,
                mat.rows,
                mat.channels(),
                RoundToInt(m_lastMeasureInputDebug.roi_x0),
                RoundToInt(m_lastMeasureInputDebug.roi_y0),
                RoundToInt(m_lastMeasureInputDebug.roi_x1),
                RoundToInt(m_lastMeasureInputDebug.roi_y1),
                static_cast<int>(m_lastMeasureInputDebug.roi_scan_half_width),
                ClampSizeToInt(m_lines_w.size()),
                ClampSizeToInt(m_lines_h.size()),
                0,
                g_pbackimage->getWidth(),
                g_pbackimage->getHeight()));
        return;
    }

    ProbeDisplayRoiGrayStats(*image);

    if (!EnsureOriginalMeasureGeometryReady())
    {
        ClearMeasureState();
        LogFindlineMeasureProbe(
            "measure_void_fail",
            "failed",
            "failure_stage=original_measure_geometry_not_ready");
        return;
    }

    const int scanCount =
        (wscanenabled() ? ClampSizeToInt(m_lines_w.size()) : 0) +
        (hscanenabled() ? ClampSizeToInt(m_lines_h.size()) : 0);
    int maxScanLength = 0;
    if (wscanenabled())
        for (LineShape& scan : m_lines_w)
            maxScanLength = std::max(maxScanLength, scan.getlinesize());
    if (hscanenabled())
        for (LineShape& scan : m_lines_h)
            maxScanLength = std::max(maxScanLength, scan.getlinesize());

    if (scanCount <= 0 || maxScanLength <= 0 ||
        scanCount > g_pbackimage->getHeight() ||
        maxScanLength > g_pbackimage->getWidth())
    {
        m_lastMeasureInputDebug.failure_stage = "scan_workspace_capacity_exceeded";
        m_lastMeasureInputDebug.detail =
            "Findline measure skipped because scan geometry exceeds the BackImage workspace.";
        ClearMeasureState();
        LogFindlineMeasureProbe(
            "measure_void_fail",
            "failed",
            FindlineMeasureMessage(
                "failure_stage=scan_workspace_capacity_exceeded_pre_measure",
                mat.cols,
                mat.rows,
                mat.channels(),
                RoundToInt(m_lastMeasureInputDebug.roi_x0),
                RoundToInt(m_lastMeasureInputDebug.roi_y0),
                RoundToInt(m_lastMeasureInputDebug.roi_x1),
                RoundToInt(m_lastMeasureInputDebug.roi_y1),
                static_cast<int>(m_lastMeasureInputDebug.roi_scan_half_width),
                ClampSizeToInt(m_lines_w.size()),
                ClampSizeToInt(m_lines_h.size()),
                maxScanLength,
                g_pbackimage->getWidth(),
                g_pbackimage->getHeight()));
        return;
    }

    LogFindlineMeasureProbe(
        "measure_void_before_measure_image",
        "running",
        FindlineMeasureMessage(
            "before Measure(Image&)",
            mat.cols,
            mat.rows,
            mat.channels(),
            RoundToInt(m_lastMeasureInputDebug.roi_x0),
            RoundToInt(m_lastMeasureInputDebug.roi_y0),
            RoundToInt(m_lastMeasureInputDebug.roi_x1),
            RoundToInt(m_lastMeasureInputDebug.roi_y1),
            static_cast<int>(m_lastMeasureInputDebug.roi_scan_half_width),
            ClampSizeToInt(m_lines_w.size()),
            ClampSizeToInt(m_lines_h.size()),
            maxScanLength,
            g_pbackimage->getWidth(),
            g_pbackimage->getHeight()));
    Measure(*image);
    LogFindlineMeasureProbe(
        "measure_void_after_measure_image",
        "finished",
        "points_w=" + std::to_string(m_measurepoints_w.size()) +
            ", points_h=" + std::to_string(m_measurepoints_h.size()) +
            ", failure_stage=" + m_lastMeasureInputDebug.failure_stage);

    const int originalPointCount =
        ClampSizeToInt(m_measurepoints_w.size()) +
        ClampSizeToInt(m_measurepoints_h.size());

    m_lastMeasureInputDebug.fallback_allowed =
        m_measure_fallback_mode > 0;
    m_lastMeasureInputDebug.fallback_used = false;

    if (originalPointCount == 0 &&
        m_measure_fallback_mode == 2)
    {
        m_lastMeasureInputDebug.original_failure_stage =
            m_lastMeasureInputDebug.failure_stage;
        m_lastMeasureInputDebug.original_detail =
            m_lastMeasureInputDebug.detail;

        FindLineMeasureProfileStats fallbackStats;
        if (MeasureSimpleRoiGradientPoints(*image, fallbackStats))
        {
            fallbackStats.total_ms = m_lastMeasureProfile.total_ms;
            m_lastMeasureProfile = fallbackStats;

            m_lastMeasureInputDebug.fallback_used = true;
            m_lastMeasureInputDebug.measure_source =
                "simple_roi_gradient_fallback";
            m_lastMeasureInputDebug.failure_stage =
                "simple_roi_gradient_fallback_used";
            m_lastMeasureInputDebug.detail =
                "Original Findline Measure produced zero points; "
                "explicit fallback generated measure points from ROI normal gradients.";
        }
        else
        {
            m_lastMeasureInputDebug.measure_source =
                "simple_roi_gradient_fallback_no_result";
            m_lastMeasureInputDebug.failure_stage =
                "simple_roi_gradient_fallback_no_result";
            m_lastMeasureInputDebug.detail =
                "Original Findline Measure and explicit ROI gradient fallback "
                "both produced fewer than two valid measure points.";
        }
    }
    else if (originalPointCount == 0 &&
             m_measure_fallback_mode == 1)
    {
        m_lastMeasureInputDebug.measure_source =
            "original_measure_pipeline_diagnostics_only";
    }
    ApplyPointConsistencyConstraint();
    ReconcilePointConsistencyDiagnostics();

    const int filteredPointCount =
        ClampSizeToInt(m_measurepoints_w.size()) +
        ClampSizeToInt(m_measurepoints_h.size());
    if (m_lastMeasureInputDebug.point_consistency_enabled != 0 &&
        m_lastMeasureInputDebug.point_consistency_input_points > 0 &&
        filteredPointCount <= 0)
    {
        m_lastMeasureInputDebug.failure_stage =
            "point_consistency_removed_all_points";
        m_lastMeasureInputDebug.detail =
            "Findline point consistency rejected all measured points.";
    }
    else if (m_lastMeasureInputDebug.point_consistency_enabled != 0 &&
             m_lastMeasureInputDebug.point_consistency_removed_points > 0)
    {
        m_lastMeasureInputDebug.detail =
            "Findline point consistency removed " +
            std::to_string(m_lastMeasureInputDebug.point_consistency_removed_points) +
            " discontinuous measured point(s).";
    }

    BuildBoundaryAnalysisSnapshot(*image);

}
void FindLine::ProbeDisplayRoiGrayStats(Image& image)
{
    cv::Mat src = image.getmat();
    if (src.empty())
        return;

    cv::Mat gray;
    if (src.channels() == 1)
    {
        gray = src;
    }
    else
    {
        cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    }

    FindLineDisplaySnapshot snapshot;
    if (!getdisplaysnapshot(snapshot) || !snapshot.has_line_roi)
        return;

    const double dx = snapshot.x1 - snapshot.x0;
    const double dy = snapshot.y1 - snapshot.y0;
    const double len = std::sqrt(dx * dx + dy * dy);

    if (len <= 1.0e-6)
        return;

    const double ux = dx / len;
    const double uy = dy / len;
    const double nx = -uy;
    const double ny = ux;

    const int alongStep = std::max(1, m_iSelectPointGap);
    const int alongCount = std::max(1, static_cast<int>(len / alongStep));
    const int halfWidth =
        std::max(2, static_cast<int>(std::round(snapshot.scan_half_width)));

    double sum = 0.0;
    int count = 0;
    int minV = 255;
    int maxV = 0;
    double maxGrad = 0.0;

    for (int i = 0; i <= alongCount; ++i)
    {
        const double t =
            static_cast<double>(i) / static_cast<double>(alongCount);

        const double cx = snapshot.x0 + ux * len * t;
        const double cy = snapshot.y0 + uy * len * t;

        int prev = -1;

        for (int s = -halfWidth; s <= halfWidth; ++s)
        {
            const int px =
                static_cast<int>(std::lround(cx + nx * s));
            const int py =
                static_cast<int>(std::lround(cy + ny * s));

            if (px < 0 || py < 0 || px >= gray.cols || py >= gray.rows)
                continue;

            const int v = static_cast<int>(gray.at<uchar>(py, px));

            minV = std::min(minV, v);
            maxV = std::max(maxV, v);
            sum += static_cast<double>(v);
            ++count;

            if (prev >= 0)
            {
                maxGrad = std::max(
                    maxGrad,
                    static_cast<double>(std::abs(v - prev)));
            }

            prev = v;
        }
    }

    m_lastMeasureInputDebug.sampled_pixel_count = count;

    if (count > 0)
    {
        m_lastMeasureInputDebug.gray_min = static_cast<double>(minV);
        m_lastMeasureInputDebug.gray_max = static_cast<double>(maxV);
        m_lastMeasureInputDebug.gray_mean = sum / static_cast<double>(count);
        m_lastMeasureInputDebug.max_gradient = maxGrad;
    }
}

bool FindLine::MeasureSimpleRoiGradientPoints(Image& image,
                                              FindLineMeasureProfileStats& stats)
{
    cv::Mat src = image.getmat();

    if (src.empty())
        return false;

    cv::Mat gray;

    if (src.channels() == 1)
    {
        gray = src;
    }
    else
    {
        cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    }

    FindLineDisplaySnapshot roi;

    if (!getdisplaysnapshot(roi) || !roi.has_line_roi || !roi.has_scan_box)
        return false;

    const double x0 = roi.x0;
    const double y0 = roi.y0;
    const double x1 = roi.x1;
    const double y1 = roi.y1;

    const double dx = x1 - x0;
    const double dy = y1 - y0;
    const double len = std::sqrt(dx * dx + dy * dy);

    if (len <= 1.0e-6)
        return false;

    const double ux = dx / len;
    const double uy = dy / len;

    const double nx = -uy;
    const double ny = ux;

    const int alongStep = std::max(1, m_iSelectPointGap);
    const int scanCount = std::max(2, static_cast<int>(len / alongStep));

    const int halfWidth =
        std::max(2, static_cast<int>(std::round(roi.scan_half_width)));

    const int edgeThreshold = std::max(1, m_iThreshold);

    m_scanEdgeBands.clear();
    m_bestEdgeChain.clear();
    m_measurepoints_w.clear();
    m_measurepoints_h.clear();

    int candidateCount = 0;

    for (int i = 0; i <= scanCount; ++i)
    {
        const double t =
            static_cast<double>(i) / static_cast<double>(scanCount);

        const double cx = x0 + ux * len * t;
        const double cy = y0 + uy * len * t;

        double bestStrength = 0.0;
        int bestS = 0;
        double bestPolarity = 0.0;

        for (int s = -halfWidth; s < halfWidth; ++s)
        {
            const double ax = cx + nx * static_cast<double>(s);
            const double ay = cy + ny * static_cast<double>(s);

            const double bx = cx + nx * static_cast<double>(s + 1);
            const double by = cy + ny * static_cast<double>(s + 1);

            const int iax = static_cast<int>(std::lround(ax));
            const int iay = static_cast<int>(std::lround(ay));
            const int ibx = static_cast<int>(std::lround(bx));
            const int iby = static_cast<int>(std::lround(by));

            if (iax < 0 || iax >= gray.cols ||
                ibx < 0 || ibx >= gray.cols ||
                iay < 0 || iay >= gray.rows ||
                iby < 0 || iby >= gray.rows)
            {
                continue;
            }

            const int va = static_cast<int>(gray.at<uchar>(iay, iax));
            const int vb = static_cast<int>(gray.at<uchar>(iby, ibx));

            const int diff = vb - va;
            const double strength = std::abs(diff);

            if (strength > bestStrength)
            {
                bestStrength = strength;
                bestS = s;
                bestPolarity = diff >= 0 ? 1.0 : -1.0;
            }
        }

        if (bestStrength < static_cast<double>(edgeThreshold))
            continue;

        const double px = cx + nx * (static_cast<double>(bestS) + 0.5);
        const double py = cy + ny * (static_cast<double>(bestS) + 0.5);

        if (!std::isfinite(px) || !std::isfinite(py))
            continue;

        EdgeBandCandidate candidate;
        candidate.scan_index = i;
        candidate.scan_type = 0;
        candidate.line_index = 0;
        candidate.candidate_index = candidateCount;
        candidate.start_index = bestS;
        candidate.end_index = bestS + 1;
        candidate.center_index = bestS;
        candidate.x = px;
        candidate.y = py;
        candidate.response_strength = bestStrength;
        candidate.polarity = bestPolarity;
        candidate.width = 1.0;
        candidate.edge_rank = 0;
        candidate.valid = true;

        ScanLineEdgeBands scan;
        scan.scan_index = i;
        scan.scan_type = 0;
        scan.bands.push_back(candidate);

        m_scanEdgeBands.push_back(scan);
        m_bestEdgeChain.push_back(candidate);

        m_measurepoints_w.addpoint(
            static_cast<Standard_Real>(px),
            static_cast<Standard_Real>(py));

        ++candidateCount;
    }

    stats.point_count = m_measurepoints_w.size();
    stats.edgeband_count = candidateCount;
    stats.chain_length = static_cast<int>(m_bestEdgeChain.size());

    if (stats.point_count > 0)
    {
        m_measurepointsboundingRect = m_measurepoints_w.boundingRect();
    }

    return stats.point_count >= 2;
}

void FindLine::measureRobust(void* pimage)
{
    if (pimage == nullptr)
    {
        m_lastMeasureInputDebug.failure_stage = "image_pointer_null";
        m_lastMeasureInputDebug.detail = "Findline.measureRobust received null image pointer";
        ClearMeasureState();

        return;
    }

    Image* image = static_cast<Image*>(pimage);
    cv::Mat mat = image->getmat();

    if (mat.empty())
    {
        m_lastMeasureInputDebug.failure_stage = "image_mat_empty";
        ClearMeasureState();

        return;
    }

    if (!ImageManager::EnsureAlgorithmRuntimeResources(mat.cols, mat.rows))
    {
        m_lastMeasureInputDebug.failure_stage = "scan_workspace_unavailable";
        ClearMeasureState();
        return;
    }

    g_pbackimage = ImageManager::GetBackImage(1);
    g_pbackfindobject = ImageManager::Getbackfindobject(1);

    if (g_pbackimage == nullptr || g_pbackimage == image ||
        g_pbackimage->getmat().empty())
    {
        m_lastMeasureInputDebug.failure_stage = "scan_workspace_unavailable";
        ClearMeasureState();
        return;
    }

    FindLineDisplaySnapshot displaySnapshot;
    if (getdisplaysnapshot(displaySnapshot))
    {
        m_lastMeasureInputDebug.has_line_roi = true;
        m_lastMeasureInputDebug.roi_x0 = displaySnapshot.x0;
        m_lastMeasureInputDebug.roi_y0 = displaySnapshot.y0;
        m_lastMeasureInputDebug.roi_x1 = displaySnapshot.x1;
        m_lastMeasureInputDebug.roi_y1 = displaySnapshot.y1;
    }

    MarkMeasureGeometryDirty();
    EnsureOriginalMeasureGeometryReady();
    MeasureRobust(*image);
}

void FindLine::pyrimage(void* pimage)
{
    Image* pgetimage = (Image*)pimage;
    if (pgetimage == nullptr)
        return;
    PyrImage(*pgetimage);
}
void FindLine::shapesetroi(void* pshape)
{
    if (pshape == nullptr)
        return;
    Shape::shapesetroi(pshape);
}
void FindLine::easycluster(int igapx, int igapy, int iclusternum)
{
    PointsShape resultpoints;
    resultpoints.addpoints(getresultpointsw());
    resultpoints.addpoints(getresultpointsh());
    vector<int> numlist;
    int isize = ClampSizeToInt(resultpoints.size());
    if (0 == isize)
        return;
    for (int i = 0; i < isize; i++)
    {
        numlist.push_back(1);
    }
    for (int i = 0; i < isize; i++)
    {
        int ix0 = RoundToInt(resultpoints.getx(i));
        int iy0 = RoundToInt(resultpoints.gety(i));
        if (i + 1 < isize)
            for (int j = i + 1; j < isize; j++)
            {
                int ix1 = RoundToInt(resultpoints.getx(j));
                int iy1 = RoundToInt(resultpoints.gety(j));
                if (abs(ix0 - ix1) < igapx
                    && abs(iy0 - iy1) < igapy)
                {
                    numlist[i]++;
                    numlist[j]++;
                }
            }
    }
    PointsShape amodelpointsw;
    PointsShape amodelpointsh;
    int isize1 = ClampSizeToInt(getresultpointsw().size());
    int isize2 = isize;
    getresultpointsw().clear();
    getresultpointsh().clear();
    int inum = 0;
    for (inum = 0; inum < isize1; inum++)
    {
        int ix0 = RoundToInt(resultpoints.getx(inum));
        int iy0 = RoundToInt(resultpoints.gety(inum));
        int inumsum = numlist[inum];
        if (inumsum > iclusternum)
        {
            getresultpointsw().addpoint(ix0, iy0);
        }
    }
    for (; inum < isize2; inum++)
    {
        int ix0 = RoundToInt(resultpoints.getx(inum));
        int iy0 = RoundToInt(resultpoints.gety(inum));
        int inumsum = numlist[inum];
        if (inumsum > iclusternum)
        {
            getresultpointsh().addpoint(ix0, iy0);
        }
    }
}
void FindLine::InflectionPoint(void* points)
{
    PointsShape* tpoints = (PointsShape*)points;
    if (tpoints == nullptr)
        return;
    if (m_measurepoints_h.size() > 0)
    {
        m_measurepoints_h.FindCrossPoints(points);
        return;
    }
    if (m_measurepoints_w.size() > 0)
    {
        m_measurepoints_w.FindCrossPoints(points);
        return;
    }
}

bool FindLine::getdisplaysnapshot(FindLineDisplaySnapshot& out) const
{
    out = FindLineDisplaySnapshot();

    if (!m_has_display_line_roi)
        return false;

    out.has_line_roi = true;
    out.x0 = static_cast<float>(m_display_line_x0);
    out.y0 = static_cast<float>(m_display_line_y0);
    out.x1 = static_cast<float>(m_display_line_x1);
    out.y1 = static_cast<float>(m_display_line_y1);
    out.scale = static_cast<float>(m_display_line_scale);

    out.wgap = m_iwgap;
    out.hgap = m_ihgap;
    out.linegap = m_iSelectPointGap;

    out.scan_half_width = std::max(
        1.0f,
        static_cast<float>(std::abs(m_display_line_scale)));

    const CxLineScanBoxSnapshot box =
        BuildCxLineScanBoxSnapshotFromHalfWidth(
            out.x0,
            out.y0,
            out.x1,
            out.y1,
            out.scan_half_width);

    out.has_scan_box = box.valid;

    if (box.valid)
    {
        out.scan_box_xy = box.xy;
        out.scan_half_width = box.half_width;
    }

    out.source = "FindLine::getdisplaysnapshot";

    return true;
}

void FindLine::exportmeasuredebugpoints(std::vector<float>& outXY) const
{
    outXY.clear();

    for (const ScanLineEdgeBands& scan : m_scanEdgeBands)
    {
        for (const EdgeBandCandidate& band : scan.bands)
        {
            if (!band.valid)
                continue;

            if (!std::isfinite(band.x) || !std::isfinite(band.y))
                continue;

            outXY.push_back(static_cast<float>(band.x));
            outXY.push_back(static_cast<float>(band.y));
        }
    }
}

int FindLine::getscandiagnosticcount() const
{
    return static_cast<int>(m_lastMeasureInputDebug.scan_diagnostics.size());
}

bool FindLine::getscandiagnostic(
    int index,
    FindLineMeasureInputDebug::ScanDiagnostic& out) const
{
    if (index < 0 ||
        index >= static_cast<int>(m_lastMeasureInputDebug.scan_diagnostics.size()))
    {
        return false;
    }
    out = m_lastMeasureInputDebug.scan_diagnostics[static_cast<std::size_t>(index)];
    return true;
}

bool FindLine::getscandiagnosticline(
    int scan_type,
    int scan_index,
    CxShapePoint& p0,
    CxShapePoint& p1) const
{
    return getscanline(scan_type, scan_index, p0, p1);
}

int FindLine::getscanlinecount(int scan_type) const
{
    if (scan_type == 0)
        return wscanenabled() ? static_cast<int>(m_lines_w.size()) : 0;
    if (scan_type == 1)
        return hscanenabled() ? static_cast<int>(m_lines_h.size()) : 0;
    return 0;
}

bool FindLine::getscanline(
    int scan_type,
    int scan_index,
    CxShapePoint& p0,
    CxShapePoint& p1) const
{
    const LineShape* line = nullptr;
    if (scan_type == 0)
    {
        if (!wscanenabled())
            return false;
        if (scan_index < 0 ||
            scan_index >= static_cast<int>(m_lines_w.size()))
        {
            return false;
        }
        line = &m_lines_w[static_cast<std::size_t>(scan_index)];
    }
    else if (scan_type == 1)
    {
        if (!hscanenabled())
            return false;
        if (scan_index < 0 ||
            scan_index >= static_cast<int>(m_lines_h.size()))
        {
            return false;
        }
        line = &m_lines_h[static_cast<std::size_t>(scan_index)];
    }
    else
    {
        return false;
    }
    return line != nullptr && line->exportLine(p0, p1);
}

bool FindLine::getscanlineprofilepoint(
    int scan_type,
    int scan_index,
    double normalized_profile_position,
    CxShapePoint& point)
{
    LineShape* scanLine = nullptr;
    if (scan_type == 0 && wscanenabled() && scan_index >= 0 &&
        scan_index < static_cast<int>(m_lines_w.size()))
    {
        scanLine = &m_lines_w[static_cast<std::size_t>(scan_index)];
    }
    else if (scan_type == 1 && hscanenabled() && scan_index >= 0 &&
             scan_index < static_cast<int>(m_lines_h.size()))
    {
        scanLine = &m_lines_h[static_cast<std::size_t>(scan_index)];
    }
    if (scanLine == nullptr || !std::isfinite(normalized_profile_position))
        return false;

    const int sampleCount = scanLine->getlinesize();
    if (sampleCount < 2)
        return false;

    const double normalized = std::max(
        0.0, std::min(1.0, normalized_profile_position));
    const gp_Pnt sampled = InterpolateFindLinePoint(
        *scanLine, normalized * static_cast<double>(sampleCount - 1),
        sampleCount);
    point.x = sampled.X();
    point.y = sampled.Y();
    return true;
}

void FindLine::setmaxelapsedms(int value)
{
    m_budget.max_elapsed_ms = value;
}

void FindLine::setmaxscanlines(int value)
{
    m_budget.max_scan_lines = value;
}

void FindLine::setmaxsamples(int value)
{
    m_budget.max_samples = value;
}

bool FindLine::budgetexceeded() const
{
    return m_budget_state.exceeded;
}

int FindLine::getelapsedms() const
{
    return m_budget_state.elapsed_ms;
}

int FindLine::getscanlinecount() const
{
    return m_budget_state.scan_line_count;
}

int FindLine::getsamplecount() const
{
    return m_budget_state.sample_count;
}

const std::string& FindLine::getfailurestage() const
{
    return m_lastMeasureInputDebug.failure_stage;
}

namespace
{
struct LineFitSample { double x; double y; double weight; };
struct LineFitResult
{
    bool valid = false;
    double x0 = 0.0, y0 = 0.0, x1 = 0.0, y1 = 0.0, avgdist = 0.0;
    int valid_points = 0;
};

LineFitResult FitWeightedTls(const std::vector<LineFitSample>& points)
{
    LineFitResult result;
    if (points.size() < 2) return result;
    double weight_sum = 0.0, mx = 0.0, my = 0.0;
    for (const auto& p : points)
    {
        const double w = std::isfinite(p.weight) && p.weight > 0.0 ? p.weight : 1.0;
        weight_sum += w; mx += w * p.x; my += w * p.y;
    }
    if (!(weight_sum > 0.0)) return result;
    mx /= weight_sum; my /= weight_sum;
    double sxx = 0.0, syy = 0.0, sxy = 0.0;
    for (const auto& p : points)
    {
        const double w = p.weight > 0.0 ? p.weight : 1.0;
        const double dx = p.x - mx, dy = p.y - my;
        sxx += w * dx * dx; syy += w * dy * dy; sxy += w * dx * dy;
    }
    if (!(sxx + syy > 0.0)) return result;
    const double theta = 0.5 * std::atan2(2.0 * sxy, sxx - syy);
    const double vx = std::cos(theta), vy = std::sin(theta);
    double min_t = std::numeric_limits<double>::infinity();
    double max_t = -std::numeric_limits<double>::infinity();
    double distance_sum = 0.0;
    for (const auto& p : points)
    {
        const double dx = p.x - mx, dy = p.y - my;
        const double t = dx * vx + dy * vy;
        min_t = std::min(min_t, t); max_t = std::max(max_t, t);
        distance_sum += std::abs(dx * -vy + dy * vx);
    }
    result.valid = std::isfinite(min_t) && std::isfinite(max_t) && max_t > min_t;
    result.x0 = mx + vx * min_t; result.y0 = my + vy * min_t;
    result.x1 = mx + vx * max_t; result.y1 = my + vy * max_t;
    result.avgdist = distance_sum / static_cast<double>(points.size());
    result.valid_points = static_cast<int>(points.size());
    return result;
}

LineFitResult FitMinimumZone(const std::vector<LineFitSample>& points)
{
    LineFitResult seed = FitWeightedTls(points);
    if (!seed.valid) return seed;
    const double seed_angle = std::atan2(seed.y1 - seed.y0, seed.x1 - seed.x0);
    double best_zone = std::numeric_limits<double>::infinity();
    double best_angle = seed_angle, best_min = 0.0, best_max = 0.0;
    for (int step = -180; step <= 180; ++step)
    {
        const double angle = seed_angle + step * (3.14159265358979323846 / 720.0);
        const double nx = -std::sin(angle), ny = std::cos(angle);
        double lo = std::numeric_limits<double>::infinity();
        double hi = -std::numeric_limits<double>::infinity();
        for (const auto& p : points) { const double d=p.x*nx+p.y*ny; lo=std::min(lo,d); hi=std::max(hi,d); }
        if (hi - lo < best_zone) { best_zone=hi-lo; best_angle=angle; best_min=lo; best_max=hi; }
    }
    const double vx=std::cos(best_angle), vy=std::sin(best_angle), nx=-vy, ny=vx;
    const double offset=(best_min+best_max)*0.5;
    double min_t=std::numeric_limits<double>::infinity(), max_t=-min_t, sum=0.0;
    for (const auto& p:points) { const double t=p.x*vx+p.y*vy; min_t=std::min(min_t,t); max_t=std::max(max_t,t); sum+=std::abs(p.x*nx+p.y*ny-offset); }
    LineFitResult r; r.valid=max_t>min_t; r.x0=vx*min_t+nx*offset; r.y0=vy*min_t+ny*offset;
    r.x1=vx*max_t+nx*offset; r.y1=vy*max_t+ny*offset; r.avgdist=sum/points.size(); r.valid_points=static_cast<int>(points.size()); return r;
}

LineFitResult FitRansac(const std::vector<LineFitSample>& points)
{
    if (points.size() < 2) return {};
    std::vector<std::size_t> best;
    const int trials = std::min(256, static_cast<int>(points.size() * points.size()));
    for (int trial=0; trial<trials; ++trial)
    {
        const std::size_t a=static_cast<std::size_t>((trial*37+3)%points.size());
        const std::size_t b=static_cast<std::size_t>((trial*91+17)%points.size());
        if (a==b) continue;
        const double dx=points[b].x-points[a].x, dy=points[b].y-points[a].y;
        const double len=std::hypot(dx,dy); if (!(len>0.0)) continue;
        std::vector<std::size_t> inliers;
        for (std::size_t i=0;i<points.size();++i)
            if (std::abs((points[i].x-points[a].x)*dy-(points[i].y-points[a].y)*dx)/len <= 2.0) inliers.push_back(i);
        if (inliers.size()>best.size()) best.swap(inliers);
    }
    if (best.size()<2) return {};
    std::vector<LineFitSample> inliers; inliers.reserve(best.size());
    for (std::size_t i:best) inliers.push_back(points[i]);
    return FitWeightedTls(inliers);
}

LineFitResult FitRansacWithThreshold(
    const std::vector<LineFitSample>& points,
    double threshold)
{
    if (points.size() < 2)
        return {};
    const double inlierThreshold = std::max(1.0, threshold);
    std::vector<std::size_t> best;
    double bestSpan = -1.0;
    const int pointCount = static_cast<int>(points.size());
    const int trials = std::min(512, pointCount * pointCount);
    for (int trial = 0; trial < trials; ++trial)
    {
        const std::size_t a =
            static_cast<std::size_t>((trial * 37 + 3) % points.size());
        const std::size_t b =
            static_cast<std::size_t>((trial * 91 + 17) % points.size());
        if (a == b)
            continue;
        const double dx = points[b].x - points[a].x;
        const double dy = points[b].y - points[a].y;
        const double len = std::hypot(dx, dy);
        if (!(len > 1e-6))
            continue;

        const double ux = dx / len;
        const double uy = dy / len;
        std::vector<std::size_t> inliers;
        double minT = std::numeric_limits<double>::infinity();
        double maxT = -std::numeric_limits<double>::infinity();
        for (std::size_t i = 0; i < points.size(); ++i)
        {
            const double dist =
                std::abs((points[i].x - points[a].x) * dy -
                         (points[i].y - points[a].y) * dx) / len;
            if (dist > inlierThreshold)
                continue;
            inliers.push_back(i);
            const double t = points[i].x * ux + points[i].y * uy;
            minT = std::min(minT, t);
            maxT = std::max(maxT, t);
        }

        const double span =
            std::isfinite(minT) && std::isfinite(maxT) ? maxT - minT : 0.0;
        if (inliers.size() > best.size() ||
            (inliers.size() == best.size() && span > bestSpan))
        {
            best.swap(inliers);
            bestSpan = span;
        }
    }
    if (best.size() < 2)
        return {};
    std::vector<LineFitSample> inliers;
    inliers.reserve(best.size());
    for (std::size_t i : best)
        inliers.push_back(points[i]);
    return FitWeightedTls(inliers);
}

LineFitResult FitAxisPriority(const std::vector<LineFitSample>& points)
{
    LineFitResult seed=FitWeightedTls(points); if(!seed.valid) return seed;
    const bool horizontal=std::abs(seed.x1-seed.x0)>=std::abs(seed.y1-seed.y0);
    double mean=0.0, lo=std::numeric_limits<double>::infinity(), hi=-lo, sum=0.0;
    for(const auto& p:points) mean += horizontal?p.y:p.x;
    mean/=points.size();
    for(const auto& p:points) { const double along=horizontal?p.x:p.y; lo=std::min(lo,along); hi=std::max(hi,along); sum+=std::abs((horizontal?p.y:p.x)-mean); }
    LineFitResult r; r.valid=hi>lo; r.x0=horizontal?lo:mean; r.y0=horizontal?mean:lo; r.x1=horizontal?hi:mean; r.y1=horizontal?mean:hi;
    r.avgdist=sum/points.size(); r.valid_points=static_cast<int>(points.size()); return r;
}
}

void FindLine::clearfitresult()
{
    m_result_x0=m_result_y0=m_result_x1=m_result_y1=m_result_avgdist=0.0;
    m_result_valid_points=0; m_has_fit_result=false;
}

void FindLine::setfitmode(int mode)
{
    m_fitline_mode = mode >= static_cast<int>(FitlineMode::LeastSquares) &&
        mode <= static_cast<int>(FitlineMode::WeightedMeasurementPoints)
        ? static_cast<FitlineMode>(mode) : FitlineMode::Unspecified;
}

void FindLine::setfitpointweight(int index, double weight)
{
    if (index < 0) return;
    if (m_fit_point_weights.size() <= static_cast<std::size_t>(index))
        m_fit_point_weights.resize(static_cast<std::size_t>(index)+1, 1.0);
    m_fit_point_weights[static_cast<std::size_t>(index)] = std::isfinite(weight) && weight > 0.0 ? weight : 1.0;
}

void FindLine::fitline() { fitline(m_fitline_mode); }

void FindLine::fitline(FitlineMode mode)
{
    clearfitresult();
    if (mode == FitlineMode::Unspecified) mode = FitlineMode::LeastSquares;
    m_fitline_mode = mode;

    m_point_consistency_removed_points = 0;
    m_lastMeasureInputDebug.point_consistency_enabled =
        m_point_consistency_enabled != 0 ? 1 : 0;
    m_lastMeasureInputDebug.point_consistency_range =
        static_cast<double>(m_point_consistency_range);
    m_lastMeasureInputDebug.point_consistency_input_points =
        ClampSizeToInt(m_measurepoints_w.size()) +
        ClampSizeToInt(m_measurepoints_h.size());
    m_lastMeasureInputDebug.point_consistency_output_points =
        m_lastMeasureInputDebug.point_consistency_input_points;
    m_lastMeasureInputDebug.point_consistency_removed_points = 0;

    auto append = [&](PointsShape& src, std::vector<LineFitSample>& dst)
    {
        for (int i = 0; i < src.size(); ++i)
        {
            const double x = src.getx(i);
            const double y = src.gety(i);
            if (std::isfinite(x) && std::isfinite(y))
                dst.push_back({ x, y, 1.0 });
        }
    };
    auto buildPointsForMode = [&]()
    {
        std::vector<LineFitSample> w;
        std::vector<LineFitSample> h;
        append(getresultpointsw(), w);
        append(getresultpointsh(), h);
        std::vector<LineFitSample> points;
        if (mode == FitlineMode::SingleEdge)
        {
            points = w.size() >= 2 ? w : h;
        }
        else if (mode == FitlineMode::EdgePairCenter)
        {
            const std::size_t count = std::min(w.size(), h.size());
            points.reserve(count);
            for (std::size_t i = 0; i < count; ++i)
                points.push_back({ (w[i].x + h[i].x) * 0.5,
                                   (w[i].y + h[i].y) * 0.5,
                                   1.0 });
        }
        else
        {
            points = w;
            points.insert(points.end(), h.begin(), h.end());
        }
        return points;
    };
    auto fitPointsForMode = [&](std::vector<LineFitSample> points)
    {
    if (mode == FitlineMode::WeightedMeasurementPoints)
            for (std::size_t i = 0; i < points.size(); ++i)
                points[i].weight = i < m_fit_point_weights.size() ? m_fit_point_weights[i] : 1.0;
        switch (mode)
        {
        case FitlineMode::MinimumZone: return FitMinimumZone(points);
        case FitlineMode::Ransac: return FitRansac(points);
        case FitlineMode::HorizontalVerticalPriority: return FitAxisPriority(points);
        default: return FitWeightedTls(points);
        }
    };
    auto filterPointsByFitLineDistance = [&](PointsShape& src,
                                             const LineFitResult& seed,
                                             double range)
    {
        const double dx = seed.x1 - seed.x0;
        const double dy = seed.y1 - seed.y0;
        const double length = std::sqrt(dx * dx + dy * dy);
        if (!(length > 1e-6) || range <= 0.0)
            return 0;
        const double ux = dx / length;
        const double uy = dy / length;
        const double nx = -dy / length;
        const double ny = dx / length;
        struct CandidatePoint
        {
            double x = 0.0;
            double y = 0.0;
            double t = 0.0;
            bool normal_pass = false;
        };
        std::vector<CandidatePoint> candidates;
        candidates.reserve(static_cast<std::size_t>(src.size()));
        for (int i = 0; i < src.size(); ++i)
        {
            const double x = src.getx(i);
            const double y = src.gety(i);
            if (!std::isfinite(x) || !std::isfinite(y) || x < -90000.0 || y < -90000.0)
            {
                candidates.push_back({ x, y, 0.0, false });
                continue;
            }
            const double distance =
                std::abs((x - seed.x0) * nx + (y - seed.y0) * ny);
            candidates.push_back({ x, y, (x - seed.x0) * ux + (y - seed.y0) * uy, distance <= range });
        }

        int normalPassCount = 0;
        for (const CandidatePoint& candidate : candidates)
            if (candidate.normal_pass)
                ++normalPassCount;
        const bool requireLocalSupport = normalPassCount >= 5;
        const double localSupportGap =
            std::max(range * 3.0, static_cast<double>(std::max(1, m_iSelectPointGap)) * 3.0);

        PointsShape kept;
        int removed = 0;
        for (std::size_t i = 0; i < candidates.size(); ++i)
        {
            const CandidatePoint& candidate = candidates[i];
            if (!candidate.normal_pass)
            {
                ++removed;
                continue;
            }

            bool hasLocalSupport = !requireLocalSupport;
            if (requireLocalSupport)
            {
                for (std::size_t j = 0; j < candidates.size(); ++j)
                {
                    if (i == j || !candidates[j].normal_pass)
                        continue;
                    if (std::abs(candidates[j].t - candidate.t) <= localSupportGap)
                    {
                        hasLocalSupport = true;
                        break;
                    }
                }
            }

            if (hasLocalSupport)
            {
                Standard_Real sx = static_cast<Standard_Real>(candidate.x);
                Standard_Real sy = static_cast<Standard_Real>(candidate.y);
                kept.addpoint(sx, sy);
            }
            else
            {
                ++removed;
            }
        }
        if (removed > 0)
        {
            src.clear();
            src.addpoints(kept);
        }
        return removed;
    };

    std::vector<LineFitSample> points = buildPointsForMode();
    LineFitResult result = fitPointsForMode(points);
    static const char* names[]={"Unspecified","LeastSquares","MinimumZone","Ransac","SingleEdge","EdgePairCenter","HorizontalVerticalPriority","WeightedMeasurementPoints"};
    const int mode_index=static_cast<int>(mode);
    if (!result.valid)
    {
        m_fitline_status=std::string("PENDING_BINDING: ")+names[mode_index]+" requires at least two non-degenerate valid points";
        return;
    }

    if (m_point_consistency_enabled != 0)
    {
        const double range = m_point_consistency_range > 0
            ? static_cast<double>(m_point_consistency_range)
            : std::max(1.0, m_measure_geometry_request.measure_half_width * 0.5);
        m_lastMeasureInputDebug.point_consistency_range = range;
        LineFitResult consistencySeed =
            FitRansacWithThreshold(points, range);
        if (!consistencySeed.valid)
            consistencySeed = result;
        PointsShape originalW;
        PointsShape originalH;
        originalW.addpoints(m_measurepoints_w);
        originalH.addpoints(m_measurepoints_h);
        const int inputPointCount =
            ClampSizeToInt(m_measurepoints_w.size()) +
            ClampSizeToInt(m_measurepoints_h.size());
        m_point_consistency_removed_points +=
            filterPointsByFitLineDistance(m_measurepoints_w, consistencySeed, range);
        m_point_consistency_removed_points +=
            filterPointsByFitLineDistance(m_measurepoints_h, consistencySeed, range);
        const int outputPointCount =
            ClampSizeToInt(m_measurepoints_w.size()) +
            ClampSizeToInt(m_measurepoints_h.size());
        const int minReasonableKept =
            std::max(2, (inputPointCount + 1) / 2);
        if (m_point_consistency_removed_points > 0 &&
            outputPointCount < minReasonableKept)
        {
            m_measurepoints_w.clear();
            m_measurepoints_w.addpoints(originalW);
            m_measurepoints_h.clear();
            m_measurepoints_h.addpoints(originalH);
            m_point_consistency_removed_points = 0;
            m_lastMeasureInputDebug.point_consistency_removed_points = 0;
            m_lastMeasureInputDebug.point_consistency_output_points =
                inputPointCount;
            m_lastMeasureInputDebug.detail +=
                " point_consistency skipped because robust seed kept too few points;";
        }
        else
        {
        m_lastMeasureInputDebug.point_consistency_removed_points =
            m_point_consistency_removed_points;
        m_lastMeasureInputDebug.point_consistency_output_points =
            outputPointCount;
        }

        if (m_point_consistency_removed_points > 0)
        {
            points = buildPointsForMode();
            result = fitPointsForMode(points);
            if (!result.valid)
            {
                m_fitline_status =
                    std::string("PENDING_BINDING: ") + names[mode_index] +
                    " point consistency removed too many points";
                return;
            }
        }
    }

    m_result_x0=result.x0; m_result_y0=result.y0; m_result_x1=result.x1; m_result_y1=result.y1;
    m_result_avgdist=result.avgdist; m_result_valid_points=result.valid_points; m_has_fit_result=true;
    m_fitline_status=std::string("geometry_result_available: ")+names[mode_index];
}



void FindLine::MarkMeasureGeometryDirty()
{
    m_measure_geometry_dirty = true;
    m_measure_geometry_ready = false;
    ++m_measure_geometry_version;
}

double FindLine::ComputeMeasureHalfWidthForLine(double x0,
                                                double y0,
                                                double x1,
                                                double y1) const
{
    const double dx = x1 - x0;
    const double dy = y1 - y0;
    const double len = std::sqrt(dx * dx + dy * dy);

    if (len <= 1.0e-9)
        return 1.0;

    const double nx = std::abs(-dy / len);
    const double ny = std::abs(dx / len);

    const double wx = static_cast<double>(std::max(1, m_iwgap));
    const double hy = static_cast<double>(std::max(1, m_ihgap));

    const double projectedHalfWidth = nx * wx + ny * hy;

    return std::max(1.0, projectedHalfWidth);
}

void FindLine::UpdateMeasureGeometryRequest(double x0,
                                            double y0,
                                            double x1,
                                            double y1,
                                            double scriptScale)
{
    m_measure_geometry_request.valid = true;

    m_measure_geometry_request.x0 = x0;
    m_measure_geometry_request.y0 = y0;
    m_measure_geometry_request.x1 = x1;
    m_measure_geometry_request.y1 = y1;

    m_measure_geometry_request.script_scale = scriptScale;

    m_measure_geometry_request.measure_half_width =
        std::max(1.0, std::abs(scriptScale));

    m_measure_geometry_request.wgap = m_iwgap;
    m_measure_geometry_request.hgap = m_ihgap;
    m_measure_geometry_request.linegap = m_iSelectPointGap;
    m_measure_geometry_request.min_edge_run_width_px =
        m_min_edge_run_width_px;
    m_measure_geometry_request.version = m_measure_geometry_version;

    MarkMeasureGeometryDirty();
}

void FindLine::BuildOriginalMeasureGeometryCore(double ix0,
                                                double iy0,
                                                double ix1,
                                                double iy1,
                                                double measureHalfWidth)
{
    if (!std::isfinite(ix0) || !std::isfinite(iy0) || !std::isfinite(ix1) || !std::isfinite(iy1) ||
        !std::isfinite(measureHalfWidth) || measureHalfWidth <= 0.0)
        return;

    const double dx = ix1 - ix0;
    const double dy = iy1 - iy0;
    const double dDist = std::sqrt(dx * dx + dy * dy);
    if (!std::isfinite(dDist) || dDist <= 1.0e-9)
        return;

    m_iwgap = PositiveGap(m_iwgap);
    m_ihgap = PositiveGap(m_ihgap);

    gp_Pnt ptRect[4];
    double kk2;

    if (iy0 == iy1)
    {
        ptRect[0].SetX(ix0);
        ptRect[0].SetY(iy0 - measureHalfWidth);
        ptRect[1].SetX(ix0);
        ptRect[1].SetY(iy0 + measureHalfWidth);
        ptRect[2].SetX(ix1);
        ptRect[2].SetY(iy1 + measureHalfWidth);
        ptRect[3].SetX(ix1);
        ptRect[3].SetY(iy1 - measureHalfWidth);

    }
    else
    {
        kk2 = (ix0 - ix1) / (iy0 - iy1);
        double theta = std::atan(kk2);
        if (theta < 0)
        {
            theta += CV_PI;
        }

        ptRect[0].SetX(ix0 + measureHalfWidth * std::cos(theta));
        ptRect[0].SetY(iy0 - measureHalfWidth * std::sin(theta));
        ptRect[1].SetX(ix0 - measureHalfWidth * std::cos(theta));
        ptRect[1].SetY(iy0 + measureHalfWidth * std::sin(theta));
        ptRect[2].SetX(ix1 - measureHalfWidth * std::cos(theta));
        ptRect[2].SetY(iy1 + measureHalfWidth * std::sin(theta));
        ptRect[3].SetX(ix1 + measureHalfWidth * std::cos(theta));
        ptRect[3].SetY(iy1 - measureHalfWidth * std::sin(theta));
    }
    m_LineA.setline(RoundToInt(ptRect[0].X()), RoundToInt(ptRect[0].Y()),
        RoundToInt(ptRect[1].X()), RoundToInt(ptRect[1].Y()));
    m_LineB.setline(RoundToInt(ptRect[1].X()), RoundToInt(ptRect[1].Y()),
        RoundToInt(ptRect[2].X()), RoundToInt(ptRect[2].Y()));
    m_LineA.setshow(0);
    m_LineB.setshow(0);
    for (int i = 0; i < m_lines_w.size(); i++)
    {
        m_lines_w[i].clear();
    }
    for (int i = 0; i < m_lines_h.size(); i++)
    {
        m_lines_h[i].clear();
    }
    m_lines_w.clear();
    m_lines_h.clear();

    for (int i = 0; i < m_l_measure_w_seek.size(); i++)
    {
        m_l_measure_w_seek[i].clear();
    }

    for (int i = 0; i < m_l_measure_h_seek.size(); i++)
    {
        m_l_measure_h_seek[i].clear();
    }
    m_l_measure_w_seek.clear();
    m_l_measure_h_seek.clear();

    double ilinesizeA = m_LineA.getlinedistance();
    double ilinesizeB = m_LineB.getlinedistance();
    const int roi_width_hint = CeilPositiveToInt(std::abs(ptRect[2].X() - ptRect[0].X()));
    const int roi_height_hint = CeilPositiveToInt(std::abs(ptRect[2].Y() - ptRect[0].Y()));
    const cxgeom::CxSetLineBuildMeta line_a_meta =
        BuildLineScanMeta(ptRect[0].X(), ptRect[0].Y(), ptRect[1].X(), ptRect[1].Y(), m_iwgap, roi_width_hint, roi_height_hint);
    const cxgeom::CxSetLineBuildMeta line_b_meta =
        BuildLineScanMeta(ptRect[1].X(), ptRect[1].Y(), ptRect[2].X(), ptRect[2].Y(), m_ihgap, roi_width_hint, roi_height_hint);
    const int dplinewbase = ComputeLineScanCount(ilinesizeA, m_iwgap, line_a_meta);
    const int dplinehbase = ComputeLineScanCount(ilinesizeB, m_ihgap, line_b_meta);
    const int dplinewsize = dplinewbase > 1 ? dplinewbase + 1 : dplinewbase;
    const int dplinehsize = dplinehbase > 1 ? dplinehbase + 1 : dplinehbase;
    const int dplinewdenom = std::max(1, dplinewsize - 1);
    const int dplinehdenom = std::max(1, dplinehsize - 1);

    double dlineax = (ptRect[1].X() - ptRect[0].X()) / dplinewdenom;
    double dlineay = (ptRect[1].Y() - ptRect[0].Y()) / dplinewdenom;

    double dlinebx = (ptRect[2].X() - ptRect[1].X()) / dplinehdenom;
    double dlineby = (ptRect[2].Y() - ptRect[1].Y()) / dplinehdenom;


    LineShape aline1, aline2;
    for (int i = 0; i < dplinewsize; i++)
    {
        m_lines_w.push_back(aline1);
        m_lines_w[i].copy(m_LineB);
        m_lines_w[i].Move(RoundToInt(-dlineax * static_cast<double>(i)),
            RoundToInt(-dlineay * static_cast<double>(i)));
        m_lines_w[i].setPercent(m_dsamplerate);
        m_lines_w[i].setshow(0);
    }
    for (int i = 0; i < dplinehsize; i++)
    {
        m_lines_h.push_back(aline2);
        m_lines_h[i].copy(m_LineA);
        m_lines_h[i].Move(RoundToInt(dlinebx * static_cast<double>(i)),
            RoundToInt(dlineby * static_cast<double>(i)));
        m_lines_h[i].setPercent(m_dsamplerate);
        m_lines_h[i].setshow(0);
    }

    Shape::setrect2(ptRect[0].X(), ptRect[0].Y(), ptRect[1].X(), ptRect[1].Y(),
        ptRect[2].X(), ptRect[2].Y(), ptRect[3].X(), ptRect[3].Y()
    );
}

bool FindLine::BuildOriginalMeasureGeometryFromRequest(
    const FindLineMeasureGeometryRequest& request)
{
    if (!request.valid)
        return false;

    const bool oldHasDisplay = m_has_display_line_roi;
    const double oldX0 = m_display_line_x0;
    const double oldY0 = m_display_line_y0;
    const double oldX1 = m_display_line_x1;
    const double oldY1 = m_display_line_y1;
    const double oldScale = m_display_line_scale;

    BuildOriginalMeasureGeometryCore(
        request.x0,
        request.y0,
        request.x1,
        request.y1,
        request.measure_half_width);

    m_has_display_line_roi = oldHasDisplay;
    m_display_line_x0 = oldX0;
    m_display_line_y0 = oldY0;
    m_display_line_x1 = oldX1;
    m_display_line_y1 = oldY1;
    m_display_line_scale = oldScale;

    return true;
}

bool FindLine::EnsureOriginalMeasureGeometryReady()
{
    if (!m_measure_geometry_request.valid)
    {
        m_lastMeasureInputDebug.failure_stage =
            "measure_geometry_request_invalid";

        m_lastMeasureInputDebug.detail =
            "Findline measure geometry request is invalid; call setline before measure.";

        return false;
    }

    if (!m_measure_geometry_dirty &&
        m_measure_geometry_ready &&
        m_measure_geometry_built_version ==
            m_measure_geometry_request.version)
    {
        return true;
    }

    const bool ok =
        BuildOriginalMeasureGeometryFromRequest(
            m_measure_geometry_request);

    m_measure_geometry_ready = ok;
    m_measure_geometry_dirty = !ok;

    if (ok)
    {
        m_measure_geometry_built_version =
            m_measure_geometry_request.version;
    }
    else
    {
        m_lastMeasureInputDebug.failure_stage =
            "measure_geometry_build_failed";

        m_lastMeasureInputDebug.detail =
            "BuildOriginalMeasureGeometryFromRequest failed.";
    }

    return ok;
}

void FindLine::PublishDisplayShapes(ICxShapeSink& sink, const std::string& owner_ref) const
{
    FindLineDisplaySnapshot snapshot;
    if (!getdisplaysnapshot(snapshot))
        return;

    if (snapshot.has_line_roi)
    {
        auto gauge_shape = std::make_unique<LineGaugeShape>(
            snapshot.x0, snapshot.y0,
            snapshot.x1, snapshot.y1,
            snapshot.scan_half_width);
        sink.UpsertShape(
            owner_ref + ".roi_axis",
            "FindLine",
            owner_ref,
            "setline",
            "roi",
            true,
            false,
            std::move(gauge_shape));
    }

    if (snapshot.has_scan_box)
    {
        auto box_shape = std::make_unique<PolylineShape>();
        const auto& xy = snapshot.scan_box_xy;
        box_shape->addPoint(xy[0], xy[1]);
        box_shape->addPoint(xy[2], xy[3]);
        box_shape->addPoint(xy[4], xy[5]);
        box_shape->addPoint(xy[6], xy[7]);
        box_shape->close(true);
        sink.UpsertShape(
            owner_ref + ".scan_box",
            "FindLine",
            owner_ref,
            "",
            "scan",
            false,
            false,
            std::move(box_shape));
    }

    const PointsShape& w_points = getresultpointsw();
    if (wscanenabled() && w_points.size() > 0)
    {
        auto pts_shape = std::make_unique<PointsShape>();
        for (int i = 0; i < w_points.size(); ++i)
        {
            pts_shape->addpoint(w_points.getx(i), w_points.gety(i));
        }
        sink.UpsertShape(
            owner_ref + ".measure_points_w",
            "FindLine",
            owner_ref,
            "",
            "measure_points",
            false,
            true,
            std::move(pts_shape));
    }

    const PointsShape& h_points = getresultpointsh();
    if (hscanenabled() && h_points.size() > 0)
    {
        auto pts_shape = std::make_unique<PointsShape>();
        for (int i = 0; i < h_points.size(); ++i)
        {
            pts_shape->addpoint(h_points.getx(i), h_points.gety(i));
        }
        sink.UpsertShape(
            owner_ref + ".measure_points_h",
            "FindLine",
            owner_ref,
            "",
            "measure_points",
            false,
            true,
            std::move(pts_shape));
    }

    if (hasfitresult())
    {
        auto fit_line = std::make_unique<LineShape>();
        fit_line->setline(
            static_cast<int>(std::lround(getresultx0())),
            static_cast<int>(std::lround(getresulty0())),
            static_cast<int>(std::lround(getresultx1())),
            static_cast<int>(std::lround(getresulty1())));
        sink.UpsertShape(
            owner_ref + ".fit_line",
            "FindLine",
            owner_ref,
            "",
            "result",
            false,
            true,
            std::move(fit_line));
    }
}


namespace
{
struct RobustFeatureGraphContext
{
    FeatureGraph graph;
    std::vector<std::vector<int>> components;
};

RobustFeatureGraphContext g_robust_ctx;

double DotProduct(const std::vector<double>& a, const std::vector<double>& b)
{
    const std::size_t n = std::min(a.size(), b.size());
    double sum = 0.0;
    for (std::size_t i = 0; i < n; ++i)
        sum += a[i] * b[i];
    return sum;
}

double Mean(const std::vector<double>& v)
{
    if (v.empty())
        return 0.0;
    double sum = 0.0;
    for (double x : v)
        sum += x;
    return sum / static_cast<double>(v.size());
}

double CalculateNCC(const std::vector<double>& a, const std::vector<double>& b)
{
    if (a.empty() || b.empty())
        return 0.0;

    const std::size_t n = std::min(a.size(), b.size());
    if (n < 2)
        return 0.0;

    std::vector<double> a_trim(a.begin(), a.begin() + static_cast<long>(n));
    std::vector<double> b_trim(b.begin(), b.begin() + static_cast<long>(n));

    const double mean_a = Mean(a_trim);
    const double mean_b = Mean(b_trim);

    double num = 0.0;
    double den_a = 0.0;
    double den_b = 0.0;

    for (std::size_t i = 0; i < n; ++i)
    {
        const double da = a_trim[i] - mean_a;
        const double db = b_trim[i] - mean_b;
        num += da * db;
        den_a += da * da;
        den_b += db * db;
    }

    const double den = std::sqrt(den_a * den_b);
    if (den < 1.0e-9)
        return 0.0;

    return std::max(-1.0, std::min(1.0, num / den));
}

std::vector<double> ExtractProfileAt(const cv::Mat& gray,
                                      double cx,
                                      double cy,
                                      int half_width,
                                      int scan_type)
{
    std::vector<double> profile;
    if (gray.empty() || half_width < 1)
        return profile;

    const int px = static_cast<int>(std::lround(cx));
    const int py = static_cast<int>(std::lround(cy));

    if (scan_type == 0)
    {
        for (int dy = -half_width; dy <= half_width; ++dy)
        {
            const int y = py + dy;
            if (px < 0 || px >= gray.cols || y < 0 || y >= gray.rows)
                break;
            profile.push_back(static_cast<double>(gray.at<uchar>(y, px)));
        }
    }
    else
    {
        for (int dx = -half_width; dx <= half_width; ++dx)
        {
            const int x = px + dx;
            if (x < 0 || x >= gray.cols || py < 0 || py >= gray.rows)
                break;
            profile.push_back(static_cast<double>(gray.at<uchar>(py, x)));
        }
    }

    return profile;
}
}

void FindLine::MeasureRobust(Image& image)
{
    const std::chrono::steady_clock::time_point total_begin = std::chrono::steady_clock::now();

    m_budget_state = CxAlgorithmBudgetState();

    MarkMeasureGeometryDirty();
    if (!EnsureOriginalMeasureGeometryReady())
    {

        m_lastMeasureProfile = FindLineMeasureProfileStats();
        return;
    }

    ClearMeasureState();

    m_fit_candidate_sequences.clear();
    m_best_sequence_index = -1;
    g_robust_ctx = RobustFeatureGraphContext();

    if (!ImageManager::EnsureAlgorithmRuntimeResources(
            image.getWidth(),
            image.getHeight()))
    {

        m_lastMeasureProfile = FindLineMeasureProfileStats();
        return;
    }

    g_pbackimage = ImageManager::GetBackImage(1);

    if (g_pbackimage == nullptr || g_pbackimage == &image ||
        g_pbackimage->getmat().empty())
    {
        m_lastMeasureProfile = FindLineMeasureProfileStats();
        return;
    }

    ProbeDisplayRoiGrayStats(image);

    FindLineMeasureProfileStats stats;


    BuildScanProfiles(image, stats, total_begin);
    if (m_budget_state.exceeded)
    {

        m_lastMeasureProfile = stats;
        return;
    }


    CollectEdgeBandsRobust(image, stats);
    if (m_scanEdgeBands.empty())
    {

        m_lastMeasureProfile = stats;
        return;
    }

    BuildFeatureGraph(stats);

    FindComponentsInGraph(stats);

    SelectBestSequence(stats);

    if (m_best_sequence_index >= 0 &&
        m_best_sequence_index < static_cast<int>(m_fit_candidate_sequences.size()))
    {
        ConvertSequenceToMeasurePoints(m_fit_candidate_sequences[m_best_sequence_index]);

        if (m_bestEdgeChain.size() >= 2)
        {
            RefineJointConsistency(stats);
            FitWeightedLeastSquares(stats);
        }
    }

    stats.point_count = m_measurepoints_w.size() + m_measurepoints_h.size();
    stats.chain_length = static_cast<int>(m_bestEdgeChain.size());
    stats.total_ms = ElapsedMilliseconds(total_begin, std::chrono::steady_clock::now());

    m_lastMeasureInputDebug.original_point_count = stats.point_count;
    m_lastMeasureInputDebug.original_edgeband_count = stats.edgeband_count;
    m_lastMeasureInputDebug.original_chain_length = stats.chain_length;

    if (stats.point_count > 0)
        m_lastMeasureInputDebug.measure_source = "robust_measure_pipeline";
    else
        m_lastMeasureInputDebug.measure_source = "robust_measure_pipeline_no_result";

    m_lastMeasureProfile = stats;
}

void FindLine::BuildScanProfilesRobust(Image& image,
                                        FindLineMeasureProfileStats& stats)
{
    const std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

    if (g_pbackimage == nullptr)
    {
        stats.profile_ms = ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
        return;
    }

    if (image.getWidth() < rect().TopLeft().X() + rect().Width()
        || image.getHeight() < rect().TopLeft().Y() + rect().Height()
        || rect().TopLeft().X() < 0
        || rect().TopLeft().Y() < 0)
    {
        stats.profile_ms = ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
        return;
    }

    const int iwsize = wscanenabled() ? ClampSizeToInt(m_lines_w.size()) : 0;
    const int ihsize = hscanenabled() ? ClampSizeToInt(m_lines_h.size()) : 0;

    if (iwsize <= 0 && ihsize <= 0)
    {
        stats.profile_ms = ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
        return;
    }

    int ilineslen1 = 0;
    int ilineslen2 = 0;
    if (iwsize > 0)
        ilineslen1 = m_lines_w[0].getlinesize();
    if (ihsize > 0)
        ilineslen2 = m_lines_h[0].getlinesize();

    const int iprocessw = ilineslen1 > ilineslen2 ? ilineslen1 : ilineslen2;
    if (iprocessw <= 0)
    {
        stats.profile_ms = ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
        return;
    }

    const int binaryPaddingRows = 2;
    const int binaryRoiHeight = iwsize + ihsize + binaryPaddingRows;

    if (binaryRoiHeight > g_pbackimage->getHeight() ||
        iprocessw > g_pbackimage->getWidth())
    {
        stats.profile_ms = ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
        return;
    }

    {
        cv::Mat& backMat = g_pbackimage->getmat();
        if (!backMat.empty())
        {
            const int clearWidth = std::min(iprocessw + 3, backMat.cols);
            const int clearHeight = std::min(binaryRoiHeight, backMat.rows);
            if (clearWidth > 0 && clearHeight > 0)
            {
                backMat(cv::Rect(0, 0, clearWidth, clearHeight))
                    .setTo(cv::Scalar(0, 0, 0));
            }
        }
    }

    for (int i = 0; i < iwsize; ++i)
    {
        m_lines_w[i].linecopyex(image, *g_pbackimage, 0, i);
    }
    for (int i = 0; i < ihsize; ++i)
    {
        m_lines_h[i].linecopyex(image, *g_pbackimage, 0, i + iwsize);
    }

    g_pbackimage->setroi(0, 0, iprocessw, binaryRoiHeight);
    g_pbackimage->roi_7blur_gap_mud_thre_bw(m_iThreshold, m_igamarate, m_iSelectPointGap, m_iMethod);

    if ((m_iobjfilterset & 0x01) && g_pbackfindobject != nullptr)
    {
        g_pbackfindobject->setrect(0, 0, iprocessw, binaryRoiHeight);

        m_effective_filter_borw = effectivefilterborw();
        m_effective_filter_min = effectivefiltermin();
        m_effective_filter_max = effectivefiltermax();

        g_pbackfindobject->setbrow(m_effective_filter_borw);
        g_pbackfindobject->setminmaxarea(
            ClampLongLongToInt(static_cast<long long>(m_effective_filter_min)),
            ClampLongLongToInt(static_cast<long long>(m_effective_filter_max)));
        RunFindObjectPrefilter(*g_pbackimage);
    }

    stats.profile_ms = ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
}

void FindLine::CollectEdgeBandsRobust(Image& image,
                                       FindLineMeasureProfileStats& stats)
{
    const std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

    if (g_pbackimage == nullptr)
    {
        stats.edgeband_ms = ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
        return;
    }

    cv::Mat gray_src;
    const cv::Mat& src_mat = image.getmat();
    if (!src_mat.empty())
    {
        if (src_mat.channels() == 1)
            gray_src = src_mat.clone();
        else
            cv::cvtColor(src_mat, gray_src, cv::COLOR_BGR2GRAY);
    }

    const int max_band_width = 70;
    const int profile_half_width = 5;

    const int iwsize = wscanenabled() ? ClampSizeToInt(m_lines_w.size()) : 0;
    const int ihsize = hscanenabled() ? ClampSizeToInt(m_lines_h.size()) : 0;
    const int ilineslen1 = iwsize > 0 ? m_lines_w[0].getlinesize() : 0;
    const int ilineslen2 = ihsize > 0 ? m_lines_h[0].getlinesize() : 0;

    auto append_scan_band_robust = [&](int scan_type,
                                        int line_index,
                                        int row_index,
                                        int line_length)
    {
        ScanLineEdgeBands scan_bands;
        scan_bands.scan_index = static_cast<int>(m_scanEdgeBands.size());
        scan_bands.scan_type = scan_type;

        bool collecting = false;
        int band_start = 0;
        int band_rank = 0;

        auto finalize_band = [&](int band_end)
        {
            if (!collecting)
                return;

            const int band_width = band_end - band_start + 1;
            collecting = false;
            if (band_width <= 0 || band_width > max_band_width)
                return;

            const int center_index = m_ineedfixs + band_start + (band_width >> 1);
            if (center_index <= m_iSelectPointGap + 3 ||
                center_index >= line_length - m_iSelectPointGap - 3)
                return;

            EdgeBandCandidate candidate;
            candidate.scan_index = scan_bands.scan_index;
            candidate.scan_type = scan_type;
            candidate.line_index = line_index;
            candidate.candidate_index = static_cast<int>(scan_bands.bands.size());
            candidate.start_index = band_start;
            candidate.end_index = band_end;
            candidate.center_index = center_index;
            candidate.width = static_cast<double>(band_width);
            candidate.edge_rank = band_rank++;
            candidate.response_strength = static_cast<double>(band_width);
            candidate.polarity = m_iMethod == 0 ? 1.0 : -1.0;
            candidate.valid = (m_iselectedgenum == 0 || (candidate.edge_rank + 1) == m_iselectedgenum);

            const gp_Pnt point = scan_type == 0
                ? m_lines_w[line_index].getlinepoint(center_index)
                : m_lines_h[line_index].getlinepoint(center_index);
            candidate.x = point.X();
            candidate.y = point.Y();

            if (!gray_src.empty())
            {
                candidate.profile = ExtractProfileAt(
                    gray_src, candidate.x, candidate.y, profile_half_width, scan_type);
            }

            if (candidate.profile.size() < 3)
                candidate.profile.clear();

            scan_bands.bands.push_back(candidate);
        };

        for (int col_index = 0; col_index < line_length; ++col_index)
        {
            const int sample_row = ResolveFindLineBinarySampleRow(
                g_pbackimage, row_index, line_index,
                scan_type == 0 ? iwsize : ihsize,
                line_length);
            const cv::Vec3b color = g_pbackimage->pixel(col_index, sample_row);
            const bool active = color[0] > 0;
            if (active && !collecting)
            {
                collecting = true;
                band_start = col_index;
            }
            else if (!active && collecting)
            {
                finalize_band(col_index - 1);
            }
        }

        if (collecting)
            finalize_band(line_length - 1);

        if (m_iselectedgenum == -1 && !scan_bands.bands.empty())
        {
            for (auto& candidate : scan_bands.bands)
                candidate.valid = false;
            scan_bands.bands.back().valid = true;
        }

        if (!scan_bands.bands.empty())
            m_scanEdgeBands.push_back(scan_bands);
    };

    for (int line_index = 0; line_index < iwsize; ++line_index)
        append_scan_band_robust(0, line_index, line_index, ilineslen1);

    for (int line_index = 0; line_index < ihsize; ++line_index)
        append_scan_band_robust(1, line_index, iwsize + line_index, ilineslen2);

    stats.edgeband_count = 0;
    for (size_t i = 0; i < m_scanEdgeBands.size(); ++i)
        stats.edgeband_count += static_cast<int>(m_scanEdgeBands[i].bands.size());

    if (stats.edgeband_count <= 0)
    {
        m_lastMeasureInputDebug.failure_stage = "no_edge_band_candidates";
    }

    stats.edgeband_ms = ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
}

void FindLine::BuildFeatureGraph(FindLineMeasureProfileStats& stats)
{
    const std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

    g_robust_ctx = RobustFeatureGraphContext();

    if (m_scanEdgeBands.empty())
    {
        stats.graph_ms = ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
        return;
    }

    const double T_space = 10.0;
    const double T_width = 5.0;
    const double T_ncc = 0.85;

    int total_candidates = 0;
    for (const auto& scan : m_scanEdgeBands)
        total_candidates += static_cast<int>(scan.bands.size());

    g_robust_ctx.graph.nodes.reserve(static_cast<std::size_t>(total_candidates));

    int node_id = 0;
    for (const auto& scan : m_scanEdgeBands)
    {
        for (const auto& band : scan.bands)
        {
            FeatureGraphNode node;
            node.id = node_id++;
            node.candidate = band;
            node.visited = false;
            node.component_id = -1;
            g_robust_ctx.graph.nodes.push_back(node);
        }
    }

    int scan_offset = 0;
    std::vector<int> scan_start_offsets;
    for (const auto& scan : m_scanEdgeBands)
    {
        scan_start_offsets.push_back(scan_offset);
        scan_offset += static_cast<int>(scan.bands.size());
    }
    scan_start_offsets.push_back(scan_offset);

    for (std::size_t scan_idx = 0; scan_idx + 1 < m_scanEdgeBands.size(); ++scan_idx)
    {
        const auto& current_scan = m_scanEdgeBands[scan_idx];
        const auto& next_scan = m_scanEdgeBands[scan_idx + 1];

        const int current_offset = scan_start_offsets[scan_idx];
        const int next_offset = scan_start_offsets[scan_idx + 1];

        for (std::size_t ci = 0; ci < current_scan.bands.size(); ++ci)
        {
            const EdgeBandCandidate& cand_a = current_scan.bands[ci];
            if (!cand_a.valid || cand_a.profile.size() < 3)
                continue;

            const int node_a_id = current_offset + static_cast<int>(ci);

            for (std::size_t cj = 0; cj < next_scan.bands.size(); ++cj)
            {
                const EdgeBandCandidate& cand_b = next_scan.bands[cj];
                if (!cand_b.valid || cand_b.profile.size() < 3)
                    continue;

                const double spatial_dist = std::abs(
                    cand_a.scan_type == 0
                        ? cand_a.x - cand_b.x
                        : cand_a.y - cand_b.y);

                if (spatial_dist > T_space)
                    continue;

                const double width_diff = std::abs(cand_a.width - cand_b.width);
                if (width_diff > T_width)
                    continue;

                if (cand_a.polarity != cand_b.polarity)
                    continue;

                const double ncc = CalculateNCC(cand_a.profile, cand_b.profile);
                if (ncc <= T_ncc)
                    continue;

                const int node_b_id = next_offset + static_cast<int>(cj);

                FeatureGraphEdge edge;
                edge.node_a = node_a_id;
                edge.node_b = node_b_id;
                edge.ncc_score = ncc;
                edge.spatial_distance = spatial_dist;
                edge.valid = true;
                g_robust_ctx.graph.edges.push_back(edge);

                g_robust_ctx.graph.nodes[node_a_id].neighbors.push_back(node_b_id);
                g_robust_ctx.graph.nodes[node_b_id].neighbors.push_back(node_a_id);
            }
        }
    }

    stats.graph_ms = ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
}

void FindLine::FindComponentsInGraph(FindLineMeasureProfileStats& stats)
{
    const std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

    g_robust_ctx.components.clear();

    FeatureGraph& graph = g_robust_ctx.graph;
    const int node_count = static_cast<int>(graph.nodes.size());

    std::vector<bool> visited(static_cast<std::size_t>(node_count), false);

    for (int start = 0; start < node_count; ++start)
    {
        if (visited[start])
            continue;

        std::vector<int> component;
        std::vector<int> stack;
        stack.push_back(start);

        while (!stack.empty())
        {
            const int current = stack.back();
            stack.pop_back();

            if (visited[current])
                continue;

            visited[current] = true;
            component.push_back(current);

            for (int neighbor : graph.nodes[static_cast<std::size_t>(current)].neighbors)
            {
                if (!visited[static_cast<std::size_t>(neighbor)])
                    stack.push_back(neighbor);
            }
        }

        if (static_cast<int>(component.size()) >= 2)
        {
            for (int node_id : component)
                graph.nodes[static_cast<std::size_t>(node_id)].component_id =
                    static_cast<int>(g_robust_ctx.components.size());

            g_robust_ctx.components.push_back(component);
        }
        else
        {
            for (int node_id : component)
                visited[static_cast<std::size_t>(node_id)] = false;
        }
    }

    if (!g_robust_ctx.components.empty())
    {
        m_lastMeasureInputDebug.failure_stage = "graph_components_found";
    }

    stats.graph_ms += ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
}

void FindLine::SelectBestSequence(FindLineMeasureProfileStats& stats)
{
    const std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

    m_fit_candidate_sequences.clear();
    m_best_sequence_index = -1;

    if (g_robust_ctx.components.empty())
    {
        stats.path_ms = ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
        return;
    }

    FeatureGraph& graph = g_robust_ctx.graph;

    for (const auto& component : g_robust_ctx.components)
    {
        FitCandidateSequence seq;
        seq.valid = true;
        seq.node_count = static_cast<int>(component.size());

        double total_response = 0.0;
        double sum_ncc = 0.0;
        int edge_count = 0;

        std::vector<const EdgeBandCandidate*> component_candidates;
        component_candidates.reserve(component.size());

        for (int node_id : component)
        {
            const FeatureGraphNode& node = graph.nodes[static_cast<std::size_t>(node_id)];
            component_candidates.push_back(&node.candidate);
            total_response += node.candidate.response_strength;
        }

        std::sort(component_candidates.begin(), component_candidates.end(),
            [](const EdgeBandCandidate* a, const EdgeBandCandidate* b)
            {
                if (a->scan_index != b->scan_index)
                    return a->scan_index < b->scan_index;
                return a->scan_type < b->scan_type;
            });

        for (const EdgeBandCandidate* cand : component_candidates)
        {
            seq.points.push_back(cv::Point2d(cand->x, cand->y));
        }

        for (const auto& edge : graph.edges)
        {
            bool a_in_comp = false;
            bool b_in_comp = false;
            for (int nid : component)
            {
                if (nid == edge.node_a) a_in_comp = true;
                if (nid == edge.node_b) b_in_comp = true;
            }
            if (a_in_comp && b_in_comp)
            {
                sum_ncc += edge.ncc_score;
                ++edge_count;
            }
        }

        seq.avg_ncc = edge_count > 0 ? sum_ncc / static_cast<double>(edge_count) : 0.0;
        seq.total_response = total_response;
        seq.score = seq.node_count * 100.0 + seq.avg_ncc * 50.0 + total_response;

        m_fit_candidate_sequences.push_back(seq);
    }

    std::sort(m_fit_candidate_sequences.begin(), m_fit_candidate_sequences.end(),
        [](const FitCandidateSequence& a, const FitCandidateSequence& b)
        {
            return a.score > b.score;
        });

    if (m_fit_candidate_sequences.size() > 5)
        m_fit_candidate_sequences.resize(5);

    if (!m_fit_candidate_sequences.empty())
        m_best_sequence_index = 0;

    stats.path_ms = ElapsedMilliseconds(begin, std::chrono::steady_clock::now());
}

void FindLine::ConvertSequenceToMeasurePoints(FitCandidateSequence& seq)
{
    m_measurepoints_w.clear();
    m_measurepoints_h.clear();
    m_bestEdgeChain.clear();

    if (!seq.valid || seq.points.empty())
        return;

    for (const auto& pt : seq.points)
    {
        Standard_Real x = pt.x;
        Standard_Real y = pt.y;

        bool added = false;

        for (const auto& scan : m_scanEdgeBands)
        {
            for (const auto& band : scan.bands)
            {
                if (!band.valid)
                    continue;
                if (std::abs(band.x - pt.x) < 1.5 && std::abs(band.y - pt.y) < 1.5)
                {
                    if (band.scan_type == 0)
                        m_measurepoints_w.addpoint(x, y);
                    else
                        m_measurepoints_h.addpoint(x, y);

                    m_bestEdgeChain.push_back(band);
                    added = true;
                    break;
                }
            }
            if (added)
                break;
        }

        if (!added)
        {
            if (std::abs(pt.x) > std::abs(pt.y))
                m_measurepoints_w.addpoint(x, y);
            else
                m_measurepoints_h.addpoint(x, y);
        }
    }

    if (m_measurepoints_w.size() + m_measurepoints_h.size() > 0)
    {
        PointsShape all_points;
        all_points.addpoints(m_measurepoints_w);
        all_points.addpoints(m_measurepoints_h);
        m_measurepointsboundingRect = all_points.boundingRect();
    }
}
