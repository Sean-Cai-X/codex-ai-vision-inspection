#include "pch.h"

#include "FindRect.h"
#include "CxCoreBoundary.h"
#include "CxCoreGeometryAttach.h"
#include "imagemanager.h"
#include "ImageAnnotationLayer.h"
#include "RectShape.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <opencv2/imgproc.hpp>

namespace
{
struct FittedLine
{
    bool valid = false;
    double nx = 0.0;
    double ny = 0.0;
    double c = 0.0;
    int point_count = 0;
    double fit_error = std::numeric_limits<double>::max();
    double angle = 0.0;
};

double FiniteOr(double value, double fallback)
{
    return std::isfinite(value) ? value : fallback;
}

double NonNegativeOr(double value, double fallback = 0.0)
{
    return std::isfinite(value) && value >= 0.0 ? value : fallback;
}

int ClampNonNegativeInt(int value)
{
    return std::max(0, value);
}

int ClampPositiveInt(int value, int fallback = 1)
{
    return value > 0 ? value : fallback;
}

void NormalizeRange(int& minimum, int& maximum)
{
    minimum = ClampNonNegativeInt(minimum);
    maximum = ClampNonNegativeInt(maximum);
    if (maximum < minimum)
    {
        maximum = minimum;
    }
}

double NormalizeUnitRatio(double value, double fallback)
{
    if (!std::isfinite(value))
    {
        return fallback;
    }
    return std::max(0.0, std::min(1.0, value));
}

gp_Rectangle NormalizeRectangle(const gp_Rectangle& rect)
{
    const double x0 = FiniteOr(rect.TopLeft().X(), 0.0);
    const double y0 = FiniteOr(rect.TopLeft().Y(), 0.0);
    const double x1 = FiniteOr(rect.BottomRight().X(), x0);
    const double y1 = FiniteOr(rect.BottomRight().Y(), y0);
    return gp_Rectangle(
        gp_Pnt(std::min(x0, x1), std::min(y0, y1), 0.0),
        gp_Pnt(std::max(x0, x1), std::max(y0, y1), 0.0));
}

bool HasPositiveArea(const gp_Rectangle& rect)
{
    return std::isfinite(rect.Width()) && std::isfinite(rect.Height()) &&
        std::fabs(rect.Width()) > 0.0 && std::fabs(rect.Height()) > 0.0;
}

int RoundNonNegativeToInt(double value)
{
    if (!std::isfinite(value) || value <= 0.0)
    {
        return 0;
    }
    const double max_int = static_cast<double>(std::numeric_limits<int>::max());
    return static_cast<int>(std::min(max_int, std::round(value)));
}

PointsShape CollectLinePoints(FindLine& finder)
{
    PointsShape points;
    points.addpoints(finder.getresultpointsw());
    points.addpoints(finder.getresultpointsh());
    return points;
}

FittedLine FitLineFromPoints(PointsShape& points)
{
    FittedLine line;
    const int size = points.size();
    if (size < 2)
        return line;

    double mean_x = 0.0;
    double mean_y = 0.0;
    for (int i = 0; i < size; ++i)
    {
        mean_x += points.getx(i);
        mean_y += points.gety(i);
    }
    mean_x /= size;
    mean_y /= size;

    double cov_xx = 0.0;
    double cov_xy = 0.0;
    double cov_yy = 0.0;
    for (int i = 0; i < size; ++i)
    {
        const double dx = points.getx(i) - mean_x;
        const double dy = points.gety(i) - mean_y;
        cov_xx += dx * dx;
        cov_xy += dx * dy;
        cov_yy += dy * dy;
    }

    const double theta = 0.5 * std::atan2(2.0 * cov_xy, cov_xx - cov_yy);
    const double dir_x = std::cos(theta);
    const double dir_y = std::sin(theta);
    const double normal_x = -dir_y;
    const double normal_y = dir_x;
    const double normal_norm = std::sqrt(normal_x * normal_x + normal_y * normal_y);
    if (normal_norm <= 1e-6)
        return line;

    line.valid = true;
    line.nx = normal_x / normal_norm;
    line.ny = normal_y / normal_norm;
    line.c = mean_x * line.nx + mean_y * line.ny;
    line.point_count = size;
    line.angle = theta;
    return line;
}

bool IntersectLines(const FittedLine& a, const FittedLine& b, gp_Pnt& point)
{
    const double det = a.nx * b.ny - a.ny * b.nx;
    if (std::fabs(det) <= 1e-6)
        return false;

    const double x = (a.c * b.ny - a.ny * b.c) / det;
    const double y = (a.nx * b.c - a.c * b.nx) / det;
    point = gp_Pnt(x, y, 0.0);
    return true;
}

FittedLine ToFittedLine(const FindRect::EdgeLearnResult::FittedLineData& data)
{
    FittedLine line;
    line.valid = data.valid;
    line.nx = data.nx;
    line.ny = data.ny;
    line.c = data.c;
    line.point_count = data.point_count;
    line.fit_error = data.fit_error;
    line.angle = data.angle;
    return line;
}

int SelectGauge(const gp_Rectangle& rect, int manual_gauge)
{
    if (manual_gauge > 0)
        return manual_gauge;

    const int width = std::max(1, static_cast<int>(std::round(std::fabs(rect.Width()))));
    const int height = std::max(1, static_cast<int>(std::round(std::fabs(rect.Height()))));
    const int compact_side = std::min(width, height);
    return std::max(3, std::min(24, compact_side / 6));
}

double NormalizeLineAngleDelta(double a, double b)
{
    if (!std::isfinite(a) || !std::isfinite(b))
        return 0.0;
    double delta = std::fabs(a - b);
    const double kPi = 3.14159265358979323846;
    while (delta > kPi)
        delta -= kPi;
    if (delta > kPi / 2.0)
        delta = kPi - delta;
    return std::fabs(delta);
}

double ComputeEdgeScore(const FindRect::EdgeLearnResult& edge)
{
    if (!edge.valid)
        return -std::numeric_limits<double>::max();

    const double fit_error_penalty = std::min(20.0, edge.stats.fit_error_avg + edge.stats.fit_error_max);
    const double chain_bonus = static_cast<double>(edge.stats.chain_length);
    const double point_bonus = static_cast<double>(edge.line.point_count) * 2.0;
    const double band_bonus = static_cast<double>(edge.stats.edgeband_count);
    const double switch_penalty = static_cast<double>(edge.stats.chain_switch_count) * 2.0;
    const double inconsistency_penalty = static_cast<double>(edge.stats.neighbor_inconsistency_count) * 1.5;
    return point_bonus + chain_bonus + band_bonus - fit_error_penalty - switch_penalty - inconsistency_penalty;
}

double ComputeRectScore(const FindRect::RectLearnResult& result)
{
    if (!result.valid)
        return -std::numeric_limits<double>::max();

    const double top_score = ComputeEdgeScore(result.top);
    const double bottom_score = ComputeEdgeScore(result.bottom);
    const double left_score = ComputeEdgeScore(result.left);
    const double right_score = ComputeEdgeScore(result.right);
    const double parallel_penalty =
        NormalizeLineAngleDelta(result.top.line.angle, result.bottom.line.angle) * 80.0 +
        NormalizeLineAngleDelta(result.left.line.angle, result.right.line.angle) * 80.0;
    const double orthogonal_penalty =
        std::fabs(NormalizeLineAngleDelta(result.top.line.angle, result.left.line.angle) - 3.14159265358979323846 / 2.0) * 80.0;
    double seed_penalty = 0.0;
    if (std::fabs(result.seed_rect.Width()) > 1e-6 && std::fabs(result.seed_rect.Height()) > 1e-6)
    {
        const double width_ratio = std::fabs(result.rect.Width()) / std::max(1.0, std::fabs(result.seed_rect.Width()));
        const double height_ratio = std::fabs(result.rect.Height()) / std::max(1.0, std::fabs(result.seed_rect.Height()));
        seed_penalty = (std::fabs(width_ratio - 1.0) + std::fabs(height_ratio - 1.0)) * 20.0;
    }
    const double score = top_score + bottom_score + left_score + right_score - parallel_penalty - orthogonal_penalty - seed_penalty;
    return std::isfinite(score) ? score : -std::numeric_limits<double>::max();
}

gp_Rectangle MergeRectWithSeed(const gp_Rectangle& learned_rect, const gp_Rectangle& seed_rect)
{
    const gp_Rectangle safe_learned = NormalizeRectangle(learned_rect);
    const gp_Rectangle safe_seed = NormalizeRectangle(seed_rect);
    const double min_x = std::min(safe_learned.TopLeft().X(), safe_seed.TopLeft().X());
    const double min_y = std::min(safe_learned.TopLeft().Y(), safe_seed.TopLeft().Y());
    const double max_x = std::max(safe_learned.BottomRight().X(), safe_seed.BottomRight().X());
    const double max_y = std::max(safe_learned.BottomRight().Y(), safe_seed.BottomRight().Y());
    return NormalizeRectangle(gp_Rectangle(gp_Pnt(min_x, min_y, 0), gp_Pnt(max_x, max_y, 0)));
}

gp_Rectangle EstimateSeedRect(Image& image, const gp_Rectangle& working_rect, int threshold)
{
    const int x = std::max(0, static_cast<int>(std::round(working_rect.TopLeft().X())));
    const int y = std::max(0, static_cast<int>(std::round(working_rect.TopLeft().Y())));
    const int w = std::max(0, static_cast<int>(std::round(std::fabs(working_rect.Width()))));
    const int h = std::max(0, static_cast<int>(std::round(std::fabs(working_rect.Height()))));
    if (w <= 0 || h <= 0)
        return working_rect;

    cv::Rect roi(x, y, w, h);
    roi &= cv::Rect(0, 0, image.getWidth(), image.getHeight());
    if (roi.width <= 0 || roi.height <= 0)
        return working_rect;

    cv::Mat source = image.getmat();
    if (source.empty())
        return working_rect;

    cv::Mat roi_image = source(roi);
    cv::Mat gray;
    if (roi_image.channels() == 3)
        cv::cvtColor(roi_image, gray, cv::COLOR_BGR2GRAY);
    else
        gray = roi_image;

    cv::Mat mask;
    cv::threshold(gray, mask, std::max(1, threshold), 255, cv::THRESH_BINARY);
    std::vector<cv::Point> foreground;
    cv::findNonZero(mask, foreground);
    if (foreground.empty())
        return working_rect;

    const cv::Rect bounds = cv::boundingRect(foreground);
    const int pad = std::max(2, std::min(bounds.width, bounds.height) / 8);
    const int seed_x0 = std::max(roi.x, roi.x + bounds.x - pad);
    const int seed_y0 = std::max(roi.y, roi.y + bounds.y - pad);
    const int seed_x1 = std::min(roi.x + roi.width, roi.x + bounds.x + bounds.width + pad);
    const int seed_y1 = std::min(roi.y + roi.height, roi.y + bounds.y + bounds.height + pad);
    return gp_Rectangle(gp_Pnt(seed_x0, seed_y0, 0), gp_Pnt(seed_x1, seed_y1, 0));
}

void ConfigureEdgeFinder(FindLine& finder,
                         int x,
                         int y,
                         int w,
                         int h,
                         int threshold,
                         int compare_gap,
                         int line_gap,
                         int method,
                         int findset,
                         int filter_borw,
                         int filter_min,
                         int filter_max,
                         int gauge)
{
    finder.setrect(x, y, w, h);
    finder.setthre(threshold);
    finder.setcomparegap(compare_gap);
    finder.setlinegap(line_gap);
    finder.setmethod(method);
    finder.setobjfilter(findset);
    finder.setfilter(filter_borw, filter_min, filter_max);
    finder.SetWHgap(std::max(2, gauge / 2), std::max(2, gauge / 2));
    finder.setselectedgenum(0);
    finder.setshow(0);
}

bool MeasureEdgeFinder(FindLine& finder, Image& image, FindRect::EdgeLearnResult& edge)
{
    finder.Measure(image);
    PointsShape points = CollectLinePoints(finder);
    if (points.size() < 2)
    {
        finder.MeasureBalanced(image);
        points = CollectLinePoints(finder);
    }
    const FittedLine fitted = FitLineFromPoints(points);
    edge.line.valid = fitted.valid;
    edge.line.nx = fitted.nx;
    edge.line.ny = fitted.ny;
    edge.line.c = fitted.c;
    edge.line.point_count = fitted.point_count;
    edge.line.fit_error = fitted.fit_error;
    edge.line.angle = fitted.angle;
    edge.stats = finder.lastmeasureprofilestats();
    edge.line.fit_error = edge.stats.fit_error_avg;
    edge.valid = edge.line.valid;
    return edge.valid;
}
}

