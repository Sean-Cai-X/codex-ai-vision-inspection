#include "CxScriptStage25Manifest.h"

Stage25ImageCase* Stage25Manifest::AddImage(const std::string& image_id, const std::string& level, const std::string& path)
{
    images.push_back({image_id, level, path});
    return &images.back();
}

Stage25FindlineProfile* Stage25Manifest::AddFindlineProfile(const std::string& profile_id)
{
    findline_profiles.push_back({profile_id});
    return &findline_profiles.back();
}

Stage25FindcircleProfile* Stage25Manifest::AddFindcircleProfile(const std::string& profile_id)
{
    findcircle_profiles.push_back({profile_id});
    return &findcircle_profiles.back();
}

Stage25EvidenceProfile* Stage25Manifest::AddEvidenceProfile(const std::string& name)
{
    evidence_profiles.push_back({name});
    return &evidence_profiles.back();
}