#pragma once

#include "FindCircle.h"

#include <string>

class CircleRingGauge
{
public:
    void settolerance(double center_tolerance, double thickness_tolerance);
    void setouter(void* outer_circle_ptr);
    void setinner(void* inner_circle_ptr);
    void build();

    int has_result();

    double outer_radius();
    double inner_radius();
    double thickness();
    double center_distance();

    int concentric_ok();
    int inside_ok();
    int thickness_ok();

    double score();

    int status_code();

    double m_center_tolerance = 3.0;
    double m_thickness_tolerance = 5.0;

    bool m_has_result = false;

    double m_outer_radius = 0.0;
    double m_inner_radius = 0.0;
    double m_thickness = 0.0;
    double m_center_distance = 0.0;

    bool m_concentric_ok = false;
    bool m_inside_ok = false;
    bool m_thickness_ok = false;

    double m_score = 0.0;

    std::string m_status;
    std::string m_reason;
    std::string m_result_ref;

private:
    FindCircle* m_outer_circle = nullptr;
    FindCircle* m_inner_circle = nullptr;

    int StatusCodeFromString(const std::string& status) const;
};
