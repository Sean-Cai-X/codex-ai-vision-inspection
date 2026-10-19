#pragma once

#include "CxRuntimePaths.h"

namespace cxvision::metrology_analytics {
inline std::filesystem::path DefaultAnalyticsOutputRoot() {
    if (const char* root = std::getenv("CXVISION_RUN_ROOT"); root && *root)
        return std::filesystem::path(root) / "metrology_analytics";
    return CxRuntimeProjectRoot().parent_path() / "cxscript_runs" / "metrology_analytics";
}

inline bool ValidateAnalyticsOutputPath(const std::filesystem::path& path,
                                        std::string& reason) {
    const std::string text = path.generic_string();
#ifndef _WIN32
    if (text.find(':') != std::string::npos || text.find('\\') != std::string::npos) {
        reason = "Unmapped Windows path; configure a native absolute CXVISION_RUN_ROOT";
        return false;
    }
#endif
    if (path.empty() || !path.is_absolute()) {
        reason = "Analytics output requires an absolute path outside the source repository";
        return false;
    }
    std::error_code ec;
    const auto output = std::filesystem::weakly_canonical(path, ec);
    if (ec) { reason = "Cannot resolve analytics output: " + ec.message(); return false; }
    const auto project = std::filesystem::weakly_canonical(CxRuntimeProjectRoot(), ec);
    if (ec || project.empty()) { reason = "Cannot resolve source repository"; return false; }
    auto p = project.begin();
    auto o = output.begin();
    for (; p != project.end() && o != output.end() && *p == *o; ++p, ++o) {}
    if (p == project.end() || output == output.root_path()) {
        reason = "Analytics output must stay outside the source repository and filesystem root";
        return false;
    }
    reason.clear();
    return true;
}
} // namespace cxvision::metrology_analytics
