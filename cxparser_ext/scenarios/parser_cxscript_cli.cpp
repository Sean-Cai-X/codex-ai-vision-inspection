#include "../debug/cxscript_debug_embedded_runner.h"

#include <iostream>
#include <string>

namespace
{
void PrintUsage()
{
  std::cout
    << "cxparser_ext_cxscript_cli --script <path> [--working-directory <path>]\n"
    << "\n"
    << "This CLI is intentionally a thin shell over cxparser_ext debug layer.\n"
    << "It does not own business runtime routing, process orchestration, or GUI logic.\n";
}
}

int main(int argc, char** argv)
{
  cxparser_ext::debug::EmbeddedDebugRunRequest request;

  for (int i = 1; i < argc; ++i)
  {
    const std::string arg = argv[i];
    if ((arg == "--help") || (arg == "-h"))
    {
      PrintUsage();
      return 0;
    }
    if (arg == "--script" && i + 1 < argc)
    {
      request.script_path = argv[++i];
      continue;
    }
    if (arg == "--working-directory" && i + 1 < argc)
    {
      request.working_directory = argv[++i];
      continue;
    }

    std::cerr << "[CXPARSER_EXT_DEBUG] unknown or incomplete argument: "
              << arg << "\n";
    PrintUsage();
    return 1;
  }

  request.capture_structured_log = true;
  request.enable_line_view = true;
  request.enable_statement_view = true;
  request.enable_object_assignment = true;
  request.enable_method_trace = true;
  request.enable_return_object_trace = true;

  const cxparser_ext::debug::EmbeddedDebugRunResult result =
    cxparser_ext::debug::RunCxScriptDebugEmbedded(request);

  std::cout << result.raw_log;
  if (!result.ok && !result.reason.empty())
    std::cerr << "[CXPARSER_EXT_DEBUG] " << result.reason << "\n";

  return result.ok ? 0 : 1;
}