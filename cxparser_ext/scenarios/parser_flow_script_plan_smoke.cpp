#include "../catalog/parser_flow_script_catalog.h"

#include <iostream>

namespace
{
bool Check(bool condition, const char *message)
{
  if (!condition)
  {
    std::cerr << "[parser_flow_script_plan_smoke] " << message << std::endl;
    return false;
  }

  return true;
}
}

int main()
{
  cxparser_ext::ParserFlowScriptCatalog catalog("rag_script_cases/flow_scripts");

  cxparser_ext::FlowScriptPlan smoke_plan;
  if (!Check(catalog.ResolvePlan("smoke",
                                 "cxcloud",
                                 "bulk_create_render_release",
                                 smoke_plan),
             "cxcloud smoke plan should resolve"))
  {
    return 1;
  }

  if (!Check(smoke_plan.inputs.size() == 3, "cxcloud smoke input count mismatch"))
  {
    return 1;
  }

  if (!Check(smoke_plan.steps.size() == 3, "cxcloud smoke step count mismatch"))
  {
    return 1;
  }

  if (!Check(smoke_plan.outputs.size() == 2, "cxcloud smoke output count mismatch"))
  {
    return 1;
  }

  if (!Check(smoke_plan.steps[0].entry_name == "cxcloud.bulk_create", "first step entry mismatch"))
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

  if (!Check(scenario_plan.steps.size() >= 4, "scenario should contain multiple steps"))
  {
    return 1;
  }

  if (!Check(!scenario_plan.checks.empty(), "scenario should contain checks"))
  {
    return 1;
  }

  std::cout << "[parser_flow_script_plan_smoke]"
            << " smoke_inputs=" << smoke_plan.inputs.size()
            << " smoke_steps=" << smoke_plan.steps.size()
            << " scenario_checks=" << scenario_plan.checks.size()
            << std::endl;
  std::cout << "[parser_flow_script_plan_smoke] ok" << std::endl;
  return 0;
}
