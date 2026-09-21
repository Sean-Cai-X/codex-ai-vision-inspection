#pragma once

#include "GridPatternClassNet.h"

#include <string>
#include <vector>

namespace cxcore {

enum class FastMatchGridFusionMode
{
    StructuralOnly = 0,
    GridClassOnly = 1,
    Cascade = 2,
    ScoreFusion = 3
};

struct FastMatchGridFusionConfig
{
    FastMatchGridFusionMode mode = FastMatchGridFusionMode::Cascade;
    double structural_min_score = 0.55;
    double class_min_score = 0.55;
    double structural_weight = 0.60;
    double class_weight = 0.40;
};

struct FastMatchGridFusionInput
{
    std::string candidate_id;
    bool structural_available = false;
    int candidate_count = 0;
    double structural_score = 0.0;
    double candidate_x = 0.0;
    double candidate_y = 0.0;
    double candidate_width = 0.0;
    double candidate_height = 0.0;
    double resolved_angle_degrees = 0.0;
    double resolved_scale_x = 1.0;
    double resolved_scale_y = 1.0;
    GridClassResult class_result;
};

struct FastMatchGridFusionResult
{
    bool success = false;
    bool rejected = true;
    std::string candidate_id;
    std::string class_id;
    std::string mode;
    double structural_score = 0.0;
    double class_score = 0.0;
    double fused_score = 0.0;
    std::vector<std::string> anomaly_flags;
    std::vector<std::string> evidence_stages;
    std::string summary;
};

class FastMatchGridClassAdapter
{
public:
    void SetConfig(const FastMatchGridFusionConfig& config);
    const FastMatchGridFusionConfig& GetConfig() const;
    FastMatchGridFusionResult Fuse(const FastMatchGridFusionInput& input) const;

private:
    FastMatchGridFusionConfig config_;
};

}  // namespace cxcore
