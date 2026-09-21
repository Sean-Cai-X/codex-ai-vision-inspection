

#include "muParser.h"

#include <cmath>
#include <exception>
#include <iostream>
#include <string>

namespace
{

bool NearlyEqual(double lhs, double rhs, double eps = 1e-9)
{
  return std::fabs(lhs - rhs) <= eps;
}

int RunUnaryMinusSmoke()
{
  std::cout << "[CASE] unary minus" << std::endl;
  mu::Parser parser;
  double a = 0.0;
  double b = 2.0;
  parser.DefineVar("a", &a);
  parser.DefineVar("b", &b);

  parser.SetExpr("a=-b+1");
  const double result = parser.Eval();

  if (!NearlyEqual(result, -1.0) || !NearlyEqual(a, -1.0))
  {
    std::cerr << "[FAIL] unary minus continuation: result=" << result << ", a=" << a << std::endl;
    return 1;
  }

  parser.SetExpr("a=3--b");
  const double second = parser.Eval();
  if (!NearlyEqual(second, 5.0) || !NearlyEqual(a, 5.0))
  {
    std::cerr << "[FAIL] unary minus after subtraction: result=" << second << ", a=" << a << std::endl;
    return 1;
  }

  return 0;
}

int RunControlFlowSmoke()
{
  std::cout << "[CASE] control flow" << std::endl;
  mu::Parser parser;
  double a = 0.0;
  double d = 0.0;
  parser.DefineVar("a", &a);
  parser.DefineVar("d", &d);

  parser.SetExpr("a=0;d=0;if(a>0){d=d+1;a=10;}else{a=100;d=10;}");
  parser.Eval();
  parser.Eval();

  if (!NearlyEqual(a, 100.0) || !NearlyEqual(d, 10.0))
  {
    std::cerr << "[FAIL] if/else repeated eval: a=" << a << ", d=" << d << std::endl;
    return 1;
  }

  return 0;
}

int RunNestedIfSmoke()
{
  std::cout << "[CASE] nested if" << std::endl;
  mu::Parser parser;
  double a = 0.0;
  double b = 0.0;
  double c = 0.0;
  double d = 0.0;
  parser.DefineVar("a", &a);
  parser.DefineVar("b", &b);
  parser.DefineVar("c", &c);
  parser.DefineVar("d", &d);

  parser.SetExpr(
    "a=0;b=0;c=0;d=0;"
    "if(a>0){"
    "a=100;"
    "if(b>0){b=100;}"
    "a=a/3;"
    "}"
    "if(c<1){"
    "c=100;"
    "if(d<1){d=100;}"
    "c=c/3;"
    "d=(d+100)/3;"
    "}"
  );
  parser.Eval();

  if (!NearlyEqual(a, 0.0) ||
      !NearlyEqual(b, 0.0) ||
      !NearlyEqual(c, 100.0 / 3.0) ||
      !NearlyEqual(d, 200.0 / 3.0))
  {
    std::cerr << "[FAIL] nested if: a=" << a
              << ", b=" << b
              << ", c=" << c
              << ", d=" << d << std::endl;
    return 1;
  }

  return 0;
}

int RunWhileSmoke()
{
  std::cout << "[CASE] while" << std::endl;
  mu::Parser parser;
  double a = 10.0;
  double b = 0.0;
  double c = 0.0;
  double d = 0.0;
  parser.DefineVar("a", &a);
  parser.DefineVar("b", &b);
  parser.DefineVar("c", &c);
  parser.DefineVar("d", &d);

  parser.SetExpr("while(a>0){d=d+1;a=a-1;}while(c<100){c=c+1;b=b+1;}");
  parser.Eval();

  if (!NearlyEqual(a, 0.0) ||
      !NearlyEqual(b, 100.0) ||
      !NearlyEqual(c, 100.0) ||
      !NearlyEqual(d, 10.0))
  {
    std::cerr << "[FAIL] while loop: a=" << a
              << ", b=" << b
              << ", c=" << c
              << ", d=" << d << std::endl;
    return 1;
  }

  return 0;
}

int RunKeywordBoundarySmoke()
{
  std::cout << "[CASE] keyword boundary" << std::endl;
  mu::Parser parser;
  double gift = 2.0;
  parser.DefineVar("gift", &gift);

  parser.SetExpr("gift+1");
  const double result = parser.Eval();

  if (!NearlyEqual(result, 3.0))
  {
    std::cerr << "[FAIL] control-flow keyword boundary: result=" << result << std::endl;
    return 1;
  }

  return 0;
}
}

int main()
{
  try
  {
    std::cout << "[SMOKE] start" << std::endl;
    int status = 0;
    status += RunUnaryMinusSmoke();
    status += RunControlFlowSmoke();
    status += RunNestedIfSmoke();
    status += RunWhileSmoke();
    status += RunKeywordBoundarySmoke();

    if (status != 0)
      return 1;

    std::cout << "[SMOKE] cxparser smoke tests passed" << std::endl;
    return 0;
  }
  catch (const std::exception &ex)
  {
    std::cerr << "[EXCEPTION] " << ex.what() << std::endl;
    return 2;
  }
  catch (...)
  {
    std::cerr << "[EXCEPTION] unknown" << std::endl;
    return 3;
  }
}
