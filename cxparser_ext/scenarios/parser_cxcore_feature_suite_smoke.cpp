#include <iostream>
#include <string>

#include "../pipeline/parser_test_driver.h"

namespace
{
bool RunCase(const char *case_name)
{
  cxparser_ext::ParserTestRequest request;
  request.layer = "feature";
  request.module = "cxcore";
  request.case_name = case_name;
  request.mode = "build-run";
  request.report_on = true;

  cxparser_ext::ParserTestDriver driver;
  cxparser_ext::ParserTestRunResult result;
  if (!driver.Execute(request, result))
  {
    std::cerr << "[FAIL] cxcore feature case failed: " << case_name
              << " summary=" << result.summary << "\n";
    return false;
  }

  if (!result.success)
  {
    std::cerr << "[FAIL] cxcore feature case not marked success: " << case_name
              << " summary=" << result.summary << "\n";
    return false;
  }

  if (result.task_id.empty())
  {
    std::cerr << "[FAIL] cxcore feature case missing task id: " << case_name << "\n";
    return false;
  }

  if (result.details.empty())
  {
    std::cerr << "[FAIL] cxcore feature case missing report details: " << case_name << "\n";
    return false;
  }

  std::cout << "[PASS] cxcore.feature." << case_name
            << " task_id=" << result.task_id
            << " summary=" << result.summary
            << "\n";
  return true;
}
}

int main(int argc, char **argv)
{
  if (argc > 1)
  {
    if (!RunCase(argv[1]))
      return 1;

    std::cout << "[PASS] parser_cxcore_feature_suite_smoke case=" << argv[1] << "\n";
    return 0;
  }

  // Keep the default feature suite on execution-compatible cases.
  // Legacy combo/suite scripts with old Flow.* / Check.* bodies are assessed
  // separately and should not block the current build-run execution chain.
  if (!RunCase("line_measurement_balanced") ||
      !RunCase("circle_measurement_balanced") ||
      !RunCase("template_feature_match") ||
      !RunCase("rect_formfit_candidate_selection") ||
      !RunCase("region_boundary_analysis") ||
      !RunCase("region_boundary_analysis_golden") ||
      !RunCase("region_boundary_analysis_boundary") ||
      !RunCase("region_boundary_analysis_noise") ||
      !RunCase("region_boundary_analysis_degenerate"))
  {
    return 1;
  }

  std::cout << "[PASS] parser_cxcore_feature_suite_smoke\n";
  return 0;
}
