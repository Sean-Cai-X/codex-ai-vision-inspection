#include <iostream>
#include <string>

#include "../adapters/clang/clang_types.h"
#include "../pipeline/parser_analysis_bridge_types.h"
#include "../pipeline/parser_cxscript_runtime.h"
#include "../pipeline/parser_ir_clang_bridge.h"

namespace
{
bool RunSchemaBridgeCase()
{
  cxparser_ext::ApiSchema schema;
  schema.module_name = "torch";

  cxparser_ext::ClangClassInfo cls;
  cls.name = "Infer";
  cls.qualified_name = "torch::Infer";

  cxparser_ext::ClangMethodInfo method;
  method.name = "infer_min";
  method.qualified_name = "torch::Infer::infer_min";
  cls.methods.push_back(method);
  schema.classes.push_back(cls);

  cxparser_ext::ParserIrClangBridge bridge;
  cxparser_ext::ParserAnalysisBridgeResult result;
  if (!bridge.BuildSchemaBridge(schema, result))
  {
    std::cerr << "[FAIL] clang schema bridge failed\n";
    return false;
  }

  if (result.bridge_name != "clang" ||
      result.steps.size() != 1 ||
      result.calls.size() != 1 ||
      result.points.size() != 1 ||
      result.steps[0].matched_symbol != "torch::Infer" ||
      result.calls[0].matched_symbol != "torch::Infer::infer_min" ||
      result.points[0].point_kind != "type_decl" ||
      result.points[0].related_symbol != "torch::Infer" ||
      !result.points[0].matched)
  {
    std::cerr << "[FAIL] clang schema bridge result mismatch\n";
    return false;
  }

  std::cout << "[PASS] clang schema bridge step=" << result.steps[0].matched_symbol
            << " call=" << result.calls[0].matched_symbol
            << " type=" << result.points[0].related_symbol << "\n";
  return true;
}

bool RunFlowBridgeCase()
{
  cxparser_ext::ParserCxScriptRuntime runtime;
  cxparser_ext::CxScriptExecutionContext context;
  cxparser_ext::CxScriptFlow flow;
  cxparser_ext::CxScriptParseError parse_error;
  std::string error_message;

  const char *script =
    "kind=integration\n"
    "layer=infer\n"
    "module=torch\n"
    "mode=run\n"
    "type Infer;\n"
    "step Infer {\n"
    "Infer infer;\n"
    "call infer_min(frame);\n"
    "}\n";

  if (!runtime.ParseScriptFlow("clang_bridge.cxs", script, context, flow, parse_error, error_message))
  {
    std::cerr << "[FAIL] clang flow parse failed: " << error_message << "\n";
    return false;
  }

  cxparser_ext::ApiSchema schema;
  schema.module_name = "torch";

  cxparser_ext::ClangClassInfo cls;
  cls.name = "Infer";
  cls.qualified_name = "torch::Infer";

  cxparser_ext::ClangMethodInfo method;
  method.name = "infer_min";
  method.qualified_name = "torch::Infer::infer_min";
  cls.methods.push_back(method);
  schema.classes.push_back(cls);

  cxparser_ext::ParserIrClangBridge bridge;
  cxparser_ext::ParserAnalysisBridgeResult result;
  if (!bridge.MatchFlow(flow, schema, result))
  {
    std::cerr << "[FAIL] clang flow bridge failed\n";
    return false;
  }

  if (result.bridge_name != "clang" ||
      result.calls.size() != 1 ||
      !result.calls[0].matched ||
      result.calls[0].matched_symbol != "torch::Infer::infer_min" ||
      result.steps.size() != 1 ||
      !result.steps[0].matched ||
      result.steps[0].matched_symbol != "torch::Infer" ||
      result.points.size() != 1 ||
      result.points[0].point_name != "Infer" ||
      result.points[0].step_id <= 0 ||
      result.points[0].frame_id < 0 ||
      result.points[0].span.line_begin <= 0 ||
      !result.points[0].matched)
  {
    std::cerr << "[FAIL] clang flow bridge result mismatch\n";
    return false;
  }

  std::cout << "[PASS] clang flow bridge call=" << result.calls[0].matched_symbol
            << " step=" << result.steps[0].matched_symbol
            << " type=" << result.points[0].related_symbol << "\n";
  return true;
}
}

int main()
{
  if (!RunSchemaBridgeCase())
    return 1;
  if (!RunFlowBridgeCase())
    return 1;

  std::cout << "[PASS] parser_ir_clang_bridge_smoke\n";
  return 0;
}
