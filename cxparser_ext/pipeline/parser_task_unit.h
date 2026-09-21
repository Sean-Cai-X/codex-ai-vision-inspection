#ifndef CXPARSER_EXT_PARSER_TASK_UNIT_H
#define CXPARSER_EXT_PARSER_TASK_UNIT_H

#include <string>

#include "../meta/parser_evidence.h"
#include "../meta/parser_validation_types.h"
#include "parser_pipeline.h"
#include "parser_scene_policy.h"
#include "parser_task_types.h"

namespace cxparser_ext
{
enum ParserTaskStatus
{
  pts_created,
  pts_prepared,
  pts_running,
  pts_executed,
  pts_validated,
  pts_failed
};

struct ParserTaskOutcome
{
  bool success = false;
  bool hard_fail = false;
  bool degraded = false;
  std::string error_stage;
  std::string error_code;
  std::string error_message;
  std::string summary;
};

struct ParserTaskUnit
{
  std::string task_id;
  ParserRoutePolicy route;
  ExecutionTarget target;
  ParserPipeline pipeline;
  ExecutionResult result;
  ParserValidationReport report;
  ParserEvidenceBundle evidence;
  ParserTaskOutcome outcome;
  ParserTaskStatus status = pts_created;
  std::string failure_reason;
};
}

#endif
