#ifndef CXPARSER_EXT_PARSER_TASK_TYPES_H
#define CXPARSER_EXT_PARSER_TASK_TYPES_H

#include <string>
#include <vector>

#include "../meta/parser_execution_guard.h"

namespace cxparser_ext
{
namespace task_constants
{
inline const char *TaskTypeCoreTest() { return "core_test"; }
inline const char *TaskSubtypeParserEval() { return "parser_eval"; }
inline const char *TaskSubtypeImageProcess() { return "image_process"; }
inline const char *TaskSubtypeVideoFrame() { return "video_frame"; }
inline const char *TaskSubtypeYoloBatch() { return "yolo_batch"; }
inline const char *RouteDefault() { return "default"; }
inline const char *RouteRealtime() { return "realtime"; }
inline const char *RouteBatch() { return "batch"; }
inline const char *RouteReplay() { return "replay"; }
inline const char *ExecutionModeMainline() { return "mainline_execute"; }
inline const char *ExecutionModeReplay() { return "replay_execute"; }
}

struct ParserModuleCall
{
  std::string caller_module;
  std::string callee_module;
  std::string protocol_name;
  std::string capability_name;
  std::string object_name;
  std::string class_name;
  std::string method_name;
};

struct ParserRoutePolicy
{
  std::string route_key;
  std::string lane_name;
  ExecutionGuardProfile guard_profile = egp_default;
  int deadline_ms = 0;
  int timeout_ms = 0;
  bool allow_degraded_result = true;
};

struct CxTaskEnvelope
{
  std::string task_id;
  std::string task_name;
  std::string trace_id;
  std::string task_type;
  std::string task_subtype;
  std::string route;
  std::string execution_mode;
  std::string caller_module;
  std::string callee_module;
  std::string target_class;
  std::string target_method;
  std::string script_text;
  std::vector<std::string> tags;
};

struct ExecutionTarget
{
  std::string task_id;
  std::string task_name;
  std::string trace_id;
  std::string task_type;
  std::string task_subtype;
  std::string execution_mode;
  std::string route_hint;
  std::string priority_hint;
  std::string module_name;
  std::string target_class;
  std::string target_method;
  std::string script_text;
  ParserModuleCall module_call;
  ParserRoutePolicy route;
  std::vector<std::string> tags;
};

struct TaskChainRecord
{
  std::string task_id;
  std::string task_type;
  std::string task_subtype;
  std::string route;
  std::string execution_mode;
  std::string replay_source_task_id;
  std::vector<std::string> modules;
};

struct ExecutionResult
{
  bool success = false;
  double scalar_result = 0.0;
  double accuracy = 0.0;
  double macro_f1 = 0.0;
  std::string text_result;
  std::string error_kind;
  std::string error_message;
  int parser_error_code = -1;
  int parser_error_pos = -1;
  std::string parser_error_token;
  std::string parser_error_expr;
  std::vector<std::string> warnings;
};

struct ValidationTarget
{
  std::string class_name;
  std::string method_name;
  std::string reason;
};
}

#endif
