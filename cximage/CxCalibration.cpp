#include "pch.h"
#include "CxCalibration.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>

namespace
{
constexpr double kCxCalibrationPi = 3.141592653589793238462643383279502884;

double DegToRad(double deg)
{
    return deg * kCxCalibrationPi / 180.0;
}

std::string NormalizedUnit(const std::string& unit)
{
    return unit.empty() ? "pixel" : unit;
}

std::uint64_t Fnv1aAppend(std::uint64_t hash, const std::string& text)
{
    constexpr std::uint64_t kPrime = 1099511628211ull;
    for (unsigned char c : text)
    {
        hash ^= static_cast<std::uint64_t>(c);
        hash *= kPrime;
    }
    return hash;
}

std::string StableNumber(double value)
{
    std::ostringstream out;
    out << std::setprecision(17) << value;
    return out.str();
}

std::string SnapshotHash(const CxCalibrationSnapshot& s)
{
    std::uint64_t h = 1469598103934665603ull;
    h = Fnv1aAppend(h, s.calibration_id);
    h = Fnv1aAppend(h, s.device_id);
    h = Fnv1aAppend(h, s.lens_id);
    h = Fnv1aAppend(h, s.magnification_id);
    h = Fnv1aAppend(h, s.coordinate_frame_id);
    h = Fnv1aAppend(h, s.source_ref);
    h = Fnv1aAppend(h, s.valid_from);
    h = Fnv1aAppend(h, s.valid_to);
    h = Fnv1aAppend(h, s.x_unit);
    h = Fnv1aAppend(h, s.y_unit);
    h = Fnv1aAppend(h, s.z_unit);
    h = Fnv1aAppend(h, StableNumber(s.xy_transform.scale_x));
    h = Fnv1aAppend(h, StableNumber(s.xy_transform.scale_y));
    h = Fnv1aAppend(h, StableNumber(s.xy_transform.offset_x));
    h = Fnv1aAppend(h, StableNumber(s.xy_transform.offset_y));
    h = Fnv1aAppend(h, StableNumber(s.xy_transform.rotation_deg));
    h = Fnv1aAppend(h, StableNumber(s.xy_transform.shear_x));
    h = Fnv1aAppend(h, StableNumber(s.xy_transform.shear_y));
    h = Fnv1aAppend(h, StableNumber(s.z_transform.z_scale));
    h = Fnv1aAppend(h, StableNumber(s.z_transform.z_offset));
    h = Fnv1aAppend(h, StableNumber(s.uncertainty.xy_scale_uncertainty));
    h = Fnv1aAppend(h, StableNumber(s.uncertainty.xy_repeatability_std));
    h = Fnv1aAppend(h, StableNumber(s.uncertainty.z_scale_uncertainty));
    h = Fnv1aAppend(h, StableNumber(s.uncertainty.z_repeatability_std));
    h = Fnv1aAppend(h, StableNumber(s.uncertainty.z_linearity_error));
    h = Fnv1aAppend(h, StableNumber(s.reprojection_rmse_px));
    h = Fnv1aAppend(h, s.has_reprojection_rmse ? "1" : "0");

    std::ostringstream out;
    out << "fnv1a64:" << std::hex << std::setw(16) << std::setfill('0') << h;
    return out.str();
}

CxCalibrationUncertaintyStatus ClassifyUncertainty(const CxCalibrationSnapshot& s)
{
    if (!s.has_uncertainty)
        return CxCalibrationUncertaintyStatus::Missing;
    if (s.uncertainty.has_xy_uncertainty && s.uncertainty.has_z_uncertainty)
        return CxCalibrationUncertaintyStatus::Complete;
    if (s.uncertainty.has_xy_uncertainty || s.uncertainty.has_z_uncertainty)
        return CxCalibrationUncertaintyStatus::Incomplete;
    return CxCalibrationUncertaintyStatus::Missing;
}
}

CxCalibration::CxCalibration()
{
    reset();
}

void CxCalibration::reset()
{
    m_snapshot = CxCalibrationSnapshot();
    m_snapshot.snapshot_hash = SnapshotHash(m_snapshot);
    m_status_code = CxCalibrationStatusCode::Empty;
}

void CxCalibration::setmetadata(
    const std::string& calibration_id,
    const std::string& device_id,
    const std::string& lens_id,
    const std::string& magnification_id)
{
    m_snapshot.calibration_id = calibration_id;
    m_snapshot.device_id = device_id;
    m_snapshot.lens_id = lens_id;
    m_snapshot.magnification_id = magnification_id;
    updateSnapshotHash();
}

