#include "pch.h"
#include "ManualConsoleFindCircleDebug.h"
#include "ManualConsoleUtils.h"
#include "ManualConsoleRuntimeView.h"
#include "ManualConsoleCxScriptDebug.h"
#include "CxScriptRunTraceRuntime.h"

#include <sstream>
#include <fstream>

namespace fs = std::filesystem;

void RefreshFindCircleDisplaySnapshot(ManualTestContext& context,
    RuntimeObjectView& object)
{
    if (object.type != "FindCircle")
        return;

    object.has_circle_roi_outer_polyline = false;
    object.has_circle_roi_inner_polyline = false;
    object.circle_roi_outer_xy.clear();
    object.circle_roi_inner_xy.clear();
    object.circle_roi_segment_count = 0;

    if (object.has_circle)
    {
        const CxCirclePolylineSnapshot outer = BuildCxCirclePolylineSnapshot(
            object.circle_cx,
            object.circle_cy,
            object.circle_radius);

        object.has_circle_roi_outer_polyline = outer.valid;
        if (outer.valid)
        {
            object.circle_roi_outer_xy = outer.xy;
            object.circle_roi_segment_count = outer.segment_count;
        }

        if (object.circle_inner > 0.0f)
        {
            const CxCirclePolylineSnapshot inner = BuildCxCirclePolylineSnapshot(
                object.circle_cx,
                object.circle_cy,
                object.circle_inner);

            object.has_circle_roi_inner_polyline = inner.valid;
            if (inner.valid)
                object.circle_roi_inner_xy = inner.xy;
        }
    }

    object.has_fit_circle_polyline = false;
    object.fit_circle_xy.clear();
    object.fit_circle_segment_count = 0;

    if (object.has_fit_result)
    {
        const CxCirclePolylineSnapshot fit = BuildCxCirclePolylineSnapshot(
            object.fit_cx,
            object.fit_cy,
            object.fit_radius);

        object.has_fit_circle_polyline = fit.valid;
        if (fit.valid)
        {
            object.fit_circle_xy = fit.xy;
            object.fit_circle_segment_count = fit.segment_count;
        }
    }

    ++object.display_version;
    ++context.runtime_overlay_version;
}

void RefreshFindCircleMeasureGeometrySnapshot(
    RuntimeObjectView& object,
    FindCircle& circle)
{
    const FindCircleMeasureGeometryDebug& dbg =
        circle.lastmeasuregeometrydebug();

    object.circle_measure_geometry_request_valid =
        dbg.request_valid;

    object.circle_measure_geometry_dirty =
        dbg.geometry_dirty;

    object.circle_measure_geometry_ready =
        dbg.geometry_ready;

    object.circle_measure_geometry_version =
        dbg.geometry_version;

    object.circle_measure_geometry_built_version =
        dbg.geometry_built_version;

    object.circle_scan_line_count =
        dbg.scan_line_count;

    object.circle_scan_line_length =
        dbg.scan_line_length;

    object.circle_process_width =
        dbg.process_width;

    object.circle_selected_edge_index =
        dbg.selected_edge_index;

    object.circle_candidate_runs_total =
        dbg.candidate_runs_total;

    object.circle_candidate_runs_max_per_line =
        dbg.candidate_runs_max_per_line;

    object.circle_selected_edge_hits =
        dbg.selected_edge_hits;

    object.circle_selected_edge_misses =
        dbg.selected_edge_misses;

    object.circle_selected_edge_radius_avg =
        dbg.selected_edge_radius_avg;

    object.circle_selected_edge_radius_min =
        dbg.selected_edge_radius_min;

    object.circle_selected_edge_radius_max =
        dbg.selected_edge_radius_max;

    object.circle_measure_image_ready =
        dbg.image_ready;

    object.circle_measure_image_width =
        dbg.image_width;

    object.circle_measure_image_height =
        dbg.image_height;

    object.circle_measure_image_channels =
        dbg.image_channels;

    object.circle_measure_backimage_ready =
        dbg.backimage_ready;

    object.circle_measure_findobject_ready =
        dbg.findobject_ready;

    object.circle_object_prefilter_requested =
        dbg.object_prefilter_requested;

    object.circle_object_prefilter_applied =
        dbg.object_prefilter_applied;

    object.circle_object_prefilter_restored =
        dbg.object_prefilter_restored;

    object.circle_object_prefilter_runs_before =
        dbg.object_prefilter_runs_before;

    object.circle_object_prefilter_runs_after =
        dbg.object_prefilter_runs_after;

    object.circle_object_prefilter_effective_min =
        dbg.object_prefilter_effective_min;

    object.circle_measure_source =
        dbg.measure_source;

    object.circle_measure_failure_stage =
        dbg.failure_stage;

    object.circle_measure_detail =
        dbg.detail;

    object.circle_scan_lines_processed =
        dbg.scan_lines_processed;

    object.circle_total_samples =
        dbg.total_samples;

    object.circle_elapsed_ms =
        dbg.elapsed_ms;

    object.circle_budget_max_scan_lines =
        dbg.budget_max_scan_lines;

    object.circle_budget_max_samples =
        dbg.budget_max_samples;

    object.circle_budget_max_elapsed_ms =
        dbg.budget_max_elapsed_ms;

    const FindCircleBoundaryAnalysisSnapshot precision =
        circle.boundaryanalysissnapshot();
    object.circle_boundary_status = precision.status;
    object.circle_boundary_reliability_level = precision.reliability_level;
    object.circle_boundary_expected_scan_count = precision.expected_scan_count;
    object.circle_boundary_accepted_point_count = precision.accepted_point_count;
    object.circle_boundary_refined_point_count =
        precision.interpolation_valid_count;
    object.circle_boundary_coverage_ratio = precision.coverage_ratio;
    object.circle_boundary_subpixel_offset_mean =
        precision.subpixel_offset_mean;
    object.circle_boundary_subpixel_offset_stddev =
        precision.subpixel_offset_stddev;
    object.circle_boundary_localization_sigma_mean_px =
        precision.localization_sigma_mean_px;
    object.circle_boundary_residual_rmse_px = precision.residual_rmse_px;
    object.circle_boundary_residual_p95_px = precision.residual_p95_px;
    object.circle_boundary_residual_max_px = precision.residual_max_px;
    object.circle_boundary_outlier_ratio = precision.outlier_ratio;
    object.circle_boundary_reliability_score = precision.reliability_score;
}

