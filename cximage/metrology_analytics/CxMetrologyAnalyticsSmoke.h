#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace cxvision::metrology_analytics
{

struct CxMetrologyAnalyticsSmokeCaseResult
{
    std::string case_id;
    std::string category;
    bool pass = false;
    double observed = 0.0;
    double expected = 0.0;
    double tolerance = 0.0;
    std::string reason;
};

struct CxMetrologyAnalyticsSmokeResult
{
    std::string run_id;
    int total_cases = 0;
    int pass_count = 0;
    int fail_count = 0;
    std::filesystem::path summary_path;
    std::filesystem::path report_path;
    std::vector<CxMetrologyAnalyticsSmokeCaseResult> cases;
};

bool RunMetrologyAnalyticsSmoke(
    const std::filesystem::path& output_dir,
    CxMetrologyAnalyticsSmokeResult& result,
    std::string& reason);

} // namespace cxvision::metrology_analytics

