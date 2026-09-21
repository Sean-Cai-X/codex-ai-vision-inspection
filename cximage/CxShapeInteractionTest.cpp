#include "CxShapeInteractionTest.h"

#include "CircleShape.h"
#include "RectShape.h"
#include "PolylineShape.h"
#include "LineGaugeShape.h"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <filesystem>

namespace
{
namespace fs = std::filesystem;

const double kEpsilon = 1.0e-6;
const double kVisualEpsilon = 0.5;

bool DoubleEqual(double a, double b, double eps = kEpsilon)
{
    return std::fabs(a - b) < eps;
}
}

bool AssertPointCreated(const ShapeBase& shape, std::string& reason)
{
    if (shape.kind() != CxShapeKind::Points)
    {
        reason = "shape kind is not Points";
        return false;
    }
    std::vector<CxShapePoint> points;
    shape.exportPoints(points);
    if (points.empty())
    {
        reason = "no points in shape";
        return false;
    }
    reason = "point created with " + std::to_string(points.size()) + " point(s)";
    return true;
}

bool AssertLineCreated(const ShapeBase& shape, std::string& reason)
{
    if (shape.kind() != CxShapeKind::Line && shape.kind() != CxShapeKind::LineGauge)
    {
        reason = "shape kind is not Line or LineGauge";
        return false;
    }
    CxShapePoint p0, p1;
    if (!shape.exportLine(p0, p1))
    {
        reason = "exportLine returned false";
        return false;
    }
    if (DoubleEqual(p0.x, p1.x) && DoubleEqual(p0.y, p1.y))
    {
        reason = "line endpoints are identical";
        return false;
    }
    reason = "line created from (" + std::to_string(p0.x) + "," + std::to_string(p0.y) +
             ") to (" + std::to_string(p1.x) + "," + std::to_string(p1.y) + ")";
    return true;
}

bool AssertRectCreated(const ShapeBase& shape, std::string& reason)
{
    if (shape.kind() != CxShapeKind::Rect)
    {
        reason = "shape kind is not Rect";
        return false;
    }
    std::vector<CxShapePoint> pts;
    bool closed = false;
    shape.exportPolyline(pts, closed);
    if (pts.size() < 4)
    {
        reason = "rect has only " + std::to_string(pts.size()) + " points, expected >=4";
        return false;
    }
    if (!closed)
    {
        reason = "rect is not closed";
        return false;
    }
    reason = "rect created with " + std::to_string(pts.size()) + " points, closed=" + (closed ? "true" : "false");
    return true;
}

bool AssertCircleCreated(const ShapeBase& shape, std::string& reason)
{
    if (shape.kind() != CxShapeKind::Circle)
    {
        reason = "shape kind is not Circle";
        return false;
    }
    CxShapePoint center;
    double radius = 0.0;
    double inner_radius = 0.0;
    if (!shape.exportCircle(center, radius, inner_radius))
    {
        reason = "exportCircle returned false";
        return false;
    }
    if (radius <= 0.0)
    {
        reason = "circle radius is <= 0";
        return false;
    }
    reason = "circle created at (" + std::to_string(center.x) + "," + std::to_string(center.y) +
             ") with radius " + std::to_string(radius);
    return true;
}

bool AssertPolylineCreated(const ShapeBase& shape, std::string& reason)
{
    if (shape.kind() != CxShapeKind::Polyline)
    {
        reason = "shape kind is not Polyline";
        return false;
    }
    std::vector<CxShapePoint> pts;
    bool closed = false;
    shape.exportPolyline(pts, closed);
    if (pts.size() < 3)
    {
        reason = "polyline has only " + std::to_string(pts.size()) + " points, expected >=3";
        return false;
    }
    reason = "polyline created with " + std::to_string(pts.size()) + " points";
    return true;
}