int FindRect::m_curfindrectnum = 0;

FindRect::FindRect()
    : Shape(),
      g_pbackimage(nullptr),
      g_pbackfindobject(nullptr),
      m_ithreshold(20),
      m_icomparegap(2),
      m_ilinegap(3),
      m_imethod(1),
      m_igauge(0),
      m_ifindset(0),
      m_ifilterborw(21),
      m_ifiltermin(16),
      m_ifiltermax(1000000),
      m_iminarea(16),
      m_imaxarea(1000000),
      m_iminobjw(3),
      m_imaxobjw(1000000),
      m_iminobjh(3),
      m_imaxobjh(1000000),
      m_depsilonratio(0.04),
      m_dminfillratio(0.75),
      m_topfinder(),
      m_bottomfinder(),
      m_leftfinder(),
      m_rightfinder()
{
    string strname = string("frect%1");
    setname(strname.c_str());
    m_curfindrectnum = m_curfindrectnum + 1;

    const int icurmodule = ImageManager::GetCurMode();
    g_pbackimage = ImageManager::GetBackImage(icurmodule);
    g_pbackfindobject = ImageManager::Getbackfindobject(icurmodule);
    m_resultrects.setshow(1);
    m_lastresult = RectLearnResult();
}

FindRect::~FindRect()
{
}

