#ifndef CXPARSER_EXT_PARSER_PSEUDOCODE_TYPES_H
#define CXPARSER_EXT_PARSER_PSEUDOCODE_TYPES_H

#include <string>
#include <vector>

namespace cxparser_ext
{
struct PseudoMethodSpec
{
  std::string name;
  std::vector<std::string> param_types;
  std::string return_type;
};

struct PseudoClassSpec
{
  std::string module_name;
  std::string class_name;
  std::string parser_alias;
  std::vector<PseudoMethodSpec> methods;
};

struct PseudoScriptSpec
{
  std::string case_name;
  std::string script_text;
  double expected_scalar = 0.0;
};

struct FlowScriptSpec
{
  std::string flow_id;
  std::string layer;
  std::string module;
  std::string function_name;
  std::string script_path;
  std::string script_text;
};

struct FlowInputSpec
{
  std::string key;
  std::string value;
};

struct FlowStepSpec
{
  std::string result_name;
  std::string entry_name;
  std::string arg_text;
};

struct FlowCheckSpec
{
  std::string kind;
  std::string target;
  std::string expected;
};

struct FlowOutputSpec
{
  std::string key;
  std::string expected;
};

struct FlowScriptPlan
{
  FlowScriptSpec script;
  std::vector<FlowInputSpec> inputs;
  std::vector<FlowStepSpec> steps;
  std::vector<FlowCheckSpec> checks;
  std::vector<FlowOutputSpec> outputs;
};

struct CxscriptFragmentStep
{
  std::string stage_name;
  std::string entry_name;
  std::string purpose;
  std::string output_name;
};

struct CxscriptCapabilityFragment
{
  std::string fragment_id;
  std::string module_name;
  std::string capability_name;
  std::string category;
  std::string summary;
  std::vector<std::string> source_files;
  std::vector<std::string> input_contracts;
  std::vector<CxscriptFragmentStep> steps;
  std::vector<std::string> checkpoints;
  std::vector<std::string> expected_outputs;
  bool reusable_for_cxcore = false;
};

struct CxscriptFlowFragmentBundle
{
  std::string bundle_id;
  std::string bundle_name;
  std::string summary;
  std::vector<std::string> fragment_ids;
  std::vector<std::string> flow_roles;
  std::vector<std::string> reusable_outputs;
  bool reusable_for_cxcore = false;
};
}

#endif
