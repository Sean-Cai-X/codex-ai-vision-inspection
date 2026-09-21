#ifndef CXPARSER_EXT_PARSER_FEEDBACK_PLAN_H
#define CXPARSER_EXT_PARSER_FEEDBACK_PLAN_H

#include <string>
#include <vector>

namespace cxparser_ext
{
struct ParserFixSuggestion
{
  std::string kind;
  std::string target;
  std::string suggestion;
};

struct ParserFeedbackPlan
{
  std::vector<ParserFixSuggestion> suggestions;
  std::vector<std::string> rerun_tasks;
};
}

#endif