void FindRect::clear()
{
    m_resultrects.clear();
    m_lastresult = RectLearnResult();
    m_last_failure_stage.clear();
    m_debug_seed_valid = false;
    m_debug_top_valid = false;
    m_debug_bottom_valid = false;
    m_debug_left_valid = false;
    m_debug_right_valid = false;
    m_debug_top_points = 0;
    m_debug_bottom_points = 0;
    m_debug_left_points = 0;
    m_debug_right_points = 0;
    m_debug_coarse_score = 0.0;
    m_debug_refine_score = 0.0;
}

void FindRect::setshow(int ishow)
{
    Shape::setshow(ishow);
    m_resultrects.setshow(ishow);
}

void FindRect::setrect(int ix, int iy, int iw, int ih)
{
    Shape::setrect(ix, iy, iw, ih);
}

void FindRect::setrotatedrect(double cx, double cy, double width, double height, double angle_deg)
{
    m_has_rotated_rect = true;
    m_rotated_cx = cx;
    m_rotated_cy = cy;
    m_rotated_width = width;
    m_rotated_height = height;
    m_rotated_angle_deg = angle_deg;
}

void FindRect::drawshape()
{
    Shape::drawshape();
    m_resultrects.drawshape(getpath());
}

void FindRect::shapesetroi(void* pshape)
{
    Shape* pshape0 = static_cast<Shape*>(pshape);
    if (pshape0 == nullptr)
        return;
    FindRect::setrect(static_cast<int>(pshape0->rect().TopLeft().X()),
                      static_cast<int>(pshape0->rect().TopLeft().Y()),
                      static_cast<int>(pshape0->rect().Width()),
                      static_cast<int>(pshape0->rect().Height()));
}

