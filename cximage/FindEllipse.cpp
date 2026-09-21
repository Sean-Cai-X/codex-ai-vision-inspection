#include "pch.h"

#include "CxCrashLogHandler.h"
#include "CxUnifiedLog.h"
#include "EllipseShape.h"
#include "FindEllipse.h"
#include "ImageAnnotationLayer.h"
#include "findobject.h"
#include "imagemanager.h"
#include "occtinclude.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <opencv2/core/core.hpp>
#include <opencv2/core/version.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/video/tracking.hpp>
#include <sstream>
#include <vector>

namespace {
void LogFindEllipseMeasureProbe(const char *phase, const char *status,
                                const std::string &message) {
  CXLOG_INFO("FindEllipse", phase, status, message);
  CxUnifiedLog::Instance().Flush();
}

double EllipseNorm(double x, double y, double cx, double cy, double rx,
                   double ry) {
  if (rx <= 1.0 || ry <= 1.0)
    return 999.0;

  const double nx = (x - cx) / rx;
  const double ny = (y - cy) / ry;

  return std::sqrt(nx * nx + ny * ny);
}

int ClampSizeToInt(std::size_t value) {
  const std::size_t max_value =
      static_cast<std::size_t>(std::numeric_limits<int>::max());
  return value > max_value ? std::numeric_limits<int>::max()
                           : static_cast<int>(value);
}

int CopyEllipseSeamTailRows(Image *process_image, int process_width,
                            int line_count, int tail_rows) {
  if (process_image == nullptr || process_width <= 0 || line_count <= 0 ||
      tail_rows <= 0)
    return 0;

  cv::Mat &mat = process_image->getmat();
  if (mat.empty())
    return 0;

  const int copy_width = std::min(process_width, mat.cols);
  const int wrap_rows = std::min(std::max(0, tail_rows), line_count);
  if (copy_width <= 0 || wrap_rows <= 0)
    return 0;

  int copied = 0;
  for (int pad = 0; pad < wrap_rows; ++pad) {
    const int src_row = pad;
    const int dst_row = line_count + pad;
    if (src_row < 0 || src_row >= mat.rows || dst_row < 0 ||
        dst_row >= mat.rows)
      continue;

    mat(cv::Rect(0, src_row, copy_width, 1))
        .copyTo(mat(cv::Rect(0, dst_row, copy_width, 1)));
    ++copied;
  }

  return copied;
}

int MergeEllipseSeamTailRows(Image *process_image, int process_width,
                             int line_count, int tail_rows) {
  if (process_image == nullptr || process_width <= 0 || line_count <= 0 ||
      tail_rows <= 0)
    return 0;

  cv::Mat &mat = process_image->getmat();
  if (mat.empty())
    return 0;

  const int copy_width = std::min(process_width, mat.cols);
  const int wrap_rows = std::min(std::max(0, tail_rows), line_count);
  if (copy_width <= 0 || wrap_rows <= 0)
    return 0;

  int merged = 0;
  const int channels = mat.channels();
  for (int pad = 0; pad < wrap_rows; ++pad) {
    const int src_row = line_count + pad;
    const int dst_row = pad;
    if (src_row < 0 || src_row >= mat.rows || dst_row < 0 ||
        dst_row >= mat.rows)
      continue;

    for (int x = 0; x < copy_width; ++x) {
      uchar *dst = mat.ptr<uchar>(dst_row) + x * channels;
      const uchar *src = mat.ptr<uchar>(src_row) + x * channels;
      for (int channel = 0; channel < channels; ++channel) {
        if (src[channel] > dst[channel]) {
          dst[channel] = src[channel];
          ++merged;
        }
      }
    }
  }

  return merged;
}

int CountEllipseForegroundPixels(const cv::Mat &mat, int process_width,
                                 int line_count) {
  if (mat.empty() || process_width <= 0 || line_count <= 0)
    return 0;

  const int rows = std::min(line_count, mat.rows);
  const int cols = std::min(process_width, mat.cols);
  const int channels = mat.channels();
  int count = 0;

  for (int y = 0; y < rows; ++y) {
    for (int x = 0; x < cols; ++x) {
      const uchar *px = mat.ptr<uchar>(y) + x * channels;
      for (int channel = 0; channel < channels; ++channel) {
        if (px[channel] > 0) {
          ++count;
          break;
        }
      }
    }
  }

  return count;
}

int ComputeEllipseEndpointGuard(int line_length, int linegap) {
  if (line_length <= 12)
    return 0;
  const int linegap_guard = std::max(6, std::max(1, linegap) * 2);
  const int length_guard = std::max(1, line_length / 12);
  return std::min(linegap_guard, length_guard);
}

double ComputeEllipseInnerCandidateNormGuard(int inner_scale_percent) {
  if (inner_scale_percent <= 0)
    return 0.0;

  const double inner_norm =
      std::max(0.0, std::min(0.95, static_cast<double>(inner_scale_percent) *
                                       0.01));
  return std::min(0.95, inner_norm + 0.18);
}

void ApplyEllipseObjectPrefilter(FindObject *find_object, Image *process_image,
                                 int process_width, int line_count,
                                 int filter_mode, int filter_min,
                                 int filter_max) {
  if (find_object == nullptr || process_image == nullptr ||
      process_width <= 0 || line_count <= 0)
    return;

  find_object->setrect(0, 0, process_width, line_count);
  find_object->setbrow(filter_mode);
  find_object->setminmaxarea(filter_min, filter_max);
  if (filter_mode == 21 || filter_mode == 22)
    find_object->MeasureConnectedComponents(*process_image);
  else
    find_object->Measure(*process_image);
}

int RoundToInt(double value) {
  if (!std::isfinite(value))
    return 0;
  const double clamped = std::min(
      std::max(value, static_cast<double>(std::numeric_limits<int>::min())),
      static_cast<double>(std::numeric_limits<int>::max()));
  return static_cast<int>(std::lround(clamped));
}

template <typename EnumT>
EnumT ClampEnumInt(int value, int min_value, int max_value, EnumT fallback) {
  if (value < min_value || value > max_value)
    return fallback;
  return static_cast<EnumT>(value);
}

int ClampPermille(int value) { return std::max(0, std::min(1000, value)); }

gp_Pnt InterpolateEllipseLinePoint(LineShape &line, double sample_position,
                                   int line_length) {
  if (!std::isfinite(sample_position) || line_length <= 1)
    return line.getlinepoint(0);

  const double clamped =
      std::max(0.0, std::min(sample_position,
                             static_cast<double>(line_length - 1)));
  const int lo = static_cast<int>(std::floor(clamped));
  const int hi = std::min(line_length - 1, lo + 1);
  const double t = clamped - static_cast<double>(lo);
  const gp_Pnt p0 = line.getlinepoint(lo);
  const gp_Pnt p1 = line.getlinepoint(hi);
  return gp_Pnt(p0.X() + (p1.X() - p0.X()) * t,
                p0.Y() + (p1.Y() - p0.Y()) * t,
                p0.Z() + (p1.Z() - p0.Z()) * t);
}

std::vector<float> BuildEllipseGrayProfile(const cv::Mat &gray,
                                           LineShape &scan_line,
                                           int line_length) {
  std::vector<float> profile;
  if (gray.empty() || line_length <= 0)
    return profile;

  profile.reserve(static_cast<std::size_t>(line_length));
  for (int i = 0; i < line_length; ++i) {
    const gp_Pnt point = scan_line.getlinepoint(i);
    const int x = RoundToInt(point.X());
    const int y = RoundToInt(point.Y());
    if (x < 0 || x >= gray.cols || y < 0 || y >= gray.rows) {
      profile.push_back(0.0f);
      continue;
    }
    profile.push_back(static_cast<float>(gray.at<uchar>(y, x)));
  }
  return profile;
}

int ClampLongLongToInt(long long value) {
  if (value < static_cast<long long>(std::numeric_limits<int>::min()))
    return std::numeric_limits<int>::min();
  if (value > static_cast<long long>(std::numeric_limits<int>::max()))
    return std::numeric_limits<int>::max();
  return static_cast<int>(value);
}

int ComputeEllipseLineStep(int gap_degrees, int point_count) {
  if (gap_degrees <= 0 || point_count <= 0)
    return 1;

  const double angle_rate = gap_degrees / 360.0;
  const int step = RoundToInt(angle_rate * static_cast<double>(point_count));
  return step > 0 ? step : 1;
}

bool HasSufficientEllipseAngularCoverage(const std::vector<cv::Point2f> &points,
                                         double center_x, double center_y,
                                         double radius_x, double radius_y) {
  if (points.size() < 5 || radius_x <= 1.0 || radius_y <= 1.0)
    return false;

  constexpr int kBins = 16;
  std::array<bool, kBins> occupied{};
  int occupied_count = 0;

  for (const cv::Point2f &point : points) {
    const double nx = (static_cast<double>(point.x) - center_x) / radius_x;
    const double ny = (static_cast<double>(point.y) - center_y) / radius_y;
    double angle = std::atan2(ny, nx);
    if (angle < 0.0)
      angle += 2.0 * CV_PI;

    int bin = static_cast<int>(std::floor(angle / (2.0 * CV_PI) * kBins));
    if (bin < 0)
      bin = 0;
    if (bin >= kBins)
      bin = kBins - 1;

    if (!occupied[bin]) {
      occupied[bin] = true;
      ++occupied_count;
    }
  }

  return occupied_count >= 4;
}

int FilterEllipsePointsByGaugeBoundaryDistance(PointsShape &points,
                                               double center_x, double center_y,
                                               double radius_x, double radius_y,
                                               double max_distance) {
  if (points.size() <= 0 || radius_x <= 1.0 || radius_y <= 1.0 ||
      max_distance <= 0.0) {
    return 0;
  }

  const int input_count = ClampSizeToInt(points.size());
  const double scale =
      std::max(1.0, (std::abs(radius_x) + std::abs(radius_y)) * 0.5);
  PointsShape filtered;
  int kept_count = 0;
  for (int i = 0; i < input_count; ++i) {
    const double x = points.getx(i);
    const double y = points.gety(i);
    const double norm =
        EllipseNorm(x, y, center_x, center_y, radius_x, radius_y);
    const double distance = std::abs(norm - 1.0) * scale;
    if (std::isfinite(distance) && distance <= max_distance) {
      filtered.addpoint(RoundToInt(x), RoundToInt(y));
      ++kept_count;
    }
  }

  if (kept_count <= 0) {
    points.clear();
    return input_count;
  }

  points.clear();
  points.addpoints(filtered);
  return std::max(0, input_count - kept_count);
}

double EllipseDotProduct(const std::vector<double> &a,
                         const std::vector<double> &b) {
  if (a.size() != b.size())
    return 0.0;
  double sum = 0.0;
  for (std::size_t i = 0; i < a.size(); ++i)
    sum += a[i] * b[i];
  return sum;
}

double EllipseMean(const std::vector<double> &v) {
  if (v.empty())
    return 0.0;
  double sum = 0.0;
  for (double x : v)
    sum += x;
  return sum / static_cast<double>(v.size());
}

double EllipseCalculateNCC(const std::vector<double> &a,
                           const std::vector<double> &b) {
  if (a.empty() || b.empty())
    return 0.0;
  std::size_t n = std::min(a.size(), b.size());
  if (n < 2)
    return 0.0;

  std::vector<double> a_aligned(a.begin(), a.begin() + n);
  std::vector<double> b_aligned(b.begin(), b.begin() + n);

  double mean_a = EllipseMean(a_aligned);
  double mean_b = EllipseMean(b_aligned);

  std::vector<double> a_centered(n), b_centered(n);
  for (std::size_t i = 0; i < n; ++i) {
    a_centered[i] = a_aligned[i] - mean_a;
    b_centered[i] = b_aligned[i] - mean_b;
  }

  double numerator = EllipseDotProduct(a_centered, b_centered);
  double denom_a = std::sqrt(EllipseDotProduct(a_centered, a_centered));
  double denom_b = std::sqrt(EllipseDotProduct(b_centered, b_centered));

  if (denom_a < 1e-8 || denom_b < 1e-8)
    return 0.0;
  double ncc = numerator / (denom_a * denom_b);
  return std::max(-1.0, std::min(1.0, ncc));
}

std::vector<double> ExtractEllipseProfileAt(const cv::Mat &gray, double cx,
                                            double cy, double angle_deg,
                                            double radius_x, double radius_y,
                                            int half_width) {
  std::vector<double> profile;
  double angle_rad = angle_deg * 3.14159265358979323846 / 180.0;
  double tangent_x = -radius_x * std::sin(angle_rad);
  double tangent_y = radius_y * std::cos(angle_rad);
  double tangent_len = std::sqrt(tangent_x * tangent_x + tangent_y * tangent_y);
  if (tangent_len < 1e-8) {
    profile.push_back(0.0);
    return profile;
  }
  tangent_x /= tangent_len;
  tangent_y /= tangent_len;

  int w = gray.cols;
  int h = gray.rows;

  for (int offset = -half_width; offset <= half_width; ++offset) {
    double px = cx + tangent_x * offset;
    double py = cy + tangent_y * offset;

    int ix = static_cast<int>(std::lround(px));
    int iy = static_cast<int>(std::lround(py));

    if (ix >= 0 && ix < w && iy >= 0 && iy < h) {
      profile.push_back(static_cast<double>(gray.at<uchar>(iy, ix)));
    } else {
      profile.push_back(0.0);
    }
  }
  return profile;
}
} // namespace

namespace {

FindEllipseMeasureGeometryDebug::ScanDiagnostic&
EnsureFindEllipseScanDiagnostic(
    std::vector<FindEllipseMeasureGeometryDebug::ScanDiagnostic>& diagnostics,
    int scan_index)
{
  auto existing = std::find_if(
      diagnostics.begin(), diagnostics.end(),
      [&](const FindEllipseMeasureGeometryDebug::ScanDiagnostic& diag) {
        return diag.scan_index == scan_index;
      });
  if (existing != diagnostics.end())
    return *existing;

  FindEllipseMeasureGeometryDebug::ScanDiagnostic diag;
  diag.scan_index = scan_index;
  diagnostics.push_back(diag);
  return diagnostics.back();
}

void RecordFindEllipseAcceptedDiagnosticPoint(
    std::vector<FindEllipseMeasureGeometryDebug::ScanDiagnostic>& diagnostics,
    int scan_index,
    int candidate_count,
    int accepted_position,
    int min_edge_run_width_px,
    const gp_Pnt& point)
{
  auto& diag = EnsureFindEllipseScanDiagnostic(diagnostics, scan_index);
  diag.candidate_count = std::max(diag.candidate_count, candidate_count);
  diag.accepted = true;
  diag.accepted_x = point.X();
  diag.accepted_y = point.Y();
  diag.accepted_points_xy.push_back(point.X());
  diag.accepted_points_xy.push_back(point.Y());
  diag.accepted_position = accepted_position;
  diag.min_edge_run_width_px = min_edge_run_width_px;
  diag.reject_reason.clear();
}

} // namespace