bool AssertLineMoved(const ShapeBase& before, const ShapeBase& after,
                     double expected_dx, double expected_dy, std::string& reason)
{
    CxShapePoint p0_before, p1_before, p0_after, p1_after;
    if (!before.exportLine(p0_before, p1_before))
    {
        reason = "before.exportLine failed";
        return false;
    }
    if (!after.exportLine(p0_after, p1_after))
    {
        reason = "after.exportLine failed";
        return false;
    }

    const double actual_dx0 = p0_after.x - p0_before.x;
    const double actual_dy0 = p0_after.y - p0_before.y;
    const double actual_dx1 = p1_after.x - p1_before.x;
    const double actual_dy1 = p1_after.y - p1_before.y;

    const bool ok0 = DoubleEqual(actual_dx0, expected_dx, kVisualEpsilon) &&
                     DoubleEqual(actual_dy0, expected_dy, kVisualEpsilon);
    const bool ok1 = DoubleEqual(actual_dx1, expected_dx, kVisualEpsilon) &&
                     DoubleEqual(actual_dy1, expected_dy, kVisualEpsilon);

    if (!ok0 || !ok1)
    {
        reason = "line not moved correctly: expected dx=" + std::to_string(expected_dx) +
                 " dy=" + std::to_string(expected_dy) +
                 ", actual p0: dx=" + std::to_string(actual_dx0) +
                 " dy=" + std::to_string(actual_dy0) +
                 ", actual p1: dx=" + std::to_string(actual_dx1) +
                 " dy=" + std::to_string(actual_dy1);
        return false;
    }

    reason = "line moved correctly by (" + std::to_string(expected_dx) + "," + std::to_string(expected_dy) + ")";
    return true;
}

bool AssertCircleRadiusChangedOnly(const ShapeBase& before, const ShapeBase& after, std::string& reason)
{
    CxShapePoint center_before, center_after;
    double radius_before = 0.0, radius_after = 0.0;
    double inner_before = 0.0, inner_after = 0.0;

    if (!before.exportCircle(center_before, radius_before, inner_before))
    {
        reason = "before.exportCircle failed";
        return false;
    }
    if (!after.exportCircle(center_after, radius_after, inner_after))
    {
        reason = "after.exportCircle failed";
        return false;
    }

    const bool center_changed = !DoubleEqual(center_before.x, center_after.x, kVisualEpsilon) ||
                                !DoubleEqual(center_before.y, center_after.y, kVisualEpsilon);
    const bool radius_changed = !DoubleEqual(radius_before, radius_after, kVisualEpsilon);

    if (center_changed)
    {
        reason = "circle center changed during radius drag: before=(" +
                 std::to_string(center_before.x) + "," + std::to_string(center_before.y) +
                 "), after=(" + std::to_string(center_after.x) + "," + std::to_string(center_after.y) + ")";
        return false;
    }

    if (!radius_changed)
    {
        reason = "circle radius did not change during radius drag: before=" +
                 std::to_string(radius_before) + ", after=" + std::to_string(radius_after);
        return false;
    }

    reason = "circle radius changed only: from " + std::to_string(radius_before) +
             " to " + std::to_string(radius_after) + ", center unchanged";
    return true;
}

bool AssertCircleCenterMoved(const ShapeBase& before, const ShapeBase& after,
                             double expected_dx, double expected_dy, std::string& reason)
{
    CxShapePoint center_before, center_after;
    double radius_before = 0.0, radius_after = 0.0;
    double inner_before = 0.0, inner_after = 0.0;

    if (!before.exportCircle(center_before, radius_before, inner_before))
    {
        reason = "before.exportCircle failed";
        return false;
    }
    if (!after.exportCircle(center_after, radius_after, inner_after))
    {
        reason = "after.exportCircle failed";
        return false;
    }

    const double actual_dx = center_after.x - center_before.x;
    const double actual_dy = center_after.y - center_before.y;
    const bool radius_changed = !DoubleEqual(radius_before, radius_after, kVisualEpsilon);

    if (radius_changed)
    {
        reason = "circle radius changed during center drag: before=" +
                 std::to_string(radius_before) + ", after=" + std::to_string(radius_after);
        return false;
    }

    const bool center_ok = DoubleEqual(actual_dx, expected_dx, kVisualEpsilon) &&
                           DoubleEqual(actual_dy, expected_dy, kVisualEpsilon);

    if (!center_ok)
    {
        reason = "circle center not moved correctly: expected dx=" + std::to_string(expected_dx) +
                 " dy=" + std::to_string(expected_dy) +
                 ", actual dx=" + std::to_string(actual_dx) +
                 " dy=" + std::to_string(actual_dy);
        return false;
    }

    reason = "circle center moved correctly by (" + std::to_string(expected_dx) + "," + std::to_string(expected_dy) +
             "), radius unchanged";
    return true;
}

