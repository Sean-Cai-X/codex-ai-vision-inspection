#include <cmath>
#include <iostream>

#include "../meta/parser_pseudocode_types.h"
#include "../pipeline/parser_binding_builder.h"
#include "../pipeline/parser_pipeline.h"
#include "image_probe_wrapper.h"

namespace
{
bool NearlyEqual(double lhs, double rhs, double eps = 1e-9)
{
  return std::fabs(lhs - rhs) <= eps;
}

void PrintGuardLimits(const cxparser_ext::ExecutionGuardLimits &limits, const char *label)
{
  std::cout << "[GUARD] " << label
            << " stage_limit=" << limits.max_stage_count
            << " event_limit=" << limits.max_event_count
            << " object_limit=" << limits.max_object_calls
            << " timeout_ms=" << limits.timeout_ms
            << "\n";
}

cxparser_ext::EvidenceEvent MakeEvent(cxparser_ext::EvidenceEventLevel level,
                                      const std::string &stage,
                                      const std::string &code,
                                      const std::string &message,
                                      const std::string &expected = std::string(),
                                      const std::string &actual = std::string())
{
  cxparser_ext::EvidenceEvent event;
  event.level = level;
  event.stage = stage;
  event.code = code;
  event.message = message;
  event.expected = expected;
  event.actual = actual;
  return event;
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

int RunParserSuccessCase(const cxparser_ext::PseudoClassSpec &pseudo_class)
{
  cxparser_ext::PseudoScriptSpec script_case;
  script_case.case_name = "image_probe_basic";
  script_case.script_text = "ImageProbe probe;probe.Load(\"sample.png\");probe.Detect(0.8);probe.Score();";
  script_case.expected_scalar = 8.0;

  cxparser_ext::ParserBindingSpec binding_spec;
  if (!cxparser_ext::BuildBindingSpec(pseudo_class, binding_spec))
  {
    std::cerr << "[FAIL] BuildBindingSpec failed\n";
    return 1;
  }

  if (binding_spec.modules.empty() || binding_spec.modules[0].classes.empty())
  {
    std::cerr << "[FAIL] binding spec is empty\n";
    return 1;
  }

  ImageProbeWrapper probe;
  if (!probe.IsReady())
  {
    std::cerr << "[FAIL] wrapper not ready: " << probe.GetLastError() << "\n";
    return 1;
  }

  probe.Load("sample.png");
  if (!probe.GetLastError().empty())
  {
    std::cerr << "[FAIL] wrapper Load failed: " << probe.GetLastError() << "\n";
    return 1;
  }

  probe.Detect(0.8);
  const double wrapper_score = probe.Score();
  if (!NearlyEqual(wrapper_score, script_case.expected_scalar))
  {
    std::cerr << "[FAIL] wrapper Score mismatch: " << wrapper_score << "\n";
    return 1;
  }

  cxparser_ext::ExecutionTarget target;
  target.task_name = script_case.case_name;
  target.module_name = pseudo_class.module_name;
  target.target_class = pseudo_class.parser_alias;
  target.target_method = "Score";
  target.script_text = script_case.script_text;

  cxparser_ext::ParserPipeline pipeline;
  pipeline.SetGuardProfile(cxparser_ext::egp_default);
  PrintGuardLimits(pipeline.GetGuardLimits(), "default");
  if (!pipeline.PrepareTask(target))
  {
    std::cerr << "[FAIL] PrepareTask failed\n";
    return 1;
  }

  if (!pipeline.MergeBindingSpec(binding_spec))
  {
    std::cerr << "[FAIL] MergeBindingSpec failed\n";
    return 1;
  }

  cxparser_ext::ParserEvidenceBundle preflight_bundle;
  preflight_bundle.task_id = script_case.case_name;
  preflight_bundle.events.push_back(MakeEvent(cxparser_ext::eel_info,
                                              "wrapper_load",
                                              "wrapper_load_success",
                                              "wrapper preflight load succeeded",
                                              "wrapper load should succeed",
                                              "wrapper load succeeded"));
  preflight_bundle.events.push_back(MakeEvent(cxparser_ext::eel_info,
                                              "wrapper_detect",
                                              "wrapper_detect_success",
                                              "wrapper preflight detect succeeded",
                                              "wrapper detect should succeed",
                                              "wrapper detect succeeded"));
  preflight_bundle.notes.push_back("wrapper preflight completed before parser execution");
  pipeline.MergeEvidence(preflight_bundle);

  cxparser_ext::ExecutionResult result;
  if (!pipeline.Run(result))
  {
    std::cerr << "[FAIL] Run failed: " << result.error_message << "\n";
    return 1;
  }

  if (!result.success)
  {
    std::cerr << "[FAIL] execution not successful\n";
    return 1;
  }

  if (!NearlyEqual(result.scalar_result, script_case.expected_scalar))
  {
    std::cerr << "[FAIL] result mismatch: " << result.scalar_result << "\n";
    return 1;
  }

  ImageProbeWrapper *parser_probe =
    static_cast<ImageProbeWrapper *>(pipeline.GetClassObject("ImageProbe", "probe"));
  if (!parser_probe)
  {
    std::cerr << "[FAIL] parser-created object not found\n";
    return 1;
  }

  const double parser_score = parser_probe->Score();
  if (!parser_probe->GetLastError().empty())
  {
    std::cerr << "[FAIL] parser object Score failed: " << parser_probe->GetLastError() << "\n";
    return 1;
  }

  if (!NearlyEqual(parser_score, script_case.expected_scalar))
  {
    std::cerr << "[FAIL] parser object score mismatch: " << parser_score << "\n";
    return 1;
  }

  cxparser_ext::ParserValidationReport report;
  if (!pipeline.Validate(report))
  {
    std::cerr << "[FAIL] Validate failed\n";
    return 1;
  }

  if (!report.issues.empty())
  {
    for (size_t i = 0; i < report.issues.size(); ++i)
    {
      std::cout << "[REPORT] level=" << report.issues[i].level
                << " code=" << report.issues[i].code
                << " stage=" << report.issues[i].failure_stage
                << " expected=" << report.issues[i].expected
                << " actual=" << report.issues[i].actual
                << " message=" << report.issues[i].message << "\n";
    }
  }

  std::cout << "[PASS] wrapper_score=" << wrapper_score
            << " parser_score=" << result.scalar_result
            << " parser_object_score=" << parser_score
            << " validated=" << (report.passed ? "true" : "false")
            << "\n";
  return 0;
}

int RunParserBadScriptCase(const cxparser_ext::PseudoClassSpec &pseudo_class)
{
  cxparser_ext::ParserBindingSpec binding_spec;
  if (!cxparser_ext::BuildBindingSpec(pseudo_class, binding_spec))
  {
    std::cerr << "[FAIL] BuildBindingSpec failed for bad script case\n";
    return 1;
  }

  cxparser_ext::ExecutionTarget target;
  target.task_name = "image_probe_bad_script";
  target.module_name = pseudo_class.module_name;
  target.target_class = pseudo_class.parser_alias;
  target.target_method = "Score";
  target.script_text = "ImageProbe probe;probe.UnknownCall();";

  cxparser_ext::ParserPipeline pipeline;
  pipeline.SetGuardProfile(cxparser_ext::egp_default);
  PrintGuardLimits(pipeline.GetGuardLimits(), "default");
  if (!pipeline.PrepareTask(target))
  {
    std::cerr << "[FAIL] PrepareTask failed for bad script case\n";
    return 1;
  }

  if (!pipeline.MergeBindingSpec(binding_spec))
  {
    std::cerr << "[FAIL] MergeBindingSpec failed for bad script case\n";
    return 1;
  }

  cxparser_ext::ExecutionResult result;
  const bool run_ok = pipeline.Run(result);
  if (run_ok || result.success)
  {
    std::cerr << "[FAIL] bad script should fail\n";
    return 1;
  }

  if (result.error_message.empty())
  {
    std::cerr << "[FAIL] bad script should report an error message\n";
    return 1;
  }

  std::cout << "[PASS] parser bad-script rejected: " << result.error_message << "\n";
  return 0;
}
}

int main()
{
  const cxparser_ext::PseudoClassSpec pseudo_class = BuildPseudoClass();

  if (RunParserSuccessCase(pseudo_class) != 0)
    return 1;
  if (RunParserBadScriptCase(pseudo_class) != 0)
    return 1;
  return 0;
}