int FindEllipse::m_curfindlinenum = 0;
FindEllipse::FindEllipse()
    : Shape(), m_igap(6), m_iSelectPointGap(3), m_iMethod(1), m_iThreshold(8),
      m_igamarate(0), m_dsamplerate(0.004), m_ifindset(1), m_ifilterborw(21),
      m_ifiltermin(50), m_ifiltermax(100000), m_iselectedgenum(0),
      m_ineedfixs(2), m_icomparegap(2), m_ishowlines(1),
      m_measurepointsboundingRect(gp_Pnt(0, 0, 0), 0, 0) {
  string strname = string("fellipse%1");
  setname(strname.c_str());
  m_curfindlinenum = m_curfindlinenum + 1;

  int icurmodule = ImageManager::GetCurMode();
  g_pbackimage = ImageManager::GetBackImage(icurmodule);
  g_pbackfindobject = ImageManager::Getbackfindobject(icurmodule);
}
FindEllipse::~FindEllipse() {}
void FindEllipse::setcomparegap(int igap) { m_icomparegap = igap; }

void FindEllipse::setshow(int ishow) {
  if (ishow == 0) {
    for (std::size_t i = 0; i < m_lines.size(); ++i)
      m_lines[i].setshow(false);
    Shape::setshow(ishow);
    return;
  }
  if (ishow & 0x02) {
    m_measurepoints.setshow(2);
  }
  if (1 == ishow) {
    m_measurepoints.setshow(1);
  }
  if (0x04 == ishow) {
    for (std::size_t i = 0; i < m_lines.size(); ++i)
      m_lines[i].setshow(true);
  } else {
    for (std::size_t i = 0; i < m_lines.size(); ++i)
      m_lines[i].setshow(false);
  }
  Shape::setshow(ishow);
}
void FindEllipse::setselectedgenum(int iedgenum) {
  m_iselectedgenum = iedgenum;
}
void FindEllipse::setpointconsistency(int enabled, int range) {
  m_point_consistency_enabled = enabled != 0 ? 1 : 0;
  m_point_consistency_range = static_cast<double>(std::max(0, range));
  m_point_consistency_input_points = 0;
  m_point_consistency_output_points = 0;
  m_point_consistency_removed_points = 0;
}
void FindEllipse::clear() {
  m_lines.clear();
  m_measurepoints.clear();
  m_measurepoints_.clear();
  m_scan_diagnostics.clear();
  m_has_display_roi = false;
  m_roi_x0 = 0;
  m_roi_y0 = 0;
  m_roi_x1 = 0;
  m_roi_y1 = 0;
  m_has_fit_result = false;
  m_fit_center_x = 0.0;
  m_fit_center_y = 0.0;
  m_fit_radius_x = 0.0;
  m_fit_radius_y = 0.0;
  m_fit_angle_deg = 0.0;
  m_fit_avgdist = 0.0;
  m_scan_candidate_lines = 0;
  m_scan_total_candidates = 0;
  m_rejected_min_edge_run_width_count = 0;
  m_scan_accepted_points_before_gate = 0;
  m_accepted_boundary_ratio_sum = 0.0;
  m_accepted_boundary_ratio_min = 999.0;
  m_accepted_boundary_ratio_max = -999.0;
  m_candidate_policy.clear();
}
void FindEllipse::Setgap(int gap) {
  m_igap = gap;
  for (std::size_t i = 0; i < m_lines.size(); ++i) {
    m_lines[i].clear();
  }
  m_lines.clear();

  const int isize = ClampSizeToInt(getpath().ElementCount());
  if (m_igap > 0 && isize > 0 && m_has_display_roi) {
    LineShape aline1;
    const int igapadd = ComputeEllipseLineStep(m_igap, isize);
    const double cx = static_cast<double>(m_roi_x0 + m_roi_x1) * 0.5;
    const double cy = static_cast<double>(m_roi_y0 + m_roi_y1) * 0.5;
    const double width0 = std::abs(static_cast<double>(m_roi_x1 - m_roi_x0));
    const double height0 = std::abs(static_cast<double>(m_roi_y1 - m_roi_y0));
    const double rx = width0 * 0.5;
    const double ry = height0 * 0.5;
    const double outer_ratio = 1.05;

    for (int i = 0; i < isize; i += igapadd) {
      gp_Pnt apoint = getpath().ElementAt(i);
      const double dx = apoint.X() - cx;
      const double dy = apoint.Y() - cy;
      const double dist = std::sqrt(dx * dx + dy * dy);

      if (dist > 1.0) {
        const double nx = dx / dist;
        const double ny = dy / dist;
        const double boundary_dist =
            rx * ry / std::sqrt(ry * ry * nx * nx + rx * rx * ny * ny);
        const double clamped_dist = std::min(boundary_dist * outer_ratio, dist);
        const double inner_ratio =
            std::max(0.0, std::min(0.99,
                                   static_cast<double>(m_inner_scale_percent) *
                                       0.01));
        const double inner_dist =
            std::min(std::max(0.0, boundary_dist * inner_ratio),
                     std::max(0.0, clamped_dist - 1.0));
        const int start_x = RoundToInt(cx + nx * inner_dist);
        const int start_y = RoundToInt(cy + ny * inner_dist);
        const int end_x = RoundToInt(cx + nx * clamped_dist);
        const int end_y = RoundToInt(cy + ny * clamped_dist);
        m_lines.push_back(aline1);
        m_lines[m_lines.size() - 1].setline(start_x, start_y, end_x, end_y);
      } else {
        m_lines.push_back(aline1);
        m_lines[m_lines.size() - 1].setline(RoundToInt(cx), RoundToInt(cy),
                                            RoundToInt(apoint.X()),
                                            RoundToInt(apoint.Y()));
      }
      m_lines[m_lines.size() - 1].setPercent(m_dsamplerate);
    }

    m_scan_lines_outside_roi_count = 0;
    m_scan_lines_cross_outside_ellipse_count = 0;
    m_scan_endpoint_norm_min = 999.0;
    m_scan_endpoint_norm_max = -999.0;

    for (auto &line : m_lines) {
      const int line_size = line.getlinesize();
      if (line_size < 2)
        continue;

      const double n_start = EllipseNorm(cx, cy, cx, cy, rx, ry);
      gp_Pnt p_end = line.getlinepoint(line_size - 1);
      const double n_end = EllipseNorm(p_end.X(), p_end.Y(), cx, cy, rx, ry);

      m_scan_endpoint_norm_min =
          std::min(m_scan_endpoint_norm_min, std::min(n_start, n_end));
      m_scan_endpoint_norm_max =
          std::max(m_scan_endpoint_norm_max, std::max(n_start, n_end));

      if (n_end > 1.05)
        m_scan_lines_cross_outside_ellipse_count++;
    }
  }
}
void FindEllipse::setellipse(int icentx, int icenty, int ipax, int ipay) {
  Shape::setellipse(icentx, icenty, ipax, ipay);
  m_has_fit_result = false;
  m_fit_avgdist = 0.0;

  m_roi_x0 = std::min(icentx, ipax);
  m_roi_y0 = std::min(icenty, ipay);
  m_roi_x1 = std::max(icentx, ipax);
  m_roi_y1 = std::max(icenty, ipay);
  m_has_display_roi = m_roi_x1 > m_roi_x0 && m_roi_y1 > m_roi_y0;

  for (std::size_t i = 0; i < m_lines.size(); ++i) {
    m_lines[i].clear();
  }
  m_lines.clear();
  const int isize = ClampSizeToInt(getpath().ElementCount());
  if (m_igap > 0 && isize > 0) {
    LineShape aline1;
    const int igapadd = ComputeEllipseLineStep(m_igap, isize);
    int icx0 = (icentx + ipax) / 2;
    int icy0 = (icenty + ipay) / 2;
    const double width0 = std::abs(static_cast<double>(ipax - icentx));
    const double height0 = std::abs(static_cast<double>(ipay - icenty));
    const double rx = width0 * 0.5;
    const double ry = height0 * 0.5;
    const double outer_ratio = 1.05;

    for (int i = 0; i < isize; i += igapadd) {
      gp_Pnt apoint = getpath().ElementAt(i);
      const double dx = apoint.X() - icx0;
      const double dy = apoint.Y() - icy0;
      const double dist = std::sqrt(dx * dx + dy * dy);

      if (dist > 1.0) {
        const double nx = dx / dist;
        const double ny = dy / dist;
        const double boundary_dist =
            rx * ry / std::sqrt(ry * ry * nx * nx + rx * rx * ny * ny);
        const double clamped_dist = std::min(boundary_dist * outer_ratio, dist);
        const double inner_ratio =
            std::max(0.0, std::min(0.99,
                                   static_cast<double>(m_inner_scale_percent) *
                                       0.01));
        const double inner_dist =
            std::min(std::max(0.0, boundary_dist * inner_ratio),
                     std::max(0.0, clamped_dist - 1.0));
        const int start_x = RoundToInt(icx0 + nx * inner_dist);
        const int start_y = RoundToInt(icy0 + ny * inner_dist);
        const int end_x = RoundToInt(icx0 + nx * clamped_dist);
        const int end_y = RoundToInt(icy0 + ny * clamped_dist);
        m_lines.push_back(aline1);
        m_lines[m_lines.size() - 1].setline(start_x, start_y, end_x, end_y);
      } else {
        m_lines.push_back(aline1);
        m_lines[m_lines.size() - 1].setline(icx0, icy0, RoundToInt(apoint.X()),
                                            RoundToInt(apoint.Y()));
      }
      m_lines[m_lines.size() - 1].setPercent(m_dsamplerate);
    }
  }

  m_scan_geometry_policy = "ellipse_boundary_clamped_1_05";
  m_scan_lines_outside_roi_count = 0;
  m_scan_lines_cross_outside_ellipse_count = 0;
  m_scan_endpoint_norm_min = 999.0;
  m_scan_endpoint_norm_max = -999.0;

  if (m_has_display_roi) {
    const double cx = static_cast<double>(m_roi_x0 + m_roi_x1) * 0.5;
    const double cy = static_cast<double>(m_roi_y0 + m_roi_y1) * 0.5;
    const double width0 = std::abs(static_cast<double>(m_roi_x1 - m_roi_x0));
    const double height0 = std::abs(static_cast<double>(m_roi_y1 - m_roi_y0));
    const double rx = width0 * 0.5;
    const double ry = height0 * 0.5;

    for (auto &line : m_lines) {
      const int line_size = line.getlinesize();
      if (line_size < 2)
        continue;

      gp_Pnt p_start = line.getlinepoint(0);
      gp_Pnt p_end = line.getlinepoint(line_size - 1);
      const double n_start =
          EllipseNorm(p_start.X(), p_start.Y(), cx, cy, rx, ry);
      const double n_end = EllipseNorm(p_end.X(), p_end.Y(), cx, cy, rx, ry);

      m_scan_endpoint_norm_min =
          std::min(m_scan_endpoint_norm_min, std::min(n_start, n_end));
      m_scan_endpoint_norm_max =
          std::max(m_scan_endpoint_norm_max, std::max(n_start, n_end));

      if (n_end > 1.05)
        m_scan_lines_cross_outside_ellipse_count++;
    }
  }
}

void FindEllipse::setbboxx0(int ix0) {
  m_pending_bbox_x0 = ix0;
  m_has_pending_bbox = true;
}

void FindEllipse::setbboxy0(int iy0) {
  m_pending_bbox_y0 = iy0;
  m_has_pending_bbox = true;
}

void FindEllipse::setbboxx1(int ix1) {
  m_pending_bbox_x1 = ix1;
  m_has_pending_bbox = true;
}

void FindEllipse::setbboxy1(int iy1) {
  m_pending_bbox_y1 = iy1;
  m_has_pending_bbox = true;
}

void FindEllipse::buildbbox() {
  const int x0 = m_pending_bbox_x0;
  const int y0 = m_pending_bbox_y0;
  const int x1 = m_pending_bbox_x1;
  const int y1 = m_pending_bbox_y1;

  {
    std::ostringstream oss;
    oss << "requested_bbox=(" << x0 << "," << y0 << "," << x1 << "," << y1
        << ")";
    LogFindEllipseMeasureProbe("setellipse_bbox", "requested", oss.str());
  }

  setellipse(x0, y0, x1, y1);

  {
    std::ostringstream oss;
    oss << "applied_roi=(" << m_roi_x0 << "," << m_roi_y0 << ")-("
        << m_roi_x1 << "," << m_roi_y1 << ")"
        << " inner_scale_percent=" << m_inner_scale_percent;
    LogFindEllipseMeasureProbe("setellipse_bbox", "applied", oss.str());
  }
}

void FindEllipse::setinnerpercent(int percent) {
  m_inner_scale_percent = std::max(0, std::min(99, percent));
  if (m_has_display_roi) {
    setellipse(m_roi_x0, m_roi_y0, m_roi_x1, m_roi_y1);
  }
  std::ostringstream oss;
  oss << "inner_scale_percent=" << m_inner_scale_percent
      << " scan_lines=" << m_lines.size()
      << " rebuild=" << (m_has_display_roi ? 1 : 0);
  LogFindEllipseMeasureProbe("setellipse_annulus", "applied", oss.str());
}

