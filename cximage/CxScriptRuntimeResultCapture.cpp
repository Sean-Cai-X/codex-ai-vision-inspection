#include "pch.h"
#include "CxScriptRuntimeResultCapture.h"
#include "FindLine.h"
#include "FindCircle.h"
#include "FindEllipse.h"
#include "FindObject.h"
#include "FindRect.h"
#include "FindSegmentation.h"
#include "FastMatch.h"
#include "CxTextInspect.h"
#include "TorchTask.h"
#include "CxTorchResultProjector.h"
#include "ParserClass.h"
#include "ImageAnnotationLayer.h"
#include "shapebase.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <string>
#include <unordered_set>

namespace
{
bool ReadTextFileForRuntimeCapture(
    const std::string& path,
    std::string& text)
{
    text.clear();
    if (path.empty())
        return false;

    std::ifstream input(path);
    if (!input)
        return false;

    text.assign(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
    return true;
}

bool ExtractJsonNumberForRuntimeCapture(
    const std::string& json,
    const std::string& key,
    double& value)
{
    const std::string needle = "\"" + key + "\"";
    const std::size_t key_pos = json.find(needle);
    if (key_pos == std::string::npos)
        return false;

    const std::size_t colon_pos = json.find(':', key_pos + needle.size());
    if (colon_pos == std::string::npos)
        return false;

    const char* begin = json.c_str() + colon_pos + 1;
    while (*begin != '\0' && std::isspace(static_cast<unsigned char>(*begin)))
        ++begin;

    char* end = nullptr;
    const double parsed = std::strtod(begin, &end);
    if (end == begin)
        return false;

    value = parsed;
    return true;
}

void BackfillTorchSegmentationMetricsFromArtifacts(
    const std::string& contour_ref,
    CxScriptToolResultCapture& output)
{
    std::string contour_json;
    if (!ReadTextFileForRuntimeCapture(contour_ref, contour_json))
        return;

    double contour_count = 0.0;
    if (ExtractJsonNumberForRuntimeCapture(contour_json, "contour_count", contour_count) &&
        contour_count >= 0.0)
    {
        output.segmentation_contour_count = static_cast<int>(contour_count);
    }

    double primary_area = 0.0;
    if (ExtractJsonNumberForRuntimeCapture(contour_json, "area", primary_area) &&
        primary_area > 0.0)
    {
        output.segmentation_primary_area = primary_area;
    }
}
}

void CopyShapeElementsToSnapshots(
    const ImageAnnotationLayer& layer,
    std::vector<CxShapeElementSnapshot>& snapshots)
{
    snapshots.clear();
    for (const auto& elem : layer.ShapeElements())
    {
        CxShapeElementSnapshot snap;
        snap.stable_ref = elem.stable_ref;
        snap.owner_type = elem.owner_type;
        snap.owner_ref = elem.owner_ref;
        snap.semantic_role = elem.semantic_role;
        snap.editable = elem.editable;
        snap.result_element = elem.result_element;

        if (elem.shape)
        {
            CxShapeGeometrySnapshot geo;
            if (elem.shape->snapshot(geo))
            {
                snap.shape_kind = CxShapeKindName(geo.kind);
                snap.center_x = geo.center.x;
                snap.center_y = geo.center.y;
                snap.radius = geo.radius;
                snap.inner_radius = geo.inner_radius;
                snap.half_width = geo.half_width;
                snap.radius_x = geo.radius_x;
                snap.radius_y = geo.radius_y;
                snap.angle_deg = geo.angle;
                snap.closed = geo.closed;

                for (const auto& pt : geo.points)
                {
                    snap.points.push_back(pt.x);
                    snap.points.push_back(pt.y);
                }
            }
        }

        snapshots.push_back(snap);
    }
}

bool CaptureFindLineResult(
    FindLine& tool,
    const std::string& object_name,
    CxScriptToolResultCapture& output)
{
    output.type = "FindLine";
    output.name = object_name;
    output.owner_ref = object_name;

    output.valid_points_count = tool.getvalidpointcount();
    output.has_fit_line = tool.hasfitresult();
    output.avgdist = tool.getavgdist();
    const FindLineMeasureInputDebug& debug = tool.lastmeasureinputdebug();
    output.tool_method = debug.method;
    output.tool_threshold = debug.threshold;
    output.tool_wgap = debug.wgap;
    output.tool_hgap = debug.hgap;
    output.tool_linegap = debug.linegap;
    output.tool_min_edge_run_width_px = tool.minedgerunwidth();
    output.tool_input_line_x0 = tool.inputlinex0();
    output.tool_input_line_y0 = tool.inputliney0();
    output.tool_input_line_x1 = tool.inputlinex1();
    output.tool_input_line_y1 = tool.inputliney1();
    output.tool_input_line_half_width = debug.measure_geometry_half_width;
    output.scan_rows_examined = debug.scan_rows_examined;
    output.scan_rows_with_foreground = debug.scan_rows_with_foreground;
    output.scan_runs_total = debug.scan_runs_total;
    output.scan_runs_within_length_limit = debug.scan_runs_within_length_limit;
    output.scan_runs_over_length_limit = debug.scan_runs_over_length_limit;
    output.scan_runs_rejected_by_min_edge_width =
        debug.scan_runs_rejected_by_min_edge_width;
    output.scan_runs_rejected_by_selection = debug.scan_runs_rejected_by_selection;
    output.scan_runs_rejected_near_endpoint = debug.scan_runs_rejected_near_endpoint;
    output.scan_points_emitted = debug.scan_points_emitted;
    output.boundary_trigger_enabled =
        debug.boundary_response_enabled ? 1 : 0;
    output.boundary_trigger_mode = debug.boundary_response_mode;
    output.boundary_trigger_polarity = debug.boundary_response_polarity;
    output.boundary_trigger_selection = debug.boundary_response_selection;
    output.boundary_trigger_scan_rows_evaluated =
        debug.boundary_response_scan_rows_evaluated;
    output.boundary_trigger_candidate_count =
        debug.boundary_response_candidate_count;
    output.boundary_trigger_points_emitted =
        debug.boundary_response_points_emitted;
    output.boundary_trigger_rejected_no_candidate =
        debug.boundary_response_rejected_no_candidate;
    output.boundary_trigger_rejected_endpoint =
        debug.boundary_response_rejected_endpoint;
    output.findline_point_consistency_enabled = debug.point_consistency_enabled;
    output.findline_point_consistency_range = debug.point_consistency_range;
    output.findline_point_consistency_input_points = debug.point_consistency_input_points;
    output.findline_point_consistency_output_points = debug.point_consistency_output_points;
    output.findline_point_consistency_removed_points = debug.point_consistency_removed_points;
    output.findline_selected_edge_index = debug.selected_edge_index;
    output.findline_evaluated_edge_count = debug.evaluated_edge_count;
    output.findline_best_edge_index = debug.best_edge_index;
    output.findline_best_edge_score = debug.best_edge_score;

    const FindLineBoundaryAnalysisSnapshot boundary =
        tool.boundaryanalysissnapshot();
    output.boundary_analysis_status = boundary.status;
    output.boundary_reliability_level = boundary.reliability_level;
    output.boundary_expected_scan_count = boundary.expected_scan_count;
    output.boundary_accepted_point_count = boundary.accepted_point_count;
    output.boundary_interpolation_valid_count =
        boundary.interpolation_valid_count;
    output.boundary_fit_residual_count = boundary.fit_residual_count;
    output.boundary_coverage_ratio = boundary.coverage_ratio;
    output.boundary_response_mean = boundary.response_mean;
    output.boundary_response_median = boundary.response_median;
    output.boundary_response_cv = boundary.response_cv;
    output.boundary_subpixel_offset_mean =
        boundary.subpixel_offset_mean;
    output.boundary_subpixel_offset_stddev =
        boundary.subpixel_offset_stddev;
    output.boundary_localization_sigma_mean_px =
        boundary.localization_sigma_mean_px;
    output.boundary_residual_rmse_px = boundary.residual_rmse_px;
    output.boundary_residual_p95_px = boundary.residual_p95_px;
    output.boundary_residual_max_px = boundary.residual_max_px;
    output.boundary_outlier_ratio = boundary.outlier_ratio;
    output.boundary_reliability_score = boundary.reliability_score;
    output.boundary_points.clear();
    for (const auto& point : boundary.points)
    {
        CxFindLineBoundaryPointEvidenceSnapshot snapshot;
        snapshot.scan_index = point.scan_index;
        snapshot.scan_type = point.scan_type;
        snapshot.measured_x = point.measured_x;
        snapshot.measured_y = point.measured_y;
        snapshot.refined_x = point.refined_x;
        snapshot.refined_y = point.refined_y;
        snapshot.subpixel_offset = point.subpixel_offset;
        snapshot.response_strength = point.response_strength;
        snapshot.local_noise = point.local_noise;
        snapshot.localization_sigma_px =
            point.localization_sigma_px;
        snapshot.fit_residual_px = point.fit_residual_px;
        snapshot.polarity = point.polarity;
        snapshot.interpolation_valid = point.interpolation_valid;
        snapshot.fit_residual_valid = point.fit_residual_valid;
        snapshot.profile = point.profile;
        output.boundary_points.push_back(snapshot);
    }

    output.findline_edge_evaluations.clear();
    for (const auto& eval : debug.edge_evaluations)
    {
        if (eval.edge_index <= 0 || eval.candidate_scan_rows <= 0)
            continue;

        CxFindLineEdgeEvaluationSnapshot snap;
        snap.edge_index = eval.edge_index;
        snap.candidate_scan_rows = eval.candidate_scan_rows;
        snap.accepted_points = eval.accepted_points;
        snap.rejected_by_selection = eval.rejected_by_selection;
        snap.rejected_near_endpoint = eval.rejected_near_endpoint;
        snap.over_length_runs = eval.over_length_runs;
        snap.coverage = eval.coverage;
        snap.score = eval.score;
        snap.selected = eval.selected;
        snap.fit_possible = eval.fit_possible;
        output.findline_edge_evaluations.push_back(snap);
    }
    output.findline_scan_diagnostics.clear();
    for (int i = 0; i < tool.getscandiagnosticcount(); ++i)
    {
        FindLineMeasureInputDebug::ScanDiagnostic diag;
        CxShapePoint p0;
        CxShapePoint p1;
        if (!tool.getscandiagnostic(i, diag) ||
            !tool.getscandiagnosticline(
                diag.scan_type,
                diag.scan_index,
                p0,
                p1))
        {
            continue;
        }

        CxFindLineScanDiagnosticSnapshot snap;
        snap.scan_index = diag.scan_index;
        snap.scan_type = diag.scan_type;
        snap.x0 = p0.x;
        snap.y0 = p0.y;
        snap.x1 = p1.x;
        snap.y1 = p1.y;
        snap.candidate_count = diag.candidate_count;
        snap.binary_logical_row = diag.binary_logical_row;
        snap.binary_sample_row = diag.binary_sample_row;
        snap.binary_logical_foreground_count = diag.binary_logical_foreground_count;
        snap.binary_logical_plus1_foreground_count = diag.binary_logical_plus1_foreground_count;
        snap.binary_logical_plus2_foreground_count = diag.binary_logical_plus2_foreground_count;
        snap.binary_sample_foreground_count = diag.binary_sample_foreground_count;
        snap.threshold_run_count = diag.threshold_run_count;
        snap.threshold_max_run_width = diag.threshold_max_run_width;
        snap.min_edge_run_width_px = diag.min_edge_run_width_px;
        snap.rejected_min_edge_run_width =
            diag.rejected_min_edge_run_width;
        snap.boundary_response_used = diag.boundary_response_used;
        snap.boundary_response_mode = diag.boundary_response_mode;
        snap.boundary_response_candidate_count =
            diag.boundary_response_candidate_count;
        snap.boundary_response_selected_index =
            diag.boundary_response_selected_index;
        snap.boundary_response_selected_position =
            diag.boundary_response_selected_position;
        snap.boundary_response_selected_score =
            diag.boundary_response_selected_score;
        snap.boundary_response_status = diag.boundary_response_status;
        snap.boundary_response_reason = diag.boundary_response_reason;
        snap.threshold_first_run_start = diag.threshold_first_run_start;
        snap.threshold_first_run_end = diag.threshold_first_run_end;
        snap.threshold_last_run_start = diag.threshold_last_run_start;
        snap.threshold_last_run_end = diag.threshold_last_run_end;
        snap.accepted = diag.accepted;
        snap.accepted_x = diag.accepted_x;
        snap.accepted_y = diag.accepted_y;
        snap.reject_reason = diag.reject_reason;
        output.findline_scan_diagnostics.push_back(snap);
    }
    output.actual_findsetting = debug.objfilterset;
    output.object_prefilter_requested = (debug.objfilterset & 0x01) != 0;
    output.object_prefilter_applied = debug.findobject_measure_called;
    output.object_filter_strategy_id = debug.findobject_strategy_id;
    output.object_filter_borw = debug.effective_filter_borw;
    output.object_filter_min = debug.effective_filter_min;
    output.object_filter_max = debug.effective_filter_max;
    output.object_component_count = debug.findobject_component_total;
    output.object_component_accepted_count = debug.findobject_component_accepted;
    output.object_component_rejected_count =
        debug.findobject_component_rejected_by_min +
        debug.findobject_component_rejected_by_max +
        debug.findobject_component_rejected_by_borw;
    output.object_component_max_area = debug.findobject_area_max_observed;
    // FindLine currently records component-area distribution only.  Keep
    // unavailable width/height explicit as zero rather than inventing values.
    output.object_component_max_width = 0;
    output.object_component_max_height = 0;
    output.object_foreground_before = debug.findobject_foreground_before;
    output.object_foreground_after = debug.findobject_foreground_after;
    output.object_white_component_count = debug.cc_white.component_total;
    output.object_white_accepted_count = debug.cc_white.accepted_by_area;
    output.object_white_rejected_count =
        debug.cc_white.rejected_by_min + debug.cc_white.rejected_by_max;
    output.object_black_component_count = debug.cc_black.component_total;
    output.object_black_accepted_count = debug.cc_black.accepted_by_area;
    output.object_black_rejected_count =
        debug.cc_black.rejected_by_min + debug.cc_black.rejected_by_max;
    output.object_algorithm_branch = debug.findobject_algorithm_branch;
    output.budget_exceeded = tool.budgetexceeded();
    if (output.has_fit_line)
    {
        output.failure_stage.clear();
    }
    else if (debug.original_point_count > 0 && output.valid_points_count <= 0)
    {
        output.failure_stage = "findline_measure_points_below_fit_min";
    }
    else if (output.valid_points_count > 0 || debug.scan_points_emitted > 0)
    {
        output.failure_stage = "findline_fail_fit_degenerate";
    }
    else
    {
        output.failure_stage = tool.getfailurestage();
    }

    if (output.has_fit_line)
    {
        output.fit_line_x0 = tool.getresultx0();
        output.fit_line_y0 = tool.getresulty0();
        output.fit_line_x1 = tool.getresultx1();
        output.fit_line_y1 = tool.getresulty1();
    }

    ImageAnnotationLayer layer;
    tool.PublishDisplayShapes(layer, output.owner_ref);
    CopyShapeElementsToSnapshots(layer, output.shapes);

    if (!output.boundary_points.empty())
    {
        CxShapeElementSnapshot refined;
        refined.stable_ref = output.owner_ref + ":boundary_refined_points";
        refined.owner_type = "FindLine";
        refined.owner_ref = output.owner_ref;
        refined.semantic_role = "boundary_refined_points";
        refined.editable = false;
        refined.result_element = true;
        refined.shape_kind = "PointsShape";
        for (const auto& point : output.boundary_points)
        {
            if (!point.interpolation_valid)
                continue;
            refined.points.push_back(point.refined_x);
            refined.points.push_back(point.refined_y);
        }
        if (!refined.points.empty())
            output.shapes.push_back(refined);
    }


    return true;
}

bool CaptureFindCircleResult(
    FindCircle& tool,
    const std::string& object_name,
    CxScriptToolResultCapture& output)
{
    output.type = "FindCircle";
    output.name = object_name;
    output.owner_ref = object_name;

    output.valid_points_count = tool.getvalidpointcount();
    output.has_fit_circle = tool.hasfitresult();
    output.circle_cx = tool.getresultcentx();
    output.circle_cy = tool.getresultcenty();
    output.circle_radius = tool.getradius();
    output.avgdist = tool.getavgdist();
    output.tool_method = tool.getmethod();
    output.tool_threshold = tool.getthreshold();
    output.tool_wgap = tool.getgap();
    output.tool_linegap = tool.getlinegap();
    output.tool_min_edge_run_width_px = tool.minedgerunwidth();
    output.tool_input_circle_cx = tool.getcirclecentx();
    output.tool_input_circle_cy = tool.getcirclecenty();
    output.tool_input_circle_px = tool.getcirclepax();
    output.tool_input_circle_py = tool.getcirclepay();
    output.tool_input_circle_gap = tool.getgap();
    output.actual_findsetting = tool.getfindsetting();
    output.object_prefilter_requested = (tool.getfindsetting() & 0x01) != 0;
    output.object_prefilter_applied = tool.getdebugprefilterused() != 0;
    output.object_filter_borw = tool.getfilterborw();
    output.object_filter_min = tool.getfiltermin();
    output.object_filter_max = tool.getfiltermax();
    output.fit_filter_input_count = tool.getfitfilterinputcount();
    output.fit_filter_kept_count = tool.getfitfilterkeptcount();
    output.fit_filter_rejected_count = tool.getfitfilterrejectedcount();
    output.fit_filter_sigma = tool.getfitfiltersigma();
    output.fit_filter_threshold = tool.getfitfilterthreshold();
    output.circle_point_consistency_enabled =
        tool.getpointconsistencyenabled();
    output.circle_point_consistency_range =
        tool.getpointconsistencyrange();
    output.circle_point_consistency_input_points =
        tool.getpointconsistencyinputcount();
    output.circle_point_consistency_output_points =
        tool.getpointconsistencyoutputcount();
    output.circle_point_consistency_removed_points =
        tool.getpointconsistencyremovedcount();

    const FindCircleMeasureGeometryDebug& geometry_debug =
        tool.lastmeasuregeometrydebug();
    output.scan_runs_rejected_by_min_edge_width =
        geometry_debug.candidate_min_edge_run_width_reject_count;
    output.boundary_trigger_enabled =
        geometry_debug.boundary_response_enabled;
    output.boundary_trigger_mode =
        geometry_debug.boundary_response_mode;
    output.boundary_trigger_polarity =
        geometry_debug.boundary_response_polarity;
    output.boundary_trigger_selection =
        geometry_debug.boundary_response_selection;
    output.boundary_trigger_scan_rows_evaluated =
        geometry_debug.boundary_response_scan_lines_evaluated;
    output.boundary_trigger_candidate_count =
        geometry_debug.boundary_response_candidate_count;
    output.boundary_trigger_points_emitted =
        geometry_debug.boundary_response_points_emitted;
    output.boundary_trigger_rejected_no_candidate =
        geometry_debug.boundary_response_rejected_no_candidate;
    output.boundary_trigger_rejected_endpoint =
        geometry_debug.boundary_response_rejected_endpoint;
    output.findline_scan_diagnostics.clear();
    for (int i = 0; i < tool.getscandiagnosticcount(); ++i)
    {
        FindCircleMeasureGeometryDebug::ScanDiagnostic diag;
        if (!tool.getscandiagnostic(i, diag))
            continue;

        CxFindLineScanDiagnosticSnapshot snap;
        snap.scan_index = diag.scan_index;
        snap.scan_type = 2;
        snap.candidate_count = diag.candidate_count;
        snap.min_edge_run_width_px = diag.min_edge_run_width_px;
        snap.rejected_min_edge_run_width = diag.rejected_min_edge_run_width;
        snap.accepted = diag.accepted;
        snap.accepted_x = diag.accepted_x;
        snap.accepted_y = diag.accepted_y;
        snap.boundary_response_used = diag.boundary_response_used;
        snap.boundary_response_mode = diag.boundary_response_mode;
        snap.boundary_response_candidate_count =
            diag.boundary_response_candidate_count;
        snap.boundary_response_selected_index =
            diag.boundary_response_selected_index;
        snap.boundary_response_selected_position =
            diag.boundary_response_selected_position;
        snap.boundary_response_selected_score =
            diag.boundary_response_selected_score;
        snap.boundary_response_status = diag.boundary_response_status;
        snap.boundary_response_reason = diag.boundary_response_reason;
        snap.reject_reason = diag.reject_reason;
        CxShapePoint p0;
        CxShapePoint p1;
        if (tool.getscandiagnosticline(diag.scan_index, p0, p1))
        {
            snap.x0 = p0.x;
            snap.y0 = p0.y;
            snap.x1 = p1.x;
            snap.y1 = p1.y;
        }
        output.findline_scan_diagnostics.push_back(snap);
    }

    const FindCircleBoundaryAnalysisSnapshot boundary =
        tool.boundaryanalysissnapshot();
    output.boundary_analysis_status = boundary.status;
    output.boundary_reliability_level = boundary.reliability_level;
    output.boundary_expected_scan_count = boundary.expected_scan_count;
    output.boundary_accepted_point_count = boundary.accepted_point_count;
    output.boundary_interpolation_valid_count =
        boundary.interpolation_valid_count;
    output.boundary_fit_residual_count = boundary.fit_residual_count;
    output.boundary_coverage_ratio = boundary.coverage_ratio;
    output.boundary_response_mean = boundary.response_mean;
    output.boundary_response_median = boundary.response_median;
    output.boundary_response_cv = boundary.response_cv;
    output.boundary_subpixel_offset_mean = boundary.subpixel_offset_mean;
    output.boundary_subpixel_offset_stddev = boundary.subpixel_offset_stddev;
    output.boundary_localization_sigma_mean_px =
        boundary.localization_sigma_mean_px;
    output.boundary_residual_rmse_px = boundary.residual_rmse_px;
    output.boundary_residual_p95_px = boundary.residual_p95_px;
    output.boundary_residual_max_px = boundary.residual_max_px;
    output.boundary_outlier_ratio = boundary.outlier_ratio;
    output.boundary_reliability_score = boundary.reliability_score;
    output.boundary_points.clear();
    for (const auto &point : boundary.points)
    {
        CxFindLineBoundaryPointEvidenceSnapshot snapshot;
        snapshot.scan_index = point.scan_index;
        snapshot.scan_type = 2;
        snapshot.measured_x = point.measured_x;
        snapshot.measured_y = point.measured_y;
        snapshot.refined_x = point.refined_x;
        snapshot.refined_y = point.refined_y;
        snapshot.subpixel_offset = point.subpixel_offset;
        snapshot.response_strength = point.response_strength;
        snapshot.local_noise = point.local_noise;
        snapshot.localization_sigma_px = point.localization_sigma_px;
        snapshot.fit_residual_px = point.fit_residual_px;
        snapshot.polarity = point.polarity;
        snapshot.interpolation_valid = point.interpolation_valid;
        snapshot.fit_residual_valid = point.fit_residual_valid;
        snapshot.profile = point.profile;
        output.boundary_points.push_back(snapshot);
    }
    output.budget_exceeded = tool.budgetexceeded();
    if (output.has_fit_circle)
    {
        output.failure_stage.clear();
    }
    else if (output.valid_points_count > 0)
    {
        output.failure_stage = "findcircle_fit_degenerate_after_measure_points";
    }
    else
    {
        output.failure_stage = tool.getfailurestage();
    }

    ImageAnnotationLayer layer;
    tool.PublishDisplayShapes(layer, output.owner_ref);
    CopyShapeElementsToSnapshots(layer, output.shapes);

    if (!output.boundary_points.empty())
    {
        CxShapeElementSnapshot refined;
        refined.stable_ref = output.owner_ref + ":boundary_refined_points";
        refined.owner_type = "FindCircle";
        refined.owner_ref = output.owner_ref;
        refined.semantic_role = "boundary_refined_points";
        refined.editable = false;
        refined.result_element = true;
        refined.shape_kind = "PointsShape";
        for (const auto &point : output.boundary_points)
        {
            if (!point.interpolation_valid)
                continue;
            refined.points.push_back(point.refined_x);
            refined.points.push_back(point.refined_y);
        }
        if (!refined.points.empty())
            output.shapes.push_back(refined);
    }

    return true;
}

bool CaptureFindEllipseResult(
    FindEllipse& tool,
    const std::string& object_name,
    CxScriptToolResultCapture& output)
{
    output.type = "FindEllipse";
    output.name = object_name;
    output.owner_ref = object_name;

    FindEllipseDisplaySnapshot snapshot;
    const bool has_snapshot = tool.getdisplaysnapshot(snapshot);
    output.valid_points_count = snapshot.measure_points_count;
    output.has_fit_ellipse = tool.hasfitresult() != 0.0;
    output.ellipse_cx = tool.getresultcentx();
    output.ellipse_cy = tool.getresultcenty();
    output.ellipse_radius_x = tool.getresultradiusx();
    output.ellipse_radius_y = tool.getresultradiusy();
    output.ellipse_angle_deg = tool.getresultangle();
    output.avgdist = tool.getavgdist();
    output.tool_method = snapshot.method;
    output.tool_threshold = snapshot.threshold;
    output.tool_wgap = snapshot.gap;
    output.tool_linegap = snapshot.linegap;
    output.tool_min_edge_run_width_px = tool.minedgerunwidth();
    output.scan_runs_rejected_by_min_edge_width =
        snapshot.rejected_min_edge_run_width_count;
    output.boundary_trigger_enabled =
        snapshot.boundary_response_enabled;
    output.boundary_trigger_mode =
        snapshot.boundary_response_mode;
    output.boundary_trigger_polarity =
        snapshot.boundary_response_polarity;
    output.boundary_trigger_selection =
        snapshot.boundary_response_selection;
    output.boundary_trigger_scan_rows_evaluated =
        snapshot.boundary_response_scan_lines_evaluated;
    output.boundary_trigger_candidate_count =
        snapshot.boundary_response_candidate_count;
    output.boundary_trigger_points_emitted =
        snapshot.boundary_response_points_emitted;
    output.boundary_trigger_rejected_no_candidate =
        snapshot.boundary_response_rejected_no_candidate;
    output.boundary_trigger_rejected_endpoint =
        snapshot.boundary_response_rejected_endpoint;
    output.findline_scan_diagnostics.clear();
    for (int i = 0; i < tool.getscandiagnosticcount(); ++i)
    {
        FindEllipseMeasureGeometryDebug::ScanDiagnostic diag;
        if (!tool.getscandiagnostic(i, diag))
            continue;

        CxFindLineScanDiagnosticSnapshot snap;
        snap.scan_index = diag.scan_index;
        snap.scan_type = 3;
        snap.candidate_count = diag.candidate_count;
        snap.min_edge_run_width_px = diag.min_edge_run_width_px;
        snap.rejected_min_edge_run_width = diag.rejected_min_edge_run_width;
        snap.accepted = diag.accepted;
        snap.accepted_x = diag.accepted_x;
        snap.accepted_y = diag.accepted_y;
        snap.boundary_response_used = diag.boundary_response_used;
        snap.boundary_response_mode = diag.boundary_response_mode;
        snap.boundary_response_candidate_count =
            diag.boundary_response_candidate_count;
        snap.boundary_response_selected_index =
            diag.boundary_response_selected_index;
        snap.boundary_response_selected_position =
            diag.boundary_response_selected_position;
        snap.boundary_response_selected_score =
            diag.boundary_response_selected_score;
        snap.boundary_response_status = diag.boundary_response_status;
        snap.boundary_response_reason = diag.boundary_response_reason;
        snap.reject_reason = diag.reject_reason;
        CxShapePoint p0;
        CxShapePoint p1;
        if (tool.getscandiagnosticline(diag.scan_index, p0, p1))
        {
            snap.x0 = p0.x;
            snap.y0 = p0.y;
            snap.x1 = p1.x;
            snap.y1 = p1.y;
        }
        output.findline_scan_diagnostics.push_back(snap);
    }
    output.actual_findsetting = tool.getfindsetting();
    output.object_prefilter_requested = (tool.getfindsetting() & 0x01) != 0;
    output.object_prefilter_applied = tool.getdebugprefilterused() != 0;
    output.object_filter_borw = tool.getfilterborw();
    output.object_filter_min = tool.getfiltermin();
    output.object_filter_max = tool.getfiltermax();
    output.object_component_count = tool.getdebugprefiltercomponentcount();
    output.object_component_accepted_count =
        tool.getdebugprefilteracceptedcount();
    output.object_component_rejected_count =
        tool.getdebugprefilterrejectedcount();
    output.object_component_max_area = tool.getdebugprefiltermaxarea();
    output.object_component_max_width = tool.getdebugprefiltermaxw();
    output.object_component_max_height = tool.getdebugprefiltermaxh();
    output.object_foreground_before =
        tool.getdebugprefilterforegroundbefore();
    output.object_foreground_after =
        tool.getdebugprefilterforegroundafter();
    output.object_algorithm_branch =
        output.object_prefilter_applied ? "FindEllipse.measureRobust.prefilter"
                                        : std::string();
    output.failure_stage = output.has_fit_ellipse
        ? std::string()
        : (snapshot.measure_points_count > 0 ? "fitellipse" : "measure_points");

    output.ellipse_selected_edge_index = snapshot.selected_edge_index;
    output.ellipse_scan_candidate_lines = snapshot.scan_candidate_lines;
    output.ellipse_scan_total_candidates = snapshot.scan_total_candidates;
    output.ellipse_scan_accepted_points_before_gate = snapshot.scan_accepted_points_before_gate;
    output.ellipse_accepted_min_boundary_ratio = snapshot.accepted_min_boundary_ratio;
    output.ellipse_accepted_max_boundary_ratio = snapshot.accepted_max_boundary_ratio;
    output.ellipse_accepted_avg_boundary_ratio = snapshot.accepted_avg_boundary_ratio;
    output.ellipse_candidate_policy = snapshot.candidate_policy;

    output.ellipse_scan_lines_outside_roi_count = snapshot.scan_lines_outside_roi_count;
    output.ellipse_scan_lines_cross_outside_ellipse_count = snapshot.scan_lines_cross_outside_ellipse_count;
    output.ellipse_scan_endpoint_norm_min = snapshot.scan_endpoint_norm_min;
    output.ellipse_scan_endpoint_norm_avg = snapshot.scan_endpoint_norm_avg;
    output.ellipse_scan_endpoint_norm_max = snapshot.scan_endpoint_norm_max;
    output.ellipse_accepted_points_outside_ellipse_count = snapshot.accepted_points_outside_ellipse_count;
    output.ellipse_accepted_point_norm_min = snapshot.accepted_point_norm_min;
    output.ellipse_accepted_point_norm_avg = snapshot.accepted_point_norm_avg;
    output.ellipse_accepted_point_norm_max = snapshot.accepted_point_norm_max;
    output.ellipse_rejected_boundary_band_candidate_count =
        snapshot.rejected_boundary_band_candidate_count;
    output.ellipse_rejected_boundary_band_norm_min =
        snapshot.rejected_boundary_band_norm_min;
    output.ellipse_rejected_boundary_band_norm_avg =
        snapshot.rejected_boundary_band_norm_avg;
    output.ellipse_rejected_boundary_band_norm_max =
        snapshot.rejected_boundary_band_norm_max;
    output.ellipse_scan_geometry_policy = snapshot.scan_geometry_policy;
    output.ellipse_point_consistency_enabled = snapshot.point_consistency_enabled;
    output.ellipse_point_consistency_range = snapshot.point_consistency_range;
    output.ellipse_point_consistency_input_points = snapshot.point_consistency_input_points;
    output.ellipse_point_consistency_output_points = snapshot.point_consistency_output_points;
    output.ellipse_point_consistency_removed_points = snapshot.point_consistency_removed_points;

    if (!snapshot.measure_failure_stage.empty())
        output.failure_stage = snapshot.measure_failure_stage;

    if (!has_snapshot)
        output.reason = "FindEllipse display snapshot is empty";
    else if (!snapshot.measure_failure_stage.empty())
    {
        output.reason = snapshot.measure_failure_reason;
        if (snapshot.scan_candidate_lines > 0)
        {
            output.reason += " candidate_lines=" + std::to_string(snapshot.scan_candidate_lines);
            output.reason += " total_candidates=" + std::to_string(snapshot.scan_total_candidates);
            output.reason += " accepted_before_gate=" + std::to_string(snapshot.scan_accepted_points_before_gate);
            output.reason += " boundary_ratio=" + std::to_string(snapshot.accepted_min_boundary_ratio);
            output.reason += "/" + std::to_string(snapshot.accepted_avg_boundary_ratio);
            output.reason += "/" + std::to_string(snapshot.accepted_max_boundary_ratio);
            output.reason += " candidate_policy=" + snapshot.candidate_policy;
        }
        if (!snapshot.scan_geometry_policy.empty())
        {
            output.reason += " scan_lines=" + std::to_string(snapshot.scan_line_count);
            output.reason += " scan_len=" + std::to_string(snapshot.scan_line_length);
            output.reason += " scan_outside_lines=" + std::to_string(snapshot.scan_lines_cross_outside_ellipse_count);
            output.reason += " endpoint_norm=" + std::to_string(snapshot.scan_endpoint_norm_min);
            output.reason += "/" + std::to_string(snapshot.scan_endpoint_norm_avg);
            output.reason += "/" + std::to_string(snapshot.scan_endpoint_norm_max);
            output.reason += " accepted_outside=" + std::to_string(snapshot.accepted_points_outside_ellipse_count);
            output.reason += " accepted_norm=" + std::to_string(snapshot.accepted_point_norm_min);
            output.reason += "/" + std::to_string(snapshot.accepted_point_norm_avg);
            output.reason += "/" + std::to_string(snapshot.accepted_point_norm_max);
            output.reason += " rejected_boundary_band=" +
                std::to_string(snapshot.rejected_boundary_band_candidate_count);
            output.reason += " rejected_norm=" +
                std::to_string(snapshot.rejected_boundary_band_norm_min);
            output.reason += "/" + std::to_string(snapshot.rejected_boundary_band_norm_avg);
            output.reason += "/" + std::to_string(snapshot.rejected_boundary_band_norm_max);
            output.reason += " scan_policy=" + snapshot.scan_geometry_policy;
            output.reason += " selected_edge=" + std::to_string(snapshot.selected_edge_index);
            output.reason += " consistency=" +
                std::to_string(snapshot.point_consistency_enabled);
            output.reason += "/" +
                std::to_string(static_cast<int>(snapshot.point_consistency_range));
            output.reason += " consistency_in_out_removed=" +
                std::to_string(snapshot.point_consistency_input_points);
            output.reason += "/" +
                std::to_string(snapshot.point_consistency_output_points);
            output.reason += "/" +
                std::to_string(snapshot.point_consistency_removed_points);
        }
    }
    else if (!output.has_fit_ellipse && snapshot.measure_points_count > 0)
        output.reason = "FindEllipse produced measure points, but fitellipse result is unavailable.";
    else if (!output.has_fit_ellipse)
        output.reason = "FindEllipse produced zero measure points.";

    ImageAnnotationLayer layer;
    tool.PublishDisplayShapes(layer, output.owner_ref);
    CopyShapeElementsToSnapshots(layer, output.shapes);

    return true;
}

bool CaptureFindRectResult(
    FindRect& tool,
    const std::string& object_name,
    CxScriptToolResultCapture& output)
{
    output.type = "FindRect";
    output.name = object_name;
    output.owner_ref = object_name;

    output.result_rect_count = tool.getresultobjsnum();
    output.has_result_rect = tool.hasresult();
    output.valid_points_count = output.result_rect_count;
    output.findrect_seed_valid = tool.getdebugseedvalid() != 0;
    output.findrect_top_valid = tool.getdebugtopvalid() != 0;
    output.findrect_bottom_valid = tool.getdebugbottomvalid() != 0;
    output.findrect_left_valid = tool.getdebugleftvalid() != 0;
    output.findrect_right_valid = tool.getdebugrightvalid() != 0;
    output.findrect_top_points = tool.getdebugtoppoints();
    output.findrect_bottom_points = tool.getdebugbottompoints();
    output.findrect_left_points = tool.getdebugleftpoints();
    output.findrect_right_points = tool.getdebugrightpoints();
    output.findrect_coarse_score = tool.getdebugcoarsescore();
    output.findrect_refine_score = tool.getdebugrefinescore();
    output.failure_stage = output.has_result_rect
        ? std::string()
        : tool.getfailurestage();
    if (output.failure_stage.empty() && !output.has_result_rect)
        output.failure_stage = "result_rect";

    if (!output.has_result_rect)
        output.reason = "FindRect result unavailable";

    ImageAnnotationLayer layer;
    tool.PublishDisplayShapes(layer, output.owner_ref);
    CopyShapeElementsToSnapshots(layer, output.shapes);

    return true;
}

bool CaptureFindObjectResult(
    FindObject& tool,
    const std::string& object_name,
    CxScriptToolResultCapture& output)
{
    output.type = "FindObject";
    output.name = object_name;
    output.owner_ref = object_name;

    output.result_rect_count = tool.getresultobjsnum();
    output.has_result_rect = output.result_rect_count > 0;
    output.valid_points_count = output.result_rect_count;
    output.object_component_count = tool.getdebugcomponentcount();
    output.object_component_accepted_count = tool.getdebugacceptedcount();
    output.object_component_rejected_count = tool.getdebugrejectedcount();
    output.object_component_max_area = tool.getdebugmaxcomponentarea();
    output.object_component_max_width = tool.getdebugmaxcomponentw();
    output.object_component_max_height = tool.getdebugmaxcomponenth();
    output.object_algorithm_branch = tool.getdebugalgorithmbranch();

    output.findobject_measurements.clear();
    output.findobject_measurements.reserve(tool.getmeasurements().size());
    for (const FindObjectMeasurementSnapshot& source :
         tool.getmeasurements())
    {
        CxFindObjectMeasurementEvidence measured;
        measured.object_index = source.object_index;
        measured.component_label = source.component_label;
        measured.generation = source.generation;
        measured.mask_hash = source.mask_hash;
        measured.object_ref = source.object_ref;
        measured.bbox_semantics = source.bbox_semantics;
        measured.centroid_semantics = source.centroid_semantics;
        measured.bbox_x = source.bbox_px.x;
        measured.bbox_y = source.bbox_px.y;
        measured.bbox_w = source.bbox_px.width;
        measured.bbox_h = source.bbox_px.height;
        measured.pixel_count = source.pixel_count;
        measured.pixel_size_x = source.pixel_size_x;
        measured.pixel_size_y = source.pixel_size_y;
        measured.centroid_x = source.centroid_px.x;
        measured.centroid_y = source.centroid_px.y;
        measured.projected_area = source.projected_area;
        measured.equivalent_side = source.equivalent_side;
        measured.equivalent_radius = source.equivalent_radius;
        measured.intensity_min = source.intensity_min;
        measured.intensity_max = source.intensity_max;
        measured.intensity_mean = source.intensity_mean;
        measured.intensity_stddev = source.intensity_stddev;
        measured.intensity_skewness = source.intensity_skewness;
        measured.intensity_valid = source.intensity_valid;
        measured.background_method = source.background_method;
        measured.background_valid = source.background_valid;
        measured.background_sample_count = source.background_sample_count;
        measured.background_baseline_mean = source.background_baseline_mean;
        measured.residual_threshold = source.residual_threshold;
        measured.segmentation_domain = source.segmentation_domain;
        measured.gw_pixel_perimeter = source.gw_pixel_perimeter;
        measured.subpixel_valid = source.subpixel_valid;
        measured.subpixel_candidate_point_count =
            source.subpixel_candidate_point_count;
        measured.subpixel_accepted_point_count =
            source.subpixel_accepted_point_count;
        measured.subpixel_rejected_low_gradient_count =
            source.subpixel_rejected_low_gradient_count;
        measured.subpixel_rejected_out_of_bounds_count =
            source.subpixel_rejected_out_of_bounds_count;
        measured.subpixel_mean_gradient = source.subpixel_mean_gradient;
        measured.subpixel_mean_fit_residual_px =
            source.subpixel_mean_fit_residual_px;
        measured.subpixel_perimeter = source.subpixel_perimeter;
        measured.subpixel_area = source.subpixel_area;
        measured.subpixel_boundary_point_count =
            static_cast<int>(source.subpixel_boundary.size());
        measured.subpixel_failure_reason = source.subpixel_failure_reason;
        measured.active_geometry_basis = source.active_geometry_basis;
        measured.gwyddion_perimeter = source.gwyddion_perimeter;
        measured.polygon_perimeter = source.polygon_perimeter;
        measured.polygon_area = source.polygon_area;
        measured.pixel_configuration_counts =
            source.pixel_configuration_counts;
        measured.outer_boundary_point_count =
            static_cast<int>(source.outer_boundary.size());
        measured.hole_count =
            static_cast<int>(source.hole_boundaries.size());
        measured.convex_hull_area = source.convex_hull_area;
        measured.circularity = source.circularity;
        measured.solidity = source.solidity;
        measured.major_axis_length = source.major_axis_length;
        measured.minor_axis_length = source.minor_axis_length;
        measured.orientation_deg = source.orientation_deg;
        measured.aspect_ratio = source.aspect_ratio;
        measured.eccentricity = source.eccentricity;
        measured.moment_ellipse_valid = source.moment_ellipse_valid;
        measured.feret_max = source.feret_max;
        measured.feret_max_angle_deg = source.feret_max_angle_deg;
        measured.feret_min = source.feret_min;
        measured.feret_min_angle_deg = source.feret_min_angle_deg;
        measured.inscribed_center_x =
            source.inscribed_circle_center_px.x;
        measured.inscribed_center_y =
            source.inscribed_circle_center_px.y;
        measured.inscribed_radius = source.inscribed_circle_radius;
        measured.inscribed_circle_valid =
            source.inscribed_circle_valid;
        measured.enclosing_center_x =
            source.enclosing_circle_center_px.x;
        measured.enclosing_center_y =
            source.enclosing_circle_center_px.y;
        measured.enclosing_radius = source.enclosing_circle_radius;
        measured.enclosing_circle_valid =
            source.enclosing_circle_valid;
        measured.length_unit = source.length_unit;
        measured.area_unit = source.area_unit;
        measured.boundary_method = source.boundary_method;
        measured.topology_policy = source.topology_policy;
        measured.inscribed_circle_method =
            source.inscribed_circle_method;
        measured.status = source.status;
        output.findobject_measurements.push_back(std::move(measured));
    }
    output.failure_stage = output.has_result_rect ? std::string() : "result_rect";
    if (!output.has_result_rect)
        output.reason = "FindObject result unavailable";

    if (output.result_rect_count > 0)
    {
        output.top1_rect_x = tool.getresultx(0);
        output.top1_rect_y = tool.getresulty(0);
        output.top1_rect_w = tool.getresultw(0);
        output.top1_rect_h = tool.getresulth(0);
    }
    ImageAnnotationLayer layer;
    tool.PublishDisplayShapes(layer, output.owner_ref);
    CopyShapeElementsToSnapshots(layer, output.shapes);

    return true;
}

bool CaptureFastMatchResult(
    FastMatch& tool,
    const std::string& object_name,
    CxScriptToolResultCapture& output)
{
    output.type = "FastMatch";
    output.name = object_name;
    output.owner_ref = object_name;

    output.fastmatch_learn_a_count = tool.getlearnacount();
    output.fastmatch_learn_b_count = tool.getlearnbcount();
    output.fastmatch_learn_a2_count = tool.getlearna2count();
    output.fastmatch_learn_b2_count = tool.getlearnb2count();
    output.fastmatch_learn_status_code = tool.getlearnstatuscode();
    output.model_point_count = tool.getmodelpointcount();
    output.fastmatch_model_width = tool.getmodelwidth();
    output.fastmatch_model_height = tool.getmodelheight();
    output.fastmatch_pattern_a_count = tool.getpatternapointcount();
    output.fastmatch_pattern_b_count = tool.getpatternbpointcount();
    output.fastmatch_pattern_a_x = tool.getpatternax();
    output.fastmatch_pattern_a_y = tool.getpatternay();
    output.fastmatch_pattern_a_width = tool.getpatternawidth();
    output.fastmatch_pattern_a_height = tool.getpatternaheight();
    output.fastmatch_pattern_b_x = tool.getpatternbx();
    output.fastmatch_pattern_b_y = tool.getpatternby();
    output.fastmatch_pattern_b_width = tool.getpatternbwidth();
    output.fastmatch_pattern_b_height = tool.getpatternbheight();
    output.candidate_count = tool.getresultcandidatecount();
    output.best_score = tool.getresultbestscore();
    output.has_best_result = tool.getresultbestindex() >= 0;
    output.has_result_box = tool.getresultnum(0) > 0.0;
    output.valid_points_count = output.candidate_count;

    const RectsShape* result_rects = tool.getresultrects();
    output.result_rect_count = result_rects != nullptr ? result_rects->size() : 0;
    output.has_result_rect = output.result_rect_count > 0;
    if (output.has_result_rect)
    {
        int rect_index = tool.getresultbestindex();
        if (rect_index < 0 || rect_index >= output.result_rect_count)
            rect_index = 0;

        const gp_Rectangle top_rect = tool.getresolvedresultrect(rect_index);
        output.top1_rect_x = static_cast<int>(top_rect.TopLeft().X());
        output.top1_rect_y = static_cast<int>(top_rect.TopLeft().Y());
        output.top1_rect_w = static_cast<int>(top_rect.Width());
        output.top1_rect_h = static_cast<int>(top_rect.Height());
    }

    output.fastmatch_match_call_count = tool.getmatchcallcount();
    output.fastmatch_match_ab_call_count = tool.getmatchabcallcount();
    output.fastmatch_match_sample_ab_call_count = tool.getmatchsampleabcallcount();
    output.fastmatch_match_last_stage = tool.getmatchlaststage();
    output.fastmatch_match_image_width = tool.getmatchimagewidth();
    output.fastmatch_match_image_height = tool.getmatchimageheight();
    output.fastmatch_learn_rect_x0 = tool.getlearnrectx0();
    output.fastmatch_learn_rect_y0 = tool.getlearnrecty0();
    output.fastmatch_learn_rect_x1 = tool.getlearnrectx1();
    output.fastmatch_learn_rect_y1 = tool.getlearnrecty1();
    output.fastmatch_match_rect_x0 = tool.getmatchrectx0();
    output.fastmatch_match_rect_y0 = tool.getmatchrecty0();
    output.fastmatch_match_rect_x1 = tool.getmatchrectx1();
    output.fastmatch_match_rect_y1 = tool.getmatchrecty1();
    output.fastmatch_raw_probe_count = tool.getrawmatchprobecount();
    output.fastmatch_raw_threshold_hit_count = tool.getrawmatchthresholdhitcount();
    output.fastmatch_result_to_list_count = tool.getresulttolistcallcount();
    output.fastmatch_candidate_insert_count = tool.getresultcandidateinsertcount();
    output.fastmatch_candidate_replace_count = tool.getresultcandidatereplacecount();
    output.fastmatch_candidate_reject_count = tool.getresultcandidaterejectcount();
    output.fastmatch_rotate_budget_exceeded = tool.getrotatebudgetexceeded() != 0;
    output.fastmatch_rotate_candidate_count = tool.getrotatecandidatecount();
    output.fastmatch_rotate_probe_count = tool.getrotateprobecount();
    output.fastmatch_rotate_sample_count = tool.getrotatesamplecount();
    output.fastmatch_rotate_elapsed_ms = tool.getrotateelapsedms();

    const FastMatch::NormalTraceEvidence& normal_trace =
        tool.getnormaltraceevidence();
    CxFastMatchNormalTraceEvidence& normal_trace_capture =
        output.fastmatch_normal_trace;
    normal_trace_capture.executed = normal_trace.executed;
    normal_trace_capture.succeeded = normal_trace.succeeded;
    normal_trace_capture.reason = normal_trace.reason;
    normal_trace_capture.candidate_count = tool.getnormaltracecandidatecount();
    normal_trace_capture.deduplicated_count =
        tool.getnormaltracededuplicatedcount();
    normal_trace_capture.trace_point_count = tool.getnormaltracepointcount();
    normal_trace_capture.normal_pair_count = tool.getnormaltracepaircount();
    normal_trace_capture.directional_side_count =
        normal_trace.directional_side_count;
    normal_trace_capture.trace_segment_count = normal_trace.trace_segment_count;
    normal_trace_capture.closure_error_px = normal_trace.closure_error_px;
    normal_trace_capture.max_consecutive_gap_px =
        normal_trace.max_consecutive_gap_px;
    normal_trace_capture.gradient_coverage = normal_trace.gradient_coverage;
    normal_trace_capture.selected_anchor_coverage =
        normal_trace.selected_anchor_coverage;
    normal_trace_capture.generated_conclusion_count =
        normal_trace.generated_conclusion_count;
    normal_trace_capture.normal_pair_findline_bound_count =
        normal_trace.normal_pair_findline_bound_count;
    normal_trace_capture.normal_pair_binding_miss_count =
        normal_trace.normal_pair_binding_miss_count;
    normal_trace_capture.normal_pair_corner_rejected_count =
        normal_trace.normal_pair_corner_rejected_count;
    normal_trace_capture.loop_erased_point_count =
        normal_trace.loop_erased_point_count;
    normal_trace_capture.normal_pair_counts_by_direction =
        normal_trace.normal_pair_counts_by_direction;
    normal_trace_capture.anchor_normal_band_px =
        normal_trace.anchor_normal_band_px;
    normal_trace_capture.anchor_near_candidate_counts =
        normal_trace.anchor_near_candidate_counts;
    normal_trace_capture.anchor_band_rejected_counts =
        normal_trace.anchor_band_rejected_counts;
    normal_trace_capture.anchor_slope_rejected_counts =
        normal_trace.anchor_slope_rejected_counts;
    normal_trace_capture.anchor_normal_rejected_counts =
        normal_trace.anchor_normal_rejected_counts;
    normal_trace_capture.anchor_prefilter_accepted_counts =
        normal_trace.anchor_prefilter_accepted_counts;
    normal_trace_capture.domain_deduplicated_counts =
        normal_trace.domain_deduplicated_counts;
    normal_trace_capture.compressed_keypoint_counts =
        normal_trace.compressed_keypoint_counts;
    normal_trace_capture.ann_edge_counts = normal_trace.ann_edge_counts;
    normal_trace_capture.ann_component_counts = normal_trace.ann_component_counts;
    normal_trace_capture.ann_selected_point_counts =
        normal_trace.ann_selected_point_counts;
    normal_trace_capture.ann_selected_coverage =
        normal_trace.ann_selected_coverage;
    normal_trace_capture.domain_join_allowed_gap_px =
        normal_trace.domain_join_allowed_gap_px;
    normal_trace_capture.domain_join_selected_gap_px =
        normal_trace.domain_join_selected_gap_px;
    normal_trace_capture.endpoint_spike_pruned_counts =
        normal_trace.endpoint_spike_pruned_counts;
    normal_trace_capture.endpoint_spike_ratio_percent =
        normal_trace.endpoint_spike_ratio_percent;
    normal_trace_capture.junction_tangent_window_points =
        normal_trace.junction_tangent_window_points;
    normal_trace_capture.junction_min_cross_angle_deg =
        normal_trace.junction_min_cross_angle_deg;
    normal_trace_capture.junction_max_extrapolation_percent =
        normal_trace.junction_max_extrapolation_percent;
    normal_trace_capture.junction_join_spacing_multiplier_percent =
        normal_trace.junction_join_spacing_multiplier_percent;
    normal_trace_capture.derived_junction_modes =
        normal_trace.derived_junction_modes;
    normal_trace_capture.derived_junction_cross_angle_deg =
        normal_trace.derived_junction_cross_angle_deg;
    normal_trace_capture.derived_junction_current_extrapolation_px =
        normal_trace.derived_junction_current_extrapolation_px;
    normal_trace_capture.derived_junction_next_extrapolation_px =
        normal_trace.derived_junction_next_extrapolation_px;
    const auto copy_direction_points = [](
        const std::array<std::vector<CxShapePoint>, 4>& source,
        std::array<std::vector<CxFastMatchNormalTracePointEvidence>, 4>& target) {
        for (std::size_t direction = 0; direction < source.size(); ++direction) {
            target[direction].clear();
            target[direction].reserve(source[direction].size());
            for (const CxShapePoint& point : source[direction])
                target[direction].push_back({point.x, point.y});
        }
    };
    copy_direction_points(normal_trace.anchor_points_by_direction,
                          normal_trace_capture.anchor_points_by_direction);
    copy_direction_points(normal_trace.domain_points_by_direction,
                          normal_trace_capture.domain_points_by_direction);
    copy_direction_points(normal_trace.compressed_points_by_direction,
                          normal_trace_capture.compressed_points_by_direction);
    copy_direction_points(normal_trace.ann_selected_points_by_direction,
                          normal_trace_capture.ann_selected_points_by_direction);
    const auto copy_normal_trace_points = [](const auto& source, auto& target) {
        target.clear();
        target.reserve(source.size());
        for (const auto& point : source)
            target.push_back({point.x, point.y});
    };
    copy_normal_trace_points(
        normal_trace.dijkstra_trace_points,
        normal_trace_capture.dijkstra_trace_points);
    copy_normal_trace_points(
        normal_trace.derived_junction_points,
        normal_trace_capture.derived_junction_points);
    normal_trace_capture.dijkstra_source_directions =
        normal_trace.dijkstra_source_directions;
    normal_trace_capture.dijkstra_source_scans =
        normal_trace.dijkstra_source_scans;
    copy_normal_trace_points(
        normal_trace.derived_trace_points,
        normal_trace_capture.derived_trace_points);
    copy_normal_trace_points(
        normal_trace.normal_pair_a,
        normal_trace_capture.normal_pair_a);
    copy_normal_trace_points(
        normal_trace.normal_pair_b,
        normal_trace_capture.normal_pair_b);
    copy_normal_trace_points(
        normal_trace.normal_pair_source_points,
        normal_trace_capture.normal_pair_source_points);
    normal_trace_capture.normal_pair_source_directions =
        normal_trace.normal_pair_source_directions;
    normal_trace_capture.normal_pair_source_scans =
        normal_trace.normal_pair_source_scans;
    if (normal_trace.executed)
        output.algorithm_executed = true;

    output.fastmatch_reference_shape_model = tool.getreferenceshapemodel();
    output.fastmatch_observed_shape_model = tool.getobservedshapemodel();
    output.fastmatch_form_fit = tool.getformfitresult();
    output.fastmatch_form_fit_gauge = tool.getformfitgauge();
    if (output.fastmatch_form_fit.executed)
        output.algorithm_executed = true;

    const FastMatchTransformSearchResult& transform_search =
        tool.gettransformsearchresult();
    CxFastMatchTransformSearchEvidence& transform_evidence =
        output.fastmatch_transform_search;
    transform_evidence.executed = transform_search.executed;
    transform_evidence.converged = transform_search.converged;
    transform_evidence.budget_exceeded = transform_search.budget_exceeded;
    transform_evidence.fallback_to_rigid = transform_search.fallback_to_rigid;
    transform_evidence.calibration_applied = transform_search.calibration_applied;
    transform_evidence.calibration_snapshot_hash = transform_search.calibration_snapshot_hash;
    transform_evidence.calibration_source_ref = transform_search.calibration_source_ref;
    transform_evidence.calibration_coordinate_frame_id =
        transform_search.calibration_coordinate_frame_id;
    transform_evidence.calibration_xy_unit = transform_search.calibration_xy_unit;
    transform_evidence.calibration_reprojection_rmse_px =
        transform_search.calibration_reprojection_rmse_px;
    transform_evidence.best_physical_cx = transform_search.best_physical_cx;
    transform_evidence.best_physical_cy = transform_search.best_physical_cy;
    transform_evidence.seed_available = transform_search.seed.available;
    transform_evidence.seed_source = transform_search.seed.source;
    transform_evidence.seed_model_id = transform_search.seed.model_id;
    transform_evidence.seed_detection_index = transform_search.seed.detection_index;
    transform_evidence.seed_class_id = transform_search.seed.class_id;
    transform_evidence.seed_confidence = transform_search.seed.confidence;
    transform_evidence.seed_angle_convention = transform_search.seed.angle_convention;
    transform_evidence.seed_reason = transform_search.seed.reason;
    transform_evidence.initial_cx = transform_search.initial.cx;
    transform_evidence.initial_cy = transform_search.initial.cy;
    transform_evidence.initial_angle_deg = transform_search.initial.angle_deg;
    transform_evidence.initial_scale_x = transform_search.initial.scale_x;
    transform_evidence.initial_scale_y = transform_search.initial.scale_y;
    transform_evidence.best_cx = transform_search.best.cx;
    transform_evidence.best_cy = transform_search.best.cy;
    transform_evidence.best_angle_deg = transform_search.best.angle_deg;
    transform_evidence.best_scale_x = transform_search.best.scale_x;
    transform_evidence.best_scale_y = transform_search.best.scale_y;
    transform_evidence.best_shear = transform_search.best.shear;
    transform_evidence.best_projective_u = transform_search.best.projective_u;
    transform_evidence.best_projective_v = transform_search.best.projective_v;
    transform_evidence.best_score = transform_search.best_score;
    transform_evidence.appearance_score = transform_search.appearance_score;
    transform_evidence.continuity_score = transform_search.continuity_score;
    transform_evidence.gradient_score = transform_search.gradient_score;
    transform_evidence.geometric_residual_px = transform_search.geometric_residual_px;
    transform_evidence.rigid_baseline_score = transform_search.rigid_baseline_score;
    transform_evidence.evaluated_candidates = transform_search.evaluated_candidates;
    transform_evidence.accepted_candidates = transform_search.accepted_candidates;
    transform_evidence.sample_count = transform_search.sample_count;
    transform_evidence.elapsed_ms = transform_search.elapsed_ms;
    transform_evidence.failure_stage = transform_search.failure_stage;
    if (transform_evidence.executed)
    {
        output.algorithm_executed = true;
        output.budget_exceeded = output.budget_exceeded || transform_evidence.budget_exceeded;
        output.elapsed_ms = std::max(output.elapsed_ms, transform_evidence.elapsed_ms);
    }

    const FastMatchTemplateGeometrySnapshot &template_geometry =
        tool.gettemplategeometry();
    output.fastmatch_template_geometry.available = template_geometry.available;
    output.fastmatch_template_geometry.source_object_index =
        template_geometry.source_object_index;
    output.fastmatch_template_geometry.source_object_ref =
        template_geometry.source_object_ref;
    output.fastmatch_template_geometry.bbox_x = template_geometry.bbox_px.x;
    output.fastmatch_template_geometry.bbox_y = template_geometry.bbox_px.y;
    output.fastmatch_template_geometry.bbox_w = template_geometry.bbox_px.width;
    output.fastmatch_template_geometry.bbox_h = template_geometry.bbox_px.height;
    output.fastmatch_template_geometry.centroid_x =
        template_geometry.centroid_px.x;
    output.fastmatch_template_geometry.centroid_y =
        template_geometry.centroid_px.y;
    output.fastmatch_template_geometry.projected_area =
        template_geometry.projected_area;
    output.fastmatch_template_geometry.major_axis_length =
        template_geometry.major_axis_length;
    output.fastmatch_template_geometry.minor_axis_length =
        template_geometry.minor_axis_length;
    output.fastmatch_template_geometry.orientation_deg =
        template_geometry.orientation_deg;
    output.fastmatch_template_geometry.aspect_ratio =
        template_geometry.aspect_ratio;
    output.fastmatch_template_geometry.solidity = template_geometry.solidity;
    output.fastmatch_template_geometry.boundary_point_count =
        static_cast<int>(template_geometry.normalized_boundary.size());
    output.fastmatch_template_geometry.status = template_geometry.status;

    output.fastmatch_pose_candidates.clear();
    for (const FastMatchPoseCandidateSnapshot &source :
         tool.getposecandidates()) {
        CxFastMatchPoseCandidateEvidence pose;
        pose.candidate_index = source.candidate_index;
        pose.observed_geometry_index = source.observed_geometry_index;
        pose.observed_geometry_ref = source.observed_geometry_ref;
        pose.bbox_x = source.bbox_px.x;
        pose.bbox_y = source.bbox_px.y;
        pose.bbox_w = source.bbox_px.width;
        pose.bbox_h = source.bbox_px.height;
        pose.center_x = source.center_px.x;
        pose.center_y = source.center_px.y;
        pose.angle_deg = source.angle_deg;
        pose.scale_x = source.scale_x;
        pose.scale_y = source.scale_y;
        pose.appearance_score = source.appearance_score;
        pose.geometry_score = source.geometry_score;
        pose.combined_score = source.combined_score;
        pose.boundary_point_count =
            static_cast<int>(source.transformed_boundary.size());
        pose.status = source.status;
        output.fastmatch_pose_candidates.push_back(std::move(pose));
    }

    const int learn_point_count =
        output.fastmatch_learn_a_count +
        output.fastmatch_learn_b_count +
        output.fastmatch_learn_a2_count +
        output.fastmatch_learn_b2_count;

    if (transform_evidence.executed && !transform_evidence.converged)
        output.failure_stage = transform_evidence.failure_stage;
    else if (normal_trace.executed && !normal_trace.succeeded)
        output.failure_stage = "normal_trace";
    else if (!normal_trace.succeeded && learn_point_count <= 0)
        output.failure_stage = "learn_points";
    else if (output.model_point_count <= 0)
        output.failure_stage = "model_points";
    else if (output.fastmatch_match_call_count > 0 && output.candidate_count <= 0)
        output.failure_stage = "match_candidates";

    if (!output.failure_stage.empty())
        output.reason =
            "FastMatch result unavailable: failure_stage=" + output.failure_stage +
            ", learn_points=" +
            std::to_string(learn_point_count) +
            ", learn_status_code=" +
            std::to_string(output.fastmatch_learn_status_code) +
            ", model_points=" +
            std::to_string(output.model_point_count) +
            ", candidates=" +
            std::to_string(output.candidate_count);

    ImageAnnotationLayer layer;
    tool.PublishDisplayShapes(layer, output.owner_ref);
    CopyShapeElementsToSnapshots(layer, output.shapes);

    return true;
}

bool CaptureCxTextInspectResult(
    CxTextInspect &tool,
    const std::string &object_name,
    CxScriptToolResultCapture &output) {
    CaptureFastMatchResult(static_cast<FastMatch &>(tool), object_name, output);
    output.type = "CxTextInspect";
    output.ocr_final_text = tool.getrecognizedtext();
    output.ocr_failure_reason = tool.getdecodefailure();
    output.ocr_glyph_candidates.clear();
    for (const CxTextInspectGlyphCandidateSnapshot &source :
         tool.getglyphcandidates()) {
        CxOcrGlyphCandidateEvidence glyph;
        glyph.source_object_index = source.source_object_index;
        glyph.line_index = source.line_index;
        glyph.reading_order = source.reading_order;
        glyph.bbox_x = source.bbox_px.x;
        glyph.bbox_y = source.bbox_px.y;
        glyph.bbox_w = source.bbox_px.width;
        glyph.bbox_h = source.bbox_px.height;
        glyph.centroid_x = source.centroid_px.x;
        glyph.centroid_y = source.centroid_px.y;
        glyph.orientation_deg = source.orientation_deg;
        glyph.projected_area = source.projected_area;
        glyph.aspect_ratio = source.aspect_ratio;
        glyph.solidity = source.solidity;
        glyph.label = source.label;
        glyph.appearance_score = source.appearance_score;
        glyph.geometry_score = source.geometry_score;
        glyph.confidence = source.confidence;
        glyph.source_object_ref = source.source_object_ref;
        glyph.status = source.status;
        output.ocr_glyph_candidates.push_back(std::move(glyph));
    }

    output.valid_points_count =
        static_cast<int>(output.ocr_glyph_candidates.size());
    output.algorithm_executed =
        !output.ocr_glyph_candidates.empty() || !output.ocr_final_text.empty();
    output.measure_completed = !output.ocr_glyph_candidates.empty();
    if (output.algorithm_executed) {
        output.failure_stage.clear();
        output.reason.clear();
    } else {
        output.failure_stage = "ocr_decode";
        output.reason = output.ocr_failure_reason.empty()
                            ? "CxTextInspect produced no glyph candidates or text"
                            : output.ocr_failure_reason;
    }
    return true;
}

bool CaptureFindSegmentationResult(
    FindSegmentation& tool,
    const std::string& object_name,
    CxScriptToolResultCapture& output)
{
    output.type = "FindSegmentation";
    output.name = object_name;
    output.owner_ref = object_name;

    output.segmentation_status_code = tool.status_code();
    output.segmentation_contour_count = tool.get_contour_count();
    output.segmentation_primary_area = tool.get_primary_area();
    output.segmentation_result_ref = tool.get_result();
    output.segmentation_mask_ref = tool.get_mask_ref();
    output.segmentation_contour_ref = tool.get_contour_ref();
    output.segmentation_overlay_ref = tool.get_overlay_ref();

    const FindSegmentationResult& segmentation_result = tool.result();
    output.segmentation_task_id = segmentation_result.task_id;
    output.segmentation_model_id = segmentation_result.model_id;
    output.segmentation_model_package_ref = segmentation_result.model_package_ref;
    output.segmentation_manifest_path = segmentation_result.manifest_path;
    output.segmentation_postprocess_profile = segmentation_result.postprocess_profile;
    output.segmentation_parameter_profile_ref = segmentation_result.parameter_profile_ref;
    output.segmentation_region_count = segmentation_result.region_count;
    output.segmentation_raw_result_available = segmentation_result.raw_result_available;
    output.segmentation_refined_result_available = segmentation_result.refined_result_available;
    output.segmentation_fallback_used = segmentation_result.fallback_used;
    output.segmentation_result_stage = segmentation_result.result_stage;
    output.segmentation_refinement_method = segmentation_result.refinement_method;
    output.segmentation_requested_geometry_type = segmentation_result.requested_geometry_type;
    output.segmentation_geometry_fit_status = segmentation_result.geometry_fit_status;
    output.segmentation_geometry_fit_reason = segmentation_result.geometry_fit_reason;
    output.segmentation_oriented_box_evidence_ref =
        segmentation_result.oriented_box_evidence_ref;
    output.segmentation_oriented_box_conclusion =
        segmentation_result.oriented_box_conclusion;
    output.segmentation_geometry_count =
        static_cast<int>(segmentation_result.primitive_hypotheses.size());
    if (!segmentation_result.primitive_hypotheses.empty()) {
        const CxGeometryPrimitiveHypothesis& geometry =
            segmentation_result.primitive_hypotheses.front();
        output.segmentation_geometry_residual_px = geometry.classical_fit_residual_px;
        output.segmentation_geometry_support = geometry.support;
        output.segmentation_geometry_center_x = geometry.center.x;
        output.segmentation_geometry_center_y = geometry.center.y;
        output.segmentation_geometry_radius = geometry.radius;
        output.segmentation_geometry_axis_x = geometry.axes.width;
        output.segmentation_geometry_axis_y = geometry.axes.height;
        output.segmentation_geometry_angle_deg = geometry.angle_deg;
    }
    output.segmentation_raw_result_ref = segmentation_result.raw_result_ref;
    output.segmentation_raw_mask_ref = segmentation_result.raw_mask_ref;
    output.segmentation_raw_contour_ref = segmentation_result.raw_contour_ref;
    output.segmentation_raw_overlay_ref = segmentation_result.raw_overlay_ref;
    output.segmentation_refined_result_ref = segmentation_result.refined_result_ref;
    output.segmentation_refined_mask_ref = segmentation_result.refined_mask_ref;
    output.segmentation_refined_contour_ref = segmentation_result.refined_contour_ref;
    output.segmentation_refined_overlay_ref = segmentation_result.refined_overlay_ref;


    output.valid_points_count = output.segmentation_contour_count;
    output.algorithm_executed = output.segmentation_status_code != 0 ||
                                !output.segmentation_result_stage.empty();
    output.measure_completed = output.segmentation_contour_count > 0;
    output.has_result_rect = output.segmentation_contour_count > 0;
    output.result_rect_count = output.segmentation_contour_count;
    output.avgdist = output.segmentation_primary_area;
    if (output.segmentation_contour_count > 0) {
        output.failure_stage.clear();
    } else {
        const std::string& backend_status = tool.result().backend_status;
        output.failure_stage = backend_status.empty() ? "boundary_contours" : backend_status;
    }
    if (!output.failure_stage.empty())
        output.reason = tool.m_reason.empty() ? "FindSegmentation boundary unavailable" : tool.m_reason;

    ImageAnnotationLayer layer;
    tool.PublishDisplayShapes(layer, output.owner_ref);
    CopyShapeElementsToSnapshots(layer, output.shapes);

    return true;
}

static void MergeToolCapture(
    const CxScriptToolResultCapture& tool,
    CxScriptExecutionCapture& capture)
{
    capture.valid_points_count += tool.valid_points_count;
    capture.tool_method = tool.tool_method;
    capture.tool_threshold = tool.tool_threshold;
    capture.tool_wgap = tool.tool_wgap;
    capture.tool_hgap = tool.tool_hgap;
    capture.tool_linegap = tool.tool_linegap;
    capture.selected_min_edge_run_width_px =
        tool.tool_min_edge_run_width_px;
    capture.tool_input_line_x0 = tool.tool_input_line_x0;
    capture.tool_input_line_y0 = tool.tool_input_line_y0;
    capture.tool_input_line_x1 = tool.tool_input_line_x1;
    capture.tool_input_line_y1 = tool.tool_input_line_y1;
    capture.tool_input_line_half_width = tool.tool_input_line_half_width;
    capture.tool_input_circle_cx = tool.tool_input_circle_cx;
    capture.tool_input_circle_cy = tool.tool_input_circle_cy;
    capture.tool_input_circle_px = tool.tool_input_circle_px;
    capture.tool_input_circle_py = tool.tool_input_circle_py;
    capture.tool_input_circle_gap = tool.tool_input_circle_gap;
    capture.scan_rows_examined = tool.scan_rows_examined;
    capture.scan_rows_with_foreground = tool.scan_rows_with_foreground;
    capture.scan_runs_total = tool.scan_runs_total;
    capture.scan_runs_within_length_limit = tool.scan_runs_within_length_limit;
    capture.scan_runs_over_length_limit = tool.scan_runs_over_length_limit;
    capture.scan_runs_rejected_by_min_edge_width =
        tool.scan_runs_rejected_by_min_edge_width;
    capture.scan_runs_rejected_by_selection = tool.scan_runs_rejected_by_selection;
    capture.scan_runs_rejected_near_endpoint = tool.scan_runs_rejected_near_endpoint;
    capture.scan_points_emitted = tool.scan_points_emitted;
    capture.boundary_trigger_enabled = tool.boundary_trigger_enabled;
    capture.boundary_trigger_mode = tool.boundary_trigger_mode;
    capture.boundary_trigger_polarity = tool.boundary_trigger_polarity;
    capture.boundary_trigger_selection = tool.boundary_trigger_selection;
    capture.boundary_trigger_scan_rows_evaluated =
        tool.boundary_trigger_scan_rows_evaluated;
    capture.boundary_trigger_candidate_count =
        tool.boundary_trigger_candidate_count;
    capture.boundary_trigger_points_emitted =
        tool.boundary_trigger_points_emitted;
    capture.boundary_trigger_rejected_no_candidate =
        tool.boundary_trigger_rejected_no_candidate;
    capture.boundary_trigger_rejected_endpoint =
        tool.boundary_trigger_rejected_endpoint;
    capture.findline_point_consistency_enabled = tool.findline_point_consistency_enabled;
    capture.findline_point_consistency_range = tool.findline_point_consistency_range;
    capture.findline_point_consistency_input_points = tool.findline_point_consistency_input_points;
    capture.findline_point_consistency_output_points = tool.findline_point_consistency_output_points;
    capture.findline_point_consistency_removed_points = tool.findline_point_consistency_removed_points;
    capture.findline_selected_edge_index = tool.findline_selected_edge_index;
    capture.findline_evaluated_edge_count = tool.findline_evaluated_edge_count;
    capture.findline_best_edge_index = tool.findline_best_edge_index;
    capture.findline_best_edge_score = tool.findline_best_edge_score;

    if (!tool.boundary_analysis_status.empty())
    {
        capture.boundary_analysis_status =
            tool.boundary_analysis_status;
        capture.boundary_reliability_level =
            tool.boundary_reliability_level;
        capture.boundary_expected_scan_count =
            tool.boundary_expected_scan_count;
        capture.boundary_accepted_point_count =
            tool.boundary_accepted_point_count;
        capture.boundary_interpolation_valid_count =
            tool.boundary_interpolation_valid_count;
        capture.boundary_fit_residual_count =
            tool.boundary_fit_residual_count;
        capture.boundary_coverage_ratio =
            tool.boundary_coverage_ratio;
        capture.boundary_response_mean =
            tool.boundary_response_mean;
        capture.boundary_response_median =
            tool.boundary_response_median;
        capture.boundary_response_cv =
            tool.boundary_response_cv;
        capture.boundary_subpixel_offset_mean =
            tool.boundary_subpixel_offset_mean;
        capture.boundary_subpixel_offset_stddev =
            tool.boundary_subpixel_offset_stddev;
        capture.boundary_localization_sigma_mean_px =
            tool.boundary_localization_sigma_mean_px;
        capture.boundary_residual_rmse_px =
            tool.boundary_residual_rmse_px;
        capture.boundary_residual_p95_px =
            tool.boundary_residual_p95_px;
        capture.boundary_residual_max_px =
            tool.boundary_residual_max_px;
        capture.boundary_outlier_ratio =
            tool.boundary_outlier_ratio;
        capture.boundary_reliability_score =
            tool.boundary_reliability_score;
        capture.boundary_points = tool.boundary_points;
    }

    capture.findline_edge_evaluations.insert(
        capture.findline_edge_evaluations.end(),
        tool.findline_edge_evaluations.begin(),
        tool.findline_edge_evaluations.end());
    capture.circle_point_consistency_enabled =
        tool.circle_point_consistency_enabled;
    capture.circle_point_consistency_range =
        tool.circle_point_consistency_range;
    capture.circle_point_consistency_input_points =
        tool.circle_point_consistency_input_points;
    capture.circle_point_consistency_output_points =
        tool.circle_point_consistency_output_points;
    capture.circle_point_consistency_removed_points =
        tool.circle_point_consistency_removed_points;
    capture.findline_scan_diagnostics.insert(
        capture.findline_scan_diagnostics.end(),
        tool.findline_scan_diagnostics.begin(),
        tool.findline_scan_diagnostics.end());
    capture.has_fit_line = capture.has_fit_line || tool.has_fit_line;
    capture.has_fit_circle = capture.has_fit_circle || tool.has_fit_circle;
    capture.has_fit_ellipse = capture.has_fit_ellipse || tool.has_fit_ellipse;
    capture.has_result_rect = capture.has_result_rect || tool.has_result_rect;
    capture.budget_exceeded = capture.budget_exceeded || tool.budget_exceeded;
    capture.avgdist = tool.avgdist;
    if (tool.has_fit_ellipse)
    {
        capture.ellipse_cx = tool.ellipse_cx;
        capture.ellipse_cy = tool.ellipse_cy;
        capture.ellipse_radius_x = tool.ellipse_radius_x;
        capture.ellipse_radius_y = tool.ellipse_radius_y;
        capture.ellipse_angle_deg = tool.ellipse_angle_deg;
    }
    capture.ellipse_selected_edge_index = tool.ellipse_selected_edge_index;
    capture.ellipse_scan_candidate_lines = tool.ellipse_scan_candidate_lines;
    capture.ellipse_scan_total_candidates = tool.ellipse_scan_total_candidates;
    capture.ellipse_scan_accepted_points_before_gate = tool.ellipse_scan_accepted_points_before_gate;
    capture.ellipse_accepted_min_boundary_ratio = tool.ellipse_accepted_min_boundary_ratio;
    capture.ellipse_accepted_max_boundary_ratio = tool.ellipse_accepted_max_boundary_ratio;
    capture.ellipse_accepted_avg_boundary_ratio = tool.ellipse_accepted_avg_boundary_ratio;
    capture.ellipse_candidate_policy = tool.ellipse_candidate_policy;
    capture.ellipse_scan_lines_cross_outside_ellipse_count = tool.ellipse_scan_lines_cross_outside_ellipse_count;
    capture.ellipse_scan_endpoint_norm_min = tool.ellipse_scan_endpoint_norm_min;
    capture.ellipse_scan_endpoint_norm_avg = tool.ellipse_scan_endpoint_norm_avg;
    capture.ellipse_scan_endpoint_norm_max = tool.ellipse_scan_endpoint_norm_max;
    capture.ellipse_accepted_points_outside_ellipse_count = tool.ellipse_accepted_points_outside_ellipse_count;
    capture.ellipse_accepted_point_norm_min = tool.ellipse_accepted_point_norm_min;
    capture.ellipse_accepted_point_norm_avg = tool.ellipse_accepted_point_norm_avg;
    capture.ellipse_accepted_point_norm_max = tool.ellipse_accepted_point_norm_max;
    capture.ellipse_rejected_boundary_band_candidate_count =
        tool.ellipse_rejected_boundary_band_candidate_count;
    capture.ellipse_rejected_boundary_band_norm_min =
        tool.ellipse_rejected_boundary_band_norm_min;
    capture.ellipse_rejected_boundary_band_norm_avg =
        tool.ellipse_rejected_boundary_band_norm_avg;
    capture.ellipse_rejected_boundary_band_norm_max =
        tool.ellipse_rejected_boundary_band_norm_max;
    capture.ellipse_scan_geometry_policy = tool.ellipse_scan_geometry_policy;
    capture.ellipse_point_consistency_enabled =
        tool.ellipse_point_consistency_enabled;
    capture.ellipse_point_consistency_range =
        tool.ellipse_point_consistency_range;
    capture.ellipse_point_consistency_input_points =
        tool.ellipse_point_consistency_input_points;
    capture.ellipse_point_consistency_output_points =
        tool.ellipse_point_consistency_output_points;
    capture.ellipse_point_consistency_removed_points =
        tool.ellipse_point_consistency_removed_points;

    capture.result_rect_count += tool.result_rect_count;
    if (tool.has_result_rect && capture.top1_rect_w == 0 && capture.top1_rect_h == 0)
    {
        capture.top1_rect_x = tool.top1_rect_x;
        capture.top1_rect_y = tool.top1_rect_y;
        capture.top1_rect_w = tool.top1_rect_w;
        capture.top1_rect_h = tool.top1_rect_h;
    }
    capture.fastmatch_learn_a_count += tool.fastmatch_learn_a_count;
    capture.fastmatch_learn_b_count += tool.fastmatch_learn_b_count;
    capture.fastmatch_learn_a2_count += tool.fastmatch_learn_a2_count;
    capture.fastmatch_learn_b2_count += tool.fastmatch_learn_b2_count;
    if (tool.fastmatch_learn_status_code != 0)
        capture.fastmatch_learn_status_code = tool.fastmatch_learn_status_code;
    capture.model_point_count += tool.model_point_count;
    if (tool.fastmatch_model_width > 0)
        capture.fastmatch_model_width = tool.fastmatch_model_width;
    if (tool.fastmatch_model_height > 0)
        capture.fastmatch_model_height = tool.fastmatch_model_height;
    capture.fastmatch_pattern_a_count += tool.fastmatch_pattern_a_count;
    capture.fastmatch_pattern_b_count += tool.fastmatch_pattern_b_count;
    capture.fastmatch_pattern_a_x = tool.fastmatch_pattern_a_x;
    capture.fastmatch_pattern_a_y = tool.fastmatch_pattern_a_y;
    capture.fastmatch_pattern_a_width = tool.fastmatch_pattern_a_width;
    capture.fastmatch_pattern_a_height = tool.fastmatch_pattern_a_height;
    capture.fastmatch_pattern_b_x = tool.fastmatch_pattern_b_x;
    capture.fastmatch_pattern_b_y = tool.fastmatch_pattern_b_y;
    capture.fastmatch_pattern_b_width = tool.fastmatch_pattern_b_width;
    capture.fastmatch_pattern_b_height = tool.fastmatch_pattern_b_height;
    capture.candidate_count += tool.candidate_count;
    if (tool.best_score > capture.best_score)
        capture.best_score = tool.best_score;
    capture.has_result_box = capture.has_result_box || tool.has_result_box;
    capture.has_best_result = capture.has_best_result || tool.has_best_result;
    capture.fastmatch_match_call_count += tool.fastmatch_match_call_count;
    capture.fastmatch_match_ab_call_count += tool.fastmatch_match_ab_call_count;
    capture.fastmatch_match_sample_ab_call_count += tool.fastmatch_match_sample_ab_call_count;
    if (tool.fastmatch_match_last_stage > 0)
        capture.fastmatch_match_last_stage = tool.fastmatch_match_last_stage;
    capture.fastmatch_match_image_width = tool.fastmatch_match_image_width;
    capture.fastmatch_match_image_height = tool.fastmatch_match_image_height;
    capture.fastmatch_learn_rect_x0 = tool.fastmatch_learn_rect_x0;
    capture.fastmatch_learn_rect_y0 = tool.fastmatch_learn_rect_y0;
    capture.fastmatch_learn_rect_x1 = tool.fastmatch_learn_rect_x1;
    capture.fastmatch_learn_rect_y1 = tool.fastmatch_learn_rect_y1;
    capture.fastmatch_match_rect_x0 = tool.fastmatch_match_rect_x0;
    capture.fastmatch_match_rect_y0 = tool.fastmatch_match_rect_y0;
    capture.fastmatch_match_rect_x1 = tool.fastmatch_match_rect_x1;
    capture.fastmatch_match_rect_y1 = tool.fastmatch_match_rect_y1;
    capture.fastmatch_raw_probe_count += tool.fastmatch_raw_probe_count;
    capture.fastmatch_raw_threshold_hit_count += tool.fastmatch_raw_threshold_hit_count;
    capture.fastmatch_result_to_list_count += tool.fastmatch_result_to_list_count;
    capture.fastmatch_candidate_insert_count += tool.fastmatch_candidate_insert_count;
    capture.fastmatch_candidate_replace_count += tool.fastmatch_candidate_replace_count;
    capture.fastmatch_candidate_reject_count += tool.fastmatch_candidate_reject_count;
    capture.fastmatch_rotate_budget_exceeded =
        capture.fastmatch_rotate_budget_exceeded || tool.fastmatch_rotate_budget_exceeded;
    capture.fastmatch_rotate_candidate_count += tool.fastmatch_rotate_candidate_count;
    capture.fastmatch_rotate_probe_count += tool.fastmatch_rotate_probe_count;
    capture.fastmatch_rotate_sample_count += tool.fastmatch_rotate_sample_count;
    capture.fastmatch_rotate_elapsed_ms = std::max(
        capture.fastmatch_rotate_elapsed_ms, tool.fastmatch_rotate_elapsed_ms);

    if (tool.fastmatch_normal_trace.executed)
        capture.fastmatch_normal_trace = tool.fastmatch_normal_trace;
    if (tool.fastmatch_reference_shape_model.available)
        capture.fastmatch_reference_shape_model =
            tool.fastmatch_reference_shape_model;
    if (tool.fastmatch_observed_shape_model.available)
        capture.fastmatch_observed_shape_model =
            tool.fastmatch_observed_shape_model;
    if (tool.fastmatch_form_fit.executed)
        capture.fastmatch_form_fit = tool.fastmatch_form_fit;
    if (!tool.fastmatch_form_fit_gauge.elements.empty())
        capture.fastmatch_form_fit_gauge = tool.fastmatch_form_fit_gauge;

    if (tool.fastmatch_template_geometry.available)
        capture.fastmatch_template_geometry =
            tool.fastmatch_template_geometry;
    if (!tool.fastmatch_pose_candidates.empty())
        capture.fastmatch_pose_candidates =
            tool.fastmatch_pose_candidates;
    if (tool.fastmatch_transform_search.executed)
        capture.fastmatch_transform_search = tool.fastmatch_transform_search;
    if (!tool.ocr_final_text.empty())
        capture.ocr_final_text = tool.ocr_final_text;
    if (!tool.ocr_failure_reason.empty())
        capture.ocr_failure_reason = tool.ocr_failure_reason;
    if (!tool.ocr_glyph_candidates.empty())
        capture.ocr_glyph_candidates = tool.ocr_glyph_candidates;
    capture.object_prefilter_requested = tool.object_prefilter_requested;
    capture.object_prefilter_applied = tool.object_prefilter_applied;
    capture.actual_findsetting = tool.actual_findsetting;
    capture.object_filter_strategy_id = tool.object_filter_strategy_id;
    capture.object_filter_borw = tool.object_filter_borw;
    capture.object_filter_min = tool.object_filter_min;
    capture.object_filter_max = tool.object_filter_max;
    capture.object_component_count = tool.object_component_count;
    capture.object_component_accepted_count = tool.object_component_accepted_count;
    capture.object_component_rejected_count = tool.object_component_rejected_count;
    capture.object_component_max_area = tool.object_component_max_area;
    capture.object_component_max_width = tool.object_component_max_width;
    capture.object_component_max_height = tool.object_component_max_height;
    capture.object_foreground_before = tool.object_foreground_before;
    capture.object_foreground_after = tool.object_foreground_after;
    capture.object_white_component_count = tool.object_white_component_count;
    capture.object_white_accepted_count = tool.object_white_accepted_count;
    capture.object_white_rejected_count = tool.object_white_rejected_count;
    capture.object_black_component_count = tool.object_black_component_count;
    capture.object_black_accepted_count = tool.object_black_accepted_count;
    capture.object_black_rejected_count = tool.object_black_rejected_count;
    capture.object_algorithm_branch = tool.object_algorithm_branch;

    if (!tool.findobject_measurements.empty())
        capture.findobject_measurements = tool.findobject_measurements;
    capture.fit_filter_input_count = tool.fit_filter_input_count;
    capture.fit_filter_kept_count = tool.fit_filter_kept_count;
    capture.fit_filter_rejected_count = tool.fit_filter_rejected_count;
    capture.fit_filter_sigma = tool.fit_filter_sigma;
    capture.fit_filter_threshold = tool.fit_filter_threshold;
    capture.findrect_seed_valid = capture.findrect_seed_valid || tool.findrect_seed_valid;
    capture.findrect_top_valid = capture.findrect_top_valid || tool.findrect_top_valid;
    capture.findrect_bottom_valid = capture.findrect_bottom_valid || tool.findrect_bottom_valid;
    capture.findrect_left_valid = capture.findrect_left_valid || tool.findrect_left_valid;
    capture.findrect_right_valid = capture.findrect_right_valid || tool.findrect_right_valid;
    capture.findrect_top_points += tool.findrect_top_points;
    capture.findrect_bottom_points += tool.findrect_bottom_points;
    capture.findrect_left_points += tool.findrect_left_points;
    capture.findrect_right_points += tool.findrect_right_points;
    if (tool.findrect_coarse_score != 0.0)
        capture.findrect_coarse_score = tool.findrect_coarse_score;
    if (tool.findrect_refine_score != 0.0)
        capture.findrect_refine_score = tool.findrect_refine_score;
    if (tool.segmentation_status_code != 0)
        capture.segmentation_status_code = tool.segmentation_status_code;
    capture.segmentation_contour_count += tool.segmentation_contour_count;
    if (tool.segmentation_primary_area != 0.0)
        capture.segmentation_primary_area = tool.segmentation_primary_area;
    if (!tool.segmentation_result_ref.empty())
        capture.segmentation_result_ref = tool.segmentation_result_ref;
    if (!tool.segmentation_mask_ref.empty())
        capture.segmentation_mask_ref = tool.segmentation_mask_ref;
    if (!tool.segmentation_contour_ref.empty())
        capture.segmentation_contour_ref = tool.segmentation_contour_ref;
    if (!tool.segmentation_overlay_ref.empty())
        capture.segmentation_overlay_ref = tool.segmentation_overlay_ref;

    if (!tool.segmentation_task_id.empty())
        capture.segmentation_task_id = tool.segmentation_task_id;

    if (!tool.segmentation_model_id.empty())
        capture.segmentation_model_id = tool.segmentation_model_id;

    if (!tool.segmentation_model_package_ref.empty())
        capture.segmentation_model_package_ref = tool.segmentation_model_package_ref;

    if (!tool.segmentation_manifest_path.empty())
        capture.segmentation_manifest_path = tool.segmentation_manifest_path;

    if (!tool.segmentation_postprocess_profile.empty())
        capture.segmentation_postprocess_profile = tool.segmentation_postprocess_profile;

    if (!tool.segmentation_parameter_profile_ref.empty())
        capture.segmentation_parameter_profile_ref = tool.segmentation_parameter_profile_ref;

    capture.segmentation_region_count += tool.segmentation_region_count;
    capture.segmentation_raw_result_available = capture.segmentation_raw_result_available || tool.segmentation_raw_result_available;
    capture.segmentation_refined_result_available = capture.segmentation_refined_result_available || tool.segmentation_refined_result_available;
    capture.segmentation_fallback_used = capture.segmentation_fallback_used || tool.segmentation_fallback_used;

    if (!tool.segmentation_result_stage.empty())
        capture.segmentation_result_stage = tool.segmentation_result_stage;

    if (!tool.segmentation_refinement_method.empty())
        capture.segmentation_refinement_method = tool.segmentation_refinement_method;

    if (!tool.segmentation_requested_geometry_type.empty())
        capture.segmentation_requested_geometry_type = tool.segmentation_requested_geometry_type;
    if (!tool.segmentation_geometry_fit_status.empty())
        capture.segmentation_geometry_fit_status = tool.segmentation_geometry_fit_status;
    if (!tool.segmentation_geometry_fit_reason.empty())
        capture.segmentation_geometry_fit_reason = tool.segmentation_geometry_fit_reason;
    if (!tool.segmentation_oriented_box_evidence_ref.empty())
        capture.segmentation_oriented_box_evidence_ref =
            tool.segmentation_oriented_box_evidence_ref;
    if (!tool.segmentation_oriented_box_conclusion.empty())
        capture.segmentation_oriented_box_conclusion =
            tool.segmentation_oriented_box_conclusion;
    capture.segmentation_geometry_count += tool.segmentation_geometry_count;
    if (tool.segmentation_geometry_count > 0) {
        capture.segmentation_geometry_residual_px = tool.segmentation_geometry_residual_px;
        capture.segmentation_geometry_support = tool.segmentation_geometry_support;
        capture.segmentation_geometry_center_x = tool.segmentation_geometry_center_x;
        capture.segmentation_geometry_center_y = tool.segmentation_geometry_center_y;
        capture.segmentation_geometry_radius = tool.segmentation_geometry_radius;
        capture.segmentation_geometry_axis_x = tool.segmentation_geometry_axis_x;
        capture.segmentation_geometry_axis_y = tool.segmentation_geometry_axis_y;
        capture.segmentation_geometry_angle_deg = tool.segmentation_geometry_angle_deg;
    }

    if (!tool.segmentation_raw_result_ref.empty())
        capture.segmentation_raw_result_ref = tool.segmentation_raw_result_ref;

    if (!tool.segmentation_raw_mask_ref.empty())
        capture.segmentation_raw_mask_ref = tool.segmentation_raw_mask_ref;

    if (!tool.segmentation_raw_contour_ref.empty())
        capture.segmentation_raw_contour_ref = tool.segmentation_raw_contour_ref;

    if (!tool.segmentation_raw_overlay_ref.empty())
        capture.segmentation_raw_overlay_ref = tool.segmentation_raw_overlay_ref;

    if (!tool.segmentation_refined_result_ref.empty())
        capture.segmentation_refined_result_ref = tool.segmentation_refined_result_ref;

    if (!tool.segmentation_refined_mask_ref.empty())
        capture.segmentation_refined_mask_ref = tool.segmentation_refined_mask_ref;

    if (!tool.segmentation_refined_contour_ref.empty())
        capture.segmentation_refined_contour_ref = tool.segmentation_refined_contour_ref;

    if (!tool.segmentation_refined_overlay_ref.empty())
        capture.segmentation_refined_overlay_ref = tool.segmentation_refined_overlay_ref;

    if (tool.torch_ok != 0)
        capture.torch_ok = tool.torch_ok;
    if (tool.torch_error_code != 0)
        capture.torch_error_code = tool.torch_error_code;
    if (tool.torch_train_ms != 0.0)
        capture.torch_train_ms = tool.torch_train_ms;
    if (tool.torch_infer_ms != 0.0)
        capture.torch_infer_ms = tool.torch_infer_ms;
    if (tool.torch_total_ms != 0.0)
        capture.torch_total_ms = tool.torch_total_ms;
    capture.torch_result_count += tool.torch_result_count;
    if (!tool.torch_status.empty())
        capture.torch_status = tool.torch_status;
    if (!tool.torch_failure_stage.empty())
        capture.torch_failure_stage = tool.torch_failure_stage;
    if (!tool.torch_reason.empty())
        capture.torch_reason = tool.torch_reason;
    if (!tool.torch_evidence_ref.empty())
        capture.torch_evidence_ref = tool.torch_evidence_ref;
    if (!tool.torch_primary_visual_ref.empty())
        capture.torch_primary_visual_ref = tool.torch_primary_visual_ref;
    if (!tool.torch_trainer_lifecycle_summary.empty())
        capture.torch_trainer_lifecycle_summary = tool.torch_trainer_lifecycle_summary;
    if (!tool.torch_unified_mainline_summary.empty())
        capture.torch_unified_mainline_summary = tool.torch_unified_mainline_summary;

    if (capture.failure_stage.empty() && !tool.failure_stage.empty())
        capture.failure_stage = tool.failure_stage;

    if (capture.reason.empty() && !tool.reason.empty())
        capture.reason = tool.reason;

    if (tool.has_fit_circle)
    {
        capture.circle_radius = tool.circle_radius;
    }

    for (const auto& shape : tool.shapes)
    {
        capture.shapes.push_back(shape);

        if (shape.semantic_role == "roi" ||
            shape.semantic_role == "learn_roi" ||
            shape.semantic_role == "search_roi")
        {
            capture.rendered_roi_count++;
        }

        if (shape.semantic_role == "scan")
            capture.rendered_scan_count++;

        if (shape.semantic_role == "measure_points")
        {
            capture.rendered_measure_points_count +=
                static_cast<int>(shape.points.size() / 2);
        }

        if (shape.semantic_role == "result" ||
            shape.semantic_role == "boundary" ||
            shape.semantic_role == "boundary_bbox" ||
            shape.semantic_role == "model_best_result" ||
            shape.semantic_role == "model_candidate" ||
            shape.semantic_role == "model_segmentation_mask" ||
            shape.semantic_role == "model_segmentation_contour")
            capture.rendered_result_count++;
    }
}

bool CaptureRuntimeToolResults(
    mu::CxParserRuntime& runtime,
    CxScriptExecutionCapture& capture,
    std::string& reason)
{
    bool supported_object_found = false;
    std::unordered_set<void*> captured_objects;

    const int findline_count = runtime.GetClassObjSum("FindLine");

    for (int i = 0; i < findline_count; ++i)
    {
        FindLine* tool = static_cast<FindLine*>(
            runtime.GetClassObj("FindLine", i));

        if (tool == nullptr)
            continue;
        if (!captured_objects.insert(tool).second)
            continue;

        supported_object_found = true;

        CxScriptToolResultCapture tool_capture;

        const std::string object_name =
            runtime.GetClassObjName("FindLine", i);

        try
        {
            if (!CaptureFindLineResult(*tool, object_name, tool_capture))
            {
                reason = "failed to capture Findline: " + object_name;
                return false;
            }
        }
        catch (...)
        {
            reason = "CaptureFindLineResult crashed for: " + object_name;
            return false;
        }

        capture.scan_line_count += tool->getscanlinecount();
        capture.sample_count += tool->getsamplecount();

        MergeToolCapture(tool_capture, capture);
    }

    const int findcircle_count = runtime.GetClassObjSum("FindCircle");

    for (int i = 0; i < findcircle_count; ++i)
    {
        FindCircle* tool = static_cast<FindCircle*>(
            runtime.GetClassObj("FindCircle", i));

        if (tool == nullptr)
            continue;
        if (!captured_objects.insert(tool).second)
            continue;

        supported_object_found = true;

        CxScriptToolResultCapture tool_capture;

        const std::string object_name =
            runtime.GetClassObjName("FindCircle", i);

        try
        {
            if (!CaptureFindCircleResult(*tool, object_name, tool_capture))
            {
                reason = "failed to capture FindCircle: " + object_name;
                return false;
            }
        }
        catch (...)
        {
            reason = "CaptureFindCircleResult crashed for: " + object_name;
            return false;
        }

        capture.scan_line_count += tool->getscanlinecount();
        capture.sample_count += tool->getsamplecount();

        MergeToolCapture(tool_capture, capture);
    }

    const int findellipse_count = runtime.GetClassObjSum("FindEllipse");

    for (int i = 0; i < findellipse_count; ++i)
    {
        FindEllipse* tool = static_cast<FindEllipse*>(
            runtime.GetClassObj("FindEllipse", i));

        if (tool == nullptr)
            continue;
        if (!captured_objects.insert(tool).second)
            continue;

        supported_object_found = true;

        CxScriptToolResultCapture tool_capture;

        const std::string object_name =
            runtime.GetClassObjName("FindEllipse", i);

        try
        {
            if (!CaptureFindEllipseResult(*tool, object_name, tool_capture))
            {
                reason = "failed to capture FindEllipse: " + object_name;
                return false;
            }
        }
        catch (...)
        {
            reason = "CaptureFindEllipseResult crashed for: " + object_name;
            return false;
        }

        MergeToolCapture(tool_capture, capture);
    }

    const int findrect_count = runtime.GetClassObjSum("FindRect");

    for (int i = 0; i < findrect_count; ++i)
    {
        FindRect* tool = static_cast<FindRect*>(
            runtime.GetClassObj("FindRect", i));

        if (tool == nullptr)
            continue;
        if (!captured_objects.insert(tool).second)
            continue;

        supported_object_found = true;

        CxScriptToolResultCapture tool_capture;

        const std::string object_name =
            runtime.GetClassObjName("FindRect", i);

        try
        {
            if (!CaptureFindRectResult(*tool, object_name, tool_capture))
            {
                reason = "failed to capture FindRect: " + object_name;
                return false;
            }
        }
        catch (...)
        {
            reason = "CaptureFindRectResult crashed for: " + object_name;
            return false;
        }

        MergeToolCapture(tool_capture, capture);
    }

    const int findobject_count = runtime.GetClassObjSum("FindObject");

    for (int i = 0; i < findobject_count; ++i)
    {
        FindObject* tool = static_cast<FindObject*>(
            runtime.GetClassObj("FindObject", i));

        if (tool == nullptr)
            continue;
        if (!captured_objects.insert(tool).second)
            continue;

        supported_object_found = true;

        CxScriptToolResultCapture tool_capture;

        const std::string object_name =
            runtime.GetClassObjName("FindObject", i);

        try
        {
            if (!CaptureFindObjectResult(*tool, object_name, tool_capture))
            {
                reason = "failed to capture FindObject: " + object_name;
                return false;
            }
        }
        catch (...)
        {
            reason = "CaptureFindObjectResult crashed for: " + object_name;
            return false;
        }

        MergeToolCapture(tool_capture, capture);
    }

    const int cxtextinspect_count = runtime.GetClassObjSum("CxTextInspect");
    for (int i = 0; i < cxtextinspect_count; ++i)
    {
        CxTextInspect* tool = static_cast<CxTextInspect*>(
            runtime.GetClassObj("CxTextInspect", i));
        if (tool == nullptr)
            continue;
        if (!captured_objects.insert(tool).second)
            continue;

        supported_object_found = true;
        CxScriptToolResultCapture tool_capture;
        const std::string object_name =
            runtime.GetClassObjName("CxTextInspect", i);
        try
        {
            if (!CaptureCxTextInspectResult(*tool, object_name, tool_capture))
            {
                reason = "failed to capture CxTextInspect: " + object_name;
                return false;
            }
        }
        catch (...)
        {
            reason = "CaptureCxTextInspectResult crashed for: " + object_name;
            return false;
        }
        MergeToolCapture(tool_capture, capture);
    }

    const int fastmatch_count = runtime.GetClassObjSum("FastMatch");

    for (int i = 0; i < fastmatch_count; ++i)
    {
        FastMatch* tool = static_cast<FastMatch*>(
            runtime.GetClassObj("FastMatch", i));

        if (tool == nullptr)
            continue;
        if (!captured_objects.insert(tool).second)
            continue;

        supported_object_found = true;

        CxScriptToolResultCapture tool_capture;

        const std::string object_name =
            runtime.GetClassObjName("FastMatch", i);

        try
        {
            if (!CaptureFastMatchResult(*tool, object_name, tool_capture))
            {
                reason = "failed to capture FastMatch: " + object_name;
                return false;
            }
        }
        catch (...)
        {
            reason = "CaptureFastMatchResult crashed for: " + object_name;
            return false;
        }

        MergeToolCapture(tool_capture, capture);
    }

    const int findseg_count = runtime.GetClassObjSum("FindSegmentation");

    for (int i = 0; i < findseg_count; ++i)
    {
        FindSegmentation* tool = static_cast<FindSegmentation*>(
            runtime.GetClassObj("FindSegmentation", i));

        if (tool == nullptr)
            continue;
        if (!captured_objects.insert(tool).second)
            continue;

        supported_object_found = true;

        CxScriptToolResultCapture tool_capture;

        const std::string object_name =
            runtime.GetClassObjName("FindSegmentation", i);

        try
        {
            if (!CaptureFindSegmentationResult(*tool, object_name, tool_capture))
            {
                reason = "failed to capture FindSegmentation: " + object_name;
                return false;
            }
        }
        catch (...)
        {
            reason = "CaptureFindSegmentationResult crashed for: " + object_name;
            return false;
        }

        MergeToolCapture(tool_capture, capture);
    }

    const int torch_count = runtime.GetClassObjSum("TorchTask");

    for (int i = 0; i < torch_count; ++i)
    {
        TorchTask* tool = static_cast<TorchTask*>(
            runtime.GetClassObj("TorchTask", i));

        if (tool == nullptr)
            continue;
        if (!captured_objects.insert(tool).second)
            continue;

        supported_object_found = true;

        CxScriptToolResultCapture tool_capture;

        const std::string object_name =
            runtime.GetClassObjName("TorchTask", i);

        try
        {
            if (!CaptureTorchTaskResult(*tool, object_name, tool_capture))
            {
                reason = "failed to capture TorchTask: " + object_name;
                return false;
            }
        }
        catch (...)
        {
            reason = "CaptureTorchTaskResult crashed for: " + object_name;
            return false;
        }

        MergeToolCapture(tool_capture, capture);
    }

    if (!supported_object_found)
    {
        reason = "no supported cximage runtime object found; expected one of Findline, FindCircle, FindEllipse, FindObject, FindRect, FindSegmentation, Match, fastmatch or TorchTask";
        return false;
    }

    reason.clear();
    return true;
}

bool CaptureTorchTaskResult(
    TorchTask& tool,
    const std::string& object_name,
    CxScriptToolResultCapture& output)
{
    output.type = "TorchTask";
    output.name = object_name;
    output.owner_ref = object_name;

    output.torch_ok = tool.getok();
    output.torch_error_code = tool.geterrorcode();
    output.torch_train_ms = tool.gettrainms();
    output.torch_infer_ms = tool.getinferms();
    output.torch_total_ms = tool.gettotalms();
    output.torch_status = tool.getstatus();
    output.torch_failure_stage = tool.getfailstage();
    output.torch_reason = tool.getreason();
    output.torch_result_count = tool.getresultcount();
    output.torch_evidence_ref = tool.getevidenceref();
    output.torch_primary_visual_ref = tool.getprimaryvisualref();
    output.torch_trainer_lifecycle_summary = tool.gettrainersummary();
    output.torch_unified_mainline_summary = tool.getmainlinesummary();

    output.segmentation_result_ref = tool.getresultref();
    output.segmentation_mask_ref = tool.getmaskref();
    output.segmentation_overlay_ref = tool.getoverlayref();
    output.segmentation_status_code = tool.getok() != 0 ? 1 : 0;
    const CxInferenceResult& inference_result = tool.GetInferenceResult();
    if (inference_result.mask.has_value())
    {
        output.segmentation_contour_ref = inference_result.mask->contour_ref;
    }
    BackfillTorchSegmentationMetricsFromArtifacts(
        output.segmentation_contour_ref,
        output);
    if (output.segmentation_contour_count == 0 &&
        inference_result.mask.has_value() &&
        inference_result.mask->available)
    {
        output.segmentation_contour_count = output.torch_result_count;
    }

    output.algorithm_executed = tool.getok() != 0;
    output.measure_completed = tool.getok() != 0;
    output.valid_points_count = output.segmentation_contour_count;
    output.has_result_rect = output.segmentation_contour_count > 0;
    output.result_rect_count = output.segmentation_contour_count;
    if (output.segmentation_primary_area > 0.0)
        output.avgdist = output.segmentation_primary_area;

    if (tool.getok() == 0)
    {
        output.failure_stage = output.torch_failure_stage.empty() ? "torch_execute_failed" : output.torch_failure_stage;
        output.reason = output.torch_reason;
    }

    CxTorchResultProjector::Project(inference_result, "TorchTask", object_name, output.shapes);

    return true;
}
