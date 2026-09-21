#include "CxShapeInteractionRunner.h"
#include "ViewController.h"
#include "CxUnifiedLog.h"
#include "CxManifestProjectionRequestResolver.h"
#include "CxShapeOverlayRenderer.h"

#include <filesystem>
#include <unordered_set>
#include <map>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <opencv2/imgcodecs.hpp>

namespace fs = std::filesystem;

static std::string EscapeJson(const std::string& s);

static std::string NormalizeComparablePath(const std::string& path)
{
    if (path.empty())
        return {};
    std::error_code ec;
    fs::path normalized = fs::weakly_canonical(fs::absolute(fs::path(path), ec), ec);
    if (ec)
        normalized = fs::path(path).lexically_normal();
    std::string value = normalized.generic_string();
#ifdef _WIN32
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
#endif
    return value;
}

static const CxShapeElementSnapshot* FindPublishedShapeByRole(
    const CxRuntimeProjectionResult& projection,
    const std::string& role)
{
    for (const auto& shape : projection.published_shapes)
    {
        if (shape.semantic_role == role)
            return &shape;
    }
    return nullptr;
}

static double AngleDifferenceDegrees(double lhs, double rhs)
{
    double difference = std::fmod(std::abs(lhs - rhs), 180.0);
    return std::min(difference, 180.0 - difference);
}

bool CxShapeInteractionRunner::TryFindToolSpec(
    const CxAnnotationToolManifestSnapshot& tool_manifest,
        const std::string& tool_id,
        CxAnnotationToolSpec& output)
{
    for (const CxAnnotationToolSpec& tool : tool_manifest.tools)
    {
        if (tool.id == tool_id)
        {
            output = tool;
            return true;
        }
    }
    return false;
}

bool CxShapeInteractionRunner::RunSuite(
    const CxAnnotationToolManifestSnapshot& tool_manifest,
    const CxShapeTestSuiteSnapshot& suite,
    const CxScriptImageManifestRuntime& image_manifest,
    ICxRuntimeProjectionExecutor& projection_executor,
    const CxShapeInteractionOptions& options,
    CxShapeInteractionBatchResultEx& result)
{
    if (options.run_id.empty())
    {
        result.pass = false;
        CXLOG_ERROR(
            "CxShapeInteractionRunner",
            "shape_suite_validate",
            "failed",
            "run_id is empty");

        if (!options.out_dir.empty())
        {
            fs::create_directories(options.out_dir);
            std::ofstream fail_file(fs::path(options.out_dir) / "shape_interaction_report.json");
            if (fail_file)
            {
                fail_file << "{\n";
                fail_file << "  \"pass\": false,\n";
                fail_file << "  \"total_cases\": 0,\n";
                fail_file << "  \"executed_cases\": 0,\n";
                fail_file << "  \"pass_count\": 0,\n";
                fail_file << "  \"fail_count\": 0,\n";
                fail_file << "  \"error\": \"run_id_empty\"\n";
                fail_file << "}\n";
            }
        }

        return false;
    }

    CXLOG_INFO("CxShapeInteractionRunner", "shape_suite_begin", "running", 
               "suite_path=" + options.test_suite_path + ", run_id=" + options.run_id);

    const auto& cases = suite.cases;

    if (cases.empty())
    {
        result.pass = false;
        CXLOG_ERROR("CxShapeInteractionRunner", "shape_suite_validate", "failed", "test suite contains no cases");

        if (!options.out_dir.empty())
        {
            fs::create_directories(options.out_dir);
            std::ofstream fail_file(fs::path(options.out_dir) / "shape_interaction_report.json");
            if (fail_file)
            {
                fail_file << "{\n";
                fail_file << "  \"pass\": false,\n";
                fail_file << "  \"total_cases\": 0,\n";
                fail_file << "  \"executed_cases\": 0,\n";
                fail_file << "  \"pass_count\": 0,\n";
                fail_file << "  \"fail_count\": 0,\n";
                fail_file << "  \"error\": \"empty_test_suite\"\n";
                fail_file << "}\n";
            }
        }

        return false;
    }

    std::unordered_set<std::string> case_ids;
    for (const auto& tc : cases)
    {
        if (tc.case_id.empty())
        {
            result.pass = false;
            CXLOG_ERROR("CxShapeInteractionRunner", "shape_suite_validate", "failed", "test suite contains case with empty case_id");
            return false;
        }

        if (!case_ids.insert(tc.case_id).second)
        {
            result.pass = false;
            CXLOG_ERROR("CxShapeInteractionRunner", "shape_suite_validate", "failed", "duplicate case_id: " + tc.case_id);
            return false;
        }
    }

    CXLOG_INFO("CxShapeInteractionRunner", "shape_suite_begin", "running",
        "total_cases=" + std::to_string(cases.size()) +
        ", tool_manifest=" + options.tool_manifest_path +
        ", test_suite=" + options.test_suite_path);

    result.extended_cases.resize(cases.size());

    int pass_count = 0;
    for (size_t i = 0; i < cases.size(); ++i)
    {
        CxShapeInteractionTrace trace;

        CxUnifiedLogContext caseContext;
        caseContext.case_id = cases[i].case_id;
        caseContext.tool = cases[i].tool_id;
        caseContext.run_id = options.run_id;

        CxScopedLogContext caseScope(caseContext);

        CXLOG_INFO("CxShapeInteractionRunner", "shape_case_begin", "running",
            "operation=" + cases[i].operation + ", handle=" + cases[i].handle);

        RunTestCase(cases[i], tool_manifest, image_manifest, projection_executor, options, result.extended_cases[i], trace);

        const auto& caseResult = result.extended_cases[i];
        if (caseResult.pass)
            pass_count++;

        std::string caseDir;
        if (!options.out_dir.empty())
        {
            caseDir = (fs::path(options.out_dir) / "cases" / caseResult.case_id).string();
            GenerateCaseOutput(caseResult, trace, options, options.out_dir);
        }

        std::string status = caseResult.pass ? "passed" : "failed";
        std::string detail =
            "expected_handle=" + caseResult.expected_handle +
            ", actual_handle=" + caseResult.actual_handle +
            ", acceptance_scope=" + caseResult.acceptance_scope +
            ", runtime_writeback=" + std::string(caseResult.runtime_writeback ? "true" : "false");

        if (!caseDir.empty())
            detail += ", case_dir=" + caseDir;

        if (caseResult.pass)
        {
            CXLOG_INFO("CxShapeInteractionRunner", "shape_case_end", status, detail);
        }
        else
        {
            CXLOG_ERROR("CxShapeInteractionRunner", "shape_case_end", status,
                detail + ", reason=" + caseResult.reason);
        }
    }

    result.pass = (pass_count == static_cast<int>(cases.size()));
    result.cases.reserve(result.extended_cases.size());
    for (const auto& ec : result.extended_cases)
        result.cases.push_back(ec);

    if (!options.out_dir.empty())
        GenerateBatchOutput(result, options, options.out_dir);

    CXLOG_INFO("CxShapeInteractionRunner", "shape_suite_end", 
               result.pass ? "passed" : "failed", 
               "total=" + std::to_string(cases.size()) + ", pass=" + std::to_string(pass_count));

    return result.pass;
}



bool CxShapeInteractionRunner::VerifyHitExpectation(
    const CxShapeTestCase& tc,
    const CxShapeHitResult& hit,
    std::string& reason)
{
    const std::string actual_handle = HandleName(hit.shape_hit.role);
    const int actual_vertex = hit.shape_hit.vertex_index;

    if (actual_handle != tc.handle)
    {
        reason = "expected handle=" + tc.handle + ", actual handle=" + actual_handle;
        return false;
    }

    if (tc.vertex_index != -1 && actual_vertex != tc.vertex_index)
    {
        reason = "expected vertex=" + std::to_string(tc.vertex_index) +
                 ", actual vertex=" + std::to_string(actual_vertex);
        return false;
    }

    reason = "hit expectation verified";
    return true;
}

