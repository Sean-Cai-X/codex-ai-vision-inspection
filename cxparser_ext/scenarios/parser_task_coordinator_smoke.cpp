#include <cmath>
#include <iostream>

#include "../meta/parser_pseudocode_types.h"
#include "../pipeline/parser_binding_builder.h"
#include "../pipeline/parser_task_coordinator.h"

namespace
{
cxparser_ext::PseudoClassSpec BuildPseudoClass()
{
  cxparser_ext::PseudoClassSpec pseudo_class;
  pseudo_class.module_name = "testdll_image_probe";
  pseudo_class.class_name = "ImageProbeWrapper";
  pseudo_class.parser_alias = "ImageProbe";

  cxparser_ext::PseudoMethodSpec load_method;
  load_method.name = "Load";
  load_method.param_types.push_back("const char*");
  load_method.return_type = "void";
  pseudo_class.methods.push_back(load_method);

  cxparser_ext::PseudoMethodSpec detect_method;
  detect_method.name = "Detect";
  detect_method.param_types.push_back("double");
  detect_method.return_type = "void";
  pseudo_class.methods.push_back(detect_method);

  cxparser_ext::PseudoMethodSpec score_method;
  score_method.name = "Score";
  score_method.return_type = "double";
  pseudo_class.methods.push_back(score_method);

  return pseudo_class;
}

bool NearlyEqual(double lhs, double rhs, double eps = 1e-9)
{
  return std::fabs(lhs - rhs) <= eps;
}

bool HasIssueCode(const cxparser_ext::ParserValidationReport &report, const char *code)
{
  for (size_t i = 0; i < report.issues.size(); ++i)
  {
    if (report.issues[i].code == code)
      return true;
  }
  return false;
}
}

