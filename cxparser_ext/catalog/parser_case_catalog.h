#ifndef CXPARSER_EXT_PARSER_CASE_CATALOG_H
#define CXPARSER_EXT_PARSER_CASE_CATALOG_H

#include <string>

namespace cxparser_ext
{
struct ParserDispatchRequest;

struct ParserDispatchCaseSpec
{
  std::string layer;
  std::string module;
  std::string case_id;
  std::string script_path;
  std::string route;
  std::string task_subtype;
  std::string target_class;
  std::string target_method;
  std::string script_text;
  std::string state;
  bool active_runtime = false;
  bool replay_after_run = false;
  bool requires_image_probe_binding = false;
  bool requires_geometry_contract_binding = false;
  bool uses_cxcore_contract_mainline = false;
  bool uses_torch_contract_mainline = false;
  bool uses_mlpack_baseline_mainline = false;
};

bool ResolveDispatchCase(const ParserDispatchRequest &request,
                         ParserDispatchCaseSpec &spec);
}

#endif
