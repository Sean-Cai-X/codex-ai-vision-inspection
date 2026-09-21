#ifndef CXPARSER_EXT_PARSER_DISPATCH_DRIVER_H
#define CXPARSER_EXT_PARSER_DISPATCH_DRIVER_H

#include <string>
#include <vector>

#include "../runtime/cxscript_runtime_types.h"
#include "../pipeline/parser_task_types.h"
#include "../pipeline/parser_unified_entry.h"

namespace cxparser_ext
{
struct ParserDispatchRequest
{
  std::string script_type;
  std::string layer;
  std::string module;
  std::string integration;
  std::string case_id;
  std::string mode;
  std::string route;
  std::string trace_id;
  std::string script_path;
  std::string script_runtime_mode = "lightweight";
  bool report_on = false;
};

struct ParserDispatchResult
{
  bool success = false;
  bool build_requested = false;
  bool run_requested = false;
  bool skipped = false;
  std::string module;
  std::string layer;
  std::string case_id;
  std::string status;
  std::vector<std::string> lines;
  CxscriptIdentity identity;
  CxscriptExecutionContext context;
  CxscriptRuntimeReport report;
  ParserMainThreadTick tick;
  ParserReplaySummary replay;
  std::vector<MultimodalSlice> multimodal_slices;
  std::vector<OperationAtom> operation_atoms;
};

bool RunDispatchRequest(const ParserDispatchRequest &request,
                        ParserDispatchResult &result);
}

#endif