bool CxShapeInteractionRunner::RunTestCase(
    const CxShapeTestCase& tc,
    const CxAnnotationToolManifestSnapshot& tool_manifest,
    const CxScriptImageManifestRuntime& image_manifest,
    ICxRuntimeProjectionExecutor& projection_executor,
    const CxShapeInteractionOptions& options,
    CxShapeInteractionCaseResultEx& case_result,
    CxShapeInteractionTrace& trace)
{
    case_result.case_id = tc.case_id;
    case_result.tool_id = tc.tool_id;
    case_result.operation = tc.operation;
    case_result.expected_handle = tc.handle;
    case_result.geometry_assertion = tc.expected;
    case_result.expected_vertex = tc.vertex_index;

    ImageAnnotationLayer layer;
    layer.ClearShapeElements();

    std::string apply_reason;
    if (!layer.ApplyToolManifestSnapshot(tool_manifest, apply_reason))
    {
        case_result.pass = false;
        case_result.conclusion = "manifest_apply_failed";
        case_result.status = "FAIL";
        case_result.reason = "manifest apply failed: " + apply_reason;
        return false;
    }

    const bool requires_palette_tool = tc.operation != "runtime_publish";
    const AnnotationToolDefinition* tool = nullptr;

    if (requires_palette_tool && !tc.tool_id.empty())
    {
        tool = layer.FindToolDefinition(tc.tool_id);
        if (!tool)
        {
            case_result.pass = false;
            case_result.conclusion = "tool_not_registered";
            case_result.status = "FAIL";
            case_result.reason = "tool not registered in manifest snapshot: " + tc.tool_id;
            return false;
        }

        case_result.shape_kind = tool->shape_type;
        case_result.owner_type = tool->owner_tool;
        case_result.owner_binding = tool->owner_binding;
    }
    
    if (tc.tool_id.empty())
    {
        CXLOG_INFO("CxShapeInteractionRunner", "shape_case_info", "info", "case=" + tc.case_id + ", tool_id_is_empty=true");
    }

    std::vector<CxShapePoint> initial_points;
    if (tc.operation == "drag_handle")
    {
        if (tool->shape_type == "LineShape" || tool->shape_type == "LineGaugeShape")
        {
            if (tc.handle == "End")
            {
                initial_points.push_back({tc.from_x - 50.0, tc.from_y - 30.0});
                initial_points.push_back({tc.from_x, tc.from_y});
            }
            else if (tc.handle == "Center")
            {
                initial_points.push_back({tc.from_x - 25.0, tc.from_y - 15.0});
                initial_points.push_back({tc.from_x + 25.0, tc.from_y + 15.0});
            }
            else if (tc.handle == "WidthPositive")
            {
                const double cx = tc.from_x;
                const double cy = tc.from_y - 20.0;
                initial_points.push_back({cx - 30.0, cy});
                initial_points.push_back({cx + 30.0, cy});
            }
            else if (tc.handle == "WidthNegative")
            {
                const double cx = tc.from_x;
                const double cy = tc.from_y + 20.0;
                initial_points.push_back({cx - 30.0, cy});
                initial_points.push_back({cx + 30.0, cy});
            }
            else
            {
                initial_points.push_back({tc.from_x, tc.from_y});
                initial_points.push_back({tc.from_x + 50.0, tc.from_y + 30.0});
            }
        }
        else if (tool->shape_type == "RectShape")
        {
            if (tc.has_initial_rect)
            {
                initial_points.push_back({tc.initial_rx0, tc.initial_ry0});
                initial_points.push_back({tc.initial_rx1, tc.initial_ry1});
            }
            else if (tc.handle == "Center")
            {
                initial_points.push_back({tc.from_x - 30.0, tc.from_y - 20.0});
                initial_points.push_back({tc.from_x + 30.0, tc.from_y + 20.0});
            }
            else
            {
                initial_points.push_back({tc.from_x, tc.from_y});
                initial_points.push_back({tc.from_x + 60.0, tc.from_y + 40.0});
            }
        }
        else if (tool->shape_type == "CircleShape")
        {
            if (tc.has_initial_circle)
            {
                initial_points.push_back({tc.initial_ccx, tc.initial_ccy});
                initial_points.push_back({tc.initial_ccx + tc.initial_cradius, tc.initial_ccy});
            }
            else if (tc.handle == "Radius")
            {
                const double initial_radius = 40.0;
                initial_points.push_back({tc.from_x - initial_radius, tc.from_y});
                initial_points.push_back({tc.from_x, tc.from_y});
            }
            else
            {
                initial_points.push_back({tc.from_x, tc.from_y});
                initial_points.push_back({tc.from_x + 40.0, tc.from_y});
            }
        }
        else if (tool->shape_type == "EllipseShape")
        {
            if (tc.has_initial_ellipse)
            {
                initial_points.push_back({tc.initial_ex - tc.initial_erx, tc.initial_ey - tc.initial_ery});
                initial_points.push_back({tc.initial_ex + tc.initial_erx, tc.initial_ey + tc.initial_ery});
            }
            else if (tc.handle == "RadiusX")
            {
                initial_points.push_back({tc.from_x - 40.0, tc.from_y - 25.0});
                initial_points.push_back({tc.from_x + 40.0, tc.from_y + 25.0});
            }
            else if (tc.handle == "RadiusY")
            {
                initial_points.push_back({tc.from_x - 40.0, tc.from_y - 25.0});
                initial_points.push_back({tc.from_x + 40.0, tc.from_y + 25.0});
            }
            else
            {
                initial_points.push_back({tc.from_x - 40.0, tc.from_y - 25.0});
                initial_points.push_back({tc.from_x + 40.0, tc.from_y + 25.0});
            }
        }
        else if (tool->shape_type == "PolylineShape")
        {
            if (tc.has_initial_points)
            {
                for (size_t i = 0; i < tc.initial_points.size(); i += 2)
                {
                    if (i + 1 < tc.initial_points.size())
                        initial_points.push_back({tc.initial_points[i], tc.initial_points[i+1]});
                }
            }
            else if (tc.handle == "Center")
            {
                initial_points.push_back({tc.from_x - 30.0, tc.from_y - 15.0});
                initial_points.push_back({tc.from_x, tc.from_y + 10.0});
                initial_points.push_back({tc.from_x + 30.0, tc.from_y - 15.0});
            }
            else
            {
                initial_points.push_back({tc.from_x, tc.from_y});
                initial_points.push_back({tc.from_x + 30.0, tc.from_y + 20.0});
                initial_points.push_back({tc.from_x + 60.0, tc.from_y});
            }
        }
        else if (tool->shape_type == "PointsShape")
        {
            initial_points.push_back({tc.from_x, tc.from_y});
        }
    }

    if (tc.operation == "drag_handle")
    {
        if (!tool)
        {
            case_result.pass = false;
            case_result.conclusion = "no_tool_for_drag_handle";
            case_result.status = "FAIL";
            case_result.reason = "drag_handle operation requires a tool";
            return false;
        }

        std::unique_ptr<ShapeBase> shape = CreateInitialShapeForTool(*tool, initial_points);
        if (!shape)
        {
            case_result.pass = false;
            case_result.conclusion = "shape_creation_failed";
            case_result.status = "FAIL";
            case_result.reason = "failed to create shape for tool: " + tc.tool_id;
            return false;
        }

        CxShapeGeometrySnapshot before_snap;
        shape->snapshot(before_snap);

        CxShapeElement& element = layer.CreateFromTool(*tool, std::move(shape));
        element.editable = tc.editable;

        trace = {};
        const bool success = layer.SimulatePointerDrag(
            tc.from_x, tc.from_y,
            tc.to_x, tc.to_y,
            options.drag_steps, options.tolerance, trace);

        case_result.hit_test_pass = trace.hit;
        case_result.drag_pass = trace.begin_drag_ok && trace.update_drag_ok;
        case_result.commit_pass = trace.commit_ok;

        if (!trace.hits.empty())
        {
            case_result.actual_handle = HandleName(trace.hits[0].shape_hit.role);
            case_result.actual_vertex = trace.hits[0].shape_hit.vertex_index;
        }

        bool outcomePass = trace.hit == (tc.expected_hit != 0);

        if (tc.expected_begin_drag >= 0)
        {
            outcomePass = outcomePass &&
                trace.begin_drag_ok == (tc.expected_begin_drag != 0);
        }

        if (tc.expected_commit >= 0)
        {
            outcomePass = outcomePass &&
                trace.commit_ok == (tc.expected_commit != 0);
        }

        if (!trace.hits.empty() && trace.hit && tc.expected_hit != 0)
        {
            std::string hit_reason;
            if (!VerifyHitExpectation(tc, trace.hits[0], hit_reason))
            {
                case_result.pass = false;
                case_result.hit_test_pass = false;
                case_result.status = "FAIL";
                case_result.conclusion = "unexpected_handle";
                case_result.reason = hit_reason;
                return false;
            }
        }

        if (outcomePass)
        {
            case_result.pass = true;

            if (tc.expected_commit != 0 && !trace.snapshots.empty())
            {
                const auto& after_snap = trace.snapshots.back();
                std::string verify_reason;
                bool assertion_pass = VerifyGeometryAssertion(tc.expected, before_snap, after_snap, verify_reason);

                if (assertion_pass)
                {
                    case_result.conclusion = "interaction_passed";
                    case_result.reason = "hit=" + case_result.actual_handle + ", geometry verified";

                    const bool runtimeBoundTool = !tool->owner_tool.empty() && !tool->owner_binding.empty();
                    if (runtimeBoundTool)
                    {
                        case_result.status = "GEOMETRY_PASS_PENDING_WRITEBACK";
                        case_result.acceptance_scope = "L0_L1_GEOMETRY";
                        case_result.runtime_writeback = false;
                    }
                    else
                    {
                        case_result.status = "PASS";
                        case_result.acceptance_scope = "L0_L1_MANUAL_SHAPE";
                        case_result.runtime_writeback = false;
                    }
                }
                else
                {
                    case_result.pass = false;
                    case_result.conclusion = "geometry_mismatch";
                    case_result.status = "FAIL";
                    case_result.reason = verify_reason;
                }
            }
            else
            {
                case_result.conclusion = "interaction_passed";
                case_result.reason = "hit=" + std::to_string(trace.hit) +
                    ", begin_drag=" + std::to_string(trace.begin_drag_ok) +
                    ", commit=" + std::to_string(trace.commit_ok);

                case_result.status = "PASS";
                case_result.acceptance_scope = "L0_L1_MANUAL_SHAPE";
                case_result.runtime_writeback = false;
            }
        }
        else
        {
            case_result.pass = false;
            case_result.conclusion = "interaction_failed";
            case_result.status = "FAIL";
            if (!trace.hit && tc.expected_hit != 0)
                case_result.reason = "expected hit but hit test failed";
            else if (trace.hit && tc.expected_hit == 0)
                case_result.reason = "expected no hit but hit test succeeded";
            else if (!trace.begin_drag_ok && tc.expected_begin_drag != 0)
                case_result.reason = "expected begin_drag but it failed";
            else if (trace.begin_drag_ok && tc.expected_begin_drag == 0)
                case_result.reason = "expected no begin_drag but it succeeded";
            else if (!trace.commit_ok && tc.expected_commit != 0)
                case_result.reason = "expected commit but it failed";
            else if (trace.commit_ok && tc.expected_commit == 0)
                case_result.reason = "expected no commit but it succeeded";
            else
                case_result.reason = "interaction failed";
        }
    }
    else if (tc.operation == "create" || tc.operation == "create_click")
    {
        case_result.hit_test_pass = false;
        case_result.drag_pass = false;
        case_result.commit_pass = false;

        std::vector<CxShapePoint> pts = {{tc.from_x, tc.from_y}};
        auto shape = CreateInitialShapeForTool(*tool, pts);
        if (!shape)
        {
            case_result.pass = false;
            case_result.conclusion = "shape_creation_failed";
            case_result.status = "FAIL";
            case_result.reason = "failed to create shape for tool: " + tc.tool_id;
            return false;
        }

        CxShapeElement& element = layer.CreateFromTool(*tool, std::move(shape));
        element.editable = tc.editable;
        element.visible = tc.visible;

        CxShapeGeometrySnapshot snap;
        if (element.shape)
        {
            element.shape->snapshot(snap);
            case_result.created_points_count = static_cast<int>(snap.points.size());
            case_result.created_handle_count = static_cast<int>(snap.points.size() + 1);
            case_result.shape_visible = element.visible;
            case_result.shape_editable = element.editable;
            case_result.created_shape_kind = tool->shape_type;
        }

        case_result.pass = true;
        case_result.conclusion = "shape_created";
        case_result.status = "PASS";
        case_result.reason = "shape created successfully";
        case_result.runtime_writeback = false;
        case_result.acceptance_scope = "FULL_INTERACTION";
    }
    else if (tc.operation == "create_drag")
    {
        case_result.hit_test_pass = false;
        case_result.drag_pass = false;
        case_result.commit_pass = false;

        const double dx = tc.to_x - tc.from_x;
        const double dy = tc.to_y - tc.from_y;

        if (tool->shape_type == "LineShape")
        {
            const double length = std::sqrt(dx * dx + dy * dy);
            if (length < 2.0)
            {
                case_result.pass = false;
                case_result.conclusion = "draft_too_small";
                case_result.status = "FAIL";
                case_result.reason = "line length too small: " + std::to_string(length);
                return false;
            }
        }
        else if (tool->shape_type == "RectShape")
        {
            const double width = std::abs(dx);
            const double height = std::abs(dy);
            if (width < 2.0 || height < 2.0)
            {
                case_result.pass = false;
                case_result.conclusion = "draft_too_small";
                case_result.status = "FAIL";
                case_result.reason = "rect too small: w=" + std::to_string(width) + ", h=" + std::to_string(height);
                return false;
            }
        }
        else if (tool->shape_type == "CircleShape")
        {
            const double radius = std::sqrt(dx * dx + dy * dy);
            if (radius < 2.0)
            {
                case_result.pass = false;
                case_result.conclusion = "draft_too_small";
                case_result.status = "FAIL";
                case_result.reason = "circle radius too small: " + std::to_string(radius);
                return false;
            }
        }
        else if (tool->shape_type == "EllipseShape")
        {
            const double rx = std::abs(dx);
            const double ry = std::abs(dy);
            if (rx < 2.0 || ry < 2.0)
            {
                case_result.pass = false;
                case_result.conclusion = "draft_too_small";
                case_result.status = "FAIL";
                case_result.reason = "ellipse too small: rx=" + std::to_string(rx) + ", ry=" + std::to_string(ry);
                return false;
            }
        }

        std::vector<CxShapePoint> pts = {
            {tc.from_x, tc.from_y},
            {tc.to_x, tc.to_y}
        };
        auto shape = CreateInitialShapeForTool(*tool, pts);
        if (!shape)
        {
            case_result.pass = false;
            case_result.conclusion = "shape_creation_failed";
            case_result.status = "FAIL";
            case_result.reason = "failed to create shape for tool: " + tc.tool_id;
            return false;
        }

        CxShapeElement& element = layer.CreateFromTool(*tool, std::move(shape));
        element.editable = tc.editable;
        element.visible = tc.visible;

        CxShapeGeometrySnapshot snap;
        if (element.shape)
        {
            element.shape->snapshot(snap);
            case_result.created_points_count = static_cast<int>(snap.points.size());
            case_result.created_handle_count = static_cast<int>(snap.points.size() + 1);
            case_result.shape_visible = element.visible;
            case_result.shape_editable = element.editable;
            case_result.created_shape_kind = tool->shape_type;
        }

        case_result.pass = true;
        case_result.conclusion = "shape_created";
        case_result.status = "PASS";
        case_result.reason = "shape created by drag successfully";
        case_result.runtime_writeback = false;
        case_result.acceptance_scope = "FULL_INTERACTION";
    }
    else if (tc.operation == "create_polyline")
    {
        case_result.hit_test_pass = false;
        case_result.drag_pass = false;
        case_result.commit_pass = false;

        std::vector<CxShapePoint> pts;
        if (tc.has_initial_points)
        {
            for (size_t i = 0; i < tc.initial_points.size(); i += 2)
            {
                if (i + 1 < tc.initial_points.size())
                    pts.push_back({tc.initial_points[i], tc.initial_points[i+1]});
            }
        }
        else
        {
            pts.push_back({tc.from_x, tc.from_y});
            pts.push_back({tc.from_x + 50.0, tc.from_y + 30.0});
            pts.push_back({tc.from_x + 100.0, tc.from_y});
        }

        if (pts.size() < 2)
        {
            case_result.pass = false;
            case_result.conclusion = "polyline_too_few_points";
            case_result.status = "FAIL";
            case_result.reason = "polyline needs at least 2 points";
            return false;
        }

        auto shape = CreateInitialShapeForTool(*tool, pts);
        if (!shape)
        {
            case_result.pass = false;
            case_result.conclusion = "shape_creation_failed";
            case_result.status = "FAIL";
            case_result.reason = "failed to create polyline";
            return false;
        }

        CxShapeElement& element = layer.CreateFromTool(*tool, std::move(shape));
        element.editable = tc.editable;
        element.visible = tc.visible;

        CxShapeGeometrySnapshot snap;
        if (element.shape)
        {
            element.shape->snapshot(snap);
            case_result.created_points_count = static_cast<int>(snap.points.size());
            case_result.created_handle_count = static_cast<int>(snap.points.size() + 1);
            case_result.shape_visible = element.visible;
            case_result.shape_editable = element.editable;
            case_result.created_shape_kind = tool->shape_type;
        }

        case_result.pass = true;
        case_result.conclusion = "shape_created";
        case_result.status = "PASS";
        case_result.reason = "polyline created successfully";
        case_result.runtime_writeback = false;
        case_result.acceptance_scope = "FULL_INTERACTION";
    }
    else if (tc.operation == "runtime_geometry_publish" || tc.operation == "runtime_result_publish")
    {
        case_result.hit_test_pass = false;
        case_result.drag_pass = false;
        case_result.commit_pass = false;

        ImageAnnotationLayer layer;
        layer.ClearShapeElements();

        std::string apply_reason;
        if (!layer.ApplyToolManifestSnapshot(tool_manifest, apply_reason))
        {
            case_result.status = "FAIL";
            case_result.conclusion = "manifest_apply_failed";
            case_result.reason = "failed to apply manifest snapshot: " + apply_reason;
            return false;
        }

        CxRuntimeProjectionRequest request;
        request.case_id = tc.case_id;
        request.tool_id = tc.tool_id;
        request.owner_type = tc.tool_id;
        request.owner_ref =
            tc.owner_binding.empty()
                ? "shape_result." + tc.case_id
                : tc.owner_binding;

        bool use_manifest_projection = !tc.image_manifest_path.empty() && 
                                       (!tc.manifest_target_id.empty() || !tc.manifest_match_case_id.empty());

        if (use_manifest_projection)
        {
            if (NormalizeComparablePath(tc.image_manifest_path) !=
                NormalizeComparablePath(image_manifest.manifest_path))
            {
                case_result.pass = false;
                case_result.status = "FAIL";
                case_result.conclusion = "manifest_path_mismatch";
                case_result.reason = "suite manifest path differs from CLI manifest path";
                return false;
            }

            std::string resolve_reason;
            if (!ResolveManifestProjectionRequest(image_manifest, tc, request, resolve_reason))
            {
                case_result.pass = false;
                case_result.status = "FAIL";
                case_result.conclusion = "manifest_projection_resolve_failed";
                case_result.reason = resolve_reason;
                return false;
            }
        }
        else
        {
            request.image_path = tc.image_path;

            if (tc.has_initial_line)
            {
                request.roi_x0 = tc.initial_lx0;
                request.roi_y0 = tc.initial_ly0;
                request.roi_x1 = tc.initial_lx1;
                request.roi_y1 = tc.initial_ly1;
            }
            else if (tc.has_initial_rect)
            {
                request.roi_x0 = tc.initial_rx0;
                request.roi_y0 = tc.initial_ry0;
                request.roi_x1 = tc.initial_rx1;
                request.roi_y1 = tc.initial_ry1;
            }

            if (tc.has_initial_circle)
            {
                request.circle_cx = tc.initial_ccx;
                request.circle_cy = tc.initial_ccy;
                request.circle_px = tc.initial_ccx + tc.initial_cradius;
                request.circle_py = tc.initial_ccy;
            }

            if (tc.has_initial_ellipse)
            {
                request.roi_x0 = tc.initial_ex - tc.initial_erx;
                request.roi_y0 = tc.initial_ey - tc.initial_ery;
                request.roi_x1 = tc.initial_ex + tc.initial_erx;
                request.roi_y1 = tc.initial_ey + tc.initial_ery;
            }

            if (tc.has_initial_learn_rect)
            {
                request.has_learn_roi = true;
                request.learn_roi.x = tc.initial_learn_x0;
                request.learn_roi.y = tc.initial_learn_y0;
                request.learn_roi.width = tc.initial_learn_x1 - tc.initial_learn_x0;
                request.learn_roi.height = tc.initial_learn_y1 - tc.initial_learn_y0;
            }

            if (tc.has_initial_search_rect)
            {
                request.has_search_roi = true;
                request.search_roi.x = tc.initial_search_x0;
                request.search_roi.y = tc.initial_search_y0;
                request.search_roi.width = tc.initial_search_x1 - tc.initial_search_x0;
                request.search_roi.height = tc.initial_search_y1 - tc.initial_search_y0;
            }

            request.tool_half_width = tc.tool_half_width;
        }
        request.wgap = tc.wgap;
        request.hgap = tc.hgap;
        request.gap = tc.gap;
        request.linegap = tc.linegap;
        request.threshold = tc.threshold;
        request.method = tc.method;
        request.filter_profile = tc.filter_profile;
        request.min_score = tc.min_score;
        request.find_num = tc.find_num;
        request.compare_gap = tc.compare_gap;
        request.require_algorithm_execution = (tc.operation == "runtime_result_publish");

        CxRuntimeProjectionResult projection;
        if (!projection_executor.Execute(request, layer, projection))
        {
            case_result.status = "FAIL";
            case_result.conclusion = "projection_execution_failed";
            case_result.reason = "projection execution failed: " + projection.reason;
            case_result.owner_type = projection.owner_type;
            case_result.owner_binding = projection.owner_ref;
            return false;
        }

        std::string owner_type = projection.owner_type;
        std::string owner_ref = projection.owner_ref;

        std::map<std::string, int> role_counts;
        std::map<std::string, int> editable_role_counts;
        std::map<std::string, int> result_role_counts;
        int stale_result_count = 0;
        int result_element_count = 0;
        std::unordered_set<std::string> stable_refs;
        int duplicate_stable_ref_count = 0;
        int owner_mismatch_count = 0;
        bool pass = true;

        for (const auto& elem : projection.published_shapes)
        {
            if (elem.owner_ref != owner_ref)
                continue;

            if (elem.owner_type != owner_type)
                owner_mismatch_count++;

            if (elem.stable_ref.empty())
                pass = false;

            if (stable_refs.count(elem.stable_ref) > 0)
                duplicate_stable_ref_count++;
            else
                stable_refs.insert(elem.stable_ref);

            role_counts[elem.semantic_role]++;
            if (elem.editable) editable_role_counts[elem.semantic_role]++;
            if (elem.result_element)
            {
                result_role_counts[elem.semantic_role]++;
                result_element_count++;
            }
        }

        case_result.created_points_count = static_cast<int>(stable_refs.size());

        std::string reason;

        if (!tc.role_expectations.empty())
        {
            for (const auto& exp : tc.role_expectations)
            {
                const int actual = role_counts.count(exp.role) ? role_counts[exp.role] : 0;
                if (actual < exp.min_count)
                {
                    pass = false;
                    reason += "role '" + exp.role + "' has " + std::to_string(actual) +
                              " elements, expected at least " + std::to_string(exp.min_count) + "; ";
                }
                if (exp.max_count >= 0 && actual > exp.max_count)
                {
                    pass = false;
                    reason += "role '" + exp.role + "' has " + std::to_string(actual) +
                              " elements, expected at most " + std::to_string(exp.max_count) + "; ";
                }
                if (exp.require_editable >= 0)
                {
                    const int editable_count = editable_role_counts.count(exp.role) ? editable_role_counts[exp.role] : 0;
                    if (exp.require_editable == 1 && editable_count == 0)
                    {
                        pass = false;
                        reason += "role '" + exp.role + "' is not editable; ";
                    }
                    if (exp.require_editable == 0 && editable_count > 0)
                    {
                        pass = false;
                        reason += "role '" + exp.role + "' should not be editable; ";
                    }
                }
                if (exp.require_result_element >= 0)
                {
                    const int result_count = result_role_counts.count(exp.role) ? result_role_counts[exp.role] : 0;
                    if (exp.require_result_element == 1 && result_count == 0)
                    {
                        pass = false;
                        reason += "role '" + exp.role + "' is not a result element; ";
                    }
                    if (exp.require_result_element == 0 && result_count > 0)
                    {
                        pass = false;
                        reason += "role '" + exp.role + "' should not be a result element; ";
                    }
                }
            }
        }
        else
        {
            pass = false;
            reason = "runtime projection case has no role expectations";
        }

        if (duplicate_stable_ref_count > 0)
        {
            pass = false;
            reason += "duplicate stable_ref detected; ";
        }

        if (owner_mismatch_count > 0)
        {
            pass = false;
            reason += "owner_type mismatch detected; ";
        }

        if (!tc.expected_geometry_kind.empty() ||
            tc.expected_geometry_point_count >= 0 ||
            tc.expect_geometry_from_manifest)
        {
            const CxShapeElementSnapshot* geometry =
                FindPublishedShapeByRole(projection, "roi");
            if (!geometry)
            {
                pass = false;
                reason += "published roi geometry snapshot is missing; ";
            }
            else
            {
                if (!tc.expected_geometry_kind.empty() &&
                    geometry->shape_kind != tc.expected_geometry_kind)
                {
                    pass = false;
                    reason += "geometry kind mismatch (actual=" + geometry->shape_kind +
                        ", expected=" + tc.expected_geometry_kind + "); ";
                }

                const int point_count = static_cast<int>(geometry->points.size() / 2);
                if (tc.expected_geometry_point_count >= 0 &&
                    point_count != tc.expected_geometry_point_count)
                {
                    pass = false;
                    reason += "geometry point count mismatch (actual=" +
                        std::to_string(point_count) + ", expected=" +
                        std::to_string(tc.expected_geometry_point_count) + "); ";
                }

                if (tc.expect_geometry_from_manifest)
                {
                    const double tolerance = 1.1;
                    if (request.has_ellipse_roi)
                    {
                        if (std::abs(geometry->center_x - request.ellipse_cx) > tolerance ||
                            std::abs(geometry->center_y - request.ellipse_cy) > tolerance ||
                            std::abs(geometry->radius_x - request.ellipse_rx) > tolerance ||
                            std::abs(geometry->radius_y - request.ellipse_ry) > tolerance)
                        {
                            pass = false;
                            reason += "ellipse geometry differs from manifest request; ";
                        }
                    }
                    else if (request.has_rotated_rect_roi)
                    {
                        if (std::abs(geometry->center_x - request.rect_cx) > tolerance ||
                            std::abs(geometry->center_y - request.rect_cy) > tolerance ||
                            std::abs(geometry->radius_x * 2.0 - request.rect_width) > tolerance ||
                            std::abs(geometry->radius_y * 2.0 - request.rect_height) > tolerance ||
                            AngleDifferenceDegrees(geometry->angle_deg,
                                                   request.rect_angle_deg) > 0.1)
                        {
                            pass = false;
                            reason += "rotated rect geometry differs from manifest request; ";
                        }
                    }
                }
            }
        }

        if (tc.operation == "runtime_geometry_publish")
        {
            if (projection.algorithm_ok)
            {
                pass = false;
                reason += "geometry-only case should not have algorithm_ok=true; ";
            }

            if (result_element_count > 0)
            {
                pass = false;
                reason += "geometry-only case should have result_element_count=0 (actual=" +
                    std::to_string(result_element_count) + "); ";
            }

            if (stale_result_count > 0)
            {
                pass = false;
                reason += "geometry-only case should have stale_result_count=0; ";
            }
        }

        if (tc.operation == "runtime_result_publish")
        {
            if (!projection.executed)
            {
                pass = false;
                reason += "algorithm was not executed; ";
            }

            if (!projection.publish_ok)
            {
                pass = false;
                reason += "runtime shape publication failed; ";
            }

            if (tc.expected_min_valid_points >= 0 &&
                projection.valid_points_count < tc.expected_min_valid_points)
            {
                pass = false;
                reason += "valid point count below expectation (actual=" +
                    std::to_string(projection.valid_points_count) +
                    ", expected_min=" +
                    std::to_string(tc.expected_min_valid_points) + "); ";
            }

            if (tc.expected_has_fit_line >= 0 &&
                projection.has_fit_line != (tc.expected_has_fit_line == 1))
            {
                pass = false;
                reason += "fit line expectation mismatch; ";
            }

            if (tc.expected_has_fit_circle >= 0 &&
                projection.has_fit_circle != (tc.expected_has_fit_circle == 1))
            {
                pass = false;
                reason += "fit circle expectation mismatch; ";
            }

            if (tc.expected_has_fit_ellipse >= 0 &&
                projection.has_fit_ellipse != (tc.expected_has_fit_ellipse == 1))
            {
                pass = false;
                reason += "fit ellipse expectation mismatch; ";
            }

            if (tc.expected_max_residual >= 0.0 &&
                projection.fit_residual > tc.expected_max_residual)
            {
                pass = false;
                reason += "fit residual exceeds expectation (actual=" +
                    std::to_string(projection.fit_residual) +
                    ", max=" +
                    std::to_string(tc.expected_max_residual) + "); ";
            }

            if (tc.expected_min_model_points >= 0 &&
                projection.model_point_count < tc.expected_min_model_points)
            {
                pass = false;
                reason += "model point count below expectation (actual=" +
                    std::to_string(projection.model_point_count) +
                    ", expected_min=" +
                    std::to_string(tc.expected_min_model_points) + "); ";
            }

            if (tc.expected_min_candidates >= 0 &&
                projection.candidate_count < tc.expected_min_candidates)
            {
                pass = false;
                reason += "candidate count below expectation (actual=" +
                    std::to_string(projection.candidate_count) +
                    ", expected_min=" +
                    std::to_string(tc.expected_min_candidates) + "); ";
            }

            if (tc.expected_min_best_score >= 0.0 &&
                projection.best_score < tc.expected_min_best_score)
            {
                pass = false;
                reason += "best score below expectation (actual=" +
                    std::to_string(projection.best_score) +
                    ", expected_min=" +
                    std::to_string(tc.expected_min_best_score) + "); ";
            }

            if (tc.expected_has_result_box >= 0 &&
                projection.has_result_box != (tc.expected_has_result_box == 1))
            {
                pass = false;
                reason += "result box expectation mismatch; ";
            }

            if (!projection.failure_stage.empty())
            {
                pass = false;
                reason += "failure_stage=" + projection.failure_stage +
                    ", reason=" + projection.reason + "; ";
            }
        }

        if (tc.operation == "runtime_result_publish")
        {
            if (stale_result_count > 0)
            {
                pass = false;
                reason += "published result contains stale elements; ";
            }

            for (const auto& elem : projection.published_shapes)
            {
                if (elem.owner_ref != projection.owner_ref)
                {
                    pass = false;
                    reason += "owner_ref mismatch: " + elem.stable_ref + "; ";
                }

                if (elem.owner_type != projection.owner_type)
                {
                    pass = false;
                    reason += "owner_type mismatch: " + elem.stable_ref + "; ";
                }
            }
        }

        const fs::path case_dir = fs::path(options.out_dir) / "cases" / case_result.case_id;
        fs::create_directories(case_dir);

        std::ofstream before_file(case_dir / "shape_elements_before.json");
        if (before_file)
        {
            before_file << "{\"elements\": []}\n";
        }

        std::ofstream after_file(case_dir / "shape_elements_after.json");
        if (after_file)
        {
            after_file << "{\n";
            after_file << "  \"elements\": [\n";
            for (size_t i = 0; i < projection.published_shapes.size(); ++i)
            {
                const auto& elem = projection.published_shapes[i];
                after_file << "    {\n";
                after_file << "      \"stable_ref\": \"" << EscapeJson(elem.stable_ref) << "\",\n";
                after_file << "      \"owner_type\": \"" << EscapeJson(elem.owner_type) << "\",\n";
                after_file << "      \"owner_ref\": \"" << EscapeJson(elem.owner_ref) << "\",\n";
                after_file << "      \"semantic_role\": \"" << EscapeJson(elem.semantic_role) << "\",\n";
                after_file << "      \"shape_kind\": \"" << EscapeJson(elem.shape_kind) << "\",\n";
                after_file << "      \"points\": [";
                for (size_t point_index = 0; point_index < elem.points.size(); ++point_index)
                {
                    if (point_index > 0) after_file << ", ";
                    after_file << elem.points[point_index];
                }
                after_file << "],\n";
                after_file << "      \"center_x\": " << elem.center_x << ",\n";
                after_file << "      \"center_y\": " << elem.center_y << ",\n";
                after_file << "      \"radius\": " << elem.radius << ",\n";
                after_file << "      \"radius_x\": " << elem.radius_x << ",\n";
                after_file << "      \"radius_y\": " << elem.radius_y << ",\n";
                after_file << "      \"angle_deg\": " << elem.angle_deg << ",\n";
                after_file << "      \"closed\": " << (elem.closed ? "true" : "false") << ",\n";
                after_file << "      \"editable\": " << (elem.editable ? "true" : "false") << ",\n";
                after_file << "      \"result_element\": " << (elem.result_element ? "true" : "false") << "\n";
                after_file << "    }";
                if (i < projection.published_shapes.size() - 1)
                    after_file << ",";
                after_file << "\n";
            }
            after_file << "  ]\n";
            after_file << "}\n";
        }

        std::ofstream projection_file(case_dir / "projection_summary.json");
        if (projection_file)
        {
            projection_file << "{\n";
            projection_file << "  \"owner_type\": \"" << EscapeJson(owner_type) << "\",\n";
            projection_file << "  \"owner_ref\": \"" << EscapeJson(owner_ref) << "\",\n";
            projection_file << "  \"roles\": {\n";
            projection_file << "    \"roi\": " << (role_counts.count("roi") ? role_counts["roi"] : 0) << ",\n";
            projection_file << "    \"scan\": " << (role_counts.count("scan") ? role_counts["scan"] : 0) << ",\n";
            projection_file << "    \"measure_points\": " << (role_counts.count("measure_points") ? role_counts["measure_points"] : 0) << ",\n";
            projection_file << "    \"result\": " << (role_counts.count("result") ? role_counts["result"] : 0) << "\n";
            projection_file << "  },\n";
            projection_file << "  \"editable_roi_count\": " << (editable_role_counts.count("roi") ? editable_role_counts["roi"] : 0) << ",\n";
            projection_file << "  \"editable_result_count\": " << ((role_counts.count("result") ? role_counts["result"] : 0) - (result_role_counts.count("result") ? result_role_counts["result"] : 0)) << ",\n";
            projection_file << "  \"stale_result_count\": " << stale_result_count << ",\n";
            projection_file << "  \"duplicate_stable_ref_count\": " << duplicate_stable_ref_count << "\n";
            projection_file << "}\n";
        }

        std::ofstream request_file(case_dir / "projection_request.json");
        if (request_file)
        {
            request_file << "{\n";
            request_file << "  \"case_id\": \"" << EscapeJson(request.case_id) << "\",\n";
            request_file << "  \"tool_id\": \"" << EscapeJson(request.tool_id) << "\",\n";
            request_file << "  \"owner_type\": \"" << EscapeJson(request.owner_type) << "\",\n";
            request_file << "  \"image_path\": \"" << EscapeJson(request.image_path) << "\",\n";
            if (request.has_ellipse_roi)
            {
                request_file << "  \"has_ellipse_roi\": true,\n";
                request_file << "  \"ellipse_cx\": " << request.ellipse_cx << ",\n";
                request_file << "  \"ellipse_cy\": " << request.ellipse_cy << ",\n";
                request_file << "  \"ellipse_rx\": " << request.ellipse_rx << ",\n";
                request_file << "  \"ellipse_ry\": " << request.ellipse_ry << ",\n";
                request_file << "  \"ellipse_angle_deg\": " << request.ellipse_angle_deg << ",\n";
            }
            if (request.has_rotated_rect_roi)
            {
                request_file << "  \"has_rotated_rect_roi\": true,\n";
                request_file << "  \"rect_cx\": " << request.rect_cx << ",\n";
                request_file << "  \"rect_cy\": " << request.rect_cy << ",\n";
                request_file << "  \"rect_width\": " << request.rect_width << ",\n";
                request_file << "  \"rect_height\": " << request.rect_height << ",\n";
                request_file << "  \"rect_angle_deg\": " << request.rect_angle_deg << ",\n";
            }
            if (!request.template_image_path.empty())
                request_file << "  \"template_image_path\": \"" << EscapeJson(request.template_image_path) << "\",\n";
            if (!request.test_image_path.empty())
                request_file << "  \"test_image_path\": \"" << EscapeJson(request.test_image_path) << "\",\n";
            if (request.has_learn_roi)
            {
                request_file << "  \"has_learn_roi\": true,\n";
                request_file << "  \"learn_roi\": {\n";
                request_file << "    \"x\": " << request.learn_roi.x << ",\n";
                request_file << "    \"y\": " << request.learn_roi.y << ",\n";
                request_file << "    \"width\": " << request.learn_roi.width << ",\n";
                request_file << "    \"height\": " << request.learn_roi.height << "\n";
                request_file << "  },\n";
            }
            if (request.has_search_roi)
            {
                request_file << "  \"has_search_roi\": true,\n";
                request_file << "  \"search_roi\": {\n";
                request_file << "    \"x\": " << request.search_roi.x << ",\n";
                request_file << "    \"y\": " << request.search_roi.y << ",\n";
                request_file << "    \"width\": " << request.search_roi.width << ",\n";
                request_file << "    \"height\": " << request.search_roi.height << "\n";
                request_file << "  },\n";
            }
            if (request.has_expected_rect)
            {
                request_file << "  \"has_expected_rect\": true,\n";
                request_file << "  \"expected_rect\": {\n";
                request_file << "    \"x\": " << request.expected_rect.x << ",\n";
                request_file << "    \"y\": " << request.expected_rect.y << ",\n";
                request_file << "    \"width\": " << request.expected_rect.width << ",\n";
                request_file << "    \"height\": " << request.expected_rect.height << "\n";
                request_file << "  },\n";
            }
            request_file << "  \"threshold\": " << request.threshold << ",\n";
            request_file << "  \"method\": " << request.method << "\n";
            request_file << "}\n";
        }

        std::string result_overlay_path;
        std::string evidence_overlay_path;
        std::string tool_display_path;
        int result_overlay_changed_pixels = 0;
        int evidence_overlay_changed_pixels = 0;
        int tool_display_changed_pixels = 0;
        int result_overlay_rendered_elements = 0;
        int evidence_overlay_rendered_elements = 0;
        int tool_display_rendered_elements = 0;
        std::string overlay_reason;

        cv::Mat source_image;
        if (!request.image_path.empty())
            source_image = cv::imread(request.image_path, cv::IMREAD_COLOR);

        if (!source_image.empty())
        {
            cv::Mat result_overlay;
            cv::Mat evidence_overlay;
            cv::Mat tool_display;
            CxOverlayRenderResult render_result;

            const fs::path result_path = case_dir / "result_overlay.png";
            if (RenderCxShapeOverlay(source_image, projection.published_shapes, CxOverlayLayer::RESULT, result_overlay, render_result))
            {
                result_overlay_changed_pixels = render_result.changed_pixel_count;
                result_overlay_rendered_elements = render_result.rendered_element_count;
                if (cv::imwrite(result_path.string(), result_overlay))
                    result_overlay_path = result_path.string();
            }
            else
            {
                overlay_reason += "result_overlay: " + render_result.reason + "; ";
            }

            const fs::path evidence_path = case_dir / "evidence_overlay.png";
            if (RenderCxShapeOverlay(source_image, projection.published_shapes, CxOverlayLayer::EVIDENCE, evidence_overlay, render_result))
            {
                evidence_overlay_changed_pixels = render_result.changed_pixel_count;
                evidence_overlay_rendered_elements = render_result.rendered_element_count;
                if (cv::imwrite(evidence_path.string(), evidence_overlay))
                    evidence_overlay_path = evidence_path.string();
            }
            else
            {
                overlay_reason += "evidence_overlay: " + render_result.reason + "; ";
            }

            const fs::path tool_display_out = case_dir / "tool_display.png";
            if (RenderCxShapeOverlay(source_image, projection.published_shapes, CxOverlayLayer::TOOL_DISPLAY, tool_display, render_result))
            {
                tool_display_changed_pixels = render_result.changed_pixel_count;
                tool_display_rendered_elements = render_result.rendered_element_count;
                if (cv::imwrite(tool_display_out.string(), tool_display))
                    tool_display_path = tool_display_out.string();
            }
            else
            {
                overlay_reason += "tool_display: " + render_result.reason + "; ";
            }
        }
        else
        {
            overlay_reason = "source image unavailable for overlay: " + request.image_path;
        }

        std::ofstream overlay_file(case_dir / "overlay_validation.json");
        if (overlay_file)
        {
            overlay_file << "{\n";
            overlay_file << "  \"source_image_path\": \"" << EscapeJson(request.image_path) << "\",\n";
            overlay_file << "  \"result_overlay_path\": \"" << EscapeJson(result_overlay_path) << "\",\n";
            overlay_file << "  \"evidence_overlay_path\": \"" << EscapeJson(evidence_overlay_path) << "\",\n";
            overlay_file << "  \"tool_display_path\": \"" << EscapeJson(tool_display_path) << "\",\n";
            overlay_file << "  \"result_overlay_changed_pixels\": " << result_overlay_changed_pixels << ",\n";
            overlay_file << "  \"evidence_overlay_changed_pixels\": " << evidence_overlay_changed_pixels << ",\n";
            overlay_file << "  \"tool_display_changed_pixels\": " << tool_display_changed_pixels << ",\n";
            overlay_file << "  \"result_overlay_rendered_elements\": " << result_overlay_rendered_elements << ",\n";
            overlay_file << "  \"evidence_overlay_rendered_elements\": " << evidence_overlay_rendered_elements << ",\n";
            overlay_file << "  \"tool_display_rendered_elements\": " << tool_display_rendered_elements << ",\n";
            overlay_file << "  \"reason\": \"" << EscapeJson(overlay_reason) << "\"\n";
            overlay_file << "}\n";
        }

        std::ofstream result_file(case_dir / "projection_result.json");
        if (result_file)
        {
            result_file << "{\n";
            result_file << "  \"pass\": " << (pass ? "true" : "false") << ",\n";
            result_file << "  \"executed\": " << (projection.executed ? "true" : "false") << ",\n";
            result_file << "  \"algorithm_ok\": " << (projection.algorithm_ok ? "true" : "false") << ",\n";
            result_file << "  \"valid_points_count\": " << projection.valid_points_count << ",\n";
            result_file << "  \"result_element_count\": " << result_element_count << ",\n";
            result_file << "  \"stale_result_count\": " << stale_result_count << ",\n";
            result_file << "  \"duplicate_stable_ref_count\": " << duplicate_stable_ref_count << ",\n";
            result_file << "  \"result_overlay_path\": \"" << EscapeJson(result_overlay_path) << "\",\n";
            result_file << "  \"evidence_overlay_path\": \"" << EscapeJson(evidence_overlay_path) << "\",\n";
            result_file << "  \"tool_display_path\": \"" << EscapeJson(tool_display_path) << "\",\n";
            result_file << "  \"result_overlay_changed_pixels\": " << result_overlay_changed_pixels << ",\n";
            result_file << "  \"evidence_overlay_changed_pixels\": " << evidence_overlay_changed_pixels << ",\n";
            result_file << "  \"tool_display_changed_pixels\": " << tool_display_changed_pixels << ",\n";
            result_file << "  \"published_shapes\": [\n";
            for (size_t i = 0; i < projection.published_shapes.size(); ++i)
            {
                const auto& shape = projection.published_shapes[i];
                result_file << "    {\n";
                result_file << "      \"stable_ref\": \"" << EscapeJson(shape.stable_ref) << "\",\n";
                result_file << "      \"owner_type\": \"" << EscapeJson(shape.owner_type) << "\",\n";
                result_file << "      \"owner_ref\": \"" << EscapeJson(shape.owner_ref) << "\",\n";
                result_file << "      \"semantic_role\": \"" << EscapeJson(shape.semantic_role) << "\",\n";
                result_file << "      \"shape_kind\": \"" << EscapeJson(shape.shape_kind) << "\",\n";
                result_file << "      \"points\": [";
                for (size_t point_index = 0; point_index < shape.points.size(); ++point_index)
                {
                    if (point_index > 0) result_file << ", ";
                    result_file << shape.points[point_index];
                }
                result_file << "],\n";
                result_file << "      \"center_x\": " << shape.center_x << ",\n";
                result_file << "      \"center_y\": " << shape.center_y << ",\n";
                result_file << "      \"radius\": " << shape.radius << ",\n";
                result_file << "      \"radius_x\": " << shape.radius_x << ",\n";
                result_file << "      \"radius_y\": " << shape.radius_y << ",\n";
                result_file << "      \"angle_deg\": " << shape.angle_deg << ",\n";
                result_file << "      \"editable\": " << (shape.editable ? "true" : "false") << ",\n";
                result_file << "      \"result_element\": " << (shape.result_element ? "true" : "false") << "\n";
                result_file << "    }" << (i + 1 < projection.published_shapes.size() ? "," : "") << "\n";
            }
            result_file << "  ]\n";
            result_file << "}\n";
        }

        case_result.pass = pass;
        case_result.conclusion = pass ? "runtime_publish_verified" : "runtime_publish_incomplete";
        case_result.status = pass ? "PASS" : "FAIL";
        case_result.reason = pass ? "all required roles published" : reason;
        case_result.runtime_writeback = false;
        case_result.acceptance_scope = "RUNTIME_PROJECTION";
        case_result.owner_type = owner_type;
        case_result.shape_count_before = 0;
        case_result.shape_count_after = static_cast<int>(projection.published_shapes.size());
        case_result.shape_count_delta = static_cast<int>(projection.published_shapes.size());
    }
    else if (tc.operation == "gui_pointer_create" ||
             tc.operation == "gui_pointer_drag_existing" ||
             tc.operation == "gui_pointer_select_existing")
    {
        case_result.hit_test_pass = false;
        case_result.drag_pass = false;
        case_result.commit_pass = false;

        ViewController viewer;
        std::string reason;

        if (!viewer.TestApplyAnnotationToolManifestSnapshot(tool_manifest, reason))
        {
            case_result.status = "FAIL";
            case_result.conclusion = "manifest_apply_failed";
            case_result.reason = "failed to apply manifest snapshot to viewer: " + reason;
            return false;
        }

        viewer.TestEnableAnnotationCreateMode();

        if (!tc.tool_id.empty())
        {
            if (!viewer.TestSetActiveAnnotationTool(tc.tool_id, reason))
            {
                case_result.status = "FAIL";
                case_result.conclusion = "tool_not_found";
                case_result.reason = "failed to set active tool: " + reason;
                return false;
            }
        }

        if (tc.operation == "gui_pointer_drag_existing" || tc.operation == "gui_pointer_select_existing")
        {
            viewer.TestEnableAnnotationCreateMode();
            CxImagePointerFrame frame_init;
            frame_init.canvas_hovered = true;
            frame_init.inside_image = true;
            frame_init.left_clicked = true;
            frame_init.left_down = true;
            frame_init.image_x = tc.from_x - 40.0;
            frame_init.image_y = tc.from_y - 20.0;
            viewer.ProcessImageAnnotationPointerFrame(frame_init);
            CxImagePointerFrame frame_init_drag;
            frame_init_drag.canvas_hovered = true;
            frame_init_drag.inside_image = true;
            frame_init_drag.left_down = true;
            frame_init_drag.image_x = tc.from_x + 40.0;
            frame_init_drag.image_y = tc.from_y + 20.0;
            viewer.ProcessImageAnnotationPointerFrame(frame_init_drag);
            CxImagePointerFrame frame_init2;
            frame_init2.canvas_hovered = true;
            frame_init2.inside_image = true;
            frame_init2.left_released = true;
            frame_init2.image_x = tc.from_x + 40.0;
            frame_init2.image_y = tc.from_y + 20.0;
            viewer.ProcessImageAnnotationPointerFrame(frame_init2);

            const size_t shape_count_after_create = viewer.TestShapeElementCount();
            CXLOG_INFO("CxShapeInteractionRunner", "shape_case_info", "info", "case=" + tc.case_id + ", shapes_after_create=" + std::to_string(shape_count_after_create));

            viewer.TestSetToolModePointerPan();
        }
        else
        {
            if (tc.tool_id.empty())
            {
                viewer.TestSetToolModePointerPan();
            }
            else
            {
                viewer.TestEnableAnnotationCreateMode();
            }
        }

        const size_t initial_count = viewer.TestShapeElementCount();
        case_result.shape_count_before = static_cast<int>(initial_count);

        if (tc.pointer_sequence == "click")
        {
            CxImagePointerFrame frame;
            frame.canvas_hovered = true;
            frame.inside_image = true;
            frame.left_clicked = true;
            frame.image_x = tc.from_x;
            frame.image_y = tc.from_y;

            CxImagePointerResult r = viewer.ProcessImageAnnotationPointerFrame(frame);
            case_result.actual_handle = r.phase;
            case_result.status = r.status;
            case_result.reason = r.reason;

            CxShapeInteractionPointerEvent evt;
            evt.event = "left_down";
            evt.image_x = tc.from_x;
            evt.image_y = tc.from_y;
            evt.canvas_hovered = true;
            evt.inside_image = true;
            evt.phase = r.phase;
            evt.status = r.status;
            case_result.pointer_events.push_back(evt);

            if (tc.expected_phase.empty() || r.phase == tc.expected_phase)
            {
                if (tc.expected_status.empty() || r.status == tc.expected_status)
                {
                    case_result.pass = true;
                    case_result.conclusion = "pointer_operation_completed";
                }
                else
                {
                    case_result.pass = false;
                    case_result.conclusion = "status_mismatch";
                    case_result.reason = "expected status=" + tc.expected_status + ", actual=" + r.status;
                }
            }
            else
            {
                case_result.pass = false;
                case_result.conclusion = "phase_mismatch";
                case_result.reason = "expected phase=" + tc.expected_phase + ", actual=" + r.phase;
            }
        }
        else if (tc.pointer_sequence == "drag_release")
        {
            CxImagePointerFrame frame_down;
            frame_down.canvas_hovered = true;
            frame_down.inside_image = true;
            frame_down.left_clicked = true;
            frame_down.left_down = true;
            frame_down.image_x = tc.from_x;
            frame_down.image_y = tc.from_y;

            CxImagePointerResult r_down = viewer.ProcessImageAnnotationPointerFrame(frame_down);

            CxShapeInteractionPointerEvent evt_down;
            evt_down.event = "left_down";
            evt_down.image_x = tc.from_x;
            evt_down.image_y = tc.from_y;
            evt_down.canvas_hovered = true;
            evt_down.inside_image = true;
            evt_down.phase = r_down.phase;
            evt_down.status = r_down.status;
            case_result.pointer_events.push_back(evt_down);

            CxImagePointerFrame frame_drag;
            frame_drag.canvas_hovered = true;
            frame_drag.inside_image = true;
            frame_drag.left_down = true;
            frame_drag.pointer_moved = true;
            frame_drag.image_x = tc.to_x;
            frame_drag.image_y = tc.to_y;

            CxImagePointerResult r_drag = viewer.ProcessImageAnnotationPointerFrame(frame_drag);

            CxShapeInteractionPointerEvent evt_drag;
            evt_drag.event = "left_drag";
            evt_drag.image_x = tc.to_x;
            evt_drag.image_y = tc.to_y;
            evt_drag.canvas_hovered = true;
            evt_drag.inside_image = true;
            evt_drag.phase = r_drag.phase;
            evt_drag.status = r_drag.status;
            case_result.pointer_events.push_back(evt_drag);

            CxImagePointerFrame frame_release;
            frame_release.canvas_hovered = true;
            frame_release.inside_image = true;
            frame_release.left_released = true;
            frame_release.image_x = tc.to_x;
            frame_release.image_y = tc.to_y;

            CxImagePointerResult r = viewer.ProcessImageAnnotationPointerFrame(frame_release);
            case_result.actual_handle = r.phase;
            case_result.status = r.status;
            case_result.reason = r.reason;

            CxShapeInteractionPointerEvent evt_release;
            evt_release.event = "left_released";
            evt_release.image_x = tc.to_x;
            evt_release.image_y = tc.to_y;
            evt_release.canvas_hovered = true;
            evt_release.inside_image = true;
            evt_release.phase = r.phase;
            evt_release.status = r.status;
            case_result.pointer_events.push_back(evt_release);

            if (tc.expected_phase.empty() || r.phase == tc.expected_phase)
            {
                if (tc.expected_status.empty() || r.status == tc.expected_status)
                {
                    case_result.pass = true;
                    case_result.conclusion = "pointer_operation_completed";
                }
                else
                {
                    case_result.pass = false;
                    case_result.conclusion = "status_mismatch";
                    case_result.reason = "expected status=" + tc.expected_status + ", actual=" + r.status;
                }
            }
            else
            {
                case_result.pass = false;
                case_result.conclusion = "phase_mismatch";
                case_result.reason = "expected phase=" + tc.expected_phase + ", actual=" + r.phase;
            }
        }
        else if (tc.pointer_sequence == "polyline_finish")
        {
            CxImagePointerFrame frame1;
            frame1.canvas_hovered = true;
            frame1.inside_image = true;
            frame1.left_clicked = true;
            frame1.image_x = tc.from_x;
            frame1.image_y = tc.from_y;
            viewer.ProcessImageAnnotationPointerFrame(frame1);

            CxImagePointerFrame frame2;
            frame2.canvas_hovered = true;
            frame2.inside_image = true;
            frame2.left_clicked = true;
            frame2.image_x = tc.to_x;
            frame2.image_y = tc.to_y;
            viewer.ProcessImageAnnotationPointerFrame(frame2);

            CxImagePointerFrame frame_commit;
            frame_commit.canvas_hovered = true;
            frame_commit.inside_image = true;
            frame_commit.right_clicked = true;
            viewer.ProcessImageAnnotationPointerFrame(frame_commit);

            CxImagePointerResult r;
            viewer.TestGetLastPointerResult(r);
            case_result.actual_handle = r.phase;
            case_result.status = r.status;
            case_result.reason = r.reason;

            if (tc.expected_phase.empty() || r.phase == tc.expected_phase)
            {
                if (tc.expected_status.empty() || r.status == tc.expected_status)
                {
                    case_result.pass = true;
                    case_result.conclusion = "pointer_operation_completed";
                }
                else
                {
                    case_result.pass = false;
                    case_result.conclusion = "status_mismatch";
                    case_result.reason = "expected status=" + tc.expected_status + ", actual=" + r.status;
                }
            }
            else
            {
                case_result.pass = false;
                case_result.conclusion = "phase_mismatch";
                case_result.reason = "expected phase=" + tc.expected_phase + ", actual=" + r.phase;
            }
        }

        const size_t final_count = viewer.TestShapeElementCount();
        case_result.shape_count_after = static_cast<int>(final_count);
        const int actual_delta = static_cast<int>(final_count - initial_count);
        case_result.shape_count_delta = actual_delta;
        case_result.created_points_count = actual_delta;

        if (actual_delta != tc.expected_shape_count_delta)
        {
            case_result.pass = false;
            case_result.conclusion = "shape_count_delta_mismatch";
            case_result.reason = "expected delta=" + std::to_string(tc.expected_shape_count_delta) +
                                 ", actual=" + std::to_string(actual_delta);
        }

        CxImagePointerResult r;
        viewer.TestGetLastPointerResult(r);
        case_result.created_ref = r.created_ref;
        case_result.created_shape_kind = viewer.TestShapeKindByRef(r.created_ref);
        case_result.commit_result = r.commit;

        if (!tc.expected_created_kind.empty() && case_result.created_shape_kind != tc.expected_created_kind)
        {
            case_result.pass = false;
            case_result.conclusion = "created_shape_kind_mismatch";
            case_result.reason = "expected created_kind=" + tc.expected_created_kind +
                                 ", actual=" + case_result.created_shape_kind;
        }

        case_result.runtime_writeback = false;
        case_result.acceptance_scope = "GUI_POINTER";
    }
    else
    {
        case_result.pass = false;
        case_result.conclusion = "unsupported_operation";
        case_result.status = "FAIL";
        case_result.reason = "unsupported operation: " + tc.operation;
    }

    return case_result.pass;
}

