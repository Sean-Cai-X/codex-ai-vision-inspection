#include "parser_validation_engine.h"

namespace cxparser_ext
{
namespace
{
ValidationLevel ToValidationLevel(EvidenceEventLevel level)
{
  switch (level)
  {
  case eel_error:
    return vl_error;
  case eel_warning:
    return vl_warning;
  case eel_info:
  default:
    return vl_info;
  }
}

void AddIssue(ParserValidationReport &report,
              ValidationLevel level,
              const std::string &code,
              const std::string &message,
              const std::string &failure_stage = std::string(),
              const std::string &expected = std::string(),
              const std::string &actual = std::string())
{
  ValidationIssue issue;
  issue.level = level;
  issue.code = code;
  issue.message = message;
  issue.failure_stage = failure_stage;
  issue.expected = expected;
  issue.actual = actual;
  report.issues.push_back(issue);
}
}

bool ParserValidationEngine::CompareExecutionAndEvidence(const ExecutionResult &result,
                                                         const ParserEvidenceBundle &bundle,
                                                         ParserValidationReport &report)
{
  report = ParserValidationReport();

  if (!result.success)
  {
    const std::string issue_code =
      result.error_kind == "parser_exception" ? "parser_execution_failed" : "execution_failed";
    const std::string actual =
      result.error_kind == "parser_exception" ?
      ("code=" + std::to_string(result.parser_error_code) +
       " pos=" + std::to_string(result.parser_error_pos) +
       " token=" + result.parser_error_token +
       " expr=" + result.parser_error_expr) :
      result.error_message;
    AddIssue(report,
             vl_error,
             issue_code,
             result.error_message.empty() ? "execution failed" : result.error_message,
             "parser_execute",
             "script execution should succeed",
             actual);
    report.mismatched_points.push_back("runtime execution failed");
    return true;
  }

  report.matched_points.push_back("runtime execution succeeded");

  if (bundle.notes.empty() &&
      bundle.events.empty() &&
      bundle.trace_entries.empty() &&
      bundle.log_entries.empty() &&
      bundle.calls.empty() &&
      bundle.graph_nodes.empty() &&
      bundle.disasm_text.empty() &&
      bundle.decompile_text.empty())
  {
    AddIssue(report,
             vl_warning,
             "evidence_empty",
             "execution succeeded but no evidence was collected",
             "validation_compare",
             "runtime evidence should be present",
             "runtime evidence is empty");
  }
  else
  {
    report.matched_points.push_back("runtime evidence is present");
  }

  if (bundle.trace_entries.empty())
  {
    AddIssue(report,
             vl_warning,
             "trace_empty",
             "execution succeeded but no unified trace entries were recorded",
             "validation_compare",
             "task trace should be present",
             "task trace is empty");
  }
  else
  {
    report.matched_points.push_back("unified trace is present");
  }

  if (bundle.log_entries.empty())
  {
    AddIssue(report,
             vl_warning,
             "log_empty",
             "execution succeeded but no unified log entries were recorded",
             "validation_compare",
             "task log should be present",
             "task log is empty");
  }
  else
  {
    report.matched_points.push_back("unified log is present");
  }

  for (size_t i = 0; i < bundle.events.size(); ++i)
  {
    const EvidenceEvent &event = bundle.events[i];
    AddIssue(report,
             ToValidationLevel(event.level),
             event.code.empty() ? "evidence_event" : event.code,
             event.message,
             event.stage,
             event.expected,
             event.actual);

    if (event.level == eel_error)
      report.mismatched_points.push_back(event.stage.empty() ? "evidence error" : event.stage);
    else
      report.matched_points.push_back(event.stage.empty() ? "evidence event" : event.stage);
  }

  report.passed = true;
  for (size_t i = 0; i < report.issues.size(); ++i)
  {
    if (report.issues[i].level == vl_error)
      report.passed = false;
  }

  return true;
}

bool ParserValidationEngine::CompareSchemaAndBinding(const ApiSchema &schema,
                                                     const ParserBindingSpec &spec,
                                                     ParserValidationReport &report)
{
  report = ParserValidationReport();

  if (schema.classes.empty())
    AddIssue(report,
             vl_warning,
             "schema_empty",
             "api schema contains no classes",
             "binding_build",
             "schema should contain classes",
             "schema classes are empty");

  if (spec.modules.empty())
  {
    AddIssue(report,
             vl_error,
             "binding_empty",
             "binding spec contains no modules",
             "binding_build",
             "binding spec should contain modules",
             "binding spec modules are empty");
    report.mismatched_points.push_back("binding spec is empty");
    return true;
  }

  bool found_match = false;
  for (size_t i = 0; i < schema.classes.size() && !found_match; ++i)
  {
    const std::string &schema_name = schema.classes[i].name;
    for (size_t m = 0; m < spec.modules.size() && !found_match; ++m)
    {
      for (size_t c = 0; c < spec.modules[m].classes.size(); ++c)
      {
        const ParserClassMeta &bound_class = spec.modules[m].classes[c];
        if (bound_class.name == schema_name || bound_class.parser_alias == schema_name)
        {
          found_match = true;
          report.matched_points.push_back("class matched: " + schema_name);
          break;
        }
      }
    }
  }

  if (!found_match && !schema.classes.empty())
  {
    AddIssue(report,
             vl_warning,
             "binding_schema_mismatch",
             "no matching class found between schema and binding spec",
             "binding_build",
             "at least one class should match",
             "no matching class found");
    report.mismatched_points.push_back("schema and binding have no class match");
  }

  report.passed = true;
  for (size_t i = 0; i < report.issues.size(); ++i)
  {
    if (report.issues[i].level == vl_error)
      report.passed = false;
  }

  return true;
}
}
