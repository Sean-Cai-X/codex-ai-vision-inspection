#ifndef CXIMAGE_METROLOGY_ANALYTICS_CXMETROLOGYREFERENCEREPLAY_H
#define CXIMAGE_METROLOGY_ANALYTICS_CXMETROLOGYREFERENCEREPLAY_H

#include <filesystem>
#include <string>
#include <vector>

namespace cxvision::metrology_analytics
{

struct CxMetrologyReferenceAssertionResult
{
    std::string case_id;
    std::string case_kind;
    std::string metric;
    bool pass = false;
    double observed = 0.0;
    double expected = 0.0;
    double tolerance = 0.0;
    std::string reason;
};

struct CxMetrologyReferenceReplayResult
{
    std::filesystem::path reference_dir;
    std::filesystem::path summary_path;
    std::filesystem::path report_path;
    int reference_case_count = 0;
    int assertion_count = 0;
    int pass_count = 0;
    int fail_count = 0;
    std::vector<CxMetrologyReferenceAssertionResult> assertions;
};

bool RunMetrologyReferenceReplay(
    const std::filesystem::path& reference_dir,
    const std::filesystem::path& output_dir,
    CxMetrologyReferenceReplayResult& result,
    std::string& reason);

} // namespace cxvision::metrology_analytics

#endif