bool CxShapeInteractionRunner::VerifyGeometryAssertion(
    const std::string& assertion,
    const CxShapeGeometrySnapshot& before,
    const CxShapeGeometrySnapshot& after,
    std::string& reason)
{
    const double epsilon = 1.0;

    if (assertion == "point_visible")
    {
        if (after.points.size() >= 1)
        {
            reason = "point exists";
            return true;
        }
        reason = "no points in shape";
        return false;
    }

    if (assertion == "target_moved_only")
    {
        if (before.points.size() != after.points.size())
        {
            reason = "point count changed";
            return false;
        }
        bool moved = false;
        for (size_t i = 0; i < before.points.size(); ++i)
        {
            const double dx = after.points[i].x - before.points[i].x;
            const double dy = after.points[i].y - before.points[i].y;
            if (std::abs(dx) > epsilon || std::abs(dy) > epsilon)
            {
                if (moved)
                {
                    reason = "multiple points moved";
                    return false;
                }
                moved = true;
            }
        }
        if (moved)
        {
            reason = "single point moved";
            return true;
        }
        reason = "no points moved";
        return false;
    }

    if (assertion == "start_moved_end_unchanged")
    {
        if (before.points.size() >= 2 && after.points.size() >= 2)
        {
            const bool start_moved = std::abs(after.points[0].x - before.points[0].x) > epsilon ||
                                     std::abs(after.points[0].y - before.points[0].y) > epsilon;
            const bool end_unchanged = std::abs(after.points[1].x - before.points[1].x) <= epsilon &&
                                       std::abs(after.points[1].y - before.points[1].y) <= epsilon;
            if (start_moved && end_unchanged)
            {
                reason = "start moved, end unchanged";
                return true;
            }
            reason = "start not moved or end changed";
            return false;
        }
        reason = "not enough points";
        return false;
    }

    if (assertion == "translate_only")
    {
        if (before.points.size() != after.points.size())
        {
            reason = "point count changed";
            return false;
        }
        double dx = 0, dy = 0;
        bool first = true;
        for (size_t i = 0; i < before.points.size(); ++i)
        {
            const double cdx = after.points[i].x - before.points[i].x;
            const double cdy = after.points[i].y - before.points[i].y;
            if (first)
            {
                dx = cdx;
                dy = cdy;
                first = false;
            }
            else
            {
                if (std::abs(cdx - dx) > epsilon || std::abs(cdy - dy) > epsilon)
                {
                    reason = "translation not uniform";
                    return false;
                }
            }
        }
        if (std::abs(dx) > epsilon || std::abs(dy) > epsilon)
        {
            reason = "uniform translation";
            return true;
        }
        reason = "no translation";
        return false;
    }

    if (assertion == "outer_radius_only")
    {
        if (std::abs(after.center.x - before.center.x) <= epsilon &&
            std::abs(after.center.y - before.center.y) <= epsilon &&
            std::abs(after.inner_radius - before.inner_radius) <= epsilon &&
            std::abs(after.radius - before.radius) > epsilon)
        {
            reason = "outer radius changed only";
            return true;
        }
        reason = "center, inner radius changed, or outer radius unchanged";
        return false;
    }

    if (assertion == "end_moved_start_unchanged")
    {
        if (before.points.size() >= 2 && after.points.size() >= 2)
        {
            const bool end_moved = std::abs(after.points[1].x - before.points[1].x) > epsilon ||
                                   std::abs(after.points[1].y - before.points[1].y) > epsilon;
            const bool start_unchanged = std::abs(after.points[0].x - before.points[0].x) <= epsilon &&
                                         std::abs(after.points[0].y - before.points[0].y) <= epsilon;
            if (end_moved && start_unchanged)
            {
                reason = "end moved, start unchanged";
                return true;
            }
            reason = "end not moved or start changed";
            return false;
        }
        reason = "not enough points";
        return false;
    }

    if (assertion == "corner0_moved_others_unchanged")
    {
        if (before.points.size() >= 4 && after.points.size() >= 4)
        {
            const bool corner0_moved = std::abs(after.points[0].x - before.points[0].x) > epsilon ||
                                       std::abs(after.points[0].y - before.points[0].y) > epsilon;
            bool others_unchanged = true;
            for (size_t i = 1; i < 4 && i < before.points.size(); ++i)
            {
                if (std::abs(after.points[i].x - before.points[i].x) > epsilon ||
                    std::abs(after.points[i].y - before.points[i].y) > epsilon)
                {
                    others_unchanged = false;
                    break;
                }
            }
            if (corner0_moved && others_unchanged)
            {
                reason = "corner0 moved, others unchanged";
                return true;
            }
            reason = "corner0 not moved or other corners changed";
            return false;
        }
        reason = "not enough points";
        return false;
    }

    if (assertion == "vertex0_moved_others_unchanged")
    {
        if (before.points.size() >= 1 && after.points.size() >= 1)
        {
            const bool vertex0_moved = std::abs(after.points[0].x - before.points[0].x) > epsilon ||
                                       std::abs(after.points[0].y - before.points[0].y) > epsilon;
            bool others_unchanged = true;
            for (size_t i = 1; i < before.points.size(); ++i)
            {
                if (std::abs(after.points[i].x - before.points[i].x) > epsilon ||
                    std::abs(after.points[i].y - before.points[i].y) > epsilon)
                {
                    others_unchanged = false;
                    break;
                }
            }
            if (vertex0_moved && others_unchanged)
            {
                reason = "vertex0 moved, others unchanged";
                return true;
            }
            reason = "vertex0 not moved or other vertices changed";
            return false;
        }
        reason = "not enough points";
        return false;
    }

    if (assertion == "half_width_increased")
    {
        if (after.half_width > before.half_width + epsilon)
        {
            reason = "half_width increased";
            return true;
        }
        reason = "half_width not increased";
        return false;
    }

    if (assertion == "half_width_decreased")
    {
        if (after.half_width < before.half_width - epsilon)
        {
            reason = "half_width decreased";
            return true;
        }
        reason = "half_width not decreased";
        return false;
    }

    if (assertion == "radius_x_only")
    {
        if (std::abs(after.center.x - before.center.x) <= epsilon &&
            std::abs(after.center.y - before.center.y) <= epsilon &&
            std::abs(after.radius_y - before.radius_y) <= epsilon &&
            std::abs(after.radius_x - before.radius_x) > epsilon)
        {
            reason = "radius_x changed only";
            return true;
        }
        reason = "center, radius_y changed, or radius_x unchanged";
        return false;
    }

    if (assertion == "radius_y_only")
    {
        if (std::abs(after.center.x - before.center.x) <= epsilon &&
            std::abs(after.center.y - before.center.y) <= epsilon &&
            std::abs(after.radius_x - before.radius_x) <= epsilon &&
            std::abs(after.radius_y - before.radius_y) > epsilon)
        {
            reason = "radius_y changed only";
            return true;
        }
        reason = "center, radius_x changed, or radius_y unchanged";
        return false;
    }

    if (assertion == "ellipse_translate_only")
    {
        if (std::abs(after.radius_x - before.radius_x) <= epsilon &&
            std::abs(after.radius_y - before.radius_y) <= epsilon &&
            (std::abs(after.center.x - before.center.x) > epsilon ||
             std::abs(after.center.y - before.center.y) > epsilon))
        {
            reason = "ellipse translated only";
            return true;
        }
        reason = "radii changed or no translation";
        return false;
    }

    if (assertion == "radius_x_minimum")
    {
        if (std::abs(after.radius_x - before.radius_x) <= epsilon &&
            after.radius_x <= 1.5)
        {
            reason = "radius_x at minimum";
            return true;
        }
        reason = "radius_x changed or not at minimum";
        return false;
    }

    if (assertion == "radius_y_minimum")
    {
        if (std::abs(after.radius_y - before.radius_y) <= epsilon &&
            after.radius_y <= 1.5)
        {
            reason = "radius_y at minimum";
            return true;
        }
        reason = "radius_y changed or not at minimum";
        return false;
    }

    if (assertion == "no_hit")
    {
        reason = "no hit expected, no geometry check needed";
        return true;
    }

    if (assertion == "drag_rejected_noneditable")
    {
        reason = "drag rejected for non-editable shape";
        return true;
    }

    reason = "unknown assertion: " + assertion;
    return false;
}

