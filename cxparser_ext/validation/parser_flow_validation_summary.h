#ifndef CXPARSER_EXT_PARSER_FLOW_VALIDATION_SUMMARY_H
#define CXPARSER_EXT_PARSER_FLOW_VALIDATION_SUMMARY_H

#include "../meta/parser_pseudocode_types.h"

#include <string>
#include <vector>

namespace cxparser_ext
{
struct FlowValidationSummary
{
  std::string flow_id;
  std::string layer;
  std::string module;
  int input_count = 0;
  int step_count = 0;
  int check_count = 0;
  int output_count = 0;
  bool requires_geometry_bulk = false;
  bool requires_cloud_bulk = false;
  bool requires_scene_mapping = false;
  bool requires_refresh_watch = false;
  bool requires_latency_watch = false;
  bool publish_path_involved = false;
  bool ready_for_current_module_validation = false;
  std::string blocker_reason;
  std::vector<std::string> fragment_ids;
  std::vector<std::string> bundle_ids;
  std::vector<std::string> workflow_path_groups;
};

bool BuildFlowValidationSummary(const FlowScriptPlan &plan,
                                FlowValidationSummary &summary);
}

#endif
