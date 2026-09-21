

#include "muParser.h"

#include <exception>
#include <iostream>
#include <sstream>
#include <string>

namespace
{
class MiniModule
{
public:
  MiniModule()
    : show_count(0)
  {
  }

  void Show()
  {
    ++show_count;
  }

  int show_count;
};

class MiniParserFacade
{
public:
  MiniParserFacade()
  {
    double *apdouble = 0;
    parser_.DefineOrgClass("double", apdouble);

    MiniModule *pmodule = 0;
    parser_.DefineClass("Module", pmodule);
    parser_.DefineClassFun("Module", pmodule, "Show", &MiniModule::Show);
    parser_.UsingClass(true);
  }

  bool Compile(const std::string &code)
  {
    try
    {
      parser_.SetExpr(code);
      parser_.Eval();
      return true;
    }
    catch (const mu::Parser::exception_type &ex)
    {
      output_ << ex.GetMsg() << "\n";
      return false;
    }
  }

  std::string initialparser()
  {
    const bool ok = Compile("Module amodule;");
    return ok ? std::string("build Module ok\n") : output_.str();
  }

  mu::Parser &GetParser()
  {
    return parser_;
  }

  void clearos()
  {
    output_.str("");
    output_.clear();
  }

private:
  mu::Parser parser_;
  std::ostringstream output_;
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
}

int main()
{
  try
  {
    std::cout << "[CXCORE-MIN] start" << std::endl;

    MiniParserFacade controller;
    const std::string init = controller.initialparser();
    if (!Check(init.find("build Module ok") != std::string::npos, "initialparser did not build module"))
      return 1;

    controller.clearos();
    const bool ok = controller.Compile("if(1){amodule.Show();}amodule.Show();");
    if (!Check(ok, "cxcore-style compile failed"))
      return 1;

    MiniModule *module = static_cast<MiniModule *>(controller.GetParser().GetClassObj("Module", "amodule"));
    if (!Check(module != 0, "cxcore module object was not created"))
      return 1;
    if (!Check(module->show_count == 2, "cxcore module show count mismatch"))
      return 1;

    std::cout << "[CXCORE-MIN] passed" << std::endl;
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