static std::string EscapeJson(const std::string& s)
{
    std::ostringstream oss;
    for (char c : s)
    {
        switch (c)
        {
        case '\\': oss << "\\\\"; break;
        case '"': oss << "\\\""; break;
        case '\n': oss << "\\n"; break;
        case '\r': oss << "\\r"; break;
        case '\t': oss << "\\t"; break;
        default:
            if (c >= 0 && c < 0x20)
                oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)c;
            else
                oss << c;
            break;
        }
    }
    return oss.str();
}

void CxShapeInteractionRunner::GenerateCaseOutput(
    const CxShapeInteractionCaseResultEx& case_result,
    const CxShapeInteractionTrace& trace,
    const CxShapeInteractionOptions& options,
    const std::string& out_dir)
{
    const fs::path case_dir = fs::path(out_dir) / "cases" / case_result.case_id;
    fs::create_directories(case_dir);

    std::ofstream conclusion_file(case_dir / "conclusion.json");
    if (conclusion_file)
    {
        conclusion_file << "{\n";
        conclusion_file << "  \"run_id\": \"" << EscapeJson(options.run_id) << "\",\n";
        conclusion_file << "  \"case_id\": \"" << EscapeJson(case_result.case_id) << "\",\n";
        conclusion_file << "  \"tool_id\": \"" << EscapeJson(case_result.tool_id) << "\",\n";
        conclusion_file << "  \"operation\": \"" << EscapeJson(case_result.operation) << "\",\n";
        conclusion_file << "  \"shape_kind\": \"" << EscapeJson(case_result.shape_kind) << "\",\n";
        conclusion_file << "  \"expected_handle\": \"" << EscapeJson(case_result.expected_handle) << "\",\n";
        conclusion_file << "  \"actual_handle\": \"" << EscapeJson(case_result.actual_handle) << "\",\n";
        conclusion_file << "  \"expected_vertex\": " << case_result.expected_vertex << ",\n";
        conclusion_file << "  \"actual_vertex\": " << case_result.actual_vertex << ",\n";
        conclusion_file << "  \"geometry_assertion\": \"" << EscapeJson(case_result.geometry_assertion) << "\",\n";
        conclusion_file << "  \"hit_test_pass\": " << (case_result.hit_test_pass ? "true" : "false") << ",\n";
        conclusion_file << "  \"drag_pass\": " << (case_result.drag_pass ? "true" : "false") << ",\n";
        conclusion_file << "  \"commit_pass\": " << (case_result.commit_pass ? "true" : "false") << ",\n";
        conclusion_file << "  \"render_pass\": " << (case_result.render_pass ? "true" : "false") << ",\n";
        conclusion_file << "  \"runtime_writeback\": " << (case_result.runtime_writeback ? "true" : "false") << ",\n";
        conclusion_file << "  \"acceptance_scope\": \"" << EscapeJson(case_result.acceptance_scope) << "\",\n";
        conclusion_file << "  \"status\": \"" << EscapeJson(case_result.status) << "\",\n";
        conclusion_file << "  \"reason\": \"" << EscapeJson(case_result.reason) << "\",\n";
        conclusion_file << "  \"pass\": " << (case_result.pass ? "true" : "false") << ",\n";
        conclusion_file << "  \"conclusion\": \"" << EscapeJson(case_result.conclusion) << "\",\n";
        conclusion_file << "  \"created_points_count\": " << case_result.created_points_count << ",\n";
        conclusion_file << "  \"created_handle_count\": " << case_result.created_handle_count << ",\n";
        conclusion_file << "  \"shape_visible\": " << (case_result.shape_visible ? "true" : "false") << ",\n";
        conclusion_file << "  \"shape_editable\": " << (case_result.shape_editable ? "true" : "false") << ",\n";
        conclusion_file << "  \"created_shape_kind\": \"" << EscapeJson(case_result.created_shape_kind) << "\",\n";
        conclusion_file << "  \"created_ref\": \"" << EscapeJson(case_result.created_ref) << "\",\n";
        conclusion_file << "  \"selected_ref\": \"" << EscapeJson(case_result.selected_ref) << "\",\n";
        conclusion_file << "  \"shape_count_before\": " << case_result.shape_count_before << ",\n";
        conclusion_file << "  \"shape_count_after\": " << case_result.shape_count_after << ",\n";
        conclusion_file << "  \"shape_count_delta\": " << case_result.shape_count_delta << ",\n";
        conclusion_file << "  \"pointer_events\": [\n";
        for (size_t i = 0; i < case_result.pointer_events.size(); ++i)
        {
            const auto& evt = case_result.pointer_events[i];
            conclusion_file << "    {\n";
            conclusion_file << "      \"event\": \"" << EscapeJson(evt.event) << "\",\n";
            conclusion_file << "      \"screen_x\": " << evt.screen_x << ",\n";
            conclusion_file << "      \"screen_y\": " << evt.screen_y << ",\n";
            conclusion_file << "      \"image_x\": " << evt.image_x << ",\n";
            conclusion_file << "      \"image_y\": " << evt.image_y << ",\n";
            conclusion_file << "      \"canvas_hovered\": " << (evt.canvas_hovered ? "true" : "false") << ",\n";
            conclusion_file << "      \"inside_image\": " << (evt.inside_image ? "true" : "false") << ",\n";
            conclusion_file << "      \"phase\": \"" << EscapeJson(evt.phase) << "\",\n";
            conclusion_file << "      \"status\": \"" << EscapeJson(evt.status) << "\"\n";
            conclusion_file << "    }";
            if (i < case_result.pointer_events.size() - 1)
                conclusion_file << ",";
            conclusion_file << "\n";
        }
        conclusion_file << "  ],\n";
        conclusion_file << "  \"commit_result\": {\n";
        conclusion_file << "    \"committed\": " << (case_result.commit_result.committed ? "true" : "false") << ",\n";
        conclusion_file << "    \"reason\": \"" << EscapeJson(case_result.commit_result.reason) << "\"\n";
        conclusion_file << "  }\n";
        conclusion_file << "}\n";
    }

    std::ofstream trace_file(case_dir / "interaction_trace.json");
    if (trace_file)
    {
        trace_file << "{\n";
        trace_file << "  \"case_id\": \"" << EscapeJson(case_result.case_id) << "\",\n";
        trace_file << "  \"events\": [\n";
        if (!trace.pointer_events.empty())
        {
            for (size_t i = 0; i < trace.pointer_events.size(); ++i)
            {
                const auto& evt = trace.pointer_events[i];
                std::string type_str;
                switch (evt.type)
                {
                case CxPointerEvent::Type::Move: type_str = "Move"; break;
                case CxPointerEvent::Type::LeftDown: type_str = "LeftDown"; break;
                case CxPointerEvent::Type::LeftDrag: type_str = "LeftDrag"; break;
                case CxPointerEvent::Type::LeftUp: type_str = "LeftUp"; break;
                }
                trace_file << "    {\n";
                trace_file << "      \"step\": " << evt.frame_no << ",\n";
                trace_file << "      \"type\": \"" << type_str << "\",\n";
                trace_file << "      \"image_x\": " << evt.image_x << ",\n";
                trace_file << "      \"image_y\": " << evt.image_y << "\n";
                trace_file << "    }";
                if (i < trace.pointer_events.size() - 1)
                    trace_file << ",";
                trace_file << "\n";
            }
        }
        else
        {
            for (size_t i = 0; i < case_result.pointer_events.size(); ++i)
            {
                const auto& evt = case_result.pointer_events[i];
                trace_file << "    {\n";
                trace_file << "      \"step\": " << i << ",\n";
                trace_file << "      \"type\": \"" << EscapeJson(evt.event) << "\",\n";
                trace_file << "      \"screen_x\": " << evt.screen_x << ",\n";
                trace_file << "      \"screen_y\": " << evt.screen_y << ",\n";
                trace_file << "      \"image_x\": " << evt.image_x << ",\n";
                trace_file << "      \"image_y\": " << evt.image_y << ",\n";
                trace_file << "      \"phase\": \"" << EscapeJson(evt.phase) << "\",\n";
                trace_file << "      \"status\": \"" << EscapeJson(evt.status) << "\"\n";
                trace_file << "    }";
                if (i < case_result.pointer_events.size() - 1)
                    trace_file << ",";
                trace_file << "\n";
            }
        }
        trace_file << "  ],\n";
        trace_file << "  \"hits\": [\n";
        for (size_t i = 0; i < trace.hits.size(); ++i)
        {
            const auto& hit = trace.hits[i];
            trace_file << "    {\n";
            trace_file << "      \"hit\": " << (hit.hit ? "true" : "false") << ",\n";
            trace_file << "      \"element_index\": " << hit.element_index << ",\n";
            trace_file << "      \"role\": \"" << HandleName(hit.shape_hit.role) << "\",\n";
            trace_file << "      \"vertex_index\": " << hit.shape_hit.vertex_index << ",\n";
            trace_file << "      \"distance\": " << hit.shape_hit.distance << "\n";
            trace_file << "    }";
            if (i < trace.hits.size() - 1)
                trace_file << ",";
            trace_file << "\n";
        }
        trace_file << "  ],\n";
        trace_file << "  \"commit_reason\": \"" << EscapeJson(trace.commit_reason) << "\",\n";
        trace_file << "  \"commit_result\": {\n";
        trace_file << "    \"committed\": " << (trace.commit_result.committed ? "true" : "false") << ",\n";
        trace_file << "    \"owner_type\": \"" << EscapeJson(trace.commit_result.owner_type) << "\",\n";
        trace_file << "    \"owner_ref\": \"" << EscapeJson(trace.commit_result.owner_ref) << "\",\n";
        trace_file << "    \"owner_binding\": \"" << EscapeJson(trace.commit_result.owner_binding) << "\",\n";
        trace_file << "    \"result_marked_stale\": " << (trace.commit_result.result_marked_stale ? "true" : "false") << ",\n";
        trace_file << "    \"stale_result_count\": " << trace.commit_result.stale_result_count << "\n";
        trace_file << "  }\n";
        trace_file << "}\n";
    }

    if (!trace.snapshots.empty())
    {
        std::ofstream before_file(case_dir / "geometry_before.json");
        if (before_file)
        {
            const auto& snap = trace.snapshots.front();
            before_file << "{\n";
            before_file << "  \"points\": [\n";
            for (size_t i = 0; i < snap.points.size(); ++i)
            {
                before_file << "    {\"x\": " << snap.points[i].x << ", \"y\": " << snap.points[i].y << "}";
                if (i < snap.points.size() - 1)
                    before_file << ",";
                before_file << "\n";
            }
            before_file << "  ],\n";
            before_file << "  \"center\": {\"x\": " << snap.center.x << ", \"y\": " << snap.center.y << "},\n";
            before_file << "  \"radius\": " << snap.radius << ",\n";
            before_file << "  \"inner_radius\": " << snap.inner_radius << ",\n";
            before_file << "  \"half_width\": " << snap.half_width << ",\n";
            before_file << "  \"closed\": " << (snap.closed ? "true" : "false") << "\n";
            before_file << "}\n";
        }

        std::ofstream after_file(case_dir / "geometry_after.json");
        if (after_file)
        {
            const auto& snap = trace.snapshots.back();
            after_file << "{\n";
            after_file << "  \"points\": [\n";
            for (size_t i = 0; i < snap.points.size(); ++i)
            {
                after_file << "    {\"x\": " << snap.points[i].x << ", \"y\": " << snap.points[i].y << "}";
                if (i < snap.points.size() - 1)
                    after_file << ",";
                after_file << "\n";
            }
            after_file << "  ],\n";
            after_file << "  \"center\": {\"x\": " << snap.center.x << ", \"y\": " << snap.center.y << "},\n";
            after_file << "  \"radius\": " << snap.radius << ",\n";
            after_file << "  \"inner_radius\": " << snap.inner_radius << ",\n";
            after_file << "  \"half_width\": " << snap.half_width << ",\n";
            after_file << "  \"closed\": " << (snap.closed ? "true" : "false") << "\n";
            after_file << "}\n";
        }
    }
}

