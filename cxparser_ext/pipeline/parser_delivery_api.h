#ifndef CXPARSER_EXT_PARSER_DELIVERY_API_H
#define CXPARSER_EXT_PARSER_DELIVERY_API_H

#include <string>

#include "../meta/parser_binding_spec.h"
#include "../meta/parser_pseudocode_types.h"
#include "parser_binding_builder.h"
#include "parser_unified_entry.h"

namespace cxparser_ext
{
struct ParserDeliveryRequest
{
  std::string task_id;
  std::string task_name;
  std::string trace_id;
  std::string caller_module;
  std::string callee_module;
  std::string protocol_name;
  std::string capability_name;
  std::string target_class;
  std::string target_method;
  std::string script_text;
  std::string route_hint;
  std::string priority_hint;
  ParserRoutePolicy route;
};

bool BuildDeliveryBindingSpec(const PseudoClassSpec &pseudo, ParserBindingSpec &spec);
ExecutionTarget MakeDeliveryTarget(const ParserDeliveryRequest &request);
bool SubmitDeliveryTask(ParserUnifiedEntry &entry, const ParserDeliveryRequest &request);

ParserDeliveryRequest MakeImageProcessRequest(const std::string &task_id,
                                             const std::string &trace_id,
                                             const std::string &caller_module,
                                             const std::string &callee_module,
                                             const std::string &target_class,
                                             const std::string &target_method,
                                             const std::string &script_text);

ParserDeliveryRequest MakeVideoFrameRequest(const std::string &task_id,
                                            const std::string &trace_id,
                                            const std::string &caller_module,
                                            const std::string &callee_module,
                                            const std::string &target_class,
                                            const std::string &target_method,
                                            const std::string &script_text);

ParserDeliveryRequest MakeYoloBatchRequest(const std::string &task_id,
                                           const std::string &trace_id,
                                           const std::string &caller_module,
                                           const std::string &callee_module,
                                           const std::string &target_class,
                                           const std::string &target_method,
                                           const std::string &script_text);
}

#endif
