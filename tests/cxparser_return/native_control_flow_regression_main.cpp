#include "muParserTest.h"

#include <exception>
#include <iostream>

int main()
{
  try
  {
    mu::Test::ParserTester tester(mu::Test::ParserTester::ControlFlowProfile);
    tester.SetStream(&std::cout);
    tester.Run();
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