bool AssertPolylineVertexMoved(const ShapeBase& before, const ShapeBase& after,
                               int vertex_index, double target_x, double target_y, std::string& reason)
{
    std::vector<CxShapePoint> pts_before, pts_after;
    bool closed_before = false, closed_after = false;

    before.exportPolyline(pts_before, closed_before);
    after.exportPolyline(pts_after, closed_after);

    if (static_cast<int>(pts_before.size()) <= vertex_index)
    {
        reason = "vertex index " + std::to_string(vertex_index) +
                 " out of range, before has " + std::to_string(pts_before.size()) + " points";
        return false;
    }
    if (static_cast<int>(pts_after.size()) <= vertex_index)
    {
        reason = "vertex index " + std::to_string(vertex_index) +
                 " out of range, after has " + std::to_string(pts_after.size()) + " points";
        return false;
    }

    bool ok = DoubleEqual(pts_after[vertex_index].x, target_x, kVisualEpsilon) &&
              DoubleEqual(pts_after[vertex_index].y, target_y, kVisualEpsilon);

    for (int i = 0; i < static_cast<int>(pts_before.size()); ++i)
    {
        if (i == vertex_index) continue;
        if (static_cast<int>(pts_after.size()) <= i) continue;
        if (!DoubleEqual(pts_before[i].x, pts_after[i].x, kVisualEpsilon) ||
            !DoubleEqual(pts_before[i].y, pts_after[i].y, kVisualEpsilon))
        {
            ok = false;
            reason = "non-target vertex " + std::to_string(i) +
                     " changed: before=(" + std::to_string(pts_before[i].x) + "," + std::to_string(pts_before[i].y) +
                     "), after=(" + std::to_string(pts_after[i].x) + "," + std::to_string(pts_after[i].y) + ")";
            break;
        }
    }

    if (!ok && reason.empty())
    {
        reason = "vertex " + std::to_string(vertex_index) +
                 " not at target: expected (" + std::to_string(target_x) + "," + std::to_string(target_y) +
                 "), actual (" + std::to_string(pts_after[vertex_index].x) + "," + std::to_string(pts_after[vertex_index].y) + ")";
    }

    if (ok)
    {
        reason = "vertex " + std::to_string(vertex_index) +
                 " moved to (" + std::to_string(target_x) + "," + std::to_string(target_y) + "), other vertices unchanged";
    }

    return ok;
}

bool AssertLineGaugeWidthChangedOnly(const LineGaugeShape& before, const LineGaugeShape& after, std::string& reason)
{
    const bool x0_changed = !DoubleEqual(before.x0(), after.x0(), kVisualEpsilon);
    const bool y0_changed = !DoubleEqual(before.y0(), after.y0(), kVisualEpsilon);
    const bool x1_changed = !DoubleEqual(before.x1(), after.x1(), kVisualEpsilon);
    const bool y1_changed = !DoubleEqual(before.y1(), after.y1(), kVisualEpsilon);
    const bool hw_changed = !DoubleEqual(before.halfWidth(), after.halfWidth(), kVisualEpsilon);

    if (x0_changed || y0_changed || x1_changed || y1_changed)
    {
        reason = "line gauge endpoints changed during width drag: ";
        if (x0_changed) reason += "x0 ";
        if (y0_changed) reason += "y0 ";
        if (x1_changed) reason += "x1 ";
        if (y1_changed) reason += "y1 ";
        return false;
    }

    if (!hw_changed)
    {
        reason = "line gauge halfWidth did not change during width drag: before=" +
                 std::to_string(before.halfWidth()) + ", after=" + std::to_string(after.halfWidth());
        return false;
    }

    reason = "line gauge width changed only: from " + std::to_string(before.halfWidth()) +
             " to " + std::to_string(after.halfWidth()) + ", endpoints unchanged";
    return true;
}