bool ResolveDebugIntToken(ManualTestContext& context, const std::string& token, int& value)
{
    const std::string key = TrimLine(token);
    const auto found = context.runtime_int_vars.find(key);
    if (found != context.runtime_int_vars.end()) { value = found->second; return true; }
    char* end = nullptr; const long parsed = std::strtol(key.c_str(), &end, 10);
    if (end == key.c_str() || *end != '\0') return false;
    value = static_cast<int>(parsed); return true;
}

bool TryExecuteFindCircleSetcircle(ManualTestContext& context,
    int lineIndex,
    const std::string& statement)
{
    const ParsedMethodCall call = ParseMethodCall(statement);

    if (!call.valid)
        return false;

    if (call.method != "setcircle")
        return false;

    if (call.args.size() < 4)
    {
        ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
        line.status = "BLOCKED";
        line.reason = "setcircle requires 4 parameters";
        context.run_state = "blocked";
        context.debug_status = "BLOCKED";
        context.debug_reason = line.reason;
        return true;
    }

    RuntimeObjectView& object = EnsureRuntimeObject(
        context,
        call.object,
        "FindCircle",
        context.line_views[static_cast<std::size_t>(lineIndex)].line_no);

    int circleValues[4] = {};
    for (int index = 0; index < 4; ++index)
    {
        if (!ResolveDebugIntToken(
                context,
                call.args[static_cast<std::size_t>(index)],
                circleValues[index]))
        {
            ScriptLineView& line =
                context.line_views[static_cast<std::size_t>(lineIndex)];
            line.status = "BLOCKED";
            line.reason = "FindCircle.setcircle unresolved parameter: " +
                call.args[static_cast<std::size_t>(index)];
            line.timestamp = CurrentTimestamp();
            context.run_state = "blocked";
            context.debug_status = "BLOCKED";
            context.debug_reason = line.reason;
            return true;
        }
    }

    object.circle_cx = static_cast<float>(circleValues[0]);
    object.circle_cy = static_cast<float>(circleValues[1]);
    object.circle_inner = static_cast<float>(circleValues[2]);
    object.circle_radius = static_cast<float>(circleValues[3]);

    DebugCximageRuntime& runtime = CxRuntime(context);

    auto circleIt = runtime.circles.find(call.object);
    if (circleIt == runtime.circles.end() || !circleIt->second)
    {
        PrepareFindCircleDebugRuntime();
        runtime.circles[call.object] = std::make_unique<FindCircle>();
        circleIt = runtime.circles.find(call.object);
    }

    const int cx = static_cast<int>(object.circle_cx);
    const int cy = static_cast<int>(object.circle_cy);
    const int scriptThird = static_cast<int>(object.circle_inner);
    const int scriptFourth = static_cast<int>(object.circle_radius);
    int perimeterX = scriptThird;
    int perimeterY = scriptFourth;

    if (scriptThird == 0 && scriptFourth > 0)
    {
        perimeterX = cx;
        perimeterY = cy + scriptFourth;
    }

    circleIt->second->setcircle(cx, cy, perimeterX, perimeterY);

    RefreshFindCircleMeasureGeometrySnapshot(object, *circleIt->second);

    object.has_circle = true;
    RefreshFindCircleDisplaySnapshot(context, object);
    object.visualizable = true;
    object.exists_in_parser = true;
    object.stale = false;
    object.visual_source = "runtime_object";
    object.last_method = "setcircle";
    object.last_runtime_status = "runtime_executed";
    object.runtime_state = "runtime_param_set";
    object.last_update_line = context.line_views[static_cast<std::size_t>(lineIndex)].line_no;

    std::ostringstream summary;
    summary << "FindCircle.setcircle executed"
            << " | script_circle=("
            << object.circle_cx << ", "
            << object.circle_cy << ", "
            << object.circle_inner << ", "
            << object.circle_radius << ")"
            << " | native_perimeter=("
            << perimeterX << ", "
            << perimeterY << ")"
            << " | request_cache=updated";
    object.display_summary = summary.str();

    ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
    line.status = "runtime_executed";
    line.reason = "FindCircle.setcircle executed";
    line.timestamp = CurrentTimestamp();

    context.runtime_current_status = "PENDING";
    context.current_line = FindNextNonEmptyLine(context, lineIndex + 1);
    context.run_state = "runtime_step";
    context.debug_status = "PENDING";
    context.debug_reason = "setcircle updated runtime object";
    AppendCxDebugEvent(
        context,
        "findcircle_setcircle",
        line.line_no,
        statement,
        call.object,
        call.method,
        line.status,
        line.reason,
        object.display_summary);

    AppendCxDebugRuntimeObjectsSnapshot(context, "runtime_objects_after_setcircle");
    return true;
}

