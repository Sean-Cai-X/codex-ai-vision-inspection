#include "CxRuntimeProjectionExecutor.h"
#include "CxScriptHeadlessRunner.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace {

static const char* kFindlineProjectionScript =
    "cxparser/cxscript/module/cximage/headless/projection/findline_projection.cxsc";
static const char* kFindCircleProjectionScript =
    "cxparser/cxscript/module/cximage/headless/projection/findcircle_projection.cxsc";
static const char* kFindEllipseProjectionScript =
    "cxparser/cxscript/module/cximage/headless/projection/findellipse_projection.cxsc";
static const char* kFindRectProjectionScript =
    "cxparser/cxscript/module/cximage/headless/projection/findrect_projection.cxsc";
static const char* kFastMatchProjectionScript =
    "cxparser/cxscript/module/cximage/headless/projection/fastmatch_projection.cxsc";

bool Fail(CxRuntimeProjectionResult& result, const std::string& stage, const std::string& reason)
{
    result.failure_stage = stage;
    result.reason = reason;
    return false;
}

std::string OwnerTypeForTool(const std::string& tool_id)
{
    if (tool_id == "findline_gauge") return "FindLine";
    if (tool_id == "findcircle_gauge") return "FindCircle";
    if (tool_id == "findellipse_gauge") return "FindEllipse";
    if (tool_id == "findrect_gauge") return "FindRect";
    if (tool_id == "FastMatch") return "FastMatch";
    return tool_id;
}

const char* ProjectionScriptForTool(const std::string& tool_id)
{
    if (tool_id == "findline_gauge") return kFindlineProjectionScript;
    if (tool_id == "findcircle_gauge") return kFindCircleProjectionScript;
    if (tool_id == "findellipse_gauge") return kFindEllipseProjectionScript;
    if (tool_id == "findrect_gauge") return kFindRectProjectionScript;
    if (tool_id == "FastMatch") return kFastMatchProjectionScript;
    return nullptr;
}

void CountRolesFromSnapshots(CxRuntimeProjectionResult& result)
{
    result.role_counts.clear();
    for (const auto& shape : result.published_shapes)
        result.role_counts[shape.semantic_role]++;
}

void NormalizePublishedShapeOwners(CxRuntimeProjectionResult& result)
{
    for (auto& shape : result.published_shapes)
    {
        shape.owner_type = result.owner_type;
        shape.owner_ref = result.owner_ref;
    }
}

bool ValidateProjectionRequest(const CxRuntimeProjectionRequest& request,
                               CxRuntimeProjectionResult& result)
{
    if (request.owner_ref.empty())
        return Fail(result, "request_validation", "owner_ref is empty");

    if (request.tool_id == "findline_gauge")
    {
        if (request.roi_x0 == request.roi_x1 && request.roi_y0 == request.roi_y1)
            return Fail(result, "request_validation", "Findline axis length is zero");
    }
    else if (request.tool_id == "findcircle_gauge")
    {
        const double dx = request.circle_px - request.circle_cx;
        const double dy = request.circle_py - request.circle_cy;
        if (std::hypot(dx, dy) < 2.0)
            return Fail(result, "request_validation", "FindCircle radius is too small");
    }
    else if (request.tool_id == "findellipse_gauge")
    {
        if (!request.has_ellipse_roi)
            return Fail(result, "request_validation", "FindEllipse requires has_ellipse_roi");
        if (request.ellipse_rx <= 1.0 || request.ellipse_ry <= 1.0)
            return Fail(result, "request_validation", "FindEllipse radius is too small");
        if (std::abs(request.ellipse_angle_deg) > 0.001)
            return Fail(result,
                        "unsupported_rotated_ellipse_roi",
                        "FindEllipse rotated ROI is not bound yet");
    }
    else if (request.tool_id == "findrect_gauge")
    {
        if (request.has_rotated_rect_roi)
        {
            if (request.rect_width < 2 || request.rect_height < 2)
                return Fail(result, "request_validation", "FindRect rotated rect width or height is too small");
        }
        else
        {
            const int width = static_cast<int>(std::abs(request.roi_x1 - request.roi_x0));
            const int height = static_cast<int>(std::abs(request.roi_y1 - request.roi_y0));
            if (width < 2 || height < 2)
                return Fail(result, "request_validation", "FindRect width or height is too small");
        }
    }
    else if (request.tool_id == "FastMatch")
    {
        if (!request.has_learn_roi && !request.has_search_roi)
            return Fail(result, "request_validation", "FastMatch requires at least one of learn_roi or search_roi");
        if (request.has_learn_roi &&
            (request.learn_roi.width < 2 || request.learn_roi.height < 2))
        {
            return Fail(result, "request_validation", "FastMatch learn ROI width or height is too small");
        }
        if (request.has_search_roi &&
            (request.search_roi.width < 2 || request.search_roi.height < 2))
        {
            return Fail(result, "request_validation", "FastMatch search ROI width or height is too small");
        }
    }

    return true;
}

