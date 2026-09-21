#include <iostream>

#include "../runtime/cxscript_c_frontend.h"

namespace
{
bool RunValidFlowCase()
{
  const std::string script_text =
    "scenario {\n"
    "  ImageProbe probe;\n"
    "  input_path=\"image.png\"; // prepare path\n"
    "  probe.Load(input_path);\n"
    "  prepare_ok=1;\n"
    "  if (prepare_ok) {\n"
    "    probe.Detect(0.8);\n"
    "    action_ok=1;\n"
    "  }\n"
    "  check {\n"
    "    score=probe.Score();\n"
    "    check_ok=score>0.0;\n"
    "  }\n"
    "  report {\n"
    "    report_text=\"done // keep\";\n"
    "  }\n"
    "}\n";

  cxparser_ext::CxscriptIrProgram program;
  if (!cxparser_ext::BuildCxscriptLinearIr(script_text, program))
  {
    std::cerr << "[FAIL] valid flow case should build IR: " << program.error_message << "\n";
    return false;
  }

  bool saw_scenario = false;
  bool saw_if = false;
  bool saw_check_stmt = false;
  bool saw_report_stmt = false;
  for (size_t i = 0; i < program.ops.size(); ++i)
  {
    const cxparser_ext::CxscriptIrOp &op = program.ops[i];
    if (op.kind == cxparser_ext::cxik_scenario_begin)
      saw_scenario = true;
    if (op.kind == cxparser_ext::cxik_if_begin)
      saw_if = true;
    if (op.kind == cxparser_ext::cxik_stmt && op.stage == "check")
      saw_check_stmt = true;
    if (op.kind == cxparser_ext::cxik_stmt && op.stage == "report")
      saw_report_stmt = true;
  }

  if (!saw_scenario || !saw_if || !saw_check_stmt || !saw_report_stmt)
  {
    std::cerr << "[FAIL] valid flow case missing expected IR markers\n";
    return false;
  }

  std::string compile_text;
  cxparser_ext::RenderCxscriptIrCompileText(program, compile_text);
  if (compile_text.find("double action_ok=0;") == std::string::npos ||
      compile_text.find("double check_ok=0;") == std::string::npos)
  {
    std::cerr << "[FAIL] valid flow case should bridge undeclared flow vars\n";
    std::cerr << compile_text << "\n";
    return false;
  }

  return true;
}

bool RunMissingSemicolonCase()
{
  const std::string script_text =
    "scenario {\n"
    "  prepare_ok=1\n"
    "}\n";

  cxparser_ext::CxscriptIrProgram program;
  if (cxparser_ext::BuildCxscriptLinearIr(script_text, program))
  {
    std::cerr << "[FAIL] missing semicolon case should fail\n";
    return false;
  }

  if (program.error_message.find("semicolon") == std::string::npos)
  {
    std::cerr << "[FAIL] missing semicolon case should report semicolon error\n";
    return false;
  }

  return true;
}

bool RunMissingBlockCase()
{
  const std::string script_text =
    "if (prepare_ok)\n"
    "  action_ok=1;\n";

  cxparser_ext::CxscriptIrProgram program;
  if (cxparser_ext::BuildCxscriptLinearIr(script_text, program))
  {
    std::cerr << "[FAIL] missing block case should fail\n";
    return false;
  }

  if (program.error_message.find("must use a block") == std::string::npos)
  {
    std::cerr << "[FAIL] missing block case should report block error\n";
    return false;
  }

  return true;
}

bool RunNumericIfBridgeCase()
{
  const std::string script_text =
    "scenario {\n"
    "  double a=1;\n"
    "  double d=0;\n"
    "  if (a>0) {\n"
    "    d=d+1;\n"
    "  }\n"
    "  d;\n"
    "}\n";

  cxparser_ext::CxscriptIrProgram program;
  if (!cxparser_ext::BuildCxscriptLinearIr(script_text, program))
  {
    std::cerr << "[FAIL] numeric if case should build IR: " << program.error_message << "\n";
    return false;
  }

  std::string compile_text;
  cxparser_ext::RenderCxscriptIrCompileText(program, compile_text);
  if (compile_text.find("double a=1;double d=0;if(a>0){d=d+1;}d;") == std::string::npos)
  {
    std::cerr << "[FAIL] numeric if case should render compact bridge text\n";
    std::cerr << compile_text << "\n";
    return false;
  }

  return true;
}

bool RunRegistrationStageCase()
{
  const std::string script_text =
    "scenario {\n"
    "  register_class(\"ImageProbe\");\n"
    "  register_fun(\"ImageProbe\", \"Score\");\n"
    "  check(true);\n"
    "}\n";

  cxparser_ext::CxscriptIrProgram program;
  if (!cxparser_ext::BuildCxscriptLinearIr(script_text, program))
  {
    std::cerr << "[FAIL] registration stage case should build IR: " << program.error_message << "\n";
    return false;
  }

  int registration_stmt_count = 0;
  bool saw_check_stmt = false;
  for (size_t i = 0; i < program.ops.size(); ++i)
  {
    const cxparser_ext::CxscriptIrOp &op = program.ops[i];
    if (op.kind == cxparser_ext::cxik_stmt && op.stage == "registration")
      ++registration_stmt_count;
    if (op.kind == cxparser_ext::cxik_stmt && op.stage == "check")
      saw_check_stmt = true;
  }

  if (registration_stmt_count != 2 || !saw_check_stmt)
  {
    std::cerr << "[FAIL] registration stage case missing expected stage markers\n";
    return false;
  }

  return true;
}
}

int main()
{
  if (!RunValidFlowCase() || !RunMissingSemicolonCase() || !RunMissingBlockCase() ||
      !RunNumericIfBridgeCase() || !RunRegistrationStageCase())
    return 1;

  std::cout << "[PASS] cxscript_c_frontend smoke comments+semicolon+blocks\n";
  return 0;
}