void FindEllipse::setellipse2(int icentx, int icenty, int ipax, int ipay,
                              int idis) {
  Shape::setellipse2(icentx, icenty, ipax, ipay, idis);
  m_has_fit_result = false;
  m_fit_avgdist = 0.0;

  m_roi_x0 = std::min(icentx, ipax);
  m_roi_y0 = std::min(icenty, ipay);
  m_roi_x1 = std::max(icentx, ipax);
  m_roi_y1 = std::max(icenty, ipay);
  m_has_display_roi = m_roi_x1 > m_roi_x0 && m_roi_y1 > m_roi_y0;
  for (std::size_t i = 0; i < m_lines.size(); ++i) {
    m_lines[i].clear();
  }
  m_lines.clear();
  const int isize = ClampSizeToInt(getpath().ElementCount());
  if (m_igap > 0 && isize > 0) {
    LineShape aline1;
    const int igapadd = ComputeEllipseLineStep(m_igap, isize);
    int icx0 = icentx;
    int icy0 = icenty;
    for (int i = 0; i < isize; i += igapadd) {
      gp_Pnt apoint = getpath().ElementAt(i);
      aline1.setline(icx0, icy0, RoundToInt(apoint.X()),
                     RoundToInt(apoint.Y()));
      std::vector<gp_Pnt> acrosspoints =
          aline1.getpath().IntersectPaths(getpath2());
      if (!acrosspoints.empty()) {
        LineShape scan_line;
        scan_line.setline(RoundToInt(acrosspoints[0].X()),
                          RoundToInt(acrosspoints[0].Y()),
                          RoundToInt(apoint.X()), RoundToInt(apoint.Y()));
        scan_line.setPercent(m_dsamplerate);
        m_lines.push_back(scan_line);
      }
      aline1.clear();
    }
  }

  m_scan_geometry_policy = "setellipse2_intersection_boundary";
  m_scan_lines_outside_roi_count = 0;
  m_scan_lines_cross_outside_ellipse_count = 0;
  m_scan_endpoint_norm_min = 999.0;
  m_scan_endpoint_norm_max = -999.0;

  if (m_has_display_roi) {
    const double cx = static_cast<double>(m_roi_x0 + m_roi_x1) * 0.5;
    const double cy = static_cast<double>(m_roi_y0 + m_roi_y1) * 0.5;
    const double width0 = std::abs(static_cast<double>(m_roi_x1 - m_roi_x0));
    const double height0 = std::abs(static_cast<double>(m_roi_y1 - m_roi_y0));
    const double rx = width0 * 0.5;
    const double ry = height0 * 0.5;

    for (auto &line : m_lines) {
      const int line_size = line.getlinesize();
      if (line_size < 2)
        continue;

      gp_Pnt p0 = line.getlinepoint(0);
      gp_Pnt p1 = line.getlinepoint(line_size - 1);

      const double n0 = EllipseNorm(p0.X(), p0.Y(), cx, cy, rx, ry);
      const double n1 = EllipseNorm(p1.X(), p1.Y(), cx, cy, rx, ry);

      m_scan_endpoint_norm_min =
          std::min(m_scan_endpoint_norm_min, std::min(n0, n1));
      m_scan_endpoint_norm_max =
          std::max(m_scan_endpoint_norm_max, std::max(n0, n1));

      if (n0 > 1.05 || n1 > 1.05)
        m_scan_lines_cross_outside_ellipse_count++;
    }
  }
}

void FindEllipse::translate(int ix, int iy) { Translate(gp_Vec(ix, iy, 0)); }
void FindEllipse::Translate(const gp_Vec &translationVector) {
  int ix0 = RoundToInt(translationVector.X());
  int iy0 = RoundToInt(translationVector.Y());
  getpath().Translate(translationVector);
  m_Line.Move(ix0, iy0);
  LineShape aline1, aline2;
  for (std::size_t i = 0; i < m_lines.size(); ++i) {
    m_lines[i].Move(ix0, iy0);
  }

  m_roi_x0 += ix0;
  m_roi_y0 += iy0;
  m_roi_x1 += ix0;
  m_roi_y1 += iy0;
}
void FindEllipse::drawpattern() {
  m_modelpoints.setshow(8);
  m_modelpoints.drawshape(getpath());
  m_measurepoints.drawshape(getpath());
  m_measurepoints_.drawshape(getpath());
}
void FindEllipse::drawpatternx(double dmovx, double dmovy, double dangle,
                               double dzoomx, double dzoomy) {
  m_modelpoints.setshow(8);
  m_modelpoints.drawshapex(getpath(), dmovx, dmovy, dangle, dzoomx, dzoomy);
  m_measurepoints.drawshapex(getpath(), dmovx, dmovy, dangle, dzoomx, dzoomy);
  m_measurepoints_.drawshapex(getpath(), dmovx, dmovy, dangle, dzoomx, dzoomy);
}
void FindEllipse::edgepattern(Image &image) {
  m_measurepoints.clear();
  m_measurepoints_.clear();
  m_modelpoints.clear();

  Setgap(gap());

  setselectedgenum(0);
  setmethod(0);
  Measure(image);
  m_measurepoints.addpoints(getresultpoints());

  m_measurepoints.doublepattern(m_icomparegap, 12, m_modelpoints);

  setselectedgenum(0);
  setmethod(1);
  Measure(image);
  m_measurepoints_.addpoints(getresultpoints());

  m_measurepoints_.doublepattern(m_icomparegap, 6, m_modelpoints);
}
void FindEllipse::patternzeroposition() {
  gp_Rectangle arect1 = m_modelpoints.boundingRect();
  m_modelpoints.Move(RoundToInt(-arect1.TopLeft().X()),
                     RoundToInt(-arect1.TopLeft().Y()));
}
void FindEllipse::savepatternfile(const char *pchar) {
  m_modelpoints.save(pchar);
}
void FindEllipse::loadpatternfile(const char *pchar) {
  m_modelpoints.load(pchar);
}
gp_Rectangle FindEllipse::patternboundingrect() {
  return m_modelpoints.boundingRect();
}
void FindEllipse::patterngap2gap(int inewgap) {
  m_modelpoints.patterngap2gap(inewgap);
}
void FindEllipse::patternrootgrid(double itype, double drate, double ilevel) {
  m_modelpoints.keysrootgrid(RoundToInt(itype), drate, RoundToInt(ilevel));
}
void FindEllipse::patterntranform(int igap, int itype, int isgap, int iline) {
  m_modelpoints.patterntranform(igap, itype, isgap, iline);
}
void FindEllipse::patternzoom(double dx, double dy, double igap, double itype) {
  m_modelpoints.patternzoom(RoundToInt(dx), RoundToInt(dy), RoundToInt(igap),
                            RoundToInt(itype));
}
void FindEllipse::patternrotate(double dangle) {
  m_modelpoints.Rotate(RoundToInt(dangle));
}
void FindEllipse::modelzoom(double dx, double dy) {
  m_modelpoints.Zoom(RoundToInt(dx), RoundToInt(dy));
}
gp_Path &FindEllipse::getpatternpath() { return m_modelpoints.getpath(); }
PointsShape &FindEllipse::getpattern() { return m_modelpoints; }
void FindEllipse::findpattern(void *pimage) {
  Image *pgetimage = (Image *)pimage;
  if (pgetimage == nullptr)
    return;
  edgepattern(*pgetimage);
}
void FindEllipse::drawshape() { Shape::drawshape(); }
void FindEllipse::drawshapex(double dmovx, double dmovy, double dangle,
                             double dzoomx, double dzoomy) {
  Shape::drawshapex(dmovx, dmovy, dangle, dzoomx, dzoomy);
}

void FindEllipse::setlinesamplerate(double dsamplerate) {
  m_dsamplerate = dsamplerate;
}
void FindEllipse::setlinegap(int igap) { m_iSelectPointGap = igap; }
void FindEllipse::setminedgerunwidth(int width) {
  m_min_edge_run_width_px = std::max(1, std::min(20, width));
}
void FindEllipse::setboundaryresponseenabled(int enabled) {
  m_boundary_response_enabled = enabled != 0;
}
void FindEllipse::setboundaryresponsemode(int mode) {
  m_boundary_response_config.response_mode = ClampEnumInt(
      mode, 0, 17,
      cxvision::metrology_analytics::CxBoundaryResponseMode::Auto);
  m_boundary_response_enabled = true;
}
int FindEllipse::getboundaryresponsemode() const {
  return static_cast<int>(m_boundary_response_config.response_mode);
}
void FindEllipse::setboundarypolarity(int polarity) {
  m_boundary_response_config.polarity = ClampEnumInt(
      polarity, 0, 2,
      cxvision::metrology_analytics::CxBoundaryPolarity::Either);
}
int FindEllipse::getboundarypolarity() const {
  return static_cast<int>(m_boundary_response_config.polarity);
}
void FindEllipse::setboundaryselection(int selection) {
  m_boundary_response_config.selection_mode = ClampEnumInt(
      selection, 0, 5,
      cxvision::metrology_analytics::CxBoundarySelectionMode::Strongest);
  m_boundary_response_selection_explicit = true;
}
int FindEllipse::getboundaryselection() const {
  return static_cast<int>(m_boundary_response_config.selection_mode);
}
void FindEllipse::setboundarynthcandidate(int nth) {
  m_boundary_response_config.nth_candidate = std::max(1, nth);
}
void FindEllipse::setboundaryreferencepositionpermille(int value) {
  m_boundary_response_config.reference_position_permille = ClampPermille(value);
}
void FindEllipse::setboundarysubpixel(int mode) {
  m_boundary_response_config.subpixel_mode = ClampEnumInt(
      mode, 0, 2,
      cxvision::metrology_analytics::CxBoundarySubpixelMode::ParabolicResponse);
}
void FindEllipse::setboundarybaseline(int mode) {
  m_boundary_response_config.baseline_mode = ClampEnumInt(
      mode, 0, 4,
      cxvision::metrology_analytics::CxBoundaryBaselineMode::Offset);
}
void FindEllipse::setboundarydenoise(int mode) {
  m_boundary_response_config.denoise_mode = ClampEnumInt(
      mode, 0, 5,
      cxvision::metrology_analytics::CxBoundaryDenoiseMode::Gaussian);
}
void FindEllipse::setboundarysmoothingradius(int radius) {
  m_boundary_response_config.smoothing_radius =
      std::max(0, std::min(20, radius));
}
void FindEllipse::setboundarybaselinewindow(int window) {
  m_boundary_response_config.baseline_window =
      std::max(1, std::min(256, window));
}
void FindEllipse::setboundarywaveletscale(int scale) {
  m_boundary_response_config.wavelet_scale =
      std::max(1, std::min(64, scale));
}
void FindEllipse::setboundarythresholdpermille(int value) {
  m_boundary_response_config.trigger_threshold_permille = ClampPermille(value);
}
void FindEllipse::setboundarylevelpermille(int value) {
  m_boundary_response_config.level_permille = ClampPermille(value);
}
void FindEllipse::setboundaryhysteresispermille(int value) {
  m_boundary_response_config.hysteresis_permille = ClampPermille(value);
}
void FindEllipse::setboundarygatestartpermille(int value) {
  m_boundary_response_config.gate_start_permille = ClampPermille(value);
}
void FindEllipse::setboundarygateendpermille(int value) {
  m_boundary_response_config.gate_end_permille = ClampPermille(value);
}
void FindEllipse::setboundaryminplateauwidth(int width) {
  m_boundary_response_config.min_plateau_width =
      std::max(1, std::min(256, width));
}
void FindEllipse::setboundaryminamplitudepermille(int value) {
  m_boundary_response_config.min_amplitude_permille = ClampPermille(value);
}
void FindEllipse::setboundarypairminwidth(int width) {
  m_boundary_response_config.pair_min_width =
      std::max(1, std::min(1024, width));
  if (m_boundary_response_config.pair_max_width <
      m_boundary_response_config.pair_min_width)
    m_boundary_response_config.pair_max_width =
        m_boundary_response_config.pair_min_width;
}
void FindEllipse::setboundarypairmaxwidth(int width) {
  const int new_width = std::max(1, std::min(4096, width));
  m_boundary_response_config.pair_max_width =
      std::max(new_width, m_boundary_response_config.pair_min_width);
}
void FindEllipse::setboundarypairanchor(int anchor) {
  m_boundary_response_config.pair_anchor_mode = ClampEnumInt(
      anchor, 0, 2,
      cxvision::metrology_analytics::CxBoundaryPairAnchorMode::Center);
}
void FindEllipse::setmethod(int imethod) { m_iMethod = imethod; }
void FindEllipse::setthre(int ithre) { m_iThreshold = ithre; }
int FindEllipse::thre() { return m_iThreshold; }
void FindEllipse::setgamarate(int igama) { m_igamarate = igama; }

