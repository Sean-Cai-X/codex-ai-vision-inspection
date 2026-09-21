#include "../catalog/parser_flow_script_catalog.h"

#include <iostream>
#include <vector>

namespace
{
bool Check(bool condition, const char *message)
{
  if (!condition)
  {
    std::cerr << "[parser_flow_script_catalog_smoke] " << message << std::endl;
    return false;
  }

  return true;
}
}

int main()
{
  cxparser_ext::ParserFlowScriptCatalog catalog("rag_script_cases/flow_scripts");
  std::vector<cxparser_ext::FlowScriptSpec> scripts;
  if (!Check(catalog.ListScripts(scripts), "catalog listing failed"))
  {
    return 1;
  }

  if (!Check(!scripts.empty(), "catalog should expose flow scripts"))
  {
    return 1;
  }

  cxparser_ext::FlowScriptSpec geometry_smoke;
  if (!Check(catalog.ResolveScript("smoke",
                                   "cxgeom",
                                   "bulk_create_presentation_release",
                                   geometry_smoke),
             "geometry smoke script should resolve"))
  {
    return 1;
  }

  if (!Check(geometry_smoke.flow_id == "smoke.cxgeom.bulk_create_presentation_release",
             "geometry flow id mismatch"))
  {
    return 1;
  }

  cxparser_ext::FlowScriptSpec train_case;
  if (!Check(catalog.ResolveScript("train",
                                   "torch_module",
                                   "minimal_image_train",
                                   train_case),
             "train script should resolve"))
  {
    return 1;
  }

  if (!Check(train_case.flow_id == "train.torch_module.minimal_image_train",
             "train flow id mismatch"))
  {
    return 1;
  }

  std::cout << "[parser_flow_script_catalog_smoke]"
            << " script_count=" << scripts.size()
            << " first_layer=" << scripts.front().layer
            << " first_module=" << scripts.front().module
            << " first_function=" << scripts.front().function_name
            << std::endl;
  std::cout << "[parser_flow_script_catalog_smoke] ok" << std::endl;
  return 0;
}