void FindRect::setthre(int ithre)
{
    m_ithreshold = std::max(1, ithre);
}

int FindRect::thre() const
{
    return m_ithreshold;
}

void FindRect::setcomparegap(int igap)
{
    m_icomparegap = ClampPositiveInt(igap, 1);
}

int FindRect::getcomparegap() const
{
    return m_icomparegap;
}

void FindRect::setlinegap(int igap)
{
    m_ilinegap = ClampPositiveInt(igap, 1);
}

int FindRect::linegap() const
{
    return m_ilinegap;
}

void FindRect::setmethod(int imethod)
{
    m_imethod = std::max(0, imethod);
}

int FindRect::method() const
{
    return m_imethod;
}

void FindRect::setgauge(int igauge)
{
    m_igauge = ClampNonNegativeInt(igauge);
}

int FindRect::gauge() const
{
    return m_igauge;
}

void FindRect::setfindsetting(int ifindset)
{
    m_ifindset = std::max(0, ifindset);
}

void FindRect::setfilter(int ifilterborw, int ifiltermin, int ifiltermax)
{
    m_ifilterborw = std::max(0, ifilterborw);
    NormalizeRange(ifiltermin, ifiltermax);
    m_ifiltermin = ifiltermin;
    m_ifiltermax = ifiltermax;
}

