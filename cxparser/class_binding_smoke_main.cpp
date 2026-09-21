

#include "muParser.h"

#include <exception>
#include <iostream>
#include <string>

namespace
{

class MiniModule
{
public:
  MiniModule()
    : show_count(0)
    , object_show_count(0)
    , flex_count(0)
    , flex_sum(0.0)
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

  void sumall(mu::paramvect &values)
  {
    ++flex_count;
    flex_sum = 0.0;
    for (size_t i = 0; i < values.size(); ++i)
      flex_sum += values[i];
  }

  double sumret(mu::paramvect &values)
  {
    sumall(values);
    return flex_sum;
  }

  int show_count;
  int object_show_count;
  int flex_count;
  double flex_sum;
};

class MiniStringMap
{
public:
  MiniStringMap()
    : add_count(0)
    , set_count(0)
    , batch_count(0)
    , batch_chars(0)
    , mixed_score(0.0)
    , last_value()
  {
  }

  void addstring(const char *value)
  {
    ++add_count;
    last_value = value ? value : "";
  }

  void setstring(const char *value)
  {
    ++set_count;
    last_value = value ? value : "";
  }

  void addmany(mu::charpvect &values)
  {
    ++batch_count;
    batch_chars = 0;
    last_value.clear();
    for (size_t i = 0; i < values.size(); ++i)
    {
      batch_chars += static_cast<int>(values[i].size());
      last_value += values[i];
      if (i + 1 < values.size())
        last_value += "|";
    }
  }

  int countchars(mu::charpvect &values)
  {
    addmany(values);
    return batch_chars;
  }

  void addtag(double score, const char *value)
  {
    mixed_score = score;
    last_value = value ? value : "";
  }

  double tagscore(double score, const char *value)
  {
    addtag(score, value);
    return mixed_score + static_cast<double>(last_value.size());
  }

