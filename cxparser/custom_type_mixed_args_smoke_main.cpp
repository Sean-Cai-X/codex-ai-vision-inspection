

#include "muParser.h"
#include "custom_type_template_adapter.h"

#include <exception>
#include <iostream>
#include <string>

namespace
{
class CustomLabelHost
{
public:
  CustomLabelHost()
    : weight(0.0)
    , apply_count(0)
    , query_count(0)
    , label()
  {
  }

  void apply(double next_weight, const char *next_label)
  {
    weight = next_weight;
    label = next_label ? next_label : "";
    ++apply_count;
  }

  double score(double base_weight, const char *suffix)
  {
    apply(base_weight, suffix);
    ++query_count;
    return weight + static_cast<double>(label.size());
  }

  double weight;
  int apply_count;
  int query_count;
  std::string label;
};

const cxparser_template::CustomTypeMethodSpec kCustomLabelMethods[] = {
  {"apply", "void apply(double,const char*)"},
  {"score", "double score(double,const char*)"}
};

const cxparser_template::CustomTypeSpec kCustomLabelSpec = {
  "custom_type_template",
  "CustomLabelHost",
  kCustomLabelMethods,
  sizeof(kCustomLabelMethods) / sizeof(kCustomLabelMethods[0])
};

class CustomLabelBindingAdapter : public cxparser_template::ICustomTypeBindingAdapter
{
public:
  const cxparser_template::CustomTypeSpec &GetTypeSpec() const override
  {
    return kCustomLabelSpec;
  }

  bool Register(mu::Parser &parser) const override
  {
    double *org_double = 0;
    parser.DefineOrgClass("double", org_double);
    parser.UsingClass(true);

    CustomLabelHost *host = 0;
    parser.DefineClass(kCustomLabelSpec.class_name, host);
    parser.DefineClassFun(kCustomLabelSpec.class_name, host, "apply", &CustomLabelHost::apply);
    parser.DefineClassFun(kCustomLabelSpec.class_name, host, "score", &CustomLabelHost::score);
    return true;
  }
};

bool Check(bool condition, const char *message)
{
  if (!condition)
  {
    std::cerr << "[FAIL] " << message << std::endl;
    return false;
  }
  return true;
}

bool ConfigureMixedArgParser(mu::Parser &parser)
{
  const CustomLabelBindingAdapter adapter;
  if (!Check(adapter.GetTypeSpec().method_count == 2, "custom label spec method count mismatch"))
    return false;
  if (!Check(adapter.Register(parser), "custom label adapter registration failed"))
    return false;
  return true;
}

int RunMixedArgSmoke()
{
  std::cout << "[CASE] custom type mixed args binding" << std::endl;

  mu::Parser parser;
  if (!ConfigureMixedArgParser(parser))
    return 1;

  double total = 0.0;
  parser.DefineVar("total", &total);
  parser.SetExpr(
    "CustomLabelHost host;"
    "host.apply(7,\"alpha\");"
    "total=host.score(3,\"beta\");");
  parser.Eval();

  CustomLabelHost *host = static_cast<CustomLabelHost *>(parser.GetClassObj("CustomLabelHost", "host"));
  if (!Check(host != 0, "custom label object was not created"))
    return 1;
  if (!Check(host->weight == 3.0, "custom label weight mismatch"))
    return 1;
  if (!Check(host->label == "beta", "custom label value mismatch"))
    return 1;
  if (!Check(host->apply_count == 2, "custom label apply count mismatch"))
    return 1;
  if (!Check(host->query_count == 1, "custom label query count mismatch"))
    return 1;
  if (!Check(total == 7.0, "custom label score mismatch"))
    return 1;

  return 0;
}
}

int main()
{
  try
  {
    std::cout << "[CUSTOM-TYPE-MIXED] start" << std::endl;
    if (RunMixedArgSmoke() != 0)
      return 1;
    std::cout << "[CUSTOM-TYPE-MIXED] passed" << std::endl;
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
