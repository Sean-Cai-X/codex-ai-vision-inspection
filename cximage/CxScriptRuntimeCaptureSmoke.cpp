#include "pch.h"
#include "CxScriptRuntimeCaptureSmoke.h"
#include "CxScriptHeadlessRuntime.h"
#include "ParserClass.h"

#include <cmath>

namespace
{
const CxShapeElementSnapshot* FindShape(
    const CxScriptExecutionCapture& capture,
    const std::string& owner_type,
    const std::string& owner_ref,
    const std::string& stable_ref,
    const std::string& semantic_role)
{
    for (const auto& shape : capture.shapes)
    {
        if (shape.owner_type == owner_type &&
            shape.owner_ref == owner_ref &&
            shape.stable_ref == stable_ref &&
            shape.semantic_role == semantic_role)
        {
            return &shape;
        }
    }

    return nullptr;
}
}

bool ValidateCxScriptRuntimeCaptureSmoke(
    mu::CxParserRuntime& runtime,
    CxScriptExecutionCapture& capture,
    std::string& reason)
{
    capture.smoke_pass = false;

    if (runtime.GetClassObjSum("FindLine") != 1)
    {
        reason = "expected exactly one Findline object";
        return false;
    }

    capture.smoke_findline_object_name = runtime.GetClassObjName("FindLine", 0);
    if (capture.smoke_findline_object_name != "m_line")
    {
        reason = "Findline object name mismatch";
        return false;
    }

    if (runtime.GetClassObj("FindLine", "m_line") == nullptr)
    {
        reason = "Findline object m_line is unavailable";
        return false;
    }

    const auto* line_roi = FindShape(
        capture,
        "FindLine",
        "m_line",
        "m_line.roi_axis",
        "roi");

    capture.smoke_findline_roi = (line_roi != nullptr);
    if (line_roi == nullptr)
    {
        reason = "missing Findline roi shape";
        return false;
    }

    const auto* line_scan = FindShape(
        capture,
        "FindLine",
        "m_line",
        "m_line.scan_box",
        "scan");

    capture.smoke_findline_scan = (line_scan != nullptr);
    if (line_scan == nullptr)
    {
        reason = "missing Findline scan shape";
        return false;
    }

    if (line_scan->shape_kind != "PolylineShape" ||
        line_scan->points.size() != 8 ||
        !line_scan->closed)
    {
        reason = "Findline scan geometry is invalid";
        return false;
    }

    if (runtime.GetClassObjSum("FindCircle") != 1)
    {
        reason = "expected exactly one FindCircle object";
        return false;
    }

    capture.smoke_findcircle_object_name = runtime.GetClassObjName("FindCircle", 0);
    if (capture.smoke_findcircle_object_name != "m_circle")
    {
        reason = "FindCircle object name mismatch";
        return false;
    }

    if (runtime.GetClassObj("FindCircle", "m_circle") == nullptr)
    {
        reason = "FindCircle object m_circle is unavailable";
        return false;
    }

    const auto* circle_roi = FindShape(
        capture,
        "FindCircle",
        "m_circle",
        "m_circle.roi_circle",
        "roi");

    if (circle_roi == nullptr)
    {
        reason = "missing FindCircle roi circle";
        return false;
    }

    capture.smoke_findcircle_roi_shape_kind = circle_roi->shape_kind;
    if (capture.smoke_findcircle_roi_shape_kind != "CircleShape")
    {
        reason = "FindCircle roi is not CircleShape";
        return false;
    }

    if (circle_roi->radius <= 0.0)
    {
        reason = "FindCircle roi radius is zero";
        return false;
    }

    capture.smoke_findcircle_roi_radius = circle_roi->radius;
    if (std::abs(capture.smoke_findcircle_roi_radius - 100.0) > 0.001)
    {
        reason = "FindCircle roi radius mismatch";
        return false;
    }

    const auto* circle_scan = FindShape(
        capture,
        "FindCircle",
        "m_circle",
        "m_circle.outer_scan_circle",
        "scan");

    if (circle_scan == nullptr ||
        circle_scan->shape_kind != "CircleShape" ||
        circle_scan->radius <= circle_roi->radius)
    {
        reason = "FindCircle outer scan circle is invalid";
        return false;
    }

    capture.smoke_findcircle_outer_scan_radius = circle_scan->radius;

    capture.smoke_pass = true;
    reason.clear();
    return true;
}