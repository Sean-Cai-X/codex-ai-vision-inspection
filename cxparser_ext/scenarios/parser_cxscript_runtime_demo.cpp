#include <iostream>
#include <string>

#include "../pipeline/parser_cxscript_runtime.h"

namespace
{
bool HasDebugData(const cxparser_ext::CxScriptExecutionResult &result)
{
  return !result.debug_view.step_traces.empty() ||
         !result.debug_view.source_map.empty() ||
         !result.debug_view.checkpoints.empty() ||
         !result.debug_view.breakpoints.empty() ||
         !result.debug_view.replay_frames.empty() ||
         !result.debug_view.variables.empty();
}

bool ShouldPrintDebugDetail(const std::string &line)
{
  return line.find("[TEST] ") != 0;
}

int FindPreviousSourceLine(const cxparser_ext::CxScriptExecutionResult &result,
                           const std::string &step_name,
                           int line)
{
  int previous_line = 0;
  for (size_t i = 0; i < result.debug_view.source_map.size(); ++i)
  {
    const cxparser_ext::CxScriptSourceMapEntry &entry = result.debug_view.source_map[i];
    if (entry.step_name != step_name)
      continue;
    if (entry.span.line_begin < line && entry.span.line_begin > previous_line)
      previous_line = entry.span.line_begin;
  }
  return previous_line;
}

void PrintBreakpointWindows(cxparser_ext::ParserCxScriptRuntime &runtime,
                            const cxparser_ext::CxScriptExecutionResult &result)
{
  for (size_t i = 0; i < result.breakpoints.size(); ++i)
  {
    const cxparser_ext::CxScriptBreakpointRecord &breakpoint = result.breakpoints[i];
    const int previous_line = FindPreviousSourceLine(result, breakpoint.step_name, breakpoint.span.line_begin);

    cxparser_ext::CxScriptDebugQueryResult before_query;
    cxparser_ext::CxScriptDebugQueryResult current_query;
    if (previous_line > 0)
      runtime.QueryDebugByLine(result.debug_view, previous_line, before_query);
    runtime.QueryDebugByLine(result.debug_view, breakpoint.span.line_begin, current_query);

    std::cout << "[BREAKWIN] name=" << breakpoint.name
              << " step=" << breakpoint.step_name
              << " before_line=" << previous_line
              << " before_next=" << before_query.next_breakpoint.name
              << " current_prev=" << current_query.previous_breakpoint.name
              << " current_next=" << current_query.next_breakpoint.name
              << "\n";
  }
}

void PrintCheckpointWindows(cxparser_ext::ParserCxScriptRuntime &runtime,
                            const cxparser_ext::CxScriptExecutionResult &result)
{
  for (size_t i = 0; i < result.checkpoints.size(); ++i)
  {
    const cxparser_ext::CxScriptCheckpointRecord &checkpoint = result.checkpoints[i];
    const int previous_line = FindPreviousSourceLine(result, checkpoint.step_name, checkpoint.span.line_begin);

    cxparser_ext::CxScriptDebugQueryResult before_query;
    cxparser_ext::CxScriptDebugQueryResult current_query;
    if (previous_line > 0)
      runtime.QueryDebugByLine(result.debug_view, previous_line, before_query);
    runtime.QueryDebugByLine(result.debug_view, checkpoint.span.line_begin, current_query);

    std::cout << "[CHECKWIN] name=" << checkpoint.name
              << " step=" << checkpoint.step_name
              << " before_line=" << previous_line
              << " before_next=" << before_query.next_checkpoint.name
              << " current_prev=" << current_query.previous_checkpoint.name
              << " current_next=" << current_query.next_checkpoint.name
              << "\n";
  }
}

bool RunScript(cxparser_ext::ParserCxScriptRuntime &runtime,
               const char *name,
               const char *text,
               bool debug_output)
{
  cxparser_ext::CxScriptExecutionResult result;
  if (!runtime.ExecuteScriptText(name, text, result))
  {
    std::cerr << "[FAIL] " << name << " summary=" << result.summary << "\n";
    if (!result.parse_error.message.empty())
      std::cerr << "  token=" << result.parse_error.token
                << " line=" << result.parse_error.line
                << " column=" << result.parse_error.column
                << " block_depth=" << result.parse_error.block_depth
                << " step=" << result.parse_error.step_name << "\n";
    return false;
  }

  std::cout << "[CXSCRIPT] PASS kind=" << result.kind
            << " layer=" << result.layer
            << " module=" << result.module
            << " case=" << result.case_name
            << " task_id=" << result.task_id
            << " summary=" << result.summary
            << "\n";
  if (debug_output)
  {
    for (size_t i = 0; i < result.details.size(); ++i)
    {
      if (!ShouldPrintDebugDetail(result.details[i]))
        continue;
      std::cout << result.details[i] << "\n";
    }
    for (size_t i = 0; i < result.step_traces.size(); ++i)
      std::cout << "[TRACE] step=" << result.step_traces[i].step_name
                << " line=" << result.step_traces[i].span.line_begin
                << " depth=" << result.step_traces[i].block_depth << "\n";
    for (size_t i = 0; i < result.source_map.size(); ++i)
      std::cout << "[SOURCE] kind=" << result.source_map[i].statement_kind
                << " step=" << result.source_map[i].step_name
                << " line=" << result.source_map[i].span.line_begin
                << " depth=" << result.source_map[i].block_depth << "\n";
    for (size_t i = 0; i < result.checkpoints.size(); ++i)
      std::cout << "[CHECK] name=" << result.checkpoints[i].name
                << " step=" << result.checkpoints[i].step_name
                << " line=" << result.checkpoints[i].span.line_begin << "\n";
    for (size_t i = 0; i < result.breakpoints.size(); ++i)
      std::cout << "[BREAK] name=" << result.breakpoints[i].name
                << " step=" << result.breakpoints[i].step_name
                << " line=" << result.breakpoints[i].span.line_begin << "\n";
    for (size_t i = 0; i < result.variables.size(); ++i)
      std::cout << "[VARDECL] type=" << result.variables[i].type_name
                << " name=" << result.variables[i].variable_name
                << " step=" << result.variables[i].step_name
                << " line=" << result.variables[i].span.line_begin
                << " init=" << (result.variables[i].initialized ? "true" : "false")
                << "\n";
    for (size_t i = 0; i < result.replay_frames.size(); ++i)
      std::cout << "[REPLAY] seq=" << result.replay_frames[i].sequence
                << " action=" << result.replay_frames[i].action
                << " step=" << result.replay_frames[i].step_name
                << " status=" << result.replay_frames[i].status
                << " line=" << result.replay_frames[i].span.line_begin << "\n";
    if (HasDebugData(result))
    {
      PrintBreakpointWindows(runtime, result);
      PrintCheckpointWindows(runtime, result);
      std::cout << "[DEBUG] traces=" << result.debug_view.step_traces.size()
                << " source=" << result.debug_view.source_map.size()
                << " checkpoints=" << result.debug_view.checkpoints.size()
                << " breakpoints=" << result.debug_view.breakpoints.size()
                << " replay=" << result.debug_view.replay_frames.size()
                << " vars=" << result.debug_view.variables.size() << "\n";
    }
    for (size_t i = 0; i < result.declared_types.size(); ++i)
      std::cout << "[TYPEDEF] " << result.declared_types[i].name
                << " builtin=" << (result.declared_types[i].builtin ? "true" : "false")
                << " user=" << (result.declared_types[i].user_defined ? "true" : "false") << "\n";
  }
  return true;
}

bool RunScriptFile(cxparser_ext::ParserCxScriptRuntime &runtime,
                   const std::string &script_path,
                   bool debug_output)
{
  cxparser_ext::CxScriptExecutionResult result;
  if (!runtime.ExecuteScriptFile(script_path, result))
  {
    std::cerr << "[FAIL] " << script_path << " summary=" << result.summary << "\n";
    if (!result.parse_error.message.empty())
      std::cerr << "  token=" << result.parse_error.token
                << " line=" << result.parse_error.line
                << " column=" << result.parse_error.column
                << " block_depth=" << result.parse_error.block_depth
                << " step=" << result.parse_error.step_name << "\n";
    return false;
  }

  std::cout << "[CXSCRIPT] PASS file=" << result.script_path
            << " kind=" << result.kind
            << " layer=" << result.layer
            << " module=" << result.module
            << " case=" << result.case_name
            << " task_id=" << result.task_id
            << " summary=" << result.summary
            << "\n";
  if (debug_output)
  {
    for (size_t i = 0; i < result.details.size(); ++i)
    {
      if (!ShouldPrintDebugDetail(result.details[i]))
        continue;
      std::cout << result.details[i] << "\n";
    }
    for (size_t i = 0; i < result.step_traces.size(); ++i)
      std::cout << "[TRACE] step=" << result.step_traces[i].step_name
                << " line=" << result.step_traces[i].span.line_begin
                << " depth=" << result.step_traces[i].block_depth << "\n";
    for (size_t i = 0; i < result.source_map.size(); ++i)
      std::cout << "[SOURCE] kind=" << result.source_map[i].statement_kind
                << " step=" << result.source_map[i].step_name
                << " line=" << result.source_map[i].span.line_begin
                << " depth=" << result.source_map[i].block_depth << "\n";
    for (size_t i = 0; i < result.checkpoints.size(); ++i)
      std::cout << "[CHECK] name=" << result.checkpoints[i].name
                << " step=" << result.checkpoints[i].step_name
                << " line=" << result.checkpoints[i].span.line_begin << "\n";
    for (size_t i = 0; i < result.replay_frames.size(); ++i)
      std::cout << "[REPLAY] seq=" << result.replay_frames[i].sequence
                << " action=" << result.replay_frames[i].action
                << " step=" << result.replay_frames[i].step_name
                << " status=" << result.replay_frames[i].status
                << " line=" << result.replay_frames[i].span.line_begin << "\n";
    if (HasDebugData(result))
    {
      PrintBreakpointWindows(runtime, result);
      PrintCheckpointWindows(runtime, result);
    }
    for (size_t i = 0; i < result.declared_types.size(); ++i)
      std::cout << "[TYPEDEF] " << result.declared_types[i].name
                << " builtin=" << (result.declared_types[i].builtin ? "true" : "false")
                << " user=" << (result.declared_types[i].user_defined ? "true" : "false") << "\n";
  }
  return true;
}
}