bool AssertLineGaugeEndpointsMoved(const LineGaugeShape& before, const LineGaugeShape& after,
                                   bool start_moved, bool end_moved, std::string& reason)
{
    const bool x0_changed = !DoubleEqual(before.x0(), after.x0(), kVisualEpsilon);
    const bool y0_changed = !DoubleEqual(before.y0(), after.y0(), kVisualEpsilon);
    const bool x1_changed = !DoubleEqual(before.x1(), after.x1(), kVisualEpsilon);
    const bool y1_changed = !DoubleEqual(before.y1(), after.y1(), kVisualEpsilon);
    const bool hw_changed = !DoubleEqual(before.halfWidth(), after.halfWidth(), kVisualEpsilon);

    if (hw_changed)
    {
        reason = "line gauge halfWidth changed during endpoint drag: before=" +
                 std::to_string(before.halfWidth()) + ", after=" + std::to_string(after.halfWidth());
        return false;
    }

    if (start_moved && !x0_changed && !y0_changed)
    {
        reason = "line gauge start point did not move: before=(" +
                 std::to_string(before.x0()) + "," + std::to_string(before.y0()) +
                 "), after=(" + std::to_string(after.x0()) + "," + std::to_string(after.y0()) + ")";
        return false;
    }

    if (!start_moved && (x0_changed || y0_changed))
    {
        reason = "line gauge start point moved when it should not: before=(" +
                 std::to_string(before.x0()) + "," + std::to_string(before.y0()) +
                 "), after=(" + std::to_string(after.x0()) + "," + std::to_string(after.y0()) + ")";
        return false;
    }

    if (end_moved && !x1_changed && !y1_changed)
    {
        reason = "line gauge end point did not move: before=(" +
                 std::to_string(before.x1()) + "," + std::to_string(before.y1()) +
                 "), after=(" + std::to_string(after.x1()) + "," + std::to_string(after.y1()) + ")";
        return false;
    }

    if (!end_moved && (x1_changed || y1_changed))
    {
        reason = "line gauge end point moved when it should not: before=(" +
                 std::to_string(before.x1()) + "," + std::to_string(before.y1()) +
                 "), after=(" + std::to_string(after.x1()) + "," + std::to_string(after.y1()) + ")";
        return false;
    }

    std::string moved_desc;
    if (start_moved && end_moved) moved_desc = "both endpoints";
    else if (start_moved) moved_desc = "start point";
    else if (end_moved) moved_desc = "end point";
    else moved_desc = "no endpoints";

    reason = "line gauge " + moved_desc + " moved correctly, halfWidth unchanged";
    return true;
}