void CxCalibration::setvalidity(const std::string& valid_from, const std::string& valid_to)
{
    m_snapshot.valid_from = valid_from;
    m_snapshot.valid_to = valid_to;
    updateSnapshotHash();
}

void CxCalibration::setcoordinateframe(const std::string& coordinate_frame_id)
{
    m_snapshot.coordinate_frame_id = coordinate_frame_id.empty() ? "image_pixel" : coordinate_frame_id;
    updateSnapshotHash();
}

void CxCalibration::setsource(const std::string& source_ref)
{
    m_snapshot.source_ref = source_ref;
    m_snapshot.source = source_ref;
    markReadyIfPossible();
}

void CxCalibration::setunits(
    const std::string& x_unit,
    const std::string& y_unit,
    const std::string& z_unit)
{
    m_snapshot.x_unit = NormalizedUnit(x_unit);
    m_snapshot.y_unit = NormalizedUnit(y_unit);
    m_snapshot.z_unit = NormalizedUnit(z_unit);
    m_snapshot.xy_unit =
        m_snapshot.x_unit == m_snapshot.y_unit
            ? m_snapshot.x_unit
            : m_snapshot.x_unit + "/" + m_snapshot.y_unit;
    markReadyIfPossible();
}

void CxCalibration::setxytransform(
    double scale_x,
    double scale_y,
    double offset_x,
    double offset_y,
    double rotation_deg)
{
    setxytransformex(scale_x, scale_y, offset_x, offset_y, rotation_deg, 0.0, 0.0);
}

void CxCalibration::setxytransformex(
    double scale_x,
    double scale_y,
    double offset_x,
    double offset_y,
    double rotation_deg,
    double shear_x,
    double shear_y)
{
    if (!finite(scale_x) || !finite(scale_y) ||
        !finite(offset_x) || !finite(offset_y) ||
        !finite(rotation_deg) || !finite(shear_x) || !finite(shear_y) ||
        std::abs(scale_x) <= std::numeric_limits<double>::epsilon() ||
        std::abs(scale_y) <= std::numeric_limits<double>::epsilon())
    {
        m_snapshot.xy_transform = CxCalibrationTransform2D();
        m_snapshot.has_xy_transform = false;
        m_snapshot.status = "INVALID_INPUT";
        m_snapshot.reason = "invalid xy calibration transform";
        m_status_code = CxCalibrationStatusCode::InvalidInput;
        updateSnapshotHash();
        return;
    }

    m_snapshot.xy_transform.scale_x = scale_x;
    m_snapshot.xy_transform.scale_y = scale_y;
    m_snapshot.xy_transform.offset_x = offset_x;
    m_snapshot.xy_transform.offset_y = offset_y;
    m_snapshot.xy_transform.rotation_deg = rotation_deg;
    m_snapshot.xy_transform.shear_x = shear_x;
    m_snapshot.xy_transform.shear_y = shear_y;
    m_snapshot.xy_transform.valid = true;

    m_snapshot.global_scale_x = scale_x;
    m_snapshot.global_scale_y = scale_y;
    m_snapshot.global_offset_x = offset_x;
    m_snapshot.global_offset_y = offset_y;
    m_snapshot.global_rotation = rotation_deg;
    m_snapshot.has_xy_transform = true;
    m_status_code = CxCalibrationStatusCode::Empty;
    markReadyIfPossible();
}

void CxCalibration::setzscale(double scale, double offset)
{
    if (!finite(scale) || !finite(offset) ||
        std::abs(scale) <= std::numeric_limits<double>::epsilon())
    {
        m_snapshot.z_transform = CxCalibrationZTransform();
        m_snapshot.z_scale = 1.0;
        m_snapshot.z_offset = 0.0;
        m_snapshot.has_z_scale = false;
        m_snapshot.status = "INVALID_INPUT";
        m_snapshot.reason = "invalid z calibration scale";
        m_status_code = CxCalibrationStatusCode::InvalidInput;
        updateSnapshotHash();
        return;
    }

    m_snapshot.z_transform.z_scale = scale;
    m_snapshot.z_transform.z_offset = offset;
    m_snapshot.z_transform.valid = true;
    m_snapshot.z_scale = scale;
    m_snapshot.z_offset = offset;
    m_snapshot.has_z_scale = true;
    m_status_code = CxCalibrationStatusCode::Empty;
    markReadyIfPossible();
}