void FindRect::setminmaxarea(int imin, int imax)
{
    NormalizeRange(imin, imax);
    m_iminarea = imin;
    m_imaxarea = imax;
}

void FindRect::setminmaxwh(int iminw, int imaxw, int iminh, int imaxh)
{
    NormalizeRange(iminw, imaxw);
    NormalizeRange(iminh, imaxh);
    m_iminobjw = iminw;
    m_imaxobjw = imaxw;
    m_iminobjh = iminh;
    m_imaxobjh = imaxh;
}

void FindRect::setpolygonepsilon(double epsilon_ratio)
{
    m_depsilonratio = NormalizeUnitRatio(epsilon_ratio, 0.04);
}

void FindRect::setfillratio(double min_fill_ratio)
{
    m_dminfillratio = NormalizeUnitRatio(min_fill_ratio, 0.75);
}

void FindRect::measure(void* pimage)
{
    Image* image = static_cast<Image*>(pimage);
    if (image != nullptr)
        Measure(*image);
}

void FindRect::Measure(Image& image)
{
    m_resultrects.clear();
    m_lastresult = RectLearnResult();
    m_last_failure_stage.clear();
    m_debug_seed_valid = false;
    m_debug_top_valid = false;
    m_debug_bottom_valid = false;
    m_debug_left_valid = false;
    m_debug_right_valid = false;
    m_debug_top_points = 0;
    m_debug_bottom_points = 0;
    m_debug_left_points = 0;
    m_debug_right_points = 0;
    m_debug_coarse_score = 0.0;
    m_debug_refine_score = 0.0;

    if (image.getmat().empty())
    {
        m_last_failure_stage = "image_empty";
        return;
    }

    if (!ImageManager::EnsureAlgorithmRuntimeResources(image.getWidth(), image.getHeight()))
    {
        m_last_failure_stage = "algorithm_runtime_resources_unavailable";
        return;
    }

    g_pbackimage = ImageManager::GetBackImage(1);
    g_pbackfindobject = ImageManager::Getbackfindobject(1);

    if (g_pbackimage == nullptr || g_pbackimage == &image ||
        g_pbackimage->getmat().empty())
    {
        m_last_failure_stage = "rect_scan_workspace_unavailable";
        return;
    }

    const gp_Rectangle roi_rect = rect();
    const int roi_x = std::max(0, static_cast<int>(roi_rect.TopLeft().X()));
    const int roi_y = std::max(0, static_cast<int>(roi_rect.TopLeft().Y()));
    const int roi_w = std::max(0, static_cast<int>(std::round(std::fabs(roi_rect.Width()))));
    const int roi_h = std::max(0, static_cast<int>(std::round(std::fabs(roi_rect.Height()))));
    if (roi_w <= 0 || roi_h <= 0)
    {
        m_last_failure_stage = "roi_empty";
        return;
    }
    if (roi_x > image.getWidth() || roi_y > image.getHeight() || roi_w > image.getWidth() - roi_x || roi_h > image.getHeight() - roi_y)
    {
        m_last_failure_stage = "roi_out_of_image";
        return;
    }

    RectLearnResult coarse_result;
    if (!LearnRectOnce(image, roi_rect, coarse_result))
    {
        if (m_last_failure_stage.empty())
            m_last_failure_stage = "coarse_rect";
        return;
    }
    m_debug_coarse_score = coarse_result.score;

    RectLearnResult refined_result;
    if (RefineRectOnce(image, coarse_result.rect, refined_result) && refined_result.score >= coarse_result.score)
    {
        coarse_result = refined_result;
        m_debug_refine_score = refined_result.score;
    }

    const double area = NonNegativeOr(std::fabs(coarse_result.rect.Width() * coarse_result.rect.Height()));
    if (area < m_iminarea || area > m_imaxarea)
    {
        m_last_failure_stage = "area_reject";
        return;
    }

    m_resultrects.addrect(coarse_result.rect);
    m_lastresult = coarse_result;
    m_last_failure_stage.clear();
}

