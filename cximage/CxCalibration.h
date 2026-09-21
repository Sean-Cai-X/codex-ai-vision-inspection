#ifndef CXIMAGE_CXCALIBRATION_H
#define CXIMAGE_CXCALIBRATION_H

#include <string>
#include <vector>

#include <opencv2/core.hpp>

// CxCalibration is the lightweight calibration boundary for measurement-aware
// image semantics. It stores value-semantic coordinate/unit/uncertainty facts
// that can be copied into case evidence. It must not own Parser, Image, Shape,
// Find* objects, UI state, or legacy calibration runtime pointers.

enum class CxCalibrationStatusCode
{
    Empty = 0,
    Ready = 1,
    InvalidInput = 2
};

enum class CxCalibrationUncertaintyStatus
{
    Unknown = 0,
    Missing = 1,
    Incomplete = 2,
    Complete = 3,
    Invalid = 4
};

struct CxCalibrationPoint2D
{
    double x = 0.0;
    double y = 0.0;
};

struct CxCalibrationPoint3D
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct CxCalibrationTransform2D
{
    double scale_x = 1.0;
    double scale_y = 1.0;
    double offset_x = 0.0;
    double offset_y = 0.0;
    double rotation_deg = 0.0;
    double shear_x = 0.0;
    double shear_y = 0.0;
    bool valid = false;
};

struct CxCalibrationZTransform
{
    double z_scale = 1.0;
    double z_offset = 0.0;
    bool valid = false;
};

struct CxCalibrationUncertainty
{
    double xy_scale_uncertainty = 0.0;
    double xy_repeatability_std = 0.0;
    double z_scale_uncertainty = 0.0;
    double z_repeatability_std = 0.0;
    double z_linearity_error = 0.0;
    bool has_xy_uncertainty = false;
    bool has_z_uncertainty = false;
    CxCalibrationUncertaintyStatus status = CxCalibrationUncertaintyStatus::Missing;
};

struct CxCalibrationTransformTrace
{
    CxCalibrationPoint3D input;
    CxCalibrationPoint3D output;
    bool has_z = false;
    bool applied_xy = false;
    bool applied_z = false;
    std::string status = "EMPTY";
    std::string reason = "calibration not initialized";
};

struct CxCalibrationSnapshot
{
    std::string calibration_id;
    std::string device_id;
    std::string lens_id;
    std::string magnification_id;
    std::string coordinate_frame_id = "image_pixel";
    std::string source_ref;
    std::string source;
    std::string valid_from;
    std::string valid_to;

    CxCalibrationTransform2D xy_transform;
    CxCalibrationZTransform z_transform;
    CxCalibrationUncertainty uncertainty;

    double global_scale_x = 1.0;
    double global_scale_y = 1.0;
    double global_offset_x = 0.0;
    double global_offset_y = 0.0;
    double global_rotation = 0.0;

    double z_scale = 1.0;
    double z_offset = 0.0;

    bool has_xy_transform = false;
    bool has_z_scale = false;
    bool has_uncertainty = false;
    // Camera/grid calibration quality, expressed in image pixels.  A negative
    // value means the receipt has no independently measured reprojection RMSE.
    double reprojection_rmse_px = -1.0;
    bool has_reprojection_rmse = false;

    std::string x_unit = "pixel";
    std::string y_unit = "pixel";
    std::string z_unit = "pixel";
    std::string xy_unit = "pixel";

    std::string status = "EMPTY";
    std::string reason = "calibration not initialized";
    std::string snapshot_hash;
};

class CxCalibration
{
public:
    CxCalibration();

    void reset();

    void setmetadata(const std::string& calibration_id,
                     const std::string& device_id,
                     const std::string& lens_id,
                     const std::string& magnification_id);
    void setvalidity(const std::string& valid_from, const std::string& valid_to);
    void setcoordinateframe(const std::string& coordinate_frame_id);
    void setsource(const std::string& source_ref);
    void setunits(const std::string& x_unit,
                  const std::string& y_unit,
                  const std::string& z_unit);