void CxCalibration::setuncertainty(
    double xy_scale_uncertainty,
    double xy_repeatability_std,
    double z_scale_uncertainty,
    double z_repeatability_std,
    double z_linearity_error)
{
    if (!validateUncertainty(xy_scale_uncertainty) ||
        !validateUncertainty(xy_repeatability_std) ||
        !validateUncertainty(z_scale_uncertainty) ||
        !validateUncertainty(z_repeatability_std) ||
        !validateUncertainty(z_linearity_error))
    {
        m_snapshot.has_uncertainty = false;
        m_snapshot.uncertainty = CxCalibrationUncertainty();
        m_snapshot.uncertainty.status = CxCalibrationUncertaintyStatus::Invalid;
        m_snapshot.status = "INVALID_INPUT";
        m_snapshot.reason = "invalid non-negative uncertainty value";
        m_status_code = CxCalibrationStatusCode::InvalidInput;
        updateSnapshotHash();
        return;
    }

    m_snapshot.uncertainty.xy_scale_uncertainty = xy_scale_uncertainty;
    m_snapshot.uncertainty.xy_repeatability_std = xy_repeatability_std;
    m_snapshot.uncertainty.z_scale_uncertainty = z_scale_uncertainty;
    m_snapshot.uncertainty.z_repeatability_std = z_repeatability_std;
    m_snapshot.uncertainty.z_linearity_error = z_linearity_error;
    m_snapshot.uncertainty.has_xy_uncertainty =
        xy_scale_uncertainty > 0.0 || xy_repeatability_std > 0.0;
    m_snapshot.uncertainty.has_z_uncertainty =
        z_scale_uncertainty > 0.0 || z_repeatability_std > 0.0 || z_linearity_error > 0.0;
    m_snapshot.has_uncertainty =
        m_snapshot.uncertainty.has_xy_uncertainty || m_snapshot.uncertainty.has_z_uncertainty;
    m_snapshot.uncertainty.status = ClassifyUncertainty(m_snapshot);
    m_status_code = CxCalibrationStatusCode::Empty;
    markReadyIfPossible();
}

void CxCalibration::setreprojectionrmse(double reprojection_rmse_px)
{
    if (!finite(reprojection_rmse_px) || reprojection_rmse_px < 0.0)
    {
        m_snapshot.reprojection_rmse_px = -1.0;
        m_snapshot.has_reprojection_rmse = false;
        m_snapshot.status = "INVALID_INPUT";
        m_snapshot.reason = "invalid reprojection RMSE";
        m_status_code = CxCalibrationStatusCode::InvalidInput;
        updateSnapshotHash();
        return;
    }

    m_snapshot.reprojection_rmse_px = reprojection_rmse_px;
    m_snapshot.has_reprojection_rmse = true;
    m_status_code = CxCalibrationStatusCode::Empty;
    markReadyIfPossible();
}

bool CxCalibration::hasxytransform() const
{
    return m_snapshot.has_xy_transform;
}

bool CxCalibration::haszscale() const
{
    return m_snapshot.has_z_scale;
}

bool CxCalibration::hasuncertainty() const
{
    return m_snapshot.has_uncertainty;
}

cv::Point2d CxCalibration::pixelToPhysical(double x, double y) const
{
    const CxCalibrationPoint2D p = pixelToPhysicalPoint(x, y);
    return cv::Point2d(p.x, p.y);
}

cv::Point2d CxCalibration::physicalToPixel(double x, double y) const
{
    const CxCalibrationPoint2D p = physicalToPixelPoint(x, y);
    return cv::Point2d(p.x, p.y);
}

CxCalibrationPoint2D CxCalibration::pixelToPhysicalPoint(double x, double y) const
{
    if (!m_snapshot.has_xy_transform)
        return { x, y };

    const auto& t = m_snapshot.xy_transform;
    double sx = x * t.scale_x;
    double sy = y * t.scale_y;

    if (t.shear_x != 0.0 || t.shear_y != 0.0)
    {
        const double sheared_x = sx + t.shear_x * sy;
        const double sheared_y = sy + t.shear_y * sx;
        sx = sheared_x;
        sy = sheared_y;
    }

    const double a = DegToRad(t.rotation_deg);
    const double ca = std::cos(a);
    const double sa = std::sin(a);
    return {
        sx * ca - sy * sa + t.offset_x,
        sx * sa + sy * ca + t.offset_y
    };
}

