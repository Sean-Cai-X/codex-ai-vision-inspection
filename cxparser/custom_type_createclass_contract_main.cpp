

#include "muParser.h"

#include <exception>
#include <iostream>
#include <typeinfo>
#include <string>

namespace
{
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
    std::cout << "[CUSTOM-TYPE-CREATECLASS] start" << std::endl;

    using DefineCreateClassSig = void (mu::Parser::*)(const char *, const char *);
    using DefineCreateClassFunSig = void (mu::Parser::*)(const char *, const char *, const char *);

    DefineCreateClassSig create_class_api = &mu::Parser::DefineCreateClass;
    DefineCreateClassFunSig create_class_fun_api = &mu::Parser::DefineCreateClasFun;

    if (!Check(create_class_api != 0, "DefineCreateClass API missing"))
      return 1;
    if (!Check(create_class_fun_api != 0, "DefineCreateClasFun API missing"))
      return 1;

    mu::Parser parser;

    double *org_double = 0;
    std::cout << "[STEP] register org double" << std::endl;
    parser.DefineOrgClass("double", org_double);
    parser.UsingClass(true);

    double result = 0.0;
    std::cout << "[STEP] define result var" << std::endl;
    parser.DefineVar("result", &result);

    std::cout << "[STEP] define create class" << std::endl;
    parser.DefineCreateClass("CustomRecordType", "double value, mirror;");
    std::cout << "[STEP] define scripted members" << std::endl;
    parser.DefineCreateClasFun("CustomRecordType", "create", "mirror=arg;");
    parser.DefineCreateClasFun("CustomRecordType", "bump", "value=value+1;");
    parser.DefineCreateClasFun("CustomRecordType", "publish", "result=value+mirror;");

    const std::string runtime_script =
      "CustomRecordType rec;"
      "rec.create(5);"
      "rec.bump();"
      "rec.bump();"
      "rec.publish();";

    std::cout << "[STEP] eval runtime script" << std::endl;
    parser.SetExpr(runtime_script);
    parser.Eval();

    std::cout << "[STEP] validate instance and result" << std::endl;
    if (!Check(parser.GetClassObj("CustomRecordType", "rec") != 0, "create-class instance was not materialized"))
      return 1;
    if (!Check(result == 7.0, "scripted create-class factory/dispatch result mismatch"))
      return 1;

    std::cout << "[INFO] create-class declaration, parameterized factory, and scripted dispatch bridge are wired" << std::endl;
    std::cout << "[SCRIPT] " << runtime_script << std::endl;
    std::cout << "[CUSTOM-TYPE-CREATECLASS] passed" << std::endl;
    return 0;
  }
  catch (const std::exception &ex)
  {
    std::cerr << "[EXCEPTION] " << ex.what() << std::endl;
    return 2;
  }
  catch (const mu::ParserError &ex)
  {
    std::cerr << "[PARSER-ERROR] " << ex.GetMsg()
              << " token=" << ex.GetToken()
              << " pos=" << ex.GetPos()
              << std::endl;
    return 3;
  }
  catch (...)
  {
    std::cerr << "[EXCEPTION] unknown non-std exception" << std::endl;
    return 4;
  }
}
