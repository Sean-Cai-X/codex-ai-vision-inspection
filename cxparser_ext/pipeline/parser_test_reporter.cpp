#include "parser_test_reporter.h"

namespace cxparser_ext
{
std::string BuildTestReport(const ParserTestRunResult &result)
{
  std::string report = "[TEST] ";
  report += result.success ? "PASS " : "FAIL ";
  report += "layer=" + result.layer;
  report += " module=" + result.module;
  report += " case=" + result.case_name;
  report += " task_id=" + result.task_id;
  report += " build_planned=" + std::string(result.build_planned ? "true" : "false");
  report += " run_executed=" + std::string(result.run_executed ? "true" : "false");
  if (!result.summary.empty())
    report += " summary=" + result.summary;
  return report;
}
}