bool AssertLineGaugeCenterMoved(const LineGaugeShape& before, const LineGaugeShape& after,
                                double expected_dx, double expected_dy, std::string& reason)
{
    const double before_center_x = (before.x0() + before.x1()) * 0.5;
    const double before_center_y = (before.y0() + before.y1()) * 0.5;
    const double after_center_x = (after.x0() + after.x1()) * 0.5;
    const double after_center_y = (after.y0() + after.y1()) * 0.5;

    const double actual_dx = after_center_x - before_center_x;
    const double actual_dy = after_center_y - before_center_y;
    const bool hw_changed = !DoubleEqual(before.halfWidth(), after.halfWidth(), kVisualEpsilon);

    if (hw_changed)
    {
        reason = "line gauge halfWidth changed during center drag: before=" +
                 std::to_string(before.halfWidth()) + ", after=" + std::to_string(after.halfWidth());
        return false;
    }

    const bool center_ok = DoubleEqual(actual_dx, expected_dx, kVisualEpsilon) &&
                           DoubleEqual(actual_dy, expected_dy, kVisualEpsilon);

    if (!center_ok)
    {
        reason = "line gauge center not moved correctly: expected dx=" + std::to_string(expected_dx) +
                 " dy=" + std::to_string(expected_dy) +
                 ", actual dx=" + std::to_string(actual_dx) +
                 " dy=" + std::to_string(actual_dy);
        return false;
    }

    reason = "line gauge center moved correctly by (" + std::to_string(expected_dx) + "," + std::to_string(expected_dy) +
             "), halfWidth and endpoints unchanged";
    return true;
}

bool AssertRectCornerMoved(const ShapeBase& before, const ShapeBase& after,
                           int corner_index, double target_x, double target_y, std::string& reason)
{
    std::vector<CxShapePoint> pts_before, pts_after;
    bool closed_before = false, closed_after = false;

    before.exportPolyline(pts_before, closed_before);
    after.exportPolyline(pts_after, closed_after);

    if (static_cast<int>(pts_before.size()) <= corner_index)
    {
        reason = "corner index " + std::to_string(corner_index) +
                 " out of range, before has " + std::to_string(pts_before.size()) + " points";
        return false;
    }
    if (static_cast<int>(pts_after.size()) <= corner_index)
    {
        reason = "corner index " + std::to_string(corner_index) +
                 " out of range, after has " + std::to_string(pts_after.size()) + " points";
        return false;
    }

    bool ok = DoubleEqual(pts_after[corner_index].x, target_x, kVisualEpsilon) &&
              DoubleEqual(pts_after[corner_index].y, target_y, kVisualEpsilon);

    for (int i = 0; i < static_cast<int>(pts_before.size()); ++i)
    {
        if (i == corner_index) continue;
        if (static_cast<int>(pts_after.size()) <= i) continue;
        if (!DoubleEqual(pts_before[i].x, pts_after[i].x, kVisualEpsilon) ||
            !DoubleEqual(pts_before[i].y, pts_after[i].y, kVisualEpsilon))
        {
            ok = false;
            reason = "non-target corner " + std::to_string(i) +
                     " changed: before=(" + std::to_string(pts_before[i].x) + "," + std::to_string(pts_before[i].y) +
                     "), after=(" + std::to_string(pts_after[i].x) + "," + std::to_string(pts_after[i].y) + ")";
            break;
        }
    }

    if (!ok && reason.empty())
    {
        reason = "corner " + std::to_string(corner_index) +
                 " not at target: expected (" + std::to_string(target_x) + "," + std::to_string(target_y) +
                 "), actual (" + std::to_string(pts_after[corner_index].x) + "," + std::to_string(pts_after[corner_index].y) + ")";
    }

    if (ok)
    {
        reason = "corner " + std::to_string(corner_index) +
                 " moved to (" + std::to_string(target_x) + "," + std::to_string(target_y) + "), other corners unchanged";
    }

    return ok;
}