int main()
{
  const cxparser_ext::PseudoClassSpec pseudo_class = BuildPseudoClass();
  cxparser_ext::ParserBindingSpec binding_spec;
  if (!cxparser_ext::BuildBindingSpec(pseudo_class, binding_spec))
  {
    std::cerr << "[FAIL] BuildBindingSpec failed for coordinator smoke\n";
    return 1;
  }

  cxparser_ext::ExecutionTarget image_target;
  image_target.task_id = "image_task";
  image_target.task_name = "image_process_case";
  image_target.trace_id = "trace.image_task";
  image_target.priority_hint = "default";
  image_target.module_name = pseudo_class.module_name;
  image_target.target_class = pseudo_class.parser_alias;
  image_target.target_method = "Score";
  image_target.module_call.caller_module = "cxparser";
  image_target.module_call.callee_module = pseudo_class.module_name;
  image_target.module_call.protocol_name = "cxparser.module.call";
  image_target.module_call.capability_name = "script_dispatch";
  image_target.module_call.class_name = pseudo_class.parser_alias;
  image_target.module_call.method_name = "Score";
  image_target.script_text = "ImageProbe probe;probe.Load(\"image.png\");probe.Detect(0.8);probe.Score();";

  cxparser_ext::ExecutionTarget video_target;
  video_target.task_id = "video_task";
  video_target.task_name = "video_frame_case";
  video_target.trace_id = "trace.video_task";
  video_target.route_hint = "realtime";
  video_target.priority_hint = "realtime";
  video_target.module_name = pseudo_class.module_name;
  video_target.target_class = pseudo_class.parser_alias;
  video_target.target_method = "Score";
  video_target.module_call.caller_module = "cxparser";
  video_target.module_call.callee_module = pseudo_class.module_name;
  video_target.module_call.protocol_name = "cxparser.module.call";
  video_target.module_call.capability_name = "stream_frame";
  video_target.module_call.class_name = pseudo_class.parser_alias;
  video_target.module_call.method_name = "Score";
  video_target.script_text = "ImageProbe probe;probe.Load(\"frame.jpg\");probe.Detect(0.6);probe.Score();";

  cxparser_ext::ExecutionTarget yolo_target;
  yolo_target.task_id = "yolo_task";
  yolo_target.task_name = "yolo_batch_case";
  yolo_target.trace_id = "trace.yolo_task";
  yolo_target.route_hint = "batch";
  yolo_target.priority_hint = "background";
  yolo_target.module_name = pseudo_class.module_name;
  yolo_target.target_class = pseudo_class.parser_alias;
  yolo_target.target_method = "Score";
  yolo_target.module_call.caller_module = "cxparser";
  yolo_target.module_call.callee_module = pseudo_class.module_name;
  yolo_target.module_call.protocol_name = "cxparser.module.call";
  yolo_target.module_call.capability_name = "yolo_infer";
  yolo_target.module_call.class_name = pseudo_class.parser_alias;
  yolo_target.module_call.method_name = "Score";
  yolo_target.script_text = "ImageProbe probe;probe.Load(\"batch.png\");probe.Detect(0.7);probe.Score();";

  cxparser_ext::ParserTaskCoordinator coordinator;
  coordinator.SetBindingSpec(binding_spec);
  if (!coordinator.AddTask(image_target))
  {
    std::cerr << "[FAIL] AddTask failed for image_process\n";
    return 1;
  }

  if (!coordinator.AddTask(video_target))
  {
    std::cerr << "[FAIL] AddTask failed for video_frame\n";
    return 1;
  }

  if (!coordinator.AddTask(yolo_target))
  {
    std::cerr << "[FAIL] AddTask failed for yolo_infer\n";
    return 1;
  }

  cxparser_ext::ParserTaskUnit *image_task = coordinator.FindTask("image_task");
  cxparser_ext::ParserTaskUnit *video_task = coordinator.FindTask("video_task");
  cxparser_ext::ParserTaskUnit *yolo_task = coordinator.FindTask("yolo_task");
  if (!image_task || !video_task || !yolo_task)
  {
    std::cerr << "[FAIL] coordinator could not find added tasks\n";
    return 1;
  }

  cxparser_ext::ParserRoutePolicy yolo_batch_route = yolo_task->route;
  yolo_batch_route.deadline_ms = -1;
  yolo_batch_route.allow_degraded_result = true;
  if (!coordinator.UpdateTaskRoute("yolo_task", yolo_batch_route))
  {
    std::cerr << "[FAIL] UpdateTaskRoute failed for yolo batch task\n";
    return 1;
  }

  yolo_task = coordinator.FindTask("yolo_task");
  if (!yolo_task)
  {
    std::cerr << "[FAIL] coordinator lost yolo task after route update\n";
    return 1;
  }

  if (image_task->pipeline.GetGuardLimits().timeout_ms != image_task->route.timeout_ms)
  {
    std::cerr << "[FAIL] image task timeout policy was not applied\n";
    return 1;
  }

  if (image_task->pipeline.GetGuardLimits().deadline_ms != image_task->route.deadline_ms)
  {
    std::cerr << "[FAIL] image task deadline policy was not applied\n";
    return 1;
  }

  if (video_task->pipeline.GetGuardLimits().timeout_ms != video_task->route.timeout_ms)
  {
    std::cerr << "[FAIL] video task timeout policy was not applied\n";
    return 1;
  }

  if (video_task->pipeline.GetGuardLimits().deadline_ms != video_task->route.deadline_ms)
  {
    std::cerr << "[FAIL] video task deadline policy was not applied\n";
    return 1;
  }

  if (yolo_task->pipeline.GetGuardLimits().timeout_ms != yolo_task->route.timeout_ms)
  {
    std::cerr << "[FAIL] yolo task timeout policy was not applied\n";
    return 1;
  }

  if (yolo_task->pipeline.GetGuardLimits().deadline_ms != yolo_task->route.deadline_ms)
  {
    std::cerr << "[FAIL] yolo task deadline policy was not applied\n";
    return 1;
  }

  if (!coordinator.ExecuteAll())
  {
    std::cerr << "[FAIL] ExecuteAll failed\n";
    return 1;
  }

  if (image_task->status != cxparser_ext::pts_validated)
  {
    std::cerr << "[FAIL] image task did not reach validated state\n";
    return 1;
  }

  if (video_task->status != cxparser_ext::pts_validated)
  {
    std::cerr << "[FAIL] video task did not reach validated state\n";
    return 1;
  }

  if (yolo_task->status != cxparser_ext::pts_validated)
  {
    std::cerr << "[FAIL] yolo task did not reach validated state\n";
    return 1;
  }

  if (!NearlyEqual(image_task->result.scalar_result, 8.0))
  {
    std::cerr << "[FAIL] image task score mismatch: " << image_task->result.scalar_result << "\n";
    return 1;
  }

  if (!NearlyEqual(video_task->result.scalar_result, 6.0))
  {
    std::cerr << "[FAIL] video task score mismatch: " << video_task->result.scalar_result << "\n";
    return 1;
  }

  if (!NearlyEqual(yolo_task->result.scalar_result, 7.0))
  {
    std::cerr << "[FAIL] yolo task score mismatch: " << yolo_task->result.scalar_result << "\n";
    return 1;
  }

  if (image_task->route.lane_name != "default")
  {
    std::cerr << "[FAIL] image task route lane mismatch: " << image_task->route.lane_name << "\n";
    return 1;
  }

  if (video_task->route.lane_name != "realtime")
  {
    std::cerr << "[FAIL] video task route lane mismatch: " << video_task->route.lane_name << "\n";
    return 1;
  }

  if (yolo_task->route.lane_name != "batch")
  {
    std::cerr << "[FAIL] yolo task route lane mismatch: " << yolo_task->route.lane_name << "\n";
    return 1;
  }

  if (image_task->evidence.trace_entries.empty() || image_task->evidence.log_entries.empty())
  {
    std::cerr << "[FAIL] image task did not record unified trace/log\n";
    return 1;
  }

  if (video_task->evidence.trace_entries.empty() || video_task->evidence.log_entries.empty())
  {
    std::cerr << "[FAIL] video task did not record unified trace/log\n";
    return 1;
  }

  if (yolo_task->evidence.trace_entries.empty() || yolo_task->evidence.log_entries.empty())
  {
    std::cerr << "[FAIL] yolo task did not record unified trace/log\n";
    return 1;
  }

  if (!HasIssueCode(yolo_task->report, "deadline_missed"))
  {
    std::cerr << "[FAIL] yolo batch task should record deadline_missed under degraded mode\n";
    return 1;
  }

  if (!yolo_task->report.passed)
  {
    std::cerr << "[FAIL] yolo batch task should remain passed with degraded-result policy\n";
    return 1;
  }

  const cxparser_ext::ParserTaskOutcome *image_outcome = coordinator.FindTaskOutcome("image_task");
  const cxparser_ext::ParserTaskOutcome *yolo_outcome = coordinator.FindTaskOutcome("yolo_task");
  if (!image_outcome || !yolo_outcome)
  {
    std::cerr << "[FAIL] coordinator did not expose task outcomes\n";
    return 1;
  }

  if (!image_outcome->success || image_outcome->hard_fail || image_outcome->degraded)
  {
    std::cerr << "[FAIL] image outcome should expose a clean success state\n";
    return 1;
  }

  if (!yolo_outcome->success || yolo_outcome->hard_fail || !yolo_outcome->degraded)
  {
    std::cerr << "[FAIL] yolo outcome should expose degraded success state\n";
    return 1;
  }

  std::cout << "[PASS] image_process score=" << image_task->result.scalar_result
            << " image_lane=" << image_task->route.lane_name
            << " image_deadline_ms=" << image_task->pipeline.GetGuardLimits().deadline_ms
            << " image_timeout_ms=" << image_task->pipeline.GetGuardLimits().timeout_ms
            << " video_frame score=" << video_task->result.scalar_result
            << " video_lane=" << video_task->route.lane_name
            << " video_deadline_ms=" << video_task->pipeline.GetGuardLimits().deadline_ms
            << " video_timeout_ms=" << video_task->pipeline.GetGuardLimits().timeout_ms
            << " yolo_batch score=" << yolo_task->result.scalar_result
            << " yolo_lane=" << yolo_task->route.lane_name
            << " yolo_deadline_ms=" << yolo_task->pipeline.GetGuardLimits().deadline_ms
            << " yolo_timeout_ms=" << yolo_task->pipeline.GetGuardLimits().timeout_ms
            << " yolo_degraded=" << (yolo_task->pipeline.GetGuardLimits().allow_degraded_result ? "true" : "false")
            << " yolo_outcome_degraded=" << (yolo_outcome->degraded ? "true" : "false")
            << "\n";
  return 0;
}