bool FindRect::LearnRectOnce(Image& image, const gp_Rectangle& working_rect, RectLearnResult& result)
{
    result = RectLearnResult();
    const gp_Rectangle seed_rect = NormalizeRectangle(EstimateSeedRect(image, working_rect, m_ithreshold));
    result.seed_rect = seed_rect;
    m_debug_seed_valid = HasPositiveArea(seed_rect);
    const int working_x = std::max(0, static_cast<int>(std::round(seed_rect.TopLeft().X())));
    const int working_y = std::max(0, static_cast<int>(std::round(seed_rect.TopLeft().Y())));
    const int working_w = std::max(0, static_cast<int>(std::round(std::fabs(seed_rect.Width()))));
    const int working_h = std::max(0, static_cast<int>(std::round(std::fabs(seed_rect.Height()))));
    if (working_w <= 0 || working_h <= 0)
    {
        m_last_failure_stage = "seed_empty";
        return false;
    }

    result.gauge = SelectGauge(seed_rect, m_igauge);
    const int horizontal_h = std::max(result.gauge, std::min(working_h, result.gauge * 2));
    const int vertical_w = std::max(result.gauge, std::min(working_w, result.gauge * 2));

    ConfigureEdgeFinder(m_topfinder, working_x, working_y, working_w, horizontal_h,
                        m_ithreshold, m_icomparegap, m_ilinegap, m_imethod,
                        m_ifindset, m_ifilterborw, static_cast<int>(m_ifiltermin), static_cast<int>(m_ifiltermax), result.gauge);
    ConfigureEdgeFinder(m_bottomfinder, working_x, working_y + std::max(0, working_h - horizontal_h), working_w, horizontal_h,
                        m_ithreshold, m_icomparegap, m_ilinegap, m_imethod,
                        m_ifindset, m_ifilterborw, static_cast<int>(m_ifiltermin), static_cast<int>(m_ifiltermax), result.gauge);
    ConfigureEdgeFinder(m_leftfinder, working_x, working_y, vertical_w, working_h,
                        m_ithreshold, m_icomparegap, m_ilinegap, m_imethod,
                        m_ifindset, m_ifilterborw, static_cast<int>(m_ifiltermin), static_cast<int>(m_ifiltermax), result.gauge);
    ConfigureEdgeFinder(m_rightfinder, working_x + std::max(0, working_w - vertical_w), working_y, vertical_w, working_h,
                        m_ithreshold, m_icomparegap, m_ilinegap, m_imethod,
                        m_ifindset, m_ifilterborw, static_cast<int>(m_ifiltermin), static_cast<int>(m_ifiltermax), result.gauge);

    if (!MeasureEdgeFinder(m_topfinder, image, result.top))
    {
        m_debug_top_valid = false;
        m_debug_top_points = result.top.line.point_count;
        m_last_failure_stage = "top_edge";
        return false;
    }
    m_debug_top_valid = result.top.valid;
    m_debug_top_points = result.top.line.point_count;
    if (!MeasureEdgeFinder(m_bottomfinder, image, result.bottom))
    {
        m_debug_bottom_valid = false;
        m_debug_bottom_points = result.bottom.line.point_count;
        m_last_failure_stage = "bottom_edge";
        return false;
    }
    m_debug_bottom_valid = result.bottom.valid;
    m_debug_bottom_points = result.bottom.line.point_count;
    if (!MeasureEdgeFinder(m_leftfinder, image, result.left))
    {
        m_debug_left_valid = false;
        m_debug_left_points = result.left.line.point_count;
        m_last_failure_stage = "left_edge";
        return false;
    }
    m_debug_left_valid = result.left.valid;
    m_debug_left_points = result.left.line.point_count;
    if (!MeasureEdgeFinder(m_rightfinder, image, result.right))
    {
        m_debug_right_valid = false;
        m_debug_right_points = result.right.line.point_count;
        m_last_failure_stage = "right_edge";
        return false;
    }
    m_debug_right_valid = result.right.valid;
    m_debug_right_points = result.right.line.point_count;

    gp_Pnt top_left;
    gp_Pnt top_right;
    gp_Pnt bottom_left;
    gp_Pnt bottom_right;
    if (!IntersectLines(ToFittedLine(result.top.line), ToFittedLine(result.left.line), top_left))
    {
        m_last_failure_stage = "intersect_top_left";
        return false;
    }
    if (!IntersectLines(ToFittedLine(result.top.line), ToFittedLine(result.right.line), top_right))
    {
        m_last_failure_stage = "intersect_top_right";
        return false;
    }
    if (!IntersectLines(ToFittedLine(result.bottom.line), ToFittedLine(result.left.line), bottom_left))
    {
        m_last_failure_stage = "intersect_bottom_left";
        return false;
    }
    if (!IntersectLines(ToFittedLine(result.bottom.line), ToFittedLine(result.right.line), bottom_right))
    {
        m_last_failure_stage = "intersect_bottom_right";
        return false;
    }

    const double min_x = std::min(std::min(top_left.X(), top_right.X()), std::min(bottom_left.X(), bottom_right.X()));
    const double max_x = std::max(std::max(top_left.X(), top_right.X()), std::max(bottom_left.X(), bottom_right.X()));
    const double min_y = std::min(std::min(top_left.Y(), top_right.Y()), std::min(bottom_left.Y(), bottom_right.Y()));
    const double max_y = std::max(std::max(top_left.Y(), top_right.Y()), std::max(bottom_left.Y(), bottom_right.Y()));
    if ((max_x - min_x) < m_iminobjw || (max_x - min_x) > m_imaxobjw)
    {
        m_last_failure_stage = "width_reject";
        return false;
    }
    if ((max_y - min_y) < m_iminobjh || (max_y - min_y) > m_imaxobjh)
    {
        m_last_failure_stage = "height_reject";
        return false;
    }

    result.valid = true;
    const gp_Rectangle learned_rect(gp_Pnt(min_x, min_y, 0), gp_Pnt(max_x, max_y, 0));
    result.rect = MergeRectWithSeed(learned_rect, seed_rect);
    result.score = ComputeRectScore(result);
    if (!std::isfinite(result.score))
    {
        m_last_failure_stage = "score_invalid";
        return false;
    }
    return true;
}