CxScriptHeadlessOptions BuildHeadlessOptions(const CxRuntimeProjectionRequest& request)
{
    CxScriptHeadlessOptions options;
    options.enabled = true;
    options.image_path = request.test_image_path.empty()
        ? request.image_path
        : request.test_image_path;
    options.template_image_path = request.template_image_path;
    options.script_path = ProjectionScriptForTool(request.tool_id);
    options.case_name = request.case_id;
    options.output_dir = ".";
    options.timeout_sec = 10;
    options.max_elapsed_ms = 5000;
    options.max_scan_lines = 4096;
    options.max_samples = 200000;

    options.roi_x0 = static_cast<int>(request.roi_x0);
    options.roi_y0 = static_cast<int>(request.roi_y0);
    options.roi_x1 = static_cast<int>(request.roi_x1);
    options.roi_y1 = static_cast<int>(request.roi_y1);

    options.circle_cx = static_cast<int>(request.circle_cx);
    options.circle_cy = static_cast<int>(request.circle_cy);
    options.circle_px = static_cast<int>(request.circle_px);
    options.circle_py = static_cast<int>(request.circle_py);

    if (request.has_ellipse_roi)
    {
        options.roi_x0 = static_cast<int>(request.ellipse_cx - request.ellipse_rx);
        options.roi_y0 = static_cast<int>(request.ellipse_cy - request.ellipse_ry);
        options.roi_x1 = static_cast<int>(request.ellipse_cx + request.ellipse_rx);
        options.roi_y1 = static_cast<int>(request.ellipse_cy + request.ellipse_ry);
    }

    if (request.has_rotated_rect_roi)
    {
        options.roi_x0 = static_cast<int>(request.rect_cx - request.rect_width * 0.5);
        options.roi_y0 = static_cast<int>(request.rect_cy - request.rect_height * 0.5);
        options.roi_x1 = static_cast<int>(request.rect_cx + request.rect_width * 0.5);
        options.roi_y1 = static_cast<int>(request.rect_cy + request.rect_height * 0.5);
    }

    options.tool_half_width = request.tool_half_width;
    options.wgap = request.wgap;
    options.hgap = request.hgap;
    options.gap = request.gap;
    options.linegap = request.linegap;
    options.threshold = request.threshold;
    options.method = request.method;
    options.filterprofile = request.filter_profile;
    options.min_score = request.min_score;
    options.find_num = request.find_num;
    options.compare_gap = request.compare_gap;
    options.algorithm_executed = request.require_algorithm_execution ? 1 : 0;

    if (request.has_learn_roi)
    {
        options.learn_roi_x = static_cast<int>(request.learn_roi.x);
        options.learn_roi_y = static_cast<int>(request.learn_roi.y);
        options.learn_roi_w = static_cast<int>(request.learn_roi.width);
        options.learn_roi_h = static_cast<int>(request.learn_roi.height);
        if (options.roi_x1 == options.roi_x0 && options.roi_y1 == options.roi_y0)
        {
            options.roi_x0 = options.learn_roi_x;
            options.roi_y0 = options.learn_roi_y;
            options.roi_x1 = options.learn_roi_x + options.learn_roi_w;
            options.roi_y1 = options.learn_roi_y + options.learn_roi_h;
        }
    }

    if (request.has_search_roi)
    {
        options.search_roi_x = static_cast<int>(request.search_roi.x);
        options.search_roi_y = static_cast<int>(request.search_roi.y);
        options.search_roi_w = static_cast<int>(request.search_roi.width);
        options.search_roi_h = static_cast<int>(request.search_roi.height);
    }

    if (request.has_expected_rect)
    {
        options.expected_rect_x = static_cast<int>(request.expected_rect.x);
        options.expected_rect_y = static_cast<int>(request.expected_rect.y);
        options.expected_rect_w = static_cast<int>(request.expected_rect.width);
        options.expected_rect_h = static_cast<int>(request.expected_rect.height);
    }

    return options;
}

