#ifndef CXPARSER_EXT_PARSER_ANALYSIS_BRIDGE_TYPES_H
#define CXPARSER_EXT_PARSER_ANALYSIS_BRIDGE_TYPES_H

#include <string>
#include <vector>

#include "parser_cxscript_types.h"

namespace cxparser_ext
{
enum ParserAnalysisBridgeSeverity
{
  pabs_info,
  pabs_warning,
  pabs_error
};

enum ParserAnalysisBridgeStatus
{
  pabs_unknown,
  pabs_resolved,
  pabs_partial,
  pabs_unresolved
};

enum ParserAnalysisBridgeOrigin
{
  pabo_unknown,
  pabo_flow,
  pabo_execution,
  pabo_schema
};

struct ParserAnalysisBridgeCall
{
  std::string step_name;
  std::string call_name;
  std::string matched_symbol;
  bool matched = false;
  ParserAnalysisBridgeSeverity severity = pabs_info;
  std::string category;
  std::string bridge_stage;
};

struct ParserAnalysisBridgeStep
{
  std::string step_name;
  std::string matched_symbol;
  bool matched = false;
  ParserAnalysisBridgeSeverity severity = pabs_info;
  std::string category;
  std::string bridge_stage;
};

struct ParserAnalysisBridgePoint
{
  std::string point_kind;
  std::string point_name;
  std::string step_name;
  int step_id = 0;
  int frame_id = 0;
  int block_depth = 0;
  CxScriptSourceSpan span;
  std::string related_symbol;
  bool matched = false;
  ParserAnalysisBridgeSeverity severity = pabs_info;
  std::string category;
  std::string bridge_stage;
};

struct ParserAnalysisBridgeMetrics
{
  int matched_call_count = 0;
  int unresolved_call_count = 0;
  int step_count = 0;
  int point_count = 0;
};

struct ParserAnalysisBridgeResult
{
  std::string bridge_name;
  std::string bridge_stage;
  std::string category;
  std::string summary;
  std::string reason;
  ParserAnalysisBridgeSeverity severity = pabs_info;
  ParserAnalysisBridgeStatus status = pabs_unknown;
  ParserAnalysisBridgeOrigin origin = pabo_unknown;
  ParserAnalysisBridgeMetrics metrics;
  std::vector<ParserAnalysisBridgeCall> calls;
  std::vector<ParserAnalysisBridgeStep> steps;
  std::vector<ParserAnalysisBridgePoint> points;
  std::vector<std::string> unresolved_calls;
  std::vector<std::string> notes;
};
}

#endif
