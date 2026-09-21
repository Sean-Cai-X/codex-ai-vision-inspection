#include "../catalog/parser_cxscript_fragment_catalog.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
void Expect(bool condition, const std::string &message)
{
  if (!condition)
  {
    throw std::runtime_error(message);
  }
}

void ExpectStageCoverage(const cxparser_ext::CxscriptCapabilityFragment &fragment)
{
  bool has_input_prepare = false;
  bool has_operator_action = false;
  bool has_result_check = false;
  for (size_t i = 0; i < fragment.steps.size(); ++i)
  {
    const std::string &stage = fragment.steps[i].stage_name;
    has_input_prepare = has_input_prepare || stage == "input_prepare";
    has_operator_action = has_operator_action || stage == "operator_action";
    has_result_check = has_result_check || stage == "result_check";
  }
  Expect(has_input_prepare, fragment.fragment_id + " missing input_prepare stage");
  Expect(has_operator_action,
         fragment.fragment_id + " missing operator_action stage");
  Expect(has_result_check, fragment.fragment_id + " missing result_check stage");
}
}

int main()
{
  std::vector<cxparser_ext::CxscriptCapabilityFragment> fragments;
  Expect(cxparser_ext::BuildCxscriptFragmentCatalog(fragments),
         "fragment catalog should build");
  Expect(fragments.size() >= 12, "fragment catalog size mismatch");

  int operator_count = 0;
  int matcher_count = 0;
  int feature_count = 0;
  int embedded_model_count = 0;
  int integration_count = 0;

  for (size_t i = 0; i < fragments.size(); ++i)
  {
    const cxparser_ext::CxscriptCapabilityFragment &fragment = fragments[i];
    Expect(!fragment.fragment_id.empty(), "fragment id should not be empty");
    Expect(!fragment.summary.empty(), "fragment summary should not be empty");
    Expect(fragment.reusable_for_cxcore, "fragment must be reusable for cxcore");
    ExpectStageCoverage(fragment);

    operator_count += fragment.category == "operator" ? 1 : 0;
    matcher_count += fragment.category == "matcher" ? 1 : 0;
    feature_count += fragment.category == "feature" ? 1 : 0;
    embedded_model_count += fragment.category == "embedded_model" ? 1 : 0;
    integration_count += fragment.category == "integration" ? 1 : 0;
  }

  Expect(operator_count >= 1, "expected at least one operator fragment");
  Expect(matcher_count >= 1, "expected at least one matcher fragment");
  Expect(feature_count >= 3, "expected at least three feature fragments");
  Expect(embedded_model_count >= 2,
         "expected at least two embedded_model fragments");
  Expect(integration_count >= 5,
         "expected at least five integration fragments");

  cxparser_ext::CxscriptCapabilityFragment match_fragment;
  Expect(cxparser_ext::FindCxscriptCapabilityFragment(
             fragments,
             "cxscript.matcher.fast_template_match",
             match_fragment),
         "fast_template_match fragment should resolve");
  Expect(match_fragment.module_name == "cxvision",
         "matcher fragment module mismatch");

  cxparser_ext::CxscriptCapabilityFragment torch_geometry_fragment;
  Expect(cxparser_ext::FindCxscriptCapabilityFragment(
             fragments,
             "cxscript.integration.torch_geometry.input_prior_min",
             torch_geometry_fragment),
         "torch geometry input prior fragment should resolve");
  Expect(torch_geometry_fragment.module_name == "torch_geometry",
         "torch geometry fragment module mismatch");
  Expect(torch_geometry_fragment.expected_outputs.size() >= 2,
         "torch geometry fragment outputs missing");

  const std::string report =
      cxparser_ext::BuildCxscriptFragmentCatalogReport(fragments);
  Expect(report.find("[CXSCRIPT-FRAGMENTS]") != std::string::npos,
         "report header missing");
  Expect(report.find("cxscript.feature.line_measure_roi") != std::string::npos,
         "line feature missing in report");
  Expect(report.find("cxscript.embedded_model.mobilevit_mainline") !=
             std::string::npos,
         "mobilevit fragment missing in report");
  Expect(report.find("cxscript.integration.torch_geometry.input_prior_min") !=
             std::string::npos,
         "torch geometry input prior fragment missing in report");
  Expect(report.find("cxscript.integration.torch_geometry.attach_back_min") !=
             std::string::npos,
         "torch geometry attach fragment missing in report");
  Expect(report.find("cxscript.integration.torch_geometry.replay_min") !=
             std::string::npos,
         "torch geometry replay fragment missing in report");
  Expect(report.find("cxscript.integration.torch_geometry.replay_contract") !=
             std::string::npos,
         "torch geometry replay contract fragment missing in report");

  std::cout << report;
  return 0;
}