void FindEllipse::setfindsetting(int ifindset) { m_ifindset = ifindset; }
void FindEllipse::setfilter(int ifilterborw, int ifiltermin, int ifiltermax) {
  m_ifilterborw = ifilterborw;
  m_ifiltermin = ifiltermin;
  m_ifiltermax = ifiltermax;
}
void FindEllipse::MeasureT(void *pimage) { (void)pimage; }
void FindEllipse::Measure(Image &image) {
  SetCxCrashBreadcrumb("FindEllipse::Measure:enter");
  m_has_fit_result = false;
  m_fit_avgdist = 0.0;
  m_measure_failure_stage.clear();
  m_measure_failure_reason.clear();

  m_scan_candidate_lines = 0;
  m_scan_total_candidates = 0;
  m_scan_accepted_points_before_gate = 0;
  m_boundary_response_scan_lines_evaluated = 0;
  m_boundary_response_candidate_count = 0;
  m_boundary_response_points_emitted = 0;
  m_boundary_response_rejected_no_candidate = 0;
  m_boundary_response_rejected_endpoint = 0;
  m_accepted_boundary_ratio_sum = 0.0;
  m_accepted_boundary_ratio_min = 999.0;
  m_accepted_boundary_ratio_max = -999.0;
  m_candidate_policy = "ellipse_boundary_band_nearest_norm_loose_fallback";
  if (m_boundary_response_enabled)
    m_candidate_policy = "ellipse_boundary_response_profile_trigger";

  m_accepted_points_outside_ellipse_count = 0;
  m_accepted_point_norm_sum = 0.0;
  m_accepted_point_norm_count = 0;
  m_accepted_point_norm_min = 999.0;
  m_accepted_point_norm_max = -999.0;
  m_rejected_boundary_band_candidate_count = 0;
  m_rejected_boundary_band_norm_sum = 0.0;
  m_rejected_boundary_band_norm_min = 999.0;
  m_rejected_boundary_band_norm_max = -999.0;
  m_point_consistency_input_points = 0;
  m_point_consistency_output_points = 0;
  m_point_consistency_removed_points = 0;

  if (image.getmat().empty() || image.getWidth() <= 0 ||
      image.getHeight() <= 0) {
    m_measure_failure_stage = "input_image_empty";
    m_measure_failure_reason =
        "FindEllipse input image is empty or has invalid dimensions.";
    LogFindEllipseMeasureProbe("measure_preflight", "failed",
                               m_measure_failure_reason);
    return;
  }

  cv::Mat boundary_gray;
  if (m_boundary_response_enabled) {
    if (image.getmat().channels() == 1)
      boundary_gray = image.getmat();
    else
      cv::cvtColor(image.getmat(), boundary_gray, cv::COLOR_BGR2GRAY);
  }

  {
    std::ostringstream oss;
    oss << "enter image=" << image.getWidth() << "x" << image.getHeight()
        << " roi=(" << m_roi_x0 << "," << m_roi_y0 << ")-(" << m_roi_x1 << ","
        << m_roi_y1 << ")"
        << " gap=" << m_igap << " linegap=" << m_iSelectPointGap
        << " min_edge_run_width_px=" << m_min_edge_run_width_px
        << " threshold=" << m_iThreshold << " method=" << m_iMethod
        << " existing_scan_lines=" << m_lines.size();
    LogFindEllipseMeasureProbe("measure_enter", "running", oss.str());
  }

  SetCxCrashBreadcrumb("FindEllipse::Measure:ensure_resources");
  if (!ImageManager::EnsureAlgorithmRuntimeResources(image.getWidth(),
                                                     image.getHeight())) {
    m_measure_failure_stage = "runtime_resources";
    m_measure_failure_reason = "FindEllipse failed to initialize ImageManager "
                               "algorithm runtime resources.";
    LogFindEllipseMeasureProbe("measure_preflight", "failed",
                               m_measure_failure_reason);
    return;
  }
  g_pbackimage = ImageManager::GetBackImage(1);
  g_pbackfindobject = ImageManager::Getbackfindobject(1);

  SetCxCrashBreadcrumb("FindEllipse::Measure:roi_preflight");
  if (image.getWidth() < rect().TopLeft().X() + rect().Width() ||
      image.getHeight() < rect().TopLeft().Y() + rect().Height()) {
    m_measure_failure_stage = "roi_outside_image";
    m_measure_failure_reason =
        "FindEllipse ROI rectangle is outside input image.";
    LogFindEllipseMeasureProbe("measure_preflight", "failed",
                               m_measure_failure_reason);
    return;
  }
  if (rect().TopLeft().X() < 0 || rect().TopLeft().Y() < 0) {
    m_measure_failure_stage = "roi_negative";
    m_measure_failure_reason = "FindEllipse ROI rectangle has negative origin.";
    LogFindEllipseMeasureProbe("measure_preflight", "failed",
                               m_measure_failure_reason);
    return;
  }
  m_measurepoints.clear();
  m_scan_diagnostics.clear();
  int isize = ClampSizeToInt(m_lines.size());

  const double ellipse_cx =
      m_has_display_roi ? static_cast<double>(m_roi_x0 + m_roi_x1) * 0.5 : 0.0;
  const double ellipse_cy =
      m_has_display_roi ? static_cast<double>(m_roi_y0 + m_roi_y1) * 0.5 : 0.0;
  const double ellipse_rx =
      m_has_display_roi
          ? std::abs(static_cast<double>(m_roi_x1 - m_roi_x0)) * 0.5
          : 1.0;
  const double ellipse_ry =
      m_has_display_roi
          ? std::abs(static_cast<double>(m_roi_y1 - m_roi_y0)) * 0.5
          : 1.0;
  if (isize <= 0) {
    m_measure_failure_stage = "scan_lines_empty";
    m_measure_failure_reason =
        "FindEllipse has no scan lines; check setellipse/setgap order.";
    LogFindEllipseMeasureProbe("measure_preflight", "failed",
                               m_measure_failure_reason);
    return;
  }
  if (g_pbackimage == nullptr) {
    m_measure_failure_stage = "backimage_null";
    m_measure_failure_reason = "FindEllipse back image is null.";
    LogFindEllipseMeasureProbe("measure_preflight", "failed",
                               m_measure_failure_reason);
    return;
  }
  if (g_pbackimage == &image) {
    m_measure_failure_stage = "backimage_alias_input";
    m_measure_failure_reason =
        "FindEllipse back image aliases the input image.";
    LogFindEllipseMeasureProbe("measure_preflight", "failed",
                               m_measure_failure_reason);
    return;
  }
  if (g_pbackimage->getmat().empty()) {
    m_measure_failure_stage = "backimage_empty";
    m_measure_failure_reason = "FindEllipse back image Mat is empty.";
    LogFindEllipseMeasureProbe("measure_preflight", "failed",
                               m_measure_failure_reason);
    return;
  }

  int ilineslen1 = 0;

  if (isize > 0)
    ilineslen1 = m_lines[0].getlinesize();

  int min_line_len = ilineslen1;
  int max_line_len = ilineslen1;
  int invalid_line_count = 0;
  int outside_endpoint_count = 0;
  for (int i = 0; i < isize; ++i) {
    const int line_len = m_lines[i].getlinesize();
    if (line_len <= 0) {
      ++invalid_line_count;
      continue;
    }
    min_line_len = std::min(min_line_len, line_len);
    max_line_len = std::max(max_line_len, line_len);

    gp_Pnt p0 = m_lines[i].getlinepoint(0);
    gp_Pnt p1 = m_lines[i].getlinepoint(line_len - 1);
    const bool p0_finite = std::isfinite(p0.X()) && std::isfinite(p0.Y());
    const bool p1_finite = std::isfinite(p1.X()) && std::isfinite(p1.Y());
    if (!p0_finite || !p1_finite) {
      ++invalid_line_count;
      continue;
    }

    if (p0.X() < 0.0 || p0.Y() < 0.0 ||
        p0.X() >= static_cast<double>(image.getWidth()) ||
        p0.Y() >= static_cast<double>(image.getHeight()) || p1.X() < 0.0 ||
        p1.Y() < 0.0 || p1.X() >= static_cast<double>(image.getWidth()) ||
        p1.Y() >= static_cast<double>(image.getHeight())) {
      ++outside_endpoint_count;
    }
  }

  if (invalid_line_count > 0 || min_line_len <= 0) {
    m_measure_failure_stage = "scan_line_invalid";
    std::ostringstream oss;
    oss << "FindEllipse generated invalid scan lines before linecopyex."
        << " invalid_line_count=" << invalid_line_count
        << " scan_lines=" << isize << " min_line_len=" << min_line_len
        << " max_line_len=" << max_line_len;
    m_measure_failure_reason = oss.str();
    LogFindEllipseMeasureProbe("measure_preflight", "failed",
                               m_measure_failure_reason);
    return;
  }

  int iprocessw = min_line_len;

  if (iprocessw <= 0 || isize > g_pbackimage->getHeight() ||
      iprocessw > g_pbackimage->getWidth()) {
    m_measure_failure_stage = "scan_buffer_too_small";
    std::ostringstream oss;
    oss << "FindEllipse scan buffer is smaller than generated scan geometry."
        << " scan_lines=" << isize << " process_w=" << iprocessw
        << " back=" << g_pbackimage->getWidth() << "x"
        << g_pbackimage->getHeight();
    m_measure_failure_reason = oss.str();
    LogFindEllipseMeasureProbe("measure_preflight", "failed",
                               m_measure_failure_reason);
    return;
  }

  {
    std::ostringstream oss;
    oss << "scan geometry ready scan_lines=" << isize
        << " process_w=" << iprocessw << " min_line_len=" << min_line_len
        << " max_line_len=" << max_line_len
        << " outside_endpoint_count=" << outside_endpoint_count
        << " back=" << g_pbackimage->getWidth() << "x"
        << g_pbackimage->getHeight();
    LogFindEllipseMeasureProbe("measure_scan_geometry", "ready", oss.str());
  }

  SetCxCrashBreadcrumb("FindEllipse::Measure:linecopyex");
  for (int i = 0; i < isize; i++) {
    m_lines[i].linecopyex(image, *g_pbackimage, 0, i);
  }
  LogFindEllipseMeasureProbe("measure_linecopyex", "finished",
                             "FindEllipse linecopyex completed.");

  SetCxCrashBreadcrumb("FindEllipse::Measure:preprocess_roi");
  g_pbackimage->setroi(0, 0, iprocessw, isize);

  g_pbackimage->roi_7blur_gap_mud_thre_bw(m_iThreshold, m_igamarate,
                                          m_iSelectPointGap, m_iMethod);
  LogFindEllipseMeasureProbe("measure_preprocess", "finished",
                             "FindEllipse preprocess completed.");

  if ((m_ifindset & 0x01) && g_pbackfindobject != nullptr) {
    SetCxCrashBreadcrumb("FindEllipse::Measure:findobject");
    g_pbackfindobject->setrect(0, 0, iprocessw, isize);
    g_pbackfindobject->setbrow(m_ifilterborw);
    g_pbackfindobject->setminmaxarea(
        ClampLongLongToInt(static_cast<long long>(m_ifiltermin)),
        ClampLongLongToInt(static_cast<long long>(m_ifiltermax)));
    g_pbackfindobject->Measure(*g_pbackimage);
    LogFindEllipseMeasureProbe("measure_findobject", "finished",
                               "FindEllipse FindObject filter completed.");
  }

  std::vector<int> irecordpoint;
  irecordpoint.reserve(128);
  bool bcollectBegin = false;

  int icurlinenum = 0;
  int icurlineposition = 0;

  int ifixvalue = 3;

  cv::Vec3b icolor = 0;
  SetCxCrashBreadcrumb("FindEllipse::Measure:candidate_collect");
  auto try_boundary_response_scan = [&](int scan_index) -> bool {
    if (!m_boundary_response_enabled || boundary_gray.empty())
      return false;
    if (scan_index < 0 || scan_index >= isize)
      return true;

    ++m_boundary_response_scan_lines_evaluated;
    std::vector<float> profile =
        BuildEllipseGrayProfile(boundary_gray, m_lines[scan_index],
                                ilineslen1);

    auto config = m_boundary_response_config;
    config.min_plateau_width =
        std::max(config.min_plateau_width, m_min_edge_run_width_px);
    if (!m_boundary_response_selection_explicit) {
      if (m_iselectedgenum > 0) {
        config.selection_mode =
            cxvision::metrology_analytics::CxBoundarySelectionMode::Nth;
        config.nth_candidate = m_iselectedgenum;
      } else if (m_iselectedgenum == -1) {
        config.selection_mode =
            cxvision::metrology_analytics::CxBoundarySelectionMode::Last;
      }
    }

    const cxvision::metrology_analytics::CxBoundaryResponseResult response =
        cxvision::metrology_analytics::EvaluateBoundaryResponse(profile,
                                                                config);
    const int candidate_count = ClampSizeToInt(response.candidates.size());
    m_boundary_response_candidate_count += candidate_count;
    if (candidate_count > 0) {
      ++m_scan_candidate_lines;
      m_scan_total_candidates += candidate_count;
    }

    auto& diag = EnsureFindEllipseScanDiagnostic(m_scan_diagnostics,
                                                 scan_index);
    diag.candidate_count = candidate_count;
    diag.min_edge_run_width_px = m_min_edge_run_width_px;
    diag.boundary_response_used = true;
    diag.boundary_response_mode =
        static_cast<int>(response.effective_response_mode);
    diag.boundary_response_candidate_count = candidate_count;
    diag.boundary_response_selected_index = response.selected_candidate;
    diag.boundary_response_status = response.status;
    diag.boundary_response_reason = response.reason;

    if (response.selected_candidate < 0 ||
        response.selected_candidate >= candidate_count) {
      ++m_boundary_response_rejected_no_candidate;
      if (diag.reject_reason.empty())
        diag.reject_reason = response.reason.empty()
                                 ? "boundary_no_candidate"
                                 : response.reason;
      return true;
    }

    const auto& candidate =
        response.candidates[static_cast<std::size_t>(
            response.selected_candidate)];
    const double sample_position = std::isfinite(candidate.position_samples)
                                       ? candidate.position_samples
                                       : static_cast<double>(
                                             candidate.sample_index);
    const int candidate_position = RoundToInt(sample_position);
    if (candidate_position >= (ilineslen1 - m_iSelectPointGap - 3) ||
        candidate_position <= m_iSelectPointGap + 3) {
      ++m_boundary_response_rejected_endpoint;
      if (diag.reject_reason.empty())
        diag.reject_reason = "boundary_endpoint_rejected";
      return true;
    }

    gp_Pnt apoint =
        InterpolateEllipseLinePoint(m_lines[scan_index], sample_position,
                                    ilineslen1);
    const double norm =
        EllipseNorm(apoint.X(), apoint.Y(), ellipse_cx, ellipse_cy,
                    ellipse_rx, ellipse_ry);
    constexpr double kBoundaryBandLooseMin = 0.45;
    constexpr double kBoundaryBandMax = 1.08;
    if (!std::isfinite(norm) || norm < kBoundaryBandLooseMin ||
        norm > kBoundaryBandMax) {
      ++m_boundary_response_rejected_endpoint;
      ++m_rejected_boundary_band_candidate_count;
      if (std::isfinite(norm)) {
        m_rejected_boundary_band_norm_sum += norm;
        m_rejected_boundary_band_norm_min =
            std::min(m_rejected_boundary_band_norm_min, norm);
        m_rejected_boundary_band_norm_max =
            std::max(m_rejected_boundary_band_norm_max, norm);
      }
      if (diag.reject_reason.empty())
        diag.reject_reason = "boundary_band_rejected";
      return true;
    }

    m_accepted_point_norm_sum += norm;
    m_accepted_point_norm_count++;
    m_accepted_point_norm_min = std::min(m_accepted_point_norm_min, norm);
    m_accepted_point_norm_max = std::max(m_accepted_point_norm_max, norm);
    if (norm > 1.05)
      m_accepted_points_outside_ellipse_count++;

    m_measurepoints.addpoint(apoint);
    ++m_boundary_response_points_emitted;
    ++m_scan_accepted_points_before_gate;
    m_accepted_boundary_ratio_sum += norm;
    m_accepted_boundary_ratio_min =
        std::min(m_accepted_boundary_ratio_min, norm);
    m_accepted_boundary_ratio_max =
        std::max(m_accepted_boundary_ratio_max, norm);

    diag.accepted = true;
    diag.accepted_x = apoint.X();
    diag.accepted_y = apoint.Y();
    diag.accepted_position = candidate_position;
    diag.accepted_points_xy.push_back(apoint.X());
    diag.accepted_points_xy.push_back(apoint.Y());
    diag.boundary_response_selected_position = sample_position;
    diag.boundary_response_selected_score = candidate.score;
    diag.reject_reason.clear();
    return true;
  };

  for (int inumy = 0 + ifixvalue; inumy < isize - ifixvalue; inumy++) {
    std::vector<int> candidate_positions;
    candidate_positions.reserve(8);
    int line_min_edge_run_width_reject_count = 0;
    irecordpoint.clear();
    icurlinenum = 0;
    bcollectBegin = false;
    auto record_short_edge_run = [&]() {
      ++m_rejected_min_edge_run_width_count;
      ++line_min_edge_run_width_reject_count;
      auto& diag = EnsureFindEllipseScanDiagnostic(m_scan_diagnostics, inumy);
      diag.min_edge_run_width_px = m_min_edge_run_width_px;
      ++diag.rejected_min_edge_run_width;
      if (!diag.accepted && diag.reject_reason.empty())
        diag.reject_reason = "min_edge_run_width_rejected";
    };
    if (try_boundary_response_scan(inumy))
      continue;
    for (int inumx = 0; inumx < ilineslen1; inumx++) {
      icolor = g_pbackimage->pixel(inumx, inumy);
      if ((icolor[0]) > 0) {
        irecordpoint.push_back(inumx);
        bcollectBegin = true;
      } else {
        if (true == bcollectBegin && !irecordpoint.empty() &&
            static_cast<int>(irecordpoint.size()) <
                m_min_edge_run_width_px) {
          record_short_edge_run();
        } else if (true == bcollectBegin && !irecordpoint.empty() &&
            irecordpoint.size() <= 70) {
          icurlineposition =
              m_ineedfixs + irecordpoint[(irecordpoint.size() >> 1)];

          icurlinenum++;
          if (icurlinenum == m_iselectedgenum || m_iselectedgenum == 0) {
            if (icurlineposition < (ilineslen1 - m_iSelectPointGap - 3) &&
                icurlineposition > m_iSelectPointGap + 3) {
              if (m_iselectedgenum == 0) {
                candidate_positions.push_back(icurlineposition);
              } else {
                gp_Pnt apoint = m_lines[inumy].getlinepoint(icurlineposition);
                const double norm =
                    EllipseNorm(apoint.X(), apoint.Y(), ellipse_cx, ellipse_cy,
                                ellipse_rx, ellipse_ry);
                m_accepted_point_norm_sum += norm;
                m_accepted_point_norm_count++;
                m_accepted_point_norm_min =
                    std::min(m_accepted_point_norm_min, norm);
                m_accepted_point_norm_max =
                    std::max(m_accepted_point_norm_max, norm);
                if (norm > 1.05)
                  m_accepted_points_outside_ellipse_count++;
                m_measurepoints.addpoint(apoint);
                RecordFindEllipseAcceptedDiagnosticPoint(
                    m_scan_diagnostics, inumy, std::max(1, icurlinenum),
                    icurlineposition, m_min_edge_run_width_px, apoint);
                break;
              }
            }
          }
        }
        irecordpoint.clear();
        bcollectBegin = false;
      }
    }
    if (true == bcollectBegin && !irecordpoint.empty()) {
      if (static_cast<int>(irecordpoint.size()) <
          m_min_edge_run_width_px) {
        record_short_edge_run();
      } else {
        icurlineposition =
            m_ineedfixs + irecordpoint[(irecordpoint.size() >> 1)];
        icurlinenum++;
        if (icurlinenum == m_iselectedgenum || m_iselectedgenum == 0) {
          if (icurlineposition < (ilineslen1 - m_iSelectPointGap - 3) &&
              icurlineposition > m_iSelectPointGap + 3) {
            if (m_iselectedgenum == 0) {
              candidate_positions.push_back(icurlineposition);
            } else {
              gp_Pnt apoint = m_lines[inumy].getlinepoint(icurlineposition);
              const double norm =
                  EllipseNorm(apoint.X(), apoint.Y(), ellipse_cx, ellipse_cy,
                              ellipse_rx, ellipse_ry);
              m_accepted_point_norm_sum += norm;
              m_accepted_point_norm_count++;
              m_accepted_point_norm_min =
                  std::min(m_accepted_point_norm_min, norm);
              m_accepted_point_norm_max =
                  std::max(m_accepted_point_norm_max, norm);
              if (norm > 1.05)
                m_accepted_points_outside_ellipse_count++;
              m_measurepoints.addpoint(apoint);
              RecordFindEllipseAcceptedDiagnosticPoint(
                  m_scan_diagnostics, inumy, std::max(1, icurlinenum),
                  icurlineposition, m_min_edge_run_width_px, apoint);
              break;
            }
          }
        }
      }
      irecordpoint.clear();
      bcollectBegin = false;
    }

    if (m_iselectedgenum == 0 && !candidate_positions.empty()) {
      m_scan_candidate_lines++;
      m_scan_total_candidates += static_cast<int>(candidate_positions.size());

      constexpr double kBoundaryBandMin = 0.65;
      constexpr double kBoundaryBandLooseMin = 0.45;
      constexpr double kBoundaryBandMax = 1.08;
      int strict_boundary_position = -1;
      double strict_best_score = 999.0;
      double strict_best_norm = 0.0;
      int loose_boundary_position = -1;
      double loose_best_score = 999.0;
      double loose_best_norm = 0.0;
      for (const int candidate_position : candidate_positions) {
        if (candidate_position < 0 || candidate_position >= ilineslen1)
          continue;

        gp_Pnt candidate_point =
            m_lines[inumy].getlinepoint(candidate_position);
        const double candidate_norm =
            EllipseNorm(candidate_point.X(), candidate_point.Y(), ellipse_cx,
                        ellipse_cy, ellipse_rx, ellipse_ry);

        if (!std::isfinite(candidate_norm) ||
            candidate_norm < kBoundaryBandLooseMin ||
            candidate_norm > kBoundaryBandMax) {
          if (std::isfinite(candidate_norm)) {
            ++m_rejected_boundary_band_candidate_count;
            m_rejected_boundary_band_norm_sum += candidate_norm;
            m_rejected_boundary_band_norm_min =
                std::min(m_rejected_boundary_band_norm_min, candidate_norm);
            m_rejected_boundary_band_norm_max =
                std::max(m_rejected_boundary_band_norm_max, candidate_norm);
          }
          continue;
        }

        const double score = std::abs(candidate_norm - 1.0);
        if (candidate_norm >= kBoundaryBandMin) {
          if (score < strict_best_score) {
            strict_best_score = score;
            strict_best_norm = candidate_norm;
            strict_boundary_position = candidate_position;
          }
        } else if (score < loose_best_score) {
          loose_best_score = score;
          loose_best_norm = candidate_norm;
          loose_boundary_position = candidate_position;
        }
      }

      const int boundary_position = strict_boundary_position >= 0
                                        ? strict_boundary_position
                                        : loose_boundary_position;
      const double best_norm =
          strict_boundary_position >= 0 ? strict_best_norm : loose_best_norm;

      if (boundary_position < 0)
        continue;

      gp_Pnt apoint = m_lines[inumy].getlinepoint(boundary_position);

      const double norm = EllipseNorm(apoint.X(), apoint.Y(), ellipse_cx,
                                      ellipse_cy, ellipse_rx, ellipse_ry);
      m_accepted_point_norm_sum += norm;
      m_accepted_point_norm_count++;
      m_accepted_point_norm_min = std::min(m_accepted_point_norm_min, norm);
      m_accepted_point_norm_max = std::max(m_accepted_point_norm_max, norm);
      if (norm > 1.05)
        m_accepted_points_outside_ellipse_count++;

      m_measurepoints.addpoint(apoint);
      RecordFindEllipseAcceptedDiagnosticPoint(
          m_scan_diagnostics, inumy,
          static_cast<int>(candidate_positions.size()), boundary_position,
          m_min_edge_run_width_px, apoint);

      double boundary_ratio = best_norm;
      m_scan_accepted_points_before_gate++;
      m_accepted_boundary_ratio_sum += boundary_ratio;
      if (boundary_ratio < m_accepted_boundary_ratio_min)
        m_accepted_boundary_ratio_min = boundary_ratio;
      if (boundary_ratio > m_accepted_boundary_ratio_max)
        m_accepted_boundary_ratio_max = boundary_ratio;
    }
  }

  m_point_consistency_input_points = ClampSizeToInt(m_measurepoints.size());
  m_point_consistency_output_points = m_point_consistency_input_points;
  m_point_consistency_removed_points = 0;
  if (m_point_consistency_enabled != 0 && m_point_consistency_range > 0.0 &&
      m_measurepoints.size() > 0) {
    const int removed = FilterEllipsePointsByGaugeBoundaryDistance(
        m_measurepoints, ellipse_cx, ellipse_cy, ellipse_rx, ellipse_ry,
        m_point_consistency_range);
    if (removed > 0) {
      m_point_consistency_removed_points = removed;
      m_point_consistency_output_points =
          ClampSizeToInt(m_measurepoints.size());
    }
  }

  if (m_measurepoints.size() <= 0) {
    if (m_scan_total_candidates > 0 &&
        m_scan_accepted_points_before_gate <= 0) {
      m_measure_failure_stage = "no_boundary_band_candidate";
      m_measure_failure_reason = "FindEllipse found edge runs, but none are "
                                 "near the Gauge ellipse boundary band.";
      if (m_rejected_boundary_band_candidate_count > 0) {
        m_measure_failure_reason +=
            " rejected_norm=" +
            std::to_string(m_rejected_boundary_band_norm_min) + "/" +
            std::to_string(
                m_rejected_boundary_band_norm_sum /
                static_cast<double>(m_rejected_boundary_band_candidate_count)) +
            "/" + std::to_string(m_rejected_boundary_band_norm_max);
      }
    } else {
      m_measure_failure_stage = "threshold_no_edge";
      m_measure_failure_reason =
          "FindEllipse preprocessing produced no accepted edge run; check "
          "threshold/method/linegap.";
    }
  }

  {
    std::ostringstream oss;
    oss << "measure finished points=" << m_measurepoints.size()
        << " candidate_lines=" << m_scan_candidate_lines
        << " total_candidates=" << m_scan_total_candidates
        << " min_edge_run_width_px=" << m_min_edge_run_width_px
        << " rejected_min_edge_run_width="
        << m_rejected_min_edge_run_width_count
        << " accepted_before_gate=" << m_scan_accepted_points_before_gate
        << " selected_edge=" << m_iselectedgenum
        << " consistency=" << m_point_consistency_enabled << "/"
        << m_point_consistency_range
        << " removed=" << m_point_consistency_removed_points
        << " failure_stage=" << m_measure_failure_stage;
    LogFindEllipseMeasureProbe("measure_exit", "finished", oss.str());
  }
}