int main(int argc, char **argv)
{
  cxparser_ext::ParserCxScriptRuntime runtime;
  bool debug_output = false;

  for (int index = 1; index < argc; ++index)
  {
    const std::string arg = argv[index];
    if (arg == "--debug")
      debug_output = true;
    else if (arg == "--help" || arg == "-h")
    {
      std::cout << "Usage:\n"
                << "  cxparser_ext_cxscript_runtime_demo [--debug]\n";
      return 0;
    }
    else
    {
      std::cerr << "[FAIL] unknown argument: " << arg << "\n";
      std::cout << "Usage:\n"
                << "  cxparser_ext_cxscript_runtime_demo [--debug]\n";
      return 1;
    }
  }

  const char *module_script =
    "# module cxscript\n"
    "kind=module\n"
    "layer=smoke\n"
    "module=cxcore\n"
    "mode=build-run\n"
    "report=on\n"
    "type ImageFrame\n"
    "use ImageFrame\n"
    "step prepare {\n"
    "ImageFrame frame;\n"
    "double threshold = 0.8;\n"
    "breakpoint prepare_bp\n"
    "input image = \"image.png\"\n"
    "call minimal_binding()\n"
    "}\n"
    "step check\n"
    "expect success == true\n"
    "expect degraded == false\n"
    "checkpoint \"after_smoke\"\n"
    "step conclude\n"
    "emit error_message\n"
    "emit summary\n";

  const char *integration_script =
    "# integration cxscript\n"
    "kind=integration\n"
    "layer=scenario\n"
    "module=cxcore\n"
    "mode=build-run\n"
    "route=default\n"
    "report=on\n"
    "type ScenarioFrame\n"
    "use ScenarioFrame\n"
    "step scenario {\n"
    "ScenarioFrame frame;\n"
    "breakpoint scenario_bp\n"
    "call image_analysis(image)\n"
    "}\n"
    "expect success == true\n"
    "expect degraded == false\n"
    "checkpoint integration_ok\n"
    "emit task_id\n";

  if (!RunScript(runtime, "smoke.minimal_binding.cxs", module_script, debug_output))
    return 1;
  if (!RunScript(runtime, "scenario.image_analysis_baseline.cxs", integration_script, debug_output))
    return 1;

  const std::string workspace_root = CXPARSER_WORKSPACE_ROOT;
  if (!RunScriptFile(runtime, workspace_root + "\\cxscript\\module\\cxcore\\smoke.minimal_binding.cxs", debug_output))
    return 1;
  if (!RunScriptFile(runtime, workspace_root + "\\cxscript\\integration\\image\\scenario.image_analysis_baseline.cxs", debug_output))
    return 1;
  if (!RunScriptFile(runtime, workspace_root + "\\cxscript\\integration\\video\\infer.video_frame_chain.cxs", debug_output))
    return 1;

  std::cout << "[PASS] parser_cxscript_runtime_demo completed module/integration dispatch\n";
  return 0;
}
