#ifndef CXPARSER_EXT_PARSER_IR_CLANG_BRIDGE_H
#define CXPARSER_EXT_PARSER_IR_CLANG_BRIDGE_H

#include "../adapters/clang/clang_types.h"
#include "parser_analysis_bridge_types.h"
#include "parser_cxscript_flow.h"

namespace cxparser_ext
{
class ParserIrClangBridge
{
public:
  bool BuildSchemaBridge(const ApiSchema &schema,
                         ParserAnalysisBridgeResult &bridge_result) const;

  bool MatchFlow(const CxScriptFlow &flow,
                 const ApiSchema &schema,
                 ParserAnalysisBridgeResult &bridge_result) const;
};
}

#endif