PointsShape &FindEllipse::getresultpoints() { return m_measurepoints; }

void FindEllipse::fitellipse() {
  m_has_fit_result = false;
  m_fit_center_x = 0.0;
  m_fit_center_y = 0.0;
  m_fit_radius_x = 0.0;
  m_fit_radius_y = 0.0;
  m_fit_angle_deg = 0.0;
  m_fit_avgdist = 0.0;

  const int count = ClampSizeToInt(m_measurepoints.size());
  if (count < 5)
    return;

  std::vector<cv::Point2f> points;
  points.reserve(static_cast<std::size_t>(count));
  for (int i = 0; i < count; ++i) {
    const double x = m_measurepoints.getx(i);
    const double y = m_measurepoints.gety(i);
    if (std::isfinite(x) && std::isfinite(y))
      points.emplace_back(static_cast<float>(x), static_cast<float>(y));
  }
  if (points.size() < 5)
    return;

  const double roi_center_x = static_cast<double>(m_roi_x0 + m_roi_x1) * 0.5;
  const double roi_center_y = static_cast<double>(m_roi_y0 + m_roi_y1) * 0.5;
  const double roi_radius_x =
      std::abs(static_cast<double>(m_roi_x1 - m_roi_x0)) * 0.5;
  const double roi_radius_y =
      std::abs(static_cast<double>(m_roi_y1 - m_roi_y0)) * 0.5;
  if (!HasSufficientEllipseAngularCoverage(points, roi_center_x, roi_center_y,
                                           roi_radius_x, roi_radius_y)) {
    m_measure_failure_stage = "insufficient_boundary_coverage";
    m_measure_failure_reason =
        "FindEllipse rejected edge set because accepted points do not cover "
        "enough of the Gauge ellipse.";
    return;
  }

  cv::RotatedRect fitted;
  try {
    fitted = cv::fitEllipse(points);
  } catch (const cv::Exception &) {
    return;
  }

  const double rx = std::abs(static_cast<double>(fitted.size.width)) * 0.5;
  const double ry = std::abs(static_cast<double>(fitted.size.height)) * 0.5;
  if (rx <= 0.0 || ry <= 0.0 || !std::isfinite(rx) || !std::isfinite(ry) ||
      !std::isfinite(static_cast<double>(fitted.center.x)) ||
      !std::isfinite(static_cast<double>(fitted.center.y))) {
    return;
  }

  const double min_radius_ratio = std::min(rx / std::max(roi_radius_x, 1.0),
                                           ry / std::max(roi_radius_y, 1.0));
  const double max_radius_ratio = std::max(rx / std::max(roi_radius_x, 1.0),
                                           ry / std::max(roi_radius_y, 1.0));
  if (min_radius_ratio < 0.45 || max_radius_ratio > 3.0) {
    m_measure_failure_stage = "fit_too_small_for_gauge";
    m_measure_failure_reason =
        "FindEllipse rejected local edge cluster because fitted ellipse is too "
        "small compared with the Gauge.";
    return;
  }

  m_fit_center_x = fitted.center.x;
  m_fit_center_y = fitted.center.y;
  m_fit_radius_x = rx;
  m_fit_radius_y = ry;
  m_fit_angle_deg = fitted.angle;

  const double angle = m_fit_angle_deg * CV_PI / 180.0;
  const double ca = std::cos(angle);
  const double sa = std::sin(angle);
  const double scale = (rx + ry) * 0.5;
  double sum = 0.0;
  int used = 0;
  for (const cv::Point2f &point : points) {
    const double dx = static_cast<double>(point.x) - m_fit_center_x;
    const double dy = static_cast<double>(point.y) - m_fit_center_y;
    const double local_x = ca * dx + sa * dy;
    const double local_y = -sa * dx + ca * dy;
    const double norm = std::sqrt((local_x * local_x) / (rx * rx) +
                                  (local_y * local_y) / (ry * ry));
    if (std::isfinite(norm)) {
      sum += std::abs(norm - 1.0) * scale;
      ++used;
    }
  }
  m_fit_avgdist = used > 0 ? sum / static_cast<double>(used) : 0.0;
  m_has_fit_result = true;
}

double FindEllipse::getresultcentx() { return m_fit_center_x; }

double FindEllipse::getresultcenty() { return m_fit_center_y; }

double FindEllipse::getresultradiusx() { return m_fit_radius_x; }

double FindEllipse::getresultradiusy() { return m_fit_radius_y; }

double FindEllipse::getresultangle() { return m_fit_angle_deg; }

double FindEllipse::getavgdist() { return m_fit_avgdist; }

int FindEllipse::getvalidpointcount() {
  return static_cast<int>(m_measurepoints.size());
}

double FindEllipse::hasfitresult() { return m_has_fit_result ? 1.0 : 0.0; }

