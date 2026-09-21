#include <iostream>
#include <algorithm>
#include <string>

#include "../cxparser_ext/drivers/parser_dispatch_driver.h"

namespace
{
bool ReadValue(int argc,
               char **argv,
               int &index,
               std::string &value)
{
  if (index + 1 >= argc)
    return false;
  value = argv[++index];
  return true;
}

bool IsKnownCxScriptLayer(const std::string &value)
{
  return value == "feature" ||
         value == "smoke" ||
         value == "scenario" ||
         value == "train" ||
         value == "infer" ||
         value == "score" ||
         value == "contract";
}

bool IsKnownCxScriptModule(const std::string &value)
{
  return value == "cxcore" ||
         value == "ensmallen_layer" ||
         value == "mlpack" ||
         value == "torch_module" ||
         value == "rag";
}

void NormalizeModuleLayerArguments(cxparser_ext::ParserDispatchRequest &request)
{
  if (IsKnownCxScriptModule(request.layer) && IsKnownCxScriptLayer(request.module))
    std::swap(request.layer, request.module);
}
}

int main(int argc, char **argv)
{
  cxparser_ext::ParserDispatchRequest request;
  request.mode = "build-run";

  for (int i = 1; i < argc; ++i)
  {
    const std::string arg = argv[i];
    if (arg == "--script-type" || arg == "--kind")
    {
      if (!ReadValue(argc, argv, i, request.script_type))
        return 2;
    }
    else if (arg == "--layer")
    {
      if (!ReadValue(argc, argv, i, request.layer))
        return 2;
    }
    else if (arg == "--module")
    {
      if (!ReadValue(argc, argv, i, request.module))
        return 2;
    }
    else if (arg == "--integration")
    {
      if (!ReadValue(argc, argv, i, request.integration))
        return 2;
    }
    else if (arg == "--case")
    {
      if (!ReadValue(argc, argv, i, request.case_id))
        return 2;
    }
    else if (arg == "--mode")
    {
      if (!ReadValue(argc, argv, i, request.mode))
        return 2;
    }
    else if (arg == "--route")
    {
      if (!ReadValue(argc, argv, i, request.route))
        return 2;
    }
    else if (arg == "--trace-id")
    {
      if (!ReadValue(argc, argv, i, request.trace_id))
        return 2;
    }
    else if (arg == "--script")
    {
      if (!ReadValue(argc, argv, i, request.script_path))
        return 2;
    }
    else if (arg == "--report")
    {
      std::string report_value;
      if (!ReadValue(argc, argv, i, report_value))
        return 2;
      request.report_on = (report_value == "on");
    }
  }

  if (request.script_type.empty())
    request.script_type = request.integration.empty() ? "module" : "integration";

  NormalizeModuleLayerArguments(request);

  if (request.layer.empty() || request.case_id.empty())
  {
    std::cerr << "[FAIL] missing required args --layer --case\n";
    return 2;
  }

  if (request.script_type == "module" && request.module.empty())
  {
    std::cerr << "[FAIL] module cxscript requires --module\n";
    return 2;
  }

  if (request.script_type == "integration" && request.integration.empty())
  {
    std::cerr << "[FAIL] integration cxscript requires --integration\n";
    return 2;
  }

  cxparser_ext::ParserDispatchResult result;
  if (!cxparser_ext::RunDispatchRequest(request, result))
  {
    for (size_t i = 0; i < result.lines.size(); ++i)
      std::cerr << result.lines[i] << "\n";
    if ((request.report_on || result.report.report_requested) && !result.report.status.empty())
    {
      std::cerr << "[REPORT] status=" << result.report.status
                << " script_type=" << result.report.script_type
                << " module=" << result.report.module_name
                << " integration=" << result.report.integration_name
                << " layer=" << result.report.layer
                << " case=" << result.report.case_id
                << " build=" << (result.build_requested ? "on" : "off")
                << " run=" << (result.run_requested ? "on" : "off")
                << " skipped=" << (result.report.skipped ? "true" : "false")
                << " origin=" << result.report.script_origin
                << " style=" << result.report.flow_profile.script_style
                << " prepare=" << (result.report.flow_profile.has_prepare ? "true" : "false")
                << " action=" << (result.report.flow_profile.has_action ? "true" : "false")
                << " check=" << (result.report.flow_profile.has_check ? "true" : "false")
                << " report=" << (result.report.flow_profile.has_report ? "true" : "false")
                << " exec_kind=" << result.report.layer_profile.execution_text_kind
                << " bridge_safe=" << (result.report.layer_profile.bridge_exec_safe ? "true" : "false")
                << " bridge_subset=" << result.report.layer_profile.bridge_exec_subset
                << " bridge_reason=" << result.report.layer_profile.bridge_exec_reason
                << " fallback_reason=" << result.report.layer_profile.fallback_reason
                << " error_kind=" << result.report.error_kind
                << " error_code=" << result.report.parser_error_code
                << " error_pos=" << result.report.parser_error_pos
                << " error_token=" << result.report.parser_error_token
                << " ir_valid=" << (result.report.ir_valid ? "true" : "false")
                << " ir_ops=" << result.report.ir_op_count
                << " ir_stmts=" << result.report.ir_stmt_count
                << " ir_blocks=" << result.report.ir_block_count
                << " ir_error=" << result.report.ir_error_message
                << "\n";
    }
    return 1;
  }

  for (size_t i = 0; i < result.lines.size(); ++i)
    std::cout << result.lines[i] << "\n";

  if (request.report_on || result.report.report_requested)
  {
    std::cout << "[REPORT] status=" << result.report.status
              << " script_type=" << result.report.script_type
              << " module=" << result.report.module_name
              << " integration=" << result.report.integration_name
              << " layer=" << result.report.layer
              << " case=" << result.report.case_id
              << " build=" << (result.build_requested ? "on" : "off")
              << " run=" << (result.run_requested ? "on" : "off")
              << " skipped=" << (result.report.skipped ? "true" : "false")
              << " origin=" << result.report.script_origin
              << " style=" << result.report.flow_profile.script_style
              << " prepare=" << (result.report.flow_profile.has_prepare ? "true" : "false")
              << " action=" << (result.report.flow_profile.has_action ? "true" : "false")
              << " check=" << (result.report.flow_profile.has_check ? "true" : "false")
              << " report=" << (result.report.flow_profile.has_report ? "true" : "false")
              << " exec_kind=" << result.report.layer_profile.execution_text_kind
              << " bridge_safe=" << (result.report.layer_profile.bridge_exec_safe ? "true" : "false")
              << " bridge_subset=" << result.report.layer_profile.bridge_exec_subset
              << " bridge_reason=" << result.report.layer_profile.bridge_exec_reason
              << " fallback_reason=" << result.report.layer_profile.fallback_reason
              << " error_kind=" << result.report.error_kind
              << " error_code=" << result.report.parser_error_code
              << " error_pos=" << result.report.parser_error_pos
              << " error_token=" << result.report.parser_error_token
              << " ir_valid=" << (result.report.ir_valid ? "true" : "false")
              << " ir_ops=" << result.report.ir_op_count
              << " ir_stmts=" << result.report.ir_stmt_count
              << " ir_blocks=" << result.report.ir_block_count
              << " ir_error=" << result.report.ir_error_message
              << "\n";
  }

  return 0;
}