bool TryExecuteFindCircleParamMethod(ManualTestContext& context,
    int lineIndex,
    const std::string& statement)
{
    const ParsedMethodCall call = ParseMethodCall(statement);

    if (!call.valid)
        return false;

    const bool isCircleParamMethod =
        call.method == "setmethod" ||
        call.method == "Setgap" ||
        call.method == "setthre" ||
        call.method == "setlinegap" ||
        call.method == "setfitmeasuregap" ||
        call.method == "setcirclegap" ||
        call.method == "setlinesamplerate" ||
        call.method == "setgamarate" ||
        call.method == "setfindsetting" ||
        call.method == "setselectedgenum" ||
        call.method == "setfilter";

    if (!isCircleParamMethod)
        return false;

    if (!RuntimeObjectIsType(context, call.object, "FindCircle"))
    {
        return false;
    }

    DebugCximageRuntime& runtime = CxRuntime(context);

    auto circleIt = runtime.circles.find(call.object);
    if (circleIt == runtime.circles.end() || !circleIt->second)
        return false;

    if (call.args.empty())
    {
        ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
        line.status = "BLOCKED";
        line.reason = call.method + " requires one parameter";
        line.timestamp = CurrentTimestamp();

        context.run_state = "blocked";
        context.debug_status = "BLOCKED";
        context.debug_reason = line.reason;
        return true;
    }

    if (call.method == "setfilter")
    {
        ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
        if (call.args.size() < 3)
        {
            line.status = "BLOCKED";
            line.reason = "FindCircle.setfilter requires borw, min, max";
            line.timestamp = CurrentTimestamp();
            context.run_state = "blocked";
            context.debug_status = "BLOCKED";
            context.debug_reason = line.reason;
            return true;
        }

        int borw = 0;
        int minArea = 0;
        int maxArea = 0;
        if (!ResolveDebugIntToken(context, call.args[0], borw) ||
            !ResolveDebugIntToken(context, call.args[1], minArea) ||
            !ResolveDebugIntToken(context, call.args[2], maxArea))
        {
            line.status = "BLOCKED";
            line.reason = "FindCircle.setfilter unresolved parameter";
            line.timestamp = CurrentTimestamp();
            context.run_state = "blocked";
            context.debug_status = "BLOCKED";
            context.debug_reason = line.reason;
            return true;
        }

        circleIt->second->setfilter(borw, minArea, maxArea);
        RuntimeObjectView& object = EnsureRuntimeObject(
            context, call.object, "FindCircle", line.line_no);
        RefreshFindCircleMeasureGeometrySnapshot(object, *circleIt->second);
        object.exists_in_parser = true;
        object.last_method = call.method;
        object.last_runtime_status = "runtime_executed";
        object.runtime_state = "runtime_param_set";
        object.last_update_line = line.line_no;
        object.display_summary = "setfilter(" + call.params + ")";
        object.stale = false;
        line.status = "runtime_executed";
        line.reason = "FindCircle.setfilter executed";
        line.timestamp = CurrentTimestamp();
        context.current_line = FindNextNonEmptyLine(context, lineIndex + 1);
        context.run_state = "runtime_step";
        context.debug_status = "PENDING";
        context.debug_reason = line.reason;
        return true;
    }

    if (call.method == "setlinesamplerate")
    {
        const std::string token = TrimLine(call.args[0]);
        char* end = nullptr;
        const double sampleRate = std::strtod(token.c_str(), &end);
        if (end == token.c_str() || *end != '\0')
        {
            ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
            line.status = "BLOCKED";
            line.reason = "FindCircle.setlinesamplerate unresolved parameter: " + token;
            line.timestamp = CurrentTimestamp();
            context.run_state = "blocked";
            context.debug_status = "BLOCKED";
            context.debug_reason = line.reason;
            return true;
        }
        circleIt->second->setlinesamplerate(sampleRate);
        RuntimeObjectView& object = EnsureRuntimeObject(
            context, call.object, "FindCircle",
            context.line_views[static_cast<std::size_t>(lineIndex)].line_no);
        RefreshFindCircleMeasureGeometrySnapshot(object, *circleIt->second);
        object.exists_in_parser = true;
        object.last_method = call.method;
        object.last_runtime_status = "runtime_executed";
        object.runtime_state = "runtime_param_set";
        object.last_update_line = context.line_views[static_cast<std::size_t>(lineIndex)].line_no;
        object.display_summary = "setlinesamplerate(" + token + ")";
        object.stale = false;
        ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
        line.status = "runtime_executed";
        line.reason = "FindCircle.setlinesamplerate executed";
        line.timestamp = CurrentTimestamp();
        context.current_line = FindNextNonEmptyLine(context, lineIndex + 1);
        context.run_state = "runtime_step";
        context.debug_status = "PENDING";
        context.debug_reason = line.reason;
        return true;
    }

    int value = 0;
    if (!ResolveDebugIntToken(context, call.args[0], value))
    {
        ScriptLineView& line =
            context.line_views[static_cast<std::size_t>(lineIndex)];
        line.status = "BLOCKED";
        line.reason = "FindCircle." + call.method +
            " unresolved parameter: " + call.args[0];
        line.timestamp = CurrentTimestamp();
        context.run_state = "blocked";
        context.debug_status = "BLOCKED";
        context.debug_reason = line.reason;
        return true;
    }

    if (call.method == "setmethod")
        circleIt->second->setmethod(value);
    else if (call.method == "Setgap")
        circleIt->second->Setgap(value);
    else if (call.method == "setthre")
        circleIt->second->setthre(value);
    else if (call.method == "setlinegap")
        circleIt->second->setlinegap(value);
    else if (call.method == "setfitmeasuregap")
        circleIt->second->setfitmeasuregap(value);
    else if (call.method == "setcirclegap")
        circleIt->second->setcirclegap(value);
    else if (call.method == "setgamarate")
        circleIt->second->setgamarate(value);
    else if (call.method == "setfindsetting")
        circleIt->second->setfindsetting(value);
    else if (call.method == "setselectedgenum")
        circleIt->second->setselectedgenum(value);
    else if (call.method == "setlinesamplerate")
        circleIt->second->setlinesamplerate(
            static_cast<double>(value));

    RefreshFindCircleMeasureGeometrySnapshot(
        EnsureRuntimeObject(
            context,
            call.object,
            "FindCircle",
            context.line_views[static_cast<std::size_t>(lineIndex)].line_no),
        *circleIt->second);

    RuntimeObjectView& object = EnsureRuntimeObject(
        context,
        call.object,
        "FindCircle",
        context.line_views[static_cast<std::size_t>(lineIndex)].line_no);

    object.exists_in_parser = true;
    object.last_method = call.method;
    object.last_runtime_status = "runtime_executed";
    object.runtime_state = "runtime_param_set";
    object.last_update_line = context.line_views[static_cast<std::size_t>(lineIndex)].line_no;
    object.display_summary = call.method + "(" + call.params + ")";
    object.stale = false;

    ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
    line.status = "runtime_executed";
    line.reason = "FindCircle." + call.method + " executed";
    line.timestamp = CurrentTimestamp();

    context.current_line = FindNextNonEmptyLine(context, lineIndex + 1);
    context.run_state = "runtime_step";
    context.debug_status = "PENDING";
    context.debug_reason = "FindCircle parameter method executed";

    return true;
}

