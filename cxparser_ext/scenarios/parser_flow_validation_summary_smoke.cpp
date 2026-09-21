#include "../catalog/parser_flow_script_catalog.h"
#include "../validation/parser_flow_validation_summary.h"

#include <iostream>

namespace
{
bool Check(bool condition, const char *message)
{
  if (!condition)
  {
    std::cerr << "[parser_flow_validation_summary_smoke] " << message << std::endl;
    return false;
  }

  return true;
}
}

int main()
{
  cxparser_ext::ParserFlowScriptCatalog catalog("rag_script_cases/flow_scripts");

  cxparser_ext::FlowScriptPlan geometry_plan;
  if (!Check(catalog.ResolvePlan("feature",
                                 "cxgeom",
                                 "scene_mapping_publish",
                                 geometry_plan),
             "geometry feature plan should resolve"))
  {
    return 1;
  }

  cxparser_ext::FlowValidationSummary geometry_summary;
  if (!Check(cxparser_ext::BuildFlowValidationSummary(geometry_plan, geometry_summary),
             "geometry summary build failed"))
  {
    return 1;
  }

  if (!Check(geometry_summary.requires_scene_mapping, "geometry summary should require scene mapping"))
  {
    return 1;
  }

  if (!Check(geometry_summary.publish_path_involved, "geometry summary should involve publish path"))
  {
    return 1;
  }

  if (!Check(!geometry_summary.fragment_ids.empty(),
             "geometry summary should expose cxscript fragments"))
  {
    return 1;
  }

  if (!Check(!geometry_summary.bundle_ids.empty(),
             "geometry summary should expose cxscript bundles"))
  {
    return 1;
  }

  if (!Check(!geometry_summary.workflow_path_groups.empty(),
             "geometry summary should expose workflow path groups"))
  {
    return 1;
  }

  if (!Check(geometry_summary.ready_for_current_module_validation, "geometry summary should be ready"))
  {
    return 1;
  }

  cxparser_ext::FlowScriptPlan scenario_plan;
  if (!Check(catalog.ResolvePlan("scenario",
                                 "cxcore",
                                 "mixed_scene_refresh_latency_watch",
                                 scenario_plan),
             "cxcore scenario plan should resolve"))
  {
    return 1;
  }

  cxparser_ext::FlowValidationSummary scenario_summary;
  if (!Check(cxparser_ext::BuildFlowValidationSummary(scenario_plan, scenario_summary),
             "scenario summary build failed"))
  {
    return 1;
  }

  if (!Check(scenario_summary.requires_geometry_bulk, "scenario should require geometry bulk"))
  {
    return 1;
  }

  if (!Check(scenario_summary.requires_cloud_bulk, "scenario should require cloud bulk"))
  {
    return 1;
  }

  if (!Check(scenario_summary.requires_refresh_watch, "scenario should require refresh watch"))
  {
    return 1;
  }

  if (!Check(scenario_summary.requires_latency_watch, "scenario should require latency watch"))
  {
    return 1;
  }

  if (!Check(!scenario_summary.fragment_ids.empty(),
             "scenario summary should expose cxscript fragments"))
  {
    return 1;
  }

  if (!Check(!scenario_summary.bundle_ids.empty(),
             "scenario summary should expose cxscript bundles"))
  {
    return 1;
  }

  if (!Check(!scenario_summary.workflow_path_groups.empty(),
             "scenario summary should expose workflow path groups"))
  {
    return 1;
  }

  std::cout << "[parser_flow_validation_summary_smoke]"
            << " geometry_ready=" << geometry_summary.ready_for_current_module_validation
            << " scenario_steps=" << scenario_summary.step_count
            << " scenario_checks=" << scenario_summary.check_count
            << std::endl;
  std::cout << "[parser_flow_validation_summary_smoke] ok" << std::endl;
  return 0;
}
