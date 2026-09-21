#include "parser_flow_script_catalog.h"

#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

namespace cxparser_ext
{
namespace
{
std::string Trim(const std::string &text)
{
  const std::string whitespace = " \t\r\n";
  const std::size_t first = text.find_first_not_of(whitespace);
  if (first == std::string::npos)
  {
    return std::string();
  }

  const std::size_t last = text.find_last_not_of(whitespace);
  return text.substr(first, last - first + 1);
}

std::string JoinPath(const std::string &lhs, const std::string &rhs)
{
  if (lhs.empty())
  {
    return rhs;
  }

  if (lhs.back() == '/' || lhs.back() == '\\')
  {
    return lhs + rhs;
  }

  return lhs + "/" + rhs;
}

std::string ScriptRootOrDefault(const std::string &root_dir)
{
  if (!root_dir.empty())
  {
    return root_dir;
  }

  return "rag_script_cases/flow_scripts";
}

std::string LoadFileString(const std::string &path)
{
  std::ifstream input(path.c_str(), std::ios::binary);
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

std::string FileStem(const std::string &path)
{
  const std::size_t slash_pos = path.find_last_of("/\\");
  const std::size_t begin = (slash_pos == std::string::npos) ? 0 : slash_pos + 1;
  const std::size_t dot_pos = path.find_last_of('.');
  if (dot_pos == std::string::npos || dot_pos < begin)
  {
    return path.substr(begin);
  }

  return path.substr(begin, dot_pos - begin);
}

std::string ExtractQuotedValue(const std::string &text, const char *token)
{
  const std::size_t token_pos = text.find(token);
  if (token_pos == std::string::npos)
  {
    return std::string();
  }

  const std::size_t first_quote = text.find('"', token_pos);
  if (first_quote == std::string::npos)
  {
    return std::string();
  }

  const std::size_t second_quote = text.find('"', first_quote + 1);
  if (second_quote == std::string::npos)
  {
    return std::string();
  }

  return text.substr(first_quote + 1, second_quote - first_quote - 1);
}

std::vector<std::string> ExtractQuotedValues(const std::string &text)
{
  std::vector<std::string> values;
  std::size_t search_pos = 0;
  while (search_pos < text.size())
  {
    const std::size_t first_quote = text.find('"', search_pos);
    if (first_quote == std::string::npos)
    {
      break;
    }

    const std::size_t second_quote = text.find('"', first_quote + 1);
    if (second_quote == std::string::npos)
    {
      break;
    }

    values.push_back(text.substr(first_quote + 1, second_quote - first_quote - 1));
    search_pos = second_quote + 1;
  }

  return values;
}

FlowScriptSpec BuildSpec(const std::string &layer,
                         const std::string &module,
                         const std::string &path)
{
  FlowScriptSpec spec;
  spec.layer = layer;
  spec.module = module;
  spec.script_path = path;
  spec.script_text = LoadFileString(path);
  spec.flow_id = ExtractQuotedValue(spec.script_text, "FlowCase.Begin(");
  spec.function_name = ExtractQuotedValue(spec.script_text, "FlowCase.Function(");
  if (spec.function_name.empty())
  {
    spec.function_name = FileStem(path);
  }
  return spec;
}

#ifdef _WIN32
bool CollectFilesInDirectory(const std::string &directory,
                             std::vector<std::string> &entries,
                             bool directories_only)
{
  WIN32_FIND_DATAA find_data;
  const std::string pattern = JoinPath(directory, "*");
  HANDLE handle = FindFirstFileA(pattern.c_str(), &find_data);
  if (handle == INVALID_HANDLE_VALUE)
  {
    return false;
  }

  do
  {
    const std::string name = find_data.cFileName;
    if (name == "." || name == "..")
    {
      continue;
    }

    const bool is_directory = (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    if (directories_only == is_directory)
    {
      entries.push_back(name);
    }
  } while (FindNextFileA(handle, &find_data) != 0);

  FindClose(handle);
  return true;
}
#else
bool CollectFilesInDirectory(const std::string &directory,
                             std::vector<std::string> &entries,
                             bool directories_only)
{
  DIR *dir = opendir(directory.c_str());
  if (dir == 0)
  {
    return false;
  }

  for (dirent *entry = readdir(dir); entry != 0; entry = readdir(dir))
  {
    const std::string name = entry->d_name;
    if (name == "." || name == "..")
    {
      continue;
    }

    const bool is_directory = entry->d_type == DT_DIR;
    if (directories_only == is_directory)
    {
      entries.push_back(name);
    }
  }

  closedir(dir);
  return true;
}
#endif

bool EndsWithCxsc(const std::string &name)
{
  return name.size() >= 5 && name.substr(name.size() - 5) == ".cxsc";
}

void ParsePlanLine(const std::string &line, FlowScriptPlan &plan)
{
  const std::string trimmed = Trim(line);
  if (trimmed.empty())
  {
    return;
  }

  const std::vector<std::string> values = ExtractQuotedValues(trimmed);
  if (trimmed.find("Input.Set(") == 0 && values.size() >= 2)
  {
    FlowInputSpec input;
    input.key = values[0];
    input.value = values[1];
    plan.inputs.push_back(input);
    return;
  }

  if (trimmed.find("Step.Call(") == 0 && values.size() >= 3)
  {
    FlowStepSpec step;
    step.result_name = values[0];
    step.entry_name = values[1];
    step.arg_text = values[2];
    plan.steps.push_back(step);
    return;
  }

  if ((trimmed.find("Check.") == 0 || trimmed.find("Expect.Metric(") == 0) && !values.empty())
  {
    FlowCheckSpec check;
    if (trimmed.find("Check.") == 0)
    {
      const std::size_t kind_begin = std::string("Check.").size();
      const std::size_t kind_end = trimmed.find('(', kind_begin);
      check.kind = trimmed.substr(kind_begin, kind_end - kind_begin);
    }
    else
    {
      check.kind = "Metric";
    }

    check.target = values[0];
    if (values.size() >= 2)
    {
      check.expected = values[1];
    }
    plan.checks.push_back(check);
    return;
  }

  if (trimmed.find("Output.Expect(") == 0 && values.size() >= 2)
  {
    FlowOutputSpec output;
    output.key = values[0];
    output.expected = values[1];
    plan.outputs.push_back(output);
  }
}

bool ParsePlan(const FlowScriptSpec &script, FlowScriptPlan &plan)
{
  plan = FlowScriptPlan();
  plan.script = script;

  std::istringstream stream(script.script_text);
  std::string line;
  while (std::getline(stream, line))
  {
    ParsePlanLine(line, plan);
  }

  return !plan.script.flow_id.empty();
}
}

ParserFlowScriptCatalog::ParserFlowScriptCatalog(const std::string &root_dir)
  : root_dir_(ScriptRootOrDefault(root_dir))
{
}

void ParserFlowScriptCatalog::SetRootDir(const std::string &root_dir)
{
  root_dir_ = ScriptRootOrDefault(root_dir);
}

const std::string &ParserFlowScriptCatalog::GetRootDir() const
{
  return root_dir_;
}

bool ParserFlowScriptCatalog::ListScripts(std::vector<FlowScriptSpec> &scripts) const
{
  scripts.clear();

  std::vector<std::string> layer_dirs;
  if (!CollectFilesInDirectory(root_dir_, layer_dirs, true))
  {
    return false;
  }

  for (std::size_t layer_index = 0; layer_index < layer_dirs.size(); ++layer_index)
  {
    const std::string &layer = layer_dirs[layer_index];
    const std::string layer_dir = JoinPath(root_dir_, layer);

    std::vector<std::string> module_dirs;
    if (!CollectFilesInDirectory(layer_dir, module_dirs, true))
    {
      continue;
    }

    for (std::size_t module_index = 0; module_index < module_dirs.size(); ++module_index)
    {
      const std::string &module = module_dirs[module_index];
      const std::string module_dir = JoinPath(layer_dir, module);

      std::vector<std::string> script_files;
      if (!CollectFilesInDirectory(module_dir, script_files, false))
      {
        continue;
      }

      for (std::size_t file_index = 0; file_index < script_files.size(); ++file_index)
      {
        if (!EndsWithCxsc(script_files[file_index]))
        {
          continue;
        }

        scripts.push_back(BuildSpec(layer, module, JoinPath(module_dir, script_files[file_index])));
      }
    }
  }

  return true;
}

bool ParserFlowScriptCatalog::ResolveScript(const std::string &layer,
                                            const std::string &module,
                                            const std::string &function_name,
                                            FlowScriptSpec &script) const
{
  std::vector<FlowScriptSpec> scripts;
  if (!ListScripts(scripts))
  {
    return false;
  }

  for (std::size_t index = 0; index < scripts.size(); ++index)
  {
    if (scripts[index].layer == layer &&
        scripts[index].module == module &&
        scripts[index].function_name == function_name)
    {
      script = scripts[index];
      return true;
    }
  }

  return false;
}

bool ParserFlowScriptCatalog::BuildPlan(const FlowScriptSpec &script, FlowScriptPlan &plan) const
{
  return ParsePlan(script, plan);
}

bool ParserFlowScriptCatalog::ResolvePlan(const std::string &layer,
                                          const std::string &module,
                                          const std::string &function_name,
                                          FlowScriptPlan &plan) const
{
  FlowScriptSpec script;
  if (!ResolveScript(layer, module, function_name, script))
  {
    return false;
  }

  return BuildPlan(script, plan);
}
}
