#ifndef CXPARSER_EXT_PARSER_TEST_REPORTER_H
#define CXPARSER_EXT_PARSER_TEST_REPORTER_H

#include <string>

#include "parser_test_driver.h"

namespace cxparser_ext
{
std::string BuildTestReport(const ParserTestRunResult &result);
}

#endif