CxCalibrationPoint2D CxCalibration::physicalToPixelPoint(double x, double y) const
{
    if (!m_snapshot.has_xy_transform)
        return { x, y };

    const auto& t = m_snapshot.xy_transform;
    const double px = x - t.offset_x;
    const double py = y - t.offset_y;
    const double a = DegToRad(t.rotation_deg);
    const double ca = std::cos(a);
    const double sa = std::sin(a);

    double sx = px * ca + py * sa;
    double sy = -px * sa + py * ca;

    if (t.shear_x != 0.0 || t.shear_y != 0.0)
    {
        const double det = 1.0 - t.shear_x * t.shear_y;
        if (std::abs(det) <= std::numeric_limits<double>::epsilon())
            return { std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN() };
        const double unsheared_x = (sx - t.shear_x * sy) / det;
        const double unsheared_y = (-t.shear_y * sx + sy) / det;
        sx = unsheared_x;
        sy = unsheared_y;
    }

    return { sx / t.scale_x, sy / t.scale_y };
}

CxCalibrationPoint3D CxCalibration::pixelToPhysicalPoint3D(double x, double y, double z) const
{
    const CxCalibrationPoint2D xy = pixelToPhysicalPoint(x, y);
    return { xy.x, xy.y, zToPhysical(z) };
}

double CxCalibration::zToPhysical(double z) const
{
    if (!m_snapshot.has_z_scale)
        return z;
    return z * m_snapshot.z_transform.z_scale + m_snapshot.z_transform.z_offset;
}

CxCalibrationTransformTrace CxCalibration::tracePixelToPhysical(double x, double y) const
{
    CxCalibrationTransformTrace trace;
    trace.input = { x, y, 0.0 };
    const CxCalibrationPoint2D out = pixelToPhysicalPoint(x, y);
    trace.output = { out.x, out.y, 0.0 };
    trace.applied_xy = m_snapshot.has_xy_transform;
    trace.applied_z = false;
    trace.has_z = false;
    trace.status = m_snapshot.status;
    trace.reason = m_snapshot.has_xy_transform
        ? "xy transform applied"
        : "xy transform unavailable; pixel coordinate preserved";
    return trace;
}

CxCalibrationTransformTrace CxCalibration::tracePixelToPhysical3D(double x, double y, double z) const
{
    CxCalibrationTransformTrace trace;
    trace.input = { x, y, z };
    trace.output = pixelToPhysicalPoint3D(x, y, z);
    trace.applied_xy = m_snapshot.has_xy_transform;
    trace.applied_z = m_snapshot.has_z_scale;
    trace.has_z = true;
    trace.status = m_snapshot.status;
    if (trace.applied_xy && trace.applied_z)
        trace.reason = "xy and z transforms applied";
    else if (trace.applied_xy)
        trace.reason = "xy transform applied; z scale unavailable";
    else if (trace.applied_z)
        trace.reason = "z transform applied; xy transform unavailable";
    else
        trace.reason = "calibration transform unavailable; input coordinate preserved";
    return trace;
}

double CxCalibration::propagateLinearUncertainty(
    const std::vector<double>& sensitivities,
    const std::vector<double>& uncertainties) const
{
    if (sensitivities.size() != uncertainties.size() || sensitivities.empty())
        return std::numeric_limits<double>::quiet_NaN();

    double variance = 0.0;
    for (std::size_t i = 0; i < sensitivities.size(); ++i)
    {
        if (!validateUncertainty(std::abs(uncertainties[i])) || !finite(sensitivities[i]))
            return std::numeric_limits<double>::quiet_NaN();
        const double term = sensitivities[i] * uncertainties[i];
        variance += term * term;
    }
    return std::sqrt(variance);
}

CxCalibrationSnapshot CxCalibration::snapshot() const
{
    return m_snapshot;
}

CxCalibrationStatusCode CxCalibration::status_code() const
{
    return m_status_code;
}

CxCalibrationUncertaintyStatus CxCalibration::uncertainty_status() const
{
    return m_snapshot.uncertainty.status;
}

const std::string& CxCalibration::status() const
{
    return m_snapshot.status;
}

const std::string& CxCalibration::reason() const
{
    return m_snapshot.reason;
}

void CxCalibration::markReadyIfPossible()
{
    if (m_status_code == CxCalibrationStatusCode::InvalidInput)
    {
        updateSnapshotHash();
        return;
    }

    if (m_snapshot.has_xy_transform || m_snapshot.has_z_scale || m_snapshot.has_uncertainty)
    {
        m_snapshot.status = "READY";
        m_snapshot.reason = "calibration snapshot available";
        m_status_code = CxCalibrationStatusCode::Ready;
    }
    else
    {
        m_snapshot.status = "EMPTY";
        m_snapshot.reason = "calibration not initialized";
        m_status_code = CxCalibrationStatusCode::Empty;
    }
    updateSnapshotHash();
}

void CxCalibration::updateSnapshotHash()
{
    m_snapshot.snapshot_hash.clear();
    m_snapshot.snapshot_hash = SnapshotHash(m_snapshot);
}