void FillFindCircleResultView(RuntimeObjectView& object,
    FindCircle& circle,
    const std::string& methodName)
{
    object.exists_in_parser = true;
    object.visualizable = true;
    object.visual_source = "runtime_object";
    object.stale = false;
    object.last_method = methodName;
    object.last_runtime_status = "runtime_executed";
    object.runtime_state = "runtime_executed";

    object.fit_cx = static_cast<float>(circle.getresultcentx());
    object.fit_cy = static_cast<float>(circle.getresultcenty());
    object.fit_radius = static_cast<float>(circle.getradius());
    object.fit_avgdist = static_cast<float>(circle.getavgdist());

    object.has_fit_result = false;

    if (methodName != "measure" &&
        std::isfinite(object.fit_cx) &&
        std::isfinite(object.fit_cy) &&
        std::isfinite(object.fit_radius) &&
        object.fit_radius > 0.0f)
    {
        object.has_fit_result = true;
    }

    object.has_measure_points = false;
    object.measure_points_xy.clear();

    PointsShape& points = circle.getresultpoints();

    const int pointCount = points.size();
    object.measure_points_count = pointCount;

    for (int i = 0; i < pointCount; ++i)
    {
        const double x = points.getx(i);
        const double y = points.gety(i);

        if (!std::isfinite(x) || !std::isfinite(y))
            continue;

        object.measure_points_xy.push_back(static_cast<float>(x));
        object.measure_points_xy.push_back(static_cast<float>(y));
    }

    object.has_measure_points = !object.measure_points_xy.empty();
    object.valid_points_count =
        static_cast<int>(object.measure_points_xy.size() / 2);

    if (object.has_fit_result)
    {
        object.runtime_state = "geometry_result_available";
        object.last_runtime_status = "runtime_executed";
    }
    else if (methodName != "measure")
    {
        object.runtime_state = object.has_measure_points
            ? "fitcircle_degenerate_after_measure_points"
            : "fitcircle_pending_binding";
        object.last_runtime_status = "PENDING_BINDING";
    }
    else if (object.has_measure_points)
    {
        object.runtime_state = "measure_points_available";
    }
    else
    {
        object.runtime_state = "measure_points_unavailable";
    }

    std::ostringstream summary;
    summary << methodName
        << " executed"
        << " | fit=(" << object.fit_cx
        << "," << object.fit_cy
        << ", r=" << object.fit_radius
        << ")"
        << " | avgdist=" << object.fit_avgdist
        << " | points=" << pointCount
        << " | valid_points=" << (object.measure_points_xy.size() / 2);

    object.display_summary = summary.str();

    object.measure_points_count = pointCount;
    object.valid_points_count = static_cast<int>(object.measure_points_xy.size() / 2);
    object.has_measure_points = !object.measure_points_xy.empty();

    RefreshFindCircleMeasureGeometrySnapshot(object, circle);
}

