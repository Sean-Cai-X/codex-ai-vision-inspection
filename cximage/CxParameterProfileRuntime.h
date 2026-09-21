#pragma once

#include <string>
#include <vector>

struct CxParameterProfile
{
    std::string profile_id;
    std::string tool;
    std::string role;
    std::string description;

    int method = 0;
    bool has_method = false;
    int threshold = 20;
    bool has_threshold = false;
    int gap = 5;
    bool has_gap = false;
    int linegap = 6;
    bool has_linegap = false;
    int min_edge_run_width_px = 3;
    bool has_min_edge_run_width_px = false;
    int wgap = 32;
    bool has_wgap = false;
    int hgap = 8;
    bool has_hgap = false;
    int filterprofile = 0;
    bool has_filterprofile = false;
};

struct CxParameterProfileRuntime
{
    std::string current_tool;
    std::vector<CxParameterProfile> profiles;

    const CxParameterProfile* FindProfile(const std::string& profile_id) const;
    const CxParameterProfile* FindProfileByToolAndRole(const std::string& tool, const std::string& role) const;
    void Clear();
};

bool LoadCxParameterProfileFile(
    const std::string& script_path,
    CxParameterProfileRuntime& out_profiles,
    std::string& out_reason);