    void setxytransform(double scale_x,
                        double scale_y,
                        double offset_x,
                        double offset_y,
                        double rotation_deg);
    void setxytransformex(double scale_x,
                          double scale_y,
                          double offset_x,
                          double offset_y,
                          double rotation_deg,
                          double shear_x,
                          double shear_y);
    void setzscale(double scale, double offset);
    void setuncertainty(double xy_scale_uncertainty,
                        double xy_repeatability_std,
                        double z_scale_uncertainty,
                        double z_repeatability_std,
                        double z_linearity_error);
    void setreprojectionrmse(double reprojection_rmse_px);

    bool hasxytransform() const;
    bool haszscale() const;
    bool hasuncertainty() const;

    cv::Point2d pixelToPhysical(double x, double y) const;
    cv::Point2d physicalToPixel(double x, double y) const;
    CxCalibrationPoint2D pixelToPhysicalPoint(double x, double y) const;
    CxCalibrationPoint2D physicalToPixelPoint(double x, double y) const;
    CxCalibrationPoint3D pixelToPhysicalPoint3D(double x, double y, double z) const;
    double zToPhysical(double z) const;

    CxCalibrationTransformTrace tracePixelToPhysical(double x, double y) const;
    CxCalibrationTransformTrace tracePixelToPhysical3D(double x, double y, double z) const;
    double propagateLinearUncertainty(const std::vector<double>& sensitivities,
                                      const std::vector<double>& uncertainties) const;

    CxCalibrationSnapshot snapshot() const;
    CxCalibrationStatusCode status_code() const;
    CxCalibrationUncertaintyStatus uncertainty_status() const;
    const std::string& status() const;
    const std::string& reason() const;

private:
    void markReadyIfPossible();
    void updateSnapshotHash();
    bool finite(double v) const;
    bool validateUncertainty(double v) const;

    CxCalibrationSnapshot m_snapshot;
    CxCalibrationStatusCode m_status_code = CxCalibrationStatusCode::Empty;
};

class CxCalibrationAdapter
{
public:
    CxCalibrationAdapter();
    explicit CxCalibrationAdapter(const CxCalibrationSnapshot& snapshot);

    void reset();
    bool bind(const CxCalibrationSnapshot& snapshot);

    bool ready() const;
    cv::Point2d pixelToPhysical(double x, double y) const;
    cv::Point2d physicalToPixel(double x, double y) const;
    double zToPhysical(double z) const;
    CxCalibrationTransformTrace tracePixelToPhysical3D(double x, double y, double z) const;

    const CxCalibrationSnapshot& snapshot() const;
    const std::string& status() const;
    const std::string& reason() const;

private:
    CxCalibration m_calibration;
    CxCalibrationSnapshot m_bound_snapshot;
};

CxCalibrationSnapshot BuildIdentityCalibrationSnapshot(const std::string& calibration_id,
                                                       const std::string& unit);
CxCalibrationSnapshot BuildCalibrationSnapshot(const CxCalibration& calibration);

// Legacy compile-boundary symbols. The full old implementation is kept outside
// this new semantic boundary as CxCalibrationXXX.*. These declarations remain
// only to avoid breaking callers that still link against the historical names.
class CalibrationGrid;
class CalibrationXYZ;
class CameraModel;
class GridPlane;
class CalibrationSystem;

struct CalibrationData;
struct PointWithZ_O;
struct CameraIntrinsics_O;
struct DistortionParams3D;

double calibtest(
    std::vector<std::vector<cv::Point3f>> tmp_obj_pts,
    std::vector<std::vector<cv::Point2d>> tmp_img_pts,
    cv::Size imageSize,
    cv::Mat cameraMatrix,
    cv::Mat distCoeffs,
    std::vector<cv::Mat> _rvecs,
    std::vector<cv::Mat> _tvecs);

extern std::vector<std::vector<cv::Point2d>> g_conversionResults;

int runCameraCalibration();
int RunCalibrationGrid();

#endif