bool FindRect::RefineRectOnce(Image& image, const gp_Rectangle& coarse_rect, RectLearnResult& result)
{
    const int pad = std::max(2, SelectGauge(coarse_rect, m_igauge) / 2);
    const double min_x = coarse_rect.TopLeft().X() - pad;
    const double min_y = coarse_rect.TopLeft().Y() - pad;
    const double max_x = coarse_rect.BottomRight().X() + pad;
    const double max_y = coarse_rect.BottomRight().Y() + pad;
    gp_Rectangle expanded = NormalizeRectangle(gp_Rectangle(gp_Pnt(min_x, min_y, 0), gp_Pnt(max_x, max_y, 0)));
    return LearnRectOnce(image, expanded, result);
}

RectsShape& FindRect::getresultrects()
{
    return m_resultrects;
}

gp_Rectangle FindRect::getresultrect(int inum) const
{
    const RectsShape& rects = m_resultrects;
    if (inum < 0 || inum >= const_cast<RectsShape&>(rects).size())
        return gp_Rectangle(gp_Pnt(0, 0, 0), gp_Pnt(0, 0, 0));
    return const_cast<RectsShape&>(rects).getrect(inum);
}

int FindRect::getresultobjsnum() const
{
    return m_resultrects.size();
}

cxgeom::CxSurfaceElement FindRect::makeresultelement(int inum, int entity_id) const
{
    const RectsShape& rects = m_resultrects;
    if (inum < 0 || inum >= const_cast<RectsShape&>(rects).size())
    {
        return cxgeom::CxGeomElementBody::MakeSurfaceElement(
            cxcore::MakeMetadataShape(entity_id, "findrect", cxgeom::CxShapeKind::Face),
            0.0,
            true);
    }

    const gp_Rectangle result_rect = const_cast<RectsShape&>(rects).getrect(inum);
    cxcore::OutputRect rect_output;
    rect_output.x = result_rect.TopLeft().X();
    rect_output.y = result_rect.TopLeft().Y();
    rect_output.width = result_rect.Width();
    rect_output.height = result_rect.Height();

    const double confidence = m_lastresult.valid
        ? std::max(0.0, std::min(1.0, m_lastresult.score / 100.0))
        : 1.0;
    return cxcore::MakeSurfaceElementFromRect(rect_output, entity_id, "findrect", confidence);
}