bool SaveFindCircleDebugSnapshotJson(const ManualTestContext& context,
    std::string& outPath,
    std::string& outReason)
{
    try
    {
        fs::path caseDir(context.case_directory.empty()
            ? "docs/notes/cxscript_case"
            : context.case_directory);

        fs::create_directories(caseDir);

        fs::path filePath = caseDir / "findcircle_debug_snapshot.json";

        std::ofstream file(filePath.string(), std::ios::out | std::ios::trunc);

        if (!file.is_open())
        {
            outReason = "failed to open snapshot file: " + filePath.string();
            return false;
        }

        file << "{\n";
        file << "  \"script_path\": \"" << EscapeJsonString(context.loaded_script_path) << "\",\n";
        file << "  \"flow_block_id\": \"cximage_find_circle_explore.N0\",\n";
        file << "  \"current_line\": " << context.current_line << ",\n";
        file << "  \"runtime_current_status\": \"" << EscapeJsonString(context.runtime_current_status) << "\",\n";

        file << "  \"current_result_ref\": {\n";
        file << "    \"name\": \"" << EscapeJsonString(context.current_result_ref.name) << "\",\n";
        file << "    \"value\": \"" << EscapeJsonString(context.current_result_ref.value) << "\",\n";
        file << "    \"source_object\": \"" << EscapeJsonString(context.current_result_ref.source_object) << "\",\n";
        file << "    \"result_type\": \"" << EscapeJsonString(context.current_result_ref.result_type) << "\",\n";
        file << "    \"status\": \"" << EscapeJsonString(context.current_result_ref.status) << "\",\n";
        file << "    \"reason\": \"" << EscapeJsonString(context.current_result_ref.reason) << "\",\n";
        file << "    \"fit_cx\": " << context.current_result_ref.fit_cx << ",\n";
        file << "    \"fit_cy\": " << context.current_result_ref.fit_cy << ",\n";
        file << "    \"fit_radius\": " << context.current_result_ref.fit_radius << ",\n";
        file << "    \"avgdist\": " << context.current_result_ref.avgdist << ",\n";
        file << "    \"points_count\": " << context.current_result_ref.points_count << ",\n";
        file << "    \"valid_points_count\": " << context.current_result_ref.valid_points_count << "\n";
        file << "  },\n";

        file << "  \"geometry_summary\": \"" << EscapeJsonString(context.geometry_summary) << "\",\n";
        file << "  \"image_overlay_summary\": \"" << EscapeJsonString(context.image_overlay_summary) << "\",\n";
        file << "  \"last_debug_result\": \"" << EscapeJsonString(context.debug_reason) << "\",\n";

        file << "  \"runtime_objects\": [\n";

        for (std::size_t i = 0; i < context.runtime_objects.size(); ++i)
        {
            const RuntimeObjectView& object = context.runtime_objects[i];

            file << "    {\n";
            file << "      \"name\": \"" << EscapeJsonString(object.name) << "\",\n";
            file << "      \"type\": \"" << EscapeJsonString(object.type) << "\",\n";
            file << "      \"runtime_state\": \"" << EscapeJsonString(object.runtime_state) << "\",\n";
            file << "      \"last_method\": \"" << EscapeJsonString(object.last_method) << "\",\n";
            file << "      \"display_summary\": \"" << EscapeJsonString(object.display_summary) << "\",\n";
            file << "      \"visualizable\": " << (object.visualizable ? "true" : "false") << ",\n";
            file << "      \"has_circle\": " << (object.has_circle ? "true" : "false") << ",\n";
            file << "      \"has_measure_points\": " << (object.has_measure_points ? "true" : "false") << ",\n";
            file << "      \"has_fit_result\": " << (object.has_fit_result ? "true" : "false") << ",\n";
            file << "      \"circle\": [" << object.circle_cx << ", " << object.circle_cy << ", "
                << object.circle_inner << ", " << object.circle_radius << "],\n";
            file << "      \"fit_circle\": [" << object.fit_cx << ", " << object.fit_cy << ", "
                << object.fit_radius << "],\n";
            file << "      \"avgdist\": " << object.fit_avgdist << ",\n";
            file << "      \"measure_points_count\": " << object.measure_points_count << ",\n";
            file << "      \"valid_points_count\": " << object.valid_points_count << ",\n";
            file << "      \"has_line_roi\": " << (object.has_line_roi ? "true" : "false") << ",\n";
            file << "      \"line_roi\": [" << object.line_x0 << ", " << object.line_y0 << ", "
                << object.line_x1 << ", " << object.line_y1 << "],\n";
            file << "      \"has_line_scan_box\": " << (object.has_line_scan_box ? "true" : "false") << ",\n";
            file << "      \"line_scan_half_width\": " << object.line_scan_half_width << ",\n";
            file << "      \"line_pointsw_count\": " << object.line_pointsw_count << ",\n";
            file << "      \"line_pointsh_count\": " << object.line_pointsh_count << ",\n";
            file << "      \"line_measure_points_count\": " << object.line_measure_points_count << ",\n";
            file << "      \"valid_line_points_count\": " << object.valid_line_points_count << ",\n";
            file << "      \"has_fit_line\": " << (object.has_fit_line ? "true" : "false") << ",\n";
            file << "      \"fit_line\": [" << object.fit_line_x0 << ", " << object.fit_line_y0 << ", "
                << object.fit_line_x1 << ", " << object.fit_line_y1 << "],\n";
            file << "      \"line_avgdist\": " << object.line_avgdist << ",\n";
            file << "      \"line_fit_mode\": \"" << EscapeJsonString(object.line_fit_mode) << "\",\n";
            file << "      \"line_fit_status\": \"" << EscapeJsonString(object.line_fit_status) << "\",\n";
            file << "      \"line_measure_status\": \"" << EscapeJsonString(object.line_measure_status) << "\"\n";
            file << "    }";

            if (i + 1 < context.runtime_objects.size())
                file << ",";

            file << "\n";
        }

        file << "  ]\n";
        file << "}\n";

        outPath = filePath.string();
        outReason = "snapshot saved";
        return true;
    }
    catch (const std::exception& e)
    {
        outReason = std::string("snapshot exception: ") + e.what();
        return false;
    }
    catch (...)
    {
        outReason = "snapshot unknown exception";
        return false;
    }
}