  int add_count;
  int set_count;
  int batch_count;
  int batch_chars;
  double mixed_score;
  std::string last_value;
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

void ConfigureClassParser(mu::Parser &parser)
{
  double *org_double = 0;
  parser.DefineOrgClass("double", org_double);
  parser.UsingClass(true);

  MiniModule *pmodule = 0;
  parser.DefineClass("module", pmodule);
  parser.DefineClassFun("module", pmodule, "Show", &MiniModule::Show);
  parser.DefineClassFun("module", pmodule, "setobjectshow", &MiniModule::setobjectshow);
  parser.DefineClassFun("module", pmodule, "sumall", &MiniModule::sumall);
  parser.DefineClassFun("module", pmodule, "sumret", &MiniModule::sumret);

  MiniStringMap *pstring = 0;
  parser.DefineClass("stringmap", pstring);
  parser.DefineClassFun("stringmap", pstring, "addstring", &MiniStringMap::addstring);
  parser.DefineClassFun("stringmap", pstring, "setstring", &MiniStringMap::setstring);
  parser.DefineClassFun("stringmap", pstring, "addmany", &MiniStringMap::addmany);
  parser.DefineClassFun("stringmap", pstring, "countchars", &MiniStringMap::countchars);
  parser.DefineClassFun("stringmap", pstring, "addtag", &MiniStringMap::addtag);
  parser.DefineClassFun("stringmap", pstring, "tagscore", &MiniStringMap::tagscore);
}

int RunModuleSmoke()
{
  std::cout << "[CASE] module binding" << std::endl;
  mu::Parser parser;
  ConfigureClassParser(parser);

  parser.SetExpr("module a;a.Show();a.setobjectshow();if(1){a.setobjectshow();}");
  parser.Eval();

  MiniModule *module = static_cast<MiniModule *>(parser.GetClassObj("module", "a"));
  if (!Check(module != 0, "module object was not created"))
    return 1;
  if (!Check(module->show_count == 1, "module.Show() count mismatch"))
    return 1;
  if (!Check(module->object_show_count == 2, "module.setobjectshow() count mismatch"))
    return 1;

  return 0;
}

int RunStringMapSmoke()
{
  std::cout << "[CASE] string map binding" << std::endl;
  mu::Parser parser;
  ConfigureClassParser(parser);

  parser.SetExpr("stringmap s;s.addstring(\"alpha\");if(1){s.setstring(\"beta\");}");
  parser.Eval();

  MiniStringMap *string_map = static_cast<MiniStringMap *>(parser.GetClassObj("stringmap", "s"));
  if (!Check(string_map != 0, "stringmap object was not created"))
    return 1;
  if (!Check(string_map->add_count == 1, "stringmap.addstring() count mismatch"))
    return 1;
  if (!Check(string_map->set_count == 1, "stringmap.setstring() count mismatch"))
    return 1;
  if (!Check(string_map->last_value == "beta", "stringmap last value mismatch"))
    return 1;

  return 0;
}

int RunVariadicStringSmoke()
{
  std::cout << "[CASE] variadic string binding" << std::endl;
  mu::Parser parser;
  ConfigureClassParser(parser);

  double total = 0.0;
  parser.DefineVar("total", &total);
  parser.SetExpr("stringmap s;s.addmany(\"alpha\",\"beta\",\"g\");total=s.countchars(\"one\",\"three\");");
  parser.Eval();

  MiniStringMap *string_map = static_cast<MiniStringMap *>(parser.GetClassObj("stringmap", "s"));
  if (!Check(string_map != 0, "stringmap object was not created for variadic string test"))
    return 1;
  if (!Check(string_map->batch_count == 2, "stringmap variadic string call count mismatch"))
    return 1;
  if (!Check(string_map->batch_chars == 8, "stringmap variadic string char count mismatch"))
    return 1;
  if (!Check(string_map->last_value == "one|three", "stringmap variadic string aggregation mismatch"))
    return 1;
  if (!Check(total == 8.0, "stringmap variadic string return mismatch"))
    return 1;

  return 0;
}

int RunMixedArgSmoke()
{
  std::cout << "[CASE] mixed numeric string binding" << std::endl;
  mu::Parser parser;
  ConfigureClassParser(parser);

  double total = 0.0;
  parser.DefineVar("total", &total);
  parser.SetExpr("stringmap s;s.addtag(7,\"alpha\");total=s.tagscore(3,\"beta\");");
  parser.Eval();

  MiniStringMap *string_map = static_cast<MiniStringMap *>(parser.GetClassObj("stringmap", "s"));
  if (!Check(string_map != 0, "stringmap object was not created for mixed arg test"))
    return 1;
  if (!Check(string_map->mixed_score == 3.0, "stringmap mixed score mismatch"))
    return 1;
  if (!Check(string_map->last_value == "beta", "stringmap mixed last value mismatch"))
    return 1;
  if (!Check(total == 7.0, "stringmap mixed return mismatch"))
    return 1;

  return 0;
}

int RunNumericGateSmoke()
{
  std::cout << "[CASE] typed numeric gate" << std::endl;
  mu::Parser parser;
  ConfigureClassParser(parser);

  parser.SetExpr("double gate=1;module a;if(gate){a.Show();}");
  parser.Eval();

  MiniModule *module = static_cast<MiniModule *>(parser.GetClassObj("module", "a"));
  if (!Check(module != 0, "module object was not created for numeric gate test"))
    return 1;
  if (!Check(module->show_count == 1, "module.Show() under double gate mismatch"))
    return 1;

  return 0;
}

int RunWhileClassSmoke()
{
  std::cout << "[CASE] while class binding" << std::endl;
  mu::Parser parser;
  ConfigureClassParser(parser);

  parser.SetExpr("double gate=3;module a;while(gate){a.setobjectshow();gate=gate-1;}");
  parser.Eval();

  MiniModule *module = static_cast<MiniModule *>(parser.GetClassObj("module", "a"));
  if (!Check(module != 0, "module object was not created for while test"))
    return 1;
  if (!Check(module->object_show_count == 3, "module.setobjectshow() under while mismatch"))
    return 1;

  return 0;
}

int RunMultiObjectSmoke()
{
  std::cout << "[CASE] multi object binding" << std::endl;
  mu::Parser parser;
  ConfigureClassParser(parser);

  parser.SetExpr("module a;module b;a.Show();b.setobjectshow();if(1){a.Show();b.setobjectshow();}");
  parser.Eval();

  MiniModule *module_a = static_cast<MiniModule *>(parser.GetClassObj("module", "a"));
  MiniModule *module_b = static_cast<MiniModule *>(parser.GetClassObj("module", "b"));
  if (!Check(module_a != 0, "first module object was not created"))
    return 1;
  if (!Check(module_b != 0, "second module object was not created"))
    return 1;
  if (!Check(module_a->show_count == 2, "first module Show() count mismatch"))
    return 1;
  if (!Check(module_b->object_show_count == 2, "second module setobjectshow() count mismatch"))
    return 1;

  return 0;
}

int RunVariadicNumericSmoke()
{
  std::cout << "[CASE] variadic numeric binding" << std::endl;
  mu::Parser parser;
  ConfigureClassParser(parser);

  double result = 0.0;
  parser.DefineVar("result", &result);
  parser.SetExpr("module a;a.sumall(1,2,3,4,5);result=a.sumret(10,20,30,40,50,60);");
  parser.Eval();

  MiniModule *module = static_cast<MiniModule *>(parser.GetClassObj("module", "a"));
  if (!Check(module != 0, "module object was not created for variadic test"))
    return 1;
  if (!Check(module->flex_count == 2, "module variadic function call count mismatch"))
    return 1;
  if (!Check(module->flex_sum == 210.0, "module variadic return sum mismatch"))
    return 1;
  if (!Check(result == 210.0, "result assignment from variadic return mismatch"))
    return 1;

  return 0;
}
}

int main()
{
  try
  {
    std::cout << "[CLASS-SMOKE] start" << std::endl;
    int status = 0;
    status += RunModuleSmoke();
    status += RunStringMapSmoke();
    status += RunVariadicStringSmoke();
    status += RunMixedArgSmoke();
    status += RunNumericGateSmoke();
    status += RunWhileClassSmoke();
    status += RunMultiObjectSmoke();
    status += RunVariadicNumericSmoke();

    if (status != 0)
      return 1;

    std::cout << "[CLASS-SMOKE] passed" << std::endl;
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
  catch (...)
  {
    std::cerr << "[EXCEPTION] unknown" << std::endl;
    return 4;
  }
}
