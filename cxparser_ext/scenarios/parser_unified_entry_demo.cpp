#include <cmath>
#include <iostream>

#include "../meta/parser_pseudocode_types.h"
#include "../pipeline/parser_delivery_api.h"
#include "../pipeline/parser_unified_entry.h"

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
  if (!cxparser_ext::BuildDeliveryBindingSpec(pseudo_class, binding_spec))
  {
    std::cerr << "[FAIL] BuildBindingSpec failed for unified entry demo\n";
    return 1;
  }

  cxparser_ext::ParserUnifiedEntry entry;
  entry.SetBindingSpec(binding_spec);

  if (!cxparser_ext::SubmitDeliveryTask(
        entry,
        cxparser_ext::MakeImageProcessRequest("image_delivery_task",
                                              "trace.image.delivery",
                                              "cxparser_ext_demo",
                                              pseudo_class.module_name,
                                              pseudo_class.parser_alias,
                                              "Score",
                                              "ImageProbe probe;probe.Load(\"image.png\");probe.Detect(0.8);probe.Score();")))
  {
    std::cerr << "[FAIL] SubmitTask failed for image task\n";
    return 1;
  }

  if (!cxparser_ext::SubmitDeliveryTask(
        entry,
        cxparser_ext::MakeVideoFrameRequest("video_delivery_task",
                                            "trace.video.delivery",
                                            "cxparser_ext_demo",
                                            pseudo_class.module_name,
                                            pseudo_class.parser_alias,
                                            "Score",
                                            "ImageProbe probe;probe.Load(\"frame.jpg\");probe.Detect(0.6);probe.Score();")))
  {
    std::cerr << "[FAIL] SubmitTask failed for video task\n";
    return 1;
  }

  if (!cxparser_ext::SubmitDeliveryTask(
        entry,
        cxparser_ext::MakeYoloBatchRequest("yolo_delivery_task",
                                           "trace.yolo.delivery",
                                           "cxparser_ext_demo",
                                           pseudo_class.module_name,
                                           pseudo_class.parser_alias,
                                           "Score",
                                           "ImageProbe probe;probe.Load(\"batch.png\");probe.Detect(0.7);probe.Score();")))
  {
    std::cerr << "[FAIL] SubmitTask failed for yolo task\n";
    return 1;
  }

  if (!entry.ExecuteMainThreadCycle())
  {
    std::cerr << "[FAIL] ExecuteMainThreadCycle failed\n";
    return 1;
  }

  const cxparser_ext::ParserTaskUnit *image_task = entry.FindTask("image_delivery_task");
  const cxparser_ext::ParserTaskUnit *video_task = entry.FindTask("video_delivery_task");
  const cxparser_ext::ParserTaskUnit *yolo_task = entry.FindTask("yolo_delivery_task");
  const cxparser_ext::ParserTaskOutcome *image_outcome = entry.FindTaskOutcome("image_delivery_task");
  const cxparser_ext::ParserTaskOutcome *yolo_outcome = entry.FindTaskOutcome("yolo_delivery_task");
  if (!image_task || !video_task || !yolo_task)
  {
    std::cerr << "[FAIL] unified entry could not find submitted tasks\n";
    return 1;
  }

  if (!image_outcome || !yolo_outcome)
  {
    std::cerr << "[FAIL] unified entry could not expose task outcomes\n";
    return 1;
  }

  if (image_task->status != cxparser_ext::pts_validated ||
      video_task->status != cxparser_ext::pts_validated ||
      yolo_task->status != cxparser_ext::pts_validated)
  {
    std::cerr << "[FAIL] unified entry tasks did not all reach validated state\n";
    return 1;
  }

  if (!NearlyEqual(image_task->result.scalar_result, 8.0))
  {
    std::cerr << "[FAIL] image delivery score mismatch: " << image_task->result.scalar_result << "\n";
    return 1;
  }

  if (!NearlyEqual(video_task->result.scalar_result, 6.0))
  {
    std::cerr << "[FAIL] video delivery score mismatch: " << video_task->result.scalar_result << "\n";
    return 1;
  }

  if (!NearlyEqual(yolo_task->result.scalar_result, 7.0))
  {
    std::cerr << "[FAIL] yolo delivery score mismatch: " << yolo_task->result.scalar_result << "\n";
    return 1;
  }

  if (!HasIssueCode(yolo_task->report, "deadline_missed"))
  {
    std::cerr << "[FAIL] yolo delivery task should report deadline_missed\n";
    return 1;
  }

  if (!yolo_task->report.passed)
  {
    std::cerr << "[FAIL] yolo delivery task should remain passed in degraded mode\n";
    return 1;
  }

  if (!image_outcome->success || image_outcome->hard_fail || image_outcome->degraded)
  {
    std::cerr << "[FAIL] image delivery outcome should be clean success\n";
    return 1;
  }

  if (!yolo_outcome->success || yolo_outcome->hard_fail || !yolo_outcome->degraded)
  {
    std::cerr << "[FAIL] yolo delivery outcome should be degraded success\n";
    return 1;
  }

  const cxparser_ext::ParserMainThreadTick &tick = entry.GetLastTick();
  std::cout << "[PASS] unified_delivery accepted=" << tick.accepted_task_count
            << " executed=" << tick.executed_task_count
            << " success=" << (tick.success ? "true" : "false")
            << " image_score=" << image_task->result.scalar_result
            << " video_score=" << video_task->result.scalar_result
            << " yolo_score=" << yolo_task->result.scalar_result
            << " yolo_lane=" << yolo_task->route.lane_name
            << " yolo_deadline_ms=" << yolo_task->route.deadline_ms
            << " yolo_degraded=" << (yolo_task->route.allow_degraded_result ? "true" : "false")
            << " yolo_outcome_degraded=" << (yolo_outcome->degraded ? "true" : "false")
            << "\n";
  return 0;
}
