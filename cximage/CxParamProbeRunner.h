#pragma once

#include <string>
#include <vector>
#include "CxParamRegressionRuntime.h"
#include "CxScriptHeadlessRuntime.h"

struct CxParamProbeRequest
{
    CxParamRegressionTask task;
    CxParamCandidate candidate;
    std::string image_path;
    std::string target_id;
    std::string script_path;
    std::string contract_path;
    std::string out_dir;
    int timeout_seconds = 10;

    int roi_x0 = 0;
    int roi_y0 = 0;
    int roi_x1 = 0;
    int roi_y1 = 0;

    int circle_cx = 0;
    int circle_cy = 0;
    int circle_px = 0;
    int circle_py = 0;

    int ellipse_x0 = 0;
    int ellipse_y0 = 0;
    int ellipse_x1 = 0;
    int ellipse_y1 = 0;

    int tool_half_width = 20;
    int max_elapsed_ms = 5000;
    int max_scan_lines = 4096;
    int max_samples = 200000;
};

struct CxParamProbeResult
{
    bool launched = false;
    bool executed = false;
    bool runtime_ok = false;
    bool assets_complete = false;
    bool probe_ok = false;
    bool support_available = false;
    bool timeout = false;
    int exit_code = -1;
    std::string result_summary_path;
    std::string result_overlay_path;
    std::string evidence_overlay_path;
    std::string tool_display_path;
    std::string snapshot_path;
    std::string failure_stage;
    std::string reason;

    int candidate_points = 0;
    bool fit_available = false;
    double support_score = 0.0;
    double mean_distance = 0.0;
    double fit_offset = 0.0;
};

bool RunSingleParamProbe(
    const CxParamProbeRequest& request,
    CxParamProbeResult& result);

void InjectCandidateGlobals(
    CxScriptHeadlessOptions& options,
    const CxParamCandidate& candidate);