bool TryExecuteFindCircleRuntimeMethod(ManualTestContext& context,
    int lineIndex,
    const std::string& statement)
{
    const ParsedMethodCall call = ParseMethodCall(statement);

    if (!call.valid)
        return false;

    const bool isFindCircleRuntimeMethod =
        call.method == "measure" ||
        call.method == "fitcircle" ||
        call.method == "FitResultMeasure";

    if (!isFindCircleRuntimeMethod)
        return false;

    if (!RuntimeObjectIsType(context, call.object, "FindCircle"))
    {
        return false;
    }

    DebugCximageRuntime& runtime = CxRuntime(context);

    auto circleIt = runtime.circles.find(call.object);
    if (circleIt == runtime.circles.end() || !circleIt->second)
        return false;

    Image* image = nullptr;

    if (call.method == "measure" || call.method == "FitResultMeasure")
    {
        if (call.args.empty())
        {
            ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
            line.status = "BLOCKED";
            line.reason = call.method + " requires image argument";
            line.timestamp = CurrentTimestamp();

            context.run_state = "blocked";
            context.debug_status = "BLOCKED";
            context.debug_reason = line.reason;
            return true;
        }

        const std::string imageName = StripAddressPrefix(call.args[0]);

        auto imageIt = runtime.images.find(imageName);
        if (imageIt == runtime.images.end() || !imageIt->second)
        {
            ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
            line.status = "BLOCKED";
            line.reason = "Image runtime object missing: " + imageName;
            line.timestamp = CurrentTimestamp();

            context.run_state = "blocked";
            context.debug_status = "BLOCKED";
            context.debug_reason = line.reason;
            return true;
        }

        image = imageIt->second.get();
    }

    RuntimeObjectView& object = EnsureRuntimeObject(
        context,
        call.object,
        "FindCircle",
        context.line_views[static_cast<std::size_t>(lineIndex)].line_no);

    try
    {
        bool balancedFallbackUsed = false;
        if (call.method == "measure")
        {
            circleIt->second->measure(image);
            FillFindCircleResultView(object, *circleIt->second, "measure");
        }
        else if (call.method == "fitcircle")
        {
            circleIt->second->fitcircle();
            FillFindCircleResultView(object, *circleIt->second, "fitcircle");
        }
        else if (call.method == "FitResultMeasure")
        {
            if (!circleIt->second->canfitresultmeasure())
            {
                object.exists_in_parser = true;
                object.type = "FindCircle";
                object.last_method = "FitResultMeasure";
                object.last_runtime_status = "PENDING_BINDING";
                object.runtime_state = "fitresultmeasure_skipped";
                object.last_update_line = context.line_views[static_cast<std::size_t>(lineIndex)].line_no;
                object.visualizable = true;
                object.stale = false;

                object.display_summary =
                    "FindCircle.FitResultMeasure skipped | reason=fitcircle result is not valid";

                ScriptLineView& skipLine = context.line_views[static_cast<std::size_t>(lineIndex)];
                skipLine.status = "PENDING_BINDING";
                skipLine.reason = object.display_summary;
                skipLine.timestamp = CurrentTimestamp();

                context.current_line = FindNextNonEmptyLine(context, lineIndex + 1);
                context.run_state = "runtime_step";
                context.debug_status = "PENDING";
                context.debug_reason = skipLine.reason;

                RefreshFindCircleDisplaySnapshot(context, object);
                RefreshFindCircleMeasureGeometrySnapshot(object, *circleIt->second);

                std::ostringstream diagnostics;
                diagnostics << object.display_summary
                    << " | scan_path=" << circleIt->second->getpath().ElementCount();
                if (image != nullptr)
                    diagnostics << " | image=" << image->getWidth() << "x" << image->getHeight();
                Image* backImage = ImageManager::GetBackImage(1);
                diagnostics << " | back_image="
                    << (backImage == nullptr ? "null" :
                        std::to_string(backImage->getWidth()) + "x" +
                        std::to_string(backImage->getHeight()));
                object.display_summary = diagnostics.str();

                return true;
            }

            circleIt->second->FitResultMeasure(image);
            FillFindCircleResultView(object, *circleIt->second, "FitResultMeasure");
            object.has_result_measure =
                object.has_fit_result || object.has_measure_points;
        }

        RefreshFindCircleDisplaySnapshot(context, object);

        std::ostringstream diagnostics;
        diagnostics << object.display_summary
            << " | scan_path=" << circleIt->second->getpath().ElementCount();
        if (image != nullptr)
            diagnostics << " | image=" << image->getWidth() << "x" << image->getHeight();
        Image* backImage = ImageManager::GetBackImage(1);
        diagnostics << " | back_image="
            << (backImage == nullptr ? "null" :
                std::to_string(backImage->getWidth()) + "x" +
                std::to_string(backImage->getHeight()));
        if (balancedFallbackUsed)
            diagnostics << " | fallback=MeasureBalanced";
        object.display_summary = diagnostics.str();
    }
    catch (const std::exception& e)
    {
        ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
        line.status = "BLOCKED";
        line.reason = std::string("FindCircle runtime exception: ") + e.what();
        line.timestamp = CurrentTimestamp();

        object.last_runtime_status = "BLOCKED";
        object.runtime_state = "runtime_exception";
        object.display_summary = line.reason;

        context.run_state = "blocked";
        context.debug_status = "BLOCKED";
        context.debug_reason = line.reason;
        return true;
    }
    catch (...)
    {
        ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
        line.status = "BLOCKED";
        line.reason = "FindCircle runtime unknown exception";
        line.timestamp = CurrentTimestamp();

        object.last_runtime_status = "BLOCKED";
        object.runtime_state = "runtime_exception";
        object.display_summary = line.reason;


        context.run_state = "blocked";
        context.debug_status = "BLOCKED";
        context.debug_reason = line.reason;
        return true;
    }

    object.last_update_line = context.line_views[static_cast<std::size_t>(lineIndex)].line_no;

    ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
    line.status = object.last_runtime_status.empty()
        ? "runtime_executed"
        : object.last_runtime_status;

    line.reason = "FindCircle." + call.method +
        " executed by direct runtime bridge | " + object.display_summary;

    line.timestamp = CurrentTimestamp();
    AppendCxDebugEvent(
        context,
        "findcircle_runtime_method",
        line.line_no,
        statement,
        call.object,
        call.method,
        line.status,
        line.reason,
        object.display_summary);

    AppendCxDebugRuntimeObjectsSnapshot(
        context,
        "runtime_objects_after_" + call.method);
    context.current_line = FindNextNonEmptyLine(context, lineIndex + 1);
    context.run_state = "runtime_step";
    context.debug_status = "PENDING";
    context.debug_reason = line.reason;
    context.runtime_current_status = "PENDING";

    return true;
}