void FindRect::PublishDisplayShapes(
    ICxShapeSink& sink,
    const std::string& owner_ref) const
{
    if (m_has_rotated_rect)
    {
        const double angle_rad = m_rotated_angle_deg * M_PI / 180.0;
        const double hw = m_rotated_width / 2.0;
        const double hh = m_rotated_height / 2.0;

        const double cos_a = cos(angle_rad);
        const double sin_a = sin(angle_rad);

        double x0 = m_rotated_cx - hw * cos_a + hh * sin_a;
        double y0 = m_rotated_cy - hw * sin_a - hh * cos_a;
        double x1 = m_rotated_cx + hw * cos_a + hh * sin_a;
        double y1 = m_rotated_cy + hw * sin_a - hh * cos_a;
        double x2 = m_rotated_cx + hw * cos_a - hh * sin_a;
        double y2 = m_rotated_cy + hw * sin_a + hh * cos_a;
        double x3 = m_rotated_cx - hw * cos_a - hh * sin_a;
        double y3 = m_rotated_cy - hw * sin_a + hh * cos_a;

        auto roiShape = std::make_unique<PolylineShape>();
        roiShape->addPoint(x0, y0);
        roiShape->addPoint(x1, y1);
        roiShape->addPoint(x2, y2);
        roiShape->addPoint(x3, y3);
        roiShape->close(true);

        sink.UpsertShape(
            owner_ref + ".roi_rotated_rect",
            "FindRect",
            owner_ref,
            "setrotatedrect",
            "roi",
            true,
            false,
            std::move(roiShape));
    }
    else
    {
        const gp_Rectangle roi = rect();
        const bool hasValidRoi =
            std::abs(roi.Width()) > 1.0 &&
            std::abs(roi.Height()) > 1.0;

        if (hasValidRoi)
        {
            const double x0 = roi.TopLeft().X();
            const double y0 = roi.TopLeft().Y();
            const double x1 = x0 + roi.Width();
            const double y1 = y0 + roi.Height();

            auto roiShape = std::make_unique<RectShape>(x0, y0, x1, y1);
            sink.UpsertShape(
                owner_ref + ".roi_rect",
                "FindRect",
                owner_ref,
                "setrect",
                "roi",
                true,
                false,
                std::move(roiShape));
        }
    }

    for (int i = 0; i < getresultobjsnum(); ++i)
    {
        const gp_Rectangle result = getresultrect(i);
        if (std::abs(result.Width()) <= 1.0 ||
            std::abs(result.Height()) <= 1.0)
        {
            continue;
        }

        const double rx0 = result.TopLeft().X();
        const double ry0 = result.TopLeft().Y();
        const double rx1 = rx0 + result.Width();
        const double ry1 = ry0 + result.Height();

        auto resultShape = std::make_unique<RectShape>(rx0, ry0, rx1, ry1);
        sink.UpsertShape(
            owner_ref + ".result_rect." + std::to_string(i),
            "FindRect",
            owner_ref,
            "",
            "result",
            false,
            true,
            std::move(resultShape));
    }
}