void CxShapeInteractionRunner::GenerateBatchOutput(
    const CxShapeInteractionBatchResultEx& result,
    const CxShapeInteractionOptions& options,
    const std::string& out_dir)
{
    fs::create_directories(out_dir);

    int pass_count = 0;
    int fail_count = 0;
    int pending_writeback_count = 0;
    int missing_tool_count = 0;

    for (const auto& c : result.extended_cases)
    {
        if (c.pass)
            pass_count++;
        else
            fail_count++;
        if (c.status == "GEOMETRY_PASS_PENDING_WRITEBACK")
            pending_writeback_count++;
        if (c.conclusion == "tool_not_registered")
            missing_tool_count++;
    }

    std::ofstream json_file(fs::path(out_dir) / "shape_interaction_report.json");
    if (json_file)
    {
        json_file << "{\n";
        json_file << "  \"run_id\": \"" << EscapeJson(options.run_id) << "\",\n";
        json_file << "  \"tool_manifest_path\": \"" << EscapeJson(options.tool_manifest_path) << "\",\n";
        json_file << "  \"test_suite_path\": \"" << EscapeJson(options.test_suite_path) << "\",\n";
        json_file << "  \"output_dir\": \"" << EscapeJson(options.out_dir) << "\",\n";
        json_file << "  \"drag_steps\": " << options.drag_steps << ",\n";
        json_file << "  \"hit_tolerance\": " << options.tolerance << ",\n";
        json_file << "  \"pass\": " << (result.pass ? "true" : "false") << ",\n";
        json_file << "  \"total_cases\": " << result.extended_cases.size() << ",\n";
        json_file << "  \"executed_cases\": " << result.extended_cases.size() << ",\n";
        json_file << "  \"pass_count\": " << pass_count << ",\n";
        json_file << "  \"fail_count\": " << fail_count << ",\n";
        json_file << "  \"pending_writeback_count\": " << pending_writeback_count << ",\n";
        json_file << "  \"missing_tool_count\": " << missing_tool_count << ",\n";
        json_file << "  \"unified_log_enabled\": " << (options.unified_log_enabled ? "true" : "false") << ",\n";
        json_file << "  \"unified_log_path\": \"" << EscapeJson(options.unified_log_path) << "\",\n";
        json_file << "  \"unified_log_status\": \"" << EscapeJson(options.unified_log_status) << "\",\n";
        json_file << "  \"unified_log_reason\": \"" << EscapeJson(options.unified_log_reason) << "\",\n";
        json_file << "  \"cases\": [\n";
        for (size_t i = 0; i < result.extended_cases.size(); ++i)
        {
            const auto& c = result.extended_cases[i];
            json_file << "    {\n";
            json_file << "      \"run_id\": \"" << EscapeJson(options.run_id) << "\",\n";
            json_file << "      \"case_id\": \"" << EscapeJson(c.case_id) << "\",\n";
            json_file << "      \"tool_id\": \"" << EscapeJson(c.tool_id) << "\",\n";
            json_file << "      \"operation\": \"" << EscapeJson(c.operation) << "\",\n";
            json_file << "      \"shape_kind\": \"" << EscapeJson(c.shape_kind) << "\",\n";
            json_file << "      \"expected_handle\": \"" << EscapeJson(c.expected_handle) << "\",\n";
            json_file << "      \"actual_handle\": \"" << EscapeJson(c.actual_handle) << "\",\n";
            json_file << "      \"expected_vertex\": " << c.expected_vertex << ",\n";
            json_file << "      \"actual_vertex\": " << c.actual_vertex << ",\n";
            json_file << "      \"geometry_assertion\": \"" << EscapeJson(c.geometry_assertion) << "\",\n";
            json_file << "      \"hit_test_pass\": " << (c.hit_test_pass ? "true" : "false") << ",\n";
            json_file << "      \"drag_pass\": " << (c.drag_pass ? "true" : "false") << ",\n";
            json_file << "      \"commit_pass\": " << (c.commit_pass ? "true" : "false") << ",\n";
            json_file << "      \"runtime_writeback\": " << (c.runtime_writeback ? "true" : "false") << ",\n";
            json_file << "      \"acceptance_scope\": \"" << EscapeJson(c.acceptance_scope) << "\",\n";
            json_file << "      \"status\": \"" << EscapeJson(c.status) << "\",\n";
            json_file << "      \"reason\": \"" << EscapeJson(c.reason) << "\",\n";
            json_file << "      \"pass\": " << (c.pass ? "true" : "false") << ",\n";
            json_file << "      \"conclusion\": \"" << EscapeJson(c.conclusion) << "\"\n";
            json_file << "    }";
            if (i < result.extended_cases.size() - 1)
                json_file << ",";
            json_file << "\n";
        }
        json_file << "  ]\n";
        json_file << "}\n";
    }

    std::ofstream md_file(fs::path(out_dir) / "shape_interaction_report.md");
    if (md_file)
    {
        md_file << "# Shape Interaction Test Report\n\n";
        md_file << "## Summary\n\n";
        md_file << "- Total Cases: " << result.extended_cases.size() << "\n";
        md_file << "- Pass: " << pass_count << "\n";
        md_file << "- Fail: " << fail_count << "\n";
        md_file << "- Pending Writeback: " << pending_writeback_count << "\n";
        md_file << "- Missing Tools: " << missing_tool_count << "\n";
        md_file << "\n## Case Details\n\n";
        for (const auto& c : result.extended_cases)
        {
            md_file << "### " << c.case_id << "\n\n";
            md_file << "- Tool: " << c.tool_id << "\n";
            md_file << "- Status: " << c.status << "\n";
            md_file << "- Expected Handle: " << c.expected_handle << "\n";
            md_file << "- Actual Handle: " << c.actual_handle << "\n";
            md_file << "- Geometry Assertion: " << c.geometry_assertion << "\n";
            md_file << "- Reason: " << c.reason << "\n\n";
        }
    }

    std::ofstream failures_file(fs::path(out_dir) / "shape_interaction_failures.md");
    if (failures_file)
    {
        failures_file << "# Shape Interaction Failures\n\n";
        bool has_failures = false;
        for (const auto& c : result.extended_cases)
        {
            if (!c.pass)
            {
                has_failures = true;
                failures_file << "## " << c.case_id << "\n\n";
                failures_file << "- Tool: " << c.tool_id << "\n";
                failures_file << "- Expected Handle: " << c.expected_handle << "\n";
                failures_file << "- Actual Handle: " << c.actual_handle << "\n";
                failures_file << "- Geometry Assertion: " << c.geometry_assertion << "\n";
                failures_file << "- Status: " << c.status << "\n";
                failures_file << "- Reason: " << c.reason << "\n\n";
            }
        }
        if (!has_failures)
            failures_file << "No failures.\n";
    }

    std::ofstream snapshot_file(fs::path(out_dir) / "shape_interaction_snapshot.txt");
    if (snapshot_file)
    {
        snapshot_file << "Shape Interaction Test Snapshot\n";
        snapshot_file << "================================\n\n";
        snapshot_file << "Total: " << result.extended_cases.size() << "\n";
        snapshot_file << "Pass: " << pass_count << "\n";
        snapshot_file << "Fail: " << fail_count << "\n";
        snapshot_file << "\n";
        for (const auto& c : result.extended_cases)
        {
            snapshot_file << c.case_id << ": " << c.status << " (" << c.reason << ")\n";
        }
    }
}