void FindEllipse::measure(void *pimage) {
  SetCxCrashBreadcrumb("FindEllipse::measure:void_ptr_enter");
  Image *pgetimage = (Image *)pimage;
  if (pgetimage == nullptr) {
    m_measure_failure_stage = "input_image_null";
    m_measure_failure_reason =
        "FindEllipse measure received a null Image pointer.";
    LogFindEllipseMeasureProbe("measure_wrapper", "failed",
                               m_measure_failure_reason);
    return;
  }
  LogFindEllipseMeasureProbe("measure_wrapper", "running",
                             "FindEllipse measure received Image pointer.");
  Measure(*pgetimage);
}
void FindEllipse::measureRobust(void *pimage) {
  SetCxCrashBreadcrumb("FindEllipse::measureRobust:void_ptr_enter");
  Image *pgetimage = (Image *)pimage;
  if (pgetimage == nullptr) {
    m_measure_failure_stage = "input_image_null";
    m_measure_failure_reason =
        "FindEllipse measureRobust received a null Image pointer.";
    return;
  }
  MeasureRobust(*pgetimage);
}
void FindEllipse::shapesetroi(void *pshape) {
  if (pshape == nullptr)
    return;
  Shape::shapesetroi(pshape);
}
void FindEllipse::easycluster(int igapx, int igapy, int iclusternum) {
  PointsShape resultpoints;
  resultpoints.addpoints(getresultpoints());
  vector<int> numlist;
  int isize = ClampSizeToInt(resultpoints.size());
  if (0 == isize)
    return;
  for (int i = 0; i < isize; i++) {
    numlist.push_back(1);
  }
  for (int i = 0; i < isize; i++) {
    int ix0 = RoundToInt(resultpoints.getx(i));
    int iy0 = RoundToInt(resultpoints.gety(i));
    if (i + 1 < isize)
      for (int j = i + 1; j < isize; j++) {
        int ix1 = RoundToInt(resultpoints.getx(j));
        int iy1 = RoundToInt(resultpoints.gety(j));
        if (abs(ix0 - ix1) < igapx && abs(iy0 - iy1) < igapy) {
          numlist[i]++;
          numlist[j]++;
        }
      }
  }
  PointsShape amodelpointsw;
  PointsShape amodelpointsh;
  int isize1 = ClampSizeToInt(getresultpoints().size());
  getresultpoints().clear();
  int inum = 0;
  for (inum = 0; inum < isize1; inum++) {
    int ix0 = RoundToInt(resultpoints.getx(inum));
    int iy0 = RoundToInt(resultpoints.gety(inum));
    int inumsum = numlist[inum];
    if (inumsum > iclusternum) {
      getresultpoints().addpoint(ix0, iy0);
    }
  }
}

bool FindEllipse::getdisplaysnapshot(FindEllipseDisplaySnapshot &out) const {
  out = {};

  out.has_roi = m_has_display_roi;

  if (m_has_display_roi) {
    out.center_x = static_cast<double>(m_roi_x0 + m_roi_x1) * 0.5;
    out.center_y = static_cast<double>(m_roi_y0 + m_roi_y1) * 0.5;
    out.radius_x = std::abs(static_cast<double>(m_roi_x1 - m_roi_x0)) * 0.5;
    out.radius_y = std::abs(static_cast<double>(m_roi_y1 - m_roi_y0)) * 0.5;
    out.inner_scale_percent = m_inner_scale_percent;
    out.has_inner_ellipse = m_inner_scale_percent > 0;
    out.inner_radius_x =
        out.radius_x * static_cast<double>(m_inner_scale_percent) * 0.01;
    out.inner_radius_y =
        out.radius_y * static_cast<double>(m_inner_scale_percent) * 0.01;
  }

  const int measure_point_count = ClampSizeToInt(m_measurepoints.size());
  out.has_measure_points = measure_point_count > 0;
  out.measure_points_count = measure_point_count;
  out.has_fit_ellipse = m_has_fit_result;
  out.fit_center_x = m_fit_center_x;
  out.fit_center_y = m_fit_center_y;
  out.fit_radius_x = m_fit_radius_x;
  out.fit_radius_y = m_fit_radius_y;
  out.fit_angle_deg = m_fit_angle_deg;
  out.fit_avgdist = m_fit_avgdist;

  out.gap = m_igap;
  out.linegap = m_iSelectPointGap;
  out.min_edge_run_width_px = m_min_edge_run_width_px;
  out.threshold = m_iThreshold;
  out.method = m_iMethod;
  out.selected_edge_index = m_iselectedgenum;
  out.scan_line_count = static_cast<int>(m_lines.size());
  out.scan_line_length = out.has_roi
                             ? RoundToInt(std::max(out.radius_x, out.radius_y) * 2.0)
                             : 0;
  out.measure_failure_stage = m_measure_failure_stage;
  out.measure_failure_reason = m_measure_failure_reason;

  out.scan_candidate_lines = m_scan_candidate_lines;
  out.scan_total_candidates = m_scan_total_candidates;
  out.rejected_min_edge_run_width_count =
      m_rejected_min_edge_run_width_count;
  out.scan_accepted_points_before_gate = m_scan_accepted_points_before_gate;
  out.boundary_response_enabled = m_boundary_response_enabled ? 1 : 0;
  out.boundary_response_mode =
      static_cast<int>(m_boundary_response_config.response_mode);
  out.boundary_response_polarity =
      static_cast<int>(m_boundary_response_config.polarity);
  out.boundary_response_selection =
      static_cast<int>(m_boundary_response_config.selection_mode);
  out.boundary_response_scan_lines_evaluated =
      m_boundary_response_scan_lines_evaluated;
  out.boundary_response_candidate_count =
      m_boundary_response_candidate_count;
  out.boundary_response_points_emitted =
      m_boundary_response_points_emitted;
  out.boundary_response_rejected_no_candidate =
      m_boundary_response_rejected_no_candidate;
  out.boundary_response_rejected_endpoint =
      m_boundary_response_rejected_endpoint;
  out.accepted_min_boundary_ratio = m_scan_accepted_points_before_gate > 0
                                        ? m_accepted_boundary_ratio_min
                                        : 0.0;
  out.accepted_max_boundary_ratio = m_scan_accepted_points_before_gate > 0
                                        ? m_accepted_boundary_ratio_max
                                        : 0.0;
  out.accepted_avg_boundary_ratio =
      m_scan_accepted_points_before_gate > 0
          ? m_accepted_boundary_ratio_sum /
                static_cast<double>(m_scan_accepted_points_before_gate)
          : 0.0;
  out.candidate_policy = m_candidate_policy;

  out.scan_lines_outside_roi_count = m_scan_lines_outside_roi_count;
  out.scan_lines_cross_outside_ellipse_count =
      m_scan_lines_cross_outside_ellipse_count;
  out.scan_endpoint_norm_min = m_scan_lines_cross_outside_ellipse_count >= 0
                                   ? m_scan_endpoint_norm_min
                                   : 0.0;
  out.scan_endpoint_norm_max = m_scan_lines_cross_outside_ellipse_count >= 0
                                   ? m_scan_endpoint_norm_max
                                   : 0.0;
  out.scan_endpoint_norm_avg =
      m_lines.empty()
          ? 0.0
          : (m_scan_endpoint_norm_min + m_scan_endpoint_norm_max) * 0.5;

  out.accepted_points_outside_ellipse_count =
      m_accepted_points_outside_ellipse_count;
  out.accepted_point_norm_min =
      m_accepted_point_norm_count > 0 ? m_accepted_point_norm_min : 0.0;
  out.accepted_point_norm_max =
      m_accepted_point_norm_count > 0 ? m_accepted_point_norm_max : 0.0;
  out.accepted_point_norm_avg =
      m_accepted_point_norm_count > 0
          ? m_accepted_point_norm_sum / m_accepted_point_norm_count
          : 0.0;

  out.rejected_boundary_band_candidate_count =
      m_rejected_boundary_band_candidate_count;
  out.rejected_boundary_band_norm_min =
      m_rejected_boundary_band_candidate_count > 0
          ? m_rejected_boundary_band_norm_min
          : 0.0;
  out.rejected_boundary_band_norm_max =
      m_rejected_boundary_band_candidate_count > 0
          ? m_rejected_boundary_band_norm_max
          : 0.0;
  out.rejected_boundary_band_norm_avg =
      m_rejected_boundary_band_candidate_count > 0
          ? m_rejected_boundary_band_norm_sum /
                static_cast<double>(m_rejected_boundary_band_candidate_count)
          : 0.0;

  out.scan_geometry_policy = m_scan_geometry_policy;
  out.point_consistency_enabled = m_point_consistency_enabled;
  out.point_consistency_range = m_point_consistency_range;
  out.point_consistency_input_points = m_point_consistency_input_points;
  out.point_consistency_output_points = m_point_consistency_output_points;
  out.point_consistency_removed_points = m_point_consistency_removed_points;

  return out.has_roi || out.has_measure_points || out.has_fit_ellipse;
}

void FindEllipse::PublishDisplayShapes(ICxShapeSink &sink,
                                       const std::string &owner_ref) const {
  FindEllipseDisplaySnapshot snapshot;
  if (!getdisplaysnapshot(snapshot))
    return;

  if (snapshot.has_roi) {
    auto outer =
        std::make_unique<EllipseShape>(snapshot.center_x, snapshot.center_y,
                                       snapshot.radius_x, snapshot.radius_y);
    outer->setInnerScalePercent(snapshot.inner_scale_percent);

    sink.UpsertShape(owner_ref + ".roi_ellipse", "FindEllipse", owner_ref,
                     "setellipse", "roi", true, false, std::move(outer));

    auto outerScan =
        std::make_unique<EllipseShape>(snapshot.center_x, snapshot.center_y,
                                       snapshot.radius_x, snapshot.radius_y);
    sink.UpsertShape(owner_ref + ".outer_scan_ellipse", "FindEllipse",
                     owner_ref, "", "scan", false, false,
                     std::move(outerScan));

    if (snapshot.has_inner_ellipse) {
      auto innerScan = std::make_unique<EllipseShape>(
          snapshot.center_x, snapshot.center_y, snapshot.inner_radius_x,
          snapshot.inner_radius_y);
      sink.UpsertShape(owner_ref + ".inner_scan_ellipse", "FindEllipse",
                       owner_ref, "", "scan", false, false,
                       std::move(innerScan));
    }
  }

  for (std::size_t i = 0; i < m_lines.size(); ++i) {
    CxShapePoint p0;
    CxShapePoint p1;
    try {
      if (!m_lines[i].exportLine(p0, p1))
        continue;
    } catch (...) {
      continue;
    }

    if (!std::isfinite(p0.x) || !std::isfinite(p0.y) ||
        !std::isfinite(p1.x) || !std::isfinite(p1.y)) {
      continue;
    }

    auto scanLine = std::make_unique<LineShape>();
    scanLine->setline(RoundToInt(p0.x), RoundToInt(p0.y),
                      RoundToInt(p1.x), RoundToInt(p1.y));
    sink.UpsertShape(owner_ref + ".scan_tick_" + std::to_string(i),
                     "FindEllipse", owner_ref, "", "scan_tick", false, false,
                     std::move(scanLine));
  }

  const PointsShape &points =
      const_cast<FindEllipse *>(this)->getresultpoints();
  if (points.size() > 0) {
    auto resultPoints = std::make_unique<PointsShape>();
    for (int i = 0; i < static_cast<int>(points.size()); ++i) {
      resultPoints->addpoint(RoundToInt(points.getx(i)),
                             RoundToInt(points.gety(i)));
    }

    sink.UpsertShape(owner_ref + ".measure_points", "FindEllipse", owner_ref,
                     "", "measure_points", false, true,
                     std::move(resultPoints));
  }

  if (snapshot.has_fit_ellipse) {
    auto fitEllipse = std::make_unique<EllipseShape>(
        snapshot.fit_center_x, snapshot.fit_center_y, snapshot.fit_radius_x,
        snapshot.fit_radius_y, snapshot.fit_angle_deg);

    sink.UpsertShape(owner_ref + ".fit_ellipse", "FindEllipse", owner_ref, "",
                     "result", false, true, std::move(fitEllipse));
  }
}

