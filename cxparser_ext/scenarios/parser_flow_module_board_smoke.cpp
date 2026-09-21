#include "../catalog/parser_flow_script_catalog.h"
#include "../validation/parser_flow_module_board.h"
#include "../validation/parser_flow_validation_summary.h"

#include <iostream>
#include <vector>

namespace
{
bool Check(bool condition, const char *message)
{
  if (!condition)
  {
    std::cerr << "[parser_flow_module_board_smoke] " << message << std::endl;
    return false;
  }

  return true;
}

bool AppendSummary(cxparser_ext::ParserFlowScriptCatalog &catalog,
                   const char *layer,
                   const char *module,
                   const char *function_name,
                   std::vector<cxparser_ext::FlowValidationSummary> &summaries)
{
  cxparser_ext::FlowScriptPlan plan;
  if (!catalog.ResolvePlan(layer, module, function_name, plan))
  {
    return false;
  }

  cxparser_ext::FlowValidationSummary summary;
  if (!cxparser_ext::BuildFlowValidationSummary(plan, summary))
  {
    return false;
  }

  summaries.push_back(summary);
  return true;
}
}

int main()
{
  cxparser_ext::ParserFlowScriptCatalog catalog("rag_script_cases/flow_scripts");
  std::vector<cxparser_ext::FlowValidationSummary> summaries;

  if (!Check(AppendSummary(catalog,
                           "smoke",
                           "cxgeom",
                           "bulk_create_presentation_release",
                           summaries),
             "geometry smoke summary should resolve"))
  {
    return 1;
  }

  if (!Check(AppendSummary(catalog,
                           "smoke",
                           "cxcloud",
                           "bulk_create_render_release",
                           summaries),
             "cloud smoke summary should resolve"))
  {
    return 1;
  }

  if (!Check(AppendSummary(catalog,
                           "feature",
                           "cxgeom",
                           "scene_mapping_publish",
                           summaries),
             "geometry feature summary should resolve"))
  {
    return 1;
  }

  if (!Check(AppendSummary(catalog,
                           "scenario",
                           "cxcore",
                           "mixed_scene_refresh_latency_watch",
                           summaries),
             "cxcore scenario summary should resolve"))
  {
    return 1;
  }

  cxparser_ext::FlowModuleBoard board;
  if (!Check(cxparser_ext::BuildFlowModuleBoard("cxgeom_cxcloud_to_cxcore", summaries, board),
             "board build failed"))
  {
    return 1;
  }

  if (!Check(board.ready_count == 4, "all tracked flows should be ready"))
  {
    return 1;
  }

  if (!Check(board.blocked_count == 0, "tracked flows should not be blocked"))
  {
    return 1;
  }

  if (!Check(board.geometry_bulk_ready, "board should track geometry bulk"))
  {
    return 1;
  }

  if (!Check(board.cloud_bulk_ready, "board should track cloud bulk"))
  {
    return 1;
  }

  if (!Check(board.scene_mapping_ready, "board should track scene mapping"))
  {
    return 1;
  }

  if (!Check(board.refresh_watch_ready, "board should track refresh watch"))
  {
    return 1;
  }

  if (!Check(board.latency_watch_ready, "board should track latency watch"))
  {
    return 1;
  }

  if (!Check(board.links.size() == summaries.size(), "board should expose one link per flow"))
  {
    return 1;
  }

  if (!Check(!board.covered_bundle_ids.empty(), "board should aggregate covered bundles"))
  {
    return 1;
  }

  if (!Check(!board.covered_bundle_groups.empty(),
             "board should aggregate covered bundle groups"))
  {
    return 1;
  }

  if (!Check(!board.links.front().smoke_targets.empty(), "first link should expose smoke targets"))
  {
    return 1;
  }

  if (!Check(!board.links.front().capability_points.empty(), "first link should expose capability points"))
  {
    return 1;
  }

  if (!Check(!board.links.front().fragment_ids.empty(), "first link should expose cxscript fragments"))
  {
    return 1;
  }

  if (!Check(!board.links.front().bundle_ids.empty(), "first link should expose cxscript bundles"))
  {
    return 1;
  }

  const std::string report = cxparser_ext::BuildFlowModuleBoardReport(board);
  if (!Check(report.find("[FLOW-BOARD]") != std::string::npos, "report header missing"))
  {
    return 1;
  }

  if (!Check(report.find("[SMOKE]") != std::string::npos, "report should include smoke mapping"))
  {
    return 1;
  }

  if (!Check(report.find("[CAPABILITY]") != std::string::npos, "report should include capability mapping"))
  {
    return 1;
  }

  if (!Check(report.find("[BUNDLE]") != std::string::npos, "report should include bundle mapping"))
  {
    return 1;
  }

  if (!Check(report.find("[COVERED-BUNDLE]") != std::string::npos,
             "report should include covered bundle summary"))
  {
    return 1;
  }

  if (!Check(report.find("[COVERED-GROUP]") != std::string::npos,
             "report should include covered bundle group summary"))
  {
    return 1;
  }

  if (!Check(report.find("[FRAGMENT]") != std::string::npos, "report should include fragment mapping"))
  {
    return 1;
  }

  std::cout << "[parser_flow_module_board_smoke]"
            << " ready=" << board.ready_count
            << " blocked=" << board.blocked_count
            << " focus_points=" << board.focus_points.size()
            << " links=" << board.links.size()
            << std::endl;
  std::cout << report << std::endl;
  std::cout << "[parser_flow_module_board_smoke] ok" << std::endl;
  return 0;
}
