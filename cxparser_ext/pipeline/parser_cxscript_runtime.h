#ifndef CXPARSER_EXT_PARSER_CXSCRIPT_RUNTIME_H
#define CXPARSER_EXT_PARSER_CXSCRIPT_RUNTIME_H

#include <string>

#include "parser_cxscript_flow.h"
#include "parser_cxscript_types.h"
#include "parser_test_driver.h"

namespace cxparser_ext
{
enum CxScriptRuntimeMode
{
  cxsrm_lightweight,
  cxsrm_debug
};

class ParserCxScriptRuntime
{
public:
  void SetExecutionMode(CxScriptRuntimeMode mode) { execution_mode_ = mode; }
  CxScriptRuntimeMode GetExecutionMode() const { return execution_mode_; }

  bool ParseScriptFlow(const std::string &script_name,
                       const std::string &script_text,
                       CxScriptExecutionContext &context,
                       CxScriptFlow &flow,
                       CxScriptParseError &parse_error,
                       std::string &error_message);

  bool BuildExecutionPreview(const std::string &script_name,
                             const std::string &script_text,
                             CxScriptExecutionResult &result);

  bool ExecuteScriptText(const std::string &script_name,
                         const std::string &script_text,
                         CxScriptExecutionResult &result);

  bool ExecuteScriptFile(const std::string &script_path,
                         CxScriptExecutionResult &result);

  bool BuildDebugView(const CxScriptExecutionResult &result,
                      CxScriptDebugView &debug_view) const;

  bool QueryDebugByStep(const CxScriptDebugView &debug_view,
                        const std::string &step_name,
                        CxScriptDebugQueryResult &query_result) const;

  bool QueryDebugByLine(const CxScriptDebugView &debug_view,
                        int line,
                        CxScriptDebugQueryResult &query_result) const;

  bool QueryDebugBySequence(const CxScriptDebugView &debug_view,
                            int sequence,
                            CxScriptDebugQueryResult &query_result) const;

  bool QueryDebugByBreakpoint(const CxScriptDebugView &debug_view,
                              const std::string &breakpoint_name,
                              CxScriptDebugQueryResult &query_result) const;

  bool QueryDebugByCheckpoint(const CxScriptDebugView &debug_view,
                              const std::string &checkpoint_name,
                              CxScriptDebugQueryResult &query_result) const;

  bool BuildBuiltinFunctionFragments(std::vector<CxScriptFunctionFragment> &fragments) const;

  bool BuildBuiltinFlowSnippets(std::vector<CxScriptFlowSnippet> &snippets) const;

  bool BuildBuiltinCStyleSnippets(std::vector<CxScriptCStyleSnippet> &snippets) const;

  bool BuildCurrentMainlineReadiness(CxScriptMainlineReadiness &readiness) const;

  bool BuildBuiltinFunctionFragmentReport(std::vector<std::string> &lines) const;

private:
  bool ParseScriptText(const std::string &script_name,
                       const std::string &script_text,
                       CxScriptExecutionContext &context,
                       CxScriptFlow &flow,
                       CxScriptParseError &parse_error,
                       std::string &error_message);

  bool BuildTestRequest(const CxScriptExecutionContext &context,
                        const CxScriptFlow &flow,
                        ParserTestRequest &request,
                        std::string &error_message);

  bool ConvertTestResult(const CxScriptExecutionContext &context,
                         const ParserTestRunResult &test_result,
                         CxScriptExecutionResult &script_result);

  CxScriptRuntimeMode execution_mode_ = cxsrm_lightweight;
  ParserTestDriver test_driver_;
};
}

#endif