void FindEllipse::MeasureRobust(Image &image) {
  std::cout << "[DIAG] FindEllipse::MeasureRobust entry: threshold="
            << m_iThreshold << " method=" << m_iMethod
            << " linegap=" << m_iSelectPointGap
            << " min_edge_run_width_px=" << m_min_edge_run_width_px
            << " gamma=" << m_igamarate
            << std::endl;

  m_has_fit_result = false;
  m_fit_avgdist = 0.0;
  m_measurepoints.clear();
  m_scan_diagnostics.clear();
  m_measure_failure_stage.clear();
  m_measure_failure_reason.clear();

  m_scan_candidate_lines = 0;
  m_scan_total_candidates = 0;
  m_rejected_min_edge_run_width_count = 0;
  m_scan_accepted_points_before_gate = 0;
  m_accepted_boundary_ratio_sum = 0.0;
  m_accepted_boundary_ratio_min = 999.0;
  m_accepted_boundary_ratio_max = -999.0;
  m_candidate_policy = "ellipse_robust_scan_buffer_edge_bands";

  m_accepted_points_outside_ellipse_count = 0;
  m_accepted_point_norm_sum = 0.0;
  m_accepted_point_norm_count = 0;
  m_accepted_point_norm_min = 999.0;
  m_accepted_point_norm_max = -999.0;
  m_rejected_boundary_band_candidate_count = 0;
  m_rejected_boundary_band_norm_sum = 0.0;
  m_rejected_boundary_band_norm_min = 999.0;
  m_rejected_boundary_band_norm_max = -999.0;
  m_point_consistency_input_points = 0;
  m_point_consistency_output_points = 0;
  m_point_consistency_removed_points = 0;
  m_debug_prefilter_used = 0;
  m_debug_prefilter_component_count = 0;
  m_debug_prefilter_accepted_count = 0;
  m_debug_prefilter_rejected_count = 0;
  m_debug_prefilter_max_area = 0;
  m_debug_prefilter_max_w = 0;
  m_debug_prefilter_max_h = 0;
  m_debug_prefilter_foreground_before = 0;
  m_debug_prefilter_foreground_after = 0;

  m_ellipse_edge_band_candidates.clear();
  m_ellipse_fit_candidate_sequences.clear();
  m_ellipse_best_sequence_index = -1;
  m_ellipse_feature_graph = EllipseFeatureGraph();

  if (!ImageManager::EnsureAlgorithmRuntimeResources(image.getWidth(),
                                                     image.getHeight())) {
    LogFindEllipseMeasureProbe("robust_measure", "failed",
                               "EnsureAlgorithmRuntimeResources failed");
    return;
  }

  g_pbackimage = ImageManager::GetBackImage(1);

  if (g_pbackimage == nullptr || g_pbackimage == &image ||
      g_pbackimage->getmat().empty()) {
    LogFindEllipseMeasureProbe("robust_measure", "failed",
                               "backimage unavailable");
    return;
  }

  {
    const int isize = ClampSizeToInt(m_lines.size());
    if (isize <= 0) {
      m_measure_failure_stage = "scan_lines_empty";
      m_measure_failure_reason =
          "FindEllipse robust measure has no scan lines; check setellipse/setgap order.";
      LogFindEllipseMeasureProbe(
          "robust_measure", "failed", m_measure_failure_reason);
      return;
    }

    int ilineslen = 0;
    if (isize > 0)
      ilineslen = m_lines[0].getlinesize();

    if (ilineslen <= 0) {
      m_measure_failure_stage = "scan_line_invalid";
      m_measure_failure_reason =
          "FindEllipse robust measure has invalid scan line length.";
      LogFindEllipseMeasureProbe(
          "robust_measure", "failed", m_measure_failure_reason);
      return;
    }

    for (int i = 0; i < isize; i++) {
      m_lines[i].linecopyex(image, *g_pbackimage, 0, i);
    }

    const int seam_tail_rows = std::min(5, isize);
    const int seam_rows_copied =
        CopyEllipseSeamTailRows(g_pbackimage, ilineslen, isize,
                                seam_tail_rows);

    g_pbackimage->setroi(0, 0, ilineslen, isize + seam_tail_rows);
    g_pbackimage->roi_7blur_gap_mud_thre_bw(m_iThreshold, m_igamarate,
                                            m_iSelectPointGap, m_iMethod);
    const int seam_values_merged =
        MergeEllipseSeamTailRows(g_pbackimage, ilineslen, isize,
                                 seam_tail_rows);
    LogFindEllipseMeasureProbe(
        "robust_measure_seam_extension", "finished",
        "circular tail padding rows=" + std::to_string(seam_rows_copied) +
            " merged_values=" + std::to_string(seam_values_merged));

    if ((m_ifindset & 0x01) != 0 && g_pbackfindobject != nullptr) {
      const int prefilter_rows = isize + seam_tail_rows;
      const int filter_min =
          ClampLongLongToInt(static_cast<long long>(m_ifiltermin));
      const int filter_max =
          ClampLongLongToInt(static_cast<long long>(m_ifiltermax));
      m_debug_prefilter_foreground_before =
          CountEllipseForegroundPixels(g_pbackimage->getmat(), ilineslen,
                                       prefilter_rows);

      ApplyEllipseObjectPrefilter(g_pbackfindobject, g_pbackimage, ilineslen,
                                  prefilter_rows, m_ifilterborw, filter_min,
                                  filter_max);

      MergeEllipseSeamTailRows(g_pbackimage, ilineslen, isize,
                               seam_tail_rows);
      m_debug_prefilter_used = 1;
      m_debug_prefilter_component_count =
          g_pbackfindobject->getdebugcomponentcount();
      m_debug_prefilter_accepted_count =
          g_pbackfindobject->getdebugacceptedcount();
      m_debug_prefilter_rejected_count =
          g_pbackfindobject->getdebugrejectedcount();
      m_debug_prefilter_max_area =
          g_pbackfindobject->getdebugmaxcomponentarea();
      m_debug_prefilter_max_w =
          g_pbackfindobject->getdebugmaxcomponentw();
      m_debug_prefilter_max_h =
          g_pbackfindobject->getdebugmaxcomponenth();
      m_debug_prefilter_foreground_after =
          CountEllipseForegroundPixels(g_pbackimage->getmat(), ilineslen,
                                       prefilter_rows);

      LogFindEllipseMeasureProbe(
          "robust_measure_object_prefilter", "finished",
          "ifindset=" + std::to_string(m_ifindset) +
              " filter=" + std::to_string(m_ifilterborw) +
              " min=" + std::to_string(filter_min) +
              " max=" + std::to_string(filter_max) +
              " components=" +
              std::to_string(m_debug_prefilter_component_count) +
              " accepted=" +
              std::to_string(m_debug_prefilter_accepted_count) +
              " rejected=" +
              std::to_string(m_debug_prefilter_rejected_count) +
              " foreground=" +
              std::to_string(m_debug_prefilter_foreground_before) + "->" +
              std::to_string(m_debug_prefilter_foreground_after));
    }
  }

  CollectEllipseEdgeBandsRobust(image);

  if (m_ellipse_edge_band_candidates.empty()) {
    m_measure_failure_stage = "threshold_no_edge";
    m_measure_failure_reason =
        "FindEllipse robust preprocessing produced no scan-buffer edge bands.";
    LogFindEllipseMeasureProbe("robust_measure", "no_candidates",
                               m_measure_failure_reason);
    return;
  }

  BuildEllipseFeatureGraph();

  if (m_ellipse_feature_graph.nodes.empty()) {
    m_measure_failure_stage = "no_graph";
    m_measure_failure_reason =
        "FindEllipse robust edge bands did not create feature graph nodes.";
    LogFindEllipseMeasureProbe("robust_measure", "no_graph",
                               m_measure_failure_reason);
    return;
  }

  FindEllipseComponentsInGraph();

  SelectBestEllipseSequence();

  if (m_ellipse_best_sequence_index < 0 ||
      m_ellipse_best_sequence_index >=
          static_cast<int>(m_ellipse_fit_candidate_sequences.size())) {
    m_measure_failure_stage = "no_sequence";
    m_measure_failure_reason =
        "FindEllipse robust feature graph did not select a valid sequence.";
    LogFindEllipseMeasureProbe("robust_measure", "no_sequence",
                               m_measure_failure_reason);
    return;
  }

  ConvertEllipseCandidatesToMeasurePoints();
  if (m_measurepoints.size() <= 0)
    ConvertEllipseSequenceToMeasurePoints(m_ellipse_best_sequence_index);
  m_scan_accepted_points_before_gate = ClampSizeToInt(m_measurepoints.size());
  m_point_consistency_input_points = ClampSizeToInt(m_measurepoints.size());
  m_point_consistency_output_points = m_point_consistency_input_points;

  LogFindEllipseMeasureProbe(
      "robust_measure", "success",
      "robust measurement completed with " +
          std::to_string(m_measurepoints.size()) +
          " points selected_edge=" + std::to_string(m_iselectedgenum));
}

void FindEllipse::CollectEllipseEdgeBandsRobust(Image &image) {
  m_ellipse_edge_band_candidates.clear();

  if (g_pbackimage == nullptr || g_pbackimage->getmat().empty())
    return;

  cv::Mat gray;
  cv::Mat &img_mat = image.getmat();
  if (img_mat.channels() == 1)
    gray = img_mat.clone();
  else if (img_mat.channels() == 3)
    cv::cvtColor(img_mat, gray, cv::COLOR_BGR2GRAY);
  else
    gray = img_mat;

  cv::Mat binary = g_pbackimage->getmat();

  int w = binary.cols;
  int h = binary.rows;

  if (w <= 0 || h <= 0)
    return;

  int half_width = m_igap;
  if (half_width < 1)
    half_width = 3;

  const double ellipse_cx =
      (static_cast<double>(m_roi_x0) + static_cast<double>(m_roi_x1)) * 0.5;
  const double ellipse_cy =
      (static_cast<double>(m_roi_y0) + static_cast<double>(m_roi_y1)) * 0.5;
  const double ellipse_rx =
      std::max(1.0, std::abs(static_cast<double>(m_roi_x1) -
                             static_cast<double>(m_roi_x0)) *
                        0.5);
  const double ellipse_ry =
      std::max(1.0, std::abs(static_cast<double>(m_roi_y1) -
                             static_cast<double>(m_roi_y0)) *
                        0.5);
  const double inner_candidate_norm_guard =
      ComputeEllipseInnerCandidateNormGuard(m_inner_scale_percent);

  int scan_buffer_out_of_range = 0;
  int short_line_count = 0;

  auto scanBufferForeground = [&](int scan_x, int scan_y) -> bool {
    if (scan_x < 0 || scan_x >= w || scan_y < 0 || scan_y >= h) {
      ++scan_buffer_out_of_range;
      return false;
    }

    if (binary.channels() == 1)
      return binary.at<uchar>(scan_y, scan_x) > 0;

    const int channels = binary.channels();
    const uchar *row = binary.ptr<uchar>(scan_y);
    const uchar *px = row + scan_x * channels;
    for (int channel = 0; channel < channels; ++channel) {
      if (px[channel] > 0)
        return true;
    }
    return false;
  };

  for (int line_idx = 0; line_idx < static_cast<int>(m_lines.size());
       ++line_idx) {
    auto &line = m_lines[line_idx];
    int candidate_index = 0;
    int line_size = line.getlinesize();
    const int scan_line_size = std::min(line_size, w);
    if (scan_line_size <= 0 || line_idx < 0 || line_idx >= h) {
      ++short_line_count;
      continue;
    }

    bool in_foreground = false;
    int seg_start = -1;
    int seg_end = -1;
    const int endpoint_guard =
        ComputeEllipseEndpointGuard(scan_line_size, m_iSelectPointGap);

    auto appendCandidate = [&](int start, int end) {
      const int seg_len = end - start;
      if (seg_len > 0 && seg_len < m_min_edge_run_width_px) {
        ++m_rejected_min_edge_run_width_count;
        auto& diag =
            EnsureFindEllipseScanDiagnostic(m_scan_diagnostics, line_idx);
        diag.min_edge_run_width_px = m_min_edge_run_width_px;
        ++diag.rejected_min_edge_run_width;
        if (!diag.accepted && diag.reject_reason.empty())
          diag.reject_reason = "min_edge_run_width_rejected";
        return;
      }
      if (seg_len < 2)
        return;

      const int seg_center = (start + end) / 2;
      if (seg_center < 0 || seg_center >= line_size)
        return;
      if (endpoint_guard > 0 &&
          (seg_center <= endpoint_guard ||
           seg_center >= scan_line_size - 1 - endpoint_guard))
        return;

      gp_Pnt center_pt = line.getlinepoint(seg_center);
      if (inner_candidate_norm_guard > 0.0) {
        const double norm =
            EllipseNorm(center_pt.X(), center_pt.Y(), ellipse_cx, ellipse_cy,
                        ellipse_rx, ellipse_ry);
        if (norm <= inner_candidate_norm_guard)
          return;
      }

      EllipseEdgeBandCandidate candidate;
      candidate.scan_index = line_idx;
      candidate.candidate_index = candidate_index++;
      candidate.start_param = start;
      candidate.end_param = end;
      candidate.center_param = seg_center;
      candidate.x = center_pt.X();
      candidate.y = center_pt.Y();
      candidate.arc_length = static_cast<double>(seg_len);
      candidate.response_strength = static_cast<double>(seg_len);
      candidate.polarity = 1.0;
      candidate.edge_rank = seg_len >= 5 ? 0 : (seg_len >= 3 ? 1 : 2);
      candidate.valid = true;

      const int denom = line_size > 1 ? line_size : 1;
      const double angle = static_cast<double>(seg_center) * 360.0 /
                           static_cast<double>(denom);
      candidate.profile = ExtractEllipseProfileAt(
          gray, candidate.x, candidate.y, angle, 1.0, 1.0, half_width);

      if (candidate.profile.size() >= 5)
        m_ellipse_edge_band_candidates.push_back(candidate);
    };

    for (int pt_idx = 0; pt_idx < scan_line_size; ++pt_idx) {
      const bool is_foreground = scanBufferForeground(pt_idx, line_idx);

      if (is_foreground && !in_foreground) {
        in_foreground = true;
        seg_start = pt_idx;
      } else if (!is_foreground && in_foreground) {
        in_foreground = false;
        seg_end = pt_idx;
        appendCandidate(seg_start, seg_end);
      }
    }

    if (in_foreground) {
      appendCandidate(seg_start, scan_line_size);
    }

    if (candidate_index > 0) {
      ++m_scan_candidate_lines;
      m_scan_total_candidates += candidate_index;
    }
  }

  {
    std::ostringstream oss;
    oss << "scan_buffer=" << w << "x" << h
        << " scan_lines=" << m_lines.size()
        << " edge_band_candidates=" << m_ellipse_edge_band_candidates.size()
        << " candidate_lines=" << m_scan_candidate_lines
        << " total_candidates=" << m_scan_total_candidates
        << " min_edge_run_width_px=" << m_min_edge_run_width_px
        << " rejected_min_edge_run_width="
        << m_rejected_min_edge_run_width_count
        << " short_lines=" << short_line_count
        << " scan_buffer_oob=" << scan_buffer_out_of_range;
    LogFindEllipseMeasureProbe(
        "robust_collect_edge_bands", "finished", oss.str());
  }
}

void FindEllipse::BuildEllipseFeatureGraph() {
  m_ellipse_feature_graph = EllipseFeatureGraph();

  for (std::size_t i = 0; i < m_ellipse_edge_band_candidates.size(); ++i) {
    EllipseFeatureNode node;
    node.id = static_cast<int>(i);
    node.candidate = m_ellipse_edge_band_candidates[i];
    m_ellipse_feature_graph.nodes.push_back(node);
  }

  if (m_ellipse_feature_graph.nodes.size() < 2)
    return;

  double T_space = 15.0;
  double T_width = 10.0;
  double T_ncc = 0.75;

  for (std::size_t i = 0; i < m_ellipse_feature_graph.nodes.size(); ++i) {
    for (std::size_t j = i + 1; j < m_ellipse_feature_graph.nodes.size(); ++j) {
      const auto &node_a = m_ellipse_feature_graph.nodes[i];
      const auto &node_b = m_ellipse_feature_graph.nodes[j];

      int scan_diff =
          std::abs(node_a.candidate.scan_index - node_b.candidate.scan_index);
      if (scan_diff != 1)
        continue;

      double dx = node_a.candidate.x - node_b.candidate.x;
      double dy = node_a.candidate.y - node_b.candidate.y;
      double spatial_dist = std::sqrt(dx * dx + dy * dy);

      if (spatial_dist > T_space)
        continue;

      double width_diff =
          std::abs(node_a.candidate.arc_length - node_b.candidate.arc_length);
      if (width_diff > T_width)
        continue;

      if (node_a.candidate.polarity != node_b.candidate.polarity)
        continue;

      double ncc = EllipseCalculateNCC(node_a.candidate.profile,
                                       node_b.candidate.profile);
      if (ncc <= T_ncc)
        continue;

      EllipseFeatureEdge edge;
      edge.node_a = static_cast<int>(i);
      edge.node_b = static_cast<int>(j);
      edge.ncc_score = ncc;
      edge.angular_distance = spatial_dist;
      edge.valid = true;

      m_ellipse_feature_graph.edges.push_back(edge);
      m_ellipse_feature_graph.nodes[i].neighbors.push_back(static_cast<int>(j));
      m_ellipse_feature_graph.nodes[j].neighbors.push_back(static_cast<int>(i));
    }
  }
}

void FindEllipse::FindEllipseComponentsInGraph() {
  m_ellipse_feature_graph.next_component_id = 0;

  for (auto &node : m_ellipse_feature_graph.nodes) {
    node.visited = false;
    node.component_id = -1;
  }

  for (std::size_t i = 0; i < m_ellipse_feature_graph.nodes.size(); ++i) {
    if (m_ellipse_feature_graph.nodes[i].visited)
      continue;

    int component_id = m_ellipse_feature_graph.next_component_id++;
    std::vector<int> stack;
    stack.push_back(static_cast<int>(i));

    while (!stack.empty()) {
      int current = stack.back();
      stack.pop_back();

      if (m_ellipse_feature_graph.nodes[current].visited)
        continue;

      m_ellipse_feature_graph.nodes[current].visited = true;
      m_ellipse_feature_graph.nodes[current].component_id = component_id;

      for (int neighbor : m_ellipse_feature_graph.nodes[current].neighbors) {
        if (!m_ellipse_feature_graph.nodes[neighbor].visited) {
          stack.push_back(neighbor);
        }
      }
    }
  }
}