bool WriteShapeInteractionReportJson(const CxShapeInteractionBatchResult& result,
                                     const std::string& path, std::string& reason)
{
    try
    {
        fs::create_directories(fs::path(path).parent_path());
        std::ofstream file(path);
        if (!file)
        {
            reason = "failed to open JSON output file: " + path;
            return false;
        }

        file << "{\n";
        file << "  \"pass\": " << (result.pass ? "true" : "false") << ",\n";
        file << "  \"case_count\": " << result.cases.size() << ",\n";
        file << "  \"pass_count\": " << std::count_if(result.cases.begin(), result.cases.end(),
                                                      [](const auto& c) { return c.pass; }) << ",\n";
        file << "  \"fail_count\": " << std::count_if(result.cases.begin(), result.cases.end(),
                                                      [](const auto& c) { return !c.pass; }) << ",\n";
        file << "  \"cases\": [\n";

        for (size_t i = 0; i < result.cases.size(); ++i)
        {
            const auto& c = result.cases[i];
            file << "    {\n";
            file << "      \"case_id\": \"" << c.case_id << "\",\n";
            file << "      \"tool_id\": \"" << c.tool_id << "\",\n";
            file << "      \"shape_kind\": \"" << c.shape_kind << "\",\n";
            file << "      \"pass\": " << (c.pass ? "true" : "false") << ",\n";
            file << "      \"conclusion\": \"" << c.conclusion << "\",\n";
            file << "      \"steps\": [\n";

            for (size_t j = 0; j < c.steps.size(); ++j)
            {
                const auto& s = c.steps[j];
                file << "        {\n";
                file << "          \"action\": \"" << s.action << "\",\n";
                file << "          \"target_ref\": \"" << s.target_ref << "\",\n";
                file << "          \"role\": " << static_cast<int>(s.role) << ",\n";
                file << "          \"x0\": " << s.x0 << ",\n";
                file << "          \"y0\": " << s.y0 << ",\n";
                file << "          \"x1\": " << s.x1 << ",\n";
                file << "          \"y1\": " << s.y1 << ",\n";
                file << "          \"ok\": " << (s.ok ? "true" : "false") << ",\n";
                file << "          \"reason\": \"" << s.reason << "\"\n";
                file << "        }" << (j < c.steps.size() - 1 ? "," : "") << "\n";
            }

            file << "      ]\n";
            file << "    }" << (i < result.cases.size() - 1 ? "," : "") << "\n";
        }

        file << "  ]\n";
        file << "}\n";

        reason = "JSON report written to: " + path;
        return true;
    }
    catch (const std::exception& e)
    {
        reason = "exception writing JSON: " + std::string(e.what());
        return false;
    }
}

bool WriteShapeInteractionReportMd(const CxShapeInteractionBatchResult& result,
                                   const std::string& path, std::string& reason)
{
    try
    {
        fs::create_directories(fs::path(path).parent_path());
        std::ofstream file(path);
        if (!file)
        {
            reason = "failed to open MD output file: " + path;
            return false;
        }

        file << "# Shape Interaction Smoke Test Report\n\n";
        file << "## Summary\n\n";
        file << "- Total cases: " << result.cases.size() << "\n";
        file << "- Passed: " << std::count_if(result.cases.begin(), result.cases.end(),
                                              [](const auto& c) { return c.pass; }) << "\n";
        file << "- Failed: " << std::count_if(result.cases.begin(), result.cases.end(),
                                              [](const auto& c) { return !c.pass; }) << "\n";
        file << "- Overall: " << (result.pass ? "✅ PASS" : "❌ FAIL") << "\n\n";

        file << "## Case Details\n\n";
        file << "| Case | Tool | Shape | Operation | Pass | Conclusion |\n";
        file << "|------|------|-------|-----------|------|------------|\n";

        for (const auto& c : result.cases)
        {
            std::string operation;
            for (const auto& s : c.steps)
            {
                if (!operation.empty()) operation += " → ";
                operation += s.action;
                if (s.role != CxShapeHandleRole::None)
                    operation += "(role=" + std::to_string(static_cast<int>(s.role)) + ")";
            }

            file << "|" << c.case_id << "|" << c.tool_id << "|" << c.shape_kind << "|"
                 << operation << "|" << (c.pass ? "✅" : "❌") << "|" << c.conclusion << "|\n";
        }

        reason = "MD report written to: " + path;
        return true;
    }
    catch (const std::exception& e)
    {
        reason = "exception writing MD: " + std::string(e.what());
        return false;
    }
}