void PopulateProjectionMetrics(const CxRuntimeProjectionRequest& request,
                               const CxScriptExecutionCapture& capture,
                               CxRuntimeProjectionResult& result)
{
    result.executed = request.require_algorithm_execution;
    result.valid_points_count = capture.valid_points_count;
    result.has_fit_line = capture.has_fit_line;
    result.has_fit_circle = capture.has_fit_circle;
    result.has_fit_ellipse = capture.has_fit_ellipse;
    result.has_result_rect = capture.has_result_rect;
    result.fit_residual = capture.avgdist;
    result.circle_radius = capture.circle_radius;
    result.avgdist = capture.avgdist;
    result.model_point_count = capture.model_point_count;
    result.candidate_count = capture.candidate_count;
    result.best_score = capture.best_score;
    result.has_result_box = capture.has_result_box;
    result.has_best_result = capture.has_best_result;
    result.published_shapes = capture.shapes;

    NormalizePublishedShapeOwners(result);
    CountRolesFromSnapshots(result);

    result.publish_ok = !result.published_shapes.empty();

    if (!request.require_algorithm_execution)
    {
        result.algorithm_ok = false;
        return;
    }

    if (request.tool_id == "findline_gauge")
    {
        result.algorithm_ok =
            result.valid_points_count >= 2 &&
            result.has_fit_line;
    }
    else if (request.tool_id == "findcircle_gauge")
    {
        result.algorithm_ok =
            result.valid_points_count >= 3 &&
            result.has_fit_circle &&
            result.circle_radius > 0.0;
    }
    else if (request.tool_id == "findellipse_gauge")
    {
        result.algorithm_ok = result.has_fit_ellipse;
    }
    else if (request.tool_id == "findrect_gauge")
    {
        result.algorithm_ok = result.has_result_rect;
    }
    else if (request.tool_id == "FastMatch")
    {
        result.algorithm_ok = result.model_point_count > 0;
    }

    if (!result.algorithm_ok)
    {
        result.failure_stage = capture.failure_stage.empty()
            ? "algorithm_result"
            : capture.failure_stage;
        result.reason = capture.reason.empty()
            ? "projection cxscript executed but result did not satisfy tool contract"
            : capture.reason;
    }
}

bool ExecuteViaCxScript(const CxRuntimeProjectionRequest& request,
                        ImageAnnotationLayer&,
                        CxRuntimeProjectionResult& result)
{
    result.owner_type = OwnerTypeForTool(request.tool_id);
    result.owner_ref = request.owner_ref;

    const char* script_path = ProjectionScriptForTool(request.tool_id);
    if (script_path == nullptr)
        return Fail(result, "tool_not_found", "no projection script registered for tool: " + request.tool_id);

    if (!ValidateProjectionRequest(request, result))
        return false;

    CxScriptHeadlessOptions options = BuildHeadlessOptions(request);

    CxScriptExecutionCapture capture;
    std::string reason;
    if (!RunCxScriptHeadlessCapture(options, capture, reason))
    {
        result.failure_stage = capture.failure_stage.empty()
            ? "headless_projection"
            : capture.failure_stage;
        result.reason = reason;
        return false;
    }

    PopulateProjectionMetrics(request, capture, result);
    return true;
}

}

CxRuntimeProjectionExecutor::CxRuntimeProjectionExecutor()
{
    Register("findline_gauge", ExecuteViaCxScript);
    Register("findcircle_gauge", ExecuteViaCxScript);
    Register("findellipse_gauge", ExecuteViaCxScript);
    Register("findrect_gauge", ExecuteViaCxScript);
    Register("FastMatch", ExecuteViaCxScript);
}

void CxRuntimeProjectionExecutor::Register(const std::string& tool_id, const ProjectionHandler& handler)
{
    m_handlers[tool_id] = handler;
}

bool CxRuntimeProjectionExecutor::Execute(const CxRuntimeProjectionRequest& request,
                                          ImageAnnotationLayer& output_layer,
                                          CxRuntimeProjectionResult& result)
{
    auto it = m_handlers.find(request.tool_id);
    if (it == m_handlers.end())
    {
        result.failure_stage = "tool_not_found";
        result.reason = "no projection handler registered for tool: " + request.tool_id;
        return false;
    }

    return it->second(request, output_layer, result);
}
