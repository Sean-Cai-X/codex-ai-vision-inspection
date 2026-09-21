#include <iostream>
#include <string>

#include "../meta/parser_evidence.h"
#include "../meta/parser_pseudocode_types.h"
#include "../pipeline/parser_binding_builder.h"
#include "../pipeline/parser_pipeline.h"

namespace
{
void PrintGuardLimits(const cxparser_ext::ExecutionGuardLimits &limits, const char *label)
{
  std::cout << "[GUARD] " << label
            << " stage_limit=" << limits.max_stage_count
            << " event_limit=" << limits.max_event_count
            << " object_limit=" << limits.max_object_calls
            << " deadline_ms=" << limits.deadline_ms
            << " timeout_ms=" << limits.timeout_ms
            << " allow_degraded=" << (limits.allow_degraded_result ? "true" : "false")
            << "\n";
}

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

bool HasGuardTriggered(const cxparser_ext::ParserValidationReport &report)
{
  for (size_t i = 0; i < report.issues.size(); ++i)
  {
    if (report.issues[i].code == "guard_triggered")
      return true;
  }
  return false;
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

int RunEventLimitCase()
{
  const cxparser_ext::PseudoClassSpec pseudo_class = BuildPseudoClass();
  cxparser_ext::ParserBindingSpec binding_spec;
  if (!cxparser_ext::BuildBindingSpec(pseudo_class, binding_spec))
  {
    std::cerr << "[FAIL] BuildBindingSpec failed for event-limit case\n";
    return 1;
  }

  cxparser_ext::ExecutionTarget target;
  target.task_name = "guard_event_limit";
  target.module_name = pseudo_class.module_name;
  target.target_class = pseudo_class.parser_alias;
  target.target_method = "Score";
  target.script_text = "ImageProbe probe;probe.Load(\"sample.png\");probe.Detect(0.8);probe.Score();";

  cxparser_ext::ParserPipeline pipeline;
  pipeline.SetGuardProfile(cxparser_ext::egp_strict);
  PrintGuardLimits(pipeline.GetGuardLimits(), "strict");
  if (!pipeline.PrepareTask(target))
  {
    std::cerr << "[FAIL] PrepareTask failed for event-limit case\n";
    return 1;
  }

  if (!pipeline.MergeBindingSpec(binding_spec))
  {
    std::cerr << "[FAIL] MergeBindingSpec failed for event-limit case\n";
    return 1;
  }

  cxparser_ext::ParserEvidenceBundle bundle;
  bundle.task_id = "guard_event_limit";
  for (int i = 0; i < 40; ++i)
  {
    cxparser_ext::EvidenceEvent event;
    event.level = cxparser_ext::eel_info;
    event.stage = "preflight";
    event.code = "bulk_event";
    event.message = "bulk evidence event " + std::to_string(i);
    bundle.events.push_back(event);
  }

  const bool merge_ok = pipeline.MergeEvidence(bundle);
  if (merge_ok)
  {
    std::cerr << "[FAIL] event-limit case should trigger guard during evidence merge\n";
    return 1;
  }

  cxparser_ext::ParserValidationReport report;
  if (!pipeline.Validate(report))
  {
    std::cerr << "[FAIL] Validate failed for event-limit case\n";
    return 1;
  }

  if (!HasGuardTriggered(report))
  {
    std::cerr << "[FAIL] event-limit case missing guard_triggered issue\n";
    return 1;
  }

  std::cout << "[PASS] guard event-limit triggered\n";
  return 0;
}

int RunObjectCallLimitCase()
{
  const cxparser_ext::PseudoClassSpec pseudo_class = BuildPseudoClass();
  cxparser_ext::ParserBindingSpec binding_spec;
  if (!cxparser_ext::BuildBindingSpec(pseudo_class, binding_spec))
  {
    std::cerr << "[FAIL] BuildBindingSpec failed for object-call case\n";
    return 1;
  }

  cxparser_ext::ExecutionTarget target;
  target.task_name = "guard_object_limit";
  target.module_name = pseudo_class.module_name;
  target.target_class = pseudo_class.parser_alias;
  target.target_method = "Score";
  target.script_text = "ImageProbe probe;probe.Load(\"sample.png\");probe.Detect(0.8);probe.Score();";

  cxparser_ext::ParserPipeline pipeline;
  pipeline.SetGuardProfile(cxparser_ext::egp_strict);
  PrintGuardLimits(pipeline.GetGuardLimits(), "strict");
  if (!pipeline.PrepareTask(target))
  {
    std::cerr << "[FAIL] PrepareTask failed for object-call case\n";
    return 1;
  }

  if (!pipeline.MergeBindingSpec(binding_spec))
  {
    std::cerr << "[FAIL] MergeBindingSpec failed for object-call case\n";
    return 1;
  }

  cxparser_ext::ExecutionResult result;
  if (!pipeline.Run(result))
  {
    std::cerr << "[FAIL] Run failed for object-call case: " << result.error_message << "\n";
    return 1;
  }

  for (int i = 0; i < 8; ++i)
  {
    if (!pipeline.GetClassObject("ImageProbe", "probe"))
    {
      std::cerr << "[FAIL] object retrieval should succeed before guard limit\n";
      return 1;
    }
  }

  if (pipeline.GetClassObject("ImageProbe", "probe") != 0)
  {
    std::cerr << "[FAIL] object-call case should trigger guard on extra retrieval\n";
    return 1;
  }

  cxparser_ext::ParserValidationReport report;
  if (!pipeline.Validate(report))
  {
    std::cerr << "[FAIL] Validate failed for object-call case\n";
    return 1;
  }

  if (!HasGuardTriggered(report))
  {
    std::cerr << "[FAIL] object-call case missing guard_triggered issue\n";
    return 1;
  }

  std::cout << "[PASS] guard object-call-limit triggered\n";
  return 0;
}

int RunDebugProfileAllowsMoreEvidenceCase()
{
  const cxparser_ext::PseudoClassSpec pseudo_class = BuildPseudoClass();
  cxparser_ext::ParserBindingSpec binding_spec;
  if (!cxparser_ext::BuildBindingSpec(pseudo_class, binding_spec))
  {
    std::cerr << "[FAIL] BuildBindingSpec failed for debug-profile case\n";
    return 1;
  }

  cxparser_ext::ExecutionTarget target;
  target.task_name = "guard_debug_profile";
  target.module_name = pseudo_class.module_name;
  target.target_class = pseudo_class.parser_alias;
  target.target_method = "Score";
  target.script_text = "ImageProbe probe;probe.Load(\"sample.png\");probe.Detect(0.8);probe.Score();";

  cxparser_ext::ParserPipeline pipeline;
  pipeline.SetGuardProfile(cxparser_ext::egp_debug);
  PrintGuardLimits(pipeline.GetGuardLimits(), "debug");
  if (!pipeline.PrepareTask(target))
  {
    std::cerr << "[FAIL] PrepareTask failed for debug-profile case\n";
    return 1;
  }

  if (!pipeline.MergeBindingSpec(binding_spec))
  {
    std::cerr << "[FAIL] MergeBindingSpec failed for debug-profile case\n";
    return 1;
  }

  cxparser_ext::ParserEvidenceBundle bundle;
  bundle.task_id = "guard_debug_profile";
  for (int i = 0; i < 40; ++i)
  {
    cxparser_ext::EvidenceEvent event;
    event.level = cxparser_ext::eel_info;
    event.stage = "preflight";
    event.code = "bulk_event";
    event.message = "debug-profile event " + std::to_string(i);
    bundle.events.push_back(event);
  }

  if (!pipeline.MergeEvidence(bundle))
  {
    std::cerr << "[FAIL] debug profile should allow 40 evidence events\n";
    return 1;
  }

  cxparser_ext::ExecutionResult result;
  if (!pipeline.Run(result))
  {
    std::cerr << "[FAIL] Run failed for debug-profile case: " << result.error_message << "\n";
    return 1;
  }

  if (!result.success)
  {
    std::cerr << "[FAIL] debug-profile execution should succeed\n";
    return 1;
  }

  cxparser_ext::ParserValidationReport report;
  if (!pipeline.Validate(report))
  {
    std::cerr << "[FAIL] Validate failed for debug-profile case\n";
    return 1;
  }

  if (HasGuardTriggered(report))
  {
    std::cerr << "[FAIL] debug profile should not trigger guard for 40 evidence events\n";
    return 1;
  }

  std::cout << "[PASS] debug profile allowed larger evidence batch\n";
  return 0;
}

int RunDeadlineMissWithDegradedResultCase()
{
  const cxparser_ext::PseudoClassSpec pseudo_class = BuildPseudoClass();
  cxparser_ext::ParserBindingSpec binding_spec;
  if (!cxparser_ext::BuildBindingSpec(pseudo_class, binding_spec))
  {
    std::cerr << "[FAIL] BuildBindingSpec failed for deadline/degraded case\n";
    return 1;
  }

  cxparser_ext::ExecutionTarget target;
  target.task_name = "guard_deadline_degraded";
  target.module_name = pseudo_class.module_name;
  target.target_class = pseudo_class.parser_alias;
  target.target_method = "Score";
  target.script_text = "ImageProbe probe;probe.Load(\"sample.png\");probe.Detect(0.8);probe.Score();";

  cxparser_ext::ParserPipeline pipeline;
  pipeline.SetGuardProfile(cxparser_ext::egp_debug);
  cxparser_ext::ExecutionGuardLimits limits = pipeline.GetGuardLimits();
  limits.deadline_ms = -1;
  limits.allow_degraded_result = true;
  pipeline.SetGuardLimits(limits);
  PrintGuardLimits(pipeline.GetGuardLimits(), "debug-degraded");

  if (!pipeline.PrepareTask(target))
  {
    std::cerr << "[FAIL] PrepareTask failed for deadline/degraded case\n";
    return 1;
  }

  if (!pipeline.MergeBindingSpec(binding_spec))
  {
    std::cerr << "[FAIL] MergeBindingSpec failed for deadline/degraded case\n";
    return 1;
  }

  cxparser_ext::ExecutionResult result;
  if (!pipeline.Run(result))
  {
    std::cerr << "[FAIL] Run failed for deadline/degraded case: " << result.error_message << "\n";
    return 1;
  }

  if (!result.success)
  {
    std::cerr << "[FAIL] degraded deadline miss should still allow execution success\n";
    return 1;
  }

  cxparser_ext::ParserValidationReport report;
  if (!pipeline.Validate(report))
  {
    std::cerr << "[FAIL] Validate failed for deadline/degraded case\n";
    return 1;
  }

  if (!HasIssueCode(report, "deadline_missed"))
  {
    std::cerr << "[FAIL] deadline/degraded case missing deadline_missed issue\n";
    return 1;
  }

  if (!report.passed)
  {
    std::cerr << "[FAIL] deadline/degraded case should remain passed with warning semantics\n";
    return 1;
  }

  std::cout << "[PASS] deadline miss allowed degraded result\n";
  return 0;
}
}

int main()
{
  if (RunEventLimitCase() != 0)
    return 1;
  if (RunObjectCallLimitCase() != 0)
    return 1;
  if (RunDebugProfileAllowsMoreEvidenceCase() != 0)
    return 1;
  if (RunDeadlineMissWithDegradedResultCase() != 0)
    return 1;
  return 0;
}