void FindEllipse::SelectBestEllipseSequence() {
  m_ellipse_fit_candidate_sequences.clear();

  if (m_ellipse_feature_graph.nodes.empty())
    return;

  int max_component_id = m_ellipse_feature_graph.next_component_id;
  if (max_component_id <= 0)
    return;

  std::vector<std::vector<int>> component_nodes(max_component_id);

  for (const auto &node : m_ellipse_feature_graph.nodes) {
    if (node.component_id >= 0 && node.component_id < max_component_id) {
      component_nodes[node.component_id].push_back(node.id);
    }
  }

  for (const auto &node_ids : component_nodes) {
    if (node_ids.size() < 2)
      continue;

    EllipseFitCandidateSequence seq;

    std::vector<const EllipseFeatureNode *> sorted_nodes;
    for (int id : node_ids) {
      sorted_nodes.push_back(&m_ellipse_feature_graph.nodes[id]);
    }

    std::sort(sorted_nodes.begin(), sorted_nodes.end(),
              [](const EllipseFeatureNode *a, const EllipseFeatureNode *b) {
                return a->candidate.scan_index < b->candidate.scan_index;
              });

    double total_ncc = 0.0;
    double total_response = 0.0;

    for (const auto *node : sorted_nodes) {
      cv::Point2d pt(node->candidate.x, node->candidate.y);
      seq.points.push_back(pt);
      total_response += node->candidate.response_strength;
    }

    int edge_count = 0;
    for (const auto &edge : m_ellipse_feature_graph.edges) {
      if (edge.valid &&
          std::find(node_ids.begin(), node_ids.end(), edge.node_a) !=
              node_ids.end() &&
          std::find(node_ids.begin(), node_ids.end(), edge.node_b) !=
              node_ids.end()) {
        total_ncc += edge.ncc_score;
        edge_count++;
      }
    }

    seq.node_count = static_cast<int>(sorted_nodes.size());
    seq.avg_ncc = edge_count > 0 ? total_ncc / edge_count : 0.0;
    seq.total_response = total_response;
    seq.score = seq.node_count * 100.0 + seq.avg_ncc * 50.0 + total_response;
    seq.valid = true;

    m_ellipse_fit_candidate_sequences.push_back(seq);
  }

  std::sort(
      m_ellipse_fit_candidate_sequences.begin(),
      m_ellipse_fit_candidate_sequences.end(),
      [](const EllipseFitCandidateSequence &a,
         const EllipseFitCandidateSequence &b) { return a.score > b.score; });

  if (m_ellipse_fit_candidate_sequences.size() > 5) {
    m_ellipse_fit_candidate_sequences.resize(5);
  }

  if (!m_ellipse_fit_candidate_sequences.empty()) {
    m_ellipse_best_sequence_index = 0;
  } else {
    m_ellipse_best_sequence_index = -1;
  }
}

void FindEllipse::ConvertEllipseCandidatesToMeasurePoints() {
  m_measurepoints.clear();

  std::map<int, std::vector<int>> candidate_params_by_scan;
  for (const auto &candidate : m_ellipse_edge_band_candidates) {
    if (!candidate.valid || !std::isfinite(candidate.x) ||
        !std::isfinite(candidate.y))
      continue;
    candidate_params_by_scan[candidate.scan_index].push_back(
        candidate.center_param);
  }

  for (auto &scan_candidates : candidate_params_by_scan) {
    auto &line_candidates = scan_candidates.second;
    std::sort(line_candidates.begin(), line_candidates.end());
    line_candidates.erase(
        std::unique(line_candidates.begin(), line_candidates.end()),
        line_candidates.end());
  }

  const int scan_count = static_cast<int>(m_lines.size());
  const double ellipse_cx =
      (static_cast<double>(m_roi_x0) + static_cast<double>(m_roi_x1)) * 0.5;
  const double ellipse_cy =
      (static_cast<double>(m_roi_y0) + static_cast<double>(m_roi_y1)) * 0.5;
  const double ellipse_rx =
      std::max(1.0, std::abs(static_cast<double>(m_roi_x1) -
                             static_cast<double>(m_roi_x0)) *
                        0.5);
  const double ellipse_ry =
      std::max(1.0, std::abs(static_cast<double>(m_roi_y1) -
                             static_cast<double>(m_roi_y0)) *
                        0.5);
  const double inner_candidate_norm_guard =
      ComputeEllipseInnerCandidateNormGuard(m_inner_scale_percent);
  auto candidatePointAllowed = [&](int scan_index, int param) -> bool {
    if (scan_index < 0 || scan_index >= scan_count)
      return false;
    if (param < 0)
      return false;
    if (inner_candidate_norm_guard <= 0.0)
      return true;

    auto &line = m_lines[static_cast<std::size_t>(scan_index)];
    if (param >= line.getlinesize())
      return false;

    const gp_Pnt point = line.getlinepoint(param);
    const double norm =
        EllipseNorm(point.X(), point.Y(), ellipse_cx, ellipse_cy, ellipse_rx,
                    ellipse_ry);
    return norm > inner_candidate_norm_guard;
  };

  if (m_iselectedgenum == 0 && scan_count >= 4) {
    auto wrapIndex = [&](int index) -> int {
      int wrapped = index % scan_count;
      if (wrapped < 0)
        wrapped += scan_count;
      return wrapped;
    };
    auto paramsAt = [&](int index) -> const std::vector<int> * {
      const auto found = candidate_params_by_scan.find(wrapIndex(index));
      return found == candidate_params_by_scan.end() ? nullptr
                                                     : &found->second;
    };
    auto sampleParam = [](const std::vector<int> &params, int index,
                          int expected_count) -> int {
      if (params.empty())
        return -1;
      if (params.size() == 1 || expected_count <= 1)
        return params.front();
      const double t =
          static_cast<double>(index) / static_cast<double>(expected_count - 1);
      const int mapped = static_cast<int>(
          std::lround(t * static_cast<double>(params.size() - 1)));
      return params[static_cast<std::size_t>(
          std::max(0, std::min(mapped, static_cast<int>(params.size()) - 1)))];
    };
    auto hasNearbyParam = [](const std::vector<int> &params, int param) {
      for (int existing : params) {
        if (std::abs(existing - param) <= 2)
          return true;
      }
      return false;
    };
    auto repairSeamScan = [&](int scan_index) {
      std::vector<int> &current =
          candidate_params_by_scan[wrapIndex(scan_index)];

      const int max_search = std::min(8, std::max(2, scan_count / 12));
      const std::vector<int> *left = nullptr;
      const std::vector<int> *right = nullptr;
      for (int offset = 1; offset <= max_search && left == nullptr; ++offset) {
        const std::vector<int> *candidate = paramsAt(scan_index - offset);
        if (candidate != nullptr && !candidate->empty())
          left = candidate;
      }
      for (int offset = 1; offset <= max_search && right == nullptr; ++offset) {
        const std::vector<int> *candidate = paramsAt(scan_index + offset);
        if (candidate != nullptr && !candidate->empty())
          right = candidate;
      }
      if (left == nullptr && right == nullptr)
        return;

      const int expected_count = std::max(
          static_cast<int>(left != nullptr ? left->size() : 0),
          static_cast<int>(right != nullptr ? right->size() : 0));
      if (expected_count <= static_cast<int>(current.size()))
        return;

      const int line_size =
          m_lines[static_cast<std::size_t>(wrapIndex(scan_index))].getlinesize();
      if (line_size <= 0)
        return;
      const int endpoint_guard =
          ComputeEllipseEndpointGuard(line_size, m_iSelectPointGap);
      for (int index = 0; index < expected_count; ++index) {
        int sum = 0;
        int count = 0;
        if (left != nullptr) {
          const int param = sampleParam(*left, index, expected_count);
          if (param >= 0) {
            sum += param;
            ++count;
          }
        }
        if (right != nullptr) {
          const int param = sampleParam(*right, index, expected_count);
          if (param >= 0) {
            sum += param;
            ++count;
          }
        }
        if (count <= 0)
          continue;

        const int repaired_param =
            std::max(0, std::min(line_size - 1, sum / count));
        if (endpoint_guard > 0 &&
            (repaired_param <= endpoint_guard ||
             repaired_param >= line_size - 1 - endpoint_guard))
          continue;
        if (!candidatePointAllowed(wrapIndex(scan_index), repaired_param))
          continue;
        if (!hasNearbyParam(current, repaired_param))
          current.push_back(repaired_param);
      }

      std::sort(current.begin(), current.end());
      current.erase(std::unique(current.begin(), current.end()),
                    current.end());
    };

    const int seam_window = std::min(5, std::max(2, scan_count / 16));
    for (int offset = 0; offset < seam_window; ++offset) {
      repairSeamScan(offset);
      repairSeamScan(scan_count - 1 - offset);
    }
  }

  for (auto &scan_candidates : candidate_params_by_scan) {
    const int scan_index = scan_candidates.first;
    if (scan_index < 0 || scan_index >= scan_count)
      continue;

    auto &line_candidates = scan_candidates.second;
    std::sort(line_candidates.begin(), line_candidates.end());

    if (m_iselectedgenum == 0) {
      const int candidate_count = static_cast<int>(line_candidates.size());
      for (const int param : line_candidates) {
        if (!candidatePointAllowed(scan_index, param))
          continue;
        gp_Pnt point =
            m_lines[static_cast<std::size_t>(scan_index)].getlinepoint(param);
        m_measurepoints.addpoint(point);
        RecordFindEllipseAcceptedDiagnosticPoint(
            m_scan_diagnostics, scan_index, candidate_count, param,
            m_min_edge_run_width_px, point);
      }
      continue;
    }

    int selected_param = -1;
    if (m_iselectedgenum < 0) {
      selected_param =
          line_candidates.empty() ? -1 : line_candidates.back();
    } else if (m_iselectedgenum <= static_cast<int>(line_candidates.size())) {
      selected_param =
          line_candidates[static_cast<std::size_t>(m_iselectedgenum - 1)];
    }

    if (selected_param >= 0) {
      if (!candidatePointAllowed(scan_index, selected_param))
        continue;
      gp_Pnt point =
          m_lines[static_cast<std::size_t>(scan_index)]
              .getlinepoint(selected_param);
      m_measurepoints.addpoint(point);
      RecordFindEllipseAcceptedDiagnosticPoint(
          m_scan_diagnostics, scan_index,
          static_cast<int>(line_candidates.size()), selected_param,
          m_min_edge_run_width_px, point);
    }
  }

  m_accepted_point_norm_sum = 0.0;
  m_accepted_point_norm_count = 0;
  m_accepted_point_norm_min = 999.0;
  m_accepted_point_norm_max = -999.0;
  m_accepted_points_outside_ellipse_count = 0;
  m_accepted_boundary_ratio_sum = 0.0;
  m_accepted_boundary_ratio_min = 999.0;
  m_accepted_boundary_ratio_max = -999.0;

  for (int index = 0; index < static_cast<int>(m_measurepoints.size());
       ++index) {
    const double norm =
        EllipseNorm(m_measurepoints.getx(index), m_measurepoints.gety(index),
                    ellipse_cx, ellipse_cy, ellipse_rx, ellipse_ry);
    if (!std::isfinite(norm))
      continue;

    m_accepted_point_norm_sum += norm;
    ++m_accepted_point_norm_count;
    m_accepted_point_norm_min = std::min(m_accepted_point_norm_min, norm);
    m_accepted_point_norm_max = std::max(m_accepted_point_norm_max, norm);
    if (norm > 1.05)
      ++m_accepted_points_outside_ellipse_count;

    m_accepted_boundary_ratio_sum += norm;
    m_accepted_boundary_ratio_min =
        std::min(m_accepted_boundary_ratio_min, norm);
    m_accepted_boundary_ratio_max =
        std::max(m_accepted_boundary_ratio_max, norm);
  }
}

void FindEllipse::ConvertEllipseSequenceToMeasurePoints(int sequence_index) {
  if (sequence_index < 0 ||
      sequence_index >=
          static_cast<int>(m_ellipse_fit_candidate_sequences.size()))
    return;

  const auto &seq = m_ellipse_fit_candidate_sequences[sequence_index];
  m_measurepoints.clear();
  m_scan_diagnostics.clear();

  for (const auto &pt : seq.points) {
    gp_Pnt point(pt.x, pt.y, 0.0);
    m_measurepoints.addpoint(point);

    int best_scan_index = -1;
    int best_position = -1;
    double best_distance2 = std::numeric_limits<double>::infinity();
    for (int scan_index = 0; scan_index < static_cast<int>(m_lines.size());
         ++scan_index) {
      CxShapePoint a;
      CxShapePoint b;
      if (!m_lines[static_cast<std::size_t>(scan_index)].exportLine(a, b))
        continue;

      const double dx = b.x - a.x;
      const double dy = b.y - a.y;
      const double len2 = dx * dx + dy * dy;
      if (len2 <= 1.0e-9)
        continue;

      double t = ((pt.x - a.x) * dx + (pt.y - a.y) * dy) / len2;
      t = std::max(0.0, std::min(1.0, t));
      const double px = a.x + dx * t;
      const double py = a.y + dy * t;
      const double dist2 =
          (pt.x - px) * (pt.x - px) + (pt.y - py) * (pt.y - py);
      if (dist2 >= best_distance2)
        continue;

      const int line_size =
          m_lines[static_cast<std::size_t>(scan_index)].getlinesize();
      best_distance2 = dist2;
      best_scan_index = scan_index;
      best_position = std::max(
          0, std::min(std::max(0, line_size - 1),
                      static_cast<int>(std::lround(
                          t * static_cast<double>(std::max(0, line_size - 1))))));
    }

    if (best_scan_index >= 0) {
      RecordFindEllipseAcceptedDiagnosticPoint(m_scan_diagnostics,
                                               best_scan_index, 1,
                                               best_position,
                                               m_min_edge_run_width_px,
                                               point);
    }
  }
}

int FindEllipse::getscandiagnosticcount() const {
  return static_cast<int>(m_scan_diagnostics.size());
}

bool FindEllipse::getscandiagnostic(
    int index, FindEllipseMeasureGeometryDebug::ScanDiagnostic &out) const {
  if (index < 0 || index >= static_cast<int>(m_scan_diagnostics.size()))
    return false;
  out = m_scan_diagnostics[static_cast<std::size_t>(index)];
  return true;
}

bool FindEllipse::getscandiagnosticline(int scan_index, CxShapePoint &p0,
                                        CxShapePoint &p1) const {
  return getscanline(scan_index, p0, p1);
}

int FindEllipse::getscanlinecount() const {
  return static_cast<int>(m_lines.size());
}

bool FindEllipse::getscanline(int scan_index, CxShapePoint &p0,
                              CxShapePoint &p1) const {
  if (scan_index < 0 || scan_index >= static_cast<int>(m_lines.size()))
    return false;
  return m_lines[static_cast<std::size_t>(scan_index)].exportLine(p0, p1);
}
