#ifndef CXPARSER_EXT_PARSER_CXSCRIPT_FRAGMENT_CATALOG_H
#define CXPARSER_EXT_PARSER_CXSCRIPT_FRAGMENT_CATALOG_H

#include "../meta/parser_pseudocode_types.h"

#include <string>
#include <vector>

namespace cxparser_ext
{
bool BuildCxscriptFragmentCatalog(
    std::vector<CxscriptCapabilityFragment> &fragments);
bool FindCxscriptCapabilityFragment(
    const std::vector<CxscriptCapabilityFragment> &fragments,
    const std::string &fragment_id,
    CxscriptCapabilityFragment &fragment);
bool BuildCxscriptFlowFragmentBundles(
    const std::vector<CxscriptCapabilityFragment> &fragments,
    std::vector<CxscriptFlowFragmentBundle> &bundles);
bool FindCxscriptFlowFragmentBundle(
    const std::vector<CxscriptFlowFragmentBundle> &bundles,
    const std::string &bundle_id,
    CxscriptFlowFragmentBundle &bundle);
std::string BuildCxscriptFragmentCatalogReport(
    const std::vector<CxscriptCapabilityFragment> &fragments);
std::string BuildCxscriptFlowFragmentBundleReport(
    const std::vector<CxscriptFlowFragmentBundle> &bundles);
}

#endif
