#include "parser_delivery_api.h"

namespace cxparser_ext
{
namespace
{
ParserDeliveryRequest MakeBaseRequest(const std::string &task_id,
                                      const std::string &trace_id,
                                      const std::string &caller_module,
                                      const std::string &callee_module,
                                      const std::string &capability_name,
                                      const std::string &target_class,
                                      const std::string &target_method,
                                      const std::string &script_text)
{
  ParserDeliveryRequest request;
  request.task_id = task_id;
  request.task_name = task_id;
  request.trace_id = trace_id;
  request.caller_module = caller_module;
  request.callee_module = callee_module;
  request.protocol_name = "cxparser.module.call";
  request.capability_name = capability_name;
  request.target_class = target_class;
  request.target_method = target_method;
  request.script_text = script_text;
  return request;
}
}

bool BuildDeliveryBindingSpec(const PseudoClassSpec &pseudo, ParserBindingSpec &spec)
{
  return BuildBindingSpec(pseudo, spec);
}

ExecutionTarget MakeDeliveryTarget(const ParserDeliveryRequest &request)
{
  ExecutionTarget target;
  target.task_id = request.task_id;
  target.task_name = request.task_name.empty() ? request.task_id : request.task_name;
  target.trace_id = request.trace_id;
  target.route_hint = request.route_hint;
  target.priority_hint = request.priority_hint;
  target.module_name = request.callee_module;
  target.target_class = request.target_class;
  target.target_method = request.target_method;
  target.script_text = request.script_text;
  target.module_call.caller_module = request.caller_module;
  target.module_call.callee_module = request.callee_module;
  target.module_call.protocol_name = request.protocol_name.empty() ? "cxparser.module.call" : request.protocol_name;
  target.module_call.capability_name = request.capability_name;
  target.module_call.class_name = request.target_class;
  target.module_call.method_name = request.target_method;
  target.route = request.route;
  return target;
}

bool SubmitDeliveryTask(ParserUnifiedEntry &entry, const ParserDeliveryRequest &request)
{
  return entry.SubmitTask(MakeDeliveryTarget(request));
}

ParserDeliveryRequest MakeImageProcessRequest(const std::string &task_id,
                                             const std::string &trace_id,
                                             const std::string &caller_module,
                                             const std::string &callee_module,
                                             const std::string &target_class,
                                             const std::string &target_method,
                                             const std::string &script_text)
{
  return MakeBaseRequest(task_id,
                         trace_id,
                         caller_module,
                         callee_module,
                         "script_dispatch",
                         target_class,
                         target_method,
                         script_text);
}

ParserDeliveryRequest MakeVideoFrameRequest(const std::string &task_id,
                                            const std::string &trace_id,
                                            const std::string &caller_module,
                                            const std::string &callee_module,
                                            const std::string &target_class,
                                            const std::string &target_method,
                                            const std::string &script_text)
{
  ParserDeliveryRequest request =
    MakeBaseRequest(task_id,
                    trace_id,
                    caller_module,
                    callee_module,
                    "stream_frame",
                    target_class,
                    target_method,
                    script_text);
  request.route_hint = "realtime";
  request.priority_hint = "realtime";
  return request;
}

ParserDeliveryRequest MakeYoloBatchRequest(const std::string &task_id,
                                           const std::string &trace_id,
                                           const std::string &caller_module,
                                           const std::string &callee_module,
                                           const std::string &target_class,
                                           const std::string &target_method,
                                           const std::string &script_text)
{
  ParserDeliveryRequest request =
    MakeBaseRequest(task_id,
                    trace_id,
                    caller_module,
                    callee_module,
                    "yolo_infer",
                    target_class,
                    target_method,
                    script_text);
  request.route_hint = "batch";
  request.priority_hint = "background";
  request.route.deadline_ms = -1;
  request.route.allow_degraded_result = true;
  return request;
}
}
