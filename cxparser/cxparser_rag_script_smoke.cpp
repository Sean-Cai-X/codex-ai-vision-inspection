

#include "muParser.h"

#include <cmath>
#include <cerrno>
#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

namespace
{

struct RagScriptCase
{
  const char *name;
  const char *module_name;
  const char *class_name;
  const char *entry_method;
  const char *file_name;
  const char *execution_mode;
  const char *script;
  double expected_result;
};

struct ExternalMethodIndex
{
  const char *method_name;
  const char *signature;
};

struct ExternalClassIndex
{
  const char *source;
  const char *class_name;
  const char *parser_alias;
  const char *status;
  const ExternalMethodIndex *methods;
  size_t method_count;
};

struct ExternalModuleIndex
{
  const char *module_name;
  const char *status;
  const ExternalClassIndex *classes;
  size_t class_count;
};

class MiniModule
{
public:
  MiniModule()
    : show_count(0)
    , object_show_count(0)
  {
  }

  void Show()
  {
    ++show_count;
  }

  void setobjectshow()
  {
    ++object_show_count;
  }

  int show_count;
  int object_show_count;
};

const ExternalMethodIndex kCxCoreModuleMethods[] = {
  {"Show", "void Show()"},
  {"setobjectshow", "void setobjectshow()"}
};

const ExternalMethodIndex kTorchModuleMethods[] = {
  {"run_core", "int run_core()"},
  {"run_mobilevit", "int run_mobilevit()"},
  {"run_preprocess_contract", "int run_preprocess_contract()"},
  {"run_postprocess_contract", "int run_postprocess_contract()"},
  {"run_full_dataset", "int run_full_dataset()"},
  {"run_full_image", "int run_full_image()"},
  {"run_full_train", "int run_full_train()"},
  {"run_current_profile_report", "TorchStageReport run_current_profile_report()"},
  {"task_specs", "std::vector<TorchTaskSpec> task_specs()"},
  {"format_task_spec_line", "std::string format_task_spec_line(const TorchTaskSpec&)"},
  {"format_task_detail_lines", "std::vector<std::string> format_task_detail_lines(const TorchTaskSpec&)"},
  {"format_report_line", "std::string format_report_line(const TorchStageReport&)"},
  {"format_check_lines", "std::vector<std::string> format_check_lines(const TorchStageReport&)"}
};

const ExternalMethodIndex kCustomTypeTemplateMethods[] = {
  {"reset", "void reset()"},
  {"setvalue", "void setvalue(double)"},
  {"step", "void step()"},
  {"getvalue", "double getvalue()"},
  {"createclass.CustomRecordType", "parser_declared create-class entry"},
  {"createclass.bump", "scripted member body placeholder"},
  {"setname", "void setname(const char*)"},
  {"appendtag", "void appendtag(const char*)"},
  {"setscore", "void setscore(double)"},
  {"summaryscore", "double summaryscore()"}
};

const ExternalClassIndex kExternalClassIndex[] = {
  {"cxcore", "Module", "Module", "bound_now", kCxCoreModuleMethods, 2},
  {"torch_module", "TorchTestHost", "TorchTestHost", "parser_host_ready", kTorchModuleMethods, 12},
  {"custom_type_template", "CustomValueHost", "CustomValueHost", "host_template_ready", kCustomTypeTemplateMethods, 10}
};

const ExternalModuleIndex kExternalModules[] = {
  {"cxcore", "active_binding", &kExternalClassIndex[0], 1},
  {"torch_module", "host_binding_planned", &kExternalClassIndex[1], 1},
  {"custom_type_template", "host_template_ready", &kExternalClassIndex[2], 1}
};

bool NearlyEqual(double lhs, double rhs, double eps = 1e-9)
{
  return std::fabs(lhs - rhs) <= eps;
}

void PrintScriptDescriptor(const RagScriptCase &script_case)
{
  std::cout << "  [SCRIPT] module=" << script_case.module_name
            << " class=" << script_case.class_name
            << " entry=" << script_case.entry_method
            << " mode=" << script_case.execution_mode
            << " file=" << script_case.file_name
            << std::endl;
}

void PrintExternalClassIndex()
{
  std::cout << "[GROUP] external_module_index" << std::endl;
  for (size_t module_index = 0; module_index < sizeof(kExternalModules) / sizeof(kExternalModules[0]); ++module_index)
  {
    const ExternalModuleIndex &module = kExternalModules[module_index];
    std::cout << "  [MODULE] name=" << module.module_name
              << " status=" << module.status << std::endl;

    for (size_t class_index = 0; class_index < module.class_count; ++class_index)
    {
      const ExternalClassIndex &entry = module.classes[class_index];
      std::cout << "    [CLASS] source=" << entry.source
                << " class=" << entry.class_name
                << " alias=" << entry.parser_alias
                << " status=" << entry.status << std::endl;
      for (size_t m = 0; m < entry.method_count; ++m)
      {
        std::cout << "      [METHOD] " << entry.methods[m].method_name
                  << " signature=" << entry.methods[m].signature << std::endl;
      }
    }
  }
}

std::string LoadFileString(const std::string &path)
{
  std::ifstream input(path.c_str(), std::ios::binary);
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

bool SaveFileString(const std::string &path, const std::string &content)
{
  std::ofstream output(path.c_str(), std::ios::binary | std::ios::trunc);
  if (!output.is_open())
    return false;
  output << content;
  return output.good();
}

std::string GetRagScriptDir()
{
  const std::string dir = "rag_script_cases";
#ifdef _WIN32
  const int rc = _mkdir(dir.c_str());
#else
  const int rc = mkdir(dir.c_str(), 0755);
#endif
  (void)rc;
  return dir;
}

std::string EnsureDir(const std::string &dir)
{
#ifdef _WIN32
  const int rc = _mkdir(dir.c_str());
#else
  const int rc = mkdir(dir.c_str(), 0755);
#endif
  (void)rc;
  return dir;
}

void EnsureParentDirs(const std::string &path)
{
  size_t cursor = 0;
  while (true)
  {
    cursor = path.find('/', cursor);
    if (cursor == std::string::npos)
      break;

    const std::string prefix = path.substr(0, cursor);
    if (!prefix.empty())
      EnsureDir(prefix);
    ++cursor;
  }
}

std::string JoinPath(const std::string &dir, const std::string &file_name)
{
  const std::string path = dir + "/" + file_name;
  EnsureParentDirs(path);
  return path;
}

void ConfigureClassBindings(mu::Parser &parser)
{
  double *org_double = 0;
  parser.DefineOrgClass("double", org_double);

  MiniModule *pmodule = 0;
  parser.DefineClass("Module", pmodule);
  parser.DefineClassFun("Module", pmodule, "Show", &MiniModule::Show);
  parser.DefineClassFun("Module", pmodule, "setobjectshow", &MiniModule::setobjectshow);
  parser.UsingClass(true);
}

int RunScalarCatalog()
{
  std::cout << "[GROUP] scalar" << std::endl;
  const std::string script_dir = GetRagScriptDir();

  const std::vector<RagScriptCase> cases = {
    {"unary_minus_chain", "cxparser_core", "native_expr", "eval", "unary_minus_chain.cxut", "execute", "a=0;b=2;a=-b+1;a", -1.0},
    {"double_negative", "cxparser_core", "native_expr", "eval", "double_negative.cxut", "execute", "a=0;b=2;a=3--b;a", 5.0},
    {"keyword_boundary", "cxparser_core", "native_expr", "eval", "keyword_boundary.cxut", "execute", "gift=2;gift+1", 3.0},
    {"simple_branch", "cxparser_core", "native_expr", "eval", "simple_branch.cxut", "execute", "a=0;d=0;if(a>0){d=d+1;a=10;}else{a=100;d=10;}d", 10.0},
    {"simple_while", "cxparser_core", "native_expr", "eval", "simple_while.cxut", "execute", "a=3;d=0;while(a>0){d=d+1;a=a-1;}d", 3.0}
  };

  for (size_t i = 0; i < cases.size(); ++i)
  {
    const std::string module_dir = EnsureDir(JoinPath(script_dir, cases[i].module_name));
    const std::string file_path = JoinPath(module_dir, cases[i].file_name);
    if (!SaveFileString(file_path, cases[i].script))
    {
      std::cerr << "[FAIL] unable to save script file: " << file_path << std::endl;
      return 1;
    }

    PrintScriptDescriptor(cases[i]);

    if (std::string(cases[i].execution_mode) == "index_only")
    {
      std::cout << "  [INDEX-ONLY] " << cases[i].name
                << " module=" << cases[i].module_name
                << " file=" << cases[i].file_name << std::endl;
      continue;
    }

    mu::Parser parser;
    double a = 0.0;
    double b = 0.0;
    double d = 0.0;
    double gift = 0.0;
    parser.DefineVar("a", &a);
    parser.DefineVar("b", &b);
    parser.DefineVar("d", &d);
    parser.DefineVar("gift", &gift);
    parser.SetExpr(LoadFileString(file_path));
    const double result = parser.Eval();

    std::cout << "  [CASE] " << cases[i].name
              << " module=" << cases[i].module_name
              << " file=" << cases[i].file_name
              << " result=" << result << std::endl;

    if (!NearlyEqual(result, cases[i].expected_result))
    {
      std::cerr << "[FAIL] " << cases[i].name
                << " expected=" << cases[i].expected_result
                << " actual=" << result << std::endl;
      return 1;
    }
  }

  return 0;
}

int RunClassCatalog()
{
  std::cout << "[GROUP] class_binding" << std::endl;
  const std::string script_dir = GetRagScriptDir();
  const std::string module_dir = EnsureDir(JoinPath(script_dir, "cxcore"));
  const std::string file_path = JoinPath(module_dir, "module_control_flow.cxsc");

  const RagScriptCase script_case = {
    "module_control_flow",
    "cxcore",
    "Module",
    "Show",
    "module_control_flow.cxsc",
    "execute",
    "Module mod;if(gate){mod.Show();}mod.setobjectshow();",
    0.0
  };
  const std::string script = script_case.script;
  if (!SaveFileString(file_path, script))
  {
    std::cerr << "[FAIL] unable to save class binding script file: " << file_path << std::endl;
    return 1;
  }

  PrintScriptDescriptor(script_case);

  mu::Parser parser;
  ConfigureClassBindings(parser);

  double gate = 1.0;
  parser.DefineVar("gate", &gate);
  parser.SetExpr(LoadFileString(file_path));
  const double result = parser.Eval();
  (void)result;

  MiniModule *module = static_cast<MiniModule *>(parser.GetClassObj("Module", "mod"));
  if (module == 0)
  {
    std::cerr << "[FAIL] class catalog did not create Module object" << std::endl;
    return 1;
  }

  std::cout << "  [CASE] module_control_flow module=cxcore file=module_control_flow.cxsc show=" << module->show_count
            << " object_show=" << module->object_show_count << std::endl;

  if (module->show_count != 1 || module->object_show_count != 1)
  {
    std::cerr << "[FAIL] module control flow binding mismatch" << std::endl;
    return 1;
  }

  return 0;
}

int RunTorchModuleCatalog()
{
  std::cout << "[GROUP] torch_module_catalog" << std::endl;
  const std::string script_dir = GetRagScriptDir();
  const std::string module_dir = EnsureDir(JoinPath(script_dir, "torch_module"));

  
  
  const std::vector<RagScriptCase> cases = {
    {"torch_test_run_core", "torch_module", "TorchTestHost", "run_core", "torch_test_run_core.cxsc", "index_only", "TorchTestHost t;t.run_core();", 0.0},
    {"torch_test_run_mobilevit_if", "torch_module", "TorchTestHost", "run_mobilevit", "torch_test_run_mobilevit_if.cxsc", "index_only", "double gate=1;TorchTestHost t;if(gate){t.run_mobilevit();}", 0.0},
    {"torch_preprocess_contract_index", "torch_module", "TorchTestHost", "run_preprocess_contract", "torch_preprocess_contract.cxsc", "index_only", "host=TorchTestHost();host.run_preprocess_contract();", 0.0},
    {"torch_full_dataset_index", "torch_module", "TorchTestHost", "run_full_dataset", "torch_full_dataset.cxsc", "index_only", "host=TorchTestHost();host.run_full_dataset();", 0.0},
    {"torch_stage_report_index", "torch_module", "TorchTestHost", "run_current_profile_report", "torch_stage_report.cxsc", "index_only", "report=TorchTestHost().run_current_profile_report();TorchTestHost::format_report_line(report);", 0.0}
  };

  for (size_t i = 0; i < cases.size(); ++i)
  {
    const std::string file_path = JoinPath(module_dir, cases[i].file_name);
    if (!SaveFileString(file_path, cases[i].script))
    {
      std::cerr << "[FAIL] unable to save torch_module script file: " << file_path << std::endl;
      return 1;
    }

    PrintScriptDescriptor(cases[i]);

    std::cout << "  [INDEX-ONLY] " << cases[i].name
              << " module=" << cases[i].module_name
              << " file=" << cases[i].file_name << std::endl;
  }

  return 0;
}

int RunCustomTypeCatalog()
{
  std::cout << "[GROUP] custom_type_catalog" << std::endl;
  const std::string script_dir = GetRagScriptDir();
  const std::string module_dir = EnsureDir(JoinPath(script_dir, "custom_type_template"));

  const std::vector<RagScriptCase> cases = {
    {"custom_value_minimal", "custom_type_template", "CustomValueHost", "getvalue", "custom_value_minimal.cxsc", "index_only",
     "CustomValueHost host;host.reset();host.setvalue(3);if(gate){host.step();}total=host.getvalue();", 0.0},
    {"custom_record_contract", "custom_type_template", "CustomRecordHost", "summaryscore", "custom_record_contract.cxsc", "index_only",
     "CustomRecordHost rec;rec.setname(\"alpha\");rec.setscore(2);if(gate){rec.appendtag(\"beta\");}rec.appendtag(\"g\");total=rec.summaryscore();", 0.0},
    {"custom_record_createclass", "custom_type_template", "CustomRecordType", "bump", "custom_record_createclass.cxsc", "index_only",
     "CustomRecordType rec;rec.bump();rec.setname(\"alpha\");", 0.0}
  };

  for (size_t i = 0; i < cases.size(); ++i)
  {
    const std::string file_path = JoinPath(module_dir, cases[i].file_name);
    if (!SaveFileString(file_path, cases[i].script))
    {
      std::cerr << "[FAIL] unable to save custom_type script file: " << file_path << std::endl;
      return 1;
    }

    PrintScriptDescriptor(cases[i]);

    std::cout << "  [INDEX-ONLY] " << cases[i].name
              << " module=" << cases[i].module_name
              << " file=" << cases[i].file_name << std::endl;
  }

  return 0;
}
}

int main()
{
  try
  {
    std::cout << "[RAG-SMOKE] start" << std::endl;
    PrintExternalClassIndex();
    int status = 0;
    status += RunScalarCatalog();
    status += RunClassCatalog();
    status += RunTorchModuleCatalog();
    status += RunCustomTypeCatalog();
    if (status != 0)
      return 1;

    std::cout << "[RAG-SMOKE] passed" << std::endl;
    return 0;
  }
  catch (const mu::Parser::exception_type &ex)
  {
    std::cerr << "[PARSER-EXCEPTION] " << ex.GetMsg() << std::endl;
    std::cerr << "[TOKEN] " << ex.GetToken() << std::endl;
    return 2;
  }
  catch (const std::exception &ex)
  {
    std::cerr << "[EXCEPTION] " << ex.what() << std::endl;
    return 3;
  }
}