bool WriteShapeInteractionFailuresMd(const CxShapeInteractionBatchResult& result,
                                     const std::string& path, std::string& reason)
{
    try
    {
        fs::create_directories(fs::path(path).parent_path());
        std::ofstream file(path);
        if (!file)
        {
            reason = "failed to open failures MD file: " + path;
            return false;
        }

        file << "# Shape Interaction Failures\n\n";

        std::vector<const CxShapeInteractionCaseResult*> failures;
        for (const auto& c : result.cases)
        {
            if (!c.pass) failures.push_back(&c);
        }

        if (failures.empty())
        {
            file << "No failures - all tests passed.\n";
        }
        else
        {
            file << "Total failures: " << failures.size() << "\n\n";

            for (const auto* c : failures)
            {
                file << "## " << c->case_id << "\n\n";
                file << "- Tool: " << c->tool_id << "\n";
                file << "- Shape: " << c->shape_kind << "\n";
                file << "- Conclusion: " << c->conclusion << "\n\n";
                file << "### Steps\n\n";

                for (const auto& s : c->steps)
                {
                    file << "- **" << s.action << "**: "
                         << (s.ok ? "OK" : "FAIL") << " - " << s.reason << "\n";
                }
                file << "\n";
            }
        }

        reason = "Failures MD written to: " + path;
        return true;
    }
    catch (const std::exception& e)
    {
        reason = "exception writing failures MD: " + std::string(e.what());
        return false;
    }
}

bool WriteShapeInteractionSnapshot(const CxShapeInteractionBatchResult& result,
                                   const std::string& path, std::string& reason)
{
    try
    {
        fs::create_directories(fs::path(path).parent_path());
        std::ofstream file(path);
        if (!file)
        {
            reason = "failed to open snapshot file: " + path;
            return false;
        }

        file << "Shape Interaction Smoke Test Snapshot\n";
        file << "=====================================\n\n";
        file << "Pass: " << (result.pass ? "YES" : "NO") << "\n";
        file << "Cases: " << result.cases.size() << "\n";
        file << "\n";

        for (const auto& c : result.cases)
        {
            file << "--- " << c.case_id << " ---\n";
            file << "  tool_id: " << c.tool_id << "\n";
            file << "  shape_kind: " << c.shape_kind << "\n";
            file << "  pass: " << (c.pass ? "YES" : "NO") << "\n";
            file << "  conclusion: " << c.conclusion << "\n";
            file << "\n";
        }

        reason = "Snapshot written to: " + path;
        return true;
    }
    catch (const std::exception& e)
    {
        reason = "exception writing snapshot: " + std::string(e.what());
        return false;
    }
}

bool WriteShapeSuiteLoadReport(
    const std::string& out_dir,
    const std::string& manifest_path,
    const std::string& suite_path,
    bool manifest_exists,
    bool manifest_parse_ok,
    bool suite_exists,
    bool suite_parse_ok,
    int case_count,
    const std::string& failure_stage,
    const std::string& reason)
{
    try
    {
        fs::create_directories(out_dir);
        const fs::path report_path = fs::path(out_dir) / "suite_load_report.json";
        std::ofstream file(report_path);
        if (!file)
        {
            return false;
        }

        file << "{\n";
        file << "  \"working_directory\": \"" << fs::current_path().string() << "\",\n";
        file << "  \"manifest_path\": \"" << manifest_path << "\",\n";
        file << "  \"manifest_exists\": " << (manifest_exists ? "true" : "false") << ",\n";
        file << "  \"manifest_parse_ok\": " << (manifest_parse_ok ? "true" : "false") << ",\n";
        file << "  \"suite_path\": \"" << suite_path << "\",\n";
        file << "  \"suite_exists\": " << (suite_exists ? "true" : "false") << ",\n";
        file << "  \"suite_parse_ok\": " << (suite_parse_ok ? "true" : "false") << ",\n";
        file << "  \"case_count\": " << case_count << ",\n";
        file << "  \"failure_stage\": \"" << failure_stage << "\",\n";
        file << "  \"reason\": \"" << reason << "\"\n";
        file << "}\n";

        return true;
    }
    catch (const std::exception& e)
    {
        return false;
    }
}