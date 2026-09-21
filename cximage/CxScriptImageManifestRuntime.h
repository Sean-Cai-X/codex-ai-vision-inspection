#pragma once

#include <string>
#include <vector>

struct CxScriptImageTargetRoi
{
    std::string target_id;
    std::string tool;
    std::string orientation;
    std::string roi_type;

    bool has_line = false;
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;

    bool has_circle = false;
    int cx = 0;
    int cy = 0;
    int px = 0;
    int py = 0;

    bool has_ellipse = false;
    double ellipse_major_radius = 0.0;
    double ellipse_minor_radius = 0.0;
    double ellipse_angle_deg = 0.0;

    bool has_rect = false;
    double rect_width = 0.0;
    double rect_height = 0.0;
    double rect_angle_deg = 0.0;

    int wgap = 32;
    bool has_wgap = false;
    int hgap = 8;
    bool has_hgap = false;
    int gap = 5;
    bool has_gap = false;
    int linegap = 6;
    bool has_linegap = false;
    int min_edge_run_width_px = 3;
    bool has_min_edge_run_width_px = false;
    int tool_half_width = 20;
    bool has_tool_half_width = false;
    int threshold = 20;
    bool has_threshold = false;
    int method = 2;
    bool has_method = false;

    std::string expected_edge;
    std::string edge_polarity_hint;
    std::string comment;
};

struct CxScriptImageManifestEntry
{
    std::string image_id;
    std::string level;
    std::string path;
    std::string source_path;

    int width = 0;
    int height = 0;
    int channels = 0;

    bool raw_not_cropped = false;
    bool raw_not_enhanced = false;
    bool raw_not_rotated = false;

    std::string sha256;

    std::vector<CxScriptImageTargetRoi> targets;
};

struct CxManifestRect
{
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
};

struct CxScriptFastMatchCase
{
    std::string case_id;
    std::string level;
    std::string tool;

    std::string template_image_id;
    std::string test_image_id;

    CxManifestRect template_rect;

    bool has_search_rect = false;
    bool search_rect_defaulted = false;
    std::string search_rect_source;
    CxManifestRect search_rect;

    CxManifestRect expected_rect;

    double rotation_min_deg = 0.0;
    double rotation_max_deg = 0.0;
    double scale_min = 1.0;
    double scale_max = 1.0;

    int candidate_budget = 0;

    std::string expected_variation;
    std::string review_focus;
    std::string comment;
};

struct CxScriptImageManifestRuntime
{
    std::string manifest_path;
    std::string image_root;
    std::string schema_version;
    std::string purpose;
    std::string selection_policy;

    int total_images = 0;
    int l0_count = 0;
    int l1_count = 0;
    int l2_count = 0;
    int l3_count = 0;

    std::vector<CxScriptImageManifestEntry> images;
    std::vector<CxScriptFastMatchCase> match_cases;
};

struct CxScriptImageManifestValidationIssue
{
    std::string severity;
    std::string image_id;
    std::string target_id;
    std::string message;
};

struct CxScriptImageManifestValidationResult
{
    bool ok = true;
    std::vector<CxScriptImageManifestValidationIssue> issues;
};

bool LoadStage25ImageManifestJson(
    const std::string& manifest_path,
    CxScriptImageManifestRuntime& out_manifest,
    std::string& out_reason);

CxScriptImageManifestValidationResult ValidateStage25ImageManifest(
    const CxScriptImageManifestRuntime& manifest);

const CxScriptImageManifestEntry* FindImageById(
    const CxScriptImageManifestRuntime& manifest,
    const std::string& image_id);

const CxScriptImageTargetRoi* FindTargetRoiByImageAndTargetId(
    const CxScriptImageManifestRuntime& manifest,
    const std::string& image_id,
    const std::string& target_id);

const CxScriptFastMatchCase* FindMatchCaseById(
    const CxScriptImageManifestRuntime& manifest,
    const std::string& case_id);

bool ParseDoubleRange(
    const std::string& text,
    double& out_min,
    double& out_max,
    std::string& reason);

bool WriteManifestDryRunReport(
    const CxScriptImageManifestRuntime& manifest,
    const std::string& output_dir);
