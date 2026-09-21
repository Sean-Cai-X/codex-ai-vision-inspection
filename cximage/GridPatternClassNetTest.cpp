#include "FastMatchGridClassAdapter.h"
#include "GridPatternClassNet.h"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include <opencv2/imgproc.hpp>

namespace {

cv::Mat MakeGlyphA(int offset_x)
{
    cv::Mat image(64, 64, CV_8UC1, cv::Scalar(255));
    cv::line(image, cv::Point(16 + offset_x, 54), cv::Point(31 + offset_x, 10), cv::Scalar(0), 5);
    cv::line(image, cv::Point(31 + offset_x, 10), cv::Point(48 + offset_x, 54), cv::Scalar(0), 5);
    cv::line(image, cv::Point(21 + offset_x, 38), cv::Point(42 + offset_x, 38), cv::Scalar(0), 5);
    return image;
}

cv::Mat MakeGlyphB(int offset_x)
{
    cv::Mat image(64, 64, CV_8UC1, cv::Scalar(255));
    cv::line(image, cv::Point(18 + offset_x, 8), cv::Point(18 + offset_x, 56), cv::Scalar(0), 5);
    cv::ellipse(image, cv::Point(29 + offset_x, 21), cv::Size(13, 12), 0.0, -90.0, 90.0, cv::Scalar(0), 5);
    cv::ellipse(image, cv::Point(29 + offset_x, 43), cv::Size(14, 13), 0.0, -90.0, 90.0, cv::Scalar(0), 5);
    return image;
}

bool VectorsEqual(const std::vector<double>& lhs, const std::vector<double>& rhs)
{
    if (lhs.size() != rhs.size())
    {
        return false;
    }
    for (size_t index = 0; index < lhs.size(); ++index)
    {
        if (std::abs(lhs[index] - rhs[index]) > 1.0e-12)
        {
            return false;
        }
    }
    return true;
}

bool Check(bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "[FAIL] " << message << '\n';
        return false;
    }
    return true;
}

}  // namespace

int main()
{
    cxcore::GridPatternConfig config;
    config.min_class_score = 0.20;
    config.min_class_margin = 0.005;

    cxcore::GridPatternClassNet net;
    std::string config_reason;
    if (!Check(net.SetConfig(config, &config_reason), config_reason))
    {
        return 1;
    }

    const cv::Mat glyph_a = MakeGlyphA(0);
    const cv::Mat glyph_b = MakeGlyphB(0);
    const std::vector<double> descriptor_a1 = net.BuildDescriptor(glyph_a);
    const std::vector<double> descriptor_a2 = net.BuildDescriptor(glyph_a);
    if (!Check(!descriptor_a1.empty(), "descriptor must not be empty") ||
        !Check(VectorsEqual(descriptor_a1, descriptor_a2), "descriptor must be deterministic"))
    {
        return 2;
    }

    const cxcore::GridFeatureMap feature_map = net.BuildFeatureMap(glyph_a);
    const cxcore::GridPatternHierarchy hierarchy = net.BuildHierarchy(feature_map);
    if (!Check(feature_map.success, "feature map must be available") ||
        !Check(feature_map.active_cell_count > 0, "active cells must be observable") ||
        !Check(hierarchy.success, "hierarchy must be available") ||
        !Check(hierarchy.levels.size() == 3u, "default hierarchy must contain three levels") ||
        !Check(hierarchy.levels.front().nodes.front().parent_node_id >= 0,
               "fine level nodes must map to parent nodes"))
    {
        return 3;
    }

    const std::vector<cxcore::GridPatternTrainingSample> samples = {
        {"A", MakeGlyphA(-1)},
        {"A", MakeGlyphA(0)},
        {"A", MakeGlyphA(1)},
        {"B", MakeGlyphB(-1)},
        {"B", MakeGlyphB(0)},
        {"B", MakeGlyphB(1)}
    };
    const cxcore::GridPatternTrainingReport training = net.Fit(samples);
    if (!Check(training.success, "prototype fitting must succeed") ||
        !Check(training.class_count == 2, "two class prototypes must be fitted"))
    {
        return 4;
    }

    const cxcore::GridClassResult result_a = net.Infer(glyph_a);
    const cxcore::GridClassResult result_b = net.Infer(glyph_b);
    if (!Check(result_a.success && !result_a.rejected, "glyph A must be accepted") ||
        !Check(result_a.class_id == "A", "glyph A class must be A") ||
        !Check(result_b.success && !result_b.rejected, "glyph B must be accepted") ||
        !Check(result_b.class_id == "B", "glyph B class must be B"))
    {
        return 5;
    }

    const cv::Mat overlay = net.RenderFeatureOverlay(glyph_a, result_a.feature_map);
    if (!Check(!overlay.empty(), "grid direction overlay must be generated"))
    {
        return 6;
    }

    cxcore::FastMatchGridClassAdapter adapter;
    cxcore::FastMatchGridFusionInput fusion_input;
    fusion_input.candidate_id = "candidate_0";
    fusion_input.structural_available = true;
    fusion_input.candidate_count = 1;
    fusion_input.structural_score = 0.90;
    fusion_input.class_result = result_a;
    const cxcore::FastMatchGridFusionResult fusion = adapter.Fuse(fusion_input);
    if (!Check(fusion.success && !fusion.rejected, "cascade fusion must accept the candidate") ||
        !Check(fusion.class_id == "A", "fusion must preserve the class result") ||
        !Check(fusion.evidence_stages.size() == 6u, "fusion evidence chain must be complete"))
    {
        return 7;
    }

    std::cout
        << "[GRID_PATTERN_CLASS_NET_TEST] success=true"
        << " descriptor_dim=" << descriptor_a1.size()
        << " active_cells=" << feature_map.active_cell_count
        << " hierarchy_levels=" << hierarchy.levels.size()
        << " class_a_score=" << result_a.best_score
        << " class_b_score=" << result_b.best_score
        << " fusion_score=" << fusion.fused_score
        << '\n';
    return 0;
}
