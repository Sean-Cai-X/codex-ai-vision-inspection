#include <cstring>
#include <iostream>

#include "../drivers/parser_dispatch_driver.h"

namespace
{
cxparser_ext::ParserDispatchRequest MakeCxcoreFeatureRequest(const char *case_id)
{
  cxparser_ext::ParserDispatchRequest request;
  request.script_type = "module";
  request.layer = "feature";
  request.module = "cxcore";
  request.case_id = case_id;
  request.mode = "build-run";
  request.report_on = true;
  return request;
}

bool RunCxcoreDefaultContractMainlineCase(const char *case_id,
                                          const char *expected_result_object,
                                          const char *expected_failure_mode)
{
  const cxparser_ext::ParserDispatchRequest request = MakeCxcoreFeatureRequest(case_id);
  cxparser_ext::ParserDispatchResult result;
  if (!cxparser_ext::RunDispatchRequest(request, result))
  {
    std::cerr << "[FAIL] cxcore mainline dispatch failed for " << case_id << "\n";
    return false;
  }

  if (!result.success ||
      result.skipped ||
      result.report.script_origin != "file" ||
      result.report.layer_profile.execution_text_kind != "source" ||
      result.tick.accepted_task_count != 1 ||
      result.tick.executed_task_count != 1)
  {
    std::cerr << "[FAIL] cxcore mainline execution metadata mismatch for "
              << case_id << "\n";
    return false;
  }

  if (result.report.result_object != expected_result_object)
  {
    std::cerr << "[FAIL] cxcore mainline result object mismatch for "
              << case_id
              << " actual=" << result.report.result_object
              << " expected=" << expected_result_object << "\n";
    return false;
  }

  if (result.report.failure_mode != expected_failure_mode)
  {
    std::cerr << "[FAIL] cxcore mainline failure mode mismatch for "
              << case_id
              << " actual=" << result.report.failure_mode
              << " expected=" << expected_failure_mode << "\n";
    return false;
  }

  if (result.report.task_id.empty() ||
      result.report.metrics.empty() ||
      result.report.summary.find("task validated") == std::string::npos)
  {
    std::cerr << "[FAIL] cxcore mainline contract fields incomplete for "
              << case_id << "\n";
    return false;
  }

  return true;
}
}

int main()
{
  struct CaseExpectation
  {
    const char *case_id;
    const char *result_object;
    const char *failure_mode;
  };

  static const CaseExpectation cases[] = {
    {"line_measurement_golden", "LineMeasurementOutput", "none"},
    {"line_measurement_boundary", "LineMeasurementOutput", "handled_boundary_condition"},
    {"line_measurement_noise", "LineMeasurementOutput", "handled_noise_condition"},
    {"line_measurement_degenerate", "LineMeasurementOutput", "handled_degenerate_input"},
    {"circle_measurement_golden", "CircleMeasurementOutput", "none"},
    {"circle_measurement_boundary", "CircleMeasurementOutput", "handled_boundary_condition"},
    {"circle_measurement_noise", "CircleMeasurementOutput", "handled_noise_condition"},
    {"circle_measurement_degenerate", "CircleMeasurementOutput", "handled_degenerate_input"},
    {"template_feature_match_golden", "MatchOutput", "none"},
    {"template_feature_match_boundary", "MatchOutput", "handled_boundary_condition"},
    {"template_feature_match_noise", "MatchOutput", "handled_noise_condition"},
    {"template_feature_match_degenerate", "MatchOutput", "handled_degenerate_input"},
    {"region_boundary_analysis_golden", "ImageAnalysisOutput", "none"},
    {"region_boundary_analysis_boundary", "ImageAnalysisOutput", "handled_boundary_condition"},
    {"region_boundary_analysis_noise", "ImageAnalysisOutput", "handled_noise_condition"},
    {"region_boundary_analysis_degenerate", "ImageAnalysisOutput", "handled_degenerate_input"}
  };

  const char *requested_case = 0;
  if (__argc > 1)
    requested_case = __argv[1];

  bool matched_requested_case = requested_case == 0;
  for (size_t index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index)
  {
    if (requested_case && std::strcmp(cases[index].case_id, requested_case) != 0)
      continue;

    matched_requested_case = true;
    if (!RunCxcoreDefaultContractMainlineCase(cases[index].case_id,
                                              cases[index].result_object,
                                              cases[index].failure_mode))
      return 1;
  }

  if (!matched_requested_case)
  {
    std::cerr << "[FAIL] cxcore_mainline_smoke unknown case filter: "
              << requested_case << "\n";
    return 1;
  }

  if (requested_case)
    std::cout << "[PASS] cxcore_mainline_smoke case=" << requested_case << "\n";
  else
    std::cout << "[PASS] cxcore_mainline_smoke 16 default contract cases\n";
  return 0;
}
