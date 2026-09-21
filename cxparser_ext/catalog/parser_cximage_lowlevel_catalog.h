#ifndef CXPARSER_EXT_PARSER_CXIMAGE_LOWLEVEL_CATALOG_H
#define CXPARSER_EXT_PARSER_CXIMAGE_LOWLEVEL_CATALOG_H

#include <string>
#include <vector>

namespace cxparser_ext
{
struct CximageLightweightIoShape
{
  std::string name;
  std::string direction;
  std::string summary;
};

struct CximageLowLevelCapability
{
  std::string capability_id;
  std::string category;
  std::string summary;
  std::vector<std::string> source_files;
  std::vector<std::string> lightweight_inputs;
  std::vector<std::string> lightweight_outputs;
  std::vector<std::string> reusable_fragments;
  std::vector<std::string> keep_in_low_level_reasons;
  bool script_friendly = false;
  bool recommended_for_fragment = false;
};

struct CximageLowLevelGuidance
{
  std::vector<CximageLightweightIoShape> recommended_script_inputs;
  std::vector<CximageLightweightIoShape> recommended_script_outputs;
  std::vector<std::string> keep_in_low_level_capabilities;
  std::vector<std::string> upper_layer_relief_rules;
};

bool BuildCximageLowLevelCapabilityCatalog(
    std::vector<CximageLowLevelCapability> &capabilities);
bool BuildCximageLowLevelGuidance(CximageLowLevelGuidance &guidance);
bool FindCximageLowLevelCapability(
    const std::vector<CximageLowLevelCapability> &capabilities,
    const std::string &capability_id,
    CximageLowLevelCapability &capability);
std::string BuildCximageLowLevelCapabilityReport(
    const std::vector<CximageLowLevelCapability> &capabilities,
    const CximageLowLevelGuidance &guidance);
}

#endif
