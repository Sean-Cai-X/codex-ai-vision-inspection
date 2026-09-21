#ifndef CXPARSER_EXT_PARSER_IR_RADARE_MATCHER_H
#define CXPARSER_EXT_PARSER_IR_RADARE_MATCHER_H

#include <string>
#include <vector>

#include "../adapters/radare/radare_types.h"
#include "../meta/parser_evidence.h"
#include "parser_analysis_bridge_types.h"
#include "parser_cxscript_flow.h"
#include "parser_cxscript_types.h"

namespace cxparser_ext
{
struct ParserIrRadareMatch
{
  std::string step_name;
  std::string call_name;
  std::string matched_symbol;
  bool matched = false;
};

struct ParserIrRadareStepMatch
{
  std::string step_name;
  std::string matched_symbol;
  bool matched = false;
};

struct ParserIrRadareEvidencePoint
{
  std::string point_kind;
  std::string point_name;
  std::string step_name;
  std::string related_symbol;
};

struct ParserIrRadareMatchResult
{
  std::vector<ParserIrRadareMatch> matched_calls;
  std::vector<ParserIrRadareStepMatch> matched_steps;
  std::vector<ParserIrRadareEvidencePoint> evidence_points;
  std::vector<std::string> unresolved_calls;
  std::vector<std::string> notes;
};

class ParserIrRadareMatcher
{
public:
  bool MatchFlow(const CxScriptFlow &flow,
                 const RadareAnalysisResult &analysis,
                 ParserIrRadareMatchResult &result) const;

  bool MatchExecution(const CxScriptExecutionResult &execution,
                      const RadareAnalysisResult &analysis,
                      ParserIrRadareMatchResult &result) const;

  bool BuildFlowBridgeResult(const ParserIrRadareMatchResult &match_result,
                             ParserAnalysisBridgeResult &bridge_result) const;

  bool BuildExecutionBridgeResult(const ParserIrRadareMatchResult &match_result,
                                  ParserAnalysisBridgeResult &bridge_result) const;

  bool BuildBridgeResult(const ParserIrRadareMatchResult &match_result,
                         ParserAnalysisBridgeResult &bridge_result) const;

  bool BuildEvidence(const ParserIrRadareMatchResult &match_result,
                     ParserEvidenceBundle &bundle) const;
};
}

#endif