bool TryHandleFindCircleGetResult(ManualTestContext& context,
    int lineIndex,
    const std::string& statement)
{
    const ParsedMethodCall call = ParseMethodCall(statement);
    if (!call.valid || call.method != "get_result")
        return false;

    RuntimeObjectView& object = EnsureRuntimeObject(
        context, call.object, "FindCircle",
        context.line_views[static_cast<std::size_t>(lineIndex)].line_no);
    object.last_method = call.method;
    object.last_update_line =
        context.line_views[static_cast<std::size_t>(lineIndex)].line_no;

    ScriptLineView& line = context.line_views[static_cast<std::size_t>(lineIndex)];
    if (object.has_fit_result)
    {
        const std::string geometryRef = "runtime_object:" + call.object;
        object.last_runtime_status = "runtime_executed";
        object.runtime_state = "geometry_result_available";
        UpsertGlobalVariableView(context, "geometry_ref", "global_circle_ref",
            geometryRef, line.line_no, "geometry_result_available");
        line.status = "runtime_executed";
        line.reason = "get_result bound global_circle_ref to " + geometryRef;
    }
    else
    {
        object.last_runtime_status = "PENDING_BINDING";
        object.runtime_state = "pending_binding";
        object.display_summary =
            "get_result requires a valid fit result; no result fabricated";
        UpsertGlobalVariableView(context, "geometry_ref", "global_circle_ref",
            "uninitialized", line.line_no, "PENDING_BINDING");
        line.status = "PENDING_BINDING";
        line.reason = object.display_summary;
    }
    line.timestamp = CurrentTimestamp();

    context.current_line = FindNextNonEmptyLine(context, lineIndex + 1);
    context.run_state = "runtime_step";
    context.debug_status = "PENDING";
    context.debug_reason = line.reason;
    context.runtime_current_status = "PENDING";
    return true;
}