bool CxCalibration::finite(double v) const
{
    return std::isfinite(v);
}

bool CxCalibration::validateUncertainty(double v) const
{
    return finite(v) && v >= 0.0;
}

CxCalibrationAdapter::CxCalibrationAdapter()
{
    reset();
}

CxCalibrationAdapter::CxCalibrationAdapter(const CxCalibrationSnapshot& snapshot)
{
    bind(snapshot);
}

void CxCalibrationAdapter::reset()
{
    m_calibration.reset();
    m_bound_snapshot = m_calibration.snapshot();
}

bool CxCalibrationAdapter::bind(const CxCalibrationSnapshot& snapshot)
{
    m_calibration.reset();
    m_calibration.setmetadata(
        snapshot.calibration_id,
        snapshot.device_id,
        snapshot.lens_id,
        snapshot.magnification_id);
    m_calibration.setvalidity(snapshot.valid_from, snapshot.valid_to);
    m_calibration.setcoordinateframe(snapshot.coordinate_frame_id);
    m_calibration.setsource(snapshot.source_ref.empty() ? snapshot.source : snapshot.source_ref);
    m_calibration.setunits(snapshot.x_unit, snapshot.y_unit, snapshot.z_unit);

    if (snapshot.has_xy_transform)
    {
        m_calibration.setxytransformex(
            snapshot.xy_transform.scale_x,
            snapshot.xy_transform.scale_y,
            snapshot.xy_transform.offset_x,
            snapshot.xy_transform.offset_y,
            snapshot.xy_transform.rotation_deg,
            snapshot.xy_transform.shear_x,
            snapshot.xy_transform.shear_y);
    }
    if (snapshot.has_z_scale)
        m_calibration.setzscale(snapshot.z_transform.z_scale, snapshot.z_transform.z_offset);
    if (snapshot.has_uncertainty)
    {
        m_calibration.setuncertainty(
            snapshot.uncertainty.xy_scale_uncertainty,
            snapshot.uncertainty.xy_repeatability_std,
            snapshot.uncertainty.z_scale_uncertainty,
            snapshot.uncertainty.z_repeatability_std,
            snapshot.uncertainty.z_linearity_error);
    }
    if (snapshot.has_reprojection_rmse)
        m_calibration.setreprojectionrmse(snapshot.reprojection_rmse_px);
    m_bound_snapshot = m_calibration.snapshot();
    return ready();
}

bool CxCalibrationAdapter::ready() const
{
    return m_calibration.status_code() == CxCalibrationStatusCode::Ready;
}

cv::Point2d CxCalibrationAdapter::pixelToPhysical(double x, double y) const
{
    return m_calibration.pixelToPhysical(x, y);
}

cv::Point2d CxCalibrationAdapter::physicalToPixel(double x, double y) const
{
    return m_calibration.physicalToPixel(x, y);
}

double CxCalibrationAdapter::zToPhysical(double z) const
{
    return m_calibration.zToPhysical(z);
}

CxCalibrationTransformTrace CxCalibrationAdapter::tracePixelToPhysical3D(double x, double y, double z) const
{
    return m_calibration.tracePixelToPhysical3D(x, y, z);
}

const CxCalibrationSnapshot& CxCalibrationAdapter::snapshot() const
{
    return m_bound_snapshot;
}

const std::string& CxCalibrationAdapter::status() const
{
    return m_calibration.status();
}

const std::string& CxCalibrationAdapter::reason() const
{
    return m_calibration.reason();
}

CxCalibrationSnapshot BuildIdentityCalibrationSnapshot(
    const std::string& calibration_id,
    const std::string& unit)
{
    CxCalibration calibration;
    calibration.setmetadata(calibration_id, "", "", "");
    calibration.setsource("identity");
    calibration.setunits(unit, unit, unit);
    calibration.setxytransform(1.0, 1.0, 0.0, 0.0, 0.0);
    calibration.setzscale(1.0, 0.0);
    return calibration.snapshot();
}

CxCalibrationSnapshot BuildCalibrationSnapshot(const CxCalibration& calibration)
{
    return calibration.snapshot();
}

std::vector<std::vector<cv::Point2d>> g_conversionResults;

double calibtest(
    std::vector<std::vector<cv::Point3f>>,
    std::vector<std::vector<cv::Point2d>>,
    cv::Size,
    cv::Mat,
    cv::Mat,
    std::vector<cv::Mat>,
    std::vector<cv::Mat>)
{
    return std::numeric_limits<double>::quiet_NaN();
}

int runCameraCalibration()
{
    return -1;
}

int RunCalibrationGrid()
{
    return -1;
}
