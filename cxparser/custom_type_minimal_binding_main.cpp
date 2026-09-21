

#include "muParser.h"
#include "custom_type_template_adapter.h"

#include <exception>
#include <iostream>

namespace
{
class CustomValueHost
{
public:
  CustomValueHost()
    : value(0.0)
    , reset_count(0)
    , set_count(0)
    , step_count(0)
  {
  }

  void reset()
  {
    value = 0.0;
    ++reset_count;
  }

  void setvalue(double next_value)
  {
    value = next_value;
    ++set_count;
  }

  void step()
  {
    value += 1.0;
    ++step_count;
  }

  double getvalue()
  {
    return value;
  }

  double value;
  int reset_count;
  int set_count;
  int step_count;
};

const cxparser_template::CustomTypeMethodSpec kCustomValueMethods[] = {
  {"reset", "void reset()"},
  {"setvalue", "void setvalue(double)"},
  {"step", "void step()"},
  {"getvalue", "double getvalue()"}
};

const cxparser_template::CustomTypeSpec kCustomValueSpec = {
  "custom_type_template",
  "CustomValueHost",
  kCustomValueMethods,
  sizeof(kCustomValueMethods) / sizeof(kCustomValueMethods[0])
};

class CustomValueBindingAdapter : public cxparser_template::ICustomTypeBindingAdapter
{
public:
  const cxparser_template::CustomTypeSpec &GetTypeSpec() const override
  {
    return kCustomValueSpec;
  }

  bool Register(mu::Parser &parser) const override
  {
    double *org_double = 0;
    parser.DefineOrgClass("double", org_double);
    parser.UsingClass(true);

    CustomValueHost *host = 0;
    parser.DefineClass(kCustomValueSpec.class_name, host);
    parser.DefineClassFun(kCustomValueSpec.class_name, host, "reset", &CustomValueHost::reset);
    parser.DefineClassFun(kCustomValueSpec.class_name, host, "setvalue", &CustomValueHost::setvalue);
    parser.DefineClassFun(kCustomValueSpec.class_name, host, "step", &CustomValueHost::step);
    parser.DefineClassFun(kCustomValueSpec.class_name, host, "getvalue", &CustomValueHost::getvalue);
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

bool ConfigureCustomTypeParser(mu::Parser &parser)
{
  const CustomValueBindingAdapter adapter;
  if (!Check(adapter.GetTypeSpec().method_count == 4, "custom type spec method count mismatch"))
    return false;
  if (!Check(adapter.Register(parser), "custom type adapter registration failed"))
    return false;
  return true;
}

int RunCustomTypeSmoke()
{
  std::cout << "[CASE] custom type minimal binding" << std::endl;

  mu::Parser parser;
  if (!ConfigureCustomTypeParser(parser))
    return 1;

  double gate = 1.0;
  double total = 0.0;
  parser.DefineVar("gate", &gate);
  parser.DefineVar("total", &total);

  parser.SetExpr(
    "CustomValueHost host;"
    "host.reset();"
    "host.setvalue(3);"
    "if(gate){host.step();}"
    "total=host.getvalue();");
  parser.Eval();

  CustomValueHost *host = static_cast<CustomValueHost *>(parser.GetClassObj("CustomValueHost", "host"));
  if (!Check(host != 0, "custom host object was not created"))
    return 1;
  if (!Check(host->reset_count == 1, "custom host reset count mismatch"))
    return 1;
  if (!Check(host->set_count == 1, "custom host set count mismatch"))
    return 1;
  if (!Check(host->step_count == 1, "custom host step count mismatch"))
    return 1;
  if (!Check(host->value == 4.0, "custom host final value mismatch"))
    return 1;
  if (!Check(total == 4.0, "custom host return assignment mismatch"))
    return 1;

  return 0;
}
}

int main()
{
  try
  {
    std::cout << "[CUSTOM-TYPE-MIN] start" << std::endl;
    if (RunCustomTypeSmoke() != 0)
      return 1;
    std::cout << "[CUSTOM-TYPE-MIN] passed" << std::endl;
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
