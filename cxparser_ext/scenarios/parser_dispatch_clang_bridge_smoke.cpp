#include <iostream>

#include "../drivers/parser_dispatch_driver.h"

namespace
{
bool RunVideoClangBridgeCase()
{
  cxparser_ext::ParserDispatchRequest request;
  request.script_type = "integration";
  request.integration = "video";
  request.layer = "infer";
  request.case_id = "video_frame_chain";
  request.mode = "build-run";

  cxparser_ext::ParserDispatchResult result;
  if (!cxparser_ext::RunDispatchRequest(request, result))
  {
    std::cerr << "[FAIL] video clang bridge dispatch failed\n";
    return false;
  }

  if (!result.skipped ||
      result.report.script_origin != "file" ||
      result.identity.file_path.empty())
  {
    std::cerr << "[FAIL] video clang bridge case should stay skipped but file-backed\n";
    return false;
  }

  bool saw_bridge_point = false;
  for (size_t i = 0; i < result.report.bridge_point_lines.size(); ++i)
  {
    if (result.report.bridge_point_lines[i].find(
          "clang point type_decl VideoFrame -> torch::VideoFrame [matched]") != std::string::npos)
    {
      saw_bridge_point = true;
      break;
    }
  }

  if (result.report.bridge_summary.find("clang points=") == std::string::npos ||
      result.report.bridge_point_count < 1 ||
      result.report.bridge_matched_call_count < 1 ||
      !saw_bridge_point)
  {
    std::cerr << "[FAIL] video clang bridge report should expose matched type point"
              << " summary=" << result.report.bridge_summary;
    for (size_t i = 0; i < result.report.bridge_point_lines.size(); ++i)
      std::cerr << " point=" << result.report.bridge_point_lines[i];
    std::cerr << "\n";
    return false;
  }

  std::cout << "[PASS] dispatch clang bridge summary=" << result.report.bridge_summary
            << " file=" << result.identity.file_path << "\n";
  return true;
}
}

int main()
{
  if (!RunVideoClangBridgeCase())
    return 1;

  std::cout << "[PASS] parser_dispatch_clang_bridge_smoke\n";
  return 0;
}
