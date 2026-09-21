#include "../catalog/parser_cximage_lowlevel_catalog.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
void Expect(bool condition, const std::string &message)
{
  if (!condition)
    throw std::runtime_error(message);
}
}

int main()
{
  std::vector<cxparser_ext::CximageLowLevelCapability> capabilities;
  Expect(cxparser_ext::BuildCximageLowLevelCapabilityCatalog(capabilities),
         "cximage low-level capability catalog should build");
  Expect(capabilities.size() >= 7, "cximage low-level capability count mismatch");

  int operator_count = 0;
  int matcher_count = 0;
  int feature_count = 0;
  int embedded_model_count = 0;
  int fragment_ready_count = 0;
  int script_friendly_count = 0;

  for (size_t index = 0; index < capabilities.size(); ++index)
  {
    const cxparser_ext::CximageLowLevelCapability &capability = capabilities[index];
    Expect(!capability.capability_id.empty(), "capability id should not be empty");
    Expect(!capability.category.empty(), "capability category should not be empty");
    Expect(!capability.summary.empty(), "capability summary should not be empty");
    Expect(!capability.source_files.empty(), "capability source files should not be empty");
    Expect(!capability.lightweight_inputs.empty(), "capability lightweight inputs should not be empty");
    Expect(!capability.lightweight_outputs.empty(), "capability lightweight outputs should not be empty");
    Expect(!capability.keep_in_low_level_reasons.empty(),
           "capability low-level reasons should not be empty");

    operator_count += capability.category == "operator" ? 1 : 0;
    matcher_count += capability.category == "matcher" ? 1 : 0;
    feature_count += capability.category == "feature" ? 1 : 0;
    embedded_model_count += capability.category == "embedded_model" ? 1 : 0;
    fragment_ready_count += capability.recommended_for_fragment ? 1 : 0;
    script_friendly_count += capability.script_friendly ? 1 : 0;
  }

  Expect(operator_count >= 1, "expected at least one operator capability");
  Expect(matcher_count >= 2, "expected at least two matcher capabilities");
  Expect(feature_count >= 3, "expected at least three feature capabilities");
  Expect(embedded_model_count >= 1, "expected at least one embedded_model capability");
  Expect(fragment_ready_count >= 5, "expected at least five fragment-ready capabilities");
  Expect(script_friendly_count >= 5, "expected at least five script-friendly capabilities");

  cxparser_ext::CximageLowLevelCapability matcher;
  Expect(cxparser_ext::FindCximageLowLevelCapability(
             capabilities,
             "cximage.matcher.fast_template_match",
             matcher),
         "fast template matcher capability should resolve");
  Expect(matcher.category == "matcher", "matcher category mismatch");
  Expect(matcher.recommended_for_fragment,
         "fast template matcher should be fragment-ready");

  cxparser_ext::CximageLowLevelCapability region;
  Expect(cxparser_ext::FindCximageLowLevelCapability(
             capabilities,
             "cximage.matcher.findobject_region",
             region),
         "findobject region capability should resolve");
  Expect(region.script_friendly,
         "findobject region should stay script-friendly at the I/O level");

  cxparser_ext::CximageLowLevelGuidance guidance;
  Expect(cxparser_ext::BuildCximageLowLevelGuidance(guidance),
         "cximage low-level guidance should build");
  Expect(guidance.recommended_script_inputs.size() >= 4,
         "recommended script inputs mismatch");
  Expect(guidance.recommended_script_outputs.size() >= 5,
         "recommended script outputs mismatch");
  Expect(guidance.keep_in_low_level_capabilities.size() >= 5,
         "keep-low-level capability guidance mismatch");
  Expect(guidance.upper_layer_relief_rules.size() >= 4,
         "upper-layer relief rule count mismatch");

  const std::string report =
      cxparser_ext::BuildCximageLowLevelCapabilityReport(capabilities, guidance);
  Expect(report.find("[CXIMAGE-LOWLEVEL]") != std::string::npos,
         "cximage low-level report header missing");
  Expect(report.find("cximage.operator.image_prepare_basic_roi") != std::string::npos,
         "image prepare capability missing in report");
  Expect(report.find("cximage.feature.line_measure_roi") != std::string::npos,
         "line measure capability missing in report");
  Expect(report.find("cximage.feature.circle_measure_fit") != std::string::npos,
         "circle measure capability missing in report");
  Expect(report.find("cximage.feature.ellipse_measure_roi") != std::string::npos,
         "ellipse measure capability missing in report");
  Expect(report.find("cximage.matcher.fast_template_match") != std::string::npos,
         "template matcher capability missing in report");
  Expect(report.find("[KEEP-LOWLEVEL]") != std::string::npos,
         "keep-low-level guidance missing in report");
  Expect(report.find("[RELIEF-RULE]") != std::string::npos,
         "relief-rule guidance missing in report");

  std::cout << report;
  return 0;
}
