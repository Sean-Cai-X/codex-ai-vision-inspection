#include "pch.h"
#include "CxParamProbeRunner.h"
#include "CxScriptHeadlessRuntime.h"
#include <cmath>

#include <filesystem>

namespace fs = std::filesystem;

void InjectCandidateGlobals(
    CxScriptHeadlessOptions& options,
    const CxParamCandidate& candidate)
{
    options.method = candidate.method;
    options.threshold = candidate.threshold;
    options.gap = candidate.gap;
    options.linegap = candidate.linegap;
    options.min_edge_run_width_px = candidate.min_edge_run_width_px;
    options.wgap = candidate.wgap;
    options.hgap = candidate.hgap;
    options.filterprofile = candidate.filterprofile;
    options.samplerate = candidate.samplerate;
    options.min_score = candidate.min_score;
    options.find_num = candidate.find_num;
    options.compare_gap = candidate.compare_gap;
}

bool RunSingleParamProbe(
    const CxParamProbeRequest& request,
    CxParamProbeResult& result)
{
    result = {};

    if (request.script_path.empty())
    {
        result.reason = "script_path is empty";
        return false;
    }
    if (request.image_path.empty())
    {
        result.reason = "image_path is empty";
        return false;
    }
    if (request.out_dir.empty())
    {
        result.reason = "out_dir is empty";
        return false;
    }

    fs::create_directories(request.out_dir);

    CxScriptHeadlessOptions options;
    options.enabled = true;
    options.image_path = request.image_path;
    options.script_path = request.script_path;
    options.output_dir = request.out_dir;
    options.case_name = request.candidate.candidate_id;
    options.timeout_sec = request.timeout_seconds;
    options.stage25_tool = request.task.tool;
    options.stage25_target_id = request.target_id;
    options.target_id = request.target_id;

    options.roi_x0 = request.roi_x0;
    options.roi_y0 = request.roi_y0;
    options.roi_x1 = request.roi_x1;
    options.roi_y1 = request.roi_y1;
    options.circle_cx = request.circle_cx;
    options.circle_cy = request.circle_cy;
    options.circle_px = request.circle_px;
    options.circle_py = request.circle_py;
    options.ellipse_x0 = request.ellipse_x0;
    options.ellipse_y0 = request.ellipse_y0;
    options.ellipse_x1 = request.ellipse_x1;
    options.ellipse_y1 = request.ellipse_y1;
    options.tool_half_width = request.tool_half_width;
    options.max_elapsed_ms = request.max_elapsed_ms;
    options.max_scan_lines = request.max_scan_lines;
    options.max_samples = request.max_samples;

    InjectCandidateGlobals(options, request.candidate);

    CxScriptHeadlessResult headless_result;
    RunCxScriptHeadless(options, headless_result);

    result.launched = headless_result.launched;
    result.executed = headless_result.executed;
    result.runtime_ok = headless_result.runtime_ok;
    result.assets_complete = headless_result.assets_complete;
    result.timeout = headless_result.timed_out;
    result.exit_code = headless_result.exit_code;
    result.reason = headless_result.reason;
    result.failure_stage = headless_result.failure_stage;

    result.snapshot_path = headless_result.snapshot_path;
    result.result_summary_path = headless_result.summary_path;
    result.result_overlay_path = headless_result.result_overlay_path;
    result.evidence_overlay_path = headless_result.evidence_overlay_path;
    result.tool_display_path = headless_result.tool_display_path;

    result.candidate_points = headless_result.valid_points_count;
    result.fit_available = headless_result.has_fit_line || headless_result.has_fit_circle;
    const bool has_valid_geometry_metric =
        result.fit_available && result.candidate_points > 0;
    if (has_valid_geometry_metric && std::isfinite(headless_result.avgdist))
        result.mean_distance = headless_result.avgdist;
    if (has_valid_geometry_metric && std::isfinite(headless_result.fit_offset))
        result.fit_offset = headless_result.fit_offset;
    result.support_available =
        has_valid_geometry_metric && headless_result.support_available;
    if (result.support_available && std::isfinite(headless_result.local_support))
        result.support_score = headless_result.local_support;

    result.probe_ok =
        result.launched &&
        result.executed &&
        result.runtime_ok &&
        result.assets_complete &&
        !result.timeout;
    return result.probe_ok;

}
