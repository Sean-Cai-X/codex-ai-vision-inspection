#ifndef CXPARSER_EXT_CXSCRIPT_C_FRONTEND_H
#define CXPARSER_EXT_CXSCRIPT_C_FRONTEND_H

#include <string>
#include <vector>

namespace cxparser_ext
{
enum CxscriptIrKind
{
  cxik_unknown,
  cxik_stmt,
  cxik_block_begin,
  cxik_block_end,
  cxik_if_begin,
  cxik_for_begin,
  cxik_check_begin,
  cxik_scenario_begin,
  cxik_report_begin
};

struct CxscriptIrOp
{
  CxscriptIrKind kind = cxik_unknown;
  std::string stage;
  std::string text;
  int block_depth = 0;
};

struct CxscriptIrProgram
{
  bool success = false;
  std::string error_message;
  std::vector<CxscriptIrOp> ops;
};

bool BuildCxscriptLinearIr(const std::string &source_text,
                           CxscriptIrProgram &program);
void RenderCxscriptIrCompileText(const CxscriptIrProgram &program,
                                 std::string &compile_text);
}

#endif
