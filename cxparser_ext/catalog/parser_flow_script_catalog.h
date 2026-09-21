#ifndef CXPARSER_EXT_PARSER_FLOW_SCRIPT_CATALOG_H
#define CXPARSER_EXT_PARSER_FLOW_SCRIPT_CATALOG_H

#include "../meta/parser_pseudocode_types.h"

#include <string>
#include <vector>

namespace cxparser_ext
{
class ParserFlowScriptCatalog
{
public:
  explicit ParserFlowScriptCatalog(const std::string &root_dir = std::string());

  void SetRootDir(const std::string &root_dir);
  const std::string &GetRootDir() const;

  bool ListScripts(std::vector<FlowScriptSpec> &scripts) const;
  bool ResolveScript(const std::string &layer,
                     const std::string &module,
                     const std::string &function_name,
                     FlowScriptSpec &script) const;
  bool BuildPlan(const FlowScriptSpec &script, FlowScriptPlan &plan) const;
  bool ResolvePlan(const std::string &layer,
                   const std::string &module,
                   const std::string &function_name,
                   FlowScriptPlan &plan) const;

private:
  std::string root_dir_;
};
}

#endif
