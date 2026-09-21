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
}

int main()
{
  std::vector<cxparser_ext::CxscriptCapabilityFragment> fragments;
  Expect(cxparser_ext::BuildCxscriptFragmentCatalog(fragments),
         "fragment catalog should build");

  std::vector<cxparser_ext::CxscriptFlowFragmentBundle> bundles;
  Expect(cxparser_ext::BuildCxscriptFlowFragmentBundles(fragments, bundles),
         "bundle catalog should build");
  Expect(bundles.size() >= 6, "bundle count mismatch");

  cxparser_ext::CxscriptFlowFragmentBundle bundle;
  Expect(cxparser_ext::FindCxscriptFlowFragmentBundle(
             bundles, "cxscript.bundle.image_to_fast_match", bundle),
         "image_to_fast_match bundle should resolve");
  Expect(bundle.reusable_for_cxcore, "bundle should be reusable for cxcore");
  Expect(bundle.fragment_ids.size() >= 2, "bundle should contain fragment ids");
  Expect(!bundle.reusable_outputs.empty(), "bundle should expose outputs");

  cxparser_ext::CxscriptFlowFragmentBundle torch_geometry_bundle;
  Expect(cxparser_ext::FindCxscriptFlowFragmentBundle(
             bundles, "cxscript.bundle.torch_geometry_alignment_min", torch_geometry_bundle),
         "torch geometry alignment bundle should resolve");
  Expect(torch_geometry_bundle.reusable_for_cxcore,
         "torch geometry bundle should be reusable for cxcore");
  Expect(torch_geometry_bundle.fragment_ids.size() == 4,
         "torch geometry bundle fragment count mismatch");
  Expect(torch_geometry_bundle.reusable_outputs.size() >= 4,
         "torch geometry bundle outputs missing");
  Expect(torch_geometry_bundle.fragment_ids[3] ==
             "cxscript.integration.torch_geometry.replay_contract",
         "torch geometry bundle should default to replay contract");

  const std::string report =
      cxparser_ext::BuildCxscriptFlowFragmentBundleReport(bundles);
  Expect(report.find("[CXSCRIPT-BUNDLES]") != std::string::npos,
         "bundle report header missing");
  Expect(report.find("cxscript.bundle.image_to_line_feature") != std::string::npos,
         "line bundle missing");
  Expect(report.find("cxscript.bundle.embedded_model_train_mainline") !=
             std::string::npos,
         "embedded model bundle missing");
  Expect(report.find("cxscript.bundle.torch_geometry_alignment_min") !=
             std::string::npos,
         "torch geometry alignment bundle missing");

  std::cout << report;
  return 0;
}
