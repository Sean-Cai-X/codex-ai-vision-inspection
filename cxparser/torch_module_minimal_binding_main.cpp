

#include "muParser.h"

#include <exception>
#include <iostream>
#include <string>

namespace
{
class TorchTestHost
{
public:
  TorchTestHost()
    : core_runs(0)
    , mobilevit_runs(0)
  {
  }

  int run_core()
  {
    ++core_runs;
    return 0;
  }

  int run_mobilevit()
  {
    ++mobilevit_runs;
    return 0;
  }

  int core_runs;
  int mobilevit_runs;
};

void ConfigureTorchModuleBinding(mu::Parser &parser)
{
  double *org_double = 0;
  parser.DefineOrgClass("double", org_double);

  TorchTestHost *host = 0;
  parser.DefineClass("TorchTestHost", host);
  parser.DefineClassFun("TorchTestHost", host, "run_core", &TorchTestHost::run_core);
  parser.DefineClassFun("TorchTestHost", host, "run_mobilevit", &TorchTestHost::run_mobilevit);
  parser.UsingClass(true);
}

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
    std::cout << "[TORCH-MODULE-MIN] start" << std::endl;

    mu::Parser parser;
    ConfigureTorchModuleBinding(parser);

    double gate = 1.0;
    parser.DefineVar("gate", &gate);
    parser.SetExpr("TorchTestHost t;t.run_core();if(gate){t.run_mobilevit();}");
    parser.Eval();

    TorchTestHost *host = static_cast<TorchTestHost *>(parser.GetClassObj("TorchTestHost", "t"));
    if (!Check(host != 0, "TorchTestHost object was not created"))
      return 1;
    if (!Check(host->core_runs == 1, "run_core dispatch count mismatch"))
      return 1;
    if (!Check(host->mobilevit_runs == 1, "run_mobilevit dispatch count mismatch"))
      return 1;

    std::cout << "[TORCH-MODULE-MIN] passed" << std::endl;
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
