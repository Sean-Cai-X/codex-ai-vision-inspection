#ifndef CXPARSER_EXT_PARSER_VALIDATION_TYPES_H
#define CXPARSER_EXT_PARSER_VALIDATION_TYPES_H

#include <string>
#include <vector>

namespace cxparser_ext
{
enum ValidationLevel
{
  vl_info,
  vl_warning,
  vl_error
};

struct ValidationIssue
{
  ValidationLevel level = vl_info;
  std::string code;
  std::string message;
  std::string related_symbol;
  std::string failure_stage;
  std::string expected;
  std::string actual;
};

struct ParserValidationReport
{
  bool passed = false;
  std::vector<ValidationIssue> issues;
  std::vector<std::string> matched_points;
  std::vector<std::string> mismatched_points;
};
}

#endif
