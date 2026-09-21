#pragma once

#include "FindSegmentationBackend.h"

#include "Image.h"

#include <string>

class ICxShapeSink;

class FindSegmentation
{
public:
    FindSegmentation();

    void setbackend(const char* backend);
    void setmodel(const char* model_path);
    void settask(const char* task_id);
    void setmodelid(const char* model_id);
    void setmodelpackage(const char* model_package_ref);
    void setmanifest(const char* manifest_path);
    void setoutputroot(const char* output_root);
    void setpostprocessprofile(const char* postprocess_profile);
    void setparameterprofile(const char* parameter_profile_ref);

    void setdevice(const char* device);

    void setthreshold(double threshold);
    void setpromptrect(int x0, int y0, int x1, int y1);
    void setpromptrectxyxy(int y1, int x1, int y0, int x0);
    // setpoint remains a legacy positive-prompt alias. New CxScript declares
    // prompt polarity explicitly.
    void setpoint(int x, int y);
    void setpositivepoint(int x, int y);
    void setnegativepoint(int x, int y);
    // CxScript adapter methods. The current two-argument binding supplies
    // arguments in reverse order, so scripts must use these xy entry points.
    void setpositivepointxy(int y, int x);
    void setnegativepointxy(int y, int x);
    void setmode(int mode);
    void setgeometrytype(const char* geometry_type);


    void segment(void* image);
    void extractboundary();
    void buildoverlay(void* image);

    const char* get_result();
    const char* get_mask_ref();
    const char* get_contour_ref();
    const char* get_overlay_ref();

    int status_code();
    int get_contour_count();
    double get_primary_area();
    int get_geometry_count();
    double get_geometry_residual();
    double get_geometry_support();
    double get_geometry_center_x();
    double get_geometry_center_y();
    double get_geometry_radius();
    double get_geometry_axis_x();
    double get_geometry_axis_y();
    double get_geometry_angle_deg();

    const std::string& backend() const;
    const std::string& model_path() const;
    const std::string& device() const;
    const FindSegmentationResult& result() const;
    const FindSegmentationInputSnapshot& lastinputrequest() const;
    const FindSegmentationBackendDiagnosticSnapshot& backenddiagnostic() const;
    void PublishDisplayShapes(ICxShapeSink& sink, const std::string& owner_ref) const;

public:
    std::string m_status;
    std::string m_reason;
    std::string m_result_ref;
    std::string m_mask_ref;
    std::string m_contour_ref;
    std::string m_overlay_ref;

private:
    std::string m_backend = "opencv_smoke";
    std::string m_model_path;
    std::string m_task_id;

    std::string m_model_id;

    std::string m_model_package_ref;

    std::string m_manifest_path;

    std::string m_output_root;

    std::string m_postprocess_profile;

    std::string m_parameter_profile_ref;


    std::string m_device = "auto";

    double m_threshold = 0.5;
    int m_mode = 0;
    std::string m_geometry_type;


    bool m_has_rect = false;
    int m_x0 = 0;
    int m_y0 = 0;
    int m_x1 = 0;
    int m_y1 = 0;

    bool m_has_positive_point = false;
    int m_positive_x = 0;
    int m_positive_y = 0;
    bool m_has_negative_point = false;
    int m_negative_x = 0;
    int m_negative_y = 0;

    FindSegmentationResult m_result;
    FindSegmentationInputSnapshot m_last_input_request;
    FindSegmentationBackendDiagnosticSnapshot m_backend_diagnostic;
};
