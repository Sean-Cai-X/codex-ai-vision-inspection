
#include <iostream>

#include "torch_test_host.h"

int main() {
    const auto profile = TorchTestHost::current_profile();

    std::cout << "=================================\n";
    std::cout << " libtorch_module full validation\n";
    std::cout << " core + image/annotation + dataset + mobilevit/two-stage\n";
    std::cout << " torch is reserved for learned tensor tasks, not every AI-style task by default\n";
    std::cout << " structured geometry/numeric routing stays on the mlpack side unless the task explicitly needs end-to-end learning\n";
    std::cout << " OpenCV-backed path, OCC deferred\n";
    std::cout << " full-dataset = image/annotation + dataset only\n";
    std::cout << " full-image   = full-dataset + two-stage inference\n";
    std::cout << " full-train   = full-image + training smoke\n";
    std::cout << " preprocess/postprocess contract profiles are reserved for cxparser/cxcore bridging\n";
    std::cout << " active profile = " << TorchTestHost::profile_name(profile) << "\n";
    std::cout << " Toggle later-stage tests with TORCH_FULL_ENABLE_* macros\n";
    std::cout << "=================================\n";

    TorchTestHost host;
    const auto report = host.run_profile_report(profile);

    std::cout << " stage summary = " << report.summary << "\n";
    std::cout << " stage report  = " << TorchTestHost::format_report_line(report) << "\n";
    std::cout << " stage checks  =";
    for (const auto& check : report.checks) {
        std::cout << " " << check.name << ":" << (check.passed ? "pass" : "fail");
    }
    std::cout << "\n";
    for (const auto& line : TorchTestHost::format_check_lines(report)) {
        std::cout << " check report = " << line << "\n";
    }

    if (report.passed) {
        std::cout << "\nALL TESTS PASSED\n";
    } else {
        std::cerr << "\nTEST FAILURES: " << report.failures << "\n";
    }

    return report.failures;
}
