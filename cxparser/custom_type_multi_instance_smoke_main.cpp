

#include "muParser.h"
#include "custom_type_template_adapter.h"

#include <exception>
#include <iostream>

namespace
{
class CustomCounterHost
{
public:
  CustomCounterHost()
    : value(0.0)
    , set_count(0)
    , bump_count(0)
  {
  }

  void setvalue(double next_value)
  {
    value = next_value;
    ++set_count;
  }

  void bump()
  {
    value += 1.0;
    ++bump_count;
  }

  double getvalue()
  {
    return value;
  }

  double value;
  int set_count;
  int bump_count;
};

const cxparser_template::CustomTypeMethodSpec kCustomCounterMethods[] = {
  {"setvalue", "void setvalue(double)"},
  {"bump", "void bump()"},
  {"getvalue", "double getvalue()"}
};

const cxparser_template::CustomTypeSpec kCustomCounterSpec = {
  "custom_type_template",
  "CustomCounterHost",
  kCustomCounterMethods,
  sizeof(kCustomCounterMethods) / sizeof(kCustomCounterMethods[0])
};

class CustomCounterBindingAdapter : public cxparser_template::ICustomTypeBindingAdapter
{
public:
  const cxparser_template::CustomTypeSpec &GetTypeSpec() const override
  {
    return kCustomCounterSpec;
  }

  bool Register(mu::Parser &parser) const override
  {
    double *org_double = 0;
    parser.DefineOrgClass("double", org_double);
    parser.UsingClass(true);

    CustomCounterHost *host = 0;
    parser.DefineClass(kCustomCounterSpec.class_name, host);
    parser.DefineClassFun(kCustomCounterSpec.class_name, host, "setvalue", &CustomCounterHost::setvalue);
    parser.DefineClassFun(kCustomCounterSpec.class_name, host, "bump", &CustomCounterHost::bump);
    parser.DefineClassFun(kCustomCounterSpec.class_name, host, "getvalue", &CustomCounterHost::getvalue);
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

bool ConfigureMultiInstanceParser(mu::Parser &parser)
{
  const CustomCounterBindingAdapter adapter;
  if (!Check(adapter.GetTypeSpec().method_count == 3, "custom counter spec method count mismatch"))
    return false;
  if (!Check(adapter.Register(parser), "custom counter adapter registration failed"))
    return false;
  return true;
}

int RunMultiInstanceSmoke()
{
  std::cout << "[CASE] custom type multi instance binding" << std::endl;

  mu::Parser parser;
  if (!ConfigureMultiInstanceParser(parser))
    return 1;

  double total = 0.0;
  parser.DefineVar("total", &total);
  parser.SetExpr(
    "CustomCounterHost left;"
    "CustomCounterHost right;"
    "left.setvalue(2);"
    "right.setvalue(10);"
    "left.bump();"
    "right.bump();"
    "right.bump();"
    "total=left.getvalue()+right.getvalue();");
  parser.Eval();

  CustomCounterHost *left = static_cast<CustomCounterHost *>(parser.GetClassObj("CustomCounterHost", "left"));
  CustomCounterHost *right = static_cast<CustomCounterHost *>(parser.GetClassObj("CustomCounterHost", "right"));
  if (!Check(left != 0, "left custom counter object was not created"))
    return 1;
  if (!Check(right != 0, "right custom counter object was not created"))
    return 1;
  if (!Check(left != right, "custom counter instances are not isolated"))
    return 1;
  if (!Check(left->value == 3.0, "left custom counter final value mismatch"))
    return 1;
  if (!Check(right->value == 12.0, "right custom counter final value mismatch"))
    return 1;
  if (!Check(left->set_count == 1 && right->set_count == 1, "custom counter set count mismatch"))
    return 1;
  if (!Check(left->bump_count == 1 && right->bump_count == 2, "custom counter bump count mismatch"))
    return 1;
  if (!Check(total == 15.0, "custom counter aggregated total mismatch"))
    return 1;

  return 0;
}
}

int main()
{
  try
  {
    std::cout << "[CUSTOM-TYPE-MULTI] start" << std::endl;
    if (RunMultiInstanceSmoke() != 0)
      return 1;
    std::cout << "[CUSTOM-TYPE-MULTI] passed" << std::endl;
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
