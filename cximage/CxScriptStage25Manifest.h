#ifndef CXIMAGE_CXSCRIPT_STAGE25_MANIFEST_H
#define CXIMAGE_CXSCRIPT_STAGE25_MANIFEST_H

#include <string>
#include <vector>
#include <map>

struct Stage25ImageTarget
{
    std::string target_id;
    std::string tool;

    std::string orientation;
    std::string target_level;
    std::string target_role;

    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;

    int wgap = 8;
    int hgap = 32;

    int cx = 0;
    int cy = 0;
    int px = 0;
    int py = 0;

    int gap = 5;
    int linegap = 3;

    std::string expected_edge;
    std::string edge_polarity_hint;
    std::string comment;
};

struct Stage25ImageCase
{
    std::string image_id;
    std::string level;
    std::string path;
    std::vector<std::string> tags;

    std::string expected_edge;
    std::string lighting;
    std::string contrast;

    std::vector<Stage25ImageTarget> targets;
};

struct Stage25FindlineProfile
{
    std::string profile_id;

    int method = 0;
    int threshold = 20;
    int linegap = 6;
    int fitmode = 1;
    int script_scale = 1;

    int filter_profile = 0;

    bool has_explicit_filter = false;
    int objfilter = 0;
    int filter_borw = 21;
    int filter_min = 50;
    int filter_max = 100000;

    bool has_gamma = false;
    int gamma = 0;

    std::string policy;

    std::string parameter_policy_id;
    std::string parameter_role;
    bool is_product_default = false;
    bool is_stage25_default = false;
};

struct Stage25FindcircleProfile
{
    std::string profile_id;

    int method = 0;
    int threshold = 20;
    int gap = 5;
    int linegap = 3;

    bool has_filter = false;
    int findsetting = 0;
    int filter_borw = 21;
    int filter_min = 50;
    int filter_max = 100000;

    bool has_samplerate = false;
    double samplerate = 0.0;

    std::string policy;

    std::string parameter_policy_id;
    std::string parameter_role;
    bool is_product_default = false;
    bool is_stage25_default = false;
};

struct Stage25EvidenceProfile
{
    std::string name;

    double nearest_point_support_px = 3.0;
    double line_distance_support_px = 3.0;
    double min_gradient = 6.0;
    double min_gradient_ratio = 0.35;
};

struct Stage25Manifest
{
    std::string name;
    std::string outroot;
    std::string imageroot;

    int min_images_for_stability = 3;
    int min_levels_for_stability = 2;

    std::vector<Stage25ImageCase> images;
    std::vector<Stage25FindlineProfile> findline_profiles;
    std::vector<Stage25FindcircleProfile> findcircle_profiles;
    std::vector<Stage25EvidenceProfile> evidence_profiles;

    Stage25ImageCase* AddImage(const std::string& image_id, const std::string& level, const std::string& path);
    Stage25FindlineProfile* AddFindlineProfile(const std::string& profile_id);
    Stage25FindcircleProfile* AddFindcircleProfile(const std::string& profile_id);
    Stage25EvidenceProfile* AddEvidenceProfile(const std::string& name);
};

#endif