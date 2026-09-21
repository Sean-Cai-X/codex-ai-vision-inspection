

#include "muParser.h"
#include "custom_type_template_adapter.h"

#include <exception>
#include <iostream>
#include <string>

namespace
{
class CustomRecordHost
{
public:
  CustomRecordHost()
    : score(0.0)
    , set_name_count(0)
    , append_tag_count(0)
    , set_score_count(0)
    , query_count(0)
    , name()
    , tag_log()
  {
  }

  void setname(const char *next_name)
  {
    name = next_name ? next_name : "";
    ++set_name_count;
  }

  void appendtag(const char *tag)
  {
    if (!tag_log.empty())
      tag_log += "|";
    tag_log += (tag ? tag : "");
    ++append_tag_count;
  }

  void setscore(double next_score)
  {
    score = next_score;
    ++set_score_count;
  }

  double summaryscore()
  {
    ++query_count;
    return score + static_cast<double>(name.size()) + static_cast<double>(tag_log.size());
  }

  double score;
  int set_name_count;
  int append_tag_count;
  int set_score_count;
  int query_count;
  std::string name;
  std::string tag_log;
};

const cxparser_template::CustomTypeMethodSpec kCustomRecordMethods[] = {
  {"setname", "void setname(const char*)"},
  {"appendtag", "void appendtag(const char*)"},
  {"setscore", "void setscore(double)"},
  {"summaryscore", "double summaryscore()"}
};

const cxparser_template::CustomTypeSpec kCustomRecordSpec = {
  "custom_type_template",
  "CustomRecordHost",
  kCustomRecordMethods,
  sizeof(kCustomRecordMethods) / sizeof(kCustomRecordMethods[0])
};

class CustomRecordBindingAdapter : public cxparser_template::ICustomTypeBindingAdapter
{
public:
  const cxparser_template::CustomTypeSpec &GetTypeSpec() const override
  {
    return kCustomRecordSpec;
  }

  bool Register(mu::Parser &parser) const override
  {
    double *org_double = 0;
    parser.DefineOrgClass("double", org_double);
    parser.UsingClass(true);

    CustomRecordHost *host = 0;
    parser.DefineClass(kCustomRecordSpec.class_name, host);
    parser.DefineClassFun(kCustomRecordSpec.class_name, host, "setname", &CustomRecordHost::setname);
    parser.DefineClassFun(kCustomRecordSpec.class_name, host, "appendtag", &CustomRecordHost::appendtag);
    parser.DefineClassFun(kCustomRecordSpec.class_name, host, "setscore", &CustomRecordHost::setscore);
    parser.DefineClassFun(kCustomRecordSpec.class_name, host, "summaryscore", &CustomRecordHost::summaryscore);
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

bool ConfigureContractParser(mu::Parser &parser)
{
  const CustomRecordBindingAdapter adapter;
  if (!Check(adapter.GetTypeSpec().method_count == 4, "custom contract spec method count mismatch"))
    return false;
  if (!Check(adapter.Register(parser), "custom contract adapter registration failed"))
    return false;
  return true;
}

int RunContractSmoke()
{
  std::cout << "[CASE] custom type contract binding" << std::endl;

  mu::Parser parser;
  if (!ConfigureContractParser(parser))
    return 1;

  double gate = 1.0;
  double total = 0.0;
  parser.DefineVar("gate", &gate);
  parser.DefineVar("total", &total);

  parser.SetExpr(
    "CustomRecordHost rec;"
    "rec.setname(\"alpha\");"
    "rec.setscore(2);"
    "if(gate){rec.appendtag(\"beta\");}"
    "rec.appendtag(\"g\");"
    "total=rec.summaryscore();");
  parser.Eval();

  CustomRecordHost *record = static_cast<CustomRecordHost *>(parser.GetClassObj("CustomRecordHost", "rec"));
  if (!Check(record != 0, "custom record object was not created"))
    return 1;
  if (!Check(record->set_name_count == 1, "custom record setname count mismatch"))
    return 1;
  if (!Check(record->set_score_count == 1, "custom record setscore count mismatch"))
    return 1;
  if (!Check(record->append_tag_count == 2, "custom record appendtag count mismatch"))
    return 1;
  if (!Check(record->query_count == 1, "custom record summary query count mismatch"))
    return 1;
  if (!Check(record->name == "alpha", "custom record name mismatch"))
    return 1;
  if (!Check(record->tag_log == "beta|g", "custom record tag log mismatch"))
    return 1;
  if (!Check(record->score == 2.0, "custom record score mismatch"))
    return 1;
  if (!Check(total == 13.0, "custom record summary mismatch"))
    return 1;

  return 0;
}
}

int main()
{
  try
  {
    std::cout << "[CUSTOM-TYPE-CONTRACT] start" << std::endl;
    if (RunContractSmoke() != 0)
      return 1;
    std::cout << "[CUSTOM-TYPE-CONTRACT] passed" << std::endl;
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
