#ifndef CXPARSER_EXT_PARSER_FLOW_MODULE_BOARD_H
#define CXPARSER_EXT_PARSER_FLOW_MODULE_BOARD_H

#include "parser_flow_validation_summary.h"

#include <string>
#include <vector>

namespace cxparser_ext
{
struct FlowExecutionLink
{
  std::string flow_id;
  std::vector<std::string> smoke_targets;
  std::vector<std::string> capability_points;
  std::vector<std::string> fragment_ids;
  std::vector<std::string> bundle_ids;
};

struct FlowModuleBoard
{
  std::string board_name;
  std::vector<FlowValidationSummary> flows;
  std::vector<FlowExecutionLink> links;
  int ready_count = 0;
  int blocked_count = 0;
  bool geometry_bulk_ready = false;
  bool cloud_bulk_ready = false;
  bool scene_mapping_ready = false;
  bool refresh_watch_ready = false;
  bool latency_watch_ready = false;
  std::vector<std::string> blockers;
  std::vector<std::string> focus_points;
  std::vector<std::string> covered_bundle_ids;
  std::vector<std::string> covered_bundle_groups;
};

bool BuildFlowModuleBoard(const std::string &board_name,
                          const std::vector<FlowValidationSummary> &summaries,
                          FlowModuleBoard &board);
std::string BuildFlowModuleBoardReport(const FlowModuleBoard &board);
}

#endif
