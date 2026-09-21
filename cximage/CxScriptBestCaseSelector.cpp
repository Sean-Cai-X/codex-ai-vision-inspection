#include "CxScriptBestCaseSelector.h"
#include <fstream>
#include <algorithm>

namespace
{
    bool IsOkScriptBestCase(const CxScriptSuiteCaseResult& r)
    {
        if (!r.contract_pass)
            return false;

        if (r.tool == "FindLine" && (!r.has_fit_line || r.valid_points_count < 2))
            return false;

        if (r.tool == "FindCircle" && (!r.has_fit_circle || r.valid_points_count < 3))
            return false;

        return true;
    }

    bool IsNgExpectedBestCase(const CxScriptSuiteCaseResult& r)
    {
        if (!r.contract_pass)
            return false;

        if (r.actual_policy_guard != "EXPECTED_FILTER_FAILURE_BASELINE")
            return false;

        return !r.failure_stage.empty();
    }

    struct BestCaseScore
    {
        const CxScriptSuiteCaseResult* result = nullptr;
        double score = 0.0;
    };

    double ComputeOkScriptScore(const CxScriptSuiteCaseResult& r)
    {
        double score = 0.0;

        score += static_cast<double>(r.valid_points_count) * 10.0;
        score += r.local_support * 100.0;
        score -= r.local_mean_distance * 50.0;
        score -= r.fit_offset * 20.0;

        if (r.has_fit_line || r.has_fit_circle)
            score += 100.0;

        return score;
    }

    double ComputeNgExpectedScore(const CxScriptSuiteCaseResult& r)
    {
        double score = 0.0;

        if (!r.failure_stage.empty())
            score += 100.0;

        return score;
    }

    void CopyCaseFiles(
        const CxScriptSuiteCaseResult& result,
        const std::filesystem::path& destDir)
    {
        std::filesystem::create_directories(destDir);

        auto copyIfExists = [&](const std::string& src, const std::string& destName)
        {
            if (!src.empty() && std::filesystem::exists(src))
            {
                std::filesystem::copy(src, destDir / destName,
                    std::filesystem::copy_options::overwrite_existing);
            }
        };

        copyIfExists(result.image_path, "original.png");
        copyIfExists(result.result_overlay_path, "result_overlay.png");
        copyIfExists(result.evidence_overlay_path, "evidence_overlay.png");
        copyIfExists(result.tool_display_path, "tool_display.png");
        copyIfExists(result.snapshot_path, "snapshot.txt");
        copyIfExists(result.summary_path, "result_summary.json");
    }
}

void CxScriptBestCaseSelector::SelectAndExportBestExamples(
    const std::filesystem::path& outRoot,
    const std::vector<CxScriptSuiteCaseResult>& caseResults)
{
    std::vector<BestCaseScore> okScores;
    std::vector<BestCaseScore> ngScores;

    for (const auto& r : caseResults)
    {
        if (r.expected_result == "ok" && IsOkScriptBestCase(r))
        {
            BestCaseScore score;
            score.result = &r;
            score.score = ComputeOkScriptScore(r);
            okScores.push_back(score);
        }
        else if (r.expected_result == "ng_expected" && IsNgExpectedBestCase(r))
        {
            BestCaseScore score;
            score.result = &r;
            score.score = ComputeNgExpectedScore(r);
            ngScores.push_back(score);
        }
    }

    std::sort(okScores.begin(), okScores.end(),
        [](const BestCaseScore& a, const BestCaseScore& b) { return a.score > b.score; });

    std::sort(ngScores.begin(), ngScores.end(),
        [](const BestCaseScore& a, const BestCaseScore& b) { return a.score > b.score; });

    const int maxBestCases = 5;

    for (size_t i = 0; i < std::min(static_cast<size_t>(maxBestCases), okScores.size()); ++i)
    {
        const auto& result = *okScores[i].result;

        std::filesystem::path toolDir = outRoot / "best_examples" / result.tool;
        std::filesystem::create_directories(toolDir);

        std::filesystem::path levelDir = toolDir / result.level;
        std::filesystem::create_directories(levelDir);

        std::filesystem::path destDir = levelDir / ("best_" + std::to_string(i + 1) + "_" + result.image_id);
        CopyCaseFiles(result, destDir);
    }

    for (size_t i = 0; i < std::min(static_cast<size_t>(maxBestCases), ngScores.size()); ++i)
    {
        const auto& result = *ngScores[i].result;

        std::filesystem::path toolDir = outRoot / "best_examples" / result.tool;
        std::filesystem::create_directories(toolDir);

        std::filesystem::path levelDir = toolDir / result.level;
        std::filesystem::create_directories(levelDir);

        std::filesystem::path destDir = levelDir / ("best_ng_" + std::to_string(i + 1) + "_" + result.image_id);
        CopyCaseFiles(result, destDir);
    }
}